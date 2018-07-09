// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include "DialogScript.h"
#include "DialogSession.h"
#include "DialogCommon.h"
#include <CryEntitySystem/IEntitySystem.h>
#include <IAnimationGraph.h>

#define DS_DEBUG_SUPPRESSION
// #undef  DS_DEBUG_SUPPRESSION

class CDialogActorContext :
	public _i_reference_target_t,
	public IEntityEventListener,
	public IGoalPipeListener,
	public IAnimationGraphStateListener
{
public:
	CDialogActorContext(CDialogSession* pSession, CDialogScript::TActorID actorID);
	~CDialogActorContext();

	bool                          PlayLine(const CDialogScript::SScriptLine* pLine); // returning false stops the Session!
	bool                          Update(float dt);

	void                          BeginSession();
	void                          EndSession();
	bool                          IsAborted() const;
	bool                          IsStillPlaying() const;
	CDialogSession::EAbortReason  GetAbortReason() const { return m_abortReason; }

	static const char*            FullSoundName(const string& soundName, bool bWav = true);
	static const char*            GetSoundKey(const string& soundName);

	CDialogSystem::ActorContextID GetContextID() const { return m_ContextID; }

	static void                   ResetStaticData();

	void                          GetMemoryUsage(ICrySizer* pSizer) const;

	ILINE bool                    CheckActorFlags(CDialogSession::TActorFlags flag) const
	{
		return (m_pSession->GetActorFlags(m_actorID) & flag) != 0;
	}

	static void                 OnAudioTriggerFinished(CryAudio::SRequestInfo const* const pAudioRequestInfo);
	static const char*          GetClassIdentifier() { return "DialogActorContext"; }

	static CDialogActorContext* GetDialogActorContextByAudioCallbackData(CryAudio::SRequestInfo const* const pAudioRequestInfo);
	EntityId                    GetEntityID() const { return m_entityID; }

protected:
	// Play an animation action on the entity [AG pr EP]
	bool DoAnimAction(IEntity* pEntity, const string& action, bool bIsSignal, bool bUseEP);

	// Play an animation action [AnimGraph Action/Signal] on the entity
	bool DoAnimActionAG(IEntity* pEntity, const char* sAction);

	// Play an animation action [Exact Positioning Action/Signal] on the entity
	bool DoAnimActionEP(IEntity* pEntity, const char* sAction);

	// Play a facial expression on the entity
	bool DoFacialExpression(IEntity* pEntity, const string& expression, float weight, float fadeTime);

	// Instrument pEntity to look at pLookAtEntity
	bool DoLookAt(IEntity* pEntity, IEntity* pLookAtEntity, bool& bTargetReached);

	// Handle any sticky lookat
	void DoStickyLookAt();

	// Do check wrt. local player
	bool DoLocalPlayerChecks(const float dt);

	// IEntityEventListener
	virtual void OnEntityEvent(IEntity* pEntity, const SEntityEvent& event);
	// ~IEntityEventListener

	// IGoalPipeListener
	virtual void OnGoalPipeEvent(IPipeUser* pPipeUser, EGoalPipeEvent event, int goalPipeId, bool& unregisterListenerAfterEvent);
	// ~IGoalPipeListener

	// IAnimationGraphStateListener
	virtual void SetOutput(const char* output, const char* value);
	virtual void QueryComplete(TAnimationGraphQueryID queryID, bool succeeded);
	virtual void DestroyedState(IAnimationGraphState*);
	// ~IAnimationGraphStateListener

	void                                              ResetState();
	void                                              CancelCurrent(bool bResetStates = false);
	bool                                              ExecuteAI(int& goalPipeID, const char* signalText, IAISignalExtraData* pExtraData = 0, bool bRegisterAsListener = true);
	void                                              AdvancePhase();
	void                                              StopSound(bool bUnregisterOnly = false);
	void                                              AbortContext(bool bCancel, CDialogSession::EAbortReason reason);
	void                                              ResetAGState();

	void                                              UpdateAuxProxyPosition();

	ILINE CDialogSession::EDialogAIInterruptBehaviour GetAIBehaviourMode() const
	{
		return m_pSession->GetAIBehaviourMode();
	}

	ILINE bool SessionAllowsLookAt()
	{
		const bool bAllowed = GetAIBehaviourMode() != CDialogSession::eDIB_InterruptNever;
#ifdef DS_DEBUG_SUPPRESSION
		if (!bAllowed && m_pCurLine && (m_pCurLine->m_lookatActor != CDialogScript::NO_ACTOR_ID || m_stickyLookAtActorID != CDialogScript::NO_ACTOR_ID))
		{
			DiaLOG::Log(DiaLOG::eDebugB, "[DIALOG] CDialogActorContext::Update: %s now=%f actorID=%d line=%d AIMode=%d SuppressingLookAt=%d",
			            m_pSession->GetDebugName(), m_pSession->GetCurTime(), m_actorID, m_pSession->GetCurrentLine(), (int) GetAIBehaviourMode(), m_pCurLine->m_lookatActor);
		}
#endif
		return bAllowed;
	}

	ILINE bool SessionAllowsAnim()
	{
		const bool bAllowed = GetAIBehaviourMode() != CDialogSession::eDIB_InterruptNever;
#ifdef DS_DEBUG_SUPPRESSION
		if (!bAllowed && m_pCurLine && m_pCurLine->m_anim.empty() == false)
		{
			DiaLOG::Log(DiaLOG::eDebugB, "[DIALOG] CDialogActorContext::Update: %s now=%f actorID=%d line=%d AIMode=%d SuppressingAnim='%s'",
			            m_pSession->GetDebugName(), m_pSession->GetCurTime(), m_actorID, m_pSession->GetCurrentLine(), (int) GetAIBehaviourMode(), m_pCurLine->m_anim.c_str());
		}
#endif
		return bAllowed;
	}

protected:
	enum EDialogActorContextPhase
	{
		eDAC_Idle = 0,
		eDAC_NewLine,
		eDAC_LookAt,
		eDAC_Anim,
		eDAC_ScheduleSoundPlay,  // Schedule sound to play and wait until it really starts
		eDAC_SoundFacial,
		eDAC_EndLine,
		eDAC_Aborted
	};

	// TODO: Currently logically grouped. Needs proper space-saving reordering
	CDialogSession*                   m_pSession;
	CDialogScript::TActorID           m_actorID;
	EntityId                          m_entityID;
	const CDialogScript::SScriptLine* m_pCurLine;
	CDialogSystem::ActorContextID     m_ContextID;

	IActor*                           m_pIActor;

	int                               m_phase;
	bool                              m_bHasScheduled;
	bool                              m_bNeedsCancel;
	bool                              m_bInCancel;
	bool                              m_bIsLocalPlayer; // Whether it's the local player
	bool                              m_bIsAware;
	bool                              m_bIsAwareLooking;
	bool                              m_bIsAwareInRange;
	float                             m_playerAwareTimeOut;
	float                             m_checkPlayerTimeOut;
	CDialogSession::EAbortReason      m_abortReason;

	CryAudio::AuxObjectId             m_SpeechAuxObject;

	// Animation specific
	IAnimationGraphState*  m_pAGState;
	TAnimationGraphQueryID m_queryID;
	bool                   m_bAnimStarted;
	bool                   m_bAnimScheduled;
	bool                   m_bAnimUseAGSignal;
	bool                   m_bAnimUseEP;
	bool                   m_bSoundStopsAnim;

	// AI
	bool m_bAbortFromAI;
	int  m_goalPipeID;
	int  m_exPosAnimPipeID;

	// Sound specific
	bool  m_bSoundScheduled;
	bool  m_bSoundStarted;
	float m_soundTimeOut;
	float m_soundLength;

	int   m_VoiceAttachmentIndex;
	int   m_BoneHeadJointID;

	// LookAt specific
	CDialogScript::TActorID m_lookAtActorID;
	CDialogScript::TActorID m_stickyLookAtActorID;
	float                   m_lookAtTimeOut;
	float                   m_animTimeOut;
	bool                    m_bLookAtNeedsReset;

	// Facial Expression
	uint32        m_currentEffectorChannelID;

	static string m_tempBuffer;
};
