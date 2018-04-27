// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include <CrySerialization/IArchive.h>

#include <CryEntitySystem/IEntityBasicTypes.h>

#include "CrySchematyc/Utils/Array.h"
#include "CrySchematyc/Utils/IString.h"

#include <CrySerialization/Color.h>
#include <CrySerialization/Math.h>

namespace Schematyc
{

// Reflect 'bool' type.

inline void BoolToString(IString& output, const bool& input)
{
	output.assign(input ? "1" : "0");
}

inline void ReflectType(CTypeDesc<bool>& desc)
{
	desc.SetGUID("03211ee1-b8d3-42a5-bfdc-296fc535fe43"_cry_guid);
	desc.SetLabel("Bool");
	desc.SetDescription("Boolean");
	desc.SetDefaultValue(false);
	desc.SetToStringOperator<&BoolToString>();
}

// Reflect 'int32' type.

inline void Int32ToString(IString& output, const int32& input)
{
	char temp[16] = "";
	itoa(input, temp, 10);
	output.assign(temp);
}

inline void ReflectType(CTypeDesc<int32>& desc)
{
	desc.SetGUID("8bd755fd-ed92-42e8-a488-79e7f1051b1a"_cry_guid);
	desc.SetLabel("Int32");
	desc.SetDescription("Signed 32bit integer");
	desc.SetFlags(ETypeFlags::Switchable);
	desc.SetDefaultValue(0);
	desc.SetToStringOperator<&Int32ToString>();
}

// Reflect 'uint32' type.

inline void UInt32ToString(IString& output, const uint32& input)
{
	char temp[16] = "";
	ltoa(input, temp, 10);
	output.assign(temp);
}

inline void ReflectType(CTypeDesc<uint32>& desc)
{
	desc.SetGUID("db4b8137-5150-4246-b193-d8d509bec2d4"_cry_guid);
	desc.SetLabel("UInt32");
	desc.SetDescription("Unsigned 32bit integer");
	desc.SetFlags(ETypeFlags::Switchable);
	desc.SetDefaultValue(0);
	desc.SetToStringOperator<&UInt32ToString>();
}

// Reflect 'float' type.
// #SchematycTODO : Move to STDEnv/MathTypes.h?

inline void FloatToString(IString& output, const float& input)
{
	output.Format("%.8f", input);
}

inline void ReflectType(CTypeDesc<float>& desc) 
{
	desc.SetGUID("03d99d5a-cf2c-4f8a-8489-7da5b515c202"_cry_guid);
	desc.SetLabel("Float");
	desc.SetDescription("32bit floating point number");
	desc.SetDefaultValue(0.0f);
	desc.SetToStringOperator<&FloatToString>();
}

// Reflect 'Vec2' type.
// #SchematycTODO : Move to STDEnv/MathTypes.h?

inline void Vec2ToString(IString& output, const Vec2& input)
{
	output.Format("%.8f, %.8f", input.x, input.y);
}

inline void ReflectType(CTypeDesc<Vec2>& desc) 
{
	desc.SetGUID("6c607bf5-76d7-45d0-9b34-9d81a13c3c89"_cry_guid);
	desc.SetLabel("Vector2");
	desc.SetDescription("2d vector");
	desc.SetDefaultValue(ZERO);
	desc.SetToStringOperator<&Vec2ToString>();
}

// Reflect 'Vec3' type.
// #SchematycTODO : Move to STDEnv/MathTypes.h?

inline void Vec3ToString(IString& output, const Vec3& input)
{
	output.Format("%.8f, %.8f, %.8f", input.x, input.y, input.z);
}

inline void ReflectType(CTypeDesc<Vec3>& desc)
{
	desc.SetGUID("e01bd066-2a42-493d-bc43-436b233833d8"_cry_guid);
	desc.SetLabel("Vector3");
	desc.SetDescription("3d vector");
	desc.SetDefaultValue(ZERO);
	desc.SetToStringOperator<& Vec3ToString>();
}

//////////////////////////////////////////////////////////////////////////
// Reflect 'ColorF' type.

inline void ColorFToString(IString& output, const ColorF& input)
{
	output.Format("%.8f, %.8f, %.8f, %.8f", input.r, input.g, input.b, input.a);
}

inline void ReflectType(CTypeDesc<ColorF>& desc) // #SchematycTODO : Move to STDEnv/BasicTypes.h?
{
	desc.SetGUID("ec4b79e2-f585-4468-8b91-db01e4f84bfa"_cry_guid);
	desc.SetLabel("Color");
	desc.SetDefaultValue(Col_Black);
	desc.SetToStringOperator<&ColorFToString>();
}

//////////////////////////////////////////////////////////////////////////
// Reflect 'ColorB' type.
inline void ColorBToString(IString& output, const ColorB& input)
{
	output.Format("%d,%d,%d,%d", input.r, input.g, input.b, input.a);
}

inline void ReflectType(CTypeDesc<ColorB>& desc)
{
	desc.SetGUID("B4F12795-7331-41B2-B92C-0C50D29566DF"_cry_guid);
	desc.SetLabel("ColorB");
	desc.SetDescription("Color 4 bytes");
	desc.SetDefaultValue(ColorB(0, 0, 0, 0));
	desc.SetToStringOperator<&ColorBToString>();
}


// Reflect 'CryGUID' type.

inline void ReflectType(CTypeDesc<CryGUID>& desc)
{
	desc.SetGUID("816b083f-b77d-4133-ae84-8d24319b609e"_cry_guid);
	desc.SetLabel("GUID");
	desc.SetToStringOperator<&GUID::ToString>();
}

// Reflect 'CArray' type.

SCHEMATYC_DECLARE_ARRAY_TYPE(CArray)

template<typename ELEMENT_TYPE> inline void ReflectType(CTypeDesc<CArray<ELEMENT_TYPE>>& desc)
{
	desc.SetLabel("Array");
	desc.template SetArraySizeOperator<& CArray<ELEMENT_TYPE>::Size>();
	desc.template SetArrayAtOperator<& CArray<ELEMENT_TYPE>::At>();
	desc.template SetArrayAtConstOperator<& CArray<ELEMENT_TYPE>::At>();
	desc.template SetArrayPushBackOperator<& CArray<ELEMENT_TYPE>::PushBack>();
}

enum class ExplicitEntityId : EntityId
{
	Invalid = 0
};

inline bool Serialize(Serialization::IArchive& archive, ExplicitEntityId& value, const char* szName, const char* szLabel)
{
	if (!archive.isEdit())
	{
		return archive(static_cast<EntityId>(value), szName, szLabel);
	}
	return true;
}

inline void ExplicitEntityIdToString(IString& output, const ExplicitEntityId& input)
{
	output.Format("%d", static_cast<EntityId>(input));
}

inline void ReflectType(CTypeDesc<ExplicitEntityId>& desc)
{
	desc.SetGUID("00782e22-3188-4538-b4f2-8749b8a9dc48"_cry_guid);
	desc.SetLabel("Entity");
	desc.SetDescription("An entity instance present in the current scene");
	desc.SetDefaultValue(ExplicitEntityId::Invalid);
	desc.SetToStringOperator<&ExplicitEntityIdToString>();
}

enum class ExplicitTextureId : int
{
	Invalid = 0
};

inline bool Serialize(Serialization::IArchive& archive, ExplicitTextureId& value, const char* szName, const char* szLabel)
{
	if (!archive.isEdit())
	{
		return archive(static_cast<int>(value), szName, szLabel);
	}
	return true;
}

inline void ToString(IString& output, const ExplicitTextureId& input)
{
	output.Format("%i", static_cast<int>(input));
}

inline void ReflectType(CTypeDesc<ExplicitTextureId>& desc)
{
	desc.SetGUID("1A53C752-8E24-4BCF-8FD4-BA60E8ADBE3F"_cry_guid);
	desc.SetLabel("TextureId");
	desc.SetDescription("Texture Identifier");
	desc.SetDefaultValue(ExplicitTextureId::Invalid);
	desc.SetToStringOperator<&ToString>();
}


} // Schematyc
