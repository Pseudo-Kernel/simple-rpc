#include "RingBuffer.h"

namespace IOCP
{

RingBuffer::RingBuffer() :
	Initialized_(false), 
	Buffer_(nullptr), 
	InsertTo_(0), 
	RemoveFrom_(0), 
	LockedIndex_(0), 
	LockedCount_(0), 
	Capacity_(0), 
	Count_(0)
{
}

RingBuffer::RingBuffer(uint32_t Capacity) : RingBuffer()
{
	Initialize(Capacity);
}

RingBuffer::~RingBuffer()
{
}

bool RingBuffer::Clear()
{
	if (!Initialized_)
		return false;

	InsertTo_ = RemoveFrom_ = Count_ = 0;

	return true;
}

bool RingBuffer::Reinitialize(uint32_t Capacity)
{
	return Initialize(Capacity);
}

uint32_t RingBuffer::Read(uint8_t * Buffer, uint32_t Count)
{
#if 0
	uint32_t BytesRead = 0;
	for (uint32_t i = 0; i < Count; i++)
	{
		if (!Count_)
			break;

		if (Buffer)
			Buffer[i] = Buffer_[RemoveFrom_ % Capacity_];

		RemoveFrom_ = (RemoveFrom_ + 1) % Capacity_;
		Count_--;

		BytesRead++;
	}

	return BytesRead;
#else
	uint32_t ReadCount = std::min<uint32_t>(Count, Count_);

	uint32_t SplitCount1 = 0;
	uint32_t SplitCount2 = 0;

	if (Buffer)
	{
		if (ReadCount + RemoveFrom_ <= Capacity_)
		{
			SplitCount1 = ReadCount;
			SplitCount2 = 0;

			memcpy(Buffer, Buffer_.get() + RemoveFrom_, SplitCount1);
		}
		else // ReadCount > Capacity_ - RemoveFrom_
		{
			SplitCount1 = Capacity_ - RemoveFrom_;
			SplitCount2 = ReadCount - SplitCount1;

			memcpy(Buffer, Buffer_.get() + RemoveFrom_, SplitCount1);
			memcpy(Buffer + SplitCount1, Buffer_.get(), SplitCount2);
		}
	}

	LockedCount_ += ReadCount;
	RemoveFrom_ = (RemoveFrom_ + ReadCount) % Capacity_;
	Count_ -= ReadCount;

	return ReadCount;
#endif
}

uint32_t RingBuffer::Write(uint8_t * Buffer, uint32_t Count)
{
#if 0
	uint32_t BytesWritten = 0;
	for (uint32_t i = 0; i < Count; i++)
	{
		if (Count_ >= Capacity_)
			break;

		if (Buffer)
			Buffer_[InsertTo_ % Capacity_] = Buffer[i];

		InsertTo_ = (InsertTo_ + 1) % Capacity_;
		Count_++;
		LockedCount_++;

		BytesWritten++;
	}

	return BytesWritten;
#else
	Assert(Capacity_ >= Count_);
	Assert(Capacity_ >= LockedCount_);
	Assert(Capacity_ >= Count_ + LockedCount_);

	uint32_t WriteCount = std::min<uint32_t>(Count, Capacity_ - Count_ - LockedCount_);

	uint32_t SplitCount1 = 0;
	uint32_t SplitCount2 = 0;

	if (Buffer && WriteCount)
	{
		if (WriteCount + InsertTo_ <= Capacity_)
		{
			SplitCount1 = WriteCount;
			SplitCount2 = 0;

			memcpy(Buffer_.get() + InsertTo_, Buffer, SplitCount1);
		}
		else // WriteCount + InsertTo_ > Capacity_
		{
			SplitCount1 = Capacity_ - InsertTo_;
			SplitCount2 = WriteCount - SplitCount1;

			memcpy(Buffer_.get() + InsertTo_, Buffer, SplitCount1);
			memcpy(Buffer_.get(), Buffer + SplitCount1, SplitCount2);
		}
	}

	InsertTo_ = (InsertTo_ + WriteCount) % Capacity_;
	Count_ += WriteCount;

	return WriteCount;
#endif
}

uint32_t RingBuffer::Release(uint32_t Count)
{
#if 0
	uint32_t BytesReleased = 0;

	for (uint32_t i = 0; i < Count; i++)
	{
		if (!LockedCount_)
			break;

		LockedIndex_ = (LockedIndex_ + 1) % Capacity_;
		LockedCount_--;

		BytesReleased++;
	}

	return BytesReleased;
#else // release
	uint32_t ReleaseCount = std::min<uint32_t>(LockedCount_, Count);
	LockedIndex_ = (LockedIndex_ + ReleaseCount) % Capacity_;
	LockedCount_ -= ReleaseCount;
	return ReleaseCount;
#endif
}

bool RingBuffer::SetReadPointerToRelease()
{
	RemoveFrom_ = LockedIndex_;
	Count_ = LockedCount_;
	return true;
}

uint32_t RingBuffer::GetCapacity()
{
	return Capacity_;
}

uint32_t RingBuffer::GetLockedCount()
{
	return LockedCount_;
}

uint32_t RingBuffer::GetReadableCount()
{
	return Count_;
}

uint32_t RingBuffer::GetWritableCount()
{
	Assert(Capacity_ >= Count_);
	Assert(Capacity_ >= LockedCount_);
	Assert(Capacity_ >= Count_ + LockedCount_);

	return Capacity_ - LockedCount_ - Count_;
}

const uint8_t * RingBuffer::GetBufferStartPointer()
{
	return Buffer_.get();
}

const uint8_t * RingBuffer::GetBufferEndPointer()
{
	return Buffer_.get() + Capacity_;
}

const uint8_t * RingBuffer::GetReadPointer()
{
	return Buffer_.get() + RemoveFrom_;
}

const uint8_t * RingBuffer::GetWritePointer()
{
	return Buffer_.get() + InsertTo_;
}

bool RingBuffer::Initialize(uint32_t Capacity)
{
	auto Buffer = std::make_unique<uint8_t[]>(Capacity);
	if (!Buffer)
		return false;

	InsertTo_ = RemoveFrom_ = Count_ = LockedIndex_ = LockedCount_ = 0;
	Capacity_ = Capacity;
	Buffer_ = std::move(Buffer);
	Initialized_ = true;

	return true;
}

}

