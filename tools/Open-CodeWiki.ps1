param(
    [string]$ProjectRoot = "",
    [int]$Port = 8765,
    [string]$PythonExecutable = "python"
)

$ErrorActionPreference = "Stop"
if ([string]::IsNullOrWhiteSpace($ProjectRoot)) {
    $ProjectRoot = Split-Path -Parent $PSScriptRoot
}
$ProjectRoot = (Resolve-Path -LiteralPath $ProjectRoot).Path
$wikiUrl = "http://127.0.0.1:$Port/docs/wiki-html/"

$serverReady = $false
try {
    $response = Invoke-WebRequest -Uri $wikiUrl -UseBasicParsing -TimeoutSec 1
    $serverReady = $response.StatusCode -eq 200 -and $response.Content -match 'Dom3D Pro Wiki'
} catch {
    $serverReady = $false
}

if (-not $serverReady) {
    Start-Process -FilePath $PythonExecutable `
        -ArgumentList @('-m', 'http.server', $Port, '--bind', '127.0.0.1', '--directory', $ProjectRoot) `
        -WorkingDirectory $ProjectRoot `
        -WindowStyle Hidden

    for ($attempt = 0; $attempt -lt 20; ++$attempt) {
        Start-Sleep -Milliseconds 150
        try {
            $response = Invoke-WebRequest -Uri $wikiUrl -UseBasicParsing -TimeoutSec 1
            if ($response.StatusCode -eq 200 -and $response.Content -match 'Dom3D Pro Wiki') {
                $serverReady = $true
                break
            }
        } catch {
        }
    }
}

if (-not $serverReady) {
    throw "The local Wiki server did not start at $wikiUrl"
}

Start-Process $wikiUrl
Write-Host "Dom3D Pro Wiki: $wikiUrl"

