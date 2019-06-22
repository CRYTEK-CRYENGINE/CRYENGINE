// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "GraphicsPipeline.h"
#include "Common/RendererResources.h"
#include "Common/Textures/TextureHelpers.h"
#include "XRenderD3D9/D3DPostProcess.h"
#include "../PostAA.h"
#include "../ShadowMap.h"
#include "../SceneGBuffer.h"
#include "../SceneForward.h"
#include "../SceneCustom.h"
#include "../TiledLightVolumes.h"
#include "../ClipVolumes.h"
#include "../Fog.h"
#include "../Sky.h"
#include "../VolumetricFog.h"
#include "../DebugRenderTargets.h"

void CGraphicsPipelineResources::Init()
{
	// Default Template textures
	int nRTFlags = FT_DONT_RELEASE | FT_DONT_STREAM | FT_STATE_CLAMP | FT_USAGE_RENDERTARGET;

	m_pTexVelocityObjects[0] = CTexture::GetOrCreateTextureObject(m_graphicsPipeline.MakeUniqueTexIdentifierName("$VelocityObjects").c_str(), 0, 0, 1, eTT_2D, FT_DONT_RELEASE | FT_DONT_STREAM | FT_USAGE_RENDERTARGET | FT_USAGE_UNORDERED_ACCESS, eTF_Unknown, -1);
	// Only used for VR, but we need to support runtime switching
	m_pTexVelocityObjects[1] = CTexture::GetOrCreateTextureObject(m_graphicsPipeline.MakeUniqueTexIdentifierName("$VelocityObjects_R").c_str(), 0, 0, 1, eTT_2D, FT_DONT_RELEASE | FT_DONT_STREAM | FT_USAGE_RENDERTARGET | FT_USAGE_UNORDERED_ACCESS, eTF_Unknown, -1);

	m_pTexSceneNormalsMap = CTexture::GetOrCreateTextureObject(m_graphicsPipeline.MakeUniqueTexIdentifierName("$SceneNormalsMap").c_str(), 0, 0, 1, eTT_2D, nRTFlags, eTF_R8G8B8A8, TO_SCENE_NORMALMAP);
	m_pTexSceneDiffuse = CTexture::GetOrCreateTextureObject(m_graphicsPipeline.MakeUniqueTexIdentifierName("$SceneDiffuse").c_str(), 0, 0, 1, eTT_2D, nRTFlags, eTF_R8G8B8A8);
	m_pTexSceneSpecular = CTexture::GetOrCreateTextureObject(m_graphicsPipeline.MakeUniqueTexIdentifierName("$SceneSpecular").c_str(), 0, 0, 1, eTT_2D, nRTFlags, eTF_R8G8B8A8);
#if defined(DURANGO_USE_ESRAM)
	m_pTexSceneSpecularESRAM = CTexture::GetOrCreateTextureObject(m_graphicsPipeline.MakeUniqueTexIdentifierName("$SceneSpecularESRAM").c_str(), 0, 0, 1, eTT_2D, nRTFlags, eTF_R8G8B8A8);
#endif

	m_pTexLinearDepth = CTexture::GetOrCreateTextureObject(m_graphicsPipeline.MakeUniqueTexIdentifierName("$ZTarget").c_str(), 0, 0, 1, eTT_2D, FT_DONT_RELEASE | FT_DONT_STREAM | FT_USAGE_RENDERTARGET, eTF_Unknown);
	m_pTexHDRTarget = CTexture::GetOrCreateTextureObject(m_graphicsPipeline.MakeUniqueTexIdentifierName("$HDRTarget").c_str(), 0, 0, 1, eTT_2D, nRTFlags, eTF_Unknown);
	m_pTexShadowMask = CTexture::GetOrCreateTextureObject(m_graphicsPipeline.MakeUniqueTexIdentifierName("$ShadowMask").c_str(), 0, 0, 1, eTT_2DArray, nRTFlags, eTF_R8, TO_SHADOWMASK);
	m_pTexSceneNormalsBent = CTexture::GetOrCreateTextureObject(m_graphicsPipeline.MakeUniqueTexIdentifierName("$SceneNormalsBent").c_str(), 0, 0, 1, eTT_2D, nRTFlags, eTF_R8G8B8A8);

	m_pTexSceneDepthScaled[0] = CTexture::GetOrCreateTextureObject(m_graphicsPipeline.MakeUniqueTexIdentifierName("$SceneDepthScaled").c_str(), 0, 0, 1, eTT_2D, FT_DONT_RELEASE | FT_DONT_STREAM | FT_USAGE_DEPTHSTENCIL, eTF_Unknown);
	m_pTexSceneDepthScaled[1] = CTexture::GetOrCreateTextureObject(m_graphicsPipeline.MakeUniqueTexIdentifierName("$SceneDepthScaled2").c_str(), 0, 0, 1, eTT_2D, FT_DONT_RELEASE | FT_DONT_STREAM | FT_USAGE_DEPTHSTENCIL, eTF_Unknown);
	m_pTexSceneDepthScaled[2] = CTexture::GetOrCreateTextureObject(m_graphicsPipeline.MakeUniqueTexIdentifierName("$SceneDepthScaled3").c_str(), 0, 0, 1, eTT_2D, FT_DONT_RELEASE | FT_DONT_STREAM | FT_USAGE_DEPTHSTENCIL, eTF_Unknown);

	m_pTexLinearDepthScaled[0] = CTexture::GetOrCreateTextureObject(m_graphicsPipeline.MakeUniqueTexIdentifierName("$ZTargetScaled").c_str(), 0, 0, 1, eTT_2D, FT_DONT_RELEASE | FT_DONT_STREAM | FT_USAGE_RENDERTARGET, eTF_Unknown, TO_DOWNSCALED_ZTARGET_FOR_AO);
	m_pTexLinearDepthScaled[1] = CTexture::GetOrCreateTextureObject(m_graphicsPipeline.MakeUniqueTexIdentifierName("$ZTargetScaled2").c_str(), 0, 0, 1, eTT_2D, FT_DONT_RELEASE | FT_DONT_STREAM | FT_USAGE_RENDERTARGET, eTF_Unknown, TO_QUARTER_ZTARGET_FOR_AO);
	m_pTexLinearDepthScaled[2] = CTexture::GetOrCreateTextureObject(m_graphicsPipeline.MakeUniqueTexIdentifierName("$ZTargetScaled3").c_str(), 0, 0, 1, eTT_2D, FT_DONT_RELEASE | FT_DONT_STREAM | FT_USAGE_RENDERTARGET, eTF_Unknown);

	m_pTexClipVolumes = CTexture::GetOrCreateTextureObject(m_graphicsPipeline.MakeUniqueTexIdentifierName("$ClipVolumes").c_str(), 0, 0, 1, eTT_2D, nRTFlags, eTF_R8G8);

	m_pTexSceneDiffuseTmp = CTexture::GetOrCreateTextureObject(m_graphicsPipeline.MakeUniqueTexIdentifierName("$SceneDiffuseTmp").c_str(), 0, 0, 1, eTT_2D, nRTFlags, eTF_R8G8B8A8, TO_SCENE_DIFFUSE_ACC);
	m_pTexSceneSpecularTmp = CTexture::GetOrCreateTextureObject(m_graphicsPipeline.MakeUniqueTexIdentifierName("$SceneSpecularTmp").c_str(), 0, 0, 1, eTT_2D, nRTFlags, eTF_R8G8B8A8, TO_SCENE_SPECULAR_ACC);

	m_pTexDisplayTargetScaled[0] = CTexture::GetOrCreateTextureObject(m_graphicsPipeline.MakeUniqueTexIdentifierName("$DisplayTarget 1/2a").c_str(), 0, 0, 1, eTT_2D, FT_DONT_RELEASE | FT_DONT_STREAM | FT_USAGE_RENDERTARGET, eTF_Unknown, TO_BACKBUFFERSCALED_D2);
	m_pTexDisplayTargetScaled[1] = CTexture::GetOrCreateTextureObject(m_graphicsPipeline.MakeUniqueTexIdentifierName("$DisplayTarget 1/4a").c_str(), 0, 0, 1, eTT_2D, FT_DONT_RELEASE | FT_DONT_STREAM | FT_USAGE_RENDERTARGET, eTF_Unknown, TO_BACKBUFFERSCALED_D4);
	m_pTexDisplayTargetScaled[2] = CTexture::GetOrCreateTextureObject(m_graphicsPipeline.MakeUniqueTexIdentifierName("$DisplayTarget 1/8").c_str(), 0, 0, 1, eTT_2D, FT_DONT_RELEASE | FT_DONT_STREAM | FT_USAGE_RENDERTARGET, eTF_Unknown, TO_BACKBUFFERSCALED_D8);

	m_pTexDisplayTargetScaledTemp[0] = CTexture::GetOrCreateTextureObject(m_graphicsPipeline.MakeUniqueTexIdentifierName("$DisplayTarget 1/2b").c_str(), 0, 0, 1, eTT_2D, FT_DONT_RELEASE | FT_DONT_STREAM | FT_USAGE_RENDERTARGET, eTF_Unknown, -1);
	m_pTexDisplayTargetScaledTemp[1] = CTexture::GetOrCreateTextureObject(m_graphicsPipeline.MakeUniqueTexIdentifierName("$DisplayTarget 1/4b").c_str(), 0, 0, 1, eTT_2D, FT_DONT_RELEASE | FT_DONT_STREAM | FT_USAGE_RENDERTARGET, eTF_Unknown, -1);

	m_pTexDisplayTargetSrc = CTexture::GetOrCreateTextureObject(m_graphicsPipeline.MakeUniqueTexIdentifierName("$DisplayTarget").c_str(), 0, 0, 1, eTT_2D, FT_DONT_RELEASE | FT_DONT_STREAM | FT_USAGE_RENDERTARGET, eTF_Unknown, TO_BACKBUFFERMAP);
	m_pTexDisplayTargetDst = CTexture::GetOrCreateTextureObject(m_graphicsPipeline.MakeUniqueTexIdentifierName("$DisplayTargetDst").c_str(), 0, 0, 1, eTT_2D, FT_DONT_RELEASE | FT_DONT_STREAM | FT_USAGE_RENDERTARGET, eTF_Unknown);

	m_pTexSceneSelectionIDs = CTexture::GetOrCreateTextureObject(m_graphicsPipeline.MakeUniqueTexIdentifierName("$SceneSelectionIDs").c_str(), 0, 0, 1, eTT_2D, FT_DONT_RELEASE | FT_DONT_STREAM | FT_USAGE_RENDERTARGET, eTF_R32F);
	
	m_pTexSceneTargetR11G11B10F[0] = CTexture::GetOrCreateTextureObject(m_graphicsPipeline.MakeUniqueTexIdentifierName("$SceneTargetR11G11B10F_0").c_str(), 0, 0, 1, eTT_2D, FT_DONT_RELEASE | FT_DONT_STREAM | FT_USAGE_RENDERTARGET, eTF_Unknown, -1);
	m_pTexSceneTargetR11G11B10F[1] = CTexture::GetOrCreateTextureObject(m_graphicsPipeline.MakeUniqueTexIdentifierName("$SceneTargetR11G11B10F_1").c_str(), 0, 0, 1, eTT_2D, FT_DONT_RELEASE | FT_DONT_STREAM | FT_USAGE_RENDERTARGET, eTF_Unknown, -1);

	m_pTexLinearDepthFixup = CTexture::GetOrCreateTextureObject(m_graphicsPipeline.MakeUniqueTexIdentifierName("$ZTargetFixup").c_str(), 0, 0, 1, eTT_2D, FT_DONT_RELEASE | FT_DONT_STREAM | FT_USAGE_RENDERTARGET, eTF_Unknown);
	m_pTexSceneTarget = CTexture::GetOrCreateTextureObject(m_graphicsPipeline.MakeUniqueTexIdentifierName("$SceneTarget").c_str(), 0, 0, 1, eTT_2D, FT_DONT_RELEASE | FT_DONT_STREAM | FT_USAGE_RENDERTARGET, eTF_Unknown, TO_SCENE_TARGET);

	if (!m_pTexModelHudBuffer)
		m_pTexModelHudBuffer = CTexture::GetOrCreateTextureObject(m_graphicsPipeline.MakeUniqueTexIdentifierName("$ModelHud").c_str(), 0, 0, 1, eTT_2D, FT_DONT_RELEASE | FT_DONT_STREAM | FT_USAGE_RENDERTARGET, eTF_Unknown, TO_MODELHUD);

	if (RainOcclusionMapsEnabled())
		PrepareRainOcclusionMaps();

	m_pTexVelocity = CTexture::GetOrCreateTextureObject(m_graphicsPipeline.MakeUniqueTexIdentifierName("$Velocity").c_str(), 0, 0, 1, eTT_2D, FT_DONT_RELEASE | FT_DONT_STREAM | FT_USAGE_RENDERTARGET, eTF_Unknown, -1);
	m_pTexVelocityTiles[0] = CTexture::GetOrCreateTextureObject(m_graphicsPipeline.MakeUniqueTexIdentifierName("$VelocityTilesTmp0").c_str(), 0, 0, 1, eTT_2D, FT_DONT_RELEASE | FT_DONT_STREAM | FT_USAGE_RENDERTARGET, eTF_Unknown, -1);
	m_pTexVelocityTiles[1] = CTexture::GetOrCreateTextureObject(m_graphicsPipeline.MakeUniqueTexIdentifierName("$VelocityTilesTmp1").c_str(), 0, 0, 1, eTT_2D, FT_DONT_RELEASE | FT_DONT_STREAM | FT_USAGE_RENDERTARGET, eTF_Unknown, -1);
	m_pTexVelocityTiles[2] = CTexture::GetOrCreateTextureObject(m_graphicsPipeline.MakeUniqueTexIdentifierName("$VelocityTiles").c_str(), 0, 0, 1, eTT_2D, FT_DONT_RELEASE | FT_DONT_STREAM | FT_USAGE_RENDERTARGET, eTF_Unknown, -1);

	m_pTexWaterVolumeRefl[0] = CTexture::GetOrCreateTextureObject(m_graphicsPipeline.MakeUniqueTexIdentifierName("$WaterVolumeRefl").c_str(), 64, 64, 1, eTT_2D, FT_DONT_RELEASE | FT_DONT_STREAM | FT_USAGE_RENDERTARGET | FT_FORCE_MIPS, eTF_Unknown, TO_WATERVOLUMEREFLMAP);
	m_pTexWaterVolumeRefl[1] = CTexture::GetOrCreateTextureObject(m_graphicsPipeline.MakeUniqueTexIdentifierName("$WaterVolumeReflPrev").c_str(), 64, 64, 1, eTT_2D, FT_DONT_RELEASE | FT_DONT_STREAM | FT_USAGE_RENDERTARGET | FT_FORCE_MIPS, eTF_Unknown, TO_WATERVOLUMEREFLMAPPREV);

#if CRY_PLATFORM_DURANGO && DURANGO_USE_ESRAM
	// Assign ESRAM offsets
	if (CRenderer::CV_r_useESRAM)
	{
		// Precomputed offsets, using xg library and aligned to 4k
		//     1600x900 RGBA16F:  11894784
		//     1600x900 RGBA8:     5955584

		if (gRenDev->GetWidth() <= 1600 && gRenDev->GetHeight() <= 900)
		{
			m_pTexHDRTarget->SetESRAMOffset(0);
			m_pTexSceneSpecularESRAM->SetESRAMOffset(0);
			m_pTexSceneNormalsMap->SetESRAMOffset(11894784 + 5955584 * 0);
			m_pTexSceneDiffuse->SetESRAMOffset(11894784 + 5955584 * 1);
			// Depth target uses: 11894784 + 5955584 * 2
		}
		else
		{
			iLog->LogError("Disabling ESRAM since resolution is larger than 1600x900");
			assert(0);
		}
	}
#endif


	CreateResources(0, 0);
}

void CGraphicsPipelineResources::CreateResources(int resourceWidth, int resourceHeight)
{
	if (!resourceWidth) resourceWidth = m_resourceWidth;
	if (!resourceHeight) resourceHeight = m_resourceHeight;

	if (!resourceWidth) resourceWidth = m_graphicsPipeline.GetRenderResolution().x;
	if (!resourceHeight) resourceHeight = m_graphicsPipeline.GetRenderResolution().y;

	CRY_ASSERT(resourceWidth);
	CRY_ASSERT(resourceHeight);

	CreateDepthMaps(resourceWidth, resourceHeight);
	CreateDeferredMaps(resourceWidth, resourceHeight);
	CreateHDRMaps(resourceWidth, resourceHeight);
	CreatePostFXMaps(resourceWidth, resourceHeight);
	CreateSceneMaps(resourceWidth, resourceHeight);

	m_resourceWidth = resourceWidth;
	m_resourceHeight = resourceHeight;
}

void CGraphicsPipelineResources::Resize(int renderWidth, int renderHeight)
{
	int resourceWidth = m_resourceWidth;
	int resourceHeight = m_resourceHeight;

	if (resourceWidth != renderWidth ||
		resourceHeight != renderHeight)
	{
		resourceWidth = renderWidth;
		resourceHeight = renderHeight;

		CreateDepthMaps(resourceWidth, resourceHeight);
		CreateDeferredMaps(resourceWidth, resourceHeight);
		CreateHDRMaps(resourceWidth, resourceHeight);
		CreatePostFXMaps(resourceWidth, resourceHeight);
		CreateSceneMaps(resourceWidth, resourceHeight);
	}

	m_resourceWidth = resourceWidth;
	m_resourceHeight = resourceHeight;
}

void CGraphicsPipelineResources::CreateDeferredMaps(int resourceWidth, int resourceHeight)
{
	Vec2i resolution = Vec2i(resourceWidth, resourceHeight);
	const int width = resolution.x, width_r2 = (width + 1) / 2, width_r4 = (width_r2 + 1) / 2, width_r8 = (width_r4 + 1) / 2;
	const int height = resolution.y, height_r2 = (height + 1) / 2, height_r4 = (height_r2 + 1) / 2, height_r8 = (height_r4 + 1) / 2;

	ETEX_Format preferredDepthFormat = CRendererResources::GetDepthFormat();
	ETEX_Format fmtZScaled = eTF_R16G16B16A16F;

	SD3DPostEffectsUtils::GetOrCreateRenderTarget(m_graphicsPipeline.MakeUniqueTexIdentifierName("$SceneNormalsMap").c_str(), m_pTexSceneNormalsMap, width, height, Clr_Unknown, true, false, eTF_R8G8B8A8, TO_SCENE_NORMALMAP, FT_USAGE_ALLOWREADSRGB);
	SD3DPostEffectsUtils::GetOrCreateRenderTarget(m_graphicsPipeline.MakeUniqueTexIdentifierName("$SceneDiffuse").c_str(), m_pTexSceneDiffuse, width, height, Clr_Empty, true, false, eTF_R8G8B8A8, -1, FT_USAGE_ALLOWREADSRGB);
	SD3DPostEffectsUtils::GetOrCreateRenderTarget(m_graphicsPipeline.MakeUniqueTexIdentifierName("$SceneSpecular").c_str(), m_pTexSceneSpecular, width, height, Clr_Empty, true, false, eTF_R8G8B8A8, -1, FT_USAGE_ALLOWREADSRGB);
#if defined(DURANGO_USE_ESRAM)
	SD3DPostEffectsUtils::GetOrCreateRenderTarget(m_graphicsPipeline.MakeUniqueTexIdentifierName("$SceneSpecularESRAM").c_str(), m_pTexSceneSpecularESRAM, width, height, Clr_Empty, true, false, eTF_R8G8B8A8, -1);
#endif
	SD3DPostEffectsUtils::GetOrCreateRenderTarget(m_graphicsPipeline.MakeUniqueTexIdentifierName("$VelocityObjects").c_str(), m_pTexVelocityObjects[0], width, height, Clr_Transparent, true, false, eTF_R16G16F, -1, FT_USAGE_UNORDERED_ACCESS);
	if (gRenDev->IsStereoEnabled())
	{
		SD3DPostEffectsUtils::GetOrCreateRenderTarget(m_graphicsPipeline.MakeUniqueTexIdentifierName("$VelocityObjects_R").c_str(), m_pTexVelocityObjects[1], width, height, Clr_Transparent, true, false, eTF_R16G16F, -1, FT_USAGE_UNORDERED_ACCESS);
	}

	SD3DPostEffectsUtils::GetOrCreateRenderTarget(m_graphicsPipeline.MakeUniqueTexIdentifierName("$SceneNormalsBent").c_str(), m_pTexSceneNormalsBent, width, height, Clr_Median, true, false, eTF_R8G8B8A8);

	SD3DPostEffectsUtils::GetOrCreateDepthStencil(m_graphicsPipeline.MakeUniqueTexIdentifierName("$SceneDepthScaled").c_str(), m_pTexSceneDepthScaled[0], width_r2, height_r2, Clr_FarPlane_Rev, false, false, preferredDepthFormat, -1, FT_USAGE_DEPTHSTENCIL);
	SD3DPostEffectsUtils::GetOrCreateDepthStencil(m_graphicsPipeline.MakeUniqueTexIdentifierName("$SceneDepthScaled2").c_str(), m_pTexSceneDepthScaled[1], width_r4, height_r4, Clr_FarPlane_Rev, false, false, preferredDepthFormat, -1, FT_USAGE_DEPTHSTENCIL);
	SD3DPostEffectsUtils::GetOrCreateDepthStencil(m_graphicsPipeline.MakeUniqueTexIdentifierName("$SceneDepthScaled3").c_str(), m_pTexSceneDepthScaled[2], width_r8, height_r8, Clr_FarPlane_Rev, false, false, preferredDepthFormat, -1, FT_USAGE_DEPTHSTENCIL);

	SD3DPostEffectsUtils::GetOrCreateRenderTarget(m_graphicsPipeline.MakeUniqueTexIdentifierName("$ZTargetScaled").c_str(), m_pTexLinearDepthScaled[0], width_r2, height_r2, ColorF(1.0f, 1.0f, 1.0f, 1.0f), 1, 0, fmtZScaled, TO_DOWNSCALED_ZTARGET_FOR_AO);
	SD3DPostEffectsUtils::GetOrCreateRenderTarget(m_graphicsPipeline.MakeUniqueTexIdentifierName("$ZTargetScaled2").c_str(), m_pTexLinearDepthScaled[1], width_r4, height_r4, ColorF(1.0f, 1.0f, 1.0f, 1.0f), 1, 0, fmtZScaled, TO_QUARTER_ZTARGET_FOR_AO);
	SD3DPostEffectsUtils::GetOrCreateRenderTarget(m_graphicsPipeline.MakeUniqueTexIdentifierName("$ZTargetScaled3").c_str(), m_pTexLinearDepthScaled[2], width_r8, height_r8, ColorF(1.0f, 1.0f, 1.0f, 1.0f), 1, 0, fmtZScaled);

	SD3DPostEffectsUtils::GetOrCreateRenderTarget(m_graphicsPipeline.MakeUniqueTexIdentifierName("$ClipVolumes").c_str(), m_pTexClipVolumes, width, height, Clr_Empty, false, false, eTF_R8G8);
	SD3DPostEffectsUtils::GetOrCreateRenderTarget(m_graphicsPipeline.MakeUniqueTexIdentifierName("$AOColorBleed").c_str(), m_pTexAOColorBleed, width_r8, height_r8, Clr_Unknown, true, false, eTF_R8G8B8A8);

	SD3DPostEffectsUtils::GetOrCreateRenderTarget(m_graphicsPipeline.MakeUniqueTexIdentifierName("$SceneDiffuseTmp").c_str(), m_pTexSceneDiffuseTmp, width, height, Clr_Empty, true, false, eTF_R8G8B8A8);
	SD3DPostEffectsUtils::GetOrCreateRenderTarget(m_graphicsPipeline.MakeUniqueTexIdentifierName("$SceneSpecularTmp").c_str(), m_pTexSceneSpecularTmp, width, height, Clr_Empty, true, false, eTF_R8G8B8A8);

	// shadow mask
	if (m_pTexShadowMask)
		m_pTexShadowMask->Invalidate(resolution.x, resolution.y, eTF_R8);

	if (!CTexture::IsTextureExist(m_pTexShadowMask))
	{
		const int nArraySize = gcpRendD3D->CV_r_ShadowCastingLightsMaxCount;
		m_pTexShadowMask = CTexture::GetOrCreateTextureArray(m_graphicsPipeline.MakeUniqueTexIdentifierName("$ShadowMask").c_str(), resolution.x, resolution.y, nArraySize, 1, eTT_2DArray, FT_DONT_STREAM | FT_USAGE_RENDERTARGET, eTF_R8, TO_SHADOWMASK);
	}

	// Pre-create shadow pool
	IF(gcpRendD3D->m_pRT->IsRenderThread() && gEnv->p3DEngine, 1)
	{
		//init shadow pool size
		static ICVar* p_e_ShadowsPoolSize = iConsole->GetCVar("e_ShadowsPoolSize");
		gcpRendD3D->m_nShadowPoolHeight = p_e_ShadowsPoolSize->GetIVal();
		gcpRendD3D->m_nShadowPoolWidth = gcpRendD3D->m_nShadowPoolHeight; //square atlas
	}
}

void CGraphicsPipelineResources::CreateDepthMaps(int resourceWidth, int resourceHeight)
{
	const uint32 nRTFlags = FT_DONT_STREAM | FT_DONT_RELEASE | FT_USAGE_RENDERTARGET;
	uint32 nUAFlags = FT_DONT_STREAM | FT_DONT_RELEASE | FT_USAGE_RENDERTARGET | FT_USAGE_UNORDERED_ACCESS | FT_USAGE_UAV_RWTEXTURE;

	m_pTexLinearDepth->SetFlags(nRTFlags);
	m_pTexLinearDepth->SetWidth(resourceWidth);
	m_pTexLinearDepth->SetHeight(resourceHeight);
	m_pTexLinearDepth->CreateRenderTarget(CRendererResources::s_eTFZ, ColorF(1.0f, 1.0f, 1.0f, 1.0f));

	if (!CRendererCVars::CV_r_HDRTexFormat && !CTexture::IsTextureExist(m_pTexLinearDepthFixup))
	{
		m_pTexLinearDepthFixup->SetFlags(nUAFlags);
		m_pTexLinearDepthFixup->SetWidth(resourceWidth);
		m_pTexLinearDepthFixup->SetHeight(resourceHeight);
		m_pTexLinearDepthFixup->CreateRenderTarget(eTF_R32F, ColorF(1.0f, 1.0f, 1.0f, 1.0f));

		SResourceView typedUAV = SResourceView::UnorderedAccessView(DXGI_FORMAT_R32_UINT, 0, -1, 0, SResourceView::eUAV_ReadWrite);
		m_pTexLinearDepthFixupUAV = m_pTexLinearDepthFixup->GetDevTexture()->GetOrCreateResourceViewHandle(typedUAV);
	}
}

void CGraphicsPipelineResources::CreateHDRMaps(int resourceWidth, int resourceHeight)
{
	m_renderTargetPool.ClearRenderTargetList();

	const int width = resourceWidth, width_r2 = (width + 1) / 2, width_r4 = (width_r2 + 1) / 2, width_r8 = (width_r4 + 1) / 2, width_r16 = (width_r8 + 1) / 2;
	const int height = resourceHeight, height_r2 = (height + 1) / 2, height_r4 = (height_r2 + 1) / 2, height_r8 = (height_r4 + 1) / 2, height_r16 = (height_r8 + 1) / 2;
	uint32 nHDRTargetFlags = FT_DONT_RELEASE;
	uint32 nHDRTargetFlagsUAV = nHDRTargetFlags | (FT_USAGE_UNORDERED_ACCESS);  // UAV required for tiled deferred shading

	const ETEX_Format nHDRFormat = CRendererResources::GetHDRFormat(false, false); // No alpha, default is HiQ, can be downgraded
	const ETEX_Format nHDRQFormat = CRendererResources::GetHDRFormat(false, true); // No alpha, default is LoQ, can be upgraded
	const ETEX_Format nHDRAFormat = CRendererResources::GetHDRFormat(true, false); // With alpha
	
	m_renderTargetPool.AddRenderTarget(width, height, Clr_Unknown, nHDRFormat, 1.0f, m_graphicsPipeline.MakeUniqueTexIdentifierName("$HDRTarget").c_str(), &m_pTexHDRTarget, nHDRTargetFlagsUAV);

	// Scaled versions of the HDR scene texture
	m_renderTargetPool.AddRenderTarget(width_r2, height_r2, Clr_Unknown, nHDRFormat, 0.9f, m_graphicsPipeline.MakeUniqueTexIdentifierName("$HDRTarget 1/2a").c_str(), &m_pTexHDRTargetScaled[0][0], FT_DONT_RELEASE);
	m_renderTargetPool.AddRenderTarget(width_r4, height_r4, Clr_Unknown, nHDRFormat, 0.9f, m_graphicsPipeline.MakeUniqueTexIdentifierName("$HDRTarget 1/4a").c_str(), &m_pTexHDRTargetScaled[1][0], FT_DONT_RELEASE);
	m_renderTargetPool.AddRenderTarget(width_r4, height_r4, Clr_Unknown, nHDRFormat, 0.9f, m_graphicsPipeline.MakeUniqueTexIdentifierName("$HDRTarget 1/4b").c_str(), &m_pTexHDRTargetScaled[1][1], FT_DONT_RELEASE);

	// Scaled versions of compositions of the HDR scene texture (with alpha)
	m_renderTargetPool.AddRenderTarget(width_r2, height_r2, Clr_Transparent, nHDRAFormat, 0.9f, m_graphicsPipeline.MakeUniqueTexIdentifierName("$HDRTargetMasked 1/2a").c_str(), &m_pTexHDRTargetMaskedScaled[0][0], FT_DONT_RELEASE);
	m_renderTargetPool.AddRenderTarget(width_r2, height_r2, Clr_Transparent, nHDRAFormat, 0.9f, m_graphicsPipeline.MakeUniqueTexIdentifierName("$HDRTargetMasked 1/2b").c_str(), &m_pTexHDRTargetMaskedScaled[0][1], FT_DONT_RELEASE);
	m_renderTargetPool.AddRenderTarget(width_r2, height_r2, Clr_Transparent, nHDRAFormat, 0.9f, m_graphicsPipeline.MakeUniqueTexIdentifierName("$HDRTargetMasked 1/2c").c_str(), &m_pTexHDRTargetMaskedScaled[0][2], FT_DONT_RELEASE);
	m_renderTargetPool.AddRenderTarget(width_r2, height_r2, Clr_Transparent, nHDRAFormat, 0.9f, m_graphicsPipeline.MakeUniqueTexIdentifierName("$HDRTargetMasked 1/2d").c_str(), &m_pTexHDRTargetMaskedScaled[0][3], FT_DONT_RELEASE);
	m_renderTargetPool.AddRenderTarget(width_r4, height_r4, Clr_Transparent, nHDRAFormat, 0.9f, m_graphicsPipeline.MakeUniqueTexIdentifierName("$HDRTargetMasked 1/4a").c_str(), &m_pTexHDRTargetMaskedScaled[1][0], FT_DONT_RELEASE);
	m_renderTargetPool.AddRenderTarget(width_r4, height_r4, Clr_Transparent, nHDRAFormat, 0.9f, m_graphicsPipeline.MakeUniqueTexIdentifierName("$HDRTargetMasked 1/4b").c_str(), &m_pTexHDRTargetMaskedScaled[1][1], FT_DONT_RELEASE);
	m_renderTargetPool.AddRenderTarget(width_r8, height_r8, Clr_Transparent, nHDRAFormat, 0.9f, m_graphicsPipeline.MakeUniqueTexIdentifierName("$HDRTargetMasked 1/8a").c_str(), &m_pTexHDRTargetMaskedScaled[2][0], FT_DONT_RELEASE);
	m_renderTargetPool.AddRenderTarget(width_r8, height_r8, Clr_Transparent, nHDRAFormat, 0.9f, m_graphicsPipeline.MakeUniqueTexIdentifierName("$HDRTargetMasked 1/8b").c_str(), &m_pTexHDRTargetMaskedScaled[2][1], FT_DONT_RELEASE);
	m_renderTargetPool.AddRenderTarget(width_r16, height_r16, Clr_Transparent, nHDRAFormat, 0.9f, m_graphicsPipeline.MakeUniqueTexIdentifierName("$HDRTargetMasked 1/16a").c_str(), &m_pTexHDRTargetMaskedScaled[3][0], FT_DONT_RELEASE);
	m_renderTargetPool.AddRenderTarget(width_r16, height_r16, Clr_Transparent, nHDRAFormat, 0.9f, m_graphicsPipeline.MakeUniqueTexIdentifierName("$HDRTargetMasked 1/16b").c_str(), &m_pTexHDRTargetMaskedScaled[3][1], FT_DONT_RELEASE);
	m_renderTargetPool.AddRenderTarget(width, height, Clr_Unknown, nHDRQFormat, 1.0f, m_graphicsPipeline.MakeUniqueTexIdentifierName("$HDRTargetPrev").c_str(), &m_pTexHDRTargetPrev);
	m_renderTargetPool.AddRenderTarget(width, height, Clr_Transparent, nHDRAFormat, 1.0f, m_graphicsPipeline.MakeUniqueTexIdentifierName("$HDRTargetMasked").c_str(), &m_pTexHDRTargetMasked, nHDRTargetFlags);
	
	m_renderTargetPool.AddRenderTarget(width, height, Clr_Unknown, nHDRQFormat, 1.0f, m_graphicsPipeline.MakeUniqueTexIdentifierName("$SceneTargetR11G11B10F_0").c_str(), &m_pTexSceneTargetR11G11B10F[0], nHDRTargetFlagsUAV);
	m_renderTargetPool.AddRenderTarget(width, height, Clr_Unknown, nHDRQFormat, 1.0f, m_graphicsPipeline.MakeUniqueTexIdentifierName("$SceneTargetR11G11B10F_1").c_str(), &m_pTexSceneTargetR11G11B10F[1], nHDRTargetFlags);

	m_renderTargetPool.AddRenderTarget(width_r4, height_r4, Clr_Unknown, nHDRQFormat, 0.9f, m_graphicsPipeline.MakeUniqueTexIdentifierName("$HDRFinalBloom").c_str(), &m_pTexHDRFinalBloom, FT_DONT_RELEASE);

	m_renderTargetPool.AddRenderTarget(width, height, Clr_Unknown, eTF_R8G8B8A8, 0.1f, m_graphicsPipeline.MakeUniqueTexIdentifierName("$Velocity").c_str(), &m_pTexVelocity, FT_DONT_RELEASE);
	m_renderTargetPool.AddRenderTarget(20, height, Clr_Unknown, eTF_R8G8B8A8, 0.1f, m_graphicsPipeline.MakeUniqueTexIdentifierName("$VelocityTilesTmp0").c_str(), &m_pTexVelocityTiles[0], FT_DONT_RELEASE);
	m_renderTargetPool.AddRenderTarget(20, 20, Clr_Unknown, eTF_R8G8B8A8, 0.1f, m_graphicsPipeline.MakeUniqueTexIdentifierName("$VelocityTilesTmp1").c_str(), &m_pTexVelocityTiles[1], FT_DONT_RELEASE);
	m_renderTargetPool.AddRenderTarget(20, 20, Clr_Unknown, eTF_R8G8B8A8, 0.1f, m_graphicsPipeline.MakeUniqueTexIdentifierName("$VelocityTiles").c_str(), &m_pTexVelocityTiles[2], FT_DONT_RELEASE);

#if RENDERER_ENABLE_FULL_PIPELINE
	m_renderTargetPool.AddRenderTarget(width_r2, height_r2, Clr_Unknown, eTF_R16G16F, 1.0f, m_graphicsPipeline.MakeUniqueTexIdentifierName("$MinCoC_0_Temp").c_str(), &m_pTexSceneCoCTemp, FT_DONT_RELEASE);
	for (int i = 0; i < MIN_DOF_COC_K; i++)
	{
		char szName[256];
		cry_sprintf(szName, m_graphicsPipeline.MakeUniqueTexIdentifierName("$MinCoC_%d").c_str(), i);
		m_renderTargetPool.AddRenderTarget(width_r2 / (i + 1), height_r2 / (i + 1), Clr_Unknown, eTF_R16G16F, 0.1f, szName, &m_pTexSceneCoC[i], FT_DONT_RELEASE, -1, true);
	}

	for (int i = 0; i < MAX_GPU_NUM; ++i)
	{
		char szName[256];
		cry_sprintf(szName, m_graphicsPipeline.MakeUniqueTexIdentifierName("$HDRMeasuredLum_%d").c_str(), i);
		m_pTexHDRMeasuredLuminance[i] = CTexture::GetOrCreate2DTexture(szName, 1, 1, 0, FT_DONT_RELEASE | FT_DONT_STREAM, NULL, eTF_R16G16F);
	}
#endif

	m_renderTargetPool.CreateRenderTargetList();
}

void CGraphicsPipelineResources::CreatePostFXMaps(int resourceWidth, int resourceHeight)
{
#if RENDERER_ENABLE_FULL_PIPELINE
	const int width = resourceWidth, width_r2 = (width + 1) / 2, width_r4 = (width_r2 + 1) / 2, width_r8 = (width_r4 + 1) / 2;
	const int height = resourceHeight, height_r2 = (height + 1) / 2, height_r4 = (height_r2 + 1) / 2, height_r8 = (height_r4 + 1) / 2;

	CRY_DISABLE_WARN_UNUSED_VARIABLES();
	const ETEX_Format nHDRFormat = CRendererResources::GetHDRFormat(false, false); // No alpha, default is HiQ, can be downgraded
	const ETEX_Format nHDRAFormat = CRendererResources::GetHDRFormat(true, false); // With alpha
	const ETEX_Format nHDRQFormat = CRendererResources::GetHDRFormat(false, true); // No alpha, default is LoQ, can be upgraded
	const ETEX_Format nLDRPFormat = CRendererResources::GetLDRFormat(true);         // With more than 8 mantissa bits for calculations
	CRY_RESTORE_WARN_UNUSED_VARIABLES();

	if (RainOcclusionMapsEnabled())
		CreateRainOcclusionMaps(resourceWidth, resourceHeight);

	SPostEffectsUtils::GetOrCreateRenderTarget(m_graphicsPipeline.MakeUniqueTexIdentifierName("$DisplayTarget 1/2a").c_str(), m_pTexDisplayTargetScaled[0], width_r2, height_r2, Clr_Unknown, 1, 0, nLDRPFormat, TO_BACKBUFFERSCALED_D2, FT_DONT_RELEASE);
	SPostEffectsUtils::GetOrCreateRenderTarget(m_graphicsPipeline.MakeUniqueTexIdentifierName("$DisplayTarget 1/4a").c_str(), m_pTexDisplayTargetScaled[1], width_r4, height_r4, Clr_Unknown, 1, 0, nLDRPFormat, TO_BACKBUFFERSCALED_D4, FT_DONT_RELEASE);
	SPostEffectsUtils::GetOrCreateRenderTarget(m_graphicsPipeline.MakeUniqueTexIdentifierName("$DisplayTarget 1/8").c_str(), m_pTexDisplayTargetScaled[2], width_r8, height_r8, Clr_Unknown, 1, 0, nLDRPFormat, TO_BACKBUFFERSCALED_D8, FT_DONT_RELEASE);

	// Scaled versions of the scene target
	SPostEffectsUtils::GetOrCreateRenderTarget(m_graphicsPipeline.MakeUniqueTexIdentifierName("$DisplayTarget").c_str(), m_pTexDisplayTargetSrc, width, height, Clr_Unknown, 1, 0, nLDRPFormat, TO_BACKBUFFERMAP, FT_DONT_RELEASE);
	SPostEffectsUtils::GetOrCreateRenderTarget(m_graphicsPipeline.MakeUniqueTexIdentifierName("$DisplayTargetDst").c_str(), m_pTexDisplayTargetDst, width, height, Clr_Unknown, 1, 0, nLDRPFormat, TO_BACKBUFFERMAP, FT_DONT_RELEASE);

	SPostEffectsUtils::GetOrCreateRenderTarget(m_graphicsPipeline.MakeUniqueTexIdentifierName("$DisplayTarget 1/2b").c_str(), m_pTexDisplayTargetScaledTemp[0], width_r2, height_r2, Clr_Unknown, 1, 0, nLDRPFormat, -1, FT_DONT_RELEASE);
	SPostEffectsUtils::GetOrCreateRenderTarget(m_graphicsPipeline.MakeUniqueTexIdentifierName("$DisplayTarget 1/4b").c_str(), m_pTexDisplayTargetScaledTemp[1], width_r4, height_r4, Clr_Unknown, 1, 0, nLDRPFormat, -1, FT_DONT_RELEASE);

	SPostEffectsUtils::GetOrCreateRenderTarget(m_graphicsPipeline.MakeUniqueTexIdentifierName("$Cached3DHud").c_str(), m_pTexCached3DHud, width, height, Clr_Unknown, 1, 0, eTF_R8G8B8A8, -1, FT_DONT_RELEASE);
	SPostEffectsUtils::GetOrCreateRenderTarget(m_graphicsPipeline.MakeUniqueTexIdentifierName("$Cached3DHudDownsampled").c_str(), m_pTexCached3DHudScaled, width_r4, height_r4, Clr_Unknown, 1, 0, eTF_R8G8B8A8, -1, FT_DONT_RELEASE);

	SPostEffectsUtils::GetOrCreateRenderTarget(m_graphicsPipeline.MakeUniqueTexIdentifierName("$WaterVolumeRefl").c_str(), m_pTexWaterVolumeRefl[0], width_r2, height_r2, Clr_Unknown, 1, true, nHDRQFormat, TO_WATERVOLUMEREFLMAP, FT_DONT_RELEASE);
	SPostEffectsUtils::GetOrCreateRenderTarget(m_graphicsPipeline.MakeUniqueTexIdentifierName("$WaterVolumeReflPrev").c_str(), m_pTexWaterVolumeRefl[1], width_r2, height_r2, Clr_Unknown, 1, true, nHDRQFormat, TO_WATERVOLUMEREFLMAPPREV, FT_DONT_RELEASE);
#endif
}

void CGraphicsPipelineResources::CreateSceneMaps(int resourceWidth, int resourceHeight)
{
	const int32 nWidth = resourceWidth;
	const int32 nHeight = resourceHeight;
	const ETEX_Format eHDRTF = CRendererResources::GetHDRFormat(false, false);
	uint32 nFlags = FT_DONT_STREAM | FT_USAGE_RENDERTARGET | FT_USAGE_UNORDERED_ACCESS;

	if (!m_pTexSceneTarget)
		m_pTexSceneTarget = CTexture::GetOrCreateRenderTarget(m_graphicsPipeline.MakeUniqueTexIdentifierName("$SceneTarget").c_str(), nWidth, nHeight, Clr_Empty, eTT_2D, nFlags, eHDRTF, TO_SCENE_TARGET);
	else
	{
		m_pTexSceneTarget->SetFlags(nFlags);
		m_pTexSceneTarget->SetWidth(nWidth);
		m_pTexSceneTarget->SetHeight(nHeight);
		m_pTexSceneTarget->CreateRenderTarget(eHDRTF, Clr_Empty);
	}

	// This RT can be used by the Render3DModelMgr if the buffer needs to be persistent
	if (CRenderer::CV_r_UsePersistentRTForModelHUD > 0)
	{
		if (!CTexture::IsTextureExist(m_pTexModelHudBuffer))
			m_pTexModelHudBuffer = CTexture::GetOrCreateRenderTarget(m_graphicsPipeline.MakeUniqueTexIdentifierName("$ModelHUD").c_str(), nWidth, nHeight, Clr_Transparent, eTT_2D, nFlags, eTF_R8G8B8A8);
		else
		{
			m_pTexModelHudBuffer->SetFlags(nFlags);
			m_pTexModelHudBuffer->SetWidth(nWidth);
			m_pTexModelHudBuffer->SetHeight(nHeight);
			m_pTexModelHudBuffer->CreateRenderTarget(eTF_R8G8B8A8, Clr_Transparent);
		}
	}

	if (gEnv->IsEditor())
	{
		SD3DPostEffectsUtils::GetOrCreateRenderTarget(m_graphicsPipeline.MakeUniqueTexIdentifierName("$SceneSelectionIDs").c_str(), m_pTexSceneSelectionIDs, nWidth, nHeight, Clr_Transparent, false, false, eTF_R32F, -1, nFlags);
	}
}

void CGraphicsPipelineResources::PrepareRainOcclusionMaps()
{
	if (!m_pTexRainOcclusion)
	{
		m_pTexRainOcclusion = CTexture::GetOrCreateTextureObject(m_graphicsPipeline.MakeUniqueTexIdentifierName("$RainOcclusion").c_str(), RAIN_OCC_MAP_SIZE, RAIN_OCC_MAP_SIZE, 1, eTT_2D, FT_DONT_RELEASE | FT_DONT_STREAM | FT_USAGE_RENDERTARGET, eTF_R8G8B8A8);
		m_pTexRainSSOcclusion[0] = CTexture::GetOrCreateTextureObject(m_graphicsPipeline.MakeUniqueTexIdentifierName("$RainSSOcclusion0").c_str(), 0, 0, 1, eTT_2D, FT_DONT_RELEASE | FT_DONT_STREAM | FT_USAGE_RENDERTARGET, eTF_Unknown);
		m_pTexRainSSOcclusion[1] = CTexture::GetOrCreateTextureObject(m_graphicsPipeline.MakeUniqueTexIdentifierName("$RainSSOcclusion1").c_str(), 0, 0, 1, eTT_2D, FT_DONT_RELEASE | FT_DONT_STREAM | FT_USAGE_RENDERTARGET, eTF_Unknown);
	}
}

void CGraphicsPipelineResources::CreateRainOcclusionMaps(int resourceWidth, int resourceHeight)
{
	PrepareRainOcclusionMaps();

	if (!CTexture::IsTextureExist(m_pTexRainOcclusion))
		SPostEffectsUtils::GetOrCreateRenderTarget(m_graphicsPipeline.MakeUniqueTexIdentifierName("$RainOcclusion").c_str(), m_pTexRainOcclusion, RAIN_OCC_MAP_SIZE, RAIN_OCC_MAP_SIZE, Clr_Neutral, false, false, eTF_R8, -1, FT_DONT_RELEASE);

	const int width_r8 = (resourceWidth + 7) / 8;
	const int height_r8 = (resourceHeight + 7) / 8;

	if (!m_pTexRainSSOcclusion[0] ||
		m_pTexRainSSOcclusion[0]->GetWidth() != width_r8 ||
		m_pTexRainSSOcclusion[0]->GetHeight() != height_r8)
	{
		SPostEffectsUtils::GetOrCreateRenderTarget(m_graphicsPipeline.MakeUniqueTexIdentifierName("$RainSSOcclusion0").c_str(), m_pTexRainSSOcclusion[0], width_r8, height_r8, Clr_Unknown, 1, false, eTF_R8);
		SPostEffectsUtils::GetOrCreateRenderTarget(m_graphicsPipeline.MakeUniqueTexIdentifierName("$RainSSOcclusion1").c_str(), m_pTexRainSSOcclusion[1], width_r8, height_r8, Clr_Unknown, 1, false, eTF_R8);
	}
}

void CGraphicsPipelineResources::DestroyRainOcclusionMaps()
{
	SAFE_RELEASE_FORCE(m_pTexRainOcclusion);
	SAFE_RELEASE_FORCE(m_pTexRainSSOcclusion[0]);
	SAFE_RELEASE_FORCE(m_pTexRainSSOcclusion[1]);
}

void CGraphicsPipelineResources::OnCVarsChanged(const CCVarUpdateRecorder& rCVarRecs)
{
	const bool enabled = RainOcclusionMapsEnabled();
	if (enabled != RainOcclusionMapsInitialized())
	{
		if (enabled)
			CreateRainOcclusionMaps(m_resourceWidth, m_resourceHeight);
		else
			DestroyRainOcclusionMaps();
	}

	if (rCVarRecs.GetCVar("r_hdrtexformat"))
	{
		const int hdrTexFormat = rCVarRecs.GetCVar("r_hdrtexformat")->intValue;

		if (hdrTexFormat && CTexture::IsTextureExist(m_pTexLinearDepthFixup))
		{
			m_pTexLinearDepthFixup->ReleaseDeviceTexture(false);
		}
		else if (!hdrTexFormat && !CTexture::IsTextureExist(m_pTexLinearDepthFixup))
		{
			uint32 nUAFlags = FT_DONT_STREAM | FT_DONT_RELEASE | FT_USAGE_RENDERTARGET | FT_USAGE_UNORDERED_ACCESS | FT_USAGE_UAV_RWTEXTURE;

			m_pTexLinearDepthFixup->SetFlags(nUAFlags);
			m_pTexLinearDepthFixup->SetWidth(m_resourceWidth);
			m_pTexLinearDepthFixup->SetHeight(m_resourceHeight);
			m_pTexLinearDepthFixup->CreateRenderTarget(eTF_R32F, ColorF(1.0f, 1.0f, 1.0f, 1.0f));

			SResourceView typedUAV = SResourceView::UnorderedAccessView(DXGI_FORMAT_R32_UINT, 0, -1, 0, SResourceView::eUAV_ReadWrite);
			m_pTexLinearDepthFixupUAV = m_pTexLinearDepthFixup->GetDevTexture()->GetOrCreateResourceViewHandle(typedUAV);
		}
	}
}

void CGraphicsPipelineResources::Update(EShaderRenderingFlags renderingFlags)
{
	const auto shouldApplyOcclusion = gcpRendD3D->m_bDeferredRainOcclusionEnabled && CRendererCVars::IsRainEnabled();

	// Create/release the occlusion texture on demand
	if (!shouldApplyOcclusion && CTexture::IsTextureExist(m_pTexRainOcclusion))
		m_pTexRainOcclusion->ReleaseDeviceTexture(false);
	else if (shouldApplyOcclusion && !CTexture::IsTextureExist(m_pTexRainOcclusion))
		m_pTexRainOcclusion->CreateRenderTarget(eTF_R8, Clr_Neutral);
}

bool CGraphicsPipelineResources::RainOcclusionMapsEnabled()
{
	return CRendererCVars::IsRainEnabled() || CRendererCVars::IsSnowEnabled();
}

void CGraphicsPipelineResources::Shutdown()
{
	DestroyRainOcclusionMaps();

	SAFE_RELEASE_FORCE(m_pTexHDRTarget);
	SAFE_RELEASE_FORCE(m_pTexHDRTargetPrev);
	SAFE_RELEASE_FORCE(m_pTexHDRTargetMasked);
	SAFE_RELEASE_FORCE(m_pTexLinearDepth);
	SAFE_RELEASE_FORCE(m_pTexSceneDiffuse);
	SAFE_RELEASE_FORCE(m_pTexSceneNormalsMap);
	SAFE_RELEASE_FORCE(m_pTexSceneSpecular);
	SAFE_RELEASE_FORCE(m_pTexVelocityObjects[0]);
	SAFE_RELEASE_FORCE(m_pTexVelocityObjects[1]);
	SAFE_RELEASE_FORCE(m_pTexShadowMask);
	SAFE_RELEASE_FORCE(m_pTexSceneNormalsBent);
	SAFE_RELEASE_FORCE(m_pTexLinearDepthScaled[0]);
	SAFE_RELEASE_FORCE(m_pTexLinearDepthScaled[1]);
	SAFE_RELEASE_FORCE(m_pTexLinearDepthScaled[2]);
	SAFE_RELEASE_FORCE(m_pTexDisplayTargetScaledTemp[0]);
	SAFE_RELEASE_FORCE(m_pTexDisplayTargetScaledTemp[1]);
	SAFE_RELEASE_FORCE(m_pTexSceneDepthScaled[0]);
	SAFE_RELEASE_FORCE(m_pTexSceneDepthScaled[1]);
	SAFE_RELEASE_FORCE(m_pTexSceneDepthScaled[2]);
	SAFE_RELEASE_FORCE(m_pTexClipVolumes);
	SAFE_RELEASE_FORCE(m_pTexAOColorBleed);
	SAFE_RELEASE_FORCE(m_pTexSceneDiffuseTmp);
	SAFE_RELEASE_FORCE(m_pTexSceneSpecularTmp);
	SAFE_RELEASE_FORCE(m_pTexDisplayTargetScaled[0]);
	SAFE_RELEASE_FORCE(m_pTexDisplayTargetScaled[1]);
	SAFE_RELEASE_FORCE(m_pTexDisplayTargetScaled[2]);
	SAFE_RELEASE_FORCE(m_pTexDisplayTargetDst);
	SAFE_RELEASE_FORCE(m_pTexDisplayTargetSrc);
	SAFE_RELEASE_FORCE(m_pTexSceneSelectionIDs);
	SAFE_RELEASE_FORCE(m_pTexSceneTargetR11G11B10F[0]);
	SAFE_RELEASE_FORCE(m_pTexSceneTargetR11G11B10F[1]);
	SAFE_RELEASE_FORCE(m_pTexLinearDepthFixup);
	SAFE_RELEASE_FORCE(m_pTexSceneTarget);
	SAFE_RELEASE_FORCE(m_pTexHDRFinalBloom);
	SAFE_RELEASE_FORCE(m_pTexVelocity);
	SAFE_RELEASE_FORCE(m_pTexVelocityTiles[0]);
	SAFE_RELEASE_FORCE(m_pTexVelocityTiles[1]);
	SAFE_RELEASE_FORCE(m_pTexVelocityTiles[2]);
	SAFE_RELEASE_FORCE(m_pTexWaterVolumeRefl[0]);
	SAFE_RELEASE_FORCE(m_pTexWaterVolumeRefl[1]);
	SAFE_RELEASE_FORCE(m_pTexCached3DHud);
	SAFE_RELEASE_FORCE(m_pTexCached3DHudScaled);

	SAFE_RELEASE_FORCE(m_pTexSceneCoCTemp);
	for (int i = 0; i < MIN_DOF_COC_K; i++)
	{
		SAFE_RELEASE_FORCE(m_pTexSceneCoC[i]);
	}

	if (CRenderer::CV_r_UsePersistentRTForModelHUD > 0)
	{
		SAFE_RELEASE_FORCE(m_pTexModelHudBuffer);
	}

	for (int i = 0; i < MAX_GPU_NUM; ++i)
	{
		SAFE_RELEASE_FORCE(m_pTexHDRMeasuredLuminance[i]);
	}

	for (int i = 0; i < 4; ++i)
	{
		for (int j = 0; j < 4; ++j)
		{
			SAFE_RELEASE_FORCE(m_pTexHDRTargetScaled[i][j]);
			SAFE_RELEASE_FORCE(m_pTexHDRTargetMaskedScaled[i][j]);
		}
	}

	m_resourceWidth  = 0;
	m_resourceHeight = 0;
}

void CGraphicsPipelineResources::Discard()
{
	// DISCARD RESOURCES
	//------------------------------------------------------------------------------
#if (CRY_RENDERER_DIRECT3D >= 111)
	gcpRendD3D->GetDeviceContext().DiscardResource(m_pTexHDRTarget->GetDevTexture(false)->GetNativeResource());
	gcpRendD3D->GetDeviceContext().DiscardResource(m_pTexHDRTargetPrev->GetDevTexture(false)->GetNativeResource());
	gcpRendD3D->GetDeviceContext().DiscardResource(m_pTexHDRTargetMasked->GetDevTexture(false)->GetNativeResource());
	gcpRendD3D->GetDeviceContext().DiscardResource(m_pTexLinearDepth->GetDevTexture(false)->GetNativeResource());
	gcpRendD3D->GetDeviceContext().DiscardResource(m_pTexSceneDiffuse->GetDevTexture(false)->GetNativeResource());
	gcpRendD3D->GetDeviceContext().DiscardResource(m_pTexSceneNormalsMap->GetDevTexture(false)->GetNativeResource());
	gcpRendD3D->GetDeviceContext().DiscardResource(m_pTexSceneSpecular->GetDevTexture(false)->GetNativeResource());
	gcpRendD3D->GetDeviceContext().DiscardResource(m_pTexVelocityObjects[0]->GetDevTexture(false)->GetNativeResource());
	gcpRendD3D->GetDeviceContext().DiscardResource(m_pTexVelocityObjects[1]->GetDevTexture(false)->GetNativeResource());
	gcpRendD3D->GetDeviceContext().DiscardResource(m_pTexShadowMask->GetDevTexture(false)->GetNativeResource());
	gcpRendD3D->GetDeviceContext().DiscardResource(m_pTexSceneNormalsBent->GetDevTexture(false)->GetNativeResource());
	gcpRendD3D->GetDeviceContext().DiscardResource(m_pTexLinearDepthScaled[0]->GetDevTexture(false)->GetNativeResource());
	gcpRendD3D->GetDeviceContext().DiscardResource(m_pTexLinearDepthScaled[1]->GetDevTexture(false)->GetNativeResource());
	gcpRendD3D->GetDeviceContext().DiscardResource(m_pTexLinearDepthScaled[2]->GetDevTexture(false)->GetNativeResource());
	gcpRendD3D->GetDeviceContext().DiscardResource(m_pTexSceneDepthScaled[0]->GetDevTexture(false)->GetNativeResource());
	gcpRendD3D->GetDeviceContext().DiscardResource(m_pTexSceneDepthScaled[1]->GetDevTexture(false)->GetNativeResource());
	gcpRendD3D->GetDeviceContext().DiscardResource(m_pTexSceneDepthScaled[2]->GetDevTexture(false)->GetNativeResource());
	gcpRendD3D->GetDeviceContext().DiscardResource(m_pTexClipVolumes->GetDevTexture(false)->GetNativeResource());
	gcpRendD3D->GetDeviceContext().DiscardResource(m_pTexAOColorBleed->GetDevTexture(false)->GetNativeResource());
	gcpRendD3D->GetDeviceContext().DiscardResource(m_pTexSceneDiffuseTmp->GetDevTexture(false)->GetNativeResource());
	gcpRendD3D->GetDeviceContext().DiscardResource(m_pTexSceneSpecularTmp->GetDevTexture(false)->GetNativeResource());
	gcpRendD3D->GetDeviceContext().DiscardResource(m_pTexDisplayTargetScaled[0]->GetDevTexture(false)->GetNativeResource());
	gcpRendD3D->GetDeviceContext().DiscardResource(m_pTexDisplayTargetScaled[1]->GetDevTexture(false)->GetNativeResource());
	gcpRendD3D->GetDeviceContext().DiscardResource(m_pTexDisplayTargetScaled[2]->GetDevTexture(false)->GetNativeResource());
	gcpRendD3D->GetDeviceContext().DiscardResource(m_pTexDisplayTargetDst->GetDevTexture(false)->GetNativeResource());
	gcpRendD3D->GetDeviceContext().DiscardResource(m_pTexDisplayTargetSrc-> GetDevTexture(false)->GetNativeResource());
	gcpRendD3D->GetDeviceContext().DiscardResource(m_pTexSceneSelectionIDs->GetDevTexture(false)->GetNativeResource());
	gcpRendD3D->GetDeviceContext().DiscardResource(m_pTexSceneTargetR11G11B10F[0]->GetDevTexture(false)->GetNativeResource());
	gcpRendD3D->GetDeviceContext().DiscardResource(m_pTexSceneTargetR11G11B10F[1]->GetDevTexture(false)->GetNativeResource());
	gcpRendD3D->GetDeviceContext().DiscardResource(m_pTexLinearDepthFixup->GetDevTexture(false)->GetNativeResource());
	gcpRendD3D->GetDeviceContext().DiscardResource(m_pTexSceneTarget->GetDevTexture(false)->GetNativeResource());

	for (int i = 0; i < MAX_GPU_NUM; ++i)
	{
		gcpRendD3D->GetDeviceContext().DiscardResource(m_pTexHDRMeasuredLuminance[i]->GetDevTexture(false)->GetNativeResource());
	}

	for (int i = 0; i < 4; ++i)
	{
		for (int j = 0; j < 4; ++j)
		{
			gcpRendD3D->GetDeviceContext().DiscardResource(m_pTexHDRTargetScaled[i][j]->GetDevTexture(false)->GetNativeResource());
			gcpRendD3D->GetDeviceContext().DiscardResource(m_pTexHDRTargetMaskedScaled[i][j]->GetDevTexture(false)->GetNativeResource());
		}
	}
#endif
}

void CGraphicsPipelineResources::Clear()
{
	CTexture* clearTextures[] =
	{
		m_pTexSceneNormalsMap,
		m_pTexSceneDiffuse,
		m_pTexSceneSpecular,
		m_pTexSceneDiffuseTmp,
		m_pTexSceneSpecularTmp,
		m_pTexDisplayTargetSrc,
		m_pTexDisplayTargetDst,
		m_pTexLinearDepth,
		m_pTexHDRTarget,
		m_pTexSceneTarget
	};

	for (auto pTex : clearTextures)
	{
		if (CTexture::IsTextureExist(pTex))
		{
			CClearSurfacePass::Execute(pTex, pTex->GetClearColor());
		}
	}
}

//////////////////////////////////////////////////////////////////////////
CGraphicsPipeline::CGraphicsPipeline(const IRenderer::SGraphicsPipelineDescription& desc, const std::string& uniqueIdentifier, const SGraphicsPipelineKey key)
	: m_changedCVars(gEnv->pConsole)
	, m_pipelineResources(*this)
	, m_pipelineDesc(desc)
	, m_uniquePipelineIdentifierName(uniqueIdentifier)
	, m_key(key)
{
	m_renderingFlags = (EShaderRenderingFlags)desc.shaderFlags;
	m_pipelineStages.fill(nullptr);
	m_pVRProjectionManager = nullptr;
}

//////////////////////////////////////////////////////////////////////////
CGraphicsPipeline::~CGraphicsPipeline()
{
	ShutDown();
}

//////////////////////////////////////////////////////////////////////////
void CGraphicsPipeline::ClearState()
{
	GetDeviceObjectFactory().GetCoreCommandList().Reset();
}

void CGraphicsPipeline::ClearDeviceState()
{
	GetDeviceObjectFactory().GetCoreCommandList().GetGraphicsInterface()->ClearState(false);
}

//////////////////////////////////////////////////////////////////////////
void CGraphicsPipeline::Init()
{
	// per view constant buffer
	m_mainViewConstantBuffer.CreateDeviceBuffer();

	m_pDeferredShading = new CDeferredShading(this);

	m_pipelineResources.Init();	

	// Register scene stages that make use of the global PSO cache
	RegisterStage<CSceneGBufferStage>();
	RegisterStage<CSceneForwardStage>();
	RegisterStage<CShadowMapStage>();

	// Register all other stages that don't need the global PSO cache
	RegisterStage<CTiledLightVolumesStage>();
	RegisterStage<CClipVolumesStage>();
	RegisterStage<CFogStage>();
	RegisterStage<CDebugRenderTargetsStage>();

	RegisterStage<CSkyStage>();

	m_pVRProjectionManager = gcpRendD3D->m_pVRProjectionManager;

	// Initializes all the pipeline stages.
	for (auto it = m_pipelineStages.begin(); it != m_pipelineStages.end(); ++it)
	{
		if (*it) (*it)->Init();
	}
}

//////////////////////////////////////////////////////////////////////////
void CGraphicsPipeline::ShutDown()
{
	m_pipelineResources.Shutdown();

	m_mainViewConstantBuffer.Clear();

	SAFE_DELETE(m_pDeferredShading);

	// destroy stages in reverse order to satisfy data dependencies
	for (auto it = m_pipelineStages.rbegin(); it != m_pipelineStages.rend(); ++it)
	{
		if (*it)
			delete *it;
	}

	m_pipelineStages.fill(nullptr);
}

//////////////////////////////////////////////////////////////////////////

bool CGraphicsPipeline::CreatePipelineStates(DevicePipelineStatesArray* pStateArray, SGraphicsPipelineStateDescription stateDesc, CGraphicsPipelineStateLocalCache* pStateCache)
{
	// NOTE: Please update SDeviceObjectHelpers::CheckTessellationSupport when adding new techniques types here.

	bool bFullyCompiled = true;

	// GBuffer
	{
		stateDesc.technique = TTYPE_Z;
		bFullyCompiled &= GetStage<CSceneGBufferStage>()->CreatePipelineStates(pStateArray, stateDesc, pStateCache);
	}

	// ShadowMap
	{
		stateDesc.technique = TTYPE_SHADOWGEN;
		bFullyCompiled &= GetStage<CShadowMapStage>()->CreatePipelineStates(pStateArray, stateDesc, pStateCache);
	}

	// Forward
	{
		stateDesc.technique = TTYPE_GENERAL;
		bFullyCompiled &= GetStage<CSceneForwardStage>()->CreatePipelineStates(pStateArray, stateDesc, pStateCache);
	}

#if RENDERER_ENABLE_FULL_PIPELINE
	// Custom
	{
		stateDesc.technique = TTYPE_DEBUG;
		bFullyCompiled &= GetStage<CSceneCustomStage>()->CreatePipelineStates(pStateArray, stateDesc, pStateCache);
	}
#endif

	return bFullyCompiled;
}

//////////////////////////////////////////////////////////////////////////
CDeviceResourceLayoutPtr CGraphicsPipeline::CreateScenePassLayout(const CDeviceResourceSetDesc& perPassResources)
{
	SDeviceResourceLayoutDesc layoutDesc;

	layoutDesc.SetConstantBuffer(EResourceLayoutSlot_PerDrawCB, eConstantBufferShaderSlot_PerDraw, EShaderStage_Vertex | EShaderStage_Pixel | EShaderStage_Domain);

	layoutDesc.SetResourceSet(EResourceLayoutSlot_PerDrawExtraRS, CSceneRenderPass::GetDefaultDrawExtraResourceLayout());
	layoutDesc.SetResourceSet(EResourceLayoutSlot_PerMaterialRS, CSceneRenderPass::GetDefaultMaterialBindPoints());
	layoutDesc.SetResourceSet(EResourceLayoutSlot_PerPassRS, perPassResources);

	CDeviceResourceLayoutPtr pResourceLayout = GetDeviceObjectFactory().CreateResourceLayout(layoutDesc);
	assert(pResourceLayout != nullptr);
	return pResourceLayout;
}

//////////////////////////////////////////////////////////////////////////
void CGraphicsPipeline::Resize(int renderWidth, int renderHeight)
{
	// Sets the current render resolution on all the pipeline stages.
	for (auto it = m_pipelineStages.begin(); it != m_pipelineStages.end(); ++it)
	{
		if (*it)
			(*it)->Resize(renderWidth, renderHeight);
	}

	m_pipelineResources.Resize(renderWidth, renderHeight);

	m_renderWidth  = renderWidth;
	m_renderHeight = renderHeight;
}

//////////////////////////////////////////////////////////////////////////
void CGraphicsPipeline::SetCurrentRenderView(CRenderView* pRenderView)
{
	m_pCurrentRenderView = pRenderView;

	// Sets the current render view on all the pipeline stages.
	for (auto it = m_pipelineStages.begin(); it != m_pipelineStages.end(); ++it)
	{
		if (*it)
			(*it)->SetRenderView(pRenderView);
	}
}

//////////////////////////////////////////////////////////////////////////
void CGraphicsPipeline::Update(EShaderRenderingFlags renderingFlags)
{
	m_pipelineResources.Update(renderingFlags);

	for (auto it = m_pipelineStages.begin(); it != m_pipelineStages.end(); ++it)
	{
		if (*it && (*it)->IsStageActive(renderingFlags))
			(*it)->Update();
	}
}

//////////////////////////////////////////////////////////////////////////
void CGraphicsPipeline::OnCVarsChanged(const CCVarUpdateRecorder& rCVarRecs)
{
	m_pipelineResources.OnCVarsChanged(rCVarRecs);

	for (auto it = m_pipelineStages.begin(); it != m_pipelineStages.end(); ++it)
	{
		if (*it)
			(*it)->OnCVarsChanged(rCVarRecs);
	}
}

//////////////////////////////////////////////////////////////////////////
std::array<SamplerStateHandle, EFSS_MAX> CGraphicsPipeline::GetDefaultMaterialSamplers() const
{
	std::array<SamplerStateHandle, EFSS_MAX> result =
	{
		{
			gcpRendD3D->m_nMaterialAnisoHighSampler,                                                                                                                                         // EFSS_ANISO_HIGH
			gcpRendD3D->m_nMaterialAnisoLowSampler,                                                                                                                                          // EFSS_ANISO_LOW
			CDeviceObjectFactory::GetOrCreateSamplerStateHandle(SSamplerState(FILTER_TRILINEAR, eSamplerAddressMode_Wrap, eSamplerAddressMode_Wrap, eSamplerAddressMode_Wrap, 0x0)),         // EFSS_TRILINEAR
			CDeviceObjectFactory::GetOrCreateSamplerStateHandle(SSamplerState(FILTER_BILINEAR, eSamplerAddressMode_Wrap, eSamplerAddressMode_Wrap, eSamplerAddressMode_Wrap, 0x0)),          // EFSS_BILINEAR
			CDeviceObjectFactory::GetOrCreateSamplerStateHandle(SSamplerState(FILTER_TRILINEAR, eSamplerAddressMode_Clamp, eSamplerAddressMode_Clamp, eSamplerAddressMode_Clamp, 0x0)),      // EFSS_TRILINEAR_CLAMP
			CDeviceObjectFactory::GetOrCreateSamplerStateHandle(SSamplerState(FILTER_BILINEAR, eSamplerAddressMode_Clamp, eSamplerAddressMode_Clamp, eSamplerAddressMode_Clamp, 0x0)),       // EFSS_BILINEAR_CLAMP
			gcpRendD3D->m_nMaterialAnisoSamplerBorder,                                                                                                                                       // EFSS_ANISO_HIGH_BORDER
			CDeviceObjectFactory::GetOrCreateSamplerStateHandle(SSamplerState(FILTER_TRILINEAR, eSamplerAddressMode_Border, eSamplerAddressMode_Border, eSamplerAddressMode_Border, 0x0)),   // EFSS_TRILINEAR_BORDER
		} };

	return result;
}

//////////////////////////////////////////////////////////////////////////
void CGraphicsPipeline::ExecuteAnisotropicVerticalBlur(CTexture* pTex, int nAmount, float fScale, float fDistribution, bool bAlphaOnly)
{
	m_AnisoVBlurPass->Execute(pTex, nAmount, fScale, fDistribution, bAlphaOnly);
}

//////////////////////////////////////////////////////////////////////////
const SRenderViewInfo& CGraphicsPipeline::GetCurrentViewInfo(CCamera::EEye eye) const
{
	const CRenderView* pRenderView = GetCurrentRenderView();
	CRY_ASSERT(pRenderView);
	if (pRenderView)
	{
		return pRenderView->GetViewInfo(eye);
	}

	static SRenderViewInfo viewInfo;
	return viewInfo;
}

void CGraphicsPipeline::SetParticleBuffers(bool bOnInit, CDeviceResourceSetDesc& resources, ResourceViewHandle hView, EShaderStage shaderStages) const
{
	if (!bOnInit && m_pCurrentRenderView)
	{
		int frameId = m_pCurrentRenderView->GetFrameId();
		const CParticleBufferSet& particleBuffer = gcpRendD3D.GetParticleBufferSet();
		const auto positionStream = particleBuffer.GetPositionStream(frameId);
		const auto axesStream     = particleBuffer.GetAxesStream(frameId);
		const auto colorStream    = particleBuffer.GetColorSTsStream(frameId);
		if (positionStream && axesStream && colorStream)
		{
			resources.SetBuffer(EReservedTextureSlot_ParticlePositionStream, const_cast<CGpuBuffer*>(positionStream), hView, shaderStages);
			resources.SetBuffer(EReservedTextureSlot_ParticleAxesStream,     const_cast<CGpuBuffer*>(axesStream),     hView, shaderStages);
			resources.SetBuffer(EReservedTextureSlot_ParticleColorSTStream,  const_cast<CGpuBuffer*>(colorStream),    hView, shaderStages);
			return;
		}
	}

	auto nullBuffer = CDeviceBufferManager::GetNullBufferStructured();
	resources.SetBuffer(EReservedTextureSlot_ParticlePositionStream, nullBuffer, hView, shaderStages);
	resources.SetBuffer(EReservedTextureSlot_ParticleAxesStream,     nullBuffer, hView, shaderStages);
	resources.SetBuffer(EReservedTextureSlot_ParticleColorSTStream,  nullBuffer, hView, shaderStages);
}

//////////////////////////////////////////////////////////////////////////
void CGraphicsPipeline::ExecutePostAA()
{
	auto* pStage = GetStage<CPostAAStage>();
	CRY_ASSERT(pStage);
	pStage->Execute();
}

//////////////////////////////////////////////////////////////////////////
void CGraphicsPipeline::GeneratePerViewConstantBuffer(const SRenderViewInfo* pViewInfo, int viewInfoCount, CConstantBufferPtr pPerViewBuffer, const SRenderViewport* pCustomViewport)
{
	CRY_ASSERT(m_pCurrentRenderView);
	if (!gEnv->p3DEngine || !pPerViewBuffer)
		return;

	const SRenderViewShaderConstants& perFrameConstants = m_pCurrentRenderView->GetShaderConstants();
	CryStackAllocWithSizeVectorCleared(HLSL_PerViewGlobalConstantBuffer, viewInfoCount, bufferData, CDeviceBufferManager::AlignBufferSizeForStreaming);

	const SRenderGlobalFogDescription& globalFog = m_pCurrentRenderView->GetGlobalFog();

	for (int i = 0; i < viewInfoCount; ++i)
	{
		CRY_ASSERT(pViewInfo[i].pCamera);

		const SRenderViewInfo& viewInfo = pViewInfo[i];

		HLSL_PerViewGlobalConstantBuffer& cb = bufferData[i];

		const float animTime = GetAnimationTime().GetSeconds();
		const bool bReverseDepth = (viewInfo.flags & SRenderViewInfo::eFlags_ReverseDepth) != 0;

		cb.CV_HPosScale = viewInfo.downscaleFactor;

		SRenderViewport viewport = pCustomViewport ? *pCustomViewport : viewInfo.viewport;
		cb.CV_ScreenSize = Vec4(float(viewport.width),
		                        float(viewport.height),
		                        0.5f / (viewport.width / viewInfo.downscaleFactor.x),
		                        0.5f / (viewport.height / viewInfo.downscaleFactor.y));

		cb.CV_ViewProjZeroMatr = viewInfo.cameraProjZeroMatrix.GetTransposed();
		cb.CV_ViewProjMatr = viewInfo.cameraProjMatrix.GetTransposed();
		cb.CV_ViewProjNearestMatr = viewInfo.cameraProjNearestMatrix.GetTransposed();
		cb.CV_InvViewProj = viewInfo.invCameraProjMatrix.GetTransposed();
		cb.CV_PrevViewProjMatr = viewInfo.prevCameraProjMatrix.GetTransposed();
		cb.CV_PrevViewProjNearestMatr = viewInfo.prevCameraProjNearestMatrix.GetTransposed();
		cb.CV_ViewMatr = viewInfo.viewMatrix.GetTransposed();
		cb.CV_InvViewMatr = viewInfo.invViewMatrix.GetTransposed();

		Vec4r vWBasisX, vWBasisY, vWBasisZ, vCamPos;
		CShadowUtils::ProjectScreenToWorldExpansionBasis(Matrix44(IDENTITY), *viewInfo.pCamera, m_pCurrentRenderView->m_vProjMatrixSubPixoffset,
		                                                 float(viewport.width), float(viewport.height), vWBasisX, vWBasisY, vWBasisZ, vCamPos, true);

		cb.CV_ScreenToWorldBasis.SetColumn(0, Vec3r(vWBasisX));
		cb.CV_ScreenToWorldBasis.SetColumn(1, Vec3r(vWBasisY));
		cb.CV_ScreenToWorldBasis.SetColumn(2, Vec3r(vWBasisZ));
		cb.CV_ScreenToWorldBasis.SetColumn(3, viewInfo.cameraOrigin);

		cb.CV_SunLightDir = Vec4(perFrameConstants.pSunDirection, 1.0f);
		cb.CV_SunColor = Vec4(perFrameConstants.pSunColor, perFrameConstants.sunSpecularMultiplier);
		cb.CV_SkyColor = Vec4(perFrameConstants.pSkyColor, 1.0f);
		cb.CV_FogColor = Vec4(globalFog.bEnable ? globalFog.color.toVec3() : Vec3(0.f, 0.f, 0.f), perFrameConstants.pVolumetricFogParams.z);
		cb.CV_TerrainInfo = Vec4(gEnv->p3DEngine->GetTerrainTextureMultiplier(), 0, 0, 0);

		cb.CV_AnimGenParams = Vec4(animTime * 2.0f, animTime * 0.25f, animTime * 1.0f, animTime * 0.125f);

		Vec3 pDecalZFightingRemedy;
		{
			const float* mProj = viewInfo.projMatrix.GetData();
			const float s = clamp_tpl(CRendererCVars::CV_r_ZFightingDepthScale, 0.1f, 1.0f);

			pDecalZFightingRemedy.x = s;                                      // scaling factor to pull decal in front
			pDecalZFightingRemedy.y = (float)((1.0f - s) * mProj[4 * 3 + 2]); // correction factor for homogeneous z after scaling is applied to xyzw { = ( 1 - v[0] ) * zMappingRageBias }
			pDecalZFightingRemedy.z = clamp_tpl(CRendererCVars::CV_r_ZFightingExtrude, 0.0f, 1.0f);

			// alternative way the might save a bit precision
			//PF.pDecalZFightingRemedy.x = s; // scaling factor to pull decal in front
			//PF.pDecalZFightingRemedy.y = (float)((1.0f - s) * mProj[4*2+2]);
			//PF.pDecalZFightingRemedy.z = clamp_tpl(CRendererCVars::CV_r_ZFightingExtrude, 0.0f, 1.0f);
		}
		cb.CV_DecalZFightingRemedy = Vec4(pDecalZFightingRemedy, 0);

		cb.CV_CamRightVector = Vec4(viewInfo.cameraVX.GetNormalized(), 0);
		cb.CV_CamFrontVector = Vec4(viewInfo.cameraVZ.GetNormalized(), 0);
		cb.CV_CamUpVector = Vec4(viewInfo.cameraVY.GetNormalized(), 0);
		cb.CV_WorldViewPosition = Vec4(viewInfo.cameraOrigin, 0);

		// CV_NearFarClipDist
		{
			// Note: CV_NearFarClipDist.z is used to put the weapon's depth range into correct relation to the whole scene
			// when generating the depth texture in the z pass (_RT_NEAREST)
			cb.CV_NearFarClipDist = Vec4(
				viewInfo.nearClipPlane,
				viewInfo.farClipPlane,
				viewInfo.farClipPlane / gEnv->p3DEngine->GetMaxViewDistance(),
				1.0f / viewInfo.farClipPlane);
		}

		// CV_ProjRatio
		{
			float zn = viewInfo.nearClipPlane;
			float zf = viewInfo.farClipPlane;
			float hfov = viewInfo.pCamera->GetHorizontalFov();
			cb.CV_ProjRatio.x = bReverseDepth ? zn / (zn - zf) : zf / (zf - zn);
			cb.CV_ProjRatio.y = bReverseDepth ? zn / (zf - zn) : zn / (zn - zf);
			cb.CV_ProjRatio.z = 1.0f / hfov;
			cb.CV_ProjRatio.w = 1.0f;
		}

		// CV_NearestScaled
		{
			float zn = viewInfo.nearClipPlane;
			float zf = viewInfo.farClipPlane;
			float nearZRange = CRendererCVars::CV_r_DrawNearZRange;
			cb.CV_NearestScaled.x = bReverseDepth ? 1.0f - zf / (zf - zn) * nearZRange : zf / (zf - zn) * nearZRange;
			cb.CV_NearestScaled.y = bReverseDepth ? zn / (zf - zn) * nearZRange * nearZRange : zn / (zn - zf) * nearZRange * nearZRange;
			cb.CV_NearestScaled.z = bReverseDepth ? 1.0f - (nearZRange - 0.001f) : nearZRange - 0.001f;
			cb.CV_NearestScaled.w = 1.0f;
		}

		// CV_TessInfo
		{
			// We want to obtain the edge length in pixels specified by CV_r_tessellationtrianglesize
			// Therefore the tess factor would depend on the viewport size and CV_r_tessellationtrianglesize
			static const ICVar* pCV_e_TessellationMaxDistance(gEnv->pConsole->GetCVar("e_TessellationMaxDistance"));
			assert(pCV_e_TessellationMaxDistance);

			const float hfov = viewInfo.pCamera->GetHorizontalFov();
			cb.CV_TessInfo.x = sqrtf(float(viewport.width * viewport.height)) / (hfov * CRendererCVars::CV_r_tessellationtrianglesize);
			cb.CV_TessInfo.y = CRendererCVars::CV_r_displacementfactor;
			cb.CV_TessInfo.z = pCV_e_TessellationMaxDistance->GetFVal();
			cb.CV_TessInfo.w = (float)CRendererCVars::CV_r_ParticlesTessellationTriSize;
		}

		cb.CV_FrustumPlaneEquation.SetRow4(0, (Vec4&)viewInfo.pFrustumPlanes[FR_PLANE_RIGHT]);
		cb.CV_FrustumPlaneEquation.SetRow4(1, (Vec4&)viewInfo.pFrustumPlanes[FR_PLANE_LEFT]);
		cb.CV_FrustumPlaneEquation.SetRow4(2, (Vec4&)viewInfo.pFrustumPlanes[FR_PLANE_TOP]);
		cb.CV_FrustumPlaneEquation.SetRow4(3, (Vec4&)viewInfo.pFrustumPlanes[FR_PLANE_BOTTOM]);

		if (gRenDev->m_pCurWindGrid)
		{
			float fSizeWH = (float)gRenDev->m_pCurWindGrid->m_nWidth * gRenDev->m_pCurWindGrid->m_fCellSize * 0.5f;
			float fSizeHH = (float)gRenDev->m_pCurWindGrid->m_nHeight * gRenDev->m_pCurWindGrid->m_fCellSize * 0.5f;
			cb.CV_WindGridOffset = Vec4(gRenDev->m_pCurWindGrid->m_vCentr.x - fSizeWH, gRenDev->m_pCurWindGrid->m_vCentr.y - fSizeHH, 1.0f / (float)gRenDev->m_pCurWindGrid->m_nWidth, 1.0f / (float)gRenDev->m_pCurWindGrid->m_nHeight);
		}
	}

	pPerViewBuffer->UpdateBuffer(&bufferData[0], sizeof(HLSL_PerViewGlobalConstantBuffer), 0, viewInfoCount);
}

//////////////////////////////////////////////////////////////////////////
void CGraphicsPipeline::ApplyShaderQuality(CDeviceGraphicsPSODesc& psoDesc, const SShaderProfile& shaderProfile)
{
	const uint64 quality = g_HWSR_MaskBit[HWSR_QUALITY];
	const uint64 quality1 = g_HWSR_MaskBit[HWSR_QUALITY1];

	psoDesc.m_ShaderFlags_RT &= ~(quality | quality1);
	switch (psoDesc.m_ShaderQuality = shaderProfile.GetShaderQuality())
	{
	case eSQ_Medium:
		psoDesc.m_ShaderFlags_RT |= quality;
		break;
	case eSQ_High:
		psoDesc.m_ShaderFlags_RT |= quality1;
		break;
	case eSQ_VeryHigh:
		psoDesc.m_ShaderFlags_RT |= (quality | quality1);
		break;
	}

}
