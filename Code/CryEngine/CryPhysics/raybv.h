// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#ifndef raybv_h
#define raybv_h
#pragma once

class CRayBV : public CBVTree {
public:
	CRayBV() { m_pGeom=0; m_pray=0; }
	virtual int GetType() { return BVT_RAY; }
	virtual float Build(CGeometry *pGeom) { m_pGeom=pGeom; return 0.0f; }
	void SetRay(ray *pray) { m_pray = pray; }
	virtual void GetNodeBV(BV *&pBV, int iNode=0, int iCaller=0);
	virtual void GetNodeBV(BV *&pBV, const Vec3 &sweepdir,float sweepstep, int iNode=0, int iCaller=0) {}
	virtual void GetNodeBV(const Matrix33 &Rw,const Vec3 &offsw,float scalew, BV *&pBV, int iNode=0, int iCaller=0);
	virtual void GetNodeBV(const Matrix33 &Rw,const Vec3 &offsw,float scalew, BV *&pBV, const Vec3 &sweepdir,float sweepstep, int iNode=0, int iCaller=0) {}
	virtual int GetNodeContents(int iNode, BV *pBVCollider,int bColliderUsed,int bColliderLocal, 
		geometry_under_test *pGTest,geometry_under_test *pGTestOp);

	CGeometry *m_pGeom;
	ray *m_pray;
};

#endif

