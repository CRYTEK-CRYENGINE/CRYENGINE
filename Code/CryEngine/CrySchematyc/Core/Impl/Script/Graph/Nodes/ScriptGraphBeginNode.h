// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include <CrySchematyc/Runtime/RuntimeGraph.h>
#include <CrySchematyc/Utils/GUID.h>

#include "Script/Graph/ScriptGraphNodeModel.h"

namespace Schematyc
{
class CScriptGraphBeginNode : public CScriptGraphNodeModel
{
private:

	struct EOutputIdx
	{
		enum : uint32
		{
			Out = 0,
			FirstParam
		};
	};

public:

	CScriptGraphBeginNode();

	// CScriptGraphNodeModel
	virtual void  Init() override;
	virtual CryGUID GetTypeGUID() const override;
	virtual void  CreateLayout(CScriptGraphNodeLayout& layout) override;
	virtual void  Compile(SCompilerContext& context, IGraphNodeCompiler& compiler) const override;
	// ~CScriptGraphNodeModel

	static void Register(CScriptGraphNodeFactory& factory);

private:

	static SRuntimeResult Execute(SRuntimeContext& context, const SRuntimeActivationParams& activationParams);
	static SRuntimeResult ExecuteFunction(SRuntimeContext& context, const SRuntimeActivationParams& activationParams);

public:

	static const CryGUID ms_typeGUID;
};
} // Schematyc
