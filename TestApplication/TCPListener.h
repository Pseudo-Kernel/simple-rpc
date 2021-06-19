#pragma once

#include "IOCPBase.h"

namespace IOCP
{

class TCPListener
{
public:
	TCPListener();
	~TCPListener();

	bool BeginListen(int Port);
	bool EndListen();

	SOCKET WaitAccept();

private:
	SOCKET ListenerSocket_;
};

}

