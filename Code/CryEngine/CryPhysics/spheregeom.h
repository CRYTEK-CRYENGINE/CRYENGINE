// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#ifndef spheregeom_h
#define spheregeom_h
#pragma once

class CSphereGeom : public CPrimitive {
public:
	CSphereGeom() { m_iCollPriority = 3; m_minVtxDist = 0; }

	CSphereGeom* CreateSphere(sphere *pcyl);
	
	virtual int GetType() { return GEOM_SPHERE; }
	virtual void GetBBox(box *pbox) { m_Tree.GetBBox(pbox); }
	virtual int CalcPhysicalProperties(phys_geometry *pgeom);
	virtual int FindClosestPoint(geom_world_data *pgwd, int &iPrim,int &iFeature, const Vec3 &ptdst0,const Vec3 &ptdst1,
		Vec3 *ptres, int nMaxIters);
	virtual int PointInsideStatus(const Vec3 &pt);
	virtual float CalculateBuoyancy(const plane *pplane, const geom_world_data *pgwd, Vec3 &massCenter);
	virtual void CalculateMediumResistance(const plane *pplane, const geom_world_data *pgwd, Vec3 &dPres,Vec3 &dLres);
	virtual void CalcVolumetricPressure(geom_world_data *gwd, const Vec3 &epicenter,float k,float rmin, 
		const Vec3 &centerOfMass, Vec3 &P,Vec3 &L);
	virtual int DrawToOcclusionCubemap(const geom_world_data *pgwd, int iStartPrim,int nPrims, int iPass, SOcclusionCubeMap* cubeMap);
	virtual CBVTree *GetBVTree() { return &m_Tree; }
	virtual int UnprojectSphere(Vec3 center,float r,float rsep, contact *pcontact);
	virtual int GetPrimitive(int iPrim, primitive *pprim) { *(sphere*)pprim = m_sphere; return sizeof(sphere); }
	virtual void DrawWireframe(IPhysRenderer *pRenderer, geom_world_data *gwd, int iLevel, int idxColor);

	virtual const primitive *GetData() { return &m_sphere; }
	virtual void SetData(const primitive* psph) { CreateSphere((sphere*)psph); }
	virtual float GetVolume() { return (4.0f/3)*g_PI*cube(m_sphere.r); }
	virtual Vec3 GetCenter() { return m_sphere.center; }
	virtual float GetExtent(EGeomForm eForm) const;
	virtual void GetRandomPoints(Array<PosNorm> points, CRndGen& seed, EGeomForm eForm) const;
	virtual int PrepareForIntersectionTest(geometry_under_test *pGTest, CGeometry *pCollider,geometry_under_test *pGTestColl, bool bKeepPrevContacts=false);
	virtual int PreparePrimitive(geom_world_data *pgwd,primitive *&pprim,int iCaller=0);
	virtual int GetPrimitiveList(int iStart,int nPrims, int typeCollider,primitive *pCollider,int bColliderLocal, 
		geometry_under_test *pGTest,geometry_under_test *pGTestOp, primitive *pRes,char *pResId);
	virtual int GetUnprojectionCandidates(int iop,const contact *pcontact, primitive *&pprim,int *&piFeature, geometry_under_test *pGTest);
	virtual void GetMemoryStatistics(ICrySizer *pSizer);
	virtual void Save(CMemStream &stm);
	virtual void Load(CMemStream &stm);
	virtual int GetSizeFast() { return sizeof(*this); }

	sphere m_sphere;
	CSingleBoxTree m_Tree;
};

#endif
