param(
    [string]$ProjectRoot = ""
)

$ErrorActionPreference = "Stop"

if ([string]::IsNullOrWhiteSpace($ProjectRoot)) {
    $ProjectRoot = Split-Path -Parent $PSScriptRoot
}
$ProjectRoot = (Resolve-Path -LiteralPath $ProjectRoot).Path
$sourceRoot = Join-Path $ProjectRoot "src"
$testsRoot = Join-Path $ProjectRoot "tests"
$cmakePath = Join-Path $ProjectRoot "CMakeLists.txt"
$outputRoot = Join-Path $ProjectRoot "docs\knowledge\generated"

if (-not (Test-Path -LiteralPath $sourceRoot) -or
    -not (Test-Path -LiteralPath $cmakePath)) {
    throw "Dom3D Pro source tree was not found under '$ProjectRoot'."
}

New-Item -ItemType Directory -Force -Path $outputRoot | Out-Null
$utf8NoBom = New-Object System.Text.UTF8Encoding($false)

function Get-ProjectRelativePath([string]$Path) {
    return $Path.Substring($ProjectRoot.Length + 1).Replace('\', '/')
}

function Add-MarkdownLine(
    [System.Text.StringBuilder]$Builder,
    [string]$Line = "") {
    [void]$Builder.AppendLine($Line)
}

$sourceFiles = @(Get-ChildItem -LiteralPath $sourceRoot -Recurse -File |
    Where-Object { $_.Extension -in @('.h', '.hpp', '.cpp', '.c') } |
    Sort-Object FullName)
$testFiles = @()
if (Test-Path -LiteralPath $testsRoot) {
    $testFiles = @(Get-ChildItem -LiteralPath $testsRoot -File -Filter '*Tests.cpp' |
        Sort-Object Name)
}

$moduleDescriptions = @{
    'core' = 'Document, scene objects, curves, meshes, materials, and common geometry.'
    '3DCoat' = 'Integrated 3DCoat contour-filling algorithms.'
    'comms' = 'Low-level 3DCoat graphics and utility components.'
    'excomms' = 'Experimental mesh containers, codecs, and mesh operations.'
    'iges' = 'IGES geometry and spline support.'
    'render' = 'Scene preparation and external/native renderers.'
    'resources' = 'Embedded application resources.'
    'shell' = 'Windows shell integration.'
    'solid' = 'OpenCascade solids, surfaces, tools, and shape builders.'
    'ui' = 'Qt UI, viewport, property panels, and the tool registry.'
}

$moduleRows = @()
foreach ($group in ($sourceFiles | Group-Object {
        $relative = Get-ProjectRelativePath $_.FullName
        $parts = $relative.Split('/')
        if ($parts.Count -gt 2) { $parts[1] } else { 'core' }
    } | Sort-Object Name)) {
    $headers = @($group.Group | Where-Object { $_.Extension -in @('.h', '.hpp') }).Count
    $implementations = @($group.Group | Where-Object { $_.Extension -in @('.cpp', '.c') }).Count
    $description = $moduleDescriptions[$group.Name]
    if ([string]::IsNullOrWhiteSpace($description)) {
        $description = 'Project subsystem; inspect its source files for details.'
    }
    $moduleRows += [pscustomobject]@{
        Name = $group.Name
        Headers = $headers
        Implementations = $implementations
        Total = $group.Count
        Description = $description
    }
}

$declarations = @()
foreach ($file in $sourceFiles | Where-Object { $_.Extension -in @('.h', '.hpp') }) {
    $lines = @(Get-Content -LiteralPath $file.FullName)
    for ($lineIndex = 0; $lineIndex -lt $lines.Count; ++$lineIndex) {
        $line = $lines[$lineIndex]
        if ($line -notmatch '^\s*(class|struct)\s+(?:(?:[A-Za-z_][A-Za-z0-9_]*_API|Q_DECL_EXPORT|APICALL)\s+)?(?<name>[A-Za-z_][A-Za-z0-9_]*)(?<tail>.*)$') {
            continue
        }
        $kind = $Matches[1]
        $name = $Matches['name']
        $tail = $Matches['tail'].Trim()
        if ($tail -match '^\s*;' -or ($tail -match ';\s*(?://.*)?$' -and $tail -notmatch '\{')) {
            continue
        }
        $base = ''
        if ($tail -match ':\s*(?:(?:public|protected|private)\s+)?(?<base>[A-Za-z_][A-Za-z0-9_:]*)') {
            $base = $Matches['base']
        }
        $relative = Get-ProjectRelativePath $file.FullName
        $parts = $relative.Split('/')
        $module = if ($parts.Count -gt 2) { $parts[1] } else { 'core' }
        $declarations += [pscustomobject]@{
            Name = $name
            Kind = $kind
            Base = $base
            Module = $module
            Path = $relative
            Line = $lineIndex + 1
        }
    }
}
$declarations = @($declarations | Sort-Object Name, Path, Line -Unique)

$cmakeText = Get-Content -LiteralPath $cmakePath -Raw
$targets = @()
$targetPattern = '(?ms)^\s*add_(?<kind>executable|library)\s*\(\s*(?<name>[^\s\)]+)'
foreach ($match in [regex]::Matches($cmakeText, $targetPattern)) {
    $targets += [pscustomobject]@{
        Name = $match.Groups['name'].Value
        Kind = $match.Groups['kind'].Value
    }
}
$targets = @($targets | Sort-Object Name -Unique)

$date = Get-Date -Format 'yyyy-MM-dd'
$inventory = New-Object System.Text.StringBuilder
Add-MarkdownLine $inventory '# Generated project map'
Add-MarkdownLine $inventory
Add-MarkdownLine $inventory "Generated from project files: $date.  "
Add-MarkdownLine $inventory 'Generator: [`tools/Generate-CodeWiki.ps1`](../../../tools/Generate-CodeWiki.ps1)'
Add-MarkdownLine $inventory
Add-MarkdownLine $inventory '> This page is generated. Manual edits are replaced by the next `CodeWiki` run.'
Add-MarkdownLine $inventory
Add-MarkdownLine $inventory '## Summary'
Add-MarkdownLine $inventory
Add-MarkdownLine $inventory "- C/C++ source files: **$($sourceFiles.Count)**"
Add-MarkdownLine $inventory "- Class and struct definitions found: **$($declarations.Count)**"
Add-MarkdownLine $inventory "- CMake targets: **$($targets.Count)**"
Add-MarkdownLine $inventory "- ``*Tests.cpp`` files: **$($testFiles.Count)**"
Add-MarkdownLine $inventory
Add-MarkdownLine $inventory '## Subsystems'
Add-MarkdownLine $inventory
Add-MarkdownLine $inventory '| Subsystem | Headers | Implementations | Total | Purpose |'
Add-MarkdownLine $inventory '|---|---:|---:|---:|---|'
foreach ($module in $moduleRows) {
    Add-MarkdownLine $inventory "| ``$($module.Name)`` | $($module.Headers) | $($module.Implementations) | $($module.Total) | $($module.Description) |"
}
Add-MarkdownLine $inventory
Add-MarkdownLine $inventory '## CMake targets'
Add-MarkdownLine $inventory
Add-MarkdownLine $inventory '| Target | Kind |'
Add-MarkdownLine $inventory '|---|---|'
foreach ($target in $targets) {
    Add-MarkdownLine $inventory "| ``$($target.Name)`` | ``$($target.Kind)`` |"
}
Add-MarkdownLine $inventory
Add-MarkdownLine $inventory '## Automated tests'
Add-MarkdownLine $inventory
foreach ($test in $testFiles) {
    $relative = Get-ProjectRelativePath $test.FullName
    Add-MarkdownLine $inventory "- [$($test.BaseName)](../../../$relative)"
}
Add-MarkdownLine $inventory
Add-MarkdownLine $inventory '## Refresh'
Add-MarkdownLine $inventory
Add-MarkdownLine $inventory '```powershell'
Add-MarkdownLine $inventory 'cmake --build build --target CodeWiki'
Add-MarkdownLine $inventory '```'

$classIndex = New-Object System.Text.StringBuilder
Add-MarkdownLine $classIndex '# Class and struct index'
Add-MarkdownLine $classIndex
Add-MarkdownLine $classIndex "Generated from project headers: $date."
Add-MarkdownLine $classIndex
Add-MarkdownLine $classIndex '> This is a syntax-oriented index, not a complete C++ parser.'
Add-MarkdownLine $classIndex '> Nested or conditionally compiled types can require manual review.'
foreach ($moduleGroup in $declarations | Group-Object Module | Sort-Object Name) {
    Add-MarkdownLine $classIndex
    Add-MarkdownLine $classIndex "## $($moduleGroup.Name)"
    Add-MarkdownLine $classIndex
    Add-MarkdownLine $classIndex '| Type | Kind | Base | Source |'
    Add-MarkdownLine $classIndex '|---|---|---|---|'
    foreach ($declaration in $moduleGroup.Group) {
        $base = if ([string]::IsNullOrWhiteSpace($declaration.Base)) { '-' } else { "``$($declaration.Base)``" }
        $link = "../../../$($declaration.Path)#$($declaration.Line)"
        Add-MarkdownLine $classIndex "| ``$($declaration.Name)`` | ``$($declaration.Kind)`` | $base | [$($declaration.Path):$($declaration.Line)]($link) |"
    }
}

[System.IO.File]::WriteAllText(
    (Join-Path $outputRoot 'project-map.md'), $inventory.ToString(), $utf8NoBom)
[System.IO.File]::WriteAllText(
    (Join-Path $outputRoot 'class-index.md'), $classIndex.ToString(), $utf8NoBom)

Write-Host "Code Wiki generated:"
Write-Host "  docs/knowledge/generated/project-map.md"
Write-Host "  docs/knowledge/generated/class-index.md"
