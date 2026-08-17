#include "stdafx.h"
#include "resource.h"
#include "View.h"
#include "net/TcpEnumerator.h"
#include "net/UdpEnumerator.h"
#include "util/StringConversion.h"
#include "util/Error.h"
#include "util/Dpi.h"
#include "util/Settings.h"

#include <algorithm>
#include <iterator>
#include <format>
#include <process.h>
#include <uxtheme.h>

#pragma comment(lib, "uxtheme.lib")

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

bool CConnectionListView::QueryHighContrast()
{
    HIGHCONTRAST hc = {};
    hc.cbSize = sizeof(hc);
    if (!::SystemParametersInfo(SPI_GETHIGHCONTRAST, sizeof(hc), &hc, 0)) {
        return false;
    }
    return (hc.dwFlags & HCF_HIGHCONTRASTON) != 0;
}

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

// Column layout and sort order, persisted per user.
//
// Widths are stored at the DPI they were captured at, normalised back to 96,
// so moving between monitors of different scale does not slowly shrink or grow
// them across sessions.
void CConnectionListView::SaveLayout() const
{
    std::vector<int> visible(COL_COUNT);
    for (int i = 0; i < COL_COUNT; ++i) {
        visible[i] = columnVisible_[i] ? 1 : 0;
    }
    netwatch::util::Settings::SetIntArray(L"ColumnVisible", visible);

    const int dpi = netwatch::util::DpiForWindow(m_hWnd);
    std::vector<int> widths(COL_COUNT, 0);
    for (size_t v = 0; v < visibleToLogical_.size(); ++v) {
        const int actual = const_cast<CConnectionListView*>(this)->GetColumnWidth(static_cast<int>(v));
        widths[visibleToLogical_[v]] = ::MulDiv(actual, netwatch::util::kReferenceDpi, dpi);
    }
    netwatch::util::Settings::SetIntArray(L"ColumnWidths", widths);

    std::vector<int> sized(COL_COUNT);
    for (int i = 0; i < COL_COUNT; ++i) {
        sized[i] = userSizedColumn_[i] ? 1 : 0;
    }
    netwatch::util::Settings::SetIntArray(L"ColumnUserSized", sized);

    netwatch::util::Settings::SetInt(L"SortColumn", m_nSortColumn);
    netwatch::util::Settings::SetInt(L"SortAscending", m_bSortAscending ? 1 : 0);
}

void CConnectionListView::LoadLayout()
{
    const auto visible = netwatch::util::Settings::GetIntArray(L"ColumnVisible");
    if (visible.size() == COL_COUNT) {
        bool any = false;
        for (int i = 0; i < COL_COUNT; ++i) {
            columnVisible_[i] = (visible[i] != 0);
            any = any || columnVisible_[i];
        }
        // Never load a state with nothing visible, however it got written.
        if (!any) {
            for (int i = 0; i < COL_COUNT; ++i) {
                columnVisible_[i] = IsColumnVisibleByDefault(i);
            }
        }
    }

    const auto sized = netwatch::util::Settings::GetIntArray(L"ColumnUserSized");
    if (sized.size() == COL_COUNT) {
        for (int i = 0; i < COL_COUNT; ++i) {
            userSizedColumn_[i] = (sized[i] != 0);
        }
    }

    savedWidths_ = netwatch::util::Settings::GetIntArray(L"ColumnWidths");
    if (savedWidths_.size() != COL_COUNT) {
        savedWidths_.clear();
    }

    const int sortColumn = netwatch::util::Settings::GetInt(L"SortColumn", -1);
    if (sortColumn >= -1 && sortColumn < COL_COUNT) {
        m_nSortColumn = sortColumn;
    }
    m_bSortAscending = netwatch::util::Settings::GetInt(L"SortAscending", 1) != 0;
}

const wchar_t* CConnectionListView::ColumnName(int logicalIndex)
{
    if (logicalIndex < 0 || logicalIndex >= COL_COUNT) {
        return L"";
    }
    return kColumnInfo[logicalIndex].name;
}

// The byte counters need ESTATS, which needs elevation, so they stay off until
// the user asks for them.
bool CConnectionListView::IsColumnVisibleByDefault(int logicalIndex)
{
    return logicalIndex != COL_BYTES_SENT && logicalIndex != COL_BYTES_RCVD;
}

void CConnectionListView::InitColumns()
{
    // Note: LVS_OWNERDATA style is set at creation time in MainFrm.cpp
    // This makes the ListView "virtual" - it doesn't store items,
    // it asks us for data via LVN_GETDISPINFO when it needs to paint

    // "Explorer" gives the list and its header the same visuals Explorer,
    // TCPView and Task Manager use: soft rounded selection, hover highlight,
    // flat gradient header, and native sort arrows. Without it comctl32 falls
    // back to the Windows 2000 solid-blue selection block.
    ::SetWindowTheme(m_hWnd, L"Explorer", nullptr);

    // No gridlines: Explorer-themed lists don't draw them, and they fight the
    // hover highlight. FULLROWSELECT + DOUBLEBUFFER + INFOTIP is the modern set.
    SetExtendedListViewStyle(
        LVS_EX_FULLROWSELECT |
        LVS_EX_HEADERDRAGDROP |
        LVS_EX_DOUBLEBUFFER |
        LVS_EX_INFOTIP |
        LVS_EX_LABELTIP
    );

    for (int i = 0; i < COL_COUNT; ++i) {
        columnVisible_[i] = IsColumnVisibleByDefault(i);
    }

    LoadLayout();
    RebuildColumns();
}

// Rebuild the header from columnVisible_ and rebuild the visible-to-logical
// map that OnGetDispInfo and the command handlers depend on.
//
// The ListView compacts subitem indices: hiding a column shifts every column
// after it down by one. Without this map, iSubItem would be read as a logical
// ConnectionListColumns value and every column past the hidden one would render
// its neighbour's data.
void CConnectionListView::RebuildColumns()
{
    const int dpi = netwatch::util::DpiForWindow(m_hWnd);

    SetRedraw(FALSE);

    while (DeleteColumn(0)) {
        // Header_DeleteItem returns FALSE once the header is empty.
    }

    visibleToLogical_.clear();
    for (int logical = 0; logical < COL_COUNT; ++logical) {
        if (!columnVisible_[logical]) {
            continue;
        }
        // Prefer a width the user chose in an earlier session over the default.
        int width96 = kColumnInfo[logical].defaultWidth;
        if (savedWidths_.size() == COL_COUNT && savedWidths_[logical] > 0) {
            width96 = savedWidths_[logical];
        }

        const int visible = static_cast<int>(visibleToLogical_.size());
        InsertColumn(visible,
            kColumnInfo[logical].name,
            kColumnInfo[logical].format,
            netwatch::util::ScaleForDpi(width96, dpi));
        visibleToLogical_.push_back(logical);
    }

    SetRedraw(TRUE);
    UpdateSortIndicator();
    Invalidate();
}

// Column widths were authored at 96 DPI, so they have to be recomputed when the
// window moves to a monitor with a different scale factor. Widths the user has
// dragged are deliberately preserved.
void CConnectionListView::OnDpiChanged()
{
    const int dpi = netwatch::util::DpiForWindow(m_hWnd);

    for (size_t visible = 0; visible < visibleToLogical_.size(); ++visible) {
        const int logical = visibleToLogical_[visible];
        if (userSizedColumn_[logical]) {
            continue;
        }
        SetColumnWidth(static_cast<int>(visible),
            netwatch::util::ScaleForDpi(kColumnInfo[logical].defaultWidth, dpi));
    }
}

int CConnectionListView::LogicalColumn(int visibleIndex) const
{
    if (visibleIndex < 0 || visibleIndex >= static_cast<int>(visibleToLogical_.size())) {
        return -1;
    }
    return visibleToLogical_[visibleIndex];
}

int CConnectionListView::VisibleColumn(int logicalIndex) const
{
    for (size_t i = 0; i < visibleToLogical_.size(); ++i) {
        if (visibleToLogical_[i] == logicalIndex) {
            return static_cast<int>(i);
        }
    }
    return -1;
}

// Paint the themed up/down arrow on the sorted column and clear it everywhere
// else, so the user can see what the list is ordered by.
void CConnectionListView::UpdateSortIndicator()
{
    WTL::CHeaderCtrl header = GetHeader();
    if (header.m_hWnd == nullptr) {
        return;
    }

    const int sortedVisible = (m_nSortColumn >= 0) ? VisibleColumn(m_nSortColumn) : -1;
    const int count = header.GetItemCount();

    for (int i = 0; i < count; ++i) {
        HDITEM item = {};
        item.mask = HDI_FORMAT;
        if (!header.GetItem(i, &item)) {
            continue;
        }

        const int wanted = (i != sortedVisible)
            ? 0
            : (m_bSortAscending ? HDF_SORTUP : HDF_SORTDOWN);

        const int current = item.fmt & (HDF_SORTUP | HDF_SORTDOWN);
        if (current == wanted) {
            continue;
        }

        item.fmt = (item.fmt & ~(HDF_SORTUP | HDF_SORTDOWN)) | wanted;
        header.SetItem(i, &item);
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
            // SHGetFileInfo, used to resolve process icons, wants an
            // initialised apartment on the calling thread.
            const HRESULT comInit = ::CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
            struct ComScope {
                bool owned;
                ~ComScope() { if (owned) ::CoUninitialize(); }
            } comScope{ SUCCEEDED(comInit) };

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

    // The cache lives on the view but is only ever touched from this worker
    // thread, one pass at a time, guarded by refreshInProgress_.
    processCache_.BeginPass();
    auto tcpEntries = netwatch::net::TcpEnumerator::Enumerate(processCache_);
    auto udpEntries = netwatch::net::UdpEnumerator::Enumerate(processCache_);
    processCache_.EndPass();

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
                // Filter unconnected endpoints if setting is disabled. The
                // enumerators set hasRemote, so this works for both the TCP
                // sentinel ("0.0.0.0") and the UDP one ("*").
                if (!currentShowUnconnected && !entry.hasRemote) {
                    return true;
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

    // refreshInProgress_ is normally cleared on the UI thread once it has
    // consumed the results. If the notification never lands, clear it here
    // instead, otherwise the flag latches on and every later refresh
    // early-returns forever.
    if (m_hWnd == nullptr || shutdownRequested_ ||
        !::PostMessage(m_hWnd, WM_REFRESH_COMPLETE, 0, 0)) {
        refreshInProgress_ = false;
    }
}

LRESULT CConnectionListView::OnRefreshComplete(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/)
{
    EnsureImageList();
    ApplyPendingEntries();

    int nConnections = 0;
    int nListening = 0;
    int nEndpoints = 0;

    for (const auto& entry : entries_) {
        // Rows lingering to show they just closed are no longer live, so they
        // must not inflate the counters.
        if (entry.displayState == netwatch::util::EndpointEntry::DisplayState::Closed) {
            continue;
        }
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

// True when anything the user can see about a connection has moved.
static bool ConnectionChanged(const netwatch::util::EndpointEntry& a,
                              const netwatch::util::EndpointEntry& b) {
    return a.state != b.state
        || a.stats.sentBytes != b.stats.sentBytes
        || a.stats.rcvdBytes != b.stats.rcvdBytes;
}

void CConnectionListView::ApplyPendingEntries() {
    std::vector<netwatch::util::EndpointEntry> newEntries;
    {
        std::lock_guard<std::mutex> lock(pendingEntriesMutex_);
        newEntries = std::move(pendingEntries_);
    }

    // Index the previous pass so each row can be classified as new, changed,
    // unchanged or gone. Without this diff, displayState stayed Normal forever
    // and the colour coding in OnCustomDraw could never fire.
    std::unordered_map<std::string, size_t> previous;
    previous.reserve(entries_.size());
    for (size_t i = 0; i < entries_.size(); ++i) {
        previous.emplace(MakeConnectionKey(entries_[i]), i);
    }

    std::unordered_set<std::string> currentKeys;
    currentKeys.reserve(newEntries.size());

    for (auto& entry : newEntries) {
        std::string key = MakeConnectionKey(entry);
        const auto it = previous.find(key);

        if (it == previous.end()) {
            entry.displayState = netwatch::util::EndpointEntry::DisplayState::New;
        } else if (ConnectionChanged(entries_[it->second], entry)) {
            entry.displayState = netwatch::util::EndpointEntry::DisplayState::Modified;
        } else {
            // A highlight lasts exactly one refresh cycle, the same way TCPView
            // behaves. No timers needed: not re-triggering is what clears it.
            entry.displayState = netwatch::util::EndpointEntry::DisplayState::Normal;
        }

        currentKeys.insert(std::move(key));
    }

    // Carry gone connections over for one cycle so the user can actually see
    // what disappeared, then let them drop.
    for (const auto& old : entries_) {
        if (old.displayState == netwatch::util::EndpointEntry::DisplayState::Closed) {
            continue;  // already shown as closed once
        }
        if (currentKeys.count(MakeConnectionKey(old)) != 0) {
            continue;
        }
        auto closed = old;
        closed.displayState = netwatch::util::EndpointEntry::DisplayState::Closed;
        newEntries.push_back(std::move(closed));
    }

    SortEntries(newEntries);

    // Remember which connection is selected. The index alone is meaningless
    // because rows shift between passes.
    std::string selectedKey;
    const int previousSelection = GetNextItem(-1, LVNI_SELECTED);
    if (previousSelection >= 0 && previousSelection < static_cast<int>(entries_.size())) {
        selectedKey = MakeConnectionKey(entries_[previousSelection]);
    }

    // If the same connections come back in the same order, only the rows whose
    // contents actually moved need repainting. That is the steady state, and it
    // is what stops an idle window burning a full redraw every tick.
    std::vector<int> dirtyRows;
    bool layoutUnchanged = (newEntries.size() == entries_.size());
    if (layoutUnchanged) {
        for (size_t i = 0; i < newEntries.size(); ++i) {
            if (MakeConnectionKey(newEntries[i]) != MakeConnectionKey(entries_[i])) {
                layoutUnchanged = false;
                dirtyRows.clear();
                break;
            }
            if (newEntries[i].displayState != entries_[i].displayState ||
                ConnectionChanged(entries_[i], newEntries[i])) {
                dirtyRows.push_back(static_cast<int>(i));
            }
        }
    }

    entries_ = std::move(newEntries);

    if (layoutUnchanged) {
        // Count is identical, so suppress the control's own full invalidate and
        // repaint only the rows that moved.
        ListView_SetItemCountEx(m_hWnd, static_cast<int>(entries_.size()),
            LVSICF_NOSCROLL | LVSICF_NOINVALIDATEALL);
        for (const int row : dirtyRows) {
            RedrawItems(row, row);
        }
    } else {
        // LVSICF_NOSCROLL keeps the scroll position where the user left it.
        ListView_SetItemCountEx(m_hWnd, static_cast<int>(entries_.size()), LVSICF_NOSCROLL);
    }

    RestoreSelection(selectedKey, previousSelection);
}

// Re-select the connection that was selected before the refresh, wherever it
// landed in the new ordering.
void CConnectionListView::RestoreSelection(const std::string& selectedKey, int previousIndex) {
    if (selectedKey.empty()) {
        return;
    }

    int newIndex = -1;
    for (int i = 0; i < static_cast<int>(entries_.size()); ++i) {
        if (MakeConnectionKey(entries_[i]) == selectedKey) {
            newIndex = i;
            break;
        }
    }

    if (newIndex == previousIndex) {
        return;  // already selected at the right index
    }

    if (previousIndex >= 0 && previousIndex < static_cast<int>(entries_.size())) {
        SetItemState(previousIndex, 0, LVIS_SELECTED | LVIS_FOCUSED);
    }

    if (newIndex >= 0) {
        SetItemState(newIndex, LVIS_SELECTED | LVIS_FOCUSED, LVIS_SELECTED | LVIS_FOCUSED);
    }
}

// LVN_GETDISPINFO handler - provides text for virtual ListView items on demand
LRESULT CConnectionListView::OnGetDispInfo(int /*idCtrl*/, LPNMHDR pnmh, BOOL& /*bHandled*/)
{
    NMLVDISPINFO* pDispInfo = reinterpret_cast<NMLVDISPINFO*>(pnmh);
    LVITEM* pItem = &pDispInfo->item;

    const int row = pItem->iItem;
    // iSubItem is a *visible* index. Translate before touching entry fields.
    const int col = LogicalColumn(pItem->iSubItem);

    if (row < 0 || row >= static_cast<int>(entries_.size()) || col < 0) {
        return 0;
    }

    if (pItem->mask & LVIF_TEXT) {
        std::wstring text = GetCellText(row, col);
        // Copy text to the buffer provided by the ListView
        wcsncpy_s(pItem->pszText, pItem->cchTextMax, text.c_str(), _TRUNCATE);
    }

    // Only the leftmost column carries an icon, and only when it is the process
    // name column. The shell image list is attached lazily once the first icon
    // has actually been resolved.
    if ((pItem->mask & LVIF_IMAGE) != 0) {
        pItem->iImage = (pItem->iSubItem == 0 && col == COL_PROCESS)
            ? entries_[row].iconIndex
            : -1;
    }

    return 0;
}

// The shell hands back its shared image list the first time an icon is
// resolved, which happens on the worker thread. Attach it once it exists.
void CConnectionListView::EnsureImageList()
{
    if (imageListAttached_) {
        return;
    }
    HIMAGELIST shellList = netwatch::system::SystemSmallImageList();
    if (shellList == nullptr) {
        return;
    }
    // LVS_SHAREIMAGELISTS on the control keeps this from being destroyed with
    // the window. It belongs to the shell.
    ListView_SetImageList(m_hWnd, shellList, LVSIL_SMALL);
    imageListAttached_ = true;
}

// Tab-separated text for the selected rows, restricted to the columns the user
// currently has visible and emitted in the order they are displayed. Falls back
// to every row when nothing is selected, which is what makes "select all, copy"
// and "copy with no selection" both do the obvious thing.
std::wstring CConnectionListView::BuildClipboardText() const
{
    if (entries_.empty() || visibleToLogical_.empty()) {
        return std::wstring();
    }

    std::vector<int> rows;
    for (int i = GetNextItem(-1, LVNI_SELECTED); i >= 0; i = GetNextItem(i, LVNI_SELECTED)) {
        rows.push_back(i);
    }
    if (rows.empty()) {
        rows.resize(entries_.size());
        for (size_t i = 0; i < rows.size(); ++i) {
            rows[i] = static_cast<int>(i);
        }
    }

    std::wstring out;
    out.reserve(rows.size() * 160);

    for (size_t c = 0; c < visibleToLogical_.size(); ++c) {
        if (c > 0) {
            out += L'\t';
        }
        out += kColumnInfo[visibleToLogical_[c]].name;
    }
    out += L"\r\n";

    for (const int row : rows) {
        if (row < 0 || row >= static_cast<int>(entries_.size())) {
            continue;
        }
        for (size_t c = 0; c < visibleToLogical_.size(); ++c) {
            if (c > 0) {
                out += L'\t';
            }
            out += GetCellText(row, visibleToLogical_[c]);
        }
        out += L"\r\n";
    }

    return out;
}

bool CConnectionListView::GetEntry(int row, netwatch::util::EndpointEntry& out) const
{
    if (row < 0 || row >= static_cast<int>(entries_.size())) {
        return false;
    }
    out = entries_[row];
    return true;
}

// LVN_GETINFOTIP handler - hover text for the row.
//
// Executable paths are routinely wider than any sensible column, and without
// this the only way to read one is to drag the column out and back.
LRESULT CConnectionListView::OnGetInfoTip(int /*idCtrl*/, LPNMHDR pnmh, BOOL& /*bHandled*/)
{
    auto* tip = reinterpret_cast<NMLVGETINFOTIP*>(pnmh);
    if (tip->pszText == nullptr || tip->cchTextMax <= 1) {
        return 0;
    }

    const int row = tip->iItem;
    if (row < 0 || row >= static_cast<int>(entries_.size())) {
        return 0;
    }

    const auto& entry = entries_[row];
    std::wstring text = netwatch::util::StringConversion::NarrowToWide(entry.processName);
    text += L" (PID " + std::to_wstring(entry.pid) + L")";

    if (!entry.executablePath.empty()) {
        text += L"\r\n";
        text += netwatch::util::StringConversion::NarrowToWide(entry.executablePath);
    }

    text += L"\r\n";
    text += netwatch::util::StringConversion::NarrowToWide(entry.protocol);
    text += L"  ";
    text += netwatch::util::StringConversion::NarrowToWide(entry.localAddress);
    text += L":" + std::to_wstring(entry.localPort);
    if (entry.hasRemote) {
        text += L"  ->  ";
        text += netwatch::util::StringConversion::NarrowToWide(entry.remoteAddress);
        text += L":" + std::to_wstring(entry.remotePort);
    }

    wcsncpy_s(tip->pszText, tip->cchTextMax, text.c_str(), _TRUNCATE);
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

    // All narrow strings in EndpointEntry are UTF-8, so widen with the same
    // codepage they were produced with. ATL::CA2W would use the ANSI codepage
    // and mangle any non-ASCII process name or path.
    const auto wide = [](const std::string& s) {
        return netwatch::util::StringConversion::NarrowToWide(s);
    };

    switch (column) {
        case COL_PROCESS:         return wide(entry.processName);
        case COL_PID:             return std::to_wstring(entry.pid);
        case COL_PROTOCOL:        return wide(entry.protocol);
        case COL_INTEGRITY:       return wide(entry.integrityLevel);
        case COL_LOCAL_ADDRESS:   return wide(entry.localAddress);
        case COL_LOCAL_PORT:      return std::to_wstring(entry.localPort);
        case COL_REMOTE_ADDRESS:  return wide(entry.remoteAddress);
        // A UDP endpoint has no peer port. Printing 0 implies a measurement
        // that was never taken.
        case COL_REMOTE_PORT:     return entry.hasRemote ? std::to_wstring(entry.remotePort) : L"";
        case COL_STATE:           return wide(entry.state);
        case COL_ARCHITECTURE:    return wide(entry.architecture);
        case COL_DEP_STATUS:      return wide(entry.depStatus);
        case COL_ASLR_STATUS:     return wide(entry.aslrStatus);
        case COL_EXECUTABLE_PATH: return wide(entry.executablePath);
        case COL_CFG_STATUS:      return wide(entry.cfgStatus);
        case COL_SAFESEH_STATUS:  return wide(entry.safeSehStatus);
        // Blank rather than "0" when ESTATS collection is not running, so an
        // unmeasured connection is not shown as an idle one.
        case COL_BYTES_SENT:
            return entry.statsAvailable ? wide(FormatNumber(entry.stats.sentBytes)) : L"";
        case COL_BYTES_RCVD:
            return entry.statsAvailable ? wide(FormatNumber(entry.stats.rcvdBytes)) : L"";
        default:
            return L"";
    }
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
    // The notification carries a visible index. m_nSortColumn is kept logical so
    // it survives a column being hidden or shown.
    const int nColumn = LogicalColumn(pnmv->iSubItem);
    if (nColumn < 0) {
        return 0;
    }

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
    UpdateSortIndicator();

    // For virtual ListView, just tell it the count again. LVSICF_NOSCROLL keeps
    // the scroll position stable. Every row index now holds different data, so a
    // full repaint is correct here.
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
        // Sample the accessibility setting once per paint cycle rather than once
        // per row, and pick up theme changes without needing a separate handler.
        highContrast_ = QueryHighContrast();
        // Request item-specific notifications
        return CDRF_NOTIFYITEMDRAW;

    case CDDS_ITEMPREPAINT:
        {
            const int row = static_cast<int>(pLVCD->nmcd.dwItemSpec);

            // Start from the theme's own colours rather than hardcoded black on
            // white, so the list stays readable under High Contrast and under a
            // non-default colour scheme.
            pLVCD->clrText = ::GetSysColor(COLOR_WINDOWTEXT);
            pLVCD->clrTextBk = ::GetSysColor(COLOR_WINDOW);

            if (row < 0 || row >= static_cast<int>(entries_.size())) {
                return CDRF_NEWFONT;
            }

            // Under High Contrast the user has asked for a specific, minimal
            // palette. Tinting rows would defeat that, so leave them alone and
            // let the state still read from the State column.
            if (highContrast_) {
                return CDRF_NEWFONT;
            }

            switch (entries_[row].displayState) {
            case netwatch::util::EndpointEntry::DisplayState::New:
                pLVCD->clrText = RGB(0, 128, 0);        // Green for new
                break;
            case netwatch::util::EndpointEntry::DisplayState::Closed:
                pLVCD->clrText = RGB(192, 0, 0);        // Red for closed
                break;
            case netwatch::util::EndpointEntry::DisplayState::Modified:
                pLVCD->clrTextBk = RGB(255, 255, 192);  // Yellow wash for modified
                break;
            case netwatch::util::EndpointEntry::DisplayState::Normal:
            default:
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

    // Refuse to hide the last remaining column: an empty header leaves the user
    // with no way to get any column back.
    if (!show) {
        int remaining = 0;
        for (int i = 0; i < COL_COUNT; ++i) {
            if (columnVisible_[i]) {
                ++remaining;
            }
        }
        if (remaining <= 1) {
            return;
        }
    }

    columnVisible_[columnIndex] = show;
    RebuildColumns();
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

    if (pnmh->hwndFrom != header.m_hWnd) {
        bHandled = FALSE;
        return 0;
    }

    // Remember columns the user has sized by hand so a DPI change does not
    // overwrite their width with a scaled default.
    if (pnmh->code == HDN_ENDTRACKW || pnmh->code == HDN_ENDTRACKA) {
        const NMHEADER* pHeader = reinterpret_cast<const NMHEADER*>(pnmh);
        const int logical = LogicalColumn(pHeader->iItem);
        if (logical >= 0) {
            userSizedColumn_[logical] = true;
        }
        bHandled = FALSE;
        return 0;
    }

    // Check if notification is from header control and it's a right-click
    if (pnmh->code == NM_RCLICK) {
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
