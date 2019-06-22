// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "VolumetricFog.h"

#include "D3DPostProcess.h"
#include "GraphicsPipeline/ClipVolumes.h"
#include "GraphicsPipeline/TiledLightVolumes.h"
#include "GraphicsPipeline/TiledShading.h"
#include "GraphicsPipeline/SceneGBuffer.h"
#include "GraphicsPipeline/ShadowMap.h"
#include "Common/ReverseDepth.h"

//////////////////////////////////////////////////////////////////////////

namespace
{
static const float ThresholdLengthGlobalProbe = 1.732f * (1000.0f * 0.5f);

// TODO: subset of STiledLightInfo
struct SFogVolumeCullInfo
{
	Vec4 posRad;
	Vec4 volumeParams0;
	Vec4 volumeParams1;
	Vec4 volumeParams2;
};

struct SFogVolumeInjectInfo
{
	uint32   miscFlag; // 1: volumeType, 8: stencilRef, 1: affectThisAreaOnly, 22: reserved
	Vec3     fogColor;

	float    globalDensity;
	Vec3     fogVolumePos;

	Vec3     heightFalloffBasePoint;
	float    invSoftEdgeLerp;

	Vec3     heightFallOffDirScaled;
	float    densityOffset;

	Vec4     rampParams;

	Vec3     windOffset;
	float    noiseElapsedTime;

	Vec3     noiseSpatialFrequency;
	float    noiseScale;

	Vec3     eyePosInOS;
	float    noiseOffset;

	Vec3     emission;
	float    padding;

	Matrix34 worldToObjMatrix;
};

struct SVolFogSceneRenderPassConstantBuffer
{
	CVolumetricFogStage::SForwardParams cbVolFogInj;
};

uint32 vfGetDownscaledShadowMapSize(uint32& shadowMapTempSize, const ICVar* pCVarShadowMaxTexRes)
{
	uint32 shadowMapSize = 0;

	uint32 tempSize = 1024; // REGISTER_CVAR(e_ShadowsMaxTexRes, 1024)
	if (pCVarShadowMaxTexRes)
	{
		tempSize = max(pCVarShadowMaxTexRes->GetIVal(), 32);//this restriction is same as it in CD3D9Renderer::PrepareShadowGenForFrustum.
	}

	switch (CRendererCVars::CV_r_VolumetricFogDownscaledSunShadowRatio)
	{
	case 0:
		tempSize /= 4;
		shadowMapSize = tempSize;
		break;
	case 1:
		tempSize /= 4;
		shadowMapSize = tempSize / 2;
		break;
	case 2:
		tempSize /= 4;
		shadowMapSize = tempSize / 4;
		break;
	default:
		tempSize /= 4;
		shadowMapSize = tempSize / 4;
		break;
	}

	shadowMapTempSize = tempSize;
	return shadowMapSize;
}

int32 vfGetShadowCascadeNum()
{
	if (CRendererCVars::CV_r_VolumetricFogDownscaledSunShadow <= 0)
	{
		return 0;
	}

	int32 num = (CRendererCVars::CV_r_VolumetricFogDownscaledSunShadow == 1) ?
	            (CVolumetricFogStage::ShadowCascadeNum - 1)
	            : CVolumetricFogStage::ShadowCascadeNum;

	return num;
}

AABB vfRotateAABB(const AABB& aabb, const Matrix33& mat)
{
	Matrix33 matAbs;
	matAbs.m00 = fabs_tpl(mat.m00);
	matAbs.m01 = fabs_tpl(mat.m01);
	matAbs.m02 = fabs_tpl(mat.m02);
	matAbs.m10 = fabs_tpl(mat.m10);
	matAbs.m11 = fabs_tpl(mat.m11);
	matAbs.m12 = fabs_tpl(mat.m12);
	matAbs.m20 = fabs_tpl(mat.m20);
	matAbs.m21 = fabs_tpl(mat.m21);
	matAbs.m22 = fabs_tpl(mat.m22);

	Vec3 sz = ((aabb.max - aabb.min) * 0.5f) * matAbs;
	Vec3 pos = ((aabb.max + aabb.min) * 0.5f) * mat;

	return AABB(pos - sz, pos + sz);
}
}

//////////////////////////////////////////////////////////////////////////

bool CVolumetricFogStage::IsEnabledInFrame()
{
	bool v = CRendererCVars::CV_r_DeferredShadingTiled > CTiledShadingStage::eDeferredMode_Off
	         && CRendererCVars::CV_r_DeferredShadingTiledDebug != 2
	         && CRendererCVars::CV_r_UseZPass != CSceneGBufferStage::eZPassMode_Off
	         && CRendererCVars::CV_r_measureoverdraw == 0;

	return v;
}

CVolumetricFogStage::CVolumetricFogStage(CGraphicsPipeline& graphicsPipeline)
	: CGraphicsPipelineStage(graphicsPipeline)
	, m_pVolFogBufDensityColor(nullptr)
	, m_pVolFogBufDensity(nullptr)
	, m_pVolFogBufEmissive(nullptr)
	, m_pInscatteringVolume(nullptr)
	, m_pMaxDepth(nullptr)
	, m_pMaxDepthTemp(nullptr)
	, m_pNoiseTexture(nullptr)
	, m_pDownscaledShadowTemp(nullptr)
	, m_globalEnvProveTex0(nullptr)
	, m_globalEnvProveTex1(nullptr)
	, m_passBuildLightListGrid(&graphicsPipeline, CComputeRenderPass::eFlags_ReflectConstantBuffersFromShader)
	, m_passDownscaleDepthHorizontal(&graphicsPipeline, CComputeRenderPass::eFlags_ReflectConstantBuffersFromShader)
	, m_passDownscaleDepthVertical(&graphicsPipeline, CComputeRenderPass::eFlags_ReflectConstantBuffersFromShader)
	, m_passInjectFogDensity(&graphicsPipeline, CComputeRenderPass::eFlags_ReflectConstantBuffersFromShader)
	, m_passInjectInscattering(&graphicsPipeline, CComputeRenderPass::eFlags_ReflectConstantBuffersFromShader)
	, m_globalEnvProbeParam0(0.0f)
	, m_globalEnvProbeParam1(0.0f)
	, m_sceneRenderPassResources()
	, m_cleared(MaxFrameNum)
{
	static_assert(MaxFrameNum >= MAX_GPU_NUM, "MaxFrameNum must be more than or equal to MAX_GPU_NUM.");

	for (auto& pass : m_passBlurDensityHorizontal)
		pass.SetFlags(CComputeRenderPass::eFlags_ReflectConstantBuffersFromShader);
	for (auto& pass : m_passBlurDensityVertical)
		pass.SetFlags(CComputeRenderPass::eFlags_ReflectConstantBuffersFromShader);
	for (auto& pass : m_passBlurInscatteringHorizontal)
		pass.SetFlags(CComputeRenderPass::eFlags_ReflectConstantBuffersFromShader);
	for (auto& pass : m_passBlurInscatteringVertical)
		pass.SetFlags(CComputeRenderPass::eFlags_ReflectConstantBuffersFromShader);
	for (auto& pass : m_passTemporalReprojection)
		pass.SetFlags(CComputeRenderPass::eFlags_ReflectConstantBuffersFromShader);
	for (auto& pass : m_passRaymarch)
		pass.SetFlags(CComputeRenderPass::eFlags_ReflectConstantBuffersFromShader);
	for (auto& pass : m_passDownscaleShadowmap)
		pass.SetGraphicsPipeline(&graphicsPipeline);
	for (auto& pass : m_passDownscaleShadowmap2)
		pass.SetGraphicsPipeline(&graphicsPipeline);
	for (auto& pass : m_passBlurDensityHorizontal)
		pass.SetGraphicsPipeline(&graphicsPipeline);
	for (auto& pass : m_passBlurDensityVertical)
		pass.SetGraphicsPipeline(&graphicsPipeline);
	for (auto& pass : m_passBlurInscatteringHorizontal)
		pass.SetGraphicsPipeline(&graphicsPipeline);
	for (auto& pass : m_passBlurInscatteringVertical)
		pass.SetGraphicsPipeline(&graphicsPipeline);
	for (auto& pass : m_passTemporalReprojection)
		pass.SetGraphicsPipeline(&graphicsPipeline);
	for (auto& pass : m_passRaymarch)
		pass.SetGraphicsPipeline(&graphicsPipeline);

	std::fill(std::begin(m_pFogInscatteringVolume), std::end(m_pFogInscatteringVolume), nullptr);
	std::fill(std::begin(m_pFogDensityVolume), std::end(m_pFogDensityVolume), nullptr);
	std::fill(std::begin(m_pDownscaledShadow), std::end(m_pDownscaledShadow), nullptr);

	for (auto& mat : m_viewProj)
	{
		mat.SetIdentity();
	}
}

CVolumetricFogStage::~CVolumetricFogStage()
{
}

void CVolumetricFogStage::Init()
{
	if (!m_lightCullInfoBuf.IsAvailable())
	{
		const DXGI_FORMAT format = DXGI_FORMAT_UNKNOWN;
		const uint32 stride = sizeof(CTiledLightVolumesStage::STiledLightCullInfo);
		m_lightCullInfoBuf.Create(MaxNumTileLights, stride, format, CDeviceObjectFactory::USAGE_CPU_WRITE | CDeviceObjectFactory::USAGE_STRUCTURED | CDeviceObjectFactory::BIND_SHADER_RESOURCE, nullptr);
	}

	if (!m_LightShadeInfoBuf.IsAvailable())
	{
		const uint32 stride = sizeof(CTiledLightVolumesStage::STiledLightShadeInfo);
		const DXGI_FORMAT format = DXGI_FORMAT_UNKNOWN;
		m_LightShadeInfoBuf.Create(MaxNumTileLights, stride, format, CDeviceObjectFactory::USAGE_CPU_WRITE | CDeviceObjectFactory::USAGE_STRUCTURED | CDeviceObjectFactory::BIND_SHADER_RESOURCE, nullptr);
	}

	if (!m_fogVolumeCullInfoBuf.IsAvailable())
	{
		const uint32 stride = sizeof(SFogVolumeCullInfo);
		const DXGI_FORMAT format = DXGI_FORMAT_UNKNOWN;
		m_fogVolumeCullInfoBuf.Create(CRenderView::MaxFogVolumeNum, stride, format, CDeviceObjectFactory::USAGE_CPU_WRITE | CDeviceObjectFactory::USAGE_STRUCTURED | CDeviceObjectFactory::BIND_SHADER_RESOURCE, nullptr);
	}

	if (!m_fogVolumeInjectInfoBuf.IsAvailable())
	{
		const uint32 stride = sizeof(SFogVolumeInjectInfo);
		const DXGI_FORMAT format = DXGI_FORMAT_UNKNOWN;
		m_fogVolumeInjectInfoBuf.Create(CRenderView::MaxFogVolumeNum, stride, format, CDeviceObjectFactory::USAGE_CPU_WRITE | CDeviceObjectFactory::USAGE_STRUCTURED | CDeviceObjectFactory::BIND_SHADER_RESOURCE, nullptr);
	}

	const uint32 commonFlags = FT_NOMIPS | FT_DONT_STREAM;
	const uint32 uavFlags = commonFlags | FT_USAGE_UNORDERED_ACCESS;
	const uint32 uavRtFlags = commonFlags | FT_USAGE_UNORDERED_ACCESS | FT_USAGE_RENDERTARGET;
	const uint32 dsFlags = commonFlags | FT_USAGE_DEPTHSTENCIL;
	const ETEX_Format fmtDensityColor = eTF_R11G11B10F;
	const ETEX_Format fmtDensity = eTF_R16F;
	const ETEX_Format fmtEmissive = eTF_R11G11B10F;

	CRY_ASSERT(m_pVolFogBufDensityColor == nullptr);
	std::string texName = "$VolFogBufDensityColor" + m_graphicsPipeline.GetUniqueIdentifierName();
	m_pVolFogBufDensityColor = CTexture::GetOrCreateTextureObjectPtr(texName.c_str(), 0, 0, 0, eTT_3D, uavRtFlags, fmtDensityColor);

	CRY_ASSERT(m_pVolFogBufDensity == nullptr);
	texName = "$VolFogBufDensity" + m_graphicsPipeline.GetUniqueIdentifierName();
	m_pVolFogBufDensity = CTexture::GetOrCreateTextureObjectPtr(texName.c_str(), 0, 0, 0, eTT_3D, uavRtFlags, fmtDensity);

	CRY_ASSERT(m_pVolFogBufEmissive == nullptr);
	texName = "$VolFogBufEmissive" + m_graphicsPipeline.GetUniqueIdentifierName();
	m_pVolFogBufEmissive = CTexture::GetOrCreateTextureObjectPtr(texName.c_str(), 0, 0, 0, eTT_3D, uavRtFlags, fmtEmissive);

	CRY_ASSERT(m_pInscatteringVolume == nullptr);
	texName = "$VolFogBufInscattering" + m_graphicsPipeline.GetUniqueIdentifierName();
	m_pInscatteringVolume = CTexture::GetOrCreateTextureObjectPtr(texName.c_str(), 0, 0, 0, eTT_3D, uavFlags, eTF_Unknown);

	{
		for (int32 i = 0; i < 2; ++i)
		{
			texName = "$VolFogInscatteringVolume" + std::to_string(i) + m_graphicsPipeline.GetUniqueIdentifierName();
			m_pFogInscatteringVolume[i] = CTexture::GetOrCreateTextureObjectPtr(texName.c_str(), 0, 0, 0, eTT_3D, uavFlags, eTF_Unknown);
		}
	}

	{
		for (int32 i = 0; i < 2; ++i)
		{
			texName = "$VolFogDensityVolume" + std::to_string(i) + m_graphicsPipeline.GetUniqueIdentifierName();
			m_pFogDensityVolume[i] = CTexture::GetOrCreateTextureObjectPtr(texName.c_str(), 0, 0, 0, eTT_3D, uavFlags, eTF_Unknown);
		}
	}

	CRY_ASSERT(m_pMaxDepth == nullptr);
	texName = "$VolFogMaxDepth" + m_graphicsPipeline.GetUniqueIdentifierName();
	m_pMaxDepth = CTexture::GetOrCreateTextureObjectPtr(texName.c_str(), 0, 0, 0, eTT_2D, uavFlags, eTF_Unknown);

	CRY_ASSERT(m_pMaxDepthTemp == nullptr);
	texName = "$VolFogMaxDepthTemp" + m_graphicsPipeline.GetUniqueIdentifierName();
	m_pMaxDepthTemp = CTexture::GetOrCreateTextureObjectPtr(texName.c_str(), 0, 0, 0, eTT_2D, uavFlags, eTF_Unknown);

	CRY_ASSERT(m_pNoiseTexture == nullptr);
	m_pNoiseTexture = CTexture::ForNamePtr("%ENGINE%/EngineAssets/Textures/rotrandomcm.dds", FT_DONT_STREAM, eTF_Unknown);

	{
		for (int32 i = 0; i < CVolumetricFogStage::ShadowCascadeNum; ++i)
		{
			CRY_ASSERT(m_pDownscaledShadow[i] == nullptr);
			texName = "$VolFogDownscaledShadow" + std::to_string(i) + m_graphicsPipeline.GetUniqueIdentifierName();
			m_pDownscaledShadow[i] = CTexture::GetOrCreateTextureObjectPtr(texName.c_str(), 0, 0, 0, eTT_2D, dsFlags, eTF_Unknown);
		}

		CRY_ASSERT(m_pDownscaledShadowTemp == nullptr);
		texName = "$VolFogDownscaledShadowTemp" + m_graphicsPipeline.GetUniqueIdentifierName();
		m_pDownscaledShadowTemp = CTexture::GetOrCreateTextureObjectPtr(texName.c_str(), 0, 0, 0, eTT_2D, dsFlags, eTF_Unknown);
	}

	// create scene render pass and resources.
	{
		m_pSceneRenderPassResourceSet = GetDeviceObjectFactory().CreateResourceSet(CDeviceResourceSet::EFlags_ForceSetAllState);
		m_pSceneRenderPassCB = gcpRendD3D->m_DevBufMan.CreateConstantBuffer(sizeof(SVolFogSceneRenderPassConstantBuffer));
		if (m_pSceneRenderPassCB) m_pSceneRenderPassCB->SetDebugName("VolumetricFogStage Per-Pass CB");

		CRY_VERIFY(PreparePerPassResources(true));

		// NOTE: use the resource layout of forward pass because CREParticle expects that layout in scene render pass.
		m_pSceneRenderResourceLayout = m_graphicsPipeline.CreateScenePassLayout(m_sceneRenderPassResources);
		CRY_ASSERT(m_pSceneRenderResourceLayout);

		// Freeze resource-set layout (assert will fire when violating the constraint)
		m_sceneRenderPassResources.AcceptChangedBindPoints();

		m_passInjectParticleDensity.SetLabel("ParticleInjection");
		m_passInjectParticleDensity.SetFlags(CSceneRenderPass::ePassFlags_None);
		m_passInjectParticleDensity.SetPassResources(m_pSceneRenderResourceLayout, m_pSceneRenderPassResourceSet);
	}
}

void CVolumetricFogStage::ResizeResource(int volumeWidth, int volumeHeight, int volumeDepth)
{
	const ETEX_Format fmtInscattering = CRendererResources::GetHDRFormat(false, false);
	const ETEX_Format fmtDensityColor = eTF_R11G11B10F;
	const ETEX_Format fmtDensity = eTF_R16F;
	const ETEX_Format fmtEmissive = eTF_R11G11B10F;
	const ETEX_Format fmtVolumetricFog = eTF_R16G16B16A16F;

	// downscaled depth buffer.
	const int32 depthTempWidth = (volumeWidth << 1);
	const int32 depthTempHeight = (m_graphicsPipelineResources.m_pTexLinearDepthScaled[0]->GetHeight() >> 1);
	const ETEX_Format fmtDepth = eTF_R16F;

	// downscaled shadow maps.
	static const ICVar* pCVarShadowMaxTexRes = nullptr;
	if (!pCVarShadowMaxTexRes)
		pCVarShadowMaxTexRes = gEnv->pConsole->GetCVar("e_ShadowsMaxTexRes");
	uint32 shadowMapTempWidth = 0;
	const uint32 shadowMapWidth = vfGetDownscaledShadowMapSize(shadowMapTempWidth, pCVarShadowMaxTexRes);
	CRY_ASSERT(CRendererCVars::CV_r_shadowtexformat >= 0 && CRendererCVars::CV_r_shadowtexformat < 3);
	const ETEX_Format fmtShadow = (CRendererCVars::CV_r_shadowtexformat == 1) ? eTF_D16 : eTF_D32F;

	auto createBuffer = [=](CGpuBuffer& buffer, uint32 elementNum) -> bool
	{
		const uint32 tileSizeX = 4;
		const uint32 tileSizeY = 4;
		const uint32 tileSizeZ = 4;
		const uint32 dispatchSizeX = (volumeWidth / tileSizeX) + (volumeWidth % tileSizeX > 0 ? 1 : 0);
		const uint32 dispatchSizeY = (volumeHeight / tileSizeY) + (volumeHeight % tileSizeY > 0 ? 1 : 0);
		const uint32 dispatchSizeZ = (volumeDepth / tileSizeZ) + (volumeDepth % tileSizeZ > 0 ? 1 : 0);
		const uint32 num = dispatchSizeX * dispatchSizeY * dispatchSizeZ * elementNum;

		bool bCreate = false;

		if (num != buffer.GetElementCount()
			|| buffer.GetDevBuffer() == nullptr)
		{
			const uint32 stride = sizeof(int8);
			const DXGI_FORMAT format = DXGI_FORMAT_R8_UINT;
			buffer.Create(num, stride, format, CDeviceObjectFactory::BIND_SHADER_RESOURCE | CDeviceObjectFactory::BIND_UNORDERED_ACCESS, nullptr);

			bCreate = true;
		}

		return bCreate;
	};

	auto createTexture3D = [=](CTexture* pTex, int32 flags, ETEX_Format texFormat) -> bool
	{
		bool bCreate = false;

		if (pTex != nullptr
			&& (volumeDepth != pTex->GetDepth()
				|| !CTexture::IsTextureExist(pTex)
				|| pTex->Invalidate(volumeWidth, volumeHeight, texFormat)))
		{
			pTex->Create3DTexture(volumeWidth, volumeHeight, volumeDepth, 1, flags, nullptr, texFormat);
			if (pTex->GetFlags() & FT_FAILED)
			{
				CryFatalError("Couldn't allocate texture.");
			}
			else
			{
				bCreate = true;
			}
		}

		return bCreate;
	};

	auto createTexture2D = [](CTexture* pTex, int32 w, int32 h, int32 flags, ETEX_Format texFormat) -> bool
	{
		bool bCreate = false;

		if (pTex != nullptr
			&& (!CTexture::IsTextureExist(pTex)
				|| pTex->Invalidate(w, h, texFormat)))
		{
			pTex->Create2DTexture(w, h, 1, flags, nullptr, texFormat);
			if (pTex->GetFlags() & FT_FAILED)
			{
				CryFatalError("Couldn't allocate texture.");
			}
			else
			{
				bCreate = true;
			}
		}

		return bCreate;
	};

	auto releaseDevTexture = [](CTexture* pTex)
	{
		if (pTex != nullptr
			&& CTexture::IsTextureExist(pTex))
		{
			pTex->ReleaseDeviceTexture(false);
		}
	};

	const uint32 commonFlags = FT_NOMIPS | FT_DONT_STREAM;
	const uint32 uavFlags = commonFlags | FT_USAGE_UNORDERED_ACCESS;
	const uint32 uavRtFlags = commonFlags | FT_USAGE_UNORDERED_ACCESS | FT_USAGE_RENDERTARGET;
	const uint32 dsFlags = commonFlags | FT_USAGE_DEPTHSTENCIL;

	bool bResetReprojection = false;
	bool bResetResource = false;

	bResetResource |= createBuffer(m_lightGridBuf, MaxNumTileLights);

	bResetResource |= createBuffer(m_lightCountBuf, 1);

	CRY_ASSERT(m_pVolFogBufDensityColor);
	CRY_ASSERT(m_pVolFogBufDensityColor->GetDstFormat() == fmtDensityColor);
	createTexture3D(m_pVolFogBufDensityColor, uavRtFlags, fmtDensityColor);

	CRY_ASSERT(m_pVolFogBufDensity);
	CRY_ASSERT(m_pVolFogBufDensity->GetDstFormat() == fmtDensity);
	createTexture3D(m_pVolFogBufDensity, uavRtFlags, fmtDensity);

	CRY_ASSERT(m_pVolFogBufEmissive);
	CRY_ASSERT(m_pVolFogBufEmissive->GetDstFormat() == fmtEmissive);
	createTexture3D(m_pVolFogBufEmissive, uavRtFlags, fmtEmissive);

	CRY_ASSERT(m_pInscatteringVolume);
	createTexture3D(m_pInscatteringVolume, uavFlags, fmtInscattering);

	for (auto pTex : m_pFogInscatteringVolume)
	{
		CRY_ASSERT(pTex);
		if (createTexture3D(pTex, uavFlags, fmtInscattering))
		{
			pTex->DisableMgpuSync();
			bResetReprojection = true;
		}
	}

	// If the inscattering format has no alpha-channel we need an extra resource for the density
	if ((m_seperateDensity = !CImageExtensionHelper::HasAlphaForTextureFormat(fmtInscattering)))
	{
		for (auto pTex : m_pFogDensityVolume)
		{
			CRY_ASSERT(pTex);
			if (createTexture3D(pTex, uavFlags, fmtDensity))
			{
				pTex->DisableMgpuSync();
				bResetReprojection = true;
			}
		}
	}
	else
	{
		for (auto pTex : m_pFogDensityVolume)
		{
			CRY_ASSERT(pTex);
			releaseDevTexture(pTex);
		}
	}

	CRY_ASSERT(m_pMaxDepth);
	createTexture2D(m_pMaxDepth, volumeWidth, volumeHeight, uavFlags, fmtDepth);

	CRY_ASSERT(m_pMaxDepthTemp);
	createTexture2D(m_pMaxDepthTemp, depthTempWidth, depthTempHeight, uavFlags, fmtDepth);

	if (CRendererCVars::CV_r_VolumetricFogDownscaledSunShadow > 0)
	{
		const int32 cascadeNum = vfGetShadowCascadeNum();

		for (int32 i = 0; i < CVolumetricFogStage::ShadowCascadeNum; ++i)
		{
			CRY_ASSERT(m_pDownscaledShadow[i]);
			if (i < cascadeNum)
			{
				bResetResource |= createTexture2D(m_pDownscaledShadow[i], shadowMapWidth, shadowMapWidth, dsFlags, fmtShadow);
			}
			else
			{
				releaseDevTexture(m_pDownscaledShadow[i].get());
			}
		}

		if (CRendererCVars::CV_r_VolumetricFogDownscaledSunShadowRatio != 0)
		{
			CRY_ASSERT(m_pDownscaledShadowTemp);
			bResetResource |= createTexture2D(m_pDownscaledShadowTemp, shadowMapTempWidth, shadowMapTempWidth, dsFlags, fmtShadow);
		}
		else
		{
			releaseDevTexture(m_pDownscaledShadowTemp);
		}
	}
	else
	{
		for (auto& pTex : m_pDownscaledShadow)
		{
			releaseDevTexture(pTex);
		}
		releaseDevTexture(m_pDownscaledShadowTemp);
	}

	// TODO: refactor after removing old graphics pipeline.
	CRY_ASSERT(CRendererResources::s_ptexVolumetricFog);
	createTexture3D(CRendererResources::s_ptexVolumetricFog, uavFlags, fmtVolumetricFog);

	if (bResetReprojection)
	{
		m_cleared = MaxFrameNum;
	}
}

void CVolumetricFogStage::Rescale(int resolutionScale, int depthScale)
{
	const CRenderView* pRenderView = RenderView();
	const int32 renderWidth  = pRenderView->GetRenderResolution()[0];
	const int32 renderHeight = pRenderView->GetRenderResolution()[1];

	// size for view frustum aligned volume texture.
	const int32 volumeWidth  = CVolumetricFogStage::GetVolumeTextureSize(renderWidth , resolutionScale);
	const int32 volumeHeight = CVolumetricFogStage::GetVolumeTextureSize(renderHeight, resolutionScale);
	const int32 volumeDepth  = CVolumetricFogStage::GetVolumeTextureDepthSize(depthScale);

	ResizeResource(volumeWidth, volumeHeight, volumeDepth);
}

void CVolumetricFogStage::Resize(int renderWidth, int renderHeight)
{
	// Recreate render targets if quality was changed
	const int32 resolutionScale = CRendererCVars::CV_r_VolumetricFogTexScale;
	const int32 depthScale      = CRendererCVars::CV_r_VolumetricFogTexDepth;

	// size for view frustum aligned volume texture.
	const int32 volumeWidth  = CVolumetricFogStage::GetVolumeTextureSize(renderWidth , resolutionScale);
	const int32 volumeHeight = CVolumetricFogStage::GetVolumeTextureSize(renderHeight, resolutionScale);
	const int32 volumeDepth  = CVolumetricFogStage::GetVolumeTextureDepthSize(depthScale);

	ResizeResource(volumeWidth, volumeHeight, volumeDepth);
}

void CVolumetricFogStage::OnCVarsChanged(const CCVarUpdateRecorder& cvarUpdater)
{
	bool hasChanged = false;
	int32 resolutionScale = CRendererCVars::CV_r_VolumetricFogTexScale;
	int32 depthScale      = CRendererCVars::CV_r_VolumetricFogTexDepth;

	if (auto pVar = cvarUpdater.GetCVar("r_VolumetricFogTexScale"))
	{
		resolutionScale = pVar->intValue;
		hasChanged = true;
	}

	if (auto pVar = cvarUpdater.GetCVar("r_VolumetricFogTexDepth"))
	{
		depthScale = pVar->intValue;
		hasChanged = true;
	}

	if (cvarUpdater.GetCVar("r_shadowtexformat") ||
	    cvarUpdater.GetCVar("r_HDRTexFormat") ||
	    cvarUpdater.GetCVar("e_ShadowsMaxTexRes") ||
	    cvarUpdater.GetCVar("r_VolumetricFogDownscaledSunShadow") ||
	    cvarUpdater.GetCVar("r_VolumetricFogDownscaledSunShadowRatio"))
	{
		hasChanged = true;
	}

	// Recreate render targets if quality was changed
	if (hasChanged)
		Rescale(resolutionScale, depthScale);
}

void CVolumetricFogStage::Update()
{
	const CRenderView* pRenderView = RenderView();
	const SRenderViewport& viewport = pRenderView->GetViewport();
	m_passInjectParticleDensity.SetViewport(viewport);
	m_passInjectParticleDensity.SetRenderTargets(
		// Depth
		nullptr,
		// Color 0
		m_pVolFogBufDensityColor,
		// Color 1
		m_pVolFogBufDensity,
		// Color 2
		m_pVolFogBufEmissive);
}

void CVolumetricFogStage::Execute()
{
	FUNCTION_PROFILER_RENDERER();

	ResetFrame();

	if (!IsTexturesValid())
	{
		return;
	}

	PROFILE_LABEL_SCOPE("VOLUMETRIC FOG");

	UpdateFrame();

	ExecuteDownscaleShadowmap();

	const bool bAsyncronous = false;
	SScopedComputeCommandList commandList(bAsyncronous);

	GenerateLightList();

	ExecuteBuildLightListGrid(commandList);
	ExecuteDownscaledDepth(commandList);
	ExecuteInjectParticipatingMedia(commandList);
	ExecuteVolumetricFog(commandList);
}

bool CVolumetricFogStage::CreatePipelineState(const SGraphicsPipelineStateDescription& desc, CDeviceGraphicsPSOPtr& outPSO) const
{
	outPSO = nullptr;

	CDeviceGraphicsPSODesc psoDesc(m_pSceneRenderResourceLayout, desc);

	// overwrite states.
	psoDesc.m_RenderState &= ~(GS_BLSRC_MASK | GS_BLDST_MASK | GS_NODEPTHTEST | GS_DEPTHWRITE | GS_DEPTHFUNC_MASK);
	psoDesc.m_RenderState |= (GS_BLSRC_ONE | GS_BLDST_ONE | GS_NODEPTHTEST);
	psoDesc.m_CullMode = eCULL_None;

	const bool bReverseDepth = true;
	if (bReverseDepth)
	{
		psoDesc.m_RenderState |= ReverseDepthHelper::ConvertDepthFunc(psoDesc.m_RenderState);
		psoDesc.m_ShaderFlags_RT |= g_HWSR_MaskBit[HWSR_REVERSE_DEPTH];
	}

	psoDesc.m_pRenderPass = m_passInjectParticleDensity.GetRenderPass();

	outPSO = GetDeviceObjectFactory().CreateGraphicsPSO(psoDesc);

	return outPSO != nullptr;
}

template<class RenderPassType>
void CVolumetricFogStage::BindVolumetricFogResources(RenderPassType& pass, int32 startTexSlot, int32 trilinearClampSamplerSlot)
{
	if (startTexSlot >= 0)
	{
		pass.SetTexture(startTexSlot++, CRendererResources::s_ptexVolumetricFog);
		pass.SetTexture(startTexSlot++, m_globalEnvProveTex0 ? m_globalEnvProveTex0.get() : CRendererResources::s_ptexBlackCM);
		pass.SetTexture(startTexSlot++, m_globalEnvProveTex1 ? m_globalEnvProveTex1.get() : CRendererResources::s_ptexBlackCM);
	}

	if (trilinearClampSamplerSlot >= 0)
	{
		pass.SetSampler(trilinearClampSamplerSlot, EDefaultSamplerStates::TrilinearClamp);
	}
}

// explicit instantiation
template
void      CVolumetricFogStage::BindVolumetricFogResources(CFullscreenPass& pass, int32 startTexSlot, int32 trilinearClampSamplerSlot);
template
void      CVolumetricFogStage::BindVolumetricFogResources(CComputeRenderPass& pass, int32 startTexSlot, int32 trilinearClampSamplerSlot);

CTexture* CVolumetricFogStage::GetVolumetricFogTex() const
{
	if (gcpRendD3D->m_bVolumetricFogEnabled && CTexture::IsTextureExist(CRendererResources::s_ptexVolumetricFog))
	{
		return CRendererResources::s_ptexVolumetricFog;
	}
	else
	{
		return CRendererResources::s_ptexBlack;
	}
}

CTexture* CVolumetricFogStage::GetGlobalEnvProbeTex0() const
{
	return (m_globalEnvProveTex0 ? m_globalEnvProveTex0.get() : CRendererResources::s_ptexBlackCM);
}

CTexture* CVolumetricFogStage::GetGlobalEnvProbeTex1() const
{
	return (m_globalEnvProveTex1 ? m_globalEnvProveTex1.get() : CRendererResources::s_ptexBlackCM);
}

bool CVolumetricFogStage::PreparePerPassResources(bool bOnInit)
{
	CRY_ASSERT(m_pSceneRenderPassResourceSet);

	// Samplers
	{
		auto materialSamplers = m_graphicsPipeline.GetDefaultMaterialSamplers();
		for (int i = 0; i < materialSamplers.size(); ++i)
		{
			m_sceneRenderPassResources.SetSampler(EEfResSamplers(i), materialSamplers[i], EShaderStage_AllWithoutCompute);
		}
	}

	// Constant buffers
	{
		if (!bOnInit)
		{
			CryStackAllocWithSize(SVolFogSceneRenderPassConstantBuffer, cb, CDeviceBufferManager::AlignBufferSizeForStreaming);

			CRY_ASSERT(m_pSceneRenderPassCB && RenderView());

			FillForwardParams(cb->cbVolFogInj);

			m_pSceneRenderPassCB->UpdateBuffer(cb, cbSize);
		}

		CConstantBufferPtr pPerPassCB;
		CConstantBufferPtr pPerViewCB;

		if (bOnInit)  // Handle case when no view is available in the initialization of the stage
		{
			pPerPassCB = CDeviceBufferManager::GetNullConstantBuffer();
			pPerViewCB = CDeviceBufferManager::GetNullConstantBuffer();
		}
		else
		{
			pPerPassCB = m_pSceneRenderPassCB;
			pPerViewCB = m_graphicsPipeline.GetMainViewConstantBuffer();
		}

		m_sceneRenderPassResources.SetConstantBuffer(eConstantBufferShaderSlot_PerPass, pPerPassCB, EShaderStage_AllWithoutCompute);
		m_sceneRenderPassResources.SetConstantBuffer(eConstantBufferShaderSlot_PerView, pPerViewCB, EShaderStage_AllWithoutCompute);
	}

	if (bOnInit)
		return true;

	CRY_ASSERT(!m_sceneRenderPassResources.HasChangedBindPoints()); // Cannot change resource layout after init. It is baked into the shaders
	return m_pSceneRenderPassResourceSet->Update(m_sceneRenderPassResources);
}

void CVolumetricFogStage::ExecuteInjectParticipatingMedia(const SScopedComputeCommandList& commandList)
{
	PROFILE_LABEL_SCOPE("INJECT_PARTICIPATING_MEDIA");

	GenerateFogVolumeList();
	ExecuteInjectFogDensity(commandList);

	CRenderView* pRenderView = RenderView();
	CRY_ASSERT(pRenderView);

	// inject particle's density and albedo into volume texture.
	auto& rendItems = pRenderView->GetRenderItems(EFSLIST_FOG_VOLUME);
	if (!rendItems.empty())
	{
		PreparePerPassResources(false);

		auto& pass = m_passInjectParticleDensity;

		pass.ExchangeRenderTarget(0, m_pVolFogBufDensityColor);
		pass.ExchangeRenderTarget(1, m_pVolFogBufDensity);
		pass.ExchangeRenderTarget(2, m_pVolFogBufEmissive);

		auto& RESTRICT_REFERENCE commandList = GetDeviceObjectFactory().GetCoreCommandList();
		pass.PrepareRenderPassForUse(commandList);

		auto& renderItemDrawer = pRenderView->GetDrawer();
		renderItemDrawer.InitDrawSubmission();

		pass.BeginExecution(m_graphicsPipeline);
		pass.SetupDrawContext(StageID, 0, TTYPE_GENERAL, FB_GENERAL);
		pass.DrawRenderItems(pRenderView, EFSLIST_FOG_VOLUME);
		pass.EndExecution();

		renderItemDrawer.JobifyDrawSubmission();
		renderItemDrawer.WaitForDrawSubmission();

		ClearDeviceOutputState();
	}
}

void CVolumetricFogStage::ExecuteVolumetricFog(const SScopedComputeCommandList& commandList)
{
	PROFILE_LABEL_SCOPE("RENDER_VOLUMETRIC_FOG");

	ExecuteInjectInscatteringLight(commandList);

	const int32 VolumetricFogBlurInscattering = 1;
	if (VolumetricFogBlurInscattering > 0)
	{
		int32 maxBlurCount = VolumetricFogBlurInscattering;
		maxBlurCount = maxBlurCount <= 4 ? maxBlurCount : 4;
		for (int32 count = 0; count < maxBlurCount; ++count)
		{
			ExecuteBlurInscatterVolume(commandList); // Supports interleaved and non-interleaved inscattering with density
//			ExecuteBlurDensityVolume(commandList);
		}
	}

	ExecuteTemporalReprojection(commandList);

	ExecuteRaymarchVolumetricFog(commandList);
}

uint32 CVolumetricFogStage::GetTemporalBufferId() const
{
	return ((m_tick / gcpRendD3D->GetActiveGPUCount()) & 0x1);
}

CTexture* CVolumetricFogStage::GetInscatterTex() const
{
	return (GetTemporalBufferId() ? m_pFogInscatteringVolume[0] : m_pFogInscatteringVolume[1]);
}

CTexture* CVolumetricFogStage::GetPrevInscatterTex() const
{
	return (GetTemporalBufferId() ? m_pFogInscatteringVolume[1] : m_pFogInscatteringVolume[0]);
}

CTexture* CVolumetricFogStage::GetDensityTex() const
{
	return (GetTemporalBufferId() ? m_pFogDensityVolume[0] : m_pFogDensityVolume[1]);
}

CTexture* CVolumetricFogStage::GetPrevDensityTex() const
{
	return (GetTemporalBufferId() ? m_pFogDensityVolume[1] : m_pFogDensityVolume[0]);
}

bool CVolumetricFogStage::IsTexturesValid() const
{
	const bool bVolumeTexture = CTexture::IsTextureExist(m_pVolFogBufDensityColor)
	                            && CTexture::IsTextureExist(m_pVolFogBufDensity)
	                            && CTexture::IsTextureExist(m_pVolFogBufEmissive)
	                            && CTexture::IsTextureExist(m_pInscatteringVolume)
	                            && CTexture::IsTextureExist(m_pFogInscatteringVolume[0])
	                            && CTexture::IsTextureExist(m_pFogInscatteringVolume[1])
	                            && CTexture::IsTextureExist(CRendererResources::s_ptexVolumetricFog);

	const bool bDepth = CTexture::IsTextureExist(m_pMaxDepth)
	                    && CTexture::IsTextureExist(m_pMaxDepthTemp);

	const bool bDownscaledShadow0 = (CRendererCVars::CV_r_VolumetricFogDownscaledSunShadow == 0);
	const bool bDownscaledShadowTex = (CTexture::IsTextureExist(m_pDownscaledShadow[0])
	                                   && CTexture::IsTextureExist(m_pDownscaledShadow[1]));
	const bool bDownscaledShadow1 = (bDownscaledShadowTex
	                                 && (CRendererCVars::CV_r_VolumetricFogDownscaledSunShadow == 1));
	const bool bDownscaledShadow2 = (bDownscaledShadowTex
	                                 && (CRendererCVars::CV_r_VolumetricFogDownscaledSunShadow == 2)
	                                 && CTexture::IsTextureExist(m_pDownscaledShadow[2]));
	const bool bShadowTemp = (CRendererCVars::CV_r_VolumetricFogDownscaledSunShadowRatio == 0)
	                         || ((CRendererCVars::CV_r_VolumetricFogDownscaledSunShadowRatio != 0)
	                             && CTexture::IsTextureExist(m_pDownscaledShadowTemp));
	const bool bShadowMaps = (bDownscaledShadow0
	                          || ((bDownscaledShadow1 || bDownscaledShadow2) && bShadowTemp));

	return bVolumeTexture && bDepth && bShadowMaps;
}

void CVolumetricFogStage::ResetFrame()
{
	m_globalEnvProbeParam0 = Vec4(0.0f, 0.0f, 0.0f, 0.0f);
	m_globalEnvProbeParam1 = Vec4(0.0f, 0.0f, 0.0f, 0.0f);
}

void CVolumetricFogStage::UpdateFrame()
{
	int64 frameID = RenderView()->GetFrameId();

	if (m_frameID != frameID)
	{
		Matrix44 mViewProj = GetCurrentViewInfo().cameraProjMatrix;
		Matrix44& mViewport = SD3DPostEffectsUtils::GetInstance().m_pScaleBias;

		++m_tick;
		m_frameID = frameID;
		m_viewProj[m_tick % MaxFrameNum] = mViewProj * mViewport;
	}
}

void CVolumetricFogStage::ExecuteDownscaleShadowmap()
{
	CRenderView* pRenderView = RenderView();
	CRY_ASSERT(pRenderView);

	if (!gcpRendD3D->m_bShadowsEnabled)
	{
		return;
	}

	if (CRendererCVars::CV_r_VolumetricFogDownscaledSunShadow <= 0)
	{
		return;
	}

	//PROFILE_LABEL_SCOPE("DOWNSCALE_SHADOWMAP");

	CShader* pShader = CShaderMan::s_shDeferredShading;

	CShadowUtils::SShadowCascades cascades;
	const bool bSunShadow = CShadowUtils::SetupShadowsForFog(cascades, pRenderView);

	const int32 cascadeNum = vfGetShadowCascadeNum();

	for (int32 i = 0; i < cascadeNum; ++i)
	{
		// downscale full resolution to half resolution.
		{
			auto& pass = m_passDownscaleShadowmap[i];

			uint32 inputFlag = 0;
			inputFlag |= bSunShadow ? BIT(0) : 0;

			CTexture* target = NULL;
			if (CRendererCVars::CV_r_VolumetricFogDownscaledSunShadowRatio == 0)
			{
				target = m_pDownscaledShadow[i];
			}
			else
			{
				target = m_pDownscaledShadowTemp;
			}

			if (pass.IsDirty(inputFlag,
			                 CRendererCVars::CV_r_VolumetricFogDownscaledSunShadowRatio,
			                 target->GetDstFormat()))
			{
				uint64 rtMask = 0;
				const uint64 lv0 = g_HWSR_MaskBit[HWSR_LIGHTVOLUME0];
				const uint64 lv1 = g_HWSR_MaskBit[HWSR_LIGHTVOLUME1];
				switch (i)
				{
				case 0:
					rtMask |= lv0;
					break;
				case 1:
					rtMask |= lv1;
					break;
				case 2:
					rtMask |= lv0 | lv1;
					break;
				default:
					break;
				}
				static CCryNameTSCRC techName("RenderDownscaledShadowMap");
				pass.SetPrimitiveFlags(CRenderPrimitive::eFlags_None);
				pass.SetPrimitiveType(CRenderPrimitive::ePrim_ProceduralTriangle);
				pass.SetTechnique(pShader, techName, rtMask);
				pass.SetDepthTarget(target);
				pass.SetState(GS_NOCOLMASK_RGBA | GS_DEPTHWRITE | GS_DEPTHFUNC_NOTEQUAL);

				pass.SetSampler(0, EDefaultSamplerStates::PointClamp);
			}

			CShadowUtils::SetShadowCascadesToRenderPass(pass, 0, -1, cascades);

			pass.BeginConstantUpdate();

			pass.Execute();
		}

		// downscale half resolution to 1/4 or 1/8 resolution.
		if (CRendererCVars::CV_r_VolumetricFogDownscaledSunShadowRatio != 0)
		{
			auto& pass = m_passDownscaleShadowmap2[i];

			if (pass.IsDirty(CRendererCVars::CV_r_VolumetricFogDownscaledSunShadowRatio,
			                 m_pDownscaledShadow[i]->GetDstFormat()))
			{
				static CCryNameTSCRC shaderName0("DownscaleShadowMap2");
				static CCryNameTSCRC shaderName1("DownscaleShadowMap4");
				const auto& techName = (CRendererCVars::CV_r_VolumetricFogDownscaledSunShadowRatio == 1) ? shaderName0 : shaderName1;
				pass.SetPrimitiveFlags(CRenderPrimitive::eFlags_None);
				pass.SetPrimitiveType(CRenderPrimitive::ePrim_ProceduralTriangle);
				pass.SetTechnique(pShader, techName, 0);

				CTexture* source = m_pDownscaledShadowTemp;
				CTexture* target = m_pDownscaledShadow[i];

				pass.SetDepthTarget(target);
				pass.SetState(GS_NOCOLMASK_RGBA | GS_DEPTHWRITE | GS_DEPTHFUNC_NOTEQUAL);
				pass.SetTexture(0, source);
				pass.SetSampler(0, EDefaultSamplerStates::PointClamp);
			}

			pass.BeginConstantUpdate();

			pass.Execute();
		}
	}

	ClearDeviceOutputState();
}

void CVolumetricFogStage::GenerateLightList()
{
	CRenderView* pRenderView = RenderView();
	CRY_ASSERT(pRenderView);

	CD3D9Renderer* const __restrict rd = gcpRendD3D;

	const SRenderViewInfo& viewInfo = GetCurrentViewInfo();
	auto* tiledLights = m_graphicsPipeline.GetStage<CTiledLightVolumesStage>();

	// Prepare view matrix with flipped z-axis
	Matrix44A matView = viewInfo.viewMatrix;
	matView.m02 *= -1;
	matView.m12 *= -1;
	matView.m22 *= -1;
	matView.m32 *= -1;

	const float itensityScale = rd->m_fAdaptedSceneScaleLBuffer;

	const auto& defLights = pRenderView->GetLightsArray(eDLT_DeferredLight);
	const auto& envProbes = pRenderView->GetLightsArray(eDLT_DeferredCubemap);
	const auto& ambientLights = pRenderView->GetLightsArray(eDLT_DeferredAmbientLight);

	const uint32 lightArraySize = 3;
	const RenderLightsList* lightLists[3] = {
		CRendererCVars::CV_r_DeferredShadingEnvProbes ? &envProbes : nullptr,
		CRendererCVars::CV_r_DeferredShadingAmbientLights ? &ambientLights : nullptr,
		CRendererCVars::CV_r_DeferredShadingLights ? &defLights : nullptr,
	};

	const Vec3 worldViewPos = viewInfo.cameraOrigin;
	const Vec3 cameraPosition = pRenderView->GetCamera(CCamera::eEye_Left).GetPosition();

	const float minBulbSize = max(0.001f, min(2.0f, CRendererCVars::CV_r_VolumetricFogMinimumLightBulbSize));// limit the minimum bulb size to reduce the light flicker.

	// NOTE: Get aligned stack-space (pointer and size aligned to manager's alignment requirement)
	CryStackAllocWithSizeVectorCleared(CTiledLightVolumesStage::STiledLightCullInfo, MaxNumTileLights, tileLightsCull, CDeviceBufferManager::AlignBufferSizeForStreaming);
	CryStackAllocWithSizeVectorCleared(CTiledLightVolumesStage::STiledLightShadeInfo, MaxNumTileLights, tileLightsShade, CDeviceBufferManager::AlignBufferSizeForStreaming);

	uint32 numTileLights = 0;
	uint32 numRenderLights = 0;
	uint32 numValidRenderLights = 0;
	CTexture* texGlobalEnvProbe0 = nullptr;
	CTexture* texGlobalEnvProbe1 = nullptr;
	Vec3 colorGlobalEnvProbe0(0.0f, 0.0f, 0.0f);
	Vec3 colorGlobalEnvProbe1(0.0f, 0.0f, 0.0f);
	float attenuationGlobalEnvProbe0 = 0.0f;
	float attenuationGlobalEnvProbe1 = 0.0f;
	float maxSizeGlobalEnvProbe0 = 0.0f;
	float maxSizeGlobalEnvProbe1 = 0.0f;

	for (uint32 lightListIdx = 0; lightListIdx < lightArraySize; ++lightListIdx)
	{
		if (lightLists[lightListIdx] == nullptr)
			continue;

		auto stp = lightLists[lightListIdx]->end();
		auto itr = lightLists[lightListIdx]->begin();
		for (uint32 lightIdx = 0; itr != stp; ++itr, ++lightIdx)
		{
			const SRenderLight& renderLight = *itr;
			CTiledLightVolumesStage::STiledLightCullInfo& lightCullInfo = tileLightsCull[numTileLights];
			CTiledLightVolumesStage::STiledLightShadeInfo& lightShadeInfo = tileLightsShade[numTileLights];

			if ((renderLight.m_Flags & DLF_FAKE) || !(renderLight.m_Flags & DLF_VOLUMETRIC_FOG))
				continue;

			++numRenderLights;

			if (numTileLights == MaxNumTileLights)
				continue;  // Skip light

			// Setup standard parameters
			float mipFactor = (cameraPosition - renderLight.m_Origin).GetLengthSquared() / max(0.001f, renderLight.m_fRadius * renderLight.m_fRadius);
			bool isAreaLight = (renderLight.m_Flags & DLF_AREA) != 0;
			float volumeSize = (lightListIdx == 0) ? renderLight.m_ProbeExtents.len() : renderLight.m_fRadius;
			Vec3 pos = renderLight.GetPosition();
			lightShadeInfo.posRad = Vec4(pos.x - worldViewPos.x, pos.y - worldViewPos.y, pos.z - worldViewPos.z, volumeSize);
			Vec4 posVS = Vec4(pos, 1) * matView;
			lightCullInfo.posRad = Vec4(posVS.x, posVS.y, posVS.z, volumeSize);
			lightShadeInfo.attenuationParams = Vec2(renderLight.m_fAttenuationBulbSize, minBulbSize);
			lightCullInfo.depthBounds = Vec2(posVS.z - volumeSize, posVS.z + volumeSize);
			lightShadeInfo.color = Vec4(renderLight.m_Color.r * itensityScale,
			                            renderLight.m_Color.g * itensityScale,
			                            renderLight.m_Color.b * itensityScale,
			                            renderLight.m_fFogRadialLobe);
			lightShadeInfo.resIndex = 0;
			lightShadeInfo.resMipClamp0 = 0;
			lightShadeInfo.resMipClamp1 = 0;
			lightShadeInfo.shadowParams = Vec2(0, 0);
			lightShadeInfo.stencilID0 = renderLight.m_nStencilRef[0];
			lightShadeInfo.stencilID1 = renderLight.m_nStencilRef[1];

			// Environment probes
			if (lightListIdx == 0)
			{
				lightCullInfo.volumeType = CTiledLightVolumesStage::tlVolumeOBB;
				lightShadeInfo.lightType = CTiledLightVolumesStage::tlTypeProbe;
				lightShadeInfo.resIndex = lightShadeInfo.resNoIndex;
				lightShadeInfo.attenuationParams.x = renderLight.m_Color.a;
				// assigning value isn't needed because AttenuationFalloffMax is hard-coded in VolumeLighting.cfi to mitigate sharp transition between probes.
				//lightShadeInfo.attenuationParams.y = max( renderLight.GetFalloffMax(), 0.001f );
				lightCullInfo.miscFlag = 0;

				AABB aabb = vfRotateAABB(AABB(-renderLight.m_ProbeExtents, renderLight.m_ProbeExtents), Matrix33(renderLight.m_ObjMatrix));
				aabb = vfRotateAABB(aabb, Matrix33(matView));
				lightCullInfo.depthBounds = Vec2(posVS.z + aabb.min.z, posVS.z + aabb.max.z);

				Vec4 u0 = Vec4(renderLight.m_ObjMatrix.GetColumn0().GetNormalized(), 0) * matView;
				Vec4 u1 = Vec4(renderLight.m_ObjMatrix.GetColumn1().GetNormalized(), 0) * matView;
				Vec4 u2 = Vec4(renderLight.m_ObjMatrix.GetColumn2().GetNormalized(), 0) * matView;
				lightCullInfo.volumeParams0 = Vec4(u0.x, u0.y, u0.z, renderLight.m_ProbeExtents.x);
				lightCullInfo.volumeParams1 = Vec4(u1.x, u1.y, u1.z, renderLight.m_ProbeExtents.y);
				lightCullInfo.volumeParams2 = Vec4(u2.x, u2.y, u2.z, renderLight.m_ProbeExtents.z);

				lightShadeInfo.projectorMatrix.SetRow4(0, Vec4(renderLight.m_ObjMatrix.GetColumn0().GetNormalized() / renderLight.m_ProbeExtents.x, 0));
				lightShadeInfo.projectorMatrix.SetRow4(1, Vec4(renderLight.m_ObjMatrix.GetColumn1().GetNormalized() / renderLight.m_ProbeExtents.y, 0));
				lightShadeInfo.projectorMatrix.SetRow4(2, Vec4(renderLight.m_ObjMatrix.GetColumn2().GetNormalized() / renderLight.m_ProbeExtents.z, 0));

				Vec3 boxProxyMin(-1000000, -1000000, -1000000);
				Vec3 boxProxyMax(1000000, 1000000, 1000000);

				if (renderLight.m_Flags & DLF_BOX_PROJECTED_CM)
				{
					boxProxyMin = Vec3(-renderLight.m_fBoxLength * 0.5f, -renderLight.m_fBoxWidth * 0.5f, -renderLight.m_fBoxHeight * 0.5f);
					boxProxyMax = Vec3(renderLight.m_fBoxLength * 0.5f, renderLight.m_fBoxWidth * 0.5f, renderLight.m_fBoxHeight * 0.5f);
				}

				lightShadeInfo.shadowMatrix.SetRow4(0, Vec4(boxProxyMin, 0));
				lightShadeInfo.shadowMatrix.SetRow4(1, Vec4(boxProxyMax, 0));

				{
					CTexture* pSpecTexture = (CTexture*)renderLight.GetSpecularCubemap();
					CTexture* pDiffTexture = (CTexture*)renderLight.GetDiffuseCubemap();

					int arrayIndex = tiledLights->InsertTexture(pSpecTexture, mipFactor, tiledLights->m_specularProbeAtlas, -1);
					if (arrayIndex < 0)
						continue;  // Skip light
					if (tiledLights->InsertTexture(pDiffTexture, mipFactor, tiledLights->m_diffuseProbeAtlas, arrayIndex) < 0)
						continue;  // Skip light

					lightShadeInfo.resIndex = arrayIndex;
					lightShadeInfo.resMipClamp0 = tiledLights->m_diffuseProbeAtlas.items[arrayIndex].lowestRenderableMip;
					lightShadeInfo.resMipClamp1 = tiledLights->m_specularProbeAtlas.items[arrayIndex].lowestRenderableMip;
				}

				// Determine the global env probe for lighting analytical volumetric fog if we have it in the scene.
				// Enough big(more than 1000 meters), the biggest and the second biggest including camera position are selected.
				if (volumeSize >= ThresholdLengthGlobalProbe &&
				    (volumeSize > maxSizeGlobalEnvProbe0 || volumeSize > maxSizeGlobalEnvProbe1))
				{
					Vec3 vLightPos = renderLight.GetPosition();
					Vec3 vCenterRel = worldViewPos - vLightPos;
					if (vCenterRel.dot(vCenterRel) <= volumeSize * volumeSize)
					{
						Vec3 vCenterOBBSpace;
						vCenterOBBSpace.x = renderLight.m_ObjMatrix.GetColumn0().GetNormalized().dot(vCenterRel);
						vCenterOBBSpace.y = renderLight.m_ObjMatrix.GetColumn1().GetNormalized().dot(vCenterRel);
						vCenterOBBSpace.z = renderLight.m_ObjMatrix.GetColumn2().GetNormalized().dot(vCenterRel);

						// Check if camera position is within probe OBB
						Vec3 vProbeExtents = renderLight.m_ProbeExtents;
						if (fabs(vCenterOBBSpace.x) < vProbeExtents.x && fabs(vCenterOBBSpace.y) < vProbeExtents.y && fabs(vCenterOBBSpace.z) < vProbeExtents.z)
						{
							Vec3 pos;
							pos.x = vCenterOBBSpace.x / vProbeExtents.x;
							pos.y = vCenterOBBSpace.y / vProbeExtents.y;
							pos.z = vCenterOBBSpace.z / vProbeExtents.z;
							Vec3 pos2 = pos.CompMul(pos);
							Vec3 t;
							t.x = sqrt(1.0f - 0.5f * pos2.y - 0.5f * pos2.z + 0.333f * pos2.y * pos2.z);
							t.y = sqrt(1.0f - 0.5f * pos2.z - 0.5f * pos2.x + 0.333f * pos2.z * pos2.x);
							t.z = sqrt(1.0f - 0.5f * pos2.x - 0.5f * pos2.y + 0.333f * pos2.x * pos2.y);
							pos = pos.CompMul(t);
							float falloff = max(0.0f, min(1.0f, 1.0f + pos.Dot(-pos)));
							const static float AttenuationFalloffMax = 0.2f;
							falloff = min(1.0f, falloff / max(renderLight.GetFalloffMax(), AttenuationFalloffMax));
							float attenuation = falloff * falloff * (3.0f - 2.0f * falloff) * renderLight.m_Color.a;

							if (volumeSize > maxSizeGlobalEnvProbe0)
							{
								maxSizeGlobalEnvProbe1 = maxSizeGlobalEnvProbe0;
								texGlobalEnvProbe1 = texGlobalEnvProbe0;
								colorGlobalEnvProbe1 = colorGlobalEnvProbe0;
								attenuationGlobalEnvProbe1 = attenuationGlobalEnvProbe0;

								maxSizeGlobalEnvProbe0 = volumeSize;
								texGlobalEnvProbe0 = (CTexture*)renderLight.GetDiffuseCubemap();
								colorGlobalEnvProbe0 = Vec3(lightShadeInfo.color);
								attenuationGlobalEnvProbe0 = attenuation;
							}
							else if (volumeSize > maxSizeGlobalEnvProbe1)
							{
								maxSizeGlobalEnvProbe1 = volumeSize;
								texGlobalEnvProbe1 = (CTexture*)renderLight.GetDiffuseCubemap();
								colorGlobalEnvProbe1 = Vec3(lightShadeInfo.color);
								attenuationGlobalEnvProbe1 = attenuation;
							}
						}
					}
				}
			}
			// Regular and ambient lights
			else
			{
				const float sqrt_2 = 1.414214f;  // Scale for cone so that it's base encloses a pyramid base

				const bool ambientLight = (lightListIdx == 1);

				lightCullInfo.volumeType = CTiledLightVolumesStage::tlVolumeSphere;
				lightShadeInfo.lightType = ambientLight ? CTiledLightVolumesStage::tlTypeAmbientPoint : CTiledLightVolumesStage::tlTypeRegularPoint;
				lightCullInfo.miscFlag = 0;

				if (!ambientLight)
				{
					lightShadeInfo.attenuationParams.x = max(lightShadeInfo.attenuationParams.x, minBulbSize);

					// Adjust light intensity so that the intended brightness is reached 1 meter from the light's surface
					// Solve I * 1 / (1 + d/lightsize)^2 = 1
					float intensityMul = 1.0f + 1.0f / lightShadeInfo.attenuationParams.x;
					intensityMul *= intensityMul;
					lightShadeInfo.color.x *= intensityMul;
					lightShadeInfo.color.y *= intensityMul;
					lightShadeInfo.color.z *= intensityMul;
				}

				// Handle projectors
				if (renderLight.m_Flags & DLF_PROJECT)
				{
					lightCullInfo.volumeType = CTiledLightVolumesStage::tlVolumeCone;
					lightShadeInfo.lightType = ambientLight ? CTiledLightVolumesStage::tlTypeAmbientProjector : CTiledLightVolumesStage::tlTypeRegularProjector;
					lightShadeInfo.resIndex = lightShadeInfo.resNoIndex;
					lightCullInfo.miscFlag = 0;

					{
						CTexture* pProjTexture = (CTexture*)renderLight.m_pLightImage;

						int arrayIndex = tiledLights->InsertTexture(pProjTexture, mipFactor, tiledLights->m_spotTexAtlas, -1);
						if (arrayIndex < 0)
							continue;  // Skip light

						lightShadeInfo.resIndex = arrayIndex;
						lightShadeInfo.resMipClamp0 = lightShadeInfo.resMipClamp1 = tiledLights->m_spotTexAtlas.items[arrayIndex].lowestRenderableMip;
					}

					// Prevent culling errors for frustums with large FOVs by slightly enlarging the frustum
					const float frustumAngleDelta = renderLight.m_fLightFrustumAngle > 50 ? 7.5f : 0.0f;

					Matrix34 objMat = renderLight.m_ObjMatrix;
					objMat.m03 = objMat.m13 = objMat.m23 = 0;  // Remove translation
					Vec3 lightDir = objMat * Vec3(-1, 0, 0);
					lightCullInfo.volumeParams0 = Vec4(lightDir.x, lightDir.y, lightDir.z, 0) * matView;
					lightCullInfo.volumeParams0.w = renderLight.m_fRadius * tanf(DEG2RAD(min(renderLight.m_fLightFrustumAngle + frustumAngleDelta, 89.9f))) * sqrt_2;

					Vec3 coneTip = Vec3(lightCullInfo.posRad.x, lightCullInfo.posRad.y, lightCullInfo.posRad.z);
					Vec3 coneDir = Vec3(-lightCullInfo.volumeParams0.x, -lightCullInfo.volumeParams0.y, -lightCullInfo.volumeParams0.z);
					AABB coneBounds = AABB::CreateAABBfromCone(Cone(coneTip, coneDir, renderLight.m_fRadius, lightCullInfo.volumeParams0.w));
					lightCullInfo.depthBounds = Vec2(coneBounds.min.z, coneBounds.max.z);

					Matrix44A projMatT;
					CShadowUtils::GetProjectiveTexGen(&renderLight, 0, &projMatT);

					// Translate into camera space
					projMatT.Transpose();
					const Vec4 vEye(viewInfo.cameraOrigin, 0.0f);
					Vec4 vecTranslation(vEye.Dot((Vec4&)projMatT.m00), vEye.Dot((Vec4&)projMatT.m10), vEye.Dot((Vec4&)projMatT.m20), vEye.Dot((Vec4&)projMatT.m30));
					projMatT.m03 += vecTranslation.x;
					projMatT.m13 += vecTranslation.y;
					projMatT.m23 += vecTranslation.z;
					projMatT.m33 += vecTranslation.w;

					lightShadeInfo.projectorMatrix = projMatT;
				}

				// Handle rectangular area lights
				if (isAreaLight)
				{
					lightShadeInfo.lightType = ambientLight ? CTiledLightVolumesStage::tlTypeAmbientArea : CTiledLightVolumesStage::tlTypeRegularArea;
					lightCullInfo.miscFlag = 0;

					// Pre-transform polygonal shape vertices
					Matrix33 rotationMat;
					rotationMat.SetColumn0(renderLight.m_ObjMatrix.GetColumn0().GetNormalized());
					rotationMat.SetColumn1(renderLight.m_ObjMatrix.GetColumn1().GetNormalized());
					rotationMat.SetColumn2(renderLight.m_ObjMatrix.GetColumn2().GetNormalized());

					float volumeSize = renderLight.m_fRadius + max(renderLight.m_fAreaWidth, renderLight.m_fAreaHeight);
					lightCullInfo.depthBounds = Vec2(posVS.z - volumeSize, posVS.z + volumeSize);
					
					Vec3 lightPos = renderLight.GetPosition() - viewInfo.cameraOrigin;
					Vec3 polygonPos[4];
					float areaWidth = renderLight.m_fAreaWidth * 0.5f;
					float areaHeight = renderLight.m_fAreaHeight * 0.5f;

					if (renderLight.m_nAreaShape == 1) // Rectangular light source
					{
						polygonPos[0] = rotationMat.TransformVector(Vec3(-areaWidth, 0, -areaHeight)) + lightPos;
						polygonPos[1] = rotationMat.TransformVector(Vec3(areaWidth, 0, -areaHeight)) + lightPos;
						polygonPos[2] = rotationMat.TransformVector(Vec3(areaWidth, 0, areaHeight)) + lightPos;
						polygonPos[3] = rotationMat.TransformVector(Vec3(-areaWidth, 0, areaHeight)) + lightPos;
					}
					else
					{
						Vec3 ex = rotationMat.TransformVector(Vec3(1, 0, 0) * areaWidth  * 0.5f);
						Vec3 ey = rotationMat.TransformVector(Vec3(0, 0, 1) * areaHeight * 0.5f);
						polygonPos[0] = lightPos - ex - ey;
						polygonPos[1] = lightPos + ex - ey;
						polygonPos[2] = lightPos + ex + ey;
						polygonPos[3] = lightPos - ex + ey;
					}

					Matrix44 areaLightParams;
					areaLightParams.SetRow4(0, Vec4(polygonPos[0].x, polygonPos[0].y, polygonPos[0].z, renderLight.m_nAreaShape));
					areaLightParams.SetRow4(1, Vec4(polygonPos[1].x, polygonPos[1].y, polygonPos[1].z, renderLight.m_bAreaTwoSided));
					areaLightParams.SetRow4(2, Vec4(polygonPos[2].x, polygonPos[2].y, polygonPos[2].z, renderLight.m_bAreaTextured));
					areaLightParams.SetRow4(3, Vec4(polygonPos[3].x, polygonPos[3].y, polygonPos[3].z, 0));

					lightShadeInfo.resIndex = lightShadeInfo.resNoIndex;
					{
						CTexture* pAreaTexture = (CTexture*)renderLight.m_pLightImage;

						int arrayIndex = tiledLights->InsertTexture(pAreaTexture, mipFactor, tiledLights->m_spotTexAtlas, -1);
						if (arrayIndex < 0)
							continue;  // Skip light

						lightShadeInfo.resIndex = arrayIndex;
						lightShadeInfo.resMipClamp0 = lightShadeInfo.resMipClamp1 = tiledLights->m_spotTexAtlas.items[arrayIndex].lowestRenderableMip;
					}

					lightShadeInfo.projectorMatrix = areaLightParams;
				}

				// Handle shadow casters
				if (!ambientLight && (renderLight.m_Flags & DLF_CASTSHADOW_MAPS))
				{
					int32 numDLights = pRenderView->GetDynamicLightsCount();
					int32 frustumIdx = lightIdx + numDLights;

					auto& SMFrustums = pRenderView->GetShadowFrustumsForLight(frustumIdx);

					if (!SMFrustums.empty())
					{
						ShadowMapFrustum& firstFrustum = *SMFrustums.front()->pFrustum;
						assert(firstFrustum.bUseShadowsPool);

						int32 numSides = firstFrustum.bOmniDirectionalShadow ? 6 : 1;
						float kernelSize = firstFrustum.bOmniDirectionalShadow ? 2.5f : 1.5f;

						if (numTileLights + numSides > MaxNumTileLights)
							continue;  // Skip light

						static ICVar* pShadowAtlasResCVar = iConsole->GetCVar("e_ShadowsPoolSize");
						const Vec2 shadowParams = Vec2(kernelSize * ((float)firstFrustum.nTexSize / (float)pShadowAtlasResCVar->GetIVal()), firstFrustum.fDepthConstBias);

						const Vec3 cubeDirs[6] = { Vec3(-1, 0, 0), Vec3(1, 0, 0), Vec3(0, -1, 0), Vec3(0, 1, 0), Vec3(0, 0, -1), Vec3(0, 0, 1) };

						for (int32 side = 0; side < numSides; ++side)
						{
							CShadowUtils::SShadowsSetupInfo shadowsSetup = rd->ConfigShadowTexgen(pRenderView, &firstFrustum, side);
							Matrix44A shadowMat = shadowsSetup.ShadowMat;
							const float invFrustumFarPlaneDistance = shadowsSetup.RecpFarDist;

							// Translate into camera space
							const Vec4 vEye(viewInfo.cameraOrigin, 0.0f);
							Vec4 vecTranslation(vEye.Dot((Vec4&)shadowMat.m00), vEye.Dot((Vec4&)shadowMat.m10), vEye.Dot((Vec4&)shadowMat.m20), vEye.Dot((Vec4&)shadowMat.m30));
							shadowMat.m03 += vecTranslation.x;
							shadowMat.m13 += vecTranslation.y;
							shadowMat.m23 += vecTranslation.z;
							shadowMat.m33 += vecTranslation.w;

							// Pre-multiply by inverse frustum far plane distance
							(Vec4&)shadowMat.m20 *= invFrustumFarPlaneDistance;

							Vec3 cubeDir = cubeDirs[side];
							Vec4 spotParamsVS = Vec4(cubeDir.x, cubeDir.y, cubeDir.z, 0) * matView;

							// slightly enlarge the frustum to prevent culling errors
							spotParamsVS.w = renderLight.m_fRadius * tanf(DEG2RAD(45.0f + 14.5f)) * sqrt_2;

							Vec3 coneTip = Vec3(lightCullInfo.posRad.x, lightCullInfo.posRad.y, lightCullInfo.posRad.z);
							Vec3 coneDir = Vec3(-spotParamsVS.x, -spotParamsVS.y, -spotParamsVS.z);
							AABB coneBounds = AABB::CreateAABBfromCone(Cone(coneTip, coneDir, renderLight.m_fRadius, spotParamsVS.w));
							Vec2 depthBoundsVS = Vec2(coneBounds.min.z, coneBounds.max.z);
							Vec2 sideShadowParams = firstFrustum.ShouldSampleSide(side) ? shadowParams : Vec2(ZERO);

							if (side == 0)
							{
								lightShadeInfo.shadowParams = sideShadowParams;
								lightShadeInfo.shadowMatrix = shadowMat;
								lightShadeInfo.shadowMaskIndex = renderLight.m_ShadowMaskIndex;

								if (numSides > 1)
								{
									lightCullInfo.volumeType = CTiledLightVolumesStage::tlVolumeCone;
									lightShadeInfo.lightType = CTiledLightVolumesStage::tlTypeRegularPointFace;
									lightShadeInfo.resIndex = side;
									lightShadeInfo.resMipClamp0 = 0;
									lightShadeInfo.resMipClamp1 = 0;
									lightCullInfo.volumeParams0 = spotParamsVS;
									lightCullInfo.depthBounds = depthBoundsVS;
									lightCullInfo.miscFlag = 0;
								}
							}
							else
							{
								// Split point light
								++numTileLights;
								tileLightsCull[numTileLights] = lightCullInfo;
								tileLightsShade[numTileLights] = lightShadeInfo;
								tileLightsShade[numTileLights].shadowParams = sideShadowParams;
								tileLightsShade[numTileLights].shadowMatrix = shadowMat;
								tileLightsShade[numTileLights].resIndex = side;
								tileLightsShade[numTileLights].resMipClamp0 = 0;
								tileLightsShade[numTileLights].resMipClamp1 = 0;
								tileLightsCull[numTileLights].volumeParams0 = spotParamsVS;
								tileLightsCull[numTileLights].depthBounds = depthBoundsVS;
							}
						}
					}
				}
			}

			// Add current light
			++numTileLights;
			++numValidRenderLights;
		}
	}

	// Invalidate last light in case it got skipped
	if (numTileLights < MaxNumTileLights)
	{
		ZeroMemory(&tileLightsCull[numTileLights], sizeof(CTiledLightVolumesStage::STiledLightCullInfo));
		ZeroMemory(&tileLightsShade[numTileLights], sizeof(CTiledLightVolumesStage::STiledLightShadeInfo));
	}

	// NOTE: Update full light buffer, because there are "num-threads" checks for Zero-struct size needs to be aligned to that (0,64,128,192,255 are the only possible ones for 64 threat-group size)
	const size_t tileLightsCullUploadSize = CDeviceBufferManager::AlignBufferSizeForStreaming(sizeof(CTiledLightVolumesStage::STiledLightCullInfo) * std::min(MaxNumTileLights, Align(numTileLights, 64) + 64));
	const size_t tileLightsShadeUploadSize = CDeviceBufferManager::AlignBufferSizeForStreaming(sizeof(CTiledLightVolumesStage::STiledLightShadeInfo) * std::min(MaxNumTileLights, Align(numTileLights, 64) + 64));

	m_numTileLights = numTileLights;

	m_lightCullInfoBuf.UpdateBufferContent(tileLightsCull, tileLightsCullUploadSize);
	m_LightShadeInfoBuf.UpdateBufferContent(tileLightsShade, tileLightsShadeUploadSize);

	tiledLights->UploadTextures(tiledLights->m_specularProbeAtlas);
	tiledLights->UploadTextures(tiledLights->m_diffuseProbeAtlas);
	tiledLights->UploadTextures(tiledLights->m_spotTexAtlas);

	// Update global env probes for analytical volumetric fog
	{
		I3DEngine* pEng = gEnv->p3DEngine;
		Vec3 fogAlbedo(0.0f, 0.0f, 0.0f);
		pEng->GetGlobalParameter(E3DPARAM_VOLFOG2_COLOR, fogAlbedo);
		colorGlobalEnvProbe0 = colorGlobalEnvProbe0.CompMul(fogAlbedo);
		colorGlobalEnvProbe1 = colorGlobalEnvProbe1.CompMul(fogAlbedo);
		m_globalEnvProbeParam0 = Vec4(colorGlobalEnvProbe0, attenuationGlobalEnvProbe0);
		m_globalEnvProbeParam1 = Vec4(colorGlobalEnvProbe1, attenuationGlobalEnvProbe1);

		// No need to call AddRef() because SRenderLight holds the reference to m_pDiffuseCubemap within current frame.
		m_globalEnvProveTex0 = texGlobalEnvProbe0;
		m_globalEnvProveTex1 = texGlobalEnvProbe1;
	}
}

void CVolumetricFogStage::ExecuteBuildLightListGrid(const SScopedComputeCommandList& commandList)
{
	//PROFILE_LABEL_SCOPE("BUILD_CLUSTERED_LIGHT_GRID");

	const float ndist = GetCurrentViewInfo().nearClipPlane;
	const float fdist = GetCurrentViewInfo().farClipPlane;
	const auto& projMat = GetCurrentViewInfo().projMatrix;

	const float numTileLights = static_cast<float>(m_numTileLights);
	const int32 nScreenWidth = m_pInscatteringVolume->GetWidth();
	const int32 nScreenHeight = m_pInscatteringVolume->GetHeight();
	const int32 volumeDepth = m_pInscatteringVolume->GetDepth();

	CShader* pShader = CShaderMan::s_shDeferredShading;
	auto& pass = m_passBuildLightListGrid;

	if (pass.IsDirty())
	{
		static CCryNameTSCRC techName("BuildLightListGrid");
		pass.SetTechnique(pShader, techName, 0);

		pass.SetOutputUAV(0, &m_lightGridBuf);
		pass.SetOutputUAV(1, &m_lightCountBuf);

		pass.SetBuffer(0, &m_lightCullInfoBuf);
	}

	pass.BeginConstantUpdate();

	const float fScreenWidth = static_cast<float>(nScreenWidth);
	const float fScreenHeight = static_cast<float>(nScreenHeight);
	static CCryNameR paramScreenSize("ScreenSize");
	const Vec4 vParamScreenSize(fScreenWidth, fScreenHeight, 1.0f / fScreenWidth, 1.0f / fScreenHeight);
	pass.SetConstant(paramScreenSize, vParamScreenSize);

	SD3DPostEffectsUtils::UpdateFrustumCorners(m_graphicsPipeline);
	static CCryNameR paramFrustumTL("FrustumTL");
	const Vec4 vParamFrustumTL(SD3DPostEffectsUtils::m_vLT.x, SD3DPostEffectsUtils::m_vLT.y, SD3DPostEffectsUtils::m_vLT.z, 0);
	pass.SetConstant(paramFrustumTL, vParamFrustumTL);
	static CCryNameR paramFrustumTR("FrustumTR");
	const Vec4 vParamFrustumTR(SD3DPostEffectsUtils::m_vRT.x, SD3DPostEffectsUtils::m_vRT.y, SD3DPostEffectsUtils::m_vRT.z, 0);
	pass.SetConstant(paramFrustumTR, vParamFrustumTR);
	static CCryNameR paramFrustumBL("FrustumBL");
	const Vec4 vParamFrustumBL(SD3DPostEffectsUtils::m_vLB.x, SD3DPostEffectsUtils::m_vLB.y, SD3DPostEffectsUtils::m_vLB.z, 0);
	pass.SetConstant(paramFrustumBL, vParamFrustumBL);

	static CCryNameR paramNameScreenInfo("ScreenInfo");
	const Vec4 screenInfo(ndist, fdist, 0.0f, static_cast<float>(volumeDepth));
	pass.SetConstant(paramNameScreenInfo, screenInfo);

	static CCryNameR paramProj("ProjParams");
	const Vec4 vParamProj(projMat.m00, projMat.m11, projMat.m20, projMat.m21);
	pass.SetConstant(paramProj, vParamProj);

	const uint32 tileSizeX = 4;
	const uint32 tileSizeY = 4;
	const uint32 tileSizeZ = 4;
	const uint32 dispatchSizeX = (nScreenWidth / tileSizeX) + (nScreenWidth % tileSizeX > 0 ? 1 : 0);
	const uint32 dispatchSizeY = (nScreenHeight / tileSizeY) + (nScreenHeight % tileSizeY > 0 ? 1 : 0);
	const uint32 dispatchSizeZ = (volumeDepth / tileSizeZ) + (volumeDepth % tileSizeZ > 0 ? 1 : 0);

	static CCryNameR paramDispatchSize("DispatchSize");
	const Vec4 vParamDispatchSize(static_cast<float>(dispatchSizeX), static_cast<float>(dispatchSizeY), static_cast<float>(dispatchSizeZ), numTileLights);
	pass.SetConstant(paramDispatchSize, vParamDispatchSize);

	pass.SetDispatchSize(dispatchSizeX, dispatchSizeY, dispatchSizeZ);
	pass.PrepareResourcesForUse(commandList);
	pass.Execute(commandList);
}

void CVolumetricFogStage::ExecuteDownscaledDepth(const SScopedComputeCommandList& commandList)
{
	//PROFILE_LABEL_SCOPE("DOWNSCALE_DEPTH");

	CShader* pShader = CShaderMan::s_shDeferredShading;

	{
		const int32 nScreenWidth = m_pMaxDepthTemp->GetWidth();
		const int32 nScreenHeight = m_pMaxDepthTemp->GetHeight();
		const int32 nSrcTexWidth = m_graphicsPipelineResources.m_pTexLinearDepthScaled[0]->GetWidth();

		auto& pass = m_passDownscaleDepthHorizontal;

		if (pass.IsDirty())
		{
			static CCryNameTSCRC techName("StoreDownscaledMaxDepthHorizontal");
			pass.SetTechnique(pShader, techName, 0);

			pass.SetOutputUAV(0, m_pMaxDepthTemp);

			pass.SetTexture(0, m_graphicsPipelineResources.m_pTexLinearDepthScaled[0]);

			pass.SetSampler(0, EDefaultSamplerStates::PointClamp);
		}

		pass.BeginConstantUpdate();

		const float destW = static_cast<float>(nScreenWidth);
		const float srcW = static_cast<float>(nSrcTexWidth);
		static CCryNameR paramDispatchParams("maxDepthDispatchParams");
		const Vec4 vParamDispatchParams((srcW / destW), (destW / srcW), 0.0f, 0.0f);
		pass.SetConstant(paramDispatchParams, vParamDispatchParams);

		const float fScreenWidth = static_cast<float>(nScreenWidth);
		const float fScreenHeight = static_cast<float>(nScreenHeight);
		static CCryNameR paramScreenSize("ScreenSize");
		const Vec4 vParamScreenSize(fScreenWidth, fScreenHeight, 1.0f / fScreenWidth, 1.0f / fScreenHeight);
		pass.SetConstant(paramScreenSize, vParamScreenSize);

		const uint32 tileSizeX = 8;
		const uint32 tileSizeY = 8;
		const uint32 dispatchSizeX = (nScreenWidth / tileSizeX) + (nScreenWidth % tileSizeX > 0 ? 1 : 0);
		const uint32 dispatchSizeY = (nScreenHeight / tileSizeY) + (nScreenHeight % tileSizeY > 0 ? 1 : 0);
		pass.SetDispatchSize(dispatchSizeX, dispatchSizeY, 1);
		pass.PrepareResourcesForUse(commandList);
		pass.Execute(commandList);
	}

	{
		const int32 nScreenWidth = m_pMaxDepth->GetWidth();
		const int32 nScreenHeight = m_pMaxDepth->GetHeight();
		const int32 nSrcTexWidth = m_pMaxDepthTemp->GetWidth();

		auto& pass = m_passDownscaleDepthVertical;

		if (pass.IsDirty())
		{
			static CCryNameTSCRC techName("StoreDownscaledMaxDepthVertical");
			pass.SetTechnique(pShader, techName, 0);

			pass.SetOutputUAV(0, m_pMaxDepth);

			pass.SetTexture(0, m_pMaxDepthTemp);

			pass.SetSampler(0, EDefaultSamplerStates::PointClamp);
		}

		pass.BeginConstantUpdate();

		const float destW = static_cast<float>(nScreenWidth);
		const float srcW = static_cast<float>(nSrcTexWidth);
		static CCryNameR paramDispatchParams("maxDepthDispatchParams");
		const Vec4 vParamDispatchParams((srcW / destW), (destW / srcW), 0.0f, 0.0f);
		pass.SetConstant(paramDispatchParams, vParamDispatchParams);

		const float fScreenWidth = static_cast<float>(nScreenWidth);
		const float fScreenHeight = static_cast<float>(nScreenHeight);
		static CCryNameR paramScreenSize("ScreenSize");
		const Vec4 vParamScreenSize(fScreenWidth, fScreenHeight, 1.0f / fScreenWidth, 1.0f / fScreenHeight);
		pass.SetConstant(paramScreenSize, vParamScreenSize);

		const uint32 tileSizeX = 8;
		const uint32 tileSizeY = 8;
		const uint32 dispatchSizeX = (nScreenWidth / tileSizeX) + (nScreenWidth % tileSizeX > 0 ? 1 : 0);
		const uint32 dispatchSizeY = (nScreenHeight / tileSizeY) + (nScreenHeight % tileSizeY > 0 ? 1 : 0);
		pass.SetDispatchSize(dispatchSizeX, dispatchSizeY, 1);
		pass.PrepareResourcesForUse(commandList);
		pass.Execute(commandList);
	}
}

void CVolumetricFogStage::GenerateFogVolumeList()
{
	const CRenderView* pRenderView = RenderView();
	CRY_ASSERT(pRenderView);

	const auto& viewInfo = GetCurrentViewInfo();

	// Prepare view matrix with flipped z-axis
	Matrix44A matView = viewInfo.viewMatrix;
	matView.m02 *= -1;
	matView.m12 *= -1;
	matView.m22 *= -1;
	matView.m32 *= -1;

	Vec3 volumetricFogRaymarchEnd(0.0f, 0.0f, 0.0f);
	gEnv->p3DEngine->GetGlobalParameter(E3DPARAM_VOLFOG2_CTRL_PARAMS, volumetricFogRaymarchEnd);

	const Vec3 cameraFront = viewInfo.cameraVZ.GetNormalized();
	const AABB aabbInObj(1.0f);

	// NOTE: Get aligned stack-space (pointer and size aligned to manager's alignment requirement)
	CryStackAllocWithSizeVector(SFogVolumeCullInfo, CRenderView::MaxFogVolumeNum, cullInfoArray, CDeviceBufferManager::AlignBufferSizeForStreaming);
	CryStackAllocWithSizeVector(SFogVolumeInjectInfo, CRenderView::MaxFogVolumeNum, injectInfoArray, CDeviceBufferManager::AlignBufferSizeForStreaming);

	uint32 numFogVol = 0;

	for (uint32 type = 0; type < IFogVolumeRenderNode::eFogVolumeType_Count; ++type)
	{
		const auto& srcArray = pRenderView->GetFogVolumes(IFogVolumeRenderNode::eFogVolumeType(type));
		for (auto& fvol : srcArray)
		{
			if (numFogVol >= CRenderView::MaxFogVolumeNum)
			{
				break;
			}

			// calculate depth bounds of FogVolume.
			// reusing light depth bounds code from CDeferredShading::GetLightDepthBounds().
			// This is not optimal for a box.
			const Matrix34 temp = fvol.m_matWSInv.GetInverted();
			const AABB aabbInWS = AABB::CreateTransformedAABB(temp, aabbInObj);
			const float fRadius = aabbInWS.GetRadius();
			const Vec3 pBounds = cameraFront * fRadius;
			const Vec3 pMin = fvol.m_center + pBounds;
			float fMinW = viewInfo.WorldToCameraZ(pMin);
			fMinW = max(-fMinW, 0.000001f);

			// not needed to be injected when FogVolume is out of volume texture.
			if (fMinW > volumetricFogRaymarchEnd.x)
			{
				continue;
			}

			SFogVolumeCullInfo& cullInfo = cullInfoArray[numFogVol];
			SFogVolumeInjectInfo& injectInfo = injectInfoArray[numFogVol];

			const Vec4 posVS = Vec4(fvol.m_center, 1.0f) * matView;
			cullInfo.posRad = Vec4(posVS.x, posVS.y, posVS.z, fRadius);

			const Vec4 u0 = Vec4(temp.GetColumn0().GetNormalized(), 0.0f) * matView;
			const Vec4 u1 = Vec4(temp.GetColumn1().GetNormalized(), 0.0f) * matView;
			const Vec4 u2 = Vec4(temp.GetColumn2().GetNormalized(), 0.0f) * matView;
			const Vec3 size = fvol.m_localAABB.GetSize() * 0.5f;
			cullInfo.volumeParams0 = Vec4(u0.x, u0.y, u0.z, size.x);
			cullInfo.volumeParams1 = Vec4(u1.x, u1.y, u1.z, size.y);
			cullInfo.volumeParams2 = Vec4(u2.x, u2.y, u2.z, size.z);

			const uint32 nData = fvol.m_stencilRef;
			injectInfo.miscFlag = fvol.m_volumeType | ((nData & 0xFF) << 1) | (fvol.m_affectsThisAreaOnly << 9);

			injectInfo.fogColor.x = fvol.m_fogColor.r;
			injectInfo.fogColor.y = fvol.m_fogColor.g;
			injectInfo.fogColor.z = fvol.m_fogColor.b;

			const float globalDensity = fvol.m_globalDensity * 0.1f;// scale density to volumetric fog.
			injectInfo.globalDensity = globalDensity;

			injectInfo.fogVolumePos = fvol.m_center;

			injectInfo.heightFalloffBasePoint = fvol.m_heightFallOffBasePoint;

			const float softEdgeLerp = (fvol.m_softEdgesLerp.x > 0.0f) ? fvol.m_softEdgesLerp.x : 0.0001f;
			injectInfo.invSoftEdgeLerp = 1.0f / softEdgeLerp;

			const Vec3 cHeightFallOffDirScaledVec(fvol.m_heightFallOffDirScaled * 0.015625f);  // scale fall off ramp to volumetric fog.
			injectInfo.heightFallOffDirScaled = cHeightFallOffDirScaledVec;

			injectInfo.densityOffset = fvol.m_densityOffset;

			float rampDist = fvol.m_rampParams.y - fvol.m_rampParams.x;
			rampDist = rampDist < 0.1f ? 0.1f : rampDist;
			const float invRampDist = 1.0f / rampDist;
			const Vec4 cRampParams(invRampDist, -fvol.m_rampParams.x * invRampDist, fvol.m_rampParams.z, -fvol.m_rampParams.z + 1.0f);
			injectInfo.rampParams = cRampParams;

			injectInfo.windOffset = fvol.m_windOffset;

			injectInfo.noiseElapsedTime = fvol.m_noiseElapsedTime;

			injectInfo.noiseSpatialFrequency = fvol.m_noiseFreq;

			const float normalizeFactor = (1.0f / (1.0f + 0.5f));
			injectInfo.noiseScale = fvol.m_noiseScale * normalizeFactor;

			injectInfo.eyePosInOS = fvol.m_eyePosInOS;

			injectInfo.noiseOffset = fvol.m_noiseOffset;

			injectInfo.emission = fvol.m_emission;
			injectInfo.padding = 0.0f;

			injectInfo.worldToObjMatrix = fvol.m_matWSInv;

			++numFogVol;
		}
	}

	m_numFogVolumes = numFogVol;

	// Minimize transfer size
	const size_t cullInfoArrayUploadSize = CDeviceBufferManager::AlignBufferSizeForStreaming(sizeof(SFogVolumeCullInfo) * m_numFogVolumes);
	const size_t injectInfoArrayUploadSize = CDeviceBufferManager::AlignBufferSizeForStreaming(sizeof(SFogVolumeInjectInfo) * m_numFogVolumes);

	m_fogVolumeCullInfoBuf.UpdateBufferContent(cullInfoArray, cullInfoArrayUploadSize);
	m_fogVolumeInjectInfoBuf.UpdateBufferContent(injectInfoArray, injectInfoArrayUploadSize);
}

void CVolumetricFogStage::ExecuteInjectFogDensity(const SScopedComputeCommandList& commandList)
{
	auto* pClipVolumes = m_graphicsPipeline.GetStage<CClipVolumesStage>();

	const auto& projMat = GetCurrentViewInfo().projMatrix;
	auto* pClipVolumeStencilTex = pClipVolumes->GetClipVolumeStencilVolumeTexture();

	const float numFogVolumes = static_cast<float>(m_numFogVolumes);
	const uint32 nScreenWidth = m_pVolFogBufDensity->GetWidth();
	const uint32 nScreenHeight = m_pVolFogBufDensity->GetHeight();
	const uint32 volumeDepth = m_pVolFogBufDensity->GetDepth();

	CShader* pShader = CShaderMan::s_shDeferredShading;
	auto& pass = m_passInjectFogDensity;

	if (pass.IsDirty())
	{
		static CCryNameTSCRC techName("InjectFogDensity");
		pass.SetTechnique(pShader, techName, 0);

		pass.SetOutputUAV(0, m_pVolFogBufDensity);
		pass.SetOutputUAV(1, m_pVolFogBufDensityColor);
		pass.SetOutputUAV(2, m_pVolFogBufEmissive);

		pass.SetTexture(0, pClipVolumeStencilTex, EDefaultResourceViews::StencilOnly);
		pass.SetTexture(1, m_pNoiseTexture);

		pass.SetBuffer(8, &m_fogVolumeInjectInfoBuf);
		pass.SetBuffer(9, &m_fogVolumeCullInfoBuf);
		pass.SetTexture(10, m_pMaxDepth);

		pass.SetSampler(0, EDefaultSamplerStates::BilinearWrap);

		pass.SetBuffer(23, pClipVolumes->GetClipVolumeInfoBuffer());

		pass.SetInlineConstantBuffer(eConstantBufferShaderSlot_PerView, m_graphicsPipeline.GetMainViewConstantBuffer());
	}

	pass.BeginConstantUpdate();

	float fScreenWidth = static_cast<float>(nScreenWidth);
	float fScreenHeight = static_cast<float>(nScreenHeight);
	static CCryNameR paramScreenSize("ScreenSize");
	const Vec4 vParamScreenSize(fScreenWidth, fScreenHeight, 1.0f / fScreenWidth, 1.0f / fScreenHeight);
	pass.SetConstant(paramScreenSize, vParamScreenSize);

	SD3DPostEffectsUtils::UpdateFrustumCorners(m_graphicsPipeline);
	static CCryNameR paramFrustumTL("FrustumTL");
	const Vec4 vParamFrustumTL(SD3DPostEffectsUtils::m_vLT.x, SD3DPostEffectsUtils::m_vLT.y, SD3DPostEffectsUtils::m_vLT.z, 0);
	pass.SetConstant(paramFrustumTL, vParamFrustumTL);
	static CCryNameR paramFrustumTR("FrustumTR");
	const Vec4 vParamFrustumTR(SD3DPostEffectsUtils::m_vRT.x, SD3DPostEffectsUtils::m_vRT.y, SD3DPostEffectsUtils::m_vRT.z, 0);
	pass.SetConstant(paramFrustumTR, vParamFrustumTR);
	static CCryNameR paramFrustumBL("FrustumBL");
	const Vec4 vParamFrustumBL(SD3DPostEffectsUtils::m_vLB.x, SD3DPostEffectsUtils::m_vLB.y, SD3DPostEffectsUtils::m_vLB.z, 0);
	pass.SetConstant(paramFrustumBL, vParamFrustumBL);

	static CCryNameR paramInjectExponentialHeightFogParams("InjectFogDensityParams");
	const Vec4 vInjectExponentialHeightFogParams(static_cast<float>(volumeDepth), numFogVolumes, 0.0f, 0.0f);
	pass.SetConstant(paramInjectExponentialHeightFogParams, vInjectExponentialHeightFogParams);

	static CCryNameR paramProj("ProjParams");
	const Vec4 vParamProj(projMat.m00, projMat.m11, projMat.m20, projMat.m21);
	pass.SetConstant(paramProj, vParamProj);

	const uint32 tileSizeX = 4;
	const uint32 tileSizeY = 4;
	const uint32 tileSizeZ = 4;
	const uint32 dispatchSizeX = (nScreenWidth / tileSizeX) + (nScreenWidth % tileSizeX > 0 ? 1 : 0);
	const uint32 dispatchSizeY = (nScreenHeight / tileSizeY) + (nScreenHeight % tileSizeY > 0 ? 1 : 0);
	const uint32 dispatchSizeZ = (volumeDepth / tileSizeZ) + (volumeDepth % tileSizeZ > 0 ? 1 : 0);
	pass.SetDispatchSize(dispatchSizeX, dispatchSizeY, dispatchSizeZ);
	pass.PrepareResourcesForUse(commandList);
	pass.Execute(commandList);
}

bool CVolumetricFogStage::ReplaceShadowMapWithStaticShadowMap(CShadowUtils::SShadowCascades& shadowCascades, uint32 shadowCascadeSlot) const
{
	CRenderView* pRenderView = RenderView();
	CRY_ASSERT(pRenderView);
	CRY_ASSERT(shadowCascadeSlot < CShadowUtils::MaxCascadesNum);

	if (shadowCascadeSlot >= CShadowUtils::MaxCascadesNum || !pRenderView)
	{
		return false;
	}

	const int32 nSunFrustumID = 0;
	auto& SMFrustums = pRenderView->GetShadowFrustumsForLight(nSunFrustumID);

	for (auto pFrustumToRender : SMFrustums)
	{
		if (pFrustumToRender
		    && pFrustumToRender->pFrustum
		    && pFrustumToRender->pFrustum->m_eFrustumType == ShadowMapFrustum::e_GsmCached
		    && pFrustumToRender->pFrustum->pDepthTex
		    && pFrustumToRender->pFrustum->pDepthTex != CRendererResources::s_ptexFarPlane)
		{
			//CShadowUtils::SShadowsSetupInfo shadowsSetup = gcpRendD3D->ConfigShadowTexgen(pRenderView, pFrustumToRender->pFrustum, -1, true);
			shadowCascades.pShadowMap[shadowCascadeSlot] = pFrustumToRender->pFrustum->pDepthTex;

			return true;
		}
	}

	return false;
}

void CVolumetricFogStage::ExecuteInjectInscatteringLight(const SScopedComputeCommandList& commandList)
{
	CRenderView* pRenderView = RenderView();
	CRY_ASSERT(pRenderView);

	// calculate fog density and accumulate in-scattering lighting along view ray.
	//PROFILE_LABEL_SCOPE("INJECT_VOLUMETRIC_FOG_INSCATTERING");

	auto* tiledLights = m_graphicsPipeline.GetStage<CTiledLightVolumesStage>();
	auto* clipVolumes = m_graphicsPipeline.GetStage<CClipVolumesStage>();
	auto* pClipVolumeStencilTex = clipVolumes->GetClipVolumeStencilVolumeTexture();
	auto* pShadowMapStage = m_graphicsPipeline.GetStage<CShadowMapStage>();

	const uint32 nScreenWidth = m_pInscatteringVolume->GetWidth();
	const uint32 nScreenHeight = m_pInscatteringVolume->GetHeight();
	const uint32 volumeDepth = m_pInscatteringVolume->GetDepth();

	CShader* pShader = CShaderMan::s_shDeferredShading;
	auto& pass = m_passInjectInscattering;

	uint64 rtMask = 0;

	// set sampling quality
	const uint64 quality = g_HWSR_MaskBit[HWSR_SAMPLE4];
	const uint64 quality1 = g_HWSR_MaskBit[HWSR_SAMPLE5];
	switch (CRendererCVars::CV_r_VolumetricFogSample)
	{
	case 1:
		rtMask |= quality;
		break;
	case 2:
		rtMask |= quality1;
		break;
	default:
		break;
	}

	// set shadow quality
	const uint64 shadowQuality = g_HWSR_MaskBit[HWSR_LIGHTVOLUME0];
	const uint64 shadowQuality1 = g_HWSR_MaskBit[HWSR_LIGHTVOLUME1];
	switch (CRendererCVars::CV_r_VolumetricFogShadow)
	{
	case 1:
		rtMask |= shadowQuality;
		break;
	case 2:
		rtMask |= shadowQuality1;
		break;
	case 3:
		rtMask |= shadowQuality | shadowQuality1;
		break;
	default:
		break;
	}

	// set texture format toggle
	if (m_seperateDensity)
	{
		rtMask |= g_HWSR_MaskBit[HWSR_SAMPLE3];
	}

	// setup shadow maps.
	CShadowUtils::SShadowCascades cascades;
	const bool bSunShadow = CShadowUtils::SetupShadowsForFog(cascades, pRenderView);

	m_pCloudShadowTex = cascades.pCloudShadowMap;

	// explicitly replace the shadow map with the static shadow map if static shadow map replace above 5th cascade.
	bool bStaticShadowMap = bSunShadow && (CRendererCVars::CV_r_ShadowsCache > 0);
	if (bStaticShadowMap
	    && (CRendererCVars::CV_r_ShadowsCache >= 5)
	    && (CRendererCVars::CV_r_VolumetricFogDownscaledSunShadow > 0))
	{
		const int32 staticShadowMapSlot = 3; // explicitly set to 4th cascade.
		bStaticShadowMap = ReplaceShadowMapWithStaticShadowMap(cascades, staticShadowMapSlot);
	}

	// set downscaled sun shadow maps
	const uint64 shadowMode0 = g_HWSR_MaskBit[HWSR_SAMPLE0];
	const uint64 shadowMode1 = g_HWSR_MaskBit[HWSR_SAMPLE1];
	if (CRendererCVars::CV_r_VolumetricFogDownscaledSunShadow == 1)
	{
		// replace the first and the second cascades, and replaced the third with static shadow map if possible.
		rtMask |= bStaticShadowMap ? (shadowMode0 | shadowMode1) : shadowMode0;

		cascades.pShadowMap[0] = m_pDownscaledShadow[0];
		cascades.pShadowMap[1] = m_pDownscaledShadow[1];
	}
	else if (CRendererCVars::CV_r_VolumetricFogDownscaledSunShadow > 0)
	{
		// replace the first, the second, and the third cascades.
		rtMask |= shadowMode1;

		cascades.pShadowMap[0] = m_pDownscaledShadow[0];
		cascades.pShadowMap[1] = m_pDownscaledShadow[1];
		cascades.pShadowMap[2] = m_pDownscaledShadow[2];
	}

	uint32 inputFlag = 0;
	inputFlag |= bSunShadow ? BIT(0) : 0;
	inputFlag |= bStaticShadowMap ? BIT(1) : 0;

	int spotLightAtlasID = tiledLights->GetProjectedLightAtlas()->GetID();
	if (pass.IsDirty(inputFlag, rtMask, spotLightAtlasID))
	{
		static CCryNameTSCRC techName("InjectVolumetricInscattering");
		pass.SetTechnique(pShader, techName, rtMask);

		pass.SetOutputUAV(0, m_pInscatteringVolume);

		CShadowUtils::SetShadowSamplingContextToRenderPass(pass, 0, -1, -1, 1, -1);

		pass.SetInlineConstantBuffer(eConstantBufferShaderSlot_PerView, m_graphicsPipeline.GetMainViewConstantBuffer());
		pass.SetSampler(3, EDefaultSamplerStates::TrilinearClamp);
		pass.SetSampler(15, EDefaultSamplerStates::TrilinearClamp);

		if (!m_seperateDensity)
		{
			pass.SetBuffer(8, &m_lightGridBuf);
			pass.SetBuffer(9, &m_lightCountBuf);
			pass.SetTexture(10, m_pMaxDepth);
			pass.SetTexture(11, pClipVolumeStencilTex, EDefaultResourceViews::StencilOnly);
			pass.SetBuffer(12, &m_LightShadeInfoBuf);
			pass.SetTexture(13, m_pVolFogBufDensityColor);
			pass.SetTexture(14, m_pVolFogBufDensity);
			pass.SetTexture(15, m_pVolFogBufEmissive);
		}
		else
		{
			pass.SetBuffer(8, &m_lightGridBuf);
			pass.SetBuffer(9, &m_lightCountBuf);
			pass.SetTexture(10, m_pMaxDepth);
			pass.SetTexture(11, pClipVolumeStencilTex, EDefaultResourceViews::StencilOnly);
			pass.SetBuffer(12, &m_LightShadeInfoBuf);
			pass.SetTexture(13, m_pVolFogBufDensityColor);
			pass.SetTexture(15, m_pVolFogBufEmissive);
		}

		pass.SetTexture(18, tiledLights->GetDiffuseProbeAtlas());
		pass.SetTexture(19, tiledLights->GetProjectedLightAtlas());
		pass.SetTexture(20, pShadowMapStage->m_pTexRT_ShadowPool);
		pass.SetBuffer(23, clipVolumes->GetClipVolumeInfoBuffer());
	}

	CShadowUtils::SetShadowCascadesToRenderPass(pass, 0, 4, cascades);

	pass.BeginConstantUpdate();

	float fScreenWidth = static_cast<float>(nScreenWidth);
	float fScreenHeight = static_cast<float>(nScreenHeight);
	static CCryNameR paramScreenSize("ScreenSize");
	const Vec4 vParamScreenSize(fScreenWidth, fScreenHeight, 1.0f / fScreenWidth, 1.0f / fScreenHeight);
	pass.SetConstant(paramScreenSize, vParamScreenSize);

	SD3DPostEffectsUtils::UpdateFrustumCorners(m_graphicsPipeline);
	static CCryNameR paramFrustumTL("FrustumTL");
	const Vec4 vParamFrustumTL(SD3DPostEffectsUtils::m_vLT.x, SD3DPostEffectsUtils::m_vLT.y, SD3DPostEffectsUtils::m_vLT.z, 0);
	pass.SetConstant(paramFrustumTL, vParamFrustumTL);
	static CCryNameR paramFrustumTR("FrustumTR");
	const Vec4 vParamFrustumTR(SD3DPostEffectsUtils::m_vRT.x, SD3DPostEffectsUtils::m_vRT.y, SD3DPostEffectsUtils::m_vRT.z, 0);
	pass.SetConstant(paramFrustumTR, vParamFrustumTR);
	static CCryNameR paramFrustumBL("FrustumBL");
	const Vec4 vParamFrustumBL(SD3DPostEffectsUtils::m_vLB.x, SD3DPostEffectsUtils::m_vLB.y, SD3DPostEffectsUtils::m_vLB.z, 0);
	pass.SetConstant(paramFrustumBL, vParamFrustumBL);

	const Vec3 sunDir = gEnv->p3DEngine->GetSunDirNormalized();
	static CCryNameR paramSunDir("vfSunDir");
	const Vec4 vParamSunDir(sunDir.x, sunDir.y, sunDir.z, 0.0f);
	pass.SetConstant(paramSunDir, vParamSunDir);

	CShadowUtils::SShadowCascadesSamplingInfo shadowSamplingInfo;
	CShadowUtils::GetShadowCascadesSamplingInfo(shadowSamplingInfo, pRenderView);
	pass.SetConstant(CCryNameR("TexGen0"), shadowSamplingInfo.shadowTexGen[0]);
	pass.SetConstant(CCryNameR("TexGen1"), shadowSamplingInfo.shadowTexGen[1]);
	pass.SetConstant(CCryNameR("TexGen2"), shadowSamplingInfo.shadowTexGen[2]);
	pass.SetConstant(CCryNameR("TexGen3"), shadowSamplingInfo.shadowTexGen[3]);
	pass.SetConstant(CCryNameR("vInvShadowMapSize"), shadowSamplingInfo.invShadowMapSize);
	pass.SetConstant(CCryNameR("fDepthTestBias"), shadowSamplingInfo.depthTestBias);
	pass.SetConstant(CCryNameR("fOneDivFarDist"), shadowSamplingInfo.oneDivFarDist);
	pass.SetConstant(CCryNameR("fKernelRadius"), shadowSamplingInfo.kernelRadius);
	pass.SetConstant(CCryNameR("CloudShadowParams"), shadowSamplingInfo.cloudShadowParams);
	pass.SetConstant(CCryNameR("CloudShadowAnimParams"), shadowSamplingInfo.cloudShadowAnimParams);
	pass.SetConstantArray(CCryNameR("irreg_kernel_2d"), shadowSamplingInfo.irregKernel2d, 8);

	Vec3 sunColor;
	gEnv->p3DEngine->GetGlobalParameter(E3DPARAM_SUN_COLOR, sunColor);
	if (CRendererCVars::CV_r_VolumetricFogSunLightCorrection == 1)
	{
		// reconstruct vanilla sun color because it's divided by pi in ConvertIlluminanceToLightColor().
		sunColor *= gf_PI;
	}
	static CCryNameR paramSunColor("SunColor");
	const Vec4 vParamSunColor(sunColor.x, sunColor.y, sunColor.z, 0.0f);
	pass.SetConstant(paramSunColor, vParamSunColor);

	Vec3 fogAlbedoColor;
	gEnv->p3DEngine->GetGlobalParameter(E3DPARAM_VOLFOG2_COLOR, fogAlbedoColor);
	static CCryNameR paramFogColor("ExponentialHeightFogColor");
	const Vec4 vParamFogColor(fogAlbedoColor.x, fogAlbedoColor.y, fogAlbedoColor.z, 0.0f);
	pass.SetConstant(paramFogColor, vParamFogColor);

	Vec3 scatteringParam;
	gEnv->p3DEngine->GetGlobalParameter(E3DPARAM_VOLFOG2_SCATTERING_PARAMS, scatteringParam);
	float k = scatteringParam.z;
	const bool bNegative = k < 0.0f ? true : false;
	k = (abs(k) > 0.99999f) ? (bNegative ? -0.99999f : 0.99999f) : k;
	static CCryNameR paramInjectInscattering("InjectInscatteringParams");
	const Vec4 vParamInjectInscattering(k, 1.0f - k * k, 0.0f, 0.0f);
	pass.SetConstant(paramInjectInscattering, vParamInjectInscattering);

	const uint32 tileSizeX = 4;
	const uint32 tileSizeY = 4;
	const uint32 tileSizeZ = 4;
	const uint32 dispatchSizeX = (nScreenWidth / tileSizeX) + (nScreenWidth % tileSizeX > 0 ? 1 : 0);
	const uint32 dispatchSizeY = (nScreenHeight / tileSizeY) + (nScreenHeight % tileSizeY > 0 ? 1 : 0);
	const uint32 dispatchSizeZ = (volumeDepth / tileSizeZ) + (volumeDepth % tileSizeZ > 0 ? 1 : 0);

	static CCryNameR paramDispatchSize("DispatchSize");
	const Vec4 vParamDispatchSize(static_cast<float>(dispatchSizeX), static_cast<float>(dispatchSizeY), static_cast<float>(dispatchSizeZ), 0.0f);
	pass.SetConstant(paramDispatchSize, vParamDispatchSize);

	pass.SetDispatchSize(dispatchSizeX, dispatchSizeY, dispatchSizeZ);
	pass.PrepareResourcesForUse(commandList);
	pass.Execute(commandList);
}

void CVolumetricFogStage::ExecuteBlurDensityVolume(const SScopedComputeCommandList& commandList)
{
	// blur density volume texture for removing jitter noise.
	//PROFILE_LABEL_SCOPE("BLUR_DENSITY");

	const float ndist = GetCurrentViewInfo().nearClipPlane;
	const float fdist = GetCurrentViewInfo().farClipPlane;

	const uint32 nScreenWidth = m_pVolFogBufDensity->GetWidth();
	const uint32 nScreenHeight = m_pVolFogBufDensity->GetHeight();
	const uint32 volumeDepth = m_pVolFogBufDensity->GetDepth();
	const uint32 bufferId = GetTemporalBufferId();

	CShader* pShader = CShaderMan::s_shDeferredShading;

	{
		auto& pass = m_passBlurDensityHorizontal[bufferId];

		if (pass.IsDirty())
		{
			static CCryNameTSCRC techName("BlurHorizontalDensityVolume");
			pass.SetTechnique(pShader, techName, 0);

			pass.SetOutputUAV(1, GetDensityTex());

			pass.SetTexture(1, m_pVolFogBufDensity);
			pass.SetTexture(2, m_pMaxDepth);

			pass.SetSampler(0, EDefaultSamplerStates::TrilinearClamp);
		}

		pass.BeginConstantUpdate();

		const float fScreenWidth = static_cast<float>(nScreenWidth);
		const float fScreenHeight = static_cast<float>(nScreenHeight);
		static CCryNameR paramScreenSize("ScreenSize");
		const Vec4 vParamScreenSize(fScreenWidth, fScreenHeight, 1.0f / fScreenWidth, 1.0f / fScreenHeight);
		pass.SetConstant(paramScreenSize, vParamScreenSize);

		static CCryNameR paramNameScreenInfo("ScreenInfo");
		const Vec4 screenInfo(ndist, fdist, 0.0f, static_cast<float>(volumeDepth));
		pass.SetConstant(paramNameScreenInfo, screenInfo);

		const uint32 tileSizeX = 8;
		const uint32 tileSizeY = 8;
		const uint32 tileSizeZ = 1;
		const uint32 dispatchSizeX = (nScreenWidth / tileSizeX) + (nScreenWidth % tileSizeX > 0 ? 1 : 0);
		const uint32 dispatchSizeY = (nScreenHeight / tileSizeY) + (nScreenHeight % tileSizeY > 0 ? 1 : 0);
		const uint32 dispatchSizeZ = (volumeDepth / tileSizeZ) + (volumeDepth % tileSizeZ > 0 ? 1 : 0);
		pass.SetDispatchSize(dispatchSizeX, dispatchSizeY, dispatchSizeZ);
		pass.PrepareResourcesForUse(commandList);
		pass.Execute(commandList);
	}

	{
		auto& pass = m_passBlurDensityVertical[bufferId];

		if (pass.IsDirty())
		{
			static CCryNameTSCRC techName("BlurVerticalDensityVolume");
			pass.SetTechnique(pShader, techName, 0);

			pass.SetOutputUAV(1, m_pVolFogBufDensity);

			pass.SetTexture(1, GetDensityTex());
			pass.SetTexture(2, m_pMaxDepth);

			pass.SetSampler(0, EDefaultSamplerStates::TrilinearClamp);
		}

		pass.BeginConstantUpdate();

		const float fScreenWidth = static_cast<float>(nScreenWidth);
		const float fScreenHeight = static_cast<float>(nScreenHeight);
		static CCryNameR paramScreenSize("ScreenSize");
		const Vec4 vParamScreenSize(fScreenWidth, fScreenHeight, 1.0f / fScreenWidth, 1.0f / fScreenHeight);
		pass.SetConstant(paramScreenSize, vParamScreenSize);

		static CCryNameR paramNameScreenInfo("ScreenInfo");
		const Vec4 screenInfo(ndist, fdist, 0.0f, static_cast<float>(volumeDepth));
		pass.SetConstant(paramNameScreenInfo, screenInfo);

		const uint32 tileSizeX = 8;
		const uint32 tileSizeY = 8;
		const uint32 tileSizeZ = 1;
		const uint32 dispatchSizeX = (nScreenWidth / tileSizeX) + (nScreenWidth % tileSizeX > 0 ? 1 : 0);
		const uint32 dispatchSizeY = (nScreenHeight / tileSizeY) + (nScreenHeight % tileSizeY > 0 ? 1 : 0);
		const uint32 dispatchSizeZ = (volumeDepth / tileSizeZ) + (volumeDepth % tileSizeZ > 0 ? 1 : 0);
		pass.SetDispatchSize(dispatchSizeX, dispatchSizeY, dispatchSizeZ);
		pass.PrepareResourcesForUse(commandList);
		pass.Execute(commandList);
	}
}

void CVolumetricFogStage::ExecuteBlurInscatterVolume(const SScopedComputeCommandList& commandList)
{
	// blur inscattering volume texture for removing jittering noise.
	//PROFILE_LABEL_SCOPE("BLUR_INSCATTERING");

	const float ndist = GetCurrentViewInfo().nearClipPlane;
	const float fdist = GetCurrentViewInfo().farClipPlane;

	const uint32 nScreenWidth = m_pInscatteringVolume->GetWidth();
	const uint32 nScreenHeight = m_pInscatteringVolume->GetHeight();
	const uint32 volumeDepth = m_pInscatteringVolume->GetDepth();
	const uint32 bufferId = GetTemporalBufferId();

	CShader* pShader = CShaderMan::s_shDeferredShading;

	// set texture format toggle
	uint64 rtMask = 0;
	if (m_seperateDensity)
		rtMask |= g_HWSR_MaskBit[HWSR_SAMPLE3];

	{
		auto& pass = m_passBlurInscatteringHorizontal[bufferId];

		if (pass.IsDirty(rtMask))
		{
			static CCryNameTSCRC techName("BlurHorizontalInscatteringVolume");
			pass.SetTechnique(pShader, techName, rtMask);

			pass.SetOutputUAV(0, GetInscatterTex());
			if (m_seperateDensity)
				pass.SetOutputUAV(1, GetDensityTex());

			pass.SetTexture(0, m_pInscatteringVolume);
			if (m_seperateDensity)
				pass.SetTexture(1, m_pVolFogBufDensity);

			pass.SetTexture(2, m_pMaxDepth);

			pass.SetSampler(0, EDefaultSamplerStates::TrilinearClamp);
		}

		pass.BeginConstantUpdate();

		const float fScreenWidth = static_cast<float>(nScreenWidth);
		const float fScreenHeight = static_cast<float>(nScreenHeight);
		static CCryNameR paramScreenSize("ScreenSize");
		const Vec4 vParamScreenSize(fScreenWidth, fScreenHeight, 1.0f / fScreenWidth, 1.0f / fScreenHeight);
		pass.SetConstant(paramScreenSize, vParamScreenSize);

		static CCryNameR paramNameScreenInfo("ScreenInfo");
		const Vec4 screenInfo(ndist, fdist, 0.0f, static_cast<float>(volumeDepth));
		pass.SetConstant(paramNameScreenInfo, screenInfo);

		const uint32 tileSizeX = 8;
		const uint32 tileSizeY = 8;
		const uint32 tileSizeZ = 1;
		const uint32 dispatchSizeX = (nScreenWidth / tileSizeX) + (nScreenWidth % tileSizeX > 0 ? 1 : 0);
		const uint32 dispatchSizeY = (nScreenHeight / tileSizeY) + (nScreenHeight % tileSizeY > 0 ? 1 : 0);
		const uint32 dispatchSizeZ = (volumeDepth / tileSizeZ) + (volumeDepth % tileSizeZ > 0 ? 1 : 0);
		pass.SetDispatchSize(dispatchSizeX, dispatchSizeY, dispatchSizeZ);
		pass.PrepareResourcesForUse(commandList);
		pass.Execute(commandList);
	}

	{
		auto& pass = m_passBlurInscatteringVertical[bufferId];

		if (pass.IsDirty(rtMask))
		{
			static CCryNameTSCRC techName("BlurVerticalInscatteringVolume");
			pass.SetTechnique(pShader, techName, rtMask);

			pass.SetOutputUAV(0, m_pInscatteringVolume);
			if (m_seperateDensity)
				pass.SetOutputUAV(1, m_pVolFogBufDensity);

			pass.SetTexture(0, GetInscatterTex());
			if (m_seperateDensity)
				pass.SetTexture(1, GetDensityTex());

			pass.SetTexture(2, m_pMaxDepth);

			pass.SetSampler(0, EDefaultSamplerStates::TrilinearClamp);
		}

		pass.BeginConstantUpdate();

		const float fScreenWidth = static_cast<float>(nScreenWidth);
		const float fScreenHeight = static_cast<float>(nScreenHeight);
		static CCryNameR paramScreenSize("ScreenSize");
		const Vec4 vParamScreenSize(fScreenWidth, fScreenHeight, 1.0f / fScreenWidth, 1.0f / fScreenHeight);
		pass.SetConstant(paramScreenSize, vParamScreenSize);

		static CCryNameR paramNameScreenInfo("ScreenInfo");
		const Vec4 screenInfo(ndist, fdist, 0.0f, static_cast<float>(volumeDepth));
		pass.SetConstant(paramNameScreenInfo, screenInfo);

		const uint32 tileSizeX = 8;
		const uint32 tileSizeY = 8;
		const uint32 tileSizeZ = 1;
		const uint32 dispatchSizeX = (nScreenWidth / tileSizeX) + (nScreenWidth % tileSizeX > 0 ? 1 : 0);
		const uint32 dispatchSizeY = (nScreenHeight / tileSizeY) + (nScreenHeight % tileSizeY > 0 ? 1 : 0);
		const uint32 dispatchSizeZ = (volumeDepth / tileSizeZ) + (volumeDepth % tileSizeZ > 0 ? 1 : 0);
		pass.SetDispatchSize(dispatchSizeX, dispatchSizeY, dispatchSizeZ);
		pass.PrepareResourcesForUse(commandList);
		pass.Execute(commandList);
	}
}

void CVolumetricFogStage::ExecuteTemporalReprojection(const SScopedComputeCommandList& commandList)
{
	//PROFILE_LABEL_SCOPE("TEMPORAL_REPROJECTION");

	const float ndist = GetCurrentViewInfo().nearClipPlane;
	const float fdist = GetCurrentViewInfo().farClipPlane;

	const int32 activeGpuCount = gRenDev->GetActiveGPUCount();

	// no temporal reprojection in several frames when resolution changes.
	if (m_cleared > 0)
	{
		m_cleared -= 1;

		GetDeviceObjectFactory().GetCoreCommandList().GetCopyInterface()->Copy(m_pInscatteringVolume->GetDevTexture(), GetPrevInscatterTex()->GetDevTexture());
		if (m_seperateDensity)
			GetDeviceObjectFactory().GetCoreCommandList().GetCopyInterface()->Copy(m_pVolFogBufDensity->GetDevTexture(), GetPrevDensityTex()->GetDevTexture());
	}

	// temporal reprojection
	{
		const uint32 nScreenWidth = m_pInscatteringVolume->GetWidth();
		const uint32 nScreenHeight = m_pInscatteringVolume->GetHeight();
		const uint32 volumeDepth = m_pInscatteringVolume->GetDepth();

		CShader* pShader = CShaderMan::s_shDeferredShading;
		const uint32 bufferId = GetTemporalBufferId();
		auto& pass = m_passTemporalReprojection[bufferId];

		if (pass.IsDirty(CRendererCVars::CV_r_VolumetricFogReprojectionMode))
		{
			uint64 rtMask = 0;
			rtMask |= (CRendererCVars::CV_r_VolumetricFogReprojectionMode != 0) ? g_HWSR_MaskBit[HWSR_SAMPLE5] : 0;
			// set texture format toggle
			rtMask |= (m_seperateDensity != false) ? g_HWSR_MaskBit[HWSR_SAMPLE3] : 0;

			static CCryNameTSCRC techName("ReprojectVolumetricFog");
			pass.SetTechnique(pShader, techName, rtMask);

			pass.SetOutputUAV(0, GetInscatterTex());
			pass.SetTexture(0, m_pInscatteringVolume);
			pass.SetTexture(1, GetPrevInscatterTex());
			pass.SetTexture(4, m_pMaxDepth);

			if (m_seperateDensity)
			{
				pass.SetOutputUAV(1, GetDensityTex());
				pass.SetTexture(2, m_pVolFogBufDensity);
				pass.SetTexture(3, GetPrevDensityTex());
			}

			pass.SetSampler(0, EDefaultSamplerStates::TrilinearClamp);

			pass.SetInlineConstantBuffer(eConstantBufferShaderSlot_PerView, m_graphicsPipeline.GetMainViewConstantBuffer());
		}

		pass.BeginConstantUpdate();

		float fScreenWidth = static_cast<float>(nScreenWidth);
		float fScreenHeight = static_cast<float>(nScreenHeight);
		static CCryNameR paramScreenSize("ScreenSize");
		const Vec4 vParamScreenSize(fScreenWidth, fScreenHeight, 1.0f / fScreenWidth, 1.0f / fScreenHeight);
		pass.SetConstant(paramScreenSize, vParamScreenSize);

		SD3DPostEffectsUtils::UpdateFrustumCorners(m_graphicsPipeline);
		static CCryNameR paramFrustumTL("FrustumTL");
		const Vec4 vParamFrustumTL(SD3DPostEffectsUtils::m_vLT.x, SD3DPostEffectsUtils::m_vLT.y, SD3DPostEffectsUtils::m_vLT.z, 0);
		pass.SetConstant(paramFrustumTL, vParamFrustumTL);
		static CCryNameR paramFrustumTR("FrustumTR");
		const Vec4 vParamFrustumTR(SD3DPostEffectsUtils::m_vRT.x, SD3DPostEffectsUtils::m_vRT.y, SD3DPostEffectsUtils::m_vRT.z, 0);
		pass.SetConstant(paramFrustumTR, vParamFrustumTR);
		static CCryNameR paramFrustumBL("FrustumBL");
		const Vec4 vParamFrustumBL(SD3DPostEffectsUtils::m_vLB.x, SD3DPostEffectsUtils::m_vLB.y, SD3DPostEffectsUtils::m_vLB.z, 0);
		pass.SetConstant(paramFrustumBL, vParamFrustumBL);

		const float reprojectionFactor = max(0.0f, min(1.0f, CRendererCVars::CV_r_VolumetricFogReprojectionBlendFactor));
		static CCryNameR paramNameScreenInfo("ScreenInfo");
		const Vec4 screenInfo(ndist, fdist, reprojectionFactor, static_cast<float>(volumeDepth));
		pass.SetConstant(paramNameScreenInfo, screenInfo);

		static CCryNameR param1("PrevViewProjMatrix");
		const Vec4* temp = reinterpret_cast<Vec4*>(m_viewProj[max((m_tick - activeGpuCount) % MaxFrameNum, 0)].GetData());
		pass.SetConstantArray(param1, temp, 4);

		const uint32 tileSizeX = 4;
		const uint32 tileSizeY = 4;
		const uint32 tileSizeZ = 4;
		const uint32 dispatchSizeX = (nScreenWidth / tileSizeX) + (nScreenWidth % tileSizeX > 0 ? 1 : 0);
		const uint32 dispatchSizeY = (nScreenHeight / tileSizeY) + (nScreenHeight % tileSizeY > 0 ? 1 : 0);
		const uint32 dispatchSizeZ = (volumeDepth / tileSizeZ) + (volumeDepth % tileSizeZ > 0 ? 1 : 0);
		pass.SetDispatchSize(dispatchSizeX, dispatchSizeY, dispatchSizeZ);
		pass.PrepareResourcesForUse(commandList);
		pass.Execute(commandList);
	}
}

void CVolumetricFogStage::ExecuteRaymarchVolumetricFog(const SScopedComputeCommandList& commandList)
{
	// accumulate in-scattering lighting and transmittance along view ray.
	//PROFILE_LABEL_SCOPE( "RAYMARCH_VOLUMETRIC_FOG" );

	const float ndist = GetCurrentViewInfo().nearClipPlane;
	const float fdist = GetCurrentViewInfo().farClipPlane;

	const uint32 nScreenWidth = m_pVolFogBufDensity->GetWidth();
	const uint32 nScreenHeight = m_pVolFogBufDensity->GetHeight();
	const uint32 volumeDepth = m_pVolFogBufDensity->GetDepth();

	CShader* pShader = CShaderMan::s_shDeferredShading;
	const uint32 bufferId = GetTemporalBufferId();
	auto& pass = m_passRaymarch[bufferId];

	// set texture format toggle
	uint64 rtMask = 0;
	if (m_seperateDensity)
		rtMask |= g_HWSR_MaskBit[HWSR_SAMPLE3];

	if (pass.IsDirty(rtMask))
	{
		static CCryNameTSCRC techName("RaymarchVolumetricFog");
		pass.SetTechnique(pShader, techName, rtMask);

		pass.SetOutputUAV(0, CRendererResources::s_ptexVolumetricFog);

		pass.SetInlineConstantBuffer(eConstantBufferShaderSlot_PerView, m_graphicsPipeline.GetMainViewConstantBuffer());

		pass.SetTexture(0, GetInscatterTex());
		if (m_seperateDensity)
			pass.SetTexture(1, GetDensityTex());
	}

	pass.BeginConstantUpdate();

	float fScreenWidth = static_cast<float>(nScreenWidth);
	float fScreenHeight = static_cast<float>(nScreenHeight);
	static CCryNameR paramScreenSize("ScreenSize");
	const Vec4 vParamScreenSize(fScreenWidth, fScreenHeight, 1.0f / fScreenWidth, 1.0f / fScreenHeight);
	pass.SetConstant(paramScreenSize, vParamScreenSize);

	SD3DPostEffectsUtils::UpdateFrustumCorners(m_graphicsPipeline);
	static CCryNameR paramFrustumTL("FrustumTL");
	const Vec4 vParamFrustumTL(SD3DPostEffectsUtils::m_vLT.x, SD3DPostEffectsUtils::m_vLT.y, SD3DPostEffectsUtils::m_vLT.z, 0);
	pass.SetConstant(paramFrustumTL, vParamFrustumTL);
	static CCryNameR paramFrustumTR("FrustumTR");
	const Vec4 vParamFrustumTR(SD3DPostEffectsUtils::m_vRT.x, SD3DPostEffectsUtils::m_vRT.y, SD3DPostEffectsUtils::m_vRT.z, 0);
	pass.SetConstant(paramFrustumTR, vParamFrustumTR);
	static CCryNameR paramFrustumBL("FrustumBL");
	const Vec4 vParamFrustumBL(SD3DPostEffectsUtils::m_vLB.x, SD3DPostEffectsUtils::m_vLB.y, SD3DPostEffectsUtils::m_vLB.z, 0);
	pass.SetConstant(paramFrustumBL, vParamFrustumBL);

	static CCryNameR paramNameScreenInfo("ScreenInfo");
	const Vec4 screenInfo(ndist, fdist, 0.0f, static_cast<float>(volumeDepth));
	pass.SetConstant(paramNameScreenInfo, screenInfo);

	const uint32 tileSizeX = 8;
	const uint32 tileSizeY = 8;
	const uint32 dispatchSizeX = (nScreenWidth / tileSizeX) + (nScreenWidth % tileSizeX > 0 ? 1 : 0);
	const uint32 dispatchSizeY = (nScreenHeight / tileSizeY) + (nScreenHeight % tileSizeY > 0 ? 1 : 0);
	pass.SetDispatchSize(dispatchSizeX, dispatchSizeY, 1);
	pass.PrepareResourcesForUse(commandList);
	pass.Execute(commandList);
}

void CVolumetricFogStage::FillForwardParams(SForwardParams& forwardParams, bool enable) const
{
	SRenderViewShaderConstants& paramsPF = RenderView()->GetShaderConstants();

	if (enable)
	{
		forwardParams.vfSamplingParams = paramsPF.pVolumetricFogSamplingParams;
		forwardParams.vfDistributionParams = paramsPF.pVolumetricFogDistributionParams;
		forwardParams.vfScatteringParams = paramsPF.pVolumetricFogScatteringParams;
		forwardParams.vfScatteringBlendParams = paramsPF.pVolumetricFogScatteringBlendParams;
		forwardParams.vfScatteringColor = paramsPF.pVolumetricFogScatteringColor;
		forwardParams.vfScatteringSecondaryColor = paramsPF.pVolumetricFogScatteringSecondaryColor;
		forwardParams.vfHeightDensityParams = paramsPF.pVolumetricFogHeightDensityParams;
		forwardParams.vfHeightDensityRampParams = paramsPF.pVolumetricFogHeightDensityRampParams;
		forwardParams.vfDistanceParams = paramsPF.pVolumetricFogDistanceParams;
		forwardParams.vfGlobalEnvProbeParams0 = GetGlobalEnvProbeShaderParam0();
		forwardParams.vfGlobalEnvProbeParams1 = GetGlobalEnvProbeShaderParam1();
	}
	else
	{
		// turning off by parameters.
		forwardParams.vfSamplingParams = paramsPF.pVolumetricFogSamplingParams;
		forwardParams.vfDistributionParams = paramsPF.pVolumetricFogDistributionParams;
		forwardParams.vfScatteringParams = paramsPF.pVolumetricFogScatteringParams;
		forwardParams.vfScatteringBlendParams = paramsPF.pVolumetricFogScatteringBlendParams;
		forwardParams.vfScatteringColor = Vec4(0.0f);
		forwardParams.vfScatteringSecondaryColor = Vec4(0.0f);
		const Vec4& heightDensityParams = paramsPF.pVolumetricFogHeightDensityParams;
		forwardParams.vfHeightDensityParams = Vec4(heightDensityParams.x, heightDensityParams.y, 0.0f, 1.0f);
		forwardParams.vfHeightDensityRampParams = paramsPF.pVolumetricFogHeightDensityRampParams;
		forwardParams.vfDistanceParams = paramsPF.pVolumetricFogDistanceParams;
		forwardParams.vfGlobalEnvProbeParams0 = Vec4(0.0f);
		forwardParams.vfGlobalEnvProbeParams1 = Vec4(0.0f);
	}
}
