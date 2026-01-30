// View.h : interface of the CConnectionListView class
//
// This view displays network connections in a ListView
/////////////////////////////////////////////////////////////////////////////

#pragma once

#include "util/Types.h"

#include <vector>
#include <string>
#include <mutex>
#include <atomic>
#include <unordered_map>

// Custom message for background refresh completion
#define WM_REFRESH_COMPLETE (WM_USER + 100)

// Column indices
enum ConnectionListColumns
{
    COL_PROCESS = 0,
    COL_PID,
    COL_PROTOCOL,
    COL_INTEGRITY,
    COL_LOCAL_ADDRESS,
    COL_LOCAL_PORT,
    COL_REMOTE_ADDRESS,
    COL_REMOTE_PORT,
    COL_STATE,
    COL_ARCHITECTURE,
    COL_DEP_STATUS,
    COL_ASLR_STATUS,
    COL_EXECUTABLE_PATH,
    COL_CFG_STATUS,
    COL_SAFESEH_STATUS,
    COL_BYTES_SENT,
    COL_BYTES_RCVD,
    COL_COUNT
};

class CConnectionListView : public ATL::CWindowImpl<CConnectionListView, WTL::CListViewCtrl>
{
public:
    DECLARE_WND_SUPERCLASS(nullptr, WC_LISTVIEW)

    // Statistics structure for status bar display
    struct ConnectionStats {
        int totalConnections = 0;  // ESTABLISHED TCP
        int totalListening = 0;    // LISTENING TCP
        int totalEndpoints = 0;    // Other states
        int totalItems = 0;        // Total items in view
    };

    CConnectionListView() = default;
    
    // Destructor to clean up background thread
    ~CConnectionListView();

    BOOL PreTranslateMessage(MSG* pMsg);

    // Initialize the list view columns
    void InitColumns();

    // Refresh connection data from the network monitoring core (async)
    void RefreshConnections();

    // Column visibility management
    void ShowColumn(int columnIndex, bool show);
    bool IsColumnVisible(int columnIndex) const;

    // Clear all connections
    void ClearAllConnections();

    // Set process filter (thread-safe)
    void SetFilter(const std::string& filter) {
        std::lock_guard<std::mutex> lock(pendingEntriesMutex_);
        processFilter_ = filter;
    }

    // Set whether to show unconnected endpoints (thread-safe)
    void SetShowUnconnected(bool show) {
        std::lock_guard<std::mutex> lock(pendingEntriesMutex_);
        showUnconnected_ = show;
    }

    // Get current connection statistics (for status bar)
    const ConnectionStats& GetStats() const { return connectionStats_; }
    
    BEGIN_MSG_MAP(CConnectionListView)
        MESSAGE_HANDLER(WM_CREATE, OnCreate)
        MESSAGE_HANDLER(WM_CONTEXTMENU, OnContextMenu)
        MESSAGE_HANDLER(WM_REFRESH_COMPLETE, OnRefreshComplete)
        MESSAGE_HANDLER(WM_NOTIFY, OnNotify)
        REFLECTED_NOTIFY_CODE_HANDLER(LVN_COLUMNCLICK, OnColumnClick)
        REFLECTED_NOTIFY_CODE_HANDLER(NM_DBLCLK, OnDoubleClick)
        REFLECTED_NOTIFY_CODE_HANDLER(NM_CUSTOMDRAW, OnCustomDraw)
        REFLECTED_NOTIFY_CODE_HANDLER(LVN_GETDISPINFO, OnGetDispInfo)
        REFLECTED_NOTIFY_CODE_HANDLER(LVN_ODFINDITEM, OnOdFindItem)
        DEFAULT_REFLECTION_HANDLER()
    END_MSG_MAP()

    LRESULT OnCreate(UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL& bHandled);
    LRESULT OnContextMenu(UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL& bHandled);
    LRESULT OnRefreshComplete(UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL& bHandled);
    LRESULT OnNotify(UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL& bHandled);
    LRESULT OnColumnClick(int idCtrl, LPNMHDR pnmh, BOOL& bHandled);
    LRESULT OnDoubleClick(int idCtrl, LPNMHDR pnmh, BOOL& bHandled);
    LRESULT OnCustomDraw(int idCtrl, LPNMHDR pnmh, BOOL& bHandled);
    LRESULT OnGetDispInfo(int idCtrl, LPNMHDR pnmh, BOOL& bHandled);
    LRESULT OnOdFindItem(int idCtrl, LPNMHDR pnmh, BOOL& bHandled);

private:
    // Helper methods
    std::wstring GetCellText(int row, int column) const;
    static std::string FormatNumber(uint64_t value);

    // Generate unique key for connection identification
    static std::string MakeConnectionKey(const netwatch::util::EndpointEntry& entry);

    // Background enumeration
    void EnumerateInBackground();
    void ApplyPendingEntries();

    // Sort entries using current sort column
    void SortEntries(std::vector<netwatch::util::EndpointEntry>& entries) const;

    // Sorting state
    int m_nSortColumn = -1;
    bool m_bSortAscending = true;

    // Current entries
    std::vector<netwatch::util::EndpointEntry> entries_;

    // Pending entries from background thread
    std::vector<netwatch::util::EndpointEntry> pendingEntries_;
    std::mutex pendingEntriesMutex_;

    // Worker thread management - WTL style
    HANDLE hWorkerThread_ = NULL;
    std::atomic<bool> shutdownRequested_{false};
    std::atomic<bool> refreshInProgress_{false};

    // Connection statistics (updated on refresh)
    ConnectionStats connectionStats_;

    // Process filter
    std::string processFilter_;

    // Show unconnected endpoints (LISTENING, UDP without remote)
    bool showUnconnected_ = true;

    // Column visibility state
    bool columnVisible_[COL_COUNT];

    // Column metadata
    struct ColumnInfo {
        const wchar_t* name;
        int format;
        int defaultWidth;
    };
    static const ColumnInfo kColumnInfo[COL_COUNT];

    // Check if entry matches filter
    bool MatchesFilter(const netwatch::util::EndpointEntry& entry) const;

    // Check if entry should be shown based on show unconnected setting
    bool ShouldShowEntry(const netwatch::util::EndpointEntry& entry) const;
};

// Typedef for backwards compatibility
typedef CConnectionListView CView;
