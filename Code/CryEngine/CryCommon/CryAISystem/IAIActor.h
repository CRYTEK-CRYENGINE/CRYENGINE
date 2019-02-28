// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#ifndef IAIActor_h
#define IAIActor_h

#include <CryAISystem/IAgent.h>      // <> required for Interfuscator
#include <CryAISystem/IPathfinder.h> // <> required for Interfuscator

struct IBlackBoard;
struct VisionID;

enum EBehaviorEvent
{
	BehaviorStarted,
	BehaviorInterrupted,
	BehaviorFinished,
	BehaviorFailed,
};

struct IActorBehaviorListener
{
	// <interfuscator:shuffle>
	virtual void BehaviorEvent(IAIObject* actor, EBehaviorEvent event) = 0;
	virtual void BehaviorChanged(IAIObject* actor, const char* current, const char* previous) = 0;
	virtual ~IActorBehaviorListener(){}
	// </interfuscator:shuffle>
};

struct IAIActor : public IAIPathAgent
{
	struct CloakObservability
	{
		CloakObservability()
			: cloakMaxDistStill(4.0f)
			, cloakMaxDistMoving(4.0f)
			, cloakMaxDistCrouchedAndStill(4.0f)
			, cloakMaxDistCrouchedAndMoving(4.0f)
		{
		}

		float cloakMaxDistStill;
		float cloakMaxDistMoving;
		float cloakMaxDistCrouchedAndStill;
		float cloakMaxDistCrouchedAndMoving;
	};

	// <interfuscator:shuffle>
	virtual ~IAIActor() {}
	
	virtual const IAIObject*            CastToIAIObject() const = 0;
	virtual IAIObject*                  CastToIAIObject() = 0;

	virtual SOBJECTSTATE&               GetState() = 0;
	virtual const SOBJECTSTATE&         GetState() const = 0;
	virtual IAIActorProxy*              GetProxy() const = 0;
	virtual void                        SetSignal(int nSignalID, const char* szText, IEntity* pSender = 0, IAISignalExtraData* pData = NULL, uint32 crcCode = 0) = 0;
	virtual void                        OnAIHandlerSentSignal(const char* szText, uint32 crcCode) = 0;
	virtual const AgentParameters&      GetParameters() const = 0;
	virtual void                        SetParameters(const AgentParameters& pParams) = 0;
	virtual const AgentMovementAbility& GetMovementAbility() const = 0;
	virtual void                        SetMovementAbility(AgentMovementAbility& pParams) = 0;
	virtual bool                        CanAcquireTarget(IAIObject* pOther) const = 0;
	virtual void                        ResetPerception() = 0;
	virtual bool                        IsActive() const = 0;

	virtual bool                        CanDamageTarget(IAIObject* target = 0) const = 0;
	virtual bool                        CanDamageTargetWithMelee() const = 0;

	virtual bool                        IsObserver() const = 0;
	virtual bool                        CanSee(const VisionID& otherID) const = 0;

	//! \return the maximum visible range to the target.
	virtual float           GetMaxTargetVisibleRange(const IAIObject* pTarget, bool bCheckCloak = true) const = 0;

	virtual void            RegisterBehaviorListener(IActorBehaviorListener* listener) = 0;
	virtual void            UnregisterBehaviorListener(IActorBehaviorListener* listener) = 0;
	virtual void            BehaviorEvent(EBehaviorEvent event) = 0;
	virtual void            BehaviorChanged(const char* current, const char* previous) = 0;
	virtual void            SetBehaviorVariable(const char* variableName, bool value) = 0;
	virtual void            SetModularBehaviorTree(const char* szTreeName) {}
	virtual void            ResetModularBehaviorTree(EObjectResetType type) = 0;

	virtual IBlackBoard*    GetBehaviorBlackBoard() { return NULL; }

	virtual IAIObject*      GetAttentionTarget() const = 0;

	virtual EAITargetThreat GetAttentionTargetThreat() const = 0;
	virtual EAITargetType   GetAttentionTargetType() const = 0;

	virtual EAITargetThreat GetPeakThreatLevel() const = 0;
	virtual EAITargetType   GetPeakThreatType() const = 0;
	virtual tAIObjectID     GetPeakTargetID() const = 0;

	virtual EAITargetThreat GetPreviousPeakThreatLevel() const = 0;
	virtual EAITargetType   GetPreviousPeakThreatType() const = 0;
	virtual tAIObjectID     GetPreviousPeakTargetID() const = 0;

	//! \return a specified point projected on floor/ground.
	virtual Vec3        GetFloorPosition(const Vec3& pos) = 0;

	virtual bool        IsDevalued(IAIObject* pAIObject) = 0;

	virtual void        ResetLookAt() = 0;
	virtual bool        SetLookAtPointPos(const Vec3& vPoint, bool bPriority = false) = 0;
	virtual bool        SetLookAtDir(const Vec3& vDir, bool bPriority = false) = 0;

	virtual void        ResetBodyTargetDir() = 0;
	virtual void        SetBodyTargetDir(const Vec3& vDir) = 0;
	virtual const Vec3& GetBodyTargetDir() const = 0;

	virtual void        SetMoveTarget(const Vec3& vMoveTarget) = 0;
	virtual void        GoTo(const Vec3& vTargetPos) = 0;
	virtual void        SetSpeed(float fSpeed) = 0;

	//! Sets the shape that defines the AI Actor territory.
	virtual void        SetTerritoryShapeName(const char* szName) = 0;
	virtual const char* GetTerritoryShapeName() const = 0;
	virtual const char* GetWaveName() const = 0;
	virtual bool        IsPointInsideTerritoryShape(const Vec3& vPos, bool bCheckHeight) const = 0;
	virtual bool        ConstrainInsideTerritoryShape(Vec3& vPos, bool bCheckHeight) const = 0;

	virtual bool        HasThrown(EntityId entity) const = 0;

	//! Populates list of physics entities to skip for raycasting.
	virtual void                  GetPhysicalSkipEntities(PhysSkipList& skipList) const {}

	virtual NavigationAgentTypeID GetNavigationTypeID() const = 0;

	//! \return true if the agent cannot be seen from the specified point.
	virtual bool IsInvisibleFrom(const Vec3& pos, bool bCheckCloak = true, bool bCheckCloakDistance = true, const CloakObservability& cloakObservability = CloakObservability()) const = 0;

	virtual bool IsLowHealthPauseActive() const = 0;

	virtual void SetPathToFollow(const char* pathName) = 0;
	// </interfuscator:shuffle>
};

#endif
