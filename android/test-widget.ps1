param([string]$DeviceSerial = 'emulator-5560')
$ErrorActionPreference = 'Stop'
if ($DeviceSerial -notmatch '^emulator-\d+$') { throw 'This smoke test is intended for an Android emulator.' }
$sdkRoot = $env:ANDROID_HOME
if (-not $sdkRoot) { $sdkRoot = Join-Path $env:LOCALAPPDATA 'Android\Sdk' }
$adbPath = Join-Path $sdkRoot 'platform-tools\adb.exe'
$resultDirectory = Join-Path $PSScriptRoot 'app\build\reports\widget-smoke'
New-Item -ItemType Directory -Path $resultDirectory -Force | Out-Null
function Invoke-TestAdb {
    param([string[]]$Arguments)
    $output = & $adbPath -s $DeviceSerial @Arguments
    if ($LASTEXITCODE -ne 0) { throw ($output -join "`n") }
    return $output
}
Invoke-TestAdb @('install', '-r', (Join-Path $PSScriptRoot 'app\build\outputs\apk\debug\app-debug.apk'))
Invoke-TestAdb @('install', '-r', (Join-Path $PSScriptRoot 'app\build\outputs\apk\androidTest\debug\app-debug-androidTest.apk'))
Invoke-TestAdb @('shell', 'appwidget', 'grantbind', '--package', 'pl.onkyo.remote', '--user', '0')
try {
    $output = Invoke-TestAdb @('shell', 'am', 'instrument', '-w', '-r', '-e', 'screenshot', '/sdcard/Android/data/pl.onkyo.remote/files/widget.png',
        '-e', 'smallScreenshot', '/sdcard/Android/data/pl.onkyo.remote/files/widget-small.png',
        '-e', 'alarmScreenshot', '/sdcard/Android/data/pl.onkyo.remote/files/alarm.png',
        'pl.onkyo.remote.test/pl.onkyo.remote.WidgetSmokeTestRunner')
    $report = $output -join "`n"
    [IO.File]::WriteAllText((Join-Path $resultDirectory 'result.txt'), $report)
    Write-Output $report
    if ($report -notmatch 'PASS:' -or $report -notmatch 'INSTRUMENTATION_CODE: -1') { throw 'Widget smoke test failed.' }
    Invoke-TestAdb @('pull', '/sdcard/Android/data/pl.onkyo.remote/files/widget.png', (Join-Path $resultDirectory 'widget.png'))
    Invoke-TestAdb @('pull', '/sdcard/Android/data/pl.onkyo.remote/files/widget-small.png', (Join-Path $resultDirectory 'widget-small.png'))
    foreach ($image in @('volume-up-pressed.png', 'volume-down-pressed.png')) {
        Invoke-TestAdb @('pull', ('/sdcard/Android/data/pl.onkyo.remote/files/' + $image), (Join-Path $resultDirectory $image))
    }
    Invoke-TestAdb @('pull', '/sdcard/Android/data/pl.onkyo.remote/files/alarm.png', (Join-Path $resultDirectory 'alarm.png'))
    Invoke-TestAdb @('pull', '/sdcard/Android/data/pl.onkyo.remote/files/time-editor.png', (Join-Path $resultDirectory 'time-editor.png'))
    Invoke-TestAdb @('pull', '/sdcard/Android/data/pl.onkyo.remote/files/source-picker.png', (Join-Path $resultDirectory 'source-picker.png'))
} finally {
    Invoke-TestAdb @('shell', 'appwidget', 'revokebind', '--package', 'pl.onkyo.remote', '--user', '0')
}
