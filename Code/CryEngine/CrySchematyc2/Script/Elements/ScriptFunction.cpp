// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "Script/Elements/ScriptFunction.h"

#include <CrySchematyc2/GUIDRemapper.h>
#include <CrySchematyc2/Serialization/ISerializationContext.h>

#include "Script/ScriptPropertyGraph.h"

namespace Schematyc2
{
	void CScriptFunction::SInput::Serialize(Serialization::IArchive& archive)
	{
		archive(guid, "guid");
		archive(declaration, "declaration", "^");
	}

	void CScriptFunction::SOutput::Serialize(Serialization::IArchive& archive)
	{
		archive(guid, "guid");
		archive(declaration, "declaration", "^");
	}

	CScriptFunction::CScriptFunction(IScriptFile& file)
		: CScriptElementBase(EScriptElementType::Function, file)
	{
		CScriptElementBase::AddExtension(std::make_shared<CScriptPropertyGraph>(*this));
	}

	CScriptFunction::CScriptFunction(IScriptFile& file, const SGUID& guid, const SGUID& scopeGUID, const char* szName)
		: CScriptElementBase(EScriptElementType::Function, file)
		, m_guid(guid)
		, m_scopeGUID(scopeGUID)
		, m_name(szName)
	{
		m_userDocumentation.SetCurrentUserAsAuthor();
		CScriptElementBase::AddExtension(std::make_shared<CScriptPropertyGraph>(*this));
	}

	EAccessor CScriptFunction::GetAccessor() const
	{
		return EAccessor::Private;
	}

	SGUID CScriptFunction::GetGUID() const
	{
		return m_guid;
	}

	SGUID CScriptFunction::GetScopeGUID() const
	{
		return m_scopeGUID;
	}

	bool CScriptFunction::SetName(const char* szName)
	{
		m_name = szName;
		return true;
	}

	const char* CScriptFunction::GetName() const
	{
		return m_name.c_str();
	}

	void CScriptFunction::EnumerateDependencies(const ScriptDependancyEnumerator& enumerator) const {}

	void CScriptFunction::Refresh(const SScriptRefreshParams& params) {}

	void CScriptFunction::Serialize(Serialization::IArchive& archive)
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
				archive(m_inputs, "inputs");
				archive(m_outputs, "outputs");
				archive(m_userDocumentation, "userDocumentation");
				break;
			}
		case ESerializationPass::Load:
			{
				archive(m_inputs, "inputs");
				archive(m_outputs, "outputs");
				break;
			}
		case ESerializationPass::Edit:
			{
				archive(m_inputs, "inputs", "Inputs");
				archive(m_outputs, "outputs", "Outputs");
				archive(m_userDocumentation, "userDocumentation", "Documentation");
				break;
			}
		}
	}

	void CScriptFunction::RemapGUIDs(IGUIDRemapper& guidRemapper)
	{
		m_guid      = guidRemapper.Remap(m_guid);
		m_scopeGUID = guidRemapper.Remap(m_scopeGUID);
		// #SchematycTODO - Remap inputs and outputs?
	}
}
