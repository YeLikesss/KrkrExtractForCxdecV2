#pragma once

#include <Windows.h>
#include <string>
#include <vector>
#include "tp_stub.h"
#include "log.h"

namespace Engine
{
    /// <summary>
    /// 文件表
    /// </summary>
    class FileEntry
    {
    public:
        /// <summary>
        /// 文件夹Hash
        /// </summary>
        unsigned __int8 DirectoryPathHash[8];
        /// <summary>
        /// 文件名Hash
        /// </summary>
        unsigned __int8 FileNameHash[32];
        /// <summary>
        /// 文件Key
        /// </summary>
        __int64 Key;
        /// <summary>
        /// 文件序号
        /// </summary>
        __int64 Ordinal;

        /// <summary>
        /// 获取合法性
        /// </summary>
        bool IsVaild() const
        {
            return this->Ordinal >= 0;
        }

        /// <summary>
        /// 获取加密模式
        /// </summary>
        unsigned __int32 GetEncryptMode() const
        {
            return ((this->Ordinal & 0x0000FFFF00000000i64) >> 32);
        }

        /// <summary>
        /// 获取封包的名字
        /// <para>最多8字节 4个字符 3个Unicode字符 + 0结束符</para>
        /// </summary>
        /// <param name="retValue">字符返回值指针</param>
        __declspec(noinline)
        void GetFakeName(wchar_t* retValue) const
        {
            wchar_t* fakeName = retValue;

            *(__int64*)fakeName = 0;      //清空8字节

            unsigned __int32 ordinalLow32 = this->Ordinal & 0x00000000FFFFFFFFi64;

            int charIndex = 0;
            do
            {
                unsigned __int32 temp = ordinalLow32;
                temp &= 0x00003FFFu;
                temp += 0x00005000u;

                fakeName[charIndex] = temp & 0x0000FFFFu;
                ++charIndex;

                ordinalLow32 >>= 0x0E;
            } while (ordinalLow32 != 0);
        }
    };

	class ExtractCore
	{
    private:
        static constexpr const wchar_t ExtractorOutFolderName[] = L"Extractor_Output";    //提取器输出文件夹名
        static constexpr const wchar_t ExtractorLogFileName[] = L"Extractor.log";        //提取器日志文件名

	private:
		static constexpr const char CreateStreamSignature[] = "\x55\x8B\xEC\x6A\xFF\x68\x2A\x2A\x2A\x2A\x64\xA1\x00\x00\x00\x00\x50\x51\xA1\x2A\x2A\x2A\x2A\x33\xC5\x50\x8D\x45\xF4\x64\xA3\x00\x00\x00\x00\xA1\x2A\x2A\x2A\x2A\x85\xC0\x75\x32\x68\xB0\x30\x00\x00";
		static constexpr const char CreateIndexSignature[] = "\x55\x8B\xEC\x6A\xFF\x68\x2A\x2A\x2A\x2A\x64\xA1\x00\x00\x00\x00\x50\x83\xEC\x14\x57\xA1\x2A\x2A\x2A\x2A\x33\xC5\x50\x8D\x45\xF4\x64\xA3\x00\x00\x00\x00\x83\x7D\x08\x00\x0F\x84\x2A\x2A\x00\x00\xA1\x2A\x2A\x2A\x2A\x85\xC0\x75\x12\x68\x2A\x2A\x2A\x2A\xE8\x2A\x2A\x2A\x2A\x83\xC4\x04\xA3\x2A\x2A\x2A\x2A\xFF\x75\x0C\x8D\x4D\xF0\x51\xFF\xD0\xA1\x2A\x2A\x2A\x2A\xC7\x45\xFC\x00\x00\x00\x00\x85\xC0";
        static constexpr const wchar_t Split[] = L"##YSig##";

		using tCreateStream = IStream* (__cdecl*)(const tTJSString* fakeName, tjs_int64 key, tjs_uint32 encryptMode);
		using tCreateIndex = tjs_error (__cdecl*)(tTJSVariant* retValue, const tTJSVariant* tjsXP3Name);

		tCreateStream mCreateStreamFunc;		//CxCreateStream打开文件流接口
		tCreateIndex mCreateIndexFunc;			//CxCreateIndex获取文件表接口

		std::wstring mExtractDirectoryPath;		//解包输出文件夹
        Log::Logger mLogger;

	public:
		ExtractCore();
		ExtractCore(const ExtractCore&) = delete;
		ExtractCore(ExtractCore&&) = delete;
        ExtractCore& operator=(const ExtractCore&) = delete;
        ExtractCore& operator=(ExtractCore&&) = delete;
        ~ExtractCore();

		/// <summary>
		/// 设置资源输出路径
		/// </summary>
		/// <param name="directory">文件夹绝对路径</param>
		void SetOutputDirectory(const std::wstring& directory);

        /// <summary>
        /// 设置日志输出路径
        /// </summary>
        /// <param name="directory">绝对路径</param>
        void SetLoggerDirectory(const std::wstring& directory);

		/// <summary>
		/// 初始化 (特征码找接口)
		/// </summary>
		/// <param name="codeVa">代码起始地址</param>
		/// <param name="codeSize">代码大小</param>
		void Initialize(PVOID codeVa, DWORD codeSize);
		/// <summary>
		/// 检查是否已经初始化
		/// </summary>
		/// <returns>True已初始化 False未初始化</returns>
		bool IsInitialized();
		/// <summary>
		/// 解包
		/// </summary>
		/// <param name="packageFileName">封包名称</param>
		void ExtractPackage(const std::wstring& packageFileName);

	private:
		/// <summary>
		/// 获取Hxv4文件表
		/// </summary>
		/// <param name="xp3PackagePath">封包绝对路径</param>
		/// <param name="retValue">文件表数组</param>
		void GetEntries(const tTJSString& xp3PackagePath, std::vector<FileEntry>& retValue);

        /// <summary>
        /// 创建资源流
        /// </summary>
        /// <param name="entry">文件表</param>
        /// <param name="packageName">封包名</param>
        /// <returns>IStream对象</returns>
        IStream* CreateStream(const FileEntry& entry, const std::wstring& packageName);

        /// <summary>
        /// 提取文件
        /// </summary>
        /// <param name="stream">流</param>
        /// <param name="extractPath">提取路径</param>
        void ExtractFile(IStream* stream, const std::wstring& extractPath);

        /// <summary>
        /// 尝试解密文本
        /// </summary>
        /// <param name="stream">资源流</param>
        /// <param name="output">输出缓冲区</param>
        /// <returns>True解密成功 False不是文本加密</returns>
        static bool TryDecryptText(IStream* stream, std::vector<uint8_t>& output);
	};
}


