#include "IOCPBuffer.h"

namespace IOCP
{

IOCPBuffer::IOCPBuffer() noexcept : 
	OverlappedExtension_{},
	Delete_(),
	Initialized_(false)
{
}

IOCPBuffer::IOCPBuffer(uint8_t * Buffer, size_t Size, OperationType Type, uint64_t SequenceNumber) noexcept :
	IOCPBuffer(Buffer, Size, Type, SequenceNumber, [](auto p) { delete p; })
{
}

IOCPBuffer::IOCPBuffer(uint8_t * Buffer, size_t Size, OperationType Type, uint64_t SequenceNumber, BufferDelete Delete) noexcept :
	OverlappedExtension_{},
	Delete_(Delete),
	Initialized_(true)
{
	OverlappedExtension_.Operation = Type;
	OverlappedExtension_.SequenceNumber = SequenceNumber;
	OverlappedExtension_.Buffer.buf = reinterpret_cast<char *>(Buffer);
	OverlappedExtension_.Buffer.len = Size;
}

IOCPBuffer::IOCPBuffer(IOCPBuffer && rhs) noexcept
{
	*this = std::move(rhs);
}

IOCPBuffer::~IOCPBuffer()
{
	if (Initialized_ && Delete_)
	{
		Delete_(OverlappedExtension_.Buffer.buf);
	}

	Delete_ = nullptr;
	OverlappedExtension_.Buffer.buf = nullptr;
	OverlappedExtension_.Buffer.len = 0;
	Initialized_ = false;
}

IOCPBuffer & IOCPBuffer::operator=(IOCPBuffer && rhs) noexcept
{
	// move
	OverlappedExtension_ = rhs.OverlappedExtension_;
	Delete_ = rhs.Delete_;
	Initialized_ = rhs.Initialized_;

	// re-initialize object rhs by default constructor
	new (&rhs) IOCPBuffer();

	return *this;
}

bool IOCPBuffer::SetFlags(uint32_t Flags)
{
	if (Initialized_)
	{
		OverlappedExtension_.Flags = Flags;
		return true;
	}

	return false;
}

bool IOCPBuffer::Valid() const noexcept
{
	return Initialized_;
}

uint64_t IOCPBuffer::SequenceNumber() const noexcept
{
	if (Initialized_)
		return OverlappedExtension_.SequenceNumber;

	return 0;
}

uint8_t * IOCPBuffer::Pointer() const noexcept
{
	if (Initialized_)
		return reinterpret_cast<uint8_t *>(OverlappedExtension_.Buffer.buf);

	return nullptr;
}

size_t IOCPBuffer::Size() const noexcept
{
	if (Initialized_)
		return OverlappedExtension_.Buffer.len;

	return 0;
}

OperationType IOCPBuffer::Type() const noexcept
{
	return OverlappedExtension_.Operation;
}

uint32_t IOCPBuffer::Flags() const noexcept
{
	return OverlappedExtension_.Flags;
}

const IOCP_OVERLAPPED_EXTENSION * IOCPBuffer::OverlappedExtension() const noexcept
{
	return &OverlappedExtension_;
}

auto IOCPBuffer::Deleter() const noexcept
{
	return Delete_;
}

}
