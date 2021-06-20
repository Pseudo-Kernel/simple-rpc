#pragma once

#include "IOCPBase.h"

namespace IOCP
{

class IODispatchHandler
{
public:
	virtual bool SendComplete(const uint8_t *BufferSent, uint32_t Size) noexcept
	{
		return true;
	}

	virtual bool ReceiveComplete(const uint8_t *BufferReceived, uint32_t Size) noexcept
	{
		return true;
	}

	virtual ~IODispatchHandler() { }
};

}
