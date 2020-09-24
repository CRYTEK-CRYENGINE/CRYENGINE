#include "StdAfx.h"

#include "../Geometry/AnimatedMeshComponent.h"
#include "../Geometry/AdvancedAnimationComponent.h"
#include "SampleRigidbodyActorComponent.h"
#include <CryPhysics/IPhysics.h>

namespace Cry
{
namespace DefaultComponents
{

void CSampleActorComponent::Register(Schematyc::CEnvRegistrationScope& componentScope)
{
	// Functions
	{
		auto pFunction = SCHEMATYC_MAKE_ENV_FUNCTION(&CSampleActorComponent::SetMoveVel, "{5DA08668-046F-4ced-B2A4-FC53E6F33F77}"_cry_guid, "SetMoveVel");
		pFunction->SetDescription("Sets the desired movement velocity");
		pFunction->BindInput(1, 'vel', "Velocity", nullptr, Vec3(ZERO));
		componentScope.Register(pFunction);
	}
	{
		auto pFunction = SCHEMATYC_MAKE_ENV_FUNCTION(&CSampleActorComponent::GetMoveVel, "{D7716491-E3E0-4257-9810-FA56E0332363}"_cry_guid, "GetMoveVel");
		pFunction->SetDescription("Get the desired movement velocity");
		pFunction->BindOutput(0, 'vel', "velocity");
		componentScope.Register(pFunction);
	}
	{
		auto pFunction = SCHEMATYC_MAKE_ENV_FUNCTION(&CSampleActorComponent::Jump, "{D2CF8D00-19FD-4e89-B792-85010B1A3530}"_cry_guid, "Jump");
		pFunction->SetDescription("Requests jump to the specified height");
		pFunction->BindInput(1, 'h', "Height", nullptr, 1.0f);
		componentScope.Register(pFunction);
	}
}

CSampleActorComponent::~CSampleActorComponent()
{
	SEntityPhysicalizeParams pp;
	pp.type = PE_NONE;
	m_pEntity->Physicalize(pp);
}

int CSampleActorComponent::OnPostStep(float deltaTime)
{
	// immediate post step; is called directly from the physics thread
	IPhysicalEntity *pEntity = m_pEntity->GetPhysicalEntity();
	if (deltaTime <= 0 || !pEntity)
		return 1;
	pe_params_walking_rigid pwr;
	pe_action_set_velocity asv;
	pe_simulation_params sp;
	pe_status_living sl;
	pe_status_dynamics sd;
	pe_status_sensors ss;
	Vec3 pt; ss.pPoints = &pt;
	// ground contact rays are extended by stickVel*dt, so if horizontal velocity is v, stickVel of v*tan(angle) will make the entity stick to slopes up to 'angle'
	float stickVel = Vec2(m_velMove).GetLength(); // * tan(gf_PI*0.25f), which is 1; will stick to slopes up to pi/4
	pEntity->GetStatus(&sl);
	pEntity->GetStatus(&sd);
	pEntity->GetStatus(&ss);
	if (sl.pGroundCollider)	
	{
		Vec3 velReq = m_velMove - sl.groundSlope*(sl.groundSlope*m_velMove); // project velMove to the ground
		sl.vel -= sl.velGround;	 // if we are standing on a moving object, convert sl.vel to "local space"
		Vec3 dv = (velReq-sl.vel)*(deltaTime*10.0f);	// default inertial movement: exponentially approach velReq with strength 10
		m_timeFly = 0;
		pwr.velLegStick = stickVel;
		if (m_velJump.len2())
		{
			// if jump is requested, immediately set velocity to velJump + velMove
			dv = m_velMove + m_velJump - sl.vel;
			m_timeFly = 0.1f;
			pwr.velLegStick = -1e6f; // make sure we don't stick to the ground until m_timeFly expires
			SetupLegs(true);
		}
		asv.v = sl.vel + dv;
		if (m_velMove.len2()) 
		{
			// if we are accelerating, subtract the momentum we gain from the object we stand on
			// this ensures physical consistency, but can be disabled if it messes too much with the ground objects
			pe_action_impulse ai;
			ai.point = ss.pPoints[0];
			ai.impulse = dv*-sd.mass;
			sl.pGroundCollider->Action(&ai);
			pwr.legFriction = 0;
			sp.minEnergy = 0;	// never sleep when accelerating
		}	
		else 
		{
			// when not accelerating, also let the natural friction stop the movement
			pwr.legFriction = m_friction;
			sp.minEnergy = sqr(0.07f);
		}
		if (!m_velJump.len2())
			asv.v -= sl.groundSlope*(sl.groundSlope*asv.v); // project final velocity on the ground to account for sudden slope changes
		asv.v += sl.velGround; // return velocity to world space
		m_velJump.zero();
	}	
	else
		m_timeFly -= deltaTime;
	if (m_timeFly <= 0)
		pwr.velLegStick = stickVel;
	pEntity->SetParams(&sp,1);
	pEntity->SetParams(&pwr,1);
	pEntity->Action(&asv,1);
	return 1;
}

void CSampleActorComponent::Physicalize()
{
	SEntityPhysicalizeParams epp;
	epp.type = PE_NONE;
	m_pEntity->Physicalize(epp);

	CBaseMeshComponent *pSkelMesh = nullptr;
	if (IEntityComponent *pChar = GetEntity()->GetComponent<CAdvancedAnimationComponent>())
		pSkelMesh = static_cast<CAdvancedAnimationComponent*>(pChar);
	if (IEntityComponent *pChar = GetEntity()->GetComponent<CAnimatedMeshComponent>())
		pSkelMesh = static_cast<CAnimatedMeshComponent*>(pChar);
	if (pSkelMesh)
	{
		pSkelMesh->m_ragdollStiffness = m_skelStiffness;
		pSkelMesh->m_ragdollLod = 0;
	}

	IPhysicalEntity *pent = gEnv->pPhysicalWorld->CreatePhysicalEntity(PE_WALKINGRIGID);
	m_pEntity->AssignPhysicalEntity(pent);
	SEntityEvent event(ENTITY_EVENT_PHYSICAL_TYPE_CHANGED);
	m_pEntity->SendEvent(event);
	pe_params_flags pf;
	pf.flagsOR = pef_monitor_poststep;
	pent->SetParams(&pf);

	pe_params_walking_rigid pwr;
	pwr.legFriction = m_friction;
	pwr.minLegTestMass = m_minMass;
	pwr.legStiffness = m_legStiffness;
	pent->SetParams(&pwr);

	SetupLegs();
	m_velMove.zero();
	m_velJump.zero();
}

void CSampleActorComponent::SetupLegs(bool immediately)
{
	if (m_pEntity->GetPhysics())
	{
		pe_params_sensors ps;
		auto trans = GetTransform();
		Vec3 org = trans->GetTranslation();
		Vec3 dir = Vec3(0,0,-org.z);
		ps.nSensors = 1;
		ps.pOrigins = &org;
		ps.pDirections = &dir;
		m_pEntity->GetPhysics()->SetParams(&ps, immediately);
	}
}

void CSampleActorComponent::OnInput()
{
	if (m_pEntity->GetPhysics() && m_velMove.len2() + m_velJump.len2())
	{
		pe_action_awake aa;
		m_pEntity->GetPhysics()->Action(&aa);
	}
}

}
}