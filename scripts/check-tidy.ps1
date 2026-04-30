param(
    [ValidateSet("All", "Launcher", "Mod", "Proxy")]
    [string]$Project = "All",

    [ValidateSet("Release", "Release Experimental", "Debug")]
    [string]$Configuration = "Release",

    [string[]]$Files = @(),

    [switch]$StrictNaming
)

$ErrorActionPreference = "Stop"

$scriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$repoRoot = Split-Path -Parent $scriptDir
Set-Location $repoRoot

$ProjectSpecs = @{
    Launcher = @{
        Name = "Launcher"
        SourceDir = "Launcher/src"
        IncludeDirs = @("Launcher/include", "ext")
        Defines = @("NOMINMAX", "WIN32_LEAN_AND_MEAN", "NDEBUG", "_CONSOLE")
    }
    Mod = @{
        Name = "Mod"
        SourceDir = "Mod/src"
        IncludeDirs = @("Mod/include", "Mod/include/imgui", "Mod/ext", "Mod/SDK", "ext")
        Defines = @("NOMINMAX", "WIN32_LEAN_AND_MEAN", "NDEBUG", "MOD_EXPORTS", "_WINDOWS", "_USRDLL")
    }
    Proxy = @{
        Name = "Proxy"
        SourceDir = "Proxy/src"
        IncludeDirs = @("Proxy/include", "ext")
        Defines = @("NOMINMAX", "WIN32_LEAN_AND_MEAN", "NDEBUG", "_WINDOWS", "_USRDLL")
    }
}

function Resolve-Executable {
    param([string]$Name)

    $command = Get-Command $Name -ErrorAction SilentlyContinue
    if ($command) {
        return $command.Source
    }

    $candidates = @(
        "$env:ProgramFiles\LLVM\bin\$Name",
        "${env:ProgramFiles(x86)}\LLVM\bin\$Name"
    )

    foreach ($candidate in $candidates) {
        if ($candidate -and (Test-Path $candidate)) {
            return $candidate
        }
    }

    return $null
}

function Import-VisualStudioEnvironment {
    if ($env:INCLUDE -and $env:LIB -and (Get-Command cl.exe -ErrorAction SilentlyContinue)) {
        return
    }

    $vswhere = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"
    if (-not (Test-Path $vswhere)) {
        throw "vswhere.exe not found. Install Visual Studio Build Tools or run this from a Developer PowerShell."
    }

    $vsRoot = & $vswhere -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
    if (-not $vsRoot) {
        throw "Visual Studio C++ tools not found."
    }

    $devCmd = Join-Path $vsRoot "Common7\Tools\VsDevCmd.bat"
    if (-not (Test-Path $devCmd)) {
        throw "VsDevCmd.bat not found at $devCmd."
    }

    $environment = cmd.exe /s /c "`"$devCmd`" -arch=x64 -host_arch=x64 >nul && set"
    foreach ($line in $environment) {
        $separator = $line.IndexOf("=")
        if ($separator -le 0) {
            continue
        }

        $name = $line.Substring(0, $separator)
        $value = $line.Substring($separator + 1)
        Set-Item -Path "Env:$name" -Value $value
    }
}

function Convert-ToClangPath {
    param([string]$Path)

    return $Path.Replace("\", "/")
}

function Get-ProjectForFile {
    param([string]$Path)

    $resolved = Resolve-Path $Path -ErrorAction SilentlyContinue
    if (-not $resolved) {
        throw "File not found: $Path"
    }

    $root = Convert-ToClangPath (Resolve-Path $repoRoot).Path
    $normalized = Convert-ToClangPath $resolved.Path
    if ($normalized.StartsWith("$root/")) {
        $normalized = $normalized.Substring($root.Length + 1)
    }

    if ($normalized -like "Launcher/*") { return "Launcher" }
    if ($normalized -like "Mod/*") { return "Mod" }
    if ($normalized -like "Proxy/*") { return "Proxy" }

    throw "Cannot infer project for file: $Path"
}

function Get-SourceFiles {
    param([hashtable]$Spec)

    if ($Files.Count -gt 0) {
        return $Files |
            Where-Object { (Get-ProjectForFile $_) -eq $Spec.Name } |
            Where-Object {
                if ($_ -notmatch "\.cpp$") {
                    Write-Host "  [$($Spec.Name)] Skipping non-source file: $_"
                    $false
                } else {
                    $true
                }
            }
    }

    return Get-ChildItem -Path $Spec.SourceDir -Recurse -File -Filter "*.cpp" |
        Where-Object {
            $path = Convert-ToClangPath $_.FullName
            $path -notmatch "/ext/" -and $path -notmatch "/SDK/"
        } |
        ForEach-Object { $_.FullName }
}

function Get-LineFilterJson {
    param([hashtable]$Spec)

    $entries = New-Object System.Collections.Generic.List[object]
    $roots = @($Spec.SourceDir)
    $includeRoot = "$($Spec.Name)/include"
    if (Test-Path $includeRoot) {
        $roots += $includeRoot
    }

    foreach ($root in $roots) {
        Get-ChildItem -Path $root -Recurse -File |
            Where-Object {
                $path = Convert-ToClangPath $_.FullName
                ($_.Extension -in @(".cpp", ".h", ".hpp")) -and $path -notmatch "/ext/" -and $path -notmatch "/SDK/"
            } |
            ForEach-Object {
                $absolute = Convert-ToClangPath (Resolve-Path $_.FullName).Path
                $entries.Add(@{ name = $absolute }) | Out-Null

                $relative = Convert-ToClangPath (Resolve-Path -Relative $_.FullName)
                if ($relative.StartsWith("./")) {
                    $relative = $relative.Substring(2)
                }

                if ($relative.StartsWith("$($Spec.Name)/include/")) {
                    $variant = $relative.Replace("$($Spec.Name)/include/", "$($Spec.Name)/src/../include/")
                    $entries.Add(@{ name = Convert-ToClangPath (Join-Path $repoRoot $variant) }) | Out-Null
                }
            }
    }

    return ($entries | ConvertTo-Json -Compress)
}

function Get-HeaderFilter {
    param([hashtable]$Spec)

    $root = [regex]::Escape((Convert-ToClangPath (Resolve-Path $repoRoot).Path))
    return "^$root/$($Spec.Name)/"
}

function Get-Checks {
    $checks = @(
        "-*",
        "bugprone-*",
        "-bugprone-easily-swappable-parameters",
        "performance-*",
        "modernize-use-override",
        "modernize-use-nullptr",
        "modernize-use-auto",
        "misc-unused-using-decls"
    )

    if ($StrictNaming) {
        $checks += "readability-identifier-naming"
    }

    return ($checks -join ",")
}

function Invoke-TidyForProject {
    param(
        [hashtable]$Spec,
        [string]$ClangTidy
    )

    $sources = @(Get-SourceFiles $Spec)
    if ($sources.Count -eq 0) {
        return 0
    }

    Write-Host "  [$($Spec.Name)] Checking $($sources.Count) source file(s)..."

    $defines = New-Object System.Collections.Generic.List[string]
    foreach ($define in $Spec.Defines) {
        $defines.Add("/D$define") | Out-Null
    }
    if ($Configuration -eq "Release Experimental") {
        $defines.Add("/DEXPERIMENTAL_VERSION") | Out-Null
    }
    if ($Configuration -eq "Debug") {
        $defines.Remove("/DNDEBUG") | Out-Null
        $defines.Add("/D_DEBUG") | Out-Null
    }

    $includes = New-Object System.Collections.Generic.List[string]
    foreach ($include in $Spec.IncludeDirs) {
        $resolved = Resolve-Path $include -ErrorAction SilentlyContinue
        if ($resolved) {
            $includes.Add("/I$($resolved.Path)") | Out-Null
        }
    }

    $compileArgs = @(
        "/nologo",
        "/TP",
        "/std:c++latest",
        "/EHsc",
        "/Zc:__cplusplus",
        "/Zc:preprocessor"
    ) + $defines.ToArray() + $includes.ToArray()

    $tidyArgs = @(
        "--quiet",
        "--checks=$(Get-Checks)",
        "--warnings-as-errors=*",
        "--header-filter=$(Get-HeaderFilter $Spec)",
        "--line-filter=$(Get-LineFilterJson $Spec)",
        "--system-headers=false",
        "--use-color=false",
        "--extra-arg-before=--driver-mode=cl"
    )

    $issues = 0
    foreach ($source in $sources) {
        $output = & $ClangTidy $source @tidyArgs "--" @compileArgs 2>&1
        $exitCode = $LASTEXITCODE

        $filteredOutput = @($output | Where-Object {
            $line = $_.ToString()
            $line -and
                $line -notmatch "^\d+ warnings?( and \d+ errors?)? generated\.$" -and
                $line -notmatch "^Suppressed \d+ warnings"
        })

        if ($filteredOutput.Count -gt 0) {
            $issues += $filteredOutput.Count
            $filteredOutput | ForEach-Object { Write-Host $_ }
        } elseif ($exitCode -ne 0) {
            $issues += 1
            Write-Host "$source`: clang-tidy exited with code $exitCode without diagnostics."
        }
    }

    return $issues
}

$clangTidy = Resolve-Executable "clang-tidy.exe"
if (-not $clangTidy) {
    $clangTidy = Resolve-Executable "clang-tidy"
}
if (-not $clangTidy) {
    Write-Host "clang-tidy not found. Install LLVM with winget install LLVM.LLVM or choco install llvm."
    exit 2
}

Import-VisualStudioEnvironment

Write-Host "Running clang-tidy analysis..."
Write-Host ""

$projectNames = if ($Project -eq "All") { @("Launcher", "Mod", "Proxy") } else { @($Project) }
$totalIssues = 0
foreach ($projectName in $projectNames) {
    $spec = $ProjectSpecs[$projectName]
    $totalIssues += Invoke-TidyForProject $spec $clangTidy
}

Write-Host ""
if ($totalIssues -gt 0) {
    Write-Host "clang-tidy found $totalIssues diagnostic line(s)."
    exit 1
}

Write-Host "No clang-tidy issues detected."
exit 0
