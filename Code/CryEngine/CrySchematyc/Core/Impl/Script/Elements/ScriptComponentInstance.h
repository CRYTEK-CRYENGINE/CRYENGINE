// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include <CrySchematyc/Script/Elements/IScriptComponentInstance.h>
#include <CrySchematyc/SerializationUtils/MultiPassSerializer.h>
#include <CrySchematyc/Utils/ClassProperties.h>
#include <CrySchematyc/Utils/Transform.h>

#include "Script/ScriptElementBase.h"

namespace Schematyc
{

class CScriptComponentInstance : public CScriptElementBase<IScriptComponentInstance>, public CMultiPassSerializer
{
public:

	CScriptComponentInstance();
	CScriptComponentInstance(const CryGUID& guid, const char* szName, const CryGUID& typeGUID);

	// IScriptElement
	virtual EScriptElementAccessor GetAccessor() const override;
	virtual void                   EnumerateDependencies(const ScriptDependencyEnumerator& enumerator, EScriptDependencyType type) const override;
	virtual void                   RemapDependencies(IGUIDRemapper& guidRemapper) override;
	virtual void                   ProcessEvent(const SScriptEvent& event) override;
	virtual void                   Serialize(Serialization::IArchive& archive) override;
	// ~IScriptElement

	// IScriptComponentInstance
	virtual CryGUID                        GetTypeGUID() const override;
	virtual ScriptComponentInstanceFlags GetComponentInstanceFlags() const override;
	virtual bool                         HasTransform() const override;
	virtual void                         SetTransform(const CTransformPtr& transform) override;
	virtual const CTransformPtr&         GetTransform() const override;
	virtual const CClassProperties&      GetProperties() const override;
	// ~IScriptComponentInstance

protected:

	// CMultiPassSerializer
	virtual void LoadDependencies(Serialization::IArchive& archive, const ISerializationContext& context) override;
	virtual void Load(Serialization::IArchive& archive, const ISerializationContext& context) override;
	virtual void Save(Serialization::IArchive& archive, const ISerializationContext& context) override;
	virtual void Edit(Serialization::IArchive& archive, const ISerializationContext& context) override;
	virtual void Validate(Serialization::IArchive& archive, const ISerializationContext& context) override;
	// ~CMultiPassSerializer

private:

	void ApplyComponent();

private:

	EScriptElementAccessor       m_accessor = EScriptElementAccessor::Private;
	CryGUID                      m_typeGUID;
	ScriptComponentInstanceFlags m_flags;
	bool                         m_bHasTransform = false;
	CTransformPtr                m_pTransform;
	CClassProperties             m_properties;
};

} // Schematyc
