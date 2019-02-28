// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

// -------------------------------------------------------------------------
//  File name:   DialogSystem.cpp
//  Version:     v1.00
//  Created:     07/07/2006 by AlexL
//  Compilers:   Visual Studio.NET
//  Description: Dialog System
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////
#include "StdAfx.h"
#include "DialogSystem.h"

#include "DialogLoader.h"
#include "DialogLoaderMK2.h"
#include "DialogScript.h"
#include "DialogSession.h"
#include "DialogCommon.h"

#include "DialogActorContext.h"

#include "CryAction.h"

#define DIALOG_LIBS_PATH_EXCEL "Libs/Dialogs"
#define DIALOG_LIBS_PATH_MK2   "Libs/Dialogs"

int CDialogSystem::sDiaLOGLevel = 0;
int CDialogSystem::sPrecacheSounds = 0;
int CDialogSystem::sLoadSoundSynchronously = 0;
int CDialogSystem::sAutoReloadScripts = 0;
int CDialogSystem::sLoadExcelScripts = 0;
int CDialogSystem::sWarnOnMissingLoc = 0;
ICVar* CDialogSystem::ds_LevelNameOverride = 0;

namespace
{
void ScriptReload(IConsoleCmdArgs* pArgs)
{
	CDialogSystem* pDS = CCryAction::GetCryAction()->GetDialogSystem();
	if (pDS)
	{
		pDS->ReloadScripts();
	}
}

///////////////////////////////////////////////////////////////////////////
void ScriptDump(IConsoleCmdArgs* pArgs)
{
	CDialogSystem* pDS = CCryAction::GetCryAction()->GetDialogSystem();
	if (pDS)
	{
		int verbosity = 0;
		if (pArgs->GetArgCount() > 1)
			verbosity = atoi(pArgs->GetArg(1));
		pDS->Dump(verbosity);
	}
}

///////////////////////////////////////////////////////////////////////////
void ScriptDumpSessions(IConsoleCmdArgs* pArgs)
{
	CDialogSystem* pDS = CCryAction::GetCryAction()->GetDialogSystem();
	if (pDS)
	{
		pDS->DumpSessions();
	}
}

///////////////////////////////////////////////////////////////////////////
bool InitCons()
{
	REGISTER_COMMAND("ds_Reload", ScriptReload, VF_NULL, "");
	REGISTER_COMMAND("ds_Dump", ScriptDump, VF_NULL, "");
	REGISTER_COMMAND("ds_DumpSessions", ScriptDumpSessions, VF_NULL, "");

	// LogLevel of DiaLOG messages

	REGISTER_CVAR2("ds_LogLevel", &CDialogSystem::sDiaLOGLevel, 0, 0, "Set the verbosity of DiaLOG Messages");
	// Precaching of sounds on dialog sounds [0 by default now, until SoundSystem supports un-caching]
	REGISTER_CVAR2("ds_PrecacheSounds", &CDialogSystem::sPrecacheSounds, 0, 0, "Precache sounds on Dialog Begin");
	// Load sounds synchronously (off by default)
	REGISTER_CVAR2("ds_LoadSoundsSync", &CDialogSystem::sLoadSoundSynchronously, 0, 0, "Load Sounds synchronously");
	// Precaching of sounds on dialog sounds [0 by default now, until SoundSystem supports un-caching]
	REGISTER_CVAR2("ds_AutoReloadScripts", &CDialogSystem::sAutoReloadScripts, 0, VF_CHEAT, "Automatically reload DialogScripts when jumping into GameMode from Editor");
	// Load old Excel based scripts
	REGISTER_CVAR2("ds_LoadExcelScripts", &CDialogSystem::sLoadExcelScripts, 1, 0, "Load legacy Excel based dialogs.");
	// Warn on missing Localization entries
	REGISTER_CVAR2("ds_WarnOnMissingLoc", &CDialogSystem::sWarnOnMissingLoc, 1, 0, "Warn on Missing Localization Entries");

	// Warn on missing Localization entries
	CDialogSystem::ds_LevelNameOverride = REGISTER_STRING("ds_LevelNameOverride", "", 0,
	                                                      "Load dialog assets from the specified level name instead of the current level name."
	                                                      "This var gets cleared after level the level is loaded.");

	return true;
}
/*
   void ReleaseConsole()
   {
    IConsole* pConsole = gEnv->pConsole;

    pConsole->RemoveCommand("ds_Reload");
    pConsole->RemoveCommand("ds_Dump");
    pConsole->RemoveCommand("ds_DumpSessions");

    pConsole->UnregisterVariable("ds_LogLevel", true);
    pConsole->UnregisterVariable("ds_PrecacheSounds", true);
    pConsole->UnregisterVariable("ds_LoadSoundsSync", true);
    pConsole->UnregisterVariable("ds_AutoReloadScripts", true);
    pConsole->UnregisterVariable("ds_LoadExcelScripts", true);
    pConsole->UnregisterVariable("ds_PlayerAwareAngle", true);
    pConsole->UnregisterVariable("ds_PlayerAwareDist", true);
    pConsole->UnregisterVariable("ds_PlayerGraceTime", true);
   }
 */
};

class CDialogSystem::CDialogScriptIterator : public IDialogScriptIterator
{
public:
	CDialogScriptIterator(CDialogSystem* pDS)
	{
		m_nRefs = 0;
		m_cur = pDS->m_dialogScriptMap.begin();
		m_end = pDS->m_dialogScriptMap.end();
	}
	void AddRef()
	{
		++m_nRefs;
	}
	void Release()
	{
		if (0 == --m_nRefs)
			delete this;
	}
	bool Next(IDialogScriptIterator::SDialogScript& s)
	{
		if (m_cur != m_end)
		{
			const CDialogScript* pScript = m_cur->second;
			s.id = pScript->GetID();
			s.desc = pScript->GetDescription();
			s.numRequiredActors = pScript->GetNumRequiredActors();
			s.numLines = pScript->GetNumLines();
			s.bIsLegacyExcel = pScript->GetVersionFlags() & CDialogScript::VF_EXCEL_BASED;
			++m_cur;
			return true;
		}
		else
		{
			s.id = 0;
			s.numRequiredActors = 0;
			s.numLines = 0;
			return false;
		}
	}

	int                        m_nRefs;
	TDialogScriptMap::iterator m_cur;
	TDialogScriptMap::iterator m_end;
};

////////////////////////////////////////////////////////////////////////////
CDialogSystem::CDialogSystem()
{
	static bool sInitVars(InitCons());
	m_nextSessionID = 1;

	CCryAction::GetCryAction()->GetILevelSystem()->AddListener(this);
}

////////////////////////////////////////////////////////////////////////////
CDialogSystem::~CDialogSystem()
{
	ReleaseSessions();
	ReleaseScripts();

	if (CCryAction::GetCryAction()->GetILevelSystem())
		CCryAction::GetCryAction()->GetILevelSystem()->RemoveListener(this);
}

////////////////////////////////////////////////////////////////////////////
/* virtual */ bool CDialogSystem::Init()
{
	// (MATT) Loading just the dialog for one level works only in Game, but saves a lot of RAM.
	// In Editor it seems very awkward to arrange so lets just load everything {2008/08/20}
	if (gEnv->IsEditor())
		ReloadScripts();
	return true;
}

////////////////////////////////////////////////////////////////////////////
/* virtual */ void CDialogSystem::Reset(bool bUnload)
{
	m_dialogQueueManager.Reset();
	ReleaseSessions();
	if (bUnload)
		ReleaseScripts();
}

////////////////////////////////////////////////////////////////////////////
void CDialogSystem::ReleaseScripts()
{
	TDialogScriptMap::iterator iter = m_dialogScriptMap.begin();
	TDialogScriptMap::iterator end = m_dialogScriptMap.end();
	while (iter != end)
	{
		delete iter->second;
		++iter;
	}
	m_dialogScriptMap.clear();
}

////////////////////////////////////////////////////////////////////////////
void CDialogSystem::ReleasePendingDeletes()
{
	if (m_pendingDeleteSessions.empty() == false)
	{
		TDialogSessionVec::iterator iter = m_pendingDeleteSessions.begin();
		TDialogSessionVec::iterator end = m_pendingDeleteSessions.end();
		while (iter != end)
		{
			// session MUST not be in m_allSessions! otherwise could be released twice
			assert(m_allSessions.find((*iter)->GetSessionID()) == m_allSessions.end());
			(*iter)->Release();
			++iter;
		}
		stl::free_container(m_pendingDeleteSessions);
	}
}

////////////////////////////////////////////////////////////////////////////
void CDialogSystem::ReleaseSessions()
{
	ReleasePendingDeletes();
	TDialogSessionMap::iterator iter = m_allSessions.begin();
	TDialogSessionMap::iterator end = m_allSessions.end();
	while (iter != end)
	{
		(*iter).second->Release();
		++iter;
	}
	stl::free_container(m_activeSessions);
	stl::free_container(m_allSessions);
	stl::free_container(m_restoreSessions);

	CDialogActorContext::ResetStaticData();

	m_nextSessionID = 1;
}

////////////////////////////////////////////////////////////////////////////
void CDialogSystem::OnLoadingComplete(ILevelInfo* pLevel)
{
	if (pLevel)
	{
		const char* levelName = pLevel->GetName();
		const char* overrideName = ds_LevelNameOverride->GetString();

		if (*overrideName)
			levelName = overrideName;

		ReloadScripts(levelName);

		ds_LevelNameOverride->ForceSet("");
	}
}

////////////////////////////////////////////////////////////////////////////
/* virtual */ void CDialogSystem::Shutdown()
{
	ReleaseSessions();
	ReleaseScripts();
}

////////////////////////////////////////////////////////////////////////////
/* virtual */ bool CDialogSystem::ReloadScripts(const char* levelName)
{
	LOADING_TIME_PROFILE_SECTION(gEnv->pSystem);

	ReleaseSessions();
	ReleaseScripts();

	bool bSuccessOld = false;
	bool bSuccessNew = false;

	// load old excel based dialogs
	if (sLoadExcelScripts)
	{
		CDialogLoader loader(this);

		string path = DIALOG_LIBS_PATH_EXCEL;
		bSuccessOld = loader.LoadScriptsFromPath(path, m_dialogScriptMap);
	}

	// load new DialogEditor based dialogs
	{
		CDialogLoaderMK2 loader(this);

		const string path = DIALOG_LIBS_PATH_MK2;
		bSuccessNew = loader.LoadScriptsFromPath(path, m_dialogScriptMap, levelName);

		if (!gEnv->IsEditor())
		{
			bSuccessNew |= loader.LoadScriptsFromPath(path, m_dialogScriptMap, "All_Levels");
		}
	}

	return bSuccessOld || bSuccessNew;
}

////////////////////////////////////////////////////////////////////////////
const CDialogScript* CDialogSystem::GetScriptByID(const string& scriptID) const
{
	return stl::find_in_map(m_dialogScriptMap, scriptID, 0);
}

////////////////////////////////////////////////////////////////////////////
// Creates a new sessionwith sessionID m_nextSessionID and increases m_nextSessionID
CDialogSystem::SessionID CDialogSystem::CreateSession(const string& scriptID)
{
	CDialogSession* pSession = InternalCreateSession(scriptID, m_nextSessionID);
	if (pSession)
	{
		++m_nextSessionID;
		return pSession->GetSessionID();
	}
	return 0;
}

////////////////////////////////////////////////////////////////////////////
// Uses sessionID for newly allocated session
CDialogSession* CDialogSystem::InternalCreateSession(const string& scriptID, CDialogSystem::SessionID sessionID)
{
	const CDialogScript* pScript = GetScriptByID(scriptID);
	if (pScript == 0)
	{
		GameWarning("CDialogSystem::CreateSession: DialogScript '%s' unknown.", scriptID.c_str());
		return 0;
	}

	CDialogSession* pSession = new CDialogSession(this, pScript, sessionID);
	std::pair<TDialogSessionMap::iterator, bool> ok = m_allSessions.insert(TDialogSessionMap::value_type(sessionID, pSession));
	if (ok.second == false)
	{
		assert(false);
		GameWarning("CDialogSystem::CreateSession: Duplicate SessionID %d", sessionID);
		delete pSession;
		pSession = 0;
	}
	return pSession;
}

////////////////////////////////////////////////////////////////////////////
CDialogSession* CDialogSystem::GetSession(CDialogSystem::SessionID id) const
{
	return stl::find_in_map(m_allSessions, id, 0);
}

////////////////////////////////////////////////////////////////////////////
CDialogSession* CDialogSystem::GetActiveSession(CDialogSystem::SessionID id) const
{
	for (TDialogSessionVec::const_iterator it = m_activeSessions.begin(); it != m_activeSessions.end(); ++it)
	{
		if ((*it)->GetSessionID() == id)
		{
			return *it;
		}
	}

	return 0;
}
////////////////////////////////////////////////////////////////////////////

CDialogActorContext* CDialogSystem::GetActiveSessionActorContext(ActorContextID id) const
{
	for (TDialogSessionVec::const_iterator it = m_activeSessions.begin(); it != m_activeSessions.end(); ++it)
	{
		CDialogSession::CDialogActorContextPtr foundContextWithMatchingID = (*it)->GetContext(id);
		if (foundContextWithMatchingID)
		{
			return foundContextWithMatchingID;
		}
	}

	return 0;
}

////////////////////////////////////////////////////////////////////////////
bool CDialogSystem::DeleteSession(CDialogSystem::SessionID id)
{
	TDialogSessionMap::iterator iter = m_allSessions.find(id);
	if (iter == m_allSessions.end())
		return false;
	CDialogSession* pSession = iter->second;
	// remove it from the active sessions
	RemoveSession(pSession);
	stl::push_back_unique(m_pendingDeleteSessions, pSession);
	m_allSessions.erase(iter);
	stl::find_and_erase(m_restoreSessions, id); // erase it from sessions which will be restored
	return true;
}

////////////////////////////////////////////////////////////////////////////
bool CDialogSystem::AddSession(CDialogSession* pSession)
{
	return stl::push_back_unique(m_activeSessions, pSession);
}

////////////////////////////////////////////////////////////////////////////
bool CDialogSystem::RemoveSession(CDialogSession* pSession)
{
	return stl::find_and_erase(m_activeSessions, pSession);
}

////////////////////////////////////////////////////////////////////////////
void CDialogSystem::Update(const float dt)
{
	CRY_PROFILE_FUNCTION(PROFILE_ACTION);

	RestoreSessions();

	if (!m_activeSessions.empty())
	{
		//original vector can get invalidate if elements are deleted during update calls. therefore we create a copy first
		TDialogSessionVec activeSessionsCopy(m_activeSessions);
		for (TDialogSessionVec::const_iterator it = activeSessionsCopy.begin(); it != activeSessionsCopy.end(); ++it)
		{
			(*it)->Update(dt);
		}
	}

	ReleasePendingDeletes();

	m_dialogQueueManager.Update();
}

///////////////////////////////////////////////////////////////////////////
void CDialogSystem::RestoreSessions()
{
	if (m_restoreSessions.empty() == false)
	{
		std::vector<SessionID>::iterator iter = m_restoreSessions.begin();
		std::vector<SessionID>::iterator end = m_restoreSessions.end();
		while (iter != end)
		{
			bool ok = false;
			CDialogSession* pSession = GetSession(*iter);
			if (pSession)
			{
				DiaLOG::Log(DiaLOG::eDebugA, "[DIALOG] CDialogSystem::RestoreSessions: Session=%s", pSession->GetDebugName());
				ok = pSession->RestoreAndPlay();
			}

			if (!ok)
			{
				SessionID id = *iter;
				GameWarning("[DIALOG] CDialogSystem::Update: Cannot restore session %d", id);
			}
			++iter;
		}
		m_restoreSessions.resize(0);
	}
}

////////////////////////////////////////////////////////////////////////////
IDialogScriptIteratorPtr CDialogSystem::CreateScriptIterator()
{
	return new CDialogScriptIterator(this);
}

////////////////////////////////////////////////////////////////////////////
void CDialogSystem::Serialize(TSerialize ser)
{
	m_dialogQueueManager.Serialize(ser);

	if (ser.IsWriting())
	{
		// All Sessions
		uint32 count = m_allSessions.size();
		ser.Value("sessionCount", count);
		for (TDialogSessionMap::const_iterator iter = m_allSessions.begin(); iter != m_allSessions.end(); ++iter)
		{
			CDialogSession* pSession = iter->second;
			ser.BeginGroup("Session");
			int sessionID = pSession->GetSessionID();
			ser.Value("id", sessionID);
			ser.Value("script", pSession->GetScript()->GetID());
			pSession->Serialize(ser);
			ser.EndGroup();
		}

		// Active Sessions: We store the SessionID of active session. They will get restored on Load
		std::vector<int> temp;
		temp.reserve(m_activeSessions.size());
		for (TDialogSessionVec::const_iterator iter = m_activeSessions.begin(); iter != m_activeSessions.end(); ++iter)
		{
			temp.push_back((*iter)->GetSessionID());
		}
		ser.Value("m_activeSessions", temp);

		// next session id
		ser.Value("m_nextSessionID", m_nextSessionID);
	}
	else
	{
		// Delete/Clean all sessions
		ReleaseSessions();

		// Serialize All Sessions
		uint32 sessionCount = 0;
		ser.Value("sessionCount", sessionCount);
		for (int i = 0; i < sessionCount; ++i)
		{
			ser.BeginGroup("Session");
			int id = 0;
			string scriptID;
			ser.Value("id", id);
			ser.Value("script", scriptID);
			CDialogSession* pSession = InternalCreateSession(scriptID, id);
			if (pSession)
			{
				pSession->Serialize(ser);
			}
			ser.EndGroup();
		}

		// Active sessions restore
		// Make sure that ID's are unique in there
		std::vector<int> temp;
		ser.Value("m_activeSessions", temp);
		std::set<int> tempSet(temp.begin(), temp.end());
		// good when temp.size() is rather large, otherwise push_back_unique would be better
		assert(tempSet.size() == temp.size());
		if (tempSet.size() != temp.size())
		{
			GameWarning("[DIALOG] CDialogSystem::Serialize: Active Sessions are not unique!");
		}

		// Store IDs of Session to be restored. They get restored on 1st Update call
		m_restoreSessions.insert(m_restoreSessions.end(), tempSet.begin(), tempSet.end());

		// next session id: in case we couldn't recreate a session (script invalid)
		// the m_nextSessionID should be taken from file
		int nextSessionID = m_nextSessionID;
		ser.Value("m_nextSessionID", nextSessionID);
		assert(nextSessionID >= m_nextSessionID);
		m_nextSessionID = nextSessionID;
	}
}

///////////////////////////////////////////////////////////////////////////
const char* ToActor(CDialogScript::TActorID id)
{
	switch (id)
	{
	case 0:
		return "Actor1";
	case 1:
		return "Actor2";
	case 2:
		return "Actor3";
	case 3:
		return "Actor4";
	case 4:
		return "Actor5";
	case 5:
		return "Actor6";
	case 6:
		return "Actor7";
	case 7:
		return "Actor8";
	case CDialogScript::NO_ACTOR_ID:
		return "<none>";
	case CDialogScript::STICKY_LOOKAT_RESET_ID:
		return "<reset>";
	default:
		return "ActorX";
	}
}

////////////////////////////////////////////////////////////////////////////
void CDialogSystem::Dump(int verbosity)
{
	int i = 0;
	TDialogScriptMap::const_iterator iter = m_dialogScriptMap.begin();
	while (iter != m_dialogScriptMap.end())
	{
		const CDialogScript* pScript = iter->second;
		CryLogAlways("Dialog %3d ID='%s' Lines=%d  NumRequiredActors=%d", i, pScript->GetID().c_str(), pScript->GetNumLines(), pScript->GetNumRequiredActors());
		if (verbosity > 0)
		{
			for (int nLine = 0; nLine < pScript->GetNumLines(); ++nLine)
			{
				const CDialogScript::SScriptLine* pLine = pScript->GetLine(nLine);
				CryLogAlways("Line%3d: %s | AudioTrigger=%u StopAnim=%u | Facial=%s Reset=%u W=%.2f T=%.2f| Anim=%s [%s] EP=%u | LookAt=%s Sticky=%u Reset=%u | Delay=%.2f",
				             nLine + 1, ToActor(pLine->m_actor), pLine->m_audioID, pLine->m_flagSoundStopsAnim, pLine->m_facial.c_str(), pLine->m_flagResetFacial, pLine->m_facialWeight, pLine->m_facialFadeTime,
				             pLine->m_anim.c_str(), pLine->m_flagAGSignal ? "SIG" : "ACT", pLine->m_flagAGEP, ToActor(pLine->m_lookatActor), pLine->m_flagLookAtSticky, pLine->m_flagResetLookAt, pLine->m_delay);
			}
		}
		++i;
		++iter;
	}
}

////////////////////////////////////////////////////////////////////////////
void CDialogSystem::DumpSessions()
{
	// all sessions
	CryLogAlways("[DIALOG] AllSessions: Count=%" PRISIZE_T, m_allSessions.size());
	for (TDialogSessionMap::const_iterator iter = m_allSessions.begin();
	     iter != m_allSessions.end(); ++iter)
	{
		const CDialogSession* pSession = iter->second;
		CryLogAlways("  Session %d 0x%p Script=%s", pSession->GetSessionID(), pSession, pSession->GetScript()->GetID().c_str());
	}

	if (m_activeSessions.empty() == false)
	{
		CryLogAlways("[DIALOG] ActiveSessions: Count=%" PRISIZE_T, m_activeSessions.size());
		// active sessions
		for (TDialogSessionVec::const_iterator iter = m_activeSessions.begin();
		     iter != m_activeSessions.end(); ++iter)
		{
			const CDialogSession* pSession = *iter;
			CryLogAlways("  Session %d 0x%p Script=%s", pSession->GetSessionID(), pSession, pSession->GetScript()->GetID().c_str());
		}
	}

	// pending delete sessions
	if (m_pendingDeleteSessions.empty() == false)
	{
		CryLogAlways("[DIALOG] PendingDelete: Count=%" PRISIZE_T, m_pendingDeleteSessions.size());
		// active sessions
		for (TDialogSessionVec::const_iterator iter = m_pendingDeleteSessions.begin();
		     iter != m_pendingDeleteSessions.end(); ++iter)
		{
			const CDialogSession* pSession = *iter;
			CryLogAlways("  Session %d 0x%p Script=%s", pSession->GetSessionID(), pSession, pSession->GetScript()->GetID().c_str());
		}
	}
	// restore sessions
	if (m_restoreSessions.empty() == false)
	{
		CryLogAlways("[DIALOG] RestoreSessions: Count=%" PRISIZE_T, m_restoreSessions.size());
		for (std::vector<SessionID>::const_iterator iter = m_restoreSessions.begin();
		     iter != m_restoreSessions.end(); ++iter)
		{
			SessionID id = *iter;
			CryLogAlways("  Session %d", id);
		}
	}
}

////////////////////////////////////////////////////////////////////////////
bool CDialogSystem::IsEntityInDialog(EntityId entityId) const
{
	CDialogScript::TActorID actorID;
	CDialogSystem::SessionID sessionID;
	return FindSessionAndActorForEntity(entityId, sessionID, actorID);
}

////////////////////////////////////////////////////////////////////////////
bool CDialogSystem::FindSessionAndActorForEntity(EntityId entityId, CDialogSystem::SessionID& outSessionID, CDialogScript::TActorID& outActorId) const
{
	TDialogSessionVec::const_iterator iter = m_activeSessions.begin();
	TDialogSessionVec::const_iterator end = m_activeSessions.end();

	CDialogScript::TActorID actorID;
	while (iter != end)
	{
		const CDialogSession* pSession = *iter;
		actorID = pSession->GetActorIdForEntity(entityId);
		if (actorID != CDialogScript::NO_ACTOR_ID)
		{
			outSessionID = pSession->GetSessionID();
			outActorId = actorID;
			return true;
		}
		++iter;
	}

	outSessionID = 0;
	outActorId = CDialogScript::NO_ACTOR_ID;
	return false;
}

///////////////////////////////////////////////////////////////////////////
void CDialogSystem::GetMemoryStatistics(ICrySizer* pSizer)
{
	SIZER_SUBCOMPONENT_NAME(pSizer, "DialogSystem");
	pSizer->AddObject(m_dialogScriptMap);
	pSizer->AddObject(m_allSessions);
	pSizer->AddObject(m_activeSessions);
	pSizer->AddObject(m_pendingDeleteSessions);
	pSizer->AddObject(m_restoreSessions);
}
