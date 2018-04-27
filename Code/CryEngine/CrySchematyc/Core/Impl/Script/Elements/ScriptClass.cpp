// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "Script/Elements/ScriptClass.h"

#include <CrySerialization/IArchiveHost.h>
#include <CrySchematyc/Script/IScriptGraph.h>
#include <CrySchematyc/Script/IScriptRegistry.h>
#include <CrySchematyc/Script/Elements/IScriptSignalReceiver.h>
#include <CrySchematyc/SerializationUtils/ISerializationContext.h>
#include <CrySchematyc/Utils/IGUIDRemapper.h>

#include "CVars.h"
#include "CoreEnv/CoreEnvSignals.h"
#include "Script/Graph/ScriptGraphNode.h"
#include "Script/Graph/Nodes/ScriptGraphReceiveSignalNode.h"

namespace Schematyc
{
CScriptClass::CScriptClass()
	: CScriptElementBase(EScriptElementFlags::MustOwnScript)
{}

CScriptClass::CScriptClass(const CryGUID& guid, const char* szName)
	: CScriptElementBase(guid, szName, EScriptElementFlags::MustOwnScript)
{}

void CScriptClass::EnumerateDependencies(const ScriptDependencyEnumerator& enumerator, EScriptDependencyType type) const {}

void CScriptClass::RemapDependencies(IGUIDRemapper& guidRemapper)                                                        {}

void CScriptClass::ProcessEvent(const SScriptEvent& event)
{
	CScriptElementBase::ProcessEvent(event);

	switch (event.id)
	{
	case EScriptEventId::EditorAdd:
		{
			// TODO: This should happen in editor!
			IScriptRegistry& scriptRegistry = gEnv->pSchematyc->GetScriptRegistry();
			scriptRegistry.AddConstructor("ConstructionGraph", this);

			IScriptSignalReceiver* pSignalReceiver = scriptRegistry.AddSignalReceiver("SignalGraph", EScriptSignalReceiverType::Universal, CryGUID(), this);
			IScriptGraph* pGraph = static_cast<IScriptGraph*>(pSignalReceiver->GetExtensions().QueryExtension(EScriptExtensionType::Graph));
			if (pGraph)
			{
				IScriptGraphNodePtr pStartNode = std::make_shared<CScriptGraphNode>(gEnv->pSchematyc->CreateGUID(), stl::make_unique<CScriptGraphReceiveSignalNode>(SElementId(EDomain::Env, GetTypeDesc<SStartSignal>().GetGUID()))); // #SchematycTODO : Shouldn't we be using CScriptGraphNodeFactory::CreateNode() instead of instantiating the node directly?!?
				pStartNode->SetPos(Vec2(0.0f, 0.0f));
				pGraph->AddNode(pStartNode);

				IScriptGraphNodePtr pStopNode = std::make_shared<CScriptGraphNode>(gEnv->pSchematyc->CreateGUID(), stl::make_unique<CScriptGraphReceiveSignalNode>(SElementId(EDomain::Env, GetTypeDesc<SStopSignal>().GetGUID()))); // #SchematycTODO : Shouldn't we be using CScriptGraphNodeFactory::CreateNode() instead of instantiating the node directly?!?
				pStopNode->SetPos(Vec2(0.0f, 100.0f));
				pGraph->AddNode(pStopNode);

				IScriptGraphNodePtr pUpdateNode = std::make_shared<CScriptGraphNode>(gEnv->pSchematyc->CreateGUID(), stl::make_unique<CScriptGraphReceiveSignalNode>(SElementId(EDomain::Env, GetTypeDesc<SUpdateSignal>().GetGUID()))); // #SchematycTODO : Shouldn't we be using CScriptGraphNodeFactory::CreateNode() instead of instantiating the node directly?!?
				pUpdateNode->SetPos(Vec2(0.0f, 200.0f));
				pGraph->AddNode(pUpdateNode);
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

void CScriptClass::Serialize(Serialization::IArchive& archive)
{
	// #SchematycTODO : Shouldn't this be handled by CScriptElementBase itself?
	CScriptElementBase::Serialize(archive);
	CMultiPassSerializer::Serialize(archive);
	CScriptElementBase::SerializeExtensions(archive);
}

const char* CScriptClass::GetAuthor() const
{
	return m_userDocumentation.author.c_str();
}

const char* CScriptClass::GetDescription() const
{
	return m_userDocumentation.description.c_str();
}

void CScriptClass::Load(Serialization::IArchive& archive, const ISerializationContext& context)
{
	archive(m_userDocumentation, "userDocumentation", "Documentation");
}

void CScriptClass::Save(Serialization::IArchive& archive, const ISerializationContext& context)
{
	archive(m_userDocumentation, "userDocumentation", "Documentation");
}

void CScriptClass::Edit(Serialization::IArchive& archive, const ISerializationContext& context)
{
	archive(m_userDocumentation, "userDocumentation", "Documentation");
}
} // Schematyc
