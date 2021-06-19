#include "TCPListener.h"

namespace IOCP
{

TCPListener::TCPListener() : ListenerSocket_(INVALID_SOCKET)
{
}


TCPListener::~TCPListener()
{
	EndListen();
}

bool TCPListener::BeginListen(int Port)
{
	addrinfo Hints = { 0 };
	addrinfo *AddrInfo = NULL;

	char PortString[20];

	_itoa_s(Port, PortString, 10);

	Hints.ai_flags = AI_PASSIVE;
	Hints.ai_family = AF_INET;
	Hints.ai_socktype = SOCK_STREAM;
	Hints.ai_protocol = IPPROTO_IP;

	if (getaddrinfo(nullptr, PortString, &Hints, &AddrInfo))
		return false;

	SOCKET Socket = WSASocket(AddrInfo->ai_family, AddrInfo->ai_socktype, AddrInfo->ai_protocol,
		NULL, 0, WSA_FLAG_OVERLAPPED);

	if (Socket == INVALID_SOCKET)
	{
		freeaddrinfo(AddrInfo);
		return false;
	}

	if (bind(Socket, AddrInfo->ai_addr, AddrInfo->ai_addrlen) == SOCKET_ERROR)
	{
		closesocket(Socket);
		freeaddrinfo(AddrInfo);
		return false;
	}

	if (listen(Socket, 5) == SOCKET_ERROR)
	{
		closesocket(Socket);
		freeaddrinfo(AddrInfo);
		return false;
	}

	ListenerSocket_ = Socket;

	return true;
}

bool TCPListener::EndListen()
{
	closesocket(ListenerSocket_);
	return true;
}

SOCKET TCPListener::WaitAccept()
{
	SOCKET Socket = WSAAccept(ListenerSocket_, nullptr, nullptr, nullptr, 0);
	return Socket;
}

}
