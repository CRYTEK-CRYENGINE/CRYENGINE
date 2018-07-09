// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

// -------------------------------------------------------------------------
//  File name:   RenderMesh.h
//  Version:     v1.00
//  Created:     01/07/2009 by Andrey Honich.
//  Description:
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////

#ifndef __RENDERMESH_H__
#define __RENDERMESH_H__

#include <CryCore/Containers/intrusive_list.hpp>
#include <CryMemory/CryPool/PoolAlloc.h>
#include <CryCore/Containers/VectorMap.h>
#include <CryCore/Containers/VectorSet.h>
#include <CryMath/GeomQuery.h>

#include "ComputeSkinningStorage.h"

// Enable the below to get fatal error is some holds a rendermesh buffer lock for longer than 1 second
//#define RM_CATCH_EXCESSIVE_LOCKS

#define DELETE_SUBSET_MESHES_AFTER_NOTUSED_FRAMES 30

struct SMeshSubSetIndicesJobEntry
{
	JobManager::SJobState jobState;
	_smart_ptr<IRenderMesh> m_pSrcRM;							// source mesh to create a new index mesh from
	_smart_ptr<IRenderMesh> m_pIndexRM;						// when finished: newly created index mesh for this mask, else NULL
	hidemask m_nMeshSubSetMask;						// mask to use

	void CreateSubSetRenderMesh();

	SMeshSubSetIndicesJobEntry()
		: jobState()
		, m_pSrcRM()
		, m_pIndexRM()
		, m_nMeshSubSetMask(0)
	{}

	SMeshSubSetIndicesJobEntry(SMeshSubSetIndicesJobEntry&& other)
		: jobState(other.jobState)
		, m_pSrcRM(std::move(other.m_pSrcRM))
		, m_pIndexRM(std::move(other.m_pIndexRM))
		, m_nMeshSubSetMask(other.m_nMeshSubSetMask)
	{}
};

class RenderMesh_hash_int32
{
public:
	ILINE size_t operator()(int32 key) const
	{
		return stl::hash_uint32()((uint32)key);
	}
};

struct SMeshStream
{
  buffer_handle_t m_nID;   // device buffer handle from device buffer manager 
  void *m_pUpdateData;     // system buffer for updating (used for async. mesh updates)
  void *m_pLockedData;     // locked device buffer data (hmm, not a good idea to store)
  uint32 m_nLockFlags : 16;
  uint32 m_nLockCount : 16;
  uint32 m_nElements;
  int32  m_nFrameAccess;
  int32  m_nFrameRequest;
  int32  m_nFrameUpdate;
  int32  m_nFrameCreate;

  SMeshStream()
  {
    m_nID = ~0u;
    m_pUpdateData = NULL;
    m_pLockedData = NULL;

		// m_nFrameRequest MUST be <= m_nFrameUpdate initially to prevent a case where LockIB(FSL_READ) will pull the GPU buffer to the CPU (because FreeIB happened in the past)
		// In this case, while thread X is halfway through memcpy of the data as part of LockIB, the RenderThread sees m_pUpdateData without FSL_WRITE flag, and re-uploads the incompletely copied data.
		m_nFrameRequest = -1;
    m_nFrameUpdate = -1;
    m_nFrameAccess = -1;
		m_nFrameCreate = -1;
    m_nLockFlags = 0;
    m_nLockCount = 0;
    m_nElements = 0;
  }

  ~SMeshStream() { memset(this, 0x0, sizeof(*this)); }
};

// CRenderMesh::m_nFlags 
#define FRM_RELEASED              BIT(0)
#define FRM_DEPRECTATED_FLAG      BIT(1)
#define FRM_READYTOUPLOAD         BIT(2)
#define FRM_ALLOCFAILURE          BIT(3)
#define FRM_SKINNED               BIT(4)
#define FRM_SKINNEDNEXTDRAW       BIT(5) // no proper support yet for objects that can be skinned and not skinned.
#define FRM_ENABLE_NORMALSTREAM   BIT(6)
#define FRM_SKINNED_EIGHT_WEIGHTS BIT(7)

#if defined(FEATURE_SVO_GI)
  #define MAX_RELEASED_MESH_FRAMES (4) // GI voxelization threads may keep using render mesh for several frames
#else
  #define MAX_RELEASED_MESH_FRAMES (2)
#endif

struct SSetMeshIntData
{
	CMesh *m_pMesh;
	char *m_pVBuff;
	SPipTangents *m_pTBuff;
	SPipQTangents *m_pQTBuff;
	SVF_P3F *m_pVelocities;
	uint32 m_nVerts;
	uint32 m_nInds;
	vtx_idx *m_pInds;
	const Vec3 *m_pPosOffset;
	uint32 m_flags;
 	Vec3 *m_pNormalsBuff;
};

class CRenderMesh : public IRenderMesh
{
  friend class CREMeshImpl;

public:

	static void ClearJobResources();

private:
  friend class CD3D9Renderer;

	SMeshStream  m_IBStream;
	SMeshStream* m_VBStream[VSF_NUM];
	struct SBoneIndexStream
	{
		buffer_handle_t buffer; 
		uint32 guid; 
		uint32 refcount; 
	}; 

	struct SBoneIndexStreamRequest
	{
		SBoneIndexStreamRequest(int _frameId, uint32 _guid, SVF_W4B_I4S *_pStream, SMeshBoneMapping_uint16 *_pExtraStream) :
			pStream(_pStream), 
			pExtraStream(_pExtraStream), 
			guid(_guid), refcount(1), frameId(_frameId)
		{}

		SVF_W4B_I4S *pStream;
		SMeshBoneMapping_uint16 *pExtraStream;
		uint32 guid; 
		uint32 refcount; 
		int    frameId;
	};

	std::vector<SBoneIndexStream> m_RemappedBoneIndices;
	std::vector<SBoneIndexStreamRequest> m_CreatedBoneIndices;
	std::vector<uint32> m_DeletedBoneIndices;	// guid

	uint32 m_nInds;
	uint32 m_nVerts;
  int   m_nRefCounter;
	InputLayoutHandle m_eVF;          // Base stream vertex format (optional streams are hardcoded: VSF_)

  Vec3 *m_pCachePos;         // float positions (cached)
  int   m_nFrameRequestCachePos;
 
	Vec2 *m_pCacheUVs;         // float UVs (cached)
	int   m_nFrameRequestCacheUVs;

  CRenderMesh *m_pVertexContainer;
  PodArray<CRenderMesh*>  m_lstVertexContainerUsers;

#ifdef RENDER_MESH_TRIANGLE_HASH_MAP_SUPPORT
  typedef std::unordered_map<int, PodArray<std::pair<int, int>>, RenderMesh_hash_int32> TrisMap;
  TrisMap * m_pTrisMap;
#endif

  SRecursiveSpinLock m_sResLock;

  int	m_nThreadAccessCounter; // counter to ensure that no system rendermesh streams are freed since they are in use
	volatile int  m_asyncUpdateState[2];
	int  m_asyncUpdateStateCounter[2];

	ERenderPrimitiveType m_nPrimetiveType;
	ERenderMeshType m_eType;
	uint16 m_nFlags														: 8;          // FRM_
  int16  m_nLod                             : 4;          // used for LOD debug visualization
  bool m_keepSysMesh                        : 1;
  bool m_nFlagsCachePos                     : 1;          // only checked for FSL_WRITE, which can be represented as a single bit
	bool m_nFlagsCacheUVs											: 1;

public:
	enum ESizeUsageArg
	{
		SIZE_ONLY_SYSTEM = 0,
		SIZE_VB = 1,
		SIZE_IB = 2,
	};

private:
  SMeshStream* GetVertexStream(int nStream, uint32 nFlags = 0);
  SMeshStream* GetVertexStream(int nStream, uint32 nFlags = 0) const { return m_VBStream[nStream]; }
  bool UpdateVidIndices(SMeshStream& IBStream, bool stall=true);

  bool CreateVidVertices(int nVerts=0, InputLayoutHandle eVF=InputLayoutHandle::Unspecified, int nStream=VSF_GENERAL);
  bool UpdateVidVertices(int nStream);

  bool CopyStreamToSystemForUpdate(SMeshStream& MS, size_t nSize);

  void ReleaseVB(int nStream);
  void ReleaseIB();

  void InitTriHash(IMaterial * pMaterial);

  bool CreateCachePos(byte *pSrc, uint32 nStrideSrc, uint32 nFlags);
  bool PrepareCachePos();
	bool CreateCacheUVs(byte *pSrc, uint32 nStrideSrc, uint32 nFlags);	

	//Internal versions of funcs - no lock
	bool UpdateVertices_Int(const void *pVertBuffer, int nVertCount, int nOffset, int nStream, uint32 copyFlags);
  bool UpdateIndices_Int(const vtx_idx *pNewInds, int nInds, int nOffsInd, uint32 copyFlags);
  size_t SetMesh_Int( CMesh &mesh, int nSecColorsSetOffset, uint32 flags, const Vec3 *pPosOffset);	

#ifdef MESH_TESSELLATION_RENDERER
	template<class VertexFormat, class VecPos, class VecUV>
	bool UpdateUVCoordsAdjacency(SMeshStream& IBStream);

	template<class VertexFormat, class VecPos, class VecUV>
	static void BuildAdjacency(const VertexFormat *pVerts, uint nVerts, const vtx_idx *pIndexBuffer, uint nTrgs, std::vector<VecUV> &pTxtAdjBuffer);
#endif

	void Cleanup();

public:

	void AddShadowPassMergedChunkIndicesAndVertices(CRenderChunk *pCurrentChunk, IMaterial *pMaterial, int &rNumVertices, int &rNumIndices );
	static bool RenderChunkMergeAbleInShadowPass(CRenderChunk *pPreviousChunk, CRenderChunk *pCurrentChunk, IMaterial *pMaterial );

	inline void PrefetchVertexStreams() const { for (int i = 0; i < VSF_NUM; CryPrefetch(m_VBStream[i++])); }

	void SetMesh_IntImpl( SSetMeshIntData data );

	//! constructor
	//! /param szSource this pointer is stored - make sure the memory stays
	CRenderMesh(const char *szType, const char *szSourceName, bool bLock=false);
  CRenderMesh();

	//! destructor
	~CRenderMesh();

	virtual bool CanUpdate() final { return (m_nFlags & FRM_ALLOCFAILURE) == 0; }
	virtual bool CanRender() final { return (m_nFlags & FRM_ALLOCFAILURE) == 0 && CheckStreams(); }

	inline bool IsSkinned() const
	{
		return (m_nFlags & (FRM_SKINNED | FRM_SKINNEDNEXTDRAW)) != 0;
	}

	virtual void AddRef() final
	{
#   if !defined(_RELEASE)
		if (m_nFlags & FRM_RELEASED)
			CryFatalError("CRenderMesh::AddRef() mesh already in the garbage list (resurrecting deleted mesh)");
#   endif
		CryInterlockedIncrement(&m_nRefCounter);
	}
	virtual int Release() final;
  void ReleaseForce()
  {
    while (true)
    {
      int nRef = Release();
#if !defined(_RELEASE) && defined(_DEBUG)
			IF (nRef < 0, 0)
			__debugbreak();
#endif
			if (nRef == 0)
        return;
    }
  }

	// ----------------------------------------------------------------
	// Helper functions
	inline int GetStreamStride(int nStream) const
	{
		InputLayoutHandle eVF = m_eVF;

		if (nStream != VSF_GENERAL)
		{
			switch (nStream)
			{
		#ifdef TANG_FLOATS
				case VSF_TANGENTS       : eVF = EDefaultInputLayouts::T4F_B4F; break;
				case VSF_QTANGENTS      : eVF = EDefaultInputLayouts::Q4F; break;
		#else
				case VSF_TANGENTS       : eVF = EDefaultInputLayouts::T4S_B4S; break;
				case VSF_QTANGENTS      : eVF = EDefaultInputLayouts::Q4S; break;
		#endif
				case VSF_HWSKIN_INFO    : eVF = EDefaultInputLayouts::W4B_I4S; break;
				case VSF_VERTEX_VELOCITY: eVF = EDefaultInputLayouts::V3F; break;
				case VSF_NORMALS        : eVF = EDefaultInputLayouts::N3F; break;
				default:
					CryWarning(EValidatorModule::VALIDATOR_MODULE_RENDERER, EValidatorSeverity::VALIDATOR_WARNING, "Unknown nStream");
					return 0;
			}
		}

		uint16 Stride = CDeviceObjectFactory::GetInputLayoutDescriptor(eVF)->m_Strides[0];
		assert(Stride != 0);

		return Stride;
	}

  inline uint32 _GetFlags() const { return m_nFlags; }
  inline int GetStreamSize(int nStream, int nVerts=0) const { return GetStreamStride(nStream) * (nVerts ? nVerts : m_nVerts); }
  inline const buffer_handle_t _GetVBStream(int nStream) const { if (!m_VBStream[nStream]) return ~0u; return m_VBStream[nStream]->m_nID; }
  inline const buffer_handle_t _GetIBStream() const { return m_IBStream.m_nID; }
  inline bool _NeedsVBStream(int nStream) const { return m_VBStream[nStream] && m_VBStream[nStream]->m_pUpdateData && (m_VBStream[nStream]->m_nFrameRequest > m_VBStream[nStream]->m_nFrameUpdate); }
  inline bool _NeedsIBStream() const { return m_IBStream.m_pUpdateData && (m_IBStream.m_nFrameRequest > m_IBStream.m_nFrameUpdate); }
  inline bool _HasVBStream(int nStream) const { return m_VBStream[nStream] && m_VBStream[nStream]->m_nID!=~0u; }
  inline bool _HasIBStream() const { return m_IBStream.m_nID!=~0u; }
  inline int _IsVBStreamLocked(int nStream) const { if (!m_VBStream[nStream]) return 0; return (m_VBStream[nStream]->m_nLockFlags & FSL_LOCKED); }
  inline int _IsIBStreamLocked() const { return m_IBStream.m_nLockFlags & FSL_LOCKED; }
  inline InputLayoutHandle _GetVertexFormat() const { return m_eVF; }
  inline void _SetVertexFormat(InputLayoutHandle eVF) { m_eVF = eVF; }
  inline int _GetNumVerts() const { return m_nVerts; }
  inline void _SetNumVerts(int nVerts) { m_nVerts = max(nVerts, 0); }
  inline int _GetNumInds() const { return m_nInds; }
  inline void _SetNumInds(int nInds) { m_nInds = nInds; }
	inline const ERenderPrimitiveType _GetPrimitiveType() const                               { return m_nPrimetiveType; }
	inline void                       _SetPrimitiveType(const ERenderPrimitiveType nPrimType) { m_nPrimetiveType = nPrimType; }
  inline void _SetRenderMeshType(ERenderMeshType eType) { m_eType = eType; }
  inline CRenderMesh *_GetVertexContainer()
  {
    if (m_pVertexContainer)
      return m_pVertexContainer;
    return this;
  }

  D3DBuffer* _GetD3DVB(int nStream, buffer_size_t* offs) const;
  D3DBuffer* _GetD3DIB(buffer_size_t* offs) const;

  buffer_size_t Size(uint32 nFlags) const;
	void Size(uint32 nFlags, ICrySizer *pSizer ) const;

  void *LockVB(int nStream, uint32 nFlags, int nOffset=0, int nVerts=0, int *nStride=NULL, bool prefetchIB=false, bool inplaceCachePos=false);

	template<class T>
	T* GetStridedArray(strided_pointer<T>& arr, EStreamIDs stream)
	{
		arr.data = (T*)LockVB(stream, FSL_READ, 0, 0, &arr.iStride);
		assert(!arr.data || arr.iStride >= sizeof(T));
		return arr.data;
	}

	template<class T>
	T* GetStridedArray(strided_pointer<T>& arr, EStreamIDs stream, int dataType)
	{
		if (GetStridedArray(arr, stream))
		{
			const auto vertexFormatDescriptor = CDeviceObjectFactory::GetInputLayoutDescriptor(_GetVertexFormat());
			int8 offset = vertexFormatDescriptor ? vertexFormatDescriptor->m_Offsets[dataType] : -1;
			if (offset < 0)
				arr.data = nullptr;
			else
				arr.data = (T*)((char*)arr.data + offset);
		}
		return arr.data;
	}

  vtx_idx *LockIB(uint32 nFlags, int nOffset=0, int nInds=0);
  void UnlockVB(int nStream);
  void UnlockIB();

  bool RT_CheckUpdate(CRenderMesh *pVContainer, InputLayoutHandle eVF, uint32 nStreamMask, bool bTessellation = false);
  void RT_SetMeshCleanup();
  void RT_AllocationFailure(const char* sPurpose, uint32 nSize);

  void AssignChunk(CRenderChunk *pChunk, class CREMeshImpl *pRE);
  void InitRenderChunk( CRenderChunk &rChunk );

  void FreeVB(int nStream);
  void FreeIB();
  void FreeDeviceBuffers(bool bRestoreSys);
  void FreeSystemBuffers();
  void FreePreallocatedData();

	bool SyncAsyncUpdate(int threadId, bool block = true);
	void MarkRenderElementsDirty();

  //===========================================================================================
  // IRenderMesh interface
	virtual const char* GetTypeName() final         { return m_sType; }
	virtual const char* GetSourceName() const final { return m_sSource; }

	virtual int GetIndicesCount() final  { return m_nInds; }
	virtual int GetVerticesCount() final { return m_nVerts; }

	virtual InputLayoutHandle   GetVertexFormat() final { return m_eVF; }
	virtual ERenderMeshType GetMeshType() final     { return m_eType; }

	virtual void SetSkinned(bool bSkinned = true) final
  {
    if (bSkinned) m_nFlags |=  FRM_SKINNED;
    else          m_nFlags &= ~FRM_SKINNED;
  };
	virtual uint GetSkinningWeightCount() const final;

	virtual float GetGeometricMeanFaceArea() const final { return m_fGeometricMeanFaceArea; }

	virtual void NextDrawSkinned() final { m_nFlags |= FRM_SKINNEDNEXTDRAW; }

  virtual void GenerateQTangents() final;
  virtual void CreateChunksSkinned() final;
  virtual void CopyTo(IRenderMesh *pDst, int nAppendVtx=0, bool bDynamic=false, bool fullCopy=true) final;
  virtual void SetSkinningDataVegetation(struct SMeshBoneMapping_uint8 *pBoneMapping) final;
  virtual void SetSkinningDataCharacter(CMesh& mesh, uint32 flags, struct SMeshBoneMapping_uint16 *pBoneMapping, struct SMeshBoneMapping_uint16 *pExtraBoneMapping) final; 
  // Creates an indexed mesh from this render mesh (accepts an optional pointer to an IIndexedMesh object that should be used)
	virtual IIndexedMesh* GetIndexedMesh(IIndexedMesh* pIdxMesh = 0) final;
	virtual int           GetRenderChunksCount(IMaterial* pMat, int& nRenderTrisCount) final;

	virtual IRenderMesh* GenerateMorphWeights() final             { return NULL; }
	virtual IRenderMesh* GetMorphBuddy() final                    { return NULL; }
	virtual void         SetMorphBuddy(IRenderMesh* pMorph) final {}

  // Create render buffers from render mesh. Returns the final size of the render mesh or ~0U on failure
	virtual size_t SetMesh(CMesh& mesh, int nSecColorsSetOffset, uint32 flags, const Vec3* pPosOffset, bool requiresLock) final;

  // Update system vertices buffer
	virtual bool UpdateVertices(const void* pVertBuffer, int nVertCount, int nOffset, int nStream, uint32 copyFlags, bool requiresLock = true) final;
  // Update system indices buffer
	virtual bool UpdateIndices(const vtx_idx* pNewInds, int nInds, int nOffsInd, uint32 copyFlags, bool requiresLock = true) final;

	virtual void SetCustomTexID(int nCustomTID) final;
	virtual void SetChunk(int nIndex, CRenderChunk& chunk) final;
	virtual void SetChunk(IMaterial* pNewMat, int nFirstVertId, int nVertCount, int nFirstIndexId, int nIndexCount, float texelAreaDensity, int nMatID = 0) final;

	virtual void SetRenderChunks(CRenderChunk* pChunksArray, int nCount, bool bSubObjectChunks) final;

	virtual TRenderChunkArray& GetChunks() final           { return m_Chunks; }
	virtual TRenderChunkArray& GetChunksSkinned() final    { return m_ChunksSkinned; }
	virtual TRenderChunkArray& GetChunksSubObjects() final { return m_ChunksSubObjects; }
	virtual IRenderMesh*       GetVertexContainer() final  { return _GetVertexContainer(); }
	virtual void               SetVertexContainer(IRenderMesh* pBuf) final;

	virtual void Render(CRenderObject* pObj, const SRenderingPassInfo& passInfo) final;
	virtual void AddRenderElements(IMaterial* pIMatInfo, CRenderObject* pObj, const SRenderingPassInfo& passInfo, int nSortId = EFSLIST_GENERAL, int nAW = 1) final;
	virtual void SetREUserData(float* pfCustomData, float fFogScale = 0, float fAlpha = 1) final;
	virtual void AddRE(IMaterial* pMaterial, CRenderObject* pObj, IShader* pEf, const SRenderingPassInfo& passInfo, int nList, int nAW) final;

	virtual byte* GetPosPtrNoCache(int32& nStride, uint32 nFlags, int32 nOffset = 0) final;
	virtual byte* GetPosPtr(int32& nStride, uint32 nFlags, int32 nOffset = 0) final;
	virtual byte* GetNormPtr(int32& nStride, uint32 nFlags, int32 nOffset = 0) final;
	virtual byte* GetColorPtr(int32& nStride, uint32 nFlags, int32 nOffset = 0) final;
	virtual byte* GetUVPtrNoCache(int32& nStride, uint32 nFlags, int32 nOffset = 0) final;
	virtual byte* GetUVPtr(int32& nStride, uint32 nFlags, int32 nOffset = 0) final;

	virtual byte* GetTangentPtr(int32& nStride, uint32 nFlags, int32 nOffset = 0) final;
	virtual byte* GetQTangentPtr(int32& nStride, uint32 nFlags, int32 nOffset = 0) final;

	virtual byte* GetHWSkinPtr(int32& nStride, uint32 nFlags, int32 nOffset = 0, bool remapped = false) final;
	virtual byte* GetVelocityPtr(int32& nStride, uint32 nFlags, int32 nOffset = 0) final;

	virtual void UnlockStream(int nStream) final;
	virtual void UnlockIndexStream() final;

	virtual vtx_idx*                              GetIndexPtr(uint32 nFlags, int32 nOffset = 0) final;
	virtual const PodArray<std::pair<int, int> >* GetTrisForPosition(const Vec3& vPos, IMaterial* pMaterial) final;
	virtual float                                 GetExtent(EGeomForm eForm) final;
	virtual void                                  GetRandomPoints(Array<PosNorm> points, CRndGen& seed, EGeomForm eForm, SSkinningData const* pSkinning = NULL) final;
	virtual uint32*                               GetPhysVertexMap() final { return NULL; }
	virtual bool                                  IsEmpty() final;

	virtual size_t GetMemoryUsage(ICrySizer* pSizer, EMemoryUsageArgument nType) const final;
	virtual void   GetMemoryUsage(ICrySizer* pSizer) const final;
	virtual float  GetAverageTrisNumPerChunk(IMaterial* pMat) final;
	virtual int    GetTextureMemoryUsage(const IMaterial* pMaterial, ICrySizer* pSizer = NULL, bool bStreamedIn = true) const final;
	// Get allocated only in video memory or only in system memory.
	virtual int GetAllocatedBytes(bool bVideoMem) const final;

	virtual void SetBBox(const Vec3& vBoxMin, const Vec3& vBoxMax) final { m_vBoxMin = vBoxMin; m_vBoxMax = vBoxMax; }
	virtual void GetBBox(Vec3& vBoxMin, Vec3& vBoxMax) final             { vBoxMin = m_vBoxMin; vBoxMax = m_vBoxMax; };
	virtual void UpdateBBoxFromMesh() final;

  // Debug draw this render mesh.
	virtual void DebugDraw(const struct SGeometryDebugDrawInfo& info, uint32 nVisibleChunksMask = ~0) final;
	virtual void KeepSysMesh(bool keep) final;
	virtual void UnKeepSysMesh() final;
	virtual void LockForThreadAccess() final;
	virtual void UnLockForThreadAccess() final;

	virtual void SetMeshLod(int nLod) final { m_nLod = nLod; }

	virtual volatile int* SetAsyncUpdateState() final;
	void CreateRemappedBoneIndicesPair(const uint pairGuid, const TRenderChunkArray& Chunks);
 	virtual void CreateRemappedBoneIndicesPair(const DynArray<JointIdType> &arrRemapTable, const uint pairGuid, const void* tag) final;
	virtual void ReleaseRemappedBoneIndicesPair(const uint pairGuid) final;

	virtual void OffsetPosition(const Vec3& delta) final { m_vBoxMin += delta; m_vBoxMax += delta; }

	virtual bool RayIntersectMesh(const Ray& ray, Vec3& hitpos, Vec3& p0, Vec3& p1, Vec3& p2, Vec2& uv0, Vec2& uv1, Vec2& uv2) final;

  IRenderMesh* GetRenderMeshForSubsetMask(SRenderObjData *pOD, hidemask nMeshSubSetMask, IMaterial * pMaterial, const SRenderingPassInfo &passInfo);
	void GarbageCollectSubsetRenderMeshes();
	void CreateSubSetRenderMesh();

  void ReleaseRenderChunks(TRenderChunkArray* pChunks);

	bool GetRemappedSkinningData(uint32 guid, SStreamInfo& streamInfo);
	bool FillGeometryInfo(CRenderElement::SGeometryInfo& geomInfo);
	bool CheckStreams();

private:
	void AddHUDRenderElement(CRenderObject* pObj, IMaterial* pMaterial, const SRenderingPassInfo& passInfo);

public:
	// --------------------------------------------------------------
	// Members

	// When modifying or traversing any of the lists below, be sure to always hold the link lock
	static CryCriticalSection      m_sLinkLock;

	// intrusive list entries - a mesh can be in multiple lists at the same time
	util::list<CRenderMesh>        m_Chain;       // mesh will either be in the mesh list or garbage mesh list
	util::list<CRenderMesh>        m_Dirty[2];    // if linked, mesh has volatile data (data read back from vram)
	util::list<CRenderMesh>        m_Modified[2]; // if linked, mesh has modified data (to be uploaded to vram)

	// The static list heads, corresponds to the entries above
	static util::list<CRenderMesh> s_MeshList;
	static util::list<CRenderMesh> s_MeshGarbageList[MAX_RELEASED_MESH_FRAMES];
	static util::list<CRenderMesh> s_MeshDirtyList[2];
	static util::list<CRenderMesh> s_MeshModifiedList[2];

	TRenderChunkArray              m_Chunks;
	TRenderChunkArray              m_ChunksSubObjects; // Chunks of sub-objects.
	TRenderChunkArray              m_ChunksSkinned;

	int                            m_nClientTextureBindID;
	Vec3                           m_vBoxMin;
	Vec3                           m_vBoxMax;

	float                          m_fGeometricMeanFaceArea;
	CGeomExtents                   m_Extents;
	DynArray<PosNorm>              m_PosNorms;

	// Frame id when this render mesh was last rendered.
	uint32                         m_nLastRenderFrameID;
	uint32                         m_nLastSubsetGCRenderFrameID;

	string                         m_sType;          //!< pointer to the type name in the constructor call
	string                         m_sSource;        //!< pointer to the source  name in the constructor call

	// For debugging purposes to catch longstanding data accesses
# if !defined(_RELEASE) && defined(RM_CATCH_EXCESSIVE_LOCKS)
	float m_lockTime;
# endif

	typedef VectorMap<hidemask,_smart_ptr<IRenderMesh> > MeshSubSetIndices;
	MeshSubSetIndices m_meshSubSetIndices;

	static CThreadSafeRendererContainer<SMeshSubSetIndicesJobEntry> m_meshSubSetRenderMeshJobs[RT_COMMAND_BUF_COUNT];
	static CThreadSafeRendererContainer<CRenderMesh*> m_deferredSubsetGarbageCollection[RT_COMMAND_BUF_COUNT];

#ifdef RENDER_MESH_TRIANGLE_HASH_MAP_SUPPORT
  CryCriticalSection m_getTrisForPositionLock;
#endif

#ifdef MESH_TESSELLATION_RENDERER
	CGpuBuffer m_adjBuffer;                // buffer containing adjacency information to fix displacement seams
#endif

	CGpuBuffer m_extraBonesBuffer;

	// shared inputs
	std::shared_ptr<compute_skinning::IPerMeshDataSupply> m_computeSkinningDataSupply;
	uint32 m_nMorphs;

	void ComputeSkinningCreateSkinningBuffers(const SVF_W4B_I4S* pBoneMapping, const SMeshBoneMapping_uint16* pExtraBoneMapping);
	void ComputeSkinningCreateBindPoseAndMorphBuffers(CMesh& mesh);
	SMeshBoneMapping_uint16* m_pExtraBoneMapping;

	static void Initialize();
	static void ShutDown();
	static void Tick(uint numFrames = 1);
	static void UpdateModified();
	static void UpdateModifiedMeshes(bool bAcquireLock, int threadId);
	static bool ClearStaleMemory(bool bAcquireLock, int threadId);
	static void PrintMeshLeaks();
	static void GetPoolStats(SMeshPoolStatistics* stats);

	void* operator new(size_t size);
	void operator delete(void* ptr);

	static void RT_PerFrameTick();
};

//////////////////////////////////////////////////////////////////////
// General VertexBuffer created by CreateVertexBuffer() function
class CVertexBuffer
{
public:
	CVertexBuffer()
	{
		m_eVF = InputLayoutHandle::Unspecified;
		m_nVerts = 0;
	}
	CVertexBuffer(void* pData, InputLayoutHandle eVF, int nVerts = 0)
	{
		m_VData = pData;
		m_eVF = eVF;
		m_nVerts = nVerts;
	}

	void* m_VData;
	InputLayoutHandle m_eVF;
	int32 m_nVerts;
};

class CIndexBuffer
{
public:
	CIndexBuffer()
	{
		m_nInds = 0;
	}

	CIndexBuffer(uint16* pData)
	{
		m_IData = pData;
		m_nInds = 0;
	}

	void* m_IData;
	int32 m_nInds;
};

#endif // __RenderMesh2_h__
