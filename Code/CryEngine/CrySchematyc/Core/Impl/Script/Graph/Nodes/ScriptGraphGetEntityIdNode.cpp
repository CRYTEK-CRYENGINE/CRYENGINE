// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "Script/Graph/Nodes/ScriptGraphGetEntityIdNode.h"

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
CScriptGraphGetEntityIdNode::CScriptGraphGetEntityIdNode() {}

CryGUID CScriptGraphGetEntityIdNode::GetTypeGUID() const
{
	return ms_typeGUID;
}

void CScriptGraphGetEntityIdNode::CreateLayout(CScriptGraphNodeLayout& layout)
{
	layout.SetName("GetEntityId");
	layout.SetStyleId("Core::Data");
	
	layout.AddOutputWithData("Entity", GetTypeDesc<ExplicitEntityId>().GetGUID(), { EScriptGraphPortFlags::Data, EScriptGraphPortFlags::MultiLink, EScriptGraphPortFlags::Pull }, ExplicitEntityId(INVALID_ENTITYID));
}

void CScriptGraphGetEntityIdNode::Compile(SCompilerContext& context, IGraphNodeCompiler& compiler) const
{
	compiler.BindCallback(&Execute);
}

void CScriptGraphGetEntityIdNode::Register(CScriptGraphNodeFactory& factory)
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
				return "GetEntity";
			}

			virtual const char* GetSubject() const override
			{
				return nullptr;
			}

			virtual const char* GetDescription() const override
			{
				return "Gets the Entity we are attached to";
			}

			virtual const char* GetStyleId() const override
			{
				return "Core::Data";
			}

			virtual IScriptGraphNodePtr Execute(const Vec2& pos) override
			{
				return std::make_shared<CScriptGraphNode>(gEnv->pSchematyc->CreateGUID(), stl::make_unique<CScriptGraphGetEntityIdNode>(), pos);
			}

			// ~IScriptGraphNodeCreationCommand
		};

	public:

		// IScriptGraphNodeCreator

		virtual CryGUID GetTypeGUID() const override
		{
			return CScriptGraphGetEntityIdNode::ms_typeGUID;
		}

		virtual IScriptGraphNodePtr CreateNode(const CryGUID& guid) override
		{
			return std::make_shared<CScriptGraphNode>(guid, stl::make_unique<CScriptGraphGetEntityIdNode>());
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

SRuntimeResult CScriptGraphGetEntityIdNode::Execute(SRuntimeContext& context, const SRuntimeActivationParams& activationParams)
{
	IEntity *pEntity = static_cast<IObject*>(context.pObject)->GetEntity();
	if (pEntity)
	{
		DynamicCast<ExplicitEntityId>(*context.node.GetOutputData(EOutputIdx::Value)) = static_cast<ExplicitEntityId>(pEntity->GetId());
	}
	else
	{
		DynamicCast<ExplicitEntityId>(*context.node.GetOutputData(EOutputIdx::Value)) = static_cast<ExplicitEntityId>(INVALID_ENTITYID);
	}

	return SRuntimeResult(ERuntimeStatus::Continue);
}

const CryGUID CScriptGraphGetEntityIdNode::ms_typeGUID = "29C4EC3A-8E27-4D95-9C6D-8B3A19865FC2"_cry_guid;
} // Schematyc

SCHEMATYC_REGISTER_SCRIPT_GRAPH_NODE(Schematyc::CScriptGraphGetEntityIdNode::Register)
