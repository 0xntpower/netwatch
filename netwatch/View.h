// View.h : interface of the CConnectionListView class
//
// This view displays network connections in a ListView
/////////////////////////////////////////////////////////////////////////////

#pragma once

#include "util/Types.h"
#include "system/ProcessCache.h"

#include <vector>
#include <string>
#include <mutex>
#include <atomic>
#include <unordered_map>
#include <unordered_set>

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

    // Column visibility management. Indices are logical (ConnectionListColumns),
    // never ListView subitem indices.
    void ShowColumn(int columnIndex, bool show);
    bool IsColumnVisible(int columnIndex) const;

    // Map between ListView subitem indices and logical column indices. These
    // diverge as soon as any column is hidden.
    int LogicalColumn(int visibleIndex) const;
    int VisibleColumn(int logicalIndex) const;

    // Column metadata, for the column chooser.
    static const wchar_t* ColumnName(int logicalIndex);
    static bool IsColumnVisibleByDefault(int logicalIndex);

    // Read a field straight out of the backing data. Command handlers must use
    // this rather than GetItemText, which takes a subitem index and would read
    // the wrong field whenever a column is hidden.
    bool GetEntry(int row, netwatch::util::EndpointEntry& out) const;

    // Tab-separated text for the selected rows, or every row when the selection
    // is empty. Uses the currently visible columns, in display order.
    std::wstring BuildClipboardText() const;

    // Re-scale default column widths after the window changes monitor.
    void OnDpiChanged();

    // Column visibility, widths and sort order, persisted between sessions.
    void SaveLayout() const;
    void LoadLayout();

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
        REFLECTED_NOTIFY_CODE_HANDLER(LVN_GETINFOTIP, OnGetInfoTip)
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
    LRESULT OnGetInfoTip(int idCtrl, LPNMHDR pnmh, BOOL& bHandled);

private:
    // Helper methods. 'column' is a logical ConnectionListColumns value.
    std::wstring GetCellText(int row, int column) const;
    static std::string FormatNumber(uint64_t value);

    // Rebuild the header and the visible-to-logical column map.
    void RebuildColumns();
    void UpdateSortIndicator();

    // Sampled once per paint cycle in CDDS_PREPAINT.
    static bool QueryHighContrast();
    bool highContrast_ = false;

    // Attach the shell's system image list once the first icon exists.
    void EnsureImageList();
    bool imageListAttached_ = false;

    // Generate unique key for connection identification
    static std::string MakeConnectionKey(const netwatch::util::EndpointEntry& entry);

    // Background enumeration
    void EnumerateInBackground();
    void ApplyPendingEntries();
    void RestoreSelection(const std::string& selectedKey, int previousIndex);

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

    // Per-PID metadata, reused across refreshes. Only touched on the worker
    // thread, and only one enumeration pass runs at a time.
    netwatch::system::ProcessCache processCache_;

    // Column visibility state, indexed by logical column.
    bool columnVisible_[COL_COUNT];

    // visibleToLogical_[subitemIndex] == logical column currently drawn there.
    std::vector<int> visibleToLogical_;

    // Columns the user has resized by hand. Those keep their width across a DPI
    // change instead of snapping back to the scaled default.
    bool userSizedColumn_[COL_COUNT] = {};

    // Widths restored from the previous session, normalised to 96 DPI. Empty
    // when there is nothing saved.
    std::vector<int> savedWidths_;

    // Column metadata
    struct ColumnInfo {
        const wchar_t* name;
        int format;
        int defaultWidth;
    };
    static const ColumnInfo kColumnInfo[COL_COUNT];

};

// Typedef for backwards compatibility
typedef CConnectionListView CView;
