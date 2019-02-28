// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#ifndef capsulegeom_h
#define capsulegeom_h
#pragma once

class CCapsuleGeom : public CCylinderGeom {
public:
	CCapsuleGeom() {}
	CCapsuleGeom* CreateCapsule(capsule *pcaps);
	virtual int GetType() { return GEOM_CAPSULE; }
	virtual void SetData(const primitive* pcaps) { CreateCapsule((capsule*)pcaps); }
	virtual int PreparePrimitive(geom_world_data *pgwd,primitive *&pprim,int iCaller=0) { 
		CCylinderGeom::PreparePrimitive(pgwd,pprim,iCaller); return capsule::type; 
	}
	virtual int CalcPhysicalProperties(phys_geometry *pgeom);
	virtual int FindClosestPoint(geom_world_data *pgwd, int &iPrim,int &iFeature, const Vec3 &ptdst0,const Vec3 &ptdst1,
		Vec3 *ptres, int nMaxIters);
	virtual int PointInsideStatus(const Vec3 &pt);
	virtual float CalculateBuoyancy(const plane *pplane, const geom_world_data *pgwd, Vec3 &massCenter);
	virtual void CalculateMediumResistance(const plane *pplane, const geom_world_data *pgwd, Vec3 &dPres,Vec3 &dLres);
	virtual int DrawToOcclusionCubemap(const geom_world_data *pgwd, int iStartPrim,int nPrims, int iPass, SOcclusionCubeMap* cubeMap);
	virtual int UnprojectSphere(Vec3 center,float r,float rsep, contact *pcontact);
	virtual float GetVolume() { return sqr(m_cyl.r)*m_cyl.hh*(g_PI*2) + (4.0f/3*g_PI)*cube(m_cyl.r); }
	virtual int PrepareForIntersectionTest(geometry_under_test *pGTest, CGeometry *pCollider,geometry_under_test *pGTestColl, bool bKeepPrevContacts=false);
	virtual int GetUnprojectionCandidates(int iop,const contact *pcontact, primitive *&pprim,int *&piFeature, geometry_under_test *pGTest);
};

#endif
