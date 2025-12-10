// netwatch.cpp : main source file for netwatch.exe
//

#include "stdafx.h"

#include "resource.h"

#include "View.h"
#include "aboutdlg.h"
#include "MainFrm.h"
#include "util/CommandLineParser.h"

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

	wndMain.ShowWindow(nCmdShow);

	int nRet = theLoop.Run();

	_Module.RemoveMessageLoop();
	return nRet;
}

int WINAPI _tWinMain(HINSTANCE hInstance, HINSTANCE /*hPrevInstance*/, LPTSTR lpstrCmdLine, int nCmdShow)
{
	HRESULT hRes = ::CoInitialize(nullptr);
	ATLASSERT(SUCCEEDED(hRes));

	WTL::AtlInitCommonControls(ICC_COOL_CLASSES | ICC_BAR_CLASSES);	// add flags to support other controls

	hRes = _Module.Init(nullptr, hInstance);
	ATLASSERT(SUCCEEDED(hRes));

	int nRet = Run(lpstrCmdLine, nCmdShow);

	_Module.Term();
	::CoUninitialize();

	return nRet;
}
