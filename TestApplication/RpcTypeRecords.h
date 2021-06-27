#pragma once

#include "Rpc.h"

namespace SRPC
{

struct RpcTypeBaseInfo
{
	uint16_t TypeId;
	RpcTypeAttributes Attributes; // optional

	struct
	{
		uint32_t Count;			// valid if array attribute is set
	} Array;
};

struct RpcTypeInfo
{
	RpcTypeBaseInfo Base;

	struct
	{
		uint8_t FieldsCount;	// Number of fields
		uint8_t PackingShift;	// structure packing (1 << Packing)

		RpcTypeBaseInfo FieldTypes[0xff];
		char Name[0x100];
	} Struct;
};

class RpcTypeRecords
{
public:
	RpcTypeRecords();
	~RpcTypeRecords();

	uint16_t AddUDT(uint8_t PackingShift, const char *Name);
	uint8_t AddField(uint16_t TypeId, uint16_t FieldTypeId);
	uint8_t AddField(uint16_t TypeId, RpcTypeId FieldTypeId);
	bool SetArrayAttribute(uint16_t TypeId, uint32_t ArrayCount);
	bool SetArrayAttribute(uint16_t TypeId, uint8_t FieldId, uint32_t ArrayCount);
	bool SetOutAttribute(uint16_t TypeId);
	bool SetOutAttribute(uint16_t TypeId, uint8_t FieldId);

	uint8_t AddParamsTypeField(uint16_t FieldTypeId);
	uint8_t AddParamsTypeField(RpcTypeId FieldTypeId);
	bool SetParamsTypeFieldArrayAttribute(uint8_t FieldId, uint32_t ArrayCount);
	bool SetParamsTypeFieldOutAttribute(uint8_t FieldId);

	// Serialization
	size_t Serialize(uint8_t *Buffer, size_t Size);
	static RpcTypeRecords *Deserialize(uint8_t *Buffer, size_t Size);

private:
	uint16_t AllocateTypeId();
	bool FreeTypeId(uint16_t TypeId);

	bool IsTypeIdInUDTRange(uint16_t TypeId) const;
	bool IsTypeIdPrimitive(uint16_t TypeId) const;

	size_t GetTypeRecordRawSize(RpcTypeInfo& Info);
	size_t GetTypeRecordRawSize(uint16_t TypeId);
	std::vector<uint16_t> GetReferencedTypeIdList(uint16_t RootTypeId);

	// Contains only UDTs (user-defined type).
	constexpr static const size_t TypeInfoCount = 0x100;
	static_assert(static_cast<size_t>(RpcTypeId::UDT_END) < RpcTypeRecords::TypeInfoCount, "TypeInfoCount <= UDT_END !!");

	std::array<RpcTypeInfo, RpcTypeRecords::TypeInfoCount> UDT_; // <TypeId, RpcTypeInfo>
	RpcTypeInfo ParamsType_;
};


}
