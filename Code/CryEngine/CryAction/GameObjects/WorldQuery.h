// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#ifndef __WORLDQUERY_H__
#define __WORLDQUERY_H__

#pragma once

#include "IWorldQuery.h"

#if 1
	#define WORLDQUERY_USE_DEFERRED_LINETESTS 1
#else // deprecated and won't compile
	#define WORLDQUERY_USE_DEFERRED_LINETESTS 0
#endif

#if WORLDQUERY_USE_DEFERRED_LINETESTS
	#include "CryActionPhysicQueues.h"
#endif

struct IActor;
struct IViewSystem;

class CWorldQuery : public CGameObjectExtensionHelper<CWorldQuery, IWorldQuery>
{
private:

#if WORLDQUERY_USE_DEFERRED_LINETESTS
	struct SRayInfo
	{
		SRayInfo()
			: rayId(0)
			, counter(0)
		{

		}

		void Reset()
		{
			if (rayId != 0)
			{
				CCryAction::GetCryAction()->GetPhysicQueues().GetRayCaster().Cancel(rayId);
				rayId = 0;
			}
			counter = 0;
		}

		QueuedRayID rayId;
		uint32      counter;
	};
#endif

public:
	CWorldQuery();
	~CWorldQuery();

	// IGameObjectExtension
	virtual bool                 Init(IGameObject* pGameObject);
	virtual void                 InitClient(int channelId)                                                       {};
	virtual void                 PostInit(IGameObject* pGameObject);
	virtual void                 PostInitClient(int channelId)                                                   {};
	virtual bool                 ReloadExtension(IGameObject* pGameObject, const SEntitySpawnParams& params);
	virtual void                 PostReloadExtension(IGameObject* pGameObject, const SEntitySpawnParams& params) {}
	virtual void                 Release();
	virtual void                 FullSerialize(TSerialize ser);
	virtual bool                 NetSerialize(TSerialize ser, EEntityAspects aspect, uint8 profile, int flags) { return true; }
	virtual void                 PostSerialize()                                                               {}
	virtual void                 SerializeSpawnInfo(TSerialize ser)                                            {}
	virtual ISerializableInfoPtr GetSpawnInfo()                                                                { return 0; }
	virtual void                 Update(SEntityUpdateContext& ctx, int slot);
	virtual void                 HandleEvent(const SGameObjectEvent&);
	virtual void                 ProcessEvent(const SEntityEvent&) {}
	virtual uint64               GetEventMask() const { return 0; }
	virtual void                 SetChannelId(uint16 id)     {};
	virtual void                 PostUpdate(float frameTime) { CRY_ASSERT(false); }
	virtual void                 PostRemoteSpawn()           {};
	virtual void                 GetMemoryUsage(ICrySizer* pSizer) const;
	// ~IGameObjectExtension

	ILINE void SetProximityRadius(float n)
	{
		m_proximityRadius = n;
		m_validQueries &= ~(eWQ_Proximity | eWQ_InFrontOf);
	}

	ILINE const ray_hit* RaycastQuery()
	{
		return GetLookAtPoint(m_proximityRadius);
	}

	ILINE const ray_hit* GetLookAtPoint(const float fMaxDist = 0, bool ignoreGlass = false)
	{
		ValidateQuery(eWQ_Raycast);

		if (m_rayHitAny)
		{
			const ray_hit* hit = !ignoreGlass && m_rayHitPierceable.dist >= 0.f ? &m_rayHitPierceable : &m_rayHitSolid;

			if ((fMaxDist <= 0) || (hit->dist <= fMaxDist))
			{
				return hit;
			}
		}
		return NULL;
	}

	ILINE const ray_hit* GetBehindPoint(const float fMaxDist = 0)
	{
		ValidateQuery(eWQ_BackRaycast);
		if (m_backRayHitAny)
		{
			if ((fMaxDist > 0) && (m_backRayHit.dist <= fMaxDist))
			{
				return &m_backRayHit;
			}
		}
		return NULL;
	}

	ILINE const EntityId GetLookAtEntityId(bool ignoreGlass = false)
	{
		ValidateQuery(eWQ_Raycast);

		return !ignoreGlass && m_rayHitPierceable.dist >= 0.f ? 0 : m_lookAtEntityId;
	}

	ILINE const EntityId* ProximityQuery(int& numberOfEntities)
	{
		ValidateQuery(eWQ_Proximity);
		numberOfEntities = m_proximity.size();
		return numberOfEntities ? &m_proximity[0] : 0;
	}

	ILINE const Entities& ProximityQuery()
	{
		ValidateQuery(eWQ_Proximity);
		return m_proximity;
	}

	ILINE const Entities& InFrontOfQuery()
	{
		ValidateQuery(eWQ_InFrontOf);
		return m_inFrontOf;
	}
	ILINE IEntity* GetEntityInFrontOf()
	{
		const Entities& entities = InFrontOfQuery();
		if (entities.empty())
			return NULL;
		else
			return m_pEntitySystem->GetEntity(entities[0]);
	}
	ILINE const Entities& GetEntitiesInFrontOf()
	{
		return InFrontOfQuery();
	}

	ILINE const EntityId* GetEntitiesAround(int& num)
	{
		ValidateQuery(eWQ_EntitiesAround);
		num = m_EntAroundOf.size();
		return num ? &m_EntAroundOf[0] : 0;
	}

	ILINE IPhysicalEntity* const* GetPhysicalEntitiesAround(int& num)
	{
		ValidateQuery(eWQ_PhysicalEntitiesAround);
		num = m_PhysEntAroundOf.size();
		return num ? &m_PhysEntAroundOf[0] : 0;
	}

	ILINE IPhysicalEntity* GetPhysicalEntityInFrontOf()
	{
		ValidateQuery(eWQ_PhysicalEntitiesInFrontOf);

		return gEnv->pPhysicalWorld->GetPhysicalEntityById(m_physicalEntityInFrontOf);
	}

	ILINE const Vec3& GetPos() const { return m_worldPosition; }
	ILINE const Vec3& GetDir() const { return m_dir; }

#if WORLDQUERY_USE_DEFERRED_LINETESTS
	void OnRayCastDataReceived(const QueuedRayID& rayID, const RayCastResult& result);
#endif

private:
	uint32 m_validQueries;
	int    m_renderFrameId;

	float  m_proximityRadius;

	// keep in sync with m_updateQueryFunctions
	enum EWorldQuery
	{
		eWQ_Raycast = 0,
		eWQ_Proximity,
		eWQ_InFrontOf,
		eWQ_BackRaycast,
		eWQ_EntitiesAround,
		eWQ_PhysicalEntitiesAround,
		eWQ_PhysicalEntitiesInFrontOf,
	};
	typedef void (CWorldQuery::* UpdateQueryFunction)();

	Vec3                       m_worldPosition;
	Vec3                       m_dir;
	IActor*                    m_pActor;
	static UpdateQueryFunction m_updateQueryFunctions[];

	IPhysicalWorld*            m_pPhysWorld;
	IEntitySystem*             m_pEntitySystem;
	IViewSystem*               m_pViewSystem;

#if WORLDQUERY_USE_DEFERRED_LINETESTS
	int GetRaySlot();
	int GetSlotForRay(const QueuedRayID& rayId) const;

	static const int kMaxQueuedRays = 6;
	SRayInfo         m_queuedRays[kMaxQueuedRays];
	uint32           m_requestCounter;
	float            m_timeLastDeferredResult;
#endif

	// ray-cast query
	bool    m_rayHitAny;
	ray_hit m_rayHitSolid;
	ray_hit m_rayHitPierceable;

	// back raycast query
	bool    m_backRayHitAny;
	ray_hit m_backRayHit;

	//the entity the object is currently looking at...
	EntityId m_lookAtEntityId;

	// proximity query
	Entities                      m_proximity;
	// "in-front-of" query
	Entities                      m_inFrontOf;

	Entities                      m_EntAroundOf;
	std::vector<IPhysicalEntity*> m_PhysEntAroundOf;

	int                           m_physicalEntityInFrontOf;

	ILINE void ValidateQuery(EWorldQuery query)
	{
		uint32 queryMask = 1u << query;

		int frameid = gEnv->nMainFrameID;
		if (m_renderFrameId != frameid)
		{
			m_renderFrameId = frameid;
			m_validQueries = 0;
		}
		else
		{
			if (m_validQueries & queryMask)
				return;
		}
		(this->*(m_updateQueryFunctions[query]))();
		m_validQueries |= queryMask;
	}

	void UpdateRaycastQuery();
	void UpdateProximityQuery();
	void UpdateInFrontOfQuery();
	void UpdateBackRaycastQuery();
	void UpdateEntitiesAround();
	void UpdatePhysicalEntitiesAround();
	void UpdatePhysicalEntityInFrontOf();
};

#endif
