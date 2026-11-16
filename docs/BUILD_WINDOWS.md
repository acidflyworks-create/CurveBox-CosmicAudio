# CurveBox на Windows: сборка по шагам + разбор ошибок

Проверено на: Windows 10/11 x64, Visual Studio 2022 Build Tools (MSVC 19.4x),
CMake 3.27+, Git 2.4x. Время: ~5 минут на установку (один раз) и ~6–10 минут на
первую сборку (дальше 20–40 секунд правки одного файла).

---

## 0. Что должно получиться

```
C:\dev\CurveBox\build\CurveBox_artefacts\Release\
├─ VST3\CurveBox.vst3\CurveBox.vst3      <- плагин (это DLL, но с расширением .vst3)
└─ Standalone\CurveBox.exe               <- отдельное приложение для тестов
```

---

## 1. Установка инструментов (один раз)

### 1.1 Через winget (Windows 10 1809+ / 11)

Откройте **PowerShell** (обычный, не обязательно админ) и выполните три команды —
лучше по одной, чтобы видеть результат:

```powershell
winget install --id Microsoft.VisualStudio.2022.BuildTools -e `
  --accept-source-agreements --accept-package-agreements `
  --override "--wait --quiet --norestart --add Microsoft.VisualStudio.Workload.VCTools --includeRecommended"

winget install --id Kitware.CMake -e
winget install --id Git.Git -e
```

Построчно, что значат флаги первой команды:

| Часть | Смысл |
|---|---|
| `--id ...BuildTools` | ставим **только** Build Tools (без тяжёлой IDE) — компилятор + Windows SDK |
| `--override "--wait --quiet --norestart ..."` | передаём аргументы установщику VS: ждать завершения, без вопросов, без авто-перезагрузки |
| `--add Microsoft.VisualStudio.Workload.VCTools` | добавляем рабочую нагрузку «Desktop development with C++» — без неё нет `cl.exe`, и CMake скажет «No CMAKE_CXX_COMPILER could be found» |
| `--includeRecommended` | тянет MSVC v143 + актуальный Windows SDK (нужен для линковки) |

> Ставится ~2–4 ГБ, поэтому `--quiet` идёт долго. Это нормально.

### 1.2 Через установщик (если winget недоступен)

1. Скачать **Visual Studio 2022 Build Tools** с сайта Microsoft.
2. В установщике отметить **Desktop development with C++** (справа в списке
   должны быть MSVC v143 и Windows 10/11 SDK).
3. Отдельно поставить **CMake** (cmake.org → Windows x64 Installer, при установке
   выбрать «Add CMake to the system PATH») и **Git for Windows**.
4. **Перезапустить PowerShell**, чтобы обновился `PATH`.

### 1.3 Проверка

```powershell
cmake --version      # нужно >= 3.22
git --version
& "C:\Program Files (x86)\Microsoft Visual Studio\Installer\vswhere.exe" -latest `
   -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationVersion
```
Последняя команда должна вывести версию вида `17.9.34728.123`. Если пусто —
не установлен C++ workload.

---

## 2. Исходники

```powershell
mkdir C:\dev
cd C:\dev
git clone <URL-вашего-репозитория> CurveBox
cd CurveBox
```

Если репозитория нет — просто распакуйте папку `CurveBox` в `C:\dev\CurveBox`.

> **Важно.** Путь без пробелов, кириллицы, OneDrive и «Мои документы».
> На путях вида `C:\Users\Иван\OneDrive\Рабочий стол\...` регулярно ловятся
> `MSB6006`, `LNK1104` и «файл занят».

---

## 3. Configure (генерация решения)

```powershell
cmake -S . -B build -G "Visual Studio 17 2022" -A x64
```

Построчно:

| Токен | Что делает |
|---|---|
| `cmake` | запускает конфигуратор |
| `-S .` | источник — текущий каталог (там лежит `CMakeLists.txt`) |
| `-B build` | каталог сборки `build\` создаётся автоматически; всё мусорное внутри него |
| `-G "Visual Studio 17 2022"` | генератор = VS 2022 (multi-config: Debug/Release). Если VS не найден — см. ошибку №2 ниже |
| `-A x64` | архитектура 64 бита (для VST3 обязательна) |

Что происходит внутри (по строкам вывода):

1. `-- The C compiler identification is MSVC ...` — CMake нашёл компилятор.
2. `-- Performing Test CMAKE_HAVE_LIBC_PTHREAD` и прочие тесты — обычные пробы.
3. **FetchContent**: клонируется JUCE 8.0.15 в `build\_deps\juce-src` (~100 МБ,
   нужен интернет; повторные конфигурации используют кэш).
4. Собирается вспомогательная утилита `juceaide`
   (`[ 20%] Building CXX object ... juce_gui_basics.cpp.o`) — JUCE генерирует ею
   заголовки и ресурсы. На Windows она собирается без проблем (на Linux нужны
   dev-пакеты X11 — именно на этом падала моя песочница).
5. `-- Configuring done` / `-- Generating done` и файл `build\CurveBox.sln`.

Полезные опции конфигурации (необязательные):

```powershell
# только VST3 (без Standalone) — быстрее итерации:
cmake -S . -B build -G "Visual Studio 17 2022" -A x64 -DCURVEBOX_BUILD_FORMATS="VST3"

# CMake сам копирует собранный VST3 в системную папку (нужен админ для сборки):
cmake -S . -B build -G "Visual Studio 17 2022" -A x64 -DCURVEBOX_COPY_PLUGIN_AFTER_BUILD=ON
```

Ожидаемый «плохой» исход: `Configuring incomplete`. Тогда смотрите §8 —
почти все случаи там разобраны дословно.

---

## 4. Build

```powershell
cmake --build build --config Release --target CurveBox_VST3 CurveBox_Standalone --parallel
```

Построчно:

| Токен | Что делает |
|---|---|
| `cmake --build build` | запускает сборку в каталоге `build` (не перегенерирует проект) |
| `--config Release` | конфигурация VS: **Release** (Debug-плагин в DAW обычно не грузится и в 5–10 раз медленнее) |
| `--target CurveBox_VST3 CurveBox_Standalone` | собираем только нужные цели, а не всё решение |
| `--parallel` | использовать все ядра (MSVC дополнительно включает `/MP`) |

Хвост успешной сборки выглядит так:

```
  CurveBox.vst3 -> C:\dev\CurveBox\build\CurveBox_artefacts\Release\VST3\CurveBox.vst3\CurveBox.vst3
  CurveBox.exe -> C:\dev\CurveBox\build\CurveBox_artefacts\Release\Standalone\CurveBox.exe
```

Только плагин, если Standalone не нужен:

```powershell
cmake --build build --config Release --target CurveBox_VST3 --parallel
```

---

## 5. Проверить, что файлы на месте

```powershell
dir build\CurveBox_artefacts\Release\VST3
dir build\CurveBox_artefacts\Release\VST3\CurveBox.vst3
```

Для Windows `.vst3` — это **папка**, внутри которой лежит файл `CurveBox.vst3`
(по сути DLL). Копировать нужно папку целиком.

---

## 6. Установка в систему

PowerShell **от имени администратора**:

```powershell
Copy-Item -Recurse -Force `
  "C:\dev\CurveBox\build\CurveBox_artefacts\Release\VST3\CurveBox.vst3" `
  "C:\Program Files\Common Files\VST3\"
```

Проверка: `dir "C:\Program Files\Common Files\VST3\CurveBox.vst3"` — внутри должен
быть `CurveBox.vst3`.

Дальше — пересканировать плагины в хосте:

| Хост | Действие |
|---|---|
| Reaper | Options → Preferences → Plug-ins → VST → **Re-scan** |
| Ableton Live | Preferences → Plug-Ins → Plug-In Sources → **Rescan** (VST3 включён) |
| FL Studio | Options → Manage plug-ins → **Find more plugins** |
| Cubase / Nuendo | пересканирует VST3 сам при старте; при необходимости — Plugin Manager → Update |
| Studio One | View → Plug-in Manager → **Refresh** |

Плагин появится как **CurveBox** в категории Fx (VST3-категории «Fx» + «Distortion»).

---

## 7. Первый запуск и самопроверка

1. Запустите `build\CurveBox_artefacts\Release\Standalone\CurveBox.exe`
   (он же проверяет, что GUI рисуется и звук идёт без хоста).
2. В Standalone: Options → Audio/MIDI Settings → выбрать драйвер (WASAPI/DirectSound).
3. В шапке выбрать пресет **02 Pulsar Pump** → в «солнечной системе» кликнуть по
   жёлтой звезде (Volume) → кривая нарисуется, планета запульсирует, playhead побежит.
4. Включить/выключить шейпер: **Alt+клик** (или ПКМ) по небесному телу, либо тумблер
   **Enabled** справа под планетой.
5. Проверить, что работает автоматизация: покрутить Mix/Output — планета и фон
   должны плавно меняться.

Если в Standalone всё хорошо, а в DAW плагина нет — это §8, пункт «плагин не появился».

---

## 8. Разбор ошибок: текст → причина → что делать

### Конфигурация (cmake -S . -B build ...)

**1.** `CMake Error at .../JUCE/CMakeLists.txt:40 (message): A C compiler is required
to build targets that depend on JUCE. Add 'C' to your project's LANGUAGES.`
— в `project()` не включён язык C (JUCE 8 компилирует C-файлы).
→ В проекте уже `project(CurveBox VERSION 1.0.0 LANGUAGES C CXX)`. Если видите эту
ошибку, вы собираете не тот `CMakeLists.txt` (старая копия) — проверьте строку 4.

**2.** `CMake Error: Could not create named generator "Visual Studio 17 2022"`
— VS 2022 не установлен (или установлен только VS 2019).
→ Либо доустановите Build Tools 2022, либо используйте `-G "Visual Studio 16 2019"`.
Без генератора вообще: `cmake -S . -B build -G Ninja` (Ninja идёт с VS Build Tools).

**3.** `CMake Error ... No CMAKE_CXX_COMPILER could be found.`
— Build Tools стоят без рабочей нагрузки C++ (нет `cl.exe`).
→ VS Installer → Modify → **Desktop development with C++** → MSVC v143 + Windows SDK.

**4.** `fatal error: 'git' is not recognized as an internal or external command`
или `error: could not fetch 'JUCE'` — нет Git в `PATH` (FetchContent клонирует JUCE).
→ Поставьте Git, **перезапустите терминал** и повторите; либо удалите `build\` целиком.

**5.** `CMake Warning ... No download info given for 'JUCE'` /
`Failed to clone repository ... Could not resolve host`
— нет интернета/прокси/файрвол режет github.
→ Настройте прокси для git (`git config --global http.proxy ...`), либо скачайте JUCE
8.0.15 вручную и укажите локально:
`cmake -S . -B build -G "Visual Studio 17 2022" -A x64 -DFETCHCONTENT_SOURCE_DIR_JUCE=C:/dev/JUCE`.

**6.** `CMake 3.22 or higher is required. You are running version 3.2x` (старая)
— устаревший CMake.
→ `winget upgrade Kitware.CMake` или удалить другой CMake из PATH.

**7.** `CMake Error: Error: could not load cache` / странные ошибки после правок
— испорченный каталог сборки.
→ `Remove-Item -Recurse -Force build` и конфигурировать заново.

### Компиляция (cmake --build ...)

**8.** `error C1083: Cannot open include file: 'juce_audio_processors/juce_audio_processors.h':
No such file or directory`
— JUCE не докачался/докачался частично (антивирус, обрыв сети).
→ `Remove-Item -Recurse -Force build`, повторить configure; при повторе — исключить
папку проекта из антивируса.

**9.** `error C4819: The file contains a character that cannot be represented in the
current code page (1251/936)` → и следом `error C2220: warning treated as error`
— исходник прочитан не как UTF-8.
→ В проекте уже добавлен `/utf-8` (`if(MSVC) target_compile_options(... /utf-8 ...)`).
Если правите CMakeLists — не удаляйте эту строку. Вручную: `cl /utf-8 ...`.

**10.** `error C2039: 'FontOptions': is not a member of 'juce'` или
`error C2039: 'getName': ...` — компилируется **не JUCE 8** (например, 7.x из кэша).
→ Проверьте `GIT_TAG 8.0.15`; удалите `build\_deps` и переконфигурируйте.

**11.** `error MSB8020: The build tools for v142 (Platform Toolset = 'v142') cannot be found`
— остался старый `build\` от другой версии VS.
→ Удалить `build\`, конфигурировать заново с `-A x64`.

**12.** `LNK1104: cannot open file '...\CurveBox.vst3'` или
`LNK1168: cannot open ... for writing`
— файл занят: плагин загружен в запущенный хост/Standalone.
→ Закройте DAW и `CurveBox.exe` (Диспетчер задач), соберите снова.

**13.** `error MSB6006: "cmd.exe" exited with code 1`
— это «контейнерная» ошибка MSBuild: настоящая причина всегда в строках **выше**.
→ Прокрутите лог вверх на 20–30 строк и ищите первую строку с `error C....`/`error LNK....`

**14.** `error C1060: compiler is out of heap space` (редко, при 8 ГБ RAM и параллельной сборке)
→ `cmake --build build --config Release --target CurveBox_VST3` (без `--parallel`), или
`--parallel 2`.

**15.** Сборка идёт 20+ минут
— нет `/MP`/`--parallel` или собирается всё решение.
→ `--parallel`; первый раз всегда дольше: компилируются все модули JUCE, дальше
пересобираются только 2 файла проекта.

### Запуск и загрузка в хосте

**16.** `CurveBox.exe` не стартует: «VCRUNTIME140.dll not found», «MSVCP140.dll not found»
— нет Visual C++ Redistributable (Build Tools его не ставят в систему).
→ Установите «Microsoft Visual C++ 2015–2022 Redistributable (x64)».

**17.** Плагин не виден в DAW
→ проверьте по порядку: (a) файл лежит в `C:\Program Files\Common Files\VST3\CurveBox.vst3\CurveBox.vst3`;
(b) хост 64-битный и VST3 включён в настройках; (c) сделан **Re-scan**;
(d) собирали **Release**, а не Debug; (e) не блокирует ли антивирус новый DLL
(иногда нужно «разрешить» файл); (f) в Ableton VST3-папка `C:\Program Files\Common Files\VST3`
включена по умолчанию — если папку меняли, вернуть.

**18.** Плагин грузится, но окно пустое/чёрное
— видеодрайвер + софтверный рендер JUCE; на Windows встречается на RDP-сессиях.
→ Запустите хост локально (не через RDP); обновите драйвер GPU.

**19.** Плагин звучит, но не реагирует на темп
— хост не отдаёт PPQ при остановленном транспорте; это ожидаемо: кривые идут по
внутреннему счётчику битов от последнего BPM (см. README §7).

**20.** `warning C4996: 'strncpy': This function or variable may be unsafe`
— предупреждение (не ошибка) из JUCE при `/W4`.
→ Игнорируйте; в проекте используется `juce_recommended_warning_flags`.

---

## 9. Быстрый цикл разработки

```powershell
# 1) один раз: только VST3 + автокопирование в системную папку
cmake -S . -B build -G "Visual Studio 17 2022" -A x64 `
      -DCURVEBOX_BUILD_FORMATS="VST3" -DCURVEBOX_COPY_PLUGIN_AFTER_BUILD=ON

# 2) правки в Source\*.h / *.cpp -> 3) 20-40 секунд:
cmake --build build --config Release --target CurveBox_VST3 --parallel
```

Альтернатива — открыть `build\CurveBox.sln` в Visual Studio: там уже есть проекты
`CurveBox_VST3` и `CurveBox_Standalone`; поставьте Standalone стартовым и жмите F5 —
отладчик с точками останова прямо в `processBlock()`.

> При `COPY_PLUGIN_AFTER_BUILD=ON` копирование в `C:\Program Files\Common Files\VST3`
> требует прав администратора: запускайте сборку из PowerShell «от имени администратора»,
> либо не включайте авто-копирование и копируйте вручную (§6).

---

## 10. Если не хочется ставить Visual Studio

Соберите в облаке: в проекте есть `.github/workflows/build.yml`. Залейте папку в
репозиторий GitHub → вкладка **Actions** → **Run workflow** → через 8–12 минут
скачайте артефакт **CurveBox-Windows-x64** (там готовый `.zip` с `CurveBox.vst3` и
`CurveBox.exe`). Тег вида `v1.0.0` дополнительно создаёт GitHub Release с архивами
для Windows/macOS/Linux.
