// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

/*************************************************************************
-------------------------------------------------------------------------
$Id$
$DateTime$
Description: MikeBullet

-------------------------------------------------------------------------
History:
- 25:1:2010   : Created by Filipe Amim

*************************************************************************/
#include "StdAfx.h"
#include <CryRenderer/IRenderAuxGeom.h>
#include "Game.h"
#include "GameCVars.h"
#include "MikeBullet.h"
#include "ItemResourceCache.h"
#include "Player.h"
#include "GameRules.h"
#include <CryParticleSystem/ParticleParams.h>
#include "FireMode.h"
#include <CryAISystem/IFactionMap.h>

namespace
{
	Matrix33 OrthoNormalVector(const Vec3& vector, float rotation = 0.0f)
	{
		if (vector.x == 0 && vector.y == 0)
			return Matrix33(IDENTITY);
		Matrix33 matrixResult;
		Vec3 z = vector;
		Vec3 x = z.cross(Vec3(0, 0, 1));
		Vec3 y = z.cross(x);
		matrixResult.SetColumn(0, x.GetNormalized());
		matrixResult.SetColumn(1, y.GetNormalized());
		matrixResult.SetColumn(2, z);

		Matrix33 rotationMatrix = Matrix33::CreateRotationAA(rotation, vector);
		matrixResult = rotationMatrix * matrixResult;

		return matrixResult;
	}
}



CBurnEffectManager::CBurnEffectManager()
	:	m_damageTimeOut(0.0f)
{
}



CBurnEffectManager::~CBurnEffectManager()
{
}



void CBurnEffectManager::AddBurnPoint(
	const EventPhysCollision& pCollision, SMikeBulletParams* pBurnBulletParams, int hitType, bool shooterIsPlayer,
	const uint8 shooterFactionID)
{
	int surfaceType = pCollision.idmat[1];

	CGameRules* pGameRules = g_pGame->GetGameRules();
	IEntity* pEntity = pCollision.iForeignData[1] == PHYS_FOREIGN_ID_ENTITY ? (IEntity*)pCollision.pForeignData[1] : NULL;
	
	const SMikeBulletParams::SBurnParams* pBurnParams = NULL;
	EntityId entityId = 0;
	
	if(pEntity)
	{
		pBurnParams = pBurnBulletParams->GetBurnClassParams(pEntity->GetClass());
		entityId = pEntity->GetId();
	}

	if(!pBurnParams)
	{
		entityId = 0;
		pBurnParams = pBurnBulletParams->GetBurnSurfaceParams(surfaceType);
	}
	
	// Burn-points are 'recycled/shared' between entities, so we can only differentiate
	// between factions. This limits the 'hit detection' to a per factions basis also.
	TBurnPoints::iterator best = entityId ? FindBurnPointOnEntity(entityId, shooterFactionID) : FindClosestBurnPoint(pCollision.pt, surfaceType, shooterFactionID);

	if (best != m_burnPoints.end())
	{
		best->m_accumulation += best->m_pBurnParams->m_buildUp;
		best->m_accumulationDelay = best->m_pBurnParams->m_dissipationDelay;
	}
	else
	{
		SBurnPoint burnPoint;
		burnPoint.m_pBurnParams = pBurnParams;
		burnPoint.m_effect = 0;
		burnPoint.m_accumulation = burnPoint.m_pBurnParams->m_buildUp;
		burnPoint.m_accumulationDelay = burnPoint.m_pBurnParams->m_dissipationDelay;
		burnPoint.m_radius = pBurnParams->m_radius;
		burnPoint.m_position = pCollision.pt;
		burnPoint.m_surfaceType = surfaceType;
		burnPoint.m_attachedEntityId = entityId;
		burnPoint.m_hitType = hitType;
		burnPoint.m_shootByPlayer = shooterIsPlayer;
		burnPoint.m_shooterFactionID = shooterFactionID;
		PushNewBurnPoint(pCollision, &burnPoint);
	}

	SpawnImpactEffect(pCollision, pBurnParams->m_impactEffectName.c_str());
}



void CBurnEffectManager::Update(float deltaTime)
{
	TBurnPoints::iterator it = m_burnPoints.begin();
	for (; it != m_burnPoints.end();)
	{
		SBurnPoint& burnPoint = *it;
		if (burnPoint.m_accumulationDelay > 0.0f)
			burnPoint.m_accumulationDelay -= deltaTime;
		else
			burnPoint.m_accumulation -= deltaTime * burnPoint.m_pBurnParams->m_dissipation;
		burnPoint.m_accumulation = SATURATE(burnPoint.m_accumulation);

		if (burnPoint.m_accumulation <= 0)
		{
			DestroyBurnEffect(&burnPoint);
			it = m_burnPoints.erase(it);
		}
		else
		{
			UpdateBurnEffect(&burnPoint);
			++it;
		}
	}

	ApplySurroundingDamage(deltaTime);

	if (g_pGameCVars->pl_mike_debug)
		DebugDraw();
}



void CBurnEffectManager::Reset()
{
	while (!m_burnPoints.empty())
	{
		DestroyBurnEffect(&*m_burnPoints.begin());
		m_burnPoints.erase(m_burnPoints.begin());
	}

	stl::free_container(m_burnPoints);
}



void CBurnEffectManager::PushNewBurnPoint(const EventPhysCollision& collision, CBurnEffectManager::SBurnPoint* burnPoint)
{
	const int maxNumBurnPoints = g_pGameCVars->pl_mike_maxBurnPoints;

	int index = 0;

	if (int(m_burnPoints.size()) < maxNumBurnPoints)
	{
		m_burnPoints.push_back(*burnPoint);
		index = m_burnPoints.size() - 1;
	}
	else
	{
		float minAccum = std::numeric_limits<float>::max();
		for (int i = 0; i < int(m_burnPoints.size()); ++i)
		{
			float accum = m_burnPoints[i].m_accumulation + m_burnPoints[i].m_accumulationDelay;
			if (accum <= minAccum)
			{
				index = i;
				minAccum = accum;
			}
		}
		DestroyBurnEffect(&m_burnPoints[index]);
		m_burnPoints[index] = *burnPoint;
	}

	CreateBurnEffect(collision, &m_burnPoints[index]);
}



CBurnEffectManager::TBurnPoints::iterator CBurnEffectManager::FindBurnPointOnEntity(EntityId entityId, const uint8 shooterFactionID)
{
	if (m_burnPoints.empty())
		return m_burnPoints.end();
	
	TBurnPoints::iterator point = m_burnPoints.end();
	
	for (TBurnPoints::iterator it = m_burnPoints.begin(); it != m_burnPoints.end(); ++it)
	{
		if ((it->m_attachedEntityId == entityId) && (it->m_shooterFactionID == shooterFactionID) )
		{
			point = it;
		}
	}

	return point;
}


CBurnEffectManager::TBurnPoints::iterator CBurnEffectManager::FindClosestBurnPoint(const Vec3& point, int surfaceType, const uint8 shooterFactionID)
{
	if (m_burnPoints.empty())
		return m_burnPoints.end();
	float minDist = m_burnPoints[0].m_position.GetSquaredDistance(point);
	TBurnPoints::iterator best = m_burnPoints.end();
	if (minDist <= sqr(m_burnPoints[0].m_radius))
		best = m_burnPoints.begin();
	for (TBurnPoints::iterator it = m_burnPoints.begin()+1; it != m_burnPoints.end(); ++it)
	{
		float dist = it->m_position.GetSquaredDistance(point);
		if (dist < minDist && dist <= sqr(it->m_radius) && (it->m_surfaceType == surfaceType) && (it->m_shooterFactionID == shooterFactionID))
		{
			minDist = dist;
			best = it;
		}
	}
	return best;
}



void CBurnEffectManager::SpawnImpactEffect(const EventPhysCollision& pCollision, const char* effectName)
{
	Vec3 surfaceNormal = pCollision.n;
	Vec3 surfacePosition = pCollision.pt;

	CItemParticleEffectCache& particleCache = g_pGame->GetGameSharedParametersStorage()->GetItemResourceCache().GetParticleEffectCache();
	IParticleEffect* pParticleEffect = particleCache.GetCachedParticle(effectName);

	if (pParticleEffect)
	{
		Matrix34 loc;
		loc.SetIdentity();
		loc.SetTranslation(surfacePosition);
		loc.SetRotation33(OrthoNormalVector(surfaceNormal));
		pParticleEffect->Spawn(false, loc);
	}
}



void CBurnEffectManager::CreateBurnEffect(const EventPhysCollision& pCollision, CBurnEffectManager::SBurnPoint* pBurnPoint)
{
	Vec3 surfaceNormal = pCollision.n;
	Vec3 hitDir(ZERO);
	if (pCollision.vloc[0].GetLengthSquared() > 1e-6f)
	{
		hitDir = pCollision.vloc[0].GetNormalized();
	}
	Vec3 surfacePosition = pCollision.pt;
	Vec3 halfVector = (surfaceNormal + (-hitDir)).GetNormalized();

	CItemParticleEffectCache& particleCache = g_pGame->GetGameSharedParametersStorage()->GetItemResourceCache().GetParticleEffectCache();
	IParticleEffect* pParticleEffect = particleCache.GetCachedParticle(pBurnPoint->m_pBurnParams->m_effectName);

	if (pParticleEffect)
	{
		Matrix34 loc;
		loc.SetIdentity();
		loc.SetTranslation(surfacePosition);
		loc.SetRotation33(OrthoNormalVector(surfaceNormal));
		IParticleEmitter* pEffect = pParticleEffect->Spawn(false, loc);
		if(pEffect)
		{
			pEffect->AddRef();
			pBurnPoint->m_effect = pEffect;

			const ParticleParams& particleParams = pParticleEffect->GetParticleParams();

			pBurnPoint->m_attachType = particleParams.eAttachType;
			pBurnPoint->m_attachForm = particleParams.eAttachForm;
		}
		UpdateBurnEffect(pBurnPoint);
	}
	
	UpdateBurnEffect(pBurnPoint);
}



void CBurnEffectManager::UpdateBurnEffect(CBurnEffectManager::SBurnPoint* pBurnPoint)
{
	if (pBurnPoint->m_effect)
	{
		float effectIntensity = pBurnPoint->m_accumulation;
		SpawnParams params;
		GeomRef geomRef;

		if(pBurnPoint->m_attachedEntityId)
		{
			params.eAttachType = pBurnPoint->m_attachType;
			params.eAttachForm = pBurnPoint->m_attachForm;

			IEntity* pEntity = gEnv->pEntitySystem->GetEntity(pBurnPoint->m_attachedEntityId);
			geomRef.Set(pEntity, 0);
			if (!geomRef)
				pBurnPoint->m_accumulation = 0.f;
		}

		params.fStrength = effectIntensity;
		pBurnPoint->m_effect->SetSpawnParams(params, geomRef);
	}
}



void CBurnEffectManager::DestroyBurnEffect(CBurnEffectManager::SBurnPoint* pBurnPoint)
{
	if (pBurnPoint->m_effect)
	{
		pBurnPoint->m_effect->Activate(false);
		pBurnPoint->m_effect->Release();
		pBurnPoint->m_effect = 0;
	}
}



void CBurnEffectManager::ApplySurroundingDamage(float deltaTime)
{
	const float damageTime = 0.5f;

	m_damageTimeOut -= deltaTime;
	if (m_damageTimeOut > 0.0f)
		return;
	m_damageTimeOut += damageTime;

	CGameRules* pGameRules = g_pGame->GetGameRules();
	if (pGameRules)
	{
		for (TBurnPoints::iterator it = m_burnPoints.begin(); it != m_burnPoints.end(); ++it)
		{
			const int hitType = it->m_hitType;
			const float damagePerSecond =
				it->m_shootByPlayer ?
					it->m_pBurnParams->m_damagePerSecond :
					it->m_pBurnParams->m_damagePerSecond_AI;
			const float damage = damagePerSecond * damageTime * it->m_accumulation;

			if (damage == 0.0f)
				continue;

			const Vec3 radius = Vec3(it->m_radius, it->m_radius, it->m_radius);
			SEntityProximityQuery query;
			query.box.min = it->m_position - radius;
			query.box.max = it->m_position + radius;
			gEnv->pEntitySystem->QueryProximity(query);

			for (int i = 0; i < query.nCount; ++i)
			{
				if (IEntity* pEntity = query.pEntities[i])
				{
					if (ShouldBurnPointInflictDamageOntoEntity(*it, pEntity))
					{
						HitInfo hitInfo(0, pEntity->GetId(), 0,
							damagePerSecond, 0.0f, 0, 0,
							hitType, it->m_position, Vec3(ZERO), Vec3(ZERO));

						pGameRules->ClientHit(hitInfo);
					}
				}
			}
		}
	}
}


bool CBurnEffectManager::ShouldBurnPointInflictDamageOntoEntity(const SBurnPoint& burnPoint, IEntity* targetEntity) const
{
	IF_UNLIKELY (targetEntity == NULL)
	{
		return false;
	}

	bool targetIsPlayerFlag = false;
	CActor* clientActor = static_cast<CActor*>(g_pGame->GetIGameFramework()->GetClientActor());
	IF_LIKELY (clientActor != NULL)
	{
		if (targetEntity == clientActor->GetEntity())
		{
			targetIsPlayerFlag = true;
		}
	}

	const uint8 shooterFactionID = burnPoint.m_shooterFactionID;
	IF_LIKELY (shooterFactionID != IFactionMap::InvalidFactionID)
	{
		const IAIObject* targetAIObject = targetEntity->GetAI();
		if (targetAIObject != NULL)
		{
			const IFactionMap::ReactionType reactionType = gEnv->pAISystem->GetFactionMap().GetReaction(shooterFactionID, targetAIObject->GetFactionID());
			switch (reactionType)
			{
			case IFactionMap::Hostile:
			case IFactionMap::Neutral:
				break;

			case IFactionMap::Friendly:
				{
					// In single player, the player may also get hit by his own flame-thrower if he is not careful enough.
					if (gEnv->bMultiplayer)
					{
						return false;
					}
					if (!targetIsPlayerFlag)
					{
						return false;
					}
				}
				break;

			default:
				// We should never get here!
				assert(false);
				break;
			}
		}
	}

	return true;
}


void CBurnEffectManager::DebugDraw()
{
#	ifndef _RELEASE
	SAuxGeomRenderFlags oldRenderFlags = gEnv->pRenderer->GetIRenderAuxGeom()->GetRenderFlags();
	SAuxGeomRenderFlags renderFlags = oldRenderFlags;
	renderFlags.SetAlphaBlendMode(e_AlphaAdditive);
	renderFlags.SetDepthWriteFlag(e_DepthWriteOff);
	gEnv->pRenderer->GetIRenderAuxGeom()->SetRenderFlags(renderFlags);

	for (size_t i = 0; i < m_burnPoints.size(); ++i)
	{
		const SBurnPoint& burnPoint = m_burnPoints[i];
		ColorB color;
		color.r = (unsigned char)(SATURATE(burnPoint.m_accumulation * 2.0f) * 255.0f);
		color.g = (unsigned char)(SATURATE(burnPoint.m_accumulation * 1.0f) * 255.0f);
		color.b = (unsigned char)(SATURATE(burnPoint.m_accumulation * 0.5f) * 255.0f);
		gEnv->pRenderer->GetIRenderAuxGeom()->DrawSphere(
			burnPoint.m_position, burnPoint.m_radius, color);
	}

	gEnv->pRenderer->GetIRenderAuxGeom()->SetRenderFlags(oldRenderFlags);
#	endif
}



CMikeBullet::CMikeBullet() :
	m_ownerFactionID(IFactionMap::InvalidFactionID)
{
}


void CMikeBullet::SetParams(const SProjectileDesc& projectileDesc)
{
	BaseClass::SetParams(projectileDesc);

	// Fetching the faction ID of the owner immediately just to be sure that we know
	// this information, even if the owner is deleted at some point.
	m_ownerFactionID = IFactionMap::InvalidFactionID;
	IEntity* ownerEntity = gEnv->pEntitySystem->GetEntity(projectileDesc.ownerId);
	IF_LIKELY (ownerEntity != NULL)
	{
		IAIObject* ownerAIObject = ownerEntity->GetAI();
		IF_LIKELY (ownerAIObject != NULL)
		{
			m_ownerFactionID = ownerAIObject->GetFactionID();
		}
	}
}


void CMikeBullet::HandleEvent(const SGameObjectEvent& event)
{
	CRY_PROFILE_FUNCTION(PROFILE_GAME);
	CBullet::HandleEvent(event);

	if (!m_pAmmoParams->pMikeBulletParams)
		return;

	if (event.event == eGFE_OnCollision && !gEnv->bMultiplayer) //No flames in MP
	{
		const bool playerIsShooter = CheckAnyProjectileFlags(ePFlag_ownerIsPlayer);
		const EventPhysCollision& collision = *reinterpret_cast<EventPhysCollision*>(event.ptr);
		g_pGame->GetBurnEffectManager()->AddBurnPoint(collision, m_pAmmoParams->pMikeBulletParams, m_hitTypeId, playerIsShooter, m_ownerFactionID);
	}
}
