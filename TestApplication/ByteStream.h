#pragma once

#include "Rpc.h"

namespace SRPC
{

class ByteStream
{
public:
	ByteStream();
	ByteStream(uint8_t *Buffer, size_t Size);
	~ByteStream();

	size_t Read1(uint8_t *Value);
	size_t Read2(uint16_t *Value);
	size_t Read4(uint32_t *Value);
	size_t Read8(uint64_t *Value);
	size_t Write1(uint8_t Value);
	size_t Write2(uint16_t Value);
	size_t Write4(uint32_t Value);
	size_t Write8(uint64_t Value);

	size_t Write(uint8_t *Buffer, size_t Size);
	size_t Advance(size_t Size);

	size_t Remaining() const;
	size_t Current() const;
	uint8_t *Pointer() const;

private:
	uint8_t *Base_;
	size_t Size_;
	size_t Current_;
};

}

