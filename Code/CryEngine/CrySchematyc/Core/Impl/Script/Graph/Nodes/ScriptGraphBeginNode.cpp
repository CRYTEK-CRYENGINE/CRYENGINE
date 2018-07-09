// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "Script/Graph/Nodes/ScriptGraphBeginNode.h"

#include <CrySchematyc/Compiler/CompilerContext.h>
#include <CrySchematyc/Compiler/IGraphNodeCompiler.h>
#include <CrySchematyc/Env/IEnvRegistry.h>
#include <CrySchematyc/Script/IScriptRegistry.h>
#include <CrySchematyc/Script/Elements/IScriptFunction.h>
#include <CrySchematyc/Utils/Any.h>

#include "Runtime/RuntimeClass.h"
#include "Script/Graph/ScriptGraphNode.h"
#include "Script/Graph/ScriptGraphNodeFactory.h"

namespace Schematyc
{

CScriptGraphBeginNode::CScriptGraphBeginNode() {}

void CScriptGraphBeginNode::Init()
{
	CScriptGraphNodeModel::GetNode().SetFlags({ EScriptGraphNodeFlags::NotCopyable, EScriptGraphNodeFlags::NotRemovable });
}

CryGUID CScriptGraphBeginNode::GetTypeGUID() const
{
	return ms_typeGUID;
}

void CScriptGraphBeginNode::CreateLayout(CScriptGraphNodeLayout& layout)
{
	layout.SetName("Begin");
	layout.SetStyleId("Core::FlowControl::Begin");
	layout.AddOutput("Out", CryGUID(), { EScriptGraphPortFlags::Flow, EScriptGraphPortFlags::Begin });

	const IScriptElement& scriptElement = CScriptGraphNodeModel::GetNode().GetGraph().GetElement();
	switch (scriptElement.GetType())
	{
	case EScriptElementType::Function:
		{
			const IScriptFunction& scriptFunction = DynamicCast<IScriptFunction>(scriptElement);
			for (uint32 functionInputIdx = 0, functionInputCount = scriptFunction.GetInputCount(); functionInputIdx < functionInputCount; ++functionInputIdx)
			{
				CAnyConstPtr pData = scriptFunction.GetInputData(functionInputIdx);
				if (pData)
				{
					layout.AddOutputWithData(CUniqueId::FromGUID(scriptFunction.GetInputGUID(functionInputIdx)), scriptFunction.GetInputName(functionInputIdx), scriptFunction.GetInputTypeId(functionInputIdx).guid, { EScriptGraphPortFlags::Data, EScriptGraphPortFlags::MultiLink }, *pData);
				}
			}
			break;
		}
	}
}

void CScriptGraphBeginNode::Compile(SCompilerContext& context, IGraphNodeCompiler& compiler) const
{
	CRuntimeClass* pClass = context.interfaces.Query<CRuntimeClass>();
	if (pClass)
	{
		const IScriptElement& scriptElement = CScriptGraphNodeModel::GetNode().GetGraph().GetElement();
		switch (scriptElement.GetType())
		{
		case EScriptElementType::Constructor:
			{
				pClass->AddConstructor(compiler.GetGraphIdx(), SRuntimeActivationParams(compiler.GetGraphNodeIdx(), EOutputIdx::Out, EActivationMode::Output));
				compiler.BindCallback(&Execute);
				break;
			}
		case EScriptElementType::Function:
			{
				pClass->AddFunction(scriptElement.GetGUID(), compiler.GetGraphIdx(), SRuntimeActivationParams(compiler.GetGraphNodeIdx(), EOutputIdx::Out, EActivationMode::Output));
				compiler.BindCallback(&ExecuteFunction);
				break;
			}
		case EScriptElementType::StateMachine:
			{
				const uint32 stateMachineIdx = pClass->FindStateMachine(scriptElement.GetGUID());
				if (stateMachineIdx != InvalidIdx)
				{
					pClass->SetStateMachineBeginFunction(stateMachineIdx, compiler.GetGraphIdx(), SRuntimeActivationParams(compiler.GetGraphNodeIdx(), EOutputIdx::Out, EActivationMode::Output));
					compiler.BindCallback(&Execute);
				}
				else
				{
					SCHEMATYC_COMPILER_ERROR("Failed to retrieve class state machine!");
				}
				break;
			}
		}
	}
}

void CScriptGraphBeginNode::Register(CScriptGraphNodeFactory& factory)
{
	class CCreator : public IScriptGraphNodeCreator
	{
	public:

		// IScriptGraphNodeCreator

		virtual CryGUID GetTypeGUID() const override
		{
			return CScriptGraphBeginNode::ms_typeGUID;
		}

		virtual IScriptGraphNodePtr CreateNode(const CryGUID& guid) override
		{
			return std::make_shared<CScriptGraphNode>(guid, stl::make_unique<CScriptGraphBeginNode>());
		}

		virtual void PopulateNodeCreationMenu(IScriptGraphNodeCreationMenu& nodeCreationMenu, const IScriptView& scriptView, const IScriptGraph& graph) override {}

		// ~IScriptGraphNodeCreator
	};

	factory.RegisterCreator(std::make_shared<CCreator>());
}

SRuntimeResult CScriptGraphBeginNode::Execute(SRuntimeContext& context, const SRuntimeActivationParams& activationParams)
{
	return SRuntimeResult(ERuntimeStatus::Continue, EOutputIdx::Out);
}

SRuntimeResult CScriptGraphBeginNode::ExecuteFunction(SRuntimeContext& context, const SRuntimeActivationParams& activationParams)
{
	for (uint8 outputIdx = EOutputIdx::FirstParam, outputCount = context.node.GetOutputCount(); outputIdx < outputCount; ++outputIdx)
	{
		if (context.node.IsDataOutput(outputIdx))
		{
			CAnyConstPtr pSrcValue = context.params.GetInput(context.node.GetOutputId(outputIdx));
			if (pSrcValue)
			{
				Any::CopyAssign(*context.node.GetOutputData(outputIdx), *pSrcValue);
			}
		}
	}

	return SRuntimeResult(ERuntimeStatus::Continue, EOutputIdx::Out);
}

const CryGUID CScriptGraphBeginNode::ms_typeGUID = "12bdfa06-ba95-4e48-bb2d-bb48a7080abc"_cry_guid;

} // Schematyc

SCHEMATYC_REGISTER_SCRIPT_GRAPH_NODE(Schematyc::CScriptGraphBeginNode::Register)
