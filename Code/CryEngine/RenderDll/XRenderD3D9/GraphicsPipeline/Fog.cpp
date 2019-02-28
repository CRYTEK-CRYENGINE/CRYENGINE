// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "Fog.h"

#include "DriverD3D.h"
#include "D3DPostProcess.h"
#include "D3D_SVO.h"

#include "Common/ReverseDepth.h"
#include "GraphicsPipeline/VolumetricFog.h"

namespace
{
inline float expf_s(float arg)
{
	return expf(clamp_tpl(arg, -80.0f, 80.0f));
}

inline float MaxChannel(const Vec4& col)
{
	return max(max(col.x, col.y), col.z);
}
}

void CFogStage::Init()
{
	m_pTexInterleaveSamplePattern = CTexture::ForNamePtr("%ENGINE%/EngineAssets/Textures/FogVolShadowJitter.tif", FT_DONT_STREAM | FT_NOMIPS, eTF_Unknown);
}

void CFogStage::ResizeResource(int resourceWidth, int resourceHeight)
{
#if defined(VOLUMETRIC_FOG_SHADOWS)
	const uint32 flags = FT_NOMIPS | FT_DONT_STREAM | FT_USAGE_RENDERTARGET;

	for (uint32 i = 0; i < 2; ++i)
	{
		if (CRendererResources::s_ptexVolFogShadowBuf[i])
		{
			const bool shouldApplyFog = true;

			// Create/release the displacement texture on demand
			if (!shouldApplyFog && CTexture::IsTextureExist(CRendererResources::s_ptexVolFogShadowBuf[i]))
				CRendererResources::s_ptexVolFogShadowBuf[i]->ReleaseDeviceTexture(false);
			else if (shouldApplyFog && (CRendererResources::s_ptexVolFogShadowBuf[i]->Invalidate(resourceWidth, resourceHeight, eTF_R8G8B8A8) || !CTexture::IsTextureExist(CRendererResources::s_ptexVolFogShadowBuf[i])))
				CRendererResources::s_ptexVolFogShadowBuf[i]->CreateRenderTarget(eTF_R8G8B8A8, Clr_Unused);
		}
	}
#endif
}

void CFogStage::Resize(int renderWidth, int renderHeight)
{
#if defined(VOLUMETRIC_FOG_SHADOWS)
	// Recreate render targets if quality was changed
	const int32 resolutionScale = (CRenderer::CV_r_FogShadows == 1) ? 2 : 4;

	const int32 width  = renderWidth  / resolutionScale;
	const int32 height = renderHeight / resolutionScale;

	ResizeResource(width, height);
#endif
}

void CFogStage::OnCVarsChanged(const CCVarUpdateRecorder& cvarUpdater)
{
#if defined(VOLUMETRIC_FOG_SHADOWS)
	if (auto pVar = cvarUpdater.GetCVar("r_FogShadows"))
	{
		const CRenderView* pRenderView = RenderView();
		const int32 renderWidth  = pRenderView->GetRenderResolution()[0];
		const int32 renderHeight = pRenderView->GetRenderResolution()[1];

		// Recreate render targets if quality was changed
		const int32 resolutionScale = (pVar->intValue == 1 ? 2 : 4);

		const int32 width  = renderWidth  / resolutionScale;
		const int32 height = renderHeight / resolutionScale;

		ResizeResource(width, height);
	}
#endif
}

void CFogStage::Execute()
{
	bool bFogEnabled = RenderView()->IsGlobalFogEnabled();

	if (!(bFogEnabled && CRenderer::CV_r_usezpass != 0))
	{
		return;
	}

	PROFILE_LABEL_SCOPE("FOG_GLOBAL");

	int32 width  = RenderView()->GetViewport().width;
	int32 height = RenderView()->GetViewport().height;

	const bool bReverseDepth = true;
	const bool bVolumtricFog = (gRenDev->m_bVolumetricFogEnabled != 0);
#if defined(VOLUMETRIC_FOG_SHADOWS)
	const bool bVolFogShadow = (gRenDev->m_bVolFogShadowsEnabled != 0);
#else
	const bool bVolFogShadow = false;
#endif
	auto* pVolFogStage = GetStdGraphicsPipeline().GetVolumetricFogStage();

	if (bVolFogShadow)
	{
		ExecuteVolumetricFogShadow();
	}

	CShader* pShader = CShaderMan::s_shHDRPostProcess;

	// render fog
	{
#if defined(FEATURE_SVO_GI)
		// activate support for SVO atmosphere in fog shader
		CSvoRenderer* pSR = CSvoRenderer::GetInstance();
		const int nSvoGiTexId = (pSR && pSR->GetTroposphereMinRT()) ? pSR->GetTroposphereMinRT()->GetTextureID() : 0;
#else
		const int nSvoGiTexId = 0;
#endif
		const int viewInfoCount = GetStdGraphicsPipeline().GetViewInfoCount();
		const bool bSinglePassStereo = (viewInfoCount > 1) ? true : false;

		// prevent fog depth clipping if volumetric fog, SVOGI, or single pass stereo rendering are activated.
		bool bFogDepthTest = (CRenderer::CV_r_FogDepthTest != 0.0f)
		                       && !bVolumtricFog
		                       && !nSvoGiTexId
		                       && !bSinglePassStereo;

		f32 fogDepth = 0.0f;
		if (bFogDepthTest)
		{
			fogDepth = GetFogCullDistance();
			bFogDepthTest = (fogDepth >= 0.01f);
		}
		gRenDev->m_fogCullDistance = fogDepth;

		uint32 inputFlag = 0;
		inputFlag |= bVolFogShadow ? BIT(0) : 0;
		inputFlag |= bVolumtricFog ? BIT(2) : 0;
		inputFlag |= bFogDepthTest ? BIT(3) : 0;
		inputFlag |= bReverseDepth ? BIT(4) : 0;

		if (m_passFog.IsDirty(inputFlag, nSvoGiTexId, RenderView()->GetDepthTarget()->GetTextureID()))
		{
			uint64 rtMask = 0;
			rtMask |= bVolFogShadow ? g_HWSR_MaskBit[HWSR_SAMPLE0] : 0;
			rtMask |= nSvoGiTexId ? g_HWSR_MaskBit[HWSR_SAMPLE2] : 0;
			rtMask |= bVolumtricFog ? g_HWSR_MaskBit[HWSR_VOLUMETRIC_FOG] : 0;

			static CCryNameTSCRC techName("FogPass");
			m_passFog.SetPrimitiveFlags(CRenderPrimitive::eFlags_ReflectShaderConstants_PS);
			m_passFog.SetTechnique(pShader, techName, rtMask);
			m_passFog.SetRenderTarget(0, CRendererResources::s_ptexHDRTarget);

			m_passFog.SetFlags(CPrimitiveRenderPass::ePassFlags_VrProjectionPass);

			// using GS_BLDST_SRCALPHA because GS_BLDST_ONEMINUSSRCALPHA causes banding artifact when alpha value is very low.
			uint32 nRS = GS_BLSRC_ONE | GS_BLDST_SRCALPHA | (bFogDepthTest ? GS_DEPTHFUNC_LEQUAL : GS_NODEPTHTEST);
			nRS = bReverseDepth ? ReverseDepthHelper::ConvertDepthFunc(nRS) : nRS;
			m_passFog.SetState(nRS);

			m_passFog.SetTexture(0, CRendererResources::s_ptexLinearDepth);
			m_passFog.SetSampler(0, EDefaultSamplerStates::PointClamp);

			m_passFog.SetDepthTarget(RenderView()->GetDepthTarget());

			if (bVolumtricFog)
			{
				pVolFogStage->BindVolumetricFogResources(m_passFog, 1, 0);
			}
#if defined(VOLUMETRIC_FOG_SHADOWS)
			else if (bVolFogShadow)
			{
				m_passFog.SetTexture(1, CRendererResources::s_ptexVolFogShadowBuf[0]);
			}
#endif
#if defined(FEATURE_SVO_GI)
			else if (nSvoGiTexId)
			{
				// bind SVO atmosphere
				m_passFog.SetTexture(12, pSR->GetTroposphereMinRT());
				m_passFog.SetTexture(13, pSR->GetTroposphereShadRT());
				m_passFog.SetTexture(14, pSR->GetTroposphereMaxRT());
			}
#endif
		}

		f32 clipZ = 0.0f;
		if (bFogDepthTest)
		{
			const auto &viewInfo = GetStdGraphicsPipeline().GetCurrentViewInfo(CCamera::eEye_Left);
			const Matrix44A& projMat = viewInfo.projMatrix;

			// projMat.m23 is -1 or 1 depending on whether we use a RH or LH coord system
			// done in favor of an if check to make homogeneous divide by fogDepth (which is always positive) work
			clipZ = projMat.m23 * fogDepth * projMat.m22 + projMat.m32;
			clipZ /= fogDepth;
			clipZ = clamp_tpl(clipZ, 0.0f, 1.0f);

			// reverse depth operation is needed here because CFullscreenPass's internal reverse operation counteracts projMat's reverse operation.
			clipZ = bReverseDepth ? (1.0f - clipZ) : clipZ;
		}

		m_passFog.SetRequireWorldPos(true, clipZ); // don't forget if shader reconstructs world position.

		m_passFog.SetRequirePerViewConstantBuffer(true);

		m_passFog.BeginConstantUpdate();

		const f32 averageLuminance = gcpRendD3D.m_vSceneLuminanceInfo.x;
		const f32 adaptedSceneScale = gcpRendD3D.m_fAdaptedSceneScale;
		static CCryNameR szHDRParams("HDRParams2");
		m_passFog.SetConstant(szHDRParams, Vec4(averageLuminance, adaptedSceneScale, 0, 0), eHWSC_Pixel);

#if defined(VOLUMETRIC_FOG_SHADOWS)
		if (bVolFogShadow)
		{
			Vec3 volFogShadowDarkeningP;
			gEnv->p3DEngine->GetGlobalParameter(E3DPARAM_VOLFOG_SHADOW_DARKENING, volFogShadowDarkeningP);

			static CCryNameR volFogShadowDarkeningN("volFogShadowDarkening");
			m_passFog.SetConstant(volFogShadowDarkeningN, Vec4(volFogShadowDarkeningP, 0), eHWSC_Pixel);

			const float aSun = (1.0f - clamp_tpl(volFogShadowDarkeningP.y, 0.0f, 1.0f)) * 1.0f;
			const float bSun = 1.0f - aSun;
			const float aAmb = (1.0f - clamp_tpl(volFogShadowDarkeningP.z, 0.0f, 1.0f)) * 0.4f;
			const float bAmb = 1.0f - aAmb;

			static CCryNameR volFogShadowDarkeningSunAmbN("volFogShadowDarkeningSunAmb");
			m_passFog.SetConstant(volFogShadowDarkeningSunAmbN, Vec4(aSun, bSun, aAmb, bAmb), eHWSC_Pixel);

			Vec4 volFogShadowRange;
			{
				Vec3 volFogShadowRangeP;
				gEnv->p3DEngine->GetGlobalParameter(E3DPARAM_VOLFOG_SHADOW_RANGE, volFogShadowRangeP);
				volFogShadowRangeP.x = clamp_tpl(volFogShadowRangeP.x, 0.01f, 1.0f);
				volFogShadowRange = Vec4(volFogShadowRangeP.x, 1.0f / volFogShadowRangeP.x, 0, 0);
			}
			static CCryNameR volFogShadowRangeN("volFogShadowRange");
			m_passFog.SetConstant(volFogShadowRangeN, volFogShadowRange, eHWSC_Pixel);

			Vec4 sampleOffsets[5];
			{
				const float tU = 1.0f / static_cast<float>(CRendererResources::s_ptexVolFogShadowBuf[0]->GetWidth());
				const float tV = 1.0f / static_cast<float>(CRendererResources::s_ptexVolFogShadowBuf[0]->GetHeight());

				sampleOffsets[0] = Vec4(0, 0, 0, 0);
				sampleOffsets[1] = Vec4(0, -tV, 0, 0);
				sampleOffsets[2] = Vec4(-tU, 0, 0, 0);
				sampleOffsets[3] = Vec4(tU, 0, 0, 0);
				sampleOffsets[4] = Vec4(0, tU, 0, 0);
			}
			static CCryNameR volFogShadowBufSampleOffsetsN("volFogShadowBufSampleOffsets");
			m_passFog.SetConstantArray(volFogShadowBufSampleOffsetsN, sampleOffsets, 5, eHWSC_Pixel);
		}
#endif

#if defined(FEATURE_SVO_GI)
		static CCryNameR sSVO_AirTextureScale("SVO_AirTextureScale");
		Vec4 vSVO_AirTextureScale(
			nSvoGiTexId ? float(width  / pSR->GetTroposphereMinRT()->GetWidth ()) : 0.0f,
			nSvoGiTexId ? float(height / pSR->GetTroposphereMinRT()->GetHeight()) : 0.0f,
		  0.0f, 0.0f);
		m_passFog.SetConstant(sSVO_AirTextureScale, vSVO_AirTextureScale, eHWSC_Pixel);
#endif

		m_passFog.Execute();
	}

	Vec3 lCol;
	gEnv->p3DEngine->GetGlobalParameter(E3DPARAM_SKY_HIGHLIGHT_COLOR, lCol);
	bool bLightning = (lCol.x > 1e-4f || lCol.y > 1e-4f || lCol.z > 1e-4f);

	// render fog with lightning
	if (bLightning)
	{
		if (m_passLightning.IsDirty())
		{
			uint64 rtMask = 0;

			static CCryNameTSCRC techName("FogPassWithLightning");
			m_passLightning.SetTechnique(pShader, techName, rtMask);
			m_passLightning.SetRenderTarget(0, CRendererResources::s_ptexHDRTarget);

			m_passLightning.SetState(GS_NODEPTHTEST | GS_BLSRC_ONE | GS_BLDST_ONE);

			m_passLightning.SetTexture(0, CRendererResources::s_ptexLinearDepth);

			m_passLightning.SetRequireWorldPos(true);

			m_passLightning.SetRequirePerViewConstantBuffer(true);
		}

		m_passLightning.BeginConstantUpdate();

		Vec3 lPos;
		gEnv->p3DEngine->GetGlobalParameter(E3DPARAM_SKY_HIGHLIGHT_POS, lPos);
		static CCryNameR Param1Name("LightningPos");
		m_passLightning.SetConstant(Param1Name, Vec4(lPos.x, lPos.y, lPos.z, 0.0f), eHWSC_Pixel);

		Vec3 lSize;
		gEnv->p3DEngine->GetGlobalParameter(E3DPARAM_SKY_HIGHLIGHT_SIZE, lSize);
		static CCryNameR Param2Name("LightningColSize");
		m_passLightning.SetConstant(Param2Name, Vec4(lCol.x, lCol.y, lCol.z, lSize.x * 0.01f), eHWSC_Pixel);

		m_passLightning.Execute();
	}
}

void CFogStage::ExecuteVolumetricFogShadow()
{
#if defined(VOLUMETRIC_FOG_SHADOWS)
	CD3D9Renderer* const __restrict rd = gcpRendD3D;
	const auto *pRenderView = RenderView();

	const bool bCloudShadow = rd->m_bVolFogCloudShadowsEnabled;
	const bool bVolCloudShadow = rd->m_bVolumetricCloudsEnabled;

	CShader* pShader = CShaderMan::s_shHDRPostProcess;

	// ray cast into sun shadow maps
	{
		CShadowUtils::SShadowCascades cascades;
		bool bSunShadow = CShadowUtils::SetupShadowsForFog(cascades, pRenderView);
		
		m_pCloudShadowTex = cascades.pCloudShadowMap;

		uint32 inputFlag = 0;
		inputFlag |= bCloudShadow ? BIT(0) : 0;
		inputFlag |= bVolCloudShadow ? BIT(1) : 0;
		inputFlag |= bSunShadow ? BIT(2) : 0;

		if (m_passVolFogShadowRaycast.IsDirty(inputFlag, CRenderer::CV_r_FogShadowsMode))
		{
			uint64 rtMask = 0;
			rtMask |= bCloudShadow ? g_HWSR_MaskBit[HWSR_SAMPLE5] : 0;
			rtMask |= bVolCloudShadow ? g_HWSR_MaskBit[HWSR_SAMPLE4] : 0;

			static CCryNameTSCRC TechName0("FogPassVolShadowsInterleavePass");
			static CCryNameTSCRC TechName1("MultiGSMShadowedFog");
			auto& techName = (CRenderer::CV_r_FogShadowsMode == 1) ? TechName1 : TechName0;

			m_passVolFogShadowRaycast.SetPrimitiveFlags(CRenderPrimitive::eFlags_ReflectShaderConstants_PS);
			m_passVolFogShadowRaycast.SetTechnique(pShader, techName, rtMask);
			m_passVolFogShadowRaycast.SetRenderTarget(0, CRendererResources::s_ptexVolFogShadowBuf[0]);
			m_passVolFogShadowRaycast.SetState(GS_NODEPTHTEST);
			m_passVolFogShadowRaycast.SetRequireWorldPos(true);
			m_passVolFogShadowRaycast.SetRequirePerViewConstantBuffer(true);

			m_passVolFogShadowRaycast.SetTexture(0, CRendererResources::s_ptexLinearDepth);
			m_passVolFogShadowRaycast.SetTexture(1, m_pTexInterleaveSamplePattern);

			CShadowUtils::SetShadowSamplingContextToRenderPass(m_passVolFogShadowRaycast, 0, 1, 2, 3, 2);

			if (bVolCloudShadow)
			{
				m_passVolFogShadowRaycast.SetTexture(8, CRendererResources::s_ptexVolCloudShadow);
				m_passVolFogShadowRaycast.SetSampler(4, EDefaultSamplerStates::BilinearBorder_Black);
			}
		}

		CShadowUtils::SetShadowCascadesToRenderPass(m_passVolFogShadowRaycast, 4, 3, cascades);

		m_passVolFogShadowRaycast.BeginConstantUpdate();

		Vec4 volFogShadowRange;
		{
			Vec3 volFogShadowRangeP;
			gEnv->p3DEngine->GetGlobalParameter(E3DPARAM_VOLFOG_SHADOW_RANGE, volFogShadowRangeP);
			volFogShadowRangeP.x = clamp_tpl(volFogShadowRangeP.x, 0.01f, 1.0f);
			volFogShadowRange = Vec4(volFogShadowRangeP.x, 1.0f / volFogShadowRangeP.x, 0, 0);
		}

		static CCryNameR volFogShadowRangeN("volFogShadowRange");
		m_passVolFogShadowRaycast.SetConstant(volFogShadowRangeN, volFogShadowRange, eHWSC_Pixel);

		CShadowUtils::SShadowCascadesSamplingInfo shadowSamplingInfo;
		CShadowUtils::GetShadowCascadesSamplingInfo(shadowSamplingInfo, pRenderView);
		m_passVolFogShadowRaycast.SetConstant(CCryNameR("TexGen0"), shadowSamplingInfo.shadowTexGen[0]);
		m_passVolFogShadowRaycast.SetConstant(CCryNameR("TexGen1"), shadowSamplingInfo.shadowTexGen[1]);
		m_passVolFogShadowRaycast.SetConstant(CCryNameR("TexGen2"), shadowSamplingInfo.shadowTexGen[2]);
		m_passVolFogShadowRaycast.SetConstant(CCryNameR("TexGen3"), shadowSamplingInfo.shadowTexGen[3]);
		m_passVolFogShadowRaycast.SetConstant(CCryNameR("vInvShadowMapSize"), shadowSamplingInfo.invShadowMapSize);
		m_passVolFogShadowRaycast.SetConstant(CCryNameR("fDepthTestBias"), shadowSamplingInfo.depthTestBias);
		m_passVolFogShadowRaycast.SetConstant(CCryNameR("fOneDivFarDist"), shadowSamplingInfo.oneDivFarDist);
		m_passVolFogShadowRaycast.SetConstant(CCryNameR("fKernelRadius"), shadowSamplingInfo.kernelRadius);
		m_passVolFogShadowRaycast.SetConstant(CCryNameR("CloudShadowParams"), shadowSamplingInfo.cloudShadowParams);
		m_passVolFogShadowRaycast.SetConstant(CCryNameR("CloudShadowAnimParams"), shadowSamplingInfo.cloudShadowAnimParams);
		m_passVolFogShadowRaycast.SetConstantArray(CCryNameR("irreg_kernel_2d"), shadowSamplingInfo.irregKernel2d, 8);

		m_passVolFogShadowRaycast.Execute();
	}

	// horizontal blur
	{
		if (m_passVolFogShadowHBlur.IsDirty())
		{
			static CCryNameTSCRC TechName("FogPassVolShadowsGatherPass");
			m_passVolFogShadowHBlur.SetPrimitiveFlags(CRenderPrimitive::eFlags_ReflectShaderConstants_PS);
			m_passVolFogShadowHBlur.SetTechnique(pShader, TechName, 0);
			m_passVolFogShadowHBlur.SetRenderTarget(0, CRendererResources::s_ptexVolFogShadowBuf[1]);
			m_passVolFogShadowHBlur.SetState(GS_NODEPTHTEST);
			m_passVolFogShadowHBlur.SetRequireWorldPos(true);
			m_passVolFogShadowHBlur.SetRequirePerViewConstantBuffer(true);

			m_passVolFogShadowHBlur.SetTextureSamplerPair(0, CRendererResources::s_ptexVolFogShadowBuf[0], EDefaultSamplerStates::PointClamp);
		}

		m_passVolFogShadowHBlur.BeginConstantUpdate();

		Vec4 sampleOffsets[8];
		const f32 tU = 1.0f / static_cast<f32>(CRendererResources::s_ptexVolFogShadowBuf[0]->GetWidth());
		for (int32 x = -4, index = 0; x < 4; ++x, ++index)
		{
			sampleOffsets[index] = Vec4(x * tU, 0.0f, 0.0f, 1.0f);
		}
		static CCryNameR volFogShadowBufSampleOffsetsN("volFogShadowBufSampleOffsets");
		m_passVolFogShadowHBlur.SetConstantArray(volFogShadowBufSampleOffsetsN, sampleOffsets, 8, eHWSC_Pixel);

		m_passVolFogShadowHBlur.Execute();
	}

	// vertical blur
	{
		if (m_passVolFogShadowVBlur.IsDirty())
		{
			static CCryNameTSCRC TechName("FogPassVolShadowsGatherPass");
			m_passVolFogShadowVBlur.SetPrimitiveFlags(CRenderPrimitive::eFlags_ReflectShaderConstants_PS);
			m_passVolFogShadowVBlur.SetTechnique(pShader, TechName, 0);
			m_passVolFogShadowVBlur.SetRenderTarget(0, CRendererResources::s_ptexVolFogShadowBuf[0]);
			m_passVolFogShadowVBlur.SetState(GS_NODEPTHTEST);
			m_passVolFogShadowVBlur.SetRequireWorldPos(true);
			m_passVolFogShadowVBlur.SetRequirePerViewConstantBuffer(true);

			m_passVolFogShadowVBlur.SetTextureSamplerPair(0, CRendererResources::s_ptexVolFogShadowBuf[1], EDefaultSamplerStates::PointClamp);
		}

		m_passVolFogShadowVBlur.BeginConstantUpdate();

		Vec4 sampleOffsets[8];
		const f32 tV = 1.0f / static_cast<f32>(CRendererResources::s_ptexVolFogShadowBuf[1]->GetWidth());
		for (int32 y = -4, index = 0; y < 4; ++y, ++index)
		{
			sampleOffsets[index] = Vec4(0.0f, y * tV, 0.0f, 1.0f);
		}
		static CCryNameR volFogShadowBufSampleOffsetsN("volFogShadowBufSampleOffsets");
		m_passVolFogShadowVBlur.SetConstantArray(volFogShadowBufSampleOffsetsN, sampleOffsets, 8, eHWSC_Pixel);

		m_passVolFogShadowVBlur.Execute();
	}
#endif // #if defined(VOLUMETRIC_FOG_SHADOWS)
}

f32 CFogStage::GetFogCullDistance() const
{
	CD3D9Renderer* const __restrict rd = gcpRendD3D;
	CRenderView* pRenderView = RenderView();
	SRenderViewShaderConstants& PF = pRenderView->GetShaderConstants();

	const auto& viewInfo = GetCurrentViewInfo();

	float fogDepth = 0.0f;

	if (CRenderer::CV_r_FogDepthTest != 0.0f)
	{
		if (CRenderer::CV_r_FogDepthTest < 0.0f)
		{
			Vec4 fogColGradColBase(0, 0, 0, 0);
			Vec4 fogColGradColDelta(0, 0, 0, 0);
			CHWShader_D3D::GetFogColorGradientConstants(fogColGradColBase, fogColGradColDelta);
			const Vec4 fogColGradRadial = CHWShader_D3D::GetFogColorGradientRadial(*viewInfo.pCamera);
			const Vec4 volFogParams = CHWShader_D3D::GetVolumetricFogParams(*viewInfo.pCamera);
			const Vec4 volFogRampParams = CHWShader_D3D::GetVolumetricFogRampParams();
			const Vec4 volFogSunDir = CHWShader_D3D::GetVolumetricFogSunDir(PF.pSunDirection);

			const float fogColorIntensityBase = MaxChannel(fogColGradColBase);
			const float fogColorIntensityTop = MaxChannel(fogColGradColBase + fogColGradColDelta);
			const float fogColorIntensityRadial = MaxChannel(fogColGradRadial);
			const float fogColorIntensity = max(fogColorIntensityBase, fogColorIntensityTop) + fogColorIntensityRadial;

			const float threshold = -CRenderer::CV_r_FogDepthTest;

			const float atmosphereScale = volFogParams.x;
			const float volFogHeightDensityAtViewer = volFogParams.y;
			const float heightDiffFromBase = volFogParams.z;
			const float expHeightDiffFromBase = volFogParams.w;
			const float finalClamp = 1.0f - volFogSunDir.w;

			SD3DPostEffectsUtils::UpdateFrustumCorners();
			Vec3 camDir = -viewInfo.cameraVZ;
			f32 camZFar = viewInfo.farClipPlane;

			Vec3 lookDir = SD3DPostEffectsUtils::m_vRT;
			if (lookDir.z * atmosphereScale < SD3DPostEffectsUtils::m_vLT.z * atmosphereScale)
				lookDir = SD3DPostEffectsUtils::m_vLT;
			if (lookDir.z * atmosphereScale < SD3DPostEffectsUtils::m_vLB.z * atmosphereScale)
				lookDir = SD3DPostEffectsUtils::m_vLB;
			if (lookDir.z * atmosphereScale < SD3DPostEffectsUtils::m_vRB.z * atmosphereScale)
				lookDir = SD3DPostEffectsUtils::m_vRB;

			lookDir.Normalize();
			const float viewDirAdj = lookDir.Dot(camDir);

			float depth = camZFar * 0.5f;
			float step = depth * 0.5f;
			uint32 numSteps = 16;

			while (numSteps)
			{
				Vec3 cameraToWorldPos = lookDir * depth;

				float fogInt = 1.0f;

				const float t = atmosphereScale * cameraToWorldPos.z;
				const float slopeThreshold = 0.01f;
				if (fabsf(t) > slopeThreshold)
				{
					fogInt *= ((expf_s(t + heightDiffFromBase) - expHeightDiffFromBase) / t);
				}
				else
				{
					fogInt *= expHeightDiffFromBase;
				}

				const float l = depth; // length(cameraToWorldPos);
				const float u = l * volFogHeightDensityAtViewer;
				fogInt = fogInt * u;

				float f = clamp_tpl(expf_s(0.69314719f * -fogInt), 0.0f, 1.0f);

				float r = clamp_tpl(l * volFogRampParams.x + volFogRampParams.y, 0.0f, 1.0f);
				r = r * (2.0f - r);
				r = r * volFogRampParams.z + volFogRampParams.w;

				f = (1.0f - f) * r;
				assert(f >= 0.0f && f <= 1.0f);

				f = min(f, finalClamp);
				f *= fogColorIntensity;

				if (f > threshold)
				{
					depth -= step;
				}
				else
				{
					fogDepth = depth * viewDirAdj;
					depth += step;
				}
				step *= 0.5f;

				--numSteps;
			}
		}
		else
		{
			fogDepth = CRenderer::CV_r_FogDepthTest;
		}
	}

	return fogDepth;
}


void CFogStage::FillForwardParams(SForwardParams& forwardParams, bool enable) const
{
	SRenderViewShaderConstants& paramsPF = RenderView()->GetShaderConstants();
	
	if (enable)
	{
		forwardParams.vfParams = paramsPF.pVolumetricFogParams;
		forwardParams.vfRampParams = paramsPF.pVolumetricFogRampParams;
		forwardParams.vfSunDir = paramsPF.pVolumetricFogSunDir;
		forwardParams.vfColGradBase = Vec3(paramsPF.pFogColGradColBase);
		forwardParams.padding0 = 0.0f;
		forwardParams.vfColGradDelta = Vec3(paramsPF.pFogColGradColDelta);
		forwardParams.padding1 = 0.0f;
		forwardParams.vfColGradParams = paramsPF.pFogColGradParams;
		forwardParams.vfColGradRadial = paramsPF.pFogColGradRadial;
	}
	else
	{
		// turning off by parameters.
		forwardParams.vfParams = paramsPF.pVolumetricFogParams;
		forwardParams.vfRampParams = paramsPF.pVolumetricFogRampParams;
		const Vec3& sunDir = paramsPF.pVolumetricFogSunDir;
		forwardParams.vfSunDir = Vec4(sunDir, 1.0f);
		forwardParams.vfColGradBase = Vec3(0.0f);
		forwardParams.padding0 = 0.0f;
		forwardParams.vfColGradDelta = Vec3(0.0f);
		forwardParams.padding1 = 0.0f;
		forwardParams.vfColGradParams = paramsPF.pFogColGradParams;
		forwardParams.vfColGradRadial = paramsPF.pFogColGradRadial;
	}

	// Volumetric fog shadows
#if defined(VOLUMETRIC_FOG_SHADOWS)
	if (gcpRendD3D->m_bVolFogShadowsEnabled && enable)
	{
		Vec3 volFogShadowDarkeningP;
		gEnv->p3DEngine->GetGlobalParameter(E3DPARAM_VOLFOG_SHADOW_DARKENING, volFogShadowDarkeningP);
		forwardParams.vfShadowDarkening = Vec4(volFogShadowDarkeningP, 0.0f);

		const float aSun = (1.0f - clamp_tpl(volFogShadowDarkeningP.y, 0.0f, 1.0f)) * 1.0f;
		const float bSun = 1.0f - aSun;
		const float aAmb = (1.0f - clamp_tpl(volFogShadowDarkeningP.z, 0.0f, 1.0f)) * 0.4f;
		const float bAmb = 1.0f - aAmb;
		forwardParams.vfShadowDarkeningSunAmb = Vec4(aSun, bSun, aAmb, bAmb);
	}
	else
#endif
	{
		forwardParams.vfShadowDarkening = Vec4(0.0f);
		forwardParams.vfShadowDarkeningSunAmb = Vec4(0.0f);
	}
}
