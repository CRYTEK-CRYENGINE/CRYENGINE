// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "Script/Elements/ScriptFunction.h"

#include <CrySchematyc/SerializationUtils/ISerializationContext.h>
#include <CrySchematyc/Utils/Any.h>

#include "Script/Graph/ScriptGraph.h"
#include "Script/Graph/ScriptGraphNode.h"
#include "Script/Graph/Nodes/ScriptGraphBeginNode.h"

namespace Schematyc
{
CScriptFunction::CScriptFunction()
	: CScriptElementBase(EScriptElementFlags::CanOwnScript)
{
	CreateGraph();
}

CScriptFunction::CScriptFunction(const CryGUID& guid, const char* szName)
	: CScriptElementBase(guid, szName, EScriptElementFlags::CanOwnScript)
{
	CreateGraph();
}

void CScriptFunction::EnumerateDependencies(const ScriptDependencyEnumerator& enumerator, EScriptDependencyType type) const
{
	ScriptParam::EnumerateDependencies(m_inputs, enumerator, type);
	ScriptParam::EnumerateDependencies(m_outputs, enumerator, type);
}

void CScriptFunction::RemapDependencies(IGUIDRemapper& guidRemapper)
{
	ScriptParam::RemapDependencies(m_inputs, guidRemapper);
	ScriptParam::RemapDependencies(m_outputs, guidRemapper);
}

void CScriptFunction::ProcessEvent(const SScriptEvent& event)
{
	CScriptElementBase::ProcessEvent(event);

	switch (event.id)
	{
	case EScriptEventId::EditorAdd:
		{
			// TODO: This should happen in editor!
			IScriptGraph* pGraph = static_cast<IScriptGraph*>(CScriptElementBase::GetExtensions().QueryExtension(EScriptExtensionType::Graph));
			SCHEMATYC_CORE_ASSERT(pGraph);
			if (pGraph)
			{
				// TODO : Shouldn't we be using CScriptGraphNodeFactory::CreateNode() instead of instantiating the node directly?!?
				pGraph->AddNode(std::make_shared<CScriptGraphNode>(gEnv->pSchematyc->CreateGUID(), stl::make_unique<CScriptGraphBeginNode>()));
				// ~TODO
			}

			m_userDocumentation.SetCurrentUserAsAuthor();
			// ~TODO

			break;
		}
	case EScriptEventId::EditorPaste:
		{
			m_userDocumentation.SetCurrentUserAsAuthor();
			break;
		}
	}
}

void CScriptFunction::Serialize(Serialization::IArchive& archive)
{
	// #SchematycTODO : Shouldn't this be handled by CScriptElementBase itself?
	CScriptElementBase::Serialize(archive);
	CMultiPassSerializer::Serialize(archive);
	CScriptElementBase::SerializeExtensions(archive);
}

const char* CScriptFunction::GetAuthor() const
{
	return m_userDocumentation.author.c_str();
}

const char* CScriptFunction::GetDescription() const
{
	return m_userDocumentation.description.c_str();
}

uint32 CScriptFunction::GetInputCount() const
{
	return m_inputs.size();
}

CryGUID CScriptFunction::GetInputGUID(uint32 inputIdx) const
{
	return inputIdx < m_inputs.size() ? m_inputs[inputIdx].guid : CryGUID();
}

const char* CScriptFunction::GetInputName(uint32 inputIdx) const
{
	return inputIdx < m_inputs.size() ? m_inputs[inputIdx].name.c_str() : "";
}

SElementId CScriptFunction::GetInputTypeId(uint32 inputIdx) const
{
	return inputIdx < m_inputs.size() ? m_inputs[inputIdx].data.GetTypeId() : SElementId();
}

CAnyConstPtr CScriptFunction::GetInputData(uint32 inputIdx) const
{
	return inputIdx < m_inputs.size() ? m_inputs[inputIdx].data.GetValue() : CAnyConstPtr();
}

uint32 CScriptFunction::GetOutputCount() const
{
	return m_outputs.size();
}

CryGUID CScriptFunction::GetOutputGUID(uint32 outputIdx) const
{
	return outputIdx < m_outputs.size() ? m_outputs[outputIdx].guid : CryGUID();
}

const char* CScriptFunction::GetOutputName(uint32 outputIdx) const
{
	return outputIdx < m_outputs.size() ? m_outputs[outputIdx].name.c_str() : "";
}

SElementId CScriptFunction::GetOutputTypeId(uint32 outputIdx) const
{
	return outputIdx < m_outputs.size() ? m_outputs[outputIdx].data.GetTypeId() : SElementId();
}

CAnyConstPtr CScriptFunction::GetOutputData(uint32 outputIdx) const
{
	return outputIdx < m_outputs.size() ? m_outputs[outputIdx].data.GetValue() : CAnyConstPtr();
}

void CScriptFunction::LoadDependencies(Serialization::IArchive& archive, const ISerializationContext& context)
{
	archive(m_inputs, "inputs");
	archive(m_outputs, "outputs");
}

void CScriptFunction::Load(Serialization::IArchive& archive, const ISerializationContext& context)
{
	archive(m_inputs, "inputs");
	archive(m_outputs, "outputs");
	archive(m_userDocumentation, "userDocumentation");
}

void CScriptFunction::Save(Serialization::IArchive& archive, const ISerializationContext& context)
{
	archive(m_inputs, "inputs");
	archive(m_outputs, "outputs");
	archive(m_userDocumentation, "userDocumentation");
}

void CScriptFunction::Edit(Serialization::IArchive& archive, const ISerializationContext& context)
{
	{
		ScriptVariableData::CScopedSerializationConfig serializationConfig(archive);
		const CryGUID guid = GetGUID();
		serializationConfig.DeclareEnvDataTypes(guid);
		serializationConfig.DeclareScriptEnums(guid);
		serializationConfig.DeclareScriptStructs(guid);

		archive(m_inputs, "inputs", "Inputs");
		archive(m_outputs, "outputs", "Outputs");
	}

	archive(m_userDocumentation, "userDocumentation", "Documentation");
}

void CScriptFunction::CreateGraph()
{
	CScriptElementBase::AddExtension(std::make_shared<CScriptGraph>(*this, EScriptGraphType::Function));
}
} // Schematyc
