#pragma once

#include "IOCPBase.h"
#include "ProtocolInterface.h"
#include "RingBuffer.h"
#include "IOCPBufferList.h"

namespace IOCP
{

class IODispatchHandler
{
public:
	virtual bool SendComplete(const uint8_t *Buffer, size_t Size) = 0;
	virtual bool ReceiveComplete(const uint8_t *Buffer, size_t Size) = 0;
	virtual ~IODispatchHandler() { };
};

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

	IOCPResultCode Send(uint8_t *Buffer, size_t Size, size_t *SizeQueued);

private:

	bool IssueSendCompleted();
	bool IssueRecvCompleted();

	IOCPResultCode SendCompletion(IOCP_OVERLAPPED_EXTENSION *OverlappedExtension);
	IOCPResultCode ReceiveCompletion(IOCP_OVERLAPPED_EXTENSION *OverlappedExtension);


	SOCKET SocketFd_;		// Socket file descriptor

	std::recursive_mutex SendBufferMutex_;			// Mutex for send operation.
	RingBuffer SendBuffer_;							// Ring buffer which stores data to send.
	IOCPBufferList SendBufferList_;					// Buffer list for WSASend().
	std::atomic_uint64_t SendSequenceNumber_;		// Send sequence number (not used).

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


#if 0

IOCPResultCode ReceiveCompletion(IOCP_OVERLAPPED_EXTENSION *OverlappedExtension)
{
	// 
	// Our receive completion routine.
	// 

	// 
	// 1. Acquire lock for receive processing.
	// 

	std::lock_guard<decltype(RecvBufferMutex_)> Lock(RecvBufferMutex_);

	// 
	// 2. Make sure that received datas are correctly ordered.
	//    This can be done by checking sequence number in receive buffer list.
	// 

	Assert(RecvBufferList_.Count());
	auto BeginIterator = RecvBufferList_.begin();
	uint64_t BeginSequenceNumber = BeginIterator->first;
	uint32_t CompletedCountContiguous = 0;
	Assert(BeginSequenceNumber <= OverlappedExtension->SequenceNumber);

	for (auto& it : RecvBufferList_)
	{
		Assert(it.first == BeginSequenceNumber + CompletedCountContiguous);

		bool Completed = HasOverlappedIoCompleted(&it.second.OverlappedExtension()->Overlapped);
		if (!Completed)
			break;

		CompletedCountContiguous++;
	}

	// 
	// 3. Write the received data chunks to received ring buffer by sequential order.
	//    If sequence number does not match as expected, stop processing and goto next step.
	// 

	for (uint32_t i = 0; i < CompletedCountContiguous; i++)
	{
		IOCPBuffer Buffer;
		uint64_t TargetSequenceNumber = BeginSequenceNumber + i;

		auto TargetOverlapped = Buffer.OverlappedExtension();

		Assert(RecvBufferList_.Remove(Buffer));
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

		Trace("Removed the buffer object [sequence number %llu, size %u]\n",
			TargetSequenceNumber, ReceivedLength);
	}

	while (true)
	{
		// 
		// *4. Parse the received data in the ring buffer.
		// 

		if (Protocol_)
		{
			uint32_t RemainingCountWraparound = RecvBuffer_.GetBufferEndPointer() - RecvBuffer_.GetReadPointer();
			uint32_t ReadableCount = RecvBuffer_.GetReadableCount();

			uint32_t Count1 = std::min<uint32_t>(ReadableCount, RemainingCountWraparound);
			auto Buffer1 = RecvBuffer_.GetReadPointer();

			uint32_t BytesProcessed = 0;

			if (Count1)
			{
				ProtocolParseContext Context{};
				Context.HintType = ProtocolParseHintType::HintNone;

				ProtocolResult Result = const_cast<ProtocolInterface *>(Protocol_)->
					DispatchParseFrame(Buffer1, Count1, Context);

				if (Result == ProtocolResult::Success)
				{
					switch (Context.HintType)
					{
					case ProtocolParseHintType::HintNone:
						break;

					case ProtocolParseHintType::FrameStart:
					case ProtocolParseHintType::FrameEnd:
					case ProtocolParseHintType::FrameStartEnd:
						Assert(Context.HintOffset + Context.HintSize <= Context.ProcessedSize);
						break;

					default:
						Assert(false);
					}

					if (LastParseHintValidContext_.HintType == ProtocolParseHintType::HintNone)
					{
						LastParseHintValidContext_ = Context;
					}
					else
					{
						// LastValid.HintType    Current.HintType     Action
						//     FrameStart           FrameStart        Revert to current hint
						//     FrameEnd             FrameStart        Revert to current hint
						//     FrameEnd             FrameStartEnd     Revert to current hint
						//     FrameStartEnd        FrameStart        Revert to current hint
						//     FrameStart           FrameEnd          Validate, Success: Get frame, Fail: Do not revert, clear last hint
						//     FrameStartEnd        FrameEnd          Validate, Success: Get frame, Fail: Do not revert, clear last hint
						//     FrameStart           FrameStartEnd     Validate, Success: Get frame, Fail: Revert to current hint, set last hint=FrameStart
						//     FrameStartEnd        FrameStartEnd     Validate, Success: Get frame, Fail: Revert to current hint, set last hint=FrameStart
						//     FrameEnd             FrameEnd          Do not revert, clear last hint

						auto Tuple = std::make_tuple(LastParseHintValidContext_.HintType, Context.HintType);

						if (Tuple == std::make_tuple(ProtocolParseHintType::FrameStart, ProtocolParseHintType::FrameStart) ||
							Tuple == std::make_tuple(ProtocolParseHintType::FrameEnd, ProtocolParseHintType::FrameStart) ||
							Tuple == std::make_tuple(ProtocolParseHintType::FrameEnd, ProtocolParseHintType::FrameStartEnd) ||
							Tuple == std::make_tuple(ProtocolParseHintType::FrameStartEnd, ProtocolParseHintType::FrameStart))
						{
							// Revert to current hint
						}
						else if (
							Tuple == std::make_tuple(ProtocolParseHintType::FrameStart, ProtocolParseHintType::FrameEnd) ||
							Tuple == std::make_tuple(ProtocolParseHintType::FrameStartEnd, ProtocolParseHintType::FrameEnd) ||
							Tuple == std::make_tuple(ProtocolParseHintType::FrameStart, ProtocolParseHintType::FrameStartEnd) ||
							Tuple == std::make_tuple(ProtocolParseHintType::FrameStartEnd, ProtocolParseHintType::FrameStartEnd))
						{
							// Validate the frame
							ProtocolResult ValidationResult = const_cast<ProtocolInterface *>(Protocol_)->
								DispatchValidateFrame(Buffer1, Count1, Context);
						}
						else if (Tuple == std::make_tuple(ProtocolParseHintType::FrameEnd, ProtocolParseHintType::FrameEnd))
						{
							// Do not revert
						}
					}



					// Advance the read pointer.
					RecvBuffer_.Read(nullptr, Context.ProcessedSize);
				}
				else if (Result == ProtocolResult::FramingError)
				{
					// revert to second frame start candidate
				}
				else if (Result == ProtocolResult::InputBufferTooSmall)
				{
					// swap buffer!
					// break!
				}
			}

			uint32_t Count2 = (ReadableCount >= RemainingCountWraparound) ?
				ReadableCount - RemainingCountWraparound : 0;
			auto Buffer2 = RecvBuffer_.GetBufferStartPointer();

			if (Count2)
			{
				ProtocolParseContext Context;
				ProtocolResult Result = const_cast<ProtocolInterface *>(Protocol_)->
					DispatchParseFrame(Buffer2, Count2, Context);
			}
		}

		// 
		// *5. Release parsed bytes from received ring buffer.
		// 

		// 
		// *6. Do step 4-5 until no more bytes can be parsed.
		//     Otherwise, do nothing.
		// 
	}



	// 7. If receive buffer list is empty, add new buffer and call WSARecv().
	// 8. Finally, release the lock.

	return IOCPResultCode::Successful;
}

#endif