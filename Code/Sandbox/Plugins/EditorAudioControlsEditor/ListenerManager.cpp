// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "ListenerManager.h"
#include <CrySystem/IConsole.h>

namespace ACE
{
//////////////////////////////////////////////////////////////////////////
void CListenerManager::Initialize()
{
	m_listeners.emplace_back(CryAudio::g_szDefaultListenerName);

	ICVar const* const pCVar = gEnv->pConsole->GetCVar(CryAudio::g_szListenersCVarName);

	if (pCVar != nullptr)
	{
		string const listenerNames = pCVar->GetString();

		if (!listenerNames.empty())
		{
			int curPos = 0;
			string listenerName = listenerNames.Tokenize(",", curPos);

			while (!listenerName.empty())
			{
				listenerName.Trim();

				GenerateUniqueListenerName(listenerName);
				m_listeners.emplace_back(listenerName);

				listenerName = listenerNames.Tokenize(",", curPos);
			}
		}
	}
}

//////////////////////////////////////////////////////////////////////////
void CListenerManager::GenerateUniqueListenerName(string& name)
{
	bool nameChanged = false;

	for (auto const& listenerName : m_listeners)
	{
		if (listenerName.compareNoCase(name) == 0)
		{
			name += "_1";
			nameChanged = true;
			break;
		}
	}

	if (nameChanged)
	{
		GenerateUniqueListenerName(name);
	}
}
} // namespace ACE
