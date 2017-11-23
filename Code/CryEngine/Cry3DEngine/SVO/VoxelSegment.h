// Copyright 2001-2017 Crytek GmbH / Crytek Group. All rights reserved.

#ifndef __VOXELSEGMENT_H__
#define __VOXELSEGMENT_H__

#if defined(FEATURE_SVO_GI)

	#pragma pack(push,4)

//#pragma optimize("",off)

const float SVO_ROOTLESS_PARENT_SIZE = 512;

const int nVoxTexMaxDim = (16);
const int nVoxBloMaxDim = (16);
const int nVoxNodMaxDim = 2;

	#define nAtlasDimMaxXY            (CVoxelSegment::nVoxTexPoolDimXY / nVoxBloMaxDim)
	#define nAtlasDimMaxZ             (CVoxelSegment::nVoxTexPoolDimZ / nVoxBloMaxDim)

	#define nAtlasDimBriXY            (CVoxelSegment::nVoxTexPoolDimXY / nVoxTexMaxDim)
	#define nAtlasDimBriZ             (CVoxelSegment::nVoxTexPoolDimZ / nVoxTexMaxDim)

	#define nVoxNodPoolDimXY          (nVoxNodMaxDim * nAtlasDimMaxXY)
	#define nVoxNodPoolDimZ           (nVoxNodMaxDim * nAtlasDimMaxZ)

	#define VOXEL_PAINT_SAFETY_BORDER 4.f

	#define SVO_TEMP_FILE_NAME        "CDD.TMP"

	#define SVO_MAX_TRIS_PER_VOXEL    512

typedef uint16 T_ObjectLayerId;
static const T_ObjectLayerId InvalidObjectLayerId = 0;
static const T_ObjectLayerId AllObjectLayersId = InvalidObjectLayerId;

struct SVoxData
{
	enum ESubSetType
	{
		OPA3D,
		COLOR,
		NORML,
		MAX_NUM,
	};

	SVoxData() { ZeroStruct(*this); }
	ColorB* pVoxData[MAX_NUM];
};

struct SCpuBrickItem
{
	ColorB arrData[nVoxTexMaxDim * nVoxTexMaxDim * nVoxTexMaxDim];
};

template<class T>
class PodArrayRT : public PodArray<T>
{
public:
	PodArrayRT()
	{
		m_bModified = false;
		m_nTexId = 0;
	}
	CryReadModifyLock m_Lock;
	bool              m_bModified;
	int               m_nTexId;
};

template<class Key, class T>
class CLockedMap : public std::map<Key, T>
{
public:
	CryReadModifyLock m_Lock;
};

struct SObjInfo
{
	SObjInfo() { ZeroStruct(*this); }

	static int32 Compare(const void* v1, const void* v2)
	{
		SObjInfo* p[2] = { (SObjInfo*)v1, (SObjInfo*)v2 };

		if (p[0]->fMaxViewDist < p[1]->fMaxViewDist)
			return 1;
		if (p[0]->fMaxViewDist > p[1]->fMaxViewDist)
			return -1;

		return 0;
	}

	Matrix34        matObjInv;
	Matrix34        matObj;
	float           fObjScale;
	IMaterial*      pMat;
	CStatObj*       pStatObj;
	bool            bIndoor;
	bool            bVegetation;
	T_ObjectLayerId nObjLayerId;
	float           fMaxViewDist;
};

struct SVoxSegmentFileHeader
{
	Vec4_tpl<byte> cropTexSize; // and objLayersNum in w
	Vec4_tpl<byte> cropBoxMin;  // and subSetsNum in w
	Vec4_tpl<byte> dummy;
};

// SSuperMesh index type
	#if CRY_PLATFORM_WINDOWS && CRY_PLATFORM_64BIT
typedef uint32 SMINDEX;
	#else
typedef uint16 SMINDEX;
	#endif

struct SRayHitTriangleIndexed
{
	SRayHitTriangleIndexed() { ZeroStruct(*this); arrVertId[0] = arrVertId[1] = arrVertId[2] = (SMINDEX) ~0; }
	Vec3            vFaceNorm;
	#ifdef FEATURE_SVO_GI_USE_MESH_RT
	uint            nGLobalId;
	#endif
	uint8           nTriArea;
	uint8           nOpacity;
	uint8           nHitObjType;
	SMINDEX         arrVertId[3];
	uint16          nMatID;
	T_ObjectLayerId nObjLayerId;
};

struct SRayHitVertex
{
	Vec3   v;
	Vec2   t;
	ColorB c;
};

struct SSuperMesh
{
	SSuperMesh();
	~SSuperMesh();

	struct SSvoMatInfo
	{
		SSvoMatInfo(){ ZeroStruct(*this); }
		IMaterial* pMat;
		ColorB*    pTexRgb;
		uint16     nTexWidth;
		uint16     nTexHeight;
		inline bool operator==(const SSvoMatInfo& other) const { return pMat == other.pMat; }
	};

	static const int nHashDim = 8;
	void AddSuperTriangle(SRayHitTriangle& htIn, PodArray<SMINDEX> arrVertHash[nHashDim][nHashDim][nHashDim], T_ObjectLayerId nObjLayerId);
	void AddSuperMesh(SSuperMesh& smIn, float fVertexOffset);
	void Clear(PodArray<SMINDEX>* parrVertHash);

	PodArrayRT<SRayHitTriangleIndexed>* m_pTrisInArea;
	PodArrayRT<Vec3>*                   m_pFaceNormals;
	PodArrayRT<SRayHitVertex>*          m_pVertInArea;
	PodArrayRT<SSvoMatInfo>*            m_pMatsInArea;
	AABB                                m_boxTris;

protected:
	int FindVertex(const Vec3& rPos, const Vec2 rTC, PodArray<SMINDEX> arrVertHash[nHashDim][nHashDim][nHashDim], PodArrayRT<SRayHitVertex>& vertsInArea);
	int AddVertex(const SRayHitVertex& rVert, PodArray<SMINDEX> arrVertHash[nHashDim][nHashDim][nHashDim], PodArrayRT<SRayHitVertex>& vertsInArea);
	bool m_bExternalData;
};

struct SBuildVoxelsParams
{
	int                  X0;
	int                  X1;
	PodArray<int>*       pNodeTrisXYZ;
	PodArray<CVisArea*>* arrPortals;
	PodArray<int>*       pTrisInt;
};

class CVoxelSegment : public Cry3DEngineBase, public SSuperMesh, public IStreamCallback
{
public:

	CVoxelSegment(class CSvoNode* pNode, bool bDumpToDiskInUse = false, EFileStreamingStatus eStreamingStatus = ecss_Ready, bool bDroppedOnDisk = false);
	~CVoxelSegment();
	static int   GetSubSetsNum() { return GetCVars()->e_svoTI_IntegrationMode ? SVoxData::MAX_NUM : 1;  }
	bool         CheckUpdateBrickRenderData(bool bJustCheck);
	bool         LoadVoxels(byte* pData, int nSize);
	void         SaveVoxels(PodArray<byte>& arrData);
	bool         StartStreaming();
	bool         UpdateBrickRenderData();
	ColorF       GetBilinearAt(float iniX, float iniY, const ColorB* pImg, int nDimW, int nDimH, float fBr);
	ColorF       GetColorF_255(int x, int y, const ColorB* pImg, int nImgSizeW, int nImgSizeH);
	ColorF       ProcessMaterial(const SRayHitTriangleIndexed& tr, const Vec3& voxBox);
	const AABB&  GetBoxOS()   { return m_boxOS; }
	float        GetBoxSize() { return (m_boxOS.max.z - m_boxOS.min.z); }
	int          StoreIndicesIntoPool(const PodArray<int>& nodeTInd, int& nCountStored);
	int32        GetID()      { return m_nSegID; }
	static AABB  GetChildBBox(const AABB& parentBox, int nChildId);
	static int   GetBrickPoolUsageLoadedMB();
	static int   GetBrickPoolUsageMB();
	static int32 ComparemLastVisFrameID(const void* v1, const void* v2);
	static void  CheckAllocateBrick(ColorB*& pPtr, int nElems, bool bClean = false);
	static void  CheckAllocateTexturePool();
	static void  FreeBrick(ColorB*& pPtr);
	static void  MakeFolderName(char szFolder[256], bool bCreateDirectory = false);
	static void  SetVoxCamera(const CCamera& newCam);
	static void  UpdateStreamingEngine();
	static void  UpdateObjectLayersInfo();
	static void  ErrorTerminate(const char* format, ...);
	Vec3i        GetDxtDim();
	void         AddTriangle(const SRayHitTriangleIndexed& ht, int trId, PodArray<int>*& rpNodeTrisXYZ, PodArrayRT<SRayHitVertex>* pVertInArea);
	void         CheckStoreTextureInPool(SShaderItem* pShItem, uint16& nTexW, uint16& nTexH, int** ppSysTexId);
	void         ComputeDistancesFast_MinDistToSurf(ColorB* pTex3dOptRGBA, ColorB* pTex3dOptNorm, ColorB* pTex3dOptOpac, int nTID);
	void         CropVoxTexture(int nTID, bool bCompSurfDist);
	void         DebugDrawVoxels();
	void         FindTrianglesForVoxelization(int nTID, PodArray<int>*& rpNodeTrisXYZ, bool bThisIsAreaParent);
	static bool  CheckCollectObjectsForVoxelization(const AABB& nodeBox, PodArray<SObjInfo>* parrObjects, bool& bThisIsAreaParent, bool& bThisIsLowLodNode, bool bAllowStartStreaming);
	void         FreeAllBrickData();
	void         FreeBrickLayers();
	void         FreeRenderData();
	void         PropagateDirtyFlag();
	void         ReleaseAtlasBlock();
	void         RenderMesh(PodArray<SVF_P3F_C4B_T2F>& arrVertsOut);
	void         SetBoxOS(const AABB& box) { m_boxOS = box; }
	void         SetID(int32 nID)          { m_nSegID = nID; }
	void         StoreAreaTrisIntoTriPool(PodArray<SRayHitTriangle>& allTrisInLevel);
	void         StreamAsyncOnComplete(IReadStream* pStream, unsigned nError) override;
	void         StreamOnComplete(IReadStream* pStream, unsigned nError) override;
	void         UnloadStreamableData();
	void         UpdateMeshRenderData();
	void         UpdateNodeRenderData();
	void         UpdateVoxRenderData();
	void         VoxelizeMeshes(int nTID, bool bMT = false);
	void         CombineLayers();
	void         BuildVoxels(SBuildVoxelsParams params);
	int          CompressToDxt(ColorB* pImgSource, byte*& pDxtOut, int nTID);
	static void  SaveCompTexture(const void* data, size_t size, void* userData);

	AABB                                                 m_boxClipped;
	AABB                                                 m_boxOS;
	bool                                                 m_bStatLightsChanged;
	byte                                                 m_nChildOffsetsDirty;
	class CSvoNode*                                      m_pNode;
	SVoxData                                             m_voxData;
	CVoxelSegment*                                       m_pParentCloud;
	EFileStreamingStatus                                 m_eStreamingStatus;
	float                                                m_fMaxAlphaInBrick;
	int                                                  m_nAllocatedAtlasOffset;
	int                                                  m_nFileStreamSize;
	int32                                                m_arrChildOffset[8];
	int32                                                m_nSegID;
	int64                                                m_nFileStreamOffset64;
	PodArray<int>                                        m_nodeTrisAllMerged;

	static CCamera                                       m_voxCam;
	static class CBlockPacker3D*                         m_pBlockPacker;
	static CryCriticalSection                            m_csLockBrick;
	static int                                           m_nAddPolygonToSceneCounter;
	static int                                           m_nCheckReadyCounter;
	static int                                           m_nCloudsCounter;
	static int                                           m_nPostponedCounter;
	static int                                           m_nCurrPassMainFrameID;
	static int                                           m_nMaxBrickUpdates;
	static int                                           m_nNextCloudId;
	static int                                           m_nPoolUsageBytes;
	static int                                           m_nPoolUsageItems;
	static int                                           m_nSvoDataPoolsCounter;
	static int                                           m_nVoxTrisCounter;
	static int                                           nVoxTexPoolDimXY;
	static int                                           nVoxTexPoolDimZ;
	static int32                                         m_nStreamingTasksInProgress;
	static int32                                         m_nTasksInProgressALL;
	static int32                                         m_nUpdatesInProgressBri;
	static int32                                         m_nUpdatesInProgressTex;
	static PodArray<CVoxelSegment*>                      m_arrLoadedSegments;
	static SRenderingPassInfo*                           m_pCurrPassInfo;
	static CLockedMap<ITexture*, _smart_ptr<ITexture>>   m_arrLockedTextures;
	static CLockedMap<IMaterial*, _smart_ptr<IMaterial>> m_arrLockedMaterials;
	static std::map<CStatObj*, float>                    m_cgfTimeStats;
	static CryReadModifyLock                             m_cgfTimeStatsLock;
	static bool                                          m_bExportMode;
	static int                                           m_nExportVisitedAreasCounter;
	static PodArray<C3DEngine::SLayerActivityInfo>       m_arrObjectLayersInfo;
	static uint                                          m_arrObjectLayersInfoVersion;
	struct SBlockMinMax*                                 m_pBlockInfo;
	SVF_P3F_C4B_T2F                                      m_vertForGS;
	uint                                                 m_nLastRendFrameId;
	uint                                                 m_nLastTexUpdateFrameId;
	uint16                                               m_nVoxNum;
	uint8                                                m_dwChildTrisTest;
	Vec3i                                                m_vCropBoxMin;
	Vec3                                                 m_vSegOrigin;
	Vec3i                                                m_vCropTexSize;
	Vec3i                                                m_vStaticGeomCheckSumm;
	Vec3i                                                m_vStatLightsCheckSumm;
	CryReadModifyLock                                    m_superMeshLock;
	std::map<T_ObjectLayerId, SVoxData>                  m_objLayerMap;
};

template<class T, int nMaxQeueSize>
struct SThreadSafeArray
{
	SThreadSafeArray()
	{
		m_bThreadDone = false;
		m_nMaxQeueSize = nMaxQeueSize;
		m_ucOverflow = 0;
		m_nRequestFrameId = 0;
	}

	bool               m_bThreadDone;
	int                m_nMaxQeueSize;
	PodArray<T>        m_arrQeue;
	CryCriticalSection m_csQeue;
	unsigned char      m_ucOverflow;
	int                m_nRequestFrameId;

	T                  GetNextTaskFromQeue()
	{
		T pNewTask = NULL;

		if (m_arrQeue.Count())
		{
			AUTO_LOCK(m_csQeue);
			if (m_arrQeue.Count())
			{
				pNewTask = m_arrQeue[0];
				m_arrQeue.Delete((const int)0);
			}
		}

		return pNewTask;
	}

	T GetNextTaskFromQeueOrdered2()
	{
		T pNewTask = NULL;

		if (m_arrQeue.Count())
		{
			AUTO_LOCK(m_csQeue);
			if (m_arrQeue.Count())
			{
				for (int i = 0; i < m_arrQeue.Count(); i++)
				{
					if (m_arrQeue[i].nRequestFrameId == m_nRequestFrameId)
					{
						pNewTask = m_arrQeue[i];
						m_arrQeue.Delete((const int)i);
						m_nRequestFrameId++;
						break;
					}
				}
			}
		}

		return pNewTask;
	}

	T GetNextTaskFromQeueOrdered()
	{
		T pNewTask = NULL;

		if (m_arrQeue.Count())
		{
			AUTO_LOCK(m_csQeue);
			if (m_arrQeue.Count())
			{
				for (int i = 0; i < m_arrQeue.Count(); i++)
				{
					if (m_arrQeue[i]->pIBackBufferReader->nRequestFrameId == m_nRequestFrameId)
					{
						pNewTask = m_arrQeue[i];
						m_arrQeue.Delete((const int)i);
						m_nRequestFrameId++;
						break;
					}
				}
			}
		}

		return pNewTask;
	}

	T GetNextTaskFromQeueOrderedForThread(int nCTID)
	{
		T pNewTask = NULL;

		if (m_arrQeue.Count())
		{
			AUTO_LOCK(m_csQeue);
			if (m_arrQeue.Count())
			{
				for (int i = 0; i < m_arrQeue.Count(); i++)
				{
					if (m_arrQeue[i]->pIBackBufferReader->nRequestFrameId == m_nRequestFrameId && ((m_arrQeue[i]->pIBackBufferReader->nUserId & 1) == (nCTID & 1)))
					{
						pNewTask = m_arrQeue[i];
						m_arrQeue.Delete((const int)i);
						m_nRequestFrameId++;
						break;
					}
				}
			}
		}

		return pNewTask;
	}

	void AddNewTaskToQeue(T pNewTask, bool bSkipOverflow = false)
	{
		if (m_ucOverflow)
			m_ucOverflow--;

		if (bSkipOverflow)
		{
			// drop new item in case of overflow
			if (m_arrQeue.Count() >= m_nMaxQeueSize)
			{
				m_ucOverflow = ~0;
				return;
			}
		}
		else
		{
			// stall in case of overflow
			while (m_arrQeue.Count() >= m_nMaxQeueSize)
			{
				m_ucOverflow = ~0;
				CrySleep(1);
			}
		}

		{
			AUTO_LOCK(m_csQeue);
			m_arrQeue.Add(pNewTask);
		}
	}

	int Count()
	{
		return m_arrQeue.Count();
	}

	void OnDisplayInfo(float& fTextPosX, float& fTextPosY, float& fTextStepY, float fTextScale, C3DEngine* pEnd, const char* szName)
	{
		ColorF colRed = ColorF(1, 0, 0, 1);
		ColorF colGreen = ColorF(0, 1, 0, 1);
		pEnd->DrawTextRightAligned(fTextPosX, fTextPosY += fTextStepY, fTextScale, m_ucOverflow ? Col_Red : Col_Green, "%s: %2d", szName, Count());
	}
};

inline uint GetCurrPassMainFrameID() { return CVoxelSegment::m_nCurrPassMainFrameID; }

	#pragma pack(pop)

#endif

#endif
