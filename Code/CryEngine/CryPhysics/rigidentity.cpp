// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"

#include "bvtree.h"
#include "geometry.h"
#include "bvtree.h"
#include "singleboxtree.h"
#include "boxgeom.h"
#include "raybv.h"
#include "raygeom.h"
#include "rigidbody.h"
#include "physicalplaceholder.h"
#include "physicalentity.h"
#include "geoman.h"
#include "physicalworld.h"
#include "rigidentity.h"
#include "waterman.h"

#if USE_IMPROVED_RIGID_ENTITY_SYNCHRONISATION
#include <CryRenderer/IRenderer.h>
#include <CryRenderer/IRenderAuxGeom.h>
#endif

#if USE_IMPROVED_RIGID_ENTITY_SYNCHRONISATION
#define MAX_SEQUENCE_NUMBER 256
#endif

inline int FrameOwner(const entity_contact &cnt) { return isneg(cnt.pbody[1]->Minv-cnt.pbody[0]->Minv) & cnt.pent[1]->m_iSimClass-3>>31; }

inline void GetContactFrames(const entity_contact &cnt, QuatT* frames, int n=2)	{ 
	float scale;
	for (int i=0;i<n;i++) 
		cnt.pent[i]->GetPartTransform(cnt.ipart[i], frames[i].t,frames[i].q,scale, cnt.pent[0]);
}

CRigidEntity::CRigidEntity(CPhysicalWorld *pWorld, IGeneralMemoryHeap* pHeap)
	: CPhysicalEntity(pWorld, pHeap)
	, m_posNew(ZERO)
	, m_qNew(IDENTITY)
	, m_iVarPart0(1000000)
	, m_bCollisionCulling(0)
	, m_bJustLoaded(0)
	, m_bStable(0)
	, m_bHadSeverePenetration(0)
	, m_bSteppedBack(0)
	, m_nStepBackCount(0)
	, m_bCanSweep(1)
	, m_nNonRigidNeighbours(0)
	, m_bFloating(0)
	, m_bDisablePreCG(0)
	, m_bSmallAndFastForced(0)
	, m_bAwake(1)
	, m_nSleepFrames(0)
	, m_nFutileUnprojFrames(0)
	, m_nEvents(0)
	, m_nMaxEvents(0)
	, m_icollMin(0)
	, m_alwaysSweep(0)
	, m_pColliderContacts(nullptr)
	, m_pColliderConstraints(nullptr)
	, m_nContacts(0)
	, m_pConstraints(nullptr)
	, m_pConstraintInfos(nullptr)
	, m_nConstraintsAlloc(0)
	, m_constraintMask(0)
	, m_nRestMask(0)
	, m_nPrevColliders(0)
	, m_collTypes(ent_terrain | ent_static | ent_sleeping_rigid | ent_rigid | ent_living | ent_independent)
	, m_velFastDir(0.0f)
	, m_sizeFastDir(0.0f)
	, m_timeStepFull(0.01f)
	, m_timeStepPerformed(0.0f)
	, m_lastTimeStep(0.0f)
	, m_minAwakeTime(0.0f)
	, m_nextTimeStep(0.0f)
	, m_gravity(0.0f, 0.0f, -9.81f)
	, m_Emin(sqr(0.07f))
	, m_maxAllowedStep(0.02f)
	, m_vAccum(ZERO)
	, m_wAccum(ZERO)
	, m_damping(0.0f)
	, m_dampingFreefall(0.0f)
	, m_dampingEx(0.0f)
	, m_maxw(20.0f)
	, m_minFriction(0.1f)
	, m_maxFriction(100.0f)
	, m_vSleep(ZERO)
	, m_wSleep(ZERO)
	, m_pStableContact(nullptr)
	, m_Pext(ZERO)
	, m_Lext(ZERO)
	, m_prevv(ZERO)
	, m_prevw(ZERO)
	, m_E0(0.0f)
	, m_Estep(0.0f)
	, m_timeCanopyFallen(0.0f)
	, m_bCanopyContact(0)
	, m_nCanopyContactsLost(0)
	, m_Psoft(ZERO)
	, m_Lsoft(ZERO)
	, m_forcedMove(ZERO)
	, m_pEventsColl(nullptr)
	, m_iLastLogColl(-2)
	, m_vcollMin(1E10f)
	, m_iLastLog(-2)
	, m_pEvent(nullptr)
	, m_waterDamping(0.0f)
	, m_kwaterDensity(1.0f)
	, m_kwaterResistance(1.0f)
	, m_EminWater(sqr(0.01f))
	, m_submergedFraction(0.0f)
	, m_iLastConstraintIdx(1)
	, m_lockConstraintIdx(0)
	, m_lockContacts(0)
	, m_lockStep(0)
	, m_iLastChecksum(0)
	, m_sweepGap(m_pWorld ? m_pWorld->m_vars.maxContactGap*0.5f : 0.01f)
#if USE_IMPROVED_RIGID_ENTITY_SYNCHRONISATION
	, m_pNetStateHistory(nullptr)
	, m_sequenceOffset(0)
	, m_hasAuthority(false)
#endif
{	 
	static_assert(CRY_ARRAY_COUNT(m_BBoxNew) == 2, "Invalid array size!");
	m_BBoxNew[0].zero();
	m_BBoxNew[1].zero();

	m_pContactStart = m_pContactEnd = CONTACT_END(m_pContactStart);

	if (pWorld)
		m_gravity = pWorld->m_vars.gravity;

	m_gravityFreefall = m_gravity;

	ZeroArray(m_checksums);


	//CPhysicalPlaceholder
	m_iSimClass=2; 
	
	//CPhysicalEntity
	m_pNewCoords = (coord_block*)&m_posNew;
	m_lockColliders = 0;
	m_flags &= ~pef_never_affect_triggers;
}

CRigidEntity::~CRigidEntity()
{
	if (m_pColliderContacts) delete[] m_pColliderContacts;
	entity_contact *pContact,*pContactNext;
	for(pContact=m_pContactStart; pContact!=CONTACT_END(m_pContactStart); pContact=pContactNext) {
		pContactNext=pContact->next; m_pWorld->FreeContact(pContact);
	}
	if (m_pColliderConstraints) delete[] m_pColliderConstraints;
	if (m_pConstraints) delete[] m_pConstraints;
	if (m_pConstraintInfos) delete[] m_pConstraintInfos;
	if (m_pEventsColl) delete[] m_pEventsColl;
#if USE_IMPROVED_RIGID_ENTITY_SYNCHRONISATION
	if (m_pNetStateHistory) delete m_pNetStateHistory;
#endif
}


void CRigidEntity::AttachContact(entity_contact *pContact, int i, CPhysicalEntity *pCollider) 
{	// register new contact as the first contact for the collider i
	ReadLock lockc(m_lockColliders);
	WriteLock lock(m_lockContacts);
	if (m_pColliders[i]!=pCollider) {
		for(i=0; i<m_nColliders && m_pColliders[i]!=pCollider; i++);
		if (i==m_nColliders)
			return;
	}
	if (m_pColliderContacts[i]==0) {
		m_pColliderContacts[i] = m_pContactStart;
		pContact->flags = contact_last;
	}	else 
		pContact->flags = 0;
	pContact->prev = m_pColliderContacts[i]->prev; pContact->next = m_pColliderContacts[i];
	m_pColliderContacts[i]->prev->next = pContact; m_pColliderContacts[i]->prev = pContact; m_pColliderContacts[i] = pContact;
	m_nContacts++;
}

void CRigidEntity::DetachContact(entity_contact *pContact, int i,int bCheckIfEmpty,int bAcquireContactLock) 
{ // detach contact from the contact list
	CPhysicalEntity *pCollider=0; {
	int bCollidersLocked = 0;
	WriteLockCond lock(m_lockContacts,bAcquireContactLock);
	pContact->prev->next = pContact->next;
	pContact->next->prev = pContact->prev;
	if (i<0) {
		ReadLockCond lockc(m_lockColliders,1); 
		bCollidersLocked = sizeof(lockc)==sizeof(ReadLockCond);	// only lock if platf0 is not defined to be an empty stub
		lockc.SetActive(0);	// lock now, but release later, outside the if scope
		for(i=0; i<m_nColliders && m_pColliders[i]!=pContact->pent[1]; i++);
	}
	if (m_pColliderContacts[i]!=pContact)
		pContact->prev->flags |= pContact->flags & contact_last;
	else {
		m_pColliderContacts[i] = (pContact->flags & contact_last) ? 0:pContact->next;
		if (bCheckIfEmpty && !HasContactsWith(m_pColliders[i]) && !m_pColliders[i]->HasContactsWith(this))
			pCollider = m_pColliders[i]; 
	}
	m_nContacts--;
	AtomicAdd(&m_lockColliders, -bCollidersLocked);	}
	if (pCollider)
		RemoveCollider(pCollider);
}

void CRigidEntity::DetachAllContacts()
{
	{ WriteLock lock(m_lockContacts);
		entity_contact *pContact,*pContactNext;
		for(pContact=m_pContactStart; pContact!=CONTACT_END(m_pContactStart); pContact=pContactNext) {
			pContactNext=pContact->next; m_pWorld->FreeContact(pContact);
		}
		m_nContacts = 0;
		m_pContactStart = m_pContactEnd = CONTACT_END(m_pContactStart);
	}
	for(int i=m_nColliders-1;i>=0;i--) {
		m_pColliderContacts[i] = 0;
		if (!m_pColliderConstraints[i] && !m_pColliders[i]->HasConstraintContactsWith(this))
			RemoveCollider(m_pColliders[i]);
	}
}

void CRigidEntity::DetachPartContacts(int ipart,int iop0, CPhysicalEntity *pent,int iop1, int bCheckIfEmpty) 
{
	WriteLock lock(m_lockContacts);
	entity_contact *pContact,*pContactNext;
	for(pContact=m_pContactStart; pContact!=CONTACT_END(m_pContactStart); pContact=pContactNext) {
		pContactNext = pContact->next;
		if (pContact->ipart[iop0]==ipart && pContact->pent[iop1]==pent) 
			DetachContact(pContact,-1,bCheckIfEmpty, 0), m_pWorld->FreeContact(pContact);
	}
}

void CRigidEntity::MoveConstrainedObjects(const Vec3 &dpos, const quaternionf &dq)
{
	pe_params_pos pp;
	for(int i=0;i<m_nColliders;i++) 
		if ((unsigned int)m_pColliders[i]->m_iSimClass-1u<2u && 
				HasConstraintContactsWith(m_pColliders[i],constraint_inactive|constraint_rope|constraint_no_enforcement) || 
				m_pColliders[i]->HasConstraintContactsWith(this,constraint_inactive|constraint_rope|constraint_no_enforcement)) 
	{
		pp.pos = m_pos+dq*(m_pColliders[i]->m_pos-m_pos+dpos);
		pp.q = dq*m_pColliders[i]->m_qrot;
		pp.bRecalcBounds = 16|2;
		m_pColliders[i]->SetParams(&pp);
	}
}


int CRigidEntity::AddGeometry(phys_geometry *pgeom, pe_geomparams* params,int id, int bThreadSafe)
{
	if (pgeom && fabs_tpl(params->mass)+fabs_tpl(params->density)!=0 && (pgeom->V<=0 || pgeom->Ibody.x<0 || pgeom->Ibody.y<0 || pgeom->Ibody.z<0))	{
		char errmsg[256];
		cry_sprintf(errmsg,"CRigidEntity::AddGeometry: (%s at %.1f,%.1f,%.1f) Trying to add bad geometry",
			m_pWorld->m_pRenderer ? m_pWorld->m_pRenderer->GetForeignName(m_pForeignData,m_iForeignData,m_iForeignFlags):"", m_pos.x,m_pos.y,m_pos.z);
		VALIDATOR_LOG(m_pWorld->m_pLog,errmsg);
		return -1;
	}

	if (!pgeom)
		return -1;
	ChangeRequest<pe_geomparams> req(this,m_pWorld,params,bThreadSafe,pgeom,id);
	if (req.IsQueued()) {
		WriteLock lock(m_lockPartIdx);
		if (id<0)
			*((int*)req.GetQueuedStruct()-1) = id = m_iLastIdx++;
		else 
			m_iLastIdx = max(m_iLastIdx,id+1);
		return id;
	}

	int res,nParts=m_nParts;
	if ((res=CPhysicalEntity::AddGeometry(pgeom,params,id,1))<0 || m_nParts==nParts)
		return res;

	int i = m_nParts-1;
	pgeom = m_parts[i].pPhysGeom;
	float V=pgeom->V*cube(params->scale), M=params->mass>0 ? params->mass : params->density*V;
	Vec3 bodypos = m_pos + m_qrot*(params->pos+params->q*pgeom->origin*params->scale); 
	quaternionf bodyq = m_qrot*params->q*pgeom->q;
	m_parts[i].mass = M;

	if (M>0) {
		if (m_body.M==0 || m_nParts==1)
			m_body.Create(bodypos,pgeom->Ibody*sqr(params->scale)*cube(params->scale),bodyq, V,M, m_qrot,m_pos);
		else
			m_body.Add(bodypos,pgeom->Ibody*sqr(params->scale)*cube(params->scale),bodyq, V,M);
	}
	m_prevPos = m_body.pos;
	m_prevq = m_body.q;
	
	return res;
}

void CRigidEntity::RemoveGeometry(int id, int bThreadSafe)
{
	ChangeRequest<void> req(this,m_pWorld,0,bThreadSafe,0,id);
	if (req.IsQueued())
		return;

	int i,bRecalcMass=0;
	for(i=0;i<m_nParts && m_parts[i].id!=id;i++);
	if (i==m_nParts) return;
	phys_geometry *pgeom = m_parts[i].pPhysGeomProxy;

	if (m_parts[i].mass>0) {
		//Vec3 bodypos = m_pos + m_qrot*(m_parts[i].pos+m_parts[i].q*pgeom->origin*m_parts[i].scale); 
		//quaternionf bodyq = m_qrot*m_parts[i].q*pgeom->q;

		if (m_nParts>1)
			bRecalcMass = 1;
		else
			m_body.zero();
			//m_body.Add(bodypos,-pgeom->Ibody*sqr(m_parts[i].scale)*cube(m_parts[i].scale), bodyq,
			//					 -pgeom->V*cube(m_parts[i].scale),-m_parts[i].mass);
	}
	DetachAllContacts();

	CPhysicalEntity::RemoveGeometry(id,1);

	if (bRecalcMass)
		RecomputeMassDistribution();

	int j;
	for(j=0; i<NMASKBITS && getmask(j)<=m_constraintMask; j++) if (m_constraintMask & getmask(j))
		m_pConstraints[j].ipart[0] = m_pConstraints[j].ipart[0]-isneg(i-m_pConstraints[j].ipart[0]);
}


void CRigidEntity::RecomputeMassDistribution(int ipart,int bMassChanged)
{
	float V;
	Vec3 bodypos,v=m_body.v,w=m_body.w;
	quaternionf bodyq,q=m_qrot.GetNormalized();

	RigidBody body;
	for(int i=0; i<m_nParts; i++) {
		V = m_parts[i].pPhysGeom->V*cube(m_parts[i].scale);
		if (V<=0)
			m_parts[i].mass = 0;
		bodypos = m_pos + q*(m_parts[i].pos+m_parts[i].q*m_parts[i].pPhysGeom->origin*m_parts[i].scale); 
		bodyq = (q*m_parts[i].q*m_parts[i].pPhysGeom->q).GetNormalized();
		if (m_parts[i].mass>0) {
			if (body.M==0)
				body.Create(bodypos,m_parts[i].pPhysGeom->Ibody*cube(m_parts[i].scale)*sqr(m_parts[i].scale),bodyq, V,m_parts[i].mass, q,m_pos);
			else
				body.Add(bodypos,m_parts[i].pPhysGeom->Ibody*sqr(m_parts[i].scale)*cube(m_parts[i].scale),bodyq, V,m_parts[i].mass);
		}
	}
	if (min(min(min(body.M,body.Ibody_inv.x),body.Ibody_inv.y),body.Ibody_inv.z)<0) {
		body.M = body.Minv = 0;
		body.Ibody_inv.zero(); body.Ibody.zero();
	}	else {
		body.P = (body.v=v)*body.M; 
		body.L = body.q*(body.Ibody*(!body.q*(body.w=w)));
	}
	m_body = body;
	m_prevPos = m_body.pos;
	m_prevq = m_body.q;
}


int CRigidEntity::SetParams(pe_params *_params, int bThreadSafe)
{
	int bRecalcBounds = 0;
	ChangeRequest<pe_params> req(this,m_pWorld,_params,bThreadSafe);
	if (req.IsQueued() && !bRecalcBounds)
		return 1+(m_bProcessed>>PENT_QUEUED_BIT);

	int res,i,flags0=m_flags;
	float scale0 = m_nParts ? m_parts[0].scale : 1.0f;
	Vec3 BBox0[2] = { m_BBox[0],m_BBox[1] };

	if (res = CPhysicalEntity::SetParams(_params,1)) {
		if (_params->type==pe_params_pos::type_id) {
			pe_params_pos *params = (pe_params_pos*)_params;
			Vec3 pos0 = m_body.pos;
			quaternionf q0 = m_body.q;
			if (!is_unused(params->pos) || params->pMtx3x4 || !is_unused(params->pGridRefEnt))
				m_body.pos = m_pos+m_qrot*m_body.offsfb; 
			if (!is_unused(params->q) || params->pMtx3x3 || params->pMtx3x4 || !is_unused(params->pGridRefEnt)) {
				m_body.pos = m_pos+m_qrot*m_body.offsfb;
				m_body.q = m_qrot*!m_body.qfb;
				m_body.Iinv = Matrix33(m_body.q)*m_body.Ibody_inv*Matrix33(!m_body.q);
			}
			if (!is_unused(params->iSimClass) && params->iSimClass<=2)
				m_bAwake = isneg(1-m_iSimClass);
			int bRealMove = isneg(min(sqr(0.003f)-(pos0-m_body.pos).len2(), sqr(0.05f)-(q0*!m_body.q).v.len2()));
			if (bRealMove && !(params->bRecalcBounds & 32) && !(m_flags & pef_disabled)) {
				if (m_pWorld->m_vars.lastTimeStep>0)
					for(i=0;i<m_nColliders;i++)
						m_pColliders[i]->Awake();
				DetachAllContacts();
				if (!(params->bRecalcBounds&2)) {
					//if (m_body.M>0) {
					//	m_body.v.zero(); m_body.w.zero(); m_body.P.zero(); m_body.L.zero();
					//}
					if (m_pWorld->m_vars.lastTimeStep>0)
						MoveConstrainedObjects(m_body.pos-pos0, m_body.q*!q0);
				}
			}
			if (!is_unused(params->scale) && m_nParts && fabsf(params->scale-scale0)>0.001f)
				RecomputeMassDistribution();
			if (m_nParts && m_pWorld->m_vars.lastTimeStep>0 && !(m_flags & pef_disabled) && !(params->bRecalcBounds & 32) && m_iSimClass<=2) {
				int nEnts;
				CPhysicalEntity **pentlist; 
				if (m_body.Minv==0) { // for animated 'static' rigid bodies, awake the environment
					for(i=0,flags0=0;i<m_nParts;i++) flags0|=m_parts[i].flagsCollider|m_parts[i].flags;
					if (flags0 & geom_colltype0) {
						nEnts = m_pWorld->GetEntitiesAround(m_BBox[0],m_BBox[1],pentlist,m_collTypes&(ent_sleeping_rigid|ent_living|ent_independent)|ent_triggers,this);
						for(--nEnts;nEnts>=0;nEnts--) if (pentlist[nEnts]!=this)
							pentlist[nEnts]->Awake();
					}
				}
				if (m_iLastLog<0 && bRealMove) {
					nEnts = m_pWorld->GetEntitiesAround(BBox0[0]-Vec3(m_pWorld->m_vars.maxContactGap*4), BBox0[1]+Vec3(m_pWorld->m_vars.maxContactGap*4),
						pentlist,m_collTypes&(ent_sleeping_rigid|ent_living|ent_independent),this);
					for(--nEnts;nEnts>=0;nEnts--) if (pentlist[nEnts]!=this)
						pentlist[nEnts]->Awake();
				}
			}
			m_prevPos = m_body.pos;
			m_prevq = m_body.q;
			if (params->bRecalcBounds & 64)
				PostStepNotify(0,0,0,0);
		} else if (_params->type==pe_params_part::type_id) {
			pe_params_part *params = (pe_params_part*)_params;
			if (!is_unused(params->partid) || !is_unused(params->ipart)) {
				if (!is_unused(params->mass) || !is_unused(params->density) || !is_unused(params->pPhysGeom) || !is_unused(params->pPhysGeomProxy))	{
					if (!is_unused(params->pPhysGeom) || !is_unused(params->pPhysGeomProxy))
						DetachAllContacts();
					if (m_parts[res-1].mass>0 || !is_unused(params->mass) || !is_unused(params->density))
						RecomputeMassDistribution(res-1,1);
				} else if (params->bRecalcBBox && m_parts[res-1].mass>0)
					RecomputeMassDistribution(res-1,0);
			} else if (!is_unused(params->mass))
				RecomputeMassDistribution();
		} else if (_params->type==pe_params_flags::type_id) {
			pe_params_flags *params = (pe_params_flags*)_params;
			if (!is_unused(params->flags) && params->flags & ref_small_and_fast || !is_unused(params->flagsOR) && params->flagsOR & ref_small_and_fast)
				m_bSmallAndFastForced = 1;
			if (flags0 & pef_invisible && !(m_flags & pef_invisible) && m_timeIdle>m_maxTimeIdle && m_submergedFraction>0 && !m_bAwake)
				Awake();
		}
		return res;
	}

	if (_params->type==pe_simulation_params::type_id) {
		pe_simulation_params *params = (pe_simulation_params*)_params;
		bool bRecompute = false;
		if (!is_unused(params->gravity)) {
			m_gravity = params->gravity;
			if (is_unused(params->gravityFreefall)) m_gravityFreefall = params->gravity;
		}
		if (!is_unused(params->maxTimeStep)) m_maxAllowedStep = params->maxTimeStep;
		if (!is_unused(params->minEnergy)) m_EminWater = min(m_EminWater, m_Emin = params->minEnergy);
		if (!is_unused(params->damping)) m_damping = params->damping;
		if (!is_unused(params->gravityFreefall)) m_gravityFreefall = params->gravityFreefall;
		if (!is_unused(params->dampingFreefall)) m_dampingFreefall = params->dampingFreefall;
		if (!is_unused(params->maxRotVel)) m_maxw = params->maxRotVel;
		if (!is_unused(params->density) && params->density>=0 && m_nParts>0) {
			for(i=0;i<m_nParts;i++) m_parts[i].mass = m_parts[i].pPhysGeom->V*cube(m_parts[i].scale)*params->density;
			bRecompute = true;
		}
		if (!is_unused(params->mass) && params->mass>=0 && m_nParts>0) {
			if (m_body.M==0) {
				float density,V=0;
				for(i=0;i<m_nParts;i++) V+=m_parts[i].pPhysGeom->V*cube(m_parts[i].scale);
				if (V>0) {
					density = params->mass/V;
					for(i=0;i<m_nParts;i++) m_parts[i].mass = m_parts[i].pPhysGeom->V*cube(m_parts[i].scale)*density;
				}
			} else {
				float scaleM = params->mass/m_body.M;
				for(i=0;i<m_nParts;i++) m_parts[i].mass *= scaleM;
			}
			bRecompute = true;
		}
		if (bRecompute)
			RecomputeMassDistribution();
		if (!is_unused(params->iSimClass))	{
			m_bAwake = isneg(1-(m_iSimClass = params->iSimClass));
			m_pWorld->RepositionEntity(this,2);
		}
		if (!is_unused(params->maxLoggedCollisions) && params->maxLoggedCollisions!=m_nMaxEvents) {
			if (m_pEventsColl) { delete[] m_pEventsColl; m_pEventsColl=0; }
			if (m_nMaxEvents = params->maxLoggedCollisions)
				m_pEventsColl = new EventPhysCollision*[m_nMaxEvents];
			m_nEvents = m_icollMin = 0; m_vcollMin = 1E10f;
		}
		if (!is_unused(params->disablePreCG)) m_bDisablePreCG = params->disablePreCG;
		if (!is_unused(params->maxFriction)) m_maxFriction = params->maxFriction;
		if (!is_unused(params->collTypes)) m_collTypes = params->collTypes;
		return 1;
	}

	if (_params->type==pe_params_buoyancy::type_id) {
		pe_params_buoyancy *params = (pe_params_buoyancy*)_params;
		float waterDensity0=1000.0f,waterResistance0=1000.0f;
		if (m_pWorld->m_pGlobalArea && !is_unused(m_pWorld->m_pGlobalArea->m_pb.waterPlane.origin)) {
			waterDensity0 = m_pWorld->m_pGlobalArea->m_pb.waterDensity;
			waterResistance0 = m_pWorld->m_pGlobalArea->m_pb.waterResistance;
		}
		if (!is_unused(params->waterDensity)) m_kwaterDensity = params->waterDensity/waterDensity0;
		if (!is_unused(params->kwaterDensity)) m_kwaterDensity = params->kwaterDensity;
		if (!is_unused(params->waterResistance)) m_kwaterResistance = params->waterResistance/waterResistance0;
		if (!is_unused(params->kwaterResistance)) m_kwaterResistance = params->kwaterResistance;
		if (!is_unused(params->waterEmin)) m_EminWater = params->waterEmin;
		if (!is_unused(params->waterDamping)) m_waterDamping = params->waterDamping;
		return 1;
	}

	return res;
}

int CRigidEntity::GetParams(pe_params *_params) const
{
	if (_params->type==pe_simulation_params::type_id) {
		pe_simulation_params *params = (pe_simulation_params*)_params;
		params->gravity = m_gravity;
		params->maxTimeStep = m_maxAllowedStep;
		params->minEnergy = m_Emin;
		params->damping = m_damping;
		params->dampingFreefall = m_dampingFreefall;
		params->gravityFreefall = m_gravityFreefall;
		params->maxRotVel = m_maxw;
		params->iSimClass = m_iSimClass;
		params->density = m_body.V>0 ? m_body.M/m_body.V:0;
		params->mass = m_body.M;
		params->maxLoggedCollisions = m_nMaxEvents;
		params->disablePreCG = m_bDisablePreCG;
		params->maxFriction = m_maxFriction;
		params->collTypes = m_collTypes;
		return 1;
	}

	if (_params->type==pe_params_buoyancy::type_id) {
		pe_params_buoyancy *params = (pe_params_buoyancy*)_params;
		float waterDensity0=1000.0f,waterResistance0=1000.0f;
		if (m_pWorld->m_pGlobalArea && !is_unused(m_pWorld->m_pGlobalArea->m_pb.waterPlane.origin)) {
			waterDensity0 = m_pWorld->m_pGlobalArea->m_pb.waterDensity;
			waterResistance0 = m_pWorld->m_pGlobalArea->m_pb.waterResistance;
		}
		params->waterDensity = m_kwaterDensity*waterDensity0;
		params->kwaterDensity = m_kwaterDensity;
		params->waterResistance = m_kwaterResistance*waterResistance0;
		params->kwaterResistance = m_kwaterResistance;
		params->waterDamping = m_waterDamping;
		params->waterEmin = m_EminWater;
		return 1;
	}

	return CPhysicalEntity::GetParams(_params);
}


int CRigidEntity::GetStatus(pe_status *_status) const
{
	if (_status->type==pe_status_awake::type_id) {
		pe_status_awake *status = (pe_status_awake*)_status;
		return status->lag ? m_iLastLog<0 || m_bAwake|(m_iLastLog>=m_pWorld->m_iLastLogPump-status->lag) : m_bAwake;
	}

	if (_status->type==pe_status_netpos::type_id) {
#if USE_IMPROVED_RIGID_ENTITY_SYNCHRONISATION
		if (m_pNetStateHistory) {
			pe_status_netpos *status = (pe_status_netpos*)_status;
			ReadLock lock(m_lockNetInterp);
			int numStates = m_pNetStateHistory->GetNumReceivedStates();
			if (numStates > 0) {
				const SRigidEntityNetSerialize& latestState = m_pNetStateHistory->GetReceivedState(numStates - 1);
				float sequenceDeltaAverage = m_pNetStateHistory->GetAverageSequenceDelta();
				float sequenceOffset = (GetLocalSequenceNumber() - latestState.sequenceNumber) + sequenceDeltaAverage;
				if (sequenceOffset > MAX_SEQUENCE_NUMBER / 2.0f) {
					sequenceOffset -= MAX_SEQUENCE_NUMBER;
				}
				status->pos = latestState.pos;
				status->rot = latestState.rot;
				status->vel = latestState.vel;
				status->angvel = latestState.angvel;
				status->timeOffset = sequenceOffset / m_pWorld->m_vars.netSequenceFrequency;
				return 1;
			}
		}
#endif
		return 0;
	}

	int res;
	if (res = CPhysicalEntity::GetStatus(_status)) {
		if (_status->type==pe_status_pos::type_id) {
			pe_status_pos *status = (pe_status_pos*)_status;
			if (status->timeBack>0) {
				status->q = m_prevq*m_body.qfb;
				status->pos = m_prevPos-status->q*m_body.offsfb;
			}
		}
		return res;
	}

	if (_status->type==pe_status_constraint::type_id) {
		pe_status_constraint *status = (pe_status_constraint*)_status;
		int i; 
		if ((i=status->idx)>=0)
			if (m_constraintMask & getmask(i)) {
				status->id=m_pConstraintInfos[i].id; goto foundconstr;
			} else
				return 0;
		for(i=0;i<NMASKBITS && getmask(i)<=m_constraintMask;i++) if (m_constraintMask & getmask(i) && m_pConstraintInfos[i].id==status->id) {
			foundconstr:
			status->pt[0] = m_pConstraints[i].pt[0];
			status->pt[1] = m_pConstraints[i].pt[1];
			status->n = m_pConstraints[i].n; 
			status->flags = m_pConstraintInfos[i].flags;
			status->pBuddyEntity = m_pConstraints[i].pent[1];
			status->pConstraintEntity = m_pConstraintInfos[i].pConstraintEnt;
			return 1;
		}
		return 0;
	}

	if (_status->type==pe_status_dynamics::type_id) {
		pe_status_dynamics *status = (pe_status_dynamics*)_status;
		if (m_bAwake) {
			status->v = m_body.v;
			status->w = m_body.w;
			status->a = m_gravity + m_body.Fcollision*m_body.Minv;
			Vec3 L = m_body.q*(m_body.Ibody*(!m_body.q*status->w));
			status->wa = m_body.Iinv*(m_body.Tcollision - (m_body.w^L));
		} else {
			status->v.zero(); status->w.zero();
			status->a.zero(); status->wa.zero();
		}
		status->centerOfMass = m_body.pos;
		status->submergedFraction = m_submergedFraction;
		status->mass = m_body.M;
		return 1;
	}

	if (_status->type==pe_status_collisions::type_id)
		return m_pContactStart!=CONTACT_END(m_pContactStart);

	if (_status->type==pe_status_sample_contact_area::type_id) {
		pe_status_sample_contact_area *status = (pe_status_sample_contact_area*)_status;
		Vec3 sz = m_BBox[1]-m_BBox[0];
		float dist,tol = (sz.x+sz.y+sz.z)*0.1f;
		int nContacts;
		entity_contact *pRes;
		return CompactContactBlock(m_pContactStart,0,tol,0,nContacts, pRes, sz,dist, status->ptTest,status->dirTest);
	}

	return 0;
}


int CRigidEntity::Action(pe_action *_action, int bThreadSafe)
{
	ChangeRequest<pe_action> req(this,m_pWorld,_action,bThreadSafe);
	if (req.IsQueued())	{
		if (_action->type==pe_action_add_constraint::type_id) {
			pe_action_add_constraint *action = (pe_action_add_constraint*)req.GetQueuedStruct();
			if (is_unused(action->id)) {
				WriteLock lock(m_lockConstraintIdx);
				action->id = m_iLastConstraintIdx++;
			}
			return action->id;
		}
		return 1;
	}

	if (_action->type==pe_action_impulse::type_id) {
		pe_action_impulse *action = (pe_action_impulse*)_action;
		ENTITY_VALIDATE("CRigidEntity:Action(action_impulse)",action);
		QuatT transG = InputGridTrans(action,this);

		if (m_body.Minv>0) {
			Vec3 P=transG.q*action->impulse, L(ZERO);
			if (!is_unused(action->angImpulse))
				L = transG.q*action->angImpulse;
			if (!is_unused(action->point))
				L += transG*action->point-m_body.pos ^ P;

			if (action->iSource!=1) {
				m_bAwake = 1;
				if (m_pWorld->m_vars.idEntBreakOnAwake==m_id)
					DoBreak
				if (m_iSimClass==1) {
					m_iSimClass = 2;	m_pWorld->RepositionEntity(this, 2);
				}
				//m_impulseTime = 0.2f;
				m_timeIdle *= isneg(-action->iSource);
			}	else {
				float vres;
				if ((vres=P.len2()*sqr(m_body.Minv)) > sqr(5.0f))
					P *= 5.0f/sqrt_tpl(vres);
				if ((vres=(m_body.Iinv*L).len2()) > sqr(5.0f))
					L *= 5.0f/sqrt_tpl(vres);
			}

			if (action->iApplyTime==0) {
				m_body.P+=P;m_body.L+=L; m_body.v=m_body.P*m_body.Minv; m_body.w=m_body.Iinv*m_body.L;
				CapBodyVel();
			} else {
				m_Pext+=P; m_Lext+=L;
			}
		}

		CPhysicalEntity::Action(_action,1);

		return 1;
	}

	if (_action->type==pe_action_reset::type_id) {
		m_body.v.zero(); m_body.w.zero(); m_body.P.zero(); m_body.L.zero();
		m_Pext.zero(); m_Lext.zero();
		if (((pe_action_reset*)_action)->bClearContacts) {
			DetachAllContacts();
			pe_action_update_constraint auc;
			auc.bRemove = 1;
			Action(&auc);
		}
		/*if (m_pWorld->m_vars.bMultiplayer) {
			m_qrot = CompressQuat(m_qrot);
			Matrix33 R = Matrix33(m_body.q = m_qrot*!m_body.qfb);
			m_body.Iinv = R*m_body.Ibody_inv*R.T();
			ComputeBBox(m_BBox);
			JobAtomicAdd(&m_pWorld->m_lockGrid,-m_pWorld->RepositionEntity(this,1));
		}*/
		m_Psoft.zero(); m_Lsoft.zero();
		//m_nStickyContacts = m_nSlidingContacts = 0;
		m_iLastLog = m_iLastLogColl = -2;
		m_nRestMask = 0;
		m_nEvents = 0;
		return 1;
	}

	if (_action->type==pe_action_add_constraint::type_id) {
		pe_action_add_constraint *action = (pe_action_add_constraint*)_action;
		CPhysicalEntity *pBuddy = (CPhysicalEntity*)action->pBuddy;
		CPhysicalPlaceholder *pBuddy0 = (CPhysicalPlaceholder*)action->pBuddy;
		bool simple = pBuddy0 && pBuddy0!=WORLD_ENTITY && pBuddy0->m_iSimClass>3 && pBuddy0->GetType()!=PE_ARTICULATED;
		if (pBuddy==WORLD_ENTITY || pBuddy0 && pBuddy0->m_iSimClass>4)
			pBuddy = &g_StaticPhysicalEntity;
		if (!pBuddy || (unsigned int)pBuddy->m_iSimClass>4u)
			return 0;
		int i,ipart[2],id,flagsInfo,bBreakable=0,flagsLin=contact_constraint_3dof;
		Vec3 nloc,pt0,pt1;
		quaternionf qframe[2];
		float damping = 0;
		qframe[0].SetIdentity(); qframe[1].SetIdentity();
		if (is_unused(action->pt[0]))
			return 0;
		if (is_unused(action->id)) {
			WriteLock lock(m_lockConstraintIdx);
			id = m_iLastConstraintIdx++;
		} else
			id = action->id;
		pt0 = action->pt[0];
		pt1 = is_unused(action->pt[1]) ? action->pt[0] : action->pt[1];
		if (!is_unused(action->damping)) damping = action->damping;
		flagsInfo = action->flags & ~(local_frames|world_frames|local_frames_part);
		QuatT transW(IDENTITY), transBuddy=GridTrans(pBuddy,this);
		if (!(action->flags & (local_frames|local_frames_part)) && (transBuddy*pt0-m_body.pos).len2() < (pt0-m_body.pos).len2())
			pt0 = (transW = transBuddy)*pt0;
		if (action->flags & local_frames) {
			pt0 = m_qrot*pt0 + m_pos;
			pt1 = transBuddy*(pBuddy->m_qrot*pt1 + pBuddy->m_pos);
		}
		if (action->flags & constraint_line)
			flagsLin = contact_constraint_1dof;
		else if (action->flags & constraint_plane)
			flagsLin = contact_constraint_2dof;

		if (!is_unused(action->partid[0])) {
			for(ipart[0]=0;ipart[0]<m_nParts && m_parts[ipart[0]].id!=action->partid[0];ipart[0]++);
			if (ipart[0]>=m_nParts)
				return 0;
		} else if (m_nParts)
			ipart[0] = 0;
		else 
			return 0;
		if (!is_unused(action->partid[1])) {
			for(ipart[1]=0;ipart[1]<pBuddy->m_nParts && pBuddy->m_parts[ipart[1]].id!=action->partid[1];ipart[1]++);
			if (pBuddy->m_nParts>0 && ipart[1]>=pBuddy->m_nParts)
				return 0;
		} else if (pBuddy->m_nParts || simple || pBuddy==&g_StaticPhysicalEntity)
			ipart[1] = 0;
		else
			return 0;
		if (action->flags & local_frames_part) {
			pt0 = m_qrot*(m_parts[ipart[0]].q*pt0*m_parts[ipart[0]].scale + m_parts[ipart[0]].pos) + m_pos;
			pt1 = transBuddy*(pBuddy->m_qrot*(pBuddy->m_parts[ipart[1]].q*pt1*pBuddy->m_parts[ipart[1]].scale + pBuddy->m_parts[ipart[1]].pos) + pBuddy->m_pos);
		}
		if (is_unused(action->pt[1]))
			pt1 = pt0;

		CPhysicalEntity *pConstraintEnt = is_unused(action->pConstraintEntity) ? 0:(CPhysicalEntity*)action->pConstraintEntity;
		if (pConstraintEnt && pConstraintEnt->GetType()==PE_AREA)
			flagsInfo |= constraint_area;
		i = RegisterConstraint(pt0,pt1,ipart[0], pBuddy,ipart[1], flagsLin,flagsInfo, pConstraintEnt);
		if (i<0)
			return 0;

		m_pConstraintInfos[i].id = id;
		if (!is_unused(action->sensorRadius)) m_pConstraintInfos[i].sensorRadius = action->sensorRadius;
		if (!is_unused(action->maxPullForce)) m_pConstraintInfos[i].limit=action->maxPullForce, bBreakable=1;
		if (!is_unused(action->hardnessLin)) m_pConstraintInfos[i].hardness = action->hardnessLin; 

		QuatT frames[2]; GetContactFrames(m_pConstraints[i], frames);
		if (!is_unused(action->qframe[0])) qframe[0] = action->qframe[0];
		if (!is_unused(action->qframe[1])) qframe[1] = action->qframe[1];
		if (action->flags & local_frames) {
			qframe[0] = m_qrot*qframe[0];
			qframe[1] = transBuddy.q*pBuddy->m_qrot*qframe[1];
		}	else if (action->flags & local_frames_part) {
			qframe[0] = m_qrot*m_parts[ipart[0]].q*qframe[0];
			qframe[1] = transBuddy.q*pBuddy->m_qrot*pBuddy->m_parts[ipart[1]].q*qframe[1];
		}	else {
			qframe[0] = transW.q*qframe[0];
			qframe[1] = transW.q*qframe[1];
		}
		m_pConstraints[i].nloc=nloc = !frames[FrameOwner(m_pConstraints[i])].q*qframe[0]*Vec3(1,0,0);
		qframe[0] = !frames[0].q*qframe[0];
		qframe[1] = !frames[1].q*qframe[1];
		m_pConstraintInfos[i].qframe_rel[0]=qframe[0]; m_pConstraintInfos[i].qframe_rel[1]=qframe[1];
		m_pConstraintInfos[i].damping = damping;

		if (pConstraintEnt && pConstraintEnt->GetType()==PE_ROPE) {
			m_pConstraints[i].flags = 0; // act like frictionless contact when rope is strained
			m_pConstraintInfos[i].flags = constraint_rope;
			m_pConstraintInfos[i].damping = damping;
		}

		if (simple) {
			pe_status_pos sp; pBuddy0->GetStatus(&sp);
			m_pConstraints[i].pent[1] = (CPhysicalEntity*)pBuddy0;
			m_pConstraintInfos[i].qframe_rel[1] = !sp.q*qframe[1];
			m_pConstraintInfos[i].ptloc[1] = (pt1-sp.pos)*sp.q;
			return id;
		}

		if (action->flags & constraint_free_position)	{
			for(int j=0;j<m_nColliders;j++)
				m_pColliderConstraints[j] &= ~getmask(i);
			m_constraintMask &= ~getmask(i);
		}

		// if constraint_no_rotation is specified or at least one of the limit pairs is valid then we go on and restrict angular dof
		// 1. constraint_no_rotation restricts all 3 angular dof
		// 2. if both limit pairs are valid then we set one limited constraint for X and another one for YZ
		// 3. if only X limits are valid then we need a hinge joint to prevent YZ rotations and a constraint to limit the X twist
		// 4. if only Y limits are valid then we need a hinge2/cone joint to prevent X twist and a constraint to limit YZ rotations
		// the X limits constrain the relative twist angle between the 2 bodies around the constraint X axis
		// the YZ limits constrain only the (sine of the) maximum angle between the 2 bodies' constraint X axes - basically the combined YZ relative rotations

		if (action->flags & constraint_no_rotation) {
			i = RegisterConstraint(pt0,pt1,ipart[0], pBuddy,ipart[1], contact_angular|contact_constraint_3dof,flagsInfo,pConstraintEnt);
			m_pConstraints[i].nloc=nloc; m_pConstraintInfos[i].qframe_rel[0]=qframe[0];	m_pConstraintInfos[i].qframe_rel[1]=qframe[1];
			m_pConstraintInfos[i].id = id; m_pConstraintInfos[i].damping = damping;
			if (!is_unused(action->maxBendTorque)) m_pConstraintInfos[i].limit=action->maxBendTorque;
			if (!is_unused(action->hardnessAng)) m_pConstraintInfos[i].hardness=action->hardnessAng;
		} else if (!(!is_unused(action->xlimits[0]) && action->xlimits[0]>=action->xlimits[1] &&
					!is_unused(action->yzlimits[0]) && action->yzlimits[0]>=action->yzlimits[1]))
		{
			if (!is_unused(action->xlimits[0]) && action->xlimits[0]>=action->xlimits[1]) {
				i = RegisterConstraint(pt0,pt1,ipart[0], pBuddy,ipart[1], contact_angular|contact_constraint_2dof,flagsInfo,pConstraintEnt);
				m_pConstraints[i].nloc=nloc; m_pConstraintInfos[i].qframe_rel[0]=qframe[0];	m_pConstraintInfos[i].qframe_rel[1]=qframe[1];
				m_pConstraintInfos[i].id = id; m_pConstraintInfos[i].damping = damping;
			} else if (!is_unused(action->yzlimits[0]) && action->yzlimits[0]>=action->yzlimits[1]) {				
				i = RegisterConstraint(pt0,pt1,ipart[0], pBuddy,ipart[1], contact_angular|contact_constraint_1dof,flagsInfo,pConstraintEnt);
				m_pConstraints[i].nloc=nloc; m_pConstraintInfos[i].qframe_rel[0]=qframe[0];	m_pConstraintInfos[i].qframe_rel[1]=qframe[1];
				m_pConstraintInfos[i].id = id; m_pConstraintInfos[i].damping = damping;
			}

			if ((flagsLin!=contact_constraint_1dof || pConstraintEnt) && !is_unused(action->xlimits[0]) && action->xlimits[0]<action->xlimits[1]) {
				i = RegisterConstraint(pt0,pt1,ipart[0], pBuddy,ipart[1], contact_angular,flagsInfo,pConstraintEnt);
				m_pConstraints[i].nloc=nloc; m_pConstraintInfos[i].qframe_rel[0]=qframe[0];	m_pConstraintInfos[i].qframe_rel[1]=qframe[1];
				m_pConstraintInfos[i].limits[0]=action->xlimits[0]; m_pConstraintInfos[i].limits[1]=action->xlimits[1];
				m_pConstraintInfos[i].flags = constraint_limited_1axis | (flagsInfo & constraint_area);
				m_pConstraintInfos[i].id = id;
				if (!is_unused(action->maxBendTorque)) m_pConstraintInfos[i].limit=action->maxBendTorque, bBreakable=2;
				if (!is_unused(action->hardnessAng)) m_pConstraintInfos[i].hardness=action->hardnessAng;
			}
			if (!is_unused(action->yzlimits[0]) && action->yzlimits[0]<action->yzlimits[1]) {
				i = RegisterConstraint(pt0,pt1,ipart[0], pBuddy,ipart[1], contact_angular,flagsInfo,pConstraintEnt);
				m_pConstraints[i].nloc=nloc; m_pConstraintInfos[i].qframe_rel[0]=qframe[0];	m_pConstraintInfos[i].qframe_rel[1]=qframe[1];
				m_pConstraintInfos[i].limits[0]=sin_tpl(action->yzlimits[1]); m_pConstraintInfos[i].limits[1]=action->yzlimits[1]; 
				m_pConstraintInfos[i].flags = constraint_limited_2axes | (flagsInfo & constraint_area);
				m_pConstraintInfos[i].id = id;
				if (!is_unused(action->maxBendTorque) && bBreakable<2) m_pConstraintInfos[i].limit=action->maxBendTorque, bBreakable=1;
				if (!is_unused(action->hardnessAng)) m_pConstraintInfos[i].hardness=action->hardnessAng;
			}
		}

		if (flagsLin==contact_constraint_1dof && !is_unused(action->xlimits[0]) && action->xlimits[0]<action->xlimits[1] && !pConstraintEnt) {
			i = RegisterConstraint(pt0,pt1,ipart[0], pBuddy,ipart[1], 0,flagsInfo,pConstraintEnt);
			m_pConstraints[i].nloc=nloc; m_pConstraintInfos[i].qframe_rel[0]=qframe[0];	m_pConstraintInfos[i].qframe_rel[1]=qframe[1];
			m_pConstraintInfos[i].limits[0]=action->xlimits[0]; m_pConstraintInfos[i].limits[1]=action->xlimits[1];
			m_pConstraintInfos[i].flags = constraint_limited_1axis;
			m_pConstraintInfos[i].id = id;
			if (!is_unused(action->maxPullForce)) m_pConstraintInfos[i].limit=action->maxPullForce, bBreakable=2;
			if (!is_unused(action->hardnessLin)) m_pConstraintInfos[i].hardness=action->hardnessLin;
		}
		if (bBreakable)
			BreakableConstraintsUpdated();

		return id;
	}

	if (_action->type==pe_action_update_constraint::type_id) {
		pe_action_update_constraint *action = (pe_action_update_constraint*)_action;
		int i,j,n;

		if (is_unused(action->idConstraint)) {
			if (action->bRemove) {
				for(i=m_nColliders-1; i>=0 ;i--) if (m_pColliderConstraints[i]) {
					m_constraintMask &= ~m_pColliderConstraints[i]; m_pColliderConstraints[i] = 0;
					if (!m_pColliderContacts[i] && !m_pColliders[i]->HasContactsWith(this))
						RemoveCollider(m_pColliders[i]);
				}
				BreakableConstraintsUpdated();
			} else for(i=NMASKBITS-1,n=0; i>=0; i--) if (m_constraintMask & getmask(i))
				m_pConstraintInfos[i].flags = m_pConstraintInfos[i].flags & action->flagsAND | action->flagsOR;
			return 1;
		} else {
			for(i=NMASKBITS-1,n=0; i>=0; i--) if (m_constraintMask & getmask(i) && m_pConstraintInfos[i].id==action->idConstraint) {
				m_pConstraintInfos[i].flags = m_pConstraintInfos[i].flags & action->flagsAND | action->flagsOR;
				QuatT frames[2]; GetContactFrames(m_pConstraints[i], frames);
				for(j=0;j<2;j++) {
					if (!is_unused(action->pt[j])) {
						m_pConstraints[i].pt[j] = (action->flags & world_frames) ? action->pt[j] : 
																			m_pConstraints[i].pent[j]->m_qrot*action->pt[j]+m_pConstraints[i].pent[j]->m_pos;
						m_pConstraintInfos[i].ptloc[j] = frames[j].GetInverted()*m_pConstraints[i].pt[j];
					}
					if (!is_unused(action->qframe[j])) {
						quaternionf qframe = action->qframe[j];
						if (action->flags & local_frames)
							qframe = m_pConstraints[i].pent[j]->m_qrot*qframe;
						qframe = !frames[j].q*qframe;
						m_pConstraintInfos[i].qframe_rel[j] = qframe;
						if (j==FrameOwner(m_pConstraints[i]))
							m_pConstraints[i].nloc = qframe*Vec3(1,0,0);
					}
				}
				if (!is_unused(action->maxPullForce) && !(m_pConstraints[i].flags & contact_angular)) m_pConstraintInfos[i].limit = action->maxPullForce;
				if (!is_unused(action->hardnessLin) && !(m_pConstraints[i].flags & contact_angular)) m_pConstraintInfos[i].hardness = action->hardnessLin;
				if (!is_unused(action->hardnessAng) && m_pConstraints[i].flags & contact_angular) m_pConstraintInfos[i].hardness = action->hardnessAng;
				if (!is_unused(action->maxBendTorque) && (m_pConstraints[i].flags & (contact_angular|contact_constraint_1dof|contact_constraint_2dof))==contact_angular)
					m_pConstraintInfos[i].limit = action->maxBendTorque;
				if (!is_unused(action->damping)) m_pConstraintInfos[i].damping = action->damping;
				if (action->bRemove)
					n += RemoveConstraint(i);
			}
			if (action->bRemove)
				BreakableConstraintsUpdated();
			return n;
		}
	}

	if (_action->type==pe_action_set_velocity::type_id) {
		pe_action_set_velocity *action = (pe_action_set_velocity*)_action;
		if (!is_unused(action->v))
			m_body.P = (m_body.v = action->v)*m_body.M;
		if (!is_unused(action->w)) {
			m_body.L = m_body.q*(m_body.Ibody*(!m_body.q*(m_body.w = action->w)));
			if (action->bRotationAroundPivot && (m_body.pos-m_pos).len2()>sqr(m_pWorld->m_vars.maxContactGap)) {
				if (is_unused(action->v))
					m_body.v.zero();
				m_body.v += m_body.w ^ m_body.pos-m_pos;
				m_body.P = m_body.v*m_body.M;
			}
		}
		CapBodyVel();
		if (m_body.v.len2()+m_body.w.len2()>0) {
			if (!m_bAwake)
				Awake();
			m_timeIdle = 0;
		} else if (m_body.Minv==0)
			Awake(0);

		return 1;
	}

	if (_action->type==pe_action_register_coll_event::type_id) {
		pe_action_register_coll_event *action = (pe_action_register_coll_event*)_action;
		EventPhysCollision event;
		event.pEntity[0] = this; event.pForeignData[0] = m_pForeignData; event.iForeignData[0] = m_iForeignData;
		event.pEntity[1] = action->pCollider; event.pForeignData[1] = ((CPhysicalEntity*)action->pCollider)->m_pForeignData; 
		event.iForeignData[1] = ((CPhysicalEntity*)action->pCollider)->m_iForeignData;
		event.pt = action->pt;
		event.n = action->n;
		event.idCollider = m_pWorld->GetPhysicalEntityId(action->pCollider);

		int i; for(i=0;i<m_nParts && m_parts[i].id!=action->partid[0];i++);
		RigidBody *pbody = GetRigidBody(i);
		event.vloc[0] = is_unused(action->vSelf) ? pbody->v+(pbody->w^pbody->pos-action->pt) : action->vSelf;
		event.vloc[1] = action->v;
		event.mass[0] = pbody->M;
		event.mass[1] = action->collMass;
		event.partid[0] = action->partid[0];
		event.partid[1] = action->partid[1];
		event.idmat[0] = action->idmat[0];
		event.idmat[1] = action->idmat[1];
		event.penetration=event.radius=event.normImpulse = 0;
		event.iPrim[0] = action->iPrim[0];
		event.iPrim[1] = action->iPrim[1];
		m_pWorld->OnEvent(m_flags,&event);

		return 1;
	}

	if (_action->type==pe_action_awake::type_id && m_iSimClass>=0 && m_iSimClass<7) {
		pe_action_awake *action = (pe_action_awake*)_action;
		Awake(action->bAwake,1);	
		if (!is_unused(action->minAwakeTime)) m_minAwakeTime = action->minAwakeTime;
		return 1;
	}

	return CPhysicalEntity::Action(_action,1);
}

int CRigidEntity::RemoveCollider(CPhysicalEntity *pCollider, bool bRemoveAlways)
{
	INT_PTR plock0=(INT_PTR)&m_lockColliders, plock1=(INT_PTR)&pCollider->m_lockColliders, mask = pCollider->m_id-m_id>>31;
	WriteLock lock(*(volatile int*)(plock0+(plock1-plock0&mask)));
	WriteLockCond lock1(*(volatile int*)(plock1+(plock0-plock1&mask)), pCollider!=this);
	pCollider->RemoveColliderNoLock(this,bRemoveAlways);
	return RemoveColliderNoLock(pCollider,bRemoveAlways);
}

int CRigidEntity::RemoveColliderNoLock(CPhysicalEntity *pCollider, bool bRemoveAlways)
{
	int i; 
	if (!bRemoveAlways) {
		for(i=0;i<m_nColliders && m_pColliders[i]!=pCollider; i++);
		if (i<m_nColliders && (m_pColliderContacts[i] || m_pColliderConstraints[i]))
			return i;
	}
	i = CPhysicalEntity::RemoveCollider(pCollider,bRemoveAlways);
	if (i>=0) {
		entity_contact *pContact,*pContactNext;
		if (m_pColliderContacts[i]) for(pContact=m_pColliderContacts[i];; pContact=pContactNext) {
			pContactNext=pContact->next; DetachContact(pContact,i,0); m_pWorld->FreeContact(pContact);
			if (pContact->flags & contact_last) break;
		}

		m_constraintMask &= ~m_pColliderConstraints[i];
		for(;i<m_nColliders;i++) {
			m_pColliderContacts[i] = m_pColliderContacts[i+1];
			m_pColliderConstraints[i] = m_pColliderConstraints[i+1];
		}
	}
	return i;
}


int CRigidEntity::AddCollider(CPhysicalEntity *pCollider)
{
	INT_PTR plock0=(INT_PTR)&m_lockColliders, plock1=(INT_PTR)&pCollider->m_lockColliders, mask = pCollider->m_id-m_id>>31;
	WriteLock lock(*(volatile int*)(plock0+(plock1-plock0&mask)));
	WriteLockCond lock1(*(volatile int*)(plock1+(plock0-plock1&mask)), pCollider!=this);
	pCollider->AddColliderNoLock(this);
	return AddColliderNoLock(pCollider);
}

int CRigidEntity::AddColliderNoLock(CPhysicalEntity *pCollider)
{
	entity_contact *pDummyContact = 0;
	masktype dummyMask = 0;
	bool bDeleteOld = true;
	if (!m_nCollidersAlloc) { // make the lists point to valid data for reads from non mt-synced functions
		m_pColliderContacts=&pDummyContact; m_pColliderConstraints=&dummyMask; bDeleteOld=false;
	}
	int nColliders=m_nColliders, nCollidersAlloc=m_nCollidersAlloc, i=CPhysicalEntity::AddCollider(pCollider);

	if (m_nCollidersAlloc>nCollidersAlloc) {
		ReallocateList(m_pColliderContacts, nColliders, m_nCollidersAlloc, true, bDeleteOld);
		ReallocateList(m_pColliderConstraints, nColliders, m_nCollidersAlloc, true, bDeleteOld);
	}

	if (m_nColliders>nColliders) {
		for(int j=nColliders-1;j>=i;j--) {
			m_pColliderContacts[j+1] = m_pColliderContacts[j];
			m_pColliderConstraints[j+1] = m_pColliderConstraints[j];
		}
		m_pColliderContacts[i] = 0;
		m_pColliderConstraints[i] = 0;
	}
	m_bJustLoaded = 0;

	return i;
}


int CRigidEntity::HasContactsWith(CPhysicalEntity *pent)
{
	int i; for(i=0;i<m_nColliders && m_pColliders[i]!=pent;i++);
	return i==m_nColliders ? 0 : (m_pColliderContacts[i]!=0 || m_pColliderConstraints[i]!=0);
}

int CRigidEntity::HasCollisionContactsWith(CPhysicalEntity *pent)
{
	int i; for(i=0;i<m_nColliders && m_pColliders[i]!=pent;i++);
	return i==m_nColliders ? 0 : m_pColliderContacts[i]!=0;
}

int CRigidEntity::HasConstraintContactsWith(const CPhysicalEntity *pent, int flagsIgnore) const
{
	int i; for(i=0;i<m_nColliders && m_pColliders[i]!=pent;i++);
	if (i==m_nColliders || m_pColliders[i]!=pent)
		return 0;
	else if (!flagsIgnore)
		return m_pColliderConstraints[i]!=0;
	for(int j=0; j<NMASKBITS && getmask(j)<=m_pColliderConstraints[i]; j++) 
		if (m_pColliderConstraints[i] & getmask(j) && !(m_pConstraintInfos[j].flags & flagsIgnore))
			return 1;
	return 0;
}

int CRigidEntity::HasPartContactsWith(CPhysicalEntity *pent, int ipart, int bGreaterOrEqual)
{
	ReadLock lockc(m_lockColliders);
	ReadLock lock(m_lockContacts);
	int i; for(i=0;i<m_nColliders && m_pColliders[i]!=pent;i++);
	if (i==m_nColliders)
		return 0;
	for(int j=0; j<NMASKBITS && getmask(j)<=m_pColliderConstraints[i]; j++) 
		if (m_pColliderConstraints[i] & getmask(j) && iszero(ipart-m_pConstraints[i].ipart[1]) | isneg(ipart-m_pConstraints[i].ipart[1]) & -bGreaterOrEqual)
			return 1;
	if (entity_contact *pContact=m_pColliderContacts[i]) for(;;pContact=pContact->next) {
		if (iszero(ipart-pContact->ipart[1]) | isneg(ipart-pContact->ipart[1]) & -bGreaterOrEqual)
			return 1;
		if (pContact->flags & contact_last)
			break;
	}
	return 0;
}


int CRigidEntity::Awake(int bAwake,int iSource)
{
	if ((unsigned int)m_iSimClass>6u) {
		//VALIDATOR_LOG(m_pWorld->m_pLog, "Error: trying to awake deleted rigid entity");
		return -1;
	}
	int i;
	if (m_iSimClass<=2) {
		CPhysicalEntity *pDeadCollider=0;
		{ ReadLock lockc(m_lockColliders);
			for(i=m_nColliders-1; i>=0; i--) if (m_pColliders[i]->m_iSimClass==7) {
				m_pColliders[i]->m_next_coll1=pDeadCollider; pDeadCollider=m_pColliders[i];
			}
		}
		for(; pDeadCollider; pDeadCollider=pDeadCollider->m_next_coll1)
			RemoveColliderMono(pDeadCollider);
		if ((m_iSimClass!=bAwake+1 || m_bAwake!=bAwake && iSource==5) && 
				(m_body.Minv>0 || m_body.v.len2()+m_body.w.len2()>0 || bAwake==0 || iSource==1)) 
		{	
			m_nSleepFrames = 0;	m_bAwake = bAwake;
			m_bSmallAndFastForced &= bAwake;
			m_iSimClass = m_bAwake+1; m_pWorld->RepositionEntity(this,2);
			if (bAwake)
				m_minAwakeTime = 0.1f;
		}
		if (m_body.Minv==0 && bAwake) for(i=0;i<m_nColliders;i++)	
		{
			PREFAST_ASSUME(m_pColliders[i]);
			if (m_pColliders[i]!=this && m_pColliders[i]->GetMassInv()>0)
				m_pColliders[i]->Awake();
		}
		if (m_pWorld->m_vars.idEntBreakOnAwake==m_id)
			DoBreak
	}	else
		m_bAwake = bAwake;
	return m_iSimClass;
}


void CRigidEntity::AlertNeighbourhoodND(int mode) 
{ 
	int i,iSimClass=m_iSimClass;
	m_iSimClass = 7; // notifies the others that we are being deleted

	for(i=0;i<NMASKBITS && getmask(i)<=m_constraintMask;i++) 
		if (m_constraintMask & getmask(i) && m_pConstraintInfos[i].pConstraintEnt && (unsigned int)m_pConstraintInfos[i].pConstraintEnt->m_iSimClass<7u) 
			m_pConstraintInfos[i].pConstraintEnt->Awake();
	m_iSimClass = iSimClass;

	int flags = m_flags;
	if (m_iLastLog<0 && m_pWorld->m_vars.lastTimeStep>0)
		m_flags |= pef_always_notify_on_deletion;
	CPhysicalEntity::AlertNeighbourhoodND(mode); 
	m_constraintMask = 0;
	m_flags = flags;
}


REdata g_REdata[MAX_PHYS_THREADS+1];

masktype CRigidEntity::MaskIgnoredColliders(int iCaller, int bScheduleForStep)
{
	ReadLock lock(m_lockColliders);
	int i;
	for(i=0; i<m_nColliders; i++)
		if (m_pColliders[i]->IgnoreCollisionsWith(this) && !(m_pColliders[i]->m_bProcessed & 1<<iCaller))
			AtomicAdd(&m_pColliders[i]->m_bProcessed, 1<<iCaller);
	for(i=0; i<NMASKBITS && getmask(i)<=m_constraintMask; i++) 
		if (m_constraintMask & getmask(i) && m_pConstraintInfos[i].flags & constraint_ignore_buddy && !(m_pConstraints[i].pent[1]->m_bProcessed & 1<<iCaller)) { 
			AtomicAdd(&m_pConstraints[i].pent[1]->m_bProcessed, 1<<iCaller);
			if (bScheduleForStep & inrange(m_pConstraints[i].pent[1]->m_iSimClass,2,5) && !(m_pConstraintInfos[i].flags & constraint_inactive))
				m_pWorld->ScheduleForStep(m_pConstraints[i].pent[1],m_lastTimeStep);
		}
	return m_constraintMask;
}
void CRigidEntity::UnmaskIgnoredColliders(masktype constraint_mask, int iCaller)
{
	ReadLock lock(m_lockColliders);
	int i;
	for(i=0;i<m_nColliders;i++) if (m_pColliders[i]->m_bProcessed & 1<<iCaller)
		AtomicAdd(&m_pColliders[i]->m_bProcessed, -(1<<iCaller));
	for(i=0; i<NMASKBITS && getmask(i)<=constraint_mask; i++) 
		if (constraint_mask & getmask(i) && m_pConstraintInfos[i].flags & constraint_ignore_buddy) 
			AtomicAdd(&m_pConstraints[i].pent[1]->m_bProcessed, -((int)m_pConstraints[i].pent[1]->m_bProcessed&1<<iCaller));
}

bool CRigidEntity::IgnoreCollisionsWith(const CPhysicalEntity *pent, int bCheckConstraints) const
{
	int i; 
	if (!m_pColliderConstraints)
		return false;
	for(i=0; i<NMASKBITS && getmask(i)<=m_constraintMask; i++) 
		if (m_constraintMask & getmask(i) && m_pConstraintInfos[i].flags & constraint_ignore_buddy && m_pConstraints[i].pent[1]==pent)
			return true;
	if (bCheckConstraints) {
		ReadLock lock(m_lockColliders);
		for(i=0;i<m_nColliders;i++) 
			if ((m_pColliderConstraints[i] || m_pColliders[i]->HasConstraintContactsWith(this)) && m_pColliders[i]->IgnoreCollisionsWith(pent,0))
				return true;
	}
	return false;
}

void CRigidEntity::FakeRayCollision(CPhysicalEntity *pent, float dt)
{
	ray_hit hit;
	int bHit;
	if (m_bSmallAndFastForced) {
		box bbox;
		if (m_nParts==1) {
			m_parts[0].pPhysGeom->pGeom->GetBBox(&bbox);
			bbox.center = m_qrot*(m_parts[0].q*bbox.center*m_parts[0].scale+m_parts[0].pos)+m_pos;
			bbox.Basis *= Matrix33(m_parts[0].q*m_qrot);
			bbox.size *= m_parts[0].scale;
		} else {
			bbox.center = (m_BBox[1]+m_BBox[0])*0.5f;
			bbox.Basis.SetIdentity();
			bbox.size = (m_BBox[1]-m_BBox[0])*0.5f;
		}
		bHit = m_pWorld->CollideEntityWithPrimitive(pent,box::type,&bbox, m_body.v*dt, &hit);
	} else 
		bHit = m_pWorld->RayTraceEntity(pent,m_body.pos,m_body.v*dt,&hit);
	if (bHit) {
		EventPhysCollision epc;
		pe_action_impulse ai;
		epc.pEntity[0]=this; epc.pForeignData[0]=m_pForeignData; epc.iForeignData[0]=m_iForeignData;
		epc.pEntity[1]=pent; epc.pForeignData[1]=pent->m_pForeignData; epc.iForeignData[1]=pent->m_iForeignData;
		epc.idCollider = m_pWorld->GetPhysicalEntityId(pent);
		epc.pt=hit.pt; epc.n=hit.n;
		epc.vloc[0]=m_body.v; epc.mass[0]=m_body.M;	epc.partid[0]=m_parts[0].id;
		epc.vloc[1].zero(); epc.mass[1]=pent->GetMass(hit.ipart); epc.partid[1]=hit.partid;
		epc.idmat[0] = GetMatId(m_parts[0].pPhysGeom->pGeom->GetPrimitiveId(0,0x40), 0);
		epc.idmat[1] = hit.surface_idx;
		epc.normImpulse = (hit.n*m_body.v)*-1.2f;
		epc.penetration=epc.radius = 0;
		epc.pEntContact = 0;
		m_pWorld->OnEvent(m_flags,&epc);

		ai.impulse = hit.n*epc.normImpulse;
		m_body.P = (m_body.v += ai.impulse)*m_body.M;
		ai.point = hit.pt;
		ai.ipart = hit.ipart;
		ai.impulse *= -m_body.M;
		ai.iApplyTime = 0;
		pent->Action(&ai,1);
	}
} 


inline int wait_for_ent(volatile CPhysicalEntity *pent) 
{ 
#if MAX_PHYS_THREADS>1
	for(; !(pent->m_bMoved|pent->m_flags>>31); ); 
#endif
	return 1; 
}


int CRigidEntity::GetPotentialColliders(CPhysicalEntity **&pentlist, float dt)
{
	int i,j,nents,bSameGroup;
	masktype constraint_mask;
	if (m_body.Minv+m_body.v.len2()+m_body.w.len2()<=0)
		return 0;
	int iCaller = get_iCaller_int();
	Vec3 BBox[2]={m_BBoxNew[0],m_BBoxNew[1]}, move=m_body.v*m_timeStepFull, inflator=Vec3(m_pWorld->m_vars.maxContactGap*4*isneg(m_iLastLog));
	BBox[0] += min(Vec3(ZERO),move); BBox[1] += max(Vec3(ZERO),move);
	BBox[0] -= inflator; BBox[1] += inflator;
	nents = m_pWorld->GetEntitiesAround(BBox[0],BBox[1], pentlist, m_collTypes|ent_sort_by_mass|ent_triggers, this, 0,iCaller);

	if (m_flags & ref_use_simple_solver) for(i=0;i<m_nColliders;i++) {
		entity_contact *pContact;
		for(j=0,pContact=m_pColliderContacts[i]; pContact && j<3; j++) 
			if (pContact->flags & contact_last) break;
		AtomicAdd(&m_pColliders[i]->m_bProcessed, 
			(m_pWorld->m_vars.bSkipRedundantColldet &
			iszero(m_pColliders[i]->m_nParts*2+m_nParts-3) &
			iszero(((CGeometry*)m_pColliders[i]->m_parts[0].pPhysGeomProxy->pGeom)->m_bIsConvex+((CGeometry*)m_parts[0].pPhysGeomProxy->pGeom)->m_bIsConvex-2) &
			isneg(3-j))<<iCaller);
	}
	constraint_mask = MaskIgnoredColliders(iCaller,1);

	for(i=j=m_nNonRigidNeighbours=0;i<nents;i++) {
		PREFAST_ASSUME(pentlist[i]);
		if (((pentlist[i]->m_bProcessed & 1<<iCaller)|IgnoreCollision(pentlist[i]->m_collisionClass,m_collisionClass))==0) {
			if (pentlist[i]->m_iSimClass>2) {
				if (pentlist[i]->GetType()!=PE_ARTICULATED) {
					pentlist[i]->Awake();
					if (/*pentlist[i]->m_iGroup!=m_iGroup &&*/ (!m_pWorld->m_vars.bMultiplayer || pentlist[i]->m_iSimClass!=3))
						m_pWorld->ScheduleForStep(pentlist[i],m_lastTimeStep),m_nNonRigidNeighbours++;
				}	else if (dt>0)
					FakeRayCollision(pentlist[i], dt);
			} else if (m_body.Minv<=0) {
				if (pentlist[i]!=this)
					pentlist[i]->Awake();
			} else if (pentlist[i]!=this && 
				((bSameGroup=iszero(pentlist[i]->m_iGroup-m_iGroup)) & pentlist[i]->m_bMoved || 
				pentlist[i]->m_iGroup==-1 || 
				!(pentlist[i]->IsAwake() | (m_flags&ref_use_simple_solver | pentlist[i]->m_flags&ref_use_simple_solver)&-bSameGroup) ||
				m_pWorld->m_pGroupNums[pentlist[i]->m_iGroup]<m_pWorld->m_pGroupNums[m_iGroup] && 
				 (max(m_body.v.len2(),((CRigidEntity*)pentlist[i])->m_body.v.len2())*sqr(dt) < sqr(m_pWorld->m_vars.maxContactGap*5) || 
					wait_for_ent(pentlist[i]))))
			{
				pentlist[j++] = pentlist[i];
				if (m_iLastLog<0)
					pentlist[i]->Awake();
			}
		}
	}

	if (m_flags & ref_use_simple_solver) for(i=0;i<m_nColliders;i++)
		AtomicAdd(&m_pColliders[i]->m_bProcessed, -((int)m_pColliders[i]->m_bProcessed&1<<iCaller));
	UnmaskIgnoredColliders(constraint_mask, iCaller);

	return j;
}


void CRigidEntity::ProcessContactEvents(geom_contact* pcontact, int i, int iCaller) 
{
	EventPhysCollision epc;
  RigidBody body(false),*pbody[2]; 
	int j, imask, ipt; 

	if (pcontact->parea) for(j=0;j<pcontact->parea->npt;j++) {
		imask = pcontact->parea->piPrim[0][j]>>31;
		(pcontact->parea->piPrim[0][j] &= ~imask) |= pcontact->iPrim[0] & imask;
		(pcontact->parea->piFeature[0][j] &= ~imask) |= (pcontact->iFeature[0] & imask) | imask&1<<31;
		imask = pcontact->parea->piPrim[1][j]>>31;
		(pcontact->parea->piPrim[1][j] &= ~imask) |= pcontact->iPrim[1] & imask;
		(pcontact->parea->piFeature[1][j] &= ~imask) |= (pcontact->iFeature[1] & imask) | imask&1<<31;
	}	
	if (pcontact->n*pcontact->dir>0.2f)
		pcontact->t = -1.0f;
	if ((m_parts[g_CurCollParts[i][0]].flags|g_CurColliders[i]->m_parts[g_CurCollParts[i][1]].flags) & (geom_manually_breakable|geom_no_coll_response) && 
		m_flags & pef_log_collisions & i-1>>31) 
	{
		epc.pEntity[0]=this; epc.pForeignData[0]=m_pForeignData; epc.iForeignData[0]=m_iForeignData;
		epc.pEntity[1]=g_CurColliders[i]; epc.pForeignData[1]=g_CurColliders[i]->m_pForeignData; epc.iForeignData[1]=g_CurColliders[i]->m_iForeignData;
		epc.pt = pcontact->pt;
		epc.n = -pcontact->n;
		epc.idCollider = m_pWorld->GetPhysicalEntityId(epc.pEntity[1]);
		epc.partid[0] = m_parts[g_CurCollParts[i][0]].id;
		epc.partid[1] = g_CurColliders[i]->m_parts[g_CurCollParts[i][1]].id;
		pbody[0] = GetRigidBody(g_CurCollParts[i][0]); 
		pbody[1] = g_CurColliders[i]->GetRigidBodyTrans(&body,g_CurCollParts[i][1],this); 
		for(ipt=0;ipt<2;ipt++) {
			epc.idmat[ipt] = pcontact->id[ipt];
			epc.vloc[ipt] = pbody[ipt]->v+(pbody[ipt]->w^epc.pt-pbody[ipt]->pos);
			epc.mass[ipt] = pbody[ipt]->M;
		}
		epc.penetration=epc.normImpulse=epc.radius = 0;
		if ((g_CurColliders[i]->m_parts[g_CurCollParts[i][1]].flags & (geom_manually_breakable|geom_no_coll_response))==geom_manually_breakable &&  
			!m_pWorld->SignalEvent(&epc,0))
			g_CurColliders[i]->m_parts[g_CurCollParts[i][1]].flags |= geom_no_coll_response;
		if (pcontact->vel*((m_parts[g_CurCollParts[i][0]].flags|g_CurColliders[i]->m_parts[g_CurCollParts[i][1]].flags) & geom_no_coll_response)>0) {
			pcontact->vel = 0;
			const Vec3 vel = m_body.v.normalized();
			Vec3 sz = (m_BBox[1]-m_BBox[0])*0.5f;
			Vec3 velVec(vel.x*vel.y*sz.x*sz.y+vel.x*vel.z*sz.x*sz.z, vel.y*vel.x*sz.y*sz.x+vel.y*vel.z*sz.y*sz.z,
				vel.z*vel.x*sz.z*sz.x+vel.z*vel.y*sz.z*sz.y);
			sz[idxmax3(velVec)] *= -1;
			sz.x*=sgnnz(vel.x); sz.y*=sgnnz(vel.y); sz.z*=sgnnz(vel.z);
			epc.radius = (vel^sz).len();
			if (g_CurColliders[i]->m_parts[g_CurCollParts[i][1]].flags & geom_no_coll_response && m_body.v.len2()>sqr(epc.radius*30.0f))
				m_pWorld->OnEvent(m_flags, &epc);
		}
	}
}

int CRigidEntity::CheckForNewContacts(geom_world_data *pgwd0,intersection_params *pip, int &itmax, Vec3 sweep, int iStartPart,int nParts, int *pFlagsAccum)
{
	CPhysicalEntity **pentlist;
	geom_world_data gwd1;
	int ient,nents,i,j,icont,ncontacts,ipt,nTotContacts=0,bHasMatSubst=0,ient1,j1,j2,bSquashy,isAwake,flagsAccum=0;
	int iCaller = get_iCaller_int();
	RigidBody body(false),*pbody[2];
	geom_contact *pcontacts;
	IGeometry *pGeom;
	CRayGeom aray;
	float tsg=!pip->bSweepTest ? 1.0f:-1.0f, tmax=1E10f*(1-tsg), vrel_min=pip->vrel_min;
	bool bStopAtFirstTri = pip->bStopAtFirstTri, bCheckBBox;
	Vec3 sz,BBox[2],BBox1[2],prevOutsidePivot=pip->ptOutsidePivot[0];
	EventPhysBBoxOverlap event;
	int bPivotFilled=isneg(pip->ptOutsidePivot[0].x-1E9f);
	pip->bThreadSafe = 1;
	box bbox;
	itmax = -1;
	nParts = m_nParts&nParts>>31 | max(nParts,0);
	event.pEntity[0]=this; event.pForeignData[0]=m_pForeignData; event.iForeignData[0]=m_iForeignData;

	nents = GetPotentialColliders(pentlist, pip->time_interval*((int)pip->bSweepTest & ~-iszero((int)m_flags & ref_small_and_fast)));
	pip->bKeepPrevContacts = false;

	for(i=iStartPart; i<iStartPart+nParts; i++) if (m_parts[i].flagsCollider) {
	  isAwake=IsAwake(i); 
		pgwd0->offset = m_pNewCoords->pos + m_pNewCoords->q*m_parts[i].pNewCoords->pos;
		pgwd0->R = Matrix33(m_pNewCoords->q*m_parts[i].pNewCoords->q);
		pgwd0->scale = m_parts[i].pNewCoords->scale;
		m_parts[i].pPhysGeomProxy->pGeom->GetBBox(&bbox);
		bbox.Basis *= pgwd0->R.T();
		sz = (bbox.size*bbox.Basis.Fabs())*m_parts[i].pNewCoords->scale;
		BBox[0] = BBox[1] = m_pNewCoords->pos + m_pNewCoords->q*(m_parts[i].pNewCoords->pos +
			m_parts[i].pNewCoords->q*bbox.center*m_parts[i].pNewCoords->scale);
		BBox[0] -= sz; BBox[1] += sz;
		BBox[isneg(-sweep.x)].x += sweep.x;	BBox[isneg(-sweep.y)].y += sweep.y;	BBox[isneg(-sweep.z)].z += sweep.z;
		pGeom = m_parts[i].pPhysGeomProxy->pGeom;
		if (!(m_parts[i].flags & geom_squashy))
			pip->vrel_min=vrel_min, bSquashy=0;
		else {
			if (!pip->bSweepTest)
				pip->vrel_min = 1E10f;
			else
				continue;
			if (m_timeCanopyFallen<3.0f) {
				ipt = idxmax3(m_gravity.abs());
				aray.m_dirn.zero()[ipt] = sgn(m_gravity[ipt]);
				aray.m_dirn = ((aray.m_dirn)*m_qNew)*m_parts[i].q;
				aray.m_ray.origin = m_parts[i].pPhysGeom->origin;
				Vec3 gloc = bbox.Basis.GetColumn(ipt);
				const Vec3 boxGloc(bbox.size.x*gloc.y*gloc.z, bbox.size.y*gloc.x*gloc.z, bbox.size.z*gloc.x*gloc.y);
				ipt = idxmin3(boxGloc);
				aray.m_ray.dir = aray.m_dirn*min((bbox.size.x+bbox.size.y+bbox.size.z)*(1.0f/3), bbox.size[ipt]/max(0.001f,gloc[ipt]));
				aray.m_ray.origin -= aray.m_ray.dir; aray.m_ray.dir *= 2;
				pGeom = &aray;
				if (m_pWorld->m_vars.iDrawHelpers & 64 && m_pWorld->m_pRenderer)
					m_pWorld->m_pRenderer->DrawLine(pgwd0->R*aray.m_ray.origin*pgwd0->scale+pgwd0->offset, 
					pgwd0->R*(aray.m_ray.origin+aray.m_ray.dir)*pgwd0->scale+pgwd0->offset, 7,1);
			}
			bSquashy = 1;
		}
	
		for(ient=0; ient<nents; ient++)
		for(j2=0,bCheckBBox=(pentlist[ient]->m_BBox[1]-pentlist[ient]->m_BBox[0]).len2()>0; 
				j2<(pentlist[ient]->GetUsedPartsCount(iCaller) & ~(-pentlist[ient]->m_iSimClass>>31 & -bSquashy)); j2++) 
		if ((pentlist[ient]->m_parts[j=pentlist[ient]->GetUsedPart(iCaller,j2)].flags & (m_parts[i].flagsCollider|geom_log_interactions)) && 
				!(pentlist[ient]==this && !CheckSelfCollision(i,j)) &&
				(m_nParts+pentlist[ient]->m_nParts==2 || 
				(isAwake || pentlist[ient]->IsAwake(j)) && (!bCheckBBox || AABB_overlap(BBox,pentlist[ient]->GetPartBBox(j,BBox1,this)))))
		{
			if (pentlist[ient]->m_parts[j].flags & geom_log_interactions) {
				event.pEntity[1]=pentlist[ient]; event.pForeignData[1]=pentlist[ient]->m_pForeignData; event.iForeignData[1]=pentlist[ient]->m_iForeignData;
				m_pWorld->OnEvent(m_flags, &event);
				if (!(pentlist[ient]->m_parts[j].flags & m_parts[i].flagsCollider))
					continue;
			}
			if (pentlist[ient]->m_parts[j].flags & geom_car_wheel && pentlist[ient]->GetMassInv()*5>m_body.Minv)
				continue;
			int bSameGroup = iszero(m_iGroup-pentlist[ient]->m_iGroup);
			if (bSameGroup & (int)pip->bSweepTest)
				continue;
			pentlist[ient]->GetPartTransform(j, gwd1.offset,gwd1.R,gwd1.scale, this);
			pbody[1] = pentlist[ient]->GetRigidBodyTrans(&body,j,this);
			gwd1.v = pbody[1]->v;
			gwd1.w = pbody[1]->w;
			gwd1.centerOfMass = pbody[1]->pos;
			pip->ptOutsidePivot[0] = m_parts[i].maxdim<pentlist[ient]->m_parts[j].maxdim ?
				prevOutsidePivot : Vec3(1E11f,1E11f,1E11f);
			/*if (!bPivotFilled && (m_bCollisionCulling || m_parts[i].pPhysGeomProxy->pGeom->IsConvex(0.1f))) {
				m_parts[i].pPhysGeomProxy->pGeom->GetBBox(&bbox);
				pip->ptOutsidePivot[0] = pgwd0->R*(bbox.center*pgwd0->scale)+pgwd0->offset;
			}*/
			//pip->bStopAtFirstTri = pentlist[ient]==this || bStopAtFirstTri || 
			//	m_parts[i].pPhysGeomProxy->pGeom->IsConvex(0.02f) && pentlist[ient]->m_parts[j].pPhysGeomProxy->pGeom->IsConvex(0.02f);

			if (pGeom!=&aray) {
				if (!(pentlist[ient]->m_BBox[0].len2()+pentlist[ient]->m_BBox[1].len2()) && pGeom->GetType()==GEOM_BOX) {	// box vs the terrain
					Vec2 coords[] = { Vec2(((BBox[1]+BBox[0])*0.5f-gwd1.offset)*gwd1.R), Vec2(((BBox[1]-BBox[0])*0.5f)*gwd1.R) }, frac(ZERO);
					float *pcoords = &coords[0].x;
					for(int ic=0;ic<4;ic++) 
						frac[ic&1] += fabs(pcoords[ic]-float2int(pcoords[ic]));
					if (min(frac.x,frac.y) < 0.001f) 
						pgwd0->scale *= 0.99f; // if either x or y bounds on the terrain are aligned with ints, perturb the scale a bit to avoid 'singularity'
				}
				ncontacts = ((CGeometry*)pGeom)->IntersectQueued(pentlist[ient]->m_parts[j].pPhysGeomProxy->pGeom, pgwd0,&gwd1, pip, pcontacts,
					this,pentlist[ient],i,j);
				#if MAX_PHYS_THREADS>1
				if ((ncontacts & ~-bSameGroup & -pentlist[ient]->m_iSimClass>>31) && 
						((CRigidEntity*)pentlist[ient])->m_body.M>0 && m_pWorld->m_nWorkerThreads>1) 
				{	
					if (pentlist[ient]->m_iGroup>=0 && m_pWorld->m_pGroupNums[pentlist[ient]->m_iGroup] < m_pWorld->m_pGroupNums[m_iGroup]) {
						wait_for_ent(pentlist[ient]); 
						g_nTotContacts -= ncontacts; 
						ncontacts = pGeom->Intersect(pentlist[ient]->m_parts[j].pPhysGeomProxy->pGeom, pgwd0,&gwd1, pip, pcontacts);
					}
					//{ ReadLockCond lockr(((CRigidEntity*)pentlist[ient])->m_lockStep,1); lockr.SetActive(0); }
					//g_LockedColliders[g_nLockedColliders++] = (CRigidEntity*)pentlist[ient];
				}
				#endif
				for(icont=0; icont<ncontacts; icont++) {
					pcontacts[icont].id[0] = GetMatId(pcontacts[icont].id[0],i);
					pcontacts[icont].id[1] = pentlist[ient]->GetMatId(pcontacts[icont].id[1],j);
					int scMask = (2<<pentlist[ient]->m_iSimClass)-(pentlist[ient]->m_id>>31);
					if (bHasMatSubst & scMask) for(ient1=0;ient1<nents;ient1++) for(int j3=0;j3<pentlist[ient1]->GetUsedPartsCount(iCaller);j3++)
						if (pentlist[ient1]->m_parts[j1=pentlist[ient1]->GetUsedPart(iCaller,j3)].flags & geom_mat_substitutor && 
								pentlist[ient1]->m_parts[j1].flagsCollider & scMask &&
								pentlist[ient1]->m_parts[j1].pPhysGeom->pGeom->PointInsideStatus(
									((pcontacts[icont].center-pentlist[ient1]->m_pos)*pentlist[ient1]->m_qrot-pentlist[ient1]->m_parts[j1].pos)*pentlist[ient1]->m_parts[j1].q))
						pcontacts[icont].id[1] = pentlist[ient1]->GetMatId(pentlist[ient1]->m_parts[j1].pPhysGeom->surface_idx,j1);
					g_CurColliders[nTotContacts] = pentlist[ient];
					g_CurCollParts[nTotContacts][0] = i;
					g_CurCollParts[nTotContacts][1] = j;
					ProcessContactEvents(&pcontacts[icont], icont, iCaller);
					if (pcontacts[icont].vel>0 && pcontacts[icont].t*tsg>tmax*tsg && !((m_parts[i].flags|pentlist[ient]->m_parts[j].flags) & geom_no_coll_response)) {
						tmax = pcontacts[icont].t; itmax = nTotContacts;
					}
					if (++nTotContacts==CRY_ARRAY_COUNT(g_CurColliders))
						goto CollidersNoMore;
				}
			} else {
				ncontacts = pentlist[ient]->m_parts[j].pPhysGeomProxy->pGeom->Intersect(pGeom, &gwd1,pgwd0, pip, pcontacts);
				for(icont=0; icont<ncontacts; icont++) {
					int imat0 = pentlist[ient]->GetMatId(pcontacts[icont].id[0],j);
					pcontacts[icont].id[0] = GetMatId(pcontacts[icont].id[1],i);
					pcontacts[icont].id[1] = imat0;
					g_CurColliders[nTotContacts] = pentlist[ient];
					g_CurCollParts[nTotContacts][0] = i;
					g_CurCollParts[nTotContacts][1] = j;
					pcontacts[icont].t = ((aray.m_ray.dir*aray.m_dirn)*pgwd0->scale-pcontacts[icont].t)*isneg(ncontacts-2-icont);
					pcontacts[icont].dir = pgwd0->R*-aray.m_dirn;
					if (++nTotContacts==CRY_ARRAY_COUNT(g_CurColliders))
						goto CollidersNoMore;
				}
			}
			flagsAccum |= m_parts[i].flags|pentlist[ient]->m_parts[j].flags;
			pip->bKeepPrevContacts = pip->bKeepPrevContacts || ncontacts>0;
		}	else
			bHasMatSubst |= pentlist[ient]->m_parts[j].flagsCollider & -((int)pentlist[ient]->m_parts[j].flags & geom_mat_substitutor)>>31;
	}
	CollidersNoMore:
	pip->bStopAtFirstTri = bStopAtFirstTri;
	pip->ptOutsidePivot[0] = prevOutsidePivot;
	pip->vrel_min = vrel_min;
	g_nLastContacts = nTotContacts;
	if (pFlagsAccum)
		*pFlagsAccum = flagsAccum;
	return nTotContacts;
}

void CRigidEntity::CleanupAfterContactsCheck(int iCaller)
{
	int i;
	for(i=0;i<g_nLastContacts;i++) 
		if ((g_CurColliders[i]->m_parts[g_CurCollParts[i][1]].flags & (geom_manually_breakable|geom_no_coll_response))==
				(geom_manually_breakable|geom_no_coll_response))
			g_CurColliders[i]->m_parts[g_CurCollParts[i][1]].flags &= ~geom_no_coll_response;
	//for(i=g_nLockedColliders-1; i>=0; i--)
	//	AtomicAdd(&g_LockedColliders[i]->m_lockStep, -1);
}


int CRigidEntity::RemoveContactPoint(CPhysicalEntity *pCollider, const Vec3 &pt, float mindist2)
{
	int i;
	for(i=0;i<m_nColliders && m_pColliders[i]!=pCollider;i++);
	if (i<m_nColliders) {
		entity_contact *pContact;
		for(pContact=m_pColliderContacts[i]; pContact && pContact!=CONTACT_END(m_pContactStart) && (pContact->pt[0]-pt).len2()>mindist2;
				pContact=pContact->next);
		if (pContact && pContact!=CONTACT_END(m_pContactStart)) {
			if (!(m_flags & ref_use_simple_solver))	{
				DetachContact(pContact,i); m_pWorld->FreeContact(pContact);
			}
			return 1;
		}
	}
	return 0;
}


entity_contact *CRigidEntity::RegisterContactPoint(int idx, const Vec3 &pt, const geom_contact *pcontacts, int iPrim0,int iFeature0, 
																									 int iPrim1,int iFeature1, int flags, float penetration, int iCaller, const Vec3 &nloc)
{
	//if (!(m_pWorld->m_vars.bUseDistanceContacts | m_flags&ref_use_simple_solver) && penetration==0 && GetType()!=PE_ARTICULATED)
	//	return 0;
	CRY_PROFILE_FUNCTION(PROFILE_PHYSICS );

	float min_dist2;
	bool bNoCollResponse=false;
	entity_contact *pContact;
	int j;
	const int bUseSimpleSolver=0;//iszero((int)m_flags&ref_use_simple_solver)^1;
	RigidBody body(false), *pbody1 = g_CurColliders[idx]->GetRigidBodyTrans(&body,g_CurCollParts[idx][1],this,1,true);
	//int iPrimCode = iPrim0|iFeature0<<8|iPrim1<<16|(iFeature1&0x7F)<<24;

	if (!((m_parts[g_CurCollParts[idx][0]].flags|g_CurColliders[idx]->m_parts[g_CurCollParts[idx][1]].flags) & geom_no_coll_response)) {
		if (!(m_parts[g_CurCollParts[idx][0]].flags & geom_squashy))
			min_dist2 = sqr(min(m_parts[g_CurCollParts[idx][0]].minContactDist, g_CurColliders[idx]->m_parts[g_CurCollParts[idx][1]].minContactDist));
		else {
			Vec3 sz = m_BBox[1]-m_BBox[0];
			min_dist2 = sqr(min(min(sz.x,sz.y),sz.z)*0.15f);
		}
		/*if (bUseSimpleSolver) {
			if (g_CurColliders[idx]->RemoveContactPoint(this,pt,min_dist2)==1)
				return 0;
		}	else if (m_pWorld->m_vars.bUseDistanceContacts)
			g_CurColliders[idx]->RemoveContactPoint(this,pt,min_dist2);*/

		for(pContact=m_pContactStart; pContact!=CONTACT_END(m_pContactStart) && 
				!((pContact->flags&contact_new || pContact->penetration==0) && 
				pContact->pent[1]==g_CurColliders[idx] && pContact->ipart[1]==g_CurCollParts[idx][1] &&
				(//(pContact->iPrimCode-iPrimCode|bUseSimpleSolver^1)==0 ||
				//((pContact->flags&contact_new) ? 
					(pContact->pt[0]-pt).len2()
					//: (pContact->pbody[0]->q*pContact->ptloc[0]+pContact->pbody[0]->pos-pt).len2()) 
					< min_dist2)); 
				pContact=pContact->next);
		if (pContact==CONTACT_END(m_pContactStart)) { // no existing point that is close enough to this one
			for(pContact=m_pContactStart,j=0; pContact!=CONTACT_END(m_pContactStart) && (pContact->flags&contact_new || pContact->penetration==0);
					pContact=pContact->next,j++);
			if (pContact!=CONTACT_END(m_pContactStart))
				DetachContact(pContact);
			else {
				if (j>=m_pWorld->m_vars.nMaxEntityContacts)
					return 0;
				pContact = m_pWorld->AllocContact();
			}
			AttachContact(pContact, AddCollider(g_CurColliders[idx]),g_CurColliders[idx]);
		} else if (bUseSimpleSolver || 
			(pContact->penetration==0 || pContact->flags&contact_new) && 
			(pContact->penetration>=penetration || flags & contact_2b_verified))
			return 0;
			//(!m_pWorld->m_vars.bUseDistanceContacts ? 	
			//(pContact->penetration==0 || pContact->flags&contact_new) :
			//(flags & contact_2b_verified && pContact->penetration==0))
	} else {
		bNoCollResponse = true;
		static entity_contact g_NoRespCnt[MAX_PHYS_THREADS+1];
		pContact = &g_NoRespCnt[iCaller];
	}

	pContact->pt[0] = pContact->pt[1] = pt;
	pContact->n = -pcontacts[idx].n;
	pContact->pent[0] = this;
	pContact->pent[1] = g_CurColliders[idx];
	pContact->ipart[0] = g_CurCollParts[idx][0];
	pContact->ipart[1] = g_CurCollParts[idx][1];
	pContact->pbody[0] = GetRigidBody(g_CurCollParts[idx][0]);
	pContact->pbody[1] = g_CurColliders[idx]->GetRigidBody(g_CurCollParts[idx][1],1);
	pContact->iConstraint = pcontacts[idx].iPrim[1];
	pContact->bConstraint = 0;
	pContact->penetration = penetration;

	Vec3 vrel = pContact->pbody[0]->v+(pContact->pbody[0]->w^pContact->pt[0]-pContact->pbody[0]->pos) - 
		pbody1->v-(pbody1->w^pContact->pt[0]-pbody1->pos);
	pContact->vrel = vrel*pContact->n;
	pContact->id0 = pcontacts[idx].id[0];
	pContact->id1 = pcontacts[idx].id[1];
	pContact->friction = m_pWorld->GetFriction(pcontacts[idx].id[0],pcontacts[idx].id[1], vrel.len2()>sqr(m_pWorld->m_vars.maxContactGap*5));
	pContact->friction *= iszero((int)pContact->pent[1]->m_parts[pContact->ipart[1]].flags & geom_car_wheel);
	pContact->friction = max(m_minFriction, pContact->friction);
	pContact->friction = min(min(m_pWorld->m_threadData[iCaller].maxGroupFriction, m_maxFriction), pContact->friction);
	pContact->flags = (pContact->flags & ~contact_archived) | (flags & ~contact_last);

	if (bNoCollResponse) {
		int bLastInGroup = flags & contact_last &&		NO_BUFFER_OVERRUN
			(idx==g_nLastContacts-1 || g_CurColliders[idx]!=g_CurColliders[idx+1] || g_CurCollParts[idx][1]!=g_CurCollParts[idx+1][1]);
		float r=0;
		if (g_idx0NoColl<0)
			g_idx0NoColl = idx;
		if (bLastInGroup)	{
			int i,npt;
			Vec3 center;
			for(i=g_idx0NoColl,center.zero(),npt=0; i<=idx; npt+=pcontacts[i++].nborderpt) for(j=0;j<pcontacts[i].nborderpt;j++)
				center += pcontacts[i].ptborder[j];
			if (npt) {
				for(i=g_idx0NoColl,center/=npt;i<=idx;i++) for(j=0;j<pcontacts[i].nborderpt;j++)
					r = max(r,(pcontacts[i].ptborder[j]-center).len());	
				pContact->pt[0] = pContact->pt[1] = center;
			}
			g_idx0NoColl = -1;
		}
		ArchiveContact(pContact,0,bLastInGroup,r);
		return 0;
	}

	/*if (bUseSimpleSolver) { 
		Vec3 ptloc[2],unproj=pcontacts[idx].dir*pcontacts[idx].t;
		pContact->iNormal = isneg((iFeature0&0xE0)-(iFeature1&0xE0));
		pContact->nloc = pContact->n*pContact->pbody[pContact->iNormal]->q;
		ptloc[0] = (pt-pContact->pbody[0]->pos-unproj)*pContact->pbody[0]->q;
		ptloc[1] = (pt-pContact->pbody[1]->pos)*pContact->pbody[1]->q;
		if (!(flags & contact_inexact)) {
			pContact->ptloc[0]=ptloc[0]; pContact->ptloc[1]=ptloc[1];
		}	else {
			Vec3 ptfeat[4];
			if (!(iFeature0&iFeature1&0xE0)) {
				j = pContact->iNormal;	// vertex-face contact
				geom *ppart = pContact->pent[j]->m_parts+pContact->ipart[j];
				// get geometry-CS coordinates of features in ptloc
				ppart->pPhysGeomProxy->pGeom->GetFeature(iPrim0&~-j|iPrim1&-j,iFeature0&~-j|iFeature1&-j, ptfeat); 
				// store world-CS coords in pContact->ptloc
				pContact->ptloc[j] = pContact->pent[j]->m_pNewCoords->q*(ppart->pNewCoords->q*ptfeat[0]*ppart->pNewCoords->scale +
					ppart->pNewCoords->pos)+pContact->pent[j]->m_pNewCoords->pos; 
				Vec3 nfeat = pContact->pent[j]->m_pNewCoords->q*ppart->pNewCoords->q*(ptfeat[1]-ptfeat[0] ^ ptfeat[2]-ptfeat[0]);
				ppart = pContact->pent[j^1]->m_parts+pContact->ipart[j^1];
				ppart->pPhysGeomProxy->pGeom->GetFeature(iPrim0&-j|iPrim1&~-j,iFeature0&-j|iFeature1&~-j, ptfeat); 
				pContact->ptloc[j^1] = pContact->pent[j^1]->m_pNewCoords->q*(ppart->pNewCoords->q*ptfeat[0]*ppart->pNewCoords->scale +
					ppart->pNewCoords->pos)+pContact->pent[j^1]->m_pNewCoords->pos; 
				pContact->ptloc[0] += unproj;
				pContact->ptloc[j] = pt-nfeat*((nfeat*(pt-pContact->ptloc[j]))/nfeat.len2());
				pContact->ptloc[0] = (pContact->ptloc[0]-pContact->pbody[0]->pos-unproj)*pContact->pbody[0]->q;
				pContact->ptloc[1] = (pContact->ptloc[1]-pContact->pbody[1]->pos)*pContact->pbody[1]->q;
			}	else { // edge-edge contact
				for(j=0;j<2;j++) {
					geom *ppart = pContact->pent[j]->m_parts+pContact->ipart[j];
					float rscale = ppart->scale==1.0f ? 1.0f : 1.0f/ppart->scale;
					// get edge in geom CS to ptloc[1]-ptloc[2]
					ppart->pPhysGeomProxy->pGeom->GetFeature(iPrim0&~-j|iPrim1&-j,iFeature0&~-j|iFeature1&-j, ptfeat+1);	
					ptfeat[0] = (((pt-pContact->pent[j]->m_pNewCoords->pos-unproj*(j^1))*pContact->pent[j]->m_pNewCoords->q-	
						ppart->pNewCoords->pos)*rscale)*ppart->pNewCoords->q;	// ptloc[0] <- contact point in geom CS
					ptfeat[0] = ptfeat[1] + (ptfeat[2]-ptfeat[1])*(((ptfeat[0]-ptfeat[1])*(ptfeat[2]-ptfeat[1]))/(ptfeat[2]-ptfeat[1]).len2());
					// transform geom CS->world CS
					ptfeat[0] = pContact->pent[j]->m_pNewCoords->q*(ppart->pNewCoords->q*ptfeat[0]*ppart->pNewCoords->scale +
						ppart->pNewCoords->pos)+pContact->pent[j]->m_pNewCoords->pos; 
					pContact->ptloc[j] = (ptfeat[0]-pContact->pbody[j]->pos)*pContact->pbody[j]->q;// world CS->body CS
				}
			}
			for(j=0;j<2 && (pContact->ptloc[j]-ptloc[j]).len2()<sqr(m_pWorld->m_vars.maxContactGapSimple);j++);
			if (j<2)
				pContact->ptloc[0]=ptloc[0], pContact->ptloc[1]=ptloc[1];
		}
		pContact->vreq.zero();
		UpdatePenaltyContact(pContact,m_lastTimeStep);
	} else*/ {
		pContact->Pspare = 0;
		//pContact->K.SetZero();
		//GetContactMatrix(pContact->pt[0], pContact->ipart[0], pContact->K);
		//g_CurColliders[idx]->GetContactMatrix(pContact->pt[1], pContact->ipart[1], pContact->K);

		float e = (m_pWorld->m_BouncinessTable[pcontacts[idx].id[0]&NSURFACETYPES-1] + 
							 m_pWorld->m_BouncinessTable[pcontacts[idx].id[1]&NSURFACETYPES-1])*0.5f;
		if (//m_body.M<2 &&	// bounce only smalll objects
				//m_nParts+g_CurColliders[idx]->m_nParts<=4 && // bounce only simple objects (bouncing is dangerous)
				//!bUseSimpleSolver && 
				e>0 && pContact->vrel<-m_pWorld->m_vars.minBounceSpeed && 
				(g_CurColliders[idx]->m_parts[pContact->ipart[1]].mass>m_body.M || 
				 !(g_CurColliders[idx]->m_parts[pContact->ipart[1]].flags & geom_monitor_contacts))) 
		{ // apply bounce impulse if needed
			pe_action_impulse ai;
			Matrix33 K; K.SetZero();
			pContact->pbody[0]->GetContactMatrix(pt-pContact->pbody[0]->pos, K);
			pbody1->GetContactMatrix(pt-pbody1->pos, K);
			ai.impulse.x = pContact->vrel*-(1+e)/max(pContact->n*K*pContact->n, (pContact->pbody[0]->Minv+pbody1->Minv)*0.1f);
			if (sqr(ai.impulse.x*pContact->pbody[0]->Minv) < max(pContact->pbody[0]->v.len2(),pbody1->v.len2())*sqr(e+1.1f)) {
				ArchiveContact(pContact, ai.impulse.x);
				ai.impulse = pContact->n*ai.impulse.x;
				ai.point = pContact->pt[0];
				ai.partid = m_parts[pContact->ipart[0]].id;
				ai.iApplyTime = 0; ai.iSource = 4;
				Action(&ai);
				ai.impulse.Flip(); ai.partid = g_CurColliders[idx]->m_parts[pContact->ipart[1]].id;
				g_CurColliders[idx]->Action(&ai,1);
			}
		}

		if (penetration>0) {
			Vec3 sz = (m_BBox[1]-m_BBox[0]);
			if (penetration>(sz.x+sz.y+sz.z)*0.06f)
				pContact->n = pcontacts[idx].dir;
			pContact->vreq = pContact->n*min(m_pWorld->m_vars.maxUnprojVel, max(0.0f,penetration-m_pWorld->m_vars.maxContactGap)*m_pWorld->m_vars.unprojVelScale);
			pContact->nloc = nloc;
		} else {
			//pContact->ptloc[0] = (pt-pContact->pbody[0]->pos)*pContact->pbody[0]->q;
			//pContact->ptloc[1] = (pt-pContact->pbody[1]->pos)*pContact->pbody[1]->q;
			pContact->iNormal = isneg((iFeature0&0xE0)-(iFeature1&0xE0));
			pContact->nloc = pContact->n*pContact->pbody[pContact->iNormal]->q;
			pContact->vreq.zero();
		}
	}

	return pContact;
}


void CRigidEntity::UpdatePenaltyContacts(float time_interval)
{
	//int i,nContacts,bResolveInstantly;
	entity_contact *pContact,*pContactNext;//*pContacts[16];
  
	//bResolveInstantly = /isneg((int)(CRY_ARRAY_COUNT(pContacts))-nContacts);
	m_bStable = max(m_bStable, (unsigned int)(isneg(2-m_nContacts)*2));
	for(pContact=m_pContactStart; pContact!=CONTACT_END(m_pContactStart); pContact=pContactNext)	{
		pContactNext = pContact->next;
		UpdatePenaltyContact(pContact, time_interval);//, bResolveInstantly,pContacts,nContacts);
		if (m_bStable && pContact->pent[1]->m_flags&ref_use_simple_solver)
			((CRigidEntity*)pContact->pent[1])->m_bStable = 2;
	}

	/*if ((bResolveInstantly^1) & -nContacts>>31) {
		int j,nBodies,iter;
		Vec3 r,dP,n,dP0;
		RigidBody *pBodies[17],*pbody;
		real pAp,a,b,r2,r2new;
		float dpn,dptang2,dPn,dPtang2;

		for(i=nBodies=0,r2=0;i<nContacts;i++) {
			for(j=0;j<2;j++) if (!pContacts[i]->pbody[j]->bProcessed)	{
				pBodies[nBodies++] = pContacts[i]->pbody[j]; pContacts[i]->pbody[j]->bProcessed = 1;
			}
			pContacts[i]->vreq = pContacts[i]->dP;
			(pContacts[i]->Kinv = pContacts[i]->K).Invert33();
			pContacts[i]->dP = pContacts[i]->Kinv*pContacts[i]->r0;
			r2 += pContacts[i]->dP*pContacts[i]->r0;
			pContacts[i]->P.zero();
		}
		for(i=0;i<nBodies;i++)
			pBodies[i]->bProcessed = 0;
		iter = nContacts;

		do {
			for(i=0; i<nBodies; i++) {
				pBodies[i]->Fcollision.zero(); pBodies[i]->Tcollision.zero();
			} for(i=0; i<nContacts; i++) for(j=0;j<2;j++) {
				r = pContacts[i]->pt[j]-pContacts[i]->pbody[j]->pos;
				pContacts[i]->pbody[j]->Fcollision += pContacts[i]->dP*(1-j*2); 
				pContacts[i]->pbody[j]->Tcollision += r^pContacts[i]->dP*(1-j*2);
			} for(i=0; i<nContacts; i++) for(pContacts[i]->vrel.zero(),j=0;j<2;j++) {
				pbody = pContacts[i]->pbody[j]; r = pContacts[i]->pt[j]-pbody->pos;
				pContacts[i]->vrel += (pbody->Fcollision*pbody->Minv + (pbody->Iinv*pbody->Tcollision^r))*(1-j*2);
			} for(i=0,pAp=0; i<nContacts; i++)
				pAp += pContacts[i]->vrel*pContacts[i]->dP;
			a = min((real)20.0,r2/max((real)1E-10,pAp));
			for(i=0,r2new=0; i<nContacts; i++) {
				pContacts[i]->vrel = pContacts[i]->Kinv*(pContacts[i]->r0 -= pContacts[i]->vrel*a);
				r2new += pContacts[i]->vrel*pContacts[i]->r0;
				pContacts[i]->P += pContacts[i]->dP*a;
			}
			b = min((real)1.0,r2new/r2); r2=r2new;
			for(i=0;i<nContacts;i++)
				(pContacts[i]->dP*=b) += pContacts[i]->vrel;
		} while(--iter && r2>sqr(0.03f));//m_Emin);

		for(i=0;i<nContacts;i++) {
			pContacts[i]->dP = pContacts[i]->vreq; pContacts[i]->vreq.zero();
			n = pContacts[i]->n;
			dpn = n*pContacts[i]->r; dptang2 = (pContacts[i]->r-n*dpn).len2();
			dP = pContacts[i]->P;
			//dP0 = (pContacts[i]->r*m_pWorld->m_vars.maxVel+pContacts[i]->vrel)*pContacts[i]->Kinv(0,0);
			//if (dP.len2()>dP0.len2()*sqr(1.5f))
			//	dP = dP0;
			dPn = n*dP; dPtang2 = (dP-n*dPn).len2();
			if (dptang2>sqr_signed(-dpn*pContacts[i]->friction) && dPtang2>sqr_signed(dPn*pContacts[i]->friction)) {
				dP = (dP-n*dPn).normalized()*(max(dPn,0.0f)*pContacts[i]->friction)+pContacts[i]->n*dPn;
				if (sqr(dpn*pContacts[i]->friction*2.5f) < dptang2) for(j=m_nColliders-1;j>=0;j--)
				if (!((m_pColliderContacts[j]&=~getmask(pContacts[i]->bProcessed)) | m_pColliderConstraints[j]) && !m_pColliders[j]->HasContactsWith(this)) {
					CPhysicalEntity *pCollider = m_pColliders[j]; 
					pCollider->RemoveCollider(this); RemoveCollider(pCollider);
				}
			}
			if (dP*n > min(0.0f,(pContacts[i]->dP*n)*-2.5f)) {
				pContacts[i]->dP = dP;
				for(j=0;j<2;j++,dP.flip()) {
					r = pContacts[i]->pt[j] - pContacts[i]->pbody[j]->pos;
					pContacts[i]->pbody[j]->P += dP;	pContacts[i]->pbody[j]->L += r^dP;
					pContacts[i]->pbody[j]->v = pContacts[i]->pbody[j]->P*pContacts[i]->pbody[j]->Minv, 
					pContacts[i]->pbody[j]->w = pContacts[i]->pbody[j]->Iinv*pContacts[i]->pbody[j]->L;
				}
			} else
				pContacts[i]->dP.zero();
		}
#ifdef _DEBUG
		for(i=0;i<nContacts;i++) {
			dP = pContacts[i]->pbody[0]->v+(pContacts[i]->pbody[0]->w^pContacts[i]->pt[0]-pContacts[i]->pbody[0]->pos);
			dP-= pContacts[i]->pbody[1]->v+(pContacts[i]->pbody[1]->w^pContacts[i]->pt[1]-pContacts[i]->pbody[1]->pos);
			r = pContacts[i]->r*m_pWorld->m_vars.maxVel; 
			a = r*dP; b = r.len2();	r2 = pContacts[i]->r0*r;
			b = a;
		}
#endif
	}*/
}

static float g_timeInterval=0.01f,g_rtimeInterval=100.0f;

int CRigidEntity::UpdatePenaltyContact(entity_contact *pContact, float time_interval)//, int bResolveInstantly,entity_contact **pContacts,int &nContacts)
{
	CRY_PROFILE_FUNCTION(PROFILE_PHYSICS );

	int j,bRemoveContact,bCanPull;
	Vec3 dp,n,vrel,dP,r;
	float dpn,dptang2,dPn,dPtang2;
	Matrix33 rmtx,rmtx1,K;
	
	if (g_timeInterval!=time_interval)
		g_rtimeInterval = 1.0f/(g_timeInterval=time_interval);

	//for(j=0; j<2; j++)
	//	pContact->pt[j] = pContact->pbody[j]->q*pContact->ptloc[j]+pContact->pbody[j]->pos;
	pContact->n=n = pContact->pbody[pContact->iNormal]->q*pContact->nloc;
	dp = pContact->pt[0]-pContact->pt[1];
	dpn = dp*n; dptang2 = (dp-n*dpn).len2();
	bRemoveContact = 0;
	bCanPull = 0;//isneg(m_impulseTime-1E-6f);

	if (dpn<m_pWorld->m_vars.maxContactGapSimple) {//,(pContact->r*n)*-2)) {
		//pContact->r = dp; 
		dp *= g_rtimeInterval*m_pWorld->m_vars.penaltyScale;
		vrel = pContact->pbody[0]->v+(pContact->pbody[0]->w^pContact->pt[0]-pContact->pbody[0]->pos);
		vrel-= pContact->pbody[1]->v+(pContact->pbody[1]->w^pContact->pt[1]-pContact->pbody[1]->pos);
		//pContact->vrel = vrel;
		dp += vrel;

		K.SetZero();
		for(j=0;j<2;j++) {
			r = pContact->pt[j] - pContact->pbody[j]->pos;
			((crossproduct_matrix(r,rmtx))*=pContact->pbody[j]->Iinv)*=crossproduct_matrix(r,rmtx1);
			K -= rmtx; 
			for(int idx=0;idx<3;idx++)
				K(idx,idx) += pContact->pbody[j]->Minv;
		}

		//(pContact->Kinv = pContact->K).Invert33();
		//dP = pContact->Kinv*-dp;
		dP = dp*(-dp.len2()/(dp*K*dp));
		dPn = dP*n; dPtang2 = (dP-n*dPn).len2();
		if (dptang2>sqr_signed(-dpn*pContact->friction)*bCanPull && dPtang2>sqr_signed(dPn*pContact->friction)) {
			dP = (dP-n*dPn).normalized()*(max(dPn,0.0f)*pContact->friction)+n*dPn;
			bRemoveContact = isneg(sqr(m_pWorld->m_vars.maxContactGapSimple)-dptang2);
		}
		if (dP*n > min(0.0f,(pContact->vreq*n)*-2.1f*bCanPull)) {
			pContact->vreq = dP;
			for(j=0;j<2;j++,dP.Flip()) {
				r = pContact->pt[j] - pContact->pbody[j]->pos;
				pContact->pbody[j]->P += dP;	pContact->pbody[j]->L += r^dP;
				pContact->pbody[j]->v = pContact->pbody[j]->P*pContact->pbody[j]->Minv; 
				pContact->pbody[j]->w = pContact->pbody[j]->Iinv*pContact->pbody[j]->L;
			}
		} else
			pContact->vreq.zero();
	}	else 
		bRemoveContact = 1;
	
	if (bRemoveContact) {
		DetachContact(pContact); m_pWorld->FreeContact(pContact);
	}

	return bRemoveContact;
}


int CRigidEntity::RegisterConstraint(const Vec3 &pt0,const Vec3 &pt1, int ipart0, CPhysicalEntity *pBuddy,int ipart1, int flags,int flagsInfo, CPhysicalEntity *pConstraintEnt)
{
	int i;

	for(i=0; i<NMASKBITS && m_constraintMask & getmask(i); i++);
	if (i==NMASKBITS) return -1;
	if (!pBuddy) pBuddy = &g_StaticPhysicalEntity;

	if (i>=m_nConstraintsAlloc) {
		entity_contact *pConstraints = m_pConstraints;
		constraint_info *pInfos = m_pConstraintInfos;
		int nConstraints = m_nConstraintsAlloc;	// cppcheck-suppress memsetClass
		memcpy(m_pConstraints = new entity_contact[(m_nConstraintsAlloc=(i&~7)+8)], pConstraints, nConstraints*sizeof(entity_contact));
		memcpy(m_pConstraintInfos = new constraint_info[m_nConstraintsAlloc], pInfos, nConstraints*sizeof(constraint_info));
		delete[] pConstraints;
		delete[] pInfos;
	}
	//michaelg: had to split the access to m_pColliderConstraints, was compiler bug in gcc
	const int buddyCollider = AddCollider(pBuddy);
	m_pColliderConstraints[buddyCollider] |= getmask(i);
	m_constraintMask |= getmask(i);
	
	entity_contact& constraint = m_pConstraints[i];

	const Vec3 nv(0, 0, 1);
	constraint.pt[0] = pt0;
	constraint.pt[1] = pt1;
	constraint.nloc = nv;
	constraint.n = nv;
	constraint.iNormal = 0;
	constraint.pent[0] = this;
	constraint.pent[1] = pBuddy;
	constraint.ipart[0] = ipart0;
	constraint.ipart[1] = ipart1;
	constraint.pbody[0] = GetRigidBody(ipart0);
	constraint.pbody[1] = pBuddy->GetRigidBody(ipart1, iszero(flagsInfo & constraint_inactive));
	QuatT frames[2]; GetContactFrames(constraint, frames);
	m_pConstraintInfos[i].ptloc[0] = frames[0].GetInverted()*constraint.pt[0];
	m_pConstraintInfos[i].ptloc[1] = frames[1].GetInverted()*constraint.pt[1];

	constraint.vrel = 0;
	constraint.friction = 0;
	constraint.flags = flags;
	constraint.iConstraint = i+1;
	constraint.bConstraint = 1;
	constraint.Pspare = 0;
	constraint.vreq.zero();

	m_pConstraintInfos[i].flags = flagsInfo;
	m_pConstraintInfos[i].damping = 0;
	m_pConstraintInfos[i].pConstraintEnt = pConstraintEnt;
	m_pConstraintInfos[i].bActive = 0;
	m_pConstraintInfos[i].sensorRadius = 0.05f;
	m_pConstraintInfos[i].limit = 0;
	m_pConstraintInfos[i].hardness = flags & contact_angular ? 20.0f : m_pWorld->m_vars.unprojVelScale;

	return i;
}


int CRigidEntity::RemoveConstraint(int iConstraint)
{
	int i;
	for(i=0;i<m_nColliders && !(m_pColliderConstraints[i] & getmask(iConstraint));i++);
	if (i==m_nColliders)
		return 0;
	m_constraintMask &= ~getmask(iConstraint);
	if (!((m_pColliderConstraints[i]&=~getmask(iConstraint)) || m_pColliderContacts[i]) && !m_pColliders[i]->HasContactsWith(this))
		RemoveCollider(m_pColliders[i]);
	return 1;
}


void CRigidEntity::VerifyExistingContacts(float maxdist)
{
	int i,bConfirmed,nRemoved=0;
	geom_world_data gwd;
	Vec3 ptres[2],n,ptclosest[2];
	entity_contact *pContact,*pContactNext;
	{ ReadLock lock(m_lockColliders);

	// verify all existing contacts with dynamic entities
	for(i=0;i<m_nColliders;i++) //if (m_bAwake+m_pColliders[i]->IsAwake()>0)
	if (pContact=m_pColliderContacts[i]) for(;;pContact=pContact->next) {
		if ((pContact->flags & contact_new)==0 && IsAwake(pContact->ipart[0])+m_pColliders[i]->IsAwake(pContact->ipart[1])>0) {
			bConfirmed = 0;
			/*if (pContact->penetration==0) {
				ptres[0].Set(1E9f,1E9f,1E9f); ptres[1].Set(1E11f,1E11f,1E11f);

				pent = pContact->pent[0]; ipart = pContact->ipart[0];
				//(pent->m_qrot*pent->m_parts[ipart].q).getmatrix(gwd.R);	//Q2M_IVO
				gwd.R = Matrix33(pent->m_pNewCoords->q*pent->m_parts[ipart].pNewCoords->q);
				gwd.offset = pent->m_pNewCoords->pos + pent->m_pNewCoords->q*pent->m_parts[ipart].pNewCoords->pos;
				gwd.scale = pent->m_parts[ipart].pNewCoords->scale;
				if ((pContact->iFeature[0]&0xFFFFFF60)!=0) {
					pbody = pContact->pbody[1]; 
					ptres[1] = pbody->q*pContact->ptloc[1]+pbody->pos;
					if (pent->m_parts[ipart].pPhysGeomProxy->pGeom->FindClosestPoint(&gwd, pContact->iPrim[0],pContact->iFeature[0], 
						ptres[1],ptres[1], ptclosest)<0)
						goto endcheck;
					ptres[0] = ptclosest[0];
				} else {
					pent->m_parts[ipart].pPhysGeomProxy->pGeom->GetFeature(pContact->iPrim[0],pContact->iFeature[0], ptres);
					ptres[0] = gwd.R*(ptres[0]*gwd.scale)+gwd.offset;
				}
				pbody = pContact->pbody[0]; pContact->ptloc[0] = (ptres[0]-pbody->pos)*pbody->q;
				pContact->pt[0] = pContact->pt[1] = ptres[0];

				pent = pContact->pent[1]; ipart = pContact->ipart[1];
				//(pent->m_qrot*pent->m_parts[ipart].q).getmatrix(gwd.R);	//Q2M_IVO
				gwd.R = Matrix33(pent->m_pNewCoords->q*pent->m_parts[ipart].pNewCoords->q);
				gwd.offset = pent->m_pNewCoords->pos + pent->m_pNewCoords->q*pent->m_parts[ipart].pNewCoords->pos;
				gwd.scale = pent->m_parts[ipart].pNewCoords->scale;
				if ((pContact->iFeature[1]&0xFFFFFF60)!=0) {
					if (pent->m_parts[ipart].pPhysGeomProxy->pGeom->FindClosestPoint(&gwd, pContact->iPrim[1],pContact->iFeature[1], 
						ptres[0],ptres[0], ptclosest)<0)
						goto endcheck;
					ptres[1] = ptclosest[0];
				} else {
					pent->m_parts[ipart].pPhysGeomProxy->pGeom->GetFeature(pContact->iPrim[1],pContact->iFeature[1], ptres+1);
					ptres[1] = gwd.R*(ptres[1]*gwd.scale)+gwd.offset;
				}
				pbody = pContact->pbody[1]; pContact->ptloc[1] = (ptres[1]-pbody->pos)*pbody->q;

				n = (ptres[0]-ptres[1]).normalized();
				if (pContact->n*n>0.98f) {
					pContact->n = n; bConfirmed = 1;
				} else 
					bConfirmed = iszero(pContact->flags & contact_2b_verified);
				pContact->iNormal = isneg((pContact->iFeature[0]&0x60)-(pContact->iFeature[1]&0x60));
				pContact->nloc = pContact->n*pContact->pbody[pContact->iNormal]->q;
				pContact->K.SetZero();
				pContact->pent[0]->GetContactMatrix(pContact->pt[0], pContact->ipart[0], pContact->K);
				pContact->pent[1]->GetContactMatrix(pContact->pt[1], pContact->ipart[1], pContact->K);

				pContact->vrel = pContact->pbody[0]->v+(pContact->pbody[0]->w^pContact->pt[0]-pContact->pbody[0]->pos) - 
					pContact->pbody[1]->v-(pContact->pbody[1]->w^pContact->pt[0]-pContact->pbody[1]->pos);
				pContact->friction = m_pWorld->GetFriction(pContact->id[0],pContact->id[1], pContact->vrel.len2()>sqr(m_pWorld->m_vars.maxContactGap*5));

				bConfirmed &= isneg((ptres[0]-ptres[1]).len2()-sqr(maxdist));
				if (iszero(pContact->iFeature[0]&0x60) | iszero(pContact->iFeature[0]&0x60)) {
					// if at least one contact point is at geometry vertex, make sure there are no other contacts for the same combination of features
					icode0 = pContact->ipart[0]<<24 | pContact->iPrim[0]<<7 | pContact->iFeature[0]&0x7F;
					icode1 = pContact->ipart[1]<<24 | pContact->iPrim[1]<<7 | pContact->iFeature[1]&0x7F;
					for(pContact1=m_pColliderContacts[i]; pContact1!=pContact; pContact1=pContact1->next) 
					if (icode0 == pContact1->ipart[0]<<24 | pContact1->iPrim[0]<<7 | pContact1->iFeature[0]&0x7F &&
							icode1 == pContact1->ipart[1]<<24 | pContact1->iPrim[1]<<7 | pContact1->iFeature[1]&0x7F)
					{ bConfirmed = 0; break; }
				}
			}

			endcheck:*/
			if (!bConfirmed) {
				pContact->flags |= contact_remove; nRemoved++;
			}
		}
		if ((pContact->flags &= ~(contact_new | contact_2b_verified | contact_archived)) & contact_last)
			break;
	}
	} // lock colliders

	if (nRemoved) 
		for(pContact=m_pContactStart; pContact!=CONTACT_END(m_pContactStart); pContact=pContactNext) {
			pContactNext=pContact->next; 
			if (pContact->flags & contact_remove)	{
				DetachContact(pContact); m_pWorld->FreeContact(pContact);
			}
		}
}


void CRigidEntity::OnContactResolved(entity_contact *pContact, int iop, int iGroupId)
{
	int i,j;
	if (iop==0 && pContact->bConstraint && m_pConstraintInfos[j=pContact->iConstraint-1].limit>0 && 
			!(m_pConstraintInfos[j].flags & constraint_no_tears) &&
			pContact->Pspare>m_pConstraintInfos[j].limit*max(0.01f,m_lastTimeStep)) 
	{	
		for(i=0;i<NMASKBITS && getmask(i)<=m_constraintMask;i++) 
			if (m_constraintMask & getmask(i) && m_pConstraintInfos[i].id==m_pConstraintInfos[j].id) {
				(m_pConstraintInfos[i].flags |= constraint_inactive|constraint_broken) &= ~constraint_ignore_buddy;

				if (!(m_pConstraintInfos[i].flags & constraint_rope)) {
					EventPhysJointBroken epjb;
					epjb.idJoint=m_pConstraintInfos[i].id; epjb.bJoint=0; epjb.pNewEntity[0]=epjb.pNewEntity[1] = 0;
					for(int idx=0;idx<2;idx++) {
						epjb.pEntity[idx] = m_pConstraints[i].pent[idx]; 
						epjb.pForeignData[idx] = m_pConstraints[i].pent[idx]->m_pForeignData;
						epjb.iForeignData[idx] = m_pConstraints[i].pent[idx]->m_iForeignData;
						epjb.partid[idx] = m_pConstraints[i].pent[idx]->m_parts[m_pConstraints[i].ipart[idx]].id;
					}
					epjb.pt = m_pConstraints[i].pt[0];
					epjb.n = m_pConstraints[i].n;
					epjb.partmat[0]=epjb.partmat[1] = -1;
					m_pWorld->OnEvent(2,&epjb);
				}
			}
		BreakableConstraintsUpdated();
	}
	CPhysicalEntity::OnContactResolved(pContact,iop,iGroupId);
}


void CRigidEntity::BreakableConstraintsUpdated()
{
	int i;
	if (GetType()!=PE_WHEELEDVEHICLE)
		for(i=0;i<m_nParts;i++) if (!m_parts[i].pLattice && !m_pStructure)
			m_parts[i].flags &= ~geom_monitor_contacts;
	for(i=0;i<NMASKBITS && getmask(i)<=m_constraintMask;i++) if (m_constraintMask & getmask(i) && m_pConstraintInfos[i].limit>0)
		m_parts[m_pConstraints[i].ipart[0]].flags |= geom_monitor_contacts;
}


void CRigidEntity::UpdateConstraints(float time_interval)
{
	int i; 
	Ang3 angles;
	Vec3 drift,dw,dL;
	quaternionf qframe0,qframe1;

	for(i=0;i<NMASKBITS && getmask(i)<=m_constraintMask;i++) if (m_constraintMask & getmask(i)) 
	if (!(m_pConstraintInfos[i].flags & constraint_inactive)) {
		int bSimple = m_pConstraints[i].pent[1]->m_iSimClass>3 && m_pConstraints[i].pent[1]->GetType()!=PE_ARTICULATED;
		QuatT frames[2]; GetContactFrames(m_pConstraints[i], frames, 2-bSimple);
		m_pConstraints[i].pt[0] = frames[0]*m_pConstraintInfos[i].ptloc[0];
		if (bSimple) {
			pe_params_pos ppos;
			ppos.q = frames[0].q*m_pConstraintInfos[i].qframe_rel[0]*!m_pConstraintInfos[i].qframe_rel[1];
			ppos.pos = m_pConstraints[i].pt[0] - ppos.q*m_pConstraintInfos[i].ptloc[1];
			m_pConstraints[i].pent[1]->SetParams(&ppos);
			m_pConstraintInfos[i].bActive = 0;
			continue;
		}
		int flagsForce = 0;
		if (m_pConstraintInfos[i].flags & constraint_area && m_pConstraintInfos[i].pConstraintEnt) {
			pe_status_area sa; sa.ptClosest = m_pConstraints[i].pt[0];
			m_pConstraintInfos[i].pConstraintEnt->GetStatus(&sa);
			m_pConstraints[i].pt[1] = sa.ptClosest;
			m_pConstraintInfos[i].qframe_rel[1] = !m_pConstraints[i].pbody[1]->q*Quat::CreateRotationV0V1(ortx,sa.dirClosest);
			m_pConstraints[i].flags &= ~contact_constraint_3dof	| -iszero(m_pConstraints[i].flags & (contact_constraint_1dof|contact_constraint_2dof));
			if (!(m_pConstraints[i].flags & (contact_angular|contact_constraint_3dof)) && 
					(m_pConstraints[i].flags & contact_constraint_2dof || sa.dirClosest.len2()>1.01f) &&
					(m_pConstraints[i].pt[0]-m_pConstraints[i].pt[1]).len2()>m_body.v.len2()*sqr(m_maxAllowedStep*2))
				m_pConstraints[i].flags |= contact_constraint_3dof;
			m_pConstraints[i].nloc = !frames[FrameOwner(m_pConstraints[i])].q*(sa.dirClosest.len2()>1.01f ? sa.dirClosest.normalized():sa.dirClosest);
		}	else
			m_pConstraints[i].pt[1] = frames[1]*m_pConstraintInfos[i].ptloc[1];

		int bActive = m_pConstraintInfos[i].bActive;
		drift = m_pConstraints[i].pt[1]-m_pConstraints[i].pt[0];
		m_pConstraintInfos[i].bActive = 1;
		m_pConstraints[i].iConstraint = i+1;
		m_pConstraints[i].bConstraint = 1;
		m_pConstraints[i].Pspare = m_pConstraintInfos[i].limit*max(0.01f,m_lastTimeStep);
		m_pConstraints[i].flags = m_pConstraints[i].flags & ~contact_preserve_Pspare | contact_preserve_Pspare & -isneg(1e-6f-m_pConstraints[i].Pspare);

		if (m_pConstraintInfos[i].flags & constraint_rope) {
			pe_params_rope pr;
			m_pConstraintInfos[i].pConstraintEnt->GetParams(&pr);
			if (drift.len2() > sqr(pr.length)) {
				m_pConstraints[i].n = drift.normalized();
				if (drift.len2() > sqr(pr.length*1.005f))
					m_pConstraints[i].vreq = (drift-m_pConstraints[i].n*(pr.length*1.005f))*5.0f;
				else m_pConstraints[i].vreq.zero();
			} else
				m_pConstraintInfos[i].bActive = 0;
			pe_simulation_params sp;
			m_pConstraintInfos[i].pConstraintEnt->GetParams(&sp);
			m_dampingEx = max(m_dampingEx, sp.damping);
			m_pConstraintInfos[i].pConstraintEnt->Awake();
			m_pConstraints[i].pbody[0]->L -= m_pConstraints[i].n*((m_pConstraints[i].n*m_pConstraints[i].pbody[0]->L)*
																														min(0.9f,time_interval*m_pConstraintInfos[i].damping));
			m_pConstraints[i].pbody[0]->w = m_pConstraints[i].pbody[0]->Iinv*m_pConstraints[i].pbody[0]->L;
		}	else if (!(m_pConstraints[i].flags & contact_angular)) {
			int iframe = FrameOwner(m_pConstraints[i]);
			m_pConstraints[i].vreq = drift*m_pConstraintInfos[i].hardness;
			if (m_pConstraints[i].flags & contact_constraint_3dof) {
				m_pConstraints[i].n = drift.normalized();
				//m_pConstraints[i].vsep = (m_pConstraints[i].vreq*m_pConstraints[i].n)*0.5f;
			}	else if (m_pConstraints[i].flags & contact_constraint_1dof) {
				m_pConstraints[i].n = frames[iframe].q*m_pConstraints[i].nloc;
				m_pConstraints[i].vreq -= m_pConstraints[i].n*(m_pConstraints[i].n*m_pConstraints[i].vreq);
			}	else if (m_pConstraints[i].flags & contact_constraint_2dof | m_pConstraintInfos[i].flags & constraint_limited_1axis) {
				m_pConstraints[i].n = frames[iframe].q*m_pConstraints[i].nloc;
				m_pConstraints[i].vreq = m_pConstraints[i].n*(m_pConstraints[i].vreq*m_pConstraints[i].n);
				if (m_pConstraintInfos[i].flags & constraint_limited_1axis && 
					 (m_pConstraintInfos[i].bActive = !inrange(angles.x=-(drift*m_pConstraints[i].n),m_pConstraintInfos[i].limits[0],m_pConstraintInfos[i].limits[1])))
				{ int iside = sgnnz(m_pConstraintInfos[i].limits[0]+m_pConstraintInfos[i].limits[1]-angles.x*2);
					m_pConstraints[i].vreq += m_pConstraints[i].n*(m_pConstraintInfos[i].hardness*m_pConstraintInfos[i].limits[1-iside>>1]);
					m_pConstraints[i].n *= iside;
				}
			}
		} else if (m_pConstraints[i].flags & contact_angular) {
			int iframe = FrameOwner(m_pConstraints[i]);
			m_pConstraints[i].n = frames[iframe].q*m_pConstraints[i].nloc;
			qframe0 = frames[0].q*m_pConstraintInfos[i].qframe_rel[0];
			qframe1 = frames[1].q*m_pConstraintInfos[i].qframe_rel[1];
			dw = m_pConstraints[i].pbody[0]->w-m_pConstraints[i].pbody[1]->w;

			if (m_pConstraints[i].flags & contact_constraint_3dof) {
				Quat dq = qframe1*!qframe0;
				m_pConstraints[i].vreq = dq.v*sgnnz(dq.w)*m_pConstraintInfos[i].hardness;
			} else if (m_pConstraints[i].flags & contact_constraint_2dof | m_pConstraintInfos[i].flags & constraint_limited_1axis) {
				angles = Ang3::GetAnglesXYZ(Matrix33(!qframe1*qframe0));
				if (!(m_pConstraintInfos[i].flags & (constraint_limited_1axis|constraint_limited_2axes)))	{
					dw -= m_pConstraints[i].n*(m_pConstraints[i].n*dw);
					m_pConstraints[i].vreq = m_pConstraints[i].n*(-angles.x*m_pConstraintInfos[i].hardness);
				} else {
					dw = m_pConstraints[i].n*(m_pConstraints[i].n*dw);
					if (angles.x < m_pConstraintInfos[i].limits[0])
						m_pConstraints[i].vreq = m_pConstraints[i].n*min(5.0f,(m_pConstraintInfos[i].limits[0]-angles.x)*m_pConstraintInfos[i].hardness);
					else if (angles.x > m_pConstraintInfos[i].limits[1]) {
						m_pConstraints[i].n.Flip();
						m_pConstraints[i].vreq = m_pConstraints[i].n*min(5.0f,(angles.x-m_pConstraintInfos[i].limits[1])*m_pConstraintInfos[i].hardness);
					} else
						m_pConstraintInfos[i].bActive = 0;
				}
			} else if (m_pConstraints[i].flags & contact_constraint_1dof | m_pConstraintInfos[i].flags & constraint_limited_2axes) {
				Vec3 xaxis = qframe1*Vec3(1-iframe,0,0) + qframe0*Vec3(-iframe,0,0);
				float cosa = xaxis*m_pConstraints[i].n*(1-iframe*2);
				drift = m_pConstraints[i].n ^ xaxis;
				if (!(m_pConstraintInfos[i].flags & (constraint_limited_1axis|constraint_limited_2axes)))	{
					if (drift.len2()>1.0f)
						drift.normalize();
					m_pConstraints[i].vreq = drift*m_pConstraintInfos[i].hardness;
				} else if (drift.len2()*fabs_tpl(cosa) > sqr(m_pConstraintInfos[i].limits[0])*cosa)
					m_pConstraints[i].vreq = (m_pConstraints[i].n=drift.normalized())*min(5.0f,(acos_tpl(cosa)-m_pConstraintInfos[i].limits[1])*m_pConstraintInfos[i].hardness);
				else
					m_pConstraintInfos[i].bActive = 0;
			}
		}

		bool hitLimit = m_pConstraintInfos[i].flags & (constraint_limited_1axis|constraint_limited_2axes) && 
			m_pConstraintInfos[i].bActive && !bActive && m_flags & (pef_monitor_collisions | pef_log_collisions);
		bool hasDamping = m_pConstraintInfos[i].bActive && m_pConstraintInfos[i].damping;
		RigidBody *pbody0=m_pConstraints[i].pbody[0], *pbody1=m_pConstraints[i].pbody[1];
		Vec3 r0,r1;
		if (hitLimit || hasDamping) {
			if (m_pConstraints[i].flags & contact_angular){
				Matrix33 K = pbody0->Iinv+pbody1->Iinv;
				dL = K.GetInverted()*dw;
			}	else {
				Matrix33 K; K.SetZero();
				pbody0->GetContactMatrix(r0=m_pConstraints[i].pt[0]-pbody0->pos, K);
				pbody1->GetContactMatrix(r1=m_pConstraints[i].pt[1]-pbody1->pos, K);
				dw = pbody0->v+(pbody0->w^r0)-pbody1->v-(pbody1->w^r1);
				dL = dw/(m_pConstraints[i].n*K*m_pConstraints[i].n);
			}
		}
		if (hitLimit)	{
			EventPhysCollision epc;
			epc.pt = m_pConstraints[i].pt[0];
			epc.normImpulse = dL*(epc.n = m_pConstraints[i].n);
			for(int j=0;j<2;j++) {
				CPhysicalEntity *pent = m_pConstraints[i].pent[j];
				RigidBody *pbody = m_pConstraints[i].pbody[j];
				epc.pEntity[j]=pent; epc.pForeignData[j]=pent->m_pForeignData; epc.iForeignData[j]=pent->m_iForeignData;
				epc.partid[j] = pent->m_parts[m_pConstraints[i].ipart[j]].id;
				epc.vloc[j] = m_pConstraints[i].flags & contact_angular ? pbody->w : pbody->v+(pbody->w^epc.pt-pbody->pos);
				epc.idmat[j] = pent->m_nParts ? pent->GetMatId(pent->m_parts[m_pConstraints[i].ipart[j]].pPhysGeom->pGeom->GetPrimitiveId(0,0x40),m_pConstraints[i].ipart[j]):0;
			}
			epc.idCollider = m_pConstraints[i].pent[1]->m_id;
			epc.mass[0] = m_body.M;
			epc.mass[1] = m_pConstraints[i].pent[1]->GetMass(m_pConstraints[i].ipart[1]);
			epc.penetration=epc.radius = 0;
			m_pWorld->OnEvent(m_flags, &epc);
		}
		if (hasDamping) {
			if (m_pConstraints[i].flags & contact_angular){
				dL *= -min(0.9f,time_interval*m_pConstraintInfos[i].damping);
				pbody0->L += dL; pbody1->L -= dL; 
				pbody0->w = pbody0->Iinv*pbody0->L;
				pbody1->w = pbody1->Iinv*pbody1->L;	
			} else if (!(m_pConstraintInfos[i].flags & constraint_rope)) {
				dL *= -min(0.9f,time_interval*m_pConstraintInfos[i].damping);
				pbody0->v = (pbody0->P+=dL)*pbody0->Minv; pbody0->w = pbody0->Iinv*(pbody0->L+=r0^dL);
				dL *= pbody1->Minv*pbody1->M;
				pbody1->P -= dL; pbody1->v -= dL*pbody1->Minv; 
				dL = r1^dL;
				pbody1->L -= dL; pbody1->w -= pbody1->Iinv*dL;
			}
		}
	} else
		m_pConstraintInfos[i].bActive = 0;
}


int CRigidEntity::EnforceConstraints(float time_interval)
{
	return 0;
	/*int i,i1,bEnforced=0;
	Vec3 pt[2],v0,v1;
	Ang3 ang;
	quaternionf qframe0,qframe1;

	for(i=0;i<NMASKBITS && getmask(i)<=m_constraintMask;i++) if (m_constraintMask & getmask(i)) 
	if (!(m_pConstraintInfos[i].flags & (constraint_inactive|constraint_rope|constraint_no_enforcement)) && 
			m_pConstraints[i].pbody[0]->Minv > m_pConstraints[i].pbody[1]->Minv*10) 
	{
		pt[0] = Loc2Glob(m_pConstraints[i], m_pConstraintInfos[i].ptloc[0], 0);
		pt[1] = Loc2Glob(m_pConstraints[i], m_pConstraintInfos[i].ptloc[1], 1);
		v0 = m_pConstraints[i].pbody[0]->v + (m_pConstraints[i].pbody[0]->w ^ pt[0]-m_pConstraints[i].pbody[0]->pos);
		v1 = m_pConstraints[i].pbody[1]->v + (m_pConstraints[i].pbody[1]->w ^ pt[1]-m_pConstraints[i].pbody[1]->pos);
		if (max(v0.len2(),v1.len2())*sqr(time_interval) < sqr(m_pWorld->m_vars.maxContactGap*12))
			continue;

		if ((m_pConstraints[i].flags & contact_angular|contact_constraint_3dof)==contact_constraint_3dof)
			m_posNew += pt[1]-pt[0];
		qframe0 = m_pConstraints[i].pbody[0]->q*m_pConstraintInfos[i].qframe_rel[0];
		qframe1 = m_pConstraints[i].pbody[1]->q*m_pConstraintInfos[i].qframe_rel[1];
		ang = Ang3::GetAnglesXYZ(Matrix33(!qframe1*qframe0));

		i1 = i+1;
		if (i1<NMASKBITS && constraintMask & getmask(i1) && m_pConstraintInfos[i+1].id==m_pConstraintInfos[i].id &&
			m_pConstraints[i1].flags & contact_angular & (contact_constraint_2dof|contact_constraint_1dof)) 
			if (m_pConstraints[i1++].flags & contact_constraint_2dof)
				ang.y=ang.z = 0;
		if (i1<NMASKBITS && constraintMask & getmask(i1) && m_pConstraintInfos[i+1].id==m_pConstraintInfos[i].id)
			if (m_pConstraintInfos[i1++].flags & constraint_limited_1axis) 
				ang.x = max(m_pConstraintInfos[i1-1].limits[0], min(m_pConstraintInfos[i1-1].limits[1], ang.x));
		m_qNew = qframe1*quaternionf(ang)*!qframe0*m_qNew;
		i = i1-1;	bEnforced++;

		m_body.pos = m_posNew+m_qNew*m_body.offsfb;
		m_body.q = m_qNew*!m_body.qfb;
		m_body.UpdateState();
	}

	return bEnforced;*/
}


float CRigidEntity::CalcEnergy(float time_interval)
{
	Vec3 v(ZERO);
	float Emax=m_body.M*sqr(m_pWorld->m_vars.maxVel)*2,Econstr=0;
	int i; 
	entity_contact *pContact;

	if (time_interval>0) {
		if (m_flags & ref_use_simple_solver & -m_pWorld->m_vars.bLimitSimpleSolverEnergy)
			return m_E0;

		if (!(m_flags & ref_use_simple_solver))
			for(pContact=m_pContactStart; pContact!=CONTACT_END(m_pContactStart); pContact=pContact->next)	{
				v += pContact->n*max(0.0f,max(0.0f,pContact->vreq*pContact->n-max(0.0f,pContact->vrel))-pContact->n*v);
				if (pContact->pbody[1]->Minv==0 && pContact->pent[1]->m_iSimClass>1) {
//					RigidBody *pbody = pContact->pbody[1];
					//vmax = max(vmax,(pbody->v + (pbody->w^pbody->pos-pContact->pt[1])).len2());
					Emax = m_body.M*sqr(10.0f)*2; // since will allow some extra energy, make sure we restrict the upper limit
				}
			}

		for(i=0; i<NMASKBITS && getmask(i)<=m_constraintMask; i++) 
		if (m_constraintMask & getmask(i) && (m_pConstraints[i].flags & contact_constraint_3dof || m_pConstraintInfos[i].flags & constraint_rope))	{
			//v += m_pConstraints[i].n*max(0.0f,max(0.0f,m_pConstraints[i].vreq*m_pConstraints[i].n-max(0.0f,m_pConstraints[i].vrel*m_pConstraints[i].n))-
			//	m_pConstraints[i].n*v);
			Econstr += max(m_pConstraints[i].pbody[0]->M*sqr(fabs_tpl(m_pConstraints[i].n*m_pConstraints[i].pbody[0]->v)+
																											 fabs_tpl(m_pConstraints[i].n*m_pConstraints[i].vreq)),
										 m_pConstraints[i].pbody[1]->M*sqr(fabs_tpl(m_pConstraints[i].n*m_pConstraints[i].pbody[1]->v)+
																											 fabs_tpl(m_pConstraints[i].n*m_pConstraints[i].vreq)));
			/*if (m_pConstraints[i].pbody[1]->Minv==0 && m_pConstraints[i].pent[1]->m_iSimClass>1) {
				RigidBody *pbody = m_pConstraints[i].pbody[1];
				vmax = max(vmax,(pbody->v + (pbody->w^pbody->pos-m_pConstraints[i].pt[1])).len2());
			}*/
		}
		return min_safe(m_body.M*max((m_body.v+v).len2(),m_body.v.len2()+v.len2()) + m_body.L*m_body.w + Econstr, Emax);
	}

	return m_body.M*max((m_body.v+v).len2(),m_body.v.len2()+v.len2()) + m_body.L*m_body.w;
}


void CRigidEntity::ComputeBBoxRE(coord_block_BBox *partCoord)
{
	for(int i=0; i<min(2,min(m_nParts,m_iVarPart0)); i++) {
		partCoord[i].pos = m_parts[i].pNewCoords->pos; partCoord[i].q = m_parts[i].pNewCoords->q;
		partCoord[i].scale = m_parts[i].pNewCoords->scale; m_parts[i].pNewCoords = &partCoord[i];
	}
	ComputeBBox(m_BBoxNew,update_part_bboxes & min(m_nParts,m_iVarPart0)-3>>31);
}

void CRigidEntity::UpdatePosition(int bGridLocked)
{
	int i;
	WriteLock lock(m_lockUpdate); 

	m_pos = m_pNewCoords->pos; m_qrot = m_pNewCoords->q;
	for(i=0; i<m_nParts; i++) {
		m_parts[i].pos = m_parts[i].pNewCoords->pos; m_parts[i].q = m_parts[i].pNewCoords->q;
	}

	if (min(m_nParts,m_iVarPart0)<=2)	{
		for(i=0; i<m_nParts; i++) {
			m_parts[i].BBox[0]=m_parts[i].pNewCoords->BBox[0]; m_parts[i].BBox[1]=m_parts[i].pNewCoords->BBox[1];
		}	
		for(i=0;i<min(2,min(m_nParts,m_iVarPart0));i++) m_parts[i].pNewCoords = (coord_block_BBox*)&m_parts[i].pos;
		m_BBox[0]=m_BBoxNew[0]; m_BBox[1]=m_BBoxNew[1];
	} else {
		for(i=0;i<min(m_nParts,m_iVarPart0);i++) m_parts[i].pNewCoords = (coord_block_BBox*)&m_parts[i].pos;
		ComputeBBox(m_BBox);
		for(i=0; i<m_nParts; i++) {
			m_parts[i].BBox[0]=m_parts[i].pNewCoords->BBox[0]; m_parts[i].BBox[1]=m_parts[i].pNewCoords->BBox[1];
		}	
	}
	m_pWorld->UnlockGrid(this,-bGridLocked);
}

void CRigidEntity::CapBodyVel() 
{
	if (m_body.v.len2() > sqr(m_pWorld->m_vars.maxVel)) {
		m_body.v.normalize() *= m_pWorld->m_vars.maxVel;
		m_body.P = m_body.v*m_body.M;
	}
	if (m_body.w.len2() > sqr(m_maxw)) {
		m_body.w.normalize() *= m_maxw;
		m_body.L = m_body.q*(m_body.Ibody*(!m_body.q*m_body.w));
	}
}


void CRigidEntity::CheckContactConflicts(geom_contact *pcontacts, int ncontacts, int iCaller)
{
	int i,j;
	for(i=0;i<ncontacts-1;i++) for(j=i+1;j<ncontacts;j++) 
		if ((g_CurColliders[i]==g_CurColliders[j] && g_CurCollParts[i][1]==g_CurCollParts[j][1] || 
				 g_CurColliders[i]->m_iSimClass+g_CurColliders[j]->m_iSimClass==0) &&
				max(pcontacts[i].vel,0.0f)+max(pcontacts[j].vel,0.0f)<=0 &&
				pcontacts[i].dir*pcontacts[j].dir<-0.85f)
			if (g_CurColliders[i]->m_iSimClass)
				pcontacts[(pcontacts[i].center-m_body.pos)*pcontacts[i].dir>0 ? i:j].t = -1;
			else {
				Vec3 thinDim(ZERO);
				if (m_nParts==1) {
					box bbox;	m_parts[0].pPhysGeomProxy->pGeom->GetBBox(&bbox);
					int idim = idxmin3(bbox.size);
					if (bbox.size[idim] < max(bbox.size[inc_mod3[idim]],bbox.size[dec_mod3[idim]])*0.1f)
						thinDim = m_qrot*m_parts[0].q*bbox.Basis.GetRow(idim);
				}
				if ((fabs_tpl(pcontacts[i].n*thinDim)>0.94f || 
						(pcontacts[i].center-pcontacts[j].center)*pcontacts[i].n<0 &&	// **||**, but not |****|
						(pcontacts[j].center-pcontacts[i].center)*pcontacts[j].n<0))
				{
					Vec3 axisx,axisy,dci=pcontacts[i].center-m_body.pos;//, dcj=pcontacts[j].center-m_body.pos;
					Vec2 pt2d,BBoxi[2],BBoxj[2],ci,cj,szi,szj; 
					int ipt;
					axisx = pcontacts[i].dir.GetOrthogonal().normalized();
					axisy = pcontacts[i].dir^axisx;

					BBoxi[0] = BBoxi[1].set((pcontacts[i].ptborder[0]-pcontacts[i].center)*axisx, (pcontacts[i].ptborder[0]-pcontacts[i].center)*axisy);
					for(ipt=1; ipt<pcontacts[i].nborderpt; ipt++)	{
						pt2d.set((pcontacts[i].ptborder[ipt]-pcontacts[i].center)*axisx, (pcontacts[i].ptborder[ipt]-pcontacts[i].center)*axisy);
						BBoxi[0] = min(BBoxi[0],pt2d); BBoxi[1] = max(BBoxi[1],pt2d);
					}

					BBoxj[0] = BBoxj[1].set((pcontacts[j].ptborder[0]-pcontacts[i].center)*axisx, (pcontacts[j].ptborder[0]-pcontacts[i].center)*axisy);
					for(ipt=1; ipt<pcontacts[j].nborderpt; ipt++)	{
						pt2d.set((pcontacts[j].ptborder[ipt]-pcontacts[i].center)*axisx, (pcontacts[j].ptborder[ipt]-pcontacts[i].center)*axisy);
						BBoxj[0] = min(BBoxj[0],pt2d); BBoxj[1] = max(BBoxj[1],pt2d);
					}

					ci = (BBoxi[1]+BBoxi[0])*0.5f; szi = (BBoxi[1]-BBoxi[0])*0.5f;
					cj = (BBoxj[1]+BBoxj[0])*0.5f; szj = (BBoxj[1]-BBoxj[0])*0.5f;
					if (max(fabs_tpl(ci.x-cj.x)-szi.x-szj.x, fabs_tpl(ci.y-cj.y)-szi.y-szj.y)<0)
						pcontacts[dci*pcontacts[i].dir>0 ? i:j].t = -1;
				}
			}
}


void CRigidEntity::ProcessCanopyContact(geom_contact *pcontacts, int i, float time_interval, int iCaller)
{
	int j;
	box bbox; 
	Vec3 dP,center,dv;
	float ks,kd,Mpt,g=m_pWorld->m_vars.gravity.len(); 
	entity_contact *pContact;

	m_parts[j = g_CurCollParts[i][0]].pPhysGeom->pGeom->GetBBox(&bbox);
	center = m_qNew*(m_parts[j].q*m_parts[j].pPhysGeom->origin*m_parts[j].scale+m_parts[j].pos)+m_posNew - m_body.pos;
	Mpt = 1/(m_body.Minv+(m_body.Iinv*(center^pcontacts[i].dir)^center)*pcontacts[i].dir);
	
	/*area = (pcontacts[i].ptborder[pcontacts[i].nborderpt-1]-pcontacts[i].center ^ pcontacts[i].ptborder[0]-pcontacts[i].center)*pcontacts[i].dir;
	for(j=0; j<pcontacts[i].nborderpt-1; j++)
		area += (pcontacts[i].ptborder[j]-pcontacts[i].center ^ pcontacts[i].ptborder[j+1]-pcontacts[i].center)*pcontacts[i].dir;*/
	//area = bbox.size.x*bbox.size.y*-3;
	if (true) {//area<0) {
		/*ks = area*-0.5f*Mpt*m_parts[g_CurCollParts[i][0]].minContactDist*g;
		ks /= bbox.size.GetVolume()*4;
		kd = area*-0.5f/(bbox.size.x*bbox.size.y*4);*/
		ks = Mpt*m_parts[g_CurCollParts[i][0]].minContactDist*g/((bbox.size.x+bbox.size.y+bbox.size.z)*m_parts[j].scale);
		kd = 3.5f;
		
		dP = pcontacts[i].dir*(ks*pcontacts[i].t);
		m_Psoft += dP; m_Lsoft += center^dP;
		dv = m_body.v + (m_body.w^center);
		if (/*dv.len2()<sqr(0.15f) ||*/m_timeCanopyFallen>3.0f) for(j=0;j<pcontacts[i].nborderpt;j++) {
			pcontacts[i].n = -pcontacts[i].dir;
			if (pContact = RegisterContactPoint(i, pcontacts[i].ptborder[j], pcontacts, pcontacts[i].iPrim[0],pcontacts[i].iFeature[0], 
				pcontacts[i].iPrim[1],pcontacts[i].iFeature[1], contact_new, 0.001f, iCaller))
				pContact->nloc.zero();
			m_Psoft.zero(); m_Lsoft.zero();
		} else {
			dP = dv*(-2.0f*sqrt_tpl(ks*Mpt)*time_interval);
			m_Pext += dP; m_Lext += (center^dP)-m_body.L*(time_interval*kd);
		}
		//m_timeCanopyFallen += isneg(dv.len2()-sqr(0.15f))*4.0f;
		m_timeCanopyFallen += time_interval*sgn(pcontacts[i].t-(bbox.size.x+bbox.size.y+bbox.size.z)*0.02f);
		m_timeCanopyFallen = min(5.0f*isneg(-m_nColliders), max(0.0f,m_timeCanopyFallen));
		m_bCanopyContact |= 1;

		if (m_flags & pef_log_collisions && m_nMaxEvents>0 || m_parts[g_CurCollParts[i][0]].flags & geom_log_interactions) {
			EventPhysCollision event;
			event.pEntity[0] = this; event.pEntity[1] = g_CurColliders[i];
			event.pForeignData[0] = m_pForeignData; event.iForeignData[0] = m_iForeignData; 
			event.pForeignData[1] = g_CurColliders[i]->m_pForeignData; event.iForeignData[1] = g_CurColliders[i]->m_iForeignData; 
			event.pt = pcontacts[i].center;
			event.n = pcontacts[i].n;
			event.idCollider = m_pWorld->GetPhysicalEntityId(event.pEntity[1]);
			event.partid[0] = m_parts[g_CurCollParts[i][0]].id;
			event.partid[1] = g_CurColliders[i]->m_parts[g_CurCollParts[i][1]].id;
			event.idmat[0] = pcontacts[i].id[0];	event.idmat[1] = pcontacts[i].id[1];
			event.mass[0] = m_body.M;
			event.mass[1] = g_CurColliders[i]->GetMass(g_CurCollParts[i][1]);
			event.vloc[0] = m_body.v+(m_body.w^pcontacts[i].center-m_body.pos);
			event.vloc[1].zero();
			event.penetration = pcontacts[i].t;
			event.radius = event.normImpulse = 0;
			m_pWorld->OnEvent(pef_log_collisions, &event);
		}
	}
}


void CRigidEntity::DelayedIntersect(geom_contact *pcontacts, int ncontacts, CPhysicalEntity **pColliders, int (*iCollParts)[2])
{
	CRY_PROFILE_FUNCTION(PROFILE_PHYSICS );
	PHYS_ENTITY_PROFILER
	int i,j;
	int iCaller = MAX_PHYS_THREADS;
	for(i=0; i<ncontacts; i++) {
		pcontacts[i].id[0] = GetMatId(pcontacts[i].id[0], iCollParts[i][0]);
		pcontacts[i].id[1] = pColliders[i]->GetMatId(pcontacts[i].id[1], iCollParts[i][1]);
		g_CurColliders[i] = pColliders[i];
		g_CurCollParts[i][0] = iCollParts[i][0];
		g_CurCollParts[i][1] = iCollParts[i][1];
		ProcessContactEvents(&pcontacts[i], i, iCaller);
	}

	if (ncontacts<=8)
		CheckContactConflicts(pcontacts,ncontacts,iCaller);

	int bCanopy = m_bCanopyContact&1;
	m_Psoft *= bCanopy; m_Lsoft *= bCanopy;

	for(i=0;i<ncontacts;i++) if (!(m_parts[g_CurCollParts[i][0]].flags & geom_squashy)) {
		Vec3 ntilt(ZERO),axis(ZERO), offs=pcontacts[i].dir*pcontacts[i].t, sz=m_BBox[1]-m_BBox[0];	
		float curdepth, r=0, e=m_pWorld->m_vars.maxContactGap*0.5f, maxUnproj = max(max(sz.x,sz.y),sz.z);	
		int bPrimPrimContact = -((pcontacts[i].iNode[0]&pcontacts[i].iNode[1])>>31), flagsLast=contact_last;
		if (pcontacts[i].parea && pcontacts[i].parea->npt>=2) {
			ntilt = (axis = pcontacts[i].parea->n1)^pcontacts[i].n;
			pcontacts[i].n = -pcontacts[i].dir;
			for(j=0,r=maxUnproj; j<pcontacts[i].parea->npt; j++)
				r = min(r, axis*(pcontacts[i].parea->pt[j]-pcontacts[i].pt));
			for(j=0; j<(pcontacts[i].parea->npt&-bPrimPrimContact); j++) 
				if ((curdepth=(float)pcontacts[i].t-axis*(pcontacts[i].parea->pt[j]-pcontacts[i].pt)+r) > -e)
					RegisterContactPoint(i, pcontacts[i].parea->pt[j]-offs, pcontacts, pcontacts[i].parea->piPrim[0][j],
						pcontacts[i].parea->piFeature[0][j], pcontacts[i].parea->piPrim[1][j],pcontacts[i].parea->piFeature[1][j], 
						contact_new|contact_inexact|contact_area|flagsLast, max(0.001f,curdepth),iCaller, ntilt), flagsLast=0;
			r = (axis*(pcontacts[i].center-pcontacts[i].pt))*(bPrimPrimContact^1);
			for(j=0; j<(pcontacts[i].nborderpt&~-bPrimPrimContact); j++)
				r = min(r, axis*(pcontacts[i].ptborder[j]-pcontacts[i].pt));
		} else if (bPrimPrimContact) RegisterContactPoint(i, pcontacts[i].pt-offs, pcontacts, pcontacts[i].iPrim[0],
				pcontacts[i].iFeature[0], pcontacts[i].iPrim[1],pcontacts[i].iFeature[1], contact_new|contact_last, pcontacts[i].t,iCaller);
		if (!bPrimPrimContact) {
			RegisterContactPoint(i, pcontacts[i].center, pcontacts, pcontacts[i].iPrim[0],pcontacts[i].iFeature[0], 
				pcontacts[i].iPrim[1],pcontacts[i].iFeature[1], contact_new|contact_last, max(0.001f,(float)pcontacts[i].t-
				axis*(pcontacts[i].center-pcontacts[i].pt)+r), iCaller, ntilt);
			for(j=0;j<pcontacts[i].nborderpt;j++)
				RegisterContactPoint(i, pcontacts[i].ptborder[j], pcontacts, pcontacts[i].iPrim[0],pcontacts[i].iFeature[0], 
					pcontacts[i].iPrim[1],pcontacts[i].iFeature[1], contact_new, max(0.001f,(float)pcontacts[i].t-
					axis*(pcontacts[i].ptborder[j]-pcontacts[i].pt)+r), iCaller, ntilt);
		}
		/*Vec3 ntilt(ZERO),axis(ZERO);	
		float r=0;
		if (pcontacts[i].parea && pcontacts[i].parea->npt>2) {
			if (pcontacts[i].parea->type==geom_contact_area::polygon) {
				if (pcontacts[i].parea->n1*pcontacts[i].n<-0.999f && fabs_tpl(pcontacts[i].parea->n1*m_gravity)>fabs_tpl(pcontacts[i].n*m_gravity)) {
					ntilt=pcontacts[i].n; pcontacts[i].n=-pcontacts[i].parea->n1; pcontacts[i].parea->n1=-ntilt;
				}
				axis = (ntilt = pcontacts[i].parea->n1^pcontacts[i].n)^pcontacts[i].n;
				for(j=1,r=pcontacts[i].ptborder[0]*axis; j<pcontacts[i].nborderpt; j++)
					r = max(r,pcontacts[i].ptborder[j]*axis);
			}
		}
		entity_contact *pContact;
		if (pContact = RegisterContactPoint(i, pcontacts[i].center, pcontacts, pcontacts[i].iPrim[0],pcontacts[i].iFeature[0], 
			pcontacts[i].iPrim[1],pcontacts[i].iFeature[1], contact_new|contact_last, max(0.001f,(float)pcontacts[i].t+axis*pcontacts[i].center-r),
			iCaller))
			pContact->nloc = ntilt;
		for(j=0;j<pcontacts[i].nborderpt;j++)
		if (pContact = RegisterContactPoint(i, pcontacts[i].ptborder[j], pcontacts, pcontacts[i].iPrim[0],pcontacts[i].iFeature[0], 
			pcontacts[i].iPrim[1],pcontacts[i].iFeature[1], contact_new, max(0.001f,(float)pcontacts[i].t+axis*pcontacts[i].ptborder[j]-r), iCaller))
			pContact->nloc = ntilt;*/
		//bSeverePenetration |= isneg(min(0.4f,ip.maxUnproj*0.4f)-pcontacts[i].t);
	} else
		ProcessCanopyContact(pcontacts,i, m_lastTimeStep, iCaller);

  CleanupAfterContactsCheck(iCaller);

	if ((bCanopy*2 | m_bCanopyContact & 1)==1) {
		m_body.P += m_Pext; m_Pext.zero();
		m_body.L += m_Lext; m_Lext.zero();
		m_body.P += m_Psoft*m_lastTimeStep; m_body.L += m_Lsoft*m_lastTimeStep;
		m_body.v = m_body.P*m_body.Minv;
		m_body.w = m_body.Iinv*m_body.L;
	}
}


void CRigidEntity::StartStep(float time_interval)
{
	m_timeStepPerformed = 0;
	m_timeStepFull = time_interval;
	m_sweepGap = m_pWorld->m_vars.maxContactGap*0.5f;
}

int __bstop = 0;
extern int __curstep;

int CRigidEntity::Step(float time_interval)
{
	CRY_PROFILE_FUNCTION(PROFILE_PHYSICS );
  PHYS_ENTITY_PROFILER

	geom_contact *pcontacts;
	int i,j,itmax,itmax0,ncontacts,bHasContacts,bNoForce,bSeverePenetration=0,bNoUnproj=0,bWasUnproj=0,nsweeps=0;
	const int bUseSimpleSolver = 0;//isneg(-((int)m_flags&ref_use_simple_solver));
	int iCaller = get_iCaller_int();
	float r,e;
	quaternionf qrot;
	Vec3 axis,pos;
	geom_world_data gwd;
	intersection_params ip;
	coord_block_BBox partCoordTmp[2];
	entity_contact *pContact;

	CPhysicalEntity *pDeadCollider=0;
	{ ReadLock lockcl(m_lockColliders); 
	for(i=m_nColliders-1,bHasContacts=isneg(-m_nContacts); i>=0; i--) {
		if (m_pColliders[i]->m_iSimClass==7) {
			m_pColliders[i]->m_next_coll1=pDeadCollider; pDeadCollider=m_pColliders[i];
		}	else
			bHasContacts |= m_pColliders[i]->HasCollisionContactsWith(this);
	}	}
	for(; pDeadCollider; pDeadCollider=pDeadCollider->m_next_coll1)
		RemoveColliderMono(pDeadCollider);
	m_prevPos = m_body.pos;
	m_prevq = m_body.q;
	m_vSleep.zero(); m_wSleep.zero();
	m_pStableContact = 0;

	m_dampingEx = 0;
	if (m_timeStepPerformed>m_timeStepFull-0.001f || m_nParts==0)
		return 1;
	m_timeStepPerformed += time_interval;
	m_lastTimeStep = time_interval;
	time_interval += (m_nextTimeStep-time_interval)*isneg(1e-8f-m_nextTimeStep);
	m_nextTimeStep = 0;
	m_minFriction = 0.1f;
	bNoForce = iszero(m_body.Fcollision.len2());
	bNoUnproj = isneg(4-(int)m_nFutileUnprojFrames*bNoForce);
	m_bSmallAndFastForced &= m_bAwake;
	m_flags &= ~ref_small_and_fast;
	m_flags |= ref_small_and_fast & -(int)m_bSmallAndFastForced;

	// if not sleeping, check for new contacts with all surrounding entities
	if (m_bAwake | bUseSimpleSolver) {
		m_Psoft.zero(); m_Lsoft.zero();
		m_body.Step(time_interval);
		m_body.pos += m_forcedMove; m_forcedMove.zero();
		CapBodyVel();
		m_qNew = m_body.q*m_body.qfb;
		m_posNew = m_body.pos-m_qNew*m_body.offsfb;

		ip.maxSurfaceGapAngle = max(0.15f, min(0.25f, (fabs_tpl(m_body.w.x)+fabs_tpl(m_body.w.y)+fabs_tpl(m_body.w.z))*time_interval));
		ip.axisContactNormal = -m_gravity.normalized();
		//ip.bNoAreaContacts = !m_pWorld->m_vars.bUseDistanceContacts;
		gwd.v = m_body.v;
		gwd.w = m_body.w;
		gwd.centerOfMass = m_body.pos;

		if (m_bCollisionCulling || m_nParts==1 && m_parts[0].pPhysGeomProxy->pGeom->IsConvex(0.02f)) 
			ip.ptOutsidePivot[0] = m_body.pos;
		Vec3 sz = m_BBox[1]-m_BBox[0];
		ip.maxUnproj = max(max(sz.x,sz.y),sz.z);
		e = m_pWorld->m_vars.maxContactGap*(0.5f-0.4f*bUseSimpleSolver);
		ip.iUnprojectionMode = 0;
		int bSlowRot = isneg(m_body.w.len2()-sqr(3.0f));
		ip.bNoIntersection = bSlowRot;//bUseSimpleSolver;
		ip.maxSurfaceGapAngle = DEG2RAD(3.5f);
		m_qNew.Normalize();
		m_sizeFastDir *= (m_bSmallAndFastForced^1 | bHasContacts); // force sweep if no contacts and small_and_fast was set manually (for projectiles)

		if (!EnforceConstraints(time_interval) && m_velFastDir*(time_interval-bNoUnproj)>m_sizeFastDir*0.5f*(1-m_alwaysSweep)) {
			SweepAgain:
			pos = m_posNew; m_posNew = m_pos;
			qrot = m_qNew; m_qNew = m_qrot;
			ComputeBBox(m_BBoxNew,0);
			ip.bSweepTest = true; 
			ip.time_interval = time_interval;
			if (m_sweepGap<0 && gwd.v.len2()>m_body.v.len2()*0.01f)
				ip.time_interval -= m_sweepGap*1.2f/max(1e-6f,gwd.v.len());
			m_flags |= ref_small_and_fast & -isneg(ip.maxUnproj-1.0f);
			int flagsAccum = 0;
			ncontacts = CheckForNewContacts(&gwd,&ip, itmax0, gwd.v*ip.time_interval, 0,-1, &flagsAccum);
			pcontacts = ip.pGlobalContacts;
			if (flagsAccum & geom_no_coll_response) {
				float tlim = itmax0>=0 ? pcontacts[itmax0].t : 1e10f;
				for(i=0;i<ncontacts;i++)
					if (pcontacts[i].t<tlim && (m_parts[g_CurCollParts[i][0]].flags|g_CurColliders[i]->m_parts[g_CurCollParts[i][1]].flags) & geom_no_coll_response)
						RegisterContactPoint(i, pcontacts[i].center, pcontacts, pcontacts[i].iPrim[0],pcontacts[i].iFeature[0], 
							pcontacts[i].iPrim[1],pcontacts[i].iFeature[1], contact_new|contact_last, 0.001f, iCaller);
			}
			if (itmax0>=0)	{
				m_body.pos = m_prevPos - pcontacts[itmax0].dir*max(0.0f,(float)pcontacts[itmax0].t+m_sweepGap);
				m_posNew = m_body.pos-qrot*m_body.offsfb;
				bWasUnproj = m_alwaysSweep^1;
				if (OnSweepHit(pcontacts[itmax0], itmax0, ip.time_interval, gwd.v, nsweeps))
					goto SweepAgain;
				if (m_bCollisionCulling || m_nParts==1 && m_parts[0].pPhysGeomProxy->pGeom->IsConvex(0.02f)) 
					ip.ptOutsidePivot[0] = m_body.pos;
			} else
				m_posNew = pos;
			ray_hit hit;
			for(i=0;i<m_nColliders;i++) 
				if (m_pColliders[i]->m_iSimClass<=2 && m_pColliders[i]!=this && m_pColliders[i]->m_iGroup!=m_iGroup && 
						m_pWorld->RayTraceEntity(m_pColliders[i],m_prevPos,m_body.pos-m_prevPos,&hit)) 
				{	
					m_body.v*=0.5f; m_body.w*=0.5f; m_body.P*=0.5f; m_body.L*=0.5f; 
					qrot=m_qrot; m_body.q=m_prevq;
					m_posNew=m_pos; m_body.pos=m_prevPos; break;
				}
			m_qNew = qrot;
			ip.bSweepTest = false;
			ip.time_interval = time_interval*3;
		}	else {
			if (m_bSmallAndFastForced) {
				CPhysicalEntity **pentlist;
				GetPotentialColliders(pentlist, time_interval);
			}
			ip.time_interval = time_interval;
		}

		ComputeBBox(m_BBoxNew,0); 
		CheckAdditionalGeometry(time_interval);

		//ip.vrel_min = ip.maxUnproj/ip.time_interval*(bHasContacts ? 0.2f : 0.05f);
		//ip.vrel_min += 1E10f*(bUseSimpleSolver|bWasUnproj);
		ip.vrel_min = 1E10f;
		int nSweptFails = 0;
		CheckContacts:
		ncontacts = CheckForNewContacts(&gwd,&ip, itmax);
		pcontacts = ip.pGlobalContacts;

		if (ncontacts==0 && bWasUnproj) {
			if (++nSweptFails==1)	{
				m_qNew = m_qrot; m_posNew = m_body.pos-m_qrot*m_body.offsfb;
				goto CheckContacts;
			}
			RegisterContactPoint(itmax0,pcontacts[itmax0].pt,pcontacts,pcontacts[itmax0].iPrim[0],pcontacts[itmax0].iFeature[0],pcontacts[itmax0].iPrim[1],pcontacts[itmax0].iFeature[1]);
		}
		if (ncontacts<=8)
			CheckContactConflicts(pcontacts, ncontacts, iCaller);

		if ((i=itmax)>=0) { // first unproject tmax contact to exact collision time (to get ptloc fields in entity_contact filled correctly)
			if (pcontacts[i].iUnprojMode==0) // check if unprojection conflicts with existing contacts
				for(pContact=m_pContactStart; pContact!=CONTACT_END(m_pContactStart) && pContact->n*pcontacts[i].dir>-0.001f; pContact=pContact->next);
			else for(pContact=m_pContactStart; pContact!=CONTACT_END(m_pContactStart); pContact=pContact->next) {
				axis = pcontacts[i].dir ^ pContact->pt[0]-ip.centerOfRotation; r = axis.len2();
				if (r>sqr(ip.minAxisDist) && sqr_signed(axis*pContact->n)<-0.001f*r)
					break;
			}
			if (pContact==CONTACT_END(m_pContactStart) && pcontacts[i].iUnprojMode==0 && (!bHasContacts || pcontacts[i].t>ip.maxUnproj*0.1f) && 
					pcontacts[i].t < pcontacts[i].vel*time_interval*1.1f) 
			{	m_posNew += pcontacts[i].dir*(pcontacts[i].t-e);
				m_body.pos += pcontacts[i].dir*(pcontacts[i].t-e);
				bWasUnproj = 1;
			}	/*else if (pcontacts[i].iUnprojMode==1 && pcontacts[i].t<0.3f) {
				e /= (pcontacts[i].pt-ip.centerOfRotation).len();
				//qrot = quaternionf(pcontacts[i].t,pcontacts[i].dir);
				qrot.SetRotationAA(pcontacts[i].t,pcontacts[i].dir);
				m_qNew = qrot*m_qNew; 
				m_posNew = ip.centerOfRotation + qrot*(m_posNew-ip.centerOfRotation);
				m_body.pos = m_posNew+m_qNew*m_body.offsfb;
				m_body.q = m_qNew*!m_body.qfb;
				m_body.UpdateState();
			}*/

			ComputeBBoxRE(partCoordTmp);
			ip.iUnprojectionMode = 0;
			ip.vrel_min = 1E10;
			ip.maxSurfaceGapAngle = 0;
			ncontacts = CheckForNewContacts(&gwd,&ip, itmax);
		} else
			ComputeBBoxRE(partCoordTmp);

		if (bUseSimpleSolver) {
			m_body.P += m_Pext; m_Pext.zero();
			m_body.L += m_Lext; m_Lext.zero();
			m_Estep = (m_body.v.len2() + (m_body.L*m_body.w)*m_body.Minv)*0.5f;
			m_body.P += m_gravity*(m_body.M*time_interval);
			m_body.v = m_body.P*m_body.Minv;
			m_body.w = m_body.Iinv*m_body.L;
			m_E0 = m_body.M*m_body.v.len2() + m_body.L*m_body.w;
			UpdatePenaltyContacts(time_interval);
		}

		m_bCanopyContact <<= 1;
		Vec3 extPull(ZERO);
		for(i=0; m_constraintMask>=getmask(i); i++) if (m_constraintMask & getmask(i) && !(m_pConstraints[i].flags & contact_angular))
			extPull += m_pConstraints[i].n*m_pConstraints[i].Pspare;
		extPull *= isneg(sqr(m_body.M*m_lastTimeStep*3)*m_gravity.len2() - extPull.len2());

		for(i=0;i<ncontacts;i++) if (pcontacts[i].t>=0) { // penetration contacts - register points and add additional penalty impulse in solver
			if (!(m_parts[g_CurCollParts[i][0]].flags & geom_squashy)) {
				Vec3 ntilt(ZERO), offs=pcontacts[i].dir*pcontacts[i].t;	float curdepth;	
				int bPrimPrimContact = -((pcontacts[i].iNode[0]&pcontacts[i].iNode[1])>>31)&-bSlowRot, hasArea=0, flagsLast=contact_last;
				r=0; axis.zero(); 
				if (pcontacts[i].parea && pcontacts[i].parea->npt>=2) {
					ntilt = (axis = pcontacts[i].parea->n1)^pcontacts[i].n;
					pcontacts[i].n = -pcontacts[i].dir;
					float esafe = max(0.0f,pcontacts[i].n*extPull);
					for(j=0,r=ip.maxUnproj; j<pcontacts[i].parea->npt; j++)
						r = min(r, axis*(pcontacts[i].parea->pt[j]-pcontacts[i].pt));
					for(j=0; j<(pcontacts[i].parea->npt&-bPrimPrimContact); j++) 
						if ((curdepth=(float)pcontacts[i].t-axis*(pcontacts[i].parea->pt[j]-pcontacts[i].pt)+r) > -e-esafe)
							RegisterContactPoint(i, pcontacts[i].parea->pt[j]-offs, pcontacts, pcontacts[i].parea->piPrim[0][j],
								pcontacts[i].parea->piFeature[0][j], pcontacts[i].parea->piPrim[1][j],pcontacts[i].parea->piFeature[1][j], 
								contact_area|contact_new|contact_inexact|flagsLast, max(0.001f,curdepth),iCaller, ntilt), flagsLast=0;
					r = (axis*(pcontacts[i].center-pcontacts[i].pt))*(bPrimPrimContact^1);
					for(j=0; j<(pcontacts[i].nborderpt&~-bPrimPrimContact); j++)
						r = min(r, axis*(pcontacts[i].ptborder[j]-pcontacts[i].pt));
					hasArea = contact_area & -iszero(pcontacts[i].parea->type-geom_contact_area::polygon);
				} else if (bPrimPrimContact) RegisterContactPoint(i, pcontacts[i].pt-offs, pcontacts, pcontacts[i].iPrim[0],
						pcontacts[i].iFeature[0], pcontacts[i].iPrim[1],pcontacts[i].iFeature[1], contact_new|contact_last, pcontacts[i].t,iCaller);
				if (!bPrimPrimContact) {
					RegisterContactPoint(i, pcontacts[i].center, pcontacts, pcontacts[i].iPrim[0],pcontacts[i].iFeature[0], 
						pcontacts[i].iPrim[1],pcontacts[i].iFeature[1], contact_new|contact_last|hasArea, max(0.001f,(float)pcontacts[i].t-
						axis*(pcontacts[i].center-pcontacts[i].pt)+r), iCaller, ntilt);
					for(j=0;j<pcontacts[i].nborderpt;j++)
						RegisterContactPoint(i, pcontacts[i].ptborder[j], pcontacts, pcontacts[i].iPrim[0],pcontacts[i].iFeature[0], 
							pcontacts[i].iPrim[1],pcontacts[i].iFeature[1], contact_new|hasArea, max(0.001f,(float)pcontacts[i].t-
							axis*(pcontacts[i].ptborder[j]-pcontacts[i].pt)+r), iCaller, ntilt);
				}
				bSeverePenetration |= isneg(ip.maxUnproj*0.5f-pcontacts[i].t);
			} else 
				ProcessCanopyContact(pcontacts,i, time_interval, iCaller);
		}
		CleanupAfterContactsCheck(iCaller);
	}	else {
		CPhysicalEntity **pentlist;
		if (m_nNonRigidNeighbours)
			GetPotentialColliders(pentlist);
		CheckAdditionalGeometry(time_interval);	
		m_BBoxNew[0]=m_BBox[0]; m_BBoxNew[1]=m_BBox[1];
	}

	if (!bUseSimpleSolver)
		VerifyExistingContacts(m_pWorld->m_vars.maxContactGap);

	UpdatePosition(m_pWorld->RepositionEntity(this,1,m_BBoxNew));

#if USE_IMPROVED_RIGID_ENTITY_SYNCHRONISATION
	if (m_pNetStateHistory && m_pNetStateHistory->Paused()==0 && !m_hasAuthority) {
		UpdateStateFromNetwork();
	}
#endif

	pe_params_buoyancy pb[4];
	int nBuoys = PostStepNotify(time_interval,pb,4,iCaller);

	Vec3 gravity = m_nColliders ? m_gravity : m_gravityFreefall;
	ApplyBuoyancy(time_interval,gravity,pb,nBuoys);
	if (!bUseSimpleSolver) {
		m_body.P += m_Pext; m_Pext.zero();
		m_body.L += m_Lext; m_Lext.zero();
		m_body.P += gravity*(m_body.M*time_interval);
		m_body.P += m_Psoft*time_interval; m_body.L += m_Lsoft*time_interval;
		m_body.UpdateState();
	}
	AddAdditionalImpulses(time_interval);
	m_body.Fcollision.zero(); m_body.Tcollision.zero();
	m_body.UpdateState();
	CapBodyVel();
	m_prevv = m_body.v; m_prevw = m_body.w;

	bSeverePenetration &= isneg(m_pWorld->m_threadData[iCaller].groupMass*0.001f-m_body.M);
	if (bSeverePenetration & m_bHadSeverePenetration) {
		if ((m_body.v.len2()+m_body.w.len2()*sqr(ip.maxUnproj*0.5f))*sqr(time_interval) < sqr(ip.maxUnproj*0.1f))
			bSeverePenetration = 0; // apparently we cannot resolve the situation by stepping back
		else {
			m_body.P.zero(); m_body.L.zero(); m_body.v.zero();m_body.w.zero();
		}
	}
	m_bHadSeverePenetration = bSeverePenetration;
	m_bSteppedBack = 0;
	m_nFutileUnprojFrames = m_nFutileUnprojFrames+bWasUnproj & -bNoForce;

	return (m_bHadSeverePenetration^1) | isneg(3-(int)m_nStepBackCount);//1;
}


int CRigidEntity::PostStepNotify(float time_interval,pe_params_buoyancy *pb,int nMaxBuoys,int iCaller)
{
	Vec3 gravity;
	int nBuoys=0;
	if (time_interval > 0.0f && (nBuoys=m_pWorld->CheckAreas(this,gravity,pb,nMaxBuoys,m_body.v))) {
		if (!is_unused(gravity)) {
			if ((m_gravityFreefall-gravity).len2())
				m_minAwakeTime = 0.2f;
			m_gravity = m_gravityFreefall = gravity;
		}
		//SetParams(&pb);
	}

	if (m_flags & (pef_monitor_poststep | pef_log_poststep)) {
		EventPhysPostStep event; InitEvent(&event,this,iCaller);
		event.dt = time_interval; event.pos = m_posNew; event.q = m_qNew; event.idStep=m_pWorld->m_idStep;
		m_pWorld->OnEvent(m_flags&pef_monitor_poststep, &event);

		if (m_pWorld->m_iLastLogPump==m_iLastLog && m_pEvent && !m_pWorld->m_vars.bMultithreaded) {
			//if (m_flags & pef_monitor_poststep)
			//	m_pWorld->SignalEvent(&event,0);
			WriteLock lock(m_pWorld->m_lockEventsQueue);
			m_pEvent->dt += time_interval;
			m_pEvent->idStep = m_pWorld->m_idStep;
			m_pEvent->pos = m_posNew; m_pEvent->q = m_qNew;
			m_pEvent->pGrid = m_pWorld->GetGrid(this);
			if (m_bProcessed & PENT_SETPOSED)
				AtomicAdd(&m_bProcessed, -PENT_SETPOSED);
		}	else {
			m_pEvent = 0;
			m_pWorld->OnEvent(m_flags&pef_log_poststep,&event,&m_pEvent);
			m_iLastLog = m_pWorld->m_iLastLogPump;
		}
	}	else 
		m_iLastLog = 1;

	return nBuoys;
}


float CRigidEntity::GetMaxTimeStep(float time_interval)
{
	if (m_timeStepPerformed > m_timeStepFull-0.001f)
		return time_interval;

	box bbox;	float bestvol=0;
	int i,j=0,j1,iBest=-1;
	int iCaller = get_iCaller_int();
	unsigned int flagsCollider=0;
	for(i=0;i<m_nParts;i++) if (m_parts[i].flags & geom_collides) {
		m_parts[i].pPhysGeomProxy->pGeom->GetBBox(&bbox);
		if (bbox.size.GetVolume()>bestvol) {
			bestvol = bbox.size.GetVolume(); iBest = i;
		}
		flagsCollider |= m_parts[i].flagsCollider;
	}
	if (iBest==-1) 
		return time_interval;
	if (m_pColliderContacts) for(i=0;i<m_nColliders;i++) if (m_pColliderContacts[i])
		j |= m_pColliders[i]->m_iSimClass-1;
	j &= -isneg(m_body.M-m_pWorld->m_threadData[iCaller].groupMass*0.001f);
	m_parts[iBest].pPhysGeomProxy->pGeom->GetBBox(&bbox);
	Vec3 vloc,vsz,size;
	vloc = m_body.v;
	if (m_forcedMove.len2()>0.0f)
		vloc += m_forcedMove/max(0.0001f,time_interval);
	vloc = bbox.Basis*(vloc*(m_qrot*m_parts[iBest].q));
	size = bbox.size*m_parts[iBest].scale;
	vsz(fabsf(vloc.x)*size.y*size.z, fabsf(vloc.y)*size.x*size.z, fabsf(vloc.z)*size.x*size.y);
	i = idxmax3(vsz);
	m_velFastDir = fabsf(vloc[i]);
	m_sizeFastDir = size[i];

	bool bSkipSpeedCheck = false;
	//if (!m_nColliders || m_nColliders==1 && m_pColliders[0]==this) {
	if (j>=0) { // if there are no collisions with static objects *or* there are collisions with very heavy rigid objects
		if (m_bCanSweep)
			bSkipSpeedCheck = true;
		else if (min(m_maxAllowedStep,time_interval)*m_velFastDir > m_sizeFastDir*0.7f) {
			int ient,nents;
			Vec3 BBox[2]={m_BBox[0],m_BBox[1]}, delta=m_body.v*m_maxAllowedStep;
			geom_contact *pcontacts;
			intersection_params ip;
			geom_world_data gwd0,gwd1;
			ip.bStopAtFirstTri = true;
			ip.time_interval = m_maxAllowedStep;
			ip.bThreadSafe = 1;

			BBox[isneg(-delta.x)].x += delta.x;
			BBox[isneg(-delta.y)].y += delta.y;
			BBox[isneg(-delta.z)].z += delta.z;
			CPhysicalEntity **pentlist;
			masktype constraint_mask = MaskIgnoredColliders(iCaller);
			nents = m_pWorld->GetEntitiesAround(BBox[0],BBox[1], pentlist, m_collTypes&(ent_terrain|ent_static|ent_sleeping_rigid|ent_rigid), this, 0,iCaller);
			UnmaskIgnoredColliders(constraint_mask,iCaller);

			CBoxGeom boxgeom;
			box abox;
			abox.Basis.SetIdentity();
			abox.center.zero();
			abox.size = (m_BBox[1]-m_BBox[0])*0.5f;
			boxgeom.CreateBox(&abox);
			gwd0.offset = (m_BBox[0]+m_BBox[1])*0.5f;
			gwd0.R.SetIdentity();
			gwd0.v = m_body.v;

			for(ient=0;ient<nents;ient++) if (pentlist[ient]!=this) for(j1=0;j1<pentlist[ient]->GetUsedPartsCount(iCaller);j1++) 
			if (pentlist[ient]->m_parts[j=pentlist[ient]->GetUsedPart(iCaller,j1)].flags & flagsCollider) {
				pentlist[ient]->GetPartTransform(j, gwd1.offset,gwd1.R,gwd1.scale, this);
				ip.bSweepTest = true;
				if (boxgeom.Intersect(pentlist[ient]->m_parts[j].pPhysGeomProxy->pGeom, &gwd0,&gwd1, &ip, pcontacts))
					goto hitsmth;
				// note that sweep check can return nothing if bbox is initially intersecting the obstacle
				ip.bSweepTest = false;
				if (boxgeom.Intersect(pentlist[ient]->m_parts[j].pPhysGeomProxy->pGeom, &gwd0,&gwd1, &ip, pcontacts))
					goto hitsmth;
			}
			bSkipSpeedCheck = true;
			hitsmth:;
		}
	}	/*else if (m_nColliders<=3)
		for(bSkipSpeedCheck=true,j=0; j<m_nColliders; j++) if (m_pColliders[j]!=this && m_pColliderContacts[j])
			bSkipSpeedCheck = false;*/

	if (!bSkipSpeedCheck && time_interval*fabsf(vloc[i])>size[i]*0.7f)
		time_interval = max(0.005f,size[i]*1.7f/fabsf(vloc[i]));

	int bDegradeQuality = -m_pWorld->m_threadData[iCaller].bGroupInvisible & ~(1-m_nColliders>>31);
	return min(min(m_timeStepFull-m_timeStepPerformed,m_maxAllowedStep*(1-bDegradeQuality*0.5f)),time_interval);
}


void CRigidEntity::ArchiveContact(entity_contact *pContact, float imp, int bLastInGroup, float r)
{
	if (m_flags & (pef_monitor_collisions | pef_log_collisions) && !(pContact->flags & contact_archived)) {
		int i;
		float vrel;
		EventPhysCollision event;
		for(i=0;i<2;i++) {
			event.pEntity[i] = pContact->pent[i]; event.pForeignData[i] = pContact->pent[i]->m_pForeignData; 
			event.iForeignData[i] = pContact->pent[i]->m_iForeignData;
		}
		event.pt = pContact->pt[0];
		event.n = pContact->n;
		event.idCollider = m_pWorld->GetPhysicalEntityId(pContact->pent[1]);
		for(i=0;i<2;i++) {
			event.vloc[i] = pContact->pbody[i]->v+(pContact->pbody[i]->w^pContact->pt[0]-pContact->pbody[i]->pos);
			event.mass[i] = pContact->pbody[i]->M;
			if ((event.partid[i] = pContact->pent[i]->m_parts[pContact->ipart[i]].id)<=-2)
				event.partid[i] = -2-event.partid[i];				
		}
		event.idmat[0] = pContact->id0;	event.idmat[1] = pContact->id1;
		event.penetration = pContact->penetration;
		event.pEntContact = pContact;
		event.normImpulse = imp;
		event.radius = r*(1+isneg(7-m_nParts)*1.5f);
		event.iPrim[1] = pContact->iConstraint;
		pContact->flags |= contact_archived;

		if (m_flags & pef_monitor_collisions)
			m_pWorld->SignalEvent(&event,0);

		if (m_flags & pef_log_collisions) {
			i = iszero(m_pWorld->m_iLastLogPump-m_iLastLogColl);
			m_nEvents &= -i;
			m_vcollMin += (1e10f-m_vcollMin)*(i^1);
			m_iLastLogColl = m_pWorld->m_iLastLogPump;
			if ((pContact->pent[0]->m_parts[pContact->ipart[0]].flags | pContact->pent[1]->m_parts[pContact->ipart[1]].flags) & geom_no_coll_response) {
				if (bLastInGroup)
					m_pWorld->OnEvent(pef_log_collisions,&event);
			}	else if (m_nMaxEvents>0) {
				vrel = (event.vloc[0]-event.vloc[1]).len2();
				if (m_nEvents==m_nMaxEvents) {	
					if (vrel>m_vcollMin && vrel<1E10f) {
						event.idmat[0]=event.idmat[1] = -1;	// make sure matids are invalid while the data is being copied (multithreading safety)
						memcpy(&m_pEventsColl[m_icollMin]->pEntity, &event.pEntity, sizeof(event)-((TRUNCATE_PTR)&event.pEntity-(TRUNCATE_PTR)&event));
						m_pEventsColl[m_icollMin]->idmat[0] = pContact->id0;	
						m_pEventsColl[m_icollMin]->idmat[1] = pContact->id1;	
						for(m_vcollMin=1E10f,i=0,m_icollMin=0x1F; i<(int)m_nEvents; i++) 
						if ((vrel=(m_pEventsColl[i]->vloc[0]-m_pEventsColl[i]->vloc[1]).len2())<m_vcollMin) {
							m_vcollMin = vrel; m_icollMin = i;
						}
					}
				} else {
					m_pWorld->OnEvent(pef_log_collisions,&event,&m_pEventsColl[m_nEvents++]);
					if (vrel<m_vcollMin) {
						m_vcollMin = vrel; m_icollMin = m_nEvents-1;
					}
				}
			}
		}
	}
}


int CRigidEntity::GetContactCount(int nMaxPlaneContacts)
{
	if (m_flags & ref_use_simple_solver)
		return 0;

	int i,nContacts,nTotContacts;
	entity_contact *pContact;

	for(i=nTotContacts=nContacts=0; i<m_nColliders; i++,nTotContacts+=min(nContacts,nMaxPlaneContacts))
		if (pContact=m_pColliderContacts[i]) for(nContacts=0;;pContact=pContact->next)	{
			nContacts += (pContact->flags>>contact_2b_verified_log2^1) & 1;
			if (pContact->flags & contact_last) break;
		}
	return nTotContacts;
}


int CRigidEntity::RegisterContacts(float time_interval,int nMaxPlaneContacts)
{
	CRY_PROFILE_FUNCTION(PROFILE_PHYSICS );

	int i,j,bComplexCollider,bStable,nContacts,bUseAreaContact,constrFlags=0;
	Vec3 sz = m_BBox[1]-m_BBox[0],n;
	float mindim=min(sz.x,min(sz.y,sz.z)), mindim1,dist,friction,penetration;
	entity_contact *pContact,*pContact0=NULL,*pContactLin=0,*pContactAng=0;
	RigidBody *pbody;
	m_body.Fcollision.zero();
	m_body.Tcollision.zero();
	m_bStable &= -((int)m_flags & ref_use_simple_solver)>>31;
	if (m_nPrevColliders!=m_nColliders)
		m_nRestMask = 0;
	m_nPrevColliders = m_nColliders;

	UpdateConstraints(time_interval);

	for(i=0;i<m_nColliders;i++) {
		if (!(m_flags & ref_use_simple_solver) && m_pColliderContacts[i]) {
			pbody=0; bComplexCollider=bStable=bUseAreaContact=0; friction=penetration=0; j=-1;
			for(pContact=m_pColliderContacts[i],nContacts=1;; pContact=pContact->next,nContacts++) {
				if (!(pContact->flags & contact_2b_verified)) {
					if (pbody && pContact->pbody[1]!=pbody)
						bComplexCollider = 1;
					pContact0 = pContact;
					pbody = pContact->pbody[1];
					bComplexCollider |= iszero(j-pContact->ipart[1])^(j>>31)+1;
					j = pContact->ipart[1];
					pContact->flags &= ~contact_maintain_count;
					friction = max(friction, pContact->friction);
					penetration = max(penetration, pContact->penetration);
				}
				pContact->nextAux=pContact->next; //pContact->prevAux=pContact->prev;
				if (pContact->flags & contact_last) break;
			}
			pContact = m_pColliderContacts[i];
			if (!bComplexCollider) {
				sz = m_pColliders[i]->m_BBox[1]-m_pColliders[i]->m_BBox[0];
				mindim1 = min(sz.x,min(sz.y,sz.z));
				mindim1 += iszero(mindim1)*mindim;
				bStable = CompactContactBlock(m_pColliderContacts[i],contact_last, min(mindim,mindim1)*0.15f, nMaxPlaneContacts,nContacts, 
					pContact, n,dist, m_body.pos,m_gravity);
				PREFAST_ASSUME(pContact0);
				if (bStable & isneg(-(pContact0->flags & contact_area)>>31) && dist<min(mindim,mindim1)*0.05f) {
					pContactLin = (entity_contact*)AllocSolverTmpBuf(sizeof(entity_contact)*2);
					pContactAng = pContactLin+1;
					if (pContactLin && pContactAng) {
						bUseAreaContact = 1;
						pContactLin->pent[0]=pContactAng->pent[0] = this;
						pContactLin->pent[1]=pContactAng->pent[1] = m_pColliders[i];
						pContactLin->pbody[0]=pContactAng->pbody[0] = &m_body;
						pContactLin->pbody[1]=pContactAng->pbody[1] = pbody;
						pContactLin->ipart[0]=pContactLin->ipart[1] = -1;
						pContactAng->ipart[0]=pContactAng->ipart[1] = -1;
						pContactAng->flags = contact_angular|contact_constraint_1dof| contact_maintain_count+3;//max(3,(nContacts>>1)+1);
						pContactAng->pBounceCount = &pContactLin->iCount;
						pContactLin->flags = 1;//contact_angular|contact_constraint_1dof|1;
						pContactLin->friction = friction;
						pContactLin->pt[0] = m_body.pos;
						pContactLin->n = pContactAng->n = pContact0->n;
						pContactLin->vreq = pContactLin->n*min(m_pWorld->m_vars.maxUnprojVel, 
							max(0.0f,(penetration-m_pWorld->m_vars.maxContactGap))*m_pWorld->m_vars.unprojVelScale);
						pContactAng->vreq = pContact0->penetration>0 ? pContact0->nloc*10.0f : Vec3(ZERO);
						if ((m_BBox[1]-m_BBox[0]).GetVolume()>(m_pColliders[i]->m_BBox[1]-m_pColliders[i]->m_BBox[0]).GetVolume()*0.1f && 
								m_pWorld->GetGrid(this)==m_pWorld->GetGrid(pContact0->pent[1])) 
							pContactLin->pt[1] = pbody->pos;
						else
							pContactLin->pt[1] = m_body.pos;
						m_pStableContact = pContact0;
					}
				}
				bStable += bStable & (m_pColliders[i]->IsAwake()^1);
			}
			PREFAST_ASSUME(pContactAng);
			m_bStable = max(m_bStable,(unsigned int)bStable);
			for(j=0; j<nContacts; j++,pContact=pContact->nextAux) {
				pContact->pBounceCount = &pContactAng->iCount;
				pContact->flags |= contact_maintain_count&-bUseAreaContact;
				RegisterContact(pContact);
				ArchiveContact(pContact);
			}
			if (bUseAreaContact) {
				RegisterContact(pContactAng);
				RegisterContact(pContactLin);
			}
		}
		for(j=0;j<NMASKBITS && getmask(j)<=m_pColliderConstraints[i];j++) if (m_pColliderConstraints[i] & getmask(j) && m_pConstraintInfos[j].bActive) {
			RegisterContact(m_pConstraints+j);
			constrFlags |= m_pConstraintInfos[j].flags;
		}
	}
	if (m_submergedFraction+(float)m_bDisablePreCG+(float)isneg(-(constrFlags & constraint_no_tears))>0)
		DisablePreCG();
	return 1;
}


void CRigidEntity::UpdateContactsAfterStepBack(float time_interval)
{
	entity_contact *pContact;
	Vec3 diff;

	for(pContact=m_pContactStart; pContact!=CONTACT_END(m_pContactStart); pContact=pContact->next) {
		diff.zero();//pContact->pbody[0]->q*pContact->ptloc[0]+pContact->pbody[0]->pos-
					 //pContact->pbody[1]->q*pContact->ptloc[1]-pContact->pbody[1]->pos;
		// leave contact point and ptloc unchanged, but update penetration and vreq
		//pContact->ptloc[0] = (pContact->pt[0]-pContact->pbody[0]->pos)*pContact->pbody[0]->q;
		//pContact->ptloc[1] = (pContact->pt[0]-pContact->pbody[1]->pos)*pContact->pbody[1]->q;
		if (pContact->penetration>0) {
			pContact->penetration = max(0.0f,pContact->penetration-min(0.0f,pContact->n*diff));
			pContact->vreq = pContact->n*min(1.2f,pContact->penetration*10.0f);
		}
	}
}


void CRigidEntity::StepBack(float time_interval)
{
	if (time_interval>0) {
		m_body.pos = m_prevPos;
		m_body.q = m_prevq;
		
		Matrix33 R = Matrix33(m_body.q);
		m_body.Iinv = R*m_body.Ibody_inv*R.T();
		m_qNew = m_body.q*m_body.qfb;
		m_posNew = m_body.pos-m_qNew*m_body.offsfb;
		m_bSteppedBack = 1;

		coord_block_BBox partCoordTmp[2];
		ComputeBBoxRE(partCoordTmp);
		UpdatePosition(m_pWorld->RepositionEntity(this,1,m_BBoxNew));
	}	else
		m_nextTimeStep = 0.001f;

	m_body.v = m_prevv;
	m_body.w = m_prevw;
	m_body.P = m_body.v*m_body.M;
	m_body.L = m_body.q*(m_body.Ibody*(!m_body.q*m_body.w));

	if (m_pEventsColl && m_pWorld->m_iLastLogPump==m_iLastLogColl) for(unsigned int i=0;i<m_nEvents;i++) 
		if (m_pEventsColl[i] && ((CPhysicalEntity*)m_pEventsColl[i]->pEntity[1])->m_flags & pef_deforming)
			m_pEventsColl[i]->mass[1] = 0.01f;
}


float CRigidEntity::GetDamping(float time_interval)
{
	float damping = max(m_nColliders ? m_damping : m_dampingFreefall,m_dampingEx);
	//if (!(m_flags & pef_fixed_damping) && m_nGroupSize>=m_pWorld->m_vars.nGroupDamping) 
	//	damping = max(damping,m_pWorld->m_vars.groupDamping);

	return max(0.0f,1.0f-damping*time_interval);
}


int CRigidEntity::Update(float time_interval, float damping)
{
	int i,j,iCaller=get_iCaller_int();
	float dt,E,E_accum, Emin = m_bFloating && m_nColliders+m_nPrevColliders==0 ? m_EminWater : m_Emin;
	Vec3 L_accum,pt[4];
	coord_block_BBox partCoordTmp[2];
	//m_nStickyContacts = m_nSlidingContacts = 0;
	m_nStepBackCount = (m_nStepBackCount&-(int)m_bSteppedBack)+m_bSteppedBack;
	if (m_flags & ref_use_simple_solver)
		damping = max(0.9f,damping);
	CapBodyVel();
	m_nCanopyContactsLost += iszero((int)(m_bCanopyContact & 3)-2);
	m_timeCanopyFallen += 3.0f*iszero((int)(m_nCanopyContactsLost & 3)-3);

	m_body.v*=damping; m_body.w*=damping; m_body.P*=damping; m_body.L*=damping;

	if (m_body.Eunproj>0) { // if the solver changed body position
		m_qNew = m_body.q*m_body.qfb;
		m_posNew = m_body.pos-m_qrot*m_body.offsfb;
		ComputeBBoxRE(partCoordTmp);
		UpdatePosition(m_pWorld->RepositionEntity(this,1,m_BBoxNew));
	}

	if (m_flags & pef_log_collisions && m_nMaxEvents>0)	{
		i = iszero(m_pWorld->m_iLastLogPump-m_iLastLogColl);
		m_nEvents &= -i;
		m_vcollMin += (1e10f-m_vcollMin)*(i^1);
		m_iLastLogColl = m_pWorld->m_iLastLogPump;
		for(i=0;i<(int)m_nEvents;i++) if (m_pEventsColl[i]->pEntContact) {
			m_pEventsColl[i]->normImpulse += ((entity_contact*)m_pEventsColl[i]->pEntContact)->Pspare;
			m_pEventsColl[i]->pEntContact = 0;
		}
	}

	pe_action_update_constraint auc; auc.bRemove=1;
	for(i=0,m_constraintMask=0;i<m_nColliders;m_constraintMask|=m_pColliderConstraints[i++]);
	for(i=0;i<NMASKBITS && getmask(i)<=m_constraintMask;i++);
	for(--i;i>=0;i--) if (m_constraintMask & getmask(i) && m_pConstraintInfos[i].flags & constraint_broken) {
		for(;i>0 && m_pConstraintInfos[i].id==m_pConstraintInfos[i-1].id;i--);
		auc.idConstraint = m_pConstraintInfos[i].id;
		Action(&auc);
	}

	/*for(j=0;j<NMASKBITS && getmask(j)<=contacts_mask;j++) 
	if (contacts_mask & getmask(j) && m_pContacts[j].penetration==0 && !(m_pContacts[j].flags & contact_2b_verified)) {
		vrel = m_pContacts[j].vrel*m_pContacts[j].n;
		if (vrel<m_pWorld->m_vars.minSeparationSpeed && m_pContacts[j].penetration==0) {
			if (m_pContacts[j].Pspare>0 && (m_pContacts[j].vrel-m_pContacts[j].n*vrel).len2()<sqr(m_pWorld->m_vars.minSeparationSpeed)) {
				m_iStickyContacts[(m_nStickyContacts = m_nStickyContacts+1&7)-1&7] = j;
				pt[nRestContacts&3] = m_pContacts[j].pt[0];
				nRestContacts += m_pContacts[j].pent[1]->IsAwake()^1;
			} else
				m_iSlidingContacts[(m_nSlidingContacts = m_nSlidingContacts+1&7)-1&7] = j;
		}
	}

	if (nRestContacts>=3) {
		m_bAwake = 0; float sg = (pt[1]-pt[0]^pt[2]-pt[0])*m_gravity;
		if (isneg(((m_body.pos-pt[0]^pt[1]-pt[0])*m_gravity)*sg) & isneg(((m_body.pos-pt[1]^pt[2]-pt[1])*m_gravity)*sg) & 
				isneg(((m_body.pos-pt[2]^pt[0]-pt[2])*m_gravity)*sg)) 
		{	// check if center of mass projects into stable contacts triangle 
			m_iSimClass=1; m_pWorld->RepositionEntity(this,2);
			goto doneupdate;
		}
	} */

	entity_contact *pContact = m_pStableContact;
	Vec3 v0=m_body.v, w0=m_body.w;
	if (m_bStable!=2) {
		for(i=0,pContact=0; i<m_nColliders && m_pColliders[i]->m_iSimClass==0; i++);
		if (i<m_nColliders)
			pContact = m_pColliderContacts[i];
	}
	if (pContact && pContact->pent[1]->m_iSimClass>0) {
		Vec3 vSleep,wSleep;
		pContact->pent[1]->GetSleepSpeedChange(pContact->ipart[1], vSleep,wSleep);
		if (vSleep.len2()+wSleep.len2()) {
			if (m_pWorld->GetGrid(this)!=m_pWorld->GetGrid(pContact->pent[1])) {
				wSleep.zero();
				vSleep = !m_pWorld->GetGrid(this)->m_transW.q*(m_pWorld->GetGrid(pContact->pent[1])->m_transW.q*vSleep);
			}
			m_body.w -= wSleep;
			m_body.v -= vSleep+(wSleep ^ pContact->pt[0]-pContact->pbody[1]->pos);
			m_body.P = m_body.v*m_body.M; 
			m_body.L = m_body.q*(m_body.Ibody*(!m_body.q*m_body.w));
		}
	}

	for(pContact=m_pContactStart,i=j=0; pContact!=CONTACT_END(m_pContactStart) && j<16*(1-i); pContact=pContact->next,j++) {
		int bLiesOnTop = isneg(sqr_signed(pContact->n*m_gravity)+m_gravity.len2()*0.5f);
		INT_PTR mask = -iszero((int)pContact->pent[1]->m_iSimClass-2);
		CRigidEntity *pTest = (CRigidEntity*)((INT_PTR)this & ~mask | (INT_PTR)pContact->pent[1] & mask);	// don't attempt to access m_bAwake of non-CRigidEntities
		i |= bLiesOnTop & (mask&1) & pTest->m_bAwake;
	}
	i |= iszero(m_nColliders-1) & isneg(m_nContacts-3);
	Emin *= 1-i*0.9f;
	
	if (m_body.Minv>0) {
		E = (m_body.v.len2() + (m_body.L*m_body.w)*m_body.Minv)*0.5f + m_body.Eunproj;
		if (m_flags & ref_use_simple_solver)
			E = min(E,m_Estep);
		dt = time_interval+(m_timeStepFull-time_interval)*m_pWorld->m_threadData[iCaller].bGroupInvisible;
		m_timeIdle = max(0.0f, m_timeIdle-(iszero(m_nColliders+m_nPrevColliders+m_submergedFraction)&isneg(-200.0f-m_pos.z))*dt*3);
		m_timeIdle *= m_pWorld->m_threadData[iCaller].bGroupInvisible;
		i = isneg(0.0001f-m_maxTimeIdle); // forceful deactivation is turned on
		i = i^1 | isneg((m_timeIdle+=dt*i)-m_maxTimeIdle);

		if (m_bAwake&=i) {
			if (E<Emin && (m_nColliders+m_nPrevColliders+m_bFloating+m_alwaysSweep || m_gravity.len2()==0) && m_minAwakeTime<=0) {
				/*if (!m_bFloating) {
					m_body.P.zero();m_body.L.zero(); m_body.v.zero();m_body.w.zero();
				}*/
				m_bAwake = 0;
			} else 
				m_nSleepFrames=0;
			m_vAccum.zero(); m_wAccum.zero();
		} else if (i) {
			m_vAccum += m_body.v; m_wAccum += m_body.w;
			L_accum = m_body.q*(m_body.Ibody*(!m_body.q*m_wAccum));
			if (m_bStable<2 && bitcount(m_nRestMask)<20)
				E_accum = (m_vAccum.len2() + (m_wAccum*L_accum)*m_body.Minv)*0.5f + m_body.Eunproj;
			else E_accum = E;
			if (E_accum>Emin) {
				m_bAwake = 1;	m_nSleepFrames = 0;
				m_vAccum.zero(); m_wAccum.zero();
				if (m_iSimClass==1) {
					m_iSimClass=2; m_pWorld->RepositionEntity(this,2);
				}
			} else {
				if (!(m_bFloating | m_flags&ref_use_simple_solver)) {
					m_vSleep = v0; m_wSleep = w0;
					m_body.P.zero();m_body.L.zero(); m_body.v.zero();m_body.w.zero();
				}
				if (++m_nSleepFrames>=2 && m_iSimClass==2) {
					m_iSimClass=1; m_pWorld->RepositionEntity(this,2);
					m_nSleepFrames = 0;
					m_timeCanopyFallen = max(0.0f, m_timeCanopyFallen-1.5f);
					m_vAccum.zero(); m_wAccum.zero();
				}
			}
		}	else if (m_iSimClass==2) {
			m_iSimClass=1; m_pWorld->RepositionEntity(this,2);
		}
		m_nRestMask = (m_nRestMask<<1) | (m_bAwake^1);
	}
	m_minAwakeTime = max(m_minAwakeTime,0.0f)-time_interval;
	m_bStable = 0;
	(m_body.flags &= ~rb_RK4) |= rb_RK4*isneg(m_nColliders-1);

	//doneupdate:
	/*if (m_pWorld->m_vars.bMultiplayer) {
		//m_pos = CompressPos(m_pos);
		//m_body.pos = m_pos+m_qrot*m_body.offsfb;
		m_qNew = CompressQuat(m_qrot);
		Matrix33 R = Matrix33(m_body.q = m_qrot*!m_body.qfb);
		m_body.v = CompressVel(m_body.v,50.0f);
		m_body.w = CompressVel(m_body.w,20.0f);
		m_body.P = m_body.v*m_body.M;
		m_body.L = m_body.q*(m_body.Ibody*(m_body.w*m_body.q));
		m_body.Iinv = R*m_body.Ibody_inv*R.T();
		ComputeBBoxRE(partCoordTmp);
		UpdatePosition(m_pWorld->RepositionEntity(this,1,m_BBoxNew));

		m_iLastChecksum = m_iLastChecksum+1 & NCHECKSUMS-1;
		m_checksums[m_iLastChecksum].iPhysTime = m_pWorld->m_iTimePhysics;
		m_checksums[m_iLastChecksum].checksum = GetStateChecksum();
	}*/

	return (m_bAwake^1) | isneg(m_timeStepFull-m_timeStepPerformed-0.001f) | m_pWorld->m_threadData[iCaller].bGroupInvisible;
}

#if USE_IMPROVED_RIGID_ENTITY_SYNCHRONISATION
void SRigidEntityNetStateHistory::PushReceivedState(const SRigidEntityNetSerialize& item) {
	if (numReceivedStates >= MAX_STATE_HISTORY_SNAPSHOTS) {
		m_receivedStates[receivedStatesStart] = item;
		receivedStatesStart = (receivedStatesStart + 1) % MAX_STATE_HISTORY_SNAPSHOTS;
	} else {
		m_receivedStates[numReceivedStates++] = item;
	}
}

void SRigidEntityNetStateHistory::PushReceivedSequenceDelta(uint8 delta) {
	if (numReceivedSequences >= MAX_SEQUENCE_HISTORY_SNAPSHOTS) {
		m_receivedSequenceDeltas[receivedSequencesStart] = delta;
		receivedSequencesStart = (receivedSequencesStart + 1) % MAX_SEQUENCE_HISTORY_SNAPSHOTS;
	} else {
		m_receivedSequenceDeltas[numReceivedSequences++] = delta;
	}
	if (receivedSequencesStart == 0 && numReceivedSequences == MAX_SEQUENCE_HISTORY_SNAPSHOTS) {
		// Recalculate the average every time it wraps round to avoid rounding errors building up too much
		for (int i=0; i<numReceivedSequences; ++i) {
			UpdateSequenceDeltaAverage(m_receivedSequenceDeltas[i], i+1);
		}
	}
	else {
		UpdateSequenceDeltaAverage(delta, numReceivedSequences);
	}
}

void SRigidEntityNetStateHistory::UpdateSequenceDeltaAverage(uint8 delta, int sampleCount) {
	if (sampleCount > 1) {
		float difference = delta - sequenceDeltaAverage;
		if (difference > MAX_SEQUENCE_NUMBER / 2.0f) {
			difference -= MAX_SEQUENCE_NUMBER;
		}
		else if (difference < -MAX_SEQUENCE_NUMBER / 2.0f) {
			difference += MAX_SEQUENCE_NUMBER;
		}
		const float weight = 1.0f / sampleCount;
		sequenceDeltaAverage += difference * weight;
	}
	else {
		sequenceDeltaAverage = delta;
	}
}

void CRigidEntity::UpdateStateFromNetwork() {
	float interpedSequence = GetInterpSequenceNumber();
	SRigidEntityNetSerialize interpolatedPos;
	if (GetInterpolatedState(interpedSequence, interpolatedPos)) {
		pe_params_pos setpos;	setpos.bRecalcBounds = 16|32|64;
		setpos.pos = interpolatedPos.pos;
		setpos.q = interpolatedPos.rot;
		setpos.iSimClass = interpolatedPos.simclass ? 2 : 1;
		SetParams( &setpos );

		pe_action_set_velocity setvel;
		setvel.v = interpolatedPos.vel;
		setvel.w = interpolatedPos.angvel;
		Action( &setvel );

		if (!interpolatedPos.simclass) {
			// Entity is now asleep, clear history
			WriteLock lock(m_lockNetInterp);
			m_pNetStateHistory->Clear();
		}
	}
}
#endif

int CRigidEntity::GetStateSnapshot(CStream &stm, float time_back, int flags)
{
	if (flags & ssf_checksum_only) {
		stm.Write(false);
		stm.Write(GetStateChecksum());
	}	else {
		stm.Write(true);
		stm.WriteNumberInBits(GetSnapshotVersion(),4);
		stm.Write(m_pos);
		if (m_pWorld->m_vars.bMultiplayer) {
			WriteCompressedQuat(stm,m_qrot);
			WriteCompressedVel(stm,m_body.v,50.0f);
			WriteCompressedVel(stm,m_body.w,20.0f);
		} else {
			stm.WriteBits((uint8*)&m_qrot,sizeof(m_qrot)*8);
			if (m_body.Minv>0) {
				stm.Write(m_body.P);
				stm.Write(m_body.L);	
			} else {
				stm.Write(m_body.v);
				stm.Write(m_body.w);
			}
		}
		if (m_Pext.len2()+m_Lext.len2()>0) {
			stm.Write(true);
			stm.Write(m_Pext);
			stm.Write(m_Lext);
		} else stm.Write(false);
		stm.Write(m_bAwake!=0);
		stm.Write(m_iSimClass>1);
		WriteContacts(stm,flags);
	}

	return 1;
}

void SRigidEntityNetSerialize::Serialize( TSerialize ser )
{
	// need to be carefull to avoid jittering (need to represent 0 somewhat accuratelly)
	ser.Value( "pos", pos, 'wrld');
	ser.Value( "rot", rot, 'ori1');
/*#if USE_IMPROVED_RIGID_ENTITY_SYNCHRONISATION
	if (ser.GetSerializationTarget()!=eST_Network)
#endif*/
	{
		ser.Value( "vel", vel, 'pRVl');
		ser.Value( "angvel", angvel, 'pRAV');
	}
	ser.Value( "simclass", simclass, 'bool');
#if USE_IMPROVED_RIGID_ENTITY_SYNCHRONISATION
	ser.Value( "sequenceNumber", sequenceNumber, 'ui8');
#endif
}

int CRigidEntity::GetStateSnapshot( TSerialize ser, float time_back, int flags )
{
	CPhysicalEntity::GetStateSnapshot(ser,time_back,flags);

#if USE_IMPROVED_RIGID_ENTITY_SYNCHRONISATION
	uint8 sequenceNumber = m_pWorld ? GetLocalSequenceNumber() : 0;
	sequenceNumber += m_sequenceOffset;		// Used to deal with authority changes
#endif

	SRigidEntityNetSerialize helper = {
		m_pos,
		m_qrot,
		m_body.v,
		m_body.w,
		m_bAwake != 0,
#if USE_IMPROVED_RIGID_ENTITY_SYNCHRONISATION
		sequenceNumber,
#endif
	};
	if (!m_bAwake)
		helper.vel = helper.angvel = ZERO;
	helper.Serialize( ser );

/*
	{
		IPersistantDebug * pPD = gEnv->pGameFramework->GetIPersistantDebug();
		char name[64];
		cry_sprintf(name, "Send_%p", this);
		pPD->Begin(name, true);
		pPD->AddQuat( helper.pos, helper.rot, 1.0f, ColorF(0,1,0,1), 1 );
	}
*/

	if (ser.GetSerializationTarget()!=eST_Network) {
		int i,count;
		bool bVal;

		if (flags & 16)
			WriteContacts(ser);

		if (ser.BeginOptionalGroup("constraints", m_constraintMask != 0))
		{
			int scount=0;
			for(i=scount=0;i<NMASKBITS && getmask(i)<=m_constraintMask;) {
				if (m_constraintMask & getmask(i) && !(m_pConstraintInfos[i].flags & constraint_rope))
				{
					pe_action_add_constraint aac;
					aac.id = m_pConstraintInfos[i].id;
					i += ExtractConstraintInfo(i,m_constraintMask,aac);
					++scount;
				}
				else
					++i;
			}

			ser.Value("count", scount);

			for(i=count=0;i<NMASKBITS && getmask(i)<=m_constraintMask;)
			{
				if (m_constraintMask & getmask(i) && !(m_pConstraintInfos[i].flags & constraint_rope))
				{
					ser.BeginOptionalGroup("constraint", true);
					pe_action_add_constraint aac;
					aac.id = m_pConstraintInfos[i].id;
					i += ExtractConstraintInfo(i,m_constraintMask,aac);
					ser.Value("id", aac.id);
					m_pWorld->SavePhysicalEntityPtr(ser,(CPhysicalEntity*)aac.pBuddy);
					ser.Value("pt0", aac.pt[0]); ser.Value("pt1", aac.pt[1]); 
					ser.Value("partid0", aac.partid[0]); ser.Value("partid1", aac.partid[1]); 
					if (!is_unused(aac.qframe[0])) {
						ser.Value("qf0used", bVal=true);
						ser.Value("qframe0", aac.qframe[0]);
					} else ser.Value("qf0used", bVal=false);
					if (!is_unused(aac.qframe[1])) {
						ser.Value("qf1used", bVal=true);
						ser.Value("qframe1", aac.qframe[1]);
					}	else ser.Value("qf1used", bVal=false);
					if (!is_unused(aac.xlimits[0])) {
						ser.Value("xlim", bVal=true);
						ser.Value("xlim0", aac.xlimits[0]); ser.Value("xlim1", aac.xlimits[1]);
					}	else ser.Value("xlim", bVal=false);
					if (!is_unused(aac.yzlimits[0])) {
						ser.Value("yzlim", bVal=true);
						ser.Value("yzlim0", aac.yzlimits[0]); ser.Value("yzlim1", aac.yzlimits[1]);
					}	else ser.Value("yzlim", bVal=false);
					ser.Value("flags", aac.flags);
					ser.Value("damping", aac.damping);
					ser.Value("radius", aac.sensorRadius);
					if (!is_unused(aac.maxPullForce)) {
						ser.Value("breakableLin", bVal=true);
						ser.Value("maxPullForce", aac.maxPullForce);
					} else
						ser.Value("breakableLin", bVal=false);
					if (!is_unused(aac.maxBendTorque)) {
						ser.Value("breakableAng", bVal=true);
						ser.Value("maxBendTorque", aac.maxBendTorque);
					} else
						ser.Value("breakableAng", bVal=false);
					ser.EndGroup();
					count++;
				} else
					i++;
			}
			ser.EndGroup();
		}
	}

	return 1;
}

int CRigidEntity::WriteContacts(CStream &stm,int flags)
{
	int i;
	entity_contact *pContact;

	WritePacked(stm, m_nColliders);
	for(i=0; i<m_nColliders; i++) {
		WritePacked(stm, m_pWorld->GetPhysicalEntityId(m_pColliders[i])+1);
		if (m_pColliderContacts[i]) {
			for(pContact=m_pColliderContacts[i];;pContact=pContact->next)
				if (pContact->flags & contact_last) break;
			for(;;pContact=pContact->prev) {
				stm.Write(true);
				//stm.Write(pContact->ptloc[0]);
				//stm.Write(pContact->ptloc[1]);
				stm.Write(pContact->nloc);
				WritePacked(stm, pContact->ipart[0]);
				WritePacked(stm, pContact->ipart[1]);
				WritePacked(stm, (int)pContact->iConstraint);
				WritePacked(stm, (int)pContact->iNormal);
				stm.Write((pContact->flags & contact_2b_verified)!=0);
				stm.Write(false);//pContact->vsep>0);
				stm.Write(pContact->penetration);
				if (pContact==m_pColliderContacts[i]) break;
			}
		}
		stm.Write(false);
	}

	return 1;
}

int CRigidEntity::WriteContacts(TSerialize ser)
{
	int n = 0;
	for(entity_contact *cnt=m_pContactStart; cnt!=CONTACT_END(m_pContactStart); cnt=cnt->next,n++) {
		ser.BeginOptionalGroup("contact", true);
		int i;
		ser.Value("ent1", i=cnt->pent[1]->m_id);
		ser.Value("ipart0", i=cnt->ipart[0]);
		ser.Value("ipart1", i=cnt->ipart[1]);
		ser.Value("pt", cnt->pt[0]);
		ser.Value("n", cnt->n);
		ser.Value("t", cnt->penetration);
		ser.Value("fric", cnt->friction);
		ser.EndGroup();
	}
	ser.BeginOptionalGroup("contact", false);
	return n;
}
int CRigidEntity::ReadContacts(TSerialize ser)
{
	DetachAllContacts();
	int n = 0;
	for(; ser.BeginOptionalGroup("contact", true); n++) {
		entity_contact *cnt = m_pWorld->AllocContact();
		int i; 
		ser.Value("ent1", i);
		cnt->pent[1] = (CPhysicalEntity*)m_pWorld->GetPhysicalEntityById(i);
		ser.Value("ipart0", i); cnt->ipart[0] = i;
		ser.Value("ipart1", i); cnt->ipart[1] = i;
		ser.Value("pt", cnt->pt[0]); cnt->pt[1]=cnt->pt[0];
		ser.Value("n", cnt->n);
		ser.Value("t", cnt->penetration);
		ser.Value("fric", cnt->friction);
		ser.EndGroup();
		if (!cnt->pent[1]) {
			m_pWorld->FreeContact(cnt); 
			continue;
		}
		cnt->pent[0] = this;
		cnt->vreq = cnt->n*min(m_pWorld->m_vars.maxUnprojVel, max(0.0f,cnt->penetration-m_pWorld->m_vars.maxContactGap)*m_pWorld->m_vars.unprojVelScale);
		for(i=0;i<2;i++)
			cnt->pbody[i] = cnt->pent[i]->GetRigidBody(cnt->ipart[i]);
		cnt->pent[1]->AddCollider(this);
		AttachContact(cnt, AddCollider(cnt->pent[1]), cnt->pent[1]);
	}
	return n;
}

int CRigidEntity::SetStateFromSnapshot(CStream &stm, int flags)
{
	bool bnz;
	int ver=0;
	int iMiddle,iBound[2]={ m_iLastChecksum+1-NCHECKSUMS,m_iLastChecksum+1 };
	unsigned int checksum,checksum_hist;
	coord_block_BBox partCoordTmp[2];
	if (m_pWorld->m_iTimeSnapshot[2]>=m_checksums[iBound[0]&NCHECKSUMS-1].iPhysTime &&
			m_pWorld->m_iTimeSnapshot[2]<=m_checksums[iBound[1]-1&NCHECKSUMS-1].iPhysTime)
	{
		do {
			iMiddle = iBound[0]+iBound[1]>>1;
			iBound[isneg(m_pWorld->m_iTimeSnapshot[2]-m_checksums[iMiddle&NCHECKSUMS-1].iPhysTime)] = iMiddle;
		} while(iBound[1]>iBound[0]+1);
		checksum_hist = m_checksums[iBound[0]&NCHECKSUMS-1].checksum;
	} else
		checksum_hist = GetStateChecksum();

#ifdef _DEBUG
	if (m_pWorld->m_iTimeSnapshot[2]!=0) {
		if (m_bAwake && m_checksums[iBound[0]&NCHECKSUMS-1].iPhysTime!=m_pWorld->m_iTimeSnapshot[2])
			m_pWorld->m_pLog->Log("Rigid Entity: time not in list (%d, bounds %d-%d) (id %d)", m_pWorld->m_iTimeSnapshot[2],
				m_checksums[iBound[0]&NCHECKSUMS-1].iPhysTime,m_checksums[iBound[1]&NCHECKSUMS-1].iPhysTime,m_id);
		if(m_bAwake && (m_checksums[iBound[0]-1&NCHECKSUMS-1].iPhysTime==m_checksums[iBound[0]&NCHECKSUMS-1].iPhysTime ||
			 m_checksums[iBound[0]+1&NCHECKSUMS-1].iPhysTime==m_checksums[iBound[0]&NCHECKSUMS-1].iPhysTime))
			m_pWorld->m_pLog->Log("Rigid Entity: 2 same times in history (id %d)",m_id);
	}
#endif

	stm.Read(bnz);
	if (!bnz) {
		stm.Read(checksum);
		m_flags |= ref_checksum_received;
		if (!(flags & ssf_no_update)) {
			m_flags &= ~ref_checksum_outofsync;
			m_flags |= ref_checksum_outofsync & ~-iszero((int)(checksum-checksum_hist));
			//if (m_flags & pef_checksum_outofsync)
			//	m_pWorld->m_pLog->Log("Rigid Entity %s out of sync (id %d)",
			//		m_pWorld->m_pPhysicsStreamer->GetForeignName(m_pForeignData,m_iForeignData,m_iForeignFlags), m_id);
		}
		return 2;
	}	else {
		m_flags = m_flags & ~ref_checksum_received;
		stm.ReadNumberInBits(ver,4);
		if (ver!=GetSnapshotVersion())
			return 0;

		if (!(flags & ssf_no_update)) {
			m_flags &= ~ref_checksum_outofsync;
			stm.Read(m_posNew);
			if (m_pWorld->m_vars.bMultiplayer) {
				ReadCompressedQuat(stm,m_qNew);
				m_body.pos = m_pos+m_qrot*m_body.offsfb;
				Matrix33 R = Matrix33(m_body.q = m_qrot*!m_body.qfb);
				ReadCompressedVel(stm,m_body.v,50.0f);
				ReadCompressedVel(stm,m_body.w,20.0f);
				m_body.P = m_body.v*m_body.M;
				m_body.L = m_body.q*(m_body.Ibody*(m_body.w*m_body.q));
				m_body.Iinv = R*m_body.Ibody_inv*R.T();
			} else {
				stm.ReadBits((uint8*)&m_qNew,sizeof(m_qrot)*8);
				if (m_body.Minv>0) {
					stm.Read(m_body.P);
					stm.Read(m_body.L);
				} else {
					stm.Read(m_body.v);
					stm.Read(m_body.w);
				}
				m_body.pos = m_pos+m_qrot*m_body.offsfb;
				m_body.q = m_qrot*!m_body.qfb;
				m_body.UpdateState();
			}
			stm.Read(bnz); if (bnz) {
				stm.Read(m_Pext);
				stm.Read(m_Lext);
			}
			stm.Read(bnz); m_bAwake = bnz ? 1:0;
			stm.Read(bnz); m_iSimClass = bnz ? 2:1;
			ComputeBBoxRE(partCoordTmp);
			UpdatePosition(m_pWorld->RepositionEntity(this,1,m_BBoxNew));

			m_iLastChecksum = iBound[0]+sgn(m_pWorld->m_iTimeSnapshot[2]-m_checksums[iBound[0]&NCHECKSUMS-1].iPhysTime) & NCHECKSUMS-1;
			m_checksums[m_iLastChecksum].iPhysTime = m_pWorld->m_iTimeSnapshot[2];
			m_checksums[m_iLastChecksum].checksum = GetStateChecksum();
		} else {
			stm.Seek(stm.GetReadPos()+sizeof(Vec3)*8 + 
				(m_pWorld->m_vars.bMultiplayer ? CMP_QUAT_SZ+CMP_VEL_SZ*2 : (sizeof(quaternionf)+2*sizeof(Vec3))*8));
			stm.Read(bnz); if (bnz)
				stm.Seek(stm.GetReadPos()+2*sizeof(Vec3)*8);
			stm.Seek(stm.GetReadPos()+2);
		}

		ReadContacts(stm, flags);
	}

	return 1;
}

void CRigidEntity::SetNetworkAuthority(int auth, int paused)
{
#if USE_IMPROVED_RIGID_ENTITY_SYNCHRONISATION
	if (auth==1 && !m_hasAuthority) {
		if (m_pNetStateHistory) {
			// Offset the sequence numbers so they match up with the previous owner of this entity
			uint8 prevSequenceNum = (uint8)GetInterpSequenceNumber();
			uint8 newSequenceNum = GetLocalSequenceNumber();
			m_sequenceOffset = prevSequenceNum - newSequenceNum;
		}
		// Reset the history
		WriteLock lock(m_lockNetInterp);
		if (!m_pNetStateHistory) {
			m_pNetStateHistory = new SRigidEntityNetStateHistory();
		}
		else {
			m_pNetStateHistory->Clear();
		}
		m_hasAuthority = true;
	}
	else if (auth==0 && m_hasAuthority) {
		// Reset the history
		WriteLock lock(m_lockNetInterp);
		if (!m_pNetStateHistory) {
			m_pNetStateHistory = new SRigidEntityNetStateHistory();
		}
		else {
			m_pNetStateHistory->Clear();
		}
		m_hasAuthority = false;
	}
	if (paused>=0 && m_pNetStateHistory)
		m_pNetStateHistory->SetPaused(paused);
#endif
}

#if USE_IMPROVED_RIGID_ENTITY_SYNCHRONISATION
uint8 CRigidEntity::GetLocalSequenceNumber() const
{
	const CTimeValue &frameStartTime = gEnv->pTimer->GetFrameStartTime();
	return (uint8)((frameStartTime.GetMilliSecondsAsInt64() * m_pWorld->m_vars.netSequenceFrequency) / 1000);
}

float CRigidEntity::GetInterpSequenceNumber()
{
	float sequenceDeltaAverage = 0.0f;
	{
			ReadLock lock(m_lockNetInterp);
			sequenceDeltaAverage = m_pNetStateHistory->GetAverageSequenceDelta();
	}
	float interpedSequence = GetLocalSequenceNumber() + sequenceDeltaAverage - m_pWorld->m_vars.netInterpTime * m_pWorld->m_vars.netSequenceFrequency;
	if (interpedSequence < 0.0f) {
		interpedSequence += MAX_SEQUENCE_NUMBER;
	}
	else if (interpedSequence > MAX_SEQUENCE_NUMBER) {
		interpedSequence -= MAX_SEQUENCE_NUMBER;
	}
	return interpedSequence;
}

bool CRigidEntity::GetInterpolatedState(float sequenceNumber, SRigidEntityNetSerialize &interpState)
{
	ReadLock lock(m_lockNetInterp);

	SRigidEntityNetSerialize* pLowerBound = NULL;
	SRigidEntityNetSerialize* pUpperBound = NULL;
	for (int i=m_pNetStateHistory->GetNumReceivedStates()-1; i>=0; --i) {
		SRigidEntityNetSerialize& state = m_pNetStateHistory->GetReceivedState(i);
		float difference = state.sequenceNumber - sequenceNumber;
		if (difference > MAX_SEQUENCE_NUMBER / 2.0f) {
			difference -= MAX_SEQUENCE_NUMBER;
		}
		else if (difference < -MAX_SEQUENCE_NUMBER / 2.0f) {
			difference += MAX_SEQUENCE_NUMBER;
		}
		if (difference > 0.0f) {
			pUpperBound = &state;
		}
		else {
			pLowerBound = &state;
			break;
		}
	}

	if (!pUpperBound && !pLowerBound) {
		return false;
	}

	if (!pUpperBound) {
		interpState = *pLowerBound;
		if (interpState.simclass) {
			// Extrapolate from lower bound if the entity is still awake
			float sequenceDiff = sequenceNumber - pLowerBound->sequenceNumber;
			if (sequenceDiff < 0.0f) {
				sequenceDiff += MAX_SEQUENCE_NUMBER;
			}
			float extrapTime = sequenceDiff / m_pWorld->m_vars.netSequenceFrequency;
			if (extrapTime > m_pWorld->m_vars.netExtrapMaxTime)
			{
				extrapTime = m_pWorld->m_vars.netExtrapMaxTime;
				interpState.simclass = false;	// Send the entity to sleep, will result in history being erased
			}
			interpState.pos += extrapTime * interpState.vel;
			float wlen = interpState.angvel.len();
			if (wlen > FLT_EPSILON) {
				interpState.rot = Quat::CreateRotationAA(wlen*extrapTime,interpState.angvel/wlen)*interpState.rot;
				interpState.rot.Normalize();
			}
		}
		return true;
	}

	if (!pLowerBound) {
		// Return the oldest received state
		interpState = *pUpperBound;
		return true;
	}

	float seq1 = pLowerBound->sequenceNumber, seq2 = sequenceNumber, seq3 = pUpperBound->sequenceNumber;
	if (seq2 < seq1) {
		seq2 += MAX_SEQUENCE_NUMBER;
		seq3 += MAX_SEQUENCE_NUMBER;
	}
	if (seq3 < seq2) {
		seq3 += MAX_SEQUENCE_NUMBER;
	}
	float t = (seq2 - seq1) / (seq3 - seq1);

	interpState.pos.SetLerp(pLowerBound->pos, pUpperBound->pos, t);
	interpState.rot.SetSlerp(pLowerBound->rot, pUpperBound->rot, t);
	interpState.vel.SetLerp(pLowerBound->vel, pUpperBound->vel, t);
	interpState.angvel.SetLerp(pLowerBound->angvel, pUpperBound->angvel, t);
	interpState.simclass = pLowerBound->simclass;
	interpState.sequenceNumber = (uint8)sequenceNumber;
	return true;
}
#endif

int CRigidEntity::SetStateFromSnapshot( TSerialize ser, int flags )
{
	CPhysicalEntity::SetStateFromSnapshot(ser,flags);

	SRigidEntityNetSerialize helper = {
		m_pos,
		m_qrot,
		m_body.v,
		m_body.w,
		m_iSimClass > 1,
#if USE_IMPROVED_RIGID_ENTITY_SYNCHRONISATION
		0,
#endif
	};
	helper.Serialize( ser );

	if ((ser.GetSerializationTarget()==eST_Network) && (helper.pos.len2()<50.0f))
		return 1;

	if (m_pWorld && ser.ShouldCommitValues())
	{
		pe_params_pos setpos;	setpos.bRecalcBounds = 16|32|64;
		pe_action_set_velocity velocity;

//		Vec3 debugOnlyOriginalHelperPos = helper.pos;

#if !USE_IMPROVED_RIGID_ENTITY_SYNCHRONISATION
		if ((flags & ssf_compensate_time_diff) && helper.simclass)
		{
			float dtBack=(m_pWorld->m_iTimeSnapshot[1]-m_pWorld->m_iTimeSnapshot[0])*m_pWorld->m_vars.timeGranularity;
			helper.pos += helper.vel * dtBack;

 			if (helper.angvel.len2()*sqr(dtBack)<sqr(0.003f))
			{
				//quaternionf q(0.0f, helper.angvel*0.5f); //q.Normalize();
				//helper.rot += q*helper.rot*dtBack;
				helper.rot.w -= (helper.angvel*helper.rot.v)*dtBack*0.5f;
				helper.rot.v += ((helper.angvel^helper.rot.v)+helper.angvel*helper.rot.w)*(dtBack*0.5f);
			}
			else
			{
				float wlen = helper.angvel.len();
				//q = quaternionf(wlen*dt,w/wlen)*q;
				helper.rot = Quat::CreateRotationAA(wlen*dtBack,helper.angvel/wlen)*helper.rot;
			}
			helper.rot.Normalize();
		}

		float distance = m_pos.GetDistance(helper.pos);
#endif

/*
		{
			IPersistantDebug * pPD = gEnv->pGameFramework->GetIPersistantDebug();
			char name[64];
			cry_sprintf(name, "Snap_%p", this);
			pPD->Begin(name, true);
			pPD->AddQuat( helper.pos, helper.rot, distance, ColorF(0,1,0,1), 1 );
			pPD->AddQuat( m_pos, m_qrot, distance, ColorF(1,0,0,1), 1 );
		}
*/

		if (m_body.Minv || ser.GetSerializationTarget() != eST_Network)
		{
#if !USE_IMPROVED_RIGID_ENTITY_SYNCHRONISATION
			if (helper.simclass && ser.GetSerializationTarget() == eST_Network)
			{
				const float MAX_DIFFERENCE = m_pWorld->m_vars.netMinSnapDist + m_pWorld->m_vars.netVelSnapMul * helper.vel.GetLength();
				if (distance > MAX_DIFFERENCE)
					setpos.pos = helper.pos;
				else
					setpos.pos = m_pos + (helper.pos - m_pos) * clamp_tpl( distance/MAX_DIFFERENCE / std::max(m_pWorld->m_vars.netSmoothTime, 0.01f), 0.0f, 1.0f );
				if (helper.vel.GetLength() < 0.1f && distance > 0.05f)
					ser.FlagPartialRead();

				const float MAX_DOT = std::max( 0.01f, 1.0f - m_pWorld->m_vars.netMinSnapDot - m_pWorld->m_vars.netAngSnapMul * helper.angvel.GetLength() );
				float quatDiff = 1.0f - fabsf(m_qrot | helper.rot);
				if (quatDiff > MAX_DOT)
					setpos.q = helper.rot;
				else
					setpos.q = Quat::CreateSlerp(m_qrot, helper.rot, clamp_tpl(quatDiff/MAX_DOT / std::max(m_pWorld->m_vars.netSmoothTime, 0.01f), 0.0f, 1.0f ));
				if (helper.angvel.GetLength() < 0.1f && quatDiff > 0.05f)
					ser.FlagPartialRead();
			}
			else
			{
				setpos.pos = helper.pos;
				setpos.q = helper.rot;
			}

			setpos.iSimClass = helper.simclass? 2 : 1;
			SetParams( &setpos,0 );

			velocity.v = helper.vel;
			velocity.w = helper.angvel;
			Action( &velocity,0 );
#else
			if (ser.GetSerializationTarget() == eST_Network)
			{
				{
					WriteLock lock(m_lockNetInterp);
					if (!m_pNetStateHistory) {
						m_pNetStateHistory = new SRigidEntityNetStateHistory();
					}

					m_pNetStateHistory->PushReceivedState(helper);
					uint8 sequenceDelta = helper.sequenceNumber - GetLocalSequenceNumber();
					m_pNetStateHistory->PushReceivedSequenceDelta(sequenceDelta);
				}

				if (helper.simclass)
				{
					pe_action_awake wakeUp;
					Action( &wakeUp,0);
				}
			}
			else
			{
				setpos.pos = helper.pos;
				setpos.q = helper.rot;
				setpos.iSimClass = helper.simclass? 2 : 1;
				SetParams( &setpos,0 );

				velocity.v = helper.vel;
				velocity.w = helper.angvel;
				Action( &velocity,0 );
			}
#endif
		}
	}


	if (ser.GetSerializationTarget()!=eST_Network)
	{
		if (flags & 16)
			ReadContacts(ser);

		if (ser.BeginOptionalGroup("constraints",true))
		{
			int i,j;
			pe_action_update_constraint auc; auc.bRemove = 1;
			for(i=NMASKBITS-1;i>=0;) if (m_constraintMask & getmask(i) && !(m_pConstraintInfos[i].flags & constraint_rope)) {
				auc.idConstraint = m_pConstraintInfos[i].id;
				for(j=i;i>=0 && m_pConstraintInfos[i].id==m_pConstraintInfos[j].id;i--);
				Action(&auc,0);
			}	else i--;

			int count = 0;
			ser.Value("count", count);
			while(--count>=0)	if (ser.BeginOptionalGroup("constraint", true))
			{
				bool bVal;
				pe_action_add_constraint aac;
				ser.Value("id", aac.id);
				PREFAST_ASSUME(m_pWorld);
				if (aac.pBuddy=m_pWorld->LoadPhysicalEntityPtr(ser))
				{
					ser.Value("pt0", aac.pt[0]); ser.Value("pt1", aac.pt[1]); 
					ser.Value("partid0", aac.partid[0]); ser.Value("partid1", aac.partid[1]); 
					ser.Value("qf0used", bVal);
					if (bVal)
						ser.Value("qframe0", aac.qframe[0]); 
					ser.Value("qf1used", bVal);
					if (bVal)
						ser.Value("qframe1", aac.qframe[1]); 
					ser.Value("xlim", bVal);
					if (bVal) {
						ser.Value("xlim0", aac.xlimits[0]); ser.Value("xlim1", aac.xlimits[1]);
					}
					ser.Value("yzlim", bVal);
					if (bVal) {
						ser.Value("yzlim0", aac.yzlimits[0]); ser.Value("yzlim1", aac.yzlimits[1]);
					}
					ser.Value("flags", aac.flags);
					ser.Value("damping", aac.damping);
					ser.Value("radius", aac.sensorRadius);
					ser.Value("breakableLin", bVal);
					if (bVal)
						ser.Value("maxPullForce", aac.maxPullForce);
					ser.Value("breakableAng", bVal);
					if (bVal)
						ser.Value("maxBendTorque", aac.maxBendTorque);
					if (ser.ShouldCommitValues())
						Action(&aac,0);
				}
				ser.EndGroup(); //constrain
			}
			ser.EndGroup(); //constrains
		}
		m_nEvents = 0;
	}

	return 1;
}

int CRigidEntity::ReadContacts(CStream &stm, int flags)
{
	int i,j,id; bool bnz;
	pe_status_id si;
	int idPrevColliders[16],nPrevColliders=0;
	masktype iPrevConstraints[16];
	entity_contact *pContact,*pContactNext;

	if (!(flags & ssf_no_update)) {
		for(i=0;i<m_nColliders;i++) {
			if (m_pColliderConstraints[i] && nPrevColliders<16) {
				idPrevColliders[nPrevColliders] = m_pWorld->GetPhysicalEntityId(m_pColliders[i]);
				iPrevConstraints[nPrevColliders++] = m_pColliderConstraints[i];
			}
			if (m_pColliders[i]!=this) {
				m_pColliders[i]->RemoveColliderMono(this);
				m_pColliders[i]->Release();
			}
		}

		ReadPacked(stm,m_nColliders);
		if (m_nCollidersAlloc<m_nColliders) {
			if (m_pColliders) delete[] m_pColliders;
			if (m_pColliderContacts) delete[] m_pColliderContacts;
			m_nCollidersAlloc = (m_nColliders-1&~7)+8;
			m_pColliders = new CPhysicalEntity*[m_nCollidersAlloc];
			m_pColliderContacts = new entity_contact*[m_nCollidersAlloc];
			memset(m_pColliderConstraints = new masktype[m_nCollidersAlloc],0,m_nCollidersAlloc*sizeof(m_pColliderConstraints[0]));
		}
		for(pContact=m_pContactStart; pContact!=CONTACT_END(m_pContactStart); pContact=pContactNext) {
			pContactNext=pContact; m_pWorld->FreeContact(pContact);
		}

		for(i=0;i<m_nColliders;i++) {
			ReadPacked(stm,id); --id;
			m_pColliders[i] = (CPhysicalEntity*)(UINT_PTR)id;
			m_pColliderConstraints[i] = 0;
			while(true) {
				stm.Read(bnz);
				if (!bnz) break;
				pContact = m_pWorld->AllocContact();
				AttachContact(pContact,i,m_pColliders[i]);

				//stm.Read(pContact->ptloc[0]);
				//stm.Read(pContact->ptloc[1]);
				stm.Read(pContact->nloc);
				ReadPacked(stm, j); pContact->ipart[0]=j;
				ReadPacked(stm, j); pContact->ipart[1]=j;
				ReadPacked(stm, j); pContact->iConstraint=j;
				int iNormal; ReadPacked(stm,iNormal); pContact->iNormal=iNormal;
				stm.Read(bnz); pContact->flags = bnz ? contact_2b_verified : 0;
				stm.Read(bnz); //pContact->vsep = bnz ? m_pWorld->m_vars.minSeparationSpeed : 0;
				stm.Read(pContact->penetration);

				pContact->pent[0] = this;
				pContact->pent[1] = (CPhysicalEntity*)(UINT_PTR)id;
				pContact->pbody[0] = pContact->pent[0]->GetRigidBody(pContact->ipart[0]);
				si.ipart = pContact->ipart[0];
				//si.iPrim = pContact->iPrim[0];
				//si.iFeature = pContact->iFeature[0];
				pContact->pent[0]->GetStatus(&si);
				pContact->id0 = si.id;
				//pContact->penetration = 0;
				//pContact->pt[0] = pContact->pt[1] = 
				//	pContact->pbody[0]->q*pContact->ptloc[0]+pContact->pbody[0]->pos;
			}
		}
		for(i=0;i<nPrevColliders;i++)	{
			for(j=0;j<m_nColliders && (int)(INT_PTR)m_pColliders[j]!=idPrevColliders[i];j++);
			if (j<m_nColliders)
				m_pColliderConstraints[j] = iPrevConstraints[i];
		}
		m_bJustLoaded = m_nColliders;

		m_nColliders = 0; // so that no1 thinks we have colliders until postload is called
		if (!m_nParts && m_bJustLoaded) {
			m_pWorld->m_pLog->LogError("Error: Rigid Body (@%.1f,%.1f,%.1f; id %d) lost its geometry after loading",m_pos.x,m_pos.y,m_pos.z,m_id);
			m_bJustLoaded = 0;
		}
	} else {
		int nColliders;
		ReadPacked(stm,nColliders); ReadPacked(stm,id);
		for(i=0;i<nColliders;i++) {
			ReadPacked(stm,id); 
			while(true) {
				stm.Read(bnz); if (!bnz) break;
				stm.Seek(stm.GetReadPos()+3*sizeof(Vec3)*8);
				ReadPacked(stm,id);ReadPacked(stm,id);ReadPacked(stm,id);ReadPacked(stm,id);
				ReadPacked(stm,id);ReadPacked(stm,id);ReadPacked(stm,id);
				stm.Seek(stm.GetReadPos()+2+sizeof(float)*8);
			}
		}
	}

	return 1;
}


int CRigidEntity::PostSetStateFromSnapshot()
{
	if (m_bJustLoaded) {
		pe_status_id si;
		int i,j;
		entity_contact *pContact,*pContactNext;
		m_nColliders = m_bJustLoaded;
		m_bJustLoaded = 0;

		for(pContact=m_pContactStart; pContact!=CONTACT_END(m_pContactStart); pContact=pContactNext) {
			pContactNext = pContact;
			pContact->pent[1] = (CPhysicalEntity*)m_pWorld->GetPhysicalEntityById((INT_PTR)pContact->pent[1]);
			if (pContact->pent[1] && (unsigned int)pContact->ipart[0]<(unsigned int)pContact->pent[0]->m_nParts &&
					(unsigned int)pContact->ipart[1]<(unsigned int)pContact->pent[1]->m_nParts) 
			{
				pContact->pbody[1] = pContact->pent[1]->GetRigidBody(pContact->ipart[1],1);
				si.ipart = pContact->ipart[1];
				//si.iPrim = pContact->iPrim[1];
				//si.iFeature = pContact->iFeature[1];
				if (pContact->pent[1]->GetStatus(&si)) {
					pContact->id1 = si.id;
					pContact->n = pContact->pbody[pContact->iNormal]->q*pContact->nloc;
					pContact->vreq = pContact->n*min(1.2f,pContact->penetration*10.0f);
					//pContact->K.SetZero();
					//pContact->pent[0]->GetContactMatrix(pContact->pt[0], pContact->ipart[0], pContact->K);
					//pContact->pent[1]->GetContactMatrix(pContact->pt[1], pContact->ipart[1], pContact->K);
					// assume that the contact is static here, for if it's dynamic, VerifyContacts will update it before passing to the solver
					pContact->vrel = 0;
					pContact->friction = m_pWorld->GetFriction(pContact->id0,pContact->id1);
				}	else {
					DetachContact(pContact,-1,0); m_pWorld->FreeContact(pContact);
				}
			} else {
				DetachContact(pContact,-1,0);	m_pWorld->FreeContact(pContact);
			}
		}

		for(i=m_nColliders-1;i>=0;i--) {
			if (!(m_pColliders[i] = (CPhysicalEntity*)m_pWorld->GetPhysicalEntityById((INT_PTR)m_pColliders[i])) || 
					!m_pColliderContacts[i] && !m_pColliderConstraints[i]) 
			{
				for(j=i;j<m_nColliders;j++) {
					m_pColliders[j] = m_pColliders[j+1];
					m_pColliderContacts[j] = m_pColliderContacts[j+1];
					m_pColliderConstraints[j] = m_pColliderConstraints[j+1];
				}
				m_nColliders--;
			}
		}
		for(i=0;i<m_nColliders;i++)
		{
			PREFAST_ASSUME(m_pColliders[i]);
			m_pColliders[i]->AddRef();
		}
	}
	return 1;
}


unsigned int CRigidEntity::GetStateChecksum()
{
	return //*(unsigned int*)&m_testval;
		float2int(m_pos.x*1024)^float2int(m_pos.y*1024)^float2int(m_pos.z*1024)^
		float2int(m_qrot.v.x*1024)^float2int(m_qrot.v.y*1024)^float2int(m_qrot.v.z*1024)^
		float2int(m_body.v.x*1024)^float2int(m_body.v.y*1024)^float2int(m_body.v.z*1024)^
		float2int(m_body.w.x*1024)^float2int(m_body.w.y*1024)^float2int(m_body.w.z*1024);
}


void CRigidEntity::ApplyBuoyancy(float time_interval,const Vec3& gravity,pe_params_buoyancy *pb,int nBuoys)
{
	if (m_kwaterDensity==0 || m_body.Minv==0) {
		m_submergedFraction=0; return;
	}
	int i,ibuoy,ibuoySplash=-1;
	Vec3 sz,center,dP,totResistance=Vec3(ZERO),vel0=m_body.v; 
	float r,dist,V=0.0f,Vsubmerged,Vfull,submergedFraction,waterFraction=0,g=gravity.len();
	geom_world_data gwd;
	pe_action_impulse ai;
	RigidBody *pbody;
	box bbox;
	ai.iSource = 1;
	ai.iApplyTime = 0;
	m_bFloating = 0;

	for(ibuoy=0;ibuoy<nBuoys;ibuoy++) if ((unsigned int)pb[ibuoy].iMedium<2u) {
		float resistance = pb[ibuoy].waterResistance * m_kwaterResistance;
		float density = pb[ibuoy].waterDensity * m_kwaterDensity;

		if (resistance + density + pb[ibuoy].waterDamping == 0.f)
			continue;

		sz = (m_BBoxNew[1]-m_BBoxNew[0])*0.5f; center = (m_BBoxNew[1]+m_BBoxNew[0])*0.5f;
		r = fabsf(pb[ibuoy].waterPlane.n.x*sz.x)+fabsf(pb[ibuoy].waterPlane.n.y*sz.y)+fabsf(pb[ibuoy].waterPlane.n.z*sz.z);
		dist = (center-pb[ibuoy].waterPlane.origin)*pb[ibuoy].waterPlane.n;
		if (dist>r)
			continue;

		for(i=0,Vsubmerged=Vfull=0;i<m_nParts;i++) if (m_parts[i].flags & geom_floats)
		{
			gwd.R = Matrix33(m_qNew*m_parts[i].pNewCoords->q);
			gwd.offset = m_posNew + m_qNew*m_parts[i].pNewCoords->pos;
			gwd.scale = m_parts[i].pNewCoords->scale;
			pbody = GetRigidBody(i);
			gwd.v = pbody->v-pb[ibuoy].waterFlow; gwd.w = pbody->w; gwd.centerOfMass = pbody->pos;

			const float accel_thresh = 0.01f;
			if (sqr(m_parts[i].pPhysGeomProxy->V)*cube(sqr(m_parts[i].scale)*gwd.v.len()*resistance*m_body.Minv) > cube(accel_thresh)) {
				m_parts[i].pPhysGeomProxy->pGeom->CalculateMediumResistance(&pb[ibuoy].waterPlane,&gwd,ai.impulse,ai.angImpulse);
				ai.impulse  *= resistance; 
				totResistance += ai.impulse*-(pb[ibuoy].iMedium-1>>31);
				ibuoySplash += ibuoy-ibuoySplash & pb[ibuoy].iMedium-1>>31;
				ai.impulse *= time_interval;
				ai.angImpulse *= resistance*time_interval;
			} else {
				ai.impulse.zero();
				ai.angImpulse.zero();
			}

			const float lift_thresh = 0.01f;
			if (m_parts[i].pPhysGeomProxy->V*cube(m_parts[i].scale)*density*m_body.Minv > lift_thresh) {
				if (dist>-r) {
					V = m_parts[i].pPhysGeomProxy->pGeom->CalculateBuoyancy(&pb[ibuoy].waterPlane,&gwd,center);
					dP = pb[ibuoy].waterPlane.n*(g*density*V*time_interval);
					ai.impulse += dP; ai.angImpulse += center-pbody->pos^dP;
				} else {
					V = m_parts[i].pPhysGeomProxy->V*cube(m_parts[i].scale);
					ai.impulse -= gravity*(density*V*time_interval);
				}
			}

			ai.ipart = i;
			if (ai.impulse.len2()+ai.angImpulse.len2()>0)
				Action(&ai,1);
			Vsubmerged += V; Vfull += m_parts[i].pPhysGeomProxy->V;
		}
		if (Vfull*Vsubmerged > 0) {
			submergedFraction = Vsubmerged<Vfull ? Vsubmerged/Vfull : 1.0f;
			m_dampingEx = max(m_dampingEx, m_damping*(1-submergedFraction)+max(m_waterDamping*max(0,1-pb[ibuoy].iMedium),pb[ibuoy].waterDamping)*submergedFraction);
			if (pb[ibuoy].iMedium==0) {
				waterFraction = max(waterFraction, submergedFraction);
				if (m_body.M < pb[ibuoy].waterDensity*m_kwaterDensity*m_body.V)
					m_bFloating = 1;
			}
		}
	}
	m_submergedFraction = waterFraction;

	if (m_pWorld->m_pWaterMan && !m_pWorld->m_pWaterMan->m_pArea && inrange(m_submergedFraction,0.001f,0.999f))
		m_pWorld->m_pWaterMan->OnWaterInteraction(this);

	float R0=m_pWorld->m_vars.minSplashForce0, R1=m_pWorld->m_vars.minSplashForce1,
				v0=m_pWorld->m_vars.minSplashVel0, v1=m_pWorld->m_vars.minSplashVel1,
				d0=m_pWorld->m_vars.splashDist0, d1=m_pWorld->m_vars.splashDist1,
				rd=isqrt_tpl(max((m_body.pos-m_pWorld->m_posViewer).len2(), d0*d0));
	if (m_pWorld->m_vars.maxSplashesPerObj*(ibuoySplash+1)>0 && !(m_flags & ref_no_splashes) && m_submergedFraction<0.9f && 
			1.0f<d1*rd && max(totResistance.len2()*rd*rd*sqr(d1-d0) - sqr(R1-R0+(R0*d1-R1*d0)*rd), 
												(pb[ibuoySplash].waterPlane.n*vel0)*rd*(d0-d1) - (v1-v0+(v0*d1-v1*d0)*rd))>0) 
	{
		int icont,icirc,nSplashes=0;
		intersection_params ip;
		geom_contact *pcont;
		CBoxGeom gbox;
		box abox;
		Vec2 *centers;
		float *radii;
		EventPhysCollision epc;

		epc.n = pb[ibuoySplash].waterPlane.n;
		abox.center = (m_BBoxNew[1]+m_BBoxNew[0])*0.5f;
		Vec3 size = m_BBoxNew[1]-m_BBoxNew[0];
		abox.size = Vec3(max(max(size.x,size.y),size.z));
		abox.center += epc.n*(epc.n*(pb[ibuoySplash].waterPlane.origin-abox.center)-abox.size.z);
		abox.Basis.SetRotationV0V1(epc.n, Vec3(0,0,1)); abox.bOriented=1;
		gbox.CreateBox(&abox);
		gwd.v.zero(); gwd.w.zero();
		ip.bNoAreaContacts=1; ip.vrel_min=1E10f;
		ip.bThreadSafe = 1;
		epc.pEntity[0]=this; epc.pForeignData[0]=m_pForeignData; epc.iForeignData[0]=m_iForeignData;
		epc.pEntity[1]=&g_StaticPhysicalEntity; epc.pForeignData[1]=0; epc.iForeignData[1]=0; epc.idCollider=-2;
		epc.vloc[1].zero();	epc.mass[1]=0; epc.partid[1]=0;
		epc.idmat[1] = m_pWorld->m_matWater;
		epc.penetration = m_submergedFraction;
		epc.normImpulse = totResistance*epc.n;
		epc.mass[0] = m_body.M;

		if ((epc.radius = abox.size.x)<0.5f) {
			epc.pt = abox.center+epc.n*abox.size.x;
			epc.vloc[0] = m_body.v; 
			epc.partid[0] = m_parts[0].id;
			epc.idmat[0] = GetMatId(m_parts[0].pPhysGeom->pGeom->GetPrimitiveId(0,0), i);
			m_pWorld->OnEvent(pef_log_collisions, &epc);
		}	else for(i=0; i<m_nParts; i++) if (m_parts[i].flags & geom_floats) {
			gwd.R = Matrix33(m_qNew*m_parts[i].pNewCoords->q);
			gwd.offset = m_posNew + m_qNew*m_parts[i].pNewCoords->pos;
			gwd.scale = m_parts[i].pNewCoords->scale;
			if (m_parts[i].pPhysGeomProxy->pGeom->GetType()==GEOM_SPHERE) {
				epc.pt = gwd.offset + m_parts[i].q*m_parts[i].pPhysGeomProxy->origin*m_parts[i].scale;
				epc.radius = ((const sphere*)m_parts[i].pPhysGeomProxy->pGeom->GetData())->r*m_parts[i].scale;
				float depth = epc.n*(pb[ibuoySplash].waterPlane.origin-epc.pt);
				if (fabs_tpl(depth) > epc.radius)
					continue;
				epc.radius = sqrt_tpl(sqr(epc.radius)-sqr(depth));
				epc.pt += epc.n*depth;
				epc.idmat[0] = GetMatId(-1, i);
				icont=icirc=0; goto GotASplash;
			} else if (icont=m_parts[i].pPhysGeomProxy->pGeom->Intersect(&gbox,&gwd,0,&ip,pcont))
				for(--icont; icont>=0; icont--) {
					for(icirc=CoverPolygonWithCircles(strided_pointer<Vec2>((Vec2*)pcont[icont].ptborder,sizeof(Vec3)), pcont[icont].nborderpt, 
							pcont[icont].bBorderConsecutive, Vec2(pcont[icont].center), centers,radii, 0.5f)-1; icirc>=0; icirc--)
					{
						epc.pt.x = centers[icirc].x; epc.pt.y = centers[icirc].y; epc.pt.z = pcont[icont].center.z;	
						epc.radius = radii[icirc];
						epc.idmat[0] = GetMatId(pcont[icont].id[0], i);
						GotASplash:
						epc.vloc[0] = m_body.v + (m_body.w^epc.pt-m_body.pos); 
						epc.partid[0] = m_parts[i].id;
						m_pWorld->OnEvent(pef_log_collisions, &epc);
						if (++nSplashes==m_pWorld->m_vars.maxSplashesPerObj)
							goto SplashesNoMore;
					}
				}
		}
		SplashesNoMore:;
	}
}


int CRigidEntity::CompactContactBlock(entity_contact *pContact,int endFlags, float maxPlaneDist, int nMaxContacts,int &nContacts,
																			entity_contact *&pResContact, Vec3 &n,float &maxDist, const Vec3 &ptTest, const Vec3 &dirTest) const
{
	int i,nEdges;
	Matrix33 C,Ctmp;
	Vec3 pt,p_avg,center,axes[2];
	float detabs[3],det;
	const int NMAXCONTACTS = 256;
	ptitem2d pts[NMAXCONTACTS];
	edgeitem edges[NMAXCONTACTS],*pedge,*pminedge;
	entity_contact *pContacts[NMAXCONTACTS];

	nContacts=0; p_avg.zero();
	if (pContact!=CONTACT_END(m_pContactStart)) do {
		if (!(pContact->flags & contact_2b_verified)) {
			p_avg += pContact->pt[0]; pts[nContacts].iContact = nContacts; pContacts[nContacts++] = pContact;
		}
	}	while(nContacts<NMAXCONTACTS && !(pContact->flags & endFlags) && (pContact=pContact->next)!=CONTACT_END(m_pContactStart));
	if (nContacts<=nMaxContacts && dirTest.len2()==0 || nContacts<3)
		return 0;

	center = p_avg/nContacts;
	for(i=0,C.SetZero(); i<nContacts; i++) // while it's possible to calculate C and center in one pass, it will lose precision
		C += dotproduct_matrix(pContacts[i]->pt[0]-center, pContacts[i]->pt[0]-center, Ctmp);
	detabs[0] = fabs_tpl(C(1,1)*C(2,2)-C(2,1)*C(1,2));
	detabs[1] = fabs_tpl(C(2,2)*C(0,0)-C(0,2)*C(2,0));
	detabs[2] = fabs_tpl(C(0,0)*C(1,1)-C(1,0)*C(0,1));
	i = idxmax3(detabs);
	if (detabs[i] <= fabs_tpl(C(inc_mod3[i],inc_mod3[i])*C(dec_mod3[i],dec_mod3[i]))*0.001f)
		return 0;
	det = 1.0f/(C(inc_mod3[i],inc_mod3[i])*C(dec_mod3[i],dec_mod3[i])-C(dec_mod3[i],inc_mod3[i])*C(inc_mod3[i],dec_mod3[i]));
	n[i] = 1;
	n[inc_mod3[i]] = -(C(inc_mod3[i],i)*C(dec_mod3[i],dec_mod3[i]) - C(dec_mod3[i],i)*C(inc_mod3[i],dec_mod3[i]))*det;
	n[dec_mod3[i]] = -(C(inc_mod3[i],inc_mod3[i])*C(dec_mod3[i],i) - C(dec_mod3[i],inc_mod3[i])*C(inc_mod3[i],i))*det;
	n.normalize();
	//axes[0]=n.orthogonal(); axes[1]=n^axes[0];
	axes[0].SetOrthogonal(n); axes[1]=n^axes[0];
	for(i=0,maxDist=0; i<nContacts; i++) {
		pt = pContacts[i]->pt[0]-center;
		maxDist = max(maxDist, fabsf(pt*n));
		pts[i].pt.set(pt*axes[0],pt*axes[1]);
	}
	if (maxDist>maxPlaneDist)
		return 0;
	
	if (nEdges = qhull2d(pts,nContacts,pminedge=edges,nMaxContacts))
		for(pminedge=edges; !pminedge->next; pminedge++);

	for(i=nContacts=0,pedge=pminedge; i<nEdges; i++,nContacts++,pedge=pedge->next)	{
		pContacts[pedge->pvtx->iContact]->nextAux = pContacts[pedge->next->pvtx->iContact];
		//pContacts[pedge->pvtx->iContact]->prevAux = pContacts[pedge->prev->pvtx->iContact];
	}
	pResContact = pContacts[pminedge->pvtx->iContact];

	if (dirTest.len2()>0) {
		if (nEdges<3 || sqr(det = dirTest*n)<dirTest.len2()*0.001f)
			return 0;
		pt = ptTest-center+dirTest*(((center-ptTest)*n)/det);
		Vec2 pt2d(axes[0]*pt,axes[1]*pt);
		for(i=0; i<nEdges; i++,pminedge=pminedge->next) 
		if ((pt2d-pminedge->pvtx->pt ^ pminedge->next->pvtx->pt-pminedge->pvtx->pt)>0)
			return 0;
	}

	return 1;
}


int CRigidEntity::ExtractConstraintInfo(int i, masktype constraintMask, pe_action_add_constraint &aac)
{
	int i1,j;
	int realent = m_pConstraints[i].pent[1]->m_iSimClass<3 || m_pConstraints[i].pent[1]->GetType()==PE_ARTICULATED;
	QuatT frames[2]; GetContactFrames(m_pConstraints[i], frames, realent+1);
	if (!realent) {
		pe_status_pos sp;
		m_pConstraints[i].pent[1]->GetStatus(&sp);
		frames[1] = QuatT(sp.q,sp.pos);
		aac.partid[1] = 0;
	}
	aac.pBuddy = m_pConstraints[i].pent[1];
	aac.pt[0] = frames[0]*m_pConstraintInfos[i].ptloc[0]; 
	aac.pt[1] = frames[1]*m_pConstraintInfos[i].ptloc[1];
	aac.id = m_pConstraintInfos[i].id;
	if (m_pConstraintInfos[i].limit>0)
		aac.maxPullForce = m_pConstraintInfos[i].limit;
	aac.damping = m_pConstraintInfos[i].damping;
	aac.flags = world_frames | m_pConstraintInfos[i].flags;
	aac.sensorRadius = m_pConstraintInfos[i].sensorRadius;
	for(j=0;j<realent+1;j++) aac.partid[j] = m_pConstraints[i].pent[j]->m_parts[m_pConstraints[i].ipart[j]].id;
	if (i+1<NMASKBITS && constraintMask & getmask(i+1) && m_pConstraintInfos[i+1].id==m_pConstraintInfos[i].id &&
			m_pConstraints[i+1].flags & (contact_constraint_2dof|contact_constraint_1dof|contact_constraint_3dof)) 
	{	
		realent = m_pConstraints[i+1].pent[1]->m_iSimClass<3 || m_pConstraints[i+1].pent[1]->GetType()==PE_ARTICULATED;
		GetContactFrames(m_pConstraints[i+1], frames, realent+1);
		if (!realent) {
			pe_status_pos sp;
			m_pConstraints[i+1].pent[1]->GetStatus(&sp);
			frames[1] = QuatT(sp.q,sp.pos);
		}
		for(j=0;j<2;j++) aac.qframe[j] = frames[j].q*m_pConstraintInfos[i+1].qframe_rel[j];
		if (m_pConstraints[i+1].flags & contact_constraint_3dof)
			aac.flags |= constraint_no_rotation;
		else if (m_pConstraints[i+1].flags & contact_constraint_2dof)
			aac.xlimits[0]=aac.xlimits[1] = 0;
		else
			aac.yzlimits[0]=aac.yzlimits[1] = 0;
		aac.damping = max(aac.damping, m_pConstraintInfos[i+1].damping);
		i1=2;
	}	else i1=1;
	if (i+i1<NMASKBITS && constraintMask & getmask(i+i1) && m_pConstraintInfos[i+i1].id==m_pConstraintInfos[i].id &&
			m_pConstraintInfos[i+i1].flags & constraint_limited_1axis) 
	{	for(j=0;j<2;j++) aac.xlimits[j] = m_pConstraintInfos[i+i1].limits[j];
		if (m_pConstraintInfos[i+i1].limit>0)
			aac.maxBendTorque = m_pConstraintInfos[i+i1].limit;
		i1++;
	}
	if (i+i1<NMASKBITS && constraintMask & getmask(i+i1) && m_pConstraintInfos[i+i1].id==m_pConstraintInfos[i].id &&
			m_pConstraintInfos[i+i1].flags & constraint_limited_2axes) 
	{	aac.yzlimits[0]=0; aac.yzlimits[1] = m_pConstraintInfos[i+i1].limits[1];
		if (m_pConstraintInfos[i+i1].limit>0)
			aac.maxBendTorque = m_pConstraintInfos[i+i1].limit;
		i1++;
	}
	return i1;
}


EventPhysJointBroken &CRigidEntity::ReportConstraintBreak(EventPhysJointBroken &epjb, int i)
{
	epjb.idJoint=m_pConstraintInfos[i].id; epjb.bJoint=0; MARK_UNUSED epjb.pNewEntity[0],epjb.pNewEntity[1];
	for(int iop=0;iop<2;iop++) {
		epjb.pEntity[iop] = m_pConstraints[i].pent[iop]; 
		epjb.pForeignData[iop] = m_pConstraints[i].pent[iop]->m_pForeignData;
		epjb.iForeignData[iop] = m_pConstraints[i].pent[iop]->m_iForeignData;
		epjb.partid[iop] = m_pConstraints[i].pent[iop]->m_parts[m_pConstraints[i].ipart[iop]].id;
	}
	epjb.pt = m_pConstraints[i].pt[0];
	epjb.n = m_pConstraints[i].n;
	epjb.partmat[0]=epjb.partmat[1] = -1;
	return epjb;
}


void CRigidEntity::OnNeighbourSplit(CPhysicalEntity *pentOrig, CPhysicalEntity *pentNew)
{
	PREFAST_ASSUME(pentOrig);
	if (this==pentNew)
		return;
	if (this==pentOrig && pentNew) {
		pe_params_buoyancy pb;
		// never leave exact 1.0f - means the physics took care of this, it's not default anymore
		pb.kwaterDensity = m_kwaterDensity-iszero(m_kwaterDensity-1.0f)*0.001f; 
		pentNew->SetParams(&pb);
	}
	int i,j,i1,ipart;
	masktype removeMask=0,cmask;
	EventPhysJointBroken epjb;
	pe_action_update_constraint auc;
	auc.bRemove = 1;

	if (pentNew) {
		for(i=0;i<NMASKBITS && getmask(i)<=m_constraintMask;) if (m_constraintMask & getmask(i) && !(m_pConstraintInfos[i].flags & constraint_rope)) {
			if (m_pConstraints[i].pent[0]==pentOrig && (ipart=pentNew->TouchesSphere(m_pConstraints[i].pt[0],m_pConstraintInfos[i].sensorRadius))>=0) {
				ReportConstraintBreak(epjb,i).pNewEntity[0]=pentNew; m_pWorld->OnEvent(2,&epjb);
				pe_action_add_constraint aac;
				removeMask |= getmask(i);
				i += ExtractConstraintInfo(i,m_constraintMask,aac);
				aac.partid[0] = pentNew->m_parts[ipart].id;
				pentNew->Action(&aac);
				pentNew->m_flags |= m_flags & (pef_monitor_env_changes|pef_log_env_changes);
			}	else if (m_pConstraints[i].pent[1]==pentOrig && (ipart=pentNew->TouchesSphere(m_pConstraints[i].pt[1],m_pConstraintInfos[i].sensorRadius))>=0) {
				ReportConstraintBreak(epjb,i).pNewEntity[1]=pentNew; m_pWorld->OnEvent(2,&epjb);
				for(j=i,cmask=0; j<NMASKBITS && m_constraintMask & getmask(j) && m_pConstraintInfos[j].id==m_pConstraintInfos[i].id; j++)
					cmask |= getmask(j);
				for(j=0; j<m_nColliders && m_pColliders[j]!=m_pConstraints[i].pent[1]; j++);
				m_pColliderConstraints[j] &= ~cmask; m_constraintMask &= ~cmask;
				if (!m_pColliderContacts[j] && !m_pColliderConstraints[j] && !m_pConstraints[i].pent[1]->HasContactsWith(this))
					RemoveCollider(m_pConstraints[i].pent[1]);
				m_pColliderConstraints[AddCollider(pentNew)] |= cmask; m_constraintMask |= cmask;
				for(j=i; j<NMASKBITS && m_constraintMask & getmask(j) && m_pConstraintInfos[j].id==m_pConstraintInfos[i].id; j++) {
					m_pConstraints[j].pent[1] = pentNew;
					m_pConstraints[j].ipart[1] = ipart;
				}
				i = j;
			}	else 
				for(j=m_pConstraintInfos[i].id; i<NMASKBITS && getmask(i)<=m_constraintMask && m_pConstraintInfos[i].id==j; i++);
		} else i++;
	} else {
		for(i=0;i<NMASKBITS && getmask(i)<=m_constraintMask;i++) if (m_constraintMask & getmask(i) && !(m_pConstraintInfos[i].flags & constraint_rope))
			if (m_pConstraints[i].pent[0]==pentOrig || m_pConstraints[i].pent[1]==pentOrig) {
				j = m_pConstraints[i].pent[1]==pentOrig;
				for(i1=i; i1<NMASKBITS && getmask(i1)<=m_constraintMask && m_pConstraintInfos[i1].id==m_pConstraintInfos[i].id; i1++);
				if ((ipart = pentOrig->TouchesSphere(m_pConstraints[i].pt[0],m_pConstraintInfos[i].sensorRadius))>=0) {
					for(;i<i1;i++) {
						m_pConstraints[i].ipart[j] = ipart;
						m_pConstraints[i].pbody[j] = pentOrig->GetRigidBody(ipart,iszero((int)m_pConstraintInfos[i].flags & constraint_inactive));
					}
				} else {
					ReportConstraintBreak(epjb,i).pNewEntity[j]=0; m_pWorld->OnEvent(2,&epjb);
					removeMask |= getmask(i);
				}
				i = i1-1;
			}
	}

	for(i=NMASKBITS-1;i>=0;i--) if (removeMask & getmask(i)) {
		auc.idConstraint = m_pConstraintInfos[i].id; m_pConstraints[i].pent[0]->Action(&auc);
	}

	if (pentNew)
		Awake(1,5);

	EventPhysEnvChange epec;
	epec.pEntity=this; epec.pForeignData=m_pForeignData; epec.iForeignData=m_iForeignData;
	epec.iCode = EventPhysEnvChange::EntStructureChange;
	epec.pentSrc = pentOrig;
	epec.pentNew = pentNew;
	m_pWorld->OnEvent(m_flags, &epec);
}


void CRigidEntity::DrawHelperInformation(IPhysRenderer *pRenderer, int flags)
{
#if USE_IMPROVED_RIGID_ENTITY_SYNCHRONISATION
	if (m_pWorld->m_vars.netDebugDraw && m_pNetStateHistory && m_bAwake) {
		IRenderAuxGeom* pAux = IRenderAuxGeom::GetAux();
		if (pAux) {
			{
				ReadLock lock(m_lockNetInterp);
				for (int i=0; i<m_pNetStateHistory->GetNumReceivedStates(); ++i) {
					SRigidEntityNetSerialize& receivedState = m_pNetStateHistory->GetReceivedState(i);
					Vec3 pos = receivedState.pos + Vec3(0, 0, 3);

					stack_string text;

					text.Format("seq: %d", (int)receivedState.sequenceNumber);

					IRenderAuxText::DrawText(pos, 1.5f, Vec3(0, 1, 0), eDrawText_Center | eDrawText_CenterV | eDrawText_800x600 | eDrawText_FixedSize | eDrawText_DepthTest, text.c_str());
					pAux->DrawCone(receivedState.pos + Vec3(0, 0, 2), receivedState.rot.GetColumn1(), 0.25f, 0.5f, ColorF(0,1,0,1));
				}
			}

			// Draw the smoothed position we want to move to
			{
				float interpedSequence = GetInterpSequenceNumber();
				SRigidEntityNetSerialize interpolatedPos;
				if (GetInterpolatedState(interpedSequence, interpolatedPos)) {
					Vec3 pos = interpolatedPos.pos + Vec3(0, 0, 3);

					stack_string text;

					text.Format("seq: %f", interpedSequence);

					IRenderAuxText::DrawText(pos, 1.5f, Vec3(1, 1, 0), eDrawText_Center | eDrawText_CenterV | eDrawText_800x600 | eDrawText_FixedSize | eDrawText_DepthTest, text.c_str());
					pAux->DrawCone(interpolatedPos.pos + Vec3(0, 0, 2), interpolatedPos.rot.GetColumn1(), 0.25f, 0.5f, ColorF(1,1,0,1));
				}
			}
		}
	}
#endif

	CPhysicalEntity::DrawHelperInformation(pRenderer, flags);

	if (flags & pe_helper_collisions) {
		ReadLock lock(m_lockContacts);
		for(entity_contact *pContact=m_pContactStart; pContact!=CONTACT_END(m_pContactStart); pContact=pContact->next)
			pRenderer->DrawLine(pContact->pt[0], pContact->pt[0] + pContact->n*m_pWorld->m_vars.maxContactGap*30, m_iSimClass);
	}

	if (flags & pe_helper_geometry) 
	{
		int i;

		for(i=0;i<NMASKBITS && getmask(i)<=m_constraintMask;i++) 
		{
			if (m_constraintMask & getmask(i) && !(m_pConstraintInfos[i].flags & constraint_inactive) && (m_pConstraints[i].flags & contact_angular)) 
			{
				// determine the reference frame of the constraint
				QuatT frames[2]; GetContactFrames(m_pConstraints[i], frames);
				const Vec3& posBody0 = m_pConstraints[i].pbody[0]->pos;
				const Vec3& posBody1 = m_pConstraints[i].pbody[1]->pos;									
				quaternionf qframe0 = frames[0].q * m_pConstraintInfos[i].qframe_rel[0];
				quaternionf qframe1 = frames[1].q * m_pConstraintInfos[i].qframe_rel[1];					
				Vec3 u = qframe0 * Vec3(1,0,0);
				Vec3 posFrame = m_pConstraints[i].pt[0];
				Vec3 l = posFrame - posBody0;
				Vec3 v = l - (l * u) * u;
				v.Normalize();
				Vec3 w = v ^ u;
				
				if (m_pConstraintInfos[i].flags & constraint_limited_1axis)
				{
					// draw the X axis
					pRenderer->DrawLine(posFrame, posFrame + u, m_iSimClass);
					const float xMin = m_pConstraintInfos[i].limits[0];
					const float xMax = m_pConstraintInfos[i].limits[1];
					if (xMax > xMin)
					{
						pRenderer->DrawLine(posFrame, posFrame + cos_tpl(xMin) * v + sin_tpl(xMin) * w, m_iSimClass);
						pRenderer->DrawLine(posFrame, posFrame + cos_tpl(xMax) * v + sin_tpl(xMax) * w, m_iSimClass);
						quaternionf qrel = !qframe1 * qframe0;
						Ang3 angles(qrel);
						pRenderer->DrawLine(posFrame, posFrame + cos_tpl(angles.x) * v + sin_tpl(angles.x) * w, m_iSimClass);
					}
				}
				if (m_pConstraintInfos[i].flags & constraint_limited_2axes)
				{
					const float yzMaxSine = m_pConstraintInfos[i].limits[0];
					if (yzMaxSine != 0 && fabs(yzMaxSine) != 1.0f)
					{
						float h, r;
						float tanAngle = fabs(yzMaxSine) / sqrt(1.0f - yzMaxSine * yzMaxSine);
						if (tanAngle < 1)	{
							h = 1; r = h * tanAngle;
						}	else {
							r = 1; h = r / tanAngle;
						}
						if (yzMaxSine < 0) u = -u;
						Quat dq = Quat::CreateRotationAA(gf_PI/12,u);
						Vec3 pt0 = u*h + u.GetOrthogonal().normalized()*r, pt1;
						for(int j=0; j<24; j++,pt0=pt1) {
							pt1 = dq*pt0;
							pRenderer->DrawLine(posFrame, posFrame+pt0, m_iSimClass);
							pRenderer->DrawLine(posFrame+pt0, posFrame+pt1, m_iSimClass);
						}
					}
					// draw the body 1 constraint X axis
					Vec3 u1 = qframe1 * Vec3(1,0,0);
					pRenderer->DrawLine(posFrame, posFrame + 2 * u1);
				}
			}
		}
	}
}


void CRigidEntity::GetMemoryStatistics(ICrySizer *pSizer) const
{
	if (GetType()==PE_RIGID)
		pSizer->AddObject(this, sizeof(CRigidEntity));
	CPhysicalEntity::GetMemoryStatistics(pSizer);
	pSizer->AddObject(m_pColliderContacts, m_nCollidersAlloc*sizeof(m_pColliderContacts[0]));
	pSizer->AddObject(m_pColliderConstraints, m_nCollidersAlloc*sizeof(m_pColliderConstraints[0]));
	pSizer->AddObject(m_pContactStart, m_nContacts*sizeof(m_pContactStart[0]));
	pSizer->AddObject(m_pConstraints, m_nConstraintsAlloc*sizeof(m_pConstraints[0]));
	pSizer->AddObject(m_pConstraintInfos, m_nConstraintsAlloc*sizeof(m_pConstraintInfos[0]));
	pSizer->AddObject(m_pEventsColl, m_nMaxEvents*sizeof(m_pEventsColl[0]));
}
