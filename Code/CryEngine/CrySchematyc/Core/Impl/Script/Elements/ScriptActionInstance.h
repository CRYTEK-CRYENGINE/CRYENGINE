// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include <CrySchematyc/Script/Elements/IScriptActionInstance.h>
#include <CrySchematyc/SerializationUtils/MultiPassSerializer.h>
#include <CrySchematyc/Utils/ClassProperties.h>

#include "Script/ScriptElementBase.h"

namespace Schematyc
{

class CScriptActionInstance : public CScriptElementBase<IScriptActionInstance>, public CMultiPassSerializer
{
public:

	CScriptActionInstance();
	CScriptActionInstance(const CryGUID& guid, const char* szName, const CryGUID& actionTypeGUID, const CryGUID& componentInstanceGUID);

	// IScriptElement
	virtual void EnumerateDependencies(const ScriptDependencyEnumerator& enumerator, EScriptDependencyType type) const override;
	virtual void RemapDependencies(IGUIDRemapper& guidRemapper) override;
	virtual void ProcessEvent(const SScriptEvent& event) override;
	virtual void Serialize(Serialization::IArchive& archive) override;
	// ~IScriptElement

	// IScriptActionInstance
	virtual CryGUID                   GetActionTypeGUID() const override;
	virtual CryGUID                   GetComponentInstanceGUID() const override;
	virtual const CClassProperties& GetProperties() const override;
	// ~IScriptActionInstance

protected:

	// CMultiPassSerializer
	virtual void LoadDependencies(Serialization::IArchive& archive, const ISerializationContext& context) override;
	virtual void Load(Serialization::IArchive& archive, const ISerializationContext& context) override;
	virtual void Save(Serialization::IArchive& archive, const ISerializationContext& context) override;
	virtual void Edit(Serialization::IArchive& archive, const ISerializationContext& context) override;
	// ~CMultiPassSerializer

private:

	void RefreshProperties();
	void SerializeProperties(Serialization::IArchive& archive);

private:

	CryGUID            m_actionTypeGUID;
	CryGUID            m_componentInstanceGUID;
	CClassProperties m_properties;
};

} // Schematyc
