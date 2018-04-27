// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"

#include "geometries.h"
#include "entities.h"
#include "world.h"

#include "CryPhysX.h"

using namespace primitives;
using namespace cpx::Helper;


int PhysXLiving::SetParams(pe_params *_params, int bThreadSafe)
{
	if (_params->type == pe_player_dimensions::type_id && m_type == PE_LIVING) {
		pe_player_dimensions *params = (pe_player_dimensions*)_params;
		if (!is_unused(params->sizeCollider) || !m_parts.size()) {
			PxRigidDynamic *pRD = m_actor->isRigidDynamic();
			pe_geomparams gp;
			gp.pos.z = m_parts.size() ? m_parts[0].shape->getLocalPose().p.z : 0.9f;
			Vec3 size = !is_unused(params->sizeCollider) ? params->sizeCollider : Vec3(0.4f, 0.4f, 0.5f);
			float mass = pRD->getMass() ? pRD->getMass() : 80.0f;
			gp.density = m_parts.size() ? m_parts[0].density : 1;
			RemoveGeometry(100);
			if (!is_unused(params->heightCollider)) gp.pos.z = params->heightCollider;
			capsule caps;
			caps.axis = Vec3(0, 0, 1);
			caps.center.zero();
			caps.r = size.x;
			caps.hh = size.z;
			float dh = max(0.0f, gp.pos.z - caps.hh - caps.r)*0.5f;
			gp.pos.z -= dh; caps.hh += dh;
			phys_geometry *pgeom = g_pPhysWorld->RegisterGeometry(g_pPhysWorld->CreatePrimitive(capsule::type, &caps), NSURFACETYPES - 2);
			pgeom->pGeom->Release();
			gp.flags &= ~geom_colltype_ray;
			AddGeometry(pgeom, &gp, 100); pgeom->nRefCount--;
			pRD->setMass(mass);
			pRD->setCMassLocalPose(T(QuatT(Quat(IDENTITY), Vec3(0, 0, gp.pos.z))));
			pRD->setMassSpaceInertiaTensor(V(Vec3(0)));
		}
		return 1;
	}

	if (_params->type == pe_player_dynamics::type_id && m_type == PE_LIVING) {
		pe_player_dynamics *params = (pe_player_dynamics*)_params;
		PxRigidDynamic *pRD = m_actor->isRigidDynamic();
		if (!is_unused(params->mass))	pRD->setMass(params->mass);
		if (!is_unused(params->kInertia)) { pRD->setLinearDamping(params->kInertia ? params->kInertia : 5.0f); }
		else { pRD->setLinearDamping(5.0); }
		if (!is_unused(params->bActive)) pRD->setActorFlag(PxActorFlag::eDISABLE_SIMULATION, !params->bActive);
		return 1;
	}

	return PhysXEnt::SetParams(_params, bThreadSafe);
}

int PhysXLiving::GetStatus(pe_status* _status) const
{
	return PhysXEnt::GetStatus(_status);
}

int PhysXLiving::Action(pe_action* _action, int bThreadSafe)
{
	if (_action->type == pe_action_move::type_id) {
		pe_action_move *action = (pe_action_move*)_action;
		if (!is_unused(action->dir) && m_actor->getScene()) {
			PxRigidDynamic *pRD = m_actor->isRigidDynamic();
			pRD->clearForce();
			pRD->addForce(V(action->dir*(pRD->getLinearDamping()*pRD->getMass())));
		}
		return 1;
	}
	return PhysXEnt::Action(_action, bThreadSafe);
}

void PhysXLiving::PostStep(float dt, int immediate)
{
	if (immediate) {
		PxTransform pose = m_actor->getGlobalPose();
		if ((!m_qRot*Q(pose.q)).v.len2() > 0.001f)
			m_actor->setGlobalPose(PxTransform(pose.p, Q(m_qRot)));
	}
	PhysXEnt::PostStep(dt, immediate);
}
