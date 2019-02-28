// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include <CrySchematyc/FundamentalTypes.h>
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

class CScriptGraphArrayAddNode : public CScriptGraphNodeModel
{
private:

	struct EInputIdx
	{
		enum : uint32
		{
			In = 0,
			Array,
			Value
		};
	};

	struct EOutputIdx
	{
		enum : uint32
		{
			Out = 0
		};
	};

public:

	CScriptGraphArrayAddNode(const SElementId& typeId = SElementId());

	// CScriptGraphNodeModel
	virtual CryGUID GetTypeGUID() const override;
	virtual void  CreateLayout(CScriptGraphNodeLayout& layout) override;
	virtual void  Compile(SCompilerContext& context, IGraphNodeCompiler& compiler) const override;
	virtual void  LoadDependencies(Serialization::IArchive& archive, const ISerializationContext& context) override;
	virtual void  Save(Serialization::IArchive& archive, const ISerializationContext& context) override;
	virtual void  Edit(Serialization::IArchive& archive, const ISerializationContext& context) override;
	virtual void  RemapDependencies(IGUIDRemapper& guidRemapper) override;
	// ~CScriptGraphNodeModel

	static void Register(CScriptGraphNodeFactory& factory);

private:

	static SRuntimeResult Execute(SRuntimeContext& context, const SRuntimeActivationParams& activationParams);

public:

	static const CryGUID ms_typeGUID;

private:

	CScriptVariableData m_defaultValue;
};

} // Schematyc
