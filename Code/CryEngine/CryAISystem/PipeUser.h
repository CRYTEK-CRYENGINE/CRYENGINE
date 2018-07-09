// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

/********************************************************************
   -------------------------------------------------------------------------
   File name:   PipeUser.h
   $Id$
   Description:

   -------------------------------------------------------------------------
   History:
   -

 *********************************************************************/
#ifndef _PIPE_USER_
#define _PIPE_USER_

#if _MSC_VER > 1000
	#pragma once
#endif

// created by Petar

#include "GoalPipe.h"
#include "AIActor.h"
#include <CryAISystem/IAISystem.h>
#include "AIHideObject.h"
#include "ObjectContainer.h"
#include "Cover/CoverUser.h"
#include "MNMPathfinder.h"
#include <CryAISystem/IMovementSystem.h>
#include "PipeUserMovementActorAdapter.h"

enum EPathfinderResult
{
	PATHFINDER_STILLFINDING,
	PATHFINDER_PATHFOUND,
	PATHFINDER_NOPATH,
	PATHFINDER_ABORT,
	PATHFINDER_MAXVALUE
};

struct PathFollowerParams;

enum EAimState
{
	AI_AIM_NONE,        // No aiming requested
	AI_AIM_WAITING,     // Aiming requested, but not yet ready.
	AI_AIM_OBSTRUCTED,  // Aiming obstructed.
	AI_AIM_READY,
	AI_AIM_FORCED,
};

class CGoalPipe;
class MovementStyle;

class CPipeUser
	: public CAIActor
	  , public CPipeUserAdapter
{
	friend class CAISystem;

public:
	CPipeUser();
	virtual ~CPipeUser();

	virtual const IPipeUser* CastToIPipeUser() const override { return this; }
	virtual IPipeUser*       CastToIPipeUser() override       { return this; }

	virtual void             Event(unsigned short int eType, SAIEVENT* pAIEvent) override;
	virtual void             ParseParameters(const AIObjectParams& params, bool bParseMovementParams = true) override;

	virtual void             RecordEvent(IAIRecordable::e_AIDbgEvent event, const IAIRecordable::RecorderEventData* pEventData = NULL) override;
	virtual void             RecordSnapshot() override;

	virtual void             Reset(EObjectResetType type) override;
	virtual void             SetName(const char* pName) override;
	void                     GetStateFromActiveGoals(SOBJECTSTATE& state);
	CGoalPipe*               GetGoalPipe(const char* name);
	void                     RemoveActiveGoal(int nOrder);

	virtual void             SetAttentionTarget(CWeakRef<CAIObject> refTarget) override;

	virtual void             ClearPotentialTargets() {};
	void                     SetLastOpResult(CWeakRef<CAIObject> refObject);

	/// fUrgency would normally be a value from AISPEED_WALK and similar. This returns the "normal" movement
	/// speed associated with each for the current stance - as a guideline for the AI system to use in its requests.
	/// For vehicles the urgency is irrelevant.
	/// if slowForStrafe is set then the returned speed depends on last the body/move direction
	//  virtual float GetNormalMovementSpeed(float fUrgency, bool slowForStrafe) const {return 0.0f;}
	/// Returns the speed (as GetNormalMovementSpeeds) used to do accurate maneuvering/tight turning.
	//  virtual float GetManeuverMovementSpeed() {return 0.0f;}

	/// Steers the puppet outdoors and makes it avoid the immediate obstacles (or slow down). This finds all the potential
	/// AI objects that must be navigated around, and calls NavigateAroundAIObject on each.
	/// targetPos is a reasonably stable position ahead on the path that should be steered towards.
	/// fullUpdate indicates if this is called from a full or "dry" update
	/// Returns true if some steering needed to be done
	virtual bool NavigateAroundObjects(const Vec3& targetPos, bool fullUpdate) { return false; }

	virtual void CancelRequestedPath(bool actorRemoved) override;

	virtual void HandlePathDecision(MNMPathRequestResult& result) override;
	void         AdjustPath();

	//////////////////////////////////////////////////////////////////////////
	/// MNM Path handling
	void OnMNMPathResult(const MNM::QueuedPathID& requestId, MNMPathRequestResult& result);

	/// Adjust the path to plan "local" steering. Returns true if the resulting path can be used, false otherwise
	bool AdjustPathAroundObstacles();

	/// returns the path follower if it exists (it may be created if appropriate). May return 0
	class IPathFollower* GetPathFollower();

	/// returns the current path follower. Will never create one. May return 0.
	virtual class IPathFollower* GetPathFollower() const override;

	virtual void                 GetPathFollowerParams(struct PathFollowerParams& outParams) const;
	EntityId                     GetPendingSmartObjectID() const;

	virtual void                 RequestPathTo(const Vec3& pos, const Vec3& dir, bool allowDangerousDestination, int forceTargetBuildingId = -1, float endTol = std::numeric_limits<float>::max(),
	                                           float endDistance = 0.0f, CAIObject* pTargetObject = 0, const bool cutPathAtSmartObject = true, const MNMDangersFlags dangersFlags = eMNMDangers_None, const bool considerActorsAsPathObstacles = false);
	void                         RequestPathTo(MNMPathRequest& request);
	virtual void                 RequestPathInDirection(const Vec3& pos, float distance, CWeakRef<CAIObject> refTargetObject, float endDistance = 0.0f);
	/// inherited
	virtual void                 SetPathToFollow(const char* pathName) override;
	virtual void                 SetPathAttributeToFollow(bool bSpline) override;

	const char*                  GetPathToFollow() const { return m_pathToFollowName.c_str(); }
	/// returns the entry point on the designer created path.
	virtual bool                 GetPathEntryPoint(Vec3& entryPos, bool reverse, bool startNearest) const;
	/// use a predefined path - returns true if successful
	virtual bool                 UsePathToFollow(bool reverse, bool startNearest, bool loop);
	virtual void                 SetPointListToFollow(const std::vector<Vec3>& pointList, IAISystem::ENavigationType navType, bool bSpline);
	virtual bool                 UsePointListToFollow(void);
	virtual void                 ClearDevalued() override              {};
	virtual void                 Forget(CAIObject* pDummyObject)       {}
	virtual void                 Navigate3d(CAIObject* pTarget)        {}
	virtual void                 MakeIgnorant(bool bIgnorant) override {}
	CGoalPipe*                   GetCurrentGoalPipe() override         { return m_pCurrentGoalPipe; }
	const CGoalPipe*             GetCurrentGoalPipe() const            { return m_pCurrentGoalPipe; }
	const CGoalPipe*             GetActiveGoalPipe() const             { return m_pCurrentGoalPipe ? m_pCurrentGoalPipe->GetLastSubpipe() : 0; }
	const char*                  GetActiveGoalPipeName() const         { const CGoalPipe* pipe = GetActiveGoalPipe(); return pipe ? pipe->GetName() : "No Active GoalPipe"; }
	void                         ResetCurrentPipe(bool resetAlways) override;
	int                          GetGoalPipeId() const override;
	void                         ResetLookAt() override;
	bool                         SetLookAtPointPos(const Vec3& point, bool priority = false) override;
	bool                         SetLookAtDir(const Vec3& dir, bool priority = false) override;
	void                         CreateLookAtTarget();
	void                         ResetBodyTargetDir() override;
	void                         SetBodyTargetDir(const Vec3& dir) override;
	const Vec3&        GetBodyTargetDir() const override { return m_vBodyTargetDir; }
	void               ResetDesiredBodyDirectionAtTarget();
	void               SetDesiredBodyDirectionAtTarget(const Vec3& dir);
	const Vec3&        GetDesiredBodyDirectionAtTarget() const { return m_vDesiredBodyDirectionAtTarget; }
	void               ResetMovementContext();
	void               ClearMovementContext(unsigned int movementContext);
	void               SetMovementContext(unsigned int movementContext);
	const unsigned int GetMovementContext() const                                                   { return m_movementContext; }
	// Sets the allowed start and end of the path strafing distance.
	void               SetAllowedStrafeDistances(float start, float end, bool whileMoving) override {}

	void               SetExtraPriority(float priority) override                                    { m_AttTargetPersistenceTimeout = priority; };
	float              GetExtraPriority() override                                                  { return m_AttTargetPersistenceTimeout; };

	// Set/reset the looseAttention target to some existing object
	//id is used when clearing the loose attention target
	// m_looseAttentionId is the id of the "owner" of the last SetLooseAttentionTarget(target) operation
	// if the given id is not the same, it means that a more recent SetLooseAttentionTarget() has been set
	// by someone else so the requester can't clear the loose att target	int SetLooseAttentionTarget(CAIObject* pObject=NULL,int id=0);
	int          SetLooseAttentionTarget(CWeakRef<CAIObject> refObject, int id = -1);

	void         SetLookStyle(ELookStyle eLookStyle);
	virtual void AllowLowerBodyToTurn(bool bAllowLowerBodyToTurn) override;
	virtual bool IsAllowingBodyTurn() override;

	ELookStyle   GetLookStyle();

	// Set/reset the looseAttention target to the lookAtPoint and its position
	inline int SetLooseAttentionTarget(const Vec3& pos)
	{
		SetLookAtPointPos(pos);
		return ++m_looseAttentionId;
	}

	Vec3       GetLooseAttentionPos();

	inline int GetLooseAttentionId()
	{
		return m_looseAttentionId;
	}

	typedef std::weak_ptr<Vec3> LookTargetWeakPtr;
	virtual LookTargetPtr CreateLookTarget() override;

	void                  RegisterAttack(const char* name);
	void                  RegisterRetreat(const char* name);
	void                  RegisterWander(const char* name);
	void                  RegisterIdle(const char* name);
	bool                  SelectPipe(int id, const char* name, CWeakRef<CAIObject> refArgument, int goalPipeId = 0, bool resetAlways = false, const GoalParams* node = 0) override; //const XmlNodeRef *node = 0);
	IGoalPipe*            InsertSubPipe(int id, const char* name, CWeakRef<CAIObject> refArgument, int goalPipeId = 0, const GoalParams* node = 0) override;                        //const XmlNodeRef *node = 0);
	bool                  CancelSubPipe(int goalPipeId) override;
	bool                  RemoveSubPipe(int goalPipeId, bool keepInserted = false) override;
	bool                  IsUsingPipe(const char* name) const override;
	bool                  IsUsingPipe(int goalPipeId) const override;
	bool                  AbortActionPipe(int goalPipeId);
#ifdef USE_DEPRECATED_AI_CHARACTER_SYSTEM
	bool                  SetCharacter(const char* character, const char* behaviour = NULL);
#endif
	bool                  IsUsing3DNavigation(); // returns true if it's currently using a 3D navigation

	virtual void          Pause(bool pause) override;
	virtual bool          IsPaused() const override;

	// Returns true if the puppet wants to fire.
	bool              AllowedToFire() const { return (m_fireMode != FIREMODE_OFF) && (m_fireMode != FIREMODE_AIM) && (m_fireMode != FIREMODE_AIM_SWEEP); }
	// Sets the fire mode and shoot target selection (attention target or lastopresult).
	virtual void      SetFireMode(EFireMode mode) override;
	virtual void      SetFireTarget(CWeakRef<CAIObject> refTargetObject);
	virtual EFireMode GetFireMode() const override { return m_fireMode; }

	// Inherited from IPipeUser
	virtual CAIObject*  GetRefPoint() override { CreateRefPoint(); return m_refRefPoint.GetAIObject(); }

	virtual void        SetRefPointPos(const Vec3& pos) override;
	virtual void        SetRefPointPos(const Vec3& pos, const Vec3& dir) override;
	virtual void        SetRefShapeName(const char* shapeName) override;
	virtual const char* GetRefShapeName() const override;
	virtual Vec3        GetProbableTargetPosition() override;
	SShape*             GetRefShape();

	void                CreateRefPoint();

	// Sets an actor target request.
	virtual void                 SetActorTargetRequest(const SAIActorTargetRequest& req) override;
	//
	virtual void                 ClearActorTargetRequest() override;
	//
	const SAIActorTargetRequest* GetActiveActorTargetRequest() const;

	virtual void                 IgnoreCurrentHideObject(float timeOut) override;

	virtual EntityId             GetLastUsedSmartObjectId() const override { return m_idLastUsedSmartObject; }
	virtual bool                 IsUsingNavSO() const override             { return m_eNavSOMethod != nSOmNone; }
	virtual void                 ClearPath(const char* dbgString) override;

	virtual void                 UpdateLookTarget(CAIObject* pTarget);
	void                         EnableUpdateLookTarget(bool bEnable = true);

	/// Called when it is detected that our current path is invalid - if we want
	/// to continue (assuming we're using it) we should request a new one
	void PathIsInvalid();

	// Check if specified hideobject was recently unreachable.
	bool WasHideObjectRecentlyUnreachable(const Vec3& pos) const;

	/// Returns the last live target position.
	const Vec3&        GetLastLiveTargetPosition() const  { return m_lastLiveTargetPos; }
	float              GetTimeSinceLastLiveTarget() const { return m_timeSinceLastLiveTarget; }

	virtual IAIObject* GetAttentionTargetAssociation() const override
	{
		CAIObject* pAttentionTarget = m_refAttentionTarget.GetAIObject();
		return (pAttentionTarget ? pAttentionTarget->GetAssociation().GetAIObject() : NULL);
	}

	virtual CAIObject*             GetLastOpResult() override { return m_refLastOpResult.GetAIObject(); }
	virtual CAIObject*             GetSpecialAIObject(const char* objName, float range = 0.0f) override;

	inline virtual EAITargetThreat GetAttentionTargetThreat() const override { return m_AttTargetThreat; }
	inline virtual EAITargetType   GetAttentionTargetType() const override   { return m_AttTargetType; }

	//Last finished AIAction sets status as succeed or failed
	virtual void SetLastActionStatus(bool bSucceed) override;

	bool         ConvertPathToSpline(IAISystem::ENavigationType navType);

	void         Update(EUpdateType type) override;

	bool           IsAdjustingAim() const;
	void           SetAdjustingAim(bool adjustingAim);

	// Cover
	void           SetCoverID(const CoverID& coverID);
	const CoverID& GetCoverID() const;

	void           SetCoverRegister(const CoverID& coverID);
	const CoverID& GetCoverRegister() const;

	void           UpdateCoverEyesWithTarget(const CAIObject* pTarget, const Vec3& targetPosition);
	void           FillCoverEyes(DynArray<Vec3>& eyesContainer);

	void           SetCoverState(const ICoverUser::StateFlags& state);

	// IPipeUser
	virtual void        SetInCover(bool bInCover) override;
	virtual void        SetMovingToCover(bool bMovingInCover) override;
	virtual void        SetCoverCompromised() override;
	virtual bool        IsCoverCompromised() const override;
	virtual bool        IsInCover() const override;
	virtual bool        IsMovingToCover() const override;
	virtual bool        IsTakingCover(float distanceThreshold) const override;
	// ~IPipeUser
	
	CoverHeight    CalculateEffectiveCoverHeight() const;
	bool           IsMovingInCover() const;

	void           SetCoverBlacklisted(const CoverID& coverID, bool blacklist, float time = 0.0f);
	bool           IsCoverBlacklisted(const CoverID& coverID) const;

	ICoverUser*    GetCoverUser() const { return m_pCoverUser; }
	void           SetCoverInvalidated(CoverID coverID, ICoverUser* pCoverUser);
	// ~Cover

	ILINE float GetLastUpdateInterval() const
	{
		return m_fTimePassed;
	}

	virtual void              OnAIHandlerSentSignal(const char* szText, uint32 crcCode) override;

	Movement::PathfinderState GetPathfinderState();
	INavPath*                 GetINavPath() { return &m_Path; };

	typedef std::vector<COPWaitSignal*> ListWaitGoalOps;
	ListWaitGoalOps m_listWaitGoalOps;

	bool            m_bLastNearForbiddenEdge;
	bool            m_bLastActionSucceed;

	// DEBUG MEMBERS
	EGoalOperations m_lastExecutedGoalop;
	//-----------------------------------

	CWeakRef<CAIObject> m_refLastOpResult;

	// position of potentially usable smart object
	Vec3                m_posLookAtSmartObject;

	CNavPath            m_Path;
	CNavPath            m_OrigPath;
	Vec3                m_PathDestinationPos;                      //! The last point on path before
	bool                m_bPathfinderConsidersPathTargetDirection; //! Defines the way the movement to the target, true = strict, false = fastest, forget end direction.
	float               m_fTimePassed;                             //! how much time passed since last full update

	CWeakRef<CAIObject> m_refPathFindTarget;    //! The target object of the current path find request, if applicable.

	bool                m_bLooseAttention; // true when we have don't have to look exactly at our target all the time

	CWeakRef<CAIObject> m_refLooseAttentionTarget;

	bool                m_bPriorityLookAtRequested;

	CAIHideObject       m_CurrentHideObject;

	Vec3                m_vLastMoveDir; // were was moving last - to be used while next path is generated
	bool                m_bKeepMoving;
	int                 m_nPathDecision;
	uint32              m_queuedPathId;

	bool                m_IsSteering;            // if stering around local obstacle now
	float               m_flightSteeringZOffset; // flight navigation only

	ENavSOMethod        m_eNavSOMethod; // defines which method to use for navigating through a navigational smart object
	bool                m_navSOEarlyPathRegen;
	EntityId            m_idLastUsedSmartObject;

	SNavSOStates m_currentNavSOStates;
	SNavSOStates m_pendingNavSOStates;

	int          m_actorTargetReqId;

	// The movement reason is for debug purposes only.
	enum EMovementReason
	{
		AIMORE_UNKNOWN,
		AIMORE_TRACE,
		AIMORE_MOVE,
		AIMORE_MANEUVER,
		AIMORE_SMARTOBJECT,
	};

#ifdef _DEBUG
	int m_DEBUGmovementReason;
#endif

	virtual void DebugDrawGoals();

	// goal pipe notifications - used by action graph nodes
	typedef std::multimap<int, std::pair<IGoalPipeListener*, const char*>> TMapGoalPipeListeners;
	TMapGoalPipeListeners m_mapGoalPipeListeners;
	void         NotifyListeners(int goalPipeId, EGoalPipeEvent event);
	void         NotifyListeners(CGoalPipe* pPipe, EGoalPipeEvent event, bool includeSubPipes = false);
	virtual void RegisterGoalPipeListener(IGoalPipeListener* pListener, int goalPipeId, const char* debugClassName) override;
	virtual void UnRegisterGoalPipeListener(IGoalPipeListener* pListener, int goalPipeId) override;

	int          CountGroupedActiveGoals(); //
	void         ClearGroupedActiveGoals(); //

	EAimState    GetAimState() const;

	void         SetNavSOFailureStates();

	enum ESpecialAIObjects
	{
		AISPECIAL_LAST_HIDEOBJECT,
		AISPECIAL_PROBTARGET,
		AISPECIAL_PROBTARGET_IN_TERRITORY,
		AISPECIAL_PROBTARGET_IN_REFSHAPE,
		AISPECIAL_PROBTARGET_IN_TERRITORY_AND_REFSHAPE,
		AISPECIAL_ATTTARGET_IN_TERRITORY,
		AISPECIAL_ATTTARGET_IN_REFSHAPE,
		AISPECIAL_ATTTARGET_IN_TERRITORY_AND_REFSHAPE,
		AISPECIAL_ANIM_TARGET,
		AISPECIAL_GROUP_TAC_POS,
		AISPECIAL_GROUP_TAC_LOOK,
		AISPECIAL_VEHICLE_AVOID_POS,
		COUNT_AISPECIAL // Must be last
	};

	CAIObject* GetOrCreateSpecialAIObject(ESpecialAIObjects type);
	bool       ShouldConsiderActorsAsPathObstacles() const { return m_considerActorsAsPathObstacles; }

protected:
	virtual void           HandleVisualStimulus(SAIEVENT* pAIEvent) override;
	virtual void           HandleSoundEvent(SAIEVENT* pAIEvent) override;

	void                   Serialize(TSerialize ser) override;
	void                   PostSerialize() override;
	void                   ClearActiveGoals();
	bool                   ProcessBranchGoal(QGoal& Goal, bool& blocking);
	bool                   ProcessRandomGoal(QGoal& Goal, bool& blocking);
	bool                   ProcessClearGoal(QGoal& Goal, bool& blocking);
	bool                   GetBranchCondition(QGoal& Goal);

	virtual IPathFollower* CreatePathFollower(const PathFollowerParams& params);

	void                   UpdateCovers(EUpdateType type, float updateTime);

	// Callbacks for Cover ability
	void                   CreateMovementPlanCoverStartBlocks(DynArray<Movement::BlockPtr>& blocks, const MovementRequest& request);
	void                   CreateMovementPlanCoverEndBlocks(DynArray<Movement::BlockPtr>& blocks, const MovementRequest& request);
	void                   PrePathFollowUpdate(const MovementUpdateContext& context, bool bIsLastFollowBlock);
	// !Callbacks for Cover ability

	void                   HandleNavSOFailure();
	void                   SyncActorTargetPhaseWithAIProxy();

	float                      m_AttTargetPersistenceTimeout; // to make sure not to jitter between targets; once target is selected, make it stay for some time
	EAITargetThreat            m_AttTargetThreat;
	EAITargetThreat            m_AttTargetExposureThreat;
	EAITargetType              m_AttTargetType;

	EFireMode                  m_fireMode; // Currently set firemode.
	CWeakRef<CAIObject>        m_refFireTarget;
	bool                       m_fireModeUpdated; // Flag indicating if the fire mode has been just updated.
	bool                       m_outOfAmmoSent;   // Flag indicating if OnOutOfAmmo signal was sent for current fireMode session.
	bool                       m_lowAmmoSent;
	bool                       m_wasReloading;
	VectorOGoals               m_vActiveGoals;
	VectorOGoals               m_DeferredActiveGoals;
	bool                       m_bBlocked;
	bool                       m_bStartTiming;
	float                      m_fEngageTime;
	CGoalPipe*                 m_pCurrentGoalPipe;
	VectorSet<int>             m_notAllowedSubpipes; // sub-pipes that we tried to remove or cancel even before inserting them
	bool                       m_bFirstUpdate;
	int                        m_looseAttentionId;
	IAISystem::ENavigationType m_CurrentNodeNavType;
	EAimState                  m_aimState;
	float                      m_spreadFireTime;
	Vec3                       m_vBodyTargetDir;
	Vec3                       m_vDesiredBodyDirectionAtTarget;
	unsigned int               m_movementContext;

	/// Name of the predefined path that we may subsequently be asked to follow
	string                m_pathToFollowName;
	bool                  m_bPathToFollowIsSpline;

	string                m_refShapeName;
	SShape*               m_refShape;
	Vec3                  m_vLastSOExitHelper;

	CStrongRef<CAIObject> m_refSpecialObjects[COUNT_AISPECIAL];

	typedef std::pair<float, Vec3>   FloatVecPair;
	typedef std::deque<FloatVecPair> TimeOutVec3List;
	TimeOutVec3List m_recentUnreachableHideObjects;

	Vec3            m_lastLiveTargetPos;
	float           m_timeSinceLastLiveTarget;

	/// path follower is only set if we're using that class to follow a path
	class IPathFollower* m_pPathFollower;

	/// Private helper - calculates the path obstacles - in practice a previous result may
	/// be used if it's not out of date
	void CalculatePathObstacles();
	CPathObstacles         m_pathAdjustmentObstacles;
	int                    m_adjustpath;

	SAIActorTargetRequest* m_pActorTargetRequest;

	// Cover
	ICoverUser*            m_pCoverUser;
	const CAIObject*       m_pCoverEyesTargetObject;
	Vec3                   m_coverEyesTargetPosition;
	// ~Cover

	//Pauses aspects of the AI's response
	uint8 m_paused;
	bool  m_bEnableUpdateLookTarget;

	typedef std::vector<LookTargetWeakPtr> LookTargets;
	LookTargets m_lookTargets;

private:
	Vec3 SetPointListToFollowSub(Vec3 a, Vec3 b, Vec3 c, std::vector<Vec3>& newPointList, float step);

	// Just here to avoid a warning and to make sure the correct version is used
	void SetAttentionTarget(IAIObject* pObject) { assert(false); }

	EntityId              m_pendingSmartObjectId;
	CStrongRef<CAIObject> m_refRefPoint;          // a tag point the puppet uses to remember a position
	CStrongRef<CAIObject> m_refLookAtTarget;      // a look point - which the loose attention target may be set to!

	bool                  m_adjustingAim;

	struct DelayedPipeSelection
	{
		DelayedPipeSelection()
			: mode(0)
			, refArgument(NILREF)
			, goalPipeId(-1)
			, resetAlways(false)
		{
		}

		DelayedPipeSelection(int _mode, const char* _name, CWeakRef<CAIObject> _refArgument,
		                     int _goalPipeId, bool _resetAlways /*, const GoalParams* _params*/)
			: mode(_mode)
			, name(_name)
			, refArgument(_refArgument)
			, goalPipeId(_goalPipeId)
			, resetAlways(_resetAlways)
		{
			//if (_params)
			//	params = *_params;
		}

		int                 mode;
		string              name;
		CWeakRef<CAIObject> refArgument;
		int                 goalPipeId;
		bool                resetAlways;
		//GoalParams params;
	};

	DelayedPipeSelection         m_delayedPipeSelection;
	bool                         m_pipeExecuting;

	bool                         m_cutPathAtSmartObject;
	bool                         m_considerActorsAsPathObstacles;

	PipeUserMovementActorAdapter m_movementActorAdapter;
	MovementActorCallbacks       m_callbacksForPipeuser;
	SMovementActionAbilityCallbacks m_coverMovementAbility;
};

ILINE const CPipeUser* CastToCPipeUserSafe(const IAIObject* pAI) { return pAI ? pAI->CastToCPipeUser() : 0; }
ILINE CPipeUser*       CastToCPipeUserSafe(IAIObject* pAI)       { return pAI ? pAI->CastToCPipeUser() : 0; }

#endif
