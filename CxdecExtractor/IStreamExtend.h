
#pragma once

#include <Windows.h>

static class IStreamEx
{
public:
	/// <summary>
	/// 获取流长度
	/// </summary>
	/// <param name="stream"></param>
	__declspec(noinline)
	static ULONGLONG WINAPI Length(IStream* stream)
	{
		LARGE_INTEGER pos{ };
		pos.QuadPart = (LONGLONG)IStreamEx::Position(stream);

		ULARGE_INTEGER size;
		stream->Seek(LARGE_INTEGER{ 0 }, STREAM_SEEK_END, &size);
		stream->Seek(pos, STREAM_SEEK_SET, NULL);

		return size.QuadPart;
	}
	/// <summary>
	/// 获取当前流位置
	/// </summary>
	/// <param name="stream"></param>
	__declspec(noinline)
	static ULONGLONG WINAPI Position(IStream* stream)
	{
		ULARGE_INTEGER pos;
		stream->Seek(LARGE_INTEGER{ 0 }, STREAM_SEEK_CUR, &pos);
		return pos.QuadPart;
	}
	/// <summary>
	/// 设置当前流位置
	/// </summary>
	/// <param name="stream"></param>
	/// <param name="offset"></param>
	/// <param name="seekMode"></param>
	__declspec(noinline)
	static void WINAPI Seek(IStream* stream, LONGLONG offset, DWORD seekMode)
	{
		LARGE_INTEGER move{ };
		move.QuadPart = offset;
		stream->Seek(move, seekMode, NULL);
	}
	/// <summary>
	/// 读取流
	/// </summary>
	/// <param name="stream"></param>
	/// <param name="buffer"></param>
	/// <param name="length"></param>
	__declspec(noinline)
	static ULONG WINAPI Read(IStream* stream, void* buffer, ULONG length)
	{
		ULONG readLength = 0;
		stream->Read(buffer, length, &readLength);
		return readLength;
	}
};


