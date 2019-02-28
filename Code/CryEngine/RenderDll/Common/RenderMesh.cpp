// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include <Cry3DEngine/I3DEngine.h>
#include <Cry3DEngine/IIndexedMesh.h>
#include <Cry3DEngine/CGF/CGFContent.h>
#include <CryMath/GeomQuery.h>
#include "RenderMesh.h"
#include "PostProcess/PostEffects.h"
#include "ComputeSkinningStorage.h"
#include <CryMath/QTangent.h>
#include <CryThreading/IJobManager_JobDelegator.h>

#include "XRenderD3D9/DriverD3D.h"

#include <algorithm>

//////////////////////////////////////////////////////////////////////////
DECLARE_JOB("CreateSubsetRenderMesh", TCreateSubsetRenderMesh, SMeshSubSetIndicesJobEntry::CreateSubSetRenderMesh);
//////////////////////////////////////////////////////////////////////////

#define RENDERMESH_ASYNC_MEMCPY_THRESHOLD (1<<10)
#define MESH_DATA_DEFAULT_ALIGN (128u)

#define RENDERMESH_BUFFER_ENABLE_DIRECT_ACCESS BUFFER_ENABLE_DIRECT_ACCESS
#ifdef MESH_TESSELLATION_RENDERER
// Rebuilding the adjaency information needs access to system copies of the buffers. Needs fixing
#undef RENDERMESH_BUFFER_ENABLE_DIRECT_ACCESS
#define RENDERMESH_BUFFER_ENABLE_DIRECT_ACCESS 0
#endif

//////////////////////////////////////////////////////////////////////////////////////
namespace 
{
	struct SScopedMeshDataLock
	{
		SScopedMeshDataLock() = default;
		SScopedMeshDataLock(CRenderMesh *pMesh) : p(pMesh) { if (p) p->LockForThreadAccess(); }
		~SScopedMeshDataLock()                             { if (p) p->UnLockForThreadAccess(); }

		SScopedMeshDataLock(const SScopedMeshDataLock&)            = delete;
		SScopedMeshDataLock &operator=(const SScopedMeshDataLock&) = delete;

		SScopedMeshDataLock(SScopedMeshDataLock&& o)            noexcept : p(o.p) { o.p = nullptr; }
		SScopedMeshDataLock &operator=(SScopedMeshDataLock&& o) noexcept
		{
			if (p) p->UnLockForThreadAccess();
			p = o.p;
			o.p = nullptr;

			return *this;
		}

		CRenderMesh* p = nullptr;
	};

	inline int GetCurrentFrameID()
	{
		return gRenDev->m_pRT->IsRenderThread() ? gRenDev->GetRenderFrameID() : gRenDev->GetMainFrameID();
	};

	inline int GetCurrentThreadID()
	{
		CRY_ASSERT(gRenDev->m_pRT->IsMainThread() || gRenDev->m_pRT->IsRenderThread());
		return gRenDev->m_pRT->IsRenderThread() ? gRenDev->GetRenderThreadID() : gRenDev->GetMainThreadID();
	};

	static inline void RelinkTail(util::list<CRenderMesh>& instance, util::list<CRenderMesh>& list)
	{
		AUTO_LOCK(CRenderMesh::m_sLinkLock);
		instance.relink_tail(&list);
	}

	struct SMeshPool
	{
		IGeneralMemoryHeap *m_MeshDataPool;
		IGeneralMemoryHeap *m_MeshInstancePool;
		void* m_MeshDataMemory;
    void* m_MeshInstanceMemory;
	  CryCriticalSection m_MeshPoolCS; 
		SMeshPoolStatistics m_MeshDataPoolStats; 
		SMeshPool()
			: m_MeshPoolCS()
			, m_MeshDataPool()
			, m_MeshInstancePool()
			, m_MeshDataMemory()
      , m_MeshInstanceMemory()
			, m_MeshDataPoolStats()
		{}
	};
	static SMeshPool s_MeshPool;

	//////////////////////////////////////////////////////////////////////////
	static buffer_size_t AlignedMeshDataSize(buffer_size_t nSize, buffer_size_t nAlign = MESH_DATA_DEFAULT_ALIGN)
	{
		return nSize = (nSize + (nAlign - 1)) & ~(nAlign - 1);
	}

	static buffer_size_t AlignedStreamableMeshDataSize(buffer_size_t nSize, buffer_size_t nAlign = MESH_DATA_DEFAULT_ALIGN)
	{
		return AlignedMeshDataSize(nSize, std::min(nAlign, CDeviceBufferManager::GetBufferAlignmentForStreaming()));
	}

	static void* AllocateMeshDataUnpooled(buffer_size_t nSize, buffer_size_t nAlign = MESH_DATA_DEFAULT_ALIGN)
	{
		return CryModuleMemalign(nSize, std::min(nAlign, CDeviceBufferManager::GetBufferAlignmentForStreaming()));
	}

	static void FreeMeshDataUnpooled(void* ptr)
	{
		if (ptr == NULL)
			return;
		CryModuleMemalignFree(ptr);
	}

	//////////////////////////////////////////////////////////////////////////
	static void* AllocateMeshData(buffer_size_t nSize, buffer_size_t nAlign = MESH_DATA_DEFAULT_ALIGN, bool bFlush = false)
	{
		nSize = AlignedMeshDataSize(nSize, nAlign);

		if (s_MeshPool.m_MeshDataPool && s_MeshPool.m_MeshDataPoolStats.nPoolSize > nSize)
		{
		try_again:
			s_MeshPool.m_MeshPoolCS.Lock();
			void* ptr = s_MeshPool.m_MeshDataPool->Memalign(nAlign, nSize, "RENDERMESH_POOL");
			if (ptr)
			{
				s_MeshPool.m_MeshDataPoolStats.nPoolInUse += s_MeshPool.m_MeshDataPool->UsableSize(ptr);
				s_MeshPool.m_MeshDataPoolStats.nPoolInUsePeak =
					std::max(
						s_MeshPool.m_MeshDataPoolStats.nPoolInUsePeak,
						s_MeshPool.m_MeshDataPoolStats.nPoolInUse);
				s_MeshPool.m_MeshPoolCS.Unlock();
				return ptr;
			}
			else
			{
				s_MeshPool.m_MeshPoolCS.Unlock();
				// Clean up the stale mesh temporary data - and do it from the main thread.
				if (gRenDev->m_pRT->IsMainThread() && CRenderMesh::ClearStaleMemory(true, gRenDev->GetMainThreadID()))
				{
					goto try_again;
				}
				else if (gRenDev->m_pRT->IsRenderThread() && CRenderMesh::ClearStaleMemory(true, gRenDev->GetRenderThreadID()))
				{
					goto try_again;
				}
			}
			s_MeshPool.m_MeshPoolCS.Lock();
			s_MeshPool.m_MeshDataPoolStats.nFallbacks += nSize;
			s_MeshPool.m_MeshPoolCS.Unlock();
		}
		return CryModuleMemalign(nSize, nAlign);
	}

	static void FreeMeshData(void* ptr)
	{
		if (ptr == NULL)
			return;
		{
			AUTO_LOCK(s_MeshPool.m_MeshPoolCS);
			size_t nSize = 0u;
			if (s_MeshPool.m_MeshDataPool && (nSize=s_MeshPool.m_MeshDataPool->Free(ptr)) > 0)
			{
				s_MeshPool.m_MeshDataPoolStats.nPoolInUse -=
					(nSize < s_MeshPool.m_MeshDataPoolStats.nPoolInUse) ? nSize : s_MeshPool.m_MeshDataPoolStats.nPoolInUse;
				return;
			}
		}
		CryModuleMemalignFree(ptr);
	}

	//////////////////////////////////////////////////////////////////////////
	template<typename Type>
	static Type* AllocateMeshData(size_t nCount = 1)
	{
		void* pStorage = AllocateMeshData(sizeof(Type) * nCount, std::max((size_t)MESH_DATA_DEFAULT_ALIGN, (size_t)__alignof(Type)));
		if (!pStorage)
			return NULL;
		Type* pTypeArray = reinterpret_cast<Type*>(pStorage);
		for (size_t i = 0; i < nCount; ++i)
			new (pTypeArray + i) Type;
		return pTypeArray;
	}

	static bool IsPoolAllocated(void *ptr)
	{
		if (s_MeshPool.m_MeshDataPool && s_MeshPool.m_MeshDataPool->IsInAddressRange(ptr))
			return true;
		return false;
	}

	static bool InitializePool()
	{
		if (gRenDev->CV_r_meshpoolsize > 0)
		{
			if (s_MeshPool.m_MeshDataPool || s_MeshPool.m_MeshDataMemory)
			{
				CryFatalError("render meshpool already initialized");
				return false;
			}
			size_t poolSize = static_cast<size_t>(gRenDev->CV_r_meshpoolsize)*1024U;
			s_MeshPool.m_MeshDataMemory = CryModuleMemalign(poolSize, 128u);
			if (!s_MeshPool.m_MeshDataMemory)
			{
				CryFatalError("could not allocate render meshpool");
				return false;
			}

			// Initialize the actual pool
			s_MeshPool.m_MeshDataPool = gEnv->pSystem->GetIMemoryManager()->CreateGeneralMemoryHeap(
				s_MeshPool.m_MeshDataMemory, poolSize,  "RENDERMESH_POOL");
			s_MeshPool.m_MeshDataPoolStats.nPoolSize = poolSize;
		}
		if (gRenDev->CV_r_meshinstancepoolsize && !s_MeshPool.m_MeshInstancePool)
		{
 			size_t poolSize = static_cast<size_t>(gRenDev->CV_r_meshinstancepoolsize)*1024U; 
			s_MeshPool.m_MeshInstanceMemory = CryModuleMemalign(poolSize, 128u);
			if (!s_MeshPool.m_MeshInstanceMemory) 
			{
				CryFatalError("could not allocate render mesh instance pool");
				return false; 
			}
      
			s_MeshPool.m_MeshInstancePool = gEnv->pSystem->GetIMemoryManager()->CreateGeneralMemoryHeap(
				s_MeshPool.m_MeshInstanceMemory, poolSize, "RENDERMESH_INSTANCE_POOL");
			s_MeshPool.m_MeshDataPoolStats.nInstancePoolInUse = 0;
			s_MeshPool.m_MeshDataPoolStats.nInstancePoolInUsePeak = 0;
			s_MeshPool.m_MeshDataPoolStats.nInstancePoolSize = gRenDev->CV_r_meshinstancepoolsize*1024;
		}
		return true;
	}

	static void ShutdownPool()
	{
		if (s_MeshPool.m_MeshDataPool)
		{
			s_MeshPool.m_MeshDataPool->Release();
			s_MeshPool.m_MeshDataPool = NULL;
		}
		if (s_MeshPool.m_MeshDataMemory)
		{
			CryModuleMemalignFree(s_MeshPool.m_MeshDataMemory);
			s_MeshPool.m_MeshDataMemory=NULL;
		}
		if (s_MeshPool.m_MeshInstancePool)
		{
			s_MeshPool.m_MeshInstancePool->Cleanup();
			s_MeshPool.m_MeshInstancePool->Release();
			s_MeshPool.m_MeshInstancePool=NULL;
		}
    if (s_MeshPool.m_MeshInstanceMemory)
    {
      CryModuleMemalignFree(s_MeshPool.m_MeshInstanceMemory);
      s_MeshPool.m_MeshInstanceMemory = NULL; 
    }
	}

  static void* AllocateMeshInstanceData(size_t size, size_t align)
  {
    if (s_MeshPool.m_MeshInstancePool)
    {
      if (void* ptr = s_MeshPool.m_MeshInstancePool->Memalign(align, size, "rendermesh instance data"))
      {
#       if !defined(_RELEASE)
		    AUTO_LOCK(s_MeshPool.m_MeshPoolCS);
		    s_MeshPool.m_MeshDataPoolStats.nInstancePoolInUsePeak = std::max(
			    s_MeshPool.m_MeshDataPoolStats.nInstancePoolInUsePeak,
			    s_MeshPool.m_MeshDataPoolStats.nInstancePoolInUse += size);
#       endif
        return ptr;
      }
    }
    return CryModuleMemalign(size, align);
  }

  static void FreeMeshInstanceData(void* ptr)
  {
    size_t size = 0u;
    if (s_MeshPool.m_MeshInstancePool && (size=s_MeshPool.m_MeshInstancePool->UsableSize(ptr)))
    {
#     if !defined(_RELEASE)
		  AUTO_LOCK(s_MeshPool.m_MeshPoolCS);
		  s_MeshPool.m_MeshDataPoolStats.nInstancePoolInUse -= size;
#     endif
      s_MeshPool.m_MeshInstancePool->Free(ptr);
      return;
    }
    CryModuleMemalignFree(ptr);
  }
}

#define alignup(alignment, value) 	((((uintptr_t)(value))+((alignment)-1))&(~((uintptr_t)(alignment)-1)))
#define alignup16(value)						alignup(16, value)

#if defined(USE_VBIB_PUSH_DOWN)
  static inline bool VidMemPushDown(void* pDst, const void* pSrc, size_t nSize, void* pDst1=NULL, const void* pSrc1=NULL, size_t nSize1=0, int cachePosStride = -1, void* pFP16Dst=NULL, uint32 nVerts=0)
{
}
	#define ASSERT_LOCK
	static std::vector<CRenderMesh*> g_MeshCleanupVec;//for cleanup of meshes itself
#else
	#define ASSERT_LOCK assert((m_nVerts == 0) || pData)
#endif

#if !defined(_RELEASE)
	ILINE void CheckVideoBufferAccessViolation(CRenderMesh& mesh)
	{
		//LogWarning("CRenderMesh::LockVB: accessing video buffer for cgf=%s",mesh.GetSourceName());
	}
	#define MESSAGE_VIDEO_BUFFER_ACC_ATTEMPT CheckVideoBufferAccessViolation(*this)
#else
	#define MESSAGE_VIDEO_BUFFER_ACC_ATTEMPT
#endif

CryCriticalSection CRenderMesh::m_sLinkLock;

util::list<CRenderMesh> CRenderMesh::s_MeshList;
util::list<CRenderMesh> CRenderMesh::s_MeshGarbageList[MAX_RELEASED_MESH_FRAMES];
util::list<CRenderMesh> CRenderMesh::s_MeshDirtyList[2];
util::list<CRenderMesh> CRenderMesh::s_MeshModifiedList[2];

int CRenderMesh::Release()
{
	long refCnt = CryInterlockedDecrement(&m_nRefCounter);
# if !defined(_RELEASE)
	if (refCnt < 0)
	{
		CRY_ASSERT_MESSAGE(refCnt>=0, "CRenderMesh::Release() called so many times on rendermesh that refcount became negative");
	}
# endif
	if (refCnt == 0)
	{
		AUTO_LOCK(m_sLinkLock);
#   if !defined(_RELEASE)
		if (m_nFlags & FRM_RELEASED)
		{
			CRY_ASSERT_MESSAGE(refCnt > 0, "CRenderMesh::Release() mesh already in the garbage list (double delete pending)");
		}
#   endif
		m_nFlags |= FRM_RELEASED;
		int nFrame = gRenDev->m_pRT ? GetCurrentFrameID() : gEnv->nMainFrameID;
		util::list<CRenderMesh>* garbage = &CRenderMesh::s_MeshGarbageList[nFrame & (MAX_RELEASED_MESH_FRAMES - 1)];
		m_Chain.relink_tail(garbage);
	}

	return refCnt;
}

CRenderMesh::CRenderMesh()
{
#if defined(USE_VBIB_PUSH_DOWN)
	m_VBIBFramePushID = 0;
#endif
  memset(m_VBStream, 0x0, sizeof(m_VBStream));

  SMeshStream *streams = (SMeshStream*)AllocateMeshInstanceData(sizeof(SMeshStream)*VSF_NUM, 64u);
  for (signed i=0; i<VSF_NUM; ++i) 
    new (m_VBStream[i] = &streams[i]) SMeshStream();

  m_keepSysMesh = false; 
	m_nLastRenderFrameID = 0;
	m_nLastSubsetGCRenderFrameID = 0;
	m_nThreadAccessCounter = 0;
	for (size_t i=0; i<2; ++i)
		m_asyncUpdateState[i] = m_asyncUpdateStateCounter[i] = 0;

# if !defined(_RELEASE) && defined(RM_CATCH_EXCESSIVE_LOCKS)
	m_lockTime = 0.f;
# endif

	m_sType = "";

	m_pExtraBoneMapping = nullptr;
	m_nMorphs = 0;

#ifdef MESH_TESSELLATION_RENDERER
	m_adjBuffer.Create(0, sizeof(uint16), DXGI_FORMAT_R16_UNORM, CDeviceObjectFactory::BIND_SHADER_RESOURCE, nullptr);
#endif
	m_extraBonesBuffer.Create(0, sizeof(SVF_W4B_I4S), DXGI_FORMAT_UNKNOWN, CDeviceObjectFactory::BIND_SHADER_RESOURCE | CDeviceObjectFactory::USAGE_STRUCTURED, nullptr);
}

CRenderMesh::CRenderMesh (const char *szType, const char *szSourceName, bool bLock)
{
  memset(m_VBStream, 0x0, sizeof(m_VBStream));

  SMeshStream *streams = (SMeshStream*)AllocateMeshInstanceData(sizeof(SMeshStream)*VSF_NUM, 64u);
  for (signed i=0; i<VSF_NUM; ++i) 
    new (m_VBStream[i] = &streams[i]) SMeshStream();

  m_keepSysMesh = false; 
  m_nRefCounter = 0;
	m_nLastRenderFrameID = 0;
	m_nLastSubsetGCRenderFrameID = 0;

  m_sType = szType;
  m_sSource = szSourceName;

  m_vBoxMin = m_vBoxMax = Vec3(0,0,0); //used for hw occlusion test
  m_nVerts = 0;
  m_nInds = 0;
  m_eVF = EDefaultInputLayouts::P3F_C4B_T2F;
  m_pVertexContainer = NULL;

  {
    AUTO_LOCK(m_sLinkLock);
    m_Chain.relink_tail(&s_MeshList);
  }
  m_nPrimetiveType = eptTriangleList;

//  m_nFrameRender = 0;
  //m_nFrameUpdate = 0;
  m_nClientTextureBindID = 0;
#ifdef RENDER_MESH_TRIANGLE_HASH_MAP_SUPPORT
  m_pTrisMap = NULL;
#endif

  m_pCachePos = NULL;        
  m_nFrameRequestCachePos = 0;
  m_nFlagsCachePos = 0;

	m_pCacheUVs = NULL;
	m_nFrameRequestCacheUVs = 0;
	m_nFlagsCacheUVs = 0;

	_SetRenderMeshType(eRMT_Static);

  m_nFlags = 0;
  m_fGeometricMeanFaceArea = 0.f;
  m_nLod = 0;

#if defined(USE_VBIB_PUSH_DOWN)
	m_VBIBFramePushID = 0;
#endif

 m_nThreadAccessCounter = 0;
 for (size_t i=0; i<2; ++i)
	m_asyncUpdateState[i] = m_asyncUpdateStateCounter[i] = 0;

	IF (bLock, 0)//when called from another thread like the Streaming AsyncCallbackCGF, we need to lock it
	{
		LockForThreadAccess();
	}

	m_pExtraBoneMapping = nullptr;
	m_nMorphs = 0;

#ifdef MESH_TESSELLATION_RENDERER
	m_adjBuffer.Create(0, sizeof(uint16), DXGI_FORMAT_R16_UNORM, CDeviceObjectFactory::BIND_SHADER_RESOURCE, nullptr);
#endif
	m_extraBonesBuffer.Create(0, sizeof(SVF_W4B_I4S), DXGI_FORMAT_UNKNOWN, CDeviceObjectFactory::BIND_SHADER_RESOURCE | CDeviceObjectFactory::USAGE_STRUCTURED, nullptr);
}

void CRenderMesh::Cleanup()
{
	SREC_AUTO_LOCK(m_sResLock);

	FreeDeviceBuffers(false);
	FreeSystemBuffers();

	m_meshSubSetIndices.clear();

	if (m_pVertexContainer)
	{
		m_pVertexContainer->m_lstVertexContainerUsers.Delete(this);
		m_pVertexContainer = NULL;
	}

	for(int i=0; i<m_lstVertexContainerUsers.Count(); i++)
	{
		if (m_lstVertexContainerUsers[i]->GetVertexContainer() == this)
			m_lstVertexContainerUsers[i]->m_pVertexContainer = NULL;
	}
	m_lstVertexContainerUsers.Clear();

	ReleaseRenderChunks(&m_ChunksSkinned);
	ReleaseRenderChunks(&m_ChunksSubObjects);
	ReleaseRenderChunks(&m_Chunks);

	m_ChunksSubObjects.clear();
	m_Chunks.clear();

#ifdef RENDER_MESH_TRIANGLE_HASH_MAP_SUPPORT
	SAFE_DELETE(m_pTrisMap);
#endif

	for (size_t i=0; i<2; ++i)
		m_asyncUpdateState[i] = m_asyncUpdateStateCounter[i] = 0;

	for (int i = 0; i < VSF_NUM; ++i)
	{
		if (m_VBStream[i])
			m_VBStream[i]->~SMeshStream();
	}

	FreeMeshInstanceData(m_VBStream[0]);
	memset(m_VBStream, 0, sizeof(m_VBStream));

	for (size_t i = 0, end = m_CreatedBoneIndices.size(); i < end; ++i)
	{
		FreeMeshDataUnpooled(m_CreatedBoneIndices[i].pStream);
//			if (m_CreatedBoneIndices[i].pExtraStream)
//				FreeMeshDataUnpooled(m_CreatedBoneIndices[i].pExtraStream);
	}
	m_CreatedBoneIndices.clear();
	m_DeletedBoneIndices.clear();

	for (size_t i=0,end=m_RemappedBoneIndices.size(); i<end; ++i)
	{
		if (m_RemappedBoneIndices[i].refcount && m_RemappedBoneIndices[i].guid != ~0u) 
			CryLogAlways("remapped bone indices with refcount '%u' still around for '%s 0x%p\n", m_RemappedBoneIndices[i].refcount, m_sSource.c_str(), this);
		if (m_RemappedBoneIndices[i].buffer != ~0u) 
			gRenDev->m_DevBufMan.Destroy(m_RemappedBoneIndices[i].buffer);
	}
	m_RemappedBoneIndices.clear();

	FreeMeshDataUnpooled(m_pExtraBoneMapping);
}

//////////////////////////////////////////////////////////////////////////
CRenderMesh::~CRenderMesh()
{
	// make sure to stop and delete all mesh subset indice tasks
	ASSERT_IS_RENDER_THREAD(gRenDev->m_pRT);

	int nThreadID = gRenDev->GetRenderThreadID();
	for (int i = 0; i < RT_COMMAND_BUF_COUNT; SyncAsyncUpdate(i++))
		;

	// make sure no subset rendermesh job is still running which uses this mesh
	for(int j=0;j<RT_COMMAND_BUF_COUNT;++j)
	{
		size_t nNumSubSetRenderMeshJobs = m_meshSubSetRenderMeshJobs[j].size();
		for( size_t i = 0 ; i < nNumSubSetRenderMeshJobs ; ++i )
		{
			SMeshSubSetIndicesJobEntry &rSubSetJob = m_meshSubSetRenderMeshJobs[j][i];
			if(rSubSetJob.m_pSrcRM == this )
			{
				gEnv->pJobManager->WaitForJob(rSubSetJob.jobState);
				rSubSetJob.m_pSrcRM = NULL;
			}
		}
	}

	// remove ourself from deferred subset mesh garbage collection
	for(size_t i = 0 ; i < m_deferredSubsetGarbageCollection[nThreadID].size() ; ++i )
	{
		if(m_deferredSubsetGarbageCollection[nThreadID][i] == this)
		{
			m_deferredSubsetGarbageCollection[nThreadID][i] = NULL;
		}
	}

	assert(m_nThreadAccessCounter == 0);

	{
		AUTO_LOCK(m_sLinkLock);
		for (int i=0; i<2; ++i)
			m_Dirty[i].erase(), m_Modified[i].erase();
		m_Chain.erase();
	}

	Cleanup();
}

void CRenderMesh::ReleaseRenderChunks(TRenderChunkArray *pChunks)
{
	if (pChunks)
	{
		for (size_t i = 0, c = pChunks->size(); i != c; ++i)
		{
			CRenderChunk &rChunk = pChunks->at(i);

			if (rChunk.pRE)
			{
				CREMeshImpl* pRE = static_cast<CREMeshImpl*>(rChunk.pRE);
				pRE->Release(false);
				pRE->m_pRenderMesh = NULL;
				rChunk.pRE = 0;
			}
		}
	}
}

SMeshStream* CRenderMesh::GetVertexStream(int nStream, uint32 nFlags)
{
  SMeshStream*& pMS = m_VBStream[nStream];
  IF (!pMS && (nFlags & FSL_WRITE), 0)
  {
    pMS = new (AllocateMeshInstanceData(sizeof(SMeshStream), alignof(SMeshStream))) SMeshStream;
  }
  return pMS;
}

void *CRenderMesh::LockVB(int nStream, uint32 nFlags, int nOffset, int nVerts, int *nStride, bool prefetchIB, bool inplaceCachePos)
{
	FUNCTION_PROFILER_RENDERER();

	MEMORY_SCOPE_CHECK_HEAP();
	MEMSTAT_CONTEXT(EMemStatContextTypes::MSC_RenderMeshType, 0, this->GetTypeName());
	MEMSTAT_CONTEXT(EMemStatContextTypes::MSC_RenderMesh, 0, this->GetSourceName());
# if !defined(_RELEASE)
	CRY_ASSERT_MESSAGE(m_nThreadAccessCounter, "rendermesh must be locked via LockForThreadAccess() before LockIB/VB is called");
#endif

	// NOTE: When called from a job thread we will use main thread ID
	const int threadId = gRenDev->m_pRT->IsRenderThread() ? gRenDev->GetRenderThreadID() : gRenDev->GetMainThreadID();

	if (!CanUpdate()) // if allocation failure suffered, don't lock anything anymore
		return NULL;

	SREC_AUTO_LOCK(m_sResLock);//need lock as resource must not be updated concurrently
	SMeshStream* MS = GetVertexStream(nStream, nFlags);

	int nFrame = GetCurrentFrameID();

#if defined(USE_VBIB_PUSH_DOWN)
	m_VBIBFramePushID = nFrame;
	if (nFlags == FSL_SYSTEM_CREATE || nFlags == FSL_SYSTEM_UPDATE)
		MS->m_nLockFlags &= ~FSL_VBIBPUSHDOWN;
#endif

	assert(nVerts <= (int)m_nVerts);
	if (nVerts > (int)m_nVerts)
		nVerts = m_nVerts;
	if (nStride)
		*nStride = GetStreamStride(nStream);

	m_nFlags |= FRM_READYTOUPLOAD;

	byte *pD;

	if (nFlags == FSL_SYSTEM_CREATE)
	{
lSysCreate:
		RelinkTail(m_Modified[threadId], s_MeshModifiedList[threadId]);
		if (!MS->m_pUpdateData)
		{
			uint32 nSize = GetStreamSize(nStream);
			pD = reinterpret_cast<byte*>(AllocateMeshData(nSize));
			if (!pD) return NULL;
			MS->m_pUpdateData = pD;
		}
		else
			pD = (byte *)MS->m_pUpdateData;
		//MS->m_nFrameRequest = nFrame;
		MS->m_nLockFlags = (FSL_SYSTEM_CREATE | (MS->m_nLockFlags & FSL_LOCKED));
		return &pD[nOffset];
	}
	else if (nFlags == FSL_SYSTEM_UPDATE)
	{
lSysUpdate:
		RelinkTail(m_Modified[threadId], s_MeshModifiedList[threadId]);
		if (!MS->m_pUpdateData)
		{
			MESSAGE_VIDEO_BUFFER_ACC_ATTEMPT;
			CopyStreamToSystemForUpdate(*MS, GetStreamSize(nStream));
		}
		assert(nStream || MS->m_pUpdateData);
		if (!MS->m_pUpdateData)
			return NULL;
		//MS->m_nFrameRequest = nFrame;
		pD = (byte *)MS->m_pUpdateData;
		MS->m_nLockFlags = (nFlags | (MS->m_nLockFlags & FSL_LOCKED));
		return &pD[nOffset];
	}
	else if (nFlags == FSL_READ)
	{
		if (!MS)
			return NULL;
		RelinkTail(m_Dirty[threadId], s_MeshDirtyList[threadId]);
		if (MS->m_pUpdateData)
		{
			pD = (byte *)MS->m_pUpdateData;
			return &pD[nOffset];
		}
		nFlags = FSL_READ | FSL_VIDEO;
	}

	if (nFlags == (FSL_READ | FSL_VIDEO))
	{
		if (!MS)
			return NULL;
		RelinkTail(m_Dirty[threadId], s_MeshDirtyList[threadId]);
#if RENDERMESH_BUFFER_ENABLE_DIRECT_ACCESS == 0
		if (gRenDev->m_pRT && gRenDev->m_pRT->IsMultithreaded())
		{
			// Always use system copy in MT mode
			goto lSysUpdate;
		}
		else
#   endif 
		{
			buffer_handle_t nVB = MS->m_nID;
			if (nVB == ~0u)
				return NULL;
			// Try to lock device buffer in single-threaded mode
			if (!MS->m_pLockedData)
			{
				MESSAGE_VIDEO_BUFFER_ACC_ATTEMPT;
				if (MS->m_pLockedData = gRenDev->m_DevBufMan.BeginRead(nVB))
					MS->m_nLockFlags |= FSL_LOCKED;
			}
			if (MS->m_pLockedData)
			{
				++MS->m_nLockCount;
				pD = (byte *)MS->m_pLockedData;
				return &pD[nOffset];
			}
		}
	}
	if (nFlags == (FSL_VIDEO_CREATE))
	{
		// Only consoles support direct uploading to vram, but as the data is not
		// double buffered in the non-dynamic case, only creation is supported
		// DX11 has to use the deferred context to call map, which is not threadsafe
		// DX9 can experience huge stalls if resources are used while rendering is performed
		buffer_handle_t nVB = ~0u;
#if RENDERMESH_BUFFER_ENABLE_DIRECT_ACCESS
		nVB = MS->m_nID;
		if ((nVB != ~0u && (MS->m_nFrameCreate != nFrame || MS->m_nElements != m_nVerts)) || !CRenderer::CV_r_buffer_enable_lockless_updates)
#   endif
			goto lSysCreate;
		RelinkTail(m_Modified[threadId], s_MeshModifiedList[threadId]);
		if (nVB == ~0u && !CreateVidVertices(m_nVerts, m_eVF, nStream))
		{
			RT_AllocationFailure("Create VB-Stream", GetStreamSize(nStream, m_nVerts));
			return NULL;
		}
		nVB = MS->m_nID;
		if (!MS->m_pLockedData)
		{
			MESSAGE_VIDEO_BUFFER_ACC_ATTEMPT;
			if ((MS->m_pLockedData = gRenDev->m_DevBufMan.BeginWrite(nVB)) == NULL)
				return NULL;
			MS->m_nLockFlags |= FSL_DIRECT | FSL_LOCKED;
		}
		++MS->m_nLockCount;
		pD = (byte *)MS->m_pLockedData;
		return &pD[nOffset];
	}
	if (nFlags == (FSL_VIDEO_UPDATE))
	{
		goto lSysUpdate;
	}

	return NULL;
}

vtx_idx *CRenderMesh::LockIB(uint32 nFlags, int nOffset, int nInds)
{
	FUNCTION_PROFILER_RENDERER();

	MEMORY_SCOPE_CHECK_HEAP();
	byte *pD;
# if !defined(_RELEASE)
	CRY_ASSERT_MESSAGE(m_nThreadAccessCounter, "rendermesh must be locked via LockForThreadAccess() before LockIB/VB is called");
#endif
	if (!CanUpdate()) // if allocation failure suffered, don't lock anything anymore
		return NULL;

	// NOTE: When called from a job thread we will use main thread ID
	const int threadId = gRenDev->m_pRT->IsRenderThread() ? gRenDev->GetRenderThreadID() : gRenDev->GetMainThreadID();

	int nFrame = GetCurrentFrameID();
	SREC_AUTO_LOCK(m_sResLock);//need lock as resource must not be updated concurrently

#if defined(USE_VBIB_PUSH_DOWN)
	m_VBIBFramePushID = nFrame;
	if (nFlags == FSL_SYSTEM_CREATE || nFlags == FSL_SYSTEM_UPDATE)
		m_IBStream.m_nLockFlags &= ~FSL_VBIBPUSHDOWN;
#endif
	m_nFlags |= FRM_READYTOUPLOAD;

	assert(nInds <= (int)m_nInds);
	if (nFlags == FSL_SYSTEM_CREATE)
	{
lSysCreate:
		RelinkTail(m_Modified[threadId], s_MeshModifiedList[threadId]);
		if (!m_IBStream.m_pUpdateData)
		{
			uint32 nSize = m_nInds * sizeof(vtx_idx);
			pD = reinterpret_cast<byte*>(AllocateMeshData(nSize));
			if (!pD) return NULL;
			m_IBStream.m_pUpdateData = (vtx_idx *)pD;
		}
		else
			pD = (byte *)m_IBStream.m_pUpdateData;
		//m_IBStream.m_nFrameRequest = nFrame;
		m_IBStream.m_nLockFlags = (nFlags | (m_IBStream.m_nLockFlags & FSL_LOCKED));
		return (vtx_idx *)&pD[nOffset];
	}
	else if (nFlags == FSL_SYSTEM_UPDATE)
	{
lSysUpdate:
		RelinkTail(m_Modified[threadId], s_MeshModifiedList[threadId]);
		if (!m_IBStream.m_pUpdateData)
		{
			MESSAGE_VIDEO_BUFFER_ACC_ATTEMPT;
			CopyStreamToSystemForUpdate(m_IBStream, sizeof(vtx_idx)*m_nInds);
		}
		assert(m_IBStream.m_pUpdateData);
		if (!m_IBStream.m_pUpdateData)
			return NULL;
		//m_IBStream.m_nFrameRequest = nFrame;
		pD = (byte *)m_IBStream.m_pUpdateData;
		m_IBStream.m_nLockFlags = (nFlags | (m_IBStream.m_nLockFlags & FSL_LOCKED));
		return (vtx_idx *)&pD[nOffset];
	}
	else if (nFlags == FSL_READ)
	{
		RelinkTail(m_Dirty[threadId], s_MeshDirtyList[threadId]);
		if (m_IBStream.m_pUpdateData)
		{
			pD = (byte *)m_IBStream.m_pUpdateData;
			return (vtx_idx *)&pD[nOffset];
		}
		nFlags = FSL_READ | FSL_VIDEO;
	}

	if (nFlags == (FSL_READ | FSL_VIDEO))
	{
		RelinkTail(m_Dirty[threadId], s_MeshDirtyList[threadId]);
		buffer_handle_t nIB = m_IBStream.m_nID;
		if (nIB == ~0u)
			return NULL;
		if (gRenDev->m_pRT && gRenDev->m_pRT->IsMultithreaded())
		{
			// Always use system copy in MT mode
			goto lSysUpdate;
		}
		else
		{
			// TODO: make smart caching mesh algorithm for consoles
			if (!m_IBStream.m_pLockedData)
			{
				MESSAGE_VIDEO_BUFFER_ACC_ATTEMPT;
				if (m_IBStream.m_pLockedData = gRenDev->m_DevBufMan.BeginRead(nIB))
					m_IBStream.m_nLockFlags |= FSL_LOCKED;
			}
			if (m_IBStream.m_pLockedData)
			{
				pD = (byte *)m_IBStream.m_pLockedData;
				++m_IBStream.m_nLockCount;
				return (vtx_idx *)&pD[nOffset];
			}
		}
	}
	if (nFlags == (FSL_VIDEO_CREATE))
	{
		// Only consoles support direct uploading to vram, but as the data is not
		// double buffered, only creation is supported
		// DX11 has to use the deferred context to call map, which is not threadsafe
		// DX9 can experience huge stalls if resources are used while rendering is performed
		buffer_handle_t nIB = -1;
#if RENDERMESH_BUFFER_ENABLE_DIRECT_ACCESS
		nIB = m_IBStream.m_nID;
		if ((nIB != ~0u && (m_IBStream.m_nFrameCreate || m_IBStream.m_nElements != m_nInds)) || !CRenderer::CV_r_buffer_enable_lockless_updates)
#   endif
			goto lSysCreate;
		RelinkTail(m_Modified[threadId], s_MeshModifiedList[threadId]);
		if (m_IBStream.m_nID == ~0u)
		{
			nIB = (m_IBStream.m_nID = gRenDev->m_DevBufMan.Create(BBT_INDEX_BUFFER, (BUFFER_USAGE)m_eType, m_nInds * sizeof(vtx_idx)));
			m_IBStream.m_nFrameCreate = nFrame;
		}
		if (nIB == ~0u)
		{
			RT_AllocationFailure("Create IB-Stream", m_nInds * sizeof(vtx_idx));
			return NULL;
		}
		m_IBStream.m_nElements = m_nInds;
		if (!m_IBStream.m_pLockedData)
		{
			MESSAGE_VIDEO_BUFFER_ACC_ATTEMPT;
			if ((m_IBStream.m_pLockedData = gRenDev->m_DevBufMan.BeginWrite(nIB)) == NULL)
				return NULL;
			m_IBStream.m_nLockFlags |= FSL_DIRECT | FSL_LOCKED;
		}
		++m_IBStream.m_nLockCount;
		pD = (byte*)m_IBStream.m_pLockedData;
		return (vtx_idx*)&pD[nOffset];
	}
	if (nFlags == (FSL_VIDEO_UPDATE))
	{
		goto lSysUpdate;
	}

	assert(0);

	return NULL;
}

ILINE void CRenderMesh::UnlockVB(int nStream)
{
  MEMORY_SCOPE_CHECK_HEAP();
	SREC_AUTO_LOCK(m_sResLock);
  SMeshStream* pMS = GetVertexStream(nStream, 0); 
  if (pMS && pMS->m_nLockFlags & FSL_LOCKED)
  {
    assert(pMS->m_nLockCount);
		if ((--pMS->m_nLockCount) == 0)
    {
      gRenDev->m_DevBufMan.EndReadWrite(pMS->m_nID);
      pMS->m_nLockFlags &= ~FSL_LOCKED;
		  pMS->m_pLockedData = NULL; 
    }
  }
  if (pMS && (pMS->m_nLockFlags & FSL_WRITE) && (pMS->m_nLockFlags & (FSL_SYSTEM_CREATE | FSL_SYSTEM_UPDATE)))
  {
    pMS->m_nLockFlags &= ~(FSL_SYSTEM_CREATE | FSL_SYSTEM_UPDATE);
    pMS->m_nFrameRequest = GetCurrentFrameID();
  }
}

ILINE void CRenderMesh::UnlockIB()
{
  MEMORY_SCOPE_CHECK_HEAP();
  SREC_AUTO_LOCK(m_sResLock);
	if (m_IBStream.m_nLockFlags & FSL_LOCKED)
  {
    assert(m_IBStream.m_nLockCount);
    if ((--m_IBStream.m_nLockCount) == 0)
    {
      gRenDev->m_DevBufMan.EndReadWrite(m_IBStream.m_nID);
      m_IBStream.m_nLockFlags &= ~FSL_LOCKED;
      m_IBStream.m_pLockedData = NULL; 
    }
  }
  if ((m_IBStream.m_nLockFlags & FSL_WRITE) && (m_IBStream.m_nLockFlags & (FSL_SYSTEM_CREATE | FSL_SYSTEM_UPDATE)))
  {
    m_IBStream.m_nLockFlags &= ~(FSL_SYSTEM_CREATE | FSL_SYSTEM_UPDATE);
    m_IBStream.m_nFrameRequest = GetCurrentFrameID();
  }
}

void CRenderMesh::UnlockStream(int nStream)
{
  MEMORY_SCOPE_CHECK_HEAP();
  UnlockVB(nStream);
	SREC_AUTO_LOCK(m_sResLock);
	
	const auto vertexFormatDescriptor = CDeviceObjectFactory::GetInputLayoutDescriptor(GetVertexFormat());
	if (!vertexFormatDescriptor)
		return;

	if (nStream == VSF_GENERAL)
	{
 		if (m_nFlagsCachePos && m_pCachePos)
		{
			uint32 i;
			int nStride;
			byte *pDst = (byte *)LockVB(nStream, FSL_SYSTEM_UPDATE, 0, m_nVerts, &nStride);

			int8 ofs = vertexFormatDescriptor->m_Offsets[SInputLayout::eOffset_Position];
			assert(ofs >= 0);
			pDst += ofs;

			assert(pDst);
			if (pDst)
			{
				for (i=0; i<m_nVerts; i++)
				{
					Vec3f16 *pVDst = (Vec3f16 *)pDst;
					*pVDst = m_pCachePos[i];
					pDst += nStride;
				}
			}
			m_nFlagsCachePos = 0;
		}

 		if (m_nFlagsCacheUVs && m_pCacheUVs)
		{
			uint32 i;
			int nStride;
			byte *pDst = (byte *)LockVB(nStream, FSL_SYSTEM_UPDATE, 0, m_nVerts, &nStride);

			int8 ofs = vertexFormatDescriptor->m_Offsets[SInputLayout::eOffset_TexCoord];
			assert(ofs >= 0);
			pDst += ofs;

			assert(pDst);
			if (pDst)
			{
				for (i=0; i<m_nVerts; i++)
				{
					Vec2f16 *pVDst = (Vec2f16 *)pDst;
					*pVDst = m_pCacheUVs[i];
					pDst += nStride;
				}
			}
			m_nFlagsCacheUVs = 0;
		}
	}

	SMeshStream* pMS = GetVertexStream(nStream, 0);
	IF (pMS, 1)
		pMS->m_nLockFlags &= ~(FSL_WRITE | FSL_READ | FSL_SYSTEM | FSL_VIDEO);
}
void CRenderMesh::UnlockIndexStream()
{
  MEMORY_SCOPE_CHECK_HEAP();
  UnlockIB();
  m_IBStream.m_nLockFlags &= ~(FSL_WRITE | FSL_READ | FSL_SYSTEM | FSL_VIDEO);
}

bool CRenderMesh::CopyStreamToSystemForUpdate(SMeshStream& MS, size_t nSize)
{
  MEMORY_SCOPE_CHECK_HEAP();
  FUNCTION_PROFILER_RENDERER();  
  SREC_AUTO_LOCK(m_sResLock);
  if (!MS.m_pUpdateData)
	{
		buffer_handle_t nVB = MS.m_nID;
		if (nVB == ~0u)
			return false;
		void *pSrc = MS.m_pLockedData;
		if (!pSrc)
		{
			pSrc = gRenDev->m_DevBufMan.BeginRead(nVB);
			MS.m_nLockFlags |= FSL_LOCKED;
		}
		assert(pSrc);
		if (!pSrc)
			return false;
    ++MS.m_nLockCount;
    byte *pD = reinterpret_cast<byte*>(AllocateMeshData(nSize, MESH_DATA_DEFAULT_ALIGN, false));
    if (pD)
    {
      cryMemcpy(pD, pSrc, nSize);
      if (MS.m_nLockFlags & FSL_LOCKED)
      {
        if ((--MS.m_nLockCount) == 0)
        {
          MS.m_nLockFlags &= ~FSL_LOCKED;
          MS.m_pLockedData = NULL;
          gRenDev->m_DevBufMan.EndReadWrite(nVB);
        }
      }
      MS.m_pUpdateData = pD;
      m_nFlags |= FRM_READYTOUPLOAD;
      return true;
    }
	}
  return false;
}

size_t CRenderMesh::SetMesh_Int(CMesh &mesh, int nSecColorsSetOffset, uint32 flags, const Vec3 *pPosOffset)	
{
  LOADING_TIME_PROFILE_SECTION;
  MEMORY_SCOPE_CHECK_HEAP();
	MEMSTAT_CONTEXT(EMemStatContextTypes::MSC_RenderMeshType, 0, this->GetTypeName());
	MEMSTAT_CONTEXT(EMemStatContextTypes::MSC_RenderMesh, 0, this->GetSourceName());

  char *pVBuff = NULL;	
  SPipTangents *pTBuff = NULL;
  SPipQTangents *pQTBuff = NULL;
  SVF_P3F *pVelocities = NULL;
  SPipNormal *pNBuff = NULL;
  uint32 nVerts = mesh.GetVertexCount();
  uint32 nInds = mesh.GetIndexCount();
  vtx_idx *pInds = NULL;

	//AUTO_LOCK(m_sResLock);//need a resource lock as mesh could be reseted due to allocation failure
  SScopedMeshDataLock _lock(this);

  ReleaseRenderChunks(&m_ChunksSkinned);

  m_vBoxMin = mesh.m_bbox.min;
  m_vBoxMax = mesh.m_bbox.max;

  m_fGeometricMeanFaceArea = mesh.m_geometricMeanFaceArea;

  //////////////////////////////////////////////////////////////////////////
  // Initialize Render Chunks.
  //////////////////////////////////////////////////////////////////////////
  uint32 numSubsets = mesh.GetSubSetCount();

	uint32 numChunks = 0;
  for (uint32 i=0; i<numSubsets; i++)
	{
		if (mesh.m_subsets[i].nNumIndices == 0)
			continue;

		if(mesh.m_subsets[i].nMatFlags & MTL_FLAG_NODRAW)
			continue;

		++ numChunks;
	}
	m_Chunks.reserve(numChunks);

  for (uint32 i=0; i<numSubsets; i++)
	{
		CRenderChunk ChunkInfo;

		if (mesh.m_subsets[i].nNumIndices == 0)
			continue;

		if(mesh.m_subsets[i].nMatFlags & MTL_FLAG_NODRAW)
			continue;

		//add empty chunk, because PodArray is not working with STL-vectors
		m_Chunks.push_back(ChunkInfo);

		uint32 num = m_Chunks.size();
		CRenderChunk* pChunk = &m_Chunks[num-1];

		pChunk->nFirstIndexId = mesh.m_subsets[i].nFirstIndexId;
		pChunk->nNumIndices   = mesh.m_subsets[i].nNumIndices;
		pChunk->nFirstVertId  = mesh.m_subsets[i].nFirstVertId;
		pChunk->nNumVerts     = mesh.m_subsets[i].nNumVerts;
		pChunk->m_nMatID      = mesh.m_subsets[i].nMatID;
		pChunk->m_nMatFlags   = mesh.m_subsets[i].nMatFlags;
		if (mesh.m_subsets[i].nPhysicalizeType==PHYS_GEOM_TYPE_NONE)
			pChunk->m_nMatFlags |= MTL_FLAG_NOPHYSICALIZE;

		float texelAreaDensity = 1.0f;
	
		if(!(flags & FSM_IGNORE_TEXELDENSITY))
		{
			float posArea;
			float texArea;
			const char* errorText = "";

			if (mesh.m_subsets[i].fTexelDensity > 0.00001f)
			{
				texelAreaDensity = mesh.m_subsets[i].fTexelDensity;
			}
			else
			{
				const bool ok = mesh.ComputeSubsetTexMappingAreas(i, posArea, texArea, errorText);
				if (ok)
				{
					texelAreaDensity = texArea / posArea;
				}
				else
				{
					// Commented out to prevent spam (contact Moritz Finck or Marco Corbetta for details)
					// gEnv->pLog->LogError("Failed to compute texture mapping density for mesh '%s': ?%s", GetSourceName(), errorText);
				}
			}
		}

		pChunk->m_texelAreaDensity = texelAreaDensity;

#define VALIDATE_CHUCKS
#if defined(_DEBUG) && defined(VALIDATE_CHUCKS)
		size_t indStart( pChunk->nFirstIndexId );
		size_t indEnd( pChunk->nFirstIndexId + pChunk->nNumIndices );
		for( size_t j( indStart ); j < indEnd; ++j )
		{
			size_t vtxStart( pChunk->nFirstVertId );
			size_t vtxEnd( pChunk->nFirstVertId + pChunk->nNumVerts );
			size_t curIndex0( mesh.m_pIndices[ j ] ); // absolute indexing
			size_t curIndex1( mesh.m_pIndices[ j ] + vtxStart ); // relative indexing using base vertex index
			assert( ( curIndex0 >= vtxStart && curIndex0 < vtxEnd ) || ( curIndex1 >= vtxStart && curIndex1 < vtxEnd ) ) ;
		}
#endif
	}

  //////////////////////////////////////////////////////////////////////////
  // Create RenderElements.
  //////////////////////////////////////////////////////////////////////////
  int nCurChunk = 0;
  for (int i=0; i<mesh.GetSubSetCount(); i++)
  {
    SMeshSubset &subset = mesh.m_subsets[i];
    if (subset.nNumIndices == 0)
      continue;

    if(subset.nMatFlags & MTL_FLAG_NODRAW)
      continue;

    CRenderChunk *pRenderChunk = &m_Chunks[nCurChunk++];
    CREMeshImpl *pRenderElement = (CREMeshImpl*) gRenDev->EF_CreateRE(eDATA_Mesh);

    // Cross link render chunk with render element.
    pRenderChunk->pRE = pRenderElement;
    AssignChunk(pRenderChunk, pRenderElement);

		if (mesh.m_pBoneMapping)
			pRenderElement->mfUpdateFlags(FCEF_SKINNED);
  }
  if (mesh.m_pBoneMapping)
    m_nFlags |= FRM_SKINNED;

  //////////////////////////////////////////////////////////////////////////
  // Create system vertex buffer in system memory.
  //////////////////////////////////////////////////////////////////////////
#if ENABLE_NORMALSTREAM_SUPPORT
  if (flags & FSM_ENABLE_NORMALSTREAM)
		m_nFlags |= FRM_ENABLE_NORMALSTREAM;
#endif

	m_nVerts = nVerts;
	m_nInds = 0;
	if (mesh.m_pPositions)
	{
		m_eVF = EDefaultInputLayouts::P3F_C4B_T2F;
	}
	else
	{
		m_eVF = EDefaultInputLayouts::P3S_C4B_T2S;
	}
	pVBuff = (char *)LockVB(VSF_GENERAL, FSL_VIDEO_CREATE);
	// stop initializing if allocation failed
	if (pVBuff == NULL) 
	{
		m_nVerts = 0; 
		goto error; 
	}

# if ENABLE_NORMALSTREAM_SUPPORT
	if (m_nFlags & FRM_ENABLE_NORMALSTREAM)
		pNBuff = (SPipNormal *)LockVB(VSF_NORMALS, FSL_VIDEO_CREATE);
# endif

  if (!(flags & FSM_NO_TANGENTS))
	{
		if (mesh.m_pQTangents)
			pQTBuff = (SPipQTangents *)LockVB(VSF_QTANGENTS, FSL_VIDEO_CREATE);
		else
      pTBuff = (SPipTangents *)LockVB(VSF_TANGENTS, FSL_VIDEO_CREATE);

		// stop initializing if allocation failed
		if (pTBuff == NULL && pQTBuff == NULL)
      goto error;
	}

	//////////////////////////////////////////////////////////////////////////
	// Copy indices.
	//////////////////////////////////////////////////////////////////////////
	m_nInds = nInds;
	pInds = LockIB(FSL_VIDEO_CREATE);

	// stop initializing if allocation failed
	if( m_nInds && pInds == NULL )
	{
		m_nInds = 0;
		goto error;
	}

	if (flags & FSM_VERTEX_VELOCITY)
	{
		pVelocities = (SVF_P3F *)LockVB(VSF_VERTEX_VELOCITY, FSL_VIDEO_CREATE);

		// stop initializing if allocation failed
		if (pVelocities == NULL)
			goto error;
	}

	// Copy data to mesh
	{
		SSetMeshIntData setMeshIntData =
		{
			&mesh,
			pVBuff,
			pTBuff,
			pQTBuff,
			pVelocities,
			nVerts,
			nInds,
			pInds,
			pPosOffset,
			flags,
			pNBuff
		};
		SetMesh_IntImpl( setMeshIntData );
	}

	// unlock all streams
	UnlockVB(VSF_GENERAL);
#if ENABLE_NORMALSTREAM_SUPPORT
	if (m_nFlags & FRM_ENABLE_NORMALSTREAM)
		UnlockVB(VSF_NORMALS);
# endif
	UnlockIB();

	if (!(flags & FSM_NO_TANGENTS))
	{
		if (mesh.m_pQTangents)
			UnlockVB(VSF_QTANGENTS);
		else
			UnlockVB(VSF_TANGENTS);
	}

	if (flags & FSM_VERTEX_VELOCITY)
	{
		UnlockVB(VSF_VERTEX_VELOCITY);
	}

  //////////////////////////////////////////////////////////////////////////
  // Copy skin-streams.
  //////////////////////////////////////////////////////////////////////////

	m_nFlags &= ~FSM_USE_COMPUTE_SKINNING;

  if (mesh.m_pBoneMapping) 
	{
		if (flags & FSM_USE_COMPUTE_SKINNING)
			m_nFlags |= FSM_USE_COMPUTE_SKINNING;
    SetSkinningDataCharacter(mesh, flags, mesh.m_pBoneMapping, mesh.m_pExtraBoneMapping);
	}

  // Create device buffers immediately in non-multithreaded mode
  if (!gRenDev->m_pRT->IsMultithreaded() && (flags & FSM_CREATE_DEVICE_MESH))
  {
		RT_CheckUpdate(_GetVertexContainer(),m_eVF, VSM_MASK);
  }

	return Size(SIZE_ONLY_SYSTEM);

error:
  RT_AllocationFailure("Generic Streaming Error", 0 );
  return ~0U;
}

size_t CRenderMesh::SetMesh(CMesh &mesh, int nSecColorsSetOffset, uint32 flags, const Vec3 *pPosOffset, bool requiresLock)
{
  LOADING_TIME_PROFILE_SECTION;
  MEMORY_SCOPE_CHECK_HEAP();
  size_t resultingSize = ~0U;
# ifdef USE_VBIB_PUSH_DOWN
  requiresLock = true;
# endif
  if(requiresLock)
	{
		SREC_AUTO_LOCK(m_sResLock);
		resultingSize = SetMesh_Int(mesh, nSecColorsSetOffset, flags, pPosOffset);
	}
	else
	{
		resultingSize = SetMesh_Int(mesh, nSecColorsSetOffset, flags, pPosOffset);
	}

  return resultingSize;
}

void CRenderMesh::SetSkinningDataVegetation(struct SMeshBoneMapping_uint8 *pBoneMapping)
{
	MEMSTAT_CONTEXT(EMemStatContextTypes::MSC_RenderMeshType, 0, this->GetTypeName());
	MEMSTAT_CONTEXT(EMemStatContextTypes::MSC_RenderMesh, 0, this->GetSourceName());
	MEMORY_SCOPE_CHECK_HEAP();
	SScopedMeshDataLock _lock(this);
	SVF_W4B_I4S *pSkinBuff = (SVF_W4B_I4S *)LockVB(VSF_HWSKIN_INFO, FSL_VIDEO_CREATE);

	// stop initializing if allocation failed
	if (pSkinBuff == NULL) return;

	for (uint32 i = 0; i < m_nVerts; i++)
	{
		// get bone IDs
		uint16 b0 = pBoneMapping[i].boneIds[0];
		uint16 b1 = pBoneMapping[i].boneIds[1];
		uint16 b2 = pBoneMapping[i].boneIds[2];
		uint16 b3 = pBoneMapping[i].boneIds[3];

		// get weights
		const uint8 w0 = pBoneMapping[i].weights[0];
		const uint8 w1 = pBoneMapping[i].weights[1];
		const uint8 w2 = pBoneMapping[i].weights[2];
		const uint8 w3 = pBoneMapping[i].weights[3];

		// if weight is zero set bone ID to zero as the bone has no influence anyway,
		// this will fix some issue with incorrectly exported models (e.g. system freezes on ATI cards when access invalid bones)
		if (w0 == 0) b0 = 0;
		if (w1 == 0) b1 = 0;
		if (w2 == 0) b2 = 0;
		if (w3 == 0) b3 = 0;

		pSkinBuff[i].indices[0] = b0;
		pSkinBuff[i].indices[1] = b1;
		pSkinBuff[i].indices[2] = b2;
		pSkinBuff[i].indices[3] = b3;

		pSkinBuff[i].weights.bcolor[0] = w0;
		pSkinBuff[i].weights.bcolor[1] = w1;
		pSkinBuff[i].weights.bcolor[2] = w2;
		pSkinBuff[i].weights.bcolor[3] = w3;

		//  if (pBSStreamTemp)
		//    pSkinBuff[i].boneSpace  = pBSStreamTemp[i];
	}
	UnlockVB(VSF_HWSKIN_INFO);

	CreateRemappedBoneIndicesPair(~0u, m_ChunksSkinned);
}

void CRenderMesh::SetSkinningDataCharacter(CMesh& mesh, uint32 flags, struct SMeshBoneMapping_uint16 *pBoneMapping, struct SMeshBoneMapping_uint16 *pExtraBoneMapping)
{
  MEMORY_SCOPE_CHECK_HEAP();
	MEMSTAT_CONTEXT(EMemStatContextTypes::MSC_RenderMeshType, 0, this->GetTypeName());
	MEMSTAT_CONTEXT(EMemStatContextTypes::MSC_RenderMesh, 0, this->GetSourceName());
  SVF_W4B_I4S *pSkinBuff = (SVF_W4B_I4S *)LockVB(VSF_HWSKIN_INFO, FSL_VIDEO_CREATE);

	// stop initializing if allocation failed
	if( pSkinBuff == NULL ) 
	{
		CRY_ASSERT_MESSAGE(pSkinBuff, "Skinning buffer allocation failed");
		return; 
	}

	for (uint32 i=0; i<m_nVerts; i++ )
	{
		// get bone IDs
		uint16 b0 = pBoneMapping[i].boneIds[0];
		uint16 b1 = pBoneMapping[i].boneIds[1];
		uint16 b2 = pBoneMapping[i].boneIds[2];
		uint16 b3 = pBoneMapping[i].boneIds[3];

		// get weights
		const uint8 w0 = pBoneMapping[i].weights[0];
		const uint8 w1 = pBoneMapping[i].weights[1];
		const uint8 w2 = pBoneMapping[i].weights[2];
		const uint8 w3 = pBoneMapping[i].weights[3];

		// if weight is zero set bone ID to zero as the bone has no influence anyway,
		// this will fix some issue with incorrectly exported models (e.g. system freezes on ATI cards when access invalid bones)
		if (w0 == 0) b0 = 0;
		if (w1 == 0) b1 = 0;
		if (w2 == 0) b2 = 0;
		if (w3 == 0) b3 = 0;

		pSkinBuff[i].indices[0] = b0;
		pSkinBuff[i].indices[1] = b1;
		pSkinBuff[i].indices[2] = b2;
		pSkinBuff[i].indices[3] = b3;

		pSkinBuff[i].weights.bcolor[0] = w0;
		pSkinBuff[i].weights.bcolor[1] = w1;
		pSkinBuff[i].weights.bcolor[2] = w2;
		pSkinBuff[i].weights.bcolor[3] = w3;
	}

	UnlockVB(VSF_HWSKIN_INFO);

	std::vector<SVF_W4B_I4S> pExtraBones;

	if (pExtraBoneMapping && m_extraBonesBuffer.IsNullBuffer() && m_nVerts)
	{
		pExtraBones.resize(m_nVerts);
		for (uint32 i=0; i<m_nVerts; i++)
		{
			// get bone IDs
			uint16 b0 = pExtraBoneMapping[i].boneIds[0];
			uint16 b1 = pExtraBoneMapping[i].boneIds[1];
			uint16 b2 = pExtraBoneMapping[i].boneIds[2];
			uint16 b3 = pExtraBoneMapping[i].boneIds[3];

			// get weights
			const uint8 w0 = pExtraBoneMapping[i].weights[0];
			const uint8 w1 = pExtraBoneMapping[i].weights[1];
			const uint8 w2 = pExtraBoneMapping[i].weights[2];
			const uint8 w3 = pExtraBoneMapping[i].weights[3];

			if (w0 == 0) b0 = 0;
			if (w1 == 0) b1 = 0;
			if (w2 == 0) b2 = 0;
			if (w3 == 0) b3 = 0;

			pExtraBones[i].indices[0] = b0;
			pExtraBones[i].indices[1] = b1;
			pExtraBones[i].indices[2] = b2;
			pExtraBones[i].indices[3] = b3;

			pExtraBones[i].weights.bcolor[0] = w0;
			pExtraBones[i].weights.bcolor[1] = w1;
			pExtraBones[i].weights.bcolor[2] = w2;
			pExtraBones[i].weights.bcolor[3] = w3;
		}

		m_nFlags |= FRM_SKINNED_EIGHT_WEIGHTS;

		m_extraBonesBuffer.Create(pExtraBones.size(), sizeof(SVF_W4B_I4S), DXGI_FORMAT_UNKNOWN, CDeviceObjectFactory::USAGE_STRUCTURED | CDeviceObjectFactory::BIND_SHADER_RESOURCE, &pExtraBones[0]);
	}
	else
	{
		// dummy buffer with no contents: there is no shader permutation, so we need to set a the resource
		// even if it's branched away (not all texture fetch instruction can be omitted and the fetch might
		// happen anyway)
		m_nFlags &= ~FRM_SKINNED_EIGHT_WEIGHTS;

		m_extraBonesBuffer.Create(0, sizeof(SVF_W4B_I4S), DXGI_FORMAT_UNKNOWN, CDeviceObjectFactory::USAGE_STRUCTURED | CDeviceObjectFactory::BIND_SHADER_RESOURCE, nullptr);
	}

	static ICVar* cvar_gd = gEnv->pConsole->GetCVar("r_ComputeSkinning");
	bool bCreateCSBuffers = (flags & FSM_USE_COMPUTE_SKINNING) && (cvar_gd && cvar_gd->GetIVal()) && m_sType == "Character";

	if (bCreateCSBuffers)
	{
		if (pExtraBoneMapping)
		{
			m_pExtraBoneMapping = reinterpret_cast<SMeshBoneMapping_uint16*>(AllocateMeshDataUnpooled(sizeof(SMeshBoneMapping_uint16) * m_nVerts));

			memcpy(m_pExtraBoneMapping, pExtraBoneMapping, sizeof(SMeshBoneMapping_uint16) * m_nVerts);
		}

		ComputeSkinningCreateBindPoseAndMorphBuffers(mesh);
	}
}


void CRenderMesh::ComputeSkinningCreateBindPoseAndMorphBuffers(CMesh& mesh)
{
	if (!m_nVerts || !m_nInds)
		return;

	std::vector<uint32> buckets(m_nVerts, 0);
	std::vector<uint32> bucketsCounter(m_nVerts, 0);
	std::vector<uint32> workBuckets(m_nVerts, 0);

	for (uint32 i=0; i<m_nInds; ++i)
		++buckets[mesh.m_pIndices[i]];

	bucketsCounter[0] = 0;
	for (uint32 i=1; i<m_nVerts; ++i)
		bucketsCounter[i] = bucketsCounter[i-1] + buckets[i-1];

	std::vector<uint32> adjTriangles;
	adjTriangles.resize(m_nInds);

	int32 k=0;
	for (uint32 i=0; i<m_nInds; i+=3)
	{
		uint32 idx0 = mesh.m_pIndices[i+0];
		uint32 idx1 = mesh.m_pIndices[i+1];
		uint32 idx2 = mesh.m_pIndices[i+2];

		adjTriangles[bucketsCounter[idx0] + workBuckets[idx0]++] = k;
		adjTriangles[bucketsCounter[idx1] + workBuckets[idx1]++] = k;
		adjTriangles[bucketsCounter[idx2] + workBuckets[idx2]++] = k;
		k++;
	}

	bool haveDeltaMorphs = mesh.m_verticesDeltaOffsets.size() > 0;

	// filling up arrays of CSSkinVertexIn
	std::vector<compute_skinning::SSkinVertexIn> vertices;
	vertices.resize(m_nVerts);
	uint32 count = 0;
	for (int32 i=0; i<m_nVerts; ++i)
	{
		if (mesh.m_pPositions)
			vertices[i].pos = mesh.m_pPositions[i];
		else
			vertices[i].pos = mesh.m_pPositionsF16[i].ToVec3();

		vertices[i].qtangent = mesh.m_pQTangents[i].GetQ();
		vertices[i].uv = mesh.m_pTexCoord[i].GetUV();
		vertices[i].morphDeltaOffset = haveDeltaMorphs ? mesh.m_verticesDeltaOffsets[i] : 0;
		vertices[i].triOffset = count;
		vertices[i].triCount = buckets[i];

		count += buckets[i];
	}
	m_nMorphs = mesh.m_numMorphs;

	m_computeSkinningDataSupply = gcpRendD3D->GetComputeSkinningStorage()->GetOrCreateComputeSkinningPerMeshData(this);
	m_computeSkinningDataSupply->PushBindPoseBuffers(m_nVerts, m_nInds, adjTriangles.size(), &vertices[0], mesh.m_pIndices, &adjTriangles[0]);
	if (mesh.m_vertexDeltas.size())
	{
		m_computeSkinningDataSupply->PushMorphs(mesh.m_vertexDeltas.size(), mesh.m_vertexMorphsBitfield.size(), &mesh.m_vertexDeltas[0], &mesh.m_vertexMorphsBitfield[0]);
	}
}

void CRenderMesh::ComputeSkinningCreateSkinningBuffers(const SVF_W4B_I4S* pBoneMapping, const SMeshBoneMapping_uint16* pExtraBoneMapping)
{
	MEMORY_SCOPE_CHECK_HEAP();

	// read remapped stream
	std::vector<compute_skinning::SSkinning> skinningVector;
	std::vector<compute_skinning::SSkinningMap> skinningVectorMap;

	uint offset = 0;
	skinningVector.reserve(m_nVerts*8);
	for (uint32 i=0; i<m_nVerts; ++i)
	{
		uint count = 0;
		for (uint32 j=0; j<4; ++j)
		{
			const uint16 index = pBoneMapping[i].indices[j];
			const uint8  weight = pBoneMapping[i].weights.bcolor[j];
			if (weight > 0)
			{
				skinningVector.push_back(compute_skinning::SSkinning());
				skinningVector.back().weightIndex = (weight << 24 | index);

				++count;
			}
		}

		// this stream is not remapped
		if (pExtraBoneMapping)
		{
			for (uint32 j=0; j<4; ++j)
			{
				const uint16 index = pExtraBoneMapping[i].boneIds[j];
				const uint8  weight = pExtraBoneMapping[i].weights[j];
				if (weight > 0)
				{
					skinningVector.push_back(compute_skinning::SSkinning());
					skinningVector.back().weightIndex = (weight << 24 | index);
					++count;
				}
			}
		}

		skinningVectorMap.push_back(compute_skinning::SSkinningMap());
		skinningVectorMap.back().offset = offset;
		skinningVectorMap.back().count = count;

		offset += count;
	}

	// this can occur before or after the other call to GetOrCreateComputeSkinningPerMeshData
	m_computeSkinningDataSupply = gcpRendD3D->GetComputeSkinningStorage()->GetOrCreateComputeSkinningPerMeshData(this);
	m_computeSkinningDataSupply->PushWeights(skinningVector.size(), skinningVectorMap.size(), &skinningVector[0], &skinningVectorMap[0]);
}

uint CRenderMesh::GetSkinningWeightCount() const
{

	if (_HasVBStream(VSF_HWSKIN_INFO))
		return (m_nFlags & FRM_SKINNED_EIGHT_WEIGHTS) ? 8 : 4;

	return 0;
}

IIndexedMesh *CRenderMesh::GetIndexedMesh(IIndexedMesh *pIdxMesh)
{
  MEMORY_SCOPE_CHECK_HEAP();

  SScopedMeshDataLock _lock(this);

  if (!pIdxMesh)
    pIdxMesh = gEnv->p3DEngine->CreateIndexedMesh();

	// catch failed allocation of IndexedMesh
	if( pIdxMesh == NULL )
		return NULL;

  CMesh* const pMesh = pIdxMesh->GetMesh();
  int i,j;

  pIdxMesh->SetVertexCount(m_nVerts);
  pIdxMesh->SetTexCoordCount(m_nVerts);
  pIdxMesh->SetTangentCount(m_nVerts);
  pIdxMesh->SetIndexCount(m_nInds);
  pIdxMesh->SetSubSetCount(m_Chunks.size());

  strided_pointer<Vec3> pVtx;
  strided_pointer<SPipTangents> pTangs;
  strided_pointer<Vec2> pTex;
  pVtx.data = (Vec3*)GetPosPtr(pVtx.iStride, FSL_READ);
  //pNorm.data = (Vec3*)GetNormalPtr(pNorm.iStride);
  pTex.data = (Vec2*)GetUVPtr(pTex.iStride, FSL_READ);
  pTangs.data = (SPipTangents*)GetTangentPtr(pTangs.iStride, FSL_READ);

	// don't copy if some src, or dest buffer is NULL (can happen because of failed allocations)
	if(		pVtx.data == NULL			|| (pMesh->m_pPositions == NULL && pMesh->m_pPositionsF16 == NULL) ||
				pTex.data == NULL			|| pMesh->m_pTexCoord == NULL ||
				pTangs.data == NULL		|| pMesh->m_pTangents  == NULL )
	{
		UnlockStream(VSF_GENERAL);
		delete pIdxMesh;
		return NULL;
	}

  for(i=0;i<(int)m_nVerts;i++)
  {
    pMesh->m_pPositions[i] = pVtx[i];
    pMesh->m_pTexCoord [i] = SMeshTexCoord(pTex[i]);
    pMesh->m_pTangents [i] = SMeshTangents(pTangs[i]);
		Vec3 n;
		pMesh->m_pTangents[i].GetN(n);
    pMesh->m_pNorms    [i] = SMeshNormal(n);
  }

  if (m_eVF==EDefaultInputLayouts::P3S_C4B_T2S || m_eVF==EDefaultInputLayouts::P3F_C4B_T2F || m_eVF==EDefaultInputLayouts::P3S_N4B_C4B_T2S)
  {
    strided_pointer<UCol> pColors;
    pColors.data = (UCol*)GetColorPtr(pColors.iStride, FSL_READ);
    pIdxMesh->SetColorCount(m_nVerts);
		for (i = 0; i < (int)m_nVerts; i++)
		{
      pMesh->m_pColor0[i] = SMeshColor(pColors[i].r, pColors[i].g, pColors[i].b, pColors[i].a);
    }
  }
  UnlockStream(VSF_GENERAL);

  vtx_idx *pInds = GetIndexPtr(FSL_READ);
  for(i=0;i<(int)m_nInds;i++)
    pMesh->m_pIndices[i] = pInds[i];
  UnlockIndexStream();

  SVF_W4B_I4S *pSkinBuff = (SVF_W4B_I4S*)LockVB(VSF_HWSKIN_INFO, FSL_READ);
  if (pSkinBuff)
  {
    pIdxMesh->AllocateBoneMapping();
    for(i=0;i<(int)m_nVerts;i++) 
      for(j=0;j<4;j++)
    {
      pMesh->m_pBoneMapping[i].boneIds[j] = pSkinBuff[i].indices[j];
      pMesh->m_pBoneMapping[i].weights[j] = pSkinBuff[i].weights.bcolor[j];
    }
    UnlockVB(VSF_HWSKIN_INFO);
  }

  for(i=0;i<(int)m_Chunks.size();i++)
  {
		pIdxMesh->SetSubsetIndexVertexRanges(i, m_Chunks[i].nFirstIndexId, m_Chunks[i].nNumIndices, m_Chunks[i].nFirstVertId, m_Chunks[i].nNumVerts);
		pIdxMesh->SetSubsetMaterialId(i, m_Chunks[i].m_nMatID);
		const int nMatFlags = m_Chunks[i].m_nMatFlags;
		const int nPhysicalizeType = (nMatFlags & MTL_FLAG_NOPHYSICALIZE)
			? PHYS_GEOM_TYPE_NONE
			: ((nMatFlags & MTL_FLAG_NODRAW) ? PHYS_GEOM_TYPE_OBSTRUCT : PHYS_GEOM_TYPE_DEFAULT);
		pIdxMesh->SetSubsetMaterialProperties(i, nMatFlags, nPhysicalizeType);

		const SMeshSubset &mss = pIdxMesh->GetSubSet(i);
		Vec3 vCenter;
		vCenter.zero();
    for(j=mss.nFirstIndexId; j<mss.nFirstIndexId+mss.nNumIndices; j++)
		{
      vCenter += pMesh->m_pPositions[pMesh->m_pIndices[j]];
		}
    if (mss.nNumIndices)
		{
      vCenter /= (float)mss.nNumIndices;
		}
		float fRadius = 0;
    for(j=mss.nFirstIndexId; j<mss.nFirstIndexId+mss.nNumIndices; j++)
		{
      fRadius = max(fRadius, (pMesh->m_pPositions[pMesh->m_pIndices[j]]-vCenter).len2());
		}
		fRadius = sqrt_tpl(fRadius);
		pIdxMesh->SetSubsetBounds(i, vCenter, fRadius);
  }

  return pIdxMesh;
}

void CRenderMesh::GenerateQTangents()
{
  MEMORY_SCOPE_CHECK_HEAP();
  // FIXME: This needs to be cleaned up. Breakable foliage shouldn't need both streams, and this shouldn't be duplicated
  // between here and CryAnimation.
  SScopedMeshDataLock _lock(this);
  int srcStride = 0;
  void *pSrcD = LockVB(VSF_TANGENTS, FSL_READ, 0, 0, &srcStride);
  if (pSrcD)
  {
    int dstStride = 0;
    void *pDstD = LockVB(VSF_QTANGENTS, FSL_VIDEO_CREATE, 0, 0, &dstStride);
    assert(pDstD);
    if (pDstD)
    {
		SPipTangents *pTangents = (SPipTangents *)pSrcD;
		SPipQTangents *pQTangents = (SPipQTangents *)pDstD;
		MeshTangentsFrameToQTangents(
			pTangents, srcStride, m_nVerts,
			pQTangents, dstStride);
    }
    UnlockVB(VSF_QTANGENTS);
  }
  UnlockVB(VSF_TANGENTS);
}

void CRenderMesh::CreateChunksSkinned()
{
  MEMORY_SCOPE_CHECK_HEAP();
	MEMSTAT_CONTEXT(EMemStatContextTypes::MSC_RenderMeshType, 0, this->GetTypeName());
	MEMSTAT_CONTEXT(EMemStatContextTypes::MSC_RenderMesh, 0, this->GetSourceName());

  ReleaseRenderChunks(&m_ChunksSkinned);

  TRenderChunkArray& arrSrcMats = m_Chunks;
  TRenderChunkArray& arrNewMats = m_ChunksSkinned;
  arrNewMats.resize (arrSrcMats.size());
  for (int i=0; i<arrSrcMats.size(); ++i)
  {
    CRenderChunk& rSrcMat = arrSrcMats[i];
    CRenderChunk& rNewMat = arrNewMats[i];
    rNewMat = rSrcMat;
    CREMeshImpl *re = (CREMeshImpl*) rSrcMat.pRE;
    if (re)
    {
      rNewMat.pRE = (CREMeshImpl*) gRenDev->EF_CreateRE(eDATA_Mesh);
      CRenderElement *pNext = rNewMat.pRE->m_NextGlobal;
      CRenderElement *pPrev = rNewMat.pRE->m_PrevGlobal;
      *(CREMeshImpl*)rNewMat.pRE = *re;
      if (rNewMat.pRE->m_pChunk) // affects the source mesh!! will only work correctly if the source is deleted after copying
        rNewMat.pRE->m_pChunk = &rNewMat;
      rNewMat.pRE->m_NextGlobal = pNext;
      rNewMat.pRE->m_PrevGlobal = pPrev;
      rNewMat.pRE->m_pRenderMesh = this;
      rNewMat.pRE->m_CustomData = NULL;
    }
  }
}

int CRenderMesh::GetRenderChunksCount(IMaterial * pMaterial, int & nRenderTrisCount)
{
  int nCount = 0;
  nRenderTrisCount = 0;

  CRenderer *rd = gRenDev;
  const uint32 ni = (uint32)m_Chunks.size();
  for (uint32 i=0; i<ni; i++)
  {
    CRenderChunk *pChunk = &m_Chunks[i];
    CRenderElement * pREMesh = pChunk->pRE;

    SShaderItem *pShaderItem = &pMaterial->GetShaderItem(pChunk->m_nMatID);

		CShaderResources* pR = (CShaderResources*)pShaderItem->m_pShaderResources;
    CShader *pS = (CShader *)pShaderItem->m_pShader;
    if (pREMesh && pS && pR)
    {
      if(pChunk->m_nMatFlags & MTL_FLAG_NODRAW)
        continue;

      if (pS->m_Flags2 & EF2_NODRAW)
        continue;

      if(pChunk->nNumIndices)
      {
        nRenderTrisCount += pChunk->nNumIndices/3;
        nCount++;
      }
    }
  }

  return nCount;
}

void CRenderMesh::CopyTo(IRenderMesh *_pDst, int nAppendVtx, bool bDynamic, bool fullCopy)
{
	ASSERT_IS_MAIN_THREAD(gRenDev->m_pRT);
	CRY_ASSERT(this != _pDst);

	MEMORY_SCOPE_CHECK_HEAP();
	CRenderMesh *pDst = (CRenderMesh *)_pDst;
#ifdef USE_VBIB_PUSH_DOWN
	SREC_AUTO_LOCK(m_sResLock);
#endif
	TRenderChunkArray& arrSrcMats = m_Chunks;
	TRenderChunkArray& arrNewMats = pDst->m_Chunks;
	//pDst->m_bMaterialsWasCreatedInRenderer  = true;
	arrNewMats.resize(arrSrcMats.size());
	int i;
	for (i = 0; i < arrSrcMats.size(); ++i)
	{
		CRenderChunk& rSrcMat = arrSrcMats[i];
		CRenderChunk& rNewMat = arrNewMats[i];
		rNewMat = rSrcMat;
		rNewMat.nNumVerts += ((m_nVerts - 2 - rNewMat.nNumVerts - rNewMat.nFirstVertId) >> 31) & nAppendVtx;
		CREMeshImpl* re = (CREMeshImpl*)rSrcMat.pRE;
		if (re)
		{
			rNewMat.pRE = (CREMeshImpl*)gRenDev->EF_CreateRE(eDATA_Mesh);
			CRenderElement *pNext = rNewMat.pRE->m_NextGlobal;
			CRenderElement *pPrev = rNewMat.pRE->m_PrevGlobal;
			*(CREMeshImpl*)rNewMat.pRE = *re;
			if (rNewMat.pRE->m_pChunk) // affects the source mesh!! will only work correctly if the source is deleted after copying
			{
				rNewMat.pRE->m_pChunk = &rNewMat;
				rNewMat.pRE->m_pChunk->nNumVerts += ((m_nVerts - 2 - re->m_pChunk->nNumVerts - re->m_pChunk->nFirstVertId) >> 31) & nAppendVtx;
			}
			rNewMat.pRE->m_NextGlobal = pNext;
			rNewMat.pRE->m_PrevGlobal = pPrev;
			rNewMat.pRE->m_pRenderMesh = pDst;
			//assert(rNewMat.pRE->m_CustomData);
			rNewMat.pRE->m_CustomData = NULL;
	}
}

	SScopedMeshDataLock _lock(this);
	SScopedMeshDataLock _lockDst(pDst);

	pDst->m_nVerts = m_nVerts + nAppendVtx;
	if (fullCopy)
	{
		pDst->m_eVF = m_eVF;
		for (i = 0; i < VSF_NUM; i++)
		{
			void *pSrcD = LockVB(i, FSL_READ);
			if (pSrcD)
			{
				void *pDstD = pDst->LockVB(i, FSL_VIDEO_CREATE);
				assert(pDstD);
				if (pDstD)
					cryMemcpy(pDstD, pSrcD, GetStreamSize(i), MC_CPU_TO_GPU);
				pDst->UnlockVB(i);
			}
			UnlockVB(i);
		}

		pDst->m_nInds = m_nInds;
		void *pSrcD = LockIB(FSL_READ);
		if (pSrcD)
		{
			void *pDstD = pDst->LockIB(FSL_VIDEO_CREATE);
			assert(pDstD);
			if (pDstD)
				cryMemcpy(pDstD, pSrcD, m_nInds * sizeof(vtx_idx), MC_CPU_TO_GPU);
			pDst->UnlockIB();
		}

		pDst->m_eType = m_eType;
		pDst->m_nFlags = m_nFlags;
	}
	UnlockIB();
}

// set effector for all chunks
void CRenderMesh::SetCustomTexID( int nCustomTID )
{
  MEMORY_SCOPE_CHECK_HEAP();
  if (m_Chunks.size() && nCustomTID != 0)
  {
    for(int i=0; i<m_Chunks.size(); i++)
    {
      CRenderChunk *pChunk = &m_Chunks[i];
      //    pChunk->shaderItem.m_pShader = pShader;
      if (pChunk->pRE)
        pChunk->pRE->m_CustomTexBind[0] = nCustomTID;
    }
  }
}

void CRenderMesh::SetChunk(int nIndex, CRenderChunk &inChunk)
{
	MEMSTAT_CONTEXT(EMemStatContextTypes::MSC_RenderMeshType, 0, this->GetTypeName());
	MEMSTAT_CONTEXT(EMemStatContextTypes::MSC_RenderMesh, 0, this->GetSourceName());

  if (!inChunk.nNumIndices || !inChunk.nNumVerts)
    return;

  CRenderChunk *pRenderChunk = NULL;

  if (nIndex < 0 || nIndex >= m_Chunks.size())
  {
    // add new chunk
    CRenderChunk matinfo;
    m_Chunks.push_back(matinfo);
    pRenderChunk = &m_Chunks.back();

    pRenderChunk->pRE = (CREMeshImpl*) gRenDev->EF_CreateRE(eDATA_Mesh);
    pRenderChunk->pRE->m_CustomTexBind[0] = m_nClientTextureBindID;
  }
  else
  {
    // use present chunk
    pRenderChunk = &m_Chunks[nIndex];
  }

  pRenderChunk->m_nMatID = inChunk.m_nMatID;
  pRenderChunk->m_nMatFlags = inChunk.m_nMatFlags;

  pRenderChunk->nFirstIndexId	= inChunk.nFirstIndexId;
  pRenderChunk->nNumIndices		= std::max<uint32>(inChunk.nNumIndices,0);
  pRenderChunk->nFirstVertId	= inChunk.nFirstVertId;
  pRenderChunk->nNumVerts			= std::max<uint32>(inChunk.nNumVerts,0);
  pRenderChunk->nSubObjectIndex = inChunk.nSubObjectIndex;

  pRenderChunk->m_texelAreaDensity = inChunk.m_texelAreaDensity;

  // update chunk RE
  if (pRenderChunk->pRE)
    AssignChunk(pRenderChunk, (CREMeshImpl*) pRenderChunk->pRE);
  assert(pRenderChunk->nFirstIndexId + pRenderChunk->nNumIndices <= m_nInds);
}

void CRenderMesh::SetChunk(IMaterial *pNewMat, int nFirstVertId, int nVertCount, int nFirstIndexId, int nIndexCount, float texelAreaDensity, int nIndex)
{
  CRenderChunk chunk;

  if (pNewMat)
    chunk.m_nMatFlags = pNewMat->GetFlags();

  if (nIndex < 0 || nIndex >= m_Chunks.size())
    chunk.m_nMatID = m_Chunks.size();
  else
    chunk.m_nMatID = nIndex;

  chunk.nFirstVertId = nFirstVertId;
  chunk.nNumVerts = nVertCount;

  chunk.nFirstIndexId = nFirstIndexId;
  chunk.nNumIndices = nIndexCount;

	chunk.m_texelAreaDensity = texelAreaDensity;

  SetChunk(nIndex, chunk);
}

//================================================================================================================

bool CRenderMesh::PrepareCachePos()
{
	if (!m_pCachePos && (m_eVF == EDefaultInputLayouts::P3S_C4B_T2S || m_eVF == EDefaultInputLayouts::P3S_N4B_C4B_T2S))
	{
		m_pCachePos = AllocateMeshData<Vec3>(m_nVerts);
		if (m_pCachePos)
		{
			m_nFrameRequestCachePos = GetCurrentFrameID();
			return true;
		}
	}
	return false;
}

bool CRenderMesh::CreateCachePos(byte *pSrc, uint32 nStrideSrc, uint nFlags)
{
  PROFILE_FRAME(Mesh_CreateCachePos);
  if (m_eVF == EDefaultInputLayouts::P3S_C4B_T2S || m_eVF == EDefaultInputLayouts::P3S_N4B_C4B_T2S)
	{
	#ifdef USE_VBIB_PUSH_DOWN
		SREC_AUTO_LOCK(m_sResLock);//on USE_VBIB_PUSH_DOWN tick is executed in renderthread
	#endif

		m_nFlagsCachePos = (nFlags & FSL_WRITE) != 0;
		m_nFrameRequestCachePos = GetCurrentFrameID();
		if ((nFlags & FSL_READ) && m_pCachePos)
			return true;
		if ((nFlags == FSL_SYSTEM_CREATE) && m_pCachePos)
			return true;
		if (!m_pCachePos)
			m_pCachePos = AllocateMeshData<Vec3>(m_nVerts);
		if (m_pCachePos)
		{
			if (nFlags == FSL_SYSTEM_UPDATE || (nFlags & FSL_READ))
			{
				const auto vertexFormatDescriptor = CDeviceObjectFactory::GetInputLayoutDescriptor(_GetVertexFormat());
				if (!vertexFormatDescriptor)
					return false;

				int8 offs = vertexFormatDescriptor->m_Offsets[SInputLayout::eOffset_Position];
				if (offs >= 0)
					pSrc += offs;

				for (uint32 i=0; i<m_nVerts; i++)
				{
					Vec3f16 *pVSrc = (Vec3f16 *)pSrc;
					m_pCachePos[i] = pVSrc->ToVec3();
					pSrc += nStrideSrc;
				}
			}
			return true;
		}
	}
  return false;
}

bool CRenderMesh::CreateCacheUVs(byte *pSrc, uint32 nStrideSrc, uint nFlags)
{
	PROFILE_FRAME(Mesh_CreateCacheUVs);
	if (m_eVF == EDefaultInputLayouts::P3S_C4B_T2S || m_eVF == EDefaultInputLayouts::P3S_N4B_C4B_T2S)
	{
		m_nFlagsCacheUVs = (nFlags & FSL_WRITE) != 0;
		m_nFrameRequestCacheUVs = GetCurrentFrameID();
		if ((nFlags & FSL_READ) && m_pCacheUVs)
			return true;
		if ((nFlags == FSL_SYSTEM_CREATE) && m_pCacheUVs)
			return true;
		if (!m_pCacheUVs)
			m_pCacheUVs = AllocateMeshData<Vec2>(m_nVerts);
		if (m_pCacheUVs)
		{
			if (nFlags == FSL_SYSTEM_UPDATE || (nFlags & FSL_READ))
			{
				const auto vertexFormatDescriptor = CDeviceObjectFactory::GetInputLayoutDescriptor(_GetVertexFormat());
				if (!vertexFormatDescriptor)
					return false;

				int8 offs = vertexFormatDescriptor->m_Offsets[SInputLayout::eOffset_TexCoord];
				if (offs >= 0)
					pSrc += offs;

				for (uint32 i=0; i<m_nVerts; i++)
				{
					Vec2f16 *pVSrc = (Vec2f16 *)pSrc;
					m_pCacheUVs[i] = pVSrc->ToVec2();
					pSrc += nStrideSrc;
				}
			}
			return true;
		}
	}
	return false;
}

byte *CRenderMesh::GetPosPtrNoCache(int32& nStride, uint32 nFlags, int32 nOffset)
{
  int nStr = 0;
  byte *pData = NULL;
  pData = (byte *)LockVB(VSF_GENERAL, nFlags, nOffset, 0, &nStr, true);
  ASSERT_LOCK;
  if (!pData)
    return NULL;
  nStride = nStr;

  const auto vertexFormatDescriptor = CDeviceObjectFactory::GetInputLayoutDescriptor(_GetVertexFormat());
  if (!vertexFormatDescriptor)
	  return NULL;

  int8 offs = vertexFormatDescriptor->m_Offsets[SInputLayout::eOffset_Position];
  if (offs >= 0)
	  return &pData[offs];
  return NULL;
}

byte *CRenderMesh::GetPosPtr(int32& nStride, uint32 nFlags, int32 nOffset)
{
  PROFILE_FRAME(Mesh_GetPosPtr);
  int nStr = 0;
  byte *pData = NULL;
  pData = (byte *)LockVB(VSF_GENERAL, nFlags, nOffset, 0, &nStr, true, true);
  ASSERT_LOCK;
  if (!pData)
    return NULL;
  nStride = nStr;

  const auto vertexFormatDescriptor = CDeviceObjectFactory::GetInputLayoutDescriptor(_GetVertexFormat());
  if (!vertexFormatDescriptor)
	  return NULL;

  int8 offs = vertexFormatDescriptor->m_Offsets[SInputLayout::eOffset_Position];

  // TODO: remove conversion from Half to Float
  if (CreateCachePos(pData, nStr, nFlags))
  {
	  pData = (byte *)m_pCachePos;
	  nStride = sizeof(Vec3);
	  return pData;
  }

  if (offs >= 0)
    return &pData[offs];
  return NULL;
}

vtx_idx *CRenderMesh::GetIndexPtr(uint32 nFlags, int32 nOffset)
{
  vtx_idx *pData = LockIB(nFlags, nOffset, 0);
  assert((m_nInds == 0) || pData);
  return pData;
}

byte *CRenderMesh::GetColorPtr(int32& nStride, uint32 nFlags, int32 nOffset)
{
  PROFILE_FRAME(Mesh_GetColorPtr);
  int nStr = 0;
  byte *pData = (byte *)LockVB(VSF_GENERAL, nFlags, nOffset, 0, &nStr);
  ASSERT_LOCK;
  if (!pData)
    return NULL;
  nStride = nStr;

  const auto vertexFormatDescriptor = CDeviceObjectFactory::GetInputLayoutDescriptor(_GetVertexFormat());
  if (!vertexFormatDescriptor)
	  return NULL;

  int8 offs = vertexFormatDescriptor->m_Offsets[SInputLayout::eOffset_Color];
  if (offs >= 0)
    return &pData[offs];
  return NULL;
}
byte *CRenderMesh::GetNormPtr(int32& nStride, uint32 nFlags, int32 nOffset)
{
  PROFILE_FRAME(Mesh_GetNormPtr);
  int nStr = 0;
  byte *pData = 0;
# if ENABLE_NORMALSTREAM_SUPPORT
  pData = (byte *)LockVB(VSF_NORMALS, nFlags, nOffset, 0, &nStr);
  if (pData)
  {
    nStride = sizeof(SPipNormal);
    int8 offs = CDeviceObjectFactory::GetInputLayoutDescriptor(EDefaultInputLayouts::N3F)->m_Offsets[SInputLayout::eOffset_Normal];
    if (offs >= 0)
      return &pData[offs];
    return NULL;
  }
# endif
  pData = (byte *)LockVB(VSF_GENERAL, nFlags, nOffset, 0, &nStr);
  ASSERT_LOCK;
  if (!pData)
    return NULL;
  nStride = nStr;

  const auto vertexFormatDescriptor = CDeviceObjectFactory::GetInputLayoutDescriptor(_GetVertexFormat());
  if (!vertexFormatDescriptor)
	  return NULL;

  int8 offs = vertexFormatDescriptor->m_Offsets[SInputLayout::eOffset_Normal];
  if (offs >= 0)
    return &pData[offs];
  return NULL;
}
byte *CRenderMesh::GetUVPtrNoCache(int32& nStride, uint32 nFlags, int32 nOffset)
{
	PROFILE_FRAME(Mesh_GetUVPtrNoCache);
	int nStr = 0;
	byte *pData = (byte *)LockVB(VSF_GENERAL, nFlags, nOffset, 0, &nStr);
	ASSERT_LOCK;
	if (!pData)
		return NULL;
	nStride = nStr;

	const auto vertexFormatDescriptor = CDeviceObjectFactory::GetInputLayoutDescriptor(_GetVertexFormat());
	if (!vertexFormatDescriptor)
		return NULL;

	int8 offs = vertexFormatDescriptor->m_Offsets[SInputLayout::eOffset_TexCoord];
	if (offs >= 0)
		return &pData[offs];
	return NULL;
}
byte *CRenderMesh::GetUVPtr(int32& nStride, uint32 nFlags, int32 nOffset)
{
  PROFILE_FRAME(Mesh_GetUVPtr);
  int nStr = 0;
  byte *pData = (byte *)LockVB(VSF_GENERAL, nFlags, nOffset, 0, &nStr);
  ASSERT_LOCK;
  if (!pData)
    return NULL;
  nStride = nStr;

  const auto vertexFormatDescriptor = CDeviceObjectFactory::GetInputLayoutDescriptor(_GetVertexFormat());
  if (!vertexFormatDescriptor)
	  return NULL;

  int8 offs = vertexFormatDescriptor->m_Offsets[SInputLayout::eOffset_TexCoord];

  // TODO: remove conversion from Half to Float
  if (CreateCacheUVs(pData, nStr, nFlags))
  {
    pData = (byte *)m_pCacheUVs;
    nStride = sizeof(Vec2);
    return pData;
  }

  if (offs >= 0)
    return &pData[offs];
  return NULL;
}

byte *CRenderMesh::GetTangentPtr(int32& nStride, uint32 nFlags, int32 nOffset)
{
  PROFILE_FRAME(Mesh_GetTangentPtr);
  int nStr = 0;
  byte *pData = (byte *)LockVB(VSF_TANGENTS, nFlags, nOffset, 0, &nStr);
  //ASSERT_LOCK;
  if (!pData)
    pData = (byte *)LockVB(VSF_QTANGENTS, nFlags, nOffset, 0, &nStr);
  if (!pData)
    return NULL;
  nStride = nStr;
  return pData;
}
byte *CRenderMesh::GetQTangentPtr(int32& nStride, uint32 nFlags, int32 nOffset)
{
  PROFILE_FRAME(Mesh_GetQTangentPtr);
  int nStr = 0;
  byte *pData = (byte *)LockVB(VSF_QTANGENTS, nFlags, nOffset, 0, &nStr);
  //ASSERT_LOCK;
  if (!pData)
    return NULL;
  nStride = nStr;
  return pData;
}

byte *CRenderMesh::GetHWSkinPtr(int32& nStride, uint32 nFlags, int32 nOffset, bool remapped)
{
  PROFILE_FRAME(Mesh_GetHWSkinPtr);
  int nStr = 0;
  byte *pData = (byte *)LockVB(VSF_HWSKIN_INFO, nFlags, nOffset, 0, &nStr);
  ASSERT_LOCK;
  if (!pData)
    return NULL;
  nStride = nStr;
  return pData;
}

byte *CRenderMesh::GetVelocityPtr(int32& nStride, uint32 nFlags, int32 nOffset)
{
  PROFILE_FRAME(Mesh_GetMorphTargetPtr);
  int nStr = 0;
  byte *pData = (byte *)LockVB(VSF_VERTEX_VELOCITY, nFlags, nOffset, 0, &nStr);
  ASSERT_LOCK;
  if (!pData)
    return NULL;
  nStride = nStr;
  return pData;
}

bool CRenderMesh::IsEmpty()
{
	ASSERT_IS_MAIN_THREAD(gRenDev->m_pRT)
  SMeshStream* pMS = GetVertexStream(VSF_GENERAL, 0);
  return (!m_nVerts || (!pMS || pMS->m_nID == ~0u || !pMS->m_pUpdateData) || (!_HasIBStream() && !m_IBStream.m_pUpdateData));
}

void CRenderMesh::RT_AllocationFailure(const char* sPurpose, uint32 nSize)
{
  SREC_AUTO_LOCK(m_sResLock);
	Cleanup();
	m_nVerts = 0;
	m_nInds = 0;
	m_nFlags |= FRM_ALLOCFAILURE;
#if !defined(_RELEASE)
  CryLogAlways("rendermesh '%s(%s)' suffered from a buffer allocation failure for \"%s\" size %u bytes on thread 0x%" PRI_THREADID, m_sSource.c_str(), m_sType.c_str(), sPurpose, nSize, CryGetCurrentThreadId());
# endif
}

#ifdef MESH_TESSELLATION_RENDERER
namespace
{
template<class VertexFormat, class VecPos> 
struct SAdjVertCompare
{
	inline bool operator()(const int i1, const int i2) const
	{
		const VecPos & pVert1 = m_pVerts[i1].xyz;
		const VecPos & pVert2 = m_pVerts[i2].xyz;
		if (pVert1.x < pVert2.x) return true;
		if (pVert1.x > pVert2.x) return false;
		if (pVert1.y < pVert2.y) return true;
		if (pVert1.y > pVert2.y) return false;
		return pVert1.z < pVert2.z;
	}
	const VertexFormat *m_pVerts;
};
}

template<class VertexFormat, class VecPos, class VecUV> 
void CRenderMesh::BuildAdjacency(const VertexFormat *pVerts, unsigned int nVerts, const vtx_idx *pIndexBuffer, uint nTrgs, std::vector<VecUV> &pTxtAdjBuffer)
{
	struct SAdjVertCompare<VertexFormat, VecPos> compare;
	compare.m_pVerts = pVerts;

	// this array will contain indices of vertices sorted by float3 position - so that we could find vertices with equal positions (they have to be adjacent in the array)
	std::vector<int> arrSortedVertIDs;
	// we allocate a bit more (by one) because we will need extra element for scan operation (lower)
	arrSortedVertIDs.resize(nVerts + 1);
	int *_iSortedVerts = &arrSortedVertIDs[0];
	for (int iv = 0; iv < nVerts; ++iv)
		arrSortedVertIDs[iv] = iv;

	std::sort(arrSortedVertIDs.begin(), arrSortedVertIDs.end() - 1, compare);

	// compute how many unique vertices are there, also setup links from each vertex to master vertex
	std::vector<int> arrLinkToMaster;
	arrLinkToMaster.resize(nVerts);
	int nUniqueVertices = 0;
	for (int iv0 = 0; iv0 < nVerts; )
	{
		int iMaster = arrSortedVertIDs[iv0];
		const VecPos vMasterVertex = pVerts[iMaster].xyz;
		arrLinkToMaster[iMaster] = iMaster;
		int iv1 = iv0 + 1;
		for ( ; iv1 < nVerts; ++iv1)
		{
			const VecPos vSlaveVertex = pVerts[arrSortedVertIDs[iv1]].xyz;
			if (vSlaveVertex != vMasterVertex)
			{
				break;
			}
			arrLinkToMaster[arrSortedVertIDs[iv1]] = iMaster;
		}
		iv0 = iv1;
		++nUniqueVertices;
	}
	if (nUniqueVertices == nVerts)
	{
		// no need to recode anything - the mesh is perfect
		return;
	}
	// compute how many triangles connect to every master vertex
	std::vector<int> &arrConnectedTrianglesCount = arrSortedVertIDs;
	for (int i = 0; i < arrConnectedTrianglesCount.size(); ++i)
		arrConnectedTrianglesCount[i] = 0;
	int *_nConnectedTriangles = &arrConnectedTrianglesCount[0];
	for (int it = 0; it < nTrgs; ++it)
	{
		const vtx_idx *pTrg = &pIndexBuffer[it * 3];
		int iMasterVertex0 = arrLinkToMaster[pTrg[0]];
		int iMasterVertex1 = arrLinkToMaster[pTrg[1]];
		int iMasterVertex2 = arrLinkToMaster[pTrg[2]];
		if (iMasterVertex0 == iMasterVertex1 || iMasterVertex0 == iMasterVertex2 || iMasterVertex1 == iMasterVertex2)
			continue; // degenerate triangle - skip it
		++arrConnectedTrianglesCount[iMasterVertex0];
		++arrConnectedTrianglesCount[iMasterVertex1];
		++arrConnectedTrianglesCount[iMasterVertex2];
	}
	// scan
	std::vector<int> &arrFirstConnectedTriangle = arrSortedVertIDs;
	for (int iv = 0; iv < nVerts; ++iv)
	{
		arrFirstConnectedTriangle[iv + 1] += arrFirstConnectedTriangle[iv];
	}
	{
		int iTmp = arrFirstConnectedTriangle[0];
		arrFirstConnectedTriangle[0] = 0;
		for (int iv = 0; iv < nVerts; ++iv)
		{
			int iTmp1 = arrFirstConnectedTriangle[iv + 1];
			arrFirstConnectedTriangle[iv + 1] = iTmp;
			iTmp = iTmp1;
		}
	}
	// create a list of triangles for each master vertex
	std::vector<int> arrConnectedTriangles;
	arrConnectedTriangles.resize(arrFirstConnectedTriangle[nVerts]);
	for (int it = 0; it < nTrgs; ++it)
	{
		const vtx_idx *pTrg = &pIndexBuffer[it * 3];
		int iMasterVertex0 = arrLinkToMaster[pTrg[0]];
		int iMasterVertex1 = arrLinkToMaster[pTrg[1]];
		int iMasterVertex2 = arrLinkToMaster[pTrg[2]];
		if (iMasterVertex0 == iMasterVertex1 || iMasterVertex0 == iMasterVertex2 || iMasterVertex1 == iMasterVertex2)
			continue; // degenerate triangle - skip it
		arrConnectedTriangles[arrFirstConnectedTriangle[iMasterVertex0]++] = it;
		arrConnectedTriangles[arrFirstConnectedTriangle[iMasterVertex1]++] = it;
		arrConnectedTriangles[arrFirstConnectedTriangle[iMasterVertex2]++] = it;
	}
	// return scan array to initial state
	{
		int iTmp = arrFirstConnectedTriangle[0];
		arrFirstConnectedTriangle[0] = 0;
		for (int iv = 0; iv < nVerts; ++iv)
		{
			int iTmp1 = arrFirstConnectedTriangle[iv + 1];
			arrFirstConnectedTriangle[iv + 1] = iTmp;
			iTmp = iTmp1;
		}
	}
	// find matches for boundary edges
	for (int it = 0; it < nTrgs; ++it)
	{
		const vtx_idx *pTrg = &pIndexBuffer[it * 3];
		for (int ie = 0; ie < 3; ++ie)
		{
			// fix the corner here
			{
				int ivCorner = pTrg[ie];
				pTxtAdjBuffer[it * 12 + 9 + ie] = pVerts[arrLinkToMaster[ivCorner]].st;
			}
			// proceed with fixing the edges
			int iv0 = pTrg[ie];
			int iMasterVertex0 = arrLinkToMaster[iv0];

			int iv1 = pTrg[(ie + 1) % 3];
			int iMasterVertex1 = arrLinkToMaster[iv1];
			if (iMasterVertex0 == iMasterVertex1)
			{
				// some degenerate case - skip it
				continue;
			}
			// find a triangle that has both iMasterVertex0 and iMasterVertex1
			for (int i0 = arrFirstConnectedTriangle[iMasterVertex0]; i0 < arrFirstConnectedTriangle[iMasterVertex0 + 1]; ++i0)
			{
				int iOtherTriangle = arrConnectedTriangles[i0];
				if (iOtherTriangle >= it) // we are going to stick to this other triangle only if it's index is less than ours
					continue;
				const vtx_idx *pOtherTrg = &pIndexBuffer[iOtherTriangle * 3];
				int iRecode0 = -1;
				int iRecode1 = -1;
				// setup recode indices
				for (int ieOther = 0; ieOther < 3; ++ieOther)
				{
					if (arrLinkToMaster[pOtherTrg[ieOther]] == iMasterVertex0)
					{
						iRecode0 = pOtherTrg[ieOther];
					}
					else if (arrLinkToMaster[pOtherTrg[ieOther]] == iMasterVertex1)
					{
						iRecode1 = pOtherTrg[ieOther];
					}
				}
				if (iRecode0 != -1 && iRecode1 != -1)
				{
					// this triangle is our neighbor
					pTxtAdjBuffer[it * 12 + 3 + ie * 2] = pVerts[iRecode0].st;
					pTxtAdjBuffer[it * 12 + 3 + ie * 2 + 1] = pVerts[iRecode1].st;
				}
			}
		}
	}
}
#endif //#ifdef MESH_TESSELLATION_RENDERER

bool CRenderMesh::RT_CheckUpdate(CRenderMesh *pVContainer, InputLayoutHandle eVF, uint32 nStreamMask, bool bTessellation)
{
	CRY_ASSERT(gRenDev->m_pRT->IsRenderThread());

	const int threadId = gRenDev->GetRenderThreadID();
	int nFrame = gRenDev->GetRenderFrameID();
	
	MEMSTAT_CONTEXT(EMemStatContextTypes::MSC_RenderMeshType, 0, this->GetTypeName());
	MEMSTAT_CONTEXT(EMemStatContextTypes::MSC_RenderMesh, 0, this->GetSourceName());
	PrefetchLine(&m_IBStream, 0);

	bool bSkinned = (m_nFlags & (FRM_SKINNED | FRM_SKINNEDNEXTDRAW)) != 0;

	m_nFlags &= ~FRM_SKINNEDNEXTDRAW;

	IF(!CanUpdate(), 0)
		return false;

	FUNCTION_PROFILER_RENDER_FLAT
		PrefetchVertexStreams();

	{
		SRecursiveSpinLocker _lock0(&m_sResLock);
		SRecursiveSpinLocker _lock1(&pVContainer->m_sResLock);

		for (const uint32& guidToDelete : pVContainer->m_DeletedBoneIndices)
		{
			for (auto it = pVContainer->m_RemappedBoneIndices.begin(); it != pVContainer->m_RemappedBoneIndices.end(); ++it)
			{
				if (it->guid == guidToDelete)
				{
					if (it->buffer != ~0u)
						gRenDev->m_DevBufMan.Destroy(it->buffer);
					pVContainer->m_RemappedBoneIndices.erase(it);
					break;
				}
			}
		}
		pVContainer->m_DeletedBoneIndices.clear();

		// For bone-indices creation list: Only process elements whose target frameId is before or equal to our current frameId
		auto createdBoneIndicesIt = std::partition(pVContainer->m_CreatedBoneIndices.begin(), pVContainer->m_CreatedBoneIndices.end(),
			[&](const SBoneIndexStreamRequest &e) { return e.frameId > nFrame; });

		for (auto it = createdBoneIndicesIt; it != pVContainer->m_CreatedBoneIndices.end(); ++it)
		{
			CRY_ASSERT(it->frameId <= nFrame);

			bool bFound = false;
			const uint32 guid = it->guid;
			for (size_t j = 0, end = pVContainer->m_RemappedBoneIndices.size(); j < end; ++j)
			{
				if (pVContainer->m_RemappedBoneIndices[j].guid == guid && pVContainer->m_RemappedBoneIndices[j].refcount)
				{
					bFound = true;
					++pVContainer->m_RemappedBoneIndices[j].refcount;
				}
			}

			if (!bFound)
			{
				const size_t bufferSize = pVContainer->GetVerticesCount() * pVContainer->GetStreamStride(VSF_HWSKIN_INFO);
				const size_t streamableSize = AlignedStreamableMeshDataSize(bufferSize);

				SBoneIndexStream stream;
				stream.buffer = gRenDev->m_DevBufMan.Create(BBT_VERTEX_BUFFER, BU_STATIC, bufferSize);
				stream.guid = guid;
				stream.refcount = it->refcount;
				gRenDev->m_DevBufMan.UpdateBuffer(stream.buffer, it->pStream, streamableSize);
				pVContainer->m_RemappedBoneIndices.push_back(stream);

				static ICVar* cvar_gd = gEnv->pConsole->GetCVar("r_ComputeSkinning");
				if (cvar_gd && cvar_gd->GetIVal() && m_sType == "Character" && (pVContainer->m_nFlags & FSM_USE_COMPUTE_SKINNING))
					pVContainer->ComputeSkinningCreateSkinningBuffers(it->pStream, it->pExtraStream);
			}

			FreeMeshDataUnpooled(it->pStream);
//			if (it->pExtraStream)
//				FreeMeshDataUnpooled(it->pExtraStream);
		}
		pVContainer->m_CreatedBoneIndices.erase(createdBoneIndicesIt, pVContainer->m_CreatedBoneIndices.end());
	}

	PrefetchLine(pVContainer->m_VBStream, 0);
	SMeshStream* pMS = pVContainer->GetVertexStream(VSF_GENERAL, 0);

	if ((m_pVertexContainer || m_nVerts > 2) && pMS)
	{
		PrefetchLine(pVContainer->m_VBStream, 128);
		if (pMS->m_pUpdateData && pMS->m_nFrameAccess != nFrame)
		{
			pMS->m_nFrameAccess = nFrame;
			if (pMS->m_nFrameRequest > pMS->m_nFrameUpdate)
			{
				{
					PROFILE_FRAME(Mesh_CheckUpdateUpdateGBuf);
					if (!(pMS->m_nLockFlags & FSL_WRITE))
					{
						if (!pVContainer->UpdateVidVertices(VSF_GENERAL))
						{
							RT_AllocationFailure("Update General Stream", GetStreamSize(VSF_GENERAL, m_nVerts));
							return false;
						}
						pMS->m_nFrameUpdate = nFrame;
					}
					else
					{
						if (pMS->m_nID == ~0u)
							return false;
					}
				}
			}
		}
		// Set both tangent flags
		if (nStreamMask & VSM_TANGENTS)
			nStreamMask |= VSM_TANGENTS;

		// Additional streams updating
		if (nStreamMask & VSM_MASK)
		{
			int i;
			uint32 iMask = 1;

			for (i = 1; i < VSF_NUM; i++)
			{
				iMask = iMask << 1;

				pMS = pVContainer->GetVertexStream(i, 0);
				if ((nStreamMask & iMask) && pMS)
				{
					if (pMS->m_pUpdateData && pMS->m_nFrameAccess != nFrame)
					{
						pMS->m_nFrameAccess = nFrame;
						if (pMS->m_nFrameRequest > pMS->m_nFrameUpdate)
						{
							// Update the device buffer
							PROFILE_FRAME(Mesh_CheckUpdateUpdateGBuf);
							if (!(pMS->m_nLockFlags & FSL_WRITE))
							{
								if (!pVContainer->UpdateVidVertices(i))
								{
									RT_AllocationFailure("Update VB Stream", GetStreamSize(i, m_nVerts));
									return false;
								}
								pMS->m_nFrameUpdate = nFrame;
							}
							else if (i != VSF_HWSKIN_INFO)
							{
								int nnn = 0;
								if (pMS->m_nID == ~0u)
									return false;
							}
						}
					}
				}
			}
		}
	}//if (m_pVertexContainer || m_nVerts > 2)

	m_IBStream.m_nFrameAccess = nFrame;
	const bool bIndUpdateNeeded = (m_IBStream.m_pUpdateData != NULL) && (m_IBStream.m_nFrameRequest > m_IBStream.m_nFrameUpdate);
	if (bIndUpdateNeeded)
	{
		PROFILE_FRAME(Mesh_CheckUpdate_UpdateInds);
		if (!(pVContainer->m_IBStream.m_nLockFlags & FSL_WRITE))
		{
			if (!UpdateVidIndices(m_IBStream))
			{
				RT_AllocationFailure("Update IB Stream", m_nInds * sizeof(vtx_idx));
				return false;
			}
			m_IBStream.m_nFrameUpdate = nFrame;
		}
		else if (pVContainer->m_IBStream.m_nID == ~0u)
			return false;
	}

#ifdef MESH_TESSELLATION_RENDERER
	// Build UV adjacency
	if ((bTessellation && m_adjBuffer.GetElementCount() == 0)       // if needed and not built already
		|| (bIndUpdateNeeded && m_adjBuffer.GetElementCount() > 0)) // if already built but needs update
	{
		if (!(pVContainer->m_IBStream.m_nLockFlags & FSL_WRITE)
			&& (pVContainer->_HasVBStream(VSF_NORMALS)))
		{
			if (m_eVF == EDefaultInputLayouts::P3S_C4B_T2S)
			{
				UpdateUVCoordsAdjacency<SVF_P3S_C4B_T2S, Vec3f16, Vec2f16>(m_IBStream);
			}
			else if (m_eVF == EDefaultInputLayouts::P3F_C4B_T2F)
			{
				UpdateUVCoordsAdjacency<SVF_P3F_C4B_T2F, Vec3, Vec2>(m_IBStream);
			}
		}
	}
#endif

	return true;
}

void CRenderMesh::ReleaseVB(int nStream)
{
  UnlockVB(nStream);
  if (SMeshStream* pMS = GetVertexStream(nStream, 0))
  {
	  if (pMS->m_nID != ~0u)
    {
      gRenDev->m_DevBufMan.Destroy(pMS->m_nID);
      pMS->m_nID = ~0u;
    }
    pMS->m_nElements = 0;
    pMS->m_nFrameUpdate = -1;
	  pMS->m_nFrameCreate = -1;
  }
}

void CRenderMesh::ReleaseIB()
{
  UnlockIB();
	if (m_IBStream.m_nID != ~0u)
  {
    gRenDev->m_DevBufMan.Destroy(m_IBStream.m_nID);
    m_IBStream.m_nID = ~0u;
  }
  m_IBStream.m_nElements = 0;
  m_IBStream.m_nFrameUpdate = -1;
  m_IBStream.m_nFrameCreate = -1;
}

bool CRenderMesh::UpdateIndices_Int(
	const vtx_idx *pNewInds
	, int nInds
	, int nOffsInd
	, uint32 copyFlags)
{
	MEMSTAT_CONTEXT(EMemStatContextTypes::MSC_RenderMeshType, 0, this->GetTypeName());
	MEMSTAT_CONTEXT(EMemStatContextTypes::MSC_RenderMesh, 0, this->GetSourceName());

	//LockVB operates now on a per mesh lock, any thread may access
	//ASSERT_IS_MAIN_THREAD(gRenDev->m_pRT)

  //SREC_AUTO_LOCK(m_sResLock);

  // Resize the index buffer
  if (m_nInds != nInds)
  {
    FreeIB();
    m_nInds = nInds;
  }
  if (!nInds)
  {
    assert(!m_IBStream.m_pUpdateData);
    return true;
  }

  vtx_idx *pDst = LockIB(FSL_VIDEO_CREATE, 0, nInds);
  if (pDst && pNewInds)
  {
		if (copyFlags&FSL_ASYNC_DEFER_COPY && nInds*sizeof(vtx_idx)<RENDERMESH_ASYNC_MEMCPY_THRESHOLD)
			cryAsyncMemcpy(
				&pDst[nOffsInd],
				pNewInds,
				nInds*sizeof(vtx_idx),
				MC_CPU_TO_GPU|copyFlags,
				SetAsyncUpdateState());
		else
		{
			cryMemcpy(
				&pDst[nOffsInd],
				pNewInds,
				nInds*sizeof(vtx_idx),
				MC_CPU_TO_GPU);
			UnlockIndexStream();
		}
  }
  else
    return false;

  return true;
}

bool CRenderMesh::UpdateVertices_Int(
	const void *pVertBuffer
	, int nVertCount
	, int nOffset
	, int nStream
	, uint32 copyFlags)
{
	//LockVB operates now on a per mesh lock, any thread may access
//  ASSERT_IS_MAIN_THREAD(gRenDev->m_pRT)

  int nStride;

  //SREC_AUTO_LOCK(m_sResLock);

  // Resize the vertex buffer
  if (m_nVerts != nVertCount)
  {
    for (int i=0; i<VSF_NUM; i++)
    {
      FreeVB(i);
    }
    m_nVerts = nVertCount;
  }
  if (!m_nVerts)
    return true;

  byte *pDstVB = (byte *)LockVB(nStream, FSL_VIDEO_CREATE, 0, nVertCount, &nStride);
  assert((nVertCount == 0) || pDstVB);
  if (pDstVB && pVertBuffer)
  {
		if (copyFlags&FSL_ASYNC_DEFER_COPY && nStride*nVertCount<RENDERMESH_ASYNC_MEMCPY_THRESHOLD)
		{
			cryAsyncMemcpy(
				&pDstVB[nOffset],
				pVertBuffer,
				nStride*nVertCount,
				MC_CPU_TO_GPU|copyFlags,
				SetAsyncUpdateState());
		}
		else
		{
			cryMemcpy(
				&pDstVB[nOffset],
				pVertBuffer,
				nStride*nVertCount,
				MC_CPU_TO_GPU);
			UnlockStream(nStream);
		}
  }
  else
    return false;

  return true;
}

bool CRenderMesh::UpdateVertices(
	const void *pVertBuffer
	, int nVertCount
	, int nOffset
	, int nStream
	, uint32 copyFlags
	, bool requiresLock)
{
  bool result = false;
	if(requiresLock)
	{
		SREC_AUTO_LOCK(m_sResLock);
		result = UpdateVertices_Int(pVertBuffer, nVertCount, nOffset, nStream, copyFlags);
	}
	else
	{
		result = UpdateVertices_Int(pVertBuffer, nVertCount, nOffset, nStream, copyFlags);
	}
  return result;
}

bool CRenderMesh::UpdateIndices(
	const vtx_idx *pNewInds
	, int nInds
	, int nOffsInd
	, uint32 copyFlags
	, bool requiresLock)
{
  bool result = false;
	if(requiresLock)
	{
		SREC_AUTO_LOCK(m_sResLock);
		result = UpdateIndices_Int(pNewInds, nInds, nOffsInd, copyFlags);
	}
	else
	{
		result = UpdateIndices_Int(pNewInds, nInds, nOffsInd, copyFlags);
	}
  return result;
}

bool CRenderMesh::UpdateVidIndices(SMeshStream& IBStream, bool stall)
{
	MEMSTAT_CONTEXT(EMemStatContextTypes::MSC_RenderMeshType, 0, this->GetTypeName());
	MEMSTAT_CONTEXT(EMemStatContextTypes::MSC_RenderMesh, 0, this->GetSourceName());
	SCOPED_RENDERER_ALLOCATION_NAME_HINT( GetSourceName() );

  assert(gRenDev->m_pRT->IsRenderThread());

  SREC_AUTO_LOCK(m_sResLock);

  assert(gRenDev->m_pRT->IsRenderThread());

  int nInds = m_nInds;

  if (!nInds)
	{
		// 0 size index buffer creation crashes on 360
		assert( nInds );
		return false;
	}

  if (IBStream.m_nElements != m_nInds && _HasIBStream())
    ReleaseIB();

  if (IBStream.m_nID == ~0u)
  {
    IBStream.m_nID = gRenDev->m_DevBufMan.Create(BBT_INDEX_BUFFER, (BUFFER_USAGE)m_eType, nInds * sizeof(vtx_idx));
    IBStream.m_nElements = m_nInds;
		IBStream.m_nFrameCreate = GetCurrentFrameID();
  }
  if (IBStream.m_nID != ~0u)
  {
    UnlockIndexStream();
    if (IBStream.m_pUpdateData)
    {
      bool bRes = true;
      return gRenDev->m_DevBufMan.UpdateBuffer(IBStream.m_nID, IBStream.m_pUpdateData, AlignedMeshDataSize(m_nInds * sizeof(vtx_idx)));
    }
  }
  return false;
}

bool CRenderMesh::CreateVidVertices(int nVerts, InputLayoutHandle eVF, int nStream)
{
	MEMSTAT_CONTEXT(EMemStatContextTypes::MSC_RenderMeshType, 0, this->GetTypeName());
	MEMSTAT_CONTEXT(EMemStatContextTypes::MSC_RenderMesh, 0, this->GetSourceName());
	SCOPED_RENDERER_ALLOCATION_NAME_HINT( GetSourceName() );

	SREC_AUTO_LOCK(m_sResLock);
	CRenderer* pRend = gRenDev;

	if (gRenDev->m_bDeviceLost)
    return false;
  if (!nVerts && eVF==InputLayoutHandle::Unspecified)
  {
    nVerts = m_nVerts;
    eVF = m_eVF;
  }

  assert (!_HasVBStream(nStream));
  SMeshStream* pMS = GetVertexStream(nStream, FSL_WRITE);
  int nSize = GetStreamSize(nStream, nVerts);
  pMS->m_nID = gRenDev->m_DevBufMan.Create(BBT_VERTEX_BUFFER, (BUFFER_USAGE)m_eType, nSize);
  pMS->m_nElements = nVerts;
	pMS->m_nFrameCreate = GetCurrentFrameID();
  return (pMS->m_nID != ~0u);
}

bool CRenderMesh::UpdateVidVertices(int nStream)
{
	MEMSTAT_CONTEXT(EMemStatContextTypes::MSC_RenderMeshType, 0, this->GetTypeName());
	MEMSTAT_CONTEXT(EMemStatContextTypes::MSC_RenderMesh, 0, this->GetSourceName());

  assert(gRenDev->m_pRT->IsRenderThread());

  SREC_AUTO_LOCK(m_sResLock);

  assert(nStream < VSF_NUM);
  SMeshStream* pMS = GetVertexStream(nStream, FSL_WRITE);

  if (m_nVerts != pMS->m_nElements && _HasVBStream(nStream))
    ReleaseVB(nStream);

  if (pMS->m_nID==~0u)
  {
		if (!CreateVidVertices(m_nVerts, m_eVF, nStream))
			return false;
  }
	if (pMS->m_nID!=~0u)
  {
		UnlockStream(nStream);
    if (pMS->m_pUpdateData)
    {
			return gRenDev->m_DevBufMan.UpdateBuffer(pMS->m_nID, pMS->m_pUpdateData, AlignedMeshDataSize(GetStreamSize(nStream)));
    }
    else
    {
      assert(0);
    }
	}
	return false;
}

#ifdef MESH_TESSELLATION_RENDERER
template<class VertexFormat, class VecPos, class VecUV> bool CRenderMesh::UpdateUVCoordsAdjacency(SMeshStream& IBStream)
{
	MEMSTAT_CONTEXT(EMemStatContextTypes::MSC_RenderMeshType, 0, this->GetTypeName());
	MEMSTAT_CONTEXT(EMemStatContextTypes::MSC_RenderMesh, 0, this->GetSourceName());
	SCOPED_RENDERER_ALLOCATION_NAME_HINT( GetSourceName() );

	assert(gRenDev->m_pRT->IsRenderThread());

	SREC_AUTO_LOCK(m_sResLock);

	int nInds = m_nInds * 4;

  if (!nInds)
	{
		// 0 size index buffer creation crashes on 360
		assert( nInds );
		return false;
	}

  SMeshStream* pMS = GetVertexStream(VSF_GENERAL);

	if (IBStream.m_nID != ~0u && pMS)
	{
		if (m_IBStream.m_pUpdateData)
		{
			bool bRes = true;
			std::vector<VecUV> pTxtAdjBuffer;

			// create triangles with adjacency
			VertexFormat *pVertexStream = (VertexFormat *)pMS->m_pUpdateData;
			if ((m_eVF == EDefaultInputLayouts::P3S_C4B_T2S || m_eVF == EDefaultInputLayouts::P3F_C4B_T2F) && pVertexStream)
			{
				int nTrgs = m_nInds / 3;
				pTxtAdjBuffer.resize(nTrgs * 12);
				int nVerts = _GetNumVerts();
				for (int n = 0; n < nTrgs; ++n)
				{
					// fill in the dummy adjacency first
					VecUV *pDst = &pTxtAdjBuffer[n * 12];
					vtx_idx *pSrc = (vtx_idx *)m_IBStream.m_pUpdateData + n * 3;
					// triangle itself
					pDst[0] = pVertexStream[pSrc[0]].st;
					pDst[1] = pVertexStream[pSrc[1]].st;
					pDst[2] = pVertexStream[pSrc[2]].st;
					// adjacency by edges
					pDst[3] = pVertexStream[pSrc[0]].st;
					pDst[4] = pVertexStream[pSrc[1]].st;
					pDst[5] = pVertexStream[pSrc[1]].st;
					pDst[6] = pVertexStream[pSrc[2]].st;
					pDst[7] = pVertexStream[pSrc[2]].st;
					pDst[8] = pVertexStream[pSrc[0]].st;
					// adjacency by corners
					pDst[9] = pVertexStream[pSrc[0]].st;
					pDst[10] = pVertexStream[pSrc[1]].st;
					pDst[11] = pVertexStream[pSrc[2]].st;
				}

				// now real adjacency is computed
				BuildAdjacency<VertexFormat, VecPos, VecUV>(pVertexStream, nVerts, (vtx_idx *)m_IBStream.m_pUpdateData, nTrgs, pTxtAdjBuffer);

				if (sizeof(VecUV) == sizeof(Vec2f16))
				{
					m_adjBuffer.Create(pTxtAdjBuffer.size(), sizeof(VecUV), DXGI_FORMAT_R16G16_FLOAT, CDeviceObjectFactory::BIND_SHADER_RESOURCE, &pTxtAdjBuffer[0]);
				}
				else
				{
					m_adjBuffer.Create(pTxtAdjBuffer.size(), sizeof(VecUV), DXGI_FORMAT_R32G32_FLOAT, CDeviceObjectFactory::BIND_SHADER_RESOURCE, &pTxtAdjBuffer[0]);
				}				

				for (int iChunk = 0; iChunk < m_Chunks.size(); ++iChunk)
				{
					((CREMeshImpl*) m_Chunks[iChunk].pRE)->m_nPatchIDOffset = m_Chunks[iChunk].nFirstIndexId / 3;
				}
			}

			return bRes;
		}
	}

	return false;
}
#endif //#ifdef MESH_TESSELLATION_RENDERER

void CRenderMesh::SetREUserData(float *pfCustomData, float fFogScale, float fAlpha)
{
  for (int i=0; i<m_Chunks.size(); i++)
  {
    if(m_Chunks[i].pRE)
    {
      m_Chunks[i].pRE->m_CustomData = pfCustomData;
    }
  }
}

void CRenderMesh::AddRenderElements(IMaterial *pIMatInfo, CRenderObject *pObj, const SRenderingPassInfo &passInfo, int nList, int nAW)
{
  assert(!(pObj->m_ObjFlags & FOB_BENDED));
  //assert (!pObj->GetInstanceInfo(0));
  //assert(pIMatInfo);

  if (!_GetVertexContainer()->m_nVerts || !m_Chunks.size() || !pIMatInfo)
    return;

	if(gRenDev->m_pDefaultMaterial && gRenDev->m_pTerrainDefaultMaterial)
	{
		IShader* pShader = pIMatInfo->GetShaderItem().m_pShader;
		bool bIsTerrainShader = pShader ? (pShader->GetShaderType() == eST_Terrain) : false;

		if((nList == EFSLIST_TERRAINLAYER || bIsTerrainShader) && pObj->GetMatrix(passInfo).GetTranslation().GetLength()>1)
		{
			pIMatInfo = gRenDev->m_pTerrainDefaultMaterial;

			// modify m_CustomTexBind settings to overwrite terrain textures.
			SEfResTexture* pResDiffTex = pIMatInfo->GetShaderItem().m_pShaderResources->GetTexture(EFTT_DIFFUSE);
			if(pResDiffTex)
			{
				ITexture* pTexture = pResDiffTex->m_Sampler.m_pITex;
				if(pTexture)
				{
					m_Chunks[0].pRE->m_CustomTexBind[0] = pTexture->GetTextureID();
					m_Chunks[0].pRE->m_CustomTexBind[1] = 0;
				}
			}
		} 
		else
		{
			pIMatInfo = gRenDev->m_pDefaultMaterial;
		}
	}

  for (int i=0; i<m_Chunks.size(); i++)
  {
    CRenderChunk * pChunk = &m_Chunks[i];
    CREMeshImpl* pOrigRE = (CREMeshImpl*) pChunk->pRE;

    // get material

    SShaderItem& shaderItem = pIMatInfo->GetShaderItem(pChunk->m_nMatID);

//    if (nTechniqueID > 0)
  //    shaderItem.m_nTechnique = shaderItem.m_pShader->GetTechniqueID(shaderItem.m_nTechnique, nTechniqueID);

	if (shaderItem.m_pShader && pOrigRE)// && pMat->nNumIndices)
    {
		 TArray<CRenderElement *> *pREs = shaderItem.m_pShader->GetREs(shaderItem.m_nTechnique);

		 if (!pREs || !pREs->Num())
			passInfo.GetRenderView()->AddRenderObject(pOrigRE, shaderItem, pObj, passInfo, nList, nAW);
		 else
			passInfo.GetRenderView()->AddRenderObject(pREs->Get(0), shaderItem, pObj, passInfo, nList, nAW);
    }
  } //i
}

void CRenderMesh::AddRE(IMaterial* pMaterial, CRenderObject* obj, IShader* ef, const SRenderingPassInfo& passInfo, int nList, int nAW)
{
  if (!m_nVerts || !m_Chunks.size())
    return;

  assert(!(obj->m_ObjFlags & FOB_BENDED));

  for(int i=0; i<m_Chunks.size(); i++)
  {
    if (!m_Chunks[i].pRE)
      continue;

    SShaderItem& SH = pMaterial->GetShaderItem();
    if (ef)
      SH.m_pShader = ef;
    if (SH.m_pShader)
    {
      TArray<CRenderElement *> *pRE = SH.m_pShader->GetREs(SH.m_nTechnique);
      if (!pRE || !pRE->Num())
				passInfo.GetRenderView()->AddRenderObject(m_Chunks[i].pRE, SH, obj, passInfo, nList, nAW);
      else
				passInfo.GetRenderView()->AddRenderObject(SH.m_pShader->GetREs(SH.m_nTechnique)->Get(0), SH, obj, passInfo, nList, nAW);
    }
  }
}

size_t CRenderMesh::GetMemoryUsage( ICrySizer* pSizer, EMemoryUsageArgument nType ) const
{
	size_t nSize = 0;
	switch (nType)
	{
	case MEM_USAGE_COMBINED:
		nSize = Size(SIZE_ONLY_SYSTEM) + Size(SIZE_VB|SIZE_IB);
		break;
	case MEM_USAGE_ONLY_SYSTEM:
		nSize = Size(SIZE_ONLY_SYSTEM);
		break;
	case MEM_USAGE_ONLY_VIDEO:
		nSize = Size(SIZE_VB|SIZE_IB);
		return nSize;
		break;
	case MEM_USAGE_ONLY_STREAMS:
		nSize = Size(SIZE_ONLY_SYSTEM) + Size(SIZE_VB|SIZE_IB);

		if (pSizer)
		{
			SIZER_COMPONENT_NAME(pSizer, "STREAM MESH");
			pSizer->AddObject((void *)this, nSize);
		}

		// Not add overhead allocations.
		return nSize;
		break;
	}

	{
		nSize += sizeof(*this);
		for (int i=0; i<(int)m_Chunks.capacity(); i++)
		{
			if (i < m_Chunks.size())
				nSize += m_Chunks[i].Size();
			else
				nSize += sizeof(CRenderChunk);
		}
		for (int i=0; i<(int)m_ChunksSkinned.capacity(); i++)
		{
			if (i < m_ChunksSkinned.size())
				nSize += m_ChunksSkinned.at(i).Size();
			else
				nSize += sizeof(CRenderChunk);
		}
	}

	if (pSizer)
	{
		pSizer->AddObject((void *)this, nSize);

#ifdef RENDER_MESH_TRIANGLE_HASH_MAP_SUPPORT
		if(m_pTrisMap)
		{
			SIZER_COMPONENT_NAME(pSizer, "Hash map");
			nSize += stl::size_of_map(*m_pTrisMap);
		}
#endif

		for (MeshSubSetIndices::const_iterator it = m_meshSubSetIndices.begin(); it != m_meshSubSetIndices.end(); ++it)
		{
			// Collect memory usage for index sub-meshes.
			it->second->GetMemoryUsage(pSizer,nType);
		}
	}

	return nSize;
}

void CRenderMesh::GetMemoryUsage( ICrySizer* pSizer ) const
{
	pSizer->AddObject( this, sizeof(*this) );
	{
		SIZER_COMPONENT_NAME(pSizer, "Vertex Data");
		for (uint32 i=0; i<VSF_NUM; i++)
		{
			if (SMeshStream* pMS = GetVertexStream(i, 0))
			{
				if (pMS->m_pUpdateData)
					pSizer->AddObject( pMS->m_pUpdateData, GetStreamSize(i));
			}
		}
	}

	{
		SIZER_COMPONENT_NAME(pSizer, "FP16 Cache");
		if (m_pCachePos)
			pSizer->AddObject(m_pCachePos, m_nVerts * sizeof(Vec3));
		if (m_pCacheUVs)
			pSizer->AddObject(m_pCacheUVs, m_nVerts * sizeof(Vec2));
	}

	{
		SIZER_COMPONENT_NAME(pSizer, "Mesh Chunks");
		pSizer->AddObject( m_Chunks );
	}
	{
		SIZER_COMPONENT_NAME(pSizer, "Mesh Skinned Chunks");
		pSizer->AddObject( m_ChunksSkinned );
	}

#ifdef RENDER_MESH_TRIANGLE_HASH_MAP_SUPPORT
	{
		SIZER_COMPONENT_NAME(pSizer, "Hash map");
		pSizer->AddObject( m_pTrisMap );
	}
#endif
	for (MeshSubSetIndices::const_iterator it = m_meshSubSetIndices.begin(); it != m_meshSubSetIndices.end(); ++it)
	{
		// Collect memory usage for index sub-meshes.
		it->second->GetMemoryUsage(pSizer);
	}
}

int CRenderMesh::GetAllocatedBytes( bool bVideoMem ) const
{
  if (!bVideoMem)
    return Size(SIZE_ONLY_SYSTEM);
  else
    return Size(SIZE_VB|SIZE_IB);
}

int CRenderMesh::GetTextureMemoryUsage( const IMaterial* pMaterial, ICrySizer* pSizer, bool bStreamedIn ) const
{
  // If no input material use internal render mesh material.
  if (!pMaterial)
    return 0;

  int textureSize = 0;
  std::set<const CTexture*> used;
  for (int a=0; a<m_Chunks.size(); a++)
  {
    const CRenderChunk * pChunk = &m_Chunks[a];

    // Override default material
    const SShaderItem& shaderItem = pMaterial->GetShaderItem(pChunk->m_nMatID);
    if (!shaderItem.m_pShaderResources)
      continue;

		const CShaderResources* pRes = ( const CShaderResources*)shaderItem.m_pShaderResources;

    for (int i=0; i<EFTT_MAX; i++)
    {
      if (!pRes->m_Textures[i])
        continue;

      const CTexture *pTexture = pRes->m_Textures[i]->m_Sampler.m_pTex;
      if (!pTexture)
        continue;

      if (used.find(pTexture) != used.end()) // Already used in size calculation.
        continue;
      used.insert(pTexture);

			int nTexSize = bStreamedIn ? pTexture->GetDeviceDataSize() : pTexture->GetDataSize();
      textureSize += nTexSize;

      if (pSizer)
        pSizer->AddObject(pTexture, nTexSize);
    }
  }

  return textureSize;
}

float CRenderMesh::GetAverageTrisNumPerChunk(IMaterial * pMat)
{
  float fTrisNum = 0;
  float fChunksNum = 0;

  for(int m=0; m<m_Chunks.size(); m++)
  {
    const CRenderChunk& chunk = m_Chunks[m];
    if ((chunk.m_nMatFlags & MTL_FLAG_NODRAW) || !chunk.pRE)
      continue;

    const IMaterial *pCustMat;
    if (pMat && chunk.m_nMatID < pMat->GetSubMtlCount())
      pCustMat = pMat->GetSubMtl(chunk.m_nMatID);
    else
      pCustMat = pMat;

    if(!pCustMat)
      continue;

    const IShader * pShader = pCustMat->GetShaderItem().m_pShader;

    if(!pShader)
      continue;

    if (pShader->GetFlags2() & EF2_NODRAW)
      continue;

    fTrisNum += chunk.nNumIndices/3;
    fChunksNum++;
  }

  return fChunksNum ? (fTrisNum/fChunksNum) : 0;
}

void CRenderMesh::InitTriHash(IMaterial * pMaterial)
{
#ifdef RENDER_MESH_TRIANGLE_HASH_MAP_SUPPORT

  SAFE_DELETE(m_pTrisMap);
  m_pTrisMap = new TrisMap;

  int nPosStride=0;
  int nIndCount = m_nInds;
  const byte * pPositions = GetPosPtr(nPosStride, FSL_READ);
  const vtx_idx * pIndices = GetIndexPtr(FSL_READ);

  iLog->Log("CRenderMesh::InitTriHash: Tris=%d, Verts=%d, Name=%s ...", nIndCount/3, GetVerticesCount(), GetSourceName() ? GetSourceName() : "Null");

  if(pIndices && pPositions && m_Chunks.size() && nIndCount && GetVerticesCount())
  {
    for (uint32 ii=0; ii<(uint32)m_Chunks.size(); ii++)
    {
      CRenderChunk *pChunk = &m_Chunks[ii];

      if (pChunk->m_nMatFlags & MTL_FLAG_NODRAW || !pChunk->pRE)
        continue;

      // skip transparent and alpha test
      const SShaderItem &shaderItem = pMaterial->GetShaderItem(pChunk->m_nMatID);
      if (!shaderItem.IsZWrite() || !shaderItem.m_pShaderResources || shaderItem.m_pShaderResources->IsAlphaTested())
        continue;

      if(shaderItem.m_pShader && shaderItem.m_pShader->GetFlags()&EF_DECAL)
        continue;

      uint32 nFirstIndex = pChunk->nFirstIndexId;
      uint32 nLastIndex = pChunk->nFirstIndexId + pChunk->nNumIndices;

      for (uint32 i = nFirstIndex; i < nLastIndex; i+=3)
      {
        int32 I0	=	pIndices[i+0];
        int32 I1	=	pIndices[i+1];
        int32 I2	=	pIndices[i+2];

        Vec3 v0 = *(Vec3*)&pPositions[nPosStride*I0];
        Vec3 v1 = *(Vec3*)&pPositions[nPosStride*I1];
        Vec3 v2 = *(Vec3*)&pPositions[nPosStride*I2];

        AABB triBox;
        triBox.min = triBox.max = v0;
        triBox.Add(v1);
        triBox.Add(v2);

				float fRayLen = CRenderer::CV_r_RenderMeshHashGridUnitSize/2;
        triBox.min -= Vec3(fRayLen,fRayLen,fRayLen);
        triBox.max += Vec3(fRayLen,fRayLen,fRayLen);

        AABB aabbCell;

        aabbCell.min = triBox.min / CRenderer::CV_r_RenderMeshHashGridUnitSize;
        aabbCell.min.x = floor(aabbCell.min.x);
        aabbCell.min.y = floor(aabbCell.min.y);
        aabbCell.min.z = floor(aabbCell.min.z);

        aabbCell.max = triBox.max / CRenderer::CV_r_RenderMeshHashGridUnitSize;
        aabbCell.max.x = ceil(aabbCell.max.x);
        aabbCell.max.y = ceil(aabbCell.max.y);
        aabbCell.max.z = ceil(aabbCell.max.z);

        for(float x=aabbCell.min.x; x<aabbCell.max.x; x++)
        {
          for(float y=aabbCell.min.y; y<aabbCell.max.y; y++)
          {
            for(float z=aabbCell.min.z; z<aabbCell.max.z; z++)
            {
              AABB cellBox;
              cellBox.min = Vec3(x,y,z)*CRenderer::CV_r_RenderMeshHashGridUnitSize;
              cellBox.max = cellBox.min + Vec3(CRenderer::CV_r_RenderMeshHashGridUnitSize,CRenderer::CV_r_RenderMeshHashGridUnitSize,CRenderer::CV_r_RenderMeshHashGridUnitSize);
              cellBox.min -= Vec3(fRayLen,fRayLen,fRayLen);
              cellBox.max += Vec3(fRayLen,fRayLen,fRayLen);
              if(!Overlap::AABB_Triangle(cellBox, v0, v1, v2))
                continue;

              int key = (int)(x*256.f*256.f + y*256.f + z);
              PodArray<std::pair<int,int> > * pTris = &(*m_pTrisMap)[key];
              std::pair<int,int> t(i,pChunk->m_nMatID);
              if(pTris->Find(t)<0)
                pTris->Add(t);
            }
          }
        }
      }
    }
  }

  iLog->LogPlus(" ok (%" PRISIZE_T ")", m_pTrisMap->size());

#endif
}

const PodArray<std::pair<int,int> > * CRenderMesh::GetTrisForPosition(const Vec3 & vPos, IMaterial * pMaterial)
{
#ifdef RENDER_MESH_TRIANGLE_HASH_MAP_SUPPORT

  if(!m_pTrisMap)
	{
		AUTO_LOCK(m_getTrisForPositionLock);
		if(!m_pTrisMap)
	    InitTriHash(pMaterial);
	}

  Vec3 vCellMin = vPos / CRenderer::CV_r_RenderMeshHashGridUnitSize;
  vCellMin.x = floor(vCellMin.x);
  vCellMin.y = floor(vCellMin.y);
  vCellMin.z = floor(vCellMin.z);

  int key = (int)(vCellMin.x*256.f*256.f + vCellMin.y*256.f + vCellMin.z);

  const TrisMap::iterator & iter = (*m_pTrisMap).find(key);
  if(iter != (*m_pTrisMap).end())
    return &iter->second;

#endif

  return 0;
}

void CRenderMesh::UpdateBBoxFromMesh()
{
  PROFILE_FRAME(UpdateBBoxFromMesh);

  AABB aabb;
  aabb.Reset();

  int nVertCount = _GetVertexContainer()->GetVerticesCount();
  int nPosStride=0;
  int nIndCount = GetIndicesCount();
  const byte * pPositions = GetPosPtr(nPosStride, FSL_READ);
  const vtx_idx * pIndices = GetIndexPtr(FSL_READ);

  if(!pIndices || !pPositions)
  {
    //assert(!"Mesh is not ready");
    return;
  }

  for (int32 a=0; a<m_Chunks.size(); a++)
  {
    CRenderChunk * pChunk = &m_Chunks[a];

    if(pChunk->m_nMatFlags & MTL_FLAG_NODRAW || !pChunk->pRE)
      continue;

    uint32 nFirstIndex = pChunk->nFirstIndexId;
    uint32 nLastIndex = pChunk->nFirstIndexId + pChunk->nNumIndices;

    for (uint32 i = nFirstIndex; i < nLastIndex; i++)
    {
      int32 I0	=	pIndices[i];
      if(I0 < nVertCount)
      {
        Vec3 v0 = *(Vec3*)&pPositions[nPosStride*I0];
        aabb.Add(v0);
      }
      else
        assert(!"Index is out of range");
    }
  }

  if (!aabb.IsReset())
  {
    m_vBoxMax = aabb.max;
    m_vBoxMin = aabb.min;
  }
}

static void BlendWeights(DualQuat& dq, const DualQuat aBoneLocs[], const JointIdType aBoneRemap[], const uint16 indices[], UCol weights)
{
	// Get 8-bit weights as floats
	f32 w0 = weights.bcolor[0];
	f32 w1 = weights.bcolor[1];
	f32 w2 = weights.bcolor[2];
	f32 w3 = weights.bcolor[3];

	// Get bone indices
	uint16 auBones[4];
	if (aBoneRemap)
	{
		auBones[0] = aBoneRemap[indices[0]];
		auBones[1] = aBoneRemap[indices[1]];
		auBones[2] = aBoneRemap[indices[2]];
		auBones[3] = aBoneRemap[indices[3]];
	}
	else
	{
		auBones[0] = indices[0];
		auBones[1] = indices[1];
		auBones[2] = indices[2];
		auBones[3] = indices[3];
	}

	// Blend dual quats
	dq = aBoneLocs[auBones[0]]*w0 + aBoneLocs[auBones[1]]*w1 + aBoneLocs[auBones[2]]*w2 + aBoneLocs[auBones[3]]*w3;

	dq.Normalize();
}

// To do: replace with VSF_MORPHBUDDY support
#define SKIN_MORPHING 0

struct PosNormData
{
	Array<PosNorm> aPosNorms;
	// Skinning data
	SSkinningData const* pSkinningData = 0;
#if SKIN_MORPHING
	strided_pointer<SVF_P3F_P3F_I4B> aMorphing;
#endif
	strided_pointer<SVF_W4B_I4S> aSkinning;

	ILINE void GetPosNorm(PosNorm& ran, int nV)
	{
		ran = aPosNorms[nV];
	#if SKIN_MORPHING
		if (aShapeDeform && aMorphing)
		{
			SVF_P3F_P3F_I4B const& morph = aMorphing[nV];
			uint8 idx = (uint8)morph.index.dcolor;
			float fDeform = aShapeDeform[idx];
			if (fDeform < 0.0f)
				ran.vPos = morph.thin*(-fDeform) + ran.vPos*(fDeform+1);
			else if (fDeform > 0.f)
				ran.vPos = ran.vPos*(1-fDeform) + morph.fat*fDeform;
		}

		/*if (!g_arrExtMorphStream.empty())
			ran.vPos += g_arrExtMorphStream[nV];*/
	#endif

		// Skinning
		if (pSkinningData)
		{
			// Transform the vertex with skinning.
			DualQuat dq;
			BlendWeights(
				dq,
				pSkinningData->pBoneQuatsS,
				pSkinningData->pRemapTable, 
				aSkinning[nV].indices,
				aSkinning[nV].weights);
			ran <<= dq;
		}
	}
};

float CRenderMesh::GetExtent(EGeomForm eForm)
{
	CGeomExtent& ext = m_Extents.Make(eForm);
	if (!ext)
	{
		SScopedMeshDataLock _lock(this);

		// Possible vertex data streams
		vtx_idx*                       pInds = GetIndexPtr(FSL_READ);
		strided_pointer<Vec3>          aPos;
		strided_pointer<Vec3f16>       aPosH;
		strided_pointer<SPipNormal>    aNorm;
		strided_pointer<UCol>          aNormB;
		strided_pointer<SPipQTangents> aQTan;
		strided_pointer<SPipTangents>  aTan2;

		// Check position streams
		if (m_eVF == EDefaultInputLayouts::P3S_C4B_T2S || m_eVF == EDefaultInputLayouts::P3S_N4B_C4B_T2S)
			GetStridedArray(aPosH, VSF_GENERAL, SInputLayout::eOffset_Position);
		else
			GetStridedArray(aPos, VSF_GENERAL, SInputLayout::eOffset_Position);

		// Check normal streams
		bool bNormals = GetStridedArray(aNormB, VSF_GENERAL, SInputLayout::eOffset_Normal)
		             || GetStridedArray(aNorm, VSF_NORMALS)
		             || GetStridedArray(aQTan, VSF_QTANGENTS)
		             || GetStridedArray(aTan2, VSF_TANGENTS);

		if (pInds && (aPos || aPosH) && bNormals)
		{
			// Cache decompressed vertex data
			m_PosNorms.resize(m_nVerts);
			if (aPos)
			{
				for (int n = 0; n < m_nVerts; ++n)
					m_PosNorms[n].vPos = aPos[n];
			}
			else if (aPosH)
			{
				for (int n = 0; n < m_nVerts; ++n)
					m_PosNorms[n].vPos = aPosH[n].ToVec3();
			}

			if (aNorm)
			{
				for (int n = 0; n < m_nVerts; ++n)
					m_PosNorms[n].vNorm = aNorm[n].GetN();
			}
			else if (aNormB)
			{
				for (int n = 0; n < m_nVerts; ++n)
					m_PosNorms[n].vNorm = aNormB[n].GetN();
			}
			else if (aQTan)
			{
				for (int n = 0; n < m_nVerts; ++n)
					m_PosNorms[n].vNorm = aQTan[n].GetN();
			}
			else if (aTan2)
			{
				for (int n = 0; n < m_nVerts; ++n)
					m_PosNorms[n].vNorm = aTan2[n].GetN();
			}

			// Iterate chunks to track renderable verts
			bool* aValidVerts = new bool[m_nVerts];
			memset(aValidVerts, 0, m_nVerts);
			TRenderChunkArray& aChunks = !m_ChunksSkinned.empty() ? m_ChunksSkinned : m_Chunks;
			for (int c = 0; c < aChunks.size(); ++c)
			{
				const CRenderChunk& chunk = aChunks[c];
				if (chunk.pRE && !(chunk.m_nMatFlags & (MTL_FLAG_NODRAW | MTL_FLAG_REQUIRE_FORWARD_RENDERING)))
				{
					assert(uint32(chunk.nFirstVertId + chunk.nNumVerts) <= m_nVerts);
					memset(aValidVerts + chunk.nFirstVertId, 1, chunk.nNumVerts);
				}
			}

			int nParts = TriMeshPartCount(eForm, GetIndicesCount());
			ext.ReserveParts(nParts);
			Vec3 vCenter = (m_vBoxMin + m_vBoxMax) * 0.5f;
			for (int i=0; i<nParts ; i++)
			{
				int aIndices[3];
				Vec3 aVec[3];
				for (int v = TriIndices(aIndices, i, eForm)-1; v >= 0; v--)
					aVec[v] = m_PosNorms[ pInds[aIndices[v]] ].vPos;
				ext.AddPart( aValidVerts[ pInds[aIndices[0]] ] ? max(TriExtent(eForm, aVec, vCenter), 0.f) : 0.f );
			}
			delete[] aValidVerts;
		}

		UnlockStream(VSF_GENERAL);
		UnlockStream(VSF_NORMALS);
		UnlockStream(VSF_QTANGENTS);
		UnlockStream(VSF_TANGENTS);
		UnlockIndexStream();
	}
	return ext.TotalExtent();
}

void CRenderMesh::GetRandomPoints(Array<PosNorm> points, CRndGen& seed, EGeomForm eForm, SSkinningData const* pSkinning)
{
	CRY_PROFILE_FUNCTION(PROFILE_RENDERER);
	SScopedMeshDataLock _lock(this);

	vtx_idx* pInds = GetIndexPtr(FSL_READ);
	if (pInds && m_PosNorms.size())
	{
		PosNormData vdata;
		vdata.aPosNorms = m_PosNorms;
		if (vdata.pSkinningData = pSkinning)
		{
			GetStridedArray(vdata.aSkinning, VSF_HWSKIN_INFO);
		#if SKIN_MORPHING
			GetStridedArray(vdata.aMorphing, VSF_HWSKIN_SHAPEDEFORM_INFO);
		#endif
		}

		// Choose part.
		if (eForm == GeomForm_Vertices)
		{
			if (m_nInds == 0)
				return points.fill(ZERO);
			else
			{
				for (auto& ran : points)
				{
					int nIndex = seed.GetRandom(0U, m_nInds - 1);
					vdata.GetPosNorm(ran, pInds[nIndex]);
				}
			}
		}
		else
		{
			CGeomExtent const& extent = m_Extents[eForm];
			if (!extent.NumParts())
				return points.fill(ZERO);
			else
			{
				Vec3 vCenter = (m_vBoxMin + m_vBoxMax) * 0.5f;
				for (auto& ran : points)
				{
					int nPart = extent.RandomPart(seed);
					int aIndices[3];
					int nVerts = TriIndices(aIndices, nPart, eForm);

					// Extract vertices, blend.
					PosNorm aRan[3];
					while (--nVerts >= 0)
						vdata.GetPosNorm(aRan[nVerts], pInds[aIndices[nVerts]]);
					TriRandomPos(ran, seed, eForm, aRan, vCenter, true);

					// Temporary fix for bad skinning data
					if (pSkinning)
					{
						if (!IsValid(ran.vPos) || AABB(m_vBoxMin, m_vBoxMax).GetDistanceSqr(ran.vPos) > 100)
							ran.vPos = vCenter;
						if (!IsValid(ran.vNorm))
							ran.vNorm = Vec3(0, 0, 1);
					}
					else
					{
						assert(IsValid(ran.vPos) && IsValid(ran.vNorm));
						assert(AABB(m_vBoxMin, m_vBoxMax).GetDistanceSqr(ran.vPos) < 100);
					}
				}
			}
		}
	}

	UnlockIndexStream();
	UnlockStream(VSF_HWSKIN_INFO);
	#if SKIN_MORPHING
		UnlockStream(VSF_HWSKIN_SHAPEDEFORM_INFO);
	#endif
}

int CRenderChunk::Size() const
{
	size_t nSize = sizeof(*this);
	return static_cast<int>(nSize);
}

void CRenderMesh::Size( uint32 nFlags, ICrySizer* pSizer ) const
{
	uint32 i;
	if (!nFlags)  // System size
	{
		for (i=0; i<VSF_NUM; i++)
		{
      if (SMeshStream* pMS = GetVertexStream(i))
      {
			  if (pMS->m_pUpdateData)
				  pSizer->AddObject(pMS->m_pUpdateData,GetStreamSize(i) );
      }
		}
		if (m_IBStream.m_pUpdateData)
			pSizer->AddObject( m_IBStream.m_pUpdateData, m_nInds * sizeof(vtx_idx) );

		if (m_pCachePos)
			pSizer->AddObject(m_pCachePos, m_nVerts * sizeof(Vec3) );
	}

}

buffer_size_t CRenderMesh::Size(uint32 nFlags) const
{
	buffer_size_t nSize = 0;
  uint32 i;
	if (nFlags == SIZE_ONLY_SYSTEM)  // System size
  {
    for (i=0; i<VSF_NUM; i++)
    {
      if (SMeshStream* pMS = GetVertexStream(i))
      {
        if (pMS->m_pUpdateData)
          nSize += GetStreamSize(i);
      }
    }
    if (m_IBStream.m_pUpdateData)
      nSize += m_nInds * sizeof(vtx_idx);

    if (m_pCachePos)
      nSize += m_nVerts * sizeof(Vec3);
  }
	if (nFlags & SIZE_VB) // VB size
  {
    for (i=0; i<VSF_NUM; i++)
    {
      if (_HasVBStream(i))
        nSize += GetStreamSize(i);
    }
  }
	if (nFlags & SIZE_IB) // IB size
  {
    if (_HasIBStream())
      nSize += m_nInds * sizeof(vtx_idx);
  }

  return nSize;
}

void CRenderMesh::FreeDeviceBuffers(bool bRestoreSys)
{
	uint32 i;

	for (i = 0; i < VSF_NUM; i++)
	{
		if (_HasVBStream(i))
		{
			if (bRestoreSys)
			{
				SScopedMeshDataLock _lock(this);
				void *pSrc = LockVB(i, FSL_READ | FSL_VIDEO);
				void *pDst = LockVB(i, FSL_SYSTEM_CREATE);
				cryMemcpy(pDst, pSrc, GetStreamSize(i));
			}
			ReleaseVB(i);
		}
	}

	if (_HasIBStream())
	{
		if (bRestoreSys)
		{
			SScopedMeshDataLock _lock(this);
			void *pSrc = LockIB(FSL_READ | FSL_VIDEO);
			void *pDst = LockIB(FSL_SYSTEM_CREATE);
			cryMemcpy(pDst, pSrc, m_nInds * sizeof(vtx_idx));
		}
		ReleaseIB();
	}
}

void CRenderMesh::FreeVB(int nStream)
{
  if (SMeshStream* pMS = GetVertexStream(nStream))
  {
    if (pMS->m_pUpdateData)
    {
      FreeMeshData(pMS->m_pUpdateData);
      pMS->m_pUpdateData = NULL;
    }
  }
}

void CRenderMesh::FreeIB()
{
  if (m_IBStream.m_pUpdateData)
  {
    FreeMeshData(m_IBStream.m_pUpdateData);
    m_IBStream.m_pUpdateData = NULL;
  }
}

void CRenderMesh::FreeSystemBuffers()
{
  uint32 i;

  for (i=0; i<VSF_NUM; i++)
  {
    FreeVB(i);
  }
  FreeIB();

  FreeMeshData(m_pCachePos);
  m_pCachePos = NULL;
}

//////////////////////////////////////////////////////////////////////////
void CRenderMesh::DebugDraw( const struct SGeometryDebugDrawInfo &info,uint32 nVisibleChunksMask )
{
  MEMORY_SCOPE_CHECK_HEAP();
	IRenderAuxGeom *pRenderAuxGeom = gEnv->pRenderer->GetIRenderAuxGeom();
	SScopedMeshDataLock _lock(this);

	const Matrix34 &mat = info.tm;

	const bool bNoCull = info.bNoCull;
	const bool bNoLines = info.bNoLines;
	const bool bDrawInFront = info.bDrawInFront;

	SAuxGeomRenderFlags prevRenderFlags = pRenderAuxGeom->GetRenderFlags();
	SAuxGeomRenderFlags renderFlags = prevRenderFlags;
	renderFlags.SetAlphaBlendMode( e_AlphaBlended );

	int oldIndex = pRenderAuxGeom->PushMatrix(info.tm);

	if (bNoCull)
	{
		renderFlags.SetCullMode(e_CullModeNone);
	}
	if (bDrawInFront)
	{
		renderFlags.SetDrawInFrontMode(e_DrawInFrontOn);
	}
	pRenderAuxGeom->SetRenderFlags(renderFlags);

	const ColorB lineColor = info.lineColor;
	const ColorB color = info.color;

#if CRY_PLATFORM_WINDOWS 
	const unsigned int kMaxBatchSize = 20000;
	static std::vector<Vec3> s_vertexBuffer;
	static std::vector<vtx_idx> s_indexBuffer;
	s_vertexBuffer.reserve(kMaxBatchSize);
	s_indexBuffer.reserve(kMaxBatchSize * 2);
	s_vertexBuffer.resize(0);
	s_indexBuffer.resize(0);
	uint32 currentIndexBase = 0;
#endif

	const int32 chunkCount = m_Chunks.size();
	for (int32 currentChunkIndex = 0; currentChunkIndex < chunkCount; ++currentChunkIndex)
	{
		const CRenderChunk *pChunk = &m_Chunks[currentChunkIndex];

		if (pChunk->m_nMatFlags & MTL_FLAG_NODRAW || !pChunk->pRE)
		{
			continue;
		}

		if ( !( (1 << currentChunkIndex) & nVisibleChunksMask) )
		{
			continue;
		}

		int posStride = 1;
		const byte *pPositions = GetPosPtr(posStride, FSL_READ);
		const vtx_idx *pIndices = GetIndexPtr(FSL_READ);
		const uint32 numVertices = GetVerticesCount();
		const uint32 indexStep = 3;
		uint32 numIndices = pChunk->nNumIndices;

		// Make sure number of indices is a multiple of 3.
		uint32 indexRemainder = numIndices % indexStep;
		if (indexRemainder != 0)
		{
			// maybe assert instead?
			numIndices -= indexRemainder;
		}

		const uint32 firstIndex = pChunk->nFirstIndexId;
		const uint32 lastIndex = firstIndex + numIndices;

		for (uint32 i = firstIndex; i < lastIndex; i += indexStep)
		{
			int32 I0 = pIndices[ i ];
			int32 I1 = pIndices[ i + 1 ];
			int32 I2 = pIndices[ i + 2 ];

			assert( (uint32)I0 < numVertices );
			assert( (uint32)I1 < numVertices );
			assert( (uint32)I2 < numVertices );

			Vec3 v0, v1, v2;
			v0 = *(Vec3*)&pPositions[ posStride * I0 ];
			v1 = *(Vec3*)&pPositions[ posStride * I1 ];
			v2 = *(Vec3*)&pPositions[ posStride * I2 ];

#if CRY_PLATFORM_WINDOWS
			s_vertexBuffer.push_back( v0 );
			s_vertexBuffer.push_back( v1 );
			s_vertexBuffer.push_back( v2 );

			if (!bNoLines)
			{
				s_indexBuffer.push_back( currentIndexBase );
				s_indexBuffer.push_back( currentIndexBase + 1 );
				s_indexBuffer.push_back( currentIndexBase + 1 );
				s_indexBuffer.push_back( currentIndexBase + 2 );
				s_indexBuffer.push_back( currentIndexBase + 2 );
				s_indexBuffer.push_back( currentIndexBase );

				currentIndexBase += indexStep;
			}

			// Need to limit batch size, because the D3D aux geometry renderer
			// can only process 32768 vertices and 65536 indices
			const size_t vertexBufferSize = s_vertexBuffer.size();
			const bool bOverLimit = vertexBufferSize > kMaxBatchSize;
			const bool bLastTriangle = (i == (lastIndex - indexStep));

			if (bOverLimit || bLastTriangle)
			{
				// Draw all triangles of the chunk in one go for better batching
				pRenderAuxGeom->DrawTriangles( &s_vertexBuffer[0], s_vertexBuffer.size(), color );

				if (!bNoLines)
				{
					const size_t indexBufferSize = s_indexBuffer.size();
					pRenderAuxGeom->DrawLines( &s_vertexBuffer[0], vertexBufferSize, &s_indexBuffer[0], indexBufferSize, lineColor );

					s_indexBuffer.resize(0);
					currentIndexBase = 0;
				}

				s_vertexBuffer.resize(0);
			}
#else
			pRenderAuxGeom->DrawTriangle( v0, color, v1, color, v2, color );

			if (!bNoLines)
			{
				pRenderAuxGeom->DrawLine( v0, lineColor, v1, lineColor );
				pRenderAuxGeom->DrawLine( v1, lineColor, v2, lineColor );
				pRenderAuxGeom->DrawLine( v2, lineColor, v0, lineColor );
			}
#endif
		}
	}

	pRenderAuxGeom->SetMatrixIndex(oldIndex);
	pRenderAuxGeom->SetRenderFlags(prevRenderFlags);
}

//===========================================================================================================
void CRenderMesh::PrintMeshLeaks()
{
  MEMORY_SCOPE_CHECK_HEAP();
	AUTO_LOCK(m_sLinkLock);
	for (util::list<CRenderMesh> *iter = CRenderMesh::s_MeshList.next; iter != &CRenderMesh::s_MeshList; iter = iter->next)
	{
		CRenderMesh* pRM = iter->item<&CRenderMesh::m_Chain>();
		Warning("--- CRenderMesh '%s' leak after level unload", (!pRM->m_sSource.empty() ? pRM->m_sSource.c_str() : pRM->m_sType.c_str()));
		CRY_ASSERT_MESSAGE(0, "CRenderMesh leak");
	}
}

bool CRenderMesh::ClearStaleMemory(bool bAcquireLock, int threadId)
{
  MEMORY_SCOPE_CHECK_HEAP();
  CRY_PROFILE_FUNCTION(PROFILE_RENDERER);
	bool cleared = false; 
	bool bKeepSystem = false; 
	AUTO_LOCK(m_sLinkLock);
	// Clean up the stale mesh temporary data
  for (util::list<CRenderMesh>* iter=s_MeshDirtyList[threadId].next, *pos=iter->next; iter != &s_MeshDirtyList[threadId]; iter=pos, pos=pos->next)
  {
		CRenderMesh* pRM = iter->item<&CRenderMesh::m_Dirty>(threadId);
		if (pRM->m_sResLock.TryLock() == false)
			continue;
		// If the mesh data is still being read, skip it. The stale data will be picked up at a later point
		if (pRM->m_nThreadAccessCounter)
		{
#     if !defined(_RELEASE) && defined(RM_CATCH_EXCESSIVE_LOCKS)
			if (gEnv->pTimer->GetAsyncTime().GetSeconds()-pRM->m_lockTime > 32.f)
			{
				CryError("data lock for mesh '%s:%s' held longer than 32 seconds", (pRM->m_sType?pRM->m_sType:"unknown"), (pRM->m_sSource?pRM->m_sSource:"unknown"));
				if (CRenderer::CV_r_BreakOnError)
					__debugbreak();
			}
#     endif
			goto dirty_done;
		}

		bKeepSystem = pRM->m_keepSysMesh;

		if (!bKeepSystem && pRM->m_pCachePos)
    {
      FreeMeshData(pRM->m_pCachePos);
      pRM->m_pCachePos = NULL;
			cleared = true;
    }

		// In DX11 we cannot lock device buffers efficiently from the MT, 
		// so we have to keep system copy. On UMA systems we can clear the buffer
		// and access VRAM directly 
#if RENDERMESH_BUFFER_ENABLE_DIRECT_ACCESS
		if (!bKeepSystem) 
		{
			for (int i=0; i<VSF_NUM; i++)
				pRM->FreeVB(i);
			pRM->FreeIB();
			cleared = true; 
		}
		#endif

		// ToDo: only remove this mesh from the dirty list if no stream contains dirty data anymore
		pRM->m_Dirty[threadId].erase();
	dirty_done:
		pRM->m_sResLock.Unlock();
  }

	return cleared;
}

void CRenderMesh::UpdateModifiedMeshes(bool bAcquireLock, int threadId)
{
	CRY_ASSERT(gRenDev->m_pRT->IsRenderThread());

	MEMORY_SCOPE_CHECK_HEAP();
	CRY_PROFILE_FUNCTION(PROFILE_RENDERER);

	// Update device buffers on modified meshes
	std::vector<CRenderMesh*> modifiedMeshes;
	{
		AUTO_LOCK(m_sLinkLock);
		
		while (!s_MeshModifiedList[threadId].empty())
		{
			util::list<CRenderMesh>* pListItem = s_MeshModifiedList[threadId].next;
			pListItem->erase();

			modifiedMeshes.push_back(pListItem->item<&CRenderMesh::m_Modified>(threadId));
		}
	}

	for (auto* pMesh : modifiedMeshes)
	{
		bool updateSuccess = false;

		if (pMesh->m_sResLock.TryLock())
		{
			if (pMesh->m_nThreadAccessCounter == 0)
			{
				// Do not block on async updates to the buffers (unless in renderloadingvideomode), they will block on the drawcall
				{
					const bool isVideoPlaying = gRenDev->m_pRT->m_eVideoThreadMode != SRenderThread::eVTM_Disabled;
					if (pMesh->SyncAsyncUpdate(gRenDev->m_nProcessThreadID, isVideoPlaying) == true)
					{
						// ToDo :
						// - mark the mesh to not update itself if depending on how the async update was scheduled
						// - returns true if no streams need further processing
						CRenderMesh* pVContainer = pMesh->_GetVertexContainer();
						updateSuccess = pMesh->RT_CheckUpdate(pVContainer, pMesh->_GetVertexFormat(), VSM_MASK, false);
					}
				}
			}
			pMesh->m_sResLock.Unlock();
		}

		if (!updateSuccess)
		{
			AUTO_LOCK(m_sLinkLock);
			pMesh->m_Modified[threadId].relink_tail(s_MeshModifiedList[threadId]);
		}
	}
}

// Mesh garbage collector
void CRenderMesh::UpdateModified()
{
  MEMORY_SCOPE_CHECK_HEAP();
	SRenderThread* pRT = gRenDev->m_pRT;
	ASSERT_IS_RENDER_THREAD(pRT);
	const int threadId = gRenDev->GetRenderThreadID(); 

	// Call the update and clear functions with bAcquireLock == false even if the lock
	// was previously released in the above scope. The reasoning behind this is
	// that only the render thread can access the below lists as they are double
	// buffered. Note: As the Lock/Unlock functions can come from the main thread
	// and from any other thread, they still have to be guarded against contention!
	// The only exception to this is if we have no render thread, as there is no
	// double buffering in that case - so always lock.

	UpdateModifiedMeshes(!pRT->IsMultithreaded(), threadId);

}

// Mesh garbage collector
void CRenderMesh::Tick(uint numFrames)
{
	CRY_PROFILE_REGION(PROFILE_RENDERER, "CRenderMesh::Tick");

	MEMORY_SCOPE_CHECK_HEAP();
	ASSERT_IS_RENDER_THREAD(gRenDev->m_pRT)
		
	bool bKeepSystem = false;
	const threadID threadId = gRenDev->m_pRT->IsMultithreaded() ? gRenDev->GetRenderThreadID() : threadID(1);
	int nFrame = GetCurrentFrameID();

	// Remove deleted meshes from the list completely
	bool deleted = false;

	for (int n = 0; n < numFrames; ++n)
	{
		AUTO_LOCK(m_sLinkLock);
		util::list<CRenderMesh>* garbage = &CRenderMesh::s_MeshGarbageList[(nFrame + n) & (MAX_RELEASED_MESH_FRAMES - 1)];
		while (garbage != garbage->prev)
		{
			CRenderMesh* pRM = garbage->next->item<&CRenderMesh::m_Chain>();
			SAFE_DELETE(pRM);
			deleted = true;
		}
	}

	// If an instance pool is used, try to reclaim any used pages if there are any
	if (deleted && s_MeshPool.m_MeshInstancePool)
	{
		s_MeshPool.m_MeshInstancePool->Cleanup();
	}

	// Call the clear functions with bAcquireLock == false even if the lock
	// was previously released in the above scope. The reasoning behind this is
	// that only the render thread can access the below lists as they are double
	// buffered. Note: As the Lock/Unlock functions can come from the main thread
	// and from any other thread, they still have to be guarded against contention!

	ClearStaleMemory(false, threadId);
}

void CRenderMesh::Initialize()
{
	InitializePool();
}

void CRenderMesh::ShutDown()
{
	if (CRenderer::CV_r_releaseallresourcesonexit)
	{
		if (gRenDev->m_pRT)
		{
			Tick(MAX_RELEASED_MESH_FRAMES);
		}

		AUTO_LOCK(m_sLinkLock);
		while (&CRenderMesh::s_MeshList != CRenderMesh::s_MeshList.prev)
		{
			CRenderMesh* pRM = CRenderMesh::s_MeshList.next->item<&CRenderMesh::m_Chain>();
			PREFAST_ASSUME(pRM);
			if (CRenderer::CV_r_printmemoryleaks)
			{
				float fSize = pRM->Size(SIZE_ONLY_SYSTEM) / 1024.0f / 1024.0f;
				iLog->Log("Warning: CRenderMesh::ShutDown: RenderMesh leak %s: %0.3fMb", pRM->m_sSource.c_str(), fSize);
			}
			SAFE_RELEASE_FORCE(pRM);
		}
	}

	CRY_ASSERT(&CRenderMesh::s_MeshList == CRenderMesh::s_MeshList.prev); // Active RenderMesh-list needs to be empty, or leaks occur!
	for (int i=0; i<CRY_ARRAY_COUNT(CRenderMesh::s_MeshGarbageList); ++i) // Make sure there are no pending deletes in garbage list
		CRY_ASSERT(&CRenderMesh::s_MeshGarbageList[i] == CRenderMesh::s_MeshGarbageList[i].prev);

	new (&CRenderMesh::s_MeshList) util::list<CRenderMesh>();
	new (&CRenderMesh::s_MeshGarbageList) util::list<CRenderMesh>();
	new (&CRenderMesh::s_MeshDirtyList) util::list<CRenderMesh>();
	new (&CRenderMesh::s_MeshModifiedList) util::list<CRenderMesh>();

	ShutdownPool();
}

//////////////////////////////////////////////////////////////////////////
void CRenderMesh::KeepSysMesh(bool keep)
{
	m_keepSysMesh = keep;
}

void CRenderMesh::UnKeepSysMesh()
{
  m_keepSysMesh = false;
}

//////////////////////////////////////////////////////////////////////////
void CRenderMesh::SetVertexContainer( IRenderMesh *pBuf )
{
	MEMSTAT_CONTEXT(EMemStatContextTypes::MSC_RenderMeshType, 0, this->GetTypeName());
	MEMSTAT_CONTEXT(EMemStatContextTypes::MSC_RenderMesh, 0, this->GetSourceName());

	if (m_pVertexContainer)
		((CRenderMesh *)m_pVertexContainer)->m_lstVertexContainerUsers.Delete(this);

	m_pVertexContainer = (CRenderMesh *)pBuf;

	if (m_pVertexContainer && ((CRenderMesh *)m_pVertexContainer)->m_lstVertexContainerUsers.Find(this)<0)
		((CRenderMesh *)m_pVertexContainer)->m_lstVertexContainerUsers.Add(this);
}

//////////////////////////////////////////////////////////////////////////
void CRenderMesh::AssignChunk(CRenderChunk *pChunk, CREMeshImpl *pRE)
{
	pRE->m_pChunk = pChunk;
	pRE->m_pRenderMesh = this;
	pRE->m_nFirstIndexId = pChunk->nFirstIndexId;
	pRE->m_nNumIndices = pChunk->nNumIndices;
	pRE->m_nFirstVertId = pChunk->nFirstVertId;
	pRE->m_nNumVerts = pChunk->nNumVerts;
}

//////////////////////////////////////////////////////////////////////////
void CRenderMesh::InitRenderChunk( CRenderChunk &rChunk )
{
	assert( rChunk.nNumIndices > 0 );
	assert( rChunk.nNumVerts > 0 );

	if (!rChunk.pRE)
	{
		rChunk.pRE = (CREMeshImpl*) gRenDev->EF_CreateRE(eDATA_Mesh);
		rChunk.pRE->m_CustomTexBind[0] = m_nClientTextureBindID;
	}

	// update chunk RE
	if (rChunk.pRE)
	{
		AssignChunk(&rChunk, (CREMeshImpl*) rChunk.pRE);
	}
	assert(rChunk.nFirstIndexId + rChunk.nNumIndices <= m_nInds);
}

//////////////////////////////////////////////////////////////////////////
void CRenderMesh::SetRenderChunks( CRenderChunk *pInputChunksArray,int nCount,bool bSubObjectChunks )
{
	TRenderChunkArray *pChunksArray = &m_Chunks;
	if (bSubObjectChunks)
	{
		pChunksArray = &m_ChunksSubObjects;
	}

	ReleaseRenderChunks(pChunksArray);

	pChunksArray->resize(nCount);
	for (int i = 0; i < nCount; i++)
	{
		CRenderChunk &rChunk = (*pChunksArray)[i];
		rChunk = pInputChunksArray[i];
		InitRenderChunk( rChunk );
	}
}

//////////////////////////////////////////////////////////////////////////
void CRenderMesh::GarbageCollectSubsetRenderMeshes()
{
	uint32 nFrameID = GetCurrentFrameID();
	m_nLastSubsetGCRenderFrameID = nFrameID;
	for (MeshSubSetIndices::iterator it = m_meshSubSetIndices.begin(); it != m_meshSubSetIndices.end(); )
	{
		IRenderMesh *pRM = it->second;
		if (abs((int)nFrameID - (int)((CRenderMesh*)pRM)->m_nLastRenderFrameID) > DELETE_SUBSET_MESHES_AFTER_NOTUSED_FRAMES)
		{
			// this mesh not relevant anymore.
			it = m_meshSubSetIndices.erase(it);
		}
		else
		{
			++it;
		}
	}
}

void CRenderMesh::MarkRenderElementsDirty()
{
	// Mark RenderElements as dirty.
	for (auto& chunk : m_Chunks)
	{
		if (chunk.pRE)
		{
			chunk.pRE->mfUpdateFlags(FCEF_DIRTY);
		}
	}
}

volatile int* CRenderMesh::SetAsyncUpdateState()
{
	SREC_AUTO_LOCK(m_sResLock); 
	ASSERT_IS_MAIN_THREAD(gRenDev->m_pRT);
	int threadID = gRenDev->GetMainThreadID();
	if (m_asyncUpdateStateCounter[threadID] == 0)
	{
		m_asyncUpdateStateCounter[threadID] = 1;
		LockForThreadAccess();
	}
	CryInterlockedIncrement(&m_asyncUpdateState[threadID]);

	MarkRenderElementsDirty();

	return &m_asyncUpdateState[threadID];
}

bool CRenderMesh::SyncAsyncUpdate(int threadID, bool block)
{
	// If this mesh is being asynchronously prepared, wait for the job to finish prior to uploading 
	// the vertices to vram. 
	SREC_AUTO_LOCK(m_sResLock); 
	if (m_asyncUpdateStateCounter[threadID]) 
	{
		CRY_PROFILE_REGION(PROFILE_RENDERER, "CRenderMesh::SyncAsyncUpdate() sync");
		int iter = 0;
		while (m_asyncUpdateState[threadID])
		{
			if (!block)
				return false;
			CrySleep(iter > 10 ? 1 : 0);
			++iter;
		}
		UnlockStream(VSF_GENERAL);
		UnlockStream(VSF_TANGENTS);
		UnlockStream(VSF_VERTEX_VELOCITY);
#   if ENABLE_NORMALSTREAM_SUPPORT
		UnlockStream(VSF_NORMALS);
#   endif
		UnlockIndexStream();
		m_asyncUpdateStateCounter[threadID] = 0;
		UnLockForThreadAccess();
	}
	return true;
}

void CRenderMesh::CreateRemappedBoneIndicesPair(const uint pairGuid, const TRenderChunkArray& Chunks)
{
	ASSERT_IS_MAIN_THREAD(gRenDev->m_pRT);

	SREC_AUTO_LOCK(m_sResLock);

	// check already created remapped bone indices
	for (size_t i=0,end= m_RemappedBoneIndices.size(); i<end; ++i)
	{
		if (m_RemappedBoneIndices[i].guid == pairGuid && m_RemappedBoneIndices[i].refcount)
		{
			++m_RemappedBoneIndices[i].refcount;
			return; 
		}
	}

	// check already currently in flight remapped bone indices
	const int threadId = gRenDev->GetMainThreadID(); 
	for (size_t i=0,end=m_CreatedBoneIndices.size(); i<end; ++i)
	{
		if (m_CreatedBoneIndices[i].guid == pairGuid)
		{
			++m_CreatedBoneIndices[i].refcount;
			return; 
		}
	}

	SVF_W4B_I4S* remappedIndices = nullptr;
	{
		SScopedMeshDataLock _lock(this);

		int32 stride;
		vtx_idx *indices = GetIndexPtr(FSL_READ);
		uint32 vtxCount = GetVerticesCount();
		std::vector<bool> touched(vtxCount, false);
		const SVF_W4B_I4S* pIndicesWeights = reinterpret_cast<SVF_W4B_I4S*>(GetHWSkinPtr(stride, FSL_READ));
		remappedIndices = reinterpret_cast<SVF_W4B_I4S*>(AllocateMeshDataUnpooled(sizeof(SVF_W4B_I4S) * vtxCount));

		CRY_ASSERT_MESSAGE(stride == sizeof(SVF_W4B_I4S), "Stride doesn't match");
		CRY_ASSERT(remappedIndices && pIndicesWeights);

		if (remappedIndices && pIndicesWeights)
		{
			for (size_t j = 0; j < Chunks.size(); ++j)
			{
				const CRenderChunk &chunk = Chunks[j];
				for (size_t k = chunk.nFirstIndexId; k < chunk.nFirstIndexId + chunk.nNumIndices; ++k)
				{
					const vtx_idx vIdx = indices[k];
					if (touched[vIdx])
						continue;
					touched[vIdx] = true;

					for (int l = 0; l < 4; ++l)
					{
						remappedIndices[vIdx].weights.bcolor[l] = pIndicesWeights[vIdx].weights.bcolor[l];
						remappedIndices[vIdx].indices[l] = pIndicesWeights[vIdx].indices[l];
					}
				}
			}
		}
	}

	UnlockStream(VSF_HWSKIN_INFO);
	UnlockIndexStream();

	if (remappedIndices)
		m_CreatedBoneIndices.emplace_back(gRenDev->GetMainFrameID(), pairGuid, remappedIndices, m_pExtraBoneMapping);

	RelinkTail(m_Modified[threadId], s_MeshModifiedList[threadId]); 
}

void CRenderMesh::CreateRemappedBoneIndicesPair(const DynArray<JointIdType> &arrRemapTable, const uint pairGuid, const void* tag)
{
	ASSERT_IS_MAIN_THREAD(gRenDev->m_pRT);
	CRY_PROFILE_REGION(PROFILE_RENDERER, "CRenderMesh::CreateRemappedBoneIndicesPair");

	SREC_AUTO_LOCK(m_sResLock); 

	const int threadId = gRenDev->GetMainThreadID(); 
	// check already created remapped bone indices
	for (size_t i=0,end=m_RemappedBoneIndices.size(); i<end; ++i)
	{
		if (m_RemappedBoneIndices[i].guid == pairGuid && m_RemappedBoneIndices[i].refcount)
		{
			++m_RemappedBoneIndices[i].refcount;
			return; 
		}
	}

	// check already currently in flight remapped bone indices
	for (size_t i=0,end=m_CreatedBoneIndices.size(); i<end; ++i)
	{
		if (m_CreatedBoneIndices[i].guid == pairGuid)
		{
			++m_CreatedBoneIndices[i].refcount;
			return; 
		}
	}
	
	SVF_W4B_I4S* remappedIndices = nullptr;
	{
		SScopedMeshDataLock _lock(this);

		int32 stride;
		vtx_idx* const indices = GetIndexPtr(FSL_READ);
		uint32 vtxCount = GetVerticesCount();
		std::vector<bool> touched(vtxCount, false);
		const SVF_W4B_I4S* pIndicesWeights = reinterpret_cast<SVF_W4B_I4S*>(GetHWSkinPtr(stride, FSL_READ));
		remappedIndices = reinterpret_cast<SVF_W4B_I4S*>(AllocateMeshDataUnpooled(sizeof(SVF_W4B_I4S) * vtxCount));

		CRY_ASSERT_MESSAGE(stride == sizeof(SVF_W4B_I4S), "Stride doesn't match");
		CRY_ASSERT(remappedIndices && pIndicesWeights);


		if (remappedIndices && pIndicesWeights)
		{
			for (size_t j = 0; j < m_Chunks.size(); ++j)
			{
				const CRenderChunk &chunk = m_Chunks[j];
				for (size_t k = chunk.nFirstIndexId; k < chunk.nFirstIndexId + chunk.nNumIndices; ++k)
				{
					const vtx_idx vIdx = indices[k];
					if (touched[vIdx])
						continue;
					touched[vIdx] = true;

					for (int l = 0; l < 4; ++l)
					{
						remappedIndices[vIdx].weights.bcolor[l] = pIndicesWeights[vIdx].weights.bcolor[l];
						remappedIndices[vIdx].indices[l] = arrRemapTable[pIndicesWeights[vIdx].indices[l]];
					}
				}
			}
		}

		// bone mapping for extra bones (8 weight skinning; map weights 5 to 8 from skin bone indices to skeleton bone indices)
		if (m_pExtraBoneMapping)
		{
			for (int i = 0; i < vtxCount; ++i)
			{
				for (int l = 0; l < 4; ++l)
				{
					auto& boneIdx = m_pExtraBoneMapping[i].boneIds[l];
					boneIdx = (boneIdx == 0) ? 0 : arrRemapTable[boneIdx];
				}
			}
		}
	}

	UnlockStream(VSF_HWSKIN_INFO);
	UnlockIndexStream();

	if (remappedIndices)
		m_CreatedBoneIndices.emplace_back(gRenDev->GetMainFrameID(), pairGuid, remappedIndices, m_pExtraBoneMapping);

	RelinkTail(m_Modified[threadId], s_MeshModifiedList[threadId]);
}

void CRenderMesh::ReleaseRemappedBoneIndicesPair(const uint pairGuid)
{
	if(gRenDev->m_pRT->IsMultithreaded() && gRenDev->m_pRT->IsMainThread())
	{
		_smart_ptr<CRenderMesh> self(this);
		gRenDev->ExecuteRenderThreadCommand( [=]{ self->ReleaseRemappedBoneIndicesPair(pairGuid); }, ERenderCommandFlags::None );
		return;
	}

	CRY_ASSERT(gRenDev->m_pRT->IsRenderThread());
	CRY_PROFILE_REGION(PROFILE_RENDERER, "CRenderMesh::ReleaseRemappedBoneIndicesPair");

	SREC_AUTO_LOCK(m_sResLock); 
	size_t deleted = ~0u; 
	const int threadId = gRenDev->GetRenderThreadID();
	bool bFound = false;

	for (size_t i=0,end=m_RemappedBoneIndices.size(); i<end; ++i)
	{
		if (m_RemappedBoneIndices[i].guid == pairGuid)
		{
			bFound = true;
			if (--m_RemappedBoneIndices[i].refcount == 0)
			{
				m_DeletedBoneIndices.emplace_back(pairGuid);
				RelinkTail(m_Modified[threadId], s_MeshModifiedList[threadId]);

				return; 
			}
		}
	}
	
	// Check for created but not yet remapped bone indices
	if(!bFound)
	{
		for (size_t i=0,end=m_CreatedBoneIndices.size(); i<end; ++i)
		{
			if (m_CreatedBoneIndices[i].guid == pairGuid)
			{
				CRY_ASSERT_MESSAGE(m_CreatedBoneIndices[i].refcount > 0, "Bone indices pair over-released!");
				if (--m_CreatedBoneIndices[i].refcount == 0)
				{
					deleted = i; 
					break; 
				}
			}
		}
		
		if (deleted != ~0u)
		{
			m_DeletedBoneIndices.emplace_back(pairGuid);
			RelinkTail(m_Modified[threadId], s_MeshModifiedList[threadId]);
		}
	}
}

void CRenderMesh::LockForThreadAccess() 
{ 
	SREC_AUTO_LOCK(this->m_sResLock); 
	++m_nThreadAccessCounter; 

# if !defined(_RELEASE) && defined(RM_CATCH_EXCESSIVE_LOCKS)
	m_lockTime = (m_lockTime > 0.f) ? m_lockTime : gEnv->pTimer->GetAsyncTime().GetSeconds();
# endif
}
void CRenderMesh::UnLockForThreadAccess() 
{ 
	SREC_AUTO_LOCK(m_sResLock); 
	--m_nThreadAccessCounter; 
	if(m_nThreadAccessCounter < 0 )
	{
		__debugbreak(); // if this triggers, a mismatch betweend rendermesh thread access lock/unlock has occured
	}
# if !defined(_RELEASE) && defined(RM_CATCH_EXCESSIVE_LOCKS)
	m_lockTime = 0.f;
# endif
}
void CRenderMesh::GetPoolStats(SMeshPoolStatistics* stats)
{
	memcpy(stats, &s_MeshPool.m_MeshDataPoolStats, sizeof(SMeshPoolStatistics));
}

void* CRenderMesh::operator new(size_t size )
{
	return AllocateMeshInstanceData(size, alignof(CRenderMesh));
}

void CRenderMesh::operator delete( void* ptr )
{
  FreeMeshInstanceData(ptr);
}

D3DBuffer* CRenderMesh::_GetD3DVB( int nStream, buffer_size_t* offs ) const
{
  if (SMeshStream* pMS = GetVertexStream(nStream))
  {
    IF (pMS->m_nID != ~0u, 1)
      return gRenDev->m_DevBufMan.GetD3D(pMS->m_nID, offs);
  }
  return NULL;
}

D3DBuffer* CRenderMesh::_GetD3DIB(buffer_size_t* offs ) const
{
  IF (m_IBStream.m_nID != ~0u, 1)
    return gRenDev->m_DevBufMan.GetD3D(m_IBStream.m_nID, offs);
  return NULL;
}

bool CRenderMesh::GetRemappedSkinningData(uint32 guid, SStreamInfo& streamInfo)
{
	for (size_t i = 0, end = m_RemappedBoneIndices.size(); i < end; ++i)
	{
		CRenderMesh::SBoneIndexStream& stream = m_RemappedBoneIndices[i];
		if (stream.guid != guid)
			continue;

		if (stream.buffer != ~0u)
		{
			streamInfo.nStride = GetStreamStride(VSF_HWSKIN_INFO);
			streamInfo.hStream = stream.buffer;
			streamInfo.nSlot   = VSF_HWSKIN_INFO;

			return true;
		}

		return false;
	}

	return false;
}

bool CRenderMesh::CheckStreams()
{
	CRenderMesh* pRenderMeshForVertices = _GetVertexContainer();

	// Check on allocation/memory overflow
	if (!pRenderMeshForVertices->CanUpdate())
		return false;

	// Check on index-buffers
	if (!_HasIBStream() &&
		_NeedsIBStream())
		return false;

	// Check on vertex-buffers
	for (int nStream = 0; nStream < VSF_NUM; ++nStream)
	{
		if (!pRenderMeshForVertices->_HasVBStream(nStream) &&
			pRenderMeshForVertices->_NeedsVBStream(nStream))
			return false;
	}

	return true;
}

bool CRenderMesh::FillGeometryInfo(CRenderElement::SGeometryInfo& geom)
{
	CRenderMesh* pRenderMeshForVertices = _GetVertexContainer();
	bool streamsMissing = false;

	// Check on allocation/memory overflow
	if (!pRenderMeshForVertices->CanUpdate())
		return false;

	// Check on index-buffers
	if (_HasIBStream())
	{
		geom.indexStream.hStream = _GetIBStream();
		geom.indexStream.nStride = (sizeof(vtx_idx) == 2 ? Index16 : Index32);
	}
	else if (_NeedsIBStream())
	{
		streamsMissing = true;
	}

	// Check on vertex-buffers
	geom.nNumVertexStreams = 0;
	for (int nStream = 0; nStream < VSF_NUM; ++nStream)
	{
		if (pRenderMeshForVertices->_HasVBStream(nStream))
		{
			geom.vertexStreams[geom.nNumVertexStreams].hStream = pRenderMeshForVertices->_GetVBStream(nStream);
			geom.vertexStreams[geom.nNumVertexStreams].nStride = pRenderMeshForVertices->GetStreamStride(nStream);
			geom.vertexStreams[geom.nNumVertexStreams].nSlot = nStream;

			geom.nNumVertexStreams++;
		}
		else if (pRenderMeshForVertices->_NeedsVBStream(nStream))
		{
			streamsMissing = true;
		}
	}

	// Check on extra vertex-buffers
	if (GetRemappedSkinningData(geom.bonesRemapGUID, geom.vertexStreams[geom.nNumVertexStreams]))
	{
		geom.nNumVertexStreams++;
	}

	geom.pSkinningExtraBonesBuffer = &m_extraBonesBuffer;
#ifdef MESH_TESSELLATION_RENDERER
	geom.pTessellationAdjacencyBuffer = &m_adjBuffer;
#else
	geom.pTessellationAdjacencyBuffer = nullptr;
#endif

	return !streamsMissing;
}

CThreadSafeRendererContainer<CRenderMesh*> CRenderMesh::m_deferredSubsetGarbageCollection[RT_COMMAND_BUF_COUNT];
CThreadSafeRendererContainer<SMeshSubSetIndicesJobEntry> CRenderMesh::m_meshSubSetRenderMeshJobs[RT_COMMAND_BUF_COUNT];

///////////////////////////////////////////////////////////////////////////////
void CRenderMesh::Render(CRenderObject* pObj, const SRenderingPassInfo& passInfo)
{
	if (!pObj->m_pCurrMaterial)
	{
		pObj->m_pCurrMaterial = gRenDev->m_pDefaultMaterial;
	}
	IMaterial* pMaterial = pObj->m_pCurrMaterial;

	if (!pMaterial || !m_nVerts || !m_nInds || m_Chunks.empty() || (m_nFlags & FRM_ALLOCFAILURE) != 0)
		return;

	CRY_PROFILE_FUNCTION(PROFILE_RENDERER);

	IF(!CanUpdate(), 0)
	return;

	CRenderer* RESTRICT_POINTER rd = gRenDev;
	bool bSkinned                  = (GetChunksSkinned().size() && (pObj->m_ObjFlags & (FOB_SKINNED)));

	hidemaskLoc nMeshSubSetMask = 0;
#if !defined(_RELEASE)
	const char* szExcl = CRenderer::CV_r_excludemesh->GetString();
	if (szExcl[0] && m_sSource)
	{
		char szMesh[1024];
		cry_strcpy(szMesh, this->m_sSource);
		strlwr(szMesh);
		if (szExcl[0] == '!')
		{
			if (!strstr(&szExcl[1], m_sSource))
				return;
		}
		else if (strstr(szExcl, m_sSource))
			return;
	}
#endif

	assert(pMaterial);
	if (!pMaterial)
		return;

	m_nLastRenderFrameID = passInfo.GetFrameID();

	//////////////////////////////////////////////////////////////////////////
	if (!m_meshSubSetIndices.empty() && abs((int)m_nLastRenderFrameID - (int)m_nLastSubsetGCRenderFrameID) > DELETE_SUBSET_MESHES_AFTER_NOTUSED_FRAMES)
	{
		m_deferredSubsetGarbageCollection[passInfo.ThreadID()].push_back(this);
	}

	//////////////////////////////////////////////////////////////////////////
	bool bRenderBreakableWithMultipleDrawCalls = false;
	if (pObj->m_ObjFlags & FOB_MESH_SUBSET_INDICES && m_nVerts >= 3)
	{
		SRenderObjData* pOD = pObj->GetObjData();
		if (pOD)
		{
			if (pOD->m_nSubObjHideMask != 0)
			{
				IRenderMesh* pRM = GetRenderMeshForSubsetMask(pOD, pOD->m_nSubObjHideMask, pMaterial, passInfo);
				// if pRM is null, it means that this subset rendermesh is not computed yet, thus we render it with multiple draw calls
				if (pRM)
				{
					static_cast<CRenderMesh*>(pRM)->CRenderMesh::Render(pObj, passInfo);
					pOD->m_nSubObjHideMask = 0;
					return;
				}
				// compute the needed mask
				nMeshSubSetMask = pOD->m_nSubObjHideMask;
				pOD->m_nSubObjHideMask                = 0;
				bRenderBreakableWithMultipleDrawCalls = true;
			}
		}
	}

	int nList = EFSLIST_GENERAL;
	if (pObj->m_ObjFlags & FOB_RENDER_AFTER_POSTPROCESSING)
	{
		// Check for mesh conditions regarding post processing
		if (CRenderer::CV_r_PostProcess && CRenderer::CV_r_PostProcessHUD3D)
			AddHUDRenderElement(pObj, pMaterial, passInfo);

		return;
	}

	const int nAW = (pObj->m_ObjFlags & FOB_AFTER_WATER) || (pObj->m_ObjFlags & FOB_NEAREST) ? 1 : 0;

	//////////////
	if (gRenDev->CV_r_MotionVectors && passInfo.IsGeneralPass() && ((pObj->m_ObjFlags & FOB_DYNAMIC_OBJECT) != 0))
		CMotionBlur::SetupObject(pObj, passInfo);

	TRenderChunkArray* pChunks = bSkinned ? &m_ChunksSkinned : &m_Chunks;
	// for rendering with multiple drawcalls, use ChunksSubObjects
	if (bRenderBreakableWithMultipleDrawCalls)
		pChunks = &m_ChunksSubObjects;

	const uint32 ni = (uint32)pChunks->size();

	CRenderChunk* pPrevChunk = NULL;
	for (uint32 i = 0; i < ni; i++)
	{
		CRenderChunk* pChunk = &pChunks->at(i);
		CRenderElement* RESTRICT_POINTER pREMesh = pChunk->pRE;

		SShaderItem& ShaderItem      = pMaterial->GetShaderItem(pChunk->m_nMatID);
		CShaderResources* pR         = (CShaderResources*)ShaderItem.m_pShaderResources;
		CShader* RESTRICT_POINTER pS = (CShader*)ShaderItem.m_pShader;

		// don't render this chunk if the hide mask for it is set
		if (bRenderBreakableWithMultipleDrawCalls && ((nMeshSubSetMask & (hidemask1 << pChunk->nSubObjectIndex)) != 0))
		{
			pPrevChunk = NULL;
			continue;
		}

		if (pREMesh == NULL || pS == NULL || pR == NULL)
		{
			pPrevChunk = NULL;
			continue;
		}

		if (pS->m_Flags2 & EF2_NODRAW)
		{
			pPrevChunk = NULL;
			continue;
		}

		if (passInfo.IsShadowPass() && (pR->m_ResFlags & MTL_FLAG_NOSHADOW))
		{
			pPrevChunk = NULL;
			continue;
		}

		if ((pObj->m_bPermanent || !CRenderer::CV_r_GraphicsPipeline) && passInfo.IsShadowPass() && !passInfo.IsDisableRenderChunkMerge() && CRenderMesh::RenderChunkMergeAbleInShadowPass(pPrevChunk, pChunk, pMaterial))
			continue; // skip the merged chunk, but keep the PrevChunkReference for further merging

		PrefetchLine(pREMesh, 0);
		PrefetchLine(pObj, 0);
		passInfo.GetRenderView()->AddRenderObject(pREMesh, ShaderItem, pObj, passInfo, nList, nAW);

		pPrevChunk = pChunk;
	}
}

void CRenderMesh::AddHUDRenderElement(CRenderObject* pObj, IMaterial* pMaterial, const SRenderingPassInfo& passInfo)
{
	EPostEffectID postEffectID = EPostEffectID::HUD3D;
	SRenderObjData* pOD        = pObj->GetObjData();

	if (pOD)
	{
		if (pOD->m_nCustomFlags & COB_CUSTOM_POST_EFFECT)
		{
			postEffectID = (EPostEffectID)pOD->m_nCustomData;
		}
	}
	CPostEffect* pPostEffect   = PostEffectMgr()->GetEffect(postEffectID);
	TRenderChunkArray* pChunks = &m_Chunks;
	const uint32 ni            = (uint32)pChunks->size();

	for (uint32 i = 0; i < ni; i++)
	{
		CRenderChunk* pChunk      = &pChunks->at(i);
		CRenderElement* pREMesh = pChunk->pRE;
		SShaderItem* pShaderItem  = &pMaterial->GetShaderItem(pChunk->m_nMatID);

		pPostEffect->AddRE(pREMesh, pShaderItem, pObj, passInfo);
	}
}

void CRenderMesh::AddShadowPassMergedChunkIndicesAndVertices(CRenderChunk* pCurrentChunk, IMaterial* pMaterial, int& rNumVertices, int& rNumIndices)
{
	if (m_Chunks.size() == 0)
		return;

	if (pMaterial == NULL)
		return;

	AUTO_LOCK(pMaterial->GetSubMaterialResizeLock());
	for (uint32 i = (uint32)(pCurrentChunk - &m_Chunks[0]) + 1; i < (uint32)m_Chunks.size(); ++i)
	{
		if (!CRenderMesh::RenderChunkMergeAbleInShadowPass(pCurrentChunk, &m_Chunks[i], pMaterial))
			return;

		rNumVertices += m_Chunks[i].nNumVerts;
		rNumIndices  += m_Chunks[i].nNumIndices;
	}
}

bool CRenderMesh::RenderChunkMergeAbleInShadowPass(CRenderChunk* pPreviousChunk, CRenderChunk* pCurrentChunk, IMaterial* pMaterial)
{
	if (!CRenderer::CV_r_MergeShadowDrawcalls)
		return false;

	if (pPreviousChunk == NULL || pCurrentChunk == NULL || pMaterial == NULL)
		return false;

	SShaderItem& rCurrentShaderItem  = pMaterial->GetShaderItem(pCurrentChunk->m_nMatID);
	SShaderItem& rPreviousShaderItem = pMaterial->GetShaderItem(pPreviousChunk->m_nMatID);

	CShaderResources* pCurrentShaderResource  = (CShaderResources*)rCurrentShaderItem.m_pShaderResources;
	CShaderResources* pPreviousShaderResource = (CShaderResources*)rPreviousShaderItem.m_pShaderResources;

	CShader* pCurrentShader  = (CShader*)rCurrentShaderItem.m_pShader;
	CShader* pPreviousShader = (CShader*)rPreviousShaderItem.m_pShader;

	bool bCurrentAlphaTested  = pCurrentShaderResource->CShaderResources::IsAlphaTested();
	bool bPreviousAlphaTested = pPreviousShaderResource->CShaderResources::IsAlphaTested();

	if (bCurrentAlphaTested != bPreviousAlphaTested)
		return false;

	if (bCurrentAlphaTested)
	{
		const SEfResTexture* pCurrentResTex  = pCurrentShaderResource->GetTexture(EFTT_DIFFUSE);
		const SEfResTexture* pPreviousResTex = pPreviousShaderResource->GetTexture(EFTT_DIFFUSE);

		const CTexture* pCurrentDiffuseTex  = pCurrentResTex ? pCurrentResTex->m_Sampler.m_pTex : NULL;
		const CTexture* pPreviousDiffuseTex = pPreviousResTex ? pPreviousResTex->m_Sampler.m_pTex : NULL;

		if (pCurrentDiffuseTex != pPreviousDiffuseTex)
			return false;
	}

	if (((pPreviousShaderResource->m_ResFlags & MTL_FLAG_NOSHADOW) != 0) || ((pCurrentShaderResource->m_ResFlags & MTL_FLAG_NOSHADOW) != 0))
		return false;

	if ((pPreviousShaderResource->m_ResFlags & MTL_FLAG_2SIDED) != (pCurrentShaderResource->m_ResFlags & MTL_FLAG_2SIDED))
		return false;

	if (((pPreviousShader->m_Flags & EF_NODRAW) != 0) || ((pCurrentShader->m_Flags & EF_NODRAW) != 0))
		return false;

	return true;
}

// break-ability support
IRenderMesh* CRenderMesh::GetRenderMeshForSubsetMask(SRenderObjData* pOD, hidemask nMeshSubSetMask, IMaterial* pMaterial, const SRenderingPassInfo& passInfo)
{
	// TODO: If only one bit is set in mask - there is no need to build new index buffer - small part of main index buffer can be re-used
	// TODO: Add auto releasing of not used for long time index buffers
	// TODO: Take into account those induces when computing render mesh memory size for CGF streaming
	// TODO: Support for multiple materials

	//assert(nMeshSubSetMask!=0);

	IRenderMesh* pSrcRM = this;

	// try to find the index mesh in the already finished list
	const MeshSubSetIndices::iterator meshSubSet = m_meshSubSetIndices.find(nMeshSubSetMask);

	if (meshSubSet != m_meshSubSetIndices.end())
	{
		return meshSubSet->second;
	}

	// subset mesh was not found, start job to create one
	SMeshSubSetIndicesJobEntry* pSubSetJob = ::new(m_meshSubSetRenderMeshJobs[passInfo.ThreadID()].push_back_new())SMeshSubSetIndicesJobEntry();
	pSubSetJob->m_pSrcRM          = pSrcRM;
	pSubSetJob->m_pIndexRM        = NULL;
	pSubSetJob->m_nMeshSubSetMask = nMeshSubSetMask;

	TCreateSubsetRenderMesh job;
	job.SetClassInstance(pSubSetJob);
	job.RegisterJobState(&pSubSetJob->jobState);
	job.SetBlocking();
	job.Run();

	return NULL;
}

void CRenderMesh::RT_PerFrameTick()
{
	assert( gRenDev->m_pRT->IsRenderThread() );

	const threadID nThreadID = gRenDev->GetRenderThreadID();

	// perform all required garbage collections
	m_deferredSubsetGarbageCollection[nThreadID].CoalesceMemory();
	for (size_t i = 0; i < m_deferredSubsetGarbageCollection[nThreadID].size(); ++i)
	{
		if (m_deferredSubsetGarbageCollection[nThreadID][i])
			m_deferredSubsetGarbageCollection[nThreadID][i]->GarbageCollectSubsetRenderMeshes();
	}
	m_deferredSubsetGarbageCollection[nThreadID].resize(0);

	// add all newly generated subset meshes
	bool bJobsStillRunning          = false;
	size_t nNumSubSetRenderMeshJobs = m_meshSubSetRenderMeshJobs[nThreadID].size();
	for (size_t i = 0; i < nNumSubSetRenderMeshJobs; ++i)
	{
		SMeshSubSetIndicesJobEntry& rSubSetJob = m_meshSubSetRenderMeshJobs[nThreadID][i];
		if (rSubSetJob.jobState.IsRunning())
		{
			bJobsStillRunning = true;
		}
		else if (rSubSetJob.m_pSrcRM) // finished job, which needs to be assigned
		{
			CRenderMesh* pSrcMesh = static_cast<CRenderMesh*>(rSubSetJob.m_pSrcRM.get());
			// check that we didn't create the same subset mesh twice, if we did, clean up the duplicate
			if (pSrcMesh->m_meshSubSetIndices.find(rSubSetJob.m_nMeshSubSetMask) == pSrcMesh->m_meshSubSetIndices.end())
			{
				pSrcMesh->m_meshSubSetIndices.insert(std::make_pair(rSubSetJob.m_nMeshSubSetMask, rSubSetJob.m_pIndexRM));
			}

			// mark job as assigned
			rSubSetJob.m_pIndexRM = NULL;
			rSubSetJob.m_pSrcRM   = NULL;
		}
	}
	if (!bJobsStillRunning)
	{
		m_meshSubSetRenderMeshJobs[nThreadID].resize(0);
	}
}

////////////////////////////////////////////////////////////////////////////////////////////////////
void CRenderMesh::ClearJobResources()
{
	for (int i = 0; i < RT_COMMAND_BUF_COUNT; ++i)
	{
		for (size_t j = 0, size = m_deferredSubsetGarbageCollection[i].size();  j < size; ++j)
		{
			if (m_deferredSubsetGarbageCollection[i][j])
			{
				m_deferredSubsetGarbageCollection[i][j]->GarbageCollectSubsetRenderMeshes();
			}
		}
		stl::free_container(m_deferredSubsetGarbageCollection[i]);

		for (size_t j = 0, size = m_meshSubSetRenderMeshJobs[i].size(); j < size; ++j)
		{
			gEnv->pJobManager->WaitForJob(m_meshSubSetRenderMeshJobs[i][j].jobState);
		}
		stl::free_container(m_meshSubSetRenderMeshJobs[i]);
	}
}

////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////
void CHud3D::AddRE(const CRenderElement* re, const SShaderItem* pShaderItem, CRenderObject* pObj, const SRenderingPassInfo& passInfo)
{
	// Main thread
	const uint32 nThreadID = gRenDev->GetMainThreadID();
	// Only add valid render elements
	if (!passInfo.IsRecursivePass() && re && pObj && pShaderItem && pShaderItem->m_pShaderResources)
	{
		SHudData pHudData(re, pShaderItem, (const CShaderResources*)pShaderItem->m_pShaderResources, pObj);
		m_pRenderData[nThreadID].push_back(pHudData);
	}
}

void SMeshSubSetIndicesJobEntry::CreateSubSetRenderMesh()
{
	CRenderMesh* pSrcMesh = static_cast<CRenderMesh*>(m_pSrcRM.get());

	TRenderChunkArray& renderChunks = pSrcMesh->m_ChunksSubObjects;
	uint32 nChunkCount              = renderChunks.size();

	SScopedMeshDataLock _lock(pSrcMesh);
	int nIndCount = pSrcMesh->GetIndicesCount();
	if (vtx_idx* pInds = pSrcMesh->GetIndexPtr(FSL_READ))
	{
		TRenderChunkArray newChunks;
		newChunks.reserve(3);

		int nMatId = -1;
		PodArray<vtx_idx> lstIndices;
		for (uint32 c = 0; c < nChunkCount; c++)
		{
			CRenderChunk& srcChunk = renderChunks[c];
			if (!(m_nMeshSubSetMask & (hidemask1 << srcChunk.nSubObjectIndex)))
			{
				uint32 nLastIndex = lstIndices.size();
				lstIndices.AddList(&pInds[srcChunk.nFirstIndexId], srcChunk.nNumIndices);
				if (newChunks.empty() || nMatId != srcChunk.m_nMatID)
				{
					// New chunk needed.
					newChunks.push_back(srcChunk);
					newChunks.back().nFirstIndexId = nLastIndex;
					newChunks.back().nNumIndices   = 0;
					newChunks.back().nNumVerts     = 0;
					newChunks.back().pRE           = 0;
				}
				nMatId = srcChunk.m_nMatID;
				newChunks.back().nNumIndices += srcChunk.nNumIndices;
				newChunks.back().nNumVerts    = max((int)srcChunk.nFirstVertId + (int)srcChunk.nNumVerts - (int)newChunks.back().nFirstVertId, (int)newChunks.back().nNumVerts);
			}
		}
		_lock = {};

		IRenderMesh::SInitParamerers params;
		SVF_P3S_C4B_T2S tempVertex;
		params.pVertBuffer       = &tempVertex;
		params.nVertexCount      = 1;
		params.eVertexFormat     = EDefaultInputLayouts::P3S_C4B_T2S;
		params.pIndices          = lstIndices.GetElements();
		params.nIndexCount       = lstIndices.Count();
		params.nPrimetiveType    = prtTriangleList;
		params.eType             = eRMT_Static;
		params.nRenderChunkCount = 1;
		params.bOnlyVideoBuffer  = false;
		params.bPrecache         = false;
		_smart_ptr<IRenderMesh> pIndexMesh = gRenDev->CreateRenderMesh(pSrcMesh->m_sType, pSrcMesh->m_sSource, &params);
		pIndexMesh->SetVertexContainer(pSrcMesh);
		if (!newChunks.empty())
		{
			pIndexMesh->SetRenderChunks(&newChunks.front(), newChunks.size(), false);
			pIndexMesh->SetBBox(pSrcMesh->m_vBoxMin, pSrcMesh->m_vBoxMax);
		}
		m_pIndexRM = pIndexMesh;
	}
}

//////////////////////////////////////////////////////////////////////////
bool CRenderMesh::RayIntersectMesh(const Ray& ray, Vec3& hitpos, Vec3& p0, Vec3& p1, Vec3& p2, Vec2& uv0, Vec2& uv1, Vec2& uv2)
{
	SScopedMeshDataLock _lock(this);

	bool hasHit = false;
	// get triangle that was hit
	const int numIndices = GetIndicesCount();
	const int numVertices = GetVerticesCount();
	if (numIndices || numVertices)
	{
		// TODO: this could be optimized, e.g cache triangles and uv's and use octree to find triangles
		strided_pointer<Vec3> pPos;
		strided_pointer<Vec2> pUV;
		pPos.data = (Vec3*)GetPosPtr(pPos.iStride, FSL_READ);
		pUV.data = (Vec2*)GetUVPtr(pUV.iStride, FSL_READ);
		const vtx_idx* pIndices = GetIndexPtr(FSL_READ);

		const TRenderChunkArray& Chunks = GetChunks();
		const int nChunkCount = Chunks.size();
		for (int nChunkId = 0; nChunkId < nChunkCount; nChunkId++)
		{
			const CRenderChunk* pChunk = &Chunks[nChunkId];
			if ((pChunk->m_nMatFlags & MTL_FLAG_NODRAW))
				continue;

			int lastIndex = pChunk->nFirstIndexId + pChunk->nNumIndices;
			for (int i = pChunk->nFirstIndexId; i < lastIndex; i += 3)
			{
				const Vec3& v0 = pPos[pIndices[i]];
				const Vec3& v1 = pPos[pIndices[i + 1]];
				const Vec3& v2 = pPos[pIndices[i + 2]];

				if (Intersect::Ray_Triangle(ray, v0, v2, v1, hitpos))  // only front face
				{
					uv0 = pUV[pIndices[i]];
					uv1 = pUV[pIndices[i + 1]];
					uv2 = pUV[pIndices[i + 2]];
					p0 = v0;
					p1 = v1;
					p2 = v2;
					hasHit = true;
					nChunkId = nChunkCount; // break outer loop
					break;
				}
			}
		}
	}

	UnlockStream(VSF_GENERAL);
	UnlockIndexStream();

	return hasHit;
}