// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#ifndef __CGFSaver_h__
#define __CGFSaver_h__

#if defined(RESOURCE_COMPILER) || defined(INCLUDE_SAVECGF)

	#include <Cry3DEngine/CGF/CGFContent.h>

	#if defined(RESOURCE_COMPILER)
class CInternalSkinningInfo;
	#endif

class CChunkFile;

//////////////////////////////////////////////////////////////////////////
class CSaverCGF
{
public:
	CSaverCGF(CChunkFile& chunkFile);

	void SaveContent(CContentCGF* pCGF, bool bSwapEndian, bool bStorePositionsAsF16, bool bUseQtangents, bool bStoreIndicesAsU16);

	void SetContent(CContentCGF* pCGF);

	// Enable/Disable saving of the node mesh.
	void SetMeshDataSaving(bool bEnable);

	// Enable/Disable saving of non-mesh related data.
	void SetNonMeshDataSaving(bool bEnable);

	// Enable/disable saving of physics meshes.
	void SetSavePhysicsMeshes(bool bEnable);

	// Enable compaction of vertex streams (for optimised streaming)
	void SetVertexStreamCompacting(bool bEnable);

	// Enable computation of subset texel density
	void SetSubsetTexelDensityComputing(bool bEnable);

	// Store nodes in chunk file.
	#if defined(RESOURCE_COMPILER)
	void SaveNodes(bool bSwapEndian, bool bStorePositionsAsF16, bool bUseQtangents, bool bStoreIndicesAsU16, CInternalSkinningInfo* pSkinningInfo);
	int  SaveNode(CNodeCGF* pNode, bool bSwapEndian, bool bStorePositionsAsF16, bool bUseQtangents, bool bStoreIndicesAsU16, CInternalSkinningInfo* pSkinningInfo);
	#else
	void SaveNodes(bool bSwapEndian, bool bStorePositionsAsF16, bool bUseQtangents, bool bStoreIndicesAsU16);
	int  SaveNode(CNodeCGF* pNode, bool bSwapEndian, bool bStorePositionsAsF16, bool bUseQtangents, bool bStoreIndicesAsU16);
	#endif
	void SaveMaterials(bool bSwapEndian);
	int  SaveMaterial(CMaterialCGF* pMtl, bool bNeedSwap);
	int  SaveExportFlags(bool bSwapEndian);

	// Compiled chunks for characters
	int SaveCompiledBones(bool bSwapEndian, void* pData, int nSize, int version);
	int SaveCompiledPhysicalBones(bool bSwapEndian, void* pData, int nSize, int version);
	int SaveCompiledPhysicalProxis(bool bSwapEndian, void* pData, int nSize, uint32 numIntMorphTargets, int version);
	int SaveCompiledMorphTargets(bool bSwapEndian, void* pData, int nSize, uint32 numIntMorphTargets, int version);
	int SaveCompiledIntFaces(bool bSwapEndian, void* pData, int nSize, int version);
	int SaveCompiledIntSkinVertices(bool bSwapEndian, void* pData, int nSize, int version);
	int SaveCompiledExt2IntMap(bool bSwapEndian, void* pData, int nSize, int version);
	int SaveCompiledBoneBox(bool bSwapEndian, void* pData, int nSize, int version);

	// Chunks for characters (for Collada->cgf export)
	int  SaveBones(bool bSwapEndian, void* pData, int numBones, int nSize);
	int  SaveBoneNames(bool bSwapEndian, char* boneList, int numBones, int listSize);
	int  SaveBoneInitialMatrices(bool bSwapEndian, SBoneInitPosMatrix* matrices, int numBones, int nSize);
	int  SaveBoneMesh(bool bSwapEndian, const PhysicalProxy& proxy);
	#if defined(RESOURCE_COMPILER)
	void SaveUncompiledNodes();
	int  SaveUncompiledNode(CNodeCGF* pNode);
	void SaveUncompiledMorphTargets();
	int  SaveUncompiledNodeMesh(CNodeCGF* pNode);
	int  SaveUncompiledHelperChunk(CNodeCGF* pNode);
	#endif

	int SaveBreakablePhysics(bool bNeedEndianSwap);

	int SaveController829(bool bSwapEndian, const CONTROLLER_CHUNK_DESC_0829& ctrlChunk, void* pData, int nSize);
	int SaveController832(bool bSwapEndian, const CONTROLLER_CHUNK_DESC_0832& ctrlChunk, void* pData, int nSize);
	int SaveControllerDB905(bool bSwapEndian, const CONTROLLER_CHUNK_DESC_0905& ctrlChunk, void* pData, int nSize);

	int SaveFoliage();

	int SaveVCloth(bool bSwapEndian);

	#if defined(RESOURCE_COMPILER)
	int SaveTiming(CInternalSkinningInfo* pSkinningInfo);
	#endif

private:
	// Return mesh chunk id
	int SaveNodeMesh(CNodeCGF* pNode, bool bSwapEndian, bool bStorePositionsAsF16, bool bUseQTangents, bool bStoreIndicesAsU16);
	int SaveHelperChunk(CNodeCGF* pNode, bool bSwapEndian);
	int SaveMeshSubsetsChunk(CMesh& mesh, bool bSwapEndian);
	int SaveStreamDataChunk(const void* pStreamData, int nStreamType, int nCount, int nElemSize, bool bSwapEndian);
	int SavePhysicalDataChunk(const void* pData, int nSize, bool bSwapEndian);

	#if defined(RESOURCE_COMPILER)
	int SaveTCB3Track(CInternalSkinningInfo* pSkinningInfo, int trackIndex);
	int SaveTCBQTrack(CInternalSkinningInfo* pSkinningInfo, int trackIndex);
	#endif

private:
	CChunkFile*             m_pChunkFile;
	CContentCGF*            m_pCGF;
	std::set<CNodeCGF*>     m_savedNodes;
	std::set<CMaterialCGF*> m_savedMaterials;
	std::map<CMesh*, int>   m_mapMeshToChunk;
	bool                    m_bDoNotSaveMeshData;
	bool                    m_bDoNotSaveNonMeshData;
	bool                    m_bSavePhysicsMeshes;
	bool                    m_bCompactVertexStreams;
	bool                    m_bComputeSubsetTexelDensity;
};

#endif
#endif
