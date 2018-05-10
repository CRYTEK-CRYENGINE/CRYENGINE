// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"

#include "utils.h"
#include <CryPhysics/primitives.h>
#include "overlapchecks.h"
#include "intersectionchecks.h"
#include "unprojectionchecks.h"
#include "bvtree.h"
#include "geometry.h"
#include "singleboxtree.h"
#include "spheregeom.h"
#include <CryMath/GeomQuery.h>
#include "physicalplaceholder.h"
#include "physicalworld.h"

volatile int *g_pLockIntersect;
class CPhysicalWorld; 
extern CPhysicalWorld* g_pPhysWorlds[]; 


CRY_ALIGN(128) intersData g_idata[MAX_PHYS_THREADS+1];

#undef g_Overlapper
#define g_Overlapper (g_idata[pGTest->iCaller].Overlapper)


InitGeometryGlobals::InitGeometryGlobals() {
  for(int iCaller=0; iCaller<=MAX_PHYS_THREADS; iCaller++) {
    memset(G(UsedNodesMap), 0, sizeof(G(UsedNodesMap)));
    G(EdgeDescBufPos) = 0; G(IdBufPos) = 0;	g_IdxTriBufPos = 0;
    G(SurfaceDescBufPos) = 0; G(UsedNodesMapPos) = 0;	G(UsedNodesIdxPos) = 0;
    G(iFeatureBufPos) = 0;
    G(nAreas)=0; g_nTotContacts=0; g_nAreaPt=0;
  }
}
static InitGeometryGlobals now;

#include "raybv.h"
#include "raygeom.h"

int IntersectBVs(geometry_under_test *pGTest, BV *pBV1, BV *pBV2) 
{
  MEMSTREAM_DEBUG_ASSERT(pGTest && pBV1 && pBV2);
	intptr_t mask;
	int iNode[2],*pUsedNodesMap[2],res=0,nullmap=0,bNodeUsed[2],*pnullmap=&nullmap;
	mask = iszero_mask(pGTest[0].pUsedNodesMap); // mask = -(pUsedNodeMap==0)
	pUsedNodesMap[0] = (int*)((intptr_t)pGTest[0].pUsedNodesMap&~mask | (intptr_t)pnullmap&mask);
	iNode[0] = pBV1->iNode&~mask;
	mask = iszero_mask(pGTest[1].pUsedNodesMap); // mask = -(pUsedNodeMap==0)
	pUsedNodesMap[1] = (int*)((intptr_t)pGTest[1].pUsedNodesMap&~mask | (intptr_t)pnullmap&mask);
	iNode[1] = pBV2->iNode&~mask;
	bNodeUsed[0] = pUsedNodesMap[0][iNode[0]>>5]>>(iNode[0]&31) & 1;
	bNodeUsed[1] = pUsedNodesMap[1][iNode[1]>>5]>>(iNode[1]&31) & 1;

	if (bNodeUsed[0] & bNodeUsed[1])
		return 0; // skip check only if both nodes are marked used

  MEMSTREAM_DEBUG_ASSERT(pBV1->type>=0 && pBV1->type<NPRIMS);
  MEMSTREAM_DEBUG_ASSERT(pBV2->type>=0 && pBV2->type<NPRIMS);
	if (!g_Overlapper.Check(pBV1->type,pBV2->type, *pBV1,*pBV2))
		return 0;

  MEMSTREAM_DEBUG_ASSERT(pGTest[0].pBVtree && pGTest[1].pBVtree);
	float split1,split2;
	split1 = pGTest[0].pBVtree->SplitPriority(pBV1);
	split2 = pGTest[1].pBVtree->SplitPriority(pBV2);

	BV *pBV_split1, *pBV_split2;

	if (split1>split2 && split1>0) { // split the first BV
		pGTest[0].pBVtree->GetNodeChildrenBVs(pBV1, pBV_split1,pBV_split2, pGTest->iCaller);
    MEMSTREAM_DEBUG_ASSERT(pBV_split1->type>=0 && pBV_split1->type<NPRIMS);
		res = IntersectBVs(pGTest, pBV_split1,pBV2);
		if (pGTest[0].bStopIntersection + pGTest[1].bStopIntersection > 0)
			return res;
    MEMSTREAM_DEBUG_ASSERT(pBV_split2->type>=0 && pBV_split2->type<NPRIMS);
		res += IntersectBVs(pGTest, pBV_split2,pBV2);
		pGTest[0].pBVtree->ReleaseLastBVs(pGTest->iCaller);
	} else 
	if (split2>0) { // split the second BV
		pGTest[1].pBVtree->GetNodeChildrenBVs(pGTest[1].R_rel,pGTest[1].offset_rel,pGTest[1].scale_rel, pBV2, pBV_split1,pBV_split2, pGTest->iCaller);
    MEMSTREAM_DEBUG_ASSERT(pBV_split1->type>=0 && pBV_split1->type<NPRIMS);
		res = IntersectBVs(pGTest, pBV1,pBV_split1);
		if (pGTest[0].bStopIntersection + pGTest[1].bStopIntersection > 0)
			return res;
    MEMSTREAM_DEBUG_ASSERT(pBV_split2->type>=0 && pBV_split2->type<NPRIMS);
		res += IntersectBVs(pGTest, pBV1,pBV_split2);
		pGTest[1].pBVtree->ReleaseLastBVs(pGTest->iCaller);
	} else {
		int nPrims1,nPrims2,iClient,i,j;
		char *ptr[2];

		prim_inters inters;
		Vec3 ptborder_loc[16];

		inters.minPtDist2 = sqr(min(pGTest[0].pGeometry->m_minVtxDist, pGTest[1].pGeometry->m_minVtxDist));
		iClient = -(pGTest[1].pGeometry->m_iCollPriority-pGTest[0].pGeometry->m_iCollPriority>>31);
		inters.pt[1].zero(); inters.n.zero(); 
		inters.ptborder = ptborder_loc;
		inters.nborderpt = inters.nBestPtVal = 0;
		inters.nbordersz = CRY_ARRAY_COUNT(ptborder_loc)-1;

		nPrims1 = pGTest[0].pBVtree->GetNodeContents(pBV1->iNode, pBV2,bNodeUsed[1],1, pGTest+0,pGTest+1);
		nPrims2 = pGTest[1].pBVtree->GetNodeContents(pBV2->iNode, pBV1,bNodeUsed[0],0, pGTest+1,pGTest+0);

		for(i=0,ptr[0]=(char*)pGTest[0].primbuf; i<nPrims1; i++,ptr[0]=(ptr[0]+pGTest[0].szprim)) 
			for(j=0,ptr[1]=(char*)pGTest[1].primbuf; j<nPrims2; j++,ptr[1]=(ptr[1]+pGTest[1].szprim))
				if (g_Intersector.Check(pGTest[iClient].typeprim,pGTest[iClient^1].typeprim, (primitive*)ptr[iClient],(primitive*)ptr[iClient^1], &inters)) 
		{
			inters.id[0] = pGTest[iClient].idbuf[i&-(iClient^1)|j&-iClient]; 
			inters.id[1] = pGTest[iClient^1].idbuf[i&-iClient|j&-(iClient^1)];
			inters.iNode[0] = pBV1->iNode; 
			inters.iNode[1] = pBV2->iNode;
			pGTest[0].bCurNodeUsed = pGTest[1].bCurNodeUsed = 0;
			res += pGTest[iClient].pGeometry->RegisterIntersection((primitive*)ptr[iClient],(primitive*)ptr[iClient^1], pGTest+iClient,pGTest+(iClient^1), &inters);
			if (pGTest[0].bStopIntersection+pGTest[1].bStopIntersection + (pGTest[0].bCurNodeUsed&pGTest[1].bCurNodeUsed)> 0)
				return res;
			inters.nborderpt = inters.nBestPtVal = 0;
			inters.nbordersz = CRY_ARRAY_COUNT(ptborder_loc)-1;
		}
	}

	return res;
}


int SweepTestBVs(geometry_under_test *pGTest, BV *pBV1, BV *pBV2) 
{
  MEMSTREAM_DEBUG_ASSERT(pGTest && pBV1 && pBV2);
	if (!g_Overlapper.Check(pBV1->type,pBV2->type, *pBV1,*pBV2))
		return 0;
	
	int res = 0;
	float split1,split2;
	split1 = pGTest[0].pBVtree->SplitPriority(pBV1);
	split2 = pGTest[1].pBVtree->SplitPriority(pBV2);

	BV *pBV_split1,  *pBV_split2;

	if (split1>split2 && split1>0) { // split the first BV
		pGTest[0].pBVtree->GetNodeChildrenBVs(pBV1, pGTest[0].sweepdir_loc,pGTest[0].sweepstep_loc, pBV_split1,pBV_split2, pGTest->iCaller);
		res = SweepTestBVs(pGTest, pBV_split1,pBV2) + SweepTestBVs(pGTest, pBV_split2,pBV2);
		pGTest[0].pBVtree->ReleaseLastSweptBVs(pGTest->iCaller);
	} else 
	if (split2>0) { // split the second BV
		pGTest[1].pBVtree->GetNodeChildrenBVs(pGTest[1].R_rel,pGTest[1].offset_rel,pGTest[1].scale_rel, pBV2, pBV_split1,pBV_split2, pGTest->iCaller);
		res = SweepTestBVs(pGTest, pBV1,pBV_split1) + SweepTestBVs(pGTest, pBV1,pBV_split2);
		pGTest[1].pBVtree->ReleaseLastBVs(pGTest->iCaller);
	} else {
		int nPrims1,nPrims2,i,j;
		char *ptr[2];
		contact contact_cur;
		geom_contact *pcontact = pGTest[0].contacts;
		unprojection_mode unproj;
		Vec3 startoffs = pGTest[0].offset;
		pGTest[0].offset += pGTest[0].sweepdir*pGTest[0].sweepstep;
		unproj.imode = 0;
		unproj.dir = -pGTest[0].sweepdir;
		unproj.tmax = pGTest[0].sweepstep;
		unproj.minPtDist = min(pGTest[0].pGeometry->m_minVtxDist, pGTest[1].pGeometry->m_minVtxDist);

		nPrims1 = pGTest[0].pBVtree->GetNodeContents(pBV1->iNode, pBV2,0,-1, pGTest+0,pGTest+1);
		nPrims2 = pGTest[1].pBVtree->GetNodeContents(pBV2->iNode, pBV1,0,0, pGTest+1,pGTest+0);

		for(i=0,ptr[0]=(char*)pGTest[0].primbuf; i<nPrims1; i++,ptr[0]=(ptr[0]+pGTest[0].szprim)) 
			for(j=0,ptr[1]=(char*)pGTest[1].primbuf; j<nPrims2; j++,ptr[1]=(ptr[1]+pGTest[1].szprim))
				if (g_Unprojector.Check(&unproj, pGTest[0].typeprim,pGTest[1].typeprim, (primitive*)ptr[0],-1,(primitive*)ptr[1],-1, &contact_cur, NULL) && 
						contact_cur.n*pGTest[0].sweepdir>0 && contact_cur.t<=pGTest[0].sweepstep && pGTest[0].sweepstep-contact_cur.t<pcontact->t) 
		{
			pcontact->t = pGTest[0].sweepstep-contact_cur.t;
			pcontact->pt = pcontact->center = contact_cur.pt;
			pcontact->n = contact_cur.n.normalized();
			pcontact->iPrim[0] = pGTest[0].typeprim!=indexed_triangle::type ? 0 : ((indexed_triangle*)ptr[0])->idx;
			pcontact->iPrim[1] = pGTest[1].typeprim==indexed_triangle::type ? ((indexed_triangle*)ptr[1])->idx : 0;
			pcontact->iFeature[0] = contact_cur.iFeature[0];
			pcontact->iFeature[1] = contact_cur.iFeature[1];
			pcontact->id[0] = pGTest[0].idbuf[i];
			pcontact->id[1] = pGTest[1].idbuf[j];
			pcontact->iNode[0] = pBV1->iNode; 
			pcontact->iNode[1] = pBV2->iNode;
			res++; *(pGTest[0].pnContacts) = 1;
		}
		pGTest[0].offset = startoffs;
	}

	return res;
}


struct radius_check_data {
	CGeometry *pGeom;
	CBVTree *pBVtree;
	geom_world_data *pgwd;
	sphere sph;
	SOcclusionCubeMap* cubeMap;
	int nGrow;
	int iPass;
	int iCaller;
};

int RadiusCheckBVs(radius_check_data *prcd, BV *pBV)
{
	if (!g_idata[prcd->iCaller].Overlapper.Check(pBV->type,sphere::type, *pBV,&prcd->sph))
		return 0;

	if (prcd->pBVtree->SplitPriority(pBV)>0) {
		BV *pBV_split1,*pBV_split2;
		prcd->pBVtree->GetNodeChildrenBVs(pBV, pBV_split1,pBV_split2, prcd->iCaller);
		int res = RadiusCheckBVs(prcd, pBV_split1)+RadiusCheckBVs(prcd, pBV_split2);
		prcd->pBVtree->ReleaseLastBVs(prcd->iCaller);
		return res;
	} else {
		int iStartPrim,nPrims;
		nPrims = prcd->pBVtree->GetNodeContentsIdx(pBV->iNode, iStartPrim);
		return prcd->pGeom->DrawToOcclusionCubemap(prcd->pgwd, iStartPrim,nPrims, prcd->iPass, prcd->cubeMap);
	}
}

float CGeometry::BuildOcclusionCubemap(geom_world_data *pgwd, int iMode, SOcclusionCubeMap* cubemap0, SOcclusionCubeMap* cubemap1, int nGrow)
{
		radius_check_data rcd;
	float rscale = pgwd->scale==1.0f ? 1.0f:1.0f/pgwd->scale;
	int nRes = cubemap0->N;
	rcd.iCaller = get_iCaller();
	ReadLockCond lock0(m_lockUpdate,isneg(m_lockUpdate)^1);

	ResetGlobalPrimsBuffers(rcd.iCaller);
	rcd.pgwd = pgwd;
	rcd.sph.center = (-pgwd->offset*rscale)*pgwd->R;
	rcd.sph.r = cubemap0->rmax*rscale;
	rcd.nGrow = nGrow;
	rcd.iPass = 0;
	if (iMode==0)
		rcd.cubeMap = cubemap0;
	else {
		rcd.cubeMap = cubemap1;
		cubemap1->Reset();
	}
	rcd.pGeom = this;
	rcd.pBVtree = GetBVTree();
	
	BV *pBV;
	rcd.pBVtree->GetNodeBV(pBV,0,rcd.iCaller);

	if (RadiusCheckBVs(&rcd,pBV)) {
		rcd.iPass = 1;
		ResetGlobalPrimsBuffers(rcd.iCaller);
		rcd.pBVtree->GetNodeBV(pBV,0,rcd.iCaller);
		RadiusCheckBVs(&rcd,pBV);
	}

	rcd.cubeMap->ProcessRmin();

	if (iMode) {
		int nCells,nOccludedCells;
		GrowAndCompareCubemaps(cubemap0, cubemap1, nGrow, nCells, nOccludedCells);
		return nCells>0 ? (float)(nCells-nOccludedCells)/nCells : 0.0f;
	}
	return 0.0f;
}


#undef g_Overlapper
#define g_Overlapper (g_idata[iCaller].Overlapper)

void GTestPrepPartDeux(geometry_under_test *gtest, Vec3r &offsetWorld)
{
	for(int i=0;i<2;i++)
		gtest[i].offset_rel = ((gtest[i].offset-gtest[i^1].offset)*gtest[i^1].R)*gtest[i^1].rscale;
	gtest[0].offset -= (offsetWorld = gtest[1].offset);
	gtest[0].centerOfMass -= offsetWorld; gtest[0].centerOfRotation -= offsetWorld;	gtest[0].ptOutsidePivot -= offsetWorld;
	gtest[1].centerOfMass -= offsetWorld;	gtest[1].centerOfRotation -= offsetWorld;	gtest[1].ptOutsidePivot -= offsetWorld;
	gtest[1].offset.zero(); // use relative offset in calculations to improve accuracy
}

int CGeometry::Intersect(IGeometry *pCollider, geom_world_data *pdata1,geom_world_data *pdata2, intersection_params *pparams, geom_contact *&pcontacts)
{
	//CRY_PROFILE_FUNCTION(PROFILE_PHYSICS );
	geometry_under_test gtest[2];
	geom_world_data *pdata[2] = { pdata1,pdata2 };
	int i,j,jmax,mask,nContacts=0,iStartNode[2],bActive=0,iCaller;
	intersection_params defip;
	Vec3r offsetWorld;
	if (!pparams)	{
		pparams = &defip;	bActive=1;
	}
	iCaller=gtest[0].iCaller=gtest[1].iCaller = get_iCaller();
	ReadLockCond lock0(m_lockUpdate, (pparams->bThreadSafeMesh|isneg(m_lockUpdate))^1), 
								  lock1(((CGeometry*)pCollider)->m_lockUpdate, (pparams->bThreadSafeMesh|isneg(((CGeometry*)pCollider)->m_lockUpdate))^1 && pCollider!=this);


	// zero all global buffer pointers
	ResetGlobalPrimsBuffers(iCaller);
	g_Overlapper.Init();
	if (!pparams->bKeepPrevContacts) {
		G(nAreas) = 0; g_nAreaPt = 0; g_nTotContacts = 0;
		g_BrdPtBufPos = 0;
	}
	if (g_nTotContacts>=g_maxContacts)
		return 0;
	G(SurfaceDescBufPos) = 0;
	G(EdgeDescBufPos) = 0;
	G(IdBufPos) = 0;
	G(iFeatureBufPos) = 0;
	G(UsedNodesMapPos) = 0;
	G(UsedNodesIdxPos) = 0;
	pcontacts = g_Contacts+g_nTotContacts;
	pparams->pGlobalContacts = g_Contacts;

	if (!pparams->bNoAreaContacts && g_nAreas<CRY_ARRAY_COUNT(g_AreaBuf) && g_nAreaPt<CRY_ARRAY_COUNT(g_AreaPtBuf)) {
		pcontacts[0].parea = g_AreaBuf+g_nAreas;
		pcontacts[0].parea->pt = g_AreaPtBuf+g_nAreaPt;
		pcontacts[0].parea->piPrim[0] = g_AreaPrimBuf0+g_nAreaPt; 
		pcontacts[0].parea->piFeature[0] = g_AreaFeatureBuf0+g_nAreaPt;
		pcontacts[0].parea->piPrim[1] = g_AreaPrimBuf1+g_nAreaPt; 
		pcontacts[0].parea->piFeature[1] = g_AreaFeatureBuf1+g_nAreaPt;
		pcontacts[0].parea->npt = 0; pcontacts[0].parea->nmaxpt = CRY_ARRAY_COUNT(g_AreaPtBuf)-g_nAreaPt;
	} else
		pcontacts[0].parea = 0;
	if (pparams->bSweepTest && pdata1->v.len2()==0) {
		pparams->bSweepTest = 0; pdata1->v.Set(0,0,1); pparams->vrel_min = 0; 
	}

	for(i=0;i<2;i++) {
		if (pdata[i]) {
			gtest[i].offset = pdata[i]->offset;
			gtest[i].R = pdata[i]->R;
			gtest[i].scale = pdata[i]->scale;
			gtest[i].rscale = 1.0f/pdata[i]->scale;
			gtest[i].v = pdata[i]->v;
			gtest[i].w = pdata[i]->w;
			gtest[i].centerOfMass = pdata[i]->centerOfMass;
			iStartNode[i] = pdata[i]->iStartNode;
		} else {
			gtest[i].offset.zero();
			gtest[i].R.SetIdentity();
			gtest[i].scale = gtest[i].rscale = 1.0f;
			gtest[i].v.zero();
			gtest[i].w.zero();
			gtest[i].centerOfMass.zero();
			iStartNode[i] = 0;
		}
		gtest[i].centerOfRotation = pparams->centerOfRotation;
		gtest[i].axisContactNormal = pparams->axisContactNormal*(1-(i<<1));
		gtest[i].contacts = pcontacts;
		gtest[i].pnContacts = &nContacts;
		gtest[i].nMaxContacts = g_maxContacts-g_nTotContacts;
		gtest[i].bStopIntersection = 0;
		gtest[i].sweepstep = 0;
		gtest[i].ptOutsidePivot = pparams->ptOutsidePivot[i];
		gtest[i].pParams = pparams;
	}
	for(i=0;i<2;i++) {
		gtest[i].offset_rel = ((gtest[i].offset-gtest[i^1].offset)*gtest[i^1].R)*gtest[i^1].rscale;
		(gtest[i].R_rel = gtest[i^1].R.T()) *= gtest[i].R;
		gtest[i].scale_rel = gtest[i].scale*gtest[i^1].rscale;
		gtest[i].rscale_rel = gtest[i^1].scale*gtest[i].rscale;
	}
	pparams->bBothConvex = m_bIsConvex & ((CGeometry*)pCollider)->m_bIsConvex;

	BV *pBV1,*pBV2;
	if (!pparams->bSweepTest) {
		if (!PrepareForIntersectionTest(gtest+0, (CGeometry*)pCollider,gtest+1, pparams->bKeepPrevContacts))
			return 0;
		if (!((CGeometry*)pCollider)->PrepareForIntersectionTest(gtest+1, this,gtest+0, pparams->bKeepPrevContacts)) {
			CleanupAfterIntersectionTest(gtest+0);
			return 0;
		}
		if (pcontacts[0].parea)
			pcontacts[0].parea->minedge = min(gtest[0].minAreaEdge, gtest[1].minAreaEdge);
		GTestPrepPartDeux(gtest, offsetWorld);

		if (iStartNode[0]+iStartNode[1]) {
			gtest[0].pBVtree->GetNodeBV(pBV1, iStartNode[0], iCaller);
			gtest[1].pBVtree->GetNodeBV(gtest[1].R_rel,gtest[1].offset_rel,gtest[1].scale_rel, pBV2, iStartNode[1], iCaller);
			IntersectBVs(gtest, pBV1,pBV2);
		}
		if (nContacts==0) {
			gtest[0].pBVtree->GetNodeBV(pBV1, 0,iCaller);
			gtest[1].pBVtree->GetNodeBV(gtest[1].R_rel,gtest[1].offset_rel,gtest[1].scale_rel, pBV2, 0,iCaller);
			IntersectBVs(gtest, pBV1,pBV2);
		}

		int iClient = -(gtest[1].pGeometry->m_iCollPriority-gtest[0].pGeometry->m_iCollPriority>>31);
		if (iClient) for(i=0;i<nContacts;i++) {
			if (pcontacts[i].parea && pcontacts[i].parea->npt) {
				Vec3 ntmp=pcontacts[i].n; pcontacts[i].n=pcontacts[i].parea->n1; pcontacts[i].parea->n1=ntmp;
				int *pprim=pcontacts[i].parea->piPrim[0]; pcontacts[i].parea->piPrim[0]=pcontacts[i].parea->piPrim[1]; pcontacts[i].parea->piPrim[1]=pprim;
				int *pfeat=pcontacts[i].parea->piFeature[0]; pcontacts[i].parea->piFeature[0]=pcontacts[i].parea->piFeature[1]; 
				pcontacts[i].parea->piFeature[1]=pfeat;
				if (pcontacts[i].iUnprojMode) {
					pcontacts[i].n = pcontacts[i].n.GetRotated(pcontacts[i].dir,-pcontacts[i].t);
					pcontacts[i].parea->n1 = pcontacts[i].parea->n1.GetRotated(pcontacts[i].dir,-pcontacts[i].t);
				}
				if (pcontacts[i].iUnprojMode)	for(j=0;j<pcontacts[i].parea->npt;j++)
					pcontacts[i].parea->pt[j] = pcontacts[i].parea->pt[j].GetRotated(gtest[0].centerOfRotation,pcontacts[i].dir,-pcontacts[i].t);
				else for(j=0;j<pcontacts[i].parea->npt;j++)
					pcontacts[i].parea->pt[j] -= pcontacts[i].dir*pcontacts[i].t;
			} else {
				if (pcontacts[i].iUnprojMode)
					pcontacts[i].n = pcontacts[i].n.GetRotated(pcontacts[i].dir,-pcontacts[i].t);
				pcontacts[i].n.Flip();
			}
			if (pcontacts[i].nborderpt>1) for(j=0; j<pcontacts[i].nborderpt; j++)	{
				mask = pcontacts[i].idxborder[j][0]; pcontacts[i].idxborder[j][0] = pcontacts[i].idxborder[j][1];
				pcontacts[i].idxborder[j][1] = mask;
			}
			if (pcontacts[i].iUnprojMode)
				pcontacts[i].pt = pcontacts[i].pt.GetRotated(gtest[0].centerOfRotation,pcontacts[i].dir,-pcontacts[i].t);
			else
				pcontacts[i].pt -= pcontacts[i].dir*pcontacts[i].t;
			pcontacts[i].dir.Flip();
			char id=pcontacts[i].id[0]; pcontacts[i].id[0]=pcontacts[i].id[1]; pcontacts[i].id[1]=id;
			int iprim=pcontacts[i].iPrim[0]; pcontacts[i].iPrim[0]=pcontacts[i].iPrim[1]; pcontacts[i].iPrim[1]=iprim;
			int ifeature=pcontacts[i].iFeature[0]; pcontacts[i].iFeature[0]=pcontacts[i].iFeature[1]; pcontacts[i].iFeature[1]=ifeature;
		}

		// sort contacts in descending t order
		geom_contact tmpcontact;
		for(i=0;i<nContacts-1;i++) {
			for(jmax=i,j=i+1; j<nContacts; j++) {
				mask = -isneg(pcontacts[jmax].t-pcontacts[j].t);
				jmax = jmax&~mask | j&mask;
			}
			if (jmax!=i) {
				if (pcontacts[i].ptborder==&pcontacts[i].center) pcontacts[i].ptborder = &pcontacts[jmax].center;
				if (pcontacts[i].ptborder==&pcontacts[i].pt) pcontacts[i].ptborder = &pcontacts[jmax].pt;
				if (pcontacts[jmax].ptborder==&pcontacts[jmax].center) pcontacts[jmax].ptborder = &pcontacts[i].center;
				if (pcontacts[jmax].ptborder==&pcontacts[jmax].pt) pcontacts[jmax].ptborder = &pcontacts[i].pt;
				tmpcontact=pcontacts[i]; pcontacts[i]=pcontacts[jmax]; pcontacts[jmax]=tmpcontact;
			}
		}
	}	else {
		gtest[0].sweepstep = pcontacts[0].vel = gtest[0].v.len();
		gtest[0].sweepdir = gtest[0].v/gtest[0].sweepstep;
		gtest[0].sweepstep *= pparams->time_interval;
		gtest[0].sweepdir_loc = gtest[0].sweepdir*gtest[0].R;
		gtest[0].sweepstep_loc = gtest[0].sweepstep*gtest[0].rscale;
		gtest[0].ptOutsidePivot=gtest[1].ptOutsidePivot = Vec3(1e11f);

		if (!PrepareForIntersectionTest(gtest+0, (CGeometry*)pCollider,gtest+1, pparams->bKeepPrevContacts) || 
			!((CGeometry*)pCollider)->PrepareForIntersectionTest(gtest+1, this,gtest+0, pparams->bKeepPrevContacts))
			return 0;
		GTestPrepPartDeux(gtest, offsetWorld);

		pcontacts[0].ptborder = &g_Contacts[0].pt;
		pcontacts[0].nborderpt = 1;
		pcontacts[0].parea = 0;
		pcontacts[0].dir = -gtest[0].sweepdir;
		pcontacts[0].t = gtest[0].sweepstep;

		gtest[0].pBVtree->GetNodeBV(pBV1, gtest[0].sweepdir_loc,gtest[0].sweepstep_loc, 0,iCaller);
		gtest[1].pBVtree->GetNodeBV(gtest[1].R_rel,gtest[1].offset_rel,gtest[1].scale_rel, pBV2, 0,iCaller);
		SweepTestBVs(gtest, pBV1,pBV2);
	}

	for(i=0;i<nContacts;i++) {
		pcontacts[i].pt += offsetWorld;
		pcontacts[i].center += offsetWorld;
		if (pcontacts[i].ptborder!=&pcontacts[i].pt && pcontacts[i].ptborder!=&pcontacts[i].center)
			for(j=0;j<pcontacts[i].nborderpt;j++)
				pcontacts[i].ptborder[j] += offsetWorld;
		if (pcontacts[i].parea) for(j=0;j<pcontacts[i].parea->npt;j++)
			pcontacts[i].parea->pt[j] += offsetWorld;
	}

	CleanupAfterIntersectionTest(gtest+0);
	((CGeometry*)pCollider)->CleanupAfterIntersectionTest(gtest+1);

	g_nTotContacts += nContacts;
	return nContacts;
}


int CGeometry::RegisterIntersection(primitive *pprim1,primitive *pprim2, geometry_under_test *pGTest1,geometry_under_test *pGTest2, 
																		prim_inters *pinters)
{
	unprojection_mode unproj;
	contact contact_cur;
	geom_contact *pres = pGTest1->contacts + *pGTest1->pnContacts;
	const int iCaller = pGTest1->iCaller;
	primitive *pprims[2] = { pprim1, pprim2 };
	pres->ptborder = &pres->pt;
	pres->nborderpt = 1;

	if (pGTest1->pParams->iUnprojectionMode==0) {
		// for linear unprojection, find point with maximum relative normal velocity
		unproj.imode = 0;
		unproj.dir = pGTest2->v+(pGTest2->w^pinters->pt[1]-pGTest2->centerOfMass) - 
			pGTest1->v-(pGTest1->w^pinters->pt[1]-pGTest1->centerOfMass);
		unproj.vel = unproj.dir.len();
		if (unproj.vel < pGTest1->pParams->vrel_min)
			goto use_normal;
		unproj.dir /= unproj.vel;
	} else if (pGTest1->pParams->iUnprojectionMode==1) {
		unproj.imode = 1;
		unproj.dir = pGTest2->w-pGTest1->w;
		unproj.center = pGTest1->centerOfRotation.len2()>pGTest2->centerOfRotation.len2() ? pGTest1->centerOfRotation : pGTest2->centerOfRotation;
		unproj.vel = unproj.dir.len();
		unproj.dir /= unproj.vel;
	}
	unproj.tmax = pGTest1->pParams->time_interval*unproj.vel;

	if (!g_Unprojector.Check(&unproj, pGTest1->typeprim,pGTest2->typeprim, pprims[0],-1,pprims[1],-1, &contact_cur, pres->parea))
		return 0;

	if (contact_cur.t > pGTest1->pParams->time_interval*unproj.vel) {
		use_normal:
		unproj.imode = 0;
		unproj.dir = pinters->n.normalize();
		unproj.vel = 0;	

		if (!g_Unprojector.Check(&unproj, pGTest1->typeprim,pGTest2->typeprim, pprims[0],-1,pprims[1],-1, &contact_cur, pres->parea))
			return 0;
	}

	pres->t = contact_cur.t;
	pres->pt = contact_cur.pt;
	pres->n = contact_cur.n;
	pres->dir = unproj.dir;
	pres->vel = unproj.vel;
	pres->iUnprojMode = unproj.imode;
	pres->id[0] = pinters->id[0];
	pres->id[1] = pinters->id[1];
	pres->iNode[0] = pinters->iNode[0];
	pres->iNode[1] = pinters->iNode[1];

	if (pres->parea && pres->parea->npt>0) {
		g_nAreaPt += pres->parea->npt;
		g_nAreas++;
	}	else 
		pres->parea = 0;

	// allocate slots for the next intersection
	(*pGTest1->pnContacts)++;
	pres = pGTest1->contacts + *pGTest1->pnContacts;
	if (g_nAreas<CRY_ARRAY_COUNT(g_AreaBuf) && g_nAreaPt<CRY_ARRAY_COUNT(g_AreaPtBuf)) {
		pres->parea = g_AreaBuf+g_nAreas;
		pres->parea->pt = g_AreaPtBuf+g_nAreaPt;
		pres->parea->piPrim[0] = g_AreaPrimBuf0+g_nAreaPt; pres->parea->piFeature[0] = g_AreaFeatureBuf0+g_nAreaPt;
		pres->parea->piPrim[1] = g_AreaPrimBuf1+g_nAreaPt; pres->parea->piFeature[1] = g_AreaFeatureBuf1+g_nAreaPt;
		pres->parea->npt = 0; pres->parea->nmaxpt = CRY_ARRAY_COUNT(g_AreaPtBuf)-g_nAreaPt;
		pres->parea->minedge = min(pGTest1->minAreaEdge, pGTest2->minAreaEdge);
	} else
		pres->parea = 0;

	if (*pGTest1->pnContacts >= pGTest1->nMaxContacts)
		pGTest1->bStopIntersection = 1;

	return 0;
}


// default geometry implementation just uses bounding box.
float CGeometry::GetExtent(EGeomForm eForm) const
{
	primitives::box box;
	non_const(*this).GetBBox(&box);
	return BoxExtent(eForm, box.size);
}

void CGeometry::GetRandomPoints(Array<PosNorm> points, CRndGen& seed, EGeomForm eForm) const
{
	primitives::box box;
	non_const(*this).GetBBox(&box);
	BoxRandomPoints(points, seed, eForm, box.size);
	for (auto& ran : points) {
		if (box.bOriented) {
			// Transform by transpose of Basis (Vec * Matrix)
			ran.vPos = ran.vPos * box.Basis;
			ran.vNorm = ran.vNorm * box.Basis;
		}
		ran.vPos += box.center;
	}
}

void DrawBBox(IPhysRenderer *pRenderer, int idxColor, geom_world_data *gwd, CBVTree *pTree,BBox *pbbox,int maxlevel,int level, int iCaller)
{
	if (level<maxlevel && pTree->SplitPriority(pbbox)>0) {
		BV *pbbox1,*pbbox2;
		pTree->GetNodeChildrenBVs(gwd->R,gwd->offset,gwd->scale, pbbox, pbbox1,pbbox2, iCaller);
		DrawBBox(pRenderer,idxColor,gwd, pTree,(BBox*)pbbox1,maxlevel,level+1,iCaller);
		DrawBBox(pRenderer,idxColor,gwd, pTree,(BBox*)pbbox2,maxlevel,level+1,iCaller);
		pTree->ReleaseLastBVs(iCaller);
		return;
	}

	Vec3 pts[8],sz;
	int i,j;

	for(i=0;i<8;i++) {
		for(j=0;j<3;j++) sz[j]=pbbox->abox.size[j]*(((i>>j&1)<<1)-1);
		pts[i] = pbbox->abox.Basis.T()*sz + pbbox->abox.center;
	}
	for(i=0;i<8;i++) for(j=0;j<3;j++) if (i&1<<j)
		pRenderer->DrawLine(pts[i],pts[i^1<<j],idxColor);
}


geom_world_data defgwd;
intersection_params defip;

int CPrimitive::Intersect(IGeometry *_pCollider, geom_world_data *pdata1, geom_world_data *pdata2, 
													intersection_params *pparams, geom_contact *&pcontacts)
{
	CGeometry *pCollider = (CGeometry*)_pCollider;
	if (!pCollider->IsAPrimitive())
		return CGeometry::Intersect(pCollider,pdata1,pdata2,pparams,pcontacts);

	//CRY_PROFILE_FUNCTION(PROFILE_PHYSICS );
	pdata1 = (geom_world_data*)((intptr_t)pdata1 | -iszero((intptr_t)pdata1) & (intptr_t)&defgwd);
	pdata2 = (geom_world_data*)((intptr_t)pdata2 | -iszero((intptr_t)pdata2) & (intptr_t)&defgwd);
	//pparams = (intersection_params*)((intptr_t)pparams | -iszero((intptr_t)pparams) & (intptr_t)&defip);
	int bActive;
	if (pparams) 
		bActive=0;
	else {
		pparams=&defip; bActive=1; 
	}
	int iCaller = get_iCaller();

	if (!pparams->bKeepPrevContacts)
		g_nAreas = g_nAreaPt = g_nTotContacts = g_BrdPtBufPos = 0;
	if (max(g_BrdPtBufPos+8-(int)(CRY_ARRAY_COUNT(g_BrdPtBuf)), g_nTotContacts-(int)g_maxContacts)>=0)
		return 0;
	pcontacts = g_Contacts+g_nTotContacts;
	pcontacts->ptborder = g_BrdPtBuf+g_BrdPtBufPos;
	pcontacts->nborderpt = 0;
	pcontacts->idxborder = NULL;
	pparams->pGlobalContacts = g_Contacts;

	BV *pBV1,*pBV2;
	prim_inters inters;
	Vec3 sweepdir(ZERO); float sweepstep=0;
	ResetGlobalPrimsBuffers(iCaller);
	g_Overlapper.Init();
	if (!pparams->bSweepTest) 
		GetBVTree()->GetNodeBV(pdata1->R,pdata1->offset,pdata1->scale, pBV1, 0,iCaller);
	else {
		sweepstep = pdata1->v.len();
		sweepdir = pdata1->v/sweepstep;
		sweepstep *= pparams->time_interval;
		GetBVTree()->GetNodeBV(pdata1->R,pdata1->offset,pdata1->scale, pBV1, sweepdir,sweepstep, 0,iCaller);
	}
	pCollider->GetBVTree()->GetNodeBV(pdata2->R,pdata2->offset,pdata2->scale, pBV2, 0,iCaller);
	if (!g_Overlapper.Check(pBV1->type,pBV2->type, *pBV1,*pBV2))
		return 0;

	// get primitives in world space
	int i,itype[2],bUnprojected=0;
	primitive *pprim[2];
	pdata1->offset += sweepdir*sweepstep;
	itype[0] = PreparePrimitive(pdata1,pprim[0],iCaller);
	itype[1] = pCollider->PreparePrimitive(pdata2,pprim[1],iCaller);
	pdata1->offset -= sweepdir*sweepstep;
	inters.n.zero();
	inters.id[0]=inters.id[1] = -1;
	inters.iNode[0]=inters.iNode[1] = 0;
	inters.nbordersz=0;
  
	if (pCollider->m_iCollPriority==0) { // probably the other geometry is a ray
		if (!g_Intersector.Check(itype[0],itype[1], pprim[0],pprim[1], &inters)) 
			return 0;
		geometry_under_test gtest;
		int nContacts = 0;
		gtest.contacts = g_Contacts+g_nTotContacts;
		gtest.pnContacts = &nContacts;
		gtest.nMaxContacts = g_maxContacts-g_nTotContacts;
		gtest.pParams = pparams;
		gtest.R = pdata2->R;
		gtest.offset = pdata2->offset;
		gtest.scale = pdata2->scale;
		pCollider->RegisterIntersection(pprim[0],pprim[1],&gtest,(geometry_under_test *)(NULL),&inters);
		g_nTotContacts += nContacts;
		i=gtest.contacts->id[0]; gtest.contacts->id[0]=gtest.contacts->id[1]; gtest.contacts->id[1]=i;
		i=gtest.contacts->iPrim[0]; gtest.contacts->iPrim[0]=gtest.contacts->id[1]; gtest.contacts->iPrim[1]=i;
		pcontacts->n.Flip();
		return 1;
	}

	unprojection_mode unproj;
	unproj.vel = 0;
	contact contact_best;
	contact_best.t = 0;
	geom_contact_area *parea;
	unproj.minPtDist = min(m_minVtxDist,pCollider->m_minVtxDist);
	unproj.maxcos = 1-sqr(pparams->maxSurfaceGapAngle)*0.5f;

	if (!pparams->bNoAreaContacts && g_nAreas<CRY_ARRAY_COUNT(g_AreaBuf) && g_nAreaPt<CRY_ARRAY_COUNT(g_AreaPtBuf)) {
		parea = g_AreaBuf+g_nAreas;
		parea->pt = g_AreaPtBuf+g_nAreaPt;
		parea->piPrim[0] = g_AreaPrimBuf0+g_nAreaPt; 
		parea->piFeature[0] = g_AreaFeatureBuf0+g_nAreaPt;
		parea->piPrim[1] = g_AreaPrimBuf1+g_nAreaPt; 
		parea->piFeature[1] = g_AreaFeatureBuf1+g_nAreaPt;
		parea->npt = 0; parea->nmaxpt = CRY_ARRAY_COUNT(g_AreaPtBuf)-g_nAreaPt;
		parea->minedge = 0; parea->n1.zero();
	} else
		parea = 0;

	if (!pparams->bSweepTest) {
		Vec3 ptm(0);
		if (!pparams->bNoIntersection) {
			if (g_Intersector.CheckExists(itype[0],itype[1]))	{
				inters.ptborder = pcontacts->ptborder;
				inters.nborderpt = 0;
				if (!g_Intersector.Check(itype[0],itype[1], pprim[0],pprim[1], &inters))
					return 0;
				pcontacts->nborderpt = inters.nborderpt;
				pcontacts->center.zero();
				if (inters.nborderpt>0) {
					ptm = inters.ptborder[0];
					for(i=0;i<inters.nborderpt;i++) pcontacts->center += pcontacts->ptborder[i];
					pcontacts->center /= inters.nborderpt;
				}
			}
		}

		if (pparams->iUnprojectionMode==0) {
			if (pparams->vrel_min<1E9f) {
				Vec3 vrel;
				if (!pcontacts->nborderpt && pdata1->w.len2()+pdata2->w.len2()>0) {
					Vec3 center[2],pt[3]; box bbox;
					int iPrim,iFeature; // dummy parameters
					if (pBV1->type==box::type) center[0] = ((box*)(primitive*)*pBV1)->center;
					else { GetBBox(&bbox); center[0] = pdata1->R*bbox.center+pdata1->offset; }
					if (pBV1->type==box::type) center[1] = ((box*)(primitive*)*pBV2)->center;
					else { pCollider->GetBBox(&bbox); center[1] = pdata2->R*bbox.center+pdata2->offset; }
					FindClosestPoint(pdata1,iPrim,iFeature,center[1],center[1],pt+0);
					pCollider->FindClosestPoint(pdata2,iPrim,iFeature,center[0],center[0],pt+1);
					if (pCollider->PointInsideStatus(((pt[0]-pdata2->offset)*pdata2->R)*(pdata2->scale==1.0f ? 1.0f:1.0f/pdata2->scale)))
						ptm = pt[0];
					else if (PointInsideStatus(((pt[1]-pdata1->offset)*pdata1->R)*(pdata1->scale==1.0f ? 1.0f:1.0f/pdata1->scale)))
						ptm = pt[1];
					else
						ptm = (pt[0]+pt[1])*0.5f;
				}
				vrel = pdata1->v+(pdata1->w^ptm-pdata1->centerOfMass)-pdata2->v-(pdata2->w^ptm-pdata2->centerOfMass);
				if (vrel.len2()==0 && itype[1]==ray::type) {
					Vec3 raydir = ((ray*)pprim[1])->dir;
					vrel = (raydir^(inters.n^raydir)).normalized();
				}
				if (vrel.len2()>sqr(pparams->vrel_min)) {
					unproj.imode = 0;	// unproject along vrel
					(unproj.dir=-vrel) /= (unproj.vel=vrel.len());
					unproj.tmax = pparams->time_interval*unproj.vel;
					bUnprojected = g_Unprojector.Check(&unproj, itype[0],itype[1], pprim[0],-1,pprim[1],-1, &contact_best, parea);
					bUnprojected &= isneg(contact_best.t-unproj.tmax);
				}
			}
		} else {
			unproj.imode = 1;
			unproj.center = pparams->centerOfRotation;
			{//if (pparams->axisOfRotation.len2()==0) {
				if (itype[1]==ray::type)
					unproj.dir = inters.n^((ray*)pprim[1])->dir;
				else
					unproj.dir = inters.n^inters.pt[0]-unproj.center;
				if (unproj.dir.len2()<1E-6f)
					unproj.dir = inters.n.GetOrthogonal();
				unproj.dir.normalize();
			}
			if (pparams->axisOfRotation.len2()>0)
				unproj.dir = pparams->axisOfRotation*sgnnz(pparams->axisOfRotation*unproj.dir);
			bUnprojected = g_Unprojector.Check(&unproj, itype[0],itype[1], pprim[0],-1,pprim[1],-1, &contact_best, parea);
			if (bUnprojected)
				contact_best.t = atan2(contact_best.t,contact_best.taux);
		}

		if (!(bUnprojected | pparams->iUnprojectionMode)) {
			unproj.imode = 0;
			unproj.dir.zero(); // zero requested direction - means minimum direction will be found
			unproj.vel = 0; unproj.tmax = pparams->maxUnproj;
			unproj.bCheckContact = iszero(itype[0]-box::type|itype[1]-capsule::type)|iszero(itype[1]-box::type|itype[0]-capsule::type)|pparams->bNoIntersection;
			bUnprojected = g_Unprojector.Check(&unproj, itype[0],itype[1], pprim[0],-1,pprim[1],-1, &contact_best, parea);
		}
	}	else {
		unproj.imode = 0;
		unproj.dir = -sweepdir;
		unproj.tmax = unproj.vel = sweepstep;
		unproj.bCheckContact = 1;
		contact_best.t = 0;
		bUnprojected = g_Unprojector.Check(&unproj, itype[0],itype[1], pprim[0],-1,pprim[1],-1, &contact_best, parea);
		bUnprojected &= isneg(contact_best.t-unproj.tmax);
		if (bUnprojected && contact_best.n*unproj.dir>0) {
			// if we hit something with the back side, 1st primitive probably just passed through the 2nd one,
			// so move a little deeper than the contact and unproject again (some primitives check for minimal separating
			// unprojection, and some - for maximum contacting; the former can cause this)
			real t = contact_best.t;
			box bbox; pCollider->GetBBox(&bbox); 
			Vec3 dirloc,dirsz;
			dirloc = bbox.Basis*(sweepdir*pdata2->R);
			dirsz(fabs_tpl(dirloc.x)*bbox.size.y*bbox.size.z, fabs_tpl(dirloc.y)*bbox.size.x*bbox.size.z,	fabs_tpl(dirloc.z)*bbox.size.x*bbox.size.y);
			i = idxmax3((float*)&dirsz);
			t += bbox.size[i]/fabs_tpl(dirloc[i])*0.1f;
			unproj.tmax = sweepstep-t;
			pdata1->offset += sweepdir*unproj.tmax;
			itype[0] = PreparePrimitive(pdata1,pprim[0],iCaller);
			pdata1->offset -= sweepdir*unproj.tmax;
			bUnprojected = g_Unprojector.Check(&unproj, itype[0],itype[1], pprim[0],-1,pprim[1],-1, &contact_best, parea);
			bUnprojected &= isneg(contact_best.t-unproj.tmax) & isneg(contact_best.n*unproj.dir);
			contact_best.t += t;
			unproj.tmax = sweepstep;
		}
	}

	if (bUnprojected) {
		pcontacts->t = contact_best.t;
		if (pparams->bSweepTest)
			pcontacts->t = unproj.tmax-pcontacts->t;
		pcontacts->pt = contact_best.pt;
		pcontacts->n = contact_best.n.normalized();
		pcontacts->dir = unproj.dir;
		pcontacts->iUnprojMode = unproj.imode;
		pcontacts->vel = unproj.vel;
		pcontacts->id[0] = pcontacts->id[1] = -1;
		pcontacts->iPrim[0] = pcontacts->iPrim[1] = 0;
		pcontacts->iFeature[0] = contact_best.iFeature[0];
		pcontacts->iFeature[1] = contact_best.iFeature[1];
		pcontacts->iNode[0] = pcontacts->iNode[1] = -1;
		if (!parea || parea->npt==0)
			pcontacts->parea = 0;
		else {
			pcontacts->parea = parea;
			g_nAreas++; g_nAreaPt += parea->npt;
			if (pcontacts->nborderpt<parea->npt && (pcontacts->nborderpt==0 || (iszero(itype[0]-cylinder::type) | iszero(itype[1]-cylinder::type)))) {
				pcontacts->ptborder = parea->pt;
				pcontacts->nborderpt = parea->npt;
				for(i=0,pcontacts->center.zero(); i<parea->npt; i++) pcontacts->center += parea->pt[i];
				pcontacts->center /= parea->npt;
			}
		}
		if (pcontacts->nborderpt==0) {
			pcontacts->ptborder[0]=pcontacts->center = pcontacts->pt;
			pcontacts->nborderpt = 1;
		}
		pcontacts->bBorderConsecutive = false;
		g_nTotContacts++;
		g_BrdPtBufPos += pcontacts->nborderpt;
	}

	return bUnprojected;
}

int SphereCheckBVs(CBVTree *pBVtree, sphere *psph, BV *pBV, int iCaller)
{
	if (!g_Overlapper.Check(pBV->type,sphere::type, *pBV,psph))
		return 0;

	if (pBVtree->SplitPriority(pBV)>0) {
		BV *pBV_split1,*pBV_split2;
		pBVtree->GetNodeChildrenBVs(pBV, pBV_split1,pBV_split2, iCaller);
		int res = SphereCheckBVs(pBVtree,psph,pBV_split1,iCaller);
		if (!res)
			res = SphereCheckBVs(pBVtree,psph,pBV_split2,iCaller);
		pBVtree->ReleaseLastBVs(iCaller);
		return res;
	} else {
		int iStartPrim;
		pBVtree->GetNodeContentsIdx(pBV->iNode, iStartPrim);
		return iStartPrim+1;
	}
}

int CGeometry::SphereCheck(const Vec3 &center, float r, int iCaller)
{
	ResetGlobalPrimsBuffers(iCaller);
	sphere sph; sph.center=center; sph.r=r;

	if (IsAPrimitive()) {
		geom_world_data gwd;
		primitive *pprim;
		int itype = PreparePrimitive(&gwd,pprim,iCaller);
		return g_Overlapper.Check(itype,sphere::type, pprim,&sph); 
	}	else {
		CBVTree *pTree = GetBVTree();
		BV *pBV;
		pTree->GetNodeBV(pBV,0,iCaller);
		return SphereCheckBVs(pTree, &sph, pBV, iCaller);
	}
}

int CGeometry::IntersectLocked(IGeometry *pCollider, geom_world_data *pdata1,geom_world_data *pdata2, intersection_params *pparams, geom_contact *&pcontacts, 
		WriteLockCond &lock,int iCaller)
{
#if !defined(WriteLockCond)
	if (iCaller==MAX_PHYS_THREADS && (!IsPODThread(g_pPhysWorlds[0]) || !*g_pLockIntersect))
		SpinLock(lock.prw=g_pLockIntersect, 0,lock.iActive=WRITE_LOCK_VAL);
	int res = Intersect(pCollider, pdata1,pdata2, pparams, pcontacts);
	if (!res) {
		AtomicAdd(lock.prw,-lock.iActive); 
		lock.iActive = 0;
	}
	return res;
#else
	return Intersect(pCollider, pdata1,pdata2, pparams, pcontacts);
#endif 
}

void TestBV(CBVTree* pBVtree, BV *pBV, int iCaller, int level, bool& okay, int maxDepth)
{
	// Walk the entire tree testing the level depth
	okay = okay && (level < maxDepth);
	BBox* bbox = (BBox*)pBV;
	float split = pBVtree->SplitPriority(pBV);
	if (split>0 && okay)
	{
		BV *pBV_split1,  *pBV_split2;
		pBVtree->GetNodeChildrenBVs(pBV, pBV_split1, pBV_split2, iCaller);
		TestBV(pBVtree, pBV_split1, iCaller, level + 1, okay, maxDepth);
		TestBV(pBVtree, pBV_split2, iCaller, level + 1, okay, maxDepth);
		pBVtree->ReleaseLastBVs(iCaller);
	}
}

int SanityCheckTree(CBVTree* pBVtree, int maxDepth)
{
	BV* pBV;
	bool okay = true;
	int iCaller = get_iCaller(), locked=0;
	if (iCaller==MAX_PHYS_THREADS && (!IsPODThread(g_pPhysWorlds[0]) || !*g_pLockIntersect))
		SpinLock(g_pLockIntersect, 0,locked=WRITE_LOCK_VAL);
	ResetGlobalPrimsBuffers(iCaller);
	pBVtree->GetNodeBV(pBV, 0, iCaller);
	TestBV(pBVtree, pBV, iCaller, 0, okay, maxDepth);
	if (locked)
		AtomicAdd(g_pLockIntersect, -locked);
	return okay?1:0;
}

#undef g_Overlapper
