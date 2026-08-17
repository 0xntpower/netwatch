#include "stdafx.h"
#include "resource.h"

#include "aboutdlg.h"
#include "View.h"
#include "MainFrm.h"
#include "FilterDlg.h"
#include "PropertiesDialog.h"
#include "ColumnsDlg.h"
#include "ProcessUtil.hpp"
#include "util/MessageBox.h"
#include "util/WindowsError.h"
#include "util/StringConversion.h"
#include "util/Dpi.h"
#include "util/Settings.h"

#include <algorithm>

#include <sstream>
#include <iomanip>
#include <htmlhelp.h>

BOOL CMainFrame::PreTranslateMessage(MSG* pMsg)
{
    if(WTL::CFrameWindowImpl<CMainFrame>::PreTranslateMessage(pMsg))
        return TRUE;

    return m_view.PreTranslateMessage(pMsg);
}

BOOL CMainFrame::OnIdle()
{
    UIUpdateToolBar();
    UpdateUIState();
    return FALSE;
}

LRESULT CMainFrame::OnCreate(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/)
{
    LoadSettings();

    HWND hWndCmdBar = m_CmdBar.Create(m_hWnd, rcDefault, nullptr, ATL_SIMPLE_CMDBAR_PANE_STYLE);
    m_CmdBar.AttachMenu(GetMenu());
    SetMenu(nullptr);

    // m_hWndToolBar ends up pointing at the rebar, so keep the toolbar itself.
    HWND hWndToolBar = CreateToolBarCtrl();
    m_hWndToolBarCtrl = hWndToolBar;

    CreateSimpleReBar(ATL_SIMPLE_REBAR_NOBORDER_STYLE | RBS_FIXEDORDER);
    AddSimpleReBarBand(hWndCmdBar);
    AddSimpleReBarBand(hWndToolBar, nullptr, TRUE);

    CreateSimpleStatusBar();

    // No WS_EX_CLIENTEDGE: the sunken 3D bevel is Windows 98 chrome. Explorer,
    // Task Manager and TCPView all sit their list flush against the frame.
    // No LVS_SINGLESEL either, so several rows can be selected and copied.
    m_hWndClient = m_view.Create(
        m_hWnd,
        rcDefault,
        nullptr,
        WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS | WS_CLIPCHILDREN |
        LVS_REPORT | LVS_SHOWSELALWAYS | LVS_OWNERDATA | LVS_SHAREIMAGELISTS,
        0
    );

    m_view.InitColumns();

    UIAddToolBar(hWndToolBar);
    UISetCheck(ID_VIEW_TOOLBAR, 1);
    UISetCheck(ID_VIEW_STATUS_BAR, 1);

    // Reflect the restored options in both the menu and the actual behaviour.
    UISetCheck(ID_OPTIONS_ALWAYSONTOP, m_bAlwaysOnTop);
    UISetCheck(ID_OPTIONS_SHOWUNCONNECTED, m_bShowUnconnected);
    m_view.SetShowUnconnected(m_bShowUnconnected);
    if (m_bAlwaysOnTop) {
        SetWindowPos(HWND_TOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE);
    }

    WTL::CMessageLoop* pLoop = _Module.GetMessageLoop();
    ATLASSERT(pLoop != nullptr);
    pLoop->AddMessageFilter(this);
    pLoop->AddIdleHandler(this);

    m_updateFreqLabel.Create(m_hWndStatusBar, rcDefault, _T("Refresh: 2s"),
        WS_CHILD | WS_VISIBLE | SS_RIGHT | SS_CENTERIMAGE, 0, IDC_UPDATE_FREQ_LABEL);

    // Borrow the status bar's own font. GetStockObject(DEFAULT_GUI_FONT) returns
    // the MS Sans Serif bitmap font from the Windows 95 era, which is visibly
    // wrong sitting next to Segoe UI status text.
    if (HFONT hStatusFont = reinterpret_cast<HFONT>(
            ::SendMessage(m_hWndStatusBar, WM_GETFONT, 0, 0))) {
        m_updateFreqLabel.SetFont(hStatusFont, FALSE);
    }

    m_updateFreqSlider.Create(m_hWndStatusBar, rcDefault, nullptr,
        WS_CHILD | WS_VISIBLE | TBS_HORZ | TBS_NOTICKS | TBS_TOOLTIPS,
        0, IDC_UPDATE_FREQ_SLIDER);
    m_updateFreqSlider.SetRange(kSliderMin, kSliderMax);
    m_updateFreqSlider.SetPos(IntervalToSliderPos(m_nUpdateInterval));

    LayoutStatusBar();

    SetTimer(IDT_REFRESH_TIMER, m_nUpdateInterval, nullptr);

    if (!m_processFilter.empty()) {
        m_view.SetFilter(m_processFilter);
    }

    m_view.RefreshConnections();
    UpdateStatusBar();

    return 0;
}

// Load the toolbar glyph strip matching the current DPI as a 32-bit ARGB image
// list.
//
// LR_CREATEDIBSECTION preserves the 32-bit source, which is what makes
// ImageList_LoadImage build an ILC_COLOR32 list with a live alpha channel
// rather than colour-keying against a mask.
HIMAGELIST CMainFrame::LoadToolbarImages(int& sizeOut) const
{
    const int dpi = netwatch::util::DpiForWindow(m_hWnd);

    struct Bracket { int minDpi; UINT resourceId; int size; };
    // Bracket on the sizes the strips were actually drawn at, so a glyph is
    // never resampled to a size it was not authored for.
    static constexpr Bracket kBrackets[] = {
        { 192, IDB_TOOLBAR32, 32 },
        { 144, IDB_TOOLBAR24, 24 },
        { 120, IDB_TOOLBAR20, 20 },
        {   0, IDB_TOOLBAR16, 16 },
    };

    const Bracket* chosen = &kBrackets[3];
    for (const auto& bracket : kBrackets) {
        if (dpi >= bracket.minDpi) {
            chosen = &bracket;
            break;
        }
    }

    sizeOut = chosen->size;
    return ::ImageList_LoadImageW(
        _Module.GetResourceInstance(),
        MAKEINTRESOURCEW(chosen->resourceId),
        chosen->size, 0, CLR_NONE, IMAGE_BITMAP,
        LR_CREATEDIBSECTION);
}

// Build the toolbar explicitly rather than through CreateSimpleToolBarCtrl.
//
// That helper routes the bitmap through TB_ADDBITMAP, the pre-XP path with no
// alpha support and no DPI scaling. Attaching an image list to a toolbar built
// that way does not reliably take. Creating the control and giving it the image
// list before any button exists is both simpler to reason about and the
// documented order.
HWND CMainFrame::CreateToolBarCtrl()
{
    HWND hWndToolBar = ::CreateWindowEx(0, TOOLBARCLASSNAME, nullptr,
        ATL_SIMPLE_TOOLBAR_PANE_STYLE,
        0, 0, 100, 100, m_hWnd,
        reinterpret_cast<HMENU>(static_cast<UINT_PTR>(ATL_IDW_TOOLBAR)),
        _Module.GetModuleInstance(), nullptr);
    if (hWndToolBar == nullptr) {
        return nullptr;
    }

    ::SendMessage(hWndToolBar, TB_BUTTONSTRUCTSIZE, sizeof(TBBUTTON), 0);

    int glyphSize = 16;
    if (HIMAGELIST images = LoadToolbarImages(glyphSize)) {
        ::SendMessage(hWndToolBar, TB_SETIMAGELIST, 0, reinterpret_cast<LPARAM>(images));
        if (m_toolbarImages != nullptr) {
            ::ImageList_Destroy(m_toolbarImages);
        }
        m_toolbarImages = images;
    }

    // Image index order matches the glyph strip. Separators carry no image.
    static constexpr struct { UINT command; int image; } kButtons[] = {
        { ID_FILE_REFRESH,              0 },
        { ID_FILE_PAUSE,                1 },
        { 0,                           -1 },
        { ID_PROCESS_ENDPROCESS,        2 },
        { ID_PROCESS_CLOSECONNECTION,   3 },
        { 0,                           -1 },
        { ID_PROCESS_PROPERTIES,        4 },
        { ID_APP_ABOUT,                 5 },
    };

    TBBUTTON buttons[std::size(kButtons)] = {};
    for (size_t i = 0; i < std::size(kButtons); ++i) {
        if (kButtons[i].image < 0) {
            buttons[i].fsStyle = BTNS_SEP;
            buttons[i].iBitmap = netwatch::util::ScaleForWindow(6, m_hWnd);
            continue;
        }
        buttons[i].iBitmap = kButtons[i].image;
        buttons[i].idCommand = static_cast<int>(kButtons[i].command);
        buttons[i].fsState = TBSTATE_ENABLED;
        // No BTNS_AUTOSIZE: it sizes a button to its label, and these buttons
        // have none, so the width collapses to zero.
        buttons[i].fsStyle = BTNS_BUTTON;
        buttons[i].iString = -1;
    }

    ::SendMessage(hWndToolBar, TB_ADDBUTTONS,
        static_cast<WPARAM>(std::size(buttons)),
        reinterpret_cast<LPARAM>(buttons));

    const int padding = netwatch::util::ScaleForWindow(8, m_hWnd);
    ::SendMessage(hWndToolBar, TB_SETPADDING, 0, MAKELPARAM(padding, padding));
    ::SendMessage(hWndToolBar, TB_AUTOSIZE, 0, 0);

    return hWndToolBar;
}

// Re-load the strip at the new scale after the window changes monitor.
void CMainFrame::ApplyToolbarImages(HWND hWndToolBar)
{
    if (hWndToolBar == nullptr) {
        return;
    }

    int glyphSize = 16;
    HIMAGELIST images = LoadToolbarImages(glyphSize);
    if (images == nullptr) {
        return;
    }

    ::SendMessage(hWndToolBar, TB_SETIMAGELIST, 0, reinterpret_cast<LPARAM>(images));

    const int padding = netwatch::util::ScaleForWindow(8, m_hWnd);
    ::SendMessage(hWndToolBar, TB_SETPADDING, 0, MAKELPARAM(padding, padding));
    ::SendMessage(hWndToolBar, TB_AUTOSIZE, 0, 0);

    if (m_toolbarImages != nullptr) {
        ::ImageList_Destroy(m_toolbarImages);
    }
    m_toolbarImages = images;
}

// Restore whatever the user left behind last time. Window placement goes
// through WINDOWPLACEMENT so a maximised window comes back maximised, and the
// stored restore rectangle is in workspace coordinates, which survives a
// monitor being unplugged.
void CMainFrame::LoadSettings()
{
    m_nUpdateInterval = netwatch::util::Settings::GetInt(L"RefreshIntervalMs", kDefaultUpdateIntervalMs);
    m_nUpdateInterval = (std::max)(SliderPosToInterval(kSliderMin),
                        (std::min)(SliderPosToInterval(kSliderMax), m_nUpdateInterval));

    m_bAlwaysOnTop = netwatch::util::Settings::GetInt(L"AlwaysOnTop", 0) != 0;
    m_bShowUnconnected = netwatch::util::Settings::GetInt(L"ShowUnconnected", 1) != 0;
}

void CMainFrame::SaveSettings()
{
    WINDOWPLACEMENT placement = {};
    placement.length = sizeof(placement);
    if (GetWindowPlacement(&placement)) {
        // Never come back minimised, that just looks like the app failed to open.
        if (placement.showCmd == SW_SHOWMINIMIZED) {
            placement.showCmd = SW_SHOWNORMAL;
        }
        netwatch::util::Settings::SetBinary(L"WindowPlacement", &placement, sizeof(placement));
    }

    netwatch::util::Settings::SetInt(L"RefreshIntervalMs", m_nUpdateInterval);
    netwatch::util::Settings::SetInt(L"AlwaysOnTop", m_bAlwaysOnTop ? 1 : 0);
    netwatch::util::Settings::SetInt(L"ShowUnconnected", m_bShowUnconnected ? 1 : 0);

    m_view.SaveLayout();
}

void CMainFrame::RestoreWindowPlacement()
{
    WINDOWPLACEMENT placement = {};
    if (!netwatch::util::Settings::GetBinary(L"WindowPlacement", &placement, sizeof(placement))) {
        return;
    }
    if (placement.length != sizeof(placement)) {
        return;
    }

    // SetWindowPlacement clamps the rectangle to the current virtual desktop,
    // so a window saved on a monitor that is now gone still lands on screen.
    SetWindowPlacement(&placement);
}

LRESULT CMainFrame::OnDestroy(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& bHandled)
{
    SaveSettings();
    KillTimer(IDT_REFRESH_TIMER);

    if (m_toolbarImages != nullptr) {
        ::ImageList_Destroy(m_toolbarImages);
        m_toolbarImages = nullptr;
    }

    WTL::CMessageLoop* pLoop = _Module.GetMessageLoop();
    ATLASSERT(pLoop != nullptr);
    pLoop->RemoveMessageFilter(this);
    pLoop->RemoveIdleHandler(this);

    bHandled = FALSE;
    return 1;
}

LRESULT CMainFrame::OnTimer(UINT /*uMsg*/, WPARAM wParam, LPARAM /*lParam*/, BOOL& /*bHandled*/)
{
    if (wParam == IDT_REFRESH_TIMER && !m_bPaused)
    {
        m_view.RefreshConnections();
    }
    return 0;
}

LRESULT CMainFrame::OnRefreshComplete(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& bHandled)
{
    UpdateStatusBar();
    bHandled = FALSE;
    return 0;
}

LRESULT CMainFrame::OnFileExit(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
    PostMessage(WM_CLOSE);
    return 0;
}

LRESULT CMainFrame::OnFileRefresh(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
    m_view.RefreshConnections();
    UpdateStatusBar();
    return 0;
}

LRESULT CMainFrame::OnFilePause(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
    m_bPaused = !m_bPaused;
    UISetCheck(ID_FILE_PAUSE, m_bPaused);
    UpdateStatusMessage();
    return 0;
}

// View menu handlers

LRESULT CMainFrame::OnViewToolBar(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
    static BOOL bVisible = TRUE;
    bVisible = !bVisible;
    WTL::CReBarCtrl rebar = m_hWndToolBar;
    int nBandIndex = rebar.IdToIndex(ATL_IDW_BAND_FIRST + 1);
    rebar.ShowBand(nBandIndex, bVisible);
    UISetCheck(ID_VIEW_TOOLBAR, bVisible);
    UpdateLayout();
    return 0;
}

LRESULT CMainFrame::OnViewStatusBar(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
    BOOL bVisible = !::IsWindowVisible(m_hWndStatusBar);
    ::ShowWindow(m_hWndStatusBar, bVisible ? SW_SHOWNOACTIVATE : SW_HIDE);
    UISetCheck(ID_VIEW_STATUS_BAR, bVisible);
    UpdateLayout();
    return 0;
}

// Options menu handlers

LRESULT CMainFrame::OnOptionsAlwaysOnTop(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
    m_bAlwaysOnTop = !m_bAlwaysOnTop;
    UISetCheck(ID_OPTIONS_ALWAYSONTOP, m_bAlwaysOnTop);
    
    SetWindowPos(m_bAlwaysOnTop ? HWND_TOPMOST : HWND_NOTOPMOST, 
        0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE);
    
    return 0;
}

LRESULT CMainFrame::OnOptionsShowUnconnected(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
    m_bShowUnconnected = !m_bShowUnconnected;
    UISetCheck(ID_OPTIONS_SHOWUNCONNECTED, m_bShowUnconnected);

    // Update view settings and refresh
    m_view.SetShowUnconnected(m_bShowUnconnected);
    m_view.RefreshConnections();
    UpdateStatusBar();

    return 0;
}

LRESULT CMainFrame::OnOptionsFilter(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
    // Show filter dialog
    CFilterDlg dlg(m_processFilter);
    if (dlg.DoModal() == IDOK)
    {
        m_processFilter = dlg.GetFilter();
        m_view.SetFilter(m_processFilter);
        m_view.RefreshConnections();
        UpdateStatusBar();

        // Reflect the filter in the title bar so it is visible from the taskbar
        // and Alt-Tab, not just inside the window.
        if (!m_processFilter.empty()) {
            std::wstring title = L"NetWatch - Filter: ";
            title += netwatch::util::StringConversion::NarrowToWide(m_processFilter);
            SetWindowText(title.c_str());
        }
        else {
            SetWindowText(_T("NetWatch"));
        }
    }

    return 0;
}

// Process menu handlers

// Read the selected row straight out of the view's backing data.
//
// The previous code pulled each field back out of the list control with
// GetItemText(row, COL_*), which takes a *subitem* index. As soon as any column
// was hidden the subitem indices stopped lining up with the COL_* constants, so
// End Process could read a neighbouring column and terminate the wrong PID.
bool CMainFrame::GetSelectedEntry(netwatch::util::EndpointEntry& entry)
{
    const int nSelected = m_view.GetNextItem(-1, LVNI_SELECTED);
    if (nSelected < 0) {
        return false;
    }
    return m_view.GetEntry(nSelected, entry);
}

LRESULT CMainFrame::OnProcessEnd(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
    netwatch::util::EndpointEntry entry;
    if (!GetSelectedEntry(entry)) {
        return 0;
    }

    if (entry.pid == 0) {
        netwatch::util::MessageBox::ShowWarning(m_hWnd,
            "The System Idle Process cannot be terminated.",
            "End Process");
        return 0;
    }

    std::ostringstream oss;
    oss << "Are you sure you want to end process '" << entry.processName
        << "' (PID " << entry.pid << ")?\n\n"
        << "Warning: This will terminate the process immediately and may cause data loss.";

    if (netwatch::util::MessageBox::ShowConfirmWarning(m_hWnd, oss.str(), "End Process") != IDYES) {
        return 0;
    }

    if (netwatch::util::TerminateTargetProcess(entry.pid)) {
        // The list reflects the result on the next tick anyway, but refreshing
        // now means the row disappears while the user is still looking at it.
        m_view.RefreshConnections();
        return 0;
    }

    const DWORD errorCode = ::GetLastError();
    std::ostringstream errOss;
    errOss << "Failed to terminate '" << entry.processName
           << "' (PID " << entry.pid << ").\n\n"
           << "Error: " << netwatch::util::WindowsError::GetErrorMessage(errorCode);
    if (errorCode == ERROR_ACCESS_DENIED) {
        errOss << "\nThis process runs at a higher integrity level. "
                  "Restart NetWatch as administrator to terminate it.";
    }
    netwatch::util::MessageBox::ShowError(m_hWnd, errOss.str(), "End Process Failed");

    return 0;
}

LRESULT CMainFrame::OnProcessCloseConnection(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
    netwatch::util::EndpointEntry entry;
    if (!GetSelectedEntry(entry)) {
        return 0;
    }

    if (entry.protocol != "TCP" && entry.protocol != "TCPv6") {
        netwatch::util::MessageBox::ShowWarning(m_hWnd,
            "Only TCP connections can be closed.\nUDP endpoints have no connection to tear down.",
            "Invalid Operation");
        return 0;
    }

    // SetTcpEntry only accepts an IPv4 MIB_TCPROW. The IPv6 equivalent
    // (SetTcp6Entry) does not exist in the public API.
    if (entry.protocol == "TCPv6") {
        netwatch::util::MessageBox::ShowWarning(m_hWnd,
            "Windows provides no API to close an IPv6 TCP connection.\n\n"
            "End the owning process instead if you need to drop it.",
            "Not Supported");
        return 0;
    }

    if (!entry.hasRemote) {
        netwatch::util::MessageBox::ShowWarning(m_hWnd,
            "Cannot close a listening socket.\nOnly established connections can be closed.",
            "Invalid Operation");
        return 0;
    }

    IN_ADDR localIn = {};
    IN_ADDR remoteIn = {};
    if (::inet_pton(AF_INET, entry.localAddress.c_str(), &localIn) != 1 ||
        ::inet_pton(AF_INET, entry.remoteAddress.c_str(), &remoteIn) != 1) {
        netwatch::util::MessageBox::ShowError(m_hWnd,
            "Could not parse the connection addresses.", "Close Connection");
        return 0;
    }

    std::ostringstream oss;
    oss << "Are you sure you want to close this TCP connection?\n\n"
        << entry.localAddress << ":" << entry.localPort << "  <->  "
        << entry.remoteAddress << ":" << entry.remotePort << "\n\n"
        << "Owned by " << entry.processName << " (PID " << entry.pid << ").";

    if (netwatch::util::MessageBox::ShowConfirmWarning(m_hWnd, oss.str(), "Close Connection") != IDYES) {
        return 0;
    }

    std::string errorMessage;
    const bool closed = netwatch::util::CloseNetworkConnection(
        localIn.s_addr, htons(entry.localPort),
        remoteIn.s_addr, htons(entry.remotePort),
        errorMessage);

    if (closed) {
        m_view.RefreshConnections();
        return 0;
    }

    std::ostringstream errOss;
    errOss << "Failed to close the connection.\n\nError: " << errorMessage
           << "\nClosing a connection requires administrator rights.";
    netwatch::util::MessageBox::ShowError(m_hWnd, errOss.str(), "Close Connection Failed");

    return 0;
}

LRESULT CMainFrame::OnProcessProperties(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
    netwatch::util::EndpointEntry entry;
    if (!GetSelectedEntry(entry)) {
        return 0;
    }

    CPropertiesDialog::ConnectionProperties props;
    props.processName = entry.processName;
    props.pid = std::to_string(entry.pid);
    props.architecture = entry.architecture;
    props.integrityLevel = entry.integrityLevel;
    props.protocol = entry.protocol;
    props.state = entry.state;
    props.localAddress = entry.localAddress;
    props.localPort = std::to_string(entry.localPort);
    props.remoteAddress = entry.remoteAddress;
    props.remotePort = entry.hasRemote ? std::to_string(entry.remotePort) : std::string();
    props.executablePath = entry.executablePath;
    props.depStatus = entry.depStatus;
    props.aslrStatus = entry.aslrStatus;
    props.cfgStatus = entry.cfgStatus;
    props.safeSehStatus = entry.safeSehStatus;

    CPropertiesDialog dlg(props);
    dlg.DoModal(m_hWnd);

    return 0;
}

LRESULT CMainFrame::OnProcessWhois(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
    netwatch::util::EndpointEntry entry;
    if (!GetSelectedEntry(entry)) {
        return 0;
    }

    if (!entry.hasRemote) {
        netwatch::util::MessageBox::ShowInfo(m_hWnd,
            "No remote address available for WHOIS lookup.\n\n"
            "This endpoint is either listening or a UDP socket with no peer.",
            "WHOIS Lookup");
        return 0;
    }

    if (!IsRoutableRemote(entry.remoteAddress)) {
        netwatch::util::MessageBox::ShowInfo(m_hWnd,
            "WHOIS is only meaningful for public addresses.\n\n"
            "This connection is to a loopback or private-range address, which "
            "is not registered with any regional internet registry.",
            "WHOIS Lookup");
        return 0;
    }

    // Sending the address to a third-party service is an outbound disclosure,
    // so confirm before opening the browser.
    std::ostringstream confirm;
    confirm << "Look up " << entry.remoteAddress << " at whois.com?\n\n"
            << "This opens your web browser and sends the address to that service.";
    if (netwatch::util::MessageBox::ShowConfirm(m_hWnd, confirm.str(), "WHOIS Lookup") != IDYES) {
        return 0;
    }

    const std::string url = "https://www.whois.com/whois/" + entry.remoteAddress;
    const std::wstring wideUrl = netwatch::util::StringConversion::NarrowToWide(url);

    const HINSTANCE result = ::ShellExecuteW(m_hWnd, L"open", wideUrl.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
    if (reinterpret_cast<INT_PTR>(result) <= 32) {
        netwatch::util::MessageBox::ShowError(m_hWnd,
            "Failed to open a web browser for the WHOIS lookup.\n\n"
            "Check that a default browser is configured.",
            "WHOIS Lookup Failed");
    }

    return 0;
}

// Loopback, link-local and RFC1918 space is not registered with any RIR, so a
// WHOIS query for it is guaranteed to be useless.
bool CMainFrame::IsRoutableRemote(const std::string& address)
{
    IN6_ADDR v6 = {};
    if (::inet_pton(AF_INET6, address.c_str(), &v6) == 1) {
        if (IN6_IS_ADDR_LOOPBACK(&v6) || IN6_IS_ADDR_LINKLOCAL(&v6) ||
            IN6_IS_ADDR_SITELOCAL(&v6) || IN6_IS_ADDR_UNSPECIFIED(&v6)) {
            return false;
        }
        // fc00::/7 unique local
        return (v6.s6_bytes[0] & 0xFE) != 0xFC;
    }

    IN_ADDR v4 = {};
    if (::inet_pton(AF_INET, address.c_str(), &v4) != 1) {
        return false;
    }

    const uint32_t host = ntohl(v4.s_addr);
    const uint8_t a = static_cast<uint8_t>(host >> 24);
    const uint8_t b = static_cast<uint8_t>(host >> 16);

    if (a == 0 || a == 127) return false;                      // unspecified, loopback
    if (a == 10) return false;                                 // 10/8
    if (a == 172 && b >= 16 && b <= 31) return false;          // 172.16/12
    if (a == 192 && b == 168) return false;                    // 192.168/16
    if (a == 169 && b == 254) return false;                    // link-local
    if (a >= 224) return false;                                // multicast, reserved

    return true;
}

// Copy the selected rows to the clipboard as tab-separated text, with a header
// row, so it pastes straight into a spreadsheet or an incident note.
//
// ID_EDIT_COPY was already in the context menu and the accelerator table, but
// no handler was ever registered for it, so Ctrl+C silently did nothing.
LRESULT CMainFrame::OnEditCopy(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
    std::wstring text = m_view.BuildClipboardText();
    if (text.empty()) {
        return 0;
    }

    if (!OpenClipboard()) {
        return 0;
    }

    ::EmptyClipboard();

    const size_t bytes = (text.size() + 1) * sizeof(wchar_t);
    if (HGLOBAL hMem = ::GlobalAlloc(GMEM_MOVEABLE, bytes)) {
        if (void* dst = ::GlobalLock(hMem)) {
            std::memcpy(dst, text.c_str(), bytes);
            ::GlobalUnlock(hMem);
            if (::SetClipboardData(CF_UNICODETEXT, hMem) == nullptr) {
                // Ownership stays with us if SetClipboardData fails.
                ::GlobalFree(hMem);
            }
        } else {
            ::GlobalFree(hMem);
        }
    }

    ::CloseClipboard();
    return 0;
}

LRESULT CMainFrame::OnEditSelectAll(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
    m_view.SetItemState(-1, LVIS_SELECTED, LVIS_SELECTED);
    return 0;
}

// Column chooser. The header right-click menu still works, but it is neither
// discoverable nor reachable from the keyboard.
LRESULT CMainFrame::OnViewColumns(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
    CColumnsDlg dlg(m_view);
    dlg.DoModal(m_hWnd);
    return 0;
}

// Help menu handlers

LRESULT CMainFrame::OnAppHelpContents(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
    TCHAR szPath[MAX_PATH];
    GetModuleFileName(nullptr, szPath, MAX_PATH);

    TCHAR* pLastSlash = _tcsrchr(szPath, _T('\\'));
    if (pLastSlash) {
        _tcscpy_s(pLastSlash + 1, MAX_PATH - (pLastSlash - szPath + 1), _T("netwatch.chm"));

        // Use HtmlHelp instead of ShellExecute for better window management
        // This ensures the help window opens on the same monitor as the main window
        HtmlHelp(m_hWnd, szPath, HH_DISPLAY_TOPIC, 0);
    }

    return 0;
}

LRESULT CMainFrame::OnAppAbout(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
    CAboutDlg dlg;
    dlg.DoModal();
    return 0;
}

// Notification handlers

LRESULT CMainFrame::OnListViewItemChanged(int /*idCtrl*/, LPNMHDR pnmh, BOOL& /*bHandled*/)
{
    LPNMLISTVIEW pnmv = reinterpret_cast<LPNMLISTVIEW>(pnmh);
    
    // Update UI state when selection changes
    if (pnmv->uChanged & LVIF_STATE)
    {
        bool bHasSelection = (m_view.GetNextItem(-1, LVNI_SELECTED) >= 0);
        UIEnable(ID_PROCESS_ENDPROCESS, bHasSelection);
        UIEnable(ID_PROCESS_CLOSECONNECTION, bHasSelection);
        UIEnable(ID_PROCESS_PROPERTIES, bHasSelection);
        UIEnable(ID_PROCESS_WHOIS, bHasSelection);
    }
    
    return 0;
}

// Helper methods

void CMainFrame::SetStatusPart(int part, LPCTSTR text)
{
    ::SendMessage(m_hWndStatusBar, SB_SETTEXT, static_cast<WPARAM>(part),
        reinterpret_cast<LPARAM>(text));
}

void CMainFrame::UpdateStatusBar()
{
    if (m_hWndStatusBar == nullptr || !::IsWindow(m_hWndStatusBar)) {
        return;
    }

    // Counts are pre-calculated by the view during refresh, so this never walks
    // the list control.
    const auto& stats = m_view.GetStats();
    TCHAR szBuf[128];

    _stprintf_s(szBuf, _T("Connections: %d"), stats.totalConnections);
    SetStatusPart(kStatusPartConnections, szBuf);

    _stprintf_s(szBuf, _T("Endpoints: %d"), stats.totalEndpoints);
    SetStatusPart(kStatusPartEndpoints, szBuf);

    _stprintf_s(szBuf, _T("Listening: %d"), stats.totalListening);
    SetStatusPart(kStatusPartListening, szBuf);

    _stprintf_s(szBuf, _T("Total: %d"), stats.totalItems);
    SetStatusPart(kStatusPartTotal, szBuf);

    UpdateStatusMessage();
}

// The leftmost, stretching part. Shows whichever mode is currently in effect so
// a paused or filtered list can never be mistaken for a live unfiltered one.
void CMainFrame::UpdateStatusMessage()
{
    if (m_hWndStatusBar == nullptr || !::IsWindow(m_hWndStatusBar)) {
        return;
    }

    std::wstring message;
    if (m_bPaused) {
        message = L"Paused";
    } else {
        message = L"Monitoring";
    }

    if (!m_processFilter.empty()) {
        message += L"  (filter: ";
        message += netwatch::util::StringConversion::NarrowToWide(m_processFilter);
        message += L")";
    }

    SetStatusPart(kStatusPartMessage, message.c_str());
}

// The slider runs on a piecewise scale so the low end, where the choice actually
// matters, gets most of the travel: 0.5s to 1s in 100ms steps, then coarser out
// to 10s.
int CMainFrame::SliderPosToInterval(int pos)
{
    if (pos <= 10) {
        return pos * 100;
    }
    if (pos <= 30) {
        return 1000 + (pos - 10) * 100;
    }
    if (pos <= 50) {
        return 3000 + (pos - 30) * 100;
    }
    return 5000 + (pos - 50) * 100;
}

int CMainFrame::IntervalToSliderPos(int intervalMs)
{
    for (int pos = kSliderMin; pos <= kSliderMax; ++pos) {
        if (SliderPosToInterval(pos) >= intervalMs) {
            return pos;
        }
    }
    return kSliderMax;
}

void CMainFrame::UpdateRefreshLabel()
{
    TCHAR szLabel[64];
    if (m_nUpdateInterval < 1000) {
        _stprintf_s(szLabel, _T("Refresh: %.1fs"), m_nUpdateInterval / 1000.0f);
    } else {
        const int seconds = m_nUpdateInterval / 1000;
        const int tenths = (m_nUpdateInterval % 1000) / 100;
        if (tenths == 0) {
            _stprintf_s(szLabel, _T("Refresh: %ds"), seconds);
        } else {
            _stprintf_s(szLabel, _T("Refresh: %d.%ds"), seconds, tenths);
        }
    }
    m_updateFreqLabel.SetWindowText(szLabel);
}

void CMainFrame::UpdateUIState()
{
    // Enable/disable menu items based on current state
    bool bHasSelection = (m_view.GetNextItem(-1, LVNI_SELECTED) >= 0);

    UIEnable(ID_PROCESS_ENDPROCESS, bHasSelection);
    UIEnable(ID_PROCESS_CLOSECONNECTION, bHasSelection);
    UIEnable(ID_PROCESS_PROPERTIES, bHasSelection);
    UIEnable(ID_PROCESS_WHOIS, bHasSelection);
}

LRESULT CMainFrame::OnHScroll(UINT /*uMsg*/, WPARAM wParam, LPARAM lParam, BOOL& /*bHandled*/)
{
    if (reinterpret_cast<HWND>(lParam) != m_updateFreqSlider.m_hWnd) {
        return 0;
    }

    const WORD scrollCode = LOWORD(wParam);
    switch (scrollCode) {
    case TB_THUMBPOSITION:
    case TB_THUMBTRACK:
    case TB_LINEDOWN:
    case TB_LINEUP:
    case TB_PAGEDOWN:
    case TB_PAGEUP:
    case TB_ENDTRACK:
        break;
    default:
        return 0;
    }

    m_nUpdateInterval = SliderPosToInterval(m_updateFreqSlider.GetPos());
    UpdateRefreshLabel();

    // Only restart the timer once the user lets go, so dragging the thumb does
    // not fire a burst of enumerations.
    if (scrollCode == TB_ENDTRACK) {
        KillTimer(IDT_REFRESH_TIMER);
        SetTimer(IDT_REFRESH_TIMER, m_nUpdateInterval, nullptr);
    }

    return 0;
}

// CFrameWindowImpl calls this through the CRTP pointer after it has resized the
// rebar and the status bar. Laying the status bar children out from WM_SIZE
// instead read the *previous* status bar width, so the slider visibly lagged a
// frame behind during a drag-resize.
void CMainFrame::UpdateLayout(BOOL bResizeBars)
{
    WTL::CFrameWindowImpl<CMainFrame>::UpdateLayout(bResizeBars);
    LayoutStatusBar();
}

// Split the status bar into fixed-width parts with real dividers instead of
// printing one long string into a single pane. The rightmost part is left blank
// and holds the refresh label and slider as child windows.
void CMainFrame::LayoutStatusBar()
{
    if (m_hWndStatusBar == nullptr || !::IsWindow(m_hWndStatusBar)) {
        return;
    }

    RECT rcStatusBar = {};
    ::GetClientRect(m_hWndStatusBar, &rcStatusBar);
    if (rcStatusBar.right <= 0) {
        return;
    }

    const int dpi = netwatch::util::DpiForWindow(m_hWnd);
    const auto dp = [dpi](int px) { return netwatch::util::ScaleForDpi(px, dpi); };

    const int gripWidth = dp(16);
    const int sliderWidth = dp(120);
    const int labelWidth = dp(78);
    const int gap = dp(6);
    const int countWidth = dp(104);
    const int sliderPartWidth = labelWidth + gap + sliderWidth + gap;

    int right = rcStatusBar.right - gripWidth;

    int edges[kStatusPartCount] = {};
    edges[kStatusPartSlider] = right;
    right -= sliderPartWidth;
    edges[kStatusPartTotal] = right;
    right -= countWidth;
    edges[kStatusPartListening] = right;
    right -= countWidth;
    edges[kStatusPartEndpoints] = right;
    right -= countWidth;
    edges[kStatusPartConnections] = right;
    right -= countWidth;
    edges[kStatusPartMessage] = right;

    // A very narrow window can push the fixed parts past the left edge. Collapse
    // them in place rather than letting the edges go negative and invert.
    for (int i = 0; i < kStatusPartCount; ++i) {
        if (edges[i] < 0) {
            edges[i] = 0;
        }
    }

    ::SendMessage(m_hWndStatusBar, SB_SETPARTS, kStatusPartCount,
        reinterpret_cast<LPARAM>(edges));

    if (!m_updateFreqLabel.IsWindow() || !m_updateFreqSlider.IsWindow()) {
        return;
    }

    const int controlHeight = dp(kStatusBarControlHeight);
    const int top = (rcStatusBar.bottom - controlHeight) / 2;
    const int labelLeft = edges[kStatusPartTotal];
    const int sliderLeft = labelLeft + labelWidth + gap;

    m_updateFreqLabel.SetWindowPos(nullptr, labelLeft, top, labelWidth, controlHeight,
        SWP_NOZORDER | SWP_NOACTIVATE);
    m_updateFreqSlider.SetWindowPos(nullptr, sliderLeft, top, sliderWidth, controlHeight,
        SWP_NOZORDER | SWP_NOACTIVATE);
}

// Windows hands us the target rectangle for the new scale factor. Take it, then
// re-run the layout so every scaled constant is recomputed against the new DPI.
LRESULT CMainFrame::OnDpiChanged(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM lParam, BOOL& /*bHandled*/)
{
    const RECT* prc = reinterpret_cast<const RECT*>(lParam);
    if (prc != nullptr) {
        SetWindowPos(nullptr, prc->left, prc->top,
            prc->right - prc->left, prc->bottom - prc->top,
            SWP_NOZORDER | SWP_NOACTIVATE);
    }

    if (HFONT hStatusFont = reinterpret_cast<HFONT>(
            ::SendMessage(m_hWndStatusBar, WM_GETFONT, 0, 0))) {
        m_updateFreqLabel.SetFont(hStatusFont, FALSE);
    }

    ApplyToolbarImages(m_hWndToolBarCtrl);
    m_view.OnDpiChanged();
    UpdateLayout();
    UpdateStatusBar();
    return 0;
}
