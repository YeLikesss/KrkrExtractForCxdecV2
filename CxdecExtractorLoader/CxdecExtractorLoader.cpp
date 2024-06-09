#include <windows.h>
#include <detours.h>

#include "path.h"
#include "util.h"
#include "directory.h"
#include "encoding.h"
#include "resource.h"

#pragma comment(linker, "/MERGE:\".detourd=.data\"")
#pragma comment(linker, "/MERGE:\".detourc=.rdata\"")

#ifdef _UNICODE
#if defined _M_IX86
#pragma comment(linker,"/manifestdependency:\"type='win32' name='Microsoft.Windows.Common-Controls' version='6.0.0.0' processorArchitecture='x86' publicKeyToken='6595b64144ccf1df' language='*'\"")
#elif defined _M_X64
#pragma comment(linker,"/manifestdependency:\"type='win32' name='Microsoft.Windows.Common-Controls' version='6.0.0.0' processorArchitecture='amd64' publicKeyToken='6595b64144ccf1df' language='*'\"")
#else
#pragma comment(linker,"/manifestdependency:\"type='win32' name='Microsoft.Windows.Common-Controls' version='6.0.0.0' processorArchitecture='*' publicKeyToken='6595b64144ccf1df' language='*'\"")
#endif
#endif


static std::wstring g_LoaderFullPath;				//加载器全路径
static std::wstring g_LoaderCurrentDirectory;		//加载器目录
static std::wstring g_KrkrExeFullPath;				//Krkr游戏主程序全路径
static std::wstring g_KrkrExeDirectory;				//Krkr游戏目录

/// <summary>
/// 主窗体消息循环
/// </summary>
/// <param name="hwnd">窗口句柄</param>
/// <param name="msg">消息</param>
/// <param name="wParam"></param>
/// <param name="lParam"></param>
INT_PTR CALLBACK LoaderDialogWindProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
	switch (msg)
	{
		case WM_COMMAND:
		{
			std::wstring injectDllFileName = std::wstring();
			switch (LOWORD(wParam))
			{
				case IDC_Extractor:
				{
					injectDllFileName = L"CxdecExtractorUI.dll";
					break;
				}
				case IDC_StringDumper:
				{
					injectDllFileName = L"CxdecStringDumper.dll";
					break;
				}
				case IDC_KeyDumper:
				{
					//NotImpl
					::MessageBoxW(nullptr, L"此功能暂未实现", L"错误", MB_OK);
					break;
				}
			}

			if (!injectDllFileName.empty())
			{
				std::wstring injectDllFullPath = Path::Combine(g_LoaderCurrentDirectory, injectDllFileName);
				std::string injectDllFullPathA = Encoding::UnicodeToAnsi(injectDllFullPath, Encoding::CodePage::ACP);

				STARTUPINFOW si{ };
				si.cb = sizeof(si);
				PROCESS_INFORMATION pi{ };

				if (DetourCreateProcessWithDllW(g_KrkrExeFullPath.c_str(), NULL, NULL, NULL, FALSE, 0u, NULL, g_KrkrExeDirectory.c_str(), &si, &pi, injectDllFullPathA.c_str(), NULL))
				{
					::CloseHandle(pi.hThread);
					::CloseHandle(pi.hProcess);

					//运行成功 关闭窗口
					::PostMessageW(hwnd, WM_CLOSE, 0u, 0u);
				}
				else
				{
					::MessageBoxW(nullptr, L"创建进程错误", L"错误", MB_OK);
				}
			}
			return TRUE;
		}
		case WM_CLOSE:
		{
			::DestroyWindow(hwnd);
			return TRUE;
		}
		case WM_DESTROY:
		{
			::PostQuitMessage(0);
			return TRUE;
		}
	}
	return FALSE;
}


int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPWSTR lpCmdLine, int nShowCmd)
{
	UNREFERENCED_PARAMETER(hPrevInstance);

	std::wstring loaderFullPath = Util::GetAppPathW();
	std::wstring loaderCurrentDirectory = Path::GetDirectoryName(loaderFullPath);
	std::wstring krkrExeFullPath = std::wstring();
	std::wstring krkrExeDirectory = std::wstring();

	//获取启动参数
	{
		int argc = 0;
		LPWSTR* argv = ::CommandLineToArgvW(lpCmdLine, &argc);
		if (argc)
		{
			wchar_t* arg = argv[0];

			krkrExeFullPath = std::wstring(arg);
			krkrExeDirectory = Path::GetDirectoryName(krkrExeFullPath);
		}
		::LocalFree(argv);
	}

	g_LoaderFullPath = loaderFullPath;
	g_LoaderCurrentDirectory = loaderCurrentDirectory;
	g_KrkrExeFullPath = krkrExeFullPath;
	g_KrkrExeDirectory = krkrExeDirectory;

	//启动加载器
	{
		if (!krkrExeFullPath.empty() && krkrExeFullPath != loaderFullPath)
		{
			HWND hwnd = ::CreateDialogParamW((HINSTANCE)hInstance, MAKEINTRESOURCEW(IDD_MainForm), NULL, LoaderDialogWindProc, 0u);
			::ShowWindow(hwnd, SW_NORMAL);

			MSG msg{ };
			while (BOOL ret = ::GetMessageW(&msg, NULL, 0u, 0u))
			{
				if (ret == -1)
				{
					return -1;
				}
				else
				{
					::TranslateMessage(&msg);
					::DispatchMessageW(&msg);
				}
			}
		}
		else
		{
			::MessageBoxW(nullptr, L"请拖拽游戏主程序到启动器", L"错误", MB_OK);
		}
	}
	return 0;
}
