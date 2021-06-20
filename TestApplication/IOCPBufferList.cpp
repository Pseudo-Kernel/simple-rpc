#include "IOCPBufferList.h"

namespace IOCP
{

IOCPBufferList::IOCPBufferList(OperationType Type) : Type_(Type)
{
}

IOCPBufferList::~IOCPBufferList()
{
}

const IOCPBuffer *IOCPBufferList::Add(uint64_t SequenceNumber, uint8_t* Buffer, uint32_t Size)
{
	auto ResultPair = BufferMap_.try_emplace(SequenceNumber, 
		std::make_unique<IOCPBuffer>(Buffer, Size, Type_, SequenceNumber));

	if (ResultPair.second)
		return ResultPair.first->second.get();

	return nullptr;
}

const IOCPBuffer *IOCPBufferList::Add(uint64_t SequenceNumber, uint8_t* Buffer, uint32_t Size, IOCPBuffer::BufferDelete BufferDeleter)
{
	auto ResultPair = BufferMap_.try_emplace(SequenceNumber, 
		std::make_unique<IOCPBuffer>(Buffer, Size, Type_, SequenceNumber, BufferDeleter));

	if (ResultPair.second)
		return ResultPair.first->second.get();

	return nullptr;
}

bool IOCPBufferList::SetBufferFlag(uint64_t SequenceNumber, uint32_t Flags)
{
	auto& it = BufferMap_.find(SequenceNumber);
	if (it == BufferMap_.end())
		return false;

	return it->second->SetFlags(Flags);
}


const IOCPBuffer* IOCPBufferList::Peek()
{
	auto& it = BufferMap_.begin();
	if (it == BufferMap_.end())
		return nullptr;

	return Peek(it->first);
}

const IOCPBuffer* IOCPBufferList::Peek(uint64_t SequenceNumber)
{
	auto& it = BufferMap_.find(SequenceNumber);
	if (it == BufferMap_.end())
		return nullptr;

	return it->second.get();
}

std::unique_ptr<IOCPBuffer> IOCPBufferList::Remove()
{
	auto& it = BufferMap_.begin();
	if (it == BufferMap_.end())
		return nullptr;

	return Remove(it->first);
}

std::unique_ptr<IOCPBuffer> IOCPBufferList::Remove(uint64_t SequenceNumber)
{
	auto& it = BufferMap_.find(SequenceNumber);
	if (it == BufferMap_.end())
		return nullptr;

	auto Buffer = std::move(it->second);
	BufferMap_.erase(it);

	return std::move(Buffer);
}

uint32_t IOCPBufferList::Count()
{
	size_t Size = BufferMap_.size();
	Assert(!(Size & 0xffffffff00000000ull));
	return static_cast<uint32_t>(Size);
}

std::map<uint64_t, std::unique_ptr<IOCPBuffer>>::const_iterator IOCPBufferList::begin() const
{
	return BufferMap_.cbegin();
}

std::map<uint64_t, std::unique_ptr<IOCPBuffer>>::const_iterator IOCPBufferList::end() const
{
	return BufferMap_.cend();
}


}
