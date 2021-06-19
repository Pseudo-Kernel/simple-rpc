#include "IOCPConnection.h"

namespace IOCP
{

IOCPConnection::IOCPConnection(SOCKET Socket, const IODispatchHandler *Dispatch, uint32_t SendBufferCapacity, uint32_t RecvBufferCapacity, uint32_t RecvBufferLengthPerRecvCall) :
	OverlappedIssueRead_{},
	OverlappedIssueWrite_{},
	SocketFd_(Socket),
	Dispatch_(Dispatch),
	SendBufferList_(OperationType::Send),
	RecvBufferList_(OperationType::Recv),
	SendBuffer_(SendBufferCapacity),
	RecvBuffer_(RecvBufferCapacity),
	SendSequenceNumber_(0),
	RecvSequenceNumber_(0),
	RecvBufferLengthPerRecvCall_(RecvBufferLengthPerRecvCall)
{
	Assert(RecvBufferLengthPerRecvCall >= 0);
	Assert(SendBufferCapacity >= 0);
	Assert(RecvBufferCapacity >= 0);
	Assert(RecvBufferLengthPerRecvCall < RecvBufferCapacity);

	OverlappedIssueRead_.Operation = OperationType::Recv;
	OverlappedIssueWrite_.Operation = OperationType::Send;

	DebugTraceTick_ = 0;
}

IOCPConnection::~IOCPConnection()
{
}

IOCPResultCode IOCPConnection::Send(uint8_t *Buffer, size_t Size, size_t *SizeQueued)
{
	std::lock_guard<decltype(SendBufferMutex_)> Lock(SendBufferMutex_);
	uint64_t SequenceId = SendSequenceNumber_;

	if (SendBuffer_.GetWritableCount() < Size)
		return IOCPResultCode::ErrorBufferFull;

	auto ResultSize = SendBuffer_.Write(Buffer, Size);
	Assert(ResultSize == Size);

	if (SizeQueued)
		*SizeQueued = ResultSize;

	if (!SendBufferList_.Count())
	{
		// Issue send.
		IssueSendCompleted();
	}

	//		bool Result = SendBufferList_.Add(SequenceId, CopyBuffer, Size);
	//
	//		if (!Result)
	//		{
	//			delete CopyBuffer;
	//			return -1;
	//		}

	//		SendSequenceNumber_++;

	return IOCPResultCode::Successful;
}

bool IOCPConnection::IssueSendCompleted()
{
	OverlappedIssueWrite_.Buffer.buf = nullptr;
	OverlappedIssueWrite_.Buffer.len = 0;

	int Result = WSASend(
		SocketFd_,
		&OverlappedIssueWrite_.Buffer,
		1,
		nullptr,
		0,
		&OverlappedIssueWrite_.Overlapped,
		nullptr);

	int LastError = WSAGetLastError();
	if (Result == SOCKET_ERROR && (ERROR_IO_PENDING != LastError))
	{
		Trace("!! Failed to issue WSASend completion, LastError = %d\n", LastError);
		return false;
	}

	return true;
}

bool IOCPConnection::IssueRecvCompleted()
{
	OverlappedIssueRead_.Buffer.buf = nullptr;
	OverlappedIssueRead_.Buffer.len = 0;

	DWORD Flags = 0;
	int Result = WSARecv(
		SocketFd_,
		&OverlappedIssueRead_.Buffer,
		1,
		nullptr,
		&Flags,
		&OverlappedIssueRead_.Overlapped,
		nullptr);

	int LastError = WSAGetLastError();
	if (Result == SOCKET_ERROR && (ERROR_IO_PENDING != LastError))
	{
		Trace("!! Failed to issue WSARecv completion, LastError = %d\n", LastError);
		return false;
	}

	return true;
}


IOCPResultCode IOCPConnection::SendCompletion(IOCP_OVERLAPPED_EXTENSION *OverlappedExtension)
{
	// 
	// Our send completion routine.
	// 

//	Trace("%s\n", __FUNCTION__);

	//
	// 1. Acquire lock for send processing.
	//

	std::lock_guard<decltype(SendBufferMutex_)> Lock(SendBufferMutex_);

	if (OverlappedExtension->Buffer.buf)
	{
		// 
		// 2. Remove completion buffer from send buffer list.
		// 

		auto Buffer = SendBufferList_.Remove(OverlappedExtension->SequenceNumber);
		Assert(Buffer != nullptr);
		Assert(HasOverlappedIoCompleted(&Buffer->OverlappedExtension()->Overlapped));

		// 
		// 3. Release sent bytes from send ring buffer.
		// 

		uint32_t BytesReleased = SendBuffer_.Release(OverlappedExtension->Buffer.len);
		Assert(BytesReleased == OverlappedExtension->Buffer.len);

		uint64_t CurrentTick = GetTickCount64();
		if (DebugTraceTick_ + 5000 < CurrentTick)
		{
			Trace("Removed the buffer object after send [sequence number %llu, size %u]\n",
				Buffer->OverlappedExtension()->SequenceNumber,
				Buffer->OverlappedExtension()->Buffer.len);
			DebugTraceTick_ = CurrentTick;
		}
	}

	// 
	// 4. If data remains in the ring buffer, call WSASend().
	//    Otherwise, do nothing.
	// 

	uint32_t BytesToSend = SendBuffer_.GetReadableCount();

	if (BytesToSend)
	{
		// Split the buffer because buffer is not contiguous

		uint32_t RemainingCountWraparound = SendBuffer_.GetBufferEndPointer() - SendBuffer_.GetReadPointer();
		uint32_t ReadableCount = SendBuffer_.GetReadableCount();

		auto SplitList = {
			// 1st buffer
			std::make_tuple(
				0,
				std::min<uint32_t>(ReadableCount, RemainingCountWraparound),
				SendBuffer_.GetReadPointer()),
			// 2nd buffer
			std::make_tuple(
				1,
				(ReadableCount >= RemainingCountWraparound) ? ReadableCount - RemainingCountWraparound : 0,
				SendBuffer_.GetBufferStartPointer()),
		};

		for (auto& SplitBufferIterator : SplitList)
		{
			uint32_t Count = 0;
			const uint8_t *Pointer = nullptr;
			std::tie(std::ignore, Count, Pointer) = SplitBufferIterator;

			if (!Count)
				continue;

			// Data remains.
			uint64_t SequenceNumber = SendSequenceNumber_;
			auto Buffer = SendBufferList_.Add(SequenceNumber, const_cast<uint8_t *>(Pointer), Count, nullptr);

			int Result = WSASend(
				SocketFd_,
				const_cast<WSABUF *>(&Buffer->OverlappedExtension()->Buffer),
				1,
				nullptr,
				0,
				const_cast<OVERLAPPED *>(&Buffer->OverlappedExtension()->Overlapped),
				nullptr);

			int LastError = WSAGetLastError();
			if (Result == SOCKET_ERROR && (ERROR_IO_PENDING != LastError))
			{
				Trace("!! WSASend failed, LastError = %d\n", LastError);
				return IOCPResultCode::ErrorSendFailure;
			}

			// Lock data.
			uint32_t FlushCount = SendBuffer_.Read(nullptr, Count);
			Assert(FlushCount == Count);

			SendSequenceNumber_++;
		}
	}

	// 
	// 5. Finally, release the lock.
	//    This is automatically done by dtor.
	// 

	return IOCPResultCode::Successful;
}

IOCPResultCode IOCPConnection::ReceiveCompletion(IOCP_OVERLAPPED_EXTENSION *OverlappedExtension)
{
	// 
	// Our receive completion routine.
	// 

//	Trace("%s\n", __FUNCTION__);

	// 
	// 1. Acquire lock for receive processing.
	// 

	std::lock_guard<decltype(RecvBufferMutex_)> Lock(RecvBufferMutex_);

	// 
	// 2. Make sure that received datas are correctly ordered.
	//    This can be done by checking sequence number in receive buffer list.
	// 

	uint32_t CompletedCountContiguous = 0;
	uint64_t BeginSequenceNumber = 0;

	if (OverlappedExtension->Buffer.len > 0)
	{
		Assert(RecvBufferList_.Count() > 0);

		auto BeginIterator = RecvBufferList_.begin();
		BeginSequenceNumber = BeginIterator->first;
		Assert(BeginSequenceNumber <= OverlappedExtension->SequenceNumber);

		for (auto& it : RecvBufferList_)
		{
			Assert(it.first == BeginSequenceNumber + CompletedCountContiguous);

			if (!HasOverlappedIoCompleted(&it.second->OverlappedExtension()->Overlapped))
				break;

			CompletedCountContiguous++;
		}
	}

	// 
	// 3. Write the received data chunks to received ring buffer by sequential order.
	//    If sequence number does not match as expected, stop processing and goto next step.
	// 

	for (uint32_t i = 0; i < CompletedCountContiguous; i++)
	{
		uint64_t TargetSequenceNumber = BeginSequenceNumber + i;

		auto Buffer = RecvBufferList_.Remove();
		Assert(Buffer != nullptr);

		auto TargetOverlapped = Buffer->OverlappedExtension();
		Assert(TargetOverlapped->SequenceNumber == TargetSequenceNumber);

		uint32_t ReceivedLength = TargetOverlapped->Buffer.len;
		if (RecvBuffer_.GetWritableCount() < ReceivedLength)
		{
			Trace("!! Receive ring buffer full, Cannot write chunk [sequence number %llu, size %u]\n",
				TargetSequenceNumber, ReceivedLength);
			break;
		}

		auto BytesWritten = RecvBuffer_.Write(
			reinterpret_cast<uint8_t *>(TargetOverlapped->Buffer.buf),
			TargetOverlapped->Buffer.len);
		Assert(BytesWritten == TargetOverlapped->Buffer.len);

		uint64_t CurrentTick = GetTickCount64();
		if (DebugTraceTick_ + 5000 < CurrentTick)
		{
			Trace("Removed the buffer object after recv [sequence number %llu, size %u]\n",
				TargetSequenceNumber, ReceivedLength);
			DebugTraceTick_ = CurrentTick;
		}
	}

	// 
	// *4. Process the received data in the ring buffer.
	// 

	if (Dispatch_)
	{
		// Split the buffer because buffer is not contiguous

		uint32_t RemainingCountWraparound = RecvBuffer_.GetBufferEndPointer() - RecvBuffer_.GetReadPointer();
		uint32_t ReadableCount = RecvBuffer_.GetReadableCount();

		auto SplitList = {
			// 1st buffer
			std::make_tuple(
				0,
				std::min<uint32_t>(ReadableCount, RemainingCountWraparound),
				RecvBuffer_.GetReadPointer()),
			// 2nd buffer
			std::make_tuple(
				1,
				(ReadableCount >= RemainingCountWraparound) ? ReadableCount - RemainingCountWraparound : 0,
				RecvBuffer_.GetBufferStartPointer()),
		};

		for (auto& SplitBufferIterator : SplitList)
		{
			uint32_t Count = 0;
			const uint8_t *Pointer = nullptr;
			std::tie(std::ignore, Count, Pointer) = SplitBufferIterator;

			if (!Count)
				continue;

			// Call our dispatch handler.
			Assert(const_cast<IODispatchHandler *>(Dispatch_)->ReceiveComplete(Pointer, Count));

			// Advance the read pointer.
			RecvBuffer_.Read(nullptr, Count);
			RecvBuffer_.Release(Count);
		}
	}
	else
	{
		// Flush the buffer.
		uint32_t Count = RecvBuffer_.GetReadableCount();
		RecvBuffer_.Read(nullptr, Count);
		RecvBuffer_.Release(Count);
	}

	// 
	// 5. If receive buffer list is empty, add new buffer and call WSARecv().
	// 

	if (!RecvBufferList_.Count())
	{
		uint32_t Size = RecvBufferLengthPerRecvCall_;
		auto Pointer = new uint8_t[Size];
		auto Buffer = RecvBufferList_.Add(RecvSequenceNumber_, Pointer, Size,
			[](void *p) { delete[] reinterpret_cast<uint8_t *>(p); });

		DWORD Flags = 0;
		int Result = WSARecv(
			SocketFd_,
			const_cast<WSABUF *>(&Buffer->OverlappedExtension()->Buffer),
			1,
			nullptr,
			&Flags,
			const_cast<OVERLAPPED *>(&Buffer->OverlappedExtension()->Overlapped),
			nullptr);

		int LastError = WSAGetLastError();
		if (Result == SOCKET_ERROR && (ERROR_IO_PENDING != LastError))
		{
			auto RequestedBuffer = RecvBufferList_.Remove(RecvSequenceNumber_);
			Assert(RequestedBuffer != nullptr);

			Trace("!! WSARecv failed, LastError = %d\n", LastError);
			return IOCPResultCode::ErrorRecvFailure;
		}

		RecvSequenceNumber_++;
	}

	// 
	// 6. Finally, release the lock.
	//    This is automatically done by dtor.
	// 

	return IOCPResultCode::Successful;
}



}

