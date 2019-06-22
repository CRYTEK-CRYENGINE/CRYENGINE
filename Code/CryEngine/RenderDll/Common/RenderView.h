// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include "LockFreeAddVector.h"
#include "RendererResources.h"
#include "RenderItemDrawer.h"
#include "RenderOutput.h"
#include "RenderPipeline.h"

#include <CryThreading/CryThreadSafePushContainer.h>
#include <CryThreading/IJobManager.h>
#include <CryMath/Range.h>

class CPermanentRenderObject;
class CREClientPoly;
class CRenderPolygonDataPool;
class CSceneRenderPass;

struct SGraphicsPipelinePassContext;

enum EObjectCompilationOptions : uint8;

//////////////////////////////////////////////////////////////////////////
// Contain information about view need to render
struct SRenderViewInfo
{
	enum EFlags
	{
		eFlags_ReverseDepth  = BIT(0),
		eFlags_MirrorCull    = BIT(1),
		eFlags_SubpixelShift = BIT(2),
		eFlags_MirrorCamera  = BIT(3),
		eFlags_DrawToTexure  = BIT(4),

		eFlags_None          = 0
	};
	enum EFrustumCorner
	{
		eFrustum_RT = 0, //!< Top Right
		eFrustum_LT = 1, //!< Top Left
		eFrustum_LB = 2, //!< Bottom Left
		eFrustum_RB = 3, //!< Bottom Right
	};

	const CCamera*  pCamera;
	const Plane*    pFrustumPlanes;

	Vec3            cameraOrigin;
	Vec3            cameraVX;
	Vec3            cameraVY;
	Vec3            cameraVZ;

	float           nearClipPlane;
	float           farClipPlane;

	Matrix44A       cameraProjZeroMatrix;
	Matrix44A       cameraProjMatrix;
	Matrix44A       cameraProjNearestMatrix;
	Matrix44A       projMatrix;
	Matrix44A       unjitteredProjMatrix;
	Matrix44A       viewMatrix;
	Matrix44A       invCameraProjMatrix;
	Matrix44A       invViewMatrix;
	Matrix44A       prevCameraMatrix;
	Matrix44A       prevCameraProjMatrix;
	Matrix44A       prevCameraProjNearestMatrix;

	Vec3            m_frustumCorners[4];

	SRenderViewport viewport;
	Vec4            downscaleFactor;

	EFlags          flags;

	SRenderViewInfo();
	void     SetCamera(const CCamera& cam, const CCamera& previousCam, Vec2 subpixelShift, float drawNearestFov, float drawNearestFarPlane);

	Matrix44 GetReprojection() const;
	Matrix44 GetNearestProjection(float nearestFOV, float farPlane, Vec2 subpixelShift) const;
	void     ExtractViewMatrices(const CCamera& cam, Matrix44& view, Matrix44& viewZero, Matrix44& invView) const;

	float    WorldToCameraZ(const Vec3& wP) const;
};
DEFINE_ENUM_FLAG_OPERATORS(SRenderViewInfo::EFlags);

// This class encapsulate all information required to render a camera view.
// It stores list of render items added by 3D engine
class CRenderView : public IRenderView
{
public:
	typedef TRange<int>          ItemsRange;
	typedef _smart_ptr<CTexture> TexSmartPtr;

	enum eShadowFrustumRenderType
	{
		// NOTE: DO NOT reorder, shadow gen depends on values
		eShadowFrustumRenderType_SunCached   = 0,
		eShadowFrustumRenderType_HeightmapAO = 1,
		eShadowFrustumRenderType_SunDynamic  = 2,
		eShadowFrustumRenderType_LocalLight  = 3,
		eShadowFrustumRenderType_Custom      = 4,

		eShadowFrustumRenderType_Count,
		eShadowFrustumRenderType_First = eShadowFrustumRenderType_SunCached
	};

	struct STransparentSegment
	{
		std::vector<TRect_tpl<std::uint16_t>> resolveRects;
		TRange<int>                           rendItems = { 0 };
	};
	using STransparentSegments = std::vector<STransparentSegment>;

	typedef lockfree_add_vector<SRendItem>       RenderItems;
	typedef std::vector<SShadowFrustumToRender>  ShadowFrustums;
	typedef std::vector<SShadowFrustumToRender*> ShadowFrustumsPtr;

	static const int32 MaxFogVolumeNum = 64;

	static const int32 CloudBlockerTypeNum = 2;
	static const int32 MaxCloudBlockerWorldSpaceNum = 4;
	static const int32 MaxCloudBlockerScreenSpaceNum = 1;

public:
	//////////////////////////////////////////////////////////////////////////
	// IRenderView
	//////////////////////////////////////////////////////////////////////////
	virtual void       SetFrameId(int frameId) override;
	virtual int        GetFrameId() const final                        { return m_frameId; }

	virtual void       SetFrameTime(CTimeValue frameTime) final        { m_frameTime = frameTime; }
	virtual CTimeValue GetFrameTime() const final                      { return m_frameTime; }

	virtual void       SetSkipRenderingFlags(uint32 nFlags) override   { m_skipRenderingFlags = nFlags; }
	virtual uint32     GetSkipRenderingFlags() const final             { return m_skipRenderingFlags; }

	virtual void       SetShaderRenderingFlags(uint32 nFlags) override { m_shaderRenderingFlags = nFlags; }
	virtual uint32     GetShaderRenderingFlags() const final           { return m_shaderRenderingFlags; }

	virtual void       SetZoomFactor(float zFactor) override           { m_zFactor = zFactor; }
	virtual float      GetZoomFactor() const final                     { return m_zFactor; }

	virtual void       SetCameras(const CCamera* pCameras, int cameraCount) final;
	virtual void       SetPreviousFrameCameras(const CCamera* pCameras, int cameraCount) final;


	// Begin/End writing items to the view from 3d engine traversal.
	virtual void                 SwitchUsageMode(EUsageMode mode) override;

	virtual void                 AddRenderObject(CRenderElement* pRenderElement, SShaderItem& pShaderItem, CRenderObject* pRenderObject, const SRenderingPassInfo& passInfo, int list, int afterWater) threadsafe final;
	virtual void                 AddPermanentObject(CRenderObject* pObject, const SRenderingPassInfo& passInfo) final;

	virtual void                 SetGlobalFog(const SRenderGlobalFogDescription& fogDescription) final { m_globalFogDescription = fogDescription; }

	virtual void                 SetTargetClearColor(const ColorF& color, bool bEnableClear) override;

	virtual CRenderObject*       AllocateTemporaryRenderObject() final;

	void                         SetSkinningDataPools(const SSkinningDataPoolInfo& skinningData) { m_SkinningData = skinningData; }
	const SSkinningDataPoolInfo& GetSkinningDataPools() const                                    { return m_SkinningData; }
	virtual uint32               GetSkinningPoolIndex() const final                              { return m_SkinningData.poolIndex; }

	//! HDR and Z Depth render target
	CTexture*            GetColorTarget() const;
	CTexture*            GetDepthTarget() const;

	void                 AssignRenderOutput(CRenderOutputPtr pRenderOutput);
	void                 InspectRenderOutput();
	void                 UnsetRenderOutput();
	const CRenderOutput* GetRenderOutput() const { return m_pRenderOutput.get(); }
	CRenderOutput*       GetRenderOutput()       { return m_pRenderOutput.get(); }

	//! Retrieve rendering viewport for this Render View
	virtual void                                      SetViewport(const SRenderViewport& viewport) final;
	virtual const SRenderViewport&                    GetViewport() const final;

	virtual void                                      SetGraphicsPipeline(std::shared_ptr<CGraphicsPipeline> pipeline) final;
	virtual const std::shared_ptr<CGraphicsPipeline>& GetGraphicsPipeline() const final                                      { return m_pGraphicsPipeline; }

	//! Get resolution of the render target surface(s)
	//! Note that Viewport can be smaller then this.
	Vec2_tpl<uint32_t> GetRenderResolution() const  { return { m_RenderWidth, m_RenderHeight }; }
	Vec2_tpl<uint32_t> GetOutputResolution() const  { return m_pRenderOutput ? m_pRenderOutput->GetOutputResolution() : GetRenderResolution(); }
	Vec2_tpl<uint32_t> GetDisplayResolution() const { return m_pRenderOutput ? m_pRenderOutput->GetDisplayResolution() : GetRenderResolution(); }

	void               ChangeRenderResolution(uint32_t renderWidth, uint32_t renderHeight, bool bForce);
	//////////////////////////////////////////////////////////////////////////

public:
	CRenderView(const char* name, EViewType type, CRenderView* pParentView = nullptr, ShadowMapFrustum* pShadowFrustumOwner = nullptr);
	~CRenderView();

	EViewType    GetType() const                         { return m_viewType; }

	void         SetParentView(CRenderView* pParentView) { m_pParentView = pParentView; }
	CRenderView* GetParentView() const                   { return m_pParentView; }

	//////////////////////////////////////////////////////////////////////////
	// View flags
	//////////////////////////////////////////////////////////////////////////
	void                          SetViewFlags(SRenderViewInfo::EFlags flags)         { m_viewFlags = flags; }
	const SRenderViewInfo::EFlags GetViewFlags() const                                { return m_viewFlags; }
	bool                          IsViewFlag(SRenderViewInfo::EFlags viewFlags) const { return 0 != (m_viewFlags & viewFlags); }

	//////////////////////////////////////////////////////////////////////////
	// Camera access
	const CCamera& GetCamera(CCamera::EEye eye)         const { CRY_ASSERT(eye == CCamera::eEye_Left || eye == CCamera::eEye_Right); return m_camera[eye]; }
	const CCamera& GetPreviousCamera(CCamera::EEye eye) const { CRY_ASSERT(eye == CCamera::eEye_Left || eye == CCamera::eEye_Right); return m_previousCamera[eye]; }

	CCamera::EEye  GetCurrentEye() const                      { return m_currentEye; }
	void           SetCurrentEye(CCamera::EEye eye)           { m_currentEye = eye; }

	RenderItems&   GetRenderItems(ERenderListID renderList)
	{
		CRY_ASSERT(m_usageMode != eUsageModeWriting || renderList == EFSLIST_PREPROCESS); // While writing we must not read back items.
		return m_renderItems[renderList];
	}

	bool HasRenderItems(ERenderListID renderList, uint32 includeFilter)
	{
		return SRendItem::TestCombinedBatchFlags(GetBatchFlags(renderList), includeFilter);
	}

	uint32        GetBatchFlags(ERenderListID renderList) const                 { return m_batchFlags[renderList]; }
	ERenderListID GetRecordingRenderList(ERenderListID nRenderListOrSide) const { return IsShadowGenView() ? EFSLIST_SHADOW_GEN : nRenderListOrSide; }

	void          AddRenderItem(CRenderElement* pElem, CRenderObject* RESTRICT_POINTER pObj, const SShaderItem& shaderItem, ERenderListID renderList, uint32 nBatchFlags, const SRenderingPassInfo& passInfo,
	                            SRendItemSorter sorter) threadsafe;

	bool       CheckPermanentRenderObjects() const { return !m_permanentObjects.empty(); }
	void       AddPermanentObjectImpl(CPermanentRenderObject* pObject, const SRenderingPassInfo& passInfo);

	ItemsRange GetItemsRange(ERenderListID renderList);

	// Find render item index in the sorted list according to arbitrary criteria.
	template<typename T>
	int        FindRenderListSplit(T predicate, ERenderListID list, int first = 0, int end = 0x7FFFFFFF);

	void       PrepareForRendering();
	void       PrepareForWriting();

	bool                           IsRecursive()        const { return m_viewType == eViewType_Recursive; }
	bool                           IsShadowGenView()    const { return m_viewType == eViewType_Shadow; }
	bool                           IsBillboardGenView() const { return m_viewType == eViewType_BillboardGen; }
	EUsageMode                     GetUsageMode()       const { return m_usageMode; }
	CRenderObject::ERenderPassType GetPassType()        const { return IsShadowGenView() ? CRenderObject::eRenderPass_Shadows : CRenderObject::eRenderPass_General; }
	//////////////////////////////////////////////////////////////////////////
	// Shadows related
	void AddShadowFrustumToRender(const SShadowFrustumToRender& frustum);

	// Get render view used for given frustum.
	CRenderView*                         GetShadowsView(ShadowMapFrustum* pFrustum);

	ItemsRange                           GetShadowItemsRange(ShadowMapFrustum* pFrustum, int nFrustumSide);
	RenderItems&                         GetShadowItems(ShadowMapFrustum* pFrustum, int nFrustumSide);
	std::vector<SShadowFrustumToRender>& GetFrustumsToRender() { return m_shadows.m_renderFrustums; }

	ShadowFrustumsPtr&                   GetShadowFrustumsForLight(int lightId);
	ShadowFrustumsPtr&       GetShadowFrustumsByType(eShadowFrustumRenderType type)       { return m_shadows.m_frustumsByType[type]; }
	const ShadowFrustumsPtr& GetShadowFrustumsByType(eShadowFrustumRenderType type) const { return m_shadows.m_frustumsByType[type]; }

	// Can start executing post write jobs on shadow views
	void                      PostWriteShadowViews();
	void                      PrepareShadowViews(); // Sync all outstanding shadow preparation jobs
	virtual void              SetShadowFrustumOwner(ShadowMapFrustum* pOwner) final { m_shadows.m_pShadowFrustumOwner = pOwner; }
	virtual ShadowMapFrustum* GetShadowFrustumOwner() const final                   { return m_shadows.m_pShadowFrustumOwner; }

	//////////////////////////////////////////////////////////////////////////
	const SRenderGlobalFogDescription& GetGlobalFog() const        { return m_globalFogDescription; }
	bool                               IsGlobalFogEnabled() const  { return m_globalFogDescription.bEnable; }

	ColorF                             GetTargetClearColor() const { return m_targetClearColor; }
	bool                               IsClearTarget()       const { return m_bClearTarget; }

	//////////////////////////////////////////////////////////////////////////
	// Dynamic Lights
	//////////////////////////////////////////////////////////////////////////

	virtual RenderLightIndex AddDeferredLight(const SRenderLight& pDL, float fMult, const SRenderingPassInfo& passInfo) final;

	virtual RenderLightIndex AddDynamicLight(const SRenderLight& light) final;
	virtual RenderLightIndex GetDynamicLightsCount() const final;
	virtual SRenderLight&    GetDynamicLight(RenderLightIndex nLightId) final;

	virtual RenderLightIndex AddLight(eDeferredLightType lightType, const SRenderLight& light) final;
	virtual RenderLightIndex GetLightsCount(eDeferredLightType lightType) const final;
	virtual SRenderLight& GetLight(eDeferredLightType lightType, RenderLightIndex nLightId) final;

	RenderLightsList&     GetLightsArray(eDeferredLightType lightType);
	SRenderLight*         AddLightAtIndex(eDeferredLightType lightType, const SRenderLight& light, RenderLightIndex index = -1);

	bool                  HasSunLight() { return GetSunLight() != nullptr; }
	const SRenderLight*   GetSunLight() const;
	const Vec4            GetSunLightColor();
	const Vec3            GetSunLightDirection();
	//////////////////////////////////////////////////////////////////////////

	virtual const SRendererData& GetRendererData() const override { return m_rendererData; }
	virtual SRendererData&       GetRendererData() override       { return m_rendererData; }

	//////////////////////////////////////////////////////////////////////////
	// Clip Volumes
	//////////////////////////////////////////////////////////////////////////
	uint8                                   AddClipVolume(const IClipVolume* pClipVolume) final;
	void                                    SetClipVolumeBlendInfo(const IClipVolume* pClipVolume, int blendInfoCount, IClipVolume** blendVolumes, Plane* blendPlanes) final;
	const std::vector<SDeferredClipVolume>& GetClipVolumes() const { return m_clipVolumes; }
	//////////////////////////////////////////////////////////////////////////

	//////////////////////////////////////////////////////////////////////////
	// Fog Volumes
	//////////////////////////////////////////////////////////////////////////
	void                               AddFogVolume(const CREFogVolume* pFogVolume) final;
	const std::vector<SFogVolumeInfo>& GetFogVolumes(IFogVolumeRenderNode::eFogVolumeType volumeType) const;
	//////////////////////////////////////////////////////////////////////////

	//////////////////////////////////////////////////////////////////////////
	// Cloud Blockers
	//////////////////////////////////////////////////////////////////////////
	void                              AddCloudBlocker(const Vec3& pos, const Vec3& param, int32 flags) final;
	const std::vector<SCloudBlocker>& GetCloudBlockers(uint32 blockerType) const;
	//////////////////////////////////////////////////////////////////////////

	//////////////////////////////////////////////////////////////////////////
	// Water Ripples
	//////////////////////////////////////////////////////////////////////////
	void                                 AddWaterRipple(const SWaterRippleInfo& waterRippleInfo) final;
	const std::vector<SWaterRippleInfo>& GetWaterRipples() const;
	//////////////////////////////////////////////////////////////////////////

	//////////////////////////////////////////////////////////////////////////
	// Polygons.
	virtual void            AddPolygon(const SRenderPolygonDescription& poly, const SRenderingPassInfo& passInfo) final;
	CRenderPolygonDataPool* GetPolygonDataPool() { return m_polygonDataPool.get(); }
	//////////////////////////////////////////////////////////////////////////

	//////////////////////////////////////////////////////////////////////////
	SDeferredDecal*              AddDeferredDecal(const SDeferredDecal& source);
	std::vector<SDeferredDecal>& GetDeferredDecals();
	size_t                       GetDeferredDecalsCount() const       { return m_deferredDecals.size(); }
	void                         SetDeferredNormalDecals(bool bValue) { m_bDeferrredNormalDecals = bValue; }
	bool                         GetDeferredNormalDecals() const      { return m_bDeferrredNormalDecals; }
	//////////////////////////////////////////////////////////////////////////

	const SRenderViewShaderConstants& GetShaderConstants() const;
	SRenderViewShaderConstants&       GetShaderConstants();

	//////////////////////////////////////////////////////////////////////////
	virtual uint16 PushFogVolumeContribution(const ColorF& fogVolumeContrib, const SRenderingPassInfo& passInfo) threadsafe final;
	void           GetFogVolumeContribution(uint16 idx, ColorF& rColor) const;
	//////////////////////////////////////////////////////////////////////////

	//////////////////////////////////////////////////////////////////////////
	// Operation on items.
	//////////////////////////////////////////////////////////////////////////
	void                   Clear();

	const SRenderViewInfo& GetViewInfo(CCamera::EEye eye) const { return m_viewInfo[eye]; }
	size_t                 GetViewInfoCount() const             { return m_viewInfoCount; }

	void                   CompileModifiedRenderObjects();
	void                   CalculateViewInfo();

	void                   StartOptimizeTransparentRenderItemsResolvesJob();
	void                   OptimizeTransparentRenderItemsResolvesJob();
	void                   WaitForOptimizeTransparentRenderItemsResolvesJob() const;
	bool                   HasResolveForList(ERenderListID list) const
	{
		const bool bSecondaryViewport = (m_shaderRenderingFlags & SHDF_SECONDARY_VIEWPORT) != 0;
		const auto refractionMask = FB_REFRACTION | FB_RESOLVE_FULL;
		const auto flags = GetBatchFlags(list);
		return (list == EFSLIST_TRANSP_BW || list == EFSLIST_TRANSP_AW || list == EFSLIST_TRANSP_NEAREST) &&
		       !!(flags & refractionMask) && CRendererCVars::CV_r_Refraction && !bSecondaryViewport;
	}
	const STransparentSegments& GetTransparentSegments(ERenderListID list) const
	{
		CRY_ASSERT_MESSAGE(list == EFSLIST_TRANSP_BW || list == EFSLIST_TRANSP_AW || list == EFSLIST_TRANSP_NEAREST, "'list' does not name a transparent list");
		const int idx = static_cast<int>(list - EFSLIST_TRANSP_BW);
		return m_transparentSegments[idx];
	}

private:
	void                   DeleteThis() const override;

	void                   Job_PostWrite();
	void                   Job_SortRenderItemsInList(ERenderListID list);
	SRendItem              PrepareRenderItemForRenderList(const SRendItem& ri, uint32 nBatchFlags, uint64 objFlags, CRenderObject* pObj, float objDistance, ERenderListID list, const SShaderItem& shaderItem);
	void                   SortLights();
	void                   PreparePermanentRenderObjectsForCompile();
	void                   ExpandPermanentRenderObjects();
	void                   UpdateModifiedShaderItems();
	void                   ClearTemporaryCompiledObjects();
	void                   CheckAndScheduleForUpdate(const SShaderItem& shaderItem) threadsafe;
	template<bool isConcurrent>
	void                   AddRenderItemToRenderLists(const SRendItem& ri, uint64 objFlags, ERenderListID nRenderList, CRenderObject* RESTRICT_POINTER pObj, const SShaderItem& shaderItem) threadsafe;

	CCompiledRenderObject* AllocCompiledObject(CRenderObject* pObj, CRenderElement* pElem, const SShaderItem& shaderItem);

	std::size_t            OptimizeTransparentRenderItemsResolves(STransparentSegments& segments, RenderItems& renderItems, std::size_t total_resolve_count) const;
	TRect_tpl<uint16>      ComputeResolveViewport(const AABB& aabb, bool forceFullscreenUpdate) const;
	STransparentSegments&  GetTransparentSegments(ERenderListID list)
	{
		CRY_ASSERT_MESSAGE(list == EFSLIST_TRANSP_BW || list == EFSLIST_TRANSP_AW || list == EFSLIST_TRANSP_NEAREST, "'list' does not name a transparent list");
		const int idx = static_cast<int>(list - EFSLIST_TRANSP_BW);
		return m_transparentSegments[idx];
	}

private:
	EUsageMode      m_usageMode;
	const EViewType m_viewType;
	string          m_name;
	/// @See SRenderingPassInfo::ESkipRenderingFlags
	uint32          m_skipRenderingFlags;
	uint32          m_shaderRenderingFlags;
	float           m_zFactor = 1.0f;

	int             m_frameId;
	CTimeValue      m_frameTime;

	// For shadows or recursive view parent will be main rendering view
	CRenderView*    m_pParentView;

	RenderItems     m_renderItems[EFSLIST_NUM];
	volatile uint32 m_batchFlags[EFSLIST_NUM];

	// Resolve passes information
	STransparentSegments m_transparentSegments[3];
	mutable CryJobState  m_optimizeTransparentRenderItemsResolvesJobStatus;

	CryMT::CThreadSafePushContainer<CRenderObject> m_tempRenderObjects;

	// Light sources affecting the view.
	RenderLightsList m_lights[eDLT_NumLightTypes];

	SRendererData    m_rendererData;

	//////////////////////////////////////////////////////////////////////////
	// Clip Volumes
	std::vector<SDeferredClipVolume> m_clipVolumes;
	//////////////////////////////////////////////////////////////////////////

	//////////////////////////////////////////////////////////////////////////
	// Fog Volumes
	std::vector<SFogVolumeInfo> m_fogVolumes[IFogVolumeRenderNode::eFogVolumeType_Count];
	CryMT::CThreadSafePushContainer<ColorF> m_fogVolumeContributions;
	volatile int m_numFogVolumes;
	//////////////////////////////////////////////////////////////////////////

	//////////////////////////////////////////////////////////////////////////
	// Cloud Blockers
	std::vector<SCloudBlocker> m_cloudBlockers[CloudBlockerTypeNum];
	//////////////////////////////////////////////////////////////////////////

	//////////////////////////////////////////////////////////////////////////
	// Water Ripples
	std::vector<SWaterRippleInfo> m_waterRipples;
	//////////////////////////////////////////////////////////////////////////

	//////////////////////////////////////////////////////////////////////////
	// Simple polygons storage
	std::vector<CREClientPoly*>             m_polygonsPool;
	int                                     m_numUsedClientPolygons;
	std::shared_ptr<CRenderPolygonDataPool> m_polygonDataPool;
	//////////////////////////////////////////////////////////////////////////

	//////////////////////////////////////////////////////////////////////////
	// Deferred Decals
	std::vector<SDeferredDecal> m_deferredDecals;
	bool                        m_bDeferrredNormalDecals;

	SRenderGlobalFogDescription m_globalFogDescription;

	ColorF                      m_targetClearColor = {};
	bool                        m_bClearTarget = false;
	bool                        m_bRenderToSwapChain = false;
	//////////////////////////////////////////////////////////////////////////

	//////////////////////////////////////////////////////////////////////////
	// Skinning Data
	SSkinningDataPoolInfo m_SkinningData;
	//////////////////////////////////////////////////////////////////////////

	std::shared_ptr<CGraphicsPipeline> m_pGraphicsPipeline;

	uint32                             m_RenderWidth = -1;
	uint32                             m_RenderHeight = -1;

	CRenderOutputPtr                   m_pRenderOutput; // Output render target (currently used for recursive pass and secondary viewport)
	TexSmartPtr                        m_pColorTarget = nullptr;
	TexSmartPtr                        m_pDepthTarget = nullptr;

	SRenderViewport                    m_viewport;

	bool                               m_bTrackUncompiledItems;
	bool                               m_bAddingClientPolys;

	uint32                             m_skinningPoolIndex = 0;

	struct SPermanentObjectRecord;

	// Render objects modified by this view.
	struct SPermanentRenderObjectCompilationData
	{
		const SPermanentObjectRecord*   pPermanentRenderObjectRecord;
		uint32                          subObjectIdx;
		CPermanentRenderObject*         pObject;
		EObjectCompilationOptions       recompilationFlags;
	};
	CryMT::CThreadSafePushContainer<SPermanentRenderObjectCompilationData> m_permanentRenderObjectsToCompile;

	// Temporary compiled objects for this frame
	struct STemporaryRenderObjectCompilationData
	{
		CCompiledRenderObject* pObject;
		AABB                   localAABB;
		ERenderObjectFlags     objFlags;
		ERenderElementFlags    elmFlags;
	};
	CryMT::CThreadSafePushContainer<STemporaryRenderObjectCompilationData> m_temporaryCompiledObjects;

	// shader items that need to be updated
	CryMT::CThreadSafePushContainer<std::pair<CShaderResources*, CShader*>> m_shaderItemsToUpdate;

	// Persistent objects added to the view.
	struct SPermanentObjectRecord
	{
		CPermanentRenderObject*                    pRenderObject;
		uint32                                     itemSorter;
		int                                        shadowFrustumSide;
		bool                                       requiresInstanceDataUpdate;

		bool                                       passPreparedMask[CRenderObject::eRenderPass_NumTypes];
		CRenderObject::SInstanceInfo               instanceInfo;
		std::vector<CRenderObject::SInstanceInfo>  subObjectInstanceInfo;

		void prepareForCompilation();

		const CRenderObject::SInstanceInfo& getSubObjectInstanceInfo(const uint32 idx) const
		{
			if (idx == 0)
				return instanceInfo;
			return subObjectInstanceInfo[idx - 1];
		}

		bool IsPreparedForPass(CRenderObject::ERenderPassType passType) const
		{
			return passPreparedMask[passType];
		}
	};
	CryMT::CThreadSafePushContainer<SPermanentObjectRecord> m_permanentObjects;

	//////////////////////////////////////////////////////////////////////////
	CCamera::EEye m_currentEye = CCamera::eEye_Left;

	// Camera and view information required to render this RenderView
	CCamera         m_camera[CCamera::eEye_eCount];           // Current camera
	CCamera         m_previousCamera[CCamera::eEye_eCount];   // Previous frame render camera

	SRenderViewInfo m_viewInfo[CCamera::eEye_eCount];         // Calculated View Information
															  // Index 0 belongs to the current eye, other eye come after that.

	size_t          m_viewInfoCount;                          // Number of current m_viewInfo structures.

	// Internal job states to control when view job processing is done.
	CryJobState                    m_jobstate_Sort;
	CryJobState                    m_jobstate_PostWrite;
	CryJobState                    m_jobstate_ShadowGen;

	CryCriticalSectionNonRecursive m_lock_UsageMode;
	CryCriticalSectionNonRecursive m_lock_PostWrite;

	std::atomic<bool>              m_bPostWriteExecuted;

	// Constants to pass to shaders.
	SRenderViewShaderConstants m_shaderConstants;

	//@see EFlags
	SRenderViewInfo::EFlags m_viewFlags = SRenderViewInfo::eFlags_None;

	struct SShadows
	{
		using TiledShadingFrustumCoveragePair = std::pair<SShadowFrustumToRender*, Vec4>;
		using TiledShadingFrustumListByMaskSlice = std::vector<std::vector<TiledShadingFrustumCoveragePair>>;

		// Shadow frustums needed for a view.
		ShadowMapFrustum*                                             m_pShadowFrustumOwner;
		std::vector<SShadowFrustumToRender>                           m_renderFrustums;

		std::map<int, ShadowFrustumsPtr>                              m_frustumsByLight;
		std::array<ShadowFrustumsPtr, eShadowFrustumRenderType_Count> m_frustumsByType;
		TiledShadingFrustumListByMaskSlice                            m_frustumsPerTiledShadingSlice;

		CryMT::CThreadSafePushContainer<AABB>                         m_nearestCasterBoxes;

		void Clear();
		void AddNearestCaster(CRenderObject* pObj, const SRenderingPassInfo& passInfo);
		void CreateFrustumGroups();
		void PrepareNearestShadows();
		void GenerateSortedFrustumsForTiledShadingByScreenspaceOverlap();
	};

	// A list of auxiliary stat objects to be injected for rendering
	std::vector<std::pair<SRendParams, IStatObj*>> m_auxiliaryStatObjects;

public:// temp
	SShadows m_shadows;

	Vec2     m_vProjMatrixSubPixoffset = Vec2(0.0f, 0.0f);

private:
	CRenderItemDrawer m_RenderItemDrawer;

public:
	void                                                  DrawCompiledRenderItems(const SGraphicsPipelinePassContext& passContext) const;

	CRenderItemDrawer&                                    GetDrawer()                                                            { return m_RenderItemDrawer; }
	const CRenderItemDrawer&                              GetDrawer() const                                                      { return m_RenderItemDrawer; }

	void                                                  InjectAuxiliaryStatObject(SRendParams rp, IStatObj* pStatObj) override { m_auxiliaryStatObjects.emplace_back(std::make_pair(rp, pStatObj)); }
	const std::vector<std::pair<SRendParams, IStatObj*>>& GetAuxiliaryStatObjects() const override                               { return m_auxiliaryStatObjects; }
};

template<typename T>
inline int CRenderView::FindRenderListSplit(T predicate, ERenderListID renderList, int first, int end)
{
	FUNCTION_PROFILER_RENDERER();

	// Binary search, assumes that list is sorted by objFlag
	auto& renderItems = GetRenderItems(renderList);
	int size = renderItems.size();

	end = std::min<int>(end, size);
	first = std::min<int>(first, end);

	int last = end - 1;
	while (first <= last)
	{
		int middle = (first + last) / 2;

		if (predicate(renderItems[middle]))
			first = middle + 1;
		else
			last = middle - 1;
	}

	return last + 1;
}

typedef _smart_ptr<CRenderView> CRenderViewPtr;
