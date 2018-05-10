// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include <CrySchematyc/FundamentalTypes.h>
#include <CrySchematyc/Script/Elements/IScriptBase.h>
#include <CrySchematyc/SerializationUtils/MultiPassSerializer.h>
#include <CrySchematyc/Utils/EnumFlags.h>

#include "Script/ScriptElementBase.h"
#include "Script/ScriptUserDocumentation.h"

namespace Schematyc
{
// Forward declare interfaces.
struct IEnvClass;
struct IScriptClass;
// Forward declare classes.
class CAnyConstPtr;
class CAnyValue;
// Forward declare shared pointers.
DECLARE_SHARED_POINTERS(CAnyValue)

class CScriptBase : public CScriptElementBase<IScriptBase>, public CMultiPassSerializer
{
private:

	enum class ERefreshFlags : uint32
	{
		None               = 0x0000,
		Name               = BIT(0),
		EnvClassProperties = BIT(1),
		Variables          = BIT(2),
		ComponentInstances = BIT(3),
		All                = 0xffff
	};

	typedef CEnumFlags<ERefreshFlags> RefreshFlags;

public:

	CScriptBase();
	CScriptBase(const CryGUID& guid, const SElementId& classId);

	// IScriptElement
	virtual void EnumerateDependencies(const ScriptDependencyEnumerator& enumerator, EScriptDependencyType type) const override;
	virtual void RemapDependencies(IGUIDRemapper& guidRemapper) override;
	virtual void ProcessEvent(const SScriptEvent& event) override;
	virtual void Serialize(Serialization::IArchive& archive) override;
	// ~IScriptElement

	// IScriptBase
	virtual SElementId   GetClassId() const override;
	virtual CAnyConstPtr GetEnvClassProperties() const override;
	// ~IScriptBase

protected:

	// CMultiPassSerializer
	virtual void LoadDependencies(Serialization::IArchive& archive, const ISerializationContext& context) override;
	virtual void Load(Serialization::IArchive& archive, const ISerializationContext& context) override;
	virtual void Save(Serialization::IArchive& archive, const ISerializationContext& context) override;
	virtual void Edit(Serialization::IArchive& archive, const ISerializationContext& context) override;
	virtual void Validate(Serialization::IArchive& archive, const ISerializationContext& context) override;
	// ~CMultiPassSerializer

private:

	void Refresh(const RefreshFlags& flags);
	void RefreshVariables(const IScriptClass& scriptClass);
	void RefreshComponentInstances(const IEnvClass& envClass);
	void GoToClass();

private:

	SElementId   m_classId;
	CAnyValuePtr m_pEnvClassProperties;
};

} // Schematyc
