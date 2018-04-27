// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "Script/Elements/ScriptStateMachine.h"

#include <CrySerialization/IArchiveHost.h>
#include <CrySchematyc2/GUIDRemapper.h>
#include <CrySchematyc2/Deprecated/DocUtils.h>
#include <CrySchematyc2/Script/IScriptGraph.h>
#include <CrySchematyc2/Serialization/ISerializationContext.h>

#include "DomainContext.h"

SERIALIZATION_ENUM_BEGIN_NESTED(Schematyc2, EScriptStateMachineLifetime, "Schematyc Script State Machine Lifetime")
	SERIALIZATION_ENUM(Schematyc2::EScriptStateMachineLifetime::Persistent, "persistent", "Persistent")
	SERIALIZATION_ENUM(Schematyc2::EScriptStateMachineLifetime::Task, "task", "Task")
SERIALIZATION_ENUM_END()

namespace Schematyc2
{
	typedef std::vector<SGUID> GUIDVector;

	//////////////////////////////////////////////////////////////////////////
	CScriptStateMachine::CScriptStateMachine(IScriptFile& file)
		: CScriptElementBase(EScriptElementType::StateMachine, file)
		, m_lifetime(EScriptStateMachineLifetime::Persistent)
	{}

	//////////////////////////////////////////////////////////////////////////
	CScriptStateMachine::CScriptStateMachine(IScriptFile& file, const SGUID& guid, const SGUID& scopeGUID, const char* szName, EScriptStateMachineLifetime lifetime, const SGUID& contextGUID, const SGUID& partnerGUID)
		: CScriptElementBase(EScriptElementType::StateMachine, file)
		, m_guid(guid)
		, m_scopeGUID(scopeGUID)
		, m_name(szName)
		, m_lifetime(lifetime)
		, m_contextGUID(contextGUID)
		, m_partnerGUID(partnerGUID)
	{}

	//////////////////////////////////////////////////////////////////////////
	EAccessor CScriptStateMachine::GetAccessor() const
	{
		return EAccessor::Private;
	}

	//////////////////////////////////////////////////////////////////////////
	SGUID CScriptStateMachine::GetGUID() const
	{
		return m_guid;
	}

	//////////////////////////////////////////////////////////////////////////
	SGUID CScriptStateMachine::GetScopeGUID() const
	{
		return m_scopeGUID;
	}

	//////////////////////////////////////////////////////////////////////////
	bool CScriptStateMachine::SetName(const char* szName)
	{
		m_name = szName;
		return true;
	}

	//////////////////////////////////////////////////////////////////////////
	const char* CScriptStateMachine::GetName() const
	{
		return m_name.c_str();
	}

	//////////////////////////////////////////////////////////////////////////
	void CScriptStateMachine::EnumerateDependencies(const ScriptDependancyEnumerator& enumerator) const {}

	//////////////////////////////////////////////////////////////////////////
	void CScriptStateMachine::Refresh(const SScriptRefreshParams& params)
	{
		switch(params.reason)
		{
		case EScriptRefreshReason::EditorAdd:
		case EScriptRefreshReason::EditorFixUp:
		case EScriptRefreshReason::EditorPaste:
			{
				RefreshTransitionGraph();
				break;
			}
		}
	}

	//////////////////////////////////////////////////////////////////////////
	void CScriptStateMachine::Serialize(Serialization::IArchive& archive)
	{
		LOADING_TIME_PROFILE_SECTION;
		
		CScriptElementBase::Serialize(archive);

		SerializationContext::SetValidatorLink(archive, SValidatorLink(m_guid)); // #SchematycTODO : Can we set this from CScriptElementBase?
		switch(SerializationContext::GetPass(archive))
		{
		case ESerializationPass::PreLoad:
		case ESerializationPass::Save:
			{
				Serialization::SContext fileContext(archive, static_cast<IScriptFile*>(&CScriptElementBase::GetFile())); // #SchematycTODO : Do we still need this?

				archive(m_guid, "guid");
				archive(m_scopeGUID, "scope_guid");
				archive(m_name, "name");
				archive(m_lifetime, "lifetime", "!Lifetime");
				archive(m_contextGUID, "contextGUID");
				archive(m_transitionGraphGUID, "transition_graph_guid");

				SerializePartner(archive);
				break;
			}
		case ESerializationPass::Edit:
			{
				SerializePartner(archive);
				break;
			}
		}
	}

	//////////////////////////////////////////////////////////////////////////
	void CScriptStateMachine::RemapGUIDs(IGUIDRemapper& guidRemapper)
	{
		m_guid                = guidRemapper.Remap(m_guid);
		m_scopeGUID           = guidRemapper.Remap(m_scopeGUID);
		m_partnerGUID         = guidRemapper.Remap(m_partnerGUID);
		m_contextGUID         = guidRemapper.Remap(m_contextGUID);
		m_transitionGraphGUID = guidRemapper.Remap(m_transitionGraphGUID);
	}

	//////////////////////////////////////////////////////////////////////////
	EScriptStateMachineLifetime CScriptStateMachine::GetLifetime() const
	{
		return m_lifetime;
	}

	//////////////////////////////////////////////////////////////////////////
	SGUID CScriptStateMachine::GetContextGUID() const
	{
		return m_contextGUID;
	}

	//////////////////////////////////////////////////////////////////////////
	SGUID CScriptStateMachine::GetPartnerGUID() const
	{
		return m_partnerGUID;
	}

	//////////////////////////////////////////////////////////////////////////
	void CScriptStateMachine::RefreshTransitionGraph()
	{
		if(gEnv->IsEditor())
		{
			IScriptFile& file = CScriptElementBase::GetFile();
			if(!m_transitionGraphGUID || !file.GetGraph(m_transitionGraphGUID))
			{
				IDocGraph* pTransitionGraph = file.AddGraph(SScriptGraphParams(m_guid, "Transitions", EScriptGraphType::Transition, SGUID()));
				SCHEMATYC2_SYSTEM_ASSERT(pTransitionGraph);
				if(pTransitionGraph)
				{
					IScriptGraphNode* pBeginNode = pTransitionGraph->AddNode(EScriptGraphNodeType::BeginState, SGUID(), SGUID(), Vec2(0.0f, 0.0f));
					if(m_lifetime == EScriptStateMachineLifetime::Task)
					{
						IScriptGraphNode* pEndNode = pTransitionGraph->AddNode(EScriptGraphNodeType::EndState, SGUID(), SGUID(), Vec2(200.0f, 0.0f));
						pTransitionGraph->AddLink(pBeginNode->GetGUID(), pBeginNode->GetOutputName(0), pEndNode->GetGUID(), pEndNode->GetInputName(0));
					}
					m_transitionGraphGUID = pTransitionGraph->GetGUID();
				}
			}
		}
	}

	//////////////////////////////////////////////////////////////////////////
	void CScriptStateMachine::SerializePartner(Serialization::IArchive& archive)
	{
		if(archive.isEdit())
		{
			GUIDVector                partnerGUIDs;
			Serialization::StringList partnerNames;
			partnerGUIDs.reserve(16);
			partnerNames.reserve(16);
			partnerGUIDs.push_back(SGUID());
			partnerNames.push_back("None");

			auto visitStateMachine = [this, &partnerGUIDs, &partnerNames] (const IScriptStateMachine& stateMachine) -> EVisitStatus
			{
				if(stateMachine.GetLifetime() == EScriptStateMachineLifetime::Persistent)
				{
					const SGUID	stateMachineGUID = stateMachine.GetGUID();
					if(stateMachineGUID != m_guid)
					{
						partnerGUIDs.push_back(stateMachineGUID);
						partnerNames.push_back(stateMachine.GetName());
					}
				}
				return EVisitStatus::Continue;
			};
			CDomainContext(SDomainContextScope(&CScriptElementBase::GetFile(), m_scopeGUID)).VisitScriptStateMachines(ScriptStateMachineConstVisitor::FromLambdaFunction(visitStateMachine), EDomainScope::Local);

			GUIDVector::const_iterator     itPartnerGUID = std::find(partnerGUIDs.begin(), partnerGUIDs.end(), m_partnerGUID);
			const int                      partnerIdx = itPartnerGUID != partnerGUIDs.end() ? static_cast<int>(itPartnerGUID - partnerGUIDs.begin()) : 0;
			Serialization::StringListValue partnerName(partnerNames, partnerIdx);
			archive(partnerName, "partnerName", "Partner");
			if(archive.isInput())
			{
				m_partnerGUID = partnerGUIDs[partnerName.index()];
			}
		}
		else
		{
			archive(m_partnerGUID, "partnerGUID");
		}
	}
}
