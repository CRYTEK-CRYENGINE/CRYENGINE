// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"

#include "geometries.h"
#include "entities.h"
#include "world.h"

using namespace primitives;
using namespace cpx;
using namespace Helper;


PhysXArticulation::PhysXArticulation(int id, pe_params* params)
{
	m_type = PE_ARTICULATED;
	m_id = id;
	m_iSimClass = 4;
	m_actor = g_cryPhysX.Physics()->createRigidDynamic(T(QuatT(IDENTITY)));
	m_actor->userData = this;
	m_artic = g_cryPhysX.Physics()->createArticulation();
	m_artic->userData = this;
	if (params) 
		SetParams(params); 
}

PxArticulationLink *PhysXArticulation::findJoint(int id) const
{
	int n = m_artic->getNbLinks();
	PxArticulationLink **links = (PxArticulationLink**)alloca(n*sizeof(void*));
	m_artic->getLinks(links,n);
	for(int i=0;i<n;i++) if (JointId(links[i])==id)
		return links[i];
	return nullptr;
}

QuatT PhysXArticulation::getGlobalPose() const
{
	if (IsRagdoll() && !m_link0) return T(m_actor->getGlobalPose());
	return IsRagdoll() ? QuatT(Quat(IDENTITY), V(m_link0->getGlobalPose().p)+m_offsPivot) : T(m_actor->getGlobalPose());
}
void PhysXArticulation::setGlobalPose(const QuatT& pose, int components)
{
	if (IsRagdoll()) {
		QuatT delta = pose*QuatT(m_rotExt, getGlobalPose().t).GetInverted();
		if (components & 2)
			m_rotExt = pose.q;
		IterateJoints([delta](auto link) { link->setGlobalPose(T(delta*T(link->getGlobalPose()))); });
	} else
		PhysXEnt::setGlobalPose(pose,components);	
}

PxRigidBody *PhysXArticulation::getRigidBody(int ipart) const
{
	if ((uint)ipart>=m_parts.size() || !IsRagdoll() || !m_artic->getScene())
		return PhysXEnt::getRigidBody(ipart);
	return findJoint(JointId(m_parts[ipart].shape));
}

QuatT PhysXArticulation::getLocalPose(const PxRigidBody* link, int idxPart) const 
{ 
	PxShape *shapeLink = m_parts[idxPart].shape;
	for(int i=link->getNbShapes()-1; i>=0 && link->getShapes(&shapeLink,1,i) && PartId(shapeLink)!=PartId(m_parts[idxPart].shape); i--);
	QuatT poseDiff(IDENTITY), poseShapeGlob = T(link->getGlobalPose()*shapeLink->getLocalPose());
	((PhysXGeom*)m_parts[idxPart].geom->pGeom)->CreateAndUse(poseDiff,m_parts[idxPart].scale,[](const PxGeometry&){});
	return getGlobalPose().GetInverted() * poseShapeGlob * poseDiff.GetInverted(); 
}
QuatT PhysXArticulation::getLocalPose(int idxPart) const 
{ 
	if (IsRagdoll())
		if (PxRigidBody *link = getRigidBody(idxPart)) 
			return getLocalPose(link, idxPart);
	return PhysXEnt::getLocalPose(idxPart);
}

void PhysXArticulation::Enable(bool enable)
{
	g_cryPhysX.Scene()->removeActor(*m_actor);
	g_cryPhysX.Scene()->removeArticulation(*m_artic);
	if (enable)	{
		_InterlockedOr((volatile long*)&m_mask, 4);
		if (IsRagdoll()) {
			g_cryPhysX.Scene()->addArticulation(*m_artic);
			Awake(m_iSimClass==2);
		} else
			g_cryPhysX.Scene()->addActor(*m_actor);
	} else
		_InterlockedAnd((volatile long*)&m_mask, ~4);
}

void PhysXArticulation::Awake(bool awake, float minTime)
{
	if (IsRagdoll() && m_mask & 4) {
		AwakeEnt(m_artic, awake,minTime);
		if (awake && m_list[0]==this)	{
			_InterlockedOr((volatile long*)&m_mask, awake ? 1:2);
			InsertBefore(this, g_pPhysWorld->m_activeEntList[1]);
		}
	}
}
bool PhysXArticulation::IsAwake() const
{
	return IsRagdoll() && !m_artic->isSleeping();
}
void PhysXArticulation::getBBox(Vec3 *BBox) const
{
	if (IsRagdoll()) {
		PxBounds3 bbox = m_artic->getWorldBounds();
		BBox[0] = V(bbox.minimum); BBox[1] = V(bbox.maximum);
	}	else
		PhysXEnt::getBBox(BBox);
}

void PhysXArticulation::SetSimClass(int iSimClass)
{
	PhysXEnt::SetSimClass(iSimClass);
	if (m_mask & 4)
		MarkForAdd();
}

int PhysXArticulation::AddGeometry(phys_geometry* pgeom, pe_geomparams* params, int id, int bThreadSafe)
{
	int idJoint = 0;
	if (params->type==pe_articgeomparams::type_id)
		idJoint = ((pe_articgeomparams*)params)->idbody;
	if (pgeom->pGeom->GetType()==GEOM_TRIMESH && ((PhysXGeom*)pgeom->pGeom)->m_geom.mesh.pMesh) {
		PxBounds3 bounds = ((PhysXGeom*)pgeom->pGeom)->m_geom.mesh.pMesh->getLocalBounds();
		box bbox;
		bbox.Basis.SetIdentity();
		bbox.center = V(bounds.getCenter());
		bbox.size = V(bounds.getExtents());
		pgeom = g_pPhysWorld->RegisterGeometry(g_pPhysWorld->CreatePrimitive(box::type,&bbox), pgeom->surface_idx, pgeom->pMatMapping,pgeom->nMats);
		pgeom->pGeom->Release(); pgeom->nRefCount=0;
	}
	id = PhysXEnt::AddGeometry(pgeom,params,id,bThreadSafe);
	if (id>=0)
		m_parts[m_parts.size()-1].shape->userData = (void*)(INT_PTR)(idJoint<<16 | id);
	return id;
}

int PhysXArticulation::SetParams(pe_params* _params, int bThreadSafe)
{
	if (_params->type==pe_params_joint::type_id) {
		pe_params_joint *params = (pe_params_joint*)_params;
		QuatT poseEnt = T(m_actor->getGlobalPose());
		PxArticulationLink *link = findJoint(params->op[1]), *linkParent=nullptr;
		if (!link) {
			int i0=-1,n=0; 
			for(int i=0;i<m_parts.size();i++) if (JointId(m_parts[i].shape)==params->op[1])
				n++, i0 += i-i0 & i0>>31;
			if (i0<0)
				return 0;
			QuatT pose = PhysXEnt::getLocalPose(i0);
			if (!(link = m_artic->createLink(linkParent=findJoint(params->op[0]), T(poseEnt*pose))))
				return 0;
			link->setName((const char*)linkParent);
			link->userData = this;
			float *densities = (float*)alloca(n*sizeof(float));
			for(int i=n=0; i<m_parts.size(); i++) if (JointId(m_parts[i].shape)==params->op[1]) {
				PxMaterial *mtl; m_parts[i].shape->getMaterials(&mtl,1);
				QuatT poseNew = pose.GetInverted()*T(m_parts[i].shape->getLocalPose());
				PxShape *shape = PxRigidActorExt::createExclusiveShape(*link, m_parts[i].shape->getGeometry().any(), *mtl);
				if (!shape)
					continue;
				shape->setLocalPose(T(poseNew));
				shape->userData = m_parts[i].shape->userData;
				shape->setSimulationFilterData(m_parts[i].shape->getSimulationFilterData());
				shape->setQueryFilterData(m_parts[i].shape->getQueryFilterData());
				densities[n++] = m_parts[i].density;
			}
			PxRigidBodyExt::updateMassAndInertia(*link, densities, n);
			if (!m_link0) {
				m_link0 = link;
				m_offsPivot = poseEnt.t-poseEnt*pose.t;
			}
			link->setMaxDepenetrationVelocity(g_pPhysWorld->m_vars.maxUnprojVel);
			n=0; IterateJoints([&n](auto link) { n+=link->getNbShapes(); });
			if (n==m_parts.size())
				MarkForAdd();
		}	else
			linkParent = (PxArticulationLink*)link->getName();
		if (PxArticulationJoint *joint = link->getInboundJoint()) {
			Vec3 limits[2] = { Vec3(-0.1f), Vec3(0.1f) };
			int nlimits = 0;
			for(int i=0;i<3;i++) for(int j=0;j<2;j++) if (!is_unused(params->limits[j][i]) && !(params->flags & angle0_locked<<i))
				limits[j][i] = max_safe(-gf_PI2,min_safe(gf_PI2, params->limits[j][i])), nlimits++;
			Quat qCenterLimits = Quat::CreateRotationXYZ(Ang3(Diag33(0,1,1)*(limits[0]+limits[1])*0.5f));
			if (linkParent && (!is_unused(params->pivot) || !is_unused(params->q0) || params->pMtx0)) {
				QuatT poseInParent(IDENTITY),poseInChild(IDENTITY);
				Vec3 pivot = is_unused(params->pivot) ? V(link->getGlobalPose().p) : poseEnt*params->pivot;
				poseInParent.t = T(linkParent->getGlobalPose()).GetInverted() * pivot;
				poseInChild.t = T(link->getGlobalPose()).GetInverted() * pivot;
				if (!is_unused(params->q0)) poseInParent.q = params->q0;
				if (params->pMtx0) poseInParent.q = Quat(*params->pMtx0);
				poseInParent.q = poseInParent.q*qCenterLimits;
				joint->setParentPose(T(poseInParent));
				joint->setChildPose(T(poseInChild));
			}
			if (nlimits) {
				joint->setTwistLimit(limits[0].x, limits[1].x);
				float yzlim[2];
				for(int i=1; i<3; i++) {
					yzlim[i-1] = float2int((limits[1][i]-limits[0][i])*0.5f*100-0.5f)*0.01f;	// centered limit, rounded to 0.01
					yzlim[i-1] += ((limits[0][i]+limits[1][i])*(0.5f/gf_PI2)+0.5f)*0.01f;	// store limit offset below 0.01
				}
				joint->setSwingLimit(yzlim[0], yzlim[1]);
				joint->setTwistLimitEnabled(true);
				joint->setSwingLimitEnabled(true);
			}
			joint->setDamping(0.2f);
		}
		return 1;
	}

	if (_params->type==pe_simulation_params::type_id) {
		pe_simulation_params *params = (pe_simulation_params*)_params;
		if (!is_unused(params->minEnergy)) m_artic->setSleepThreshold(params->minEnergy);
		if (!is_unused(params->iSimClass)) SetSimClass(params->iSimClass);
		return 1;
	}

	if (_params->type==pe_params_articulated_body::type_id) {
		pe_params_articulated_body *params = (pe_params_articulated_body*)_params;
		if (!is_unused(params->dampingLyingMode)) IterateJoints([&params](auto link) {
			if (auto joint = link->getInboundJoint()) 
				joint->setDamping(params->dampingLyingMode);
		});
		if (!is_unused(params->pHost) || !is_unused(params->posHostPivot) || !is_unused(params->qHostPivot)) {
			if (!is_unused(params->pHost) && !params->pHost) {
				m_hostAttach->release(); m_hostAttach = nullptr;
				g_cryPhysX.Scene()->removeActor(*m_actor);
			}	else if (m_parts.size()) {
				PxRigidActor *host = !is_unused(params->pHost) ? ((PhysXEnt*)params->pHost)->m_actor : nullptr;
				QuatT pose = m_hostAttach ? T(m_hostAttach->getLocalPose(PxJointActorIndex::eACTOR1)) : QuatT(IDENTITY);
				if (!is_unused(params->posHostPivot)) pose.t = params->posHostPivot;
				if (!is_unused(params->qHostPivot)) pose.q = params->qHostPivot;
				if (!m_hostAttach) {
					g_cryPhysX.Scene()->addActor(*m_actor);
					if (host)
						m_actor->setGlobalPose(host->getGlobalPose());
					m_hostAttach = PxFixedJointCreate(*g_cryPhysX.Physics(), host,T(pose), m_actor,T(QuatT(IDENTITY)));
					m_hostAttach->setProjectionLinearTolerance(0.01f);
					m_hostAttach->setProjectionAngularTolerance(0.01f);
					m_hostAttach->setConstraintFlag(PxConstraintFlag::ePROJECTION, true);
					m_hostAttach->setConstraintFlag(PxConstraintFlag::eCOLLISION_ENABLED, false);
					m_hostAttach->setInvMassScale0(0);
					m_actor->isRigidDynamic()->setSleepThreshold(0);
					m_actor->isRigidDynamic()->wakeUp();
				}	else {
					if (!is_unused(params->pHost)) m_hostAttach->setActors(host,m_actor);
					m_hostAttach->setLocalPose(PxJointActorIndex::eACTOR0, T(pose));
				}
			}
		}
		return 1;
	}

	return PhysXEnt::SetParams(_params,bThreadSafe);
}

int PhysXArticulation::GetStatus(pe_status* _status) const
{
	if (_status->type==pe_status_joint::type_id) {
		pe_status_joint *status = (pe_status_joint*)_status;
		int idJoint = status->idChildBody, ipart;
		if (!is_unused(status->partid) && (ipart=idxPart(status->partid))>=0) idJoint = JointId(m_parts[ipart].shape);
		PxArticulationLink *link = findJoint(idJoint);
		if (!link)
			return 0;
		Quat qchild = Q(link->getGlobalPose().q), qparent(IDENTITY), q0(IDENTITY);
		if (link->getName())
			qparent = Q(((PxArticulationLink*)link->getName())->getGlobalPose().q);
		if (PxArticulationJoint *joint = link->getInboundJoint()) {
			Quat q0 = Q(joint->getParentPose().q);
			Ang3 angOffs(ZERO);
			float yzlim[2];
			joint->getSwingLimit(yzlim[0], yzlim[1]);
			for(int i=0;i<2;i++)
				angOffs[i+1] = (yzlim[i]*100 - float2int(yzlim[i]*100-0.5f) - 0.5f)*gf_PI;
			q0 = q0*!Quat::CreateRotationXYZ(angOffs);
		}
		status->quat0 = q0;
		status->q = Ang3::GetAnglesXYZ(!q0*!qparent*qchild);
		status->qext = Ang3(ZERO);
		return 1;
	}

	int res = PhysXEnt::GetStatus(_status);
	if (res && _status->type==pe_status_dynamics::type_id) {
		float mass = 0;
		IterateJoints([&mass](auto link) { mass += link->getMass(); });
		((pe_status_dynamics*)_status)->mass = mass;
		return res;
	}

	return res;
}

int PhysXArticulation::Action(pe_action* _action, int bThreadSafe)
{
	if (_action->type==pe_action_batch_parts_update::type_id) {
		pe_action_batch_parts_update *action = (pe_action_batch_parts_update*)_action;
		if (action->pValidator && !action->pValidator->Lock())
			return 0;
		ReapplyParts:
		for(int i=0,j=0,nParts; i<action->numParts; i++) {
			for(;j<m_parts.size() && PartId(m_parts[j].shape)!=action->pIds[i];j++);
			if (j < m_parts.size())
				setLocalPose(j,QuatT(action->qOffs,action->posOffs)*QuatT(action->qParts[i],action->posParts[i]));
			else {
				for(i=nParts=0;i<m_parts.size();i++) for(j=nParts;j<action->numParts;j++) if (action->pIds[j]==PartId(m_parts[i].shape)) {
					std::swap(action->pIds[j],action->pIds[nParts++]); break;
				}
				if (action->pnumParts)
					*action->pnumParts = nParts;
				action->numParts = nParts;
				goto ReapplyParts;
			}
		}
		if (action->pValidator)
			action->pValidator->Unlock();
		return 1;
	}

	return PhysXEnt::Action(_action,bThreadSafe);
}
