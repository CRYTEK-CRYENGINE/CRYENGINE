// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"

#include "geometries.h"
#include "entities.h"
#include "world.h"

using namespace primitives;
using namespace cpx;
using namespace Helper;


PhysXProjectile::PhysXProjectile(int id, pe_params *params)
{
	m_type = PE_PARTICLE;
	m_id = id;
	if (params) 
		SetParams(params); 
}

void PhysXProjectile::Enable(bool enable)
{
	if (m_actor)
		PhysXEnt::Enable(enable);
	if (!enable && !(m_mask & 0x18)) {
		m_vel.zero();
		g_pPhysWorld->UpdateProjectileState(this);
	}
}

int PhysXProjectile::SetParams(pe_params* _params, int bThreadSafe)
{
	if (_params->type==pe_params_pos::type_id) {
		pe_params_pos *params = (pe_params_pos*)_params;
		if (!is_unused(params->pos)) m_pos = params->pos;
		if (params->pMtx3x4) m_pos = params->pMtx3x4->GetTranslation();
		return 1;
	}

	if (_params->type==pe_params_particle::type_id) {
		pe_params_particle *params = (pe_params_particle*)_params;
		Vec3 heading = m_vel.normalized();
		float vel = heading*m_vel;
		if (!is_unused(params->heading)) heading = params->heading;
		if (!is_unused(params->velocity)) vel = params->velocity; 
		if (!is_unused(params->gravity)) m_gravity = params->gravity;
		if (!is_unused(params->mass)) m_mass = params->mass;
		if (!is_unused(params->size)) m_size = params->size;
		if (!is_unused(params->iPierceability)) m_pierceability = params->iPierceability;
		if (!is_unused(params->surface_idx)) m_surfaceIdx = params->surface_idx;
		if (!is_unused(params->minBounceVel)) m_minBounceVel = params->minBounceVel;
		if (!is_unused(params->pColliderToIgnore)) m_pIgnoredEnt = (PhysXEnt*)params->pColliderToIgnore;
		if (!is_unused(params->flags)) m_flags = params->flags;
		if (!is_unused(params->heading) || !is_unused(params->velocity)) {
			m_vel = heading*vel;
			g_pPhysWorld->UpdateProjectileState(this);
		}
		return 1;
	}

	return PhysXEnt::SetParams(_params);
}

int PhysXProjectile::GetParams(pe_params* _params) const
{
	if (_params->type==pe_params_particle::type_id) {
		pe_params_particle *params = (pe_params_particle*)_params;
		memset(params,0,sizeof(pe_params_particle));
		params->type = pe_params_particle::type_id;
		Vec3 vel = m_actor ? V(m_actor->isRigidBody()->getLinearVelocity()) : m_vel;
		params->heading = vel.normalized();
		params->velocity = vel*params->heading;
		params->gravity = m_gravity;
		params->mass = m_mass;
		params->size = m_size;
		params->iPierceability = m_pierceability;
		params->surface_idx = m_surfaceIdx;
		params->minBounceVel = m_minBounceVel;
		params->pColliderToIgnore = m_pIgnoredEnt;
		params->flags = m_flags;
		return 1;
	}

	return PhysXEnt::GetParams(_params);
}

QuatT PhysXProjectile::getGlobalPose() const
{
	if (m_actor)
		return T(m_actor->getGlobalPose());
	QuatT pose(Quat(IDENTITY), m_pos);
	if (!(m_flags & particle_no_path_alignment)) {
		Vec3 heading = m_vel.normalized(), up = m_gravity.len2() ? -m_gravity:Vec3(0,0,-1), upNorm = (up-heading*(up*heading)).normalized();
		pose.q = Quat(Matrix33(heading^upNorm,heading,upNorm));
	} 
	return pose;
}

void PhysXProjectile::getBBox(Vec3 *BBox) const
{
	if (!m_actor) {
		BBox[0] = m_pos-Vec3(m_size); 
		BBox[1] = m_pos+Vec3(m_size);
	}	else
		PhysXEnt::getBBox(BBox);
}

int PhysXProjectile::GetStatus(pe_status* _status) const
{
	if (!m_actor) {
		if (_status->type==pe_status_pos::type_id) {
			pe_status_pos *status = (pe_status_pos*)_status;
			if (status->ipart>=0 || status->partid>=0)
				return 0;
			status->pos = m_pos;
			status->q = getGlobalPose().q;
			if (status->pMtx3x3)
				*status->pMtx3x3 = Matrix33(status->q);
			if (status->pMtx3x4)
				*status->pMtx3x4 = Matrix34(Vec3(1),status->q,status->pos);
			status->iSimClass = 4;
			return 1;
		}

		if (_status->type==pe_status_dynamics::type_id) {
			pe_status_dynamics *status = (pe_status_dynamics*)_status;
			memset(status, 0, sizeof(pe_status_dynamics));
			status->type = pe_status_dynamics::type_id;
			status->v = m_vel;
			status->centerOfMass = m_pos;
			return 1;
		}

		if (_status->type==pe_status_awake::type_id)
			return m_vel.len2()>0;

		return 0;
	}

	return PhysXEnt::GetStatus(_status);
}

int PhysXProjectile::Action(pe_action* _action, int bThreadSafe)
{
	if (_action->type==pe_action_set_velocity::type_id) {
		pe_action_set_velocity *action = (pe_action_set_velocity*)_action;
		if (!is_unused(action->v)) {
			m_vel = action->v;
			g_pPhysWorld->UpdateProjectileState(this);
		}
		return 1;
	}

	return m_actor ? PhysXEnt::Action(_action) : 0;
}

int PhysXProjectile::DoStep(float dt, int iCaller) 
{
	IPhysicalWorld::SRWIParams rp;
	IPhysicalEntity *pSkipEnt = m_pIgnoredEnt;
	ray_hit hits[8];
	rp.org = m_pos;
	rp.dir = (m_vel+m_gravity*(dt*0.5f))*dt; 
	rp.pSkipEnts = &pSkipEnt;
	rp.nSkipEnts = pSkipEnt!=nullptr;
	rp.flags = rwi_pierceability(m_pierceability);
	rp.hits = hits;
	rp.nMaxHits = 8;
	m_vel += m_gravity*dt;

	int solid = 0;
	if (int nhits = g_pPhysWorld->RayWorldIntersection(rp)) {
		solid = hits[0].dist>=0;
		if (!iCaller) for(int j=0; j<nhits; j++) {
			int i = j+1 & ~(nhits-1-solid-j>>31);	// report solid hit (slot 0) the last
			EventPhysCollision &epc = *(EventPhysCollision*)g_pPhysWorld->AllocEvent(EventPhysCollision::id);
			epc.pEntity[0]=this; epc.pForeignData[0]=m_pForeignData; epc.iForeignData[0]=m_iForeignData;
			epc.mass[0] = m_mass;
			epc.idmat[0] = m_surfaceIdx;
			epc.partid[0] = 0;
			epc.vloc[0] = m_vel;
			PhysXEnt *pColl = (PhysXEnt*)hits[i].pCollider;
			epc.pEntity[1]=pColl; epc.pForeignData[1]=pColl->m_pForeignData; epc.iForeignData[1]=pColl->m_iForeignData;
			epc.idCollider = pColl->m_id;
			epc.pt = hits[i].pt;
			epc.n = hits[i].n;
			epc.idmat[1] = hits[i].surface_idx;
			epc.partid[1] = hits[i].partid;
			pe_status_dynamics sd;
			pColl->GetStatus(&sd);
			epc.mass[1] = sd.mass;
			epc.vloc[1] = sd.v+(sd.w ^ epc.pt-sd.centerOfMass);
			float kmass = m_mass*(epc.mass[1]>0 ? epc.mass[1]/(m_mass+epc.mass[1]) : 1.0f);
			epc.penetration=epc.radius=epc.normImpulse = 0;
			epc.iPrim[0]=epc.iPrim[1] = 0;
			if ((epc.vloc[1]-epc.vloc[0])*epc.n > m_minBounceVel)
				if (!(m_flags & particle_no_impulse)) {
					pe_action_impulse ai;
					ai.partid = epc.partid[1];
					ai.point = epc.pt;
					ai.impulse = epc.n*(((m_vel-epc.vloc[1])*epc.n)*kmass);
					pColl->Action(&ai);
				}	else
					epc.normImpulse = kmass;
		}
	}

	if (solid)	{
		m_pos = hits[0].pt + hits[0].n*m_size;
		float bounciness[2],friction[2];
		uint flags[2];
		g_pPhysWorld->GetSurfaceParameters(m_surfaceIdx, bounciness[0],friction[0],flags[0]);
		g_pPhysWorld->GetSurfaceParameters(hits[0].surface_idx, bounciness[1],friction[1],flags[1]);
		float e = max(0.0f,min(1.0f, (bounciness[0]+bounciness[1])*0.5f));
		m_vel += hits[0].n*(e-m_vel*hits[0].n);
		bool noBounce = fabs(m_vel*hits[0].n) < m_minBounceVel;
		if (m_flags & particle_single_contact || noBounce && !m_gravity.len2() || iCaller) {
			m_vel.zero();
			g_pPhysWorld->UpdateProjectileState(this);
		}	else if (noBounce) {
			PxRigidDynamic *pRD = g_cryPhysX.Physics()->createRigidDynamic(T(QuatT(Quat(IDENTITY),m_pos)));
			pRD->setActorFlag(PxActorFlag::eSEND_SLEEP_NOTIFIES, true);
			PxShape *shape = pRD->createShape(PxSphereGeometry(m_size), *g_pPhysWorld->GetSurfaceType(m_surfaceIdx));
			PxRigidBodyExt::updateMassAndInertia(*pRD, m_mass*3/(4*gf_PI*cube(m_size)));
			shape->setSimulationFilterData(PxFilterData(0,geom_colltype_ray,0,2|16));
			pRD->setLinearVelocity(V(m_vel));
			m_actor = pRD;
			m_actor->userData = this;
			g_cryPhysX.Scene()->addActor(*m_actor);
		}
	}	else
		m_pos += rp.dir;

	if (m_flags & pef_monitor_poststep && !iCaller) {
		EventPhysPostStep epps;
		epps.pEntity=this; epps.pForeignData=m_pForeignData; epps.iForeignData=m_iForeignData;
		epps.pos = m_pos;
		epps.q = getGlobalPose().q;
		epps.dt = dt;
		g_pPhysWorld->SendEvent(epps,0);
	}
	AtomicAdd(&m_stepped,1);
	return 0; 
}
