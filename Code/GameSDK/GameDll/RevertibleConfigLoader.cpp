// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

/*************************************************************************
-------------------------------------------------------------------------
$Id$
$DateTime$
Description: Class that loads a config file with the ability to restore the
						 cvars to the values they were before loading them.

-------------------------------------------------------------------------
History:
- 25:07:2012  : Refactored into a class by Martin Sherburn

*************************************************************************/

#include "StdAfx.h"
#include "RevertibleConfigLoader.h"

CRevertibleConfigLoader::CRevertibleConfigLoader(int maxCvars, int maxTextBufferSize)
	: m_allowCheatCVars(false)
{
	m_savedCVars.reserve(maxCvars);
	m_cvarsTextBlock.IncreaseSizeNeeded(maxTextBufferSize);
	m_cvarsTextBlock.Allocate();
}

// Callback called whenever a cvar is about to be changed while reading a config file
void CRevertibleConfigLoader::OnLoadConfigurationEntry( const char *szKey, const char *szValue, const char *szGroup )
{
	ApplyAndStoreCVar(szKey, szValue);
}

void CRevertibleConfigLoader::LoadConfiguration(const char *szConfig)
{
	CryLog ("Loading %s configuration...", szConfig);
	INDENT_LOG_DURING_SCOPE();
	gEnv->pSystem->LoadConfiguration(szConfig, this, eLoadConfigGame);
	CryLog ("Finished loading %s configuration, used %" PRISIZE_T "/%" PRISIZE_T " bytes storing old cvar values", szConfig, m_cvarsTextBlock.GetNumBytesUsed(), m_cvarsTextBlock.GetSizeNeeded());
}

void CRevertibleConfigLoader::ApplyAndStoreCVar( const char *szKey, const char *szValue )
{
	ICVar * var = gEnv->pConsole->GetCVar(szKey);

	if (var == NULL)
	{
		GameWarning("Variable \"%s\" not found (trying to set it to '%s')", szKey, szValue);
		return;
	}

	const char * oldValue = var->GetString();

	if (oldValue!=0 && 0 == strcmp(szValue, oldValue))
	{
		CryLog("No need to change \"%s\" to '%s' as that's also its current value", szKey, szValue );
		return;
	}
	if (m_allowCheatCVars || (var->GetFlags() & VF_CHEAT) == 0)
	{
		const int numStoredCvars = (int)m_savedCVars.size();

#ifndef _RELEASE
		const char* savedValue = NULL;
		for (int i=0; i<numStoredCvars; i++)
		{
			if (stricmp(m_savedCVars[i].m_name, szKey) == 0)
			{
				savedValue = m_savedCVars[i].m_value;
				break;
			}
		}
		if (savedValue)
		{
			GameWarning("Already got an old value for cvar \"%s\" (currently '%s'), about to add another entry...", szKey, savedValue);
		}
#endif

		SSavedCVar savedCvar;
		savedCvar.m_name = m_cvarsTextBlock.StoreText(szKey);
		savedCvar.m_value = m_cvarsTextBlock.StoreText(var->GetString());

		if (savedCvar.m_name && savedCvar.m_value)
		{
			m_savedCVars.push_back(savedCvar);
			CryLog("Changing \"%s\" cvar from '%s' to '%s' (now got %" PRISIZE_T " %s to revert when switching game type)", szKey, oldValue, szValue, m_savedCVars.size(), (m_savedCVars.size() == 1) ? "cvar" : "cvars");
		}
		else
		{
			GameWarning("!Ran out of room in cvar array.\nPlease increase maxTextBufferSize - current value is %" PRISIZE_T ".\nFor now, game won't remember to undo \"%s = %s\".", m_cvarsTextBlock.GetSizeNeeded(), szKey, var->GetString());
		}
	}
	else
	{
		GameWarning("!Can't change \"%s\" cvar from '%s' to '%s' because it is a cheat var and cannot be changed in release mode", szKey, oldValue, szValue);
		return;
	}

	CryFixedStringT<128> cmd;
	cmd.Format("%s %s", szKey, szValue);

	IConsole * pConsole = gEnv->pConsole;
	pConsole->ExecuteString(cmd.c_str(), true);
}

void CRevertibleConfigLoader::RevertCVarChanges()
{
	if (!m_savedCVars.empty())
	{
		CryLog ("Need to undo %" PRISIZE_T " %s...", m_savedCVars.size(), (m_savedCVars.size() == 1) ? "variable" : "variables");
		IConsole * pConsole = gEnv->pConsole;
		CryFixedStringT<128> cmd;
		//Revert the saved cvars in reverse order to handle duplicate settings of the same cvar (which shouldn't be done but people ignore warnings)
		for (int n = m_savedCVars.size()-1; n >= 0; --n)
		{
			ICVar * var = gEnv->pConsole->GetCVar(m_savedCVars[n].m_name);
			
			if (var && var->GetType() == CVAR_STRING && strlen(m_savedCVars[n].m_value) == 0)
			{
				var->Set(m_savedCVars[n].m_value);
			}
			else
			{
				cmd.Format("%s %s", m_savedCVars[n].m_name, m_savedCVars[n].m_value);
			}
			
			pConsole->ExecuteString(cmd.c_str(), true);
		}

		m_cvarsTextBlock.EmptyWithoutFreeing();
		m_savedCVars.clear();
	}
}
