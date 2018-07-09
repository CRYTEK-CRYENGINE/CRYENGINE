// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"

#include "utils.h"
#include <CryPhysics/primitives.h>
#include "bvtree.h"
#include "geometry.h"
#include "raybv.h"
#include "raygeom.h"

void CRayGeom::GetBBox(box *pbox) 
{
	//pbox->Basis.SetRow(2,m_dirn);
	//pbox->Basis.SetRow(1,m_dirn.GetOrthogonal().normalized());
	//pbox->Basis.SetRow(0,pbox->Basis.GetRow(1)^m_dirn);
	pbox->Basis.SetIdentity(); pbox->bOriented = 0;
	pbox->center = m_ray.origin+m_ray.dir*0.5f;
	pbox->size = m_ray.dir.abs()*0.5f;
}

static char g_id;
int CRayGeom::PrepareForIntersectionTest(geometry_under_test *pGTest, CGeometry *pCollider,geometry_under_test *pGTestColl, bool bKeepPrevContacts)
{
	g_id = (char)-1;
	m_Tree.PrepareForIntersectionTest(pGTest, pCollider,pGTestColl);
	pGTest->pGeometry = this;
	pGTest->pBVtree = &m_Tree;
	pGTest->primbuf = pGTest->primbuf1 = g_RayBuf;
	pGTest->typeprim = ray::type;
	pGTest->szprim = sizeof(ray);
	pGTest->idbuf = &g_id;
	pGTest->surfaces = 0;
	pGTest->edges = 0;
	pGTest->minAreaEdge = 0;
	PrepareRay(g_RayBuf, pGTest);
	return 1;
}

int CRayGeom::PreparePrimitive(geom_world_data *pgwd, primitive *&pprim,int iCaller)
{
	G(RayBuf)[0].origin = pgwd->R*m_ray.origin*pgwd->scale+pgwd->offset;
	G(RayBuf)[0].dir = pgwd->R*m_ray.dir*pgwd->scale;
	pprim = G(RayBuf);
	return ray::type;
}

void CRayGeom::PrepareRay(ray *pray, geometry_under_test *pGTest)
{
	pray->origin = pGTest->R*m_ray.origin*pGTest->scale+pGTest->offset;
	pray->dir = pGTest->R*m_ray.dir*pGTest->scale;
}

int CRayGeom::GetUnprojectionCandidates(int iop,const contact *pcontact, primitive *&pprim,int *&piFeature, geometry_under_test *pGTest)
{
	if (pGTest->bTransformUpdated)
		PrepareRay((ray*)pGTest->primbuf1, pGTest);
	pGTest->idbuf[0] = (char)-1;
	pGTest->nEdges=pGTest->nSurfaces = 0;
	return 1;
}


int CRayGeom::GetPrimitiveList(int iStart,int nPrims, int typeCollider,primitive *pCollider,int bColliderLocal, 
													 geometry_under_test *pGTest,geometry_under_test *pGTestOp, primitive *pRes,char *pResId)
{
	PrepareRay((ray*)pRes, pGTest);
	pGTest->bTransformUpdated = 0; *pResId = (char)-1;
	return 1;
}

int CRayGeom::RegisterIntersection(primitive *pprim1,primitive *pprim2, geometry_under_test *pGTest1,geometry_under_test *pGTest2, prim_inters *pinters)
{
	geom_contact *pres = pGTest1->contacts + *pGTest1->pnContacts;
	(*pGTest1->pnContacts)++;
	pres->ptborder = &pres->pt;
	pres->nborderpt = 1;
	pres->parea = 0;

	pres->t = ((pinters->pt[0]-pGTest1->offset)*pGTest1->R-m_ray.origin)*m_dirn;
	pres->pt=pres->center = pinters->pt[0];
	pres->n = -pinters->n;
	pres->dir.zero();
	pres->vel = 0;
	pres->iUnprojMode = 0;
	pres->id[0] = -1;
	pres->id[1] = pinters->id[1];
	pres->iPrim[0] = 0; pres->iFeature[0] = 0x20;
	pres->iPrim[1] = pGTest2 && pGTest2->typeprim==indexed_triangle::type ? ((indexed_triangle*)pprim2)->idx : 0;
	pres->iFeature[1] = pinters->iFeature[0][1];
	pres->iNode[0] = pinters->iNode[0];
	pres->iNode[1] = pinters->iNode[1];
	if (*pGTest1->pnContacts>=pGTest1->nMaxContacts || pGTest1->pParams->bStopAtFirstTri)
		pGTest1->bStopIntersection = 1;
	return 1;
}
