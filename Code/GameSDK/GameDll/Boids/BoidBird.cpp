// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

// -------------------------------------------------------------------------
//  File name:   BoidBird.cpp
//  Version:     v1.00
//  Created:     72010 by Luciano Morpurgo (refactored from flock.cpp).
//  Compilers:   Visual C++ 7.0
//  Description: 
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////

#include "StdAfx.h"
#include "BoidBird.h"
#include "BirdsFlock.h"
#include <CryAnimation/ICryAnimation.h>
#include <CryEntitySystem/IBreakableManager.h>

#define MAX_BIRDS_DISTANCE 300

#define MAX_FLIGHT_TIME 10
#define MIN_FLIGHT_TIME 10
#define BIRDS_PHYSICS_DENSITY 200
#define BIRDS_PHYSICS_INWATER_DENSITY 900
#define TAKEOFF_TIME 3

#define SCARE_DISTANCE 10

float CBoidBird::m_TakeOffAnimLength = 0.f;

CBoidBird::CBoidBird(SBoidContext &bc)
	: CBoidObject(bc)
	, m_actionTime(0)
	, m_lastThinkTime(0)
	, m_maxActionTime(0)
	, m_fNoCenterAttract(0.0f)
	, m_fNoKeepHeight(0.0f)
	, m_fAttractionFactor(0.0f)
	, m_elapsedSlowdownTime(0.0f)
	, m_startLandSpeed(0.0f)
	, m_onGroundStatus(Bird::EOnGroundStatus::OGS_WALKING)
	, m_playingTakeOffAnim(0)
	, m_attractedToPt(false)
	, m_spawnFromPt(false)
	, m_loadedParticleEffects(false)
	, m_pDestroyParticleEffect(nullptr)
{
	m_floorCollisionInfo.Reset();
	SetStatus(Bird::FLYING);

	m_maxActionTime = bc.flightTime *(1.2f +Boid::Frand()* 0.2f);// /2*(MAX_FLIGHT_TIME-MIN_FLIGHT_TIME);
	m_desiredHeigh = cry_random(bc.MinHeight, bc.MaxHeight) + bc.flockPos.z;
	m_birdOriginPos = bc.flockPos;
	m_birdOriginPosTrg = m_birdOriginPos;
	m_takeOffStartTime = gEnv->pTimer->GetFrameStartTime();
	m_orientation.zero();
	m_walkSpeed = bc.walkSpeed > 0 ? bc.walkSpeed * (1 + Boid::Frand()*0.2f) :0;

	LoadParticleEffects();
}

CBoidBird::~CBoidBird()
{
	ReleaseParticleEffects();

	m_floorCollisionInfo.Reset();
}

void CBoidBird::OnFlockMove( SBoidContext &bc )
{
	m_birdOriginPos = bc.flockPos;
	m_birdOriginPosTrg = bc.flockPos;
	bc.randomFlockCenter = bc.flockPos;
}

//////////////////////////////////////////////////////////////////////////
void CBoidBird::Physicalize( SBoidContext &bc )
{
	bc.fBoidThickness = bc.fBoidRadius*0.5f;
	CBoidObject::Physicalize( bc );
}

//////////////////////////////////////////////////////////////////////////
void CBoidBird::UpdatePhysics( float dt,SBoidContext &bc )
{
	if (m_pPhysics)
	{
		pe_status_pos ppos;
		m_pPhysics->GetStatus(&ppos);
		m_pos = ppos.pos;
		Vec3 pos = m_pos;
		// When hitting water surface, increase physics density.
		if (!m_inwater && m_pos.z+bc.fBoidRadius <= bc.engine->GetWaterLevel( &pos ))
		{
			m_inwater = true;
			pe_simulation_params sym;
			sym.density = BIRDS_PHYSICS_INWATER_DENSITY;
			m_pPhysics->SetParams( &sym );
		}
	}
}

//////////////////////////////////////////////////////////////////////////
void CBoidBird::Update( float dt,SBoidContext &bc )
{
	if (m_physicsControlled)
	{
		UpdatePhysics(dt,bc);
		return;
	}
	if (m_dead)
		return;
	
	if (m_heading.IsZero())
		m_heading = Vec3(1,0,0);

	m_lastThinkTime += dt;

	{
		if (bc.followPlayer && !m_spawnFromPt)
		{
			if (m_pos.GetSquaredDistance(bc.playerPos) > MAX_BIRDS_DISTANCE*MAX_BIRDS_DISTANCE)
			{
				float z = bc.MinHeight + (Boid::Frand()+1)/2.0f*(bc.MaxHeight - bc.MinHeight);
				m_pos = bc.playerPos + Vec3(Boid::Frand()*MAX_BIRDS_DISTANCE,Boid::Frand()*MAX_BIRDS_DISTANCE,z );
				m_speed = bc.MinSpeed + ((Boid::Frand()+1)/2.0f) / (bc.MaxSpeed - bc.MinSpeed);
				m_heading = Vec3(Boid::Frand(),Boid::Frand(),0).GetNormalized();
			}
		}

		if(m_status == Bird::TAKEOFF)
		{
			float timePassed = (gEnv->pTimer->GetFrameStartTime() - m_takeOffStartTime).GetSeconds();
			if(m_playingTakeOffAnim &&  timePassed >= m_TakeOffAnimLength)
			{
				m_playingTakeOffAnim = false;
				PlayAnimationId(Bird::ANIM_FLY,true);
			}
			else if( timePassed > TAKEOFF_TIME)
			{
				SetStatus(Bird::FLYING);
			}

		}

		if(m_status == Bird::LANDING)
		{
			Vec3 vDist(m_landingPoint - m_pos);
			float dist2 = vDist.GetLengthSquared2D();
			float dist = sqrt_tpl(dist2 + vDist.z*vDist.z);

			if(dist > 0.02f && m_pos.z > m_landingPoint.z)
			{
				//if(vDist.z < 3 && m_heading)
				vDist /= dist;
				float fInterpSpeed = 2+fabs(m_heading.Dot(vDist))*3.f;
				Interpolate(m_heading,vDist, fInterpSpeed, dt);
				m_heading.NormalizeSafe(); 
			
				if(m_heading.z < vDist.z)
				{
					Interpolate(m_heading.z,vDist.z,3.0f,dt);
					m_heading.NormalizeSafe();
				}

				bool wasLandDeceleratingAlready = m_landDecelerating;
				m_accel.zero();
				m_landDecelerating = dist < bc.landDecelerationHeight;
				if(m_landDecelerating)
				{
					float newspeed= m_startLandSpeed* dist/3.f;
					if(m_speed > newspeed)
						m_speed = newspeed;
 					if(m_speed < 0.2f)
 						m_speed = 0.2f;
					if(!wasLandDeceleratingAlready)
						PlayAnimationId(Bird::ANIM_LANDING_DECELERATING, true);
				}
				else
					m_startLandSpeed = m_speed;
			}
			else
				Landed(bc);

			CalcMovementBird( dt,bc,true );
			UpdatePitch(dt,bc);
			return;

		}

		if (m_status != Bird::ON_GROUND)
		{
			Think(dt,bc);

			// Calc movement with current velocity.
			CalcMovementBird( dt,bc,true );
		}
		else 
		{
			if(bc.walkSpeed > 0 && m_onGroundStatus == Bird::OGS_WALKING)
				ThinkWalk(dt,bc);
			CalcMovementBird( dt,bc,true );
		}

		m_accel.Set(0,0,0);
		UpdatePitch(dt,bc);

		// Check if landing/on ground after think().
		if ( m_status == Bird::LANDING ||(m_dying && m_status != Bird::ON_GROUND))
		{
			float LandEpsilon = 0.5f;

			// Check if landed on water.
			if (m_pos.z-bc.waterLevel < LandEpsilon+0.1f && !m_dying)
			{
				//! From water immidiatly take off.
				//! Gives fishing effect. 
				TakeOff(bc);
			}

		}

		m_actionTime += dt;

		if (m_status == Bird::ON_GROUND )
			UpdateOnGroundAction(dt, bc);
		else
		{
			if(!bc.noLanding && m_actionTime > m_maxActionTime 
				&& !static_cast<CBirdsFlock*>(m_flock)->IsPlayerNearOrigin())
				Land();
		}
	}
}


//////////////////////////////////////////////////////////////////////////

void CBoidBird::UpdateOnGroundAction(float dt, SBoidContext& bc)
{
	if(m_actionTime > m_maxActionTime)
	{
		switch(m_onGroundStatus)
		{
		case Bird::OGS_IDLE:
			// walk again if the designer has provided us with a walk speed > 0
			if(bc.walkSpeed > 0)
			{
				m_onGroundStatus = Bird::OGS_WALKING;
				PlayAnimationId(Bird::ANIM_WALK,true);
				m_actionTime = 0;
				m_maxActionTime = cry_random(bc.fOnGroundWalkDurationMin, bc.fOnGroundWalkDurationMax);
			}
			else
			{
				// otherwise just keep idling
				PlayAnimationId(Bird::ANIM_IDLE,true);
				m_actionTime = 0;
				m_maxActionTime = cry_random(bc.fOnGroundIdleDurationMin, bc.fOnGroundIdleDurationMax);
			}
			break;

		case Bird::OGS_SLOWINGDOWN:
			// came to a rest? => start idling
			if(m_speed < 0.001f)
			{
				m_onGroundStatus = Bird::OGS_IDLE;
				PlayAnimationId(Bird::ANIM_IDLE,true);
				m_actionTime = 0;
				m_maxActionTime = cry_random(bc.fOnGroundIdleDurationMin, bc.fOnGroundIdleDurationMax);
			}
			else
			{
				// still slowing down
				m_elapsedSlowdownTime += dt;
			}
			break;

		case Bird::OGS_WALKING:
			// start slowing down until we come to a rest
			m_onGroundStatus = Bird::OGS_SLOWINGDOWN;
			m_elapsedSlowdownTime = 0.0f;
			break;

		default:
			CRY_ASSERT_MESSAGE(0, "CBoidBird::UpdateOnGroundAction: omitted EOnGroundStatus");
		}
	}
}	

//////////////////////////////////////////////////////////////////////////

void CBoidBird::ClampSpeed(SBoidContext& bc,float dt)
{
	if(m_status == Bird::ON_GROUND)
	{
		if(m_onGroundStatus == Bird::OGS_WALKING)
		{
			m_speed = m_walkSpeed;
		}
		else if(m_onGroundStatus == Bird::OGS_SLOWINGDOWN)
		{
			float elapsedTimeNormalized = clamp_tpl(m_elapsedSlowdownTime / bc.fWalkToIdleDuration, 0.0f, 1.0f);
			m_speed = LERP(m_walkSpeed, 0.0f, elapsedTimeNormalized);
		}
		else if(m_onGroundStatus == Bird::OGS_IDLE)
		{
			m_speed = 0.0f;
		}
	}
	else
	{
		if (m_speed > bc.MaxSpeed)
			m_speed = bc.MaxSpeed;
		if (m_speed < bc.MinSpeed)
			m_speed = bc.MinSpeed;
	}
}

//////////////////////////////////////////////////////////////////////////

void CBoidBird::CalcMovementBird(float dt,SBoidContext& bc,bool banking)
{
	// correct heading, can't be too vertical
	// do only some rough linear computation for optimization

	if(m_status == Bird::ON_GROUND)
	{
		if(bc.walkSpeed <= 0)
		{
			m_accel.zero();
			m_speed = 0;
			m_currentAccel.zero();
			Interpolate(m_pos,m_landingPoint, 2.0f,dt);
			return;
		}
		// Avoid obstacles & terrain.
		IPhysicalWorld *physWorld = bc.physics;

		Vec3 vDir0 = m_heading*bc.fBoidRadius*0.5f;
		Vec3 vPos = m_pos;
		vPos.z += bc.fBoidRadius*0.5f;
		Vec3 vDir(0,0,bc.fBoidRadius*1.5f);

		m_floorCollisionInfo.QueueRaycast(m_entity,vPos,vDir);
	}

	// check current raycast result
	if(m_floorCollisionInfo.IsColliding())
	{
		m_heading.z = 0;
	}

	static const float maxZ = sinf(gf_PI/4);
	static const float coeffXY = cosf(gf_PI/4);

	if(m_heading.z > maxZ)
	{
		float l = m_heading.GetLength2D();
		m_heading.x *= coeffXY/l;
		m_heading.y *= coeffXY/l;
		m_heading.z = maxZ;
		m_heading.NormalizeFast();
	}

	CBoidObject::CalcMovement(dt, bc, banking);


	if(m_floorCollisionInfo.IsColliding())
	{
		float minZ = m_floorCollisionInfo.Point().z;
		m_pos.z = minZ +bc.groundOffset ;
	}

	UpdateAnimationSpeed(bc);

}

//////////////////////////////////////////////////////////////////////////

void CBoidBird::SetStatus(Bird::EStatus status)
{
	// reset the deceleration flag when transitioning into the LANDING state
	// (that way we can realize when to start the ANIM_LANDING_DECELERATING animation)
	if(m_status != Bird::LANDING && status == Bird::LANDING)
	{
		m_landDecelerating = 0;
	}
	m_status = status;
	m_collisionInfo.SetCheckDistance(status == Bird::LANDING ? 5.f : 10.f);
}

//////////////////////////////////////////////////////////////////////////

void CBoidBird::Land()
{
	if(m_status != Bird::ON_GROUND && m_status!= Bird::LANDING)
	{
		SLandPoint lpt = static_cast<CBirdsFlock*>(m_flock)->GetLandingPoint(m_pos);
		m_landingPoint = lpt;
		if(!lpt.IsZero())
		{
			m_startLandSpeed = m_speed;
			SetStatus(Bird::LANDING);
		}
	}
}


//////////////////////////////////////////////////////////////////////////

void CBoidBird::Landed(SBoidContext& bc)
{
	SetStatus(Bird::ON_GROUND);
	static_cast<CBirdsFlock*>(m_flock)->NotifyBirdLanded();
	m_actionTime = 1000;
	m_accel.zero();
	m_heading.z = 0;
	m_heading.NormalizeSafe(Vec3Constants<float>::fVec3_OneY);
	m_speed = 0;
	m_onGroundStatus = Bird::OGS_IDLE;
	PlayAnimationId( Bird::ANIM_IDLE,true );

}


//////////////////////////////////////////////////////////////////////////
void CBoidBird::TakeOff( SBoidContext &bc )
{
	// Take-off.
	if(m_status != Bird::LANDING && m_status != Bird::ON_GROUND)
		return;
	
	SetStatus(Bird::TAKEOFF);

	static_cast<CBirdsFlock*>(m_flock)->LeaveLandingPoint(m_landingPoint);
	
	m_landingPoint.zero();

	m_actionTime = 0;
//	m_maxActionTime = MIN_FLIGHT_TIME + (Boid::Frand()+1)/2*(MAX_FLIGHT_TIME-MIN_FLIGHT_TIME);
	m_maxActionTime = bc.flightTime *(1.2f +Boid::Frand()* 0.2f);// /2*(MAX_FLIGHT_TIME-MIN_FLIGHT_TIME);
	m_desiredHeigh = bc.MinHeight + (Boid::Frand()+1)/2*(bc.MaxHeight - bc.MinHeight)+bc.flockPos.z;

	m_takeOffStartTime = gEnv->pTimer->GetFrameStartTime();

	if (m_object)
	{
		const char* takeoffName = bc.GetAnimationName(Bird::ANIM_TAKEOFF);
		if(m_TakeOffAnimLength ==0 && takeoffName != NULL && strlen(takeoffName) )
		{
			int id = m_object->GetIAnimationSet()->GetAnimIDByName(takeoffName);
			m_TakeOffAnimLength = m_object->GetIAnimationSet()->GetDuration_sec(id);
			if(m_TakeOffAnimLength ==0)
				m_TakeOffAnimLength = 0.5f;
		}
		
		m_playingTakeOffAnim = PlayAnimation( takeoffName ,false );
		if(!m_playingTakeOffAnim)
			PlayAnimationId(Bird::ANIM_FLY,true);
	}
}

//////////////////////////////////////////////////////////////////////////

void CBoidBird::UpdatePitch(float dt,SBoidContext& bc)
{
	Vec3 orientationGoal;
	if(m_status == Bird::LANDING && m_landDecelerating)
	{
		float zdiff = m_pos.z - m_landingPoint.z;
		if(zdiff <0)
			zdiff = 0;

		float rotAngle = zdiff >0.5f? gf_PI/4 : gf_PI/4 * zdiff;

		Vec3 horHeading(m_heading);
		if(horHeading.x != 0 || horHeading.y != 0)
		{
			horHeading.z = 0;
			float l = horHeading.GetLength2D();
			horHeading *= cosf(rotAngle) /l;
			orientationGoal.Set(horHeading.x,horHeading.y, sinf(rotAngle));

		}
		else 
			orientationGoal = m_heading;
		//		Matrix33 XRot = Matrix33::CreateRotationX(rotAngle);
		//orientationGoal = XRot.TransformVector(horHeading);

	}
	else if (m_status == Bird::ON_GROUND && m_heading.z!=0)
	{
		orientationGoal = m_heading;
		orientationGoal.z=0;
		orientationGoal.NormalizeSafe();
	}
	else orientationGoal = m_heading;

	if(m_orientation.IsZero())
		m_orientation = orientationGoal;
	else
		Interpolate(m_orientation, orientationGoal,2.0f,dt);
}

//////////////////////////////////////////////////////////////////////////

void CBoidBird::Think( float dt,SBoidContext &bc )
{
	m_accel.zero();
	

	float factorAlignment = bc.factorAlignment;
	float factorCohesion = bc.factorCohesion;
	float factorSeparation = bc.factorSeparation;
	float factorAvoidLand = bc.factorAvoidLand;
	float factorAttractToOrigin = bc.factorAttractToOrigin;
	float factorKeepHeight = bc.factorKeepHeight;

	if(m_status == Bird::LANDING)
	{
		//factorAlignment /= 3.f;
		factorCohesion *=10.f;
		factorSeparation = 0;
		factorAvoidLand = 0;
		factorAttractToOrigin *=10;
		factorKeepHeight = 0;
	}
	
	if (m_spawnFromPt)
	{
		// Set the acceleration of the boid
		float fHalfDesired = m_desiredHeigh * .5f;
		if (m_pos.z < fHalfDesired)
		{
			m_accel.z = 3.f;
		}
		else
		{
			m_accel.z = .5f;
			// Set z-acceleration
			float fRange = 1.f - (m_pos.z-fHalfDesired)/fHalfDesired;	// 1..0 range

			if (m_heading.z > 0.2f)
				m_accel.z = .5f + fRange;	// 2..1

			m_accel.x += m_heading.x * .1f;
			m_accel.y += m_heading.y * .1f;
		}

		if (m_pos.z > m_desiredHeigh)
			SetSpawnFromPt(false);

		// Limits birds to above water and land.
		if (m_pos.z < bc.terrainZ-0.2f)
		{
			m_pos.z = bc.terrainZ-0.2f;
		}
		if (m_pos.z < bc.waterLevel-0.2f)
		{
			m_pos.z = bc.waterLevel-0.2f;
		}
		return;
	}
	
	float height = m_pos.z - bc.flockPos.z;

	// Free will.
	// Continue accelerating in same dir until target speed reached.
	// Try to maintain average speed of (maxspeed+minspeed)/2
	float targetSpeed = (bc.MaxSpeed + bc.MinSpeed)/2;
	m_accel -= m_heading*(m_speed-targetSpeed)*0.5f;

	// Desired height.
	float dh = m_desiredHeigh - m_pos.z;
	float dhexp = -(dh*dh)/(3*3);

	dhexp = clamp_tpl(dhexp,-70.0f,70.0f); // Max values for expf
	// Gaussian weight.
	float fKeepHeight = factorKeepHeight - m_fNoKeepHeight;
	m_fNoKeepHeight = max(0.f, m_fNoKeepHeight - .01f*dt);
	m_accel.z = exp_tpl(dhexp) * fKeepHeight;

	if (factorAlignment != 0)
	{
		//CalcCohesion();
		Vec3 alignmentAccel;
		Vec3 cohesionAccel;
		Vec3 separationAccel;
		CalcFlockBehavior(bc,alignmentAccel,cohesionAccel,separationAccel);

		//! Adjust for allignment,
		//m_accel += alignmentAccel.Normalized()*ALIGNMENT_FACTOR;

		m_accel += alignmentAccel*factorAlignment;
		m_accel += cohesionAccel*factorCohesion;
		m_accel += separationAccel*factorSeparation;
	}

	// Avoid land.
	if (height < bc.MinHeight && m_status != Bird::LANDING)
	{
		float v = (1.0f - height/bc.MinHeight);
		m_accel += Vec3(0,0,v*v)*bc.factorAvoidLand;
	}
	else if (height > bc.MaxHeight)
	{		// Avoid max height.
		float v = (height - bc.MaxHeight)*0.1f;
		m_accel += Vec3(0,0,-v);
	}
	else if(m_status != Bird::TAKEOFF)
	{
		// Always try to accelerate in direction opposite to current in Z axis.
		m_accel.z = -(m_heading.z*m_heading.z*m_heading.z * 100.0f);
	}

		// Attract to origin point.
	float fAttractToOrigin = factorAttractToOrigin - m_fNoCenterAttract;
	m_fNoCenterAttract = max(0.f, m_fNoCenterAttract - .01f*dt);
	if (bc.followPlayer)
	{
		m_accel += (bc.playerPos - m_pos) * fAttractToOrigin;
	}
	else
	{
		if (cry_random(0, 31) == 1)
		{
			m_birdOriginPos = Vec3(	bc.flockPos.x+Boid::Frand()*bc.fSpawnRadius,bc.flockPos.y+Boid::Frand()*bc.fSpawnRadius,bc.flockPos.z+Boid::Frand()*bc.fSpawnRadius );
			if (m_birdOriginPos.z - bc.terrainZ < bc.MinHeight)
			{
				m_birdOriginPos.z = bc.terrainZ + bc.MinHeight;
			}
		}

		m_accel += (m_birdOriginPos - m_pos) * fAttractToOrigin;
	}

	// Attract to bc.attractionPoint
	if (m_pos.GetSquaredDistance2D(bc.attractionPoint) < 5.f)
	{
		SetAttracted(false);

		CBirdsFlock *pFlock = (CBirdsFlock *)m_flock;
		if (!pFlock->GetAttractOutput())
		{	// Activate the AttractTo flownode output
			pFlock->SetAttractOutput(true);

			// Activate the flow node output
			IEntity *pFlockEntity = pFlock->GetEntity();
			if (pFlockEntity)
			{
				IScriptTable *scriptObject = pFlockEntity->GetScriptTable();
				if (scriptObject)
					Script::CallMethod(scriptObject, "OnAttractPointReached");
			}
		}
	}
	if (m_attractedToPt)
	{
		if(m_status == Bird::ON_GROUND)
			TakeOff(bc);
		else
			SetStatus(Bird::FLYING);

		m_accel += (bc.attractionPoint - m_pos) * m_fAttractionFactor;
		if (m_fAttractionFactor < 300.f * factorAttractToOrigin) m_fAttractionFactor += 3.f*dt;
	}

	// Avoid collision with Terrain and Static objects.
	float fCollisionAvoidanceWeight = 1.0f;

	m_alignHorizontally = 0;

	if(m_status == Bird::TAKEOFF)
	{ 
		if(m_accel.z < 0)
			m_accel.z = -m_accel.z;

		m_accel.z *= bc.factorTakeOff;
	}

	if (bc.avoidObstacles)
	{
		// Avoid obstacles & terrain.
		float fCollDistance = m_collisionInfo.CheckDistance() ;

		if(m_collisionInfo.IsColliding())
		{
				float w = (1.0f - m_collisionInfo.Distance()/fCollDistance);
				m_accel += m_collisionInfo.Normal() * w*w * bc.factorAvoidLand * fCollisionAvoidanceWeight;
		}
	}

	// Limits birds to above water and land.
	if (m_pos.z < bc.terrainZ-0.2f)
	{
		m_pos.z = bc.terrainZ-0.2f;
	}
	if (m_pos.z < bc.waterLevel-0.2f)
	{
		m_pos.z = bc.waterLevel-0.2f;
	}
}

//////////////////////////////////////////////////////////////////////////
void CBoidBird::ThinkWalk( float dt,SBoidContext &bc )
{

	m_accel = -m_heading*(m_speed-m_walkSpeed)*0.4f;


	if (bc.factorAlignment != 0)
	{
		Vec3 alignmentAccel;
		Vec3 cohesionAccel;
		Vec3 separationAccel;
		CalcFlockBehavior(bc,alignmentAccel,cohesionAccel,separationAccel);

		m_accel += alignmentAccel*bc.factorAlignmentGround;
		m_accel += cohesionAccel*bc.factorCohesionGround;
		m_accel += separationAccel*bc.factorSeparationGround;
	}


	// Attract to origin point.
	if (bc.followPlayer)
	{
		m_accel += (bc.playerPos - m_pos) * bc.factorAttractToOriginGround;
	}
	else
	{
		if (cry_random(0, 31) == 1)
		{
			m_birdOriginPos = Vec3(	bc.flockPos.x+Boid::Frand()*bc.fSpawnRadius,bc.flockPos.y+Boid::Frand()*bc.fSpawnRadius,bc.flockPos.z+Boid::Frand()*bc.fSpawnRadius );
			if (m_birdOriginPos.z - bc.terrainZ < bc.MinHeight)
			{
				m_birdOriginPos.z = bc.terrainZ + bc.MinHeight;
			}
		}

		m_accel += (m_birdOriginPos - m_pos) * bc.factorAttractToOriginGround;
	}


	// Avoid collision with Terrain and Static objects.
	float fCollisionAvoidanceWeight = 10.0f;

	// Do walk sounds.
// 	if ((cry_rand()&0xFF) == 0)
// 		PlaySound(CHICKEN_SOUND_CLUCK);


	
	m_accel.z = 0;

	//////////////////////////////////////////////////////////////////////////
	// Avoid water ocean..
	if (m_pos.z < bc.waterLevel && bc.bAvoidWater)
	{
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
void CBoidBird::UpdateAnimationSpeed(SBoidContext &bc)
{
	if(!m_object)
		return;

	float animSpeed;

	switch(m_status)
	{ 
	case Bird::TAKEOFF:
		animSpeed = m_playingTakeOffAnim ? 1.f : max(2.f, bc.MaxAnimationSpeed);
		break;

	case Bird::LANDING:
		{
			const float maxLandSpeed = 2.f;
			float dist2 = Distance::Point_PointSq(m_pos,m_landingPoint);
			if(dist2 < 1.f)
				animSpeed = maxLandSpeed;
			else if (dist2 < 9.f)
				animSpeed = 1.f+ ( 9.f - dist2  )/(9.f-1.f) *(maxLandSpeed - 1.f);
			else 
				animSpeed = 1.f;
		}
		break;

	case Bird::ON_GROUND:
		{
			if(bc.walkSpeed > 0 && m_onGroundStatus != Bird::OGS_IDLE)
				animSpeed = m_speed/bc.walkSpeed;
			else
				animSpeed = 1.0f;
		}
		break;

	default:
		{
			if(bc.MaxSpeed == bc.MinSpeed)
				animSpeed = bc.MaxAnimationSpeed;
			else
				animSpeed = ((m_speed - bc.MinSpeed)/ (bc.MaxSpeed - bc.MinSpeed)*0.6f +0.4f) * bc.MaxAnimationSpeed;
		}
		break;
	}
	
	m_object->SetPlaybackScale( animSpeed );
}

//////////////////////////////////////////////////////////////////////////
void CBoidBird::Kill( const Vec3 &hitPoint,const Vec3 &force )
{
	if (m_dead)
		return;
	
	m_status = Bird::DEAD;

	m_flock->m_bAnyKilled = true;
	IEntity *pEntity = gEnv->pEntitySystem->GetEntity(m_entity);
	if (pEntity)
	{
		SpawnParams params;
		params.eAttachForm = GeomForm_Surface;
		params.eAttachType = GeomType_Render;
		gEnv->pEntitySystem->GetBreakableManager()->AttachSurfaceEffect( pEntity,0,SURFACE_BREAKAGE_TYPE("destroy"),params );
	}

	if (CFlock::m_e_flocks_hunt == 0)
	{
		m_physicsControlled = false;
		m_dead = true;
		m_nodraw = true;

		if (pEntity)
		{
			pEntity->Hide(true);
		}

		if (!m_dead && m_pPhysics)
		{
			if (pEntity)
			{
				pEntity->UnphysicalizeSlot(0);
			}
			m_pPhysics = 0;
		}

		// No physics at all.
		return;
	}

	SBoidContext bc;
	m_flock->GetBoidSettings(bc);

	Vec3 impulse = force;
	if (impulse.GetLength() > 100.0f)
	{
		impulse.Normalize();
		impulse *= 100.0f;
	}

	//if (!m_physicsControlled)
	{
		if (!m_object)
			return;

		AABB aabb =	m_object->GetAABB( );
		Vec3 size = ((aabb.max - aabb.min)/2.2f)*m_scale;

		CreateArticulatedCharacter( bc, size, bc.fBoidMass );

		m_physicsControlled = true;

		// Small impulse.
		// Apply force on this body.
		pe_action_impulse theAction;
		Vec3 someforce;
		someforce = impulse;
		//someforce.Normalize();
		theAction.impulse = someforce;
		theAction.ipart = 0;
		theAction.iApplyTime = 0;
		if (m_pPhysics)
			m_pPhysics->Action(&theAction);
	}

	if (m_object && !m_dying && !m_dead)
	{
		m_object->GetISkeletonAnim()->StopAnimationsAllLayers();
	}

	m_dead = true;
}

/////////////////////////////////////////////////////////////

bool CBoidBird::ShouldUpdateCollisionInfo(const CTimeValue& t)
{
	if(m_status == Bird::LANDING && m_landDecelerating  )
		return false;
	return CBoidObject::ShouldUpdateCollisionInfo(t);
}

/////////////////////////////////////////////////////////////

void CBoidBird::LoadParticleEffects()
{
	if (!m_loadedParticleEffects)
	{
		m_pDestroyParticleEffect = gEnv->pParticleManager->FindEffect("breakable_objects.Shatter.destroy", string("Surface Effect ") + "mat_flesh");
		if (m_pDestroyParticleEffect)
		{
			m_pDestroyParticleEffect->AddRef();
		}
		else
		{
			CryLog("###### CBoidBird DestroyParticleEffect not found");
		}

		m_loadedParticleEffects = true;
	}
}

/////////////////////////////////////////////////////////////

void CBoidBird::ReleaseParticleEffects()
{
	SAFE_RELEASE(m_pDestroyParticleEffect);
}

/////////////////////////////////////////////////////////////

/*
void CBoidBird::OnGrab( )
{
	//CBoidObject::OnGrab();

	m_status = Bird::PICKED_UP;
	PlayAnimationId(Bird::ANIM_GRABBED,true);
}

/////////////////////////////////////////////////////////////

void CBoidBird::OnThrow()
{
	Kill(ZERO,ZERO);
}*/

