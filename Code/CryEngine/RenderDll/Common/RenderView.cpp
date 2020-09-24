// Copyright 2001-2019 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "RenderView.h"

#include "GraphicsPipeline/ShadowMap.h"
#include "GraphicsPipeline/ClipVolumes.h"
#include "GraphicsPipeline/SceneGBuffer.h"
#include "CompiledRenderObject.h"
#include "D3D_SVO.h"

#include <CryRenderer/branchmask.h>
#include <Common/RenderDisplayContext.h>

#include "RendElements/CREClientPoly.h"
#include "RendElements/FlareSoftOcclusionQuery.h"

//////////////////////////////////////////////////////////////////////////
CRenderView::CRenderView(const char* name, EViewType type, CRenderView* pParentView, ShadowMapFrustum* pShadowFrustumOwner)
	: m_usageMode(eUsageModeUndefined)
	, m_viewType(type)
	, m_name(name)
	, m_skipRenderingFlags(0)
	, m_frameId(0)
	, m_pParentView(pParentView)
	, m_tempRenderObjects(64, 1 << 20 /* capacity = 1 MiB*/)
	, m_fogVolumeContributions(64, 1 << 20 /* capacity = 1 MiB*/)
	, m_numFogVolumes(0)
	, m_numUsedClientPolygons(0)
	, m_bDeferrredNormalDecals(false)
	, m_bTrackUncompiledItems(true)
	, m_bAddingClientPolys(false)
	, m_permanentRenderObjectsToCompile(64, 1 << 20 /* capacity = 1 MiB*/)
	, m_temporaryCompiledObjects(64, 1 << 20 /* capacity = 1 MiB*/)
	, m_shaderItemsToUpdate(64, 1 << 20 /* capacity = 1 MiB*/)
	, m_permanentObjects(64, (1 << 14) * sizeof(SPermanentObjectRecord) /* capacity = 16 ki objects*/)
	, m_viewInfoCount(1)
	, m_bPostWriteExecuted(false)
{
	MEMSTAT_CONTEXT(EMemStatContextType::D3D, "CRenderView::CRenderView");

	for (int i = 0; i < EFSLIST_NUM; i++)
	{
		m_addedItems[i] = 0;
		m_batchFlags[i] = 0;

		m_renderItems[i].Init();
		m_renderItems[i].SetNoneWorkerThreadID(gEnv->mMainThreadId);
	}

	m_shadows.m_pShadowFrustumOwner = pShadowFrustumOwner;

	m_polygonDataPool.reset(new CRenderPolygonDataPool);

	ZeroStruct(m_shaderConstants);
}

//////////////////////////////////////////////////////////////////////////
CRenderView::~CRenderView()
{
	Clear();

	for (auto pClientPoly : m_polygonsPool)
		delete pClientPoly;
}

SRenderViewShaderConstants& CRenderView::GetShaderConstants()
{
	if (m_pParentView)
		return m_pParentView->GetShaderConstants();
	return m_shaderConstants;
}

const SRenderViewShaderConstants& CRenderView::GetShaderConstants() const
{
	if (m_pParentView)
		return m_pParentView->GetShaderConstants();
	return m_shaderConstants;
}

void CRenderView::Clear()
{
	FUNCTION_PROFILER_RENDERER();

	CRY_ASSERT(m_usageMode == IRenderView::eUsageModeReadingDone ||
	           m_usageMode == IRenderView::eUsageModeWritingDone ||
	           m_usageMode == IRenderView::eUsageModeUndefined);

	if (m_usageMode == IRenderView::eUsageModeWritingDone)
	{
		m_jobstate_Sort.Wait();
		m_jobstate_PostWrite.Wait();
		m_jobstate_ShadowGen.Wait();
	}

	m_usageMode = IRenderView::eUsageModeUndefined;
	m_name.clear();
	m_frameId = -1;
	m_frameTime = CTimeValue();
	m_pParentView = nullptr;

	m_RenderWidth  = -1;
	m_RenderHeight = -1;

	ZeroStruct(m_viewport);
	m_bTrackUncompiledItems = true;
	m_bAddingClientPolys = false;
	m_skinningPoolIndex = 0;
	m_shaderItemsToUpdate.clear();
	for (int i = 0; i < CCamera::eEye_eCount; ++i)
		m_camera[i] = m_previousCamera[i] = CCamera();
	ZeroArray(m_viewInfo);
	m_viewInfoCount = 0;
	m_bPostWriteExecuted = false;
	m_vProjMatrixSubPixoffset = Vec2(ZERO);

	m_viewFlags = SRenderViewInfo::eFlags_None;
	m_viewInfoCount = 1;
	m_numUsedClientPolygons = 0;
	m_polygonDataPool->Clear();

	for (int i = 0; i < EFSLIST_NUM; i++)
	{
		m_renderItems[i].clear();
		m_batchFlags[i] = 0;
		m_addedItems[i] = 0;
	}

	for (int i = 0; i < eDLT_NumLightTypes; i++)
	{
		for (auto& light : m_lights[i])
		{
			light.DropResources();
		}
		m_lights[i].clear();
	}

	m_deferredDecals.clear();
	m_bDeferrredNormalDecals = false;
	m_clipVolumes.clear();

	for (int32 i = 0; i < IFogVolumeRenderNode::eFogVolumeType_Count; ++i)
	{
		m_fogVolumes[i].clear();
	}

	for (int32 i = 0; i < CloudBlockerTypeNum; ++i)
	{
		m_cloudBlockers[i].clear();
	}

	m_waterRipples.clear();

	m_shadows.Clear();

	m_fogVolumeContributions.clear();
	m_numFogVolumes = 0;

	m_globalFogDescription = SRenderGlobalFogDescription();

	ClearTemporaryCompiledObjects();
	m_permanentRenderObjectsToCompile.clear();

	m_bTrackUncompiledItems = true;

	ZeroStruct(m_shaderConstants);

	m_skipRenderingFlags = 0;

	m_tempRenderObjects.reset_container();

	m_permanentObjects.clear();

	m_bClearTarget = false;
	m_targetClearColor = ColorF(0,0,0,0);

	UnsetRenderOutput();

	ZeroStruct(m_SkinningData);

	m_auxiliaryStatObjects.clear();
}

// Helper function to allocate new compiled object from pool
CCompiledRenderObject* CRenderView::AllocCompiledObject(CRenderObject* pObj, CRenderElement* pElem, const SShaderItem& shaderItem)
{
	CCompiledRenderObject* pCompiledObject = CCompiledRenderObject::AllocateFromPool();
	pCompiledObject->Init(shaderItem, pElem);

	// Assign any compiled object to the RenderObject, just to be used as a root reference for constant buffer sharing
	pObj->m_pCompiledObject = pCompiledObject;
	pCompiledObject->m_pRO = pObj;

	return pCompiledObject;
}

SDeferredDecal* CRenderView::AddDeferredDecal(const SDeferredDecal& source)
{
	if (!source.pMaterial)
	{
		assert(0);
	}
	m_deferredDecals.push_back(source);
	return &m_deferredDecals.back();
}

std::vector<SDeferredDecal>& CRenderView::GetDeferredDecals()
{
	return m_deferredDecals;
}

void CRenderView::SetFrameId(int frameId)
{
	m_frameId = frameId;
}

void CRenderView::SetCameras(const CCamera* pCameras, int cameraCount)
{
	CRY_ASSERT(cameraCount == 1 || cameraCount == 2);

	for (int i = 0; i < cameraCount; ++i)
	{
		CRY_ASSERT(pCameras[i].GetEye() == CCamera::eEye_Left || pCameras[i].GetEye() == CCamera::eEye_Right);

		const CCamera& cam = pCameras[i];
		m_camera[cam.GetEye()] = cam;
	}
}

//////////////////////////////////////////////////////////////////////////
void CRenderView::SetPreviousFrameCameras(const CCamera* pCameras, int cameraCount)
{
	CRY_ASSERT(cameraCount == 1 || cameraCount == 2);

	for (int i = 0; i < cameraCount; ++i)
	{
		CRY_ASSERT(pCameras[i].GetEye() == CCamera::eEye_Left || pCameras[i].GetEye() == CCamera::eEye_Right);
		const CCamera& cam = pCameras[i];
		m_previousCamera[cam.GetEye()] = cam;
	}
}

//////////////////////////////////////////////////////////////////////////
void CRenderView::CalculateViewInfo()
{
	SRenderViewInfo::EFlags viewFlags = m_viewFlags;
	viewFlags |= SRenderViewInfo::eFlags_ReverseDepth;

	//if (!(m_viewFlags & SRenderViewInfo::eFlags_DrawToTexure) && !(gRenDev->m_RP.m_PersFlags2 & RBPF2_NOPOSTAA))
	if (!(m_viewFlags & SRenderViewInfo::eFlags_DrawToTexure))
	{
		viewFlags |= SRenderViewInfo::eFlags_SubpixelShift;
	}

	viewFlags |= m_viewFlags;

	CRenderView* pRenderView = this;
	size_t viewInfoCount = 0;

	m_viewInfoCount = (gcpRendD3D->IsStereoEnabled() ? CCamera::eEye_Both : CCamera::eEye_Right);
	for(uint32 eyeIdx = 0; eyeIdx < m_viewInfoCount; eyeIdx++)
	{
		CCamera::EEye eye = (CCamera::EEye)((GetCurrentEye() + eyeIdx) % CCamera::eEye_eCount);

		const CCamera& cam = pRenderView->GetCamera(eye);
		const CCamera& previousCam = pRenderView->GetPreviousCamera(eye);

		m_viewInfo[viewInfoCount].flags = viewFlags;
		m_viewInfo[viewInfoCount].SetCamera(cam, previousCam, m_vProjMatrixSubPixoffset,
		                                    gRenDev->GetDrawNearestFOV(), CRendererCVars::CV_r_DrawNearFarPlane);
		m_viewInfo[viewInfoCount].viewport = pRenderView->GetViewport();
		auto& downscaleFactor = gRenDev->GetRenderQuality().downscaleFactor;
		m_viewInfo[viewInfoCount].downscaleFactor = Vec4(downscaleFactor.x, downscaleFactor.y, gRenDev->m_PrevViewportScale.x, gRenDev->m_PrevViewportScale.y);

		++viewInfoCount;
	}
	m_viewInfoCount = viewInfoCount;
}

//////////////////////////////////////////////////////////////////////////

void CRenderView::SwitchUsageMode(EUsageMode mode)
{
	FUNCTION_PROFILER_RENDERER();

	if (mode == m_usageMode)
		return;

	//assert(mode != m_usageMode);

	if (mode == eUsageModeWriting)
	{
		CRY_ASSERT(m_usageMode == IRenderView::eUsageModeUndefined || m_usageMode == IRenderView::eUsageModeReadingDone);
		CRY_ASSERT(!m_jobstate_Sort.IsRunning());

		m_bPostWriteExecuted = false;
		}

	if (mode == eUsageModeWritingDone)
	{
		CRY_ASSERT(m_usageMode == IRenderView::eUsageModeWriting);
		CRY_ASSERT(!m_jobstate_PostWrite.IsRunning());

		PreparePermanentRenderObjectsForCompile();
		gEnv->pJobManager->AddLambdaJob("JobRenderViewPostWrite", [this] { CRenderView::Job_PostWrite();  }, JobManager::eRegularPriority, &m_jobstate_PostWrite);

		gEnv->pRenderer->WaitAndClearSkinningSimulationJobs();
	}

	if (mode == eUsageModeReading)
	{
		{
			PROFILE_FRAME(RenderViewWaitForPostWriteJob);

			// Prepare view for rasterizing (reading)
			// We now need to wait until all prepare for writing jobs are done.
			m_jobstate_PostWrite.Wait();
			m_jobstate_Sort.Wait();
		}

		//Job_PostWrite();
		CRY_ASSERT(m_bPostWriteExecuted);

		CRY_ASSERT(m_usageMode == IRenderView::eUsageModeWritingDone || m_usageMode == IRenderView::eUsageModeReadingDone);

		if (m_pRenderOutput)
		{
			m_pRenderOutput->BeginRendering(this);
		}

		CalculateViewInfo();

		//CompileModifiedRenderObjects();
		UpdateModifiedShaderItems();
	}

	if (mode == eUsageModeReadingDone)
	{
		CRY_ASSERT(m_usageMode == IRenderView::eUsageModeReading);
		CRY_ASSERT(!m_jobstate_PostWrite.IsRunning());
		CRY_ASSERT(!m_jobstate_Sort.IsRunning());

		if (m_pRenderOutput)
		{
			m_pRenderOutput->EndRendering(this);
		}
	}

	{
		CryAutoLock<CryCriticalSectionNonRecursive> lock(m_lock_UsageMode);
		m_usageMode = mode;
	}
}

void CRenderView::PrepareForRendering()
{
	for (int i = 0; i < EFSLIST_NUM; i++)
	{
		//m_renderItems[i].CoalesceMemory();
		//m_RenderListDesc[0].m_nEndRI[i] = m_renderItems[i].size();
	}
	//ClearBatchFlags();
}

//////////////////////////////////////////////////////////////////////////
void CRenderView::PrepareForWriting()
{
}

//////////////////////////////////////////////////////////////////////////

RenderLightIndex CRenderView::AddDeferredLight(const SRenderLight& pDL, float fMult, const SRenderingPassInfo& passInfo)
{
	CRenderView* pRenderView = passInfo.GetRenderView();
	CRY_ASSERT(pRenderView);

	bool bSort = true;
	bool bAppend = true;

	eDeferredLightType LightType = eDLT_DeferredLight;
	if (pDL.m_Flags & DLF_DEFERRED_CUBEMAPS)
	{
		LightType = eDLT_DeferredCubemap;
		bSort = false;
	}
	else if (pDL.m_Flags & DLF_AMBIENT)
	{
		LightType = eDLT_DeferredAmbientLight;
		bSort = false;
	}

	if (pDL.GetLensOpticsElement() && !pDL.m_pSoftOccQuery)
	{
		SRenderLight* pLight = const_cast<SRenderLight*>(&pDL);

		const uint8 numVisibilityFaders = 2; // For each flare type
		pLight->m_pSoftOccQuery = new CFlareSoftOcclusionQuery(numVisibilityFaders);
	}

	SRenderLight* pAddedLight = nullptr;

	const RenderLightIndex lightsNum = pRenderView->GetLightsCount(LightType);
	RenderLightIndex nLightId = -1;

	if (CRenderer::CV_r_DeferredShadingSortLights == 1 && bSort && lightsNum)
	{
		RenderLightIndex endPoint = lightsNum - 1;
		RenderLightIndex midPoint = 0;
		RenderLightIndex startPoint = 0;

		uint32 lightArea = pDL.m_sWidth * pDL.m_sHeight;

		while (startPoint <= endPoint)  //binary search for insertion point
		{
			midPoint = (endPoint + startPoint) / 2;

			const SRenderLight& compareLight = pRenderView->GetLight(LightType, midPoint);
			uint32 compareLightArea = compareLight.m_sWidth * compareLight.m_sHeight;

			if (lightArea < compareLightArea)
			{
				endPoint = midPoint - 1;
			}
			else
			{
				startPoint = midPoint + 1;
			}
		}

		if (startPoint < lightsNum)
		{
			pAddedLight = pRenderView->AddLightAtIndex(LightType, pDL, startPoint);
			nLightId = startPoint /* => pAddedLight->m_Id */;
			bAppend = false;
		}
	}

	if (bAppend)
	{
		pAddedLight = pRenderView->AddLightAtIndex(LightType, pDL);
		nLightId = pAddedLight->m_Id;
	}

	if (pDL.m_Flags & DLF_LINK_TO_SKY_COLOR)
	{
#if defined(FEATURE_SVO_GI)
		CSvoRenderer* pSR = CSvoRenderer::GetInstance();
		if (pSR)
		{
			pAddedLight->m_Color *= pSR->GetSkyColor();
		}
		else
#endif

		{
			pAddedLight->m_Color *= gEnv->p3DEngine->GetSkyColor();
		}
	}

	IF_LIKELY((pDL.m_Flags & (DLF_DEFERRED_CUBEMAPS | DLF_AMBIENT)) == 0)
	{
		pAddedLight->m_Color *= fMult;
		pAddedLight->m_SpecMult *= fMult;
	}
	else if (pDL.m_Flags & DLF_AMBIENT)
	{
		ColorF origCol(pAddedLight->m_Color);
		pAddedLight->m_Color.lerpFloat(Col_White, origCol, fMult);
	}
	else
	{
		pAddedLight->m_Color.a = fMult; // store fade-out constant separately in alpha channel for deferred cubemaps
	}

	return nLightId;
}

//////////////////////////////////////////////////////////////////////////
RenderLightIndex CRenderView::AddDynamicLight(const SRenderLight& light)
{
	return AddLight(eDLT_DynamicLight, light);
}

//////////////////////////////////////////////////////////////////////////
RenderLightIndex CRenderView::GetDynamicLightsCount() const
{
	return GetLightsCount(eDLT_DynamicLight);
}

//////////////////////////////////////////////////////////////////////////
SRenderLight& CRenderView::GetDynamicLight(RenderLightIndex nLightId)
{
	return GetLight(eDLT_DynamicLight, nLightId);
}

//////////////////////////////////////////////////////////////////////////
RenderLightIndex CRenderView::AddLight(eDeferredLightType lightType, const SRenderLight& light)
{
	CRY_ASSERT((light.m_Flags & DLF_LIGHTTYPE_MASK) != 0);

	RenderLightIndex nLightId = -1;
	if (!(light.m_Flags & DLF_FAKE))
	{
		nLightId = GetLightsCount(lightType);
	}

	m_lights[lightType].push_back(light);
	SRenderLight* pLight = &m_lights[lightType].back();

	pLight->m_Id = nLightId;

	return nLightId;
}

//////////////////////////////////////////////////////////////////////////
SRenderLight* CRenderView::AddLightAtIndex(eDeferredLightType lightType, const SRenderLight& light, RenderLightIndex nLightId /*=-1*/)
{
	CRY_ASSERT(nLightId == -1 || nLightId < GetLightsCount(lightType));

	SRenderLight* pLight = nullptr;
	if (nLightId < 0)
	{
		nLightId = GetLightsCount(lightType);

		m_lights[lightType].push_back(light);
		pLight = &m_lights[lightType].back();
	}
	else
	{
		// find iterator at given index (assuming no [] operator)
		auto itr = m_lights[lightType].begin();
		while (--nLightId >= 0)
			++itr;
		m_lights[lightType].insert(itr, light);

		// invalidate indices of following lights (ID could also be incremented ...)
		auto stp = m_lights[lightType].end();
		while (++itr != stp)
			(*itr).m_Id = -2;
	}

	pLight->m_Id = nLightId;

	return pLight;
}

RenderLightIndex CRenderView::GetLightsCount(eDeferredLightType lightType) const
{
	return (RenderLightIndex)m_lights[lightType].size();
}

SRenderLight& CRenderView::GetLight(eDeferredLightType lightType, RenderLightIndex nLightId)
{
	CRY_ASSERT(nLightId >= 0 && nLightId < m_lights[lightType].size());

	// find iterator at given index (assuming no [] operator)
	auto itr = m_lights[lightType].begin();
	while (--nLightId >= 0)
		++itr;
	return *itr;
}

//////////////////////////////////////////////////////////////////////////
RenderLightsList& CRenderView::GetLightsArray(eDeferredLightType lightType)
{
	assert(lightType >= 0 && lightType < eDLT_NumLightTypes);
	return m_lights[lightType];
}

//////////////////////////////////////////////////////////////////////////
const SRenderLight* CRenderView::GetSunLight() const
{
	for (const auto& iter : m_lights[eDLT_DynamicLight])
	{
		if (iter.m_Flags & DLF_SUN)
			return &iter;
	}
	return nullptr;
}

const Vec4 CRenderView::GetSunLightColor()
{
	Vec4 color;

	if (m_shaderRenderingFlags & SHDF_SECONDARY_VIEWPORT)
	{
		const SRenderLight* sunLight = GetSunLight();
		if (sunLight)
		{
			Vec3 sunColor = sunLight->GetFinalColor().toVec3();
			sunColor *= gcpRendD3D->m_fAdaptedSceneScaleLBuffer;  // Apply LBuffers range rescale
			color = Vec4(sunColor.x, sunColor.y, sunColor.z, 1.0f);
		}
	}
	else // Main viewport
	{
		Vec3 sunColor;
		gEnv->p3DEngine->GetGlobalParameter(E3DPARAM_SUN_COLOR, sunColor);
		sunColor *= gcpRendD3D->m_fAdaptedSceneScaleLBuffer;  // Apply LBuffers range rescale
		color = Vec4(sunColor.x, sunColor.y, sunColor.z, gEnv->p3DEngine->GetGlobalParameter(E3DPARAM_SUN_SPECULAR_MULTIPLIER));
	}

	return color;
}

const Vec3 CRenderView::GetSunLightDirection()
{
	Vec3 lightDir;

	if (m_shaderRenderingFlags & SHDF_SECONDARY_VIEWPORT)
	{
		const SRenderLight* sunLight = GetSunLight();
		if (sunLight)
		{
			lightDir = sunLight->GetPosition().normalized();
		}		
	}
	else
	{
		lightDir = gEnv->p3DEngine->GetSunDirNormalized();
	}

	return lightDir;
}

//////////////////////////////////////////////////////////////////////////
uint8 CRenderView::AddClipVolume(const IClipVolume* pClipVolume)
{
	if (m_clipVolumes.size() < CClipVolumesStage::MaxDeferredClipVolumes)
	{
		m_clipVolumes.push_back(SDeferredClipVolume());

		auto& volume = m_clipVolumes.back();
		volume.nStencilRef = uint8(m_clipVolumes.size()); // the first clip volume ID is reserved for outdoors. See STENCIL_VALUE_OUTDOORS
		volume.nFlags = pClipVolume->GetClipVolumeFlags();
		volume.mAABB = pClipVolume->GetClipVolumeBBox();
		pClipVolume->GetClipVolumeMesh(volume.m_pRenderMesh, volume.mWorldTM);

		return volume.nStencilRef;
	}

	return -1;
}

void CRenderView::SetClipVolumeBlendInfo(const IClipVolume* pClipVolume, int blendInfoCount, IClipVolume** blendVolumes, Plane* blendPlanes)
{
	size_t nClipVolumeIndex = pClipVolume->GetStencilRef() - 1; // 0 is reserved for outdoor
	CRY_ASSERT(blendInfoCount <= SDeferredClipVolume::MaxBlendInfoCount);
	CRY_ASSERT(nClipVolumeIndex >= 0 && nClipVolumeIndex < m_clipVolumes.size() && m_clipVolumes[nClipVolumeIndex].nStencilRef == pClipVolume->GetStencilRef());

	SDeferredClipVolume& volume = m_clipVolumes[nClipVolumeIndex];

	SDeferredClipVolume::BlendInfo emptyBlendInfo = { std::numeric_limits<uint8>::max(), Vec4(ZERO) };
	std::fill(volume.blendInfo, volume.blendInfo + SDeferredClipVolume::MaxBlendInfoCount, emptyBlendInfo);

	for (int i = 0; i < blendInfoCount; ++i)
	{
		volume.blendInfo[i].blendID = blendVolumes[i] ? blendVolumes[i]->GetStencilRef() : 0;
		volume.blendInfo[i].blendPlane = Vec4(blendPlanes[i].n, blendPlanes[i].d);
	}

	volume.nFlags |= IClipVolume::eClipVolumeBlend;
}

//////////////////////////////////////////////////////////////////////////

void CRenderView::AddFogVolume(const CREFogVolume* pFogVolume)
{
	if (pFogVolume == nullptr
	    || pFogVolume->m_volumeType >= IFogVolumeRenderNode::eFogVolumeType_Count)
	{
		return;
	}

	auto& array = m_fogVolumes[pFogVolume->m_volumeType];
	array.push_back(SFogVolumeInfo());
	auto& volume = array.back();

	volume.m_center = pFogVolume->m_center;
	volume.m_viewerInsideVolume = pFogVolume->m_viewerInsideVolume;
	volume.m_affectsThisAreaOnly = pFogVolume->m_affectsThisAreaOnly;
	volume.m_stencilRef = pFogVolume->m_stencilRef;
	volume.m_volumeType = pFogVolume->m_volumeType;
	volume.m_localAABB = pFogVolume->m_localAABB;
	volume.m_matWSInv = pFogVolume->m_matWSInv;
	volume.m_fogColor = pFogVolume->m_fogColor;
	volume.m_globalDensity = pFogVolume->m_globalDensity;
	volume.m_densityOffset = pFogVolume->m_densityOffset;
	volume.m_softEdgesLerp = pFogVolume->m_softEdgesLerp;
	volume.m_heightFallOffDirScaled = pFogVolume->m_heightFallOffDirScaled;
	volume.m_heightFallOffBasePoint = pFogVolume->m_heightFallOffBasePoint;
	volume.m_eyePosInOS = pFogVolume->m_eyePosInOS;
	volume.m_rampParams = pFogVolume->m_rampParams;
	volume.m_windOffset = pFogVolume->m_windOffset;
	volume.m_noiseScale = pFogVolume->m_noiseScale;
	volume.m_noiseFreq = pFogVolume->m_noiseFreq;
	volume.m_noiseOffset = pFogVolume->m_noiseOffset;
	volume.m_noiseElapsedTime = pFogVolume->m_noiseElapsedTime;
	volume.m_emission = pFogVolume->m_emission;
}

const std::vector<SFogVolumeInfo>& CRenderView::GetFogVolumes(IFogVolumeRenderNode::eFogVolumeType volumeType) const
{
	CRY_ASSERT(volumeType < IFogVolumeRenderNode::eFogVolumeType_Count);
	return m_fogVolumes[volumeType];
}

//////////////////////////////////////////////////////////////////////////

void CRenderView::AddCloudBlocker(const Vec3& pos, const Vec3& param, int32 flags)
{
	if (flags == 0)
	{
		if (m_cloudBlockers[0].size() < MaxCloudBlockerWorldSpaceNum)
		{
			SCloudBlocker blocker;
			blocker.position = pos;
			blocker.param = param;
			blocker.flags = flags;
			m_cloudBlockers[0].push_back(blocker);
		}
	}
	else
	{
		if (m_cloudBlockers[1].size() < MaxCloudBlockerScreenSpaceNum)
		{
			SCloudBlocker blocker;
			blocker.position = pos;
			blocker.param = param;
			blocker.flags = flags;
			m_cloudBlockers[1].push_back(blocker);
		}
	}
}

const std::vector<SCloudBlocker>& CRenderView::GetCloudBlockers(uint32 blockerType) const
{
	CRY_ASSERT(blockerType < CloudBlockerTypeNum);
	return m_cloudBlockers[blockerType];
}

//////////////////////////////////////////////////////////////////////////

void CRenderView::AddWaterRipple(const SWaterRippleInfo& waterRippleInfo)
{
	if (m_waterRipples.size() < SWaterRippleInfo::MaxWaterRipplesInScene)
	{
		m_waterRipples.emplace_back(waterRippleInfo);
	}
}

const std::vector<SWaterRippleInfo>& CRenderView::GetWaterRipples() const
{
	return m_waterRipples;
}

//////////////////////////////////////////////////////////////////////////
void CRenderView::AddPolygon(const SRenderPolygonDescription& poly, const SRenderingPassInfo& passInfo)
{
	SShaderItem& shaderItem = const_cast<SShaderItem&>(poly.shaderItem);

	assert(shaderItem.m_pShader && shaderItem.m_pShaderResources);
	if (!shaderItem.m_pShader || !shaderItem.m_pShaderResources)
	{
		Warning("CRenderView::AddPolygon without shader...");
		return;
	}
	if (shaderItem.m_nPreprocessFlags == -1)
	{
		if (!shaderItem.Update())
			return;
	}

	//////////////////////////////////////////////////////////////////////////
	// Get next client poly from pool
	int nIndex = m_numUsedClientPolygons++;
	if (nIndex >= m_polygonsPool.size())
	{
		m_polygonsPool.push_back(new CREClientPoly);
	}
	CREClientPoly* pl = m_polygonsPool[nIndex];
	//////////////////////////////////////////////////////////////////////////

	//////////////////////////////////////////////////////////////////////////
	// Fill CREClientPoly structure
	pl->AssignPolygon(poly, passInfo, GetPolygonDataPool());

	//////////////////////////////////////////////////////////////////////////
	// Add Render Item directly here
	//////////////////////////////////////////////////////////////////////////
	uint32 batchFlags = FB_GENERAL;

	ERenderListID renderListId = (ERenderListID)poly.renderListId;

	bool bSkipAdding = false;

	if (!IsShadowGenView())
	{
		batchFlags |= (pl->m_nCPFlags & CREClientPoly::efAfterWater) ? 0 : FB_BELOW_WATER;

		CShader* pShader = (CShader*)pl->m_Shader.m_pShader;
		CShaderResources* const __restrict pShaderResources = static_cast<CShaderResources*>(pl->m_Shader.m_pShaderResources);
		SShaderTechnique* pTech = pl->m_Shader.GetTechnique();

		if (pl->m_Shader.m_nPreprocessFlags & FSPR_MASK)
		{
			renderListId = EFSLIST_PREPROCESS;
		}

		if ((pShader->GetFlags() & EF_DECAL) || (poly.pRenderObject && (poly.pRenderObject->m_ObjFlags & FOB_DECAL)))
		{
			if (pTech && pTech->m_nTechnique[TTYPE_Z] > 0 && (pShader && (pShader->m_Flags & EF_SUPPORTSDEFERREDSHADING)))
			{
				batchFlags |= FB_Z;
			}

			if (!(pl->m_nCPFlags & CREClientPoly::efShadowGen))
				renderListId = EFSLIST_DECAL;
		}
		else
		{
			renderListId = EFSLIST_GENERAL;
			if ((poly.pRenderObject && poly.pRenderObject->m_fAlpha < 1.0f)
					|| (pShaderResources && pShaderResources->IsTransparent()))
			{
				renderListId = !(poly.pRenderObject->m_ObjFlags & FOB_AFTER_WATER) ? EFSLIST_TRANSP_BW : EFSLIST_TRANSP_AW;
				batchFlags |= FB_TRANSPARENT;
			}
		}
	}
	else
	{
		renderListId = (ERenderListID)passInfo.ShadowFrustumSide(); // in shadow map rendering render list is the shadow frustum side

		// Rendering polygon into the shadow view if supported
		if (!(pl->m_nCPFlags & CREClientPoly::efShadowGen))
		{
			bSkipAdding = true;
		}
	}

	AddRenderItem(pl, poly.pRenderObject, poly.shaderItem, renderListId, batchFlags, passInfo, SRendItemSorter());
}

//////////////////////////////////////////////////////////////////////////
CTexture* CRenderView::GetColorTarget() const
{
	// Toggle back-buffer on first use
	if (m_bRenderToSwapChain)
		m_pRenderOutput->GetDisplayContext()->PostPresent();

	CRY_ASSERT(m_pColorTarget);
	return m_pColorTarget.get();
}

CTexture* CRenderView::GetDepthTarget() const
{
	if (m_pRenderOutput && !m_pDepthTarget)
		return m_pRenderOutput->GetDepthTarget();

	CRY_ASSERT(m_pDepthTarget);
	return m_pDepthTarget.get();
}

void CRenderView::AssignRenderOutput(CRenderOutputPtr pRenderOutput)
{
	m_pRenderOutput = pRenderOutput;
	InspectRenderOutput();
}

void CRenderView::InspectRenderOutput()
{
	if (CRenderOutput* pRenderOutput = m_pRenderOutput.get())
	{
		SRenderViewport vp = pRenderOutput->GetViewport();

		int outputWidth = (vp.width - vp.x);
		CRY_ASSERT(pRenderOutput->GetOutputResolution()[0] == outputWidth);
		int outputHeight = (vp.height - vp.y);
		CRY_ASSERT(pRenderOutput->GetOutputResolution()[1] == outputHeight);

		// Calculate the rendering resolution based on inputs ///////////////////////////////////////////////////////////
		int renderWidth  = outputWidth;
		int renderHeight = outputHeight;

		if (CRenderDisplayContext* pDisplayContext = pRenderOutput->GetDisplayContext())
		{
			const int nMaxResolutionX = std::max(std::min(CRendererCVars::CV_r_CustomResMaxSize, gcpRendD3D.m_MaxTextureSize), outputWidth);
			const int nMaxResolutionY = std::max(std::min(CRendererCVars::CV_r_CustomResMaxSize, gcpRendD3D.m_MaxTextureSize), outputHeight);

			int nSSSamplesX = pDisplayContext->m_nSSSamplesX; do { renderWidth  = outputWidth  * nSSSamplesX; --nSSSamplesX; } while (renderWidth  > nMaxResolutionX);
			int nSSSamplesY = pDisplayContext->m_nSSSamplesY; do { renderHeight = outputHeight * nSSSamplesY; --nSSSamplesX; } while (renderHeight > nMaxResolutionY);
		}

		ChangeRenderResolution(renderWidth, renderHeight, true);

		// Assign Render viewport from Render Output
		// Render viewport is always in the top left corner
		SetViewport(SRenderViewport(0, 0, renderWidth, renderHeight));
	}
}

void CRenderView::ChangeRenderResolution(uint32_t renderWidth, uint32_t renderHeight, bool bForce)
{
	// Rule is: factor is natural number
	CRY_ASSERT((renderWidth  % GetOutputResolution()[0]) == 0);
	CRY_ASSERT((renderHeight % GetOutputResolution()[1]) == 0);

	// No changes do not need to resize
	if (m_RenderWidth  == renderWidth &&
	    m_RenderHeight == renderHeight && !bForce)
	{
		CRY_ASSERT(m_pDepthTarget->GetWidth() >= renderWidth && m_pDepthTarget->GetHeight() >= renderHeight);
		CRY_ASSERT(m_pColorTarget->GetWidth() >= renderWidth && m_pColorTarget->GetHeight() >= renderHeight);
		return;
	}

	m_RenderWidth  = renderWidth;
	m_RenderHeight = renderHeight;

	if (renderWidth  == GetOutputResolution()[0] &&
	    renderHeight == GetOutputResolution()[1] && m_pRenderOutput) 
	{
		m_pColorTarget = m_pRenderOutput->GetColorTarget();
		m_pDepthTarget = m_pRenderOutput->GetDepthTarget();

		m_bRenderToSwapChain = m_pRenderOutput->m_bRenderToSwapChain;
	}
	else
	{
		m_pColorTarget = m_pGraphicsPipeline->GetPipelineResources().m_pTexHDRTarget;
		m_pDepthTarget = nullptr; // Allocate temporary depth target
		m_pDepthTarget.Assign_NoAddRef(CRendererResources::CreateDepthTarget(renderWidth, renderHeight, Clr_Transparent, eTF_Unknown));

		m_bRenderToSwapChain = false;
	}

	CRY_ASSERT(m_pColorTarget->GetWidth() >= renderWidth && m_pColorTarget->GetHeight() >= renderHeight);
}

void CRenderView::UnsetRenderOutput()
{
	m_pRenderOutput.reset();
	m_pColorTarget = nullptr;
	m_pDepthTarget = nullptr;
}

//////////////////////////////////////////////////////////////////////////
template<bool shadowView, class T>
void CountRenderItems(T& permanentRenderItems, std::atomic<size_t> (&addedItems)[EFSLIST_NUM], int frustumSide = 0)
{
	size_t numItems = permanentRenderItems.size();
	assert(numItems < 128); // Sanity check, otherwise too many chunks in the mesh
	for (size_t i = 0; i < numItems; i++)
	{
		auto& RESTRICT_REFERENCE pri = permanentRenderItems[i];

		SShaderItem shaderItem;
		SRendItem::ExtractShaderItem(pri.m_sortValue, pri.m_nBatchFlags, shaderItem);

		CShaderResources* const __restrict pShaderResources = (CShaderResources*)shaderItem.m_pShaderResources;

		// Discard 0 alpha blended geometry - this should be discarded earlier on 3dengine side preferably
		const bool isInvisible = pShaderResources && pShaderResources->::CShaderResources::IsInvisible();
		if (!isInvisible)
		{
			ERenderListID renderList;
			if (shadowView)
				renderList = ERenderListID(frustumSide);
			else
				renderList = pri.m_nRenderList;

			++addedItems[renderList];
		}
	}
}

void CRenderView::AddPermanentObjectImpl(CPermanentRenderObject* pObject, const SRenderingPassInfo& passInfo)
{
	auto passType = GetPassType();
	m_permanentObjects.push_back(SPermanentObjectRecord{ pObject, passInfo.GetRendItemSorter().GetValue(), passInfo.ShadowFrustumSide(), pObject->IsInstanceDataDirty(passType) });

	if (IsShadowGenView())
	{
		for (CPermanentRenderObject* pRenderObject = pObject; pRenderObject; pRenderObject = static_cast<CPermanentRenderObject*>(pRenderObject->m_pNextPermanent))
		{
			// Simulate the number of calls to AddRenderObject()
			const bool isInvisible = !pRenderObject->m_fAlpha;
			if (!isInvisible)
				CountRenderItems<true>(pRenderObject->m_permanentRenderItems[passType], m_addedItems, passInfo.ShadowFrustumSide());

			if (pRenderObject->m_ObjFlags & FOB_NEAREST)
				m_shadows.AddNearestCaster(pRenderObject, passInfo);
		}

		CRY_ASSERT(!m_shadows.m_pShadowFrustumOwner->bRestrictToRT);
		if (m_shadows.m_pShadowFrustumOwner->IsCached())
		{
			if (IRenderNode* pNode = pObject->m_pRenderNode)
				m_shadows.m_pShadowFrustumOwner->MarkNodeAsCached(pNode);
		}
	}
	else
	{
		for (CPermanentRenderObject* pRenderObject = pObject; pRenderObject; pRenderObject = static_cast<CPermanentRenderObject*>(pRenderObject->m_pNextPermanent))
		{
			// Simulate the number of calls to AddRenderObject()
			const bool isInvisible = !pRenderObject->m_fAlpha;
			if (!isInvisible)
				CountRenderItems<false>(pRenderObject->m_permanentRenderItems[passType], m_addedItems);
		}
	}
}

//////////////////////////////////////////////////////////////////////////
void CRenderView::AddPermanentObject(CRenderObject* pObject, const SRenderingPassInfo& passInfo)
{
	assert(pObject->m_bPermanent);
	AddPermanentObjectImpl(static_cast<CPermanentRenderObject*>(pObject), passInfo);
}

//////////////////////////////////////////////////////////////////////////
CRenderObject* CRenderView::AllocateTemporaryRenderObject()
{
	CRenderObject* pObj = m_tempRenderObjects.push_back_new();

	pObj->Init();
	pObj->m_pCompiledObject = nullptr;

	return pObj;
}

//////////////////////////////////////////////////////////////////////////
void CRenderView::SetViewport(const SRenderViewport& viewport)
{
	m_viewport = viewport;

	for (CCamera::EEye eye = CCamera::eEye_Left; eye != CCamera::eEye_eCount; eye = CCamera::EEye(eye + 1))
		m_viewInfo[eye].viewport = viewport;
}

//////////////////////////////////////////////////////////////////////////
const SRenderViewport& CRenderView::GetViewport() const
{
	return m_viewport;
}

//////////////////////////////////////////////////////////////////////////

void CRenderView::SetGraphicsPipeline(std::shared_ptr<CGraphicsPipeline> pipeline)
{
	m_pGraphicsPipeline = pipeline;
	m_shaderRenderingFlags = m_pGraphicsPipeline->GetPipelineDescription().shaderFlags;
}

//////////////////////////////////////////////////////////////////////////
static inline uint32 CalculateRenderItemBatchFlags(SShaderItem& shaderItem, CRenderObject* pObj, CRenderElement* re, const SRenderingPassInfo& passInfo, int nAboveWater) threadsafe
{
	uint32 nBatchFlags = shaderItem.m_nPreprocessFlags & FB_MASK;

	SShaderTechnique* const __restrict pTech = shaderItem.GetTechnique();
	CShaderResources* const __restrict pShaderResources = (CShaderResources*)shaderItem.m_pShaderResources;
	CShader* const __restrict pShader = (CShader*)shaderItem.m_pShader;

	//float fAlpha = pObj->m_fAlpha;
	uint32 uTransparent = 0; //(bool)(fAlpha < 1.0f); Not supported in new rendering pipeline
	const ERenderObjectFlags ObjFlags = pObj->m_ObjFlags;

	if (!passInfo.IsRecursivePass() && pTech)
	{
		const uint32 DecalFlags = pShader->m_Flags & EF_DECAL;
		const bool isDecalOrOverlay = DecalFlags != 0;

		CryPrefetch(pTech->m_nTechnique);
		CryPrefetch(pShaderResources);

		//if (pObj->m_fAlpha < 1.0f) nFlags |= FB_TRANSPARENT;
		nBatchFlags |= FB_TRANSPARENT * uTransparent;

		if (!((nBatchFlags & FB_Z) && (!(pObj->m_RState & OS_NODEPTH_WRITE) || (pShader->m_Flags2 & EF2_FORCE_ZPASS))))
			nBatchFlags &= ~FB_Z;

		if ((ObjFlags & FOB_DECAL) || (ObjFlags & FOB_TERRAIN_LAYER) || isDecalOrOverlay)
			nBatchFlags &= ~FB_ZPREPASS;

		pObj->m_ObjFlags |= (nBatchFlags & FB_ZPREPASS) ? FOB_ZPREPASS : FOB_NONE;

		if (passInfo.IsShadowPass())
			nBatchFlags &= ~FB_PREPROCESS;

		nBatchFlags &= ~(FB_PREPROCESS & uTransparent);

		const uint32 nMaterialLayers = pObj->m_nMaterialLayers;
		if ((nMaterialLayers & ~uTransparent) && (CRenderer::CV_r_usemateriallayers > 0))
		{
			const uint32 nResourcesNoDrawFlags = static_cast<CShaderResources*>(pShaderResources)->CShaderResources::GetMtlLayerNoDrawFlags();

			// if ((nMaterialLayers & MTL_LAYER_BLEND_DYNAMICFROZEN) && !(nResourcesNoDrawFlags & MTL_LAYER_FROZEN))
			uint32 uMask = mask_nz_zr(nMaterialLayers & MTL_LAYER_BLEND_DYNAMICFROZEN, nResourcesNoDrawFlags & MTL_LAYER_FROZEN);

			//if ((nMaterialLayers & MTL_LAYER_BLEND_CLOAK) && !(nResourcesNoDrawFlags&MTL_LAYER_CLOAK))
			uMask = mask_nz_zr(nMaterialLayers & MTL_LAYER_BLEND_CLOAK, nResourcesNoDrawFlags & MTL_LAYER_CLOAK);

			//prevent general pass when fully cloaked
			nBatchFlags &= ~(uMask & (FB_TRANSPARENT | (((nMaterialLayers & MTL_LAYER_BLEND_CLOAK) == MTL_LAYER_BLEND_CLOAK) ? FB_GENERAL : 0)));
			nBatchFlags |= uMask & (FB_TRANSPARENT);
		}

		//if ( ((ObjFlags & (FOB_DECAL)) | DecalFlags) == 0 ) // put the mask below
		{
			if (pTech->m_nTechnique[TTYPE_MOTIONBLURPASS] > 0 && (ObjFlags & FOB_HAS_PREVMATRIX) && CRenderer::CV_r_MotionVectors)
			{
				uint32 uMask = mask_zr_zr(ObjFlags & (FOB_DECAL), DecalFlags);
				nBatchFlags |= FB_MOTIONBLUR & uMask;
			}
		}

		// apply motion blur to skinned vegetation when it moves (for example breaking trees)
		if (pTech->m_nTechnique[TTYPE_MOTIONBLURPASS] > 0 && ((ObjFlags & FOB_SKINNED) != 0) && ((ObjFlags & FOB_HAS_PREVMATRIX) != 0) && CRenderer::CV_r_MotionVectors)
		{
			nBatchFlags |= FB_MOTIONBLUR;
		}

		SRenderObjData* pOD = pObj->GetObjData();
		if (pTech->m_nTechnique[TTYPE_CUSTOMRENDERPASS] > 0)
		{
			const uint32 nVisionParams = (pOD && pOD->m_nVisionParams);
			if (gRenDev->m_nThermalVisionMode && (pShaderResources->HeatAmount_unnormalized() || nVisionParams))
				nBatchFlags |= FB_CUSTOM_RENDER;

			const uint32 customvisions = CRenderer::CV_r_customvisions;
			const uint32 nHUDSilhouettesParams = (pOD && pOD->m_nHUDSilhouetteParams);
			if (customvisions && nHUDSilhouettesParams)
			{
				nBatchFlags |= FB_CUSTOM_RENDER;
			}
		}

		if (pOD->m_nCustomFlags & COB_POST_3D_RENDER)
		{
			nBatchFlags |= FB_POST_3D_RENDER;
		}

		if (nBatchFlags & FB_LAYER_EFFECT)
		{
			if ((!pOD->m_pLayerEffectParams) && !CRenderer::CV_r_DebugLayerEffect)
				nBatchFlags &= ~FB_LAYER_EFFECT;
		}

		if (pShaderResources && pShaderResources->IsAlphaTested())
			pObj->m_ObjFlags |= FOB_ALPHATEST;
	}
	else if (passInfo.IsRecursivePass() && pTech && passInfo.GetRenderView()->IsViewFlag(SRenderViewInfo::eFlags_MirrorCamera))
	{
		nBatchFlags &= (FB_TRANSPARENT | FB_GENERAL);
		nBatchFlags |= FB_TRANSPARENT * uTransparent;
		// if (pObj->m_fAlpha < 1.0f) nFlags |= FB_TRANSPARENT;
	}

	{
		//if ( (objFlags & FOB_ONLY_Z_PASS) || CRenderer::CV_r_ZPassOnly) && !(nFlags & (FB_TRANSPARENT))) - put it to the mask
		const uint32 mask = mask_nz_zr((uint32)CRenderer::CV_r_ZPassOnly, nBatchFlags & (FB_TRANSPARENT));

		nBatchFlags = iselmask(mask, FB_Z, nBatchFlags);
	}

	nBatchFlags |= nAboveWater ? 0 : FB_BELOW_WATER;

	// Cloak also requires resolve
	const uint32 nCloakMask = mask_nz_zr(pObj->m_nMaterialLayers & MTL_LAYER_BLEND_CLOAK, (pShaderResources ? pShaderResources->CShaderResources::GetMtlLayerNoDrawFlags() & MTL_LAYER_CLOAK : 0));
	if (nCloakMask)
		nBatchFlags |= FB_REFRACTION;

	const uint32 nMaterialLayers = pObj->m_nMaterialLayers;
	// Need to differentiate between something rendered with cloak layer material, and sorted with cloak.
	// e.g. ironsight glows on gun should be sorted with cloak to not write depth - can be inconsistent with no depth from gun.
	const uint32 nCloakRenderedMask = pShaderResources && mask_nz_zr(nMaterialLayers & MTL_LAYER_BLEND_CLOAK, pShaderResources->CShaderResources::GetMtlLayerNoDrawFlags() & MTL_LAYER_CLOAK);

	if ((pShader->m_Flags & EF_REFRACTIVE) || nCloakRenderedMask)
		nBatchFlags |= FB_REFRACTION;
	if (pShader->m_Flags & EF_FORCEREFRACTIONUPDATE)
		nBatchFlags |= FB_RESOLVE_FULL;

	return nBatchFlags;
}

///////////////////////////////////////////////////////////////////////////////
static inline void AddEf_HandleOldRTMask(CRenderObject* obj) threadsafe
{
	ERenderObjectFlags objFlags = obj->m_ObjFlags;
	uint32 ortFlags = 0;

	objFlags |= FOB_UPDATED_RTMASK;
	if (objFlags & (FOB_NEAREST | FOB_DECAL_TEXGEN_2D | FOB_DISSOLVE | FOB_SOFT_PARTICLE))
	{
		if (objFlags & FOB_DECAL_TEXGEN_2D)
			ortFlags |= g_HWSR_MaskBit[HWSR_DECAL_TEXGEN_2D];

		if (objFlags & FOB_NEAREST)
			ortFlags |= g_HWSR_MaskBit[HWSR_NEAREST];

		if (objFlags & FOB_DISSOLVE)
			ortFlags |= g_HWSR_MaskBit[HWSR_DISSOLVE];

		if (CRenderer::CV_r_ParticlesSoftIsec && (objFlags & FOB_SOFT_PARTICLE))
			ortFlags |= g_HWSR_MaskBit[HWSR_SOFT_PARTICLE];
	}

	obj->m_ObjFlags = objFlags;
	obj->m_nRTMask = ortFlags;
}

///////////////////////////////////////////////////////////////////////////////
static inline void AddEf_HandleForceFlags(int& nList, int& nAW, uint32& nBatchFlags, const uint32 nShaderFlags, const uint32 nShaderFlags2, CRenderObject* obj) threadsafe
{
	// Force rendering in last place
	// FIXME: If object is permanent this is wrong!
	// branch-less

	const int32 drawlast = nz2mask(nShaderFlags2 & EF2_FORCE_DRAWLAST);
	const int32 drawfrst = nz2one(nShaderFlags2 & EF2_FORCE_DRAWFIRST);
	float fSort = static_cast<float>(100000 * (drawlast + drawfrst));

	if (nShaderFlags2 & EF2_FORCE_ZPASS && !(nShaderFlags & EF_REFRACTIVE))
		nBatchFlags |= FB_Z;

	{
		// below branch-less version of:
		//if      (nShaderFlags2 & EF2_FORCE_TRANSPASS  ) nList = EFSLIST_TRANSP;
		//else if (nShaderFlags2 & EF2_FORCE_GENERALPASS) nList = EFSLIST_GENERAL;
		//else if (nShaderFlags2 & EF2_FORCE_WATERPASS  ) nList = EFSLIST_WATER;

		uint32 mb1 = nShaderFlags2 & EF2_FORCE_TRANSPASS;
		uint32 mb2 = nShaderFlags2 & EF2_FORCE_GENERALPASS;
		uint32 mb3 = nShaderFlags2 & EF2_FORCE_WATERPASS;

		mb1 = nz2msb(mb1);
		mb2 = nz2msb(mb2) & ~mb1;
		mb3 = nz2msb(mb3) & ~(mb1 ^ mb2);

		mb1 = msb2mask(mb1);
		mb2 = msb2mask(mb2);
		mb3 = msb2mask(mb3);

		const uint32 mask = mb1 | mb2 | mb3;
		mb1 &= EFSLIST_TRANSP_AW;
		mb2 &= EFSLIST_GENERAL;
		mb3 &= EFSLIST_WATER;

		nList = iselmask(mask, mb1 | mb2 | mb3, nList);
	}

	const uint32 afterhdr = nz2mask(nShaderFlags2 & EF2_AFTERHDRPOSTPROCESS);
	const uint32 afterldr = nz2mask(nShaderFlags2 & EF2_AFTERPOSTPROCESS) | (afterhdr & drawlast);

	nList = iselmask(afterhdr, EFSLIST_AFTER_HDRPOSTPROCESS, nList);
	nList = iselmask(afterldr, EFSLIST_AFTER_POSTPROCESS, nList);

	//if (nShaderFlags2 & EF2_FORCE_DRAWAFTERWATER) nAW = 1;   -> branchless
	nAW |= nz2one(nShaderFlags2 & EF2_FORCE_DRAWAFTERWATER);

	obj->m_fSort += fSort;
}

//////////////////////////////////////////////////////////////////////////
static inline ERenderListID CalculateRenderItemList(const SShaderItem& shaderItem, CRenderObject* pObj, uint32& nBatchFlags, int nList, const SRenderingPassInfo& passInfo, int nAboveWater) threadsafe
{
	CShaderResources* const __restrict pShaderResources = (CShaderResources*)shaderItem.m_pShaderResources;
	const uint32 nMaterialLayers = pObj->m_nMaterialLayers;

	CShader* const __restrict pSH = (CShader*)shaderItem.m_pShader;
	const uint32 nShaderFlags = pSH->m_Flags;

	if (pObj->m_ObjFlags & FOB_DECAL)
	{
		// Send all decals to decals list
		nList = EFSLIST_DECAL;
	}

	// Need to differentiate between something rendered with cloak layer material, and sorted with cloak.
	// e.g. ironsight glows on gun should be sorted with cloak to not write depth - can be inconsistent with no depth from gun.
	const uint32 nCloakRenderedMask = pShaderResources && mask_nz_zr(nMaterialLayers & MTL_LAYER_BLEND_CLOAK, pShaderResources ? pShaderResources->CShaderResources::GetMtlLayerNoDrawFlags() & MTL_LAYER_CLOAK : 0);
	uint32 nCloakLayerMask = nz2mask(nMaterialLayers & MTL_LAYER_BLEND_CLOAK);

	const uint32 nRenderlistsFlags = (FB_PREPROCESS | FB_TRANSPARENT);
	if (nBatchFlags & nRenderlistsFlags || nCloakLayerMask)
	{
		const auto transparentList = !!(nBatchFlags & FB_BELOW_WATER) ? EFSLIST_TRANSP_BW : EFSLIST_TRANSP_AW;

		// branchless version of:
		//if      (pSH->m_Flags & FB_REFRACTIVE || nCloakLayerMask)           nList = EFSLIST_TRANSP, nBatchFlags &= ~FB_Z;
		//else if((nBatchFlags & FB_TRANSPARENT) && nList == EFSLIST_GENERAL) nList = EFSLIST_TRANSP;

		// Refractive objects go into same list as transparent objects - partial resolves support
		// arbitrary ordering between transparent and refractive correctly.

		uint32 mx1 = (nShaderFlags & EF_REFRACTIVE) | nCloakLayerMask;
		uint32 mx2 = mask_nz_zr(nBatchFlags & FB_TRANSPARENT, (nList ^ EFSLIST_GENERAL) | mx1);

		nBatchFlags &= iselmask(mx1 = nz2mask(mx1), ~FB_Z, nBatchFlags);
		nList = iselmask(mx1 | mx2, (mx1 & transparentList) | (mx2 & transparentList), nList);
	}

	// FogVolume contribution for transparencies isn't needed when volumetric fog is turned on.
	if ((((nBatchFlags & FB_TRANSPARENT) || (pSH->GetFlags2() & EF2_HAIR)) && !gRenDev->m_bVolumetricFogEnabled)
		|| passInfo.IsRecursivePass() /* account for recursive scene traversal done in forward fashion*/)
	{
		SRenderObjData* pOD = pObj->GetObjData();
		if (pOD && !pOD->m_FogVolumeContribIdx)
		{
			I3DEngine* pEng = gEnv->p3DEngine;
			ColorF newContrib;
			pEng->TraceFogVolumes(pObj->GetMatrix().GetTranslation(), newContrib, passInfo);

			pOD->m_FogVolumeContribIdx = passInfo.GetRenderView()->PushFogVolumeContribution(newContrib, passInfo);
		}
	}
	//if (nList != EFSLIST_GENERAL && nList != EFSLIST_TERRAINLAYER) nBatchFlags &= ~FB_Z;
	nBatchFlags &= ~(FB_Z & mask_nz_nz(nList ^ EFSLIST_GENERAL, nList ^ EFSLIST_TERRAINLAYER));

	nList = (nBatchFlags & FB_EYE_OVERLAY) ? EFSLIST_EYE_OVERLAY : nList;

	const EShaderDrawType shaderDrawType = pSH->m_eSHDType;
	const uint32 nShaderFlags2 = pSH->m_Flags2;
	const ERenderObjectFlags ObjDecalFlag = pObj->m_ObjFlags & FOB_DECAL;

	// make sure decals go into proper render list
	// also, set additional shadow flag (i.e. reuse the shadow mask generated for underlying geometry)
	// TODO: specify correct render list and additional flags directly in the engine once non-material decal rendering support is removed!
	if ((ObjDecalFlag || (nShaderFlags & EF_DECAL)))
	{
		// BK: Drop decals that are refractive (and cloaked!). They look bad if forced into refractive pass,
		// and break if they're in the decal pass
		if (nShaderFlags & (EF_REFRACTIVE | EF_FORCEREFRACTIONUPDATE) || nCloakRenderedMask)
			return ERenderListID(nList);

		//SShaderTechnique *pTech = SH.GetTechnique();
		//if (pTech && pTech->m_nTechnique[TTYPE_Z] > 0 && ((nShaderFlags2 & EF2_FORCE_ZPASS) || CV_r_deferredshading)) // deferred shading always enabled
		if (nShaderFlags & EF_SUPPORTSDEFERREDSHADING_FULL)
		{
			nBatchFlags |= FB_Z;
		}

		nList = EFSLIST_DECAL;
		pObj->m_ObjFlags |= FOB_INSHADOW;

		if (ObjDecalFlag == 0 && pShaderResources)
			pObj->m_nSort = pShaderResources->m_SortPrio;
	}

	// Enable tessellation for water geometry
	pObj->m_ObjFlags |= (pSH->m_Flags2 & EF2_HW_TESSELLATION && pSH->m_eShaderType == eST_Water) ? FOB_ALLOW_TESSELLATION : FOB_NONE;

	const uint32 nForceFlags = (EF2_FORCE_DRAWLAST | EF2_FORCE_DRAWFIRST | EF2_FORCE_ZPASS | EF2_FORCE_TRANSPASS | EF2_FORCE_GENERALPASS | EF2_FORCE_DRAWAFTERWATER | EF2_FORCE_WATERPASS | EF2_AFTERHDRPOSTPROCESS | EF2_AFTERPOSTPROCESS);

	if (nShaderFlags2 & nForceFlags)
	{
		AddEf_HandleForceFlags(nList, nAboveWater, nBatchFlags, nShaderFlags, nShaderFlags2, pObj);
	}

	// Always force cloaked geometry to render after water
	//if (obj->m_nMaterialLayers & MTL_LAYER_BLEND_CLOAK) nAW = 1;   -> branchless
	nAboveWater |= nz2one(pObj->m_nMaterialLayers & MTL_LAYER_BLEND_CLOAK);

	// final step, for post 3d items, remove them from any other list than POST_3D_RENDER
	// (have to do this here as the batch needed to go through the normal nList assign path first)
	nBatchFlags = iselmask(nz2mask(nBatchFlags & FB_POST_3D_RENDER), FB_POST_3D_RENDER, nBatchFlags);

	// No need to sort opaque passes by water/after water. Ensure always on same list for more coherent sorting
	nAboveWater |= nz2one((nList == EFSLIST_GENERAL) | (nList == EFSLIST_TERRAINLAYER) | (nList == EFSLIST_DECAL));

#ifndef _RELEASE
	nList = (shaderDrawType == eSHDT_DebugHelper) ? EFSLIST_DEBUG_HELPER : nList;
#endif

	const bool bShadowPass = passInfo.IsShadowPass();
	if (!bShadowPass)
	{
		CGraphicsPipeline* pGraphicsPipeline = gcpRendD3D->FindGraphicsPipeline(passInfo.GetGraphicsPipelineKey()).get();
		const bool bForceOpaqueForward = passInfo.IsAuxWindow() && (pGraphicsPipeline->GetPipelineDescription().shaderFlags & SHDF_FORWARD_MINIMAL);

		const bool bHair = (pSH->m_Flags2 & EF2_HAIR) != 0;
		const bool bRefractive = (pSH->m_Flags & EF_REFRACTIVE) != 0;
		const bool bTransparent = pShaderResources && pShaderResources->IsTransparent();

		if ((nList == EFSLIST_GENERAL && (bHair || bTransparent)) ||
			((nList == EFSLIST_TRANSP_BW || nList == EFSLIST_TRANSP_AW) && bRefractive))
		{
			const auto transparentList = !(pObj->m_ObjFlags & FOB_AFTER_WATER) ? EFSLIST_TRANSP_BW : EFSLIST_TRANSP_AW;
			nList = (pObj->m_ObjFlags & FOB_NEAREST) ? EFSLIST_TRANSP_NEAREST : transparentList;
		}
		else if (nList == EFSLIST_GENERAL && (!(pSH->m_Flags & EF_SUPPORTSDEFERREDSHADING_FULL) || bForceOpaqueForward))
		{
			// Redirect general list items to the forward opaque list when they _only_ support Forward+
			nList = (pObj->m_ObjFlags & FOB_NEAREST) ? EFSLIST_FORWARD_OPAQUE_NEAREST : EFSLIST_FORWARD_OPAQUE;
		}
	}

	// objects with FOB_NEAREST go to EFSLIST_NEAREST in shadow pass and general (but not FORWARD_OPAQUE)
	if ((pObj->m_ObjFlags & FOB_NEAREST) && (bShadowPass || (nList == EFSLIST_GENERAL && CRenderer::CV_r_GraphicsPipeline > 0)))
	{
		nList = EFSLIST_NEAREST_OBJECTS;
	}

	// Redirect all sky shader draw types shaders to the SKY-list
	if (shaderDrawType == eSHDT_Sky)
	{
		nList = EFSLIST_SKY;
	}

	return ERenderListID(nList);
}

///////////////////////////////////////////////////////////////////////////////
void CRenderView::AddRenderObject(CRenderElement* pElem, SShaderItem& shaderItem, CRenderObject* pObj, const SRenderingPassInfo& passInfo, int nSuggestedList, int nAW) threadsafe
{
	assert(nSuggestedList > 0 && nSuggestedList < EFSLIST_NUM);
	if (!pElem || !shaderItem.m_pShader)
		return;

	// shader item is not set up yet
	if (shaderItem.m_nPreprocessFlags == -1)
	{
		if (pObj->m_bPermanent && pObj->m_pRenderNode)
			pObj->m_pRenderNode->InvalidatePermanentRenderObject();

		return;
	}

	CShader* const __restrict pShader = (CShader*)shaderItem.m_pShader;
	const uint32 nShaderFlags = pShader->m_Flags;
	if (nShaderFlags & EF_NODRAW)
		return;

	if (passInfo.IsShadowPass())
	{
		if (pShader->m_HWTechniques.Num() && pShader->m_HWTechniques[0]->m_nTechnique[TTYPE_SHADOWGEN] >= 0)
		{
 			passInfo.GetRenderView()->AddRenderItem(pElem, pObj, shaderItem, ERenderListID(passInfo.ShadowFrustumSide()), FB_GENERAL, passInfo, passInfo.GetRendItemSorter());
		}
		return;
	}

	CShaderResources* const __restrict pShaderResources = (CShaderResources*)shaderItem.m_pShaderResources;

	// Discard 0 alpha blended geometry - this should be discarded earlier on 3dengine side preferably
	const bool isInvisible = !pObj->m_fAlpha || (pShaderResources && pShaderResources->IsInvisible());
	if (isInvisible)
		return;

#ifdef _DEBUG
	static float sMatIdent[12] =
	{
		1, 0, 0, 0,
		0, 1, 0, 0,
		0, 0, 1, 0
	};

	if (memcmp(sMatIdent, pObj->GetMatrix().GetData(), 3 * 4 * 4))
	{
		if (!(pObj->m_ObjFlags & FOB_TRANS_MASK))
		{
			assert(0);
		}
	}
#endif

	if (!(pObj->m_ObjFlags & FOB_UPDATED_RTMASK))
	{
		AddEf_HandleOldRTMask(pObj);
	}

	uint32 nBatchFlags = CalculateRenderItemBatchFlags(shaderItem, pObj, pElem, passInfo, nAW);
	ERenderListID nRenderList = CalculateRenderItemList(shaderItem, pObj, nBatchFlags, nSuggestedList, passInfo, nAW);

	passInfo.GetRenderView()->AddRenderItem(pElem, pObj, shaderItem, nRenderList, nBatchFlags, passInfo, passInfo.GetRendItemSorter());
}

//////////////////////////////////////////////////////////////////////////
void CRenderView::AddRenderItem(CRenderElement* pElem, CRenderObject* RESTRICT_POINTER pObj, const SShaderItem& shaderItem, ERenderListID nList, uint32 nBatchFlags, const SRenderingPassInfo& passInfo,
								SRendItemSorter sorter) threadsafe
{
	assert(m_usageMode == eUsageModeWriting || m_bAddingClientPolys || nList == EFSLIST_PREPROCESS);  // Adding items only in writing mode

	nBatchFlags |= shaderItem.m_nPreprocessFlags & FSPR_MASK;

	if (passInfo.IsShadowPass() && (nList == EFSLIST_NEAREST_OBJECTS))
	{
		m_shadows.AddNearestCaster(pObj, passInfo);
	}

	// Track added items as separate scalars (permanent objects don't add to renderitems immediately)
	++m_addedItems[nList];

	// Calculate AABB
	AABB aabb;
	pElem->mfGetBBox(aabb);
	float objDistance = pObj->m_fDistance;

	ERenderObjectFlags objFlags = pObj->m_ObjFlags;    // last time we read flags from renderObject

	if (!(pElem->mfGetFlags() & FCEF_KEEP_DISTANCE) || passInfo.IsShadowPass())
	{
		// Use the (possibly) tighter AABB extracted from the render element and store distance.
		const auto position = passInfo.GetCamera().GetPosition();
		const auto transformed_aabb = pObj->TransformAABB(objFlags, aabb, position);

		objDistance = crymath::sqrt_fast(Distance::Point_AABBSq(position, transformed_aabb)) * GetZoomFactor();
	}

	//////////////////////////////////////////////////////////////////////////
	// Permanent objects support.
	//////////////////////////////////////////////////////////////////////////
	if (pObj->m_bPermanent)
	{
		CPermanentRenderObject* RESTRICT_POINTER pPermanentRendObj = reinterpret_cast<CPermanentRenderObject*>(pObj);
		auto renderPassType = passInfo.IsShadowPass() ? CPermanentRenderObject::eRenderPass_Shadows : CPermanentRenderObject::eRenderPass_General;
		pPermanentRendObj->AddRenderItem(pElem, shaderItem, aabb, objDistance, nBatchFlags, nList, renderPassType);
		return;
	}

	SRendItem ri = {0};

	// First by ShaderID, then by ShaderTechnique, then by ShaderResourceID
	ri.SortVal = SRendItem::PackShaderItem(shaderItem);
	ri.nBatchFlags = nBatchFlags;
	ri.rendItemSorter = sorter;
	ri.pCompiledObject = nullptr;
	ri.pElem = pElem;
	ri.fDist = objDistance;
	//ri.nStencRef = pObj->m_nClipVolumeStencilRef + 1; // + 1, we start at 1. 0 is reserved for MSAAed areas.

	if (m_bTrackUncompiledItems)
	{
		// This item will need a temporary compiled object
		EDataType reType = pElem ? pElem->mfGetType() : eDATA_Unknown;

		const bool meshCompatibleRenderElement =
			reType == eDATA_Mesh ||
			reType == eDATA_Terrain ||
			reType == eDATA_GeomCache ||
			reType == eDATA_ClientPoly;

		const bool compiledRenderElement =
			reType == eDATA_WaterVolume ||
			reType == eDATA_WaterOcean ||
			reType == eDATA_FogVolume;

		const bool customRenderLoop =
			reType == eDATA_LensOptics;

		if (meshCompatibleRenderElement || compiledRenderElement) // temporary disable for these types
		{
			// Allocate new CompiledRenderObject.
			ri.pCompiledObject = AllocCompiledObject(pObj, pElem, shaderItem);
			ri.nBatchFlags |= (ri.pCompiledObject ? FB_COMPILED_OBJECT : 0);

			// Add to temporary objects to compile
			m_temporaryCompiledObjects.push_back(STemporaryRenderObjectCompilationData{ ri.pCompiledObject, aabb, objFlags, pElem->m_Flags });
		}
		else if (customRenderLoop)
		{
			ri.pRenderObject = pObj;
		}
	}
	//////////////////////////////////////////////////////////////////////////

#if !defined(_RELEASE)
	if (CRenderer::CV_r_SkipAlphaTested && (pObj->m_ObjFlags & FOB_ALPHATEST))
		return;
#endif

	AddRenderItemToRenderLists<true>(ri, objFlags, nList, pObj, shaderItem);

	////////////////////////////////////////////////////////////////////////
	// Check if shader item needs update
	CheckAndScheduleForUpdate(shaderItem);
}

//////////////////////////////////////////////////////////////////////////
template<bool isConcurrent>
inline void UpdateRenderListBatchFlags(volatile uint32& listFlags, int newFlags) threadsafe
{
	if (isConcurrent)
	{
		CryInterlockedExchangeOr((volatile LONG*)&listFlags, newFlags);
	}
	else
	{
		listFlags |= newFlags;
	}
}

template<bool isConcurrent>
inline void CRenderView::AddRenderItemToRenderLists(const SRendItem& ri, uint64 objFlags, ERenderListID renderList, CRenderObject* RESTRICT_POINTER pObj, const SShaderItem& shaderItem) threadsafe
{
	uint32 nBatchFlags = ri.nBatchFlags;
	float objDistance = ri.fDist;
	bool distributeToOtherLists = !IsShadowGenView();

	if (renderList == EFSLIST_SKY)
		distributeToOtherLists = false;

	if (distributeToOtherLists)
	{
		const bool hasDeferredOpaqueFlags = (nBatchFlags & FB_Z) != 0;
		const bool hasForwardOpaqueFlags = (nBatchFlags & FB_TILED_FORWARD) != 0;
		const bool isTransparent = renderList == EFSLIST_TRANSP_BW || renderList == EFSLIST_TRANSP_AW || renderList == EFSLIST_TRANSP_NEAREST;
		const bool isForwardOpaque = renderList == EFSLIST_FORWARD_OPAQUE || renderList == EFSLIST_FORWARD_OPAQUE_NEAREST;
		const bool isSelectable = pObj->m_editorSelectionID > 0;

		const bool isNearest     = (objFlags & FOB_NEAREST  ) != 0;
		const bool isDissolved   = (objFlags & FOB_DISSOLVE ) != 0;
		const bool isAlphaTested = (objFlags & FOB_ALPHATEST) != 0;

		// ZPrepass only for non-alpha tested/blended geometry (decals, terrain).
		// We assume vegetation is special case due to potential massive overdraw
		const bool isEmissive   = shaderItem.IsEmissive();
		const bool isVegetation = shaderItem.IsVegetation();
		const bool isTesselated = shaderItem.IsTesselated();
		const bool skipPrepass  =
			(CRenderer::CV_r_UseZPass <= CSceneGBufferStage::eZPassMode_GBufferOnly) ||
			(CRenderer::CV_r_UseZPass <= CSceneGBufferStage::eZPassMode_PartialZPrePass && (isDissolved && !isVegetation)) ||
			(CRenderer::CV_r_UseZPass <= CSceneGBufferStage::eZPassMode_PartialZPrePass && (isAlphaTested && !isVegetation)) ||
			(CRenderer::CV_r_UseZPass <= CSceneGBufferStage::eZPassMode_DiscardingZPrePass && (isTesselated)) ||
			(CRenderer::CV_r_UseZPass <= CSceneGBufferStage::eZPassMode_DiscardingZPrePass && (objDistance > CRenderer::CV_r_ZPrepassMaxDist));

		if (isTransparent || (skipPrepass && hasDeferredOpaqueFlags))
			nBatchFlags &= ~FB_ZPREPASS;

		// These lists are traversed by the GBuffer-pass by default (see CSceneGBufferStage::ExecuteSceneOpaque)
		const bool isGeneralList =
			renderList == EFSLIST_NEAREST_OBJECTS ||
			renderList == EFSLIST_GENERAL ||
			renderList == EFSLIST_TERRAINLAYER ||
			renderList == EFSLIST_DECAL;

		if ((nBatchFlags & FB_ZPREPASS))
		{
			const ERenderListID targetRenderList = isNearest ? EFSLIST_ZPREPASS_NEAREST : EFSLIST_ZPREPASS;
			m_renderItems[targetRenderList].push_back(PrepareRenderItemForRenderList(ri, nBatchFlags, objFlags, pObj, objDistance, targetRenderList, shaderItem));
			UpdateRenderListBatchFlags<isConcurrent>(m_batchFlags[targetRenderList], nBatchFlags);
		}

		// Add unconventional objects that are supported in the GBuffer-pass to the GENERAL list (in addition to the original list)
		if ((hasDeferredOpaqueFlags) && !(isGeneralList))
		{
			m_renderItems[EFSLIST_GENERAL].push_back(PrepareRenderItemForRenderList(ri, nBatchFlags, objFlags, pObj, objDistance, EFSLIST_GENERAL, shaderItem));
			UpdateRenderListBatchFlags<isConcurrent>(m_batchFlags[EFSLIST_GENERAL], nBatchFlags);
		}

		// Add forward objects that are supported in the Forward-pass to the FORWARD_OPAQUE list (in addition to the original list)
		if (!isTransparent && (isEmissive || (hasForwardOpaqueFlags && !(isForwardOpaque | hasDeferredOpaqueFlags))))
		{
			const ERenderListID targetRenderList = isNearest ? EFSLIST_FORWARD_OPAQUE_NEAREST : EFSLIST_FORWARD_OPAQUE;
			m_renderItems[targetRenderList].push_back(PrepareRenderItemForRenderList(ri, nBatchFlags, objFlags, pObj, objDistance, targetRenderList, shaderItem));
			UpdateRenderListBatchFlags<isConcurrent>(m_batchFlags[targetRenderList], nBatchFlags);
		}

		if (nBatchFlags & FB_PREPROCESS)
		{
			// Prevent water usage on non-water specific meshes (it causes reflections updates). Todo: this should be checked in editor side and not allow such usage
			EShaderType shaderType = reinterpret_cast<CShader*>(shaderItem.m_pShader)->m_eShaderType;
			if (shaderType != eST_Water || (shaderType == eST_Water && renderList == EFSLIST_WATER))
			{
				m_renderItems[EFSLIST_PREPROCESS].push_back(PrepareRenderItemForRenderList(ri, nBatchFlags, objFlags, pObj, objDistance, EFSLIST_PREPROCESS, shaderItem));
				UpdateRenderListBatchFlags<isConcurrent>(m_batchFlags[EFSLIST_PREPROCESS], nBatchFlags);
			}
		}

		if (nBatchFlags & FB_CUSTOM_RENDER)
		{
			m_renderItems[EFSLIST_CUSTOM].push_back(PrepareRenderItemForRenderList(ri, nBatchFlags, objFlags, pObj, objDistance, EFSLIST_CUSTOM, shaderItem));
			UpdateRenderListBatchFlags<isConcurrent>(m_batchFlags[EFSLIST_CUSTOM], nBatchFlags);
		}

		if (isSelectable)
		{
			m_renderItems[EFSLIST_HIGHLIGHT].push_back(PrepareRenderItemForRenderList(ri, nBatchFlags, objFlags, pObj, objDistance, EFSLIST_HIGHLIGHT, shaderItem));
			UpdateRenderListBatchFlags<isConcurrent>(m_batchFlags[EFSLIST_HIGHLIGHT], nBatchFlags);
		}
	}

	m_renderItems[renderList].push_back(PrepareRenderItemForRenderList(ri, nBatchFlags, objFlags, pObj, objDistance, renderList, shaderItem));
	UpdateRenderListBatchFlags<isConcurrent>(m_batchFlags[renderList], nBatchFlags);
}

void CRenderView::SPermanentObjectRecord::prepareForCompilation()
{
	IPermanentRenderObject* pPermanentRenderObject = static_cast<IPermanentRenderObject*>(pRenderObject);
	for (uint32 i = 0; i < CRenderObject::eRenderPass_NumTypes; ++i)
		passPreparedMask[i] = pPermanentRenderObject->IsPreparedForPass(CRenderObject::ERenderPassType(i));

	instanceInfo = pPermanentRenderObject->GetInstanceInfo();
	for (IPermanentRenderObject* pSubObj = pPermanentRenderObject->m_pNextPermanent; pSubObj;)
	{
		subObjectInstanceInfo.push_back(pSubObj->GetInstanceInfo());

		pSubObj = pSubObj->m_pNextPermanent;
		CRY_ASSERT(!pSubObj || pSubObj != pSubObj->m_pNextPermanent);
	}
}

void CRenderView::PreparePermanentRenderObjectsForCompile()
{
	CRY_PROFILE_FUNCTION(PROFILE_3DENGINE);

	for (auto& permanentRenderObj : m_permanentObjects)
		permanentRenderObj.prepareForCompilation();
}

void CRenderView::ExpandPermanentRenderObjects()
{
	PROFILE_FRAME(ExpandPermanentRenderObjects);


	if (!m_permanentObjects.empty())
	{
		assert(m_usageMode != eUsageModeWriting);
	}

	auto   renderPassType = GetPassType();

	// Expand normal render items
	for (const auto& record : m_permanentObjects) 
	{
		CPermanentRenderObject* pRenderObject = record.pRenderObject;
		assert(pRenderObject->m_bPermanent);

		uint32 subobjectIdx = 0;
		// Submit all valid objects (skip not ready and helper objects), TODO: release helper objects
		while (pRenderObject)
		{
			bool needsCompilation = false;
			EObjectCompilationOptions reCompilationOptions = eObjCompilationOption_None;

			// The scope that Renderobject is locked for reading.
			const bool isInvisible = !pRenderObject->m_fAlpha;
			if (!isInvisible)
			{
				ReadLock lock(pRenderObject->m_accessLock); // Block on read access to the render object

#if !defined(_RELEASE)
				if (CRenderer::CV_r_SkipAlphaTested && (pRenderObject->m_ObjFlags & FOB_ALPHATEST))
				{
					pRenderObject = static_cast<CPermanentRenderObject*>(pRenderObject->m_pNextPermanent);
					continue;
				}
#endif
				// IsCompiledForPass() can race with CompileModifiedRenderObjects() in render thread but in the worst case, the object will be passed to CompileModifiedRenderObjects via 
				// m_permanentRenderObjectsToCompile in two consecutive frames and the second compilation will be skipped there.
				needsCompilation = record.IsPreparedForPass(renderPassType) && !pRenderObject->IsCompiledForPass(renderPassType);

				auto& permanent_items = pRenderObject->m_permanentRenderItems[renderPassType];
				size_t numItems = permanent_items.size();
				assert(numItems < 128); // Sanity check, otherwise too many chunks in the mesh
				for (size_t i = 0; i < numItems; i++)
				{
					auto& RESTRICT_REFERENCE pri = permanent_items[i];

					SShaderItem shaderItem;
					SRendItem::ExtractShaderItem(pri.m_sortValue, pri.m_nBatchFlags, shaderItem);

					CShaderResources* const __restrict pShaderResources = (CShaderResources*)shaderItem.m_pShaderResources;

					// Rebuild permanent object next frame in case we still rendered with the shader item which has been replaced (and is old)
					assert(shaderItem.m_pShader && pShaderResources);
					if (!pShaderResources->IsValid())
					{
						if (pRenderObject->m_pRenderNode)
							pRenderObject->m_pRenderNode->InvalidatePermanentRenderObject();

						continue;
					}

					if (!(volatile CCompiledRenderObject*)pri.m_pCompiledObject)
					{
						if (pri.m_pRenderElement && (pri.m_pRenderElement->mfGetType() == eDATA_Mesh || pri.m_pRenderElement->mfGetType() == eDATA_Particle))
						{
							// This item will need a temporary compiled object
							static CryCriticalSectionNonRecursive allocCS;
							AUTO_LOCK_T(CryCriticalSectionNonRecursive, allocCS);

							if (!(volatile CCompiledRenderObject*)pri.m_pCompiledObject)
							{
								pri.m_pCompiledObject = AllocCompiledObject(pRenderObject, pri.m_pRenderElement, shaderItem); // Allocate new CompiledRenderObject.
								needsCompilation = true;
							}
						}
					}

					CheckAndScheduleForUpdate(shaderItem);

					// The need for compilation is not detected beforehand, and it only needs to be recompiled because the elements are either skinned, dirty, or need to be updated always.
					// In this case object need to be recompiled to get skinning, or input stream updated but compilation of constant buffer is not needed.
					if (!needsCompilation && pri.m_ElmFlags & (FCEF_DIRTY | FCEF_SKINNED | FCEF_UPDATEALWAYS))
					{
						if (pri.m_ElmFlags & FCEF_SKINNED)
							reCompilationOptions |= eObjCompilationOption_PerDrawExtraResources;

						needsCompilation = true;
					}

					// Discard 0 alpha blended geometry - this should be discarded earlier on 3dengine side preferably
					const bool isInvisible = pShaderResources && pShaderResources->IsInvisible();
					if (!isInvisible)
					{
						ERenderListID renderList;
						if (renderPassType == CPermanentRenderObject::eRenderPass_Shadows)
							renderList = ERenderListID(record.shadowFrustumSide);
						else
							renderList = pri.m_nRenderList;

						SRendItem ri;

						ri.SortVal = pri.m_sortValue;
						ri.nBatchFlags = pri.m_nBatchFlags | (pri.m_pCompiledObject ? FB_COMPILED_OBJECT : 0);
						ri.rendItemSorter = SRendItemSorter(record.itemSorter);
						ri.pCompiledObject = pri.m_pCompiledObject;
						ri.pElem = pri.m_pRenderElement;
						ri.fDist = pri.m_fDist;
						//ri.nStencRef = pRenderObject->m_nClipVolumeStencilRef + 1; // + 1, we start at 1. 0 is reserved for MSAAed areas.

						AddRenderItemToRenderLists<false>(ri, pri.m_ObjFlags, renderList, pRenderObject, shaderItem);
					}
				}
			}

			// Only the instance data needs to be updated (aka. fast-path), the input stream and pipeline creation is not needed.
			const auto needsInstanceDataUpdateOnly = !needsCompilation && record.requiresInstanceDataUpdate;
			if (needsInstanceDataUpdateOnly)
			{
				reCompilationOptions |=
					eObjCompilationOption_PerDrawConstantBuffer;

				needsCompilation = true;
			}

			if (needsCompilation)
			{
				CRY_ASSERT(subobjectIdx == 0 || record.subObjectInstanceInfo.size() >= subobjectIdx);
				m_permanentRenderObjectsToCompile.push_back(SPermanentRenderObjectCompilationData { &record, subobjectIdx, pRenderObject, reCompilationOptions });
			}

			pRenderObject = static_cast<CPermanentRenderObject*>(pRenderObject->m_pNextPermanent);
			subobjectIdx++;
		}
	}
}

//////////////////////////////////////////////////////////////////////////
void CRenderView::CompileModifiedRenderObjects()
{
	CRY_PROFILE_FUNCTION_ARG(PROFILE_RENDERER, m_name.c_str())
	MEMSTAT_CONTEXT(EMemStatContextType::Other, "CRenderView::CompileModifiedRenderObjects");

	assert(gRenDev->m_pRT->IsRenderThread());

	// Prepare polygon data buffers.
	m_polygonDataPool->UpdateAPIBuffers();

	//////////////////////////////////////////////////////////////////////////
	// Compile all modified objects.

	CRenderObject::ERenderPassType passType = GetPassType();

#if defined(ENABLE_PROFILING_CODE)
	int numObjects = 0;
	int numTempObjects = 0;
#endif

	auto* pShadowStage = IsShadowGenView() ? m_pGraphicsPipeline->GetStage<CShadowMapStage>() : nullptr;

	for (const auto& compilationData : m_permanentRenderObjectsToCompile)
	{
#if defined(ENABLE_PROFILING_CODE)
		++numObjects;
#endif
		const auto* pPermanentObjectRecord  = compilationData.pPermanentRenderObjectRecord;
		const uint32 subobjectIdx           = compilationData.subObjectIdx;
		const auto&  instanceInfo           = pPermanentObjectRecord->getSubObjectInstanceInfo(subobjectIdx);
		auto* pRenderObject                 = compilationData.pObject;
		auto  recompilationFlags            = compilationData.recompilationFlags;

		if (pRenderObject->GetFailedCompilationCount(passType) > IPermanentRenderObject::MaxFailedCompilationCount)
			CryWarning(VALIDATOR_MODULE_RENDERER, VALIDATOR_WARNING, "Object is failing too many times!!!");

		if (pRenderObject->IsCompiledForPass(passType) && recompilationFlags == eObjCompilationOption_None)
			continue;

		WriteLock lock(pRenderObject->m_accessLock); // Block on write access to the render object (e.g. pri.m_pCompiledObject, pRenderObject->SetCompiledForPass())

		// Do compilation on the chain of the compiled objects
		bool bAllCompiled = true;
		bool bAllShadowRenderable = true;

		// compile items
		const auto& items = pRenderObject->m_permanentRenderItems[passType];
		for (int i = 0, num = items.size(); i < num; i++)
		{
			auto& pri = items[i];
			if (!pri.m_pCompiledObject || !pri.m_pCompiledObject->Compile(recompilationFlags, instanceInfo, pri.m_ObjFlags, pri.m_ElmFlags, pri.m_aabb, this))
				bAllCompiled = false;

			if (pShadowStage)
				bAllShadowRenderable &= pShadowStage->CanRenderCachedShadows(pri.m_pCompiledObject);
		}

		pRenderObject->SetCompiledForPass(passType, bAllCompiled);

		if (!bAllCompiled)
		{
			pRenderObject->IncrementFailedCompilationCount(passType);

			if (!bAllShadowRenderable)
			{
				// NOTE: this can race with the the main thread but the worst outcome will be that the object is rendered multiple times into the shadow cache
				ShadowMapFrustum::ForceMarkNodeAsUncached(pRenderObject->m_pRenderNode);
			}
		}
	}
	m_permanentRenderObjectsToCompile.clear();

	// Compile all temporary compiled objects
	for (const auto& t : m_temporaryCompiledObjects)
	{
#if defined(ENABLE_PROFILING_CODE)
		++numTempObjects;
#endif

		t.pObject->Compile(eObjCompilationOption_All, t.pObject->m_pRO->GetInstanceInfo(), t.objFlags, t.elmFlags, t.localAABB, this);

		bool bAllShadowRenderable = true;
		if (pShadowStage)
			bAllShadowRenderable &= pShadowStage->CanRenderCachedShadows(t.pObject);

		if (!bAllShadowRenderable)
		{
			// NOTE: this can race with the the main thread but the worst outcome will be that the object is rendered multiple times into the shadow cache
			ShadowMapFrustum::ForceMarkNodeAsUncached(t.pObject->m_pRO->m_pRenderNode);
		}
	}

	//////////////////////////////////////////////////////////////////////////

#if defined(ENABLE_PROFILING_CODE)
	CryInterlockedAdd(&SRenderStatistics::Write().m_nModifiedCompiledObjects, numObjects);
	CryInterlockedAdd(&SRenderStatistics::Write().m_nTempCompiledObjects, numTempObjects);
#endif

	for (auto& fr : m_shadows.m_renderFrustums)
	{
		CRenderView* pShadowView = (CRenderView*)fr.pShadowsView.get();
		pShadowView->SetGraphicsPipeline(m_pGraphicsPipeline);
		pShadowView->CompileModifiedRenderObjects();
	}
}

//////////////////////////////////////////////////////////////////////////
void CRenderView::UpdateModifiedShaderItems()
{
	for (int i = 0; i < m_numUsedClientPolygons; ++i)
	{
		CheckAndScheduleForUpdate(m_polygonsPool[i]->m_Shader);
	}

	for (auto& decal : m_deferredDecals)
	{
		if (decal.pMaterial)
			CheckAndScheduleForUpdate(decal.pMaterial->GetShaderItem(0));
	}

	for (const auto& item : m_shaderItemsToUpdate)
	{
		auto pShaderResources = item.first;
		auto pShader = item.second;

		if (pShaderResources->HasDynamicTexModifiers())
		{
			pShaderResources->RT_UpdateConstants(pShader);
		}

		if (pShaderResources->HasAnimatedTextures())
		{
			// update dynamic texture sources based on a shared RT only
			if (GetCurrentEye() == CCamera::eEye_Left)
			{
				// TODO: optimize search (flagsfield?)
				for (EEfResTextures texType = EFTT_DIFFUSE; texType < EFTT_MAX; texType = EEfResTextures(texType + 1))
				{
					if (SEfResTexture* pTex = pShaderResources->m_Textures[texType])
					{
						_smart_ptr<CTexture> previousTex = pTex->m_Sampler.m_pTex; // keep reference to previous texture here as it might get released inside Update call

						if (pTex->m_Sampler.Update())
						{
							if (previousTex)
							{
								previousTex->RemoveInvalidateCallbacks((void*)pShaderResources);
							}
						}
					}
				}
			}
		}

		if (pShaderResources->HasDynamicUpdates())
		{
			uint32 batchFilter = FB_GENERAL;
			bool bUpdated = false;

			// update dynamic texture sources based on a shared RT only
			if (GetCurrentEye() == CCamera::eEye_Left)
			{
				// TODO: optimize search (flagsfield?)
				for (EEfResTextures texType = EFTT_DIFFUSE; texType < EFTT_MAX; texType = EEfResTextures(texType + 1))
				{
					const SEfResTexture* pTex = pShaderResources->m_Textures[texType];
					if (pTex)
					{
						IDynTextureSourceImpl* pDynTexSrc = (IDynTextureSourceImpl*)pTex->m_Sampler.m_pDynTexSource;
						if (pDynTexSrc)
						{
							if (pDynTexSrc->GetSourceType() == IDynTextureSource::DTS_I_FLASHPLAYER)
							{
								if (batchFilter & (FB_GENERAL | FB_TRANSPARENT))
								{
									bUpdated = bUpdated | pDynTexSrc->Update();
								}
							}
						}
					}
				}
			}
		}
	}

	m_shaderItemsToUpdate.clear();
}

//////////////////////////////////////////////////////////////////////////
void CRenderView::ClearTemporaryCompiledObjects()
{
	PROFILE_FRAME(ClearTemporaryCompiledObjects);

	/////////////////////////////////////////////////////////////////////////////
	// Clean up non permanent compiled objects
	for (const auto& t : m_temporaryCompiledObjects)
		CCompiledRenderObject::FreeToPool(t.pObject);
	m_temporaryCompiledObjects.clear();
	/////////////////////////////////////////////////////////////////////////////
}

CRenderView::ItemsRange CRenderView::GetItemsRange(ERenderListID renderList)
{
	const auto& items = GetRenderItems(renderList);
	return ItemsRange(0, items.size());
}

CRenderView::ItemsRange CRenderView::GetShadowItemsRange(ShadowMapFrustum* pFrustum, int nFrustumSide)
{
	if (nFrustumSide < EFSLIST_NUM)
	{
		const auto& items = GetShadowItems(pFrustum, nFrustumSide);
		return ItemsRange(0, items.size());
	}
	return ItemsRange(0, 0);
}

CRenderView::RenderItems& CRenderView::GetShadowItems(ShadowMapFrustum* pFrustum, int nFrustumSide)
{
	assert(m_usageMode != eUsageModeWriting); // While writing we must not read back items.
	assert(nFrustumSide >= 0 && nFrustumSide < EFSLIST_NUM);

	CRenderView* pShadowsView = GetShadowsView(pFrustum);
	assert(pShadowsView);
	if (!pShadowsView)
	{
		static CRenderView::RenderItems empty;
		return empty;
	}
	return pShadowsView->GetRenderItems(ERenderListID(nFrustumSide));
}

//////////////////////////////////////////////////////////////////////////
void CRenderView::Job_PostWrite()
{
	CRY_PROFILE_FUNCTION_ARG(PROFILE_RENDERER, m_name.c_str());
	CRY_ASSERT(!m_bPostWriteExecuted);

	DECLARE_JOB("SortRenderItems", TSortRenderItemsJob, CRenderView::Job_SortRenderItemsInList);

	// Prevent double entering this
	CryAutoLock<CryCriticalSectionNonRecursive> lock(m_lock_PostWrite);

	// At this point it is really writing done, if was not set yet.
	{
		CryAutoLock<CryCriticalSectionNonRecursive> lock(m_lock_UsageMode);
		if (m_usageMode == eUsageModeWriting)
			m_usageMode = eUsageModeWritingDone;
	}

	if (m_bPostWriteExecuted)
		return;

	ExpandPermanentRenderObjects();

	SortLights();

	const auto listCount = IsShadowGenView() ? OMNI_SIDES_NUM : EFSLIST_NUM;
	for (int renderList = 0; renderList < listCount; renderList++)
	{
		if (renderList == EFSLIST_PREPROCESS && IsShadowGenView())
		{
			// These lists need not to be sorted here
			continue;
		}

		m_renderItems[renderList].CoalesceMemory();
		auto& renderItems = GetRenderItems(ERenderListID(renderList));

		if (!renderItems.empty())
		{
			TSortRenderItemsJob job((ERenderListID)renderList);
			job.SetClassInstance(this);
			job.Run(JobManager::eRegularPriority, &m_jobstate_Sort);
		}
	}

	m_bPostWriteExecuted = true;
}

////////////////////////////////////////
struct SCompareRendItemSelectionPass
{
	bool operator()(const SRendItem& a, const SRendItem& b) const
	{
		// Selection and highlight are the first two bits of the selectionID
		uint8 bAHighlightSelect = (a.pCompiledObject->m_pRO->m_editorSelectionID & 0x3);
		uint8 bBHighlightSelect = (b.pCompiledObject->m_pRO->m_editorSelectionID & 0x3);

		// Highlight is the higher bit, so highlighted objects win in the comparison.
		// Also, selected objects win over non-selected objects, which is exactly what we want
		return bAHighlightSelect < bBHighlightSelect;
	}
};

///////////////////////////////////////////////////////////////////////////////
#if defined(DEBUG_PRINT_RENDERLISTS) // For printing out renderLists:
extern const char* RenderListNames[];

void DebugPrintRenderlists(CRenderView* pRenderView)
{
	const auto listCount = pRenderView->IsShadowGenView() ? OMNI_SIDES_NUM : EFSLIST_NUM;
	for (int renderList = 0; renderList < listCount; renderList++)
	{
		auto& renderItems = pRenderView->GetRenderItems(ERenderListID(renderList));

		CryLogAlways("renderList %s %d", RenderListNames[pRenderView->IsShadowGenView() ? EFSLIST_SHADOW_GEN : renderList], renderList);
		for (size_t i = 0; i < renderItems.size(); i++)
		{
			if (renderItems[i].ObjSort & (FOB_SORT_MASK & FOB_ALPHATEST))
			{
				int a = 0;
			}

			if (renderItems[i].ObjSort & (FOB_SORT_MASK & FOB_HAS_PREVMATRIX))
			{
				int a = 0;
			}

			// SCompareRendItemZPrePass
			if (renderList == EFSLIST_ZPREPASS_NEAREST ||
			    renderList == EFSLIST_ZPREPASS)
				CryLogAlways("%6d: %c %2d 0x%04x 0x%016zx 0x%04x", i,
				             ((renderItems[i].ObjSort & (FOB_SORT_MASK & FOB_ALPHATEST))) ? 'a' : 'o',
				             (renderItems[i].ObjSort >> 10) & 0x3F,
				             renderItems[i].SortVal,
				             renderItems[i].pElem,
				             renderItems[i].ObjSort & 0xFFFF
				             );

			// SCompareRendItemZPass
			if (renderList == EFSLIST_NEAREST_OBJECTS ||
			    renderList == EFSLIST_GENERAL ||
			    renderList == EFSLIST_SKIN)
				CryLogAlways("%6d: %c %2d 0x%04x 0x%016zx 0x%04x", i,
				             ((renderItems[i].ObjSort & (FOB_SORT_MASK & ~FOB_DISCARD_MASK)) >> 30) + 'A',
				             !(renderItems[i].ObjSort & FOB_ZPREPASS) ? (renderItems[i].ObjSort >> 10) & 0x3F : 0x3F,
				             renderItems[i].SortVal,
				             renderItems[i].pElem,
				             renderItems[i].ObjSort & 0xFFFF
				             );

			// SCompareDist
			if (renderList == EFSLIST_WATER_VOLUMES ||
			    renderList == EFSLIST_TRANSP_BW ||
			    renderList == EFSLIST_TRANSP_AW ||
			    renderList == EFSLIST_TRANSP_NEAREST ||
			    renderList == EFSLIST_WATER ||
			    renderList == EFSLIST_HALFRES_PARTICLES ||
			    renderList == EFSLIST_LENSOPTICS ||
			    renderList == EFSLIST_EYE_OVERLAY) int a = 0;

			// SCompareItem_Decal
			if (renderList == EFSLIST_DECAL) int a = 0;

			// SCompareItem_NoPtrCompare
			if (renderList == EFSLIST_TERRAINLAYER) int a = 0;

			// SCompareRendItemZPrePass
			if (renderList == EFSLIST_FORWARD_OPAQUE_NEAREST ||
			    renderList == EFSLIST_FORWARD_OPAQUE ||
			    IsShadowGenView())
				CryLogAlways("%6d: %c %2d 0x%08x 0x%016zx 0x%04x", i,
				             ((renderItems[i].ObjSort & (FOB_SORT_MASK & FOB_ALPHATEST))) ? 'a' : 'o',
				             (renderItems[i].ObjSort >> 10) & 0x3F,
				             renderItems[i].SortVal,
				             renderItems[i].pElem,
				             renderItems[i].ObjSort & 0xFFFF
				             );

			renderItems[i].nBatchFlags;
		}
	}
}
#endif

SRendItem CRenderView::PrepareRenderItemForRenderList(const SRendItem& ri_templ, uint32 nBatchFlags, uint64 objFlags, CRenderObject* pObj, float objDistance, ERenderListID renderList, const SShaderItem& shaderItem)
{
	SRendItem ri = ri_templ;

	// Batch-flags can be specified on a per-list basis
	ri.nBatchFlags = nBatchFlags;

	if (IsShadowGenView())
	{
		ri.ObjSort = SRendItem::EncodeObjFlagsSortingValue(pObj, objFlags, objDistance);
		return ri;
	}

	// ATTENTION: Keep in sync with symmetric switch in CRenderView::Job_SortRenderItemsInList(ERenderListID list), see below
	switch (renderList)
	{
	case EFSLIST_SHADOW_GEN:
	case EFSLIST_FOG_VOLUME:
		// No need to sort.
		break;
	case EFSLIST_ZPREPASS_NEAREST:
	case EFSLIST_ZPREPASS:

	case EFSLIST_NEAREST_OBJECTS:
	case EFSLIST_GENERAL:
	case EFSLIST_DEBUG_HELPER:

	case EFSLIST_FORWARD_OPAQUE_NEAREST:
	case EFSLIST_FORWARD_OPAQUE:
		if (CRenderer::CV_r_ZPassDepthSorting == 2)
			goto fallthrough;

		ri.ObjSort = SRendItem::EncodeObjFlagsSortingValue(pObj, objFlags, objDistance);

		// Take the object flag away when not put into the zprepass-list (used for splitting)
		if (!(nBatchFlags & FB_ZPREPASS))
			ri.ObjSort &= ~FOB_ZPREPASS;
		else
			ri.ObjSort |= FOB_ZPREPASS;

		break;
	case EFSLIST_CUSTOM:
		ri.fDist = SRendItem::EncodeCustomDistanceSortingValue(pObj, objDistance); // custom sorting for silhouettes, etc...
		break;
	case EFSLIST_WATER_VOLUMES:
	case EFSLIST_TRANSP_AW:
	case EFSLIST_TRANSP_BW:
	case EFSLIST_TRANSP_NEAREST:
	case EFSLIST_WATER:
	case EFSLIST_HALFRES_PARTICLES:
	case EFSLIST_LENSOPTICS:
	case EFSLIST_EYE_OVERLAY:
	case EFSLIST_SKY:
	fallthrough:
		ri.fDist   = SRendItem::EncodeDistanceSortingValue(pObj, objDistance);
		break;
	default:
		ri.ObjSort = SRendItem::EncodeObjFlagsSortingValue(pObj, objFlags);
		break;
	}

	return ri;
}

void CRenderView::Job_SortRenderItemsInList(ERenderListID renderList)
{
	FUNCTION_PROFILER_RENDERER();

	auto& renderItems = GetRenderItems(renderList);
	if (renderItems.empty())
		return;

	int nStart = 0;
	int n = renderItems.size();

	if (IsShadowGenView())
	{
		// Sort Shadow render items differently
		if (m_shadows.m_pShadowFrustumOwner)
		{
			CRY_ASSERT(m_shadows.m_pShadowFrustumOwner->bRestrictToRT);

			const auto side = renderList;
			CRY_ASSERT(side >= 0 && side < OMNI_SIDES_NUM);
			if (n > 0)
				m_shadows.m_pShadowFrustumOwner->SortRenderItemsForFrustumAsync(side, &renderItems[0], n);
		}

		return;
	}

	switch (renderList)
	{
	case EFSLIST_PREPROCESS:
		{
			PROFILE_FRAME(State_SortingPre);
			SRendItem::mfSortPreprocess(&renderItems[nStart], n);
		}
		break;

	case EFSLIST_SHADOW_GEN:
		// No need to sort.
		break;

	case EFSLIST_FOG_VOLUME:
		break;

	case EFSLIST_WATER_VOLUMES:
	case EFSLIST_TRANSP_BW:
	case EFSLIST_TRANSP_AW:
	case EFSLIST_TRANSP_NEAREST:
	case EFSLIST_WATER:
	case EFSLIST_HALFRES_PARTICLES:
	case EFSLIST_LENSOPTICS:
	case EFSLIST_EYE_OVERLAY:
	case EFSLIST_SKY:
	case EFSLIST_CUSTOM:
		{
			PROFILE_FRAME(State_SortingDist);
			SRendItem::mfSortByDist(&renderItems[nStart], n, false);
		}
		break;
	case EFSLIST_DECAL:
		{
			PROFILE_FRAME(State_SortingDecals);
			SRendItem::mfSortByDist(&renderItems[nStart], n, true);
		}
		break;

	case EFSLIST_ZPREPASS_NEAREST:
	case EFSLIST_ZPREPASS:
		{
			PROFILE_FRAME(State_SortingZPass);
			if (CRenderer::CV_r_ZPassDepthSorting == 1)
				SRendItem::mfSortForZPass(&renderItems[nStart], n, true);
			else if (CRenderer::CV_r_ZPassDepthSorting == 2)
				SRendItem::mfSortByDist(&renderItems[nStart], n, false, true);
		}
		break;

	case EFSLIST_NEAREST_OBJECTS:
	case EFSLIST_GENERAL:
	case EFSLIST_DEBUG_HELPER:
		{
			PROFILE_FRAME(State_SortingGBuffer);
 			if (CRenderer::CV_r_ZPassDepthSorting == 0)
				SRendItem::mfSortForDepthPass(&renderItems[nStart], n);
			else if (CRenderer::CV_r_ZPassDepthSorting == 1)
				SRendItem::mfSortForZPass(&renderItems[nStart], n, false);
			else if (CRenderer::CV_r_ZPassDepthSorting == 2)
				SRendItem::mfSortByDist(&renderItems[nStart], n, false, true);
		}
		break;

	case EFSLIST_FORWARD_OPAQUE_NEAREST:
	case EFSLIST_FORWARD_OPAQUE:
		{
			{
				PROFILE_FRAME(State_SortingForwardOpaque);
				SRendItem::mfSortForZPass(&renderItems[nStart], n, false);
			}
		}
		break;

	case EFSLIST_AFTER_POSTPROCESS:
	case EFSLIST_AFTER_HDRPOSTPROCESS:
		{
			PROFILE_FRAME(State_SortingLight);
			SRendItem::mfSortByLight(&renderItems[nStart], n, true, false, renderList == EFSLIST_DECAL);
		}
		break;
	case EFSLIST_TERRAINLAYER:
		{
			PROFILE_FRAME(State_SortingLight_TerrainLayers);
			SRendItem::mfSortByLight(&renderItems[nStart], n, true, true, false);
		}
		break;

	case EFSLIST_HIGHLIGHT:
		// only sort the selection list if we are in editor and not in game mode
		if (gcpRendD3D->IsEditorMode() && !gEnv->IsEditorGameMode())
		{
			PROFILE_FRAME(State_SortingSelection);
			std::sort(&renderItems[nStart], &renderItems[nStart + n], SCompareRendItemSelectionPass());
		}
		break;

	default:
		assert(0);
	}
}

void CRenderView::SortLights()
{
	struct CubemapCompare
	{
		bool operator()(const SRenderLight& l0, const SRenderLight& l1) const
		{
			if (l0.m_nSortPriority != l1.m_nSortPriority)
				return l0.m_nSortPriority > l1.m_nSortPriority;

			if (l0.m_fRadius == l1.m_fRadius)  // Sort by entity id to guarantee deterministic order across frames
				return l0.m_nEntityId > l1.m_nEntityId;

			return l0.m_fRadius < l1.m_fRadius;
		}
	};

	// does not invalidate references or iterators, but invalidates all m_Id in the lights
	GetLightsArray(eDLT_DeferredCubemap).sort(CubemapCompare());
}

//////////////////////////////////////////////////////////////////////////
void CRenderView::AddShadowFrustumToRender(SShadowFrustumToRender&& frustum)
{
	CRY_ASSERT(frustum.pShadowsView->GetFrameId() == GetFrameId());
	m_shadows.m_renderFrustums.emplace_back(std::move(frustum));
}

//////////////////////////////////////////////////////////////////////////
CRenderView* CRenderView::GetShadowsView(ShadowMapFrustum* pFrustum)
{
	for (auto& fr : m_shadows.m_renderFrustums)
	{
		if (fr.pFrustum == pFrustum)
		{
			return (CRenderView*)fr.pShadowsView.get();
		}
	}
	assert(0); // Should not be used like this
	return 0;
}

//////////////////////////////////////////////////////////////////////////
CRenderView::ShadowFrustumsPtr& CRenderView::GetShadowFrustumsForLight(int lightId)
{
	assert(m_usageMode != eUsageModeWriting); // While writing we must not read back frustums per light.

	auto iter = m_shadows.m_frustumsByLight.find(lightId);
	if (iter != m_shadows.m_frustumsByLight.end())
		return iter->second;
	static ShadowFrustumsPtr empty;
	return empty;
}

//////////////////////////////////////////////////////////////////////////
void CRenderView::PostWriteShadowViews()
{
	for (auto& fr : m_shadows.m_renderFrustums)
	{
		fr.pShadowsView->SwitchUsageMode(IRenderView::eUsageModeWritingDone);
	}
}

//////////////////////////////////////////////////////////////////////////
void CRenderView::PrepareShadowViews()
{
	FUNCTION_PROFILER_RENDERER();

	for (auto& fr : m_shadows.m_renderFrustums)
	{
		if (fr.pFrustum->m_eFrustumType != ShadowMapFrustum::e_Nearest)
		{
			fr.pShadowsView->SwitchUsageMode(CRenderView::eUsageModeReading);
		}
	}

	m_shadows.PrepareNearestShadows();
	m_shadows.CreateFrustumGroups();
}

//////////////////////////////////////////////////////////////////////////
void CRenderView::DrawCompiledRenderItems(const SGraphicsPipelinePassContext& passContext) const
{
	GetDrawer().DrawCompiledRenderItems(passContext);
}

//////////////////////////////////////////////////////////////////////////
void CRenderView::CheckAndScheduleForUpdate(const SShaderItem& shaderItem) threadsafe
{
	auto* pSR = reinterpret_cast<CShaderResources*>(shaderItem.m_pShaderResources);
	auto* pShader = reinterpret_cast<CShader*>(shaderItem.m_pShader);

	if (pSR && pShader)
	{
		if (pSR->HasDynamicTexModifiers() || pSR->HasAnimatedTextures())
		{
			if (CryInterlockedExchange((volatile LONG*)&pSR->m_nUpdateFrameID, (uint32)m_frameId) != (uint32)m_frameId)
			{
				m_shaderItemsToUpdate.push_back(std::make_pair(pSR, pShader));
			}
		}
	}
}

// Fog Volume indices use the same convention as Light Volume indices: 0 is invalid, 1 is first valid.
//////////////////////////////////////////////////////////////////////////
uint16 CRenderView::PushFogVolumeContribution(const ColorF& fogVolumeContrib, const SRenderingPassInfo& passInfo) threadsafe
{
	int numFogVolumes = CryInterlockedIncrement(&m_numFogVolumes);
	const size_t maxElems((1 << (sizeof(uint16) * 8)) - 1);

	CRY_ASSERT(numFogVolumes < maxElems);
	if (numFogVolumes >= maxElems)
		return 0;

	uint32 nIndex = ~0;
	m_fogVolumeContributions.push_back(fogVolumeContrib, nIndex);
	return static_cast<uint16>(nIndex + 1);
}

//////////////////////////////////////////////////////////////////////////
void CRenderView::GetFogVolumeContribution(uint16 idx, ColorF& rColor) const
{
	rColor = idx ? m_fogVolumeContributions[idx - 1] : ColorF(0.0f, 0.0f, 0.0f, 1.0f);
}

//////////////////////////////////////////////////////////////////////////
void CRenderView::SetTargetClearColor(const ColorF& color, bool bEnableClear)
{
	m_targetClearColor = color;
	m_bClearTarget = bEnableClear;
}

///////////////////////////////////////////////////////////////////////////////
// Sort priority: light id, shadow map type (Dynamic, DynamicDistance, Cached), shadow map lod
struct SCompareFrustumsByLightIds
{
	bool operator()(const SShadowFrustumToRender& rA, const SShadowFrustumToRender& rB) const
	{
		if (rA.nLightID != rB.nLightID)
		{
			return rA.nLightID < rB.nLightID;
		}
		else
		{
			if (rA.pFrustum->m_eFrustumType != rB.pFrustum->m_eFrustumType)
			{
				return int(rA.pFrustum->m_eFrustumType) < int(rB.pFrustum->m_eFrustumType);
			}
			else
			{
				return rA.pFrustum->nShadowMapLod < rB.pFrustum->nShadowMapLod;
			}
		}
	}
};

//////////////////////////////////////////////////////////////////////////
void CRenderView::SShadows::GenerateSortedFrustumsForTiledShadingByScreenspaceOverlap()
{
	for (auto& fr : m_renderFrustums)
	{
		const auto pLight = fr.pLight;

		// We are only interested in dynamic point lights
		if (fr.pFrustum->m_eFrustumType != ShadowMapFrustum::e_GsmDynamic &&
			fr.pFrustum->m_eFrustumType != ShadowMapFrustum::e_GsmDynamicDistance)
			continue;
		if (pLight->m_Flags & DLF_SUN)
			continue;

		Vec4 curLightRect = Vec4(
			float(pLight->m_sX),
			float(pLight->m_sY),
			float(pLight->m_sWidth),
			float(pLight->m_sHeight));

		auto frustumPair = std::make_pair(&fr, curLightRect);
		bool bNewSliceRequired = true;

		for (auto& curSlice : m_frustumsPerTiledShadingSlice)
		{
			bool bHasOverlappingLight = false;

			float minX = curLightRect.x, maxX = curLightRect.x + curLightRect.z;
			float minY = curLightRect.y, maxY = curLightRect.y + curLightRect.w;

			for (auto curFrustumPair : curSlice)
			{
				Vec4 lightRect = curFrustumPair.second;

				if (maxX >= lightRect.x && minX <= lightRect.x + lightRect.z &&
					maxY >= lightRect.y && minY <= lightRect.y + lightRect.w)
				{
					bHasOverlappingLight = true;
					break;
				}
			}

			if (!bHasOverlappingLight)
			{
				curSlice.push_back(frustumPair);
				bNewSliceRequired = false;
				break;
			}
		}

		if (bNewSliceRequired)
		{
			std::vector<TiledShadingFrustumCoveragePair> firstSliceFrustums(1, frustumPair);
			m_frustumsPerTiledShadingSlice.emplace_back(std::move(firstSliceFrustums));
		}
	}
}

//////////////////////////////////////////////////////////////////////////
void CRenderView::SShadows::CreateFrustumGroups()
{
	m_frustumsByLight.clear();
	for (auto& frustumList : m_frustumsByType)
		frustumList.resize(0);

	// Go thru frustums render views and make sure we can read from them.
	for (auto& fr : m_renderFrustums)
	{
		m_frustumsByLight[fr.nLightID].push_back(&fr);
		CRY_ASSERT(fr.pFrustum->bRestrictToRT);

		switch (fr.pFrustum->m_eFrustumType)
		{
		case ShadowMapFrustum::e_GsmDynamic:
		case ShadowMapFrustum::e_GsmDynamicDistance:
			{
				if (fr.pLight->m_Flags & DLF_SUN)
					m_frustumsByType[eShadowFrustumRenderType_SunDynamic].push_back(&fr);
				else
					m_frustumsByType[eShadowFrustumRenderType_LocalLight].push_back(&fr);
			}
			break;
		case ShadowMapFrustum::e_GsmCached:
			m_frustumsByType[eShadowFrustumRenderType_SunCached].push_back(&fr);
			break;
		case ShadowMapFrustum::e_HeightMapAO:
			m_frustumsByType[eShadowFrustumRenderType_HeightmapAO].push_back(&fr);
			break;
		case ShadowMapFrustum::e_Nearest:
		case ShadowMapFrustum::e_PerObject:
			if (fr.pFrustum->ShouldSample())
			{
				m_frustumsByType[eShadowFrustumRenderType_Custom].push_back(&fr);
			}
			break;
		}
	}
}

//////////////////////////////////////////////////////////////////////////
void CRenderView::SShadows::Clear()
{
	m_renderFrustums.clear();
	m_frustumsByLight.clear();
	for (auto& frustumList : m_frustumsByType)
		frustumList.resize(0);

	m_pShadowFrustumOwner = nullptr;
	m_nearestCasterBoxes.clear();
	m_frustumsPerTiledShadingSlice.clear();
}

//////////////////////////////////////////////////////////////////////////
void CRenderView::SShadows::AddNearestCaster(CRenderObject* pObj, const SRenderingPassInfo& passInfo)
{
	if (pObj->m_pRenderNode)
	{
		// CRenderProxy::GetLocalBounds is not thread safe due to lazy evaluation
		CRY_ASSERT(pObj->m_pRenderNode->GetRenderNodeType() != eERType_MovableBrush ||
		           !gRenDev->m_pRT->IsMultithreaded() ||
		           gRenDev->m_pRT->IsMainThread());

		Vec3 objTranslation = pObj->GetMatrix().GetTranslation();

		AABB* pObjectBox = m_nearestCasterBoxes.push_back_new();
		pObj->m_pRenderNode->GetLocalBounds(*pObjectBox);
		pObjectBox->min += objTranslation;
		pObjectBox->max += objTranslation;
	}
}

//////////////////////////////////////////////////////////////////////////
void CRenderView::SShadows::PrepareNearestShadows()
{
	FUNCTION_PROFILER_RENDERER();
	SShadowFrustumToRender* pNearestFrustum = nullptr;
	for (auto& fr : m_renderFrustums)
	{
		if (fr.pFrustum->m_eFrustumType == ShadowMapFrustum::e_Nearest)
		{
			pNearestFrustum = &fr;
			break;
		}
	}

	if (pNearestFrustum)
	{
		auto* pNearestShadowsView = reinterpret_cast<CRenderView*>(pNearestFrustum->pShadowsView.get());
		auto& nearestRenderItems = pNearestShadowsView->m_renderItems[0]; // NOTE: rend items go in list 0

		pNearestFrustum->pFrustum->aabbCasters.Reset();

		for (auto& fr : m_renderFrustums)
		{
			if (fr.pFrustum->m_eFrustumType == ShadowMapFrustum::e_GsmDynamic && (fr.pLight->m_Flags & DLF_SUN))
			{
				CRenderView* pShadowsView = reinterpret_cast<CRenderView*>(fr.pShadowsView.get());
				CRY_ASSERT(pShadowsView->m_usageMode == IRenderView::eUsageModeReading);

				for(const auto& AABB : pShadowsView->m_shadows.m_nearestCasterBoxes)
					pNearestFrustum->pFrustum->aabbCasters.Add(AABB);

				for (auto& ri : pShadowsView->GetRenderItems(EFSLIST_NEAREST_OBJECTS))
					nearestRenderItems.lockfree_push_back(ri);
			}
		}

		nearestRenderItems.CoalesceMemory();
		CRY_ASSERT(pNearestFrustum->pFrustum->bRestrictToRT);
		pNearestFrustum->pFrustum->RequestSamples(nearestRenderItems.empty() ? 0 : 1);
		pNearestShadowsView->SwitchUsageMode(CRenderView::eUsageModeReading);
	}
}

//////////////////////////////////////////////////////////////////////////
SRenderViewInfo::SRenderViewInfo()
	: pCamera(nullptr)
	, pFrustumPlanes(nullptr)
	, cameraOrigin(0)
	, cameraVX(0)
	, cameraVY(0)
	, cameraVZ(0)
	, nearClipPlane(0)
	, farClipPlane(1)
	, cameraProjZeroMatrix(IDENTITY)
	, cameraProjMatrix(IDENTITY)
	, cameraProjNearestMatrix(IDENTITY)
	, projMatrix(IDENTITY)
	, unjitteredProjMatrix(IDENTITY)
	, viewMatrix(IDENTITY)
	, invCameraProjMatrix(IDENTITY)
	, invViewMatrix(IDENTITY)
	, prevCameraMatrix(IDENTITY)
	, prevCameraProjMatrix(IDENTITY)
	, prevCameraProjNearestMatrix(IDENTITY)
	, m_frustumCorners {ZERO, ZERO, ZERO, ZERO}
	, downscaleFactor(1)
	, flags(eFlags_None)
{
}

//////////////////////////////////////////////////////////////////////////
void SRenderViewInfo::SetCamera(const CCamera& cam, const CCamera& previousCam, Vec2 subpixelShift, float drawNearestFov, float drawNearestFarPlane)
{
	cam.CalculateRenderMatrices();
	pCamera = &cam;

	pFrustumPlanes = cam.GetFrustumPlane(0);

	Matrix44A proj, nearestProj;

	float wl,wr,wb,wt;
	cam.GetAsymmetricFrustumParams(wl,wr,wb,wt);

	nearClipPlane = cam.GetNearPlane();
	farClipPlane = cam.GetFarPlane();
	cameraOrigin = cam.GetPosition();
	cameraVX = cam.GetRenderVectorX();
	cameraVY = cam.GetRenderVectorY();
	cameraVZ = cam.GetRenderVectorZ();

	if (flags & eFlags_ReverseDepth)
	{
		mathMatrixPerspectiveOffCenterReverseDepth((Matrix44A*)&proj, wl, wr, wb, wt, nearClipPlane, farClipPlane);
	}
	else
	{
		mathMatrixPerspectiveOffCenter((Matrix44A*)&proj, wl, wr, wb, wt, nearClipPlane, farClipPlane);
	}

	if (cam.IsObliqueClipPlaneEnabled())
	{
		Matrix44A mObliqueProjMatrix(IDENTITY);
		Plane obliqueClippingPlane = cam.GetObliqueClipPlane();
		mObliqueProjMatrix.m02 = obliqueClippingPlane.n[0];
		mObliqueProjMatrix.m12 = obliqueClippingPlane.n[1];
		mObliqueProjMatrix.m22 = obliqueClippingPlane.n[2];
		mObliqueProjMatrix.m32 = obliqueClippingPlane.d;

		proj = proj * mObliqueProjMatrix;
	}

	unjitteredProjMatrix = proj;

	if (flags & eFlags_SubpixelShift)
	{
		proj.m20 += subpixelShift.x;
		proj.m21 += subpixelShift.y;
	}

	nearestProj = GetNearestProjection(drawNearestFov, drawNearestFarPlane, subpixelShift);

	Matrix44 view, viewZero, invView;
	ExtractViewMatrices(cam, view, viewZero, invView);

	Matrix44 prevView, prevViewZero, prevInvView;
	ExtractViewMatrices(previousCam, prevView, prevViewZero, prevInvView);

	cameraProjMatrix = view * proj;
	cameraProjZeroMatrix = viewZero * proj;
	projMatrix = proj;
	viewMatrix = view;
	invViewMatrix = invView;
	cameraProjNearestMatrix = viewZero * nearestProj;

	prevCameraMatrix = prevView;
	prevCameraProjMatrix = prevView * proj;
	prevCameraProjNearestMatrix = prevView;
	prevCameraProjNearestMatrix.SetRow(3, Vec3(ZERO));
	prevCameraProjNearestMatrix = prevCameraProjNearestMatrix * nearestProj;

	Matrix44_tpl<f64> mProjInv;
	if (mathMatrixPerspectiveFovInverse(&mProjInv, &proj))
	{
		Matrix44_tpl<f64> mViewInv;
		mathMatrixLookAtInverse(&mViewInv, &view);
		invCameraProjMatrix = mProjInv * mViewInv;
	}
	else
	{
		invCameraProjMatrix = cameraProjMatrix.GetInverted();
	}

	//////////////////////////////////////////////////////////////////////////
	// Calculate Frustum Corners
	Vec3 vCoords[8];
	cam.CalcAsymmetricFrustumVertices(vCoords);

	// Swap order when mirrored culling enabled
	if (flags & eFlags_MirrorCamera)
	{
		m_frustumCorners[eFrustum_LT] = vCoords[4] - vCoords[0]; // vLT
		m_frustumCorners[eFrustum_RT] = vCoords[5] - vCoords[1]; // vRT
		m_frustumCorners[eFrustum_RB] = vCoords[6] - vCoords[2]; // vRB
		m_frustumCorners[eFrustum_LB] = vCoords[7] - vCoords[3]; // vLB
	}
	else
	{
		m_frustumCorners[eFrustum_RT] = vCoords[4] - vCoords[0]; // vRT
		m_frustumCorners[eFrustum_LT] = vCoords[5] - vCoords[1]; // vLT
		m_frustumCorners[eFrustum_LB] = vCoords[6] - vCoords[2]; // vLB
		m_frustumCorners[eFrustum_RB] = vCoords[7] - vCoords[3]; // vRB
	}
}

//////////////////////////////////////////////////////////////////////////
void SRenderViewInfo::ExtractViewMatrices(const CCamera& cam, Matrix44& view, Matrix44& viewZero, Matrix44& invView) const
{
	Matrix34_tpl<f64> mCam34 = cam.GetMatrix();
	mCam34.OrthonormalizeFast();

	Matrix44_tpl<f64> mCam44T = mCam34.GetTransposed();

	// Rotate around x-axis by -PI/2
	Vec4_tpl<f64> row1 = mCam44T.GetRow4(1);
	mCam44T.SetRow4(1, mCam44T.GetRow4(2));
	mCam44T.SetRow4(2, Vec4_tpl<f64>(-row1.x, -row1.y, -row1.z, -row1.w));

	if (flags & eFlags_MirrorCamera)
	{
		Vec4_tpl<f64> _row1 = mCam44T.GetRow4(1);
		mCam44T.SetRow4(1, Vec4_tpl<f64>(-_row1.x, -_row1.y, -_row1.z, -_row1.w));
	}
	invView = (Matrix44_tpl<f32>)mCam44T;

	Matrix44_tpl<f64> mView64;
	mathMatrixLookAtInverse(&mView64, &mCam44T);
	view = (Matrix44_tpl<f32>)mView64;

	viewZero = view;
	viewZero.SetRow(3, Vec3(ZERO));
}

//////////////////////////////////////////////////////////////////////////
float SRenderViewInfo::WorldToCameraZ(const Vec3& wP) const
{
	Vec3 sP(wP - cameraOrigin);
	float zdist = cameraVZ | sP;
	return zdist;
}

//////////////////////////////////////////////////////////////////////////

Matrix44 SRenderViewInfo::GetReprojection() const
{
	// TODO: Make sure NEAREST projection is handled correctly
	Matrix44_tpl<f64> matReprojection64;

	const Matrix44A& matProj = unjitteredProjMatrix; // Use Projection matrix without the pixel shift
	const Matrix44A& matView = viewMatrix;

	CRY_ASSERT(gcpRendD3D->GetS3DRend().GetStereoMode() == EStereoMode::STEREO_MODE_DUAL_RENDERING
	           || (matProj.m20 == 0 && matProj.m21 == 0)); // Ensure jittering is removed from projection matrix

	Matrix44_tpl<f64> matViewInv, matProjInv;
	mathMatrixLookAtInverse(&matViewInv, &matView);
	CRY_VERIFY(mathMatrixPerspectiveFovInverse(&matProjInv, &matProj));

	Matrix44_tpl<f64> matScaleBias1 = Matrix44_tpl<f64>(
		 0.5,  0.0, 0.0, 0.0,
		 0.0, -0.5, 0.0, 0.0,
		 0.0,  0.0, 1.0, 0.0,
		 0.5,  0.5, 0.0, 1.0);
	Matrix44_tpl<f64> matScaleBias2 = Matrix44_tpl<f64>(
		 2.0,  0.0, 0.0, 0.0,
		 0.0, -2.0, 0.0, 0.0,
		 0.0,  0.0, 1.0, 0.0,
		-1.0,  1.0, 0.0, 1.0);

	const Matrix44& mPrevView = prevCameraMatrix;
	const Matrix44& mPrevProj = matProj;

	matReprojection64 = matProjInv * matViewInv * Matrix44_tpl<f64>(mPrevView) * Matrix44_tpl<f64>(mPrevProj);
	matReprojection64 = matScaleBias2 * matReprojection64 * matScaleBias1;

	return (Matrix44)matReprojection64;
}

Matrix44 SRenderViewInfo::GetNearestProjection(float nearestFOV, float farPlane, Vec2 subpixelShift) const
{
	CRY_ASSERT(pCamera);

	Matrix44A result;

	float fFov = pCamera->GetFov();
	if (nearestFOV > 1.0f && nearestFOV < 179.0f)
		fFov = DEG2RAD(nearestFOV);

	float fNearRatio = DRAW_NEAREST_MIN / pCamera->GetNearPlane();
	float wT = tanf(fFov * 0.5f) * DRAW_NEAREST_MIN, wB = -wT;
	float wR = wT * pCamera->GetProjRatio(), wL = -wR;

	wL += pCamera->GetAsymL() * fNearRatio;
	wR += pCamera->GetAsymR() * fNearRatio;
	wB += pCamera->GetAsymB() * fNearRatio, wT += pCamera->GetAsymT() * fNearRatio;

	if (flags & eFlags_ReverseDepth) mathMatrixPerspectiveOffCenterReverseDepth(&result, wL, wR, wB, wT, DRAW_NEAREST_MIN, farPlane);
	else                             mathMatrixPerspectiveOffCenter(&result, wL, wR, wB, wT, DRAW_NEAREST_MIN, farPlane);

	if (flags & eFlags_SubpixelShift)
	{
		result.m20 += subpixelShift.x;
		result.m21 += subpixelShift.y;
	}

	return result;
}

TRect_tpl<uint16> CRenderView::ComputeResolveViewport(const AABB& aabb, bool forceFullscreenUpdate) const
{
	// TODO: Re-evaluate camera selection for multiviewport rendering
	const auto& camera = GetCamera(GetCurrentEye());
	const auto& viewport = GetViewport();

	TRect_tpl<uint16> resolveViewport;
	if (!forceFullscreenUpdate)
	{
		int iOut[4];

		camera.CalcScreenBounds(&iOut[0], &aabb, viewport.x, viewport.y, viewport.width, viewport.height);
		resolveViewport.Min.x = iOut[0];
		resolveViewport.Min.y = iOut[1];
		resolveViewport.Max.x = iOut[2];
		resolveViewport.Max.y = iOut[3];
	}
	else
	{
		resolveViewport.Min.x = static_cast<uint16>(viewport.x);
		resolveViewport.Min.y = static_cast<uint16>(viewport.y);
		resolveViewport.Max.x = static_cast<uint16>(viewport.width);
		resolveViewport.Max.y = static_cast<uint16>(viewport.height);
	}

	return resolveViewport;
}
