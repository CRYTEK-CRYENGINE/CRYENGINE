// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"

#include "utils.h"
#include <CryPhysics/primitives.h>
#include "bvtree.h"
#include "geometry.h"
#include "trimesh.h"
#include "voxelbv.h"

struct InitVoxgridGlobals {
	InitVoxgridGlobals() {
		for(int iCaller=0; iCaller<=MAX_PHYS_THREADS; iCaller++) {
			g_BVvox.iNode = 0;
			g_BVvox.type = voxelgrid::type;
		}
	}
};
static InitVoxgridGlobals initVoxgridGlobals;


void CVoxelBV::GetBBox(box *pbox)
{
	for(int i=0;i<3;i++)
		pbox->size[i] = m_pgrid->size[i]*m_pgrid->step[i]*0.5f;
	pbox->center = m_pgrid->origin+pbox->size;
	pbox->Basis.SetIdentity();
	pbox->bOriented = 0;
}

void CVoxelBV::GetNodeBV(BV *&pBV,int iNode,int iCaller)
{
	pBV = &g_BVvox;
	g_BVvox.voxgrid.origin = m_pgrid->origin + Vec3(m_iBBox[0].x*m_pgrid->step.x, m_iBBox[0].y*m_pgrid->step.y, m_iBBox[0].z*m_pgrid->step.z);
	g_BVvox.voxgrid.step = m_pgrid->step;
	g_BVvox.voxgrid.stepr = m_pgrid->stepr;
	g_BVvox.voxgrid.size = m_iBBox[1]-m_iBBox[0];
	g_BVvox.voxgrid.stride = m_pgrid->stride;
	g_BVvox.voxgrid.Basis.SetIdentity();
	g_BVvox.voxgrid.bOriented = 0;

	g_BVvox.voxgrid.R.SetIdentity();
	g_BVvox.voxgrid.offset.zero();
	g_BVvox.voxgrid.scale = g_BVvox.voxgrid.rscale = 1;
	g_BVvox.voxgrid.pVtx = m_pgrid->pVtx;
	g_BVvox.voxgrid.pIndices = m_pgrid->pIndices;
	g_BVvox.voxgrid.pNormals = m_pgrid->pNormals;
	g_BVvox.voxgrid.pCellTris = m_pgrid->pCellTris+m_iBBox[0]*m_pgrid->stride;
	g_BVvox.voxgrid.pTriBuf = m_pgrid->pTriBuf;
}

void CVoxelBV::GetNodeBV(const Matrix33 &Rw,const Vec3 &offsw,float scalew, BV *&pBV, int iNode, int iCaller)
{
	pBV = &g_BVvox;
	g_BVvox.voxgrid.origin = m_pgrid->origin + Vec3(m_iBBox[0].x*m_pgrid->step.x, m_iBBox[0].y*m_pgrid->step.y, m_iBBox[0].z*m_pgrid->step.z);
	g_BVvox.voxgrid.origin = Rw*g_BVvox.voxgrid.origin*scalew + offsw;
	g_BVvox.voxgrid.step = m_pgrid->step*scalew;
	g_BVvox.voxgrid.stepr = m_pgrid->stepr*(scalew==1.0f ? 1:1/scalew);
	g_BVvox.voxgrid.size = m_iBBox[1]-m_iBBox[0];
	g_BVvox.voxgrid.stride = m_pgrid->stride;
	g_BVvox.voxgrid.Basis = Rw.T();
	g_BVvox.voxgrid.bOriented = 1;

	g_BVvox.voxgrid.R = Rw;
	g_BVvox.voxgrid.offset = offsw;
	g_BVvox.voxgrid.scale = scalew;
	g_BVvox.voxgrid.rscale = scalew==1.0f ? 1.0f : 1/scalew;
	g_BVvox.voxgrid.pVtx = m_pgrid->pVtx;
	g_BVvox.voxgrid.pIndices = m_pgrid->pIndices;
	g_BVvox.voxgrid.pNormals = m_pgrid->pNormals;
	g_BVvox.voxgrid.pCellTris = m_pgrid->pCellTris+m_iBBox[0]*m_pgrid->stride;
	g_BVvox.voxgrid.pTriBuf = m_pgrid->pTriBuf;
}


void project_box_on_3dgrid(box *pbox,grid3d *pgrid, geometry_under_test *pGTest, Vec3i *iBBox)
{
	Vec3 center,dim;
	if (!pGTest) {
		Matrix33 Basis = pbox->Basis;
		dim = pbox->size*Basis.Fabs();
		center = pbox->center;
	} else {
		Matrix33 Basis;
		if (pbox->bOriented)
			Basis = pbox->Basis*pGTest->R_rel;
		else
			Basis = pGTest->R_rel;
		dim = (pbox->size*pGTest->rscale_rel)*Basis.Fabs();
		center = ((pbox->center-pGTest->offset_rel)*pGTest->R_rel)*pGTest->rscale_rel;
	}
	center -= pgrid->origin;
	for(int i=0;i<3;i++) {
		iBBox[0][i] = min(pgrid->size[i],max(0, float2int((center[i]-dim[i])*pgrid->stepr[i]-0.5f)));
		iBBox[1][i] = min(pgrid->size[i],max(0, float2int((center[i]+dim[i])*pgrid->stepr[i]+0.5f)));
	}
}

void CVoxelBV::MarkUsedTriangle(int itri, geometry_under_test *pGTest)
{
	pGTest->pUsedNodesMap[itri>>5] |= 1<<(itri&31);
	pGTest->pUsedNodesIdx[pGTest->nUsedNodes = min(pGTest->nUsedNodes+1,pGTest->nMaxUsedNodes-1)] = itri;
}

int CVoxelBV::GetNodeContents(int iNode, BV *pBVCollider,int bColliderUsed,int bColliderLocal, 
															geometry_under_test *pGTest,geometry_under_test *pGTestOp)
{
	int i,icell,nPrims=0,nTris=0,nTrisDst;
	Vec3i iBBox[2],ic;
	indexed_triangle *ptri;
	const int MAXTESTRIS = 256;
	int idxbuf[MAXTESTRIS*2], *plist=idxbuf, *plistDst=idxbuf+MAXTESTRIS;
	intptr_t idmask = ~iszero_mask(m_pMesh->m_pIds);
	char idnull=(char)-1, *ptrnull=&idnull, *pIds = (char*)((intptr_t)m_pMesh->m_pIds&idmask | (intptr_t)ptrnull&~idmask);

	if (pBVCollider->type==box::type)	{
		project_box_on_3dgrid((box*)(primitive*)*pBVCollider,m_pgrid, (geometry_under_test*)((intptr_t)pGTest & -((intptr_t)bColliderLocal^1)), iBBox);
		iBBox[0] = max(iBBox[0], m_iBBox[0]); iBBox[1] = min(iBBox[1], m_iBBox[1]);
	} else {
		iBBox[0] = m_iBBox[0]; iBBox[1] = m_iBBox[1];
	}

	for(ic.z=iBBox[0].z;ic.z<iBBox[1].z;ic.z++) for(ic.y=iBBox[0].y;ic.y<iBBox[1].y;ic.y++) for(ic.x=iBBox[0].x;ic.x<iBBox[1].x;ic.x++)	{
		icell = ic*m_pgrid->stride;
		nTrisDst = unite_lists(plist,nTris, m_pgrid->pTriBuf+m_pgrid->pCellTris[icell],m_pgrid->pCellTris[icell+1]-m_pgrid->pCellTris[icell], 
			plistDst,MAXTESTRIS);
		swap(nTris,nTrisDst); swap(plist,plistDst);
	}
	for(i=0;i<nTris;i++) if (!(bColliderUsed & pGTest->pUsedNodesMap[plist[i]>>5]>>(plist[i]&31) & 1)) {
		ptri = (indexed_triangle*)((char*)pGTest->primbuf+nPrims*pGTest->szprim);
		m_pMesh->PrepareTriangle(ptri->idx=plist[i], ptri, pGTest);
		pGTest->idbuf[nPrims++] = pIds[plist[i] & idmask];
	}

	return nPrims;
}


int CVoxelBV::PrepareForIntersectionTest(geometry_under_test *pGTest, CGeometry *pCollider,geometry_under_test *pGTestColl)
{
	int mapsz = (m_pMesh->m_nTris-1>>5)+1;
	if (mapsz<=(int)(CRY_ARRAY_COUNT(g_UsedNodesMap))-g_UsedNodesMapPos) {
		pGTest->pUsedNodesMap = g_UsedNodesMap+g_UsedNodesMapPos; g_UsedNodesMapPos += mapsz;
	} else 
		pGTest->pUsedNodesMap = new int[mapsz];
	pGTest->pUsedNodesIdx = g_UsedNodesIdx+g_UsedNodesIdxPos;
	pGTest->nMaxUsedNodes = min(32,(int)(CRY_ARRAY_COUNT(g_UsedNodesIdx)-g_UsedNodesIdxPos));
	g_UsedNodesIdxPos += pGTest->nMaxUsedNodes;
	pGTest->nUsedNodes = -1;

	box abox,aboxext,*pbox; pCollider->GetBBox(&abox);
	if (pGTestColl->sweepstep>0) {
		ExtrudeBox(&abox, pGTestColl->sweepdir_loc,pGTestColl->sweepstep_loc, &aboxext);
		pbox = &aboxext;
	} else pbox = &abox;
	project_box_on_3dgrid(pbox,m_pgrid, pGTest, m_iBBox);

	Vec3i ic;
	m_nTris = 0;
	for(ic.z=m_iBBox[0].z;ic.z<m_iBBox[1].z;ic.z++) for(ic.y=m_iBBox[0].y;ic.y<m_iBBox[1].y;ic.y++) for(ic.x=m_iBBox[0].x;ic.x<m_iBBox[1].x;ic.x++)
		m_nTris += m_pgrid->pCellTris[ic*m_pgrid->stride+1]-m_pgrid->pCellTris[ic*m_pgrid->stride];

	return 1;
}

void CVoxelBV::CleanupAfterIntersectionTest(geometry_under_test *pGTest)
{
	if (!pGTest->pUsedNodesMap)
		return;
	if ((unsigned int)(pGTest->pUsedNodesMap-g_UsedNodesMap) > (unsigned int)CRY_ARRAY_COUNT(g_UsedNodesMap)) {
		delete[] pGTest->pUsedNodesMap; return;
	}
	if (pGTest->nUsedNodes < pGTest->nMaxUsedNodes-1) {
		for(int i=0;i<=pGTest->nUsedNodes;i++) 
			pGTest->pUsedNodesMap[pGTest->pUsedNodesIdx[i]>>5] &= ~(1<<(pGTest->pUsedNodesIdx[i]&31));
	} else
		memset(pGTest->pUsedNodesMap, 0, ((m_nTris-1>>5)+1)*4);
}
