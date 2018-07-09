// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"

#include "bvtree.h"
#include "geometry.h"
#include "trimesh.h"
#include "rigidbody.h"
#include "physicalplaceholder.h"
#include "physicalentity.h"
#include "geoman.h"
#include "physicalworld.h"
#include "tetrlattice.h"
#include "raybv.h"
#include "raygeom.h"
#include "singleboxtree.h"
#include "boxgeom.h"
#include "spheregeom.h"
#include "cylindergeom.h"
#include "capsulegeom.h"
#include "rigidentity.h"
#include "softentity.h"

RigidBody g_StaticRigidBodies[MAX_PHYS_THREADS];
CPhysicalEntity g_StaticPhysicalEntity(0);
geom CPhysicalEntity::m_defpart;

SPartHelper *CPhysicalEntity::g_parts = 0;
SStructuralJointHelper *CPhysicalEntity::g_joints = 0;
SStructuralJointDebugHelper *CPhysicalEntity::g_jointsDbg = 0;
int *CPhysicalEntity::g_jointidx = 0;
int CPhysicalEntity::g_nPartsAlloc=0, CPhysicalEntity::g_nJointsAlloc=0;

void CPhysicalEntity::CleanupGlobalState()
{
	if (g_parts)
		delete[] (g_parts-1);
	g_parts = NULL;

	delete[] g_joints;
	g_joints = NULL;

	delete[] g_jointidx;
	g_jointidx = NULL;

	delete[] g_jointsDbg;
	g_jointsDbg = NULL;

	g_nJointsAlloc = 0;
	g_nPartsAlloc = 0;
}

CPhysicalEntity::CPhysicalEntity(CPhysicalWorld *pworld, IGeneralMemoryHeap* pHeap)
	: m_iDeletionTime(0)
	, m_nRefCount(0)
	, m_flags(pef_pushable_by_players | pef_traceable | pef_never_affect_triggers | pef_never_break)
	, m_next(nullptr)
	, m_prev(nullptr)
	, m_pWorld(pworld)
	, m_pHeap(pHeap)
	, m_nRefCountPOD(0)
	, m_iLastPODUpdate(0)
	, m_iPrevSimClass(-1)
	, m_bMoved(0)
	, m_bPermanent(1)
	, m_bPrevPermanent(0)
	, m_iGroup(-1)
	, m_next_coll(nullptr)
	, m_next_coll1(nullptr)
	, m_next_coll2(nullptr)
	, m_pos(ZERO)
	, m_qrot(IDENTITY)
	, m_pColliders(nullptr)
	, m_nColliders(0)
	, m_nCollidersAlloc(0)
	, m_lockColliders(0)
	, m_pOuterEntity(nullptr)
	, m_bProcessed_aux(0)
	, m_timeIdle(0.0f)
	, m_maxTimeIdle(0.0f)
	, m_timeStructUpdate(0.0f)
	, m_updStage(1)
	, m_nUpdTicks(0)
	, m_pExpl(nullptr)
	, m_nParts(0)
	, m_nPartsAlloc(0)
	, m_iLastIdx(0)
	, m_lockPartIdx(0)
	, m_ground(nullptr)
	, m_nGroundPlanes(0)
	, m_pUsedParts(nullptr)
	, m_nUsedParts(0)
	, m_pStructure(nullptr)
{ 
	//CPhysicalPlaceholder
	static_assert(CRY_ARRAY_COUNT(m_BBox) == 2, "Invalid array size!");
	m_BBox[0].zero();
	m_BBox[1].zero(); 
	m_iSimClass = 0; 
	m_iGThunk0 = 0;
	static_assert(CRY_ARRAY_COUNT(m_ig) == 2, "Invalid array size!");
	m_ig[0].x=m_ig[1].x=m_ig[0].y=m_ig[1].y = GRID_REG_PENDING;
	m_bProcessed = 0;
	m_id = -1;
	m_pForeignData = nullptr;
	m_iForeignData = m_iForeignFlags = 0;
	m_pEntBuddy = nullptr;
	m_lockUpdate = 0;

	//CPhysicalEntity
	m_parts = &m_defpart;
	m_parts[0].pNewCoords = (coord_block_BBox*)&m_parts[0].pos;
	m_pNewCoords = (coord_block*)&m_pos;

	m_collisionClass.type = m_collisionClass.ignore = 0;
}

CPhysicalEntity::~CPhysicalEntity()
{	
	if( m_pUsedParts )
	{
		if (m_pHeap)
			m_pHeap->Free(m_pUsedParts);
		else
			delete[] m_pUsedParts;
	}
	for(int i=0;i<m_nParts;i++) if (m_parts[i].pPhysGeom) {
		if (m_parts[i].pMatMapping && m_parts[i].pMatMapping!=m_parts[i].pPhysGeom->pMatMapping) 
			delete[] m_parts[i].pMatMapping;
		m_pWorld->GetGeomManager()->UnregisterGeometry(m_parts[i].pPhysGeom);
		m_pWorld->GetGeomManager()->UnregisterGeometry(m_parts[i].pPhysGeomProxy);
		if (m_parts[i].flags & geom_can_modify && m_parts[i].pLattice)
			m_parts[i].pLattice->Release();
		if (m_parts[i].pPlaceholder)
			m_pWorld->DestroyPhysicalEntity(ReleasePartPlaceholder(i),0,1);
	}
	if (m_parts!=&m_defpart)
	{
		if (m_pHeap)
			m_pHeap->Free(m_parts);
		else
			m_pWorld->FreeEntityParts(m_parts,m_nPartsAlloc);
	}
	if (m_pColliders) delete[] m_pColliders;
	if (m_ground) delete[] m_ground;
	if (m_pExpl) delete[] m_pExpl;
	if (m_pStructure) {
		delete[] m_pStructure->pJoints;
		delete[] m_pStructure->pParts;
		if (m_pStructure->defparts) {
			for(int j=0;j<m_nParts;j++) if (m_pStructure->defparts[j].pSkinInfo) {
				delete[] m_pStructure->defparts[j].pSkinInfo;
				m_pWorld->DestroyPhysicalEntity(m_pStructure->defparts[j].pSkelEnt,0,1);
			}
			delete[] m_pStructure->defparts;
		}
		if (m_pStructure->Pexpl) delete[] m_pStructure->Pexpl;
		if (m_pStructure->Lexpl) delete[] m_pStructure->Lexpl;
		delete m_pStructure; m_pStructure = 0;
	}
	if(m_pOuterEntity && !m_pWorld->m_bMassDestruction)m_pOuterEntity->Release();
	if(m_pWorld && !m_pWorld->m_bMassDestruction)assert(!m_nRefCount && !m_nRefCountPOD);
}

void CPhysicalEntity::Delete() {
	IGeneralMemoryHeap* pHeap = m_pHeap;

	this->~CPhysicalEntity();

	if (pHeap)
		pHeap->Free(this);
	else
		CryModuleFree(this);
}

int CPhysicalEntity::AddRef() { 
	if (IsPODThread(m_pWorld) && m_pWorld->m_lockPODGrid) {
		if (m_iLastPODUpdate==m_pWorld->m_iLastPODUpdate)
			return m_nRefCount;
		m_iLastPODUpdate = m_pWorld->m_iLastPODUpdate;
		++m_nRefCountPOD;
	}
	AtomicAdd(&m_nRefCount, 1);
	return m_nRefCount; 
}

int CPhysicalEntity::Release() { 
	if (IsPODThread(m_pWorld) && m_pWorld->m_lockPODGrid) {
		if (m_iLastPODUpdate==m_pWorld->m_iLastPODUpdate)
			return m_nRefCount;
		m_iLastPODUpdate = m_pWorld->m_iLastPODUpdate;
		if (m_nRefCountPOD<=0)
			return m_nRefCount;
		--m_nRefCountPOD;
	}
	AtomicAdd(&m_nRefCount, -1);
	return m_nRefCount; 
}

void CPhysicalEntity::GetLocTransform(int ipart, Vec3 &offs, quaternionf &q, float &scale, const CPhysicalPlaceholder *trg) const
{
	if ((unsigned int)ipart<(unsigned int)m_nParts) {
		q = m_qrot*m_parts[ipart].q;
		offs = m_qrot*m_parts[ipart].pos + m_pos;
		scale = m_parts[ipart].scale;
	} else {
		q.SetIdentity(); offs.zero(); scale=1.0f;
	}
	m_pWorld->TransformToGrid(this,trg, offs,q);
}

void CPhysicalEntity::ComputeBBox(Vec3 *BBox, int flags)
{
	const int numParts = m_nParts+(flags & part_added);
	if (numParts) {
		IGeometry *pGeom[3];
		int i,j;
		const float fMax = 1.0E15f;
		const float fMin = -1.0E15f;
		Vec3 BBoxMin(fMax), BBoxMax(fMin);
		const Quat& qNewCoords = m_pNewCoords->q;
		const Vec3& vNewCoords = m_pNewCoords->pos;

		if (m_iSimClass-4 | m_nParts-10>>31) {
			for(i=0; i<numParts ; i++) {				
				int inext = min(i+1, numParts-1);
				PrefetchLine(m_parts[inext].pPhysGeom, 0);
				PrefetchLine(m_parts[inext].pPhysGeomProxy, 0);
				PrefetchLine(m_parts[inext].pNewCoords, 0);
				pGeom[0] = m_parts[i].pPhysGeomProxy->pGeom;
				pGeom[1]=pGeom[2] = m_parts[i].pPhysGeom->pGeom; 
				const Matrix33 RT = Matrix33(qNewCoords*m_parts[i].pNewCoords->q).T();
				Vec3 partBBoxMin(fMax),partBBoxMax(fMin);
				j=0; do {
					box abox;
					pGeom[j]->GetBBox(&abox);				
					const Matrix33 boxMat = (abox.Basis * RT).Fabs();	//Put in local var to avoid writes to memory caused by use of abox as an out-reference in the call to GetBBox()
					Vec3 sz = (abox.size*boxMat)*m_parts[i].pNewCoords->scale;
					Vec3 pos = vNewCoords + qNewCoords*(m_parts[i].pNewCoords->pos + 
						m_parts[i].pNewCoords->q*abox.center*m_parts[i].pNewCoords->scale);
					Vec3 boxMin = pos-sz;
					Vec3 boxMax = pos+sz;
					partBBoxMin = min_safe(partBBoxMin, boxMin);
					partBBoxMax = max_safe(partBBoxMax, boxMax);				
					j++;
				} while(pGeom[j]!=pGeom[j-1]);

				if (!(m_parts[i].flags & geom_ignore_BBox)) {
					BBoxMin = min_safe(BBoxMin, partBBoxMin);
					BBoxMax = max_safe(BBoxMax, partBBoxMax);
				}

				if(flags & update_part_bboxes) {
					m_parts[i].pNewCoords->BBox[0] = partBBoxMin;
					m_parts[i].pNewCoords->BBox[1] = partBBoxMax;
				}
			}
		} else { // for simclass-4 ents with 10+ parts (typically character skeletons) use a simplified bounding sphere-based calculation
			for(i=0; i<numParts ; i++) {
				int inext = min(i+1, numParts-1);
				PrefetchLine(m_parts[inext].pPhysGeom, 0);
				PrefetchLine(m_parts[inext].pNewCoords, 0);
				sphere sph;
				switch((pGeom[0] = m_parts[i].pPhysGeom->pGeom)->GetType()) {
					case GEOM_SPHERE: {
						CSphereGeom *pSph = (CSphereGeom*)pGeom[0]; sph.center=pSph->m_sphere.center; sph.r=pSph->m_sphere.r; 
					} break;
					case GEOM_CAPSULE: { 
						CCapsuleGeom *pCaps = (CCapsuleGeom*)pGeom[0]; sph.center=pCaps->m_cyl.center; sph.r=pCaps->m_cyl.hh+pCaps->m_cyl.r; 
					} break;
					default: { 
						box abox; pGeom[0]->GetBBox(&abox); sph.center=abox.center; sph.r=max(max(abox.size.x,abox.size.y),abox.size.z); 
					} break;
				}
				PrefetchLine(m_parts[inext].pPhysGeom->pGeom, 0);
				sph.center = qNewCoords*(m_parts[i].pNewCoords->q*sph.center + m_parts[i].pNewCoords->pos) + vNewCoords;
				sph.r *= m_parts[i].pNewCoords->scale;
				Vec3 partBBox[2] = { sph.center-Vec3(sph.r), sph.center+Vec3(sph.r) };

				BBoxMin = min_safe(BBoxMin, partBBox[0]);
				BBoxMax = max_safe(BBoxMax, partBBox[1]);

				if(flags & update_part_bboxes) {
					m_parts[i].pNewCoords->BBox[0] = partBBox[0];
					m_parts[i].pNewCoords->BBox[1] = partBBox[1];
				}
			}
		}

		BBox[0] = BBoxMin;
		BBox[1] = BBoxMax;
	}	else
		BBox[0]=BBox[1] = m_pNewCoords->pos;

	if (m_pEntBuddy && BBox==m_BBox)
		m_pEntBuddy->m_BBox[0]=BBox[0], m_pEntBuddy->m_BBox[1]=BBox[1];
}

int CPhysicalEntity::SetParams(pe_params *_params, int bThreadSafe)
{
	if (_params->type==pe_params_part::type_id) {
		pe_params_part *params = (pe_params_part*)_params;
		if (!is_unused(params->pPhysGeom) && params->pPhysGeom && params->pPhysGeom->pGeom)
			m_pWorld->GetGeomManager()->AddRefGeometry(params->pPhysGeom);
		if (!is_unused(params->pPhysGeomProxy) && params->pPhysGeomProxy && params->pPhysGeomProxy->pGeom)
			m_pWorld->GetGeomManager()->AddRefGeometry(params->pPhysGeomProxy);
	}
	int bRecalcBounds = 0;
	ChangeRequest<pe_params> req(this,m_pWorld,_params,bThreadSafe);
	if (req.IsQueued() && !bRecalcBounds)
		return 1+(m_bProcessed>>PENT_QUEUED_BIT);

	if (_params->type==pe_params_pos::type_id) {
		pe_params_pos *params = (pe_params_pos*)_params;
		int i,j,bPosChanged=0,bBBoxReady=0;
		SEntityGrid *pgridCur = m_pWorld->GetGrid(this), *pgridNew = is_unused(params->pGridRefEnt) ? pgridCur : 
			(!params->pGridRefEnt || params->pGridRefEnt==WORLD_ENTITY ? &m_pWorld->m_entgrid : m_pWorld->GetGrid(params->pGridRefEnt));
		if (pgridNew != pgridCur) {
			{ WriteLock lockG(m_pWorld->m_lockGrid);
				m_pWorld->DetachEntityGridThunks(this);
			}
			QuatT transNew = pgridNew->m_transW.GetInverted() * pgridCur->m_transW * QuatT(m_pNewCoords->q,m_pNewCoords->pos);
			m_pWorld->SetGrid(this, pgridNew);
			params->pos = transNew.t; params->q = transNew.q;
			int nparts = GetType()==PE_ARTICULATED ? m_nParts : 1;
			pe_status_dynamics sd;
			pe_action_set_velocity asv; 
			for(asv.ipart=sd.ipart=0; asv.ipart<nparts; asv.ipart++,sd.ipart++) {
				GetStatus(&sd);
				asv.v = !pgridNew->m_transW.q*(pgridCur->m_transW.q*sd.v + pgridCur->m_velW - pgridNew->m_velW);
				asv.w = !pgridNew->m_transW.q*(pgridCur->m_transW.q*sd.w);
				Action(&asv);
			}
		}

		Vec3 scale;
		if ((scale=get_xqs_from_matrices(params->pMtx3x4,params->pMtx3x3, params->pos,params->q,params->scale)).len2()>3.03f) {
			WriteLock lock(m_lockUpdate);
			for(i=0;i<m_nParts;i++) {
				phys_geometry *pgeom;
				if (m_parts[i].pPhysGeom!=m_parts[i].pPhysGeomProxy) {
					if (!(pgeom = (phys_geometry*)m_parts[i].pPhysGeomProxy->pGeom->GetForeignData(DATA_UNSCALED_GEOM)))
						pgeom = m_parts[i].pPhysGeomProxy;
					if (BakeScaleIntoGeometry(pgeom,m_pWorld->GetGeomManager(),scale)) {
						m_pWorld->GetGeomManager()->UnregisterGeometry(m_parts[i].pPhysGeomProxy);
						(m_parts[i].pPhysGeomProxy=pgeom)->nRefCount++;
					}
				}
				bool doBake = true;
				if (!(pgeom = (phys_geometry*)m_parts[i].pPhysGeom->pGeom->GetForeignData(DATA_UNSCALED_GEOM)))
					doBake = (pgeom = m_parts[i].pPhysGeom)->nRefCount>1;	// can't store DATA_UNSCALED_GEOM ptr to a volatile geom
				if (doBake && BakeScaleIntoGeometry(pgeom,m_pWorld->GetGeomManager(),scale)) {
					if (m_parts[i].pPhysGeom==m_parts[i].pPhysGeomProxy) {
						m_pWorld->GetGeomManager()->UnregisterGeometry(m_parts[i].pPhysGeomProxy);
						(m_parts[i].pPhysGeomProxy = pgeom)->nRefCount++;
					}
					if (m_parts[i].pMatMapping==m_parts[i].pPhysGeom->pMatMapping)
						m_parts[i].pMatMapping = pgeom->pMatMapping;
					m_pWorld->GetGeomManager()->UnregisterGeometry(m_parts[i].pPhysGeom);
					(m_parts[i].pPhysGeom=pgeom)->nRefCount++;
				}
			}
		}
		ENTITY_VALIDATE("CPhysicalEntity:SetParams(pe_params_pos)",params);
		Vec3 BBox[2];
		coord_block cnew,*pcPrev;
		cnew.pos = m_pos; cnew.q = m_qrot;
		if (!is_unused(params->pos)) { cnew.pos = params->pos; bPosChanged=1; }
		if (!is_unused(params->q)) { cnew.q = params->q; bPosChanged=1; }
		if (!is_unused(params->iSimClass) && m_iSimClass>=0 && m_iSimClass<7) {
			m_iSimClass = params->iSimClass;
			m_pWorld->RepositionEntity(this,2);
		}

		if (!is_unused(params->scale)) {
			if (params->bRecalcBounds) {
				IGeometry *pGeom[3];
				Matrix33 R;
				box abox;
				BBox[0]=Vec3(VMAX); BBox[1]=Vec3(VMIN);
				if (m_nParts==0)
					BBox[0]=BBox[1] = cnew.pos; 
				else for(i=0;i<m_nParts;i++) {
					pGeom[0] = m_parts[i].pPhysGeomProxy->pGeom;
					pGeom[1]=pGeom[2] = m_parts[i].pPhysGeom->pGeom; 
					j=0; do {
						pGeom[j]->GetBBox(&abox);
						R = Matrix33(cnew.q*m_parts[i].q);
						abox.Basis *= R.T();
						Vec3 sz = (abox.size*abox.Basis.Fabs())*params->scale;
						Vec3 pos = cnew.pos+cnew.q*(m_parts[i].pos*(params->scale/m_parts[i].scale)+m_parts[i].q*abox.center*params->scale);
						BBox[0] = min_safe(BBox[0], pos-sz);
						BBox[1] = max_safe(BBox[1], pos+sz);
						j++;
					} while(pGeom[j]!=pGeom[j-1]);
				}
				bBBoxReady = 1;
			}
			bPosChanged = 1; 
		}

		if (bPosChanged) {
			if (params->bRecalcBounds) {
				CPhysicalEntity **pentlist;
				// make triggers aware of the object's movements
				if (!(m_flags & pef_never_affect_triggers))
					m_pWorld->GetEntitiesAround(m_BBox[0],m_BBox[1],pentlist,ent_triggers,this);
				pcPrev=m_pNewCoords; m_pNewCoords=&cnew;
				if (!bBBoxReady)
					ComputeBBox(BBox,0);
				if (!params->bEntGridUseOBB || m_pStructure)
					bPosChanged = m_pWorld->RepositionEntity(this,1 | (params->bRecalcBounds&128)>>3,BBox);
				m_pNewCoords = pcPrev;
			}

			{ WriteLock lock(m_lockUpdate);
				m_pos=m_pNewCoords->pos = cnew.pos; m_qrot=m_pNewCoords->q = cnew.q;
				if (!is_unused(params->scale)) for(i=0;i<m_nParts;i++) {
					float dscale = params->scale/m_parts[i].scale;
					m_parts[i].mass *= cube(dscale);
					m_parts[i].pNewCoords->pos = (m_parts[i].pos *= dscale);
					m_parts[i].pNewCoords->scale = (m_parts[i].scale = params->scale);
				}
				if (params->bRecalcBounds) {
					ComputeBBox(BBox);
					if (params->bEntGridUseOBB && !m_pStructure)
						bPosChanged = m_pWorld->RepositionEntity(this,5,BBox);
					m_BBox[0] = BBox[0];
					m_BBox[1] = BBox[1];
					m_pWorld->UnlockGrid(this,-bPosChanged);
				}
			}
			if (params->bRecalcBounds)
				RepositionParts();
			if (params->bRecalcBounds && !(m_flags & pef_never_affect_triggers)) {
				CPhysicalEntity **pentlist;
				m_pWorld->GetEntitiesAround(m_BBox[0],m_BBox[1],pentlist,ent_triggers,this); 
			}
		}

		return 1;
	} 

	if (_params->type==pe_params_bbox::type_id) {
		pe_params_bbox *params = (pe_params_bbox*)_params;
		ENTITY_VALIDATE("CPhysicalEntity::SetParams(pe_params_bbox)",params);
		Vec3 BBox[2] = { m_BBox[0],m_BBox[1] };
		if (!is_unused(params->BBox[0])) BBox[0] = params->BBox[0];
		if (!is_unused(params->BBox[1])) BBox[1] = params->BBox[1];
		int bPosChanged = m_pWorld->RepositionEntity(this,1,BBox);
		{ WriteLock lock(m_lockUpdate); 
			WriteBBox(BBox);
			m_pWorld->UnlockGrid(this,-bPosChanged);
		}
		return 1;
	}
	
	if (_params->type==pe_params_part::type_id) {
		pe_params_part *params = (pe_params_part*)_params;
		int i = params->ipart;
		if (is_unused(params->ipart) && is_unused(params->partid)) {
			for(i=0;i<m_nParts;i++) if (is_unused(params->flagsCond) || m_parts[i].flags & params->flagsCond) {
				m_parts[i].flags = m_parts[i].flags & params->flagsAND | params->flagsOR;
				m_parts[i].flagsCollider = m_parts[i].flagsCollider & params->flagsColliderAND | params->flagsColliderOR;
				if (!is_unused(params->mass)) m_parts[i].mass = params->mass;
				if (!is_unused(params->idmatBreakable)) m_parts[i].idmatBreakable = -1;
				if (!is_unused(params->pMatMapping)) {
					int *pMapping0 = m_parts[i].pMatMapping;
					if ((!pMapping0 || params->nMats!=m_parts[i].nMats || pMapping0==m_parts[i].pPhysGeom->pMatMapping) && params->pMatMapping)
						m_parts[i].pMatMapping = new int[m_parts[i].nMats = params->nMats]; 
					else pMapping0 = 0;
					if (params->pMatMapping && params->nMats>0)
						memcpy(m_parts[i].pMatMapping, params->pMatMapping, params->nMats*sizeof(int));
					else
					{
						if (m_parts[i].pMatMapping!=pMapping0 && m_parts[i].pMatMapping!=m_parts[i].pPhysGeom->pMatMapping)
							delete[] m_parts[i].pMatMapping;
						m_parts[i].pMatMapping=0, m_parts[i].nMats=0;
					}
					if (pMapping0 && pMapping0!=m_parts[i].pPhysGeom->pMatMapping) 
						delete[] pMapping0;
				}
			}
			return 1;
		}
		if (is_unused(params->ipart))
			for(i=0;i<m_nParts && m_parts[i].id!=params->partid;i++);
		if (i>=m_nParts)
			return 0;
		Vec3 scale;
		if ((scale=get_xqs_from_matrices(params->pMtx3x4,params->pMtx3x3, params->pos,params->q,params->scale)).len2()>3.03f) {
			if (is_unused(params->pPhysGeom))
				m_pWorld->GetGeomManager()->AddRefGeometry(params->pPhysGeom=m_parts[i].pPhysGeom);
			if (is_unused(params->pPhysGeomProxy))
				m_pWorld->GetGeomManager()->AddRefGeometry(params->pPhysGeomProxy=m_parts[i].pPhysGeomProxy);
			phys_geometry *pgeom0 = params->pPhysGeom;
			if (BakeScaleIntoGeometry(params->pPhysGeom,m_pWorld->GetGeomManager(),scale,1))
				m_pWorld->GetGeomManager()->AddRefGeometry(params->pPhysGeom);
			if (pgeom0!=params->pPhysGeomProxy)	{
				if (BakeScaleIntoGeometry(params->pPhysGeomProxy,m_pWorld->GetGeomManager(),scale,1))
					m_pWorld->GetGeomManager()->AddRefGeometry(params->pPhysGeomProxy);
			} else
				params->pPhysGeomProxy = params->pPhysGeom;
		}
		ENTITY_VALIDATE("CPhysicalEntity:SetParams(pe_params_part)",params);
		coord_block_BBox newCoord,*prevCoord=m_parts[i].pNewCoords;
		Vec3 BBox[2] = { m_BBox[0],m_BBox[1] };
		newCoord.pos = newCoord.BBox[0]=newCoord.BBox[1] = m_parts[i].pos;
		newCoord.q = m_parts[i].q;
		newCoord.scale = m_parts[i].scale;
		int bPosChanged=0;
		if (!is_unused(params->ipart) && !is_unused(params->partid)) m_parts[i].id = params->partid;
		if (!is_unused(params->pos.x)) newCoord.pos = params->pos;
		if (!is_unused(params->q)) newCoord.q = params->q;
		if (!is_unused(params->scale)) m_parts[i].mass *= cube((newCoord.scale = params->scale)/m_parts[i].scale);
		if (!is_unused(params->mass)) m_parts[i].mass = params->mass;
		if (!is_unused(params->density)) m_parts[i].mass = m_parts[i].pPhysGeom->V*cube(m_parts[i].scale)*params->density;
		if (!is_unused(params->minContactDist) && is_unused(params->idSkeleton)) m_parts[i].minContactDist = params->minContactDist;
		if (!is_unused(params->pPhysGeom) && params->pPhysGeom!=m_parts[i].pPhysGeom && params->pPhysGeom && params->pPhysGeom->pGeom) {
			WriteLock lock(m_lockUpdate);
			if (m_parts[i].pMatMapping==m_parts[i].pPhysGeom->pMatMapping) {
				m_parts[i].pMatMapping = params->pPhysGeom->pMatMapping;
				m_parts[i].nMats = params->pPhysGeom->nMats;
			}
			m_parts[i].surface_idx = params->pPhysGeom->surface_idx;
			if (m_parts[i].pPhysGeom==m_parts[i].pPhysGeomProxy) {
				m_pWorld->GetGeomManager()->UnregisterGeometry(m_parts[i].pPhysGeomProxy);
				m_pWorld->GetGeomManager()->AddRefGeometry(m_parts[i].pPhysGeomProxy = params->pPhysGeom);
			}
			m_pWorld->GetGeomManager()->UnregisterGeometry(m_parts[i].pPhysGeom);
			m_parts[i].pPhysGeom = params->pPhysGeom;
			m_pWorld->GetGeomManager()->AddRefGeometry(params->pPhysGeom);
			if (m_parts[i].flags & geom_can_modify && m_pStructure && m_pStructure->defparts && m_pStructure->defparts[i].pSkelEnt && 
					!((CGeometry*)m_parts[i].pPhysGeom->pGeom)->IsAPrimitive())
				m_pWorld->ClonePhysGeomInEntity(this, i, m_pWorld->CloneGeometry(m_parts[i].pPhysGeom->pGeom));
			if (m_nRefCount && m_iSimClass==0 && m_pWorld->m_vars.lastTimeStep>0.0f) {
				CPhysicalEntity **pentlist;
				Vec3 inflator = Vec3(10.0f)*m_pWorld->m_vars.maxContactGap;
				for(int j=m_pWorld->GetEntitiesAround(m_BBox[0]-inflator,m_BBox[1]+inflator, pentlist, ent_sleeping_rigid|ent_living|ent_independent)-1; j>=0; j--)
					if (m_iSimClass || pentlist[j]->HasPartContactsWith(this,i))
						pentlist[j]->Awake();
			}
		}
		if (!is_unused(params->pPhysGeom) && params->pPhysGeom && params->pPhysGeom->pGeom)
			m_pWorld->GetGeomManager()->UnregisterGeometry(params->pPhysGeom);
		if (!is_unused(params->pPhysGeomProxy) && params->pPhysGeomProxy!=m_parts[i].pPhysGeomProxy && 
				params->pPhysGeomProxy && params->pPhysGeomProxy->pGeom) 
		{
			WriteLock lock(m_lockUpdate);
			m_pWorld->GetGeomManager()->UnregisterGeometry(m_parts[i].pPhysGeomProxy);
			m_parts[i].pPhysGeomProxy = params->pPhysGeomProxy;
			m_pWorld->GetGeomManager()->AddRefGeometry(params->pPhysGeomProxy);
		}
		if (!is_unused(params->pPhysGeomProxy) && params->pPhysGeomProxy && params->pPhysGeomProxy->pGeom)
			m_pWorld->GetGeomManager()->UnregisterGeometry(params->pPhysGeomProxy);
		if (!is_unused(params->pLattice) && (ITetrLattice*)m_parts[i].pLattice!=params->pLattice && 
				m_parts[i].pPhysGeomProxy->pGeom->GetType()==GEOM_TRIMESH) 
		{
			if (m_parts[i].flags & geom_can_modify && m_parts[i].pLattice)
				m_parts[i].pLattice->Release();
			(m_parts[i].pLattice = (CTetrLattice*)params->pLattice)->SetMesh((CTriMesh*)m_parts[i].pPhysGeomProxy->pGeom);
		}
		if (!is_unused(params->pMatMapping)) {
			int *pMapping0 = m_parts[i].pMatMapping;
			if ((!pMapping0 || params->nMats!=m_parts[i].nMats) && params->pMatMapping)
				m_parts[i].pMatMapping = new int[m_parts[i].nMats = params->nMats]; 
			else pMapping0 = 0;
			if (params->pMatMapping && params->nMats>0)
				memcpy(m_parts[i].pMatMapping, params->pMatMapping, params->nMats*sizeof(int));
			else
			{
				if (m_parts[i].pMatMapping!=pMapping0 && m_parts[i].pMatMapping!=m_parts[i].pPhysGeom->pMatMapping)
					delete[] m_parts[i].pMatMapping;
				m_parts[i].pMatMapping=0, m_parts[i].nMats=0;
			}
			if (pMapping0 && pMapping0!=m_parts[i].pPhysGeom->pMatMapping) 
				delete[] pMapping0;
		}
		if (!is_unused(params->idmatBreakable)) 
			m_parts[i].idmatBreakable = params->idmatBreakable;
		else if (!is_unused(params->pMatMapping) || !is_unused(params->pPhysGeom))
			UpdatePartIdmatBreakable(i);
		m_parts[i].flags = m_parts[i].flags & params->flagsAND | params->flagsOR;
		m_parts[i].flagsCollider = m_parts[i].flagsCollider & params->flagsColliderAND | params->flagsColliderOR;
		if (!is_unused(params->idSkeleton)) {
			if (m_pStructure && m_pStructure->defparts && m_pStructure->defparts[i].pSkinInfo && params->idSkeleton<0) {
				delete[] m_pStructure->defparts[i].pSkinInfo;
				m_pWorld->DestroyPhysicalEntity(m_pStructure->defparts[i].pSkelEnt,0,1);
				m_pStructure->defparts[i].pSkinInfo = 0; 
				m_pStructure->defparts[i].pSkelEnt = 0;
			}	else if (params->idSkeleton>=0) {
				int iskel;
				for(iskel=0; iskel<m_nParts && m_parts[iskel].id!=params->idSkeleton; iskel++);
				if (iskel<m_nParts && MakeDeformable(i, iskel, is_unused(params->minContactDist) ? 0.0f:params->minContactDist)) {
					assert(m_pStructure->defparts);
					m_pStructure->defparts[i].idSkel = params->idSkeleton;
					m_parts[i].flags |= geom_monitor_contacts;
				}
			}
		}
		if (!is_unused(params->idParent)) {
			AllocStructureInfo();
			int j; for(j=m_nParts-1;j>=0 && m_parts[j].id!=params->idParent;j--);
			m_pStructure->pParts[i].iParent = j+1;
			m_pStructure->pParts[j].bGroup = 1;
			if (j>=0) {
				m_parts[j].flags &= ~geom_colltype_explosion;
				m_pStructure->pParts[i].flags0 = m_parts[i].flags;
				m_pStructure->pParts[i].flagsCollider0 = m_parts[i].flagsCollider;
				m_parts[i].flags &= geom_colltype_explosion|geom_monitor_contacts;
				m_parts[i].flagsCollider = 0;
			}
		}
		if (m_parts[i].pLattice)
			m_parts[i].flags |= geom_monitor_contacts;
		if (params->bRecalcBBox) {
			m_parts[i].pNewCoords = &newCoord;
			ComputeBBox(BBox);
			bPosChanged = m_pWorld->RepositionEntity(this,1,BBox);
		}
		{ WriteLock lock(m_lockUpdate);
			m_parts[i].pNewCoords = prevCoord;
			m_parts[i].pos = m_parts[i].pNewCoords->pos = newCoord.pos;
			m_parts[i].q = m_parts[i].pNewCoords->q = newCoord.q;
			m_parts[i].scale = m_parts[i].pNewCoords->scale = newCoord.scale;
			if (params->bRecalcBBox) {
				WriteBBox(BBox);
				m_parts[i].BBox[0] = m_parts[i].pNewCoords->BBox[0] = newCoord.BBox[0];
				m_parts[i].BBox[1] = m_parts[i].pNewCoords->BBox[1] = newCoord.BBox[1];
				for(int j=0;j<m_nParts;j++) 
					if (i!=j && m_parts[j].pNewCoords!=(coord_block_BBox*)&m_parts[j].pos) {
						m_parts[j].BBox[0] = m_parts[j].pNewCoords->BBox[0];
						m_parts[j].BBox[1] = m_parts[j].pNewCoords->BBox[1];
					}
			}
			m_pWorld->UnlockGrid(this,-bPosChanged);
		}
		if (params->bRecalcBBox)
			RepositionParts();
		return i+1;
	} 
	
	if (_params->type==pe_params_outer_entity::type_id) {
		if (m_pOuterEntity) m_pOuterEntity->Release();
		m_pOuterEntity = (CPhysicalEntity*)((pe_params_outer_entity*)_params)->pOuterEntity;
		if (m_pOuterEntity) m_pOuterEntity->AddRef();
		return 1;
	}
	
	if (_params->type==pe_params_foreign_data::type_id) {
		//WriteLock lock(m_lockUpdate);
		pe_params_foreign_data *params = (pe_params_foreign_data*)_params;

		void* pForeignDataOld = m_pForeignData;
		if (!is_unused(params->pForeignData)) m_pForeignData = 0;
		if (!is_unused(params->iForeignData)) m_iForeignData = params->iForeignData;
		if (!is_unused(params->pForeignData)) m_pForeignData = params->pForeignData;
		if (!is_unused(params->iForeignFlags)) m_iForeignFlags = params->iForeignFlags;
		m_iForeignFlags = (m_iForeignFlags & params->iForeignFlagsAND) | params->iForeignFlagsOR;
		if (m_pEntBuddy) {
			m_pEntBuddy->m_pForeignData = m_pForeignData;
			m_pEntBuddy->m_iForeignData = m_iForeignData;
			m_pEntBuddy->m_iForeignFlags = m_iForeignFlags;
		}

		if (pForeignDataOld && m_pForeignData && pForeignDataOld!=m_pForeignData) 
			// Changing foreign-data, clear the event queue of potentially stale events
			m_pWorld->PatchEventsQueue(this, m_pForeignData, m_iForeignData);

		return 1;
	}
	
	if (_params->type==pe_params_collision_class::type_id) {
		pe_params_collision_class *params = (pe_params_collision_class*)_params;
		m_collisionClass.type &= params->collisionClassAND.type;
		m_collisionClass.type |= params->collisionClassOR.type;
		m_collisionClass.ignore &= params->collisionClassAND.ignore;
		m_collisionClass.ignore |= params->collisionClassOR.ignore;
		return 1;
	}

	if (_params->type==pe_params_flags::type_id) {
		pe_params_flags *params = (pe_params_flags*)_params;
		if (!is_unused(params->flags)) m_flags = params->flags;
		if (!is_unused(params->flagsAND)) m_flags &= params->flagsAND;
		if (!is_unused(params->flagsOR)) m_flags |= params->flagsOR;
		if (m_flags & pef_parts_traceable && m_iSimClass<3)	{
			Vec3 gBBox[2]; m_pWorld->GetGrid(this)->BBoxToGrid(m_BBox[0],m_BBox[1],gBBox);
			Vec3 sz = gBBox[1]-gBBox[0];
			if (float2int(sz.x*m_pWorld->m_entgrid.stepr.x+0.5f)*float2int(sz.y*m_pWorld->m_entgrid.stepr.y+0.5f)*4 < m_nParts)
				(m_flags &= ~pef_parts_traceable) |= pef_traceable;
			else
				m_flags &= ~pef_traceable;
		}

		if (m_flags&pef_traceable && m_ig[0].x==NO_GRID_REG) {
			m_ig[0].x=m_ig[1].x=m_ig[0].y=m_ig[1].y = m_flags & pef_disabled ? GRID_REG_LAST : GRID_REG_PENDING;
			if (m_pos.len2()>0)
				m_pWorld->RepositionEntity(this,1|8);
			RepositionParts();
		}
		if (!(m_flags&pef_traceable) && m_ig[0].x!=NO_GRID_REG) {
			{ WriteLock lock(m_pWorld->m_lockGrid);
				m_pWorld->DetachEntityGridThunks(this);
				m_ig[0].x=m_ig[1].x=m_ig[0].y=m_ig[1].y = NO_GRID_REG;
			}
			RepositionParts();
		}

		return 1;
	}

	if (_params->type==pe_params_ground_plane::type_id) {
		pe_params_ground_plane *params = (pe_params_ground_plane*)_params;
		if (params->iPlane<0)	{
			m_nGroundPlanes = 0; 
			if (m_ground) { delete[] m_ground; m_ground=0; }
		} else if (params->iPlane>=m_nGroundPlanes)	{
			ReallocateList(m_ground,m_nGroundPlanes,params->iPlane+1,true);
			m_nGroundPlanes = params->iPlane+1;
		}
		if (!is_unused(params->ground.origin)) m_ground[params->iPlane].origin = params->ground.origin;
		if (!is_unused(params->ground.n)) m_ground[params->iPlane].n = params->ground.n;
		return 1;
	}

	if (_params->type==pe_params_structural_joint::type_id) {
		pe_params_structural_joint *params = (pe_params_structural_joint*)_params;
		int i,j,iop;

		if (!is_unused(params->idx) && params->idx==-1) {
			if (m_pStructure) {
				if (!is_unused(params->partidEpicenter)) m_pStructure->idpartBreakOrg = params->partidEpicenter;
				m_pWorld->MarkEntityAsDeforming(this);
			}
			return 1;
		}
		if (params->idx==-2)
			return GenerateJoints();	
		AllocStructureInfo();

		if (params->bReplaceExisting)
			for(i=0; i<m_pStructure->nJoints && m_pStructure->pJoints[i].id!=params->id; i++);
		else i = m_pStructure->nJoints;
		if (i>=m_pStructure->nJointsAlloc)
			ReallocateList(m_pStructure->pJoints, m_pStructure->nJointsAlloc,m_pStructure->nJointsAlloc+4,true), m_pStructure->nJointsAlloc+=4;
		if (i>=m_pStructure->nJoints)
			memset(m_pStructure->pJoints+i, 0, sizeof(m_pStructure->pJoints[0]));

		for(iop=0;iop<2;iop++) if (!is_unused(params->partid[iop]) || i==m_pStructure->nJoints) {
			for(j=m_nParts-1;j>=0 && m_parts[j].id!=params->partid[iop];j--);
			if (j>=0 && m_parts[j].mass==0 && (is_unused(params->limitConstraint) || params->limitConstraint.z<=0) && params->id!=joint_impulse)
				j = -1;
			m_pStructure->pJoints[i].ipart[iop] = j;
		}
		if (m_pStructure->pJoints[i].ipart[0]==-1) {
			j=m_pStructure->pJoints[i].ipart[0]; m_pStructure->pJoints[i].ipart[0]=m_pStructure->pJoints[i].ipart[1]; m_pStructure->pJoints[i].ipart[1]=j;
		}
		if (!is_unused(params->pt)) m_pStructure->pJoints[i].pt = params->pt;
		if (!is_unused(params->n)) m_pStructure->pJoints[i].n = params->n;
		if (!is_unused(params->axisx)) m_pStructure->pJoints[i].axisx = params->axisx;
		if (!is_unused(params->maxForcePush)) m_pStructure->pJoints[i].maxForcePush = min_safe(1e15f, params->maxForcePush);
		if (!is_unused(params->maxForcePull)) m_pStructure->pJoints[i].maxForcePull = min_safe(1e15f, params->maxForcePull);
		if (!is_unused(params->maxForceShift)) m_pStructure->pJoints[i].maxForceShift = min_safe(1e15f, params->maxForceShift);
		if (!is_unused(params->maxTorqueBend)) m_pStructure->pJoints[i].maxTorqueBend = min_safe(1e15f, params->maxTorqueBend);
		if (!is_unused(params->maxTorqueTwist)) m_pStructure->pJoints[i].maxTorqueTwist = min_safe(1e15f, params->maxTorqueTwist);
    if (!is_unused(params->limitConstraint)) m_pStructure->pJoints[i].limitConstr = params->limitConstraint;
		if (!is_unused(params->dampingConstraint)) m_pStructure->pJoints[i].dampingConstr = params->dampingConstraint;
		if (!is_unused(params->bBreakable)) 
			(m_pStructure->pJoints[i].bBreakable &= ~1) |= params->bBreakable;
		else if (i==m_pStructure->nJoints)
			m_pStructure->pJoints[i].bBreakable = 1;
		if (!is_unused(params->bConstraintWillIgnoreCollisions))
			(m_pStructure->pJoints[i].bBreakable &= ~2) |= params->bConstraintWillIgnoreCollisions*2^2;
		if (!is_unused(params->bDirectBreaksOnly)) {
			(m_pStructure->pJoints[i].bBreakable &= ~(4|params->bDirectBreaksOnly)) |= params->bDirectBreaksOnly*4;
			m_pStructure->bHasDirectBreaks |= params->bDirectBreaksOnly;
		}
		if (!is_unused(params->id))
			m_pStructure->pJoints[i].id = params->id;
		else if (i==m_pStructure->nJoints)
			m_pStructure->pJoints[i].id = i;
		if (!is_unused(params->szSensor))
			m_pStructure->pJoints[i].size = params->szSensor;
		else if (i==m_pStructure->nJoints)
			m_pStructure->pJoints[i].size = m_pWorld->m_vars.maxContactGap*5;
    if (!is_unused(params->damageAccum)) m_pStructure->pJoints[i].damageAccum =  max_safe(0.f,params->damageAccum); 
    else m_pStructure->pJoints[i].damageAccum = m_pWorld->m_vars.jointDmgAccum; 
    if (!is_unused(params->damageAccumThresh)) m_pStructure->pJoints[i].damageAccumThresh =  max_safe(0.f,min_safe(1.f, params->damageAccumThresh));
    else m_pStructure->pJoints[i].damageAccumThresh = m_pWorld->m_vars.jointDmgAccumThresh; 
    m_pStructure->pJoints[i].tension = 0;
		m_pStructure->pJoints[i].itens = 0;
		m_pStructure->nJoints = max(i+1,m_pStructure->nJoints);
		if (!is_unused(params->bBroken)) {
			if (m_pStructure->pJoints[i].bBroken!=params->bBroken && (m_pStructure->pJoints[i].bBroken=params->bBroken) && i<m_pStructure->nJoints) {
				if (is_unused(params->partidEpicenter)) {
					if (i!=m_pStructure->nJoints-1)
						m_pStructure->pJoints[i] = m_pStructure->pJoints[m_pStructure->nJoints-1];
					--m_pStructure->nJoints;
				} else {
					m_pStructure->pJoints[i].bBroken = params->bBroken;
					m_pStructure->idpartBreakOrg = params->partidEpicenter;
				}
				m_pWorld->MarkEntityAsDeforming(this);
			}
		} else
			m_pStructure->pJoints[i].bBroken = 0;
		MEMSTAT_USAGE(m_pStructure->pJoints, sizeof(m_pStructure->pJoints[0]) * m_pStructure->nJoints);
		return 1;
	}

	if (_params->type==pe_params_structural_initial_velocity::type_id) {
		pe_params_structural_initial_velocity *params = (pe_params_structural_initial_velocity*)_params;
		if (m_pStructure) {
			int i;
			for(i=0; i<m_nParts && m_parts[i].id!=params->partid; i++);
			if (i<m_nParts) {
				m_pStructure->pParts[i].initialVel = params->v;
				m_pStructure->pParts[i].initialAngVel = params->w;
				return 1;
			}
			return 0;
		}
	}
	if (_params->type==pe_params_timeout::type_id) {
		pe_params_timeout *params = (pe_params_timeout*)_params;
		if (!is_unused(params->timeIdle)) m_timeIdle = params->timeIdle;
		if (!is_unused(params->maxTimeIdle)) m_maxTimeIdle = params->maxTimeIdle;
		return 1;
	}

	if (_params->type==pe_params_skeleton::type_id) {
		pe_params_skeleton *params = (pe_params_skeleton*)_params;
		int i;
		if (!is_unused(params->ipart)) 
			i = params->ipart;
		else if (!is_unused(params->partid)) 
			for(i=m_nParts-1;i>=0 && params->partid!=m_parts[i].id;i--);
		else 
			return 0;
		if ((unsigned int)i>=(unsigned int)m_nParts || !m_pStructure || !m_pStructure->defparts || !m_pStructure->defparts[i].pSkelEnt)
			return 0;
		pe_params_softbody psb;
		if (!is_unused(params->stiffness)) psb.shapeStiffnessNorm = params->stiffness;
		if (!is_unused(params->thickness)) psb.thickness = params->thickness;
		if (!is_unused(params->maxStretch)) psb.maxSafeStep = params->maxStretch;
		if (!is_unused(params->maxImpulse)) m_pStructure->defparts[i].maxImpulse = params->maxImpulse;
		if (!is_unused(params->timeStep)) m_pStructure->defparts[i].timeStep = params->timeStep;
		if (!is_unused(params->nSteps)) m_pStructure->defparts[i].nSteps = params->nSteps;
		if (!is_unused(params->hardness)) psb.nMaxIters=max(1,min(20,float2int(psb.ks=params->hardness)));
		if (!is_unused(params->explosionScale)) { 
			psb.explosionScale = params->explosionScale*0.005f;
			psb.maxCollisionImpulse = 200.0f*params->explosionScale;
		}
		m_pStructure->defparts[i].pSkelEnt->SetParams(&psb);
		if (!is_unused(params->bReset) && params->bReset) {
			pe_action_reset ar;	ar.bClearContacts=3;
			m_pStructure->defparts[i].pSkelEnt->Action(&ar);
		}
		return 1;
	}

	return 0;
}


int CPhysicalEntity::GetParams(pe_params *_params) const
{
	if (_params->type==pe_params_bbox::type_id) {
		pe_params_bbox *params = (pe_params_bbox*)_params;
		ReadLock lock(m_lockUpdate);
		params->BBox[0] = m_BBox[0];
		params->BBox[1] = m_BBox[1];
		return 1;
	}

	if (_params->type==pe_params_outer_entity::type_id) {
		((pe_params_outer_entity*)_params)->pOuterEntity = m_pOuterEntity;
		return 1;
	}
	
	if (_params->type==pe_params_foreign_data::type_id) {
		pe_params_foreign_data *params = (pe_params_foreign_data*)_params;
		ReadLock lock(m_lockUpdate);
		params->iForeignData = m_iForeignData;
		params->pForeignData = m_pForeignData;
		params->iForeignFlags = m_iForeignFlags;
		return 1;
	}

	if (_params->type==pe_params_part::type_id) {
		pe_params_part *params = (pe_params_part*)_params;
		ReadLockCond lock(m_lockUpdate,m_pWorld->m_vars.bLogStructureChanges);
		int i;
		if ((is_unused(params->ipart) || params->ipart==-1) && params->partid>=0) {
			for(i=0;i<m_nParts && m_parts[i].id!=params->partid;i++);
			if (i==m_nParts) return 0;
		} else if ((unsigned int)(i = params->ipart)>=(unsigned int)m_nParts)
			return 0;
		params->partid = m_parts[params->ipart = i].id;
		params->pos = m_parts[i].pos;
		params->q = m_parts[i].q;
		params->scale = m_parts[i].scale;
		if (params->pMtx3x4) 
			(*params->pMtx3x4 = Matrix33(m_parts[i].q)*m_parts[i].scale).SetTranslation(m_parts[i].pos);
		if (params->pMtx3x3) 
			*params->pMtx3x3 = Matrix33(m_parts[i].q)*m_parts[i].scale;
		params->flagsOR=params->flagsAND = m_parts[i].flags;
		params->flagsColliderOR=params->flagsColliderAND = m_parts[i].flagsCollider;
		params->mass = m_parts[i].mass;
		params->density = m_parts[i].pPhysGeomProxy->V>0 ?
			m_parts[i].mass/(m_parts[i].pPhysGeomProxy->V*cube(m_parts[i].scale)) : 0;
		params->minContactDist = m_parts[i].minContactDist;
		params->pPhysGeom = m_parts[i].pPhysGeom;
		params->pPhysGeomProxy = m_parts[i].pPhysGeomProxy;
		params->idmatBreakable = m_parts[i].idmatBreakable;
		params->pLattice = m_parts[i].pLattice;
		params->pMatMapping = m_parts[i].pMatMapping;
		params->nMats = m_parts[i].nMats;
		params->idSkeleton = m_pStructure && m_pStructure->defparts ? m_pStructure->defparts[i].idSkel : -1;
		if (params->bAddrefGeoms) {
			m_pWorld->GetGeomManager()->AddRefGeometry(params->pPhysGeom);
			m_pWorld->GetGeomManager()->AddRefGeometry(params->pPhysGeomProxy);
		}
		return 1;
	}

	if (_params->type==pe_params_flags::type_id) {
		((pe_params_flags*)_params)->flags = m_flags;
		return 1;
	}
	
	if (_params->type==pe_params_collision_class::type_id) {
		pe_params_collision_class *params = (pe_params_collision_class*)_params;
		params->collisionClassAND.type = params->collisionClassOR.type = m_collisionClass.type;
		params->collisionClassAND.ignore = params->collisionClassOR.ignore = m_collisionClass.ignore;
		return 1;
	}

	if (_params->type==pe_params_ground_plane::type_id) {
		pe_params_ground_plane *params = (pe_params_ground_plane*)_params;
		SIZEOF_ARRAY_OK
		if (params->iPlane>=m_nGroundPlanes)
			return 0;
		params->ground.origin = m_ground[params->iPlane].origin;
		params->ground.n = m_ground[params->iPlane].n;
		return 1;
	}

	if (_params->type==pe_params_structural_joint::type_id) {
		pe_params_structural_joint *params = (pe_params_structural_joint*)_params;
		if (!m_pStructure)
			return 0;
		int i,iop;
		if (!is_unused(params->idx))
			i = params->idx;
		else for(i=0; i<m_pStructure->nJoints && m_pStructure->pJoints[i].id!=params->id; i++);
		if ((unsigned int)i>=(unsigned int)m_pStructure->nJoints)
			return 0;
		for(iop=0;iop<2;iop++)
			if ((params->partid[iop] = m_pStructure->pJoints[i].ipart[iop])>=0)
				params->partid[iop] = m_parts[params->partid[iop]].id;
		params->idx = i;
		params->id = m_pStructure->pJoints[i].id;
		params->pt = m_pStructure->pJoints[i].pt;
		params->n = m_pStructure->pJoints[i].n;
		params->axisx = m_pStructure->pJoints[i].axisx;
		params->maxForcePush = m_pStructure->pJoints[i].maxForcePush;
		params->maxForcePull = m_pStructure->pJoints[i].maxForcePull;
		params->maxForceShift = m_pStructure->pJoints[i].maxForceShift;
		params->maxTorqueBend = m_pStructure->pJoints[i].maxTorqueBend;
		params->maxTorqueTwist = m_pStructure->pJoints[i].maxTorqueTwist;
    params->damageAccum = m_pStructure->pJoints[i].damageAccum; 
    params->damageAccumThresh = m_pStructure->pJoints[i].damageAccumThresh; 
		params->limitConstraint = m_pStructure->pJoints[i].limitConstr;
		params->dampingConstraint = m_pStructure->pJoints[i].dampingConstr;
		params->bBreakable = m_pStructure->pJoints[i].bBreakable & 1;
		params->bConstraintWillIgnoreCollisions = m_pStructure->pJoints[i].bBreakable>>1^1;
		params->bDirectBreaksOnly = m_pStructure->pJoints[i].bBreakable>>2;
		params->bBroken = m_pStructure->pJoints[i].bBroken;
		return 1;
	}

	if (_params->type==pe_params_timeout::type_id) {
		pe_params_timeout *params = (pe_params_timeout*)_params;
		params->timeIdle = m_timeIdle;
		params->maxTimeIdle = m_maxTimeIdle;
		return 1;
	}

	if (_params->type==pe_params_skeleton::type_id) {
		pe_params_skeleton *params = (pe_params_skeleton*)_params;
		int i;
		if (!is_unused(params->ipart)) 
			i = params->ipart;
		else if (!is_unused(params->partid)) 
			for(i=m_nParts-1;i>=0 && params->partid!=m_parts[i].id;i--);
		else 
			return 0;
		if ((unsigned int)i>=(unsigned int)m_nParts || !m_pStructure || !m_pStructure->defparts || !m_pStructure->defparts[i].pSkelEnt)
			return 0;
		pe_params_softbody psb;
		m_pStructure->defparts[i].pSkelEnt->GetParams(&psb);
		params->stiffness = psb.shapeStiffnessNorm;
		params->thickness = psb.thickness;
		params->maxStretch = psb.maxSafeStep;
		params->maxImpulse = m_pStructure->defparts[i].maxImpulse;
		params->timeStep = m_pStructure->defparts[i].timeStep;
		params->nSteps = m_pStructure->defparts[i].nSteps;
		params->hardness = psb.ks;
		params->explosionScale = psb.explosionScale;
		return 1;
	}

	return 0;
}


int CPhysicalEntity::GetStatus(pe_status *_status) const
{
	if (_status->type==pe_status_pos::type_id) {
		pe_status_pos *status = (pe_status_pos*)_status;
		ReadLockCond lock(m_lockUpdate, (status->flags^status_thread_safe)&status_thread_safe & -m_pWorld->m_vars.bLogStructureChanges);
		Vec3 respos;
		quaternionf resq;
		float resscale;
		int i;

		if (status->ipart==-1 && status->partid>=0) {
			for(i=0;i<m_nParts && m_parts[i].id!=status->partid;i++);
			if (i==m_nParts) return 0;
		} else
			i = status->ipart;

		if (i<0) {
			respos=m_pos; resq=m_qrot; resscale=1.0f;
			for(i=0,status->flagsOR=0,status->flagsAND=0xFFFFFFFF;i<m_nParts;i++) 
				status->flagsOR|=m_parts[i].flags, status->flagsAND&=m_parts[i].flags;
			if (m_nParts>0) {
				status->pGeom = m_parts[0].pPhysGeom->pGeom;
				status->pGeomProxy = m_parts[0].pPhysGeomProxy->pGeom;
			} else
				status->pGeom = status->pGeomProxy = 0;
			status->BBox[0] = m_BBox[0]-m_pos;
			status->BBox[1] = m_BBox[1]-m_pos;
		} else if (i<m_nParts) {
			if (status->flags & status_local) {
				respos = m_parts[i].pos; resq = m_parts[i].q;
			} else {
				respos = m_pos+m_qrot*m_parts[i].pos;
				resq = m_qrot*m_parts[i].q;
			}
			resscale = m_parts[i].scale;
			status->partid = m_parts[i].id;
			status->flagsOR = status->flagsAND = m_parts[i].flags;
			status->pGeom = m_parts[i].pPhysGeom->pGeom;
			status->pGeomProxy = m_parts[i].pPhysGeomProxy->pGeom;
			if (status->flags & status_addref_geoms) {
				if (status->pGeom) status->pGeom->AddRef();
				if (status->pGeomProxy) status->pGeomProxy->AddRef();
			}
			status->BBox[0] = m_parts[i].BBox[0]-respos;
			status->BBox[1] = m_parts[i].BBox[1]-respos;
		}	else
			return 0;

		if (!is_unused(status->pGridRefEnt)) {
			if (status->pGridRefEnt && m_pWorld->GetGrid(this)!=m_pWorld->GetGrid(status->pGridRefEnt) && (i<0 || !(status->flags & status_local)))
				transformBBox(status->BBox[0],status->BBox[1], status->BBox, 
					QuatT(m_pWorld->TransformToGrid(this,status->pGridRefEnt, respos,resq).q, Vec3(ZERO)));
		} else 
			status->pGridRefEnt = m_pWorld->GetGrid(this);

		status->pos = respos;
		status->q = resq;
		status->scale = resscale;
		status->iSimClass = m_iSimClass;

		if (status->pMtx3x4)
			(*status->pMtx3x4 = Matrix33(resq)*resscale).SetTranslation(respos);
		if (status->pMtx3x3)
			(Matrix33&)*status->pMtx3x3 = (Matrix33(resq)*resscale);
		return 1;
	}

	if (_status->type==pe_status_id::type_id) {
		pe_status_id *status = (pe_status_id*)_status;
		ReadLock lock(m_lockUpdate);
		int ipart = status->ipart;
		if (ipart<0) 
			for(ipart=0;ipart<m_nParts-1 && m_parts[ipart].id!=status->partid;ipart++);
		if (ipart>=m_nParts)
			return 0;
		phys_geometry *pgeom = status->bUseProxy ? m_parts[ipart].pPhysGeomProxy : m_parts[ipart].pPhysGeom;
		if ((unsigned int)status->iPrim >= (unsigned int)pgeom->pGeom->GetPrimitiveCount() ||
				pgeom->pGeom->GetType()==GEOM_TRIMESH && status->iFeature>2)
			return 0;
		status->id = pgeom->pGeom->GetPrimitiveId(status->iPrim, status->iFeature);
		status->id = status->id&~(status->id>>31) | m_parts[ipart].surface_idx&status->id>>31;
		return 1;
	}

	if (_status->type==pe_status_nparts::type_id)
		return m_nParts;

	if (_status->type==pe_status_awake::type_id)
		return IsAwake();

	if (_status->type==pe_status_contains_point::type_id)
		return IsPointInside(((pe_status_contains_point*)_status)->pt);

	if (_status->type==pe_status_caps::type_id) {
		pe_status_caps *status = (pe_status_caps*)_status;
		status->bCanAlterOrientation = 0;
		return 1;
	}

	if (_status->type==pe_status_extent::type_id)
	{
		pe_status_extent *status = (pe_status_extent*)_status;
		status->extent = GetExtent(status->eForm);
		return 1;
	}

	if (_status->type==pe_status_random::type_id)
	{
		pe_status_random *status = (pe_status_random*)_status;
		GetRandomPoints(status->points, status->seed, status->eForm);
		return 1;
	}

	return 0;
};


int CPhysicalEntity::Action(pe_action *_action, int bThreadSafe)
{
	ChangeRequest<pe_action> req(this,m_pWorld,_action,bThreadSafe);
	if (req.IsQueued())	{
		if (_action->type==pe_action_remove_all_parts::type_id) {
			WriteLock lock(m_lockUpdate);
			for(int i=0;i<m_nParts;i++) m_parts[i].flags |= geom_removed;
		}
		return 1;
	}

	if (_action->type==pe_action_impulse::type_id) {
		pe_action_impulse *action = (pe_action_impulse*)_action;
		if (!is_unused(action->ipart) || !is_unused(action->partid)) {
			int i;
			if (is_unused(action->ipart))
				for(i=0; i<m_nParts && m_parts[i].id!=action->partid; i++);
			else i = action->ipart;
			if ((unsigned int)i>=(unsigned int)m_nParts)
				return 0;
			if (!is_unused(action->point))
				i = MapHitPointFromParent(i,action->point);

			float scale = 1.0f+(m_pWorld->m_vars.breakImpulseScale-1.0f)*iszero((int)m_flags & pef_override_impulse_scale);
			if (m_pStructure) {
				int bMarkEntityAsDeforming = 0;
				Vec3 dL(ZERO),dP=action->impulse*scale;
				if (!is_unused(action->angImpulse))
					dL = action->angImpulse;
				else if (!is_unused(action->point)) {
					Vec3 bodypos = m_qrot*(m_parts[i].q*m_parts[i].pPhysGeom->origin*m_parts[i].scale+m_parts[i].pos)+m_pos;
					dL = action->point-bodypos ^ action->impulse;
				}

				Vec3 sz = m_parts[i].BBox[1]-m_parts[i].BBox[0];
				if (max((m_pStructure->pParts[i].Pext+dP*scale).len2(), (m_pStructure->pParts[i].Lext+dL*scale).len2()*sqr(sz.x+sz.y+sz.z)*0.1f) > sqr(m_parts[i].mass*0.01f)) {
					bMarkEntityAsDeforming=1;	m_pStructure->pParts[i].Pext+=dP*scale; m_pStructure->pParts[i].Lext+=dL*scale;
					if (m_pStructure->bHasDirectBreaks && !is_unused(action->point) && action->iSource==0) {
						Vec3 ptloc = (action->point-m_pos)*m_qrot;
						int ijoint=-1; float dist,mindist=1e10f,imp=dP.len2()*sqr(scale);
						for(int j=0; j<m_pStructure->nJoints; j++) 
							if (m_pStructure->pJoints[j].bBreakable & 4 && 
									(m_pStructure->pJoints[j].ipart[0]==i || m_pStructure->pJoints[j].ipart[1]==i) &&
									imp>sqr(m_pStructure->pJoints[j].maxForcePull*0.01f) &&
									(dist=(ptloc-m_pStructure->pJoints[j].pt).len2())<mindist)
									mindist=dist, ijoint=j;
						if (ijoint>=0) {
							m_pStructure->pJoints[ijoint].bBroken = 1;
							m_pStructure->idpartBreakOrg = m_parts[i].id;
						}
					}
				}

				if (max(action->impulse.len2(), dL.len2()*sqr(sz.x+sz.y+sz.z)*0.1f) >
						sqr(m_parts[i].mass*0.01f)*m_pWorld->m_vars.gravity.len2())
					if ((action->iSource==0 || action->iSource==3) && m_pStructure->defparts && m_pStructure->defparts[i].pSkelEnt && !is_unused(action->point)) {
						pe_action_impulse ai; 
						ai.impulse = m_pStructure->defparts[i].lastUpdateq*!m_qrot*action->impulse;
						ai.point = m_pStructure->defparts[i].lastUpdateq*(!m_qrot*(action->point-m_pos))+m_pStructure->defparts[i].lastUpdatePos;
						if (ai.impulse.len2()>sqr(m_pStructure->defparts[i].maxImpulse))
							ai.impulse = ai.impulse.normalized()*m_pStructure->defparts[i].maxImpulse;
						m_pStructure->defparts[i].pSkelEnt->Action(&ai);
					}
				if (bMarkEntityAsDeforming)
					m_pWorld->MarkEntityAsDeforming(this);
				if (action->iSource==2 && m_pWorld->m_vars.bDebugExplosions) {
					if (!m_pStructure->Pexpl) m_pStructure->Pexpl = new Vec3[m_nParts];
					if (!m_pStructure->Lexpl) m_pStructure->Lexpl = new Vec3[m_nParts];
					if ((m_pStructure->lastExplPos-m_pWorld->m_lastEpicenter).len2()) {
						memset(m_pStructure->Pexpl, 0, m_nParts*sizeof(Vec3));
						memset(m_pStructure->Lexpl, 0, m_nParts*sizeof(Vec3));
					}
					m_pStructure->lastExplPos = m_pWorld->m_lastEpicenter;
					m_pStructure->Pexpl[i] += action->impulse*scale;
					m_pStructure->Lexpl[i] += dL*scale;
				}
			}

			if (m_parts[i].pLattice && m_parts[i].idmatBreakable>=0 && !is_unused(action->point)) {
				float rdensity = m_parts[i].pLattice->m_density*m_parts[i].pPhysGeom->V*cube(m_parts[i].scale)/m_parts[i].mass;
				Vec3 ptloc = ((action->point-m_pos)*m_qrot-m_parts[i].pos)*m_parts[i].q, P(ZERO),L(ZERO);
				rdensity *= scale;
				if (m_parts[i].scale!=1.0f)
					ptloc /= m_parts[i].scale;
				if (!is_unused(action->impulse))
					P = ((action->impulse*rdensity)*m_qrot)*m_parts[i].q;
				if (!is_unused(action->angImpulse))
					L = ((action->angImpulse*rdensity)*m_qrot)*m_parts[i].q;
				if (m_parts[i].pLattice->AddImpulse(ptloc, P,L, m_pWorld->m_vars.gravity*(m_pWorld->m_vars.jointGravityStep*m_pWorld->m_updateTimes[0]),
						m_pWorld->m_updateTimes[0]) && !(m_flags & pef_deforming)) 
				{	m_updStage = 0; m_pWorld->MarkEntityAsDeforming(this);
				}
			}
		}

		return 1;
	}

	if (_action->type==pe_action_awake::type_id && m_iSimClass>=0 && m_iSimClass<7) {
		Awake(((pe_action_awake*)_action)->bAwake,1);	
		return 1;
	}
	if (_action->type==pe_action_remove_all_parts::type_id) {
		while(m_nParts)
			RemoveGeometry(m_parts[m_nParts-1].id);
		return 1;
	}

	if (_action->type==pe_action_reset_part_mtx::type_id) {
		pe_action_reset_part_mtx *action = (pe_action_reset_part_mtx*)_action;
		int i=0;
		if (!is_unused(action->partid))
			for(i=m_nParts-1;i>0 && m_parts[i].id!=action->partid;i--);
		else if (!is_unused(action->ipart))
			i = action->ipart;

		pe_params_pos pp;
		pp.pos = m_pos+m_qrot*m_parts[i].pos;
		pp.q = m_qrot*m_parts[i].q;
		pp.bRecalcBounds = 2;
		SetParams(&pp,1);

		pe_params_part ppt;
		ppt.ipart = i;
		ppt.pos.zero();
		ppt.q.SetIdentity();
		SetParams(&ppt,1);
		return 1;
	}

	if (_action->type==pe_action_auto_part_detachment::type_id) {
		pe_action_auto_part_detachment *action = (pe_action_auto_part_detachment*)_action;
		if (!is_unused(action->threshold)) {
			pe_params_structural_joint psj;
			pe_params_buoyancy pb;
			Vec3 gravity;
			float glen;
			//if (!m_pWorld->CheckAreas(this,gravity,&pb) || is_unused(gravity))
				gravity = m_pWorld->m_vars.gravity;	
			glen = gravity.len();
			psj.partid[1]=-1; psj.pt.zero(); psj.n=gravity/(glen>0 ? -glen:1.0f);
			psj.maxTorqueBend=psj.maxTorqueTwist = 1E20f;
			for(int i=0;i<m_nParts;i++) {
				psj.partid[0] = m_parts[i].id;
				psj.maxForcePush=psj.maxForcePull=psj.maxForceShift = m_parts[i].mass*glen*action->threshold;
				SetParams(&psj);
			}
			m_flags |= aef_recorded_physics;
		}
		if (m_pStructure && m_flags & aef_recorded_physics)
			m_pStructure->autoDetachmentDist = action->autoDetachmentDist;
		return 1;
	}

	if (_action->type==pe_action_move_parts::type_id) {
		pe_action_move_parts *action = (pe_action_move_parts*)_action;
		CPhysicalEntity *pTarget = (CPhysicalEntity*)action->pTarget;
		int i,j,iop;
		pe_geomparams gp;
		pe_params_structural_joint psj;
		Matrix34 mtxPart; gp.pMtx3x4=&mtxPart;
		if (pTarget) for(i=m_nParts-1;i>=0;i--) 
			if (m_parts[i].id>=action->idStart && m_parts[i].id<=action->idEnd) {
				int idnew = action->MapPartId ? action->MapPartId(m_parts[i].id,action->auxData) : m_parts[i].id+action->idOffset;
				if (idnew<0)
					continue;
				gp.mass = m_parts[i].mass*cube(action->mtxRel.GetColumn0().len()/m_parts[i].scale); 
				gp.flags=m_parts[i].flags; gp.flagsCollider=m_parts[i].flagsCollider;
				mtxPart = action->mtxRel*Matrix34(Vec3(1), m_parts[i].q, m_parts[i].pos);
				gp.pLattice=m_parts[i].pLattice; gp.idmatBreakable=m_parts[i].idmatBreakable;
				gp.pMatMapping=m_parts[i].pMatMapping; gp.nMats=m_parts[i].nMats;
				pTarget->AddGeometry(m_parts[i].pPhysGeom, &gp, idnew, 1);

				if (m_pStructure) for(j=0;j<m_pStructure->nJoints;j++) 
					if ((iop=m_pStructure->pJoints[j].ipart[1]==i) || m_pStructure->pJoints[j].ipart[0]==i)	{
						psj.id = m_pStructure->pJoints[j].id;
						psj.partid[iop] = m_parts[i].id+action->idOffset;
						psj.pt = action->mtxRel*m_pStructure->pJoints[j].pt;
						psj.n = m_pStructure->pJoints[j].n;
						int ibuddy = m_pStructure->pJoints[j].ipart[iop^1]; 
						if (ibuddy>=0 && m_parts[ibuddy].id>=action->idStart && m_parts[ibuddy].id<=action->idEnd) {
							if (ibuddy<i)
								continue;
							psj.partid[iop^1] = m_parts[ibuddy].id+action->idOffset;
						} else {
							int ipart = pTarget->TouchesSphere(pTarget->m_qrot*psj.pt+pTarget->m_pos, m_pStructure->pJoints[i].size*0.5f);
							if (ipart<pTarget->m_nParts-1)
								psj.partid[iop^1] = pTarget->m_parts[ipart].id;
							else
								psj.partid[iop^1] = -1;
						}
						psj.axisx = m_pStructure->pJoints[j].axisx;
						psj.maxForcePush = m_pStructure->pJoints[j].maxForcePush;
						psj.maxForcePull = m_pStructure->pJoints[j].maxForcePull;
						psj.maxForceShift = m_pStructure->pJoints[j].maxForceShift;
						psj.maxTorqueBend = m_pStructure->pJoints[j].maxTorqueBend;
						psj.maxTorqueTwist = m_pStructure->pJoints[j].maxTorqueTwist;
						psj.limitConstraint = m_pStructure->pJoints[j].limitConstr;
						psj.dampingConstraint = m_pStructure->pJoints[j].dampingConstr;
						psj.bBreakable = m_pStructure->pJoints[j].bBreakable & 1;
						psj.bConstraintWillIgnoreCollisions = m_pStructure->pJoints[j].bBreakable>>1^1;
						psj.bDirectBreaksOnly = m_pStructure->pJoints[j].bBreakable>>2;
						psj.szSensor = m_pStructure->pJoints[j].size;
						pTarget->SetParams(&psj);
					}
			}
		for(i=m_nParts-1;i>=0;i--) if (m_parts[i].id>=action->idStart && m_parts[i].id<=action->idEnd)
			if (!action->MapPartId || action->MapPartId(m_parts[i].id,action->auxData)>=0)
				RemoveGeometry(m_parts[i].id);
		return 1;
	}

	if (_action->type==pe_action_add_constraint::type_id && m_iSimClass==SC_TRIGGER && ((pe_action_add_constraint*)_action)->pBuddy) {
		m_pEntBuddy = (CPhysicalPlaceholder*)((pe_action_add_constraint*)_action)->pBuddy;
		m_pEntBuddy->m_iForeignFlags = (m_iForeignFlags = ent_rigid|ent_sleeping_rigid|ent_living|ent_independent | TRIGGER_PORTAL) | TRIGGER_PORTAL_INV;
		m_pEntBuddy->m_pEntBuddy = this;
		return 1;
	}
	
	return 0;
}

float CPhysicalEntity::GetExtent(EGeomForm eForm) const
{
	CGeomExtent& ext = m_Extents.Make(eForm);
	if (!ext) {
		// iterate parts, caching extents
		ext.ReserveParts(m_nParts);
		for(int i=0;i<m_nParts;i++) {
			geom const& part = m_parts[i];
			if ((part.flags & geom_collides) && part.pPhysGeom && part.pPhysGeom->pGeom)
				ext.AddPart( part.pPhysGeom->pGeom->GetExtent(eForm) * ScaleExtent(eForm, part.scale) );
			else
				ext.AddPart(0.f);
		}
	}
	return ext.TotalExtent();
}

void CPhysicalEntity::GetRandomPoints(Array<PosNorm> points, CRndGen& seed, EGeomForm eForm) const
{
	// choose sub-part, get random pos, transform to world
	const CGeomExtent& ext = m_Extents[eForm];
	for (auto subPoints : ext.RandomPartsAliasSum(points, seed)) {
		if (subPoints.iPart >= 0 && subPoints.iPart < m_nParts) {
			geom const& part = m_parts[subPoints.iPart];
			part.pPhysGeom->pGeom->GetRandomPoints(subPoints.aPoints, seed, eForm);
			QuatTS qts(m_qrot * part.q, m_qrot * part.pos + m_pos, part.scale);
			for (auto& ran : subPoints.aPoints)
				ran <<= qts;
		}
	}
}

bool CPhysicalEntity::OccupiesEntityGridSquare(const AABB &bbox)
{
	if (m_pWorld->m_vars.bEntGridUseOBB) {
		const Vec3 *BBox = (const Vec3*)&bbox;
		int i,j;
		if (m_nParts==0)
			return PtInAABB(BBox,m_pos);
		else for(i=0;i<m_nParts;i++) {
			Matrix33 R(m_qrot*m_parts[i].q);
			IGeometry *pGeom[3];
			COverlapChecker Overlapper;	Overlapper.Init();
			pGeom[0] = m_parts[i].pPhysGeomProxy->pGeom;
			pGeom[1]=pGeom[2] = m_parts[i].pPhysGeom->pGeom; 
			j=0; do {
				box abox,bbox;
				bbox.center=(BBox[0]+BBox[1])*0.5f; bbox.size=(BBox[1]-BBox[0])*0.5f;
				bbox.Basis.SetIdentity(); bbox.bOriented=0;
				pGeom[j]->GetBBox(&abox);
				abox.center = m_qrot*(m_parts[i].q*abox.center*m_parts[i].scale+m_parts[i].pos)+m_pos;
				abox.size *= m_parts[i].scale;
				abox.Basis = abox.Basis*Matrix33(!m_parts[i].q*!m_qrot);
				abox.bOriented = 1;
				if (box_box_overlap_check(&abox,&bbox,&Overlapper))
					return true;
				j++;
			} while(pGeom[j]!=pGeom[j-1]);
		}
		return false;
	}
	return true;
}

void CPhysicalEntity::UpdatePartIdmatBreakable(int ipart, int nParts)
{
	int i,id,flags=0;		
	m_parts[ipart].idmatBreakable = -1;
	m_parts[ipart].flags &= ~geom_manually_breakable;
	nParts += m_nParts+1 & nParts>>31;

	if (m_parts[ipart].pMatMapping) {
		for(i=0;i<(int)m_parts[ipart].nMats;i++) {
			id = m_pWorld->m_SurfaceFlagsTable[min(NSURFACETYPES-1,max(0,m_parts[ipart].pMatMapping[i]))];
			m_parts[ipart].idmatBreakable = max(m_parts[ipart].idmatBreakable, (id>>sf_matbreakable_bit)-1);
			flags |= id;
		}
		if ((m_parts[ipart].idmatBreakable>=0 || flags & sf_manually_breakable) && nParts>1) {
			m_parts[ipart].idmatBreakable = -1; flags = 0;
			for(i=m_parts[ipart].pPhysGeom->pGeom->GetPrimitiveCount()-1; i>=0; i--) {
				id = m_parts[ipart].pPhysGeom->pGeom->GetPrimitiveId(i,0x40);
				id += m_parts[ipart].surface_idx+1 & id>>31;
				id = m_pWorld->m_SurfaceFlagsTable[m_parts[ipart].pMatMapping[min((int)m_parts[ipart].nMats-1,id)]];
				m_parts[ipart].idmatBreakable = max(m_parts[ipart].idmatBreakable, (id>>sf_matbreakable_bit)-1);
				flags |= id;
			}
		}
	}	else for(i=m_parts[ipart].pPhysGeom->pGeom->GetPrimitiveCount()-1; i>=0; i--) {
		id = m_parts[ipart].pPhysGeom->pGeom->GetPrimitiveId(i,0x40);
		id += m_parts[ipart].surface_idx+1 & id>>31;
		id = m_pWorld->m_SurfaceFlagsTable[id];
		m_parts[ipart].idmatBreakable = max(m_parts[ipart].idmatBreakable, (id>>sf_matbreakable_bit)-1);
		flags |= id;
	}
	if (m_parts[ipart].idmatBreakable>=0 && m_parts[ipart].pPhysGeom->pGeom->GetType()!=GEOM_TRIMESH)
		m_parts[ipart].idmatBreakable=-1, flags &= ~sf_manually_breakable;

	if (flags & sf_manually_breakable)
		m_parts[ipart].flags |= geom_manually_breakable;
}


int CPhysicalEntity::AddGeometry(phys_geometry *pgeom, pe_geomparams* params, int id, int bThreadSafe)
{
	if (!pgeom || !pgeom->pGeom)
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

	if (id>=0 && (!(params->flagsCollider & geom_allow_id_duplicates) || params->flags & geom_proxy)) {
		int i; for(i=0;i<m_nParts && m_parts[i].id!=id;i++);
		if (i<m_nParts) {
			if (params->flags & geom_proxy) {
				if (pgeom) {
					m_pWorld->GetGeomManager()->AddRefGeometry(m_parts[i].pPhysGeomProxy = pgeom);
					if (params->bRecalcBBox) {
						Vec3 BBox[2];	ComputeBBox(BBox);
						int bPosChanged = m_pWorld->RepositionEntity(this,1,BBox);
						{ WriteLock lock(m_lockUpdate);
							WriteBBox(BBox); 
							m_pWorld->UnlockGrid(this,-bPosChanged);
						}
						RepositionParts();
					}
				}
			}
			return id;
		}
	}
	get_xqs_from_matrices(params->pMtx3x4,params->pMtx3x3, params->pos,params->q,params->scale, &pgeom,m_pWorld->GetGeomManager());
	ENTITY_VALIDATE_ERRCODE("CPhysicalEntity:AddGeometry",params,-1);

	box abox; pgeom->pGeom->GetBBox(&abox);
	float mindim=min(min(abox.size.x,abox.size.y),abox.size.z), mindims=mindim*params->scale;
	float maxdim=max(max(abox.size.x,abox.size.y),abox.size.z), maxdims=maxdim*params->scale;

	if (m_nParts==m_nPartsAlloc) {
		WriteLock lock(m_lockUpdate);
		int nPartsAlloc = m_nPartsAlloc;
		int nPartsAllocNew = (nPartsAlloc<2)?nPartsAlloc+1:(nPartsAlloc+4 & ~3); // 0 -> 1 -> 2 -> 4 -> next boundary. 2 is very common in vegetation
		geom *pparts = m_parts;
		geom *ppartsNew = m_pHeap 
			? (geom*)m_pHeap->Malloc(sizeof(geom) * nPartsAllocNew, "Parts")
			: m_pWorld->AllocEntityParts(nPartsAllocNew);
		memcpy(ppartsNew, pparts, sizeof(geom)*m_nParts);
		for(int i=0; i<m_nParts; i++) if (m_parts[i].pNewCoords==(coord_block_BBox*)&pparts[i].pos)
			ppartsNew[i].pNewCoords = (coord_block_BBox*)&ppartsNew[i].pos;
		m_parts = ppartsNew; m_nPartsAlloc = nPartsAllocNew;
		if (m_pHeap)
			m_pHeap->Free(pparts);
		else
			m_pWorld->FreeEntityParts(pparts, nPartsAlloc);
		if (m_nPartsAlloc!=1) { MEMSTAT_USAGE(m_parts, sizeof(geom) * m_nParts); }
	}
	{ WriteLock lock(m_lockPartIdx);
		if (id<0)
			id = m_iLastIdx++;
		else
			m_iLastIdx = max(m_iLastIdx,id+1);
	}
	m_parts[m_nParts].id = id;
	m_parts[m_nParts].pPhysGeom = m_parts[m_nParts].pPhysGeomProxy = pgeom;
	for(int j=0;j<2;j++) m_pWorld->GetGeomManager()->AddRefGeometry(pgeom);
	m_parts[m_nParts].surface_idx = pgeom->surface_idx;
	if (!is_unused(params->surface_idx)) m_parts[m_nParts].surface_idx = params->surface_idx;
	m_parts[m_nParts].flags = params->flags & ~geom_proxy;
	m_parts[m_nParts].flagsCollider = params->flagsCollider & geom_allow_id_duplicates-1;
	m_parts[m_nParts].pos = params->pos;
	m_parts[m_nParts].q = params->q;
	m_parts[m_nParts].scale = params->scale;
	if (is_unused(params->minContactDist)) {
		m_parts[m_nParts].minContactDist = max(maxdims*0.03f,mindims*(mindims<maxdims*0.3f ? 0.4f:0.1f));
	} else
		m_parts[m_nParts].minContactDist = params->minContactDist;
	m_parts[m_nParts].maxdim = maxdim;
	m_parts[m_nParts].pNewCoords = (coord_block_BBox*)&m_parts[m_nParts].pos;
	if (pgeom->pGeom->GetType()==GEOM_TRIMESH && params->pLattice)
		(m_parts[m_nParts].pLattice = (CTetrLattice*)params->pLattice)->SetMesh((CTriMesh*)pgeom->pGeom);
	else m_parts[m_nParts].pLattice = 0;
	if (params->pMatMapping)
		memcpy(m_parts[m_nParts].pMatMapping = new int[params->nMats], params->pMatMapping, (m_parts[m_nParts].nMats=params->nMats)*sizeof(int));
	else {
		m_parts[m_nParts].pMatMapping = pgeom->pMatMapping;
		m_parts[m_nParts].nMats = pgeom->nMats;
	}
	if (!is_unused(params->idmatBreakable))
		m_parts[m_nParts].idmatBreakable = params->idmatBreakable;
	else if (m_parts[m_nParts].flags & geom_colltype_solid) {
		UpdatePartIdmatBreakable(m_nParts,m_nParts+1);
		if (m_nParts==1)
			UpdatePartIdmatBreakable(0,2);
	} else
		m_parts[m_nParts].idmatBreakable = -1;
	if (m_parts[m_nParts].idmatBreakable>=0 && m_parts[m_nParts].pPhysGeom->pGeom->GetType()==GEOM_TRIMESH) {
		CTriMesh *pMesh = (CTriMesh*)m_parts[m_nParts].pPhysGeom->pGeom;
		if (pMesh->m_pTree->GetMaxSkipDim()>0.05f) {
			int bNonLockable = isneg(pMesh->m_lockUpdate);
			pMesh->Lock();
            pMesh->RebuildBVTree();
            //pMesh->m_lockUpdate;
            pMesh->Unlock();
			pMesh->m_lockUpdate -= bNonLockable;
		}
	}
	m_parts[m_nParts].mass = params->mass>0 ? params->mass : params->density*pgeom->V*cube(params->scale);
	if (m_pStructure || m_parts[m_nParts].pLattice)
		m_parts[m_nParts].flags |= geom_monitor_contacts;
	if (m_pStructure)
		if (m_pStructure->nPartsAlloc<m_nParts+1) {
			ReallocateList(m_pStructure->pParts,m_nParts,m_nParts+1,true);
			if (m_pStructure->defparts)
				ReallocateList(m_pStructure->defparts,m_nParts,m_nParts+1,true);
			m_pStructure->nPartsAlloc = m_nParts+1;
		} else {
			memset(m_pStructure->pParts+m_nParts, 0, sizeof(m_pStructure->pParts[0]));
			if (m_pStructure->defparts)
				memset(m_pStructure->defparts+m_nParts, 0, sizeof(m_pStructure->defparts[0]));
		}
	m_parts[m_nParts].BBox[0].zero(); m_parts[m_nParts].BBox[1].zero();
	m_parts[m_nParts].pPlaceholder = 0;

	if (params->bRecalcBBox) {
		Vec3 BBox[2];
		ComputeBBox(BBox, update_part_bboxes|part_added);
		int bPosChanged = m_pWorld->RepositionEntity(this,1,BBox);
		{ WriteLock lock(m_lockUpdate);
			WriteBBox(BBox); m_nParts++;
			m_pWorld->UnlockGrid(this,-bPosChanged);
		}
		RepositionParts();
	}	else { 
		WriteLock lock(m_lockUpdate);
		m_nParts++;
	}

	if (m_nPartsAlloc!=1) { MEMSTAT_USAGE(m_parts, sizeof(geom) * m_nParts); }

	m_Extents.Clear();

	return id;
}


void CPhysicalEntity::RemoveGeometry(int id, int bThreadSafe)
{
	ChangeRequest<void> req(this,m_pWorld,0,bThreadSafe,0,id);
	if (req.IsQueued())
		return;
	int i,j,n=0;

	for(i=m_nParts-1;i>=0;i--) if (m_parts[i].id==id) {
		if (m_nRefCount && m_iSimClass==0 && m_pWorld->m_vars.lastTimeStep>0.0f) {
			CPhysicalEntity **pentlist;
			Vec3 inflator = Vec3(10.0f)*m_pWorld->m_vars.maxContactGap;
			for(j=m_pWorld->GetEntitiesAround(m_BBox[0]-inflator,m_BBox[1]+inflator, pentlist, ent_sleeping_rigid|ent_living|ent_independent)-1; j>=0; j--)
				if (m_iSimClass || pentlist[j]->HasPartContactsWith(this,i,1))
					pentlist[j]->Awake();
		}
		if (m_pStructure) {
			for(j=0;j<m_pStructure->nJoints;j++) 
				if (m_pStructure->pJoints[j].ipart[0]==i || m_pStructure->pJoints[j].ipart[1]==i) {
					m_pStructure->pJoints[j] = m_pStructure->pJoints[--m_pStructure->nJoints]; --j;
					m_pStructure->bModified++;
				}
			for(j=0;j<m_pStructure->nJoints;j++) {
				m_pStructure->pJoints[j].ipart[0] -= isneg(i-m_pStructure->pJoints[j].ipart[0]);
				m_pStructure->pJoints[j].ipart[1] -= isneg(i-m_pStructure->pJoints[j].ipart[1]);
			}
			if (m_pStructure->pParts) {
				memmove(m_pStructure->pParts+i, m_pStructure->pParts+i+1, (m_nParts-1-i)*sizeof(SPartInfo));
				for(j=0;j<m_nParts;j++)	{
					m_pStructure->pParts[j].iParent &= ~-iszero(i+1-m_pStructure->pParts[j].iParent);
					m_pStructure->pParts[j].iParent -= isneg(i-m_pStructure->pParts[j].iParent);
				}
			}
			if (m_pStructure->Pexpl) memmove(m_pStructure->Pexpl+i, m_pStructure->Pexpl+i+1, (m_nParts-1-i)*sizeof(Vec3));
			if (m_pStructure->Lexpl) memmove(m_pStructure->Lexpl+i, m_pStructure->Lexpl+i+1, (m_nParts-1-i)*sizeof(Vec3));
			if (m_pStructure->defparts) {
				if (m_pStructure->defparts[i].pSkelEnt) {
					delete[] m_pStructure->defparts[i].pSkinInfo;
					m_pWorld->DestroyPhysicalEntity(m_pStructure->defparts[i].pSkelEnt,0,1);
				}
				memmove(m_pStructure->defparts+i, m_pStructure->defparts+i+1, (m_nParts-1-i)*sizeof(SSkelInfo));
			}
		}
		CPhysicalPlaceholder *ppc=0;
		{ WriteLockCond lock(m_lockUpdate,m_pWorld->m_vars.bLogStructureChanges);
			if (m_parts[i].pMatMapping && m_parts[i].pMatMapping!=m_parts[i].pPhysGeom->pMatMapping) 
				delete[] m_parts[i].pMatMapping;
			m_pWorld->GetGeomManager()->UnregisterGeometry(m_parts[i].pPhysGeom);
			m_pWorld->GetGeomManager()->UnregisterGeometry(m_parts[i].pPhysGeomProxy);
			if (m_parts[i].pPlaceholder)
				ppc = ReleasePartPlaceholder(i);
			for(;i<m_nParts-1;i++) {
				m_parts[i] = m_parts[i+1];
				if (m_parts[i].pNewCoords==(coord_block_BBox*)&m_parts[i+1].pos)
					m_parts[i].pNewCoords = (coord_block_BBox*)&m_parts[i].pos;
				if (m_parts[i].pPlaceholder)
					m_parts[i].pPlaceholder->m_id = -2-i;
			}
			m_nParts--;
			if (m_nPartsAlloc!=1) { MEMSTAT_USAGE(m_parts, sizeof(geom) * m_nParts); }
			for(m_iLastIdx=i=0;i<m_nParts;i++)
				m_iLastIdx = max(m_iLastIdx, m_parts[i].id+1);
		}
		if (ppc)
			m_pWorld->DestroyPhysicalEntity(ppc,0,1);
		n++;
	}
	if (n) {
		// keep the original bounding box while RepositionEntity emits the signal and substitute at the end
		Vec3 newBBox[2];
		ComputeBBox(newBBox);
		m_pWorld->RepositionEntity(this,1|8,newBBox);
		m_BBox[0] = newBBox[0];
		m_BBox[1] = newBBox[1];
		RepositionParts();
	}
}


RigidBody *CPhysicalEntity::GetRigidBody(int ipart,int bWillModify)
{
	g_StaticRigidBody.P.zero(); g_StaticRigidBody.v.zero();
	g_StaticRigidBody.L.zero(); g_StaticRigidBody.w.zero();
	return &g_StaticRigidBody;
}


int CPhysicalEntity::IsPointInside(Vec3 pt) const
{
	pt = (pt-m_pos)*m_qrot;
	ReadLock lock(m_lockUpdate);
	for(int i=0;i<m_nParts;i++) if ((m_parts[i].flags & geom_collides) && 
			m_parts[i].pPhysGeom->pGeom->PointInsideStatus(((pt-m_parts[i].pos)*m_parts[i].q)/m_parts[i].scale)) 
		return 1;
	return 0;
}	


void CPhysicalEntity::AlertNeighbourhoodND(int mode)
{
	if (m_pOuterEntity && !m_pWorld->m_bMassDestruction) {
		m_pOuterEntity->Release();
		m_pOuterEntity = 0;
	}

	int i;
	if (m_iSimClass>3)
		return;

	for(i=0;i<m_nColliders;i++)	if (m_pColliders[i]!=this) {
		m_pColliders[i]->RemoveColliderMono(this);
		m_pColliders[i]->Awake();
		m_pColliders[i]->Release();
	}
	m_nColliders = 0;

	if (!(m_flags & pef_disabled) && (m_nRefCount>0 && m_iSimClass<3 || m_flags&pef_always_notify_on_deletion)) {
		CPhysicalEntity **pentlist;
		Vec3 inflator = (m_BBox[1]-m_BBox[0])*1E-3f+Vec3(4,4,4)*m_pWorld->m_vars.maxContactGap;
		for(i=m_pWorld->GetEntitiesAround(m_BBox[0]-inflator,m_BBox[1]+inflator, pentlist, 
			ent_sleeping_rigid|ent_rigid|ent_living|ent_independent|ent_triggers)-1; i>=0; i--)
			if (pentlist[i]!=this)
			pentlist[i]->Awake();
	}

	if (m_pStructure && m_pStructure->defparts) {
		for(i=0;i<m_nParts;i++) if (m_pStructure->defparts[i].pSkinInfo)
			m_pWorld->DestroyPhysicalEntity(m_pStructure->defparts[i].pSkelEnt,mode,1);
		if (mode==0) {
			for(i=0;i<m_nParts;i++) if (m_pStructure->defparts[i].pSkinInfo)
				delete[] m_pStructure->defparts[i].pSkinInfo;
			delete[] m_pStructure->defparts;
			m_pStructure->defparts = 0;
		}
	}
}


int CPhysicalEntity::RemoveCollider(CPhysicalEntity *pCollider, bool bRemoveAlways)
{
	PREFAST_ASSUME(pCollider);
	if (m_pColliders && m_iSimClass!=0) {
		int i,islot; for(i=0;i<m_nColliders && m_pColliders[i]!=pCollider;i++);
		if (i<m_nColliders) {
			for(islot=i;i<m_nColliders-1;i++) m_pColliders[i] = m_pColliders[i+1];
			if (pCollider!=this)
				pCollider->Release();
			m_nColliders--; return islot;
		}
	}
	return -1;
}

int CPhysicalEntity::AddCollider(CPhysicalEntity *pCollider)
{
	PREFAST_ASSUME(pCollider);
	if (m_iSimClass==0)
		return 1;
	int i,j;
	for(i=0;i<m_nColliders && m_pColliders[i]!=pCollider;i++);
	if (i==m_nColliders) {
		if (m_nColliders==m_nCollidersAlloc)
			ReallocateList(m_pColliders, m_nColliders, m_nCollidersAlloc+=8, true);
		for(i=0;i<m_nColliders && pCollider->GetMassInv()>m_pColliders[i]->GetMassInv();i++);
		NO_BUFFER_OVERRUN
		for(j=m_nColliders-1; j>=i; j--) m_pColliders[j+1] = m_pColliders[j];
		m_pColliders[i] = pCollider; m_nColliders++;
		if (pCollider!=this)
			pCollider->AddRef();
	}
	return i;
}


void CPhysicalEntity::DrawHelperInformation(IPhysRenderer *pRenderer, int flags)
{	
	//ReadLock lock(m_lockUpdate);
	int i,j;
	geom_world_data gwd;

	if (flags & pe_helper_bbox) {
		Vec3 sz,center,pt[8];
		center = (m_BBox[1]+m_BBox[0])*0.5f;
		sz = (m_BBox[1]-m_BBox[0])*0.5f;
		for(i=0;i<8;i++)
			pt[i] = Vec3(sz.x*((i<<1&2)-1),sz.y*((i&2)-1),sz.z*((i>>1&2)-1))+center;
		for(i=0;i<8;i++) for(j=0;j<3;j++) if (i&1<<j)
			pRenderer->DrawLine(pt[i],pt[i^1<<j],m_iSimClass);
	}

	if (flags & (pe_helper_geometry|pe_helper_lattice)) {
		int iLevel = flags>>16 & 0xFFF, mask=0, bTransp=(m_pStructure && (flags & 16 || m_pWorld->m_vars.bLogLatticeTension)) | flags>>29 & 1;
		if (iLevel & 1<<11)
			mask=iLevel&0xF7FF,iLevel=0;
		for(i=0;i<m_nParts;i++) if ((m_parts[i].flags & mask)==mask && (m_parts[i].flags || m_parts[i].flagsCollider || m_parts[i].mass>0)) {
			gwd.R = Matrix33(m_qrot*m_parts[i].q);
			gwd.offset = m_pos + m_qrot*m_parts[i].pos;
			gwd.scale = m_parts[i].scale;
			if (flags & pe_helper_geometry) {
				if (m_parts[i].pPhysGeomProxy->pGeom && (m_parts[i].pPhysGeomProxy==m_parts[i].pPhysGeom || mask!=geom_colltype_ray)) 
					m_parts[i].pPhysGeomProxy->pGeom->DrawWireframe(pRenderer,&gwd, iLevel, m_iSimClass | (m_parts[i].flags&1^1|bTransp)<<8);
				if (m_parts[i].pPhysGeom->pGeom && m_parts[i].pPhysGeomProxy!=m_parts[i].pPhysGeom && (mask & geom_colltype_ray)==geom_colltype_ray)
					m_parts[i].pPhysGeom->pGeom->DrawWireframe(pRenderer,&gwd, iLevel, m_iSimClass | 1<<8);
			}
			if ((flags & pe_helper_lattice) && m_parts[i].pLattice)
				m_parts[i].pLattice->DrawWireframe(pRenderer,&gwd,m_iSimClass);
		}

		if (m_pWorld->m_vars.bLogLatticeTension && !m_pStructure && GetMassInv()>0)	{
			char txt[32];
			cry_sprintf(txt, "%.2fkg" ,1.0f/GetMassInv() );
			pRenderer->DrawText((m_BBox[0]+m_BBox[1])*0.5f, txt, 1);
		}
	}

	if ((m_pWorld->m_vars.bLogLatticeTension || flags & 16) && m_pStructure) {
		CTriMesh mesh;
		CBoxGeom boxGeom;
		box abox;
		Vec3 norms[4], vtx[]={Vec3(0,0.2f,0),Vec3(-0.1732f,-0.1f,0),Vec3(0.1732f,-0.1f,0),Vec3(0,0,1)};
		int ipart; index_t idx[]={0,2,1,0,1,3,1,2,3,2,0,3};
		trinfo top[4];
		box bbox;
		for(i=0;i<4;i++) norms[i] = (vtx[idx[i*3+1]]-vtx[idx[i*3]]^vtx[idx[i*3+2]]-vtx[idx[i*3]]).normalized();
		memset(top,-1,4*sizeof(top[0]));
		mesh.m_flags = mesh_shared_vtx|mesh_shared_idx|mesh_shared_normals|mesh_shared_topology|mesh_SingleBB;
		mesh.m_pVertices = strided_pointer<Vec3>(vtx);
		mesh.m_pIndices = idx;
		mesh.m_pNormals = norms;
		mesh.m_pTopology = top;
		mesh.m_nTris=mesh.m_nVertices = 4;
		mesh.m_ConvexityTolerance[0] = 0.02f; mesh.m_bConvex[0] = 1;
		mesh.RebuildBVTree();
		abox.Basis.SetIdentity(); abox.center.zero(); abox.size.Set(0.5f,0.5f,0.5f);
		boxGeom.CreateBox(&abox);
		for(i=0;i<m_pStructure->nJoints;i++) {
			gwd.offset = m_qrot*m_pStructure->pJoints[i].pt+m_pos;
			gwd.R = Matrix33(m_qrot*Quat::CreateRotationV0V1(Vec3(0,0,1),m_pStructure->pJoints[i].n));
			gwd.scale = m_pStructure->pJoints[i].size*2;
			j = 4+2*isneg(m_pStructure->pJoints[i].ipart[0]|m_pStructure->pJoints[i].ipart[1]);
			pRenderer->DrawGeometry(&mesh,&gwd,j);
			gwd.scale = m_pStructure->pJoints[i].size;
			pRenderer->DrawGeometry(&boxGeom,&gwd,j);
			for(j=0;j<2;j++) if ((unsigned int)(ipart=m_pStructure->pJoints[i].ipart[j])<(unsigned int)m_nParts) {
				m_parts[ipart].pPhysGeom->pGeom->GetBBox(&bbox);
				pRenderer->DrawLine(gwd.offset, m_qrot*(m_parts[ipart].q*bbox.center*m_parts[ipart].scale+m_parts[ipart].pos)+m_pos, m_iSimClass);
			}
		}
		if (m_pWorld->m_vars.bLogLatticeTension) {
			char txt[32];
			if (m_nParts != 0)
			{
				for(i=0;i<m_nParts;i++)	{
					m_parts[i].pPhysGeom->pGeom->GetBBox(&bbox);
					cry_sprintf(txt, "%.2fkg" ,m_parts[i].mass);
					pRenderer->DrawText(m_qrot*(m_parts[i].q*bbox.center*m_parts[i].scale+m_parts[i].pos)+m_pos, txt, 1);
				}
			}
			if (m_pStructure->nPrevJoints) {
				float rdt = 1.0f/m_pStructure->prevdt, tens,maxtens;
				for(i=0;i<m_pStructure->nPrevJoints;i++) if (m_pStructure->pJoints[i].tension>0) {
					tens = sqrt_tpl(m_pStructure->pJoints[i].tension)*rdt;
					switch (m_pStructure->pJoints[i].itens) {
						case 0: cry_sprintf(txt, "twist: %.1f/%.1f", tens,maxtens=m_pStructure->pJoints[i].maxTorqueTwist); break;
						case 1: cry_sprintf(txt, "bend: %.1f/%.1f", tens,maxtens=m_pStructure->pJoints[i].maxTorqueBend); break;
						case 2: cry_sprintf(txt, "push: %.1f/%.1f", tens,maxtens=m_pStructure->pJoints[i].maxForcePush); break;
						case 3: cry_sprintf(txt, "pull: %.1f/%.1f", tens,maxtens=m_pStructure->pJoints[i].maxForcePull); break;
						case 4: cry_sprintf(txt, "shift: %.1f/%.1f", tens,maxtens=m_pStructure->pJoints[i].maxForceShift); break;
						default: continue;
					}
					pRenderer->DrawText(m_qrot*m_pStructure->pJoints[i].pt+m_pos, txt, 0, tens/max(tens,maxtens));
				}
			}
		}
	}

	if (m_pWorld->m_vars.bDebugExplosions) {
		Vec3 imp;
		char str[128];
		box bbox;
		if (!m_pStructure || !m_pStructure->Pexpl) {
			if (m_pWorld->IsAffectedByExplosion(this,&imp)) {
				cry_sprintf(str, "%.1f", imp.len());
				pRenderer->DrawText(imp=(m_BBox[0]+m_BBox[1])*0.5f, str, 1,0.5f);
				pRenderer->DrawLine(m_pWorld->m_lastEpicenter, imp, 5);
			}
		} else for(i=0;i<m_nParts;i++) if (m_pStructure->Pexpl[i].len2()+m_pStructure->Lexpl[i].len2()>0) {
			cry_sprintf(str, "%.1f/%.1f", m_pStructure->Pexpl[i].len(),m_pStructure->Lexpl[i].len());
			m_parts[i].pPhysGeom->pGeom->GetBBox(&bbox);
			pRenderer->DrawText(imp=m_qrot*(m_parts[i].q*bbox.center*m_parts[i].scale+m_parts[i].pos)+m_pos, str, 1,0.5f);
			pRenderer->DrawLine(m_pWorld->m_lastEpicenter, imp, 5);
		}
	}

	if (m_pWorld->GetGrid(this) != &m_pWorld->m_entgrid)
		pRenderer->DrawLine(m_pos, m_pWorld->GetGrid(this)->origin, m_iSimClass);
}


void CPhysicalEntity::GetMemoryStatistics(ICrySizer *pSizer) const
{
	if (GetType()==PE_STATIC)
		pSizer->AddObject(this, sizeof(CPhysicalEntity));
	if (m_pWorld->m_vars.iDrawHelpers & 1<<31 && m_ig[0].x>-1)
		pSizer->AddObject(&m_iGThunk0, (m_ig[1].x-m_ig[0].x+1)*(m_ig[1].y-m_ig[0].y+1)*sizeof(pe_gridthunk));
	pSizer->AddObject(m_parts, m_nPartsAlloc*sizeof(m_parts[0]));
	for(int i=0;i<m_nParts;i++) {
		if (CPhysicalPlaceholder *ppc=m_parts[i].pPlaceholder) {
			//pSizer->AddObject(ppc, sizeof(CPhysicalPlaceholder));
			if (m_pWorld->m_vars.iDrawHelpers & 1<<31 && ppc->m_ig[0].x>-1)
				pSizer->AddObject(&ppc->m_iGThunk0, (ppc->m_ig[1].x-ppc->m_ig[0].x+1)*(ppc->m_ig[1].y-ppc->m_ig[0].y+1)*sizeof(pe_gridthunk));
		}
		int nMats = m_parts[i].pMatMapping && m_parts[i].pMatMapping!=m_parts[i].pPhysGeom->pMatMapping ? m_parts[i].nMats : 0;
		pSizer->AddObject(m_parts[i].pMatMapping, nMats*sizeof(int), nMats);
	}
	if(m_pColliders)
		pSizer->AddObject(m_pColliders, m_nCollidersAlloc*sizeof(m_pColliders[0]));
	if (m_pStructure) {
		pSizer->AddObject(m_pStructure, sizeof(*m_pStructure));
		pSizer->AddObject(m_pStructure->pParts, sizeof(m_pStructure->pParts[0])*m_nParts);
		if (m_pStructure->Pexpl) {
			pSizer->AddObject(m_pStructure->Pexpl, sizeof(m_pStructure->Pexpl[0])*m_nParts);
			pSizer->AddObject(m_pStructure->Lexpl, sizeof(m_pStructure->Lexpl[0])*m_nParts);
		}
		pSizer->AddObject(m_pStructure->pJoints, sizeof(m_pStructure->pJoints[0])*m_pStructure->nJointsAlloc);
		if (m_pStructure->defparts) {
			pSizer->AddObject(m_pStructure->defparts, sizeof(m_pStructure->defparts[0])*m_nParts);
			for(int i=0;i<m_nParts;i++) if (m_pStructure->defparts[i].pSkelEnt) {
				//m_pStructure->defparts[i].pSkelEnt->GetMemoryStatistics(pSizer); // already taken into account in global entity lists
				pSizer->AddObject(m_pStructure->defparts[i].pSkinInfo, sizeof(SSkinInfo)*(
					((CGeometry*)m_parts[i].pPhysGeom->pGeom)->IsAPrimitive() ? 1 : ((mesh_data*)m_parts[i].pPhysGeom->pGeom->GetData())->nVertices));
			}
		}
	}
	pSizer->AddObject(m_ground, m_nGroundPlanes*sizeof(m_ground[0]));
	if (m_pUsedParts)
		pSizer->AddObject(m_pUsedParts, (MAX_PHYS_THREADS+1)*sizeof(m_pUsedParts[0]));
}

struct SMemSerializer : ISerialize {
	SMemSerializer(int size0=256) { buf=new char[size=size0]; pos=0; reading=false; }
	SMemSerializer(char *buf0, bool read=true) { buf=buf0; size=-1; pos=0; reading=read; }
	virtual ~SMemSerializer() { if (size>0) delete[] buf; }
	virtual void ReadStringValue(const char * name, SSerializeString &curValue, uint32 policy=0) {}
	virtual void WriteStringValue(const char * name, SSerializeString& buffer, uint32 policy=0) {}
	virtual void Update( ISerializeUpdateFunction * pUpdate ) {}
	virtual void FlagPartialRead() {}
	virtual void BeginGroup( const char * szName ) {}
	virtual bool BeginOptionalGroup(const char* szName, bool condition) { return condition; }
	virtual void EndGroup() {}
	virtual bool IsReading() const { return reading!=0; }
	virtual bool ShouldCommitValues() const { return true; }
	virtual ESerializationTarget GetSerializationTarget() const { return eST_SaveGame; }
	virtual bool Ok() const { return true; }
#define SERIALIZATION_TYPE(T) \
	virtual void Value( const char * name, T& x, uint32 policy=0 ) { \
		if (pos+sizeof(T)>(unsigned int)size) { ReallocateList(buf,size,size+256); size+=256; } \
		T *op[2]; op[reading]=(T*)(buf+pos); op[reading^1]=&x; *op[0]=*op[1]; pos+=sizeof(T); \
	}
#include <CryNetwork/SerializationTypes.h>
#undef SERIALIZATION_TYPE
#define SERIALIZATION_TYPE(T) virtual void ValueWithDefault( const char * name, T& x, const T& defaultValue ) { Value(name,x); }
#include <CryNetwork/SerializationTypes.h>
#undef SERIALIZATION_TYPE
	virtual void ValueWithDefault( const char * name, SSerializeString& x, const SSerializeString& defaultValue ) {}
	int reading;
	char* buf;
	int pos,size;
};

int CPhysicalEntity::GetStateSnapshotTxt(char *txtbuf,int szbuf, float time_back) 
{
	const int version = 0;
	SMemSerializer mser;
	if (txtbuf)	{
		for(int i=0,j=version,j10; i<4; i++,j=j10) 
			j10=j/10, txtbuf[3-i]=j-(j10*10)+'0';
		txtbuf += 4;
	}
	if (time_back<0) {
		if (txtbuf) *txtbuf++='~'; szbuf--;
		CPhysicalEntity::GetStateSnapshot(TSerialize(&mser));
	} else
		GetStateSnapshot(TSerialize(&mser));
	return (txtbuf ? bin2ascii((unsigned char*)mser.buf,mser.pos,(unsigned char*)txtbuf) : (mser.pos*4)/3+2)+4;
}

void CPhysicalEntity::SetStateFromSnapshotTxt(const char *txtbuf,int szin)
{
	int szbuf=szin, version=0;
	for(int i=0; i<4; i++)
		version = version*10 + *txtbuf++-'0';
	const unsigned char *txtbuf1 = (const unsigned char*)txtbuf+(*txtbuf=='~' ? 1:0);
	char *buf = new char[szbuf];
	SMemSerializer mser(buf);
	while (!ascii2bin(txtbuf1,szbuf,(unsigned char*)buf)) {
		delete[] buf; buf=mser.buf=new char[szbuf*=2];
	}
	if (*txtbuf=='~') 
		CPhysicalEntity::SetStateFromSnapshot(TSerialize(&mser));
	else
		SetStateFromSnapshot(TSerialize(&mser));
	delete[] buf;
}


inline void FillGeomParams(pe_geomparams &dst, geom &src)
{
	dst.flags = (src.flags|geom_can_modify)&~(geom_invalid|geom_removed|geom_constraint_on_break);
	dst.flagsCollider = src.flagsCollider;
	dst.mass = src.mass; 
	dst.pos = src.pos;
	dst.q = src.q;
	dst.scale = src.scale;
	dst.idmatBreakable = src.idmatBreakable;
	dst.minContactDist = src.minContactDist;
	dst.pMatMapping = src.pMatMapping!=src.pPhysGeom->pMatMapping ? src.pMatMapping:0;
	dst.nMats = src.nMats;
}


void CPhysicalEntity::RemoveBrokenParent(int i, int nIsles)
{
	EventPhysRevealEntityPart eprep;
	eprep.pEntity=this; eprep.pForeignData=m_pForeignData; eprep.iForeignData=m_iForeignData;
	if (m_pStructure->pParts[i].iParent)
		RemoveBrokenParent(m_pStructure->pParts[i].iParent-1, nIsles);
	if (nIsles)
		g_parts[i].isle = nIsles;
	else
		m_parts[i].flags |= geom_removed;
	for(int j=0;j<m_nParts;j++) if (m_pStructure->pParts[j].iParent-1==i)	{
		eprep.partId = m_parts[j].id;
		if (nIsles)
			m_pWorld->OnEvent(2,&eprep);
		m_parts[j].flags = m_pStructure->pParts[j].flags0;
		m_parts[j].flagsCollider = m_pStructure->pParts[j].flagsCollider0;
		m_pStructure->pParts[j].iParent = m_pStructure->pParts[i].iParent;
	}
}


int CPhysicalEntity::MapHitPointFromParent(int iParent, const Vec3 &pt)
{
	if (!m_pStructure || !m_pStructure->pParts)
		return iParent;

	int i,j,iprim,iClosest,iter=0;
	float dist,minDist,tol,maxDist,massCheck=0.0f;
	geom_world_data gwd;
	box bbox;
	CBoxGeom boxGeom;
	IGeometry *pGeom;
	Vec3 ptloc = (pt-m_pos)*m_qrot, ptres[2];
	Vec3 sz = m_BBox[1]-m_BBox[0];
	maxDist = max(max(sz.x,sz.y),sz.z);
	tol = sqr(max(m_pWorld->m_vars.maxContactGap*5.0f, maxDist*0.05f));

	do {
		for(i=0,minDist=maxDist*10.0f,iClosest=iParent; i<m_nParts; i++) if (m_parts[i].mass>massCheck)
			for(j=m_pStructure->pParts[i].iParent-1; j>=0; j=m_pStructure->pParts[j].iParent-1) if (j==iParent) {
				gwd.offset = m_parts[i].pos;
				gwd.R = Matrix33(m_parts[i].q);
				gwd.scale = m_parts[i].scale;
				if ((pGeom = m_parts[i].pPhysGeomProxy->pGeom)->GetPrimitiveCount()>1) {
					m_parts[i].pPhysGeomProxy->pGeom->GetBBox(&bbox);
					pGeom = boxGeom.CreateBox(&bbox);
				}
				pGeom->FindClosestPoint(&gwd, iprim,iprim, ptloc,ptloc, ptres);
				int bBest = isneg((dist=(ptres[0]-ptres[1]).len2())-minDist);
				minDist += (dist-minDist)*bBest;
				iClosest += i-iClosest & -bBest;
				break;
			}
		massCheck = -1e10f;
	} while (minDist>tol && ++iter<2);
	return iClosest;
}

int CPhysicalEntity::UpdateStructure(float time_interval, pe_explosion *pexpl, int iCaller, Vec3 gravity)
{
	int i,j,i0,i1,flags=0,nPlanes,ihead,itail,nIsles,iter,bBroken,nJoints,nJoints0,nMeshSplits=0,iLastIdx=m_iLastIdx;
	EventPhysUpdateMesh epum;
	EventPhysCreateEntityPart epcep;
	EventPhysRemoveEntityParts eprep;
	EventPhysJointBroken epjb;
	pe_geomparams gp,gpAux;
	pe_params_pos pp;
	pe_params_part ppt;
	pe_status_dynamics psd;
	CTriMesh *pMeshNew,*pMeshNext,*pMesh,*pChunkMeshesBuf[16],**pChunkMeshes=pChunkMeshesBuf;
	IGeometry *pMeshIsland,*pMeshIslandPrev;
	CTetrLattice *pChunkLatticesBuf[16],**pChunkLattices=pChunkLatticesBuf;
	CPhysicalEntity **pents;
	phys_geometry *pgeom,*pgeom0,*pgeom1;
	geom_world_data gwd,gwdAux;
	pe_action_impulse shockwave;
	pe_action_set_velocity asv;
	pe_explosion expl;
	plane ground[8];
	box bbox,bbox1;
	Vec3 org,sz,n,P,L,dw,dv,Ptang,Pmax,Lmax,BBoxNew[2];
	Matrix33 R;
	CSphereGeom sphGeom;
	//CBoxGeom boxGeom;
	intersection_params ip;
	geom_contact *pcontacts;
	bop_meshupdate *pmu=0;
	pe_params_buoyancy pb;
	pe_params_structural_joint psj;
	float t,Pn,vmax,M,V0,dt=time_interval,impTot;
	quotientf jtens;
	
	static int idRemoveGeoms[256];
	static phys_geometry *pRemoveGeoms[256],*pAddGeoms[64];
	static pe_geomparams gpAddGeoms[64];
	int nRemoveGeoms=0,nAddGeoms=0,bGeomOverflow=0;

	epum.pEntity=epcep.pEntity=eprep.pEntity=epjb.pEntity[0]=epjb.pEntity[1] = this; 
	epum.pForeignData=epcep.pForeignData=eprep.pForeignData=epjb.pForeignData[0]=epjb.pForeignData[1] = m_pForeignData; 
	epum.iForeignData=epcep.iForeignData=eprep.iForeignData=epjb.iForeignData[0]=epjb.iForeignData[1] = m_iForeignData;
	epum.iReason = EventPhysUpdateMesh::ReasonFracture;
	pp.pos = m_pos;
	pp.q = m_qrot;
	pp.pGridRefEnt = this;
	nPlanes = min(m_nGroundPlanes, (int)(CRY_ARRAY_COUNT(ground)));
	if (iCaller>=0 && (!m_pWorld->CheckAreas(this,gravity,&pb,1,Vec3(ZERO),iCaller) || is_unused(gravity)))
		gravity = m_pWorld->m_vars.gravity;	
	gravity *= dt*m_pWorld->m_vars.jointGravityStep;
	static volatile int g_lockUpdateStructure = 0;
	WriteLock lock(g_lockUpdateStructure);
	epcep.breakImpulse.zero(); epcep.breakAngImpulse.zero(); epcep.cutRadius=0; epcep.breakSize=0; 
	MARK_UNUSED epcep.cutPtLoc[0],epcep.cutPtLoc[1];
	epjb.bJoint=1; MARK_UNUSED epjb.pNewEntity[0],epjb.pNewEntity[1];
	if ((unsigned int)m_iSimClass>=7u)
		return 0;

	{ ReadLock lockUpd(m_lockUpdate);
		if (pexpl) {
			if (!m_pExpl) m_pExpl = new SExplosionInfo;
			m_pExpl->center = pexpl->epicenterImp;
			m_pExpl->dir = pexpl->explDir;
			m_pExpl->kr = pexpl->impulsivePressureAtR*sqr(pexpl->r);
			m_pExpl->rmin = pexpl->rmin;
			m_timeStructUpdate = m_pWorld->m_vars.tickBreakable;
			m_updStage = 1;	m_nUpdTicks = 0;
			return 1;
		}

		for(i=m_nParts-1;i>=0;i--) if (m_parts[i].idmatBreakable<=-2)	{
			for(j=0;j<i;j++) if (-2-m_parts[j].id==m_parts[i].idmatBreakable) {
				m_parts[j].flags |= m_parts[i].flags;
				m_parts[j].flagsCollider |= m_parts[i].flagsCollider;
			}
			m_pWorld->GetGeomManager()->UnregisterGeometry(m_parts[i].pPhysGeom);
			m_pWorld->GetGeomManager()->UnregisterGeometry(m_parts[i].pPhysGeomProxy);
			--m_nParts;
		}	else
			flags |= m_parts[i].flags & geom_structure_changes;

		if (m_nPartsAlloc!=1)	{ MEMSTAT_USAGE(m_parts, sizeof(m_parts[0]) * m_nParts); }

		PHYS_ENTITY_PROFILER

		if (m_pStructure && m_nParts>0 && m_pStructure->nJoints>0) {
			if (g_nPartsAlloc < m_nParts+2) {
				if (g_parts) delete[] (g_parts-1);
				g_parts = (new SPartHelper[g_nPartsAlloc = m_nParts+2])+1;
			}
			if (g_nJointsAlloc < m_pStructure->nJoints+1) {
				if (g_joints) delete[] g_joints;
				if (g_jointidx) delete[] g_jointidx;
				g_joints = new SStructuralJointHelper[g_nJointsAlloc = m_pStructure->nJoints+1];
				g_jointidx = new int[g_nJointsAlloc*2];
				if (g_jointsDbg) delete[] g_jointsDbg;
				g_jointsDbg = new SStructuralJointDebugHelper[g_nJointsAlloc];
			}
			nJoints0 = m_pStructure->nJoints;

			// prepare part infos
			for(i=0,M=0,g_parts[0].idx=-1; i<m_nParts; i++) {
				g_parts[i].org = m_parts[i].q*m_parts[i].pPhysGeom->origin + m_parts[i].pos;
				if (m_parts[i].mass>0) {
					g_parts[i].Minv = 1.0f/m_parts[i].mass;
					Diag33 invIbody(m_parts[i].pPhysGeom->Ibody*(m_parts[i].mass/m_parts[i].pPhysGeom->V)*sqr(m_parts[i].scale));
					g_parts[i].Iinv = Matrix33(m_parts[i].q)*invIbody.invert()*Matrix33(!m_parts[i].q);
				} else {
					g_parts[i].Minv=0; g_parts[i].Iinv.SetZero();
				}
				g_parts[i].bProcessed = 0; g_parts[i].ijoint0 = 0;
				M += m_parts[i].mass;
				g_parts[i].pent = this;
			}
			impTot=0; vmax=-1; Pmax.zero(); Lmax.zero();
			if (m_pWorld->m_vars.breakImpulseScale || m_flags & pef_override_impulse_scale) {
				for(i=0; i<m_nParts; i++) {
					P = m_pStructure->pParts[i].Pext*dt*100+m_pStructure->pParts[i].Fext;
					L = m_pStructure->pParts[i].Lext*dt*100+m_pStructure->pParts[i].Text;
					j = isneg(Pmax.len()+Lmax.len2()-P.len()-L.len2());
					Pmax = Pmax*(1-j)+P*j; Lmax = Lmax*(1-j)+L*j;
					g_parts[i].v = (m_pStructure->pParts[i].Pext*dt*100+m_pStructure->pParts[i].Fext)*g_parts[i].Minv;
					g_parts[i].w = g_parts[i].Iinv*(m_pStructure->pParts[i].Lext*dt*100+m_pStructure->pParts[i].Text);
					g_parts[i].v += gravity*iszero((int)m_flags & aef_recorded_physics);
					n = m_parts[i].BBox[1]-m_parts[i].BBox[0];
					if ((Pn=g_parts[i].v.len2()+g_parts[i].w.len2()*sqr(n.x+n.y+n.z)*0.01f) > vmax)
						vmax=Pn, g_parts[0].idx=i;
					impTot += m_pStructure->pParts[i].Pext.len2()+m_pStructure->pParts[i].Lext.len2()+
						m_pStructure->pParts[i].Fext.len2()+m_pStructure->pParts[i].Text.len2();
				}
			} else for(i=0; i<m_nParts; i++) {
				g_parts[i].v.zero();
				g_parts[i].w.zero();
			}

			g_parts[-1].org.zero();
			g_parts[-1].v.zero(); g_parts[-1].w.zero();
			g_parts[-1].Minv = 0; g_parts[-1].Iinv.SetZero();
			g_parts[-1].bProcessed = 0;
			g_parts[-1].ijoint0 = g_parts[m_nParts].ijoint0 = 0;
			g_parts[-1].pent = this;
			eprep.massOrg = M;
			if (M>0) M = 1.0f/M;
			if (m_pStructure->idpartBreakOrg>=0)
				for(g_parts[0].idx=m_nParts-1; g_parts[0].idx>=0 && m_parts[g_parts[0].idx].id!=m_pStructure->idpartBreakOrg; g_parts[0].idx--);
			epjb.partidEpicenter = g_parts[0].idx>=0 ? m_parts[g_parts[0].idx].id : -1;

			// for each part, build the list of related joints
			for(i=0;i<m_pStructure->nJoints;i++) {
				g_parts[m_pStructure->pJoints[i].ipart[0]].ijoint0++; 
				g_parts[m_pStructure->pJoints[i].ipart[1]].ijoint0++;
			}
			for(i=0;i<m_nParts;i++)
				g_parts[i].ijoint0 += g_parts[i-1].ijoint0;
			for(i=0;i<m_pStructure->nJoints;i++) {
				g_jointidx[--g_parts[m_pStructure->pJoints[i].ipart[0]].ijoint0] = i;
				g_jointidx[--g_parts[m_pStructure->pJoints[i].ipart[1]].ijoint0] = i;
			}
			g_parts[m_nParts].ijoint0 = m_pStructure->nJoints*2;

			// iterate parts around the fastest moving part breadth-wise, add connecting joints as we add next part to the queue
			ihead=nJoints=0; 
			if (g_parts[0].idx>=0) do {
				for(g_parts[g_parts[ihead].idx].bProcessed=1,itail=ihead+1; ihead<itail; ihead++) {
					for(i=g_parts[g_parts[ihead].idx].ijoint0,g_parts[g_parts[ihead].idx].bProcessed++; i<g_parts[g_parts[ihead].idx+1].ijoint0; i++) {
						j = m_pStructure->pJoints[g_jointidx[i]].ipart[0] + m_pStructure->pJoints[g_jointidx[i]].ipart[1] - g_parts[ihead].idx;
						g_joints[nJoints].idx = g_jointidx[i];
						nJoints += -((g_parts[j].bProcessed-2|j)>>31) & (m_pStructure->pJoints[g_jointidx[i]].bBreakable & 1);
						if (!g_parts[j].bProcessed) {
							g_parts[itail].idx = j;	itail += g_parts[j].Minv>0;
							g_parts[j].bProcessed = 1; 
							/*j += g_parts[ihead].idx;
							for(i1=i; i1<g_parts[g_parts[ihead].idx+1].ijoint0; i1++) {
								g_joints[nJoints].idx = g_jointidx[i1];
								nJoints += iszero(m_pStructure->pJoints[g_jointidx[i1]].ipart[0] + m_pStructure->pJoints[g_jointidx[i1]].ipart[1] - j) & 
													 (m_pStructure->pJoints[g_jointidx[i1]].bBreakable & 1);*/
						}
					} 
					g_parts[-1].bProcessed = 0;
				}
				if (ihead<m_nParts) {
					for(i=0; i<m_nParts && (g_parts[i].bProcessed || m_parts[i].mass*(m_pStructure->pParts[i].Pext.len2()+
							m_pStructure->pParts[i].Lext.len2()+m_pStructure->pParts[i].Fext.len2()+m_pStructure->pParts[i].Text.len2())==0); i++);
					if (i==m_nParts)
						break;
					g_parts[ihead].idx = i;
				}	else 
					break;
			}	while(true);

			// prepare joint infos
			for(i=0; i<nJoints; i++) {
				j = g_joints[i].idx; i0 = m_pStructure->pJoints[j].ipart[0]; i1 = m_pStructure->pJoints[j].ipart[1];
				g_joints[i].r0 = g_parts[i0].org - m_pStructure->pJoints[j].pt;
				g_joints[i].r1 = g_parts[i1].org - m_pStructure->pJoints[j].pt;
				t = g_parts[i0].Minv+g_parts[i1].Minv; g_joints[i].vKinv.SetZero(); 
				g_joints[i].vKinv(0,0)=t; g_joints[i].vKinv(1,1)=t; g_joints[i].vKinv(2,2)=t;
				crossproduct_matrix(g_joints[i].r0,R); g_joints[i].vKinv -= R*g_parts[i0].Iinv*R;
				crossproduct_matrix(g_joints[i].r1,R); g_joints[i].vKinv -= R*g_parts[i1].Iinv*R;
				g_joints[i].vKinv.Invert();
				(g_joints[i].wKinv = g_parts[i0].Iinv+g_parts[i1].Iinv).Invert();
				g_joints[i].P = m_pStructure->pJoints[j].Paccum;
				g_joints[i].L = m_pStructure->pJoints[j].Laccum;
				g_joints[i].bBroken = 0;
				g_jointsDbg[i].tension.set(-1,0);
			}

			// resolve joints, alternating order each iteration; track saturated joints
			iter=0; if (m_pStructure->idpartBreakOrg<0) do {
				ihead = nJoints-1 & -(iter&1); 
				itail = nJoints-(nJoints+1 & -(iter&1));
				for(i=ihead,bBroken=0; i!=itail; i+=1-(iter<<1&2)) {
					j = g_joints[i].idx; i0 = m_pStructure->pJoints[j].ipart[0]; i1 = m_pStructure->pJoints[j].ipart[1];
					P = g_joints[i].P; L = g_joints[i].L;	n = m_pStructure->pJoints[j].n;
					m_pStructure->pJoints[j].bBroken = 0;

					dw = g_parts[i0].w-g_parts[i1].w;
					g_joints[i].L -= g_joints[i].wKinv*dw;
					Pn = g_joints[i].L*n;
					if (m_pWorld->m_vars.bLogLatticeTension && jtens.set(Pn*Pn, sqr(m_pStructure->pJoints[j].maxTorqueTwist*dt))>=g_jointsDbg[i].tension)
						g_jointsDbg[i].tension=jtens, g_jointsDbg[i].itens=0;
					if (fabs_tpl(Pn) > m_pStructure->pJoints[j].maxTorqueTwist*dt) {
						g_joints[i].L -= n*(Pn+sgnnz(Pn)*m_pStructure->pJoints[j].maxTorqueTwist*dt); 
						g_joints[i].bBroken=m_pStructure->pJoints[j].bBroken = 1;
						m_pStructure->pJoints[j].itens = 0;
					}
					Ptang = g_joints[i].L-n*(Pn=n*g_joints[i].L);
					if (m_pWorld->m_vars.bLogLatticeTension && jtens.set(Ptang.len2(), sqr(m_pStructure->pJoints[j].maxTorqueBend*dt))>=g_jointsDbg[i].tension)
						g_jointsDbg[i].tension=jtens, g_jointsDbg[i].itens=1;
					if (Ptang.len2() > sqr(m_pStructure->pJoints[j].maxTorqueBend*dt)) {
						g_joints[i].L = Ptang.normalized()*(m_pStructure->pJoints[j].maxTorqueBend*dt) + n*Pn; 
						g_joints[i].bBroken=m_pStructure->pJoints[j].bBroken = 1;
						m_pStructure->pJoints[j].itens = 1;
					}
					L = g_joints[i].L-L;
					g_parts[i0].w += g_parts[i0].Iinv*L;
					g_parts[i1].w -= g_parts[i1].Iinv*L;

					dv = g_parts[i0].v+(g_parts[i0].w^g_joints[i].r0) - g_parts[i1].v-(g_parts[i1].w^g_joints[i].r1);
					g_joints[i].P -= g_joints[i].vKinv*dv; 
					Pn = g_joints[i].P*(n=m_pStructure->pJoints[j].n); 
					if (m_pWorld->m_vars.bLogLatticeTension)
						if (jtens.set(sqr_signed(Pn), sqr(m_pStructure->pJoints[j].maxForcePush*dt))>=g_jointsDbg[i].tension)
							g_jointsDbg[i].tension=jtens, g_jointsDbg[i].itens=2;
						else if (jtens.set(-sqr_signed(Pn), sqr(m_pStructure->pJoints[j].maxForcePull*dt))>=g_jointsDbg[i].tension)
							g_jointsDbg[i].tension=jtens, g_jointsDbg[i].itens=3;
					if (Pn > m_pStructure->pJoints[j].maxForcePush*dt) {
						g_joints[i].P -= n*(Pn-m_pStructure->pJoints[j].maxForcePush*dt); 
						g_joints[i].bBroken=m_pStructure->pJoints[j].bBroken = 1;
						m_pStructure->pJoints[j].itens = 2;
					} else if (Pn < -m_pStructure->pJoints[j].maxForcePull*dt) {
						g_joints[i].P -= n*(Pn+m_pStructure->pJoints[j].maxForcePull*dt);
						g_joints[i].bBroken=m_pStructure->pJoints[j].bBroken = 1;
						m_pStructure->pJoints[j].itens = 3;
					}
					Ptang = g_joints[i].P-n*(Pn=n*g_joints[i].P);
					if (m_pWorld->m_vars.bLogLatticeTension && jtens.set(Ptang.len2(), sqr(m_pStructure->pJoints[j].maxForceShift*dt))>=g_jointsDbg[i].tension)
						g_jointsDbg[i].tension=jtens, g_jointsDbg[i].itens=4;
					if (Ptang.len2() > sqr(m_pStructure->pJoints[j].maxForceShift*dt))	{
						g_joints[i].P = Ptang.normalized()*(m_pStructure->pJoints[j].maxForceShift*dt) + n*Pn; 
						g_joints[i].bBroken=m_pStructure->pJoints[j].bBroken = 1;
						m_pStructure->pJoints[j].itens = 4;
					}
					P = g_joints[i].P-P; 
					g_parts[i0].v += P*g_parts[i0].Minv; g_parts[i0].w += g_parts[i0].Iinv*(g_joints[i].r0^P);
					g_parts[i1].v -= P*g_parts[i1].Minv; g_parts[i1].w -= g_parts[i1].Iinv*(g_joints[i].r1^P);

					m_pStructure->pJoints[j].bBroken &= m_pStructure->bTestRun^1;
					bBroken += m_pStructure->pJoints[j].bBroken;
	#ifdef _DEBUG
					dv = g_parts[i0].v+(g_parts[i0].w^g_joints[i].r0) - g_parts[i1].v-(g_parts[i1].w^g_joints[i].r1);
					dw = g_parts[i0].w-g_parts[i1].w;
	#endif
				}
				++iter;
			} while((iter<3 || bBroken) && iter<23);

			for(i=0;i<nJoints;i++) for(j=0;j<2;j++) 
				if ((i0=m_pStructure->pJoints[g_joints[i].idx].ipart[j])>=0 && m_parts[i0].pLattice && m_parts[i0].idmatBreakable>=0) {
					t = m_parts[i0].pLattice->m_density*m_parts[i0].pPhysGeom->V*cube(m_parts[i0].scale)/m_parts[i0].mass;
					org = (m_pStructure->pJoints[g_joints[i].idx].pt-m_parts[i0].pos)*m_parts[i0].q;
					if (m_parts[i0].scale!=1.0f)
						org /= m_parts[i0].scale;
					m_parts[i0].pLattice->AddImpulse(org, (g_joints[i].P*m_parts[i0].q)*t, (g_joints[i].L*m_parts[i0].q)*t, 
						gravity,m_pWorld->m_updateTimes[0]);
				}

			for(i=0; i<nJoints; i++) if ((1-m_pStructure->bTestRun)*m_pStructure->pJoints[g_joints[i].idx].damageAccum>0.0f)
				if (m_pStructure->pJoints[g_joints[i].idx].damageAccumThresh<=0.0f)	for(i=0; i<nJoints; i++) {
					m_pStructure->pJoints[g_joints[i].idx].Paccum += g_joints[i].P*m_pStructure->pJoints[g_joints[i].idx].damageAccum;
					m_pStructure->pJoints[g_joints[i].idx].Laccum += g_joints[i].L*m_pStructure->pJoints[g_joints[i].idx].damageAccum;		
				}	else {
					n = m_pStructure->pJoints[j = g_joints[i].idx].n;
					Pn = g_joints[i].L*n;	Ptang = g_joints[i].L-n*Pn;
					jtens = max(quotientf(sqr(Pn), sqr(m_pStructure->pJoints[j].maxTorqueTwist*dt)),
											quotientf(Ptang.len2(), sqr(m_pStructure->pJoints[j].maxTorqueBend*dt)));
					Pn = g_joints[i].P*n;	Ptang = g_joints[i].P-n*Pn;
					jtens = max(jtens, quotientf(sqr_signed(Pn), sqr(m_pStructure->pJoints[j].maxForcePush*dt)));
					jtens = max(jtens, quotientf(-sqr_signed(Pn), sqr(m_pStructure->pJoints[j].maxForcePull*dt)));
					jtens = max(jtens, quotientf(Ptang.len2(), sqr(m_pStructure->pJoints[j].maxForceShift*dt)));
					if (jtens > sqr(m_pStructure->pJoints[j].damageAccumThresh)) {
						t = (1.0f-m_pStructure->bTestRun)*m_pStructure->pJoints[j].damageAccum;
            t *= (1.0f-m_pStructure->pJoints[j].damageAccumThresh*sqrt_tpl(jtens.y/jtens.x));
						m_pStructure->pJoints[j].Paccum += g_joints[i].P*t;
						m_pStructure->pJoints[j].Laccum += g_joints[i].L*t;
					}
				}

			// check connectivity among parts, assign island number to each part (iterate starting from the 'world' part)
			for(i=0;i<m_nParts;i++) g_parts[i].bProcessed = 0;
			g_parts[-1].bProcessed = 1;
			g_parts[g_parts[ihead=0].idx = g_parts[0].ijoint0>0 ? -1:0].bProcessed = 1; 
			nIsles=0; bBroken=0; iter=0;
			do { 
				for(itail=ihead+1; ihead<itail; ihead++) {
					g_parts[g_parts[ihead].idx].isle = nIsles & iter;
					for(i=g_parts[g_parts[ihead].idx].ijoint0; i<g_parts[g_parts[ihead].idx+1].ijoint0; i++) 
					if (!m_pStructure->pJoints[g_jointidx[i]].bBroken) {
						j = m_pStructure->pJoints[g_jointidx[i]].ipart[0] + m_pStructure->pJoints[g_jointidx[i]].ipart[1] - g_parts[ihead].idx;
						if (!g_parts[j].bProcessed) {
							g_parts[itail++].idx = j;	g_parts[j].bProcessed = 1;
						}
					} else
						bBroken -= nIsles-1>>31;
				} nIsles++;
				if (ihead>=m_nParts+1)
					break;
				for(i=0;i<m_nParts && (g_parts[i].bProcessed || g_parts[i].Minv>0);i++);
				if (i==m_nParts) {
					for(i=0;i<m_nParts && g_parts[i].bProcessed;i++);
					if (i>=m_nParts)
						break;
					iter = -1;
				}
				g_parts[g_parts[ihead].idx = i].bProcessed = 1;
			} while(true);

			if (!m_pStructure->bTestRun) for(i=0;i<m_nParts;i++) if (g_parts[i].isle && m_pStructure->pParts[i].iParent)
				RemoveBrokenParent(m_pStructure->pParts[i].iParent-1, nIsles);

			for(i=m_nParts-1,j=0; i>=0; i--) if (g_parts[i].isle!=0)
				if (nRemoveGeoms+j<CRY_ARRAY_COUNT(idRemoveGeoms))
					j++;
				else
					g_parts[i].isle=0;

			epcep.pMeshNew = 0;
			epcep.pLastUpdate = 0;
			epcep.bInvalid = 0;
			epcep.iReason = EventPhysCreateEntityPart::ReasonJointsBroken;
			pents = 0;
			nIsles &= ~-m_pStructure->bTestRun;

			if (m_pWorld->m_vars.breakImpulseScale || m_flags & pef_override_impulse_scale || !pexpl || 
					(pexpl && pexpl->forceDeformEntities)) for(i=1;i<nIsles;i++) 
			{
				for(j=0,epcep.nTotParts=0;j<m_nParts;j++) 
					epcep.nTotParts += iszero(g_parts[j].isle-i);
				if (!epcep.nTotParts)
					continue;
				epcep.pEntNew = m_pWorld->CreatePhysicalEntity(PE_RIGID,&pp,0,0x5AFE);
				asv.v.zero(); asv.w.zero(); t=0;
				int ipartSrc;

				for(j=i1=0,BBoxNew[0]=m_BBox[1],BBoxNew[1]=m_BBox[0];j<m_nParts;j++) if (g_parts[j].isle==i) {
					gpAux.flags = m_parts[j].flags;
					gpAux.flagsCollider = m_parts[j].flagsCollider;
					if (gpAux.flagsCollider & geom_destroyed_on_break && epcep.nTotParts==1)
						gpAux.flags=gpAux.flagsCollider = 0;
					gpAux.mass = m_parts[j].mass;
					gpAux.pos = m_parts[j].pos;
					gpAux.q = m_parts[j].q;
					gpAux.scale = m_parts[j].scale;
					gpAux.idmatBreakable = m_parts[j].idmatBreakable;
					gpAux.minContactDist = m_parts[j].minContactDist;
					gpAux.pMatMapping = m_parts[j].pMatMapping!=m_parts[j].pPhysGeom->pMatMapping ? m_parts[j].pMatMapping:0;
					gpAux.nMats = m_parts[j].nMats;
					if (gpAux.mass<=m_pWorld->m_vars.massLimitDebris) {
						if (!is_unused(m_pWorld->m_vars.flagsColliderDebris)) 
							gpAux.flagsCollider = m_pWorld->m_vars.flagsColliderDebris;
						gpAux.flags &= m_pWorld->m_vars.flagsANDDebris;
					}
					/*if (!(m_parts[j].flags & geom_will_be_destroyed) && m_parts[j].pPhysGeom->pGeom->GetType()==GEOM_VOXELGRID)	{
						(pMeshNew = new CTriMesh())->Clone((CTriMesh*)m_parts[j].pPhysGeom->pGeom,0);
						delete pMeshNew->m_pTree;	pMeshNew->m_pTree=0;
						pMeshNew->m_flags |= mesh_AABB;
						pMeshNew->RebuildBVTree();
						m_parts[j].pPhysGeom->pGeom->Release();
						m_parts[j].pPhysGeom->pGeom = pMeshNew;
					}*/
					epcep.pEntNew->AddGeometry(m_parts[j].pPhysGeom,&gpAux,i1,1);
					if (m_parts[j].pPhysGeomProxy!=m_parts[j].pPhysGeom) {
						gpAux.flags |= geom_proxy;
						epcep.pEntNew->AddGeometry(m_parts[j].pPhysGeomProxy,&gpAux,i1,1);
						gpAux.flags &= ~geom_proxy;
					}
					if (m_iSimClass>0) {
						pe_status_dynamics sd; sd.ipart=j;
						GetStatus(&sd);
						asv.v += sd.v*m_parts[j].mass;
						asv.w += sd.w*m_parts[j].mass;
						t += m_parts[j].mass;
					}
					shockwave.ipart = g_parts[j].idx = i1++;
					shockwave.iApplyTime = 0;
					if (g_parts[j].ijoint0>=nJoints || 
							min(min(min(min(m_pStructure->pJoints[g_jointidx[g_parts[j].ijoint0]].maxForcePull, 
															m_pStructure->pJoints[g_jointidx[g_parts[j].ijoint0]].maxForcePush),
															m_pStructure->pJoints[g_jointidx[g_parts[j].ijoint0]].maxForceShift), 
															m_pStructure->pJoints[g_jointidx[g_parts[j].ijoint0]].maxTorqueBend),
															m_pStructure->pJoints[g_jointidx[g_parts[j].ijoint0]].maxTorqueTwist) > 0) 
					{
						shockwave.impulse = ((m_pStructure->pParts[j].Pext*dt*100+m_pStructure->pParts[j].Fext)*0.6f+Pmax*0.4f)*max(0.1f,m_parts[j].mass*M);
						shockwave.angImpulse = ((m_pStructure->pParts[j].Lext*dt*100+m_pStructure->pParts[j].Text)*0.6f+Lmax*0.4f)*max(0.1f,m_parts[j].mass*M);
					} else {
						shockwave.impulse = m_pStructure->pParts[j].Pext*dt*100;
						shockwave.angImpulse = m_pStructure->pParts[j].Lext*dt*100;
					}
					if (shockwave.impulse.len2() > sqr(m_parts[j].mass*4.0f))
						shockwave.impulse*=(vmax=m_parts[j].mass*4.0f/shockwave.impulse.len()), shockwave.angImpulse*=vmax;
					epcep.breakImpulse=shockwave.impulse; epcep.breakAngImpulse=shockwave.angImpulse;
					if (!(m_flags & aef_recorded_physics))
						epcep.pEntNew->Action(&shockwave,1);
					epcep.partidSrc = m_parts[ipartSrc = j].id;
					epcep.partidNew = shockwave.ipart;
					epcep.pEntNew->GetStatus(&psd); epcep.v = psd.v; epcep.w = psd.w;
					m_pWorld->OnEvent(m_pWorld->m_vars.bLogStructureChanges+1,&epcep);
					g_parts[j].pent = (CPhysicalEntity*)epcep.pEntNew;
					BBoxNew[0] = min(BBoxNew[0], m_parts[j].BBox[0]);
					BBoxNew[1] = max(BBoxNew[1], m_parts[j].BBox[1]);
				}
				for(j=0;j<m_pStructure->nJoints;j++) 
					if (!m_pStructure->pJoints[j].bBroken && g_parts[m_pStructure->pJoints[j].ipart[0]].isle==i) {
						psj.id = m_pStructure->pJoints[j].id;
						psj.partid[0] = g_parts[m_pStructure->pJoints[j].ipart[0]].idx;
						psj.partid[1] = g_parts[m_pStructure->pJoints[j].ipart[1]].idx;
						psj.pt = m_pStructure->pJoints[j].pt;
						psj.n = m_pStructure->pJoints[j].n;
						psj.axisx = m_pStructure->pJoints[j].axisx;
						psj.maxForcePush = m_pStructure->pJoints[j].maxForcePush;
						psj.maxForcePull = m_pStructure->pJoints[j].maxForcePull;
						psj.maxForceShift = m_pStructure->pJoints[j].maxForceShift;
						psj.maxTorqueBend = m_pStructure->pJoints[j].maxTorqueBend;
						psj.maxTorqueTwist = m_pStructure->pJoints[j].maxTorqueTwist;
						psj.limitConstraint = m_pStructure->pJoints[j].limitConstr;
						psj.dampingConstraint = m_pStructure->pJoints[j].dampingConstr;
						psj.bBreakable = m_pStructure->pJoints[j].bBreakable & 1;
						psj.bConstraintWillIgnoreCollisions = m_pStructure->pJoints[j].bBreakable>>1^1;
						psj.bDirectBreaksOnly = m_pStructure->pJoints[j].bBreakable>>2;
						psj.szSensor = m_pStructure->pJoints[j].size;
						epcep.pEntNew->SetParams(&psj,1);
						((CPhysicalEntity*)epcep.pEntNew)->m_pStructure->bModified = 1;
					}
				j = ipartSrc;
				if (i1>0 && m_pStructure->pParts[j].initialVel.GetLengthSquared()!=0.f) {
					// override the initial velocity
					asv.v = m_pStructure->pParts[j].initialVel;
					asv.w = m_pStructure->pParts[j].initialAngVel;
					m_pStructure->pParts[j].initialVel.zero();
					m_pStructure->pParts[j].initialAngVel.zero();
					epcep.pEntNew->Action(&asv,1);
				} else {
					if (m_iSimClass*t>0) {
						asv.v*=(t=1.0f/t); asv.w*=t;
						epcep.pEntNew->Action(&asv,1);
					}
				}

				if (!(m_flags & aef_recorded_physics)) {
					if (!pents)
						iter = m_pWorld->GetEntitiesAround(m_BBox[0]-Vec3(0.1f),m_BBox[1]+Vec3(0.1f),pents,ent_rigid|ent_sleeping_rigid|ent_independent);
					BBoxNew[0] -= Vec3(m_pWorld->m_vars.maxContactGap*5); 
					BBoxNew[1] += Vec3(m_pWorld->m_vars.maxContactGap*5);	// cppcheck-suppress nullPointer
					for(j=iter-1; j>=0; j--) if (AABB_overlap(pents[j]->m_BBox, BBoxNew))	// cppcheck-suppress nullPointer
						pents[j]->OnNeighbourSplit(this,(CPhysicalEntity*)epcep.pEntNew);
				}
			}

			for(i=0;i<m_pStructure->nJoints;i++) if (m_pStructure->pJoints[i].bBroken) {
				for(j=0;j<2;j++) if ((i0=m_pStructure->pJoints[i].ipart[j])>=0 && m_parts[i0].flags & geom_manually_breakable) {
					EventPhysCollision epc;
					epc.pEntity[0]=&g_StaticPhysicalEntity; epc.pForeignData[0]=0; epc.iForeignData[0]=0;
					epc.vloc[1].zero(); epc.mass[0] = 1E10f;
					epc.partid[0]=0; epc.penetration=epc.normImpulse=0;
					epc.pt = (m_parts[i0].BBox[0]+m_parts[i0].BBox[1])*0.5f;
					epc.pEntity[1] = g_parts[i0].pent; epc.pForeignData[1] = g_parts[i0].pent->m_pForeignData; 
					epc.iForeignData[1] = g_parts[i0].pent->m_iForeignData;
					m_parts[i0].pPhysGeom->pGeom->GetBBox(&bbox);
					epc.n.zero()[idxmin3(bbox.size)] = 1;
					epc.n = m_qrot*(m_parts[i0].q*(epc.n*bbox.Basis));
					epc.vloc[0] = epc.n*-4.0f;
					epc.mass[0]=epc.mass[1] = m_parts[i0].mass;
					epc.radius = (m_BBox[1].x+m_BBox[1].y+m_BBox[1].z-m_BBox[0].x-m_BBox[0].y-m_BBox[0].z)*2;
					epc.partid[1] = g_parts[i0].pent==this ? m_parts[i0].id:g_parts[i0].idx;
					epc.idmat[0] = -2;
					for(i1=m_parts[i0].pPhysGeom->pGeom->GetPrimitiveCount()-1; i1>=0; i1--) {
						epc.idmat[1] = GetMatId(m_parts[i0].pPhysGeom->pGeom->GetPrimitiveId(i1,0x40), i0);
						if (m_pWorld->m_SurfaceFlagsTable[epc.idmat[1]] & sf_manually_breakable)
							break; 
					}
					m_pWorld->OnEvent(pef_log_collisions, &epc);
				}
				if (m_pStructure->pJoints[i].itens<2 && m_pStructure->pJoints[i].limitConstr.z>0 && 
						g_parts[m_pStructure->pJoints[i].ipart[0]].pent!=g_parts[m_pStructure->pJoints[i].ipart[1]].pent) 
				{
					j = isneg(g_parts[m_pStructure->pJoints[i].ipart[0]].isle-g_parts[m_pStructure->pJoints[i].ipart[1]].isle);
					pe_action_add_constraint aac;
					aac.flags = local_frames | constraint_ignore_buddy&~-isneg(-(m_pStructure->pJoints[i].bBreakable&2));
					aac.maxBendTorque = m_pStructure->pJoints[i].limitConstr.z;
					aac.maxPullForce = min(m_pStructure->pJoints[i].maxForcePull, m_pStructure->pJoints[i].maxForcePush);
					aac.partid[0] = g_parts[m_pStructure->pJoints[i].ipart[j]].idx;
					aac.partid[1] = g_parts[m_pStructure->pJoints[i].ipart[j^1]].pent==this ? 
						m_parts[m_pStructure->pJoints[i].ipart[j^1]].id : g_parts[m_pStructure->pJoints[i].ipart[j^1]].idx;
					aac.pBuddy = g_parts[m_pStructure->pJoints[i].ipart[j^1]].pent;
					aac.pt[0]=aac.pt[1] = m_pStructure->pJoints[i].pt;
					aac.qframe[0]=aac.qframe[1] = quaternionf(
						Matrix33(m_pStructure->pJoints[i].axisx, m_pStructure->pJoints[i].n^m_pStructure->pJoints[i].axisx, m_pStructure->pJoints[i].n));
					aac.sensorRadius = m_pStructure->pJoints[i].size;
					if (m_pStructure->pJoints[i].limitConstr.y > m_pStructure->pJoints[i].limitConstr.x) {
						aac.yzlimits[0]=aac.yzlimits[1]=aac.xlimits[0]=aac.xlimits[1] = 0;
						*(Vec2*)(m_pStructure->pJoints[i].itens ? aac.yzlimits:aac.xlimits) = Vec2(m_pStructure->pJoints[i].limitConstr);
						aac.qframe[0]=aac.qframe[1] = aac.qframe[0]*quaternionf(Matrix33(Vec3(0,0,1),Vec3(1,0,0),Vec3(0,1,0)));
						aac.damping = m_pStructure->pJoints[i].dampingConstr;
					}	else if (m_pStructure->pJoints[i].dampingConstr>0) {
						pe_simulation_params psp; psp.damping = m_pStructure->pJoints[i].dampingConstr;
						g_parts[m_pStructure->pJoints[i].ipart[j]].pent->SetParams(&psp,1);
					}
					g_parts[m_pStructure->pJoints[i].ipart[j]].pent->Action(&aac,1);
					pe_params_flags pf;
					pf.flagsAND = ~pef_never_break;
					g_parts[m_pStructure->pJoints[i].ipart[j]].pent->SetParams(&pf,1);
					aac.pBuddy->SetParams(&pf,1);
				}
			}

			if (m_pWorld->m_vars.bLogLatticeTension && (bBroken || impTot>0 && m_pWorld->m_timePhysics>m_pStructure->minSnapshotTime))	{
				if (bBroken)
					m_pStructure->minSnapshotTime=m_pWorld->m_timePhysics+2.0f;
				m_pStructure->nPrevJoints = nJoints0;
				m_pStructure->prevdt = dt;
				for(i=0;i<nJoints;i++) {
					m_pStructure->pJoints[g_joints[i].idx].tension = g_jointsDbg[i].tension.x;
					m_pStructure->pJoints[g_joints[i].idx].itens = g_jointsDbg[i].itens;
				}
			}
			m_pStructure->nLastUsedJoints = nJoints;
				
			// move broken or detached joints to the end of the list
			for(i=iter=0;i<m_pStructure->nJoints & ~-m_pStructure->bTestRun;i++) 
				if (m_pStructure->pJoints[i].bBroken+g_parts[m_pStructure->pJoints[i].ipart[0]].isle > 0) {
					if (m_pStructure->pJoints[i].bBroken) {
						epjb.idJoint = m_pStructure->pJoints[i].id;
						epjb.pt = m_qrot*m_pStructure->pJoints[i].pt+m_pos;
						epjb.n = m_qrot*m_pStructure->pJoints[i].n;
						i0 = (m_pStructure->pJoints[i].ipart[0]|m_pStructure->pJoints[i].ipart[1])>=0 && 
							m_parts[m_pStructure->pJoints[i].ipart[0]].pPhysGeom->V*cube(m_parts[m_pStructure->pJoints[i].ipart[0]].scale) < 
							m_parts[m_pStructure->pJoints[i].ipart[1]].pPhysGeom->V*cube(m_parts[m_pStructure->pJoints[i].ipart[1]].scale);
						for(j=0;j<2;j++) if (m_pStructure->pJoints[i].ipart[j]>=0) {
							epjb.partid[j^i0] = m_parts[m_pStructure->pJoints[i].ipart[j]].id;
							epjb.partmat[j^i0] = GetMatId(m_parts[m_pStructure->pJoints[i].ipart[j]].pPhysGeom->pGeom->GetPrimitiveId(0,0x40), 
								m_pStructure->pJoints[i].ipart[j]);
						}	else
							epjb.partid[j^i0] = epjb.partmat[j^i0] = -1;
						m_pWorld->OnEvent(m_pWorld->m_vars.bLogStructureChanges+1,&epjb);
						++iter;
					}
					SStructuralJoint sj = m_pStructure->pJoints[i];
					m_pStructure->pJoints[i] = m_pStructure->pJoints[--m_pStructure->nJoints]; --i;
					m_pStructure->pJoints[m_pStructure->nJoints] = sj;
				}
			i = m_nParts-1 | -m_pStructure->bTestRun;
			next_eprep:
			for(j=ihead=0,eprep.idOffs=0,i0=-1; i>=0; i--) {
				m_pStructure->pParts[i].Pext.zero(); m_pStructure->pParts[i].Lext.zero();
				m_pStructure->pParts[i].Fext.zero(); m_pStructure->pParts[i].Text.zero();
				if (g_parts[i].isle!=0 && nRemoveGeoms<CRY_ARRAY_COUNT(idRemoveGeoms)) {
					i0 += i-i0 & i0>>31;
					if (m_parts[i].id-eprep.idOffs>=sizeof(eprep.partIds)*8) {
						for(i1=i0,eprep.idOffs=1<<30;i1>i;i1--) if (g_parts[i1].isle!=0)
							ihead=max(ihead,m_parts[i1].id), eprep.idOffs=min(eprep.idOffs,m_parts[i1].id);
						if (max(ihead,m_parts[i].id)-min(eprep.idOffs,m_parts[i].id) >= sizeof(eprep.partIds)*8)
							break;
						eprep.idOffs = min(eprep.idOffs,m_parts[i].id);
					}
					idRemoveGeoms[nRemoveGeoms] = m_parts[i].id; pRemoveGeoms[nRemoveGeoms++] = m_parts[i].pPhysGeom;	j++;
				}
			}
			if (j | iter) {
				memset(eprep.partIds, 0, sizeof(eprep.partIds));
				for(i1=1;i1<=j;i1++) {
					int idloc = idRemoveGeoms[nRemoveGeoms-i1]-eprep.idOffs;
					eprep.partIds[idloc>>5] |= 1<<idloc;
				}
				m_pWorld->OnEvent(m_pWorld->m_vars.bLogStructureChanges+1,&eprep);
				if (i>=0)
					goto next_eprep;
			}

			m_pStructure->bModified += bBroken;
			if (m_nParts-nRemoveGeoms==0)
				bBroken = 0;
			if (bBroken!=m_pStructure->nLastBrokenJoints)
				flags = geom_structure_changes;
			else if (flags & geom_structure_changes)
				m_pStructure->nLastBrokenJoints = bBroken;
			if (m_pStructure->idpartBreakOrg>=0) {
				flags &= ~geom_structure_changes;
				m_pStructure->idpartBreakOrg = -1;
				goto SkipMeshUpdates;
			}
		}
		
		if ((m_timeStructUpdate+=time_interval) >= m_pWorld->m_vars.tickBreakable) {
			if (m_pStructure) for(m_iLastIdx=i=0;i<m_nParts;i++)
				m_iLastIdx = max(m_iLastIdx, m_parts[i].id+1);
			epcep.iReason = EventPhysCreateEntityPart::ReasonMeshSplit;
			for(i=0;i<m_nParts;i++) if (m_parts[i].flags & geom_structure_changes) {
				epum.partid = m_parts[i].id;
				epum.pMesh = m_parts[i].pPhysGeomProxy->pGeom;
				nPlanes = min(m_nGroundPlanes, (int)(CRY_ARRAY_COUNT(ground)));
				for(j=0;j<nPlanes;j++) {
					ground[j].origin = (m_ground[j].origin-m_parts[i].pos)*m_parts[i].q;
					if (m_parts[i].scale!=1.0f)
						ground[j].origin /= m_parts[i].scale;
					ground[j].n = m_ground[j].n*m_parts[i].q;
				}
				if (m_pStructure && !m_pStructure->nJoints && m_pStructure->bTestRun && !nPlanes) {
					ground[0].origin = (m_parts[i].BBox[0]+Vec3(0,0,1)*m_pWorld->m_vars.maxContactGap-(m_pos+m_qrot*m_parts[i].pos))*m_parts[i].q;
					if (m_parts[i].scale!=1.0f)
						ground[0].origin /= m_parts[i].scale;
					ground[0].n = Vec3(0,0,1)*m_parts[i].q*m_qrot;
					nPlanes = 1;
				}

				if (m_updStage==0) {
					if (m_parts[i].pLattice && m_pExpl) {
						expl.epicenter=expl.epicenterImp = ((m_pExpl->center-m_pos)*m_qrot-m_parts[i].pos)*m_parts[i].q;
						expl.impulsivePressureAtR = max(0.0f,m_pExpl->kr)*m_parts[i].pLattice->m_density*
							m_parts[i].pPhysGeom->V*cube(m_parts[i].scale)/m_parts[i].mass;
						expl.r = 1.0f;
						expl.rmin = m_pExpl->rmin;
						expl.rmax = 1E10f;
						if (m_parts[i].scale!=1.0f) {
							t = 1.0f/m_parts[i].scale;
							expl.rmin*=t; expl.r*=t; expl.epicenter*=t; expl.epicenterImp*=t;
						}
					}
					if (m_parts[i].pLattice && m_parts[i].pLattice->CheckStructure(time_interval,(gravity*m_qrot)*m_parts[i].q, ground,nPlanes, 
							m_nUpdTicks<2 && m_pExpl ? &expl:0, m_pWorld->m_vars.nMaxLatticeIters,m_pWorld->m_vars.bLogLatticeTension))
					{
						epum.pMesh->Lock(0); epum.pLastUpdate = (bop_meshupdate*)epum.pMesh->GetForeignData(DATA_MESHUPDATE);
						for(; epum.pLastUpdate && epum.pLastUpdate->next; epum.pLastUpdate=epum.pLastUpdate->next);
						epum.pMesh->Unlock(0);
						m_pWorld->OnEvent(m_pWorld->m_vars.bLogStructureChanges+1,&epum);
					} else
						m_parts[i].flags &= ~geom_structure_changes;
					if (m_parts[i].pLattice && m_pWorld->m_pLog && m_pWorld->m_vars.bLogLatticeTension)	{
						t = m_parts[i].pLattice->GetLastTension(j);
						m_pWorld->m_pLog->LogToConsole("Lattice tension: %f (%s)", t, 
							j==LPush?"push":(j==LPull?"pull":(j==LShift?"shift":(j==LBend?"bend":"twist"))));
					}
				}	else {
					if (pMeshNew = ((CTriMesh*)m_parts[i].pPhysGeomProxy->pGeom)->SplitIntoIslands(ground,nPlanes,GetType()==PE_RIGID)) {
						nMeshSplits++;
						if (m_parts[i].pLattice) {
							for(j=0,pMesh=pMeshNew; pMesh; pMesh=(CTriMesh*)pMesh->GetForeignData(-2),j++);
							SIZEOF_ARRAY_OK
							if (j>CRY_ARRAY_COUNT(pChunkMeshesBuf)) {
								pChunkMeshes = new CTriMesh*[j];
								pChunkLattices = new CTetrLattice*[j];
							}
							for(j=0,pMesh=pMeshNew; pMesh; pMesh=(CTriMesh*)pMesh->GetForeignData(-2),j++)
								pChunkMeshes[j] = pMesh;
							m_parts[i].pLattice->Split(pChunkMeshes,j,pChunkLattices);
						}

						gp.pos = m_parts[i].pos;
						gp.q = m_parts[i].q;
						gp.scale = m_parts[i].scale;
						gp.pLattice = m_parts[i].pLattice;
						gp.pMatMapping = m_parts[i].pMatMapping!=m_parts[i].pPhysGeom->pMatMapping ? m_parts[i].pMatMapping:0;
						/*if (m_parts[i].pMatMapping!=m_parts[i].pPhysGeom->pMatMapping) {
							pOrgMapping = gp.pMatMapping = m_parts[i].pMatMapping; m_parts[i].pMatMapping = 0;
						}	else 
							gp.pMatMapping = pOrgMapping = 0;*/
						gp.nMats = m_parts[i].nMats;
						pgeom0 = m_parts[i].pPhysGeomProxy;
						//t = m_parts[i].mass/((V0=pgeom0->V)*cube(m_parts[i].scale)); 
						//ppt.ipart=i; ppt.density=0; SetParams(&ppt);
						V0 = pgeom0->V;
						float Imax0 = max(max(pgeom0->Ibody.x,pgeom0->Ibody.y),pgeom0->Ibody.z);
						pgeom0->pGeom->CalcPhysicalProperties(pgeom0);
						t = max(max(pgeom0->Ibody.x,pgeom0->Ibody.y),pgeom0->Ibody.z)*m_pWorld->m_vars.breakageMinAxisInertia;
						pgeom0->Ibody.x = max(pgeom0->Ibody.x, t); 
						pgeom0->Ibody.y = max(pgeom0->Ibody.y, t); 
						pgeom0->Ibody.z = max(pgeom0->Ibody.z, t); 
						pgeom0->pGeom->GetBBox(&bbox);
						org = bbox.Basis*(pgeom0->origin-bbox.center);
						if (m_iSimClass>0 &&
								(max(max(fabs_tpl(org.x)-bbox.size.x,fabs_tpl(org.y)-bbox.size.y),fabs_tpl(org.z)-bbox.size.z)>0 ||
								min(min(min(pgeom0->V,pgeom0->Ibody.x),pgeom0->Ibody.y),pgeom0->Ibody.z)<0 ||
								pgeom0->V<V0*0.002f)) 
						{
							//((CTriMesh*)pgeom0->pGeom)->Empty();
							pgeom0->V=0; pgeom0->Ibody.zero();
							m_parts[i].flags |= geom_invalid;	epum.bInvalid = 1;
						}	else
							epum.bInvalid = 0;
						//ppt.density=t; SetParams(&ppt);

						for(j=0; pMeshNew; pMeshNew=pMeshNext,j++) {
							pMeshNext = (CTriMesh*)pMeshNew->GetForeignData(-2);
							pMeshNew->SetForeignData(0,0);
							pgeom = m_pWorld->RegisterGeometry(pMeshNew,pgeom0->surface_idx,pgeom0->pMatMapping,pgeom0->nMats);
							pMeshNew->GetBBox(&bbox);
							org = bbox.Basis*(pgeom->origin-bbox.center);
							if (max(max(fabs_tpl(org.x)-bbox.size.x,fabs_tpl(org.y)-bbox.size.y),fabs_tpl(org.z)-bbox.size.z)>0 ||
									min(min(min(pgeom->V,pgeom->Ibody.x),pgeom->Ibody.y),pgeom->Ibody.z)<0 ||
									pgeom->V<V0*0.002f)
							{
								gp.flags=gp.flagsCollider=0; gp.idmatBreakable=-1;
								gp.density=0, epcep.bInvalid=1;
							} else {
								gp.flags = m_parts[i].flags & ~geom_structure_changes;
								gp.flagsCollider = m_parts[i].flagsCollider;
								gp.idmatBreakable = m_parts[i].idmatBreakable;
								gp.density = m_parts[i].mass/(V0*cube(m_parts[i].scale));
								epcep.bInvalid = 0;
							}
							t = max(max(pgeom->Ibody.x,pgeom->Ibody.y),pgeom->Ibody.z)*V0 / (pgeom->V*Imax0);
							gp.minContactDist = max(m_pWorld->m_vars.maxContactGap*2, m_parts[i].minContactDist*sqrt_fast_tpl(t));
							gp.pLattice = m_parts[i].pLattice ? pChunkLattices[j] : 0;
							if (!m_pStructure) {
								epcep.pEntNew = m_pWorld->CreatePhysicalEntity(PE_RIGID,&pp,0,0x5AFE);
								epcep.pEntNew->AddGeometry(pgeom,&gp,-1,1);
								pgeom->nRefCount--;
								epcep.partidNew = 0;
							} else {
								epcep.pEntNew = this;
								pAddGeoms[nAddGeoms]=pgeom; gpAddGeoms[nAddGeoms]=gp;
								bGeomOverflow |= isneg((int)(CRY_ARRAY_COUNT(gpAddGeoms))-nAddGeoms-2);
								nAddGeoms += 1-bGeomOverflow;
								//nAddGeoms=min(nAddGeoms+1, CRY_ARRAY_COUNT(gpAddGeoms));
								epcep.partidNew = iLastIdx+j;
							}
								
							gwd.R = Matrix33(m_parts[i].q);
							gwd.offset = m_parts[i].pos;
							gwd.scale = m_parts[i].scale;
							ip.bStopAtFirstTri=ip.bNoAreaContacts=ip.bNoBorder = true;

							for(i1=0;i1<m_nParts;i1++) if (m_parts[i1].idmatBreakable<0) {
								m_parts[i1].maxdim = m_parts[i1].pPhysGeom->V;
								if (m_parts[i1].pPhysGeom->pGeom->GetType()==GEOM_TRIMESH && 
										((CTriMesh*)m_parts[i1].pPhysGeom->pGeom)->BuildIslandMap()>1 && !m_parts[i1].pLattice) 
								{	// separate non-breakable meshes into islands; store island list in part's pLattice
									if (!(m_parts[i1].flags & geom_can_modify))
										m_pWorld->ClonePhysGeomInEntity(this,i1,(new CTriMesh())->Clone((CTriMesh*)m_parts[i1].pPhysGeom->pGeom,0));
									m_parts[i1].pLattice = (CTetrLattice*)((CTriMesh*)m_parts[i1].pPhysGeom->pGeom)->SplitIntoIslands(0,0,1);
									m_parts[i1].pPhysGeom->pGeom->CalcPhysicalProperties(m_parts[i1].pPhysGeom);
								}
							}

							if (!m_pStructure) {
								// find non-breakable parts that should be moved to the new entity
								int partId = m_parts[i].id;	// new foliage proxies will have ids main id+1024*n
								for(i1=0;i1<m_nParts;i1++) if (i1!=i) {
									pMeshIsland = !(m_parts[i1].flags & geom_removed) ? m_parts[i1].pPhysGeom->pGeom : 
																(m_parts[i1].idmatBreakable<0 ? (CTriMesh*)m_parts[i1].pLattice : 0);
									partId = max(partId, m_parts[i1].id);
									pMeshIslandPrev = 0;
									gwdAux.R = Matrix33(m_parts[i1].q);
									gwdAux.offset = m_parts[i1].pos;
									gwdAux.scale = m_parts[i1].scale;
									if (pMeshIsland) do {
										pMeshIsland->GetBBox(&bbox);
										t = m_parts[i].scale==1.0f ? 1.0f:1.0f/m_parts[i].scale;
										bbox.center = ((m_parts[i1].q*bbox.center*m_parts[i1].scale+m_parts[i1].pos-m_parts[i].pos)*m_parts[i].q)*t;
										//bbox.Basis *= Matrix33(!m_parts[i1].q*m_parts[i].q);
										//bbox.size *= m_parts[i1].scale*t;
										//boxGeom.CreateBox(&bbox);
										pMeshNew->GetBBox(&bbox1);
										org = bbox.size-(bbox.Basis*(gwd.R*bbox1.center*gwd.scale+gwd.offset-bbox.center)).abs();
										//bBroken = 0;//min(min(org.x,org.y),org.z)>0;
										bBroken = pMeshIsland->PointInsideStatus(((gwdAux.R*bbox1.center*gwdAux.scale+gwdAux.offset-gwd.offset)*gwd.R)*t);
										bBroken |= pMeshNew->Intersect(pMeshIsland, &gwd,&gwdAux, &ip, pcontacts)!=0;
										if (bBroken) {
											FillGeomParams(gpAux, m_parts[i1]);
											gpAux.mass *= (t=pMeshIsland->GetVolume()/m_parts[i1].maxdim); 
											gpAux.minContactDist = max(m_pWorld->m_vars.maxContactGap*2, gpAux.minContactDist*cubert_approx(t));
											if (pMeshIsland==m_parts[i1].pPhysGeom->pGeom) {
												if (epcep.pEntNew && !epcep.bInvalid)
													epcep.pEntNew->AddGeometry(m_parts[i1].pPhysGeom,&gpAux,m_parts[i1].id,1);
												m_parts[i1].flags |= geom_removed; //m_parts[i1].idmatBreakable = -10;
											} else {
												pgeom1 = m_pWorld->RegisterGeometry(pMeshIsland, m_parts[i1].pPhysGeom->surface_idx,
													m_parts[i1].pPhysGeom->pMatMapping,m_parts[i1].pPhysGeom->nMats);
												if (epcep.pEntNew && !epcep.bInvalid)
													epcep.pEntNew->AddGeometry(pgeom1,&gpAux,partId+=1024,1);
												CryInterlockedDecrement((volatile int*) &pgeom1->nRefCount);
												if (pMeshIslandPrev)
													pMeshIslandPrev->SetForeignData(pMeshIsland->GetForeignData(),-2);
												else
													m_parts[i1].pLattice = (CTetrLattice*)pMeshIsland->GetForeignData(-2);
											}
											if (pmu = (bop_meshupdate*)pMeshNew->GetForeignData(DATA_MESHUPDATE)) {
												ReallocateList(pmu->pMovedBoxes, pmu->nMovedBoxes, pmu->nMovedBoxes+1);
												pmu->pMovedBoxes[pmu->nMovedBoxes++] = bbox;
											}
										}
										if (pMeshIsland==m_parts[i1].pPhysGeom->pGeom)
											pMeshIsland = m_parts[i1].idmatBreakable<0 ? (CTriMesh*)m_parts[i1].pLattice : 0;
										else {
											pMeshIslandPrev = pMeshIsland;
											pMeshIsland = (CTriMesh*)pMeshIsland->GetForeignData(-2);
										}
									} while(pMeshIsland);
									if (m_parts[i1].idmatBreakable<0)	{
										m_parts[i1].mass *= (t=m_parts[i1].pPhysGeom->V/m_parts[i1].maxdim);
										m_parts[i1].minContactDist = max(m_pWorld->m_vars.maxContactGap*2, m_parts[i1].minContactDist*cubert_approx(t));
									}
								}
							} else if (!bGeomOverflow) {
								// find joints that must be deleted or re-assigned to the new part
								for(i1=0; i1<m_pStructure->nJoints; i1++) if (m_pStructure->pJoints[i1].ipart[0]==i || m_pStructure->pJoints[i1].ipart[1]==i) {
									sphere sph;
									sph.center = m_pStructure->pJoints[i1].pt; sph.r = m_pStructure->pJoints[i1].size*0.5f;
									sphGeom.CreateSphere(&sph);
									//aray.m_ray.dir = (aray.m_dirn = m_pStructure->pJoints[i1].n)*t;
									//aray.m_ray.origin = m_pStructure->pJoints[i1].pt-aray.m_ray.dir*0.5f;
									if (pMeshNew->Intersect(&sphGeom,&gwd,0,0,pcontacts)) { // joint touches the new mesh
										m_pStructure->pJoints[i1].ipart[iszero(m_pStructure->pJoints[i1].ipart[1]-i)] = m_nParts+j;
										m_pStructure->bModified++;
									}
									//else if (!m_parts[i].pPhysGeomProxy->pGeom->Intersect(&aray,&gwd,0,0,pcontacts)) // joint doesn't touch neither new nor old
									//	m_pStructure->pJoints[i1] = m_pStructure->pJoints[--m_pStructure->nJoints], i1--;
								}
							}

							if (epcep.pEntNew && !epcep.bInvalid) {
								org = m_pos+m_qrot*(m_parts[i].pos+m_parts[i].q*pgeom->origin);
								if (m_iSimClass>0) {
									pe_status_dynamics sd; sd.ipart=i;
									GetStatus(&sd);
									asv.v = sd.v+(sd.w^org-sd.centerOfMass);
									asv.w = sd.w;
									epcep.pEntNew->Action(&asv,1);
								}

								// add impulse from the explosion
								if (m_nUpdTicks==0 && m_pExpl) {
									gwd.R = Matrix33(m_qrot*m_parts[i].q);
									gwd.offset = m_pos + m_qrot*m_parts[i].pos;
									gwd.scale = m_parts[i].scale;
									shockwave.impulse.zero(); shockwave.angImpulse.zero();
									if (m_pExpl->kr>0)
										pgeom->pGeom->CalcVolumetricPressure(&gwd, m_pExpl->center,m_pExpl->kr,m_pExpl->rmin, 
											gwd.R*pgeom->origin*gwd.scale+gwd.offset, shockwave.impulse,shockwave.angImpulse);
									else if (m_pExpl->kr<0) {
										MARK_UNUSED shockwave.point;
										shockwave.impulse = m_pExpl->dir*(pgeom->V*cube(gwd.scale)*gp.density);
										n = m_pExpl->center-org;
										if (n.len2()>0) {
											shockwave.impulse -= n*((n*shockwave.impulse)/n.len2());
											quaternionf q = m_qrot*m_parts[i].q*pgeom->q;
											shockwave.angImpulse = q*(Diag33(pgeom->Ibody)*(!q*(m_pExpl->dir^n)))*(cube(gwd.scale)*gp.density/n.len2());
										}
									}
									shockwave.ipart = 0;
									epcep.breakImpulse=shockwave.impulse; epcep.breakAngImpulse=shockwave.angImpulse;
									shockwave.iApplyTime = (epcep.pEntNew!=this)*2;
									epcep.pEntNew->Action(&shockwave, 1);//shockwave.iApplyTime>>1^1);
									epcep.breakSize = m_pExpl->rmin;
									epcep.cutPtLoc[0]=epcep.cutPtLoc[1] = (m_pExpl->center-m_pos)*m_qrot;
									epcep.cutDirLoc[0]=epcep.cutDirLoc[1] = m_pExpl->dir*m_qrot;
								}

								//if (m_parts[i].flags & geom_break_approximation && ((CPhysicalEntity*)epcep.pEntNew)->m_nParts)
								//	((CPhysicalEntity*)epcep.pEntNew)->CapsulizePart(0);
								if (m_parts[i].flags & geom_break_approximation && ((CPhysicalEntity*)epcep.pEntNew)->m_nParts &&
										(nJoints=((CPhysicalEntity*)epcep.pEntNew)->CapsulizePart(0)))
								{
									CPhysicalEntity *pent = (CPhysicalEntity*)epcep.pEntNew;
									Vec3 ptEnd[2];
									capsule *pcaps = (capsule*)pent->m_parts[pent->m_nParts-nJoints].pPhysGeom->pGeom->GetData();
									epcep.cutRadius = pcaps->r*pent->m_parts[pent->m_nParts-nJoints].scale;
									ptEnd[0]=ptEnd[1]=epcep.cutPtLoc[0]=epcep.cutPtLoc[1] = pent->m_parts[pent->m_nParts-nJoints].pos - 
										pent->m_parts[pent->m_nParts-nJoints].q*Vec3(0,0,pcaps->hh+pcaps->r);
									epcep.cutDirLoc[1] = -(epcep.cutDirLoc[0] = pent->m_parts[pent->m_nParts-nJoints].q*(n=pcaps->axis));
									if (t=((CTriMesh*)m_parts[i].pPhysGeom->pGeom)->GetIslandDisk(100, ((epcep.cutPtLoc[0]-m_parts[i].pos)*m_parts[i].q)/m_parts[i].scale,
										epcep.cutPtLoc[0],n,vmax))
									{
										epcep.cutRadius = t*m_parts[i].scale;
										epcep.cutPtLoc[0] = m_parts[i].q*epcep.cutPtLoc[0]*m_parts[i].scale + m_parts[i].pos;
										epcep.cutDirLoc[0] = m_parts[i].q*n;
										ptEnd[0] = epcep.cutPtLoc[0] + epcep.cutDirLoc[0]*(vmax*m_parts[i].scale);
									}
									if (t=pMeshNew->GetIslandDisk(100, ((epcep.cutPtLoc[1]-m_parts[i].pos)*m_parts[i].q)/m_parts[i].scale,
										epcep.cutPtLoc[1],n,vmax))
									{
										epcep.cutRadius = epcep.cutRadius<t*m_parts[i].scale ? epcep.cutRadius : t*m_parts[i].scale;
										epcep.cutPtLoc[1] = m_parts[i].q*epcep.cutPtLoc[1]*m_parts[i].scale + m_parts[i].pos;
										epcep.cutDirLoc[1] = m_parts[i].q*n;
										ptEnd[1] = epcep.cutPtLoc[1] + epcep.cutDirLoc[1]*(vmax*m_parts[i].scale);
									}

									if (m_iSimClass==0 && !m_pWorld->m_vars.bMultiplayer && 
											(m_parts[i].flags & (geom_constraint_on_break|geom_colltype_vehicle))==(geom_constraint_on_break|geom_colltype_vehicle)) 
									{
										pe_action_add_constraint aac;
										aac.pBuddy = this; aac.id = 1000000;
										aac.pt[0] = ptEnd[1];	aac.pt[1] = ptEnd[0];
										//aac.pt[1] = aac.pt[0]-pent->m_qrot*epcep.cutDirLoc[0]*(pcaps->r*pent->m_parts[pent->m_nParts-nJoints].scale);
										aac.qframe[0]=aac.qframe[1] = 
											pent->m_qrot*pent->m_parts[pent->m_nParts-nJoints].q*Quat::CreateRotationV0V1(Vec3(1,0,0),pcaps->axis);
										aac.partid[0] = pent->m_parts[pent->m_nParts-nJoints].id;
										aac.partid[1] = m_parts[i].id;
										aac.xlimits[0]=aac.xlimits[1] = 0;
										aac.yzlimits[0] = 0; aac.yzlimits[1] = 1.1f;
										aac.damping = 1;
										aac.maxBendTorque = m_parts[i].mass*0.001f;
										//aac.maxPullForce = m_parts[i].mass*10.0f;
										aac.flags = constraint_ignore_buddy|local_frames;
										aac.sensorRadius = 0.5f+pcaps->r;
										pent->Action(&aac);
										m_parts[i].flags &= ~geom_constraint_on_break;
									}	else
										epcep.cutDirLoc[1].zero();

									if (pmu) {
										pe_simulation_params sp;
										sp.damping = 1.5f;
										epcep.pEntNew->SetParams(&sp);
										pe_action_awake aa;
										aa.minAwakeTime = 2.0f;
										epcep.pEntNew->Action(&aa);
									}
								}

								epcep.partidSrc = m_parts[i].id;
								epcep.pMeshNew = pMeshNew;
								epcep.nTotParts = 1;
								epcep.pLastUpdate = (bop_meshupdate*)epcep.pMeshNew->GetForeignData(DATA_MESHUPDATE);
								for(; epcep.pLastUpdate && epcep.pLastUpdate->next; epcep.pLastUpdate=epcep.pLastUpdate->next);
								epcep.pEntNew->GetStatus(&psd); epcep.v = psd.v; epcep.w = psd.w;
								m_pWorld->OnEvent(m_pWorld->m_vars.bLogStructureChanges+1,&epcep);

								pMeshNew->GetBBox(&bbox1);
								bbox.Basis *= Matrix33(!m_parts[i].q*!m_qrot);
								sz = (bbox1.size*bbox1.Basis.Fabs())*m_parts[i].scale + Vec3(m_pWorld->m_vars.maxContactGap*5);
								BBoxNew[0]=BBoxNew[1] = m_pos + m_qrot*(m_parts[i].pos + m_parts[i].q*bbox1.center*m_parts[i].scale);
								BBoxNew[0] -= sz; BBoxNew[1] += sz;
								for(i1=m_pWorld->GetEntitiesAround(BBoxNew[0],BBoxNew[1],pents,ent_rigid|ent_sleeping_rigid|ent_independent)-1; 
										i1>=0; i1--) 
									pents[i1]->OnNeighbourSplit(this,(CPhysicalEntity*)epcep.pEntNew);
							} else if (epcep.pEntNew && epcep.pEntNew!=this)
								m_pWorld->DestroyPhysicalEntity(epcep.pEntNew,0,1);
						}
						if (V0>0)	{
							m_parts[i].mass *= pgeom0->V/V0;
							t = max(max(pgeom0->Ibody.x,pgeom0->Ibody.y),pgeom0->Ibody.z)*V0 / (pgeom0->V*Imax0);
							m_parts[i].minContactDist = max(m_pWorld->m_vars.maxContactGap*2, m_parts[i].minContactDist*sqrt_fast_tpl(max(0.0f,t)));
						}
						//if (pOrgMapping) delete[] pOrgMapping;
			
						epum.pMesh->Lock(0); epum.pLastUpdate = (bop_meshupdate*)epum.pMesh->GetForeignData(DATA_MESHUPDATE);
						for(; epum.pLastUpdate && epum.pLastUpdate->next; epum.pLastUpdate=epum.pLastUpdate->next);
						epum.pMesh->Unlock(0);
						m_pWorld->OnEvent(m_pWorld->m_vars.bLogStructureChanges+1,&epum);
					}
				}
				flags |= m_parts[i].flags;
				iLastIdx = m_iLastIdx+nAddGeoms;
			}

			if (nMeshSplits) {
				// attach the islands of non-breakable geoms to the original entity if they are not moved to any chunk
				for(i=m_nParts-1;i>=0;i--) if (m_parts[i].idmatBreakable<0)	{
					for(j=0,gpAux.bRecalcBBox=0; j<m_nParts; j++) if (!(m_parts[j].id-m_parts[i].id & 1023))
						gpAux.bRecalcBBox = max(gpAux.bRecalcBBox, m_parts[j].id); // use gpAux.bRecalcBBox to store part id for new foliage proxies
					for(j=0; j<nAddGeoms; j++) if (!(gpAddGeoms[j].bRecalcBBox-m_parts[i].id & 1023))
						gpAux.bRecalcBBox = max(gpAux.bRecalcBBox, gpAddGeoms[j].bRecalcBBox);
					for(pMeshIsland=(CTriMesh*)m_parts[i].pLattice; pMeshIsland; pMeshIsland=(CTriMesh*)pMeshIsland->GetForeignData(-2)) {
						FillGeomParams(gpAux, m_parts[i]); 
						gpAux.mass *= pMeshIsland->GetVolume()/m_parts[i].maxdim; 
						pgeom1 = m_pWorld->RegisterGeometry(pMeshIsland, m_parts[i].pPhysGeom->surface_idx,
							m_parts[i].pPhysGeom->pMatMapping,m_parts[i].pPhysGeom->nMats);
						//AddGeometry(pgeom1, &gpAux,-1,1); pgeom1->nRefCount--;
						gpAux.bRecalcBBox += 1024;
						pAddGeoms[nAddGeoms]=pgeom1; gpAddGeoms[nAddGeoms]=gpAux; 
						nAddGeoms=min(nAddGeoms+1, (int)(CRY_ARRAY_COUNT(gpAddGeoms)-1));
					}
					gpAux.bRecalcBBox = 1;
					m_parts[i].pPhysGeom->pGeom->GetBBox(&bbox);
					m_parts[i].maxdim = max(max(bbox.size.x,bbox.size.y),bbox.size.z)*m_parts[i].scale;
					m_parts[i].pLattice = 0;
				}
				for(i=m_nParts-1; i>=0; i--)
					if (m_parts[i].flags & geom_removed) {
						//(pgeom=m_parts[i].pPhysGeom)->nRefCount++; RemoveGeometry(m_parts[i].id); pgeom->nRefCount--;
						idRemoveGeoms[nRemoveGeoms] = m_parts[i].id; pRemoveGeoms[nRemoveGeoms] = m_parts[i].pPhysGeom;
						nRemoveGeoms = min(nRemoveGeoms+1, (int)(CRY_ARRAY_COUNT(idRemoveGeoms)-1));
					}	else if (m_parts[i].flags & geom_invalid) {
						m_parts[i].flags=m_parts[i].flagsCollider=0; m_parts[i].idmatBreakable=-1;
					}
				if (pChunkMeshes!=pChunkMeshesBuf) delete[] pChunkMeshes;
				if (pChunkLattices!=pChunkLatticesBuf) delete[] pChunkLattices;
				RecomputeMassDistribution();
				//pe_action_reset ar; ar.bClearContacts=0;
				//Action(&ar,1);
				for(j=m_pWorld->GetEntitiesAround(m_BBox[0]-Vec3(0.1f),m_BBox[1]+Vec3(0.1f),pents,ent_rigid|ent_sleeping_rigid|ent_independent)-1; j>=0; j--)
					pents[j]->OnNeighbourSplit(this,0);
			}

			m_timeStructUpdate = m_updStage*m_pWorld->m_vars.tickBreakable;
			m_updStage ^= 1; m_nUpdTicks++;
		}
	}

	SkipMeshUpdates:
	for(i=0;i<nAddGeoms;i++) {
		int id = gpAddGeoms[i].bRecalcBBox | gpAddGeoms[i].bRecalcBBox-2>>31;
		gpAddGeoms[i].bRecalcBBox = 1;
		AddGeometry(pAddGeoms[i], gpAddGeoms+i,id,1);
		CryInterlockedDecrement((volatile int*) &pAddGeoms[i]->nRefCount);
	}
	for(i=0;i<nRemoveGeoms;i++) {
		CryInterlockedIncrement((volatile int*) &pRemoveGeoms[i]->nRefCount);
		RemoveGeometry(idRemoveGeoms[i]);
		CryInterlockedDecrement((volatile int*) &pRemoveGeoms[i]->nRefCount);
	}
	if (m_iSimClass && m_nParts && m_parts[0].flags & geom_break_approximation)
		CapsulizePart(0);
	if (!(m_flags & aef_recorded_physics) && (nRemoveGeoms || nMeshSplits))
		for(i1=m_pWorld->GetEntitiesAround(m_BBox[0]-Vec3(0.1f),m_BBox[1]+Vec3(0.1f),pents,ent_rigid|ent_sleeping_rigid|ent_independent)-1; i1>=0; i1--)
			pents[i1]->OnNeighbourSplit(this,0);

	i0 = 0;
	if (m_pStructure && m_pStructure->defparts) for(i=0;i<m_nParts;i++) if (m_pStructure->defparts[i].pSkelEnt) {
		CPhysicalEntity *pent = m_pStructure->defparts[i].pSkelEnt;
		pe_params_pos ppos; 
		memcpy(&ppos.pos, &m_pos, sizeof(m_pos)); // gcc workaround
		memcpy(&ppos.q, &m_qrot, sizeof(m_qrot));
		pent->SetParams(&ppos); 
		pe_params_flags pf; 
		pf.flagsAND = ~pef_invisible;	pent->SetParams(&pf);
		i0=m_parts[i].flags; m_parts[i].flags&=geom_colltype_explosion|geom_colltype_ray|geom_monitor_contacts;
		RigidBody *pbody = GetRigidBody(i);
		Vec3 v=pbody->v, w=pbody->w;
		pbody->v.zero(); pbody->w.zero();
		pent->Awake();
		for(j=0;j<m_pStructure->defparts[i].nSteps;j++) {
			pent->StartStep(m_pStructure->defparts[i].timeStep);
			pent->Step(m_pStructure->defparts[i].timeStep);
		}
		m_parts[i].flags=i0;
		pbody->v=v; pbody->w=w;
		pf.flagsOR = pef_invisible; pent->SetParams(&pf);
		pe_action_reset ar;	ar.bClearContacts = 2;
		pent->Action(&ar);
		UpdateDeformablePart(i);
		if (m_pStructure->defparts[i].lastUpdateTime && m_pWorld->m_timePhysics > m_pStructure->defparts[i].lastUpdateTime+2.0f)
			((CSoftEntity*)pent)->BakeCurrentPose();
		m_pStructure->defparts[i].lastUpdateTime = m_pWorld->m_timePhysics;
		m_pStructure->defparts[i].lastUpdatePos = m_pos;
		m_pStructure->defparts[i].lastUpdateq = m_qrot;
	}

	if (flags & geom_structure_changes || i0) {
		//WriteLock lock(m_lockUpdate);
		if (GetType()!=PE_STATIC && flags & geom_structure_changes) {
			for(i=0;i<m_nColliders;i++)	if (m_pColliders[i]!=this)
				m_pColliders[i]->Awake();
		}/* else if (m_nRefCount>0) {
			CPhysicalEntity **pentlist;
			Vec3 inflator = (m_BBox[1]-m_BBox[0])*1E-3f+Vec3(4,4,4)*m_pWorld->m_vars.maxContactGap;
			for(i=m_pWorld->GetEntitiesAround(m_BBox[0]-inflator,m_BBox[1]+inflator, pentlist, 
				ent_sleeping_rigid|ent_rigid|ent_living|ent_independent|ent_triggers)-1; i>=0; i--)
				pentlist[i]->Awake();
		}*/

		Vec3 BBox[2];
		ComputeBBox(BBox,0);
		i = m_pWorld->RepositionEntity(this,1,BBox);
		{ WriteLock locku(m_lockUpdate);
			ComputeBBox(m_BBox);
			m_pWorld->UnlockGrid(this,-i);
		}
		RepositionParts();
	}

	return (i0==0 && m_updStage==0) || flags & geom_structure_changes;
}


void CPhysicalEntity::OnContactResolved(entity_contact *pContact, int iop, int iGroupId)
{
	int i;
	if (iop<2 && (unsigned int)pContact->ipart[iop]<(unsigned int)m_nParts && 
			(m_flags & aef_recorded_physics || !(pContact->pent[iop^1]->m_flags & pef_never_break) || 
			 m_iSimClass>0 && GetRigidBody()->v.len2()>sqr(3.0f)) && 
			!(pContact->flags & contact_angular)) 
	{
		Vec3 n,pt=pContact->pt[iop&1];
		if (!(pContact->flags & contact_rope) || iop==0)
			n = pContact->n*(1-iop*2);
		else if (iop==1) 
			n = pContact->vreq*pContact->nloc.x;//pContact->K(0,0);
		else {
			rope_solver_vtx &svtx = ((rope_solver_vtx*)pContact->pBounceCount)[iop-2];
			n = svtx.P;
			pt = svtx.r+svtx.pbody->pos;
		}
		float scale = 1.0f+(m_pWorld->m_vars.breakImpulseScale-1.0f)*iszero((int)m_flags & pef_override_impulse_scale);
		if (m_pStructure) {
			Vec3 dP,r,sz;
			float Plen2,Llen2;
			/*if (m_pWorld->m_updateTimes[0]!=m_pStructure->timeLastUpdate) {
				for(i=0;i<m_nParts;i++)
					m_pStructure->Pext[i].zero(),m_pStructure->Lext[i].zero();
				m_pStructure->timeLastUpdate = m_pWorld->m_updateTimes[0];
			}*/
			i = MapHitPointFromParent(pContact->ipart[iop], pt);
			dP = n*(pContact->Pspare*scale);
			r = m_qrot*(m_parts[i].q*m_parts[i].pPhysGeom->origin*m_parts[i].scale+m_parts[i].pos)+m_pos-pt;
			if (pContact->flags & contact_rope || pContact->pent[iop^1]->GetType()!=PE_RIGID || 
					(((CRigidEntity*)pContact->pent[iop^1])->m_prevv-pContact->pbody[iop^1]->v).len2()<4) 
			{	Plen2 = (m_pStructure->pParts[i].Fext += dP).len2();
				Llen2 = (m_pStructure->pParts[i].Text += r^dP).len2();
			}	else {
				Plen2 = (m_pStructure->pParts[i].Pext += dP).len2();
				Llen2 = (m_pStructure->pParts[i].Lext += r^dP).len2();
			}
			sz = m_parts[i].BBox[1]-m_parts[i].BBox[0];
			if (max(Plen2, Llen2*sqr(sz.x+sz.y+sz.z)*0.1f) > 
					sqr(m_parts[i].mass*0.01f)*m_pWorld->m_vars.gravity.len2()*(0.01f+0.99f*iszero((int)m_flags & aef_recorded_physics)))
			{
				m_pWorld->MarkEntityAsDeforming(this), m_iGroup=iGroupId;
				if (m_pStructure->defparts && m_pStructure->defparts[i].pSkelEnt && 
						(!((CGeometry*)m_parts[i].pPhysGeom->pGeom)->IsAPrimitive() ||
						 m_pWorld->m_timePhysics > m_pStructure->defparts[i].lastCollTime+2.0f || 
						 pContact->Pspare > m_pStructure->defparts[i].lastCollImpulse*1.5f))
				{
					pe_action_impulse ai;
					ai.point = m_pStructure->defparts[i].lastUpdateq*(!m_qrot*(pt-m_pos))+m_pStructure->defparts[i].lastUpdatePos;
					ai.impulse = n*min(m_pStructure->defparts[i].maxImpulse, pContact->Pspare);
					m_pStructure->defparts[i].pSkelEnt->Action(&ai);
					m_pStructure->defparts[i].lastCollTime = m_pWorld->m_timePhysics;
					m_pStructure->defparts[i].lastCollImpulse = pContact->Pspare;
				}
			}
		}

		if (m_parts[i=pContact->ipart[iop]].pLattice && m_parts[i].idmatBreakable>=0) {
			float rdensity = m_parts[i].pLattice->m_density*m_parts[i].pPhysGeom->V*cube(m_parts[i].scale)/m_parts[i].mass;
			Vec3 ptloc = ((pContact->pt[iop]-m_pos)*m_qrot-m_parts[i].pos)*m_parts[i].q, dP;
			if (m_parts[i].scale!=1.0f)
				ptloc /= m_parts[i].scale;
			dP = n*(pContact->Pspare*rdensity*scale);

			if (m_parts[i].pLattice->AddImpulse(ptloc, (dP*m_qrot)*m_parts[i].q, Vec3(0,0,0), 
					m_pWorld->m_vars.gravity*m_pWorld->m_vars.jointGravityStep*m_pWorld->m_updateTimes[0],m_pWorld->m_updateTimes[0]) && !(m_flags & pef_deforming))
				m_updStage=0,m_iGroup=iGroupId,m_pWorld->MarkEntityAsDeforming(this);
		}	
	}
}


int CPhysicalEntity::GetStateSnapshot(TSerialize ser, float time_back, int flags) 
{ 
	if (ser.GetSerializationTarget()!=eST_Network)
	{
		bool bVal; int i,id;
		ser.Value("hasJoints", bVal=(m_pStructure!=0 && m_pStructure->bModified>0), 'bool');
		if (bVal) {
			ser.Value("nJoints", m_pStructure->nJoints);
			for(i=0;i<m_pStructure->nJoints;i++) {
				ser.BeginGroup("joint");
				ser.Value("id",m_pStructure->pJoints[i].id);
				ser.Value("ipart0",id=m_pStructure->pJoints[i].ipart[0]>=0 ? m_parts[m_pStructure->pJoints[i].ipart[0]].id:-1);
				ser.Value("ipart1",id=m_pStructure->pJoints[i].ipart[1]>=0 ? m_parts[m_pStructure->pJoints[i].ipart[1]].id:-1);
				ser.Value("pt",m_pStructure->pJoints[i].pt);
				ser.Value("n",m_pStructure->pJoints[i].n);
				ser.Value("maxForcePush",m_pStructure->pJoints[i].maxForcePush);
				ser.Value("maxForcePull",m_pStructure->pJoints[i].maxForcePull);
				ser.Value("maxForceShift",m_pStructure->pJoints[i].maxForceShift);
				ser.Value("maxTorqueBend",m_pStructure->pJoints[i].maxTorqueBend);
				ser.Value("maxTorqueTwist",m_pStructure->pJoints[i].maxTorqueTwist);
				ser.Value("bBreakable",m_pStructure->pJoints[i].bBreakable);
				ser.Value("szHelper",m_pStructure->pJoints[i].size);
				ser.Value("axisx",m_pStructure->pJoints[i].axisx);
				ser.Value("limitConstr",m_pStructure->pJoints[i].limitConstr);
				ser.Value("dampingConstr",m_pStructure->pJoints[i].dampingConstr);
				ser.EndGroup();
			}
			char buf[2048],*ptr;
			for(i=0,*(ptr=buf)++='|'; i<m_nParts && ptr<buf+2038; i++,*ptr++=',')
				ptr += strlen(ltoa(m_parts[i].id, ptr, 16));
			*ptr = 0;
			ser.Value("usedParts", (const char*)buf);
		}

		if (!m_pStructure || !m_pStructure->defparts)
			ser.Value("hasDfm", bVal=false);
		else {
			ser.Value("hasDfm", bVal=true);
			for(i=0;i<m_nParts;i++) if (m_pStructure->defparts[i].pSkelEnt)
				m_pStructure->defparts[i].pSkelEnt->GetStateSnapshot(ser,time_back,flags);
		}
	}

	return 1; 
}

int CPhysicalEntity::SetStateFromSnapshot(TSerialize ser, int flags) 
{
	if (ser.GetSerializationTarget()!=eST_Network)
	{
		bool bVal;
		ser.Value("hasJoints", bVal, 'bool');
		if (bVal) {
			int i,nJoints;
			pe_params_structural_joint psj;
      psj.bReplaceExisting = true; 
			if (m_pStructure)
				m_pStructure->nJoints = 0;
			ser.Value("nJoints", nJoints);
			for(i=0;i<nJoints;i++) {
				ser.BeginGroup("joint");
				ser.Value("id",psj.id);
				ser.Value("ipart0",psj.partid[0]); 
				ser.Value("ipart1",psj.partid[1]); 
				ser.Value("pt",psj.pt);
				ser.Value("n",psj.n);
				ser.Value("maxForcePush",psj.maxForcePush);
				ser.Value("maxForcePull",psj.maxForcePull);
				ser.Value("maxForceShift",psj.maxForceShift);
				ser.Value("maxTorqueBend",psj.maxTorqueBend);
				ser.Value("maxTorqueTwist",psj.maxTorqueTwist);
				ser.Value("bBreakable",psj.bBreakable);
				psj.bConstraintWillIgnoreCollisions = psj.bBreakable>>1^1;
				psj.bDirectBreaksOnly = psj.bBreakable>>2;
				psj.bBreakable &= 1;
				ser.Value("szHelper",psj.szSensor);
				ser.Value("axisx",psj.axisx);
				ser.Value("limitConstr",psj.limitConstraint);
				ser.Value("dampingConstr",psj.dampingConstraint);
				SetParams(&psj);
				ser.EndGroup();
			}
			if (m_pStructure)
				m_pStructure->bModified = 1;
			string str; ser.Value("usedParts", str);
			if (str.size()) {
				for(i=0;i<m_nParts;i++) {
					m_parts[i].flags |= geom_will_be_destroyed;
					if (m_pStructure)	m_pStructure->pParts[i].flags0 |= geom_will_be_destroyed;
				}
				const char *ptr=str.c_str()+1, *ptr1;
				for(;*ptr;ptr=ptr1+1) {
					int id = strtoul(ptr,(char**)&ptr1,16);
					for(i=0;i<m_nParts;i++) {
						int mask = ~geom_will_be_destroyed | ~-iszero(m_parts[i].id-id);
						m_parts[i].flags &= mask; 
						if (m_pStructure)	m_pStructure->pParts[i].flags0 &= mask;
					}
				}
				if (m_pStructure)	for(i=0;i<m_nParts;i++) if (m_parts[i].flags & geom_will_be_destroyed && m_pStructure->pParts[i].iParent && m_parts[i].mass)
					RemoveBrokenParent(m_pStructure->pParts[i].iParent-1, 0);
				for(i=m_nParts-1;i>=0;i--) if (m_parts[i].flags & (geom_will_be_destroyed|geom_removed))
					RemoveGeometry(m_parts[i].id);
			}
		}

		bVal = false; 
		ser.Value("hasDfm", bVal, 'bool');
		if (m_pStructure && m_pStructure->defparts && bVal)
			for(int i=0;i<m_nParts;i++) if (m_pStructure->defparts[i].pSkelEnt)	{
				m_pStructure->defparts[i].pSkelEnt->SetStateFromSnapshot(ser,flags);
				if (((CSoftEntity*)m_pStructure->defparts[i].pSkelEnt)->m_bMeshUpdated)
					UpdateDeformablePart(i);
			}
	}
	return 1; 
}


int CPhysicalEntity::TouchesSphere(const Vec3 &center, float r)
{
	ReadLock lock(m_lockUpdate);
	int i;
	geom_world_data gwd[2];
	intersection_params ip;
	sphere sph;
	geom_contact *pcont;
	CSphereGeom sphGeom;
	sph.center.zero(); sph.r=r;
	sphGeom.CreateSphere(&sph);
	gwd[0].offset = center;
	ip.bStopAtFirstTri=ip.bNoBorder=ip.bNoAreaContacts = true;

	for(i=0;i<m_nParts;i++) if (m_parts[i].flags & (geom_colltype_solid|geom_colltype_ray)) {
		gwd[1].offset=m_pos+m_qrot*m_parts[i].pos; gwd[1].R=Matrix33(m_qrot*m_parts[i].q); gwd[1].scale=m_parts[i].scale;
		if (sphGeom.Intersect(m_parts[i].pPhysGeomProxy->pGeom, gwd,gwd+1, &ip, pcont))
			return i;
	}
	return -1;
}

struct caps_piece {
	capsule caps;
	Vec3 pos;
	quaternionf q;
};

CTriMesh *g_pSliceMesh = 0;

int CPhysicalEntity::CapsulizePart(int ipart)
{
	IGeometry *pGeom;
	caps_piece pieceBuf[16],*pieces=pieceBuf;
	pe_geomparams gp;
	int i,j,nSlices,nParts=0;

	{ ReadLock lock(m_lockUpdate);
		if (ipart>=m_nParts || (pGeom = m_parts[ipart].pPhysGeom->pGeom)->GetPrimitiveCount()<50 || m_pWorld->m_vars.approxCapsLen<=0)
			return 0;

		if (!g_pSliceMesh) {
			static Vec3 vtx[] = { Vec3(-1,-1,0), Vec3(1,-1,0), Vec3(1,1,0), Vec3(-1,1,0) };
			static index_t indices[] = { 0,1,2, 0,2,3 };
			g_pSliceMesh = (CTriMesh*)m_pWorld->CreateMesh(vtx,	indices,0,0, 2, mesh_shared_idx|mesh_shared_vtx|mesh_no_vtx_merge|mesh_SingleBB, 0);
		}
		int iax,icnt,nLastParts=0,nNewParts,nPartsAlloc=CRY_ARRAY_COUNT(pieceBuf);
		Vec3 axis,axcaps,ptc,ptcPrev,ptcCur;
		quotientf t,dist,mindist;
		float step,r0;
		box bbox;
		capsule caps;
		geom_world_data gwd;
		intersection_params ip;

		ip.bNoAreaContacts = true;
		ip.maxUnproj = 0.01f;
		geom_contact *pcont;
		pGeom->GetBBox(&bbox);
		axis = bbox.Basis.GetRow(iax=idxmax3(bbox.size));
		nSlices = min(m_pWorld->m_vars.nMaxApproxCaps+1, float2int(bbox.size[iax]*2/m_pWorld->m_vars.approxCapsLen-0.5f)+2);
		if (nSlices<4)
			return 0;
		axis *= sgnnz(axis[idxmax3(axis.abs())]);
		axcaps.zero()[iax] = 1;
		gwd.R = bbox.Basis.T()*Matrix33::CreateRotationV0V1(Vec3(0,0,1),axcaps);
		gwd.scale = max(bbox.size[inc_mod3[iax]],bbox.size[dec_mod3[iax]])*1.1f;
		gwd.offset = bbox.center-axis*(bbox.size[iax]*0.9f);
		caps.center.zero();
		caps.axis.Set(0,0,1);
		gp.flagsCollider = m_parts[ipart].flagsCollider;
		gp.flags = m_parts[ipart].flags & geom_colltype_solid;
		gp.density = gp.mass = 0;
		gp.surface_idx = m_parts[ipart].surface_idx;
		if (m_parts[ipart].pMatMapping)
			gp.surface_idx = m_parts[ipart].pMatMapping[gp.surface_idx];
		gp.idmatBreakable = -2-m_parts[ipart].id;
		//gp.scale = m_parts[ipart].scale;

		for(i=0;;i++) {
			if (pGeom->Intersect(g_pSliceMesh, 0,&gwd, &ip,pcont)) {
				if (!pcont[0].nborderpt)
					return 0;
				float r,rmin;
				for(j=0,r0=0,rmin=bbox.size[iax]; j<pcont[0].nborderpt; j++) {
					r0 += (r=(pcont[0].ptborder[j]-pcont[0].center).len());
					rmin = min(rmin,r);
				}
				r0 /= pcont[0].nborderpt;
				if (r0-rmin > r0*0.4f && i==0)
					gwd.offset += axis*(bbox.size[iax]*0.9f);
				else {
					gwd.offset = bbox.center-axis*(bbox.size[iax]-r0);
					step = (bbox.size[iax]*2-r0-m_pWorld->m_vars.maxContactGap*5)/(nSlices-1);
					r0 *= m_parts[ipart].scale;
					break;
				}
			} else
				return 0;
		} 

		for(i=0; i<nSlices; i++,gwd.offset+=axis*step) {
			if (!(icnt=pGeom->Intersect(g_pSliceMesh, 0,&gwd, &ip,pcont)))
				break;
			nNewParts = 0;
			if (!i)	{
				for(--icnt,ptcPrev.zero(); icnt>=0; i+=pcont[icnt--].nborderpt) for(j=0; j<pcont[icnt].nborderpt; j++) 
					ptcPrev += pcont[icnt].ptborder[j];
				if (i>0)
					ptcPrev /= i;
				else
					ptcPrev = pcont[0].center;
				ptcPrev = m_parts[ipart].q*ptcPrev*m_parts[ipart].scale;
				i = 0;
			} else for(--icnt,nNewParts=0; icnt>=0; icnt--) {
				if (!pcont[icnt].bClosed)
					continue; // failure
				ptcCur = m_parts[ipart].q*pcont[icnt].center*m_parts[ipart].scale;
				for(j=nParts-nNewParts-nLastParts,mindist.set(1,0); j<nParts-nNewParts; j++) {
					axcaps = pieces[j].q*Vec3(0,0,1);
					ptc = pieces[j].pos+axcaps*(pieces[j].caps.hh*gp.scale);
					t.set((m_parts[ipart].pos+m_parts[ipart].q*gwd.offset*m_parts[ipart].scale-ptc)*axcaps, axcaps*axis);
					dist.set(((ptc-ptcCur)*t.y+axcaps*t.x).len2(), t.y*t.y);
					if (dist<mindist) {
						mindist=dist; ptcPrev=ptc;
					}
				}
				for(j=0,caps.r=0; j<pcont[icnt].nborderpt; j++) 
					caps.r += (pcont[icnt].ptborder[j]-pcont[icnt].center).len();
				caps.r = caps.r/pcont[icnt].nborderpt*m_parts[ipart].scale;
				if (i==1 && fabs_tpl(caps.r-r0)<min(caps.r,r0)*0.5f)
					caps.r = r0;
				caps.hh = (ptcCur-ptcPrev).len()*0.5f;
				if (caps.hh==0)
					continue;
				gp.pos = (ptcPrev+ptcCur)*0.5f;
				axcaps = (ptcCur-ptcPrev)/(caps.hh*2);
				gp.q = Quat::CreateRotationV0V1(Vec3(0,0,1), axcaps);
				/*if (i==1)	{
					caps.hh-=caps.r*0.5f; 
					gp.pos += axcaps*(caps.r*0.5f);
				}*/
				if (nParts==nPartsAlloc)
					if (pieces==pieceBuf)
						memcpy(pieces=new caps_piece[nPartsAlloc+=4], pieceBuf, nParts*sizeof(caps_piece));
					else
						ReallocateList(pieces, nParts, nPartsAlloc+=4);
				pieces[nParts].caps = caps;
				pieces[nParts].pos = gp.pos; 
				pieces[nParts].q = gp.q;
				nParts++; nNewParts++;
			}
			nLastParts = nNewParts;
		}
	}

	if (i<nSlices)
		return 0;
	float V0=m_parts[ipart].pPhysGeom->V*cube(m_parts[ipart].scale), V1=0;
	for(j=0;j<nParts;j++) 
		V1 += g_PI*sqr(pieces[j].caps.r)*pieces[j].caps.hh*2*cube(gp.scale);
	if (fabs_tpl(V0-V1)>max(V0,V1)*0.2f)
		return 0;
	for(j=0;j<nParts;j++) {
		gp.pos = pieces[j].pos; gp.q = pieces[j].q;
		pieces[j].caps.r = max(0.03f, pieces[j].caps.r);
		phys_geometry *pgeom = m_pWorld->RegisterGeometry(m_pWorld->CreatePrimitive(capsule::type, &pieces[j].caps));
		pgeom->pGeom->Release();
		if (AddGeometry(pgeom, &gp)<0)
			goto exitcp;
		CryInterlockedDecrement((volatile int*) &pgeom->nRefCount);
	}
	m_parts[ipart].flags &= ~geom_colltype_solid;
	m_parts[ipart].flagsCollider = 0;
	exitcp:
	if (pieces!=pieceBuf)
		delete[] pieces;
	return j;
}


bool CPhysicalEntity::MakeDeformable(int ipart, int iskel, float r)
{
	if (((CGeometry*)m_parts[iskel].pPhysGeom->pGeom)->IsAPrimitive())
		return false;
	int i,j,bPrimitive=((CGeometry*)m_parts[ipart].pPhysGeom->pGeom)->IsAPrimitive();
	geom_world_data gwd[2];
	float thickness=0,r0;
	mesh_data *md[2];
	Vec3 vtx[4],n;
	geom_contact *pcontact;
	box bbox;
	m_parts[ipart].pPhysGeom->pGeom->GetBBox(&bbox);
	sphere sph;	sph.center.zero(); sph.r=r0= r>0.0f ? r:min(min(bbox.size.x,bbox.size.y),bbox.size.z)*2.0f*m_parts[ipart].scale;
	CSphereGeom sphGeom; sphGeom.CreateSphere(&sph);
	SSkelInfo *pski;

	if (!(m_parts[ipart].flags & geom_can_modify))
		if (!bPrimitive)
			m_pWorld->ClonePhysGeomInEntity(this, ipart, m_pWorld->CloneGeometry(m_parts[ipart].pPhysGeom->pGeom));
		else
			m_parts[ipart].pPhysGeom->pGeom->SetForeignData(0,0);
	if (!(m_parts[iskel].flags & geom_can_modify))
		m_pWorld->ClonePhysGeomInEntity(this, iskel, m_pWorld->CloneGeometry(m_parts[iskel].pPhysGeom->pGeom));

	AllocStructureInfo();
	if (!m_pStructure->defparts)
		memset(m_pStructure->defparts = new SSkelInfo[m_nParts], 0, sizeof(SSkelInfo)*m_nParts);
	md[0] = (mesh_data*)m_parts[ipart].pPhysGeom->pGeom->GetData();
	md[1] = (mesh_data*)m_parts[iskel].pPhysGeom->pGeom->GetData();
	gwd[1].offset=m_parts[iskel].pos; gwd[1].R=Matrix33(m_parts[iskel].q); gwd[1].scale=m_parts[iskel].scale;
	pski = m_pStructure->defparts+ipart;
	if (!bPrimitive) {
		m_pStructure->defparts[ipart].pSkinInfo = new SSkinInfo[md[0]->nVertices];
		for(i=0; i<md[0]->nVertices; i++) {
			gwd[0].offset = m_parts[ipart].pos + m_parts[ipart].q*md[0]->pVertices[i]*m_parts[ipart].scale;
			if ((sphGeom.m_Tree.m_Box.size=Vec3(sphGeom.m_sphere.r=r0),
					 sphGeom.Intersect(m_parts[iskel].pPhysGeom->pGeom, gwd,gwd+1, 0, pcontact)) ||
					(sphGeom.m_Tree.m_Box.size=Vec3(sphGeom.m_sphere.r=r0*3),
					 sphGeom.Intersect(m_parts[iskel].pPhysGeom->pGeom, gwd,gwd+1, 0, pcontact)))
			{
				pski->pSkinInfo[i].itri = pcontact->iPrim[1];
				for(j=0;j<3;j++) 
					vtx[j] = m_parts[iskel].pos + m_parts[iskel].q*md[1]->pVertices[md[1]->pIndices[pcontact->iPrim[1]*3+j]]*m_parts[iskel].scale;
				n = m_parts[iskel].q*md[1]->pNormals[pcontact->iPrim[1]];
				vtx[3] = gwd[0].offset	- n*(pski->pSkinInfo[i].w[3]=n*(gwd[0].offset-vtx[0]));
				n = vtx[1]-vtx[0]^vtx[2]-vtx[0]; n *= 1.0f/n.len2();
				for(j=0;j<3;j++)
					pski->pSkinInfo[i].w[j] = (vtx[inc_mod3[j]]-vtx[3]^vtx[dec_mod3[j]]-vtx[3])*n;
				thickness = max(thickness, fabs_tpl(pski->pSkinInfo[i].w[3]));
			}	else
				pski->pSkinInfo[i].itri = -1;	
		}
	}	else {
		m_pStructure->defparts[ipart].pSkinInfo = new SSkinInfo[1];
		thickness = 0.1f;
	}

	pe_action_attach_points aap;
	aap.piVtx = new int[md[1]->nVertices];
	gwd[0].R = Matrix33(!m_parts[ipart].q*m_parts[iskel].q);
	gwd[0].scale = 1.0f/m_parts[ipart].scale;
	gwd[0].offset = !m_parts[ipart].q*(m_parts[iskel].pos-m_parts[ipart].pos)*gwd[0].scale;
	gwd[0].scale *= m_parts[iskel].scale;
	for(i=aap.nPoints=0; i<md[1]->nVertices; i++) {
		aap.piVtx[aap.nPoints] = i;
		aap.nPoints += 1-m_parts[ipart].pPhysGeom->pGeom->PointInsideStatus(gwd[0].R*md[1]->pVertices[i]*gwd[0].scale+gwd[0].offset);
	}
	aap.pEntity = this;
	aap.partid = m_parts[ipart].id;

	pe_params_pos pp;
	pe_geomparams gp;
	pe_simulation_params sp;
	pe_params_softbody psb;
	pe_params_flags pf;
	pp.pos = m_pos; pp.q = m_qrot;
	pf.flagsAND = ~(pef_traceable|se_skip_longest_edges); pf.flagsOR = pef_invisible|pef_ignore_areas|sef_skeleton;
	psb.thickness = 0.0f;//thickness;
	for(i=1,j=m_parts[0].flags; i<m_nParts; j&=m_parts[i++].flags);
	if (j & geom_colltype0)
		psb.collTypes = ent_static|ent_rigid|ent_sleeping_rigid;
	else {
		psb.collTypes = 0; gp.flagsCollider = geom_colltype_obstruct|geom_colltype_foliage_proxy;
	}
	psb.maxSafeStep = 0.01f; 
	psb.impulseScale = 0.002f;
	psb.explosionScale = 0.005f;
	psb.shapeStiffnessNorm=10.0f; psb.shapeStiffnessTang=0.0f;
	psb.nMaxIters = 3;
	sp.gravity.zero();
	gp.pos = m_parts[iskel].pos; gp.q = m_parts[iskel].q; gp.scale = m_parts[iskel].scale;
	for(i=0,gp.mass=0.0f; i<md[1]->nTris; i++) 
		gp.mass += fabs_tpl(md[1]->pNormals[i]*
								(md[1]->pVertices[md[1]->pIndices[i*3+1]]-md[1]->pVertices[md[1]->pIndices[i*3]] ^
								 md[1]->pVertices[md[1]->pIndices[i*3+2]]-md[1]->pVertices[md[1]->pIndices[i*3]]));	
	if (m_parts[iskel].mass>1e-6f)
		gp.mass *= m_parts[iskel].mass; 
	gp.density = 1000.0f;
	m_pStructure->defparts[ipart].maxImpulse = 500.0f;
	psb.maxCollisionImpulse = 200.0f;
	pski->pSkelEnt = (CPhysicalEntity*)m_pWorld->CreatePhysicalEntity(PE_SOFT,&pp,0,0x5AFE);
	pski->pSkelEnt->SetParams(&pf);
	pski->pSkelEnt->SetParams(&sp);
	pski->pSkelEnt->SetParams(&psb);
	pski->pSkelEnt->AddGeometry(m_parts[iskel].pPhysGeom, &gp);
	pski->pSkelEnt->Action(&aap);
	pski->pSkelEnt->m_pForeignData = m_pForeignData;
	pski->pSkelEnt->m_iForeignData = m_iForeignData;
	delete[] aap.piVtx;
	RemoveGeometry(m_parts[iskel].id);
	m_pStructure->defparts[ipart].timeStep = 0.005f;
	m_pStructure->defparts[ipart].nSteps = 7;
	m_pStructure->defparts[ipart].lastUpdateTime = m_pWorld->m_timePhysics;
	m_pStructure->defparts[ipart].lastUpdatePos = m_pos;
	m_pStructure->defparts[ipart].lastUpdateq = m_qrot;
	m_parts[ipart].flags |= geom_monitor_contacts;
	return true;
}

void CPhysicalEntity::UpdateDeformablePart(int ipart)
{
	int i,j;
	mesh_data *md[2];
	Vec3 vtx[3],doffs;
	quaternionf dq;
	float dscale;
	SSkelInfo *psi = m_pStructure->defparts+ipart;

	dscale = m_parts[ipart].scale==1.0f ? 1.0f:1.0f/m_parts[ipart].scale;
	dq = !m_parts[ipart].q*!m_qrot*psi->pSkelEnt->m_qrot*psi->pSkelEnt->m_parts[0].q;
	doffs = !m_parts[ipart].q*(!m_qrot*(psi->pSkelEnt->m_qrot*psi->pSkelEnt->m_parts[0].pos+psi->pSkelEnt->m_pos-m_pos)-m_parts[ipart].pos)*dscale;
	dscale *= psi->pSkelEnt->m_parts[0].scale;

	if (!((CGeometry*)m_parts[ipart].pPhysGeom->pGeom)->IsAPrimitive()) {
		{ WriteLock lock(((CGeometry*)m_parts[ipart].pPhysGeom->pGeom)->m_lockUpdate);
			md[0] = (mesh_data*)m_parts[ipart].pPhysGeom->pGeom->GetData();
			md[1] = (mesh_data*)psi->pSkelEnt->m_parts[0].pPhysGeom->pGeom->GetData();

			for(i=0; i<md[0]->nVertices; i++) if (psi->pSkinInfo[i].itri>=0) {
				for(j=0,md[0]->pVertices[i].zero(); j<3; j++)
					md[0]->pVertices[i] += psi->pSkinInfo[i].w[j]*(vtx[j] = doffs + dq*md[1]->pVertices[md[1]->pIndices[psi->pSkinInfo[i].itri*3+j]]*dscale);
				md[0]->pVertices[i] += (vtx[1]-vtx[0]^vtx[2]-vtx[0]).normalized()*psi->pSkinInfo[i].w[3];
			}
			for(i=0; i<md[0]->nTris; i++)
				md[0]->pNormals[i] = (md[0]->pVertices[md[0]->pIndices[i*3+1]]-md[0]->pVertices[md[0]->pIndices[i*3]] ^
															md[0]->pVertices[md[0]->pIndices[i*3+2]]-md[0]->pVertices[md[0]->pIndices[i*3]]).normalized();
			((CTriMesh*)m_parts[ipart].pPhysGeom->pGeom)->RebuildBVTree();
		}
		if (m_iSimClass>0) {
			m_parts[ipart].pPhysGeom->pGeom->CalcPhysicalProperties(m_parts[ipart].pPhysGeom);
			RecomputeMassDistribution(ipart);
		}
	}

	EventPhysUpdateMesh epum;
	epum.pEntity=this; epum.pForeignData=m_pForeignData; epum.iForeignData=m_iForeignData;
	epum.iReason = EventPhysUpdateMesh::ReasonDeform;
	epum.partid = m_parts[ipart].id;
	epum.bInvalid = 0; epum.pLastUpdate = 0;
	epum.pMesh = m_parts[ipart].pPhysGeom->pGeom;
	epum.pMeshSkel = psi->pSkelEnt->m_parts[0].pPhysGeom->pGeom;
	epum.mtxSkelToMesh = Matrix34(Vec3(dscale),dq,doffs);
	m_pWorld->OnEvent(m_pWorld->m_vars.bLogStructureChanges+1,&epum);
}


CPhysicalPlaceholder *CPhysicalEntity::ReleasePartPlaceholder(int i) 
{
	CPhysicalPlaceholder *res = m_parts[i].pPlaceholder;
	m_parts[i].pPlaceholder->m_BBox[0] = Vec3(1e20f); // make sure rwi and geb don't notice it
	m_parts[i].pPlaceholder->m_BBox[1] = Vec3(-1e20f);
	int64 timeout = CryGetTicks()+m_pWorld->m_vars.ticksPerSecond*5;
	while(m_parts[i].pPlaceholder->m_bProcessed & 0xFFFF && CryGetTicks()<timeout); // make sure no external rwi/geb references it
	m_parts[i].pPlaceholder->m_pEntBuddy = 0;
	m_parts[i].pPlaceholder = 0;
	return res;
}

void CPhysicalEntity::RepositionParts()
{
	if (!(m_flags & pef_parts_traceable) || (unsigned int)m_iSimClass>2u)	{
		for(int i=0;i<m_nParts;i++) if (m_parts[i].pPlaceholder)
			m_pWorld->DestroyPhysicalEntity(ReleasePartPlaceholder(i),0,1);
		m_nUsedParts = 0;
		return;
	}
	if (!m_pUsedParts) {
		if (m_pHeap)
			m_pUsedParts = (int(*)[16])m_pHeap->Malloc(sizeof(int) * (MAX_PHYS_THREADS+1) * 16, "Used parts");
		else
			m_pUsedParts = new int[MAX_PHYS_THREADS+1][16];
		memset(m_pUsedParts, 0, sizeof(*m_pUsedParts));
	}

	pe_params_bbox pbb;
	pe_type type=GetType();
	for(int i=0;i<m_nParts;i++) {
		pbb.BBox[0]=m_parts[i].BBox[0]; pbb.BBox[1]=m_parts[i].BBox[1];
		if (!m_parts[i].pPlaceholder)
			(m_parts[i].pPlaceholder = (CPhysicalPlaceholder*)m_pWorld->CreatePhysicalPlaceholder(
																	type,0,m_pForeignData,m_iForeignData,-2-i))->m_pEntBuddy = this;
		m_parts[i].pPlaceholder->SetParams(&pbb);
		m_Extents.Clear();
	}
}


void CPhysicalEntity::AllocStructureInfo()
{
	if (!m_pStructure) {
		memset(m_pStructure = new SStructureInfo, 0, sizeof(SStructureInfo));
		m_pStructure->idpartBreakOrg = -1;
		memset(m_pStructure->pParts = new SPartInfo[m_nParts], 0, m_nParts*sizeof(SPartInfo)); 
		m_pStructure->pJoints = 0;
		m_pStructure->nPartsAlloc = m_nParts;
		for(int i=0;i<m_nParts;i++)
			m_parts[i].flags |= geom_monitor_contacts;
	}
}


int CPhysicalEntity::GenerateJoints()
{
	int i,j,i1,j1,ncont,ihead,itail,nFakeJoints,idGnd=-1,iCaller=get_iCaller();
	CPhysicalEntity **pentlist;
	geom_world_data gwd[2];
	intersection_params ip;
	geom_contact *pcontacts;
	pe_params_structural_joint psj;
	SStructuralJoint sampleJoint,*pJoint,*pFakeJoints;
	float rareaAvg;
	ptitem2d pt2d[64];
	edgeitem edge2d[64];
	box abox;

	ushort idxGnd[] = { 0,1,2 };
	Vec3 ptGnd[3];
	for(i=m_nParts-1;i>=0 && m_parts[i].mass>0;i--);
	if (i<0) {
		Vec3 g=m_pWorld->m_vars.gravity.normalized()*m_qrot, c=((m_BBox[1]+m_BBox[0])*0.5f-m_pos)*m_qrot, r,ptbott;
		float bott = 0;
		for(j=0;j<m_nParts;j++) {
			Vec3 gpart = g*m_parts[j].q;
			switch (m_parts[j].pPhysGeom->pGeom->GetType()) {
				case GEOM_BOX: { 
					const box *pbox = (const box*)m_parts[j].pPhysGeom->pGeom->GetData();
					Vec3 gloc = pbox->Basis*gpart, sz;
					for(i1=0;i1<3;i1++) sz[i1] = pbox->size[i1]*sgnnz(gloc[i1]);
					ptbott = pbox->center + sz*pbox->Basis;
				}	break;
				case GEOM_CAPSULE: {
					const capsule *pcaps = (const capsule*)m_parts[j].pPhysGeom->pGeom->GetData();
					ptbott = pcaps->center + pcaps->axis*(pcaps->hh*sgnnz(pcaps->axis*gpart)) + gpart*pcaps->r;
				}	break;
				case GEOM_CYLINDER: {
					const cylinder *pcyl = (const cylinder*)m_parts[j].pPhysGeom->pGeom->GetData();
					ptbott = pcyl->center + pcyl->axis*(pcyl->hh*sgnnz(pcyl->axis*gpart)) + (pcyl->axis^gpart).normalized()^pcyl->axis*pcyl->r;
				}	break;
				case GEOM_SPHERE: {
					const sphere *psph = (const sphere*)m_parts[j].pPhysGeom->pGeom->GetData();
					ptbott = psph->center+gpart*psph->r;
				}	break;
				case GEOM_TRIMESH: {
					const mesh_data *pmd = (const mesh_data*)m_parts[j].pPhysGeom->pGeom->GetData();
					for(j1=0,i1=1; i1<pmd->nTris*3; i1++) 
						j1 += (i1-j1)*isneg(pmd->pVertices[pmd->pIndices[j1]]*gpart-pmd->pVertices[pmd->pIndices[i1]]*gpart);
					ptbott = pmd->pVertices[pmd->pIndices[j1]];
				}	break;
			}
			bott = max(bott, (m_parts[j].q*ptbott*m_parts[j].scale+m_parts[j].pos-c)*g);
		}
		for(i=0,r.Set((m_BBox[1]-m_BBox[0]).len()*1.1f,0,0); i<3; i++,r=r.GetRotated(Vec3(0,0,1),-0.5f,sqrt3*0.5f)) ptGnd[i]=r;
		IGeometry *pMesh = m_pWorld->CreateMesh(ptGnd,idxGnd,0,0,1,mesh_shared_vtx|mesh_shared_idx|mesh_no_filter|mesh_no_vtx_merge|mesh_SingleBB,0);
		phys_geometry *pgeom = m_pWorld->RegisterGeometry(pMesh); pMesh->Release(); pgeom->nRefCount=0;	pgeom->V=1e10f;
		pe_geomparams gp;	gp.pos=c+g*bott; gp.q=Quat::CreateRotationV0V1(Vec3(0,0,-1),g);
		idGnd = AddGeometry(pgeom,&gp);
	}

	AllocStructureInfo();
	sampleJoint.maxForcePull=1e5f; sampleJoint.maxForcePush=4e5f; sampleJoint.maxForceShift=2e5f;
	sampleJoint.maxTorqueBend=1e5f; sampleJoint.maxTorqueTwist=2e5f;
	sampleJoint.size=0.05f;
	for(i=0,nFakeJoints=m_pStructure->nJoints; i<m_pStructure->nJoints; i++) if (m_pStructure->pJoints[i].bBroken) {
		SStructuralJoint jnt = m_pStructure->pJoints[i]; 
		m_pStructure->pJoints[i--] = m_pStructure->pJoints[m_pStructure->nJoints-1];
		m_pStructure->pJoints[--m_pStructure->nJoints] = jnt;
	}
	nFakeJoints-=m_pStructure->nJoints;
	pFakeJoints = new SStructuralJoint[max(1,nFakeJoints)];
	memcpy(pFakeJoints, m_pStructure->pJoints+m_pStructure->nJoints, nFakeJoints*sizeof(SStructuralJoint));
	for(i=0;i<nFakeJoints;i++) for(j=0;j<2;j++) {
		i1 = pFakeJoints[i].ipart[j]<0 ? 0 :
			GetMatId(m_parts[pFakeJoints[i].ipart[j]].pPhysGeom->pGeom->GetPrimitiveId(0,0), pFakeJoints[i].ipart[j]);
		(pFakeJoints[i].bBreakable<<=16) |= i1;
	}
	for(i=j=0,rareaAvg=1e-10f;i<m_nParts;i++) if (m_parts[i].mass>0) {
		m_parts[i].pPhysGeom->pGeom->GetBBox(&abox); j++;
		rareaAvg += min(min(abox.size.x*abox.size.y,abox.size.x*abox.size.z),abox.size.y*abox.size.z)*sqr(m_parts[i].scale); 
	}
	rareaAvg = j*0.25f/rareaAvg;
	psj.id = 1000;

	for(i=0;i<m_nParts;i++) if (m_parts[i].mass>0) {
		gwd[0].R = Matrix33(m_parts[i].q); 
		m_pWorld->GetEntitiesAround(m_parts[i].BBox[0],m_parts[i].BBox[1],pentlist,1<<m_iSimClass);
		for(int j0=GetUsedPartsCount(iCaller)-1; j0>=0; j0--) {
			j = GetUsedPart(iCaller,j0);
			if (m_parts[i].mass>m_parts[j].mass && m_parts[j].mass>0 || m_pStructure->pParts[j].bGroup)
				continue;
			for(i1=m_pStructure->nJoints-1; i1>=0; i1--) {
				int icode = m_pStructure->pJoints[i1].ipart[1]<<16 | m_pStructure->pJoints[i1].ipart[0];
				if (icode==(i<<16|j) || icode==(j<<16|i))
					break;
			}
			if (i1>=0 || i==j)
				continue;
			gwd[0].offset=m_parts[i].pos;
			gwd[1].offset=m_parts[j].pos; gwd[1].R=Matrix33(m_parts[j].q); 
			gwd[0].scale=m_parts[i].scale; gwd[1].scale=m_parts[j].scale;
			i1 = isneg(m_parts[j].pPhysGeom->V-m_parts[i].pPhysGeom->V);
			float dscale = 0.01f;
			gwd[i1].offset -= gwd[i1].R*m_parts[i+(j-i&-i1)].pPhysGeom->origin*(gwd[i1].scale*dscale);
			gwd[i1].scale *= 1+dscale;
			float areai,areaj,area;
			m_parts[i].pPhysGeom->pGeom->GetBBox(&abox); 
			areai = min(min(abox.size.x*abox.size.y,abox.size.x*abox.size.z),abox.size.y*abox.size.z)*sqr(m_parts[i].scale)*4; 	
			if (ncont=m_parts[i].pPhysGeom->pGeom->Intersect(m_parts[j].pPhysGeom->pGeom, gwd,gwd+1, &ip, pcontacts)) {
				psj.partid[0]=m_parts[i].id; psj.partid[1]=m_parts[j].id;	
				if (m_parts[j].mass>0) {
					m_parts[j].pPhysGeom->pGeom->GetBBox(&abox); 
					areaj = min(min(abox.size.x*abox.size.y,abox.size.x*abox.size.z),abox.size.y*abox.size.z)*sqr(m_parts[j].scale)*4;
				}	else
					areaj = areai;
				for(psj.pt.zero(),psj.n.zero(),i1=0,area=0; i1<ncont; i1++) {
					Vec3 axx=pcontacts[i1].n.GetOrthogonal().normalized(), axy=pcontacts[i1].n^axx, pt;
					int npt = min(pcontacts[i1].nborderpt, (int)(CRY_ARRAY_COUNT(pt2d)));
					for(j1=0; j1<npt; j1++)
						pt=pcontacts[i1].ptborder[j1]-pcontacts[i1].center, pt2d[j1].pt.set(axx*pt,axy*pt);
					if (!pcontacts[i1].bBorderConsecutive)
						npt = qhull2d(pt2d,npt,edge2d);
					else for(j1=0,edge2d[npt-1].next=edge2d,edge2d[npt-1].pvtx=pt2d+npt-1; j1<npt-1; j1++)
						edge2d[j1].pvtx=pt2d+j1, edge2d[j1].next=edge2d+j1+1;
					for(edgeitem *edge=edge2d+(j1=0); j1<npt; j1++,edge=edge->next)
						area += fabs_tpl(edge->next->pvtx->pt^edge->pvtx->pt);
					psj.pt += pcontacts[i1].center; psj.n -= pcontacts[i1].n;
				}
				if ((area*=0.5f) < min(areai,areaj)*0.1f)
					continue;
				psj.pt /= ncont; psj.n.normalize();
				int icodeMat = GetMatId(m_parts[i].pPhysGeom->pGeom->GetPrimitiveId(0,0),i)<<16 | 
											 GetMatId(m_parts[j].pPhysGeom->pGeom->GetPrimitiveId(0,0),j);
				int icodeMat1 = icodeMat>>16 | icodeMat<<16;
				for(i1=0,pJoint=&sampleJoint,ihead=5; i1<nFakeJoints; i1++) if (pFakeJoints[i1].id!=joint_impulse) {
					int icode = pFakeJoints[i1].ipart[1]<<16 | pFakeJoints[i1].ipart[0];
					if (icode==(i<<16|j) || icode==(j<<16|i)) {
						pJoint=pFakeJoints+i1; break;	// same two parts
					}
					if ((icode&0xFFFF)==i || (icode&0xFFFF)==j || (icode>>16)==i || (icode>>16)==j) {
						pJoint=pFakeJoints+i1; ihead=1;	continue; // either of the parts is the same
					}
					icode = 0;
					if (pFakeJoints[i1].ipart[0]>=0) 
						icode =	GetMatId(m_parts[pFakeJoints[i1].ipart[0]].pPhysGeom->pGeom->GetPrimitiveId(0,0), pFakeJoints[i1].ipart[0]);
					if (pFakeJoints[i1].ipart[1]>=0) 
						icode |=GetMatId(m_parts[pFakeJoints[i1].ipart[1]].pPhysGeom->pGeom->GetPrimitiveId(0,0), pFakeJoints[i1].ipart[1])<<16;
					if (ihead>2 && (pFakeJoints[i].bBreakable==icodeMat || pFakeJoints[i].bBreakable==(icodeMat>>16|icodeMat<<16)))
						pJoint=pFakeJoints+i1, ihead=2; // both mat.ids are the same
					else if (((pFakeJoints[i1].bBreakable&0xFFFF)==(icodeMat&0xFFFF) || (pFakeJoints[i1].bBreakable>>16)==(icodeMat>>16) ||
									  (pFakeJoints[i1].bBreakable>>16)==(icodeMat&0xFFFF) || (pFakeJoints[i1].bBreakable&0xFFFF)==(icodeMat>>16)) && ihead>3)
						pJoint=pFakeJoints+i1, ihead=3; // either of mat.ids is the same
					else if (ihead>4)
						pJoint=pFakeJoints+i1, ihead=4;	// just any sample joint
				}
				float s = min(10.0f,max(0.1f,area*rareaAvg));
				psj.maxForcePull=pJoint->maxForcePull*s; psj.maxForcePush=pJoint->maxForcePush*s; psj.maxForceShift=pJoint->maxForceShift*s;
				psj.maxTorqueBend=pJoint->maxTorqueBend*s; psj.maxTorqueTwist=pJoint->maxTorqueTwist*s;
				psj.szSensor=pJoint->size;
				SetParams(&psj); psj.id++;
			}
		}
	}

	float *pScale,*pScaleNorm,weight;
	memset(pScale=new float[m_pStructure->nJoints+1], 0, m_pStructure->nJoints*sizeof(float));
	memset(pScaleNorm=new float[m_pStructure->nJoints+1], 0, m_pStructure->nJoints*sizeof(float));
	++m_pWorld->m_vars.bLogLatticeTension;
	m_pStructure->bTestRun = 1;

	for(i=0; i<nFakeJoints; i++) if (pFakeJoints[i].id==joint_impulse) {
		j = pFakeJoints[i].ipart[0];
		Vec3 impulse = m_qrot*pFakeJoints[i].n*pFakeJoints[i].maxTorqueBend;
		m_pStructure->pParts[j].Pext += impulse;
		m_pStructure->pParts[j].Lext += m_qrot*(pFakeJoints[i].pt-m_parts[j].q*m_parts[j].pPhysGeom->origin*m_parts[j].scale-m_parts[j].pos) ^ impulse;
		UpdateStructure(0.01f,0,-1,m_pWorld->m_vars.gravity);
		if (g_nPartsAlloc < m_nParts || !m_pStructure->nJoints)
			break;

		quotientf maxtens(0.0f,1.0f);
		for(i1=0; i1<m_pStructure->nLastUsedJoints; i1++)
			maxtens = max(maxtens, g_jointsDbg[i1].tension);
		maxtens.x = sqrt_tpl(maxtens.val());
		for(i1=0;i1<m_nParts;i1++) g_parts[i1].bProcessed=0;
		g_parts[g_parts[ihead=0].idx = j].isle = 0;
		g_parts[-1].bProcessed = 1;
		for(g_parts[g_parts[ihead].idx].bProcessed=1,itail=ihead+1; ihead<itail; ihead++)	{
			for(j=g_parts[g_parts[ihead].idx].ijoint0,weight=1.0f/max(1e-10f,(float)g_parts[g_parts[ihead].idx].isle); j<g_parts[g_parts[ihead].idx+1].ijoint0; j++) {
				i1 = m_pStructure->pJoints[g_jointidx[j]].ipart[0] + m_pStructure->pJoints[g_jointidx[j]].ipart[1] - g_parts[ihead].idx;
				pScale[g_jointidx[j]]+=maxtens.x*weight; pScaleNorm[g_jointidx[j]]+=weight; 
				if (!g_parts[i1].bProcessed) {
					g_parts[itail++].idx=i1; g_parts[i1].bProcessed=1; g_parts[i1].isle=g_parts[g_parts[ihead].idx].isle+1;
				}
			}
			if (ihead+1==itail && itail<m_nParts) for(i1=0;i1<m_nParts;i1++) {
				g_parts[itail].idx=i1; itail += (g_parts[i1].bProcessed^1);
				g_parts[i1].bProcessed=1; g_parts[i1].isle=m_nParts;
			}
		}
	}
	--m_pWorld->m_vars.bLogLatticeTension;
	m_pStructure->bTestRun = 0;
	m_pWorld->UnmarkEntityAsDeforming(this);

	for(i=0; i<m_pStructure->nJoints; i++) if (pScaleNorm[i]>0) {
		float scale = pScale[i]/pScaleNorm[i];
		m_pStructure->pJoints[i].maxForcePull*=scale; m_pStructure->pJoints[i].maxForcePush*=scale; m_pStructure->pJoints[i].maxForceShift*=scale;
		m_pStructure->pJoints[i].maxTorqueBend*=scale; m_pStructure->pJoints[i].maxTorqueTwist*=scale;
	}
	delete[] pFakeJoints; delete[] pScale; delete[] pScaleNorm;
	RemoveGeometry(idGnd);
	m_pStructure->bModified = 1;
	return m_pStructure->nJoints;
}
