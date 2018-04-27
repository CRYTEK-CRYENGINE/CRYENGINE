// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#ifndef voxelbv_h
#define voxelbv_h
#pragma once

class CVoxelBV : public CBVTree {
public:
	CVoxelBV()
		: m_pMesh(nullptr)
		, m_pgrid(nullptr)
		, m_nTris(0)
	{
		static_assert(CRY_ARRAY_COUNT(m_iBBox) == 2, "Invalid array size!");
		m_iBBox[0].zero();
		m_iBBox[1].zero();
	}

	virtual ~CVoxelBV() {}
	virtual int GetType() { return BVT_VOXEL; }
	float Build(CGeometry *pGeom) { m_pMesh = (CTriMesh*)pGeom; return 0; }

	virtual void GetBBox(box *pbox);
	virtual int MaxPrimsInNode() { return m_nTris; }

	virtual int PrepareForIntersectionTest(geometry_under_test *pGTest, CGeometry *pCollider,geometry_under_test *pGTestColl);
	virtual void CleanupAfterIntersectionTest(geometry_under_test *pGTest);
	virtual void GetNodeBV(BV *&pBV, int iNode=0, int iCaller=0);
	virtual void GetNodeBV(BV *&pBV, const Vec3 &sweepdir,float sweepstep, int iNode=0, int iCaller=0) { GetNodeBV(pBV); }
	virtual void GetNodeBV(const Matrix33 &Rw,const Vec3 &offsw,float scalew, BV *&pBV, int iNode=0, int iCaller=0);
	virtual void GetNodeBV(const Matrix33 &Rw,const Vec3 &offsw,float scalew, BV *&pBV, const Vec3 &sweepdir,float sweepstep, int iNode=0, int iCaller=0) 
	{ GetNodeBV(Rw,offsw,scalew,pBV,0,iCaller); }
	virtual int GetNodeContents(int iNode, BV *pBVCollider,int bColliderUsed,int bColliderLocal, 
		geometry_under_test *pGTest,geometry_under_test *pGTestOp);
	virtual void MarkUsedTriangle(int itri, geometry_under_test *pGTest);
	virtual void ResetCollisionArea() { m_iBBox[0].zero(); m_iBBox[1]=m_pgrid->size; } 

	CTriMesh *m_pMesh;
	voxelgrid *m_pgrid;
	Vec3i m_iBBox[2];
	int m_nTris;
};

#endif
