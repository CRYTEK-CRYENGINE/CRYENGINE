// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

/*=============================================================================
   PostEffects.cpp : Post processing effects implementation

   Revision history:
* Created by Tiago Sousa

   =============================================================================*/

#include "StdAfx.h"
#include <Cry3DEngine/I3DEngine.h>
#include "PostEffects.h"
#include "PostProcessUtils.h"

#include <DriverD3D.h>

////////////////////////////////////////////////////////////////////////////////////////////////////
// Engine specific post-effects
////////////////////////////////////////////////////////////////////////////////////////////////////

int CMotionBlur::Initialize()
{
	return 1;
}

int CMotionBlur::CreateResources()
{
	SAFE_RELEASE(m_pBokehShape);
	m_pBokehShape = CTexture::ForName("%ENGINE%/EngineAssets/ScreenSpace/bokeh_pentagon.dds", FT_DONT_STREAM, eTF_Unknown);

	return 1;
}

void CMotionBlur::Release()
{
	m_pOMBData[0].clear();
	m_pOMBData[1].clear();
	m_pOMBData[2].clear();
	SAFE_RELEASE(m_pBokehShape);
}

void CMotionBlur::Reset(bool bOnSpecChange)
{
	m_pActive->ResetParam(0.0f);

	m_pAmount->ResetParam(0.5f);

	m_pExposureTime->ResetParam(0.004f);
	m_pVectorsScale->ResetParam(1.5f);
	m_pRadialBlurMaskUVScaleX->ResetParam(1.0f);
	m_pRadialBlurMaskUVScaleY->ResetParam(1.0f);

	m_pDrawNearZRangeOverride->ResetParam(0.0f);

	m_pRadialBlurMaskTex->Release();
	m_pDirectionalBlurVec->ResetParamVec4(Vec4(0, 0, 0, 0));
}

////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////

int CDepthOfField::CreateResources()
{
	SAFE_RELEASE(m_pNoise);

	m_pNoise = CTexture::ForName("%ENGINE%/EngineAssets/Textures/vector_noise.dds", FT_DONT_STREAM, eTF_Unknown);

	return true;
}

void CDepthOfField::Release()
{
	SAFE_RELEASE(m_pNoise);
}

void CDepthOfField::Reset(bool bOnSpecChange)
{
	m_pUseMask->ResetParam(0.0f);
	m_pFocusDistance->ResetParam(3.5f);
	m_pFocusRange->ResetParam(0.0f);
	m_pMaxCoC->ResetParam(12.0f);
	m_pCenterWeight->ResetParam(1.0f);
	m_pBlurAmount->ResetParam(1.0f);
	m_pFocusMin->ResetParam(2.0f);
	m_pFocusMax->ResetParam(10.0f);
	m_pFocusLimit->ResetParam(100.0f);
	m_pUseMask->ResetParam(0.0f);
	m_pMaskTex->Release();

	m_pMaskedBlurAmount->ResetParam(0.0f);
	m_pMaskedBlurMaskTex->Release();

	m_pBokehMaskTex->Release();
	m_pDebug->ResetParam(0.0f);
	m_pActive->ResetParam(0.0f);

	m_pUserActive->ResetParam(0.0f);
	m_pUserFocusDistance->ResetParam(3.5f);
	m_pUserFocusRange->ResetParam(5.0f);
	m_pUserBlurAmount->ResetParam(1.0f);

	m_pFocusMinZ->ResetParam(0.0f);
	m_pFocusMinZScale->ResetParam(0.0f);

	m_fUserFocusRangeCurr = 0;
	m_fUserFocusDistanceCurr = 0;
	m_fUserBlurAmountCurr = 0;
}

bool CDepthOfField::Preprocess(const SRenderViewInfo& viewInfo)
{
	// Skip LDR processing, DOF is always performed in HDR
	return false;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////

int CSunShafts::Initialize()
{
	Release();

	m_pOcclQuery[0] = new COcclusionQuery;
	m_pOcclQuery[0]->Create();
	m_pOcclQuery[1] = new COcclusionQuery;
	m_pOcclQuery[1]->Create();

	return true;
}

void CSunShafts::Release()
{
	SAFE_DELETE(m_pOcclQuery[0]);
	SAFE_DELETE(m_pOcclQuery[1]);
}

void CSunShafts::Reset(bool bOnSpecChange)
{
}

void CSunShafts::OnLostDevice()
{
	Release();
}

////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////

bool CSharpening::Preprocess(const SRenderViewInfo& viewInfo)
{
	bool bQualityCheck = CPostEffectsMgr::CheckPostProcessQuality(eRQ_Medium, eSQ_Medium);

	if (!bQualityCheck)
		return false;

	if (!CRenderer::CV_r_PostProcessFilters)
		return false;

	if (fabs(m_pAmount->GetParam() - 1.0f) + CRenderer::CV_r_Sharpening + CRenderer::CV_r_ChromaticAberration > 0.09f)
	{
		return true;
	}

	return false;
}

void CSharpening::Reset(bool bOnSpecChange)
{
	m_pAmount->ResetParam(1.0f);
	m_pType->ResetParam(0.0f);
}

////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////

bool CBlurring::Preprocess(const SRenderViewInfo& viewInfo)
{
	bool bQualityCheck = CPostEffectsMgr::CheckPostProcessQuality(eRQ_Medium, eSQ_Medium);

	if (!bQualityCheck)
		return false;

	if (!CRenderer::CV_r_PostProcessFilters)
		return false;

	if (m_pAmount->GetParam() > 0.09f)
	{
		return true;
	}

	return false;
}

void CBlurring::Reset(bool bOnSpecChange)
{
	m_pAmount->ResetParam(0.0f);
	m_pType->ResetParam(0.0f);
}

////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////

bool CUberGamePostProcess::Preprocess(const SRenderViewInfo& viewInfo)
{
	m_nCurrPostEffectsMask = 0;

	const float fParamThreshold = 1.0f / 255.0f;
	const Vec4 vWhite = Vec4(1.0f, 1.0f, 1.0f, 1.0f);

	bool bEnable = false;
	bEnable |= m_pColorTint->GetParamVec4() != vWhite;
	bEnable |= m_pNoise->GetParam() > fParamThreshold;
	bEnable |= m_pSyncWaveAmplitude->GetParam() > fParamThreshold;
	bEnable |= m_pGrainAmount->GetParam() > fParamThreshold;
	bEnable |= m_pPixelationScale->GetParam() > fParamThreshold;

	if (m_pInterlationAmount->GetParam() > fParamThreshold || m_pVSyncAmount->GetParam() > fParamThreshold)
	{
		m_nCurrPostEffectsMask |= ePE_SyncArtifacts;
		bEnable = true;
	}

	// todo: looks like some game code/flowgraph doing silly stuff - investigate
	const float fParamThresholdBackCompatibility = 0.09f;

	if (m_pChromaShiftAmount->GetParam() > fParamThresholdBackCompatibility || m_pFilterChromaShiftAmount->GetParam() > fParamThresholdBackCompatibility)
	{
		m_nCurrPostEffectsMask |= ePE_ChromaShift;
		bEnable = true;
	}

	return bEnable;
}

void CUberGamePostProcess::Reset(bool bOnSpecChange)
{
	m_nCurrPostEffectsMask = 0;

	m_pVSyncAmount->ResetParam(0.0f);
	m_pVSyncFreq->ResetParam(1.0f);

	const Vec4 vWhite = Vec4(1.0f, 1.0f, 1.0f, 1.0f);
	m_pColorTint->ResetParamVec4(vWhite);

	m_pInterlationAmount->ResetParam(0.0f);
	m_pInterlationTilling->ResetParam(1.0f);
	m_pInterlationRotation->ResetParam(0.0f);

	m_pPixelationScale->ResetParam(0.0f);
	m_pNoise->ResetParam(0.0f);

	m_pSyncWaveFreq->ResetParam(0.0f);
	m_pSyncWavePhase->ResetParam(0.0f);
	m_pSyncWaveAmplitude->ResetParam(0.0f);

	m_pFilterChromaShiftAmount->ResetParam(0.0f);
	m_pChromaShiftAmount->ResetParam(0.0f);

	m_pGrainAmount->ResetParam(0.0f);
	m_pGrainTile->ResetParam(1.0f);

	m_pMask->Release();
}

////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////

bool CColorGrading::Preprocess(const SRenderViewInfo& viewInfo)
{
	// Depreceated: to be removed / replaced by UberPostProcess shader
	return false;
}

void CColorGrading::Reset(bool bOnSpecChange)
{
	// reset user params
	m_pSaturationOffset->ResetParam(0.0f);
	m_pPhotoFilterColorOffset->ResetParamVec4(Vec4(0.0f, 0.0f, 0.0f, 0.0f));
	m_pPhotoFilterColorDensityOffset->ResetParam(0.0f);
	m_pGrainAmountOffset->ResetParam(0.0f);
}

////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////

bool CUnderwaterGodRays::Preprocess(const SRenderViewInfo& viewInfo)
{
	bool bQualityCheck = CPostEffectsMgr::CheckPostProcessQuality(eRQ_Medium, eSQ_Medium);
	if (!bQualityCheck)
		return false;

	static ICVar* pVar = iConsole->GetCVar("e_WaterOcean");

	//bool bOceanVolumeVisible = (gEnv->p3DEngine->GetOceanRenderFlags() & OCR_OCEANVOLUME_VISIBLE) != 0;

	if (CRenderer::CV_r_water_godrays && m_pAmount->GetParam() > 0.005f) // && bOceanEnabled && bOceanVolumeVisible)
	{
		float fWatLevel = SPostEffectsUtils::m_fWaterLevel;
		if (fWatLevel - 0.1f > viewInfo.cameraOrigin.z)
		{
			// check water level

			return true;
		}
	}

	return false;
}

void CUnderwaterGodRays::Reset(bool bOnSpecChange)
{
	m_pAmount->ResetParam(1.0f);
	m_pQuality->ResetParam(1.0f);
}

////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////

bool CVolumetricScattering::Preprocess(const SRenderViewInfo& viewInfo)
{
	bool bQualityCheck = CPostEffectsMgr::CheckPostProcessQuality(eRQ_High, eSQ_High);
	if (!bQualityCheck)
		return false;

	if (!CRenderer::CV_r_PostProcessGameFx)
		return false;

	if (m_pAmount->GetParam() > 0.005f)
	{
		return true;
	}

	return false;
}

void CVolumetricScattering::Reset(bool bOnSpecChange)
{
	m_pAmount->ResetParam(0.0f);
	m_pType->ResetParam(0.0f);
	m_pQuality->ResetParam(1.0f);
	m_pTilling->ResetParam(1.0f);
	m_pSpeed->ResetParam(1.0f);
	m_pColor->ResetParamVec4(Vec4(0.5f, 0.75f, 1.0f, 1.0f));
}

////////////////////////////////////////////////////////////////////////////////////////////////////
// Game/Hud specific post-effects
////////////////////////////////////////////////////////////////////////////////////////////////////

void CAlienInterference::Reset(bool bOnSpecChange)
{
	m_pAmount->ResetParam(0.0f);
	m_pTintColor->ResetParamVec4(Vec4(Vec3(0.85f, 0.95f, 1.25f) * 0.5f, 1.0f));
}

bool CAlienInterference::Preprocess(const SRenderViewInfo& viewInfo)
{
	bool bQualityCheck = CPostEffectsMgr::CheckPostProcessQuality(eRQ_Medium, eSQ_Medium);
	if (!bQualityCheck)
		return false;

	if (!CRenderer::CV_r_PostProcessGameFx)
		return false;

	if (m_pAmount->GetParam() > 0.09f)
	{
		return true;
	}

	return false;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CWaterDroplets::Preprocess(const SRenderViewInfo& viewInfo)
{
	bool bQualityCheck = CPostEffectsMgr::CheckPostProcessQuality(eRQ_Medium, eSQ_Medium);
	if (!bQualityCheck)
		return false;

	const bool bUserActive = m_pAmount->GetParam() > 0.005f;

	if (CRenderer::CV_r_water_godrays)
	{
		return bUserActive; // user enabled override
	}

	return false;
}

void CWaterDroplets::Reset(bool bOnSpecChange)
{
	m_pAmount->ResetParam(0.0f);
}

////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////

bool CWaterFlow::Preprocess(const SRenderViewInfo& viewInfo)
{
	bool bQualityCheck = CPostEffectsMgr::CheckPostProcessQuality(eRQ_Medium, eSQ_Medium);
	if (!bQualityCheck)
		return false;

	if (!CRenderer::CV_r_PostProcessGameFx)
		return false;

	if (m_pAmount->GetParam() > 0.005f)
		return true;

	return false;
}

void CWaterFlow::Reset(bool bOnSpecChange)
{
	m_pAmount->ResetParam(0.0f);
}

////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////

bool CScreenFrost::Preprocess(const SRenderViewInfo& viewInfo)
{
	bool bQualityCheck = CPostEffectsMgr::CheckPostProcessQuality(eRQ_Medium, eSQ_Medium);
	if (!bQualityCheck)
		return false;

	if (!CRenderer::CV_r_PostProcessGameFx)
		return false;

	if (m_pAmount->GetParam() > 0.09f)
	{
		return true;
	}

	return false;
}

void CScreenFrost::Reset(bool bOnSpecChange)
{
	m_pAmount->ResetParam(0.0f);
	m_pCenterAmount->ResetParam(1.0f);
	m_fRandOffset = 0;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////

int CRainDrops::CreateResources()
{
	Release();

	//create texture for HitEffect accumulation
	m_bFirstFrame = true;

	// Already generated ? No need to proceed
	if (!m_pDropsLst.empty())
	{
		return 1;
	}

	m_pDropsLst.reserve(m_nMaxDropsCount);
	for (int p = 0; p < m_nMaxDropsCount; p++)
	{
		SRainDrop* pDrop = new SRainDrop;
		m_pDropsLst.push_back(pDrop);
	}

	return 1;
}

void CRainDrops::Release()
{
	if (m_pDropsLst.empty())
	{
		return;
	}

	SRainDropsItor pItor, pItorEnd = m_pDropsLst.end();
	for (pItor = m_pDropsLst.begin(); pItor != pItorEnd; ++pItor)
	{
		SAFE_DELETE((*pItor));
	}
	m_pDropsLst.clear();
}

void CRainDrops::Reset(bool bOnSpecChange)
{
	m_bFirstFrame = true;
	m_uCurrentDytex = 0;

	m_pAmount->ResetParam(0.0f);
	m_pSpawnTimeDistance->ResetParam(0.35f);
	m_pSize->ResetParam(5.0f);
	m_pSizeVar->ResetParam(2.5f);
	m_nAliveDrops = 0;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////

int CNightVision::CreateResources()
{
	SAFE_RELEASE(m_pGradient);
	SAFE_RELEASE(m_pNoise);

	m_pGradient = CTexture::ForName("%ENGINE%/EngineAssets/Textures/nightvis_grad.dds", FT_DONT_STREAM, eTF_Unknown);
	m_pNoise = CTexture::ForName("%ENGINE%/EngineAssets/Textures/vector_noise.dds", FT_DONT_STREAM, eTF_Unknown);

	return true;
}

void CNightVision::Release()
{
	SAFE_RELEASE(m_pGradient);
	SAFE_RELEASE(m_pNoise);
}

void CNightVision::Reset(bool bOnSpecChange)
{
	if (!bOnSpecChange)
	{
		m_pActive->ResetParam(0.0f);
		m_pAmount->ResetParam(1.0f);

		m_bWasActive = false;
		m_fActiveTime = 0.0f;
		m_fRandOffset = 0;
		m_fPrevEyeAdaptionSpeed = 0.25f;
		m_fPrevEyeAdaptionBase = 100.0f;
	}
}

////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////

int CSonarVision::CreateResources()
{
	SAFE_RELEASE(m_pGradient);
	SAFE_RELEASE(m_pNoise);

	m_pGradient = CTexture::ForName("%ENGINE%/EngineAssets/Shading/SonarVisionGradient.tif", FT_DONT_STREAM, eTF_Unknown);
	m_pNoise = CTexture::ForName("%ENGINE%/EngineAssets/Textures/vector_noise.dds", FT_DONT_STREAM, eTF_Unknown);

	return true;
}

void CSonarVision::Release()
{
	SAFE_RELEASE(m_pGradient);
	SAFE_RELEASE(m_pNoise);
}

void CSonarVision::Reset(bool bOnSpecChange)
{
	m_pActive->ResetParam(0.0f);
	m_pAmount->ResetParam(1.0f);
}

////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////

void CThermalVision::Release()
{
	SAFE_RELEASE(m_pGradient);
	SAFE_RELEASE(m_pNoise);
}

int CThermalVision::CreateResources()
{
	Release();

	m_pGradient = CTexture::ForName("%ENGINE%/EngineAssets/Shading/ThermalVisionGradient.tif", FT_DONT_STREAM, eTF_Unknown);
	m_pNoise = CTexture::ForName("%ENGINE%/EngineAssets/Textures/vector_noise.dds", FT_DONT_STREAM, eTF_Unknown);

	return true;
}

void CThermalVision::Reset(bool bOnSpecChange)
{
	if (!bOnSpecChange)
	{
		m_pActive->ResetParam(0.0f);
		m_pRenderOffscreen->ResetParam(0.0f);
		m_pAmount->ResetParam(1.0f);
		m_pCamMovNoiseAmount->ResetParam(0.0f);
		m_fBlending = 0.0f;
		m_bRenderOffscreen = false;
	}
	m_bInit = true;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////

void CHudSilhouettes::Reset(bool bOnSpecChange)
{
	m_pActive->ResetParam(0.0f);
	m_pAmount->ResetParam(1.0f);
	m_pType->ResetParam(1);

	FindIfSilhouettesOptimisedTechAvailable();
}

bool CHudSilhouettes::Preprocess(const SRenderViewInfo& viewInfo)
{
	if ((CRenderer::CV_r_customvisions != 3) || (m_bSilhouettesOptimisedTechAvailable))
	{
		if (!CRenderer::CV_r_PostProcessGameFx ||
		    !CRenderer::CV_r_customvisions ||
		    gRenDev->IsCustomRenderModeEnabled(eRMF_MASK) ||
		    gRenDev->IsPost3DRendererEnabled())
		{
			return false;
		}

		CRenderView *pRenderView = gcpRendD3D->GetGraphicsPipeline().GetCurrentRenderView();
		// no need to proceed
		float fType = m_pType->GetParam();
		uint32 nBatchMask = pRenderView->GetBatchFlags(EFSLIST_GENERAL) | pRenderView->GetBatchFlags(EFSLIST_TRANSP_BW) | pRenderView->GetBatchFlags(EFSLIST_TRANSP_AW);

		if ((!(nBatchMask & FB_CUSTOM_RENDER)) && fType == 1.0f)
		{
			return false;
		}

		if (m_pAmount->GetParam() > 0.005f)
		{
			return true;
		}
	}

	return false;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////

void CFlashBang::Release()
{
	SAFE_DELETE(m_pGhostImage);
}

void CFlashBang::Reset(bool bOnSpecChange)
{
	SAFE_DELETE(m_pGhostImage);
	m_pActive->ResetParam(0.0f);
	m_pTime->ResetParam(2.0f);
	m_pDifractionAmount->ResetParam(1.0f);
	m_pBlindAmount->ResetParam(0.5f);
	m_fSpawnTime = 0.0f;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////

void CPostStereo::Reset(bool bOnSpecChange)
{
}
////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////

bool CImageGhosting::Preprocess(const SRenderViewInfo& viewInfo)
{
	CTexture* pPrevFrame = CRendererResources::s_ptexPrevFrameScaled;
	if (!pPrevFrame)
	{
		m_bInit = true;
		return false;
	}

	if (m_pAmount->GetParam() > 0.09f)
		return true;

	m_bInit = true;

	return false;
}

void CImageGhosting::Reset(bool bOnSpecChange)
{
	m_bInit = true;
	m_pAmount->ResetParam(0.0f);
}

////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////

int CFilterKillCamera::Initialize()
{
	m_techName = "KillCameraFilter";
	m_paramName = "psParams";
	return 1;
}

bool CFilterKillCamera::Preprocess(const SRenderViewInfo& viewInfo)
{
	if (!CRenderer::CV_r_PostProcessFilters)
		return false;

	if (m_pActive->GetParam() > 0.0f)
	{
		const int mode = int_round(m_pMode->GetParam());
		if (mode != m_lastMode)
		{
			m_blindTimer = 0.0f;
			m_lastMode = mode;
		}

		// Update time
		float frameTime = gEnv->pTimer->GetFrameTime();
		m_blindTimer += frameTime;

		return true;
	}

	m_blindTimer = 0.0f;

	return false;
}

void CFilterKillCamera::Reset(bool bOnSpecChange)
{
	// Game controls parameters + reset (removed from here due to a race condition).
}

////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////

int CNanoGlass::CreateResources()
{
	Release();
	m_pHexOutline = CTexture::ForName("%ENGINE%/EngineAssets/Textures/hex.dds", FT_DONT_STREAM, eTF_Unknown);
	m_pHexRand = CTexture::ForName("%ENGINE%/EngineAssets/Textures/hex_rand.dds", FT_DONT_STREAM, eTF_Unknown);
	m_pHexGrad = CTexture::ForName("%ENGINE%/EngineAssets/Textures/hex_grad.dds", FT_DONT_STREAM, eTF_Unknown);
	m_pNoise = CTexture::ForName("%ENGINE%/EngineAssets/Textures/perlinNoise_sum_small.dds", FT_DONT_STREAM, eTF_Unknown);
	return 1;
}

void CNanoGlass::Release()
{
	SAFE_RELEASE(m_pHexOutline);
	SAFE_RELEASE(m_pHexRand);
	SAFE_RELEASE(m_pHexGrad);
	SAFE_RELEASE(m_pNoise);
}

bool CNanoGlass::Preprocess(const SRenderViewInfo& viewInfo)
{
	//////////////////////////////////////////////////////////////////////////
	// Logic to determine if we can enable the effect. As m_pActive is not double buffered it's likely that this is
	// activated by the main thread while the render thread is not yet finished processing the current frame. It'll
	// result in 1 frame flickering which is hard to debug. Proper fix would be to double buffer all params correctly,
	// i.e. either double buffer effect param impls internally or create multiple instances when filling m_pNameIdMap
	// in CPostEffectsMgr::Init(). At the time of writing this is not possible as C3 is near completion and game code
	// is based on the single thread, state like behavior of effect params.
	// Note: This code assumes that the effect is turned off before ending a level!
	static bool previouslyEnabled = false;
	const bool enabled = m_pActive->GetParam() > 0.0f;
	const bool bail = enabled != previouslyEnabled;
	previouslyEnabled = enabled;
	if (bail)
		return false;
	//////////////////////////////////////////////////////////////////////////

	if (!CRenderer::CV_r_PostProcessFilters)
		return false;

	if (m_pActive->GetParam() > 0.0f)
	{
		// Update back buffer when the brightness scalar is enabled
		const float backBufferBrightessScalar = m_pFilter->GetParamVec4().x;
		m_nRenderFlags = (backBufferBrightessScalar > 0.0f) ? PSP_UPDATE_BACKBUFFER : 0;

		return true;
	}

	m_noiseTime = 0.0f;
	m_time = 0.0f;
	return false;
}

void CNanoGlass::Reset(bool bOnSpecChange)
{
	// The post effect manager now gets reset at the start of the game - seems to be because the shader quality
	// gets changed between menus and game.
	// This reset gets called after this post effect is turned on, so cannot use this as a reliable reset,
	// will have to rely on game code to control this effect and reset it. All these values do get set in the
	// constructor. This post effect is recreated at the start of each session, so the constructor gets called then
}

void CNanoGlass::AddRE(const CRenderElement* pRE, const SShaderItem* pShaderItem, CRenderObject* pObj, const SRenderingPassInfo& passInfo)
{
	// Main thread
	const uint32 nThreadID = passInfo.ThreadID();
	// Only add valid render elements
	if (pRE && pObj)
	{
		m_pRenderData[nThreadID].pRenderElement = pRE;
		m_pRenderData[nThreadID].pRenderObject = pObj;
	}
}

////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////

void CScreenBlood::Reset(bool bOnSpecChange)
{
	m_pAmount->ResetParam(0.0f);
	m_pBorder->ResetParamVec4(Vec4(0.0f, 0.0f, 2.0f, 1.0f)); // Border: x=xOffset y=yOffset z=range w=alpha
}

bool CScreenBlood::Preprocess(const SRenderViewInfo& viewInfo)
{
	return (CRenderer::CV_r_PostProcessGameFx && m_pAmount->GetParam() > 0.005f);
}

////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////

void CScreenFader::Reset(bool bOnSpecChange)
{
	m_pColor->ResetParamVec4(Vec4(0.0f, 0.0f, 0.0f, 0.0f));
}

bool CScreenFader::Preprocess(const SRenderViewInfo& viewInfo)
{
	return (CRenderer::CV_r_PostProcessGameFx && m_pColor->GetParamVec4().w > 0.001f);
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

bool CPost3DRenderer::Preprocess(const SRenderViewInfo& viewInfo)
{
	if (IsActive())
	{
		// Defer turning off post effect for 5 frames - sometimes the flash is left rendering on the screen
		// for a frame, if we don't render the post effect for this frame then junk will be rendered into
		// the flash. Currently the post effect is disabled at the latest point in menu code, thus this is the
		// simplest/safest fix.
		m_deferDisableFrameCountDown = 5;
	}
	else if (m_deferDisableFrameCountDown > 0)
	{
		m_deferDisableFrameCountDown--;
	}

	const bool bRender = (m_deferDisableFrameCountDown > 0) ? true : false;
	return bRender;
}

void CPost3DRenderer::Reset(bool bOnSpecChange)
{
	// Let game code fully control its active status, otherwise in some situations
	// the post effect system will get reset between menus and game and thus this
	// will get turned off when undesired
}

bool CPost3DRenderer::HasModelsToRender() const
{
	CRenderView* pRenderView = gcpRendD3D->GetGraphicsPipeline().GetCurrentRenderView();
	const uint32 batchMask = pRenderView->GetBatchFlags(EFSLIST_GENERAL)
		| pRenderView->GetBatchFlags(EFSLIST_SKIN)
		| pRenderView->GetBatchFlags(EFSLIST_DECAL)
		| pRenderView->GetBatchFlags(EFSLIST_TRANSP_BW)
		| pRenderView->GetBatchFlags(EFSLIST_TRANSP_AW);
	return (batchMask & FB_POST_3D_RENDER) ? true : false;
}

//////////////////////////////////////////////////////////////////////////
void CMotionBlur::SetupObject(CRenderObject* pObj, const SRenderingPassInfo& passInfo)
{
	assert(pObj);

	uint32 nThreadID = passInfo.ThreadID();

	if (passInfo.IsRecursivePass())
		return;

	SRenderObjData* const __restrict pOD = pObj->GetObjData();
	if (!pOD)
	{
		return;
	}

	pObj->m_ObjFlags &= ~FOB_HAS_PREVMATRIX;

	// don't apply regular object motion blur to skinned objects with bending (foliage)
	// they get their motion blur in the DrawSkinned Pass
	if (pOD->m_pSkinningData && pOD->m_pSkinningData->pAsyncJobs == NULL)
		return;

	if (pOD->m_pSkinningData)
	{
		assert(pOD->m_pSkinningData->pPreviousSkinningRenderData);

		if (pOD->m_pSkinningData->pAsyncJobs)
		{
			gEnv->pJobManager->WaitForJob(*pOD->m_pSkinningData->pAsyncJobs);
		}

		if (pOD->m_pSkinningData->pPreviousSkinningRenderData->pAsyncJobs)
		{
			gEnv->pJobManager->WaitForJob(*pOD->m_pSkinningData->pPreviousSkinningRenderData->pAsyncJobs);
		}
	}

	if (pOD->m_uniqueObjectId != 0 && pObj->m_fDistance < CRenderer::CV_r_MotionBlurMaxViewDist)
	{
		const uint32 nFrameID = passInfo.GetMainFrameID();
		const uintptr_t ObjID = pOD ? pOD->m_uniqueObjectId : 0;
		const uint32 nObjFrameWriteID = (nFrameID) % 3;
		OMBParamsMapItor it = m_pOMBData[nObjFrameWriteID].find(ObjID);
		if (it != m_pOMBData[nObjFrameWriteID].end())
		{
			// if all good, get previous buffered frame
			const uint32 nObjPrevFrameID = (nFrameID - 1) % 3;
			OMBParamsMapItor itPrev = m_pOMBData[nObjPrevFrameID].find(ObjID);

			if (itPrev != m_pOMBData[nObjPrevFrameID].end())
			{
				SObjMotionBlurParams* pWriteObjMBData = &it->second;
				SObjMotionBlurParams* pPrevObjMBData = &itPrev->second;
				pWriteObjMBData->mObjToWorld = pObj->GetMatrix(passInfo);

				const float fThreshold = CRenderer::CV_r_MotionBlurThreshold;
				if (pObj->m_ObjFlags & (FOB_NEAREST | FOB_MOTION_BLUR) || !Matrix34::IsEquivalent(pPrevObjMBData->mObjToWorld, pWriteObjMBData->mObjToWorld, fThreshold))
					pObj->m_ObjFlags |= FOB_HAS_PREVMATRIX;

				pWriteObjMBData->nFrameUpdateID = nFrameID;
				pWriteObjMBData->pRenderObj = pObj;

				return;
			}
		}

		m_FillData[nThreadID].push_back(OMBParamsMap::value_type(ObjID, SObjMotionBlurParams(pObj, pObj->GetMatrix(passInfo), nFrameID)));
	}
}
//////////////////////////////////////////////////////////////////////////
