// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "GameObjects/GameObject.h"
#include "WorldQuery.h"
#include "CryAction.h"
#include "IActorSystem.h"
#include "IViewSystem.h"
#include "IMovementController.h"

static const float AROUND_X = 2.0f;
static const float AROUND_Y = 2.0f;
static const float AROUND_Z = 2.0f;

#define PIERCE_GLASS                     (13)
#define ENTITY_GRID_PROXIMITY_MULTIPLIER 2

CWorldQuery::UpdateQueryFunction CWorldQuery::m_updateQueryFunctions[] =
{
	&CWorldQuery::UpdateRaycastQuery,
	&CWorldQuery::UpdateProximityQuery,
	&CWorldQuery::UpdateInFrontOfQuery,
	&CWorldQuery::UpdateBackRaycastQuery,
	&CWorldQuery::UpdateEntitiesAround,
	&CWorldQuery::UpdatePhysicalEntitiesAround,
	&CWorldQuery::UpdatePhysicalEntityInFrontOf,
};

namespace
{
class CCompareEntitiesByDistanceFromPoint
{
public:
	CCompareEntitiesByDistanceFromPoint(const Vec3& point) : m_point(point), m_pEntitySystem(gEnv->pEntitySystem) {}

	bool operator()(IEntity* pEnt0, IEntity* pEnt1) const
	{
		float distSq0 = (pEnt0->GetPos() - m_point).GetLengthSquared();
		float distSq1 = (pEnt1->GetPos() - m_point).GetLengthSquared();
		return distSq0 < distSq1;
	}

private:
	Vec3           m_point;
	IEntitySystem* m_pEntitySystem;
};

}

CWorldQuery::CWorldQuery()
	: m_physicalEntityInFrontOf(-100)
{
	m_worldPosition = Vec3(ZERO);
	m_dir = Vec3(0, 1, 0);
	m_pActor = NULL;
	m_lookAtEntityId = 0;
	m_validQueries = 0;
	m_proximityRadius = 5.0f;
	m_pEntitySystem = gEnv->pEntitySystem;
	m_pPhysWorld = gEnv->pPhysicalWorld;
	m_pViewSystem = CCryAction::GetCryAction()->GetIViewSystem();
	m_rayHitAny = false;
	m_backRayHitAny = false;
	m_renderFrameId = -1;
	m_rayHitPierceable.dist = -1.0f;

#if WORLDQUERY_USE_DEFERRED_LINETESTS
	m_timeLastDeferredResult = 0.0f;
	m_requestCounter = 0;
#endif
}

CWorldQuery::~CWorldQuery()
{
#if WORLDQUERY_USE_DEFERRED_LINETESTS
	for (int i = 0; i < kMaxQueuedRays; ++i)
	{
		m_queuedRays[i].Reset();
	}
#endif
}

bool CWorldQuery::Init(IGameObject* pGameObject)
{
	SetGameObject(pGameObject);
	m_pActor = CCryAction::GetCryAction()->GetIActorSystem()->GetActor(GetEntityId());
	if (!m_pActor)
	{
		GameWarning("WorldQuery extension only available for actors");
		return false;
	}
	return true;
}

void CWorldQuery::PostInit(IGameObject* pGameObject)
{
	pGameObject->EnableUpdateSlot(this, 0);
}

bool CWorldQuery::ReloadExtension(IGameObject* pGameObject, const SEntitySpawnParams& params)
{
	ResetGameObject();

	CRY_ASSERT_MESSAGE(false, "CWorldQuery::ReloadExtension not implemented");

	return false;
}

void CWorldQuery::Release()
{
	delete this;
}

void CWorldQuery::FullSerialize(TSerialize ser)
{
	if (ser.GetSerializationTarget() == eST_Network)
		return;

	ser.Value("proximityRadius", m_proximityRadius);
	m_validQueries = 0;

	if (GetISystem()->IsSerializingFile() == 1)
	{
		m_pActor = CCryAction::GetCryAction()->GetIActorSystem()->GetActor(GetEntityId());
		CRY_ASSERT(m_pActor);
	}
}

void CWorldQuery::Update(SEntityUpdateContext& ctx, int slot)
{
	//m_validQueries = 0;
	//m_renderFrameId = gEnv->pRenderer->GetFrameID();

	if (!m_pActor)
		return;

	IMovementController* pMC = (m_pActor != NULL) ? m_pActor->GetMovementController() : NULL;
	if (pMC)
	{
		SMovementState s;
		pMC->GetMovementState(s);
		m_worldPosition = s.eyePosition;
		m_dir = s.eyeDirection;
	}
}

void CWorldQuery::UpdateRaycastQuery()
{
	if (!m_pActor || m_pActor->GetEntity()->IsHidden())
		return;

	IEntity* pEntity = m_pActor->GetEntity();
	IPhysicalEntity* pPhysEnt = pEntity ? pEntity->GetPhysics() : NULL;

	static const int obj_types = ent_all; // ent_terrain|ent_static|ent_rigid|ent_sleeping_rigid|ent_living;
	static const unsigned int flags = rwi_pierceability(PIERCE_GLASS) | rwi_colltype_any;

#if WORLDQUERY_USE_DEFERRED_LINETESTS
	IPhysicalEntity* skipEntities[1];
	skipEntities[0] = pPhysEnt;

	int raySlot = GetRaySlot();
	CRY_ASSERT(raySlot != -1);

	m_queuedRays[raySlot].rayId = CCryAction::GetCryAction()->GetPhysicQueues().GetRayCaster().Queue(
	  RayCastRequest::HighPriority,
	  RayCastRequest(m_worldPosition, 300.0f * m_dir,
	                 obj_types,
	                 flags,
	                 skipEntities,
	                 pPhysEnt ? 1 : 0, 2),
	  functor(*this, &CWorldQuery::OnRayCastDataReceived));

	m_queuedRays[raySlot].counter = ++m_requestCounter;

	static const float k_maxTimeRaycastStaysValid = 0.1f;

	const float frameStartTime = gEnv->pTimer->GetCurrTime();
	const float timeSinceLastDeferred = (frameStartTime - m_timeLastDeferredResult);
	if (timeSinceLastDeferred > k_maxTimeRaycastStaysValid)
	{
		// it's been too long since the last deferred raycast result assume hit nothing
		m_rayHitAny = false;
		m_lookAtEntityId = 0;
		//CryLogAlways("CWorldQuery::UpdateRaycastQuery() - frameStartTime=%f; it's been too long (%f) since the last deferred raycast result, assuming hit nothing\n", frameStartTime, timeSinceLastDeferred);
	}
#else // WORLDQUERY_USE_DEFERRED_LINETESTS
	m_rayHitAny = 0 != m_pPhysWorld->RayWorldIntersection(m_worldPosition, 300.0f * m_dir, obj_types, flags, &m_rayHit, 1, pPhysEnt);
	if (m_rayHitAny)
	{
		IEntity* pLookAt = m_pEntitySystem->GetEntityFromPhysics(m_rayHit.pCollider);
		if (pLookAt)
		{
			m_lookAtEntityId = pLookAt->GetId();
			//m_rayHit.pCollider=0; // -- commented until a better solution is thought of.
		}
		else
			m_lookAtEntityId = 0;
	}
	else
		m_lookAtEntityId = 0;

	//	if (m_rayHitAny)
	//		CryLogAlways( "HIT: terrain:%i distance:%f (%f,%f,%f)", m_rayHit.bTerrain, m_rayHit.dist, m_rayHit.pt.x, m_rayHit.pt.y, m_rayHit.pt.z );
#endif // WORLDQUERY_USE_DEFERRED_LINETESTS
}

void CWorldQuery::UpdateBackRaycastQuery()
{
	if (!m_pActor)
		return;
	IEntity* pEntity = m_pEntitySystem->GetEntity(m_pActor->GetEntityId());
	IPhysicalEntity* pPhysEnt = pEntity ? pEntity->GetPhysics() : NULL;

	static const int obj_types = ent_all; // ent_terrain|ent_static|ent_rigid|ent_sleeping_rigid|ent_living;
	static const unsigned int flags = rwi_stop_at_pierceable | rwi_colltype_any;
	m_backRayHitAny = 0 != m_pPhysWorld->RayWorldIntersection(m_worldPosition, -100.0f * m_dir, obj_types, flags, &m_backRayHit, 1, pPhysEnt);
}

void CWorldQuery::UpdateProximityQuery()
{
	EntityId ownerActorId = m_pActor ? m_pActor->GetEntityId() : 0;

	m_proximity.resize(0);

	SEntityProximityQuery qry;
	const float entityGridProxRadius = m_proximityRadius * ENTITY_GRID_PROXIMITY_MULTIPLIER;
	qry.box = AABB(Vec3(m_worldPosition.x - entityGridProxRadius, m_worldPosition.y - entityGridProxRadius, m_worldPosition.z - entityGridProxRadius),
	               Vec3(m_worldPosition.x + entityGridProxRadius, m_worldPosition.y + entityGridProxRadius, m_worldPosition.z + entityGridProxRadius));

	m_pEntitySystem->QueryProximity(qry);
	m_proximity.reserve(qry.nCount);
	for (int i = 0; i < qry.nCount; i++)
	{
		IEntity* pEntity = qry.pEntities[i];
		EntityId entityId = pEntity ? pEntity->GetId() : 0;

		//skip the user
		if (!entityId || (entityId == ownerActorId))
			continue;

		const Vec3 entityWorldPos = pEntity->GetWorldPos();

		//Check height, entity grid is 2D
		const float heightThreshold = m_proximityRadius * 1.2f;
		if (fabs_tpl(entityWorldPos.z - m_worldPosition.z) > heightThreshold)
			continue;

		//Check flat (2D) distance
		AABB aabb;
		pEntity->GetWorldBounds(aabb);
		aabb.min.z = aabb.max.z = m_worldPosition.z;

		const float flatDistanceSqr = (m_proximityRadius * m_proximityRadius);
		if (aabb.GetDistanceSqr(m_worldPosition) > flatDistanceSqr)
			continue;

		m_proximity.push_back(entityId);
	}
}

void CWorldQuery::UpdateInFrontOfQuery()
{
	EntityId ownerActorId = m_pActor ? m_pActor->GetEntityId() : 0;

	m_inFrontOf.resize(0);
	Lineseg lineseg(m_worldPosition, m_worldPosition + m_proximityRadius * m_dir);

	SEntityProximityQuery qry;
	//qry.pEntityClass=0;
	qry.box = AABB(Vec3(m_worldPosition.x - m_proximityRadius, m_worldPosition.y - m_proximityRadius, m_worldPosition.z - m_proximityRadius),
	               Vec3(m_worldPosition.x + m_proximityRadius, m_worldPosition.y + m_proximityRadius, m_worldPosition.z + m_proximityRadius));
	m_pEntitySystem->QueryProximity(qry);
	int n = qry.nCount;
	m_inFrontOf.reserve(n);

	IEntity* pEntity;

	for (int i = 0; i < n; ++i)
	{
		pEntity = qry.pEntities[i];

		//skip the user
		if (!pEntity || (pEntity->GetId() == ownerActorId))
			continue;

		AABB bbox;
		pEntity->GetLocalBounds(bbox);
		if (!bbox.IsEmpty())
		{
			OBB obb(OBB::CreateOBBfromAABB(Matrix33(pEntity->GetWorldTM()), bbox));
			if (Overlap::Lineseg_OBB(lineseg, pEntity->GetWorldPos(), obb))
				m_inFrontOf.push_back(pEntity->GetId());
		}
	}
}

void CWorldQuery::HandleEvent(const SGameObjectEvent&)
{
}

void CWorldQuery::GetMemoryUsage(ICrySizer* pSizer) const
{
	pSizer->AddObject(this, sizeof(*this));
	//pSizer->AddObject(m_PhysEntAroundOf);
	pSizer->AddObject(m_proximity);
	pSizer->AddObject(m_inFrontOf);
	pSizer->AddObject(m_EntAroundOf);
}

void CWorldQuery::UpdatePhysicalEntitiesAround()
{
	if (!m_pActor)
	{
		m_PhysEntAroundOf.resize(0);
		return;
	}
	Vec3 checkOffset(AROUND_X, AROUND_Y, AROUND_Z);
	//
	IPhysicalEntity** ppList = NULL;
	int numEntities = gEnv->pPhysicalWorld->GetEntitiesInBox(m_worldPosition - checkOffset, m_worldPosition + checkOffset, ppList, ent_static | ent_sleeping_rigid | ent_rigid | ent_living);
	m_PhysEntAroundOf.resize(numEntities);
	for (int i = 0; i < numEntities; ++i)
	{
		m_PhysEntAroundOf[i] = ppList[i];
	}
}

void CWorldQuery::UpdatePhysicalEntityInFrontOf()
{
	const int objectTypes = ent_static | ent_sleeping_rigid | ent_rigid | ent_living;
	const unsigned int flags = ent_all;

	ray_hit rayHitResult;
	const int numberOfHits = gEnv->pPhysicalWorld->RayWorldIntersection(m_worldPosition, m_proximityRadius * m_dir, objectTypes, flags, &rayHitResult, 1, (IPhysicalEntity*)0);

	m_physicalEntityInFrontOf = -100; // Anton said it's a safe value to initialise my var with

	if (numberOfHits > 0)
	{
		m_physicalEntityInFrontOf = gEnv->pPhysicalWorld->GetPhysicalEntityId(rayHitResult.pCollider);
	}
}

void CWorldQuery::UpdateEntitiesAround()
{
	if (!m_pActor)
	{
		m_EntAroundOf.resize(0);
		return;
	}
	//
	Vec3 checkOffset(AROUND_X, AROUND_Y, AROUND_Z);
	//
	SEntityProximityQuery qry;
	qry.box.min = m_worldPosition - checkOffset;
	qry.box.max = m_worldPosition + checkOffset;
	qry.nEntityFlags = ~0;
	gEnv->pEntitySystem->QueryProximity(qry);
	m_EntAroundOf.resize(qry.nCount);
	for (int i = 0; i < qry.nCount; i++)
	{
		m_EntAroundOf[i] = qry.pEntities[i]->GetId();
	}
	// add to ent list if available in phys list
	int physEntListSize = 0;
	IPhysicalEntity* const* physEntList = this->GetPhysicalEntitiesAround(physEntListSize);
	for (size_t n = 0; n < physEntListSize; n++)
	{
		IEntity* ent = gEnv->pEntitySystem->GetEntityFromPhysics(physEntList[n]);
		if (ent)
		{
			if (std::find(m_EntAroundOf.begin(), m_EntAroundOf.end(), ent->GetId()) == m_EntAroundOf.end())
			{
				m_EntAroundOf.push_back(ent->GetId());
			}
		}
	}
}

#if WORLDQUERY_USE_DEFERRED_LINETESTS
//async raycasts results callback
void CWorldQuery::OnRayCastDataReceived(const QueuedRayID& rayID, const RayCastResult& result)
{
	int raySlot = GetSlotForRay(rayID);
	CRY_ASSERT(raySlot != -1);

	if (raySlot != -1)
	{
		m_queuedRays[raySlot].rayId = 0;
		m_queuedRays[raySlot].counter = 0;
	}

	// these need setting
	// m_rayHitAny
	// m_lookAtEntityId
	const bool rayHitSucced = (result.hitCount > 0);
	EntityId entityIdHit = 0;

	m_rayHitPierceable.dist = -1.f;

	if (rayHitSucced)
	{
		// only because the raycast is being done with  rwi_stop_at_pierceable can we rely on there being only 1
		// hit. Otherwise hit[0] is always the solid hit (and thus the last), hit[1-n] are then first to last-1 in order of distance
		IEntity* pEntity = gEnv->pEntitySystem->GetEntityFromPhysics(result.hits[0].pCollider);

		//Check if it's the child or not
		int partId = result.hits[0].partid;
		if (partId && pEntity)
		{
			pEntity = pEntity->UnmapAttachedChild(partId);
		}

		if (pEntity)
		{
			entityIdHit = pEntity->GetId();
		}

		m_rayHitSolid = result.hits[0];

		//It is not safe to access this, since the result can be kept multiple frames
		//Yet other data of the hit can be useful, and the entity Id is also remembered.
		m_rayHitSolid.pCollider = NULL;
		m_rayHitSolid.next = NULL;

		if (result.hitCount > 1)
		{
			m_rayHitPierceable = result.hits[1];
			m_rayHitPierceable.pCollider = NULL;
			m_rayHitPierceable.next = NULL;
		}
	}

	m_timeLastDeferredResult = gEnv->pTimer->GetCurrTime();

	m_rayHitAny = rayHitSucced;
	m_lookAtEntityId = entityIdHit;
}

int CWorldQuery::GetRaySlot()
{
	for (int i = 0; i < kMaxQueuedRays; ++i)
	{
		if (m_queuedRays[i].rayId == 0)
		{
			return i;
		}
	}

	//Find out oldest request and remove it
	uint32 oldestCounter = m_requestCounter + 1;
	int oldestSlot = 0;
	for (int i = 0; i < kMaxQueuedRays; ++i)
	{
		if (m_queuedRays[i].counter < oldestCounter)
		{
			oldestCounter = m_queuedRays[i].counter;
			oldestSlot = i;
		}
	}

	m_queuedRays[oldestSlot].Reset();

	return oldestSlot;
}

int CWorldQuery::GetSlotForRay(const QueuedRayID& rayId) const
{
	for (int i = 0; i < kMaxQueuedRays; ++i)
	{
		if (m_queuedRays[i].rayId == rayId)
		{
			return i;
		}
	}

	return -1;
}

#endif
