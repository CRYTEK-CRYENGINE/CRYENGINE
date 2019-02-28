// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include <CrySchematyc/Script/Elements/IScriptModule.h>
#include <CrySchematyc/SerializationUtils/MultiPassSerializer.h>

#include "Script/ScriptElementBase.h"

namespace Schematyc
{
class CScriptModule : public CScriptElementBase<IScriptModule>, public CMultiPassSerializer
{
public:

	CScriptModule();
	CScriptModule(const CryGUID& guid, const char* szName);

	// IScriptElement
	virtual void EnumerateDependencies(const ScriptDependencyEnumerator& enumerator, EScriptDependencyType type) const override;
	virtual void RemapDependencies(IGUIDRemapper& guidRemapper) override;
	virtual void ProcessEvent(const SScriptEvent& event) override;
	virtual void Serialize(Serialization::IArchive& archive) override;
	// ~IScriptElement

protected:
	// CMultiPassSerializer
	virtual void Save(Serialization::IArchive& archive, const ISerializationContext& context) override { CMultiPassSerializer::Save(archive, context); }
	// ~CMultiPassSerializer
};
} // Schematyc
