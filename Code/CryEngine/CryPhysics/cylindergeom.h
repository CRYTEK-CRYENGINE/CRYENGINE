// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#ifndef cylindergeom_h
#define cylindergeom_h
#pragma once

class CCylinderGeom : public CPrimitive {
public:
	CCylinderGeom() { m_iCollPriority = 3; m_nTessellation = 18; m_minVtxDist = 0; }

	CCylinderGeom* CreateCylinder(cylinder *pcyl);

	void PrepareCylinder(cylinder *pcyl, geometry_under_test *pGTest);
	virtual int PreparePrimitive(geom_world_data *pgwd,primitive *&pprim,int iCaller=0);
	virtual int GetType() { return GEOM_CYLINDER; }
	virtual void GetBBox(box *pbox) { m_Tree.GetBBox(pbox); }
	virtual int CalcPhysicalProperties(phys_geometry *pgeom);
	virtual int FindClosestPoint(geom_world_data *pgwd, int &iPrim,int &iFeature, const Vec3 &ptdst0,const Vec3 &ptdst1,
		Vec3 *ptres, int nMaxIters);
	virtual int PointInsideStatus(const Vec3 &pt);
	virtual void CalcVolumetricPressure(geom_world_data *gwd, const Vec3 &epicenter,float k,float rmin, 
		const Vec3 &centerOfMass, Vec3 &P,Vec3 &L);
	virtual float CalculateBuoyancy(const plane *pplane, const geom_world_data *pgwd, Vec3 &massCenter);
	virtual void CalculateMediumResistance(const plane *pplane, const geom_world_data *pgwd, Vec3 &dPres,Vec3 &dLres);
	virtual int DrawToOcclusionCubemap(const geom_world_data *pgwd, int iStartPrim,int nPrims, int iPass, SOcclusionCubeMap* cubeMap);
	virtual CBVTree *GetBVTree() { return &m_Tree; }
	virtual int UnprojectSphere(Vec3 center,float r,float rsep, contact *pcontact);
	virtual int GetPrimitive(int iPrim, primitive *pprim) { *(cylinder*)pprim = m_cyl; return sizeof(cylinder); }
	virtual void DrawWireframe(IPhysRenderer *pRenderer, geom_world_data *gwd, int iLevel, int idxColor);

	virtual const primitive *GetData() { return &m_cyl; }
	virtual void SetData(const primitive* pcyl) { CreateCylinder((cylinder*)pcyl); }
	virtual float GetVolume() { return sqr(m_cyl.r)*m_cyl.hh*(g_PI*2); }
	virtual Vec3 GetCenter() { return m_cyl.center; }
	virtual float GetExtent(EGeomForm eForm) const;
	virtual void GetRandomPoints(Array<PosNorm> points, CRndGen& seed, EGeomForm eForm) const;
	virtual int PrepareForIntersectionTest(geometry_under_test *pGTest, CGeometry *pCollider,geometry_under_test *pGTestColl, bool bKeepPrevContacts=false);
	virtual int GetPrimitiveList(int iStart,int nPrims, int typeCollider,primitive *pCollider,int bColliderLocal, 
		geometry_under_test *pGTest,geometry_under_test *pGTestOp, primitive *pRes,char *pResId);
	virtual int GetUnprojectionCandidates(int iop,const contact *pcontact, primitive *&pprim,int *&piFeature, geometry_under_test *pGTest);
	virtual int PreparePolygon(coord_plane *psurface, int iPrim,int iFeature, geometry_under_test *pGTest, Vec2 *&ptbuf,
		int *&pVtxIdBuf,int *&pEdgeIdBuf);
	virtual int PreparePolyline(coord_plane *psurface, int iPrim,int iFeature, geometry_under_test *pGTest, Vec2 *&ptbuf,
		int *&pVtxIdBuf,int *&pEdgeIdBuf);

	virtual void GetMemoryStatistics(ICrySizer *pSizer);
	virtual void Save(CMemStream &stm);
	virtual void Load(CMemStream &stm);
	virtual int GetSizeFast() { return sizeof(*this); }

	cylinder m_cyl;
	int m_nTessellation;
	CSingleBoxTree m_Tree;
};

#endif
