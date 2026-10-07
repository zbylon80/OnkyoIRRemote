"""Build/run restart and volume safety with simulated hardware (Windows/MSVC)."""
from pathlib import Path
import json
import re
import subprocess

root = Path(__file__).resolve().parents[1]
sketch = root / 'firmware/OnkyoRemote'
source = (sketch / 'OnkyoRemote.ino').read_text(encoding='utf-8')
functions = ('restartUptimeMs', 'recordUserActivity', 'onTimeSynchronized',
             'startTimeSynchronization', 'onWiFiDiagnostics', 'resetReasonName', 'handleDiagnostics',
             'handleDiagnosticsPage', 'handleDailyRestart', 'stopVolume',
             'readVolumeSession', 'handleVolumePress', 'expireVolumeHold', 'handleVolumeStart',
             'handleVolumeKeepalive', 'handleVolumeStop', 'repeatHeldVolume', 'handleVersion',
             'handleRoot', 'handleManifest', 'handleIcon', 'startOta',
             'persistAlarms', 'handleAlarmSettings', 'readAlarmTime', 'saveAlarmSettings', 'handleAlarms',
             'handleRestartRequest', 'handleRequestedRestart')
timeout = re.search(r'^constexpr uint32_t OTA_RECEIVE_TIMEOUT_MS = \d+;', source, re.M)
if timeout is None:
    raise SystemExit('Cannot find OTA timeout in milliseconds.')
pieces = [timeout.group()]
for name in functions:
    match = re.search(rf'^(?:uint64_t|bool|void|const char \*)\s*{name}\([^\n]*\) \{{.*?^\}}', source, re.M | re.S)
    if match is None:
        raise SystemExit(f'Cannot find actual sketch function: {name}')
    pieces.append(match.group())
build = sketch / 'build/daily-restart-tests'
build.mkdir(parents=True, exist_ok=True)
(build / 'daily_restart_under_test.inc').write_text('\n\n'.join(pieces), encoding='utf-8')
vswhere = Path(r'C:\Program Files (x86)\Microsoft Visual Studio\Installer\vswhere.exe')
installation = subprocess.check_output([
    str(vswhere), '-latest', '-products', '*', '-requires',
    'Microsoft.VisualStudio.Component.VC.Tools.x86.x64', '-property', 'installationPath'
], text=True).strip()
if not installation:
    raise SystemExit('MSVC C++ build tools are required for this host test.')
vcvars = Path(installation) / 'VC/Auxiliary/Build/vcvars64.bat'
runner = build / 'run.cmd'
runner.write_text(
    f'@echo off\ncall "{vcvars}" >nul\nif errorlevel 1 exit /b %errorlevel%\n'
    f'cl /nologo /EHsc /std:c++17 /utf-8 /I"{build}" /I"{sketch}" '
    f'"{root / "tests/daily_restart_tests.cpp"}" '
    f'/Fe:"{build / "daily_restart_tests.exe"}" /Fo:"{build / "daily_restart_tests.obj"}"\n'
    f'if errorlevel 1 exit /b %errorlevel%\n"{build / "daily_restart_tests.exe"}"\n',
    encoding='utf-8'
)
result = subprocess.run(['cmd.exe', '/d', '/c', str(runner)], cwd=build, capture_output=True, text=True)
if result.returncode:
    print(result.stdout, result.stderr)
    raise SystemExit(result.returncode)
checked = 0
for line in result.stdout.splitlines():
    if line.startswith('ALARMS:'):
        data = json.loads(line.split(':', 1)[1])
        assert data['clockReady'] and data['storageReady']
        assert data['off'] == {'enabled': True, 'time': '02:00', 'date': '2026-10-03'}
        assert data['on'] == {'enabled': True, 'time': '07:00', 'date': '2026-10-02', 'source': 'CD'}
        continue
    if line.startswith('DIAGNOSTICS_'):
        label, body = line.split(':', 1)
        data = json.loads(body)
        version = re.search(r'ONKYO_FIRMWARE_VERSION "([^"]+)"', (sketch / 'FirmwareVersion.h').read_text()).group(1)
        assert data['version'] == version and data['volumeUpLimitMs'] == 3000
        assert data['volumeWatchdogMs'] == 2000 and data['reset']['reason'] == 'software'
        assert data['memory']['freeBytes'] == 180000 and data['memory']['minimumFreeBytes'] == 160000
        assert not any(f'"{key}"' in body.lower() for key in ('password', 'ssid', 'macaddress', 'otapassword', 'wifissid'))
        if label == 'DIAGNOSTICS_CONNECTED':
            assert data['wifi']['rssiDbm'] == -64 and not data['wifi']['sleepEnabled']
            assert data['wifi']['connections'] == 2 and data['wifi']['disconnects'] == 1
            assert data['wifi']['lastDisconnectReason'] == 201
            assert data['wifi']['lastDisconnectUptimeSeconds'] == 90
            assert data['uptimeSeconds'] == 90 and data['loopMaxMs'] == 75
        else:
            assert not data['wifi']['connected'] and data['wifi']['sleepEnabled']
            assert data['wifi']['rssiDbm'] is None and data['wifi']['lastDisconnectReason'] is None
        checked += 1
    elif line:
        print(line)
assert checked == 2
print('Diagnostics JSON schema and event data verified.')
