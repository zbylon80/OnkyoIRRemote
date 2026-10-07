param([switch]$WithTests, [switch]$WithTouchProbe, [switch]$Offline)
$ErrorActionPreference = 'Stop'
$androidRoot = $PSScriptRoot
if (-not $env:JAVA_HOME) {
    $bundledJava = 'C:\Program Files\Android\Android Studio\jbr'
    if (Test-Path -LiteralPath $bundledJava) { $env:JAVA_HOME = $bundledJava }
}
if (-not (Test-Path -LiteralPath (Join-Path $androidRoot 'local.properties'))) {
    $sdkPath = $env:ANDROID_HOME
    if (-not $sdkPath) { $sdkPath = Join-Path $env:LOCALAPPDATA 'Android\Sdk' }
    if (-not (Test-Path -LiteralPath $sdkPath)) { throw 'Set ANDROID_HOME to an installed Android SDK.' }
    $sdkProperty = $sdkPath.Replace('\', '/').Replace(':', '\:')
    [IO.File]::WriteAllText((Join-Path $androidRoot 'local.properties'), "sdk.dir=$sdkProperty`n")
}
Push-Location $androidRoot
try {
    $tasks = @('--console=plain', ':app:assembleDebug', ':app:lintDebug')
    if ($WithTests) { $tasks += ':app:assembleDebugAndroidTest' }
    if ($WithTouchProbe) { $tasks += @('-PtouchProbe', ':touch-probe:assembleDebug', ':touch-probe:assembleDebugAndroidTest') }
    if ($Offline) { $tasks += '--offline' }
    & .\gradlew.bat @tasks
    if ($LASTEXITCODE -ne 0) { throw "Android build failed: $LASTEXITCODE" }
    $packagePath = Join-Path $androidRoot 'app\build\outputs\apk\debug\app-debug.apk'
    $versionMatch = [regex]::Match((Get-Content -LiteralPath (Join-Path $androidRoot 'app/build.gradle') -Raw), "versionName\s+'([^']+)'")
    if (-not $versionMatch.Success) { throw 'Cannot read APK version.' }
    $artifactDirectory = Join-Path $androidRoot 'artifacts'
    New-Item -ItemType Directory -Path $artifactDirectory -Force | Out-Null
    $artifactPath = Join-Path $artifactDirectory ("OnkyoRemote-Android-" + $versionMatch.Groups[1].Value + '.apk')
    Copy-Item -LiteralPath $packagePath -Destination $artifactPath
    Write-Output "APK: $artifactPath"
} finally {
    Pop-Location
}
