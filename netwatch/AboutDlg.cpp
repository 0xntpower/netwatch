// aboutdlg.cpp : implementation of the CAboutDlg class
//
/////////////////////////////////////////////////////////////////////////////

#include "stdafx.h"
#include "resource.h"

#include "aboutdlg.h"

#include <string>
#include <vector>

#pragma comment(lib, "version.lib")

namespace {

// Read the version out of the binary's own VS_VERSION_INFO rather than keeping
// a second copy in the dialog template. The two had already drifted apart:
// the resource said 1.0.0.1 while the About box said 1.0.2.
std::wstring GetOwnFileVersion() {
    WCHAR path[MAX_PATH] = {};
    if (::GetModuleFileNameW(nullptr, path, MAX_PATH) == 0) {
        return L"";
    }

    DWORD handle = 0;
    const DWORD size = ::GetFileVersionInfoSizeW(path, &handle);
    if (size == 0) {
        return L"";
    }

    std::vector<BYTE> buffer(size);
    if (!::GetFileVersionInfoW(path, handle, size, buffer.data())) {
        return L"";
    }

    VS_FIXEDFILEINFO* info = nullptr;
    UINT infoLen = 0;
    if (!::VerQueryValueW(buffer.data(), L"\\", reinterpret_cast<LPVOID*>(&info), &infoLen) ||
        info == nullptr || infoLen == 0) {
        return L"";
    }

    const WORD major = HIWORD(info->dwFileVersionMS);
    const WORD minor = LOWORD(info->dwFileVersionMS);
    const WORD build = HIWORD(info->dwFileVersionLS);

    return L"Version " + std::to_wstring(major) + L"." +
           std::to_wstring(minor) + L"." + std::to_wstring(build);
}

} // namespace

LRESULT CAboutDlg::OnInitDialog(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/)
{
	CenterWindow(GetParent());

	const std::wstring version = GetOwnFileVersion();
	if (!version.empty()) {
		SetDlgItemText(IDC_ABOUT_VERSION, version.c_str());
	}

	return TRUE;
}

LRESULT CAboutDlg::OnCloseCmd(WORD /*wNotifyCode*/, WORD wID, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	EndDialog(wID);
	return 0;
}
