// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "SerializationContext.h"

#include <CrySchematyc/Utils/Assert.h>

namespace Schematyc
{
CSerializationContext::CSerializationContext(const SSerializationContextParams& params)
	: m_context(params.archive, static_cast<ISerializationContext*>(nullptr))
	, m_pass(params.pass)
{
	SCHEMATYC_CORE_ASSERT(!params.archive.context<ISerializationContext>());
	m_context.set(static_cast<ISerializationContext*>(this));
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
}
