// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#ifndef heightfieldbv_h
#define heightfieldbv_h
#pragma once

class CHeightfieldBV : public CBVTree {
public:
	CHeightfieldBV() { m_pUsedTriMap=0; m_minHeight=0.f; m_maxHeight=0.f; m_pMesh=0; m_phf=0; }
	virtual ~CHeightfieldBV() { if (m_pUsedTriMap) delete[] m_pUsedTriMap; }
	virtual int GetType() { return BVT_HEIGHTFIELD; }

	virtual float Build(CGeometry *pGeom);
	void SetHeightfield(heightfield *phf);
	virtual void GetBBox(box *pbox);
	virtual int MaxPrimsInNode() { return m_PatchSize.x*m_PatchSize.y*2; }
	virtual void GetNodeBV(BV *&pBV, int iNode=0, int iCaller=0);
	virtual void GetNodeBV(BV *&pBV, const Vec3 &sweepdir,float sweepstep, int iNode=0, int iCaller=0) {}
	virtual void GetNodeBV(const Matrix33 &Rw,const Vec3 &offsw,float scalew, BV *&pBV, int iNode=0, int iCaller=0);
	virtual void GetNodeBV(const Matrix33 &Rw,const Vec3 &offsw,float scalew, BV *&pBV, const Vec3 &sweepdir,float sweepstep, int iNode=0, int iCaller=0) {}
	virtual int GetNodeContents(int iNode, BV *pBVCollider,int bColliderUsed,int bColliderLocal, 
		geometry_under_test *pGTest,geometry_under_test *pGTestOp);
	virtual void MarkUsedTriangle(int itri, geometry_under_test *pGTest);

	CTriMesh *m_pMesh;
	heightfield *m_phf;
	Vec2i m_PatchStart;
	Vec2i m_PatchSize;
	float m_minHeight,m_maxHeight;
	unsigned int *m_pUsedTriMap;
};

struct InitHeightfieldGlobals { InitHeightfieldGlobals(); };

void project_box_on_grid(box *pbox,grid *pgrid, geometry_under_test *pGTest, int &ix,int &iy,int &sx,int &sy,float &minz);


#endif
