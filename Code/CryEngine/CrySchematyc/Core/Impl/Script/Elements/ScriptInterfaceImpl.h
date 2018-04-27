// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include <CrySchematyc/Script/Elements/IScriptInterfaceImpl.h>
#include <CrySchematyc/SerializationUtils/MultiPassSerializer.h>

#include "Script/ScriptElementBase.h"

namespace Schematyc
{
// Forward declare interfaces.
struct IEnvInterface;

class CScriptInterfaceImpl : public CScriptElementBase<IScriptInterfaceImpl>, public CMultiPassSerializer
{
public:

	CScriptInterfaceImpl();
	CScriptInterfaceImpl(const CryGUID& guid, EDomain domain, const CryGUID& refGUID);

	// IScriptElement
	virtual void EnumerateDependencies(const ScriptDependencyEnumerator& enumerator, EScriptDependencyType type) const override;
	virtual void RemapDependencies(IGUIDRemapper& guidRemapper) override;
	virtual void ProcessEvent(const SScriptEvent& event) override;
	virtual void Serialize(Serialization::IArchive& archive) override;
	// ~IScriptElement

	// IScriptInterfaceImpl
	virtual EDomain GetDomain() const override;
	virtual CryGUID   GetRefGUID() const override;
	// ~IScriptInterfaceImpl

protected:

	// CMultiPassSerializer
	virtual void LoadDependencies(Serialization::IArchive& archive, const ISerializationContext& context) override;
	virtual void Save(Serialization::IArchive& archive, const ISerializationContext& context) override;
	virtual void Validate(Serialization::IArchive& archive, const ISerializationContext& context) override;
	// ~CMultiPassSerializer

private:

	void RefreshEnvInterfaceFunctions(const IEnvInterface& envInterface);
	//void RefreshScriptInterfaceFunctions(const IScriptFile& interfaceFile);
	//void RefreshScriptInterfaceTasks(const IScriptFile& iInterfaceFile);
	//void RefreshScriptInterfaceTaskPropertiess(const IScriptFile& interfaceFile, const CryGUID& taskGUID);

private:

	// #SchematycTODO : Replace m_domain and m_refGUID with SElementId!!!
	EDomain m_domain;
	CryGUID   m_refGUID;
};
} // Schematyc
