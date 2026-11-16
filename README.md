# CurveBox 1.0.0 — темпо-синхронный шейпер-мультиэффект (VST3 + Standalone)

Космический аудиоплагин на JUCE 8: 8 шейперов, каждый управляется рисуемой кривой,
синхронизированной с темпом хоста. Каждый эффект — небесное тело, ручки влияют на
планеты в реальном времени.

---

## 1. Архитектура: таблица файлов

| Файл | Строк | Содержимое |
|---|---:|---|
| `CMakeLists.txt` | 77 | JUCE 8 через FetchContent (GIT_TAG 8.0.15), `juce_add_plugin`, форматы VST3 + Standalone, VST3_CATEGORIES «Fx» «Distortion» |
| `Source/Params.h` | 213 | Таблицы модулей (префиксы id, цвета планет, признак send), id параметров, списки Rate (4 такта … 1/16T) и Delay Time (1/2 … 1/16T) с длительностями в долях |
| `Source/Curve.h` | 369 | `CurveShape`: список точек (x, y, tension), LUT 1024, lock-free публикация через два слота + atomic-индекс, генераторы заготовок (Flat/Saw вниз/вверх/Square/Triangle/Sine/Steps/Random), сериализация в XML |
| `Source/Effects.h` | 713 | TPT SVF, `Oversampler2x`, `DriveProc` (tanh + компенсация + 2x OS), `CrushProc` (bits + децимация + 2x OS), equal-power pan, `StereoDelay` (сглаживание времени «по-ленточному», Tone-LPF в петле, ping-pong), `GrainDelay` (24 зерна, окно Ханна, интерполяция, spray/pitch/feedback), `ReverbProc` (Freeverb: 8 comb + 4 allpass на канал + pre-delay) |
| `Source/Presets.h` | 519 | 40 фабричных пресетов: имя + значения параметров + кривые всех 8 слоёв; хелперы кривых (`pump`, `gate`, `steps`, `sine`, `riser`, `fall`…) |
| `Source/PluginProcessor.h/.cpp` | 166 / 832 | `AudioProcessor`, APVTS (52 параметра), кэш `std::atomic<float>*`, транспорт (BPM/PPQ + внутренний счётчик битов), обработка по сэмплам в порядке Filter→Drive→Crush→Grain→Delay→Reverb→Pan→Volume, send-архитектура, сброс буферов при выключении, программы хоста, state (APVTS + кривые в XML) |
| `Source/CurveEditor.h` | 466 | Редактор «орбитальная карта»: добавление/перетаскивание/удаление точек, tension правым/Alt-драгом, Snap (Shift — обход), меню Shape, комета-playhead 30 Гц |
| `Source/SpaceLookAndFeel.h` | 274 | Палитра, шрифты (`FontOptions`), rotary-слайдер как кольцо-орбита с «спутником», combo/кнопки/тумблеры, `drawGlow`, детерминированный хеш-рандом |
| `Source/PlanetView.h` | 772 | 8 «живых» планет: звезда с короной, газовый гигант с полосами/кольцами/штормом, вулканическая планета, пикселизующаяся луна, панорамная планета с трейлом, система лун-эхо, астероидный пояс, атмосфера+туманность. Все величины lerp-сглажены |
| `Source/PluginEditor.h/.cpp` | 310 / 571 | Header (пресеты «<» «>» + список, Shape, Snap, Enabled), `SolarSystemView` (8 тел-вкладок), CurveEditor, PlanetView, 43 контрола с attachments, layout со скейлом, кэш звёздного фона, hover-подсказки |
| `_tests/TestDSP.cpp` + `build_and_run.sh` + `check_param_ids.py` | — | Офлайн-тест DSP (не входит в сборку плагина) и скрипт сверки id пресетов с layout |

Итого: **5282 строки**, два .cpp-файла (остальное — header-only по требованию к структуре).

### Поток сигнала

```
in ──► Filter ─► Drive ─► Crush ─►┬──────────────┬──────────────┬──────────────┬──► Pan ─► Volume ─► Mix/Output ─► out
                                   │ Grain (send) │ Delay (send) │ Reverb(send) │
                                   └──── + wet ───┴──── + wet ───┴──── + wet ───┘
```

* Send-эффекты получают `сигнал × curve × Send`; их возврат **добавляется** к сигналу,
  поэтому хвосты дозвучивают при падении кривой в ноль.
* Volume последний — им «качаются» и хвосты эффектов.
* `Mix` — глобальный dry/wet, `Output` — −24…+12 дБ, `Smooth` — сглаживание кривой
  0…50 мс (one-pole), `Offset` — сдвиг фазы 0…360°.

### Кривая без блокировок

GUI-поток владеет точками, при каждом изменении строит LUT на 1024 отсчёта в
**неактивный** слот и публикует его `store(release)`. Аудио-поток читает
`load(acquire)` + линейную интерполяцию. Ни аллокаций, ни мьютексов, ни `new`.

---

## 2. CMakeLists.txt

Файл лежит в корне проекта. Ключевое:

```cmake
cmake_minimum_required(VERSION 3.22)
project(CurveBox VERSION 1.0.0 LANGUAGES C CXX)   # 'C' обязателен для JUCE 8

include(FetchContent)
FetchContent_Declare(JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG        8.0.15
    GIT_SHALLOW    TRUE)
FetchContent_MakeAvailable(JUCE)

juce_add_plugin(CurveBox
    COMPANY_NAME "CurveBox Audio"  BUNDLE_ID "com.curveboxaudio.curvebox"
    IS_SYNTH FALSE  NEEDS_MIDI_INPUT TRUE  NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE  EDITOR_WANTS_KEYBOARD_FOCUS TRUE
    COPY_PLUGIN_AFTER_BUILD FALSE
    PLUGIN_MANUFACTURER_CODE Cvbx  PLUGIN_CODE Crbx
    FORMATS VST3 Standalone
    PRODUCT_NAME "CurveBox"
    VST3_CATEGORIES "Fx" "Distortion"
    AU_MAIN_TYPE "kAudioUnitType_Effect"  AUMID "CurveBox"
    VST3_AUTO_MANIFEST TRUE)

target_sources(CurveBox PRIVATE Source/PluginProcessor.cpp Source/PluginEditor.cpp)
target_include_directories(CurveBox PRIVATE Source)
target_compile_definitions(CurveBox PUBLIC JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0 JUCE_DISPLAY_SPLASH_SCREEN=0 JUCE_REPORT_APP_USAGE=0
    JUCE_STRICT_REFCOUNTEDPOINTER=1 JUCE_MODAL_LOOPS_PERMITTED=0)
target_link_libraries(CurveBox
    PRIVATE juce::juce_audio_utils juce::juce_audio_processors juce::juce_dsp
    PUBLIC juce::juce_recommended_config_flags juce::juce_recommended_lto_flags
           juce::juce_recommended_warning_flags)
```

---

## 3. Исходники

Полные файлы находятся в рабочей папке (не дублирую 5000 строк в чат):

```
CurveBox/
├─ CMakeLists.txt
├─ Source/
│  ├─ Params.h
│  ├─ Curve.h
│  ├─ Effects.h
│  ├─ Presets.h
│  ├─ PluginProcessor.h / .cpp
│  ├─ CurveEditor.h
│  ├─ SpaceLookAndFeel.h
│  ├─ PlanetView.h
│  └─ PluginEditor.h / .cpp
└─ _tests/  (TestDSP.cpp, build_and_run.sh, check_param_ids.py)
```

---

## 4. Где взять готовый `.vst3`

**Готового файла для скачивания не существует.** Плагин написан с нуля именно в этом
проекте и никуда не публиковался, официальной сборки нет. Любой «CurveBox VST3
download» в интернете — это не он.

Собрать бинарник нужно один раз одним из трёх способов:

| Способ | Что нужно | Результат |
|---|---|---|
| **A. Локально на Windows** (быстрее всего) | Visual Studio 2022 Build Tools (C++), CMake, Git | `build\CurveBox_artefacts\Release\VST3\CurveBox.vst3` |
| **B. GitHub Actions** (собирает за вас) | только аккаунт GitHub | файл `CurveBox-Windows.zip` в разделе Actions → Artifacts |
| **C. Локально macOS / Linux** | Xcode CLT / пакеты из §4 ниже | `build/CurveBox_artefacts/Release/VST3/CurveBox.vst3` |

### Вариант B — «нажал и скачал» (workflow уже в проекте)

В проект добавлен `.github/workflows/build.yml`: он собирает VST3 + Standalone под
Windows, macOS и Linux и выкладывает их как артефакты.

1. Создайте пустой репозиторий на GitHub и загрузите туда папку `CurveBox`
   (кроме `build/`): `git init && git add . && git commit -m init && git push`.
2. Откройте вкладку **Actions** → workflow «Build CurveBox (VST3 + Standalone)» →
   **Run workflow** (или просто дождитесь сборки после push).
3. Через ~8–12 минут скачайте **Artifacts → CurveBox-Windows-x64** — внутри
   `CurveBox-Windows-x64.zip` с `CurveBox.vst3` (+ `CurveBox.exe`).
   Распакуйте в `C:\Program Files\Common Files\VST3\`.
4. Если запушить тег `v1.0.0` (`git tag v1.0.0 && git push --tags`), workflow сам
   создаст **GitHub Release** с тремя архивами (Windows / macOS universal / Linux) —
   это и есть «страница скачивания» плагина.

### Вариант A — минимум действий на Windows

> Полный пошаговый разбор для Windows (что делает каждый токен команды + таблица
> из 20 типовых ошибок сборки с причиной и лечением) — в файле
> **[`docs/BUILD_WINDOWS.md`](docs/BUILD_WINDOWS.md)**.


```bat
:: 1) один раз: поставьте "Visual Studio 2022 Build Tools" с компонентом
::    "Desktop development with C++", а также Git и CMake (или: winget install Kitware.CMake Git.Git)
cd CurveBox
cmake -S . -B build -G "Visual Studio 17 2022" -A x64
cmake --build build --config Release --target CurveBox_VST3 CurveBox_Standalone
:: 2) забрать плагин
explorer build\CurveBox_artefacts\Release\VST3
:: 3) скопировать CurveBox.vst3 в  C:\Program Files\Common Files\VST3\  (нужен админ)
```

Первый `cmake` скачивает JUCE 8.0.15 из GitHub (~100 МБ), поэтому нужен интернет.

Полезные опции конфигурации:

| Опция | Смысл |
|---|---|
| `-DCURVEBOX_BUILD_FORMATS="VST3"` | собрать только VST3 (быстрая итерация) |
| `-DCURVEBOX_COPY_PLUGIN_AFTER_BUILD=ON` | CMake сам копирует VST3 в системную папку (нужен админ) |
| `-DCURVEBOX_MACOS_UNIVERSAL=ON` | macOS: universal binary (arm64 + x86_64) |
| `-DFETCHCONTENT_SOURCE_DIR_JUCE=C:/dev/JUCE` | использовать локальную копию JUCE вместо скачивания |

### Почему я не могу просто отдать вам бинарник

Моя песочница пустая и без root: нет dev-пакетов X11/ALSA (`X11/extensions/Xrandr.h`),
поэтому `cmake`-конфигурация JUCE падает до компиляции плагина, а `.deb` я поставить
не могу. Плюс даже удачная сборка была бы **Linux**-бинарником из песочницы, а вам,
скорее всего, нужен Windows. Поэтому путь A/B выше — реальный и быстрый.

---

## 4.1. Сборка из исходников

### Все платформы (общее)

```bash
git clone <ваш-репозиторий> CurveBox && cd CurveBox
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release      # первый запуск скачает JUCE
cmake --build build --config Release -j
```

Цели: `CurveBox_VST3`, `CurveBox_Standalone`.

### Windows (MSVC 2022)

```bat
cmake -S . -B build -G "Visual Studio 17 2022" -A x64
cmake --build build --config Release --target CurveBox_VST3 CurveBox_Standalone
```
Артефакты: `build\CurveBox_artefacts\Release\VST3\CurveBox.vst3`,
`build\CurveBox_artefacts\Release\Standalone\CurveBox.exe`.
Копировать в `C:\Program Files\Common Files\VST3\` (нужны права администратора).
Либо поставьте `COPY_PLUGIN_AFTER_BUILD TRUE`, чтобы CMake копировал сам.

### macOS

```bash
cmake -S . -B build -G Xcode
cmake --build build --config Release --target CurveBox_VST3 CurveBox_Standalone
```
Копировать `build/CurveBox_artefacts/Release/VST3/CurveBox.vst3` в
`~/Library/Audio/Plug-Ins/VST3/` (или `/Library/Audio/Plug-Ins/VST3/`).
Сборка без подписи: при первом запуске может понадобиться
`xattr -dr com.apple.quarantine CurveBox.vst3` и/или ad-hoc подпись
`codesign --force --deep -s - CurveBox.vst3`.

### Linux (Debian/Ubuntu)

```bash
sudo apt install build-essential cmake git pkg-config \
     libasound2-dev libfreetype-dev libfontconfig1-dev \
     libx11-dev libxext-dev libxrandr-dev libxcursor-dev libxinerama-dev \
     libjack-jackd2-dev
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j"$(nproc)"
```
Копировать в `~/.vst3/` (создать каталог, если нет):
```bash
mkdir -p ~/.vst3 && cp -r build/CurveBox_artefacts/Release/VST3/CurveBox.vst3 ~/.vst3/
```
Standalone: `build/CurveBox_artefacts/Release/Standalone/CurveBox`.

---

## 5. Пресеты (40)

| № | Название | Модуль(и) | Rate | Что делает |
|---|---|---|---|---|
| 01 | Big Bang | — | — | Init: всё выключено, чиcтый сигнал |
| 02 | Pulsar Pump | Volume | 1/4 | Классический сайдчейн |
| 03 | Supernova Pump | Volume | 1/4 | Жёсткий памп, короткая атака |
| 04 | Orbit Gate | Volume | 1/8 | Гейт 50 % |
| 05 | Warp Drive | Volume | 1/16 | Транс-гейт |
| 06 | Zero Gravity | Volume | 1 такт | Медленный свелл вверх |
| 07 | Moon Tides | Volume | 1/2 | Синус-тремоло 45 % |
| 08 | Stellar Wind | Volume | 4 такта | Очень медленный райзер |
| 09 | Dark Matter Gate | Volume | 1/32 | Резкий строб-гейт |
| 10 | Escape Velocity | Volume | 1/16T | Триольный ступенчатый гейт |
| 11 | Gravity Wobble | Filter | 1/8 | LP-вобл, Res 68 %, 900 Гц |
| 12 | Solar Flare Sweep | Filter | 1 такт | LP-свип вверх |
| 13 | Red Shift | Filter | 1 такт | LP-свип вниз |
| 14 | Blue Shift | Filter | 2 такта | HP-райзер |
| 15 | Acid Orbit | Filter | 1/16 | BP-«acid», Res 88 % |
| 16 | Comet Talk | Filter | 1/4 | BP-«говорящие» ступени |
| 17 | Nebula Drift | Filter | 2 такта | Медленный дрейф LP |
| 18 | Solar Burn | Drive | 1/4 | Постоянный тёплый драйв |
| 19 | Meteor Shower | Drive | 1/16 | Ритмический драйв |
| 20 | Iron Core | Crush | 1/8 | Биткраш 6 бит + децимация ×4 |
| 21 | Static Void | Crush | 1 такт | Экстремальный краш 3 бита, ×24 |
| 22 | Pulsar Static | Drive + Crush | 1/16 | Драйв + гейтированный краш |
| 23 | Quasar Fuzz | Drive | 1 такт | Фузз-свелл |
| 24 | Binary Star | Pan | 1/4 | Автопан 100 % |
| 25 | Orbital Sway | Pan | 1/2 | Треугольный пан 80 % |
| 26 | Asteroid Tumble | Pan | 1/8 | Ступенчатый пан |
| 27 | Stardust Shimmer | Pan | 1/16 | Быстрый мелкий пан |
| 28 | Cosmic Storm | Filter + Drive + Pan | 1/8 / 1/4 | Комбинация «шторм» |
| 29 | Galactic Pump | Volume + Reverb | 1/4 | Памп + дакинг реверба |
| 30 | Gravitational Collapse | Filter + Volume + Delay | 1 такт | Всё падает вниз, эхо |
| 31 | Deep Space Echo | Delay | 1/4 | Дуб-эхо, ping-pong, FB 55 % |
| 32 | Satellite Ping | Delay | 1/8. | Пинг-понг пунктиром |
| 33 | Black Hole Gate | Reverb | 1/4 | Gated reverb (квадрат) |
| 34 | Ducked Nebula | Reverb + Volume | 1/4 | Ducked reverb |
| 35 | Starlight Echo | Delay + Filter | 1/4. | Ритмичное эхо + свип LP |
| 36 | Nebula Dust | Grain | 1 такт | Grain cloud, spray 120 мс |
| 37 | Octave Comets | Grain | 1 такт | Зёрна на октаву выше |
| 38 | Granular Stutter | Grain | 1/16 | Гранулярный стуттер |
| 39 | Space Ambience | Reverb | 2 такта | Большая атмосфера |
| 40 | Event Horizon | всё | 1/16 / 1/8 / 1/4 | Total chaos |

Загрузка пресета: **сначала все параметры сбрасываются в default**
(`setValueNotifyingHost(getDefaultValue())`), затем применяются значения пресета,
затем кривые, и сбрасываются буферы DSP. Пресеты доступны как программы хоста
(`getNumPrograms` = 40).

---

## 6. Управление редактором

| Действие | Результат |
|---|---|
| Клик по пустому месту графика | Добавить точку и сразу тащить |
| Перетаскивание точки | Двигать (крайние точки — только по Y) |
| Двойной клик по точке | Удалить (крайние защищены) |
| ПКМ или Alt + драг по сегменту | Изгиб сегмента (tension, степенная функция) |
| Shift при перетаскивании | Временно отключить Snap |
| `Snap` | Привязка к сетке 1/16 по X и 1/8 по Y (Y-snap) |
| `Shape` | Flat / Saw down / Saw up / Square / Triangle / Sine / Steps / Random steps |
| Клик по телу в «солнечной системе» | Выбрать шейпер |
| Alt/ПКМ по телу | Включить/байпасить шейпер |
| Hover по контролу | Внизу: имя параметра и отформатированное значение |

---

## 7. Известные нюансы и ограничения

1. **Полная сборка/линковка не проверена в моём окружении.** Проверено:
   оба `.cpp` компилируются в объектные файлы против реальных заголовков JUCE 8.0.15
   (`-std=c++17 -Wall -Wextra`, 0 ошибок и 0 предупреждений), плюс офлайн-тест DSP
   и сверка id пресетов. Линковка VST3 требует системных dev-пакетов
   (X11/Xrandr/Xcursor, ALSA), которых в песочнице нет и установить их без root нельзя.
   CMake-конфигурация доходит до сборки `juceaide` и падает именно на отсутствующем
   `X11/extensions/Xrandr.h` — это дефект окружения, не кода.
2. **LUT кривых**: двойная буферизация с atomic-индексом. Формально поток-читатель,
   успевший загрузить старый индекс, может читать слот, который писатель начнёт
   перезаписывать при следующем редактировании. На практике (float-запись, x86/ARM)
   это безвредно; строго корректный вариант — 3 слота с epoch-счётчиком.
3. **Глубина (Depth/Send) и громкость не сглаживаются** отдельным сглаживателем —
   сглаживается только кривая (`Smooth`). Резкая автоматизация ручки Depth может дать
   лёгкий зиппер; лечится добавлением `SmoothedValue` на amount.
4. **Reverb** — Freeverb-подобный: при Size ~100 % слышен «металлический» призвук,
   модуляция не предусмотрена. Тюнинги линий масштабируются от 44.1 кГц.
5. **Оversampling** реализован как простой 2× (zero-stuff + 2 каскадных SVF-LP вверх/вниз)
   без компенсации задержки — фаза не идеальна, зато нет аллокаций и латентности.
6. **GUI**: фон перерисовывается целиком (таймер 40 мс) ради мерцания звёзд;
   звёздное поле, текстура полос газового гиганта кэшируются в `juce::Image` и
   пересобираются только в `resized()` / при смене режима фильтра. OpenGL не подключён.
7. **Не реализовано из «дополнительно»**: TimeShaper/Width, несколько кривых на шейпер,
   сохранение пользовательских пресетов в файлы, OpenGL. Реализовано: 2× oversampling
   Drive/Crush, запуск кривой по MIDI-ноте (`MIDI Retrigger`), Y-snap.
8. `getTailLengthSeconds() = 4.0` соответствует максимальному времени delay (1/2 при
   очень медленном темпе) и хвосту реверба.
9. Моно-режим поддержан (`BusesProperties` stereo, layout mono→mono разрешён),
   но send-эффекты обрабатывают один канал как два одинаковых — стерео-разница
   в моно не слышна по определению.
10. Пресеты «сбрасывают всё в default», поэтому **любая ручная правка** после загрузки
    пресета не сохраняется в пресет — для этого нужны user-пресеты (см. п. 7).

---

## 8. Проверки, которые прогонялись

```bash
# 1) Компиляция обоих TU против JUCE 8.0.15 (без линковки)
cd _tests && JUCE_DIR=/path/to/JUCE ./syntax_check.sh
#   OK: both translation units compiled with no warnings

# 2) DSP-смоук-тест (хвосты, NaN, границы)
cd _tests && JUCE_DIR=/path/to/JUCE ./build_and_run.sh
#   presets: 40
#   ALL TESTS PASSED

# 3) Сверка идентификаторов пресетов с layout APVTS
python3 _tests/check_param_ids.py
#   layout parameters : 52 / preset parameters : 47
#   OK: every preset parameter id exists in the APVTS layout
```

Тест проверяет: форму кривой и tension, границы SVF/Drive/Crush, что хвост delay
и reverb **звучит после того, как send упал в ноль**, что грануляр реально выдаёт
сигнал и не выходит за границы, корректность 40 пресетов (2+ точки, x ∈ [0,1]).

---

## 9. Что улучшить дальше

1. **User-пресеты в файлы** (`.cbpreset`, XML/JSON в `~/Documents/CurveBox/Presets`):
   кнопки Save/Init/Load, `getNumPrograms` расширить на пользовательские.
2. **Несколько кривых на шейпер** (например, Volume: amplitude + pan) — интерфейс
   уже позволяет добавить переключатель «Curves A/B»: `CurveShape` уже массивоподобен,
   нужен лишь второй LUT и второй селектор в процессоре.
3. **TimeShaper и Width**: TimeShaper — модуляция позиции чтения кольцевого буфера
   (реверс/стоп/полускорость), Width — M/S-матрица с кривой.
4. **Сглаживание всех ручек + anti-zipper**: `SmoothedValue` на Depth/Output/Send,
   плюс `juce::dsp::Oversampling` с корректной задержкой вместо самописного 2×.
5. **GPU/производительность UI**: `OpenGLContext` на `PlanetView` (опция в CMake уже
   обсуждалась), dirty-rect перерисовка фона, снижение таймера до 20 Гц на слабых машинах,
   A/B-сравнение и undo для редактора кривых.
