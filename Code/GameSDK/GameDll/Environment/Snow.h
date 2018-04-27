// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

/*************************************************************************
-------------------------------------------------------------------------
$Id$
$DateTime$
Description: Snow entity

-------------------------------------------------------------------------
History:
- 12:12:2012: Created by Stephen Clement

*************************************************************************/
#ifndef __SNOW_H__
#define __SNOW_H__
#pragma once

#include <IGameObject.h>

class CSnow : public CGameObjectExtensionHelper<CSnow, IGameObjectExtension>
{
public:
	CSnow();
	virtual ~CSnow();

	// IGameObjectExtension
	virtual bool Init(IGameObject *pGameObject);
	virtual void InitClient(int channelId) {};
	virtual void PostInit(IGameObject *pGameObject);
	virtual void PostInitClient(int channelId) {};
	virtual bool ReloadExtension( IGameObject * pGameObject, const SEntitySpawnParams &params );
	virtual void PostReloadExtension( IGameObject * pGameObject, const SEntitySpawnParams &params ) {}
	virtual bool GetEntityPoolSignature( TSerialize signature );
	virtual void Release();
	virtual bool NetSerialize(TSerialize ser, EEntityAspects aspect, uint8 profile, int pflags) { return true; }
	virtual void FullSerialize(TSerialize ser);
	virtual void PostSerialize() {}
	virtual void SerializeSpawnInfo( TSerialize ser ) {}
	virtual ISerializableInfoPtr GetSpawnInfo() {return 0;}
	virtual void Update( SEntityUpdateContext &ctx, int updateSlot);
	virtual void PostUpdate(float frameTime ) {};
	virtual void PostRemoteSpawn() {};
	virtual void HandleEvent( const SGameObjectEvent &);
	virtual void ProcessEvent(const SEntityEvent& );
	virtual uint64 GetEventMask() const;
	virtual void SetChannelId(uint16 id) {}
	virtual void GetMemoryUsage(ICrySizer *pSizer) const { pSizer->Add(*this); }
	
	//~IGameObjectExtension

	bool Reset();

protected:
	
	bool	m_bEnabled;
	float	m_fRadius;

	// Surface params.
	float	m_fSnowAmount;
	float	m_fFrostAmount;
	float	m_fSurfaceFreezing;
	
	// Snowfall params.
	int		m_nSnowFlakeCount;
	float	m_fSnowFlakeSize;
	float	m_fSnowFallBrightness;
	float	m_fSnowFallGravityScale;
	float	m_fSnowFallWindScale;
	float	m_fSnowFallTurbulence;
	float	m_fSnowFallTurbulenceFreq;

private:
	CSnow(const CSnow&);
	CSnow& operator = (const CSnow&);
};

#endif //__RAIN_H__
