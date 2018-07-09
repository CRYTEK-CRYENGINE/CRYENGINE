// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#ifndef _LIGHTNING_ARC_H_
#define _LIGHTNING_ARC_H_

#pragma once


#include "Effects/GameEffects/LightningGameEffect.h"



class CLightningArc : public CGameObjectExtensionHelper<CLightningArc, IGameObjectExtension>
{
public:
	CLightningArc();

	virtual void GetMemoryUsage(ICrySizer *pSizer) const;
	virtual bool Init(IGameObject* pGameObject);
	virtual void PostInit( IGameObject * pGameObject );
	virtual void InitClient(int channelId);
	virtual void PostInitClient(int channelId);
	virtual bool ReloadExtension( IGameObject * pGameObject, const SEntitySpawnParams &params );
	virtual void PostReloadExtension( IGameObject * pGameObject, const SEntitySpawnParams &params );
	virtual bool GetEntityPoolSignature( TSerialize signature );
	virtual void Release();
	virtual void FullSerialize( TSerialize ser );
	virtual bool NetSerialize( TSerialize ser, EEntityAspects aspect, uint8 profile, int pflags );
	virtual void PostSerialize();
	virtual void SerializeSpawnInfo( TSerialize ser );
	virtual ISerializableInfoPtr GetSpawnInfo();
	virtual void Update( SEntityUpdateContext& ctx, int updateSlot );
	virtual void HandleEvent( const SGameObjectEvent& event );
	virtual void ProcessEvent( const SEntityEvent& event );	
	virtual uint64 GetEventMask() const;
	virtual void SetChannelId(uint16 id);
	virtual const void * GetRMIBase() const;
	virtual void PostUpdate( float frameTime );
	virtual void PostRemoteSpawn();

	void TriggerSpark();
	void Enable(bool enable);
	void ReadLuaParameters();

private:
	void Reset(bool jumpingIntoGame);

	const char* m_lightningPreset;
	float m_delay;
	float m_delayVariation;
	float m_timer;
	bool m_enabled;
	bool m_inGameMode;
};



#endif
