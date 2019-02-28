// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include <CrySchematyc/Script/Elements/IScriptStruct.h>
#include <CrySchematyc/SerializationUtils/MultiPassSerializer.h>

#include "Script/ScriptElementBase.h"
#include "Script/ScriptParam.h"
#include "Script/ScriptUserDocumentation.h"

namespace Schematyc
{
// Forward declare classes.
class CAnyConstPtr;

class CScriptStruct : public CScriptElementBase<IScriptStruct>, public CMultiPassSerializer
{
public:

	CScriptStruct();
	CScriptStruct(const CryGUID& guid, const char* szName);

	// IScriptElement
	virtual void EnumerateDependencies(const ScriptDependencyEnumerator& enumerator, EScriptDependencyType type) const override;
	virtual void RemapDependencies(IGUIDRemapper& guidRemapper) override;
	virtual void ProcessEvent(const SScriptEvent& event) override;
	virtual void Serialize(Serialization::IArchive& archive) override;
	// ~IScriptElement

	// IScriptStruct
	virtual uint32       GetFieldCount() const override;
	virtual const char*  GetFieldName(uint32 fieldIdx) const override;
	virtual CAnyConstPtr GetFieldValue(uint32 fieldIdx) const override;
	// ~IScriptStruct

protected:

	// CMultiPassSerializer
	virtual void LoadDependencies(Serialization::IArchive& archive, const ISerializationContext& context) override;
	virtual void Load(Serialization::IArchive& archive, const ISerializationContext& context) override;
	virtual void Save(Serialization::IArchive& archive, const ISerializationContext& context) override;
	virtual void Edit(Serialization::IArchive& archive, const ISerializationContext& context) override;
	// ~CMultiPassSerializer

private:

	ScriptParams             m_fields;
	SScriptUserDocumentation m_userDocumentation;
};
} // Schematyc
