// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

// -------------------------------------------------------------------------
//  File name:   bugsflock.cpp
//  Version:     v1.00
//  Created:     11/4/2003 by Timur.
//  Compilers:   Visual Studio.NET
//  Description: 
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////

#include "StdAfx.h"
#include "BugsFlock.h"
#include <CryEntitySystem/IEntitySystem.h>
#include <CryAnimation/ICryAnimation.h>

#define BUGS_SCARE_DISTANCE 3.0f

enum EBugsFlockBehaviors
{
	EBUGS_BUG,
	EBUGS_DRAGONFLY,
	EBUGS_FROG,
};


//////////////////////////////////////////////////////////////////////////
CBugsFlock::CBugsFlock( IEntity *pEntity )
:	CFlock( pEntity,EFLOCK_BUGS )
{
	m_boidDefaultAnimName = "fly_loop";
}

//////////////////////////////////////////////////////////////////////////
CBugsFlock::~CBugsFlock()
{
}

//////////////////////////////////////////////////////////////////////////
void CBugsFlock::CreateBoids( SBoidsCreateContext &ctx )
{
	CFlock::CreateBoids(ctx);
	
	int i;

	if (!m_e_flocks)
		return;

	if (m_pEntity)
	{
		m_pEntity->ClearSlots();
	}

	// Different boids.
	for (i = 0; i < ctx.boidsCount; i++)
	{
		CBoidBug *boid = new CBoidBug( m_bc );
		float radius = m_bc.fSpawnRadius;
		boid->m_pos = m_origin + Vec3(radius*Boid::Frand(),radius*Boid::Frand(),
			m_bc.MinHeight+(m_bc.MaxHeight-m_bc.MinHeight)*Boid::Frand() );

		boid->m_heading = Vec3(Boid::Frand(),Boid::Frand(),0).GetNormalized();
		boid->m_speed = m_bc.MinSpeed + (m_bc.MaxSpeed-m_bc.MinSpeed)*Boid::Frand();
		boid->m_scale = m_bc.boidScale + Boid::Frand()*m_bc.boidRandomScale;

		/*
		//boid->CalcRandomTarget( GetPos(),m_bc );
		if (!ctx.characterModel.empty())
		{
			if (numObj == 0 || (i % (numObj+1) == 0))
			{
				m_pEntity->LoadCharacter( i,ctx.characterModel );
				boid->m_object = m_pEntity->GetCharacter(i);
				if (boid->m_object)
				{
					AABB aabb=boid->m_object->GetAABB();
					bbox.Add( aabb.min );
					bbox.Add( aabb.max );
					if (!ctx.animation.empty())
					{
						CryCharAnimationParams animParams;
						animParams.m_nFlags |= CA_REMOVE_FROM_FIFO;

						// Play animation in loop.
						boid->m_object->GetISkeleton()->StartAnimation( ctx.animation.c_str(),0, 0,0,  animParams );
						IAnimationSet* animSet = boid->m_object->GetIAnimationSet();
						if (animSet)
						{
							//Ivo: this is an animation flag in the new system
							//boid->m_object->SetLoop( ctx.animation.c_str(), true, 0 );
						}
					}
				}
			}
		}
	
		if (numObj > 0)
			boid->m_objectId = (i % numObj);
		else
			boid->m_objectId = 0;

		if (!boid->m_object)
		{
			m_pEntity->LoadGeometry( i,ctx.models[boid->m_objectId] );
		}
		*/

		//boid->m_p = gEnv->p3DEngine->MakeCharacter( model.c_str() );
		/*
		if (boid->m_object)
		{
			boid->Physicalize(m_bc);
		}
		*/
		AddBoid(boid);
	}
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
CBoidBug::CBoidBug( SBoidContext &bc )
: CBoidObject( bc )
{
	m_objectId = 0;
	m_onGround = 0;
}

void CBoidBug::UpdateBugsBehavior( float dt,SBoidContext &bc )
{
	if (cry_random(0, 9) == 0)
	{
		// Randomally modify heading vector.
		m_heading.x += Boid::Frand()*0.2f*bc.factorAlignment; // Used as random movement.
		m_heading.y += Boid::Frand()*0.2f*bc.factorAlignment;
		m_heading.z += Boid::Frand()*0.1f*bc.factorAlignment;
		m_heading = m_heading.GetNormalized();
		if (bc.behavior == EBUGS_DRAGONFLY)
			m_speed = bc.MinSpeed + (bc.MaxSpeed - bc.MinSpeed)*Boid::Frand();
	}

	// Avoid player.
	Vec3 fromPlayerVec = Vec3( m_pos.x-bc.playerPos.x, m_pos.y-bc.playerPos.y, 0 );
	if ((fromPlayerVec).GetLengthSquared() < BUGS_SCARE_DISTANCE*BUGS_SCARE_DISTANCE) // 2 meters.
	{
		float d = (BUGS_SCARE_DISTANCE - fromPlayerVec.GetLength());
		m_accel += 5.0f * fromPlayerVec * d;
	}

	// Maintain average speed.
	float targetSpeed = (bc.MaxSpeed + bc.MinSpeed)/2;
	m_accel -= m_heading*(m_speed-targetSpeed)*0.5f;

	//m_accel = (m_targetPos - m_pos)*bc.factorAttractToOrigin;

	if (m_pos.z < bc.terrainZ+bc.MinHeight)
	{
		m_accel.z = (bc.terrainZ+bc.MinHeight-m_pos.z)*bc.factorAttractToOrigin;
	}
	else if (m_pos.z > bc.terrainZ+bc.MaxHeight)
	{
		m_accel.z = -(m_pos.z-bc.terrainZ+bc.MinHeight)*bc.factorAttractToOrigin;
	}
	else
	{
		// Always try to accelerate in direction oposite to current in Z axis.
		m_accel.z += -m_heading.z * 0.2f;
	}
}

//////////////////////////////////////////////////////////////////////////
void CBoidBug::UpdateDragonflyBehavior( float dt,SBoidContext &bc )
{
	UpdateBugsBehavior( dt,bc );	
}

//////////////////////////////////////////////////////////////////////////
void CBoidBug::UpdateFrogsBehavior( float dt,SBoidContext &bc )
{
	if (m_onGround)
	{
		if ((cry_random(0, 99) == 1) ||
			((Vec3(bc.playerPos.x-m_pos.x,bc.playerPos.y-m_pos.y,0).GetLengthSquared()) < BUGS_SCARE_DISTANCE*BUGS_SCARE_DISTANCE))
		{
			// Sacred by player or random jump.
			m_onGround = false;
			m_heading = m_pos - bc.playerPos;
			if (m_heading != Vec3(0,0,0))
				m_heading = m_heading.GetNormalized();
			else
				m_heading = Vec3(Boid::Frand(),Boid::Frand(),Boid::Frand()).GetNormalized();
			m_heading.z = 0.2f + (Boid::Frand()+1.0f)*0.5f;
			m_heading += Vec3(Boid::Frand()*0.3f,Boid::Frand()*0.3f,0 );
			if (m_heading != Vec3(0,0,0))
				m_heading = m_heading.GetNormalized();
			else
				m_heading = Vec3(Boid::Frand(),Boid::Frand(),Boid::Frand()).GetNormalized();
			m_speed = bc.MaxSpeed;
		}
	}

	bc.terrainZ = bc.engine->GetTerrainElevation(m_pos.x,m_pos.y);

	float range = bc.MaxAttractDistance;

	Vec3 origin = bc.flockPos;

	if (bc.followPlayer)
	{
		origin = bc.playerPos;
	}

	// Keep in range.
	if (bc.followPlayer)
	{
		if (m_pos.x < origin.x - range)
			m_pos.x = origin.x + range;
		if (m_pos.y < origin.y - range)
			m_pos.y = origin.y + range;
		if (m_pos.x > origin.x + range)
			m_pos.x = origin.x - range;
		if (m_pos.y > origin.y + range)
			m_pos.y = origin.y - range;
	}
	else
	{
		/*
		if (bc.behavior == EBUGS_BUG || bc.behavior == EBUGS_DRAGONFLY)
		{
			if (m_pos.x < origin.x-range)
				m_accel = (origin - m_pos)*bc.factorAttractToOrigin;
			if (m_pos.y < origin.y-range)
				m_accel = (origin - m_pos)*bc.factorAttractToOrigin;
			if (m_pos.x > origin.x+range)
				m_accel = (origin - m_pos)*bc.factorAttractToOrigin;
			if (m_pos.y > origin.y+range)
				m_accel = (origin - m_pos)*bc.factorAttractToOrigin;
		}
		*/
	}
	m_accel.Set( 0,0,-10 );

	bool bBanking = m_object != 0;
	CalcMovement( dt,bc,bBanking );
	
	UpdateAnimationSpeed(bc);

	if (m_pos.z < bc.terrainZ+0.1f)
	{
		// Land.
		m_pos.z = bc.terrainZ+0.1f;
		m_onGround = true;
		m_speed = 0;
	}
}

//////////////////////////////////////////////////////////////////////////
void CBoidBug::Update( float dt,SBoidContext &bc )
{
	if (bc.behavior == EBUGS_FROG)
	{
		UpdateFrogsBehavior( dt,bc );
		return;
	}

	if (m_onGround)
	{
		if ((Vec3(bc.playerPos.x-m_pos.x,bc.playerPos.y-m_pos.y,0).GetLengthSquared()) < BUGS_SCARE_DISTANCE*BUGS_SCARE_DISTANCE)
		{
			// Sacred by player, fast takeoff.
			m_onGround = false;
			m_heading = (m_pos - bc.playerPos).GetNormalized();
			m_heading.z = 0.3f;
			m_heading = (m_heading).GetNormalized();
			m_speed = 1;
		}
		else if (cry_random(0, 49) == 0)
		{
			// take off.
			m_onGround = false;
			m_heading.z = 0.2f;
			m_heading = (m_heading).GetNormalized();
		}
		return;
	}
	// Keep in range.
	bc.terrainZ = bc.engine->GetTerrainElevation(m_pos.x,m_pos.y);

	float range = bc.MaxAttractDistance;

	Vec3 origin = bc.flockPos;

	if (bc.followPlayer)
	{
		origin = bc.playerPos;
	}
	
	if (bc.followPlayer)
	{
		if (m_pos.x < origin.x - range)
			m_pos.x = origin.x + range;
		if (m_pos.y < origin.y - range)
			m_pos.y = origin.y + range;
		if (m_pos.x > origin.x + range)
			m_pos.x = origin.x - range;
		if (m_pos.y > origin.y + range)
			m_pos.y = origin.y - range;
	}
	else
	{
		if (bc.behavior == EBUGS_BUG || bc.behavior == EBUGS_DRAGONFLY)
		{
			if (m_pos.x < origin.x-range)
				m_accel = (origin - m_pos)*bc.factorAttractToOrigin;
			if (m_pos.y < origin.y-range)
				m_accel = (origin - m_pos)*bc.factorAttractToOrigin;
			if (m_pos.x > origin.x+range)
				m_accel = (origin - m_pos)*bc.factorAttractToOrigin;
			if (m_pos.y > origin.y+range)
				m_accel = (origin - m_pos)*bc.factorAttractToOrigin;
		}
	}

	if (bc.behavior == EBUGS_BUG)
	{
		UpdateBugsBehavior( dt,bc );
	}
	else 	if (bc.behavior == EBUGS_DRAGONFLY)
	{
		UpdateDragonflyBehavior( dt,bc );
	}
	else 	if (bc.behavior == EBUGS_FROG)
	{
		UpdateFrogsBehavior( dt,bc );
	}
	else
	{
		UpdateBugsBehavior( dt,bc );
	}

	bool bBanking = m_object != 0;
	CalcMovement( dt,bc,bBanking );
	
	UpdateAnimationSpeed(bc);

	if (m_pos.z < bc.terrainZ+0.1f)
	{
		// Land.
		m_pos.z = bc.terrainZ+0.1f;
		if (!bc.noLanding && cry_random(0, 9) == 0)
		{
			m_onGround = true;
			m_speed = 0;
			m_accel.Set(0,0,0);
		}
	}

	if (m_pos.z < bc.waterLevel)
		m_pos.z = bc.waterLevel;
}

//////////////////////////////////////////////////////////////////////////
void CBoidBug::Render( SRendParams &rp,CCamera &cam,SBoidContext &bc )
{
	/*
	// Cull boid.
	if (!cam.IsSphereVisibleFast( Sphere(m_pos,bc.fBoidRadius*bc.boidScale) ))
		return;

	Vec3 oldheading;
	if (bc.behavior == EBUGS_FROG)
	{
		// Frogs/grasshopers do not change z orientation.
		oldheading = m_heading;
		if (fabsf(m_heading.x > 0.0001f) || fabsf(m_heading.y > 0.0001f))
			m_heading = Vec3(m_heading.x,m_heading.y,0 ).GetNormalized();
		else
		{
			m_heading = Vec3(Boid::Frand(),Boid::Frand(),Boid::Frand()).GetNormalized();
			oldheading = m_heading;
		}
	}

	Matrix44 mtx;
	CalcMatrix( mtx );
	mtx.ScaleMatRow( Vec3(bc.boidScale,bc.boidScale,bc.boidScale) );

	if (bc.behavior == EBUGS_FROG)
	{
		m_heading = oldheading;
	}

	rp.pMatrix = &mtx;

	if (m_object)
	{
		m_object->Update();
		m_object->Draw( rp, Vec3(zero) );
	}
	else
	{
		//pStatObj
		CBugsFlock *flock = (CBugsFlock*)m_flock;
		int numo = flock->m_objects.size();
		if (numo == 0)
			return;
		IStatObj *pObj = flock->m_objects[m_objectId % numo];
		if (!pObj)
			return;

		pObj->Render( rp, Vec3(zero),0);
	}
	*/
}
