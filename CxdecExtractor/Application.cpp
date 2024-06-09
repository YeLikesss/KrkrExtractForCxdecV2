#include "Application.h"
#include "path.h"
#include "util.h"
#include "ExtendUtils.h"

namespace Engine
{
    /// <summary>
    /// 单实例
    /// </summary>
    static Application* g_Instance = nullptr;

    //Hook插件功能
    tTVPV2LinkProc g_V2Link = nullptr;
    HRESULT __stdcall HookV2Link(iTVPFunctionExporter* exporter)
    {
        HRESULT result = g_V2Link(exporter);
        HookUtils::InlineHook::UnHook(g_V2Link, HookV2Link);
        g_V2Link = nullptr;

        //初始化插件
        Application::GetInstance()->InitializeTVPEngine(exporter);

        return result;
    }

    //Hook插件加载
    auto g_GetProcAddressFunction = ::GetProcAddress;
    FARPROC WINAPI HookGetProcAddress(HMODULE hModule, LPCSTR lpProcName)
    {
        FARPROC result = g_GetProcAddressFunction(hModule, lpProcName);
        if (result)
        {
            // 忽略序号导出
            if (HIWORD(lpProcName) != 0)
            {
                if (strcmp(lpProcName, "V2Link") == 0)
                {
                    //Nt头偏移
                    PIMAGE_NT_HEADERS ntHeader = PIMAGE_NT_HEADERS((ULONG_PTR)hModule + ((PIMAGE_DOS_HEADER)hModule)->e_lfanew);
                    //可选头大小
                    DWORD optionalHeaderSize = ntHeader->FileHeader.SizeOfOptionalHeader;
                    //第一个节表(代码段)
                    PIMAGE_SECTION_HEADER codeSectionHeader = (PIMAGE_SECTION_HEADER)((ULONG_PTR)ntHeader + sizeof(ntHeader->Signature) + sizeof(IMAGE_FILE_HEADER) + optionalHeaderSize);

                    DWORD codeStartRva = codeSectionHeader->VirtualAddress;  //代码段起始RVA
                    DWORD codeSize = codeSectionHeader->SizeOfRawData;		//代码段大小

                    ULONG_PTR codeStartVa = (ULONG_PTR)hModule + codeStartRva;      //代码段起始VA

                    //初始化
                    Application* app = Application::GetInstance();
                    if (!app->IsTVPEngineInitialize())
                    {
                        g_V2Link = (tTVPV2LinkProc)result;
                        HookUtils::InlineHook::Hook(g_V2Link, HookV2Link);
                    }

                    //解包接口
                    ExtractCore* extractor = app->GetExtractor();
                    if (!extractor->IsInitialized())
                    {
                        extractor->Initialize((PVOID)codeStartVa, codeSize);
                    }

                    //初始化完毕 解除Hook
                    if (extractor->IsInitialized())
                    {
                        HookUtils::InlineHook::UnHook(g_GetProcAddressFunction, HookGetProcAddress);
                    }
                }
            }
        }
        return result;
    }


    //**********Application***********//
    Application::Application()
    {
        this->mCurrentDirectoryPath = Path::GetDirectoryName(Util::GetModulePathW(GetModuleHandleW(NULL)));
        this->mTVPExporterInitialized = false;
        this->mExtractor = new ExtractCore();

        //设置解包输出路径
        this->mExtractor->SetOutputDirectory(this->mCurrentDirectoryPath);
    }

    Application::~Application()
    {
        if (this->mExtractor)
        {
            delete this->mExtractor;
            this->mExtractor = nullptr;
        }
    }

    void Application::InitializeModule(HMODULE hModule)
    {
        this->mModuleDirectoryPath = Path::GetDirectoryName(Util::GetModulePathW(hModule));

        //设置解包Log输出路径
        this->mExtractor->SetLoggerDirectory(this->mModuleDirectoryPath);
    }

    void Application::InitializeTVPEngine(iTVPFunctionExporter* exporter)
    {
        this->mTVPExporterInitialized = TVPInitImportStub(exporter);
        TVPSetCommandLine(L"-debugwin", L"yes");
    }

    bool Application::IsTVPEngineInitialize()
    {
        return this->mTVPExporterInitialized;
    }

    ExtractCore* Application::GetExtractor()
    {
        return this->mExtractor;
    }

    //**********====Static====**********//

    Application* Application::GetInstance()
    {
        return g_Instance;
    }

    void Application::Initialize(HMODULE hModule)
    {
        g_Instance = new Application();
        g_Instance->InitializeModule(hModule);

        //Hook
        HookUtils::InlineHook::Hook(g_GetProcAddressFunction, HookGetProcAddress);
    }

    void Application::Release()
    {
        if (g_Instance)
        {
            delete g_Instance;
            g_Instance = nullptr;
        }
    }

    //================================//
}

