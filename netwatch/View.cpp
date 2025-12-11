// View.cpp : implementation of the CConnectionListView class
//
// This view displays network connections in a ListView
/////////////////////////////////////////////////////////////////////////////

#include "stdafx.h"
#include "resource.h"
#include "View.h"
#include "net/TcpEnumerator.h"
#include "net/UdpEnumerator.h"
#include "util/StringConversion.h"

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
            CConnectionListView* pThis = static_cast<CConnectionListView*>(pParam);
            pThis->EnumerateInBackground();
            return 0;
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
            [this](const auto& entry) {
                if (!processFilter_.empty() && !MatchesFilter(entry)) {
                    return true;
                }
                if (!showUnconnected_ && !ShouldShowEntry(entry)) {
                    return true;
                }
                return false;
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
    SetRedraw(FALSE);

    int nSelectedItem = GetNextItem(-1, LVNI_SELECTED);
    DWORD selectedPid = 0;
    std::string selectedProtocol;
    std::string selectedLocalAddr;
    uint16_t selectedLocalPort = 0;
    std::string selectedRemoteAddr;
    uint16_t selectedRemotePort = 0;

    if (nSelectedItem >= 0) {
        TCHAR szBuffer[256];
        GetItemText(nSelectedItem, COL_PID, szBuffer, 256);
        selectedPid = _ttoi(szBuffer);

        GetItemText(nSelectedItem, COL_PROTOCOL, szBuffer, 256);
        selectedProtocol = netwatch::util::StringConversion::WideToNarrow(szBuffer);

        GetItemText(nSelectedItem, COL_LOCAL_ADDRESS, szBuffer, 256);
        selectedLocalAddr = netwatch::util::StringConversion::WideToNarrow(szBuffer);

        GetItemText(nSelectedItem, COL_LOCAL_PORT, szBuffer, 256);
        selectedLocalPort = static_cast<uint16_t>(_ttoi(szBuffer));

        GetItemText(nSelectedItem, COL_REMOTE_ADDRESS, szBuffer, 256);
        selectedRemoteAddr = netwatch::util::StringConversion::WideToNarrow(szBuffer);

        GetItemText(nSelectedItem, COL_REMOTE_PORT, szBuffer, 256);
        if (_tcscmp(szBuffer, _T("*")) != 0) {
            selectedRemotePort = static_cast<uint16_t>(_ttoi(szBuffer));
        }
    }

    int nTopIndex = GetTopIndex();

    {
        std::lock_guard<std::mutex> lock(pendingEntriesMutex_);
        entries_ = std::move(pendingEntries_);
    }

    ClearAllConnections();
    int newSelectedItem = -1;
    for (size_t i = 0; i < entries_.size(); ++i) {
        const auto& entry = entries_[i];
        AddEntry(entry);

        if (nSelectedItem >= 0 && entry.pid == selectedPid &&
            entry.protocol == selectedProtocol &&
            entry.localAddress == selectedLocalAddr &&
            entry.localPort == selectedLocalPort &&
            entry.remoteAddress == selectedRemoteAddr &&
            entry.remotePort == selectedRemotePort) {
            newSelectedItem = static_cast<int>(i);
        }
    }

    if (nTopIndex >= 0 && nTopIndex < GetItemCount()) {
        int currentTop = GetTopIndex();
        if (currentTop != nTopIndex) {
            RECT rcItem;
            GetItemRect(0, &rcItem, LVIR_BOUNDS);
            int itemHeight = rcItem.bottom - rcItem.top;
            int scrollAmount = (nTopIndex - currentTop) * itemHeight;
            SIZE sz = { 0, scrollAmount };
            Scroll(sz);
        }
    }

    if (newSelectedItem >= 0) {
        SetItemState(newSelectedItem, LVIS_SELECTED | LVIS_FOCUSED, LVIS_SELECTED | LVIS_FOCUSED);
    }

    SetRedraw(TRUE);
    Invalidate();
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
    DeleteAllItems();
}

void CConnectionListView::AddEntry(const netwatch::util::EndpointEntry& entry) {
    int nItem = InsertItem(GetItemCount(), ATL::CA2T(entry.processName.c_str()));

    SetItemText(nItem, COL_PID, ATL::CA2T(std::format("{}", entry.pid).c_str()));
    SetItemText(nItem, COL_PROTOCOL, ATL::CA2T(entry.protocol.c_str()));
    SetItemText(nItem, COL_INTEGRITY, ATL::CA2T(entry.integrityLevel.c_str()));
    SetItemText(nItem, COL_LOCAL_ADDRESS, ATL::CA2T(entry.localAddress.c_str()));
    SetItemText(nItem, COL_LOCAL_PORT, ATL::CA2T(std::format("{}", entry.localPort).c_str()));
    SetItemText(nItem, COL_REMOTE_ADDRESS, ATL::CA2T(entry.remoteAddress.c_str()));

    // Format remote port
    SetItemText(nItem, COL_REMOTE_PORT, ATL::CA2T(std::format("{}", entry.remotePort).c_str()));

    SetItemText(nItem, COL_STATE, ATL::CA2T(entry.state.c_str()));
    SetItemText(nItem, COL_ARCHITECTURE, ATL::CA2T(entry.architecture.c_str()));
    SetItemText(nItem, COL_DEP_STATUS, ATL::CA2T(entry.depStatus.c_str()));
    SetItemText(nItem, COL_ASLR_STATUS, ATL::CA2T(entry.aslrStatus.c_str()));
    SetItemText(nItem, COL_EXECUTABLE_PATH, ATL::CA2T(entry.executablePath.c_str()));
    SetItemText(nItem, COL_CFG_STATUS, ATL::CA2T(entry.cfgStatus.c_str()));
    SetItemText(nItem, COL_SAFESEH_STATUS, ATL::CA2T(entry.safeSehStatus.c_str()));
    SetItemText(nItem, COL_BYTES_SENT, ATL::CA2T(FormatNumber(entry.stats.sentBytes).c_str()));
    SetItemText(nItem, COL_BYTES_RCVD, ATL::CA2T(FormatNumber(entry.stats.rcvdBytes).c_str()));

    SetItemData(nItem, static_cast<DWORD_PTR>(entry.displayState));
}

std::string CConnectionListView::FormatNumber(uint64_t value) {
    if (value == 0) return "0";

    // Format with thousands separators
    std::string result = std::to_string(value);
    int insertPosition = static_cast<int>(result.length()) - 3;
    while (insertPosition > 0) {
        result.insert(insertPosition, ",");
        insertPosition -= 3;
    }
    return result;
}

LRESULT CConnectionListView::OnContextMenu(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM lParam, BOOL& /*bHandled*/)
{
    // Get the context menu position
    POINT pt = { GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) };

    // Check if the right-click was on the header control
    // If so, don't show the process context menu (header has its own menu)
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

    // Sort entries based on column type
    std::sort(entries_.begin(), entries_.end(), [this, nColumn](const auto& a, const auto& b) {
        int comparison = 0;

        switch (nColumn) {
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

    // Refresh display with sorted entries
    SetRedraw(FALSE);
    ClearAllConnections();
    for (const auto& entry : entries_) {
        AddEntry(entry);
    }
    SetRedraw(TRUE);
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
    // TCPView uses: Green for new connections, Red for closing/closed connections
    // Yellow/highlight background for recently modified connections

    LPNMLVCUSTOMDRAW pLVCD = reinterpret_cast<LPNMLVCUSTOMDRAW>(pnmh);

    switch (pLVCD->nmcd.dwDrawStage)
    {
    case CDDS_PREPAINT:
        // Request item-specific notifications
        return CDRF_NOTIFYITEMDRAW;

    case CDDS_ITEMPREPAINT:
        {
            // Get the display state from item data
            auto displayState = static_cast<netwatch::util::EndpointEntry::DisplayState>(pLVCD->nmcd.lItemlParam);

            // Set default colors (black text on white background)
            pLVCD->clrText = RGB(0, 0, 0);
            pLVCD->clrTextBk = RGB(255, 255, 255);

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

    // Refresh the list to update the display
    SetRedraw(FALSE);
    ClearAllConnections();
    for (const auto& entry : entries_) {
        AddEntry(entry);
    }
    SetRedraw(TRUE);
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
