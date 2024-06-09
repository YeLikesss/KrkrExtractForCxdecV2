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

extern "C" __declspec(dllexport) void Dummy() {}