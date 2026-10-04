param([string]$DeviceSerial = 'emulator-5560', [switch]$Production)
$ErrorActionPreference = 'Stop'
if ($DeviceSerial -notmatch '^emulator-\d+$') { throw 'Use an emulator for the isolated touch probe.' }
$sdkRoot = $env:ANDROID_HOME
if (-not $sdkRoot) { $sdkRoot = Join-Path $env:LOCALAPPDATA 'Android\Sdk' }
$adbPath = Join-Path $sdkRoot 'platform-tools\adb.exe'
$probeRoot = Join-Path $PSScriptRoot 'probes\remote-touch'
$reportRoot = Join-Path $probeRoot 'build\reports\touch-probe'
New-Item -ItemType Directory -Path $reportRoot -Force | Out-Null
function Invoke-ProbeAdb {
    param([string[]]$Arguments)
    $output = & $adbPath -s $DeviceSerial @Arguments
    if ($LASTEXITCODE -ne 0) { throw "ADB failed: $Arguments" }
    return $output
}
Invoke-ProbeAdb @('install', '-r', (Join-Path $probeRoot 'build\outputs\apk\debug\touch-probe-debug.apk'))
Invoke-ProbeAdb @('install', '-r', (Join-Path $probeRoot 'build\outputs\apk\androidTest\debug\touch-probe-debug-androidTest.apk'))
if ($Production) { Invoke-ProbeAdb @('install', '-r', (Join-Path $PSScriptRoot 'app\build\outputs\apk\debug\app-debug.apk')) }
Invoke-ProbeAdb @('shell', 'appwidget', 'grantbind', '--package', 'pl.onkyo.remote.touchprobe', '--user', '0')
try {
    $output = Invoke-ProbeAdb @('shell', 'am', 'instrument', '-w', '-r', '-e', 'production', $Production.IsPresent.ToString().ToLowerInvariant(),
        'pl.onkyo.remote.touchprobe.test/pl.onkyo.remote.touchprobe.TouchProbeRunner')
    $report = $output -join "`n"
    [IO.File]::WriteAllText((Join-Path $reportRoot 'result.txt'), $report)
    Write-Output $report
    Invoke-ProbeAdb @('pull', '/sdcard/Android/data/pl.onkyo.remote.touchprobe/files/.', $reportRoot)
    if ($report -notmatch 'stream=PASS' -or $report -notmatch 'INSTRUMENTATION_CODE: -1') { throw 'Remote touch probe did not pass.' }
} finally {
    Invoke-ProbeAdb @('shell', 'appwidget', 'revokebind', '--package', 'pl.onkyo.remote.touchprobe', '--user', '0')
}
