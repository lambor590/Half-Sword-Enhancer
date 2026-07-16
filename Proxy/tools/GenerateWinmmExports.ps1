[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)]
    [string] $DefinitionFile,

    [Parameter(Mandatory = $true)]
    [string] $CppOutput,

    [Parameter(Mandatory = $true)]
    [string] $AsmOutput,

    [Parameter(Mandatory = $true)]
    [string] $ModuleDefinitionOutput,

    [ValidateRange(1, 65535)]
    [int] $ExpectedCount = 180
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

function Write-DeterministicFile {
    param(
        [Parameter(Mandatory = $true)]
        [string] $Path,

        [Parameter(Mandatory = $true)]
        [string] $Content
    )

    $fullPath = [System.IO.Path]::GetFullPath($Path)
    $directory = [System.IO.Path]::GetDirectoryName($fullPath)
    [System.IO.Directory]::CreateDirectory($directory) | Out-Null

    $encoding = [System.Text.UTF8Encoding]::new($false)
    $expectedBytes = $encoding.GetBytes($Content)
    $matchesExpectedBytes = $false
    if ([System.IO.File]::Exists($fullPath)) {
        $currentBytes = [System.IO.File]::ReadAllBytes($fullPath)
        $matchesExpectedBytes = [Convert]::ToBase64String($currentBytes) -ceq [Convert]::ToBase64String($expectedBytes)
    }

    if (-not $matchesExpectedBytes) {
        [System.IO.File]::WriteAllBytes($fullPath, $expectedBytes)
    }

    $writtenBytes = [System.IO.File]::ReadAllBytes($fullPath)
    if ([Convert]::ToBase64String($writtenBytes) -cne [Convert]::ToBase64String($expectedBytes)) {
        throw "Generated file verification failed: $fullPath"
    }
}

function Get-NotReadyResult {
    param(
        [Parameter(Mandatory = $true)]
        [string] $ExportName
    )

    # Most pointer, BOOL and device-count APIs use zero as their natural failure
    # result. Status-code APIs are the exception: zero means success for them.
    if ($ExportName -match '^time(?:BeginPeriod|EndPeriod|GetDevCaps|GetSystemTime|KillEvent)$') {
        return 97 # TIMERR_NOCANDO
    }
    if ($ExportName -match '^mciSend(?:Command|String)[AW]$') {
        return 277 # MCIERR_INTERNAL
    }
    if ($ExportName -match '^mmio(?:Read|Seek|Write)$') {
        return -1
    }
    if ($ExportName -match '^mmio(?:Advance|Ascend|Close|CreateChunk|Descend|Flush|GetInfo|Rename[AW]|SetBuffer|SetInfo)$' -or
        $ExportName -eq 'mmTaskCreate') {
        return 1 # MMSYSERR_ERROR / TASKERR_NOTASKSUPPORT
    }
    if ($ExportName -match '^(?:aux|joy|midi|mixer|wave)' -and $ExportName -notmatch 'GetNumDevs$') {
        return 1 # MMSYSERR_ERROR
    }
    return 0
}

$definitionPath = (Resolve-Path -LiteralPath $DefinitionFile).Path
$meaningfulLines = @(
    [System.IO.File]::ReadAllLines($definitionPath) |
        ForEach-Object { $_.Trim() } |
        Where-Object { $_ -and -not $_.StartsWith(';') }
)

if ($meaningfulLines.Count -lt 3 -or $meaningfulLines[0] -notmatch '^LIBRARY\s+winmm$' -or
    $meaningfulLines[1] -notmatch '^EXPORTS$') {
    throw "Expected '$definitionPath' to start with 'LIBRARY winmm' followed by 'EXPORTS'."
}

$exports = @($meaningfulLines | Select-Object -Skip 2)
if ($exports.Count -ne $ExpectedCount) {
    throw "Expected $ExpectedCount winmm exports in '$definitionPath', found $($exports.Count)."
}

$seen = [System.Collections.Generic.HashSet[string]]::new([System.StringComparer]::Ordinal)
foreach ($exportName in $exports) {
    if ($exportName -notmatch '^[A-Za-z_][A-Za-z0-9_]*$') {
        throw "Unsupported winmm export declaration '$exportName'. Use one plain symbol per line."
    }
    if (-not $seen.Add($exportName)) {
        throw "Duplicate winmm export '$exportName'."
    }
}

if (-not $seen.Contains('PlaySound') -or -not $seen.Contains('mciExecute')) {
    throw "The canonical winmm export list must contain PlaySound and mciExecute."
}

$cppLines = [System.Collections.Generic.List[string]]::new()
$cppLines.Add('// Generated from Proxy/src/winmm.def. Do not edit.')
$cppLines.Add('#pragma once')
$cppLines.Add('')
$cppLines.Add('#include <array>')
$cppLines.Add('#include <cstddef>')
$cppLines.Add('')
$cppLines.Add('namespace winmm_exports {')
$cppLines.Add("    inline constexpr std::array<const char*, $($exports.Count)> kNames{")
foreach ($exportName in $exports) {
    $cppLines.Add("        `"$exportName`",")
}
$cppLines.Add('    };')
$cppLines.Add('    inline constexpr std::size_t kCount = kNames.size();')
$cppLines.Add('}')
$cppLines.Add('')

$asmLines = [System.Collections.Generic.List[string]]::new()
$asmLines.Add('; Generated from Proxy/src/winmm.def. Do not edit.')
$asmLines.Add("WINMM_EXPORT_COUNT = $($exports.Count)")
$asmLines.Add('')

$moduleDefinitionLines = [System.Collections.Generic.List[string]]::new()
$moduleDefinitionLines.Add('; Generated from Proxy/src/winmm.def. Do not edit.')
$moduleDefinitionLines.Add('LIBRARY winmm')
$moduleDefinitionLines.Add('EXPORTS')
# Preserve the legacy winmm ABI: ordinal 2 is an unnamed PlaySound alias,
# mciExecute is ordinal 3, and the remaining canonical names occupy 4..182.
$moduleDefinitionLines.Add('    __winmm_ordinal_2=PlaySound @2 NONAME')
$nextOrdinal = 4
foreach ($exportName in $exports) {
    $notReadyResult = Get-NotReadyResult -ExportName $exportName
    $asmLines.Add("PROXY $exportName, $($exportName)_wait, $notReadyResult")
    if ($exportName -ceq 'mciExecute') {
        $ordinal = 3
    } else {
        $ordinal = $nextOrdinal
        $nextOrdinal += 1
    }
    $moduleDefinitionLines.Add("    $exportName @$ordinal")
}
$asmLines.Add('')
$moduleDefinitionLines.Add('')

$cppContent = [string]::Join("`r`n", $cppLines)
$asmContent = [string]::Join("`r`n", $asmLines)
$moduleDefinitionContent = [string]::Join("`r`n", $moduleDefinitionLines)

Write-DeterministicFile -Path $CppOutput -Content $cppContent
Write-DeterministicFile -Path $AsmOutput -Content $asmContent
Write-DeterministicFile -Path $ModuleDefinitionOutput -Content $moduleDefinitionContent

Write-Host "Generated and verified $($exports.Count) named winmm exports and the ordinal-only alias from '$definitionPath'."
