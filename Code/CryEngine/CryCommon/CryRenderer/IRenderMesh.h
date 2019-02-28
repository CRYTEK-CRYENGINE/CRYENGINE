// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.
#pragma once

#ifndef _RenderMesh_H_
#define _RenderMesh_H_

#include "VertexFormats.h"
#include <Cry3DEngine/IMaterial.h>
#include <CryRenderer/IShader.h>
#include <CryRenderer/IRenderer.h>  // PublicRenderPrimitiveType
#include <CryMath/Cry_Geo.h>
#include <CryCore/Containers/CryArray.h>
#include <CryThreading/IJobManager.h>

class CMesh;
struct CRenderChunk;
class CRenderObject;
struct SSkinningData;
struct IMaterial;
struct IShader;
struct IIndexedMesh;
struct SMRendTexVert;
struct UCol;
struct GeomInfo;

struct TFace;
struct SMeshSubset;
struct SRenderingPassInfo;
struct SRenderObjectModifier;

//! Keep this in sync with BUFFER_USAGE hints DevBuffer.h.
enum ERenderMeshType
{
	eRMT_Immmutable = 0,
	eRMT_Static     = 1,
	eRMT_Dynamic    = 2,
	eRMT_Transient  = 3,
};

#define FSM_VERTEX_VELOCITY      1
#define FSM_NO_TANGENTS          2
#define FSM_CREATE_DEVICE_MESH   4
#define FSM_SETMESH_ASYNC        8
#define FSM_ENABLE_NORMALSTREAM  16
#define FSM_IGNORE_TEXELDENSITY  32
#define FSM_USE_COMPUTE_SKINNING 64

// Invalidate video buffer flags
#define FMINV_STREAM      1
#define FMINV_STREAM_MASK ((1 << VSF_NUM) - 1)
#define FMINV_INDICES     0x100
#define FMINV_ALL         -1

// Stream lock flags
#define  FSL_READ            0x01
#define  FSL_WRITE           0x02
#define  FSL_DYNAMIC         0x04
#define  FSL_DISCARD         0x08
#define  FSL_VIDEO           0x10
#define  FSL_SYSTEM          0x20
#define  FSL_INSTANCED       0x40
#define  FSL_NONSTALL_MAP    0x80    // Map must not stall for VB/IB locking
#define  FSL_VBIBPUSHDOWN    0x100   // Push down from vram on demand if target architecture supports it, used internally
#define  FSL_DIRECT          0x200   // Access VRAM directly if target architecture supports it, used internally
#define  FSL_LOCKED          0x400   // Internal use
#define  FSL_SYSTEM_CREATE   (FSL_WRITE | FSL_DISCARD | FSL_SYSTEM)
#define  FSL_SYSTEM_UPDATE   (FSL_WRITE | FSL_SYSTEM)
#define  FSL_VIDEO_CREATE    (FSL_WRITE | FSL_DISCARD | FSL_VIDEO)
#define  FSL_VIDEO_UPDATE    (FSL_WRITE | FSL_VIDEO)

#define FSL_ASYNC_DEFER_COPY (1u << 1)
#define FSL_FREE_AFTER_ASYNC (2u << 1)

struct IRenderMesh
{
	enum EMemoryUsageArgument
	{
		MEM_USAGE_COMBINED,
		MEM_USAGE_ONLY_SYSTEM,
		MEM_USAGE_ONLY_VIDEO,
		MEM_USAGE_ONLY_STREAMS,
	};

	//! Render mesh initialization parameters, that can be used to create RenderMesh from row pointers.
	struct SInitParamerers
	{
		InputLayoutHandle             eVertexFormat;
		ERenderMeshType           eType;

		void*                     pVertBuffer;
		int                       nVertexCount;
		SPipTangents*             pTangents;
		SPipNormal*               pNormals;
		vtx_idx*                  pIndices;
		int                       nIndexCount;
		PublicRenderPrimitiveType nPrimetiveType;
		int                       nRenderChunkCount;
		int                       nClientTextureBindID;
		bool                      bOnlyVideoBuffer;
		bool                      bPrecache;
		bool                      bLockForThreadAccess;

		SInitParamerers() : eVertexFormat(EDefaultInputLayouts::P3F_C4B_T2F), eType(eRMT_Static), pVertBuffer(0), nVertexCount(0), pTangents(0), pNormals(0), pIndices(0), nIndexCount(0),
			nPrimetiveType(prtTriangleList), nRenderChunkCount(0), nClientTextureBindID(0), bOnlyVideoBuffer(false), bPrecache(true), bLockForThreadAccess(false) {}
	};

	struct ThreadAccessLock
	{
		ThreadAccessLock(IRenderMesh* pRM)
			: m_pRM(pRM)
		{
			m_pRM->LockForThreadAccess();
		}

		~ThreadAccessLock()
		{
			m_pRM->UnLockForThreadAccess();
		}

	private:
		ThreadAccessLock(const ThreadAccessLock&);
		ThreadAccessLock& operator=(const ThreadAccessLock&);

	private:
		IRenderMesh* m_pRM;
	};

	// <interfuscator:shuffle>
	virtual ~IRenderMesh(){}

	//////////////////////////////////////////////////////////////////////////
	// Reference Counting.
	virtual void AddRef() = 0;
	virtual int  Release() = 0;
	//////////////////////////////////////////////////////////////////////////

	//! Prevent rendering if video memory could not been allocated for it.
	virtual bool CanUpdate() = 0;
	virtual bool CanRender() = 0;

	//! Returns type name given to the render mesh on creation time.
	virtual const char* GetTypeName() = 0;

	//! Returns the name of the source given to the render mesh on creation time.
	virtual const char*     GetSourceName() const = 0;

	virtual int             GetIndicesCount() = 0;
	virtual int             GetVerticesCount() = 0;
	virtual InputLayoutHandle   GetVertexFormat() = 0;
	virtual ERenderMeshType GetMeshType() = 0;
	virtual float           GetGeometricMeanFaceArea() const = 0;

	virtual void            SetSkinned(bool bSkinned = true) = 0;
	virtual uint            GetSkinningWeightCount() const = 0;

	//! Create render buffers from render mesh. Returns the final size of the render mesh or ~0U on failure.
	virtual size_t SetMesh(CMesh& mesh, int nSecColorsSetOffset, uint32 flags, const Vec3* pPosOffset, bool requiresLock) = 0;
	virtual void   CopyTo(IRenderMesh* pDst, int nAppendVtx = 0, bool bDynamic = false, bool fullCopy = true) = 0;
	virtual void   SetSkinningDataVegetation(struct SMeshBoneMapping_uint8* pBoneMapping) = 0;
	virtual void   SetSkinningDataCharacter(CMesh& mesh, uint32 flags, struct SMeshBoneMapping_uint16* pBoneMapping, struct SMeshBoneMapping_uint16* pExtraBoneMapping) = 0;

	//! Creates an indexed mesh from this render mesh (accepts an optional pointer to an IIndexedMesh object that should be used).
	virtual IIndexedMesh* GetIndexedMesh(IIndexedMesh* pIdxMesh = 0) = 0;
	virtual int           GetRenderChunksCount(IMaterial* pMat, int& nRenderTrisCount) = 0;

	virtual IRenderMesh*  GenerateMorphWeights() = 0;
	virtual IRenderMesh*  GetMorphBuddy() = 0;
	virtual void          SetMorphBuddy(IRenderMesh* pMorph) = 0;

	virtual bool          UpdateVertices(const void* pVertBuffer, int nVertCount, int nOffset, int nStream, uint32 copyFlags, bool requiresLock = true) = 0;
	virtual bool          UpdateIndices(const vtx_idx* pNewInds, int nInds, int nOffsInd, uint32 copyFlags, bool requiresLock = true) = 0;
	virtual void          SetCustomTexID(int nCustomTID) = 0;
	virtual void          SetChunk(int nIndex, CRenderChunk& chunk) = 0;
	virtual void          SetChunk(IMaterial* pNewMat, int nFirstVertId, int nVertCount, int nFirstIndexId, int nIndexCount, float texelAreaDensity, int nMatID = 0) = 0;

	//! Assign array of render chunks.
	//! Initializes render element for each render chunk.
	virtual void                                 SetRenderChunks(CRenderChunk* pChunksArray, int nCount, bool bSubObjectChunks) = 0;

	virtual void                                 GenerateQTangents() = 0;
	virtual void                                 CreateChunksSkinned() = 0;
	virtual void                                 NextDrawSkinned() = 0;
	virtual IRenderMesh*                         GetVertexContainer() = 0;
	virtual void                                 SetVertexContainer(IRenderMesh* pBuf) = 0;
	virtual TRenderChunkArray&                   GetChunks() = 0;
	virtual TRenderChunkArray&                   GetChunksSkinned() = 0;
	virtual TRenderChunkArray&                   GetChunksSubObjects() = 0;
	virtual void                                 SetBBox(const Vec3& vBoxMin, const Vec3& vBoxMax) = 0;
	virtual void                                 GetBBox(Vec3& vBoxMin, Vec3& vBoxMax) = 0;
	virtual void                                 UpdateBBoxFromMesh() = 0;
	virtual uint32*                              GetPhysVertexMap() = 0;
	virtual bool                                 IsEmpty() = 0;

	virtual uint8*                               GetPosPtrNoCache(int32& nStride, uint32 nFlags, int32 nOffset = 0) = 0;
	virtual uint8*                               GetPosPtr(int32& nStride, uint32 nFlags, int32 nOffset = 0) = 0;
	virtual uint8*                               GetColorPtr(int32& nStride, uint32 nFlags, int32 nOffset = 0) = 0;
	virtual uint8*                               GetNormPtr(int32& nStride, uint32 nFlags, int32 nOffset = 0) = 0;
	virtual uint8*                               GetUVPtrNoCache(int32& nStride, uint32 nFlags, int32 nOffset = 0) = 0;
	virtual uint8*                               GetUVPtr(int32& nStride, uint32 nFlags, int32 nOffset = 0) = 0;

	virtual uint8*                               GetTangentPtr(int32& nStride, uint32 nFlags, int32 nOffset = 0) = 0;
	virtual uint8*                               GetQTangentPtr(int32& nStride, uint32 nFlags, int32 nOffset = 0) = 0;

	virtual uint8*                               GetHWSkinPtr(int32& nStride, uint32 nFlags, int32 nOffset = 0, bool remapped = false) = 0;
	virtual uint8*                               GetVelocityPtr(int32& nStride, uint32 nFlags, int32 nOffset = 0) = 0;

	virtual void                                 UnlockStream(int nStream) = 0;
	virtual void                                 UnlockIndexStream() = 0;

	virtual vtx_idx*                             GetIndexPtr(uint32 nFlags, int32 nOffset = 0) = 0;
	virtual const PodArray<std::pair<int, int>>* GetTrisForPosition(const Vec3& vPos, IMaterial* pMaterial) = 0;

	virtual float                                GetExtent(EGeomForm eForm) = 0;
	virtual void                                 GetRandomPoints(Array<PosNorm> points, CRndGen& seed, EGeomForm eForm, SSkinningData const* pSkinning = NULL) = 0;

	virtual void                                 Render(CRenderObject* pObj, const SRenderingPassInfo& passInfo) = 0;
	virtual void                                 AddRenderElements(IMaterial* pIMatInfo, CRenderObject* pObj, const SRenderingPassInfo& passInfo, int nSortId = EFSLIST_GENERAL, int nAW = 1) = 0;
	virtual void                                 AddRE(IMaterial* pMaterial, CRenderObject* pObj, IShader* pEf, const SRenderingPassInfo& passInfo, int nList, int nAW) = 0;
	virtual void                                 SetREUserData(float* pfCustomData, float fFogScale = 0, float fAlpha = 1) = 0;

	//! Debug draw this render mesh.
	virtual void DebugDraw(const struct SGeometryDebugDrawInfo& info, uint32 nVisibleChunksMask = ~0) = 0;

	//! Returns mesh memory usage and add it to the CrySizer (if not NULL).
	//! \param pSizer Sizer interface, can be NULL if caller only want to calculate size.
	//! \param nType see EMemoryUsageArgument.
	virtual size_t GetMemoryUsage(ICrySizer* pSizer, EMemoryUsageArgument nType) const = 0;
	virtual void   GetMemoryUsage(ICrySizer* pSizer) const = 0;

	//! Get allocated only in video memory or only in system memory.
	virtual int   GetAllocatedBytes(bool bVideoMem) const = 0;
	virtual float GetAverageTrisNumPerChunk(IMaterial* pMat) = 0;
	virtual int   GetTextureMemoryUsage(const IMaterial* pMaterial, ICrySizer* pSizer = NULL, bool bStreamedIn = true) const = 0;
	virtual void  KeepSysMesh(bool keep) = 0;                                                                                     // HACK: temp workaround for GDC-888
	virtual void  UnKeepSysMesh() = 0;
	virtual void  SetMeshLod(int nLod) = 0;

	virtual void  LockForThreadAccess() = 0;
	virtual void  UnLockForThreadAccess() = 0;

	//! Sets the async update state - will sync before rendering to this.
	virtual volatile int* SetAsyncUpdateState(void) = 0;
	virtual void          CreateRemappedBoneIndicesPair(const DynArray<JointIdType>& arrRemapTable, const uint pairGuid, const void* tag) = 0;
	virtual void          ReleaseRemappedBoneIndicesPair(const uint pairGuid) = 0;

	virtual void          OffsetPosition(const Vec3& delta) = 0;

	virtual bool          RayIntersectMesh(const Ray& ray, Vec3& hitpos, Vec3& p0, Vec3& p1, Vec3& p2, Vec2& uv0, Vec2& uv1, Vec2& uv2) = 0;

	// </interfuscator:shuffle>
};

#endif                                                                                                                          // _RenderMesh_H_
