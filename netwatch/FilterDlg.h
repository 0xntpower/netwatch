#pragma once

#include "resource.h"

class CFilterDlg : public ATL::CDialogImpl<CFilterDlg>
{
public:
    enum { IDD = IDD_FILTER };

    CFilterDlg(const std::string& currentFilter = "") : filter_(currentFilter) {}

    std::string GetFilter() const { return filter_; }

    BEGIN_MSG_MAP(CFilterDlg)
        MESSAGE_HANDLER(WM_INITDIALOG, OnInitDialog)
        COMMAND_ID_HANDLER(IDOK, OnCloseCmd)
        COMMAND_ID_HANDLER(IDCANCEL, OnCloseCmd)
    END_MSG_MAP()

    LRESULT OnInitDialog(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/)
    {
        CenterWindow(GetParent());

        // Set current filter text
        if (!filter_.empty()) {
            SetDlgItemTextA(m_hWnd, IDC_FILTER_EDIT, filter_.c_str());
        }

        // Select all text in edit box
        ::SendMessage(GetDlgItem(IDC_FILTER_EDIT), EM_SETSEL, 0, -1);
        ::SetFocus(GetDlgItem(IDC_FILTER_EDIT));

        return FALSE; // We set focus manually
    }

    LRESULT OnCloseCmd(WORD /*wNotifyCode*/, WORD wID, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
    {
        if (wID == IDOK)
        {
            // Get filter text
            char buffer[256] = {};
            GetDlgItemTextA(m_hWnd, IDC_FILTER_EDIT, buffer, sizeof(buffer));
            filter_ = buffer;
        }

        EndDialog(wID);
        return 0;
    }

private:
    std::string filter_;
};
