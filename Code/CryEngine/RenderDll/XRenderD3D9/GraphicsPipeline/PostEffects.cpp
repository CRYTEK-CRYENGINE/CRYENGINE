// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "PostEffects.h"

#include "DriverD3D.h"
#include "D3DPostProcess.h"
#include "Common/ReverseDepth.h"
#include "SceneCustom.h"

#include <Common/RenderDisplayContext.h>

//////////////////////////////////////////////////////////////////////////

CPostEffectContext::CPostEffectContext()
{

}

CPostEffectContext::~CPostEffectContext()
{

}

void CPostEffectContext::Setup(CPostEffectsMgr* pPostEffectsMgr)
{
	m_pPostEffectsMgr = pPostEffectsMgr;
	CRY_ASSERT(m_pPostEffectsMgr);

	CD3D9Renderer* const RESTRICT_POINTER rd = gcpRendD3D;
	const auto shaderType = eST_PostProcess;
	SShaderProfile* const pSP = &(rd->m_cEF.m_ShaderProfiles[shaderType]);
	const uint64 quality = g_HWSR_MaskBit[HWSR_QUALITY];
	const uint64 quality1 = g_HWSR_MaskBit[HWSR_QUALITY1];
	int nQuality = (int)pSP->GetShaderQuality();

	m_shaderRTMask = 0;

	switch (nQuality)
	{
	case eSQ_Medium:
		m_shaderRTMask |= quality;
		break;
	case eSQ_High:
		m_shaderRTMask |= quality1;
		break;
	case eSQ_VeryHigh:
		m_shaderRTMask |= (quality | quality1);
		break;
	}

	m_bUseAltBackBuffer = false;
}

void CPostEffectContext::EnableAltBackBuffer(bool enable)
{
	m_bUseAltBackBuffer = enable;
}

uint64 CPostEffectContext::GetShaderRTMask() const
{
	return m_shaderRTMask;
}

CTexture* CPostEffectContext::GetSrcBackBufferTexture() const
{
	return CRendererResources::s_ptexBackBuffer;
}

CTexture* CPostEffectContext::GetDstBackBufferTexture() const
{
	if (m_bUseAltBackBuffer)
		return CRendererResources::s_ptexSceneDiffuse;
	return GetRenderView()->GetColorTarget();
}

CTexture* CPostEffectContext::GetDstDepthStencilTexture() const
{
	return GetRenderView()->GetDepthTarget();
}

CPostEffect* CPostEffectContext::GetPostEffect(EPostEffectID nID) const
{
	CRY_ASSERT(m_pPostEffectsMgr);
	return m_pPostEffectsMgr->GetEffect(nID);
}

const CEffectParam* CPostEffectContext::GetEffectParamByName(const char* pszParam) const
{
	CRY_ASSERT(m_pPostEffectsMgr);
	return m_pPostEffectsMgr->GetByName(pszParam);
}

//////////////////////////////////////////////////////////////////////////

CPostEffectStage::CPostEffectStage()
{

}

CPostEffectStage::~CPostEffectStage()
{

}

void CPostEffectStage::Init()
{
	// TODO: all commented ones
//	m_postEffectArray[EPostEffectID::SunShafts           ] = stl::make_unique<CSunShaftsPass           >();
//	m_postEffectArray[EPostEffectID::MotionBlur          ] = stl::make_unique<CMotionBlurPass          >();
//	m_postEffectArray[EPostEffectID::ColorGrading        ] = stl::make_unique<CColorGradingPass        >();
//	m_postEffectArray[EPostEffectID::DepthOfField        ] = stl::make_unique<CDepthOfFieldPass        >();
	m_postEffectArray[EPostEffectID::HUDSilhouettes      ] = stl::make_unique<CHudSilhouettesPass      >();
//	m_postEffectArray[EPostEffectID::PostAA              ] = stl::make_unique<CPostAAPass              >();
	m_postEffectArray[EPostEffectID::UnderwaterGodRays   ] = stl::make_unique<CUnderwaterGodRaysPass   >();
//	m_postEffectArray[EPostEffectID::VolumetricScattering] = stl::make_unique<CVolumetricScatteringPass>();
	m_postEffectArray[EPostEffectID::Sharpening          ] = stl::make_unique<CSharpeningPass          >();
	m_postEffectArray[EPostEffectID::Blurring            ] = stl::make_unique<CBlurringPass            >();
	m_postEffectArray[EPostEffectID::UberGamePostProcess ] = stl::make_unique<CUberGamePostEffectPass  >();
//	m_postEffectArray[EPostEffectID::NightVision         ] = stl::make_unique<CNightVisionPass         >();
//	m_postEffectArray[EPostEffectID::SonarVision         ] = stl::make_unique<CSonarVisionPass         >();
//	m_postEffectArray[EPostEffectID::ThermalVision       ] = stl::make_unique<CThermalVisionPass       >();
	m_postEffectArray[EPostEffectID::FlashBang           ] = stl::make_unique<CFlashBangPass           >();
//	m_postEffectArray[EPostEffectID::ImageGhosting       ] = stl::make_unique<CImageGhostingPass       >();
//	m_postEffectArray[EPostEffectID::NanoGlass           ] = stl::make_unique<CNanoGlassPass           >();
//	m_postEffectArray[EPostEffectID::RainDrops           ] = stl::make_unique<CRainDropsPass           >();
	m_postEffectArray[EPostEffectID::WaterDroplets       ] = stl::make_unique<CWaterDropletsPass       >();
	m_postEffectArray[EPostEffectID::WaterFlow           ] = stl::make_unique<CWaterFlowPass           >();
	m_postEffectArray[EPostEffectID::ScreenBlood         ] = stl::make_unique<CScreenBloodPass         >();
//	m_postEffectArray[EPostEffectID::ScreenFrost         ] = stl::make_unique<CScreenFrostPass         >();
	m_postEffectArray[EPostEffectID::KillCamera          ] = stl::make_unique<CKillCameraPass          >();
//	m_postEffectArray[EPostEffectID::AlienInterference   ] = stl::make_unique<CAlienInterferencePass   >();
	m_postEffectArray[EPostEffectID::PostStereo          ] = stl::make_unique<CPostStereoPass          >();
	m_postEffectArray[EPostEffectID::HUD3D               ] = stl::make_unique<CHud3DPass               >();
	m_postEffectArray[EPostEffectID::ScreenFader         ] = stl::make_unique<CScreenFaderPass         >();
//	m_postEffectArray[EPostEffectID::Post3DRenderer      ] = stl::make_unique<CPost3DRendererPass      >();

	for (auto& pPostEffect : m_postEffectArray)
	{
		if (pPostEffect)
		{
			pPostEffect->Init();
		}
	}
}

void CPostEffectStage::Update()
{
	CRenderView* pRenderView = RenderView();
	m_context.SetRenderView(pRenderView);
}

bool CPostEffectStage::Execute()
{
	// TODO: each viewport would need to have its own post effect manager when we will support multi-viewport.
	CPostEffectsMgr* pPostMgr = PostEffectMgr();

	if (!pPostMgr)
	{
		return false;
	}

	if (CRenderer::CV_r_PostProcessReset)
	{
		CRenderer::CV_r_PostProcessReset = 0;

		pPostMgr->Reset();
	}

	CD3D9Renderer* const RESTRICT_POINTER rd = gcpRendD3D;

	IF (!rd
	    || !CRenderer::CV_r_PostProcess
	    || pPostMgr->GetEffects().empty(), 0)
	{
		return false;
	}

	// Skip hdr/post processing when rendering different camera views
	if ((RenderView()->IsViewFlag(SRenderViewInfo::eFlags_MirrorCull))
	    || (RenderView()->GetShaderRenderingFlags() & SHDF_CUBEMAPGEN)
	    || (RenderView()->GetShaderRenderingFlags() & SHDF_ALLOWPOSTPROCESS) == 0)
	{
		return false;
	}

	if (rd->m_bDeviceLost)
	{
		return false;
	}

	IF (!CShaderMan::s_shPostEffects, 0)
	{
		return false;
	}

	IF (!CTexture::IsTextureExist(CRendererResources::s_ptexBackBuffer), 0)
	{
		return false;
	}

	IF (!CTexture::IsTextureExist(CRendererResources::s_ptexSceneTarget), 0)
	{
		return false;
	}

	PROFILE_LABEL_SCOPE("POST_EFFECTS_LDR");

	// TODO: port to new graphics pipeline after all post effects are ported to new graphics pipeline.
	pPostMgr->Begin();

	m_context.Setup(pPostMgr);
	m_context.SetRenderView(RenderView());

	m_context.EnableAltBackBuffer(true);

	for (CPostEffectItor pItor = pPostMgr->GetEffects().begin(), pItorEnd = pPostMgr->GetEffects().end(); pItor != pItorEnd; ++pItor)
	{
		CPostEffect* pCurrEffect = (*pItor);
		if (pCurrEffect->GetRenderFlags() & PSP_REQUIRES_UPDATE)
		{
			pCurrEffect->Update();
		}
	}

	Execute3DHudFlashUpdate();

	const auto& viewInfo = RenderView()->GetViewInfo(CCamera::eEye_Left);

#ifndef _RELEASE
	CPostEffectDebugVec& activeEffects = pPostMgr->GetActiveEffectsDebug();
	CPostEffectDebugVec& activeParams = pPostMgr->GetActiveEffectsParamsDebug();
#endif

	for (CPostEffectItor pItor = pPostMgr->GetEffects().begin(), pItorEnd = pPostMgr->GetEffects().end(); pItor != pItorEnd; ++pItor)
	{
		CPostEffect* pCurrEffect = (*pItor);
		pCurrEffect->SetCurrentContext(&m_context);
		if (pCurrEffect->Preprocess(viewInfo))
		{
			pCurrEffect->SetCurrentContext(nullptr);
			const auto id = pCurrEffect->GetID();

			if (id >= EPostEffectID::PostAA)
				m_context.EnableAltBackBuffer(false);

			uint32 nRenderFlags = pCurrEffect->GetRenderFlags();
			if (nRenderFlags & PSP_UPDATE_BACKBUFFER)
			{
				CTexture* pDstTex = m_context.GetSrcBackBufferTexture();
				CTexture* pSrcTex = m_context.GetDstBackBufferTexture();

				m_passCopyScreenToTex.Execute(pSrcTex, pDstTex);
			}

#ifndef _RELEASE
			SPostEffectsDebugInfo* pDebugInfo = nullptr;
			for (uint32 i = 0, nNumEffects = activeEffects.size(); i < nNumEffects; ++i)
			{
				if ((pDebugInfo = &activeEffects[i]) && pDebugInfo->pEffect == pCurrEffect)
				{
					pDebugInfo->fTimeOut = POSTSEFFECTS_DEBUGINFO_TIMEOUT;
					break;
				}
				pDebugInfo = nullptr;
			}
			if (pDebugInfo == nullptr)
			{
				activeEffects.push_back(SPostEffectsDebugInfo(pCurrEffect));
			}
#endif

			auto& pPostEffect = m_postEffectArray[id];
			if (pPostEffect)
			{
				pPostEffect->Execute(m_context);
			}
			else
			{
				pCurrEffect->Render();
			}
		}

		pCurrEffect->SetCurrentContext(nullptr);
	}

	// display debug info.
#ifndef _RELEASE
	if (CRenderer::CV_r_AntialiasingModeDebug > 0)
	{
		CTexture* pSrcTex = m_context.GetSrcBackBufferTexture();
		CTexture* pDstTex = m_context.GetDstBackBufferTexture();

		m_passCopyScreenToTex.Execute(pDstTex, pSrcTex);

		auto& pass = m_passAntialiasingDebug;

		if (pass.IsDirty(pSrcTex->GetID(), pDstTex->GetID()))
		{
			static CCryNameTSCRC pszTechName("DebugPostAA");
			pass.SetPrimitiveFlags(CRenderPrimitive::eFlags_ReflectShaderConstants_PS);
			pass.SetTechnique(CShaderMan::s_shPostAA, pszTechName, 0);
			pass.SetState(GS_NODEPTHTEST);

			pass.SetRenderTarget(0, pDstTex);

			pass.SetTexture(0, pSrcTex);
			pass.SetSampler(0, EDefaultSamplerStates::PointClamp);
		}

		pass.BeginConstantUpdate();

		float mx = static_cast<float>(pSrcTex->GetWidth() >> 1);
		float my = static_cast<float>(pSrcTex->GetHeight() >> 1);
	#if CRY_PLATFORM_WINDOWS
		gEnv->pHardwareMouse->GetHardwareMouseClientPosition(&mx, &my);
	#endif

		const Vec4 vDebugParams(mx, my, 1.f, max(1.0f, (float)CRenderer::CV_r_AntialiasingModeDebug));
		static CCryNameR pszDebugParams("vDebugParams");
		pass.SetConstant(pszDebugParams, vDebugParams);

		pass.Execute();
	}

	if (!activeEffects.empty() && CRenderer::CV_r_PostProcess >= 2) // Debug output for active post effects
	{
		if (CRenderer::CV_r_PostProcess >= 2)
		{
			float nPosY = 20.0f;
			IRenderAuxText::Draw2dText(30.0f, nPosY += 15.0f, Vec3(0, 1, 0), "Active post effects:");

			for (uint32 i = 0, nNumEffects = activeEffects.size(); i < nNumEffects; ++i)
			{
				SPostEffectsDebugInfo& debugInfo = activeEffects[i];
				if (debugInfo.fTimeOut > 0.0f)
				{
					IRenderAuxText::Draw2dText(30.0f, nPosY += 10.0f, Vec3(1), debugInfo.pEffect->GetName());
				}
				debugInfo.fTimeOut -= gEnv->pTimer->GetFrameTime();
			}
		}

		if (CRenderer::CV_r_PostProcess == 3)
		{
			StringEffectMap* pEffectsParamsUpdated = pPostMgr->GetDebugParamsUsedInFrame();
			if (pEffectsParamsUpdated)
			{
				if (!pEffectsParamsUpdated->empty())
				{
					StringEffectMapItor pEnd = pEffectsParamsUpdated->end();
					for (StringEffectMapItor pItor = pEffectsParamsUpdated->begin(); pItor != pEnd; ++pItor)
					{
						SPostEffectsDebugInfo* pDebugInfo = NULL;
						for (uint32 p = 0, nNumParams = activeParams.size(); p < nNumParams; ++p)
						{
							if ((pDebugInfo = &activeParams[p]) && pDebugInfo->szParamName == (pItor->first))
							{
								pDebugInfo->fTimeOut = POSTSEFFECTS_DEBUGINFO_TIMEOUT;
								break;
							}
							pDebugInfo = NULL;
						}
						if (pDebugInfo == NULL)
							activeParams.push_back(SPostEffectsDebugInfo((pItor->first), (pItor->second != 0) ? pItor->second->GetParam() : 0.0f));
					}
					pEffectsParamsUpdated->clear();
				}

				float nPosX = 250.0f, nPosY = 5.0f;

				IRenderAuxText::Draw2dText(nPosX, nPosY += 15.0f, Vec3(0, 1, 0), "Frame parameters:");

				for (uint32 p = 0, nNumParams = activeParams.size(); p < nNumParams; ++p)
				{
					SPostEffectsDebugInfo& debugInfo = activeParams[p];
					if (debugInfo.fTimeOut > 0.0f)
					{
						char pNameAndValue[128];
						cry_sprintf(pNameAndValue, "%s: %.4f\n", debugInfo.szParamName.c_str(), debugInfo.fParamVal);
						IRenderAuxText::Draw2dText(nPosX, nPosY += 10.0f, Vec3(1), pNameAndValue);
					}
					debugInfo.fTimeOut -= gEnv->pTimer->GetFrameTime();
				}
			}
		}
	}
#endif

	// TODO: port to new graphics pipeline after all post effects are ported to new graphics pipeline.
	pPostMgr->End(RenderView());

	return true;
}

void CPostEffectStage::Execute3DHudFlashUpdate()
{
	const auto& viewInfo = RenderView()->GetViewInfo(CCamera::eEye_Left);
	const auto& context = m_context;
	auto* p3DHUD = context.GetPostEffect(EPostEffectID::HUD3D);

	// If HUD enabled, pre-process flash updates first.
	// post effects of EPostEffectID::NanoGlass and EPostEffectID::PostStereo depend on this HUD update.
	if (p3DHUD && p3DHUD->Preprocess(viewInfo))
	{
		auto& pPostEffect = m_postEffectArray[EPostEffectID::HUD3D];
		if (pPostEffect)
		{
			auto* pHud3dPass = static_cast<CHud3DPass*>(pPostEffect.get());
			auto& hud3d = *static_cast<CHud3D*>(p3DHUD);

			pHud3dPass->ExecuteFlashUpdate(context, hud3d);
		}
	}
}

//////////////////////////////////////////////////////////////////////////
CUnderwaterGodRaysPass::CUnderwaterGodRaysPass()
{

}

CUnderwaterGodRaysPass::~CUnderwaterGodRaysPass()
{
	SAFE_RELEASE(m_pWavesTex);
	SAFE_RELEASE(m_pCausticsTex);
	SAFE_RELEASE(m_pUnderwaterBumpTex);
}

void CUnderwaterGodRaysPass::Init()
{
	CRY_ASSERT(m_pWavesTex == nullptr);
	m_pWavesTex = CTexture::ForName("%ENGINE%/EngineAssets/Textures/oceanwaves_ddn.tif", FT_DONT_STREAM, eTF_Unknown);

	CRY_ASSERT(m_pCausticsTex == nullptr);
	m_pCausticsTex = CTexture::ForName("%ENGINE%/EngineAssets/Textures/caustics_sampler.dds", FT_DONT_STREAM, eTF_Unknown);

	CRY_ASSERT(m_pUnderwaterBumpTex == nullptr);
	m_pUnderwaterBumpTex = CTexture::ForName("%ENGINE%/EngineAssets/Textures/screen_noisy_bump.dds", FT_DONT_STREAM, eTF_Unknown);
}

void CUnderwaterGodRaysPass::Execute(const CPostEffectContext& context)
{
	const CEffectParam* pAmount = context.GetEffectParamByName("UnderwaterGodRays_Amount");
	CRY_ASSERT(pAmount != nullptr);

	if (!pAmount)
	{
		return;
	}

	PROFILE_LABEL_SCOPE("UNDERWATER_GODRAYS");

	CD3D9Renderer* const RESTRICT_POINTER rd = gcpRendD3D;

	static CCryNameR param0Name("PB_GodRaysParamsVS");
	static CCryNameR param1Name("PB_GodRaysParamsPS");

	// render god-rays into low-res render target for less fillrate hit.
	{
		CClearSurfacePass::Execute(CRendererResources::s_ptexBackBufferScaled[1], Clr_Transparent);

		const float fAmount = pAmount->GetParam();
		const float fWatLevel = SPostEffectsUtils::m_fWaterLevel;

		uint64 rtMask = context.GetShaderRTMask();

		// TODO: move to context's shader RT mask.
		{
			rtMask |= g_HWSR_MaskBit[HWSR_REVERSE_DEPTH];
		}

		CTexture* pSrcBackBufferTexture = context.GetSrcBackBufferTexture();

		for (int32 r = 0; r < SliceCount; ++r)
		{
			auto& pass = m_passUnderwaterGodRaysGen[r];

			if (pass.IsDirty(rtMask, pSrcBackBufferTexture->GetID()))
			{
				static CCryNameTSCRC techName("UnderwaterGodRays");
				pass.SetTechnique(CShaderMan::s_shPostEffects, techName, rtMask);
				pass.SetState(GS_BLSRC_ONE | GS_BLDST_ONE | GS_NODEPTHTEST);

				pass.SetRenderTarget(0, CRendererResources::s_ptexBackBufferScaled[1]);

				pass.SetTexture(0, pSrcBackBufferTexture);
				pass.SetTexture(1, m_pWavesTex);
				pass.SetTexture(2, m_pCausticsTex);

				pass.SetSampler(0, EDefaultSamplerStates::LinearClamp);
				pass.SetSampler(1, EDefaultSamplerStates::TrilinearWrap);

				pass.SetRequirePerViewConstantBuffer(true);
			}

			pass.BeginConstantUpdate();

			const Vec4 pParams = Vec4(fWatLevel, fAmount, (float)r, 1.0f / (float)SliceCount);
			pass.SetConstant(param0Name, pParams, eHWSC_Vertex);
			pass.SetConstant(param1Name, pParams, eHWSC_Pixel);

			pass.Execute();
		}
	}

	// render god-rays into screen.
	{
		auto& pass = m_passUnderwaterGodRaysFinal;
		CTexture* pSrcTex = context.GetSrcBackBufferTexture();
		CTexture* pDstTex = context.GetDstBackBufferTexture();

		if (pass.IsDirty(pDstTex->GetID(), pSrcTex->GetID()))
		{
			static CCryNameTSCRC techName("UnderwaterGodRaysFinal");
			pass.SetPrimitiveFlags(CRenderPrimitive::eFlags_ReflectShaderConstants_PS);
			pass.SetPrimitiveType(CRenderPrimitive::ePrim_ProceduralTriangle);
			pass.SetTechnique(CShaderMan::s_shPostEffects, techName, 0);
			pass.SetState(GS_NODEPTHTEST);

			pass.SetRenderTarget(0, pDstTex);

			pass.SetTexture(0, pSrcTex);
			pass.SetTexture(1, m_pUnderwaterBumpTex);
			pass.SetTexture(2, CRendererResources::s_ptexBackBufferScaled[1]);

			pass.SetSampler(0, EDefaultSamplerStates::TrilinearClamp);
			pass.SetSampler(1, EDefaultSamplerStates::TrilinearWrap);
		}

		pass.BeginConstantUpdate();

		Vec4 pParams = Vec4(CRenderer::CV_r_water_godrays_distortion, 0, 0, 0);
		pass.SetConstant(param1Name, pParams);

		pass.Execute();
	}
}

//////////////////////////////////////////////////////////////////////////

CWaterDropletsPass::CWaterDropletsPass()
{

}

CWaterDropletsPass::~CWaterDropletsPass()
{
	SAFE_RELEASE(m_pWaterDropletsBumpTex);
}

void CWaterDropletsPass::Init()
{
	CRY_ASSERT(m_pWaterDropletsBumpTex == nullptr);
	m_pWaterDropletsBumpTex = CTexture::ForName("%ENGINE%/EngineAssets/Textures/water_droplets.dds", FT_DONT_STREAM, eTF_Unknown);
}

void CWaterDropletsPass::Execute(const CPostEffectContext& context)
{
	const CEffectParam* pAmount = context.GetEffectParamByName("WaterDroplets_Amount");
	CRY_ASSERT(pAmount != nullptr);

	if (!pAmount)
	{
		return;
	}

	CD3D9Renderer* const RESTRICT_POINTER rd = gcpRendD3D;

	auto& pass = m_passWaterDroplets;

	CTexture* pSrcTex = context.GetSrcBackBufferTexture();
	CTexture* pDstTex = context.GetDstBackBufferTexture();

	if (pass.IsDirty(pDstTex->GetID(), pSrcTex->GetID()))
	{
		static CCryNameTSCRC techName("WaterDroplets");
		pass.SetPrimitiveFlags(CRenderPrimitive::eFlags_ReflectShaderConstants_PS);
		pass.SetTechnique(CShaderMan::s_shPostEffectsGame, techName, 0);
		pass.SetState(GS_NODEPTHTEST);

		pass.SetRenderTarget(0, pDstTex);

		pass.SetTexture(0, pSrcTex);
		pass.SetTexture(1, m_pWaterDropletsBumpTex);

		pass.SetSampler(0, EDefaultSamplerStates::PointClamp);
		pass.SetSampler(1, EDefaultSamplerStates::TrilinearWrap);

		pass.SetRequirePerViewConstantBuffer(true);
	}

	pass.BeginConstantUpdate();

	const float fUserAmount = pAmount->GetParam();
	const float fAtten = 1.0f; //clamp_tpl<float>(fabs( gRenDev->GetRCamera().vOrigin.z - SPostEffectsUtils::m_fWaterLevel ), 0.0f, 1.0f);
	Vec4 vParams = Vec4(1, 1, 1, min(fUserAmount, 1.0f) * fAtten);
	static CCryNameR pParamName("waterDropletsParams");
	pass.SetConstant(pParamName, vParams);

	pass.Execute();
}

//////////////////////////////////////////////////////////////////////////

CWaterFlowPass::CWaterFlowPass()
{

}

CWaterFlowPass::~CWaterFlowPass()
{
	SAFE_RELEASE(m_pWaterFlowBumpTex);
}

void CWaterFlowPass::Init()
{
	CRY_ASSERT(m_pWaterFlowBumpTex == nullptr);
	m_pWaterFlowBumpTex = CTexture::ForName("%ENGINE%/EngineAssets/Textures/perlinNoiseNormal_ddn.tif", FT_DONT_STREAM, eTF_Unknown);
}

void CWaterFlowPass::Execute(const CPostEffectContext& context)
{
	const CEffectParam* pAmount = context.GetEffectParamByName("WaterFlow_Amount");
	CRY_ASSERT(pAmount != nullptr);

	if (!pAmount)
	{
		return;
	}

	CD3D9Renderer* const RESTRICT_POINTER rd = gcpRendD3D;

	auto& pass = m_passWaterFlow;

	CTexture* pSrcTex = context.GetSrcBackBufferTexture();
	CTexture* pDstTex = context.GetDstBackBufferTexture();

	if (pass.IsDirty(pDstTex->GetID(), pSrcTex->GetID()))
	{
		static CCryNameTSCRC techName("WaterFlow");
		pass.SetPrimitiveFlags(CRenderPrimitive::eFlags_ReflectShaderConstants_PS);
		pass.SetTechnique(CShaderMan::s_shPostEffectsGame, techName, 0);
		pass.SetState(GS_NODEPTHTEST);

		pass.SetRenderTarget(0, pDstTex);

		pass.SetTexture(0, pSrcTex);
		pass.SetTexture(1, m_pWaterFlowBumpTex);

		pass.SetSampler(0, EDefaultSamplerStates::PointClamp);
		pass.SetSampler(1, EDefaultSamplerStates::TrilinearWrap);

		pass.SetRequirePerViewConstantBuffer(true);
	}

	pass.BeginConstantUpdate();

	const float fAmount = pAmount->GetParam();
	Vec4 vParams = Vec4(1, 1, 1, fAmount);
	static CCryNameR pParamName("waterFlowParams");
	pass.SetConstant(pParamName, vParams);

	pass.Execute();
}

//////////////////////////////////////////////////////////////////////////

void CSharpeningPass::Init()
{

}

void CSharpeningPass::Execute(const CPostEffectContext& context)
{
	const CEffectParam* pAmount = context.GetEffectParamByName("FilterSharpening_Amount");
	CRY_ASSERT(pAmount != nullptr);

	if (!pAmount)
	{
		return;
	}

	PROFILE_LABEL_SCOPE("SHARPENING");

	CD3D9Renderer* const RESTRICT_POINTER rd = gcpRendD3D;

	const f32 fSharpenAmount = max(pAmount->GetParam(), CRenderer::CV_r_Sharpening + 1.0f);
	if (fSharpenAmount > 1e-6f)
	{
		m_passStrechRect.Execute(CRendererResources::s_ptexBackBuffer, CRendererResources::s_ptexBackBufferScaled[0]);
	}

	auto& pass = m_passSharpeningAndChromaticAberration;

	CTexture* pSrcTex = context.GetSrcBackBufferTexture();
	CTexture* pDstTex = context.GetDstBackBufferTexture();

	if (pass.IsDirty(pDstTex->GetID(), pSrcTex->GetID()))
	{
		static CCryNameTSCRC techName("CA_Sharpening");
		pass.SetPrimitiveFlags(CRenderPrimitive::eFlags_ReflectShaderConstants_PS);
		pass.SetPrimitiveType(CRenderPrimitive::ePrim_ProceduralTriangle);
		pass.SetTechnique(CShaderMan::s_shPostEffects, techName, 0);
		pass.SetState(GS_NODEPTHTEST);

		pass.SetRenderTarget(0, pDstTex);

		pass.SetTexture(0, pSrcTex);
		pass.SetTexture(1, CRendererResources::s_ptexBackBufferScaled[0]);

		pass.SetSampler(0, EDefaultSamplerStates::PointClamp);
		pass.SetSampler(1, EDefaultSamplerStates::LinearClamp);

		pass.SetRequirePerViewConstantBuffer(true);
	}

	pass.BeginConstantUpdate();

	Vec4 pParams = Vec4(CRenderer::CV_r_ChromaticAberration, 0, 0, fSharpenAmount);
	static CCryNameR pParamName("psParams");
	pass.SetConstant(pParamName, pParams);

	pass.Execute();
}

//////////////////////////////////////////////////////////////////////////

void CBlurringPass::Init()
{

}

void CBlurringPass::Execute(const CPostEffectContext& context)
{
	const CEffectParam* pAmount = context.GetEffectParamByName("FilterBlurring_Amount");
	CRY_ASSERT(pAmount != nullptr);

	if (!pAmount)
	{
		return;
	}

	PROFILE_LABEL_SCOPE("BLURRING");

	CD3D9Renderer* const RESTRICT_POINTER rd = gcpRendD3D;

	auto& pass = m_passBlurring;

	float fAmount = pAmount->GetParam();
	fAmount = clamp_tpl<float>(fAmount, 0.0f, 1.0f);

	// maximum blur amount to have nice results
	const float fMaxBlurAmount = 5.0f;

	m_passStrechRect.Execute(CRendererResources::s_ptexBackBuffer, CRendererResources::s_ptexBackBufferScaled[0]);
	m_passGaussianBlur.Execute(CRendererResources::s_ptexBackBufferScaled[0], CRendererResources::s_ptexBackBufferScaledTemp[0], 1.0f, LERP(0.0f, fMaxBlurAmount, fAmount));

	CTexture* pSrcTex = context.GetSrcBackBufferTexture();
	CTexture* pDstTex = context.GetDstBackBufferTexture();

	if (pass.IsDirty(pDstTex->GetID(), pSrcTex->GetID()))
	{
		static CCryNameTSCRC techName("BlurInterpolation");
		pass.SetPrimitiveFlags(CRenderPrimitive::eFlags_ReflectShaderConstants_PS);
		pass.SetPrimitiveType(CRenderPrimitive::ePrim_ProceduralTriangle);
		pass.SetTechnique(CShaderMan::s_shPostEffects, techName, 0);
		pass.SetState(GS_NODEPTHTEST);

		pass.SetRenderTarget(0, pDstTex);

		pass.SetTexture(0, CRendererResources::s_ptexBackBufferScaled[0]);
		pass.SetTexture(1, pSrcTex);

		pass.SetSampler(0, EDefaultSamplerStates::PointClamp);
		pass.SetSampler(1, EDefaultSamplerStates::LinearClamp);
	}

	pass.BeginConstantUpdate();

	Vec4 vParams(0, 0, 0, fAmount * fAmount);
	static CCryNameR pParamName("psParams");
	pass.SetConstant(pParamName, vParams);

	pass.Execute();
}

//////////////////////////////////////////////////////////////////////////

void CUberGamePostEffectPass::Init()
{

}

void CUberGamePostEffectPass::Execute(const CPostEffectContext& context)
{
	const auto* pPostEffect = context.GetPostEffect(EPostEffectID::UberGamePostProcess);
	if (pPostEffect)
	{
		PROFILE_LABEL_SCOPE("UBER_GAME_POSTPROCESS");

		CD3D9Renderer* const RESTRICT_POINTER rd = gcpRendD3D;

		const auto& gamePostEffect = *static_cast<const CUberGamePostProcess*>(pPostEffect);

		uint64 rtMask = 0;
		uint8 postEffectMask = gamePostEffect.m_nCurrPostEffectsMask;
		if (postEffectMask & CUberGamePostProcess::ePE_ChromaShift)
		{
			rtMask |= g_HWSR_MaskBit[HWSR_SAMPLE0];
		}
		if (postEffectMask & CUberGamePostProcess::ePE_SyncArtifacts)
		{
			rtMask |= g_HWSR_MaskBit[HWSR_SAMPLE2];
		}

		CTexture* pMaskTex = const_cast<CTexture*>(static_cast<CParamTexture*>(gamePostEffect.m_pMask)->GetParamTexture());
		int32 renderState = GS_NODEPTHTEST;
		{
			// Blend with backbuffer when user sets a mask
			if (pMaskTex)
			{
				renderState |= GS_BLSRC_SRCALPHA | GS_BLDST_ONEMINUSSRCALPHA;
			}

			pMaskTex = (pMaskTex != nullptr) ? pMaskTex : CRendererResources::s_ptexWhite;
		}

		auto& pass = m_passRadialBlurAndChromaShift;
		CTexture* pSrcTex = context.GetSrcBackBufferTexture();
		CTexture* pDstTex = context.GetDstBackBufferTexture();

		if (pass.IsDirty(pDstTex->GetID(), pSrcTex->GetID(), pMaskTex->GetID(), postEffectMask))
		{
			static CCryNameTSCRC techName("UberGamePostProcess");
			pass.SetPrimitiveFlags(CRenderPrimitive::eFlags_ReflectShaderConstants_PS);
			pass.SetPrimitiveType(CRenderPrimitive::ePrim_ProceduralTriangle);
			pass.SetTechnique(CShaderMan::s_shPostEffectsGame, techName, rtMask);
			pass.SetState(renderState);

			pass.SetRenderTarget(0, pDstTex);

			pass.SetTexture(0, pSrcTex);
			pass.SetTexture(2, pMaskTex);

			pass.SetSampler(0, EDefaultSamplerStates::LinearClamp);

			pass.SetRequirePerViewConstantBuffer(true);
		}

		pass.BeginConstantUpdate();

		Vec4 cColor = gamePostEffect.m_pColorTint->GetParamVec4();
		static CCryNameR pParamNamePS0("UberPostParams0");
		static CCryNameR pParamNamePS1("UberPostParams1");
		static CCryNameR pParamNamePS2("UberPostParams2");
		static CCryNameR pParamNamePS3("UberPostParams3");
		static CCryNameR pParamNamePS4("UberPostParams4");
		static CCryNameR pParamNamePS5("UberPostParams5");

		Vec4 vParamsPS[6] =
		{
			Vec4(gamePostEffect.m_pVSyncAmount->GetParam(), gamePostEffect.m_pInterlationAmount->GetParam(),           gamePostEffect.m_pInterlationTilling->GetParam(), gamePostEffect.m_pInterlationRotation->GetParam()),
			//Vec4(gamePostEffect.m_pVSyncFreq->GetParam(), 1.0f + max(0.0f, gamePostEffect.m_pPixelationScale->GetParam()*0.25f), gamePostEffect.m_pNoise->GetParam()*0.25f, gamePostEffect.m_pChromaShiftAmount->GetParam() + gamePostEffect.m_pFilterChromaShiftAmount->GetParam()),
			Vec4(gamePostEffect.m_pVSyncFreq->GetParam(),   1.0f,                                                      gamePostEffect.m_pNoise->GetParam() * 0.25f,      gamePostEffect.m_pChromaShiftAmount->GetParam() + gamePostEffect.m_pFilterChromaShiftAmount->GetParam()),
			Vec4(min(1.0f,                                  gamePostEffect.m_pGrainAmount->GetParam() * 0.1f * 0.25f), gamePostEffect.m_pGrainTile->GetParam(),          gamePostEffect.m_pSyncWavePhase->GetParam(),                                                             gamePostEffect.m_pSyncWaveFreq->GetParam()),
			Vec4(cColor.x,                                  cColor.y,                                                  cColor.z,                                         min(1.0f,                                                                                                gamePostEffect.m_pSyncWaveAmplitude->GetParam() * 0.01f)),
			Vec4(cry_random(0.0f,                           1.0f),                                                     cry_random(0.0f,                                  1.0f),                                                                                                   cry_random(0.0f,                                           1.0f),cry_random(0.0f, 1.0f)),
			Vec4(0,                                         0,                                                         0,                                                0),
		};

		pass.SetConstant(pParamNamePS0, vParamsPS[0]);
		pass.SetConstant(pParamNamePS1, vParamsPS[1]);
		pass.SetConstant(pParamNamePS2, vParamsPS[2]);
		pass.SetConstant(pParamNamePS3, vParamsPS[3]);
		pass.SetConstant(pParamNamePS4, vParamsPS[4]);
		pass.SetConstant(pParamNamePS5, vParamsPS[5]);

		pass.Execute();
	}
}

//////////////////////////////////////////////////////////////////////////

void CFlashBangPass::Init()
{

}

void CFlashBangPass::Execute(const CPostEffectContext& context)
{
	// TODO: each viewport would need to have its own post effect instance when we will support multi-viewport.
	auto* pPostEffect = const_cast<CPostEffect*>(context.GetPostEffect(EPostEffectID::FlashBang));
	if (pPostEffect)
	{
		CD3D9Renderer* const RESTRICT_POINTER rd = gcpRendD3D;

		CTexture* pSrcTex = context.GetSrcBackBufferTexture();

		auto& postEffect = *static_cast<CFlashBang*>(pPostEffect);

		const float fTimeDuration = postEffect.m_pTime->GetParam();
		const float fDifractionAmount = postEffect.m_pDifractionAmount->GetParam();
		const float fBlindTime = postEffect.m_pBlindAmount->GetParam();

		if (!postEffect.m_fSpawnTime)
		{
			postEffect.m_fSpawnTime = gEnv->pTimer->GetCurrTime();

			// Create temporary ghost image and capture screen
			SAFE_DELETE(postEffect.m_pGhostImage);

			const int flags = FT_USAGE_RENDERTARGET | FT_NOMIPS;
			postEffect.m_pGhostImage = new SDynTexture(pSrcTex->GetWidth() >> 1, pSrcTex->GetHeight() >> 1, Clr_Transparent, eTF_R8G8B8A8, eTT_2D, flags, "GhostImageTempRT");
			postEffect.m_pGhostImage->Update(pSrcTex->GetWidth() >> 1, pSrcTex->GetHeight() >> 1);

			if (postEffect.m_pGhostImage && postEffect.m_pGhostImage->m_pTexture)
			{
				m_passStrechRect.Execute(pSrcTex, postEffect.m_pGhostImage->m_pTexture);
			}
		}

		// Update current time
		const float fCurrTime = (gEnv->pTimer->GetCurrTime() - postEffect.m_fSpawnTime) / fTimeDuration;

		// Effect finished
		if (fCurrTime > 1.0f)
		{
			postEffect.m_fSpawnTime = 0.0f;
			postEffect.m_pActive->SetParam(0.0f);

			SAFE_DELETE(postEffect.m_pGhostImage);

			return;
		}

		// make sure to update dynamic texture if required
		if (postEffect.m_pGhostImage && !postEffect.m_pGhostImage->m_pTexture)
		{
			postEffect.m_pGhostImage->Update(pSrcTex->GetWidth() >> 1, pSrcTex->GetHeight() >> 1);
		}

		if (!postEffect.m_pGhostImage || !postEffect.m_pGhostImage->m_pTexture)
		{
			return;
		}

		auto& pass = m_passFlashBang;

		CTexture* pDstTex = context.GetDstBackBufferTexture();

		if (pass.IsDirty(pDstTex->GetID(), pSrcTex->GetID(), postEffect.m_pGhostImage->m_pTexture->GetID()))
		{
			static CCryNameTSCRC techName("FlashBang");
			pass.SetPrimitiveFlags(CRenderPrimitive::eFlags_ReflectShaderConstants_PS);
			pass.SetPrimitiveType(CRenderPrimitive::ePrim_ProceduralTriangle);
			pass.SetTechnique(CShaderMan::s_shPostEffectsGame, techName, 0);
			pass.SetState(GS_NODEPTHTEST);

			pass.SetRenderTarget(0, pDstTex);

			pass.SetTexture(0, pSrcTex);
			pass.SetTexture(1, postEffect.m_pGhostImage->m_pTexture);

			pass.SetSampler(0, EDefaultSamplerStates::PointClamp);
			pass.SetSampler(1, EDefaultSamplerStates::LinearClamp);
		}

		pass.BeginConstantUpdate();

		const float fLuminance = 1.0f - fCurrTime;  //PostProcessUtils().InterpolateCubic(0.0f, 1.0f, 0.0f, 1.0f, fCurrTime);
		Vec4 vParams = Vec4(fLuminance, fLuminance * fDifractionAmount, 3.0f * fLuminance * fBlindTime, fLuminance);
		static CCryNameR pParamName("vFlashBangParams");
		pass.SetConstant(pParamName, vParams);

		pass.Execute();
	}
}

//////////////////////////////////////////////////////////////////////////

void CPostStereoPass::Init()
{
	m_samplerLinearMirror = GetDeviceObjectFactory().GetOrCreateSamplerStateHandle(
		SSamplerState(FILTER_LINEAR, eSamplerAddressMode_Mirror, eSamplerAddressMode_Mirror, eSamplerAddressMode_Mirror, 0x0));

}

void CPostStereoPass::Execute(const CPostEffectContext& context)
{
	CD3D9Renderer* const RESTRICT_POINTER rd = gcpRendD3D;
	CD3DStereoRenderer* const RESTRICT_POINTER rendS3D = &(gcpRendD3D.GetS3DRend());
	
 	if (!rendS3D->IsPostStereoEnabled())
	{
		return;
	}

	PROFILE_LABEL_SCOPE("POST_STEREO");
	
	CTexture* pSrcBackBufferTexture = context.GetSrcBackBufferTexture();

	// Mask near geometry (weapon)
	CTexture* pTmpMaskTex = CRendererResources::s_ptexSceneNormalsBent; // non-msaaed target
	CRY_ASSERT(pTmpMaskTex);
	CRY_ASSERT(pTmpMaskTex->GetWidth() == pSrcBackBufferTexture->GetWidth());
	CRY_ASSERT(pTmpMaskTex->GetHeight() == pSrcBackBufferTexture->GetHeight());
	CRY_ASSERT(pTmpMaskTex->GetDstFormat() == pSrcBackBufferTexture->GetDstFormat());
	CTexture* pZTexture = context.GetRenderView()->GetDepthTarget();

	const bool bReverseDepth = true;

	{
		PROFILE_LABEL_SCOPE("NEAR_MASK");

		CClearSurfacePass::Execute(pTmpMaskTex, Clr_Neutral);

		auto& pass = m_passNearMask;

		if (pass.IsDirty(pTmpMaskTex->GetID()))
		{
			static CCryNameTSCRC techName("StereoNearMask");
			pass.SetPrimitiveFlags(CRenderPrimitive::eFlags_ReflectShaderConstants_PS);
			pass.SetTechnique(CShaderMan::s_shPostEffects, techName, 0);

			int32 nRS = GS_DEPTHFUNC_LEQUAL;
			nRS = bReverseDepth ? ReverseDepthHelper::ConvertDepthFunc(nRS) : nRS;
			pass.SetState(nRS);

			pass.SetRenderTarget(0, pTmpMaskTex);
			pass.SetDepthTarget(pZTexture);
		}

		const float clipZ = CRenderer::CV_r_DrawNearZRange;
		pass.SetRequireWorldPos(true, clipZ);

		pass.BeginConstantUpdate();

		pass.Execute();
	}

	{
		auto& pass = m_passPostStereo;

		auto *leftDc = rendS3D->GetEyeDisplayContext(CCamera::eEye_Left).first;
		auto *rightDc = rendS3D->GetEyeDisplayContext(CCamera::eEye_Right).first;
		if (!leftDc || !rightDc)
			return;

		auto* pLeftEyeTex = leftDc->GetCurrentBackBuffer();
		auto* pRightEyeTex = rightDc->GetCurrentBackBuffer();

		if (pass.IsDirty(pLeftEyeTex->GetID(), pRightEyeTex->GetID(), pSrcBackBufferTexture->GetID()))
		{
			static CCryNameTSCRC techName("PostStereo");
			pass.SetPrimitiveFlags(CRenderPrimitive::eFlags_ReflectShaderConstants_PS);
			pass.SetPrimitiveType(CRenderPrimitive::ePrim_ProceduralTriangle);
			pass.SetTechnique(CShaderMan::s_shPostEffects, techName, 0);
			pass.SetState(GS_NODEPTHTEST);

			pass.SetRenderTarget(0, pLeftEyeTex);
			pass.SetRenderTarget(1, pRightEyeTex);
			pass.SetDepthTarget(pZTexture);

			pass.SetTexture(0, pSrcBackBufferTexture);
			pass.SetTexture(1, CRendererResources::s_ptexLinearDepth);
			pass.SetTexture(2, pTmpMaskTex);

			pass.SetSampler(0, m_samplerLinearMirror);
			pass.SetSampler(1, EDefaultSamplerStates::PointClamp);
		}

		pass.BeginConstantUpdate();

		const auto& viewInfo = context.GetRenderView()->GetViewInfo(CCamera::eEye_Left);

		const float viewportScaleX = rd->m_CurViewportScale.x;
		const float viewportScaleY = rd->m_CurViewportScale.y;

		const float maxParallax = rendS3D->GetMaxSeparationScene();
		const float screenDist = rendS3D->GetZeroParallaxPlaneDist() / viewInfo.farClipPlane;
		const float nearGeoShift = rendS3D->GetNearGeoShift() / viewInfo.farClipPlane;
		const float nearGeoScale = rendS3D->GetNearGeoScale();

		static CCryNameR pParamName0("StereoParams");
		Vec4 stereoParams(maxParallax, screenDist, nearGeoShift, nearGeoScale);
		pass.SetConstant(pParamName0, stereoParams);

		static CCryNameR pParamName2("HPosScale");
		Vec4 hposScale(viewportScaleX, viewportScaleY, 0.0f, 0.0f);
		pass.SetConstant(pParamName2, hposScale);

		pass.Execute();
	}
}

//////////////////////////////////////////////////////////////////////////

CKillCameraPass::CKillCameraPass()
{

}

CKillCameraPass::~CKillCameraPass()
{
	SAFE_RELEASE(m_pNoiseTex);
}

void CKillCameraPass::Init()
{
	CRY_ASSERT(m_pNoiseTex == nullptr);
	m_pNoiseTex = CTexture::ForName("%ENGINE%/EngineAssets/Textures/vector_noise.dds", FT_DONT_STREAM, eTF_Unknown);
}

void CKillCameraPass::Execute(const CPostEffectContext& context)
{
	const auto* pPostEffect = context.GetPostEffect(EPostEffectID::KillCamera);
	if (pPostEffect)
	{
		PROFILE_LABEL_SCOPE("KILL_CAMERA");

		CD3D9Renderer* const RESTRICT_POINTER rd = gcpRendD3D;

		const auto& pe = *static_cast<const CFilterKillCamera*>(pPostEffect);

		auto& pass = m_passKillCameraFilter;

		CTexture* pSrcTex = context.GetSrcBackBufferTexture();
		CTexture* pDsttex = context.GetDstBackBufferTexture();

		if (pass.IsDirty(pDsttex->GetID(), pSrcTex->GetID()))
		{
			static CCryNameTSCRC techName("KillCameraFilter");
			pass.SetPrimitiveFlags(CRenderPrimitive::eFlags_ReflectShaderConstants_PS);
			pass.SetPrimitiveType(CRenderPrimitive::ePrim_ProceduralTriangle);
			pass.SetTechnique(CShaderMan::s_shPostEffectsGame, techName, 0);
			pass.SetState(GS_NODEPTHTEST | GS_BLSRC_SRCALPHA | GS_BLDST_ONEMINUSSRCALPHA);

			pass.SetRenderTarget(0, pDsttex);

			pass.SetTexture(0, pSrcTex);
			pass.SetTexture(1, m_pNoiseTex);

			pass.SetSampler(0, EDefaultSamplerStates::PointClamp);
			pass.SetSampler(1, EDefaultSamplerStates::PointWrap);

			pass.SetRequirePerViewConstantBuffer(true);
		}

		pass.BeginConstantUpdate();

		// update constant buffer
		{
			float grainStrength = pe.m_pGrainStrength->GetParam();
			Vec4 chromaShift = pe.m_pChromaShift->GetParamVec4(); // xyz = offset, w = strength
			Vec4 vignette = pe.m_pVignette->GetParamVec4();       // xy = screen scale, z = radius, w = blind noise vignette scale
			Vec4 colorScale = pe.m_pColorScale->GetParamVec4();

			Vec2 overscanBorders = Vec2(0.0f, 0.0f);
			rd->EF_Query(EFQ_OverscanBorders, overscanBorders);

			// Scale vignette with overscan borders
			const float vignetteOverscanMaxValue = 4.0f;
			const Vec2 vignetteOverscanScalar = Vec2(1.0f, 1.0f) + (overscanBorders * vignetteOverscanMaxValue);
			vignette.x *= vignetteOverscanScalar.x;
			vignette.y *= vignetteOverscanScalar.y;

			float inverseVignetteRadius = 1.0f / clamp_tpl<float>(vignette.z * 2.0f, 0.001f, 2.0f);
			Vec2 vignetteScreenScale(max(vignette.x, 0.0f), max(vignette.y, 0.0f));

			// Blindness
			Vec4 blindness = pe.m_pBlindness->GetParamVec4(); // x = blind duration, y = blind fade out duration, z = blindness grey scale, w = blind noise min scale
			float blindDuration = max(blindness.x, 0.0f);
			float blindFadeOutDuration = max(blindness.y, 0.0f);
			float blindGreyScale = clamp_tpl<float>(blindness.z, 0.0f, 1.0f);
			float blindNoiseMinScale = clamp_tpl<float>(blindness.w, 0.0f, 10.0f);
			float blindNoiseVignetteScale = clamp_tpl<float>(vignette.w, 0.0f, 10.0f);

			float blindAmount = 0.0f;
			if (pe.m_blindTimer < blindDuration)
			{
				blindAmount = 1.0f;
			}
			else
			{
				float blindFadeOutTimer = pe.m_blindTimer - blindDuration;
				if (blindFadeOutTimer < blindFadeOutDuration)
				{
					blindAmount = 1.0f - (blindFadeOutTimer / blindFadeOutDuration);
				}
			}

			// TODO: move to post effect context.
			int width = 0;
			int height = 0;
			if (context.GetRenderView())
			{
				const SRenderViewport &viewport = context.GetRenderView()->GetViewport();
				width = viewport.width;
				height = viewport.height;
			}

			// Set PS default params
			const int32 PARAM_COUNT = 4;
			Vec4 pParams[PARAM_COUNT];

			// psParams[0] - xy = Rand lookup, zw = vignetteScreenScale * invRadius
			pParams[0].x = cry_random(0, 1023) / (float)width;
			pParams[0].y = cry_random(0, 1023) / (float)height;
			pParams[0].z = vignetteScreenScale.x * inverseVignetteRadius;
			pParams[0].w = vignetteScreenScale.y * inverseVignetteRadius;

			// psParams[1] - xyz = color scale, w = grain strength
			pParams[1].x = colorScale.x;
			pParams[1].y = colorScale.y;
			pParams[1].z = colorScale.z;
			pParams[1].w = grainStrength;

			// psParams[2] - xyz = chroma shift, w = chroma shift color strength
			pParams[2] = chromaShift;

			// psParams[3] - x = blindAmount, y = blind grey scale, z = blindNoiseVignetteScale, w = blindNoiseMinScale
			pParams[3].x = blindAmount;
			pParams[3].y = blindGreyScale;
			pParams[3].z = blindNoiseVignetteScale;
			pParams[3].w = blindNoiseMinScale;

			static CCryNameR pParamName("psParams");
			pass.SetConstantArray(pParamName, pParams, PARAM_COUNT);
		}

		pass.Execute();
	}
}

//////////////////////////////////////////////////////////////////////////

CScreenBloodPass::CScreenBloodPass()
{

}

CScreenBloodPass::~CScreenBloodPass()
{
	SAFE_RELEASE(m_pWaterDropletsBumpTex);
}

void CScreenBloodPass::Init()
{
	CRY_ASSERT(m_pWaterDropletsBumpTex == nullptr);
	m_pWaterDropletsBumpTex = CTexture::ForName("%ENGINE%/EngineAssets/Textures/water_droplets.dds", FT_DONT_STREAM, eTF_Unknown);
}

void CScreenBloodPass::Execute(const CPostEffectContext& context)
{
	const CEffectParam* pAmount = context.GetEffectParamByName("ScreenBlood_Amount");
	CRY_ASSERT(pAmount != nullptr);

	const CEffectParam* pBorder = context.GetEffectParamByName("ScreenBlood_Border");
	CRY_ASSERT(pBorder != nullptr);

	if (!pAmount || !pBorder)
	{
		return;
	}

	PROFILE_LABEL_SCOPE("SCREEN BLOOD");

	CD3D9Renderer* const RESTRICT_POINTER rd = gcpRendD3D;

	auto& pass = m_passScreenBlood;

	CTexture* pDstTex = context.GetDstBackBufferTexture();

	if (pass.IsDirty(pDstTex->GetID()))
	{
		static CCryNameTSCRC techName("ScreenBlood");
		pass.SetPrimitiveFlags(CRenderPrimitive::eFlags_ReflectShaderConstants_PS);
		pass.SetPrimitiveType(CRenderPrimitive::ePrim_ProceduralTriangle);
		pass.SetTechnique(CShaderMan::s_shPostEffectsGame, techName, 0);
		pass.SetState(GS_NODEPTHTEST | GS_BLSRC_DSTCOL | GS_BLDST_SRCALPHA);

		pass.SetRenderTarget(0, pDstTex);

		pass.SetTexture(0, m_pWaterDropletsBumpTex);

		pass.SetSampler(0, EDefaultSamplerStates::TrilinearWrap);

		pass.SetRequirePerViewConstantBuffer(true);
	}

	pass.BeginConstantUpdate();

	// Border params
	const Vec4 borderParams = pBorder->GetParamVec4();
	const float borderRange = borderParams.z;
	const Vec2 borderOffset = Vec2(borderParams.x, borderParams.y);
	const float alpha = borderParams.w;

	Vec2 overscanBorders = Vec2(0.0f, 0.0f);
	rd->EF_Query(EFQ_OverscanBorders, overscanBorders);

	// Use overscan borders to scale blood thickness around screen
	const float overscanScalar = 3.0f;
	overscanBorders = Vec2(1.0f, 1.0f) + ((overscanBorders + borderOffset) * overscanScalar);

	const float borderScale = max(0.2f, borderRange - (pAmount->GetParam() * borderRange));
	Vec4 pParams = Vec4(overscanBorders.x, overscanBorders.y, alpha, borderScale);
	static CCryNameR pParamName("psParams");
	pass.SetConstant(pParamName, pParams);

	pass.Execute();
}

//////////////////////////////////////////////////////////////////////////

void CScreenFaderPass::Init()
{}

void CScreenFaderPass::Execute(const CPostEffectContext& context)
{
	const CEffectParam* pColor = context.GetEffectParamByName("ScreenFader_Color");
	CRY_ASSERT(pColor != nullptr);

	if (!pColor)
	{
		return;
	}

	PROFILE_LABEL_SCOPE("SCREEN FADER");

	CD3D9Renderer* const RESTRICT_POINTER rd = gcpRendD3D;

	auto& pass = m_passScreenFader;
	const Vec4 &color = pColor->GetParamVec4();

	CTexture* pDstTex = context.GetDstBackBufferTexture();

	if (pass.IsDirty(pDstTex->GetID()))
	{
		static CCryNameTSCRC techTexToTex("TextureToTextureTinted");
		pass.SetPrimitiveFlags(CRenderPrimitive::eFlags_ReflectShaderConstants_PS);
		pass.SetPrimitiveType(CRenderPrimitive::ePrim_ProceduralTriangle);
		pass.SetTechnique(CShaderMan::s_shPostEffects, techTexToTex, 0);
		pass.SetState(GS_NODEPTHTEST | GS_BLSRC_SRCALPHA | GS_BLDST_ONEMINUSSRCALPHA);

		pass.SetRenderTarget(0, pDstTex);

		pass.SetTexture(0, CRendererResources::s_ptexWhite);

		pass.SetSampler(0, EDefaultSamplerStates::PointClamp);

		pass.SetRequirePerViewConstantBuffer(true);
	}

	pass.BeginConstantUpdate();

	static CCryNameR paramName("texToTexParams2");
	pass.SetConstant(paramName, color);

	pass.Execute();
}

//////////////////////////////////////////////////////////////////////////

void CHudSilhouettesPass::Init()
{

}

void CHudSilhouettesPass::Execute(const CPostEffectContext& context)
{
	const CEffectParam* pAmount = context.GetEffectParamByName("HudSilhouettes_Amount");
	CRY_ASSERT(pAmount != nullptr);

	const CEffectParam* pType = context.GetEffectParamByName("HudSilhouettes_Type");
	CRY_ASSERT(pType != nullptr);

	const CEffectParam* pFillStr = context.GetEffectParamByName("HudSilhouettes_FillStr");
	CRY_ASSERT(pFillStr != nullptr);

	if (!pAmount || !pType || !pFillStr)
	{
		return;
	}

	PROFILE_LABEL_SCOPE("HUD_SILHOUETTES");

	// Render highlighted geometry
	if (CRenderer::CV_r_customvisions == 1
	    || CRenderer::CV_r_customvisions == 3)
	{
		CD3D9Renderer* const RESTRICT_POINTER rd = gcpRendD3D;
		auto* pSceneCustom = rd->GetGraphicsPipeline().GetSceneCustomStage();
		if (pSceneCustom)
		{
			pSceneCustom->ExecuteSilhouettePass();
		}
	}

	const float fBlendParam = clamp_tpl<float>(pAmount->GetParam(), 0.0f, 1.0f);
	const float fType = pType->GetParam();
	const float fFillStrength = pFillStr->GetParam();

	// Render silhouettes
	switch (CRenderer::CV_r_customvisions)
	{
	case 2:
		// These are forward rendered so do nothing
		break;
	case 1:
	case 3:
		// use optimized version for all
		ExecuteDeferredSilhouettesOptimised(context, fBlendParam, fType, fFillStrength);
		break;
	default:
		// do nothing
		break;
	}
}

void CHudSilhouettesPass::ExecuteDeferredSilhouettesOptimised(const CPostEffectContext& context, float fBlendParam, float fType, float fFillStrength)
{
	const bool bHasSilhouettesToRender = (context.GetRenderView()->GetBatchFlags(EFSLIST_CUSTOM) & FB_CUSTOM_RENDER) ? true : false;

	if (bHasSilhouettesToRender)
	{
		PROFILE_LABEL_SCOPE("DEFERRED_SILHOUETTES_PASS");

		// Down Sample
		m_passStrechRect.Execute(CRendererResources::s_ptexSceneNormalsMap, CRendererResources::s_ptexBackBufferScaled[0]);

		auto& pass = m_passDeferredSilhouettesOptimised;
		CTexture* pDstTex = context.GetDstBackBufferTexture();
		if (pass.IsDirty(pDstTex->GetID()))
		{
			static CCryNameTSCRC techName("DeferredSilhouettesOptimised");
			pass.SetTechnique(CShaderMan::s_shPostEffectsGame, techName, 0);
			pass.SetState(GS_NODEPTHTEST | GS_BLSRC_ONE | GS_BLDST_ONE | GS_NOCOLMASK_A);

			pass.SetRenderTarget(0, pDstTex);

			pass.SetTexture(0, CRendererResources::s_ptexBackBufferScaled[0]);

			pass.SetSampler(0, EDefaultSamplerStates::LinearClamp);

			pass.SetRequirePerViewConstantBuffer(true);
		}

		pass.BeginConstantUpdate();

		// Set vs params
		const float uvOffset = 1.5f;
		Vec4 vsParams = Vec4(uvOffset, 0.0f, 0.0f, 0.0f);
		static CCryNameR vsParamName("vsParams");
		pass.SetConstant(vsParamName, vsParams, eHWSC_Vertex);

		// Set ps params
		const float fillStrength = fFillStrength;
		const float silhouetteBoost = 1.7f;
		const float silhouetteBrightness = 1.333f;
		const float focusReduction = 0.33f;
		const float silhouetteAlpha = 0.8f;
		const bool bBinocularsActive = (!fType);
		const float silhouetteStrength = (bBinocularsActive ? 1.0f : (fBlendParam * focusReduction)) * silhouetteAlpha;

		Vec4 psParams;
		psParams.x = silhouetteStrength;
		psParams.y = silhouetteBoost;
		psParams.z = silhouetteBrightness;
		psParams.w = fillStrength;

		static CCryNameR psParamName("psParams");
		pass.SetConstant(psParamName, psParams, eHWSC_Pixel);

		pass.Execute();
	}
}

//////////////////////////////////////////////////////////////////////////

CHud3DPass::CHud3DPass()
{

}

CHud3DPass::~CHud3DPass()
{
	for (auto it = m_downsamplePrimitiveArray.begin(); it != m_downsamplePrimitiveArray.end(); ++it)
	{
		delete *it;
	}
	m_downsamplePrimitiveArray.clear();

	for (auto it = m_bloomPrimitiveArray.begin(); it != m_bloomPrimitiveArray.end(); ++it)
	{
		delete *it;
	}
	m_bloomPrimitiveArray.clear();

	for (auto it = m_hudPrimitiveArray.begin(); it != m_hudPrimitiveArray.end(); ++it)
	{
		delete *it;
	}
	m_hudPrimitiveArray.clear();
}

void CHud3DPass::Init()
{

}

void CHud3DPass::Execute(const CPostEffectContext& context)
{
	// TODO: each viewport would need to have its own post effect instance when we will support multi-viewport.
	auto* pPostEffect = const_cast<CPostEffect*>(context.GetPostEffect(EPostEffectID::HUD3D));
	if (pPostEffect)
	{
		PROFILE_LABEL_SCOPE("3D HUD");

		CD3D9Renderer* const RESTRICT_POINTER rd = gcpRendD3D;

		auto& hud3d = *static_cast<CHud3D*>(pPostEffect);

		CD3DStereoRenderer& pS3DRend = rd->GetS3DRend();

		// Update interference rand timer
		hud3d.m_interferenceRandTimer += gEnv->pTimer->GetFrameTime();

		CTexture* pDstTex = context.GetDstBackBufferTexture();
		CTexture* pDepthS = context.GetDstDepthStencilTexture();
		if (pS3DRend.IsPostStereoEnabled())
			pDstTex = pS3DRend.GetVrQuadLayerDisplayContext(RenderLayer::eQuadLayers_Headlocked_0).first->GetCurrentBackBuffer();

		ExecuteBloomTexUpdate(context, hud3d);
		ExecuteFinalPass(context, pDstTex, pDepthS, hud3d);
	}
}

// Reminder: for efficient multiple flash files to work correctly - uv's must not override
void CHud3DPass::ExecuteFlashUpdate(const CPostEffectContext& context, CHud3D& hud3d)
{
#if defined(USE_VBIB_PUSH_DOWN)
	//workaround for deadlock when streaming thread wants renderthread to clean mesh pool
	if (gRenDev->m_bStartLevelLoading)
	{
		CRenderMesh::Tick();
	}
#endif

	const uint32 nThreadID = gRenDev->GetRenderThreadID();

	uint32 nRECount = hud3d.m_pRenderData[nThreadID].size();
	const bool bForceRefresh = (hud3d.m_pOverideCacheDelay->GetParam() > 0.5f);

	if (nRECount || bForceRefresh) //&& m_nFlashUpdateFrameID != rd->GetFrameID(false) )
	{
		// Share hud render target with scene normals
		hud3d.m_pHUD_RT       = CRendererResources::s_ptexCached3DHud;
		hud3d.m_pHUDScaled_RT = CRendererResources::s_ptexCached3DHudScaled;

		if ((context.GetRenderView()->GetFrameId() % max(1, (int)CRenderer::CV_r_PostProcessHUD3DCache)) != 0)
		{
			if (!bForceRefresh)
			{
				// CV_r_PostProcessHUD3DCache>0 will skip flash asset advancing. Accumulate frame time of
				//skipped frames and add it when we're actually advancing.
				hud3d.m_accumulatedFrameTime += gEnv->pTimer->GetFrameTime();

				hud3d.ReleaseFlashPlayerRef(nThreadID);

				return;
			}
			else
			{
				hud3d.m_pOverideCacheDelay->SetParam(0.0f);
				CryLog("reseting param");
			}
		}

		hud3d.m_nFlashUpdateFrameID = context.GetRenderView()->GetFrameId();

		PROFILE_LABEL_SCOPE("3D HUD FLASHPLAYER UPDATES");

		CClearSurfacePass::Execute(hud3d.m_pHUD_RT, Clr_Transparent);

		const int rtWidth  = std::min<int>(SHudData::s_nFlashWidthMax , hud3d.m_pHUD_RT->GetWidth());
		const int rtHeight = std::min<int>(SHudData::s_nFlashHeightMax, hud3d.m_pHUD_RT->GetHeight());

		const float accumulatedDeltaTime = gEnv->pTimer->GetFrameTime() + hud3d.m_accumulatedFrameTime;

		SEfResTexture* pDiffuse = nullptr;
		SEfResTexture* pPrevDiffuse = nullptr;

		for (uint32 r = 0; r < nRECount; ++r)
		{
			SHudData& pData = hud3d.m_pRenderData[nThreadID][r];

			pDiffuse = pData.pDiffuse;
			if (!pDiffuse || !pDiffuse->m_Sampler.m_pDynTexSource)
			{
				continue;
			}

			if (pData.pFlashPlayer && pPrevDiffuse != pDiffuse)
			{
				PROFILE_LABEL_SCOPE("3D HUD FLASHFILE");

				const int width  = std::min(pData.pFlashPlayer->GetWidth (), rtWidth);
				const int height = std::min(pData.pFlashPlayer->GetHeight(), rtHeight);

				pData.pFlashPlayer->Advance(accumulatedDeltaTime);

				pData.pFlashPlayer->SetViewport(0, 0, width, height);
				pData.pFlashPlayer->SetScissorRect(0, 0, width, height);
				pData.pFlashPlayer->AvoidStencilClear(!CRendererCVars::CV_r_PostProcessHUD3DStencilClear);

				// Lockless rendering playback.
				CScaleformPlayback::RenderFlashPlayerToTexture(pData.pFlashPlayer, hud3d.m_pHUD_RT);

				pData.pFlashPlayer->AvoidStencilClear(false);

				pPrevDiffuse = pDiffuse;

				break;
			}
		}

		hud3d.m_accumulatedFrameTime = 0.0f;
		hud3d.ReleaseFlashPlayerRef(nThreadID);

		// Downsample/blur hud into half res target _1 time only_ - we'll use this for Bloom/Dof
		ExecuteDownsampleHud4x4(context, hud3d, hud3d.m_pHUDScaled_RT);
	}
}

void CHud3DPass::ExecuteDownsampleHud4x4(const CPostEffectContext& context, class CHud3D& hud3d, CTexture* pDstRT)
{
	PROFILE_LABEL_SCOPE("3D HUD DOWNSAMPLE 4X4");

	CD3D9Renderer* const RESTRICT_POINTER rd = gcpRendD3D;
	const uint32 nThreadID = gRenDev->GetRenderThreadID();

	// temporary clear/fix - try avoiding this
	CClearSurfacePass::Execute(pDstRT, Clr_Transparent);

	// Render in uv space - use unique geometry for discarding uncovered pixels for faster blur/downsampling
	// tessellation and skinning aren't supported.
	{
		auto& pass = m_passDownsampleHud4x4;

		D3DViewPort viewport;
		viewport.TopLeftX = 0.0f;
		viewport.TopLeftY = 0.0f;
		viewport.Width = static_cast<float>(pDstRT->GetWidth());
		viewport.Height = static_cast<float>(pDstRT->GetHeight());
		viewport.MinDepth = 0.0f;
		viewport.MaxDepth = 1.0f;

		pass.SetRenderTarget(0, pDstRT);
		pass.SetViewport(viewport);
		pass.BeginAddingPrimitives();

		auto& renderDataArray = hud3d.m_pRenderData[nThreadID];
		uint32 nRECount = renderDataArray.size();
		uint32 index = 0;

		CConstantBufferPtr pPerViewCB = rd->GetGraphicsPipeline().GetMainViewConstantBuffer();

		auto& primArray = m_downsamplePrimitiveArray;
		if (primArray.size() > nRECount)
		{
			for (auto it = (primArray.begin() + nRECount); it != primArray.end(); ++it)
			{
				delete *it;
			}
		}
		primArray.resize(nRECount, nullptr);

		for (uint32 r = 0; r < nRECount; ++r)
		{
			SHudData& pData = renderDataArray[r];
			CShaderResources* pShaderResources = (CShaderResources*)pData.pShaderResources;

			SEfResTexture* pDiffuse = pData.pDiffuse;
			if (!pShaderResources || !pDiffuse || !pDiffuse->m_Sampler.m_pDynTexSource)
			{
				continue;
			}

			SRenderObjData* pROData = pData.pRO->GetObjData();
			if (pROData)
			{
				if (pROData->m_nCustomFlags & COB_HUD_DISABLEBLOOM)
				{
					continue; // Skip the low alpha planes in bloom phase (Fixes overglow and cut off with alien hud ghosted planes)
				}
			}

			if (primArray[index] == nullptr)
			{
				primArray[index] = new CRenderPrimitive();
			}
			auto& prim = *primArray[index];

			if (SetVertex(prim, pData))
			{
				prim.SetFlags(CRenderPrimitive::eFlags_ReflectShaderConstants);
				prim.SetTechnique(CShaderMan::s_sh3DHUD, hud3d.m_pDownsampleTechName, 0);
				prim.SetCullMode(eCULL_Back);
				prim.SetRenderState(GS_NODEPTHTEST);

				if (pDiffuse && pDiffuse->m_Sampler.m_pTex)
				{
					prim.SetTexture(0, pDiffuse->m_Sampler.m_pTex);
				}
				else
				{
					prim.SetTexture(0, hud3d.m_pHUD_RT);
				}
				prim.SetSampler(0, EDefaultSamplerStates::LinearClamp);
				prim.SetInlineConstantBuffer(eConstantBufferShaderSlot_PerView, pPerViewCB, EShaderStage_Vertex | EShaderStage_Pixel);
				prim.Compile(pass);

				// update constant buffer
				{
					auto& cm = prim.GetConstantManager();
					cm.BeginNamedConstantUpdate();

					SetShaderParams(context,EShaderStage_Vertex | EShaderStage_Pixel, cm, pData, hud3d);

					cm.EndNamedConstantUpdate(&viewport); // Unmap constant buffers and mark as bound
				}

				pass.AddPrimitive(&prim);

				++index;
			}
		}

		pass.Execute();
	}
}

void CHud3DPass::ExecuteBloomTexUpdate(const CPostEffectContext& context, class CHud3D& hud3d)
{
	PROFILE_LABEL_SCOPE("UPDATE BLOOM TEX");

	CD3D9Renderer* const RESTRICT_POINTER rd = gcpRendD3D;
	const auto nThreadID = gRenDev->GetRenderThreadID();

	// Calculate HUD's projection matrix using fixed FOV.
	hud3d.CalculateProjMatrix();

	CTexture* pOutputRT = CRendererResources::s_ptexBackBufferScaled[1];

	// temporary clear/fix - try avoiding this
	CClearSurfacePass::Execute(pOutputRT, Clr_Transparent);

	// render HUD meshes. tessellation and skinning aren't supported.
	{
		auto& pass = m_passUpdateBloom;

		D3DViewPort viewport;
		viewport.TopLeftX = 0.0f;
		viewport.TopLeftY = 0.0f;
		viewport.Width = static_cast<float>(pOutputRT->GetWidth());
		viewport.Height = static_cast<float>(pOutputRT->GetHeight());
		viewport.MinDepth = 0.0f;
		viewport.MaxDepth = 1.0f;

		pass.SetRenderTarget(0, pOutputRT);
		pass.SetViewport(viewport);
		pass.BeginAddingPrimitives();

		auto& renderDataArray = hud3d.m_pRenderData[nThreadID];
		uint32 nRECount = renderDataArray.size();
		uint32 index = 0;

		CConstantBufferPtr pPerViewCB = rd->GetGraphicsPipeline().GetMainViewConstantBuffer();

		auto& primArray = m_bloomPrimitiveArray;
		if (primArray.size() > nRECount)
		{
			for (auto it = (primArray.begin() + nRECount); it != primArray.end(); ++it)
			{
				delete *it;
			}
		}
		primArray.resize(nRECount, nullptr);

		for (uint32 r = 0; r < nRECount; ++r)
		{
			SHudData& pData = renderDataArray[r];
			CShaderResources* pShaderResources = (CShaderResources*)pData.pShaderResources;

			SEfResTexture* pDiffuse = pData.pDiffuse;
			if (!pShaderResources || !pDiffuse || !pDiffuse->m_Sampler.m_pDynTexSource)
			{
				continue;
			}

			SRenderObjData* pROData = pData.pRO->GetObjData();
			if (pROData)
			{
				if (pROData->m_nCustomFlags & COB_HUD_DISABLEBLOOM)
				{
					continue; // Skip the low alpha planes in bloom phase (Fixes overglow and cut off with alien hud ghosted planes)
				}
			}

			if (primArray[index] == nullptr)
			{
				primArray[index] = new CRenderPrimitive();
			}
			auto& prim = *primArray[index];

			if (SetVertex(prim, pData))
			{
				prim.SetFlags(CRenderPrimitive::eFlags_ReflectShaderConstants_VS);
				prim.SetTechnique(CShaderMan::s_sh3DHUD, hud3d.m_pUpdateBloomTechName, 0);
				prim.SetCullMode(eCULL_Back);
				prim.SetRenderState(GS_NODEPTHTEST);

				prim.SetTexture(0, hud3d.m_pHUDScaled_RT);
				prim.SetSampler(0, EDefaultSamplerStates::LinearClamp);
				prim.SetInlineConstantBuffer(eConstantBufferShaderSlot_PerView, pPerViewCB, EShaderStage_Vertex | EShaderStage_Pixel);
				prim.Compile(pass);

				// update constant buffer
				{
					auto& cm = prim.GetConstantManager();
					cm.BeginNamedConstantUpdate();

					SetShaderParams(context,EShaderStage_Vertex, cm, pData, hud3d);

					cm.EndNamedConstantUpdate(&viewport); // Unmap constant buffers and mark as bound
				}

				pass.AddPrimitive(&prim);

				++index;
			}
		}

		pass.Execute();
	}

	CTexture* pBlurDst = CRendererResources::s_ptexBackBufferScaledTemp[1];
	if (pBlurDst)
	{
		m_passBlurGaussian.Execute(pOutputRT, pBlurDst, 1.0f, 0.85f);
	}
}

void CHud3DPass::ExecuteFinalPass(const CPostEffectContext& context, CTexture* pOutputRT, CTexture* pOutputDS, CHud3D& hud3d)
{
	PROFILE_LABEL_SCOPE("3D HUD FINAL PASS");

	CD3D9Renderer* const RESTRICT_POINTER rd = gcpRendD3D;
	const auto nThreadID = gRenDev->GetRenderThreadID();

	// Calculate HUD's projection matrix using fixed FOV.
	hud3d.CalculateProjMatrix();

	bool bInterferenceApplied = false;
	uint32 hudEffectParamCount = 1; // Default to only pass 1 vecs

	// [0] = default params
	// [1]-[4] = interference params
	Vec4 vHudEffectParams[5];
	Vec4 vInterferenceParams;
	float interferenceStrength = clamp_tpl<float>(hud3d.m_pInterference->GetParam(), hud3d.m_pInterferenceMinValue->GetParam(), 1.0f);
	uint64 rtMask = 0;

	if (interferenceStrength > 0.0f)
	{
		bInterferenceApplied = true;
		rtMask = g_HWSR_MaskBit[HWSR_SAMPLE1]; // Set interference combination
		hudEffectParamCount += 4;              // Pass interference params as well

		// x= hudItemOverrideStrength, y= interferenceRandFrequency, z= dofInterferenceStrength, w = free
		vInterferenceParams = hud3d.m_pInterferenceParams[0]->GetParamVec4();
		Limit(vInterferenceParams.x, 0.0f, 1.0f);
		Limit(vInterferenceParams.y, 0.0f, 1.0f);
		Limit(vInterferenceParams.z, 0.0f, 1.0f);

		float interferenceRandFrequency = max(vInterferenceParams.y, 0.0f);
		if (hud3d.m_interferenceRandTimer >= interferenceRandFrequency)
		{
			hud3d.m_interferenceRandNums.x = cry_random(-1.0f, 1.0f);
			hud3d.m_interferenceRandNums.y = cry_random(-1.0f, 1.0f);
			hud3d.m_interferenceRandNums.z = cry_random(0.0f, 1.0f);
			hud3d.m_interferenceRandNums.w = cry_random(-1.0f, 1.0f);
			hud3d.m_interferenceRandTimer = 0.0f;
		}

		// x = randomGrainStrengthScale, y= randomFadeStrengthScale, z= chromaShiftStrength, w= chromaShiftDist
		const float chromaShiftMaxStrength = 2.0f;
		const float chromaShiftMaxDist = 0.007f;
		vHudEffectParams[2] = hud3d.m_pInterferenceParams[1]->GetParamVec4();
		Limit(vHudEffectParams[2].x, 0.0f, 1.0f);
		Limit(vHudEffectParams[2].y, 0.0f, 1.0f);
		vHudEffectParams[2].x = ((1.0f - vHudEffectParams[2].x) + (hud3d.m_interferenceRandNums.z * vHudEffectParams[2].x));
		vHudEffectParams[2].y = ((1.0f - vHudEffectParams[2].y) + (hud3d.m_interferenceRandNums.z * vHudEffectParams[2].y));
		vHudEffectParams[2].z = clamp_tpl<float>(vHudEffectParams[2].z, 0.0f, 1.0f) * chromaShiftMaxStrength;
		vHudEffectParams[2].w = clamp_tpl<float>(vHudEffectParams[2].w, 0.0f, 1.0f) * chromaShiftMaxDist;

		// x= disruptScale, y= disruptMovementScale, z= noiseStrength, w = barScale
		const float disruptMaxScale = 100.0f;
		const float disruptMovementMaxScale = 0.012f;
		const float noiseMaxScale = 10.0f;
		const float barMaxScale = 10.0f;
		vHudEffectParams[3] = hud3d.m_pInterferenceParams[2]->GetParamVec4();
		vHudEffectParams[3].x = clamp_tpl<float>(vHudEffectParams[3].x, 0.0f, 1.0f) * disruptMaxScale;
		vHudEffectParams[3].y = clamp_tpl<float>(vHudEffectParams[3].y, 0.0f, 1.0f) * disruptMovementMaxScale;
		vHudEffectParams[3].z = clamp_tpl<float>(vHudEffectParams[3].z, 0.0f, 1.0f) * noiseMaxScale;
		Limit(vHudEffectParams[3].w, 0.0f, 1.0f);
		vHudEffectParams[3].x *= hud3d.m_interferenceRandNums.z * hud3d.m_interferenceRandNums.z;
		vHudEffectParams[3].w = (1.0f - vHudEffectParams[3].w) * hud3d.m_interferenceRandNums.z * barMaxScale;

		// xyz= barColor, w= bar color scale
		vHudEffectParams[4] = hud3d.m_pInterferenceParams[3]->GetParamVec4();
		Limit(vHudEffectParams[4].x, 0.0f, 1.0f);
		Limit(vHudEffectParams[4].y, 0.0f, 1.0f);
		Limit(vHudEffectParams[4].z, 0.0f, 1.0f);
		Limit(vHudEffectParams[4].w, 0.0f, 20.0f);
		vHudEffectParams[4] *= vHudEffectParams[4].w;  // Scale color values by color scale
		vHudEffectParams[4].w = vInterferenceParams.z; // dofInterferenceStrength
	}

	// Hud simple 2D dof blend
	const CPostEffect* pDofPostEffect = context.GetPostEffect(EPostEffectID::DepthOfField);
	const bool bGameDof = pDofPostEffect->IsActive();

	static float fDofBlend = 0.0f;
	fDofBlend += ((bGameDof ? .6f : 0.0f) - fDofBlend) * gEnv->pTimer->GetFrameTime() * 10.0f;
	float dofMultiplier = clamp_tpl<float>(hud3d.m_pDofMultiplier->GetParam(), 0.0f, 2.0f);
	float fCurrentDofBlend = fDofBlend * dofMultiplier;

	Vec4 vOverrideColorParams = hud3d.m_pGlowColorMul->GetParamVec4(); // Apply override color multipliers (Used to change the glow flash color for Alien HUD), alpha multiplier (.w) is set per plane

	// render HUD meshes. tessellation and skinning aren't supported.
	{
		auto& pass = m_passRenderHud;

		D3DViewPort viewport;
		viewport.TopLeftX = 0.0f;
		viewport.TopLeftY = 0.0f;
		viewport.Width = static_cast<float>(pOutputRT->GetWidth());
		viewport.Height = static_cast<float>(pOutputRT->GetHeight());
		viewport.MinDepth = 0.0f;
		viewport.MaxDepth = 1.0f;

		pass.SetRenderTarget(0, pOutputRT);
		pass.SetDepthTarget(pOutputDS);
		pass.SetViewport(viewport);
		pass.BeginAddingPrimitives();

		auto& renderDataArray = hud3d.m_pRenderData[nThreadID];
		uint32 nRECount = renderDataArray.size();
		uint32 index = 0;

		CConstantBufferPtr pPerViewCB = rd->GetGraphicsPipeline().GetMainViewConstantBuffer();

		auto& primArray = m_hudPrimitiveArray;
		if (primArray.size() > nRECount)
		{
			for (auto it = (primArray.begin() + nRECount); it != primArray.end(); ++it)
			{
				delete *it;
			}
		}
		primArray.resize(nRECount, nullptr);

		for (uint32 r = 0; r < nRECount; ++r)
		{
			SHudData& pData = renderDataArray[r];
			CShaderResources* pShaderResources = (CShaderResources*)pData.pShaderResources;

			SEfResTexture* pDiffuse = pData.pDiffuse;
			if (!pShaderResources || !pDiffuse || !pDiffuse->m_Sampler.m_pDynTexSource)
			{
				continue;
			}

			if (primArray[index] == nullptr)
			{
				primArray[index] = new CRenderPrimitive();
			}
			auto& prim = *primArray[index];

			if (SetVertex(prim, pData))
			{
				SRenderObjData* pROData = pData.pRO->GetObjData();

				prim.SetFlags(CRenderPrimitive::eFlags_ReflectShaderConstants);
				prim.SetTechnique(CShaderMan::s_sh3DHUD, hud3d.m_pGeneralTechName, rtMask);
				prim.SetCullMode(eCULL_Back);

				int32 nRenderState = GS_BLSRC_ONE | GS_BLDST_ONEMINUSSRCALPHA;
				if (!pROData || !(pROData->m_nCustomFlags & COB_HUD_REQUIRE_DEPTHTEST))
				{
					nRenderState |= GS_NODEPTHTEST;
				}
				prim.SetRenderState(nRenderState);

				if (pDiffuse && pDiffuse->m_Sampler.m_pTex)
				{
					prim.SetTexture(0, pDiffuse->m_Sampler.m_pTex);
				}
				else
				{
					prim.SetTexture(0, hud3d.m_pHUD_RT);
				}
				prim.SetSampler(0, EDefaultSamplerStates::LinearClamp);

				prim.SetTexture(1, CRendererResources::s_ptexBackBufferScaled[1]);
				prim.SetSampler(1, EDefaultSamplerStates::LinearClamp);

				if (bInterferenceApplied)
				{
					prim.SetTexture(2, hud3d.m_pNoise);
					prim.SetSampler(2, EDefaultSamplerStates::PointWrap);
				}

				prim.SetInlineConstantBuffer(eConstantBufferShaderSlot_PerView, pPerViewCB, EShaderStage_Vertex | EShaderStage_Pixel);
				prim.Compile(pass);

				// update constant buffer
				{
					auto& cm = prim.GetConstantManager();
					cm.BeginNamedConstantUpdate();

					SetShaderParams(context,EShaderStage_Vertex | EShaderStage_Pixel, cm, pData, hud3d);

					// Set additional parameters
					vHudEffectParams[0].x = fCurrentDofBlend;

					// baking constant glow out of shader
					const float fGlowConstScale = CRenderer::CV_r_PostProcessHUD3DGlowAmount; // tweaked constant - before was 2.0, but barely visible due to hud shadow
					vHudEffectParams[0].y = pShaderResources->GetFinalEmittance().Luminance() * hud3d.m_pGlowMul->GetParam() * fGlowConstScale;
					vHudEffectParams[0].z = interferenceStrength;

					if (pROData)
					{
						// Apply filter if ignore flag not set
						if ((pROData->m_nCustomFlags ^ COB_IGNORE_HUD_INTERFERENCE_FILTER) && interferenceStrength > 0.0f)  // without the last check it is possibel that vInterference is NaN
						{
							vHudEffectParams[0].z *= vInterferenceParams.x;
						}

						// Set per plane alpha multipler
						vOverrideColorParams.w = pData.pRO->m_fAlpha;
					}

					// baking out of shader shadow amount const multiplication
					const float fShadowAmountDotConst = 0.33f;
					vHudEffectParams[0].w = CRenderer::CV_r_PostProcessHUD3DShadowAmount * fShadowAmountDotConst;

					if (bInterferenceApplied)
					{
						float vTCScaleX = (float)SHudData::s_nFlashWidthMax /  (float)hud3d.m_pHUD_RT->GetWidth () * 2.0f;
						float vTCScaleY = (float)SHudData::s_nFlashHeightMax / (float)hud3d.m_pHUD_RT->GetHeight() * 2.0f;

						vHudEffectParams[1].x = hud3d.m_interferenceRandNums.x * vTCScaleX;
						vHudEffectParams[1].y = hud3d.m_interferenceRandNums.y * vTCScaleY;
						vHudEffectParams[1].z = 0.0f;
						vHudEffectParams[1].w = hud3d.m_interferenceRandNums.w;
					}

					cm.SetNamedConstantArray(hud3d.m_pHudEffectsParamName, vHudEffectParams, hudEffectParamCount, eHWSC_Pixel);
					cm.SetNamedConstant(hud3d.m_pHudOverrideColorMultParamName, vOverrideColorParams, eHWSC_Pixel);

					cm.EndNamedConstantUpdate(&viewport); // Unmap constant buffers and mark as bound
				}

				pass.AddPrimitive(&prim);

				++index;
			}
		}

		pass.Execute();
	}
}

bool CHud3DPass::SetVertex(CRenderPrimitive& prim, struct SHudData& pData) const
{
	if (pData.pRE /* && pData.pRE->mfGetType() == eDATA_Mesh*/)
	{
		const auto* pREMesh = static_cast<const CREMesh*>(pData.pRE);

		auto* pRenderMesh = const_cast<CRenderMesh*>(pREMesh->m_pRenderMesh);
		if (pRenderMesh)
		{
			pRenderMesh->RT_CheckUpdate(pRenderMesh->_GetVertexContainer(), pRenderMesh->_GetVertexFormat(), 0);

			buffer_handle_t hVertexStream = pRenderMesh->_GetVBStream(VSF_GENERAL);
			buffer_handle_t hIndexStream = pRenderMesh->_GetIBStream();

			if (hVertexStream != ~0u && hIndexStream != ~0u)
			{
				const auto primType = pRenderMesh->_GetPrimitiveType();
				const auto numIndex = pRenderMesh->_GetNumInds();

				prim.SetCustomVertexStream(hVertexStream, pRenderMesh->_GetVertexFormat(), pRenderMesh->GetStreamStride(VSF_GENERAL));
				prim.SetCustomIndexStream(hIndexStream, (sizeof(vtx_idx) == 2 ? Index16 : Index32));
				prim.SetDrawInfo(primType, 0, 0, numIndex);

				return true;
			}
		}
	}

	return false;
}

void CHud3DPass::SetShaderParams(
	const CPostEffectContext &context,
  EShaderStage shaderStages,
  CRenderPrimitive::ConstantManager& constantManager,
  const struct SHudData& data,
  const class CHud3D& hud3d) const
{
	CShaderResources* pShaderResources = (CShaderResources*)data.pShaderResources;
	float fOpacity = pShaderResources->GetStrengthValue(EFTT_OPACITY) * hud3d.m_pOpacityMul->GetParam();

	const CRenderObject* pRO = data.pRO;
	Matrix44A mObjCurr, mViewProj;
	mObjCurr = pRO->GetMatrix(gcpRendD3D->GetObjectAccessorThreadConfig());
	mObjCurr.Transpose();

	// Render in camera space to remove precision bugs
	const bool bCameraSpace = (pRO->m_ObjFlags & FOB_NEAREST) ? true : false;

	Matrix44A mView = context.GetRenderView()->GetViewInfo(CCamera::eEye_Left).viewMatrix;
	if (bCameraSpace)
	{
		mView.m30 = 0.0f;
		mView.m31 = 0.0f;
		mView.m32 = 0.0f;
	}

	mViewProj = mView * hud3d.m_mProj;
	mViewProj = mObjCurr * mViewProj;
	mViewProj.Transpose();

	constantManager.SetNamedConstantArray(hud3d.m_pMatViewProjParamName, (Vec4*)mViewProj.GetData(), 4, eHWSC_Vertex);

	// Since we can have multiple flash files with different resolutions we need to update resolution for correct mapping
	Vec4 vHudTexCoordParams = Vec4(min(1.0f, (float)data.nFlashWidth / (float)hud3d.m_pHUD_RT->GetWidth()),
	                               min(1.0f, (float)data.nFlashHeight / (float)hud3d.m_pHUD_RT->GetHeight()),
	                               max(1.0f, (float)hud3d.m_pHUD_RT->GetWidth() / (float)data.s_nFlashWidthMax),
	                               max(1.0f, (float)hud3d.m_pHUD_RT->GetHeight() / (float)data.s_nFlashHeightMax));

	if (shaderStages & EShaderStage_Vertex)
	{
		constantManager.SetNamedConstant(hud3d.m_pHudTexCoordParamName, vHudTexCoordParams, eHWSC_Vertex);
	}

	if (shaderStages & EShaderStage_Pixel)
	{
		constantManager.SetNamedConstant(hud3d.m_pHudTexCoordParamName, vHudTexCoordParams, eHWSC_Pixel);

		ColorF cDiffuse = pShaderResources->GetColorValue(EFTT_DIFFUSE);

		cDiffuse *= fOpacity; // pre-multiply alpha in all cases
		if (pShaderResources->m_ResFlags & MTL_FLAG_ADDITIVE)
		{
			fOpacity = 0.0f;
		}

		Vec4 vHudParams = Vec4(cDiffuse.r, cDiffuse.g, cDiffuse.b, fOpacity) * hud3d.m_pHudColor->GetParamVec4();
		constantManager.SetNamedConstant(hud3d.m_pHudParamName, vHudParams, eHWSC_Pixel);
	}
}
