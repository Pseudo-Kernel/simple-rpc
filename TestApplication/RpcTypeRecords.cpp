#include "RpcTypeRecords.h"

namespace SRPC
{

RpcTypeRecords::RpcTypeRecords() : UDT_{}
{
}

RpcTypeRecords::~RpcTypeRecords()
{
}

uint16_t RpcTypeRecords::AddUDT(uint16_t TypeId, uint8_t PackingShift, const char * Name)
{
	uint16_t NewTypeId = AllocateTypeId(TypeId);
	if (!NewTypeId)
		return 0;

	auto& TypeInfo = UDT_[NewTypeId];
	TypeInfo.Struct.PackingShift = PackingShift;
	if (Name)
		strcpy_s(TypeInfo.Struct.Name, Name);
	else
		TypeInfo.Struct.Name[0] = 0;

	return NewTypeId;
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

size_t RpcTypeRecords::Serialize(uint8_t * Buffer, size_t Size)
{
	// FunctionCallTypeRecord: <LEN1><PARAM_LIST> <LEN2><REF_TYPE_LIST>
	// PARAM_LIST: [TypeRecord_Return][TypeRecord_Param1]...[TypeRecord_ParamN]
	// REF_TYPE_LIST: [TypeRecord_Ref1][TypeRecord_Ref2]...[TypeRecord_RefN]

	// Non-UDT:        TypeId(2)/Attributes(1)
	// Non-UDT Array:  TypeId(2)/Attributes(1) / ArrayCount(4)
	// PARAM_LIST/UDT: TypeId(2)/Attributes(1) / Packing(1)/FieldsCount(1)
	// UDT Array:      TypeId(2)/Attributes(1) / ArrayCount(4)/Packing(1)/FieldsCount(1)

//	size_t ParamListSize = GetTypeRecordSize(static_cast<uint16_t>(RpcTypeId::PARAM_LIST));
//	size_t RefSize = 0;
//
//	auto TypeIdList = GetReferencedTypeIdList(static_cast<uint16_t>(RpcTypeId::PARAM_LIST));
//	for (auto TypeId : TypeIdList)
//	{
//		RefSize += GetTypeRecordSize(TypeId);
//	}

	ByteStream Stream(Buffer, Size);

	uint16_t ParamListTypeId = static_cast<uint16_t>(RpcTypeId::PARAM_LIST);
	size_t TypeRecordSizeWritten = WriteTypeRecord(ParamListTypeId, Buffer, Size);

	Stream.Advance(TypeRecordSizeWritten);

	auto TypeIdList = GetReferencedTypeIdList(ParamListTypeId);
	for (auto TypeId : TypeIdList)
	{
		size_t FieldTypeRecordSizeWritten = WriteTypeRecord(TypeId, Stream.Pointer(), Stream.Remaining());
		Stream.Advance(FieldTypeRecordSizeWritten);
	}

	return Stream.Current();
}

RpcTypeRecords * RpcTypeRecords::Deserialize(uint8_t * Buffer, size_t Size)
{
	auto TypeRecords = std::make_unique<RpcTypeRecords>();

	ByteStream Stream(Buffer, Size);
	bool Result = true;

	uint16_t TypeId = 0;
	RpcTypeAttributes Attributes = RpcTypeAttributes::None;
	uint32_t ArrayCount = 0;
	uint8_t PackingShift = 0;
	uint8_t FieldsCount = 0;

	while (Result && Stream.Remaining())
	{
		// Read header.
		Result &=
			Stream.Read2(&TypeId) &&
			Stream.Read1(reinterpret_cast<uint8_t *>(&Attributes));

		if (!Result ||
			!IsTypeIdInUDTRange(TypeId))
			return nullptr;

		if (Attributes & RpcTypeAttributes::Array)
			Result &= !!Stream.Read4(&ArrayCount);

		Result &=
			Stream.Read1(&PackingShift) &&
			Stream.Read1(&FieldsCount);

		if (!Result ||
			!TypeId ||
			TypeRecords->AddUDT(TypeId, PackingShift, nullptr) != TypeId)
			return nullptr;

		uint16_t FieldTypeId = 0;
		RpcTypeAttributes FieldTypeAttributes = RpcTypeAttributes::None;
		uint32_t FieldArrayCount = 0;

		for (uint8_t i = 0; i < FieldsCount; i++)
		{
			Result &=
				Stream.Read2(&FieldTypeId) &&
				Stream.Read1(reinterpret_cast<uint8_t *>(&FieldTypeAttributes));

			if (!Result)
				return nullptr;

			if (FieldTypeAttributes & RpcTypeAttributes::Array)
				Result &= !!Stream.Read4(&ArrayCount);

			uint8_t FieldId = TypeRecords->AddField(TypeId, FieldTypeId);
			if (FieldId == 0xff)
				return nullptr;

			if (FieldTypeAttributes & RpcTypeAttributes::Array)
				Result &= TypeRecords->SetArrayAttribute(TypeId, FieldId, FieldArrayCount);

			if (FieldTypeAttributes & RpcTypeAttributes::OutAttribute)
				Result &= TypeRecords->SetOutAttribute(TypeId, FieldId);
		}
	}

	if (Result && !Stream.Remaining())
		return TypeRecords.release();

	return nullptr;
}

void RpcTypeRecords::DebugDump()
{
	uint16_t Start = static_cast<uint16_t>(RpcTypeId::UDT_START);
	uint16_t End = static_cast<uint16_t>(RpcTypeId::UDT_END);

	static std::map<RpcTypeId, const char *> PrimitiveTypeMap
	{
		{ RpcTypeId::INT_8,		"INT8" }, 
		{ RpcTypeId::INT_16,	"INT16" }, 
		{ RpcTypeId::INT_32,	"INT32" }, 
		{ RpcTypeId::INT_64,	"INT64" }, 

		{ RpcTypeId::INT_8U, 	"UINT8" }, 
		{ RpcTypeId::INT_16U,	"UINT16" }, 
		{ RpcTypeId::INT_32U,	"UINT32" }, 
		{ RpcTypeId::INT_64U,	"UINT64" }, 

		{ RpcTypeId::FLOAT_32,	"FLOAT32" }, 
		{ RpcTypeId::FLOAT_64,	"FLOAT64" }, 

		{ RpcTypeId::CP_8, 		"CHAR8" }, 
		{ RpcTypeId::CP_16,		"CHAR16" }, 
		{ RpcTypeId::CP_32,		"CHAR32" }, 

		{ RpcTypeId::PTR_32,	"PTR32" }, 
		{ RpcTypeId::PTR_64,	"PTR64" }, 
	};

	for (uint16_t i = Start; i <= End; i++)
	{
		auto& TypeInfo = UDT_[i];
		if (TypeInfo.Base.TypeId != i)
			continue;

		// PrimitiveTypeMap.find()

		printf("Type 0x%hx: ", TypeInfo.Base.TypeId);

		if (TypeInfo.Base.TypeId == static_cast<uint16_t>(RpcTypeId::PARAM_LIST))
			printf("<FunctionCallParamList> ");

		if (TypeInfo.Base.Attributes & RpcTypeAttributes::OutAttribute)
			printf("Out ");

		if (TypeInfo.Base.Attributes & RpcTypeAttributes::Array)
			printf("Array[%u] ", TypeInfo.Base.Array.Count);

		printf("\n");

		printf("Struct `%s' [Packing %hhu, %hhu Fields] {\n",
			TypeInfo.Struct.Name,
			1 << TypeInfo.Struct.PackingShift,
			TypeInfo.Struct.FieldsCount);

		for (uint8_t j = 0; j < TypeInfo.Struct.FieldsCount; j++)
		{
			auto& FieldTypeInfo = TypeInfo.Struct.FieldTypes[j];

			printf("  Type 0x%hx: ", FieldTypeInfo.TypeId);

			auto& it = PrimitiveTypeMap.find(static_cast<RpcTypeId>(FieldTypeInfo.TypeId));
			if (it != PrimitiveTypeMap.end())
				printf("[%s] ", it->second);

			if (FieldTypeInfo.Attributes & RpcTypeAttributes::OutAttribute)
				printf("Out ");

			if (FieldTypeInfo.Attributes & RpcTypeAttributes::Array)
				printf("Array[%u] ", FieldTypeInfo.Array.Count);

			printf("\n");
		}

		printf("}\n");
	}
}

uint16_t RpcTypeRecords::AllocateTypeId(uint16_t TypeId)
{
	uint16_t TypeIdTarget = 0;

	if (IsTypeIdInUDTRange(TypeId))
	{
		if (UDT_[TypeId].Base.TypeId == 0)
			TypeIdTarget = TypeId;
	}
	else
	{
		for (uint16_t i = static_cast<uint16_t>(RpcTypeId::UDT_START);
			i <= static_cast<uint16_t>(RpcTypeId::UDT_END); i++)
		{
			if (i == static_cast<uint16_t>(RpcTypeId::PARAM_LIST))
				continue;

			if (UDT_[i].Base.TypeId == 0)
			{
				Assert(i <= 0xffff);
				TypeIdTarget = i;
				break;
			}
		}
	}

	if (TypeIdTarget)
	{
		auto& TypeInfo = UDT_[TypeIdTarget];
		memset(&TypeInfo, 0, sizeof(TypeInfo));
		TypeInfo.Base.TypeId = TypeIdTarget;
		return TypeIdTarget;
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

bool RpcTypeRecords::IsTypeIdInUDTRange(uint16_t TypeId)
{
	if (static_cast<uint16_t>(RpcTypeId::UDT_START) <= TypeId &&
		TypeId <= static_cast<uint16_t>(RpcTypeId::UDT_END))
		return true;

	return false;
}

bool RpcTypeRecords::IsTypeIdPrimitive(uint16_t TypeId)
{
	if (static_cast<uint16_t>(RpcTypeId::PRIMITIVE_START) <= TypeId &&
		TypeId <= static_cast<uint16_t>(RpcTypeId::PRIMITIVE_END))
		return true;

	return false;
}

size_t RpcTypeRecords::GetTypeRecordSize(RpcTypeInfo & Info)
{
	size_t Size =
		sizeof(Info.Base.TypeId) +
		sizeof(Info.Base.Attributes);
	
	if (Info.Base.Attributes & RpcTypeAttributes::Array)
		Size += sizeof(Info.Base.Array.Count);

	if (IsTypeIdInUDTRange(Info.Base.TypeId))
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

size_t RpcTypeRecords::GetTypeRecordSize(uint16_t TypeId)
{
	if (IsTypeIdInUDTRange(TypeId))
		return GetTypeRecordSize(UDT_[TypeId]);

	return 0;
}

size_t RpcTypeRecords::WriteTypeRecord(RpcTypeInfo & Info, uint8_t *Buffer, size_t Size)
{
	ByteStream Stream(Buffer, Size);

	size_t SizeWritten = Stream.Write2(Info.Base.TypeId) +
		Stream.Write1(Info.Base.Attributes);

	if (Info.Base.Attributes & RpcTypeAttributes::Array)
		SizeWritten += Stream.Write4(Info.Base.Array.Count);

	if (IsTypeIdInUDTRange(Info.Base.TypeId))
	{
		SizeWritten += Stream.Write1(Info.Struct.PackingShift) + Stream.Write1(Info.Struct.FieldsCount);

		for (uint8_t FieldId = 0; FieldId < Info.Struct.FieldsCount; FieldId++)
		{
			auto& FieldTypeInfo = Info.Struct.FieldTypes[FieldId];
			Assert(
				IsTypeIdPrimitive(FieldTypeInfo.TypeId) ||
				IsTypeIdInUDTRange(FieldTypeInfo.TypeId));

			SizeWritten += Stream.Write2(FieldTypeInfo.TypeId) + Stream.Write1(FieldTypeInfo.Attributes);

			if (FieldTypeInfo.Attributes & RpcTypeAttributes::Array)
				SizeWritten += Stream.Write4(FieldTypeInfo.Array.Count);
		}
	}

	return SizeWritten;
}

size_t RpcTypeRecords::WriteTypeRecord(uint16_t TypeId, uint8_t *Buffer, size_t Size)
{
	if (IsTypeIdInUDTRange(TypeId))
		return WriteTypeRecord(UDT_[TypeId], Buffer, Size);

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

		if (IsTypeIdInUDTRange(TypeIdCurrent))
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

