#pragma once

#include <Windows.h>
#include <string>
#include "tp_stub.h"
#include "log.h"

namespace Engine
{

	/// <summary>
	/// Hash接口
	/// </summary>
	class IStringHasher
	{
	public:
		//IStringHasher::Calculate接口  fastcall模拟thiscall
		using tCalculate = tjs_int(__fastcall*)(IStringHasher* thisObj, void* unuseEdx, tTJSVariant* hashValueRet, const tTJSString* str, const tTJSString* seed);

		/// <summary>
		/// 虚表结构
		/// </summary>
		struct VptrTable
		{
			void* Destruct;			//析构
			tCalculate Calculate;	//计算Hash
		};

	private:
		//vtable   offset:0x00
		tjs_uint8* mSalt;   //盐指针   offset:0x04
		tjs_int mSaltSize;	//盐大小   offset:0x08
	public:
		virtual ~IStringHasher() = 0;
		/// <summary>
		/// Hash计算
		/// </summary>
		/// <param name="hashValueRet">返回值</param>
		/// <param name="str">字符串</param>
		/// <param name="seed">种子</param>
		/// <returns>Hash长度</returns>
		virtual tjs_int Calculate(tTJSVariant* hashValueRet, const tTJSString* str, const tTJSString* seed) = 0;

	public:
		/// <summary>
		/// 获取盐的长度
		/// </summary>
		tjs_int GetSaltLength() const;
		/// <summary>
		/// 获取盐数据指针
		/// </summary>
		const tjs_uint8* GetSaltBytes() const;

		/// <summary>
		/// 获取虚表指针 (Hook用)
		/// </summary>
		VptrTable* GetVptrTable();
		/// <summary>
		/// 设置虚表指针 (Hook用)
		/// </summary>
		void SetVptrTable(const VptrTable* vt);

		IStringHasher() = delete;
		IStringHasher(const IStringHasher&) = delete;
		IStringHasher(IStringHasher&&) = delete;
		IStringHasher& operator=(const IStringHasher&) = delete;
		IStringHasher& operator=(IStringHasher&&) = delete;
	};

	/// <summary>
	/// 文件路径Hash接口
	/// </summary>
	class PathNameHasher : public IStringHasher
	{
	private:
		//IStringHasher Base offset:0x00
		tjs_uint8 mSaltData[0x10]; //盐数据 offset:0x0C

	public:
		PathNameHasher() = delete;
		PathNameHasher(const PathNameHasher&) = delete;
		PathNameHasher(PathNameHasher&&) = delete;
		PathNameHasher& operator=(const PathNameHasher&) = delete;
		PathNameHasher& operator=(PathNameHasher&&) = delete;
	};

	/// <summary>
	/// 文件名Hash接口
	/// </summary>
	class FileNameHasher : public IStringHasher
	{
	private:
		//IStringHasher Base offset:0x00
		tjs_uint8 mSaltData[0x20]; //盐数据 offset:0x0C

	public:
		FileNameHasher() = delete;
		FileNameHasher(const FileNameHasher&) = delete;
		FileNameHasher(FileNameHasher&&) = delete;
		FileNameHasher& operator=(const FileNameHasher&) = delete;
		FileNameHasher& operator=(FileNameHasher&&) = delete;
	};


	//Cxdec插件储存管理 size = 0x60
	//CompoundStorageMedia : public iTVPStorageMedia
	struct CompoundStorageMedia
	{
		void* VptrTable;					//0x00
		int RefCount;						//0x04
		DWORD field_8;						//0x08
		tTJSString PreFix;					//0x0C  资源前缀
		tTJSString HasherSeed;				//0x10	Hash盐
		CRITICAL_SECTION CriticalSection;	//0x14	临界区锁

		//0x2C  已加载资源表
		//std::unorder_map<tTJSString, FileEntry> ArchiveEntryLoaded
		BYTE Reserve[0x20];

		//0x4C	已加载封包名字
		//std::vector<tTJSString>  PackageLoaded
		tTJSString* Start;
		tTJSString* Position;
		tTJSString* End;

		//0x58	路径Hash对象
		IStringHasher* PathNameHasher;

		//0x5C  文件名Hash对象
		IStringHasher* FileNameHasher;
	};


	class HashCore
	{
	public:
		using tCreateCompoundStorageMedia = tjs_error(__cdecl*)(CompoundStorageMedia** retTVPStorageMedia, tTJSVariant* tjsVarPrefix, int argc, void* argv);

		//Hook TVPStorageMedia创建
		friend tjs_error __cdecl HookCreateCompoundStorageMedia(CompoundStorageMedia** retTVPStorageMedia, tTJSVariant* tjsVarPrefix, int argc, void* argv);
		//Hook 文件夹路径计算
		friend tjs_int __fastcall HookPathNameHasherCalcute(IStringHasher* thisObj, void* unusedEdx, tTJSVariant* hashValueRet, const tTJSString* str, const tTJSString* seed);
		//Hook 文件名计算
		friend tjs_int __fastcall HookFileNameHasherCalcute(IStringHasher* thisObj, void* unusedEdx, tTJSVariant* hashValueRet, const tTJSString* str, const tTJSString* seed);

	private:
		static constexpr const wchar_t FolderName[] = L"StringHashDumper_Output";			//字符串Dump文件夹
		static constexpr const wchar_t DirectoryHashFileName[] = L"DirectoryHash.log";		//Dump路径日志文件名
		static constexpr const wchar_t FileNameHashFileName[] = L"FileNameHash.log";		//Dump文件名日志文件名
		static constexpr const wchar_t UniversalFileName[] = L"Universal.log";				//通用日志文件名

	private:
		static constexpr const char CreateCompoundStorageMediaSignature[] = "\x55\x8B\xEC\x6A\xFF\x68\x2A\x2A\x2A\x2A\x64\xA1\x00\x00\x00\x00\x50\x83\xEC\x08\x56\xA1\x2A\x2A\x2A\x2A\x33\xC5\x50\x8D\x45\xF4\x64\xA3\x00\x00\x00\x00\xA1\x2A\x2A\x2A\x2A\x85\xC0\x75\x12\x68\x2A\x2A\x2A\x2A\xE8\x2A\x2A\x2A\x2A\x83\xC4\x04\xA3\x2A\x2A\x2A\x2A\x8B\x75\x0C\x56\xFF\xD0\x83\xF8\x02\x74\x2A\xB8\x15\xFC\xFF\xFF\x8B\x4D\xF4\x64\x89\x0D\x00\x00\x00\x00\x59\x5E\x8B\xE5\x5D\xC3";
		static constexpr const wchar_t Split[] = L"##YSig##";		//输出分隔符

		std::wstring mDumperDirectoryPath;			//Dump输出目录
		Log::Logger mDirectoryHashLogger;			//文件夹Hash日志
		Log::Logger mFileNameHashLogger;			//文件名Hash日志
		Log::Logger mUniversalLogger;				//通用日志

	private:
		HashCore();
		HashCore(const HashCore&) = delete;
		HashCore(HashCore&&) = delete;
		HashCore& operator=(const HashCore&) = delete;
		HashCore& operator=(HashCore&&) = delete;
		~HashCore();

	public:

		/// <summary>
		/// 设置Hash表输出路径
		/// </summary>
		/// <param name="directory">文件夹绝对路径</param>
		void SetOutputDirectory(const std::wstring& directory);

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
		/// 获取对象实例
		/// </summary>
		static HashCore* GetInstance();
		/// <summary>
		/// 释放
		/// </summary>
		static void Release();
	};
}


