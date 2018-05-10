// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#ifndef WATER_PUDDLE_H
#define WATER_PUDDLE_H


#include <IGameObject.h>
#include "ActorManager.h"



class CWaterPuddle;



class CWaterPuddleManager
{
private:
	struct SWaterPuddle
	{
		CWaterPuddle* m_pPuddle;
		EntityId m_entityId;
	};

public:
	void AddWaterPuddle(CWaterPuddle* pPuddle);
	void RemoveWaterPuddle(CWaterPuddle* pPuddle);
	void Reset();

	CWaterPuddle* FindWaterPuddle(Vec3 point);

private:
	std::vector<SWaterPuddle> m_waterPuddles;
};



class CWaterPuddle : public CGameObjectExtensionHelper<CWaterPuddle, IGameObjectExtension>
{
public:
	CWaterPuddle();
	~CWaterPuddle();

	// IGameObjectExtension
	virtual bool Init(IGameObject * pGameObject);
	virtual void InitClient(int channelId);
	virtual void PostInit(IGameObject * pGameObject);
	virtual void PostInitClient(int channelId);
	virtual bool ReloadExtension(IGameObject * pGameObject, const SEntitySpawnParams &params);
	virtual void PostReloadExtension(IGameObject * pGameObject, const SEntitySpawnParams &params);
	virtual bool GetEntityPoolSignature(TSerialize signature);
	virtual void Release();
	virtual void FullSerialize(TSerialize ser);
	virtual bool NetSerialize(TSerialize ser, EEntityAspects aspect, uint8 profile, int flags);
	virtual void PostSerialize();
	virtual void SerializeSpawnInfo(TSerialize ser);
	virtual ISerializableInfoPtr GetSpawnInfo();
	virtual void Update(SEntityUpdateContext& ctx, int slot);
	virtual void HandleEvent(const SGameObjectEvent& gameObjectEvent);
	virtual void ProcessEvent(const SEntityEvent& entityEvent);
	virtual uint64 GetEventMask() const { return 0; }
	virtual void SetChannelId(uint16 id);
	virtual void PostUpdate(float frameTime);
	virtual void PostRemoteSpawn();
	virtual void GetMemoryUsage(ICrySizer *pSizer) const;
	// ~IGameObjectExtension

	void ZapEnemiesOnPuddle(int ownTeam, EntityId shooterId, EntityId weaponId, float damage, int hitTypeId, IParticleEffect* hitEffect);

private:
	void ApplyHit(const SActorData& actorData, EntityId shooterId, EntityId weaponId, float damage, int hitTypeId, float waterLevel, IParticleEffect* hitEffect);
};



#endif
