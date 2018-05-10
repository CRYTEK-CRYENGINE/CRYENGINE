// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"

#include "utils.h"
#include <CryPhysics/primitives.h>
#include "bvtree.h"
#include "geometry.h"
#include "raybv.h"

int CRayBV::GetNodeContents(int iNode, BV *pBVCollider,int bColliderUsed,int bColliderLocal, 
														geometry_under_test *pGTest,geometry_under_test *pGTestOp)
{
	return m_pGeom->GetPrimitiveList(0,1, pBVCollider->type,*pBVCollider,bColliderLocal, pGTest,pGTestOp, pGTest->primbuf,pGTest->idbuf);
	//return 1;	// ray should already be in the buffer
}

void CRayBV::GetNodeBV(BV *&pBV, int iNode, int iCaller)
{
	pBV = &g_BVray;
	g_BVray.iNode = 0;
	g_BVray.type = ray::type;
	g_BVray.aray.origin = m_pray->origin;
	g_BVray.aray.dir = m_pray->dir;
}

void CRayBV::GetNodeBV(const Matrix33 &Rw,const Vec3 &offsw,float scalew, BV *&pBV, int iNode, int iCaller)
{
	pBV = &g_BVray;
	g_BVray.iNode = 0;
	g_BVray.type = ray::type;
	g_BVray.aray.origin = Rw*m_pray->origin*scalew + offsw;
	g_BVray.aray.dir = Rw*m_pray->dir*scalew;
}