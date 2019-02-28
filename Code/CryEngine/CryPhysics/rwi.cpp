// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"

#include "bvtree.h"
#include "geometry.h"
#include "raybv.h"
#include "raygeom.h"
#include "physicalplaceholder.h"
#include "physicalentity.h"
#include "physicalworld.h"
#include "heightfieldgeom.h"
#include "geoman.h"


int CPhysicalEntity::RayTrace(SRayTraceRes&) { return 0; }
CPhysicalEntity *CPhysicalEntity::GetEntity() { return this; }

geom_contact g_RWIContacts[64];

#include "physicalworld.h"
#define PrepGeom(pGeom,iCaller) (pGeom)

const int FD_RWI_RECURSIVE = -999;

ILINE void MarkProcessed(volatile unsigned int& bProcessed, int mask)	{ AtomicAdd(&bProcessed, bProcessed & mask ^ mask); }
ILINE void UnmarkProcessed(volatile unsigned int& bProcessed, int mask)	{ AtomicAdd(&bProcessed, -(int)(bProcessed & mask)); }
ILINE void MarkSkipEnts(IPhysicalEntity* pSkipEnts[8],int nSkipEnts, int mask)
{
	for(int i=0;i<nSkipEnts;i++) {
		CPhysicalPlaceholder* pEnt = (CPhysicalPlaceholder*)pSkipEnts[i];
		if (pEnt) {
			MarkProcessed(pEnt->m_bProcessed, mask);
			if (pEnt->m_pEntBuddy)
				MarkProcessed(pEnt->m_pEntBuddy->m_bProcessed, mask);
		}
	}
}
ILINE void UnmarkSkipEnts(IPhysicalEntity* pSkipEnts[8],int nSkipEnts, int mask)
{
	for(int i=0;i<nSkipEnts;i++) {
		CPhysicalPlaceholder* pEnt = (CPhysicalPlaceholder*)pSkipEnts[i];
		if (pEnt) {
			UnmarkProcessed(pEnt->m_bProcessed, mask);
			if (pEnt->m_pEntBuddy)
				UnmarkProcessed(pEnt->m_pEntBuddy->m_bProcessed, mask);
		}
	}
}

bool ray_box_overlap2d(const Vec2 &org,const Vec2 &dir, const Vec2 &boxmin,const Vec2 &boxmax)
{
	Vec2 n(dir.y,-dir.x), center=(boxmin+boxmax)*0.5f, size=(boxmax-boxmin)*0.5f;
	return max(max(fabs_tpl(center.x-org.x-dir.x*0.5f) - (size.x+fabs_tpl(dir.x)*0.5f),
								 fabs_tpl(center.y-org.y-dir.y*0.5f) - (size.y+fabs_tpl(dir.y)*0.5f)),
								 fabs_tpl(n*(center-org))-fabs_tpl(n.x)*size.x-fabs_tpl(n.y)*size.y) <= 0;
}
 
struct entity_grid_checker {
	geom_world_data gwd;
	intersection_params ip;
	int nMaxHits,nThroughHits,nThroughHitsAux,objtypes,nEnts,bCallbackUsed,nParts;
	unsigned int flags,flagsColliderAll,flagsColliderAny;
	SCollisionClass collclass;
	Vec2 org2d,dir2d;
	float dir2d_len,maxt;
	ray_hit *phits;
	CPhysicalWorld *pWorld;
	SEntityGrid *pgrid;
	CPhysicalPlaceholder *pGridEnt;
	CPhysicalEntity **pTmpEntList;
	int szList;
	void *pSkipForeignData;
	int iSkipForeignData;
	int iCaller;
	int bUsePhysOnDemand;
	CRayGeom aray;
	pe_gridthunk *pThunkSubst,thunkSubst;
	int ipartSubst,ipartMask;
	int iSolidNode;
  int iPartSubst_initial,iPartSubst_inFlight; 
	IPhysicalEntity **pSkipEnts;
	int nSkipEnts;
	IPhysicalEntity **pPortals;
	int nPortals, nMaxPortals;
	const char *pNameTag;
	void sync_from(const entity_grid_checker &egc) { nThroughHits=egc.nThroughHits; nThroughHitsAux=egc.nThroughHitsAux; }

	entity_grid_checker() 
		: nMaxHits(0)
		, nThroughHits(0)
		, nThroughHitsAux(0)
		, objtypes(0)
		, nEnts(0)
		, bCallbackUsed(0)
		, nParts(0)
		, flags(0)
		, flagsColliderAll(0)
		, flagsColliderAny(0)
		, org2d(ZERO)
		, dir2d(ZERO)
		, dir2d_len(0.0f)
		, maxt(0.0f)
		, phits(nullptr)
		, pWorld(nullptr)
		, pGridEnt(nullptr)
		, pTmpEntList(nullptr)
		, szList(0)
		, pSkipForeignData(nullptr)
		, iSkipForeignData(0)
		, iCaller(0)
		, bUsePhysOnDemand(0)
		, pThunkSubst(nullptr)
		, ipartSubst(0)
		, ipartMask(0)
		, iSolidNode(0)
		, iPartSubst_initial(0)
		, iPartSubst_inFlight(0)
		, pSkipEnts(nullptr)
		, nSkipEnts(0)
		, pPortals(nullptr)
		, nPortals(0)
		, nMaxPortals(1)
		, pNameTag(nullptr)
	{
		ZeroStruct(thunkSubst);
	}

	NO_INLINE int check_cell(const Vec2i &icell, int &ilastcell) {

		quotientf t((org2d+icell)*dir2d, dir2d_len*dir2d_len);
		if (t.x>maxt && (icell.x&icell.y)!=-1)
			return 1;		
		box bbox;
		bbox.Basis.SetIdentity();
		bbox.bOriented = 0;
		geom_contact *pcontacts = 0;
    geom_world_data *dummy = NULL;
		pe_gridthunk *pthunk;
		int ithunk,ithunk_next;
		pe_PODcell *pPODcell;
    IGeometry *pGeom = NULL; 
		int i,j,ihit,imat,nCellEnts=0,nEntsChecked=0,bRecheckOtherParts,bNoThunkSubst;
		pgrid->GetPODGridCellBBox(icell.x,icell.y, bbox.center,bbox.size);
		if (bUsePhysOnDemand && (bbox.size.z>0.f) && box_ray_overlap_check(&bbox,&aray.m_ray)) {
			AtomicAdd(&pWorld->m_lockGrid,-1);
			ReadLock lockPOD(pWorld->m_lockPODGrid);
			if ((pPODcell=pgrid->getPODcell(icell.x,icell.y))->lifeTime<=0) {
				AtomicAdd(&pWorld->m_lockPODGrid,-1);
				{ WriteLock lockPODw(pWorld->m_lockPODGrid);
					if (pPODcell->lifeTime<=0) {
						MarkAsPODThread(pWorld);
						pWorld->m_nOnDemandListFailures=0; ++pWorld->m_iLastPODUpdate;
						if (pWorld->m_pPhysicsStreamer && pWorld->m_pPhysicsStreamer->CreatePhysicalEntitiesInBox(bbox.center-bbox.size,bbox.center+bbox.size)) {	
							pPODcell->lifeTime = pWorld->m_nOnDemandListFailures ? 0.001f:8.0f;
							pPODcell->inextActive = pgrid->iActivePODCell0;
							pgrid->iActivePODCell0 = icell.y<<16|icell.x;
							szList = max(szList, pWorld->GetTmpEntList(pTmpEntList, iCaller));
						}
						pWorld->m_nOnDemandListFailures=0;
					}
				} ReadLockCond lockPODr(pWorld->m_lockPODGrid,1); lockPODr.SetActive(0);
			}	else
				pPODcell->lifeTime = max(8.0f,pPODcell->lifeTime);
			UnmarkAsPODThread(pWorld);
			ReadLockCond relock(pWorld->m_lockGrid,1); relock.SetActive(0);
		}
		//fetch some memory references to avoid reloads
		primitives::grid& RESTRICT_REFERENCE entgrid = *pgrid;
		const Vec3 entgrid_origin			= entgrid.origin;
		const int icellX = icell.x, icellY = icell.y;
		const Vec2 entgrid_step	= entgrid.step;
		const Vec2 entgrid_stepr	= entgrid.stepr;
		pe_entgrid pEntGrid = pgrid->cells;
		pe_gridthunk *const __restrict pgthunks = pWorld->m_gthunks;
		const int gridIdx = entgrid.getcell_safe(icellX,icellY);
		const float pWorld_m_zGran = pgrid->zGran;
		const int pWorld_m_matWater = pWorld->m_matWater;
    /*const uint32 simClass[] = {1<<0,1<<1,1<<2,1<<3,1<<4,1<<5,1<<6,1<<7,1<<8,1<<9};*/

		int ithunk_prev = 0;
		pthunk = pgthunks+(ithunk = pEntGrid[gridIdx]);
		bNoThunkSubst = iszero_mask(pThunkSubst);
		pthunk = (pe_gridthunk*)((intptr_t)pthunk + ((intptr_t)pThunkSubst-(intptr_t)pthunk & ~bNoThunkSubst));

		for(; ithunk; pthunk=pgthunks+(ithunk=ithunk_next)) {
      const pe_gridthunk& thunk = *pthunk;
      
			ithunk_next = thunk.inext;
			ithunk_prev = ithunk;
			PrefetchLine(thunk.pent, 0);	//We tend to L2 cache miss on access to thunk.iSimClass anyway, so this prefetch will actually help - Rich S

			if ((objtypes & 1<<thunk.iSimClass)
				&& !(thunk.pent->m_bProcessed & 1<<iCaller)
				&& (!pSkipForeignData || (CPhysicalPlaceholder*)thunk.pent->CPhysicalPlaceholder::GetForeignData(iSkipForeignData)!=pSkipForeignData))
			{				
				Diag33 gscale(entgrid_step.x*(1.0f/256),entgrid_step.y*(1.0f/256),pWorld_m_zGran);
				Vec3 gBBox[2], cellOffs(icell.x*256,icell.y*256,0); 
				int iz0 = thunk.BBoxZ0, iz1 = thunk.BBoxZ1;
				iz1 += isneg(iz1-iz0)<<16;
				pgrid->BBoxFromGrid(gscale*(Vec3(thunk.BBox[0],thunk.BBox[1],iz0)+cellOffs), gscale*(Vec3(thunk.BBox[2]+1,thunk.BBox[3]+1,iz1)+cellOffs), gBBox);
				bbox.center = (gBBox[1]+gBBox[0])*0.5f;
				bbox.size = (gBBox[1]-gBBox[0])*0.5f;
				i = pgrid->iup;
				bbox.center[i] += float2int((aray.m_ray.origin[i]-bbox.center[i])*pgrid->rzGran*(1.0f/65536))*pgrid->zGran*65536;

				if(!(entgrid.inrange(icell.x,icell.y)&-bNoThunkSubst) || box_ray_overlap_check(&bbox,&aray.m_ray)) {
					ReadLock lock(thunk.pent->m_lockUpdate);
					bbox.center = (thunk.pent->m_BBox[0]+thunk.pent->m_BBox[1])*0.5f;
					bbox.size = (thunk.pent->m_BBox[1]-thunk.pent->m_BBox[0])*0.5f;
					nCellEnts++;

					/*if ((bbox.center-aray.m_ray.origin-aray.m_dirn*((bbox.center-aray.m_ray.origin)*aray.m_dirn)).len2() > bbox.size.len2())
					continue; // skip objects that lie to far from the ray
					if ((box_ray_overlap_check(&bbox,&aray.m_ray) & nEnts-pWorld->m_nEnts>>31)==0)
					continue;*/
					if (nEnts>=szList)
						szList = pWorld->ReallocTmpEntList(pTmpEntList,iCaller,szList+1024); 
					nEntsChecked++;	
					CPhysicalEntity *pent,*pentLog,*pentFlags,*pentList;
					bCallbackUsed=bRecheckOtherParts = 0;

					if (thunk.pent->m_iSimClass==5) {
						if (((CPhysArea*)thunk.pent)->m_pb.iMedium!=0)
							continue;
						ray_hit ahit;
						CPhysArea	*pPhysArea = (CPhysArea*)thunk.pent;
						if (pPhysArea->RayTrace(aray.m_ray.origin,aray.m_ray.dir,&ahit)) {
							pcontacts = g_RWIContacts;
							pentFlags = &g_StaticPhysicalEntity;
							pentFlags->m_parts[0].flags = geom_colltype_ray & -iszero((int)flags & rwi_force_pierceable_noncoll);
							pcontacts->t = ahit.dist;
							pcontacts->pt = ahit.pt; 
							pcontacts->n = ahit.n;
							pcontacts->id[0] = pWorld_m_matWater;
							pcontacts->iNode[0] = -1;
							pentList = pent = (CPhysicalEntity*)(pGridEnt = thunk.pent);
							i=0; j=1; nParts=0; pentLog=0; goto gotcontacts;
						}
						continue;
					}

					//This is causing a load hit store on the branch two lines down. Safe to remove? Suspect not due to GetEntity() doing phys on demand and causing a reposition
					//	call that could set m_bGridThunksChanged. Damn. Could it be set earlier...?
					pWorld->m_bGridThunksChanged = 0;	
					pentList = pentLog = pentFlags = pent = (pGridEnt=thunk.pent)->GetEntity();
					if (pWorld->m_bGridThunksChanged) 
						ithunk_next=pEntGrid[gridIdx], ithunk_prev=0;
					pWorld->m_bGridThunksChanged = 0;
					IF (pent->m_iDeletionTime, 0)
						continue;
					
					if (IgnoreCollision(pent->m_collisionClass, collclass))
						continue;

					if ((nParts=pent->m_nParts)==0 || pent->m_flags&pef_use_geom_callbacks) {
						SRayTraceRes rtr(&aray,pcontacts);
						j = pent->RayTrace(rtr);
						pcontacts = rtr.pcontacts;
						i=0; bCallbackUsed=1; goto gotcontacts;
					}

					if (pent!=pGridEnt && pent->m_pEntBuddy!=pGridEnt) {
						iPartSubst_inFlight = 
							ipartSubst = -2-pGridEnt->m_id; ipartMask = -1;	nParts = 1;
						pentList = (CPhysicalEntity*)pGridEnt;
					}

					for(i=ipartSubst; i<nParts+(ipartSubst+1-nParts & ipartMask); i++) {
						if ((pent->m_parts[i].flags & flagsColliderAll)==flagsColliderAll && (pent->m_parts[i].flags & flagsColliderAny)) {
							if (nParts>1) {
								bbox.center = (pent->m_parts[i].BBox[0]+pent->m_parts[i].BBox[1])*0.5f;
								bbox.size = (pent->m_parts[i].BBox[1]-pent->m_parts[i].BBox[0])*0.5f;
								if (!box_ray_overlap_check(&bbox,&aray.m_ray))
									continue;
							}
							gwd.offset = pent->m_pos + pent->m_qrot*pent->m_parts[i].pos;
							//(pent->m_qrot*pent->m_parts[i].q).getmatrix(gwd.R);	//Q2M_IVO 
							gwd.R = Matrix33(pent->m_qrot*pent->m_parts[i].q);
							gwd.scale = pent->m_parts[i].scale;

							pGeom = PrepGeom(pent->m_parts[i].pPhysGeom->pGeom,iCaller);
							j = pGeom->Intersect(&aray, &gwd, dummy, &ip, pcontacts);

gotcontacts:
							bRecheckOtherParts = (j-1 & 1-nParts)>>31 & ipartMask;

							if (-j>>31 & IsPortal(pent)) {
								QuatT transGrid = pWorld->GetGrid(pent->m_pEntBuddy)->m_transW.GetInverted() * pWorld->GetGrid(pent)->m_transW;
								IPhysicalWorld::SRWIParams rp;
								pPortals[nPortals] = pent;
								nPortals = min(nPortals+1,nMaxPortals);
								rp.Init(transGrid*aray.m_ray.origin, transGrid.q*aray.m_ray.dir, objtypes & ~ent_terrain, flags, collclass, 
									phits,nMaxHits, (IPhysicalEntity**)&pent->m_pEntBuddy,1, this,FD_RWI_RECURSIVE, nullptr, pPortals+nPortals,nMaxPortals-nPortals);
								int nhits1 = pWorld->RayWorldIntersection(rp, pNameTag, iCaller);
								szList = max(szList, pWorld->GetTmpEntList(pTmpEntList, iCaller));
								for(; pPortals[nPortals]; nPortals++);
								transGrid.Invert();
								int noSolid = !phits[0].pCollider;
								for(int i1=noSolid; i1<nhits1+noSolid; i1++) if (pWorld->GetGrid(phits[i1].pCollider)==pWorld->GetGrid(pent->m_pEntBuddy)) {
									phits[i1].pt = transGrid*phits[i1].pt; phits[i1].n = transGrid.q*phits[i1].n;	
								}
								break;
							}

							check_cell_contact(j,pcontacts,phits,flags,pentFlags,nThroughHits,nThroughHitsAux, imat,ilastcell,aray,iSolidNode,pentLog,i,ihit);
						}	else if (pent->m_parts[i].flags & geom_mat_substitutor) {
							for(int ihit=!phits[0].pCollider; ihit<=nThroughHits+nThroughHitsAux; ihit++) 
								if (pent->m_parts[i].flagsCollider & (2<<((CPhysicalPlaceholder*)phits[ihit].pCollider)->m_iSimClass) - (((CPhysicalPlaceholder*)phits[ihit].pCollider)->m_id>>31) &&
									pent->m_parts[i].pPhysGeom->pGeom->PointInsideStatus(((phits[ihit].pt-pent->m_pos)*pent->m_qrot-pent->m_parts[i].pos)*pent->m_parts[i].q)) 
								{	phits[ihit].surface_idx = pent->GetMatId(pent->m_parts[i].pPhysGeom->surface_idx, i);
									phits[ihit].idmatOrg = -1;
								}
							bRecheckOtherParts = -1;
						}
					}

					pTmpEntList[nEnts] = pentList; nEnts += 1+bRecheckOtherParts;
					AtomicAdd(&pGridEnt->m_bProcessed, 1+bRecheckOtherParts<<iCaller);
					iPartSubst_inFlight = ipartMask = ipartSubst = 0;

					/*next_cell_ent:
					if (thunk.iSimClass==6 && thunk.pent->m_iForeignFlags==0x100) {
					float zlowest = max(phits[0].pt[pWorld_m_iEntAxisz], origin_grid.z*t.y + dir_grid.z*t.x - max_zcell;
					if (zlowest>thunk.pent->m_BBox[0][pWorld_m_iEntAxisz)
					goto return_res;
					}*/
				}
			}
		}

		return iszero((icell.y<<16|icell.x&0xFFFF)-ilastcell);
	}

	ILINE void check_cell_contact(int& j,geom_contact *&pcontacts, ray_hit *phits, unsigned int flags,
		CPhysicalEntity *pentFlags,int& nThroughHits, int& nThroughHitsAux, int& imat, int &ilastcell,CRayGeom& aray,
		int& iSolidNode, CPhysicalEntity *pentLog,int i, int& ihit)
	{ 
		primitives::grid& RESTRICT_REFERENCE entgrid = *pgrid;
		const Vec3 entgrid_origin	= entgrid.origin;
		const Vec2 entgrid_stepr	= entgrid.stepr;
		
		float facing;
		for(j--; j>=0; j--) 
		if (pcontacts[j].t<phits[0].dist && (flags & rwi_ignore_back_faces)*(facing=pcontacts[j].n*aray.m_dirn)<=0) {
			imat = pentFlags->GetMatId(pcontacts[j].id[0],i);
			int rayPierceability = (int)((flags & rwi_pierceability_mask)-(flags & rwi_max_piercing));
			int pierceability = pWorld->m_SurfaceFlagsTable[imat&NSURFACETYPES-1] & sf_pierceable_mask;
			ihit = -(int)(flags&rwi_force_pierceable_noncoll)>>31 & -iszero((int)pentFlags->m_parts[i].flags & (geom_colltype_solid|geom_colltype_ray));
			pierceability += sf_max_pierceable+1-pierceability & ihit;
			ihit = 0;
			if (rayPierceability < pierceability) {
				if ((pWorld->m_SurfaceFlagsTable[imat&NSURFACETYPES-1]|flags) & sf_important) {
					for(ihit=1; ihit<=nThroughHits && phits[ihit].dist<pcontacts[j].t; ihit++);
					if (ihit<=nThroughHits)	{
						for(int idx=min(nThroughHits+1,nMaxHits-1)-1; idx>=ihit; idx--)
							memcpy(phits+idx+1, phits+idx, sizeof(ray_hit)-sizeof(ray_hit*)); // don't touch the *next member
						//memmove(phits+ihit+1, phits+ihit, (min(nThroughHits+1,nMaxHits-1)-ihit)*sizeof(ray_hit));
					} else if (nThroughHits+1==nMaxHits)
						continue;
					nThroughHits = min(nThroughHits+1, nMaxHits-1);
					nThroughHitsAux = min(nThroughHitsAux, nMaxHits-1-nThroughHits);
				}	else {
					for(ihit=nMaxHits-1; ihit>=nMaxHits-nThroughHitsAux && phits[ihit].dist<pcontacts[j].t; ihit--);
					if (ihit>=nMaxHits-nThroughHitsAux) {
						int istart = max(nMaxHits-nThroughHitsAux-1,nThroughHits+1);
						for(int idx=istart; idx<ihit; idx++)
							memcpy(phits+idx, phits+idx+1, sizeof(ray_hit)-sizeof(ray_hit*));
						//memmove(phits+istart, phits+istart+1, (ihit-istart)*sizeof(ray_hit));
					} else if (nThroughHits+nThroughHitsAux>=nMaxHits-1)
						continue;
					nThroughHitsAux = min(nThroughHitsAux+1, nMaxHits-1-nThroughHits);
				}
			} else {
				if ((flags & rwi_ignore_solid_back_faces)*facing>0)
					continue;
				Vec3 gpt = pgrid->vecToGrid(pcontacts[j].pt-pgrid->origin);
				ilastcell = float2int(gpt.x*entgrid_stepr.x-0.5f) & 0xFFFF | float2int(gpt.y*entgrid_stepr.y-0.5f)<<16;
				aray.m_ray.dir = pcontacts[j].pt-aray.m_ray.origin;
				iSolidNode = pcontacts[j].iNode[0];
			}
			
			phits[ihit].dist = max(1e-8f,(float)pcontacts[j].t);
			phits[ihit].pCollider = pentLog; 
			phits[ihit].ipart = i+(pcontacts[j].iNode[0]-i & -bCallbackUsed);
			phits[ihit].partid = pcontacts[j].iPrim[0];
			phits[ihit].surface_idx = imat;
			phits[ihit].idmatOrg = pcontacts[j].id[0] + (pentFlags->m_parts[i].surface_idx+1 & pcontacts[j].id[0]>>31);
			phits[ihit].pt = pcontacts[j].pt; 
			phits[ihit].n = pcontacts[j].n;
			phits[ihit].iNode = pcontacts[j].iNode[0];
			phits[ihit].bTerrain = 0;
		}
	}
};

getSurfTypeCallback g_getSurfType;
unsigned char getSurfTypeNoHoles(int ix, int iy) { return max((int)(char)g_getSurfType(ix,iy),0);	}

void CPhysicalWorld::RayHeightfield(const Vec3 &org,Vec3 &dir, ray_hit *hits, int flags, int iCaller) 
{
	geom_world_data gwd;
	intersection_params ip;
	geom_contact *pcontacts;
	CRayGeom aray(org,dir);
	gwd.R = m_HeightfieldBasis.T();
	gwd.offset = m_HeightfieldOrigin;
  IGeometry* iGeom = PrepGeom(m_pHeightfield[iCaller]->m_parts[0].pPhysGeom->pGeom,iCaller);
	CHeightfield* hfGeom = (CHeightfield*)iGeom;
	g_getSurfType = hfGeom->m_hf.fpGetSurfTypeCallback;
	if (flags & rwi_ignore_terrain_holes) 
		hfGeom->m_hf.fpGetSurfTypeCallback = getSurfTypeNoHoles;
	if (hfGeom->CHeightfield::Intersect(&aray, &gwd, (geom_world_data*)0, &ip, pcontacts)) {
		dir = pcontacts->pt-org;
		hits[0].dist = max(1e-8f,(float)pcontacts->t);
		hits[0].pCollider = m_pHeightfield[iCaller]; 
		hits[0].partid = hits[0].ipart = 0;
		hits[0].surface_idx = m_pHeightfield[iCaller]->GetMatId(pcontacts->id[0],0);
		hits[0].idmatOrg = pcontacts->id[0];
		hits[0].pt = pcontacts->pt; 
		hits[0].n = pcontacts->n;
		hits[0].bTerrain = 1;
	}
	hfGeom->m_hf.fpGetSurfTypeCallback = g_getSurfType;
}

void CPhysicalWorld::RayWater(const Vec3 &org,const Vec3 &dir, entity_grid_checker &egc, int flags,int nMaxHits, ray_hit *hits)
{
	ReadLock lockAr(m_lockAreas);
	int i;
	quotientf t(1,0);//(m_pGlobalArea->m_pb.waterPlane.origin-org)*m_pGlobalArea->m_pb.waterPlane.n, dir*m_pGlobalArea->m_pb.waterPlane.n);
	CPhysArea *pArea = NULL,*pHitArea = NULL;
	ray_hit whit;
	Vec3 n = m_pGlobalArea->m_pb.waterPlane.n;
	box bbox;
	float l = egc.aray.m_ray.dir*egc.aray.m_dirn;
	bbox.Basis.SetIdentity();
	bbox.bOriented = 0;
	if (m_pGlobalArea->RayTrace(org,dir,&whit))
		t.set(whit.dist,l), n=whit.n, pHitArea=m_pGlobalArea;

	for(pArea=m_pGlobalArea->m_nextBig; pArea; pArea=pArea->m_nextBig) if (pArea->m_pb.iMedium==0) {
		const Vec3& bb0 = pArea->m_BBox[0]; 
		const Vec3& bb1 = pArea->m_BBox[1];
		bbox.center = (bb0+bb1)*0.5f;
		bbox.size = (bb1-bb0)*0.5f;
		if (box_ray_overlap_check(&bbox,&egc.aray.m_ray) && pArea->RayTrace(org,dir,&whit) && whit.dist*sqr(t.y)<=t.x*t.y*l) {
			t.set(whit.dist,l); n=whit.n, pHitArea=pArea;
		}
	}

	if (inrange(t.x, 0.0f,t.y)) {
		if ((uint)(flags & rwi_pierceability_mask) < (m_SurfaceFlagsTable[m_matWater] & sf_pierceable_mask)+((uint)flags & rwi_force_pierceable_noncoll)) {
			if (nMaxHits<=1)
				goto nowater;
			if ((m_SurfaceFlagsTable[m_matWater]|(flags^rwi_separate_important_hits)) & sf_important)
				i = egc.nThroughHits = 1;
			else
				i = nMaxHits-(egc.nThroughHitsAux=1);
		}	else
			i = 0;
		hits[i].dist = (t.x=t.val())*dir.len();
		hits[i].pCollider = pHitArea; 
		hits[i].partid = hits[0].ipart = 0;
		hits[i].surface_idx = m_matWater;
		hits[i].idmatOrg = 0;
		hits[i].pt = org+dir*t.x; 
		hits[i].n = n;
		hits[i].bTerrain = 0;
		nowater:;
	}
}

int CPhysicalWorld::RayWorldIntersection(const IPhysicalWorld::SRWIParams &rp, const char *pNameTag, int iCaller)
{
	ray_hit *hits = rp.hits;
	Vec3 dir = rp.dir;
	int objtypes = rp.objtypes;

	CRY_PROFILE_FUNCTION(PROFILE_PHYSICS );

	IF (rp.dir.len2()<=0, 0)
		return 0;

	IF (rp.flags & rwi_queue, 0) {
		WriteLock lockQ(m_lockRwiQueue);
		int i;
		ReallocQueue(m_rwiQueue, m_rwiQueueSz,m_rwiQueueAlloc, m_rwiQueueHead,m_rwiQueueTail, 64);
		m_rwiQueue[m_rwiQueueHead].pForeignData = rp.pForeignData;
		m_rwiQueue[m_rwiQueueHead].iForeignData = rp.iForeignData;
		m_rwiQueue[m_rwiQueueHead].org = rp.org;
		m_rwiQueue[m_rwiQueueHead].dir = rp.dir;
		m_rwiQueue[m_rwiQueueHead].objtypes = objtypes;
		m_rwiQueue[m_rwiQueueHead].flags = rp.flags & ~rwi_queue;
		m_rwiQueue[m_rwiQueueHead].phitLast = rp.phitLast;
		m_rwiQueue[m_rwiQueueHead].iCaller = iCaller;
		m_rwiQueue[m_rwiQueueHead].OnEvent = rp.OnEvent;
		if (!(m_rwiQueue[m_rwiQueueHead].hits = rp.hits)) {
			WriteLock lockH(m_lockRwiHitsPool);
			int nhits=0;
			ray_hit *phit=m_pRwiHitsTail->next,*pchunk=0;
			if (m_rwiPoolEmpty || phit!=m_pRwiHitsHead)
				for(nhits=1; nhits<rp.nMaxHits && (m_rwiPoolEmpty || phit!=m_pRwiHitsHead); nhits++,phit=phit->next,m_rwiPoolEmpty=0) 
					if (phit->next!=phit+1)
						pchunk=phit,nhits=0;
			if (nhits<rp.nMaxHits) {
				phit = new ray_hit[(nhits=max(rp.nMaxHits,512))+1]+1;
				memset(phit-1, 0, (nhits+1)*sizeof(ray_hit));
				for(i=0;i<nhits-1;i++) phit[i].next = phit+i+1;
				phit[nhits-1].next=m_pRwiHitsTail->next; m_pRwiHitsTail->next=phit;
				m_rwiHitsPoolSize += nhits;
			}	else
				phit = (pchunk ? pchunk:m_pRwiHitsTail)->next;
			m_pRwiHitsTail = phit+rp.nMaxHits-1; m_rwiPoolEmpty = 0;
			m_rwiQueue[m_rwiQueueHead].hits = phit;
			m_rwiQueue[m_rwiQueueHead].iCaller |= 1<<16;
		}
		m_rwiQueue[m_rwiQueueHead].nMaxHits = rp.nMaxHits;
		m_rwiQueue[m_rwiQueueHead].nSkipEnts = min((int)(sizeof(m_rwiQueue[0].idSkipEnts)/sizeof(m_rwiQueue[0].idSkipEnts[0])),rp.nSkipEnts);
		for(i=0;i<m_rwiQueue[m_rwiQueueHead].nSkipEnts;i++)
			m_rwiQueue[m_rwiQueueHead].idSkipEnts[i] = rp.pSkipEnts[i] ? GetPhysicalEntityId(rp.pSkipEnts[i]):-3;
		m_rwiQueueSz++;
		return 1;
	} 

	PHYS_FUNC_PROFILER( pNameTag );
	int i,nHits; 
	entity_grid_checker egc;
	egc.nThroughHits = egc.nThroughHitsAux = 0;
	CPhysicalEntity *pentLastHit = 0;
	int ipartLastHit,inodeLastHit;
	IF (rp.phitLast, 0) {
		pentLastHit = (CPhysicalEntity*)rp.phitLast->pCollider;
		ipartLastHit = rp.phitLast->ipart;
		inodeLastHit = rp.phitLast->iNode;
	}

	assert(iCaller<=MAX_PHYS_THREADS);
	WriteLockCond lock(m_lockCaller[iCaller], iCaller==MAX_PHYS_THREADS && rp.iForeignData!=FD_RWI_RECURSIVE);

	if (rp.iForeignData==FD_RWI_RECURSIVE) {
		egc.nEnts = ((entity_grid_checker*)rp.pForeignData)->nEnts;
		egc.sync_from(*(entity_grid_checker*)rp.pForeignData);
	} else {
		for(i=0;i<rp.nMaxHits;i++) { hits[i].dist=1E10; hits[i].bTerrain=0; hits[i].pCollider=0; }
		egc.nEnts = 0;
	}
	int nEnts0 = egc.nEnts;
	egc.pNameTag = pNameTag;
	if (m_entgrid.m_next)
		objtypes |= ent_triggers;
	IPhysicalEntity *dummyPortal;
	if (!rp.pPortals) {
		egc.pPortals = &dummyPortal; egc.nMaxPortals = 0;
	} else {
		egc.pPortals = rp.pPortals;	egc.nMaxPortals = rp.nMaxPortals;
	}
	egc.nPortals = 0;
	if ((objtypes & ent_terrain) && m_pHeightfield[iCaller]) {
    RayHeightfield(rp.org,dir,rp.hits,rp.flags,iCaller);
  }
	egc.aray.CreateRay(rp.org,dir);

	IF (objtypes & m_bCheckWaterHits & ent_water, 0) {
		RayWater(rp.org,dir,egc,rp.flags,rp.nMaxHits,rp.hits);
		objtypes |= ent_areas;
	}

	IF (objtypes & ~(ent_terrain|ent_water) && m_gthunks, 1) {
		MarkSkipEnts(rp.pSkipEnts,rp.nSkipEnts,1<<iCaller);

		egc.phits = rp.hits;
		egc.pWorld = this;
		egc.pgrid = &m_entgrid;
		for(i=0;i<rp.nSkipEnts;i++) if (rp.pSkipEnts[i] && (egc.pgrid=GetGrid(rp.pSkipEnts[i]))!=&m_entgrid)
			break;
		egc.objtypes = objtypes;
		egc.flags = rp.flags^rwi_separate_important_hits;
		if (!(egc.flagsColliderAll = rp.flags>>rwi_colltype_bit))
			egc.flagsColliderAll = geom_colltype_ray;
		if (rp.flags & rwi_ignore_noncolliding)
			egc.flagsColliderAll |= geom_colltype0;
		if (rp.flags & rwi_colltype_any)	{
			egc.flagsColliderAny = egc.flagsColliderAll; egc.flagsColliderAll = 0;
		}	else egc.flagsColliderAny = geom_collides;
		egc.collclass = rp.collclass;
		egc.bUsePhysOnDemand = iszero((objtypes & (ent_static|ent_no_ondemand_activation))-ent_static);
		egc.nMaxHits = rp.nMaxHits;
		egc.pSkipForeignData = (rp.nSkipEnts>0 && rp.pSkipEnts[0]) ? 
			((CPhysicalPlaceholder*)rp.pSkipEnts[0])->CPhysicalPlaceholder::GetForeignData(
				egc.iSkipForeignData=((CPhysicalPlaceholder*)rp.pSkipEnts[0])->CPhysicalPlaceholder::GetiForeignData()) : 
			(void*)NULL;
		egc.pSkipEnts = rp.pSkipEnts;
		egc.nSkipEnts = rp.nSkipEnts;
		egc.ip.bStopAtFirstTri = (rp.flags & rwi_any_hit)!=0;
		egc.szList = GetTmpEntList(egc.pTmpEntList, iCaller);
		egc.iCaller = iCaller;
		m_prevGEAobjtypes[iCaller] = -1;

		Vec3 origin_grid = egc.pgrid->vecToGrid(rp.org-egc.pgrid->origin), dir_grid = egc.pgrid->vecToGrid(dir);
		egc.org2d.set(0.5f-origin_grid.x*egc.pgrid->stepr.x, 0.5f-origin_grid.y*egc.pgrid->stepr.y);
		egc.dir2d.set(dir_grid.x*egc.pgrid->stepr.x, dir_grid.y*egc.pgrid->stepr.y);
		egc.dir2d_len = len(egc.dir2d);

		if (pentLastHit) {
			egc.maxt = 1E20f;
			egc.thunkSubst.inext = 0;
			egc.thunkSubst.pent = pentLastHit;
			egc.thunkSubst.iSimClass = pentLastHit->m_iSimClass;
			egc.pThunkSubst = &egc.thunkSubst;
			egc.iPartSubst_initial = egc.ipartSubst = ipartLastHit; egc.ipartMask = -1;
			egc.gwd.iStartNode = inodeLastHit;
			egc.check_cell(Vec2i(0,0),i);
			if (rp.hits[0].dist>0) {
				dir=egc.aray.m_ray.dir = hits[0].pt-egc.aray.m_ray.origin;
				dir_grid = egc.pgrid->vecToGrid(dir);
				egc.dir2d.set(dir_grid.x*egc.pgrid->stepr.x, dir_grid.y*egc.pgrid->stepr.y);
				egc.dir2d_len = len(egc.dir2d);
			}
			egc.pThunkSubst=0; egc.ipartSubst=egc.ipartMask=0; egc.gwd.iStartNode=0;
		}
		egc.maxt = egc.dir2d_len*(egc.dir2d_len+sqrt2)+0.0001f;

		{
		ReadLock lockg(m_lockGrid);
	  const int checkOutOfBoundsCell = m_vars.iOutOfBounds & raycast_out_of_bounds;
		  IF (checkOutOfBoundsCell, 0) {
				if (fabsf(origin_grid.x*egc.pgrid->stepr.x*2-egc.pgrid->size.x) > egc.pgrid->size.x || 
						fabsf(origin_grid.y*egc.pgrid->stepr.y*2-egc.pgrid->size.y) > egc.pgrid->size.y || 
						fabsf((origin_grid.x+dir_grid.x)*egc.pgrid->stepr.x*2-egc.pgrid->size.x) > egc.pgrid->size.x || 
						fabsf((origin_grid.y+dir_grid.y)*egc.pgrid->stepr.y*2-egc.pgrid->size.y) > egc.pgrid->size.y)
					egc.check_cell(Vec2i(-1,-1),i);
			}

			DrawRayOnGrid(egc.pgrid, origin_grid,dir_grid, egc);
		}

		for(i=nEnts0; i<egc.nEnts; i++)
			AtomicAdd(&(egc.pTmpEntList[i]->m_pEntBuddy && egc.pTmpEntList[i]->m_pEntBuddy->m_pEntBuddy==egc.pTmpEntList[i] && !IsPortal(egc.pTmpEntList[i]) ?
								egc.pTmpEntList[i]->m_pEntBuddy : egc.pTmpEntList[i])->m_bProcessed, -(1<<iCaller));
		
		UnmarkSkipEnts(rp.pSkipEnts,rp.nSkipEnts,1<<iCaller);

		if (rp.flags & rwi_separate_important_hits) {
			int j,idx[2]; ray_hit thit;
			for(idx[0]=1,idx[1]=rp.nMaxHits-1,i=1; idx[0]+rp.nMaxHits-idx[1]-2<egc.nThroughHits+egc.nThroughHitsAux; i++) {
				j = isneg(hits[idx[1]].dist-hits[idx[0]].dist);	// j = hits[1].dist<hits[0].dist ? 1:0;
				j |= isneg(egc.nThroughHits-idx[0]);						// if (idx[0]>nThroughHits) j = 1; 
				j &= rp.nMaxHits-egc.nThroughHitsAux-1-idx[1]>>31; // if (idx[1]<=nMaxHits-nThroughHits1-1) j=0;	
				hits[idx[j]].bTerrain = i;
				idx[j] += 1-j*2;
			}
			for(i=egc.nThroughHits+1; i<rp.nMaxHits-egc.nThroughHitsAux; i++)
				hits[i].bTerrain = rp.nMaxHits+1;
			for(i=1;i<rp.nMaxHits;) if (hits[i].bTerrain!=i && hits[i].bTerrain<rp.nMaxHits) {
				thit=hits[hits[i].bTerrain]; hits[hits[i].bTerrain]=hits[i]; hits[i]=thit;
			}	else i++;
		}
	}

	egc.pPortals[egc.nPortals] = nullptr;
	if (rp.iForeignData==FD_RWI_RECURSIVE) {
		((entity_grid_checker*)rp.pForeignData)->sync_from(egc);
		return egc.nThroughHits + egc.nThroughHitsAux + !!hits[0].pCollider;
	}

	nHits = 0;
	if (hits[0].dist>1E9f) {
		hits[0].dist = -1;
		hits[0].pt = rp.org+dir;
	} else {
		CPhysicalEntity *pent = (CPhysicalEntity*)hits[0].pCollider;
		if (pent && pent->m_iSimClass!=5 && hits[0].ipart<pent->m_nParts) {
			hits[0].foreignIdx = pent->m_parts[hits[0].ipart].pPhysGeom->pGeom->GetForeignIdx(hits[0].partid);
			hits[0].iPrim = hits[0].partid;
			hits[0].partid = pent->m_parts[hits[0].ipart].id;
			pentLastHit = pent;
			ipartLastHit = hits[0].ipart;
			inodeLastHit = egc.iSolidNode;
		} else
			hits[0].foreignIdx=hits[0].partid = -1;
		nHits++;
	}
	if (m_vars.iDrawHelpers & 64 && m_pRenderer) {
		int nTicks=0;
#ifndef PHYS_FUNC_PROFILER_DISABLED
		nTicks = CryGetTicks()-func_profiler.m_iStartTime;
#endif
		int bQueued = iszero(m_lockTPR>>16^1 | iCaller);
		m_pRenderer->DrawLine(rp.org,hits[0].pt,8-bQueued,1|nTicks*2);
	}
	for(i=1;i<rp.nMaxHits;i++) {
		if (hits[i].dist>1E9f || hits[0].dist>0 && hits[i].dist>hits[0].dist)
			hits[i].dist = -1;
		else { 
			CPhysicalEntity *pent = (CPhysicalEntity*)hits[i].pCollider;
			if (pent && pent->m_iSimClass!=5 && hits[i].ipart<pent->m_nParts) {
				hits[i].foreignIdx = pent->m_parts[hits[i].ipart].pPhysGeom->pGeom->GetForeignIdx(hits[i].partid);
				hits[i].iPrim = hits[i].partid;
				hits[i].partid = pent->m_parts[hits[i].ipart].id;
			} else
				hits[i].foreignIdx=hits[i].partid = -1;
			hits[i].bTerrain=0; nHits++; 
		}
	}

	if (rp.phitLast && rp.flags & rwi_update_last_hit) {
		rp.phitLast->pCollider = pentLastHit;
		rp.phitLast->ipart = ipartLastHit;
		rp.phitLast->iNode = inodeLastHit;
	}

	return nHits;
}

int CPhysicalWorld::TracePendingRays(int bDoTracing)
{	
	int i,nChex=0;
	IPhysicalEntity* pSkipEnts[8] = { NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL };
	int iCaller = get_iCaller();
	WriteLock lockg(m_lockTPR);

	if (bDoTracing==2)
		return 0;

	{ 
		SRwiRequest curreq;
		EventPhysRWIResult eprr;
		eprr.pEntity = &g_StaticPhysicalEntity;

		do {
			{ WriteLock lock(m_lockRwiQueue);
				if (m_rwiQueueSz==0)
					break;
				curreq = m_rwiQueue[m_rwiQueueTail];
				m_rwiQueueTail = m_rwiQueueTail+1 - (m_rwiQueueAlloc & m_rwiQueueAlloc-2-m_rwiQueueTail>>31);
				m_rwiQueueSz--; nChex++;
			}
			eprr.pForeignData = curreq.pForeignData;
			eprr.iForeignData = curreq.iForeignData;
			for(i=0; i<curreq.nSkipEnts; i++)
				pSkipEnts[i] = GetPhysicalEntityById(curreq.idSkipEnts[i]);
			curreq.pSkipEnts = pSkipEnts;
			eprr.nHits = bDoTracing ? RayWorldIntersection(curreq, "RayWorldIntersection(Queued)", iCaller) : 0;
			eprr.bHitsFromPool = curreq.iCaller>>16;
			eprr.nMaxHits = curreq.nMaxHits;
			eprr.pHits = curreq.hits;
			eprr.OnEvent = curreq.OnEvent;
			OnEvent(0,&eprr);
		} while(true);
	}

	{ SPwiRequest curreq;
		EventPhysPWIResult eppr;
		eppr.pEntity = &g_StaticPhysicalEntity;
		geom_contact *pcontact=0;

		do {
			{ WriteLock lock(m_lockPwiQueue);
				if (m_pwiQueueSz==0)
					break;
				curreq = m_pwiQueue[m_pwiQueueTail];
				m_pwiQueueTail = m_pwiQueueTail+1 - (m_pwiQueueAlloc & m_pwiQueueAlloc-2-m_pwiQueueTail>>31);
				m_pwiQueueSz--; nChex++;
			}
			eppr.pForeignData = curreq.pForeignData;
			eppr.iForeignData = curreq.iForeignData;
			for(i=0; i<curreq.nSkipEnts; i++)
				pSkipEnts[i] = GetPhysicalEntityById(curreq.idSkipEnts[i]);
			eppr.OnEvent = curreq.OnEvent;
			curreq.pprim = (primitive*)curreq.primbuf;
			curreq.pSkipEnts = pSkipEnts;
			curreq.ppcontact = &pcontact;
			if ((eppr.dist = bDoTracing ? PrimitiveWorldIntersection(curreq, &curreq.lockContacts, "PrimitiveWorldIntersection(Queued)") : 0) && pcontact)
			{ eppr.pt = pcontact->pt;
				eppr.n = pcontact->n;
				eppr.idxMat = pcontact->id[1];
				eppr.partId = pcontact->iPrim[1];
				if (!(eppr.pEntity = GetPhysicalEntityById(pcontact->iPrim[0])))
					eppr.pEntity = &g_StaticPhysicalEntity;
			}
			OnEvent(0,&eppr);
		} while(true);
	}

	return nChex;
}


IGeometry *PrepGeomExt(IGeometry *pGeom) { return PrepGeom(pGeom,0); }


