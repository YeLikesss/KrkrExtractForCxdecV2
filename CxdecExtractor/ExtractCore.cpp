#include "ExtractCore.h"
#include "pe.h"
#include "file.h"
#include "directory.h"
#include "path.h"
#include "stringhelper.h"
#include "ExtendUtils.h"

namespace Engine
{
	//**********ExtractCore***********//
	ExtractCore::ExtractCore()
	{
		this->mCreateStreamFunc = nullptr;
		this->mCreateIndexFunc = nullptr;
	}

	ExtractCore::~ExtractCore()
	{
	}

	void ExtractCore::SetOutputDirectory(const std::wstring& directory)
	{
		this->mExtractDirectoryPath = Path::Combine(directory, ExtractCore::ExtractorOutFolderName);
	}

	void ExtractCore::SetLoggerDirectory(const std::wstring& directory)
	{
		std::wstring path = Path::Combine(directory, ExtractCore::ExtractorLogFileName);

		File::Delete(path);
		this->mLogger.Open(path.c_str());
	}

	void ExtractCore::Initialize(PVOID codeVa, DWORD codeSize)
	{
		PVOID createStream = PE::SearchPattern(codeVa, codeSize, ExtractCore::CreateStreamSignature, sizeof(ExtractCore::CreateStreamSignature) - 1u);
		PVOID createIndex = PE::SearchPattern(codeVa, codeSize, ExtractCore::CreateIndexSignature, sizeof(ExtractCore::CreateIndexSignature) - 1u);

		if (createStream && createIndex)
		{
			this->mCreateStreamFunc = (tCreateStream)createStream;
			this->mCreateIndexFunc = (tCreateIndex)createIndex;
		}
	}

	bool ExtractCore::IsInitialized()
	{
		return this->mCreateStreamFunc && this->mCreateIndexFunc;
	}

	void ExtractCore::ExtractPackage(const std::wstring& packageFileName)
	{
		if (!this->IsInitialized())
		{
			::MessageBoxW(nullptr, L"未初始化CxdecV2接口\n请检查是否为无DRM的Wamsoft Hxv4加密游戏", L"错误", MB_OK);
			return;
		}

		tTJSString tjsXp3PackagePath = TVPGetAppPath() + packageFileName.c_str();   //获取封包标准路径
		std::vector<FileEntry> entries = std::vector<FileEntry>();

		this->GetEntries(tjsXp3PackagePath, entries);
		if (!entries.empty())
		{
			Directory::Create(this->mExtractDirectoryPath);     //创建资源提取导出文件夹

			std::wstring packageName = Path::GetFileNameWithoutExtension(packageFileName);				//封包名
			std::wstring extractOutput = Path::Combine(this->mExtractDirectoryPath, packageName);		//输出文件夹
			std::wstring fileTableOutput = extractOutput + L".alst";									//文件表输出路径

			File::Delete(fileTableOutput);
			Log::Logger fileTable = Log::Logger(fileTableOutput.c_str());

			//写UTF-16LE bom头
			{
				WORD bom = 0xFEFF;
				fileTable.WriteData(&bom, sizeof(bom));
			}

			for (FileEntry& entry : entries)
			{
				std::wstring dirHash = StringHelper::BytesToHexStringW(entry.DirectoryPathHash, sizeof(entry.DirectoryPathHash));  //文件夹Hash字符串形式
				std::wstring fileNameHash = StringHelper::BytesToHexStringW(entry.FileNameHash, sizeof(entry.FileNameHash));     //文件名Hash字符串形式
				std::wstring arcOutputPath = extractOutput + L"\\" + dirHash + L"\\" + fileNameHash;				//该资源导出路径

				if (IStream* stream = this->CreateStream(entry, packageFileName))
				{
					//写表  dirHash[Sign]dirHash[Sign]fileHash[Sign]fileHash[NewLine]
					fileTable.WriteUnicode(L"%s%s%s%s%s%s%s\r\n",
										   dirHash.c_str(),
										   ExtractCore::Split,
										   dirHash.c_str(),
										   ExtractCore::Split,
										   fileNameHash.c_str(),
										   ExtractCore::Split,
										   fileNameHash.c_str());
					//解包
					this->ExtractFile(stream, arcOutputPath);
					stream->Release();
				}
				else
				{
					this->mLogger.WriteLine(L"File Not Exist: %s/%s/%s", packageName.c_str(), dirHash.c_str(), fileNameHash.c_str());
				}
			}
			::MessageBoxW(nullptr, (packageFileName + L"提取成功").c_str(), L"信息", MB_OK);
			fileTable.Close();
		}
		else
		{
			::MessageBoxW(nullptr, L"请选择正确的XP3封包", L"错误", MB_OK);
		}
	}

	void ExtractCore::GetEntries(const tTJSString& xp3PackagePath, std::vector<FileEntry>& retValue)
	{
		retValue.clear();

		//加载文件表
		tTJSVariant tjsEntries = tTJSVariant();
		tTJSVariant tjsPackagePath = tTJSVariant(xp3PackagePath);
		this->mCreateIndexFunc(&tjsEntries, &tjsPackagePath);

		if (tjsEntries.Type() == tvtObject)
		{
			tTJSVariantClosure& dirEntriesObj = tjsEntries.AsObjectClosureNoAddRef();

			//读取数组项数
			tTJSVariant tjsCount = tTJSVariant();
			tjs_int count = 0;
			dirEntriesObj.PropGet(TJS_MEMBERMUSTEXIST, L"count", NULL, &tjsCount, nullptr);
			count = tjsCount.AsInteger();

			constexpr tjs_int DirectoryItemSize = 2;
			//获取文件夹项数 (占KR数组2项)
			//文件夹路径Hash
			//子文件数组
			tjs_int dirCount = count / DirectoryItemSize;

			//遍历文件夹
			for (tjs_int di = 0; di < dirCount; ++di)
			{
				//获取文件夹路径Hash与子文件表
				tTJSVariant tjsDirHash = tTJSVariant();
				tTJSVariant tjsFileEntries = tTJSVariant();
				dirEntriesObj.PropGetByNum(TJS_CII_GET, di * DirectoryItemSize + 0, &tjsDirHash, nullptr);
				dirEntriesObj.PropGetByNum(TJS_CII_GET, di * DirectoryItemSize + 1, &tjsFileEntries, nullptr);

				//获取Hash
				tTJSVariantOctet* dirHash = tjsDirHash.AsOctetNoAddRef();

				//获取子文件表数组
				tTJSVariantClosure& fileEntries = tjsFileEntries.AsObjectClosureNoAddRef();

				//获取子文件数组项数
				tjsCount.Clear();
				fileEntries.PropGet(TJS_MEMBERMUSTEXIST, L"count", NULL, &tjsCount, nullptr);
				count = tjsCount.AsInteger();

				constexpr tjs_int FileItemSize = 2;
				//获取文件项数 (占KR数组2项)
				//文件名Hash
				//文件信息
				tjs_int fileCount = count / FileItemSize;

				//遍历子文件
				for (tjs_int fi = 0; fi < fileCount; ++fi)
				{
					//获取文件名Hash与文件信息
					tTJSVariant tjsFileNameHash = tTJSVariant();
					tTJSVariant tjsFileInfo = tTJSVariant();
					fileEntries.PropGetByNum(TJS_CII_GET, fi * FileItemSize + 0, &tjsFileNameHash, nullptr);
					fileEntries.PropGetByNum(TJS_CII_GET, fi * FileItemSize + 1, &tjsFileInfo, nullptr);

					//获取Hash
					tTJSVariantOctet* fileNameHash = tjsFileNameHash.AsOctetNoAddRef();

					//获取文件信息
					tTJSVariantClosure& fileInfo = tjsFileInfo.AsObjectClosureNoAddRef();

					//获取文件信息
					__int64 ordinal = 0i64;
					__int64 key = 0i64;

					tTJSVariant tjsValue = tTJSVariant();
					fileInfo.PropGetByNum(TJS_CII_GET, 0, &tjsValue, nullptr);
					ordinal = tjsValue.AsInteger();

					tjsValue.Clear();
					fileInfo.PropGetByNum(TJS_CII_GET, 1, &tjsValue, nullptr);
					key = tjsValue.AsInteger();

					//解析后的文件表
					FileEntry entry{ };
					memcpy(entry.DirectoryPathHash, dirHash->GetData(), dirHash->GetLength());
					memcpy(entry.FileNameHash, fileNameHash->GetData(), fileNameHash->GetLength());

					entry.Key = key;
					entry.Ordinal = ordinal;

					retValue.push_back(entry);
				}
			}
		}
	}

	IStream* ExtractCore::CreateStream(const FileEntry& entry, const std::wstring& packageName)
	{
		tjs_char fakeName[4]{ };
		entry.GetFakeName(fakeName);

		tTJSString tjsArcPath = TVPGetAppPath();       //获取游戏路径
		tjsArcPath += packageName.c_str();     //添加封包
		tjsArcPath += L">";                    //添加封包分隔符
		tjsArcPath += fakeName;    //添加资源名
		tjsArcPath = TVPNormalizeStorageName(tjsArcPath);

		return this->mCreateStreamFunc(&tjsArcPath, entry.Key, entry.GetEncryptMode());
	}

	void ExtractCore::ExtractFile(IStream* stream, const std::wstring& extractPath)
	{
		unsigned long long size = StreamUtils::IStreamEx::Length(stream);	//获取流长度

		//相对路径
		std::wstring relativePath(&extractPath.c_str()[this->mExtractDirectoryPath.length() + 1u]);
		if (size > 0)
		{
			//创建文件夹
			{
				std::wstring outputDir = Path::GetDirectoryName(extractPath);
				if (!outputDir.empty())
				{
					Directory::Create(outputDir.c_str());
				}
			}

			std::vector<uint8_t> buffer;
			bool success = false;

			if (ExtractCore::TryDecryptText(stream, buffer))  //尝试解密SimpleEncrypt
			{
				success = true;
			}
			else
			{
				//普通资源
				buffer.resize(size);

				stream->Seek(LARGE_INTEGER{ }, STREAM_SEEK_SET, nullptr);
				if (StreamUtils::IStreamEx::Read(stream, buffer.data(), size) == size)
				{
					success = true;
				}
			}

			if (success && !buffer.empty())
			{
				//保存文件
				if (File::WriteAllBytes(extractPath, buffer.data(), buffer.size()))
				{
					this->mLogger.WriteLine(L"Extract Successed: %s", relativePath.c_str());
				}
				else
				{
					this->mLogger.WriteLine(L"Write Error: %s", relativePath.c_str());
				}
			}
			else
			{
				this->mLogger.WriteLine(L"Invaild File: %s", relativePath.c_str());
			}
			stream->Seek(LARGE_INTEGER{ }, STREAM_SEEK_SET, nullptr);
		}
		else
		{
			this->mLogger.WriteLine(L"Empty File: %s", relativePath.c_str());
		}
	}


	bool ExtractCore::TryDecryptText(IStream* stream, std::vector<uint8_t>& output)
	{
		uint8_t mark[2]{ };
		StreamUtils::IStreamEx::Read(stream, mark, 2ul);

		if (mark[0] == 0xfe && mark[1] == 0xfe)   //检查加密标记头
		{
			uint8_t mode;
			StreamUtils::IStreamEx::Read(stream, &mode, 1ul);

			if (mode != 0 && mode != 1 && mode != 2)  //识别模式
			{
				return false;
			}

			ZeroMemory(mark, sizeof(mark));
			StreamUtils::IStreamEx::Read(stream, mark, 2ul);

			if (mark[0] != 0xff || mark[1] != 0xfe)  //Unicode Bom
			{
				return false;
			}

			if (mode == 2)   //压缩模式
			{
				long long compressed = 0;
				long long uncompressed = 0;
				StreamUtils::IStreamEx::Read(stream, &compressed, sizeof(long long));
				StreamUtils::IStreamEx::Read(stream, &uncompressed, sizeof(long long));

				if (compressed <= 0 || compressed >= INT_MAX || uncompressed <= 0 || uncompressed >= INT_MAX)
				{
					return false;
				}

				std::vector<uint8_t> data = std::vector<uint8_t>((size_t)compressed);

				//读取压缩数据
				if (StreamUtils::IStreamEx::Read(stream, data.data(), compressed) != compressed)
				{
					return false;
				}

				std::vector<uint8_t> buffer = std::vector<uint8_t>((size_t)uncompressed + 2u);

				//写入Bom头
				buffer[0] = mark[0];
				buffer[1] = mark[1];

				uint8_t* dest = buffer.data() + 2;
				unsigned long destLen = (unsigned long)uncompressed;

				if (ZLIB_uncompress(dest, &destLen, data.data(), compressed) || destLen != (unsigned long)uncompressed)
				{
					return false;
				}

				output = std::move(buffer);
				return true;
			}
			else
			{
				long long startpos = StreamUtils::IStreamEx::Position(stream); //解密起始位置
				long long endpos = StreamUtils::IStreamEx::Length(stream); //解密结束位置

				StreamUtils::IStreamEx::Seek(stream, startpos, STREAM_SEEK_SET);      //设置回起始位置

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

				StreamUtils::IStreamEx::Read(stream, buffer.data(), size);  //读取资源

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

				output.resize(sizeToCopy + 2u);

				//写入Unicode Bom
				output[0] = mark[0];
				output[1] = mark[1];

				//回写解密后的数据
				memcpy(output.data() + 2, buffer.data(), sizeToCopy);

				return true;
			}
		}
		return false;
	}
	//================================//
}
