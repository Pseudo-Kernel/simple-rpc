#pragma once

#include "IOCPBase.h"

namespace IOCP
{

enum class ProtocolResult : uint32_t
{
	Success = 0,
	NotSupported,

	InputBufferTooSmall,
	FramingError,
};

enum class ProtocolParseHintType : uint32_t
{
	HintNone = 0,		// None.
	FrameStart,			// Hint describes frame start.
	FrameEnd,			// Hint describes frame end.
	FrameStartEnd,		// Hint describes both frame start and frame end.
};

struct ProtocolParseContext
{
	size_t ProcessedSize;			// [out] processed size in bytes

	uint32_t HintOffset;			// [out] offset of hint (frame start/frame end)
	uint32_t HintSize;				// [out] hint size in bytes
	ProtocolParseHintType HintType;	// [out] hint type
};

class ProtocolInterface
{
public:
	// 
	// Parse:
	//  1. Complete (frame is completely valid)
	//  2. MoreProcessingRequired (cannot determine whether the frame is valid or not because frame is incomplete)
	//  3. FramingError (need to rollback to next frame candidate)
	// 
	// Scenario:
	//  a. MoreProcessingRequired -> MoreProcessingRequired -> ... -> Complete
	//  b. MoreProcessingRequired -> MoreProcessingRequired -> ... -> FramingError
	//     -> (Rollback to next frame candidate) MoreProcessingRequired -> Complete
	// 

	// ????H...F????
	// S           E
	//     1   2

//	virtual int DispatchHandshake(void *HandshakeContext) = 0;

	virtual ProtocolResult DispatchEncapsulate(const uint8_t *Buffer, size_t Size) = 0;
	virtual ProtocolResult DispatchDecapsulate(const uint8_t *Buffer, size_t Size) = 0;
	virtual ProtocolResult DispatchParseFrame(const uint8_t *Buffer, size_t Size, ProtocolParseContext& ParseContext) = 0;
	virtual ProtocolResult DispatchValidateFrame(const uint8_t *Buffer, size_t Size) = 0;
	virtual ProtocolResult DispatchShutdown(void *ShutdownContext) = 0;

//	virtual int Send(uint8_t *Buffer, size_t Size, void *ProtocolContext) = 0;
};

}
