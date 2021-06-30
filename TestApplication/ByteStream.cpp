#include "ByteStream.h"

namespace SRPC
{

ByteStream::ByteStream() : ByteStream(nullptr, 0)
{
}

ByteStream::ByteStream(uint8_t * Buffer, size_t Size) : 
	Base_(Buffer), Size_(Size), Current_(0)
{
}

ByteStream::~ByteStream()
{
}

size_t ByteStream::Read1(uint8_t * Value)
{
	if (Remaining() < sizeof(*Value))
		return 0;

	if (Value)
		*Value = *Pointer();

	return Advance(sizeof(*Value));
}

size_t ByteStream::Read2(uint16_t * Value)
{
	if (Remaining() < sizeof(*Value))
		return 0;

	if (Value)
	{
		uint8_t *p = Pointer();
		*Value = 
			(static_cast<uint16_t>(p[1]) << 0x08) | p[0];
	}

	return Advance(sizeof(*Value));
}

size_t ByteStream::Read4(uint32_t * Value)
{
	if (Remaining() < sizeof(*Value))
		return 0;

	if (Value)
	{
		uint8_t *p = Pointer();
		*Value =
			(static_cast<uint32_t>(p[3]) << 0x18) |
			(static_cast<uint32_t>(p[2]) << 0x10) |
			(static_cast<uint32_t>(p[1]) << 0x08) |
			p[0];
	}

	return Advance(sizeof(*Value));
}

size_t ByteStream::Read8(uint64_t * Value)
{
	if (Remaining() < sizeof(*Value))
		return 0;

	if (Value)
	{
		uint8_t *p = Pointer();
		*Value =
			(static_cast<uint64_t>(p[7]) << 0x38) |
			(static_cast<uint64_t>(p[6]) << 0x30) |
			(static_cast<uint64_t>(p[5]) << 0x28) |
			(static_cast<uint64_t>(p[4]) << 0x20) |
			(static_cast<uint64_t>(p[3]) << 0x18) |
			(static_cast<uint64_t>(p[2]) << 0x10) |
			(static_cast<uint64_t>(p[1]) << 0x08) |
			p[0];
	}

	return Advance(sizeof(*Value));
}

size_t ByteStream::Write1(uint8_t Value)
{
	if (Remaining() < sizeof(Value))
		return 0;

	*Pointer() = Value;

	return Advance(sizeof(Value));
}

size_t ByteStream::Write2(uint16_t Value)
{
	if (Remaining() < sizeof(Value))
		return 0;

	uint8_t *p = Pointer();
	p[0] = static_cast<uint8_t>(Value & 0xff);
	p[1] = static_cast<uint8_t>(Value >> 0x08);

	return Advance(sizeof(Value));
}

size_t ByteStream::Write4(uint32_t Value)
{
	if (Remaining() < sizeof(Value))
		return 0;

	uint8_t *p = Pointer();
	p[0] = static_cast<uint8_t>(Value & 0xff);
	p[1] = static_cast<uint8_t>(Value >> 0x08);
	p[2] = static_cast<uint8_t>(Value >> 0x10);
	p[3] = static_cast<uint8_t>(Value >> 0x18);

	return Advance(sizeof(Value));
}

size_t ByteStream::Write8(uint64_t Value)
{
	if (Remaining() < sizeof(Value))
		return 0;

	uint8_t *p = Pointer();
	p[0] = static_cast<uint8_t>(Value & 0xff);
	p[1] = static_cast<uint8_t>(Value >> 0x08);
	p[2] = static_cast<uint8_t>(Value >> 0x10);
	p[3] = static_cast<uint8_t>(Value >> 0x18);
	p[4] = static_cast<uint8_t>(Value >> 0x20);
	p[5] = static_cast<uint8_t>(Value >> 0x28);
	p[6] = static_cast<uint8_t>(Value >> 0x30);
	p[7] = static_cast<uint8_t>(Value >> 0x38);

	return Advance(sizeof(Value));
}

size_t ByteStream::Write(uint8_t * Buffer, size_t Size)
{
	size_t Count = std::min<size_t>(Size, Remaining());

	memcpy(Pointer(), Buffer, Count);

	return Advance(Count);
}

size_t ByteStream::Advance(size_t Size)
{
	size_t Prev = Current_;
	size_t Next = std::min<size_t>(Current_ + Size, Size_);

	Current_ = Next;
	return Next - Prev;
}

size_t ByteStream::Remaining() const
{
	return Size_ - Current_;
}

size_t ByteStream::Current() const
{
	return Current_;
}

uint8_t * ByteStream::Pointer() const
{
	return Base_ + Current_;
}

}

