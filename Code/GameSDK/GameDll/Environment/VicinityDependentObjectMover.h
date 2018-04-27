// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

/*************************************************************************
-------------------------------------------------------------------------

Description: Object that moves based on range to player

-------------------------------------------------------------------------
History:
- 10:11:2012: Created by Dean Claassen

*************************************************************************/

#pragma once

#ifndef _VICINITYDEPENDENTOBJECTMOVER_H_
#define _VICINITYDEPENDENTOBJECTMOVER_H_

#include <IGameObject.h>
#include "../State.h"

enum EObjectRangeMoverState
{
	eObjectRangeMoverState_None = 0,
	eObjectRangeMoverState_MovingTo,
	eObjectRangeMoverState_MovingBack,
	eObjectRangeMoverState_Moved,
};

//////////////////////////////////////////////////////////////////////////
/// VICINITYDEPENDENTOBJECTMOVER

class CVicinityDependentObjectMover : public CGameObjectExtensionHelper<CVicinityDependentObjectMover, IGameObjectExtension>
{
public:
	CVicinityDependentObjectMover();
	virtual ~CVicinityDependentObjectMover();

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
	DECLARE_STATE_MACHINE( CVicinityDependentObjectMover, Behavior );

	void Reset( const bool bEnteringGameMode );
	void SetUpdate();
	void ActivateOutputPortBool( const char* szPortName );
	void SetupEntity();
	void DrawSlot( const int iSlot, const bool bEnable );
	void SetState( const EObjectRangeMoverState state);

	Vec3																m_vOriginalPos;
	Vec3																m_vMoveToPos;
	float																m_fMoveToDistance;
	float																m_fMoveToDistanceSq;
	float																m_fMoveToSpeed;
	float																m_fMoveBackSpeed;
	float																m_fAreaTriggerRange;
	float																m_fAreaTriggerRangeSq;
	float																m_fBackAreaTriggerRange;
	float																m_fBackAreaTriggerRangeSq;
	float																m_fForceMoveCompleteDistanceSq; // Current world pos of entity to player pos distance, if your closer than that, the move finishes immediately to compensate for no physics
	EObjectRangeMoverState							m_currentState;
	bool																m_bUseAreaTrigger;
	bool																m_bDisableAreaTriggerOnMoveComplete;
	bool																m_bForcedReverseMoveToStartPos;
};

#endif //_VICINITYDEPENDENTOBJECTMOVER_H_