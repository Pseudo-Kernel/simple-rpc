
#include <stdio.h>
#include "IOCPClasses.h"

// experimental
#include "SRPCBase.h"
#include "ThreadPool.h"
#include "SRPCFrameHandler.h"

#include <initializer_list>
#include <Windows.h>

void ring_buffer_test()
{
	IOCP::RingBuffer Rb(1000);
	uint8_t buf[512];

	uint32_t alloc_size = 0x1000000;
	auto rdptr = std::make_unique<uint8_t[]>(alloc_size + 0x1000);
	auto wrptr = std::make_unique<uint8_t[]>(alloc_size + 0x1000);
	auto read_bytes = rdptr.get();
	auto write_bytes = wrptr.get();

	srand(static_cast<unsigned int>(__rdtsc()));

	uint32_t read_size_total = 0;
	uint32_t write_size_total = 0;
	while (write_size_total < alloc_size)
	{
		uint32_t size = rand() % 320;

		if (rand() & 2)
		{
			for (uint32_t i = 0; i < size; i++)
				buf[i] = rand();

			size = Rb.Write(buf, size);
			memcpy(write_bytes + write_size_total, buf, size);

			write_size_total += size;
		}
		else
		{
			size = Rb.Read(read_bytes + read_size_total, size);
			IOCP::Assert(size == Rb.Release(size));
			read_size_total += size;
		}
	}


	while (Rb.GetReadableCount())
	{
		uint32_t size = rand() % 32;

		size = Rb.Read(read_bytes + read_size_total, size);
		read_size_total += size;
	}

	IOCP::Trace("total read %d, total write %d\n", read_size_total, write_size_total);
	if (read_size_total != write_size_total)
	{
		IOCP::Trace("!! mismatched read/write size\n");
	}

	IOCP::Trace("compare result = %d\n",
		memcmp(read_bytes, write_bytes, read_size_total));
}

SOCKET Connect(const char *Address, unsigned short Port)
{
	SOCKET Socket = WSASocket(AF_INET, SOCK_STREAM, IPPROTO_TCP, nullptr, 0, WSA_FLAG_OVERLAPPED);

	if (Socket == INVALID_SOCKET)
		return INVALID_SOCKET;

	IN_ADDR AddressBuffer;
	if (inet_pton(AF_INET, Address, &AddressBuffer) != 1)
	{
		closesocket(Socket);
		return INVALID_SOCKET;
	}

	SOCKADDR_IN Target;
	Target.sin_addr = AddressBuffer;
	Target.sin_family = AF_INET;
	Target.sin_port = htons(Port);

	int Result = WSAConnect(
		Socket,
		reinterpret_cast<const sockaddr *>(&Target),
		sizeof(Target),
		nullptr,
		nullptr,
		nullptr,
		nullptr);

	int LastError = WSAGetLastError();
	if (Result == SOCKET_ERROR && (LastError != WSAEWOULDBLOCK))
	{
		closesocket(Socket);
		return INVALID_SOCKET;
	}

	return Socket;
}


void TestThread(bool ServerMode, char *IPAddress, unsigned short Port, int ConnectionCount)
{
	IOCP::IOCPConnectionManager ConnectionManager;
	SOCKET s = INVALID_SOCKET;

	ConnectionManager.Initialize();

	if (ServerMode)
	{
		IOCP::Trace("Starting server...\n");

		IOCP::TCPListener Listener;
		Listener.BeginListen(Port);

		while (true)
		{
			IOCP::Trace("Wait for client accept...\n");

			s = Listener.WaitAccept();
			IOCP::Trace("Accept returns 0x%x\n", s);

			if (s == INVALID_SOCKET)
				continue;

			auto Connection = ConnectionManager.AddConnection(s, nullptr, 0x100000, 0x100000, 0x10000);
			//			const_cast<IOCP::IOCPConnection *>(Connection)->Send((uint8_t *)"1234567890", 10, nullptr);

			Sleep(1);
		}
	}
	else
	{
		static uint8_t SendBuffer[0x1000];

		IOCP::Trace("Starting client...\n");

		std::vector<IOCP::IOCPConnection*> ConnectionObjects;

		while (ConnectionObjects.size() < ConnectionCount)
		{
			s = Connect(IPAddress, Port);
			if (s == INVALID_SOCKET)
			{
				Sleep(1);
				continue;
			}

			// Add connection to manager.
			auto Connection = ConnectionManager.AddConnection(s, nullptr, 0x100000, 0x100000, 0x10000);
			if (Connection)
			{
				IOCP::Trace("Connection established %zd\n", ConnectionObjects.size());
				ConnectionObjects.push_back(Connection);
			}
			else
				closesocket(s);
		}

		IOCP::Trace("Starting send test...\n");

		while (true)
		{
			uint32_t Index = rand() % ConnectionObjects.size();
			auto Connection = ConnectionObjects[Index];

			uint32_t Size = rand() % sizeof(SendBuffer);
			auto Result = Connection->Send(SendBuffer, Size, nullptr);

			if (Result != IOCP::IOCPResultCode::Successful)
				Sleep(1);
		}
	}
}

void usage(char **argv)
{
	IOCP::Trace("usage: %s <opt> <mode> [<ip> <port>]\n", argv[0]);
	IOCP::Trace("opt: v (verbose), vi (verbose with interval)\n");
	IOCP::Trace("mode: 0 (server), 1 (client), else (self-test).\n");
	IOCP::Trace("ip: address of ip.\n");
	IOCP::Trace("port: port number.\n\n");
}


void threadpool_test()
{
	using namespace std::chrono_literals;

	SRPC::ThreadPool Pool(2);

	auto Sum = [](int start, int end)
	{
		int sum = 0;

		for (int i = start; i < end; i++)
		{
			std::this_thread::sleep_for(10ms);
			sum += i;
		}

		IOCP::Trace("Thread %d, result %d\n", GetCurrentThreadId(), sum);
		return sum;
	};

	decltype(Pool.Queue(Sum, 1, 100)) FutureList[] = {
		Pool.Queue(Sum, 1, 100),
		Pool.Queue(Sum, 101, 200),
		Pool.Queue(Sum, 201, 300),
	};

	for (int i = 0; i < 1000; i++)
	{
		Pool.Queue([]()
		{
			std::this_thread::sleep_for(50ms);
		});
	}

	auto Future5 = Pool.Queue(Sum, 0, 400);
	auto Future6 = Pool.Queue(Sum, 0, 400);
//	auto Future7 = Pool.Queue(Sum, 0, 40000);
//	auto Future8 = Pool.Queue(Sum, 0, 40000);

	int result = 0;

	for (auto& it : FutureList)
		result += it.get();

	printf("total sum = %d\n", result);


}


int main(int argc, char **argv)
{
	threadpool_test();

	printf("end\n");
	std::this_thread::sleep_for(std::chrono::seconds(99999));
	exit(0);



	int Mode = 1; // 0 - server, 1 - client, else - self-test
	char IPAddress[32] = "127.0.0.1";
	int Port = 9009;
	int ConnectionCount = 100;

#if 1
	if (argc >= 2)
	{
		Mode = atoi(argv[1]);

		if (argc >= 4)
		{
			// ip/port missing
			strcpy_s(IPAddress, argv[2]);
			Port = atoi(argv[3]);
		}
		else
		{
			IOCP::Trace("using default %s:%d ...\n", IPAddress, Port);
		}
	}
#endif

	WSADATA WSAData;
	WSAStartup(MAKEWORD(2, 2), &WSAData);
	
	std::thread server_thread;
	std::thread client_thread;

	IOCP::Trace("target = %s:%d\n\n", IPAddress, Port);

	if (Mode != 1)
	{
		server_thread = std::thread([&IPAddress, Port, ConnectionCount]()
		{
			TestThread(true, IPAddress, Port, ConnectionCount);
		});
	}
	
	if (Mode != 0)
	{
		client_thread = std::thread([&IPAddress, Port, ConnectionCount]()
		{
			TestThread(false, IPAddress, Port, ConnectionCount);
		});
	}

	if (server_thread.joinable())
		server_thread.join();

	if (client_thread.joinable())
		client_thread.join();

	return 0;
}

