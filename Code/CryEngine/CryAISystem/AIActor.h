// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

/********************************************************************
   -------------------------------------------------------------------------
   File name:   AIActor.h
   $Id$
   Description: Header for the CAIActor class

   -------------------------------------------------------------------------
   History:
   14:12:2006 -  Created by Kirill Bulatsev

 *********************************************************************/

#ifndef AIACTOR_H
#define AIACTOR_H

#if _MSC_VER > 1000
	#pragma once
#endif // _MSC_VER > 1000

#include "AIObject.h"
#include "BlackBoard.h"
#include "PersonalLog.h"
#include "CollisionAvoidance/CollisionAvoidanceSystem.h"

#include <CryAISystem/IAIActor.h>
#include <CryAISystem/IAgent.h>

#include <CryAISystem/ValueHistory.h>
#include <CryAISystem/INavigationSystem.h>

struct IPerceptionHandlerModifier;
struct SShape;

namespace BehaviorTree
{
struct INode;
class TimestampCollection;
}

// AI Actor Collision Avoidance agent
class CActorCollisionAvoidance : public ICollisionAvoidanceAgent
{
public:
	CActorCollisionAvoidance(CAIActor* pActor);
	virtual ~CActorCollisionAvoidance() override;

	void Reset();
	void Serialize(TSerialize ser);

	virtual NavigationAgentTypeID GetNavigationTypeId() const override;
	virtual const INavMeshQueryFilter* GetNavigationQueryFilter() const override;
	virtual const char* GetName() const override;
	virtual TreatType GetTreatmentType() const override;

	virtual void InitializeCollisionAgent(CCollisionAvoidanceSystem::SAgentParams& agent) const override;
	virtual void InitializeCollisionObstacle(CCollisionAvoidanceSystem::SObstacleParams& obstacle) const override;
	virtual void ApplyComputedVelocity(const Vec2& avoidanceVelocity, float updateTime) override;

private:
	CAIActor* m_pActor;
	float m_radiusIncrement;
};

// Structure reflecting the physical entity parts.
// When choosing the target location to hit, the AI chooses amongst one of these.
struct SAIDamagePart
{
	SAIDamagePart() : pos(ZERO), damageMult(0.f), volume(0.f), surfaceIdx(0) {}
	void GetMemoryUsage(ICrySizer* pSizer) const {}
	Vec3  pos;        // Position of the part.
	float damageMult; // Damage multiplier of the part, can be used to choose the most damage causing part.
	float volume;     // The volume of the part, can be used to choose the largest part to hit.
	int   surfaceIdx; // The index of the surface material.
};

typedef std::vector<SAIDamagePart>               DamagePartVector;
typedef std::vector<IPerceptionHandlerModifier*> TPerceptionHandlerModifiersVector;

/*! Basic ai object class. Defines a framework that all puppets and points of interest
   later follow.
 */
class CAIActor
	: public CAIObject
	  , public IAIActor
{
	friend class CAISystem;

public:
	CAIActor();
	virtual ~CAIActor();

	virtual const IAIObject*  CastToIAIObject() const override { return this; }
	virtual IAIObject*        CastToIAIObject() override { return this; }
	virtual const IAIActor*   CastToIAIActor() const override { return this; }
	virtual IAIActor*         CastToIAIActor() override       { return this; }

	virtual bool              CanDamageTarget(IAIObject* target = 0) const override;
	virtual bool              CanDamageTargetWithMelee() const override;

	virtual IPhysicalEntity*  GetPhysics(bool bWantCharacterPhysics = false) const override;

	virtual bool              HasThrown(EntityId entity) const override { return false; }

	void                      SetBehaviorVariable(const char* variableName, bool value) override;
	bool                      GetBehaviorVariable(const char* variableName) const;

	void                      ResetModularBehaviorTree(EObjectResetType type) override;
	virtual void              SetModularBehaviorTree(const char* szTreeName) override { m_modularBehaviorTreeName = szTreeName; ResetModularBehaviorTree(AIOBJRESET_INIT); }

	const SAIBodyInfo& QueryBodyInfo();
	const SAIBodyInfo& GetBodyInfo() const;

	////////////////////////////////////////////////////////////////////////////////////////
	//IAIPathAgent//////////////////////////////////////////////////////////////////////////
	virtual IEntity*                    GetPathAgentEntity() const override;
	virtual const char*                 GetPathAgentName() const override;
	virtual unsigned short              GetPathAgentType() const override;
	virtual float                       GetPathAgentPassRadius() const override;
	virtual Vec3                        GetPathAgentPos() const override;
	virtual Vec3                        GetPathAgentVelocity() const override;

	virtual const AgentMovementAbility& GetPathAgentMovementAbility() const override;
	virtual IPathFollower*              GetPathFollower() const override;

	virtual void                        SetPathToFollow(const char* pathName) override;
	virtual void                        SetPathAttributeToFollow(bool bSpline) override;

	virtual bool GetValidPositionNearby(const Vec3& proposedPosition, Vec3& adjustedPosition) const override;
	virtual bool GetTeleportPosition(Vec3& teleportPos) const override;

	virtual bool IsPointValidForAgent(const Vec3& pos, uint32 flags) const override { return true; }
	//IAIPathAgent//////////////////////////////////////////////////////////////////////////
	////////////////////////////////////////////////////////////////////////////////////////

	//===================================================================
	// inherited virtual interface functions
	//===================================================================
	virtual void                SetPos(const Vec3& pos, const Vec3& dirFwrd = Vec3Constants<float>::fVec3_OneX) override;
	virtual void                Reset(EObjectResetType type) override;
	virtual void                OnObjectRemoved(CAIObject* pObject) override;
	virtual SOBJECTSTATE&       GetState(void) override       { return m_State; }
	virtual const SOBJECTSTATE& GetState(void) const override { return m_State; }
	virtual void                SetSignal(int nSignalID, const char* szText, IEntity* pSender = 0, IAISignalExtraData* pData = NULL, uint32 crcCode = 0) override;
	virtual void                OnAIHandlerSentSignal(const char* szText, uint32 crcCode) override;
	virtual void                Serialize(TSerialize ser) override;
	virtual void                Update(EUpdateType type);
	virtual void                UpdateProxy(EUpdateType type);
	virtual void                UpdateDisabled(EUpdateType type); // when AI object is disabled still may need to send some signals
	virtual void                SetProxy(IAIActorProxy* proxy) override;
	virtual IAIActorProxy*      GetProxy() const override;
	virtual bool                CanAcquireTarget(IAIObject* pOther) const override;
	virtual void                ResetPerception() override {}
	virtual bool                IsHostile(const IAIObject* pOther, bool bUsingAIIgnorePlayer = true) const override;
	virtual void                ParseParameters(const AIObjectParams& params, bool bParseMovementParams = true);
	virtual void                Event(unsigned short eType, SAIEVENT* pAIEvent) override;
	virtual void                EntityEvent(const SEntityEvent& event) override;
	virtual void                SetGroupId(int id) override;
	virtual void                SetFactionID(uint8 factionID) override;

	virtual void                SetObserver(bool observer);
	virtual uint32              GetObserverTypeMask() const;
	virtual uint32              GetObservableTypeMask() const override;

	virtual void                ReactionChanged(uint8 factionID, IFactionMap::ReactionType reaction);
	virtual void                VisionChanged(float sightRange, float primaryFOVCos, float secondaryFOVCos);
	virtual bool                IsObserver() const override;
	virtual bool                CanSee(const VisionID& otherVisionID) const override;

	virtual void                RegisterBehaviorListener(IActorBehaviorListener* listener) override;
	virtual void                UnregisterBehaviorListener(IActorBehaviorListener* listener) override;
	virtual void                BehaviorEvent(EBehaviorEvent event) override;
	virtual void                BehaviorChanged(const char* current, const char* previous) override;

	//===================================================================
	// virtual functions rooted here
	//===================================================================

	// Returns a list containing the information about the parts that can be shot at.
	virtual DamagePartVector*           GetDamageParts()                                          { return 0; }

	const AgentParameters&              GetParameters() const override                            { return m_Parameters; }
	virtual void                        SetParameters(const AgentParameters& params) override;
	virtual const AgentMovementAbility& GetMovementAbility() const override                       { return m_movementAbility; }
	virtual void                        SetMovementAbility(AgentMovementAbility& params) override { m_movementAbility = params; }
	// (MATT) There is now a method to serialise these {2009/04/23}

	virtual bool        IsLowHealthPauseActive() const override      { return false; }
	virtual IEntity*    GetGrabbedEntity() const                     { return 0; } // consider only player grabbing things, don't care about NPC
	virtual bool        IsGrabbedEntityInView(const Vec3& pos) const { return false; }

	void                GetLocalBounds(AABB& bbox) const;

	virtual bool        IsDevalued(IAIObject* pAIObject) override { return false; }

	virtual void        ResetLookAt() override;
	virtual bool        SetLookAtPointPos(const Vec3& vPoint, bool bPriority = false) override;
	virtual bool        SetLookAtDir(const Vec3& vDir, bool bPriority = false) override;

	virtual void        ResetBodyTargetDir() override;
	virtual void        SetBodyTargetDir(const Vec3& vDir) override;
	virtual const Vec3& GetBodyTargetDir() const override;

	virtual void        SetMoveTarget(const Vec3& vMoveTarget) override;
	virtual void        GoTo(const Vec3& vTargetPos) override;
	virtual void        SetSpeed(float fSpeed) override;

	virtual bool        IsInvisibleFrom(const Vec3& pos, bool bCheckCloak = true, bool bCheckCloakDistance = true, const CloakObservability& cloakObservability = CloakObservability()) const override;

	//===================================================================
	// non-virtual functions
	//===================================================================

	void NotifyDeath();
	// Returns true if the cloak is effective (includes check for cloak and
	bool IsCloakEffective(const Vec3& pos) const;

	// Get the sight FOV cosine values for the actor
	void GetSightFOVCos(float& primaryFOVCos, float& secondaryFOVCos) const;
	void CacheFOVCos(float primaryFOV, float secondaryFOV);

	// Used to adjust the visibility range for an object checking if they can see me
	virtual float AdjustTargetVisibleRange(const CAIActor& observer, float fVisibleRange) const;

	// Returns the maximum visible range to the target
	virtual float GetMaxTargetVisibleRange(const IAIObject* pTarget, bool bCheckCloak = true) const override;

	// Gets the light level at actors position.
	inline EAILightLevel GetLightLevel() const     { return m_lightLevel; }
	virtual bool         IsAffectedByLight() const { return m_Parameters.m_PerceptionParams.isAffectedByLight; }

	// Populates list of physics entities to skip for raycasting.
	virtual void                  GetPhysicalSkipEntities(PhysSkipList& skipList) const override;
	virtual void                  UpdateObserverSkipList();

	virtual NavigationAgentTypeID GetNavigationTypeID() const override
	{
		return m_navigationTypeID;
	}

	// Coordination state change functions
	void CoordinationEntered(const char* signalName);
	void CoordinationExited(const char* signalName);

	bool                 m_bCheckedBody;

	SOBJECTSTATE         m_State;
	AgentParameters      m_Parameters;
	AgentMovementAbility m_movementAbility;

#ifdef CRYAISYSTEM_DEBUG
	CValueHistory<float>* m_healthHistory;
#endif

	virtual bool                   IsActive() const override            { return m_bEnabled; }
	virtual bool                   IsAgent() const override             { return true; }

	inline bool                    IsUsingCombatLight() const           { return m_usingCombatLight; }

	inline float                   GetCachedWaterOcclusionValue() const { return m_cachedWaterOcclusionValue; }

	virtual IBlackBoard*           GetBlackBoard() override             { return &m_blackBoard; }
	virtual IBlackBoard*           GetBehaviorBlackBoard() override     { return &m_behaviorBlackBoard; }

	virtual IAIObject*             GetAttentionTarget() const override  { return m_refAttentionTarget.GetAIObject(); }
	virtual void                   SetAttentionTarget(CWeakRef<CAIObject> refAttTarget);

	inline virtual EAITargetThreat GetAttentionTargetThreat() const override   { return m_State.eTargetThreat; }
	inline virtual EAITargetType   GetAttentionTargetType() const override     { return m_State.eTargetType; }

	inline virtual EAITargetThreat GetPeakThreatLevel() const override         { return m_State.ePeakTargetThreat; }
	inline virtual EAITargetType   GetPeakThreatType() const override          { return m_State.ePeakTargetType; }
	inline virtual tAIObjectID     GetPeakTargetID() const override            { return m_State.ePeakTargetID; }

	inline virtual EAITargetThreat GetPreviousPeakThreatLevel() const override { return m_State.ePreviousPeakTargetThreat; }
	inline virtual EAITargetType   GetPreviousPeakThreatType() const override  { return m_State.ePreviousPeakTargetType; }
	inline virtual tAIObjectID     GetPreviousPeakTargetID() const override    { return m_State.ePreviousPeakTargetID; }

	virtual Vec3                   GetFloorPosition(const Vec3& pos) override;

	void               AddPersonallyHostile(tAIObjectID hostileID);
	void               RemovePersonallyHostile(tAIObjectID hostileID);
	void               ResetPersonallyHostiles();
	bool               IsPersonallyHostile(tAIObjectID hostileID) const;

#ifdef CRYAISYSTEM_DEBUG
	void UpdateHealthHistory();
#endif

	virtual void               SetTerritoryShapeName(const char* szName) override;
	virtual const char*        GetTerritoryShapeName() const override;
	virtual const char*        GetWaveName() const override { return m_Parameters.m_sWaveName.c_str(); }
	virtual bool               IsPointInsideTerritoryShape(const Vec3& vPos, bool bCheckHeight) const override;
	virtual bool               ConstrainInsideTerritoryShape(Vec3& vPos, bool bCheckHeight) const override;

	const SShape*              GetTerritoryShape() const { return m_territoryShape; }
	SShape*                    GetTerritoryShape()       { return m_territoryShape; }

	void                       GetMovementSpeedRange(float fUrgency, bool bSlowForStrafe, float& normalSpeed, float& minSpeed, float& maxSpeed) const;

	virtual EFieldOfViewResult IsPointInFOV(const Vec3& pos, float distanceScale = 1.f) const override;
	virtual EFieldOfViewResult IsObjectInFOV(const IAIObject* pTarget, float distanceStale = 1.f) const override;

	enum ENavInteraction { NI_IGNORE, NI_STEER, NI_SLOW };
	// indicates the way that two objects should negotiate each other
	static ENavInteraction GetNavInteraction(const CAIObject* navigator, const CAIObject* obstacle);

	virtual void           CancelRequestedPath(bool actorRemoved);

#ifdef AI_COMPILE_WITH_PERSONAL_LOG
	PersonalLog& GetPersonalLog() { return m_personalLog; }
#endif

	// - returns the initial position at the time the Puppet was constructed
	// - this is the position of where the level designer placed the Puppet
	// - if false is returned, then the Puppet was probably not created through the level editor
	bool GetInitialPosition(Vec3& initialPosition) const;

	// Returns the attention target if it is an agent, or the attention target association, or null if there is no association.
	static CWeakRef<CAIActor> GetLiveTarget(const CWeakRef<CAIObject>& refTarget);

	// Returns the attention target if it is an agent, or the attention target association, or null if there is no association.
	static const CAIObject* GetLiveTarget(const CAIObject* pTarget);

protected:

	uint32 GetFactionVisionMask(uint8 factionID) const;

	void   SerializeMovementAbility(TSerialize ser);

	//process cloak_scale fading
	void UpdateCloakScale();

	// Updates the properties of the damage parts.
	void               UpdateDamageParts(DamagePartVector& parts);

	EFieldOfViewResult CheckPointInFOV(const Vec3& vPoint, float fSightRange) const;

	virtual void       HandlePathDecision(MNMPathRequestResult& result);
	virtual void       HandleVisualStimulus(SAIEVENT* pAIEvent);
	virtual void       HandleSoundEvent(SAIEVENT* pAIEvent);
	virtual void       HandleBulletRain(SAIEVENT* pAIEvent);

	// AIOT_PLAYER is to be treated specially, as it's the player, but otherwise is the same as
	// AIOT_AGENTSMALL
	enum EAIObjectType { AIOT_UNKNOWN, AIOT_PLAYER, AIOT_AGENTSMALL, AIOT_AGENTMED, AIOT_AGENTBIG, AIOT_MAXTYPES };
	// indicates the sort of object the agent is with regard to navigation
	static EAIObjectType GetObjectType(const CAIObject* ai, unsigned short type);

	_smart_ptr<IAIActorProxy>         m_proxy;

	EAILightLevel                     m_lightLevel;
	bool                              m_usingCombatLight;

	float                             m_cachedWaterOcclusionValue;

	Vec3                              m_vLastFullUpdatePos;
	EStance                           m_lastFullUpdateStance;

	CBlackBoard                       m_blackBoard;
	CBlackBoard                       m_behaviorBlackBoard;

	TPerceptionHandlerModifiersVector m_perceptionHandlerModifiers;

	typedef std::set<IActorBehaviorListener*> BehaviorListeners;
	BehaviorListeners                   m_behaviorListeners;

	// (MATT) Note: this is a different attention target to the classic. Nasty OO hack {2009/02/03}
	// [4/20/2010 evgeny] Moved here from CPipeUser and CAIPlayer
	CWeakRef<CAIObject> m_refAttentionTarget;

	string              m_territoryShapeName;
	SShape*             m_territoryShape;

	float               m_bodyTurningSpeed;
	Vec3                m_lastBodyDir;

	float               m_stimulusStartTime;

	unsigned int        m_activeCoordinationCount;

	bool                m_observer;

	string              m_modularBehaviorTreeName;

private:
	CActorCollisionAvoidance m_collisionAvoidanceAgent;

	float m_FOVPrimaryCos;
	float m_FOVSecondaryCos;

	typedef VectorSet<tAIObjectID> PersonallyHostiles;
	PersonallyHostiles    m_forcefullyHostiles;

	SAIBodyInfo           m_bodyInfo;

	NavigationAgentTypeID m_navigationTypeID;

#ifdef AI_COMPILE_WITH_PERSONAL_LOG
	PersonalLog m_personalLog;
#endif

	// position of the Puppet placed by the level designer
	struct SInitialPosition
	{
		Vec3 pos;
		bool isValid;   // becomes valid in Reset()

		SInitialPosition() { isValid = false; }
	};

	SInitialPosition m_initialPosition;

private:
	void StartBehaviorTree(const char* behaviorName);
	void StopBehaviorTree();
	bool IsRunningBehaviorTree() const;

	bool m_runningBehaviorTree;
};

ILINE const CAIActor* CastToCAIActorSafe(const IAIObject* pAI) { return pAI ? pAI->CastToCAIActor() : 0; }
ILINE CAIActor*       CastToCAIActorSafe(IAIObject* pAI)       { return pAI ? pAI->CastToCAIActor() : 0; }

#endif
