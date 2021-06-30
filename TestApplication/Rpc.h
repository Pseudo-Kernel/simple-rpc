#pragma once

#include "SRPCBase.h"
#include "ThreadPool.h"
#include "ByteStream.h"

namespace SRPC
{

template <typename TReturn, typename... TArgs>
constexpr uint32_t EvaluateParamsCount(TReturn(*Function)(TArgs...))
{
	return sizeof...(TArgs);
}

struct RpcRegisteredCall
{
	uint64_t Id;
	void *Function;
	uint32_t ParamsCount;
};

enum class RpcFrameType : uint16_t
{
	None = 0,
	Register,
	Unregister,
	Call,
//	Cancel,
	CallReturn,
};

#pragma pack(push, 4)
struct RpcFrameHeader
{
	uint8_t Magic[4];		// Byte sequence { 1a a5 fa 71 }
	uint32_t Digest0;		// [31:0] of digest
	uint64_t Fsn;			// Frame sequence number
	uint32_t Digest1;		// [63:32] of digest
	uint32_t Rsn;			// Request/Response sequence number
	uint32_t Digest2;		// [95:64] of digest
	uint16_t Type;			// Type of frame (see RpcFrameType)
	union
	{
		struct
		{
			uint16_t Response : 1;	// Response (if set)
			uint16_t EoR : 1;		// End of request/response (if set)
			uint16_t Reserved : 14;
		} Bitfields;
		uint16_t Flags;
	} u;
	uint32_t Digest3;		// [127:96] of digest
};


/*
	RPC Type Record

	<TypeId><Attributes>[...]

	TypeId: 2-byte
	Attributes: 1-byte
	ArrayCount: 4-byte
	Packing: 1-byte
	FieldsCount: 1-byte


	### Arrays ###
	[TypeId][Attributes=Array][ArrayCount=[Count0]..[Count3]]
		Int8[0x1f700]
		=> [INT_8][Array][ArrayCount=0x1f700]
		Utf-16[0x120], Read/Writable
		=> [CP_16][Array|OutAttribute][ArrayCount=0x120]


	### Structure ###
	[TypeId=UDT][Attributes][Packing][FieldsCount] { [Field1TypeId][Field2TypeId]... }
		pack(4) struct {
			Int8 f1;
			Int16 f2;
			Int32 f3;
		} [0x123]
			=> [UDT][Array][ArrayCount=0x123][Packing=4][FieldsCount=3] {
			     [INT_8]	// f1.Ty
			     [INT_16]	// f2.Ty
			     [INT_32]	// f3.Ty
			   }

		### Nested structure ###
		pack(4) struct { // UDT 0x21
			pack(2) struct { // UDT 0x20
				Int8 g;
			} f1;
			Int16 f2;
			Int32 f3;
		} [0x123]
			=> [UDT_0x20][None][Packing=2][FieldsCount=1] {
			     [INT_8] // g.Ty
			   }
			   [UDT_0x21][Array][ArrayCount=0x123][Packing=4][FieldsCount=3] {
			     [UDT_0x20] // f1.Ty
				 [INT_16] // f2.Ty
				 [INT_32] // f3.Ty
			   }

*/

enum class RpcTypeId : uint16_t
{
	Undefined = 0x00,

	// Type primitives [0x01..0x10]
	PRIMITIVE_START = 0x01,
	INT_8 = PRIMITIVE_START,
	INT_16,
	INT_32,
	INT_64,

	INT_8U,
	INT_16U,
	INT_32U,
	INT_64U,

	FLOAT_32,
	FLOAT_64,

	CP_8,
	CP_16,
	CP_32,

	PTR_32,
	PTR_64,
	PRIMITIVE_END,

	// User-defined types (UDT) [0x80..0xef]
	UDT_START = 0x80,
	PARAM_LIST = UDT_START,	// Special type. Parameter list for function call

	UDT_END = 0xef,

};

enum RpcTypeAttributes : uint8_t
{
	None = 0x00,
	Array = 0x01,
	OutAttribute = 0x80,
};

constexpr inline RpcTypeAttributes operator|(RpcTypeAttributes lhs, RpcTypeAttributes rhs)
{
	return static_cast<RpcTypeAttributes>(
		static_cast<uint8_t>(lhs) | static_cast<uint8_t>(rhs)
		);
}

inline RpcTypeAttributes& operator|=(RpcTypeAttributes& lhs, RpcTypeAttributes rhs)
{
	lhs = lhs | rhs;
	return lhs;
}


union RpcFrameBody
{
	struct
	{
		// Register: Id, { Parameter Type Records }
		uint64_t Id;
	} Register;

	struct
	{
		// Call: Id, { Parameter Type Records }, { Parameters }
		uint64_t Id;
	} Call;

	struct
	{
		// CallReturn: Id, { Return Type Record }, Return Value
	} CallReturn;
};


class Rpc
{
public:
	Rpc(uint32_t ThreadsCount) : 
		ThreadPool_(ThreadsCount),
		ThreadCount_(ThreadsCount)
	{

	}
	~Rpc() { }

	uint64_t Invoke(uint64_t Id, uint32_t ParamsCount, va_list *Params)
	{
		return 0;
	}

	uint64_t Invoke(uint64_t Id, uint32_t ParamsCount, ...)
	{
		va_list Params;
		va_start(Params, ParamsCount);
		auto Result = Invoke(Id, ParamsCount, Params);
		va_end(Params);

		return Result;
	}

	uint64_t Register(void *PtrFunction, uint32_t ParamsCount, uint64_t Id)
	{
		std::lock_guard<decltype(Mutex_)> Lock(Mutex_);

		RpcRegisteredCall Call;
		Call.Function = PtrFunction;
		Call.Id = Id;
		Call.ParamsCount = ParamsCount;

		auto ResultPair = CallMap_.try_emplace(Id, Call);
		if (ResultPair.second)
			return Id;

		return 0;
	}

	template <
		typename TFunction, 
		typename = std::enable_if<std::is_function<TFunction>::value, typename = void>>
		uint64_t Register(TFunction Function, uint64_t Id)
	{
		uint32_t ParamsCount = EvaluateParamsCount(PtrFunction);
		return Register(PtrFunction, ParamsCount, Id);
	}

private:
	
	/*
		TODO: Implement function call helper.
	*/


	uint32_t ThreadCount_;
	ThreadPool ThreadPool_;
	std::recursive_mutex Mutex_;
	std::map<uint64_t, RpcRegisteredCall> CallMap_;
};

}
