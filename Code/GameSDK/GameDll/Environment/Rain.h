// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

/*************************************************************************
-------------------------------------------------------------------------
$Id$
$DateTime$
Description: Rain entity

-------------------------------------------------------------------------
History:
- 27:04:2006: Created by Vaclav Kyba

*************************************************************************/
#ifndef __RAIN_H__
#define __RAIN_H__
#pragma once

#include <IGameObject.h>

class CRain : public CGameObjectExtensionHelper<CRain, IGameObjectExtension>
{
public:
	CRain();
	virtual ~CRain();

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

	void PreloadTextures();

	SRainParams	m_params;
	bool				m_bEnabled;

	typedef std::vector<ITexture*> TTextureList;
	TTextureList m_Textures;

private:
	CRain(const CRain&);
	CRain& operator = (const CRain&);
};

#endif //__RAIN_H__
