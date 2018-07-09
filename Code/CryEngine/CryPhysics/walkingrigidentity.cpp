// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"

#include "bvtree.h"
#include "geometry.h"
#include "bvtree.h"
#include "raybv.h"
#include "raygeom.h"
#include "rigidbody.h"
#include "physicalplaceholder.h"
#include "physicalentity.h"
#include "geoman.h"
#include "physicalworld.h"
#include "walkingrigidentity.h"

CWalkingRigidEntity::CWalkingRigidEntity(CPhysicalWorld *pworld, IGeneralMemoryHeap* pHeap) : CRigidEntity(pworld,pHeap) 
{ 
	m_alwaysSweep = 1; 
	m_maxAllowedStep = 0.05f;
}

int CWalkingRigidEntity::AddGeometry(phys_geometry *pgeom, pe_geomparams* params,int id,int bThreadSafe)
{
	ChangeRequest<pe_geomparams> req(this,m_pWorld,params,bThreadSafe,pgeom,id);
	if (req.IsQueued()) {
		WriteLock lock(m_lockPartIdx);
		if (id<0)
			*((int*)req.GetQueuedStruct()-1) = id = m_iLastIdx++;
		else 
			m_iLastIdx = max(m_iLastIdx,id+1);
		return id;
	}
	int res = CPhysicalEntity::AddGeometry(pgeom,params,id);
	if (res>=0)
		RecomputeMassDistribution();
	return res;
}

void CWalkingRigidEntity::RemoveGeometry(int id, int bThreadSafe)
{
	ChangeRequest<void> req(this,m_pWorld,0,bThreadSafe,0,id);
	if (req.IsQueued())
		return;
	int nParts = m_nParts;
	CPhysicalEntity::RemoveGeometry(id);
	if (m_nParts < nParts)
		RecomputeMassDistribution();
}

void CWalkingRigidEntity::RecomputeMassDistribution(int ipart, int bMassChanged)
{
	m_body.M = 0;	m_body.offsfb.zero();
	for(int i=0; i<m_nParts; i++) {
		m_body.offsfb += m_parts[i].pPhysGeomProxy->origin*(m_parts[i].scale*m_parts[i].mass);
		m_body.M += m_parts[i].mass;
	}
	m_body.offsfb *= (m_body.Minv = m_body.M>0 ? 1/m_body.M : 0.0f);
	m_body.qfb.SetIdentity();
	m_body.Ibody.zero(); m_body.Ibody_inv.zero();
}

int CWalkingRigidEntity::SetParams(pe_params *_params, int bThreadSafe)
{
	ChangeRequest<pe_params> req(this,m_pWorld,_params,bThreadSafe);
	if (req.IsQueued())
		return 1+(m_bProcessed>>PENT_QUEUED_BIT);

	if (_params->type==pe_player_dimensions::type_id) {
		pe_player_dimensions *params = (pe_player_dimensions*)_params;
		pe_geomparams gp;	
		capsule caps;	
		caps.center.zero(); caps.axis = ortz;
		if (m_nParts && m_parts[0].id==100) {
			if (is_unused(params->sizeCollider) && is_unused(params->heightCollider))
				return 1;
			gp.pos = m_parts[0].pos;
			box bbox;	m_parts[0].pPhysGeom->pGeom->GetBBox(&bbox);
			caps.r = bbox.size.x; caps.hh = bbox.size.z;
		}	else {
			gp.pos = Vec3(0,0,1);
			caps.r = 0.3f; caps.hh = 0.3f;
		}
		if (!is_unused(params->sizeCollider))	{
			caps.r=params->sizeCollider.x; caps.hh=params->sizeCollider.z;
		}
		IGeometry *pcaps = m_pWorld->CreatePrimitive(capsule::type,&caps);
		if (m_nParts && m_parts[0].id==100) {
			m_parts[0].pPhysGeom->pGeom->Release();
			m_parts[0].pPhysGeom->pGeom = pcaps;
			ComputeBBox(m_BBox);
		}	else {
			phys_geometry *pgeom = m_pWorld->RegisterGeometry(m_pWorld->CreatePrimitive(capsule::type,&caps));
			pgeom->nRefCount = 0;	pcaps->Release();
			if (!is_unused(params->heightCollider))	gp.pos.z = params->heightCollider;
			gp.mass = m_massLegacy;
			gp.surface_idx = m_matidLegacy;
			gp.flags &= ~geom_colltype_ray;
			RemoveGeometry(100);
			AddGeometry(pgeom,&gp,100);
		}
		if (!m_nLegs)
			m_legs = new Vec3[3];
		m_nLegs = 1;
		m_legs[1] = -(m_legs[0] = Vec3(0,0,gp.pos.z-caps.hh-caps.r));
		m_legs[2] = -ortz;
		return 1;
	}
	if (_params->type==pe_player_dynamics::type_id) {
		pe_player_dynamics *params = (pe_player_dynamics*)_params;
		if (!is_unused(params->mass)) {
			pe_simulation_params sp; sp.mass = params->mass;
			SetParams(&sp);
			m_massLegacy = sp.mass;
		}
		if (!is_unused(params->surface_idx)) {
			for(int i=0;i<m_nParts;i++) if (m_parts[i].id==100)
				m_parts[i].surface_idx = params->surface_idx;
			m_matidLegacy = params->surface_idx;
		}
		return 1;
	}
	if (_params->type==pe_params_sensors::type_id) {
		pe_params_sensors *params = (pe_params_sensors*)_params;
		if (params->nSensors < m_nLegs-8) {
			delete[] m_legs; m_legs=nullptr; m_nLegs=0;
		}
		if (params->nSensors > m_nLegs) {
			delete[] m_legs;
			m_legs = new Vec3[params->nSensors*3];
		}
		m_nLegs = params->nSensors;
		for(int i=0; i<m_nLegs; i++) {
			m_legs[i*3] = params->pOrigins[i];
			m_legs[i*3+2] = (m_legs[i*3+1] = params->pDirections[i]).normalized();
		}
		m_pentGround = nullptr;
		m_ilegHit = -1;
		return 1;
	}
	if (_params->type==pe_params_walking_rigid::type_id) {
		pe_params_walking_rigid *params = (pe_params_walking_rigid*)_params;
		if (!is_unused(params->velLegStick)) m_velStick = params->velLegStick;
		if (!is_unused(params->legFriction)) m_friction = params->legFriction;
		if (!is_unused(params->legStiffness)) m_unprojScale = params->legStiffness;
		if (!is_unused(params->legsColltype)) m_colltypeLegs = params->legsColltype;
		if (!is_unused(params->minLegTestMass)) m_minLegsCollMass = params->minLegTestMass;
		return 1;
	}
	if (_params->type==pe_params_flags::type_id) {
		pe_params_flags *params = (pe_params_flags*)_params;
		if (!is_unused(params->flags)) params->flags &= ~pef_parts_traceable;
		params->flagsOR &= ~pef_parts_traceable;
	}
	return CRigidEntity::SetParams(_params);
}

int CWalkingRigidEntity::GetParams(pe_params *_params) const
{
	if (_params->type==pe_player_dynamics::type_id) {
		pe_player_dynamics *params = (pe_player_dynamics*)_params;
		memset(params, 0, sizeof(*params));
		params->gravity = m_gravity;
		return 1;
	}	
	if (_params->type==pe_params_sensors::type_id) {
		pe_params_sensors *params = (pe_params_sensors*)_params;
		params->nSensors = m_nLegs;
		if (!is_unused(params->pOrigins) && params->pOrigins)
			for(int i=0;i<m_nLegs;i++) const_cast<Vec3*>(params->pOrigins)[i] = m_legs[i*3];
		if (!is_unused(params->pDirections) && params->pDirections)
			for(int i=0;i<m_nLegs;i++) const_cast<Vec3*>(params->pDirections)[i] = m_legs[i*3+1];
		return 1;
	}
	if (_params->type==pe_params_walking_rigid::type_id) {
		pe_params_walking_rigid *params = (pe_params_walking_rigid*)_params;
		params->velLegStick = m_velStick;
		params->legFriction = m_friction;
		params->legStiffness = m_unprojScale;
		params->legsColltype = m_colltypeLegs;
		params->minLegTestMass = m_minLegsCollMass;
		return 1;
	}
	return CRigidEntity::GetParams(_params);
}

int CWalkingRigidEntity::GetStatus(pe_status* _status) const
{
	if (_status->type==pe_status_living::type_id) {
		pe_status_living *status = (pe_status_living*)_status;
		memset(status, 0, sizeof(*status));
		status->vel = status->velUnconstrained = m_body.v;
		ReadLock lock(m_lockUpdate);
		if (status->pGroundCollider = m_pentGround) {
			QuatTS qts; m_pentGround->GetPartTransform(m_ipartGround, qts.t,qts.q,qts.s, this);
			RigidBody *pbody = m_pentGround->GetRigidBody(m_ipartGround);
			Vec3 pt = qts*m_ptlocGround;
			status->groundSlope = qts.q*m_nlocGround;
			status->groundHeight = pt.z;
			status->groundSurfaceIdx=status->groundSurfaceIdxAux = m_matidGround;
			status->velGround = pbody->v + (pbody->w ^ pt-pbody->pos);
		}	else 
			status->bFlying = 1;
		return 1;
	}
	if (_status->type==pe_status_sensors::type_id) {
		pe_status_sensors *status = (pe_status_sensors*)_status;
		ReadLock lock(m_lockUpdate);
		if ((unsigned int)m_ilegHit>=32u)
			return 0;
		status->flags = 1<<m_ilegHit;
		QuatTS qts; m_pentGround->GetPartTransform(m_ipartGround, qts.t,qts.q,qts.s, this);
		status->pPoints[m_ilegHit] = qts*m_ptlocGround;
		return 1;
	}
	return CRigidEntity::GetStatus(_status);
}

int CWalkingRigidEntity::Step(float dt) 
{ 
	m_nCollEnts=-1; m_moveDist=0; m_Eaux=0; 
	m_sweepGap = -m_pWorld->m_vars.maxContactGapPlayer;
	return CRigidEntity::Step(dt); 
}

bool CWalkingRigidEntity::OnSweepHit(geom_contact &cnt, int icnt, float &dt, Vec3 &vel, int &nsweeps)
{
	m_pos = m_posNew;
	if (!m_pentGround) {
		int iCaller = get_iCaller_int();
		RigidBody *pbody = g_CurColliders[icnt]->GetRigidBody(g_CurCollParts[icnt][1]);
		Matrix33 K = Diag33(m_body.Minv);	
		pbody->GetContactMatrix(cnt.pt-pbody->pos, K);
		pe_action_impulse ai;
		ai.point = cnt.pt;
		ai.impulse = cnt.n*((cnt.n*(m_body.v-pbody->v-(pbody->w ^ cnt.pt-pbody->pos)))/max(1e-6f,cnt.n*K*cnt.n));
		g_CurColliders[icnt]->Action(&ai);
		m_body.v = (m_body.P-=ai.impulse)*m_body.Minv;
	}
	float move = max(0.0f, (float)cnt.t+m_sweepGap);
	if (nsweeps==1 && m_moveDist+move==0)
		dt = 0;
	else
		dt = max(0.0f, dt-(move-m_sweepGap*1.2f)/vel.len());
	m_moveDist += move;
	vel -= cnt.n*(vel*cnt.n-m_sweepGap);
	if (++nsweeps>=3 || vel.len2()*sqr(dt)<sqr(m_sweepGap)*0.01f || vel*m_body.v<=0)
		return false;
	m_posNew += vel*dt; m_body.pos += vel*dt;
	return true;
}

int CWalkingRigidEntity::GetPotentialColliders(CPhysicalEntity **&pentlist, float dt)
{
	if (m_nCollEnts<0) {
		for(int i=0,j;i<m_nLegs;i++) 
			for(Vec3 pt=m_legs[j=0,i*3]; j<2; pt+=m_legs[i*3+ ++j]) {
				Vec3 ptG = m_posNew + m_qNew*pt;
				m_BBoxNew[0] = min(m_BBoxNew[0], ptG);
				m_BBoxNew[1] = max(m_BBoxNew[1], ptG);
			}
		m_nCollEnts = CRigidEntity::GetPotentialColliders(m_pCollEnts, dt);
	}
	pentlist = m_pCollEnts;
	return m_nCollEnts;
}

void CWalkingRigidEntity::CheckAdditionalGeometry(float dt)
{
	int iCaller = get_iCaller_int();
	CPhysicalEntity **pents;
	int nents = GetPotentialColliders(pents);
	CRayGeom aray;
	geom_world_data gwd[2];
	float thit = 1e10f, stickDist = m_velStick*dt;
	CPhysicalEntity *pentGround = nullptr;
	Vec3 ptlocGround(ZERO), nlocGround(ZERO);
	int ipartGround, matidGround, ilegHit = -1;
	gwd[1].R = Matrix33(m_qNew);
	gwd[1].offset = m_posNew;
	m_Eaux = 0;

	for(int ileg=0; ileg<m_nLegs; ileg++) {
		aray.m_ray.origin = m_legs[ileg*3];
		aray.m_ray.dir = m_legs[ileg*3+1] + (aray.m_dirn = m_legs[ileg*3+2])*stickDist;
		for(int i=0; i<nents; i++) if (pents[i]->GetMassInv()*m_minLegsCollMass <= 1.0f)
			for(int j,j1=pents[i]->GetUsedPartsCount(iCaller)-1; j1>=0; j1--)
				if (pents[i]->m_parts[j = pents[i]->GetUsedPart(iCaller,j1)].flags & m_colltypeLegs) {
					const geom &part = pents[i]->m_parts[j];
					pents[i]->GetPartTransform(j, gwd[0].offset,gwd[0].R,gwd[0].scale, this);
					geom_contact *pcont;
					if (int ncont = part.pPhysGeomProxy->pGeom->Intersect(&aray, gwd,gwd+1, 0, pcont)) 
						if (pcont[ncont-1].t<thit && pcont[ncont-1].n*(m_qNew*aray.m_dirn)<0 && pents[i]->GetMassInv()<=(pentGround ? pentGround->GetMassInv() : 1e20f)) {
							thit = pcont[ncont-1].t; ilegHit = ileg;
							m_Eaux = m_body.M*sqr((m_legs[ileg*3+1]*m_legs[ileg*3+2]-thit)*m_unprojScale);
							pentGround = pents[i]; ipartGround = j;
							ptlocGround = (pcont[ncont-1].pt-gwd[0].offset)*gwd[0].R;
							nlocGround = pcont[ncont-1].n*gwd[0].R;
							matidGround = pents[i]->GetMatId(pcont[ncont-1].id[1], j);
						}
				}
	}
	if (pentGround!=m_pentGround) {
		if (m_pentGround && !HasContactsWith(m_pentGround) && !m_pentGround->HasContactsWith(this))
			RemoveCollider(m_pentGround);
		if (pentGround && pentGround->GetMassInv()>0)
			AddCollider(pentGround);
	}
	{ WriteLock lock(m_lockUpdate);
		m_pentGround = pentGround;
		m_distHit=thit; m_ilegHit=ilegHit; m_ipartGround=ipartGround;
		m_ptlocGround=ptlocGround; m_nlocGround=nlocGround;	m_matidGround=matidGround;
	}
}

int CWalkingRigidEntity::RegisterContacts(float dt, int nMaxPlaneContacts)
{
	if (m_pentGround) 
		if (m_nContacts || m_constraintMask || m_friction>0 || m_pentGround->GetMassInv()>0) {
			entity_contact *pcont = (entity_contact*)AllocSolverTmpBuf(sizeof(entity_contact));
			pcont->pent[0] = this; pcont->ipart[0] = -1; pcont->pbody[0] = &m_body;
			pcont->pbody[1] = (pcont->pent[1] = m_pentGround)->GetRigidBody(pcont->ipart[1] = m_ipartGround);
			QuatTS qts; m_pentGround->GetPartTransform(m_ipartGround, qts.t,qts.q,qts.s, this);
			pcont->pt[0]=pcont->pt[1] = QuatT(qts)*m_ptlocGround;
			pcont->n = qts.q*m_nlocGround;
			pcont->friction = m_friction;
			pcont->flags = 0;
			pcont->penetration = m_legs[m_ilegHit*3+1]*m_legs[m_ilegHit*3+2]-m_distHit;
			pcont->vreq = m_qNew*m_legs[m_ilegHit*3+2]*(-pcont->penetration*m_unprojScale);
			::RegisterContact(pcont);
		}	else {
			float diff = m_legs[m_ilegHit*3+1]*m_legs[m_ilegHit*3+2]-m_distHit;
			Vec3 leg = m_qNew*-m_legs[m_ilegHit*3+2];
			m_body.v -= (m_nColliders ? m_gravity : m_gravityFreefall)*dt;
			m_body.v += leg*(diff*m_unprojScale-m_body.v*leg);
			m_body.P = m_body.v*m_body.M;
		}
	return CRigidEntity::RegisterContacts(dt, nMaxPlaneContacts);	
}

void CWalkingRigidEntity::GetMemoryStatistics(ICrySizer *pSizer) const
{
	if (GetType()==PE_WALKINGRIGID)
		pSizer->AddObject(this, sizeof(CWalkingRigidEntity));
	CRigidEntity::GetMemoryStatistics(pSizer);
	pSizer->AddObject(m_legs, m_nLegs*sizeof(m_legs[0])*3);
}