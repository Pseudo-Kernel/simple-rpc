#pragma once

#include "SRPCBase.h"
#include "IODispatchHandler.h"

namespace SRPC
{

class SRPCFrameHandler : public IOCP::IODispatchHandler
{
public:
	SRPCFrameHandler();
	~SRPCFrameHandler();

private:
	bool SendComplete(const uint8_t *BufferSent, uint32_t Size) noexcept;
	bool ReceiveComplete(const uint8_t *BufferReceived, uint32_t Size) noexcept;

};

}


