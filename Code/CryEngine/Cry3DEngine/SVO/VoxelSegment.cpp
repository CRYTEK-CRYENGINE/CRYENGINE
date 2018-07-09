// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

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

	#pragma warning(push)
	#pragma warning(disable: 4244) //conversion' conversion from 'type1' to 'type2', possible loss of data

	#define SVO_OFFSET_MESH        0
	#define SVO_OFFSET_TERRAIN     -0.04f
	#define SVO_OFFSET_VISAREA     (Cry3DEngineBase::GetCVars()->e_svoMinNodeSize / (float)SVO_VOX_BRICK_MAX_SIZE)
	#define SVO_POOL_SIZE_MB       (12 * 1024)
	#define SVO_AREA_SCALE         200.f
	#define SVO_DIST_TO_SURF_RANGE 4
	#define SVO_NODES_POOL_DIM_XY  (SVO_NODE_BRICK_SIZE * SVO_ATLAS_DIM_MAX_XY)
	#define SVO_NODES_POOL_DIM_Z   (SVO_NODE_BRICK_SIZE * SVO_ATLAS_DIM_MAX_Z)
	#define SVO_MAX_TRIS_PER_VOXEL 512
	#define SVO_PACK_TO_16_BIT     false // disabled because causes not enough occlusion

CBlockPacker3D* CVoxelSegment::m_pBlockPacker = 0;
CCamera CVoxelSegment::m_voxCam;
extern CSvoEnv* gSvoEnv;
int CVoxelSegment::m_addPolygonToSceneCounter = 0;
int CVoxelSegment::m_checkReadyCounter = 0;
int CVoxelSegment::m_segmentsCounter = 0;
int CVoxelSegment::m_postponedCounter = 0;
int CVoxelSegment::m_currPassMainFrameID = 0;
int CVoxelSegment::m_maxBrickUpdates = 128;
int CVoxelSegment::m_nextSegmentId = 0;
int CVoxelSegment::m_poolUsageBytes = 0;
int CVoxelSegment::m_poolUsageItems = 0;
int CVoxelSegment::m_streamingTasksInProgress = 0;
int CVoxelSegment::m_svoDataPoolsCounter = 0;
bool CVoxelSegment::m_bUpdateBrickRenderDataPostponed = 0;
int CVoxelSegment::m_updatesInProgressBri = 0;
int CVoxelSegment::m_updatesInProgressTex = 0;
int CVoxelSegment::m_voxTrisCounter = 0;
int CVoxelSegment::m_voxTexPoolDimXY = 0;
int CVoxelSegment::m_voxTexPoolDimZ = 0;
bool CVoxelSegment::m_bExportMode = false;
int CVoxelSegment::m_exportVisitedAreasCounter = 0;
int CVoxelSegment::m_exportVisitedNodesCounter = 0;
bool CVoxelSegment::m_bExportAbortRequested = false;
PodArray<C3DEngine::SLayerActivityInfo> CVoxelSegment::m_arrObjectLayersInfo;
uint CVoxelSegment::m_arrObjectLayersInfoVersion = 0;

std::map<CStatObj*, float> CVoxelSegment::m_cgfTimeStats;
CryReadModifyLock CVoxelSegment::m_cgfTimeStatsLock;

CLockedMap<ITexture*, _smart_ptr<ITexture>> CVoxelSegment::m_arrLockedTextures;
CLockedMap<IMaterial*, _smart_ptr<IMaterial>> CVoxelSegment::m_arrLockedMaterials;

PodArray<CVoxelSegment*> CVoxelSegment::m_arrLoadedSegments;
SRenderingPassInfo* CVoxelSegment::m_pCurrPassInfo = 0;

DECLARE_JOB("VoxelSegmentFileDecompress", TDecompressVoxStreamItemJob, CVoxStreamEngine::DecompressVoxStreamItem);
DECLARE_JOB("VoxelSegmentBuildVoxels", TBuildVoxelsJob, CVoxelSegment::BuildVoxels);

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Voxel Stream Engine Thread
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

CVoxStreamEngine::CVoxStreamEngineThread::CVoxStreamEngineThread(CVoxStreamEngine* pStreamingEngine) : m_pStreamingEngine(pStreamingEngine)
	, m_bRun(true)
{
}

void CVoxStreamEngine::CVoxStreamEngineThread::ThreadEntry()
{
	MEMSTAT_CONTEXT(EMemStatContextTypes::MSC_Other, 0, "VoxStreamEngine");

	while (m_bRun)
	{
		m_pStreamingEngine->m_fileReadSemaphore.Acquire();

		if (!m_bRun)
		{
			break;
		}

		SVoxStreamItem pItem;
		if (m_pStreamingEngine->m_arrForFileRead.dequeue(pItem))
		{
			if (Cry3DEngineBase::GetCVars()->e_svoMaxStreamRequests > 4)
			{
				TDecompressVoxStreamItemJob job(pItem);
				job.SetClassInstance(m_pStreamingEngine);
				job.SetPriorityLevel(JobManager::eStreamPriority);
				job.Run();
			}
			else
			{
				m_pStreamingEngine->DecompressVoxStreamItem(pItem);
			}
		}
	}
}

void CVoxStreamEngine::CVoxStreamEngineThread::SignalStopWork()
{
	m_bRun = false;
	m_pStreamingEngine->m_fileReadSemaphore.Release();
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Voxel Stream Engine
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

CVoxStreamEngine::CVoxStreamEngine() : m_arrForFileRead(512)
	, m_arrForSyncCallBack(512)
	, m_fileReadSemaphore(512)
{
	const int numThreads = gEnv->pConsole->GetCVar("e_svoTI_NumStreamingThreads")->GetIVal();
	for (int i = 0; i < numThreads; ++i)
	{
		CVoxStreamEngineThread* pStreamingThread = new CVoxStreamEngineThread(this);
		if (!gEnv->pThreadManager->SpawnThread(pStreamingThread, "VoxelStreamingWorker_%u", i))
		{
			CryFatalError("Error spawning \"VoxelStreamingWorker_%u\" thread.", i);
			delete pStreamingThread;
		}
		else
		{
			m_workerThreads.push_back(pStreamingThread);
		}
	}
}

CVoxStreamEngine::~CVoxStreamEngine()
{
	for (CVoxStreamEngineThread* pWorker : m_workerThreads)
	{
		pWorker->SignalStopWork();
	}

	for (CVoxStreamEngineThread* pWorker : m_workerThreads)
	{
		gEnv->pThreadManager->JoinThread(pWorker, eJM_Join);
		delete pWorker;
	}
	m_workerThreads.clear();

	ProcessSyncCallBacks();
}

void CVoxStreamEngine::DecompressVoxStreamItem(const SVoxStreamItem item)
{
	item.pObj->StreamAsyncOnComplete(0, 0);
	const bool ret = m_arrForSyncCallBack.enqueue(item);
	CRY_ASSERT_MESSAGE(ret, "CVoxStreamEngine::m_arrForSyncCallBack is not big enough.");
}

void CVoxStreamEngine::ProcessSyncCallBacks()
{

	SVoxStreamItem item;
	while (m_arrForSyncCallBack.dequeue(item))
	{
		item.pObj->StreamOnComplete(0, 0);
	}
}

bool CVoxStreamEngine::StartRead(CVoxelSegment* pObj, int64 fileOffset, int bytesToRead)
{
	FUNCTION_PROFILER_3DENGINE;

	if (m_arrForFileRead.enqueue({ pObj, GetCurrPassMainFrameID() / 10 }))
	{
		m_fileReadSemaphore.Release();
		return true;
	}

	return false;
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
	m_maxAlphaInBrick = 0;
	m_allocatedAtlasOffset = -2;
	m_lastTexUpdateFrameId = m_lastRendFrameId = 0;
	m_fileStreamOffset64 = m_fileStreamSize = -1;
	m_bChildOffsetsDirty = 0;
	m_segmentsCounter++;
	m_segmentID = -1;
	m_bStatLightsChanged = 0;
	m_solidVoxelsNum = 0;
	m_pBlockInfo = 0;
	m_pNode = pNode;
	m_pParentCloud = 0;
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

	if ((p[0]->m_lastRendFrameId + arrNodeSize[0]) > (p[1]->m_lastRendFrameId + arrNodeSize[1]))
		return 1;
	if ((p[0]->m_lastRendFrameId + arrNodeSize[0]) < (p[1]->m_lastRendFrameId + arrNodeSize[1]))
		return -1;

	if (p[0] > p[1])
		return 1;
	if (p[0] < p[1])
		return -1;

	return 0;
}

int CVoxelSegment::GetBrickPoolUsageMB()
{
	return gSvoEnv->m_brickSubSetAllocator.GetCapacity() * SVO_VOX_BRICK_MAX_SIZE * SVO_VOX_BRICK_MAX_SIZE * SVO_VOX_BRICK_MAX_SIZE * sizeof(ColorB) / 1024 / 1024;
}

int CVoxelSegment::GetBrickPoolUsageLoadedMB()
{
	return gSvoEnv->m_brickSubSetAllocator.GetCount() * SVO_VOX_BRICK_MAX_SIZE * SVO_VOX_BRICK_MAX_SIZE * SVO_VOX_BRICK_MAX_SIZE * sizeof(ColorB) / 1024 / 1024;
}

void CVoxelSegment::CheckAllocateBrick(ColorB*& pPtr, int elemsNum, bool bClean)
{
	if (pPtr)
	{
		if (bClean)
			memset(pPtr, 0, SVO_VOX_BRICK_MAX_SIZE * SVO_VOX_BRICK_MAX_SIZE * SVO_VOX_BRICK_MAX_SIZE * sizeof(ColorB));

		return;
	}

	{
		AUTO_LOCK(m_csLockBrick);

		pPtr = (ColorB*)gSvoEnv->m_brickSubSetAllocator.GetNewElement();
	}

	memset(pPtr, 0, SVO_VOX_BRICK_MAX_SIZE * SVO_VOX_BRICK_MAX_SIZE * SVO_VOX_BRICK_MAX_SIZE * sizeof(ColorB));
}

void CVoxelSegment::FreeBrick(ColorB*& pPtr)
{
	if (pPtr)
	{
		AUTO_LOCK(m_csLockBrick);

		gSvoEnv->m_brickSubSetAllocator.ReleaseElement((SBrickSubSet*)pPtr);

		pPtr = 0;
	}
}

int GetBrickDataSize(ColorB*& pPtr)
{
	return pPtr ? SVO_VOX_BRICK_MAX_SIZE * SVO_VOX_BRICK_MAX_SIZE * SVO_VOX_BRICK_MAX_SIZE * sizeof(ColorB) : 0;
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// CVoxelSegment
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

CVoxelSegment::~CVoxelSegment()
{
	m_arrLoadedSegments.Delete(this);
	m_segmentsCounter--;
	FreeAllBrickData();
	FreeRenderData();
}

void CVoxelSegment::RenderMesh(PodArray<SVF_P3F_C4B_T2F>& arrVertsOut)
{
	m_lastRendFrameId = GetCurrPassMainFrameID();

	if (m_eStreamingStatus == ecss_Ready)
	{
		if (!CheckUpdateBrickRenderData(true))
			return;

		// accumulate DVR proxy mesh
		if ((GetCVars()->e_svoDVR == 10 && m_maxAlphaInBrick > 0.05) || GetCVars()->e_svoTI_Active)
			if (GetBoxSize() <= Cry3DEngineBase::GetCVars()->e_svoMaxNodeSize)
			{
				arrVertsOut.Add(m_vertForGS);
				m_addPolygonToSceneCounter++;
			}

		DebugDrawVoxels();
	}
}

bool CVoxelSegment::LoadVoxels(byte* pDataRead, int dataSize)
{
	MEMSTAT_CONTEXT(EMemStatContextTypes::MSC_Other, 0, "LoadVoxels");

	byte* pData = (byte*)pDataRead;

	SVoxSegmentFileHeader* pHeader = (SVoxSegmentFileHeader*)pData;

	m_boxOS = m_pNode->m_nodeBox;
	m_boxOS.min -= m_vSegOrigin;
	m_boxOS.max -= m_vSegOrigin;

	pData += sizeof(SVoxSegmentFileHeader);

	FreeAllBrickData();

	m_vCropTexSize.x = pHeader->cropTexSize.x;
	m_vCropTexSize.y = pHeader->cropTexSize.y;
	m_vCropTexSize.z = pHeader->cropTexSize.z;

	m_vCropBoxMin.x = pHeader->cropBoxMin.x;
	m_vCropBoxMin.y = pHeader->cropBoxMin.y;
	m_vCropBoxMin.z = pHeader->cropBoxMin.z;

	int texDataSize = (m_vCropTexSize.x * m_vCropTexSize.y * m_vCropTexSize.z * sizeof(ColorB));

	if (SVO_PACK_TO_16_BIT)
	{
		texDataSize /= 2;
	}

	if (gSvoEnv->m_voxTexFormat == eTF_BC3)
	{
		Vec3i vDxtDim = GetDxtDim();
		texDataSize = (vDxtDim.x * vDxtDim.y * vDxtDim.z * sizeof(ColorB)) / 4;
	}

	if (int dataSize = texDataSize)
	{
		assert(m_objLayerMap.empty());

		// for every layer
		for (int s = 0; s < pHeader->cropTexSize.w; s++)
		{
			uint32 nLayerId = *((uint32*)pData);
			pData += sizeof(uint32);

			SVoxBrick voxData;

			assert(GetSubSetsNum() == pHeader->cropBoxMin.w); // number of subset in file must be compatible with current GI settings

			// for every subset
			for (int s = 0; s < GetSubSetsNum(); s++)
			{
				CheckAllocateBrick(voxData.pData[s], dataSize / sizeof(ColorB));

				if (SVO_PACK_TO_16_BIT)
				{
					byte* pDataOut = (byte*)voxData.pData[s];

					for (int i = 0; i < texDataSize; i++)
					{
						(*pDataOut) = (((*pData) >> 0) & 15) << 4;
						(*pDataOut) = SATURATEB(int(powf(float((*pDataOut)) / 255.f, 2.f) * 255.f));
						pDataOut++;

						(*pDataOut) = (((*pData) >> 4) & 15) << 4;
						(*pDataOut) = SATURATEB(int(powf(float((*pDataOut)) / 255.f, 2.f) * 255.f));
						pDataOut++;

						pData++;
					}
				}
				else
				{
					memcpy(voxData.pData[s], pData, dataSize);

					pData += dataSize;
				}

				if (gSvoEnv->m_voxTexFormat == eTF_R8G8B8A8)
				{
					uint8 nMaxAlphaInBrick = 0;

					// swap r and b
					int pixNum = m_vCropTexSize.x * m_vCropTexSize.y * m_vCropTexSize.z;

					for (int p = 0; p < pixNum; p++)
					{
						if (voxData.pData[s])
						{
							ColorB& col = voxData.pData[s][p];
							std::swap(col.r, col.b);

							if (s == 0)
							{
								nMaxAlphaInBrick = max(nMaxAlphaInBrick, col.a);
							}
						}
					}

					m_maxAlphaInBrick = 1.f / 255.f * (float)nMaxAlphaInBrick;
				}
			}

			m_objLayerMap[nLayerId] = voxData;
		}

		CombineLayers();
	}

	assert((pData - pDataRead) == dataSize);

	return true;
}

Vec3i CVoxelSegment::GetDxtDim()
{
	if (m_vCropTexSize.x * m_vCropTexSize.y * m_vCropTexSize.z == 0)
		return Vec3i(0, 0, 0);

	Vec3i vDxtSize = m_vCropTexSize;

	// adjust X and Y sizes for DXT
	for (int n = 0; n < 2; n++)
	{
		while (1)
		{
			if (((vDxtSize[n]) % 4) == 0)
				break;

			vDxtSize[n]++;
		}
	}

	return vDxtSize;
}

void CVoxelSegment::FreeAllBrickData()
{
	FreeBrickLayers();

	for (int s = 0; s < GetSubSetsNum(); s++)
	{
		FreeBrick(m_voxData.pData[s]);
	}

	m_nodeTrisAllMerged.Reset();
	m_boxTris.Reset();

	if (m_pTrisInArea && (!m_pParentCloud || !m_pParentCloud->m_pTrisInArea))
	{
		m_pTrisInArea->Reset();
		m_pVertInArea->Reset();
		m_pMatsInArea->Reset();
	}
}

void CVoxelSegment::FreeBrickLayers()
{
	for (auto& it : m_objLayerMap)
	{
		ObjectLayerIdType nLayerId = it.first;

		SVoxBrick& voxData = it.second;

		for (int s = 0; s < GetSubSetsNum(); s++)
		{
			if (voxData.pData[s] != m_voxData.pData[s])
			{
				FreeBrick(voxData.pData[s]);
			}
			else
			{
				voxData.pData[s] = nullptr;
			}
		}
	}

	m_objLayerMap.clear();
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
	cry_sprintf(szFolder, 256, "%s", szLevelFolder);
}

void CVoxelSegment::StreamAsyncOnComplete(IReadStream* pStream, unsigned nError)
{
	FUNCTION_PROFILER_3DENGINE;

	if (m_fileStreamSize < 0)
	{
		VoxelizeMeshes(0);
		return;
	}

	if (pStream->IsError())
	{
		m_eStreamingStatus = ecss_Ready;
		return;
	}

	byte* pDataRead = (byte*)pStream->GetBuffer();
	uint bytesRead = pStream->GetBytesRead();

	byte* pData = (byte*)pDataRead;

	int compressedSize = *(int*)pData;
	pData += sizeof(int);

	if (compressedSize)
	{
		if ((compressedSize < 0) || (compressedSize > SVO_VOX_BRICK_MAX_SIZE * SVO_VOX_BRICK_MAX_SIZE * SVO_VOX_BRICK_MAX_SIZE * 4 * 2))
		{
			ErrorTerminate("%s: Data corruption detected", __FUNCTION__);
		}

		CMemoryBlock mbZip;
		mbZip.Allocate(compressedSize);
		memcpy(mbZip.GetData(), pData, mbZip.GetSize());

		pData += mbZip.GetSize();
		while ((pData - pDataRead) & 3)
		{
			pData++;
		}

		CMemoryBlock* pUnpacked = CMemoryBlock::DecompressFromMemBlock(&mbZip, GetSystem());

		if (!pUnpacked || !pUnpacked->GetSize())
		{
			ErrorTerminate("%s: DecompressFromMemBlock error", __FUNCTION__);
		}
		else
		{
			LoadVoxels((byte*)pUnpacked->GetData(), pUnpacked->GetSize());
		}

		delete pUnpacked;
	}

	// load child info if available
	if (ptrdiff_t(pData - pDataRead + (sizeof(uint32) * 2 * 8)) <= ptrdiff_t(bytesRead))
	{
		bool bChildsExist = false;

		for (int childId = 0; childId < 8; childId++)
		{
			if (((uint32*)pData)[childId * 2 + 1])
			{
				bChildsExist = true;
				break;
			}
		}

		if (bChildsExist)
		{
			m_pNode->m_pChildFileOffsets = new std::pair<uint32, uint32>[8];

			for (int childId = 0; childId < 8; childId++)
			{
				m_pNode->m_pChildFileOffsets[childId].first = *(uint32*)pData; // file offset
				pData += sizeof(uint32);

				m_pNode->m_pChildFileOffsets[childId].second = *(uint32*)pData; // data size
				pData += sizeof(uint32);

				if (m_pNode->m_pChildFileOffsets[childId].second)
				{
					m_dwChildTrisTest |= (1 << childId);
				}
			}
		}
		else
		{
			pData += sizeof(uint32) * 2 * 8;
		}
	}

	assert(ptrdiff_t(pData - pDataRead) <= ptrdiff_t(bytesRead));

	if (ptrdiff_t(pData - pDataRead) > ptrdiff_t(bytesRead))
	{
		ErrorTerminate("%s: Data size error, nBytesRead = %d, (pData - pDataRead) = %td", __FUNCTION__, bytesRead, ptrdiff_t(pData - pDataRead));
	}
}

void CVoxelSegment::StreamOnComplete(IReadStream* pStream, unsigned nError)
{
	FUNCTION_PROFILER_3DENGINE;

	m_eStreamingStatus = ecss_Ready;

	m_streamingTasksInProgress--;

	if (m_arrLoadedSegments.Find(this) < 0)
		m_arrLoadedSegments.Add(this);

	if (m_vCropTexSize.GetVolume() == 0 || !m_voxData.pData[SVoxBrick::OPA3D])
	{
		if (GetBoxSize() <= GetCVars()->e_svoMaxNodeSize && m_pParentCloud && !Cry3DEngineBase::GetCVars()->e_svoTI_Troposphere_Subdivide)
		{
			CSvoNode** ppChilds = m_pParentCloud->m_pNode->m_ppChilds;

			for (int childId = 0; childId < 8; childId++)
			{
				if (ppChilds[childId] == m_pNode)
				{
					m_pParentCloud->m_pNode->m_arrChildNotNeeded[childId] = true;
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
}

bool CVoxelSegment::StartStreaming(CVoxStreamEngine* pVoxStreamEngine)
{
	if (m_eStreamingStatus != ecss_NotLoaded)
	{
		return true;
	}

	if (CSvoNode::IsStreamingActive() && m_pNode->m_nodeBox.GetSize().x <= SVO_ROOTLESS_PARENT_SIZE / 2)
	{
		// stream voxels from disk

		assert(m_fileStreamOffset64 >= 0 && m_fileStreamSize);

		StreamReadParams params;
		params.nOffset = m_fileStreamOffset64;
		params.nSize = m_fileStreamSize;
		params.ePriority = estpAboveNormal;

		CSvoNode* pAreaNode = m_pNode;

		while (pAreaNode->m_nodeBox.GetSize().x < SVO_ROOTLESS_PARENT_SIZE / 2)
		{
			pAreaNode = pAreaNode->m_pParent;
		}

		char szAreaFileName[256];
		pAreaNode->MakeNodeFilePath(szAreaFileName);

		GetSystem()->GetStreamEngine()->StartRead(eStreamTaskTypeGeometry, szAreaFileName, this, &params);
	}
	else
	{
		// generate voxels on CPU
		if (!pVoxStreamEngine->StartRead(this, m_fileStreamOffset64, m_fileStreamSize))
		{
			return false;
		}
	}

	m_eStreamingStatus = ecss_InProgress;

	m_streamingTasksInProgress++;

	return true;
}

void CVoxelSegment::CropVoxTexture(int threadId, bool bCompSurfDist)
{
	m_vCropTexSize.Set(0, 0, 0);

	if (!GetBrickDataSize(m_voxData.pData[SVoxBrick::OPA3D]))
	{
		return;
	}

	Vec3i vMin(SVO_VOX_BRICK_MAX_SIZE, SVO_VOX_BRICK_MAX_SIZE, SVO_VOX_BRICK_MAX_SIZE);
	Vec3i vMax(0, 0, 0);

	for (auto& it : m_objLayerMap)
	{
		ObjectLayerIdType nLayerId = it.first;

		SVoxBrick& voxData = it.second;

		for (int x = 0; x < SVO_VOX_BRICK_MAX_SIZE; x++)
			for (int y = 0; y < SVO_VOX_BRICK_MAX_SIZE; y++)
				for (int z = 0; z < SVO_VOX_BRICK_MAX_SIZE; z++)
				{
					const int id = z * SVO_VOX_BRICK_MAX_SIZE * SVO_VOX_BRICK_MAX_SIZE + y * SVO_VOX_BRICK_MAX_SIZE + x;

					ColorB& opaOut = voxData.pData[SVoxBrick::OPA3D][id];

					if ((opaOut.r || opaOut.g || opaOut.b) || Cry3DEngineBase::GetCVars()->e_svoTI_Troposphere_Subdivide)
					{
						vMin.CheckMin(Vec3i(x, y, z));
						vMax.CheckMax(Vec3i(x + 1, y + 1, z + 1));
					}

					if (voxData.pData[SVoxBrick::NORML] && (opaOut.r || opaOut.g || opaOut.b))
					{
						voxData.pData[SVoxBrick::NORML][id].a = 255;
					}
				}
	}

	m_vCropTexSize = vMax - vMin;

	for (int d = 0; d < 3; d++)
	{
		if (vMax[d] < SVO_VOX_BRICK_MAX_SIZE)
			vMax[d]++;

		if (vMin[d] > 0)
			vMin[d]--;
	}

	m_vCropTexSize = vMax - vMin;

	if (m_vCropTexSize.x > 0 && m_vCropTexSize.y > 0 && m_vCropTexSize.z > 0)
	{
		for (auto& it : m_objLayerMap)
		{
			ObjectLayerIdType nLayerId = it.first;

			SVoxBrick& voxDataIn = it.second;

			SVoxBrick voxTemp;
			for (int s = 0; s < GetSubSetsNum(); s++)
			{
				voxTemp.pData[s] = new ColorB[m_vCropTexSize.x * m_vCropTexSize.y * m_vCropTexSize.z];
				memset(voxTemp.pData[s], 0, m_vCropTexSize.x * m_vCropTexSize.y * m_vCropTexSize.z * sizeof(ColorB));
			}

			// copy cropped data into temp
			for (int x = 0; x < m_vCropTexSize.x; x++)
				for (int y = 0; y < m_vCropTexSize.y; y++)
					for (int z = 0; z < m_vCropTexSize.z; z++)
					{
						int x_in = x + vMin.x;
						int y_in = y + vMin.y;
						int z_in = z + vMin.z;

						int id_in = z_in * SVO_VOX_BRICK_MAX_SIZE * SVO_VOX_BRICK_MAX_SIZE + y_in * SVO_VOX_BRICK_MAX_SIZE + x_in;

						ColorB& opaIn = voxDataIn.pData[SVoxBrick::OPA3D][id_in];

						if (opaIn.r || opaIn.g || opaIn.b)
						{
							int id_out = z * m_vCropTexSize.x * m_vCropTexSize.y + y * m_vCropTexSize.x + x;

							for (int s = 0; s < GetSubSetsNum(); s++)
							{
								voxTemp.pData[s][id_out] = voxDataIn.pData[s][id_in];
							}
						}
					}

			// copy back from temp
			for (int s = 0; s < GetSubSetsNum(); s++)
			{
				memcpy(voxDataIn.pData[s], voxTemp.pData[s], sizeof(ColorB) * m_vCropTexSize.x * m_vCropTexSize.y * m_vCropTexSize.z);
				SAFE_DELETE_ARRAY(voxTemp.pData[s]);
			}
		}

		m_vCropBoxMin = vMin;
	}
	else
	{
		FreeAllBrickData();

		m_vCropTexSize.zero();
		m_vCropBoxMin.zero();
	}
}

void CVoxelSegment::ComputeDistancesFast_MinDistToSurf(ColorB* pTex3dOptRGBA, ColorB* pTex3dOptNorm, ColorB* pTex3dOptOpac, int threadId)
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

					float minDistSq = 255 * 255 * 2;

					int _nNearestId = 0;

					int X1 = CLAMP(X - SVO_DIST_TO_SURF_RANGE, 0, m_vCropTexSize.x - 1);
					int X2 = CLAMP(X + SVO_DIST_TO_SURF_RANGE, 0, m_vCropTexSize.x - 1);
					int Y1 = CLAMP(Y - SVO_DIST_TO_SURF_RANGE, 0, m_vCropTexSize.y - 1);
					int Y2 = CLAMP(Y + SVO_DIST_TO_SURF_RANGE, 0, m_vCropTexSize.y - 1);
					int Z1 = CLAMP(Z - SVO_DIST_TO_SURF_RANGE, 0, m_vCropTexSize.z - 1);
					int Z2 = CLAMP(Z + SVO_DIST_TO_SURF_RANGE, 0, m_vCropTexSize.z - 1);

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
									float distSq = v1.GetSquaredDistance(v0);

									if (distSq < minDistSq)
									{
										minDistSq = distSq;
										_nNearestId = _idOut;
									}
								}
							}

					minDistSq = sqrt(minDistSq);

					if (minDistSq <= 4)
						norOut_.a = SATURATEB(255 - (int)minDistSq);
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
		m_pBlockPacker = new CBlockPacker3D(SVO_ATLAS_DIM_MAX_XY, SVO_ATLAS_DIM_MAX_XY, SVO_ATLAS_DIM_MAX_Z, true);

	int blockW = max(1, (m_vCropTexSize.x + SVO_BRICK_ALLOC_CHUNK_SIZE - 1) / SVO_BRICK_ALLOC_CHUNK_SIZE);
	int blockH = max(1, (m_vCropTexSize.y + SVO_BRICK_ALLOC_CHUNK_SIZE - 1) / SVO_BRICK_ALLOC_CHUNK_SIZE);
	int blockD = max(1, (m_vCropTexSize.z + SVO_BRICK_ALLOC_CHUNK_SIZE - 1) / SVO_BRICK_ALLOC_CHUNK_SIZE);

	const float minNodeSize = Cry3DEngineBase::GetCVars()->e_svoMinNodeSize;

	const int dataSizeStatsScale = 4;

	#ifndef _RELEASE
	{
		// count stats
		m_poolUsageItems = 0;
		m_poolUsageBytes = 0;

		const uint numBlocks = m_pBlockPacker->GetNumBlocks();
		for (uint blockId = 0; blockId < numBlocks; blockId++)
		{
			if (SBlockMinMax* pInfo = m_pBlockPacker->GetBlockInfo(blockId))
			{
				m_poolUsageItems++;
				m_poolUsageBytes += dataSizeStatsScale * pInfo->m_nDataSize;
			}
		}
	}
	#endif

	// TODO: pack multiple bricks into single compressed block and upload to GPU only once

	for (int passId = 0; passId < 16; passId++)
	{
		m_pBlockInfo = m_pBlockPacker->AddBlock(blockW, blockH, blockD,
		                                        this, GetCurrPassMainFrameID(), m_vCropTexSize.x * m_vCropTexSize.y * m_vCropTexSize.z * (gSvoEnv->m_voxTexFormat == eTF_BC3 ? 1 : 4) / dataSizeStatsScale);
		if (m_pBlockInfo)
		{
			bool bIsMultiThreadedRenderer = false;
			gEnv->pRenderer->EF_Query(EFQ_RenderMultithreaded, bIsMultiThreadedRenderer);

			if (bIsMultiThreadedRenderer)
				m_pNode->m_requestSegmentUpdateFrametId = max(m_pNode->m_requestSegmentUpdateFrametId, GetCurrPassMainFrameID() + 1);
			else
				m_pNode->m_requestSegmentUpdateFrametId = max(m_pNode->m_requestSegmentUpdateFrametId, GetCurrPassMainFrameID() + 0);

			m_vStatLightsCheckSumm.zero();
			break;
		}

		// remove 4 oldest blocks
		SBlockMinMax* pOldestBlockInfo[4] = { 0, 0, 0, 0 };
		uint arrOldestVisFrameId[4] = { ~0U, ~0U, ~0U, ~0U };
		const uint maxAllowedFrameId = GetCurrPassMainFrameID() - 16;
		const uint numBlocks = m_pBlockPacker->GetNumBlocks();
		for (uint blockId = 0; blockId < numBlocks; blockId++)
		{
			if (SBlockMinMax* pInfo = m_pBlockPacker->GetBlockInfo(blockId))
			{
				int newestSlotId = 0;
				for (int i = 0; i < 4; i++)
					if (arrOldestVisFrameId[i] > arrOldestVisFrameId[newestSlotId])
						newestSlotId = i;

				if (pInfo->m_nLastVisFrameId < arrOldestVisFrameId[newestSlotId] && pInfo->m_nLastVisFrameId < maxAllowedFrameId)
				{
					CVoxelSegment* pSeg = (CVoxelSegment*)pInfo->m_pUserData;
					pInfo->m_nLastVisFrameId = pSeg->m_lastRendFrameId;

					uint frameIdWeighted = pSeg->m_lastRendFrameId + int(pSeg->GetBoxSize() / minNodeSize);

					if ((frameIdWeighted < arrOldestVisFrameId[newestSlotId])
					    && (pSeg->m_lastTexUpdateFrameId < maxAllowedFrameId)
					    && (pSeg->m_lastRendFrameId < maxAllowedFrameId))
					{
						pOldestBlockInfo[newestSlotId] = pInfo;
						arrOldestVisFrameId[newestSlotId] = frameIdWeighted;
					}
				}
			}
		}

		int numRemovedBlocks = 0;

		for (int i = 0; i < 4; i++)
			if (pOldestBlockInfo[i])
			{
				CVoxelSegment* pSeg = (CVoxelSegment*)pOldestBlockInfo[i]->m_pUserData;
				if (pSeg->m_pBlockInfo != pOldestBlockInfo[i])
					ErrorTerminate("pSeg->m_pBlockInfo != pOldestBlockInfo[i]");
				pSeg->ReleaseAtlasBlock();
				numRemovedBlocks++;
			}

		if (!numRemovedBlocks)
		{
			break;
		}
	}

	if (m_pBlockInfo == 0)
	{
		Cry3DEngineBase::PrintMessage("UpdateBrickRenderData postponed %d", GetCurrPassMainFrameID());
		gSvoEnv->m_svoFreezeTime = -1; // prevent hang in case of full sync update
		CVoxelSegment::m_bUpdateBrickRenderDataPostponed = 1;
		return false;
	}

	CVoxelSegment::m_bUpdateBrickRenderDataPostponed = 0;
	m_lastTexUpdateFrameId = GetCurrPassMainFrameID();
	m_lastRendFrameId = GetCurrPassMainFrameID();

	assert(m_pBlockInfo->m_pUserData == this);
	Vec3i vOffset(m_pBlockInfo->m_dwMinX, m_pBlockInfo->m_dwMinY, m_pBlockInfo->m_dwMinZ);
	m_allocatedAtlasOffset = vOffset.z * SVO_ATLAS_DIM_MAX_XY * SVO_ATLAS_DIM_MAX_XY + vOffset.y * SVO_ATLAS_DIM_MAX_XY + vOffset.x;

	if (GetCVars()->e_svoMaxBrickUpdates > 0)
	{
		if (m_vCropTexSize.GetVolume() && m_voxData.pData[SVoxBrick::OPA3D])
		{
			UpdateVoxRenderData();
		}

		UpdateNodeRenderData();

		UpdateMeshRenderData();
	}

	return true;
}

bool CVoxelSegment::CheckUpdateBrickRenderData(bool bJustCheck)
{
	bool bRes = true;

	if (m_allocatedAtlasOffset < 0)
	{
		if (bJustCheck)
			bRes = false;
		else
			bRes = UpdateBrickRenderData();
	}

	return bRes;
}

void CVoxelSegment::UpdateObjectLayersInfo()
{
	// Copy only if modified
	if (m_arrObjectLayersInfoVersion == Get3DEngine()->m_objectLayersModificationId)
	{
		return;
	}

	m_arrObjectLayersInfoVersion = Get3DEngine()->m_objectLayersModificationId;
	m_arrObjectLayersInfo = Get3DEngine()->m_arrObjectLayersActivity;
}

void CVoxelSegment::CheckAllocateTexturePool()
{
	int flagsReadOnly = FT_DONT_STREAM;
	int flagsReadWrite = FT_DONT_STREAM | FT_USAGE_UNORDERED_ACCESS | FT_USAGE_UAV_RWTEXTURE;

	if (GetSubSetsNum() > 1)
	{
		if (!gSvoEnv->m_texRgb0PoolId)
		{
			gSvoEnv->m_texRgb0PoolId = gEnv->pRenderer->UploadToVideoMemory3D(NULL,
			                                                                  m_voxTexPoolDimXY, m_voxTexPoolDimXY, m_voxTexPoolDimZ, gSvoEnv->m_voxTexFormat, gSvoEnv->m_voxTexFormat, 1, false, FILTER_LINEAR, 0, 0, flagsReadWrite);
			m_svoDataPoolsCounter++;
		}

		if (Cry3DEngineBase::GetCVars()->e_svoTI_Active && Cry3DEngineBase::GetCVars()->e_svoTI_IntegrationMode)
		{
			// direct lighting
			if (!gSvoEnv->m_texRgb1PoolId)
			{
				gSvoEnv->m_texRgb1PoolId = gEnv->pRenderer->UploadToVideoMemory3D(NULL,
				                                                                  m_voxTexPoolDimXY, m_voxTexPoolDimXY, m_voxTexPoolDimZ, gSvoEnv->m_voxTexFormat, gSvoEnv->m_voxTexFormat, 1, false, FILTER_LINEAR, 0, 0, flagsReadWrite);
				m_svoDataPoolsCounter++;
			}

			// dyn direct lighting
			if (!gSvoEnv->m_texDynlPoolId && Cry3DEngineBase::GetCVars()->e_svoTI_DynLights)
			{
				gSvoEnv->m_texDynlPoolId = gEnv->pRenderer->UploadToVideoMemory3D(NULL,
				                                                                  m_voxTexPoolDimXY, m_voxTexPoolDimXY, m_voxTexPoolDimZ, gSvoEnv->m_voxTexFormat, gSvoEnv->m_voxTexFormat, 1, false, FILTER_LINEAR, 0, 0, flagsReadWrite);
				m_svoDataPoolsCounter++;
			}

			// propagation
			if (!gSvoEnv->m_texRgb2PoolId && Cry3DEngineBase::GetCVars()->e_svoTI_NumberOfBounces > 1)
			{
				gSvoEnv->m_texRgb2PoolId = gEnv->pRenderer->UploadToVideoMemory3D(NULL,
				                                                                  m_voxTexPoolDimXY, m_voxTexPoolDimXY, m_voxTexPoolDimZ, gSvoEnv->m_voxTexFormat, gSvoEnv->m_voxTexFormat, 1, false, FILTER_LINEAR, 0, 0, flagsReadWrite);
				m_svoDataPoolsCounter++;
			}

			// propagation
			if (!gSvoEnv->m_texRgb3PoolId && Cry3DEngineBase::GetCVars()->e_svoTI_NumberOfBounces > 2)
			{
				gSvoEnv->m_texRgb3PoolId = gEnv->pRenderer->UploadToVideoMemory3D(NULL,
				                                                                  m_voxTexPoolDimXY, m_voxTexPoolDimXY, m_voxTexPoolDimZ, gSvoEnv->m_voxTexFormat, gSvoEnv->m_voxTexFormat, 1, false, FILTER_LINEAR, 0, 0, flagsReadWrite);
				m_svoDataPoolsCounter++;
			}

			// mesh
	#ifdef FEATURE_SVO_GI_USE_MESH_RT
			if (!gSvoEnv->m_texTrisPoolId && Cry3DEngineBase::GetCVars()->e_svoTI_RT_MaxDist)
			{
				gSvoEnv->m_texTrisPoolId = gEnv->pRenderer->UploadToVideoMemory3D(NULL,
				                                                                  m_voxTexPoolDimXY, m_voxTexPoolDimXY, m_voxTexPoolDimZ, gSvoEnv->m_voxTexFormat, gSvoEnv->m_voxTexFormat, 1, false, FILTER_LINEAR, 0, 0, flagsReadWrite);
				m_svoDataPoolsCounter++;
			}
	#endif
			// snow
			if (!gSvoEnv->m_texRgb4PoolId && Cry3DEngineBase::GetCVars()->e_svoTI_Troposphere_Snow_Height)
			{
				gSvoEnv->m_texRgb4PoolId = gEnv->pRenderer->UploadToVideoMemory3D(NULL,
				                                                                  m_voxTexPoolDimXY, m_voxTexPoolDimXY, m_voxTexPoolDimZ, gSvoEnv->m_voxTexFormat, gSvoEnv->m_voxTexFormat, 1, false, FILTER_LINEAR, 0, 0, flagsReadWrite);
				m_svoDataPoolsCounter++;
			}
		}

		{
			if (!gSvoEnv->m_texNormPoolId)
			{
				gSvoEnv->m_texNormPoolId = gEnv->pRenderer->UploadToVideoMemory3D(NULL,
				                                                                  m_voxTexPoolDimXY, m_voxTexPoolDimXY, m_voxTexPoolDimZ, gSvoEnv->m_voxTexFormat, gSvoEnv->m_voxTexFormat, 1, false, FILTER_LINEAR, 0, 0, flagsReadOnly);
				m_svoDataPoolsCounter++;
			}

			if (!gSvoEnv->m_texAldiPoolId && Cry3DEngineBase::GetCVars()->e_svoTI_Diffuse_Cache)
			{
				gSvoEnv->m_texAldiPoolId = gEnv->pRenderer->UploadToVideoMemory3D(NULL,
				                                                                  m_voxTexPoolDimXY, m_voxTexPoolDimXY, m_voxTexPoolDimZ, gSvoEnv->m_voxTexFormat, gSvoEnv->m_voxTexFormat, 1, false, FILTER_LINEAR, 0, 0, flagsReadWrite);
				m_svoDataPoolsCounter++;
			}
		}
	}

	if (!gSvoEnv->m_texOpasPoolId)
	{
	#ifndef _RELEASE
		int voxDataSize = m_voxTexPoolDimXY * m_voxTexPoolDimXY * m_voxTexPoolDimZ * sizeof(ColorB);
		byte* pDataZero = new byte[voxDataSize];
		memset(pDataZero, 0, voxDataSize);
	#else
		byte* pDataZero = 0;
	#endif

		gSvoEnv->m_texOpasPoolId = gEnv->pRenderer->UploadToVideoMemory3D(pDataZero,
		                                                                  m_voxTexPoolDimXY, m_voxTexPoolDimXY, m_voxTexPoolDimZ, gSvoEnv->m_voxTexFormat, gSvoEnv->m_voxTexFormat, 1, false, FILTER_LINEAR, 0, 0, flagsReadOnly);
		m_svoDataPoolsCounter++;

		gSvoEnv->m_texNodePoolId = gEnv->pRenderer->UploadToVideoMemory3D(pDataZero,
		                                                                  SVO_NODES_POOL_DIM_XY, SVO_NODES_POOL_DIM_XY, SVO_NODES_POOL_DIM_Z, eTF_R32G32B32A32F, eTF_R32G32B32A32F, 1, false, FILTER_POINT, 0, 0, flagsReadOnly);

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

	static Vec4 voxNodeData[SVO_NODE_BRICK_SIZE * SVO_NODE_BRICK_SIZE * SVO_NODE_BRICK_SIZE];
	ZeroStruct(voxNodeData);

	voxNodeData[0] = Vec4(m_boxOS.min + m_vSegOrigin, 0);
	voxNodeData[1] = Vec4(m_boxOS.max + m_vSegOrigin, 0);
	voxNodeData[0] = voxNodeData[0] + Vec4(Vec3((float)m_vCropBoxMin.x / SVO_VOX_BRICK_MAX_SIZE, (float)m_vCropBoxMin.y / SVO_VOX_BRICK_MAX_SIZE, (float)m_vCropBoxMin.z / SVO_VOX_BRICK_MAX_SIZE) * GetBoxSize(), 0);
	voxNodeData[1] = voxNodeData[0] + Vec4(Vec3((float)m_vCropTexSize.x / SVO_VOX_BRICK_MAX_SIZE, (float)m_vCropTexSize.y / SVO_VOX_BRICK_MAX_SIZE, (float)m_vCropTexSize.z / SVO_VOX_BRICK_MAX_SIZE) * GetBoxSize(), 0);
	voxNodeData[0].w = GetBoxSize();
	voxNodeData[1].w = m_pParentCloud ? (0.1f + (float)m_pParentCloud->m_allocatedAtlasOffset) : -2.f;

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
	  gSvoEnv->m_texNodePoolId,
	  (byte*)&voxNodeData[0],
	  vOffset.x * SVO_NODE_BRICK_SIZE,
	  vOffset.y * SVO_NODE_BRICK_SIZE,
	  SVO_NODE_BRICK_SIZE,
	  SVO_NODE_BRICK_SIZE,
	  eTF_R32G32B32A32F,
	  vOffset.z * SVO_NODE_BRICK_SIZE,
	  SVO_NODE_BRICK_SIZE);
	CVoxelSegment::m_updatesInProgressTex++;

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
		m_vertForGS.st.x = (0.5f + (float)m_allocatedAtlasOffset);

		// pack box size
		float nodeSize = m_vertForGS.st.y = GetBoxSize();
		Vec3 vBoxSize = m_boxClipped.GetSize();
		m_vertForGS.color.bcolor[0] = SATURATEB(int(vBoxSize.x / nodeSize * 255.f));
		m_vertForGS.color.bcolor[1] = SATURATEB(int(vBoxSize.y / nodeSize * 255.f));
		m_vertForGS.color.bcolor[2] = SATURATEB(int(vBoxSize.z / nodeSize * 255.f));
		m_vertForGS.color.bcolor[3] = SATURATEB(int(m_maxAlphaInBrick * 255.f));
	}
}

void CVoxelSegment::UpdateVoxRenderData()
{
	FUNCTION_PROFILER_3DENGINE;

	assert(m_pBlockInfo->m_pUserData == this);

	Vec3i vOffset(m_pBlockInfo->m_dwMinX, m_pBlockInfo->m_dwMinY, m_pBlockInfo->m_dwMinZ);

	Vec3i vSizeFin = m_vCropTexSize;

	if (gSvoEnv->m_voxTexFormat == eTF_BC3)
	{
		vSizeFin = GetDxtDim();
	}

	int arrTexId[SVoxBrick::MAX_NUM] = { gSvoEnv->m_texOpasPoolId, gSvoEnv->m_texRgb0PoolId, gSvoEnv->m_texNormPoolId };

	for (int s = 0; s < GetSubSetsNum(); s++)
	{
		if (m_voxData.pData[s])
		{
			gEnv->pRenderer->UpdateTextureInVideoMemory(
			  arrTexId[s],
			  (byte*)m_voxData.pData[s],
			  vOffset.x * SVO_BRICK_ALLOC_CHUNK_SIZE, vOffset.y * SVO_BRICK_ALLOC_CHUNK_SIZE,
			  vSizeFin.x, vSizeFin.y,
			  gSvoEnv->m_voxTexFormat,
			  vOffset.z * SVO_BRICK_ALLOC_CHUNK_SIZE,
			  vSizeFin.z);

			CVoxelSegment::m_updatesInProgressTex++;
		}
	}
}

void CVoxelSegment::ReleaseAtlasBlock()
{
	if (m_pBlockInfo)
		m_pBlockPacker->RemoveBlock(m_pBlockInfo);
	m_pBlockInfo = 0;
	m_allocatedAtlasOffset = -2;

	m_pNode->m_requestSegmentUpdateFrametId = 0;
	m_vStatLightsCheckSumm.zero();

	PropagateDirtyFlag();
}

void CVoxelSegment::PropagateDirtyFlag()
{
	if (CVoxelSegment* pParent = m_pParentCloud)
	{
		pParent->m_bChildOffsetsDirty = 2;

		if (pParent->m_pParentCloud)
			pParent->m_pParentCloud->m_bChildOffsetsDirty = 2;

		while (pParent->m_pParentCloud)
		{
			pParent = pParent->m_pParentCloud;
			pParent->m_bChildOffsetsDirty = max(pParent->m_bChildOffsetsDirty, (byte)1);
		}
	}
}

AABB CVoxelSegment::GetChildBBox(const AABB& parentBox, int childId)
{
	int x = (childId / 4);
	int y = (childId - x * 4) / 2;
	int z = (childId - x * 4 - y * 2);
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

bool GetBarycentricTC(const Vec3& a, const Vec3& b, const Vec3& c, float& u, float& v, float& w, const Vec3& p, const float& border)
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
	return (u >= -border) && (v >= -border) && (w >= -border);
}

ColorF CVoxelSegment::GetColorF_255(int x, int y, const ColorB* pImg, int imgSizeW, int imgSizeH)
{
	const ColorB& colB = pImg[x + y * imgSizeW];
	ColorF colF;
	colF.r = colB.r;
	colF.g = colB.g;
	colF.b = colB.b;
	colF.a = colB.a;

	return colF;
}

ColorF CVoxelSegment::GetBilinearAt(float iniX, float iniY, const ColorB* pImg, int dimW, int dimH, float multiplier)
{
	int imgSizeW = dimW;
	int imgSizeH = dimH;

	iniX *= imgSizeW;
	iniY *= imgSizeH;

	//  iniX -= .5f;
	//  iniY -= .5f;

	int x = (int)floor(iniX);
	int y = (int)floor(iniY);

	float rx = iniX - x;    // fractional part
	float ry = iniY - y;    // fractional part

	int maskW = imgSizeW - 1;
	int maskH = imgSizeH - 1;

	//  return GetColorF_255(nMaskW&(x  ),nMaskH&(y  ), pImg, nImgSizeW, nImgSizeH) * fBr;

	ColorF top = GetColorF_255(maskW & (x), maskH & (y), pImg, imgSizeW, imgSizeH) * (1.f - rx)     // left top
	             + GetColorF_255(maskW & (x + 1), maskH & (y), pImg, imgSizeW, imgSizeH) * rx;      // right top
	ColorF bot = GetColorF_255(maskW & (x), maskH & (y + 1), pImg, imgSizeW, imgSizeH) * (1.f - rx) // left bottom
	             + GetColorF_255(maskW & (x + 1), maskH & (y + 1), pImg, imgSizeW, imgSizeH) * rx;  // right bottom

	return (top * (1.f - ry) + bot * ry) * multiplier;
}

void CVoxelSegment::VoxelizeMeshes(int threadId, bool bUseMT)
{
	MEMSTAT_CONTEXT(EMemStatContextTypes::MSC_Other, 0, "VoxelizeMeshes");

	m_vCropBoxMin.Set(0, 0, 0);
	m_vCropTexSize.Set(SVO_VOX_BRICK_MAX_SIZE, SVO_VOX_BRICK_MAX_SIZE, SVO_VOX_BRICK_MAX_SIZE);

	PodArray<int>* pNodeTrisXYZ = 0;

	if (GetBoxSize() <= GetCVars()->e_svoMaxNodeSize)
	{
		pNodeTrisXYZ = new PodArray<int>[SVO_VOX_BRICK_MAX_SIZE * SVO_VOX_BRICK_MAX_SIZE * SVO_VOX_BRICK_MAX_SIZE];
		FindTrianglesForVoxelization(pNodeTrisXYZ);

		gSvoEnv->m_arrVoxelizeMeshesCounter[0]++;
		if (GetBoxSize() == GetCVars()->e_svoMaxNodeSize)
			gSvoEnv->m_arrVoxelizeMeshesCounter[1]++;
	}

	for (int s = 0; s < GetSubSetsNum(); s++)
	{
		CheckAllocateBrick(m_voxData.pData[s], m_vCropTexSize.x * m_vCropTexSize.y * m_vCropTexSize.z, true);
	}

	Vec3 vBoxCenter = m_pNode->m_nodeBox.GetCenter();

	// voxelize node tris
	if ((m_nodeTrisAllMerged.Count() || Cry3DEngineBase::GetCVars()->e_svoTI_Troposphere_Subdivide) && m_voxData.pData[SVoxBrick::OPA3D] && m_pTrisInArea && pNodeTrisXYZ)
	{
		PodArray<CVisArea*> arrPortals;

		// collect overlapping portals
		if (GetSubSetsNum() > 1)
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

		if (m_objLayerMap.empty())
		{
			m_objLayerMap[kAllObjectLayersId] = m_voxData;
		}

		/*if (bUseMT) // commented because causes not enough occlusion error
		   {
		   const int VOX_THREADS_NUM = 8;

		   JobManager::SJobState jobState[VOX_THREADS_NUM];
		   PodArray<int> arrTrisInt[VOX_THREADS_NUM];

		   for (int t = 0; t < VOX_THREADS_NUM; t++)
		   {
		    int x0 = t * SVO_VOX_BRICK_MAX_SIZE / VOX_THREADS_NUM;
		    int x1 = x0 + SVO_VOX_BRICK_MAX_SIZE / VOX_THREADS_NUM;

		    SBuildVoxelsParams params;
		    params.X0 = x0;
		    params.X1 = x1;
		    params.pNodeTrisXYZ = pNodeTrisXYZ;
		    params.arrPortals = &arrPortals;

		    arrTrisInt[t].PreAllocate(SVO_MAX_TRIS_PER_VOXEL);
		    params.pTrisInt = &arrTrisInt[t];

		    TBuildVoxelsJob job(params);
		    job.SetClassInstance(this);
		    job.SetPriorityLevel(JobManager::eHighPriority);
		    job.RegisterJobState(&jobState[t]);
		    job.Run();
		   }

		   for (int t = 0; t < VOX_THREADS_NUM; t++)
		   {
		    gEnv->GetJobManager()->WaitForJob(jobState[t]);
		   }
		   }
		   else*/
		{
			SBuildVoxelsParams params;
			params.X0 = 0;
			params.X1 = SVO_VOX_BRICK_MAX_SIZE;
			params.pNodeTrisXYZ = pNodeTrisXYZ;
			params.arrPortals = &arrPortals;

			PodArray<int> trisInt;
			trisInt.PreAllocate(SVO_MAX_TRIS_PER_VOXEL);
			params.pTrisInt = &trisInt;

			BuildVoxels(params);
		}

		if (!Cry3DEngineBase::GetCVars()->e_svoTI_Troposphere_Subdivide)
		{
			CropVoxTexture(threadId, true);
		}

		if (!m_bExportMode)
		{
			CombineLayers();
		}
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
		m_vCropTexSize.Set(SVO_VOX_BRICK_MAX_SIZE, SVO_VOX_BRICK_MAX_SIZE, SVO_VOX_BRICK_MAX_SIZE);
	}

	SAFE_DELETE_ARRAY(pNodeTrisXYZ);
}

void CVoxelSegment::CombineLayers()
{
	if (m_objLayerMap.size() != 1 || m_objLayerMap.begin()->second.pData[SVoxBrick::OPA3D] != m_voxData.pData[SVoxBrick::OPA3D])
	{
		for (int s = 0; s < GetSubSetsNum(); s++)
		{
			CheckAllocateBrick(m_voxData.pData[s], m_vCropTexSize.x * m_vCropTexSize.y * m_vCropTexSize.z, true);
		}

		for (auto& it : m_objLayerMap)
		{
			ObjectLayerIdType nLayerId = it.first;

			SVoxBrick& layerVoxData = it.second;

			if (nLayerId && ((nLayerId >= m_arrObjectLayersInfo.Count()) || !m_arrObjectLayersInfo[nLayerId].bActive))
			{
				continue;
			}

			int pixNum = m_vCropTexSize.x * m_vCropTexSize.y * m_vCropTexSize.z;

			for (int s = 0; s < GetSubSetsNum(); s++)
			{
				for (int p = 0; p < pixNum; p++)
				{
					for (int c = 0; c < 4; c++)
					{
						m_voxData.pData[s][p][c] = max(m_voxData.pData[s][p][c], layerVoxData.pData[s][p][c]);
					}
				}
			}
		}
	}

	FreeBrickLayers();
}

void CVoxelSegment::BuildVoxels(SBuildVoxelsParams params)
{
	int X0 = params.X0;
	int X1 = params.X1;
	PodArray<int>* pNodeTrisXYZ = params.pNodeTrisXYZ;
	PodArray<CVisArea*>* arrPortals = params.arrPortals;

	Vec4 voxNodeData[SVO_NODE_BRICK_SIZE * SVO_NODE_BRICK_SIZE * SVO_NODE_BRICK_SIZE];
	ZeroStruct(voxNodeData);
	voxNodeData[0] = Vec4(m_boxOS.min + m_vSegOrigin, 0);
	voxNodeData[1] = Vec4(m_boxOS.max + m_vSegOrigin, 0);

	const int dimSS = 4;
	const int dimS = dimSS / 2;

	PodArray<int> arrSubOpa[dimS][dimS][dimS];

	for (int X = X0; X < X1; X++)
	{
		for (int Y = 0; Y < SVO_VOX_BRICK_MAX_SIZE; Y++)
		{
			for (int Z = 0; Z < SVO_VOX_BRICK_MAX_SIZE; Z++)
			{
				Vec4 vMin = voxNodeData[0] + (voxNodeData[1] - voxNodeData[0]) * Vec4((float) X / SVO_VOX_BRICK_MAX_SIZE, (float) Y / SVO_VOX_BRICK_MAX_SIZE, (float) Z / SVO_VOX_BRICK_MAX_SIZE, 1);
				Vec4 vMax = voxNodeData[0] + (voxNodeData[1] - voxNodeData[0]) * Vec4((float) (X + 1) / SVO_VOX_BRICK_MAX_SIZE, (float) (Y + 1) / SVO_VOX_BRICK_MAX_SIZE, (float) (Z + 1) / SVO_VOX_BRICK_MAX_SIZE, 1);

				// safety border support
				//      Vec4 vCenter(m_vOrigin.x,m_vOrigin.y,m_vOrigin.z,0);
				//        vMin += (vMin - vCenter)/(kVoxTexMaxDim/2)/2;
				//        vMax += (vMax - vCenter)/(kVoxTexMaxDim/2)/2;

				AABB voxBox;
				voxBox.min.Set(vMin.x, vMin.y, vMin.z);
				voxBox.max.Set(vMax.x, vMax.y, vMax.z);

				if (Overlap::AABB_AABB(voxBox, m_boxTris))
				{
					// loop through found layers
					for (auto& it : m_objLayerMap)
					{
						ObjectLayerIdType nLayerId = it.first;

						SVoxBrick& voxData = it.second;

						const int id = Z * SVO_VOX_BRICK_MAX_SIZE * SVO_VOX_BRICK_MAX_SIZE + Y * SVO_VOX_BRICK_MAX_SIZE + X;

						ColorB& opaOutFin = voxData.pData[SVoxBrick::OPA3D][id];

						PodArray<int>& trisInt = *params.pTrisInt;
						trisInt.Clear();

						bool bVisAreaTrisDetected = false;

						AUTO_READLOCK(m_superMeshLock);

						// collect tris only for this voxel; TODO: check maybe pNodeTrisXYZ[id] already have only what is needed and trisInt can be dropped
						for (int d = 0; (d < pNodeTrisXYZ[id].Count()) && (trisInt.Count() < SVO_MAX_TRIS_PER_VOXEL); d++)
						{
							int trId = pNodeTrisXYZ[id].GetAt(d);

							SRayHitTriangleIndexed& tr = (*m_pTrisInArea)[trId];

							if (tr.objectLayerId == nLayerId)
							{
								Vec3 arrV[3] = { (*m_pVertInArea)[tr.arrVertId[0]].v, (*m_pVertInArea)[tr.arrVertId[1]].v, (*m_pVertInArea)[tr.arrVertId[2]].v };

								if (Overlap::AABB_Triangle(voxBox, arrV[0], arrV[1], arrV[2])) // 14s
								{
									if (tr.hitObjectType == HIT_OBJ_TYPE_VISAREA)
										bVisAreaTrisDetected = true;

									trisInt.Add(trId);
								}
							}
						}

	#ifdef FEATURE_SVO_GI_USE_MESH_RT
						if (GetCVars()->e_svoTI_RT_MaxDist && gSvoEnv->m_texTrisPoolId)
						{
							int trisCount = 0;
							int firstTriIndex = StoreIndicesIntoPool(trisIntRT, trisCount);
							int id = firstTriIndex;
							m_voxData.pVoxTris[id].r = id & 255;
							id /= 256;
							m_voxData.pVoxTris[id].g = id & 255;
							id /= 256;
							m_voxData.pVoxTris[id].b = id & 255;
							id /= 256;
							m_voxData.pVoxTris[id].a = trisCount;
						}
	#endif

						// OPA
						{
							{
								// Fill nSDim x nSDim x nSDim super voxel

								Vec3 vSubVoxSize = voxBox.GetSize() / (float)dimS;

								for (int x = 0; x < dimS; x++)
								{
									for (int y = 0; y < dimS; y++)
									{
										for (int z = 0; z < dimS; z++)
										{
											PodArray<int>& opaOutSub = arrSubOpa[x][y][z];

											opaOutSub.Clear();

											AABB SubBox;
											SubBox.min = voxBox.min + vSubVoxSize.CompMul(Vec3((float)x, (float)y, (float)z));
											SubBox.max = SubBox.min + vSubVoxSize;

											AABB SubBoxT = SubBox;
											SubBoxT.max.z++;

											for (int c = 0; c < trisInt.Count(); c++)
											{
												int triangleId = trisInt.GetAt(c);

												SRayHitTriangleIndexed& tr = (*m_pTrisInArea)[triangleId];

												Vec3 arrV[3] = { (*m_pVertInArea)[tr.arrVertId[0]].v, (*m_pVertInArea)[tr.arrVertId[1]].v, (*m_pVertInArea)[tr.arrVertId[2]].v };

												if (Overlap::AABB_Triangle(tr.materialID ? SubBox : SubBoxT, arrV[0], arrV[1], arrV[2])) // 40 ms
												{
													opaOutSub.Add(triangleId);
												}
											}
										}
									}
								}
							}

							uint8 arrSubSubOpa[dimSS][dimSS][dimSS];
							Vec4 arrSubSubNor[dimSS][dimSS][dimSS];
							ColorF arrSubSubCol[dimSS][dimSS][dimSS];
							float arrSubSubEmi[dimSS][dimSS][dimSS];

							ZeroStruct(arrSubSubOpa);

							if (GetSubSetsNum() > 1)
							{
								ZeroStruct(arrSubSubNor);
								ZeroStruct(arrSubSubCol);
								ZeroStruct(arrSubSubEmi);
							}

							// Fill nSSDim x nSSDim x nSSDim super voxel

							Vec3 vSubSubVoxSize = voxBox.GetSize() / (float)dimSS;

							for (int x = 0; x < dimSS; x++)
							{
								for (int y = 0; y < dimSS; y++)
								{
									for (int z = 0; z < dimSS; z++)
									{
										PodArray<int>& opaOutSub = arrSubOpa[x / 2][y / 2][z / 2];

										uint8& opaOutSubSub = arrSubSubOpa[x][y][z];
										Vec4& norOutSubSub = arrSubSubNor[x][y][z];
										ColorF& colOutSubSub = arrSubSubCol[x][y][z];
										float& EmiOutSubSub = arrSubSubEmi[x][y][z];

										AABB SubSubBox;
										SubSubBox.min = voxBox.min + vSubSubVoxSize.CompMul(Vec3((float)x, (float)y, (float)z));
										SubSubBox.max = SubSubBox.min + vSubSubVoxSize;

										AABB SubSubBoxT = SubSubBox;
										SubSubBoxT.max.z++;

										AABB SubSubBoxP = SubSubBox;
										SubSubBoxP.Expand(Vec3(.25, .25, .25));

										if (GetSubSetsNum() > 1)
										{
											bool bVoxelInPortal = false;

											for (int v = 0; v < arrPortals->Count(); v++)
											{
												if (Overlap::AABB_AABB(*(*arrPortals)[v]->GetAABBox(), SubSubBoxP))
												{
													bVoxelInPortal = true;
													break;
												}
											}

											if (bVoxelInPortal) // skip sub-voxels in the portal
												continue;
										}

										for (int c = 0; c < opaOutSub.Count(); c++)
										{
											if (opaOutSub.GetAt(c) >= (*m_pTrisInArea).Count())
											{
												PrintMessage("%s warning: trId>=(*m_pTrisInArea).Count()", __FUNC__);
												break;
											}

											SRayHitTriangleIndexed& tr = (*m_pTrisInArea)[opaOutSub.GetAt(c)];

											Vec3 arrV[3] = { (*m_pVertInArea)[tr.arrVertId[0]].v, (*m_pVertInArea)[tr.arrVertId[1]].v, (*m_pVertInArea)[tr.arrVertId[2]].v };

											if (Overlap::AABB_Triangle(tr.materialID ? SubSubBox : SubSubBoxT, arrV[0], arrV[1], arrV[2])) // 8ms
											{
												ColorF colTraced = ProcessMaterial(tr, SubSubBox.GetCenter());
												if (colTraced.a)
												{
													opaOutSubSub = max(opaOutSubSub, min(tr.opacity, (uint8)SATURATEB(colTraced.a * 255.f)));

													if (GetSubSetsNum() > 1)
													{
														norOutSubSub += Vec4(tr.vFaceNorm, 1);

														colOutSubSub.r += colTraced.r;
														colOutSubSub.g += colTraced.g;
														colOutSubSub.b += colTraced.b;
														colOutSubSub.a += 1.f;

														SSvoMatInfo& rMI = m_pMatsInArea->GetAt(tr.materialID);
														if (rMI.pMat)
														{
															EmiOutSubSub = +rMI.pMat->GetShaderItem(0).m_pShaderResources->GetFinalEmittance().Luminance();
														}
													}
													else
													{
														if (opaOutSubSub == 255)
														{
															break;
														}
													}
												}
											}
										}
									}
								}
							}

							// Compute tri-planar opacity

							uint8 arrQuad[3][dimSS][dimSS];
							ZeroStruct(arrQuad);
							Vec4 vNorFin(0, 0, 0, 0);
							ColorF vColFin(0, 0, 0, 0);
							float emiFin(0);

							for (int _x = 0; _x < dimSS; _x++)
							{
								for (int _y = 0; _y < dimSS; _y++)
								{
									for (int _z = 0; _z < dimSS; _z++)
									{
										uint8& opaCh = arrSubSubOpa[_x][_y][_z];
										Vec4& norCh = arrSubSubNor[_x][_y][_z];
										ColorF& colCh = arrSubSubCol[_x][_y][_z];
										float& EmiCh = arrSubSubEmi[_x][_y][_z];

										arrQuad[0][_y][_z] = max(arrQuad[0][_y][_z], opaCh);
										arrQuad[1][_x][_z] = max(arrQuad[1][_x][_z], opaCh);
										arrQuad[2][_x][_y] = max(arrQuad[2][_x][_y], opaCh);

										if (GetSubSetsNum() > 1)
										{
											vNorFin += norCh;
											vColFin += colCh;
											emiFin += EmiCh;
										}
									}
								}
							}

							// we do not normalize normal, length of the normal says how reliable the normal is
							if (vNorFin.w)
							{
								vNorFin = vNorFin / vNorFin.w;
							}

							if (vColFin.a)
							{
								emiFin /= vColFin.a;
								vColFin /= vColFin.a;
							}

							ColorF opaSummF(0, 0, 0, 0);

							for (int k = 0; k < dimSS; k++)
							{
								for (int m = 0; m < dimSS; m++)
								{
									opaSummF.r += arrQuad[0][k][m];
									opaSummF.g += arrQuad[1][k][m];
									opaSummF.b += arrQuad[2][k][m];
								}
							}

							opaSummF.r /= dimSS * dimSS;
							opaSummF.g /= dimSS * dimSS;
							opaSummF.b /= dimSS * dimSS;

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
								vColFin.r = vColFin.g = vColFin.b = 0;         // full black
							}

							if (GetSubSetsNum() > 1)
							{
								ColorB& norOut = voxData.pData[SVoxBrick::NORML][id];

								norOut.a = (opaOutFin.r || opaOutFin.g || opaOutFin.b) ? 255 : 0; // contains 255 for any geometry and 0 for empty space

								for (int c = 0; c < 3; c++)
								{
									norOut[c] = SATURATEB(int(vNorFin[2 - c] * 127.5f + 127.5f));
								}

								ColorB& colOut = voxData.pData[SVoxBrick::COLOR][id];

								colOut.r = SATURATEB((int)(vColFin.r * 255.f));
								colOut.g = SATURATEB((int)(vColFin.g * 255.f));
								colOut.b = SATURATEB((int)(vColFin.b * 255.f));
								colOut.a = SATURATEB((int)(emiFin * 255.f));

								if (opaOutFin.r || opaOutFin.g || opaOutFin.b)
								{
									m_solidVoxelsNum++;
								}
							}
						}
					}
				}
			}
		}
	}
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
		for (int objType = 0; objType < eERType_TypesNum; objType++)
		{
			if ((bThisIsAreaParent && (objType == eERType_Brush || objType == eERType_MovableBrush)) || (objType == eERType_Vegetation))
			{
				PodArray<IRenderNode*> arrRenderNodes;

				Get3DEngine()->GetObjectsByTypeGlobal(arrRenderNodes, (EERType)objType, &cloudBoxWS, bAllowStartStreaming ? &bSuccess : 0, ERF_GI_MODE_BIT0);
				if (Get3DEngine()->GetVisAreaManager())
					Get3DEngine()->GetVisAreaManager()->GetObjectsByType(arrRenderNodes, (EERType)objType, &cloudBoxWS, bAllowStartStreaming ? &bSuccess : 0, ERF_GI_MODE_BIT0);

				if (!arrRenderNodes.Count())
					continue;

				int culledNum = 0;

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

					float maxViewDist = pNode->GetBBox().GetRadius() * GetCVars()->e_ViewDistRatio;

					float minAllowedViewDist = (pNode->GetRenderNodeType() == eERType_Vegetation) ? (GetCVars()->e_svoTI_ObjectsMaxViewDistance * 2) : GetCVars()->e_svoTI_ObjectsMaxViewDistance;
					minAllowedViewDist *= GetCVars()->e_svoTI_ObjectsMaxViewDistanceScale;

					if (bThisIsLowLodNode)
					{
						if (pNode->GetBBox().GetSize().z < cloudBoxWS.GetSize().z * 0.25f)
							continue;

						minAllowedViewDist *= 4.f;
					}

					if (maxViewDist < minAllowedViewDist)
					{
						culledNum++;
						continue;
					}

					if (pNode->GetBBox().GetSize().z > 256.f)
					{
						//            const T_ObjectLayerId layerId = pNode->GetLayerId();
						//
						//            CStatObj* pStatObj = (CStatObj*)pNode->GetEntityStatObj();
						//            gEnv->pLog->LogWarning("%s: Warning: Too big object skipped at position (%.1f, %.1f, %.1f), name: '%s', layer: %d, CGF name: '%s'",
						//                                   __FUNC__, pNode->GetBBox().GetCenter().x, pNode->GetBBox().GetCenter().y, pNode->GetBBox().GetCenter().z
						//                                   , pNode->GetName(), layerId, pStatObj ? pStatObj->GetFilePath() : "NONE");
						culledNum++;
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

						int lodId = GetCVars()->e_svoTI_ObjectsLod;

						if (bThisIsLowLodNode)
							lodId++;

						info.pStatObj = (CStatObj*)info.pStatObj->GetLodObject(lodId, true);

						CStatObj* pParent = info.pStatObj->GetParentObject() ? ((CStatObj*)info.pStatObj->GetParentObject()) : (CStatObj*)info.pStatObj;
						EFileStreamingStatus eStreamingStatusParent = pParent->m_eStreamingStatus;
						bool bUnloadable = pParent->IsUnloadable();

						if (pNode->GetRenderNodeType() == eERType_Vegetation)
						{
							info.objectScale = ((CVegetation*)pNode)->GetScale();
						}
						else if (pNode->GetRenderNodeType() == eERType_MovableBrush || pNode->GetRenderNodeType() == eERType_Brush)
						{
							Vec3 vScaleAbs = info.matObj.TransformVector(Vec3(1, 1, 1)).abs();
							info.objectScale = min(min(vScaleAbs.x, vScaleAbs.y), vScaleAbs.z);
						}
						else
							assert(!"Undefined object type");

						info.pMat = pMaterial;

						if (info.pStatObj->m_nFlags & STATIC_OBJECT_HIDDEN)
							continue;

						info.bIndoor = pNode->GetEntityVisArea() != 0 || (pNode->GetRndFlags() & ERF_REGISTER_BY_BBOX);

						info.bVegetation = (objType == eERType_Vegetation);

						info.maxViewDist = maxViewDist;

						if (parrObjects)
						{
							AUTO_MODIFYLOCK(CVoxelSegment::m_arrLockedMaterials.m_Lock);
							CVoxelSegment::m_arrLockedMaterials[info.pMat] = info.pMat;

							parrObjects->Add(info);
						}
						else if (eStreamingStatusParent != ecss_Ready && bUnloadable)
						{
							// request streaming of missing meshes
							if (Cry3DEngineBase::GetCVars()->e_svoTI_VoxelizationPostpone == 2)
							{
								info.pStatObj->UpdateStreamableComponents(0.5f, info.matObj, false, lodId);
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

void CVoxelSegment::FindTrianglesForVoxelization(PodArray<int>*& rpNodeTrisXYZ)
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
	//    cloudBoxWS.min -= (vCloudSize/kVoxTexMaxDim)/2;
	//    cloudBoxWS.max += (vCloudSize/kVoxTexMaxDim)/2;

	//if(!m_pParentCloud)
	if (bThisIsAreaParent || bThisIsLowLodNode)
	{
		// get tris from real level geometry

		//		PodArray<SRayHitTriangle> allTrisInArea;
		//	allTrisInArea.PreAllocate(4000);

		float startTimeAll = GetCurAsyncTimeSec();
		//		PrintMessage("VoxelizeMeshes: starting triangle search for node id %d (size=%d)", m_nId, (int)GetBoxSize());

		SSuperMesh superMesh;
		PodArray<SMINDEX> arrVertHash[hashDim][hashDim][hashDim];

		//      if(nCulled)
		//        PrintMessage("  %d objects culled", nCulled);

		// add terrain
		if (Get3DEngine()->m_bShowTerrainSurface)
		{
			//float startTime = GetCurAsyncTimeSec();

			CTerrain* pTerrain = GetTerrain();
			int worldSize = pTerrain->GetTerrainSize();
			int S = (int)max(2.f, pTerrain->GetHeightMapUnitSize());

			if (bThisIsLowLodNode)
				S *= 4;

			int halfStep = S / 2;

			SRayHitTriangle ht;
			ZeroStruct(ht);
			ht.c[0] = ht.c[1] = ht.c[2] = Col_White;
			ht.nOpacity = 255;
			ht.nHitObjType = HIT_OBJ_TYPE_TERRAIN;
			Plane pl;

			int I = 0, X = 0, Y = 0;

			superMesh.Clear(&arrVertHash[0][0][0]);

			for (int x = (int)cloudBoxWS.min.x; x < (int)cloudBoxWS.max.x; x += S)
			{
				for (int y = (int)cloudBoxWS.min.y; y < (int)cloudBoxWS.max.y; y += S)
				{
					if (!pTerrain->GetHole(x + halfStep, y + halfStep))
					{
						// prevent surface interpolation over long edge
						bool bFlipTris = false;
						int type10 = pTerrain->GetSurfaceTypeID(x + S, y);
						int type01 = pTerrain->GetSurfaceTypeID(x, y + S);
						if (type10 != type01)
						{
							int type00 = pTerrain->GetSurfaceTypeID(x, y);
							int type11 = pTerrain->GetSurfaceTypeID(x + S, y + S);
							if ((type10 == type00 && type10 == type11) || (type01 == type00 && type01 == type11))
								bFlipTris = true;
						}

						if (bFlipTris)
						{
							I = 0;
							X = x + S, Y = y + 0;
							ht.v[I].Set((float)X, (float)Y, pTerrain->GetZ(X, Y));
							I++;
							X = x + S, Y = y + S;
							ht.v[I].Set((float)X, (float)Y, pTerrain->GetZ(X, Y));
							I++;
							X = x + 0, Y = y + 0;
							ht.v[I].Set((float)X, (float)Y, pTerrain->GetZ(X, Y));
							I++;

							if (Overlap::AABB_Triangle(cloudBoxWS, ht.v[0], ht.v[1], ht.v[2]))
							{
								ht.nTriArea = SATURATEB(int(SVO_AREA_SCALE * 0.5f * (ht.v[1] - ht.v[0]).Cross(ht.v[2] - ht.v[0]).GetLength()));
								pl.SetPlane(ht.v[0], ht.v[1], ht.v[2]);
								ht.n = pl.n;

								superMesh.AddSuperTriangle(ht, arrVertHash, kAllObjectLayersId);
							}

							I = 0;
							X = x + 0, Y = y + 0;
							ht.v[I].Set((float)X, (float)Y, pTerrain->GetZ(X, Y));
							I++;
							X = x + S, Y = y + S;
							ht.v[I].Set((float)X, (float)Y, pTerrain->GetZ(X, Y));
							I++;
							X = x + 0, Y = y + S;
							ht.v[I].Set((float)X, (float)Y, pTerrain->GetZ(X, Y));
							I++;

							if (Overlap::AABB_Triangle(cloudBoxWS, ht.v[0], ht.v[1], ht.v[2]))
							{
								ht.nTriArea = SATURATEB(int(SVO_AREA_SCALE * 0.5f * (ht.v[1] - ht.v[0]).Cross(ht.v[2] - ht.v[0]).GetLength()));
								pl.SetPlane(ht.v[0], ht.v[1], ht.v[2]);
								ht.n = pl.n;

								superMesh.AddSuperTriangle(ht, arrVertHash, kAllObjectLayersId);
							}
						}
						else
						{
							I = 0;
							X = x + 0, Y = y + 0;
							ht.v[I].Set((float)X, (float)Y, pTerrain->GetZ(X, Y));
							I++;
							X = x + S, Y = y + 0;
							ht.v[I].Set((float)X, (float)Y, pTerrain->GetZ(X, Y));
							I++;
							X = x + 0, Y = y + S;
							ht.v[I].Set((float)X, (float)Y, pTerrain->GetZ(X, Y));
							I++;

							if (Overlap::AABB_Triangle(cloudBoxWS, ht.v[0], ht.v[1], ht.v[2]))
							{
								ht.nTriArea = SATURATEB(int(SVO_AREA_SCALE * 0.5f * (ht.v[1] - ht.v[0]).Cross(ht.v[2] - ht.v[0]).GetLength()));
								pl.SetPlane(ht.v[0], ht.v[1], ht.v[2]);
								ht.n = pl.n;

								superMesh.AddSuperTriangle(ht, arrVertHash, kAllObjectLayersId);
							}

							I = 0;
							X = x + S, Y = y + 0;
							ht.v[I].Set((float)X, (float)Y, pTerrain->GetZ(X, Y));
							I++;
							X = x + S, Y = y + S;
							ht.v[I].Set((float)X, (float)Y, pTerrain->GetZ(X, Y));
							I++;
							X = x + 0, Y = y + S;
							ht.v[I].Set((float)X, (float)Y, pTerrain->GetZ(X, Y));
							I++;

							if (Overlap::AABB_Triangle(cloudBoxWS, ht.v[0], ht.v[1], ht.v[2]))
							{
								ht.nTriArea = SATURATEB(int(SVO_AREA_SCALE * 0.5f * (ht.v[1] - ht.v[0]).Cross(ht.v[2] - ht.v[0]).GetLength()));
								pl.SetPlane(ht.v[0], ht.v[1], ht.v[2]);
								ht.n = pl.n;

								superMesh.AddSuperTriangle(ht, arrVertHash, kAllObjectLayersId);
							}
						}
					}
				}
			}

			AUTO_MODIFYLOCK(m_superMeshLock);

			AddSuperMesh(superMesh, SVO_OFFSET_TERRAIN);
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
			nodeHitInfo.fMaxHitDistance = GetBoxSize() / 2.f / info.objectScale * sqrt(3.f);

			arrTris.Clear();
			nodeHitInfo.pHitTris = &arrTris;

			nodeHitInfo.fMinHitOpacity = GetCVars()->e_svoTI_MinVoxelOpacity;
			int minVoxelOpacity = (int)(GetCVars()->e_svoTI_MinVoxelOpacity * 255.f);

			float timeRayIntersection = Cry3DEngineBase::GetTimer()->GetAsyncCurTime();

			info.pStatObj->RayIntersection(nodeHitInfo, info.pMat);

			if (arrTris.Count())
			{
				{
					AUTO_MODIFYLOCK(CVoxelSegment::m_arrLockedMaterials.m_Lock);
					CVoxelSegment::m_arrLockedMaterials[info.pMat] = info.pMat;

					for (int s = 0; s < info.pMat->GetSubMtlCount(); s++)
					{
						IMaterial* pSubMtl = info.pMat->GetSafeSubMtl(s);
						CVoxelSegment::m_arrLockedMaterials[pSubMtl] = pSubMtl;
					}
				}

				superMesh.Clear(&arrVertHash[0][0][0]);

				float epsilon = GetCVars()->e_svoTI_ObjectsMaxViewDistance ? (VEC_EPSILON / 2) : (VEC_EPSILON / 10);

				for (int t = 0; t < arrTris.Count(); t++)
				{
					SRayHitTriangle ht = arrTris[t];

					// Workaround for over occlusion from vegetation; TODO: make thin geometry produce less occlusion
					if (ht.pMat)
					{
						SShaderItem& rSI = ht.pMat->GetShaderItem();

						if (rSI.m_pShaderResources && rSI.m_pShader && (rSI.m_pShader->GetShaderType() == eST_Vegetation || info.bVegetation))
						{
							bool vegetationLeaves = (rSI.m_pShaderResources->GetAlphaRef() > 0.05f && rSI.m_pShader->GetShaderType() == eST_Vegetation);

							if (vegetationLeaves)
							{
								ht.nOpacity = min(ht.nOpacity, uint8(SATURATEB(GetCVars()->e_svoTI_VegetationMaxOpacity * 255.f)));
							}
							else
							{
								float midZ = 0;
								for (int v = 0; v < 3; v++)
								{
									midZ += ht.v[v].z;
								}
								midZ *= 0.333f;

								ht.nOpacity = min(ht.nOpacity, uint8(SATURATEB(LERP(255.f, GetCVars()->e_svoTI_VegetationMaxOpacity * 255.f, SATURATE(midZ * .5f)))));
							}
						}
					}

					if (ht.nOpacity < minVoxelOpacity)
						continue;

					for (int v = 0; v < 3; v++)
						ht.v[v] = info.matObj.TransformPoint(ht.v[v]);

					ht.nTriArea = SATURATEB(int(SVO_AREA_SCALE * 0.5f * (ht.v[1] - ht.v[0]).Cross(ht.v[2] - ht.v[0]).GetLength()));
					Plane pl;
					pl.SetPlane(ht.v[0], ht.v[1], ht.v[2]);
					ht.n = pl.n;

					if (!ht.v[0].IsEquivalent(ht.v[1], epsilon) && !ht.v[1].IsEquivalent(ht.v[2], epsilon) && !ht.v[2].IsEquivalent(ht.v[0], epsilon))
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
									if (vH.z > (GetTerrain()->GetZ((int)vH.x, (int)vH.y) - 1.f) || GetTerrain()->GetHole((int)vH.x, (int)vH.y))
									{
										bSkipUnderTerrain = false;
										break;
									}
								}
							}

							if (!bSkipUnderTerrain)
							{
								superMesh.AddSuperTriangle(ht, arrVertHash, info.objectLayerId);
							}
						}
				}

				{
					AUTO_MODIFYLOCK(m_superMeshLock);

					int meshMemoryAllocated = m_pTrisInArea ? (m_pTrisInArea->ComputeSizeInMemory() + m_pVertInArea->ComputeSizeInMemory()) : 0;
					int meshMemoryToAppend = superMesh.m_pTrisInArea ? (superMesh.m_pTrisInArea->ComputeSizeInMemory() + superMesh.m_pVertInArea->ComputeSizeInMemory()) : 0;

					if ((meshMemoryAllocated + meshMemoryToAppend) > GetCVars()->e_svoMaxAreaMeshSizeKB * 1024)
					{
						// skip low importance objects
						break;
					}

					AddSuperMesh(superMesh, SVO_OFFSET_MESH);
				}
			}

			timeRayIntersection = Cry3DEngineBase::GetTimer()->GetAsyncCurTime() - timeRayIntersection;

			if (GetCVars()->e_svoDebug)
			{
				AUTO_MODIFYLOCK(CVoxelSegment::m_cgfTimeStatsLock);
				m_cgfTimeStats[info.pStatObj] += timeRayIntersection;
			}
		}

		if (GetSubSetsNum() > 1)
		{
			AABB cloudBoxWS_VisAreaEx = cloudBoxWS;
			cloudBoxWS_VisAreaEx.Expand(Vec3(SVO_OFFSET_VISAREA, SVO_OFFSET_VISAREA, SVO_OFFSET_VISAREA));

			// add visarea shapes
			for (int v = 0; !bThisIsLowLodNode; v++)
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
				float height = pVisArea->GetHeight();

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
					ht.v[1] = v0 + Vec3(0, 0, height);
					ht.v[2] = v1;

					if (Overlap::AABB_Triangle(cloudBoxWS_VisAreaEx, ht.v[0], ht.v[1], ht.v[2]))
					{
						ht.nTriArea = SATURATEB(int(SVO_AREA_SCALE * 0.5f * (ht.v[1] - ht.v[0]).Cross(ht.v[2] - ht.v[0]).GetLength()));
						pl.SetPlane(ht.v[0], ht.v[1], ht.v[2]);
						ht.n = pl.n;

						superMesh.AddSuperTriangle(ht, arrVertHash, kAllObjectLayersId);
					}

					ht.v[0] = v1;
					ht.v[1] = v0 + Vec3(0, 0, height);
					ht.v[2] = v1 + Vec3(0, 0, height);

					if (Overlap::AABB_Triangle(cloudBoxWS_VisAreaEx, ht.v[0], ht.v[1], ht.v[2]))
					{
						ht.nTriArea = SATURATEB(int(SVO_AREA_SCALE * 0.5f * (ht.v[1] - ht.v[0]).Cross(ht.v[2] - ht.v[0]).GetLength()));
						pl.SetPlane(ht.v[0], ht.v[1], ht.v[2]);
						ht.n = pl.n;

						superMesh.AddSuperTriangle(ht, arrVertHash, kAllObjectLayersId);
					}
				}

				// top and bottom
				for (float h = 0; fabs(h) <= fabs(height); h += height)
				{
					for (int p = 0; p < ((int)nPoints - 2l); p++)
					{
						ht.v[0] = (pPoints)[0 + 0] + Vec3(0, 0, h);
						ht.v[1] = (pPoints)[p + 1] + Vec3(0, 0, h);
						ht.v[2] = (pPoints)[p + 2] + Vec3(0, 0, h);

						if (Overlap::AABB_Triangle(cloudBoxWS_VisAreaEx, ht.v[0], ht.v[1], ht.v[2]))
						{
							ht.nTriArea = SATURATEB(int(SVO_AREA_SCALE * 0.5f * (ht.v[1] - ht.v[0]).Cross(ht.v[2] - ht.v[0]).GetLength()));
							pl.SetPlane(ht.v[0], ht.v[1], ht.v[2]);
							ht.n = pl.n;

							superMesh.AddSuperTriangle(ht, arrVertHash, kAllObjectLayersId);
						}
					}
				}

				AUTO_MODIFYLOCK(m_superMeshLock);

				AddSuperMesh(superMesh, SVO_OFFSET_VISAREA);
			}
		}

		//		if(allTrisInArea.Count())
		//		qsort(allTrisInArea.GetElements(), allTrisInArea.Count(), sizeof(allTrisInArea[0]), CompareTriArea);

		m_voxTrisCounter += m_pTrisInArea ? m_pTrisInArea->Count() : 0;

		//    if(m_nodeTrisAllMerged.Count())
		//      PrintMessage("VoxelizeMeshes: %d tris found for node id %d (size=%d) in %.2f sec", m_nodeTrisAllMerged.Count(), m_nId, (int)GetBoxSize(), GetCurAsyncTimeSec() - fStartTimeAll);

		//    if(allTrisInLevel.Count())
		//      PrintMessage("VoxelizeMeshes: max triangle area: %.2f, min triangle area: %.2f", allTrisInLevel[0].triArea, allTrisInLevel.Last().triArea);

	#ifdef FEATURE_SVO_GI_USE_MESH_RT
		if (GetCVars()->e_svoTI_RT_MaxDist && gSvoEnv->m_texTrisPoolId)
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

		if (gEnv->IsEditor() && (gSvoEnv->m_streamingStartTime < 0) && !m_bExportMode)
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

	const float voxSizeExpand = GetBoxSize() / (float)SVO_VOX_BRICK_MAX_SIZE / 8.f;
	triBox.Expand(Vec3(voxSizeExpand, voxSizeExpand, voxSizeExpand)); // for RT

	AABB nodeBoxWS = m_boxOS;
	nodeBoxWS.min += m_vSegOrigin;
	nodeBoxWS.max += m_vSegOrigin;

	// safety border support
	Vec3 vBoxSize = nodeBoxWS.GetSize();
	//    nodeBoxWS.min -= (vBoxSize/kVoxTexMaxDim)/2;
	//    nodeBoxWS.max += (vBoxSize/kVoxTexMaxDim)/2;

	int x0 = clamp_tpl<int>((int)(((triBox.min.x - nodeBoxWS.min.x) / vBoxSize.x * SVO_VOX_BRICK_MAX_SIZE)), (int)0, SVO_VOX_BRICK_MAX_SIZE - 1);
	int x1 = clamp_tpl<int>((int)(((triBox.max.x - nodeBoxWS.min.x) / vBoxSize.x * SVO_VOX_BRICK_MAX_SIZE)), (int)0, SVO_VOX_BRICK_MAX_SIZE - 1);

	int y0 = clamp_tpl<int>((int)(((triBox.min.y - nodeBoxWS.min.y) / vBoxSize.y * SVO_VOX_BRICK_MAX_SIZE)), (int)0, SVO_VOX_BRICK_MAX_SIZE - 1);
	int y1 = clamp_tpl<int>((int)(((triBox.max.y - nodeBoxWS.min.y) / vBoxSize.y * SVO_VOX_BRICK_MAX_SIZE)), (int)0, SVO_VOX_BRICK_MAX_SIZE - 1);

	int z0 = clamp_tpl<int>((int)(((triBox.min.z - nodeBoxWS.min.z) / vBoxSize.z * SVO_VOX_BRICK_MAX_SIZE)), (int)0, SVO_VOX_BRICK_MAX_SIZE - 1);
	int z1 = clamp_tpl<int>((int)(((triBox.max.z - nodeBoxWS.min.z) / vBoxSize.z * SVO_VOX_BRICK_MAX_SIZE)), (int)0, SVO_VOX_BRICK_MAX_SIZE - 1);

	for (int z = z0; z <= z1; z++)
	{
		for (int y = y0; y <= y1; y++)
		{
			for (int x = x0; x <= x1; x++)
			{
				rpNodeTrisXYZ[z * SVO_VOX_BRICK_MAX_SIZE * SVO_VOX_BRICK_MAX_SIZE + y * SVO_VOX_BRICK_MAX_SIZE + x].Add(trId);
			}
		}
	}

	m_nodeTrisAllMerged.Add(trId);

	AABB boxWS = m_boxOS;
	boxWS.min += m_vSegOrigin;
	boxWS.max += m_vSegOrigin;

	for (int childId = 0; childId < 8; childId++)
	{
		if (m_dwChildTrisTest & (1 << childId))
			continue;

		AABB childBox = CVoxelSegment::GetChildBBox(boxWS, childId);

		childBox.min -= (vBoxSize / (float)SVO_VOX_BRICK_MAX_SIZE) / 4.0f;
		childBox.max += (vBoxSize / (float)SVO_VOX_BRICK_MAX_SIZE) / 4.0f;

		if (Overlap::AABB_Triangle(childBox, arrV[0], arrV[1], arrV[2]))
			m_dwChildTrisTest |= (1 << childId);
	}

	if (m_objLayerMap.find(tr.objectLayerId) == m_objLayerMap.end())
	{
		//    if (tr.nObjLayerId)
		//    {
		//      PrintMessage("Allocating layer id %d", tr.nObjLayerId);
		//    }

		SVoxBrick voxData;

		for (int s = 0; s < GetSubSetsNum(); s++)
		{
			CheckAllocateBrick(voxData.pData[s], SVO_VOX_BRICK_MAX_SIZE * SVO_VOX_BRICK_MAX_SIZE * SVO_VOX_BRICK_MAX_SIZE);
		}

		m_objLayerMap[tr.objectLayerId] = voxData;
	}
}

void CVoxelSegment::SetVoxCamera(const CCamera& newCam)
{
	CVoxelSegment::m_voxCam = newCam;

	if (gSvoEnv)
	{
		gSvoEnv->m_debugDrawVoxelsCounter = 0;
	}
}

ColorF CVoxelSegment::ProcessMaterial(const SRayHitTriangleIndexed& tr, const Vec3& vHitPos)
{
	ColorF colVert = Col_White;
	Vec2 vHitTC(0, 0);

	SSvoMatInfo& rMI = m_pMatsInArea->GetAt(tr.materialID);

	SShaderItem* pShItem = rMI.pMat ? &rMI.pMat->GetShaderItem() : nullptr;

	Vec3 arrV[3] = { (*m_pVertInArea)[tr.arrVertId[0]].v, (*m_pVertInArea)[tr.arrVertId[1]].v, (*m_pVertInArea)[tr.arrVertId[2]].v };

	float w0 = 0, w1 = 0, w2 = 0;
	if (GetBarycentricTC(arrV[0], arrV[1], arrV[2], w0, w1, w2, vHitPos, 2.f))
	{
		Vec2 arrT[3] = { (*m_pVertInArea)[tr.arrVertId[0]].t, (*m_pVertInArea)[tr.arrVertId[1]].t, (*m_pVertInArea)[tr.arrVertId[2]].t };

		vHitTC = arrT[0] * w0 + arrT[1] * w1 + arrT[2] * w2;

		if (GetSubSetsNum() > 1 && (!(pShItem && pShItem->m_pShader) || pShItem->m_pShader->GetFlags2() & EF2_VERTEXCOLORS))
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
	}
	else
	{
		colVert = Col_DimGray;
	}

	ColorF colTex = Col_Gray;

	ColorB* pTexRgb = 0;
	int texWidth = 0, texHeight = 0;

	if (rMI.pMat)
	{
		// objects
		pTexRgb = rMI.pTexRgb;
		texWidth = rMI.textureWidth;
		texHeight = rMI.textureHeight;
	}
	else if (const PodArray<ColorB>* pTerrLowResTex = GetTerrain()->GetTerrainRgbLowResSystemCopy())
	{
		// terrain
		texWidth = texHeight = (int)sqrt((float)pTerrLowResTex->Count());
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
			  pTexRgb, texWidth, texHeight, 1.f / 255.f);
			colTex.srgb2rgb();

			// ignore alpha if material do not use it
			if (pShItem && pShItem->m_pShaderResources && (pShItem->m_pShaderResources->GetAlphaRef() == 0.f) && (pShItem->m_pShaderResources->GetStrengthValue(EFTT_OPACITY) == 1.f))
				colTex.a = 1;
		}
		else
		{
			// terrain tex-gen
			int worldSize = GetTerrain()->GetTerrainSize();
			colTex = GetBilinearAt(
			  vHitPos.y / worldSize,
			  vHitPos.x / worldSize,
			  pTexRgb, texWidth, texHeight, 1.f / 255.f);

			colTex.srgb2rgb();
			colTex *= GetTerrain()->GetTerrainTextureMultiplier();

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

	int val = x * 256 * 256 + y;

	return 0.1f + (float)val;
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
					gSvoEnv->m_arrRTPoolTris.PreAllocate(m_voxTexPoolDimXY * m_voxTexPoolDimXY * m_voxTexPoolDimZ);

					tr.nGLobalId = gSvoEnv->m_arrRTPoolTris.Count() / 4;

					// add triangle into pool
					for (int i = 0; i < 3; i++)
						gSvoEnv->m_arrRTPoolTris.Add(Vec4(tr.v[i] - tr.n * (tr.nMatID ? SVO_OFFSET_MESH : SVO_OFFSET_TERRAIN), CompressTC(tr.t[i])));
					gSvoEnv->m_arrRTPoolTris.Add(Vec4(0.1f + (float)(*pLowResSystemCopyAtlasId), (float)nTexW / (float)m_voxTexPoolDimXY, 0, 0));

					gSvoEnv->m_arrRTPoolTris.m_bModified = true;
				}
			}
		}
	}
	#endif
}

void CVoxelSegment::CheckStoreTextureInPool(SShaderItem* pShItem, uint16& nTexW, uint16& nTexH, int** ppSysTexId)
{
	#ifdef FEATURE_SVO_GI_USE_MESH_RT
	static int s_atlasSlotId = 0;
	if (s_atlasSlotId >= m_voxTexPoolDimZ)
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

					if (nTexW > m_voxTexPoolDimXY || nTexH > m_voxTexPoolDimXY)
						CVoxelSegment::ErrorTerminate("CheckStoreTextureInPool");

					gSvoEnv->m_arrRTPoolTexs.PreAllocate(m_voxTexPoolDimXY * m_voxTexPoolDimXY * m_voxTexPoolDimZ, s_atlasSlotId * m_voxTexPoolDimXY * m_voxTexPoolDimXY);

					// apply high-pass filter
					ColorB* pTexRgbHP = new ColorB[nTexW * nTexH];
					ColorF* pBlurredF = new ColorF[nTexW * nTexH];

					// make blurred copy
					for (int x = 0; x < nTexW; x++)
					{
						for (int y = 0; y < nTexH; y++)
						{
							ColorF colAver(0, 0, 0, 0);

							int samplingRange = 8;

							for (int i = -samplingRange; i <= samplingRange; i += 2)
							{
								for (int j = -samplingRange; j <= samplingRange; j += 2)
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

					for (int lineId = 0; lineId < nTexH; lineId++)
						memcpy(gSvoEnv->m_arrRTPoolTexs.GetElements() + s_atlasSlotId * (m_voxTexPoolDimXY * m_voxTexPoolDimXY) + lineId * m_voxTexPoolDimXY, pTexRgbHP + lineId * nTexW, nTexW * sizeof(ColorB));

					delete[] pBlurredF;
					delete[] pTexRgbHP;

					(**ppSysTexId) = s_atlasSlotId + 1;
					s_atlasSlotId++;

					gSvoEnv->m_arrRTPoolTexs.m_bModified = true;
				}
			}
		}
	}
	#endif
}

int CVoxelSegment::StoreIndicesIntoPool(const PodArray<int>& nodeTInd, int& countStored)
{
	#ifdef FEATURE_SVO_GI_USE_MESH_RT
	if (nodeTInd.Count() && GetBoxSize() == Cry3DEngineBase::GetCVars()->e_svoMinNodeSize)
	{
		AUTO_MODIFYLOCK(gSvoEnv->m_arrRTPoolInds.m_Lock);

		int startId = gSvoEnv->m_arrRTPoolInds.Count();

		gSvoEnv->m_arrRTPoolInds.PreAllocate(m_voxTexPoolDimXY * m_voxTexPoolDimXY * m_voxTexPoolDimZ);

		for (int t = 0; t < nodeTInd.Count() && t < 255; t++)
		{
			int trId = nodeTInd[t];
			SRayHitTriangle& tr = (*m_pTrisInArea)[trId];
			int id = tr.nGLobalId;

			ColorB valOut;
			valOut.r = id & 255;
			id /= 256;
			valOut.g = id & 255;
			id /= 256;
			valOut.b = id & 255;
			id /= 256;
			valOut.a = id & 255;
			id /= 256;
			gSvoEnv->m_arrRTPoolInds.Add(valOut);
		}

		gSvoEnv->m_arrRTPoolInds.m_bModified = true;

		countStored = gSvoEnv->m_arrRTPoolInds.Count() - startId;

		return startId;
	}

	countStored = 0;
	#endif

	return 0;
}

void CVoxelSegment::DebugDrawVoxels()
{
	if (Cry3DEngineBase::GetCVars()->e_svoDebug == 6 || Cry3DEngineBase::GetCVars()->e_svoDebug == 3)
	{
		if (m_voxData.pData[SVoxBrick::OPA3D] && CVoxelSegment::m_voxCam.IsAABBVisible_F(m_pNode->m_nodeBox) && gSvoEnv->m_debugDrawVoxelsCounter < 100000)
		{
			Vec4 voxNodeData[SVO_NODE_BRICK_SIZE * SVO_NODE_BRICK_SIZE * SVO_NODE_BRICK_SIZE];
			ZeroStruct(voxNodeData);
			voxNodeData[0] = Vec4(m_boxOS.min + m_vSegOrigin, 0);
			voxNodeData[1] = Vec4(m_boxOS.max + m_vSegOrigin, 0);
			voxNodeData[0] = voxNodeData[0] + Vec4(Vec3((float)m_vCropBoxMin.x / SVO_VOX_BRICK_MAX_SIZE, (float)m_vCropBoxMin.y / SVO_VOX_BRICK_MAX_SIZE, (float)m_vCropBoxMin.z / SVO_VOX_BRICK_MAX_SIZE) * GetBoxSize(), 0);
			voxNodeData[1] = voxNodeData[0] + Vec4(Vec3((float)m_vCropTexSize.x / SVO_VOX_BRICK_MAX_SIZE, (float)m_vCropTexSize.y / SVO_VOX_BRICK_MAX_SIZE, (float)m_vCropTexSize.z / SVO_VOX_BRICK_MAX_SIZE) * GetBoxSize(), 0);

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
						ColorB& opaOutFin = m_voxData.pData[SVoxBrick::OPA3D][id];

						if (opaOutFin.r || opaOutFin.g || opaOutFin.b)
						{
							Vec4 vMin = voxNodeData[0] + (voxNodeData[1] - voxNodeData[0]) * Vec4((float) x / m_vCropTexSize.x, (float) y / m_vCropTexSize.y, (float) z / m_vCropTexSize.z, 1);
							Vec4 vMax = voxNodeData[0] + (voxNodeData[1] - voxNodeData[0]) * Vec4((float) (x + 1) / m_vCropTexSize.x, (float) (y + 1) / m_vCropTexSize.y, (float) (z + 1) / m_vCropTexSize.z, 1);

							//          // safety border support
							//          if(Cry3DEngineBase::GetCVars()->e_svoDebug == 6)
							//          {
							//            Vec4 vCenter(m_vOrigin.x,m_vOrigin.y,m_vOrigin.z,0);
							//            vMin += (vMin - vCenter)/(kVoxTexMaxDim/2)/2;
							//            vMax += (vMax - vCenter)/(kVoxTexMaxDim/2)/2;
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

							gSvoEnv->m_debugDrawVoxelsCounter++;
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

const float kSvoSuperMeshHashScale = .1f;

int SSuperMesh::FindVertex(const Vec3& rPos, const Vec2 rTC, PodArray<SMINDEX> arrVertHash[hashDim][hashDim][hashDim], PodArrayRT<SRayHitVertex>& vertsInArea)
{
	Vec3i vPosI0 = rPos / kSvoSuperMeshHashScale - Vec3(VEC_EPSILON, VEC_EPSILON, VEC_EPSILON);
	Vec3i vPosI1 = rPos / kSvoSuperMeshHashScale + Vec3(VEC_EPSILON, VEC_EPSILON, VEC_EPSILON);

	for (int x = vPosI0.x; x <= vPosI1.x; x++)
		for (int y = vPosI0.y; y <= vPosI1.y; y++)
			for (int z = vPosI0.z; z <= vPosI1.z; z++)
			{
				Vec3i vPosI(x, y, z);

				PodArray<SMINDEX>& rSubIndices = arrVertHash[vPosI.x & (hashDim - 1)][vPosI.y & (hashDim - 1)][vPosI.z & (hashDim - 1)];

				for (int ii = 0; ii < rSubIndices.Count(); ii++)
				{
					if (vertsInArea[rSubIndices[ii]].v.IsEquivalent(rPos))
						if (vertsInArea[rSubIndices[ii]].t.IsEquivalent(rTC))
							return rSubIndices[ii];
				}
			}

	return -1;
}

int SSuperMesh::AddVertex(const SRayHitVertex& rVert, PodArray<SMINDEX> arrVertHash[hashDim][hashDim][hashDim], PodArrayRT<SRayHitVertex>& vertsInArea)
{
	Vec3 vPosS = rVert.v / kSvoSuperMeshHashScale;

	Vec3i vPosI(int(floor(vPosS.x)), int(floor(vPosS.y)), int(floor(vPosS.z)));

	PodArray<SMINDEX>& rSubIndices = arrVertHash[vPosI.x & (hashDim - 1)][vPosI.y & (hashDim - 1)][vPosI.z & (hashDim - 1)];

	rSubIndices.Add(vertsInArea.Count());

	vertsInArea.Add(rVert);

	return vertsInArea.Count() - 1;
}

void SSuperMesh::AddSuperTriangle(SRayHitTriangle& htIn, PodArray<SMINDEX> arrVertHash[hashDim][hashDim][hashDim], ObjectLayerIdType nObjLayerId)
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
	htOut.vFaceNorm = htIn.n;

	SSvoMatInfo matInfo;
	matInfo.pMat = htIn.pMat;
	int matId = m_pMatsInArea->Find(matInfo);
	if (matId < 0)
	{
		matId = m_pMatsInArea->Count();

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
						CVoxelSegment::m_arrLockedTextures[pITex] = pITex;
						matInfo.pTexRgb = (ColorB*)pITex->GetLowResSystemCopy(matInfo.textureWidth, matInfo.textureHeight, &pLowResSystemCopyAtlasId);
					}
				}
			}
		}

		m_pMatsInArea->Add(matInfo);
	}
	htOut.materialID = matId;
	htOut.objectLayerId = nObjLayerId;

	#ifdef FEATURE_SVO_GI_USE_MESH_RT
	htOut.globalId = htIn.nGLobalId;
	#endif
	htOut.triArea = htIn.nTriArea;
	htOut.opacity = htIn.nOpacity;
	htOut.hitObjectType = htIn.nHitObjType;

	for (int v = 0; v < 3; v++)
	{
		int vertId = FindVertex(htIn.v[v], htIn.t[v], arrVertHash, *m_pVertInArea);

		if (vertId < 0)
		{
			SRayHitVertex hv;
			hv.v = htIn.v[v];
			hv.t = htIn.t[v];
			hv.c = htIn.c[v];

			vertId = AddVertex(hv, arrVertHash, *m_pVertInArea);
		}

		htOut.arrVertId[v] = vertId;
	}

	m_pTrisInArea->Add(htOut);
	m_pFaceNormals->Add(htIn.n);
}

void SSuperMesh::AddSuperMesh(SSuperMesh& smIn, float vertexOffset)
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
		smIn.m_pVertInArea->GetAt(v).v += vertInNormals[v].GetNormalized() * vertexOffset;
		m_boxTris.Add(smIn.m_pVertInArea->GetAt(v).v);
	}

	if (!m_pTrisInArea)
	{
		m_pTrisInArea = new PodArrayRT<SRayHitTriangleIndexed>;
		m_pVertInArea = new PodArrayRT<SRayHitVertex>;
		m_pMatsInArea = new PodArrayRT<SSvoMatInfo>;
	}

	int numVertBefore = m_pVertInArea->Count();

	m_pTrisInArea->PreAllocate(m_pTrisInArea->Count() + smIn.m_pTrisInArea->Count());

	for (int t = 0; t < smIn.m_pTrisInArea->Count(); t++)
	{
		SRayHitTriangleIndexed tr = smIn.m_pTrisInArea->GetAt(t);

		for (int v = 0; v < 3; v++)
			tr.arrVertId[v] += numVertBefore;

		SSvoMatInfo matInfo;
		matInfo.pMat = (*smIn.m_pMatsInArea)[tr.materialID].pMat;

		int matId = m_pMatsInArea->FindReverse(matInfo);
		if (matId < 0)
		{
			matId = m_pMatsInArea->Count();

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
							CVoxelSegment::m_arrLockedTextures[pITex] = pITex;
							matInfo.pTexRgb = (ColorB*)pITex->GetLowResSystemCopy(matInfo.textureWidth, matInfo.textureHeight, &pLowResSystemCopyAtlasId);
						}
					}
				}
			}

			m_pMatsInArea->Add(matInfo);
		}
		tr.materialID = matId;

		m_pTrisInArea->Add(tr);
	}

	m_pVertInArea->AddList(*smIn.m_pVertInArea);

	if (vertexOffset == SVO_OFFSET_TERRAIN)
	{
		AddSuperMesh(smIn, -1.f);
	}

	smIn.Clear(nullptr);
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
		for (int i = 0; i < hashDim * hashDim * hashDim; i++)
			parrVertHash[i].Clear();

	m_bExternalData = false;
	m_boxTris.Reset();
}

void CVoxelSegment::SaveVoxels(PodArray<byte>& arrData)
{
	int texDataSize = m_voxData.pData[SVoxBrick::OPA3D] ? (m_vCropTexSize.x * m_vCropTexSize.y * m_vCropTexSize.z * sizeof(ColorB)) : 0;

	if (SVO_PACK_TO_16_BIT && texDataSize)
	{
		texDataSize /= 2;
	}

	if (gSvoEnv->m_voxTexFormat == eTF_BC3)
	{
		Vec3i vDxtDim = GetDxtDim();
		texDataSize = (vDxtDim.x * vDxtDim.y * vDxtDim.z * sizeof(ColorB)) / 4;
	}

	int dataSize = texDataSize * m_objLayerMap.size() + m_objLayerMap.size() * sizeof(uint32) + sizeof(SVoxSegmentFileHeader);

	byte* pDataStart = new byte[dataSize];
	byte* pDataPtr = pDataStart;

	// store header
	SVoxSegmentFileHeader* pHeader = (SVoxSegmentFileHeader*)pDataStart;

	pHeader->cropTexSize.x = m_vCropTexSize.x;
	pHeader->cropTexSize.y = m_vCropTexSize.y;
	pHeader->cropTexSize.z = m_vCropTexSize.z;

	pHeader->cropBoxMin.x = m_vCropBoxMin.x;
	pHeader->cropBoxMin.y = m_vCropBoxMin.y;
	pHeader->cropBoxMin.z = m_vCropBoxMin.z;

	pHeader->dummy.zero();

	assert(m_objLayerMap.size() <= 255);
	pHeader->cropTexSize.w = (byte)m_objLayerMap.size();

	assert(GetSubSetsNum() <= 255);
	pHeader->cropBoxMin.w = GetSubSetsNum();

	pDataPtr += sizeof(SVoxSegmentFileHeader);

	for (auto& it : m_objLayerMap)
	{
		ObjectLayerIdType nLayerId = it.first;

		// store layer id's
		*((uint32*)pDataPtr) = nLayerId;
		pDataPtr += sizeof(uint32);

		// store voxel data
		if (texDataSize)
		{
			for (int s = 0; s < GetSubSetsNum(); s++)
			{
				byte* pDataIn = (byte*)it.second.pData[s];

				if (gSvoEnv->m_voxTexFormat == eTF_BC3)
				{
					CompressToDxt((ColorB*)it.second.pData[s], pDataIn, 0);
				}

				if (SVO_PACK_TO_16_BIT)
				{
					for (int i = 0; i < texDataSize; i++)
					{
						byte val0 = *(pDataIn + 0);
						byte val1 = *(pDataIn + 1);

						val0 = SATURATEB(int(powf(float(val0) / 255.f, 1.f / 2.f) * 255.f));
						val1 = SATURATEB(int(powf(float(val1) / 255.f, 1.f / 2.f) * 255.f));

						byte b0 = val0 >> 4;
						byte b1 = val1 >> 4;

						(*pDataPtr) = b0 | (b1 << 4);

						pDataIn += 2;
						pDataPtr++;
					}
				}
				else
				{
					memcpy(pDataPtr, pDataIn, texDataSize);

					pDataPtr += texDataSize;
				}
			}
		}
	}

	assert(pDataPtr == pDataStart + dataSize);

	// compress entire data block
	if (m_objLayerMap.size() && texDataSize)
	{
		CMemoryBlock* pZipped = CMemoryBlock::CompressToMemBlock(pDataStart, dataSize, GetSystem());  // TODO: try CCryPak::RawCompress
		int zipSize = pZipped->GetSize();

		arrData.AddList((byte*)&zipSize, sizeof(zipSize));
		arrData.AddList((byte*)pZipped->GetData(), zipSize);

		while (arrData.Count() & 3)
		{
			arrData.Add(103);
		}
	}
	else
	{
		int zipSize = 0;

		arrData.AddList((byte*)&zipSize, sizeof(zipSize));
	}

	SAFE_DELETE_ARRAY(pDataStart);
}

PodArray<byte> m_arrSaveCompTexture_Data[16];

void CVoxelSegment::SaveCompTexture(const void* data, size_t size, void* userData)
{
	int64 id = (int64)userData;
	m_arrSaveCompTexture_Data[id].Clear();
	m_arrSaveCompTexture_Data[id].AddList((byte*)data, size);
}

int CVoxelSegment::CompressToDxt(ColorB* pImgSource, byte*& pDxtOut, int threadId)
{
	Vec3i vSizeFin = GetDxtDim();

	static PodArray<ColorB> arrImgResizedPool[16];
	PodArray<ColorB>& arrImgResized = arrImgResizedPool[threadId];

	arrImgResized.CheckAllocated(SVO_VOX_BRICK_MAX_SIZE * SVO_VOX_BRICK_MAX_SIZE * SVO_VOX_BRICK_MAX_SIZE);
	memset(arrImgResized.GetElements(), 0, arrImgResized.GetDataSize());

	for (int z = 0; z < vSizeFin.z; z++)
	{
		for (int y = 0; y < vSizeFin.y; y++)
		{
			for (int x = 0; x < vSizeFin.x; x++)
			{
				Vec3i vXyzCropped;
				vXyzCropped.x = min(x, m_vCropTexSize.x - 1);
				vXyzCropped.y = min(y, m_vCropTexSize.y - 1);
				vXyzCropped.z = min(z, m_vCropTexSize.z - 1);

				ColorB& rIn = pImgSource[vXyzCropped.z * m_vCropTexSize.x * m_vCropTexSize.y + vXyzCropped.y * m_vCropTexSize.x + vXyzCropped.x];
				ColorB& rOut = arrImgResized[z * vSizeFin.x * vSizeFin.y + y * vSizeFin.x + x];
				rOut = rIn;
			}
		}
	}

	static PodArray<byte> arrImgDxtPool[16];
	PodArray<byte>& arrImgDxt = arrImgDxtPool[threadId];

	arrImgDxt.CheckAllocated(SVO_VOX_BRICK_MAX_SIZE * SVO_VOX_BRICK_MAX_SIZE * SVO_VOX_BRICK_MAX_SIZE);
	memset(arrImgDxt.GetElements(), 0, arrImgDxt.GetDataSize());

	for (int lineId = 0; lineId < vSizeFin.z; lineId++)
	{
		ColorB* pLayerIn = arrImgResized.GetElements() + (vSizeFin.x * vSizeFin.y * lineId);

		byte* pLayerOut = arrImgDxt.GetElements() + (vSizeFin.x * vSizeFin.y * lineId);

		GetRenderer()->DXTCompress((byte*)pLayerIn, vSizeFin.x, vSizeFin.y, gSvoEnv->m_voxTexFormat, false, 0, 4, SaveCompTexture);

		memcpy(pLayerOut, m_arrSaveCompTexture_Data[threadId].GetElements(), m_arrSaveCompTexture_Data[threadId].GetDataSize());
	}

	pDxtOut = (byte*)arrImgDxt.GetElements();

	return 0;
}

#endif
