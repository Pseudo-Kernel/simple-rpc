#pragma once

#include <cstdint>
#include <algorithm>
#include <vector>
#include <map>
#include <memory>
#include <chrono>
#include <thread>
#include <mutex>
#include <atomic>
//nclude <shared_mutex>

#include <queue>
#include <future>

#ifndef WIN32_LEAN_AND_MEAN
#define	WIN32_LEAN_AND_MEAN
#endif
#include <Windows.h>


namespace SRPC
{
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




