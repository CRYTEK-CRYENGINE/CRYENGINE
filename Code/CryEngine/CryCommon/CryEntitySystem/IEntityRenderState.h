// Copyright 2001-2016 Crytek GmbH / Crytek Group. All rights reserved.

#ifndef IRenderNodeSTATE_H
#define IRenderNodeSTATE_H

#define SUPP_HMAP_OCCL

struct IMaterial;
struct IVisArea;
struct SEntityEvent;
struct SRenderingPassInfo;
struct IGeomCache;
struct SRendItemSorter;
struct SFrameLodInfo;

enum EERType
{
	eERType_NotRenderNode,
	eERType_Brush,
	eERType_Vegetation,
	eERType_Light,
	eERType_Cloud,
	eERType_Dummy_1, //!< Used to be eERType_VoxelObject, preserve order for compatibility.
	eERType_FogVolume,
	eERType_Decal,
	eERType_ParticleEmitter,
	eERType_WaterVolume,
	eERType_WaterWave,
	eERType_Road,
	eERType_DistanceCloud,
	eERType_VolumeObject,
	eERType_Dummy_0, //!< Used to be eERType_AutoCubeMap, preserve order for compatibility.
	eERType_Rope,
	eERType_PrismObject,
	eERType_TerrainSector,
	eERType_Dummy_2, //!< Used to be eERType_LightPropagationVolume, preserve order for compatibility.
	eERType_RenderProxy,
	eERType_GameEffect,
	eERType_BreakableGlass,
	eERType_Dummy_3, //!< Used to be eERType_LightShape, preserve order for compatibility.
	eERType_MergedMesh,
	eERType_GeomCache,
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

// RenderNode flags

#define ERF_GOOD_OCCLUDER              BIT(0)
#define ERF_PROCEDURAL                 BIT(1)
#define ERF_CLONE_SOURCE               BIT(2)  // set if this object was cloned from another one
#define ERF_CASTSHADOWMAPS             BIT(3)  // if you ever set this flag, be sure also to set ERF_HAS_CASTSHADOWMAPS
#define ERF_RENDER_ALWAYS              BIT(4)
#define ERF_DYNAMIC_DISTANCESHADOWS    BIT(5)
#define ERF_HIDABLE                    BIT(6)
#define ERF_HIDABLE_SECONDARY          BIT(7)
#define ERF_HIDDEN                     BIT(8)
#define ERF_SELECTED                   BIT(9)
#define ERF_PROCEDURAL_ENTITY          BIT(10)  // this is an object generated at runtime which has a limited lifetime (matches procedural entity)
#define ERF_OUTDOORONLY                BIT(11)
#define ERF_NODYNWATER                 BIT(12)
#define ERF_EXCLUDE_FROM_TRIANGULATION BIT(13)
#define ERF_REGISTER_BY_BBOX           BIT(14)
#define ERF_PICKABLE                   BIT(15)
#define ERF_VOXELIZE_STATIC            BIT(16)
#define ERF_NO_PHYSICS                 BIT(17)
#define ERF_NO_DECALNODE_DECALS        BIT(18)
#define ERF_REGISTER_BY_POSITION       BIT(19)
#define ERF_STATIC_INSTANCING          BIT(20)
#define ERF_RECVWIND                   BIT(21)

//! Collision proxy is a special object that is only visible in editor and used for physical collisions with player and vehicles.
#define ERF_COLLISION_PROXY    BIT(22)

#define ERF_VOXELIZE_DYNAMIC   BIT(23)
#define ERF_SPEC_BIT0          BIT(24)                                         // Bit0 of min config specification.
#define ERF_SPEC_BIT1          BIT(25)                                         // Bit1 of min config specification.
#define ERF_SPEC_BIT2          BIT(26)                                         // Bit2 of min config specification.
#define ERF_SPEC_BITS_MASK     (ERF_SPEC_BIT0 | ERF_SPEC_BIT1 | ERF_SPEC_BIT2) // Bit mask of the min spec bits.
#define ERF_SPEC_BITS_SHIFT    (24)                                            // Bit offset of the ERF_SPEC_BIT0.
#define ERF_RAYCAST_PROXY      BIT(27)                                         // raycast proxy is only used for raycasting
#define ERF_HUD                BIT(28)                                         // Hud object that can avoid some visibility tests
#define ERF_RAIN_OCCLUDER      BIT(29)                                         // Is used for rain occlusion map
#define ERF_HAS_CASTSHADOWMAPS BIT(30)                                         // at one point had ERF_CASTSHADOWMAPS set
#define ERF_ACTIVE_LAYER       BIT(31)                                         // the node is on a currently active layer

struct IShadowCaster
{
	// <interfuscator:shuffle>
	virtual ~IShadowCaster(){}
	virtual bool                       HasOcclusionmap(int nLod, IRenderNode* pLightOwner)           { return false; }
	virtual CLodValue                  ComputeLod(int wantedLod, const SRenderingPassInfo& passInfo) { return CLodValue(wantedLod); }
	virtual void                       Render(const SRendParams& RendParams, const SRenderingPassInfo& passInfo) = 0;
	virtual const AABB                 GetBBoxVirtual() = 0;
	virtual void                       FillBBox(AABB& aabb) = 0;
	virtual struct ICharacterInstance* GetEntityCharacter(unsigned int nSlot, Matrix34A* pMatrix = NULL, bool bReturnOnlyVisible = false) = 0;
	virtual EERType                    GetRenderNodeType() = 0;
	virtual bool                       IsRenderNode() { return true; }
	// </interfuscator:shuffle>
	uint8                              m_cStaticShadowLod;
};

struct IOctreeNode
{
public:
	struct CTerrainNode* GetTerrainNode() const                    { return (CTerrainNode*) (m_pTerrainNode & ~0x1); }
	void                 SetTerrainNode(struct CTerrainNode* node) { m_pTerrainNode = (m_pTerrainNode & 0x1) | ((INT_PTR) node); }

	// If true - this node needs to be recompiled for example update nodes max view distance.
	bool IsCompiled() const         { return (bool) (m_pTerrainNode & 0x1); }
	void SetCompiled(bool compiled) { m_pTerrainNode = ((int) compiled) | (m_pTerrainNode & ~0x1); }

	struct CVisArea* m_pVisArea;

private:
	INT_PTR m_pTerrainNode;
};

struct SLodDistDissolveTransitionState
{
	float fStartDist;
	int8  nOldLod;
	int8  nNewLod;
	bool  bFarside;
};

struct SLightInfo
{
	bool operator==(const SLightInfo& other) const
	{ return other.vPos.IsEquivalent(vPos, 0.1f) && fabs(other.fRadius - fRadius) < 0.1f; }
	Vec3  vPos;
	float fRadius;
	bool  bAffecting;
};

struct IRenderNode : public IShadowCaster
{
	enum EInternalFlags
	{
		DECAL_OWNER                = BIT(0),   //!< Owns some decals.
		REQUIRES_NEAREST_CUBEMAP   = BIT(1),   //!< Pick nearest cube map.
		UPDATE_DECALS              = BIT(2),   //!< The node changed geometry - decals must be updated.
		REQUIRES_FORWARD_RENDERING = BIT(3),   //!< Special shadow processing needed.
		WAS_INVISIBLE              = BIT(4),   //!< Was invisible last frame.
		WAS_IN_VISAREA             = BIT(5),   //!< Was inside vis-ares last frame.
		WAS_FARAWAY                = BIT(6),   //!< Was considered 'far away' for the purposes of physics deactivation.
		HAS_OCCLUSION_PROXY        = BIT(7),   //!< This node has occlusion proxy.
		PERMANENT_RO_INVALID       = BIT(8)    //!< If this node uses permanent render object, it is not valid anymore and must be recreated.
	};

	IRenderNode()
	{
		m_dwRndFlags = 0;
		m_ucViewDistRatio = 100;
		m_ucLodRatio = 100;
		m_pOcNode = 0;
		m_fWSMaxViewDist = 0;
		m_nInternalFlags = 0;
		m_nMaterialLayers = 0;
		m_pTempData = NULL;
		m_pPrev = m_pNext = NULL;
		m_nSID = 0;
		m_cShadowLodBias = 0;
		m_cStaticShadowLod = 0;
	}

	virtual bool CanExecuteRenderAsJob() { return false; }

	// <interfuscator:shuffle>

	//! Debug info about object.
	virtual const char* GetName() const = 0;
	virtual const char* GetEntityClassName() const = 0;
	virtual string      GetDebugString(char type = 0) const { return ""; }
	virtual float       GetImportance() const               { return 1.f; }

	//! Releases IRenderNode.
	virtual void         ReleaseNode(bool bImmediate = false) { delete this; }

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
	virtual struct IStatObj*             GetEntityStatObj(unsigned int nPartId = 0, unsigned int nSubPartId = 0, Matrix34A* pMatrix = NULL, bool bReturnOnlyVisible = false);
	virtual IMaterial*                   GetEntitySlotMaterial(unsigned int nPartId, bool bReturnOnlyVisible = false, bool* pbDrawNear = NULL)  { return NULL; }
	virtual void                         SetEntityStatObj(unsigned int nSlot, IStatObj* pStatObj, const Matrix34A* pMatrix = NULL)              {};
	virtual struct ICharacterInstance*   GetEntityCharacter(unsigned int nSlot, Matrix34A* pMatrix = NULL, bool bReturnOnlyVisible = false)     { return 0; }
#if defined(USE_GEOM_CACHES)
	virtual struct IGeomCacheRenderNode* GetGeomCacheRenderNode(unsigned int nSlot, Matrix34A* pMatrix = NULL, bool bReturnOnlyVisible = false) { return NULL; }
#endif
	virtual int                          GetSlotCount() const                                                                                   { return 1; }

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
	virtual ~IRenderNode() { assert(!m_pTempData); };

	//! Set override material for this instance.
	virtual void SetMaterial(IMaterial* pMat) = 0;

	//! Queries override material of this instance.
	virtual IMaterial* GetMaterial(Vec3* pHitPos = NULL) const = 0;
	virtual IMaterial* GetMaterialOverride() = 0;

	//! Used by the editor during export.
	virtual void       SetCollisionClassIndex(int tableIndex)          {}

	virtual int        GetEditorObjectId()                             { return 0; }
	virtual void       SetEditorObjectId(int nEditorObjectId)          {}
	virtual void       SetStatObjGroupIndex(int nVegetationGroupIndex) {}
	virtual int        GetStatObjGroupId() const                       { return -1; }
	virtual void       SetLayerId(uint16 nLayerId)                     {}
	virtual uint16     GetLayerId()                                    { return 0; }
	virtual float      GetMaxViewDist() = 0;

	virtual EERType    GetRenderNodeType() = 0;
	virtual bool       IsAllocatedOutsideOf3DEngineDLL()             { return GetRenderNodeType() == eERType_RenderProxy; }
	virtual void       Dephysicalize(bool bKeepIfReferenced = false) {}
	virtual void       Dematerialize()                               {}
	virtual void       GetMemoryUsage(ICrySizer* pSizer) const = 0;

	virtual void       Precache()       {};

	virtual const AABB GetBBoxVirtual() { return GetBBox(); }

	//	virtual float GetLodForDistance(float fDistance) { return 0; }

	virtual void  OnRenderNodeBecomeVisible(const SRenderingPassInfo& passInfo) {}
	virtual void  OnEntityEvent(IEntity* pEntity, SEntityEvent const& event)    {}

	virtual bool  IsMovableByGame() const                                       { return false; }

	virtual uint8 GetSortPriority()                                             { return 0; }

#if defined(FEATURE_SVO_GI)
	//! Types of voxelization for objects and lights.
	enum EVoxMode
	{
		VM_None = 0, //!< No voxelization.
		VM_Static,   //!< Incremental or asynchronous lazy voxelization.
		VM_Dynamic,  //!< Real-time every-frame voxelization on GPU.
	};
	virtual EVoxMode GetVoxMode() { return VM_None; }
#endif

	virtual void SetMinSpec(int nMinSpec) { m_dwRndFlags &= ~ERF_SPEC_BITS_MASK; m_dwRndFlags |= (nMinSpec << ERF_SPEC_BITS_SHIFT) & ERF_SPEC_BITS_MASK; };

	//! Allows to adjust default max view distance settings.
	//! If fMaxViewDistRatio is 100 - default max view distance is used.
	virtual void SetViewDistRatio(int nViewDistRatio);
	// </interfuscator:shuffle>

	void CopyIRenderNodeData(IRenderNode* pDest) const
	{
		pDest->m_fWSMaxViewDist = m_fWSMaxViewDist;
		pDest->m_dwRndFlags = m_dwRndFlags;
		//pDest->m_pOcNode						= m_pOcNode;		// Removed to stop the registering from earlying out.
		pDest->m_ucViewDistRatio = m_ucViewDistRatio;
		pDest->m_ucLodRatio = m_ucLodRatio;
		pDest->m_cShadowLodBias = m_cShadowLodBias;
		pDest->m_cStaticShadowLod = m_cStaticShadowLod;
		pDest->m_nInternalFlags = m_nInternalFlags;
		pDest->m_nMaterialLayers = m_nMaterialLayers;
		//pDestBrush->m_pRNTmpData				//If this is copied from the source render node, there are two
		//	pointers to the same data, and if either is deleted, there will
		//	be a crash when the dangling pointer is used on the other
	}

	//! Rendering flags.
	ILINE void         SetRndFlags(unsigned int dwFlags)               { m_dwRndFlags = dwFlags; }
	ILINE void         SetRndFlags(unsigned int dwFlags, bool bEnable) { if (bEnable) SetRndFlags(m_dwRndFlags | dwFlags); else SetRndFlags(m_dwRndFlags & (~dwFlags)); }
	ILINE unsigned int GetRndFlags() const                             { return m_dwRndFlags; }

	//! Object draw frames (set if was drawn).
	ILINE void SetDrawFrame(int nFrameID, int nRecursionLevel)
	{
		assert(m_pTempData);
		int* pDrawFrames = (int*)m_pTempData;
		pDrawFrames[nRecursionLevel] = nFrameID;
	}

	ILINE int GetDrawFrame(int nRecursionLevel = 0) const
	{
		IF (!m_pTempData, 0)
			return 0;

		int* pDrawFrames = (int*)m_pTempData;
		return pDrawFrames[nRecursionLevel];
	}

	//! \return Current VisArea or null if in outdoors or entity was not registered in 3Dengine.
	ILINE IVisArea* GetEntityVisArea() const { return m_pOcNode ? (IVisArea*)(m_pOcNode->m_pVisArea) : NULL; }

	//! \return Current VisArea or null if in outdoors or entity was not registered in 3Dengine.
	struct CTerrainNode* GetEntityTerrainNode() const { return (m_pOcNode && !m_pOcNode->m_pVisArea) ? m_pOcNode->GetTerrainNode() : NULL; }

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
	ILINE void InvalidatePermanentRenderObject() { m_nInternalFlags |= PERMANENT_RO_INVALID; };

	//////////////////////////////////////////////////////////////////////////
	// Variables
	//////////////////////////////////////////////////////////////////////////

public:

	//! Every sector has linked list of IRenderNode objects.
	IRenderNode* m_pNext, * m_pPrev;

	//! Current objects tree cell.
	IOctreeNode* m_pOcNode;

	//! Pointer to temporary data allocated only for currently visible objects.
	struct SRenderNodeTempData* m_pTempData;

	//! Max view distance.
	float m_fWSMaxViewDist;

	//! Render flags.
	int m_dwRndFlags;

	//! Segment Id.
	int m_nSID;

	//! Flags for render node internal usage, one or more bits from EInternalFlags.
	uint32 m_nInternalFlags;

	//! Max view distance settings.
	unsigned char m_ucViewDistRatio;

	//! LOD settings.
	unsigned char m_ucLodRatio;

	//! Material layers bitmask -> which material layers are active.
	unsigned char m_nMaterialLayers;

	//! Shadow LOD bias.
	//! Set to SHADOW_LODBIAS_DISABLE to disable any shadow lod overrides for this rendernode.
	static const int8 SHADOW_LODBIAS_DISABLE = -128;
	int8              m_cShadowLodBias;
};

inline void IRenderNode::SetViewDistRatio(int nViewDistRatio)
{
	nViewDistRatio = SATURATEB(nViewDistRatio);
	if (m_ucViewDistRatio != nViewDistRatio)
	{
		m_ucViewDistRatio = nViewDistRatio;
		if (m_pOcNode)
			m_pOcNode->SetCompiled(false);
	}
}

///////////////////////////////////////////////////////////////////////////////
inline IStatObj* IRenderNode::GetEntityStatObj(unsigned int nPartId, unsigned int nSubPartId, Matrix34A* pMatrix, bool bReturnOnlyVisible)
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
};

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

const int FAR_TEX_COUNT = 12;             //!< Number of sprites per object.
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

struct ILightSource : public IRenderNode
{
	// <interfuscator:shuffle>
	virtual void                     SetLightProperties(const CDLight& light) = 0;
	virtual CDLight&                 GetLightProperties() = 0;
	virtual const Matrix34&          GetMatrix() = 0;
	virtual struct ShadowMapFrustum* GetShadowFrustum(int nId = 0) = 0;
	virtual bool                     IsLightAreasVisible() = 0;
	virtual void                     SetCastingException(IRenderNode* pNotCaster) = 0;
	virtual void                     SetSrcEntity(IEntity* pEnt) = 0;
	// </interfuscator:shuffle>
};

struct SCloudMovementProperties
{
	bool  m_autoMove;
	Vec3  m_speed;
	Vec3  m_spaceLoopBox;
	float m_fadeDistance;
};

//! Interface to the Cloud Render Node object.
struct ICloudRenderNode : public IRenderNode
{
	// <interfuscator:shuffle>
	//! Loads a cloud from a cloud description XML file.
	virtual bool LoadCloud(const char* sCloudFilename) = 0;
	virtual bool LoadCloudFromXml(XmlNodeRef cloudNode) = 0;
	virtual void SetMovementProperties(const SCloudMovementProperties& properties) = 0;
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

//! IFogVolumeRenderNode is an interface to the Fog Volume Render Node object.
struct SFogVolumeProperties
{
	// Common parameters.
	// Center position & rotation values are taken from the entity matrix.

	int    m_volumeType;
	Vec3   m_size;
	ColorF m_color;
	bool   m_useGlobalFogColor;
	bool   m_ignoresVisAreas;
	bool   m_affectsThisAreaOnly;
	float  m_globalDensity;
	float  m_densityOffset;
	float  m_softEdges;
	float  m_fHDRDynamic;               //!< 0 to get the same results in LDR, <0 to get darker, >0 to get brighter.
	float  m_nearCutoff;

	float  m_heightFallOffDirLong;        //!< Height based fog specifics.
	float  m_heightFallOffDirLati;        //!< Height based fog specifics.
	float  m_heightFallOffShift;          //!< Height based fog specifics.
	float  m_heightFallOffScale;          //!< Height based fog specifics.

	float  m_rampStart;
	float  m_rampEnd;
	float  m_rampInfluence;
	float  m_windInfluence;
	float  m_densityNoiseScale;
	float  m_densityNoiseOffset;
	float  m_densityNoiseTimeFrequency;
	Vec3   m_densityNoiseFrequency;
	Vec3   m_emission;
};

struct IFogVolumeRenderNode : public IRenderNode
{
	// <interfuscator:shuffle>
	virtual void            SetFogVolumeProperties(const SFogVolumeProperties& properties) = 0;
	virtual const Matrix34& GetMatrix() const = 0;

	virtual void            FadeGlobalDensity(float fadeTime, float newGlobalDensity) = 0;
	// </interfuscator:shuffle>
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
	virtual void             CreateArea(uint64 volumeID, const Vec3* pVertices, unsigned int numVertices, const Vec2& surfUVScale, const Plane& fogPlane, bool keepSerializationParams = false, int nSID = -1) = 0;
	virtual void             CreateRiver(uint64 volumeID, const Vec3* pVertices, unsigned int numVertices, float uTexCoordBegin, float uTexCoordEnd, const Vec2& surfUVScale, const Plane& fogPlane, bool keepSerializationParams = false, int nSID = -1) = 0;

	virtual void             SetAreaPhysicsArea(const Vec3* pVertices, unsigned int numVertices, bool keepSerializationParams = false) = 0;
	virtual void             SetRiverPhysicsArea(const Vec3* pVertices, unsigned int numVertices, bool keepSerializationParams = false) = 0;

	virtual IPhysicalEntity* SetAndCreatePhysicsArea(const Vec3* pVertices, unsigned int numVertices) = 0;
	// </interfuscator:shuffle>
};

//! Interface to the Water Wave Render Node object.
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

//! Interface to the Distance Cloud Render Node object.
struct SDistanceCloudProperties
{
	Vec3        m_pos;
	float       m_sizeX;
	float       m_sizeY;
	float       m_rotationZ;
	const char* m_pMaterialName;
};

struct IDistanceCloudRenderNode : public IRenderNode
{
	virtual void SetProperties(const SDistanceCloudProperties& properties) = 0;
};

struct SVolumeObjectProperties
{
};

struct SVolumeObjectMovementProperties
{
	bool  m_autoMove;
	Vec3  m_speed;
	Vec3  m_spaceLoopBox;
	float m_fadeDistance;
};

//! Interface to the Volume Object Render Node object.
struct IVolumeObjectRenderNode : public IRenderNode
{
	// <interfuscator:shuffle>
	virtual void LoadVolumeData(const char* filePath) = 0;
	virtual void SetProperties(const SVolumeObjectProperties& properties) = 0;
	virtual void SetMovementProperties(const SVolumeObjectMovementProperties& properties) = 0;
	// </interfuscator:shuffle>
};

#if !defined(EXCLUDE_DOCUMENTATION_PURPOSE)
struct IPrismRenderNode : public IRenderNode
{
};
#endif // EXCLUDE_DOCUMENTATION_PURPOSE

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
		int   nFlags; //!< ERopeParamFlags.

		float fThickness;

		//! Radius for the end points anchors that bind rope to objects in world.
		float fAnchorRadius;

		//////////////////////////////////////////////////////////////////////////
		// Rendering/Tessellation.
		int   nNumSegments;
		int   nNumSides;
		float fTextureTileU;
		float fTextureTileV;
		//////////////////////////////////////////////////////////////////////////

		//////////////////////////////////////////////////////////////////////////
		// Rope Physical parameters.
		int   nPhysSegments;
		int   nMaxSubVtx;

		float mass;        //!< Rope mass. if mass is 0 it will be static.
		float tension;
		float friction;
		float frictionPull;

		Vec3  wind;
		float windVariance;
		float airResistance;
		float waterResistance;

		float jointLimit;
		float maxForce;

		int   nMaxIters;
		float maxTimeStep;
		float stiffness;
		float hardness;
		float damping;
		float sleepSpeed;
	};
	struct SEndPointLink
	{
		IPhysicalEntity* pPhysicalEntity;
		Vec3             offset;
		int              nPartId;
	};

	// <interfuscator:shuffle>
	virtual void                                SetName(const char* sNodeName) = 0;

	virtual void                                SetParams(const SRopeParams& params) = 0;
	virtual const IRopeRenderNode::SRopeParams& GetParams() const = 0;

	virtual void                                SetEntityOwner(uint32 nEntityId) = 0;
	virtual uint32                              GetEntityOwner() const = 0;

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

	//! Sound related.
	virtual void SetRopeSound(char const* const pcSoundName, int unsigned const nSegmentToAttachTo, float const fOffset) = 0;
	virtual void StopRopeSound() = 0;
	virtual void ResetRopeSound() = 0;
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
	virtual void DebugDraw(const struct SGeometryDebugDrawInfo& info, float fExtrudeScale = 0.01f, uint nodeIndex = 0) const = 0;

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

#endif // IRenderNodeSTATE_H
