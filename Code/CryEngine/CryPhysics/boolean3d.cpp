// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"

#include "utils.h"
#include <CryPhysics/primitives.h>
#include "overlapchecks.h"
#include "intersectionchecks.h"
#include "unprojectionchecks.h"
#include "bvtree.h"
#include "aabbtree.h"
#include "obbtree.h"
#include "geometry.h"
#include "raybv.h"
#include "raygeom.h"
#include "trimesh.h"
#include "heightfieldgeom.h"


int g_bSaferBooleans = 1;
extern int g_nTriangulationErrors;


bop_meshupdate::~bop_meshupdate() 
{
	if (pRemovedVtx) delete[] pRemovedVtx;
	if (pRemovedTri) delete[] pRemovedTri;
	if (pNewVtx) delete[] pNewVtx;
	if (pNewTri) delete[] pNewTri;
	if (pWeldedVtx) delete[] pWeldedVtx;
	if (pTJFixes) delete[] pTJFixes;
	if (pMovedBoxes) delete[] pMovedBoxes;
	if (next) delete next;
	prevRef->nextRef = nextRef; nextRef->prevRef = prevRef;
	prevRef = nextRef = this;
	if (pMesh[0]) pMesh[0]->Release(); 
	if (pMesh[1]) pMesh[1]->Release();
}


// client processing: (has client vtx->phys vtx mapping table)
// remove deleted vertices (?how to map them to client vtx?)
// remove deleted triangles (using ids as indices) 
// add new triangles. add each vertex in this way:
//   	if it's an A or B vertex, select client vertex of this tri that maps to the specified phys vertex
//    (keep Bvtx->Avtx map, if slot is unused, add new vtx to A and put its index into the slot)
//    else add a copy of the vertex (calculating texture from idTri[0] or .[1], depending on iop of this tri

enum tessvtx_flags { contour_end=4, vtx_processed=8, vtx_instablept_log2=4, vtx_instable=4<<vtx_instablept_log2 };
struct tessvtx {
	int ivtx;
	int inext,inextBrd;
	int iprev;
	int icont; // -1 if source A, -2 if source B
	int itwin;
	int ipoly;
	float t;
	Vec3 pt;
	int flags;
};
struct tesspoly {
	int itri;
	int ivtx;
	int ivtx0;
	int ivtxCont;
	int nvtx;
	Vec3 n;
	int id;
	float holeArea;
	float areaOrg;
	char mat;
};

void insertBorderVtx(tessvtx *pVtx, int ivtx0, int ivtx, const Vec3 &n, int bEnd)
{
	int i;
	float t,e[4],estart[4] = { 0, (pVtx[ivtx0+1].pt-pVtx[ivtx0].pt).len2(), (pVtx[ivtx0+2].pt-pVtx[ivtx0+1].pt).len2(),
														 (pVtx[ivtx0].pt-pVtx[ivtx0+2].pt).len2() };
	Vec3 ptc = (pVtx[ivtx0].pt+pVtx[ivtx0+1].pt+pVtx[ivtx0+2].pt)*(1.0f/3), dpt = pVtx[ivtx].pt-ptc;
	e[0]=e[3] = min(estart[1],estart[3]); e[1] = min(estart[1],estart[2]); e[2] = min(estart[2],estart[3]);
	estart[3] += (estart[2] += estart[1]);

	for(i=0; i<2 && !((pVtx[ivtx0+i].pt-ptc ^ dpt)*n>0 && (pVtx[ivtx0+inc_mod3[i]].pt-ptc ^ dpt)*n<0); i++);
	pVtx[ivtx].t=t = (pVtx[ivtx].pt-pVtx[ivtx0+i].pt)*(pVtx[ivtx0+inc_mod3[i]].pt-pVtx[ivtx0+i].pt)+estart[i];

	for(; (pVtx[ivtx0].t<=t)+(pVtx[pVtx[ivtx0].inextBrd].t>t)+(pVtx[ivtx0].t>pVtx[pVtx[ivtx0].inextBrd].t)<2; ivtx0=pVtx[ivtx0].inextBrd);
	pVtx[ivtx].inextBrd = pVtx[ivtx0].inextBrd;
	pVtx[ivtx0].inextBrd = ivtx;
	pVtx[ivtx].flags |= i+1 & -bEnd;

	for(i=0; i<4 && fabs_tpl(t-estart[i])>e[i]*sqr(0.005f); i++);
	pVtx[ivtx].flags |= (i&4^4 | i&-isneg(i-3)) << vtx_instablept_log2;
}


int CTriMesh::Subtract(IGeometry *pGeom, geom_world_data *pdata1,geom_world_data *pdata2, int bLogUpdates)
{
	if (!pGeom || pGeom->GetType()!=GEOM_TRIMESH && pGeom->GetType()!=GEOM_HEIGHTFIELD || m_flags & mesh_no_booleans)
		return 0;
	CRY_PROFILE_FUNCTION(PROFILE_PHYSICS );

	int i,j,iop,icont,ncont,ipt,idx,ivtx,ivtx0,ivtx1,itri,itri1,imask,ipoly,flags,ivtxStart,ivtxEnd;
	int nVtx,nPolies,nTriSlots,nTriSlotsAlloc,nVtxSlots,nVtxSlotsAlloc,nBTris,nBTrisAlloc,nTris,nTrisAlloc,
			nTris0=m_nTris,nVtx0=m_nVertices,nErrors0=m_nErrors,nMaxPolyVtx,nNewVtx,nNewTris,bVtxMap,nVtxAlloc=m_nVertices,nTriAlloc=m_nTris, 
			nNewVtxAlloc,nNewTrisAlloc,nIsles,nTries=1,bCheckVolumes=0;
	int iCurTri[2],iCurVtx[2],iCurVtx0[2],iCurPoly[2],iVtxCntStart[2],iCurTwin[2],iTwin0[2],bIsolatedCnt[2],idmask[2],matmask[2];
	int *pTriSlots,*pVtxSlots,*pTris,*pBTris,*pBVtxMap,*pBVtxMapNew,*pVtxMap;
	int iCaller = get_iCaller();
	unsigned int *pTriMask[2],*pVtxMask[2];
	Vec3 n,edge,edge1,*pdata0=m_pVertices.data,*pBackupVertices=0,*pBackupNormals=0;
	int *pIds[2],dummyId,*pIds0=m_pForeignIdx,*pBackupIds=0,*pBackupVtxMap=0;
	char *pMats[2],dummyMat,*pMats0=m_pIds,*pBackupMats=0;
	index_t *pIndices0 = m_pIndices,*pBackupIndices=0;
	trinfo *pTopology0 = m_pTopology;
	CRayGeom rayGeom;
	box bbox;
	float e = sqr(m_minVtxDist*2*pdata1->scale),e1,e2,denom, rscale=1/pdata1->scale, V[3];
//	float rscale2[2]={1,pdata1->scale/pdata2->scale};
	tessvtx *pVtx;
	tesspoly *pPolies;
	CTriMesh *pMesh[2] = { this, (CTriMesh*)pGeom };
	geom_world_data gwd[2];
	intersection_params ip;
	geom_contact *pcont,*pcont1;
	bop_meshupdate *pRes=0;
	ip.bExactBorder = 1;
	ip.bThreadSafe = ip.bThreadSafeMesh = 1;

	{ ReadLock lock0(m_lockUpdate); 
		Matrix33 R;
		pMesh[0]->GetBBox(&bbox);
		n = pdata1->R*bbox.center*pdata1->scale + pdata1->offset;
		edge = ((R=pdata1->R*bbox.Basis.T()).Fabs()*bbox.size)*pdata1->scale;
		if (pGeom->GetType()!=GEOM_HEIGHTFIELD) { 
			pMesh[1]->GetBBox(&bbox);
			n -= pdata2->R*bbox.center*pdata2->scale + pdata2->offset;
			edge += ((R=pdata2->R*bbox.Basis.T()).Fabs()*bbox.size)*pdata2->scale;
			if (max(max(fabs_tpl(n.x)-edge.x,fabs_tpl(n.y)-edge.y), fabs_tpl(n.z)-edge.z)>=0)
				return 0;
		}
	}

	SScopedRandomSeedChange seedChange(12102012);

	{	WriteLock lock0(m_lockUpdate);
	if (!ExpandVtxList())
		return 0;

	if (m_pVertices.iStride!=sizeof(Vec3)) {
		m_pVertices.data = new Vec3[m_nVertices];
		for(i=0;i<m_nVertices;i++) m_pVertices.data[i] = strided_pointer<Vec3>(pdata0,m_pVertices.iStride)[i];
		m_pVertices.iStride = sizeof(Vec3);
	}
	for(i=0;i<2;i++) {
		if (pMesh[i]->m_pForeignIdx) {
			pIds[i] = pMesh[i]->m_pForeignIdx; idmask[i] = -1;
		} else {
			pIds[i] = &dummyId; idmask[i] = 0;
		}
		if (pMesh[i]->m_pIds) {
			pMats[i] = pMesh[i]->m_pIds; matmask[i] = -1;
		} else {
			pMats[i] = &dummyMat; matmask[i] = 0;
		}
	}
	if (m_pVtxMap) {
		pVtxMap = m_pVtxMap; bVtxMap = -1;
	}	else {
		pVtxMap = &dummyId; bVtxMap = 0;
	}

	gwd[1].scale = pdata2->scale*rscale;
	e1 = (bbox.size.x+bbox.size.y+bbox.size.z)*gwd[1].scale*0.003f;
	RetryWithJitter:
	gwd[1].R = pdata1->R.T()*pdata2->R;
	gwd[1].offset = ((pdata2->offset-pdata1->offset)*pdata1->R)*rscale;
	if (nTries>2) {
		if (nTries>=6)
			return 0;
		if (pGeom->GetType()!=GEOM_HEIGHTFIELD) { 
			edge.Set(cry_random(0.0f, 1.0f), cry_random(0.0f, 1.0f), cry_random(0.0f, 1.0f)).normalize();
			gwd[1].R *= Matrix33::CreateRotationAA(0.01f,edge);
		}	
		edge.Set(e1*cry_random(0.0f, 1.0f), e1*cry_random(0.0f, 1.0f), e1*cry_random(0.0f, 1.0f));
		gwd[1].offset += edge;
	}
	if (pGeom->GetType()==GEOM_HEIGHTFIELD) { 
		delete[] pMesh[1]->m_pIslands; pMesh[1]->m_pIslands = 0;
		delete[] pMesh[1]->m_pTri2Island; pMesh[1]->m_pTri2Island = 0;
	}
	ip.bKeepPrevContacts = false;

	ncont = Intersect(pGeom, gwd,gwd+1, &ip, pcont);
	if (pGeom->GetType()==GEOM_HEIGHTFIELD) {
		if (!ncont)
			return 0;
		gwd[1].offset += ((CHeightfield*)pGeom)->m_lastOriginOffs;
	}
	nIsles = pMesh[1]->BuildIslandMap();
	for(i=0;i<nIsles;i++) pMesh[1]->m_pIslands[i].bProcessed = 0;

	for(i=nVtx=0;i<ncont;i++) {
		for(j=1;j<pcont[i].nborderpt && (pcont[i].ptborder[j]-pcont[i].ptborder[0]).len2()<e;j++);
		if (j==pcont[i].nborderpt)
			pcont[i].nborderpt = 0;
		else {
			if (!pcont[i].bClosed) {
				++nTries; goto RetryWithJitter;
			}
			nVtx += pcont[i].nborderpt;
			pMesh[1]->m_pIslands[pMesh[1]->m_pTri2Island[pcont[i].idxborder[0][1] & IDXMASK].iprev].bProcessed = 1;
		}
	}

	nMaxPolyVtx = 0; nNewVtx = 0; nNewTris = 0;
	pPolies = new tesspoly[nVtx*2+1]; nPolies=0;
	pVtx = new tessvtx[nVtx*8+1];	nVtx=0; 
	for(iop=0;iop<2;iop++) {
		i = (pMesh[iop]->m_nVertices-1>>5)+1; memset(pVtxMask[iop] = new unsigned int[i], 0,i*4);
		i = (pMesh[iop]->m_nTris>>5)+1; memset(pTriMask[iop] = new unsigned int[i], 0,i*4);
	}
	pBVtxMap = new int[pMesh[1]->m_nVertices];
	pBVtxMapNew = new int[pMesh[1]->m_nVertices];
	pTriSlots = new int[nTriSlotsAlloc = 32];	nTriSlots=0;
	pVtxSlots = new int[nVtxSlotsAlloc = 32];	nVtxSlots=0;
	pBTris = new int[nBTrisAlloc = 32]; nBTris=0;
	pTris = new int[nTrisAlloc = 32];
	if (bLogUpdates)
		pRes = new bop_meshupdate;
	dummyId = 0; g_nTriangulationErrors = 0;

	for(icont=0; icont<ncont; icont++) {
		iCurTri[0]=iCurTri[1]=iCurVtx[0]=iCurVtx[1]=iVtxCntStart[0]=iVtxCntStart[1]=iCurTwin[0]=iCurTwin[1]=iTwin0[0]=iTwin0[1] = -1;
		bIsolatedCnt[0]=bIsolatedCnt[1] = 1;
		for(ipt=1; ipt<pcont[icont].nborderpt; ipt++) {
			for(iop=0; iop<2; iop++) {
				itri = pcont[icont].idxborder[ipt][iop] & IDXMASK;
				if (itri==iCurTri[iop]) {
					ivtx = iCurVtx[iop]; ipoly = iCurPoly[iop];
				} else {
					if (check_mask(pTriMask[iop], itri)) // if triangle is 'processed', lookup its ipoly
						for(ipoly=0; ipoly<nPolies && pPolies[ipoly].itri!=(iop<<30|itri); ipoly++);
					else { // triangle wasn't encountered before, initialize a poly for it
						set_mask(pTriMask[iop], itri);
						if (iop==0)	{
							if (nTriSlots>=nTriSlotsAlloc-1)
								ReallocateList(pTriSlots, nTriSlots,nTriSlotsAlloc+=16);
							pTriSlots[nTriSlots++] = itri; 
						}
						pPolies[ipoly = nPolies++].itri = iop<<30|itri;
						for(j=0;j<3;j++) {
							pVtx[nVtx+j].inextBrd = nVtx+inc_mod3[j];
							pVtx[nVtx+j].inext = -1;
							pVtx[nVtx+j].icont = -(iop+1);
							idx = 2-j; //idx = (iop-1&3)+(j^~-iop); // for boolean addition, use 2-j for A, j for B
							pVtx[nVtx+j].ivtx = pMesh[iop]->m_pIndices[itri*3+idx]; 
							pVtx[nVtx+j].pt = gwd[iop].R*pMesh[iop]->m_pVertices[pVtx[nVtx+j].ivtx]*gwd[iop].scale + gwd[iop].offset;
							pVtx[nVtx+j].flags = j+1 | vtx_processed;
							set_mask(pVtxMask[iop], pVtx[nVtx+j].ivtx);	// we need all 3 vertices marked (instead of just those that end up
							// in the polygon) since intersection points can be substituted with original vertices
						}
						pVtx[nVtx].t = 0;
						pVtx[nVtx+1].t = (pVtx[nVtx+1].pt-pVtx[nVtx].pt).len2();
						pVtx[nVtx+2].t = (pVtx[nVtx+2].pt-pVtx[nVtx+1].pt).len2()+pVtx[nVtx+1].t;
						pVtx[nVtx].ipoly=pVtx[nVtx+1].ipoly=pVtx[nVtx+2].ipoly = ipoly;
						pVtx[nVtx].itwin=pVtx[nVtx+1].itwin=pVtx[nVtx+2].itwin = -1;
						pPolies[ipoly].ivtx = pPolies[ipoly].ivtx0 = nVtx;
						pPolies[ipoly].ivtxCont = -1;
						pPolies[ipoly].nvtx = 3;
						pPolies[ipoly].n = gwd[iop].R*pMesh[iop]->m_pNormals[itri]*(1-iop*2);
						pPolies[ipoly].id = pIds[iop][itri & idmask[iop]];
						pPolies[ipoly].mat = pMats[iop][itri & matmask[iop]];
						pPolies[ipoly].holeArea = 0;
						pPolies[ipoly].areaOrg = (1-iop*2)*(pPolies[ipoly].n*(pVtx[nVtx+2].pt-pVtx[nVtx].pt ^ pVtx[nVtx+1].pt-pVtx[nVtx].pt));
						nVtx += 3;
					}
					// add the previous vertex to this triangle	as stripe start
					pVtx[nVtx].icont = icont; pVtx[nVtx].ivtx = ipt-1; 
					pVtx[nVtx].itwin = iCurTwin[iop];	
					iTwin0[iop] = iTwin0[iop]&~(ipt-2>>31) | nVtx&(ipt-2>>31); // set iTwin0 for ipt==1
					pVtx[nVtx].ipoly = ipoly;
					pVtx[nVtx].pt = pcont[icont].ptborder[ipt-1];
					pVtx[nVtx].flags = 0;
					ivtx = nVtx++; pPolies[ipoly].nvtx++; 
					iCurTri[iop]=itri; iCurPoly[iop]=ipoly;	iCurVtx0[iop]=ivtx;
					iVtxCntStart[iop] += iCurVtx0[iop]+1 & iVtxCntStart[iop]>>31;
				}
				pVtx[nVtx].inext = -1; pVtx[ivtx].inext = nVtx;
				pVtx[nVtx].icont = icont;
				//pVtx[nVtx].ivtx = ipt & ipt+1-pcont[icont].nborderpt>>31;	// replace nborderpt-1 with 0
				if (ipt<pcont[icont].nborderpt-1)	{
					pVtx[nVtx].ivtx = ipt; 
					pVtx[nVtx].itwin = -1;
				} else {
					pVtx[nVtx].ivtx = 0;
					pVtx[nVtx].itwin = iTwin0[iop];
				}
				iCurTwin[iop] = nVtx;
				pVtx[nVtx].ipoly = ipoly;
				pVtx[nVtx].pt = pcont[icont].ptborder[pVtx[nVtx].ivtx];
				pVtx[nVtx].flags = 0;
				pVtx[nVtx].inextBrd = -1;
				if (pcont[icont].idxborder[ipt][iop] & TRIEND) {
					if (pVtx[iCurVtx0[iop]].ivtx>0 || (pcont[icont].idxborder[pcont[icont].nborderpt-1][iop] & TRIEND))
						insertBorderVtx(pVtx, pPolies[ipoly].ivtx,iCurVtx0[iop], pPolies[ipoly].n*(iop*2-1), 0);
					insertBorderVtx(pVtx, pPolies[ipoly].ivtx,nVtx, pPolies[ipoly].n*(iop*2-1), 1);
					pPolies[ipoly].holeArea = -1E10f; // never remove border based on hole area if there are border intersections
					iCurVtx[iop] = -1; bIsolatedCnt[iop] = 0;
					nMaxPolyVtx = max(nMaxPolyVtx, ++pPolies[ipoly].nvtx); // account for a dummy vertex slot for each contour
				}	else
					iCurVtx[iop] = nVtx;
				nMaxPolyVtx = max(nMaxPolyVtx, ++pPolies[ipoly].nvtx);
				nVtx++;
			}
		}

		for(iop=0;iop<2;iop++) if (iCurVtx[iop]>=0) { // if the last triangle's contour didn't end, stitch its pending ends
			pVtx[iCurVtx[iop]].inext = iVtxCntStart[iop];
			nMaxPolyVtx = max(nMaxPolyVtx, ++pPolies[iCurPoly[iop]].nvtx); // account for a dummy vertex slot for each contour
			if (!bIsolatedCnt[iop])	{
				insertBorderVtx(pVtx, pPolies[iCurPoly[iop]].ivtx,iCurVtx0[iop], pPolies[iCurPoly[iop]].n*(iop*2-1), 0);
				pPolies[iCurPoly[iop]].holeArea = -1E10f; // never remove border based on hole area if there are border intersections
			} else { // we have a closed contour fully inside one triangle
				pVtx[iCurVtx[iop]].flags |= contour_end;
				pVtx[iCurVtx0[iop]].flags |= contour_end;
				pVtx[iCurVtx[iop]].inext = pPolies[iCurPoly[iop]].ivtxCont;
				pPolies[iCurPoly[iop]].ivtxCont = iCurVtx0[iop];
				n = pPolies[iCurPoly[iop]].n*(iop*2-1);
				for(ivtx=iCurVtx0[iop],edge.zero(); ivtx!=iCurVtx[iop]; ivtx=pVtx[ivtx].inext)
					edge += pVtx[ivtx].pt-n*(n*pVtx[ivtx].pt) ^ pVtx[pVtx[ivtx].inext].pt-n*(n*pVtx[pVtx[ivtx].inext].pt);
				edge += pVtx[ivtx].pt-n*(n*pVtx[ivtx].pt) ^ pVtx[iCurVtx0[iop]].pt-n*(n*pVtx[iCurVtx0[iop]].pt);
				pPolies[iCurPoly[iop]].holeArea += edge*n;
			}
		}
	}

	// if a vtx is instable and has a twin
	// find twin's triangle, find edge that has this tri as buddy, remove vtx from its pos, move it before or after the vtx
	for(i=0;i<nVtx;i++) pVtx[i].iprev = -1;
	for(i=0;i<nVtx;i++) {
		if (pVtx[i].itwin>=0) pVtx[pVtx[i].itwin].itwin = i;
		if (pVtx[i].inext>=0) pVtx[pVtx[i].inext].iprev = i;
	}
	for(i=0;i<nVtx;i++) if (pVtx[i].flags & vtx_instable && pVtx[i].itwin>=0) {
		ivtx0 = pPolies[pVtx[i].ipoly].ivtx;
		imask = pVtx[i].flags>>vtx_instablept_log2 & 7;
		for(ivtx=ivtx0; ((pVtx[ivtx].flags>>vtx_instablept_log2 & 7)!=imask || ivtx==i) && pVtx[ivtx].inextBrd!=ivtx0; ivtx=pVtx[ivtx].inextBrd);
		if ((pVtx[ivtx].flags>>vtx_instablept_log2 & 7)!=imask || ivtx==i)
			continue; // continue only if we have at least 2 instable points near one vertex
		for(ivtx=ivtx0; pVtx[ivtx].inextBrd!=i && pVtx[ivtx].inextBrd!=ivtx0; ivtx=pVtx[ivtx].inextBrd);
		if (pVtx[ivtx].inextBrd!=i)
			continue;
		iop = pPolies[pVtx[i].ipoly].itri>>30; 
		itri = pPolies[pVtx[i].ipoly].itri & (1<<30)-1;
		itri1 = pPolies[pVtx[pVtx[i].itwin].ipoly].itri & (1<<30)-1;
		ipt = pMesh[iop]->GetEdgeByBuddy(itri,itri1);
		if (pMesh[iop]->m_pTopology[itri].ibuddy[ipt]==itri1) {
			j = (pVtx[i].flags >> vtx_instablept_log2) & 3;
			ipt = dec_mod3[2-ipt];
			if (j==ipt)	{	// re-insert the vertex after ipt
				pVtx[ivtx].inextBrd = pVtx[i].inextBrd;
				pVtx[i].inextBrd = pVtx[ivtx0+ipt].inextBrd; pVtx[ivtx0+ipt].inextBrd = i; 
			} else if (j==inc_mod3[ipt]) { // re-insert the vertex before ipt+1
				pVtx[ivtx].inextBrd = pVtx[i].inextBrd;
				for(ivtx=ivtx0; pVtx[ivtx].inextBrd!=ivtx0+j; ivtx=pVtx[ivtx].inextBrd);
				pVtx[i].inextBrd = ivtx0+j; pVtx[ivtx].inextBrd = i;
			}
		}
	}

	// detect if any 2 points on a tri border are very close to each other
	// and their outgoing edges are criss-crossing. swap them if so
	if (nTries==1) for(i=0; i<nPolies; i++) {
		iop = pPolies[i].itri>>30; ivtx0 = pPolies[i].ivtx0;
		e2 = ((pVtx[ivtx0+1].pt-pVtx[ivtx0].pt).len2()+(pVtx[ivtx0+2].pt-pVtx[ivtx0+1].pt).len2()+(pVtx[ivtx0].pt-pVtx[ivtx0+2].pt).len2())*sqr(0.0001f);
		for(ivtx1=pPolies[i].ivtx; pVtx[ivtx1].inextBrd!=pPolies[i].ivtx; ivtx1=pVtx[ivtx1].inextBrd);
		for(ivtx0=pPolies[i].ivtx,ivtx=-1; ivtx!=pPolies[i].ivtx; ivtx1=ivtx0,ivtx0=ivtx)
		if (sqr(pVtx[ivtx = pVtx[ivtx0].inextBrd].t-pVtx[ivtx0].t)<e2 && 
				min(pVtx[ivtx].icont,pVtx[ivtx0].icont)>=0 && max(pVtx[ivtx0].inext*pVtx[ivtx].inext,pVtx[ivtx0].iprev*pVtx[ivtx].iprev)<0) 
		{
			edge = pVtx[max(pVtx[ivtx].inext,pVtx[ivtx].iprev)].pt - pVtx[ivtx].pt;
			edge1 = pVtx[max(pVtx[ivtx0].inext,pVtx[ivtx0].iprev)].pt - pVtx[ivtx0].pt;
			denom = (edge ^ edge1)*pPolies[i].n;
			if (denom*(iop*2-1)>0 && (pVtx[ivtx].pt-pVtx[ivtx0].pt).len2()<max(edge.len2(),edge1.len2())*sqr(0.01f))
					//inrange((pVtx[ivtx].pt-pVtx[ivtx0].pt^edge1)*pPolies[i].n, denom*-0.1f,denom*0.8f) && 
					//inrange((pVtx[ivtx].pt-pVtx[ivtx0].pt^edge )*pPolies[i].n, denom*-0.1f,denom*0.8f)) 
			{ // swap ivtx and ivtx0
				pVtx[ivtx1].inextBrd=ivtx; pVtx[ivtx0].inextBrd=pVtx[ivtx].inextBrd; pVtx[ivtx].inextBrd=ivtx0;
				ivtx1=ivtx0; ivtx0=ivtx; ivtx=ivtx1;
			}
		}
	}

	// arrange contours inside polygons
	for(i=0; i<nPolies; i++) {
		ivtxEnd=ivtxStart=-1; flags=0; ipt=0; 
		do { 
			for(ivtx0=pPolies[i].ivtx; pVtx[ivtx0].flags & vtx_processed && pVtx[ivtx0].inextBrd!=pPolies[i].ivtx; ivtx0=pVtx[ivtx0].inextBrd);
			if (pVtx[ivtx0].flags & vtx_processed)
				break;
			if (ivtxEnd<0) ivtxStart = ivtx0;
			else pVtx[ivtxEnd].inext = ivtx0;
			pVtx[ivtx0].flags |= flags;	ivtx = ivtx0;
			do {
				pVtx[ivtxEnd=ivtx].flags |= vtx_processed;
				pVtx[ivtx].inext += pVtx[ivtx].inextBrd+1 & pVtx[ivtx].inext>>31;
			} while ((ivtx=pVtx[ivtx].inext)!=ivtx0 && ++ipt<pPolies[i].nvtx*2);
			pVtx[ivtxEnd].flags |= flags;	
			flags = contour_end;
		}	while (++ipt<pPolies[i].nvtx*2);
		if (ipt>=pPolies[i].nvtx*2) {	
			BadContours:
			delete[] pBVtxMap; delete[] pBVtxMapNew; delete[] pTriSlots;
			delete[] pVtxSlots; delete[] pBTris; delete[] pTris;
			delete[] pTriMask[1];	delete[] pVtxMask[1]; 
			delete[] pTriMask[0];	delete[] pVtxMask[0]; 
			delete[] pPolies; delete[] pVtx;
			//--- Delete pRes -> nulling certain members to ensure that they are not prematurely or multiply deleted
			if (pRes) { pRes->pMesh[0]=pRes->pMesh[1]=0; pRes->pRemovedVtx = NULL; delete pRes; pRes=0; }
			nTries++; goto RetryWithJitter;
		}
		if (ivtxStart>=0)
			pPolies[i].ivtx = ivtxStart;
		else {
			for(j=0;j<3;j++) pVtx[pPolies[i].ivtx+j].inext = pPolies[i].ivtx+inc_mod3[j];
			ivtxEnd = (ivtxStart=pPolies[i].ivtx)+2;
		}
		if (pPolies[i].ivtxCont>=0) {
			pVtx[ivtxEnd].inext = pPolies[i].ivtxCont;
			for(ivtxEnd=pPolies[i].ivtxCont; pVtx[ivtxEnd].inext>=0; ivtxEnd=pVtx[ivtxEnd].inext);
			if (pPolies[i].holeArea>0) // we have a "positive" hole - remove triangle border from contour
				pPolies[i].ivtx=ivtxStart = pPolies[i].ivtxCont;
		}
		pVtx[ivtxEnd].inext = ivtxStart;
	}

/*#ifdef _DEBUG
if (!IsHeapValid())
  assert(0);
#endif*/

	// if an A polygon does not have a full edge, flood fill A triangles starting from it and remove them from A
	for(i=0; i<nPolies; i++) if (!(pPolies[i].itri & 1<<30)) {
		itri=pPolies[i].itri; ivtx=pPolies[i].ivtx; imask=0; ipt=0; do {
			imask |= 1<<(pVtx[ivtx].flags-1&3);
			if (pVtx[ivtx].icont<0)	// mark A vertices used by polygons
				set_mask(pVtxMask[0], pVtx[ivtx].ivtx);
		} while ((ivtx=pVtx[ivtx].inext)!=pPolies[i].ivtx && ++ipt<pPolies[i].nvtx*2);
		if (ipt>=pPolies[i].nvtx*2)
			goto BadContours;
		imask = imask&4 | (imask&2)>>1 | (imask&1)<<1; // since vertex order for A is reversed

		for(j=nTris=0; j<3; j++) if (m_pTopology[itri].ibuddy[j]>=0 && (imask>>j&1 | check_mask(pTriMask[0], m_pTopology[itri].ibuddy[j]))==0) {
			if (nTriSlots>=nTriSlotsAlloc-1)
				ReallocateList(pTriSlots, nTriSlots,nTriSlotsAlloc+=16);
			pTris[nTris++] = m_pTopology[itri].ibuddy[j]; pTriSlots[nTriSlots++] = m_pTopology[itri].ibuddy[j];
			set_mask(pTriMask[0], m_pTopology[itri].ibuddy[j]);
		}
		while (nTris) {
			itri = pTris[--nTris];
			for(j=0;j<3;j++) if (m_pTopology[itri].ibuddy[j]>=0 && !check_mask(pTriMask[0], m_pTopology[itri].ibuddy[j])) {
				if (nTriSlots>=nTriSlotsAlloc-1)
					ReallocateList(pTriSlots, nTriSlots,nTriSlotsAlloc+=16);
				pTris[nTris++] = m_pTopology[itri].ibuddy[j]; pTriSlots[nTriSlots++] = m_pTopology[itri].ibuddy[j];
				set_mask(pTriMask[0], m_pTopology[itri].ibuddy[j]);
			}
			if (nTris>nTrisAlloc-3)
				ReallocateList(pTris, nTris,nTrisAlloc+=16);
		}
	} pTriSlots[nTriSlots] = m_nTris;

	if (nTriSlots*2>m_nTris || g_bSaferBooleans && !m_nErrors) {
		bCheckVolumes = 1;
		V[0] = GetVolume(); V[1] = pMesh[1]->GetVolume()*cube(gwd[1].scale);
		memcpy(pBackupVertices = new Vec3[m_nVertices], &m_pVertices[0], m_nVertices*sizeof(m_pVertices[0]));
		memcpy(pBackupIndices = new index_t[m_nTris*3], m_pIndices, m_nTris*3*sizeof(m_pIndices[0]));
		memcpy(pBackupNormals = new Vec3[m_nTris], m_pNormals, m_nTris*sizeof(m_pNormals[0]));
		if (m_pIds) memcpy(pBackupMats = new char[m_nTris], m_pIds, m_nTris*sizeof(m_pIds[0]));
		if (m_pForeignIdx) memcpy(pBackupIds = new int[m_nTris], m_pForeignIdx, m_nTris*sizeof(m_pForeignIdx[0]));
		if (m_pVtxMap) memcpy(pBackupVtxMap = new int [m_nVertices], m_pVtxMap, m_nVertices*sizeof(m_pVtxMap[0]));
	}

	for(i=0; i<m_nTris; i++) if (!check_mask(pTriMask[0],i)) for(j=0;j<3;j++)
		set_mask(pVtxMask[0], m_pIndices[i*3+j]); // mark A vertices used by remaining tris
	for(i=0; i<m_nVertices; i++) { //	build a list of free vertex slots
		pVtxSlots[nVtxSlots] = i; 
		nVtxSlots += (check_mask(pVtxMask[0], pVtxMap[i&bVtxMap] | i&~bVtxMap) | check_mask(pVtxMask[0], i))^1;
		if (nVtxSlots>=nVtxSlotsAlloc-1)
			ReallocateList(pVtxSlots, nVtxSlots,nVtxSlotsAlloc+=16);
	} pVtxSlots[nVtxSlots] = m_nVertices;

	if (pRes) {
		pRes->pMesh[0] = pMesh[0];
		pRes->pMesh[1] = pMesh[1];
		pRes->pRemovedVtx = pVtxSlots;
		pRes->nRemovedVtx = nVtxSlots;
		pRes->pRemovedTri = new int[nTriSlots+1];
		for(i=0;i<nTriSlots;i++)
			pRes->pRemovedTri[i] = pIds[0][pTriSlots[i] & idmask[0]];
		pRes->nRemovedTri = nTriSlots;
	}

/*#ifdef _DEBUG
if (!IsHeapValid())
  assert(0);
#endif*/

	// if a B polygon has a full edge, flood fill B triangles starting from it and make a list of B's full triangles used
	for(i=0; i<nPolies; i++) if (pPolies[i].itri & 1<<30) {
		itri=pPolies[i].itri & (1<<30)-1; ivtx=ivtx0=pPolies[i].ivtx; imask=0; nTris=ipt=0;
		do {
			if (pVtx[ivtx].icont==-2) {
				set_mask(pVtxMask[1], pVtx[ivtx].ivtx);	// mark B vertices used by polygons
				idx = ivtx-pPolies[i].ivtx0; idx = 3-idx-inc_mod3[idx];
				if (pVtx[pVtx[ivtx].inext].icont==-2 &&
						(j = pMesh[1]->m_pTopology[itri].ibuddy[dec_mod3[3-(pVtx[ivtx].flags & 3)]])>=0 && !check_mask(pTriMask[1],j) &&
						(!(pVtx[ivtx0].flags & pVtx[pVtx[pVtx[ivtx].inext].inext].flags & vtx_instable) ||
							(pVtx[ivtx0].flags>>vtx_instablept_log2&3)==idx ||
							(pVtx[pVtx[pVtx[ivtx].inext].inext].flags>>vtx_instablept_log2&3)==idx)) // don't floodfill from instable edges
				{
					if (nBTris==nBTrisAlloc)
						ReallocateList(pBTris, nBTris,nBTrisAlloc+=16);
					pTris[nTris++] = j; pBTris[nBTris++] = j; set_mask(pTriMask[1], j);
				}
			}	ivtx0 = ivtx;
		} while ((ivtx=pVtx[ivtx].inext)!=pPolies[i].ivtx && ++ipt<pPolies[i].nvtx*2);
		if (ipt==pPolies[i].nvtx*2)
			goto BadContours;
		while (nTris) {
			itri = pTris[--nTris];
			for(j=0;j<3;j++) if (pMesh[1]->m_pTopology[itri].ibuddy[j]>=0 && !check_mask(pTriMask[1], pMesh[1]->m_pTopology[itri].ibuddy[j])) {
				itri1 = pTris[nTris++] = pMesh[1]->m_pTopology[itri].ibuddy[j]; 
				if (nBTris==nBTrisAlloc)
					ReallocateList(pBTris, nBTris,nBTrisAlloc+=16);
				pBTris[nBTris++] = itri1; set_mask(pTriMask[1], itri1);
			}
			if (nTris>nTrisAlloc-3)
				ReallocateList(pTris, nTris,nTrisAlloc+=16);
		}
	}

/*#ifdef _DEBUG
if (!IsHeapValid())
  assert(0);
#endif*/
	
	// add B triangles that belong to B islands that are fully inside A
	ip.bKeepPrevContacts = true;
	GetBBox(&bbox,1);
	for(i=0;i<nIsles;i++) if (!pMesh[1]->m_pIslands[i].bProcessed) {
		const Vec3 dirn(0,0,1);
		rayGeom.CreateRay(gwd[1].R*pMesh[1]->m_pVertices[pMesh[1]->m_pIndices[pMesh[1]->m_pIslands[i].itri*3]]*gwd[1].scale+gwd[1].offset,
			Vec3(0,0,bbox.size.z*2.5f),&dirn);
		if ((ipt=Intersect(&rayGeom, gwd,0, &ip, pcont1)) && pcont1[ipt-1].n.z>0)
			for(itri = pMesh[1]->m_pIslands[i].itri; itri!=0x7FFF; itri=pMesh[1]->m_pTri2Island[itri].inext) {
				if (nBTris==nBTrisAlloc)
					ReallocateList(pBTris, nBTris,nBTrisAlloc+=16);
				pBTris[nBTris++] = itri; set_mask(pTriMask[1], itri);
			}
	}	else 
		pMesh[1]->m_pIslands[i].bProcessed = 0;

	// mark B's vertices that B's full triangles use
	for(i=0; i<nBTris; i++) for(j=0;j<3;j++)
		set_mask(pVtxMask[1], pMesh[1]->m_pIndices[pBTris[i]*3+j]);
	// add used B vertices to A (using free slots), build a mapping table B index -> new A index
	for(i=0; i<pMesh[1]->m_nVertices; i++) if (check_mask(pVtxMask[1], i)) {
		if ((pBVtxMap[i] = ivtx = pVtxSlots[min(nVtxSlots,nNewVtx++)]) >= nVtxAlloc) {
			ReallocateList(m_pVertices.data, m_nVertices,nVtxAlloc=m_nVertices+32&~31);
			if (m_pVtxMap) pVtxMap = ReallocateList(m_pVtxMap, m_nVertices,nVtxAlloc);
		}
		pVtxSlots[nVtxSlots] = (m_nVertices += isneg(m_nVertices-1-ivtx));
		m_pVertices[ivtx] = gwd[1].R*pMesh[1]->m_pVertices[i]*gwd[1].scale+gwd[1].offset;
		pVtxMap[ivtx & bVtxMap] = ivtx;
	}
	if (pRes) {	// register the new vertices
		pRes->pNewVtx = new bop_newvtx[nNewVtxAlloc = max(1,pRes->nNewVtx = nNewVtx)];
		for(i=j=0; i<pMesh[1]->m_nVertices; i++) if (check_mask(pVtxMask[1], i)) {
			pBVtxMapNew[i] = j;
			pRes->pNewVtx[j].idx = pBVtxMap[i];
			pRes->pNewVtx[j].idxTri[0]=pRes->pNewVtx[j].idxTri[1] = 0;
			pRes->pNewVtx[j++].iBvtx = i;
		}
	}

/*#ifdef _DEBUG
if (!IsHeapValid())
  assert(0);
#endif*/

	// add all intersection points to A as new vertices (unless they are close enough to the corresponding tri vertices)
	for(icont=0; icont<ncont; icont++) for(ipt=0; ipt<pcont[icont].nborderpt-1; ipt++) {
		// check if each point is close enough to either A vertex or B vertex
		// stores: pcont[icont].idxborder[ipt][0] <- corresponding new vertex index in A
		//				 pcont[icont].idxborder[ipt][1]	<- (vertex index for bop_meshupdate.pNewTri.iVtx)*2 + (vertex modified flag)
		itri = pcont[icont].idxborder[ipt][0] & IDXMASK;
		for(j=0; j<3 && (pcont[icont].ptborder[ipt]-m_pVertices[m_pIndices[itri*3+j]]).len2()>e; j++);
		if (j==3) {
			itri = pcont[icont].idxborder[ipt][1] & IDXMASK;
			for(j=0; j<3 && (pcont[icont].ptborder[ipt]-m_pVertices[pBVtxMap[pMesh[1]->m_pIndices[itri*3+j]]]).len2()>e; j++);
			if (j==3) {
				if ((ivtx = pVtxSlots[min(nVtxSlots,nNewVtx)]) >= nVtxAlloc) {
					ReallocateList(m_pVertices.data, m_nVertices,nVtxAlloc=m_nVertices+32&~31);
					if (m_pVtxMap) pVtxMap = ReallocateList(m_pVtxMap, m_nVertices,nVtxAlloc);
				}
				pVtxSlots[nVtxSlots] = (m_nVertices += isneg(m_nVertices-1-ivtx));
				if (pRes) {
					if (nNewVtx==nNewVtxAlloc)
						ReallocateList(pRes->pNewVtx, nNewVtx,nNewVtxAlloc=nNewVtx+32&~31);
					pRes->pNewVtx[nNewVtx].idx = ivtx;
					pRes->pNewVtx[nNewVtx].iBvtx = -1;
					for(iop=0;iop<2;iop++)
						pRes->pNewVtx[nNewVtx].idxTri[iop] = pIds[iop][(itri = pcont[icont].idxborder[ipt][iop] & IDXMASK) & idmask[iop]];
				}
				pcont[icont].idxborder[ipt][1] = -(nNewVtx+++1)*2;
				m_pVertices[ivtx] = pcont[icont].ptborder[ipt];
				pVtxMap[ivtx & bVtxMap] = ivtx;
			} else {
				ivtx = pBVtxMap[pMesh[1]->m_pIndices[itri*3+j]];
				pcont[icont].idxborder[ipt][1] = -(pBVtxMapNew[pMesh[1]->m_pIndices[itri*3+j]]+1)*2+1;
			}
		} else 
			pcont[icont].idxborder[ipt][1] = (ivtx = m_pIndices[itri*3+j])*2+1;
		pcont[icont].idxborder[ipt][0] = ivtx;
	}

	// add full B triangles to A
	for(i=0,nTris=m_nTris; i<nBTris; i++) {
		if ((itri = pTriSlots[min(nTriSlots,nNewTris++)]) >= nTriAlloc) {
			ReallocateList(m_pIndices, m_nTris*3,(nTriAlloc=m_nTris+32&~31)*3); 
			ReallocateList(m_pNormals, m_nTris,nTriAlloc);
			if (m_pForeignIdx) pIds[0] = ReallocateList(m_pForeignIdx, m_nTris,nTriAlloc);
			if (m_pIds) pMats[0] = ReallocateList(m_pIds, m_nTris,nTriAlloc);
		}
		clear_mask(pTriMask[0],min(nTris0,itri));
		pTriSlots[nTriSlots] = (m_nTris += isneg(m_nTris-1-itri));
		for(j=0;j<3;j++) m_pIndices[itri*3+j] = pBVtxMap[pMesh[1]->m_pIndices[pBTris[i]*3+2-j]];
		m_pNormals[itri] = gwd[1].R*-pMesh[1]->m_pNormals[pBTris[i]];
		pIds[0][itri & idmask[0]] = m_iLastNewTriIdx+i;
		pMats[0][itri & matmask[0]] = pMats[1][pBTris[i] & matmask[1]];
	}
	if (pRes) {
		pRes->pNewTri = new bop_newtri[nNewTrisAlloc = max(1,pRes->nNewTri=nNewTris)];
		for(i=0; i<nBTris; i++) {
			pRes->pNewTri[i].idxNew = m_iLastNewTriIdx+i;
			pRes->pNewTri[i].iop = 1;
			pRes->pNewTri[i].idxOrg = pIds[1][pBTris[i] & idmask[1]];
			pRes->pNewTri[i].areaOrg = pMesh[1]->m_pNormals[pBTris[i]]*
				(pMesh[1]->m_pVertices[pMesh[1]->m_pIndices[pBTris[i]*3+1]]-pMesh[1]->m_pVertices[pMesh[1]->m_pIndices[pBTris[i]*3]] ^
				 pMesh[1]->m_pVertices[pMesh[1]->m_pIndices[pBTris[i]*3+2]]-pMesh[1]->m_pVertices[pMesh[1]->m_pIndices[pBTris[i]*3]]);
			for(j=0;j<3;j++) {
				pRes->pNewTri[i].iVtx[j] = -(pBVtxMapNew[pMesh[1]->m_pIndices[pBTris[i]*3+2-j]]+1);
				pRes->pNewTri[i].area[j].zero()[j] = pRes->pNewTri[i].areaOrg;
			}
		}
	}

/*#ifdef _DEBUG
if (!IsHeapValid())
  assert(0);
#endif*/

	// triangulate the polygons, add results to A
	int npt2d,bHasConts,dstflags,*ipt2d,*ipt2dnew,*pResTris,nResTrisAlloc;
	Vec3 axisx,axisy;
	Vec2 *pt2d;
	pt2d = new Vec2[nMaxPolyVtx+=4];
	ipt2d = new int[nMaxPolyVtx];
	ipt2dnew = new int[nMaxPolyVtx];
	pResTris = new int[nResTrisAlloc = 64];

	for(i=0; i<nPolies; i++) {
		iop = pPolies[i].itri>>30; itri = pPolies[i].itri & (1<<30)-1;
		n = pPolies[i].n*(iop*2-1); 
		for(j=0,axisx=n.GetOrthogonal();j<nTries;j++) axisx = axisx.GetRotated(n,0.6f,0.8f); 
		axisy = n^axisx;
		MARK_UNUSED pt2d[0].x; ipt2d[0]=-1; npt2d=1;

		for(dstflags=0,ncont=1; ;dstflags+=contour_end) {
			flags=0; bHasConts=0;
			ivtx = pPolies[i].ivtx; do { // always start from a point on triangle boundary, if it exists
			} while((pVtx[ivtx].flags&3)==0 && (ivtx=pVtx[ivtx].inext)!=pPolies[i].ivtx);
			ivtx0=ivtx; idx=-1; ipt=-1; do {
				flags ^= pVtx[ivtx].flags;
				if (((flags|pVtx[ivtx].flags) & contour_end) == dstflags) {
					if (pVtx[ivtx].icont>=0) {
						ipt2d[npt2d] = pcont[pVtx[ivtx].icont].idxborder[pVtx[ivtx].ivtx][0];
						ipt2dnew[npt2d] = pcont[pVtx[ivtx].icont].idxborder[pVtx[ivtx].ivtx][1];
					} else if (pVtx[ivtx].icont==-2) {
						ipt2d[npt2d] = pBVtxMap[pVtx[ivtx].ivtx];
						ipt2dnew[npt2d] = -(pBVtxMapNew[pVtx[ivtx].ivtx]+1)*2;
					} else
						ipt2dnew[npt2d] = (ipt2d[npt2d] = pVtx[ivtx].ivtx)*2;
					pt2d[npt2d].set(axisx*m_pVertices[ipt2d[npt2d]], axisy*m_pVertices[ipt2d[npt2d]]); 
					j = (iszero(ipt2d[npt2d-1]-ipt2d[npt2d]) | iszero(ipt2d[npt2d]-idx)) ^ 1;
					idx += ipt2d[npt2d]+1 & idx>>31; // stores the first vertex idx for a contour
					npt2d += j;	ipt = pVtx[ivtx].icont;
				}	else 
					bHasConts = 1;
				if (dstflags & (flags^contour_end) & pVtx[ivtx].flags & contour_end) {
					MARK_UNUSED pt2d[npt2d].x; ipt2d[npt2d]=-1; npt2d++;ncont++;idx=-1;
				}
			} while ((ivtx=pVtx[ivtx].inext)!=ivtx0);
			if (dstflags==0) {
				MARK_UNUSED pt2d[npt2d].x; ipt2d[npt2d++]=-1;
			}
			if (!bHasConts || dstflags==contour_end || npt2d>=nMaxPolyVtx)
				break;
		}

		if ((nTris = (npt2d+ncont*2)*3)>nResTrisAlloc)
			ReallocateList(pResTris, 0,nResTrisAlloc=nTris+48 & ~15);
		nTris = TriangulatePoly(pt2d+1,npt2d-1, pResTris,nResTrisAlloc);
		for(icont=0; icont<nTris; icont++) 
		if (!(iszero(ipt2d[pResTris[icont*3+0]+1]-ipt2d[pResTris[icont*3+1]+1]) | // skip degenerate tris
					iszero(ipt2d[pResTris[icont*3+0]+1]-ipt2d[pResTris[icont*3+2]+1]) | 
					iszero(ipt2d[pResTris[icont*3+1]+1]-ipt2d[pResTris[icont*3+2]+1]))) 
		{
			if ((itri = pTriSlots[min(nTriSlots,nNewTris)]) >= nTriAlloc) {
				ReallocateList(m_pIndices, m_nTris*3,(nTriAlloc=m_nTris+33&~31)*3); 
				ReallocateList(m_pNormals, m_nTris,nTriAlloc);
				if (m_pForeignIdx) pIds[0] = ReallocateList(m_pForeignIdx, m_nTris,nTriAlloc);
				if (m_pIds) pMats[0] = ReallocateList(m_pIds, m_nTris,nTriAlloc);
			}
			clear_mask(pTriMask[0],min(nTris0,itri));
			for(j=0;j<3;j++) {
				idx = (iop-1&3)+(j^~-iop); // 2-j for A, j for B
				m_pIndices[itri*3+j] = ipt2d[pResTris[icont*3+idx]+1];
			}
			n = (m_pVertices[m_pIndices[itri*3+1]]-m_pVertices[m_pIndices[itri*3]] ^ 
					 m_pVertices[m_pIndices[itri*3+2]]-m_pVertices[m_pIndices[itri*3]]);
			if (!((ipt2dnew[pResTris[icont*3]+1] | ipt2dnew[pResTris[icont*3+1]+1] | ipt2dnew[pResTris[icont*3+2]+1]) & 1))	{
				m_pNormals[itri] = pPolies[i].n;
				if (sqr_signed(m_pNormals[itri]*n) < n.len2()*0.999f)
					m_pNormals[itri] = n.normalized();
			} else
				m_pNormals[itri] = n.normalized();
			pIds[0][itri & idmask[0]] = m_iLastNewTriIdx+nNewTris;
			pMats[0][itri & matmask[0]] = pPolies[i].mat;
			pTriSlots[nTriSlots] = (m_nTris += isneg(m_nTris-1-itri));
			if (pRes) {
				if (nNewTris>=nNewTrisAlloc)
					ReallocateList(pRes->pNewTri, nNewTris,nNewTrisAlloc=nNewTris+32&~31);
				pRes->pNewTri[nNewTris].idxNew = m_iLastNewTriIdx+nNewTris;
				pRes->pNewTri[nNewTris].iop = iop;
				pRes->pNewTri[nNewTris].idxOrg = pPolies[i].id;
				pRes->pNewTri[nNewTris].areaOrg = pPolies[i].areaOrg;
				for(j=0;j<3;j++) {
					idx = (iop-1&3)+(j^~-iop); 
					pRes->pNewTri[nNewTris].iVtx[j] = ipt2dnew[pResTris[icont*3+idx]+1]>>1;
					for(ipt=0; ipt<3; ipt++)
						pRes->pNewTri[nNewTris].area[j][ipt] = (1-iop*2)*(pPolies[i].n*(
							pVtx[pPolies[i].ivtx0 + 2-inc_mod3[ipt]].pt-m_pVertices[m_pIndices[itri*3+j]] ^ 
							pVtx[pPolies[i].ivtx0 + 2-dec_mod3[ipt]].pt-m_pVertices[m_pIndices[itri*3+j]]));
				}
			}
			nNewTris++;
		}
	}

/*#ifdef _DEBUG
if (!IsHeapValid())
  assert(0);
#endif*/

	if (nNewTris<nTriSlots) {
		// compact triangle list if more triangles were deleted than added
		for(i=0,itri=m_nTris-1; true; i++,itri--) {
			for(; i<itri && !check_mask(pTriMask[0],i); i++);
			for(;	i<itri && check_mask(pTriMask[0],itri); itri--);
			if (i>=itri)
				break;
			for(j=0;j<3;j++) m_pIndices[i*3+j] = m_pIndices[itri*3+j];
			if (m_pIds) m_pIds[i] = m_pIds[itri];
			if (m_pForeignIdx) m_pForeignIdx[i] = m_pForeignIdx[itri];
			m_pNormals[i] = m_pNormals[itri];
		}
		nTris = m_nTris-nTriSlots+nNewTris;
		ReallocateList(m_pIndices, nTris*3,nTris*3);
		ReallocateList(m_pNormals, nTris,nTris);
		if (m_pIds) ReallocateList(m_pIds, nTris,nTris);
		if (m_pForeignIdx) ReallocateList(m_pForeignIdx, nTris,nTris);
		m_nTris = nTris;
	}
	// set pVtxMap of all empty vtx slots that are left unused to themselves (to avoid dangling unused vertices)
	for(i=nNewVtx;i<nVtxSlots;i++)
		pVtxMap[pVtxSlots[i] & bVtxMap] = pVtxSlots[i];
	if (pRes) {
		pRes->nNewTri = nNewTris;
		pRes->nNewVtx = nNewVtx;
	}

/*#ifdef _DEBUG
if (!IsHeapValid())
  assert(0);
#endif*/

	delete[] pResTris;
	delete[] ipt2dnew; delete[] ipt2d; delete[] pt2d; 
	delete[] pTris;
	delete[] pBTris;
	if (!pRes) delete[] pVtxSlots; delete[] pTriSlots; 
	delete[] pBVtxMapNew; delete[] pBVtxMap;
	delete[] pTriMask[1];	delete[] pVtxMask[1]; 
	delete[] pTriMask[0];	delete[] pVtxMask[0]; 
	delete[] pVtx; delete[] pPolies;	

	if (pdata0!=m_pVertices.data)
		m_flags &= ~mesh_shared_vtx;
	if (pIndices0!=m_pIndices)
		m_flags &= ~mesh_shared_idx;
	if (pMats0!=m_pIds)
		m_flags &= ~mesh_shared_mats;
	if (pIds0!=m_pForeignIdx)
		m_flags &= ~mesh_shared_foreign_idx;
	if (pTopology0!=m_pTopology)
		m_flags &= ~mesh_shared_topology;
	MARK_UNUSED m_V,m_center;

	i = 0;

	if (bCheckVolumes) {
		V[2] = GetVolume();
		if (i || !(V[1]>0 ? inrange(V[2], V[0]+V[1]*0.1f,V[0]-V[1]*1.1f) : isneg(V[2]-V[0]*1.1f)) || !m_nErrors && nTries<4 && g_bSaferBooleans && CalculateTopology(m_pIndices,1) || 
				g_nTriangulationErrors>0) 
		{
			MARK_UNUSED m_V,m_center;	m_nErrors = nErrors0;
			if (!(m_flags & mesh_shared_vtx)) delete[] m_pVertices.data; 
			if (!(m_flags & mesh_shared_idx)) delete[] m_pIndices;
			if (m_pIds && !(m_flags & mesh_shared_mats)) delete[] m_pIds;
			if (m_pForeignIdx && !(m_flags & mesh_shared_foreign_idx)) delete[] m_pForeignIdx;
			if (m_pVtxMap) delete[] m_pVtxMap;
			delete[] m_pNormals;
			nVtxAlloc=m_nVertices=nVtx0; nTriAlloc=m_nTris=nTris0;
			m_pVertices.data=pBackupVertices; m_pIndices=pBackupIndices; m_pNormals=pBackupNormals;
			if (m_pTopology!=pTopology0) {
				delete[] m_pTopology; m_pTopology = pTopology0;
			}
			if (m_pIds) pMats[0] = m_pIds = pBackupMats; 
			if (m_pForeignIdx) pIds[0] = m_pForeignIdx = pBackupIds;
			if (m_pVtxMap) pVtxMap = m_pVtxMap = pBackupVtxMap;
			bCheckVolumes = 0;
			if (pRes) { pRes->pMesh[0]=pRes->pMesh[1]=0; delete pRes; pRes=0; }
			nTries++; goto RetryWithJitter;
		}
		{
			delete[] pBackupVertices; delete[] pBackupIndices; delete[] pBackupNormals;
			if (m_pIds) delete[] pBackupMats;
			if (m_pForeignIdx) delete[] pBackupIds;
			if (m_pVtxMap) delete[] pBackupVtxMap;
		}
	}

	m_iLastNewTriIdx += nNewTris;
	m_nSubtracts++;

	// reallocate (if necessary) and recalculate connectivity information
	if (nNewTris>nTriSlots)	{
		ReallocateList(m_pTopology, 0,m_nTris, false,false);		
		delete[] pTopology0;
	}
	CalculateTopology(m_pIndices);

	if (pRes) {
		pRes->relScale = gwd[1].scale/gwd[0].scale;
		pRes->relRot = Quat(gwd[1].R*gwd[0].R.T());
		pRes->pMesh[0]->AddRef(); pRes->pMesh[1]->AddRef();
	}
	if (!m_pMeshUpdate)
		m_pMeshUpdate = pRes;
	else {
		bop_meshupdate *pbop;
		for(pbop=m_pMeshUpdate; pbop->next; pbop=pbop->next);
		pbop->next = pRes;
	}

	FilterMesh(m_minVtxDist*2,0.01f,bLogUpdates);

	if (m_pIslands) { delete[] m_pIslands; m_pIslands=0; }
	if (m_pTri2Island) { delete[] m_pTri2Island; m_pTri2Island=0; }
	m_nIslands = 0;
	for(i=1;i<CRY_ARRAY_COUNT(m_bConvex);i++) m_ConvexityTolerance[i] = -1;
	m_ConvexityTolerance[0] = 0.02f;
	m_bConvex[0] = 0;

	RebuildBVTree();
	}	// lock intersect

#ifdef _DEBUG
	for(i=0;i<m_nTris;i++) if ((e=(m_pVertices[m_pIndices[i*3+1]]-m_pVertices[m_pIndices[i*3]] ^ 
		m_pVertices[m_pIndices[i*3+2]]-m_pVertices[m_pIndices[i*3]]).normalized()*m_pNormals[i]) < 0.999f)
		j=i;
#endif

	return nTries;
}


inline int closest_edge(const triangle &tri, const Vec3 &pt, int iedge0,int iedge1)
{
	Vec3 edge[2] = { tri.pt[inc_mod3[iedge0]]-tri.pt[iedge0], tri.pt[inc_mod3[iedge1]]-tri.pt[iedge1] };
	return quotientf((pt-tri.pt[iedge0]^edge[0]).len2(), edge[0].len2()) < quotientf((pt-tri.pt[iedge1]^edge[1]).len2(), edge[1].len2()) ? iedge0:iedge1;
}

inline void snap_point(const triangle &tri,int iedge, Vec3 &pt, float minlen2, int *idx, int bExit, triangle &triCut)
{
	Vec3 edge = tri.pt[inc_mod3[iedge]]-tri.pt[iedge], pt0=pt; 
	float t=(pt-tri.pt[iedge])*edge, elen2=edge.len2();
	int ivtx = -1;
	if (t*t < elen2*minlen2) {
		pt=tri.pt[iedge]; idx[bExit^1]=ivtx=iedge;	idx[bExit]=-1;
	}	else if (sqr_signed(elen2-t) < elen2*minlen2) {
		pt=tri.pt[inc_mod3[iedge]]; idx[bExit^1]=-1; idx[bExit]=ivtx=inc_mod3[iedge];
	}	else
		idx[0]=idx[1]=-1;
	if (ivtx>=0 && (pt0-pt).len2()<sqr(0.001f))	{
		Vec3 nudge = (tri.pt[inc_mod3[ivtx]]+tri.pt[dec_mod3[ivtx]]-tri.pt[ivtx]*2).normalized()*0.001f;
		for(int j=0;j<3;j++) triCut.pt[j]+=nudge;
	}
}

inline int push_tri(index_t itri[4],int idxSrc, int ix,int iy,int iz, int flip=0)
{
	itri[0]=ix; itri[1]=iy+(iz-iy&-flip); itri[2]=iz+(iy-iz&-flip); itri[3]=idxSrc;
	return 1-(iszero(ix-iy)|iszero(ix-iz)|iszero(iy-iz));
}

int CTriMesh::Slice(const triangle *pcut, float minlen, float minArea)
{
	WriteLock lock(m_lockUpdate);
	int i,nNewTri=0,nRemovedTri=0,nNewVtx=0,ivtx=0;
	triangle tri;
	prim_inters inters;
	index_t (*newTri)[4],*removedTri;
	Vec3 *newVtx,*bufNewVtx;
	int vtx[8],idx[2];
	float minlen2 = sqr(minlen);
	newVtx = (bufNewVtx = new Vec3[m_nVertices*2])+1;
	newTri = new index_t[m_nTris*2][4];
	removedTri = new index_t[m_nTris];
	newVtx[-1] = Vec3(1e10f);
	if (!m_pVtxMap) {
		m_pVtxMap = new int[m_nVertices];
		for(i=0;i<m_nVertices;i++) m_pVtxMap[i]=i;
	}

	for(i=0;i<m_nTris;i++) {
		int itri=i,itri1,iedge0,iedge1,nNewTri0=nNewTri,nNewVtx0=nNewVtx,nRemovedTri0=nRemovedTri;
		triangle triCut = *pcut;
		const index_t *pidx = m_pIndices+itri*3;
		for(int j=0;j<3;j++) tri.pt[j]=m_pVertices[pidx[j]]; tri.n=m_pNormals[i];
		if (tri_tri_intersection(&tri,&triCut,&inters) && (inters.iFeature[0][1] & 0x20 || m_pTopology[i].ibuddy[inters.iFeature[0][0] & 3]<0)) {
			if (inters.iFeature[1][1] & 0x20 || m_pTopology[i].ibuddy[inters.iFeature[1][0] & 3]<0)	
				continue;
			if (inters.iFeature[0][1] & 0x20) {
				iedge0 = closest_edge(tri, inters.pt[0], inc_mod3[inters.iFeature[1][0]&3],dec_mod3[inters.iFeature[1][0]&3]);
				for(int j=0;j<2;j++) vtx[++ivtx&=7] = m_nVertices+nNewVtx; 
				newVtx[nNewVtx++] = inters.pt[0];
				nNewTri += push_tri(newTri[nNewTri],itri, m_nVertices+nNewVtx-1,pidx[iedge0],pidx[inc_mod3[iedge0]]);
			} else {
				iedge0 = inters.iFeature[0][0] & 3;
				snap_point(tri, iedge0=inters.iFeature[0][0] & 3, inters.pt[0], minlen2, idx,0, triCut); 
				for(int j=0;j<2;j++) {
					int mask=idx[j]>>31; newVtx[nNewVtx]=inters.pt[0]; nNewVtx-=mask;
					vtx[++ivtx&=7] = pidx[idx[j]] & ~mask | m_nVertices+nNewVtx-1 & mask;
				}
			}
			do {
				pidx = m_pIndices+itri*3;
				for(int j=0;j<3;j++) tri.pt[j]=m_pVertices[pidx[j]]; tri.n=m_pNormals[itri];
				if (!tri_tri_intersection(&tri,&triCut,&inters))	{
					nNewTri=nNewTri0; nNewVtx=nNewVtx0; nRemovedTri=nRemovedTri0; break;
				}
				removedTri[nRemovedTri++] = itri;
				if (inters.iFeature[1][0] & 0x20) {
					iedge1 = inters.iFeature[1][0] & 3;
					snap_point(tri, iedge1=inters.iFeature[1][0] & 3, inters.pt[1], minlen2, idx,1, triCut); 
					for(int j=0;j<2;j++) {
						int mask=idx[j]>>31;
						if ((idx[0]&idx[1])<0 || (newVtx[nNewVtx-1]-inters.pt[1]).len2()>0) {
							newVtx[nNewVtx]=inters.pt[1]; nNewVtx-=mask;
						}
						vtx[++ivtx&=7] = pidx[idx[j]] & ~mask | m_nVertices+nNewVtx-1 & mask;
					}
					nNewTri += push_tri(newTri[nNewTri],itri, vtx[ivtx],pidx[iedge0],vtx[ivtx-2&7]);
					nNewTri += push_tri(newTri[nNewTri],itri, vtx[ivtx-1&7],vtx[ivtx-3&7],pidx[inc_mod3[iedge0]]);
					int flip = iszero(iedge1-inc_mod3[iedge0]);
					nNewTri += push_tri(newTri[nNewTri],itri, pidx[dec_mod3[iedge0]],vtx[ivtx-1+flip&7],pidx[dec_mod3[iedge1]], flip);
					if ((itri1 = m_pTopology[itri].ibuddy[iedge1])<0)
						break;
					iedge0=GetEdgeByBuddy(itri1,itri); itri=itri1;
				}	else {
					newVtx[nNewVtx++] = inters.pt[1];
					nNewTri += push_tri(newTri[nNewTri],itri, m_nVertices+nNewVtx-1,pidx[iedge0],vtx[ivtx]);
					nNewTri += push_tri(newTri[nNewTri],itri, m_nVertices+nNewVtx-1,vtx[ivtx-1&7],pidx[inc_mod3[iedge0]]);
					nNewTri += push_tri(newTri[nNewTri],itri, m_nVertices+nNewVtx-1,pidx[dec_mod3[iedge0]],pidx[iedge0]);
					nNewTri += push_tri(newTri[nNewTri],itri, m_nVertices+nNewVtx-1,pidx[inc_mod3[iedge0]],pidx[dec_mod3[iedge0]]);
					break;
				}
			} while(true);
		}
	}
	if (!nNewTri)
		return 0;

	ReallocateList(m_pVertices.data, m_nVertices,m_nVertices+nNewVtx);
	ReallocateList(m_pVtxMap, m_nVertices,m_nVertices+nNewVtx);
	for(i=0;i<nNewVtx;i++) { m_pVertices.data[m_nVertices+i]=newVtx[i]; m_pVtxMap[m_nVertices+i]=m_nVertices+i; }

	if (m_pForeignIdx) {
		bop_meshupdate *pmd = new bop_meshupdate;
		pmd->pMesh[0]=pmd->pMesh[1] = this;	AddRef();AddRef();
		pmd->pRemovedTri = new int[pmd->nRemovedTri = nRemovedTri];
		for(i=0;i<nRemovedTri;i++) pmd->pRemovedTri[i] = m_pForeignIdx[removedTri[i]];
		memset(pmd->pNewVtx = new bop_newvtx[pmd->nNewVtx = nNewVtx], 0, nNewVtx*sizeof(bop_newvtx));
		for(i=0;i<nNewVtx;i++) { pmd->pNewVtx[i].idx=m_nVertices+i; pmd->pNewVtx[i].iBvtx=-1; }
		pmd->pNewTri = new bop_newtri[pmd->nNewTri = nNewTri];
		int iLastNewTriIdx0 = m_iLastNewTriIdx;
		for(i=0;i<nNewTri;i++) {
			pmd->pNewTri[i].iop = 0;
			pmd->pNewTri[i].idxNew = m_iLastNewTriIdx++;
			pmd->pNewTri[i].idxOrg = m_pForeignIdx[newTri[i][3]];
			const index_t *pidx = m_pIndices+newTri[i][3]*3;
			Vec3 n = m_pNormals[newTri[i][3]];
			for(int j=0;j<3;j++) { 
				int mask=~(newTri[i][j]-m_nVertices>>31); pmd->pNewTri[i].iVtx[j] = (newTri[i][j]+mask^mask)+(m_nVertices-1&mask);	
				for(int j1=0;j1<3;j1++) pmd->pNewTri[i].area[j][j1] = 
					(m_pVertices[pidx[dec_mod3[j1]]]-m_pVertices[pidx[inc_mod3[j1]]] ^ m_pVertices[newTri[i][j]]-m_pVertices[pidx[inc_mod3[j1]]])*n;
			}
			pmd->pNewTri[i].areaOrg = (m_pVertices[pidx[1]]-m_pVertices[pidx[0]]^m_pVertices[pidx[2]]-m_pVertices[pidx[0]])*n;
		}
		if (!m_pMeshUpdate)
			m_pMeshUpdate = pmd;
		else {
			bop_meshupdate *pbop;
			for(pbop=m_pMeshUpdate; pbop->next; pbop=pbop->next);
			pbop->next = pmd;
		}
		ReallocateList(m_pForeignIdx, m_nTris,m_nTris+nNewTri-nRemovedTri);
		for(i=0;i<nRemovedTri;i++)
			m_pForeignIdx[removedTri[i]] = iLastNewTriIdx0+i;
		for(;i<nNewTri;i++)
			m_pForeignIdx[m_nTris+i-nRemovedTri] = iLastNewTriIdx0+i;
	}

	m_nVertices += nNewVtx;
	ReallocateList(m_pIndices, m_nTris*3,(m_nTris+nNewTri-nRemovedTri)*3);
	ReallocateList(m_pNormals, m_nTris,m_nTris+nNewTri-nRemovedTri);
	ReallocateList(m_pTopology, m_nTris,m_nTris+nNewTri-nRemovedTri);
	if (m_pIds) ReallocateList(m_pIds, m_nTris,m_nTris+nNewTri-nRemovedTri, true);
	for(i=0;i<nRemovedTri;i++) {
		((Vec3_tpl<index_t>*)m_pIndices)[removedTri[i]] = *(Vec3_tpl<index_t>*)newTri[i];
		RecalcTriNormal(removedTri[i]);
	}
	for(;i<nNewTri;i++) {
		PREFAST_ASSUME(i>=0 && i<m_nTris*2);
		((Vec3_tpl<index_t>*)m_pIndices)[m_nTris+i-nRemovedTri] = *(Vec3_tpl<index_t>*)newTri[i];
		RecalcTriNormal(m_nTris+i-nRemovedTri);
	}
	m_nTris += nNewTri-nRemovedTri;
	delete[] removedTri; delete[] newTri; delete[] bufNewVtx;
	CalculateTopology(m_pIndices);

	if (minArea>0) {
		float area=0;
		if (m_pIslands)	delete[] m_pIslands; m_pIslands=0; m_nIslands=0;
		BuildIslandMap();
		for(i=0; i<m_nIslands; area+=m_pIslands[i++].V) for(int itri=(m_pIslands[i].V=0,m_pIslands[i].itri); itri!=0x7FFF; itri=m_pTri2Island[itri].inext)
			m_pIslands[i].V += (m_pVertices[m_pIndices[itri*3+1]]-m_pVertices[m_pIndices[itri*3]]^m_pVertices[m_pIndices[itri*3+2]]-m_pVertices[m_pIndices[itri*3]])*m_pNormals[itri];
		for(i=0,nRemovedTri=0; i<m_nIslands; i++) if (m_pIslands[i].V < area*minArea)
			for(int itri=m_pIslands[i].itri; itri!=0x7FFF; itri=m_pTri2Island[itri].inext)
				m_pTri2Island[itri].bFree=1, nRemovedTri++;
		if (nRemovedTri && m_pForeignIdx) {
			Vec3_tpl<index_t> *pIdx = (Vec3_tpl<index_t>*)m_pIndices;
			bop_meshupdate *pmd = new bop_meshupdate, *pmd0;
			pmd->pMesh[0]=pmd->pMesh[1] = this;	AddRef();AddRef();
			for(pmd0=m_pMeshUpdate; pmd0->next; pmd0=pmd0->next);
			pmd0->next = pmd;
			pmd->pRemovedTri = new int[pmd->nRemovedTri = nRemovedTri];
			for(int i0=i=nRemovedTri=0;i0<m_nTris;i0++) if (!m_pTri2Island[i0].bFree) {
				pIdx[i]=pIdx[i0]; m_pNormals[i]=m_pNormals[i0];	m_pForeignIdx[i++]=m_pForeignIdx[i0];
			}	else
				pmd->pRemovedTri[nRemovedTri++] = m_pForeignIdx[i0];
			m_nTris = i;
			CalculateTopology(m_pIndices);
			delete[] m_pIslands; m_pIslands=0; m_nIslands=0;
		}
	}
	RebuildBVTree();
	return 1;
}