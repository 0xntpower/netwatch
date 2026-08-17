#include "stdafx.h"

#include "resource.h"

#include "View.h"
#include "aboutdlg.h"
#include "MainFrm.h"
#include "util/CommandLineParser.h"
#include "util/Error.h"

WTL::CAppModule _Module;

int Run(LPTSTR lpstrCmdLine = nullptr, int nCmdShow = SW_SHOWDEFAULT)
{
	// Parse command-line arguments
	auto opts = netwatch::util::CommandLineParser::Parse(lpstrCmdLine);

	if (opts.showHelp) {
		netwatch::util::CommandLineParser::ShowUsage();
		return 0;
	}

	WTL::CMessageLoop theLoop;
	_Module.AddMessageLoop(&theLoop);

	CMainFrame wndMain;

	// Set filter if provided
	if (!opts.processFilter.empty()) {
		wndMain.SetProcessFilter(opts.processFilter);
	}

	if(wndMain.CreateEx() == nullptr)
	{
		ATLTRACE(_T("Main window creation failed!\n"));
		return 0;
	}

	// Restore the saved size and position before the window is first shown, so
	// it does not visibly jump from the default rectangle to the saved one.
	wndMain.RestoreWindowPlacement();
	wndMain.ShowWindow(nCmdShow);

	int nRet = theLoop.Run();

	_Module.RemoveMessageLoop();
	return nRet;
}

int WINAPI _tWinMain(HINSTANCE hInstance, HINSTANCE /*hPrevInstance*/, LPTSTR lpstrCmdLine, int nCmdShow)
{
	try {
		HRESULT hRes = ::CoInitialize(nullptr);
		ATLASSERT(SUCCEEDED(hRes));

		// LISTVIEW registers the list and its header, STANDARD the dialog
		// controls the column chooser and property sheets rely on. Both were
		// previously being registered only as a side effect of something else
		// pulling them in.
		WTL::AtlInitCommonControls(ICC_COOL_CLASSES | ICC_BAR_CLASSES |
			ICC_LISTVIEW_CLASSES | ICC_STANDARD_CLASSES);

		hRes = _Module.Init(nullptr, hInstance);
		ATLASSERT(SUCCEEDED(hRes));

		int nRet = Run(lpstrCmdLine, nCmdShow);

		_Module.Term();
		::CoUninitialize();

		return nRet;
	}
	catch (const netwatch::util::AppError& e) {
		netwatch::util::LogError(e.what(), e.code);
		::MessageBoxA(nullptr, e.what(), "Application Error", MB_ICONERROR | MB_OK);
		return netwatch::util::HResultFromErrorCode(e.code);
	}
	catch (const std::exception& e) {
		netwatch::util::LogError("Unexpected error", e);
		::MessageBoxA(nullptr, e.what(), "Unexpected Error", MB_ICONERROR | MB_OK);
		return E_FAIL;
	}
	catch (...) {
		netwatch::util::LogError("Unknown fatal error");
		::MessageBoxA(nullptr, "An unknown fatal error occurred", "Fatal Error", MB_ICONERROR | MB_OK);
		return E_UNEXPECTED;
	}
}
