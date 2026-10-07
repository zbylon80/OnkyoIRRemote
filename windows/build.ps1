param([switch]$Test, [switch]$Portable)
$ErrorActionPreference = 'Stop'
$workspace = $PSScriptRoot
$env:DOTNET_CLI_HOME = Join-Path $workspace '.tools/dotnet-home'
$env:DOTNET_NOLOGO = '1'
$env:DOTNET_CLI_TELEMETRY_OPTOUT = '1'
$env:DOTNET_ADD_GLOBAL_TOOLS_TO_PATH = '0'
$env:NUGET_PACKAGES = Join-Path $workspace '.tools/nuget-packages'
$project = Join-Path $workspace 'OnkyoRemote.Desktop/OnkyoRemote.Desktop.csproj'
& dotnet build $project -c Release --nologo -v minimal -p:RestoreIgnoreFailedSources=true
if ($LASTEXITCODE -ne 0) { throw 'Application build failed.' }
if ($Test) {
    & dotnet run --project (Join-Path $workspace 'OnkyoRemote.Tests/OnkyoRemote.Tests.csproj') -c Release -- (Join-Path $workspace 'artifacts/tests')
    if ($LASTEXITCODE -ne 0) { throw 'Tests failed.' }
}
if ($Portable) {
    $version = ([xml](Get-Content -LiteralPath $project -Raw)).Project.PropertyGroup.Version
    $output = Join-Path $workspace "artifacts/portable-$version"
    & dotnet publish $project -c Release -r win-x64 --self-contained true -o $output --nologo -v minimal -p:PublishSingleFile=true -p:IncludeNativeLibrariesForSelfExtract=true -p:DebugType=None -p:DebugSymbols=false
    if ($LASTEXITCODE -ne 0) { throw 'Portable publish failed.' }
    Copy-Item -LiteralPath (Join-Path $workspace 'README.md') -Destination (Join-Path $output 'README.md')
    Compress-Archive -Path (Join-Path $output 'OnkyoRemote.Windows.exe'), (Join-Path $output 'README.md') -DestinationPath (Join-Path $workspace "artifacts/OnkyoRemote-Windows-$version-x64.zip") -Force
}
