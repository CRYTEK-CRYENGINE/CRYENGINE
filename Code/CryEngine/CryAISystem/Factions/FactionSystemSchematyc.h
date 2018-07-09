// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include "FactionSystem.h"

bool Serialize(Serialization::IArchive& archive, SFactionID& value, const char* szName, const char* szLabel);
bool Serialize(Serialization::IArchive& archive, SFactionFlagsMask& value, const char* szName, const char* szLabel);

inline void ReflectType(Schematyc::CTypeDesc<IFactionMap::ReactionType>& desc)
{
	desc.SetGUID("9317594f-1e3c-4d8e-8dba-3b67bf67b935"_cry_guid);

	desc.AddConstant(IFactionMap::ReactionType::Hostile, "Hostile", "Hostile");
	desc.AddConstant(IFactionMap::ReactionType::Neutral, "Neutral", "Neutral");
	desc.AddConstant(IFactionMap::ReactionType::Friendly, "Friendly", "Friendly");
}

inline void ReflectType(Schematyc::CTypeDesc<SFactionID>& desc)
{
	desc.SetGUID("8ae8716f-af8a-49d3-be75-bfc7dc81e84a"_cry_guid);
}

inline void ReflectType(Schematyc::CTypeDesc<SFactionFlagsMask>& desc)
{
	desc.SetGUID("a060b5fd-72f5-44b8-9237-a221474a41ad"_cry_guid);
}

namespace FactionSystemSchematyc
{
	void Register(Schematyc::IEnvRegistrar& registrar, Schematyc::CEnvRegistrationScope& parentScope);
}