#include "stdafx.h"
#include "resource.h"
#include "View.h"
#include "net/TcpEnumerator.h"
#include "net/UdpEnumerator.h"
#include "util/StringConversion.h"
#include "util/Error.h"

#include <algorithm>
#include <iterator>
#include <format>
#include <process.h>

const CConnectionListView::ColumnInfo CConnectionListView::kColumnInfo[COL_COUNT] = {
    { L"Process Name", LVCFMT_LEFT, 150 },
    { L"PID", LVCFMT_RIGHT, 60 },
    { L"Protocol", LVCFMT_LEFT, 70 },
    { L"Integrity", LVCFMT_LEFT, 80 },
    { L"Local Address", LVCFMT_LEFT, 120 },
    { L"LPort", LVCFMT_RIGHT, 55 },
    { L"Remote Address", LVCFMT_LEFT, 120 },
    { L"RPort", LVCFMT_RIGHT, 55 },
    { L"State", LVCFMT_LEFT, 90 },
    { L"Arch", LVCFMT_LEFT, 45 },
    { L"DEP/NX", LVCFMT_LEFT, 65 },
    { L"ASLR", LVCFMT_LEFT, 65 },
    { L"Executable Path", LVCFMT_LEFT, 250 },
    { L"CFG", LVCFMT_LEFT, 65 },
    { L"SafeSEH", LVCFMT_LEFT, 65 },
    { L"Bytes Sent", LVCFMT_RIGHT, 100 },
    { L"Bytes Received", LVCFMT_RIGHT, 100 }
};

CConnectionListView::~CConnectionListView()
{
    shutdownRequested_ = true;

    if (hWorkerThread_ != NULL)
    {
        DWORD dwWaitResult = ::WaitForSingleObject(hWorkerThread_, 5000);

        if (dwWaitResult == WAIT_TIMEOUT)
        {
            ::TerminateThread(hWorkerThread_, 1);
        }

        ::CloseHandle(hWorkerThread_);
        hWorkerThread_ = NULL;
    }
}

BOOL CConnectionListView::PreTranslateMessage(MSG* pMsg)
{
    return FALSE;
}

LRESULT CConnectionListView::OnCreate(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& bHandled)
{
    bHandled = FALSE;
    return 0;
}

void CConnectionListView::InitColumns()
{
    // Note: LVS_OWNERDATA style is set at creation time in MainFrm.cpp
    // This makes the ListView "virtual" - it doesn't store items,
    // it asks us for data via LVN_GETDISPINFO when it needs to paint

    SetExtendedListViewStyle(
        LVS_EX_FULLROWSELECT |
        LVS_EX_GRIDLINES |
        LVS_EX_HEADERDRAGDROP |
        LVS_EX_DOUBLEBUFFER
    );

    // Initialize column visibility (all visible except bytes columns)
    for (int i = 0; i < COL_COUNT; ++i) {
        if (i == COL_BYTES_SENT || i == COL_BYTES_RCVD) {
            columnVisible_[i] = false;
        } else {
            columnVisible_[i] = true;
        }
    }

    // Insert only visible columns
    for (int i = 0; i < COL_COUNT; ++i) {
        if (columnVisible_[i]) {
            InsertColumn(i, kColumnInfo[i].name, kColumnInfo[i].format, kColumnInfo[i].defaultWidth);
        }
    }
}

void CConnectionListView::RefreshConnections() {
    if (shutdownRequested_) {
        return;
    }

    if (refreshInProgress_.exchange(true)) {
        return;
    }

    if (hWorkerThread_ != NULL)
    {
        ::CloseHandle(hWorkerThread_);
        hWorkerThread_ = NULL;
    }

    hWorkerThread_ = (HANDLE)::_beginthreadex(
        nullptr,
        0,
        [](void* pParam) -> unsigned int {
            try {
                CConnectionListView* pThis = static_cast<CConnectionListView*>(pParam);
                pThis->EnumerateInBackground();
                return 0;
            }
            catch (const std::exception& e) {
                netwatch::util::LogError("Background enumeration thread failed", e);
                return ERROR_INTERNAL_ERROR;
            }
            catch (...) {
                netwatch::util::LogError("Unknown exception in background enumeration thread");
                return ERROR_INTERNAL_ERROR;
            }
        },
        this,
        0,
        nullptr
    );

    if (hWorkerThread_ == NULL)
    {
        refreshInProgress_ = false;
    }
}

void CConnectionListView::EnumerateInBackground() {
    if (shutdownRequested_) {
        refreshInProgress_ = false;
        return;
    }

    // Capture filter settings at start of enumeration for thread safety
    // This ensures consistent filtering even if settings change during enumeration
    std::string currentFilter;
    bool currentShowUnconnected;
    {
        std::lock_guard<std::mutex> lock(pendingEntriesMutex_);
        currentFilter = processFilter_;
        currentShowUnconnected = showUnconnected_;
    }

    auto tcpEntries = netwatch::net::TcpEnumerator::Enumerate();
    auto udpEntries = netwatch::net::UdpEnumerator::Enumerate();

    if (shutdownRequested_) {
        refreshInProgress_ = false;
        return;
    }

    std::vector<netwatch::util::EndpointEntry> allEntries;
    allEntries.reserve(tcpEntries.size() + udpEntries.size());
    allEntries.insert(allEntries.end(),
        std::make_move_iterator(tcpEntries.begin()),
        std::make_move_iterator(tcpEntries.end()));
    allEntries.insert(allEntries.end(),
        std::make_move_iterator(udpEntries.begin()),
        std::make_move_iterator(udpEntries.end()));

    allEntries.erase(
        std::remove_if(allEntries.begin(), allEntries.end(),
            [&currentFilter, currentShowUnconnected](const auto& entry) {
                // Filter by process name if filter is set
                if (!currentFilter.empty()) {
                    std::string processNameLower = entry.processName;
                    std::string filterLower = currentFilter;
                    std::transform(processNameLower.begin(), processNameLower.end(), processNameLower.begin(), ::tolower);
                    std::transform(filterLower.begin(), filterLower.end(), filterLower.begin(), ::tolower);
                    if (processNameLower.find(filterLower) == std::string::npos) {
                        return true;  // Remove: doesn't match filter
                    }
                }
                // Filter unconnected endpoints if setting is disabled
                if (!currentShowUnconnected) {
                    if (entry.state == "LISTENING") {
                        return true;  // Remove LISTENING sockets
                    }
                    if (entry.protocol == "UDP" || entry.protocol == "UDPv6") {
                        if (entry.remoteAddress == "0.0.0.0" || entry.remoteAddress.empty()) {
                            return true;  // Remove UDP endpoints without remote
                        }
                    }
                }
                return false;  // Keep this entry
            }),
        allEntries.end());

    std::sort(allEntries.begin(), allEntries.end(), [](const auto& a, const auto& b) {
        if (a.pid != b.pid) return a.pid < b.pid;
        return a.protocol < b.protocol;
    });

    {
        std::lock_guard<std::mutex> lock(pendingEntriesMutex_);
        pendingEntries_ = std::move(allEntries);
    }

    if (m_hWnd && !shutdownRequested_) {
        ::PostMessage(m_hWnd, WM_REFRESH_COMPLETE, 0, 0);
    }
}

LRESULT CConnectionListView::OnRefreshComplete(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/)
{
    ApplyPendingEntries();

    int nConnections = 0;
    int nListening = 0;
    int nEndpoints = 0;

    for (const auto& entry : entries_) {
        if (entry.state == "ESTABLISHED") {
            nConnections++;
        } else if (entry.state == "LISTENING") {
            nListening++;
        } else {
            nEndpoints++;
        }
    }

    connectionStats_.totalConnections = nConnections;
    connectionStats_.totalListening = nListening;
    connectionStats_.totalEndpoints = nEndpoints;
    connectionStats_.totalItems = static_cast<int>(entries_.size());

    refreshInProgress_ = false;

    // Notify parent window (MainFrame) to update status bar
    ::PostMessage(::GetParent(m_hWnd), WM_REFRESH_COMPLETE, 0, 0);

    return 0;
}

void CConnectionListView::ApplyPendingEntries() {
    // Get new entries and sort them using current sort column
    std::vector<netwatch::util::EndpointEntry> newEntries;
    {
        std::lock_guard<std::mutex> lock(pendingEntriesMutex_);
        newEntries = std::move(pendingEntries_);
    }
    SortEntries(newEntries);

    // Save the identity of the currently selected item so we can re-select it
    // after the entries are replaced (the index alone is meaningless since rows shift)
    std::string selectedKey;
    int nSelected = GetNextItem(-1, LVNI_SELECTED);
    if (nSelected >= 0 && nSelected < static_cast<int>(entries_.size())) {
        selectedKey = MakeConnectionKey(entries_[nSelected]);
    }

    // Update entries
    entries_ = std::move(newEntries);

    // THIS IS THE KEY: Use ListView_SetItemCountEx with LVSICF_NOSCROLL
    // This tells the ListView how many items we have WITHOUT changing the scroll position
    // The ListView will call LVN_GETDISPINFO to get text when it needs to paint
    ListView_SetItemCountEx(m_hWnd, static_cast<int>(entries_.size()), LVSICF_NOSCROLL);

    // Restore selection to the same connection in the new entry list
    if (!selectedKey.empty()) {
        int newIndex = -1;
        for (int i = 0; i < static_cast<int>(entries_.size()); ++i) {
            if (MakeConnectionKey(entries_[i]) == selectedKey) {
                newIndex = i;
                break;
            }
        }

        if (newIndex >= 0) {
            // Deselect old index if it differs, then select the correct one
            if (newIndex != nSelected) {
                if (nSelected >= 0 && nSelected < static_cast<int>(entries_.size())) {
                    SetItemState(nSelected, 0, LVIS_SELECTED | LVIS_FOCUSED);
                }
                SetItemState(newIndex, LVIS_SELECTED | LVIS_FOCUSED, LVIS_SELECTED | LVIS_FOCUSED);
            }
        } else {
            // The previously selected connection no longer exists; clear stale selection
            if (nSelected >= 0 && nSelected < static_cast<int>(entries_.size())) {
                SetItemState(nSelected, 0, LVIS_SELECTED | LVIS_FOCUSED);
            }
        }
    }

    // Invalidate to redraw with new data (but scroll position is preserved!)
    Invalidate();
}

// LVN_GETDISPINFO handler - provides text for virtual ListView items on demand
LRESULT CConnectionListView::OnGetDispInfo(int /*idCtrl*/, LPNMHDR pnmh, BOOL& /*bHandled*/)
{
    NMLVDISPINFO* pDispInfo = reinterpret_cast<NMLVDISPINFO*>(pnmh);
    LVITEM* pItem = &pDispInfo->item;

    int row = pItem->iItem;
    int col = pItem->iSubItem;

    if (row < 0 || row >= static_cast<int>(entries_.size())) {
        return 0;
    }

    if (pItem->mask & LVIF_TEXT) {
        std::wstring text = GetCellText(row, col);
        // Copy text to the buffer provided by the ListView
        wcsncpy_s(pItem->pszText, pItem->cchTextMax, text.c_str(), _TRUNCATE);
    }

    return 0;
}

// LVN_ODFINDITEM handler - for keyboard navigation/search in virtual ListView
LRESULT CConnectionListView::OnOdFindItem(int /*idCtrl*/, LPNMHDR pnmh, BOOL& /*bHandled*/)
{
    NMLVFINDITEM* pFindInfo = reinterpret_cast<NMLVFINDITEM*>(pnmh);

    if (pFindInfo->lvfi.flags & LVFI_STRING) {
        std::wstring searchStr = pFindInfo->lvfi.psz;
        std::transform(searchStr.begin(), searchStr.end(), searchStr.begin(), ::towlower);

        int startPos = pFindInfo->iStart;
        if (startPos >= static_cast<int>(entries_.size())) {
            startPos = 0;
        }

        // Search from startPos to end, then from beginning to startPos
        for (int i = startPos; i < static_cast<int>(entries_.size()); ++i) {
            std::wstring processName = static_cast<LPCWSTR>(ATL::CA2W(entries_[i].processName.c_str()));
            std::transform(processName.begin(), processName.end(), processName.begin(), ::towlower);
            if (processName.find(searchStr) == 0) {
                return i;
            }
        }
        for (int i = 0; i < startPos; ++i) {
            std::wstring processName = static_cast<LPCWSTR>(ATL::CA2W(entries_[i].processName.c_str()));
            std::transform(processName.begin(), processName.end(), processName.begin(), ::towlower);
            if (processName.find(searchStr) == 0) {
                return i;
            }
        }
    }

    return -1; // Not found
}

// Helper to get text for a specific cell
std::wstring CConnectionListView::GetCellText(int row, int column) const {
    if (row < 0 || row >= static_cast<int>(entries_.size())) {
        return L"";
    }

    const auto& entry = entries_[row];

    switch (column) {
        case COL_PROCESS:
            return static_cast<LPCWSTR>(ATL::CA2W(entry.processName.c_str()));
        case COL_PID:
            return std::to_wstring(entry.pid);
        case COL_PROTOCOL:
            return static_cast<LPCWSTR>(ATL::CA2W(entry.protocol.c_str()));
        case COL_INTEGRITY:
            return static_cast<LPCWSTR>(ATL::CA2W(entry.integrityLevel.c_str()));
        case COL_LOCAL_ADDRESS:
            return static_cast<LPCWSTR>(ATL::CA2W(entry.localAddress.c_str()));
        case COL_LOCAL_PORT:
            return std::to_wstring(entry.localPort);
        case COL_REMOTE_ADDRESS:
            return static_cast<LPCWSTR>(ATL::CA2W(entry.remoteAddress.c_str()));
        case COL_REMOTE_PORT:
            return std::to_wstring(entry.remotePort);
        case COL_STATE:
            return static_cast<LPCWSTR>(ATL::CA2W(entry.state.c_str()));
        case COL_ARCHITECTURE:
            return static_cast<LPCWSTR>(ATL::CA2W(entry.architecture.c_str()));
        case COL_DEP_STATUS:
            return static_cast<LPCWSTR>(ATL::CA2W(entry.depStatus.c_str()));
        case COL_ASLR_STATUS:
            return static_cast<LPCWSTR>(ATL::CA2W(entry.aslrStatus.c_str()));
        case COL_EXECUTABLE_PATH:
            return static_cast<LPCWSTR>(ATL::CA2W(entry.executablePath.c_str()));
        case COL_CFG_STATUS:
            return static_cast<LPCWSTR>(ATL::CA2W(entry.cfgStatus.c_str()));
        case COL_SAFESEH_STATUS:
            return static_cast<LPCWSTR>(ATL::CA2W(entry.safeSehStatus.c_str()));
        case COL_BYTES_SENT:
            return static_cast<LPCWSTR>(ATL::CA2W(FormatNumber(entry.stats.sentBytes).c_str()));
        case COL_BYTES_RCVD:
            return static_cast<LPCWSTR>(ATL::CA2W(FormatNumber(entry.stats.rcvdBytes).c_str()));
        default:
            return L"";
    }
}

bool CConnectionListView::MatchesFilter(const netwatch::util::EndpointEntry& entry) const {
    if (processFilter_.empty()) {
        return true;
    }

    std::string processNameLower = entry.processName;
    std::string filterLower = processFilter_;

    std::transform(processNameLower.begin(), processNameLower.end(), processNameLower.begin(), ::tolower);
    std::transform(filterLower.begin(), filterLower.end(), filterLower.begin(), ::tolower);

    return processNameLower.find(filterLower) != std::string::npos;
}

bool CConnectionListView::ShouldShowEntry(const netwatch::util::EndpointEntry& entry) const {
    if (showUnconnected_) {
        return true;
    }

    if (entry.state == "LISTENING") {
        return false;
    }

    if (entry.protocol == "UDP" || entry.protocol == "UDPv6") {
        if (entry.remoteAddress == "0.0.0.0" || entry.remoteAddress.empty()) {
            return false;
        }
    }

    return true;
}

void CConnectionListView::ClearAllConnections() {
    entries_.clear();
    ListView_SetItemCountEx(m_hWnd, 0, 0);
}

std::string CConnectionListView::FormatNumber(uint64_t value) {
    return std::format("{:L}", value);
}

std::string CConnectionListView::MakeConnectionKey(const netwatch::util::EndpointEntry& entry) {
    return std::format("{}:{}:{}:{}:{}:{}",
        entry.pid,
        entry.protocol,
        entry.localAddress,
        entry.localPort,
        entry.remoteAddress,
        entry.remotePort);
}

void CConnectionListView::SortEntries(std::vector<netwatch::util::EndpointEntry>& entries) const {
    if (m_nSortColumn < 0) {
        // Default sort by PID, then protocol
        std::sort(entries.begin(), entries.end(), [](const auto& a, const auto& b) {
            if (a.pid != b.pid) return a.pid < b.pid;
            return a.protocol < b.protocol;
        });
        return;
    }

    std::sort(entries.begin(), entries.end(), [this](const auto& a, const auto& b) {
        int comparison = 0;

        switch (m_nSortColumn) {
            case COL_PID:
                comparison = (a.pid < b.pid) ? -1 : (a.pid > b.pid) ? 1 : 0;
                break;
            case COL_LOCAL_PORT:
                comparison = (a.localPort < b.localPort) ? -1 : (a.localPort > b.localPort) ? 1 : 0;
                break;
            case COL_REMOTE_PORT:
                comparison = (a.remotePort < b.remotePort) ? -1 : (a.remotePort > b.remotePort) ? 1 : 0;
                break;
            case COL_BYTES_SENT:
                comparison = (a.stats.sentBytes < b.stats.sentBytes) ? -1 : (a.stats.sentBytes > b.stats.sentBytes) ? 1 : 0;
                break;
            case COL_BYTES_RCVD:
                comparison = (a.stats.rcvdBytes < b.stats.rcvdBytes) ? -1 : (a.stats.rcvdBytes > b.stats.rcvdBytes) ? 1 : 0;
                break;
            case COL_PROCESS:
                comparison = a.processName.compare(b.processName);
                break;
            case COL_PROTOCOL:
                comparison = a.protocol.compare(b.protocol);
                break;
            case COL_INTEGRITY:
                comparison = a.integrityLevel.compare(b.integrityLevel);
                break;
            case COL_LOCAL_ADDRESS:
                comparison = a.localAddress.compare(b.localAddress);
                break;
            case COL_REMOTE_ADDRESS:
                comparison = a.remoteAddress.compare(b.remoteAddress);
                break;
            case COL_STATE:
                comparison = a.state.compare(b.state);
                break;
            case COL_ARCHITECTURE:
                comparison = a.architecture.compare(b.architecture);
                break;
            case COL_DEP_STATUS:
                comparison = a.depStatus.compare(b.depStatus);
                break;
            case COL_ASLR_STATUS:
                comparison = a.aslrStatus.compare(b.aslrStatus);
                break;
            case COL_CFG_STATUS:
                comparison = a.cfgStatus.compare(b.cfgStatus);
                break;
            case COL_SAFESEH_STATUS:
                comparison = a.safeSehStatus.compare(b.safeSehStatus);
                break;
            case COL_EXECUTABLE_PATH:
                comparison = a.executablePath.compare(b.executablePath);
                break;
            default:
                comparison = 0;
                break;
        }

        return m_bSortAscending ? (comparison < 0) : (comparison > 0);
    });
}

LRESULT CConnectionListView::OnContextMenu(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM lParam, BOOL& /*bHandled*/)
{
    POINT pt = { GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) };

    if (pt.x != -1 && pt.y != -1)
    {
        HWND hWndFromPoint = ::WindowFromPoint(pt);
        WTL::CHeaderCtrl header = GetHeader();
        if (hWndFromPoint == header.m_hWnd)
        {
            return 0;
        }
    }

    // If coordinates are -1, -1 then the context menu was invoked via keyboard
    if (pt.x == -1 && pt.y == -1)
    {
        // Get the position of the selected item
        int nSelected = GetNextItem(-1, LVNI_SELECTED);
        if (nSelected >= 0)
        {
            RECT rc;
            GetItemRect(nSelected, &rc, LVIR_BOUNDS);
            pt.x = rc.left;
            pt.y = rc.bottom;
            ClientToScreen(&pt);
        }
        else
        {
            pt.x = pt.y = 0;
            ClientToScreen(&pt);
        }
    }

    // Load and display the context menu
    WTL::CMenu menu;
    menu.LoadMenu(IDR_CONTEXT_MENU);
    WTL::CMenuHandle popup = menu.GetSubMenu(0);

    // Enable/disable menu items based on selection and context
    int nSelected = GetNextItem(-1, LVNI_SELECTED);
    bool bHasSelection = (nSelected >= 0);

    popup.EnableMenuItem(ID_PROCESS_ENDPROCESS, bHasSelection ? MF_ENABLED : MF_GRAYED);
    popup.EnableMenuItem(ID_PROCESS_CLOSECONNECTION, bHasSelection ? MF_ENABLED : MF_GRAYED);
    popup.EnableMenuItem(ID_EDIT_COPY, bHasSelection ? MF_ENABLED : MF_GRAYED);
    popup.EnableMenuItem(ID_PROCESS_PROPERTIES, bHasSelection ? MF_ENABLED : MF_GRAYED);
    popup.EnableMenuItem(ID_PROCESS_WHOIS, bHasSelection ? MF_ENABLED : MF_GRAYED);

    // Show the context menu and send the selected command to the parent window (MainFrame)
    popup.TrackPopupMenu(TPM_LEFTALIGN | TPM_RIGHTBUTTON, pt.x, pt.y, GetParent());

    return 0;
}

LRESULT CConnectionListView::OnColumnClick(int /*idCtrl*/, LPNMHDR pnmh, BOOL& /*bHandled*/)
{
    LPNMLISTVIEW pnmv = reinterpret_cast<LPNMLISTVIEW>(pnmh);
    int nColumn = pnmv->iSubItem;

    // Toggle sort direction if clicking the same column
    if (m_nSortColumn == nColumn)
    {
        m_bSortAscending = !m_bSortAscending;
    }
    else
    {
        m_nSortColumn = nColumn;
        m_bSortAscending = true;
    }

    // Sort entries using the shared sort function
    SortEntries(entries_);

    // For virtual ListView, just invalidate to redraw with new sort order
    // LVSICF_NOSCROLL keeps the scroll position stable
    ListView_SetItemCountEx(m_hWnd, static_cast<int>(entries_.size()), LVSICF_NOSCROLL);
    Invalidate();

    return 0;
}

LRESULT CConnectionListView::OnDoubleClick(int /*idCtrl*/, LPNMHDR /*pnmh*/, BOOL& /*bHandled*/)
{
    int nSelected = GetNextItem(-1, LVNI_SELECTED);
    if (nSelected >= 0)
    {
        // Send command to parent window to show properties dialog
        ::SendMessage(GetParent(), WM_COMMAND, ID_PROCESS_PROPERTIES, 0);
    }

    return 0;
}

LRESULT CConnectionListView::OnCustomDraw(int /*idCtrl*/, LPNMHDR pnmh, BOOL& /*bHandled*/)
{
    // Custom draw handler for color-coding connections similar to TCPView
    // For virtual ListView, we get the display state from entries_ directly

    LPNMLVCUSTOMDRAW pLVCD = reinterpret_cast<LPNMLVCUSTOMDRAW>(pnmh);

    switch (pLVCD->nmcd.dwDrawStage)
    {
    case CDDS_PREPAINT:
        // Request item-specific notifications
        return CDRF_NOTIFYITEMDRAW;

    case CDDS_ITEMPREPAINT:
        {
            int row = static_cast<int>(pLVCD->nmcd.dwItemSpec);

            // Set default colors (black text on white background)
            pLVCD->clrText = RGB(0, 0, 0);
            pLVCD->clrTextBk = RGB(255, 255, 255);

            // Get display state from entries_ for virtual ListView
            if (row >= 0 && row < static_cast<int>(entries_.size())) {
                auto displayState = entries_[row].displayState;

                // Apply color coding based on display state
                switch (displayState) {
                case netwatch::util::EndpointEntry::DisplayState::New:
                    pLVCD->clrText = RGB(0, 128, 0);        // Green for new
                    break;
                case netwatch::util::EndpointEntry::DisplayState::Closed:
                    pLVCD->clrText = RGB(255, 0, 0);        // Red for closed
                    break;
                case netwatch::util::EndpointEntry::DisplayState::Modified:
                    pLVCD->clrTextBk = RGB(255, 255, 192);  // Yellow background for modified
                    break;
                case netwatch::util::EndpointEntry::DisplayState::Normal:
                default:
                    // Keep default black on white
                    break;
                }
            }

            return CDRF_NEWFONT;
        }

    default:
        return CDRF_DODEFAULT;
    }
}

void CConnectionListView::ShowColumn(int columnIndex, bool show) {
    if (columnIndex < 0 || columnIndex >= COL_COUNT) {
        return;
    }

    if (columnVisible_[columnIndex] == show) {
        return;
    }

    columnVisible_[columnIndex] = show;

    if (show) {
        // Find the correct position to insert the column
        int insertPos = 0;
        for (int i = 0; i < columnIndex; ++i) {
            if (columnVisible_[i]) {
                insertPos++;
            }
        }
        InsertColumn(insertPos, kColumnInfo[columnIndex].name,
            kColumnInfo[columnIndex].format, kColumnInfo[columnIndex].defaultWidth);
    } else {
        // Find the current position of the column
        int currentPos = 0;
        for (int i = 0; i < columnIndex; ++i) {
            if (columnVisible_[i]) {
                currentPos++;
            }
        }
        DeleteColumn(currentPos);
    }

    // For virtual ListView, just invalidate to redraw
    Invalidate();
}

bool CConnectionListView::IsColumnVisible(int columnIndex) const {
    if (columnIndex < 0 || columnIndex >= COL_COUNT) {
        return false;
    }
    return columnVisible_[columnIndex];
}

LRESULT CConnectionListView::OnNotify(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM lParam, BOOL& bHandled)
{
    LPNMHDR pnmh = reinterpret_cast<LPNMHDR>(lParam);

    // Get the header control
    WTL::CHeaderCtrl header = GetHeader();

    // Check if notification is from header control and it's a right-click
    if (pnmh->hwndFrom == header.m_hWnd && pnmh->code == NM_RCLICK) {
        // Get cursor position for menu
        POINT pt;
        ::GetCursorPos(&pt);

        // Create popup menu
        WTL::CMenu menu;
        menu.CreatePopupMenu();

        // Add all columns to the menu with checkmarks
        for (int i = 0; i < COL_COUNT; ++i) {
            UINT flags = MF_STRING | (columnVisible_[i] ? MF_CHECKED : MF_UNCHECKED);
            menu.AppendMenu(flags, i + 1, kColumnInfo[i].name);
        }

        // Show menu and get selection
        int selection = menu.TrackPopupMenu(TPM_LEFTALIGN | TPM_RIGHTBUTTON | TPM_RETURNCMD, pt.x, pt.y, m_hWnd);

        if (selection > 0) {
            int columnIndex = selection - 1;
            ShowColumn(columnIndex, !columnVisible_[columnIndex]);
        }

        return 0;
    }

    // Let other notifications fall through to default handlers
    bHandled = FALSE;
    return 0;
}
