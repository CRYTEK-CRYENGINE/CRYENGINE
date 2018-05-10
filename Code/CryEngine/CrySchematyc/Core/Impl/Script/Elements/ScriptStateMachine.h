// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include <CrySchematyc/Script/Elements/IScriptStateMachine.h>
#include <CrySchematyc/SerializationUtils/MultiPassSerializer.h>

#include "Script/ScriptElementBase.h"

namespace Schematyc
{
class CScriptStateMachine : public CScriptElementBase<IScriptStateMachine>, public CMultiPassSerializer
{
public:

	CScriptStateMachine();
	CScriptStateMachine(const CryGUID& guid, const char* szName, EScriptStateMachineLifetime lifetime, const CryGUID& contextGUID, const CryGUID& partnerGUID);

	// IScriptElement
	virtual void EnumerateDependencies(const ScriptDependencyEnumerator& enumerator, EScriptDependencyType type) const override;
	virtual void RemapDependencies(IGUIDRemapper& guidRemapper) override;
	virtual void ProcessEvent(const SScriptEvent& event) override;
	virtual void Serialize(Serialization::IArchive& archive) override;
	// ~IScriptElement

	// IScriptStateMachine
	virtual EScriptStateMachineLifetime GetLifetime() const override;
	virtual CryGUID                       GetContextGUID() const override;
	virtual CryGUID                       GetPartnerGUID() const override;
	// ~IScriptStateMachine

protected:

	// CMultiPassSerializer
	virtual void Load(Serialization::IArchive& archive, const ISerializationContext& context) override;
	virtual void Save(Serialization::IArchive& archive, const ISerializationContext& context) override;
	virtual void Edit(Serialization::IArchive& archive, const ISerializationContext& context) override;
	// ~CMultiPassSerializer

private:

	void CreateTransitionGraph();
	void RefreshTransitionGraph();

private:

	EScriptStateMachineLifetime m_lifetime;
	CryGUID                       m_contextGUID;
	CryGUID                       m_partnerGUID;
	CryGUID                       m_transitionGraphGUID;
};
} // Schematyc
