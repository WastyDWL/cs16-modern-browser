# cs16-modern-browser (nextchromium)

**Современный Chromium для MOTD и HTML-окон Counter-Strike 1.6 на NextClient — без замены файлов игры.**

Встроенный браузер CS 1.6 — это CEF1 на движке Chromium 18 из 2012 года. Он не понимает flex, grid,
CSS-переменные, современный JavaScript, поэтому сайты, статистика и MOTD в игре выглядят сломанными.
Мод `nextchromium` подменяет его на **Chromium 150** (CEF 150, 32-bit):

| До (встроенный CEF1) | После (nextchromium) |
|---|---|
| вёрстка «едет», блоки не на местах, JS-ошибки | страница выглядит как в обычном браузере |
| Chromium 18 (2012) | Chromium 150 |

- MOTD, окна статистики (`/top15`, `/rank`…) и все HTML-окна игры рендерятся современным движком.
- Широкие страницы автоматически уменьшаются, чтобы влезать в окно MOTD без горизонтальной прокрутки.
- Работает JS-API NextClient (`window.nextclient`): cvar-ы, подключение, список игроков, события.
- **Ни один файл игры не заменяется и не изменяется.** Мод только добавляет файлы; удалите их, и игра
  вернётся к старому браузеру.

## Установка

1. Скачайте `nextchromium-<версия>.zip` со страницы [Releases](../../releases).
2. Распакуйте содержимое в папку игры, рядом с `cstrike.exe`:

   ```
   nextchromium.asi   ← загрузчик (игра сама подхватывает *.asi через Miles Sound System)
   nextchromium\      ← мод и Chromium (~320 МБ)
   ```

3. Запустите игру как обычно.

Требования: **Windows 10 или новее** (на более старых системах мод сам отключается), NextClient (см. ниже).

## Удаление

Удалите `nextchromium.asi` и папку `nextchromium\`.

Временно выключить мод: добавьте параметр запуска `-nonextchromium` или поставьте `enabled=0` в
`nextchromium\nextchromium.ini`.

## Совместимость с NextClient

| Проверено | |
|---|---|
| NextClient | **2.5.3** (ветка `main`) |
| `chromehtml.dll` / CEF1 | 1.989.464 (Chromium 18.0.989), из комплекта NextClient |
| ОС | Windows 10 x64 |

**Обновления NextClient.** Мод не зависит от адресов в коде. Он находит нужные объекты через RTTI и
интерфейсы, которые NextClient и GoldSrc не меняют годами (`ChromeHTML_Controller_001`, `HTMLSerializer`,
`VGUI_Panel007`, `GameUI007`), а при запуске проверяет их раскладку. Если после обновления что-то не сходится,
мод **не включается**, игра работает со старым браузером, а причина пишется в `nextchromium\nextchromium.log`.
Обновление NextClient не удаляет файлы мода.

**Функции NextClient внутри страниц.** Страницы, написанные под NextClient, работают без изменений:

| `window.nextclient` | Статус |
|---|---|
| `getCvarValue(name)` | ✔ (кроме паролей: `*password*`, `*_pw*` → `null`) |
| `connectedHost` → `{address, state, players[]}` | ✔ |
| события `connect`, `disconnect`, `putinserver` | ✔ |
| `gameUi.shown`, события `show`, `hide` | ✔ |
| `exec(cmd)` | ✔, только для доменов из `platform\accepted_domains.txt` |
| `addEventListener`, `removeEventListener`, `dispatchEvent`, `makeAsEventTarget` | ✔ |
| `gameUi.optionsDialog.open(tab)` | ✘ пока нет |
| `console`, событие `message` | ✘ объект есть, событий нет |
| `matchmaking.*` | ✘ Promise отклоняется |

Как и в NextClient, без файла `platform\accepted_domains.txt` (в обычной установке его нет) `exec`
запрещён всем страницам. Проверка домена выполняется в процессе браузера по URL фрейма, а не на стороне страницы.

**Главное меню NextClient** (`platform\mmb_home_page.html`), если оно включено, тоже рендерится новым движком.

## Настройки — `nextchromium\nextchromium.ini`

| Параметр | По умолчанию | Что делает |
|---|---|---|
| `enabled` | `1` | `0`: игра использует свой старый браузер |
| `zoom` | `90` | масштаб страниц в процентах (25–300) |
| `fit_width` | `1` | уменьшать страницы шире окна, чтобы не было горизонтальной прокрутки |
| `frame_rate` | `60` | частота отрисовки страниц (1–240) |
| `debug_port` | `0` | порт Chrome DevTools для отладки страниц (`0`: выключено; не оставляйте включённым) |

## Для владельцев серверов

Делайте MOTD и страницы статистики как для обычного современного браузера. Окно MOTD в игре — около 720×360
пикселей, страницы шире него мод уменьшит сам. Отладка: `debug_port=9222` в `nextchromium.ini`, затем
откройте `http://127.0.0.1:9222` в Chrome, пока MOTD открыт.

Игроки без мода по-прежнему видят страницу старым движком, так что важные вещи лучше проверять и там.

## Ограничения

- Видео в H.264/AAC не воспроизводится: стандартный CEF собран без проприетарных кодеков (VP8/VP9/AV1/Opus работают).
- Flash и NPAPI-плагины не поддерживаются.
- Пункт «Копировать ссылку» в контекстном меню панели не работает.
- Только Windows.

## Если что-то не так

- Посмотрите `nextchromium\nextchromium.log`: там видно, активировался ли мод и почему нет.
- Ошибки Chromium пишутся в `nextchromium\cef.log`.
- Проверьте, что `nextchromium.asi` лежит рядом с `cstrike.exe`, а не внутри `nextchromium\`.

## Как это устроено

1. `Mss32.dll` (Miles) загружает все `*.asi` из папки игры, в том числе `nextchromium.asi`. К этому моменту
   игра уже создала свой браузер, поэтому загрузчик подменяет один слот vtable у контроллера `chromehtml.dll`.
2. На первом кадре, в главном потоке, загружается `nextchromium\nextchromium.dll`. Она проверяет раскладку
   `chromehtml.dll`, запускает CEF 150 (свой `libcef.dll` по полному пути, рядом со старым), перенаправляет
   на себя контроллер и класс `HTMLSerializer`, включая окна, созданные до загрузки мода.
3. Страницы рисуются без окна (offscreen) и отдаются игре как текстура BGRA, тем же способом, что у
   старого браузера. Размер берётся прямо у vgui-панели.
4. `window.nextclient` внедряется в каждую страницу процессом `ncb_subprocess.exe` и обращается к игре
   через `ncb://api/…` (только XHR/fetch). Данные игры читаются из таблицы функций движка в главном потоке.

Дизайн и решения: [`docs/superpowers/specs`](docs/superpowers/specs), [`docs/spike-results.md`](docs/spike-results.md).

## Сборка из исходников

Нужны Visual Studio Build Tools (MSVC, x86), CMake ≥ 3.21 и:

- CEF `150.0.23+g4ccc4ca+chromium-150.0.7871.287` **windows32 minimal**
  ([cef-builds.spotifycdn.com](https://cef-builds.spotifycdn.com/index.html)), распакованный в `third_party\cef`;
- [MinHook](https://github.com/TsudaKageyu/minhook) в `third_party\minhook`.

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

`tools\deploy.ps1 [-Game <папка игры>] [-Remove]` ставит или убирает мод. `out\ncb_harness.exe` рендерит страницу
без игры: `ncb_harness <папка nextchromium> <url> <out.bmp> [w h]`.

## Лицензия

Код мода распространяется под [MIT](LICENSE). Релизы включают CEF (BSD), Chromium (лицензии в
`nextchromium\licenses\CHROMIUM-CREDITS.html`) и MinHook (BSD-2-Clause). Не аффилировано с Valve
или командой NextClient.
