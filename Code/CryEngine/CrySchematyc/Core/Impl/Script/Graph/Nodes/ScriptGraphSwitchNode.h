// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include <CrySchematyc/FundamentalTypes.h>
#include <CrySchematyc/Reflection/TypeDesc.h>
#include <CrySchematyc/Runtime/RuntimeGraph.h>
#include <CrySchematyc/Utils/GUID.h>

#include "Script/ScriptVariableData.h"
#include "Script/Graph/ScriptGraphNodeModel.h"

namespace Schematyc
{
// Forward declare classes.
class CAnyValue;
// Forward declare shared pointers.
DECLARE_SHARED_POINTERS(CAnyValue)

class CScriptGraphSwitchNode : public CScriptGraphNodeModel
{
private:

	struct EInputIdx
	{
		enum : uint32
		{
			In = 0,
			Value
		};
	};

	struct EOutputIdx
	{
		enum : uint32
		{
			Default = 0,
			FirstCase
		};
	};

	struct SCase
	{
		SCase();
		SCase(const CAnyValuePtr& _pValue);

		void        Serialize(Serialization::IArchive& archive);

		inline bool operator==(const SCase& rhs) const;

		CAnyValuePtr pValue;
		bool         bIsDuplicate = false;
	};

	typedef std::vector<SCase> Cases;

	DECLARE_SHARED_POINTERS(Cases);

	struct SRuntimeData
	{
		SRuntimeData(const CasesPtr& _pCases);
		SRuntimeData(const SRuntimeData& rhs);

		static void ReflectType(CTypeDesc<SRuntimeData>& desc);

		CasesPtr     pCases;
	};

public:

	CScriptGraphSwitchNode(const SElementId& typeId = SElementId());

	// CScriptGraphNodeModel
	virtual CryGUID GetTypeGUID() const override;
	virtual void  CreateLayout(CScriptGraphNodeLayout& layout) override;
	virtual void  Compile(SCompilerContext& context, IGraphNodeCompiler& compiler) const override;
	virtual void  LoadDependencies(Serialization::IArchive& archive, const ISerializationContext& context) override;
	virtual void  PostLoad(Serialization::IArchive& archive, const ISerializationContext& context) override;
	virtual void  Save(Serialization::IArchive& archive, const ISerializationContext& context) override;
	virtual void  Edit(Serialization::IArchive& archive, const ISerializationContext& context) override;
	virtual void  Validate(Serialization::IArchive& archive, const ISerializationContext& context) override;
	virtual void  RemapDependencies(IGUIDRemapper& guidRemapper) override;
	// ~CScriptGraphNodeModel

	static void Register(CScriptGraphNodeFactory& factory);

private:

	void                  SerializeCases(Serialization::IArchive& archive);
	void                  RefreshCases();

	static SRuntimeResult Execute(SRuntimeContext& context, const SRuntimeActivationParams& activationParams);

public:

	static const CryGUID ms_typeGUID;

private:

	CScriptVariableData m_defaultValue;
	Cases               m_cases;
	CasesPtr            m_pValidCases;
};
} // Schematyc
