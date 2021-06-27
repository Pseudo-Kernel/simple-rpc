#include "RpcTypeRecords.h"

namespace SRPC
{

RpcTypeRecords::RpcTypeRecords() : UDT_{}, ParamsType_{}
{
	ParamsType_.Base.TypeId = static_cast<uint16_t>(RpcTypeId::PARAM_LIST);
	ParamsType_.Base.Attributes = RpcTypeAttributes::None;
}

RpcTypeRecords::~RpcTypeRecords()
{
}

uint16_t RpcTypeRecords::AddUDT(uint8_t PackingShift, const char * Name)
{
	uint16_t TypeId = AllocateTypeId();
	if (!TypeId)
		return 0;

	auto& TypeInfo = UDT_[TypeId];
	TypeInfo.Struct.PackingShift = PackingShift;
	strcpy_s(TypeInfo.Struct.Name, Name);

	return TypeId;
}

uint8_t RpcTypeRecords::AddField(uint16_t TypeId, uint16_t FieldTypeId)
{
	if (!IsTypeIdInUDTRange(TypeId))
		return ~0; // we accept only UDT

	if (!IsTypeIdInUDTRange(FieldTypeId) && !IsTypeIdPrimitive(FieldTypeId))
		return ~0; // neither UDT nor primitive type

	if (TypeId == FieldTypeId)
		return ~0; // typeid circular reference

	auto& TypeInfo = UDT_[TypeId];
	if (TypeInfo.Struct.FieldsCount >= std::size(TypeInfo.Struct.FieldTypes))
		return ~0;

	uint8_t FieldId = TypeInfo.Struct.FieldsCount++;
	TypeInfo.Struct.FieldTypes[FieldId].TypeId = FieldTypeId;

	return FieldId;
}

uint8_t RpcTypeRecords::AddField(uint16_t TypeId, RpcTypeId FieldTypeId)
{
	return AddField(TypeId, static_cast<uint16_t>(FieldTypeId));
}

bool RpcTypeRecords::SetArrayAttribute(uint16_t TypeId, uint32_t ArrayCount)
{
	// Set array attribute for UDT.

	if (!IsTypeIdInUDTRange(TypeId))
		return false;

	auto& TypeInfo = UDT_[TypeId];
	TypeInfo.Base.Attributes |= RpcTypeAttributes::Array;
	TypeInfo.Base.Array.Count = ArrayCount;

	return true;
}

bool RpcTypeRecords::SetArrayAttribute(uint16_t TypeId, uint8_t FieldId, uint32_t ArrayCount)
{
	// Set array attribute for field.

	if (!IsTypeIdInUDTRange(TypeId))
		return false;

	auto& TypeInfo = UDT_[TypeId];
	if (FieldId >= std::size(TypeInfo.Struct.FieldTypes))
		return false;

	auto& FieldInfo = TypeInfo.Struct.FieldTypes[FieldId];
	FieldInfo.Attributes |= RpcTypeAttributes::Array;
	FieldInfo.Array.Count = ArrayCount;

	return true;
}

bool RpcTypeRecords::SetOutAttribute(uint16_t TypeId)
{
	// Set output attribute for UDT.

	if (!IsTypeIdInUDTRange(TypeId))
		return false;

	auto& TypeInfo = UDT_[TypeId];
	TypeInfo.Base.Attributes |= RpcTypeAttributes::OutAttribute;

	return true;
}

bool RpcTypeRecords::SetOutAttribute(uint16_t TypeId, uint8_t FieldId)
{
	// Set output attribute for field.

	if (!IsTypeIdInUDTRange(TypeId))
		return false;

	auto& TypeInfo = UDT_[TypeId];
	if (FieldId >= std::size(TypeInfo.Struct.FieldTypes))
		return false;

	auto& FieldInfo = TypeInfo.Struct.FieldTypes[FieldId];
	FieldInfo.Attributes |= RpcTypeAttributes::OutAttribute;

	return true;
}

uint8_t RpcTypeRecords::AddParamsTypeField(uint16_t FieldTypeId)
{
	auto& TypeInfo = ParamsType_;
	if (TypeInfo.Struct.FieldsCount >= std::size(TypeInfo.Struct.FieldTypes))
		return ~0;

	uint8_t FieldId = TypeInfo.Struct.FieldsCount++;
	TypeInfo.Struct.FieldTypes[FieldId].TypeId = FieldTypeId;

	return FieldId;
}

uint8_t RpcTypeRecords::AddParamsTypeField(RpcTypeId FieldTypeId)
{
	return AddParamsTypeField(static_cast<uint16_t>(FieldTypeId));
}

bool RpcTypeRecords::SetParamsTypeFieldArrayAttribute(uint8_t FieldId, uint32_t ArrayCount)
{
	// Set array attribute for field in ParamsType.

	auto& TypeInfo = ParamsType_;
	if (FieldId >= std::size(TypeInfo.Struct.FieldTypes))
		return false;

	auto& FieldInfo = TypeInfo.Struct.FieldTypes[FieldId];
	FieldInfo.Attributes |= RpcTypeAttributes::Array;
	FieldInfo.Array.Count = ArrayCount;

	return true;
}

bool RpcTypeRecords::SetParamsTypeFieldOutAttribute(uint8_t FieldId)
{
	// Set output attribute for field.

	auto& TypeInfo = ParamsType_;
	if (FieldId >= std::size(TypeInfo.Struct.FieldTypes))
		return false;

	auto& FieldInfo = TypeInfo.Struct.FieldTypes[FieldId];
	FieldInfo.Attributes |= RpcTypeAttributes::OutAttribute;

	return true;
}

size_t RpcTypeRecords::Serialize(uint8_t * Buffer, size_t Size)
{
	// FunctionCallTypeRecord: <LEN1><PARAM_LIST> <LEN2><REF_TYPE_LIST>
	// PARAM_LIST: [TypeRecord_Return][TypeRecord_Param1]...[TypeRecord_ParamN]
	// REF_TYPE_LIST: [TypeRecord_Ref1][TypeRecord_Ref2]...[TypeRecord_RefN]

	// Non-UDT:        TypeId(2)/Attributes(1)
	// Non-UDT Array:  TypeId(2)/Attributes(1) / ArrayCount(4)
	// PARAM_LIST/UDT: TypeId(2)/Attributes(1) / Packing(1)/FieldsCount(1)
	// UDT Array:      TypeId(2)/Attributes(1) / ArrayCount(4)/Packing(1)/FieldsCount(1)

	size_t ParamListSize = GetTypeRecordRawSize(ParamsType_);
	size_t RefSize = 0;

	auto TypeIdList = GetReferencedTypeIdList(static_cast<uint16_t>(RpcTypeId::PARAM_LIST));
	for (auto TypeId : TypeIdList)
	{
		RefSize += GetTypeRecordRawSize(TypeId);
	}

	// FIXME: Calculate and serialize type records (PARAM_LIST and Referenced Types)
	Assert(false);

	return size_t(0);
}

RpcTypeRecords * RpcTypeRecords::Deserialize(uint8_t * Buffer, size_t Size)
{
	return nullptr;
}

uint16_t RpcTypeRecords::AllocateTypeId()
{
	for (size_t i = static_cast<size_t>(RpcTypeId::UDT_START);
		i <= static_cast<size_t>(RpcTypeId::UDT_END); i++)
	{
		auto& TypeInfo = UDT_[i];
		if (TypeInfo.Base.TypeId == 0)
		{
			Assert(i <= 0xffff);

			memset(&TypeInfo, 0, sizeof(TypeInfo));
			TypeInfo.Base.TypeId = static_cast<uint16_t>(i);
			return TypeInfo.Base.TypeId;
		}
	}

	return 0;
}

bool RpcTypeRecords::FreeTypeId(uint16_t TypeId)
{
	if (!IsTypeIdInUDTRange(TypeId))
		return false;

	auto& TypeInfo = UDT_[TypeId];
	Assert(TypeInfo.Base.TypeId == TypeId);

	TypeInfo.Base.TypeId = 0;
	return true;
}

bool RpcTypeRecords::IsTypeIdInUDTRange(uint16_t TypeId) const
{
	if (static_cast<uint16_t>(RpcTypeId::UDT_START) <= TypeId &&
		TypeId <= static_cast<uint16_t>(RpcTypeId::UDT_END))
		return true;

	return false;
}

bool RpcTypeRecords::IsTypeIdPrimitive(uint16_t TypeId) const
{
	if (static_cast<uint16_t>(RpcTypeId::PRIMITIVE_START) <= TypeId &&
		TypeId <= static_cast<uint16_t>(RpcTypeId::PRIMITIVE_END))
		return true;

	return false;
}

size_t RpcTypeRecords::GetTypeRecordRawSize(RpcTypeInfo & Info)
{
	size_t Size =
		sizeof(Info.Base.TypeId) +
		sizeof(Info.Base.Attributes);
	
	if (Info.Base.Attributes & RpcTypeAttributes::Array)
		Size += sizeof(Info.Base.Array.Count);

	if (IsTypeIdInUDTRange(Info.Base.TypeId) ||
		Info.Base.TypeId == static_cast<uint16_t>(RpcTypeId::PARAM_LIST))
	{
		Size += sizeof(Info.Struct.PackingShift) + sizeof(Info.Struct.FieldsCount);

		for (uint8_t FieldId = 0; FieldId < Info.Struct.FieldsCount; FieldId++)
		{
			auto& FieldTypeInfo = Info.Struct.FieldTypes[FieldId];
			Assert(
				IsTypeIdPrimitive(FieldTypeInfo.TypeId) ||
				IsTypeIdInUDTRange(FieldTypeInfo.TypeId));

			size_t FieldTypeRecordSize = sizeof(FieldTypeInfo.TypeId) + sizeof(FieldTypeInfo.Attributes);

			if (FieldTypeInfo.Attributes & RpcTypeAttributes::Array)
				FieldTypeRecordSize += sizeof(FieldTypeInfo.Array.Count);

			Size += FieldTypeRecordSize;
		}
	}

	return Size;
}

size_t RpcTypeRecords::GetTypeRecordRawSize(uint16_t TypeId)
{
	if (TypeId == static_cast<uint16_t>(RpcTypeId::PARAM_LIST))
		return GetTypeRecordRawSize(ParamsType_);
	else if (IsTypeIdInUDTRange(TypeId))
		return GetTypeRecordRawSize(UDT_[TypeId]);

	return 0;
}

std::vector<uint16_t> RpcTypeRecords::GetReferencedTypeIdList(uint16_t RootTypeId)
{
	std::map<uint16_t, bool> TypeMap;
	std::queue<uint16_t> Queue;

	Queue.push(RootTypeId);

	while (!Queue.empty())
	{
		uint16_t TypeIdCurrent = Queue.front();
		Queue.pop();

		RpcTypeInfo *Info = nullptr;

		if (TypeIdCurrent == static_cast<uint16_t>(RpcTypeId::PARAM_LIST))
			Info = &ParamsType_;
		else if (IsTypeIdInUDTRange(TypeIdCurrent))
			Info = &UDT_[TypeIdCurrent];
		else if (IsTypeIdPrimitive(TypeIdCurrent))
			; // primitive type, nothing to do
		else
			Assert(false);

		if (!Info)
			continue;

		for (uint8_t FieldId = 0; FieldId < Info->Struct.FieldsCount; FieldId++)
		{
			uint16_t TypeIdNext = Info->Struct.FieldTypes[FieldId].TypeId;

			if (!IsTypeIdInUDTRange(TypeIdNext))
				continue;

			auto ResultPair = TypeMap.try_emplace(TypeIdNext, true);
			if (ResultPair.second)
			{
				// Add to the queue if emplace was successful.
				Queue.push(TypeIdNext);
			}
		}
	}

	std::vector<uint16_t> Result;
	for (auto& it : TypeMap)
		Result.push_back(it.first);

	return std::move(Result);
}

}

