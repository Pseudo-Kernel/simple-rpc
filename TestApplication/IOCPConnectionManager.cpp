#include "IOCPConnectionManager.h"

namespace IOCP
{

IOCPConnectionManager::IOCPConnectionManager() : 
	Initialized_(false),
	WorkersCount_(0)
{
}

IOCPConnectionManager::IOCPConnectionManager(uint32_t WorkersCount) : 
	Initialized_(false),
	WorkersCount_(WorkersCount)
{

}

IOCPConnectionManager::~IOCPConnectionManager()
{
	Shutdown();
}

bool IOCPConnectionManager::Initialize()
{
	if (Initialized_)
		return false;

	if (!WorkersCount_)
	{
		SYSTEM_INFO SystemInfo{};
		GetSystemInfo(&SystemInfo);
		WorkersCount_ = SystemInfo.dwNumberOfProcessors;
		Assert(WorkersCount_);
	}

	IoCompletionPort_ = CreateIoCompletionPort(INVALID_HANDLE_VALUE, nullptr, 0, WorkersCount_);
	if (!IoCompletionPort_)
		return false;

	Workers_ = std::make_unique<std::thread[]>(WorkersCount_);
	auto Waiter = [this]()
	{
		while (true)
		{
			DWORD BytesTransferred = 0;
			IOCPConnection* Connection = nullptr;
			IOCP_OVERLAPPED_EXTENSION* OverlappedExtension = nullptr;

			// FIXME: Use GetQueuedCompletionStatusEx()
			BOOL Result = GetQueuedCompletionStatus(IoCompletionPort_,
				&BytesTransferred,
				reinterpret_cast<ULONG_PTR *>(&Connection),
				reinterpret_cast<LPOVERLAPPED *>(&OverlappedExtension),
				INFINITE);

			// 
			// Add order number in our overlapped context when calling WSARecv()
			// so that it can reordered even if completion is processed out of order.
			// 

			bool CloseConnection = false;
//			Trace("[%5d] GetQueuedCompletionStatus: len %u\n", GetCurrentThreadId(), BytesTransferred);

			if (!BytesTransferred && !Connection && !OverlappedExtension)
			{
				Trace("[%5d] Requested shutdown!\n", GetCurrentThreadId());
				break;
			}

			if (Result)
			{
				if (OverlappedExtension->Operation == OperationType::Send)
				{
					// Call our send completion routine.
					Connection->SendCompletion(OverlappedExtension);
				}
				else if (OverlappedExtension->Operation == OperationType::Recv)
				{
					// Call our receive completion routine.
					Connection->ReceiveCompletion(OverlappedExtension);
				}
				else // unknown!
				{
					// it cannot be!
					Assert(false);
				}


				if (OverlappedExtension->Operation == OperationType::Recv && 
					!BytesTransferred)
				{
					// Graceful close
					CloseConnection = true;
				}

				// do something
			}
			else
			{
				// FIXME: Close the connection.
				Trace("%s: GetQueuedCompletionStatus() failed\n", __FUNCTION__);
				CloseConnection = true;
			}

		}
	};

	for (size_t i = 0; i < WorkersCount_; i++)
		Workers_[i] = std::thread(Waiter);

	return true;
}

bool IOCPConnectionManager::Shutdown()
{
	if (!Initialized_)
		return false;

	// Send terminate message to all worker threads
	for (uint32_t i = 0; i < WorkersCount_; i++)
		PostQueuedCompletionStatus(IoCompletionPort_, 0, 0, nullptr);

	// Wait for thread termination
	for (uint32_t i = 0; i < WorkersCount_; i++)
	{
		if (Workers_[i].joinable())
			Workers_[i].join();
	}

	std::lock_guard<decltype(Mutex_)> Lock(Mutex_);
	{
		CloseHandle(IoCompletionPort_);
		IoCompletionPort_ = nullptr;

		ConnectionMap_.clear();
		Workers_ = nullptr;
	}

	Initialized_ = false;

	return true;
}


IOCPConnection * IOCPConnectionManager::AddConnection(SOCKET Socket, const IODispatchHandler *Dispatch, uint32_t SendBufferCapacity, uint32_t RecvBufferCapacity, uint32_t RecvBufferLengthPerRecvCall)
{
	auto Connection = std::make_unique<IOCPConnection>(Socket, Dispatch, SendBufferCapacity, RecvBufferCapacity, RecvBufferLengthPerRecvCall);
	if (!Connection)
		return nullptr;

	std::lock_guard<decltype(Mutex_)> Lock(Mutex_);
	uintptr_t Key = Socket;
	auto& it = ConnectionMap_.find(Key);
	if (it != ConnectionMap_.end())
		return nullptr;

	const auto Object = Connection.get();
	//	ConnectionMap_.emplace(Key, std::move(Connection));
	auto ResultPair = ConnectionMap_.try_emplace(Key, std::move(Connection));
	Assert(ResultPair.second);

	// Associate an I/O completion port with socket.
	HANDLE PrevPort = IoCompletionPort_;
	IoCompletionPort_ = CreateIoCompletionPort(
		reinterpret_cast<HANDLE>(Socket),
		PrevPort,
		reinterpret_cast<ULONG_PTR>(Object),
		0);

	if (!IoCompletionPort_)
	{
		ConnectionMap_.erase(Key);
		return nullptr;
	}

	Assert(PrevPort == IoCompletionPort_);

	if (!Object->IssueRecvCompleted())
	{
		ConnectionMap_.erase(Key);
		return nullptr;
	}

	return Object;
}

}
