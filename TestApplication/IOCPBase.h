#pragma once

#include <cstdint>
#include <algorithm>
#include <vector>
#include <map>
#include <memory>
#include <thread>
#include <mutex>
#include <atomic>
//nclude <shared_mutex>

#ifndef WIN32_LEAN_AND_MEAN
#define	WIN32_LEAN_AND_MEAN
#endif
#include <Windows.h>

#include <winsock2.h>
#include <WS2tcpip.h>

#pragma comment(lib, "ws2_32.lib")


namespace IOCP
{
	enum class OperationType : int
	{
		Send,
		Recv,
	};

	struct IOCP_OVERLAPPED_EXTENSION // Must compatible with OVERLAPPED structure
	{
		OVERLAPPED Overlapped; // WSASend/WSARecv references it
		WSABUF Buffer;
		OperationType Operation;
		uint32_t Flags;
		uint64_t SequenceNumber;
	};


	inline void Assert(bool Condition)
	{
		if (!Condition)
			__debugbreak();
	}

	static void Trace(const char *Format, ...)
	{
		char Buffer[512];
		va_list list;
		va_start(list, Format);
		int Result = vsprintf_s(Buffer, Format, list);
		va_end(list);

		HANDLE StdOut = GetStdHandle(STD_OUTPUT_HANDLE);
		DWORD BytesWritten = 0;
		WriteConsoleA(StdOut, Buffer, static_cast<DWORD>(Result), &BytesWritten, nullptr);
	}

}




