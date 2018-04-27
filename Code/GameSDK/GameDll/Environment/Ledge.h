// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

/*************************************************************************
-------------------------------------------------------------------------
$Id$
$DateTime$
Description: Object for ledge placement in the levels

-------------------------------------------------------------------------
History:
- 25:02:2012   Created by Benito Gangoso Rodriguez

*************************************************************************/
#pragma once

#ifndef _LEDGE_H_
#define _LEDGE_H_

#include <IGameObject.h>

class CLedgeObject : public CGameObjectExtensionHelper<CLedgeObject, IGameObjectExtension>
{
	struct LedgeProperties
	{
		LedgeProperties(const IEntity& entity);

		float ledgeCornerMaxAngle;
		float ledgeCornerEndAdjustAmount;
		bool ledgeFlipped;
		uint16 ledgeFlags_MainSide;
		uint16 ledgeFlags_OppositeSide;


	private:
		LedgeProperties();
	};

public:

	CLedgeObject();
	virtual ~CLedgeObject();

	// IGameObjectExtension
	virtual bool Init( IGameObject * pGameObject );
	virtual void InitClient( int channelId ) {};
	virtual void PostInit( IGameObject * pGameObject );
	virtual void PostInitClient( int channelId ) {};
	virtual bool ReloadExtension( IGameObject * pGameObject, const SEntitySpawnParams &params );
	virtual void PostReloadExtension( IGameObject * pGameObject, const SEntitySpawnParams &params ) {}
	virtual bool GetEntityPoolSignature( TSerialize signature );
	virtual void Release();
	virtual void FullSerialize( TSerialize ser ) {};
	virtual bool NetSerialize( TSerialize ser, EEntityAspects aspect, uint8 profile, int flags ) { return false; };
	virtual void PostSerialize() {};
	virtual void SerializeSpawnInfo( TSerialize ser ) {}
	virtual ISerializableInfoPtr GetSpawnInfo() {return 0;}
	virtual void Update( SEntityUpdateContext& ctx, int slot ) { };
	virtual void HandleEvent( const SGameObjectEvent& gameObjectEvent );
	virtual void ProcessEvent( const SEntityEvent& entityEvent );
	virtual uint64 GetEventMask() const;
	virtual void SetChannelId( uint16 id ) {};
	virtual void PostUpdate( float frameTime ) { CRY_ASSERT(false); }
	virtual void PostRemoteSpawn() {};
	virtual void GetMemoryUsage( ICrySizer *pSizer ) const;
	// ~IGameObjectExtension

protected:

	virtual bool IsStatic() const
	{
		return false;
	}

private:

	void UpdateLocation();
	void ComputeLedgeMarkers();
	
	ILINE bool IsFlipped() const { return m_flipped; };

	bool m_flipped;
};

//////////////////////////////////////////////////////////////////////////

class CLedgeObjectStatic : public CLedgeObject
{
public:

	CLedgeObjectStatic();
	virtual ~CLedgeObjectStatic();

protected:

	virtual bool IsStatic() const
	{
		return true;
	}

};

#endif //_LEDGE_H_