#include "Base.h"
#include "pe.h"
#include "path.h"
#include "file.h"
#include "util.h"
#include "stringhelper.h"
#include "encoding.h"
#include "log.h"
#include "detours.h"
#include "CxdecFileEntry.h"
#include "IStreamExtend.h"
#include "zlib.h"

#pragma warning ( push )
#pragma warning ( disable : 4100 4201 4457 )
#include "tp_stub.h"
#pragma warning ( pop )

template<class T>
void InlineHook(T& OriginalFunction, T DetourFunction)
{
    DetourUpdateThread(GetCurrentThread());
    DetourTransactionBegin();
    DetourAttach(&(PVOID&)OriginalFunction, (PVOID&)DetourFunction);
    DetourTransactionCommit();
}

template<class T>
void UnInlineHook(T& OriginalFunction, T DetourFunction)
{
    DetourUpdateThread(GetCurrentThread());
    DetourTransactionBegin();
    DetourDetach(&(PVOID&)OriginalFunction, (PVOID&)DetourFunction);
    DetourTransactionCommit();
}

//*******************配置相关*******************//

static std::wstring g_dllPath;          //本dll路径

static wchar_t g_tableSplit[] = L"##YSig##";    //文件表分割符
static wchar_t g_tableNewLine[] = L"\r\n";        //文件表换行符 [CRLF]

static std::wstring g_currentDirPath;	    //当前游戏文件夹路径
static std::wstring g_extractDirPath;       //资源解包输出文件夹路径

//*********************基本函数************************//

//尝试解密SimpleEncrypted
__declspec(noinline)
bool WINAPI TryDecryptText(IStream* stream, std::vector<uint8_t>& output)
{
    try
    {
        uint8_t mark[2]{ 0 };
        IStreamEx::Read(stream, mark, 2);

        if (mark[0] == 0xfe && mark[1] == 0xfe)   //检查加密标记头
        {
            uint8_t mode;
            IStreamEx::Read(stream, &mode, 1);

            if (mode != 0 && mode != 1 && mode != 2)  //识别模式
            {
                return false;
            }

            ZeroMemory(mark, sizeof(mark));
            IStreamEx::Read(stream, mark, 2);

            if (mark[0] != 0xff || mark[1] != 0xfe)  //Unicode Bom
            {
                return false;
            }

            if (mode == 2)   //压缩模式
            {
                long long compressed = 0;
                long long uncompressed = 0;
                IStreamEx::Read(stream, &compressed, sizeof(long long));
                IStreamEx::Read(stream, &uncompressed, sizeof(long long));

                if (compressed <= 0 || compressed >= INT_MAX || uncompressed <= 0 || uncompressed >= INT_MAX)
                {
                    return false;
                }

                std::vector<uint8_t> data((size_t)compressed);

                //读取压缩数据
                if (IStreamEx::Read(stream, data.data(), compressed) != compressed)
                {
                    return false;
                }

                std::vector<uint8_t> buffer((unsigned int)uncompressed + 2);

                //写入Bom头
                buffer[0] = mark[0];
                buffer[1] = mark[1];

                Bytef* dest = buffer.data() + 2;
                uLongf destLen = (uLongf)uncompressed;

                int result = Z_OK;

                try
                {
                    result = uncompress(dest, &destLen, data.data(), (uLong)compressed);  //Zlib解压
                }
                catch (...)
                {
                    return false;
                }

                if (result != Z_OK || destLen != (uLongf)uncompressed)
                {
                    return false;
                }

                output = std::move(buffer);

                return true;
            }
            else
            {
                long long startpos = IStreamEx::Position(stream); //解密起始位置
                long long endpos = IStreamEx::Length(stream); //解密结束位置

                IStreamEx::Seek(stream, startpos, STREAM_SEEK_SET);      //设置回起始位置

                long long size = endpos - startpos;   //解密大小

                if (size <= 0 || size >= INT_MAX)
                {
                    return false;
                }

                size_t count = (size_t)(size / sizeof(wchar_t));

                if (count == 0)
                {
                    return false;
                }

                std::vector<wchar_t> buffer(count);  //存放文本

                IStreamEx::Read(stream, buffer.data(), size);  //读取资源

                if (mode == 0)  //模式0
                {
                    for (size_t i = 0; i < count; i++)
                    {
                        wchar_t ch = buffer[i];
                        if (ch >= 0x20) buffer[i] = ch ^ (((ch & 0xfe) << 8) ^ 1);
                    }
                }
                else if (mode == 1)   //模式1
                {
                    for (size_t i = 0; i < count; i++)
                    {
                        wchar_t ch = buffer[i];
                        ch = ((ch & 0xaaaaaaaa) >> 1) | ((ch & 0x55555555) << 1);
                        buffer[i] = ch;
                    }
                }

                size_t sizeToCopy = count * sizeof(wchar_t);

                output.resize(sizeToCopy + 2);

                //写入Unicode Bom
                output[0] = mark[0];
                output[1] = mark[1];

                //回写解密后的数据
                memcpy(output.data() + 2, buffer.data(), sizeToCopy);

                return true;
            }
        }
    }
    catch (...)
    {
    }

    return false;
}

//*******************插件初始化相关*******************//

extern "C"
{
    typedef HRESULT(_stdcall* tTVPV2LinkProc)(iTVPFunctionExporter*);
    typedef HRESULT(_stdcall* tTVPV2UnlinkProc)();
}

static bool g_IsTVPStubInitialized = false;     //插件初始化标志

extern "C" __declspec(dllexport) HRESULT _stdcall V2Link(iTVPFunctionExporter* exporter) 
{ 
    g_IsTVPStubInitialized = TVPInitImportStub(exporter);
    return S_OK; 
}
extern "C" __declspec(dllexport) HRESULT _stdcall V2Unlink() { return S_OK; }

//插件功能Hook
tTVPV2LinkProc pfnV2Link = NULL;		//原插件入口

HRESULT _stdcall HookV2Link(iTVPFunctionExporter* exporter)
{
    HRESULT result = pfnV2Link(exporter);
    V2Link(exporter);   //插件初始化

    UnInlineHook(pfnV2Link, HookV2Link);

    TVPSetCommandLine(L"-debugwin", L"yes");
    return result;
}


//*******************资源提取相关***********************//


#define CX_CREATESTREAM_SIGNATURE "\x55\x8B\xEC\x6A\xFF\x68\x2A\x2A\x2A\x2A\x64\xA1\x00\x00\x00\x00\x50\x51\xA1\x2A\x2A\x2A\x2A\x33\xC5\x50\x8D\x45\xF4\x64\xA3\x00\x00\x00\x00\xA1\x2A\x2A\x2A\x2A\x85\xC0\x75\x32\x68\xB0\x30\x00\x00"
#define CX_CREATESTREAM_SIGNATURE_LENGTH ( sizeof(CX_CREATESTREAM_SIGNATURE) -1 )

#define CX_PARSERARCHIVEINDEX_SIGNATURE "\x55\x8B\xEC\x6A\xFF\x68\x2A\x2A\x2A\x2A\x64\xA1\x00\x00\x00\x00\x50\x83\xEC\x14\x57\xA1\x2A\x2A\x2A\x2A\x33\xC5\x50\x8D\x45\xF4\x64\xA3\x00\x00\x00\x00\x83\x7D\x08\x00\x0F\x84\x2A\x2A\x00\x00\xA1\x2A\x2A\x2A\x2A\x85\xC0\x75\x12\x68\x2A\x2A\x2A\x2A\xE8\x2A\x2A\x2A\x2A\x83\xC4\x04\xA3\x2A\x2A\x2A\x2A\xFF\x75\x0C\x8D\x4D\xF0\x51\xFF\xD0\xA1\x2A\x2A\x2A\x2A\xC7\x45\xFC\x00\x00\x00\x00\x85\xC0"
#define CX_PARSERARCHIVEINDEX_SIGNATURE_LENGTH ( sizeof(CX_PARSERARCHIVEINDEX_SIGNATURE) -1 )

typedef tjs_error (_cdecl* tCxParserArchiveIndex)(tTJSVariant* retValue, const tTJSVariant* tjsXP3Name);
tCxParserArchiveIndex CxParserArchiveIndexPtr = nullptr;   //Cx解析文件表函数指针


//此处CryptoFilterStream 继承 IStream
typedef IStream* (_cdecl* tCxCreateStream)(tTJSString* fakeName, tjs_int64 key, tjs_uint32 encryptMode);
tCxCreateStream CxCreateStreamPtr = nullptr;       //CxCreateStream函数指针



IStream* WINAPI CxCreateStreamWithIndex(CxdecFileEntry*, std::wstring&);
bool WINAPI CxGetArchiveList(std::vector<CxdecFileEntry>&, std::wstring&);
void WINAPI ExtractFile(IStream*, std::wstring&);
void WINAPI ExtractorWithPackageName(std::wstring&);

/// <summary>
/// 解析文件表
/// </summary>
/// <param name="retList">文件表</param>
/// <param name="xp3PackageFilePath">封包路径</param>
/// <returns></returns>
bool WINAPI CxGetArchiveList(std::vector<CxdecFileEntry>& retList, std::wstring& xp3PackageFilePath)
{
    retList.clear();

    //加载文件表
    tTJSVariant tjsList = tTJSVariant();
    tTJSVariant tjsXp3FilePath(xp3PackageFilePath.c_str());
    CxParserArchiveIndexPtr(&tjsList, &tjsXp3FilePath);

    //读取文件表数组
    if (tjsList.Type() == tvtObject) 
    {
        tTJSVariantClosure& list = tjsList.AsObjectClosureNoAddRef();

        //读取数组项数
        tTJSVariant tjsCount = tTJSVariant();
        list.PropGet(TJS_MEMBERMUSTEXIST, L"count", NULL, &tjsCount, nullptr);
        long long count = tjsCount.AsInteger();

        if (count > 0) 
        {
            //获取文件夹项数 (占KR数组2项)
            //文件夹路径Hash
            //子文件数组
            int dirListSize = 2;
            int dirCount = (int)count / dirListSize;

            //遍历文件夹
            for (int di = 0; di < dirCount; ++di) 
            {
                //获取文件夹路径Hash与子文件表
                tTJSVariant tjsDirHash = tTJSVariant();
                tTJSVariant tjsFileList = tTJSVariant();
                list.PropGetByNum(TJS_CII_GET, di * dirListSize + 0, &tjsDirHash, nullptr);
                list.PropGetByNum(TJS_CII_GET, di * dirListSize + 1, &tjsFileList, nullptr);

                //获取Hash
                tTJSVariantOctet_S* dirHash = (tTJSVariantOctet_S*)(tjsDirHash.AsOctetNoAddRef());      

                //获取子文件表数组
                tTJSVariantClosure& fileList = tjsFileList.AsObjectClosureNoAddRef();

                //获取子文件数组项数
                tjsCount.Clear();
                fileList.PropGet(TJS_MEMBERMUSTEXIST, L"count", NULL, &tjsCount, nullptr);
                count = tjsCount.AsInteger();

                //获取文件项数 (占KR数组2项)
                //文件名Hash
                //文件信息
                int fileListSize = 2;
                int fileCount = (int)count / dirListSize;

                //遍历子文件
                for (int fi = 0; fi < fileCount; ++fi) 
                {
                    //获取文件名Hash与文件信息
                    tTJSVariant tjsFileNameHash = tTJSVariant();
                    tTJSVariant tjsFileInfo = tTJSVariant();
                    fileList.PropGetByNum(TJS_CII_GET, fi * fileListSize + 0, &tjsFileNameHash, nullptr);
                    fileList.PropGetByNum(TJS_CII_GET, fi * fileListSize + 1, &tjsFileInfo, nullptr);

                    //获取Hash
                    tTJSVariantOctet_S* fileNameHash = (tTJSVariantOctet_S*)(tjsFileNameHash.AsOctetNoAddRef());

                    //获取文件信息
                    tTJSVariantClosure& fileInfo = tjsFileInfo.AsObjectClosureNoAddRef();

                    //获取文件信息
                    long long ordinal = 0;
                    long long key = 0;

                    tTJSVariant tjsValue = tTJSVariant();
                    fileInfo.PropGetByNum(TJS_CII_GET, 0, &tjsValue, nullptr);
                    ordinal = tjsValue.AsInteger();

                    tjsValue.Clear();
                    fileInfo.PropGetByNum(TJS_CII_GET, 1, &tjsValue, nullptr);
                    key = tjsValue.AsInteger();

                    //解析后的文件表
                    CxdecFileEntry arcListBin{ 0 };
                    memcpy(arcListBin.DirectoryPathHash, dirHash->Data, dirHash->Length);
                    memcpy(arcListBin.FileNameHash, fileNameHash->Data, fileNameHash->Length);

                    arcListBin.Key = key;
                    arcListBin.Ordinal = ordinal;

                    retList.push_back(arcListBin);
                }
            }
            return true;
        }
    }
    return false;
}

//创建解密流
IStream* WINAPI CxCreateStreamWithIndex(CxdecFileEntry* arcIndex, std::wstring& packageName)
{
    tjs_char fakeName[4]{ 0 };
    arcIndex->GetFakeName(fakeName);

    tTJSString tjsArcPath = TVPGetAppPath();       //获取游戏路径
    tjsArcPath += packageName.c_str();     //添加封包
    tjsArcPath += L">";                    //添加封包分隔符
    tjsArcPath += fakeName;    //添加资源名

    tjsArcPath = TVPNormalizeStorageName(tjsArcPath);

    return CxCreateStreamPtr(&tjsArcPath, arcIndex->Key, arcIndex->GetEncryptMode());
}

/// <summary>
/// 提取资源
/// </summary>
/// <param name="stream">流</param>
/// <param name="extractPath">导出文件路径</param>
void WINAPI ExtractFile(IStream* stream, std::wstring& extractPath)
{
    unsigned long long size = IStreamEx::Length(stream);	//获取流长度

    //相对路径
    std::wstring relativePath(&extractPath.c_str()[g_extractDirPath.length()]);

    if (size > 0)
    {
        //创建文件夹
        {
            std::wstring outputDir = Path::GetDirectoryName(extractPath);
            if (!outputDir.empty())
            {
                FullCreateDirectoryW(outputDir.c_str());
            }
        }

        std::vector<uint8_t> buffer;

        bool success = false;

        if (TryDecryptText(stream, buffer))  //尝试解密SimpleEncrypt
        {
            success = true;
        }
        else
        {
            buffer.resize(size);  //调整动态数组容器大小

            stream->Seek(LARGE_INTEGER{ 0 }, STREAM_SEEK_SET, NULL);

            //读取KR资源流
            if (IStreamEx::Read(stream, buffer.data(), size) == size)
            {
                success = true;
            }
        }

        if (success && !buffer.empty())
        {
            OutputDebugStringW((L"Extract ---> " + relativePath).c_str());

            if (File::WriteAllBytes(extractPath, buffer.data(), buffer.size()) == false)  //回写文件
            {
                OutputDebugStringW((L"WriteError ---> " + relativePath).c_str());
            }
        }
        else 
        {
            OutputDebugStringW((L"InVaild File ---> " + relativePath).c_str());
        }
        stream->Seek(LARGE_INTEGER{ 0 }, STREAM_SEEK_SET, NULL);
    }
    else
    {
        OutputDebugStringW((L"EmptyFile ---> " + relativePath).c_str());
    }
}

//使用封包名解包
void WINAPI ExtractorWithPackageName(std::wstring& packageFileName)
{
    tTJSString tjsXp3PackagePath = TVPGetAppPath() + packageFileName.c_str();   //获取封包标准路径

    std::wstring xp3PackagePath(tjsXp3PackagePath.c_str());
    std::vector<CxdecFileEntry> arcListBins = std::vector<CxdecFileEntry>();

    if (CxGetArchiveList(arcListBins, xp3PackagePath) && !arcListBins.empty()) 
    {
        FullCreateDirectoryW(g_extractDirPath);     //创建资源提取导出文件夹

        std::wstring extractOutput = g_extractDirPath + L"\\";	//导出文件夹
        std::wstring packageName = Path::GetFileNameWithoutExtension(packageFileName);   //封包名
        std::wstring fileTableOutput = extractOutput + packageName + L".alst";      //文件表输出路径

        //创建写入文件表
        HANDLE tableHandle = CreateFileW(fileTableOutput.c_str(), GENERIC_READ | GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
        if (tableHandle != INVALID_HANDLE_VALUE) 
        {
            //写表准备
            SetFilePointer(tableHandle, 0, NULL, FILE_BEGIN);

            wchar_t* tableSplit = g_tableSplit;
            unsigned int tableSplitSize = sizeof(g_tableSplit) - sizeof(wchar_t);

            wchar_t* tableNewLine = g_tableNewLine;
            unsigned int tableNewLineSize = sizeof(g_tableNewLine) - sizeof(wchar_t);

            DWORD bytesWriten = 0;

            //写bom头
            {
                WORD bom = 0xFEFF;
                WriteFile(tableHandle, &bom, sizeof(bom), &bytesWriten, NULL);
            }

            //遍历文件表
            for (CxdecFileEntry& arcIndexBin : arcListBins)
            {
                std::wstring dirHash = BytesToHexStringW(arcIndexBin.DirectoryPathHash, sizeof(arcIndexBin.DirectoryPathHash)); //文件夹Hash字符串形式
                std::wstring fileNameHash = BytesToHexStringW(arcIndexBin.FileNameHash, sizeof(arcIndexBin.FileNameHash)); //文件名Hash字符串形式
                std::wstring arcOutputPath = extractOutput + packageName + L"\\" + dirHash + L"\\" + fileNameHash;	//该资源导出路径


                //写表  dirHash[Sign]dirHash[Sign]fileHash[Sign]fileHash[NewLine]
                WriteFile(tableHandle, dirHash.c_str(), dirHash.size() * sizeof(wchar_t), &bytesWriten, NULL);
                WriteFile(tableHandle, tableSplit, tableSplitSize, &bytesWriten, NULL);
                WriteFile(tableHandle, dirHash.c_str(), dirHash.size() * sizeof(wchar_t), &bytesWriten, NULL);
                WriteFile(tableHandle, tableSplit, tableSplitSize, &bytesWriten, NULL);

                WriteFile(tableHandle, fileNameHash.c_str(), fileNameHash.size() * sizeof(wchar_t), &bytesWriten, NULL);
                WriteFile(tableHandle, tableSplit, tableSplitSize, &bytesWriten, NULL);
                WriteFile(tableHandle, fileNameHash.c_str(), fileNameHash.size() * sizeof(wchar_t), &bytesWriten, NULL);

                WriteFile(tableHandle, tableNewLine, tableNewLineSize, &bytesWriten, NULL);

                //创建流
                IStream* stream = CxCreateStreamWithIndex(&arcIndexBin, packageFileName);
                if (stream)
                {
                    ExtractFile(stream, arcOutputPath);
                    stream->Release();
                }
            }

            FlushFileBuffers(tableHandle);
            CloseHandle(tableHandle);

            OutputDebugStringW((L"Extract Completed ---> " + packageFileName).c_str());
        }
        else
        {
            OutputDebugStringW((L"Create Archive Table Failed ---> " + fileTableOutput).c_str());
        }
    }
    else
    {
        OutputDebugStringW((L"InVaild PackageFile ---> " + packageFileName).c_str());
    }
}

/// <summary>
/// 解包
/// </summary>
/// <param name="packageName">封包名 例如xxx.xp3</param>
extern "C" __declspec(dllexport) void WINAPI ExtractPackage(const wchar_t* packageName)
{
    std::wstring pkg(packageName);
    ExtractorWithPackageName(pkg);
}


//*******************Hash相关***********************//

//Hash计算接口
class ICxStringHasher
{
public:
    virtual ICxStringHasher* Release(bool isHeap) = 0;
    virtual int Calculate(tTJSVariant* hashValueRet, tTJSString* str, tTJSString* seed) = 0;
};

//Hash计算函数类型  fastcall模拟thiscall
typedef int(_fastcall* tHasherCalcute)(PVOID thisObj, DWORD unusedEdx, tTJSVariant* hashValueRet, tTJSString* str, tTJSString* seed);

//Hash计算虚表布局
struct ICxStringHasherVptrTable
{
    PVOID Release;
    PVOID Calculate;
};


//路径Hasher结构
struct CxPathStringHasher
{
    ICxStringHasherVptrTable* VptrTable;       //虚表
    BYTE* Salt;         //盐指针
    int SaltSize;  //大小
    BYTE SaltData[0x10];    //盐数据
};

//文件名Hasher结构
struct CxFileStringHasher
{
    ICxStringHasherVptrTable* VptrTable;       //虚表
    BYTE* Salt;         //盐指针
    int SaltSize;  //大小
    BYTE SaltData[0x20];    //盐数据
};

//Cx插件储存管理
struct CxTVPStorageMedia
{
    PVOID* VptrTable;
    int RefCount;
    DWORD field_8;     //未知字段 +0x08
    tTJSString PreFix;      //资源前缀
    tTJSString HasherSeed;   //Hash盐
    CRITICAL_SECTION CriticalSection;

    //此处为 ArchiveEntryLoaded   已加载资源表
    BYTE Reserve[0x20];

    /* 
    std::vector<tTJSString>  ???
        tTJSString* Start;
        tTJSString* End;
        tTJSString* MaxEnd;
    */
    std::vector<tTJSString> PackageFileNameLoaded;      //已加载封包

    CxPathStringHasher* PathNameHasher;     //路径Hash对象
    CxFileStringHasher* FileNameHasher;     //文件名Hash对象
};

//初始化封包管理函数指针类型
typedef HRESULT(_cdecl* tInitCompoundStorageMedia)(CxTVPStorageMedia** retTVPStorageMedia, tTJSVariant* tjsVarPrefix, int argCount, PVOID saltGroups);

#define CX_INITSTORAGEMEDIA_SIGNATURE "\x55\x8B\xEC\x6A\xFF\x68\x2A\x2A\x2A\x2A\x64\xA1\x00\x00\x00\x00\x50\x83\xEC\x08\x56\xA1\x2A\x2A\x2A\x2A\x33\xC5\x50\x8D\x45\xF4\x64\xA3\x00\x00\x00\x00\xA1\x2A\x2A\x2A\x2A\x85\xC0\x75\x12\x68\x2A\x2A\x2A\x2A\xE8\x2A\x2A\x2A\x2A\x83\xC4\x04\xA3\x2A\x2A\x2A\x2A\x8B\x75\x0C\x56\xFF\xD0\x83\xF8\x02\x74\x2A\xB8\x15\xFC\xFF\xFF\x8B\x4D\xF4\x64\x89\x0D\x00\x00\x00\x00\x59\x5E\x8B\xE5\x5D\xC3"
#define CX_INITSTORAGEMEDIA_SIGNATURE_LENGTH ( sizeof(CX_INITSTORAGEMEDIA_SIGNATURE) - 1 )


static bool g_HasherInitialized = false;        //Hash功能初始化
static CxTVPStorageMedia* g_TVPStorageMedia = NULL;            //Cx资源管理
static CxPathStringHasher g_PathStringHasher = { 0 };          //路径名hash对象
static CxFileStringHasher g_FileStringHasher = { 0 };          //文件名hash对象

static Log::Logger g_PathHashDumpLog;         //路径名HashDump
static Log::Logger g_FileHashDumpLog;         //文件名HashDump

static ICxStringHasherVptrTable g_HookPathHasherVptr = { 0 };       //Hook路径Hash对象虚表
static ICxStringHasherVptrTable g_HookFileHasherVptr = { 0 };       //Hook文件名Hash对象虚表

static tHasherCalcute PathHashCalcPtr = NULL;   //路径hash计算函数指针
static tHasherCalcute FileHashCalcPtr = NULL;   //文件名hash计算函数指针

static tInitCompoundStorageMedia oInitCompoundStorageMediaPtr = NULL;       //初始化封包管理函数指针

int _fastcall HookPathHasherCalcute(CxPathStringHasher*, DWORD, tTJSVariant*, tTJSString*, tTJSString*);
int _fastcall HookFileHasherCalcute(CxFileStringHasher*, DWORD, tTJSVariant*, tTJSString*, tTJSString*);

//Hook游戏路径Hash计算  fastcall模拟thiscall
int _fastcall HookPathHasherCalcute(CxPathStringHasher* thisObj, DWORD unusedEdx, tTJSVariant* hashValueRet, tTJSString* str, tTJSString* seed)
{
    int result = PathHashCalcPtr(thisObj, 0, hashValueRet, str, seed);
    const wchar_t* relativeDirPath = str->c_str();
    //空文件夹替换
    if (*relativeDirPath == L'\0')
    {
        relativeDirPath = L"%EmptyString%";
    }

    //获取Hash值
    tTJSVariantOctet_S* hashValue = (tTJSVariantOctet_S*)(hashValueRet->AsOctetNoAddRef());
    //打印 String[Sign]Hash[NewLine]
    g_PathHashDumpLog.WriteUnicode(L"%s%s%s\r\n", relativeDirPath, g_tableSplit, BytesToHexStringW(hashValue->Data, hashValue->Length).c_str());

    return result;
}
//Hook游戏文件名Hash计算  fastcall模拟thiscall
int _fastcall HookFileHasherCalcute(CxFileStringHasher* thisObj, DWORD unusedEdx, tTJSVariant* hashValueRet, tTJSString* str, tTJSString* seed)
{
    int result = FileHashCalcPtr(thisObj, 0, hashValueRet, str, seed);
    const wchar_t* fileName = str->c_str();

    //获取Hash值
    tTJSVariantOctet_S* hashValue = (tTJSVariantOctet_S*)(hashValueRet->AsOctetNoAddRef());
    //打印 String[Sign]Hash[NewLine]
    g_FileHashDumpLog.WriteUnicode(L"%s%s%s\r\n", fileName, g_tableSplit, BytesToHexStringW(hashValue->Data, hashValue->Length).c_str());
    
    return result;
}


/// <summary>
/// Hook封包管理初始化函数
/// </summary>
/// <param name="retTVPStorageMedia">KR媒体管理</param>
/// <param name="tjsVarPrefix">前缀</param>
/// <param name="argCount">可选参数个数</param>
/// <param name="saltGroups">可选参数</param>
HRESULT _cdecl HookInitCompoundStorageMedia(CxTVPStorageMedia** retTVPStorageMedia, tTJSVariant* tjsVarPrefix, int argCount, PVOID saltGroups)
{
    HRESULT result = oInitCompoundStorageMediaPtr(retTVPStorageMedia, tjsVarPrefix, argCount, saltGroups);
    if (result == S_OK) 
    {
        CxTVPStorageMedia* storageMedia = *retTVPStorageMedia;

        //打印输出Hash盐值
        OutputDebugStringW((std::wstring(L"Hash Seed ---> ") + storageMedia->HasherSeed.c_str()).c_str());
        OutputDebugStringW((L"PathHasherSalt ---> " + BytesToHexStringW(storageMedia->PathNameHasher->Salt, storageMedia->PathNameHasher->SaltSize)).c_str());
        OutputDebugStringW((L"FileHasherSalt ---> " + BytesToHexStringW(storageMedia->FileNameHasher->Salt, storageMedia->FileNameHasher->SaltSize)).c_str());

        memcpy(&g_PathStringHasher, storageMedia->PathNameHasher, sizeof(CxPathStringHasher));      //保存路径Hash对象
        memcpy(&g_FileStringHasher, storageMedia->FileNameHasher, sizeof(CxFileStringHasher));      //保存文件名Hash对象

        //拷贝虚表
        memcpy(&g_HookPathHasherVptr, storageMedia->PathNameHasher->VptrTable, sizeof(ICxStringHasherVptrTable));   
        memcpy(&g_HookFileHasherVptr, storageMedia->FileNameHasher->VptrTable, sizeof(ICxStringHasherVptrTable));
        //修改虚表
        g_HookPathHasherVptr.Calculate = HookPathHasherCalcute;
        g_HookFileHasherVptr.Calculate = HookFileHasherCalcute;

        //保存原函数指针
        PathHashCalcPtr = (tHasherCalcute)storageMedia->PathNameHasher->VptrTable->Calculate;
        FileHashCalcPtr = (tHasherCalcute)storageMedia->FileNameHasher->VptrTable->Calculate;

        //Hook虚表
        storageMedia->PathNameHasher->VptrTable = &g_HookPathHasherVptr;
        storageMedia->FileNameHasher->VptrTable = &g_HookFileHasherVptr;

        g_TVPStorageMedia = storageMedia;       //保存封包管理对象
        g_HasherInitialized = true;         //初始化完成

        UnInlineHook(oInitCompoundStorageMediaPtr, HookInitCompoundStorageMedia);
    }
    return result;
}

// Original
auto orgFuncGetProcAddress = GetProcAddress;
// Hooked
FARPROC WINAPI HookGetProcAddress(HMODULE hModule, LPCSTR lpProcName)
{
    FARPROC result = orgFuncGetProcAddress(hModule, lpProcName);
    if (result)
    {
        // 忽略序号导出
        if (HIWORD(lpProcName) != 0)
        {
            if (strcmp(lpProcName, "V2Link") == 0)
            {
                //Nt头偏移
                PIMAGE_NT_HEADERS ntHeader = PIMAGE_NT_HEADERS((DWORD)hModule + ((PIMAGE_DOS_HEADER)hModule)->e_lfanew);
                //可选头大小
                DWORD optionalHeaderSize = ntHeader->FileHeader.SizeOfOptionalHeader;
                //第一个节表(代码段)
                PIMAGE_SECTION_HEADER codeSectionHeader = (PIMAGE_SECTION_HEADER)((DWORD)ntHeader + sizeof(ntHeader->Signature) + sizeof(IMAGE_FILE_HEADER) + optionalHeaderSize);

                DWORD codeStartRva = codeSectionHeader->VirtualAddress;  //代码段起始RVA
                DWORD codeSize = codeSectionHeader->SizeOfRawData;		//代码段大小

                DWORD codeStartVa = (DWORD)hModule + codeStartRva;      //代码段起始地址

                //Hook插件入口
                if (!g_IsTVPStubInitialized)
                {
                    pfnV2Link = (tTVPV2LinkProc)result;
                    InlineHook(pfnV2Link, HookV2Link);
                }

                //CxCreateStream接口
                if (!CxCreateStreamPtr)
                {
                    CxCreateStreamPtr = (tCxCreateStream)PE::SearchPattern((PVOID)codeStartVa, codeSize, CX_CREATESTREAM_SIGNATURE, CX_CREATESTREAM_SIGNATURE_LENGTH);
                }
                //CxParserArchiveIndex接口
                if(!CxParserArchiveIndexPtr)
                {
                    CxParserArchiveIndexPtr = (tCxParserArchiveIndex)PE::SearchPattern((PVOID)codeStartVa, codeSize, CX_PARSERARCHIVEINDEX_SIGNATURE, CX_PARSERARCHIVEINDEX_SIGNATURE_LENGTH);
                }

                //CompoundStorageMedia初始化接口
                if (!oInitCompoundStorageMediaPtr) 
                {
                    oInitCompoundStorageMediaPtr = (tInitCompoundStorageMedia)PE::SearchPattern((PVOID)codeStartVa, codeSize, CX_INITSTORAGEMEDIA_SIGNATURE, CX_INITSTORAGEMEDIA_SIGNATURE_LENGTH);
                }

                //获取完毕
                if (CxCreateStreamPtr && CxParserArchiveIndexPtr && oInitCompoundStorageMediaPtr)
                {
                    UnInlineHook(orgFuncGetProcAddress, HookGetProcAddress);

                    //Hook Hash解码
                    InlineHook(oInitCompoundStorageMediaPtr, HookInitCompoundStorageMedia);
                }
            }
        }
    }
    return result;
}

//加载配置
void WINAPI LoadConfiguration()
{
}

/// <summary>
/// 字符串Hash相关初始化
/// </summary>
void WINAPI StringHashStartup()
{
    //初始化dump路径
    std::wstring hashExtractDir = g_currentDirPath + L"\\Hash_Output";
    std::wstring dirHashDump = hashExtractDir + L"\\_dumpDirectoryHash.log";
    std::wstring fileHashDump = hashExtractDir + L"\\_dumpFileHash.log";
    FullCreateDirectoryW(hashExtractDir);

    File::Delete(dirHashDump);
    File::Delete(fileHashDump);

    //打开dump日志
    g_PathHashDumpLog.Open(dirHashDump.c_str());
    g_FileHashDumpLog.Open(fileHashDump.c_str());

    //写UTF-16LE bom头
    WORD bom = 0xFEFF;
    g_PathHashDumpLog.WriteData(&bom, sizeof(bom));
    g_FileHashDumpLog.WriteData(&bom, sizeof(bom));
}


/// <summary>
/// Dll加载初始化
/// </summary>
void WINAPI OnStartup(HMODULE hModule)
{
    InlineHook(orgFuncGetProcAddress, HookGetProcAddress);

    g_currentDirPath.clear();
    g_extractDirPath.clear();
    g_dllPath.clear();

    std::wstring currentDir = Path::GetDirectoryName(Util::GetModulePathW(GetModuleHandleW(NULL)));
    
    g_extractDirPath = currentDir + L"\\Archive_Output";
    g_currentDirPath = std::move(currentDir);

    g_dllPath = std::move(Util::GetModulePathW(hModule));

    LoadConfiguration();   

    StringHashStartup();
}

/// <summary>
/// Dll卸载
/// </summary>
void WINAPI OnShutdown()
{
    //关闭HashDump文本输出
    g_PathHashDumpLog.Close();
    g_FileHashDumpLog.Close();
}

BOOL APIENTRY DllMain(HMODULE hModule, DWORD ul_reason_for_call, LPVOID lpReserved)
{
    UNREFERENCED_PARAMETER(lpReserved);
    switch (ul_reason_for_call)
    {
        case DLL_PROCESS_ATTACH: 
        {
            OnStartup(hModule);
            break;
        }
        case DLL_THREAD_ATTACH: { break; }
        case DLL_THREAD_DETACH: { break; }
        case DLL_PROCESS_DETACH: 
        {
            OnShutdown();
            break;
        }
    }
    return TRUE;
}

