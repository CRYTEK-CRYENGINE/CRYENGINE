// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "Water.h"

#include "DriverD3D.h"
#include "D3DPostProcess.h"
#include "Common/ReverseDepth.h"
#include "GraphicsPipeline/WaterRipples.h"
#include "GraphicsPipeline/VolumetricFog.h"
#include "GraphicsPipeline/TiledLightVolumes.h"

//////////////////////////////////////////////////////////////////////////

namespace water
{
// water stage unique parameters
struct SPerPassWater
{
	Matrix44 causticViewProjMatr;
	Vec4     waterRippleLookup;
	Vec4     ssrParams;
};

// fog parameters
struct SPerPassFog
{
	Vec4  vfParams;      // PB_VolumetricFogParams
	Vec4  vfRampParams;  // PB_VolumetricFogRampParams
	Vec4  vfSunDir;      // PB_VolumetricFogSunDir
	Vec3  vfColGradBase; // PB_FogColGradColBase
	float padding0;
	Vec3  vfColGradDelta; // PB_FogColGradColDelta
	float padding1;
	Vec4  vfColGradParams; // PB_FogColGradParams
	Vec4  vfColGradRadial; // PB_FogColGradRadial
};

// Volumetric fog shadows
struct SPerPassVolFogShadow
{
	Vec4 volFogShadowDarkening;
	Vec4 volFogShadowDarkeningSunAmb;
};

// voxel-based volumetric fog parameters
struct SPerPassVoxelVolFog
{
	Vec4 vfSamplingParams;           // PB_VolumetricFogSamplingParams
	Vec4 vfDistributionParams;       // PB_VolumetricFogDistributionParams
	Vec4 vfScatteringParams;         // PB_VolumetricFogScatteringParams
	Vec4 vfScatteringBlendParams;    // PB_VolumetricFogScatteringBlendParams
	Vec4 vfScatteringColor;          // PB_VolumetricFogScatteringColor
	Vec4 vfScatteringSecondaryColor; // PB_VolumetricFogScatteringSecondaryColor
	Vec4 vfHeightDensityParams;      // PB_VolumetricFogHeightDensityParams
	Vec4 vfHeightDensityRampParams;  // PB_VolumetricFogHeightDensityRampParams
	Vec4 vfDistanceParams;           // PB_VolumetricFogDistanceParams
	Vec4 vfGlobalEnvProbeParams0;    // PB_VolumetricFogGlobalEnvProbe0
	Vec4 vfGlobalEnvProbeParams1;    // PB_VolumetricFogGlobalEnvProbe1
};

struct SPerPassConstantBuffer
{
	SPerPassWater                             cbPerPassWater;
	SPerPassFog                               cbPerPassFog;
	SPerPassVolFogShadow                      cbVolFogShadow;
	SPerPassVoxelVolFog                       cbVoxVolFog;
	CShadowUtils::SShadowCascadesSamplingInfo cbShadowSampling;
};

struct SPrimitiveConstants
{
	Matrix44 transformMatrix;
};

void GetSnapVector(Vec3& vVector, const float fSnapRange)
{
	Vec3 vSnapped = vVector / fSnapRange;
	vSnapped.Set(floor_tpl(vSnapped.x), floor_tpl(vSnapped.y), floor_tpl(vSnapped.z));
	vSnapped *= fSnapRange;
	vVector = vSnapped;
}
}

//////////////////////////////////////////////////////////////////////////

bool CWaterStage::UpdateCausticsGrid(N3DEngineCommon::SCausticInfo& causticInfo, bool& bVertexUpdated, CRenderer* pRenderer)
{
	// 16 bit index limit, can only do max 256x256 grid.
	// could use hardware tessellation to reduce memory and increase tessellation amount for higher precision.
	const uint32 nCausticMeshWidth = clamp_tpl(CRenderer::CV_r_watervolumecausticsdensity, 16, 255);
	const uint32 nCausticMeshHeight = clamp_tpl(CRenderer::CV_r_watervolumecausticsdensity, 16, 255);

	bVertexUpdated = false;

	// Update the grid mesh if required.
	if ((!causticInfo.m_pCausticQuadMesh || causticInfo.m_nCausticMeshWidth != nCausticMeshWidth || causticInfo.m_nCausticMeshHeight != nCausticMeshHeight))
	{
		// Make sure we aren't recreating the mesh.
		causticInfo.m_pCausticQuadMesh = NULL;

		const uint32 nCausticVertexCount = (nCausticMeshWidth + 1) * (nCausticMeshHeight + 1);
		const uint32 nCausticIndexCount = (nCausticMeshWidth * nCausticMeshHeight) * 6;

		// Store the new resolution and vertex/index counts.
		causticInfo.m_nCausticMeshWidth = nCausticMeshWidth;
		causticInfo.m_nCausticMeshHeight = nCausticMeshHeight;
		causticInfo.m_nVertexCount = nCausticVertexCount;
		causticInfo.m_nIndexCount = nCausticIndexCount;

		// Reciprocal for scaling.
		float fRecipW = 1.0f / (float)nCausticMeshWidth;
		float fRecipH = 1.0f / (float)nCausticMeshHeight;

		// Buffers.
		SVF_P3F_C4B_T2F* pCausticQuads = new SVF_P3F_C4B_T2F[nCausticVertexCount];
		vtx_idx* pCausticIndices = new vtx_idx[nCausticIndexCount];

		// Fill vertex buffer.
		for (uint32 y = 0; y <= nCausticMeshHeight; ++y)
		{
			for (uint32 x = 0; x <= nCausticMeshWidth; ++x)
			{
				pCausticQuads[y * (nCausticMeshWidth + 1) + x].xyz = Vec3(((float)x) * fRecipW, ((float)y) * fRecipH, 0.0f);
			}
		}

		// Fill index buffer.
		for (uint32 y = 0; y < nCausticMeshHeight; ++y)
		{
			for (uint32 x = 0; x < nCausticMeshWidth; ++x)
			{
				PREFAST_SUPPRESS_WARNING(6386)
				pCausticIndices[(y * (nCausticMeshWidth) + x) * 6] = y * (nCausticMeshWidth + 1) + x;
				pCausticIndices[(y * (nCausticMeshWidth) + x) * 6 + 1] = y * (nCausticMeshWidth + 1) + x + 1;
				pCausticIndices[(y * (nCausticMeshWidth) + x) * 6 + 2] = (y + 1) * (nCausticMeshWidth + 1) + x + 1;
				pCausticIndices[(y * (nCausticMeshWidth) + x) * 6 + 3] = (y + 1) * (nCausticMeshWidth + 1) + x + 1;
				pCausticIndices[(y * (nCausticMeshWidth) + x) * 6 + 4] = (y + 1) * (nCausticMeshWidth + 1) + x;
				pCausticIndices[(y * (nCausticMeshWidth) + x) * 6 + 5] = y * (nCausticMeshWidth + 1) + x;
			}
		}

		// Create the mesh.
		causticInfo.m_pCausticQuadMesh = pRenderer->CreateRenderMeshInitialized(pCausticQuads, nCausticVertexCount, EDefaultInputLayouts::P3F_C4B_T2F, pCausticIndices, nCausticIndexCount, prtTriangleList, "WaterCausticMesh", "WaterCausticMesh");

		// Delete the temporary buffers.
		delete[] pCausticQuads;
		delete[] pCausticIndices;

		bVertexUpdated = true;
	}

	// If we created the mesh, return true.
	if (causticInfo.m_pCausticQuadMesh)
		return true;

	return false;
}

CWaterStage::CWaterStage()
	: m_rainRippleTexIndex(0)
	, m_frameIdWaterSim(0)
	, m_bWaterNormalGen(false)
	, m_bOceanMaskGen(false)
	, m_defaultPerInstanceResources()
	, m_perPassResources()
{
	std::fill(std::begin(m_oceanAnimationParams), std::end(m_oceanAnimationParams), Vec4(0.0f));
}

void CWaterStage::Init()
{
	m_pDefaultPerInstanceResourceSet = GetDeviceObjectFactory().CreateResourceSet(CDeviceResourceSet::EFlags_ForceSetAllState);
	for (uint32 i = 0; i < ePass_Count; ++i)
	{
		m_pPerPassResourceSets[i] = GetDeviceObjectFactory().CreateResourceSet(CDeviceResourceSet::EFlags_ForceSetAllState);
		m_pPerPassCB[i] = gcpRendD3D->m_DevBufMan.CreateConstantBuffer(sizeof(water::SPerPassConstantBuffer));
	}

	CRY_ASSERT(m_pFoamTex == nullptr);
	m_pFoamTex = CTexture::ForNamePtr("%ENGINE%/EngineAssets/Shading/WaterFoam.tif", FT_DONT_STREAM, eTF_Unknown);

	const string fileNamePath = "%ENGINE%/EngineAssets/Textures/Rain/Ripple/ripple";
	const string fileNameExt = "_ddn.tif";
	uint32 index = 1;
	for (auto& pTex : m_pRainRippleTex)
	{
		CRY_ASSERT(pTex == nullptr);
		string fileName = fileNamePath + string().Format("%d", index) + fileNameExt;
		pTex = CTexture::ForNamePtr(fileName.c_str(), FT_DONT_STREAM, eTF_Unknown);
		++index;
	}

	CRY_ASSERT(m_pJitterTex == nullptr);
	m_pJitterTex = CTexture::ForNamePtr("%ENGINE%/EngineAssets/Textures/FogVolShadowJitter.tif", FT_DONT_STREAM, eTF_Unknown);

	CRY_ASSERT(m_pWaterGlossTex == nullptr);
	m_pWaterGlossTex = CTexture::ForNamePtr("%ENGINE%/EngineAssets/Textures/water_gloss.tif", FT_DONT_STREAM, eTF_Unknown);

	CRY_ASSERT(m_pOceanWavesTex == nullptr);
	m_pOceanWavesTex = CTexture::ForNamePtr("%ENGINE%/EngineAssets/Textures/oceanwaves_ddn.tif", FT_DONT_STREAM, eTF_Unknown);

	CRY_ASSERT(m_pOceanCausticsTex == nullptr);
	m_pOceanCausticsTex = CTexture::ForNamePtr("%ENGINE%/EngineAssets/Textures/caustics_sampler.dds", FT_DONT_STREAM, eTF_Unknown);

	CConstantBufferPtr pCB = gcpRendD3D->m_DevBufMan.CreateConstantBuffer(sizeof(water::SPrimitiveConstants));
	m_deferredOceanStencilPrimitive[0].SetInlineConstantBuffer(eConstantBufferShaderSlot_PerPrimitive, pCB, EShaderStage_Vertex);
	m_deferredOceanStencilPrimitive[1].SetInlineConstantBuffer(eConstantBufferShaderSlot_PerPrimitive, pCB, EShaderStage_Vertex);

	m_aniso16xClampSampler      = CDeviceObjectFactory::GetOrCreateSamplerStateHandle(SSamplerState(FILTER_ANISO16X, eSamplerAddressMode_Clamp, eSamplerAddressMode_Clamp, eSamplerAddressMode_Clamp, 0x0));
	m_aniso16xWrapSampler       = CDeviceObjectFactory::GetOrCreateSamplerStateHandle(SSamplerState(FILTER_ANISO16X, eSamplerAddressMode_Wrap, eSamplerAddressMode_Wrap, eSamplerAddressMode_Wrap, 0x0));
	m_linearCompareClampSampler = CDeviceObjectFactory::GetOrCreateSamplerStateHandle(SSamplerState(FILTER_LINEAR, eSamplerAddressMode_Clamp, eSamplerAddressMode_Clamp, eSamplerAddressMode_Clamp, 0x0, true));
	m_linearMirrorSampler       = CDeviceObjectFactory::GetOrCreateSamplerStateHandle(SSamplerState(FILTER_LINEAR, eSamplerAddressMode_Mirror, eSamplerAddressMode_Clamp, eSamplerAddressMode_Clamp, 0x0));

	PrepareDefaultPerInstanceResources();
	for (uint32 i = 0; i < ePass_Count; ++i)
		SetAndBuildPerPassResources(true, EPass(i));

	// Create resource layout
	m_pResourceLayout = CreateScenePassLayout(m_perPassResources[ePass_WaterSurface]);

	// Freeze resource-set layout (assert will fire when violating the constraint)
	for (uint32 i = 0; i < ePass_Count; ++i)
		m_perPassResources[i].AcceptChangedBindPoints();

	// TODO: move a texture to local member variable.
	//CRY_ASSERT(CTexture::IsTextureExist(CRendererResources::s_ptexWaterCaustics[0])); // this can fail because CRendererResources::s_ptexWaterCaustics[0] is null when launching the engine with r_watervolumecaustics=0.
	CRY_ASSERT(CTexture::IsTextureExist(CRendererResources::s_ptexWaterVolumeRefl[0]));

	m_passOceanMaskGen.SetLabel("OCEAN_MASK_GEN");
	m_passOceanMaskGen.SetupPassContext(m_stageID, ePass_OceanMaskGen, TTYPE_GENERAL, FB_GENERAL, EFSLIST_WATER, 0, false);
	m_passOceanMaskGen.SetPassResources(m_pResourceLayout, m_pPerPassResourceSets[ePass_OceanMaskGen]);

	auto* pDummyRenderTarget = CRendererResources::s_ptexSceneSpecular;
	CRY_ASSERT(pDummyRenderTarget && pDummyRenderTarget->GetTextureDstFormat() == eTF_R8G8B8A8);

	m_passWaterCausticsSrcGen.SetLabel("WATER_VOLUME_CAUSTICS_SRC_GEN");
	m_passWaterCausticsSrcGen.SetupPassContext(m_stageID, ePass_CausticsGen, TTYPE_WATERCAUSTICPASS, FB_WATER_CAUSTIC, EFSLIST_WATER, 0, false);
	m_passWaterCausticsSrcGen.SetPassResources(m_pResourceLayout, m_pPerPassResourceSets[ePass_CausticsGen]);
	m_passWaterCausticsSrcGen.SetRenderTargets(nullptr, pDummyRenderTarget);

	m_passWaterFogVolumeBeforeWater.SetLabel("WATER_FOG_VOLUME_BEFORE_WATER");
	m_passWaterFogVolumeBeforeWater.SetupPassContext(m_stageID, ePass_FogVolume, TTYPE_GENERAL, FB_BELOW_WATER, EFSLIST_WATER_VOLUMES, 0, false);
	m_passWaterFogVolumeBeforeWater.SetPassResources(m_pResourceLayout, m_pPerPassResourceSets[ePass_FogVolume]);

	m_passWaterReflectionGen.SetLabel("WATER_VOLUME_REFLECTION_GEN");
	m_passWaterReflectionGen.SetupPassContext(m_stageID, ePass_ReflectionGen, TTYPE_WATERREFLPASS, FB_WATER_REFL, EFSLIST_WATER, 0, false);
	m_passWaterReflectionGen.SetPassResources(m_pResourceLayout, m_pPerPassResourceSets[ePass_ReflectionGen]);
	m_passWaterReflectionGen.SetRenderTargets(nullptr, CRendererResources::s_ptexWaterVolumeRefl[0]);

	m_passWaterSurface.SetLabel("WATER_SURFACE");
	m_passWaterSurface.SetupPassContext(m_stageID, ePass_WaterSurface, TTYPE_GENERAL, FB_GENERAL, EFSLIST_WATER, 0, false);
	m_passWaterSurface.SetPassResources(m_pResourceLayout, m_pPerPassResourceSets[ePass_WaterSurface]);

	m_passWaterFogVolumeAfterWater.SetLabel("WATER_FOG_VOLUME_AFTER_WATER");
	m_passWaterFogVolumeAfterWater.SetupPassContext(m_stageID, ePass_FogVolume, TTYPE_GENERAL, FB_GENERAL, EFSLIST_WATER_VOLUMES, FB_BELOW_WATER, false);
	m_passWaterFogVolumeAfterWater.SetPassResources(m_pResourceLayout, m_pPerPassResourceSets[ePass_FogVolume]);
}

void CWaterStage::Update()
{
	CRenderView* pRenderView = RenderView();
	auto* pDepthTarget = RenderView()->GetDepthTarget();
	auto* pRenderTarget = CRendererResources::s_ptexHDRTarget;

	auto& waterRenderItems = pRenderView->GetRenderItems(EFSLIST_WATER);
	auto& waterVolumeRenderItems = pRenderView->GetRenderItems(EFSLIST_WATER_VOLUMES);
	const bool isEmpty = waterRenderItems.empty() && waterVolumeRenderItems.empty();

	// Create Domain Shader Texture
	// NOTE: textures which are static uniform (with path in the code) are assigned before
	// the render-element is called, so if the render element allocated the required textures
	// then they will not make it into the first draw of the shader, and are 0 instead
	if (!isEmpty && !CTexture::IsTextureExist(CRendererResources::s_ptexWaterOcean))
	{
		CRendererResources::s_ptexWaterOcean->Create2DTexture(nGridSize, nGridSize, 1, FT_DONT_RELEASE | FT_NOMIPS, nullptr, eTF_R32G32B32A32F);
	}

	// Activate normal generation
	m_bWaterNormalGen = (gRenDev->EF_GetRenderQuality() >= eRQ_Medium && !isEmpty) ? true : false;
	m_bOceanMaskGen   = (gRenDev->EF_GetRenderQuality() >= eRQ_High) ? true : false;

	// Dynamic dependencies besides dimensions, re-call de-/allocation code
	Resize(pDepthTarget->GetWidth(), pDepthTarget->GetHeight());

	if (m_bOceanMaskGen)
		m_passOceanMaskGen.SetRenderTargets(pDepthTarget, m_pOceanMaskTex);

	m_passWaterFogVolumeBeforeWater.SetRenderTargets(pDepthTarget, pRenderTarget);
	m_passWaterSurface.SetRenderTargets(pDepthTarget, pRenderTarget);
	m_passWaterFogVolumeAfterWater.SetRenderTargets(pDepthTarget, pRenderTarget);
}

void CWaterStage::Prepare()
{
	if (m_defaultPerInstanceResources.HasChanged())
	{
		PrepareDefaultPerInstanceResources();

		// prepare resources early to avoid assert during initial upload. TODO: should not be necessary
		auto pGraphicsInterface = GetDeviceObjectFactory().GetCoreCommandList().GetGraphicsInterface();
		pGraphicsInterface->PrepareResourcesForUse(EResourceLayoutSlot_PerDrawExtraRS, m_pDefaultPerInstanceResourceSet.get());
	}
}

void CWaterStage::Resize(int renderWidth, int renderHeight)
{
	const uint32 flags = FT_NOMIPS | FT_DONT_STREAM | FT_USAGE_RENDERTARGET; 

	m_pOceanMaskTex = CTexture::GetOrCreateTextureObjectPtr("$OceanMask", renderWidth, renderHeight, 1, eTT_2D, flags, eTF_R8);
	if (m_pOceanMaskTex)
	{
		const bool shouldApplyMaskGen = m_bOceanMaskGen;

		// Create/release the displacement texture on demand
		if (!shouldApplyMaskGen && CTexture::IsTextureExist(m_pOceanMaskTex))
			m_pOceanMaskTex->ReleaseDeviceTexture(false);
		else if (shouldApplyMaskGen && (m_pOceanMaskTex->Invalidate(renderWidth, renderHeight, eTF_R8) || !CTexture::IsTextureExist(m_pOceanMaskTex)))
			m_pOceanMaskTex->CreateRenderTarget(eTF_R8G8B8A8, Clr_Transparent);
	}
}

void CWaterStage::ExecuteWaterVolumeCaustics()
{
	CRenderView* pRenderView = RenderView();
	CRY_ASSERT(pRenderView);

	// must be called before all render passes in water stage.
	Prepare();

	if (pRenderView->IsRecursive())
	{
		return;
	}

	CD3D9Renderer* const RESTRICT_POINTER rd = gcpRendD3D;

	const bool bWaterRipple = (pRenderView->GetShaderRenderingFlags() & SHDF_ALLOWPOSTPROCESS) != 0;

	auto& graphicsPipeline = rd->GetGraphicsPipeline();
	auto* pWaterRipplesStage = graphicsPipeline.GetWaterRipplesStage();

	const auto& waterRenderItems = pRenderView->GetRenderItems(EFSLIST_WATER);
	const auto& waterVolumeRenderItems = pRenderView->GetRenderItems(EFSLIST_WATER_VOLUMES);
	const bool bEmpty = waterRenderItems.empty() && waterVolumeRenderItems.empty();

	// process water ripples
	if (pWaterRipplesStage)
	{
		if (!bEmpty && bWaterRipple)
		{
			pWaterRipplesStage->Execute();
		}
	}

	// generate normal texture for water volumes and ocean.
	if (m_bWaterNormalGen && pRenderView->GetCurrentEye() == CCamera::eEye_Left)
	{
		ExecuteWaterNormalGen();
	}

	ExecuteOceanMaskGen();

	// Check if there are any water volumes that have caustics enabled
	bool isEmpty = bEmpty;
	if (!bEmpty)
	{
		auto& RESTRICT_REFERENCE RI = pRenderView->GetRenderItems(EFSLIST_WATER);

		int32 curRI = 0;
		int32 endRI = RI.size();

		isEmpty = true;

		while (curRI < endRI)
		{
			CRenderElement* pRE = RI[curRI++].pElem;
			if (pRE->m_Type == eDATA_WaterVolume
			    && ((CREWaterVolume*)pRE)->m_pParams
			    && ((CREWaterVolume*)pRE)->m_pParams->m_caustics == true)
			{
				isEmpty = false;
				break;
			}
		}
	}

	const uint32 nBatchMask = pRenderView->GetBatchFlags(EFSLIST_WATER);

	if (!isEmpty
	    && (nBatchMask & FB_WATER_CAUSTIC)
	    && CTexture::IsTextureExist(CRendererResources::s_ptexWaterCaustics[0])
	    && CTexture::IsTextureExist(CRendererResources::s_ptexWaterCaustics[1])
	    && CRenderer::CV_r_watercaustics
	    && CRenderer::CV_r_watercausticsdeferred
	    && CRenderer::CV_r_watervolumecaustics)
	{
		PROFILE_LABEL_SCOPE("WATERVOLUME_CAUSTICS");

		// TODO: move this to local member if it isn't used in other stages.
		// Caustics info.
		N3DEngineCommon::SCausticInfo& causticInfo = rd->m_p3DEngineCommon.m_CausticInfo;

		// NOTE: caustics gen texture is generated only once if stereo rendering is activated.
		if (pRenderView->GetCurrentEye() == CCamera::eEye_Left)
		{
			ExecuteWaterVolumeCausticsGen(causticInfo);
		}

		if (causticInfo.m_pCausticQuadMesh)
		{
			// NOTE: this function is called instead in CDeferredShading::Render() when tiled deferred shading is disabled.
			const bool bTiledDeferredShading = CRenderer::CV_r_DeferredShadingTiled >= 2;
			if (bTiledDeferredShading)
			{
				ExecuteDeferredWaterVolumeCaustics();
			}
		}
	}
}

void CWaterStage::ExecuteDeferredWaterVolumeCaustics()
{
	if (!CRendererResources::s_ptexBackBuffer || !CRendererResources::s_ptexSceneTarget)
	{
		return;
	}

	PROFILE_LABEL_SCOPE("DEFERRED_WATERVOLUME_CAUSTICS");

	CD3D9Renderer* const RESTRICT_POINTER rd = gcpRendD3D;
	N3DEngineCommon::SCausticInfo& causticInfo = rd->m_p3DEngineCommon.m_CausticInfo;

	auto* pTargetTex = CRendererResources::s_ptexSceneTargetR11G11B10F[1];
	auto& pass = m_passDeferredWaterVolumeCaustics;

	if (pass.IsDirty())
	{
		static CCryNameTSCRC techName = "WaterVolumeCaustics";
		pass.SetTechnique(CShaderMan::s_ShaderDeferredCaustics, techName, 0);

		pass.SetRenderTarget(0, pTargetTex);
		pass.SetState(GS_NODEPTHTEST);

		pass.SetTexture(0, CRendererResources::s_ptexLinearDepth);
		pass.SetTexture(1, CRendererResources::s_ptexSceneNormalsMap);
		pass.SetTexture(2, CRendererResources::s_ptexWaterCaustics[0]);

		pass.SetSampler(0, EDefaultSamplerStates::TrilinearClamp);
		pass.SetSampler(1, EDefaultSamplerStates::PointClamp);
	
		pass.SetPrimitiveFlags(CRenderPrimitive::eFlags_ReflectShaderConstants_PS);
		pass.SetPrimitiveType(CRenderPrimitive::ePrim_ProceduralTriangle);
		pass.SetRequirePerViewConstantBuffer(true);
	}

	pass.BeginConstantUpdate();

	static CCryNameR paramLightView("mLightView");
	pass.SetConstantArray(paramLightView, (Vec4*)causticInfo.m_mCausticMatr.GetData(), 4);

	pass.Execute();

	GetStdGraphicsPipeline().GetTiledLightVolumesStage()->NotifyCausticsVisible();
}

void CWaterStage::ExecuteDeferredOceanCaustics()
{
	if (!CRenderer::CV_r_watercaustics
	    || !CRenderer::CV_r_watercausticsdeferred
	    || !CRendererResources::s_ptexBackBuffer
	    || !CRendererResources::s_ptexSceneTarget)
	{
		return;
	}

	CD3D9Renderer* const RESTRICT_POINTER rd = gcpRendD3D;
	I3DEngine* RESTRICT_POINTER pEng = gEnv->p3DEngine;

	const Vec4 causticParams = gEnv->p3DEngine->GetOceanAnimationCausticsParams();
	const float fCausticsHeight = causticParams.y;
	const float fCausticsDepth = causticParams.z;
	const float fCausticsIntensity = causticParams.w;

	N3DEngineCommon::SOceanInfo& OceanInfo = rd->m_p3DEngineCommon.m_OceanInfo;
	const bool bOceanVolumeVisible = (OceanInfo.m_nOceanRenderFlags & OCR_OCEANVOLUME_VISIBLE) != 0;
	if (!bOceanVolumeVisible || iszero(fCausticsIntensity))
	{
		return;
	}

	PROFILE_LABEL_SCOPE("OCEAN_CAUSTICS");
	PROFILE_FRAME(DrawShader_DeferredCausticsPass);

	// Caustics are done with projection from sun - hence they update too fast with regular
	// sun direction. Use a smooth sun direction update instead to workaround this
	SRenderViewShaderConstants& PF = RenderView()->GetShaderConstants();
	if (PF.nCausticsFrameID != RenderView()->GetFrameId())
	{
		PF.nCausticsFrameID = RenderView()->GetFrameId();

		Vec3 pRealtimeSunDirNormalized = pEng->GetRealtimeSunDirNormalized();

		const float fSnapDot = 0.98f;
		float fDot = fabs(PF.vCausticsCurrSunDir.Dot(pRealtimeSunDirNormalized));
		if (fDot < fSnapDot)
			PF.vCausticsCurrSunDir = pRealtimeSunDirNormalized;

		const float factor = 0.005f;
		PF.vCausticsCurrSunDir += (pRealtimeSunDirNormalized - PF.vCausticsCurrSunDir) * factor * gEnv->pTimer->GetFrameTime();
		PF.vCausticsCurrSunDir.Normalize();
	}

	const float fWatLevel = OceanInfo.m_fWaterLevel;
	const float fCausticsLevel = fWatLevel + fCausticsHeight;
	const Vec4 pCausticsParams = Vec4(CRenderer::CV_r_watercausticsdistance,
	                                  OceanInfo.m_vCausticsParams.z * fCausticsIntensity,
	                                  OceanInfo.m_vCausticsParams.w,
	                                  fCausticsLevel);

	SRenderViewInfo viewInfo[2];
	const size_t viewInfoCount = GetGraphicsPipeline().GenerateViewInfo(viewInfo);
	CRY_ASSERT(viewInfoCount > 0);

	static const uint8 stencilReadWriteMask = 0xFF & ~BIT_STENCIL_RESERVED;
	uint8 stencilRef = 0xFF;

	auto* pTargetTex = CRendererResources::s_ptexHDRTarget;
	auto* pDepthTarget = RenderView()->GetDepthTarget();

	// Stencil pre-pass
	if (CRenderer::CV_r_watercausticsdeferred == 2)
	{
		Matrix34 mLocal[2];
		const float fDist = sqrtf((pCausticsParams.x * 5.0f) * 13.333f); // Hard cut off when caustic would be attenuated to 0.2 (1/5.0f)
		for (uint32 i = 0; i < viewInfoCount; ++i)
		{
			Vec3 vCamPos = viewInfo[i].cameraOrigin;

			float heightAboveWater = max(0.0f, vCamPos.z - fCausticsLevel);
			const float dist = sqrtf(max((fDist * fDist) - (heightAboveWater * heightAboveWater), 0.0f));

			mLocal[i].SetIdentity();
			mLocal[i].SetScale(Vec3(dist * 2.0f, dist * 2.0f, fCausticsHeight + fCausticsDepth));
			mLocal[i].SetTranslation(Vec3(vCamPos.x - dist, vCamPos.y - dist, fWatLevel - fCausticsDepth));
		}

		static CCryNameTSCRC techName("DeferredOceanCausticsStencil");

		const bool bReverseDepth = (viewInfo[0].flags & SRenderViewInfo::eFlags_ReverseDepth) != 0;
		const int32 gsDepthFunc = bReverseDepth ? GS_DEPTHFUNC_GEQUAL : GS_DEPTHFUNC_LEQUAL;

		rd->m_nStencilMaskRef += 1;
		if (rd->m_nStencilMaskRef > STENC_MAX_REF)
		{
			CClearSurfacePass::Execute(pDepthTarget, FRT_CLEAR_STENCIL, Clr_Unused.r, Val_Stencil);
			rd->m_nStencilMaskRef = Val_Stencil + 1;
		}

		stencilRef = rd->m_nStencilMaskRef;
		CRY_ASSERT(rd->m_nStencilMaskRef > 0 && rd->m_nStencilMaskRef <= STENC_MAX_REF);

		auto& pass = m_passDeferredOceanCausticsStencil;
		pass.SetDepthTarget(pDepthTarget);
		pass.BeginAddingPrimitives();

		D3DViewPort viewport;
		viewport.TopLeftX = 0.0f;
		viewport.TopLeftY = 0.0f;
		viewport.Width = static_cast<float>(pTargetTex->GetWidth());
		viewport.Height = static_cast<float>(pTargetTex->GetHeight());
		viewport.MinDepth = 0.0f;
		viewport.MaxDepth = 1.0f;
		pass.SetViewport(viewport);

		auto& backFacePrim = m_deferredOceanStencilPrimitive[0];
		if (backFacePrim.IsDirty())
		{
			backFacePrim.SetTechnique(CShaderMan::s_ShaderDeferredCaustics, techName, 0);

			backFacePrim.SetPrimitiveType(CRenderPrimitive::ePrim_UnitBox);

			backFacePrim.SetCullMode(eCULL_Front);
			backFacePrim.SetRenderState(GS_STENCIL | GS_NOCOLMASK_RGBA | gsDepthFunc);

			backFacePrim.SetInlineConstantBuffer(eConstantBufferShaderSlot_PerView, rd->GetGraphicsPipeline().GetMainViewConstantBuffer(), EShaderStage_Vertex);
		}

		static const int32 stencilStateBackFace = STENC_FUNC(FSS_STENCFUNC_GEQUAL) |
		                                          STENCOP_FAIL(FSS_STENCOP_KEEP) |
		                                          STENCOP_ZFAIL(FSS_STENCOP_REPLACE) |
		                                          STENCOP_PASS(FSS_STENCOP_KEEP);
		backFacePrim.SetStencilState(stencilStateBackFace, stencilRef, stencilReadWriteMask, stencilReadWriteMask);
		backFacePrim.Compile(pass);

		// Update constant buffer
		{
			auto& constantManager = backFacePrim.GetConstantManager();

			auto constants = constantManager.BeginTypedConstantUpdate<water::SPrimitiveConstants>(eConstantBufferShaderSlot_PerPrimitive, EShaderStage_Vertex);
			constants->transformMatrix = mLocal[0];

			if (viewInfoCount > 1)
			{
				constants.BeginStereoOverride();
				constants->transformMatrix = mLocal[1];
			}

			constantManager.EndTypedConstantUpdate(constants);
		}

		pass.AddPrimitive(&backFacePrim);

		auto& frontFacePrim = m_deferredOceanStencilPrimitive[1];
		if (frontFacePrim.IsDirty())
		{
			frontFacePrim.SetTechnique(CShaderMan::s_ShaderDeferredCaustics, techName, 0);

			frontFacePrim.SetPrimitiveType(CRenderPrimitive::ePrim_UnitBox);

			frontFacePrim.SetCullMode(eCULL_Back);
			frontFacePrim.SetRenderState(GS_STENCIL | GS_NOCOLMASK_RGBA | gsDepthFunc);

			frontFacePrim.SetInlineConstantBuffer(eConstantBufferShaderSlot_PerView, rd->GetGraphicsPipeline().GetMainViewConstantBuffer(), EShaderStage_Vertex);
		}

		static const int32 stencilStateFrontFace = STENC_FUNC(FSS_STENCFUNC_GEQUAL) |
		                                           STENCOP_FAIL(FSS_STENCOP_KEEP) |
		                                           STENCOP_ZFAIL(FSS_STENCOP_ZERO) |
		                                           STENCOP_PASS(FSS_STENCOP_KEEP);
		frontFacePrim.SetStencilState(stencilStateFrontFace, stencilRef, stencilReadWriteMask, stencilReadWriteMask);
		frontFacePrim.Compile(pass);
		pass.AddPrimitive(&frontFacePrim);

		pass.Execute();
	}

	// Deferred ocean caustic pass
	{
		auto* pOceanMask = CTexture::IsTextureExist(m_pOceanMaskTex) ? m_pOceanMaskTex.get() : CRendererResources::s_ptexBlack;

		auto& pass = m_passDeferredOceanCaustics;

		if (pass.IsDirty(CRenderer::CV_r_watercausticsdeferred, pOceanMask->GetID()))
		{
			static CCryNameTSCRC techName = "General";
			pass.SetPrimitiveFlags(CRenderPrimitive::eFlags_ReflectShaderConstants_PS);
			pass.SetTechnique(CShaderMan::s_ShaderDeferredCaustics, techName, 0);

			pass.SetRenderTarget(0, pTargetTex);
			pass.SetDepthTarget(pDepthTarget);

			int32 nRState = GS_NODEPTHTEST
			                | ((CRenderer::CV_r_watercausticsdeferred == 2) ? GS_STENCIL : 0)
			                | (GS_BLSRC_ONE | GS_BLDST_ONEMINUSSRCALPHA);
			pass.SetState(nRState);

			pass.SetTexture(0, CRendererResources::s_ptexLinearDepth);
			pass.SetTexture(1, CRendererResources::s_ptexSceneNormalsMap);
			pass.SetTexture(2, m_pOceanWavesTex);
			pass.SetTexture(3, m_pOceanCausticsTex);
			pass.SetTexture(4, pOceanMask);

			pass.SetSampler(0, EDefaultSamplerStates::TrilinearClamp);
			pass.SetSampler(1, EDefaultSamplerStates::TrilinearWrap);
			pass.SetSampler(2, EDefaultSamplerStates::PointClamp);

			pass.SetRequirePerViewConstantBuffer(true);
		}

		static const int32 stencilStateTest = STENC_FUNC(FSS_STENCFUNC_EQUAL) |
		                                      STENCOP_FAIL(FSS_STENCOP_KEEP) |
		                                      STENCOP_ZFAIL(FSS_STENCOP_KEEP) |
		                                      STENCOP_PASS(FSS_STENCOP_KEEP);
		pass.SetStencilState(stencilStateTest, stencilRef, stencilReadWriteMask, stencilReadWriteMask);

		pass.BeginConstantUpdate();

		static CCryNameR nameCausticParams("vCausticParams");
		pass.SetConstant(nameCausticParams, pCausticsParams);

		float fTime = 0.125f * GetGraphicsPipeline().GetAnimationTime().GetSeconds();
		Vec4 vAnimParams(0.06f * fTime, 0.05f * fTime, 0.1f * fTime, -0.11f * fTime);

		static CCryNameR nameAnimParams("vAnimParams");
		pass.SetConstant(nameAnimParams, vAnimParams);

		Vec3 up = Vec3(0, 0, 1);
		Vec3 dirZ = -PF.vCausticsCurrSunDir.GetNormalized();
		Vec3 dirX = up.Cross(dirZ).GetNormalized();
		Vec3 dirY = dirZ.Cross(dirX).GetNormalized();

		Matrix44 lightView;
		lightView.SetIdentity();
		lightView.SetRow(0, dirX);
		lightView.SetRow(1, dirY);
		lightView.SetRow(2, dirZ);

		static CCryNameR nameLightView("mLightView");
		pass.SetConstantArray(nameLightView, (Vec4*)lightView.GetData(), 4);

		pass.Execute();
	}
}

void CWaterStage::ExecuteWaterFogVolumeBeforeTransparent()
{
	const CRenderView* pRenderView = RenderView();
	CRY_ASSERT(pRenderView);

	if (pRenderView->IsRecursive())
	{
		return;
	}

	// prepare per pass device resource
	// this update must be called once after water ripple stage's execute() because waterRippleLookup parameter depends on it.
	// this update needs to be called after the execute() of fog and volumetric fog because their data is updated in their execute().
	SetAndBuildPerPassResources(false, ePass_FogVolume);

	// render water fog volumes before water.
	ExecuteSceneRenderPass(m_passWaterFogVolumeBeforeWater, EFSLIST_WATER_VOLUMES);
}

void CWaterStage::Execute()
{
	CRenderView* pRenderView = RenderView();
	CRY_ASSERT(pRenderView);

	if (pRenderView->IsRecursive())
	{
		return;
	}

	PROFILE_LABEL_SCOPE("WATER");

	// TODO: these should be kept in new graphics pipeline when main viewport is rendered.
	CRY_ASSERT(pRenderView->GetShaderRenderingFlags() & (SHDF_ALLOW_WATER | SHDF_ALLOWPOSTPROCESS));

	auto* pWaterRipplesStage = GetStdGraphicsPipeline().GetWaterRipplesStage();

	const auto& waterRenderItems = pRenderView->GetRenderItems(EFSLIST_WATER);
	const auto& waterVolumeRenderItems = pRenderView->GetRenderItems(EFSLIST_WATER_VOLUMES);
	const bool bEmpty = waterRenderItems.empty() && waterVolumeRenderItems.empty();

	// Pre-process refraction
	if (!bEmpty && CTexture::IsTextureExist(CRendererResources::s_ptexSceneTarget))
	{
		if (CRenderer::CV_r_debugrefraction == 0)
		{
			m_passCopySceneTarget.Execute(CRendererResources::s_ptexHDRTarget, CRendererResources::s_ptexSceneTarget);
		}
		else
		{
			CClearSurfacePass::Execute(CRendererResources::s_ptexSceneTarget, Clr_Debug);
		}
	}

	// generate water volume reflection.
	SetAndBuildPerPassResources(false, ePass_ReflectionGen);
	ExecuteReflection();

	// render water volume and ocean surfaces.
	SetAndBuildPerPassResources(false, ePass_WaterSurface);
	ExecuteSceneRenderPass(m_passWaterSurface, EFSLIST_WATER);

	// render water fog volumes after water.
	ExecuteSceneRenderPass(m_passWaterFogVolumeAfterWater, EFSLIST_WATER_VOLUMES);
}

bool CWaterStage::CreatePipelineStates(uint32 passMask, DevicePipelineStatesArray& stateArray, const SGraphicsPipelineStateDescription& stateDesc, CGraphicsPipelineStateLocalCache* pStateCache)
{
	if (pStateCache->Find(stateDesc, stateArray))
		return true;

	bool bFullyCompiled = (passMask != 0) ? true : false;

	SGraphicsPipelineStateDescription _stateDesc = stateDesc;

	if (passMask & ePassMask_WaterSurface)
	{
		_stateDesc.technique = TTYPE_GENERAL;
		bFullyCompiled &= CreatePipelineState(stateArray[ePass_WaterSurface], _stateDesc, ePass_WaterSurface, nullptr);
	}

	if (passMask & ePassMask_ReflectionGen)
	{
		_stateDesc.technique = TTYPE_WATERREFLPASS;
		bFullyCompiled &= CreatePipelineState(stateArray[ePass_ReflectionGen], _stateDesc, ePass_ReflectionGen, nullptr);
	}

	if (passMask & ePassMask_FogVolume)
	{
		_stateDesc.technique = TTYPE_GENERAL;
		bFullyCompiled &= CreatePipelineState(stateArray[ePass_FogVolume], _stateDesc, ePass_FogVolume, nullptr);
	}

	if (passMask & ePassMask_CausticsGen)
	{
		_stateDesc.technique = TTYPE_WATERCAUSTICPASS;
		bFullyCompiled &= CreatePipelineState(stateArray[ePass_CausticsGen], _stateDesc, ePass_CausticsGen, nullptr);
	}

	if (bFullyCompiled)
	{
		pStateCache->Put(stateDesc, stateArray);
	}

	return bFullyCompiled;
}

bool CWaterStage::CreatePipelineState(
  CDeviceGraphicsPSOPtr& outPSO,
  const SGraphicsPipelineStateDescription& desc,
  EPass passID,
  std::function<void(CDeviceGraphicsPSODesc& psoDesc)> modifier)
{
	outPSO = nullptr;

	if (!(ePass_ReflectionGen <= passID && passID <= ePass_Count))
		return true; // psssID doesn't exit in water stage.

	if (!m_bOceanMaskGen && (passID == ePass_OceanMaskGen))
		return true; // OceanMaskGen is not needed (low/medium spec)

	auto shaderType = desc.shaderItem.m_pShader->GetShaderType();
	if (shaderType != eST_Water)
		return true; // non water type shader can't be rendered in water stage.

	CDeviceGraphicsPSODesc psoDesc(m_pResourceLayout, desc);
	if (!GetStdGraphicsPipeline().FillCommonScenePassStates(desc, psoDesc))
		return true; // technique doesn't exist so null PSO is returned.

	if (modifier)
	{
		modifier(psoDesc);
	}

	bool bWaterRipples = false;

	switch (passID)
	{
	case CWaterStage::ePass_ReflectionGen:
		{
			psoDesc.m_pRenderPass = m_passWaterReflectionGen.GetRenderPass();

			if (CRenderer::CV_r_DeferredShadingTiled > 0)
				psoDesc.m_ShaderFlags_RT |= g_HWSR_MaskBit[HWSR_TILED_SHADING];

			bWaterRipples = true;
		}
		break;
	case CWaterStage::ePass_FogVolume:
		{
			psoDesc.m_pRenderPass = m_passWaterFogVolumeBeforeWater.GetRenderPass();
		}
		break;
	case CWaterStage::ePass_WaterSurface:
		{
			psoDesc.m_pRenderPass = m_passWaterSurface.GetRenderPass();

			if (CRenderer::CV_r_DeferredShadingTiled > 0)
				psoDesc.m_ShaderFlags_RT |= g_HWSR_MaskBit[HWSR_TILED_SHADING];

			bWaterRipples = true;
		}
		break;
	case CWaterStage::ePass_CausticsGen:
		{
			psoDesc.m_pRenderPass = m_passWaterCausticsSrcGen.GetRenderPass();

			bWaterRipples = true;
		}
		break;
	case CWaterStage::ePass_OceanMaskGen:
		{
			psoDesc.m_pRenderPass = m_passOceanMaskGen.GetRenderPass();

			// use optimized shader to generate mask.
			psoDesc.m_ShaderFlags_RT |= g_HWSR_MaskBit[HWSR_SAMPLE3];

			bWaterRipples = true;
		}
		break;
	default:
		CRY_ASSERT(false);
		return true;
	}

	// always adds the shader runtime mask for water ripples to avoid shader permutations.
	// TODO: move shader runtime masks to shader constants to avoid shader permutations after removing old graphics pipeline.
	psoDesc.m_ShaderFlags_RT |= bWaterRipples ? g_HWSR_MaskBit[HWSR_SAMPLE4] : 0;

	const bool bReverseDepth = true;
	if (bReverseDepth)
	{
		psoDesc.m_RenderState = ReverseDepthHelper::ConvertDepthFunc(psoDesc.m_RenderState);
	}

	outPSO = GetDeviceObjectFactory().CreateGraphicsPSO(psoDesc);

	return (outPSO != nullptr);
}

CDeviceResourceLayoutPtr CWaterStage::CreateScenePassLayout(const CDeviceResourceSetDesc& perPassResources)
{
	SDeviceResourceLayoutDesc layoutDesc;

	layoutDesc.SetConstantBuffer(EResourceLayoutSlot_PerDrawCB, eConstantBufferShaderSlot_PerDraw, EShaderStage_Vertex | EShaderStage_Domain | EShaderStage_Pixel);

	layoutDesc.SetResourceSet(EResourceLayoutSlot_PerMaterialRS, gcpRendD3D->GetGraphicsPipeline().GetDefaultMaterialBindPoints());
	layoutDesc.SetResourceSet(EResourceLayoutSlot_PerDrawExtraRS, m_defaultPerInstanceResources); // <- this is different from standard graphics pipeline stages
	layoutDesc.SetResourceSet(EResourceLayoutSlot_PerPassRS, perPassResources);

	CDeviceResourceLayoutPtr pResourceLayout = GetDeviceObjectFactory().CreateResourceLayout(layoutDesc);
	assert(pResourceLayout != nullptr);
	return pResourceLayout;
}

bool CWaterStage::PrepareDefaultPerInstanceResources()
{
	CD3D9Renderer* RESTRICT_POINTER pRenderer = gcpRendD3D;
	auto& res = m_defaultPerInstanceResources;

	// default textures for water volume
	{
		res.SetTexture(ePerInstanceTexture_Foam, m_pFoamTex, EDefaultResourceViews::Default, EShaderStage_Pixel);
		res.SetTexture(ePerInstanceTexture_Displacement, CRendererResources::s_ptexBlack, EDefaultResourceViews::Default, EShaderStage_Vertex | EShaderStage_Domain);

		if (!pRenderer->m_bPauseTimer)
		{
			// flip rain ripple texture
			const float elapsedTime = GetGraphicsPipeline().GetAnimationTime().GetSeconds();
			CRY_ASSERT(elapsedTime >= 0.0f);
			const float AnimTexFlipTime = 0.05f;
			m_rainRippleTexIndex = (uint32)(elapsedTime / AnimTexFlipTime) % m_pRainRippleTex.size();
		}

		CRY_ASSERT(m_rainRippleTexIndex < m_pRainRippleTex.size());
		res.SetTexture(ePerInstanceTexture_RainRipple, m_pRainRippleTex[m_rainRippleTexIndex], EDefaultResourceViews::Default, EShaderStage_Pixel);
		// NOTE ePerInstanceTexture_OceanReflection maps to ePerInstanceTexture_RainRipple (both are EShaderStage_Pixel)
	}

	return m_pDefaultPerInstanceResourceSet->Update(m_defaultPerInstanceResources);
}

bool CWaterStage::SetAndBuildPerPassResources(bool bOnInit, EPass passId)
{
	const CRenderView* pRenderView = RenderView();

	auto* pTiledLights = GetStdGraphicsPipeline().GetTiledLightVolumesStage();
	auto* pVolFogStage = GetStdGraphicsPipeline().GetVolumetricFogStage();
	auto* pRippleStage = GetStdGraphicsPipeline().GetWaterRipplesStage();

	auto& resources    = m_perPassResources    [passId];
	auto& pResourceSet = m_pPerPassResourceSets[passId];

	CD3D9Renderer* RESTRICT_POINTER pRenderer = gcpRendD3D;
	const int32 frameID = (pRenderView) ? pRenderView->GetFrameId() : gRenDev->GetRenderFrameID();

	// Samplers
	{
		// default material samplers
		auto materialSamplers = GetStdGraphicsPipeline().GetDefaultMaterialSamplers();
		for (int32 i = 0; i < materialSamplers.size(); ++i)
		{
			resources.SetSampler(EEfResSamplers(i), materialSamplers[i], EShaderStage_AllWithoutCompute);
		}

		// Hard-coded point samplers
		// NOTE: overwrite default material sampler to avoid the limitation of DXOrbis.
		resources.SetSampler(ePerPassSampler_Aniso16xWrap, m_aniso16xWrapSampler, EShaderStage_AllWithoutCompute);
		resources.SetSampler(ePerPassSampler_Aniso16xClamp, m_aniso16xClampSampler, EShaderStage_AllWithoutCompute);

		resources.SetSampler(ePerPassSampler_PointWrap, EDefaultSamplerStates::PointWrap, EShaderStage_AllWithoutCompute);
		resources.SetSampler(ePerPassSampler_PointClamp, EDefaultSamplerStates::PointClamp, EShaderStage_AllWithoutCompute);

		resources.SetSampler(ePerPassSampler_LinearClampComp, m_linearCompareClampSampler, EShaderStage_AllWithoutCompute);
		resources.SetSampler(ePerPassSampler_LinearMirror, m_linearMirrorSampler, EShaderStage_AllWithoutCompute);
	}

	CTexture* pVolFogShadowTex = CRendererResources::s_ptexBlack;
#if defined(VOLUMETRIC_FOG_SHADOWS)
	if (pRenderer->m_bVolFogShadowsEnabled)
	{
		pVolFogShadowTex = CRendererResources::s_ptexVolFogShadowBuf[0];
	}
#endif

	auto* pWaterNormalTex = m_bWaterNormalGen ? CRendererResources::s_ptexWaterVolumeDDN : CRendererResources::s_ptexFlatBump;

	// NOTE: update this resource set at every frame, otherwise have double resource sets.
	const int32 currWaterVolID = GetCurrentFrameID(frameID);
	const int32 prevWaterVolID = GetPreviousFrameID(frameID);
	CTexture* pCurrWaterVolRefl = CRendererResources::s_ptexWaterVolumeRefl[currWaterVolID];
	CTexture* pPrevWaterVolRefl = CRendererResources::s_ptexWaterVolumeRefl[prevWaterVolID];

	// NOTE: only water surface needs water reflection texture.
	const bool bWaterSurface = (passId == ePass_WaterSurface);
	auto* pWaterReflectionTex = bWaterSurface ? pCurrWaterVolRefl : CRendererResources::s_ptexBlack;

	// Textures
	{
		// volumetric fog shadow
		resources.SetTexture(ePerPassTexture_VolFogShadow, pVolFogShadowTex, EDefaultResourceViews::Default, EShaderStage_Pixel);

		// voxel-based volumetric fog
		resources.SetTexture(ePerPassTexture_VolumetricFog, pVolFogStage->GetVolumetricFogTex(), EDefaultResourceViews::Default, EShaderStage_Pixel);
		resources.SetTexture(ePerPassTexture_VolFogGlobalEnvProbe0, pVolFogStage->GetGlobalEnvProbeTex0(), EDefaultResourceViews::Default, EShaderStage_Pixel);
		resources.SetTexture(ePerPassTexture_VolFogGlobalEnvProbe1, pVolFogStage->GetGlobalEnvProbeTex1(), EDefaultResourceViews::Default, EShaderStage_Pixel);

		resources.SetTexture(ePerPassTexture_PerlinNoiseMap, CRendererResources::s_ptexPerlinNoiseMap, EDefaultResourceViews::Default, EShaderStage_Pixel | EShaderStage_Vertex | EShaderStage_Domain);
		resources.SetTexture(ePerPassTexture_Jitter, m_pJitterTex, EDefaultResourceViews::Default, EShaderStage_Pixel);

		resources.SetTexture(ePerPassTexture_WaterRipple, pRippleStage->GetWaterRippleTex(), EDefaultResourceViews::Default, EShaderStage_Vertex | EShaderStage_Pixel | EShaderStage_Domain);
		resources.SetTexture(ePerPassTexture_WaterNormal, pWaterNormalTex, EDefaultResourceViews::Default, EShaderStage_Vertex | EShaderStage_Pixel | EShaderStage_Domain);

		if (passId == ePass_FogVolume)
		{
			auto* pOceanMask = CTexture::IsTextureExist(m_pOceanMaskTex) ? m_pOceanMaskTex.get() : CRendererResources::s_ptexBlack;
			resources.SetTexture(ePerPassTexture_WaterGloss, pOceanMask, EDefaultResourceViews::Default, EShaderStage_Pixel);
		}
		else
		{
			resources.SetTexture(ePerPassTexture_WaterGloss, m_pWaterGlossTex, EDefaultResourceViews::Default, EShaderStage_Pixel);
		}

		if (passId == ePass_ReflectionGen)
		{
			resources.SetTexture(ePerPassTexture_Reflection, pPrevWaterVolRefl, EDefaultResourceViews::Default, EShaderStage_Pixel);
			resources.SetTexture(ePerPassTexture_Refraction, CRendererResources::s_ptexHDRTargetScaled[0], EDefaultResourceViews::Default, EShaderStage_Pixel);
			resources.SetTexture(ePerPassTexture_SceneDepth, CRendererResources::s_ptexLinearDepthScaled[0], EDefaultResourceViews::Default, EShaderStage_Pixel);
		}
		else
		{
			resources.SetTexture(ePerPassTexture_Reflection, pCurrWaterVolRefl, EDefaultResourceViews::Default, EShaderStage_Pixel);
			resources.SetTexture(ePerPassTexture_Refraction, CRendererResources::s_ptexSceneTarget, EDefaultResourceViews::Default, EShaderStage_Pixel);
			resources.SetTexture(ePerPassTexture_SceneDepth, CRendererResources::s_ptexLinearDepth, EDefaultResourceViews::Default, EShaderStage_Pixel);
		}

		// forward shadow textures.
		CShadowUtils::SShadowCascades cascades;
		if (bOnInit)
		{
			std::fill(std::begin(cascades.pShadowMap), std::end(cascades.pShadowMap), CRendererResources::s_ptexFarPlane);
		}
		else
		{
			CShadowUtils::GetShadowCascades(cascades, RenderView());
		}
		resources.SetTexture(ePerPassTexture_ShadowMap0, cascades.pShadowMap[0], EDefaultResourceViews::Default, EShaderStage_Pixel);
		resources.SetTexture(ePerPassTexture_ShadowMap1, cascades.pShadowMap[1], EDefaultResourceViews::Default, EShaderStage_Pixel);
		resources.SetTexture(ePerPassTexture_ShadowMap2, cascades.pShadowMap[2], EDefaultResourceViews::Default, EShaderStage_Pixel);
		resources.SetTexture(ePerPassTexture_ShadowMap3, cascades.pShadowMap[3], EDefaultResourceViews::Default, EShaderStage_Pixel);
	}

	// Tiled shading resources
	{
		// Commented out resources are currently unused. They'll be enabled when WaterVolume rendering is through the standard Forward+ shader-framework
		if (bOnInit)
		{
			resources.SetBuffer(17, CDeviceBufferManager::GetNullBufferTyped(), EDefaultResourceViews::Default, EShaderStage_AllWithoutCompute);
			resources.SetBuffer(18, CDeviceBufferManager::GetNullBufferStructured(), EDefaultResourceViews::Default, EShaderStage_AllWithoutCompute);
		//	resources.SetBuffer(19, CDeviceBufferManager::GetNullBufferStructured(), EDefaultResourceViews::Default, EShaderStage_AllWithoutCompute);
			resources.SetTexture(20, CRendererResources::s_ptexBlackCM, EDefaultResourceViews::Default, EShaderStage_AllWithoutCompute);
		//	resources.SetTexture(21, CRendererResources::s_ptexBlackCM, EDefaultResourceViews::Default, EShaderStage_AllWithoutCompute);
		//	resources.SetTexture(22, CRendererResources::s_ptexBlack, EDefaultResourceViews::Default, EShaderStage_AllWithoutCompute);

		//	if (bTransparentPass || (CRenderer::CV_r_DeferredShadingTiled < 3))
			{
				resources.SetBuffer(17, CDeviceBufferManager::GetNullBufferTyped(), EDefaultResourceViews::Default, EShaderStage_AllWithoutCompute);
			}
		}
		else
		{
			resources.SetBuffer(17, pTiledLights->GetTiledOpaqueLightMaskBuffer(), EDefaultResourceViews::Default, EShaderStage_AllWithoutCompute);
			resources.SetBuffer(18, pTiledLights->GetLightShadeInfoBuffer(), EDefaultResourceViews::Default, EShaderStage_AllWithoutCompute);
		//	resources.SetBuffer(19, pClipVolumes->GetClipVolumeInfoBuffer(), EDefaultResourceViews::Default, EShaderStage_AllWithoutCompute);
			resources.SetTexture(20, pTiledLights->GetSpecularProbeAtlas(), EDefaultResourceViews::Default, EShaderStage_AllWithoutCompute);
		//	resources.SetTexture(21, pTiledLights->GetDiffuseProbeAtlas(), EDefaultResourceViews::Default, EShaderStage_AllWithoutCompute);
		//	resources.SetTexture(22, pTiledLights->GetProjectedLightAtlas(), EDefaultResourceViews::Default, EShaderStage_AllWithoutCompute);

		//	if (bTransparentPass || (CRenderer::CV_r_DeferredShadingTiled < 3))
			{
				resources.SetBuffer(17, pTiledLights->GetTiledTranspLightMaskBuffer(), EDefaultResourceViews::Default, EShaderStage_AllWithoutCompute);
			}
		}
		resources.SetTexture(40, CRendererResources::s_ptexEnvironmentBRDF, EDefaultResourceViews::Default, EShaderStage_AllWithoutCompute);
	}

	// Constant buffers
	{
		// update constant buffer if needed.
		if (!bOnInit)
		{
			UpdatePerPassResources(passId);
		}

		resources.SetConstantBuffer(eConstantBufferShaderSlot_PerPass, m_pPerPassCB[passId], EShaderStage_AllWithoutCompute);

		CConstantBufferPtr pPerViewCB;
		if (bOnInit)  // Handle case when no view is available in the initialization of the stage
			pPerViewCB = CDeviceBufferManager::GetNullConstantBuffer();
		else
			pPerViewCB = GetStdGraphicsPipeline().GetMainViewConstantBuffer();

		resources.SetConstantBuffer(eConstantBufferShaderSlot_PerView, pPerViewCB, EShaderStage_AllWithoutCompute);
	}

	if (bOnInit)
		return true;

	CRY_ASSERT(!resources.HasChangedBindPoints()); // Cannot change resource layout after init. It is baked into the shaders
	return pResourceSet->Update(resources);
}

void CWaterStage::UpdatePerPassResources(EPass passId)
{
	CD3D9Renderer* RESTRICT_POINTER pRenderer = gcpRendD3D;
	SRenderViewShaderConstants& PF = RenderView()->GetShaderConstants();

	// update per pass constant buffer.
	{
		auto* pWaterRipplesStage = GetStdGraphicsPipeline().GetWaterRipplesStage();
		auto* pVolFogStage = GetStdGraphicsPipeline().GetVolumetricFogStage();
#if defined(VOLUMETRIC_FOG_SHADOWS)
		const bool bRenderFogShadow = pRenderer->m_bVolFogShadowsEnabled;
#else
		const bool bRenderFogShadow = false;
#endif
		N3DEngineCommon::SCausticInfo& causticInfo = pRenderer->m_p3DEngineCommon.m_CausticInfo;

		CRY_ASSERT(pWaterRipplesStage);
		CRY_ASSERT(pVolFogStage);

		CryStackAllocWithSize(water::SPerPassConstantBuffer, cb, CDeviceBufferManager::AlignBufferSizeForStreaming);

		// water related parameters
		auto& cbWater = cb->cbPerPassWater;
		cbWater.causticViewProjMatr = causticInfo.m_mCausticMatr;
		cbWater.waterRippleLookup = pWaterRipplesStage->GetWaterRippleLookupParam();
		cbWater.ssrParams = Vec4(
			false ? 2.0f : 1.0f,
			false ? 2.0f : 1.0f,
			CRenderer::CV_r_SSReflDistance,
			CRenderer::CV_r_SSReflSamples * 1.0f);

		// fog
		auto& cbFog = cb->cbPerPassFog;
		cbFog.vfParams = PF.pVolumetricFogParams;          // PB_VolumetricFogParams
		cbFog.vfRampParams = PF.pVolumetricFogRampParams;  // PB_VolumetricFogRampParams
		cbFog.vfSunDir = PF.pVolumetricFogSunDir;          // PB_VolumetricFogSunDir
		cbFog.vfColGradBase = Vec3(PF.pFogColGradColBase); // PB_FogColGradColBase
		cbFog.padding0 = 0.0f;
		cbFog.vfColGradDelta = Vec3(PF.pFogColGradColDelta); // PB_FogColGradColDelta
		cbFog.padding1 = 0.0f;
		cbFog.vfColGradParams = PF.pFogColGradParams; // PB_FogColGradParams
		cbFog.vfColGradRadial = PF.pFogColGradRadial; // PB_FogColGradRadial

		// volumetric fog shadow
		auto& cbVolFogShadow = cb->cbVolFogShadow;
#if defined(VOLUMETRIC_FOG_SHADOWS)
		if (bRenderFogShadow)
		{
			Vec3 volFogShadowDarkeningP;
			gEnv->p3DEngine->GetGlobalParameter(E3DPARAM_VOLFOG_SHADOW_DARKENING, volFogShadowDarkeningP);
			cbVolFogShadow.volFogShadowDarkening = Vec4(volFogShadowDarkeningP, 0.0f);

			const float aSun = (1.0f - clamp_tpl(volFogShadowDarkeningP.y, 0.0f, 1.0f)) * 1.0f;
			const float bSun = 1.0f - aSun;
			const float aAmb = (1.0f - clamp_tpl(volFogShadowDarkeningP.z, 0.0f, 1.0f)) * 0.4f;
			const float bAmb = 1.0f - aAmb;
			cbVolFogShadow.volFogShadowDarkeningSunAmb = Vec4(aSun, bSun, aAmb, bAmb);
		}
		else
		{
			cbVolFogShadow.volFogShadowDarkening = Vec4(0.0f);
			cbVolFogShadow.volFogShadowDarkeningSunAmb = Vec4(0.0f);
		}
#else
		cbVolFogShadow.volFogShadowDarkening = Vec4(0.0f);
		cbVolFogShadow.volFogShadowDarkeningSunAmb = Vec4(0.0f);
#endif

		// voxel-based volumetric fog
		auto& cbVolFog = cb->cbVoxVolFog;
		cbVolFog.vfSamplingParams = PF.pVolumetricFogSamplingParams;                      // PB_VolumetricFogSamplingParams
		cbVolFog.vfDistributionParams = PF.pVolumetricFogDistributionParams;              // PB_VolumetricFogDistributionParams
		cbVolFog.vfScatteringParams = PF.pVolumetricFogScatteringParams;                  // PB_VolumetricFogScatteringParams
		cbVolFog.vfScatteringBlendParams = PF.pVolumetricFogScatteringBlendParams;        // PB_VolumetricFogScatteringBlendParams
		cbVolFog.vfScatteringColor = PF.pVolumetricFogScatteringColor;                    // PB_VolumetricFogScatteringColor
		cbVolFog.vfScatteringSecondaryColor = PF.pVolumetricFogScatteringSecondaryColor;  // PB_VolumetricFogScatteringSecondaryColor
		cbVolFog.vfHeightDensityParams = PF.pVolumetricFogHeightDensityParams;            // PB_VolumetricFogHeightDensityParams
		cbVolFog.vfHeightDensityRampParams = PF.pVolumetricFogHeightDensityRampParams;    // PB_VolumetricFogHeightDensityRampParams
		cbVolFog.vfDistanceParams = PF.pVolumetricFogDistanceParams;                      // PB_VolumetricFogDistanceParams
		cbVolFog.vfGlobalEnvProbeParams0 = pVolFogStage->GetGlobalEnvProbeShaderParam0(); // PB_VolumetricFogGlobalEnvProbe0
		cbVolFog.vfGlobalEnvProbeParams1 = pVolFogStage->GetGlobalEnvProbeShaderParam1(); // PB_VolumetricFogGlobalEnvProbe1

		// forward shadow sampling parameters.
		CShadowUtils::SShadowCascadesSamplingInfo shadowSamplingInfo;
		CShadowUtils::GetShadowCascadesSamplingInfo(shadowSamplingInfo, RenderView());
		cb->cbShadowSampling = shadowSamplingInfo;

		CRY_ASSERT(m_pPerPassCB[passId]);
		m_pPerPassCB[passId]->UpdateBuffer(cb, cbSize);
	}
}

void CWaterStage::ExecuteWaterNormalGen()
{
	PROFILE_LABEL_SCOPE("WATER_NORMAL_GEN");

	CRY_ASSERT(RenderView()->GetCurrentEye() == CCamera::eEye_Left);

	const int32 nGridSize = 64;

	const int32 frameID = SPostEffectsUtils::m_iFrameCounter % 2;

	// Create texture if required
	CTexture* pTexture = CRendererResources::s_ptexWaterVolumeTemp[frameID];
	if (!CTexture::IsTextureExist(pTexture))
	{
		if (!pTexture->Create2DTexture(nGridSize, nGridSize, 1, FT_DONT_RELEASE | FT_NOMIPS, nullptr, eTF_R32G32B32A32F))
		{
			return;
		}
	}

	// spawn water simulation job for next frame.
	{
		uint64 nCurFrameID = RenderView()->GetFrameId();
		if (m_frameIdWaterSim != nCurFrameID)
		{
			Vec4 pCurrParams0, pCurrParams1;
			gEnv->p3DEngine->GetOceanAnimationParams(pCurrParams0, pCurrParams1);

			// TODO: this water sim is shared with CREWaterOcean and CWaterStage. the sim result also could be shared.
			// Update sim settings
			auto& pParams0 = m_oceanAnimationParams[0];
			auto& pParams1 = m_oceanAnimationParams[1];
			if (WaterSimMgr()->NeedInit() || pCurrParams0.x != pParams0.x || pCurrParams0.y != pParams0.y ||
			    pCurrParams0.z != pParams0.z || pCurrParams0.w != pParams0.w || pCurrParams1.x != pParams1.x ||
			    pCurrParams1.y != pParams1.y || pCurrParams1.z != pParams1.z || pCurrParams1.w != pParams1.w)
			{
				pParams0 = pCurrParams0;
				pParams1 = pCurrParams1;
				WaterSimMgr()->Create(1.0, pParams0.x, 1.0f, 1.0f);
			}

			// Copy data..
			if (CTexture::IsTextureExist(pTexture))
			{
				//const float fUpdateTime = 2.f*0.125f*gEnv->pTimer->GetCurrTime();
				const float fUpdateTime = 0.125f * gEnv->pTimer->GetCurrTime();// / clamp_tpl<float>(pParams1.x, 0.55f, 1.0f);

				void* pRawPtr = nullptr;
				WaterSimMgr()->Update((int)nCurFrameID, fUpdateTime, true, pRawPtr);

				Vec4* pDispGrid = WaterSimMgr()->GetDisplaceGrid();

				const uint32 pitch = 4 * sizeof(f32) * nGridSize;
				const uint32 width = nGridSize;
				const uint32 height = nGridSize;

				CRY_PROFILE_REGION_WAITING(PROFILE_RENDERER, "CWaterStage: update subresource");

				CDeviceTexture * pDevTex = pTexture->GetDevTexture();
				pDevTex->UploadFromStagingResource(0, [=](void* pData, uint32 rowPitch, uint32 slicePitch)
				{
					cryMemcpy(pData, pDispGrid, 4 * width * height * sizeof(f32));
					return true;
				});
			}
			m_frameIdWaterSim = nCurFrameID;
		}
	}

	// generate water normal from the simulation result.
	{
		auto& pass = m_passWaterNormalGen;

		if (pass.IsDirty())
		{
			static CCryNameTSCRC techName("WaterVolumesNormalGen");
			pass.SetPrimitiveFlags(CRenderPrimitive::eFlags_ReflectShaderConstants_PS);
			pass.SetPrimitiveType(CRenderPrimitive::ePrim_ProceduralTriangle);
			pass.SetTechnique(CShaderMan::s_shPostEffectsGame, techName, 0);
			pass.SetRenderTarget(0, CRendererResources::s_ptexWaterVolumeDDN);
			pass.SetState(GS_NODEPTHTEST);
			pass.SetTextureSamplerPair(0, pTexture, EDefaultSamplerStates::BilinearWrap);
		}

		pass.BeginConstantUpdate();

		static CCryNameR pParamName("waterVolumesParams");
		Vec4 vParams = Vec4(64.0f, 64.0f, 64.0f, 64.0f);
		pass.SetConstant(pParamName, vParams);

		pass.Execute();
	}

	// generate mipmap.
	m_passWaterNormalMipmapGen.Execute(CRendererResources::s_ptexWaterVolumeDDN);
}

void CWaterStage::ExecuteOceanMaskGen()
{
	N3DEngineCommon::SOceanInfo& OceanInfo = gcpRendD3D->m_p3DEngineCommon.m_OceanInfo;
	const bool bOceanVolumeVisible = (OceanInfo.m_nOceanRenderFlags & OCR_OCEANVOLUME_VISIBLE) != 0;

	if (!m_bOceanMaskGen || !bOceanVolumeVisible)
	{
		return;
	}

	PROFILE_LABEL_SCOPE("OCEAN_MASK_GEN");

	// prepare per pass device resource
	// this update must be called after water ripple stage's execute() because waterRippleLookup parameter depends on it.
	// this update must be called after updating N3DEngineCommon::SCausticInfo.
	SetAndBuildPerPassResources(false, ePass_OceanMaskGen);

	CClearSurfacePass::Execute(m_pOceanMaskTex, Clr_Transparent);

	// render ocean surface to generate the mask to identify the pixels where ocean is behind the others and invisible.
	ExecuteSceneRenderPass(m_passOceanMaskGen, EFSLIST_WATER);
}

void CWaterStage::ExecuteWaterVolumeCausticsGen(N3DEngineCommon::SCausticInfo& causticInfo)
{
	CRenderView* pRenderView = RenderView();

	CRY_ASSERT(pRenderView);
	CRY_ASSERT(pRenderView->GetCurrentEye() == CCamera::eEye_Left);

	PROFILE_LABEL_SCOPE("WATER_VOLUME_CAUSTICS_GEN");

	const auto& viewInfo = GetCurrentViewInfo();
	// update view-projection matrix to render water volumes to caustics gen texture.
	{
		// NOTE: caustics gen texture is generated only once if stereo rendering is activated.
		Vec3 vDir = -viewInfo.cameraVZ;
		Vec3 vPos = viewInfo.cameraOrigin;

		const float fMaxDistance = CRenderer::CV_r_watervolumecausticsmaxdistance;
		const float fOffsetDist = fMaxDistance * 0.25f;

		// Offset in viewing direction to maximize view distance.
		vPos += Vec3(vDir.x * fOffsetDist, vDir.y * fOffsetDist, 0.0f);

		// Snap to avoid some aliasing.
		const float fSnapRange = CRenderer::CV_r_watervolumecausticssnapfactor;
		if (fSnapRange > 0.05f) // don't bother snapping if the value is low.
		{
			water::GetSnapVector(vPos, fSnapRange);
		}

		Vec3 vEye = vPos + Vec3(0.0f, 0.0f, 10.0f);

		// Create the matrices.
		Matrix44A mOrthoMatr;
		Matrix44A mViewMatr;
		mOrthoMatr.SetIdentity();
		mViewMatr.SetIdentity();
		mathMatrixOrtho(&mOrthoMatr, fMaxDistance, fMaxDistance, 0.25f, 100.0f);
		mathMatrixLookAt(&mViewMatr, vEye, vPos, Vec3(0.0f, 1.0f, 0.0f));

		// Store for projection onto the scene.
		causticInfo.m_mCausticMatr = mViewMatr * mOrthoMatr;
		causticInfo.m_mCausticMatr.Transpose();
	}

	// prepare per pass device resource
	// this update must be called after water ripple stage's execute() because waterRippleLookup parameter depends on it.
	// this update must be called after updating N3DEngineCommon::SCausticInfo.
	SetAndBuildPerPassResources(false, ePass_CausticsGen);

	CClearSurfacePass::Execute(CRendererResources::s_ptexWaterCaustics[0], Clr_Transparent);

	// render water volumes to caustics gen texture.
	{
		const bool bReverseDepth = true;

		const auto renderList = EFSLIST_WATER;
		auto& pass = m_passWaterCausticsSrcGen;
		float fWidth  = static_cast<float>(CRendererResources::s_ptexWaterCaustics[0]->GetWidth());
		float fHeight = static_cast<float>(CRendererResources::s_ptexWaterCaustics[0]->GetHeight());

		// need to set render target becuase CRendererResources::s_ptexWaterCaustics[0] is recreated when cvars are changed.
		pass.ExchangeRenderTarget(0, CRendererResources::s_ptexWaterCaustics[0]);

		D3DViewPort viewport = { 0.0f, 0.0f, fWidth, fHeight, 0.0f, 1.0f };
		pass.SetViewport(viewport);

		CSceneRenderPass::EPassFlags passFlags = CSceneRenderPass::ePassFlags_None;
		passFlags |= bReverseDepth ? CSceneRenderPass::ePassFlags_ReverseDepth : CSceneRenderPass::ePassFlags_None;
		pass.SetFlags(passFlags);

		auto& RESTRICT_REFERENCE commandList = GetDeviceObjectFactory().GetCoreCommandList();
		pass.PrepareRenderPassForUse(commandList);

		auto& renderItemDrawer = pRenderView->GetDrawer();
		renderItemDrawer.InitDrawSubmission();

		pass.BeginExecution();
		pass.DrawRenderItems(pRenderView, renderList);
		pass.EndExecution();

		renderItemDrawer.JobifyDrawSubmission();

		renderItemDrawer.WaitForDrawSubmission();
	}

	// Dilate caustics gen texture.
	{
		PROFILE_LABEL_SCOPE("DILATION");

		auto& pass = m_passWaterCausticsDilation;

		if (pass.IsDirty())
		{
			static CCryNameTSCRC techName("WaterCausticsInfoDilate");
			pass.SetPrimitiveFlags(CRenderPrimitive::eFlags_None);
			pass.SetPrimitiveType(CRenderPrimitive::ePrim_ProceduralTriangle);
			pass.SetTechnique(CShaderMan::s_ShaderDeferredCaustics, techName, 0);
			pass.SetRenderTarget(0, CRendererResources::s_ptexWaterCaustics[1]);
			pass.SetState(GS_NODEPTHTEST);
			pass.SetTextureSamplerPair(0, CRendererResources::s_ptexWaterCaustics[0], EDefaultSamplerStates::PointClamp);
		}

		pass.Execute();
	}

	// Super blur for alpha to mask edges of volumes.
	m_passBlurWaterCausticsGen0.Execute(CRendererResources::s_ptexWaterCaustics[1], CRendererResources::s_ptexWaterCaustics[0], 1.0f, 10.0f, true);

	////////////////////////////////////////////////
	// Procedural caustic generation
	// Generate the caustics map using the grid mesh.
	// for future:
	// - merge this somehow with shadow gen for correct projection/intersection with geometry (and lighting) - can use shadow map for position reconstruction of world around volume and project caustic geometry to it.
	// - try hardware tessellation to increase quality and reduce memory (perhaps do projection per volume instead of as a single pass, that way it's basically screen-space)
	bool bVertexUpdated = false;
	if (UpdateCausticsGrid(causticInfo, bVertexUpdated, gRenDev)) // returns true if the mesh is valid
	{
		{
			PROFILE_LABEL_SCOPE("CAUSTICS_GEN");

			auto& pass = m_passRenderCausticsGrid;
			auto& prim = m_causticsGridPrimitive;

			if (bVertexUpdated || prim.IsDirty())
			{
				auto* pTargetTex = CRendererResources::s_ptexWaterCaustics[0];
				D3DViewPort viewport;

				viewport.TopLeftX = 0.0f;
				viewport.TopLeftY = 0.0f;
				viewport.Width  = static_cast<float>(pTargetTex->GetWidth());
				viewport.Height = static_cast<float>(pTargetTex->GetHeight());
				viewport.MinDepth = 0.0f;
				viewport.MaxDepth = 1.0f;

				pass.SetRenderTarget(0, pTargetTex);
				pass.SetViewport(viewport);
				pass.BeginAddingPrimitives();

				CRenderMesh* pCausticQuadMesh = static_cast<CRenderMesh*>(causticInfo.m_pCausticQuadMesh.get());
				pCausticQuadMesh->RT_CheckUpdate(pCausticQuadMesh->_GetVertexContainer(),pCausticQuadMesh->_GetVertexFormat(), 0);
				buffer_handle_t hVertexStream = pCausticQuadMesh->_GetVBStream(VSF_GENERAL);
				buffer_handle_t hIndexStream = pCausticQuadMesh->_GetIBStream();

				if (hVertexStream != ~0u && hIndexStream != ~0u)
				{
					prim.SetFlags(CRenderPrimitive::eFlags_None);
					prim.SetCullMode(eCULL_None);
					prim.SetRenderState(GS_NODEPTHTEST | GS_NOCOLMASK_RGA);

					prim.SetCustomVertexStream(hVertexStream, pCausticQuadMesh->_GetVertexFormat(), pCausticQuadMesh->GetStreamStride(VSF_GENERAL));
					prim.SetCustomIndexStream(hIndexStream, (sizeof(vtx_idx) == 2 ? RenderIndexType::Index16 : RenderIndexType::Index32));
					prim.SetDrawInfo(eptTriangleList, 0, 0, causticInfo.m_nIndexCount);

					static CCryNameTSCRC techName("WaterCausticsGen");
					prim.SetTechnique(CShaderMan::s_ShaderDeferredCaustics, techName, 0);

					prim.SetTexture(0, CRendererResources::s_ptexWaterCaustics[1], EDefaultResourceViews::Default, EShaderStage_Vertex);
					prim.SetSampler(0, EDefaultSamplerStates::TrilinearWrap, EShaderStage_Vertex);

					prim.SetInlineConstantBuffer(eConstantBufferShaderSlot_PerView, GetStdGraphicsPipeline().GetMainViewConstantBuffer(), EShaderStage_Vertex);
					prim.Compile(pass);
					
					pass.AddPrimitive(&prim);
				}
			}

			pass.Execute();
		}

		// NOTE: this is needed to avoid a broken consistency of the cached state in SSharedState,
		//       because d3d11 runtime removes SRV (s_ptexWaterCaustics[0]) from pixel shader slot 0 but it still remains in the cached state.
		GetDeviceObjectFactory().GetCoreCommandList().Reset();

		// Smooth out any inconsistencies in the caustic map (pixels, etc).
		m_passBlurWaterCausticsGen1.Execute(CRendererResources::s_ptexWaterCaustics[0], CRendererResources::s_ptexWaterCaustics[1], 1.0f, 1.0f);
	}
}

void CWaterStage::ExecuteReflection()
{
	CRenderView* pRenderView = RenderView();
	CRY_ASSERT(pRenderView);

	CD3D9Renderer* const RESTRICT_POINTER rd = gcpRendD3D;

	const bool bReverseDepth = true;
	const int32 frameID = pRenderView->GetFrameId();

	const auto renderList = EFSLIST_WATER;
	const uint32 batchMask = pRenderView->GetBatchFlags(renderList);

	if ((batchMask & FB_WATER_REFL)
	    && CTexture::IsTextureExist(CRendererResources::s_ptexWaterVolumeRefl[0]))
	{
		PROFILE_LABEL_SCOPE("WATER_VOLUME_REFLECTION_GEN");

		const int32 currWaterVolID = GetCurrentFrameID(frameID);
		CTexture* pCurrWaterVolRefl = CRendererResources::s_ptexWaterVolumeRefl[currWaterVolID];

		// TODO: Is m_CurDownscaleFactor needed for new graphics pipeline?
		const auto& downscaleFactor = gRenDev->GetRenderQuality().downscaleFactor;
		const int32 nWidth = int32(pCurrWaterVolRefl->GetWidth() * (downscaleFactor.x));
		const int32 nHeight = int32(pCurrWaterVolRefl->GetHeight() * (downscaleFactor.y));

		// TODO: looks something wrong between rect and viewport?
		const RECT rect = { 0, pCurrWaterVolRefl->GetHeight() - nHeight, nWidth, nHeight };
		D3DViewPort viewport = { 0.0f, float(pCurrWaterVolRefl->GetHeight() - nHeight), float(nWidth), float(nHeight), 0.0f, 1.0f };

		m_passCopySceneTargetReflection.Execute(CRendererResources::s_ptexSceneTarget, CRendererResources::s_ptexHDRTargetScaled[0]);
		m_passWaterReflectionClear.Execute(pCurrWaterVolRefl, Clr_Transparent, 1, &rect);

		// draw render items to generate water reflection texture.
		{
			auto& pass = m_passWaterReflectionGen;

			// alternates render targets along with updating per pass resource set.
			pass.ExchangeRenderTarget(0, pCurrWaterVolRefl);

			CSceneRenderPass::EPassFlags passFlags = CSceneRenderPass::ePassFlags_None;
			passFlags |= bReverseDepth ? CSceneRenderPass::ePassFlags_ReverseDepth : CSceneRenderPass::ePassFlags_None;
			pass.SetFlags(passFlags);
			pass.SetViewport(viewport);

			auto& RESTRICT_REFERENCE commandList = GetDeviceObjectFactory().GetCoreCommandList();
			pass.PrepareRenderPassForUse(commandList);

			auto& renderItemDrawer = pRenderView->GetDrawer();
			renderItemDrawer.InitDrawSubmission();

			pass.BeginExecution();
			pass.DrawRenderItems(pRenderView, renderList);
			pass.EndExecution();

			renderItemDrawer.JobifyDrawSubmission();
			renderItemDrawer.WaitForDrawSubmission();
		}

		m_passWaterReflectionMipmapGen.Execute(pCurrWaterVolRefl);
	}
}

void CWaterStage::ExecuteSceneRenderPass(CSceneRenderPass& pass, ERenderListID renderList)
{
	CRenderView* pRenderView = RenderView();
	const bool bReverseDepth = true;

	pass.SetViewport(GetViewport());

	CSceneRenderPass::EPassFlags passFlags = CSceneRenderPass::ePassFlags_None;
	passFlags |= bReverseDepth ? CSceneRenderPass::ePassFlags_ReverseDepth : CSceneRenderPass::ePassFlags_None;
	pass.SetFlags(passFlags);

	auto& RESTRICT_REFERENCE commandList = GetDeviceObjectFactory().GetCoreCommandList();
	pass.PrepareRenderPassForUse(commandList);

	auto& renderItemDrawer = pRenderView->GetDrawer();
	renderItemDrawer.InitDrawSubmission();

	pass.BeginExecution();
	pass.DrawRenderItems(pRenderView, renderList);
	pass.EndExecution();

	renderItemDrawer.JobifyDrawSubmission();
	renderItemDrawer.WaitForDrawSubmission();
}

int32 CWaterStage::GetCurrentFrameID(const int32 frameID) const
{
	return (frameID % 2);
}

int32 CWaterStage::GetPreviousFrameID(const int32 frameID) const
{
	return (frameID + 1) % 2;
}
