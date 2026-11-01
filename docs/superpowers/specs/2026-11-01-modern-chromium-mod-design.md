# Мод «современный Chromium» для CS 1.6 (NextClient) — дизайн

Дата: 2026-11-01
Статус: черновик, ждёт утверждения

## 1. Зачем

Встроенный браузер игры — CEF1 `1.989.464` (Chromium 18.0.989, 2012 год). Он не понимает
современную вёрстку и JS: flex/grid, CSS-переменные, ES6+, `fetch`, `Promise` и т.д.
Из-за этого страницы MOTD, главное меню NextClient и любые HTML-окна игры приходится
верстать «под 2012 год».

Цель — **мод-дополнение**, который подменяет движок всех HTML-окон игры на актуальный
Chromium (CEF 150, Chromium 150.0.7871), **не изменяя и не заменяя ни одного оригинального
файла**. Удалил файлы мода — игра работает как раньше.

## 2. Цели, не-цели, анти-цели

**Цели**
- Все HTML-окна, которые игра создаёт через `ChromeHTML_Controller_001`, рендерит новый
  Chromium: MOTD, главное меню NextClient (`MainMenuBrowser`), всплывающие HTML-окна.
- Полная совместимость с JS-API NextClient (`window.nextclient.*`) — страницы, написанные
  под NextClient, работают без правок (подробно в §6).
- Мышь, клавиатура (включая ввод русского текста), колесо, курсоры, выпадающие списки
  (`<select>`), подсказки, `alert/confirm`, прокрутка, страницы ошибок — работают как сейчас.
- Мод ставится копированием файлов в папку игры и удаляется их удалением.

**Не-цели**
- Linux / macOS (NextClient на них браузер тоже не поддерживает).
- Flash и NPAPI-плагины.
- Видео в проприетарных кодеках (H.264/AAC): стандартный CEF собран без них. VP8/VP9/AV1/Opus
  работают.
- Изменение кода NextClient или пересборка его DLL.

**Анти-цели (провал, даже если «технически работает»)**
- Любая замена/правка оригинальных файлов игры или NextClient.
- Игра падает или не запускается, если с модом что-то не так. При любой ошибке
  инициализации мод обязан тихо отдать управление оригинальному браузеру.
- Заметное падение FPS в бою, когда HTML-окна закрыты.
- `nextclient.exec` и другие привилегированные функции становятся доступны чужим сайтам
  (нарушение списка доверенных доменов).

## 3. Ограничения

- Игра 32-битная (`hl.exe`, `hw.dll`) → нужен CEF **windows32**. Последняя стабильная сборка:
  `150.0.23+g4ccc4ca+chromium-150.0.7871.287`, minimal-архив ≈134 МБ. Размер мода на диске
  ≈200–250 МБ.
- Современный Chromium требует **Windows 10+**. На Windows 7/8 мод должен сам отключиться.
- Сборка: Visual Studio Build Tools 2026 (MSVC 14.51, x86), CMake, Git — есть на машине.
- Заголовки интерфейсов: `CS-NextClient/ncl-hl1-source-sdk`, `public/html/*.h`
  (`ichromehtmlwrapper.h`, `ihtmlserializer.h`, `ihtmlchrome.h`, `htmlmessages.h`).

## 4. Что выяснила разведка

- `hw.dll` загружает `chromehtml.dll` и берёт у неё через `CreateInterface` интерфейс
  `ChromeHTML_Controller_001` (`IHTMLChromeController`). Импорты `chromehtml.dll`:
  `KERNEL32`, `USER32`, `tier0`, `libcef`.
- Путь HL1 простой: `IHTMLChromeController::CreateSerializer(IHTMLResponses_HL1*)` возвращает
  `IHTMLSerializer`. Это обычные C++-виртуальные интерфейсы: игра шлёт ввод/URL/размер, браузер
  отвечает колбэками, например `BrowserNeedsPaint(textureid, w, h, rgba, …, combobox_*)`.
  Protobuf-путь (`CreateBrowser(IHTMLResponses*)`) в HL1 не используется — проверить в спайке.
- `Mss32.dll` (Miles 9.3a) при старте делает `FindFirstFile("*.asi")` + `LoadLibrary` в папке
  игры; так уже грузятся `binkawin.asi` и `mssmp3.asi`. Это точка входа мода.
- NextClient сам регистрирует JS-расширения в старом CEF1 (`CefRegisterExtension`) из
  `next_engine_mini.dll` и `GameUI.dll`. В новом CEF (многопроцессном) эти регистрации не
  действуют — API нужно реализовать в моде заново.
- Список доверенных доменов для привилегированного API: `platform/accepted_domains.txt`.
- Стартовая страница главного меню: `platform/mmb_home_page.html` (там URL),
  страница ошибки: `platform/mmb_error_page.html` — работают через `BrowserErrorStrings`/`PostURL`.

## 5. Как устроено

### 5.1 Файлы мода в папке игры

```
cstrike.exe, hw.dll, chromehtml.dll, libcef.dll ...   ← оригиналы, не трогаем
nextchromium.asi                                       ← загрузчик (Miles его подхватит)
nextchromium\
    nextchromium.dll       ← сам мод: контроллер, сериализатор, JS-мост
    ncb_subprocess.exe     ← вспомогательный процесс Chromium (рендер/GPU/сеть)
    cef\                   ← libcef.dll 150, *.pak, icudtl.dat, locales\, v8 snapshot …
    cache\                 ← кэш/куки нового браузера (отдельно от htmlcache)
    nextchromium.log
    nextchromium.ini       ← настройки (вкл/выкл, FPS отрисовки, отладочный порт)
```

Новый `libcef.dll` лежит в подпапке и грузится по полному пути, поэтому он не конфликтует со
старым `libcef.dll` в корне (тот продолжает использоваться `chromehtml.dll` и NextClient).

### 5.2 Загрузка и перехват

1. Miles загружает `nextchromium.asi`. В `DllMain` — только минимум (без загрузки DLL под
   loader lock): ставится хук.
2. Перехват `ChromeHTML_Controller_001`:
   - если `chromehtml.dll` ещё не загружена — хук на `LoadLibraryA/W` (или `LdrLoadDll`);
     при её загрузке ставится хук на её экспорт `CreateInterface`;
   - если уже загружена — хук сразу на её `CreateInterface`;
   - хук `CreateInterface("ChromeHTML_Controller_001")` возвращает **наш** контроллер, который
     держит указатель на оригинальный (для отката).
3. Если `hw.dll` получила интерфейс **раньше**, чем загрузился `.asi` (порядок загрузки не
   проверен — первая задача спайка), нужен запасной путь. Варианты на выбор исполнителю после
   замера: найти и подменить сохранённый в `hw.dll` указатель на контроллер, либо патч vtable
   оригинального `CHTMLController` так, чтобы его методы вели в наш контроллер.
4. Отключение: `nextchromium.ini` → `enabled=0`, параметр запуска `-nonextchromium`, Windows
   ниже 10, или ошибка `CefInitialize` → все вызовы идут в оригинальный контроллер.

### 5.3 Контроллер и сериализатор

- `NcController : IHTMLChromeController` — `Init` поднимает CEF 150: windowless (OSR),
  `browser_subprocess_path = nextchromium\ncb_subprocess.exe`, `resources_dir_path`/
  `locales_dir_path` в `nextchromium\cef`, `root_cache_path = nextchromium\cache`,
  `multi_threaded_message_loop = true`. `RunFrame` (вызывается игрой каждый кадр в главном
  потоке) разбирает очередь событий от CEF и вызывает колбэки `IHTMLResponses_HL1` — **все
  колбэки в игру идут только из главного потока**.
- `NcSerializer : IHTMLSerializer` — один на браузер. Переводит вызовы игры в CEF:
  `PostURL`, `BrowserSize`, `MouseMove/Down/Up/Wheel/DoubleClick`, `KeyDown/Up/Char`,
  `RunJavaScript`, `SetFocus`, скролл, `Reload/GoBack/GoForward/StopLoad`, `AddHeader`,
  `BrowserErrorStrings`, `JSDialogResponse`, `GetLinkAtPosition`, `NeedsPaintResponse`.
- Отрисовка: `CefRenderHandler::OnPaint` (BGRA) → копия в RGBA-буфер → `BrowserNeedsPaint`
  с dirty-прямоугольником. `PET_POPUP` (выпадающий `<select>`) → `BrowserShowPopup/SizePopup`
  + `combobox_rgba`. Новый кадр не отправляется, пока игра не ответила `NeedsPaintResponse`
  (как в оригинале). FPS отрисовки по умолчанию 60, ограничивается `SetCefThreadTargetFrameRate`.
- Остальные колбэки: URL/заголовок/загрузка (`StartRequest`, `URLChanged`, `FinishedRequest`,
  `SetHTMLTitle`, `CanGoBackandForward`), курсор, подсказки, `alert/confirm`, всплывающие окна
  (`BrowserPopupHTMLWindow`), `window.close` → `BrowserClose`.
- `StartRequest` — синхронный вопрос игре «можно ли перейти» (`StartRequestResponse`); в CEF
  это `OnBeforeBrowse`, ответ ждётся с таймаутом, чтобы не блокировать навигацию навсегда.
- Сопоставление кодов клавиш VGUI → Windows VK / `CefKeyEvent` — отдельный модуль с тестами.
- User-Agent: современный Chromium + суффикс, который передаёт игра (`pchUserAgentIdentifier`).
- Куки: `SetWebCookie/GetWebCookiesForURL` → `CefCookieManager`.

### 5.4 JS-API NextClient

Реализуется в моде, в рендер-процессе (`CefRenderProcessHandler::OnContextCreated`), с той же
семантикой, что в исходниках NextClient (`gameui/src/Browser/*`, `engine_mini/src/binding/jsapi/*`):

| API | Источник данных в игре |
|---|---|
| `nextclient.exec(cmd)` (только доверенные домены) | `pfnClientCmd` |
| `addEventListener/removeEventListener/dispatchEvent` на `nextclient`, `gameUi`, `console` | в JS |
| `nextclient.getCvarValue(name)` | `pfnGetCvarPointer` |
| `nextclient.connectedHost` → `{address, state, players[]}` | состояние подключения + `pfnGetPlayerInfo` |
| события `connect` / `disconnect` / `putinserver` | отслеживание состояния подключения |
| `nextclient.gameUi.shown`, события `show` / `hide` | `IGameUI::IsGameUIActive` |
| `nextclient.gameUi.optionsDialog.open(tab)` (только доверенные домены) | открытие диалога настроек GameUI |
| `nextclient.console`, событие `message` | перехват вывода консоли |
| `nextclient.matchmaking.getServerInfo/getPlayersInfo/getRules(ip)` | A2S-запросы / Steam matchmaking servers |
| `nextclient.matchmaking.get/add/removeFavoriteServer`, `getHistoryServers` | Steam matchmaking / хранилище NextClient |

Важное отличие от CEF1: в новом Chromium JS живёт в отдельном процессе, синхронных вызовов в
игру нет. Поэтому:
- **синхронные геттеры** (`getCvarValue`, `connectedHost`, `gameUi.shown`) отвечают из
  **зеркала состояния** в рендер-процессе. Игра присылает его при создании контекста и
  обновления при изменениях; cvar-ы — по запросу с кэшем, первое чтение неизвестной cvar
  делает синхронный IPC-запрос с таймаутом (механизм выбирает исполнитель: например,
  sync-запрос через `CefURLRequest` к собственной схеме или общая память);
- асинхронные вызовы (`exec`, `optionsDialog.open`, matchmaking) — `CefProcessMessage` →
  очередь → главный поток игры;
- matchmaking в оригинале асинхронный (Promise) — так и остаётся.

Проверка доверенного домена — в **процессе браузера** по URL фрейма, приславшего сообщение,
а не в рендер-процессе. Список — тот же `platform/accepted_domains.txt` с тем же регулярным
выражением, что в `AcceptedDomains.cpp`.

Доступ к движку: через `nitro_api2.dll` NextClient, если он даёт нужное (подключение,
консоль, `cl_enginefunc_t`), иначе собственные хуки. Выбор за исполнителем после изучения
`nextclient/nitro_api`.

### 5.5 Старый CEF1

Оригинальные `chromehtml.dll`/`libcef.dll` остаются загруженными. NextClient при старте зовёт
`CefRegisterExtension` старого CEF1. Если оригинальный контроллер не инициализирован, эти
вызовы могут завершиться ошибкой или assert. Как поступить, решает спайк: либо вызывать `Init` у
оригинального контроллера (CEF1 поднимется без единого браузера), либо убедиться, что
неинициализированный CEF1 просто возвращает `false`.

## 6. Как проверяем

- **Спайк (первый шаг плана)**: порядок загрузки `.asi` vs запрос контроллера; что HL1 зовёт
  только `CreateSerializer`; поведение CEF1 без `Init`; запуск CEF 150 x86 внутри `hl.exe`.
- **Модульные тесты** (без игры): маппинг клавиш, BGRA→RGBA и dirty-rect, парсер доверенных
  доменов (те же кейсы, что regex NextClient), сериализация зеркала состояния.
- **Тестовый стенд** `ncb_harness.exe`: грузит `nextchromium.dll`, создаёт браузер через наш
  контроллер с фейковым `IHTMLResponses_HL1`, сохраняет кадры в PNG — проверка рендера и ввода
  без запуска игры.
- **Тестовая страница** `tests/pages/modern.html`: flex, grid, CSS-переменные, `:has()`,
  ES2023, `fetch`, анимации, `<select>`, ввод русского текста, `alert/confirm`, все функции
  `nextclient.*` с выводом результатов.
- **Ручная проверка в игре**: MOTD на локальном сервере (`tools/test_server` NextClient),
  главное меню, удаление мода → оригинальное поведение, `-nonextchromium`.
- Замер FPS: закрытые окна — без изменений; открытый MOTD — без заметных просадок.

## 7. Решения, принятые мной (проверьте)

- Название мода и папки: `nextchromium` / `nextchromium.asi`.
- CEF 150.0.23 windows32 (последний stable на 2026-11-01), multi-threaded message loop.
- Отдельный кэш/куки (`nextchromium\cache`), старый `htmlcache` не трогаем.
- Хуки через MinHook (или funchook, который уже использует NextClient).
- Синхронные JS-геттеры через зеркало состояния, а не через блокирующий IPC.
- Matchmaking (`nextclient.matchmaking.*`) — последний этап, после основного браузера.
- Отключение мода на Windows < 10 и при любой ошибке инициализации.
- Эталон совместимости — JS-API из исходников NextClient (`main` на 2026-11-01), таблица §5.4.
  Прямая работа NextClient с DOM старых CEF1-браузеров (классы `cef_domvisitor_t` и пр. в
  импортах — часть обёртки `libcef_dll_wrapper`) в объём не входит, если спайк не покажет,
  что она реально используется.
- Обновления NextClient: мод проверяет версию/хэш `chromehtml.dll` и `hw.dll`; на незнакомой
  сборке хуки ставятся только по сигнатурам, а если сигнатура не найдена — мод отключается.
- Файлов `platform/accepted_domains.txt`, `mmb_home_page.html`, `mmb_error_page.html` в
  текущей установке нет. Поведение — как у оригинала: нет списка → привилегированные вызовы
  запрещены всем; нет `mmb_home_page.html` → браузер главного меню не создаётся.
- Тестовая среда — эта же установка игры: мод только **добавляет** свои файлы, проверки в
  игре запускаю я, ручные проверки (FPS, ощущения) — по чек-листу пользователем.

## 8. Оставлено исполнителю

Конкретный запасной способ перехвата (если `.asi` грузится поздно), механизм синхронного
чтения cvar, способ доступа к состоянию подключения и выводу консоли, раскладка CMake-проекта,
формат лога.
