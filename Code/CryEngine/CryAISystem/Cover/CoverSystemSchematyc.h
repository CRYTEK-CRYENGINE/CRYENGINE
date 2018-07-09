// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include "CoverSystem.h"

inline bool Serialize(Serialization::IArchive& archive, CoverID& value, const char* szName, const char* szLabel)
{
	if (!archive.isEdit())
	{
		if (archive.isInput())
		{
			uint32 rawValue;
			if (archive(rawValue, szName, szLabel))
			{
				value = CoverID(rawValue);
				return true;
			}
			return false;
		}
		else // Output
		{
			return archive(uint32(value), szName, szLabel);
		}
	}
	return true;
}

inline void ReflectType(Schematyc::CTypeDesc<CoverID>& desc)
{
	desc.SetGUID("62FD8F5A-F3D7-45B9-8384-9D7D9DBEDBAE"_cry_guid);
	desc.SetLabel("CoverId");
	desc.SetDescription("Cover identifier");
	desc.SetDefaultValue(CoverID());
}

namespace CoverSystemSchematyc
{
	void Register(Schematyc::IEnvRegistrar& registrar, Schematyc::CEnvRegistrationScope& parentScope);
}