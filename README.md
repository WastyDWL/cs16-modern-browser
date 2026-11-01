# nextchromium — современный браузер для CS 1.6 (NextClient)

Мод заменяет встроенный браузер игры (CEF1, Chromium 18 из 2012 года) на **Chromium 150** (CEF 150, 32-bit).
MOTD, окна статистики и любые HTML-окна игры начинают понимать современную вёрстку и JS:
flex, grid, CSS-переменные, `:has()`, ES2023, `fetch` и всё остальное.

**Ни один файл игры не заменяется.** Мод только добавляет файлы; удалите их, и игра работает как раньше.

## Установка

Скопируйте в папку игры (рядом с `cstrike.exe`):

```
nextchromium.asi        ← загрузчик (игра подхватывает *.asi сама, через Miles Sound System)
nextchromium\           ← сам мод и Chromium (~200 МБ)
```

Требуется Windows 10 или новее. На более старой Windows мод сам отключается.

## Удаление

Удалите `nextchromium.asi` и папку `nextchromium\`.

Временно выключить мод: параметр запуска `-nonextchromium` или `enabled=0` в `nextchromium\nextchromium.ini`.

## Настройки — `nextchromium\nextchromium.ini`

| Параметр | По умолчанию | Что делает |
|---|---|---|
| `enabled` | `1` | `0`: игра использует свой старый браузер |
| `zoom` | `90` | масштаб страниц в процентах (25–300) |
| `fit_width` | `1` | уменьшать страницы шире окна, чтобы не было горизонтальной прокрутки |
| `frame_rate` | `60` | частота отрисовки страниц (1–240) |
| `debug_port` | `0` | порт Chrome DevTools для отладки страниц (`0`: выключено; не оставляйте включённым) |

## JS-API NextClient (`window.nextclient`)

Есть в каждой странице, как в оригинальном NextClient:

| API | Статус |
|---|---|
| `nextclient.getCvarValue(name)` | ✔ |
| `nextclient.connectedHost` → `{address, state, players[]}` | ✔ |
| события `connect`, `disconnect`, `putinserver` | ✔ |
| `nextclient.gameUi.shown`, события `show`, `hide` | ✔ |
| `nextclient.exec(cmd)` | ✔, только для доменов из `platform\accepted_domains.txt` |
| `addEventListener`, `removeEventListener`, `dispatchEvent`, `makeAsEventTarget` | ✔ |
| `nextclient.gameUi.optionsDialog.open(tab)` | ✘ пока не реализовано |
| `nextclient.console` (событие `message`) | ✘ объект есть, событий нет |
| `nextclient.matchmaking.*` | ✘ Promise отклоняется |

Без файла `platform\accepted_domains.txt` (в обычной установке его нет) `exec` запрещён всем страницам, как и в NextClient.

## Как это устроено

- `Mss32.dll` (Miles) загружает все `*.asi` из папки игры, в том числе `nextchromium.asi`.
- Загрузчик подменяет один слот vtable у контроллера `chromehtml.dll` и на первом кадре загружает `nextchromium\nextchromium.dll`.
- `nextchromium.dll` проверяет, что сборка `chromehtml.dll` знакомая, запускает CEF 150 и перенаправляет на себя
  контроллер и класс `HTMLSerializer`, включая окна, созданные до загрузки мода.
- Страницы рисуются без окна (offscreen) и отдаются игре как текстура BGRA. Ввод, курсоры, `alert`/`confirm`,
  выпадающие списки и переходы по ссылкам работают через те же интерфейсы, что и у старого браузера.
- `window.nextclient` внедряется в каждую страницу процессом `ncb_subprocess.exe` и обращается к игре через
  `ncb://api/…`. Доверенный домен проверяется по URL фрейма в процессе браузера.

Если что-то пошло не так (незнакомая версия игры, ошибка запуска CEF), мод ничего не меняет, и игра работает
со старым браузером. Подробности пишутся в `nextchromium\nextchromium.log` и `nextchromium\cef.log`.

## Сборка

Нужны Visual Studio Build Tools (MSVC x86), CMake ≥ 3.21, CEF 150 windows32 minimal в `third_party\cef`
и MinHook в `third_party\minhook`.

```bash
tools\build.cmd
```

```bash
build\tests\nc_tests.exe
```

```bash
powershell -ExecutionPolicy Bypass -File tools\stage.ps1
```

```bash
powershell -ExecutionPolicy Bypass -File tools\package.ps1
```

`tools\deploy.ps1 [-Game <папка игры>] [-Remove]` ставит или убирает мод. `out\ncb_harness.exe` рендерит
страницу без игры (`ncb_harness <папка nextchromium> <url> <out.bmp> [w h]`).
