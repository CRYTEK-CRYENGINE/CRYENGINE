// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "SerializationContext.h"

namespace Schematyc2
{
	CSerializationContext::CSerializationContext(const SSerializationContextParams& params)
		: m_context(params.archive, static_cast<ISerializationContext*>(nullptr))
		, m_pScriptFile(params.pScriptFile)
		, m_pass(params.pass)
	{
		SCHEMATYC2_SYSTEM_ASSERT(!params.archive.context<ISerializationContext>());
		m_context.set(static_cast<ISerializationContext*>(this));
	}

	IScriptFile* CSerializationContext::GetScriptFile() const
	{
		return m_pScriptFile;
	}

	ESerializationPass CSerializationContext::GetPass() const
	{
		return m_pass;
	}

	void CSerializationContext::SetValidatorLink(const SValidatorLink& validatorLink)
	{
		m_validatorLink = validatorLink;
	}

	const SValidatorLink& CSerializationContext::GetValidatorLink() const
	{
		return m_validatorLink;
	}

	void CSerializationContext::AddDependency(const void* pDependency, const EnvTypeId& typeId)
	{
		m_dependencies.insert(DependencyMap::value_type(typeId, pDependency));
	}

	const void* CSerializationContext::GetDependency_Protected(const EnvTypeId& typeId) const
	{
		DependencyMap::const_iterator itDependency = m_dependencies.find(typeId);
		return itDependency != m_dependencies.end() ? itDependency->second : nullptr;
	}
}
