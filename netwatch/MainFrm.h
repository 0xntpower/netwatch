#pragma once

#include <string>

#define WM_REFRESH_COMPLETE (WM_USER + 100)

class CMainFrame : 
    public WTL::CFrameWindowImpl<CMainFrame>, 
    public WTL::CUpdateUI<CMainFrame>,
    public WTL::CMessageFilter, 
    public WTL::CIdleHandler
{
public:
    DECLARE_FRAME_WND_CLASS(nullptr, IDR_MAINFRAME)

    CConnectionListView m_view;
    WTL::CCommandBarCtrl m_CmdBar;
    WTL::CTrackBarCtrl m_updateFreqSlider;
    WTL::CStatic m_updateFreqLabel;

    // Owned by us, unlike the shell image list the connection list borrows.
    HIMAGELIST m_toolbarImages = nullptr;

    // CFrameWindowImpl::m_hWndToolBar holds the rebar once one exists, so the
    // real toolbar control is tracked separately.
    HWND m_hWndToolBarCtrl = nullptr;

    bool m_bAlwaysOnTop = false;
    bool m_bShowUnconnected = true;
    bool m_bPaused = false;

    static constexpr int kDefaultUpdateIntervalMs = 2000;
    static constexpr int kStatusBarControlHeight = 18;
    static constexpr int kSliderMin = 5;
    static constexpr int kSliderMax = 100;

    // Status bar parts, left to right. Part 0 stretches, the rest are fixed.
    enum StatusPart {
        kStatusPartMessage = 0,
        kStatusPartConnections,
        kStatusPartEndpoints,
        kStatusPartListening,
        kStatusPartTotal,
        kStatusPartSlider,
        kStatusPartCount
    };

    int m_nUpdateInterval = kDefaultUpdateIntervalMs;
    std::string m_processFilter;

    void SetProcessFilter(const std::string& filter) { m_processFilter = filter; }

    virtual BOOL PreTranslateMessage(MSG* pMsg);
    virtual BOOL OnIdle();

    // Overrides CFrameWindowImpl through the CRTP pointer so the status bar
    // children are positioned after the bars have been resized.
    void UpdateLayout(BOOL bResizeBars = TRUE);

    BEGIN_UPDATE_UI_MAP(CMainFrame)
        UPDATE_ELEMENT(ID_VIEW_TOOLBAR, UPDUI_MENUPOPUP)
        UPDATE_ELEMENT(ID_VIEW_STATUS_BAR, UPDUI_MENUPOPUP)
        UPDATE_ELEMENT(ID_FILE_PAUSE, UPDUI_MENUPOPUP | UPDUI_TOOLBAR)
        UPDATE_ELEMENT(ID_OPTIONS_ALWAYSONTOP, UPDUI_MENUPOPUP)
        UPDATE_ELEMENT(ID_OPTIONS_SHOWUNCONNECTED, UPDUI_MENUPOPUP)
        UPDATE_ELEMENT(ID_PROCESS_ENDPROCESS, UPDUI_MENUPOPUP | UPDUI_TOOLBAR)
        UPDATE_ELEMENT(ID_PROCESS_CLOSECONNECTION, UPDUI_MENUPOPUP)
        UPDATE_ELEMENT(ID_PROCESS_PROPERTIES, UPDUI_MENUPOPUP)
        UPDATE_ELEMENT(ID_PROCESS_WHOIS, UPDUI_MENUPOPUP)
    END_UPDATE_UI_MAP()

    BEGIN_MSG_MAP(CMainFrame)
        MESSAGE_HANDLER(WM_CREATE, OnCreate)
        MESSAGE_HANDLER(WM_DESTROY, OnDestroy)
        MESSAGE_HANDLER(WM_TIMER, OnTimer)
        MESSAGE_HANDLER(WM_HSCROLL, OnHScroll)
        MESSAGE_HANDLER(WM_DPICHANGED, OnDpiChanged)
        MESSAGE_HANDLER(WM_REFRESH_COMPLETE, OnRefreshComplete)
        COMMAND_ID_HANDLER(ID_APP_EXIT, OnFileExit)
        COMMAND_ID_HANDLER(ID_FILE_REFRESH, OnFileRefresh)
        COMMAND_ID_HANDLER(ID_FILE_PAUSE, OnFilePause)
        COMMAND_ID_HANDLER(ID_VIEW_TOOLBAR, OnViewToolBar)
        COMMAND_ID_HANDLER(ID_VIEW_STATUS_BAR, OnViewStatusBar)
        COMMAND_ID_HANDLER(ID_VIEW_COLUMNS, OnViewColumns)
        COMMAND_ID_HANDLER(ID_EDIT_SELECT_ALL, OnEditSelectAll)
        COMMAND_ID_HANDLER(ID_OPTIONS_ALWAYSONTOP, OnOptionsAlwaysOnTop)
        COMMAND_ID_HANDLER(ID_OPTIONS_SHOWUNCONNECTED, OnOptionsShowUnconnected)
        COMMAND_ID_HANDLER(ID_OPTIONS_FILTER, OnOptionsFilter)
        COMMAND_ID_HANDLER(ID_EDIT_COPY, OnEditCopy)
        COMMAND_ID_HANDLER(ID_PROCESS_ENDPROCESS, OnProcessEnd)
        COMMAND_ID_HANDLER(ID_PROCESS_CLOSECONNECTION, OnProcessCloseConnection)
        COMMAND_ID_HANDLER(ID_PROCESS_PROPERTIES, OnProcessProperties)
        COMMAND_ID_HANDLER(ID_PROCESS_WHOIS, OnProcessWhois)
        COMMAND_ID_HANDLER(ID_APP_HELP_CONTENTS, OnAppHelpContents)
        COMMAND_ID_HANDLER(ID_APP_ABOUT, OnAppAbout)
        NOTIFY_CODE_HANDLER(LVN_ITEMCHANGED, OnListViewItemChanged)
        CHAIN_MSG_MAP(WTL::CUpdateUI<CMainFrame>)
        CHAIN_MSG_MAP(WTL::CFrameWindowImpl<CMainFrame>)
        REFLECT_NOTIFICATIONS()
    END_MSG_MAP()

    // Message handlers
    LRESULT OnCreate(UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL& bHandled);
    LRESULT OnDestroy(UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL& bHandled);
    LRESULT OnTimer(UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL& bHandled);
    LRESULT OnHScroll(UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL& bHandled);
    LRESULT OnDpiChanged(UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL& bHandled);
    LRESULT OnRefreshComplete(UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL& bHandled);
    
    // File menu handlers
    LRESULT OnFileExit(WORD wNotifyCode, WORD wID, HWND hWndCtl, BOOL& bHandled);
    LRESULT OnFileRefresh(WORD wNotifyCode, WORD wID, HWND hWndCtl, BOOL& bHandled);
    LRESULT OnFilePause(WORD wNotifyCode, WORD wID, HWND hWndCtl, BOOL& bHandled);
    
    // View menu handlers
    LRESULT OnViewToolBar(WORD wNotifyCode, WORD wID, HWND hWndCtl, BOOL& bHandled);
    LRESULT OnViewStatusBar(WORD wNotifyCode, WORD wID, HWND hWndCtl, BOOL& bHandled);
    
    // Options menu handlers
    LRESULT OnOptionsAlwaysOnTop(WORD wNotifyCode, WORD wID, HWND hWndCtl, BOOL& bHandled);
    LRESULT OnOptionsShowUnconnected(WORD wNotifyCode, WORD wID, HWND hWndCtl, BOOL& bHandled);
    LRESULT OnOptionsFilter(WORD wNotifyCode, WORD wID, HWND hWndCtl, BOOL& bHandled);
    
    LRESULT OnViewColumns(WORD wNotifyCode, WORD wID, HWND hWndCtl, BOOL& bHandled);

    // Edit menu handlers
    LRESULT OnEditCopy(WORD wNotifyCode, WORD wID, HWND hWndCtl, BOOL& bHandled);
    LRESULT OnEditSelectAll(WORD wNotifyCode, WORD wID, HWND hWndCtl, BOOL& bHandled);

    // Process menu handlers
    LRESULT OnProcessEnd(WORD wNotifyCode, WORD wID, HWND hWndCtl, BOOL& bHandled);
    LRESULT OnProcessCloseConnection(WORD wNotifyCode, WORD wID, HWND hWndCtl, BOOL& bHandled);
    LRESULT OnProcessProperties(WORD wNotifyCode, WORD wID, HWND hWndCtl, BOOL& bHandled);
    LRESULT OnProcessWhois(WORD wNotifyCode, WORD wID, HWND hWndCtl, BOOL& bHandled);
    
    // Help menu handlers
    LRESULT OnAppHelpContents(WORD wNotifyCode, WORD wID, HWND hWndCtl, BOOL& bHandled);
    LRESULT OnAppAbout(WORD wNotifyCode, WORD wID, HWND hWndCtl, BOOL& bHandled);
    
    // Notification handlers
    LRESULT OnListViewItemChanged(int idCtrl, LPNMHDR pnmh, BOOL& bHandled);

    // Helper methods
    void UpdateStatusBar();
    void UpdateStatusMessage();
    void UpdateUIState();
    void LayoutStatusBar();
    void SetStatusPart(int part, LPCTSTR text);
    void UpdateRefreshLabel();

    HWND CreateToolBarCtrl();
    HIMAGELIST LoadToolbarImages(int& sizeOut) const;
    void ApplyToolbarImages(HWND hWndToolBar);

    // Persisted user state.
    void LoadSettings();
    void SaveSettings();
    void RestoreWindowPlacement();

    bool GetSelectedEntry(netwatch::util::EndpointEntry& entry);

    static int SliderPosToInterval(int pos);
    static int IntervalToSliderPos(int intervalMs);
    static bool IsRoutableRemote(const std::string& address);
};
