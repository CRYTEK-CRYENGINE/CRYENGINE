// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"

#include "geometries.h"
#include "entities.h"
#include "world.h"

#include <CryMath/Random.h>

using namespace primitives;
using namespace cpx;
using namespace Helper;


PhysXCloth::PhysXCloth(int id, pe_params* params) : PhysXEnt(PE_SOFT,id,params) 
{
	m_actor->setActorFlag(PxActorFlag::eDISABLE_SIMULATION, true);
}

void PhysXCloth::Enable(bool enable)
{
	PhysXEnt::Enable(enable);
	if (m_cloth)
		if (enable)	{
			g_cryPhysX.Scene()->addActor(*m_cloth);
			InsertBefore(this, g_pPhysWorld->m_auxEntList[1]); 
		} else {
			g_cryPhysX.Scene()->removeActor(*m_cloth);
			if (!(m_mask & 0x18)) {
				WriteLock lock(g_lockEntList);
				RemoveFromList(this);
			}
		}
}

void PhysXCloth::getBBox(Vec3 *BBox) const
{
	if (m_cloth) {
		PxBounds3 bbox = m_cloth->getWorldBounds();
		BBox[0] = V(bbox.minimum); BBox[1] = V(bbox.maximum);
	}	else
		PhysXEnt::getBBox(BBox);
}

void PhysXCloth::setGlobalPose(const QuatT& pose, int components)
{
	PhysXEnt::setGlobalPose(pose,components);
	if (m_cloth)
		m_cloth->setGlobalPose(T(pose));
}

void PhysXCloth::PostStep(float dt, int immediate)
{
	if (immediate && m_cloth) {
		PxBounds3 bbox = m_cloth->getWorldBounds();
		QuatT worldInv = T(m_actor->getGlobalPose()).GetInverted();
		PxShape *shape = m_parts[0].shape;
		PxMaterial *mtl; shape->getMaterials(&mtl,1);
		PxFilterData fd = shape->getQueryFilterData();
		if (m_parts.size()<2)	{
			m_parts.resize(2);
			m_parts[1].geom = m_parts[0].geom;
			shape->setQueryFilterData(PxFilterData(0,fd.word1,fd.word2,fd.word3));
		} else
			m_actor->detachShape(*m_parts[1].shape);
		m_parts[1].shape = m_actor->createShape(PxBoxGeometry(bbox.getExtents()+PxVec3(0.01f)), *mtl, PxShapeFlag::eSCENE_QUERY_SHAPE);
		m_parts[1].shape->setLocalPose(T(QuatT(worldInv*V(bbox.getCenter()), worldInv.q)));
		fd.word0 = geom_colltype_ray;
		m_parts[1].shape->setQueryFilterData(fd);
		m_parts[1].shape->setSimulationFilterData(shape->getSimulationFilterData());
		m_parts[1].shape->userData = (void*)(INT_PTR)-1;

		if (m_host)	{
			QuatT attachPartPose = m_host->getLocalPose(m_host->idxPart(m_idpartHost));
			m_cloth->setTargetPose(T((m_lastHostPose = m_host->getGlobalPose()*attachPartPose)*m_poseInHost));
			QuatT poseInHostInv = (attachPartPose*m_poseInHost).GetInverted();
			PxClothCollisionSphere *sphBuf = (PxClothCollisionSphere*)alloca(m_host->m_parts.size()*2*sizeof(sphBuf[0]));
			int nsph=0, ncaps=0, *capsBuf = (int*)alloca(m_host->m_parts.size()*sizeof(int));
			PxCapsuleGeometry caps;
			PxSphereGeometry sph;
			for(int i=0;i<m_host->m_parts.size();i++)	{
				QuatT partPose = T(m_host->m_parts[i].shape->getLocalPose());
				if (m_host->m_parts[i].shape->getSphereGeometry(sph)) {
					sphBuf[nsph].pos = V(poseInHostInv*partPose.t);
					sphBuf[nsph++].radius = sph.radius;
				}	else if (m_host->m_parts[i].shape->getCapsuleGeometry(caps)) {
					capsBuf[ncaps++] = nsph;
					for(int j=0;j<2;j++) {
						sphBuf[nsph].pos = V(poseInHostInv*(partPose.t+partPose.q*Vec3(caps.halfHeight*(j*2-1),0,0)));
						sphBuf[nsph++].radius = caps.radius;
					}
				}
			}
			m_cloth->setCollisionSpheres(sphBuf, nsph);
			//for(int i=m_cloth->getNbCollisionCapsules()-1; i>=0; i--)	
			//	m_cloth->removeCollisionCapsule(i);	// Px crashes
			if (!m_cloth->getNbCollisionCapsules())	for(int i=0; i<ncaps; i++)
				m_cloth->addCollisionCapsule(capsBuf[i],capsBuf[i]+1);
		}

		ReadLock lock(m_lockAttach);
		if (!m_attach.empty()) {
			m_constr.resize(m_cloth->getNbParticles());
			for(int i=0;i<m_constr.size();i++) {
				m_constr[i].pos = PxVec3(0);
				m_constr[i].radius = FLT_MAX;
			}
			QuatT gposeInv = getGlobalPose().GetInverted(), poseRel;
			for(int i=0; i<m_attach.size(); i++) {
				if (i==0 || m_attach[i].ent!=m_attach[i-1].ent)
					poseRel = gposeInv*m_attach[i].ent->getGlobalPose();
				m_constr[m_attach[i].idx].pos = V(poseRel*m_attach[i].ptrel);
				m_constr[m_attach[i].idx].radius = 0;
			}
			m_cloth->setMotionConstraints(&m_constr[0]);
		}	else
			m_cloth->setMotionConstraints(nullptr);

		if ((m_windTimer+=dt*4)>1) {
			m_wind[0] = m_wind[1];
			Vec3 w=m_wind[2]+g_pPhysWorld->m_wind, wrange(w.len()*m_windVariance), wmin = w-wrange, wmax = w+wrange;
			m_wind[1] = Vec3(cry_random(wmin.x,wmax.x), cry_random(wmin.y,wmax.y), cry_random(wmin.z,wmax.z));
			m_windTimer -= (int)m_windTimer;
		}
		m_cloth->setExternalAcceleration(V(m_wind[0]*(1-m_windTimer)+m_wind[1]*m_windTimer));
	}
	PhysXEnt::PostStep(dt,immediate);
}

int PhysXCloth::RefineRayHit(const Vec3& rayOrg, const Vec3& rayDir, Vec3 &ptRet, Vec3& nRet, int partid) const
{
	if (m_cloth) {
		PxClothParticleData *vtx = m_cloth->lockParticleData(PxDataAccessFlag::eREADABLE);
		const mesh_data& md = *(mesh_data*)m_parts[0].geom->pGeom->GetData();
		QuatT gpose = getGlobalPose();
		float mindist = 1;
		for(int i=0,j; i<md.nTris; i++) {
			Vec3 pt[3]; for(j=0;j<3;j++) pt[j] = gpose*V(vtx->particles[md.pIndices[i*3+j]].pos);
			Vec3 n = pt[1]-pt[0] ^ pt[2]-pt[0];
			float tx=(pt[0]-rayOrg)*n, ty=rayDir*n;
			if (inrange(tx,0.0f,ty)) {
				Vec3 ptHit = rayOrg*ty + rayDir*tx;
				for(j=0; j<3 && (n*(pt[incm3(j)]-pt[j] ^ ptHit-pt[j]*ty))*ty>0; j++);
				if (j==3 && tx*ty<mindist*ty*ty) {
					ptRet = rayOrg+rayDir*(mindist=tx/ty);
					nRet = n.GetNormalized();
					partid = md.pIndices[i*3+idxmin3(Vec3((pt[0]-ptRet).len2(),(pt[1]-ptRet).len2(),(pt[2]-ptRet).len2()))];
				}
			}
		}
		vtx->unlock();	
	}
	return partid;
}

int PhysXCloth::AddGeometry(phys_geometry* pgeom, pe_geomparams* params, int id, int bThreadSafe)
{
	if (pgeom->pGeom->GetType()!=GEOM_TRIMESH)
		return -1;
	m_mass += max(0.0f, params->mass);
	int res = PhysXEnt::AddGeometry(pgeom, params, id, bThreadSafe);
	if (res>=0) {
		PxFilterData fd = m_parts.back().shape->getQueryFilterData();
		m_parts.back().shape->setQueryFilterData(PxFilterData(geom_colltype_ray,0,fd.word2,fd.word3));
	}
	return res;
}

void PhysXCloth::ApplyParams()
{
	if (m_cloth) {
		m_cloth->setFrictionCoefficient(m_friction);
		m_cloth->setSleepLinearVelocity(m_sleepSpeed);
		m_cloth->setSolverFrequency(1/m_maxTimeStep);
		m_cloth->setContactOffset(m_thickness);
		m_cloth->setRestOffset(m_thickness);
		m_cloth->setDampingCoefficient(PxVec3(m_damping));
		m_cloth->setInertiaScale(1-m_hostSpaceSim);
		PxFilterData fd = m_parts[0].shape->getSimulationFilterData();
		fd.word3 |= 32;
		m_cloth->setSimulationFilterData(fd);
		m_cloth->setClothFlag(PxClothFlag::eSCENE_COLLISION, !!(m_collTypes & (ent_rigid|ent_static)));
	}
}

void PhysXCloth::Attach(int *piVtx, int nPoints)
{
	mesh_data *md = (mesh_data*)m_parts[0].geom->pGeom->GetData();
	PxClothParticle *vtx = (PxClothParticle*)alloca(md->nVertices*sizeof(PxClothParticle));
	float invmass = md->nVertices/max(0.01f,m_mass);
	for(int i=0; i<md->nVertices; i++) {
		vtx[i].pos = V(md->pVertices[i]);
		vtx[i].invWeight = invmass;
	}
	for(int i=0; i<nPoints; vtx[piVtx[i++]].invWeight=0);
		
	if (!m_parts[0].geom->pForeignData) {
		PxClothMeshDesc cmd;
		cmd.points.data = &vtx[0].pos;
		cmd.points.stride = cmd.invMasses.stride = sizeof(vtx[0]);
		cmd.points.count = cmd.invMasses.count = md->nVertices;
		cmd.flags = PxMeshFlags(0);
		cmd.triangles.data = md->pIndices;
		cmd.triangles.stride = 3*sizeof(index_t);
		cmd.triangles.count = md->nTris;
		cmd.invMasses.data = &vtx[0].invWeight;
		m_parts[0].geom->pForeignData = PxClothFabricCreate(*g_cryPhysX.Physics(), cmd, V(g_pPhysWorld->m_vars.gravity), false);
	}
	m_cloth = g_cryPhysX.Physics()->createCloth(m_actor->getGlobalPose(), *(PxClothFabric*)m_parts[0].geom->pForeignData, vtx, PxClothFlag::eSCENE_COLLISION);
	m_normal = (md->pVertices[md->pIndices[1]]-md->pVertices[md->pIndices[0]] ^ md->pVertices[md->pIndices[2]]-md->pVertices[md->pIndices[0]]).normalized();
}

void PhysXCloth::DeferredAttachAndCreate()
{
	if (!m_ivtxAttachAccum.empty()) {
		Attach(&m_ivtxAttachAccum[0], m_ivtxAttachAccum.size());
		m_ivtxAttachAccum.clear();
		ApplyParams();
		MarkForAdd();
	}
}

int PhysXCloth::SetParams(pe_params* _params, int bThreadSafe)
{
	DeferredAttachAndCreate();
	if (_params->type==pe_params_softbody::type_id) {
		pe_params_softbody *params = (pe_params_softbody*)_params;
		if (!is_unused(params->friction)) m_friction = params->friction;
		if (!is_unused(params->thickness)) m_thickness = params->thickness;
		if (!is_unused(params->hostSpaceSim)) m_hostSpaceSim = params->hostSpaceSim;
		if (!is_unused(params->collTypes)) m_collTypes = params->collTypes;
		if (!is_unused(params->wind)) m_wind[2] = params->wind;
		if (!is_unused(params->windVariance)) m_windVariance = params->windVariance;
		ApplyParams();
		return 1;
	}
	if (_params->type==pe_simulation_params::type_id && m_cloth) {
		pe_simulation_params *params = (pe_simulation_params*)_params;
		if (!is_unused(params->minEnergy)) m_sleepSpeed = sqrt(params->minEnergy);
		if (!is_unused(params->maxTimeStep)) m_maxTimeStep = params->maxTimeStep;
		if (!is_unused(params->damping)) m_damping = params->damping;
		ApplyParams();
		return 1;
	}
	if (_params->type==pe_params_part::type_id) {
		return 1;
	}
	return PhysXEnt::SetParams(_params,bThreadSafe);
}

int PhysXCloth::Action(pe_action* _action, int bThreadSafe)
{
	if (_action->type==pe_action_attach_points::type_id && m_parts.size()) {
		pe_action_attach_points *action = (pe_action_attach_points*)_action;
		bool deferred = !m_cloth && action->nPoints==1 && action->pEntity!=WORLD_ENTITY && action->pEntity && (!m_host || (PhysXEnt*)action->pEntity==m_host && action->partid==m_idpartHost);
		if (!m_cloth && !m_host && action->pEntity!=WORLD_ENTITY && action->pEntity->GetType()!=PE_STATIC) {
			m_host = (PhysXEnt*)action->pEntity;
			int ipart = m_host->idxPart(m_idpartHost = action->partid);
			if (ipart>=0)
				m_poseInHost = (m_host->getGlobalPose()*m_host->getLocalPose(ipart)).GetInverted()*getGlobalPose();
		}
		if (deferred) {
			m_ivtxAttachAccum.push_back(action->piVtx[0]); 
			return 1;
		}
		DeferredAttachAndCreate();
		if (m_cloth) {
			WriteLock lock(m_lockAttach);
			mesh_data *md = (mesh_data*)m_parts[0].geom->pGeom->GetData();
			PxClothParticleData *vtx = m_cloth->lockParticleData(PxDataAccessFlag::eREADABLE);
			PhysXEnt *entAttach = (PhysXEnt*)action->pEntity;
			QuatT gpose = getGlobalPose(), gposeAtt = entAttach->getGlobalPose();
			Vec3 BBox[2]; entAttach->getBBox(BBox);
			Vec3 center = (BBox[1]+BBox[0])*0.5f, size = (BBox[1]-BBox[0])*0.5f, pt;
			if (action->nPoints<0) {
				for(int i=m_cloth->getNbParticles()-1; i>=0; i--)
					if (max(Vec3(0), ((pt=gpose*V(vtx->particles[i].pos))-center).abs()-size)*Vec3(1) < 1e-7f)
						m_attach.push_back({ i, entAttach, gposeAtt.GetInverted()*pt });
			} else if (action->nPoints>0) for(int i=0;i<action->nPoints;i++)
				m_attach.push_back({ action->piVtx[i], entAttach, gposeAtt.GetInverted()*(gpose*V(vtx->particles[action->piVtx[i]].pos)) });
			else
				m_attach.erase(std::remove_if(m_attach.begin(),m_attach.end(), [entAttach](auto att) { return att.ent==entAttach; }), m_attach.end());
			vtx->unlock();
		} else {
			Attach(action->piVtx,action->nPoints);
			ApplyParams();
			MarkForAdd();
		}
		return 1;
	}

	if (_action->type==pe_action_reset::type_id && m_cloth) {
		PxClothParticleData *vtxBuf = m_cloth->lockParticleData(PxDataAccessFlag::eWRITABLE);
		if (!vtxBuf || !vtxBuf->particles)
			return 0;
		mesh_data *md = (mesh_data*)m_parts[0].geom->pGeom->GetData();
		for(int i=min(md->nVertices,m_cloth->getNbParticles())-1; i>=0; i--)
			vtxBuf->particles[i].pos = V(md->pVertices[i]);
		vtxBuf->unlock();
		PostStep(0,0);
		return 1;
	}

	return PhysXEnt::Action(_action,bThreadSafe);
}

int PhysXCloth::GetStatus(pe_status* _status) const
{
	if (_status->type==pe_status_softvtx::type_id && m_cloth) {
		pe_status_softvtx *status = (pe_status_softvtx*)_status;
		status->nVtx = m_cloth->getNbParticles();
		PxClothParticleData *vtxBuf = m_cloth->lockParticleData(PxDataAccessFlag::eREADABLE);
		if (!vtxBuf->particles)
			return 0;
		strided_pointer<Vec3> pVtx = strided_pointer<Vec3>((Vec3*)&vtxBuf->particles->pos, sizeof(vtxBuf->particles[0]));
		m_vtx.resize(status->nVtx);
		for(int i=0;i<status->nVtx;i++)
			m_vtx[i] = pVtx[i];
		status->pVtx = strided_pointer<Vec3>(&m_vtx[0]);
		status->pNormals = strided_pointer<Vec3>((Vec3*)&m_normal, 0);
		vtxBuf->unlock();
		QuatT pose = getGlobalPose();
		status->pos = pose.t;
		status->q = pose.q;
		status->posHost = m_lastHostPose.t-status->pos;
		status->qHost = m_lastHostPose.q;
		return 1;
	}
	return PhysXEnt::GetStatus(_status);
}
