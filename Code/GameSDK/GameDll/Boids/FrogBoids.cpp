// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

// -------------------------------------------------------------------------
//  File name:   FrogBoids.cpp
//  Created:     28/4/2007 by Timur.
//  Description: 
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////

#include "StdAfx.h"
#include "FrogBoids.h"
#include <CrySystem/ITimer.h>
#include <CryAnimation/ICryAnimation.h>

#define MIN_REST_TIME 2.0f
#define MAX_REST_TIME 4.0f
#define MAX_WALK_TIME 5.0f

#define FROG_JUMP_ANIM   0
#define FROG_IDLE_ANIM   1
#define FROG_PICKUP_ANIM 4
#define FROG_THROW_ANIM  5

#define FROG_SOUND_IDLE    0
#define FROG_SOUND_SCARED    1
#define FROG_SOUND_DIE     2

#define MAX_FROG_SCARE_DISTANCE 3
#define MAX_FROG_DISTANCE_FROM_PLAYER 30

//////////////////////////////////////////////////////////////////////////
CFrogFlock::CFrogFlock( IEntity *pEntity )
: CFlock( pEntity,EFLOCK_FROGS )
{
	m_boidEntityName = "FrogBoid";
	m_nBoidEntityFlagsAdd = ENTITY_FLAG_CASTSHADOW;
};

//////////////////////////////////////////////////////////////////////////
void CFrogFlock::CreateBoids( SBoidsCreateContext &ctx )
{
	m_bc.noLanding = true;

	m_bc.vEntitySlotOffset = Vec3(0,0,-0.5f); // Offset down by half boid radius.

	if (FROG_IDLE_ANIM < m_bc.animations.size())
		m_boidDefaultAnimName = m_bc.animations[FROG_IDLE_ANIM];

	CFlock::CreateBoids(ctx);

	for (uint32 i = 0; i < m_RequestedBoidsCount; i++)
	{
		CBoidObject *boid = new CFrogBoid( m_bc );
		float radius = m_bc.fSpawnRadius;
		boid->m_pos = m_origin + Vec3(radius*Boid::Frand(),radius*Boid::Frand(),Boid::Frand()*radius);
		boid->m_pos.z = m_bc.engine->GetTerrainElevation(boid->m_pos.x,boid->m_pos.y) + m_bc.fBoidRadius*0.5f;
		boid->m_heading = ( Vec3(Boid::Frand(),Boid::Frand(),0) ).GetNormalized();
		boid->m_scale = m_bc.boidScale + Boid::Frand()*m_bc.boidRandomScale;

		AddBoid(boid);
	}	
}


//////////////////////////////////////////////////////////////////////////
bool CFrogFlock::CreateEntities()
{
	for (int i = 0,num = m_boids.size(); i < num; i++)
	{
		CBoidObject *boid = m_boids[i];
		boid->m_pos.z = m_bc.engine->GetTerrainElevation(boid->m_pos.x,boid->m_pos.y) + m_bc.fBoidRadius*0.5f;
	}	
	return CFlock::CreateEntities();
}

//////////////////////////////////////////////////////////////////////////
CFrogBoid::CFrogBoid(SBoidContext &bc)
	: CBoidBird(bc)
	, m_avoidanceAccel(Vec3(ZERO))
	, m_pOnWaterSplashFunc(nullptr)
	, m_bThrown(false)
	, m_bIsInsideWater(false)
	, m_fTimeToNextJump(0.0f)
	, m_onGround(0)
{
	m_alignHorizontally = 1.0f;
	m_maxIdleTime = cry_random(MIN_REST_TIME, MAX_REST_TIME);
	m_maxActionTime = cry_random(0.0f, MAX_WALK_TIME);
}

CFrogBoid::~CFrogBoid()
{
	if(m_pOnWaterSplashFunc)
		gEnv->pScriptSystem->ReleaseFunc( m_pOnWaterSplashFunc );
}

//////////////////////////////////////////////////////////////////////////
void CFrogBoid::OnPickup( bool bPickup,float fSpeed )
{
	if (bPickup)
	{
		m_physicsControlled = true;
		PlayAnimationId( FROG_PICKUP_ANIM,true );
		//PlaySound(FROG_SOUND_PICKUP);
	}
	else
	{
		m_bThrown = true;
		//PlaySound(FROG_SOUND_THROW);

		if (fSpeed > 5.0f && !m_invulnerable)
		{
			Kill( m_pos,Vec3(0,0,0) );
		}
		else
		{
			// Wait until fall to the ground and go resting.
			PlayAnimationId( FROG_THROW_ANIM,true );
		}

		if (m_pPhysics && !m_dead)
		{
			// Enabling reporting of physics collisions.
			pe_params_flags pf;
			pf.flagsOR = pef_log_collisions;
			m_pPhysics->SetParams(&pf);
		}
	}
}

//////////////////////////////////////////////////////////////////////////
void CFrogBoid::OnCollision(const SEntityEvent &event)
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
			else
			{
				pe_status_pos ppos;
				m_pPhysics->GetStatus(&ppos);
				float z = gEnv->p3DEngine->GetTerrainElevation(ppos.pos.x,ppos.pos.y);
				if (fabs(z-ppos.pos.z) < 0.5f)
				{
					m_bThrown = false;
					m_physicsControlled = false;
				}
			}
		}
	}
}

//////////////////////////////////////////////////////////////////////////
void CFrogBoid::Kill( const Vec3 &hitPoint,const Vec3 &force )
{
	CBoidBird::Kill(hitPoint,force);
	ExecuteTrigger(FROG_SOUND_DIE);
}

//////////////////////////////////////////////////////////////////////////
void CFrogBoid::Physicalize( SBoidContext &bc )
{
	bc.fBoidThickness = bc.fBoidRadius;
	CBoidObject::Physicalize(bc);
}

//////////////////////////////////////////////////////////////////////////
void CFrogBoid::Update( float dt,SBoidContext &bc )
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
		if (m_pos.GetSquaredDistance(bc.playerPos) > MAX_FROG_DISTANCE_FROM_PLAYER*MAX_FROG_DISTANCE_FROM_PLAYER)
		{
			m_pos = bc.playerPos + Vec3(Boid::Frand()*MAX_FROG_DISTANCE_FROM_PLAYER,Boid::Frand()*MAX_FROG_DISTANCE_FROM_PLAYER,0 );
			m_pos.z = bc.engine->GetTerrainElevation(m_pos.x,m_pos.y) + bc.fBoidRadius*0.5f;
			m_speed = bc.MinSpeed + ((Boid::Frand()+1)/2.0f) / (bc.MaxSpeed - bc.MinSpeed);
			m_heading = Vec3(Boid::Frand(),Boid::Frand(),0).GetNormalized();
		}
	}

	Think(dt,bc);
	if (!m_onGround)
	{
		// Calc movement with current velocity.
		CalcMovement( dt,bc,false );
		UpdateAnimationSpeed(bc);
		m_accel.Set(0,0,0);
	}

	UpdateWaterInteraction(bc);
}

void CFrogBoid::UpdateWaterInteraction(const SBoidContext &bc)
{
	if(!m_bIsInsideWater && m_pos.z < bc.waterLevel)
	{
		OnEnteringWater(bc);
		m_bIsInsideWater = true;
	}

	m_bIsInsideWater = m_pos.z < bc.waterLevel;
}

void CFrogBoid::OnEnteringWater(const SBoidContext &bc)
{
	if (!bc.entity)
		return;

	IScriptTable *pEntityTable = bc.entity->GetScriptTable();
	if (!pEntityTable)
		return;

	// Calling script function
	if(!m_pOnWaterSplashFunc)
	{
		pEntityTable->GetValue( "OnWaterSplash",m_pOnWaterSplashFunc );
	}

	if(m_pOnWaterSplashFunc)
		Script::Call( gEnv->pScriptSystem,m_pOnWaterSplashFunc,pEntityTable,m_pos );

}

//////////////////////////////////////////////////////////////////////////
void CFrogBoid::Think( float dt,SBoidContext &bc )
{
	Vec3 flockHeading(0,0,0);

	m_accel(0,0,0);

	bool bScaredJump = false;
	//////////////////////////////////////////////////////////////////////////
	// Scare points also scare chicken off.
	//////////////////////////////////////////////////////////////////////////
	if (bc.scareRatio > 0)
	{
		float sqrScareDist = m_pos.GetSquaredDistance(bc.scarePoint);
		if (sqrScareDist < bc.scareRadius*bc.scareRadius)
		{
			bScaredJump = true;
		}
	}
	//////////////////////////////////////////////////////////////////////////

	m_fTimeToNextJump -= dt;
	if (m_onGround)
	{
		float fScareDist = MAX_FROG_SCARE_DISTANCE;
		float sqrPlayerDist = m_pos.GetSquaredDistance(bc.playerPos);
		if (m_fTimeToNextJump <= 0 || sqrPlayerDist < fScareDist*fScareDist || bScaredJump)
		{
			if (cry_random(0, 127) == 0)
				ExecuteTrigger(FROG_SOUND_SCARED);
			PlayAnimationId( FROG_JUMP_ANIM,false,0 );
			
			m_fTimeToNextJump = cry_random(2.0f, 7.0f);
			//m_fTimeToNextJump = 0;

			// Scared by player or random jump.
			m_onGround = false;
			m_heading = m_pos - bc.playerPos;
			
			if (bScaredJump)
			{
				// Jump from scare point.
				m_heading = Vec3(m_pos - bc.scarePoint).GetNormalized();
			}
			else if (sqrPlayerDist < fScareDist*fScareDist)
			{
				// Jump from player.
				m_heading = Vec3(m_pos - bc.playerPos).GetNormalized();
			}
			else
			{
				if (m_heading != Vec3(0,0,0))
				{
					m_heading = m_heading.GetNormalized();
				}
				else
					m_heading = Vec3(Boid::Frand(),Boid::Frand(),Boid::Frand()).GetNormalized();

				if (m_pos.GetSquaredDistance(bc.flockPos) > bc.fSpawnRadius)
				{
					// If we are too far from spawn radius, jump back.
					Vec3 jumpToOrigin = Vec3(	bc.flockPos.x+Boid::Frand()*bc.fSpawnRadius,bc.flockPos.y+Boid::Frand()*bc.fSpawnRadius,bc.flockPos.z+Boid::Frand()*bc.fSpawnRadius );
					m_heading = Vec3(jumpToOrigin-m_pos).GetNormalized();
				}
			}
			
			m_heading += Vec3(Boid::Frand()*0.4f,Boid::Frand()*0.4f,0 );
			m_heading.Normalize();
			m_heading.z = 0.5f + (Boid::Frand()+1.0f)*0.3f;
			m_heading.Normalize();

			if (bc.avoidObstacles)
			{
				int retries = 4;
				bool bCollision;
				do 
				{
					bCollision = false;
					// Avoid obstacles & terrain.
					IPhysicalWorld *physWorld = bc.physics;

					Vec3 vPos = m_pos + Vec3(0,0,bc.fBoidRadius*0.5f);
					Vec3 vDir = m_heading*(bc.fBoidRadius*5) + Vec3(0,0,bc.fBoidRadius*1.0f);

					int objTypes = ent_all|ent_no_ondemand_activation;
					int flags = rwi_stop_at_pierceable|rwi_ignore_terrain_holes;
					ray_hit hit;
					int col = physWorld->RayWorldIntersection( vPos,vDir,objTypes,flags,&hit,1 );
					if (col != 0 && hit.dist > 0)
					{
						bCollision = true;
						m_heading = Vec3(Boid::Frand(),Boid::Frand(),0 ); // Pick some random jump vector.
						m_heading.Normalize();
						m_heading.z = 0.5f + (Boid::Frand()+1.0f)*0.3f;
						m_heading.Normalize();
					}
				} while (!bCollision && retries-- > 0);
			}

			m_speed = cry_random(bc.MinSpeed, bc.MaxSpeed);
		}
	}

	bc.terrainZ = bc.engine->GetTerrainElevation(m_pos.x,m_pos.y) + bc.fBoidRadius*0.5f;

	float range = bc.MaxAttractDistance;

	Vec3 origin = bc.flockPos;

	if (bc.followPlayer)
	{
		origin = bc.playerPos;
	}

	// Keep in range.
	if (bc.followPlayer)
	{
		bool bChanged = false;
		if (m_pos.x < origin.x - range)
		{
			m_pos.x = origin.x + range;
			bChanged = true;
		}
		if (m_pos.y < origin.y - range)
		{
			m_pos.y = origin.y + range;
			bChanged = true;
		}
		if (m_pos.x > origin.x + range)
		{
			m_pos.x = origin.x - range;
			bChanged = true;
		}
		if (m_pos.y > origin.y + range)
		{
			m_pos.y = origin.y - range;
			bChanged = true;
		}
		if (bChanged)
			m_pos.z = bc.terrainZ = bc.engine->GetTerrainElevation(m_pos.x,m_pos.y) + bc.fBoidRadius*0.5f;
	}
	else
	{
	}

	if (!m_onGround)
	{
		m_accel.Set( 0,0,-10 );
	}

	if (m_pos.z < bc.terrainZ+0.001f)
	{
		// Land.
		m_pos.z = bc.terrainZ+0.001f;
		if (!m_onGround)
		{
			m_heading.z = 0;
			m_onGround = true;
			m_speed = 0;
			PlayAnimationId( FROG_IDLE_ANIM,true );
		}
	}

	// Do random idle sounds.
	if (cry_random(0, 254) == 0)
		ExecuteTrigger(FROG_SOUND_IDLE);
}
