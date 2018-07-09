// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#ifndef geometry_h
#define geometry_h
#pragma once

#include "overlapchecks.h"

struct tritem {
	int itri;
	int itri_parent;
	int ivtx0;
};
struct vtxitem {
	int ivtx;
	int id;
	int ibuddy[2];
};

struct BBoxExt : BBox {
	box aboxStatic;
};


struct intersData {
	indexed_triangle IdxTriBuf[256]; 
	int IdxTriBufPos;
	cylinder CylBuf[2];
	int CylBufPos;
	sphere SphBuf[2];
	int SphBufPos;
	box BoxBuf[2];
	int BoxBufPos;
	ray RayBuf[2];

	surface_desc SurfaceDescBuf[64];
	int SurfaceDescBufPos;

	edge_desc EdgeDescBuf[64];
	int EdgeDescBufPos;

	int iFeatureBuf[64];
	int iFeatureBufPos;

	char IdBuf[128];
	int IdBufPos;

	int UsedNodesMap[8192];
	int UsedNodesMapPos;

	int UsedNodesIdx[64];
	int UsedNodesIdxPos;

	geom_contact Contacts[64];
	int nTotContacts;

	geom_contact_area AreaBuf[32];
	int nAreas;
	Vec3 AreaPtBuf[256];
	int AreaPrimBuf0[256],AreaFeatureBuf0[256],AreaPrimBuf1[256],AreaFeatureBuf1[256];
	int nAreaPt;

	Vec3 BrdPtBuf[2048];
	int BrdPtBufPos,BrdPtBufStart;
	int BrdiTriBuf[2048][2];
	float BrdSeglenBuf[2048];
	int UsedVtxMap[4096];
	int UsedTriMap[4096];
	Vec2 PolyPtBuf[1024];
	int PolyVtxIdBuf[1024];
	int PolyEdgeIdBuf[1024];
	int PolyPtBufPos;

	tritem TriQueue[512];
	vtxitem VtxList[512];

	Vec2 BoxCont[8];
	int BoxVtxId[8],BoxEdgeId[8];
	char BoxIdBuf[3];
	surface_desc BoxSurfaceBuf[3];
	edge_desc BoxEdgeBuf[3];

	Vec2 CylCont[32];
	int CylContId[32];
	char CylIdBuf[1];
	surface_desc CylSurfaceBuf[1];
	edge_desc CylEdgeBuf[1];

	BBox BBoxBuf[128];
	int BBoxBufPos;
	BBoxExt BBoxExtBuf[64];
	int BBoxExtBufPos;

	BVheightfield BVhf;

	BVvoxelgrid BVvox;

	BVray BVRay;
	COverlapChecker Overlapper;
	volatile int lockIntersect;
};


extern intersData g_idata[];
#define G(vname) (g_idata[iCaller].vname)

const int k_BBoxBufSize = sizeof(g_idata[0].BBoxBuf)/sizeof(g_idata[0].BBoxBuf[0]);
const int k_BBoxExtBufSize = sizeof(g_idata[0].BBoxExtBuf)/sizeof(g_idata[0].BBoxExtBuf[0]);

#define g_ContiPart ((int(*)[2])g_idata+iCaller)
#define g_IdxTriBuf (g_idata[iCaller].IdxTriBuf)
#define g_IdxTriBufPos (g_idata[iCaller].IdxTriBufPos)
#define g_CylBuf (g_idata[pGTest->iCaller].CylBuf)
#define g_CylBufPos (g_idata[pGTest->iCaller].CylBufPos)
#define g_SphBuf (g_idata[pGTest->iCaller].SphBuf)
#define g_SphBufPos (g_idata[pGTest->iCaller].SphBufPos)
#define g_BoxBuf (g_idata[pGTest->iCaller].BoxBuf)
#define g_BoxBufPos (g_idata[pGTest->iCaller].BoxBufPos)
#define g_RayBuf (g_idata[pGTest->iCaller].RayBuf)

#define g_SurfaceDescBuf (g_idata[pGTest->iCaller].SurfaceDescBuf)
#define g_SurfaceDescBufPos (g_idata[pGTest->iCaller].SurfaceDescBufPos)

#define g_EdgeDescBuf (g_idata[pGTest->iCaller].EdgeDescBuf)
#define g_EdgeDescBufPos (g_idata[pGTest->iCaller].EdgeDescBufPos)

#define g_iFeatureBuf (g_idata[pGTest->iCaller].iFeatureBuf)
#define g_iFeatureBufPos (g_idata[pGTest->iCaller].iFeatureBufPos)

#define g_IdBuf (g_idata[pGTest->iCaller].IdBuf)
#define g_IdBufPos (g_idata[pGTest->iCaller].IdBufPos)

#define g_UsedNodesMap (g_idata[pGTest->iCaller].UsedNodesMap)
#define g_UsedNodesMapPos (g_idata[pGTest->iCaller].UsedNodesMapPos)

#define g_UsedNodesIdx (g_idata[pGTest->iCaller].UsedNodesIdx)
#define g_UsedNodesIdxPos (g_idata[pGTest->iCaller].UsedNodesIdxPos)

#define g_AreaBuf (g_idata[iCaller].AreaBuf)
#define g_AreaPtBuf (g_idata[iCaller].AreaPtBuf)
#define g_AreaPrimBuf0 (g_idata[iCaller].AreaPrimBuf0)
#define g_AreaFeatureBuf0 (g_idata[iCaller].AreaFeatureBuf0)
#define g_AreaPrimBuf1 (g_idata[iCaller].AreaPrimBuf1)
#define g_AreaFeatureBuf1 (g_idata[iCaller].AreaFeatureBuf1)

#define g_BrdPtBuf (g_idata[iCaller].BrdPtBuf)
#define g_BrdPtBufStart (g_idata[iCaller].BrdPtBufStart)
#define g_BrdiTriBuf (g_idata[iCaller].BrdiTriBuf)

#define g_BrdSeglenBuf (g_idata[iCaller].BrdSeglenBuf)
#define g_UsedVtxMap (g_idata[pGTest->iCaller].UsedVtxMap)
#define g_UsedTriMap (g_idata[pGTest->iCaller].UsedTriMap)
#define g_PolyPtBuf (g_idata[pGTest->iCaller].PolyPtBuf)
#define g_PolyVtxIdBuf (g_idata[pGTest->iCaller].PolyVtxIdBuf)
#define g_PolyEdgeIdBuf (g_idata[pGTest->iCaller].PolyEdgeIdBuf)
#define g_PolyPtBufPos (g_idata[pGTest->iCaller].PolyPtBufPos)

#define g_TriQueue (g_idata[pGTest->iCaller].TriQueue)
#define g_VtxList (g_idata[pGTest->iCaller].VtxList)

#define g_BoxCont (g_idata[pGTest->iCaller].BoxCont)
#define g_BoxVtxId (g_idata[pGTest->iCaller].BoxVtxId)
#define g_BoxEdgeId (g_idata[pGTest->iCaller].BoxEdgeId)
#define g_BoxIdBuf (g_idata[pGTest->iCaller].BoxIdBuf)
#define g_BoxSurfaceBuf (g_idata[pGTest->iCaller].BoxSurfaceBuf)
#define g_BoxEdgeBuf (g_idata[pGTest->iCaller].BoxEdgeBuf)

#define g_CylCont (g_idata[pGTest->iCaller].CylCont)
#define g_CylContId (g_idata[pGTest->iCaller].CylContId)
#define g_CylIdBuf (g_idata[pGTest->iCaller].CylIdBuf)
#define g_CylSurfaceBuf (g_idata[pGTest->iCaller].CylSurfaceBuf)
#define g_CylEdgeBuf (g_idata[pGTest->iCaller].CylEdgeBuf)

#define g_BBoxBuf (g_idata[iCaller].BBoxBuf)
#define g_BBoxBufPos (g_idata[iCaller].BBoxBufPos)
#define g_BBoxExtBuf (g_idata[iCaller].BBoxExtBuf)
#define g_BBoxExtBufPos (g_idata[iCaller].BBoxExtBufPos)

#define g_BVhf (g_idata[iCaller].BVhf)

#define g_BVvox (g_idata[iCaller].BVvox)

#define g_Contacts (g_idata[iCaller].Contacts)
#define g_maxContacts (CRY_ARRAY_COUNT(g_Contacts))
#define g_nTotContacts (g_idata[iCaller].nTotContacts)
#define g_BrdPtBufPos (g_idata[iCaller].BrdPtBufPos)
#define g_nAreas (g_idata[iCaller].nAreas)
#define g_nAreaPt (g_idata[iCaller].nAreaPt)
#define g_BVray (g_idata[iCaller].BVRay)

extern volatile int *g_pLockIntersect;

/*extern indexed_triangle g_IdxTriBuf[256];
extern int g_IdxTriBufPos;
extern cylinder g_CylBuf[2];
extern int g_CylBufPos;
extern sphere g_SphBuf[2];
extern int g_SphBufPos;
extern box g_BoxBuf[2];
extern int g_BoxBufPos;
extern ray g_RayBuf[2];

extern surface_desc g_SurfaceDescBuf[64];
extern int g_SurfaceDescBufPos;

extern edge_desc g_EdgeDescBuf[64];
extern int g_EdgeDescBufPos;

extern int g_iFeatureBuf[64];
extern int g_iFeatureBufPos;

extern char g_IdBuf[256];
extern int g_IdBufPos;

extern int g_UsedNodesMap[8192];
extern int g_UsedNodesMapPos;
extern int g_UsedNodesIdx[64];
extern int g_UsedNodesIdxPos;

extern geom_contact g_Contacts[64];
extern int g_nTotContacts;

extern geom_contact_area g_AreaBuf[32];
extern int g_nAreas;
extern Vec3 g_AreaPtBuf[256];
extern int g_AreaPrimBuf0[256],g_AreaFeatureBuf0[256],g_AreaPrimBuf1[256],g_AreaFeatureBuf1[256];
extern int g_nAreaPt;

// trimesh-related
extern Vec3 g_BrdPtBuf[2048];
extern int g_BrdPtBufPos,g_BrdPtBufStart;
extern int g_BrdiTriBuf[2048][2];
extern float g_BrdSeglenBuf[2048];
extern int g_UsedVtxMap[4096];
extern int g_UsedTriMap[4096];
extern Vec2 g_PolyPtBuf[1024];
extern int g_PolyVtxIdBuf[1024];
extern int g_PolyEdgeIdBuf[1024];
extern int g_PolyPtBufPos;

extern tritem g_TriQueue[512];
extern vtxitem g_VtxList[512];

// boxgeom-related
extern Vec2 g_BoxCont[8];
extern int g_BoxVtxId[8],g_BoxEdgeId[8];
extern char g_BoxIdBuf[3];
extern surface_desc g_BoxSurfaceBuf[3];
extern edge_desc g_BoxEdgeBuf[3];

// cylindergeom-related
extern Vec2 g_CylCont[64];
extern int g_CylContId[64];
extern char g_CylIdBuf[1];
extern surface_desc g_CylSurfaceBuf[1];
extern edge_desc g_CylEdgeBuf[1];

extern BBox g_BBoxBuf[128];
extern int g_BBoxBufPos;
extern BBoxExt g_BBoxExtBuf[64];
extern int g_BBoxExtBufPos;

// heightfieldbv-related
extern BVheightfield g_BVhf;

// voxelbv-related
extern BVvoxelgrid g_BVvox;

*/

inline void ResetGlobalPrimsBuffers(int iCaller=0)
{
	g_BBoxBufPos = 0;
	G(IdxTriBufPos) = 0;
	G(CylBufPos) = 0;
	G(BoxBufPos) = 0;
	G(SphBufPos) = 0;
}


class CGeometry : public IGeometry {
public:
	CGeometry() { m_bIsConvex=0; m_pForeignData=0; m_iForeignData=0; m_nRefCount=1; m_lockUpdate=-1; m_minVtxDist=0; m_iCollPriority=0; }
	virtual ~CGeometry() { 
    if (m_iForeignData==DATA_OWNED_OBJECT && m_pForeignData) ((IOwnedObject*)m_pForeignData)->Release(); 
  }
	virtual int GetType() = 0;
	virtual int AddRef() { return CryInterlockedIncrement(&m_nRefCount); }
	virtual void Release();
	virtual void GetBBox(box *pbox) = 0;
	virtual int Intersect(IGeometry *pCollider, geom_world_data *pdata1,geom_world_data *pdata2, intersection_params *pparams, geom_contact *&pcontacts);
	virtual int IntersectLocked(IGeometry *pCollider, geom_world_data *pdata1,geom_world_data *pdata2, intersection_params *pparams, geom_contact *&pcontacts, 
		WriteLockCond &lock,int iCaller);
	virtual int CalcPhysicalProperties(phys_geometry *pgeom) { return 0; }
	virtual int FindClosestPoint(geom_world_data *pgwd, int &iPrim,int &iFeature, const Vec3 &ptdst0,const Vec3 &ptdst1, 
		Vec3 *ptres, int nMaxIters=10) { return 0; }
	virtual int PointInsideStatus(const Vec3 &pt) { return -1; }
	virtual void DrawWireframe(IPhysRenderer *pRenderer, geom_world_data *gwd, int iLevel, int idxColor) { pRenderer->DrawGeometry(this,gwd,idxColor); }
	virtual void CalcVolumetricPressure(geom_world_data *gwd, const Vec3 &epicenter,float k,float rmin, 
		const Vec3 &centerOfMass, Vec3 &P,Vec3 &L) {}
	virtual float CalculateBuoyancy(const plane *pplane, const geom_world_data *pgwd, Vec3 &massCenter) { massCenter.zero(); return 0; }
	virtual void CalculateMediumResistance(const plane *pplane, const geom_world_data *pgwd, Vec3 &dPres,Vec3 &dLres) { dPres.zero(); dLres.zero(); }
	virtual int GetPrimitiveId(int iPrim,int iFeature) { return -1; }
	virtual int GetForeignIdx(int iPrim) { return -1; }
	virtual Vec3 GetNormal(int iPrim, const Vec3 &pt) { return Vec3(0,0,1); }
	virtual int IsConvex(float tolerance) { return 1; }
	virtual void PrepareForRayTest(float raylen) {}
	virtual CBVTree *GetBVTree() { return 0; }
	virtual int GetPrimitiveCount() { return 1; }
	virtual int IsAPrimitive() { return 0; }
	virtual int PreparePrimitive(geom_world_data *pgwd,primitive *&pprim,int iCaller=0) { return -1; }
	virtual int GetFeature(int iPrim,int iFeature, Vec3 *pt) { return 0; }
	virtual int UnprojectSphere(Vec3 center,float r,float rsep, contact *pcontact) { return 0; }
	virtual int Subtract(IGeometry *pGeom, geom_world_data *pdata1,geom_world_data *pdata2, int bLogUpdates=1) { return 0; }
	virtual int GetSubtractionsCount() { return 0; }
	virtual void SetData(const primitive*) {}
	virtual IGeometry *GetTriMesh(int bClone=1) { return 0; }
	int SphereCheck(const Vec3 &center, float r, int iCaller=get_iCaller());
	inline int IntersectQueued(IGeometry *piCollider, geom_world_data *pdata1,geom_world_data *pdata2, intersection_params *pip, geom_contact *&pcontacts, 
		void *pent0,void *pent1,int ipart0,int ipart1) 
	{ return Intersect(piCollider,pdata1,pdata2,pip,pcontacts); }

	virtual float GetVolume() { return 0; }
	virtual Vec3 GetCenter() { return Vec3(ZERO); }
	virtual void *GetForeignData(int iForeignData=0) { return iForeignData==m_iForeignData ? m_pForeignData:0; }
	virtual int GetiForeignData() { return m_iForeignData; }
	virtual void SetForeignData(void *pForeignData, int iForeignData) { m_pForeignData=pForeignData; m_iForeignData=iForeignData; }

	virtual float BuildOcclusionCubemap(geom_world_data *pgwd, int iMode, SOcclusionCubeMap* grid0, SOcclusionCubeMap* grid1, int nGrow);
	virtual int DrawToOcclusionCubemap(const geom_world_data *pgwd, int iStartPrim,int nPrims, int iPass, SOcclusionCubeMap* cubeMap) { return 0; }


	virtual void GetMemoryStatistics(ICrySizer *pSizer) {}
	virtual void Save(CMemStream &stm) {}
	virtual void Load(CMemStream &stm) {}
	virtual void Load(CMemStream &stm, strided_pointer<const Vec3> pVertices, strided_pointer<unsigned short> pIndices, char *pIds) { Load(stm); }
	virtual int GetErrorCount() { return 0; }
	virtual int GetSizeFast() { return 0; }
	virtual int PrepareForIntersectionTest(geometry_under_test *pGTest, CGeometry *pCollider,geometry_under_test *pGTestColl, bool bKeepPrevContacts=false) 
	{ return 1; };
	virtual int RegisterIntersection(primitive *pprim1,primitive *pprim2, geometry_under_test *pGTest1,geometry_under_test *pGTest2, 
		prim_inters *pinters);
	virtual void CleanupAfterIntersectionTest(geometry_under_test *pGTest) {}
	virtual int GetPrimitiveList(int iStart,int nPrims, int typeCollider,primitive *pCollider,int bColliderLocal, geometry_under_test *pGTest,
		geometry_under_test *pGTestOp, primitive *pRes,char *pResId) { return 0; }
	virtual int GetUnprojectionCandidates(int iop,const contact *pcontact, primitive *&pprim,int *&piFeature, geometry_under_test *pGTest) { 
		pGTest->nSurfaces=pGTest->nEdges = 0; return 0; 
	}
	virtual int PreparePolygon(coord_plane *psurface, int iPrim,int iFeature, geometry_under_test *pGTest, Vec2 *&ptbuf,
		int *&pVtxIdBuf,int *&pEdgeIdBuf) { return 0; }
	virtual int PreparePolyline(coord_plane *psurface, int iPrim,int iFeature, geometry_under_test *pGTest, Vec2 *&ptbuf,
		int *&pVtxIdBuf,int *&pEdgeIdBuf) { return 0; }

	virtual void Lock(int bWrite=1) { 
		if (m_lockUpdate>>31 & bWrite)
			m_lockUpdate = 0;
		if (bWrite) {
			m_lockUpdate += isneg(m_lockUpdate);
			SpinLock(&m_lockUpdate,0,WRITE_LOCK_VAL); 
		} else if (m_lockUpdate>=0) {
			AtomicAdd(&m_lockUpdate,1);
			volatile char *pw=(volatile char*)&m_lockUpdate+(1+eBigEndian); for(;*pw;);
		}
	}
	virtual void Unlock(int bWrite=1) { AtomicAdd(&m_lockUpdate,-1-((WRITE_LOCK_VAL-1)&-bWrite) & ~(m_lockUpdate>>31)); }

	virtual void DestroyAuxilaryMeshData(int) {}
	virtual void RemapForeignIdx(int *pCurForeignIdx, int *pNewForeignIdx, int nTris) {}
	virtual void AppendVertices(Vec3 *pVtx,int *pVtxMap, int nVtx) {}
	virtual float GetExtent(EGeomForm eForm) const;
	virtual void GetRandomPoints(Array<PosNorm> points, CRndGen& seed, EGeomForm eForm) const;
	virtual void CompactMemory() { }
	virtual int Boxify(primitives::box *pboxes,int nMaxBoxes, const SBoxificationParams &params) { return 0; }
	virtual int Proxify(IGeometry **&pOutGeoms, SProxifyParams *pparams=0) { return 0; }
	virtual int SanityCheck() { return 1; }

	volatile int m_lockUpdate;
	volatile int m_nRefCount;
	float m_minVtxDist;
	int m_iCollPriority;
	int m_bIsConvex;
	void *m_pForeignData;
	int m_iForeignData;
};

class CPrimitive : public CGeometry {
public:
	CPrimitive() { m_bIsConvex=1; }
	virtual int Intersect(IGeometry *pCollider, geom_world_data *pdata1,geom_world_data *pdata2, intersection_params *pparams, geom_contact *&pcontacts);
	virtual int IsAPrimitive() { return 1; }
};

void DrawBBox(IPhysRenderer *pRenderer,int idxColor, geom_world_data *gwd, CBVTree *pTree,BBox *pbbox,int maxlevel,int level=0,int iCaller=0);
int SanityCheckTree(CBVTree* pBVtree, int maxDepth);

struct InitGeometryGlobals { InitGeometryGlobals(); }; 

#endif
