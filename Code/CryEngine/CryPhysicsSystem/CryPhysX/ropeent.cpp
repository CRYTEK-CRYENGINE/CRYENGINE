// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"

#include "geometries.h"
#include "entities.h"
#include "world.h"

using namespace primitives;
using namespace cpx;
using namespace Helper;


PhysXRope::PhysXRope(int id,pe_params* params) 
{ 
	m_type=PE_ROPE; m_id=id; 
	InsertBefore(this, g_pPhysWorld->m_auxEntList[1]);
	m_idStepLastAwake = g_pPhysWorld->m_idStep;
	if (params) 
		SetParams(params); 
}

void PhysXRope::setGlobalPose(const QuatT& pose, int components)
{
	for(int i=0;i<m_points.size();i++)
		m_points[i] = pose*(m_points[i]-m_pos);
	m_pos = pose.t;
}

void PhysXRope::getBBox(Vec3 *BBox) const
{
	if (m_points.size()) {
		BBox[0] = min(m_points[0], m_points[m_points.size()-1]);
		BBox[1] = max(m_points[0], m_points[m_points.size()-1]);
	} else
		BBox[0] = BBox[1] = m_pos;
}

bool PhysXRope::IsAwake() const
{
	PxRigidActor *actors[2];
	if (!m_joint)
		return false;
	if (g_pPhysWorld->m_idStep <= m_idStepLastAwake+1)
		return true;
	m_joint->getActors(actors[0], actors[1]);
	int awake = 0;
	for(int i=0; i<2; i++)
		awake |= actors[i] && Ent(actors[i])->IsAwake();
	m_idStepLastAwake += g_pPhysWorld->m_idStep-m_idStepLastAwake & -awake;
	return g_pPhysWorld->m_idStep <= m_idStepLastAwake+1;
}

int PhysXRope::SetParams(pe_params* _params, int bThreadSafe)
{
	if (_params->type==pe_params_rope::type_id) {
		pe_params_rope *params = (pe_params_rope*)_params;
		int i;
		if (!is_unused(params->length)) m_length = params->length;
		if (!is_unused(params->nSegments)) {
			m_points.resize(params->nSegments+1);
			float seglen = m_length/max(1,params->nSegments);
			for(i=0;i<=params->nSegments;i++) (m_points[i]=m_pos).z -= m_length*i*seglen;
		}
		if (!is_unused(params->pPoints) && params->pPoints)	{
			for(i=m_points.size()-1,m_length=0;i>0;i--) m_length += ((m_points[i]=params->pPoints[i])-params->pPoints[i-1]).len();
			m_points[0] = params->pPoints[0];
		}
		Vec3 pt[2] = { m_points[0], m_points[m_points.size()-1] };
		PxRigidActor *actors[2] = { nullptr,nullptr };
		if (m_joint)
			m_joint->getActors(actors[0], actors[1]);
		for(i=0;i<2;i++) if (!is_unused(params->pEntTiedTo[i]))
			if (!params->pEntTiedTo[i])
				break;
			else if (params->pEntTiedTo[i]==WORLD_ENTITY) {
				if (!is_unused(params->ptTiedTo[i])) pt[i] = params->ptTiedTo[i];
			} else {
				PhysXEnt *pent = (PhysXEnt*)params->pEntTiedTo[i];
				actors[i] = pent->getRigidBody(max(0,pent->idxPart(params->idPartTiedTo[i])));
				if (!is_unused(params->ptTiedTo[i]))
					pt[i] = params->bLocalPtTied && pent ? pent->getGlobalPose()*params->ptTiedTo[i] : params->ptTiedTo[i];
				if (actors[i])
					pt[i] = T(actors[i]->getGlobalPose()).GetInverted() * pt[i];
			}
		if (i<0) {
			if (m_joint)
				m_joint->setDistanceJointFlag(PxDistanceJointFlag::eMAX_DISTANCE_ENABLED, false);
		}	else if (!is_unused(params->pEntTiedTo[0]) || !is_unused(params->pEntTiedTo[1])) {
			if (!m_joint)
				m_joint = PxDistanceJointCreate(*g_cryPhysX.Physics(), actors[0],T(pt[0]), actors[1],T(pt[1]));
			else {
				m_joint->setActors(actors[0], actors[1]);
				m_joint->setLocalPose(PxJointActorIndex::eACTOR0, T(pt[0]));
				m_joint->setLocalPose(PxJointActorIndex::eACTOR1, T(pt[1]));
			}
			if (m_joint) {
				m_joint->setMaxDistance(m_length);
				m_joint->setDistanceJointFlag(PxDistanceJointFlag::eMAX_DISTANCE_ENABLED, true);
				m_joint->setDistanceJointFlag(PxDistanceJointFlag::eMIN_DISTANCE_ENABLED, false);
			}
		}
		return 1;
	}
	return PhysXEnt::SetParams(_params,bThreadSafe);
}

bool PhysXRope::UpdateEnds()
{
	if (m_joint && m_points.size()) {
		PxRigidActor *actors[2];
		Vec3 pt[2];
		m_joint->getActors(actors[0], actors[1]);
		for(int i=0;i<2;i++) {
			pt[i] = V(m_joint->getLocalPose(PxJointActorIndex::Enum(i)).p);
			if (actors[i])
				pt[i] = T(actors[i]->getGlobalPose())*pt[i];
		}
		if (max((pt[0]-m_points[0]).len2(), (pt[1]-m_points[m_points.size()-1]).len2())>0.0001f) {
			m_points[0] = pt[0]; m_points[m_points.size()-1] = pt[1];
			Vec3 dir = pt[1]-pt[0], dirNorm = dir.GetNormalized();
			float seglen = (dir*dirNorm)/max(1,m_points.size()-1);
			for(int i=0; i<m_points.size(); i++)
				m_points[i] = pt[0]+dirNorm*(i*seglen);
			return true;
		}
	}
	return false;
}

int PhysXRope::GetStatus(pe_status* _status) const
{
	if (_status->type==pe_status_rope::type_id) {
		pe_status_rope *status = (pe_status_rope*)_status;
		status->nSegments = max(0,m_points.size()-1);
		if (!is_unused(status->pPoints) && status->pPoints)	{
			const_cast<PhysXRope*>(this)->UpdateEnds();
			for(int i=0;i<m_points.size();i++)
				status->pPoints[i] = m_points[i];
		}
		if (!is_unused(status->pVelocities) && status->pVelocities)
			for(int i=0; i<=m_points.size(); status->pVelocities[i++].zero());
		return 1;
	}
	return PhysXEnt::GetStatus(_status);
}

int PhysXRope::GetParams(pe_params* _params) const
{
	if (_params->type==pe_params_rope::type_id) {
		pe_params_rope *params = (pe_params_rope*)_params;
		memset(params, 0, sizeof(*params));
		params->type = pe_params_rope::type_id;
		params->nSegments = max(0,m_points.size()-1);
		params->length = m_length;
		params->mass = 0.1f;
		const_cast<PhysXRope*>(this)->UpdateEnds();
		params->pPoints = strided_pointer<Vec3>((Vec3*)&m_points[0]);
		static Vec3 vel0(ZERO);
		params->pVelocities = strided_pointer<Vec3>(&vel0,0);
		return 1;
	}
	return PhysXEnt::GetParams(_params);
}

int PhysXRope::Action(pe_action* _action, int bThreadSafe)
{
	if (_action->type==pe_action_reset::type_id) {
		if (m_joint)
			m_joint->release();
		m_joint = nullptr;
		return 1;
	}
	return PhysXEnt::Action(_action,bThreadSafe);
}
