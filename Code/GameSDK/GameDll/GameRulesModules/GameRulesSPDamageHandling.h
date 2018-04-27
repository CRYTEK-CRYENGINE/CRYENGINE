// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#ifndef GameRulesSPDamageHandling_h
#define GameRulesSPDamageHandling_h

#include "GameRulesCommonDamageHandling.h"
#include <CryCore/CryFlags.h>

class CMercyTimeFilter
{
	enum 
	{
		MaxDifficultyLevels = 5,
	};

	struct ClassFilter
	{
		enum
		{
			eType_None,
			eType_Self
		};

		bool operator == (const uint16& otherClassId) const
		{
			return (classId == otherClassId);
		}

		bool operator < (const uint16& otherClassId) const
		{
			return (classId < otherClassId);
		}

		uint16 classId;
		uint16 type;
	};

	struct CompareClassFilter
	{
		bool operator() (const ClassFilter& lhs, const ClassFilter& rhs) const
		{
			return (lhs.classId < rhs.classId);
		}
	};

	typedef std::vector<ClassFilter> FilteredProjectiles;

public:

	CMercyTimeFilter();

	void Init( const XmlNodeRef& xml );
	bool Filtered( const uint16 projectileClassId, const EntityId ownerId, const EntityId targetId ) const;

	void OnLocalPlayerEnteredMercyTime();

private:
	
	FilteredProjectiles m_filteredProjectiles;
	CryFixedArray<uint32, MaxDifficultyLevels> m_difficultyTolerance;

	mutable uint32 m_lastMercyTimeFilterCount;
};

class CInvulnerableFilter
{
public:
	enum EReason
	{
		eReason_InTutorial = BIT(0),
		eReason_TrainRide  = BIT(1),
	};

	CInvulnerableFilter()
		: m_minHealthThreshold(20.0f)
	{

	}

	void Init();
	bool FilterIncomingDamage( const HitInfo& hitInfo, const float currentTargetHealth ) const;

	void AddInvulnerableReason(const EReason reason) { m_enableState.AddFlags(reason); };
	void RemoveInvulnerableReason(const EReason reason) { m_enableState.ClearFlags(reason); };

private:
	
	ILINE bool IsEnabled() const { return (m_enableState.GetRawFlags() != 0); }

	float m_minHealthThreshold;
	CCryFlags<uint32> m_enableState;
};

//////////////////////////////////////////////////////////////////////////

class CGameRulesSPDamageHandling : public CGameRulesCommonDamageHandling
{
public:
	CGameRulesSPDamageHandling();

	// IGameRulesDamageHandlingModule
	virtual ~CGameRulesSPDamageHandling();

	virtual void Init(XmlNodeRef xml);
	virtual void Update(float frameTime);

	virtual bool SvOnHit(const HitInfo& hitInfo);
	virtual bool SvOnHitScaled(const HitInfo& hitInfo);
	virtual void SvOnExplosion(const ExplosionInfo& explosionInfo, const CGameRules::TExplosionAffectedEntities& affectedEntities);
	virtual void SvOnCollision(const IEntity* victimEntity, const CGameRules::SCollisionHitInfo& collisionHitInfo);

	virtual void MakeMovementVisibleToAIForEntityClass(const IEntityClass* pEntityClass);

	virtual void OnGameEvent(const IGameRulesDamageHandlingModule::EGameEvents& gameEvent);
	//~ IGameRulesDamageHandlingModule

	bool IsDead(CActor* actor, IScriptTable* actorScript);

protected:
	struct SReactionInfoOnHit
	{
		SReactionInfoOnHit() : bMakeVictimFall(false), bMakeOffenderFall(false), bTriggerHitReaction(false), pOffenderActor(NULL) {}

		bool bMakeVictimFall;
		bool bMakeOffenderFall;
		bool bTriggerHitReaction;

		IActor* pOffenderActor;
	};

	void ProcessDeath(IActor* victimActor, const SmartScriptTable& victimScript, const HitInfo& hitInfo);
	float CalcExplosionDamage(IEntity* entity, const ExplosionInfo& explosionInfo, float obstruction);

	float AdjustPlayerCollisionDamage(const IEntity* victimEntity, bool offenderIsVehicle, const CGameRules::SCollisionHitInfo& colHitInfo, float damage);
	float AdjustVehicleActorCollisionDamage(IVehicle* pOffenderVehicle, IActor* pVictimActor, const CGameRules::SCollisionHitInfo& colHitInfo, float damage);

	void DelegateServerHit(IScriptTable* victimScript, const HitInfo& hit, CActor* pVictimActor, const SReactionInfoOnHit* pReactionInfo = NULL);

	bool ShouldApplyMercyTime(CActor& localClientActor, const HitInfo& hitInfo) const;

	struct EntityCollisionRecord
	{
		EntityCollisionRecord() { entityID = 0; time = 0.0f; }
		EntityCollisionRecord(EntityId _entityID, float _time) : entityID(_entityID), time(_time) { }

		EntityId entityID;
		float time;
	};

	typedef std::vector<const IEntityClass*>	TEntityClassesTrackedForAI;
	TEntityClassesTrackedForAI	m_entityClassesWithTrackedMovement;

	typedef std::unordered_map<EntityId, EntityCollisionRecord, stl::hash_uint32> EntityCollisionRecords;
	EntityCollisionRecords m_entityCollisionRecords;

	CMercyTimeFilter m_mercyTimeFilter;
	CInvulnerableFilter  m_invulnerableFilter;

	float m_entityLastDamageUpdateTimer;

#ifndef _RELEASE
	struct FakeHitDebug
	{
		FakeHitDebug()
		{
		}

		FakeHitDebug(const CTimeValue& timeValue)
			: time(timeValue)
		{
		}

		CTimeValue time;
	};
	std::vector<FakeHitDebug> m_fakeHits;

	void DrawFakeHits(float frameTime);
#endif
};

#endif // GameRulesSPDamageHandling_h