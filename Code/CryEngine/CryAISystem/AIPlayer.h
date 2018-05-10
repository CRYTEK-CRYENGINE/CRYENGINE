// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

/********************************************************************
   -------------------------------------------------------------------------
   File name:   AIPlayer.h
   $Id$
   Description:

   -------------------------------------------------------------------------
   History:
   - ?
   - 2 Mar 2009			 : Evgeny Adamenkov: Removed IRenderer

 *********************************************************************/
#ifndef _AI_PLAYER_
#define _AI_PLAYER_

#if _MSC_VER > 1000
	#pragma once
#endif

#include "AIActor.h"
#include <CrySystem/TimeValue.h>
#include "AIDbgRecorder.h"

class CMissLocationSensor;

// ChrisR: Disable the MissLocationSensor to save multiple
// GetEntitiesAround calls in physics that are quite heavy in very concentrated
// combat scenes. Currently this can amount to large percentages of the main
// thread time.
#if !CRY_PLATFORM_MOBILE
	#define ENABLE_MISSLOCATION_SENSOR 1
#endif
#ifndef ENABLE_MISSLOCATION_SENSOR
	#define ENABLE_MISSLOCATION_SENSOR 1
#endif

class CAIPlayer : public CAIActor
{
	typedef CAIActor MyBase;

public:
	CAIPlayer();
	~CAIPlayer();

	void                       ParseParameters(const AIObjectParams& params, bool bParseMovementParams = true) override;
	void                       Reset(EObjectResetType type) override;

	void                       UpdateAttentionTarget(CWeakRef<CAIObject> refTarget);
	virtual EFieldOfViewResult IsPointInFOV(const Vec3& pos, float distanceScale = 1.0f) const override;
	void                       Update(EUpdateType type) override;
	virtual void               UpdateProxy(EUpdateType type) override {}
	virtual void               Serialize(TSerialize ser) override;
	virtual void               OnObjectRemoved(CAIObject* pObject) override;
	virtual bool               IsLowHealthPauseActive() const override;
	virtual IEntity*           GetGrabbedEntity() const override;// consider only player grabbing things, don't care about NPC
	virtual bool               IsGrabbedEntityInView(const Vec3& pos) const override;

	virtual void               GetObservablePositions(ObservableParams& observableParams) const override;
	virtual uint32             GetObservableTypeMask() const override;

	// Inherited
	virtual DamagePartVector* GetDamageParts() override;

	virtual void              Event(unsigned short eType, SAIEVENT* pEvent) override;

	int                       GetDeathCount() { return m_deathCount; }
	void                      IncDeathCount() { m_deathCount++; }

	virtual void              RecordEvent(IAIRecordable::e_AIDbgEvent event, const IAIRecordable::RecorderEventData* pEventData = NULL) override;
	virtual void              RecordSnapshot() override;

	virtual float             AdjustTargetVisibleRange(const CAIActor& observer, float fVisibleRange) const override;
	virtual bool              IsAffectedByLight() const override;
	virtual bool              HasThrown(EntityId entity) const override { return IsThrownByPlayer(entity); }

	bool                      IsThrownByPlayer(EntityId ent) const;
	bool                      IsPlayerStuntAffectingTheDeathOf(CAIActor* pDeadActor) const;
	EntityId                  GetNearestThrownEntity(const Vec3& pos);
	bool                      IsDoingStuntActionRelatedTo(const Vec3& pos, float nearDistance);

	virtual void              GetPhysicalSkipEntities(PhysSkipList& skipList) const override;

	bool                      GetMissLocation(const Vec3& shootPos, const Vec3& shootDir, float maxAngle, Vec3& pos);
	void                      NotifyMissLocationConsumed();
	void                      NotifyPlayerActionToTheLookingAgents(const char* eventName);

	void                      DebugDraw();

private:
	CAIPlayer(const CAIPlayer&); // disallow copies
	CAIPlayer& operator=(const CAIPlayer&);

	EntityId         m_lastGrabbedEntityID;

	CTimeValue       m_fLastUpdateTargetTime;
	float            m_FOV;
	DamagePartVector m_damageParts;
	bool             m_damagePartsUpdated;
	int              m_deathCount;

	struct SThrownItem
	{
		SThrownItem(EntityId id) : id(id), time(0.0f), pos(0, 0, 0), vel(0, 0, 0), r(0.1f) {}
		inline bool operator<(const SThrownItem& rhs) const { return time < rhs.time; }
		float    time;
		Vec3     pos, vel;
		float    r;
		EntityId id;
	};
	std::vector<SThrownItem> m_lastThrownItems;

	struct SStuntTargetAIActor
	{
		SStuntTargetAIActor(CAIActor* pAIActor, const Vec3& pos) : pAIActor(pAIActor), t(0), exposed(0), signalled(false), threatPos(pos) {}
		CAIActor* pAIActor;
		Vec3      threatPos;
		float     t;
		float     exposed;
		bool      signalled;
	};
	std::vector<SStuntTargetAIActor> m_stuntTargets;

	void AddThrownEntity(EntityId id);

	void UpdatePlayerStuntActions();
	void HandleCloaking(bool cloak);
	void HandleArmoredHit();
	void HandleStampMelee();

	Vec3  m_stuntDir;

	float m_playerStuntSprinting;
	float m_playerStuntJumping;
	float m_playerStuntCloaking;
	float m_playerStuntUncloaking;

	float m_mercyTimer;

	void CollectExposedCover();
	void ReleaseExposedCoverObjects();
	void AddExposedCoverObject(IPhysicalEntity* pPhysEnt);
	void CollectExposedCoverRayComplete(const QueuedRayID& rayID, const RayCastResult& result);

	float m_coverExposedTime;
	float m_coolMissCooldown;

	struct SExposedCoverObject
	{
		SExposedCoverObject(IPhysicalEntity* pPhysEnt, float t) : pPhysEnt(pPhysEnt), t(t) {}
		IPhysicalEntity* pPhysEnt;
		float            t;
	};

	struct ExposedCoverState
	{
		ExposedCoverState() : asyncState(AsyncReady), rayID(0) {}
		AsyncState  asyncState;
		QueuedRayID rayID;
	};

	std::vector<SExposedCoverObject> m_exposedCoverObjects;
	ExposedCoverState                m_exposedCoverState;

#if ENABLE_MISSLOCATION_SENSOR
	std::unique_ptr<CMissLocationSensor> m_pMissLocationSensor;
#endif
};

inline const CAIPlayer* CastToCAIPlayerSafe(const IAIObject* pAI) { return pAI ? pAI->CastToCAIPlayer() : 0; }
inline CAIPlayer*       CastToCAIPlayerSafe(IAIObject* pAI)       { return pAI ? pAI->CastToCAIPlayer() : 0; }

#endif
