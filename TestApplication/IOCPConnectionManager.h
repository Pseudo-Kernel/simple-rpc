#pragma once

#include "IOCPBase.h"
#include "ProtocolInterface.h"
#include "RingBuffer.h"
#include "IOCPBufferList.h"
#include "IOCPConnection.h"

namespace IOCP
{

class IOCPConnectionManager
{
public:
	IOCPConnectionManager();
	IOCPConnectionManager(uint32_t WorkersCount);
	~IOCPConnectionManager();

	bool Initialize();
	bool Shutdown();

	IOCPConnection *AddConnection(SOCKET Socket, const IODispatchHandler *Dispatch, uint32_t SendBufferCapacity, uint32_t RecvBufferCapacity, uint32_t RecvBufferLengthPerRecvCall);

private:
	std::recursive_mutex Mutex_;
	std::map<uintptr_t, std::unique_ptr<IOCPConnection>> ConnectionMap_;
	std::unique_ptr<std::thread[]> Workers_;
	uint32_t WorkersCount_;
	HANDLE IoCompletionPort_;
	bool Initialized_;
};

}
