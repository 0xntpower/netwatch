// PropertiesDialog.cpp : implementation of the CPropertiesDialog class
//
/////////////////////////////////////////////////////////////////////////////

#include "stdafx.h"
#include "PropertiesDialog.h"
#include "util/StringConversion.h"

LRESULT CPropertiesDialog::OnInitDialog(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/)
{
    CenterWindow(GetParent());

    // Populate all fields with connection properties
    SetDialogText(IDC_PROP_PROCESS, properties_.processName);
    SetDialogText(IDC_PROP_PID, properties_.pid);
    SetDialogText(IDC_PROP_ARCH, properties_.architecture);
    SetDialogText(IDC_PROP_INTEGRITY, properties_.integrityLevel);
    SetDialogText(IDC_PROP_PROTOCOL, properties_.protocol);
    SetDialogText(IDC_PROP_STATE, properties_.state);
    SetDialogText(IDC_PROP_LOCAL_ADDR, properties_.localAddress);
    SetDialogText(IDC_PROP_LOCAL_PORT, properties_.localPort);
    SetDialogText(IDC_PROP_REMOTE_ADDR, properties_.remoteAddress);
    SetDialogText(IDC_PROP_REMOTE_PORT, properties_.remotePort);
    SetDialogText(IDC_PROP_EXECUTABLE, properties_.executablePath);
    SetDialogText(IDC_PROP_DEP, properties_.depStatus);
    SetDialogText(IDC_PROP_ASLR, properties_.aslrStatus);
    SetDialogText(IDC_PROP_CFG, properties_.cfgStatus);
    SetDialogText(IDC_PROP_SAFESEH, properties_.safeSehStatus);

    return TRUE;
}

LRESULT CPropertiesDialog::OnCloseCmd(WORD /*wNotifyCode*/, WORD wID, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
    EndDialog(wID);
    return 0;
}

void CPropertiesDialog::SetDialogText(int controlId, const std::string& text)
{
    std::wstring wideText = netwatch::util::StringConversion::NarrowToWide(text);
    SetDlgItemText(controlId, wideText.c_str());
}
