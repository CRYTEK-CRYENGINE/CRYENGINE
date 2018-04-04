// Copyright 2001-2017 Crytek GmbH / Crytek Group. All rights reserved. 

// -------------------------------------------------------------------------
//  File name:   VoxelSegment.cpp
//  Created:     2012 by Vladimir Kajalin.
//  Description: SVO brick implementation
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////

#include "StdAfx.h"

#if defined(FEATURE_SVO_GI)

	#include <CryCore/Platform/CryWindows.h>
	#include "VoxelSegment.h"
	#include "BlockPacker.h"
	#include "visareas.h"
	#include "brush.h"
	#include "SceneTree.h"
	#include <CryThreading/IJobManager_JobDelegator.h>
	#include <CryAnimation/ICryAnimation.h>

	#define SVO_CPU_VOXELIZATION_OFFSET_MESH    0
	#define SVO_CPU_VOXELIZATION_OFFSET_TERRAIN -0.04f
	#define SVO_CPU_VOXELIZATION_OFFSET_VISAREA (Cry3DEngineBase::GetCVars()->e_svoMinNodeSize / (float)nVoxTexMaxDim)
	#define SVO_CPU_VOXELIZATION_POOL_SIZE_MB   (12 * 1024)
	#define SVO_CPU_VOXELIZATION_AREA_SCALE     200.f

CBlockPacker3D* CVoxelSegment::m_pBlockPacker = 0;
CCamera CVoxelSegment::m_voxCam;
const int nDistToSurfRange = 4;
extern CSvoEnv* gSvoEnv;
int CVoxelSegment::m_nAddPolygonToSceneCounter = 0;
int CVoxelSegment::m_nCheckReadyCounter = 0;
int CVoxelSegment::m_nCloudsCounter = 0;
int CVoxelSegment::m_nPostponedCounter = 0;
int CVoxelSegment::m_nCurrPassMainFrameID = 0;
int CVoxelSegment::m_nMaxBrickUpdates = 128;
int CVoxelSegment::m_nNextCloudId = 0;
int CVoxelSegment::m_nPoolUsageBytes = 0;
int CVoxelSegment::m_nPoolUsageItems = 0;
int CVoxelSegment::m_nStreamingTasksInProgress = 0;
int CVoxelSegment::m_nSvoDataPoolsCounter = 0;
int CVoxelSegment::m_nTasksInProgressALL = 0;
int CVoxelSegment::m_nUpdatesInProgressBri = 0;
int CVoxelSegment::m_nUpdatesInProgressTex = 0;
int CVoxelSegment::m_nVoxTrisCounter = 0;
int CVoxelSegment::nVoxTexPoolDimXY = 0;
int CVoxelSegment::nVoxTexPoolDimZ = 0;
std::map<CStatObj*, float> CVoxelSegment::m_cgfTimeStats;
CryReadModifyLock CVoxelSegment::m_cgfTimeStatsLock;

PodArrayRT<ITexture*> CVoxelSegment::m_arrLockedTextures;
PodArrayRT<IMaterial*> CVoxelSegment::m_arrLockedMaterials;

PodArray<CVoxelSegment*> CVoxelSegment::m_arrLoadedSegments;
SRenderingPassInfo* CVoxelSegment::m_pCurrPassInfo = 0;

#if CRY_PLATFORM_ORBIS
int   _fseeki64(FILE* pFile, int64 nOffset, int nOrigin) { return fseek(pFile, (int32)nOffset, nOrigin); }
int64 _ftelli64(FILE* pFile)                             { return ftell(pFile); }
#endif

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Streaming engine
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

class CVoxStreamEngine
{
public:

	struct SVoxStreamItem
	{
		static int32 Compare(const void* v1, const void* v2)
		{
			SVoxStreamItem* p[2] = { *(SVoxStreamItem**)v1, *(SVoxStreamItem**)v2 };

			if (p[0]->nRequestFrameId > p[1]->nRequestFrameId)
				return 1;
			if (p[0]->nRequestFrameId < p[1]->nRequestFrameId)
				return -1;

	#ifdef FEATURE_SVO_GI_USE_FILE_STREAMING
			if (p[0]->nFileOffset > p[1]->nFileOffset)
				return 1;
			if (p[0]->nFileOffset < p[1]->nFileOffset)
				return -1;
	#endif
			return 0;
		}

		CVoxelSegment* pObj;
	#ifdef FEATURE_SVO_GI_USE_FILE_STREAMING
		int64          nFileOffset;
		int            nBytesToRead;
		byte*          pReadData;
	#endif
		uint           nRequestFrameId;
	};

	void        ThreadEntry();

  #if CRY_PLATFORM_ORBIS
	static void* FileReadThreadFunc(void* pUD)
	{
		CVoxStreamEngine* pEng = (CVoxStreamEngine*)pUD;
		pEng->ThreadEntry();
		return NULL;
	}
  #else
	static void FileReadThreadFunc(void* pUD)
	{
		CVoxStreamEngine* pEng = (CVoxStreamEngine*)pUD;
		pEng->ThreadEntry();
	}
  #endif

	SThreadSafeArray<SVoxStreamItem*, 512> m_arrForFileRead;
	SThreadSafeArray<SVoxStreamItem*, 512> m_arrForSyncCallBack;
	#ifdef FEATURE_SVO_GI_USE_FILE_STREAMING
	byte* m_pMemPool;
	int   m_nPoolSize;
	int   m_nPoolPos;
	FILE* m_pFile;
	#endif
	bool  m_bDoResort;

	CVoxStreamEngine(string strFileName, int nPoolSize)
	{
		m_bDoResort = false;
	#ifdef FEATURE_SVO_GI_USE_FILE_STREAMING
		m_nPoolSize = nPoolSize;
		m_pMemPool = NULL;
		m_nPoolPos = 0;
		m_pFile = fopen(strFileName, "rb");
		if (m_pFile || Cry3DEngineBase::GetCVars()->e_svoTI_Active)
	#endif
		{
			//			const char * szThreadName = "SvoStreamEngine";
			//      if(!gEnv->pThreadManager->SpawnThread(this, szThreadName))
			//      {
			//        CRY_ASSERT_MESSAGE(false, string().Format("Error spawning \"%s\" thread.", szThreadName).c_str());
			//        __debugbreak();
			//      }
			//			_beginthread(FileReadThreadFunc,0,(void*)this);

	#if CRY_PLATFORM_DURANGO
			void* m_thread;
			uint32 m_threadId;

			m_threadId = 0;
			m_thread = (void*)CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)FileReadThreadFunc, (void*)this, 0, (LPDWORD)&m_threadId);
			SetThreadPriority(m_thread, THREAD_PRIORITY_BELOW_NORMAL);
			if (Cry3DEngineBase::GetCVars()->e_svoTI_ThreadAffinity0 >= 0)
			{
				SetThreadAffinityMask(m_thread, BIT64(Cry3DEngineBase::GetCVars()->e_svoTI_ThreadAffinity0));
			}

			m_threadId = 0;
			m_thread = (void*)CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)FileReadThreadFunc, (void*)this, 0, (LPDWORD)&m_threadId);
			SetThreadPriority(m_thread, THREAD_PRIORITY_BELOW_NORMAL);
			if (Cry3DEngineBase::GetCVars()->e_svoTI_ThreadAffinity1 >= 0)
			{
				SetThreadAffinityMask(m_thread, BIT64(Cry3DEngineBase::GetCVars()->e_svoTI_ThreadAffinity1));
			}
	#elif CRY_PLATFORM_ORBIS
			ScePthread m_thread0;
			::scePthreadCreate(&m_thread0, nullptr, FileReadThreadFunc, (void*)this, "VoxelThread_0");
			::scePthreadSetprio(m_thread0, THREAD_PRIORITY_BELOW_NORMAL);
			if (Cry3DEngineBase::GetCVars()->e_svoTI_ThreadAffinity0 >= 0)
			{
				::scePthreadSetaffinity(m_thread0, BIT64(Cry3DEngineBase::GetCVars()->e_svoTI_ThreadAffinity0));
			}

			ScePthread m_thread1;
			::scePthreadCreate(&m_thread1, nullptr, FileReadThreadFunc, (void*)this, "VoxelThread_1");
			::scePthreadSetprio(m_thread1, THREAD_PRIORITY_BELOW_NORMAL);
			if (Cry3DEngineBase::GetCVars()->e_svoTI_ThreadAffinity1 >= 0)
			{
				::scePthreadSetaffinity(m_thread1, BIT64(Cry3DEngineBase::GetCVars()->e_svoTI_ThreadAffinity1));
			}
	#else
			_beginthread(FileReadThreadFunc, 0, (void*)this);
			_beginthread(FileReadThreadFunc, 0, (void*)this);
	#endif
		}
	}

	~CVoxStreamEngine()
	{
		m_arrForFileRead.m_bThreadDone = true;

		while (m_arrForFileRead.m_bThreadDone || CVoxelSegment::m_nTasksInProgressALL)
		{
			CrySleep(1);
		}
	}

	void DecompressVoxStreamItem(SVoxStreamItem item)
	{
	#ifdef FEATURE_SVO_GI_USE_FILE_STREAMING
		item.pObj->StreamAsyncOnComplete(item.pReadData, item.nBytesToRead, (int)0);
	#else
		item.pObj->StreamAsyncOnComplete(0, 0, 0);
	#endif
		SVoxStreamItem* pNewItem = new SVoxStreamItem;
		*pNewItem = item;

		m_arrForSyncCallBack.AddNewTaskToQeue(pNewItem);
	}

	void ProcessSyncCallBacks()
	{
		while (SVoxStreamItem* pItem = m_arrForSyncCallBack.GetNextTaskFromQeue())
		{
			pItem->pObj->StreamOnComplete();
			delete pItem;
			CVoxelSegment::m_nTasksInProgressALL--;
		}
	}

	bool StartRead(CVoxelSegment* pObj, int64 nFileOffset, int nBytesToRead);

}* pVoxStreamEngine = 0;

DECLARE_JOB("VoxelSegmentFileDecompress", TDecompressVoxStreamItemJob, CVoxStreamEngine::DecompressVoxStreamItem);

void CVoxStreamEngine::ThreadEntry()
{
	MEMSTAT_CONTEXT(EMemStatContextTypes::MSC_Other, 0, "VoxStreamEngine");

	while (1)
	{
		if (m_bDoResort)
		{
			AUTO_LOCK(m_arrForFileRead.m_csQeue);
			qsort(m_arrForFileRead.m_arrQeue.GetElements(), m_arrForFileRead.m_arrQeue.Count(), sizeof(SVoxStreamItem*), SVoxStreamItem::Compare);
			m_bDoResort = false;
		}

		if (SVoxStreamItem* pItem = m_arrForFileRead.GetNextTaskFromQeue())
		{
	#ifdef FEATURE_SVO_GI_USE_FILE_STREAMING
			if (pItem->nBytesToRead > 0)
			{
				// load data
				_fseeki64(m_pFile, pItem->nFileOffset, SEEK_SET);

				if (m_nPoolPos + pItem->nBytesToRead >= m_nPoolSize)
				{
					m_nPoolPos = 0;
					// Cry3DEngineBase::PrintMessage("Pool restart");
				}

				if (!m_pMemPool)
					m_pMemPool = new byte[m_nPoolSize];

				pItem->pReadData = &m_pMemPool[m_nPoolPos];
				m_nPoolPos += pItem->nBytesToRead;

				if (pItem->nBytesToRead != ::fread(pItem->pReadData, 1, pItem->nBytesToRead, m_pFile))
					CVoxelSegment::ErrorTerminate("FileReadThreadFunc, fread");
			}
	#endif
			if (Cry3DEngineBase::GetCVars()->e_svoMaxStreamRequests > 4)
			{
				TDecompressVoxStreamItemJob job(*pItem);
				job.SetClassInstance(this);
				job.SetPriorityLevel(JobManager::eStreamPriority);
				job.Run();
			}
			else
			{
				DecompressVoxStreamItem(*pItem);
				// todo: wait in loop until all done, don't uase jobs
			}

			delete pItem;
		}
		else
		{
			if (m_arrForFileRead.m_bThreadDone && !CVoxelSegment::m_nTasksInProgressALL)
				break;

			CrySleep(1);
		}
	}

	m_arrForFileRead.m_bThreadDone = false;
}

bool CVoxStreamEngine::StartRead(CVoxelSegment* pObj, int64 nFileOffset, int nBytesToRead)
{
	if (m_arrForFileRead.Count() >= m_arrForFileRead.m_nMaxQeueSize)
		return false;

	FUNCTION_PROFILER_3DENGINE;

	SVoxStreamItem* pNewItem = new SVoxStreamItem;
	pNewItem->pObj = pObj;
	#ifdef FEATURE_SVO_GI_USE_FILE_STREAMING
	pNewItem->nFileOffset = nFileOffset;
	pNewItem->nBytesToRead = nBytesToRead;
	pNewItem->pReadData = 0;
	#endif
	pNewItem->nRequestFrameId = GetCurrPassMainFrameID() / 10;

	CVoxelSegment::m_nTasksInProgressALL++;

	m_arrForFileRead.AddNewTaskToQeue(pNewItem);

	m_bDoResort = true;

	return true;
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Voxel Segment
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

CryCriticalSection CVoxelSegment::m_csLockBrick;

CVoxelSegment::CVoxelSegment(class CSvoNode* pNode, bool bDumpToDiskInUse, EFileStreamingStatus eStreamingStatus, bool bDroppedOnDisk)
{
	m_boxTris.Reset();
	m_boxOS.Reset();
	m_boxClipped.Reset();
	m_dwChildTrisTest = 0;
	m_eStreamingStatus = eStreamingStatus;
	m_fMaxAlphaInBrick = 0;
	m_nAllocatedAtlasOffset = -2;
	m_nLastTexUpdateFrameId = m_nLastRendFrameId = 0;
	m_nFileStreamOffset64 = m_nFileStreamSize = -1;
	m_nChildOffsetsDirty = 0;
	m_nCloudsCounter++;
	m_nSegID = -1;
	m_bStatLightsChanged = 0;
	m_nVoxNum = 0;
	m_pBlockInfo = 0;
	m_pNode = pNode;
	m_pParentCloud = 0;
	m_pVoxOpacit = 0;
	#ifdef FEATURE_SVO_GI_ALLOW_HQ
	m_pVoxVolume = m_pVoxNormal = 0;
		#ifdef FEATURE_SVO_GI_USE_MESH_RT
	m_pVoxTris = 0;
		#endif
	#endif
	m_vCropBoxMin.zero();
	m_vCropTexSize.zero();
	m_vSegOrigin = Vec3(0, 0, 0);
	m_vStaticGeomCheckSumm.zero();
	m_vStatLightsCheckSumm.zero();
	ZeroStruct(m_arrChildOffset);
}

int32 CVoxelSegment::ComparemLastVisFrameID(const void* v1, const void* v2)
{
	CVoxelSegment* p[2] = { *(CVoxelSegment**)v1, *(CVoxelSegment**)v2 };

	uint arrNodeSize[2] =
	{
		uint((p[0]->m_boxOS.max.x - p[0]->m_boxOS.min.x) * 4),
		uint((p[1]->m_boxOS.max.x - p[1]->m_boxOS.min.x) * 4)
	};

	if ((p[0]->m_nLastRendFrameId + arrNodeSize[0]) > (p[1]->m_nLastRendFrameId + arrNodeSize[1]))
		return 1;
	if ((p[0]->m_nLastRendFrameId + arrNodeSize[0]) < (p[1]->m_nLastRendFrameId + arrNodeSize[1]))
		return -1;

	if (p[0] > p[1])
		return 1;
	if (p[0] < p[1])
		return -1;

	return 0;
}

int CVoxelSegment::GetBrickPoolUsageMB()
{
	return gSvoEnv->m_cpuBricksAllocator.GetCapacity() * nVoxTexMaxDim * nVoxTexMaxDim * nVoxTexMaxDim * sizeof(ColorB) / 1024 / 1024;
}

int CVoxelSegment::GetBrickPoolUsageLoadedMB()
{
	return gSvoEnv->m_cpuBricksAllocator.GetCount() * nVoxTexMaxDim * nVoxTexMaxDim * nVoxTexMaxDim * sizeof(ColorB) / 1024 / 1024;
}

void CVoxelSegment::CheckAllocateBrick(ColorB*& pPtr, int nElems, bool bClean)
{
	if (pPtr)
	{
		if (bClean)
			memset(pPtr, 0, nVoxTexMaxDim * nVoxTexMaxDim * nVoxTexMaxDim * sizeof(ColorB));

		return;
	}

	{
		AUTO_LOCK(m_csLockBrick);

		pPtr = (ColorB*)gSvoEnv->m_cpuBricksAllocator.GetNewElement();
	}

	memset(pPtr, 0, nVoxTexMaxDim * nVoxTexMaxDim * nVoxTexMaxDim * sizeof(ColorB));
}

void CVoxelSegment::FreeBrick(ColorB*& pPtr)
{
	if (pPtr)
	{
		AUTO_LOCK(m_csLockBrick);

		gSvoEnv->m_cpuBricksAllocator.ReleaseElement((SCpuBrickItem*)pPtr);

		pPtr = 0;
	}
}

int GetBrickDataSize(ColorB*& pPtr)
{
	return pPtr ? nVoxTexMaxDim * nVoxTexMaxDim * nVoxTexMaxDim * sizeof(ColorB) : 0;
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// CVoxelSegment
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

CVoxelSegment::~CVoxelSegment()
{
	m_arrLoadedSegments.Delete(this);
	m_nCloudsCounter--;
	FreeAllBrickData();
	FreeRenderData();
}

int32 IntersectPlaneAABB(const Plane& plane, const AABB& aabb)
{
	Vec3 extents = aabb.GetSize() * 0.5f;

	f32 d1 = plane.DistFromPlane(aabb.GetCenter());

	f32 d2 =
	  fabs(extents.x * plane.n.Dot(Vec3(1, 0, 0))) +
	  fabs(extents.y * plane.n.Dot(Vec3(0, 1, 0))) +
	  fabs(extents.z * plane.n.Dot(Vec3(0, 0, 1)));

	if (d1 - d2 > FLT_EPSILON)
		return +1;
	if (d1 + d2 < -FLT_EPSILON)
		return -1;

	return 0;
}

void CVoxelSegment::RenderMesh(CRenderObject* pObj, PodArray<SVF_P3F_C4B_T2F>& arrVertsOut)
{
	m_nLastRendFrameId = GetCurrPassMainFrameID();

	if (m_eStreamingStatus == ecss_Ready)
	{
		if (!CheckUpdateBrickRenderData(true))
			return;

		// accumulate DVR proxy mesh
		if ((GetCVars()->e_svoDVR == 10 && m_fMaxAlphaInBrick > 0.05) || GetCVars()->e_svoTI_Active)
			if (GetBoxSize() <= Cry3DEngineBase::GetCVars()->e_svoMaxNodeSize)
			{
				arrVertsOut.Add(m_vertForGS);
				m_nAddPolygonToSceneCounter++;
			}

		DebugDrawVoxels();
	}
}

bool CVoxelSegment::LoadFromMem(CMemoryBlock* pMB)
{
	byte* pData = (byte*)pMB->GetData();
	SVoxSegmentFileHeader* pHeader = (SVoxSegmentFileHeader*)pData;
	m_nSegID = pHeader->nSecId;
	m_boxOS = pHeader->box;
	//m_vAverNormal = pHeader->vAverNormal;

	pData += sizeof(SVoxSegmentFileHeader);

	FreeAllBrickData();

	m_vCropTexSize = pHeader->vCropTexSize;
	m_vCropBoxMin = pHeader->vCropBoxMin;

	int nTexDataSize = (m_vCropTexSize.x * m_vCropTexSize.y * m_vCropTexSize.z * sizeof(ColorB));

	if (gSvoEnv->m_nVoxTexFormat == eTF_BC3)
	{
		Vec3i vDxtDim = GetDxtDim();
		nTexDataSize = (vDxtDim.x * vDxtDim.y * vDxtDim.z * sizeof(ColorB)) / 4;
	}

	if (int nDataSize = nTexDataSize)
	{
	#ifdef FEATURE_SVO_GI_ALLOW_HQ
		// loads RGB
		CheckAllocateBrick(m_pVoxVolume, nDataSize / sizeof(ColorB));
		memcpy(m_pVoxVolume, pData, nDataSize);
		pData += nDataSize;

		// load normals
		CheckAllocateBrick(m_pVoxNormal, nDataSize / sizeof(ColorB));
		memcpy(m_pVoxNormal, pData, nDataSize);
		pData += nDataSize;
	#endif

		CheckAllocateBrick(m_pVoxOpacit, nDataSize / sizeof(ColorB));
		memcpy(m_pVoxOpacit, pData, nDataSize);
		pData += nDataSize;

		if (gSvoEnv->m_nVoxTexFormat == eTF_R8G8B8A8)
		{
			uint8 nMaxAlphaInBrick = 0;

			int nPixNum = m_vCropTexSize.x * m_vCropTexSize.y * m_vCropTexSize.z;
			for (int i = 0; i < nPixNum; i++)
			{
	#ifdef FEATURE_SVO_GI_ALLOW_HQ
				ColorB& col = m_pVoxVolume[i];
				std::swap(col.r, col.b);
				nMaxAlphaInBrick = max(nMaxAlphaInBrick, col.a);

				ColorB& nor = m_pVoxNormal[i];
				std::swap(nor.r, nor.b);
	#endif
				ColorB& op = m_pVoxOpacit[i];
				std::swap(op.r, op.b);
			}

			m_fMaxAlphaInBrick = 1.f / 255.f * (float)nMaxAlphaInBrick;
		}
	}

	return true;
}

Vec3i CVoxelSegment::GetDxtDim()
{
	if (m_vCropTexSize.x * m_vCropTexSize.y * m_vCropTexSize.z == 0)
		return Vec3i(0, 0, 0);

	Vec3i vDxtSize = m_vCropTexSize;

	// adjust X and Y sizes for DXT
	for (int nC = 0; nC < 2; nC++)
	{
		while (1)
		{
			if (((vDxtSize[nC]) % 4) == 0)
				break;

			vDxtSize[nC]++;
		}
	}

	return vDxtSize;
}

void CVoxelSegment::FreeAllBrickData()
{
	#ifdef FEATURE_SVO_GI_ALLOW_HQ
		#ifdef FEATURE_SVO_GI_USE_MESH_RT
	FreeBrick(m_pVoxTris);
		#endif
	FreeBrick(m_pVoxNormal);
	FreeBrick(m_pVoxVolume);
	#endif
	FreeBrick(m_pVoxOpacit);

	m_nodeTrisAllMerged.Reset();
	m_boxTris.Reset();

	if (m_pTrisInArea && (!m_pParentCloud || !m_pParentCloud->m_pTrisInArea))
	{
		m_pTrisInArea->Reset();
		m_pVertInArea->Reset();
		m_pMatsInArea->Reset();
	}
}

void CVoxelSegment::FreeRenderData()
{
	FreeAllBrickData();

	ReleaseAtlasBlock();

	m_eStreamingStatus = ecss_NotLoaded;
}

void CVoxelSegment::MakeFolderName(char szFolder[256], bool bCreateDirectory)
{
	char szLevelFolder[MAX_PATH_LENGTH];
	cry_strcpy(szLevelFolder, Cry3DEngineBase::Get3DEngine()->GetLevelFolder());
	cry_strcat(szLevelFolder, "SvoData");
	cry_sprintf(szFolder, 256, "%s/", szLevelFolder);
}

string CVoxelSegment::m_strRenderDataFileName = "";

void CVoxelSegment::StreamAsyncOnComplete(byte* pDataRead, int nBytesRead, int nTID)
{
	FUNCTION_PROFILER_3DENGINE;

	if (m_nFileStreamSize < 0)
	{
		VoxelizeMeshes(nTID);
		return;
	}

	byte* pData = (byte*)pDataRead;
	int nCompressedSize = *(int*)pData;
	pData += 4;

	if (nCompressedSize)
	{
		if ((nCompressedSize < 0) || (nCompressedSize > nVoxTexMaxDim * nVoxTexMaxDim * nVoxTexMaxDim * 4 * 4))
			ErrorTerminate("%s: Data corruption detected", __FUNCTION__);

		CMemoryBlock mbComp;
		mbComp.Allocate(nCompressedSize);
		memcpy(mbComp.GetData(), pData, mbComp.GetSize());
		pData += mbComp.GetSize();

		CMemoryBlock* pUnpacked = CMemoryBlock::DecompressFromMemBlock(&mbComp, GetSystem());
		if (!pUnpacked || !pUnpacked->GetSize())
			ErrorTerminate("%s: DecompressFromMemBlock error %s", __FUNCTION__, m_strRenderDataFileName.c_str());
		else
			LoadFromMem(pUnpacked);

		delete pUnpacked;
	}

	if (pData - (byte*)pDataRead != nBytesRead)
		PrintMessage("Error reading mesh from %s, %.1f", m_strRenderDataFileName.c_str(), float(nBytesRead) / 1024.f / 1024.f);
	//	else
	//	PrintMessage("Loaded mesh from %s, %.1f", m_strRenderDataFileName.c_str(), float(nBytesRead)/1024.f/1024.f);
}

void CVoxelSegment::StreamOnComplete()
{
	FUNCTION_PROFILER_3DENGINE;

	m_eStreamingStatus = ecss_Ready;

	m_nStreamingTasksInProgress--;

	if (m_arrLoadedSegments.Find(this) < 0)
		m_arrLoadedSegments.Add(this);

	if (m_vCropTexSize.GetVolume() == 0 || !m_pVoxOpacit)
	{
		if (GetBoxSize() <= GetCVars()->e_svoMaxNodeSize && m_pParentCloud && !Cry3DEngineBase::GetCVars()->e_svoTI_Troposphere_Subdivide)
		{
			CSvoNode** ppChilds = m_pParentCloud->m_pNode->m_ppChilds;

			for (int nChildId = 0; nChildId < 8; nChildId++)
			{
				if (ppChilds[nChildId] == m_pNode)
				{
					m_pParentCloud->m_pNode->m_arrChildNotNeeded[nChildId] = true;
				}
			}
		}
	}
}

void CVoxelSegment::UnloadStreamableData()
{
	FreeRenderData();

	m_arrLoadedSegments.Delete(this);

	m_eStreamingStatus = ecss_NotLoaded;

	//	char szFileName[256];
	//MakeFileName("CCM", szFileName);

	//	PrintMessage("Unloaded mesh (%d) %s", m_arrLoadedSegments.Count(), szFileName);
}

bool CVoxelSegment::StartStreaming()
{
	if (m_eStreamingStatus == ecss_InProgress)
		return true;

	if (m_eStreamingStatus != ecss_NotLoaded)
		return true;

	if (!pVoxStreamEngine)
		pVoxStreamEngine = new CVoxStreamEngine(m_strRenderDataFileName, 128 * 1024 * 1024);

	if (!pVoxStreamEngine->StartRead(this, m_nFileStreamOffset64, m_nFileStreamSize))
		return false;

	m_eStreamingStatus = ecss_InProgress;

	m_nStreamingTasksInProgress++;

	return true;
}

void CVoxelSegment::CropVoxTexture(int nTID, bool bCompSurfDist)
{
	m_vCropTexSize.Set(0, 0, 0);

	if (!GetBrickDataSize(m_pVoxOpacit))
		return;

	Vec3i vMin(nVoxTexMaxDim, nVoxTexMaxDim, nVoxTexMaxDim);
	Vec3i vMax(0, 0, 0);

	for (int x = 0; x < nVoxTexMaxDim; x++)
		for (int y = 0; y < nVoxTexMaxDim; y++)
			for (int z = 0; z < nVoxTexMaxDim; z++)
			{
	#ifdef FEATURE_SVO_GI_ALLOW_HQ
				ColorB& colOut = m_pVoxVolume[z * nVoxTexMaxDim * nVoxTexMaxDim + y * nVoxTexMaxDim + x];
				ColorB& norOut = m_pVoxNormal[z * nVoxTexMaxDim * nVoxTexMaxDim + y * nVoxTexMaxDim + x];
	#endif

				ColorB& opaOut = m_pVoxOpacit[z * nVoxTexMaxDim * nVoxTexMaxDim + y * nVoxTexMaxDim + x];

				if (opaOut.r || opaOut.g || opaOut.b)
				{
	#ifdef FEATURE_SVO_GI_ALLOW_HQ
					norOut.a = 255;
	#endif
				}

				if ((opaOut.r || opaOut.g || opaOut.b) /*|| GetCVars()->e_svoTI_Active*/ || Cry3DEngineBase::GetCVars()->e_svoTI_Troposphere_Subdivide)
				{
					vMin.CheckMin(Vec3i(x, y, z));
					vMax.CheckMax(Vec3i(x + 1, y + 1, z + 1));
				}
			}

	m_vCropTexSize = vMax - vMin;

	for (int d = 0; d < 3; d++)
	{
		if (vMax[d] < nVoxTexMaxDim)
			vMax[d]++;

		if (vMin[d] > 0)
			vMin[d]--;
	}

	m_vCropTexSize = vMax - vMin;

	if (m_vCropTexSize.x > 0 && m_vCropTexSize.y > 0 && m_vCropTexSize.z > 0)
	{
	#ifdef FEATURE_SVO_GI_ALLOW_HQ
		#ifdef FEATURE_SVO_GI_USE_MESH_RT
		ColorB* pTex3dOptTRIS = new ColorB[m_vCropTexSize.x * m_vCropTexSize.y * m_vCropTexSize.z];
		memset(pTex3dOptTRIS, 0, m_vCropTexSize.x * m_vCropTexSize.y * m_vCropTexSize.z * sizeof(ColorB));
		#endif
		ColorB* pTex3dOptRGBA = new ColorB[m_vCropTexSize.x * m_vCropTexSize.y * m_vCropTexSize.z];
		memset(pTex3dOptRGBA, 0, m_vCropTexSize.x * m_vCropTexSize.y * m_vCropTexSize.z * sizeof(ColorB));

		ColorB* pTex3dOptNorm = new ColorB[m_vCropTexSize.x * m_vCropTexSize.y * m_vCropTexSize.z];
		memset(pTex3dOptNorm, 0, m_vCropTexSize.x * m_vCropTexSize.y * m_vCropTexSize.z * sizeof(ColorB));
	#endif

		ColorB* pTex3dOptOpas = new ColorB[m_vCropTexSize.x * m_vCropTexSize.y * m_vCropTexSize.z];
		memset(pTex3dOptOpas, 0, m_vCropTexSize.x * m_vCropTexSize.y * m_vCropTexSize.z * sizeof(ColorB));

		for (int x = 0; x < m_vCropTexSize.x; x++)
			for (int y = 0; y < m_vCropTexSize.y; y++)
				for (int z = 0; z < m_vCropTexSize.z; z++)
				{
	#ifdef FEATURE_SVO_GI_ALLOW_HQ
		#ifdef FEATURE_SVO_GI_USE_MESH_RT
					ColorB& triOut = pTex3dOptTRIS[z * m_vCropTexSize.x * m_vCropTexSize.y + y * m_vCropTexSize.x + x];
		#endif
					ColorB& colOut = pTex3dOptRGBA[z * m_vCropTexSize.x * m_vCropTexSize.y + y * m_vCropTexSize.x + x];
					ColorB& norOut = pTex3dOptNorm[z * m_vCropTexSize.x * m_vCropTexSize.y + y * m_vCropTexSize.x + x];
	#endif
					ColorB& opaOut = pTex3dOptOpas[z * m_vCropTexSize.x * m_vCropTexSize.y + y * m_vCropTexSize.x + x];

					int x_in = x + vMin.x;
					int y_in = y + vMin.y;
					int z_in = z + vMin.z;

	#ifdef FEATURE_SVO_GI_ALLOW_HQ
		#ifdef FEATURE_SVO_GI_USE_MESH_RT
					ColorB& triIn = m_pVoxTris[z_in * nVoxTexMaxDim * nVoxTexMaxDim + y_in * nVoxTexMaxDim + x_in];
		#endif
					ColorB& colIn = m_pVoxVolume[z_in * nVoxTexMaxDim * nVoxTexMaxDim + y_in * nVoxTexMaxDim + x_in];
					ColorB& norIn = m_pVoxNormal[z_in * nVoxTexMaxDim * nVoxTexMaxDim + y_in * nVoxTexMaxDim + x_in];
	#endif
					ColorB& opaIn = m_pVoxOpacit[z_in * nVoxTexMaxDim * nVoxTexMaxDim + y_in * nVoxTexMaxDim + x_in];

					if (opaIn.r || opaIn.g || opaIn.b)
					{
	#ifdef FEATURE_SVO_GI_ALLOW_HQ
		#ifdef FEATURE_SVO_GI_USE_MESH_RT
						if (m_pVoxTris)
							triOut = triIn;
		#endif
						colOut = colIn;
						norOut = norIn;
	#endif

						opaOut = opaIn;
					}
				}
		/*
		   static float fTimeSumm = 0;

		   {
		   float fTimeStart = GetCurAsyncTimeSec();

		   if(bCompSurfDist)
		   {
		   #ifdef FEATURE_SVO_GI_ALLOW_HQ
		    ComputeDistancesFast_MinDistToSurf(pTex3dOptRGBA, pTex3dOptNorm, pTex3dOptOpas, nTID);
		   #endif
		   }

		   fTimeSumm += GetCurAsyncTimeSec()-fTimeStart;
		   }

		   static int nCount = 0;
		   nCount++;
		   if(nCount==1000)
		   {
		   Cry3DEngineBase::PrintMessage("%s: fTimeSumm = %.1f for %d nodes", __FUNCTION__, fTimeSumm, nCount);
		   //CVoxelSegment::ErrorTerminate("CVoxelSegment::SetDistanceToClosestVoxel: fTimeSumm = %.1f for %d nodes", fTimeSumm, nCount);
		   }
		 */
	#ifdef FEATURE_SVO_GI_ALLOW_HQ
		#ifdef FEATURE_SVO_GI_USE_MESH_RT
		if (m_pVoxTris)
			memcpy(m_pVoxTris, pTex3dOptTRIS, sizeof(ColorB) * m_vCropTexSize.x * m_vCropTexSize.y * m_vCropTexSize.z);
		SAFE_DELETE_ARRAY(pTex3dOptTRIS);
		#endif
		memcpy(m_pVoxVolume, pTex3dOptRGBA, sizeof(ColorB) * m_vCropTexSize.x * m_vCropTexSize.y * m_vCropTexSize.z);
		SAFE_DELETE_ARRAY(pTex3dOptRGBA);

		memcpy(m_pVoxNormal, pTex3dOptNorm, sizeof(ColorB) * m_vCropTexSize.x * m_vCropTexSize.y * m_vCropTexSize.z);
		SAFE_DELETE_ARRAY(pTex3dOptNorm);
	#endif

		memcpy(m_pVoxOpacit, pTex3dOptOpas, sizeof(ColorB) * m_vCropTexSize.x * m_vCropTexSize.y * m_vCropTexSize.z);
		SAFE_DELETE_ARRAY(pTex3dOptOpas);

		m_vCropBoxMin = vMin;
	}
	else
	{
		FreeAllBrickData();

		m_vCropTexSize.zero();
		m_vCropBoxMin.zero();
	}
}

void CVoxelSegment::ComputeDistancesFast_MinDistToSurf(ColorB* pTex3dOptRGBA, ColorB* pTex3dOptNorm, ColorB* pTex3dOptOpac, int nTID)
{
	for (int Z = 0; Z < m_vCropTexSize.z; Z++)
	{
		for (int Y = 0; Y < m_vCropTexSize.y; Y++)
		{
			for (int X = 0; X < m_vCropTexSize.x; X++)
			{
				int idOut_ = Z * m_vCropTexSize.x * m_vCropTexSize.y + Y * m_vCropTexSize.x + X;

				ColorB& colOut_ = pTex3dOptRGBA[idOut_];
				ColorB& norOut_ = pTex3dOptNorm[idOut_];

				if (colOut_.a != 255) // if empty
				{
					norOut_.a = 0;

					Vec3 v1((float)X, (float)Y, (float)Z);

					float fMinDistSq = 255 * 255 * 2;

					int _nNearestId = 0;

					int X1 = CLAMP(X - nDistToSurfRange, 0, m_vCropTexSize.x - 1);
					int X2 = CLAMP(X + nDistToSurfRange, 0, m_vCropTexSize.x - 1);
					int Y1 = CLAMP(Y - nDistToSurfRange, 0, m_vCropTexSize.y - 1);
					int Y2 = CLAMP(Y + nDistToSurfRange, 0, m_vCropTexSize.y - 1);
					int Z1 = CLAMP(Z - nDistToSurfRange, 0, m_vCropTexSize.z - 1);
					int Z2 = CLAMP(Z + nDistToSurfRange, 0, m_vCropTexSize.z - 1);

					// find nearest voxel
					for (int _x = X1; _x <= X2; _x++)
						for (int _y = Y1; _y <= Y2; _y++)
							for (int _z = Z1; _z <= Z2; _z++)
							{
								int _idOut = (_z) * m_vCropTexSize.x * m_vCropTexSize.y + (_y) * m_vCropTexSize.x + (_x);

								const ColorB& colOut = pTex3dOptRGBA[_idOut];

								if (colOut.a == 255)
								{
									Vec3 v0((float)_x, (float)_y, (float)_z);
									float fDistSq = v1.GetSquaredDistance(v0);

									if (fDistSq < fMinDistSq)
									{
										fMinDistSq = fDistSq;
										_nNearestId = _idOut;
									}
								}
							}

					fMinDistSq = sqrt(fMinDistSq);

					if (fMinDistSq <= 4)
						norOut_.a = SATURATEB(255 - (int)fMinDistSq);
				}
			}
		}
	}
}

bool CVoxelSegment::UpdateBrickRenderData()
{
	FUNCTION_PROFILER_3DENGINE;

	ReleaseAtlasBlock();

	if (!m_pBlockPacker)
		m_pBlockPacker = new CBlockPacker3D(nAtlasDimMaxXY, nAtlasDimMaxXY, nAtlasDimMaxZ, true);

	int nBlockW = max(1, (m_vCropTexSize.x + nVoxBloMaxDim - 1) / nVoxBloMaxDim);
	int nBlockH = max(1, (m_vCropTexSize.y + nVoxBloMaxDim - 1) / nVoxBloMaxDim);
	int nBlockD = max(1, (m_vCropTexSize.z + nVoxBloMaxDim - 1) / nVoxBloMaxDim);

	const float fMinNodeSize = Cry3DEngineBase::GetCVars()->e_svoMinNodeSize;

	const int nDataSizeStatsScale = 4;

	static int nLastStatFrame = 0;
	if (nLastStatFrame != GetCurrPassMainFrameID())
	{
		nLastStatFrame = GetCurrPassMainFrameID();

		// count stats
		m_nPoolUsageItems = 0;
		m_nPoolUsageBytes = 0;

		// remove 4 oldest blocks
		SBlockMinMax* pOldestBlockInfo[4] = { 0, 0, 0, 0 };
		uint nOldestVisFrameId[4] = { ~0U, ~0U, ~0U, ~0U };
		const uint nMaxAllowedFrameId = GetCurrPassMainFrameID() - 16;
		const uint nNumBlocks = m_pBlockPacker->GetNumBlocks();
		for (uint nBlockId = 0; nBlockId < nNumBlocks; nBlockId++)
		{
			if (SBlockMinMax* pInfo = m_pBlockPacker->GetBlockInfo(nBlockId))
			{
				m_nPoolUsageItems++;
				m_nPoolUsageBytes += nDataSizeStatsScale * pInfo->m_nDataSize;
			}
		}
	}

	// TODO: pack multiple bricks into single compressed block and upload to GPU only once

	for (int nPass = 0; nPass < 16; nPass++)
	{
		m_pBlockInfo = m_pBlockPacker->AddBlock(nBlockW, nBlockH, nBlockD,
		                                        this, GetCurrPassMainFrameID(), m_vCropTexSize.x * m_vCropTexSize.y * m_vCropTexSize.z * (gSvoEnv->m_nVoxTexFormat == eTF_BC3 ? 1 : 4) / nDataSizeStatsScale);
		if (m_pBlockInfo)
		{
			bool bIsMultiThreadedRenderer = false;
			gEnv->pRenderer->EF_Query(EFQ_RenderMultithreaded, bIsMultiThreadedRenderer);

			if (bIsMultiThreadedRenderer)
				m_pNode->m_nRequestSegmentUpdateFrametId = max(m_pNode->m_nRequestSegmentUpdateFrametId, GetCurrPassMainFrameID() + 1);
			else
				m_pNode->m_nRequestSegmentUpdateFrametId = max(m_pNode->m_nRequestSegmentUpdateFrametId, GetCurrPassMainFrameID() + 0);

			m_vStatLightsCheckSumm.zero();
			break;
		}

		// remove 4 oldest blocks
		SBlockMinMax* pOldestBlockInfo[4] = { 0, 0, 0, 0 };
		uint nOldestVisFrameId[4] = { ~0U, ~0U, ~0U, ~0U };
		const uint nMaxAllowedFrameId = GetCurrPassMainFrameID() - 16;
		const uint nNumBlocks = m_pBlockPacker->GetNumBlocks();
		for (uint nBlockId = 0; nBlockId < nNumBlocks; nBlockId++)
		{
			if (SBlockMinMax* pInfo = m_pBlockPacker->GetBlockInfo(nBlockId))
			{
				int nNewestSlotId = 0;
				for (int i = 0; i < 4; i++)
					if (nOldestVisFrameId[i] > nOldestVisFrameId[nNewestSlotId])
						nNewestSlotId = i;

				if (pInfo->m_nLastVisFrameId < nOldestVisFrameId[nNewestSlotId] && pInfo->m_nLastVisFrameId < nMaxAllowedFrameId)
				{
					CVoxelSegment* pSeg = (CVoxelSegment*)pInfo->m_pUserData;
					pInfo->m_nLastVisFrameId = pSeg->m_nLastRendFrameId;

					uint nFrameIdWeighted = pSeg->m_nLastRendFrameId + int(pSeg->GetBoxSize() / fMinNodeSize);

					if ((nFrameIdWeighted < nOldestVisFrameId[nNewestSlotId])
					    && (pSeg->m_nLastTexUpdateFrameId < nMaxAllowedFrameId)
					    && (pSeg->m_nLastRendFrameId < nMaxAllowedFrameId))
					{
						pOldestBlockInfo[nNewestSlotId] = pInfo;
						nOldestVisFrameId[nNewestSlotId] = nFrameIdWeighted;
					}
				}
			}
		}

		int nNumRemovedBlocks = 0;

		for (int i = 0; i < 4; i++)
			if (pOldestBlockInfo[i])
			{
				CVoxelSegment* pSeg = (CVoxelSegment*)pOldestBlockInfo[i]->m_pUserData;
				if (pSeg->m_pBlockInfo != pOldestBlockInfo[i])
					ErrorTerminate("pSeg->m_pBlockInfo != pOldestBlockInfo[i]");
				pSeg->ReleaseAtlasBlock();
				nNumRemovedBlocks++;
			}

		if (!nNumRemovedBlocks)
		{
			break;
		}
	}

	if (m_pBlockInfo == 0)
	{
		Cry3DEngineBase::PrintMessage("UpdateBrickRenderData postponed %d", GetCurrPassMainFrameID());
		return false;
	}

	m_nLastTexUpdateFrameId = GetCurrPassMainFrameID();
	m_nLastRendFrameId = GetCurrPassMainFrameID();

	assert(m_pBlockInfo->m_pUserData == this);
	Vec3i vOffset(m_pBlockInfo->m_dwMinX, m_pBlockInfo->m_dwMinY, m_pBlockInfo->m_dwMinZ);
	m_nAllocatedAtlasOffset = vOffset.z * nAtlasDimMaxXY * nAtlasDimMaxXY + vOffset.y * nAtlasDimMaxXY + vOffset.x;

	if (m_vCropTexSize.GetVolume() && m_pVoxOpacit)
		UpdateVoxRenderData();

	UpdateNodeRenderData();

	UpdateMeshRenderData();

	return true;
}

bool CVoxelSegment::CheckUpdateBrickRenderData(bool bJustCheck)
{
	bool bRes = true;

	if (m_nAllocatedAtlasOffset < 0)
	{
		if (bJustCheck)
			bRes = false;
		else
			bRes = UpdateBrickRenderData();
	}

	return bRes;
}

void CVoxelSegment::UpdateStreamingEngine()
{
	FUNCTION_PROFILER_3DENGINE;

	if (pVoxStreamEngine)
		pVoxStreamEngine->ProcessSyncCallBacks();
}

void CVoxelSegment::CheckAllocateTexturePool()
{
	int nFlagsReadOnly = FT_DONT_STREAM;
	int nFlagsReadWrite = FT_DONT_STREAM | FT_USAGE_UNORDERED_ACCESS | FT_USAGE_UAV_RWTEXTURE;

	#ifdef FEATURE_SVO_GI_ALLOW_HQ
	if (!gSvoEnv->m_nTexRgb0PoolId)
	{
		gSvoEnv->m_nTexRgb0PoolId = gEnv->pRenderer->DownLoadToVideoMemory3D(NULL,
		                                                                     nVoxTexPoolDimXY, nVoxTexPoolDimXY, nVoxTexPoolDimZ, gSvoEnv->m_nVoxTexFormat, gSvoEnv->m_nVoxTexFormat, 1, false, FILTER_LINEAR, 0, 0, nFlagsReadWrite);
		m_nSvoDataPoolsCounter++;
	}

	if (Cry3DEngineBase::GetCVars()->e_svoTI_Active && Cry3DEngineBase::GetCVars()->e_svoTI_IntegrationMode)
	{
		// direct lighting
		if (!gSvoEnv->m_nTexRgb1PoolId)
		{
			gSvoEnv->m_nTexRgb1PoolId = gEnv->pRenderer->DownLoadToVideoMemory3D(NULL,
			                                                                     nVoxTexPoolDimXY, nVoxTexPoolDimXY, nVoxTexPoolDimZ, gSvoEnv->m_nVoxTexFormat, gSvoEnv->m_nVoxTexFormat, 1, false, FILTER_LINEAR, 0, 0, nFlagsReadWrite);
			m_nSvoDataPoolsCounter++;
		}

		// dyn direct lighting
		if (!gSvoEnv->m_nTexDynlPoolId && Cry3DEngineBase::GetCVars()->e_svoTI_DynLights)
		{
			gSvoEnv->m_nTexDynlPoolId = gEnv->pRenderer->DownLoadToVideoMemory3D(NULL,
			                                                                     nVoxTexPoolDimXY, nVoxTexPoolDimXY, nVoxTexPoolDimZ, gSvoEnv->m_nVoxTexFormat, gSvoEnv->m_nVoxTexFormat, 1, false, FILTER_LINEAR, 0, 0, nFlagsReadWrite);
			m_nSvoDataPoolsCounter++;
		}

		// propagation
		if (!gSvoEnv->m_nTexRgb2PoolId && Cry3DEngineBase::GetCVars()->e_svoTI_NumberOfBounces > 1)
		{
			gSvoEnv->m_nTexRgb2PoolId = gEnv->pRenderer->DownLoadToVideoMemory3D(NULL,
			                                                                     nVoxTexPoolDimXY, nVoxTexPoolDimXY, nVoxTexPoolDimZ, gSvoEnv->m_nVoxTexFormat, gSvoEnv->m_nVoxTexFormat, 1, false, FILTER_LINEAR, 0, 0, nFlagsReadWrite);
			m_nSvoDataPoolsCounter++;
		}

		// propagation
		if (!gSvoEnv->m_nTexRgb3PoolId && Cry3DEngineBase::GetCVars()->e_svoTI_NumberOfBounces > 2)
		{
			gSvoEnv->m_nTexRgb3PoolId = gEnv->pRenderer->DownLoadToVideoMemory3D(NULL,
			                                                                     nVoxTexPoolDimXY, nVoxTexPoolDimXY, nVoxTexPoolDimZ, gSvoEnv->m_nVoxTexFormat, gSvoEnv->m_nVoxTexFormat, 1, false, FILTER_LINEAR, 0, 0, nFlagsReadWrite);
			m_nSvoDataPoolsCounter++;
		}

		// mesh
		#ifdef FEATURE_SVO_GI_USE_MESH_RT
		if (!gSvoEnv->m_nTexTrisPoolId && Cry3DEngineBase::GetCVars()->e_svoTI_RT_MaxDist)
		{
			gSvoEnv->m_nTexTrisPoolId = gEnv->pRenderer->DownLoadToVideoMemory3D(NULL,
			                                                                     nVoxTexPoolDimXY, nVoxTexPoolDimXY, nVoxTexPoolDimZ, gSvoEnv->m_nVoxTexFormat, gSvoEnv->m_nVoxTexFormat, 1, false, FILTER_LINEAR, 0, 0, nFlagsReadWrite);
			m_nSvoDataPoolsCounter++;
		}
		#endif
		// snow
		if (!gSvoEnv->m_nTexRgb4PoolId && Cry3DEngineBase::GetCVars()->e_svoTI_Troposphere_Snow_Height)
		{
			gSvoEnv->m_nTexRgb4PoolId = gEnv->pRenderer->DownLoadToVideoMemory3D(NULL,
			                                                                     nVoxTexPoolDimXY, nVoxTexPoolDimXY, nVoxTexPoolDimZ, gSvoEnv->m_nVoxTexFormat, gSvoEnv->m_nVoxTexFormat, 1, false, FILTER_LINEAR, 0, 0, nFlagsReadWrite);
			m_nSvoDataPoolsCounter++;
		}
	}

	{
		if (!gSvoEnv->m_nTexNormPoolId)
		{
			gSvoEnv->m_nTexNormPoolId = gEnv->pRenderer->DownLoadToVideoMemory3D(NULL,
			                                                                     nVoxTexPoolDimXY, nVoxTexPoolDimXY, nVoxTexPoolDimZ, gSvoEnv->m_nVoxTexFormat, gSvoEnv->m_nVoxTexFormat, 1, false, FILTER_LINEAR, 0, 0, nFlagsReadOnly);
			m_nSvoDataPoolsCounter++;
		}

		if (!gSvoEnv->m_nTexAldiPoolId && Cry3DEngineBase::GetCVars()->e_svoTI_Diffuse_Cache)
		{
			gSvoEnv->m_nTexAldiPoolId = gEnv->pRenderer->DownLoadToVideoMemory3D(NULL,
			                                                                     nVoxTexPoolDimXY, nVoxTexPoolDimXY, nVoxTexPoolDimZ, gSvoEnv->m_nVoxTexFormat, gSvoEnv->m_nVoxTexFormat, 1, false, FILTER_LINEAR, 0, 0, nFlagsReadWrite);
			m_nSvoDataPoolsCounter++;
		}
	}
	#endif

	if (!gSvoEnv->m_nTexOpasPoolId)
	{
#ifndef _RELEASE
		int nVoxDataSize = nVoxTexPoolDimXY * nVoxTexPoolDimXY * nVoxTexPoolDimZ * sizeof(ColorB);
		byte * pDataZero = new byte[nVoxDataSize];
		memset(pDataZero, 0, nVoxDataSize);
#else
		byte * pDataZero = 0;
#endif

		gSvoEnv->m_nTexOpasPoolId = gEnv->pRenderer->DownLoadToVideoMemory3D(pDataZero,
			nVoxTexPoolDimXY, nVoxTexPoolDimXY, nVoxTexPoolDimZ, gSvoEnv->m_nVoxTexFormat, gSvoEnv->m_nVoxTexFormat, 1, false, FILTER_LINEAR, 0, 0, nFlagsReadOnly);
		m_nSvoDataPoolsCounter++;

		gSvoEnv->m_nTexNodePoolId = gEnv->pRenderer->DownLoadToVideoMemory3D(pDataZero,
			nVoxNodPoolDimXY, nVoxNodPoolDimXY, nVoxNodPoolDimZ, eTF_R32G32B32A32F, eTF_R32G32B32A32F, 1, false, FILTER_POINT, 0, 0, nFlagsReadOnly);

#ifndef _RELEASE
		delete[] pDataZero;
#endif
	}
}

void CVoxelSegment::UpdateNodeRenderData()
{
	FUNCTION_PROFILER_3DENGINE;

	assert(m_pBlockInfo->m_pUserData == this);
	Vec3i vOffset(m_pBlockInfo->m_dwMinX, m_pBlockInfo->m_dwMinY, m_pBlockInfo->m_dwMinZ);

	static Vec4 voxNodeData[nVoxNodMaxDim * nVoxNodMaxDim * nVoxNodMaxDim];
	ZeroStruct(voxNodeData);

	voxNodeData[0] = Vec4(m_boxOS.min + m_vSegOrigin, 0);
	voxNodeData[1] = Vec4(m_boxOS.max + m_vSegOrigin, 0);
	voxNodeData[0] = voxNodeData[0] + Vec4(Vec3((float)m_vCropBoxMin.x / nVoxTexMaxDim, (float)m_vCropBoxMin.y / nVoxTexMaxDim, (float)m_vCropBoxMin.z / nVoxTexMaxDim) * GetBoxSize(), 0);
	voxNodeData[1] = voxNodeData[0] + Vec4(Vec3((float)m_vCropTexSize.x / nVoxTexMaxDim, (float)m_vCropTexSize.y / nVoxTexMaxDim, (float)m_vCropTexSize.z / nVoxTexMaxDim) * GetBoxSize(), 0);
	voxNodeData[0].w = GetBoxSize();
	voxNodeData[1].w = m_pParentCloud ? (0.1f + (float)m_pParentCloud->m_nAllocatedAtlasOffset) : -2.f;

	for (int c = 0; c < 4; c++)
	{
		if (m_arrChildOffset[c + 0] >= 0)
			voxNodeData[2][c] = 0.1f + (float)m_arrChildOffset[c + 0];
		else
			voxNodeData[2][c] = -0.1f + (float)m_arrChildOffset[c + 0];

		if (m_arrChildOffset[c + 4] >= 0)
			voxNodeData[3][c] = 0.1f + (float)m_arrChildOffset[c + 4];
		else
			voxNodeData[3][c] = -0.1f + (float)m_arrChildOffset[c + 4];
	}

	voxNodeData[4][0] = 0.1f + (float)GetRenderer()->GetFrameID(false);

	gEnv->pRenderer->UpdateTextureInVideoMemory(
	  gSvoEnv->m_nTexNodePoolId,
	  (byte*)&voxNodeData[0],
	  vOffset.x * nVoxNodMaxDim,
	  vOffset.y * nVoxNodMaxDim,
	  nVoxNodMaxDim,
	  nVoxNodMaxDim,
	  eTF_R32G32B32A32F,
	  vOffset.z * nVoxNodMaxDim,
	  nVoxNodMaxDim);
	CVoxelSegment::m_nUpdatesInProgressTex++;

	m_boxClipped.min = Vec3(voxNodeData[0].x, voxNodeData[0].y, voxNodeData[0].z);
	m_boxClipped.max = Vec3(voxNodeData[1].x, voxNodeData[1].y, voxNodeData[1].z);
}

void CVoxelSegment::UpdateMeshRenderData()
{
	FUNCTION_PROFILER_3DENGINE;

	assert(m_pBlockInfo->m_pUserData == this);

	// define single vertex for GS
	{
		// set box origin
		m_vertForGS.xyz = m_boxClipped.min;

		// set pinter to brick data
		m_vertForGS.st.x = (0.5f + (float)m_nAllocatedAtlasOffset);

		// pack box size
		float fNodeSize = m_vertForGS.st.y = GetBoxSize();
		Vec3 vBoxSize = m_boxClipped.GetSize();
		m_vertForGS.color.bcolor[0] = SATURATEB(int(vBoxSize.x / fNodeSize * 255.f));
		m_vertForGS.color.bcolor[1] = SATURATEB(int(vBoxSize.y / fNodeSize * 255.f));
		m_vertForGS.color.bcolor[2] = SATURATEB(int(vBoxSize.z / fNodeSize * 255.f));
		m_vertForGS.color.bcolor[3] = SATURATEB(int(m_fMaxAlphaInBrick * 255.f));
	}
}

void CVoxelSegment::UpdateVoxRenderData()
{
	FUNCTION_PROFILER_3DENGINE;

	assert(m_pBlockInfo->m_pUserData == this);
	Vec3i vOffset(m_pBlockInfo->m_dwMinX, m_pBlockInfo->m_dwMinY, m_pBlockInfo->m_dwMinZ);

	#ifdef FEATURE_SVO_GI_ALLOW_HQ
		#ifdef FEATURE_SVO_GI_USE_MESH_RT
	byte* pImgTri = (byte*)m_pVoxTris;
		#endif
	byte* pImgRgb = (byte*)m_pVoxVolume;
	byte* pImgNor = (byte*)m_pVoxNormal;
	#endif
	byte* pImgOpa = (byte*)m_pVoxOpacit;

	Vec3i vSizeFin;
	vSizeFin.x = (m_vCropTexSize.x);
	vSizeFin.y = (m_vCropTexSize.y);
	vSizeFin.z = (m_vCropTexSize.z);

	if (gSvoEnv->m_nVoxTexFormat == eTF_BC3)
	{
		vSizeFin = GetDxtDim();
	}

	#ifdef FEATURE_SVO_GI_ALLOW_HQ

	gEnv->pRenderer->UpdateTextureInVideoMemory(
	  gSvoEnv->m_nTexRgb0PoolId,
	  pImgRgb,
	  vOffset.x * nVoxBloMaxDim,
	  vOffset.y * nVoxBloMaxDim,
	  vSizeFin.x,
	  vSizeFin.y,
	  gSvoEnv->m_nVoxTexFormat,
	  vOffset.z * nVoxBloMaxDim,
	  vSizeFin.z);
	CVoxelSegment::m_nUpdatesInProgressTex++;

		#ifdef FEATURE_SVO_GI_USE_MESH_RT
	if (Cry3DEngineBase::GetCVars()->e_svoTI_Active && gSvoEnv->m_nTexTrisPoolId)
	{
		gEnv->pRenderer->UpdateTextureInVideoMemory(
		  gSvoEnv->m_nTexTrisPoolId,
		  pImgTri,
		  vOffset.x * nVoxBloMaxDim,
		  vOffset.y * nVoxBloMaxDim,
		  vSizeFin.x,
		  vSizeFin.y,
		  gSvoEnv->m_nVoxTexFormat,
		  vOffset.z * nVoxBloMaxDim,
		  vSizeFin.z);
		CVoxelSegment::m_nUpdatesInProgressTex++;
	}
		#endif

	gEnv->pRenderer->UpdateTextureInVideoMemory(
	  gSvoEnv->m_nTexNormPoolId,
	  pImgNor,
	  vOffset.x * nVoxBloMaxDim,
	  vOffset.y * nVoxBloMaxDim,
	  vSizeFin.x,
	  vSizeFin.y,
	  gSvoEnv->m_nVoxTexFormat,
	  vOffset.z * nVoxBloMaxDim,
	  vSizeFin.z);
	CVoxelSegment::m_nUpdatesInProgressTex++;

	#endif

	gEnv->pRenderer->UpdateTextureInVideoMemory(
	  gSvoEnv->m_nTexOpasPoolId,
	  pImgOpa,
	  vOffset.x * nVoxBloMaxDim,
	  vOffset.y * nVoxBloMaxDim,
	  vSizeFin.x,
	  vSizeFin.y,
	  gSvoEnv->m_nVoxTexFormat,
	  vOffset.z * nVoxBloMaxDim,
	  vSizeFin.z);
	CVoxelSegment::m_nUpdatesInProgressTex++;
}

void CVoxelSegment::ReleaseAtlasBlock()
{
	if (m_pBlockInfo)
		m_pBlockPacker->RemoveBlock(m_pBlockInfo);
	m_pBlockInfo = 0;
	m_nAllocatedAtlasOffset = -2;

	m_pNode->m_nRequestSegmentUpdateFrametId = 0;
	m_vStatLightsCheckSumm.zero();

	PropagateDirtyFlag();
}

void CVoxelSegment::PropagateDirtyFlag()
{
	if (CVoxelSegment* pParent = m_pParentCloud)
	{
		pParent->m_nChildOffsetsDirty = 2;

		if (pParent->m_pParentCloud)
			pParent->m_pParentCloud->m_nChildOffsetsDirty = 2;

		while (pParent->m_pParentCloud)
		{
			pParent = pParent->m_pParentCloud;
			pParent->m_nChildOffsetsDirty = max(pParent->m_nChildOffsetsDirty, (byte)1);
		}
	}
}

AABB CVoxelSegment::GetChildBBox(const AABB& parentBox, int nChildId)
{
	int x = (nChildId / 4);
	int y = (nChildId - x * 4) / 2;
	int z = (nChildId - x * 4 - y * 2);
	Vec3 vSize = parentBox.GetSize() * 0.5f;
	Vec3 vOffset = vSize;
	vOffset.x *= x;
	vOffset.y *= y;
	vOffset.z *= z;
	AABB childBox;
	childBox.min = parentBox.min + vOffset;
	childBox.max = childBox.min + vSize;
	return childBox;
}

bool GetBarycentricTC(const Vec3& a, const Vec3& b, const Vec3& c, float& u, float& v, float& w, const Vec3& p, const float& fBorder)
{
	Vec3 v0 = b - a, v1 = c - a, v2 = p - a;
	float d00 = v0.Dot(v0);
	float d01 = v0.Dot(v1);
	float d11 = v1.Dot(v1);
	float d20 = v2.Dot(v0);
	float d21 = v2.Dot(v1);
	float d = d00 * d11 - d01 * d01;
	float invDenom = d ? (1.0f / d) : 1000000.f;
	v = (d11 * d20 - d01 * d21) * invDenom;
	w = (d00 * d21 - d01 * d20) * invDenom;
	u = 1.0f - v - w;
	return (u >= -fBorder) && (v >= -fBorder) && (w >= -fBorder);
}

ColorF CVoxelSegment::GetColorF_255(int x, int y, const ColorB* pImg, int nImgSizeW, int nImgSizeH)
{
	const ColorB& colB = pImg[x + y * nImgSizeW];
	ColorF colF;
	colF.r = colB.r;
	colF.g = colB.g;
	colF.b = colB.b;
	colF.a = colB.a;

	return colF;
}

ColorF CVoxelSegment::GetBilinearAt(float iniX, float iniY, const ColorB* pImg, int nDimW, int nDimH, float fBr)
{
	int nImgSizeW = nDimW;
	int nImgSizeH = nDimH;

	iniX *= nImgSizeW;
	iniY *= nImgSizeH;

	//  iniX -= .5f;
	//  iniY -= .5f;

	int x = (int)floor(iniX);
	int y = (int)floor(iniY);

	float rx = iniX - x;    // fractional part
	float ry = iniY - y;    // fractional part

	int nMaskW = nImgSizeW - 1;
	int nMaskH = nImgSizeH - 1;

	//  return GetColorF_255(nMaskW&(x  ),nMaskH&(y  ), pImg, nImgSizeW, nImgSizeH) * fBr;

	ColorF top = GetColorF_255(nMaskW & (x), nMaskH & (y), pImg, nImgSizeW, nImgSizeH) * (1.f - rx)     // left top
	             + GetColorF_255(nMaskW & (x + 1), nMaskH & (y), pImg, nImgSizeW, nImgSizeH) * rx;      // right top
	ColorF bot = GetColorF_255(nMaskW & (x), nMaskH & (y + 1), pImg, nImgSizeW, nImgSizeH) * (1.f - rx) // left bottom
	             + GetColorF_255(nMaskW & (x + 1), nMaskH & (y + 1), pImg, nImgSizeW, nImgSizeH) * rx;  // right bottom

	return (top * (1.f - ry) + bot * ry) * fBr;
}

void CVoxelSegment::VoxelizeMeshes(int nTID)
{
	MEMSTAT_CONTEXT(EMemStatContextTypes::MSC_Other, 0, "VoxelizeMeshes");

	m_vCropBoxMin.Set(0, 0, 0);
	m_vCropTexSize.Set(nVoxTexMaxDim, nVoxTexMaxDim, nVoxTexMaxDim);

	PodArray<int>* pNodeTrisXYZ = 0;

	if (GetBoxSize() <= GetCVars()->e_svoMaxNodeSize)
	{
		pNodeTrisXYZ = new PodArray<int>[nVoxTexMaxDim * nVoxTexMaxDim * nVoxTexMaxDim];
		FindTrianglesForVoxelization(nTID, pNodeTrisXYZ, GetBoxSize() == GetCVars()->e_svoMaxNodeSize);

		gSvoEnv->m_arrVoxelizeMeshesCounter[0]++;
		if (GetBoxSize() == GetCVars()->e_svoMaxNodeSize)
			gSvoEnv->m_arrVoxelizeMeshesCounter[1]++;
	}

	#ifdef FEATURE_SVO_GI_ALLOW_HQ
		#ifdef FEATURE_SVO_GI_USE_MESH_RT
	CheckAllocateBrick(m_pVoxTris, m_vCropTexSize.x * m_vCropTexSize.y * m_vCropTexSize.z, true);
		#endif
	CheckAllocateBrick(m_pVoxVolume, m_vCropTexSize.x * m_vCropTexSize.y * m_vCropTexSize.z, true);
	CheckAllocateBrick(m_pVoxNormal, m_vCropTexSize.x * m_vCropTexSize.y * m_vCropTexSize.z, true);
	#endif
	CheckAllocateBrick(m_pVoxOpacit, m_vCropTexSize.x * m_vCropTexSize.y * m_vCropTexSize.z, true);

	Vec3 vBoxCenter = m_pNode->m_nodeBox.GetCenter();

	// voxelize node tris
	if ((m_nodeTrisAllMerged.Count() || Cry3DEngineBase::GetCVars()->e_svoTI_Troposphere_Subdivide) && m_pVoxOpacit && m_pTrisInArea && pNodeTrisXYZ)
	{
		Vec4 voxNodeData[nVoxNodMaxDim * nVoxNodMaxDim * nVoxNodMaxDim];
		ZeroStruct(voxNodeData);
		voxNodeData[0] = Vec4(m_boxOS.min + m_vSegOrigin, 0);
		voxNodeData[1] = Vec4(m_boxOS.max + m_vSegOrigin, 0);

	#ifdef FEATURE_SVO_GI_ALLOW_HQ
		// collect overlapping portals
		PodArray<CVisArea*> arrPortals;
		{
			for (int v = 0;; v++)
			{
				CVisArea* pVisArea = (CVisArea*)GetVisAreaManager()->GetVisAreaById(v);
				if (!pVisArea)
					break;

				if (pVisArea->IsPortal() && Overlap::AABB_AABB(*pVisArea->GetAABBox(), m_pNode->m_nodeBox))
					arrPortals.Add(pVisArea);
			}
		}
	#endif

		for (int X = 0; X < nVoxTexMaxDim; X++)
			for (int Y = 0; Y < nVoxTexMaxDim; Y++)
				for (int Z = 0; Z < nVoxTexMaxDim; Z++)
				{
					const int id = Z * nVoxTexMaxDim * nVoxTexMaxDim + Y * nVoxTexMaxDim + X;
	#ifdef FEATURE_SVO_GI_ALLOW_HQ
					ColorB& colOut = m_pVoxVolume[id];
					ColorB& norOut = m_pVoxNormal[id];
	#endif
					ColorB& opaOutFin = m_pVoxOpacit[id];
					Vec4 vMin = voxNodeData[0] + (voxNodeData[1] - voxNodeData[0]) * Vec4((float) X / nVoxTexMaxDim, (float) Y / nVoxTexMaxDim, (float) Z / nVoxTexMaxDim, 1);
					Vec4 vMax = voxNodeData[0] + (voxNodeData[1] - voxNodeData[0]) * Vec4((float) (X + 1) / nVoxTexMaxDim, (float) (Y + 1) / nVoxTexMaxDim, (float) (Z + 1) / nVoxTexMaxDim, 1);

					// safety border support
					//      Vec4 vCenter(m_vOrigin.x,m_vOrigin.y,m_vOrigin.z,0);
					//        vMin += (vMin - vCenter)/(nVoxTexMaxDim/2)/2;
					//        vMax += (vMax - vCenter)/(nVoxTexMaxDim/2)/2;

					AABB voxBox;
					voxBox.min.Set(vMin.x, vMin.y, vMin.z);
					voxBox.max.Set(vMax.x, vMax.y, vMax.z);

					AABB voxBoxRT = voxBox;
					voxBoxRT.Expand(voxBoxRT.GetSize() / 2);

					if (Overlap::AABB_AABB(voxBox, m_boxTris))
					{
						PodArray<int> trisInt;
						PodArray<int> trisIntRT;
						bool bVisAreaTrisDetected = false;

						AUTO_READLOCK(m_superMeshLock);

						// collect tris only for this voxel; TODO: check maybe pNodeTrisXYZ[id] already have only what is needed and trisInt can be dropped
						for (int d = 0; d < pNodeTrisXYZ[id].Count(); d++)
						{
							int trId = pNodeTrisXYZ[id].GetAt(d);

							SRayHitTriangleIndexed& tr = (*m_pTrisInArea)[trId];

							Vec3 arrV[3] = { (*m_pVertInArea)[tr.arrVertId[0]].v, (*m_pVertInArea)[tr.arrVertId[1]].v, (*m_pVertInArea)[tr.arrVertId[2]].v };

							if (Overlap::AABB_Triangle(voxBoxRT, arrV[0], arrV[1], arrV[2])) // 14s
							{
								trisIntRT.Add(trId);

								if (Overlap::AABB_Triangle(voxBox, arrV[0], arrV[1], arrV[2])) // 14s
								{
									if (tr.nHitObjType == HIT_OBJ_TYPE_VISAREA)
										bVisAreaTrisDetected = true;

									assert(trisInt.Find(trId) < 0);

									trisInt.Add(trId);
								}
							}
						}

	#ifdef FEATURE_SVO_GI_USE_MESH_RT
						if (GetCVars()->e_svoTI_RT_MaxDist && gSvoEnv->m_nTexTrisPoolId)
						{
							int nTrisCount = 0;
							int nFirstTriIndex = StoreIndicesIntoPool(trisIntRT, nTrisCount);
							int nId = nFirstTriIndex;
							m_pVoxTris[id].r = nId & 255;
							nId /= 256;
							m_pVoxTris[id].g = nId & 255;
							nId /= 256;
							m_pVoxTris[id].b = nId & 255;
							nId /= 256;
							m_pVoxTris[id].a = nTrisCount;
						}
	#endif

						// OPA
						{
							const int nSSDim = 4;

							const int nSDim = nSSDim / 2;

							PodArray<int> arrSubOpa[nSDim][nSDim][nSDim];

							{
								// Fill nSDim x nSDim x nSDim super voxel

								Vec3 vSubVoxSize = voxBox.GetSize() / (float)nSDim;

								for (int x = 0; x < nSDim; x++)
									for (int y = 0; y < nSDim; y++)
										for (int z = 0; z < nSDim; z++)
										{
											PodArray<int>& opaOutSub = arrSubOpa[x][y][z];

											AABB SubBox;
											SubBox.min = voxBox.min + vSubVoxSize.CompMul(Vec3((float)x, (float)y, (float)z));
											SubBox.max = SubBox.min + vSubVoxSize;

											AABB SubBoxT = SubBox;
											SubBoxT.max.z++;

											for (int c = 0; c < trisInt.Count(); c++)
											{
												int nTrId = trisInt.GetAt(c);

												SRayHitTriangleIndexed& tr = (*m_pTrisInArea)[nTrId];

												Vec3 arrV[3] = { (*m_pVertInArea)[tr.arrVertId[0]].v, (*m_pVertInArea)[tr.arrVertId[1]].v, (*m_pVertInArea)[tr.arrVertId[2]].v };

												if (Overlap::AABB_Triangle(tr.nMatID ? SubBox : SubBoxT, arrV[0], arrV[1], arrV[2])) // 40 ms
												{
													opaOutSub.Add(nTrId);
												}
											}
										}
							}

							uint8 arrSubSubOpa[nSSDim][nSSDim][nSSDim];
	#ifdef FEATURE_SVO_GI_ALLOW_HQ
							Vec4 arrSubSubNor[nSSDim][nSSDim][nSSDim];
							ColorF arrSubSubCol[nSSDim][nSSDim][nSSDim];
							float arrSubSubEmi[nSSDim][nSSDim][nSSDim];
	#endif
							ZeroStruct(arrSubSubOpa);
	#ifdef FEATURE_SVO_GI_ALLOW_HQ
							ZeroStruct(arrSubSubNor);
							ZeroStruct(arrSubSubCol);
							ZeroStruct(arrSubSubEmi);
	#endif

							// Fill nSSDim x nSSDim x nSSDim super voxel

							Vec3 vSubSubVoxSize = voxBox.GetSize() / (float)nSSDim;

							for (int x = 0; x < nSSDim; x++)
								for (int y = 0; y < nSSDim; y++)
									for (int z = 0; z < nSSDim; z++)
									{
										PodArray<int>& opaOutSub = arrSubOpa[x / 2][y / 2][z / 2];

										uint8& opaOutSubSub = arrSubSubOpa[x][y][z];
	#ifdef FEATURE_SVO_GI_ALLOW_HQ
										Vec4& norOutSubSub = arrSubSubNor[x][y][z];
										ColorF& colOutSubSub = arrSubSubCol[x][y][z];
										float& EmiOutSubSub = arrSubSubEmi[x][y][z];
	#endif

										AABB SubSubBox;
										SubSubBox.min = voxBox.min + vSubSubVoxSize.CompMul(Vec3((float)x, (float)y, (float)z));
										SubSubBox.max = SubSubBox.min + vSubSubVoxSize;

										AABB SubSubBoxT = SubSubBox;
										SubSubBoxT.max.z++;

										AABB SubSubBoxP = SubSubBox;
										SubSubBoxP.Expand(Vec3(.25, .25, .25));

	#ifdef FEATURE_SVO_GI_ALLOW_HQ
										bool bVoxelInPortal = false;

										for (int v = 0; v < arrPortals.Count(); v++)
										{
											if (Overlap::AABB_AABB(*arrPortals[v]->GetAABBox(), SubSubBoxP))
											{
												bVoxelInPortal = true;
												break;
											}
										}

										if (bVoxelInPortal) // skip sub-voxels in the portal
											continue;
	#endif

										for (int c = 0; c < opaOutSub.Count(); c++)
										{
											if (opaOutSub.GetAt(c) >= (*m_pTrisInArea).Count())
											{
												PrintMessage("%s warning: trId>=(*m_pTrisInArea).Count()", __FUNC__);
												break;
											}

											SRayHitTriangleIndexed& tr = (*m_pTrisInArea)[opaOutSub.GetAt(c)];

											Vec3 arrV[3] = { (*m_pVertInArea)[tr.arrVertId[0]].v, (*m_pVertInArea)[tr.arrVertId[1]].v, (*m_pVertInArea)[tr.arrVertId[2]].v };

											if (Overlap::AABB_Triangle(tr.nMatID ? SubSubBox : SubSubBoxT, arrV[0], arrV[1], arrV[2])) // 8ms
											{
												ColorF colTraced = ProcessMaterial(tr, SubSubBox.GetCenter());
												if (colTraced.a)
												{
													opaOutSubSub = max(opaOutSubSub, min(tr.nOpacity, (uint8)SATURATEB(colTraced.a * 255.f)));
	#ifdef FEATURE_SVO_GI_ALLOW_HQ
													norOutSubSub += Vec4(tr.vFaceNorm, 1);

													colOutSubSub.r += colTraced.r;
													colOutSubSub.g += colTraced.g;
													colOutSubSub.b += colTraced.b;
													colOutSubSub.a += 1.f;

													SSvoMatInfo& rMI = m_pMatsInArea->GetAt(tr.nMatID);
													if (rMI.pMat)
														EmiOutSubSub = +rMI.pMat->GetShaderItem(0).m_pShaderResources->GetFinalEmittance().Luminance();
	#else
													if (opaOutSubSub == 255)
														break;
	#endif
												}
											}
										}
									}

							// Compute tri-planar opacity

							uint8 arrQuad[3][nSSDim][nSSDim];
							ZeroStruct(arrQuad);
	#ifdef FEATURE_SVO_GI_ALLOW_HQ
							Vec4 vNorFin(0, 0, 0, 0);
							ColorF vColFin(0, 0, 0, 0);
							float fEmiFin(0);
	#endif

							for (int _x = 0; _x < nSSDim; _x++)
								for (int _y = 0; _y < nSSDim; _y++)
									for (int _z = 0; _z < nSSDim; _z++)
									{
										uint8& opaCh = arrSubSubOpa[_x][_y][_z];
	#ifdef FEATURE_SVO_GI_ALLOW_HQ
										Vec4& norCh = arrSubSubNor[_x][_y][_z];
										ColorF& colCh = arrSubSubCol[_x][_y][_z];
										float& EmiCh = arrSubSubEmi[_x][_y][_z];
	#endif

										arrQuad[0][_y][_z] = max(arrQuad[0][_y][_z], opaCh);
										arrQuad[1][_x][_z] = max(arrQuad[1][_x][_z], opaCh);
										arrQuad[2][_x][_y] = max(arrQuad[2][_x][_y], opaCh);

	#ifdef FEATURE_SVO_GI_ALLOW_HQ
										vNorFin += norCh;
										vColFin += colCh;
										fEmiFin += EmiCh;
	#endif
									}

							// we do not normalize normal, length of the normal says how reliable the normal is
	#ifdef FEATURE_SVO_GI_ALLOW_HQ
							if (vNorFin.w)
								vNorFin = vNorFin / vNorFin.w;

							if (vColFin.a)
							{
								fEmiFin /= vColFin.a;

								vColFin /= vColFin.a;
							}
	#endif

							ColorF opaSummF(0, 0, 0, 0);

							for (int k = 0; k < nSSDim; k++)
								for (int m = 0; m < nSSDim; m++)
								{
									opaSummF.r += arrQuad[0][k][m];
									opaSummF.g += arrQuad[1][k][m];
									opaSummF.b += arrQuad[2][k][m];
								}

							opaSummF.r /= nSSDim * nSSDim;
							opaSummF.g /= nSSDim * nSSDim;
							opaSummF.b /= nSSDim * nSSDim;

							opaOutFin.r = SATURATEB((int)opaSummF.b);
							opaOutFin.g = SATURATEB((int)opaSummF.g);
							opaOutFin.b = SATURATEB((int)opaSummF.r);

							bool bTerrainTrisDetected;
							Vec3 vH = voxBox.GetCenter();
							if (vH.z > (GetTerrain()->GetZ((int)vH.x, (int)vH.y, 0) + 1.5f))
								bTerrainTrisDetected = false;
							else
								bTerrainTrisDetected = true;

							opaOutFin.a = bTerrainTrisDetected ? 0 : 1; // reserved for opacity of dynamic voxels or [0 = triangle is missing in RSM]

							if (bVisAreaTrisDetected && (opaOutFin.r || opaOutFin.g || opaOutFin.b))
							{
								opaOutFin.r = opaOutFin.g = opaOutFin.b = 255; // full opaque
	#ifdef FEATURE_SVO_GI_ALLOW_HQ
								vColFin.r = vColFin.g = vColFin.b = 0; // full black
	#endif
							}

	#ifdef FEATURE_SVO_GI_ALLOW_HQ
							norOut.a = (opaOutFin.r || opaOutFin.g || opaOutFin.b) ? 255 : 0; // contains 255 for any geometry and 0 for empty space

							for (int c = 0; c < 3; c++)
								norOut[c] = SATURATEB(int(vNorFin[2 - c] * 127.5f + 127.5f));

							colOut.r = SATURATEB((int)(vColFin.r * 255.f));
							colOut.g = SATURATEB((int)(vColFin.g * 255.f));
							colOut.b = SATURATEB((int)(vColFin.b * 255.f));
							colOut.a = SATURATEB((int)(fEmiFin * 255.f));
	#endif
							if (opaOutFin.r || opaOutFin.g || opaOutFin.b)
								m_nVoxNum++;
						}
					}
				}

		//		UpdateTerrainNormals(m_pNode->m_nodeBox.GetCenter(), 1000000, m_pVoxTerrain);

		// dilation
		//ComputeDistancesFast( m_pVoxVolume, m_pVoxNormal, m_pVoxOpacit, nTID );

		if (!Cry3DEngineBase::GetCVars()->e_svoTI_Troposphere_Subdivide)
			CropVoxTexture(nTID, true);
	}
	else
	{
		m_vCropTexSize.zero();
		m_vCropBoxMin.zero();
	}

	if (Cry3DEngineBase::GetCVars()->e_svoTI_Troposphere_Subdivide)
	{
		// allocate all nodes
		m_vCropTexSize.zero();
		m_vCropTexSize.Set(nVoxTexMaxDim, nVoxTexMaxDim, nVoxTexMaxDim);
	}

	//  if(m_pVoxVolume)
	//  {
	//    int nPixNum = m_vCropTexSize.x *m_vCropTexSize.y * m_vCropTexSize.z;
	//    FILE * f = 0;
	//    char fName[128];
	//    cry_sprintf(fName, "E:\\BR\\brick_%5d__%2d_%2d_%2d.BR", m_nId, m_vCropTexSize.x, m_vCropTexSize.y, m_vCropTexSize.z);
	//    fopen_s(&f, fName, "wb");
	//    if(f)
	//    {
	//      fwrite(m_pVoxVolume, sizeof(ColorB), nPixNum, f);
	//      fclose(f);
	//    }
	//  }

	SAFE_DELETE_ARRAY(pNodeTrisXYZ);
}

static int32 CompareTriArea(const void* v1, const void* v2)
{
	SRayHitTriangle* t1 = (SRayHitTriangle*)v1;
	SRayHitTriangle* t2 = (SRayHitTriangle*)v2;

	if (t1->nTriArea > t2->nTriArea)
		return -1;
	if (t1->nTriArea < t2->nTriArea)
		return 1;

	return 0;
};

bool CVoxelSegment::CheckCollectObjectsForVoxelization(const AABB& cloudBoxWS, PodArray<SObjInfo>* parrObjects, bool& bThisIsAreaParent, bool& bThisIsLowLodNode, bool bAllowStartStreaming)
{
	FUNCTION_PROFILER_3DENGINE;

	bool bSuccess = true;

	bThisIsAreaParent = (cloudBoxWS.GetSize().z == GetCVars()->e_svoMaxAreaSize);
	bThisIsLowLodNode = (cloudBoxWS.GetSize().z > GetCVars()->e_svoMaxAreaSize);

	if (bThisIsAreaParent || bThisIsLowLodNode)
	{
		for (int nObjType = 0; nObjType < eERType_TypesNum; nObjType++)
		{
			if ((bThisIsAreaParent && (nObjType == eERType_Brush || nObjType == eERType_MovableBrush)) || (nObjType == eERType_Vegetation))
			{
				PodArray<IRenderNode*> arrRenderNodes;

				Get3DEngine()->GetObjectsByTypeGlobal(arrRenderNodes, (EERType)nObjType, &cloudBoxWS, bAllowStartStreaming ? &bSuccess : 0, ERF_GI_MODE_BIT0);
				if (Get3DEngine()->GetVisAreaManager())
					Get3DEngine()->GetVisAreaManager()->GetObjectsByType(arrRenderNodes, (EERType)nObjType, &cloudBoxWS, bAllowStartStreaming ? &bSuccess : 0, ERF_GI_MODE_BIT0);

				if (!arrRenderNodes.Count())
					continue;

				int nCulled = 0;

				for (int d = 0; d < arrRenderNodes.Count(); d++)
				{
					IRenderNode* pNode = arrRenderNodes[d];

					if (pNode->GetRndFlags() & (ERF_COLLISION_PROXY | ERF_RAYCAST_PROXY))
						continue;

					if (!GetCVars()->e_svoTI_VoxelizeHiddenObjects && (pNode->GetRndFlags() & ERF_HIDDEN))
						continue;

					if (bThisIsLowLodNode)
						if (!(pNode->GetRndFlags() & ERF_CASTSHADOWMAPS))
							continue;

					if (pNode->GetGIMode() != IRenderNode::eGM_StaticVoxelization)
						continue;

					float fMaxViewDist = pNode->GetBBox().GetRadius() * GetCVars()->e_ViewDistRatio;

					float fMinAllowedViewDist = (pNode->GetRenderNodeType() == eERType_Vegetation) ? (GetCVars()->e_svoTI_ObjectsMaxViewDistance * 2) : GetCVars()->e_svoTI_ObjectsMaxViewDistance;

					if (bThisIsLowLodNode)
					{
						if (pNode->GetBBox().GetSize().z < cloudBoxWS.GetSize().z * 0.25f)
							continue;

						fMinAllowedViewDist *= 4.f;
					}

					if (fMaxViewDist < fMinAllowedViewDist)
					{
						nCulled++;
						continue;
					}

					Matrix34A nodeTM;
					CStatObj* pStatObj = (CStatObj*)pNode->GetEntityStatObj(0, &nodeTM);

					IMaterial* pMaterial = pNode->GetMaterial();
					if (!pMaterial && pStatObj)
						pMaterial = pStatObj->GetMaterial();

					if (pMaterial)
					{
						SObjInfo info;
						info.matObjInv = nodeTM.GetInverted();
						info.matObj = nodeTM;
						info.pStatObj = pStatObj;

						if (!info.pStatObj)
							continue;

						int nLod = GetCVars()->e_svoTI_ObjectsLod;

						if (bThisIsLowLodNode)
							nLod++;

						info.pStatObj = (CStatObj*)info.pStatObj->GetLodObject(nLod, true);

						CStatObj* pParent = info.pStatObj->GetParentObject() ? ((CStatObj*)info.pStatObj->GetParentObject()) : (CStatObj*)info.pStatObj;
						EFileStreamingStatus eStreamingStatusParent = pParent->m_eStreamingStatus;
						bool bUnloadable = pParent->IsUnloadable();

						if (pNode->GetRenderNodeType() == eERType_Vegetation)
						{
							info.fObjScale = ((CVegetation*)pNode)->GetScale();
						}
						else if (pNode->GetRenderNodeType() == eERType_MovableBrush || pNode->GetRenderNodeType() == eERType_Brush)
						{
							Vec3 vScaleAbs = info.matObj.TransformVector(Vec3(1, 1, 1)).abs();
							info.fObjScale = min(min(vScaleAbs.x, vScaleAbs.y), vScaleAbs.z);
						}
						else
							assert(!"Undefined object type");

						info.pMat = pMaterial;

						if (info.pStatObj->m_nFlags & STATIC_OBJECT_HIDDEN)
							continue;

						info.bIndoor = pNode->GetEntityVisArea() != 0 || (pNode->GetRndFlags() & ERF_REGISTER_BY_BBOX);

						info.bVegetation = (nObjType == eERType_Vegetation);

						info.fMaxViewDist = fMaxViewDist;

						if (parrObjects)
						{
							parrObjects->Add(info);
						}
						else if (eStreamingStatusParent != ecss_Ready && bUnloadable)
						{
							// request streaming of missing meshes
							if (Cry3DEngineBase::GetCVars()->e_svoTI_VoxelizaionPostpone == 2)
							{
								info.pStatObj->UpdateStreamableComponents(0.5f, info.matObj, false, nLod);
							}

							if (Cry3DEngineBase::GetCVars()->e_svoDebug == 7)
							{
								Cry3DEngineBase::Get3DEngine()->DrawBBox(pNode->GetBBox(), Col_Red);
								IRenderAuxText::DrawLabel(pNode->GetBBox().GetCenter(), 1.3f, info.pStatObj->GetFilePath());
							}
							bSuccess = false;
						}
					}
				}
			}
		}
	}

	return bSuccess;
}

void CVoxelSegment::FindTrianglesForVoxelization(int nTID, PodArray<int>*& rpNodeTrisXYZ, bool _bThisIsAreaParent)
{
	MEMSTAT_CONTEXT(EMemStatContextTypes::MSC_Other, 0, "FindTrianglesForVoxelization");

	AABB cloudBoxWS;
	cloudBoxWS.min = m_boxOS.min + m_vSegOrigin;
	cloudBoxWS.max = m_boxOS.max + m_vSegOrigin;

	PodArray<SObjInfo> arrObjects;
	bool bThisIsAreaParent, bThisIsLowLodNode;

	CVoxelSegment::CheckCollectObjectsForVoxelization(cloudBoxWS, &arrObjects, bThisIsAreaParent, bThisIsLowLodNode, false);

	m_nodeTrisAllMerged.Reset();
	m_boxTris.Reset();

	// safety border support
	//  Vec3 vCloudSize = cloudBoxWS.GetSize();
	//    cloudBoxWS.min -= (vCloudSize/nVoxTexMaxDim)/2;
	//    cloudBoxWS.max += (vCloudSize/nVoxTexMaxDim)/2;

	//if(!m_pParentCloud)
	if (bThisIsAreaParent || bThisIsLowLodNode)
	{
		// get tris from real level geometry

		//		PodArray<SRayHitTriangle> allTrisInArea;
		//	allTrisInArea.PreAllocate(4000);

		float fStartTimeAll = GetCurAsyncTimeSec();
		//		PrintMessage("VoxelizeMeshes: starting triangle search for node id %d (size=%d)", m_nId, (int)GetBoxSize());

		SSuperMesh superMesh;
		PodArray<SMINDEX> arrVertHash[nHashDim][nHashDim][nHashDim];

		//      if(nCulled)
		//        PrintMessage("  %d objects culled", nCulled);

		// add terrain
		if (Get3DEngine()->m_bShowTerrainSurface)
		{
			float fStartTime = GetCurAsyncTimeSec();

			CTerrain* pTerrain = GetTerrain();
			int nWorldSize = pTerrain->GetTerrainSize();
			int S = pTerrain->GetHeightMapUnitSize();

			if (bThisIsLowLodNode)
				S *= 4;

			int nHalfStep = S / 2;
			//			int nInitTriCount = allTrisInArea.Count();

			SRayHitTriangle ht;
			ZeroStruct(ht);
	#ifdef FEATURE_SVO_GI_ALLOW_HQ
			ht.c[0] = ht.c[1] = ht.c[2] = Col_White;
	#endif
			ht.nOpacity = 255;
			ht.nHitObjType = HIT_OBJ_TYPE_TERRAIN;
			Plane pl;

			int I = 0, X = 0, Y = 0;

			superMesh.Clear(&arrVertHash[0][0][0]);

			for (int x = (int)cloudBoxWS.min.x; x < (int)cloudBoxWS.max.x; x += S)
			{
				for (int y = (int)cloudBoxWS.min.y; y < (int)cloudBoxWS.max.y; y += S)
				{
					if (!pTerrain->GetHole(x + nHalfStep, y + nHalfStep, 0))
					{
						// prevent surface interpolation over long edge
						bool bFlipTris = false;
						int nType10 = pTerrain->GetSurfaceTypeID(x + S, y, 0);
						int nType01 = pTerrain->GetSurfaceTypeID(x, y + S, 0);
						if (nType10 != nType01)
						{
							int nType00 = pTerrain->GetSurfaceTypeID(x, y, 0);
							int nType11 = pTerrain->GetSurfaceTypeID(x + S, y + S, 0);
							if ((nType10 == nType00 && nType10 == nType11) || (nType01 == nType00 && nType01 == nType11))
								bFlipTris = true;
						}

						if (bFlipTris)
						{
							I = 0;
							X = x + S, Y = y + 0;
							ht.v[I].Set((float)X, (float)Y, pTerrain->GetZ(X, Y, 0));
							I++;
							X = x + S, Y = y + S;
							ht.v[I].Set((float)X, (float)Y, pTerrain->GetZ(X, Y, 0));
							I++;
							X = x + 0, Y = y + 0;
							ht.v[I].Set((float)X, (float)Y, pTerrain->GetZ(X, Y, 0));
							I++;

							if (Overlap::AABB_Triangle(cloudBoxWS, ht.v[0], ht.v[1], ht.v[2]))
							{
								ht.nTriArea = SATURATEB(int(SVO_CPU_VOXELIZATION_AREA_SCALE * 0.5f * (ht.v[1] - ht.v[0]).Cross(ht.v[2] - ht.v[0]).GetLength()));
								pl.SetPlane(ht.v[0], ht.v[1], ht.v[2]);
	#ifdef FEATURE_SVO_GI_ALLOW_HQ
								ht.n = pl.n;
	#endif
								superMesh.AddSuperTriangle(ht, arrVertHash);
							}

							I = 0;
							X = x + 0, Y = y + 0;
							ht.v[I].Set((float)X, (float)Y, pTerrain->GetZ(X, Y, 0));
							I++;
							X = x + S, Y = y + S;
							ht.v[I].Set((float)X, (float)Y, pTerrain->GetZ(X, Y, 0));
							I++;
							X = x + 0, Y = y + S;
							ht.v[I].Set((float)X, (float)Y, pTerrain->GetZ(X, Y, 0));
							I++;

							if (Overlap::AABB_Triangle(cloudBoxWS, ht.v[0], ht.v[1], ht.v[2]))
							{
								ht.nTriArea = SATURATEB(int(SVO_CPU_VOXELIZATION_AREA_SCALE * 0.5f * (ht.v[1] - ht.v[0]).Cross(ht.v[2] - ht.v[0]).GetLength()));
								pl.SetPlane(ht.v[0], ht.v[1], ht.v[2]);
	#ifdef FEATURE_SVO_GI_ALLOW_HQ
								ht.n = pl.n;
	#endif
								superMesh.AddSuperTriangle(ht, arrVertHash);
							}
						}
						else
						{
							I = 0;
							X = x + 0, Y = y + 0;
							ht.v[I].Set((float)X, (float)Y, pTerrain->GetZ(X, Y, 0));
							I++;
							X = x + S, Y = y + 0;
							ht.v[I].Set((float)X, (float)Y, pTerrain->GetZ(X, Y, 0));
							I++;
							X = x + 0, Y = y + S;
							ht.v[I].Set((float)X, (float)Y, pTerrain->GetZ(X, Y, 0));
							I++;

							if (Overlap::AABB_Triangle(cloudBoxWS, ht.v[0], ht.v[1], ht.v[2]))
							{
								ht.nTriArea = SATURATEB(int(SVO_CPU_VOXELIZATION_AREA_SCALE * 0.5f * (ht.v[1] - ht.v[0]).Cross(ht.v[2] - ht.v[0]).GetLength()));
								pl.SetPlane(ht.v[0], ht.v[1], ht.v[2]);
	#ifdef FEATURE_SVO_GI_ALLOW_HQ
								ht.n = pl.n;
	#endif
								superMesh.AddSuperTriangle(ht, arrVertHash);
							}

							I = 0;
							X = x + S, Y = y + 0;
							ht.v[I].Set((float)X, (float)Y, pTerrain->GetZ(X, Y, 0));
							I++;
							X = x + S, Y = y + S;
							ht.v[I].Set((float)X, (float)Y, pTerrain->GetZ(X, Y, 0));
							I++;
							X = x + 0, Y = y + S;
							ht.v[I].Set((float)X, (float)Y, pTerrain->GetZ(X, Y, 0));
							I++;

							if (Overlap::AABB_Triangle(cloudBoxWS, ht.v[0], ht.v[1], ht.v[2]))
							{
								ht.nTriArea = SATURATEB(int(SVO_CPU_VOXELIZATION_AREA_SCALE * 0.5f * (ht.v[1] - ht.v[0]).Cross(ht.v[2] - ht.v[0]).GetLength()));
								pl.SetPlane(ht.v[0], ht.v[1], ht.v[2]);
	#ifdef FEATURE_SVO_GI_ALLOW_HQ
								ht.n = pl.n;
	#endif
								superMesh.AddSuperTriangle(ht, arrVertHash);
							}
						}
					}
				}
			}

			//      if(allTrisInLevel.Count()-nInitTriCount)
			//        PrintMessage("Terrain: %d tris added (in %.2f sec) for node id %d (size=%d)",
			//        (allTrisInLevel.Count()-nInitTriCount), GetCurAsyncTimeSec()-fStartTime,
			//        m_nId, (int)GetBoxSize());

			AUTO_MODIFYLOCK(m_superMeshLock);

			AddSuperMesh(superMesh, SVO_CPU_VOXELIZATION_OFFSET_TERRAIN);
		}

		PodArray<SRayHitTriangle> arrTris;

		// sort objects by size
		if (arrObjects.Count())
		{
			qsort(arrObjects.GetElements(), arrObjects.Count(), sizeof(arrObjects[0]), SObjInfo::Compare);
		}

		for (int d = 0; d < arrObjects.Count(); d++)
		{
			SRayHitInfo nodeHitInfo;
			nodeHitInfo.bInFirstHit = true;
			nodeHitInfo.bUseCache = false;
			nodeHitInfo.bGetVertColorAndTC = true;

			SObjInfo& info = arrObjects[d];

			nodeHitInfo.nHitTriID = HIT_UNKNOWN;
			nodeHitInfo.nHitMatID = HIT_UNKNOWN;
			nodeHitInfo.inRay.origin = info.matObjInv.TransformPoint(m_vSegOrigin);
			nodeHitInfo.inRay.direction = Vec3(0, 0, 0);
			nodeHitInfo.inReferencePoint = nodeHitInfo.inRay.origin + nodeHitInfo.inRay.direction * 0.5f;
			nodeHitInfo.fMaxHitDistance = GetBoxSize() / 2.f / info.fObjScale * sqrt(3.f);

			arrTris.Clear();
			nodeHitInfo.pHitTris = &arrTris;

			nodeHitInfo.fMinHitOpacity = GetCVars()->e_svoTI_MinVoxelOpacity;
			int nMinVoxelOpacity = (int)(GetCVars()->e_svoTI_MinVoxelOpacity * 255.f);

			float fTimeRayIntersection = Cry3DEngineBase::GetTimer()->GetAsyncCurTime();

			info.pStatObj->RayIntersection(nodeHitInfo, info.pMat);

			if (arrTris.Count())
			{
				{
					AUTO_MODIFYLOCK(CVoxelSegment::m_arrLockedMaterials.m_Lock);
					CVoxelSegment::m_arrLockedMaterials.Add(info.pMat);
					info.pMat->AddRef();
					for (int s = 0; s < info.pMat->GetSubMtlCount(); s++)
					{
						IMaterial* pSubMtl = info.pMat->GetSafeSubMtl(s);
						CVoxelSegment::m_arrLockedMaterials.Add(pSubMtl);
						pSubMtl->AddRef();
					}
				}

				superMesh.Clear(&arrVertHash[0][0][0]);

				float fEpsilon = GetCVars()->e_svoTI_ObjectsMaxViewDistance ? (VEC_EPSILON / 2) : (VEC_EPSILON / 10);

				for (int t = 0; t < arrTris.Count(); t++)
				{
					SRayHitTriangle ht = arrTris[t];

					// Workaround for over occlusion from vegetation; TODO: make thin geoemtry produce less occlusion
					if (info.bVegetation && ht.pMat)
					{
						float fMidZ = 0;
						for (int v = 0; v < 3; v++)
							fMidZ += ht.v[v].z;
						fMidZ *= 0.333f;

						SShaderItem& rSI = ht.pMat->GetShaderItem();

						if (rSI.m_pShaderResources && rSI.m_pShader)
						{
							bool bVegetationLeaves = (rSI.m_pShaderResources->GetAlphaRef() > 0.05f && rSI.m_pShader->GetShaderType() == eST_Vegetation);

							if (bVegetationLeaves)
								ht.nOpacity = min(ht.nOpacity, uint8(SATURATEB(GetCVars()->e_svoTI_VegetationMaxOpacity * 255.f)));
							else
								ht.nOpacity = min(ht.nOpacity, uint8(SATURATEB(LERP(255.f, GetCVars()->e_svoTI_VegetationMaxOpacity * 255.f, SATURATE(fMidZ * .5f)))));
						}
					}

					if (ht.nOpacity < nMinVoxelOpacity)
						continue;

					for (int v = 0; v < 3; v++)
						ht.v[v] = info.matObj.TransformPoint(ht.v[v]);

					ht.nTriArea = SATURATEB(int(SVO_CPU_VOXELIZATION_AREA_SCALE * 0.5f * (ht.v[1] - ht.v[0]).Cross(ht.v[2] - ht.v[0]).GetLength()));
					Plane pl;
					pl.SetPlane(ht.v[0], ht.v[1], ht.v[2]);
#ifdef FEATURE_SVO_GI_ALLOW_HQ
					ht.n = pl.n;
#endif
					if (!ht.v[0].IsEquivalent(ht.v[1], fEpsilon) && !ht.v[1].IsEquivalent(ht.v[2], fEpsilon) && !ht.v[2].IsEquivalent(ht.v[0], fEpsilon))
						if ((ht.nTriArea || !GetCVars()->e_svoTI_ObjectsMaxViewDistance) && Overlap::AABB_Triangle(cloudBoxWS, ht.v[0], ht.v[1], ht.v[2]))
						{
							bool bSkipUnderTerrain = Get3DEngine()->m_bShowTerrainSurface && !info.bIndoor && (!GetCVars()->e_svoTI_VoxelizeUnderTerrain || info.bVegetation);

							if (bSkipUnderTerrain)
							{
								for (int h = 0; h < 3; h++)
								{
									Vec3 vH = ht.v[h];
									vH.CheckMax(cloudBoxWS.min);
									vH.CheckMin(cloudBoxWS.max);
									if (vH.z > (GetTerrain()->GetZ((int)vH.x, (int)vH.y, 0) - 1.f) || GetTerrain()->GetHole((int)vH.x, (int)vH.y, 0))
									{
										bSkipUnderTerrain = false;
										break;
									}
								}
							}

							if (!bSkipUnderTerrain)
								superMesh.AddSuperTriangle(ht, arrVertHash);
						}
				}

				{
					AUTO_MODIFYLOCK(m_superMeshLock);

					int nMeshMemoryAllocated = m_pTrisInArea ? (m_pTrisInArea->ComputeSizeInMemory() + m_pVertInArea->ComputeSizeInMemory()) : 0;
					int nMeshMemoryToAppend = superMesh.m_pTrisInArea ? (superMesh.m_pTrisInArea->ComputeSizeInMemory() + superMesh.m_pVertInArea->ComputeSizeInMemory()) : 0;

					if ((nMeshMemoryAllocated + nMeshMemoryToAppend) > GetCVars()->e_svoMaxAreaMeshSizeKB * 1024)
					{
						// skip low importance objects
						break;
					}

					AddSuperMesh(superMesh, SVO_CPU_VOXELIZATION_OFFSET_MESH);
				}
			}

			fTimeRayIntersection = Cry3DEngineBase::GetTimer()->GetAsyncCurTime() - fTimeRayIntersection;

			if (GetCVars()->e_svoDebug)
			{
				AUTO_MODIFYLOCK(CVoxelSegment::m_cgfTimeStatsLock);
				m_cgfTimeStats[info.pStatObj] += fTimeRayIntersection;
			}
		}

	#ifdef FEATURE_SVO_GI_ALLOW_HQ
		AABB cloudBoxWS_VisAreaEx = cloudBoxWS;
		cloudBoxWS_VisAreaEx.Expand(Vec3(SVO_CPU_VOXELIZATION_OFFSET_VISAREA, SVO_CPU_VOXELIZATION_OFFSET_VISAREA, SVO_CPU_VOXELIZATION_OFFSET_VISAREA));

		// add visarea shapes
		if (!bThisIsLowLodNode)
			for (int v = 0;; v++)
			{
				superMesh.Clear(&arrVertHash[0][0][0]);

				CVisArea* pVisArea = (CVisArea*)GetVisAreaManager()->GetVisAreaById(v);
				if (!pVisArea)
					break;

				if (pVisArea->IsPortal() || !Overlap::AABB_AABB(*pVisArea->GetAABBox(), cloudBoxWS_VisAreaEx))
					continue;

				size_t nPoints = 0;
				const Vec3* pPoints = 0;
				pVisArea->GetShapePoints(pPoints, nPoints);
				float fHeight = pVisArea->GetHeight();

				SRayHitTriangle ht;
				ZeroStruct(ht);
				ht.c[0] = ht.c[1] = ht.c[2] = Col_Black;
				ht.nOpacity = 255;
				ht.nHitObjType = HIT_OBJ_TYPE_VISAREA;
				Plane pl;

				// sides
				for (size_t i = 0; i < nPoints; i++)
				{
					const Vec3& v0 = (pPoints)[i];
					const Vec3& v1 = (pPoints)[(i + 1) % nPoints];

					ht.v[0] = v0;
					ht.v[1] = v0 + Vec3(0, 0, fHeight);
					ht.v[2] = v1;

					if (Overlap::AABB_Triangle(cloudBoxWS_VisAreaEx, ht.v[0], ht.v[1], ht.v[2]))
					{
						ht.nTriArea = SATURATEB(int(SVO_CPU_VOXELIZATION_AREA_SCALE * 0.5f * (ht.v[1] - ht.v[0]).Cross(ht.v[2] - ht.v[0]).GetLength()));
						pl.SetPlane(ht.v[0], ht.v[1], ht.v[2]);
						ht.n = pl.n;
						superMesh.AddSuperTriangle(ht, arrVertHash);
					}

					ht.v[0] = v1;
					ht.v[1] = v0 + Vec3(0, 0, fHeight);
					ht.v[2] = v1 + Vec3(0, 0, fHeight);

					if (Overlap::AABB_Triangle(cloudBoxWS_VisAreaEx, ht.v[0], ht.v[1], ht.v[2]))
					{
						ht.nTriArea = SATURATEB(int(SVO_CPU_VOXELIZATION_AREA_SCALE * 0.5f * (ht.v[1] - ht.v[0]).Cross(ht.v[2] - ht.v[0]).GetLength()));
						pl.SetPlane(ht.v[0], ht.v[1], ht.v[2]);
						ht.n = pl.n;
						superMesh.AddSuperTriangle(ht, arrVertHash);
					}
				}

				// top and bottom
				for (float fH = 0; fabs(fH) <= fabs(fHeight); fH += fHeight)
				{
					for (int p = 0; p < ((int)nPoints - 2l); p++)
					{
						ht.v[0] = (pPoints)[0 + 0] + Vec3(0, 0, fH);
						ht.v[1] = (pPoints)[p + 1] + Vec3(0, 0, fH);
						ht.v[2] = (pPoints)[p + 2] + Vec3(0, 0, fH);

						if (Overlap::AABB_Triangle(cloudBoxWS_VisAreaEx, ht.v[0], ht.v[1], ht.v[2]))
						{
							ht.nTriArea = SATURATEB(int(SVO_CPU_VOXELIZATION_AREA_SCALE * 0.5f * (ht.v[1] - ht.v[0]).Cross(ht.v[2] - ht.v[0]).GetLength()));
							pl.SetPlane(ht.v[0], ht.v[1], ht.v[2]);
							ht.n = pl.n;
							superMesh.AddSuperTriangle(ht, arrVertHash);
						}
					}
				}

				AUTO_MODIFYLOCK(m_superMeshLock);

				AddSuperMesh(superMesh, SVO_CPU_VOXELIZATION_OFFSET_VISAREA);
			}
	#endif

		//		if(allTrisInArea.Count())
		//		qsort(allTrisInArea.GetElements(), allTrisInArea.Count(), sizeof(allTrisInArea[0]), CompareTriArea);

		m_nVoxTrisCounter += m_pTrisInArea ? m_pTrisInArea->Count() : 0;

		//    if(m_nodeTrisAllMerged.Count())
		//      PrintMessage("VoxelizeMeshes: %d tris found for node id %d (size=%d) in %.2f sec", m_nodeTrisAllMerged.Count(), m_nId, (int)GetBoxSize(), GetCurAsyncTimeSec() - fStartTimeAll);

		//    if(allTrisInLevel.Count())
		//      PrintMessage("VoxelizeMeshes: max triangle area: %.2f, min triangle area: %.2f", allTrisInLevel[0].triArea, allTrisInLevel.Last().triArea);

	#ifdef FEATURE_SVO_GI_USE_MESH_RT
		if (GetCVars()->e_svoTI_RT_MaxDist && gSvoEnv->m_nTexTrisPoolId)
			StoreAreaTrisIntoTriPool(allTrisInArea);
	#endif

		{
			AUTO_READLOCK(m_superMeshLock);

			if (m_pTrisInArea)
			{
				for (int t = 0; t < m_pTrisInArea->Count(); t++)
				{
					AddTriangle(m_pTrisInArea->GetAt(t), t, rpNodeTrisXYZ, m_pVertInArea);
				}
			}
		}
	}
	else if (m_pParentCloud->m_pTrisInArea)
	{
		// copy some tris from parent
		//		FUNCTION_PROFILER_3DENGINE;

		m_pTrisInArea = m_pParentCloud->m_pTrisInArea;
		m_pVertInArea = m_pParentCloud->m_pVertInArea;
		m_pMatsInArea = m_pParentCloud->m_pMatsInArea;
		m_bExternalData = true;

		AUTO_READLOCK(m_superMeshLock);

		if (gEnv->IsEditor() && (gSvoEnv->m_fStreamingStartTime < 0))
		{
			// slow but reliable for editing
			for (int trId = 0; trId < (*m_pTrisInArea).Count(); trId++)
			{
				const SRayHitTriangleIndexed& tr = (*m_pTrisInArea)[trId];

				Vec3 arrV[3] = { (*m_pVertInArea)[tr.arrVertId[0]].v, (*m_pVertInArea)[tr.arrVertId[1]].v, (*m_pVertInArea)[tr.arrVertId[2]].v };

				if (Overlap::AABB_Triangle(cloudBoxWS, arrV[0], arrV[1], arrV[2])) // 20ms
				{
					AddTriangle(tr, trId, rpNodeTrisXYZ, m_pVertInArea);
				}
			}
		}
		else
		{
			// fast but not reliable for editing - content of m_pTrisInArea may not much m_nodeTrisAllMerged
			for (int d = 0; d < m_pParentCloud->m_nodeTrisAllMerged.Count(); d++)
			{
				int trId = m_pParentCloud->m_nodeTrisAllMerged.GetAt(d);

				if (trId >= (*m_pTrisInArea).Count())
				{
					PrintMessage("%s warning: trId>=(*m_pTrisInArea).Count()", __FUNC__);
					break;
				}

				const SRayHitTriangleIndexed& tr = (*m_pTrisInArea)[trId];

				Vec3 arrV[3] = { (*m_pVertInArea)[tr.arrVertId[0]].v, (*m_pVertInArea)[tr.arrVertId[1]].v, (*m_pVertInArea)[tr.arrVertId[2]].v };

				if (Overlap::AABB_Triangle(cloudBoxWS, arrV[0], arrV[1], arrV[2])) // 20ms
				{
					AddTriangle(tr, trId, rpNodeTrisXYZ, m_pVertInArea);
				}
			}
		}
	}
}

void CVoxelSegment::AddTriangle(const SRayHitTriangleIndexed& tr, int trId, PodArray<int>*& rpNodeTrisXYZ, PodArrayRT<SRayHitVertex>* pVertInArea)
{
	Vec3 arrV[3] = { (*pVertInArea)[tr.arrVertId[0]].v, (*pVertInArea)[tr.arrVertId[1]].v, (*pVertInArea)[tr.arrVertId[2]].v };

	for (int v = 0; v < 3; v++)
		m_boxTris.Add(arrV[v]);

	AABB triBox(arrV[0], arrV[0]);
	triBox.Add(arrV[2]);
	triBox.Add(arrV[1]);

	const float fVoxSize = GetBoxSize() / (float)nVoxTexMaxDim / 2.f;
	triBox.Expand(Vec3(fVoxSize, fVoxSize, fVoxSize)); // for RT

	AABB nodeBoxWS = m_boxOS;
	nodeBoxWS.min += m_vSegOrigin;
	nodeBoxWS.max += m_vSegOrigin;

	int nDim = nVoxTexMaxDim;

	// safety border support
	Vec3 vBoxSize = nodeBoxWS.GetSize();
	//    nodeBoxWS.min -= (vBoxSize/nVoxTexMaxDim)/2;
	//    nodeBoxWS.max += (vBoxSize/nVoxTexMaxDim)/2;

	int x0 = clamp_tpl<int>((int)(((triBox.min.x - nodeBoxWS.min.x) / vBoxSize.x * nDim)), (int)0, nDim - 1);
	int x1 = clamp_tpl<int>((int)(((triBox.max.x - nodeBoxWS.min.x) / vBoxSize.x * nDim)), (int)0, nDim - 1);

	int y0 = clamp_tpl<int>((int)(((triBox.min.y - nodeBoxWS.min.y) / vBoxSize.y * nDim)), (int)0, nDim - 1);
	int y1 = clamp_tpl<int>((int)(((triBox.max.y - nodeBoxWS.min.y) / vBoxSize.y * nDim)), (int)0, nDim - 1);

	int z0 = clamp_tpl<int>((int)(((triBox.min.z - nodeBoxWS.min.z) / vBoxSize.z * nDim)), (int)0, nDim - 1);
	int z1 = clamp_tpl<int>((int)(((triBox.max.z - nodeBoxWS.min.z) / vBoxSize.z * nDim)), (int)0, nDim - 1);

	for (int z = z0; z <= z1; z++)
		for (int y = y0; y <= y1; y++)
			for (int x = x0; x <= x1; x++)
				rpNodeTrisXYZ[z * nVoxTexMaxDim * nVoxTexMaxDim + y * nVoxTexMaxDim + x].Add(trId);

	m_nodeTrisAllMerged.Add(trId);

	AABB boxWS = m_boxOS;
	boxWS.min += m_vSegOrigin;
	boxWS.max += m_vSegOrigin;

	for (int nChildId = 0; nChildId < 8; nChildId++)
	{
		if (m_dwChildTrisTest & (1 << nChildId))
			continue;

		AABB childBox = CVoxelSegment::GetChildBBox(boxWS, nChildId);

		childBox.min -= (vBoxSize / (float)nVoxTexMaxDim) / 4.0f;
		childBox.max += (vBoxSize / (float)nVoxTexMaxDim) / 4.0f;

		if (Overlap::AABB_Triangle(childBox, arrV[0], arrV[1], arrV[2]))
			m_dwChildTrisTest |= (1 << nChildId);
	}
}

void CVoxelSegment::SetVoxCamera(const CCamera& newCam)
{
	CVoxelSegment::m_voxCam = newCam;

	if (gSvoEnv)
	{
		if (gSvoEnv->m_matDvrTm.GetTranslation().x || gSvoEnv->m_matDvrTm.GetTranslation().y || gSvoEnv->m_matDvrTm.GetTranslation().z)
		{
			Matrix34 m34 = CVoxelSegment::m_voxCam.GetMatrix();
			Quat q(m34);
			Matrix33 rot(q);
			Vec3 vPos = CVoxelSegment::m_voxCam.GetPosition();
			vPos = gSvoEnv->m_matDvrTm.GetInverted().TransformPoint(vPos);
			m34.SetIdentity();
			m34.SetTranslation(vPos);
			m34 = m34 * rot;
			CVoxelSegment::m_voxCam.SetMatrix(m34);

			//    PrintMessage("cam = %.1f,%.1f,%.1f",
			//      CVoxelSegment::voxCam.GetPosition().x,
			//      CVoxelSegment::voxCam.GetPosition().y,
			//      CVoxelSegment::voxCam.GetPosition().z);
		}

		gSvoEnv->m_nDebugDrawVoxelsCounter = 0;
	}
}

ColorF CVoxelSegment::ProcessMaterial(const SRayHitTriangleIndexed& tr, const Vec3& vHitPos)
{
	ColorF colVert = Col_White;
	Vec2 vHitTC(0, 0);

	SSvoMatInfo& rMI = m_pMatsInArea->GetAt(tr.nMatID);

	SShaderItem* pShItem = rMI.pMat ? &rMI.pMat->GetShaderItem() : NULL;

	Vec3 arrV[3] = { (*m_pVertInArea)[tr.arrVertId[0]].v, (*m_pVertInArea)[tr.arrVertId[1]].v, (*m_pVertInArea)[tr.arrVertId[2]].v };

	float w0 = 0, w1 = 0, w2 = 0;
	if (GetBarycentricTC(arrV[0], arrV[1], arrV[2], w0, w1, w2, vHitPos, 2.f))
	{
		Vec2 arrT[3] = { (*m_pVertInArea)[tr.arrVertId[0]].t, (*m_pVertInArea)[tr.arrVertId[1]].t, (*m_pVertInArea)[tr.arrVertId[2]].t };

		vHitTC = arrT[0] * w0 + arrT[1] * w1 + arrT[2] * w2;

	#ifdef FEATURE_SVO_GI_ALLOW_HQ
		if (!(pShItem && pShItem->m_pShader) || pShItem->m_pShader->GetFlags2() & EF2_VERTEXCOLORS)
		{
			ColorB arrC[3] = { (*m_pVertInArea)[tr.arrVertId[0]].c, (*m_pVertInArea)[tr.arrVertId[1]].c, (*m_pVertInArea)[tr.arrVertId[2]].c };

			if (!(pShItem && pShItem->m_pShader) || pShItem->m_pShader->GetShaderType() != eST_Vegetation)
			{
				Vec4 c0 = arrC[0].toVec4();
				Vec4 c1 = arrC[1].toVec4();
				Vec4 c2 = arrC[2].toVec4();

				Vec4 colInter = c0 * w0 + c1 * w1 + c2 * w2;

				if (pShItem)
				{
					// swap r and b
					colVert.r = 1.f / 255.f * colInter.z;
					colVert.g = 1.f / 255.f * colInter.y;
					colVert.b = 1.f / 255.f * colInter.x;
				}
				else
				{
					colVert.r = 1.f / 255.f * colInter.x;
					colVert.g = 1.f / 255.f * colInter.y;
					colVert.b = 1.f / 255.f * colInter.z;
				}
			}
		}
	#endif
	}
	else
	{
		colVert = Col_DimGray;
	}

	ColorF colTex = Col_Gray;

	ColorB* pTexRgb = 0;
	int nTexWidth = 0, nTexHeight = 0;

	if (rMI.pMat)
	{
		// objects
		pTexRgb = rMI.pTexRgb;
		nTexWidth = rMI.nTexWidth;
		nTexHeight = rMI.nTexHeight;
	}
	else if (const PodArray<ColorB>* pTerrLowResTex = GetTerrain()->GetTerrainRgbLowResSystemCopy())
	{
		// terrain
		nTexWidth = nTexHeight = (int)sqrt((float)pTerrLowResTex->Count());
		pTexRgb = (ColorB*)pTerrLowResTex->GetElements();
	}

	if (pTexRgb)
	{
		if (rMI.pMat)
		{
			Vec4 vTextureAtlasInfo(0, 0, 1, 1);
			//      vTextureAtlasInfo.x = pResTexture->GetOffset(0);
			//      vTextureAtlasInfo.y = pResTexture->GetOffset(1);
			//      vTextureAtlasInfo.z = pResTexture->GetTiling(0);
			//      vTextureAtlasInfo.w = pResTexture->GetTiling(1);

			colTex = GetBilinearAt(
			  vHitTC.x * vTextureAtlasInfo.z + vTextureAtlasInfo.x,
			  vHitTC.y * vTextureAtlasInfo.w + vTextureAtlasInfo.y,
			  pTexRgb, nTexWidth, nTexHeight, 1.f / 255.f);
			colTex.srgb2rgb();

			// ignore alpha if material do not use it
			if (pShItem && pShItem->m_pShaderResources && (pShItem->m_pShaderResources->GetAlphaRef() == 0.f) && (pShItem->m_pShaderResources->GetStrengthValue(EFTT_OPACITY) == 1.f))
				colTex.a = 1;
		}
		else
		{
			// terrain tex-gen
			int nWorldSize = GetTerrain()->GetTerrainSize();
			colTex = GetBilinearAt(
			  vHitPos.y / nWorldSize,
			  vHitPos.x / nWorldSize,
			  pTexRgb, nTexWidth, nTexHeight, 1.f / 255.f);

			colTex.srgb2rgb();
			colTex *= GetTerrain()->GetTerrainTextureMultiplier(0);

			colTex.r = max(colTex.r, .02f);
			colTex.g = max(colTex.g, .02f);
			colTex.b = max(colTex.b, .02f);
			colTex.a = 1;
		}
	}

	ColorF colMat = (pShItem && pShItem->m_pShaderResources) ? pShItem->m_pShaderResources->GetColorValue(EFTT_DIFFUSE) : Col_White;

	ColorF colRes = colTex * colMat * colVert;

	return colRes;
}

float CompressTC(Vec2 tc)
{
	tc.x = (tc.x / 16.f) /* + .5f*/;
	tc.y = (tc.y / 16.f) /* + .5f*/;

	// todo: from -10 to +10, make size 4 !!!!!

	int x = CLAMP(int(tc.x * (256 * 256 - 1)), 0, (256 * 256 - 1));

	int y = CLAMP(int(tc.y * (256 * 256 - 1)), 0, (256 * 256 - 1));

	int nVal = x * 256 * 256 + y;

	return 0.1f + (float)nVal;
}

void CVoxelSegment::StoreAreaTrisIntoTriPool(PodArray<SRayHitTriangle>& allTrisInLevel)
{
	#ifdef FEATURE_SVO_GI_USE_MESH_RT
	if (allTrisInLevel.Count())
	{
		AUTO_MODIFYLOCK(gSvoEnv->m_arrRTPoolTris.m_Lock);

		for (int t = 0; t < allTrisInLevel.Count(); t++)
		{
			SRayHitTriangle& tr = allTrisInLevel[t];

			// normalize TC
			Vec2 tcMin;
			tcMin.x = min(tr.t[0].x, min(tr.t[1].x, tr.t[2].x));
			tcMin.y = min(tr.t[0].y, min(tr.t[1].y, tr.t[2].y));

			for (int v = 0; v < 3; v++)
			{
				tr.t[v].x -= floor(tcMin.x);
				tr.t[v].y -= floor(tcMin.y);
			}

			uint16 nTexW = 0, nTexH = 0;
			int* pLowResSystemCopyAtlasId = 0;
			if (tr.pMat)
			{
				CheckStoreTextureInPool(&tr.pMat->GetShaderItem(tr.nSubMatID), nTexW, nTexH, &pLowResSystemCopyAtlasId);

				if (pLowResSystemCopyAtlasId)
				{
					gSvoEnv->m_arrRTPoolTris.PreAllocate(nVoxTexPoolDimXY * nVoxTexPoolDimXY * nVoxTexPoolDimZ);

					tr.nGLobalId = gSvoEnv->m_arrRTPoolTris.Count() / 4;

					// add triangle into pool
					for (int i = 0; i < 3; i++)
						gSvoEnv->m_arrRTPoolTris.Add(Vec4(tr.v[i] - tr.n * (tr.nMatID ? SVO_CPU_VOXELIZATION_OFFSET_MESH : SVO_CPU_VOXELIZATION_OFFSET_TERRAIN), CompressTC(tr.t[i])));
					gSvoEnv->m_arrRTPoolTris.Add(Vec4(0.1f + (float)(*pLowResSystemCopyAtlasId), (float)nTexW / (float)nVoxTexPoolDimXY, 0, 0));

					gSvoEnv->m_arrRTPoolTris.m_bModified = true;
				}
			}
		}
	}
	#endif
}

void CVoxelSegment::CheckStoreTextureInPool(SShaderItem* pShItem, uint16& nTexW, uint16& nTexH, int** ppSysTexId)
{
	#ifdef FEATURE_SVO_GI_ALLOW_HQ
	static int nAtlasSlotId = 0;
	if (nAtlasSlotId >= nVoxTexPoolDimZ)
		return;

	if (SEfResTexture* pResTexture = pShItem->m_pShaderResources->GetTexture(EFTT_DIFFUSE))
	{
		if (ITexture* pITex = pResTexture->m_Sampler.m_pITex)
		{
			if (const ColorB* pTexRgbOr = pITex->GetLowResSystemCopy(nTexW, nTexH, ppSysTexId))
			{
				AUTO_MODIFYLOCK(gSvoEnv->m_arrRTPoolTexs.m_Lock);

				if ((**ppSysTexId) == 0)
				{
					// add new texture into RT pool

					if (nTexW > nVoxTexPoolDimXY || nTexH > nVoxTexPoolDimXY)
						CVoxelSegment::ErrorTerminate("CheckStoreTextureInPool");

					gSvoEnv->m_arrRTPoolTexs.PreAllocate(nVoxTexPoolDimXY * nVoxTexPoolDimXY * nVoxTexPoolDimZ, nAtlasSlotId * nVoxTexPoolDimXY * nVoxTexPoolDimXY);

					// apply high-pass filter
					ColorB* pTexRgbHP = new ColorB[nTexW * nTexH];
					ColorF* pBlurredF = new ColorF[nTexW * nTexH];

					// make blurred copy
					for (int x = 0; x < nTexW; x++)
					{
						for (int y = 0; y < nTexH; y++)
						{
							ColorF colAver(0, 0, 0, 0);

							int nRange = 8;

							for (int i = -nRange; i <= nRange; i += 2)
							{
								for (int j = -nRange; j <= nRange; j += 2)
								{
									int X = (x + i) & (nTexW - 1);
									int Y = (y + j) & (nTexH - 1);

									colAver.r += pTexRgbOr[X * nTexH + Y].r;
									colAver.g += pTexRgbOr[X * nTexH + Y].g;
									colAver.b += pTexRgbOr[X * nTexH + Y].b;
									colAver.a++;
								}
							}

							pBlurredF[x * nTexH + y] = colAver / colAver.a;
						}
					}

					// get difference between blurred and original
					for (int x = 0; x < nTexW; x++)
					{
						for (int y = 0; y < nTexH; y++)
						{
							ColorF colF;
							colF.r = pTexRgbOr[x * nTexH + y].r;
							colF.g = pTexRgbOr[x * nTexH + y].g;
							colF.b = pTexRgbOr[x * nTexH + y].b;
							colF.a = pTexRgbOr[x * nTexH + y].a;

							colF = (colF - pBlurredF[x * nTexH + y] + 127.5f);

							pTexRgbHP[x * nTexH + y].r = SATURATEB((int)colF.r);
							pTexRgbHP[x * nTexH + y].g = SATURATEB((int)colF.g);
							pTexRgbHP[x * nTexH + y].b = SATURATEB((int)colF.b);
							pTexRgbHP[x * nTexH + y].a = 255;
						}
					}

					for (int nLine = 0; nLine < nTexH; nLine++)
						memcpy(gSvoEnv->m_arrRTPoolTexs.GetElements() + nAtlasSlotId * (nVoxTexPoolDimXY * nVoxTexPoolDimXY) + nLine * nVoxTexPoolDimXY, pTexRgbHP + nLine * nTexW, nTexW * sizeof(ColorB));

					delete[] pBlurredF;
					delete[] pTexRgbHP;

					(**ppSysTexId) = nAtlasSlotId + 1;
					nAtlasSlotId++;

					gSvoEnv->m_arrRTPoolTexs.m_bModified = true;
				}
			}
		}
	}
	#endif
}

int CVoxelSegment::StoreIndicesIntoPool(const PodArray<int>& nodeTInd, int& nCountStored)
{
	#ifdef FEATURE_SVO_GI_USE_MESH_RT
	if (nodeTInd.Count() && GetBoxSize() == Cry3DEngineBase::GetCVars()->e_svoMinNodeSize)
	{
		AUTO_MODIFYLOCK(gSvoEnv->m_arrRTPoolInds.m_Lock);

		int nStartId = gSvoEnv->m_arrRTPoolInds.Count();

		gSvoEnv->m_arrRTPoolInds.PreAllocate(nVoxTexPoolDimXY * nVoxTexPoolDimXY * nVoxTexPoolDimZ);

		for (int t = 0; t < nodeTInd.Count() && t < 255; t++)
		{
			int trId = nodeTInd[t];
			SRayHitTriangle& tr = (*m_pTrisInArea)[trId];
			int nId = tr.nGLobalId;

			ColorB valOut;
			valOut.r = nId & 255;
			nId /= 256;
			valOut.g = nId & 255;
			nId /= 256;
			valOut.b = nId & 255;
			nId /= 256;
			valOut.a = nId & 255;
			nId /= 256;
			gSvoEnv->m_arrRTPoolInds.Add(valOut);
		}

		gSvoEnv->m_arrRTPoolInds.m_bModified = true;

		nCountStored = gSvoEnv->m_arrRTPoolInds.Count() - nStartId;

		return nStartId;
	}

	nCountStored = 0;
	#endif

	return 0;
}

void CVoxelSegment::DebugDrawVoxels()
{
	if (Cry3DEngineBase::GetCVars()->e_svoDebug == 6 || Cry3DEngineBase::GetCVars()->e_svoDebug == 3)
	{
		if (m_pVoxOpacit && CVoxelSegment::m_voxCam.IsAABBVisible_F(m_pNode->m_nodeBox) && gSvoEnv->m_nDebugDrawVoxelsCounter < 100000)
		{
			Vec4 voxNodeData[nVoxNodMaxDim * nVoxNodMaxDim * nVoxNodMaxDim];
			ZeroStruct(voxNodeData);
			voxNodeData[0] = Vec4(m_boxOS.min + m_vSegOrigin, 0);
			voxNodeData[1] = Vec4(m_boxOS.max + m_vSegOrigin, 0);
			voxNodeData[0] = voxNodeData[0] + Vec4(Vec3((float)m_vCropBoxMin.x / nVoxTexMaxDim, (float)m_vCropBoxMin.y / nVoxTexMaxDim, (float)m_vCropBoxMin.z / nVoxTexMaxDim) * GetBoxSize(), 0);
			voxNodeData[1] = voxNodeData[0] + Vec4(Vec3((float)m_vCropTexSize.x / nVoxTexMaxDim, (float)m_vCropTexSize.y / nVoxTexMaxDim, (float)m_vCropTexSize.z / nVoxTexMaxDim) * GetBoxSize(), 0);

			if (Cry3DEngineBase::GetCVars()->e_svoDebug == 3)
			{
				Vec3 vMin = *(Vec3*)&(voxNodeData[0]);
				Vec3 vMax = *(Vec3*)&(voxNodeData[1]);
				Cry3DEngineBase::DrawBBox(AABB(vMin, vMax));
				return;
			}

			for (int x = 0; x < m_vCropTexSize.x; x++)
				for (int y = 0; y < m_vCropTexSize.y; y++)
					for (int z = 0; z < m_vCropTexSize.z; z++)
					{
						int id = z * m_vCropTexSize.x * m_vCropTexSize.y + y * m_vCropTexSize.x + x;
						ColorB& opaOutFin = m_pVoxOpacit[id];

						if (opaOutFin.r || opaOutFin.g || opaOutFin.b)
						{
							Vec4 vMin = voxNodeData[0] + (voxNodeData[1] - voxNodeData[0]) * Vec4((float) x / m_vCropTexSize.x, (float) y / m_vCropTexSize.y, (float) z / m_vCropTexSize.z, 1);
							Vec4 vMax = voxNodeData[0] + (voxNodeData[1] - voxNodeData[0]) * Vec4((float) (x + 1) / m_vCropTexSize.x, (float) (y + 1) / m_vCropTexSize.y, (float) (z + 1) / m_vCropTexSize.z, 1);

							//          // safety border support
							//          if(Cry3DEngineBase::GetCVars()->e_svoDebug == 6)
							//          {
							//            Vec4 vCenter(m_vOrigin.x,m_vOrigin.y,m_vOrigin.z,0);
							//            vMin += (vMin - vCenter)/(nVoxTexMaxDim/2)/2;
							//            vMax += (vMax - vCenter)/(nVoxTexMaxDim/2)/2;
							//          }

							AABB voxBox;
							voxBox.min.Set(vMin.x, vMin.y, vMin.z);
							voxBox.max.Set(vMax.x, vMax.y, vMax.z);

							if (!CVoxelSegment::m_voxCam.IsAABBVisible_F(voxBox))
								continue;

							voxBox.Expand(-voxBox.GetSize() * 0.025f);

							Cry3DEngineBase::DrawBBox(voxBox,
							                          ColorF(
							                            (GetBoxSize() == GetCVars()->e_svoMinNodeSize * 1),
							                            (GetBoxSize() == GetCVars()->e_svoMinNodeSize * 2),
							                            (GetBoxSize() == GetCVars()->e_svoMinNodeSize * 4),
							                            1));

							gSvoEnv->m_nDebugDrawVoxelsCounter++;
						}
					}
		}
	}
}

void CVoxelSegment::ErrorTerminate(const char* format, ...)
{
	va_list args;
	va_start(args, format);
	char szText[512];
	cry_vsprintf(szText, format, args);
	va_end(args);

	#if CRY_PLATFORM_WINDOWS && CRY_PLATFORM_64BIT
	if (!gEnv->pGameFramework)
	{
		// quick terminate if in developer mode
		char szTextFull[512];
		cry_sprintf(szTextFull, "%s\nTerminate process?", szText);
		if (CryMessageBox(szTextFull, "3DEngine fatal error", eMB_YesCancel) == eQR_Yes)
			TerminateProcess(GetCurrentProcess(), 0);
		else
			return;
	}
	#endif

	gEnv->pSystem->FatalError(szText); // it causes 8 message boxes but we need just one
}

const float fSvoSuperMeshHashScale = .1f;

int SSuperMesh::FindVertex(const Vec3& rPos, const Vec2 rTC, PodArray<SMINDEX> arrVertHash[nHashDim][nHashDim][nHashDim], PodArrayRT<SRayHitVertex>& vertsInArea)
{
	Vec3i vPosI0 = rPos / fSvoSuperMeshHashScale - Vec3(VEC_EPSILON, VEC_EPSILON, VEC_EPSILON);
	Vec3i vPosI1 = rPos / fSvoSuperMeshHashScale + Vec3(VEC_EPSILON, VEC_EPSILON, VEC_EPSILON);

	for (int x = vPosI0.x; x <= vPosI1.x; x++)
		for (int y = vPosI0.y; y <= vPosI1.y; y++)
			for (int z = vPosI0.z; z <= vPosI1.z; z++)
			{
				Vec3i vPosI(x, y, z);

				PodArray<SMINDEX>& rSubIndices = arrVertHash[vPosI.x & (nHashDim - 1)][vPosI.y & (nHashDim - 1)][vPosI.z & (nHashDim - 1)];

				for (int ii = 0; ii < rSubIndices.Count(); ii++)
				{
					if (vertsInArea[rSubIndices[ii]].v.IsEquivalent(rPos))
						if (vertsInArea[rSubIndices[ii]].t.IsEquivalent(rTC))
							return rSubIndices[ii];
				}
			}

	return -1;
}

int SSuperMesh::AddVertex(const SRayHitVertex& rVert, PodArray<SMINDEX> arrVertHash[nHashDim][nHashDim][nHashDim], PodArrayRT<SRayHitVertex>& vertsInArea)
{
	Vec3 vPosS = rVert.v / fSvoSuperMeshHashScale;

	Vec3i vPosI(int(floor(vPosS.x)), int(floor(vPosS.y)), int(floor(vPosS.z)));

	PodArray<SMINDEX>& rSubIndices = arrVertHash[vPosI.x & (nHashDim - 1)][vPosI.y & (nHashDim - 1)][vPosI.z & (nHashDim - 1)];

	rSubIndices.Add(vertsInArea.Count());

	vertsInArea.Add(rVert);

	return vertsInArea.Count() - 1;
}

void SSuperMesh::AddSuperTriangle(SRayHitTriangle& htIn, PodArray<SMINDEX> arrVertHash[nHashDim][nHashDim][nHashDim])
{
	if (!m_pTrisInArea)
	{
		m_pTrisInArea = new PodArrayRT<SRayHitTriangleIndexed>;
		m_pVertInArea = new PodArrayRT<SRayHitVertex>;
		m_pMatsInArea = new PodArrayRT<SSvoMatInfo>;
		m_pFaceNormals = new PodArrayRT<Vec3>;
	}

	if (m_pVertInArea->Count() + 3 > (SMINDEX) ~0)
		return;

	SRayHitTriangleIndexed htOut;

	#ifdef FEATURE_SVO_GI_ALLOW_HQ
	htOut.vFaceNorm = htIn.n;
	#endif

	SSvoMatInfo matInfo;
	matInfo.pMat = htIn.pMat;
	int nMatId = m_pMatsInArea->Find(matInfo);
	if (nMatId < 0)
	{
		nMatId = m_pMatsInArea->Count();

		// stat obj, get access to texture RGB data
		if (htIn.pMat)
		{
			SShaderItem* pShItem = &htIn.pMat->GetShaderItem();
			int* pLowResSystemCopyAtlasId = 0;
			if (pShItem->m_pShaderResources)
			{
				SEfResTexture* pResTexture = pShItem->m_pShaderResources->GetTexture(EFTT_DIFFUSE);
				if (pResTexture)
				{
					ITexture* pITex = pResTexture->m_Sampler.m_pITex;
					if (pITex)
					{
						AUTO_MODIFYLOCK(CVoxelSegment::m_arrLockedTextures.m_Lock);
						CVoxelSegment::m_arrLockedTextures.Add(pITex);
						matInfo.pTexRgb = (ColorB*)pITex->GetLowResSystemCopy(matInfo.nTexWidth, matInfo.nTexHeight, &pLowResSystemCopyAtlasId);
						pITex->AddRef();
					}
				}
			}
		}

		m_pMatsInArea->Add(matInfo);
	}
	htOut.nMatID = nMatId;

	#ifdef FEATURE_SVO_GI_USE_MESH_RT
	htOut.nGLobalId = htIn.nGLobalId;
	#endif
	htOut.nTriArea = htIn.nTriArea;
	htOut.nOpacity = htIn.nOpacity;
	htOut.nHitObjType = htIn.nHitObjType;

	for (int v = 0; v < 3; v++)
	{
		int nVID = FindVertex(htIn.v[v], htIn.t[v], arrVertHash, *m_pVertInArea);

		if (nVID < 0)
		{
			SRayHitVertex hv;
			hv.v = htIn.v[v];
			hv.t = htIn.t[v];
	#ifdef FEATURE_SVO_GI_ALLOW_HQ
			hv.c = htIn.c[v];
	#endif
			nVID = AddVertex(hv, arrVertHash, *m_pVertInArea);
		}

		htOut.arrVertId[v] = nVID;
	}

	m_pTrisInArea->Add(htOut);
	m_pFaceNormals->Add(htIn.n);
}

void SSuperMesh::AddSuperMesh(SSuperMesh& smIn, float fVertexOffset)
{
	MEMSTAT_CONTEXT(EMemStatContextTypes::MSC_Other, 0, "AddSuperMesh");

	if (!smIn.m_pVertInArea || !smIn.m_pTrisInArea || !smIn.m_pTrisInArea->Count())
		return;

	if ((m_pVertInArea ? m_pVertInArea->Count() : 0) + smIn.m_pVertInArea->Count() > (SMINDEX) ~0)
		return;

	PodArrayRT<Vec3> vertInNormals;
	vertInNormals.PreAllocate(smIn.m_pVertInArea->Count(), smIn.m_pVertInArea->Count());

	for (int t = 0; t < smIn.m_pTrisInArea->Count(); t++)
	{
		SRayHitTriangleIndexed tr = smIn.m_pTrisInArea->GetAt(t);

		for (int v = 0; v < 3; v++)
			vertInNormals[tr.arrVertId[v]] += smIn.m_pFaceNormals->GetAt(t);
	}

	for (int v = 0; v < smIn.m_pVertInArea->Count(); v++)
	{
		smIn.m_pVertInArea->GetAt(v).v += vertInNormals[v].GetNormalized() * fVertexOffset;
		m_boxTris.Add(smIn.m_pVertInArea->GetAt(v).v);
	}

	if (!m_pTrisInArea)
	{
		m_pTrisInArea = new PodArrayRT<SRayHitTriangleIndexed>;
		m_pVertInArea = new PodArrayRT<SRayHitVertex>;
		m_pMatsInArea = new PodArrayRT<SSvoMatInfo>;
	}

	int nNumVertBefore = m_pVertInArea->Count();

	m_pTrisInArea->PreAllocate(m_pTrisInArea->Count() + smIn.m_pTrisInArea->Count());

	for (int t = 0; t < smIn.m_pTrisInArea->Count(); t++)
	{
		SRayHitTriangleIndexed tr = smIn.m_pTrisInArea->GetAt(t);

		for (int v = 0; v < 3; v++)
			tr.arrVertId[v] += nNumVertBefore;

		SSvoMatInfo matInfo;
		matInfo.pMat = (*smIn.m_pMatsInArea)[tr.nMatID].pMat;

		int nMatId = m_pMatsInArea->FindReverse(matInfo);
		if (nMatId < 0)
		{
			nMatId = m_pMatsInArea->Count();

			// stat obj, get access to texture RGB data
			if (matInfo.pMat)
			{
				SShaderItem* pShItem = &matInfo.pMat->GetShaderItem();
				int* pLowResSystemCopyAtlasId = 0;
				if (pShItem->m_pShaderResources)
				{
					SEfResTexture* pResTexture = pShItem->m_pShaderResources->GetTexture(EFTT_DIFFUSE);
					if (pResTexture)
					{
						ITexture* pITex = pResTexture->m_Sampler.m_pITex;
						if (pITex)
						{
							AUTO_MODIFYLOCK(CVoxelSegment::m_arrLockedTextures.m_Lock);
							CVoxelSegment::m_arrLockedTextures.Add(pITex);
							matInfo.pTexRgb = (ColorB*)pITex->GetLowResSystemCopy(matInfo.nTexWidth, matInfo.nTexHeight, &pLowResSystemCopyAtlasId);
							pITex->AddRef();
						}
					}
				}
			}

			m_pMatsInArea->Add(matInfo);
		}
		tr.nMatID = nMatId;

		m_pTrisInArea->Add(tr);
	}

	m_pVertInArea->AddList(*smIn.m_pVertInArea);

	if (fVertexOffset == SVO_CPU_VOXELIZATION_OFFSET_TERRAIN)
	{
		AddSuperMesh(smIn, -1.f);
	}

	smIn.Clear(NULL);
}

SSuperMesh::SSuperMesh()
{
	ZeroStruct(*this);
}

SSuperMesh::~SSuperMesh()
{
	if (!m_bExternalData)
	{
		SAFE_DELETE(m_pTrisInArea);
		SAFE_DELETE(m_pVertInArea);
		SAFE_DELETE(m_pMatsInArea);
		SAFE_DELETE(m_pFaceNormals);
	}

	m_bExternalData = false;
	m_boxTris.Reset();
}

void SSuperMesh::Clear(PodArray<SMINDEX>* parrVertHash)
{
	if (!m_bExternalData && m_pTrisInArea)
	{
		m_pTrisInArea->Clear();
		m_pVertInArea->Clear();
		m_pMatsInArea->Clear();
		m_pFaceNormals->Clear();
	}

	if (parrVertHash)
		for (int i = 0; i < nHashDim * nHashDim * nHashDim; i++)
			parrVertHash[i].Clear();

	m_bExternalData = false;
	m_boxTris.Reset();
}

#endif
