param(
    [Parameter(Mandatory)]
    [ValidateSet("release", "experimental")]
    [string]$Channel,

    [Parameter(Mandatory)]
    [ValidatePattern("^\d+\.\d+\.\d+$")]
    [string]$Version,

    [Parameter(Mandatory)]
    [ValidatePattern("^[A-Za-z0-9._-]{1,64}$")]
    [string]$BuildId,

    [Parameter(Mandatory)]
    [uint64]$Sequence
)

$ErrorActionPreference = "Stop"
if ($Sequence -eq 0) {
    throw "Package sequence must be greater than zero"
}

$scriptDirectory = Split-Path -Parent $MyInvocation.MyCommand.Path
$repositoryRoot = Split-Path -Parent $scriptDirectory
$outputDirectory = Join-Path $repositoryRoot "release_files"
$bundleDirectory = Join-Path $outputDirectory "bundle"
$manualDirectory = Join-Path $bundleDirectory "Manual Install"
$archivePath = Join-Path $outputDirectory "HSEnhancer.zip"
$launcherSource = if ($Channel -eq "experimental") {
    Join-Path $repositoryRoot "bin/HSEnhancerLauncherExperimental.exe"
} else {
    Join-Path $repositoryRoot "bin/HSEnhancerLauncher.exe"
}

$sourceFiles = @(
    $launcherSource
    (Join-Path $repositoryRoot "bin/HSEnhancer.dll")
    (Join-Path $repositoryRoot "bin/winmm.dll")
    (Join-Path $repositoryRoot "bin/main.dll")
    (Join-Path $repositoryRoot "Manual_Install.txt")
    (Join-Path $repositoryRoot "Linux-Guide.md")
)
foreach ($sourceFile in $sourceFiles) {
    if (-not (Test-Path -LiteralPath $sourceFile -PathType Leaf)) {
        throw "Required package file not found: $sourceFile"
    }
}

if (Test-Path -LiteralPath $bundleDirectory) {
    Remove-Item -LiteralPath $bundleDirectory -Recurse -Force
}
if (Test-Path -LiteralPath $archivePath) {
    Remove-Item -LiteralPath $archivePath -Force
}

New-Item -ItemType Directory -Force -Path $manualDirectory | Out-Null
Copy-Item -LiteralPath $launcherSource -Destination (Join-Path $bundleDirectory "HSEnhancerLauncher.exe")
Copy-Item -LiteralPath (Join-Path $repositoryRoot "bin/HSEnhancer.dll") -Destination $manualDirectory
Copy-Item -LiteralPath (Join-Path $repositoryRoot "bin/winmm.dll") -Destination $manualDirectory
Copy-Item -LiteralPath (Join-Path $repositoryRoot "bin/main.dll") -Destination $manualDirectory
Copy-Item -LiteralPath (Join-Path $repositoryRoot "Manual_Install.txt") -Destination $manualDirectory
Copy-Item -LiteralPath (Join-Path $repositoryRoot "Linux-Guide.md") -Destination $bundleDirectory

$hashes = [ordered]@{
    "HSEnhancerLauncher.exe" = (
        Get-FileHash -Algorithm SHA256 -LiteralPath (Join-Path $bundleDirectory "HSEnhancerLauncher.exe")
    ).Hash.ToLowerInvariant()
    "HSEnhancer.dll" = (
        Get-FileHash -Algorithm SHA256 -LiteralPath (Join-Path $manualDirectory "HSEnhancer.dll")
    ).Hash.ToLowerInvariant()
    "winmm.dll" = (
        Get-FileHash -Algorithm SHA256 -LiteralPath (Join-Path $manualDirectory "winmm.dll")
    ).Hash.ToLowerInvariant()
    "main.dll" = (
        Get-FileHash -Algorithm SHA256 -LiteralPath (Join-Path $manualDirectory "main.dll")
    ).Hash.ToLowerInvariant()
}

$manifest = @(
    "[Package]"
    "format=1"
    "channel=$Channel"
    "version=$Version"
    "build=$BuildId"
    "sequence=$Sequence"
    ""
    "[Files]"
)
foreach ($entry in $hashes.GetEnumerator()) {
    $manifest += "$($entry.Key)=$($entry.Value)"
}
$manifest | Set-Content -LiteralPath (Join-Path $manualDirectory "package.ini") -Encoding utf8NoBOM

Compress-Archive -Path (Join-Path $bundleDirectory "*") -DestinationPath $archivePath -CompressionLevel Optimal
Remove-Item -LiteralPath $bundleDirectory -Recurse -Force

Write-Host "Created $archivePath"
