#include "Application.h"

#pragma comment(linker, "/MERGE:\".detourd=.data\"")
#pragma comment(linker, "/MERGE:\".detourc=.rdata\"")

BOOL APIENTRY DllMain(HMODULE hModule, DWORD ul_reason_for_call, LPVOID lpReserved)
{
    UNREFERENCED_PARAMETER(lpReserved);
    switch (ul_reason_for_call)
    {
        case DLL_PROCESS_ATTACH:
        {
            Engine::Application::Initialize(hModule);
            break;
        }
        case DLL_THREAD_ATTACH: 
        case DLL_THREAD_DETACH:
            break;
        case DLL_PROCESS_DETACH:
        {
            Engine::Application::Release();
            break;
        }
    }
    return TRUE;
}

/// <summary>
/// 解包
/// </summary>
/// <param name="packageName">封包名 例如xxx.xp3</param>
extern "C" __declspec(dllexport) void WINAPI ExtractPackage(const wchar_t* packageName)
{
    Engine::Application::GetInstance()->GetExtractor()->ExtractPackage(packageName);
}

