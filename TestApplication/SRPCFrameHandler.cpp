#include "SRPCFrameHandler.h"


namespace SRPC
{

SRPCFrameHandler::SRPCFrameHandler()
{
}

SRPCFrameHandler::~SRPCFrameHandler()
{
}

bool SRPCFrameHandler::SendComplete(const uint8_t * BufferSent, uint32_t Size) noexcept
{
	return true;
}

bool SRPCFrameHandler::ReceiveComplete(const uint8_t * BufferReceived, uint32_t Size) noexcept
{
	return true;
}

}
