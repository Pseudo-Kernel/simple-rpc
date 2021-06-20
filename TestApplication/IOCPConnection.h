#pragma once

#include "IOCPBase.h"
#include "IODispatchHandler.h"
#include "RingBuffer.h"
#include "IOCPBufferList.h"

namespace IOCP
{

enum class IOCPResultCode : uint32_t
{
	Successful = 0,
	ErrorBufferFull,
	ErrorSendFailure,
	ErrorRecvFailure,
};

class IOCPConnection
{
public:
	IOCPConnection(SOCKET Socket, const IODispatchHandler *Dispatch, uint32_t SendBufferCapacity, uint32_t RecvBufferCapacity, uint32_t RecvBufferLengthPerRecvCall);
	~IOCPConnection();

	IOCPResultCode Send(uint8_t *Buffer, uint32_t Size, uint32_t *SizeQueued);

private:

	bool IssueSendCompleted();
	bool IssueRecvCompleted();

	IOCPResultCode SendCompletion(IOCP_OVERLAPPED_EXTENSION *OverlappedExtension);
	IOCPResultCode ReceiveCompletion(IOCP_OVERLAPPED_EXTENSION *OverlappedExtension);


	SOCKET SocketFd_;								// Socket file descriptor.

	std::recursive_mutex SendBufferMutex_;			// Mutex for send operation.
	RingBuffer SendBuffer_;							// Ring buffer which stores data to send.
	IOCPBufferList SendBufferList_;					// Buffer list for WSASend().
	std::atomic_uint64_t SendSequenceNumber_;		// Send sequence number.

	std::recursive_mutex RecvBufferMutex_;			// Mutex for receive operation.
	RingBuffer RecvBuffer_;							// Ring buffer which stores data received.
	IOCPBufferList RecvBufferList_;					// Buffer list for WSARecv().
	std::atomic_uint64_t RecvSequenceNumber_;		// Receive sequence number.
	uint32_t RecvBufferLengthPerRecvCall_;			// Receive buffer length per WSARecv() call (WSABUF::len)

	uint64_t DebugTraceTick_;

	IOCP_OVERLAPPED_EXTENSION OverlappedIssueRead_;
	IOCP_OVERLAPPED_EXTENSION OverlappedIssueWrite_;

	const IODispatchHandler *Dispatch_;

	friend class IOCPConnectionManager;
};

}

