param(
    [ValidateSet("All", "Launcher", "Mod", "Proxy")]
    [string]$Project = "All",

    [ValidateSet("Release", "Release Experimental", "Debug")]
    [string]$Configuration = "Release",

    [string[]]$Files = @(),

    [ValidateRange(1, 64)]
    [int]$Jobs = [Math]::Min([Environment]::ProcessorCount, 8)
)

$ErrorActionPreference = "Stop"

$scriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$repoRoot = Split-Path -Parent $scriptDir
Set-Location $repoRoot

$ProjectSpecs = @{
    Launcher = @{
        Name = "Launcher"
        IncludeDirs = @("Launcher/include", "ext")
        Defines = @("NOMINMAX", "WIN32_LEAN_AND_MEAN", "_CONSOLE")
    }
    Mod = @{
        Name = "Mod"
        IncludeDirs = @("Mod/include", "Mod/include/imgui", "Mod/ext", "Mod/SDK", "ext")
        Defines = @("NOMINMAX", "WIN32_LEAN_AND_MEAN", "MOD_EXPORTS", "_WINDOWS", "_USRDLL")
    }
    Proxy = @{
        Name = "Proxy"
        IncludeDirs = @("Proxy/bin/intermediate/tidy/generated")
        Defines = @("NOMINMAX", "WIN32_LEAN_AND_MEAN", "_WINDOWS", "_USRDLL")
    }
}

$clangRepoRoot = $repoRoot.Replace("\", "/")
$headerRoot = [regex]::Escape($clangRepoRoot)
$tidyChecks = @(
    "-*",
    "bugprone-*",
    "-bugprone-easily-swappable-parameters",
    "performance-*",
    "modernize-use-override",
    "modernize-use-nullptr",
    "modernize-use-auto",
    "misc-unused-using-decls",
    "readability-identifier-naming"
) -join ","
$ignoredOutputPattern = @(
    '^\d+ warnings?( and \d+ errors?)? generated\.$',
    '^Suppressed \d+ warnings',
    '^Error while trying to load a compilation database:$',
    '^Could not auto-detect compilation database for file ',
    '^No compilation database found in ',
    '^fixed-compilation-database: Error while opening fixed database:',
    '^json-compilation-database: Error while opening JSON database:',
    '^Running without flags\.$'
) -join '|'

function Resolve-Executable {
    param([string]$Name)

    $command = Get-Command $Name -ErrorAction SilentlyContinue
    if ($command) {
        return $command.Source
    }

    $candidates = @(
        "$env:ProgramFiles\LLVM\bin\$Name",
        "$env:ProgramFiles\Microsoft Visual Studio\18\Community\VC\Tools\Llvm\x64\bin\$Name"
    )

    foreach ($candidate in $candidates) {
        if (Test-Path $candidate) {
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

function Get-ProjectForFile {
    param([string]$Path)

    $resolved = Resolve-Path $Path -ErrorAction SilentlyContinue
    if (-not $resolved) {
        throw "File not found: $Path"
    }

    $normalized = $resolved.Path.Replace("\", "/")
    if ($normalized.StartsWith("$clangRepoRoot/")) {
        $normalized = $normalized.Substring($clangRepoRoot.Length + 1)
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

    return Get-ChildItem -Path (Join-Path $Spec.Name "src") -Recurse -File -Filter "*.cpp" |
        ForEach-Object { $_.FullName }
}

function Invoke-TidyForProject {
    param(
        [hashtable]$Spec,
        [string]$ClangTidy
    )

    if ($Spec.Name -eq "Proxy") {
        $generatedDir = Join-Path $repoRoot "Proxy/bin/intermediate/tidy/generated"
        & (Join-Path $repoRoot "Proxy/tools/GenerateWinmmExports.ps1") `
            -DefinitionFile (Join-Path $repoRoot "Proxy/src/winmm.def") `
            -CppOutput (Join-Path $generatedDir "winmm_exports.generated.h") `
            -AsmOutput (Join-Path $generatedDir "winmm_exports.generated.inc") `
            -ModuleDefinitionOutput (Join-Path $generatedDir "winmm.generated.def") | Out-Null
    }

    $sources = @(Get-SourceFiles $Spec)
    if ($sources.Count -eq 0) {
        return 0
    }

    Write-Host "  [$($Spec.Name)] Checking $($sources.Count) source file(s)..."

    $defines = @($Spec.Defines | ForEach-Object { "/D$_" })
    if ($Configuration -eq "Debug") {
        $defines += "/D_DEBUG"
    } else {
        $defines += "/DNDEBUG"
    }
    if ($Configuration -eq "Release Experimental") { $defines += "/DEXPERIMENTAL_VERSION" }

    $includes = @($Spec.IncludeDirs | ForEach-Object { "/I$clangRepoRoot/$_" })

    $compileArgs = @(
        "/nologo",
        "/TP",
        "/std:c++latest",
        "/EHsc",
        "/Zc:__cplusplus",
        "/Zc:preprocessor",
        "-Wno-c++11-narrowing"
    ) + $defines + $includes

    $tidyArgs = @(
        "--quiet",
        "--checks=$tidyChecks",
        "--warnings-as-errors=*",
        "--header-filter=^$headerRoot/$($Spec.Name)/(?!SDK[\\/]|ext[\\/]|bin[\\/]intermediate[\\/])",
        "--system-headers=false",
        "--use-color=false",
        "--extra-arg-before=--driver-mode=cl"
    ) + ($compileArgs | ForEach-Object { "--extra-arg=$_" })

    $indexedSources = for ($index = 0; $index -lt $sources.Count; $index++) {
        [pscustomobject]@{ Index = $index; Path = $sources[$index] }
    }
    $results = @($indexedSources | ForEach-Object -ThrottleLimit $Jobs -Parallel {
        $item = $_
        $clangTidy = $using:ClangTidy
        $arguments = $using:tidyArgs
        $ignorePattern = $using:ignoredOutputPattern
        $output = @(& $clangTidy $item.Path @arguments 2>&1 | ForEach-Object { $_.ToString() })
        $exitCode = $LASTEXITCODE
        $filtered = @($output | Where-Object { $_ -and $_ -notmatch $ignorePattern })
        [pscustomobject]@{
            Index = $item.Index
            Path = $item.Path
            ExitCode = $exitCode
            Output = $filtered
        }
    })

    $issues = 0
    foreach ($result in ($results | Sort-Object Index)) {
        if ($result.Output.Count -gt 0) {
            $issues += $result.Output.Count
            $result.Output | ForEach-Object { Write-Host $_ }
        } elseif ($result.ExitCode -ne 0) {
            $issues += 1
            Write-Host "$($result.Path): clang-tidy exited with code $($result.ExitCode) without diagnostics."
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
