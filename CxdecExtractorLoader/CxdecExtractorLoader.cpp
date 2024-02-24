#include <windows.h>
#include <detours.h>
#include <Shlwapi.h>
#pragma comment(lib,"shlwapi.lib")

int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPWSTR lpCmdLine, int nShowCmd)
{
	UNREFERENCED_PARAMETER(hPrevInstance);
	constexpr size_t MaxPath = 1024u;

	wchar_t appFullPathW[MaxPath];
	wchar_t gameFullPathW[MaxPath];
	GetModuleFileNameW(hInstance, appFullPathW, MaxPath);

	int argc = 0;
	LPWSTR* argv = CommandLineToArgvW(lpCmdLine, &argc);
	if (argc) 
	{
		wchar_t* arg = argv[0];
		memcpy(gameFullPathW, arg, (lstrlenW(arg) + 1) * 2);

		if (lstrcmpW(appFullPathW, gameFullPathW))
		{
			wchar_t gameCurrentDirectory[MaxPath];
			char injectDllFullPathA[MaxPath];
			
			{
				memcpy(gameCurrentDirectory, gameFullPathW, MaxPath);
				*(PathFindFileNameW(gameCurrentDirectory) - 1u) = L'\0';
			}
			{
				constexpr const char InjectDllNameA[] = "CxdecExtractorUI.dll";
				GetModuleFileNameA(hInstance, injectDllFullPathA, MaxPath);
				char* dllName = PathFindFileNameA(injectDllFullPathA);
				memcpy(dllName, InjectDllNameA, sizeof(InjectDllNameA));
			}

			STARTUPINFO si{ 0 };
			si.cb = sizeof(si);
			PROCESS_INFORMATION pi{ 0 };

			if (DetourCreateProcessWithDllW(gameFullPathW, NULL, NULL, NULL, FALSE, 0, NULL, gameCurrentDirectory, &si, &pi, injectDllFullPathA, NULL))
			{
				CloseHandle(pi.hThread);
				CloseHandle(pi.hProcess);
			}
			else
			{
				MessageBoxW(nullptr, L"创建进程错误", L"Error", MB_OK);
			}
		}
		else
		{
			MessageBoxW(nullptr, L"请拖拽游戏主程序到Loader", L"Error", MB_OK);
		}
	}
	LocalFree(argv);
	return 0;
}
