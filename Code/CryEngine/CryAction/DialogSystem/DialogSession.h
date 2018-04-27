// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

// -------------------------------------------------------------------------
//  File name:   DialogSession.h
//  Version:     v1.00
//  Created:     07/07/2006 by AlexL
//  Compilers:   Visual Studio.NET
//  Description: Dialog System
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////

#ifndef __DIALOGSESSION_H__
#define __DIALOGSESSION_H__

#pragma once

#include "DialogScript.h"
#include "DialogSystem.h"
#include <CryNetwork/SerializeFwd.h>

struct IDialogSessionListener;
class CDialogActorContext;

// A Dialog Session uses a DialogScript together with dialog parameters
// to play the script
class CDialogSession
{
public:
	typedef _smart_ptr<CDialogActorContext>                           CDialogActorContextPtr;
	typedef std::map<CDialogScript::TActorID, EntityId>               TIdToEntityMap;
	typedef std::vector<IDialogSessionListener*>                      TListenerVec;
	typedef std::map<CDialogScript::TActorID, CDialogActorContextPtr> TActorContextMap;
	typedef uint16                                                    TActorFlags;

	enum EDialogSessionEvent
	{
		eDSE_SessionStart = 0, // Session is about to be started
		eDSE_EndOfDialog,      // Session stopped normally
		eDSE_UserStopped,      // Session stopped on user request
		eDSE_Aborted,          // Session aborted
		eDSE_SessionDeleted,   // Session is Deleted
		eDSE_LineStarted       // Called when a new line starts to play
	};

	enum EAbortReason
	{
		eAR_None = 0,          // Not aborted
		eAR_ActorDead,         // Actor died during dialog
		eAR_AIAborted,         // AI aborted it
		eAR_PlayerOutOfRange,  // Player was not aware
		eAR_PlayerOutOfView,   // Player out of view
		eAR_EntityDestroyed    // Entity was destroyed/removed
	};

	enum EDialogActorContextFlags
	{
		eDACF_None             = 0x0000,
		eDACF_NoAIAbort        = 0x0001,  // Don't abort when AI selects different goal pipe
		eDACF_NoAbortSound     = 0x0002,  // Don't abort sound when AI/Player aborts. But if actor dies, sound is aborted anyway!
		eDACF_NoActorDeadAbort = 0x0004,  // Don't abort when an Actor is dead

		eDACF_Default          = eDACF_None
	};

	enum EDialogAIInterruptBehaviour
	{
		eDIB_InterruptAlways = 0,     // ACT_ANIM signal and interrupt by goal-pipe deselection
		eDIB_InterruptMedium = 1,     // ACT_DIALOG signal and interrupt by goal-pipe deselection
		eDIB_InterruptNever  = 2,     // no signal, and no animation/lookat possible
	};

	enum AlertnessInterruptMode
	{
		None   = 0,
		Alert  = 1,
		Combat = 2,
	};

public:
	bool SetActor(CDialogScript::TActorID actorID, EntityId id);

	// Call this after all params have been setup. Will be called by Play as well.
	bool Validate();

	// All params o.k.
	bool IsOK() const { return m_bOK; }

	// Play the Session [advanced usage can
	bool Play(int fromScriptLine = 0);

	// Stop the Session
	bool Stop();

	// Delete the Session when stopped manually, aborted, ...
	void SetAutoDelete(bool bAutoDelete);

	void SetPlayerAwarenessDistance(float distance)
	{
		m_playerAwareDistance = distance;
	}

	void SetPlayerAwarenessAngle(float angleDeg)
	{
		m_playerAwareAngle = angleDeg;
	}

	void GetPlayerAwarenessValues(float& distance, float& angleDeg) const
	{
		distance = m_playerAwareDistance;
		angleDeg = m_playerAwareAngle;
	}

	float GetPlayerAwarenessGraceTime() const
	{
		return m_playerAwareGraceTime;
	}

	void SetPlayerAwarenessGraceTime(float graceTime)
	{
		m_playerAwareGraceTime = graceTime;
	}

	void        SetActorFlags(CDialogScript::TActorID actorID, TActorFlags inFlags);
	TActorFlags GetActorFlags(CDialogScript::TActorID actorID) const;

	void        SetAIBehaviourMode(EDialogAIInterruptBehaviour mode)
	{
		assert(m_bPlaying == false);
		if (m_bPlaying)
			return;
		m_aiBehaviourMode = mode;
	}

	EDialogAIInterruptBehaviour GetAIBehaviourMode() const
	{
		return (EDialogAIInterruptBehaviour) m_aiBehaviourMode;
	}

	int                      GetCurrentLine() const     { return m_curScriptLine; }
	CDialogScript::SActorSet GetCurrentActorSet() const { return m_actorSet; }
	const CDialogScript*     GetScript() const          { return m_pScript; }

	const char*              GetEventName(EDialogSessionEvent event);

	bool                     AddListener(IDialogSessionListener* pListener);
	bool                     RemoveListener(IDialogSessionListener* pListener);

	CDialogSystem::SessionID GetSessionID() const   { return m_sessionID; }
	EAbortReason             GetAbortReason() const { return m_abortReason; }
	EAbortReason             GetAbortReasonForActor(CDialogScript::TActorID actorID) const;

	// used by CDialogActorContext
	CDialogActorContextPtr  GetContext(CDialogScript::TActorID actorID) const;
	CDialogActorContextPtr  GetContext(CDialogSystem::ActorContextID contextID) const;
	const TActorContextMap& GetAllContexts() const { return m_actorContextMap; }

	IEntity*                GetActorEntity(CDialogScript::TActorID actorID) const;
	EntityId                GetActorEntityId(CDialogScript::TActorID actorID) const;
	CDialogScript::TActorID GetActorIdForEntity(EntityId entityId) const;
	IEntityAudioComponent*      GetEntityAudioProxy(IEntity* pEntity) const;
	int                     ScheduleNextLine(float dt); // schedule next line to be played. returns index of line
	float                   GetCurTime() const
	{
		return m_curTime;
	}
	const char* GetDebugName() const { return m_debugName.c_str(); }

	void        GetMemoryUsage(ICrySizer* pSizer) const;

	void        SetAlertnessInterruptMode(AlertnessInterruptMode alertnessInterruptMode)
	{
		m_alertnessInterruptMode = alertnessInterruptMode;
	}

	AlertnessInterruptMode GetAlertnessInterruptMode() const
	{
		return m_alertnessInterruptMode;
	}

protected:
	friend class CDialogSystem;

	CDialogSession(CDialogSystem* pDS, const CDialogScript* pScript, CDialogSystem::SessionID id);
	~CDialogSession();

	// Deletes this session. Called from CDialogSystem
	void Release();

	// Called from CDialogSystem to update every frame
	bool Update(float dt);

	// Called from CDialogSystem after serialization
	bool RestoreAndPlay();

	bool InternalPlay(int fromScriptLine, bool bNotify);

	void DoPlay(float dt);
	bool PlayLine(const CDialogScript::SScriptLine* pLine);

	bool DoStop();

	void NotifyListeners(EDialogSessionEvent event);

	void Serialize(TSerialize ser);
protected:
	IEntitySystem*           m_pEntitySystem;
	CDialogSystem*           m_pDS;
	const CDialogScript*     m_pScript;
	TIdToEntityMap           m_idToEntityMap;
	TListenerVec             m_listenerVec;
	TListenerVec             m_listenerVecTemp;
	TActorContextMap         m_actorContextMap;
	CDialogSystem::SessionID m_sessionID;
	string                   m_debugName;
	CDialogScript::SActorSet m_actorSet;

	float                    m_curTime;
	float                    m_nextTimeDelay;
	float                    m_endGraceTimeOut;
	int                      m_curScriptLine;
	int                      m_nextScriptLine;
	int                      m_pendingActors;
	EAbortReason             m_abortReason;

	float                    m_playerAwareAngle;
	float                    m_playerAwareDistance;
	float                    m_playerAwareGraceTime;

	TActorFlags              m_actorFlags[CDialogScript::MAX_ACTORS];
	int8                     m_aiBehaviourMode;

	uint16                   m_bPlaying      : 1;  // currently playing
	uint16                   m_bValidated    : 1;  // session is validated (does not mean ok, though)
	uint16                   m_bOK           : 1;  // session validated & ok
	uint16                   m_bHaveSchedule : 1;  // new line scheduled
	uint16                   m_bAutoDelete   : 1;  // auto delete session when stopped/aborted: false by default
	uint16                   m_bReachedEnd   : 1;  // whether dialog has reached end [may still wait for pending contexts]
	uint16                   m_bFirstUpdate  : 1;  // first update done

	AlertnessInterruptMode   m_alertnessInterruptMode;
};

struct IDialogSessionListener
{
	virtual ~IDialogSessionListener(){}
	virtual void SessionEvent(CDialogSession* pSession, CDialogSession::EDialogSessionEvent event) = 0;
	void         GetMemoryUsage(ICrySizer* pSizer) const {}
};

#endif
