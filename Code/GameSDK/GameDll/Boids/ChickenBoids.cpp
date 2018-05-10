// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

// -------------------------------------------------------------------------
//  File name:   ChickenBoids.cpp
//  Created:     28/4/2007 by Timur.
//  Description: 
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////

#include "StdAfx.h"
#include "ChickenBoids.h"
#include <CrySystem/ITimer.h>
#include <CryAnimation/ICryAnimation.h>

#define MIN_REST_TIME 2.0f
#define MAX_REST_TIME 4.0f
#define MAX_WALK_TIME 5.0f

#define CHICKEN_WALK_ANIM   0
#define CHICKEN_IDLE_ANIM   1
#define CHICKEN_IDLE_ANIM_NUM 3
#define CHICKEN_PICKUP_ANIM 4
#define CHICKEN_THROW_ANIM  5

#define CHICKEN_SOUND_CLUCK   0
#define CHICKEN_SOUND_SCARED  1
#define CHICKEN_SOUND_DIE     2
#define CHICKEN_SOUND_PICKUP  3
#define CHICKEN_SOUND_THROW   4

#define MAX_CHICKEN_DISTANCE_FROM_PLAYER 30


//////////////////////////////////////////////////////////////////////////
CChickenFlock::CChickenFlock( IEntity *pEntity )
: CFlock( pEntity,EFLOCK_CHICKENS )
{
	m_boidEntityName = "BirdBoid";
	m_nBoidEntityFlagsAdd = ENTITY_FLAG_CASTSHADOW;
};

//////////////////////////////////////////////////////////////////////////
void CChickenFlock::CreateBoids( SBoidsCreateContext &ctx )
{
	m_bc.noLanding = true;
	m_bc.vEntitySlotOffset = Vec3(0,0,-0.5f); // Offset down by half boid radius.

	if (CHICKEN_IDLE_ANIM < m_bc.animations.size())
		m_boidDefaultAnimName = m_bc.animations[CHICKEN_IDLE_ANIM];

	CFlock::CreateBoids(ctx);

	for (uint32 i = 0; i < m_RequestedBoidsCount; i++)
	{
		CBoidObject *boid = CreateBoid();
		float radius = m_bc.fSpawnRadius;
		boid->m_pos = m_origin + Vec3(radius*Boid::Frand(),radius*Boid::Frand(),Boid::Frand()*radius);
		boid->m_pos.z = m_bc.engine->GetTerrainElevation(boid->m_pos.x,boid->m_pos.y) + m_bc.fBoidRadius*0.5f;
		boid->m_heading = ( Vec3(Boid::Frand(),Boid::Frand(),0) ).GetNormalized();
		boid->m_scale = m_bc.boidScale + Boid::Frand()*m_bc.boidRandomScale;
		boid->m_speed = m_bc.MinSpeed;

		AddBoid(boid);
	}	
}

//////////////////////////////////////////////////////////////////////////
CChickenBoid::CChickenBoid(SBoidContext &bc)
	: CBoidBird(bc)
	, m_lastRayCastFrame(0)
	, m_avoidanceAccel(Vec3(ZERO))
	, m_bThrown(false)
	, m_bScared(false)
	, m_landing(0)
{
	m_alignHorizontally = 1.0f;
	m_maxIdleTime = cry_random(MIN_REST_TIME, MAX_REST_TIME);
	m_maxNonIdleTime = cry_random(0.0f, MAX_WALK_TIME);
}

//////////////////////////////////////////////////////////////////////////
void CChickenBoid::OnPickup( bool bPickup,float fSpeed )
{
	if (bPickup)
	{
		m_physicsControlled = true;
		if (!m_dead)
		{
			PlayAnimationId( CHICKEN_PICKUP_ANIM,true );
			ExecuteTrigger(CHICKEN_SOUND_PICKUP);
		}
	}
	else
	{
		m_landing = false;
		m_bThrown = true;
		m_physicsControlled = false;
		
		if (!m_dead)
		{
			if (fSpeed > 5.0f && !m_invulnerable)
			{
				Kill( m_pos,Vec3(0,0,0) );
			}
			else
			{
				PlayAnimationId( CHICKEN_WALK_ANIM,true );
				//m_physicsControlled = true;
			}
			ExecuteTrigger(CHICKEN_SOUND_THROW);
			// Wait until fall to the ground and go resting.
			//PlayAnimationId( CHICKEN_THROW_ANIM,true );
		}

		if (m_pPhysics && !m_dead)
		{
			// Enabling reporting of physics collisions.
			pe_params_flags pf;
			pf.flagsOR = pef_log_collisions;
			m_pPhysics->SetParams(&pf);

			pe_action_awake aa;
			aa.bAwake = 1;
			m_pPhysics->Action(&aa);
		}
	}
}

//////////////////////////////////////////////////////////////////////////
void CChickenBoid::OnCollision( const SEntityEvent &event )
{
	if (m_bThrown)
	{
		// Get speed.
		if (m_pPhysics && m_pPhysics->GetType() == PE_PARTICLE)
		{
			pe_params_particle pparams;
			m_pPhysics->GetParams(&pparams);
			if (pparams.velocity > 5.0f && !m_invulnerable)
			{
				m_bThrown = false;
				m_physicsControlled = false;
				Kill( m_pos,Vec3(0,0,0) );
			}
		}
	}
	else
		CBoidBird::OnCollision(event);
}

//////////////////////////////////////////////////////////////////////////
void CChickenBoid::Kill( const Vec3 &hitPoint,const Vec3 &force )
{
	CBoidBird::Kill(hitPoint,force);
	ExecuteTrigger(CHICKEN_SOUND_DIE);
}

//////////////////////////////////////////////////////////////////////////
void CChickenBoid::Physicalize( SBoidContext &bc )
{
	bc.fBoidThickness = bc.fBoidRadius;
	CBoidObject::Physicalize(bc);
}

//////////////////////////////////////////////////////////////////////////
void CChickenBoid::Update( float dt,SBoidContext &bc )
{
	if (m_physicsControlled)
	{
		UpdatePhysics(dt,bc);
		if (m_bThrown && m_pPhysics)
		{
			pe_status_awake tmp;
			bool bAwake = m_pPhysics->GetStatus(&tmp) != 0;
			if (!bAwake)
			{
				// Falled on ground after being thrown.
				m_bThrown = false;
				m_physicsControlled = false;
			}
		}
		return;
	}
	if (m_dead)
		return;

	m_lastThinkTime += dt;

	if (bc.waterLevel > bc.terrainZ)
		bc.terrainZ = bc.waterLevel;

	if (bc.followPlayer)
	{
		if (m_pos.GetSquaredDistance(bc.playerPos) > MAX_CHICKEN_DISTANCE_FROM_PLAYER*MAX_CHICKEN_DISTANCE_FROM_PLAYER)
		{
			float z = bc.MinHeight + (Boid::Frand()+1)/2.0f*(bc.MaxHeight - bc.MinHeight);
			m_pos = bc.playerPos + Vec3(Boid::Frand()*MAX_CHICKEN_DISTANCE_FROM_PLAYER,Boid::Frand()*MAX_CHICKEN_DISTANCE_FROM_PLAYER,z );
			m_pos.z = bc.engine->GetTerrainElevation(m_pos.x,m_pos.y) + bc.fBoidRadius*0.5f;
			m_speed = bc.MinSpeed;
			m_heading = Vec3(Boid::Frand(),Boid::Frand(),0).GetNormalized();
		}
	}

	Think(dt,bc);
	if (!m_landing)
	{
		// Calc movement with current velocity.
		CalcMovement( dt,bc,true );

		UpdateAnimationSpeed(bc);
		m_accel.Set(0,0,0);
	}
}

void CChickenBoid::CalcOrientation( Quat &qOrient )
{
	if (m_physicsControlled && m_pPhysics)
	{
		pe_status_pos ppos;
		m_pPhysics->GetStatus(&ppos);
		qOrient = ppos.q;
		return;
	}

	if (m_heading.IsZero())
		m_heading = Vec3(1,0,0);

	Vec3 dir = m_heading;

	if (m_alignHorizontally != 0)
	{
		dir.z *= (1.0f-m_alignHorizontally);
	}
	dir.NormalizeFast();

	if (m_banking != 0)
		qOrient.SetRotationVDir( dir,-m_banking*0.5f );
	else
		qOrient.SetRotationVDir( dir );


}


//////////////////////////////////////////////////////////////////////////
void CChickenBoid::Think( float dt,SBoidContext &bc )
{
	Vec3 flockHeading(0,0,0);

	m_accel(0,0,0);
//	float height = m_pos.z - bc.terrainZ;

	if (m_bThrown)
	{
		m_accel.Set(0,0,-10.0f);
		//float z = bc.engine->GetTerrainElevation(m_pos.x,m_pos.y) + bc.fBoidRadius*0.5f;
		m_pos.z = bc.engine->GetTerrainElevation(m_pos.x,m_pos.y) + bc.fBoidRadius*0.5f;
		//pe_status_pos ppos;
		//m_pPhysics->GetStatus(&ppos);
		//if (m_pos.z < z)
		{
			m_physicsControlled = false;
			m_bThrown = false;
			m_heading.z = 0;
			if (m_heading.IsZero())
				m_heading = Vec3(1,0,0);
			m_heading.Normalize();
			m_accel.Set(0,0,0);
			m_speed = bc.MinSpeed;
			m_heading.z = 0;
		}
		return;
	}

	// Free will.
	// Continue accelerating in same dir until target speed reached.
	// Try to maintain average speed of (maxspeed+minspeed)/2
	float targetSpeed = bc.MinSpeed;
	m_accel -= m_heading*(m_speed-targetSpeed)*0.4f;

	// Gaussian weight.
	m_accel.z = 0;

	m_bScared = false;

	if (bc.factorAlignment != 0)
	{
		//CalcCohesion();
		Vec3 alignmentAccel;
		Vec3 cohesionAccel;
		Vec3 separationAccel;
		CalcFlockBehavior(bc,alignmentAccel,cohesionAccel,separationAccel);

		//! Adjust for allignment,
		//m_accel += alignmentAccel.Normalized()*ALIGNMENT_FACTOR;
		m_accel += alignmentAccel*bc.factorAlignment;
		m_accel += cohesionAccel*bc.factorCohesion;
		m_accel += separationAccel;
	}

	/*
	// Avoid land.
	if (height < bc.MinHeight && !m_landing)
	{
		float v = (1.0f - height/bc.MinHeight);
		m_accel += Vec3(0,0,v*v)*bc.factorAvoidLand;
	}
	else if (height > bc.MaxHeight) // Avoid max height.
	{
		float v = (height - bc.MaxHeight)*0.1f;
		m_accel += Vec3(0,0,-v);
	}
	else
	{
		// Always try to accelerate in direction oposite to current in Z axis.
		m_accel.z = -(m_heading.z*m_heading.z*m_heading.z * 100.0f);
	}
	*/

	// Attract to origin point.
	if (bc.followPlayer)
	{
		m_accel += (bc.playerPos - m_pos) * bc.factorAttractToOrigin;
	}
	else
	{
		//m_accel += (m_birdOriginPos - m_pos) * bc.factorAttractToOrigin;
		if (cry_random(0, 31) == 1)
		{
			m_birdOriginPos = Vec3(	bc.flockPos.x+Boid::Frand()*bc.fSpawnRadius,bc.flockPos.y+Boid::Frand()*bc.fSpawnRadius,bc.flockPos.z+Boid::Frand()*bc.fSpawnRadius );
			if (m_birdOriginPos.z - bc.terrainZ < bc.MinHeight)
			{
				m_birdOriginPos.z = bc.terrainZ + bc.MinHeight;
			}
		}

		/*
		if (m_pos.x < bc.flockPos.x-bc.fSpawnRadius || m_pos.x > bc.flockPos.x+bc.fSpawnRadius ||
		m_pos.y < bc.flockPos.y-bc.fSpawnRadius || m_pos.y > bc.flockPos.y+bc.fSpawnRadius ||
		m_pos.z < bc.flockPos.z-bc.fSpawnRadius || m_pos.z > bc.flockPos.z+bc.fSpawnRadius)
		*/
		{
			m_accel += (m_birdOriginPos - m_pos) * bc.factorAttractToOrigin;
		}
	}


	// Avoid collision with Terrain and Static objects.
	float fCollisionAvoidanceWeight = 10.0f;


	//////////////////////////////////////////////////////////////////////////
	// Player must scare chickens off.
	//////////////////////////////////////////////////////////////////////////
	float fScareDist = 5.0f;
	float sqrPlayerDist = m_pos.GetSquaredDistance(bc.playerPos);
	if (sqrPlayerDist < fScareDist*fScareDist)
	{
		Vec3 retreatDir = (m_pos - bc.playerPos) + Vec3(Boid::Frand()*2.0f,Boid::Frand()*2.0f,0);
		retreatDir.NormalizeFast();
		float scareFactor = (1.0f - sqrPlayerDist/(fScareDist*fScareDist));
		m_accel.x += retreatDir.x*scareFactor*bc.factorAvoidLand;
		m_accel.y += retreatDir.y*scareFactor*bc.factorAvoidLand;

		m_bScared = true;
		if (m_landing) m_actionTime = m_maxIdleTime+1.0f; // Stop idle.
		// Do walk sounds.
		if (cry_random(0, 127) == 0)
			ExecuteTrigger(CHICKEN_SOUND_SCARED);
	}
	

	//////////////////////////////////////////////////////////////////////////
	// Scare points also scare chicken off.
	//////////////////////////////////////////////////////////////////////////
	if (bc.scareRatio > 0)
	{
		float sqrScareDist = m_pos.GetSquaredDistance(bc.scarePoint);
		if (sqrScareDist < bc.scareRadius*bc.scareRadius)
		{
			float fScareMultiplier = 10.0f;
			Vec3 retreatDir = m_pos - bc.scarePoint;
			retreatDir.NormalizeFast();
			float scareFactor = (1.0f - sqrScareDist/(bc.scareRadius*bc.scareRadius));
			m_accel.x += retreatDir.x*scareFactor*fScareMultiplier;
			m_accel.y += retreatDir.y*scareFactor*fScareMultiplier;

			if (m_landing) m_actionTime = m_maxIdleTime+1.0f; // Stop idle.

			m_bScared = true;
		}
	}

	if (cry_random(0, (m_bScared ? 127 : 254)) == 0)
		ExecuteTrigger(CHICKEN_SOUND_CLUCK);

	//////////////////////////////////////////////////////////////////////////

	const int frameID = gEnv->pRenderer->GetFrameID();
	if (bc.avoidObstacles && m_speed > 0.0f && (frameID-m_lastRayCastFrame) > 3)
	{
		// Avoid obstacles & terrain.
		IPhysicalWorld *physWorld = bc.physics;

		Vec3 vDir0 = m_heading*bc.fBoidRadius*0.5f;
		Vec3 vPos = m_pos + Vec3(0,0,bc.fBoidRadius*0.5f) + vDir0;
		Vec3 vDir = m_heading*(bc.fBoidRadius*2) + Vec3(0,0,bc.fBoidRadius*0.5f);
		// Add some random variation in probe ray.
		vDir.x += Boid::Frand()*0.5f;
		vDir.y += Boid::Frand()*0.5f;

		int objTypes = ent_all|ent_no_ondemand_activation;
		int flags = rwi_stop_at_pierceable|rwi_ignore_terrain_holes;
		ray_hit hit;
		int col = physWorld->RayWorldIntersection( vPos,vDir,objTypes,flags,&hit,1 );
		if (col != 0 && hit.dist > 0)
		{
			// Turn from collided surface.
			Vec3 normal = hit.n;
			float rayLen = vDir.GetLength();
			float w = (1.0f - hit.dist/rayLen);
			Vec3 R = m_heading - (2.0f*m_heading.Dot(normal))*normal;
			R.NormalizeFast();
			R += normal;
			//m_accel += R*(w*w)*bc.factorAvoidLand * fCollisionAvoidanceWeight;
			Vec3 accel = R*w*bc.factorAvoidLand * fCollisionAvoidanceWeight;
			m_avoidanceAccel = m_avoidanceAccel*bc.fSmoothFactor + accel*(1.0f-bc.fSmoothFactor);
		}

		m_lastRayCastFrame = frameID;
	}

	m_accel += m_avoidanceAccel;
	m_avoidanceAccel = m_avoidanceAccel*bc.fSmoothFactor;

	if (!m_landing)
	{
		m_actionTime += dt;
		if (m_actionTime > m_maxNonIdleTime && (m_pos.z > bc.waterLevel && bc.bAvoidWater))
		{
			// Play idle.
			PlayAnimationId( CHICKEN_IDLE_ANIM + cry_random(0, CHICKEN_IDLE_ANIM_NUM - 1), true );
			m_maxIdleTime = cry_random(MIN_REST_TIME, MAX_REST_TIME);
			m_landing = true;
			m_actionTime = 0;
			
			m_accel.Set(0,0,0);
			m_speed = 0;
		}
	}
	else
	{
		m_accel = m_heading;
		m_speed = 0.1f;

		m_actionTime += dt;
		if (m_actionTime > m_maxIdleTime)
		{
			m_maxNonIdleTime = cry_random(0.0f, MAX_WALK_TIME);
			PlayAnimationId( CHICKEN_WALK_ANIM,true );
			m_landing = false;
			m_actionTime = 0;
		}
	}

	// Limits birds to above water and land.
	m_pos.z = bc.engine->GetTerrainElevation(m_pos.x,m_pos.y) + bc.fBoidRadius*0.5f;
	m_accel.z = 0;

	//////////////////////////////////////////////////////////////////////////
	// Avoid water ocean..
	if (m_pos.z < bc.waterLevel && bc.bAvoidWater)
	{
		if (m_landing)
			m_actionTime = m_maxIdleTime;
		Vec3 nextpos = m_pos + m_heading;
		float farz = bc.engine->GetTerrainElevation(nextpos.x,nextpos.y) + bc.fBoidRadius*0.5f;
		if (farz > m_pos.z)
			m_accel += m_heading*bc.factorAvoidLand;
		else
			m_accel += -m_heading*bc.factorAvoidLand;
		m_accel.z = 0;
	}
	//////////////////////////////////////////////////////////////////////////
}

//////////////////////////////////////////////////////////////////////////
void CTurtleBoid::Think( float dt,SBoidContext &bc )
{
	bc.fMaxTurnRatio = 0.05f;

	if (m_bScared)
	{
		m_landing = false;
		m_actionTime = 0;
		dt = 0;
	}
	bool bWasScared = m_bScared;
	CChickenBoid::Think( dt,bc );
	if (m_bThrown)
	{
		m_pos.z = bc.engine->GetTerrainElevation(m_pos.x,m_pos.y) + bc.fBoidRadius*0.5f;
	}
	if (m_bScared && !m_bThrown)
	{
		// When scared will not move
		m_accel.Set(0,0,0);

		// If first frame scared play scared animation.
		if (!bWasScared)
			PlayAnimationId( CHICKEN_IDLE_ANIM+1,false,1.0f );
		m_landing = true;
		m_actionTime = 0;
	}
	else if (bWasScared)
	{
		// Not scared anymore, resume idle anim.
		m_maxNonIdleTime = cry_random(0.0f, MAX_WALK_TIME);
		PlayAnimationId( CHICKEN_WALK_ANIM,true );
		m_landing = false;
		m_actionTime = 0;
	}
}

//////////////////////////////////////////////////////////////////////////
CTurtleFlock::CTurtleFlock( IEntity *pEntity ) : CChickenFlock(pEntity)
{
	m_type = EFLOCK_TURTLES;
	m_boidEntityName = "TurtleBoid";
}
