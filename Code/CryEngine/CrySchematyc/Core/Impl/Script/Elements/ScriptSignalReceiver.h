// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include <CrySchematyc/Script/Elements/IScriptSignalReceiver.h>
#include <CrySchematyc/SerializationUtils/MultiPassSerializer.h>
#include <CrySchematyc/Utils/GUID.h>

#include "Script/ScriptElementBase.h"
#include "Script/ScriptUserDocumentation.h"

namespace Schematyc
{
class CScriptSignalReceiver : public CScriptElementBase<IScriptSignalReceiver>, public CMultiPassSerializer
{
public:

	CScriptSignalReceiver();
	CScriptSignalReceiver(const CryGUID& guid, const char* szName, EScriptSignalReceiverType type, const CryGUID& signalGUID);

	// IScriptElement
	virtual void EnumerateDependencies(const ScriptDependencyEnumerator& enumerator, EScriptDependencyType type) const override;
	virtual void RemapDependencies(IGUIDRemapper& guidRemapper) override;
	virtual void ProcessEvent(const SScriptEvent& event) override;
	virtual void Serialize(Serialization::IArchive& archive) override;
	// ~IScriptElement

	// IScriptSignalReceiver
	virtual EScriptSignalReceiverType GetSignalReceiverType() const override;
	virtual CryGUID                     GetSignalGUID() const override;
	// ~IScriptSignalReceiver

protected:

	// CMultiPassSerializer
	virtual void LoadDependencies(Serialization::IArchive& archive, const ISerializationContext& context) override;
	virtual void Load(Serialization::IArchive& archive, const ISerializationContext& context) override;
	virtual void Save(Serialization::IArchive& archive, const ISerializationContext& context) override;
	virtual void Edit(Serialization::IArchive& archive, const ISerializationContext& context) override;
	virtual void Validate(Serialization::IArchive& archive, const ISerializationContext& context) override;
	// ~CMultiPassSerializer

private:

	void CreateGraph();
	void GoToSignal();

private:

	EScriptSignalReceiverType m_type;
	CryGUID                     m_signalGUID;
	SScriptUserDocumentation  m_userDocumentation;
};
} // Schematyc
