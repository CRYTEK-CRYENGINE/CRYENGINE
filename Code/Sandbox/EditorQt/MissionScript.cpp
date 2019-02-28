// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "missionscript.h"
#include <CryScriptSystem/IScriptSystem.h>

#define EVENT_PREFIX "Event_"

struct CMissionScriptMethodsDump : public IScriptTableDumpSink
{
	std::vector<CString> methods;
	std::vector<CString> events;
	void         OnElementFound(int nIdx, ScriptVarType type) { /*ignore non string indexed values*/ };
	virtual void OnElementFound(const char* sName, ScriptVarType type)
	{
		if (type == svtFunction)
		{
			if (strncmp(sName, EVENT_PREFIX, 6) == 0)
				events.push_back(sName + 6);
			else
				methods.push_back(sName);
		}
	}
};

CMissionScript::CMissionScript()
{
	m_sFilename = "";
}

CMissionScript::~CMissionScript()
{
}

bool CMissionScript::Load()
{
	if (m_sFilename.IsEmpty())
		return true;

	// Parse .lua file.
	IScriptSystem* script = GetIEditorImpl()->GetSystem()->GetIScriptSystem();
	if (!script->ExecuteFile(m_sFilename, true, true))
	{
		CString msg = CString("Unable to execute script '") + CString(GetIEditorImpl()->GetMasterCDFolder()) + m_sFilename + "'. Check syntax ! Script not loaded.";
		CryWarning(VALIDATOR_MODULE_EDITOR, VALIDATOR_ERROR, (const char*)msg);
		return false;
	}
	SmartScriptTable pMission(script, true);
	if (!script->GetGlobalValue("Mission", pMission))
	{
		CString msg = "Unable to find script-table 'Mission'. Check mission script ! Mission Script not loaded.";
		CryWarning(VALIDATOR_MODULE_EDITOR, VALIDATOR_ERROR, (const char*)msg);
		return false;
	}
	CMissionScriptMethodsDump dump;
	pMission->Dump(&dump);
	m_methods = dump.methods;
	m_events = dump.events;

	// Sort methods and events alphabetically.
	std::sort(m_methods.begin(), m_methods.end());
	std::sort(m_events.begin(), m_events.end());
	return true;
}

void CMissionScript::Edit()
{
	if (m_sFilename.IsEmpty())
		return;

	CFileUtil::EditTextFile(m_sFilename);
}

//////////////////////////////////////////////////////////////////////////
void CMissionScript::OnReset()
{
	IScriptSystem* pScriptSystem = GetIEditorImpl()->GetSystem()->GetIScriptSystem();

	SmartScriptTable pMission(pScriptSystem, true);
	if (!pScriptSystem->GetGlobalValue("Mission", pMission))
	{
		return;
	}

	HSCRIPTFUNCTION scriptFunction;
	if (pMission->GetValue("OnReset", scriptFunction))
		Script::CallMethod(pMission, "OnReset");
}

//////////////////////////////////////////////////////////////////////////
void CMissionScript::SetScriptFile(const CString& file)
{
	m_sFilename = file;
}

