// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "Script/Elements/ScriptState.h"

#include <CrySerialization/BlackBox.h>
#include <CrySerialization/IArchiveHost.h>
#include <CrySerialization/STL.h>
#include <CrySchematyc2/GUIDRemapper.h>
#include <CrySchematyc2/Deprecated/DocUtils.h>
#include <CrySchematyc2/Env/IEnvRegistry.h>
#include <CrySchematyc2/Serialization/ISerializationContext.h>
#include <CrySchematyc2/Serialization/SerializationUtils.h>

#include "DomainContext.h"

namespace Schematyc2
{
	CScriptState::CScriptState(IScriptFile& file, const SGUID& guid, const SGUID& scopeGUID, const char* szName, const SGUID& partnerGUID)
		: CScriptElementBase(EScriptElementType::State, file)
		, m_guid(guid)
		, m_scopeGUID(scopeGUID)
		, m_name(szName)
		, m_partnerGUID(partnerGUID)
	{}

	EAccessor CScriptState::GetAccessor() const
	{
		return EAccessor::Private;
	}

	SGUID CScriptState::GetGUID() const
	{
		return m_guid;
	}

	SGUID CScriptState::GetScopeGUID() const
	{
		return m_scopeGUID;
	}

	bool CScriptState::SetName(const char* szName)
	{
		m_name = szName;
		return true;
	}

	const char* CScriptState::GetName() const
	{
		return m_name.c_str();
	}

	void CScriptState::EnumerateDependencies(const ScriptDependancyEnumerator& enumerator) const {}

	void CScriptState::Refresh(const SScriptRefreshParams& params) {}

	void CScriptState::Serialize(Serialization::IArchive& archive)
	{
		LOADING_TIME_PROFILE_SECTION;

		CScriptElementBase::Serialize(archive);

		SerializationContext::SetValidatorLink(archive, SValidatorLink(m_guid)); // #SchematycTODO : Can we set this from CScriptElementBase?
		switch(SerializationContext::GetPass(archive))
		{
		case ESerializationPass::PreLoad:
		case ESerializationPass::Save:
			{
				archive(m_guid, "guid");
				archive(m_scopeGUID, "scope_guid");
				archive(m_name, "name");

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

	void CScriptState::RemapGUIDs(IGUIDRemapper& guidRemapper)
	{
		m_guid        = guidRemapper.Remap(m_guid);
		m_scopeGUID   = guidRemapper.Remap(m_scopeGUID);
		m_partnerGUID = guidRemapper.Remap(m_partnerGUID);
	}

	SGUID CScriptState::GetPartnerGUID() const
	{
		return m_partnerGUID;
	}

	void CScriptState::SerializePartner(Serialization::IArchive& archive)
	{
		if(archive.isEdit())
		{
			const IScriptStateMachine* pStateMachine = nullptr;
			for(const IScriptElement* pParent = CScriptElementBase::GetParent(); pParent; pParent = pParent->GetParent())
			{
				if(pParent->GetElementType() == EScriptElementType::StateMachine)
				{
					pStateMachine = static_cast<const IScriptStateMachine*>(pParent);
					break;
				}
			}

			if(pStateMachine)
			{
				const IScriptStateMachine* pPartnerStateMachine = nullptr;
				{
					CDomainContext domainContext(SDomainContextScope(&pStateMachine->GetFile(), pStateMachine->GetGUID()));
					pPartnerStateMachine = domainContext.GetScriptStateMachine(pStateMachine->GetPartnerGUID());
				}
				if(pPartnerStateMachine)
				{
					GUIDVector                partnerGUIDs;
					Serialization::StringList partnerNames;
					partnerGUIDs.reserve(25);
					partnerNames.reserve(25);
					partnerGUIDs.push_back(SGUID());
					partnerNames.push_back("None");

					auto visitScriptState = [&partnerGUIDs, &partnerNames] (const IScriptState& scriptState) -> EVisitStatus
					{
						partnerGUIDs.push_back(scriptState.GetGUID());
						partnerNames.push_back(scriptState.GetName());
						return EVisitStatus::Continue;
					};

					CDomainContext domainContext(SDomainContextScope(&pPartnerStateMachine->GetFile(), pPartnerStateMachine->GetGUID()));
					domainContext.VisitScriptStates(ScriptStateConstVisitor::FromLambdaFunction(visitScriptState), EDomainScope::Local);

					GUIDVector::const_iterator     itPartnerGUID = std::find(partnerGUIDs.begin(), partnerGUIDs.end(), m_partnerGUID);
					const int                      partnerIdx = itPartnerGUID != partnerGUIDs.end() ? static_cast<int>(itPartnerGUID - partnerGUIDs.begin()) : 0;
					Serialization::StringListValue partnerName(partnerNames, partnerIdx);
					archive(partnerName, "partnerName", "Partner");
					if(archive.isInput())
					{
						m_partnerGUID = partnerGUIDs[partnerName.index()];
					}
				}
			}
		}
		else
		{
			archive(m_partnerGUID, "partnerGUID");
		}
	}
}
