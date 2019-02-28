// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "Script/Graph/Nodes/ScriptGraphSequenceNode.h"

#include <CrySchematyc/Compiler/IGraphNodeCompiler.h>
#include <CrySchematyc/Env/IEnvRegistry.h>
#include <CrySchematyc/Env/Elements/IEnvDataType.h>
#include <CrySchematyc/Reflection/TypeDesc.h>
#include <CrySchematyc/Script/Elements/IScriptEnum.h>
#include <CrySchematyc/Utils/Any.h>
#include <CrySchematyc/Utils/StackString.h>
#include <CrySchematyc/Utils/IGUIDRemapper.h>

#include "Script/ScriptVariableData.h"
#include "Script/ScriptView.h"
#include "Script/Graph/ScriptGraphNode.h"
#include "Script/Graph/ScriptGraphNodeFactory.h"
#include "SerializationUtils/SerializationContext.h"

namespace Schematyc
{
void CScriptGraphSequenceNode::SOutput::Serialize(Serialization::IArchive& archive)
{
	archive(name, "name", "^");

	if (archive.isValidation())
	{
		if (name.empty())
		{
			archive.error(*this, "Empty output!");
		}
		else if (bIsDuplicate)
		{
			archive.warning(*this, "Duplicate output!");
		}
	}
}

CryGUID CScriptGraphSequenceNode::GetTypeGUID() const
{
	return ms_typeGUID;
}

void CScriptGraphSequenceNode::CreateLayout(CScriptGraphNodeLayout& layout)
{
	layout.SetName("Sequence");
	layout.SetStyleId("Core::FlowControl");

	layout.AddInput("In", CryGUID(), { EScriptGraphPortFlags::Flow, EScriptGraphPortFlags::MultiLink });

	for (const SOutput& output : m_outputs)
	{
		if (!output.bIsDuplicate)
		{
			layout.AddOutput(output.name.c_str(), CryGUID(), EScriptGraphPortFlags::Flow);
		}
	}
}

void CScriptGraphSequenceNode::Compile(SCompilerContext& context, IGraphNodeCompiler& compiler) const
{
	compiler.BindCallback(&Execute);
}

void CScriptGraphSequenceNode::PostLoad(Serialization::IArchive& archive, const ISerializationContext& context)
{
	archive(m_outputs, "outputs");
	RefreshOutputs();
}

void CScriptGraphSequenceNode::Save(Serialization::IArchive& archive, const ISerializationContext& context)
{
	archive(m_outputs, "outputs");
}

void CScriptGraphSequenceNode::Edit(Serialization::IArchive& archive, const ISerializationContext& context)
{
	archive(m_outputs, "outputs", "Outputs");
	RefreshOutputs();
}

void CScriptGraphSequenceNode::Validate(Serialization::IArchive& archive, const ISerializationContext& context)
{
	archive(m_outputs, "outputs");
}

void CScriptGraphSequenceNode::Register(CScriptGraphNodeFactory& factory)
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
				return "Sequence";
			}

			virtual const char* GetSubject() const override
			{
				return nullptr;
			}

			virtual const char* GetDescription() const override
			{
				return "Split flow into multiple sequences";
			}

			virtual const char* GetStyleId() const override
			{
				return "Core::FlowControl";
			}

			virtual IScriptGraphNodePtr Execute(const Vec2& pos) override
			{
				return std::make_shared<CScriptGraphNode>(gEnv->pSchematyc->CreateGUID(), stl::make_unique<CScriptGraphSequenceNode>(), pos);
			}

			// ~IScriptGraphNodeCreationCommand
		};

	public:

		// IScriptGraphNodeCreator

		virtual CryGUID GetTypeGUID() const override
		{
			return CScriptGraphSequenceNode::ms_typeGUID;
		}

		virtual IScriptGraphNodePtr CreateNode(const CryGUID& guid) override
		{
			return std::make_shared<CScriptGraphNode>(guid, stl::make_unique<CScriptGraphSequenceNode>());
		}

		virtual void PopulateNodeCreationMenu(IScriptGraphNodeCreationMenu& nodeCreationMenu, const IScriptView& scriptView, const IScriptGraph& graph) override
		{
			nodeCreationMenu.AddCommand(std::make_shared<CCreationCommand>());
		}

		// ~IScriptGraphNodeCreator
	};

	factory.RegisterCreator(std::make_shared<CCreator>());
}

void CScriptGraphSequenceNode::RefreshOutputs()
{
	std::vector<string> outputNames;
	outputNames.reserve(m_outputs.size());

	for (SOutput& output : m_outputs)
	{
		output.bIsDuplicate = std::find(outputNames.begin(), outputNames.end(), output.name) != outputNames.end();
		if (!output.bIsDuplicate)
		{
			outputNames.push_back(output.name);
		}
	}
}

SRuntimeResult CScriptGraphSequenceNode::Execute(SRuntimeContext& context, const SRuntimeActivationParams& activationParams)
{
	const uint32 outputIdx = activationParams.mode == EActivationMode::Output ? activationParams.portIdx + 1 : 0;
	if (outputIdx < (context.node.GetOutputCount() - 1))
	{
		return SRuntimeResult(ERuntimeStatus::ContinueAndRepeat, outputIdx);
	}
	else
	{
		return SRuntimeResult(ERuntimeStatus::Continue, outputIdx);
	}
}

const CryGUID CScriptGraphSequenceNode::ms_typeGUID = "87aa640f-b8c6-449f-bc37-2d5400009290"_cry_guid;
} // Schematyc

SCHEMATYC_REGISTER_SCRIPT_GRAPH_NODE(Schematyc::CScriptGraphSequenceNode::Register)
