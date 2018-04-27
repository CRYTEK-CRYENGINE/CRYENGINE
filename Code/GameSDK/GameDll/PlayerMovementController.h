// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#ifndef __PLAYERMOVEMENTCONTROLLER_H__
#define __PLAYERMOVEMENTCONTROLLER_H__

#pragma once

#include "Player.h"
#include <CrySystem/TimeValue.h>
#include <CryGame/GameUtils.h>
#include "MovementTransitionsController.h"
#include "ExactPositioning.h"
#include "CornerSmoother.h"

class CPlayerMovementController : public IActorMovementController
{
public:
	CPlayerMovementController( CPlayer * pPlayer );
	virtual ~CPlayerMovementController();

	virtual void Reset();
	virtual void Update( float frameTime, SActorFrameMovementParams& params );
	ILINE virtual void PostUpdate( float frameTime )
	{
		// GetMovementState is called 4-5 times per entity per frame, and calculating this is expensive
		// => it's better to cache it and just copy when we need it... it's constant after the update
		UpdateMovementState( m_currentMovementState );
	}

	bool HandleEvent( const SGameObjectEvent& event ); // returns true when the event was handled

	virtual void Release();

	virtual void ApplyControllerAcceleration( float& xRot, float& zRot, float dt );

	virtual bool RequestMovement( CMovementRequest& request );
	ILINE virtual void GetMovementState( SMovementState& state )
	{
		state = m_currentMovementState;
	};

	virtual bool GetStanceState( const SStanceStateQuery& query, SStanceState& state );

	virtual void Serialize(TSerialize &ser);

	virtual void GetMemoryUsage(ICrySizer *pSizer ) const;

	virtual void SetExactPositioningListener(IExactPositioningListener* pExactPositioningListener);

	// Note: this will not return an exactpositioning target if the request is still pending
	virtual const SExactPositioningTarget* GetExactPositioningTarget();

protected:
	virtual void UpdateMovementState( SMovementState& state );
	void UpdateSafeLine(const Vec3& playerPos, const Vec3& moveDirection);

protected:
	CPlayer * m_pPlayer;

	CMovementRequest m_state;
	float m_desiredSpeed;
	float m_lastRotX;
	float m_lastRotZ;
	float m_timeTurningInSameDirZ;
	float m_timeTurningInSameDirX;
	bool m_atTarget;
	bool m_usingLookIK;
	bool m_usingAimIK;
	bool m_aimClamped;
	Vec3 m_lookTarget;
	Vec3 m_aimTarget;
	float m_animTargetSpeed;
	int m_animTargetSpeedCounter;
	Vec3 m_fireTarget;
	EStance m_targetStance;
	float m_lastReqTurnSpeed;
	bool  m_wasTurning;

	SMovementState m_currentMovementState;
	
protected:
	CMovementTransitionsController m_movementTransitionsController;
	CCornerSmoother m_cornerSmoother;
	CornerSmoothing::CCornerSmoother2 m_cornerSmoother2;

	float m_zRotationRate;

	// Exact Positioning
	std::unique_ptr<CExactPositioning> m_pExactPositioning; // memory is only allocated when used (and reset whenever Reset() is called)
};

#endif
