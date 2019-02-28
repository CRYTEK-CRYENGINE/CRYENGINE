// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "AggregateTypeIdSerialize.h"

#include <CrySchematyc2/IEnvTypeDesc.h>
#include <CrySchematyc2/Env/IEnvRegistry.h>
#include <CrySchematyc2/Services/ILog.h>

namespace Schematyc2
{
	enum class EDocVariableOrigin // Deprecated!!!
	{
		Unknown,
		Env,
		DocEnumeration,
		DocStructure
	};

	struct SDocVariableTypeInfo // Deprecated!!!
	{
		inline SDocVariableTypeInfo()
			: origin(EDocVariableOrigin::Unknown)
		{}

		inline SDocVariableTypeInfo(EDocVariableOrigin _origin, const SGUID& _guid)
			: origin(_origin)
			, guid(_guid)
		{}

		inline void Serialize(Serialization::IArchive& archive)
		{
			archive(origin, "origin");
			archive(guid, "guid");
		}

		inline bool operator == (const SDocVariableTypeInfo& other) const
		{
			return (origin == other.origin) && (guid == other.guid);
		}

		inline bool operator != (const SDocVariableTypeInfo& other) const
		{
			return (origin != other.origin) || (guid != other.guid);
		}

		EDocVariableOrigin origin;
		SGUID              guid;
	};

	struct SAggregateTypeIdSerializer
	{
		inline SAggregateTypeIdSerializer(CAggregateTypeId& _value)
			: value(_value)
		{}
		
		void Serialize(Serialization::IArchive& archive)
		{
			if(!archive.isEdit())
			{
				EDomain domain;
				SGUID   guid;
				if(archive.isOutput())
				{
					domain = value.GetDomain();
					switch(domain)
					{
					case EDomain::Env:
						{
							const IEnvTypeDesc* pEnvTypeDesc = gEnv->pSchematyc2->GetEnvRegistry().GetTypeDesc(value.AsEnvTypeId());
							SCHEMATYC2_SYSTEM_ASSERT(pEnvTypeDesc);
							if(pEnvTypeDesc)
							{
								guid = pEnvTypeDesc->GetGUID();
							}
							break;
						}
					case EDomain::Script:
						{
							guid = value.AsScriptTypeGUID();
							break;
						}
					}
				}
				archive(domain, "domain");
				archive(guid, "guid");
				if(archive.isInput())
				{
					switch(domain)
					{
					case EDomain::Env:
						{
							const IEnvTypeDesc* pEnvTypeDesc = gEnv->pSchematyc2->GetEnvRegistry().GetTypeDesc(guid);
							SCHEMATYC2_SYSTEM_ASSERT(pEnvTypeDesc);
							if(pEnvTypeDesc)
							{
								value = CAggregateTypeId::FromEnvTypeId(pEnvTypeDesc->GetEnvTypeId());
							}
							break;
						}
					case EDomain::Script:
						{
							value = CAggregateTypeId::FromScriptTypeGUID(guid);
							break;
						}
					}
				}
			}
		}

		CAggregateTypeId& value;
	};

	bool Serialize(Serialization::IArchive& archive, CAggregateTypeId& value, const char* szName, const char* szLabel)
	{
		return archive(SAggregateTypeIdSerializer(value), szName, szLabel);
	}

	bool PatchAggregateTypeIdFromDocVariableTypeInfo(Serialization::IArchive& archive, CAggregateTypeId& value, const char* szName)
	{
		if(archive.isInput())
		{
			SDocVariableTypeInfo docVariableTypeInfo;
			if(archive(docVariableTypeInfo, szName))
			{
				switch(docVariableTypeInfo.origin)
				{
				case EDocVariableOrigin::Env:
					{
						const IEnvTypeDesc* pEnvTypeDesc = gEnv->pSchematyc2->GetEnvRegistry().GetTypeDesc(docVariableTypeInfo.guid);
						if(pEnvTypeDesc)
						{
							value = CAggregateTypeId::FromEnvTypeId(pEnvTypeDesc->GetEnvTypeId());
							return true;
						}
						break;
					}
				case EDocVariableOrigin::DocEnumeration:
				case EDocVariableOrigin::DocStructure:
					{
						value = CAggregateTypeId::FromScriptTypeGUID(docVariableTypeInfo.guid);
						return true;
					}
				}
			}
		}
		return false;
	}
}

SERIALIZATION_ENUM_BEGIN_NESTED(Schematyc2, EDocVariableOrigin, "Schematyc Document Variable Origin")
	SERIALIZATION_ENUM(Schematyc2::EDocVariableOrigin::Env, "env", "Environment")
	SERIALIZATION_ENUM(Schematyc2::EDocVariableOrigin::DocEnumeration, "doc_enumeration", "Document Enumeration")
	SERIALIZATION_ENUM(Schematyc2::EDocVariableOrigin::DocStructure, "doc_structure", "Document Structure")
SERIALIZATION_ENUM_END()
