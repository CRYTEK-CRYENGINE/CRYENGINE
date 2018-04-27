// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "Script/Elements/ScriptAbstractInterfaceFunction.h"

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
	CScriptAbstractInterfaceFunction::CScriptAbstractInterfaceFunction(IScriptFile& file, const SGUID& guid, const SGUID& scopeGUID, const char* szName)
		: CScriptElementBase(EScriptElementType::AbstractInterfaceFunction, file)
		, m_guid(guid)
		, m_scopeGUID(scopeGUID)
		, m_name(szName)
	{}

	//////////////////////////////////////////////////////////////////////////
	EAccessor CScriptAbstractInterfaceFunction::GetAccessor() const
	{
		return EAccessor::Private;
	}

	//////////////////////////////////////////////////////////////////////////
	SGUID CScriptAbstractInterfaceFunction::GetGUID() const
	{
		return m_guid;
	}

	//////////////////////////////////////////////////////////////////////////
	SGUID CScriptAbstractInterfaceFunction::GetScopeGUID() const
	{
		return m_scopeGUID;
	}

	//////////////////////////////////////////////////////////////////////////
	bool CScriptAbstractInterfaceFunction::SetName(const char* szName)
	{
		m_name = szName;
		return true;
	}

	//////////////////////////////////////////////////////////////////////////
	const char* CScriptAbstractInterfaceFunction::GetName() const
	{
		return m_name.c_str();
	}

	//////////////////////////////////////////////////////////////////////////
	void CScriptAbstractInterfaceFunction::EnumerateDependencies(const ScriptDependancyEnumerator& enumerator) const
	{
		SCHEMATYC2_SYSTEM_ASSERT(enumerator);
		if(enumerator)
		{
			for(const CScriptVariableDeclaration& input : m_inputs)
			{
				input.EnumerateDependencies(enumerator);
			}
			for(const CScriptVariableDeclaration& output : m_outputs)
			{
				output.EnumerateDependencies(enumerator);
			}
		}
	}

	//////////////////////////////////////////////////////////////////////////
	void CScriptAbstractInterfaceFunction::Refresh(const SScriptRefreshParams& params)
	{
		if(params.reason == EScriptRefreshReason::EditorAdd)
		{
			m_userDocumentation.SetCurrentUserAsAuthor();
		}
	}

	//////////////////////////////////////////////////////////////////////////
	void CScriptAbstractInterfaceFunction::Serialize(Serialization::IArchive& archive)
	{
		LOADING_TIME_PROFILE_SECTION;

		CScriptElementBase::Serialize(archive);

		SerializationContext::SetValidatorLink(archive, SValidatorLink(m_guid)); // #SchematycTODO : Can we set this from CScriptElementBase?
		switch(SerializationContext::GetPass(archive))
		{
		case ESerializationPass::PreLoad:
			{
				archive(m_guid, "guid");
				archive(m_scopeGUID, "scope_guid");
				archive(m_name, "name");
				archive(m_inputs, "inputs");
				archive(m_outputs, "outputs");
				archive(m_userDocumentation, "userDocumentation", "Documentation");
				break;
			}
		case ESerializationPass::Load:
			{
				archive(m_inputs, "inputs");
				archive(m_outputs, "outputs");
				break;
			}
		case ESerializationPass::Save:
			{
				Serialization::SContext fileContext(archive, static_cast<IScriptFile*>(&CScriptElementBase::GetFile())); // #SchematycTODO : Do we still need this?
				archive(m_guid, "guid");
				archive(m_scopeGUID, "scope_guid");
				archive(m_name, "name");
				archive(m_inputs, "inputs");
				archive(m_outputs, "outputs");
				archive(m_userDocumentation, "userDocumentation", "Documentation");
				break;
			}
		case ESerializationPass::Edit:
			{
				Serialization::SContext fileContext(archive, static_cast<IScriptFile*>(&CScriptElementBase::GetFile())); // #SchematycTODO : Do we still need this?
				archive(m_inputs, "inputs", "Inputs");
				archive(m_outputs, "outputs", "Outputs");
				archive(m_userDocumentation, "userDocumentation", "Documentation");
				break;
			}
		}
	}

	//////////////////////////////////////////////////////////////////////////
	void CScriptAbstractInterfaceFunction::RemapGUIDs(IGUIDRemapper& guidRemapper)
	{
		m_guid      = guidRemapper.Remap(m_guid);
		m_scopeGUID = guidRemapper.Remap(m_scopeGUID);
	}

	//////////////////////////////////////////////////////////////////////////
	const char* CScriptAbstractInterfaceFunction::GetAuthor() const
	{
		return m_userDocumentation.author.c_str();
	}

	//////////////////////////////////////////////////////////////////////////
	const char* CScriptAbstractInterfaceFunction::GetDescription() const
	{
		return m_userDocumentation.description.c_str();
	}

	//////////////////////////////////////////////////////////////////////////
	size_t CScriptAbstractInterfaceFunction::GetInputCount() const
	{
		return m_inputs.size();
	}

	//////////////////////////////////////////////////////////////////////////
	const char* CScriptAbstractInterfaceFunction::GetInputName(size_t inputIdx) const
	{
		return inputIdx < m_inputs.size() ? m_inputs[inputIdx].GetName() : "";
	}

	//////////////////////////////////////////////////////////////////////////
	IAnyConstPtr CScriptAbstractInterfaceFunction::GetInputValue(size_t inputIdx) const
	{
		return inputIdx < m_inputs.size() ? m_inputs[inputIdx].GetValue() : IAnyConstPtr();
	}

	//////////////////////////////////////////////////////////////////////////
	size_t CScriptAbstractInterfaceFunction::GetOutputCount() const
	{
		return m_outputs.size();
	}

	//////////////////////////////////////////////////////////////////////////
	const char* CScriptAbstractInterfaceFunction::GetOutputName(size_t outputIdx) const
	{
		return outputIdx < m_outputs.size() ? m_outputs[outputIdx].GetName() : "";
	}

	//////////////////////////////////////////////////////////////////////////
	IAnyConstPtr CScriptAbstractInterfaceFunction::GetOutputValue(size_t outputIdx) const
	{
		return outputIdx < m_outputs.size() ? m_outputs[outputIdx].GetValue() : IAnyConstPtr();
	}
}
