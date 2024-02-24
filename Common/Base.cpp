#include "Base.h"


//字节转16进制字符串
std::wstring BytesToHexStringW(byte* data, int length)
{
	wchar_t hexStringW[32] = L"0123456789ABCDEF";

	std::wstring s;
	for (int index = 0; index < length; index++)
	{
		s += hexStringW[(data[index] & 0xF0) >> 4];
		s += hexStringW[(data[index] & 0x0F) >> 0];
	}
	return s;
}


