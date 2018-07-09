// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#ifndef __RenderMeshMerger_H
#define __RenderMeshMerger_H

#pragma once

//////////////////////////////////////////////////////////////////////////
// Input structure for RenderMesh merger
//////////////////////////////////////////////////////////////////////////
struct SRenderMeshInfoInput
{
	_smart_ptr<IRenderMesh> pMesh;
	_smart_ptr<IMaterial>   pMat;
	IRenderNode*            pSrcRndNode;
	Matrix34                mat;
	int                     nSubObjectIndex;
	int                     nChunkId;
	bool                    bIdentityMatrix;

	SRenderMeshInfoInput() : pMesh(0), pMat(0), nChunkId(-1), nSubObjectIndex(0), pSrcRndNode(0), bIdentityMatrix(false) { mat.SetIdentity(); }

	void GetMemoryUsage(ICrySizer* pSizer) const {}
};

struct SDecalClipInfo
{
	Vec3  vPos;
	float fRadius;
	Vec3  vProjDir;
};

struct SMergeInfo
{
	const char*     sMeshType;
	const char*     sMeshName;
	bool            bCompactVertBuffer;
	bool            bPrintDebugMessages;
	bool            bMakeNewMaterial;
	bool            bMergeToOneRenderMesh;
	bool            bPlaceInstancePositionIntoVertexNormal;
	IMaterial*      pUseMaterial; // Force to use this material

	SDecalClipInfo* pDecalClipInfo;
	AABB*           pClipCellBox;
	Vec3            vResultOffset; // this offset will be subtracted from output vertex positions

	SMergeInfo() :
		sMeshType(""),
		sMeshName(""),
		bCompactVertBuffer(false),
		bPrintDebugMessages(false),
		bMakeNewMaterial(true),
		bMergeToOneRenderMesh(false),
		bPlaceInstancePositionIntoVertexNormal(0),
		pUseMaterial(0),
		pDecalClipInfo(0),
		pClipCellBox(0),
		vResultOffset(0, 0, 0) {}
};

struct SMergedChunk
{
	CRenderChunk          rChunk;
	IMaterial*            pMaterial;
	SRenderMeshInfoInput* pFromMesh;

	void                  GetMemoryUsage(ICrySizer* pSizer) const {}
};

struct SMergeBuffersData
{
	int      nPosStride;
	int      nTexStride;
	int      nColorStride;
	int      nTangsStride;
	int      nIndCount;
	uint8*   pPos;
	uint8*   pTex;
	uint8*   pColor;
	uint8*   pTangs;
	vtx_idx* pSrcInds;
#if ENABLE_NORMALSTREAM_SUPPORT
	int      nNormStride;
	uint8*   pNorm;
#endif
};

class CRenderMeshMerger : public Cry3DEngineBase
{
public:
	CRenderMeshMerger(void);
	~CRenderMeshMerger(void);

	void                    Reset();

	_smart_ptr<IRenderMesh> MergeRenderMeshes(SRenderMeshInfoInput* pRMIArray, int nRMICount, PodArray<SRenderMeshInfoOutput>& outRenderMeshes, SMergeInfo& info);

	_smart_ptr<IRenderMesh> MergeRenderMeshes(SRenderMeshInfoInput* pRMIArray, int nRMICount, SMergeInfo& info);
public:

	void GetMemoryUsage(ICrySizer* pSizer) const
	{
		pSizer->AddObject(this, sizeof(*this));

		pSizer->AddObject(m_lstRMIChunks);
		pSizer->AddObject(m_lstVerts);
		pSizer->AddObject(m_lstTangBasises);
		pSizer->AddObject(m_lstIndices);

		pSizer->AddObject(m_lstChunks);
		pSizer->AddObject(m_lstChunksAll);

		pSizer->AddObject(m_lstNewVerts);
		pSizer->AddObject(m_lstNewTangBasises);
		pSizer->AddObject(m_lstNewIndices);
		pSizer->AddObject(m_lstNewChunks);

		pSizer->AddObject(m_lstChunksMergedTemp);

		pSizer->AddObject(m_tmpRenderChunkArray);
		pSizer->AddObject(m_tmpClipContext);
	}

	void MergeBuffersImpl(AABB* pBounds, SMergeBuffersData* arrMergeBuffersData);

protected:
	PodArray<SRenderMeshInfoInput> m_lstRMIChunks;
	PodArray<SVF_P3S_C4B_T2S>      m_lstVerts;
	PodArray<SPipTangents>         m_lstTangBasises;
	PodArray<uint32>               m_lstIndices;

	PodArray<SMergedChunk>         m_lstChunks;
	PodArray<SMergedChunk>         m_lstChunksAll;

	PodArray<SVF_P3S_C4B_T2S>      m_lstNewVerts;
	PodArray<SPipTangents>         m_lstNewTangBasises;
	PodArray<vtx_idx>              m_lstNewIndices;
	PodArray<SMergedChunk>         m_lstNewChunks;

#if ENABLE_NORMALSTREAM_SUPPORT
	PodArray<SPipNormal> m_lstNormals;
	PodArray<SPipNormal> m_lstNewNormals;
#endif

	PodArray<SMergedChunk>      m_lstChunksMergedTemp;

	TRenderChunkArray           m_tmpRenderChunkArray;

	PodArray<SMergeBuffersData> m_lstMergeBuffersData;
	AABB                        m_tmpAABB;

	CPolygonClipContext         m_tmpClipContext;

	int                         m_nTotalVertexCount;
	int                         m_nTotalIndexCount;

private:
	bool       GenerateRenderChunks(SRenderMeshInfoInput* pRMIArray, int nRMICount);
	void       MergeRenderChunks();
	void       MergeBuffers(AABB& bounds);

	void       MakeRenderMeshInfoListOfAllChunks(SRenderMeshInfoInput* pRMIArray, int nRMICount, SMergeInfo& info);
	void       MakeListOfAllCRenderChunks(SMergeInfo& info);
	void       IsChunkValid(CRenderChunk& Ch, PodArray<SVF_P3S_C4B_T2S>& lstVerts, PodArray<uint32>& lstIndices);

	void       ClipByAABB(SMergeInfo& info);
	void       ClipDecals(SMergeInfo& info);
	void       CompactVertices(SMergeInfo& info);
	void       TryMergingChunks(SMergeInfo& info);

	bool       ClipTriangle(int nStartIdxId, Plane* pPlanes, int nPlanesNum);

	static int Cmp_Materials(IMaterial* pMat1, IMaterial* pMat2);
	static int Cmp_RenderChunks_(const void* v1, const void* v2);
	static int Cmp_RenderChunksInfo(const void* v1, const void* v2);

};

#endif //__RenderMeshMerger_H
