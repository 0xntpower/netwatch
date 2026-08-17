// ColumnsDlg.h : column chooser
//
// The header right-click menu can toggle columns, but it is undiscoverable and
// unreachable from the keyboard. This is the classic Windows equivalent: a
// checked list box with a Show/Hide pair, reachable from View > Columns.
/////////////////////////////////////////////////////////////////////////////

#pragma once

#include "resource.h"
#include "View.h"

class CColumnsDlg : public ATL::CDialogImpl<CColumnsDlg>
{
public:
    enum { IDD = IDD_COLUMNS };

    explicit CColumnsDlg(CConnectionListView& view) : view_(view) {}

    BEGIN_MSG_MAP(CColumnsDlg)
        MESSAGE_HANDLER(WM_INITDIALOG, OnInitDialog)
        COMMAND_ID_HANDLER(IDOK, OnCloseCmd)
        COMMAND_ID_HANDLER(IDCANCEL, OnCloseCmd)
        COMMAND_ID_HANDLER(IDC_COLUMNS_RESET, OnReset)
    END_MSG_MAP()

    LRESULT OnInitDialog(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/)
    {
        CenterWindow(GetParent());

        list_ = GetDlgItem(IDC_COLUMNS_LIST);
        list_.SetExtendedListViewStyle(LVS_EX_CHECKBOXES | LVS_EX_FULLROWSELECT);
        ::SetWindowTheme(list_.m_hWnd, L"Explorer", nullptr);

        // A single full-width column, no header: this is a checklist, not a grid.
        RECT rc = {};
        list_.GetClientRect(&rc);
        list_.InsertColumn(0, L"Column", LVCFMT_LEFT, rc.right - ::GetSystemMetrics(SM_CXVSCROLL));

        for (int i = 0; i < COL_COUNT; ++i) {
            list_.InsertItem(i, CConnectionListView::ColumnName(i));
            list_.SetCheckState(i, view_.IsColumnVisible(i));
        }

        list_.SetItemState(0, LVIS_SELECTED | LVIS_FOCUSED, LVIS_SELECTED | LVIS_FOCUSED);
        ::SetFocus(list_.m_hWnd);
        return FALSE;
    }

    LRESULT OnReset(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
    {
        for (int i = 0; i < COL_COUNT; ++i) {
            list_.SetCheckState(i, CConnectionListView::IsColumnVisibleByDefault(i));
        }
        return 0;
    }

    LRESULT OnCloseCmd(WORD /*wNotifyCode*/, WORD wID, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
    {
        if (wID == IDOK) {
            // Refuse an empty header rather than leaving the user with no way
            // to get a column back.
            bool any = false;
            for (int i = 0; i < COL_COUNT && !any; ++i) {
                any = list_.GetCheckState(i) != FALSE;
            }
            if (!any) {
                ::MessageBoxW(m_hWnd, L"At least one column has to stay visible.",
                    L"Columns", MB_ICONINFORMATION | MB_OK);
                return 0;
            }

            // Shows before hides. ShowColumn refuses to remove the final
            // column, so hiding first could hit that guard part-way through and
            // leave a column visible the user had just unchecked.
            for (int i = 0; i < COL_COUNT; ++i) {
                if (list_.GetCheckState(i)) {
                    view_.ShowColumn(i, true);
                }
            }
            for (int i = 0; i < COL_COUNT; ++i) {
                if (!list_.GetCheckState(i)) {
                    view_.ShowColumn(i, false);
                }
            }
        }

        EndDialog(wID);
        return 0;
    }

private:
    CConnectionListView& view_;
    WTL::CListViewCtrl list_;
};
