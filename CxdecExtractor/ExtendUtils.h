#pragma once

#include <windows.h>
#include "detours.h"
namespace Engine
{
	namespace HookUtils
	{
		class InlineHook
		{
		public:
			InlineHook() = delete;
			InlineHook(const InlineHook&) = delete;
			InlineHook(InlineHook&&) = delete;
			InlineHook& operator=(const InlineHook&) = delete;
			InlineHook& operator=(InlineHook&&) = delete;
			~InlineHook() = delete;


			template<class T>
			static void Hook(T& OriginalFunction, T DetourFunction)
			{
				DetourUpdateThread(GetCurrentThread());
				DetourTransactionBegin();
				DetourAttach(&(PVOID&)OriginalFunction, (PVOID&)DetourFunction);
				DetourTransactionCommit();
			}

			template<class T>
			static void UnHook(T& OriginalFunction, T DetourFunction)
			{
				DetourUpdateThread(GetCurrentThread());
				DetourTransactionBegin();
				DetourDetach(&(PVOID&)OriginalFunction, (PVOID&)DetourFunction);
				DetourTransactionCommit();
			}
		};
	}

	namespace StreamUtils
	{
		class IStreamEx
		{
		public:
			IStreamEx() = delete;
			IStreamEx(const IStreamEx&) = delete;
			IStreamEx(IStreamEx&&) = delete;
			IStreamEx& operator=(const IStreamEx&) = delete;
			IStreamEx& operator=(IStreamEx&&) = delete;
			~IStreamEx() = delete;

			/// <summary>
			/// 获取流长度
			/// </summary>
			/// <param name="stream"></param>
			static ULONGLONG WINAPI Length(IStream* stream)
			{
				LARGE_INTEGER pos{ };
				pos.QuadPart = (LONGLONG)IStreamEx::Position(stream);

				ULARGE_INTEGER size;
				stream->Seek(LARGE_INTEGER{ }, STREAM_SEEK_END, &size);
				stream->Seek(pos, STREAM_SEEK_SET, nullptr);

				return size.QuadPart;
			}
			/// <summary>
			/// 获取当前流位置
			/// </summary>
			/// <param name="stream"></param>
			static ULONGLONG WINAPI Position(IStream* stream)
			{
				ULARGE_INTEGER pos;
				stream->Seek(LARGE_INTEGER{ }, STREAM_SEEK_CUR, &pos);
				return pos.QuadPart;
			}
			/// <summary>
			/// 设置当前流位置
			/// </summary>
			/// <param name="stream"></param>
			/// <param name="offset"></param>
			/// <param name="seekMode"></param>
			static void WINAPI Seek(IStream* stream, LONGLONG offset, DWORD seekMode)
			{
				LARGE_INTEGER move{ };
				move.QuadPart = offset;
				stream->Seek(move, seekMode, nullptr);
			}
			/// <summary>
			/// 读取流
			/// </summary>
			/// <param name="stream"></param>
			/// <param name="buffer"></param>
			/// <param name="length"></param>
			static ULONG WINAPI Read(IStream* stream, void* buffer, ULONG length)
			{
				ULONG readLength = 0ul;
				stream->Read(buffer, length, &readLength);
				return readLength;
			}
		};
	}
}



