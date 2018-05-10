// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include <CrySchematyc/SerializationUtils/MultiPassSerializer.h>
#include <CrySchematyc/Script/ScriptDependencyEnumerator.h>
#include <CrySchematyc/Utils/GUID.h>

#include "Script/ScriptVariableData.h"

namespace Schematyc
{
// Forward declare interfaces.
struct IGUIDRemapper;

struct SScriptParam : public CMultiPassSerializer
{
	// CMultiPassSerializer
	virtual void LoadDependencies(Serialization::IArchive& archive, const ISerializationContext& context) override;
	virtual void Load(Serialization::IArchive& archive, const ISerializationContext& context) override;
	virtual void Save(Serialization::IArchive& archive, const ISerializationContext& context) override;
	virtual void Edit(Serialization::IArchive& archive, const ISerializationContext& context) override;
	// ~CMultiPassSerializer

	CryGUID               guid;
	string              name;
	CScriptVariableData data;
};

typedef std::vector<SScriptParam> ScriptParams;

namespace ScriptParam
{
void EnumerateDependencies(const ScriptParams& params, const ScriptDependencyEnumerator& enumerator, EScriptDependencyType type);
void RemapDependencies(ScriptParams& params, IGUIDRemapper& guidRemapper);
} // ScriptParam
} // Schematyc
