// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"

#include "utils.h"
#include <CryPhysics/primitives.h>
#include "bvtree.h"
#include "geometry.h"
#include "trimesh.h"
#include "heightfieldbv.h"

InitHeightfieldGlobals::InitHeightfieldGlobals() {
  for(int iCaller=0; iCaller<=MAX_PHYS_THREADS; iCaller++) {
    g_BVhf.iNode = 0; g_BVhf.type = heightfield::type;
  }
}

static InitHeightfieldGlobals initHeightfieldGlobals;

float CHeightfieldBV::Build(CGeometry *pGeom)
{
	m_pMesh = (CTriMesh*)pGeom;
	return (m_phf->size.x*m_phf->step.x*m_phf->size.y*m_phf->step.y*(m_maxHeight-m_minHeight));
}

void CHeightfieldBV::SetHeightfield(heightfield *phf)
{
	m_phf = phf;
	m_PatchStart.set(-1,-1);
	m_PatchSize = m_phf->size;
	m_minHeight = -1;
	m_maxHeight = 1;
}

void CHeightfieldBV::GetBBox(box *pbox)
{
	pbox->size.x = m_PatchSize.x*m_phf->step.x*0.5f;
	pbox->size.y = m_PatchSize.y*m_phf->step.y*0.5f;
	pbox->size.z = (m_maxHeight-m_minHeight)*0.5f;
	pbox->center.x = m_PatchStart.x*m_phf->step.x + pbox->size.x;
	pbox->center.y = m_PatchStart.y*m_phf->step.y + pbox->size.y;
	pbox->center.z = m_minHeight + pbox->size.z;
	pbox->Basis.SetIdentity();
	pbox->bOriented = 0;
}

void CHeightfieldBV::GetNodeBV(BV *&pBV,int iNode,int iCaller)
{
	pBV = &g_BVhf;
	g_BVhf.hf = *m_phf;
	g_BVhf.hf.Basis.SetIdentity();
	g_BVhf.hf.bOriented = 0;
	g_BVhf.hf.origin.Set(-m_PatchStart.x*m_phf->step.x,-m_PatchStart.y*m_phf->step.y,0);
}

void CHeightfieldBV::GetNodeBV(const Matrix33 &Rw,const Vec3 &offsw,float scalew, BV *&pBV, int iNode,int iCaller)
{
	pBV = &g_BVhf;
	g_BVhf.hf = *m_phf;
	g_BVhf.hf.Basis = Rw.T();
	g_BVhf.hf.bOriented = 1;
	g_BVhf.hf.origin = offsw;
	g_BVhf.hf.step = m_phf->step*scalew;
	g_BVhf.hf.stepr = m_phf->stepr*(scalew==1.0f ? 1:1/scalew);
	g_BVhf.hf.origin -= Rw*Vec3(m_PatchStart.x*g_BVhf.hf.step.x,m_PatchStart.y*g_BVhf.hf.step.y,0);
	g_BVhf.hf.heightscale *= scalew;
}


void project_box_on_grid(box *pbox,grid *pgrid, geometry_under_test *pGTest, int &ix,int &iy,int &sx,int &sy,float &minz)
{
	Vec3 center; Vec3 dim;
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
	ix = float2int((center.x-dim.x)*pgrid->stepr.x-0.5f); ix &= ~(ix>>31);
	iy = float2int((center.y-dim.y)*pgrid->stepr.y-0.5f); iy &= ~(iy>>31);
	sx = min(float2int((center.x+dim.x)*pgrid->stepr.x+0.5f),pgrid->size.x)-ix;
	sy = min(float2int((center.y+dim.y)*pgrid->stepr.y+0.5f),pgrid->size.y)-iy;
	minz = center.z-dim.z;
}

void CHeightfieldBV::MarkUsedTriangle(int itri, geometry_under_test *pGTest)
{
	m_pUsedTriMap[itri>>5] |= 1u<<(itri&31);
}

int CHeightfieldBV::GetNodeContents(int iNode, BV *pBVCollider,int bColliderUsed,int bColliderLocal, 
																		geometry_under_test *pGTest,geometry_under_test *pGTestOp)
{
	int iStart,nCols,nRows,nPrims,ix,iy,i,j;
	float minz;

	if (pBVCollider->type==box::type)	{
		project_box_on_grid((box*)(primitive*)*pBVCollider,m_phf, (geometry_under_test*)((intptr_t)pGTest & -((intptr_t)bColliderLocal^1)), 
			ix,iy,nCols,nRows,minz);
		nCols = min(ix+nCols, m_PatchSize.x);
		nRows = min(iy+nRows, m_PatchSize.y);
		ix &= ~(ix>>31); iy &= ~(iy>>31);
	} else {
		nCols=m_PatchSize.x; nRows=m_PatchSize.y; ix=iy=0;
	}

	//if (m_phf->gettype(ix,iy)==-1)
	//	return 0;

	iStart = ix+iy*m_PatchSize.x;
	if (!bColliderUsed) {
		for(i=nPrims=0; i<nRows; i++,iStart+=m_PatchSize.x) {
      char *primbuf = (char*)pGTest->primbuf;
			nPrims += m_pMesh->GetPrimitiveList(iStart*2,nCols*2, pBVCollider->type,*pBVCollider,bColliderLocal, pGTest,pGTestOp,
                                          (primitive*)(primbuf+nPrims*pGTest->szprim), pGTest->idbuf+nPrims);
    }
	} else {
		for(i=nPrims=0; i<nRows; i++,iStart+=m_PatchSize.x)	for(j=0;j<nCols*2;j++) {
			int itri = iStart*2+j;
			if (!(m_pUsedTriMap[itri>>5]>>(itri&31) & 1)) {
        char *primbuf = (char*)pGTest->primbuf;
				nPrims += m_pMesh->GetPrimitiveList(itri,1, pBVCollider->type,*pBVCollider,bColliderLocal, pGTest,pGTestOp,
                                            (primitive*)(primbuf+nPrims*pGTest->szprim), pGTest->idbuf+nPrims);
      }
		}
	}

	return nPrims;
}
