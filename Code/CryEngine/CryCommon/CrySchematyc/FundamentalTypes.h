// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include <CrySerialization/Forward.h>
#include <CryEntitySystem/IEntityBasicTypes.h>

#include "CrySchematyc/Utils/GUID.h"
#include "CrySchematyc/Utils/PreprocessorUtils.h"

#include "CrySchematyc/Reflection/TypeDesc.h"

#define SCHEMATYC_SOURCE_FILE_INFO Schematyc::SSourceFileInfo(SCHEMATYC_FILE_NAME, SCHEMATYC_LINE_NUMBER)

namespace Schematyc
{
enum : uint32
{
	InvalidId = ~0u,
	InvalidIdx = ~0u
};

enum class EVisitStatus
{
	Continue,   // Continue visit.
	Recurse,    // Continue visit recursively (if applicable).
	Stop,       // Stop visit.
	Error       // Stop visit because error occurred.
};

enum class EVisitResult
{
	Complete,   // Visit was completed.
	Stopped,    // Visit was stopped before completion.
	Error       // Visit was stopped before completion error occurred.
};

// In-place storage parameters.
struct SInPlaceStorageParams
{
	explicit inline SInPlaceStorageParams(uint32 _capacity, void* _pData)
		: capacity(_capacity)
		, pData(_pData)
	{}

	uint32 capacity;
	void*  pData;
};

struct SSourceFileInfo
{
	explicit inline SSourceFileInfo(const char* _szFileName, uint32 _lineNumber)
		: szFileName(_szFileName)
		, lineNumber(_lineNumber)
	{}

	const char* szFileName;
	uint32      lineNumber;
};

enum class EDomain /* : uint8*/ // Ideally this would have uint8 as an underlying type but that screws with serialization.
{
	Unknown,
	Env,
	Script
};

struct SElementId
{
	inline SElementId()
		: domain(EDomain::Unknown)
		, guid(CryGUID())
	{}

	inline SElementId(EDomain _domain, const CryGUID& _guid)
		: domain(_domain)
		, guid(_guid)
	{}

	inline void Serialize(Serialization::IArchive& archive)
	{
		archive(domain, "domain");
		archive(guid, "guid");
	}

	inline bool operator==(const SElementId& rhs) const
	{
		return (domain == rhs.domain) && (guid == rhs.guid);
	}

	inline bool operator!=(const SElementId& rhs) const
	{
		return (domain != rhs.domain) || (guid != rhs.guid);
	}

	EDomain domain;
	CryGUID   guid;
};

enum class EOverridePolicy // #SchematycTODO : Move to IScriptElement.h and rename EScriptElementOverridePolicy?
{
	Default,
	Override,
	Final
};

using ESimulationMode = EEntitySimulationMode;

enum class ObjectId : uint32
{
	Invalid = 0xffffffff
};

// Reflect 'ObjectId' type.
inline void ObjectIdToString(IString& output, const ObjectId& input)
{
	output.Format("%d", static_cast<uint32>(input));
}

inline void ReflectType(CTypeDesc<ObjectId>& desc)
{
	desc.SetGUID("95b8918e-9e65-4b6c-9c48-8899754f9d3c"_cry_guid);
	desc.SetLabel("ObjectId");
	desc.SetDescription("Object id");
	desc.SetDefaultValue(ObjectId::Invalid);
	desc.SetToStringOperator<&ObjectIdToString>();
}

inline bool Serialize(Serialization::IArchive& archive, ObjectId& value, const char* szName, const char* szLabel)
{
	if (!archive.isEdit())
	{
		return archive(static_cast<uint32>(value), szName, szLabel);
	}
	return true;
}

constexpr const char* g_szCrytek = "Crytek GmbH";
constexpr const char* g_szNoType = "No Type";
} // Schematyc

enum { ESYSTEM_EVENT_REGISTER_SCHEMATYC_ENV = ESYSTEM_EVENT_GAME_POST_INIT_DONE };
