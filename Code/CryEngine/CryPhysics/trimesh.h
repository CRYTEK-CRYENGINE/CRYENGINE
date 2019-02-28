// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#ifndef trimesh_h
#define trimesh_h
#pragma once

#include <CryMath/GeomQuery.h>

const int	 mesh_force_AABB = 0x10000000;
const int  mesh_shared_topology = 0x20000000;
const int  mesh_should_die = 0x40000000;

struct border_trace {
	Vec3 *pt;
	int (*itri)[2];
	float *seglen;
	int npt,szbuf;

	Vec3 pt_end;
	int itri_end;
	int iedge_end;
	float end_dist2;
	int bExactBorder;

	int iMark,nLoop;

	Vec3r n_sum[2];
	Vec3 n_best;
	int ntris[2];
};

struct tri_flags {
	unsigned int inext : 16;
	unsigned int iprev : 15;
	unsigned int bFree : 1;
};

class CTriMesh : public CGeometry {
public:
	static void CleanupGlobalLoadState();

public:
	CTriMesh();
	virtual ~CTriMesh();

	CTriMesh* CreateTriMesh(strided_pointer<const Vec3> pVertices,strided_pointer<unsigned short> pIndices,char *pMats,int *pForeignIdx,int nTris, 
		int flags, int nMinTrisPerNode=2,int nMaxTrisPerNode=4, float favorAABB=1.0f);
	CTriMesh *Clone(CTriMesh *src,int flags);
	virtual int GetType() { return GEOM_TRIMESH; }
	virtual int Intersect(IGeometry *pCollider, geom_world_data *pdata1,geom_world_data *pdata2, intersection_params *pparams, geom_contact *&pcontacts);
	virtual int Subtract(IGeometry *pGeom, geom_world_data *pdata1,geom_world_data *pdata2, int bLogUpdates=1);
	virtual int Slice(const triangle *pcut, float minlen=0, float minArea=0);
	virtual void GetBBox(box *pbox) { ReadLockCond lock(m_lockUpdate,isneg(m_lockUpdate)^1); m_pTree->GetBBox(pbox); }
	virtual void GetBBox(box *pbox, int bThreadSafe) { ReadLockCond lock(m_lockUpdate,(bThreadSafe|isneg(m_lockUpdate))^1); m_pTree->GetBBox(pbox); }
	virtual int FindClosestPoint(geom_world_data *pgwd, int &iPrim,int &iFeature, const Vec3 &ptdst0,const Vec3 &ptdst1,
		Vec3 *ptres, int nMaxIters=10);
	virtual int CalcPhysicalProperties(phys_geometry *pgeom);
	virtual int PointInsideStatus(const Vec3 &pt) { return PointInsideStatusMesh(pt,0); }
	virtual int PointInsideStatusMesh(const Vec3 &pt,indexed_triangle *pHitTri);
	virtual void DrawWireframe(IPhysRenderer *pRenderer,geom_world_data *gwd, int iLevel, int idxColor);
	virtual void CalcVolumetricPressure(geom_world_data *gwd, const Vec3 &epicenter,float k,float rmin, 
		const Vec3 &centerOfMass, Vec3 &P,Vec3 &L);
	virtual float CalculateBuoyancy(const plane *pplane, const geom_world_data *pgwd, Vec3 &massCenter);
	virtual void CalculateMediumResistance(const plane *pplane, const geom_world_data *pgwd, Vec3 &dPres,Vec3 &dLres);
	int FloodFill(const Vec3& org, float &V, const Vec3& gravity, float e, Vec2* &ptborder,QuatT &qtBorder,float &depth,int bConvexBorder, 
		IGeometry **pFloaters,QuatTS *qtsFloaters,int nFloaters, float &Vcombined,Vec3 &comCombined);
	void Flip();
	float ComputeVesselVolume(const plane *pplane, IGeometry **pFloaters,QuatTS *qtsFloaters,int nFloaters, int ihash, index_t *pIndices,Vec3 *pNormals, float dim, float &Vsum,Vec3 &comSum);
	virtual int GetPrimitiveId(int iPrim,int iFeature) { return m_pIds ? m_pIds[iPrim]:-1; }
	virtual int GetForeignIdx(int iPrim) { return m_pForeignIdx ? m_pForeignIdx[iPrim] : -1; }
	virtual Vec3 GetNormal(int iPrim, const Vec3 &pt) { return m_pNormals[iPrim]; }
	virtual int IsConvex(float tolerance);
	virtual void PrepareForRayTest(float raylen);
	virtual int DrawToOcclusionCubemap(const geom_world_data *pgwd, int iStartPrim,int nPrims, int iPass, SOcclusionCubeMap* cubeMap);
	virtual CBVTree *GetBVTree() { return m_pTree; }
	virtual int GetPrimitiveCount() { return m_nTris; }
	virtual int GetPrimitive(int iPrim, primitive *pprim) { 
		if ((unsigned int)iPrim>=(unsigned int)m_nTris) 
			return 0;
		((triangle*)pprim)->pt[0] = m_pVertices[m_pIndices[iPrim*3+0]];
		((triangle*)pprim)->pt[1] = m_pVertices[m_pIndices[iPrim*3+1]];
		((triangle*)pprim)->pt[2] = m_pVertices[m_pIndices[iPrim*3+2]];
		((triangle*)pprim)->n = m_pNormals[iPrim];
		return sizeof(triangle); 
	}
	virtual int GetFeature(int iPrim,int iFeature, Vec3 *pt);
	virtual int PreparePrimitive(geom_world_data *pgwd,primitive *&pprim,int iCaller);
	virtual int GetSubtractionsCount() { return m_nSubtracts; }
	virtual IGeometry *GetTriMesh(int bClone=1) {
		if (!bClone) return this;
		CTriMesh *pClone = new CTriMesh; pClone->Clone(this,0);
		return pClone;
	}
	virtual int UnprojectSphere(Vec3 center,float r,float rsep, contact *pcontact);

	virtual float GetExtent(EGeomForm eForm) const;
	virtual void GetRandomPoints(Array<PosNorm> points, CRndGen& seed, EGeomForm eForm) const;

	virtual void GetMemoryStatistics(ICrySizer *pSizer);
	virtual void Save(CMemStream &stm);
	virtual void Load(CMemStream &stm) { Load(stm,0,0,0); }
	virtual void Load(CMemStream &stm, strided_pointer<const Vec3> pVertices, strided_pointer<unsigned short> pIndices, char *pIds);
	virtual void CompactMemory();
	virtual int GetErrorCount() { return m_nErrors; }
	virtual int PrepareForIntersectionTest(geometry_under_test *pGTest, CGeometry *pCollider,geometry_under_test *pGTestColl, bool bKeepPrevContacts=false);
	virtual int RegisterIntersection(primitive *pprim1,primitive *pprim2, geometry_under_test *pGTest1,geometry_under_test *pGTest2, 
		prim_inters *pinters);
	virtual void CleanupAfterIntersectionTest(geometry_under_test *pGTest);
	virtual int GetPrimitiveList(int iStart,int nPrims, int typeCollider,primitive *pCollider,int bColliderLocal, 
		geometry_under_test *pGTest,geometry_under_test *pGTestOp, primitive *pRes,char *pResId);
	virtual int GetUnprojectionCandidates(int iop,const contact *pcontact, primitive *&pprim,int *&piFeature, geometry_under_test *pGTest);
	virtual int PreparePolygon(coord_plane *psurface, int iPrim,int iFeature, geometry_under_test *pGTest, Vec2 *&ptbuf,
		int *&pVtxIdBuf,int *&pEdgeIdBuf);
	virtual int PreparePolyline(coord_plane *psurface, int iPrim,int iFeature, geometry_under_test *pGTest, Vec2 *&ptbuf,
		int *&pVtxIdBuf,int *&pEdgeIdBuf);

	int GetEdgeByBuddy(int itri,int itri_buddy) {
		int iedge=0,imask;
		imask = m_pTopology[itri].ibuddy[1]-itri_buddy; imask = imask-1>>31 ^ imask>>31; iedge = 1&imask;
		imask = m_pTopology[itri].ibuddy[2]-itri_buddy; imask = imask-1>>31 ^ imask>>31; iedge = iedge&~imask | 2&imask;
		return iedge;
	}
	int GetNeighbouringEdgeId(int vtxid, int ivtx);
	void PrepareTriangle(int itri,triangle *ptri, const geometry_under_test *pGTest);
	int TraceTriangleInters(int iop, primitive *pprims[], int idx_buddy,int type_buddy, prim_inters *pinters, 
													geometry_under_test *pGTest, border_trace *pborder);
	void HashTrianglesToPlane(const coord_plane &hashplane, const Vec2 &hashsize, grid &hashgrid,index_t *&pHashGrid,index_t *&pHashData,float rcellsize=0, 
		float pad=1E-5f, const index_t* pIndices=0,int nTris=0,int maxhash=0);
	int CalculateTopology(index_t *pIndices, int bCheckOnly=0);
	int BuildIslandMap();
	void RebuildBVTree(CBVTree *pRefTree=0);
	void Empty();
	float GetIslandDisk(int matid, const Vec3 &ptref, Vec3 &center,Vec3 &normal,float &peakDist);

	CTriMesh *SplitIntoIslands(plane *pGround,int nPlanes,int bOriginallyMobile);
	int FilterMesh(float minlen,float minangle,int bLogUpdates=1);

	void CompactTriangleList(int *pTriMap, bool bNoRealloc=false);
	void CollapseTriangleToLine(int itri,int ivtx, int *pTriMap, bop_meshupdate *pmu);
	void RecalcTriNormal(int i) {
		m_pNormals[i] = (m_pVertices[m_pIndices[i*3+1]]-m_pVertices[m_pIndices[i*3]] ^ 
										 m_pVertices[m_pIndices[i*3+2]]-m_pVertices[m_pIndices[i*3]]).normalized();
	}

	virtual const primitive *GetData() { return (mesh_data*)&m_pIndices; }
	virtual void SetData(const primitives::primitive*) { for(int i=0;i<m_nTris;i++) RecalcTriNormal(i); RebuildBVTree(); MARK_UNUSED m_V,m_center; }
	virtual float GetVolume();
	virtual Vec3 GetCenter();
	virtual void *GetForeignData(int iForeignData=0) { 
		return iForeignData==DATA_MESHUPDATE ? m_pMeshUpdate : CGeometry::GetForeignData(iForeignData); 
	}
	virtual void SetForeignData(void *pForeignData, int iForeignData) { 
		if (iForeignData==DATA_MESHUPDATE) {
			m_pMeshUpdate = (bop_meshupdate*)pForeignData;
			if (!pForeignData)
				m_iLastNewTriIdx = BOP_NEWIDX0;
		} else
			CGeometry::SetForeignData(pForeignData,iForeignData);
	}
	virtual void RemapForeignIdx(int *pCurForeignIdx, int *pNewForeignIdx, int nTris);
	virtual void AppendVertices(Vec3 *pVtx,int *pVtxMap, int nVtx);
	virtual void DestroyAuxilaryMeshData(int idata) {
		if (idata & mesh_data_materials && m_pIds) { if (!(m_flags & mesh_shared_mats)) delete[] m_pIds; m_pIds=0; }
		if (idata & mesh_data_foreign_idx && m_pForeignIdx) { if (!(m_flags & mesh_shared_foreign_idx)) delete[] m_pForeignIdx; m_pForeignIdx=0; }
		if (idata & mesh_data_vtxmap && m_pVtxMap) { delete[] m_pVtxMap; m_pVtxMap=0; }
	}

	virtual int SanityCheck();

	struct voxgrid_surf	{
		voxgrid_surf() : cells(0),vtx(0),norm(0),nbcnt(0),nbidx(0) {}
		void Free() { delete[] cells;cells=0; delete[] vtx;vtx=0; delete[] norm;norm=0; delete[] nbcnt;nbcnt=0; delete[] nbidx;nbidx=0; }
		Vec3i *cells;
		Vec3 *vtx,*norm;
		int *nbcnt,*nbidx;
		int ncells;
	};
	template<bool rngchk> struct voxgrid_tpl {
		Vec3i sz;
		Vec3 center;
		quaternionf q;
		float celldim,rcelldim;
		int *data;
		Vec3i lastHit;
		Vec3i *flipHist;
		int nflips;
		int bitVis;
		voxgrid_surf surf;
		bool persistent;
		voxgrid_tpl() : lastHit(1<<10),persistent(false),nflips(0),flipHist(0) { data=0; celldim=0; }
		voxgrid_tpl(const Vec3i &dim,Vec3 c=Vec3(ZERO)) : sz(dim),center(c),lastHit(1<<10),persistent(false),nflips(0),flipHist(0) { center=c; celldim=0; Alloc(); }
		~voxgrid_tpl() { if (persistent) Free(); }
		void Alloc() { int alloc=sz.GetVolume()*8+1; memset(data=new int[alloc],0,alloc*sizeof(data[0])); }
		void Free() { delete[] data; data=0; surf.Free(); delete[] flipHist; }
		int &operator()(int ix,int iy,int iz) {
			if (rngchk) {
				int mask = (ix+sz.x|sz.x-1-ix|iy+sz.y|sz.y-1-iy|iz+sz.z|sz.z-1-iz)>>31;
				return data[(((iz+sz.z)*sz.y*2+iy+sz.y)*sz.x*2+ix+sz.x&~mask) + (sz.GetVolume()*8&mask)]; 
			} else
				return data[((iz+sz.z)*sz.y*2+iy+sz.y)*sz.x*2+ix+sz.x]; 
		}
		int &operator()(const Vec3i &idx) { return (*this)(idx.x,idx.y,idx.z); }
		Vec3i RayTrace(const Vec3& org, const Vec3& dir);
		void DrawHelpers(IPhysRenderer *pRenderer, const Vec3& pos,const quaternionf& q,float scale, int flags);
	};
	typedef voxgrid_tpl<true> voxgrid;
	#define vox_iterate(vox,ix,iy,iz) for((ix)=-(vox).sz.x;(ix)<(vox).sz.x;(ix)++) for((iy)=-(vox).sz.y;(iy)<(vox).sz.y;(iy)++) for((iz)=-(vox).sz.z;(iz)<(vox).sz.z;(iz)++)
	#define vox_iterate1(vox,ic) for((ic).z=-(vox).sz.z+1;(ic).z<(vox).sz.z-1;(ic).z++) for((ic).y=-(vox).sz.y+1;(ic).y<(vox).sz.y-1;(ic).y++) for((ic).x=-(vox).sz.x+1;(ic).x<(vox).sz.x-1;(ic).x++)
	template<bool rngchk> voxgrid_tpl<rngchk> Voxelize(quaternionf q, float celldim, const index_t *pIndices=0, int nTris=0, int pad=0);
	virtual int Boxify(primitives::box *pboxes,int nMaxBoxes, const SBoxificationParams &params);
	virtual int Proxify(IGeometry **&pOutGeoms, SProxifyParams *pparams=0);
	voxgrid_tpl<false> *m_voxgrid;

	CBVTree *m_pTree;
	index_t *m_pIndices;
	char *m_pIds;
	int *m_pForeignIdx;
	strided_pointer<Vec3> m_pVertices;
	Vec3 *m_pNormals;
	int *m_pVtxMap;
	trinfo *m_pTopology;
	int m_nTris,m_nVertices;
	mesh_island *m_pIslands;
	int m_nIslands;
	tri_flags *m_pTri2Island;
	int m_flags;
	int *m_pIdxNew2Old;
	int m_nMaxVertexValency;
	index_t *m_pHashGrid[3],*m_pHashData[3];
	grid m_hashgrid[3];
	int m_nHashPlanes;
	volatile int m_lockHash;
	int m_bConvex[4];
	float m_ConvexityTolerance[4];
	int m_bMultipart;
	float m_V;
	Vec3 m_center;
	mutable CGeomExtents m_Extents;
	int m_nErrors;
	bop_meshupdate *m_pMeshUpdate;
	int m_iLastNewTriIdx;
	bop_meshupdate_thunk m_refmu;
	int m_nMessyCutCount;
	int m_nSubtracts;
	unsigned int *m_pUsedTriMap;

	int m_ivtx0;
	int ExpandVtxList();
};


struct InitTriMeshGlobals { InitTriMeshGlobals(); };


#endif
