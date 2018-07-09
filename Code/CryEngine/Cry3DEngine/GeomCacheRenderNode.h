// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

// ------------------------------------------------------------------------
//  File name:   GeomCacheRenderNode.h
//  Created:     19/7/2012 by Axel Gneiting
//  Description: Draws geometry caches
// -------------------------------------------------------------------------
//
////////////////////////////////////////////////////////////////////////////

#ifndef _GEOMCACHE_RENDERNODE_
#define _GEOMCACHE_RENDERNODE_

#pragma once

#if defined(USE_GEOM_CACHES)

	#include "GeomCache.h"
	#include "GeomCacheDecoder.h"

struct SGeomCacheRenderMeshUpdateContext
{
	SGeomCacheRenderMeshUpdateContext() : m_meshId(0), m_pRenderMesh(NULL),
		m_pUpdateState(NULL), m_pIndices(NULL) {}

	// Information needed to create the render mesh each frame
	uint m_meshId;

	// The render mesh
	_smart_ptr<IRenderMesh> m_pRenderMesh;

	// Locks the render mesh from rendering until it was filled
	volatile int* m_pUpdateState;

	// Previous positions for motion blur
	stl::aligned_vector<Vec3, 16> m_prevPositions;

	// Data pointers for updating
	vtx_idx*                      m_pIndices;
	strided_pointer<Vec3>         m_pPositions;
	strided_pointer<UCol>         m_pColors;
	strided_pointer<Vec2>         m_pTexcoords;
	strided_pointer<SPipTangents> m_pTangents;
	strided_pointer<Vec3>         m_pVelocities;
};

struct SGeomCacheRenderElementData
{
	CREGeomCache*                            m_pRenderElement;
	volatile int*                            m_pUpdateState;
	int                                      m_threadId;
	DynArray<CREGeomCache::SMeshRenderData>* m_pCurrentFillData;
};

class CGeomCacheRenderNode : public IGeomCacheRenderNode, public IGeomCacheListener, public Cry3DEngineBase
{
public:
	CGeomCacheRenderNode();
	virtual ~CGeomCacheRenderNode();

	virtual const char* GetName() const final;
	virtual const char* GetEntityClassName() const final;
	virtual EERType     GetRenderNodeType() final { return eERType_GeomCache; }

	virtual Vec3        GetPos(bool bWorldOnly) const final;
	virtual void        SetBBox(const AABB& WSBBox) final;
	virtual const AABB  GetBBox() const final;
	virtual void        GetLocalBounds(AABB& bbox) final;
	virtual void        OffsetPosition(const Vec3& delta) final;
	virtual void        SetOwnerEntity(IEntity* pEntity) { m_pOwnerEntity = pEntity; }
	virtual IEntity*    GetOwnerEntity() const { return m_pOwnerEntity; }

	// Called before rendering to update to current frame bbox
	void                            UpdateBBox();

	virtual void                    Render(const struct SRendParams& entDrawParams, const SRenderingPassInfo& passInfo) final;

	void                            SetMatrix(const Matrix34& matrix);
	const Matrix34&                 GetMatrix() const                         { return m_matrix; }

	virtual struct IPhysicalEntity* GetPhysics() const final                  { return NULL; }
	virtual void                    SetPhysics(IPhysicalEntity* pPhys)  final {}

	virtual void                    SetMaterial(IMaterial* pMat) final;
	virtual IMaterial*              GetMaterial(Vec3* pHitPos) const final;
	virtual IMaterial*              GetMaterialOverride()  final { return m_pMaterial; }

	virtual float                   GetMaxViewDist() final;

	virtual void                    GetMemoryUsage(ICrySizer* pSizer) const final;

	virtual void                    UpdateStreamingPriority(const SUpdateStreamingPriorityContext& streamingContext) final;

	// Streaming
	float GetStreamingTime() const { return std::max(m_streamingTime, m_playbackTime); }

	// Called for starting the update job in CGeomCacheManager
	void StartAsyncUpdate();

	// Called by fill job if it didn't call FillFrameAsync because data wasn't available
	void SkipFrameFill();

	// Called from the update job in CGeomCacheManager
	bool FillFrameAsync(const char* const pFloorFrameData, const char* const pCeilFrameData, const float lerpFactor);

	// Called from FillFrameAsync
	void UpdateMesh_JobEntry(SGeomCacheRenderMeshUpdateContext* pUpdateContext, SGeomCacheStaticMeshData* pStaticMeshData,
	                         const char* pFloorMeshData, const char* pCeilMeshData, float lerpFactor);

	// Called from CGeomCacheManager when playback stops
	void ClearFillData();

	// Called from CObjManager to update streaming
	void UpdateStreamableComponents(float fImportance, float fDistance, bool bFullUpdate, int nLod, const float fInvScale, bool bDrawNear);

	// IGeomCacheRenderNode
	virtual bool        LoadGeomCache(const char* sGeomCacheFileName) final;

	virtual void        SetPlaybackTime(const float time) final;
	virtual float       GetPlaybackTime() const final { return m_playbackTime; }

	virtual bool        IsStreaming() const final;
	virtual void        StartStreaming(const float time) final;
	virtual void        StopStreaming() final;
	virtual bool        IsLooping() const final;
	virtual void        SetLooping(const bool bEnable) final;
	virtual float       GetPrecachedTime() const final;

	virtual IGeomCache* GetGeomCache() const final { return m_pGeomCache; }

	virtual bool        DidBoundsChange() final;

	virtual void        SetDrawing(bool bDrawing) final { m_bDrawing = bDrawing; }

	// Set stand in CGFs and distance
	virtual void SetStandIn(const char* pFilePath, const char* pMaterial) final;
	virtual void SetFirstFrameStandIn(const char* pFilePath, const char* pMaterial) final;
	virtual void SetLastFrameStandIn(const char* pFilePath, const char* pMaterial) final;
	virtual void SetStandInDistance(const float distance) final;

	// Set distance at which cache will start streaming automatically (0 means no auto streaming)
	virtual void SetStreamInDistance(const float distance) final;

	virtual void DebugDraw(const SGeometryDebugDrawInfo& info, uint nodeIndex) const final;
	virtual bool RayIntersection(SRayHitInfo& hitInfo, IMaterial* pCustomMtl, uint* pHitNodeIndex) const final;

	// Get node information
	virtual uint        GetNodeCount() const final;
	virtual Matrix34    GetNodeTransform(const uint nodeIndex) const final;
	virtual const char* GetNodeName(const uint nodeIndex) const final;
	virtual uint32      GetNodeNameHash(const uint nodeIndex) const final;
	virtual bool        IsNodeDataValid(const uint nodeIndex) const final;

	// Physics
	virtual void InitPhysicalEntity(IPhysicalEntity* pPhysicalEntity, const pe_articgeomparams& params) final;

	#ifndef _RELEASE
	void DebugRender();
	#endif

private:
	void                    CalcBBox();

	void                    FillRenderObject(const SRendParams& rendParams, const SRenderingPassInfo& passInfo, IMaterial* pMaterial, CRenderObject* pRenderObject);

	bool                    Initialize();
	bool                    InitializeRenderMeshes();
	_smart_ptr<IRenderMesh> SetupDynamicRenderMesh(SGeomCacheRenderMeshUpdateContext& updateContext);

	void                    Clear(bool bWaitForStreamingJobs);

	void                    InitTransformsRec(uint& currentNodeIndex, const std::vector<SGeomCacheStaticNodeData>& staticNodeData, const QuatTNS& currentTransform);

	void                    UpdateTransformsRec(uint& currentNodeIndex, uint& currentMeshIndex, const std::vector<SGeomCacheStaticNodeData>& staticNodeData,
	                                            const std::vector<SGeomCacheStaticMeshData>& staticMeshData, uint& currentNodeDataOffset, const char* const pFloorNodeData,
	                                            const char* const pCeilNodeData, const QuatTNS& currentTransform, const float lerpFactor);

	// IGeomCacheListener
	virtual void OnGeomCacheStaticDataLoaded();
	virtual void OnGeomCacheStaticDataUnloaded();

	void         DebugDrawRec(const SGeometryDebugDrawInfo& info,
	                          uint& currentNodeIndex, const std::vector<SGeomCacheStaticNodeData>& staticNodeData) const;
	bool         RayIntersectionRec(SRayHitInfo& hitInfo, IMaterial* pCustomMtl, uint* pHitNodeIndex,
	                                uint& currentNodeIndex, const std::vector<SGeomCacheStaticNodeData>& staticNodeData,
	                                SRayHitInfo& hitOut, float& fMinDistance) const;

	#ifndef _RELEASE
	void InstancingDebugDrawRec(uint& currentNodeIndex, const std::vector<SGeomCacheStaticNodeData>& staticNodeData);
	#endif

	enum EStandInType
	{
		eStandInType_None,
		eStandInType_Default,
		eStandInType_FirstFrame,
		eStandInType_LastFrame
	};

	EStandInType SelectStandIn() const;
	IStatObj*    GetStandIn(const EStandInType type) const;

	void         PrecacheStandIn(IStatObj* pStandIn, float fImportance, float fDistance, bool bFullUpdate, int nLod, const float fInvScale, bool bDrawNear);

	void         UpdatePhysicalEntity(const pe_articgeomparams* pParams);
	void         UpdatePhysicalMaterials();

	// Material ID -> render element data + update state pointer
	typedef std::unordered_map<uint32, SGeomCacheRenderElementData> TRenderElementMap;
	TRenderElementMap m_pRenderElements;

	// Saved node transforms for motion blur and attachments
	std::vector<Matrix34> m_nodeMatrices;

	// All render meshes
	std::vector<_smart_ptr<IRenderMesh>> m_renderMeshes;

	// Update contexts for render meshes
	std::vector<SGeomCacheRenderMeshUpdateContext> m_renderMeshUpdateContexts;

	// Override material
	_smart_ptr<IMaterial> m_pMaterial;

	// The rendered cache
	_smart_ptr<CGeomCache> m_pGeomCache;

	// World space matrix
	Matrix34 m_matrix;

	// Playback
	volatile float m_playbackTime;

	// Streaming flag
	volatile float m_streamingTime;

	// Misc
	IPhysicalEntity* m_pPhysicalEntity;
	float            m_maxViewDist;

	// World space bounding box
	AABB m_bBox;

	// AABB of current displayed frame and render buffer
	AABB m_currentAABB;
	AABB m_currentDisplayAABB;

	// Used for editor debug rendering & ray intersection
	mutable CryCriticalSection m_fillCS;

	// Transform ready sync
	mutable CryMutex             m_bTransformsReadyCS;
	mutable CryConditionVariable m_bTransformReadyCV;

	// Stand in stat objects
	EStandInType         m_standInVisible;
	_smart_ptr<IStatObj> m_pStandIn;
	_smart_ptr<IStatObj> m_pFirstFrameStandIn;
	_smart_ptr<IStatObj> m_pLastFrameStandIn;
	float                m_standInDistance;

	// Distance at which render node will automatically start streaming
	float m_streamInDistance;

	// Flags
	volatile bool m_bInitialized;
	bool          m_bLooping;
	volatile bool m_bIsStreaming;
	bool          m_bFilledFrameOnce;
	bool          m_bBoundsChanged;
	bool          m_bDrawing;
	bool          m_bTransformReady;
	IEntity*      m_pOwnerEntity = 0;
};

#endif
#endif
