// PropertiesDialog.h : interface of the CPropertiesDialog class
//
// Professional connection properties dialog
/////////////////////////////////////////////////////////////////////////////

#pragma once

#include "resource.h"
#include <string>

class CPropertiesDialog : public ATL::CDialogImpl<CPropertiesDialog>
{
public:
    enum { IDD = IDD_PROPERTIES };

    // Connection properties to display
    struct ConnectionProperties {
        std::string processName;
        std::string pid;
        std::string architecture;
        std::string integrityLevel;
        std::string protocol;
        std::string state;
        std::string localAddress;
        std::string localPort;
        std::string remoteAddress;
        std::string remotePort;
        std::string executablePath;
        std::string depStatus;
        std::string aslrStatus;
        std::string cfgStatus;
        std::string safeSehStatus;
    };

    CPropertiesDialog(const ConnectionProperties& props) : properties_(props) {}

    BEGIN_MSG_MAP(CPropertiesDialog)
        MESSAGE_HANDLER(WM_INITDIALOG, OnInitDialog)
        COMMAND_ID_HANDLER(IDOK, OnCloseCmd)
        COMMAND_ID_HANDLER(IDCANCEL, OnCloseCmd)
    END_MSG_MAP()

    LRESULT OnInitDialog(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/);
    LRESULT OnCloseCmd(WORD /*wNotifyCode*/, WORD wID, HWND /*hWndCtl*/, BOOL& /*bHandled*/);

private:
    ConnectionProperties properties_;

    void SetDialogText(int controlId, const std::string& text);
};
