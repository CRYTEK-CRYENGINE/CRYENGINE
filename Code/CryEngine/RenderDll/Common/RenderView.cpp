// Copyright 2001-2016 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "RenderView.h"

#include "DriverD3D.h"

#include "GraphicsPipeline/ShadowMap.h"
#include "GraphicsPipeline/ClipVolumes.h"
#include "CompiledRenderObject.h"

#include "RendElements/CREClientPoly.h"

//////////////////////////////////////////////////////////////////////////
CRenderView::CRenderView(const char* name, EViewType type, CRenderView* pParentView, ShadowMapFrustum* pShadowFrustumOwner)
	: m_usageMode(eUsageModeUndefined)
	, m_viewType(type)
	, m_name(name)
	, m_frameId(0)
	, m_nSkipRenderingFlags(0)
	, m_nShaderRenderingFlags(0)
	, m_pParentView(pParentView)
	, m_bPostWriteExecuted(false)
	, m_bRenderOutput(false)
	, m_bTrackUncompiledItems(false)
	, m_numUsedClientPolygons(0)
	, m_bDeferrredNormalDecals(false)
	, m_bAddingClientPolys(false)
{
	for (int i = 0; i < EFSLIST_NUM; i++)
	{
		m_renderItems[i].Init();
		m_renderItems[i].SetNoneWorkerThreadID(gEnv->mMainThreadId);
	}

	m_shadows.m_pShadowFrustumOwner = pShadowFrustumOwner;

	m_permanentRenderObjectsToCompile.Init();
	m_permanentRenderObjectsToCompile.SetNoneWorkerThreadID(gEnv->mMainThreadId);

	m_permanentObjects.Init();
	m_permanentObjects.SetNoneWorkerThreadID(gEnv->mMainThreadId);

	m_polygonDataPool.reset(new CRenderPolygonDataPool);
}

//////////////////////////////////////////////////////////////////////////
CRenderView::~CRenderView()
{
	Clear();

	for (auto pClientPoly : m_polygonsPool)
		delete pClientPoly;
}

void CRenderView::Clear()
{
	m_numUsedClientPolygons = 0;
	m_polygonDataPool->Clear();

	for (int i = 0; i < EFSLIST_NUM; i++)
	{
		m_renderItems[i].clear();
		m_BatchFlags[i] = 0;
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

	ClearTemporaryCompiledObjects();
	m_permanentRenderObjectsToCompile.clear();

	m_bTrackUncompiledItems = gRenDev->m_nGraphicsPipeline >= 2;
}

// Helper function to allocate new compiled object from pool
CCompiledRenderObject* CRenderView::AllocCompiledObject(CRenderObject* pObj, CRenderElement* pElem, const SShaderItem& shaderItem)
{
	CCompiledRenderObject* pCompiledObject = CCompiledRenderObject::AllocateFromPool();
	pCompiledObject->Init(shaderItem, pElem);

	// Assign any compiled object to the RenderObject, just to be used as a root reference for constant buffer sharing
	pObj->m_pCompiledObject = pCompiledObject;

	return pCompiledObject;
}

// Helper function
CCompiledRenderObject* CRenderView::AllocCompiledObjectTemporary(CRenderObject* pObj, CRenderElement* pElem, const SShaderItem& shaderItem)
{
	CCompiledRenderObject* pCompiledObject = CCompiledRenderObject::AllocateFromPool();
	pCompiledObject->Init(shaderItem, pElem);

	// Assign any compiled object to the RenderObject, just to be used as a root reference for constant buffer sharing
	pObj->m_pCompiledObject = pCompiledObject;

	SCompiledPair pair;
	pair.pRenderObject = pObj;
	pair.pCompiledObject = pCompiledObject;
	m_temporaryCompiledObjects.push_back(pair);

	// Temporary object is not linked to the RenderObject owner
	return pCompiledObject;
}

SDeferredDecal* CRenderView::AddDeferredDecal(const SDeferredDecal& source)
{
	m_deferredDecals.push_back(source);
	return &m_deferredDecals.back();
}

std::vector<SDeferredDecal>& CRenderView::GetDeferredDecals()
{
	return m_deferredDecals;
}

void CRenderView::SetFrameId(uint64 frameId)
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

		CRenderCamera rcam;

		// Ortho-normalize camera matrix in double precision to minimize numerical errors and improve precision when inverting matrix
		Matrix34_tpl<f64> mCam34 = cam.GetMatrix();
		mCam34.OrthonormalizeFast();

		// Asymmetric frustum
		float Near = cam.GetNearPlane(), Far = cam.GetFarPlane();

		float wT = tanf(cam.GetFov() * 0.5f) * Near, wB = -wT;
		float wR = wT * cam.GetProjRatio(), wL = -wR;
		rcam.Frustum(wL + cam.GetAsymL(), wR + cam.GetAsymR(), wB + cam.GetAsymB(), wT + cam.GetAsymT(), Near, Far);

		Vec3 vEye = cam.GetPosition();
		Vec3 vAt = vEye + Vec3((f32)mCam34(0, 1), (f32)mCam34(1, 1), (f32)mCam34(2, 1));
		Vec3 vUp = Vec3((f32)mCam34(0, 2), (f32)mCam34(1, 2), (f32)mCam34(2, 2));
		rcam.LookAt(vEye, vAt, vUp);

		m_renderCamera[cam.GetEye()] = rcam;
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
void CRenderView::SwitchUsageMode(EUsageMode mode)
{
	FUNCTION_PROFILER_RENDERER();

	if (mode == m_usageMode)
		return;

	//assert(mode != m_usageMode);

	if (mode == eUsageModeWriting)
	{
		CRY_ASSERT(m_usageMode == IRenderView::eUsageModeUndefined || m_usageMode == IRenderView::eUsageModeReadingDone);
		CRY_ASSERT(!m_jobstate_Write.IsRunning());
		CRY_ASSERT(!m_jobstate_Sort.IsRunning());

		m_bPostWriteExecuted = false;

		// Post jobs are not executed immediately for Shadow views, they will only start when all shadow views are done writing
		//if (m_viewType != eViewType_Shadow)
		{
			// Job_PostWrite job will be called when all writing to the view will finish (m_jobstate_Write is stopped)
			auto lambda_job_after_write = [this] {
				Job_PostWrite();
			};
			m_jobstate_Write.RegisterPostJobCallback("JobRenderViewPostWrite", lambda_job_after_write, JobManager::eRegularPriority, &m_jobstate_PostWrite);
		}

		// If no items will be written, next SetStopped call will trigger post job
		m_jobstate_Write.SetRunning();
	}

	if (mode == eUsageModeWritingDone)
	{
		CRY_ASSERT(m_usageMode == IRenderView::eUsageModeWriting);

		// Write to view is finished, we can now start processing render items preparing them for rendering.
		// Try stopping writes, this can trigger PostWrite job
		if (!m_jobstate_Write.TryStopping())
		{
			// If still jobs running we cannot switch to WritingDone yet.
			return;
		}
	}

	if (mode == eUsageModeReading)
	{
		{
			PROFILE_FRAME(RenderViewWaitForPostWriteJob);

			// Prepare view for rasterizing (reading)
			// We now need to wait until all prepare for writing jobs are done.
			m_jobstate_Write.Wait();
			m_jobstate_PostWrite.Wait();
			m_jobstate_Sort.Wait();
		}

		//Job_PostWrite();
		assert(m_bPostWriteExecuted);

		CRY_ASSERT(m_usageMode == IRenderView::eUsageModeWritingDone);

		if (m_bRenderOutput)
		{
			m_renderOutput.BeginRendering();
		}

		CompileModifiedRenderObjects();
		UpdateModifiedShaderItems();
	}

	if (mode == eUsageModeReadingDone)
	{
		CRY_ASSERT(m_usageMode == IRenderView::eUsageModeReading);
		CRY_ASSERT(!m_jobstate_Write.IsRunning());
		CRY_ASSERT(!m_jobstate_PostWrite.IsRunning());
		CRY_ASSERT(!m_jobstate_Sort.IsRunning());

		if (m_bRenderOutput)
		{
			m_renderOutput.EndRendering();
			m_bRenderOutput = false;
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
	assert((light.m_Flags & DLF_LIGHTTYPE_MASK) != 0);

	RenderLightIndex nLightId = -1;
	if (m_lights[lightType].size() < MAX_DEFERRED_LIGHT_SOURCES)
	{
		if (!(light.m_Flags & DLF_FAKE))
		{
			nLightId = GetLightsCount(lightType);
		}

		m_lights[lightType].push_back(light);
		SRenderLight* pLight = &m_lights[lightType].back();

		pLight->m_Id = nLightId;
		pLight->AcquireResources();
	}

	return nLightId;
}

//////////////////////////////////////////////////////////////////////////
SRenderLight* CRenderView::AddLightAtIndex(eDeferredLightType lightType, const SRenderLight& light, RenderLightIndex nLightId /*=-1*/)
{
	assert(nLightId                   <  GetLightsCount(lightType));
	assert(MAX_DEFERRED_LIGHT_SOURCES >= GetLightsCount(lightType));

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
	pLight->AcquireResources();

	return pLight;
}

RenderLightIndex CRenderView::GetLightsCount(eDeferredLightType lightType) const
{
	return (RenderLightIndex)m_lights[lightType].size();
}

SRenderLight& CRenderView::GetLight(eDeferredLightType lightType, RenderLightIndex nLightId)
{
	assert(nLightId >= 0 && nLightId < m_lights[lightType].size());
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
uint8 CRenderView::AddClipVolume(const IClipVolume* pClipVolume)
{
	if (m_clipVolumes.size() < CClipVolumesStage::MaxDeferredClipVolumes)
	{
		m_clipVolumes.push_back(SDeferredClipVolume());

		auto& volume = m_clipVolumes.back();
		volume.nStencilRef = uint8(m_clipVolumes.size()); // the first clip volume ID is reserved for outdoors
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

	SDeferredClipVolume::BlendInfo emptyBlendInfo = { -1, Vec4(ZERO) };
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

	bool bRenderToShadowMap = (m_viewType == eViewType_Shadow);
	if (!bRenderToShadowMap)
	{
		batchFlags |= (pl->m_nCPFlags & CREClientPoly::efAfterWater) ? 0 : FB_BELOW_WATER;

		CShader* pShader = (CShader*)pl->m_Shader.m_pShader;
		CShaderResources* const __restrict pShaderResources = static_cast<CShaderResources*>(pl->m_Shader.m_pShaderResources);
		SShaderTechnique* pTech = pl->m_Shader.GetTechnique();

		if (pl->m_Shader.m_nPreprocessFlags & FSPR_MASK)
		{
			renderListId = EFSLIST_PREPROCESS;
		}

		if ((pShader->GetFlags() & EF_DECAL) || poly.pRenderObject && (poly.pRenderObject->m_ObjFlags & FOB_DECAL))
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
			if (poly.pRenderObject->m_fAlpha < 1.0f || (pShaderResources && pShaderResources->IsTransparent()))
			{
				renderListId = EFSLIST_TRANSP;
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

	AddRenderItem(pl, poly.pRenderObject, poly.shaderItem, renderListId, batchFlags, SRendItemSorter(), bRenderToShadowMap, false);
}

//////////////////////////////////////////////////////////////////////////
void CRenderView::SetRenderOutput(const CRenderOutput* pRenderOutput)
{
	if (pRenderOutput)
	{
		m_renderOutput = *pRenderOutput;
		m_bRenderOutput = true;
	}
	else
	{
		m_bRenderOutput = false;
	}
}

//////////////////////////////////////////////////////////////////////////
CRenderView::RenderItems& CRenderView::GetRenderItems(int nRenderList)
{
	assert(m_usageMode != eUsageModeWriting || (nRenderList == EFSLIST_PREPROCESS)); // While writing we must not read back items.

	m_renderItems[nRenderList].CoalesceMemory();
	return m_renderItems[nRenderList];
}

uint32 CRenderView::GetBatchFlags(int nRenderList) const
{
	return m_BatchFlags[nRenderList];
}

//////////////////////////////////////////////////////////////////////////
void CRenderView::AddPermanentObjectInline(CPermanentRenderObject* pObject, SRendItemSorter sorter, int shadowFrustumSide)
{
	SPermanentObjectRecord rec;
	rec.pRenderObject = pObject;
	rec.itemSorter = sorter.GetValue();
	rec.shadowFrustumSide = shadowFrustumSide;
	m_permanentObjects.push_back(rec);

	if (IsShadowGenView())
	{
		for (CPermanentRenderObject* pCurObj = pObject; pCurObj; pCurObj = pCurObj->m_pNextPermanent)
		{
			if (pCurObj->m_ObjFlags & FOB_NEAREST)
				m_shadows.AddNearestCaster(pCurObj);
		}
	}
}

//////////////////////////////////////////////////////////////////////////
void CRenderView::AddPermanentObject(CRenderObject* pObject, const SRenderingPassInfo& passInfo)
{
	assert(pObject->m_bPermanent);
	AddPermanentObjectInline(static_cast<CPermanentRenderObject*>(pObject), passInfo.GetRendItemSorter(), passInfo.ShadowFrustumSide());
}

//////////////////////////////////////////////////////////////////////////
void CRenderView::AddRenderItem(CRenderElement* pElem, CRenderObject* RESTRICT_POINTER pObj, const SShaderItem& shaderItem, uint32 nList, uint32 nBatchFlags,
                                SRendItemSorter sorter, bool bShadowPass, bool bForceOpaqueForward)
{
	assert(m_usageMode == eUsageModeWriting || m_bAddingClientPolys || nList == EFSLIST_PREPROCESS);  // Adding items only in writing mode

	CShader* RESTRICT_POINTER pShader = (CShader*)shaderItem.m_pShader;

	if (!bShadowPass)
	{
		const bool bHair = (pShader->m_Flags2 & EF2_HAIR) != 0;
		const bool bRefractive = (pShader->m_Flags & EF_REFRACTIVE) != 0;
		const bool bTransparent = shaderItem.m_pShaderResources && static_cast<CShaderResources*>(shaderItem.m_pShaderResources)->IsTransparent();

		if (nList == EFSLIST_GENERAL && (bHair || bTransparent) || 
		   (nList == EFSLIST_TRANSP && bRefractive))
		{
			nList = (pObj->m_ObjFlags & FOB_NEAREST) ? EFSLIST_TRANSP_NEAREST : EFSLIST_TRANSP;
		}
		else if (nList == EFSLIST_GENERAL && (!(pShader->m_Flags & EF_SUPPORTSDEFERREDSHADING_FULL) || bForceOpaqueForward))
		{
			// Redirect general list items to the forward opaque list
			nList = (pObj->m_ObjFlags & FOB_NEAREST) ? EFSLIST_FORWARD_OPAQUE_NEAREST : EFSLIST_FORWARD_OPAQUE;
		}
	}

	nBatchFlags |= (shaderItem.m_nPreprocessFlags & FSPR_MASK);

	SRendItem ri;

	ri.pObj = pObj;
	ri.pCompiledObject = nullptr;

	if (nList == EFSLIST_TRANSP || nList == EFSLIST_TRANSP_NEAREST || nList == EFSLIST_HALFRES_PARTICLES)
		ri.fDist = pObj->m_fDistance + pObj->m_fSort;
	else
		ri.ObjSort = (pObj->m_ObjFlags & 0xffff0000) | pObj->m_nSort;
	ri.nBatchFlags = nBatchFlags;
	//ri.nStencRef = pObj->m_nClipVolumeStencilRef + 1; // + 1, we start at 1. 0 is reserved for MSAAed areas.

	ri.rendItemSorter = sorter;

	ri.SortVal = SRendItem::PackShaderItem(shaderItem);
	ri.pElem = pElem;

	// objects with FOB_NEAREST go to EFSLIST_NEAREST in shadow pass and general
	if ((pObj->m_ObjFlags & FOB_NEAREST) && (bShadowPass || (nList == EFSLIST_GENERAL && CRenderer::CV_r_GraphicsPipeline > 0)))
	{
		nList = EFSLIST_NEAREST_OBJECTS;

		if (bShadowPass)
			m_shadows.AddNearestCaster(pObj);
	}

	//////////////////////////////////////////////////////////////////////////
	// Permanent objects support.
	//////////////////////////////////////////////////////////////////////////
	if (pObj->m_bPermanent)
	{
		CPermanentRenderObject* RESTRICT_POINTER pPermanentRendObj = reinterpret_cast<CPermanentRenderObject*>(pObj);
		WriteLock lock(pPermanentRendObj->m_accessLock); // Block on write access to the render object

		// Must add this render item to the local array.
		// This is not thread safe!!!, code at 3d engine makes sure this can never be called simultaneously on the same RenderObject on the same pass type.
		//  General and shadows can be called simultaneously, they are writing to the separate arrays.
		CPermanentRenderObject::SPermanentRendItem pri;
		pri.m_nBatchFlags = nBatchFlags;
		pri.m_nRenderList = nList;
		pri.m_objSort = ri.ObjSort;
		pri.m_pCompiledObject = ri.pCompiledObject;
		pri.m_pRenderElement = pElem;
		pri.m_sortValue = ri.SortVal;

		auto renderPassType = (bShadowPass) ? CPermanentRenderObject::eRenderPass_Shadows : CPermanentRenderObject::eRenderPass_General;
		pPermanentRendObj->m_permanentRenderItems[renderPassType].push_back(pri);

		pPermanentRendObj->PrepareForUse(pElem, renderPassType);

		return;
	}
	else if (m_bTrackUncompiledItems)
	{
		// This item will need a temporary compiled object
		EDataType reType = pElem ? pElem->mfGetType() : eDATA_Unknown;
		bool bMeshCompatibleRenderElement = reType == eDATA_Mesh || reType == eDATA_Terrain || reType == eDATA_GeomCache || reType == eDATA_ClientPoly;
		bool bCompiledRenderElement = (reType == eDATA_WaterVolume
		                               || reType == eDATA_WaterOcean
		                               || reType == eDATA_Sky
		                               || reType == eDATA_HDRSky
		                               || reType == eDATA_FogVolume);
		if (bMeshCompatibleRenderElement || bCompiledRenderElement) // temporary disable for these types
		{
			// Allocate new CompiledRenderObject.
			ri.pCompiledObject = AllocCompiledObjectTemporary(pObj, pElem, shaderItem);
		}
	}
	//////////////////////////////////////////////////////////////////////////
#if !defined(_RELEASE)
	if (CRenderer::CV_r_SkipAlphaTested && (ri.pObj->m_ObjFlags & FOB_ALPHATEST))
		return;
#endif

	AddRenderItemToRenderLists<true>(ri, nList, nBatchFlags, shaderItem);

	////////////////////////////////////////////////////////////////////////
	// Check if shader item needs update
	CheckAndScheduleForUpdate(shaderItem);
}

//////////////////////////////////////////////////////////////////////////
template<bool bConcurrent>
inline void UpdateRenderListBatchFlags(volatile uint32& listFlags, int newFlags)
{
	if (bConcurrent)
	{
		CryInterlockedExchangeOr((volatile LONG*)&listFlags, newFlags);
	}
	else
	{
		listFlags |= newFlags;
	}
}

template<bool bConcurrent>
inline void CRenderView::AddRenderItemToRenderLists(const SRendItem& ri, int nRenderList, int nBatchFlags, const SShaderItem& shaderItem)
{
	m_renderItems[nRenderList].push_back(ri);
	UpdateRenderListBatchFlags<bConcurrent>(m_BatchFlags[nRenderList], nBatchFlags);

	if (!IsShadowGenView())
	{
		const bool bForwardOpaqueFlags = (nBatchFlags & (FB_DEBUG | FB_TILED_FORWARD)) != 0;
		const bool bIsMaterialEmissive = (shaderItem.m_pShaderResources && shaderItem.m_pShaderResources->IsEmissive());
		const bool bIsTransparent = (nRenderList == EFSLIST_TRANSP) || (nRenderList == EFSLIST_TRANSP_NEAREST);
		const bool bIsSelectable = ri.pObj->m_editorSelectionID > 0;
		const bool bNearest = (ri.pObj->m_ObjFlags & FOB_NEAREST) != 0;

		const bool bGeneralList =
			nRenderList == EFSLIST_GENERAL ||
			nRenderList == EFSLIST_TERRAINLAYER ||
			nRenderList == EFSLIST_DECAL ||
			nRenderList == EFSLIST_NEAREST_OBJECTS;

		if (!bGeneralList && (nBatchFlags & FB_Z))
		{
			m_renderItems[EFSLIST_GENERAL].push_back(ri);
			UpdateRenderListBatchFlags<bConcurrent>(m_BatchFlags[EFSLIST_GENERAL], nBatchFlags);
		}

		if ((nBatchFlags & FB_ZPREPASS) && !bNearest)
		{
			m_renderItems[EFSLIST_ZPREPASS].push_back(ri);
			UpdateRenderListBatchFlags<bConcurrent>(m_BatchFlags[EFSLIST_ZPREPASS], nBatchFlags);
		}

		const bool bForwardOpaqueList = (nRenderList == EFSLIST_FORWARD_OPAQUE) || (nRenderList == EFSLIST_FORWARD_OPAQUE_NEAREST);
		if ((bForwardOpaqueFlags || bIsMaterialEmissive) && !bIsTransparent && !bForwardOpaqueList)
		{
			const int targetRenderList = bNearest ? EFSLIST_FORWARD_OPAQUE_NEAREST : EFSLIST_FORWARD_OPAQUE;
			m_renderItems[targetRenderList].push_back(ri);
			UpdateRenderListBatchFlags<bConcurrent>(m_BatchFlags[targetRenderList], nBatchFlags);
		}

		if (nBatchFlags & FB_PREPROCESS)
		{
			// Prevent water usage on non-water specific meshes (it causes reflections updates). Todo: this should be checked in editor side and not allow such usage
			EShaderType shaderType = reinterpret_cast<CShader*>(shaderItem.m_pShader)->m_eShaderType;
			if (shaderType != eST_Water || (shaderType == eST_Water && nRenderList == EFSLIST_WATER))
			{
				m_renderItems[EFSLIST_PREPROCESS].push_back(ri);
				UpdateRenderListBatchFlags<bConcurrent>(m_BatchFlags[EFSLIST_PREPROCESS], nBatchFlags);
			}
		}

		if (nBatchFlags & FB_CUSTOM_RENDER)
		{
			m_renderItems[EFSLIST_CUSTOM].push_back(ri);
			UpdateRenderListBatchFlags<bConcurrent>(m_BatchFlags[EFSLIST_CUSTOM], nBatchFlags);
		}

		if (bIsSelectable)
		{
			m_renderItems[EFSLIST_HIGHLIGHT].push_back(ri);
			UpdateRenderListBatchFlags<bConcurrent>(m_BatchFlags[EFSLIST_HIGHLIGHT], nBatchFlags);
		}
	}
}

void CRenderView::ExpandPermanentRenderObjects()
{
	PROFILE_FRAME(ExpandPermanentRenderObjects);

	m_permanentObjects.CoalesceMemory();

	if (!m_permanentObjects.empty())
	{
		assert(m_usageMode != eUsageModeWriting);
	}

	SShaderItem shaderItem;
	shaderItem.m_nPreprocessFlags = 0;

	int renderPassType = IsShadowGenView() ? CPermanentRenderObject::eRenderPass_Shadows : CPermanentRenderObject::eRenderPass_General;
	uint32 passId = IsShadowGenView() ? 1 : 0;
	uint32 passMask = 1 << passId;

	// Expand normal render items
	for (int obj = 0, numObj = m_permanentObjects.size(); obj < numObj; obj++)
	{
		const SPermanentObjectRecord& RESTRICT_REFERENCE record = m_permanentObjects[obj];
		CPermanentRenderObject* RESTRICT_POINTER pRenderObject = record.pRenderObject;
		assert(pRenderObject->m_bPermanent);

		bool bInvalidateChildObjects = false;

		// Submit all valid objects (skip not ready and helper objects), TODO: release helper objects
		while (pRenderObject)
		{
			ReadLock lock(pRenderObject->m_accessLock); // Block on read access to the render object

#if !defined(_RELEASE)
			if (CRenderer::CV_r_SkipAlphaTested && (pRenderObject->m_ObjFlags & FOB_ALPHATEST))
			{
				pRenderObject = pRenderObject->m_pNextPermanent;
				continue;
			}
#endif

			bool bRecompile = (pRenderObject->m_passReadyMask != pRenderObject->m_compiledReadyMask) && (pRenderObject->m_passReadyMask & passMask);

			auto& permanent_items = pRenderObject->m_permanentRenderItems[renderPassType];
			auto& RESTRICT_REFERENCE shadow_items = pRenderObject->m_permanentRenderItems[CPermanentRenderObject::eRenderPass_Shadows];
			int num_shadow_items = shadow_items.size();

			size_t numItems = permanent_items.size();
			assert(numItems < 128); // Sanity check, otherwise too many chunks in the mesh
			for (size_t i = 0; i < numItems; i++)
			{
				auto& RESTRICT_REFERENCE pri = permanent_items[i];

				SShaderItem shaderItem;
				SRendItem::ExtractShaderItem(pri.m_sortValue, shaderItem);

				if (!pri.m_pCompiledObject)
				{
					bool bRequireCompiledRenderObject = false;
					// This item will need a temporary compiled object
					if (pri.m_pRenderElement && (pri.m_pRenderElement->mfGetType() == eDATA_Mesh || pri.m_pRenderElement->mfGetType() == eDATA_Particle))
					{
						bRequireCompiledRenderObject = true;
					}

					if (bRequireCompiledRenderObject)
					{
						if (((volatile CCompiledRenderObject*) pri.m_pCompiledObject) == nullptr)
						{
							static CryCriticalSectionNonRecursive allocCS;
							AUTO_LOCK_T(CryCriticalSectionNonRecursive, allocCS);

							if (((volatile CCompiledRenderObject*)pri.m_pCompiledObject) == nullptr)
							{
								pri.m_pCompiledObject = AllocCompiledObject(pRenderObject, pri.m_pRenderElement, shaderItem); // Allocate new CompiledRenderObject.
								pRenderObject->m_bInstanceDataDirty = false;                                                  // In this case everything need to be recompiled, not only instance data.
								bRecompile = true;
							}
						}
					}
				}

				// Rebuild permanent object next frame in case we still rendered with the shader item which has been replaced (and is old)
				assert(shaderItem.m_pShader && shaderItem.m_pShaderResources);
				if (!shaderItem.m_pShaderResources->IsValid())
				{
					if (pRenderObject->m_pRenderNode)
						pRenderObject->m_pRenderNode->InvalidatePermanentRenderObject();
				}

				CheckAndScheduleForUpdate(shaderItem);

				SRendItem ri;
				ri.SortVal = pri.m_sortValue;
				ri.pElem = pri.m_pRenderElement;
				ri.pObj = pRenderObject;
				ri.pCompiledObject = pri.m_pCompiledObject;
				ri.ObjSort = pri.m_objSort;
				ri.nBatchFlags = pri.m_nBatchFlags;
				//ri.nStencRef = pRenderObject->m_nClipVolumeStencilRef + 1; // + 1, we start at 1. 0 is reserved for MSAAed areas.
				ri.rendItemSorter = SRendItemSorter(record.itemSorter);

				int renderList;
				if (renderPassType == CPermanentRenderObject::eRenderPass_Shadows)
					renderList = record.shadowFrustumSide;
				else
					renderList = pri.m_nRenderList;

				AddRenderItemToRenderLists<false>(ri, renderList, pri.m_nBatchFlags, shaderItem);

				if (pri.m_pRenderElement && pri.m_pRenderElement->m_Flags & (FCEF_DIRTY | FCEF_SKINNED | FCEF_UPDATEALWAYS))
				{
					pRenderObject->m_bInstanceDataDirty = false; // In this case everything need to be recompiled, not only instance data.
					bRecompile = true;
				}
			}

			if (bRecompile ||
			    pRenderObject->m_bInstanceDataDirty ||
			    !pRenderObject->m_bAllCompiledValid ||
			    bInvalidateChildObjects)
			{
				CPermanentRenderObject* pNonRestrict = pRenderObject;
				m_permanentRenderObjectsToCompile.push_back(pNonRestrict);
				pRenderObject->m_compiledReadyMask &= ~passMask;      // This compiled masks invalid
				bInvalidateChildObjects = true;
			}

			pRenderObject = pRenderObject->m_pNextPermanent;
		}
	}

	// Clean permanent objects
	m_permanentObjects.clear();
}

//////////////////////////////////////////////////////////////////////////
void CRenderView::CompileModifiedRenderObjects()
{
	CRY_PROFILE_FUNCTION_ARG(PROFILE_RENDERER, m_name.c_str())

	assert(gRenDev->m_pRT->IsRenderThread());

	// Prepare polygon data buffers.
	m_polygonDataPool->UpdateAPIBuffers();

	//////////////////////////////////////////////////////////////////////////
	// Compile all modified objects.
	m_permanentRenderObjectsToCompile.CoalesceMemory();

	uint32 passId = IsShadowGenView() ? 1 : 0;
	uint32 passMask = 1 << passId;

	int numObjects = m_permanentRenderObjectsToCompile.size();

	int nFrameId = gEnv->pRenderer->GetFrameID(false);
	//{ char buf[1024]; cry_sprintf(buf, "CRenderView::CompileModifiedRenderObjects: frame(%d) numObjects(%d) \r\n", nFrameId,numObjects); OutputDebugString(buf); }

	for (int obj = 0; obj < numObjects; obj++)
	{
		CPermanentRenderObject* pRenderObject = m_permanentRenderObjectsToCompile[obj];

		//if (pRenderObject->m_compiledReadyMask == pRenderObject->m_passReadyMask &&	pRenderObject->m_lastCompiledFrame == nFrameId)
		if (pRenderObject->m_compiledReadyMask == pRenderObject->m_passReadyMask)
		//if (pRenderObject->m_bAllCompiledValid && pRenderObject->m_lastCompiledFrame == nFrameId)
		{
			// Already compiled in this frame
			//continue;
		}

		ReadLock lock(pRenderObject->m_accessLock); // Block on read access to the render object

		// Do compilation on the chain of the compiled objects
		bool bAllCompiled = true;

		// compile items
		auto& RESTRICT_REFERENCE shadow_items = pRenderObject->m_permanentRenderItems[CPermanentRenderObject::eRenderPass_Shadows];
		auto& RESTRICT_REFERENCE general_items = pRenderObject->m_permanentRenderItems[CPermanentRenderObject::eRenderPass_General];

		for (int i = 0, num = general_items.size(); i < num; i++)
		{
			auto& pri = general_items[i];
			if (!pri.m_pCompiledObject)
				continue;
			if (!pri.m_pCompiledObject->Compile(pRenderObject))
			{
				bAllCompiled = false;
			}
		}

		for (int i = 0, num = shadow_items.size(); i < num; i++)
		{
			auto& pri = shadow_items[i];
			if (!pri.m_pCompiledObject || pri.m_pCompiledObject->m_bSharedWithShadow) // This compiled object must be already compiled with the general items
				continue;
			if (!pri.m_pCompiledObject->Compile(pRenderObject))
				bAllCompiled = false;
		}

		if (bAllCompiled)
		{
			pRenderObject->m_compiledReadyMask |= passMask;
		}

		pRenderObject->m_lastCompiledFrame = nFrameId;
		pRenderObject->m_bAllCompiledValid = bAllCompiled;
	}
	m_permanentRenderObjectsToCompile.clear();

	// Compile all temporary compiled objects
	m_temporaryCompiledObjects.CoalesceMemory();
	int numTempObjects = m_temporaryCompiledObjects.size();
	for (int i = 0; i < numTempObjects; i++)
	{
		auto& pair = m_temporaryCompiledObjects[i]; // first=CRenderObject, second=CCompiledObject
		pair.pCompiledObject->Compile(pair.pRenderObject);
	}

	//////////////////////////////////////////////////////////////////////////

#if defined(ENABLE_PROFILING_CODE)
	CryInterlockedAdd(&gcpRendD3D->m_RP.m_PS[gcpRendD3D->m_RP.m_nProcessThreadID].m_nModifiedCompiledObjects, numObjects);
	CryInterlockedAdd(&gcpRendD3D->m_RP.m_PS[gcpRendD3D->m_RP.m_nProcessThreadID].m_nTempCompiledObjects, numTempObjects);
#endif
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
		CheckAndScheduleForUpdate(decal.pMaterial->GetShaderItem(0));
	}

	m_shaderItemsToUpdate.CoalesceMemory();

	for (auto& item : m_shaderItemsToUpdate)
	{
		auto pShaderResources = item.first;
		auto pShader = item.second;

		if (pShaderResources->HasDynamicTexModifiers())
		{
			pShaderResources->RT_UpdateConstants(pShader);
		}

		if (pShaderResources->HasAnimatedTextures())
		{
			gcpRendD3D->FX_UpdateAnimatedShaderResources(pShaderResources);
		}

		if (pShaderResources->HasDynamicUpdates())
		{
			gcpRendD3D->FX_UpdateDynamicShaderResources(pShaderResources, FB_GENERAL, 0);
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
	size_t numObjects = m_temporaryCompiledObjects.size();
	for (size_t i = 0; i < numObjects; i++)
	{
		auto& pair = m_temporaryCompiledObjects[i];
		CCompiledRenderObject::FreeToPool(pair.pCompiledObject);
	}
	m_temporaryCompiledObjects.clear();
	/////////////////////////////////////////////////////////////////////////////
}

CRenderView::ItemsRange CRenderView::GetItemsRange(ERenderListID renderList)
{
	auto& items = GetRenderItems(renderList);
	return ItemsRange(0, items.size());
}

CRenderView::ItemsRange CRenderView::GetShadowItemsRange(ShadowMapFrustum* pFrustum, int nFrustumSide)
{
	if (nFrustumSide < EFSLIST_NUM)
	{
		auto& items = GetShadowItems(pFrustum, nFrustumSide);
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
	return pShadowsView->GetRenderItems(nFrustumSide);
}

//////////////////////////////////////////////////////////////////////////
void CRenderView::Job_PostWrite()
{
	CRY_PROFILE_FUNCTION_ARG(PROFILE_RENDERER, m_name.c_str());

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

	for (int renderList = 0; renderList < EFSLIST_NUM; renderList++)
	{
		if (renderList == EFSLIST_PREPROCESS && m_viewType != eViewType_Shadow)
		{
			// These lists need not to be sorted here
			continue;
		}

		auto& renderItems = GetRenderItems(renderList);

		if (!renderItems.empty())
		{
			auto lambda_job = [ = ]
			{
				Job_SortRenderItemsInList((ERenderListID)renderList);
			};
			gEnv->pJobManager->AddLambdaJob("SortRenderItems", lambda_job, JobManager::eRegularPriority, &m_jobstate_Sort);
			//lambda_job();
		}
	}
	m_bPostWriteExecuted = true;
}

///////////////////////////////////////////////////////////////////////////////
void CRenderView::Job_SortRenderItemsInList(ERenderListID list)
{
	FUNCTION_PROFILER_RENDERER();

	auto& renderItems = GetRenderItems(list);
	if (renderItems.empty())
		return;

	int nStart = 0;
	int n = renderItems.size();

	if (m_viewType == eViewType_Shadow)
	{
		// Sort Shadow render items differently
		//assert(m_shadows.m_frustums.size() == 1);// Should only have one current frustum.
		if (m_shadows.m_pShadowFrustumOwner)
			m_shadows.m_pShadowFrustumOwner->SortRenderItemsForFrustumAsync(list, &renderItems[0], renderItems.size());
		return;
	}

	if (m_viewType != eViewType_Shadow)
	{
		// Reorder all but shadow gen
		//std::sort(&renderItems[nStart],&renderItems[nStart] + n, SCompareByOnlyStableFlagsOctreeID());
	}

	switch (list)
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

	case EFSLIST_DEFERRED_PREPROCESS:
	case EFSLIST_FOG_VOLUME:
		break;

	case EFSLIST_WATER_VOLUMES:
	case EFSLIST_TRANSP:
	case EFSLIST_TRANSP_NEAREST:
	case EFSLIST_WATER:
	case EFSLIST_HALFRES_PARTICLES:
	case EFSLIST_LENSOPTICS:
	case EFSLIST_EYE_OVERLAY:
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

	case EFSLIST_ZPREPASS:
		//if (m_bUseGPUFriendlyBatching[nThread])
		{
			PROFILE_FRAME(State_SortingZPass);
			if (CRenderer::CV_r_ZPassDepthSorting == 1)
				SRendItem::mfSortForZPass(&renderItems[nStart], n);
			else if (CRenderer::CV_r_ZPassDepthSorting == 2)
				SRendItem::mfSortByDist(&renderItems[nStart], n, false, true);
		}
		break;

	case EFSLIST_GENERAL:
	case EFSLIST_SKIN:
	case EFSLIST_NEAREST_OBJECTS:
	case EFSLIST_DEBUG_HELPER:
		{
			PROFILE_FRAME(State_SortingGBuffer);
			if (CRenderer::CV_r_ZPassDepthSorting == 0)
				SRendItem::mfSortByLight(&renderItems[nStart], n, true, false, false);
			else if (CRenderer::CV_r_ZPassDepthSorting == 1)
				SRendItem::mfSortForZPass(&renderItems[nStart], n);
			else if (CRenderer::CV_r_ZPassDepthSorting == 2)
				SRendItem::mfSortByDist(&renderItems[nStart], n, false, true);
		}
		break;

	case EFSLIST_FORWARD_OPAQUE:
	case EFSLIST_FORWARD_OPAQUE_NEAREST:
	case EFSLIST_CUSTOM:
		{
			{
				PROFILE_FRAME(State_SortingForwardOpaque);
				SRendItem::mfSortByLight(&renderItems[nStart], n, true, false, false);
			}
		}
		break;

	case EFSLIST_AFTER_POSTPROCESS:
	case EFSLIST_AFTER_HDRPOSTPROCESS:
		{
			PROFILE_FRAME(State_SortingLight);
			SRendItem::mfSortByLight(&renderItems[nStart], n, true, false, list == EFSLIST_DECAL);
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

int CRenderView::FindRenderListSplit(ERenderListID list, uint32 objFlag)
{
	FUNCTION_PROFILER_RENDERER();

	// Binary search, assumes that list is sorted by objFlag
	auto& renderItems = GetRenderItems(list);

	int first = 0;
	int last = renderItems.size() - 1;

	while (first <= last)
	{
		int middle = (first + last) / 2;

		if (renderItems[middle].pObj->m_ObjFlags & objFlag)
			first = middle + 1;
		else
			last = middle - 1;
	}

	return last + 1;
}

//////////////////////////////////////////////////////////////////////////
void CRenderView::AddShadowFrustumToRender(const SShadowFrustumToRender& frustum)
{
	CRY_ASSERT(frustum.pShadowsView->GetFrameId() == GetFrameId());
	m_shadows.m_renderFrustums.push_back(frustum);
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

CRenderView::ShadowFrustumsPtr& CRenderView::GetShadowFrustumsByType(eShadowFrustumRenderType type)
{
	return m_shadows.m_frustumsByType[type];
}

//////////////////////////////////////////////////////////////////////////
void CRenderView::PostWriteShadowViews()
{
	// Rendering of our shadow views finished, but they may still write items in the jobs in the background
}

//////////////////////////////////////////////////////////////////////////
void CRenderView::PrepareShadowViews()
{
	FUNCTION_PROFILER_RENDERER();
	for (auto & fr : m_shadows.m_renderFrustums)
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
void CRenderView::CheckAndScheduleForUpdate(const SShaderItem& shaderItem)
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
void CRenderView::SShadows::CreateFrustumGroups()
{
	m_frustumsByLight.clear();
	for (auto& frustumList : m_frustumsByType)
		frustumList.resize(0);

	// Go thru frustums render views and make sure we can read from them.
	for (auto& fr : m_renderFrustums)
	{
		m_frustumsByLight[fr.nLightID].push_back(&fr);

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
			if (fr.pFrustum->nShadowGenMask)
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
	// Force clear call on all dependent shadow views.
	for (auto& fr : m_renderFrustums)
	{
		CRenderView* pShadowView = reinterpret_cast<CRenderView*>(fr.pShadowsView.get());
		pShadowView->Clear();
	}
	m_renderFrustums.resize(0);

	m_frustumsByLight.clear();
	for (auto& frustumList : m_frustumsByType)
		frustumList.resize(0);

	m_pShadowFrustumOwner = nullptr;
	m_nearestCasterBoxes.clear();
}

//////////////////////////////////////////////////////////////////////////
void CRenderView::SShadows::AddNearestCaster(CRenderObject* pObj)
{
	if (pObj->m_pRenderNode)
	{
		// CRenderProxy::GetLocalBounds is not thread safe due to lazy evaluation
		CRY_ASSERT(pObj->m_pRenderNode->GetRenderNodeType() != eERType_MovableBrush ||
		           !gRenDev->m_pRT->IsMultithreaded() ||
		           gRenDev->m_pRT->IsMainThread());

		AABB* pObjectBox = reinterpret_cast<AABB*>(m_nearestCasterBoxes.push_back_new());
		pObj->m_pRenderNode->GetLocalBounds(*pObjectBox);
		pObjectBox->min += pObj->GetTranslation();
		pObjectBox->max += pObj->GetTranslation();
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
		CRenderView* pNearestShadowsView = reinterpret_cast<CRenderView*>(pNearestFrustum->pShadowsView.get());
		RenderItems& nearestRenderItems = pNearestShadowsView->m_renderItems[0]; // NOTE: rend items go in list 0

		pNearestFrustum->pFrustum->aabbCasters.Reset();
		pNearestFrustum->pFrustum->nShadowGenMask = 0;

		for (auto& fr : m_renderFrustums)
		{
			if (fr.pFrustum->m_eFrustumType == ShadowMapFrustum::e_GsmDynamic && (fr.pLight->m_Flags & DLF_SUN))
			{
				CRenderView* pShadowsView = reinterpret_cast<CRenderView*>(fr.pShadowsView.get());
				CRY_ASSERT(pShadowsView->m_usageMode == IRenderView::eUsageModeReading);

				pShadowsView->m_shadows.m_nearestCasterBoxes.CoalesceMemory();
				for (int i = 0; i < pShadowsView->m_shadows.m_nearestCasterBoxes.size(); ++i)
					pNearestFrustum->pFrustum->aabbCasters.Add(pShadowsView->m_shadows.m_nearestCasterBoxes[i]);

				for (auto& ri : pShadowsView->GetRenderItems(EFSLIST_NEAREST_OBJECTS))
					nearestRenderItems.lockfree_push_back(ri);
			}
		}

		nearestRenderItems.CoalesceMemory();
		pNearestFrustum->pFrustum->nShadowGenMask = nearestRenderItems.empty() ? 0 : 1;
		pNearestShadowsView->SwitchUsageMode(CRenderView::eUsageModeReading);
	}
}

//////////////////////////////////////////////////////////////////////////
CRenderOutput::~CRenderOutput()
{
}

CRenderOutput::CRenderOutput(const SEnvTexture& envTex, const SHRenderTarget& renderTarget)
{
	m_pOutputDynTexture = envTex.m_pTex;

	m_bUseTempDepthBuffer = renderTarget.m_bTempDepth;
	if (m_bUseTempDepthBuffer)
	{
		m_width = envTex.m_pTex->GetWidth();
		m_height = envTex.m_pTex->GetHeight();
	}
	else
	{
		m_width = -1;
		m_height = -1;
	}

	m_clearTargetFlag = renderTarget.m_nFlags & (FRT_CLEAR_COLOR | FRT_CLEAR_DEPTH);
	m_clearColor = renderTarget.m_ClearColor;
	m_clearDepth = renderTarget.m_fClearDepth;
}

CRenderOutput::CRenderOutput(CTexture* pHDRTargetTex, int32 width, int32 height, bool bClear, ColorF clearColor)
{
	m_pOutputDynTexture = nullptr;

	CRY_ASSERT(pHDRTargetTex);
	m_pHDRTargetTexture = pHDRTargetTex;

	// Secondary viewports have to have their own depth/stencil buffers
	m_bUseTempDepthBuffer = true;

	m_width = width;
	m_height = height;

	if (bClear)
	{
		m_clearTargetFlag = (FRT_CLEAR_COLOR | FRT_CLEAR_DEPTH | FRT_CLEAR_STENCIL);
		m_clearColor = clearColor;
		m_clearDepth = 1.0f;
	}
}

void CRenderOutput::BeginRendering()
{
	CRY_ASSERT(gcpRendD3D->m_pRT->IsRenderThread());

	gcpRendD3D->FX_PushVP();

	if (m_pOutputDynTexture)
	{
		CRY_ASSERT(m_pHDRTargetTexture == nullptr);
		CRY_ASSERT(m_pDepthTexture == nullptr);

		m_pOutputDynTexture->Update(m_width, m_height);

		m_pHDRTargetTexture = m_pOutputDynTexture->m_pTexture;
	}
	else if (m_pHDRTargetTexture)
	{
		ETEX_Format texFormat = eTF_R16G16B16A16F;
		CRY_ASSERT(!CTexture::IsTextureExist(CTexture::s_ptexHDRTarget) || texFormat == CTexture::s_ptexHDRTarget->GetDstFormat()); // need to be same format to reuse same PSO.

		if (!CTexture::IsTextureExist(m_pHDRTargetTexture)
		    || m_pHDRTargetTexture->Invalidate(m_width, m_height, texFormat))
		{
			const uint32 flags = m_pHDRTargetTexture->GetFlags();
			m_pHDRTargetTexture->Create2DTexture(m_width, m_height, 1, flags, nullptr, texFormat);
		}
	}
	CRY_ASSERT(m_pHDRTargetTexture && CTexture::IsTextureExist(m_pHDRTargetTexture));

	if (m_bUseTempDepthBuffer)
	{
		SDepthTexture* pDepthSurface = gcpRendD3D->FX_GetDepthSurface(m_width, m_height, false, true);
		CRY_ASSERT(pDepthSurface);
		m_pDepthTexture = pDepthSurface->pTexture;
	}
	else
	{
		m_pDepthTexture = gcpRendD3D->m_pZTexture;
	}
	CRY_ASSERT(m_pDepthTexture);

	gcpRendD3D->RT_SetViewport(0, 0, m_pHDRTargetTexture->GetWidth(), m_pHDRTargetTexture->GetHeight());

	if (m_clearTargetFlag & (FRT_CLEAR_COLOR | FRT_CLEAR_DEPTH))
	{
		CDeviceCommandListRef commandList = GetDeviceObjectFactory().GetCoreCommandList();

		if (m_pDepthTexture && (m_clearTargetFlag & FRT_CLEAR_DEPTH))
		{
			const bool bReverseDepth = (gcpRendD3D->m_RP.m_TI[gcpRendD3D->m_RP.m_nProcessThreadID].m_PersFlags & RBPF_REVERSE_DEPTH) != 0;
			const float clearDepth = bReverseDepth ? 1.0f - m_clearDepth : m_clearDepth;

			D3DDepthSurface* pDsv = m_pDepthTexture->GetDevTexture()->LookupDSV(EDefaultResourceViews::DepthStencil);
			commandList.GetGraphicsInterface()->ClearSurface(pDsv, CLEAR_ZBUFFER, clearDepth, 0);
		}

		if (m_pHDRTargetTexture && (m_clearTargetFlag & FRT_CLEAR_COLOR))
		{
			commandList.GetGraphicsInterface()->ClearSurface(m_pHDRTargetTexture->GetSurface(0, 0), m_clearColor);
		}

		m_clearTargetFlag = 0;
	}
}

void CRenderOutput::EndRendering()
{
	CRY_ASSERT(gcpRendD3D->m_pRT->IsRenderThread());

	m_pOutputDynTexture = nullptr;
	m_pHDRTargetTexture = nullptr;
	m_pDepthTexture = nullptr;

	gcpRendD3D->FX_PopVP();
}

CTexture* CRenderOutput::GetHDRTargetTexture() const
{
	return m_pHDRTargetTexture;
}

CTexture* CRenderOutput::GetDepthTexture() const
{
	return m_pDepthTexture;
}
