// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"

#include "geometries.h"
#include "entities.h"
#include "world.h"

using namespace primitives;
using namespace cpx;
using namespace Helper;


PhysXWorld *PhysXEnt::g_pPhysWorld;
IPhysicalWorld* PhysXEnt::GetWorld() const { return g_pPhysWorld; }

int phys_geometry_refcnt::Release() 
{ 
	int res = CryInterlockedDecrement((volatile int*)&nRefCount); 
	if (res<=0) 
		delete this; 
	return res; 
}

PhysXEnt::PhysXEnt(pe_type type, int id, pe_params *params) : m_type(type), m_id(id)
{
	QuatT trans(IDENTITY);
	Diag33 scale;
	if (params && params->type==pe_params_pos::type_id)
		ExtractTransform((pe_params_pos*)params,trans,scale);	
	if (type==PE_STATIC || type==PE_AREA || type==PE_SOFT)	{
		m_actor = g_cryPhysX.Physics()->createRigidStatic(T(trans));
		m_iSimClass = (type==PE_AREA)*5;
	} else {
		PxRigidDynamic *pRD = g_cryPhysX.Physics()->createRigidDynamic(T(trans));
		pRD->setRigidBodyFlag(PxRigidBodyFlag::eKINEMATIC, true);
		pRD->setMaxDepenetrationVelocity(g_pPhysWorld->m_vars.maxUnprojVel);
		m_actor = pRD;
		m_iSimClass = 2;
	}
	if (params)
		SetParams(params);
	m_actor->setActorFlag(PxActorFlag::eSEND_SLEEP_NOTIFIES, type>PE_STATIC);
	m_actor->userData = this;
}

int PhysXEnt::AddGeometry(phys_geometry* pgeom, pe_geomparams* params, int id, int bThreadSafe)
{
	if (!pgeom || !pgeom->pGeom || !m_actor || m_type==PE_LIVING && m_parts.size())
		return -1;
	WriteLockCondScene lock((m_flags & 4)!=0);
	QuatT trans(IDENTITY); Diag33 scale(1);
	ExtractTransform(params,trans,scale);
	part newPart;
	if (id<0)
		for(int i=id=0;i<m_parts.size();i++) id = max(id,PartId(m_parts[i].shape)+1);
	newPart.geom = (phys_geometry_refcnt*)pgeom;
	PhysXGeom *pGeom = (PhysXGeom*)pgeom->pGeom;
	int *pMatMapping=pgeom->pMatMapping, nMats=pgeom->nMats;
	if (params->pMatMapping)
		pMatMapping=params->pMatMapping, nMats=params->nMats;
	PxMaterial *const mtl = g_pPhysWorld->GetSurfaceType(pMatMapping ? pMatMapping[max(0,min(nMats-1,pgeom->surface_idx))] : pgeom->surface_idx);

	newPart.shape = pGeom->CreateAndUse(trans, scale, [this, mtl](const PxGeometry &geom) { return PxRigidActorExt::createExclusiveShape(*m_actor, geom, *mtl); });
	if (!newPart.shape)
		return -1;
	newPart.shape->setLocalPose(T(trans));
	
	if (pGeom->GetType()== GEOM_HEIGHTFIELD) newPart.shape->setFlag(physx::PxShapeFlag::eVISUALIZATION, false); // disable debug vis for heightfield (performance)

	PxFilterData fd(params->flags, params->flagsCollider, m_flags & pef_auto_id ? -3 : m_id, m_type!=PE_STATIC);
	newPart.shape->setSimulationFilterData(fd);
	newPart.shape->setQueryFilterData(fd);
	newPart.scale = scale;
	newPart.density = params->mass>0 && pgeom->V>0 ? params->mass/(pgeom->V*(scale*Vec3(1)).GetVolume()) : max(0.0f,params->density);
	newPart.shape->userData = (void*)(INT_PTR)id;
	
	m_parts.push_back(newPart);
	if (newPart.density>0)
		if (PxRigidBody *pRB = m_actor->isRigidBody()) {
			auto meshList = std::remove_if(m_parts.begin(),m_parts.end(), [this](auto part) { 
				if (part.shape->getGeometryType()==PxGeometryType::eTRIANGLEMESH)	{
					m_actor->detachShape(*part.shape);
					return true;
				}
				return false;
			});
			m_parts.erase(meshList, m_parts.end());
			float *densities = (float*)alloca(m_parts.size()*sizeof(float));
			for(int i=0; i<m_parts.size(); i++)
				densities[i] = max(1e-8f, m_parts[i].density);
			pRB->setRigidBodyFlag(PxRigidBodyFlag::eKINEMATIC, false);
			PxRigidBodyExt::updateMassAndInertia(*pRB, densities, m_parts.size());
		}
	if (m_type!=PE_ARTICULATED && m_type!=PE_SOFT && !(m_mask & 4))
		MarkForAdd();

	return id;
}

void PhysXEnt::RemoveGeometry(int id, int bThreadSafe)
{
	if (!m_actor)
		return;
	WriteLockScene lock;
	auto list = std::remove_if(m_parts.begin(), m_parts.end(), [id,this](auto part) { 
		if (PartId(part.shape)==id) {
			m_actor->detachShape(*part.shape);
			return true;
		}
		return false;
	});
	m_parts.erase(list, m_parts.end());
}

PhysXEnt::~PhysXEnt() 
{ 
	RemoveFromList(this);
	if (m_actor)
		m_actor->release(); 
}

void PhysXEnt::MarkForAdd()
{
	if (!g_pPhysWorld->m_dt) {
		WriteLockScene lock;
		AddToScene();
	}	else if (!(m_mask & 1u<<31)) {
		InsertBefore(this, g_pPhysWorld->m_addEntList[1]);
		_InterlockedOr((volatile long*)&m_mask, 1u<<31);
	}
}

PhysXEnt* PhysXEnt::AddToScene(bool fromList)
{
	PhysXEnt *next = m_list[1];
	if (fromList)
		m_list[0] = m_list[1] = this;
	Enable(!(m_mask & 1<<5));
	_InterlockedAnd((volatile long*)&m_mask, ~(1u<<31 | 1<<5));
	return next;
}

void PhysXEnt::DrawHelperInformation(IPhysRenderer *pRenderer, int iDrawHelpers)
{
	pe_params_bbox bb;
	QuatT pose = getGlobalPose();
	pRenderer->DrawLine(pose.t, pose.t + Vec3(0, 0, 3));
}

void PhysXEnt::Enable(bool enable) 
{ 
	if (!enable) {
		_InterlockedAnd((volatile long*)&m_mask, ~4);
		g_cryPhysX.Scene()->removeActor(*m_actor);
	} else {
		_InterlockedOr((volatile long*)&m_mask, 4);
		g_cryPhysX.Scene()->addActor(*m_actor);
		if (m_iSimClass<=3) 
			AwakeEnt(m_actor->isRigidDynamic(), m_iSimClass>1);
	}
}

void PhysXEnt::Awake(bool awake, float minTime)
{
	AwakeEnt(m_actor->isRigidDynamic(), awake, minTime);
}

QuatT PhysXEnt::getLocalPose(int idxPart) const 
{ 
	QuatT poseDiff(IDENTITY);
	((PhysXGeom*)m_parts[idxPart].geom->pGeom)->CreateAndUse(poseDiff,m_parts[idxPart].scale,[](const PxGeometry&){});
	return cpx::Helper::T(m_parts[idxPart].shape->getLocalPose()) * poseDiff.GetInverted(); 
}

void PhysXEnt::setLocalPose(int idxPart, const QuatT& pose) 
{ 
	QuatT posePx = pose;
	((PhysXGeom*)m_parts[idxPart].geom->pGeom)->CreateAndUse(posePx,m_parts[idxPart].scale,[](const PxGeometry&){});
	m_parts[idxPart].shape->setLocalPose(cpx::Helper::T(posePx)); 
}

void PhysXEnt::SetSimClass(int iSimClass)
{
	m_iSimClass = iSimClass;
	if (m_iSimClass<3)
		Awake(m_iSimClass==2);
	for(int i=0;i<m_parts.size();i++) {
		PxFilterData fd = m_parts[i].shape->getSimulationFilterData();
		(fd.word3 &= ~7) |= m_iSimClass;
		m_parts[i].shape->setSimulationFilterData(fd);
		m_parts[i].shape->setQueryFilterData(fd);
	}
}

void PhysXEnt::PostStep(float dt, int immediate)
{
	if (m_flags & (pef_log_poststep + (pef_monitor_poststep-pef_log_poststep & -immediate))) {
		EventPhysPostStep epps;
		epps.pEntity=this; epps.pForeignData=m_pForeignData; epps.iForeignData=m_iForeignData;
		QuatT pose = getGlobalPose();
		epps.pos = pose.t;
		epps.q = pose.q;
		epps.dt = dt;
		g_pPhysWorld->SendEvent(epps,immediate^1);
	}
}

int PhysXEnt::SetParams(pe_params *_params, int bThreadSafe)
{
	if (_params->type==pe_params_pos::type_id) {
		pe_params_pos *params = (pe_params_pos*)_params;
		QuatT trans = getGlobalPose(); 
		Diag33 scale(1);
		if (int upd = ExtractTransform(params,trans,scale))
			setGlobalPose(trans, upd);
		if (!is_unused(params->iSimClass)) SetSimClass(params->iSimClass);
		return 1;
	}

	if (_params->type==pe_params_foreign_data::type_id) {
		pe_params_foreign_data *params = (pe_params_foreign_data*)_params;
		if (!is_unused(params->pForeignData)) m_pForeignData = params->pForeignData;
		if (!is_unused(params->iForeignData)) m_iForeignData = params->iForeignData;
		if (!is_unused(params->iForeignFlags)) m_iForeignFlags = params->iForeignFlags;
		return 1;
	}

	if (_params->type==pe_params_flags::type_id) {
		pe_params_flags *params = (pe_params_flags*)_params;
		if (!is_unused(params->flagsAND)) m_flags &= params->flagsAND;
		if (!is_unused(params->flagsOR)) m_flags |= params->flagsOR;
		if (!is_unused(params->flags)) m_flags = params->flags;
		return 1;
	}

	if (_params->type==pe_params_part::type_id) {
		pe_params_part *params = (pe_params_part*)_params;
		int i0=0, i1=m_parts.size()-1;
		if (!is_unused(params->partid))
			i0=i1 = idxPart(params->partid);
		else if (!is_unused(params->ipart))
			i0 = i1 = params->ipart;
		if ((uint)i0 >= m_parts.size())
			return 0;
		for(int i=i0; i<=i1; i++) {
			PxFilterData fd = m_parts[i].shape->getSimulationFilterData();
			if (!is_unused(params->flagsCond) && !(fd.word0 & params->flagsCond))
				continue;
			fd.word0 &= params->flagsAND;
			fd.word0 |= params->flagsOR;
			fd.word1 &= params->flagsColliderAND;
			fd.word1 |= params->flagsColliderOR;
			m_parts[i].shape->setSimulationFilterData(fd);
			m_parts[i].shape->setQueryFilterData(fd);
			if (!is_unused(params->pos) || !is_unused(params->q)) {
				QuatT trans = getLocalPose(i); 
				if (!is_unused(params->pos)) trans.t = params->pos;
				if (!is_unused(params->q)) trans.q = params->q;
				setLocalPose(i,trans);
				/*if (PxRigidBody *pRB = m_actor->isRigidBody())
					if (!(pRB->getRigidBodyFlags() & PxRigidBodyFlag::eKINEMATIC) && m_type!=PE_ARTICULATED) {
						float *densities = (float*)alloca(m_parts.size()*sizeof(float));
						for(int i=0; i<m_parts.size(); i++)
						densities[i] = max(1e-8f, m_parts[i].density);
						PxRigidBodyExt::updateMassAndInertia(*pRB, densities, m_parts.size());
					}*/
			}
		}
		return 1;
	}

	if (_params->type==pe_simulation_params::type_id && m_actor) {
		pe_simulation_params *params = (pe_simulation_params*)_params;
		if (PxRigidDynamic *pRD = m_actor->isRigidDynamic()) {
			if (!is_unused(params->minEnergy)) pRD->setSleepThreshold(params->minEnergy);
			if (!is_unused(params->damping)) {
				pRD->setLinearDamping(params->damping);
				pRD->setAngularDamping(params->damping);
			}
		}
		if (!is_unused(params->iSimClass)) SetSimClass(params->iSimClass);
		return 1;
	}

	if (_params->type==pe_params_buoyancy::type_id && m_type==PE_AREA && m_flags & pef_ignore_areas) {
		pe_params_buoyancy *params = (pe_params_buoyancy*)_params;
		if (params->iMedium==0) {
			if (!is_unused(params->waterPlane.origin)) g_pPhysWorld->m_pbGlob.waterPlane.origin = params->waterPlane.origin;
			if (!is_unused(params->waterPlane.n)) g_pPhysWorld->m_pbGlob.waterPlane.n = params->waterPlane.n;
			if (!is_unused(params->waterDensity)) g_pPhysWorld->m_pbGlob.waterDensity = params->waterDensity;
		}	else
			if (!is_unused(params->waterFlow)) g_pPhysWorld->m_wind = params->waterFlow;
		return 1;
	}

	return 0;
}

int PhysXEnt::GetParams(pe_params* _params) const 
{ 
	if (_params->type==pe_params_foreign_data::type_id) {
		pe_params_foreign_data *params = (pe_params_foreign_data*)_params;
		params->pForeignData = m_pForeignData;
		params->iForeignData = m_iForeignData;
		params->iForeignFlags = m_iForeignFlags;
		return 1;
	}

	if (_params->type==pe_params_bbox::type_id) {
		pe_params_bbox *params = (pe_params_bbox*)_params;
		getBBox(params->BBox);
		return 1;
	}

	if (_params->type==pe_params_buoyancy::type_id && m_type==PE_AREA) {
		*(pe_params_buoyancy*)_params = g_pPhysWorld->m_pbGlob;
		return 1;
	}

	if (_params->type==pe_simulation_params::type_id) {
		pe_simulation_params *params = (pe_simulation_params*)_params;
		memset(params, 0, sizeof(pe_simulation_params));
		params->type = pe_simulation_params::type_id;
		params->gravity = g_pPhysWorld->m_vars.gravity;
		if (PxRigidDynamic *pRD = m_actor->isRigidDynamic()) {
			params->damping = pRD->getLinearDamping();
			params->minEnergy = pRD->getSleepThreshold();
			params->mass = pRD->getMass();
		}
		return 1;
	}

	if (_params->type==pe_params_part::type_id) {
		pe_params_part *params = (pe_params_part*)_params;
		int i = 0;
		if (!is_unused(params->partid))
			i = idxPart(params->partid);
		else if (!is_unused(params->ipart))
			i = params->ipart;
		if ((unsigned int)i >= m_parts.size())
			return 0;
		params->ipart = i;
		params->partid = PartId(m_parts[i].shape);
		params->density = m_parts[i].density;
		params->mass = m_parts[i].geom->pGeom->GetVolume()*params->density*(Vec3(1)*m_parts[i].scale).GetVolume();
		PxFilterData fd = m_parts[i].shape->getSimulationFilterData();
		params->flagsAND=params->flagsOR = fd.word0;
		params->flagsColliderAND=params->flagsColliderOR = fd.word1;
		QuatT pose = getLocalPose(i);
		params->pos = pose.t;
		params->q = pose.q;
		params->scale = m_parts[i].scale.x;
		params->pPhysGeom=params->pPhysGeomProxy = m_parts[i].geom;
		if (params->bAddrefGeoms)
			g_pPhysWorld->AddRefGeometry(params->pPhysGeom);	
		params->pMatMapping = 0;
		params->nMats = 0;
		params->pLattice = 0;
		return 1;
	}

	if (_params->type==pe_params_area::type_id && m_type==PE_AREA) {
		pe_params_area *params = (pe_params_area*)_params;
		memset(params, 0, sizeof(pe_params_area));
		params->gravity = g_pPhysWorld->m_vars.gravity;
		params->bUniform = 1;
		return 1;
	}

	return 0; 
}

int PhysXEnt::GetStatus(pe_status* _status) const
{
	if (_status->type==pe_status_pos::type_id) {
		pe_status_pos *status = (pe_status_pos*)_status;
		QuatT trans = getGlobalPose();
		Diag33 scale(1);
		if (status->ipart>=0 || status->partid>=0) {
			uint i = status->ipart;
			if (status->partid>=0 && status->ipart<0)
				i = idxPart(status->partid);
			if (i>=m_parts.size())
				return 0;
			QuatT transPart = getLocalPose(i);
			trans = status->flags & status_local ? transPart : trans*transPart;
			scale = m_parts[i].scale;
			status->partid = PartId(m_parts[i].shape);
			status->pGeom = status->pGeomProxy = m_parts[i].geom->pGeom;
			status->flagsOR=status->flagsAND = m_parts[i].shape->getSimulationFilterData().word0;
		}
		status->pos = trans.t;
		status->q = trans.q;
		status->scale = scale.x;
		if (status->pMtx3x3)
			*status->pMtx3x3 = Matrix33(trans.q)*scale;
		if (status->pMtx3x4)
			*status->pMtx3x4 = Matrix34(Matrix33(trans.q)*scale, trans.t);
		status->iSimClass = 0;
		if (m_actor) if (PxRigidDynamic *pRD = m_actor->isRigidDynamic())
			status->iSimClass = 2-pRD->isSleeping();
		getBBox(status->BBox);
		status->BBox[0]-=status->pos; status->BBox[1]-=status->pos;
		return 1;
	}

	if (_status->type==pe_status_nparts::type_id)
		return m_parts.size();

	if (_status->type==pe_status_dynamics::type_id) {
		pe_status_dynamics *status = (pe_status_dynamics*)_status;
		if (PxRigidBody *pRB = getRigidBody(0)) {
			status->centerOfMass = T(pRB->getGlobalPose())*V(pRB->getCMassLocalPose().p);
			status->v = V(pRB->getLinearVelocity());
			status->w = V(pRB->getAngularVelocity());
			status->mass = pRB->getMass();
			status->a=status->wa = Vec3(0);
			return 1;
		}
	}

	if (_status->type==pe_status_area::type_id && m_type==PE_AREA) {
		pe_status_area *status = (pe_status_area*)_status;
		status->gravity = g_pPhysWorld->m_vars.gravity;
		status->pb = g_pPhysWorld->m_pbGlob;
		return 1;
	}

	if (_status->type==pe_status_awake::type_id)
		return IsAwake();

	return 0;
}


template<class Taction> void ExtractConstrFrames(PhysXEnt *ent0,PhysXEnt *ent1, int ipart0,int ipart1, Taction *action, QuatT* frame)
{
	for(int i=0; i<2; i++)
		if (PhysXEnt *pent = ((PhysXEnt*[2]){ent0,ent1})[i]) {
			if (!is_unused(action->pt[i])) {
				frame[i].t = action->pt[i];
				if (!(action->flags & (local_frames|local_frames_part)) && is_unused(action->pt[i^1]))
					frame[i^1].t = action->pt[i];
			}
			if (!is_unused(action->qframe[i])) {
				frame[i].q = action->qframe[i];
				if (!(action->flags & (local_frames|local_frames_part)) && is_unused(action->qframe[i^1]))
					frame[i^1].q = action->qframe[i];
			}
			if (action->flags & local_frames_part) 
				frame[i] = pent->getLocalPose(ipart0+(ipart1-ipart0 & -i))*frame[i];
			if (action->flags & (local_frames|local_frames_part))
				frame[i] = pent->getGlobalPose()*frame[i];
		}
}

int PhysXEnt::Action(pe_action* _action, int bThreadSafe)
{
	if (_action->type==pe_action_awake::type_id) {
		pe_action_awake *action = (pe_action_awake*)_action;
		Awake(!!action->bAwake);
		if ((uint)m_iSimClass-1u < 2u)
			m_iSimClass = action->bAwake+1;
		return 1;
	}

	if (_action->type==pe_action_impulse::type_id) {
		pe_action_impulse *action = (pe_action_impulse*)_action;

		if (m_actor && !m_actor->getScene()) Enable(true); // enable (i.e., add to scene), if not already in scene

		int i = is_unused(action->ipart) ? 0 : action->ipart;
		if (!is_unused(action->partid)) i = idxPart(action->partid);
		PxRigidBody *pRB = getRigidBody(i);

		if (pRB) {
			Vec3 dL = !is_unused(action->angImpulse) ? action->angImpulse : Vec3(0);
			if (!is_unused(action->impulse)) {
				pRB->addForce(V(action->impulse), PxForceMode::eIMPULSE);
				if (!is_unused(action->point)) {
					Vec3 com = T(pRB->getGlobalPose())*V(pRB->getCMassLocalPose().p);
					dL += action->point - com ^ action->impulse;
				}
			}
			pRB->addTorque(V(dL), PxForceMode::eIMPULSE);
			Awake(true, 0.5f);
		}
		return 1;
	}

	if (_action->type==pe_action_set_velocity::type_id) {
		pe_action_set_velocity *action = (pe_action_set_velocity*)_action;
		int i = is_unused(action->ipart) ? 0 : action->ipart;
		if (!is_unused(action->partid)) i = idxPart(action->partid);
		if (PxRigidBody *pRB = getRigidBody(i)) {
			if (!is_unused(action->v)) pRB->setLinearVelocity(V(action->v));
			if (!is_unused(action->w)) pRB->setAngularVelocity(V(action->w));
			return 1;
		}
	}

	if (_action->type==pe_action_reset::type_id) {
		pe_action_set_velocity asv;
		asv.v.zero(); asv.w.zero();
		Action(&asv);
		return 1;
	};

	if (_action->type==pe_action_add_constraint::type_id) {
		pe_action_add_constraint *action = (pe_action_add_constraint*)_action;
		if (!action->pBuddy || is_unused(action->pt[0]))
			return 0;
		PhysXEnt *pBuddy = action->pBuddy!=WORLD_ENTITY ? (PhysXEnt*)action->pBuddy : nullptr;
		QuatT frame[2] = { QuatT(IDENTITY), QuatT(IDENTITY) };
		int ipart[2];
		ExtractConstrFrames(this, pBuddy, ipart[0]=idxPart(action->partid[0]), ipart[1]=(pBuddy ? pBuddy->idxPart(action->partid[1]):0), action, frame);
		PxRigidBody *body[2] = { getRigidBody(ipart[0]), pBuddy ? pBuddy->getRigidBody(ipart[1]) : nullptr };
		for(int i=0;i<2;i++) if (body[i])
			frame[i] = T(body[i]->getGlobalPose()).GetInverted()*frame[i];
		PxD6Joint *joint = PxD6JointCreate(*g_cryPhysX.Physics(), body[0], T(frame[0]), body[1], T(frame[1]));
		if (!joint)
			return 0;
		PxD6Motion::Enum defMotion = action->flags & constraint_no_rotation ? PxD6Motion::eLOCKED : PxD6Motion::eFREE;
		if (!is_unused(action->xlimits[0]) && !action->xlimits[0] && !action->xlimits[1] && !is_unused(action->yzlimits[0] && !action->yzlimits[1])) {
			joint->setMotion(PxD6Axis::eTWIST, defMotion);
			joint->setMotion(PxD6Axis::eSWING1, defMotion);
			joint->setMotion(PxD6Axis::eSWING2, defMotion);
		} else {
			if (is_unused(action->xlimits[0]) || action->xlimits[1]>=gf_PI*2)
				joint->setMotion(PxD6Axis::eTWIST, defMotion);
			else if (action->xlimits[0]>=action->xlimits[1])
				joint->setMotion(PxD6Axis::eTWIST, PxD6Motion::eLOCKED);
			else {
				joint->setMotion(PxD6Axis::eTWIST, PxD6Motion::eLIMITED);
				joint->setTwistLimit(PxJointAngularLimitPair(action->xlimits[0], action->xlimits[1]));
			}
			if (is_unused(action->yzlimits[0]) || action->yzlimits[1]>=gf_PI) {
				joint->setMotion(PxD6Axis::eSWING1, defMotion);
				joint->setMotion(PxD6Axis::eSWING2, defMotion);
			} else if (!action->yzlimits[1]) {
				joint->setMotion(PxD6Axis::eSWING1, PxD6Motion::eLOCKED);
				joint->setMotion(PxD6Axis::eSWING2, PxD6Motion::eLOCKED);
			} else {
				joint->setMotion(PxD6Axis::eSWING1, PxD6Motion::eLIMITED);
				joint->setMotion(PxD6Axis::eSWING2, PxD6Motion::eLIMITED);
				joint->setSwingLimit(PxJointLimitCone(action->yzlimits[1], action->yzlimits[1]));
			}
		}
		int id = 0;
		if (is_unused(action->id))
			std::find_if(m_constraints.begin(),m_constraints.end(), [&](auto p)->bool { return (int)(INT_PTR)p->userData!=++id; });
		else {
			id = action->id;
			auto iter = std::find(m_constraints.begin(),m_constraints.end(), comp_userDataPtr<PxD6Joint>::ptrFromKey(id));
			if (iter!=m_constraints.end()) {
				(*iter)->release();
				m_constraints.erase(iter);
			}
		}
		joint->setConstraintFlag(PxConstraintFlag::eCOLLISION_ENABLED, !(action->flags & constraint_ignore_buddy));
		joint->userData = (void*)(INT_PTR)max(id,1);
		m_constraints.insert(joint);
		Awake(true, 1.0f);
		return 1;
	}

	if (_action->type==pe_action_update_constraint::type_id) {
		pe_action_update_constraint *action = (pe_action_update_constraint*)_action;
		auto iter = m_constraints.find(comp_userDataPtr<PxD6Joint>::ptrFromKey(action->idConstraint));
		if (iter==m_constraints.end())
			return 0;
		auto *joint = *iter;
		if (action->bRemove) {
			joint->release();
			m_constraints.erase(iter);
		} else {
			PxRigidActor *actors[2];
			joint->getActors(actors[0], actors[1]);
			QuatT frame[2] = { T(joint->getLocalPose(PxJointActorIndex::eACTOR0)), T(joint->getLocalPose(PxJointActorIndex::eACTOR1)) };
			ExtractConstrFrames(Ent(actors[0]),actors[1] ? Ent(actors[1]):nullptr, 0,0, action,frame);
			joint->setLocalPose(PxJointActorIndex::eACTOR0, actors[0]->getGlobalPose().getInverse()*T(frame[0]));
			joint->setLocalPose(PxJointActorIndex::eACTOR1, actors[1] ? actors[1]->getGlobalPose().getInverse()*T(frame[1]) : T(frame[1]));
		}
		return 1;
	}

	if (_action->type==pe_action_reset::type_id) {
		std::for_each(m_constraints.begin(),m_constraints.end(), [](auto pjnt) { pjnt->release(); });
		m_constraints.clear();
		return 1;
	}

	return 0;
}

int PhysXEnt::GetStateSnapshot(TSerialize ser, float time_back, int flags) { return 0; }
int PhysXEnt::SetStateFromSnapshot(TSerialize ser, int flags) { return 0; }
int PhysXEnt::SetStateFromTypedSnapshot(TSerialize ser, int type, int flags) { return 0; }
