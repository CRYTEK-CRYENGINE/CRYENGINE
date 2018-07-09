// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

/*************************************************************************
-------------------------------------------------------------------------
Description: 

-------------------------------------------------------------------------
History:
- 6:11:2009	16:35 : Created by David Ramos
*************************************************************************/
#pragma once
#ifndef __HIT_DEATH_REACTIONS_H
#define __HIT_DEATH_REACTIONS_H

#include "HitDeathReactionsDefs.h"

#include "Actor.h"										// CActor::KillParams
#include <IGameRulesSystem.h>					// HitInfo
#include "PlayerAnimation.h"

class CActor;
struct ICharacterInstance;
struct ExplosionInfo;
struct EventPhysCollision;
class IAnimationDatabase;

class CHitDeathReactions;

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
class CHitDeathReactionsPhysics
{
private:
	struct SPrimitiveRequest
	{
		SPrimitiveRequest() : queuedId(0), counter(0)	{}
		
		void Reset();

		QueuedIntersectionID	queuedId;
		uint32								counter;
	};

public:
	CHitDeathReactionsPhysics();
	~CHitDeathReactionsPhysics();

	void InitWithOwner(CHitDeathReactions* pOwner);

	void Queue(int primitiveType, primitives::primitive &primitive, const Vec3 &sweepDir, int objTypes, int geomFlags);
	void IntersectionTestComplete(const QueuedIntersectionID& intID, const IntersectionTestResult& result);

private:
	enum { kMaxQueuedPrimitives = 4 };


	int GetPrimitiveRequestSlot();
	int GetSlotFromPrimitiveId(const QueuedIntersectionID& intID);


	SPrimitiveRequest		m_queuedPrimitives[kMaxQueuedPrimitives];
	uint32							m_requestCounter;

	CHitDeathReactions* m_pOwnerHitReactions;
};

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
class CHitDeathReactions
{
private:
	friend class CHitDeathReactionsPhysics;

	// Private typedefs
	struct SCustomAnim
	{
		SCustomAnim() : iLayer(0), fOverrideTransTimeToAG(-1) {}

		void 								Invalidate() { sAnimName.clear(); }  
		bool 								IsValid() const { return !sAnimName.empty(); }
		void 								SetAnimName(int animID, IAnimationSet* pAnimSet);
		ILINE const string& GetAnimName() const { return sAnimName; }

		int											iLayer;
		float										fOverrideTransTimeToAG;

	private:
		string									sAnimName;
	};

	enum EExecutionType
	{
		eET_None = -1,

		eET_ReactionAnim,
		eET_AnimationGraphAnim,
		eET_Mannequin,

		eET_Custom,
	};

	enum EAIPausedState
	{
		eAIP_NotPaused,
		eAIP_ExecutingCustomSignal,		// Current reaction has sent a custom signal to the AI for behavior processing
		eAIP_PipeUserPaused,			// Pipe user is paused as a result of the current reaction
	};

public:
	// Public methods
	CHitDeathReactions(CActor& actor);

	enum EReactionType
	{
		eRT_None = -1,

		eRT_Hit,
		eRT_Death,
		eRT_Action,
		eRT_Collision
	};
	void							Update(float fFrameTime);

	// Events
	bool 							OnKill(const CActor::KillParams& killParams);
	bool 							OnHit(const HitInfo& hitInfo, float fCausedDamage = 0.0f);
	bool							OnReaction(const HitInfo& hitInfo, int* pReturnedAnimIndex);
	bool 							OnAnimationEvent(const AnimEventInstance &event);
	bool 							HandleEvent(const SGameObjectEvent& event);
	void 							OnRevive();
	void 							OnActorReused();
	void							OnActorReturned();

	void							FullSerialize(TSerialize ser);	
	void							PostSerialize();

	void							RequestReactionAnims(uint32 requestFlags);
	void							ReleaseReactionAnims(uint32 requestFlags);

	void 							Reload();
	void 							LogUsedAnimations(bool bLog);

	bool 							StartHitReaction(const HitInfo& hitInfo, const SReactionParams& reactionParams);
	bool 							StartDeathReaction(const HitInfo& hitInfo, const SReactionParams& reactionParams);
	bool 							StartReaction(EReactionType reactionType, const HitInfo& hitInfo, const SReactionParams& reactionParams);
	bool							StartCollisionReaction(const HitInfo& hitInfo, const SReactionParams& reactionParams);

	bool 							IsValidReaction(const HitInfo& hitInfo, const SReactionParams::SValidationParams& validationParams, float fCausedDamage) const;
	bool 							ExecuteHitReaction(const SReactionParams& reactionParams);
	bool 							ExecuteDeathReaction(const SReactionParams& reactionParams);

	// Suppress passedByValue for smart pointers like ScriptTablePtr
	// cppcheck-suppress passedByValue
	bool 							IsValidReaction(const HitInfo& hitInfo, const ScriptTablePtr pScriptTable, float fCausedDamage) const;
	// cppcheck-suppress passedByValue
	bool 							ExecuteHitReaction(const ScriptTablePtr pScriptTable);
	// cppcheck-suppress passedByValue
	bool 							ExecuteDeathReaction(const ScriptTablePtr pScriptTable);

	bool 							EndCurrentReaction();

	void OnRagdollize( bool forceFallback );

	ILINE bool				IsInDeathReaction() const { return m_currentReaction == eRT_Death; }
	ILINE bool				IsInAction() const { return m_currentReaction == eRT_Action; }
	ILINE bool				IsInHitReaction() const { return m_currentReaction == eRT_Hit; }
	ILINE bool				IsInReaction() const { return m_currentReaction!=eRT_None; }

	ILINE bool				AreReactionsForbidden() const { return (m_reactionFlags & SReactionParams::ReactionsForbidden) != 0; }

	ILINE ProfileId		GetProfileId() const { return m_profileId; }

	// FIXME: Currently used to prevent AI from moving while in a hit reaction - To be replaced by better method.
	ILINE bool				CanActorMove() const { return (eAIP_NotPaused == m_AIPausedState); }

	// "Reaction Anim" utils. This methods allows playing animations directly, without the need of a dedicated 
	// animation graph state setup. Designed (and enforced) to be used _only_ during hit or death reactions
	bool 							StartReactionAnim(const string& sAnimName, bool bLoop = false, float fBlendTime = 0.2f, int iSlot = 0, int iLayer = 0, uint32 animFlags = (DEFAULT_REACTION_ANIM_FLAGS), float fAdditiveWeight = 0.0f, float fAniSpeed = 1.0f, bool bNoAnimCamera = false);
	bool 							StartReactionAnimByID(int animID, bool bLoop = false, float fBlendTime = 0.2f, int iSlot = 0, int iLayer = 0, uint32 animFlags = (DEFAULT_REACTION_ANIM_FLAGS), float fAdditiveWeight = 0.0f, float fAniSpeed = 1.0f, bool bNoAnimCamera = false);
	void 							EndReactionAnim();
	bool 							IsPlayingReactionAnim() const;
	bool							IsPlayingMannequinReactionAnim() const { return m_currentExecutionType ==  eET_Mannequin; }
	static FragmentID	GetHitDeathFragmentID( const IActionController* piActionController );

	// mem tracking and profiling
	void 							GetMemoryUsage(ICrySizer *pSizer ) const;

private:
	// Private predicates
	struct SPredFindValidReaction;

	// Private methods
	bool										OnKill(const HitInfo& hitInfo);

	void										ClearState();

	bool 										CanPlayHitReaction() const;
	bool 										CanPlayDeathReaction() const;
	ILINE bool							DeathReactionEndsInRagdoll() const { return (!g_pGameCVars->g_hitDeathReactions_disableRagdoll && !(m_reactionFlags & SReactionParams::NoRagdollOnEnd)); }

	void 										SetInReaction(EReactionType reactionType);
	ILINE void 							SetExecutionType(EExecutionType executionType) { m_currentExecutionType = executionType; }
	ILINE bool 							IsExecutionType(EExecutionType executionType) const { return (m_currentExecutionType == executionType); }
	bool										EndCurrentReactionInternal(bool bForceRagdollOnHit, bool bFinished, bool bForceFinishMannequinAction=true);

	void 										SetVariations(const SReactionParams::SAnimGraphReaction::VariationsContainer& variations) const;
	bool 										ExecuteReactionCommon(const SReactionParams& reactionParams);
	void 										PausePipeUser(bool bPause);

	void 										LoadData(SmartScriptTable pSelfTable, bool bForceReload);

	void 										OnCustomAnimFinished(bool bInterrupted);

	void										EnableFirstPersonAnimation(bool bEnable);

	void										DoCollisionCheck();
	void 										StartCollisionReaction(const Vec3& vNormal, const Vec3& vPoint);

	ECardinalDirection			GetRelativeCardinalDirection2D(const Vec2& vDir1, const Vec2& vDir2) const;
	bool										CheckCardinalDirection2D(ECardinalDirection direction, const Vec2& vDir1, const Vec2& vDir2) const;

	bool 										CustomAnimHasFinished() const;
	void 										StopHigherLayers(int iSlot, int iLayer, float fBlendOut);

	uint32									GetSynchedSeed(bool bKillReaction) const;
	float										GetRandomProbability() const;

	bool										IsAI() const;
	IPipeUser*							GetAIPipeUser() const;
	void										HandleAIStartReaction(const HitInfo& hitInfo, const SReactionParams& reactionParams);
	void										HandleAIEndReaction();
	void										HandleAIReactionInterrupted();
	bool										SendAISignal(const char* szSignal, IAISignalExtraData *pData = NULL, bool bWaitOpOnly = false) const;

#ifndef _RELEASE
	void 										DrawDebugInfo();
#endif

	bool										IsValidReactionId(ReactionId reactionId) const;
	const SReactionParams&	GetReactionParamsById(ReactionId reactionId) const;
	string									GetCurrentAGReactionAnimName() const;

	bool										StartFacialAnimation(ICharacterInstance* pCharacter, const char* szEffectorName, float fWeight = 1.0f, float fFadeTime = 0.1f);
	void										StopFacialAnimation(ICharacterInstance* pCharacter, float fFadeOutTime = 0.1f);

	ILINE bool							DoesAnimEventWantToSleepRagdoll(const AnimEventInstance &event) const { return (event.m_CustomParameter && (strcmp("sleep", event.m_CustomParameter) == 0)); }
	void										SleepRagdoll();

	TagState								MergeCurrentFragTagState( const IActionController* piActionController, const FragmentID fragID, const TagState hitDeathFragTagState) const; // adds the current fragtag state onto hitDeathFragTagState
	uint32									GetRootScopeID( const IActionController* piActionController, const FragmentID fragID, const TagState hitDeathFragTagState ) const;
	void										GeneratePostDeathTags();

	// Private attributes
	IScriptSystem*									m_pScriptSystem;

	ReactionsContainerConstPtr			m_pDeathReactions;
	ReactionsContainerConstPtr			m_pHitReactions;
	ReactionsContainerConstPtr			m_pCollisionReactions;

	SHitDeathReactionsConfigConstPtr	m_pHitDeathReactionsConfig;

	ProfileId												m_profileId; // Cached profile id

	ScriptTablePtr									m_pSelfTable;

	CActor&													m_actor;

	CMTRand_int32&									m_pseudoRandom;

	float														m_fReactionCounter; // holds time since last reaction triggering. Sanity check purpose
	float														m_fReactionEndTime; // time to switch to ragdoll
	int															m_iGoalPipeId; // holds the Id of the goalpipe used by the actor
	EAIPausedState									m_AIPausedState;

	EExecutionType									m_currentExecutionType;
	SCustomAnim											m_currentCustomAnim;

	EReactionType										m_currentReaction;

	const HitInfo*									m_pHitInfo;
	HitInfo													m_lastHitInfo;

	CHitDeathReactionsPhysics				m_primitiveIntersectionQueue;

	uint32													m_effectorChannel;
	TagState												m_postDeathTagRagdoll;

	const SReactionParams*					m_pCurrentReactionParams;	// This will point to the current reaction params, if a reload is called, this pointer will be set to NULL (since that will probably change its address)

	_smart_ptr<TPlayerAction>				m_pAction;

	bool														m_bInSmartObject;

	unsigned char										m_reactionOnCollision;
	uint8														m_reactionFlags;
};

#endif // __HIT_DEATH_REACTIONS
