param(
    [string]$Vst2SdkPath = "",
    [string]$InnoSetup = "${env:ProgramFiles(x86)}\Inno Setup 6\ISCC.exe"
)
$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent $PSScriptRoot
Set-Location $root
$formats = 'VST3'
$targets = @('CurveBox_VST3', 'CurveBoxDSPTests')
$cmakeArgs = @('-S', '.', '-B', 'build', '-G', 'Visual Studio 17 2022', '-A', 'x64', '-DCURVEBOX_BUILD_TESTS=ON')
if ($Vst2SdkPath) {
    $sdk = (Resolve-Path $Vst2SdkPath).Path
    if (!(Test-Path (Join-Path $sdk 'pluginterfaces/vst2.x/aeffect.h'))) { throw 'VST2 SDK root must contain pluginterfaces/vst2.x/aeffect.h' }
    $formats = 'VST3 VST'
    $targets += 'CurveBox_VST'
    $cmakeArgs += "-DCURVEBOX_VST2_SDK_PATH=$sdk"
}
$cmakeArgs += "-DCURVEBOX_BUILD_FORMATS=$formats"
& cmake @cmakeArgs
if ($LASTEXITCODE -ne 0) { throw 'CMake configure failed' }
& cmake --build build --config Release --target @targets --parallel 2
if ($LASTEXITCODE -ne 0) { throw 'Build failed' }
& ctest --test-dir build -C Release --output-on-failure
if ($LASTEXITCODE -ne 0) { throw 'DSP tests failed' }
& python _tests/check_param_ids.py
if ($LASTEXITCODE -ne 0) { throw 'Parameter ID check failed' }
if (!(Test-Path $InnoSetup)) { throw "Inno Setup compiler not found: $InnoSetup" }
$artefacts = Join-Path $root 'build/CurveBox_artefacts/Release'
$jobs = @(@{ Format='VST3'; Dir='VST3'; Binary='CurveBox.vst3/Contents/x86_64-win/CurveBox.vst3' })
if ($Vst2SdkPath) { $jobs += @{ Format='VST2'; Dir='VST'; Binary='CurveBox.dll' } }
foreach ($job in $jobs) {
    $payload = Join-Path $artefacts $job.Dir
    & python _tests/check_windows_binary.py (Join-Path $payload $job.Binary)
    if ($LASTEXITCODE -ne 0) { throw "Invalid $($job.Format) plugin" }
    & $InnoSetup "/DPluginFormat=$($job.Format)" "/DPayloadDir=$payload" 'installer/CurveBox.iss'
    if ($LASTEXITCODE -ne 0) { throw "$($job.Format) installer build failed" }
}
Write-Host 'Finished. Installers are in packages/.'
