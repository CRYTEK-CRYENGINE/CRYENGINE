// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

// -------------------------------------------------------------------------
//  File name:   boidbird.h
//  Version:     v1.00
//  Created:     7/2010 by Luciano Morpurgo (refactored from flock.h).
//  Compilers:   Visual C++ 7.0
//  Description: 
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////

#ifndef __boidbird_h__
#define __boidbird_h__

#if _MSC_VER > 1000
#pragma once
#endif

#include "BoidObject.h"
#include "BirdEnum.h"
#include <CryScriptSystem/IScriptSystem.h>
#include <CryAISystem/IAISystem.h>


//////////////////////////////////////////////////////////////////////////
class CBoidBird : public CBoidObject
{
public:
	CBoidBird( SBoidContext &bc );
	virtual ~CBoidBird();

	virtual void Update( float dt,SBoidContext &bc );
	virtual void UpdatePhysics( float dt,SBoidContext &bc );
	virtual void Kill( const Vec3 &hitPoint,const Vec3 &force );
	virtual void OnFlockMove( SBoidContext &bc );
	virtual void Physicalize( SBoidContext &bc );
	//void Render( CCamera &cam,SBoidContext &bc );
	virtual void Think( float dt,SBoidContext &bc );
	virtual bool ShouldUpdateCollisionInfo(const CTimeValue& t);
//	virtual void OnGrab();
//	virtual void OnThrow();

	void Land();
	void TakeOff( SBoidContext &bc );
	void SetAttracted(bool bAttracted = true) { m_attractedToPt = bAttracted; m_fAttractionFactor = 0; }
	void SetSpawnFromPt(bool bSpawnFromPt = true) { m_spawnFromPt = bSpawnFromPt; }
	bool IsLanding() {return m_status == Bird::LANDING || m_status == Bird::ON_GROUND;}
	static void SetTakeOffAnimLength(float l) {m_TakeOffAnimLength = l;}

	// Parameters for birds spawned from a point
	float m_fNoCenterAttract;		// Compensates for attraction to center point
	float m_fNoKeepHeight;			// Compensates for attraction to terrain
	float m_fAttractionFactor;

protected:
	void Landed(SBoidContext& bc);
	void SetStatus(Bird::EStatus status);
	virtual void UpdateAnimationSpeed(SBoidContext& bc);
	void UpdatePitch(float dt,SBoidContext& bc);
	void CalcMovementBird(float dt,SBoidContext& bc, bool banking); // avoiding virtual functions (calcmovement is called only from CBoidBird)
	void ThinkWalk( float dt,SBoidContext &bc );
	void UpdateOnGroundAction(float dt, SBoidContext& bc);
	virtual void ClampSpeed(SBoidContext& bc,float dt);
	void LoadParticleEffects();
	void ReleaseParticleEffects();

protected:
	static float m_TakeOffAnimLength;

	float m_actionTime;	//!< Time flying/walking/standing after take off.
	float m_maxActionTime; // Time this bird can be in flight/walk/stand.
	float m_lastThinkTime; //! Time of last think operation.
	float m_elapsedSlowdownTime;	// accumulated time since we started to slow down in order to come to a rest after SBoidContext::fWalkToIdleDuration seconds
	float m_desiredHeigh; // Desired height this birds want to fly at.
	float m_startLandSpeed;
	Vec3 m_birdOriginPos;
	Vec3 m_birdOriginPosTrg;
	Vec3 m_landingPoint;
	Vec3 m_orientation;
	CTimeValue m_takeOffStartTime;
	float m_walkSpeed;
	
	CBoidCollision m_floorCollisionInfo;

	Bird::EStatus m_status;
	Bird::EOnGroundStatus m_onGroundStatus;	// sub-status for when m_status == Bird::ON_GROUND
	// Flags.

	unsigned m_attractedToPt : 1;	//! True if bird is attracted to a point
	unsigned m_spawnFromPt : 1;		//! True if bird is spawned from point
	unsigned m_landDecelerating : 1;
	unsigned m_playingTakeOffAnim : 1;

	bool m_loadedParticleEffects;
	IParticleEffect* m_pDestroyParticleEffect;
};


#endif // __flock_h__
