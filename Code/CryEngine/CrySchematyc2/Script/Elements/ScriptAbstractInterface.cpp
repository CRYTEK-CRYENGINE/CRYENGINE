// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "Script/Elements/ScriptAbstractInterface.h"

#include <CrySerialization/BlackBox.h>
#include <CrySerialization/IArchiveHost.h>
#include <CrySerialization/STL.h>
#include <CrySchematyc2/GUIDRemapper.h>
#include <CrySchematyc2/Deprecated/DocUtils.h>
#include <CrySchematyc2/Env/IEnvRegistry.h>
#include <CrySchematyc2/Serialization/ISerializationContext.h>
#include <CrySchematyc2/Serialization/SerializationUtils.h>

namespace Schematyc2
{
	//////////////////////////////////////////////////////////////////////////
	CScriptAbstractInterface::CScriptAbstractInterface(IScriptFile& file, const SGUID& guid, const SGUID& scopeGUID, const char* szName)
		: CScriptElementBase(EScriptElementType::AbstractInterface, file)
		, m_guid(guid)
		, m_scopeGUID(scopeGUID)
		, m_name(szName)
	{}

	//////////////////////////////////////////////////////////////////////////
	EAccessor CScriptAbstractInterface::GetAccessor() const
	{
		return EAccessor::Private;
	}

	//////////////////////////////////////////////////////////////////////////
	SGUID CScriptAbstractInterface::GetGUID() const
	{
		return m_guid;
	}

	//////////////////////////////////////////////////////////////////////////
	SGUID CScriptAbstractInterface::GetScopeGUID() const
	{
		return m_scopeGUID;
	}

	//////////////////////////////////////////////////////////////////////////
	bool CScriptAbstractInterface::SetName(const char* szName)
	{
		m_name = szName;
		return true;
	}

	//////////////////////////////////////////////////////////////////////////
	const char* CScriptAbstractInterface::GetName() const
	{
		return m_name.c_str();
	}

	//////////////////////////////////////////////////////////////////////////
	void CScriptAbstractInterface::EnumerateDependencies(const ScriptDependancyEnumerator& enumerator) const {}

	//////////////////////////////////////////////////////////////////////////
	void CScriptAbstractInterface::Refresh(const SScriptRefreshParams& params)
	{
		if(params.reason == EScriptRefreshReason::EditorAdd)
		{
			m_userDocumentation.SetCurrentUserAsAuthor();
		}
	}

	//////////////////////////////////////////////////////////////////////////
	void CScriptAbstractInterface::Serialize(Serialization::IArchive& archive)
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
				archive(m_userDocumentation, "userDocumentation", "Documentation");
				break;
			}
		case ESerializationPass::Edit:
			{
				archive(m_userDocumentation, "userDocumentation", "Documentation");
				break;
			}
		}
	}

	//////////////////////////////////////////////////////////////////////////
	void CScriptAbstractInterface::RemapGUIDs(IGUIDRemapper& guidRemapper)
	{
		m_guid      = guidRemapper.Remap(m_guid);
		m_scopeGUID = guidRemapper.Remap(m_scopeGUID);
	}

	//////////////////////////////////////////////////////////////////////////
	const char* CScriptAbstractInterface::GetAuthor() const
	{
		return m_userDocumentation.author.c_str();
	}

	//////////////////////////////////////////////////////////////////////////
	const char* CScriptAbstractInterface::GetDescription() const
	{
		return m_userDocumentation.description.c_str();
	}
}
