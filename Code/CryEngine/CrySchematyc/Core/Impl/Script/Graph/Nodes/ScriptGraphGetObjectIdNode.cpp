// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "Script/Graph/Nodes/ScriptGraphGetObjectIdNode.h"

#include <CrySchematyc/IObject.h>
#include <CrySchematyc/Compiler/IGraphNodeCompiler.h>

#include <CrySchematyc/Script/Elements/IScriptVariable.h>

#include "Runtime/RuntimeClass.h"
#include "Script/ScriptView.h"
#include "Script/Graph/ScriptGraphNode.h"
#include "Script/Graph/ScriptGraphNodeFactory.h"
#include "SerializationUtils/SerializationContext.h"

namespace Schematyc
{
CScriptGraphGetObjectIdNode::CScriptGraphGetObjectIdNode() {}

CryGUID CScriptGraphGetObjectIdNode::GetTypeGUID() const
{
	return ms_typeGUID;
}

void CScriptGraphGetObjectIdNode::CreateLayout(CScriptGraphNodeLayout& layout)
{
	layout.SetName("GetObjectId");
	layout.SetStyleId("Core::Data");
	
	layout.AddOutputWithData("ObjectId", GetTypeDesc<ObjectId>().GetGUID(), { EScriptGraphPortFlags::Data, EScriptGraphPortFlags::MultiLink, EScriptGraphPortFlags::Pull }, ObjectId());
}

void CScriptGraphGetObjectIdNode::Compile(SCompilerContext& context, IGraphNodeCompiler& compiler) const
{
	compiler.BindCallback(&Execute);
}

void CScriptGraphGetObjectIdNode::Register(CScriptGraphNodeFactory& factory)
{
	class CCreator : public IScriptGraphNodeCreator
	{
	private:

		class CCreationCommand : public IScriptGraphNodeCreationCommand
		{
		public:

			// IScriptGraphNodeCreationCommand

			virtual const char* GetBehavior() const override
			{
				return "GetObjectId";
			}

			virtual const char* GetSubject() const override
			{
				return nullptr;
			}

			virtual const char* GetDescription() const override
			{
				return "Get id of this object";
			}

			virtual const char* GetStyleId() const override
			{
				return "Core::Data";
			}

			virtual IScriptGraphNodePtr Execute(const Vec2& pos) override
			{
				return std::make_shared<CScriptGraphNode>(gEnv->pSchematyc->CreateGUID(), stl::make_unique<CScriptGraphGetObjectIdNode>(), pos);
			}

			// ~IScriptGraphNodeCreationCommand
		};

	public:

		// IScriptGraphNodeCreator

		virtual CryGUID GetTypeGUID() const override
		{
			return CScriptGraphGetObjectIdNode::ms_typeGUID;
		}

		virtual IScriptGraphNodePtr CreateNode(const CryGUID& guid) override
		{
			return std::make_shared<CScriptGraphNode>(guid, stl::make_unique<CScriptGraphGetObjectIdNode>());
		}

		virtual void PopulateNodeCreationMenu(IScriptGraphNodeCreationMenu& nodeCreationMenu, const IScriptView& scriptView, const IScriptGraph& graph) override
		{
			if (scriptView.GetScriptClass())
			{
				nodeCreationMenu.AddCommand(std::make_shared<CCreationCommand>());
			}
		}

		// ~IScriptGraphNodeCreator
	};

	factory.RegisterCreator(std::make_shared<CCreator>());
}

SRuntimeResult CScriptGraphGetObjectIdNode::Execute(SRuntimeContext& context, const SRuntimeActivationParams& activationParams)
{
	DynamicCast<ObjectId>(*context.node.GetOutputData(EOutputIdx::Value)) = static_cast<IObject*>(context.pObject)->GetId();

	return SRuntimeResult(ERuntimeStatus::Continue);
}

const CryGUID CScriptGraphGetObjectIdNode::ms_typeGUID = "6b0c3534-1117-4a57-bb15-e43c51aff2e0"_cry_guid;
} // Schematyc

SCHEMATYC_REGISTER_SCRIPT_GRAPH_NODE(Schematyc::CScriptGraphGetObjectIdNode::Register)
