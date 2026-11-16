$ErrorActionPreference = 'Stop'
$root = (Resolve-Path '.').Path
$test = Join-Path $env:RUNNER_TEMP 'curvebox-reaper'
New-Item -ItemType Directory -Force $test | Out-Null
$installer = Join-Path $test 'reaper-installer.exe'
Invoke-WebRequest 'https://www.reaper.fm/files/7.x/reaper786_x64-install.exe' -OutFile $installer
$setup = Start-Process $installer -ArgumentList @('/S', "/D=$test\app") -Wait -PassThru
if ($setup.ExitCode -ne 0) { throw "REAPER setup failed: $($setup.ExitCode)" }
$app = Join-Path $test 'app'
$plugins = Join-Path $test 'plugins'
New-Item -ItemType Directory -Force $plugins | Out-Null
Copy-Item 'build/CurveBox_artefacts/Release/VST3/CurveBox.vst3' $plugins -Recurse
@"
[REAPER]
vstpath64=$plugins
vst_scan=1
"@ | Set-Content -Encoding Ascii (Join-Path $app 'reaper.ini')
$env:CURVEBOX_REAPER_LOG = Join-Path $root 'reaper-smoke.log'
$lua = Join-Path $root '_tests/reaper/smoke.lua'
$p = Start-Process (Join-Path $app 'reaper.exe') -ArgumentList @('-newinst', '-new', "`"$lua`"") -PassThru
if (!$p.WaitForExit(120000)) { $p.Kill(); throw 'REAPER smoke test timed out (possibly a startup dialog)' }
if (Test-Path $env:CURVEBOX_REAPER_LOG) { Get-Content $env:CURVEBOX_REAPER_LOG }
if ($p.ExitCode -ne 0) { throw "REAPER exited with code $($p.ExitCode)" }
if (!(Test-Path $env:CURVEBOX_REAPER_LOG) -or !(Select-String -Quiet -Path $env:CURVEBOX_REAPER_LOG -Pattern '^PASS$')) { throw 'REAPER smoke test did not complete' }
