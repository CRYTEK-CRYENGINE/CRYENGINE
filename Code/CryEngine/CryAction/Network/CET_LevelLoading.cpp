// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "CET_LevelLoading.h"
#include "ILevelSystem.h"
#include "GameClientChannel.h"
#include <CryNetwork/NetHelpers.h>
#include "GameContext.h"
#include "GameContext.h"
#include <CryThreading/IThreadManager.h>

/*
 * Prepare level
 */

class CCET_PrepareLevel : public CCET_Base
{
public:
	const char*                 GetName() { return "PrepareLevel"; }

	EContextEstablishTaskResult OnStep(SContextEstablishState& state)
	{
		CGameContext* pGameContext = CCryAction::GetCryAction()->GetGameContext();
		string levelName = pGameContext ? pGameContext->GetLevelName() : "";
		if (levelName.empty())
		{
			//GameWarning("No level name set");
			return eCETR_Wait;
		}
		if (gEnv->IsClient() && !gEnv->bServer)
		{
			CryLogAlways("============================ PrepareLevel %s ============================", levelName.c_str());

			gEnv->pSystem->GetISystemEventDispatcher()->OnSystemEvent(ESYSTEM_EVENT_LEVEL_LOAD_PREPARE, 0, 0);

			CCryAction::GetCryAction()->GetILevelSystem()->PrepareNextLevel(levelName);
		}
		return eCETR_Ok;
	}
};

void AddPrepareLevelLoad(IContextEstablisher* pEst, EContextViewState state)
{
	pEst->AddTask(state, new CCET_PrepareLevel());
}

/*
 * Load a level
 */

class CCET_LoadLevel : public CCET_Base
{
public:
	CCET_LoadLevel() : m_bStarted(false) {}

	const char*                 GetName() { return "LoadLevel"; }

	EContextEstablishTaskResult OnStep(SContextEstablishState& state)
	{
		ILevelInfo* pILevel = NULL;
		string levelName = CCryAction::GetCryAction()->GetGameContext()->GetLevelName();
		if (levelName.empty())
		{
			GameWarning("No level name set");
			return eCETR_Failed;
		}

		CCryAction* pAction = CCryAction::GetCryAction();

		pAction->StartNetworkStallTicker(true);
		GetISystem()->GetISystemEventDispatcher()->OnSystemEvent(ESYSTEM_EVENT_LEVEL_LOAD_START, (UINT_PTR)(levelName.c_str()), 0);
		pILevel = pAction->GetILevelSystem()->LoadLevel(levelName);
		GetISystem()->GetISystemEventDispatcher()->OnSystemEvent(ESYSTEM_EVENT_LEVEL_LOAD_END, 0, 0);
		pAction->StopNetworkStallTicker();

		if (pILevel == NULL)
		{
			GameWarning("Failed to load level: %s", levelName.c_str());
			return eCETR_Failed;
		}
		m_bStarted = true;
		return eCETR_Ok;
	}

	bool m_bStarted;
};

class CLevelLoadingThread : public IThread
{
public:
	enum EState
	{
		eState_Working,
		eState_Failed,
		eState_Succeeded,
	};

	CLevelLoadingThread(ILevelSystem* pLevelSystem, const char* szLevelName) 
		: m_pLevelSystem(pLevelSystem), m_state(eState_Working), m_levelName(szLevelName) 
	{
	}

	virtual void ThreadEntry() override
	{
		const threadID levelLoadingThreadId = CryGetCurrentThreadId();

		if (gEnv->pRenderer) //Renderer may be unavailable when turned off
		{
			gEnv->pRenderer->SetLevelLoadingThreadId(levelLoadingThreadId);
		}

		const ILevelInfo* pLoadedLevelInfo = m_pLevelSystem->LoadLevel(m_levelName.c_str());
		const bool bResult = (pLoadedLevelInfo != NULL);

		if (gEnv->pRenderer) //Renderer may be unavailable when turned off
		{
			gEnv->pRenderer->SetLevelLoadingThreadId(0);
		}

		m_state = bResult ? eState_Succeeded : eState_Failed;
	}

	EState GetState() const
	{
		return m_state;
	}

private:
	ILevelSystem* m_pLevelSystem;
	volatile EState m_state;
	const string m_levelName;
};

class CCET_LoadLevelAsync : public CCET_Base
{
public:
	CCET_LoadLevelAsync() : m_bStarted(false), m_pLevelLoadingThread(nullptr) {}

	virtual const char* GetName() override 
	{ 
		return "LoadLevelAsync"; 
	}

	virtual EContextEstablishTaskResult OnStep( SContextEstablishState& state ) override
	{
		CCryAction* pAction = CCryAction::GetCryAction();
		const string levelName = pAction->GetGameContext()->GetLevelName();
		if (!m_bStarted)
		{
			if (levelName.empty())
			{
				GameWarning("No level name set");
				return eCETR_Failed;
			} 

			pAction->StartNetworkStallTicker(true);
			GetISystem()->GetISystemEventDispatcher()->OnSystemEvent(ESYSTEM_EVENT_LEVEL_LOAD_START, (UINT_PTR)(levelName.c_str()), 0);

			ILevelSystem* pLevelSystem = pAction->GetILevelSystem();
			m_pLevelLoadingThread = new CLevelLoadingThread(pLevelSystem, levelName.c_str());
			if (!gEnv->pThreadManager->SpawnThread(m_pLevelLoadingThread, "LevelLoadingThread"))
			{
				CryFatalError("Error spawning LevelLoadingThread thread.");
			}

			m_bStarted = true;
		}

		const CLevelLoadingThread::EState threadState = m_pLevelLoadingThread->GetState();
		if(threadState == CLevelLoadingThread::eState_Working)
			return eCETR_Wait;

		gEnv->pThreadManager->JoinThread(m_pLevelLoadingThread, eJM_Join);
		delete m_pLevelLoadingThread;
		m_pLevelLoadingThread = nullptr;

		GetISystem()->GetISystemEventDispatcher()->OnSystemEvent(ESYSTEM_EVENT_LEVEL_LOAD_END, 0, 0);
		pAction->StopNetworkStallTicker();
		if(threadState == CLevelLoadingThread::eState_Succeeded)
		{
			return eCETR_Ok;
		}
		else
		{
			GameWarning("Failed to load level: %s", levelName.c_str());
			return eCETR_Failed;
		}
	}

	bool m_bStarted;
	CLevelLoadingThread* m_pLevelLoadingThread;
};

void AddLoadLevel( IContextEstablisher * pEst, EContextViewState state, bool ** ppStarted )
{
	const bool bIsEditor = gEnv->IsEditor();
	const bool bIsDedicated = gEnv->IsDedicated();
	const ICVar* pAsyncLoad = gEnv->pConsole->GetCVar("g_asynclevelload");
	const bool bAsyncLoad = pAsyncLoad && pAsyncLoad->GetIVal() > 0;

	if(!bIsEditor && !bIsDedicated && bAsyncLoad)
	{
		CCET_LoadLevelAsync* pLL = new CCET_LoadLevelAsync;
		pEst->AddTask( state, pLL );
		*ppStarted = &pLL->m_bStarted;
	}
	else
	{
		CCET_LoadLevel* pLL = new CCET_LoadLevel;
		pEst->AddTask( state, pLL );
		*ppStarted = &pLL->m_bStarted;
	}
}

/*
 * Loading complete
 */

class CCET_LoadingComplete : public CCET_Base
{
public:
	CCET_LoadingComplete(bool* pStarted) : m_pStarted(pStarted) {}

	const char*                 GetName() { return "LoadingComplete"; }

	EContextEstablishTaskResult OnStep(SContextEstablishState& state)
	{
		ILevelSystem* pLS = CCryAction::GetCryAction()->GetILevelSystem();
		if (pLS->GetCurrentLevel())
		{
			pLS->OnLoadingComplete(pLS->GetCurrentLevel());
			return eCETR_Ok;
		}
		return eCETR_Failed;
	}
	virtual void OnFailLoading(bool hasEntered)
	{
		ILevelSystem* pLS = CCryAction::GetCryAction()->GetILevelSystem();
		if (m_pStarted && *m_pStarted && !hasEntered)
			pLS->OnLoadingError(NULL, "context establisher failed");
	}

	bool* m_pStarted;
};

void AddLoadingComplete(IContextEstablisher* pEst, EContextViewState state, bool* pStarted)
{
	pEst->AddTask(state, new CCET_LoadingComplete(pStarted));
}

/*
 * Reset Areas
 */
class CCET_ResetAreas : public CCET_Base
{
public:
	const char*                 GetName() { return "ResetAreas"; }

	EContextEstablishTaskResult OnStep(SContextEstablishState& state)
	{
		gEnv->pEntitySystem->ResetAreas();
		return eCETR_Ok;
	}
};

void AddResetAreas(IContextEstablisher* pEst, EContextViewState state)
{
	pEst->AddTask(state, new CCET_ResetAreas);
}

/*
 * Action events
 */
class CCET_ActionEvent : public CCET_Base
{
public:
	CCET_ActionEvent(const SActionEvent& evt) : m_evt(evt) {}

	const char*                 GetName() { return "ActionEvent"; }

	EContextEstablishTaskResult OnStep(SContextEstablishState& state)
	{
		CCryAction::GetCryAction()->OnActionEvent(m_evt);
		return eCETR_Ok;
	}

private:
	SActionEvent m_evt;
};

void AddActionEvent(IContextEstablisher* pEst, EContextViewState state, const SActionEvent& evt)
{
	pEst->AddTask(state, new CCET_ActionEvent(evt));
}
