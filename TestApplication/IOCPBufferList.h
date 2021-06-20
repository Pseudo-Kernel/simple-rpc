#pragma once

#include "IOCPBase.h"
#include "IOCPBuffer.h"

namespace IOCP
{

class IOCPBufferList
{
public:
	IOCPBufferList(OperationType Type);
	~IOCPBufferList();

	const IOCPBuffer *Add(uint64_t SequenceNumber, uint8_t* Buffer, uint32_t Size);
	const IOCPBuffer *Add(uint64_t SequenceNumber, uint8_t* Buffer, uint32_t Size, IOCPBuffer::BufferDelete BufferDeleter);

	bool SetBufferFlag(uint64_t SequenceNumber, uint32_t Flags);

	const IOCPBuffer* Peek();
	const IOCPBuffer* Peek(uint64_t SequenceNumber);

	std::unique_ptr<IOCPBuffer> Remove();
	std::unique_ptr<IOCPBuffer> Remove(uint64_t SequenceNumber);

	uint32_t Count();

	// returns const iterator
	std::map<uint64_t, std::unique_ptr<IOCPBuffer>>::const_iterator begin() const;
	std::map<uint64_t, std::unique_ptr<IOCPBuffer>>::const_iterator end() const;

private:
	OperationType Type_;
	std::map<uint64_t, std::unique_ptr<IOCPBuffer>> BufferMap_;  // <SequenceNumber, BufferPtr>
};

}
