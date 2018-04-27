// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "WaterPuddle.h"
#include <CryGame/IGameVolumes.h>
#include "EntityUtility/EntityEffects.h"


void CWaterPuddle::InitClient(int channelId) {}
void CWaterPuddle::PostInitClient(int channelId) {}
void CWaterPuddle::PostReloadExtension(IGameObject * pGameObject, const SEntitySpawnParams &params) {}
bool CWaterPuddle::GetEntityPoolSignature(TSerialize signature) {return false;}
void CWaterPuddle::FullSerialize(TSerialize ser) {}
bool CWaterPuddle::NetSerialize(TSerialize ser, EEntityAspects aspect, uint8 profile, int flags) {return false;}
void CWaterPuddle::PostSerialize() {}
void CWaterPuddle::SerializeSpawnInfo(TSerialize ser) {}
ISerializableInfoPtr CWaterPuddle::GetSpawnInfo() {return 0;}
void CWaterPuddle::Update(SEntityUpdateContext& ctx, int slot) {}
void CWaterPuddle::HandleEvent(const SGameObjectEvent& gameObjectEvent) {}
void CWaterPuddle::ProcessEvent(const SEntityEvent& entityEvent) {}
void CWaterPuddle::SetChannelId(uint16 id) {}
void CWaterPuddle::PostUpdate(float frameTime) {}
void CWaterPuddle::PostRemoteSpawn() {}
void CWaterPuddle::GetMemoryUsage(ICrySizer *pSizer) const {}



namespace
{


	bool GetVolumeInfoForEntity(EntityId entityId, IGameVolumes::VolumeInfo* volumeInfo)
	{
		IGameVolumes* pGameVolumesMgr = gEnv->pGameFramework->GetIGameVolumesManager();
		if (pGameVolumesMgr != NULL)
		{
			return pGameVolumesMgr->GetVolumeInfoForEntity(entityId, volumeInfo);
		}

		return false;
	}



	bool IsPointInsideVolume(const Matrix34& worldTM, const IGameVolumes::VolumeInfo& volumeInfo, Vec3 testPoint, float pointRadius)
	{
		Matrix34 worldToLocalTM = worldTM.GetInverted();
		Vec3 point = worldToLocalTM.TransformPoint(testPoint);

		const float widthBorder = 0.15f;
		float volumeHeight = volumeInfo.volumeHeight;
		float minZ = min(-widthBorder, min(0.0f, volumeHeight));
		float maxZ = max(widthBorder, max(0.0f, volumeHeight));

		if (point.z <= minZ || point.z >= maxZ)
			return false;

		uint32 vertexCount = volumeInfo.verticesCount;
		size_t ii = vertexCount - 1;
		bool count = false;
		for (uint32 i = 0; i < vertexCount; ++i)
		{
			Vec3 v0 = volumeInfo.pVertices[ii];
			Vec3 v1 = volumeInfo.pVertices[i];
			const Vec3 edge = v1 - v0;
			const Vec3 normal = Vec3(edge.y, -edge.x, 0.0f).GetNormalized();
			v0 += normal * pointRadius;
			v1 += normal * pointRadius;
			ii = i;

			if ((((v1.y <= point.y) && (point.y < v0.y)) || ((v0.y <= point.y) && (point.y < v1.y))) &&
				(point.x < (v0.x - v1.x) * (point.y - v1.y) / (v0.y - v1.y) + v1.x))
			{
				count = !count;
			}
		}

		return count;
	}



	bool IsActorInsideVolume(const Matrix34& volumeWorldTM, const IGameVolumes::VolumeInfo& volumeInfo, EntityId actorId)
	{
		IEntity* pActorEntity = gEnv->pEntitySystem->GetEntity(actorId);
		if (!pActorEntity)
			return false;

		AABB actorBounds;
		pActorEntity->GetWorldBounds(actorBounds);
		Vec3 actorPivotPoint = pActorEntity->GetWorldPos();
		float approximateActorRadius = max((actorBounds.max.x - actorBounds.min.x), (actorBounds.max.y - actorBounds.min.y)) * 0.5f;

		bool isInsideVolume = IsPointInsideVolume(volumeWorldTM, volumeInfo, actorPivotPoint, approximateActorRadius);

		return isInsideVolume;
	}


}




void CWaterPuddleManager::AddWaterPuddle(CWaterPuddle* pPuddle)
{
	SWaterPuddle waterPuddle;
	waterPuddle.m_entityId = pPuddle->GetEntityId();
	waterPuddle.m_pPuddle = pPuddle;
	m_waterPuddles.push_back(waterPuddle);
}



void CWaterPuddleManager::RemoveWaterPuddle(CWaterPuddle* pPuddle)
{
	for (size_t i = 0; i < m_waterPuddles.size(); ++i)
	{
		if (m_waterPuddles[i].m_pPuddle == pPuddle)
		{
			m_waterPuddles.erase(m_waterPuddles.begin() + i);
			break;
		}
	}
}



void CWaterPuddleManager::Reset()
{
	m_waterPuddles.clear();
	stl::free_container(m_waterPuddles);
}



CWaterPuddle* CWaterPuddleManager::FindWaterPuddle(Vec3 point)
{
	size_t numPuddles = m_waterPuddles.size();
	for (size_t i = 0; i < numPuddles; ++i)
	{
		EntityId puddle = m_waterPuddles[i].m_entityId;

		IEntity* pEntity = gEnv->pEntitySystem->GetEntity(puddle);
		if (!pEntity)
			continue;

		IGameVolumes::VolumeInfo volumeInfo;
		if (!GetVolumeInfoForEntity(puddle, &volumeInfo))
			continue;

		if (IsPointInsideVolume(pEntity->GetWorldTM(), volumeInfo, point, 0.0f))
			return m_waterPuddles[i].m_pPuddle;
	}

	return 0;
}



CWaterPuddle::CWaterPuddle()
{
}



CWaterPuddle::~CWaterPuddle()
{
	if (g_pGame != NULL)
	{
		CWaterPuddleManager* pWaterPuddleManager = g_pGame->GetWaterPuddleManager();
		if (pWaterPuddleManager != NULL)
		{
			pWaterPuddleManager->RemoveWaterPuddle(this);
		}
	}
}



bool CWaterPuddle::Init(IGameObject * pGameObject)
{
	SetGameObject(pGameObject);
	return true;
}



void CWaterPuddle::PostInit(IGameObject * pGameObject)
{
	g_pGame->GetWaterPuddleManager()->AddWaterPuddle(this);
}



void CWaterPuddle::Release()
{
	delete this;
}



bool CWaterPuddle::ReloadExtension(IGameObject * pGameObject, const SEntitySpawnParams &params)
{
	ResetGameObject();
	return true;
}



void CWaterPuddle::ZapEnemiesOnPuddle(int ownTeam, EntityId shooterId, EntityId weaponId, float damage, int hitTypeId, IParticleEffect* hitEffect)
{
	IGameVolumes::VolumeInfo volumeInfo;
	if (!GetVolumeInfoForEntity(GetEntityId(), &volumeInfo))
		return;
	IEntity* pEntity = GetEntity();
	Matrix34 worldTM = pEntity->GetWorldTM();
	float waterLevel = worldTM.GetTranslation().z + volumeInfo.volumeHeight * 0.5f;

	CActorManager* pActorManager = CActorManager::GetActorManager();
	const int numberOfActors	= pActorManager->GetNumActors();

	for(int i = 0; i < numberOfActors; i++)
	{
		SActorData actorData;
		pActorManager->GetNthActorData(i, actorData);

		bool isActorAlive = (actorData.health > 0.0f);
		bool isActorEnemy = (actorData.teamId != ownTeam);
		bool isActorInsidevolume = IsActorInsideVolume(worldTM, volumeInfo, actorData.entityId);

		if (isActorAlive && isActorEnemy && isActorInsidevolume)
			ApplyHit(actorData, shooterId, weaponId, damage, hitTypeId, waterLevel, hitEffect);
	}
}



void CWaterPuddle::ApplyHit(const SActorData& actorData, EntityId shooterId, EntityId weaponId, float damage, int hitTypeId, float waterLevel, IParticleEffect* hitEffect)
{
	CGameRules* pGameRules = g_pGame->GetGameRules();

	const Vec3 hitPosition = actorData.position + Vec3Constants<float>::fVec3_OneZ;
	const Vec3 hitDirection = hitPosition.GetNormalized();
	HitInfo hitInfo(shooterId, actorData.entityId, weaponId,
		damage, 0.0f, -1, -1,
		hitTypeId, hitPosition, hitDirection,
		-hitDirection);

	hitInfo.projectileId = GetEntityId();
	hitInfo.aimed = false;

	pGameRules->ClientHit(hitInfo);

	const EntityEffects::SEffectSpawnParams effectSpawnParams(Vec3(actorData.position.x, actorData.position.y, waterLevel));
	EntityEffects::SpawnParticleFX(hitEffect, effectSpawnParams);
}
