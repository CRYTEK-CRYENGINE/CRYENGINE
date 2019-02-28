// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#ifndef boxgeom_h
#define boxgeom_h
#pragma once

class CTriMesh;

class CBoxGeom : public CPrimitive {
public:
	ILINE CBoxGeom() { m_iCollPriority = 3; m_minVtxDist = 0; m_Tree.m_pGeom = this; }

	CBoxGeom* CreateBox(box *pcyl);

	void PrepareBox(box *pbox, geometry_under_test *pGTest);
	virtual int PreparePrimitive(geom_world_data *pgwd,primitive *&pprim,int iCaller=0);
	virtual int GetType() { return GEOM_BOX; }
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
	virtual void DrawWireframe(IPhysRenderer *pRenderer, geom_world_data *gwd, int iLevel, int idxColor);
	virtual int GetPrimitive(int iPrim, primitive *pprim) { *(box*)pprim = GetBox(); return sizeof(box); }
	virtual int GetFeature(int iPrim,int iFeature, Vec3 *pt);
	virtual int UnprojectSphere(Vec3 center,float r,float rsep, contact *pcontact);
	virtual IGeometry *GetTriMesh(int bClone=1);

	virtual const primitive *GetData() { return &GetBox(); }
	virtual void SetData(const primitive* pbox) { CreateBox((box*)pbox); }
	virtual float GetVolume() { return GetBox().size.GetVolume()*8; } 
	virtual Vec3 GetCenter() { return GetBox().center; }
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

	void BuildTriMesh(CTriMesh &mesh, int bStaticArrays=1);

	ILINE box& GetBox() { return m_Tree.m_Box; }

	CSingleBoxTree m_Tree;
};

#endif
