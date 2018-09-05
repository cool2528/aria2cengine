#include "MainFrame_ui.h"
int APIENTRY wWinMain(_In_ HINSTANCE hInstance, _In_opt_ HINSTANCE hPrevInstance, _In_ LPWSTR lpCmdLine, _In_ int nShowCmd)
{
	g_hInstance = hInstance;
	CPaintManagerUI::SetInstance(hInstance);
	CPaintManagerUI::SetResourcePath(CPaintManagerUI::GetInstancePath());

	HRESULT hr = ::CoInitialize(NULL);
	if (FAILED(hr))return 0;
	MainFrame_ui* MainFrame = new MainFrame_ui;
	MainFrame->Create(NULL, _T("MainFrame"), UI_WNDSTYLE_FRAME | WS_CLIPCHILDREN, WS_EX_ACCEPTFILES);
	MainFrame->CenterWindow();
	MainFrame->ShowWindow();
	CPaintManagerUI::MessageLoop();
	delete MainFrame;
	::CoUninitialize();
	return 0;
}