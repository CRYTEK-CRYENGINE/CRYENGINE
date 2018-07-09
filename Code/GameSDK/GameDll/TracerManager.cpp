// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

/*************************************************************************
-------------------------------------------------------------------------
$Id$
$DateTime$

-------------------------------------------------------------------------
History:
- 17:1:2006   11:18 : Created by Márcio Martins

*************************************************************************/
#include "StdAfx.h"
#include "TracerManager.h"
#include "Game.h"
#include "GameCVars.h"
#include "Actor.h"
#include <Cry3DEngine/I3DEngine.h>
#include "RecordingSystem.h"
#include "WeaponSystem.h"
#include "Projectile.h"


#define TRACER_GEOM_SLOT  0
#define TRACER_FX_SLOT    1
//------------------------------------------------------------------------
CTracer::CTracer(const Vec3 &pos)
: m_pos(0,0,0),
	m_dest(0,0,0),
	m_startingPos(pos),
	m_age(0.0f),
	m_lifeTime(1.5f),
	m_entityId(0),
	m_geometrySlot(0),
	m_startFadeOutTime(0.0f),
	m_scale(1.0f),
	m_geometryOpacity(0.99f),
	m_slideFrac(0.f),
	m_tracerFlags(0)
{
	CreateEntity();
}

//------------------------------------------------------------------------
CTracer::CTracer()
	: m_speed(0.0f)
	, m_pos(ZERO)
	, m_dest(ZERO)
	, m_startingPos(ZERO)
	, m_age(0.0f)
	, m_lifeTime(1.5f)
	, m_fadeOutTime(0.0f)
	, m_startFadeOutTime(0.0f)
	, m_scale(1.0f)
	, m_geometryOpacity(0.99f)
	, m_slideFrac(0.f)
	, m_entityId(0)
	, m_boundToBulletId(0)
	, m_tracerFlags(0)
	, m_effectSlot(0)
	, m_geometrySlot(0)
{
}

//------------------------------------------------------------------------
CTracer::~CTracer()
{
	//DeleteEntity();
}

//------------------------------------------------------------------------
void CTracer::Reset(const Vec3 &pos, const Vec3 &dest)
{
	m_pos.zero();
	m_dest.zero();
	m_startingPos=pos;
	m_age=0.0f;
	m_lifeTime=1.5f;
	m_tracerFlags &= ~kTracerFlag_scaleToDistance;
	m_startFadeOutTime = 0.0f;
	m_slideFrac = 0.f;

	if (IEntity *pEntity=gEnv->pEntitySystem->GetEntity(m_entityId))
	{
		pEntity->FreeSlot(TRACER_GEOM_SLOT);
		pEntity->FreeSlot(TRACER_FX_SLOT);

		Vec3 dir = dest - pos;
		dir.Normalize();

		Matrix34 tm;
		
		tm.SetIdentity();

		if(!dir.IsZero())
		{
			tm = Matrix33::CreateRotationVDir(dir);
		}
		
		tm.AddTranslation(pos);
		pEntity->SetWorldTM(tm);
	}
}

//------------------------------------------------------------------------
void CTracer::CreateEntity()
{
	if (!m_entityId)
	{
		SEntitySpawnParams spawnParams;
		spawnParams.pClass = gEnv->pEntitySystem->GetClassRegistry()->GetDefaultClass();
		spawnParams.sName = "_tracer";
		spawnParams.nFlags = ENTITY_FLAG_NO_PROXIMITY | ENTITY_FLAG_CLIENT_ONLY | ENTITY_FLAG_NO_SAVE;

		if (IEntity *pEntity=gEnv->pEntitySystem->SpawnEntity(spawnParams))
			m_entityId=pEntity->GetId();
	}
}

void CTracer::DeleteEntity()
{
	if (m_entityId)
	{
		gEnv->pEntitySystem->RemoveEntity(m_entityId);
	}

	m_entityId=0;
}

//------------------------------------------------------------------------
void CTracer::GetMemoryStatistics(ICrySizer * s) const
{
	s->Add(*this);
}

//------------------------------------------------------------------------
void CTracer::SetGeometry(const char *name, float scale)
{
	if (IEntity *pEntity=gEnv->pEntitySystem->GetEntity(m_entityId))
	{
		m_geometrySlot =pEntity->LoadGeometry(TRACER_GEOM_SLOT, name);

		if (scale!=1.0f)
		{
			Matrix34 tm=Matrix34::CreateIdentity();
			tm.ScaleColumn(Vec3(scale, scale, scale));
			pEntity->SetSlotLocalTM(m_geometrySlot, tm);
		}

		// Set opacity
		pEntity->SetOpacity(m_geometryOpacity);
	}
}

//------------------------------------------------------------------------
void CTracer::SetEffect(const char *name, float scale)
{
	IParticleEffect *pEffect = gEnv->pParticleManager->FindEffect(name);
	if (!pEffect)
		return;

	if (IEntity *pEntity=gEnv->pEntitySystem->GetEntity(m_entityId))
	{
		m_effectSlot = pEntity->LoadParticleEmitter(TRACER_FX_SLOT, pEffect);
		if (scale!=1.0f)
		{
			Matrix34 tm=Matrix34::CreateIdentity();
			tm.ScaleColumn(Vec3(scale, scale, scale));
			pEntity->SetSlotLocalTM(m_effectSlot, tm);
		}
	}
}

//------------------------------------------------------------------------
bool CTracer::Update(float frameTime, const Vec3 &camera, const float fovScale)
{
	frameTime							= (float)__fsel(-m_age, 0.002f, frameTime);
	const float tracerAge	= (float)__fsel(-m_age, 0.002f, m_age+frameTime);

	m_age = tracerAge;

	if (tracerAge >= m_lifeTime)
		return false;

	Vec3 currentLimitDestination;
	if (gEnv->bMultiplayer)
	{
		if(m_tracerFlags & kTracerFlag_updateDestFromBullet)
		{
			IEntity* pBulletEntity = gEnv->pEntitySystem->GetEntity(m_boundToBulletId);
			if(pBulletEntity)
			{
				m_dest = pBulletEntity->GetPos();
			}
		}
		currentLimitDestination = m_dest;
	}
	else
	{
		IEntity* pBulletEntity = gEnv->pEntitySystem->GetEntity(m_boundToBulletId);
		currentLimitDestination = pBulletEntity ? pBulletEntity->GetPos() : m_dest;
	}

	const Vec3 maxTravelledDistance = m_dest - m_startingPos;
	const float maxTravelledDistanceSqr = maxTravelledDistance.len2();
	
	float dist = sqrt_tpl(maxTravelledDistanceSqr);
	if (dist <= 0.001f)
		return false;
		
	const Vec3 dir = maxTravelledDistance * (float)__fres(dist);
	Vec3 pos = m_pos;
	float lengthScale = 1.f;
	Vec3 newPos = m_pos;
	
	if (!(m_tracerFlags & kTracerFlag_dontTranslate))
  {
    const float sqrRadius = GetGameConstCVar(g_tracers_slowDownAtCameraDistance) * GetGameConstCVar(g_tracers_slowDownAtCameraDistance);
		const float cameraDistance = (m_pos-camera).len2();
    const float speed = m_speed * (float)__fsel(sqrRadius - cameraDistance, 0.35f + (cameraDistance/(sqrRadius*2)), 1.0f); //Slow down tracer when near the player
    newPos += dir * min(speed*frameTime, dist);
	  pos = newPos;

		if(m_slideFrac > 0.f)
		{
			pos += cry_random(-0.5f, 1.5f) * m_slideFrac * speed * frameTime * dir;
		}
  }

	// Now update visuals...
	if (IEntity *pEntity = gEnv->pEntitySystem->GetEntity(m_entityId))
	{
		AABB tracerBbox;
		pEntity->GetWorldBounds(tracerBbox);

		float tracerHalfLength = !tracerBbox.IsEmpty() ? tracerBbox.GetRadius() : 0.0f;
		const Vec3 frontOfTracerPos = pos + (dir * tracerHalfLength);

		if((frontOfTracerPos-m_startingPos).len2() > maxTravelledDistanceSqr)
		{
			return false;
		}
	
		if (!(m_tracerFlags & kTracerFlag_dontTranslate) && tracerHalfLength > 0.f)
		{
			//Ensure that never goes in front of the bullet
			const Vec3 dirFromFrontOfTracerToDestination = currentLimitDestination - frontOfTracerPos;
			if (dir.dot(dirFromFrontOfTracerToDestination) < 0)
			{
				pos += dirFromFrontOfTracerToDestination;
			}

			// ... and check if back of tracer is behind starting point, so adjust length.
			const Vec3 backOfTracerPos = pos - (dir * tracerHalfLength);
			const Vec3 dirFromBackOfTracerToStart = m_startingPos - backOfTracerPos;

			if (dir.dot(dirFromBackOfTracerToStart) > 0)
			{
				if(dir.dot((m_startingPos - pos)) > 0)
				{
					pos = m_startingPos + (dir * cry_random(0.0f, 1.0f) * tracerHalfLength);
				}

				lengthScale = ((pos - m_startingPos).GetLength() / tracerHalfLength);
			}
		}
		
		m_pos = newPos;
		
		Matrix34 tm(Matrix33::CreateRotationVDir(dir));
		tm.AddTranslation(pos);

		pEntity->SetWorldTM(tm);

		//Do not scale effects
		if((m_tracerFlags & kTracerFlag_useGeometry))
		{
			float finalFovScale = fovScale;
			if((m_tracerFlags & kTracerFlag_scaleToDistance) != 0)
			{
				lengthScale = dist * 0.5f;
			}
			else
			{
				const float cameraDistanceSqr = (m_pos-camera).len2();
				const float minScale = GetGameConstCVar(g_tracers_minScale);
				const float maxScale = GetGameConstCVar(g_tracers_maxScale);
				const float minDistanceRange = GetGameConstCVar(g_tracers_minScaleAtDistance) * GetGameConstCVar(g_tracers_minScaleAtDistance);
				const float maxDistanceRange = max(GetGameConstCVar(g_tracers_maxScaleAtDistance) * GetGameConstCVar(g_tracers_maxScaleAtDistance), minDistanceRange + 1.0f);
				const float currentRefDistance = clamp_tpl(cameraDistanceSqr, minDistanceRange, maxDistanceRange);

				const float distanceToCameraFactor = ((currentRefDistance - minDistanceRange) / (maxDistanceRange - minDistanceRange));
				const float distanceToCameraScale = LERP(minScale, maxScale, distanceToCameraFactor);
				
				lengthScale = m_scale * distanceToCameraScale;
				finalFovScale *= distanceToCameraScale;
			}
			
			const float widthScale = fovScale;

			tm.SetIdentity();
			tm.SetScale(Vec3(m_scale * finalFovScale, lengthScale, m_scale * finalFovScale));
			pEntity->SetSlotLocalTM(m_geometrySlot,tm);
		}
	}

	return true;
}

//------------------------------------------------------------------------
void CTracer::SetLifeTime(float lifeTime)
{
	m_lifeTime = lifeTime;
}

//////////////////////////////////////////////////////////////////////////

//------------------------------------------------------------------------
CTracerManager::CTracerManager()
: m_numActiveTracers(0)
{
}

//------------------------------------------------------------------------
CTracerManager::~CTracerManager()
{
}

//------------------------------------------------------------------------
int CTracerManager::EmitTracer(const STracerParams &params, const EntityId bulletId)
{
	if(!gEnv->IsClient())
		return -1;

	CRecordingSystem* pRecordingSystem = g_pGame->GetRecordingSystem();
	if (pRecordingSystem)
	{
		if (!pRecordingSystem->OnEmitTracer(params))
		{
			return -1;
		}
	}

	int idx = 0;

	if(m_numActiveTracers < kMaxNumTracers)
	{
		//Add to end of array
		idx = m_numActiveTracers;
		m_tracerPool[idx].CreateEntity();
		m_tracerPool[idx].Reset(params.position, params.destination);
		m_numActiveTracers++;
	}
	else
	{
		//Find the oldest existing tracer and remove
		assert(!"Too many tracers in existence! Removing oldest");

		float oldest = 0.0f;

		for(int i = 0; i < kMaxNumTracers; i++)
		{
			CryPrefetch(&m_tracerPool[i+4]);
			if(m_tracerPool[i].m_age > oldest)
			{
				oldest = m_tracerPool[i].m_age;
				idx = i;
			}
		}

		m_tracerPool[idx].Reset(params.position, params.destination);
	}

	CTracer& tracer = m_tracerPool[idx];

	tracer.m_boundToBulletId = bulletId;
	tracer.m_tracerFlags = kTracerFlag_active;
	tracer.m_effectSlot = -1;
	tracer.m_geometryOpacity = params.geometryOpacity;
	if (params.geometry && params.geometry[0])
	{
		tracer.SetGeometry(params.geometry, params.scale);
		tracer.m_tracerFlags |= kTracerFlag_useGeometry;
	}

	tracer.SetLifeTime(params.lifetime);

	tracer.m_speed = params.speed;
	tracer.m_pos = params.position;
	tracer.m_startingPos = params.position;
	tracer.m_dest = params.destination;
	tracer.m_fadeOutTime = params.delayBeforeDestroy;
	
	if(params.scaleToDistance)
	{
		tracer.m_tracerFlags |= kTracerFlag_scaleToDistance;
	}

  if(params.dontTranslate)
  {
    tracer.m_tracerFlags |= kTracerFlag_dontTranslate;
  }

	if(params.updateDestPosFromBullet)
	{
		tracer.m_tracerFlags |= kTracerFlag_updateDestFromBullet;
	}
	
	tracer.m_startFadeOutTime = params.startFadeOutTime;
	tracer.m_scale = params.scale;
	tracer.m_slideFrac = params.slideFraction;

	if (IEntity *pEntity=gEnv->pEntitySystem->GetEntity(tracer.m_entityId))
	{
		pEntity->Hide(0);
	}

	// set effect after unhiding the entity, or else the particle emitter is created hidden
	if (params.effect && params.effect[0])
		tracer.SetEffect(params.effect, params.scale);

	return idx;
}

//------------------------------------------------------------------------
void CTracerManager::OnBoundProjectileDestroyed( const int tracerIdx, const EntityId projectileId, const Vec3& newEndTracerPosition )
{
	bool validTracerIdx = (tracerIdx >= 0) && (tracerIdx < kMaxNumTracers);

	if (validTracerIdx)
	{
		CTracer& tracer = m_tracerPool[tracerIdx];

		bool tracerMatchesProjectile = (tracer.m_tracerFlags & kTracerFlag_active) && (tracer.m_boundToBulletId == projectileId);
		if (tracerMatchesProjectile)
		{
			tracer.m_dest = newEndTracerPosition;
			tracer.m_boundToBulletId = 0; 
		}
	}
}

//------------------------------------------------------------------------
void CTracerManager::Update(float frameTime)
{
	CryPrefetch(m_tracerPool);

	const CCamera& viewCamera = gEnv->pSystem->GetViewCamera();
	const Vec3 cameraPosition = viewCamera.GetPosition();
	const float viewCameraFovScale = viewCamera.GetFov() / DEG2RAD(g_pGame->GetFOV());

	const int kNumActiveTracers = m_numActiveTracers;

	for(int i = 0; i < kNumActiveTracers;)
	{
		int nextTracerIndex = i + 1;
		CTracer& tracer = m_tracerPool[i];
		CryPrefetch(&m_tracerPool[nextTracerIndex]);
		
		//Update
		const bool stillMoving = tracer.Update(frameTime, cameraPosition, viewCameraFovScale);

		if(stillMoving || (tracer.m_fadeOutTime > 0.f))
		{
			tracer.m_tracerFlags |= kTracerFlag_active;
			
			if (!stillMoving)
			{
				tracer.m_fadeOutTime -= frameTime;

				if (tracer.m_effectSlot > -1)
				{
					if (IEntity *pEntity = gEnv->pEntitySystem->GetEntity(tracer.m_entityId))
					{
						IParticleEmitter * emitter = pEntity->GetParticleEmitter(tracer.m_effectSlot);
						if (emitter)
						{
							emitter->Activate(false);
						}

						if (tracer.m_tracerFlags & kTracerFlag_useGeometry)
						{
							pEntity->SetSlotFlags(tracer.m_geometrySlot, pEntity->GetSlotFlags(tracer.m_geometrySlot) &~ (ENTITY_SLOT_RENDER|ENTITY_SLOT_RENDER_NEAREST));
						}

						tracer.m_effectSlot = -1;
					}
				}

				if(tracer.m_tracerFlags & kTracerFlag_useGeometry)
				{
					// Fade out geometry
					if((tracer.m_fadeOutTime < tracer.m_startFadeOutTime) && (tracer.m_startFadeOutTime > 0.0f))
					{
						if (IEntity *pEntity = gEnv->pEntitySystem->GetEntity(tracer.m_entityId))
						{
							pEntity->SetOpacity(max((tracer.m_fadeOutTime / tracer.m_startFadeOutTime) * tracer.m_geometryOpacity, 0.0f));
						}
					}
				}
			}
		}
		else
		{
			tracer.m_tracerFlags &= ~kTracerFlag_active;
			tracer.m_boundToBulletId = 0;
		}

		i = nextTracerIndex;
	}

	//This is where we clear out the inactive tracers, so the counter isn't const
	int numActiveTracers = kNumActiveTracers;
	
	CWeaponSystem* pWeaponSystem = g_pGame->GetWeaponSystem();

	for(int i = kNumActiveTracers - 1; i >= 0;)
	{
		int nextIndex = i - 1;
		CTracer& tracer = m_tracerPool[i];
		CryPrefetch(&m_tracerPool[nextIndex]);
		
		if(!(tracer.m_tracerFlags & kTracerFlag_active))
		{
			if (IEntity *pEntity=gEnv->pEntitySystem->GetEntity(tracer.m_entityId))
			{
				pEntity->Hide(true);
				pEntity->FreeSlot(TRACER_GEOM_SLOT);
				pEntity->FreeSlot(TRACER_FX_SLOT); 
			}

			//Switch the inactive tracer so it's at the end of the array;
			const int lastTracer = numActiveTracers - 1;
			static CTracer temp;
			temp = tracer;			
			numActiveTracers = lastTracer;
			tracer = m_tracerPool[lastTracer];
			m_tracerPool[lastTracer] = temp;

			//Re-bind index to the corresponding bullet
			if (tracer.m_boundToBulletId)
			{
				CProjectile* pProjectile = pWeaponSystem->GetProjectile(tracer.m_boundToBulletId);
				if (pProjectile && pProjectile->IsAlive())
				{
					if(lastTracer == pProjectile->GetTracerIdx())
					{
						pProjectile->BindToTracer(i);
					}
					else if(lastTracer == pProjectile->GetThreatTrailTracerIdx())
					{
						pProjectile->BindToThreatTrailTracer(i);
					}
				}
			}

		}

		i = nextIndex;
	}

	m_numActiveTracers = numActiveTracers;
}

//------------------------------------------------------------------------
void CTracerManager::Reset()
{
	for(int i = 0; i < kMaxNumTracers; i++)
	{
		m_tracerPool[i].DeleteEntity();
	}

	m_numActiveTracers = 0;	
}

//------------------------------------------------------------------------
void CTracerManager::ClearCurrentActiveTracers()
{
	const int kNumActiveTracers = m_numActiveTracers;
	for(int i = 0; i < kNumActiveTracers; i++)
	{
		CTracer& tracer = m_tracerPool[i];
		CryPrefetch(&m_tracerPool[i+4]);

		tracer.m_tracerFlags &= ~kTracerFlag_active;

		if (IEntity *pEntity=gEnv->pEntitySystem->GetEntity(tracer.m_entityId))
		{
			pEntity->Hide(true);
			pEntity->FreeSlot(TRACER_GEOM_SLOT);
			pEntity->FreeSlot(TRACER_FX_SLOT); 
		}
	}

	m_numActiveTracers = 0;
}

void CTracerManager::GetMemoryStatistics(ICrySizer * s)
{
	SIZER_SUBCOMPONENT_NAME(s, "TracerManager");
	//s->Add(*this);
	//s->AddObject(m_tracerPool);

	for (int16 i=0; i<m_numActiveTracers; i++)
		m_tracerPool[i].GetMemoryStatistics(s);
}


