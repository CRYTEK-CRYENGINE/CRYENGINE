// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#ifndef voxelgeom_h
#define voxelgeom_h
#pragma once

class CVoxelGeom : public CTriMesh {
public:
	CVoxelGeom() { m_grid.pCellTris=0; m_grid.pTriBuf=0; }
	virtual ~CVoxelGeom() { 
		if (m_grid.pCellTris) delete[] m_grid.pCellTris;
		if (m_grid.pTriBuf) delete[] m_grid.pTriBuf;
		m_pTree=0; 
	}

	CVoxelGeom *CreateVoxelGrid(grid3d *pgrid);
	virtual int GetType() { return GEOM_VOXELGRID; }
	virtual int Intersect(IGeometry *pCollider, geom_world_data *pdata1,geom_world_data *pdata2, intersection_params *pparams, geom_contact *&pcontacts);
	virtual int PointInsideStatus(const Vec3 &pt) { return -1; }
	virtual void CalcVolumetricPressure(geom_world_data *gwd, const Vec3 &epicenter,float k,float rmin, 
		const Vec3 &centerOfMass, Vec3 &P,Vec3 &L) {}
	virtual int IsConvex(float tolerance) { return 0; }
	virtual int DrawToOcclusionCubemap(const geom_world_data *pgwd, int iStartPrim,int nPrims, int iPass, SOcclusionCubeMap* cubeMap);
	virtual void PrepareForRayTest(float raylen) {}
	virtual CBVTree *GetBVTree() { return &m_Tree; }
	virtual void GetMemoryStatistics(ICrySizer *pSizer);

	voxelgrid m_grid;
	CVoxelBV m_Tree;
};

#endif
