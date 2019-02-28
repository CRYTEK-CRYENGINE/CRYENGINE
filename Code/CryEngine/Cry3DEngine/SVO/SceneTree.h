// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#ifndef __SCENETREE_H__
#define __SCENETREE_H__

#if defined(FEATURE_SVO_GI)

	#pragma pack(push,4)

typedef std::unordered_map<uint64, std::pair<byte, byte>> PvsMap;
typedef std::set<class CVoxelSegment*>                    VsSet;

template<class T, int maxElemsInChunk> class CCustomSVOPoolAllocator
{
public:
	CCustomSVOPoolAllocator() { m_numElements = 0; }

	~CCustomSVOPoolAllocator()
	{
		Reset();
	}

	void Reset()
	{
		for (int i = 0; i < m_chunksPool.Count(); i++)
		{
			delete[](byte*)m_chunksPool[i];
			m_chunksPool[i] = NULL;
		}
		m_numElements = 0;
	}

	void ReleaseElement(T* pElem)
	{
		if (pElem)
			m_freeElements.Add(pElem);
	}

	T* GetNewElement()
	{
		if (m_freeElements.Count())
		{
			T* pPtr = m_freeElements.Last();
			m_freeElements.DeleteLast();
			return pPtr;
		}

		int poolId = m_numElements / maxElemsInChunk;
		int elemId = m_numElements - poolId * maxElemsInChunk;
		m_chunksPool.PreAllocate(poolId + 1, poolId + 1);
		if (!m_chunksPool[poolId])
			m_chunksPool[poolId] = (T*)new byte[maxElemsInChunk * sizeof(T)];
		m_numElements++;
		return &m_chunksPool[poolId][elemId];
	}

	int GetCount()         { return m_numElements - m_freeElements.Count(); }
	int GetCapacity()      { return m_chunksPool.Count() * maxElemsInChunk; }
	int GetCapacityBytes() { return GetCapacity() * sizeof(T); }

private:

	int          m_numElements;
	PodArray<T*> m_chunksPool;
	PodArray<T*> m_freeElements;
};

class CSvoNode
{
public:

	CSvoNode(const AABB& box, CSvoNode* pParent);
	~CSvoNode();

	static void*         operator new(size_t);
	static void          operator delete(void* ptr);

	void                 CheckAllocateChilds();
	void                 DeleteChilds();
	void                 Render(PodArray<struct SPvsItem>* pSortedPVS, uint64 nodeKey, int treeLevel, PodArray<SVF_P3F_C4B_T2F>& arrVertsOut, PodArray<class CVoxelSegment*> arrForStreaming[SVO_STREAM_QUEUE_MAX_SIZE][SVO_STREAM_QUEUE_MAX_SIZE]);
	bool                 IsStreamingInProgress();
	void                 GetTrisInAreaStats(int& trisCount, int& vertCount, int& trisBytes, int& vertBytes, int& maxVertPerArea, int& matsCount);
	void                 GetVoxSegMemUsage(int& allocated);
	AABB                 GetChildBBox(int childId);
	void                 CheckAllocateSegment(int lod);
	void                 OnStatLightsChanged(const AABB& objBox);
	class CVoxelSegment* AllocateSegment(int cloudId, int stationId, int lod, EFileStreamingStatus eStreamingStatus, bool bDroppedOnDisk);
	uint32               SaveNode(PodArray<byte>& arrData, uint32& nNodesCounter, ICryArchive* pArchive, uint32& totalSizeCounter);
	void                 MakeNodeFilePath(char* szFileName);
	bool                 CheckReadyForRendering(int treeLevel, PodArray<CVoxelSegment*> arrForStreaming[SVO_STREAM_QUEUE_MAX_SIZE][SVO_STREAM_QUEUE_MAX_SIZE]);
	CSvoNode*            FindNodeByPosition(const Vec3& vPosWS, int treeLevelToFind, int treeLevelCur);
	void                 UpdateNodeRenderDataPtrs();
	void                 RegisterMovement(const AABB& objBox);
	Vec3i                GetStatGeomCheckSumm();
	CSvoNode*            FindNodeByPoolAffset(int allocatedAtlasOffset);
	static bool          IsStreamingActive();

	AABB                       m_nodeBox;
	CSvoNode**                 m_ppChilds;
	std::pair<uint32, uint32>* m_pChildFileOffsets;
	CSvoNode*                  m_pParent;
	CVoxelSegment*             m_pSeg;
	uint                       m_requestSegmentUpdateFrametId;
	bool                       m_arrChildNotNeeded[8];
	bool                       m_bForceRecreate;
};

class CPointTreeNode
{
public:
	bool TryInsertPoint(int pointId, const Vec3& vPos, const AABB& nodeBox, int recursionLevel = 0);
	bool IsThereAnyPointInTheBox(const AABB& testBox, const AABB& nodeBox);
	bool GetAllPointsInTheBox(const AABB& testBox, const AABB& nodeBox, PodArray<int>& arrIds);
	AABB GetChildBBox(int childId, const AABB& nodeBox);
	void Clear();
	CPointTreeNode() { m_ppChilds = 0; m_pPoints = 0; }
	~CPointTreeNode() { Clear(); }

	struct SPointInfo
	{ Vec3 vPos; int id; };
	PodArray<SPointInfo>* m_pPoints;
	CPointTreeNode**      m_ppChilds;
};

struct SBrickSubSet
{
	ColorB arrData[SVO_VOX_BRICK_MAX_SIZE * SVO_VOX_BRICK_MAX_SIZE * SVO_VOX_BRICK_MAX_SIZE];
};

class CSvoEnv : public Cry3DEngineBase
{
public:

	CSvoEnv(const AABB& worldBox);
	~CSvoEnv();
	bool GetSvoStaticTextures(I3DEngine::SSvoStaticTexInfo& svoInfo, PodArray<I3DEngine::SLightTI>* pLightsTI_S, PodArray<I3DEngine::SLightTI>* pLightsTI_D);
	void GetSvoBricksForUpdate(PodArray<I3DEngine::SSvoNodeInfo>& arrNodeInfo, float nodeSize, PodArray<SVF_P3F_C4B_T2F>* pVertsOut);
	bool Render();
	void ProcessSvoRootTeleport();
	void CheckUpdateMeshPools();
	int  GetWorstPointInSubSet(const int start, const int end);
	void StartupStreamingTimeTest(bool bDone);
	void OnLevelGeometryChanged();
	void ReconstructTree(bool bMultiPoint);
	void AllocateRootNode();
	int  ExportSvo(ICryArchive* pArchive);
	void DetectMovement_StaticGeom();
	void DetectMovement_StatLights();
	void CollectLights();
	void CollectAnalyticalOccluders();
	void AddAnalyticalOccluder(IRenderNode* pRN, Vec3 camPos);
	void GetGlobalEnvProbeProperties(_smart_ptr<ITexture>& specEnvCM, float& mult);

	PodArray<I3DEngine::SLightTI>            m_lightsTI_S, m_lightsTI_D;
	PodArray<I3DEngine::SAnalyticalOccluder> m_analyticalOccluders[2];
	Vec4                      m_vSvoOriginAndSize;
	AABB                      m_aabbLightsTI_D;
	SRenderLight*             m_pGlobalEnvProbe;
	CryCriticalSection        m_csLockGlobalEnvProbe;
	CVoxStreamEngine*         m_pStreamEngine;
	CSvoNode*                 m_pSvoRoot;
	bool                      m_bReady;
	bool                      m_bRootTeleportSkipFrame = false;
	PodArray<CVoxelSegment*>  m_arrForStreaming[SVO_STREAM_QUEUE_MAX_SIZE][SVO_STREAM_QUEUE_MAX_SIZE];
	int                       m_debugDrawVoxelsCounter;
	int                       m_nodeCounter;
	int                       m_dynNodeCounter;
	int                       m_dynNodeCounter_DYNL;
	PodArray<CVoxelSegment*>  m_arrForBrickUpdate[16];
	CryCriticalSection        m_csLockTree;
	float                     m_streamingStartTime;
	float                     m_svoFreezeTime;
	int                       m_arrVoxelizeMeshesCounter[2];
	AABB                      m_worldBox;
	PodArray<SVF_P3F_C4B_T2F> m_arrSvoProxyVertices;
	double                    m_prevCheckVal;
	bool                      m_bFirst_SvoFreezeTime;
	bool                      m_bFirst_StartStreaming;
	bool                      m_bStreamingDonePrev;

	int                       m_texOpasPoolId;
	int                       m_texNodePoolId;
	int                       m_texNormPoolId;
	int                       m_texRgb0PoolId;
	int                       m_texRgb1PoolId;
	int                       m_texRgb2PoolId;
	int                       m_texRgb3PoolId;
	int                       m_texRgb4PoolId;
	int                       m_texDynlPoolId;
	int                       m_texAldiPoolId;
	#ifdef FEATURE_SVO_GI_USE_MESH_RT
	int                       m_texTrisPoolId;
	#endif

	ETEX_Format                                       m_voxTexFormat;
	TDoublyLinkedList<CVoxelSegment>                  m_arrSegForUnload;
	CCustomSVOPoolAllocator<struct SBrickSubSet, 128> m_brickSubSetAllocator;
	CCustomSVOPoolAllocator<CSvoNode, 128>            m_nodeAllocator;

	#ifdef FEATURE_SVO_GI_USE_MESH_RT
	PodArrayRT<ColorB> m_arrRTPoolTexs;
	PodArrayRT<Vec4>   m_arrRTPoolTris;
	PodArrayRT<ColorB> m_arrRTPoolInds;
	#endif
};

	#pragma pack(pop)

#endif

#endif
