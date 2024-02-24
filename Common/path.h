// path.h

#pragma once

#include <string>

namespace Path
{
	std::string GetFileName(const std::string& path);
	std::wstring GetFileName(const std::wstring& path);
	std::string GetFileNameWithoutExtension(const std::string& path);
	std::wstring GetFileNameWithoutExtension(const std::wstring& path);
	std::string GetDirectoryName(const std::string& path);
	std::wstring GetDirectoryName(const std::wstring& path);
	std::string GetExtension(const std::string& path);
	std::wstring GetExtension(const std::wstring& path);
	std::string ChangeExtension(const std::string& path, const std::string& ext);
	std::wstring ChangeExtension(const std::wstring& path, const std::wstring& ext);
	std::string GetFullPath(const std::string& path);
	std::wstring GetFullPath(const std::wstring& path);
}
//判断文件夹是否存在
bool CheckDirectoryExist(const std::wstring& dirPath);
//判断文件是否存在
bool CheckFileExist(const std::wstring& filePath);
//创建文件夹
void FullCreateDirectoryW(const std::wstring& dirPath);
