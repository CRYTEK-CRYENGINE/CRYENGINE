// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"

#include <CryMath/Cry_Camera.h>
#include <CryRenderer/IRenderAuxGeom.h>

#include "geometries.h"
#include "entities.h"
#include "world.h"

using namespace primitives;
using namespace cpx;
using namespace Helper;

volatile int g_lockEntList = 0;


void PhysXWorld::Release()
{
	delete this;
}

IPhysicalEntity* PhysXWorld::SetupEntityGrid(int axisz, Vec3 org, int nx, int ny, float stepx, float stepy, int log2PODscale, int bCyclic, IPhysicalEntity* pHost, const QuatT& posInHost) 
{
	PxBroadPhaseRegion rgn;
	rgn.bounds = PxBounds3(V(org-Vec3(0,0,100)), V(org+Vec3(nx*stepx,ny*stepy,200)));
	g_cryPhysX.Scene()->removeBroadPhaseRegion(m_idWorldRgn);
	m_idWorldRgn = g_cryPhysX.Scene()->addBroadPhaseRegion(rgn);
	return nullptr;
}

IPhysicalEntity *PhysXWorld::SetHeightfieldData(const heightfield *phf, int *pMatMapping, int nMats)
{
	delete m_phf;	m_phf=nullptr;
	if (phf) {
		m_phf = new PhysXEnt(PE_STATIC,-1);
		m_phf->m_pForeignData = this;
		phys_geometry *pgeom = RegisterGeometry(CreatePrimitive(GEOM_HEIGHTFIELD,phf));
		pgeom->pGeom->Release();
		pe_geomparams gp;
		m_phf->AddGeometry(pgeom, &gp);
	}
	return m_phf;
}

int PhysXWorld::GetPhysicalEntityId(IPhysicalEntity* pent) { return pent ? ((PhysXEnt*)pent)->m_id : -2; }
IPhysicalEntity* PhysXWorld::GetPhysicalEntityById(int id) 
{
	return id==-1 ? m_phf : (uint)id<m_entsById.size() && !((INT_PTR)m_entsById[id]&1) ? m_entsById[id] : nullptr;
}

int PhysXWorld::GetFreeEntId()
{
	if (m_idFree<0)
		return m_entsById.size();
	else {
		int id = m_idFree;
		m_idFree = (int)(INT_PTR)m_entsById[id]>>1;
		m_entsById[id] = nullptr;
		return id;
	}
}

IPhysicalEntity* PhysXWorld::CreatePhysicalEntity(pe_type type, float lifeTime, pe_params* params, void *pForeignData, int iForeignData, int id, IPhysicalEntity*,IGeneralMemoryHeap*)
{
	int flagsOR = 0;
	if (id<0)	{
		id = GetFreeEntId();
		flagsOR = pef_auto_id;
	} else if (id<m_entsById.size())
		if ((INT_PTR)m_entsById[id] & 1) {
			if (id==m_idFree)
				m_idFree = (int)(INT_PTR)m_entsById[m_idFree]>>1;
			else {
				int id0=id-1;
				if (id0<0 || !((INT_PTR)m_entsById[id0] & 1) || (int)(INT_PTR)m_entsById[id0]>>1!=id)
					for(id0=m_idFree; (int)(INT_PTR)m_entsById[id0]>>1!=id; id0=(int)(INT_PTR)m_entsById[id0]>>1);
				m_entsById[id0] = m_entsById[id];
			}
		} else if (m_entsById[id]) {
			int id1 = GetFreeEntId();
			m_entsById[id]->m_id = id1;
			if (id1>=m_entsById.size())
				m_entsById.resize(id1+1,0);
			m_entsById[id1] = m_entsById[id];
		}
	PhysXEnt *pent;
	switch (type) {
		case PE_PARTICLE: pent = new PhysXProjectile(id,params); break;
		case PE_LIVING: pent = new PhysXLiving(id, params); if (params) pent->SetParams(params);  break;
		case PE_ARTICULATED: pent = new PhysXArticulation(id,params); break;
		case PE_WHEELEDVEHICLE: pent = new PhysXVehicle(id,params); if (params) pent->SetParams(params); break;
		case PE_ROPE: pent = new PhysXRope(id,params); break;
		case PE_SOFT: pent = new PhysXCloth(id,params); if (params) pent->SetParams(params); break;
		default: pent = new PhysXEnt(type,id,params);
	}
	pent->m_pForeignData = pForeignData;
	pent->m_iForeignData = iForeignData;
	pent->m_flags |= flagsOR;
	if (id >= m_entsById.size()) {
		m_entsById.reserve(id+255 & ~255);
		int size = m_entsById.size();
		m_entsById.resize(id+1, 0);
		if (size<id-1) {
			for(int i=size; i<id-1; i++) m_entsById[i]=(PhysXEnt*)(INT_PTR)(i+1<<1|1);
			m_entsById[id-1] = (PhysXEnt*)(INT_PTR)(m_idFree<<1|1);
			m_idFree = size;
		}
	}
	m_entsById[id] = pent;
	return pent;
}

IPhysicalEntity *PhysXWorld::AddArea(IGeometry *pGeom, const Vec3& pos, const quaternionf &q, float scale)
{
	PhysXEnt *area = new PhysXEnt(PE_AREA,-2);
	box bbox; pGeom->GetBBox(&bbox);
	if (bbox.size*Vec3(1)>1e4)
		area->m_flags |= pef_ignore_areas;
	return area;
}

int PhysXWorld::DestroyPhysicalEntity(IPhysicalEntity* _pent, int mode, int bThreadSafe)
{
	PhysXEnt *pent = (PhysXEnt*)_pent;
	if (pent->m_id<0)
		return 1;
	WriteLock lock(m_lockEntDelete);
	if (mode)	{
		_InterlockedAnd((volatile long*)&pent->m_mask, ~(1<<5));
		_InterlockedOr((volatile long*)&pent->m_mask, (mode&1)<<5);
		pent->MarkForAdd();
		return 1;
	}
	EventPhysEntityDeleted eped;
	eped.pEntity=pent; eped.pForeignData=pent->m_pForeignData; eped.iForeignData=pent->m_iForeignData;
	eped.mode = mode;
	SendEvent(eped, 0);
	int id = GetPhysicalEntityId(pent);
	if ((uint)id < m_entsById.size()) {
		m_entsById[id] = (PhysXEnt*)(INT_PTR)(m_idFree<<1|1);
		m_idFree = id;
	}
	_InterlockedOr((volatile long*)&pent->m_mask, 8);
	InsertBefore(pent, m_delEntList[1], true);
	pent->m_pForeignData = nullptr;
	pent->m_iForeignData = -1;
	return 1;
}

int PhysXWorld::SetSurfaceParameters(int surface_idx, float bounciness, float friction, unsigned int flags) 
{
	if ((uint)surface_idx>=(uint)NSURFACETYPES)
		return 0;
	if (!m_mats[surface_idx])
		m_mats[surface_idx] = g_cryPhysX.Physics()->createMaterial(0.5f,0.3f,0.0f);
	m_mats[surface_idx]->setStaticFriction(max(0.0f,friction));
	m_mats[surface_idx]->setDynamicFriction(max(0.0f,friction)*(1/1.5f));
	m_mats[surface_idx]->setFrictionCombineMode(friction<0 ? PxCombineMode::eMIN : PxCombineMode::eAVERAGE);
	m_mats[surface_idx]->setRestitution(max(0.0f,bounciness));
	m_mats[surface_idx]->setRestitutionCombineMode(bounciness<0 ? PxCombineMode::eMIN : PxCombineMode::eAVERAGE);
	m_mats[surface_idx]->userData = (void*)(INT_PTR)(surface_idx | (flags&15)<<16);
	return 1;
}

int PhysXWorld::GetSurfaceParameters(int surface_idx, float& bounciness, float& friction, unsigned int& flags) 
{
	if ((uint)surface_idx>=(uint)NSURFACETYPES || !m_mats[surface_idx])
		return 0;
	friction = m_mats[surface_idx]->getStaticFriction();
	bounciness = m_mats[surface_idx]->getRestitution();
	flags = ((INT_PTR)m_mats[surface_idx]->userData >> 16) & 15;
	return 1;
}

struct RaycastCallback : PxRaycastCallback {
	using PxRaycastCallback::PxRaycastCallback;
	virtual PxAgain processTouches(const PxRaycastHit* buffer, PxU32 nbHits) { return false; }
};

int CopyHitData(const PxRaycastHit &src, ray_hit &dst, bool valid=true, const Vec3& rayOrg=Vec3(0), const Vec3& rayDir=Vec3(0))
{
	if (!valid) {
		dst.dist = -1;
		return 0;
	}
	PhysXEnt *pent = Ent(src.actor);
	dst.pCollider = pent->m_mask & (8|16) ? PhysXEnt::g_pPhysWorld->m_phf : pent;
	dst.bTerrain = pent==PhysXEnt::g_pPhysWorld->m_phf;
	dst.dist = src.distance;
	dst.pt = V(src.position);
	dst.n = V(src.normal);
		dst.partid = pent->RefineRayHit(rayOrg,rayDir, dst.pt,dst.n, PartId(src.shape));
	PxMaterial *pMat;
	src.shape->getMaterials(&pMat,1);
	dst.surface_idx = MatId(pMat);
	dst.iPrim = src.faceIndex;
	return 1;
}

struct RaycastFilter : PxQueryFilterCallback {
	RaycastFilter(int pierceability=15, PhysXEnt** pSkipEnts=nullptr, int nSkipEnts=0) : pierceability(pierceability),pSkipEnts(pSkipEnts), nSkipEnts(nSkipEnts) {}
	virtual PxQueryHitType::Enum preFilter(const PxFilterData& filterData, const PxShape* shape, const PxRigidActor* actor, PxHitFlags& queryFlags) {
		PxFilterData fd = shape->getQueryFilterData();
		if (!(fd.word0 & flagsAny) || (fd.word0 & flagsAll)!=flagsAll || fd.word2==skipId)
			return PxQueryHitType::eNONE;
		if (nSkipEnts && pSkipEnts[0] && pSkipEnts[0]->m_pForeignData==Ent(actor)->m_pForeignData)
			return PxQueryHitType::eNONE;
		for(int i=1;i<nSkipEnts;i++) if (pSkipEnts[i]==Ent(actor))
			return PxQueryHitType::eNONE;
		PxMaterial *pMat;
		shape->getMaterials(&pMat,1);
		return pierceability < (int)(INT_PTR)pMat->userData>>16 ? PxQueryHitType::eTOUCH : PxQueryHitType::eBLOCK;
	}
	virtual PxQueryHitType::Enum postFilter(const PxFilterData& filterData, const PxQueryHit& hit) { return PxQueryHitType::eBLOCK; }
	int pierceability;
	int flagsAny=geom_colltype_ray, flagsAll=0;
	PhysXEnt **pSkipEnts;
	int nSkipEnts;
	int skipId = -10;
};

inline PxQueryFlags objtypesFilter(int objtypes) {
	return 
		(objtypes & (ent_terrain|ent_static) ? PxQueryFlag::eSTATIC : PxQueryFlags()) | 
		(objtypes & (ent_rigid|ent_sleeping_rigid) ? PxQueryFlag::eDYNAMIC : PxQueryFlags());
}

PxQueryHitType::Enum RaycastBatchFilter(PxFilterData queryFD, PxFilterData shapeFD, const void* constantBlock, PxU32 constantBlockSize, PxHitFlags& hitFlags)
{
	if (!(queryFD.word0 & shapeFD.word0<<rwi_colltype_bit) || shapeFD.word2==queryFD.word1	|| shapeFD.word2==queryFD.word2 || shapeFD.word2==queryFD.word3)
		return PxQueryHitType::eNONE;
	return (queryFD.word0 & 15) < shapeFD.word3 ? PxQueryHitType::eTOUCH : PxQueryHitType::eBLOCK;
}

int PhysXWorld::RayWorldIntersection(const SRWIParams& rp, const char* pNameTag, int iCaller)
{
	if (!(rp.dir.len2()>0) || !rp.hits)
		return 0;
	Vec3 dirNorm = rp.dir.GetNormalized();
	PxHitFlags hitFlags(PxHitFlag::eDEFAULT|PxHitFlag::eMESH_MULTIPLE);
	PxQueryFilterData fd(objtypesFilter(rp.objtypes) | PxQueryFlag::ePREFILTER);

	if (rp.flags & rwi_queue) {
		fd.data.word0 = rp.flags;
		int *skipIds = (int*)&fd.data.word1, nSkipIds=0;
		if ((rp.objtypes & (ent_static|ent_terrain))==ent_terrain)
			skipIds[nSkipIds++] = -1;
		for(int i=0; i<rp.nSkipEnts && nSkipIds<3; i++)
			skipIds[nSkipIds++] = ((PhysXEnt*)rp.pSkipEnts)->m_id;
		for(; nSkipIds<3; skipIds[nSkipIds++]=-10);
		{ WriteLock lock(m_lockRWIqueue),lock1(m_lockRWIres);
			m_batchQuery[0]->raycast(V(rp.org), V(dirNorm), dirNorm*rp.dir, rp.nMaxHits-1, hitFlags, fd);
			EventPhysRWIResult eprr;
			eprr.pForeignData=rp.pForeignData; eprr.iForeignData=rp.iForeignData;
			eprr.OnEvent = rp.OnEvent;
			eprr.pHits = rp.hits;
			m_rwiRes[0].push_back(eprr);
		}
		m_nqRWItouches += rp.nMaxHits-1;
		return 0;
	}

	RaycastCallback hitCB(rp.nMaxHits>1 ? (PxRaycastHit*)alloca((rp.nMaxHits-1)*sizeof(PxRaycastHit)):nullptr, rp.nMaxHits-1);
	RaycastFilter hitFilter(rp.flags & 15, (PhysXEnt**)rp.pSkipEnts, rp.nSkipEnts);
	if (int collTypes = rp.flags >> rwi_colltype_bit)
		if (!(rp.flags & rwi_colltype_any)) {
			hitFilter.flagsAll = collTypes;
			hitFilter.flagsAny = -1;
		} else
			hitFilter.flagsAny = collTypes;
	if (!(rp.objtypes & ent_terrain))
		hitFilter.skipId = -1;
	ReadLockScene lock;
	g_cryPhysX.Scene()->raycast(V(rp.org), V(dirNorm), rp.dir*dirNorm, hitCB, hitFlags, fd, &hitFilter);
	int nhits = CopyHitData(hitCB.block, rp.hits[0], hitCB.hasBlock, rp.org,rp.dir);
	for(int i=0; i<hitCB.nbTouches; i++)
		nhits += CopyHitData(hitCB.touches[i], rp.hits[hitCB.hasBlock+i], true, rp.org,rp.dir);
	return nhits;
}

int PhysXWorld::TracePendingRays(int bDoActualTracing) 
{
	{ WriteLock lock(m_lockRWIqueue),lock1(m_lockRWIres);
		std::swap(m_batchQuery[0], m_batchQuery[1]);
		m_rwiRes[0].swap(m_rwiRes[1]);
		m_rwiResPx.resize(m_rwiRes[1].size());
	}

	if (m_rwiResPx.size()) {
		PxBatchQueryMemory bqm(0,0,0);
		m_rwiTouchesPx.resize(m_nqRWItouches);
		bqm.userRaycastResultBuffer = &m_rwiResPx[0];
		bqm.userRaycastTouchBuffer = &m_rwiTouchesPx[0];
		m_batchQuery[1]->setUserMemory(bqm);
		m_batchQuery[1]->execute();
	}

	{ WriteLock lock(m_lockRWIres);
		for(int i=0; i<m_rwiResPx.size(); i++) {
			EventPhysRWIResult &eprr = m_rwiRes[1][i];
			const PxRaycastQueryResult &res = m_rwiResPx[i];
			if (eprr.bHitsFromPool = !eprr.pHits) {
				m_rwiHits.resize(m_rwiHits.size()+1+res.nbTouches);
				eprr.pHits = &m_rwiHits.back()-res.nbTouches;
			}
			CopyHitData(res.block, eprr.pHits[0], res.hasBlock);
			for(int j=0; j<res.nbTouches; j++)
				CopyHitData(res.touches[j], eprr.pHits[j+1]);
		}
		m_rwiRes[1].swap(m_rwiRes[2]);
		m_rwiRes[1].clear();
	}
	return 0;
}

int PhysXWorld::RayTraceEntity(IPhysicalEntity* pient, Vec3 org, Vec3 dir, ray_hit* pHit, pe_params_pos* pp, unsigned int geomFlagsAny) 
{
	QuatT qres(IDENTITY);
	if (pp) {
		QuatT trans = ((PhysXEnt*)pient)->getGlobalPose() * QuatT(pp->q, pp->pos).GetInverted();
		org = trans*org;
		dir = trans.q*dir;
		qres = trans.GetInverted();
	}
	Vec3 dirNorm = dir.GetNormalized();	
	PhysXEnt *pent = (PhysXEnt*)pient;
	RaycastCallback hitCB(nullptr,0);
	PxQueryCache cache;
	cache.actor = pent->m_actor;
	pHit->dist = 1e10f;
	for(int i=0; i<pent->m_parts.size(); i++) {
		cache.actor = (cache.shape = pent->m_parts[i].shape)->getActor();
		if (g_cryPhysX.Scene()->raycast(V(org), V(dirNorm), dir*dirNorm, hitCB, PxHitFlags(PxHitFlag::eDEFAULT), PxQueryFilterData(PxQueryFlag::eANY_HIT), 0, &cache) && 
				hitCB.block.distance < pHit->dist) 
		{
			CopyHitData(hitCB.block, *pHit, true, org,dir);
			pHit->pt = qres*pHit->pt;
			pHit->n = qres.q*pHit->n;
		}
	}
	return pHit->dist<1e9f;
}


struct SweepCallback : PxSweepCallback {
	SweepCallback() : PxSweepCallback(0,0) {}
	virtual PxAgain processTouches(const PxSweepHit* buffer, PxU32 nbHits) { return false; }
};

struct OverlapCallback : PxOverlapCallback {
	using PxOverlapCallback::PxOverlapCallback;
	virtual PxAgain processTouches(const PxOverlapHit* buffer, PxU32 nbHits) { return false; }
};

template<class HitSrc> inline float CopyContactDataPos(const HitSrc&, geom_contact&) { return 0; }
template<> inline float CopyContactDataPos(const PxSweepHit &src, geom_contact& contact) 
{
	contact.pt = V(src.position);
	contact.n = V(src.normal);
	return src.distance;
}
template<class HitSrc> inline float CopyContactData(const HitSrc& src, geom_contact &contact) 
{
	contact.iPrim[0] = Ent(src.actor)->m_id;
	contact.iPrim[1] = PartId(src.shape);
	PxMaterial *pmtl; src.shape->getMaterials(&pmtl,1);
	contact.id[1] = pmtl ? MatId(pmtl) : 0;
	return contact.t = CopyContactDataPos(src,contact);
}

float PhysXWorld::PrimitiveWorldIntersection(const SPWIParams& pp, WriteLockCond* pLockContacts, const char* pNameTag)
{
	static geom_contact g_contacts[16];
	static volatile int g_lockContacts = 0;
	QuatT pose(IDENTITY);
	Diag33 scale(1);
	PhysXGeom geom;
	switch (geom.m_type=pp.itype) {
		case sphere::type: geom.m_geom.sph = *(sphere*)pp.pprim; break;
		case capsule::type: case cylinder::type: geom.m_geom.caps = *(capsule*)pp.pprim; break;
		case box::type: geom.m_geom.box = *(box*)pp.pprim; break;
	}
	PxQueryFilterData fd(objtypesFilter(pp.entTypes));
	RaycastFilter filter(15, (PhysXEnt**)pp.pSkipEnts, max(0,pp.nSkipEnts));
	filter.flagsAny = pp.geomFlagsAny;
	filter.flagsAll = pp.geomFlagsAll;
	if (pp.ppcontact)
		*const_cast<SPWIParams&>(pp).ppcontact = g_contacts;
	WriteLockCond lock(g_lockContacts), &lockRet = pLockContacts ? *pLockContacts : const_cast<SPWIParams&>(pp).lockContacts;
	lockRet.prw=lock.prw; lockRet.SetActive(1); lock.SetActive(0);

	float dist = 0;
	ReadLockScene locks;
	if (pp.sweepDir.len2()) {
		Vec3 dirNorm = pp.sweepDir.normalized();
		SweepCallback res;
		PxHitFlags flags(PxHitFlag::eDEFAULT | PxHitFlag::ePRECISE_SWEEP);
		if (geom.CreateAndUse(pose,scale, [&](PxGeometry &pxGeom)	{
			if (pp.nSkipEnts>=0)
				return g_cryPhysX.Scene()->sweep(pxGeom, T(pose), V(dirNorm), dirNorm*pp.sweepDir, res, flags, fd, &filter); 
			else {
				PxQueryCache cache;
				SweepCallback res1;
				res.block.distance = 1e10f;
				fd.flags = PxQueryFlags(0);
				for(int i=0; i<-pp.nSkipEnts; i++) {
					PhysXEnt *pent = (PhysXEnt*)pp.pSkipEnts[i];
					cache.actor = pent->m_actor;
					for(int j=0; j<pent->m_parts.size(); j++) {
						cache.shape = pent->m_parts[j].shape;
						if (g_cryPhysX.Scene()->sweep(pxGeom, T(pose), V(dirNorm), dirNorm*pp.sweepDir, res1, flags, fd, 0, &cache) && res1.block.distance < res.block.distance)
							res.block = res1.block; 
					}
				}
				return res.block.distance < 1e10f;
			}
		})) 
			dist = CopyContactData(res.block, g_contacts[0]);
	}	else {
		PxOverlapHit hits[16];
		OverlapCallback res(hits,16);
		if (geom.CreateAndUse(pose,scale, [&](PxGeometry &pxGeom) {
			return g_cryPhysX.Scene()->overlap(pxGeom, T(pose), res, fd, &filter); }))
		{
			for(int i=0; i<res.nbTouches; i++)
				CopyContactData(hits[i], g_contacts[i]);
			dist = res.nbTouches;
		}
	}

	if (pp.entTypes & rwi_queue) {
		EventPhysPWIResult ev;
		ev.pForeignData = pp.pForeignData; ev.iForeignData = pp.iForeignData;
		ev.OnEvent = pp.OnEvent;
		if (ev.dist = dist) {
			ev.pt = g_contacts[0].pt;
			ev.n = g_contacts[0].n;
			ev.pEntity = GetPhysicalEntityById(g_contacts[0].iPrim[0]);
			ev.partId = g_contacts[0].iPrim[1];
			ev.idxMat = g_contacts[0].id[1];
		}
		if (pp.OnEvent)
			(*pp.OnEvent)(&ev);
		else
			SendEvent(ev,1);
		return 1;
	}
	return dist;
}

int PhysXWorld::CollideEntityWithPrimitive(IPhysicalEntity* _pent, int itype, primitives::primitive* pprim, Vec3 dir, ray_hit* phit, intersection_params* pip)
{
	SPWIParams pp;
	geom_contact *cont;
	pp.itype = itype;	pp.pprim = pprim;
	pp.pSkipEnts = &_pent; pp.nSkipEnts = -1;
	pp.sweepDir = dir; pp.ppcontact = &cont;
	if (PrimitiveWorldIntersection(pp)) {
		phit->dist = cont->t;
		phit->pt = cont->pt;
		phit->n = cont->n;
		phit->ipart = ((PhysXEnt*)_pent)->idxPart(phit->partid = cont->iPrim[1]);
		return 1;
	}
	return 0;
}

//////////////////////////////////////////////////////////////////////////

struct BBoxFilter : public PxQueryFilterCallback {
	BBoxFilter(int ithread, const Vec3& vmin, const Vec3& vmax) : ithread(ithread), bbox(V(vmin),V(vmax)) {}
	virtual PxQueryHitType::Enum preFilter(const PxFilterData& filterData, const PxShape* shape, const PxRigidActor* actor, PxHitFlags& queryFlags) {
		PhysXEnt *pent = Ent(actor);
		if (!(pent->m_mask & 1<<ithread)) {
			if (!PxShapeExt::getWorldBounds(*shape,*actor,1.0f).intersects(bbox))
				return PxQueryHitType::eNONE;
			AtomicAdd(&pent->m_mask, 1<<ithread);
			if (nEnts < 64)
				entBuf[nEnts++] = Ent(actor);
			else {
				WriteLockCond lock(PhysXEnt::g_pPhysWorld->m_lockEntList, nEnts==64);
				lock.SetActive(0);
				if (PhysXEnt::g_pPhysWorld->m_entList.size()<nEnts+1)
					PhysXEnt::g_pPhysWorld->m_entList.resize(nEnts+64 & ~63);
				if (nEnts==64)
					memcpy(&PhysXEnt::g_pPhysWorld->m_entList[0], entBuf, 64*sizeof(void*));
				PhysXEnt::g_pPhysWorld->m_entList[nEnts++] = pent;
			}
		}
		return PxQueryHitType::eNONE;
	}
	virtual PxQueryHitType::Enum postFilter(const PxFilterData& filterData, const PxQueryHit& hit) { return PxQueryHitType::eNONE; }
	PxBounds3 bbox;
	int ithread = -1;
	PhysXEnt *entBuf[64];
	int nEnts = 0;
};

struct BBoxCallback : public PxOverlapCallback {
	BBoxCallback() : PxOverlapCallback(0,0) {}
	virtual PxAgain processTouches(const PxOverlapHit* buf, PxU32 nbHits) { return true; }
};

int PhysXWorld::GetEntitiesInBox(Vec3 ptmin, Vec3 ptmax, IPhysicalEntity**& pList, int objtypes, int szListPrealloc) 
{
	int ithread;
	do {
		WriteLock lock(m_lockMask);
		if (m_maskUsed+1)	{
			ithread = ilog2((m_maskUsed ^ m_maskUsed+1)+1);
			m_maskUsed |= 1u<<ithread;
		}
	} while(ithread<0);
	BBoxFilter filter(ithread,ptmin,ptmax);

	{ ReadLockScene lock;
		g_cryPhysX.Scene()->overlap(PxBoxGeometry(V(ptmax-ptmin)*0.5f), PxTransform(V(ptmax+ptmin)*0.5f,PxQuat0), BBoxCallback(), 
			PxQueryFilterData(objtypesFilter(objtypes) | PxQueryFlag::ePREFILTER | PxQueryFlag::eANY_HIT), &filter);
	}

	PhysXEnt **pListSrc = filter.nEnts>64 ? &m_entList[0] : filter.entBuf;
	if (szListPrealloc < filter.nEnts)
		if (objtypes & ent_allocate_list)
			pList = new (IPhysicalEntity*[filter.nEnts]);
		else
			pList = (IPhysicalEntity**)&m_entList[0];
	if (objtypes & ent_sort_by_mass)
		std::qsort(pListSrc, filter.nEnts, sizeof(void*), [](const void *pent0,const void *pent1)->int { 
			PxRigidBody *body0 = (*(PhysXEnt**)pent0)->m_actor->isRigidBody(), *body1 = (*(PhysXEnt**)pent1)->m_actor->isRigidBody();
			return sgn((body1 ? body1->getInvMass():0.0f) - (body0 ? body0->getInvMass():0.0f));
		});
	for(int i=0; i<filter.nEnts; i++) 
		AtomicAdd(&pListSrc[i]->m_mask, -(1<<filter.ithread));
	int n = 0;
	for(int i=0; i<filter.nEnts; i++)	if (!(pListSrc[i]->m_mask & (8|16)))
		pList[n++] = pListSrc[i];
	if (objtypes & ent_addref_results)
		for(int i=0; i<n; pListSrc[i++]->AddRef()); 
		
	WriteLock lock(m_lockMask);
	m_maskUsed &= ~(1u<<filter.ithread);
	if (filter.nEnts>64)
		AtomicAdd(&m_lockEntList, -WRITE_LOCK_VAL);
	return n;
}

void PhysXWorld::AddEventClient(int type, int(*func)(const EventPhys*), int bLogged, float priority)
{
	RemoveEventClient(type,func,bLogged);
	SEventClient ec;
	ec.OnEvent = func;
	ec.priority = priority;
	m_eventClients[type][bLogged].insert(ec);
}

int PhysXWorld::RemoveEventClient(int type, int(*func)(const EventPhys*), int bLogged) 
{ 
	int count = 0;
	auto &list = m_eventClients[type][bLogged];
	for(auto it=list.begin(); it!=list.end(); )
		if (it->OnEvent==func) {
			it = list.erase(it); count++;
		} else it++;
	return count;
}

EventPhys* PhysXWorld::AddDeferredEvent(int type, EventPhys* event)
{
	if (type!=EventPhysCollision::id)
		return nullptr;
	WriteLock lock(m_lockCollEvents);
	return &(*AllocEvent(type) = *(EventPhysCollision*)event);
}

void* PhysXWorld::GetInternalImplementation(int type, void* object)
{
	switch (type)
	{
	case 0: return reinterpret_cast<void*>( cpx::g_cryPhysX.Scene() ); break;                 // return native physx scene
	case 1: return reinterpret_cast<void*>( static_cast<PhysXEnt*>(object)->m_actor ); break; // return native physx actor
	default: break;
	}
	return nullptr;
}

void PhysXWorld::onWake(PxActor** actors, PxU32 count)
{
	for(int i=0; i<count; i++) {
		InsertBefore(Ent((PxRigidActor*)actors[i]), m_activeEntList[1]);
		_InterlockedOr((volatile long*)&Ent((PxRigidActor*)actors[i])->m_mask, 1);
	}
}

void PhysXWorld::onSleep(PxActor** actors, PxU32 count)
{
	for(int i=0; i<count; i++)
		_InterlockedOr((volatile long*)&Ent((PxRigidActor*)actors[i])->m_mask, 2);
}

void PhysXWorld::onContact(const PxContactPairHeader& pairHeader, const PxContactPair* pairs, PxU32 nbPairs)
{
	PxContactPairExtraDataIterator aux(pairHeader.extraDataStream, pairHeader.extraDataStreamSize);
	static Vec3 v0[2] = { Vec3(0),Vec3(0) };
	Vec3 *v=v0, *w=v0;
	while(aux.nextItemSet()) if (aux.preSolverVelocity) {
		v = (Vec3*)aux.preSolverVelocity->linearVelocity;
		w = (Vec3*)aux.preSolverVelocity->angularVelocity;
	}

	for(int i=0; i<nbPairs; i++) {
		EventPhysCollision &epc = *(EventPhysCollision*)AllocEvent(EventPhysCollision::id);
		PhysXEnt *pent[2];
		PxContactStreamIterator csi(pairs[i].contactPatches, pairs[i].contactPoints, pairs[i].getInternalFaceIndices(), pairs[i].patchCount, pairs[i].contactStreamSize);
		float *imp = (float*)pairs[i].contactImpulses;
		csi.nextPatch(); csi.nextContact();
		epc.pt = V(csi.getContactPoint());
		epc.n = V(csi.getContactNormal());
		epc.penetration = csi.getSeparation();
		epc.normImpulse = pairs[i].flags & PxContactPairFlag::eINTERNAL_HAS_IMPULSES ? imp[0] : 0.0f;

		for(int j=0; j<2; j++) {
			epc.pEntity[j] = (pent[j] = Ent(pairs[i].shapes[j]->getActor()));
			epc.pForeignData[j] = pent[j]->m_pForeignData;
			epc.iForeignData[j] = pent[j]->m_iForeignData;
			if (PxRigidBody *pRB = pairs[i].shapes[j]->getActor()->isRigidBody()) {
				epc.mass[j] = pRB->getMass();
				Vec3 com = T(pRB->getGlobalPose())*V(pRB->getCMassLocalPose().p);
				epc.vloc[j] = v[j] + (w[j] ^ epc.pt-com);
			}	else
				epc.mass[j] = 0;
			epc.partid[j] = (INT_PTR)pairs[i].shapes[j]->userData;
			PxMaterial *pMat;	pairs[i].shapes[j]->getMaterials(&pMat,1);
			epc.idmat[j] = MatId(pMat);
		}
		epc.idCollider = pent[1]->m_id;
		epc.radius = 0;
		epc.iPrim[0]=epc.iPrim[1] = 0;
	}
}


void PhysXWorld::UpdateProjectileState(PhysXProjectile *pent)
{
	int active = pent->m_vel.len2()>0;
	Insert(pent, m_projectilesList[active], active^1);
}


void PhysXWorld::TimeStep(float dt, int flags)
{
	//if (!m_dt && dt)
	//	g_cpx.Scene()->forceDynamicTreeRebuild(true,true);
	m_time += dt;
	if (m_vars.fixedTimestep>0 && dt>0)	{
		g_cryPhysX.dt() = dt = m_vars.fixedTimestep;
		m_dtSurplus = 0;
	}
	m_vars.lastTimeStep = m_dt = dt;
	g_cryPhysX.Scene()->setBounceThresholdVelocity(m_vars.minBounceSpeed);

	PhysXEnt *pentAdd;
	{ WriteLock lock(g_lockEntList);
		m_addEntList[0]->m_list[1] = nullptr;
		pentAdd = m_addEntList[1];
		m_addEntList[1] = m_addEntList[0] = ListStart(m_addEntList);
	}
	{ WriteLockScene lock;
		for(; pentAdd; pentAdd=pentAdd->AddToScene(true));
	}

	if (flags & ent_rigid && (!m_vars.bSingleStepMode || m_vars.bDoStep)) {
		float dtFixed = g_cryPhysX.dt();
		for(int i=0; m_dtSurplus+dt>=dtFixed && i<m_vars.nMaxSubsteps; dt-=dtFixed,i++)	{
			g_cryPhysX.Scene()->simulate(dtFixed,0,m_scratchBuf.data(),m_scratchBuf.size());
			WriteLockScene lockScene;
			{ WriteLock lock(m_lockCollEvents); 
				g_cryPhysX.Scene()->fetchResults(true);
			}
			g_cryPhysX.Scene()->flushQueryUpdates();
			AtomicAdd((volatile uint*)&m_updated, 1);
			{ ReadLock lock(g_lockEntList);
				for(PhysXEnt *pent=m_activeEntList[1]; pent!=ListStart(m_activeEntList); pent=pent->m_list[1])
					pent->PostStep(dtFixed);
				for(PhysXEnt *pent=m_auxEntList[1]; pent!=ListStart(m_auxEntList); pent=pent->m_list[1])
					pent->PostStep(dtFixed);
			}
		}
		m_dtSurplus += dt;
		m_dtSurplus = min(dtFixed, m_dtSurplus);
		m_vars.bDoStep = 0;

		{ WriteLock lock(m_lockCollEvents);
			ReadLock lock1(m_lockEntDelete);
			for(PhysXProjectile *pent=m_projectilesList[1],*pentNext; pent!=ListStart(m_projectilesList) && pent->m_vel.len2(); pent=pentNext)	{
				pentNext = (PhysXProjectile*)pent->m_list[1];
				pent->DoStep(m_dt,0);
			}
		}
	}
	if (flags & ent_deleted) { 
		WriteLockScene lock1;
		WriteLock lock2(m_lockEntDelete), lock(g_lockEntList);
		for(PhysXEnt *pent=m_delEntList[1],*pentNext; pent!=ListStart(m_delEntList); pent=pentNext) {
			pentNext = pent->m_list[1];
			if (!(pent->m_mask & 16))	{
				pent->Enable(false);
				_InterlockedOr((volatile long*)&pent->m_mask, 16);
			}
			if (pent->m_nRefCount<=0) {
				RemoveFromList(pent);
				delete pent;
			}
		}
	}

	if (PxPvdSceneClient *dbg = g_cryPhysX.Scene()->getScenePvdClient()) {
		const CCamera &cam = gEnv->pSystem->GetViewCamera();
		dbg->updateCamera("CryCamera", V(cam.GetPosition()), V(cam.GetUp()), V(cam.GetPosition()+cam.GetViewdir()));
		//if (PhysXEnt* player = (PhysXEnt*)GetPhysicalEntityById(0x7777))
		//	player->m_actor->setActorFlag(PxActorFlag::eVISUALIZATION, false);//!player->m_actor->getWorldBounds().contains(V(cam.GetPosition())));
	}
	if (m_renderer && m_vars.bMultithreaded)
		DrawPhysicsHelperInformation(m_renderer, 0);
}

EventPhys *PhysXWorld::AllocEvent(int id)
{
	if (m_nCollEvents>=m_collEvents[0].size())
		m_collEvents[0].resize(m_nCollEvents+64 & ~63);
	return &m_collEvents[0][m_nCollEvents++];
}

void PhysXWorld::PumpLoggedEvents()
{
	if (m_updated) { 
		m_updated = 0;
		PhysXEnt **pents;
		int n = 0;
		{ WriteLock lock(g_lockEntList);
			for(PhysXEnt *pent=m_activeEntList[1]; pent!=ListStart(m_activeEntList); pent=pent->m_list[1],n++);
			if (n) pents = (PhysXEnt**)alloca(n*sizeof(void*));
			for(PhysXEnt *pent=m_activeEntList[(n=0)+1],*pentNext; pent!=ListStart(m_activeEntList); pent=pentNext) {
				pentNext = pent->m_list[1];
				pents[n++] = pent;
				if ((pent->m_mask & 3)==2)
					RemoveFromList(pent);
			}
		}
			
		EventPhysStateChange epsc;
		for(int i=0; i<n; i++) {
			pents[i]->PostStep(m_dt,0);
			for(int j=0;j<2;j++) if (pents[i]->m_mask & 1<<j && pents[i]->m_flags & pef_log_state_changes) {
				epsc.pEntity=pents[i]; epsc.pForeignData=pents[i]->m_pForeignData; epsc.iForeignData=pents[i]->m_iForeignData;
				epsc.iSimClass[0] = i+1; epsc.iSimClass[1] = 2-j;
				PxBounds3 bbox = pents[i]->m_actor->getWorldBounds();
				epsc.BBoxNew[0]=epsc.BBoxOld[0] = V(bbox.minimum);
				epsc.BBoxNew[1]=epsc.BBoxOld[1] = V(bbox.maximum);
				SendEvent(epsc,1);
			}
			_InterlockedAnd((volatile long*)&pents[i]->m_mask,~3);
		}

		{ ReadLock lock(g_lockEntList);
			int n1 = 0;
			for(PhysXProjectile *pent=m_projectilesList[1]; pent!=ListStart(m_projectilesList); pent=(PhysXProjectile*)pent->m_list[1])
				n1 += pent->m_stepped && (pent->m_flags & pef_log_poststep);
			for(PhysXEnt *pent=m_auxEntList[1]; pent!=ListStart(m_auxEntList); pent=pent->m_list[1])
				n1 += pent->IsAwake() && (pent->m_flags & pef_log_poststep);
			if (n1>n) pents = (PhysXEnt**)alloca(n1*sizeof(void*));
			for(PhysXProjectile *pent=m_projectilesList[(n=0)+1]; pent!=ListStart(m_projectilesList) && n<n1; pent=(PhysXProjectile*)pent->m_list[1])	{
				if (pent->m_stepped && pent->m_flags & pef_log_poststep)
					pents[n++] = pent;
				pent->m_stepped = 0;
			}
			for(PhysXEnt *pent=m_auxEntList[1]; pent!=ListStart(m_auxEntList) && n<n1; pent=pent->m_list[1]) 
				if (pent->IsAwake() && pent->m_flags & pef_log_poststep)
					pents[n++] = pent;
		}
		for(int i=0; i<n; i++)
			pents[i]->PostStep(m_dt,0);
	}

	int nEvents = 0;
	{ WriteLock lock(m_lockCollEvents);
		m_collEvents[0].swap(m_collEvents[1]);
		std::swap(nEvents, m_nCollEvents);
	}
	for(int i=0;i<nEvents;i++) if (m_collEvents[1][i].pForeignData[0] && ((PhysXEnt*)m_collEvents[1][i].pEntity[0])->m_pForeignData && m_collEvents[1][i].pForeignData[1])
		SendEvent(m_collEvents[1][i],1);

	{ WriteLock lock(m_lockRWIres);
		for(int i=0; i<m_rwiRes[2].size(); i++)
			if (m_rwiRes[2][i].OnEvent)
				m_rwiRes[2][i].OnEvent(&m_rwiRes[2][i]);
			else
				SendEvent(m_rwiRes[2][i],1);
		m_rwiRes[2].clear();
		m_rwiHits.clear();
	}

	++m_idStep;
}

void PhysXWorld::ClearLoggedEvents()
{
	WriteLock lock(m_lockCollEvents);
	m_collEvents[0].clear();
	m_collEvents[1].clear();
}

void PhysXWorld::ResetDynamicEntities()
{
	WriteLockScene slock;
	ReadLock lock(g_lockEntList);
	pe_action_reset ar;
	for(PhysXEnt *pent=m_activeEntList[1]; pent!=ListStart(m_activeEntList); pent=pent->m_list[1])
		pent->Action(&ar);
	for(PhysXEnt *pent=m_auxEntList[1]; pent!=ListStart(m_auxEntList); pent=pent->m_list[1])
		pent->Action(&ar);
}


//////////////////////////////////////////////////////////////////////////

PhysXWorld::PhysXWorld(ILog* pLog) : m_debugDraw(false)
{
	g_cryPhysX.Init();
	memset(m_mats, 0, sizeof(m_mats));
	SetSurfaceParameters(0,0,0.5f);
	SetSurfaceParameters(NSURFACETYPES-2,0,0); // 0-friction mat
	PhysXEnt::g_pPhysWorld = this;
	m_entList.resize(64);
	m_activeEntList[0] = m_activeEntList[1] = ListStart(m_activeEntList);
	m_prevActiveEntList[0] = m_prevActiveEntList[1] = ListStart(m_prevActiveEntList);
	m_projectilesList[0] = m_projectilesList[1] = ListStart(m_projectilesList);
	m_auxEntList[0] = m_auxEntList[1] = ListStart(m_auxEntList);
	m_delEntList[0] = m_delEntList[1] = ListStart(m_delEntList);
	m_addEntList[0] = m_addEntList[1] = ListStart(m_addEntList);
	g_cryPhysX.Scene()->setSimulationEventCallback(this);
	m_pentStatic = new PhysXEnt(PE_STATIC,-2);
	PxBatchQueryDesc bqd(1000,0,0);
	bqd.preFilterShader = RaycastBatchFilter;
	m_batchQuery[0] = g_cryPhysX.Scene()->createBatchQuery(bqd);
	m_batchQuery[1] = g_cryPhysX.Scene()->createBatchQuery(bqd);
	m_scratchBuf.resize(1<<20);

	m_pbGlob.waterDensity = 1000;
	m_pbGlob.kwaterDensity = 1;
	m_pbGlob.waterDamping = 0;
	m_pbGlob.waterResistance = 1000;
	m_pbGlob.kwaterResistance = 1;
	m_pbGlob.waterFlow.zero();
	m_pbGlob.flowVariance = 0;
	m_pbGlob.waterPlane.origin.zero();
	m_pbGlob.waterPlane.n = Vec3(0,0,1);
	m_pbGlob.waterEmin = 0.01f;

	m_pLog = pLog;

	m_vars.nMaxSubsteps = 10;
	m_vars.nMaxStackSizeMC = 8;
	m_vars.maxMassRatioMC = 50.0f;
	m_vars.nMaxMCiters = 6000;
	m_vars.nMinMCiters = 4;
	m_vars.nMaxMCitersHopeless = 6000;
	m_vars.accuracyMC = 0.002f;
	m_vars.accuracyLCPCG = 0.005f;
	m_vars.nMaxContacts = 150;
	m_vars.nMaxPlaneContacts = 8;
	m_vars.nMaxPlaneContactsDistress = 4;
	m_vars.nMaxLCPCGsubiters = 120;
	m_vars.nMaxLCPCGsubitersFinal = 250;
	m_vars.nMaxLCPCGmicroiters = 12000;
	m_vars.nMaxLCPCGmicroitersFinal = 25000;
	m_vars.nMaxLCPCGiters = 5;
	m_vars.minLCPCGimprovement = 0.05f;
	m_vars.nMaxLCPCGFruitlessIters = 4;
	m_vars.accuracyLCPCGnoimprovement = 0.05f;
	m_vars.minSeparationSpeed = 0.02f;
	m_vars.maxwCG = 500.0f;
	m_vars.maxvCG = 500.0f;
	m_vars.maxvUnproj = 10.0f;
	m_vars.maxMCMassRatio = 100.0f;
	m_vars.maxMCVel = 15.0f;
	m_vars.maxLCPCGContacts = 100;
	m_vars.bFlyMode = 0;
	m_vars.iCollisionMode = 0;
	m_vars.bSingleStepMode = 0;
	m_vars.bDoStep = 0;
	m_vars.fixedTimestep = 0;
	m_vars.timeGranularity = 0.0001f;
	m_vars.maxWorldStep = 0.2f;
	m_vars.iDrawHelpers = 0;
	m_vars.iOutOfBounds = raycast_out_of_bounds | get_entities_out_of_bounds;
	#if CRY_PLATFORM_MOBILE
	m_vars.nMaxSubsteps = 2;
	#else
	m_vars.nMaxSubsteps = 5;
	#endif
	m_vars.nMaxSurfaces = NSURFACETYPES;
	m_vars.maxContactGap = 0.01f;
	m_vars.maxContactGapPlayer = 0.01f;
	m_vars.bProhibitUnprojection = 1;//2;
	m_vars.bUseDistanceContacts = 0;
	m_vars.unprojVelScale = 10.0f;
	m_vars.maxUnprojVel = 3.0f;
	m_vars.maxUnprojVelRope = 10.0f;
	m_vars.gravity.Set(0, 0, -9.8f);
	m_vars.nGroupDamping = 8;
	m_vars.groupDamping = 0.5f;
	m_vars.nMaxSubstepsLargeGroup = 5;
	m_vars.nBodiesLargeGroup = 30;
	m_vars.bEnforceContacts = 1;//1;
	m_vars.bBreakOnValidation = 0;
	m_vars.bLogActiveObjects = 0;
	m_vars.bMultiplayer = 0;
	m_vars.bProfileEntities = 0;
	m_vars.bProfileFunx = 0;
	m_vars.bProfileGroups = 0;
	m_vars.minBounceSpeed = 1;
	m_vars.nGEBMaxCells = 1024;
	m_vars.maxVel = 100.0f;
	m_vars.maxVelPlayers = 150.0f;
	m_vars.maxVelBones = 10.0f;
	m_vars.bSkipRedundantColldet = 1;
	m_vars.penaltyScale = 0.3f;
	m_vars.maxContactGapSimple = 0.03f;
	m_vars.bLimitSimpleSolverEnergy = 1;
	m_vars.nMaxEntityCells = 300000;
	m_vars.nMaxAreaCells = 128;
	m_vars.nMaxEntityContacts = 256;
	m_vars.tickBreakable = 0.1f;
	m_vars.approxCapsLen = 1.2f;
	m_vars.nMaxApproxCaps = 7;
	m_vars.bCGUnprojVel = 0;
	m_vars.bLogLatticeTension = 0;
	m_vars.nMaxLatticeIters = 100000;
	m_vars.bLogStructureChanges = 1;
	m_vars.bPlayersCanBreak = 0;
	m_vars.bMultithreaded = 0;
	m_vars.breakImpulseScale = 1.0f;
	m_vars.jointGravityStep = 1.0f;
	m_vars.jointDmgAccum = 2.0f;
	m_vars.jointDmgAccumThresh = 0.2f;
	m_vars.massLimitDebris = 1E10f;
	m_vars.maxSplashesPerObj = 0;
	m_vars.splashDist0 = 7.0f; m_vars.minSplashForce0 = 15000.0f;	m_vars.minSplashVel0 = 4.5f;
	m_vars.splashDist1 = 30.0f; m_vars.minSplashForce1 = 150000.0f; m_vars.minSplashVel1 = 10.0f;
	m_vars.lastTimeStep = 0;
	m_vars.numThreads = 2;
	m_vars.physCPU = 4;
	m_vars.physWorkerCPU = 1;
	m_vars.helperOffset.zero();
	m_vars.timeScalePlayers = 1.0f;
	MARK_UNUSED m_vars.flagsColliderDebris;
	m_vars.flagsANDDebris = -1;
	m_vars.bDebugExplosions = 0;
	m_vars.ticksPerSecond = 3000000000U;
	#if USE_IMPROVED_RIGID_ENTITY_SYNCHRONISATION
	m_vars.netInterpTime = 0.1f;
	m_vars.netExtrapMaxTime = 0.5f;
	m_vars.netSequenceFrequency = 256;
	m_vars.netDebugDraw = 0;
	#else
	m_vars.netMinSnapDist = 0.1f;
	m_vars.netVelSnapMul = 0.1f;
	m_vars.netMinSnapDot = 0.99f;
	m_vars.netAngSnapMul = 0.01f;
	m_vars.netSmoothTime = 5.0f;
	#endif
	m_vars.bEntGridUseOBB = 1;
	m_vars.nStartupOverloadChecks = 20;
	m_vars.maxRopeColliderSize =
	#if CRY_PLATFORM_MOBILE
	100;
	#else
	0;
	#endif
	m_vars.breakageMinAxisInertia = 0.01f;
}

IPhysicalEntity *PhysXWorld::AddGlobalArea()
{
	static PhysXEnt g_dummyArea(PE_AREA,-2);
	g_dummyArea.m_flags |= pef_ignore_areas;
	return &g_dummyArea;
}

int PhysXWorld::CheckAreas(const Vec3& ptc, Vec3& gravity, pe_params_buoyancy* pb, int nMaxBuoys, int iMedium, const Vec3& vec, IPhysicalEntity* pent, int iCaller) 
{
	pb[0] = m_pbGlob;
	gravity = m_vars.gravity;
	return 1;
}

IPhysicalEntity* PhysXWorld::GetNextArea(IPhysicalEntity* pPrevArea)
{
	return pPrevArea ? nullptr : AddGlobalArea();
}

IPhysicalEntity *PhysXWorld::CreatePhysicalPlaceholder(pe_type type, pe_params* params, void *pForeignData, int iForeignData, int id)
{
	return m_pentStatic;
}

void PhysXWorld::SimulateExplosion(pe_explosion* pexpl, IPhysicalEntity** pSkipEnts, int nSkipEnts, int iTypes, int iCaller) 
{
	pexpl->nAffectedEnts = 0;
}

void PhysXWorld::DrawPhysicsHelperInformation(IPhysRenderer *pRenderer, int iCaller)
{
	m_renderer = pRenderer;
	if (m_vars.bMultithreaded && iCaller)
		return;
#ifndef _RELEASE
	if (!m_vars.iDrawHelpers && (m_debugDraw==false)) return;

	// ensure exuberant physics access
	WriteLockScene lockSceneRAII;
	assert(iCaller <= MAX_PHYS_THREADS);

	// state configuration and call
	if (!m_vars.iDrawHelpers) { m_debugDraw = false; g_cryPhysX.SetDebugVisualizationForAllSceneElements(false); return; }
	if (m_vars.iDrawHelpers && (m_debugDraw == false)) { m_debugDraw = true; g_cryPhysX.SetDebugVisualizationForAllSceneElements(true); }

	g_cryPhysX.Scene()->setVisualizationParameter(PxVisualizationParameter::eSCALE, m_vars.iDrawHelpers ? 1.0f : 0.0f);

	g_cryPhysX.Scene()->setVisualizationParameter(PxVisualizationParameter::eCONTACT_POINT, m_vars.iDrawHelpers & (1 << 0) ? 1.0f : 0.0f);
	g_cryPhysX.Scene()->setVisualizationParameter(PxVisualizationParameter::eCONTACT_NORMAL, m_vars.iDrawHelpers & (1 << 0) ? 1.0f : 0.0f);
	g_cryPhysX.Scene()->setVisualizationParameter(PxVisualizationParameter::eCONTACT_FORCE, 0.0f);

	g_cryPhysX.Scene()->setVisualizationParameter(PxVisualizationParameter::eCOLLISION_SHAPES, m_vars.iDrawHelpers & (1 << 1) ? 1.0f : 0.0f);
	g_cryPhysX.Scene()->setVisualizationParameter(PxVisualizationParameter::eBODY_LIN_VELOCITY, 0.0f);
	g_cryPhysX.Scene()->setVisualizationParameter(PxVisualizationParameter::eBODY_ANG_VELOCITY, 0.0f);
	g_cryPhysX.Scene()->setVisualizationParameter(PxVisualizationParameter::eACTOR_AXES, 0.0f);
	g_cryPhysX.Scene()->setVisualizationParameter(PxVisualizationParameter::eCOLLISION_AABBS, m_vars.iDrawHelpers & (1 << 2) ? 2.0f : 0.0f);
	g_cryPhysX.Scene()->setVisualizationParameter(PxVisualizationParameter::eJOINT_LOCAL_FRAMES, 0.0f);
	g_cryPhysX.Scene()->setVisualizationParameter(PxVisualizationParameter::eJOINT_LIMITS, 0.0f);

	float dist = 50.0f;
	if (ICVar *CVdist = gEnv->pConsole->GetCVar("p_cull_distance"))
		dist = CVdist->GetFVal();
	Vec3 campos = gEnv->pSystem->GetViewCamera().GetPosition();
	physx::PxRenderBuffer const& rb = g_cryPhysX.Scene()->getRenderBuffer();

	IRenderAuxGeom* renderer = gEnv->pAuxGeomRenderer;

	const float lineThickness = 2.0f;
	const int pointSize = 10;

	SDrawTextInfo tInfo;
	for (PxU32 i = 0; i < rb.getNbTexts(); i++)
	{
		const PxDebugText& t = rb.getTexts()[i];
		renderer->RenderText(V(t.position), tInfo, t.string, 0);
	}

	for (PxU32 i = 0; i < rb.getNbPoints(); i++)
	{
		const PxDebugPoint& p = rb.getPoints()[i];
		uint32 col0 = (255 << 24) | p.color;
		if ((campos-V(p.pos)).len2()<sqr(dist))
			renderer->DrawPoint(V(p.pos), col0, pointSize);
	}

	int nLines = rb.getNbLines(), j;
	if (nLines > 0)
	{
		m_debugDrawLines.resize(nLines * 2);
		m_debugDrawLinesColors.resize(nLines * 2);
		for (PxU32 i = j = 0; i < nLines; i++)
		{
			const PxDebugLine& line = rb.getLines()[i];
			if (min((campos-V(line.pos0)).len2(),(campos-V(line.pos1)).len2()) < sqr(dist)) 
			{
				int idx = j++ * 2;
				m_debugDrawLines[idx + 0] = V(line.pos0);
				m_debugDrawLines[idx + 1] = V(line.pos1);

				m_debugDrawLinesColors[idx + 0] = ColorB((255 << 24) | line.color0); // set alpha to 1.0
				m_debugDrawLinesColors[idx + 1] = ColorB((255 << 24) | line.color1); // set alpha to 1.0
			}
		}
		renderer->DrawLines(m_debugDrawLines.data(), j * 2, m_debugDrawLinesColors.data());
	}

	int nTriangles = rb.getNbTriangles();
	if (nTriangles > 0)
	{
		m_debugDrawTriangles.resize(nTriangles * 3);
		m_debugDrawTrianglesColors.resize(nTriangles * 3);
		for (PxU32 i = j = 0; i < nTriangles; i++)
		{
			const PxDebugTriangle& tri = rb.getTriangles()[i];
			if (min(min((campos-V(tri.pos0)).len2(),(campos-V(tri.pos1)).len2()),(campos-V(tri.pos2)).len2()) < sqr(dist))
			{
				int idx = j++ * 3;
				m_debugDrawTriangles[idx + 0] = V(tri.pos0);
				m_debugDrawTriangles[idx + 1] = V(tri.pos1);
				m_debugDrawTriangles[idx + 2] = V(tri.pos2);

				m_debugDrawTrianglesColors[idx + 0] = ColorB(tri.color0);
				m_debugDrawTrianglesColors[idx + 1] = ColorB(tri.color1);
				m_debugDrawTrianglesColors[idx + 2] = ColorB(tri.color2);
			}
		}
		renderer->DrawTriangles(m_debugDrawTriangles.data(), j * 3, m_debugDrawTrianglesColors.data());
	}
#endif
}

void PhysXWorld::DrawEntityHelperInformation(IPhysRenderer *pRenderer, int entityId, int iDrawHelpers)
{
	
}
