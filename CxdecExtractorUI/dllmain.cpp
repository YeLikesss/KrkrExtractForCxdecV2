#include <Windows.h>
#include "resource.h"

#include <Shlwapi.h>
#pragma comment(lib,"shlwapi.lib")

/// <summary>
/// 最大路径
/// </summary>
constexpr size_t MaxPath = 1024u;

typedef void (WINAPI* tExtractFunc)(const wchar_t* packageName);
static tExtractFunc g_ExtractPackage = nullptr;

/// <summary>
/// 主窗体消息循环
/// </summary>
/// <param name="hwnd">窗口句柄</param>
/// <param name="msg">消息</param>
/// <param name="wParam"></param>
/// <param name="lParam"></param>
INT_PTR CALLBACK ExtractorDialogWindProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) 
{
    switch (msg)
    {
        case WM_DROPFILES:
        {
            HDROP hDrop = (HDROP)wParam;
            wchar_t fullName[MaxPath];
            //只获取第一项
            if (UINT strLen = ::DragQueryFileW(hDrop, 0, fullName, MaxPath))
            {
                DWORD attr = ::GetFileAttributesW(fullName);
                if (attr != INVALID_FILE_ATTRIBUTES && (attr & FILE_ATTRIBUTE_ARCHIVE) == FILE_ATTRIBUTE_ARCHIVE)
                {
                    const wchar_t* fileName = ::PathFindFileNameW(fullName);
                    g_ExtractPackage(fileName);
                }
                ::DragFinish(hDrop);
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


/// <summary>
/// 窗口代码
/// </summary>
/// <param name="hInstance">模块基地址</param>
DWORD WINAPI WinExtractorEntry(LPVOID hInstance) 
{
    HWND hwnd = ::CreateDialogParamW((HINSTANCE)hInstance, MAKEINTRESOURCEW(IDD_MainForm), NULL, ExtractorDialogWindProc, 0);
    ::ShowWindow(hwnd, SW_NORMAL);

    MSG msg{ };
    while (BOOL ret = ::GetMessageW(&msg, NULL, 0, 0))
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
    return 0;
}

BOOL APIENTRY DllMain(HMODULE hModule, DWORD ul_reason_for_call, LPVOID lpReserved)
{
    UNREFERENCED_PARAMETER(lpReserved);
    switch (ul_reason_for_call)
    {
        case DLL_PROCESS_ATTACH:
        {
            constexpr const wchar_t CoreDllNameW[] = L"CxdecExtractor.dll";

            wchar_t moduleFullPath[MaxPath];
            DWORD strLen = ::GetModuleFileNameW(hModule, moduleFullPath, MaxPath);
            wchar_t* dllName = ::PathFindFileNameW(moduleFullPath);
            memcpy(dllName, CoreDllNameW, sizeof(CoreDllNameW));

            if (HMODULE coreBase = ::LoadLibraryW(moduleFullPath))
            {
                g_ExtractPackage = (tExtractFunc)::GetProcAddress(coreBase, "ExtractPackage");
                if (HANDLE hThread = ::CreateThread(NULL, 0, WinExtractorEntry, hModule, 0, NULL))
                {
                    ::CloseHandle(hThread);
                }
            }
            else
            {
                ::MessageBoxW(nullptr, L"CxdecExtractor.dll加载失败", L"错误", MB_OK);
            }
            break;
        }
        case DLL_THREAD_ATTACH:
        case DLL_THREAD_DETACH: 
            break;
        case DLL_PROCESS_DETACH:
        {
            break;
        }
    }
    return TRUE;
}

extern "C" __declspec(dllexport) void Dummy(){}