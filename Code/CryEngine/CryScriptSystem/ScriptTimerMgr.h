// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

//
// ScriptTimerMgr.h: interface for the CScriptTimerMgr class.
//
//////////////////////////////////////////////////////////////////////

#if !defined(AFX_SCRIPTTIMERMGR_H__D0CF3857_AEE5_4606_ADF9_549779C7DEEC__INCLUDED_)
#define AFX_SCRIPTTIMERMGR_H__D0CF3857_AEE5_4606_ADF9_549779C7DEEC__INCLUDED_

#if _MSC_VER > 1000
	#pragma once
#endif // _MSC_VER > 1000
#include <map>

#include <CryScriptSystem/IScriptSystem.h>
#include <CrySystem/TimeValue.h>
#include <CryNetwork/ISerialize.h>

//////////////////////////////////////////////////////////////////////////
struct ScriptTimer
{
	int              nTimerID;
	int64            nMillis;
	int64            nEndTime;
	int64            nStartTime;
	char             sFuncName[128];
	HSCRIPTFUNCTION  pScriptFunction;
	SmartScriptTable pUserData;
	bool             bUpdateDuringPause;

	ScriptTimer() : nTimerID(0), nMillis(0), nStartTime(0), nEndTime(0), bUpdateDuringPause(0), pScriptFunction(0) { sFuncName[0] = 0; }
	void GetMemoryUsage(ICrySizer* pSizer) const { /*nothing*/ }
};

typedef std::map<int, ScriptTimer*> ScriptTimerMap;
typedef ScriptTimerMap::iterator    ScriptTimerMapItor;

//////////////////////////////////////////////////////////////////////////
class CScriptTimerMgr
{
public:
	CScriptTimerMgr(IScriptSystem* pScriptSystem);
	virtual ~CScriptTimerMgr();
	virtual void GetMemoryUsage(ICrySizer* pSizer) const;
	virtual void Serialize(TSerialize& ser);

	// Create a new timer and put it in the list of managed timers.
	int  AddTimer(ScriptTimer& timer);
	void RemoveTimer(int nTimerID);
	void Update(int64 nCurrentTime);
	void Reset();
	void Pause(bool bPause);
private:
	void DeleteTimer(ScriptTimer* pTimer);

	ScriptTimerMap m_mapTimers;
	ScriptTimerMap m_mapTempTimers;
	IScriptSystem* m_pScriptSystem;
	int            m_nLastTimerID;
	int            m_nTimerIDCurrentlyBeingCalled;
	bool           m_bPause;
};

#endif // !defined(AFX_SCRIPTTIMERMGR_H__D0CF3857_AEE5_4606_ADF9_549779C7DEEC__INCLUDED_)
