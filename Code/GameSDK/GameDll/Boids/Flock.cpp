// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

// -------------------------------------------------------------------------
//  File name:   flock.cpp
//  Version:     v1.00
//  Created:     5/4/2002 by Timur.
//  Compilers:   Visual C++ 7.0
//  Description:
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////

#include "StdAfx.h"
#include "Flock.h"
#include "BugsFlock.h"

#include <CryEntitySystem/IEntitySystem.h>
#include "BoidsProxy.h"
#include "GameCache.h"

#include <float.h>
#include <limits.h>
#include <CrySystem/ITimer.h>
#include <CryScriptSystem/IScriptSystem.h>
#include <CryAnimation/ICryAnimation.h>
#include <CryMath/Cry_Camera.h>
#include <CryString/CryPath.h>
#include <IPerceptionManager.h>

#define  PHYS_FOREIGN_ID_BOID PHYS_FOREIGN_ID_USER - 1

#define MAX_SPEED             15
#define MIN_SPEED             2.5f

#define ALIGNMENT_FACTOR      1.0f
#define COHESION_FACTOR       1.0f
#define SEPARATION_FACTOR     10.0f

#define ORIGIN_ATTRACT_FACTOR 0.1f
#define DESIRED_HEIGHT_FACTOR 0.4f
#define AVOID_LAND_FACTOR     10.0f

#define MAX_ANIMATION_SPEED   1.7f

// previously hard-coded value in CBoidBird::ClampSpeed()
#define WALK_TO_IDLE_DURATION 3.0f

// previously hard-coded values in CBoidBird::UpdateOnGroundAction()
#define ON_GROUND_WALK_DURATION_MIN 4.0f
#define ON_GROUND_WALK_DURATION_MAX 6.0f
#define ON_GROUND_IDLE_DURATION_MIN 1.6f
#define ON_GROUND_IDLE_DURATION_MAX 3.2f

int CFlock::m_e_flocks = 1;
int CFlock::m_e_flocks_hunt = 1;

//////////////////////////////////////////////////////////////////////////
CFlock::CFlock(IEntity* pEntity, EFlockType flockType)
{
	m_nViewDistRatio = 100;
	m_pEntity = pEntity;
	m_bEnabled = true;

	m_updateFrameID = 0;
	m_percentEnabled = 100;

	m_type = flockType;

	m_bounds.min = Vec3(-1, -1, -1);
	m_bounds.max = Vec3(1, 1, 1);

	m_origin = m_pEntity->GetWorldPos();
	m_fCenterFloatingTime = 0.0f;

	m_nBoidEntityFlagsAdd = 0;

	GetDefaultBoidsContext(m_bc);

	m_bc.engine = gEnv->p3DEngine;
	m_bc.physics = gEnv->pPhysicalWorld;
	m_bc.waterLevel = m_bc.engine->GetWaterLevel(&m_origin);
	m_bc.fBoidMass = 1;
	m_bc.fBoidRadius = 1;
	m_bc.fBoidThickness = 1;
	m_bc.flockPos = m_origin;
	m_bc.randomFlockCenter = m_origin;
	m_bc.factorRandomAccel = 0;
	m_bc.boidScale = 5;

	m_bc.scarePoint = Vec3(0, 0, 0);
	m_bc.scareRadius = 0;
	m_bc.scareRatio = 0;
	m_bc.scareThreatLevel = 0;

	m_bc.fSmoothFactor = 0;
	m_bc.fMaxTurnRatio = 0;

	m_bEntityCreated = false;
	m_bAnyKilled = false;

	m_bAIEventListenerRegistered = false;
}

//////////////////////////////////////////////////////////////////////////
void CFlock::GetDefaultBoidsContext(SBoidContext& bc)
{
	static SBoidContext zeroed;
	bc = zeroed;

	bc.MinHeight = MIN_FLIGHT_HEIGHT;
	bc.MaxHeight = MAX_FLIGHT_HEIGHT;

	bc.MaxAttractDistance = MAX_ATTRACT_DISTANCE;
	bc.MinAttractDistance = MIN_ATTRACT_DISTANCE;

	bc.MaxSpeed = MAX_SPEED;
	bc.MinSpeed = MIN_SPEED;

	bc.factorAlignment = ALIGNMENT_FACTOR;
	bc.factorCohesion = COHESION_FACTOR;
	bc.factorSeparation = SEPARATION_FACTOR;

	bc.factorAttractToOrigin = ORIGIN_ATTRACT_FACTOR;
	bc.factorKeepHeight = DESIRED_HEIGHT_FACTOR;
	bc.factorAvoidLand = AVOID_LAND_FACTOR;

	bc.MaxAnimationSpeed = MAX_ANIMATION_SPEED;

	bc.followPlayer = false;
	bc.avoidObstacles = true;
	bc.noLanding = false;
	bc.bAvoidWater = true;

	bc.cosFovAngle = cos_tpl(125.0f * gf_PI / 180.0f);
	bc.maxVisibleDistance = 300;

	bc.boidScale = 5;

	bc.fSmoothFactor = 0;
	bc.fMaxTurnRatio = 0;
	bc.vEntitySlotOffset.Set(0, 0, 0);

	bc.attractionPoint.Set(0, 0, 0);

	bc.fWalkToIdleDuration = WALK_TO_IDLE_DURATION;

	bc.fOnGroundIdleDurationMin = ON_GROUND_IDLE_DURATION_MIN;
	bc.fOnGroundIdleDurationMax = ON_GROUND_IDLE_DURATION_MAX;

	bc.fOnGroundWalkDurationMin = ON_GROUND_WALK_DURATION_MIN;
	bc.fOnGroundWalkDurationMax = ON_GROUND_WALK_DURATION_MAX;
}

CFlock::~CFlock()
{
	ClearBoids();
	RegisterAIEventListener(false);
}

//////////////////////////////////////////////////////////////////////////
void CFlock::ClearBoids()
{
	I3DEngine* engine = gEnv->p3DEngine;

	DeleteEntities(true);
	for (Boids::iterator it = m_boids.begin(); it != m_boids.end(); ++it)
	{
		CBoidObject* boid = *it;
		delete boid;
	}
	m_boids.clear();
	m_BoidCollisionMap.clear();

}

void CFlock::DeleteEntities(bool bForceDeleteAll)
{
	if (m_pEntity)
	{
		m_pEntity->ClearSlots();
	}

	I3DEngine* engine = gEnv->p3DEngine;
	for (Boids::iterator it = m_boids.begin(); it != m_boids.end(); ++it)
	{
		CBoidObject* boid = *it;

		if (boid->m_pickedUp && !bForceDeleteAll) // Do not delete picked up boids.
			continue;

		//		SAFE_RELEASE(boid->m_object);
		boid->m_object = 0;
		boid->m_noentity = true;
		if (boid->m_entity)
		{
			// Delete entity.

			IEntity* pBoidEntity = gEnv->pEntitySystem->GetEntity(boid->m_entity);
			if (pBoidEntity)
			{
				CBoidObjectProxy* pBoidObjectProxy = static_cast<CBoidObjectProxy*>(pBoidEntity->GetProxy(ENTITY_PROXY_BOID_OBJECT));

				if (pBoidObjectProxy)
					pBoidObjectProxy->SetBoid(0);
			}

			gEnv->pEntitySystem->RemoveEntity(boid->m_entity);

			boid->m_entity = 0;
			boid->m_pPhysics = 0;
			boid->m_physicsControlled = false;
		}
	}
	m_bEntityCreated = false;
}
//////////////////////////////////////////////////////////////////////////
void CFlock::AddBoid(CBoidObject* boid)
{
	boid->m_flock = this;
	m_boids.push_back(boid);
}

//////////////////////////////////////////////////////////////////////////
bool CFlock::IsFlockActive()
{
	if (!m_bEnabled)
		return false;

	if (m_percentEnabled <= 0)
		return false;

	if (m_bc.followPlayer)
		return true;

	float d = m_bc.maxVisibleDistance;
	float dist = m_origin.GetSquaredDistance(GetISystem()->GetViewCamera().GetMatrix().GetTranslation());
	if (d * d < dist)
	{
		if (m_bEntityCreated)
			DeleteEntities(false);

		return false;
	}

	return true;
}

//////////////////////////////////////////////////////////////////////////
void CFlock::SetEnabled(bool bEnabled)
{
	if (m_bEnabled != bEnabled)
	{
		m_bEnabled = bEnabled;
		if (!bEnabled && m_bEntityCreated)
			DeleteEntities(false);
	}
}

//////////////////////////////////////////////////////////////////////////
void CFlock::SetPercentEnabled(int percent)
{
	if (percent < 0)
		percent = 0;
	if (percent > 100)
		percent = 100;

	m_percentEnabled = percent;
}

//////////////////////////////////////////////////////////////////////////
void CFlock::UpdateBoidsViewDistRatio()
{
	m_nViewDistRatio = m_pEntity->GetRenderNodeParams().viewDistRatio;

	for (Boids::iterator it = m_boids.begin(); it != m_boids.end(); ++it)
	{
		CBoidObject* boid = *it;
		IEntity* pBoidEntity = gEnv->pEntitySystem->GetEntity(boid->m_entity);
		if (pBoidEntity)
		{
			m_pEntity->SetViewDistRatio(m_nViewDistRatio);
		}
	}
}

//////////////////////////////////////////////////////////////////////////
void CFlock::Update(CCamera* pCamera)
{
	CRY_PROFILE_FUNCTION(PROFILE_ENTITY);

	if (!IsFlockActive())
		return;

	if (!m_e_flocks)
	{
		if (m_bEntityCreated)
			DeleteEntities(true);
		return;
	}

	if (GetISystem()->IsSerializingFile() == 1) //quickloading
		return;

	if (!m_bEntityCreated)
	{
		if (!CreateEntities())
			return;
	}

	float dt = gEnv->pTimer->GetFrameTime();
	// Make sure delta time is limited.
	if (dt > 1.0f)
		dt = 0.01f;
	if (dt > 0.1f)
		dt = 0.1f;

	m_bc.fSmoothFactor = 1.f - gEnv->pTimer->GetProfileFrameBlending();
	/*
	   for (Boids::iterator it = m_boids.begin(); it != m_boids.end(); ++it)
	   {
	   CBoidObject *boid = *it;
	   boid->Think();
	   }
	 */
	//m_bc.playerPos = m_flockMgr->GetPlayerPos();

	m_bc.playerPos = GetISystem()->GetViewCamera().GetMatrix().GetTranslation(); // Player position is position of camera.
	m_bc.flockPos = m_origin;
	m_bc.waterLevel = m_bc.engine->GetWaterLevel(&m_origin);

	m_bounds.min = Vec3(FLT_MAX, FLT_MAX, FLT_MAX);
	m_bounds.max = Vec3(-FLT_MAX, -FLT_MAX, -FLT_MAX);

	int numBoids = m_boids.size();
	if (m_percentEnabled < 100)
	{
		numBoids = (m_percentEnabled * numBoids) / 100;
	}

	if (!m_pEntity->GetRotation().IsIdentity())
	{
		// Entity matrix must not have rotation.
		//Quat q;
		//q.SetIdentity();
		//m_pEntity->SetRotation(q);
	}

	//////////////////////////////////////////////////////////////////////////
	// Update flock random center.
	//////////////////////////////////////////////////////////////////////////
	m_fCenterFloatingTime += gEnv->pTimer->GetFrameTime();
	float tc = m_fCenterFloatingTime * 0.2f;
	m_bc.randomFlockCenter = m_bc.flockPos +
	                         //m_bc.fSpawnRadius*Vec3(sinf(0.9f*m_fCenterFloatingTime),cosf(1.1f*sin(0.9f*m_fCenterFloatingTime)),0.3f*sinf(1.2f*m_fCenterFloatingTime) );
	                         m_bc.fSpawnRadius * Vec3(sinf(tc * 0.913f) * cosf(tc * 1.12f), sinf(tc * 0.931f) * cosf(tc * 0.971f), 0.4f * sinf(tc * 1.045f) * cosf(tc * 0.962f));
	//gEnv->pAuxGeomRenderer->DrawSphere( m_bc.randomFlockCenter,0.1f,ColorB(255,0,0,255) );

	//////////////////////////////////////////////////////////////////////////

	//////////////////////////////////////////////////////////////////////////
	{
		if (m_pEntity->GetRenderNodeParams().viewDistRatio != m_nViewDistRatio)
			UpdateBoidsViewDistRatio();
	}
	//////////////////////////////////////////////////////////////////////////

	//////////////////////////////////////////////////////////////////////////
	// Update scare factors.
	if (m_bc.scareThreatLevel > 0)
	{
		m_bc.scareThreatLevel *= 0.95f;
		m_bc.scareRatio *= 0.95f;
		if (m_bc.scareRatio < 0.01f)
		{
			m_bc.scareRatio = 0;
			m_bc.scareThreatLevel = 0;
		}
		if (m_e_flocks == 2)
		{
			int c = (int)(255 * m_bc.scareRatio);
			gEnv->pAuxGeomRenderer->DrawSphere(m_bc.scarePoint, m_bc.scareRadius, ColorB(c, 0, 0, c), false);
		}
	}

	//////////////////////////////////////////////////////////////////////////

	UpdateBoidCollisions();

	Vec3 entityPos = m_pEntity->GetWorldPos();
	Matrix34 boidTM;
	int num = 0;
	for (Boids::iterator it = m_boids.begin(); it != m_boids.end(); ++it, num++)
	{
		if (num > numBoids)
			break;

		CBoidObject* boid = *it;

		m_bc.terrainZ = m_bc.engine->GetTerrainElevation(boid->m_pos.x, boid->m_pos.y);
		boid->Update(dt, m_bc);

		if (!boid->m_physicsControlled && !boid->m_dead)
		{
			IEntity* pBoidEntity = gEnv->pEntitySystem->GetEntity(boid->m_entity);
			if (pBoidEntity)
			{
				Quat q(IDENTITY);
				boid->CalcOrientation(q);
				const Vec3 scaleVector(boid->m_scale, boid->m_scale, boid->m_scale);
				pBoidEntity->SetPosRotScale(boid->m_pos, q, scaleVector);
			}
		}

	}

	m_updateFrameID = gEnv->pRenderer->GetFrameID(false);
	//gEnv->pLog->Log( "Birds Update" );
}

//////////////////////////////////////////////////////////////////////////
void CFlock::Render(const SRendParams& EntDrawParams)
{
	/*
	   CRY_PROFILE_FUNCTION(PROFILE_ENTITY );

	   if (!m_e_flocks)
	   return;

	   // Only draw birds flock on the same frame id, as update call.
	   int frameId = gEnv->pRenderer->GetFrameID(false);
	   if (abs(frameId-m_updateFrameID) > 2)
	   return;

	   float d = GetMaxVisibilityDistance();
	   if (d*d < GetSquaredDistance(m_playerPos,GetPos())
	   return;

	   SRendParams rp( EntDrawParams );
	   CCamera &cam = GetISystem()->GetViewCamera();
	   // Check if flock bounding box is visible.
	   if (!cam.IsAABBVisibleFast( AABB(m_bounds.min,m_bounds.max) ))
	   return;

	   int numBoids = m_boids.size();
	   if (m_percentEnabled < 100)
	   {
	   numBoids = (m_percentEnabled*numBoids)/100;
	   }

	   int num = 0;
	   for (Boids::iterator it = m_boids.begin(); it != m_boids.end(); ++it)
	   {
	   if (num++ > numBoids)
	    break;

	   CBoidObject *boid = *it;
	   boid->Render(rp,cam,m_bc);
	   }
	   //gEnv->pLog->Log( "Birds Draw" );
	 */
}

//////////////////////////////////////////////////////////////////////////
void CFlock::SetBoidSettings(SBoidContext& bc)
{
	m_bc = bc;
	if (m_bc.MinHeight == 0)
		m_bc.MinHeight = 0.01f;
	RegisterAIEventListener(true);
}

//////////////////////////////////////////////////////////////////////////
void CFlock::SetPos(const Vec3& pos)
{
	Vec3 ofs = pos - m_origin;
	m_origin = pos;
	m_bc.flockPos = m_origin;
	for (Boids::iterator it = m_boids.begin(); it != m_boids.end(); ++it)
	{
		CBoidObject* boid = *it;
		boid->m_pos += ofs;
		boid->OnFlockMove(m_bc);
	}
	// Update bounding box of flock entity.
	if (m_pEntity)
	{
		//float s = m_bc.MaxAttractDistance;
		//		float s = 1;
		//m_pEntity->SetBBox( pos-Vec3(s,s,s),pos+Vec3(s,s,s) );
		//m_pEntity->ForceRegisterInSectors();
	}
	RegisterAIEventListener(true);
}

//////////////////////////////////////////////////////////////////////////
void CFlock::RegisterAIEventListener(bool bEnable)
{
	if (gEnv->bMultiplayer)
		return;

	IPerceptionManager* pPerceptionManager = IPerceptionManager::GetInstance();

	if (!pPerceptionManager)
		return;

	if (bEnable)
	{
		//Re-register with possibly changed parameters
		SAIStimulusEventListenerParams params;
		params.pos = m_bc.flockPos;
		params.radius = m_bc.maxVisibleDistance;
		params.flags = (1 << AISTIM_EXPLOSION) | (1 << AISTIM_SOUND) | (1 << AISTIM_BULLET_HIT);
		pPerceptionManager->RegisterAIStimulusEventListener(functor(*this, &CFlock::OnStimulusReceived), params);
	}
	else if (m_bAIEventListenerRegistered)
	{
		pPerceptionManager->UnregisterAIStimulusEventListener(functor(*this, &CFlock::OnStimulusReceived));
	}

	m_bAIEventListenerRegistered = bEnable;
}

//////////////////////////////////////////////////////////////////////////
bool CFlock::RayTest(Vec3& raySrc, Vec3& rayTrg, SFlockHit& hit)
{
	//	Vec3 v;
	Vec3 p1, p2;
	// Check all boids.
	for (unsigned int i = 0; i < m_boids.size(); i++)
	{
		CBoidObject* boid = m_boids[i];

		Lineseg lseg(raySrc, rayTrg);
		Sphere sphere(boid->m_pos, boid->m_scale);
		if (Intersect::Lineseg_Sphere(lseg, sphere, p1, p2) > 0)
		{
			hit.object = boid;
			hit.dist = (raySrc - p1).GetLength();
			return true;
		}
	}
	return false;
}

//////////////////////////////////////////////////////////////////////////
void CFlock::OnBoidHit(EntityId nBoidId, SmartScriptTable& hit)
{
	int num = (int)m_boids.size();
	for (int i = 0; i < num; i++)
	{
		CBoidObject* boid = m_boids[i];
		if (boid->m_entity == nBoidId)
		{
			if (!boid->m_invulnerable)
			{
				Vec3 pos = boid->m_pos;
				Vec3 force = Vec3(1, 1, 1);
				float damage = 1.0f;
				hit->GetValue("pos", pos);
				hit->GetValue("dir", force);
				hit->GetValue("damage", damage);

				boid->Kill(pos, force * damage);
			}

			break;
		}
	}
}

//////////////////////////////////////////////////////////////////////////
void CFlock::CreateBoids(SBoidsCreateContext& ctx)
{
	m_createContext = ctx;

	ClearBoids();

	if (!m_e_flocks)
		return;

	if (!ctx.models.empty())
		m_model = ctx.models[0];

	static ICVar* e_obj_quality(gEnv->pConsole->GetCVar("e_ObjQuality"));
	int obj_quality = 0;
	if (e_obj_quality)
		obj_quality = e_obj_quality->GetIVal();

	//////////////////////////////////////////////////////////////////////////
	// Scale boids down depended on spec
	//////////////////////////////////////////////////////////////////////////
	if (obj_quality == CONFIG_LOW_SPEC)
	{
		if (ctx.boidsCount > 10)
			ctx.boidsCount /= 4;
		if (ctx.boidsCount > 20)
			ctx.boidsCount = 20;
	}
	if (obj_quality == CONFIG_MEDIUM_SPEC)
	{
		if (ctx.boidsCount > 10)
			ctx.boidsCount /= 2;
	}
	m_RequestedBoidsCount = ctx.boidsCount;
	if (ctx.boidsCount == 0)
		return;
	//////////////////////////////////////////////////////////////////////////
	// precache flock model
	CGameCache& gameCache = g_pGame->GetGameCache();
	if (IsCharacterFile(m_model) || IsStatObjFile(m_model))
	{
		gameCache.CacheGeometry(m_model);
	}

}

//////////////////////////////////////////////////////////////////////////
bool CFlock::CreateEntities()
{
	if (!m_e_flocks)
		return false;

	SEntitySpawnParams spawnParams;
	spawnParams.pClass = gEnv->pEntitySystem->GetClassRegistry()->FindClass("Boid");
	if (!spawnParams.pClass)
		return false;

	spawnParams.nFlags = ENTITY_FLAG_CLIENT_ONLY | ENTITY_FLAG_NO_SAVE | ENTITY_FLAG_NO_PROXIMITY | m_nBoidEntityFlagsAdd;
	spawnParams.sName = m_boidEntityName;

	m_modelCgf = PathUtil::ReplaceExtension(m_model, "cgf");

	bool bHasCgfFile = gEnv->pCryPak->IsFileExist(m_modelCgf) && m_bc.animationMaxDistanceSq > 0;

	if (!bHasCgfFile)
		m_bc.animationMaxDistanceSq = 0;

	bool bAnyCreated = false;

	for (Boids::iterator it = m_boids.begin(); it != m_boids.end(); ++it)
	{
		CBoidObject* boid = *it; /*m_boids[i]*/;

		if (boid->m_dead || boid->m_dying)
			continue;

		spawnParams.vPosition = boid->m_pos;
		spawnParams.vScale = Vec3(boid->m_scale, boid->m_scale, boid->m_scale);
		spawnParams.id = 0;
		IEntity* pBoidEntity = gEnv->pEntitySystem->SpawnEntity(spawnParams);
		if (!pBoidEntity)
		{
			//delete boid;
			//it = m_boids.erase(it);
			continue;
		}
		boid->m_noentity = false;
		boid->m_entity = pBoidEntity->GetId();

		auto pBoidObjectProxy = pBoidEntity->GetOrCreateComponent<CBoidObjectProxy>();
		pBoidObjectProxy->SetBoid(boid);

		// check if character.
		if (IsCharacterFile(m_model))
		{
			if (bHasCgfFile)
				pBoidEntity->LoadGeometry(CBoidObject::eSlot_Cgf, m_modelCgf);

			pBoidEntity->LoadCharacter(CBoidObject::eSlot_Chr, m_model);
			boid->m_object = pBoidEntity->GetCharacter(0);
			if (!boid->m_object)
			{
				gEnv->pEntitySystem->RemoveEntity(boid->m_entity, true);
				boid->m_entity = 0;
				//delete boid;
				//it = m_boids.erase(it);
				continue;
			}

		}
		else
		{
			pBoidEntity->LoadGeometry(0, m_model);
		}

		bAnyCreated = true;

		//		boid->m_object->GetModel()->AddRef();

		AABB aabb;
		if (boid->m_object)
		{
			boid->m_object->SetFlags(CS_FLAG_DRAW_MODEL | CS_FLAG_UPDATE);
			boid->PlayAnimation(m_boidDefaultAnimName, true);
			aabb = boid->m_object->GetAABB();
		}
		else
		{
			pBoidEntity->GetLocalBounds(aabb);
		}

		float fBBoxRadius = ((aabb.max - aabb.min).GetLength() / 2.0f);
		m_bc.fBoidRadius = max(fBBoxRadius, 0.001f);
		m_bc.fBoidThickness = m_bc.fBoidRadius;

		if (!m_bc.vEntitySlotOffset.IsZero())
		{
			pBoidEntity->SetSlotLocalTM(0, Matrix34::CreateTranslationMat(m_bc.vEntitySlotOffset * fBBoxRadius));
		}

		boid->Physicalize(m_bc);

		IEntityRender* pIEntityRender = pBoidEntity->GetRenderInterface();
		if (pIEntityRender != NULL && m_bc.fBoidRadius > 0)
		{
			float r = m_bc.fBoidRadius;
			AABB box;
			box.min = Vec3(-r, -r, -r);
			box.max = Vec3(r, r, r);
			pIEntityRender->SetLocalBounds(box, true);
		}
		IScriptTable* pScriptTable = pBoidEntity->GetScriptTable();
		if (pScriptTable)
		{
			pScriptTable->SetValue("flock_entity", m_pEntity->GetScriptTable());
		}
	}

	m_bEntityCreated = true;
	return bAnyCreated;
}

//////////////////////////////////////////////////////////////////////////
void CFlock::Reset()
{
	if (m_bAnyKilled)
	{
		CreateBoids(m_createContext);
	}
	m_bAnyKilled = false;
}

//////////////////////////////////////////////////////////////////////////
void CFlock::OnStimulusReceived(const SAIStimulusParams& params)
{
	if (m_bc.scareThreatLevel * 1.2f < params.threat)
	{
		m_bc.scareThreatLevel = params.threat;
		m_bc.scarePoint = params.position;
		m_bc.scareRatio = 1.0f;
		m_bc.scareRadius = params.radius;
	}
}

void CFlock::GetMemoryUsage(ICrySizer* pSizer) const
{
	pSizer->AddContainer(m_boids);
	pSizer->AddObject(m_model);
	pSizer->AddObject(m_boidEntityName);
	pSizer->AddObject(m_boidDefaultAnimName);
}

//////////////////////////////////////////////////////////////////////////

void CFlock::UpdateAvgBoidPos(float dt)
{
	m_lastUpdatePosTimePassed += dt;
	if (m_lastUpdatePosTimePassed < 0.5f + Boid::Frand() / 4.f)
		return;

	m_lastUpdatePosTimePassed = 0;

	m_avgBoidPos.zero();
	int n = 0;

	for (Boids::iterator it = m_boids.begin(), itEnd = m_boids.end(); it != itEnd; ++it)
	{
		CBoidObject* pBoid = (*it);
		if (pBoid && !pBoid->IsDead())
		{
			m_avgBoidPos += pBoid->GetPos();
			++n;
		}
	}
	if (n)
		m_avgBoidPos /= float(n);

}

//////////////////////////////////////////////////////////////////////////

void CFlock::UpdateBoidCollisions()
{
	if (!m_bc.avoidObstacles)
		return;
	const int numberOfChecks = 5;

	CTimeValue now = gEnv->pTimer->GetFrameStartTime();
	int checked = 0;

	// rough check
	if (m_BoidCollisionMap.size() != m_boids.size())
	{
		m_BoidCollisionMap.clear();
		m_BoidCollisionMap.reserve(m_boids.size());

		for (Boids::iterator it = m_boids.begin(), itEnd = m_boids.end(); it != itEnd; ++it)
		{
			m_BoidCollisionMap.push_back(SBoidCollisionTime(now, *it));
		}

		std::sort(m_BoidCollisionMap.begin(), m_BoidCollisionMap.end(), FSortBoidByTime());
	}

	for (TTimeBoidMap::iterator it = m_BoidCollisionMap.begin(), itEnd = m_BoidCollisionMap.end();
	     it != itEnd && checked < numberOfChecks; ++it)
	{
		CBoidObject* pBoid = it->m_pBoid;
		if (pBoid && pBoid->ShouldUpdateCollisionInfo(now))
		{
			pBoid->UpdateCollisionInfo();
			it->m_time = now;
			++checked;
		}
	}
	std::sort(m_BoidCollisionMap.begin(), m_BoidCollisionMap.end(), FSortBoidByTime());
}

//////////////////////////////////////////////////////////////////

/*
   //////////////////////////////////////////////////////////////////////////
   CFlockManager::CFlockManager( ISystem *system )
   {
   // Create one flock.
   m_system = system;
   m_lastFlockId = 1;
   //m_object = system->GetI3DEngine()->MakeCharacter( "Objects\\Other\\Seagull\\Seagull.cgf" );
   //m_object = system->GetI3DEngine()->LoadStatObj( "Objects\\Other\\Seagull\\Seagull.cgf" );
   REGISTER_CVAR2( "e_flocks",&m_e_flocks,1,VF_DUMPTODISK,"Enable Flocks (Birds/Fishes)" );
   REGISTER_CVAR2( "e_flocks_hunt",&m_e_flocks_hunt,0,0,"Birds will fall down..." );
   }
 */
