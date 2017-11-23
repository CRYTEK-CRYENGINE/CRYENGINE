// Copyright 2001-2017 Crytek GmbH / Crytek Group. All rights reserved.

#ifndef __SCENETREE_H__
#define __SCENETREE_H__

#if defined(FEATURE_SVO_GI)

	#pragma pack(push,4)

	#define SCENE_SCAN_FILE_VERSION     2
	#define SVO_FILE_VERSION            1
	#define GLOBAL_CLOUD_TREE_FILE_NAME "TREE.DAT"
	#define GLOBAL_CLOUD_MESH_FILE_NAME "CCM.DAT"
	#define GLOBAL_CLOUD_PVS_FILE_NAME  "PVS.DAT"

typedef std::unordered_map<uint64, std::pair<byte, byte>> PvsMap;
typedef std::set<class CVoxelSegment*>                    VsSet;

const int nVoxStreamQueueMaxSize = 12;
const float fMaxScanDistance = 256;

template<class T, int nMaxElemsInChunk> class CCustomSVOPoolAllocator
{
public:
	CCustomSVOPoolAllocator() { m_nCounter = 0; }

	~CCustomSVOPoolAllocator()
	{
		Reset();
	}

	void Reset()
	{
		for (int i = 0; i < m_Pools.Count(); i++)
		{
			delete[](byte*)m_Pools[i];
			m_Pools[i] = NULL;
		}
		m_nCounter = 0;
	}

	void ReleaseElement(T* pElem)
	{
		if (pElem)
			m_FreeElements.Add(pElem);
	}

	T* GetNewElement()
	{
		if (m_FreeElements.Count())
		{
			T* pPtr = m_FreeElements.Last();
			m_FreeElements.DeleteLast();
			return pPtr;
		}

		int nPoolId = m_nCounter / nMaxElemsInChunk;
		int nElemId = m_nCounter - nPoolId * nMaxElemsInChunk;
		m_Pools.PreAllocate(nPoolId + 1, nPoolId + 1);
		if (!m_Pools[nPoolId])
			m_Pools[nPoolId] = (T*)new byte[nMaxElemsInChunk * sizeof(T)];
		m_nCounter++;
		return &m_Pools[nPoolId][nElemId];
	}

	int GetCount()         { return m_nCounter - m_FreeElements.Count(); }
	int GetCapacity()      { return m_Pools.Count() * nMaxElemsInChunk; }
	int GetCapacityBytes() { return GetCapacity() * sizeof(T); }

private:

	int          m_nCounter;
	PodArray<T*> m_Pools;
	PodArray<T*> m_FreeElements;
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
	void                 Render(PodArray<struct SPvsItem>* pSortedPVS, uint64 nNodeKey, int nTreeLevel, PodArray<SVF_P3F_C4B_T2F>& arrVertsOut, PodArray<class CVoxelSegment*> arrForStreaming[nVoxStreamQueueMaxSize][nVoxStreamQueueMaxSize]);
	bool                 IsStreamingInProgress();
	void                 GetTrisInAreaStats(int& nTrisCount, int& nVertCount, int& nTrisBytes, int& nVertBytes, int& nMaxVertPerArea, int& nMatsCount);
	void                 GetVoxSegMemUsage(int& nAllocated);
	AABB                 GetChildBBox(int nChildId);
	bool                 TryInsertPoint(const Vec3& vPos, const ColorB& vNormalB, float fAmbFactor, const ColorB& vColor, int32& nInserted, float fDot, PodArray<Vec3>& arrScanPoints, bool bImportDVB = false);
	void                 CheckAllocateSegment(int nLod);
	void                 RayTrace(Lineseg& rLS, Vec3 vLineDir, VsSet& visitedNodes, ColorB& hitColor, ColorB& hitNormal);
	void                 OnStatLightsChanged(const AABB& objBox);
	class CVoxelSegment* AllocateSegment(int nCloudId, int nStationId, int nLod, EFileStreamingStatus eStreamingStatus, bool bDroppedOnDisk);
	void                 ExportRenderData(int& nNodesProcessed, int nNodesAll, FILE* pFileToWrite, const int nTreeLevel, const int nTreeLevelToExport);
	void                 TestRefCount(int& nLeaks);
	uint32               SaveNode(PodArray<byte>& arrData, uint32& nNodesCounter, ICryArchive* pArchive, uint32& totalSizeCounter);
	void                 MakeNodeFilePath(char* szFileName);
	void                 CutChilds();
	bool                 CheckReadyForRendering(int nTreeLevel, PodArray<CVoxelSegment*> arrForStreaming[nVoxStreamQueueMaxSize][nVoxStreamQueueMaxSize]);
	void                 CollectResetVisNodes(PvsMap& arrNodes, uint64 nNodeKey);
	void                 StartBuildAOTasks(int& nNodesProcessed, int nNodesAll);
	void                 ComputeAO(int nTID);
	void                 Task_Compute(int nTID)      { ComputeAO(nTID); }
	void                 Task_Store();
	static void          Task_OnTaskConveyorFinish() {}
	void                 BuildMips(int& nNodesProcessed, int nNodesAll);
	void                 BuildParentMips();
	void                 FixGaps(int& nNodesProcessed, int nNodesAll);
	void                 FixSeams(int& nNodesProcessed, int nNodesAll, int nTreeLevelCur);
	void                 FixSeamsFromNeigbours(int nTreeLevelCur);
	CSvoNode*            FindNodeByPosition(const Vec3& vPosWS, int nTreeLevelToFind, int nTreeLevelCur);
	void                 FixGapsInChilds();
	bool                 IsNodeGoodForExport();
	void                 UpdateNodeRenderDataPtrs();
	void                 RegisterMovement(const AABB& objBox);
	Vec3i                GetStatGeomCheckSumm();
	CSvoNode*            FindNodeByPoolAffset(int nAllocatedAtlasOffset);
	static bool          IsStreamingActive();

	AABB                       m_nodeBox;
	CSvoNode**                 m_ppChilds;
	std::pair<uint32, uint32>* m_pChildFileOffsets;
	CSvoNode*                  m_pParent;
	CVoxelSegment*             m_pSeg;
	uint                       m_nRequestSegmentUpdateFrametId;
	bool                       m_arrChildNotNeeded[8];
	bool                       m_bForceRecreate;
};

class CPointTreeNode
{
public:
	bool TryInsertPoint(int nId, const Vec3& vPos, const AABB& nodeBox, int nRecursionLevel = 0);
	bool IsThereAnyPointInTheBox(const AABB& testBox, const AABB& nodeBox);
	bool GetAllPointsInTheBox(const AABB& testBox, const AABB& nodeBox, PodArray<int>& arrIds);
	AABB GetChildBBox(int nChildId, const AABB& nodeBox);
	void Clear();
	CPointTreeNode() { m_ppChilds = 0; m_pPoints = 0; }
	~CPointTreeNode() { Clear(); }

	struct SPointInfo
	{ Vec3 vPos; int nId; };
	PodArray<SPointInfo>* m_pPoints;
	CPointTreeNode**      m_ppChilds;
};

class CSvoEnv : public Cry3DEngineBase
{
public:

	CSvoEnv(const AABB& worldBox);
	~CSvoEnv();
	bool GetSvoStaticTextures(I3DEngine::SSvoStaticTexInfo& svoInfo, PodArray<I3DEngine::SLightTI>* pLightsTI_S, PodArray<I3DEngine::SLightTI>* pLightsTI_D);
	void GetSvoBricksForUpdate(PodArray<I3DEngine::SSvoNodeInfo>& arrNodeInfo, float fNodeSize, PodArray<SVF_P3F_C4B_T2F>* pVertsOut);
	bool Render();
	void ProcessSvoRootTeleport();
	void CheckUpdateMeshPools();
	int  GetWorstPointInSubSet(const int nStart, const int eEnd);
	void StartupStreamingTimeTest(bool bDone);
	void OnLevelGeometryChanged();
	void DrawLightProbeDebug(Vec3 vPos, int nTexId);
	void ReconstructTree(bool bMultiPoint);
	void AllocateRootNode();
	int  ExportSvo(ICryArchive* pArchive);
	void LoadTree(int& nNodesCreated);
	void DetectMovement_StaticGeom();
	void DetectMovement_StatLights();
	void CollectLights();
	void CollectAnalyticalOccluders();
	void AddAnalyticalOccluder(IRenderNode* pRN, Vec3 camPos);
	void GetGlobalEnvProbeProperties(_smart_ptr<ITexture>& specEnvCM, float& mult);

	PodArray<I3DEngine::SLightTI>            m_lightsTI_S, m_lightsTI_D;
	PodArray<I3DEngine::SAnalyticalOccluder> m_AnalyticalOccluders[2];
	Vec4                      m_vSvoOriginAndSize;
	AABB                      m_aabbLightsTI_D;
	SRenderLight*             m_pGlobalEnvProbe;
	CSvoNode*                 m_pSvoRoot;
	bool                      m_bReady;
	PodArray<CVoxelSegment*>  m_arrForStreaming[nVoxStreamQueueMaxSize][nVoxStreamQueueMaxSize];
	int                       m_nDebugDrawVoxelsCounter;
	int                       m_nNodeCounter;
	int                       m_nDynNodeCounter;
	int                       m_nDynNodeCounter_DYNL;
	PodArray<CVoxelSegment*>  m_arrForBrickUpdate[16];
	CryCriticalSection        m_csLockTree;
	float                     m_fStreamingStartTime;
	float                     m_fSvoFreezeTime;
	int                       m_arrVoxelizeMeshesCounter[2];
	AABB                      m_worldBox;
	PodArray<SVF_P3F_C4B_T2F> m_arrSvoProxyVertices;
	double                    m_prevCheckVal;
	bool                      m_bFirst_SvoFreezeTime;
	bool                      m_bFirst_StartStreaming;
	bool                      m_bStreamingDonePrev;

	int                       m_nTexOpasPoolId;
	int                       m_nTexNodePoolId;

	#ifdef FEATURE_SVO_GI_USE_MESH_RT
	int                                                m_nTexTrisPoolId;
	#endif
	int                                                m_nTexRgb0PoolId;
	int                                                m_nTexRgb1PoolId;
	int                                                m_nTexDynlPoolId;
	int                                                m_nTexRgb2PoolId;
	int                                                m_nTexRgb3PoolId;
	int                                                m_nTexRgb4PoolId;
	int                                                m_nTexNormPoolId;
	int                                                m_nTexAldiPoolId;

	ETEX_Format                                        m_nVoxTexFormat;
	TDoublyLinkedList<CVoxelSegment>                   m_arrSegForUnload;
	CCustomSVOPoolAllocator<struct SCpuBrickItem, 128> m_cpuBricksAllocator;
	CCustomSVOPoolAllocator<CSvoNode, 128>             m_nodeAllocator;

	#ifdef FEATURE_SVO_GI_USE_MESH_RT
	PodArrayRT<ColorB> m_arrRTPoolTexs;
	PodArrayRT<Vec4>   m_arrRTPoolTris;
	PodArrayRT<ColorB> m_arrRTPoolInds;
	#endif
};

	#pragma pack(pop)

#endif

#endif
