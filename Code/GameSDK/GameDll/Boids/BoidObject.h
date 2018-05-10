// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

// -------------------------------------------------------------------------
//  File name:   BoidObject.h
//  Version:     v1.00
//  Created:     8/2010 by Luciano Morpurgo (refactored from flock.h)
//  Compilers:   Visual C++ 7.0
//  Description: BoidObject class declaration
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////

#ifndef __boidobject_h__
#define __boidobject_h__

#if _MSC_VER > 1000
#pragma once
#endif

#include <CryScriptSystem/IScriptSystem.h>
#include <CryAISystem/IAISystem.h>
//#include "Flock.h"
#include <CryPhysics/DeferredActionQueue.h>
#include "BoidCollision.h"

class CFlock;

namespace Boid
{

	// Return random value in [-1,1] range.
	inline float Frand()
	{
		return cry_random(-1.0f, 1.0f);
	}

	// Fast normalize vector and return square distance.
	inline float Normalize_fast( Vec3 &v )
	{
		return v.NormalizeSafe();
	}
}


struct SBoidContext
{
	Vec3 playerPos;
	Vec3 flockPos;

	//! Some integer for various behaviors.
	int behavior;

	float fSpawnRadius;
	float fBoidRadius;
	float fBoidThickness; // For particle physics.

	float fBoidMass;
	float fGravity;

	float terrainZ;
	float waterLevel;

	// Flock constants.
	float MinHeight;
	float MaxHeight;

	// Attraction distances.
	float MaxAttractDistance;
	float MinAttractDistance;

	float flightTime;
	// Speed.
	float MaxSpeed;
	float MinSpeed;

	float landDecelerationHeight;

	// AI factors.
	// Group behavior factors.
	float factorAlignment;
	float factorCohesion;
	float factorSeparation;
	// Other behavior factors.
	float factorAttractToOrigin;
	float factorKeepHeight;
	float factorAvoidLand;
	float factorTakeOff;
//! Cosine of boid field of view angle.
	//! Other boids which are not in field of view of this boid not considered as a neightboards.
	float cosFovAngle;
	float factorRandomAccel;

	float factorAlignmentGround;
	float factorCohesionGround;
	float factorSeparationGround;
	float factorAttractToOriginGround;
	float walkSpeed;

	//! Maximal Character animation speed.
	float MaxAnimationSpeed;

	// Settings.
	bool followPlayer;
	bool avoidObstacles;
	bool noLanding;
	bool bStartOnGround;
	bool bAvoidWater;
	bool bSpawnFromPoint;
	bool bPickableWhenAlive;
	bool bPickableWhenDead;
	
	const char* pickableMessage;

	float groundOffset;
	//! Max visible distance of flock from player.
	float maxVisibleDistance;
	float animationMaxDistanceSq;

	//! Size of boid.
	float boidScale;
	float boidRandomScale;

	I3DEngine *engine;
	IPhysicalWorld *physics;
	IEntity* entity;

	Vec3 vEntitySlotOffset;

	float fSmoothFactor;

	// One time attraction point
	Vec3 attractionPoint;

	// Ratio that limits the speed at which boid can turn.
	float fMaxTurnRatio;

	Vec3 randomFlockCenter;
	
	Vec3 scarePoint;        // Point where some scaring event happened
	float scareRadius;      // Radius from scare point where boids get scared.
	float scareRatio;       // How scarry is scare point
	float scareThreatLevel; // Level of that scare threat, comparable with AI events

	std::vector<CryAudio::ControlId> audio;
	std::vector<string> animations;

	bool bAutoTakeOff;

	float fWalkToIdleDuration;	// how long it takes from walking to coming to a rest when on ground

	// random min/max durations for 2 of the Bird's 3 ON_GROUND states (walk and idle; slowdown is determined by fWalkToIdleDuration)
	float fOnGroundWalkDurationMin;
	float fOnGroundWalkDurationMax;
	float fOnGroundIdleDurationMin;
	float fOnGroundIdleDurationMax;

	bool bInvulnerable;

	const char* GetAnimationName(int id)
	{
		if((size_t) id < animations.size())
			return animations[id].c_str();
		return NULL;
	}
};

//////////////////////////////////////////////////////////////////////////
// BoidObject.
//////////////////////////////////////////////////////////////////////////
/*! Single Boid object.
 */
class CBoidObject
{
	enum 
	{
		eSlot_Chr = 0,
		eSlot_Cgf
	};

public:
	CBoidObject( SBoidContext &bc );
	virtual ~CBoidObject();

	virtual void Update( float dt,SBoidContext &bc ) {};
	virtual void Physicalize( SBoidContext &bc );

	//! Kill this boid object.
	//! @param force Force vector applyed on dying boid (shot vector).
	virtual void Kill( const Vec3 &hitPoint,const Vec3 &force ) {};

	virtual void OnFlockMove( SBoidContext &bc ) {};
	virtual void OnEntityEvent( const SEntityEvent &event );
	virtual void CalcOrientation( Quat &qOrient );
	virtual void Render( SRendParams &rp,CCamera &cam,SBoidContext &bc );
	void CalcFlockBehavior( SBoidContext &bc,Vec3 &vAlignment,Vec3 &vCohesion,Vec3 &vSeparation );
	void CalcMovement( float dt,SBoidContext &bc,bool banking );

	void CreateRigidBox( SBoidContext &bc,const Vec3 &size,float mass,float density );
	void CreateArticulatedCharacter( SBoidContext &bc,const Vec3 &size,float mass );

	bool PlayAnimation( const char *animation,bool bLooped,float fBlendTime=0.3f );
	bool PlayAnimationId( int nIndex,bool bLooped,float fBlendTime=0.3f );
	void ExecuteTrigger(int nIndex);

	int GetGeometrySurfaceType();

	virtual void OnPickup( bool bPickup,float fSpeed ) {};
	virtual void OnCollision( const SEntityEvent &event );
	
	const Vec3& GetPos() const {return m_pos;}
	const EntityId& GetId() const {return m_entity;}

	bool IsDead() const {return m_dead || m_dying;}

	void GetMemoryUsage(ICrySizer *pSizer )const
	{
		//pSizer->AddObject(this, sizeof(*this));
		//pSizer->AddObject(m_flock);
		//pSizer->AddObject(m_object);		
	}

	CTimeValue& GetLastCollisionCheckTime() 
	{
		return m_collisionInfo.LastCheckTime();
	}

	virtual void UpdateCollisionInfo();
	
	virtual bool ShouldUpdateCollisionInfo(const CTimeValue& t);


protected:
	void DisplayCharacter(bool bEnable);
	void UpdateDisplay(SBoidContext& bc);
	virtual float GetCollisionDistance();
	virtual void UpdateAnimationSpeed(SBoidContext &bc);
	virtual void ClampSpeed(SBoidContext& bc, float dt);

public:
	//////////////////////////////////////////////////////////////////////////
	friend class CFlock;

	CFlock *m_flock;	//!< Flock of this boid.
	Vec3 m_pos;			//!< Boid position.
	Vec3 m_heading;	//!< Current heading direction.
	Vec3 m_accel;		//!< Desired acceleration vector.
	Vec3 m_currentAccel; // Current acceleration vector.
	float m_speed;		//!< Speed of bird at heading direction.
	float m_banking;	//!< Amount of banking to apply on boid.
	float m_bankingTrg;	//!< Amount of banking to apply on boid.
	float m_scale;
	ICharacterInstance *m_object;	//< Geometry of this boid.
	float m_alignHorizontally; // 0-1 to align bird horizontally when it lands.
	float m_scareRatio;  // When boid is scared it is between 0 and 1 to show how scared it is.
	EntityId m_entity;
	IPhysicalEntity	 *m_pPhysics;
	CBoidCollision m_collisionInfo;

	// Flags.
	unsigned m_dead : 1;			//! Boid is dead, do not update it.
	unsigned m_dying : 1;			//! Boid is dying.
	unsigned m_physicsControlled : 1;	//! Boid is controlled by physics.
	unsigned m_inwater : 1;		//! When boid falls in water.
	unsigned m_nodraw : 1;		//! Do not draw this boid.
	unsigned m_pickedUp : 1;	//! Boid was picked up by player.
	unsigned m_noentity : 1;  //! Entity for this boid suppose to be deleted.
	unsigned m_displayChr : 1; //! 1 ->displays chr, 0->displays cgf
	unsigned m_invulnerable : 1;	// ! 0 -> can be killed (default), 1 -> cannot be killed
};

#endif // __boidobject_h__
