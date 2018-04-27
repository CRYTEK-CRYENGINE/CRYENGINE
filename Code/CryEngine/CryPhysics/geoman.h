// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#ifndef geoman_h
#define geoman_h
#pragma once

const int GEOM_CHUNK_SZ = 64;
const int PHYS_GEOM_VER = 1;

class CTriMesh;
struct SCrackGeom {
	int id;
	CTriMesh *pGeom;
	int idmat;
	Vec3 pt0;
	Matrix33 Rc;
	float maxedge,rmaxedge;
	float ry3;
	Vec2 pt3;
	Vec3 vtx[3];
};

struct phys_geometry_serialize {
	int dummy0;
	Vec3 Ibody;
	quaternionf q;
	Vec3 origin;
	float V;
	int nRefCount;
	int surface_idx;
	int dummy1;
	int nMats;

	AUTO_STRUCT_INFO;
};

class CGeomManager : public IGeomManager {
public:
	CGeomManager() { InitGeoman(); }
	~CGeomManager() { ShutDownGeoman(); }

	void InitGeoman();
	void ShutDownGeoman();

	virtual IGeometry *CreateMesh(strided_pointer<const Vec3> pVertices,strided_pointer<unsigned short> pIndices,char *pMats,int *pForeignIdx,int nTris, 
		int flags, float approx_tolerance=0.05f, int nMinTrisPerNode=2,int nMaxTrisPerNode=4, float favorAABB=1.0f) 
	{
		SBVTreeParams params;
		params.nMinTrisPerNode = nMinTrisPerNode; params.nMaxTrisPerNode = nMaxTrisPerNode;
		params.favorAABB = favorAABB;
		return CreateMesh(pVertices,pIndices,pMats,pForeignIdx, nTris, flags & ~mesh_VoxelGrid, approx_tolerance, &params);
	}
	virtual IGeometry *CreateMesh(strided_pointer<const Vec3> pVertices,strided_pointer<unsigned short> pIndices,char *pMats,int *pForeignIdx,int nTris, 
		int flags, float approx_tolerance, SMeshBVParams *pParams);
	virtual IGeometry *CreatePrimitive(int type, const primitives::primitive *pprim);
	virtual void DestroyGeometry(IGeometry *pGeom);

	virtual phys_geometry *RegisterGeometry(IGeometry *pGeom,int defSurfaceIdx=0, int *pMatMapping=0,int nMats=0);
	virtual int AddRefGeometry(phys_geometry *pgeom);
	virtual int UnregisterGeometry(phys_geometry *pgeom);
	virtual void SetGeomMatMapping(phys_geometry *pgeom, int *pMatMapping, int nMats);

	virtual void SaveGeometry(CMemStream &stm, IGeometry *pGeom);
	virtual IGeometry *LoadGeometry(CMemStream &stm, strided_pointer<const Vec3> pVertices, strided_pointer<unsigned short> pIndices, char *pIds);
	virtual void SavePhysGeometry(CMemStream &stm, phys_geometry *pgeom);
	virtual phys_geometry *LoadPhysGeometry(CMemStream &stm, strided_pointer<const Vec3> pVertices, strided_pointer<unsigned short> pIndices, char *pIds);
	virtual IGeometry *CloneGeometry(IGeometry *pGeom);

	virtual ITetrLattice *CreateTetrLattice(const Vec3 *pt,int npt, const int *pTets,int nTets);
	virtual int RegisterCrack(IGeometry *pGeom, Vec3 *pVtx, int idmat);
	virtual void UnregisterCrack(int id);
	virtual IGeometry *GetCrackGeom(const Vec3 *pt,int idmat, geom_world_data *pgwd);
	virtual void UnregisterAllCracks(void (*OnRemoveGeom)(IGeometry *pGeom)=0) {
		for(int i=0;i<m_nCracks;i++) {
			if (OnRemoveGeom) OnRemoveGeom((IGeometry*)m_pCracks[i].pGeom);
			((IGeometry*)m_pCracks[i].pGeom)->Release();
		} m_nCracks=m_idCrack=0;
	}

	virtual IBreakableGrid2d *GenerateBreakableGrid(Vec2 *ptsrc,int npt, const Vec2i &nCells, int bStaticBorder, int seed=-1);
	virtual void ReleaseGeomsImmediately(bool bReleaseImmediately) { m_bReleaseGeomsImmediately = bReleaseImmediately; }

	phys_geometry *GetFreeGeomSlot();
	virtual IPhysicalWorld *GetIWorld() { return 0; }
	void FlushOldGeoms();

	phys_geometry **m_pGeoms;
	int m_nGeomChunks,m_nGeomsInLastChunk;
	phys_geometry *m_pFreeGeom;
	int m_lockGeoman;

	SCrackGeom *m_pCracks;
	int m_nCracks;
	int m_idCrack;
	float m_kCrackScale,m_kCrackSkew;
	int m_sizeExtGeoms;

	bool m_bReleaseGeomsImmediately;
	class CGeometry *m_oldGeoms;
	volatile int m_lockOldGeoms;
};

#endif
