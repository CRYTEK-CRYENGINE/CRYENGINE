// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include <CrySchematyc/Script/Elements/IScriptSignal.h>
#include <CrySchematyc/SerializationUtils/MultiPassSerializer.h>

#include "Script/ScriptElementBase.h"
#include "Script/ScriptParam.h"
#include "Script/ScriptUserDocumentation.h"

namespace Schematyc
{
// Forward declare classes.
class CAnyConstPtr;

class CScriptSignal : public CScriptElementBase<IScriptSignal>, public CMultiPassSerializer
{
public:

	CScriptSignal();
	CScriptSignal(const CryGUID& guid, const char* szName);

	// IScriptElement
	virtual void EnumerateDependencies(const ScriptDependencyEnumerator& enumerator, EScriptDependencyType type) const override;
	virtual void RemapDependencies(IGUIDRemapper& guidRemapper) override;
	virtual void ProcessEvent(const SScriptEvent& event) override;
	virtual void Serialize(Serialization::IArchive& archive) override;
	// ~IScriptElement

	// IScriptSignal
	virtual const char*  GetAuthor() const override;
	virtual const char*  GetDescription() const override;
	virtual uint32       GetInputCount() const override;
	virtual CryGUID        GetInputGUID(uint32 inputIdx) const override;
	virtual const char*  GetInputName(uint32 inputIdx) const override;
	virtual SElementId   GetInputTypeId(uint32 inputIdx) const override;
	virtual CAnyConstPtr GetInputData(uint32 inputIdx) const override;
	// ~IScriptSignal

protected:

	// CMultiPassSerializer
	virtual void LoadDependencies(Serialization::IArchive& archive, const ISerializationContext& context) override;
	virtual void Load(Serialization::IArchive& archive, const ISerializationContext& context) override;
	virtual void Save(Serialization::IArchive& archive, const ISerializationContext& context) override;
	virtual void Edit(Serialization::IArchive& archive, const ISerializationContext& context) override;
	// ~CMultiPassSerializer

private:

	ScriptParams             m_inputs;
	SScriptUserDocumentation m_userDocumentation;
};
} // Schematyc
