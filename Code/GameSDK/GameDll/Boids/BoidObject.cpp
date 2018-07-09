// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

// -------------------------------------------------------------------------
//  File name:   BoidObject.cpp
//  Version:     v1.00
//  Created:     8/2010 by Luciano Morpurgo (refactored from flock.cpp)
//  Compilers:   Visual C++ 7.0
//  Description: 
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////

#include "StdAfx.h"
#include "BoidObject.h"

#include <CryEntitySystem/IEntitySystem.h>
#include "BoidsProxy.h"

#include <float.h>
#include <limits.h>
#include <CrySystem/ITimer.h>
#include <CryScriptSystem/IScriptSystem.h>
#include <CryAnimation/ICryAnimation.h>
#include <CryMath/Cry_Camera.h>
#include <CryString/CryPath.h>
#include "Flock.h"
#include <CryEntitySystem/IBreakableManager.h>

#define BIRDS_PHYSICS_DENSITY 200
#define BIRDS_PHYSICS_INWATER_DENSITY 900
#define FISH_PHYSICS_DENSITY 850

#define  PHYS_FOREIGN_ID_BOID PHYS_FOREIGN_ID_USER-1

#define MAX_SPEED 15
#define MIN_SPEED 2.5f


#define SCARE_DISTANCE 10

#define ALIGNMENT_FACTOR 1.0f
#define COHESION_FACTOR 1.0f
#define SEPARATION_FACTOR 10.0f

#define ORIGIN_ATTRACT_FACTOR 0.1f
#define DESIRED_HEIGHT_FACTOR 0.4f
#define AVOID_LAND_FACTOR 10.0f

#define MAX_ANIMATION_SPEED 1.7f


//////////////////////////////////////////////////////////////////////////
CBoidObject::CBoidObject( SBoidContext &bc )
{
	m_flock = 0;
	m_entity = 0;

	m_heading.Set(1,0,0);
	m_accel.Set(0,0,0);
	m_currentAccel.Set(0,0,0);
	m_speed = 0;
	m_object = 0;
	m_banking = 0;
	m_bankingTrg = 0;
	m_alignHorizontally = 0;
	m_scale = 1.0f;
	// flags
	m_dead = false;
	m_dying = false;
	m_physicsControlled = false;
	m_inwater = false;
	m_nodraw = false;
	m_pPhysics = 0;
	m_pickedUp = false;
	m_scareRatio = 0;
	m_displayChr = 1;
	m_invulnerable = bc.bInvulnerable;
	m_collisionInfo.Reset();

	m_speed = bc.MinSpeed + (Boid::Frand()+1)/2.0f*(bc.MaxSpeed - bc.MinSpeed);
	m_heading = Vec3(Boid::Frand(),Boid::Frand(),0).GetNormalized();

}

//////////////////////////////////////////////////////////////////////////
CBoidObject::~CBoidObject()
{
	m_pPhysics = 0;
	m_collisionInfo.Reset();
}

//////////////////////////////////////////////////////////////////////////
bool CBoidObject::PlayAnimation( const char *animation, bool bLooped, float fBlendTime )
{
	bool playing = false;

	if (m_object)
	{
		ISkeletonAnim* pSkeleton = m_object->GetISkeletonAnim();
		assert(pSkeleton);

		CryCharAnimationParams animParams;
		if (bLooped)
			animParams.m_nFlags |= CA_LOOP_ANIMATION;
		animParams.m_fTransTime = fBlendTime;

		const int amountAnimationsInFIFO = pSkeleton->GetNumAnimsInFIFO(0);
		const uint32 maxAnimationsAllowedInQueue = 2;
		if(amountAnimationsInFIFO >= maxAnimationsAllowedInQueue)
		{
			animParams.m_nFlags |= CA_REMOVE_FROM_FIFO;
		}

		playing = pSkeleton->StartAnimation( animation, animParams );
		assert(pSkeleton->GetNumAnimsInFIFO(0) <= maxAnimationsAllowedInQueue);
		m_object->SetPlaybackScale( 1.0f ); 
	}
	return playing;
}

//////////////////////////////////////////////////////////////////////////
bool CBoidObject::PlayAnimationId( int nIndex,bool bLooped,float fBlendTime )
{
	if (nIndex >= 0 && nIndex < (int)m_flock->m_bc.animations.size())
		return PlayAnimation( m_flock->m_bc.animations[nIndex],bLooped,fBlendTime );
	return false;
}

//////////////////////////////////////////////////////////////////////////
void CBoidObject::ExecuteTrigger(int nIndex)
{
	if ((nIndex >= 0) && (nIndex < (int)m_flock->m_bc.audio.size()))
	{
		const CryAudio::ControlId id = m_flock->m_bc.audio[nIndex];
		if (id != CryAudio::InvalidControlId)
		{
			IEntity* pEntity = gEnv->pEntitySystem->GetEntity(m_entity);
			if (pEntity)
			{
				IEntityAudioComponent* audioProxy = pEntity->GetOrCreateComponent<IEntityAudioComponent>();

				if (audioProxy)
				{
					audioProxy->ExecuteTrigger(id);
				}
			}
		}
	}
}

//////////////////////////////////////////////////////////////////////////
int CBoidObject::GetGeometrySurfaceType()
{
	if (m_object)
	{
		ISurfaceType *pSurfaceType = gEnv->pEntitySystem->GetBreakableManager()->GetFirstSurfaceType(m_object);
		if (pSurfaceType)
			return pSurfaceType->GetId();
	}
	else
	{
		IEntity *pEntity = gEnv->pEntitySystem->GetEntity(m_entity);
		if (pEntity)
		{
			IStatObj *pStatObj = pEntity->GetStatObj(0|ENTITY_SLOT_ACTUAL);
			if (pStatObj)
			{
				ISurfaceType *pSurfaceType = gEnv->pEntitySystem->GetBreakableManager()->GetFirstSurfaceType(pStatObj);
				if (pSurfaceType)
					return pSurfaceType->GetId();
			}
		}
	}
	return 0;
}

//////////////////////////////////////////////////////////////////////////
void CBoidObject::OnEntityEvent( const SEntityEvent &event )
{
	switch (event.event)
	{
	case ENTITY_EVENT_DONE:
		m_entity = 0;
		m_object = 0;
		m_pPhysics = 0;
		break;
	case ENTITY_EVENT_COLLISION:
		OnCollision(event);
		break;

	}
}

//////////////////////////////////////////////////////////////////////////
void CBoidObject::OnCollision( const SEntityEvent &event )
{
	EventPhysCollision *pCollision = (EventPhysCollision *)(event.nParam[0]);

	IPhysicalEntity *pPhysObject = 0;
	if (pCollision->pEntity[0] == m_pPhysics)
	{
		pPhysObject = pCollision->pEntity[1];
	}
	else if (pCollision->pEntity[1] == m_pPhysics)
	{
		pPhysObject = pCollision->pEntity[0];
	}

	// Get speed.
	if (pPhysObject)
	{
		pe_status_dynamics dynamics;
		pPhysObject->GetStatus(&dynamics);
		if (dynamics.v.GetLengthFast() > 1.0f && !m_invulnerable)
		{
			m_physicsControlled = false;
			Kill( m_pos,Vec3(0,0,0) );
		}
	}

}

//////////////////////////////////////////////////////////////////////////
void CBoidObject::CalcOrientation( Quat &qOrient )
{
	if (m_physicsControlled && m_pPhysics)
	{
		pe_status_pos ppos;
		m_pPhysics->GetStatus(&ppos);
		qOrient = ppos.q;
		return;
	}

	if (m_heading.IsZero())
		m_heading = Vec3Constants<float>::fVec3_OneX;

	Vec3 dir = m_heading;
	if (m_alignHorizontally != 0)
	{
		dir.z *= (1.0f-m_alignHorizontally);
	}
	dir.NormalizeSafe(Vec3Constants<float>::fVec3_OneX);
	if (m_banking != 0)
		qOrient.SetRotationVDir( dir,-m_banking*0.5f );
	else
		qOrient.SetRotationVDir( dir );
}

//////////////////////////////////////////////////////////////////////////
void CBoidObject::Render( SRendParams &rp,CCamera &cam,SBoidContext &bc )
{

}

//////////////////////////////////////////////////////////////////////////
inline float AngleBetweenVectors( const Vec3 &v1,const Vec3 &v2 )
{
	float a = acos_tpl(v1.Dot(v2));
	Vec3 r = v1.Cross(v2);
	if (r.z < 0)
		a = -a;
	return a;
}

//////////////////////////////////////////////////////////////////////////

void CBoidObject::ClampSpeed(SBoidContext& bc, float dt)
{
	if (m_speed > bc.MaxSpeed)
		m_speed = bc.MaxSpeed;
	if (m_speed < bc.MinSpeed)
		m_speed = bc.MinSpeed;
}

//////////////////////////////////////////////////////////////////////////
void CBoidObject::CalcMovement( float dt,SBoidContext &bc,bool banking )
{
	// Calc movement with current velocity.
	Vec3 prevAccel(0,0,0);

	if (banking)
	{
		if (m_currentAccel.x != 0 && m_currentAccel.y != 0 && m_currentAccel.z != 0)
			prevAccel = m_currentAccel.GetNormalized();
		else
			banking = false;
	}

	m_currentAccel = m_currentAccel*bc.fSmoothFactor + m_accel*(1.0f - bc.fSmoothFactor);
	
	Vec3 velocity = m_heading*m_speed;
	m_pos = m_pos + velocity*dt;
	velocity = velocity + m_currentAccel*dt;
	m_speed = velocity.GetLength();

//	ClampSpeed(bc,dt);

	if (fabs(m_speed) > 0.0001f)
	{
		Vec3 newHeading = velocity;// * (1.0f/m_speed); // Normalized velocity vector is our heading.
		newHeading.NormalizeFast();
		if (bc.fMaxTurnRatio)
		{
			float fHeadingSmothFactor = bc.fMaxTurnRatio * bc.fSmoothFactor;
			if (fHeadingSmothFactor > 1.0f)
				fHeadingSmothFactor = 1.0f;
			m_heading = newHeading*fHeadingSmothFactor + m_heading*(1.0f - fHeadingSmothFactor);
		}
		else
		{
			m_heading = newHeading;
		}
	}

/*
	if (m_speed > bc.MaxSpeed)
		m_speed = bc.MaxSpeed;
	if (m_speed < bc.MinSpeed)
		m_speed = bc.MinSpeed;
*/
	ClampSpeed(bc,dt);

	if (banking)
	{
		Vec3 sideDir = m_heading.Cross(Vec3(0,0,1));
		if (sideDir.IsZero())
			sideDir = m_heading.Cross(Vec3(0,1,0));
//		Vec3 v = m_currentAccel.GetLength();
		m_bankingTrg = prevAccel.Dot(sideDir.GetNormalized());
	}
	else
		m_bankingTrg = 0;

	// Slowly go into the target banking.
	float bd = m_bankingTrg - m_banking;
	m_banking = m_banking + bd*dt*10.0f;



	/*
	if (m_pPhysics)
	{
		//pe_params_pos ppos;
		//ppos.pos = Vec3(m_pos);
		//m_pPhysics->SetParams(&ppos);

		//pe_action_set_velocity asv;
		//asv.v = m_heading*m_speed;
		//m_pPhysics->Action(&asv);
	}
	*/
}

//////////////////////////////////////////////////////////////////////////

void CBoidObject::UpdateAnimationSpeed(SBoidContext &bc)
{
	if (m_object && fabs(m_speed) > 0.0001f)
	{
		//float animSpeed = ((m_speed - bc.MinSpeed)/ (bc.MaxSpeed - bc.MinSpeed + 0.1f )) * bc.MaxAnimationSpeed;
		float animSpeed = m_speed * bc.MaxAnimationSpeed;
		m_object->SetPlaybackScale( animSpeed );
	}
}

//////////////////////////////////////////////////////////////////////////
void CBoidObject::CalcFlockBehavior( SBoidContext &bc,Vec3 &vAlignment,Vec3 &vCohesion,Vec3 &vSeparation )
{
	// Vector of sight between boids.
	Vec3 sight;

	float MaxAttractDistance2 = bc.MaxAttractDistance*bc.MaxAttractDistance;
	float MinAttractDistance2 = bc.MinAttractDistance * bc.MinAttractDistance;

	vSeparation.zero();
	vAlignment.zero();
	vCohesion.zero();

	// Average alignment and speed.
	Vec3 avgAlignment(0,0,0);
	Vec3 avgNeighborsCenter(0,0,0);
	int numMates = 0;

	int numBoids = m_flock->GetBoidsCount();
	for (int i = 0; i < numBoids; i++)
	{
		CBoidObject *boid = m_flock->GetBoid(i);
		if (!boid || boid == this) // skip myself.
			continue;

		sight = boid->m_pos - m_pos;

		float dist2 = Boid::Normalize_fast(sight);

		// Check if this boid is in our range of sight.
		// And If this neighbor is in our field of view.
		if (dist2 < MaxAttractDistance2 && m_heading.Dot(sight) > bc.cosFovAngle)
		{
			// Separation from other boids.
			if (dist2 < MinAttractDistance2)
			{
				// Boid too close, distract from him.
				float w = (1.0f - dist2/MinAttractDistance2);
				vSeparation -= sight*(w)*bc.factorSeparation;
			}

			numMates++;

			// Alignment with boid direction.
			avgAlignment += boid->m_heading * boid->m_speed;

			// Calculate average center of all neighbor boids.
			avgNeighborsCenter += boid->m_pos;
		}
	}
	if (numMates > 0)
	{
		avgAlignment = avgAlignment * (1.0f/numMates);
		//float avgSpeed = avgAlignment.GetLength();
		vAlignment = avgAlignment;

		// Attraction to mates.
		avgNeighborsCenter = avgNeighborsCenter * (1.0f/numMates);
		Vec3 cohesionDir = avgNeighborsCenter - m_pos;

		float sqrDist = cohesionDir.IsZeroFast() ? 0: Boid::Normalize_fast(cohesionDir);
		float w = MaxAttractDistance2 != MinAttractDistance2 ? 
			(sqrDist - MinAttractDistance2)/(MaxAttractDistance2 - MinAttractDistance2) : 0;
		vCohesion = cohesionDir*w;
	}
}

//////////////////////////////////////////////////////////////////////////
void CBoidObject::Physicalize( SBoidContext &bc )
{
	pe_params_particle ppart;
	//ppart.gravity = Vec3(0,0,0);
	ppart.flags = particle_traceable | particle_no_roll | pef_never_affect_triggers | pef_log_collisions;
	ppart.mass = 0;
	ppart.size = max(bc.fBoidRadius,0.01f);
	ppart.thickness = max(bc.fBoidThickness,0.01f);
	ppart.gravity = Vec3(0,0,0);
	ppart.kAirResistance = 0.0f;

	if(m_object)
	{
		IMaterial* pMat = m_object->GetIMaterial();
		if(pMat->GetSubMtlCount() > 0)
		{
			ppart.surface_idx = pMat->GetSubMtl(0)->GetSurfaceTypeId();
		}
		else
		{
			ppart.surface_idx = pMat->GetSurfaceTypeId();
		}
	}
	else
	{
		// just like in GetGeometrySurfaceType()
		ppart.surface_idx = 0;
		IEntity *pEntity = gEnv->pEntitySystem->GetEntity(m_entity);
		if (pEntity)
		{
			IStatObj *pStatObj = pEntity->GetStatObj(0|ENTITY_SLOT_ACTUAL);
			if (pStatObj)
			{
				ISurfaceType *pSurfaceType = gEnv->pEntitySystem->GetBreakableManager()->GetFirstSurfaceType(pStatObj);
				if (pSurfaceType)
					ppart.surface_idx = pSurfaceType->GetId();
			}
		}
	}

	if (IEntity* pEntity = gEnv->pEntitySystem->GetEntity(m_entity))
	{
		SEntityPhysicalizeParams params;
		params.pParticle = &ppart;
		params.type = PE_PARTICLE;
		pEntity->Physicalize(params);
		m_pPhysics = pEntity->GetPhysics();
	}
}

//////////////////////////////////////////////////////////////////////////
void CBoidObject::CreateRigidBox( SBoidContext &bc,const Vec3 &boxSize,float mass,float density )
{
	if (m_pPhysics)
	{
		m_pPhysics = 0;
	}

	Vec3 orgVelocity = m_speed*m_heading;
	if (m_pPhysics && m_pPhysics->GetType() == PE_PARTICLE)
	{
		pe_params_particle pparams;
		m_pPhysics->GetParams(&pparams);
		orgVelocity = pparams.velocity*pparams.heading;
	}

	Quat q;
	CalcOrientation( q);

	pe_params_pos bodypos;
	bodypos.pos = m_pos;
	bodypos.q = q;
	bodypos.scale = m_scale;

	IEntity *pEntity = gEnv->pEntitySystem->GetEntity(m_entity);
	if (!pEntity)
		return;

	SEntityPhysicalizeParams entityPhysParams;
	entityPhysParams.type = PE_RIGID;
	pEntity->Physicalize( entityPhysParams );
	m_pPhysics =  pEntity->GetPhysics();
	if (!m_pPhysics)
		return;
	m_pPhysics->SetParams(&bodypos);

	pe_params_flags pf;
	pf.flagsOR = pef_never_affect_triggers|pef_never_break;
	m_pPhysics->SetParams(&pf);

	primitives::box geomBox;
	geomBox.Basis.SetIdentity();
	geomBox.center.Set(0,0,0);
	geomBox.size = boxSize;
	geomBox.bOriented = 0;
	IGeometry *pGeom = bc.physics->GetGeomManager()->CreatePrimitive( primitives::box::type,&geomBox );
	phys_geometry *physGeom = bc.physics->GetGeomManager()->RegisterGeometry( pGeom );
	pGeom->Release();

	pe_geomparams partpos;
	partpos.pos.Set(0,0,0);
	if (mass > 0)
		partpos.mass = mass; // some fish mass.
	if (density > 0)
		partpos.density = density;
	partpos.surface_idx = GetGeometrySurfaceType();

	m_pPhysics->AddGeometry( physGeom,&partpos,0 );
	bc.physics->GetGeomManager()->UnregisterGeometry( physGeom );

	pe_simulation_params symparams;
	symparams.damping = 0.3f;
	symparams.dampingFreefall = 0.2f;
	m_pPhysics->SetParams(&symparams);

	pe_params_buoyancy pb;
	pb.waterDensity = 1000.0f;
	pb.waterDamping = 1;
	pb.waterResistance = 1000;
	pb.waterPlane.n.Set(0,0,1);
	//pb.waterPlane.origin.set(0,0,gEnv->p3DEngine->GetWaterLevel(&m_center));
	pb.waterPlane.origin.Set( 0,0,bc.waterLevel );
	m_pPhysics->SetParams(&pb);

	// Set original velocity on physics.
	pe_action_set_velocity psetvel;
	psetvel.v = orgVelocity;
	m_pPhysics->Action( &psetvel );
}

//////////////////////////////////////////////////////////////////////////
void CBoidObject::CreateArticulatedCharacter( SBoidContext &bc,const Vec3 &size,float mass )
{
	Vec3 orgVelocity = m_speed*m_heading;
	if (m_pPhysics && m_pPhysics->GetType() == PE_PARTICLE)
	{
		pe_params_particle pparams;
		m_pPhysics->GetParams(&pparams);
		orgVelocity = pparams.velocity*pparams.heading;
	}

	if (m_pPhysics)
	{
		m_pPhysics = 0;
	}

	IEntity *pEntity = gEnv->pEntitySystem->GetEntity(m_entity);
	if (!pEntity)
		return;

	Quat q;
	CalcOrientation(q);

	pe_params_pos bodypos;
	bodypos.pos = m_pos;
	bodypos.q = q;
	bodypos.scale = m_scale;
	bodypos.iSimClass = 2;

	SEntityPhysicalizeParams entityPhysParams;
	entityPhysParams.type = PE_ARTICULATED;
	entityPhysParams.nSlot = 0;
	entityPhysParams.mass = mass;
	entityPhysParams.nLod = 1;
	pEntity->Physicalize( entityPhysParams );

	// After physicalization reset entity slot matrix if present.
	if (!bc.vEntitySlotOffset.IsZero())
	{
		Matrix34 tmIdent;
		tmIdent.SetIdentity();
		pEntity->SetSlotLocalTM(0,tmIdent);
	}

	m_pPhysics =  pEntity->GetPhysics();
	if (!m_pPhysics)
		return;
	m_pPhysics->SetParams(&bodypos);

	//m_pPhysics =  m_object->RelinquishCharacterPhysics();

	pe_params_flags pf;
	pf.flagsOR = pef_never_affect_triggers|pef_never_break;
	m_pPhysics->SetParams(&pf);
	
	pe_params_articulated_body pab;
	pab.bGrounded = 0;
	pab.bCheckCollisions = 1;
	pab.bCollisionResp = 1;
	m_pPhysics->SetParams(&pab);

	pe_simulation_params symparams;
	symparams.damping = 0.3f;
	symparams.dampingFreefall = 0.2f;
	m_pPhysics->SetParams(&symparams);

	pe_params_buoyancy pb;
	pb.waterDensity = 1000.0f;
	pb.waterDamping = 1;
	pb.waterResistance = 1000;
	pb.waterPlane.n.Set(0,0,1);
	//pb.waterPlane.origin.set(0,0,gEnv->p3DEngine->GetWaterLevel(&m_center));
	pb.waterPlane.origin.Set( 0,0,bc.waterLevel );
	m_pPhysics->SetParams(&pb);

	// Set original velocity on ragdoll.
	pe_action_set_velocity psetvel;
	psetvel.v = orgVelocity;
	m_pPhysics->Action( &psetvel );

	pe_params_part pp;
	pp.flagsAND = ~(geom_colltype_player|geom_colltype_ray|geom_colltype_foliage_proxy|geom_colltype_foliage);

	if (bc.bPickableWhenDead)
	{
		// keep geom_colltype_ray for player interactor
		pp.flagsAND = geom_colltype_ray;
	}

	m_pPhysics->SetParams(&pp);
}

////////////////////////////////////////////////////////////////////
void CBoidObject::UpdateCollisionInfo()
{
	Vec3 vPos = m_pos + m_heading*0.5f;
	Vec3 vDir = m_heading*GetCollisionDistance();
	
	m_collisionInfo.QueueRaycast(m_entity,vPos,vDir);

	m_collisionInfo.UpdateTime();
}

/////////////////////////////////////////////////////
float CBoidObject::GetCollisionDistance()
{
	return 10.f;
}


void CBoidObject::DisplayCharacter(bool bEnable) // passed as parameter - avoid redundancy in class member 
{
	IEntity* pMyEntity = gEnv->pEntitySystem->GetEntity(m_entity);
	if(!pMyEntity)
		return;

	if(bEnable)
	{
		m_displayChr = 1;
		if(pMyEntity->IsSlotValid(eSlot_Chr))
			pMyEntity->SetSlotFlags(eSlot_Chr,pMyEntity->GetSlotFlags(eSlot_Chr) | ENTITY_SLOT_RENDER);

		if(pMyEntity->IsSlotValid(eSlot_Cgf))
			pMyEntity->SetSlotFlags(eSlot_Cgf,pMyEntity->GetSlotFlags(eSlot_Cgf) & ~ENTITY_SLOT_RENDER);
	}
	else
	{
		m_displayChr = 0;
		if(pMyEntity->IsSlotValid(eSlot_Chr))
			pMyEntity->SetSlotFlags(eSlot_Chr,pMyEntity->GetSlotFlags(eSlot_Chr) & ~ENTITY_SLOT_RENDER);

		if(pMyEntity->IsSlotValid(eSlot_Cgf))
			pMyEntity->SetSlotFlags(eSlot_Cgf,pMyEntity->GetSlotFlags(eSlot_Cgf) | ENTITY_SLOT_RENDER);
	}
}
/////////////////////////////////////////////////////////////

void CBoidObject::UpdateDisplay(SBoidContext& bc)
{
	if(bc.animationMaxDistanceSq ==0)
		return;

	Vec3 cameraPos(GetISystem()->GetViewCamera().GetPosition());
	Vec3 cameraDir(GetISystem()->GetViewCamera().GetMatrix().GetColumn1());

	float  dot = (m_pos - cameraPos).Dot(cameraDir);

	float distSq = Distance::Point_PointSq(cameraPos,m_pos );
	if(m_displayChr)
	{
		if(dot < 0 || distSq > bc.animationMaxDistanceSq )
		{
			DisplayCharacter(false);
		}
	}
	else
	{
		if(dot > 0 && distSq <= bc.animationMaxDistanceSq )
		{	
			// show animated character
			DisplayCharacter(true);
		}
	}

}

bool CBoidObject::ShouldUpdateCollisionInfo(const CTimeValue &t)
{
	return !IsDead();// && (t -GetLastCollisionCheckTime()).GetSeconds() > 0.2f + Boid::Frand()*0.1f;
}

