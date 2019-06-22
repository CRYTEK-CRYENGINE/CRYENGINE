// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

struct SIntersectionData;

class CRenderMeshUtils : public Cry3DEngineBase
{
public:
	// Do a render-mesh vs Ray intersection, return true for intersection.
	static bool RayIntersection(IRenderMesh* pRenderMesh, SRayHitInfo& hitInfo, IMaterial* pMtl = 0);
	static bool RayIntersectionFast(IRenderMesh* pRenderMesh, SRayHitInfo& hitInfo, IMaterial* pCustomMtl = 0);

	// async versions, aren't using the cache, and are used by the deferredrayintersection class
	static void RayIntersectionAsync(SIntersectionData* pIntersectionRMData);

	static void ClearHitCache();

	//////////////////////////////////////////////////////////////////////////
	//void FindCollisionWithRenderMesh( IRenderMesh *pRenderMesh, SRayHitInfo &hitInfo );
	//	void FindCollisionWithRenderMesh( IPhysiIRenderMesh2 *pRenderMesh, SRayHitInfo &hitInfo );
private:
	// functions implementing the logic for RayIntersection
	static bool RayIntersectionImpl(SIntersectionData* pIntersectionRMData, SRayHitInfo* phitInfo, IMaterial* pCustomMtl, bool bAsync);
	static bool RayIntersectionFastImpl(SIntersectionData& rIntersectionRMData, SRayHitInfo& hitInfo, IMaterial* pCustomMtl, bool bAsync);
#if defined(FEATURE_SVO_GI)
	static bool ProcessBoxIntersection(Ray& inRay, SRayHitInfo& hitInfo, SIntersectionData& rIntersectionRMData, IMaterial* pMtl, vtx_idx* pInds, int nVerts, uint8* pPos, int nPosStride, uint8* pUV, int nUVStride, uint8* pCol, int nColStride, byte* pTangs, int nTangsStride, int nInds, bool& bAnyHit, float& fBestDist, Vec3& vHitPos, Vec3* tri);
#endif
};

// struct to collect parameters for the wrapped RayInterseciton functions
struct SIntersectionData
{
	SIntersectionData() :
		pRenderMesh(NULL), nVerts(0), nInds(0),
		nPosStride(0), pPos(NULL), pInds(NULL),
#if defined(FEATURE_SVO_GI)
		nUVStride(0), pUV(NULL),
		nColStride(0), pCol(NULL),
		nTangsStride(0), pTangs(NULL),
#endif
		bResult(false), bNeedFallback(false),
		fDecalPlacementTestMaxSize(1000.f), bDecalPlacementTestRequested(false),
		pHitInfo(0), pMtl(0)
	{
	}

	bool Init(IRenderMesh* pRenderMesh, SRayHitInfo* _pHitInfo, IMaterial* _pMtl, bool _bRequestDecalPlacementTest = false);

	IRenderMesh* pRenderMesh;
	SRayHitInfo* pHitInfo;
	IMaterial*   pMtl;
	bool         bDecalPlacementTestRequested;

	int          nVerts;
	int          nInds;

	int          nPosStride;
	uint8*       pPos;
	vtx_idx*     pInds;

#if defined(FEATURE_SVO_GI)
	int          nUVStride;
	uint8*       pUV;

	int          nColStride;
	uint8*       pCol;

	int          nTangsStride;
	byte*        pTangs;
#endif

	bool         bResult;
	float        fDecalPlacementTestMaxSize; // decal will look acceptable in this place
	bool         bNeedFallback;
};
