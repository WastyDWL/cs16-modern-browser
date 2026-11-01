# Результаты спайка (Task 1), 2026-11-01

Сборка: NextClient, `chromehtml.dll` CEF1 1.989.464. Проверено в игре на сервере
45.95.31.157:27115 (MOTD `/top15`).

1. **`.asi` грузится поздно.** К моменту `DllMain` уже загружены `hw.dll`, `chromehtml.dll`,
   `libcef.dll`, `client.dll`, `GameUI.dll`, `next_engine_mini.dll`, `nitro_api2.dll`.
   `IHTMLChromeController::Init` уже вызван, `CreateInterface("ChromeHTML_Controller_001")`
   после загрузки `.asi` никто не зовёт. Хук `CreateInterface` бесполезен.
2. **Контроллер — синглтон** `CHTMLController` (`CreateInterface` возвращает тот же объект).
   Vtable из 15 слотов, порядок совпадает с `ncl-hl1-source-sdk` (0 — деструктор, 14 —
   `CreateSerializer`). `RunFrame` вызывается каждый кадр из главного потока.
3. **Окно MOTD создаёт сериализатор и браузер при старте клиента**, до загрузки `.asi`.
   Поэтому перехватывать нужно класс, а не фабрику.
4. **Класс `HTMLSerializer`** (RTTI `.?AVHTMLSerializer@@`): по смещению +0 — `IHTMLResponses`,
   по +4 — `IHTMLSerializer` (28 слотов, без деструктора; слот 23 — NeedsPaintResponse, 24 — StartRequestResponse). Для `this` подобъекта
   `IHTMLSerializer` (адрес объекта + 4):
   - `this+0x4` — индекс браузера (`65536` у MOTD);
   - `this+0x8` — указатель на `CHTMLController`;
   - `this+0xC` — `IHTMLResponses_HL1*` (цель колбэков, vtable в `client.dll`).
   `GetResponseTarget()` возвращает подобъект `IHTMLResponses` = `this - 4`.
5. **MOTD:** `client.dll` вызывает `PostURL("file:///c:\games\cs 1.6 - nextclient\\cstrike\motd_temp.html")`
   — нужна поддержка `file://`. Размер окна задаётся `BrowserSize` (у MOTD 720×360).
   `NeedsPaintResponse(textureid)` — непрозрачный cookie, который вернули в `BrowserNeedsPaint`.

## Следствия для реализации

- Загрузчик патчит **vtable класса `HTMLSerializer` (подобъект +4) в `.rdata` `chromehtml.dll`**
  и **vptr объекта-синглтона `CHTMLController`**. Так перехватываются и уже созданные окна.
- Состояние нашего браузера хранится в карте `this сериализатора → NcBrowser`; цель колбэков
  читается из `this+0xC` (проверка: указатель на vtable внутри загруженного модуля).
- Старые CEF1-браузеры остаются нетронутыми, но простаивают: `PostURL`, `BrowserSize` и ввод
  до них не доходят, оригинальный `RunFrame` не вызывается, `Init` уже был.
- Совместимость сборки проверяется по: 15 слотам контроллера, найденному RTTI
  `HTMLSerializer` с подобъектом +4, и указателю `this+0xC` на объект с vtable в модуле.
  Любое несовпадение → мод не активируется.
