# Сборка Windows x64

Исходники извлечены из архива `CurveBox-CosmicAudio.rar`. Нужны Windows x64,
Visual Studio 2022 с C++ и Windows SDK, CMake 3.22+, Git, Python и Inno Setup 6.

В PowerShell из корня проекта:

```powershell
./installer/build-windows.ps1
```

Скрипт собирает VST3, запускает DSP smoke-тест и проверку ID параметров,
проверяет архитектуру плагина и создаёт
`packages/CurveBox-VST3-Windows-x64-Setup.exe`. Установщик требует права
администратора; по умолчанию VST3 устанавливается в `C:\Program Files\Common Files\VST3`.
Установщик неподписан, поэтому Windows может показать предупреждение SmartScreen.

## VST2

В архиве нет VST2 SDK. Нужна собственная копия SDK Steinberg и необходимые права
на использование/распространение. SDK отдельно не загружается и не входит в
репозиторий. Передайте путь к его корню (в нём должен находиться
`pluginterfaces/vst2.x/aeffect.h`):

```powershell
./installer/build-windows.ps1 -Vst2SdkPath 'C:\SDKs\VST2_SDK'
```

Скрипт дополнительно соберёт `CurveBox.dll` и
`packages/CurveBox-VST2-Windows-x64-Setup.exe`. Путь VST2 по умолчанию —
`C:\Program Files\VstPlugins`; его можно изменить в установщике. Каждый
формат удаляется отдельно через приложения Windows.

GitHub Actions собирает только VST3 (SDK для VST2 в облако не загружается).
Артефакты VST3 и установщика доступны на странице успешного запуска workflow.
Тесты DSP и проверка заголовка PE не заменяют проверку плагина в DAW.
