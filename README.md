# Voopoo Firmware Studio

Редактор прошивок **VOOPOO DRAG S** (`.zwdf` / `.swdf`) для Windows 10/11 x64.
Работает с файлами; **не прошивает устройство по USB**.

## Возможности

- Редактирование 119 графических ресурсов, импорт и экспорт PNG, сравнение с заводским образом.
- Пиксельный редактор: карандаш, ластик, пипетка, заливка, буфер обмена.
- Темы и ручная настройка цветов RGB565.
- Генерация 46 глифов из TTF/OTF для четырёх типоразмеров.

## Запуск

Скачайте `VoopooFirmwareStudio-<версия>-Release.exe` из [Releases](../../releases) и запустите. Установка не требуется.

Встроенный образ и ограничения: [Прошивка VOOPOO DRAG S](docs/firmware.md).

Drag & drop: прошивка, файл шрифта или папка с PNG для пакетного импорта по именам ресурсов.

## Сборка

C++20, Dear ImGui, DirectX 11, статический CRT (`/MT`). Нужны Visual Studio 2022/2026 с C++ x64 и Windows SDK.

В Developer Command Prompt:

```cmd
msbuild VoopooFirmwareStudio.sln /p:Configuration=Release /p:Platform=x64
```

Результат: `build/Release/VoopooFirmwareStudio.exe`.

## Проверки и релизы

```powershell
$p = Start-Process .\build\Release\VoopooFirmwareStudio.exe -ArgumentList '--smoke-test' -Wait -PassThru
if ($p.ExitCode -ne 0) { throw "Smoke tests failed: $($p.ExitCode)" }
```

Проверяются перепаковка, редактор, цвета, история и сохранение. Лог: `studio_debug.log` в рабочей папке.

CI: push/PR в `master`, теги `v*`, ручной запуск. Релиз — по аннотированному тегу из `master` после проверок; текст аннотации становится чейнджлогом.

[Карта цветов прошивки](docs/firmware-colors.md) · [Лицензия MIT](LICENSE)
