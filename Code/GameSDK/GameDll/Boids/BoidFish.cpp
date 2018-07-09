// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

// -------------------------------------------------------------------------
//  File name:   boidfish.cpp
//  Version:     v1.00
//  Created:     8/2010 by Luciano Morpurgo (refactored from flock.cpp)
//  Compilers:   Visual C++ 7.0
//  Description: 
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////

#include "StdAfx.h"
#include "Flock.h"
#include "BoidFish.h"

#include <CryEntitySystem/IEntitySystem.h>
#include "BoidsProxy.h"

#include <float.h>
#include <limits.h>
#include <CrySystem/ITimer.h>
#include <CryScriptSystem/IScriptSystem.h>
#include <CryAnimation/ICryAnimation.h>
#include <CryMath/Cry_Camera.h>
#include <CryString/CryPath.h>
#include <CryEntitySystem/IBreakableManager.h>


#define FISH_PHYSICS_DENSITY 850
#define SCARE_DISTANCE 10
#define MAX_FISH_DISTANCE 200


//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
CBoidFish::CBoidFish( SBoidContext &bc )
: CBoidObject( bc )
{
	m_dead = 0;
	m_dying = 0;
	m_dyingTime = 0.0f;
	m_pOnSpawnBubbleFunc = NULL;
	m_pOnSpawnSplashFunc = NULL;
}

CBoidFish::~CBoidFish()
{
	if (m_pOnSpawnBubbleFunc)
		gEnv->pScriptSystem->ReleaseFunc( m_pOnSpawnBubbleFunc );
	if (m_pOnSpawnSplashFunc)
		gEnv->pScriptSystem->ReleaseFunc( m_pOnSpawnSplashFunc );
}

void CBoidFish::Update( float dt,SBoidContext &bc )
{
	if (m_dead)
		return;

	if (m_physicsControlled)
	{
		if (m_pPhysics)
		{
			// If fish is dead, get it position from physics.
			pe_status_pos ppos;
			m_pPhysics->GetStatus(&ppos);
			m_pos = ppos.pos;

			{
				m_dyingTime += Boid::Frand()*0.2f;
				// Apply force on this body.
				pe_action_impulse theAction;
				theAction.impulse = Vec3(sinf(m_dyingTime*0.1f),cosf(m_dyingTime*0.13f),cosf(m_dyingTime*0.171f)*2.8f) * 0.01f;
				theAction.point = m_pos + Vec3(Boid::Frand(),Boid::Frand(),Boid::Frand())*0.1f;
				theAction.iApplyTime = 0;
				theAction.ipart = 0;
				m_pPhysics->Action(&theAction);

				pe_simulation_params sym;
				sym.density = 950.0f + 200.0f*sinf(m_dyingTime);
				if (sym.density < FISH_PHYSICS_DENSITY)
					sym.density = FISH_PHYSICS_DENSITY;
				m_pPhysics->SetParams( &sym );
			}
		}
	}
	if (m_dying)
	{
		// If fish is dying it floats up to the water surface, and die there.
		//UpdateDying(dt,bc);
		m_dyingTime += dt;
		if (m_dyingTime > 60)
		{
			m_dead = true;
			m_dying = false;
			if (m_object)
				m_object->GetISkeletonAnim()->StopAnimationsAllLayers();
		}
		return;
	}


	//////////////////////////////////////////////////////////////////////////
	if (bc.followPlayer)
	{
		if (m_pos.GetSquaredDistance(bc.playerPos) > MAX_FISH_DISTANCE*MAX_FISH_DISTANCE)
		{
			float z = bc.MinHeight + (Boid::Frand()+1)/2.0f*(bc.MaxHeight - bc.MinHeight);
			m_pos = bc.playerPos + Vec3(Boid::Frand()*MAX_FISH_DISTANCE,Boid::Frand()*MAX_FISH_DISTANCE,z );
			m_speed = bc.MinSpeed + ((Boid::Frand()+1)/2.0f) / (bc.MaxSpeed - bc.MinSpeed);
			m_heading = Vec3(Boid::Frand(),Boid::Frand(),0).GetNormalized();
		}
	}

	float height = m_pos.z - bc.terrainZ;

	m_accel.Set(0,0,0);
	m_accel = bc.factorRandomAccel*Vec3(Boid::Frand(),Boid::Frand(),Boid::Frand());
	// Continue accelerating in same dir until target speed reached.
	// Try to maintain average speed of (maxspeed+minspeed)/2
	float targetSpeed = (bc.MaxSpeed + bc.MinSpeed)/2;
	m_accel -= m_heading*(m_speed-targetSpeed)*0.2f;


	
	if (bc.factorAlignment != 0)
	{
		Vec3 alignmentAccel;
		Vec3 cohesionAccel;
		Vec3 separationAccel;
		CalcFlockBehavior(bc,alignmentAccel,cohesionAccel,separationAccel);

		m_accel += alignmentAccel*bc.factorAlignment;
		m_accel += cohesionAccel*bc.factorCohesion;
		m_accel += separationAccel;
	}

	// Avoid water.
	if (m_pos.z > bc.waterLevel-1)
	{
		float h = bc.waterLevel - m_pos.z;
		float v = (1.0f - h);
		float vv = v*v;
		m_accel.z += (-vv)*bc.factorAvoidLand;

		//gEnv->pRenderer->GetIRenderAuxGeom()->DrawLine( m_pos,ColorB(0,0,255,255),m_pos+m_accel,ColorB(0,0,255,255) );
	}
	// Avoid land.
	if (height < bc.MinHeight)
	{
		float v = (1.0f - height/(bc.MinHeight+0.01f));
		float vv = v*v;
		m_accel.z += vv*bc.factorAvoidLand;
		
		// Slow down fast.
		m_accel -= m_heading*(m_speed-0.1f)*vv*bc.factorAvoidLand;
		// Go to origin.
		m_accel += (bc.flockPos - m_pos) * vv * bc.factorAvoidLand;

		//gEnv->pRenderer->GetIRenderAuxGeom()->DrawLine( m_pos,ColorB(255,0,0,255),Vec3(m_pos.x,m_pos.y,bc.terrainZ),ColorB(255,0,0,255) );
	}

	if (fabs(m_heading.z) > 0.5f)
	{
		// Always try to accelerate in direction opposite to the current in Z axis.
		m_accel.z += -m_heading.z * 0.8f;
	}

	// Attract to the origin point.
	if (bc.followPlayer)
	{
		m_accel += (bc.playerPos - m_pos) * bc.factorAttractToOrigin;
	}
	else
	{
		m_accel += (bc.randomFlockCenter - m_pos) * bc.factorAttractToOrigin;
	}

	bool bAvoidObstacles = bc.avoidObstacles;
	//////////////////////////////////////////////////////////////////////////
	// High Terrain avoidance.
	//////////////////////////////////////////////////////////////////////////
	Vec3 fwd_pos = m_pos + m_heading*1.0f; // Look ahead 1 meter.
	float fwd_z = bc.engine->GetTerrainElevation(fwd_pos.x,fwd_pos.y);
	if (fwd_z >= m_pos.z-bc.fBoidRadius)
	{
		// If terrain in front of the fish is high, enable obstacle avoidance.
		bAvoidObstacles = true;
	}

	//////////////////////////////////////////////////////////////////////////

	// Avoid collision with Terrain and Static objects.
	float fCollisionAvoidanceWeight = 10.0f;
	float fCollisionDistance = 2.0f;

	if (bAvoidObstacles)
	{
		// Avoid obstacles & terrain.
		IPhysicalWorld *physWorld = bc.physics;

		Vec3 vPos = m_pos;
		Vec3 vDir = m_heading*fCollisionDistance;
		// Add some random variation in probe ray.
		vDir.x += Boid::Frand()*0.8f;
		vDir.y += Boid::Frand()*0.8f;

		int objTypes = ent_all|ent_no_ondemand_activation;
		int flags = rwi_stop_at_pierceable|rwi_ignore_terrain_holes;
		ray_hit hit;

		//gEnv->pRenderer->GetIRenderAuxGeom()->DrawLine( vPos,ColorB(0,0,255,255),vPos+vDir,ColorB(0,0,255,255) );

		int col = physWorld->RayWorldIntersection( vPos,vDir,objTypes,flags,&hit,1 );
		if (col != 0 && hit.dist > 0)
		{
				// Turn from collided surface.
			Vec3 normal = hit.n;
			//normal.z = 0; // Only turn left/right.
			float w = (1.0f - hit.dist/fCollisionDistance);
			Vec3 R = m_heading - (2.0f*m_heading.Dot(normal))*normal;
			Boid::Normalize_fast(R);
			R += normal;
			R.z = R.z*0.2f;
			m_accel += R*(w*w)*bc.factorAvoidLand * fCollisionAvoidanceWeight;

			//gEnv->pRenderer->GetIRenderAuxGeom()->DrawLine( m_pos,ColorB(0,0,255,255),hit.pt,ColorB(0,0,255,255) );

			//gEnv->pRenderer->GetIRenderAuxGeom()->DrawLine( hit.pt,ColorB(255,0,0,255),hit.pt+R*2,ColorB(255,0,0,255) );
			//gEnv->pRenderer->GetIRenderAuxGeom()->DrawLine( m_pos,ColorB(255,0,0,255),m_pos+R*2,ColorB(255,0,0,255) );
		}
	}

	/*
	if (cry_random(0, 79) == 1)
	{
		if (m_pos.GetSquaredDistance(bc.playerPos) < 10.0f*10.0f)
		{
			// Spawn bubble.
			SpawnParticleEffect( m_pos,bc,SPAWN_BUBBLE );
		}
	}
	*/

	//////////////////////////////////////////////////////////////////////////
	// Player must scare fishes off.
	//////////////////////////////////////////////////////////////////////////
	float sqrPlayerDist = m_pos.GetSquaredDistance(bc.playerPos);
	if (sqrPlayerDist < SCARE_DISTANCE*SCARE_DISTANCE)
	{
		Vec3 retreatDir = m_pos - bc.playerPos;
		Boid::Normalize_fast(retreatDir);
		float scareFactor = (1.0f - sqrPlayerDist/(SCARE_DISTANCE*SCARE_DISTANCE));
		m_accel.x += retreatDir.x*scareFactor*bc.factorAvoidLand;
		m_accel.y += retreatDir.y*scareFactor*bc.factorAvoidLand;
	}

	//////////////////////////////////////////////////////////////////////////
	// Calc movement.
	CalcMovement( dt,bc,false );
	m_accel.Set(0,0,0);

	// Limits fishes to under water and above terrain.
	if (m_pos.z > bc.waterLevel-0.2f)
	{
		m_pos.z = bc.waterLevel-0.2f;
		if (cry_random(0, 39) == 1)
		{
			if (m_pos.GetSquaredDistance(bc.playerPos) < 10.0f*10.0f)
			{
				// Spawn splash.
				SpawnParticleEffect( m_pos,bc,SPAWN_SPLASH );
			}
		}
	}
	else if (m_pos.z < bc.terrainZ+0.2f && bc.terrainZ < bc.waterLevel)
	{
		m_pos.z = bc.terrainZ+0.2f;
	}
}

//////////////////////////////////////////////////////////////////////////
void CBoidFish::Physicalize( SBoidContext &bc )
{
	IEntity *pEntity = gEnv->pEntitySystem->GetEntity(m_entity);
	if (pEntity)
	{
		AABB aabb;
		pEntity->GetLocalBounds(aabb);
		bc.fBoidRadius = max( (aabb.max-aabb.min).GetLength() * bc.boidScale,0.001f );
		bc.fBoidThickness = bc.fBoidRadius*0.5f;
	}

	CBoidObject::Physicalize( bc );
}

//////////////////////////////////////////////////////////////////////////
void CBoidFish::SpawnParticleEffect( const Vec3 &pos,SBoidContext &bc,int nEffect )
{
	if (!bc.entity)
		return;

	IScriptTable *pEntityTable = bc.entity->GetScriptTable();
	if (!pEntityTable)
		return;

	if (!m_pOnSpawnBubbleFunc)
	{
		pEntityTable->GetValue( "OnSpawnBubble",m_pOnSpawnBubbleFunc );
	}
	if (!m_pOnSpawnSplashFunc)
	{
		pEntityTable->GetValue( "OnSpawnSplash",m_pOnSpawnSplashFunc );
	}

	HSCRIPTFUNCTION pScriptFunc = NULL;
	switch (nEffect)
	{
	case SPAWN_BUBBLE:
		pScriptFunc = m_pOnSpawnBubbleFunc;
		break;
	case SPAWN_SPLASH:
		pScriptFunc = m_pOnSpawnSplashFunc;
		break;
	}

	if (pScriptFunc)
	{
		if (!vec_Bubble)
		{
			vec_Bubble = gEnv->pScriptSystem->CreateTable();
		}
		{
			CScriptSetGetChain bubbleChain(vec_Bubble);
			bubbleChain.SetValue( "x",pos.x );
			bubbleChain.SetValue( "y",pos.y );
			bubbleChain.SetValue( "z",pos.z );
		}

		Script::Call( gEnv->pScriptSystem,pScriptFunc,pEntityTable,pos );
	}
}

//////////////////////////////////////////////////////////////////////////
void CBoidFish::Kill( const Vec3 &hitPoint,const Vec3 &force )
{
	m_flock->m_bAnyKilled = true;
	SBoidContext bc;
	m_flock->GetBoidSettings(bc);

	IEntity *pEntity = gEnv->pEntitySystem->GetEntity(m_entity);
	if (pEntity)
	{
		SpawnParams params;
		params.eAttachForm = GeomForm_Surface;
		params.eAttachType = GeomType_Render;
		gEnv->pEntitySystem->GetBreakableManager()->AttachSurfaceEffect( pEntity,0,SURFACE_BREAKAGE_TYPE("destroy"),params );
	}

	/*
	// Can`t Kill fish in MP game.
	IGame *pGame = g_pGame;
	if (pGame && pGame->GetModuleState( EGameMultiplayer ) )
		return;
		*/

	float boxSize = bc.fBoidRadius/2;
	float mass = ((boxSize/4)*boxSize*boxSize)*FISH_PHYSICS_DENSITY; // box volume * density
	Vec3 impulse = force * mass * 0.1f;

	if (!m_physicsControlled)
	{
		CreateRigidBox( bc,Vec3(boxSize/4,boxSize,boxSize),-1,950 );
		//impulse += m_heading*m_speed*mass;
		m_physicsControlled = true;
	}

	if (m_physicsControlled)
	{
		// Apply force on this body.
		pe_action_impulse theAction;
		theAction.impulse = impulse;
		theAction.point = hitPoint;
		theAction.iApplyTime = 0;
		m_pPhysics->Action(&theAction);
	}

	if (m_object && !m_dead && !m_dying)
	{
		//m_object->ResetAnimations();
		//m_object->StartAnimation( "death" );
		m_object->SetPlaybackScale( 1 );
	}

	m_dying = true;
	m_dyingTime = 0;
}



