// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include <CrySchematyc/Script/Elements/IScriptInterfaceTask.h>
#include <CrySchematyc/SerializationUtils/MultiPassSerializer.h>

#include "Script/ScriptElementBase.h"
#include "Script/ScriptUserDocumentation.h"

namespace Schematyc
{
class CScriptInterfaceTask : public CScriptElementBase<IScriptInterfaceTask>, public CMultiPassSerializer
{
public:

	CScriptInterfaceTask();
	CScriptInterfaceTask(const CryGUID& guid, const char* szName);

	// IScriptElement
	virtual void EnumerateDependencies(const ScriptDependencyEnumerator& enumerator, EScriptDependencyType type) const override;
	virtual void RemapDependencies(IGUIDRemapper& guidRemapper) override;
	virtual void ProcessEvent(const SScriptEvent& event) override;
	virtual void Serialize(Serialization::IArchive& archive) override;
	// ~IScriptElement

	// IScriptInterfaceTask
	virtual const char* GetAuthor() const override;
	virtual const char* GetDescription() const override;
	// ~IScriptInterfaceTask

protected:

	// CMultiPassSerializer
	virtual void Load(Serialization::IArchive& archive, const ISerializationContext& context) override;
	virtual void Save(Serialization::IArchive& archive, const ISerializationContext& context) override;
	virtual void Edit(Serialization::IArchive& archive, const ISerializationContext& context) override;
	// ~CMultiPassSerializer

private:

	SScriptUserDocumentation m_userDocumentation;
};

} // Schematyc
