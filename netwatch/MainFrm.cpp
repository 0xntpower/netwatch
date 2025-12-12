#include "stdafx.h"
#include "resource.h"

#include "aboutdlg.h"
#include "View.h"
#include "MainFrm.h"
#include "FilterDlg.h"
#include "PropertiesDialog.h"
#include "ProcessUtil.hpp"
#include "util/MessageBox.h"
#include "util/WindowsError.h"
#include "util/StringConversion.h"

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
    HWND hWndCmdBar = m_CmdBar.Create(m_hWnd, rcDefault, nullptr, ATL_SIMPLE_CMDBAR_PANE_STYLE);
    m_CmdBar.AttachMenu(GetMenu());
    SetMenu(nullptr);

    HWND hWndToolBar = CreateSimpleToolBarCtrl(m_hWnd, IDR_MAINFRAME, FALSE, ATL_SIMPLE_TOOLBAR_PANE_STYLE);

    CreateSimpleReBar(ATL_SIMPLE_REBAR_NOBORDER_STYLE | RBS_FIXEDORDER);
    AddSimpleReBarBand(hWndCmdBar);
    AddSimpleReBarBand(hWndToolBar, nullptr, TRUE);

    CreateSimpleStatusBar();

    m_hWndClient = m_view.Create(
        m_hWnd,
        rcDefault,
        nullptr,
        WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS | WS_CLIPCHILDREN |
        LVS_REPORT | LVS_SHOWSELALWAYS | LVS_SINGLESEL,
        WS_EX_CLIENTEDGE
    );

    m_view.InitColumns();

    UIAddToolBar(hWndToolBar);
    UISetCheck(ID_VIEW_TOOLBAR, 1);
    UISetCheck(ID_VIEW_STATUS_BAR, 1);

    WTL::CMessageLoop* pLoop = _Module.GetMessageLoop();
    ATLASSERT(pLoop != nullptr);
    pLoop->AddMessageFilter(this);
    pLoop->AddIdleHandler(this);

    RECT rcStatusBar;
    ::GetClientRect(m_hWndStatusBar, &rcStatusBar);

    int sliderWidth = 150;
    int labelWidth = 95;
    int rightMargin = 10;
    int controlHeight = kStatusBarControlHeight;
    int topMargin = 3;

    int sliderRight = rcStatusBar.right - rightMargin;
    int sliderLeft = sliderRight - sliderWidth;
    int labelRight = sliderLeft - 5;
    int labelLeft = labelRight - labelWidth;

    RECT rcLabel = { labelLeft, topMargin, labelRight, topMargin + controlHeight };
    m_updateFreqLabel.Create(m_hWndStatusBar, rcLabel, _T("Refresh:"),
        WS_CHILD | WS_VISIBLE | SS_RIGHT, 0, IDC_UPDATE_FREQ_LABEL);

    HFONT hFont = (HFONT)::GetStockObject(DEFAULT_GUI_FONT);
    m_updateFreqLabel.SetFont(hFont);

    RECT rcSlider = { sliderLeft, topMargin, sliderRight, topMargin + controlHeight };
    m_updateFreqSlider.Create(m_hWndStatusBar, rcSlider, nullptr,
        WS_CHILD | WS_VISIBLE | TBS_HORZ | TBS_NOTICKS | TBS_TOOLTIPS,
        0, IDC_UPDATE_FREQ_SLIDER);
    m_updateFreqSlider.SetRange(5, 100);
    m_updateFreqSlider.SetPos(20);

    SetTimer(IDT_REFRESH_TIMER, m_nUpdateInterval, nullptr);

    if (!m_processFilter.empty()) {
        m_view.SetFilter(m_processFilter);
    }

    m_view.RefreshConnections();
    UpdateStatusBar();

    return 0;
}

LRESULT CMainFrame::OnDestroy(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& bHandled)
{
    KillTimer(IDT_REFRESH_TIMER);

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
    
    // Update status bar to show paused state
    if (m_bPaused)
    {
        ::SetWindowText(m_hWndStatusBar, _T("Paused"));
    }
    else
    {
        UpdateStatusBar();
    }
    
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

        // Update window title to show filter
        if (!m_processFilter.empty()) {
            TCHAR szTitle[256];
            _stprintf_s(szTitle, _T("NetWatch - Filter: %S"), m_processFilter.c_str());
            SetWindowText(szTitle);
        }
        else {
            SetWindowText(_T("NetWatch"));
        }
    }

    return 0;
}

// Process menu handlers

LRESULT CMainFrame::OnProcessEnd(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
    int nSelected = m_view.GetNextItem(-1, LVNI_SELECTED);
    if (nSelected >= 0)
    {
        // Get PID from the selected item
        TCHAR szPid[32];
        m_view.GetItemText(nSelected, COL_PID, szPid, 32);
        DWORD pid = _ttoi(szPid);

        // Get process name for confirmation message
        TCHAR szProcess[MAX_PATH];
        m_view.GetItemText(nSelected, COL_PROCESS, szProcess, MAX_PATH);
        std::string processName = netwatch::util::StringConversion::WideToNarrow(szProcess);

        // Confirm with user before terminating (critical operation!)
        std::ostringstream oss;
        oss << "Are you sure you want to end process '" << processName << "' (PID " << pid << ")?\n\n"
            << "Warning: This will terminate the process immediately and may cause data loss.";

        if (netwatch::util::MessageBox::ShowConfirmWarning(m_hWnd, oss.str(), "End Process") == IDYES)
        {
            if (netwatch::util::TerminateTargetProcess(pid))
            {
                netwatch::util::MessageBox::ShowInfo(m_hWnd,
                    "Process terminated successfully.", "End Process");
            }
            else
            {
                DWORD errorCode = GetLastError();
                std::string errorMsg = netwatch::util::WindowsError::GetErrorMessage(errorCode);
                std::ostringstream errOss;
                errOss << "Failed to terminate process.\n\n"
                       << "Error: " << errorMsg;
                netwatch::util::MessageBox::ShowError(m_hWnd, errOss.str(), "End Process Failed");
            }
        }
    }

    return 0;
}

LRESULT CMainFrame::OnProcessCloseConnection(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
    int nSelected = m_view.GetNextItem(-1, LVNI_SELECTED);
    if (nSelected >= 0)
    {
        // Get protocol to ensure this is a TCP connection
        TCHAR szProtocol[16];
        m_view.GetItemText(nSelected, COL_PROTOCOL, szProtocol, 16);
        std::string protocol = netwatch::util::StringConversion::WideToNarrow(szProtocol);

        // Check if it's TCP (or TCPv6, but we only support IPv4 for now)
        if (protocol != "TCP" && protocol != "TCPv6")
        {
            netwatch::util::MessageBox::ShowWarning(m_hWnd,
                "Only TCP connections can be closed.\nUDP endpoints cannot be closed.",
                "Invalid Operation");
            return 0;
        }

        if (protocol == "TCPv6")
        {
            netwatch::util::MessageBox::ShowWarning(m_hWnd,
                "Closing IPv6 TCP connections is not currently supported.",
                "Not Supported");
            return 0;
        }

        // Get connection details
        TCHAR szLocalAddr[256], szRemoteAddr[256];
        TCHAR szLocalPort[32], szRemotePort[32];

        m_view.GetItemText(nSelected, COL_LOCAL_ADDRESS, szLocalAddr, 256);
        m_view.GetItemText(nSelected, COL_LOCAL_PORT, szLocalPort, 32);
        m_view.GetItemText(nSelected, COL_REMOTE_ADDRESS, szRemoteAddr, 256);
        m_view.GetItemText(nSelected, COL_REMOTE_PORT, szRemotePort, 32);

        // Check if it's a listening socket (no remote address)
        std::string remoteAddrCheck = netwatch::util::StringConversion::WideToNarrow(szRemoteAddr);
        if (remoteAddrCheck == "0.0.0.0" || remoteAddrCheck.empty())
        {
            netwatch::util::MessageBox::ShowWarning(m_hWnd,
                "Cannot close a listening socket.\nOnly established connections can be closed.",
                "Invalid Operation");
            return 0;
        }

        // Convert addresses and ports
        std::string localAddrStr = netwatch::util::StringConversion::WideToNarrow(szLocalAddr);
        std::string remoteAddrStr = netwatch::util::StringConversion::WideToNarrow(szRemoteAddr);

        DWORD localAddr = inet_addr(localAddrStr.c_str());
        DWORD remoteAddr = inet_addr(remoteAddrStr.c_str());
        DWORD localPort = htons(_ttoi(szLocalPort));
        DWORD remotePort = htons(_ttoi(szRemotePort));

        // Confirm with user
        std::ostringstream oss;
        oss << "Are you sure you want to close this TCP connection?\n\n"
            << localAddrStr << ":" << _ttoi(szLocalPort) << " <-> "
            << remoteAddrStr << ":" << _ttoi(szRemotePort);

        if (netwatch::util::MessageBox::ShowConfirmWarning(m_hWnd, oss.str(), "Close Connection") == IDYES)
        {
            std::string errorMessage;
            if (netwatch::util::CloseNetworkConnection(localAddr, localPort, remoteAddr, remotePort, errorMessage))
            {
                netwatch::util::MessageBox::ShowInfo(m_hWnd,
                    "Connection closed successfully.", "Close Connection");
            }
            else
            {
                std::ostringstream errOss;
                errOss << "Failed to close connection.\n\n"
                       << "Error: " << errorMessage;
                netwatch::util::MessageBox::ShowError(m_hWnd, errOss.str(), "Close Connection Failed");
            }
        }
    }

    return 0;
}

LRESULT CMainFrame::OnProcessProperties(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
    int nSelected = m_view.GetNextItem(-1, LVNI_SELECTED);
    if (nSelected >= 0)
    {
        // Helper lambda to get text from list view
        auto getText = [this, nSelected](int column) -> std::string {
            TCHAR buffer[512];
            m_view.GetItemText(nSelected, column, buffer, 512);
            return netwatch::util::StringConversion::WideToNarrow(buffer);
        };

        // Populate connection properties structure
        CPropertiesDialog::ConnectionProperties props;
        props.processName = getText(COL_PROCESS);
        props.pid = getText(COL_PID);
        props.architecture = getText(COL_ARCHITECTURE);
        props.integrityLevel = getText(COL_INTEGRITY);
        props.protocol = getText(COL_PROTOCOL);
        props.state = getText(COL_STATE);
        props.localAddress = getText(COL_LOCAL_ADDRESS);
        props.localPort = getText(COL_LOCAL_PORT);
        props.remoteAddress = getText(COL_REMOTE_ADDRESS);
        props.remotePort = getText(COL_REMOTE_PORT);
        props.executablePath = getText(COL_EXECUTABLE_PATH);
        props.depStatus = getText(COL_DEP_STATUS);
        props.aslrStatus = getText(COL_ASLR_STATUS);
        props.cfgStatus = getText(COL_CFG_STATUS);
        props.safeSehStatus = getText(COL_SAFESEH_STATUS);

        // Display the properties dialog
        CPropertiesDialog dlg(props);
        dlg.DoModal(m_hWnd);
    }

    return 0;
}

LRESULT CMainFrame::OnProcessWhois(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
    int nSelected = m_view.GetNextItem(-1, LVNI_SELECTED);
    if (nSelected >= 0)
    {
        // Get remote address from the selected connection
        TCHAR szRemoteAddr[256];
        m_view.GetItemText(nSelected, COL_REMOTE_ADDRESS, szRemoteAddr, 256);

        // Skip if no valid remote address
        std::string remoteCheck = netwatch::util::StringConversion::WideToNarrow(szRemoteAddr);
        if (remoteCheck == "0.0.0.0" || remoteCheck.empty())
        {
            netwatch::util::MessageBox::ShowInfo(m_hWnd,
                "No remote address available for WHOIS lookup.\n\n"
                "This connection is either listening or using UDP without an established remote endpoint.",
                "WHOIS Lookup");
            return 0;
        }

        // Check if it's localhost
        if (_tcsncmp(szRemoteAddr, _T("127."), 4) == 0 || _tcsncmp(szRemoteAddr, _T("::1"), 3) == 0)
        {
            netwatch::util::MessageBox::ShowInfo(m_hWnd,
                "WHOIS lookup is not available for localhost addresses.",
                "WHOIS Lookup");
            return 0;
        }

        // Convert to narrow string
        std::string remoteAddrStr = netwatch::util::StringConversion::WideToNarrow(szRemoteAddr);

        // Open browser to online WHOIS service
        std::ostringstream urlStream;
        urlStream << "https://www.whois.com/whois/" << remoteAddrStr;
        std::string url = urlStream.str();

        HINSTANCE result = ShellExecuteA(NULL, "open", url.c_str(), NULL, NULL, SW_SHOWNORMAL);
        if ((INT_PTR)result <= 32)
        {
            netwatch::util::MessageBox::ShowError(m_hWnd,
                "Failed to open web browser for WHOIS lookup.\n\n"
                "Please ensure you have a default web browser configured.",
                "WHOIS Lookup Failed");
        }
    }

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

void CMainFrame::UpdateStatusBar()
{
    // Get pre-calculated statistics from the view (efficient - no UI iteration!)
    const auto& stats = m_view.GetStats();

    // Format status bar text with all counts (TCPView-style)
    TCHAR szStatus[256];
    _stprintf_s(szStatus, _T("Connections: %d  |  Endpoints: %d  |  Listening: %d  |  Total: %d"),
        stats.totalConnections, stats.totalEndpoints, stats.totalListening, stats.totalItems);
    ::SetWindowText(m_hWndStatusBar, szStatus);
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
    // Check if the scroll message is from the update frequency slider
    if ((HWND)lParam == m_updateFreqSlider.m_hWnd)
    {
        // Only process scroll events (not initial positioning)
        WORD scrollCode = LOWORD(wParam);
        if (scrollCode == TB_THUMBPOSITION || scrollCode == TB_THUMBTRACK ||
            scrollCode == TB_LINEDOWN || scrollCode == TB_LINEUP ||
            scrollCode == TB_PAGEDOWN || scrollCode == TB_PAGEUP ||
            scrollCode == TB_ENDTRACK)
        {
            int nPos = m_updateFreqSlider.GetPos();

            // Non-linear scale: 5-100 -> 0.5s-10s
            if (nPos <= 10) {
                m_nUpdateInterval = nPos * 100;
            } else if (nPos <= 30) {
                m_nUpdateInterval = 1000 + (nPos - 10) * 100;
            } else if (nPos <= 50) {
                m_nUpdateInterval = 3000 + (nPos - 30) * 100;
            } else {
                m_nUpdateInterval = 5000 + (nPos - 50) * 100;
            }

            TCHAR szLabel[64];
            if (m_nUpdateInterval < 1000) {
                _stprintf_s(szLabel, _T("Refresh: %.1fs"), m_nUpdateInterval / 1000.0f);
            } else {
                int seconds = m_nUpdateInterval / 1000;
                int tenths = (m_nUpdateInterval % 1000) / 100;
                if (tenths == 0) {
                    _stprintf_s(szLabel, _T("Refresh: %ds"), seconds);
                } else {
                    _stprintf_s(szLabel, _T("Refresh: %d.%ds"), seconds, tenths);
                }
            }
            m_updateFreqLabel.SetWindowText(szLabel);
            m_updateFreqLabel.Invalidate();
            m_updateFreqLabel.UpdateWindow();

            if (scrollCode == TB_ENDTRACK) {
                KillTimer(IDT_REFRESH_TIMER);
                SetTimer(IDT_REFRESH_TIMER, m_nUpdateInterval, nullptr);
            }
        }
    }

    return 0;
}

LRESULT CMainFrame::OnSize(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& bHandled)
{
    bHandled = FALSE;

    if (m_updateFreqSlider.IsWindow() && m_updateFreqLabel.IsWindow())
    {
        RECT rcStatusBar;
        ::GetClientRect(m_hWndStatusBar, &rcStatusBar);

        int sliderWidth = 150;
        int labelWidth = 95;
        int rightMargin = 10;
        int controlHeight = kStatusBarControlHeight;
        int topMargin = 3;

        int sliderRight = rcStatusBar.right - rightMargin;
        int sliderLeft = sliderRight - sliderWidth;
        int labelRight = sliderLeft - 5;
        int labelLeft = labelRight - labelWidth;

        m_updateFreqLabel.SetWindowPos(nullptr, labelLeft, topMargin, labelWidth, controlHeight, SWP_NOZORDER);
        m_updateFreqSlider.SetWindowPos(nullptr, sliderLeft, topMargin, sliderWidth, controlHeight, SWP_NOZORDER);
    }

    return 0;
}
