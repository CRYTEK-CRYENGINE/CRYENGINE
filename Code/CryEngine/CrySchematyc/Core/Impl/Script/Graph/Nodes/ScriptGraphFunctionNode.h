// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include <CrySchematyc/FundamentalTypes.h>
#include <CrySchematyc/Reflection/TypeDesc.h>
#include <CrySchematyc/Runtime/RuntimeGraph.h>
#include <CrySchematyc/Utils/GUID.h>

#include "Script/Graph/ScriptGraphNodeModel.h"

namespace Schematyc
{

// Forward declare interfaces.
struct IEnvFunction;
struct IScriptFunction;

class CScriptGraphFunctionNode : public CScriptGraphNodeModel
{
public:

	struct EInputIdx
	{
		enum : uint32
		{
			In = 0,
			FirstParam
		};
	};

	struct EOutputIdx
	{
		enum : uint32
		{
			Out = 0,
			FirstParam
		};
	};

	struct SEnvGlobalFunctionRuntimeData
	{
		SEnvGlobalFunctionRuntimeData(const IEnvFunction* _pEnvFunction);
		SEnvGlobalFunctionRuntimeData(const SEnvGlobalFunctionRuntimeData& rhs);

		static void ReflectType(CTypeDesc<SEnvGlobalFunctionRuntimeData>& desc);

		const IEnvFunction* pEnvFunction;
	};

	struct SEnvComponentFunctionRuntimeData
	{
		SEnvComponentFunctionRuntimeData(const IEnvFunction* _pEnvFunction, uint32 _componentIdx);
		SEnvComponentFunctionRuntimeData(const SEnvComponentFunctionRuntimeData& rhs);

		static void ReflectType(CTypeDesc<SEnvComponentFunctionRuntimeData>& desc);

		const IEnvFunction* pEnvFunction;
		uint32              componentIdx;
	};

	struct SScriptFunctionRuntimeData
	{
		SScriptFunctionRuntimeData(uint32 _functionIdx);
		SScriptFunctionRuntimeData(const SScriptFunctionRuntimeData& rhs);

		static void ReflectType(CTypeDesc<SScriptFunctionRuntimeData>& desc);

		uint32      functionIdx;
	};

public:

	CScriptGraphFunctionNode();
	CScriptGraphFunctionNode(const SElementId& functionId, const CryGUID& objectGUID);

	// CScriptGraphNodeModel
	virtual CryGUID GetTypeGUID() const override;
	virtual void    CreateLayout(CScriptGraphNodeLayout& layout) override;
	virtual void    Compile(SCompilerContext& context, IGraphNodeCompiler& compiler) const override;
	virtual void    LoadDependencies(Serialization::IArchive& archive, const ISerializationContext& context) override;
	virtual void    Load(Serialization::IArchive& archive, const ISerializationContext& context) override;
	virtual void    Save(Serialization::IArchive& archive, const ISerializationContext& context) override;
	virtual void    Edit(Serialization::IArchive& archive, const ISerializationContext& context) override;
	virtual void    Validate(Serialization::IArchive& archive, const ISerializationContext& context) override;
	virtual void    RemapDependencies(IGUIDRemapper& guidRemapper) override;
	// ~CScriptGraphNodeModel

	static void Register(CScriptGraphNodeFactory& factory);

private:

	void                  CreateInputsAndOutputs(CScriptGraphNodeLayout& layout, const IEnvFunction& envFunction);
	void                  CreateInputsAndOutputs(CScriptGraphNodeLayout& layout, const IScriptFunction& scriptFunction);

	void                  GoToFunction();

	static SRuntimeResult ExecuteEnvGlobalFunction(SRuntimeContext& context, const SRuntimeActivationParams& activationParams);
	static SRuntimeResult ExecuteEnvComponentFunction(SRuntimeContext& context, const SRuntimeActivationParams& activationParams);
	static SRuntimeResult ExecuteScriptFunction(SRuntimeContext& context, const SRuntimeActivationParams& activationParams);

public:

	static const CryGUID ms_typeGUID;

private:

	SElementId m_functionId;
	CryGUID    m_objectGUID;
};

} // Schematyc
