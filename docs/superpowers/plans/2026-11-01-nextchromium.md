# nextchromium Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Мод `nextchromium` для CS 1.6 NextClient: подменяет `ChromeHTML_Controller_001` на реализацию поверх CEF 150 (x86, OSR), не трогая оригинальные файлы.

**Architecture:** `nextchromium.asi` (грузится Miles) ставит хуки и при первом запросе `ChromeHTML_Controller_001` загружает `nextchromium\nextchromium.dll`, которая возвращает `NcController`. Каждое HTML-окно игры получает `NcSerializer`, внутри — windowless CefBrowser. События CEF складываются в очередь и отдаются игре в `RunFrame` (главный поток). JS-API `nextclient.*` живёт в `ncb_subprocess.exe` (render process) и общается с игрой через `CefProcessMessage`.

**Tech Stack:** C++20, MSVC 14.51 x86, CMake ≥3.21, CEF 150.0.23 windows32 minimal, MinHook. Тесты — собственный header-only раннер (`tests/testing.h`), без внешних фреймворков.

**Spec:** `docs/superpowers/specs/2026-11-01-modern-chromium-mod-design.md`

## Global Constraints

- Ни один файл в `C:\games\CS 1.6 - NextClient` не изменяется и не заменяется; мод только добавляет `nextchromium.asi` и папку `nextchromium\`.
- Все колбэки `IHTMLResponses_HL1` вызываются только из главного потока игры (внутри `RunFrame` или вызова сериализатора).
- Любая ошибка инициализации → вызовы уходят в оригинальный контроллер, игра не падает.
- Выключатели: `nextchromium\nextchromium.ini` `[main] enabled=0`, параметр `-nonextchromium`, Windows < 10.
- Привилегированный JS (`exec`, `gameUi.optionsDialog.open`) — только для доменов из `platform/accepted_domains.txt`, проверка в процессе браузера; regex `^(?:https?:\/\/)?(?:[^@\/\n]+@)?(?:www\.)?([^:\/\n]+)`.
- Пути: CEF-ресурсы `nextchromium\cef\`, кэш `nextchromium\cache\`, лог `nextchromium\nextchromium.log`.
- Пиксели отдаются игре как BGRA (игра зовёт `DrawSetTextureBGRA`).

## Review Focus

- Окно закрывают, пока CEF ещё создаёт браузер (`RemoveBrowser` до `OnAfterCreated`) → без падений и утечек; тест в Task 4 (`serializer_remove_before_created`).
- Размер окна 0×0 или отрицательный (`BrowserSize(0,0)`) → CEF получает минимум 1×1; тест в Task 3 (`clamp_view_size`).
- Ввод русского текста и Ctrl/Shift-комбинаций (`KeyChar` с кириллицей, `KeyDown` с mods) → правильные `CefKeyEvent`; тест в Task 3 (`keyevent_cyrillic_char`, `keyevent_modifiers`).
- `accepted_domains.txt` отсутствует/пустой/с пробелами и `\r\n` → всё запрещено / корректный разбор; тест в Task 3 (`domains_*`).
- Игра не отвечает `StartRequestResponse` (окно уже скрыто) → навигация разрешается по таймауту 2 с, CEF не зависает; тест в Task 4 (`start_request_timeout`).

---

### Task 1: Спайк — порядок загрузки и реальные вызовы

**Files:**
- Create: `CMakeLists.txt`, `src/common/valve_html.h`, `src/common/log.h`, `src/common/log.cpp`, `src/spike/spike_asi.cpp`, `src/spike/logging_proxy.h`, `src/spike/logging_proxy.cpp`

**Interfaces:**
- Produces: `valve_html.h` — `IHTMLChromeController`, `IHTMLSerializer`, `IHTMLResponses_HL1`, `IHTMLResponses` (opaque), порядок vtable из `ncl-hl1-source-sdk/public/html` (без `DBGFLAG_VALIDATE`). `nc::log(const char* fmt, ...)` пишет в файл, путь задаётся `nc::log_open(const wchar_t*)`.

- [ ] Собрать `nextchromium_spike.asi`: в `DllMain` логирует, загружена ли уже `chromehtml.dll`; MinHook на `LoadLibraryA/W/ExA/ExW` и, после загрузки `chromehtml.dll`, на её `CreateInterface`. Хук возвращает логирующий прокси, пересылающий всё в оригинал (контроллер, сериализатор, `IHTMLResponses_HL1`).
- [ ] Положить в папку игры, запустить, зайти в главное меню и на локальный сервер с MOTD, выйти.
- [ ] По логу зафиксировать в `docs/spike-results.md`: успевает ли `.asi`; какие методы зовутся; формат `BrowserNeedsPaint` (rgba — весь кадр или фрагмент); из какого потока; вызывает ли кто-то `Init` и `CefRegisterExtension` до/после.
- [ ] Удалить spike-файлы из папки игры. Commit.

### Task 2: Каркас сборки и CEF

**Files:**
- Create: `cmake/cef.cmake`, `src/core/CMakeLists.txt`, `src/subprocess/CMakeLists.txt`, `tests/testing.h`, `tests/CMakeLists.txt`, `tools/deploy.ps1`

- [ ] `cmake/cef.cmake` подключает `third_party/cef` (`FindCEF.cmake`, `libcef_dll_wrapper`), static CRT `/MT` как у wrapper.
- [ ] Цели: `nextchromium_asi` (SHARED, суффикс `.asi`), `nextchromium` (SHARED), `ncb_subprocess` (WIN32 exe, manifest с supportedOS Win10), `nc_tests` (exe), `ncb_harness` (exe).
- [ ] `tools/deploy.ps1 -Game <path>` копирует `nextchromium.asi` в корень игры и `nextchromium\` (dll, exe, CEF runtime) — и `-Remove` для удаления.
- [ ] `cmake -B build -A Win32 && cmake --build build --config Release` — без ошибок; `nc_tests` запускается и сообщает `0 tests`. Commit.

### Task 3: Чистые модули с тестами

**Files:**
- Create: `src/core/keymap.h/.cpp`, `src/core/view_math.h/.cpp`, `src/core/domains.h/.cpp`, `src/core/config.h/.cpp`, `tests/test_keymap.cpp`, `tests/test_view_math.cpp`, `tests/test_domains.cpp`, `tests/test_config.cpp`

**Interfaces:**
- Produces:
  - `CefKeyEvent nc::make_key_event(KeyKind kind, int vk_or_char, int vgui_mods)`; `enum class KeyKind {Down, Up, Char}`; vgui mods: Alt=1, Ctrl=2, Shift=4 → `EVENTFLAG_ALT_DOWN/CONTROL_DOWN/SHIFT_DOWN`.
  - `uint32_t nc::mouse_flags(int vgui_mods, bool left, bool middle, bool right)`; `cef_mouse_button_type_t nc::mouse_button(int code)` (0 left, 1 middle, 2 right).
  - `CefSize nc::clamp_view_size(int w, int h)` → минимум 1×1, максимум 8192.
  - `class nc::Domains { bool load(const std::wstring& path); void load_from_string(std::string_view); bool is_accepted(std::string_view url) const; }`
  - `struct nc::Config { bool enabled=true; int frame_rate=60; int debug_port=0; }; Config nc::load_config(const std::wstring& ini, std::wstring_view cmdline);`

- [ ] Тесты: `keyevent_char_ascii` (`'a'` → `KEYEVENT_CHAR`, `character==L'a'`), `keyevent_cyrillic_char` (`0x0436` → `character==0x0436`), `keyevent_modifiers` (Down, VK_A, mods=2|4 → `modifiers` содержит CONTROL и SHIFT, `windows_key_code==0x41`), `clamp_view_size` (`(0,-5)`→`1×1`, `(10000,20)`→`8192×20`), `domains_missing_file_denies_all`, `domains_crlf_and_spaces` (`"a.com\r\n  b.ru \n"` → `https://www.a.com/x` true, `http://b.ru:8080` true, `https://evil.com/?a.com` false), `domains_userinfo` (`https://u@a.com/` true), `config_cmdline_disables` (`-nonextchromium` → enabled=false), `config_ini_values` (`[main]\nenabled=0\nframe_rate=30`).
- [ ] Реализовать, `nc_tests` — все PASS. Commit.

### Task 4: Контроллер, сериализатор, очередь

**Files:**
- Create: `src/core/main_thread_queue.h/.cpp`, `src/core/controller.h/.cpp`, `src/core/serializer.h/.cpp`, `src/core/browser_client.h/.cpp`, `src/core/cef_app.h/.cpp`, `src/core/exports.cpp`, `tests/test_queue.cpp`, `tests/test_serializer_logic.cpp`

**Interfaces:**
- Consumes: Task 3.
- Produces:
  - `class nc::MainThreadQueue { void post(std::function<void()>); void drain(); }` — потокобезопасная.
  - `class nc::NcController final : public IHTMLChromeController` с конструктором `NcController(IHTMLChromeController* original)`.
  - `class nc::NcSerializer final : public IHTMLSerializer` — хранит `IHTMLResponses_HL1* target`, `CefRefPtr<BrowserClient>`, `int index` (−1 до `OnAfterCreated`); `GetResponseTarget()` возвращает собственный объект-маркер `NcResponsesTag`, по которому `CreateBrowser/RemoveBrowser` находят сериализатор.
  - экспорт `nextchromium.dll`: `extern "C" IHTMLChromeController* nc_create_controller(IHTMLChromeController* original, const wchar_t* game_dir)`; `nullptr` при отключении/ошибке.
- [ ] Тесты: `queue_runs_in_order`, `queue_post_from_other_thread`, `serializer_remove_before_created` (удаление до создания браузера не зовёт колбэков после `RemoveBrowser`), `start_request_timeout` (без ответа 2 с → разрешено), `paint_waits_for_response` (второй кадр не уходит до `NeedsPaintResponse`, последний кадр сохраняется).
- [ ] Реализация: `Init` → `CefInitialize` (OSR, `multi_threaded_message_loop=true`, `windowless_rendering_enabled=true`, пути из Global Constraints, `remote_debugging_port` из конфига). `RunFrame` → `queue.drain()`, возвращает `true`. `Shutdown` → закрыть браузеры, `CefShutdown`. Колбэки CEF (`OnPaint`, `OnLoadStart/End`, `OnAddressChange`, `OnTitleChange`, `OnCursorChange`, `OnTooltip`, `OnJSDialog`, `OnBeforePopup`, `OnPopupShow/Size`, `DoClose`, `OnBeforeBrowse`) → `queue.post(...)` → `IHTMLResponses_HL1`. Ввод → `SendMouse*/SendKeyEvent`. `BrowserErrorStrings` → HTML ошибки через `OnLoadError` + `LoadURL(data:)`.
- [ ] `nc_tests` PASS, сборка без ошибок. Commit.

### Task 5: Стенд `ncb_harness` и проверка рендера

**Files:**
- Create: `tools/harness/main.cpp`, `tests/pages/modern.html`

- [ ] `ncb_harness.exe <url> <out.bmp> [w h]`: грузит `nextchromium.dll`, `Init`, `CreateSerializer(fake)` + `CreateBrowser`, крутит `RunFrame` до `BrowserFinishedRequest` + 500 мс, сохраняет последний кадр в BMP, `Shutdown`. Код возврата 0.
- [ ] `tests/pages/modern.html` — flex, grid, CSS-переменные, `:has()`, `fetch` к `data:`, optional chaining; на странице `#result` с `PASS`/`FAIL` по JS-проверкам фич.
- [ ] Запуск стенда на `modern.html` → картинка содержит ожидаемую раскладку, `document.title` = `PASS` (стенд печатает заголовок). Commit.

### Task 6: Загрузчик `nextchromium.asi`

**Files:**
- Create: `src/loader/asi_main.cpp`, `src/loader/hooks.h/.cpp`

- [ ] По результатам Task 1: хук `LoadLibrary*` → хук `CreateInterface` у `chromehtml.dll`; на `ChromeHTML_Controller_001` — `LoadLibraryW(nextchromium\nextchromium.dll)` вне loader lock, `nc_create_controller(original)`; `nullptr` → вернуть оригинал. Запасной путь из `docs/spike-results.md`, если `.asi` опаздывает.
- [ ] Проверка совместимости сборки: версия `chromehtml.dll` == `1.0.0.1`/хэш из спайка, иначе — оригинал и запись в лог.
- [ ] Ручная проверка в игре (deploy → запуск → главное меню и MOTD рендерит новый движок, в логе `controller: nextchromium active`; `-nonextchromium` → оригинал; `deploy -Remove` → оригинал). Commit.

### Task 7: JS-API `nextclient.*`

**Files:**
- Create: `src/subprocess/main.cpp`, `src/subprocess/render_app.h/.cpp`, `src/subprocess/nextclient_api.js`, `src/core/game_bridge.h/.cpp`, `src/core/js_bridge.h/.cpp`, `src/common/ipc_names.h`, `tests/pages/api.html`, `tests/test_state_mirror.cpp`

**Interfaces:**
- Produces: имена IPC в `ipc_names.h`: `ncl.exec`, `ncl.optionsDialog`, `ncl.state` (browser→render, JSON полное зеркало), `ncl.event` (browser→render: `{target, type, detail}`), `ncl.cvar.req`/`ncl.cvar.resp`, `ncl.mm.req`/`ncl.mm.resp`.
  - `struct nc::StateMirror { std::optional<ConnectedHost> host; bool gameui_shown; std::map<std::string,std::string> cvars; }` + `std::string to_json() const; static StateMirror from_json(std::string_view);`
  - `class nc::GameBridge` (главный поток): `std::optional<std::string> cvar(std::string_view)`, `void client_cmd(std::string_view)`, `std::optional<ConnectedHost> host()`, `bool gameui_active()`, `void open_options(std::string_view tab)`, `void poll()` — сравнивает состояние кадр к кадру и шлёт события `connect/disconnect/putinserver/show/hide`.
- [ ] Тест `state_mirror_roundtrip` (JSON туда-обратно, игроки с кириллицей в имени, `host=null`).
- [ ] `nextclient_api.js` — то же API, что в исходниках NextClient (`makeAsEventTarget`, `exec`, `getCvarValue`, `connectedHost`, `gameUi.shown/optionsDialog.open`, `console`, `matchmaking.*`), нативные функции через `window.__ncb_native` из `render_app`.
- [ ] `getCvarValue` для cvar, которой нет в зеркале — синхронный запрос в процесс браузера через собственную схему `ncb://cvar/<name>` и синхронный XHR (единственный синхронный путь в многопроцессном CEF), ответ кэшируется в зеркале.
- [ ] Доступ к движку — через `nitro_api2.dll`, если его интерфейс даёт `cl_enginefunc_t`/состояние клиента; иначе `cl_enginefunc_t` из хука экспорта `Initialize` `client.dll`. GameUI — `CreateInterface("GameUI007")` из `GameUI.dll`.
- [ ] Проверка: `tests/pages/api.html` через стенд (без игры — функции возвращают null/ничего, без исключений) и в игре (вывод `getCvarValue('name')`, `connectedHost` на локальном сервере, события). Commit.

### Task 8: Финальная проверка в игре и упаковка

**Files:**
- Create: `README.md` (установка/удаление/настройки), `tools/package.ps1`

- [ ] Чек-лист в игре: главное меню, MOTD с `modern.html`, ввод текста (рус/англ), `<select>`, `alert`, прокрутка, смена разрешения, alt-tab, выход без зависания процесса, `-nonextchromium`, удаление мода.
- [ ] Замер: FPS с закрытыми окнами до/после мода (разница в пределах погрешности).
- [ ] `tools/package.ps1` → `out/nextchromium-<version>.zip` (asi + папка). Commit.
