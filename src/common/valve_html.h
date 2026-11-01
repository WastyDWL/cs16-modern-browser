// Interfaces between GoldSrc (hw.dll / vgui2 HTML panels) and chromehtml.dll.
// Layout mirrors CS-NextClient/ncl-hl1-source-sdk public/html/*.h, release build
// (DBGFLAG_VALIDATE not defined). Vtable order must not change.
#pragma once

#include <cstdint>

using RTime32 = uint32_t;

class CUtlString;
class IHTMLResponses;  // protobuf response target; opaque to HL1 code
struct HTMLCommandBuffer_t;
enum EHTMLCommands : int;

class IHTMLResponses_HL1 {
public:
    virtual void BrowserReady() = 0;
    virtual void BrowserNeedsPaint(int textureid, int wide, int tall, const unsigned char* rgba,
                                   int updatex, int updatey, int updatewide, int updatetall,
                                   int combobox_wide, int combobox_tall, const unsigned char* combobox_rgba) = 0;
    virtual void BrowserStartRequest(const char* url, const char* target, const char* postdata, bool isredirect) = 0;
    virtual void BrowserURLChanged(const char* url, const char* postdata, bool isredirect) = 0;
    virtual void BrowserFinishedRequest(const char* url, const char* pagetitle) = 0;
    virtual void BrowserShowPopup() = 0;
    virtual void BrowserHidePopup() = 0;
    virtual void BrowserSizePopup(int x, int y, int wide, int tall) = 0;
    virtual void BrowserHorizontalScrollBarSizeResponse(int x, int y, int wide, int tall, int scroll, int scroll_max, float zoom) = 0;
    virtual void BrowserVerticalScrollBarSizeResponse(int x, int y, int wide, int tall, int scroll, int scroll_max, float zoom) = 0;
    virtual void BrowserGetZoomResponse(float flZoom) = 0;
    virtual void BrowserCanGoBackandForward(bool bgoback, bool bgoforward) = 0;
    virtual void BrowserJSAlert(const char* message) = 0;
    virtual void BrowserJSConfirm(const char* message) = 0;
    virtual void BrowserPopupHTMLWindow(const char* url, int wide, int tall, int x, int y) = 0;
    virtual void BrowserSetHTMLTitle(const char* title) = 0;
    virtual void BrowserLoadingResource() = 0;
    virtual void BrowserStatusText(const char* text) = 0;
    virtual void BrowserSetCursor(int in_cursor) = 0;
    virtual void BrowserFileLoadDialog() = 0;
    virtual void BrowserShowToolTip(const char* text) = 0;
    virtual void BrowserUpdateToolTip(const char* text) = 0;
    virtual void BrowserHideToolTip() = 0;
    virtual void BrowserClose() = 0;
    virtual void BrowserLinkAtPositionResponse(const char* url, int x, int y) = 0;
};

class IHTMLSerializer {
public:
    virtual IHTMLResponses* GetResponseTarget() = 0;
    virtual int BrowserGetIndex() = 0;
    virtual void BrowserPosition(int x, int y) = 0;
    virtual void PostURL(const char* pchURL, const char* pchPostData) = 0;
    virtual void BrowserErrorStrings(const char* pchTitle, const char* pchHeader, const char* pchDetailCacheMiss,
                                     const char* pchDetailBadUURL, const char* pchDetailConnectionProblem,
                                     const char* pchDetailProxyProblem, const char* pchDetailUnknown) = 0;
    virtual void StopLoad() = 0;
    virtual void Reload() = 0;
    virtual void GoBack() = 0;
    virtual void GoForward() = 0;
    virtual void MouseDown(int code) = 0;
    virtual void MouseUp(int code) = 0;
    virtual void MouseWheel(int delta) = 0;
    virtual void MouseMove(int x, int y) = 0;
    virtual void MouseDoubleClick(int code) = 0;
    virtual void BrowserSize(int wide, int tall) = 0;
    virtual void KeyUp(int key, int mods) = 0;
    virtual void KeyDown(int key, int mods) = 0;
    virtual void KeyChar(int unichar) = 0;
    virtual void RunJavaScript(const char* pchScript) = 0;
    virtual void SetHorizontalScroll(int scroll) = 0;
    virtual void SetVerticalScroll(int scroll) = 0;
    virtual void SetFocus(bool focus) = 0;
    virtual void AddHeader(const char* pchHeader, const char* pchValue) = 0;
    virtual void NeedsPaintResponse(int tex) = 0;
    virtual void StartRequestResponse(bool bRes) = 0;
    virtual void RequestBrowserSizes() = 0;
    virtual void JSDialogResponse(int res) = 0;
    virtual void GetLinkAtPosition(int x, int y) = 0;
};

class IHTMLChromeController {
public:
    virtual ~IHTMLChromeController() {}
    virtual bool Init(const char* pchHTMLCacheDir, const char* pchCookiePath) = 0;
    virtual void Shutdown() = 0;
    virtual bool RunFrame() = 0;
    virtual void SetWebCookie(const char* pchHostname, const char* pchKey, const char* pchValue, const char* pchPath,
                              RTime32 nExpires = 0) = 0;
    virtual void GetWebCookiesForURL(CUtlString* pstrValue, const char* pchURL, const char* pchName) = 0;
    virtual void SetClientBuildID(uint64_t ulBuildID) = 0;
    virtual bool BHasPendingMessages() = 0;
    virtual void CreateBrowser(IHTMLResponses* pBrowser, bool bPopupWindow, const char* pchUserAgentIdentifier) = 0;
    virtual void RemoveBrowser(IHTMLResponses* pBrowser) = 0;
    virtual void WakeThread() = 0;
    virtual HTMLCommandBuffer_t* GetFreeCommandBuffer(EHTMLCommands eCmd, int iBrowser) = 0;
    virtual void PushCommand(HTMLCommandBuffer_t*) = 0;
    virtual void SetCefThreadTargetFrameRate(uint32_t nFPS) = 0;
    virtual IHTMLSerializer* CreateSerializer(IHTMLResponses_HL1* pResponseTarget) = 0;
};

#define CHROMEHTML_CONTROLLER_INTERFACE_VERSION "ChromeHTML_Controller_001"

// Source-engine style factory exported as CreateInterface.
using CreateInterfaceFn = void* (*)(const char* pName, int* pReturnCode);
