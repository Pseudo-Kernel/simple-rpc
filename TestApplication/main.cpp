
#include <stdio.h>
#include "IOCPClasses.h"

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

	srand(__rdtsc());

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

	SOCKADDR_IN Target;
	Target.sin_addr.S_un.S_addr = inet_addr(Address);
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

void echo_thread()
{
	SOCKET s = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);

	if (s == INVALID_SOCKET)
	{
		IOCP::Trace("[ECHO] socket error\n");
		return;
	}

	SOCKADDR_IN inaddr;
	inaddr.sin_addr.S_un.S_addr = inet_addr("192.168.5.87");// inet_addr("192.168.127.5");
	inaddr.sin_family = AF_INET;
	inaddr.sin_port = htons(8401);
	//	if (bind(s, (const sockaddr *)&inaddr, sizeof(inaddr)) == SOCKET_ERROR)
	//	{
	//		IOCP::Trace("[ECHO] bind error\n");
	//		closesocket(s);
	//		return;
	//	}

	while (true)
	{
		if (connect(s, (const sockaddr *)&inaddr, sizeof(inaddr)) == 0)
			break;

		IOCP::Trace("[ECHO] connect error, retrying...\n");

		Sleep(1000);
	}

	IOCP::Trace("[ECHO] connect success\n");

	uint32_t buf[0x1000];
	for (int i = 0; i < std::size(buf); i++)
		buf[i] = rand();

	for (uint64_t i = 0; i < 0xffffffffffffUI64; i++)
	{
		//int buf_len = sprintf(buf, "send_test_%llu-%llu| ", i, __rdtsc());
		int buf_len = std::size(buf);

		int sent_len = 0;
		do
		{
			int len = send(s, (char *)buf + sent_len, buf_len - sent_len, 0);
			if (len < 0)
				break;

			sent_len += len;
		} while (sent_len < buf_len);
	}

	closesocket(s);
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

int main(int argc, char **argv)
{
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

