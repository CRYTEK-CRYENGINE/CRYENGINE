// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "Script/Elements/ScriptModule.h"

#include <CrySchematyc2/GUIDRemapper.h>
#include <CrySchematyc2/Serialization/ISerializationContext.h>

namespace Schematyc2
{
	CScriptModule::CScriptModule(IScriptFile& file)
		: CScriptElementBase(EScriptElementType::Module, file)
	{}

	CScriptModule::CScriptModule(IScriptFile& file, const SGUID& guid, const SGUID& scopeGUID, const char* szName)
		: CScriptElementBase(EScriptElementType::Module, file)
		, m_guid(guid)
		, m_scopeGUID(scopeGUID)
		, m_name(szName)
	{}

	EAccessor CScriptModule::GetAccessor() const
	{
		return EAccessor::Private;
	}

	SGUID CScriptModule::GetGUID() const
	{
		return m_guid;
	}

	SGUID CScriptModule::GetScopeGUID() const
	{
		return m_scopeGUID;
	}

	bool CScriptModule::SetName(const char* szName)
	{
		m_name = szName;
		return true;
	}

	const char* CScriptModule::GetName() const
	{
		return m_name.c_str();
	}

	void CScriptModule::EnumerateDependencies(const ScriptDependancyEnumerator& enumerator) const {}

	void CScriptModule::Refresh(const SScriptRefreshParams& params) {}

	void CScriptModule::Serialize(Serialization::IArchive& archive)
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
				break;
			}
		}
	}

	void CScriptModule::RemapGUIDs(IGUIDRemapper& guidRemapper)
	{
		m_guid      = guidRemapper.Remap(m_guid);
		m_scopeGUID = guidRemapper.Remap(m_scopeGUID);
	}
}
