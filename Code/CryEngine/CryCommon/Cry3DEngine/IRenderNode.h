// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include <atomic>
#include "../CryAudio/IAudioSystem.h"

#define SUPP_HMAP_OCCL

struct IMaterial;
struct IVisArea;
struct SRenderingPassInfo;
struct IGeomCache;
struct SRendItemSorter;
struct SFrameLodInfo;
struct ICharacterInstance;
struct IStatObj;
struct IEntity;

enum EERType
{
	eERType_NotRenderNode,
	eERType_Brush,
	eERType_Vegetation,
	eERType_Light,
	eERType_Dummy_5, //!< Used to be eERType_Cloud, preserve order for compatibility.
	eERType_Dummy_1, //!< Used to be eERType_VoxelObject, preserve order for compatibility.
	eERType_FogVolume,
	eERType_Decal,
	eERType_ParticleEmitter,
	eERType_WaterVolume,
	eERType_WaterWave,
	eERType_Road,
	eERType_DistanceCloud,
	eERType_Dummy_4, // Used to be eERType_VolumeObject, preserve order for compatibility.
	eERType_Dummy_0, //!< Used to be eERType_AutoCubeMap, preserve order for compatibility.
	eERType_Rope,
	eERType_Dummy_3, //!< Used to be  eERType_PrismObject, preserve order for compatibility.
	eERType_TerrainSector,
	eERType_Dummy_2, //!< Used to be eERType_LightPropagationVolume, preserve order for compatibility.
	eERType_MovableBrush,
	eERType_GameEffect,
	eERType_BreakableGlass,
	eERType_CloudBlocker,
	eERType_MergedMesh,
	eERType_GeomCache,
	eERType_Character,
	eERType_TypesNum, //!< Must be at the end - gives the total number of ER types.
};

enum ERNListType
{
	//! This must be the first enum element.
	eRNListType_Unknown,

	eRNListType_Brush,
	eRNListType_Vegetation,
	eRNListType_DecalsAndRoads,
	eRNListType_ListsNum,

	//! This should be the last member.
	//! It relies on eRNListType_Unknown being the first enum element.
	eRNListType_First = eRNListType_Unknown,
};

enum EOcclusionObjectType
{
	eoot_OCCLUDER,
	eoot_OCEAN,
	eoot_OCCELL,
	eoot_OCCELL_OCCLUDER,
	eoot_OBJECT,
	eoot_OBJECT_TO_LIGHT,
	eoot_TERRAIN_NODE,
	eoot_PORTAL,
};

struct OcclusionTestClient
{
	OcclusionTestClient() : nLastOccludedMainFrameID(0), nLastVisibleMainFrameID(0)
	{
#ifdef SUPP_HMAP_OCCL
		vLastVisPoint.Set(0, 0, 0);
		nTerrainOccLastFrame = 0;
#endif
	}

	uint32 nLastVisibleMainFrameID, nLastOccludedMainFrameID;
	uint32 nLastShadowCastMainFrameID, nLastNoShadowCastMainFrameID;

#ifdef SUPP_HMAP_OCCL
	Vec3 vLastVisPoint;
	int  nTerrainOccLastFrame;
#endif
};

struct SRenderNodeTempData
{
	struct SUserData
	{
		int                 lastSeenFrame[MAX_RECURSION_LEVELS];             // must be first, see IRenderNode::SetDrawFrame()
		int                 lastSeenShadowFrame;                             // When was last rendered to shadow
		CRenderObject*      arrPermanentRenderObjects[MAX_STATOBJ_LODS_NUM];
		float               arrLodLastTimeUsed[MAX_STATOBJ_LODS_NUM];
		Matrix34            objMat;
		OcclusionTestClient m_OcclState;
		struct IFoliage*    m_pFoliage;
		struct IClipVolume* m_pClipVolume;

		Vec4                vEnvironmentProbeMults;
		Vec3                lastClipVolumePosition;
		uint32              nCubeMapId                  : 16;
		uint32              nCubeMapIdCacheClearCounter : 16;
		uint32              nWantedLod                  : 8;
		uint32              bTerrainColorWasUsed        : 1;
		uint32              bClipVolumeAssigned         : 1;
		IRenderNode*        pOwnerNode;
		uint32              nStatObjLastModificationId;
	};

public:
	SUserData           userData;

	CryRWLock           arrPermanentObjectLock[MAX_STATOBJ_LODS_NUM];

	std::atomic<uint32> hasValidRenderObjects;
	std::atomic<uint32> invalidRenderObjects;

public:
	SRenderNodeTempData()  { ZeroStruct(userData); hasValidRenderObjects = invalidRenderObjects = false; }
	~SRenderNodeTempData() { Free(); };

	CRenderObject* GetRenderObject(int nLod); /* thread-safe */
	void           Free();
	void           FreeRenderObjects(); /* non-thread-safe */
	void           InvalidateRenderObjectsInstanceData();

	void           SetClipVolume(IClipVolume* pClipVolume, const Vec3& pos);
	void           ResetClipVolume();

	void           OffsetPosition(const Vec3& delta)
	{
		userData.objMat.SetTranslation(userData.objMat.GetTranslation() + delta);
	}

	void MarkForAutoDelete()
	{
		userData.lastSeenFrame[0] =
		  userData.lastSeenFrame[1] =
		    userData.lastSeenShadowFrame = 0;
	}
};

// RenderNode flags
enum ERenderNodeFlags : uint64
{
	ERF_GOOD_OCCLUDER                 = BIT(0),
	ERF_PROCEDURAL                    = BIT(1),
	ERF_CLONE_SOURCE                  = BIT(2), //!< set if this object was cloned from another one
	ERF_CASTSHADOWMAPS                = BIT(3), //!< if you ever set this flag, be sure also to set ERF_HAS_CASTSHADOWMAPS
	ERF_RENDER_ALWAYS                 = BIT(4),
	ERF_DYNAMIC_DISTANCESHADOWS       = BIT(5),
	ERF_HIDABLE                       = BIT(6),
	ERF_HIDABLE_SECONDARY             = BIT(7),
	ERF_HIDDEN                        = BIT(8),
	ERF_SELECTED                      = BIT(9),
	ERF_GI_MODE_BIT0                  = BIT(10), //!< Bit0 of GI mode.
	ERF_OUTDOORONLY                   = BIT(11),
	ERF_NODYNWATER                    = BIT(12),
	ERF_EXCLUDE_FROM_TRIANGULATION    = BIT(13),
	ERF_REGISTER_BY_BBOX              = BIT(14),
	ERF_PICKABLE                      = BIT(15),
	ERF_GI_MODE_BIT1                  = BIT(16), //!< Bit1 of GI mode.
	ERF_NO_PHYSICS                    = BIT(17),
	ERF_NO_DECALNODE_DECALS           = BIT(18),
	ERF_REGISTER_BY_POSITION          = BIT(19),
	ERF_STATIC_INSTANCING             = BIT(20),
	ERF_RECVWIND                      = BIT(21),
	ERF_COLLISION_PROXY               = BIT(22), //!< Collision proxy is a special object that is only visible in editor and used for physical collisions with player and vehicles.
	ERF_GI_MODE_BIT2                  = BIT(23), //!< Bit2 of GI mode.
	ERF_SPEC_BIT0                     = BIT(24), //!< Bit0 of min config specification.
	ERF_SPEC_BIT1                     = BIT(25), //!< Bit1 of min config specification.
	ERF_SPEC_BIT2                     = BIT(26), //!< Bit2 of min config specification.
	ERF_RAYCAST_PROXY                 = BIT(27), //!< raycast proxy is only used for raycasting
	ERF_HUD                           = BIT(28), //!< Hud object that can avoid some visibility tests
	ERF_RAIN_OCCLUDER                 = BIT(29), //!< Is used for rain occlusion map
	ERF_HAS_CASTSHADOWMAPS            = BIT(30), //!< at one point had ERF_CASTSHADOWMAPS set
	ERF_ACTIVE_LAYER                  = BIT(31), //!< the node is on a currently active layer

	ERF_ENABLE_ENTITY_RENDER_CALLBACK = BIT64(32),  //!< Enables render nodes to send a special render callback to their entity owner

	ERF_CUSTOM_VIEW_DIST_RATIO        = BIT64(33), //!< Override normal view dist ratio for this node with the one from the cvar e_ViewDistRatioCustom (AI/Vehicles use it)
	ERF_FORCE_POST_3D_RENDER          = BIT64(34), //!< Enables this node to be rendered in the special post 3d render pass
	ERF_DISABLE_MOTION_BLUR           = BIT64(35), //!< Disable motion blur effect for this render node
	ERF_SHADOW_DISSOLVE               = BIT64(36), //!< Clocking effect require shadow to also dissolve to simulate disappearance of the object
	ERF_HUD_REQUIRE_DEPTHTEST         = BIT64(37), //!< If 3D HUD Object requires to be rendered at correct depth (i.e. behind weapon)

	ERF_MOVES_EVERY_FRAME             = BIT64(38), //!< Set on Render Nodes that are highly dynamic, for optimization reasons

	// Special additional flags that are set on CRenderObject flags
	ERF_FOB_RENDER_AFTER_POSTPROCESSING = BIT64(39), //!< Set FOB_RENDER_AFTER_POSTPROCESSING on the CRenderObject flags
	ERF_FOB_NEAREST                     = BIT64(40), //!< Set FOB_NEAREST on the CRenderObject flags
	ERF_PENDING_DELETE                  = BIT64(41),
};

#define ERF_SPEC_BITS_MASK    (ERF_SPEC_BIT0 | ERF_SPEC_BIT1 | ERF_SPEC_BIT2)
#define ERF_SPEC_BITS_SHIFT   24

#define ERF_GI_MODE_BITS_MASK (ERF_GI_MODE_BIT0 | ERF_GI_MODE_BIT1 | ERF_GI_MODE_BIT2)          // Bit mask of the GI mode.

/* Base class of IRenderNode: Be careful when modifying the size as we can easily have millions of IRenderNode in a level. */
struct IShadowCaster
{
	// <interfuscator:shuffle>
	virtual ~IShadowCaster(){}
	virtual bool                       HasOcclusionmap(int nLod, IRenderNode* pLightOwner)           { return false; }
	virtual CLodValue                  ComputeLod(int wantedLod, const SRenderingPassInfo& passInfo) { return CLodValue(wantedLod); }
	virtual void                       Render(const SRendParams& RendParams, const SRenderingPassInfo& passInfo) = 0;
	virtual const AABB                 GetBBoxVirtual() = 0;
	virtual void                       FillBBox(AABB& aabb) = 0;
	virtual struct ICharacterInstance* GetEntityCharacter(Matrix34A* pMatrix = NULL, bool bReturnOnlyVisible = false) = 0;
	virtual EERType                    GetRenderNodeType() = 0;
	// </interfuscator:shuffle>

	//! Internal states to track shadow cache status
	uint8                              m_shadowCacheLastRendered[MAX_GSM_CACHED_LODS_NUM];
	uint8                              m_shadowCacheLod[MAX_GSM_CACHED_LODS_NUM];

	//! Shadow LOD bias.
	//! Set to SHADOW_LODBIAS_DISABLE to disable any shadow lod overrides for this rendernode.
	static const int8 SHADOW_LODBIAS_DISABLE = -128;
	int8              m_cShadowLodBias;

	int8              unused;
};

struct IOctreeNode
{
public:
	struct CTerrainNode* GetTerrainNode() const { return m_pTerrainNode; }
	struct CVisArea*     GetVisArea() const     { return m_pVisArea; }
	virtual void         MarkAsUncompiled(const ERNListType eListType) = 0;

protected:
	struct CVisArea*     m_pVisArea;
	struct CTerrainNode* m_pTerrainNode;
};

/*! \brief Represents a rendered object in the world
 *
 * To visualize objects in a world CRYENGINE defines the concept of render nodes and render elements. Render nodes represent general objects in the 3D engine. Among other things they are used to build a hierarchy for visibility culling, allow physics interactions (optional) and rendering.
 * For actual rendering they add themselves to the renderer (with the help of render objects as you can see in the sample code below) passing an appropriate render element which implements the actual drawing of the object.
 *
 * Be careful when modifying the size as we can easily have millions of IRenderNode in a level.
 */
struct IRenderNode : public IShadowCaster
{
	enum EInternalFlags : uint8
	{
		DECAL_OWNER                = BIT(0),   //!< Owns some decals.
		REQUIRES_NEAREST_CUBEMAP   = BIT(1),   //!< Pick nearest cube map.
		UPDATE_DECALS              = BIT(2),   //!< The node changed geometry - decals must be updated.
		REQUIRES_FORWARD_RENDERING = BIT(3),   //!< Special shadow processing needed.
		WAS_INVISIBLE              = BIT(4),   //!< Was invisible last frame.
		WAS_IN_VISAREA             = BIT(5),   //!< Was inside vis-ares last frame.
		WAS_FARAWAY                = BIT(6),   //!< Was considered 'far away' for the purposes of physics deactivation.
		HAS_OCCLUSION_PROXY        = BIT(7),   //!< This node has occlusion proxy.
	};
	typedef uint64 RenderFlagsType;

	struct SUpdateStreamingPriorityContext
	{
		const SRenderingPassInfo* pPassInfo = nullptr;
		float                     distance = 0.0f;
		float                     importance = 0.0f;
		bool                      bFullUpdate = false;
		int                       lod = 0;
	};

public:

	IRenderNode()
	{
		m_dwRndFlags = 0;
		m_ucViewDistRatio = 100;
		m_ucLodRatio = 100;
		m_pOcNode = 0;
		m_nHUDSilhouettesParam = 0;
		m_fWSMaxViewDist = 0;
		m_nInternalFlags = 0;
		m_nMaterialLayers = 0;
		m_pTempData.store(nullptr);
		m_pPrev = m_pNext = nullptr;
		m_cShadowLodBias = 0;
		m_nEditorSelectionID = 0;

		ZeroArray(m_shadowCacheLod);
		ZeroArray(m_shadowCacheLastRendered);
	}

	virtual bool CanExecuteRenderAsJob() { return false; }

	// <interfuscator:shuffle>

	//! Debug info about object.
	virtual const char* GetName() const = 0;
	virtual const char* GetEntityClassName() const = 0;
	virtual string      GetDebugString(char type = 0) const { return ""; }
	virtual float       GetImportance() const               { return 1.f; }

	//! Releases IRenderNode.
	virtual void         ReleaseNode(bool bImmediate = false) { CRY_ASSERT((m_dwRndFlags & ERF_PENDING_DELETE) == 0); delete this; }

	virtual IRenderNode* Clone() const                        { return NULL; }

	//! Sets render node transformation matrix.
	virtual void SetMatrix(const Matrix34& mat) {}

	//! Gets local bounds of the render node.
	virtual void       GetLocalBounds(AABB& bbox) { AABB WSBBox(GetBBox()); bbox = AABB(WSBBox.min - GetPos(true), WSBBox.max - GetPos(true)); }

	virtual Vec3       GetPos(bool bWorldOnly = true) const = 0;
	virtual const AABB GetBBox() const = 0;
	virtual void       FillBBox(AABB& aabb) { aabb = GetBBox(); }
	virtual void       SetBBox(const AABB& WSBBox) = 0;

	//! Changes the world coordinates position of this node by delta.
	//! Don't forget to call this base function when overriding it.
	virtual void OffsetPosition(const Vec3& delta) = 0;

	//! Renders node geometry
	virtual void Render(const struct SRendParams& EntDrawParams, const SRenderingPassInfo& passInfo) = 0;

	//! Hides/disables node in renderer.
	virtual void Hide(bool bHide) { SetRndFlags(ERF_HIDDEN, bHide); }

	//! Gives access to object components.
	virtual IStatObj* GetEntityStatObj(unsigned int nSubPartId = 0, Matrix34A* pMatrix = NULL, bool bReturnOnlyVisible = false);
	virtual void      SetEntityStatObj(IStatObj* pStatObj, const Matrix34A* pMatrix = NULL) {}

	//! Retrieve access to the character instance of the the RenderNode
	virtual ICharacterInstance* GetEntityCharacter(Matrix34A* pMatrix = NULL, bool bReturnOnlyVisible = false) { return 0; }

#if defined(USE_GEOM_CACHES)
	virtual struct IGeomCacheRenderNode* GetGeomCacheRenderNode(unsigned int nSlot, Matrix34A* pMatrix = NULL, bool bReturnOnlyVisible = false) { return NULL; }
#endif

	//! \return IRenderMesh of the object.
	virtual struct IRenderMesh* GetRenderMesh(int nLod) { return 0; };

	//! Allows to adjust default lod distance settings.
	//! If fLodRatio is 100 - default lod distance is used.
	virtual void SetLodRatio(int nLodRatio) { m_ucLodRatio = min(255, max(0, nLodRatio)); }

	//! Get material layers mask.
	virtual uint8 GetMaterialLayers() const { return m_nMaterialLayers; }

	//! Get physical entity.
	virtual struct IPhysicalEntity* GetPhysics() const = 0;
	virtual void                    SetPhysics(IPhysicalEntity* pPhys) = 0;

	//! Physicalizes if it isn't already.
	virtual void CheckPhysicalized() {};

	//! Physicalize the node.
	virtual void Physicalize(bool bInstant = false) {}

	//! Physicalize stat object's foliage.
	virtual bool PhysicalizeFoliage(bool bPhysicalize = true, int iSource = 0, int nSlot = 0) { return false; }

	//! Get physical entity (rope) for a given branch (if foliage is physicalized).
	virtual IPhysicalEntity* GetBranchPhys(int idx, int nSlot = 0) { return 0; }

	//! \return Physicalized foliage, or NULL if it isn't physicalized.
	virtual struct IFoliage* GetFoliage(int nSlot = 0) { return 0; }

	//! Make sure I3DEngine::FreeRenderNodeState(this) is called in destructor of derived class.
	virtual ~IRenderNode() { CRY_ASSERT(!m_pTempData.load()); };

	//! Set override material for this instance.
	virtual void SetMaterial(IMaterial* pMat) = 0;

	//! Queries override material of this instance.
	virtual IMaterial* GetMaterial(Vec3* pHitPos = NULL) const = 0;
	virtual IMaterial* GetMaterialOverride() = 0;

	//! Used by the editor during export.
	virtual void       SetCollisionClassIndex(int tableIndex)          {}

	virtual void       SetStatObjGroupIndex(int nVegetationGroupIndex) {}
	virtual int        GetStatObjGroupId() const                       { return -1; }
	virtual void       SetLayerId(uint16 nLayerId)                     {}
	virtual uint16     GetLayerId()                                    { return 0; }
	virtual float      GetMaxViewDist() = 0;

	virtual EERType    GetRenderNodeType() = 0;
	virtual bool       IsAllocatedOutsideOf3DEngineDLL()             { return GetOwnerEntity() != nullptr; }
	virtual void       Dephysicalize(bool bKeepIfReferenced = false) {}
	virtual void       Dematerialize()                               {}
	virtual void       GetMemoryUsage(ICrySizer* pSizer) const = 0;

	virtual void       Precache()                                                                       {};

	virtual void       UpdateStreamingPriority(const SUpdateStreamingPriorityContext& streamingContext) {};

	virtual const AABB GetBBoxVirtual()                                                                 { return GetBBox(); }

	//	virtual float GetLodForDistance(float fDistance) { return 0; }

	//! Called immediately when render node becomes visible from any thread.
	//! Not reentrant, multiple simultaneous calls to this method on the same rendernode from multiple threads is not supported and should not happen
	virtual void OnRenderNodeBecomeVisibleAsync(SRenderNodeTempData* pTempData, const SRenderingPassInfo& passInfo) {}

	//! Called when RenderNode becomes visible or invisible, can only be called from the Main thread
	virtual void  OnRenderNodeVisible(bool bBecomeVisible) {}

	virtual uint8 GetSortPriority()                        { return 0; }

	//! Object can be used by GI system in several ways.
	enum EGIMode
	{
		eGM_None = 0,                //!< No voxelization.
		eGM_StaticVoxelization,      //!< Incremental or asynchronous lazy voxelization.
		eGM_DynamicVoxelization,     //!< Real-time every-frame voxelization on GPU.
		eGM_HideIfGiIsActive,        //!< Hide this light source if GI is enabled
		eGM_AnalyticalProxy_Soft,    //!< Analytical proxy (with shadow fading)
		eGM_AnalyticalProxy_Hard,    //!< Analytical proxy (no shadow fading)
		eGM_AnalytPostOccluder,      //!< Analytical occluder (used with average light direction)
		eGM_IntegrateIntoTerrain,    //!< Copy object mesh into terrain mesh and render using usual terrain materials
	};

	//! Retrieves the way object is used by GI system.
	virtual EGIMode GetGIMode() const
	{
		return (EGIMode)(((m_dwRndFlags & ERF_GI_MODE_BIT0) ? 1 : 0) | ((m_dwRndFlags & ERF_GI_MODE_BIT1) ? 2 : 0) | ((m_dwRndFlags & ERF_GI_MODE_BIT2) ? 4 : 0));
	}

	virtual void SetMinSpec(RenderFlagsType nMinSpec) { m_dwRndFlags &= ~ERF_SPEC_BITS_MASK; m_dwRndFlags |= (nMinSpec << ERF_SPEC_BITS_SHIFT) & ERF_SPEC_BITS_MASK; };

	//! Allows to adjust default max view distance settings.
	//! If fMaxViewDistRatio is 100 - default max view distance is used.
	virtual void SetViewDistRatio(int nViewDistRatio);
	// </interfuscator:shuffle>

	void CopyIRenderNodeData(IRenderNode* pDest) const
	{
		pDest->m_nHUDSilhouettesParam = m_nHUDSilhouettesParam;
		pDest->m_fWSMaxViewDist = m_fWSMaxViewDist;
		pDest->m_dwRndFlags = m_dwRndFlags;
		//pDest->m_pOcNode						= m_pOcNode;		// Removed to stop the registering from earlying out.
		pDest->m_ucViewDistRatio = m_ucViewDistRatio;
		pDest->m_ucLodRatio = m_ucLodRatio;
		pDest->m_cShadowLodBias = m_cShadowLodBias;
		memcpy(pDest->m_shadowCacheLod, m_shadowCacheLod, sizeof(m_shadowCacheLod));
		ZeroArray(pDest->m_shadowCacheLastRendered);
		pDest->m_nInternalFlags = m_nInternalFlags;
		pDest->m_nMaterialLayers = m_nMaterialLayers;
		//pDestBrush->m_pRNTmpData				//If this is copied from the source render node, there are two
		//	pointers to the same data, and if either is deleted, there will
		//	be a crash when the dangling pointer is used on the other
	}

	//! Rendering flags. (@see ERenderNodeFlags)
	ILINE void            SetRndFlags(RenderFlagsType dwFlags)               { m_dwRndFlags = dwFlags; }
	ILINE void            SetRndFlags(RenderFlagsType dwFlags, bool bEnable) { if (bEnable) SetRndFlags(m_dwRndFlags | dwFlags); else SetRndFlags(m_dwRndFlags & (~dwFlags)); }
	ILINE RenderFlagsType GetRndFlags() const                                { return m_dwRndFlags; }

	//! Object draw frames (set if was drawn).
	ILINE void SetDrawFrame(int nFrameID, int nRecursionLevel)
	{
		// If we can get a pointer atomically it must be valid [until the end of the frame] and we can access it
		const auto pTempData = m_pTempData.load();
		CRY_ASSERT(pTempData);

		int* pDrawFrames = pTempData->userData.lastSeenFrame;
		pDrawFrames[nRecursionLevel] = nFrameID;
	}

	ILINE int GetDrawFrame(int nRecursionLevel = 0) const
	{
		// If we can get a pointer atomically it must be valid [until the end of the frame] and we can access it
		const auto pTempData = m_pTempData.load();
		IF (!pTempData, 0) return 0;

		int* pDrawFrames = pTempData->userData.lastSeenFrame;
		return pDrawFrames[nRecursionLevel];
	}

	//! \return Current VisArea or null if in outdoors or entity was not registered in 3Dengine.
	ILINE IVisArea* GetEntityVisArea() const { return m_pOcNode ? (IVisArea*)(m_pOcNode->GetVisArea()) : NULL; }

	//! \return Current VisArea or null if in outdoors or entity was not registered in 3Dengine.
	struct CTerrainNode* GetEntityTerrainNode() const { return (m_pOcNode && !m_pOcNode->GetVisArea()) ? m_pOcNode->GetTerrainNode() : NULL; }

	//! Makes object visible at any distance.
	ILINE void SetViewDistUnlimited() { SetViewDistRatio(255); }

	//! Retrieves the view distance settings.
	ILINE int GetViewDistRatio() const { return (m_ucViewDistRatio == 255) ? 1000l : m_ucViewDistRatio; }

	//! Retrieves the view distance settings without any value interpretation.
	ILINE int GetViewDistRatioVal() const { return m_ucViewDistRatio; }

	//! \return Max view distance ratio.
	ILINE float GetViewDistRatioNormilized() const
	{
		if (m_ucViewDistRatio == 255)
			return 100.f;
		return (float)m_ucViewDistRatio * 0.01f;
	}

	//! \return Lod distance ratio.
	ILINE int GetLodRatio() const { return m_ucLodRatio; }

	//! \return Lod distance ratio
	ILINE float  GetLodRatioNormalized() const                                              { return 0.01f * m_ucLodRatio; }

	virtual bool GetLodDistances(const SFrameLodInfo& frameLodInfo, float* distances) const { return false; }

	//! Bias value to add to the regular lod
	virtual void SetShadowLodBias(int8 nShadowLodBias) { m_cShadowLodBias = nShadowLodBias; }

	//! \return Lod distance ratio.
	ILINE int GetShadowLodBias() const { return m_cShadowLodBias; }

	//! Sets camera space position of the render node.
	//! Only implemented by few nodes.
	virtual void SetCameraSpacePos(Vec3* pCameraSpacePos) {};

	//! Set material layers mask.
	ILINE void               SetMaterialLayers(uint8 nMtlLayers) { m_nMaterialLayers = nMtlLayers; }

	ILINE int                GetMinSpec() const                  { return (m_dwRndFlags & ERF_SPEC_BITS_MASK) >> ERF_SPEC_BITS_SHIFT; };

	static const ERNListType GetRenderNodeListId(const EERType eRType)
	{
		switch (eRType)
		{
		case eERType_Vegetation:
			return eRNListType_Vegetation;
		case eERType_Brush:
			return eRNListType_Brush;
		case eERType_Decal:
		case eERType_Road:
			return eRNListType_DecalsAndRoads;
		default:
			return eRNListType_Unknown;
		}
	}

	//! Inform 3d engine that permanent render object that captures drawing state of this node is not valid and must be recreated.
	ILINE void   InvalidatePermanentRenderObject() { if (auto pTempData = m_pTempData.load()) pTempData->invalidRenderObjects = pTempData->hasValidRenderObjects.load(); };

	virtual void SetEditorObjectId(uint32 nEditorObjectId)
	{
		// lower 8 bits of the ID is for highlight/selection and other flags
		m_nEditorSelectionID = (nEditorObjectId << 8) | (m_nEditorSelectionID & 0xFF);
		InvalidatePermanentRenderObject();
	}
	virtual void SetEditorObjectInfo(bool bSelected, bool bHighlighted)
	{
		uint32 flags = (bSelected * (1)) | (bHighlighted * (1 << 1));
		m_nEditorSelectionID &= ~(0x3);
		m_nEditorSelectionID |= flags;
		InvalidatePermanentRenderObject();
	}
	// Set a new owner entity
	virtual void     SetOwnerEntity(IEntity* pEntity) { assert(!"Not supported by this object type");  }
	// Retrieve a pointer to the entity who owns this render node.
	virtual IEntity* GetOwnerEntity() const           { return nullptr; }

	//////////////////////////////////////////////////////////////////////////
	// Variables
	//////////////////////////////////////////////////////////////////////////

	void RemoveAndMarkForAutoDeleteTempData()
	{
		// Remove pointer atomically
		SRenderNodeTempData* pTempData = m_pTempData.exchange(nullptr);

		// Keep the contents of the object valid, but schedule it for removal at the end of the frame
		if (pTempData)
			pTempData->MarkForAutoDelete();
	}

public:

	//! Every sector has linked list of IRenderNode objects.
	IRenderNode* m_pNext, * m_pPrev;

	//! Current objects tree cell.
	IOctreeNode* m_pOcNode;

	//! Render flags (@see ERenderNodeFlags)
	RenderFlagsType m_dwRndFlags;

	//! Pointer to temporary data allocated only for currently visible objects.
	std::atomic<SRenderNodeTempData*> m_pTempData;
	CryRWLock                         m_manipulationLock;	
	int                               m_manipulationFrame = -1;

	//! Hud silhouette parameter, default is black with alpha zero
	uint32 m_nHUDSilhouettesParam;

	//! Max view distance.
	float m_fWSMaxViewDist;

	//! Flags for render node internal usage, one or more bits from EInternalFlags.
	uint8 m_nInternalFlags;

	//! Max view distance settings.
	uint8 m_ucViewDistRatio;

	//! LOD settings.
	uint8 m_ucLodRatio;

	//! Material layers bitmask -> which material layers are active.
	uint8 m_nMaterialLayers;

	//! Selection ID used to map the rendernode to a baseobject in the editor, or differentiate between objects
	//! in highlight framebuffer
	//! This ID is split in two parts. The low 8 bits store flags such as selection and highlight state
	//! The high 24 bits store the actual ID of the object. This need not be the same as CryGUID,
	//! though the CryGUID could be used to generate it
	uint32 m_nEditorSelectionID;

	//! Used to request visiting of the node during one-pass traversal
	uint32 m_onePassTraversalFrameId = 0;
	uint32 m_onePassTraversalShadowCascades = 0;
};

inline void IRenderNode::SetViewDistRatio(int nViewDistRatio)
{
	nViewDistRatio = SATURATEB(nViewDistRatio);
	if (m_ucViewDistRatio != nViewDistRatio)
	{
		m_ucViewDistRatio = nViewDistRatio;
		if (m_pOcNode)
			m_pOcNode->MarkAsUncompiled(GetRenderNodeListId(GetRenderNodeType()));
	}
}

///////////////////////////////////////////////////////////////////////////////
inline IStatObj* IRenderNode::GetEntityStatObj(unsigned int nSubPartId, Matrix34A* pMatrix, bool bReturnOnlyVisible)
{
	return 0;
}

//! We must use interfaces instead of unsafe type casts and unnecessary includes.
struct IVegetation : public IRenderNode
{
	virtual float GetScale(void) const = 0;
};

struct IBrush : public IRenderNode
{
	virtual const Matrix34& GetMatrix() const = 0;
	virtual void            SetDrawLast(bool enable) = 0;
	virtual void            DisablePhysicalization(bool bDisable) = 0; //!< This render node will not be physicalized
	virtual float           GetScale() const = 0;

	// Hide mask disable individual sub-objects rendering in the compound static objects
	// Only implemented by few nodes.
	virtual void SetSubObjectHideMask(hidemask subObjHideMask) {};
};

//! \cond INTERNAL
struct SVegetationSpriteInfo
{
	Sphere                             sp;
	float                              fScaleH;
	float                              fScaleV;
	struct SSectorTextureSet*          pTerrainTexInfo;
	struct SVegetationSpriteLightInfo* pLightInfo;
	uint8                              ucAlphaTestRef;
	uint8                              ucDissolveOut;
	uint8                              ucShow3DModel;

	ColorB                             terrainColor;

	//! Used only in case of sprite texture update.
	uint8                  ucSlotId;
	struct IStatInstGroup* pStatInstGroup;

	//! Used only by 3DEngine.
	const IRenderNode* pVegetation;

	void               GetMemoryUsage(ICrySizer* pSizer) const { /*nothing*/ }
};

const int FAR_TEX_COUNT = 16;             //!< Number of sprites per object.
const int FAR_TEX_ANGLE = (360 / FAR_TEX_COUNT);
const int FAR_TEX_HAL_ANGLE = (256 / FAR_TEX_COUNT) / 2;

//! Groups sprite gen data.
struct SVegetationSpriteLightInfo
{
	SVegetationSpriteLightInfo() { m_vSunDir = Vec3(0, 0, 0); m_MipFactor = 0.0f; }

	float        m_MipFactor;
	IDynTexture* m_pDynTexture;

	void         SetLightingData(const Vec3& vSunDir)
	{
		m_vSunDir = vSunDir;
	}

	//! \param vSunDir Should be normalized.
	bool IsEqual(const Vec3& vSunDir, const float fDirThreshold) const
	{
		return IsEquivalent(m_vSunDir, vSunDir, fDirThreshold);
	}

private:

	Vec3 m_vSunDir;                //!< Normalized sun direction.
};
//! \endcond

struct ILightSource : public IRenderNode
{
	// <interfuscator:shuffle>
	virtual void                     SetLightProperties(const SRenderLight& light) = 0;
	virtual SRenderLight&            GetLightProperties() = 0;
	virtual const Matrix34&          GetMatrix() = 0;
	virtual struct ShadowMapFrustum* GetShadowFrustum(int nId = 0) = 0;
	virtual bool                     IsLightAreasVisible() = 0;
	virtual void                     SetCastingException(IRenderNode* pNotCaster) = 0;
	// </interfuscator:shuffle>
};

struct SCloudBlockerProperties
{
	f32  decayStart;
	f32  decayEnd;
	f32  decayInfluence;
	bool bScreenspace;
};

//! Interface to the Cloud Blocker Render Node object.
struct ICloudBlockerRenderNode : public IRenderNode
{
	// <interfuscator:shuffle>
	virtual void SetProperties(const SCloudBlockerProperties& properties) = 0;
	// </interfuscator:shuffle>
};

//! Interface to the Road Render Node object.
struct IRoadRenderNode : public IRenderNode
{
	// <interfuscator:shuffle>
	virtual void SetVertices(const Vec3* pVerts, int nVertsNum, float fTexCoordBegin, float fTexCoordEnd, float fTexCoordBeginGlobal, float fTexCoordEndGlobal) = 0;
	virtual void SetSortPriority(uint8 sortPrio) = 0;
	virtual void SetIgnoreTerrainHoles(bool bVal) = 0;
	virtual void SetPhysicalize(bool bVal) = 0;
	virtual void GetClipPlanes(Plane* pPlanes, int nPlanesNum, int nVertId = 0) = 0;
	virtual void GetTexCoordInfo(float* pTexCoordInfo) = 0;
	// </interfuscator:shuffle>
};

//! Interface to the Breakable Glass Render Node object.
struct SBreakableGlassInitParams;
struct SBreakableGlassUpdateParams;
struct SBreakableGlassState;
struct SBreakableGlassCVars;
struct SGlassPhysFragment;

struct IBreakableGlassRenderNode : public IRenderNode
{
	// <interfuscator:shuffle>
	virtual bool   InitialiseNode(const SBreakableGlassInitParams& params, const Matrix34& matrix) = 0;
	virtual void   SetId(const uint16 id) = 0;
	virtual uint16 GetId() = 0;

	virtual void   Update(SBreakableGlassUpdateParams& params) = 0;
	virtual bool   HasGlassShattered() = 0;
	virtual bool   HasActiveFragments() = 0;
	virtual void   ApplyImpactToGlass(const EventPhysCollision* pPhysEvent) = 0;
	virtual void   ApplyExplosionToGlass(const EventPhysCollision* pPhysEvent) = 0;

	virtual void   DestroyPhysFragment(SGlassPhysFragment* pPhysFrag) = 0;

	virtual void   SetCVars(const SBreakableGlassCVars* pCVars) = 0;
	// </interfuscator:shuffle>
};

//! Interface to the Voxel Object Render Node object.
struct IVoxelObject : public IRenderNode
{
	// <interfuscator:shuffle>
	virtual struct IMemoryBlock* GetCompiledData(EEndian eEndian) = 0;
	virtual void                 SetCompiledData(void* pData, int nSize, uint8 ucChildId, EEndian eEndian) = 0;
	virtual void                 SetObjectName(const char* pName) = 0;
	virtual void                 SetMatrix(const Matrix34& mat) = 0;
	virtual bool                 ResetTransformation() = 0;
	virtual void                 InterpolateVoxelData() = 0;
	virtual void                 SetFlags(int nFlags) = 0;
	virtual void                 Regenerate() = 0;
	virtual void                 CopyHM() = 0;
	virtual bool                 IsEmpty() = 0;
	// </interfuscator:shuffle>
};

struct SFogVolumeProperties;

struct IFogVolumeRenderNode : public IRenderNode
{
	enum eFogVolumeType
	{
		eFogVolumeType_Ellipsoid = 0,
		eFogVolumeType_Box       = 1,

		eFogVolumeType_Count,
	};

	// <interfuscator:shuffle>
	virtual void            SetFogVolumeProperties(const SFogVolumeProperties& properties) = 0;
	virtual const Matrix34& GetMatrix() const = 0;

	virtual void            FadeGlobalDensity(float fadeTime, float newGlobalDensity) = 0;
	// </interfuscator:shuffle>
};

//! IFogVolumeRenderNode is an interface to the Fog Volume Render Node object.
struct SFogVolumeProperties
{
	// Common parameters.
	// Center position & rotation values are taken from the entity matrix.

	int    m_volumeType = IFogVolumeRenderNode::eFogVolumeType_Box;
	Vec3   m_size = Vec3(1.f);
	ColorF m_color = ColorF(1, 1, 1, 1);
	bool   m_useGlobalFogColor = false;
	bool   m_ignoresVisAreas = false;
	bool   m_affectsThisAreaOnly = true;
	float  m_globalDensity = 1.f;
	float  m_densityOffset = 0.f;
	float  m_softEdges = 1.f;
	float  m_fHDRDynamic = 0.f;               //!< 0 to get the same results in LDR, <0 to get darker, >0 to get brighter.
	float  m_nearCutoff = 0.f;

	float  m_heightFallOffDirLong = 0.f;        //!< Height based fog specifics.
	float  m_heightFallOffDirLati = 0.f;        //!< Height based fog specifics.
	float  m_heightFallOffShift = 0.f;          //!< Height based fog specifics.
	float  m_heightFallOffScale = 1.f;          //!< Height based fog specifics.

	float  m_rampStart = 0.f;
	float  m_rampEnd = 50.f;
	float  m_rampInfluence = 0.f;
	float  m_windInfluence = 1.f;
	float  m_densityNoiseScale = 0.f;
	float  m_densityNoiseOffset = 0.f;
	float  m_densityNoiseTimeFrequency = 0.f;
	Vec3   m_densityNoiseFrequency = Vec3(1.f);
	Vec3   m_emission = Vec3(1.f);
};

struct SDecalProperties
{
	SDecalProperties()
	{
		m_projectionType = ePlanar;
		m_sortPrio = 0;
		m_deferred = false;
		m_pos = Vec3(0.0f, 0.0f, 0.0f);
		m_normal = Vec3(0.0f, 0.0f, 1.0f);
		m_explicitRightUpFront = Matrix33::CreateIdentity();
		m_radius = 1.0f;
		m_depth = 1.0f;
	}

	enum EProjectionType
	{
		ePlanar,
		eProjectOnStaticObjects,
		eProjectOnTerrain,
		eProjectOnTerrainAndStaticObjects
	};

	EProjectionType m_projectionType;
	uint8           m_sortPrio;
	uint8           m_deferred;
	Vec3            m_pos;
	Vec3            m_normal;
	Matrix33        m_explicitRightUpFront;
	float           m_radius;
	float           m_depth;
	const char*     m_pMaterialName;
};

//! Interface to the Decal Render Node object.
struct IDecalRenderNode : public IRenderNode
{
	// <interfuscator:shuffle>
	virtual void                    SetDecalProperties(const SDecalProperties& properties) = 0;
	virtual const SDecalProperties* GetDecalProperties() const = 0;
	virtual const Matrix34& GetMatrix() = 0;
	virtual void            CleanUpOldDecals() = 0;
	// </interfuscator:shuffle>
};

//! Interface to the Water Volume Render Node object.
struct IWaterVolumeRenderNode : public IRenderNode
{
	enum EWaterVolumeType
	{
		eWVT_Unknown,
		eWVT_Ocean,
		eWVT_Area,
		eWVT_River
	};

	// <interfuscator:shuffle>
	//! Sets whether the render node is attached to a parent entity.
	//! This must be called right after the object construction if it is the case.
	//! Only supported for Areas (not rivers or ocean).
	virtual void             SetAreaAttachedToEntity() = 0;

	virtual void             SetFogDensity(float fogDensity) = 0;
	virtual float            GetFogDensity() const = 0;
	virtual void             SetFogColor(const Vec3& fogColor) = 0;
	virtual void             SetFogColorAffectedBySun(bool enable) = 0;
	virtual void             SetFogShadowing(float fogShadowing) = 0;

	virtual void             SetCapFogAtVolumeDepth(bool capFog) = 0;
	virtual void             SetVolumeDepth(float volumeDepth) = 0;
	virtual void             SetStreamSpeed(float streamSpeed) = 0;

	virtual void             SetCaustics(bool caustics) = 0;
	virtual void             SetCausticIntensity(float causticIntensity) = 0;
	virtual void             SetCausticTiling(float causticTiling) = 0;
	virtual void             SetCausticHeight(float causticHeight) = 0;
	virtual void             SetAuxPhysParams(pe_params_area*) = 0;

	virtual void             CreateOcean(uint64 volumeID, /* TBD */ bool keepSerializationParams = false) = 0;
	virtual void             CreateArea(uint64 volumeID, const Vec3* pVertices, unsigned int numVertices, const Vec2& surfUVScale, const Plane& fogPlane, bool keepSerializationParams = false) = 0;
	virtual void             CreateRiver(uint64 volumeID, const Vec3* pVertices, unsigned int numVertices, float uTexCoordBegin, float uTexCoordEnd, const Vec2& surfUVScale, const Plane& fogPlane, bool keepSerializationParams = false) = 0;

	virtual void             SetAreaPhysicsArea(const Vec3* pVertices, unsigned int numVertices, bool keepSerializationParams = false) = 0;
	virtual void             SetRiverPhysicsArea(const Vec3* pVertices, unsigned int numVertices, bool keepSerializationParams = false) = 0;

	virtual IPhysicalEntity* SetAndCreatePhysicsArea(const Vec3* pVertices, unsigned int numVertices) = 0;
	// </interfuscator:shuffle>
};

//! Interface to the Water Wave Render Node object.
//! \cond INTERNAL
struct SWaterWaveParams
{
	SWaterWaveParams() : m_fSpeed(5.0f), m_fSpeedVar(2.0f), m_fLifetime(8.0f), m_fLifetimeVar(2.0f),
		m_fHeight(0.75f), m_fHeightVar(0.4f), m_fPosVar(5.0f), m_fCurrLifetime(-1.0f), m_fCurrFrameLifetime(-1.0f),
		m_fCurrSpeed(0.0f), m_fCurrHeight(1.0f)
	{

	}

	Vec3  m_pPos;
	float m_fSpeed, m_fSpeedVar;
	float m_fLifetime, m_fLifetimeVar;
	float m_fHeight, m_fHeightVar;
	float m_fPosVar;

	float m_fCurrLifetime;
	float m_fCurrFrameLifetime;
	float m_fCurrSpeed;
	float m_fCurrHeight;
};

struct IWaterWaveRenderNode : public IRenderNode
{
	// <interfuscator:shuffle>
	virtual void                    Create(uint64 nID, const Vec3* pVertices, uint32 nVertexCount, const Vec2& pUVScale, const Matrix34& pWorldTM) = 0;
	virtual void                    SetParams(const SWaterWaveParams& pParams) = 0;
	virtual const SWaterWaveParams& GetParams() const = 0;
	// </interfuscator:shuffle>
};
// \endcond

//! \cond INTERNAL
//! Interface to the Distance Cloud Render Node object.
struct SDistanceCloudProperties
{
	Vec3        m_pos;
	float       m_sizeX;
	float       m_sizeY;
	float       m_rotationZ;
	const char* m_pMaterialName;
};
//! \endcond INTERNAL

struct IDistanceCloudRenderNode : public IRenderNode
{
	virtual void SetProperties(const SDistanceCloudProperties& properties) = 0;
};

//////////////////////////////////////////////////////////////////////////
struct IRopeRenderNode : public IRenderNode
{
	enum ERopeParamFlags
	{
		eRope_BindEndPoints          = 0x0001,  //!< Bind rope at both end points.
		eRope_CheckCollisinos        = 0x0002,  //!< Rope will check collisions.
		eRope_Subdivide              = 0x0004,  //!< Rope will be subdivided in physics.
		eRope_Smooth                 = 0x0008,  //!< Rope will be smoothed after physics.
		eRope_NoAttachmentCollisions = 0x0010,  //!< Rope will ignore collisions against the objects to which it is attached.
		eRope_Nonshootable           = 0x0020,  //!< Rope cannot be broken by shooting.
		eRope_Disabled               = 0x0040,  //!< simulation is completely disabled.
		eRope_NoPlayerCollisions     = 0x0080,  //!< explicit control over collisions with players.
		eRope_StaticAttachStart      = 0x0100,  //!< attach start point to the 'world'.
		eRope_StaticAttachEnd        = 0x0200,  //!< attach end point to the 'world'.
		eRope_CastShadows            = 0x0400,  //!< self-explanatory.
		eRope_Awake                  = 0x0800,  //!< Rope will be awake initially.
	};
	struct SRopeParams
	{
		int   nFlags = eRope_CheckCollisinos; //!< ERopeParamFlags.

		float fThickness = 0.02f;

		//! Radius for the end points anchors that bind rope to objects in world.
		float fAnchorRadius = 0.1f;

		//////////////////////////////////////////////////////////////////////////
		// Rendering/Tessellation.
		int   nNumSegments = 8;
		int   nNumSides = 4;
		float fTextureTileU = 1.f;
		float fTextureTileV = 10.f;
		//////////////////////////////////////////////////////////////////////////

		//////////////////////////////////////////////////////////////////////////
		// Rope Physical parameters.
		int   nPhysSegments = 8;
		int   nMaxSubVtx = 3;

		float mass = 1.f;        //!< Rope mass. if mass is 0 it will be static.
		float tension = 0.5f;
		float friction = 2.f;
		float frictionPull = 2.f;

		Vec3  wind = ZERO;
		float windVariance = 0.f;
		float airResistance = 0.f;
		float waterResistance = 0.f;

		float jointLimit = 0.f;
		float maxForce = 0.f;

		int   nMaxIters = 650;
		float maxTimeStep = 0.25f;
		float stiffness = 10.f;
		float hardness = 20.f;
		float damping = 0.2f;
		float sleepSpeed = 0.04f;
	};
	struct SEndPointLink
	{
		IPhysicalEntity* pPhysicalEntity;
		Vec3             offset;
		int              nPartId;
	};
	struct SRopeAudioParams
	{
		SRopeAudioParams() = default;

		CryAudio::ControlId      startTrigger = CryAudio::InvalidControlId;
		CryAudio::ControlId      stopTrigger = CryAudio::InvalidControlId;
		CryAudio::ControlId      angleParameter = CryAudio::InvalidControlId;
		CryAudio::EOcclusionType occlusionType = CryAudio::EOcclusionType::Ignore;
		int                      segementToAttachTo = 1;
		float                    offset = 0.0f;
	};

	// <interfuscator:shuffle>
	virtual void                                SetName(const char* sNodeName) = 0;

	virtual void                                SetParams(const SRopeParams& params) = 0;
	virtual const IRopeRenderNode::SRopeParams& GetParams() const = 0;

	virtual void                                SetPoints(const Vec3* pPoints, int nCount) = 0;
	virtual int                                 GetPointsCount() const = 0;
	virtual const Vec3*                         GetPoints() const = 0;

	virtual void                                LinkEndPoints() = 0;
	virtual uint32                              GetLinkedEndsMask() = 0;

	virtual void                                LinkEndEntities(IPhysicalEntity* pStartEntity, IPhysicalEntity* pEndEntity) = 0;

	//! Retrieves information about linked objects at the end points, links must be a pointer to the 2 SEndPointLink structures.
	virtual void GetEndPointLinks(SEndPointLink* links) = 0;

	//! Callback from physics.
	virtual void OnPhysicsPostStep() = 0;

	virtual void ResetPoints() = 0;

	virtual void DisableAudio() = 0;
	virtual void SetAudioParams(SRopeAudioParams const& audioParams) = 0;
	// </interfuscator:shuffle>
};

#if defined(USE_GEOM_CACHES)
struct IGeomCacheRenderNode : public IRenderNode
{
	virtual bool LoadGeomCache(const char* sGeomCacheFileName) = 0;

	//! Gets the geometry cache that is rendered.
	virtual IGeomCache* GetGeomCache() const = 0;

	//! Sets the time in the animation for the current frame.
	//! \note You should start streaming before calling this.
	virtual void SetPlaybackTime(const float time) = 0;

	//! Get the current playback time.
	virtual float GetPlaybackTime() const = 0;

	//! Check if cache is streaming.
	virtual bool IsStreaming() const = 0;

	//! Need to start streaming before playback, otherwise there will be stalls.
	virtual void StartStreaming(const float time = 0.0f) = 0;

	//! Stops streaming and trashes the buffers.
	virtual void StopStreaming() = 0;

	//! Checks if looping is enabled.
	virtual bool IsLooping() const = 0;

	//! Enable/disable looping playback.
	virtual void SetLooping(const bool bEnable) = 0;

	//! Gets time delta from current playback position to last ready to play frame.
	virtual float GetPrecachedTime() const = 0;

	//! Check if bounds changed since last call to this function.
	virtual bool DidBoundsChange() = 0;

	//! Set stand in CGFs and distance.
	virtual void SetStandIn(const char* pFilePath, const char* pMaterial) = 0;
	virtual void SetFirstFrameStandIn(const char* pFilePath, const char* pMaterial) = 0;
	virtual void SetLastFrameStandIn(const char* pFilePath, const char* pMaterial) = 0;
	virtual void SetStandInDistance(const float distance) = 0;

	//! Set distance at which cache will start streaming automatically (0 means no auto streaming).
	virtual void SetStreamInDistance(const float distance) = 0;

	//! Start/Stop drawing the cache.
	virtual void SetDrawing(bool bDrawing) = 0;

	//! Debug draw geometry.
	virtual void DebugDraw(const struct SGeometryDebugDrawInfo& info, uint nodeIndex = 0) const = 0;

	//! Ray intersection against cache.
	virtual bool RayIntersection(struct SRayHitInfo& hitInfo, IMaterial* pCustomMtl = NULL, uint* pHitNodeIndex = NULL) const = 0;

	//! Get node information.
	virtual uint     GetNodeCount() const = 0;
	virtual Matrix34 GetNodeTransform(const uint nodeIndex) const = 0;

	//! Node name is only stored in editor.
	virtual const char* GetNodeName(const uint nodeIndex) const = 0;
	virtual uint32      GetNodeNameHash(const uint nodeIndex) const = 0;

	//!< \return false if cache isn't loaded yet or index is out of range.
	virtual bool IsNodeDataValid(const uint nodeIndex) const = 0;

	virtual void InitPhysicalEntity(IPhysicalEntity* pPhysicalEntity, const pe_articgeomparams& params) = 0;
};
#endif

//! Interface to the Character Rendering
struct ICharacterRenderNode : public IRenderNode
{
	virtual void SetCharacter(struct ICharacterInstance* pCharacter) = 0;
};
