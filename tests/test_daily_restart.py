"""Build/run restart and volume safety with simulated hardware (Windows/MSVC)."""
from pathlib import Path
import re
import subprocess

root = Path(__file__).resolve().parents[1]
sketch = root / 'firmware/OnkyoRemote'
source = (sketch / 'OnkyoRemote.ino').read_text(encoding='utf-8')
functions = ('restartUptimeMs', 'recordUserActivity', 'onTimeSynchronized',
             'startTimeSynchronization', 'handleDailyRestart', 'stopVolume',
             'readVolumeSession', 'handleVolumePress', 'expireVolumeHold', 'handleVolumeStart',
             'handleVolumeKeepalive', 'handleVolumeStop', 'repeatHeldVolume', 'handleVersion',
             'handleRoot', 'handleManifest', 'handleIcon', 'startOta')
pieces = []
for name in functions:
    match = re.search(rf'^(?:uint64_t|void) {name}\([^\n]*\) \{{.*?^\}}', source, re.M | re.S)
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
subprocess.run(['cmd.exe', '/d', '/c', str(runner)], cwd=build, check=True)
