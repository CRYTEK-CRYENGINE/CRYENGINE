// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

//
// ScriptTimerMgr.cpp: implementation of the CScriptTimerMgr class.
//
//////////////////////////////////////////////////////////////////////

#include "StdAfx.h"
#include "ScriptTimerMgr.h"
#include <CrySystem/ITimer.h>
#include <CryEntitySystem/IEntitySystem.h>

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

CScriptTimerMgr::CScriptTimerMgr(IScriptSystem* pScriptSystem)
{
	m_nLastTimerID = 1;
	m_pScriptSystem = pScriptSystem;
	m_bPause = false;
	m_nTimerIDCurrentlyBeingCalled = -1;
}

CScriptTimerMgr::~CScriptTimerMgr()
{
	Reset();
}

//////////////////////////////////////////////////////////////////////////
void CScriptTimerMgr::DeleteTimer(ScriptTimer* pTimer)
{
	assert(pTimer);

	if (pTimer->pScriptFunction)
		m_pScriptSystem->ReleaseFunc(pTimer->pScriptFunction);
	delete pTimer;
}

//////////////////////////////////////////////////////////////////////////
// Create a new timer and put it in the list of managed timers.
int CScriptTimerMgr::AddTimer(ScriptTimer& timer)
{
	CTimeValue nCurrTimeMillis = gEnv->pTimer->GetFrameStartTime();
	timer.nStartTime = nCurrTimeMillis.GetMilliSecondsAsInt64();
	timer.nEndTime = timer.nStartTime + timer.nMillis;
	if (!timer.nTimerID)
	{
		m_nLastTimerID++;
		timer.nTimerID = m_nLastTimerID;
	}
	else
	{
		if (timer.nTimerID > m_nLastTimerID)
			m_nLastTimerID = timer.nTimerID + 1;
	}
	m_mapTempTimers[timer.nTimerID] = new ScriptTimer(timer);
	return timer.nTimerID;
}

//////////////////////////////////////////////////////////////////////////
// Delete a timer from the list.
void CScriptTimerMgr::RemoveTimer(int nTimerID)
{
	if (nTimerID == m_nTimerIDCurrentlyBeingCalled)
	{
		ScriptWarning("[CScriptTimerMgr::RemoveTimer] Trying to remove the timer (ID = %d, Lua function = %s) currently being called.",
		              nTimerID,
		              m_mapTimers.find(nTimerID)->second->sFuncName);
		return;
	}

	ScriptTimerMapItor it = m_mapTimers.find(nTimerID);

	if (it != m_mapTimers.end())
	{
		ScriptTimer* timer = it->second;
		DeleteTimer(timer);
		m_mapTimers.erase(it);
	}
	else
	{
		it = m_mapTempTimers.find(nTimerID);

		if (it != m_mapTempTimers.end())
		{
			ScriptTimer* timer = it->second;
			DeleteTimer(timer);
			m_mapTempTimers.erase(it);
		}
	}
}

//////////////////////////////////////////////////////////////////////////
void CScriptTimerMgr::Pause(bool bPause)
{
	m_bPause = bPause;
}

//////////////////////////////////////////////////////////////////////////
// Remove all timers.
void CScriptTimerMgr::Reset()
{
	m_nLastTimerID = 1;
	ScriptTimerMapItor itor;
	itor = m_mapTimers.begin();
	while (itor != m_mapTimers.end())
	{
		if (itor->second)
		{
			DeleteTimer(itor->second);
		}
		++itor;
	}
	m_mapTimers.clear();

	itor = m_mapTempTimers.begin();
	while (itor != m_mapTempTimers.end())
	{
		if (itor->second)
		{
			DeleteTimer(itor->second);
		}
		++itor;
	}
	m_mapTempTimers.clear();

}

//////////////////////////////////////////////////////////////////////////
// Update all managed timers.
void CScriptTimerMgr::Update(int64 nCurrentTime)
{
	CRY_PROFILE_FUNCTION(PROFILE_SCRIPT);

	ScriptTimerMapItor itor;

	// Loop through all timers.
	itor = m_mapTimers.begin();

	while (itor != m_mapTimers.end())
	{
		ScriptTimer* pST = itor->second;

		if (m_bPause && !pST->bUpdateDuringPause)
		{
			++itor;
			continue;
		}

		// if it is time send a timer-event
		if (nCurrentTime >= pST->nEndTime)
		{
			ScriptHandle timerIdHandle;
			timerIdHandle.n = pST->nTimerID;
			// Call function specified by the script.

			m_nTimerIDCurrentlyBeingCalled = pST->nTimerID;

			if (pST->pScriptFunction)
			{
				if (!pST->pUserData)
					Script::Call(m_pScriptSystem, pST->pScriptFunction, timerIdHandle);
				else
					Script::Call(m_pScriptSystem, pST->pScriptFunction, pST->pUserData, timerIdHandle);
			}
			else if (*pST->sFuncName)
			{
				HSCRIPTFUNCTION hFunc = 0;
				if (gEnv->pScriptSystem->GetGlobalValue(pST->sFuncName, hFunc))
				{
					if (!pST->pUserData)
						Script::Call(m_pScriptSystem, hFunc, timerIdHandle);
					else
						Script::Call(m_pScriptSystem, hFunc, pST->pUserData, timerIdHandle);
					gEnv->pScriptSystem->ReleaseFunc(hFunc);
				}
			}

			m_nTimerIDCurrentlyBeingCalled = -1;

			// After sending the event we can remove the timer.
			ScriptTimerMapItor tempItor = itor;
			++itor;
			m_mapTimers.erase(tempItor);
			DeleteTimer(pST);
		}
		else
		{
			++itor;
		}
	}
	// lets move all new created timers in the map. this is done at this point to avoid recursion, while trying to create a timer on a timer-event.
	if (!m_mapTempTimers.empty())
	{
		ScriptTimerMapItor timer_it;
		timer_it = m_mapTempTimers.begin();
		while (timer_it != m_mapTempTimers.end())
		{
			m_mapTimers.insert(ScriptTimerMapItor::value_type(timer_it->first, timer_it->second));
			++timer_it;
		}
		m_mapTempTimers.clear();
	}
}

//////////////////////////////////////////////////////////////////////////
void CScriptTimerMgr::GetMemoryUsage(ICrySizer* pSizer) const
{
	pSizer->AddObject(this, sizeof(*this));
	pSizer->AddObject(m_mapTimers);
	pSizer->AddObject(m_mapTempTimers);
}

//////////////////////////////////////////////////////////////////////////
void CScriptTimerMgr::Serialize(TSerialize& ser)
{
	if (ser.GetSerializationTarget() == eST_Network)
		return;

	ser.BeginGroup("ScriptTimers");

	if (ser.IsReading())
	{
		Reset();

		m_nLastTimerID++;

		int nTimers = 0;
		ser.Value("count", nTimers);

		for (int i = 0; i < nTimers; i++)
		{
			ScriptTimer timer;

			ser.BeginGroup("timer");
			ser.Value("id", timer.nTimerID);
			ser.Value("millis", timer.nMillis);

			string func;
			ser.Value("func", func);
			cry_strcpy(timer.sFuncName, func);

			uint32 nEntityId = 0;
			ser.Value("entity", nEntityId);
			if (nEntityId && gEnv->pEntitySystem)
			{
				IEntity* pEntity = gEnv->pEntitySystem->GetEntity(nEntityId);
				if (pEntity)
				{
					timer.pUserData = pEntity->GetScriptTable();
				}
			}
			AddTimer(timer);
			ser.EndGroup(); //timer
		}
	}
	else
	{
		// Saving.
		int nTimers = 0;
		for (ScriptTimerMap::iterator it = m_mapTimers.begin(); it != m_mapTimers.end(); ++it)
		{
			ScriptTimer* pTimer = it->second;

			if (pTimer->pScriptFunction) // Do not save timers that have script handle callback.
				continue;

			uint32 userDataEntityID = 0;

			// Save timer if there is either no user data
			// or if the user data is an entity table
			if (pTimer->pUserData)
			{
				ScriptHandle handle;

				if (pTimer->pUserData->GetValue("id", handle))
					userDataEntityID = (uint32)handle.n;
				else
				{
					gEnv->pLog->LogWarning(
					  "Attempted to save timer with function '%s'"
					  "where user data was not entity table. Skipping...",
					  pTimer->sFuncName);
					continue;
				}
			}

			ser.BeginGroup("timer");
			ser.Value("id", pTimer->nTimerID);
			ser.Value("millis", pTimer->nMillis);

			nTimers++;
			string sFuncName = pTimer->sFuncName;
			ser.Value("func", sFuncName);

			if (userDataEntityID)
				ser.Value("entity", userDataEntityID);

			ser.EndGroup(); //timer
		}
		ser.Value("count", nTimers);
	}

	ser.EndGroup(); //ScriptTimers
}
