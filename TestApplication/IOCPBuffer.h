#pragma once

#include "IOCPBase.h"

namespace IOCP
{

class IOCPBuffer
{
public:
//	static const struct Flags
//	{
//	};

	using BufferDelete = std::function<void(void *Buffer)>;

	IOCPBuffer() noexcept;
	IOCPBuffer(uint8_t *Buffer, size_t Size, OperationType Type, uint64_t SequenceNumber) noexcept;
	IOCPBuffer(uint8_t *Buffer, size_t Size, OperationType Type, uint64_t SequenceNumber, BufferDelete Delete) noexcept;
	IOCPBuffer(IOCPBuffer &) = delete;
	IOCPBuffer(IOCPBuffer&& rhs) noexcept;
	~IOCPBuffer() noexcept;

	IOCPBuffer& operator=(const IOCPBuffer& rhs) = delete;
	IOCPBuffer& operator=(IOCPBuffer&& rhs) noexcept;

	bool SetFlags(uint32_t Flags);

	bool Valid() const noexcept;
	uint64_t SequenceNumber() const noexcept;
	uint8_t *Pointer() const noexcept;
	size_t Size() const noexcept;
	OperationType Type() const noexcept;
	uint32_t Flags() const noexcept;
	const IOCP_OVERLAPPED_EXTENSION *OverlappedExtension() const noexcept;
	auto Deleter() const noexcept;

private:
	IOCP_OVERLAPPED_EXTENSION OverlappedExtension_;
	std::function<void(void *Buffer)> Delete_;
	bool Initialized_;
};

}

