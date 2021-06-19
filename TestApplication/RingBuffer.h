#pragma once

#include "IOCPBase.h"

namespace IOCP
{

class RingBuffer
{
public:
	RingBuffer();
	RingBuffer(uint32_t Capacity);
	~RingBuffer();

	bool Clear();
	bool Reinitialize(uint32_t Capacity);
	uint32_t Read(uint8_t *Buffer, uint32_t Count);
	uint32_t Write(uint8_t *Buffer, uint32_t Count);
	uint32_t Release(uint32_t Count);
	bool SetReadPointerToRelease();
	uint32_t GetCapacity();
	uint32_t GetLockedCount();
	uint32_t GetReadableCount();
	uint32_t GetWritableCount();
	const uint8_t *GetBufferStartPointer();
	const uint8_t *GetBufferEndPointer();
	const uint8_t *GetReadPointer();
	const uint8_t *GetWritePointer();

private:
	bool Initialize(uint32_t Capacity);

	bool Initialized_;
	std::unique_ptr<uint8_t[]> Buffer_;
	uint32_t InsertTo_;
	uint32_t RemoveFrom_;
	uint32_t Capacity_;
	uint32_t Count_;

	uint32_t LockedIndex_;
	uint32_t LockedCount_;
};

}
