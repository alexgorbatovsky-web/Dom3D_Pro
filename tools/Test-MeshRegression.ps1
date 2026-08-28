param(
    [string]$BuildDirectory = (Join-Path $PSScriptRoot '..\build'),
    [string]$Configuration = 'Release',
    [string]$Manifest = (Join-Path $PSScriptRoot '..\tests\data\mesh-regression\cases.json')
)

$ErrorActionPreference = 'Stop'
$manifestPath = (Resolve-Path -LiteralPath $Manifest).Path
$corpusDirectory = Split-Path -Parent $manifestPath
$testExecutable = Join-Path $BuildDirectory "$Configuration\CatalogImportTests.exe"
if (-not (Test-Path -LiteralPath $testExecutable)) {
    throw "CatalogImportTests was not found: $testExecutable"
}

$definition = Get-Content -LiteralPath $manifestPath -Raw -Encoding UTF8 |
    ConvertFrom-Json
$failures = [System.Collections.Generic.List[string]]::new()
$runs = 0

function Get-Sha256([string]$Path) {
    $stream = [IO.File]::OpenRead($Path)
    try {
        $sha = [Security.Cryptography.SHA256]::Create()
        try {
            return ([BitConverter]::ToString($sha.ComputeHash($stream))).Replace('-', '')
        }
        finally {
            $sha.Dispose()
        }
    }
    finally {
        $stream.Dispose()
    }
}

foreach ($case in $definition.cases) {
    $project = Join-Path $corpusDirectory $case.file
    if (-not (Test-Path -LiteralPath $project)) {
        $failures.Add("$($case.id): project is missing: $project")
        continue
    }
    $actualHash = Get-Sha256 $project
    if ($actualHash -ne $case.sha256) {
        $failures.Add("$($case.id): SHA256 differs; expected $($case.sha256), got $actualHash")
        continue
    }
    if ($case.referenceStep) {
        $referenceStep = Join-Path $corpusDirectory $case.referenceStep.file
        if (-not (Test-Path -LiteralPath $referenceStep)) {
            $failures.Add("$($case.id): reference STEP is missing: $referenceStep")
            continue
        }
        $referenceHash = Get-Sha256 $referenceStep
        if ($referenceHash -ne $case.referenceStep.sha256) {
            $failures.Add(
                "$($case.id): reference STEP SHA256 differs; expected " +
                "$($case.referenceStep.sha256), got $referenceHash")
            continue
        }
    }

    foreach ($density in $case.densities) {
        ++$runs
        $densityText = [Convert]::ToString(
            [double]$density, [Globalization.CultureInfo]::InvariantCulture)
        $savedErrorPreference = $ErrorActionPreference
        $ErrorActionPreference = 'Continue'
        $output = & $testExecutable --diagnose-project-quadro $project $densityText 2>&1 |
            Out-String
        $diagnosticExitCode = $LASTEXITCODE
        $ErrorActionPreference = $savedErrorPreference
        if ($diagnosticExitCode -ne 0) {
            $failures.Add("$($case.id) density=${densityText}: diagnostic process failed")
            continue
        }

        $solidMatch = [regex]::Match(
            $output, 'solid="(?<name>[^"]+)" rebuilt=(?<rebuilt>[01]) surfaces=(?<count>\d+)')
        if (-not $solidMatch.Success) {
            $failures.Add("$($case.id) density=${densityText}: solid summary is missing")
            continue
        }
        $actualEmpty = @(
            [regex]::Matches(
                $output, 'surface=(?<index>\d+)[^\r\n]*vertices=0(?:\s|$)') |
                ForEach-Object { [int]$_.Groups['index'].Value } |
                Sort-Object -Unique
        )
        $expectedEmpty = @($case.expected.emptySurfaces | ForEach-Object { [int]$_ } |
            Sort-Object -Unique)
        $rebuilt = $solidMatch.Groups['rebuilt'].Value -eq '1'
        $expectedRebuilt = [bool]$case.expected.rebuild
        $surfaceCount = [int]$solidMatch.Groups['count'].Value
        $expectedSurfaceCount = [int]$case.expected.surfaceCount

        $emptyEqual = ($actualEmpty.Count -eq $expectedEmpty.Count) -and
            (-not (Compare-Object $actualEmpty $expectedEmpty))
        if ($solidMatch.Groups['name'].Value -ne $case.expected.solidName -or
            $surfaceCount -ne $expectedSurfaceCount -or
            $rebuilt -ne $expectedRebuilt -or
            -not $emptyEqual) {
            $failures.Add(
                "$($case.id) density=${densityText}: expected rebuilt=$expectedRebuilt, " +
                "surfaces=$expectedSurfaceCount, empty=[$($expectedEmpty -join ',')]; " +
                "got rebuilt=$rebuilt, surfaces=$surfaceCount, empty=[$($actualEmpty -join ',')]")
            continue
        }

        Write-Host (
            "PASS {0} density={1} status={2} rebuilt={3} empty=[{4}]" -f
            $case.id, $densityText, $case.status, $rebuilt,
            ($actualEmpty -join ','))
    }
}

if ($failures.Count -gt 0) {
    Write-Error ("Mesh regression failures:`n - " + ($failures -join "`n - "))
    exit 1
}

Write-Host "Mesh regression corpus passed: $runs run(s)."
