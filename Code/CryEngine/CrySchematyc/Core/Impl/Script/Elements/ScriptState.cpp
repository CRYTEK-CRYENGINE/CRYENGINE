// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "Script/Elements/ScriptState.h"

#include <CrySerialization/BlackBox.h>
#include <CrySerialization/IArchiveHost.h>
#include <CrySerialization/STL.h>
#include <CrySchematyc/Env/IEnvRegistry.h>
#include <CrySchematyc/Script/IScriptGraph.h>
#include <CrySchematyc/Script/IScriptRegistry.h>
#include <CrySchematyc/Script/Elements/IScriptSignalReceiver.h>
#include <CrySchematyc/SerializationUtils/ISerializationContext.h>
#include <CrySchematyc/SerializationUtils/SerializationUtils.h>
#include <CrySchematyc/Utils/IGUIDRemapper.h>

#include "CoreEnv/CoreEnvSignals.h"
#include "Script/ScriptView.h"
#include "Script/Graph/ScriptGraphNode.h"
#include "Script/Graph/Nodes/ScriptGraphReceiveSignalNode.h"

namespace Schematyc
{
namespace
{
typedef std::vector<CryGUID> GUIDVector;
}

CScriptState::CScriptState() {}

CScriptState::CScriptState(const CryGUID& guid, const char* szName)
	: CScriptElementBase(guid, szName)
{}

void CScriptState::EnumerateDependencies(const ScriptDependencyEnumerator& enumerator, EScriptDependencyType type) const {}

void CScriptState::RemapDependencies(IGUIDRemapper& guidRemapper)
{
	m_partnerGUID = guidRemapper.Remap(m_partnerGUID);
}

void CScriptState::ProcessEvent(const SScriptEvent& event)
{
	switch (event.id)
	{
	case EScriptEventId::EditorAdd:
		{
			// TODO: This should happen in editor!
			IScriptRegistry& scriptRegistry = gEnv->pSchematyc->GetScriptRegistry();
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
			// ~TODO

			break;
		}
	}
}

void CScriptState::Serialize(Serialization::IArchive& archive)
{
	// #SchematycTODO : Shouldn't this be handled by CScriptElementBase itself?
	CScriptElementBase::Serialize(archive);
	CMultiPassSerializer::Serialize(archive);
	CScriptElementBase::SerializeExtensions(archive);
}

CryGUID CScriptState::GetPartnerGUID() const
{
	return m_partnerGUID;
}

void CScriptState::LoadDependencies(Serialization::IArchive& archive, const ISerializationContext& context)
{
	archive(m_partnerGUID, "partnerGUID");
}

void CScriptState::Save(Serialization::IArchive& archive, const ISerializationContext& context)
{
	archive(m_partnerGUID, "partnerGUID");
}

void CScriptState::Edit(Serialization::IArchive& archive, const ISerializationContext& context)
{
	const IScriptStateMachine* pStateMachine = nullptr;
	for (const IScriptElement* pParent = CScriptElementBase::GetParent(); pParent; pParent = pParent->GetParent())
	{
		pStateMachine = DynamicCast<IScriptStateMachine>(pParent);
		if (pStateMachine)
		{
			break;
		}
	}

	if (pStateMachine)
	{
		const IScriptStateMachine* pPartnerStateMachine = nullptr;
		{
			CScriptView scriptView(pStateMachine->GetGUID());
			pPartnerStateMachine = scriptView.GetScriptStateMachine(pStateMachine->GetPartnerGUID());
		}
		if (pPartnerStateMachine)
		{
			GUIDVector partnerGUIDs;
			Serialization::StringList partnerNames;
			partnerGUIDs.reserve(25);
			partnerNames.reserve(25);
			partnerGUIDs.push_back(CryGUID());
			partnerNames.push_back("None");

			CScriptView scriptView(pPartnerStateMachine->GetGUID());

			auto visitScriptState = [&partnerGUIDs, &partnerNames](const IScriptState& scriptState)
			{
				partnerGUIDs.push_back(scriptState.GetGUID());
				partnerNames.push_back(scriptState.GetName());
			};
			scriptView.VisitAccesibleStates(visitScriptState);

			GUIDVector::const_iterator itPartnerGUID = std::find(partnerGUIDs.begin(), partnerGUIDs.end(), m_partnerGUID);
			const int partnerIdx = itPartnerGUID != partnerGUIDs.end() ? static_cast<int>(itPartnerGUID - partnerGUIDs.begin()) : 0;
			Serialization::StringListValue partnerName(partnerNames, partnerIdx);
			archive(partnerName, "partnerName", "Partner");
			if (archive.isInput())
			{
				m_partnerGUID = partnerGUIDs[partnerName.index()];
			}
		}
	}
}
} // Schematyc
