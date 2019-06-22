// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "Context.h"

#include "Common.h"
#include "ContextManager.h"
#include "NameValidator.h"

namespace ACE
{
//////////////////////////////////////////////////////////////////////////
void CContext::SetName(string const& name)
{
	string fixedName = name;
	g_nameValidator.FixupString(fixedName);

	if ((!fixedName.IsEmpty()) && (fixedName != m_name) && g_nameValidator.IsValid(fixedName))
	{
		m_name = fixedName;
		m_id = g_contextManager.GenerateContextId(m_name);
	}
}
} // namespace ACE
