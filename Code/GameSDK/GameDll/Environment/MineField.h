// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

/*************************************************************************
-------------------------------------------------------------------------

Description: Minefield to handle groups of mines

-------------------------------------------------------------------------
History:
- 07:11:2012: Created by Dean Claassen

*************************************************************************/

#pragma once

#ifndef _MINE_FIELD_H_
#define _MINE_FIELD_H_

#include <IGameObject.h>

#include "SmartMine.h"

//////////////////////////////////////////////////////////////////////////
/// Mine field

class CMineField : public CGameObjectExtensionHelper<CMineField, IGameObjectExtension>
{
public:
	CMineField();
	virtual ~CMineField();

	// IGameObjectExtension
	virtual bool Init( IGameObject * pGameObject );
	virtual void InitClient( int channelId ) {};
	virtual void PostInit( IGameObject * pGameObject );
	virtual void PostInitClient( int channelId ) {};
	virtual bool ReloadExtension( IGameObject * pGameObject, const SEntitySpawnParams &params );
	virtual void PostReloadExtension( IGameObject * pGameObject, const SEntitySpawnParams &params ) {}
	virtual bool GetEntityPoolSignature( TSerialize signature );
	virtual void Release();
	virtual void FullSerialize( TSerialize ser );
	virtual bool NetSerialize( TSerialize ser, EEntityAspects aspect, uint8 profile, int flags ) { return false; };
	virtual void PostSerialize() {};
	virtual void SerializeSpawnInfo( TSerialize ser ) {}
	virtual ISerializableInfoPtr GetSpawnInfo() {return 0;}
	virtual void Update( SEntityUpdateContext& ctx, int slot );
	virtual void HandleEvent( const SGameObjectEvent& gameObjectEvent );
	virtual void ProcessEvent( const SEntityEvent& entityEvent );
	virtual uint64 GetEventMask() const;
	virtual void SetChannelId( uint16 id ) {};
	virtual void PostUpdate( float frameTime ) { CRY_ASSERT(false); }
	virtual void PostRemoteSpawn() {};
	virtual void GetMemoryUsage( ICrySizer *pSizer ) const;
	// ~IGameObjectExtension

private:
	enum EMineFieldState
	{
		eMineFieldState_Showing = 0,
		eMineFieldState_NotShowing,
	};

	enum EMineState
	{
		eMineState_Enabled		= 0x01,
		eMineState_Destroyed	= 0x02,
	};

	struct SMineData
	{
		SMineData()
		: m_state(eMineState_Enabled)
		, m_entityId(0)
		{
		}

		int					m_state;
		EntityId		m_entityId;
	};

	typedef std::vector<SMineData> TMinesData;

	void SetState( const EMineFieldState state, const bool bForce = false );
	ILINE EMineFieldState GetState() const { return m_currentState; }
	void AddToTacticalManager();
	void RemoveFromTacticalManager();
	void NotifyAllMinesEvent( const EMineGameObjectEvent event );
	void NotifyMineEvent( const EntityId targetEntity, const EMineGameObjectEvent event );
	void Reset( const bool bEnteringGameMode );
	SMineData* GetMineData( const EntityId entityId );
	void UpdateState();

	TMinesData							m_minesData;
	EMineFieldState					m_currentState;
};

#endif //_MINE_FIELD_H_