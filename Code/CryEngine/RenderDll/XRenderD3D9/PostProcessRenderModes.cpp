// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

/*=============================================================================
   PostProcessRenderModes : custom render modes post processing

   Revision history:
* 23/02/2005: Re-factored/Converted to CryEngine 2.0 by Tiago Sousa
* Created by Tiago Sousa

* notes:
*		- all render modes are mutually exclusive (only 1 at a time)
*		- optimization/beautification/clean up iteration still to be done
   =============================================================================*/

#include "StdAfx.h"
#include "DriverD3D.h"
#include <Cry3DEngine/I3DEngine.h>
#include "D3DPostProcess.h"
#include <CryAudio/IAudioSystem.h>

#pragma warning(push)
#pragma warning(disable: 4244)

/////////////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////////////////

bool CNightVision::Preprocess(const SRenderViewInfo& viewInfo)
{
	if (gRenDev->IsCustomRenderModeEnabled(eRMF_NIGHTVISION))
		return false;

	// LDR Nightvision - depreceated/kept for backwards compatibility
	m_bWasActive = IsActive();
	if ((IsActive() || m_fActiveTime) && m_pGradient && m_pNoise && CRenderer::CV_r_NightVision == 1)
		return true;

	return false;
}

void CNightVision::Render()
{
	ASSERT_LEGACY_PIPELINE
/*
	gRenDev->m_cEF.mfRefreshSystemShader("PostEffectsRenderModes", CShaderMan::s_shPostEffectsRenderModes);

	m_fActiveTime += ((m_bWasActive) ? 4.0f : -4.0f) * gEnv->pTimer->GetFrameTime();
	m_fActiveTime = clamp_tpl<float>(m_fActiveTime, 0.0f, 1.0f);

	Vec4 vParamsPS = Vec4(1, 1, 0, m_fActiveTime);
	Vec4 vParamsVS = Vec4(cry_random(0, 1023), cry_random(0, 1023), cry_random(0, 1023), cry_random(0, 1023));

	uint64 nFlagsShader_RT = gRenDev->m_RP.m_FlagsShader_RT;

	PostProcessUtils().StretchRect(CRendererResources::s_ptexBackBuffer, CRendererResources::s_ptexBackBufferScaled[1]);
	PostProcessUtils().TexBlurGaussian(CRendererResources::s_ptexBackBufferScaled[1], 1, 1.25f, 1.5f, false, 0, false, CRendererResources::s_ptexBackBufferScaledTemp[1]);

	static CCryNameTSCRC pTechName("NightVision");
	PostProcessUtils().ShBeginPass(CShaderMan::s_shPostEffectsRenderModes, pTechName, FEF_DONTSETTEXTURES | FEF_DONTSETSTATES);

	gcpRendD3D->FX_SetState(GS_NODEPTHTEST);

	static CCryNameR pParamNameVS("nightVisionParamsVS");
	static CCryNameR pParamNamePS("nightVisionParamsPS");
	CShaderMan::s_shPostEffectsRenderModes->FXSetVSFloat(pParamNameVS, &vParamsVS, 1);
	CShaderMan::s_shPostEffectsRenderModes->FXSetPSFloat(pParamNamePS, &vParamsPS, 1);

	PostProcessUtils().SetTexture(CRendererResources::s_ptexBackBuffer, 0, FILTER_POINT);
	PostProcessUtils().SetTexture(CRendererResources::s_ptexBackBufferScaled[1], 1, FILTER_LINEAR);
	PostProcessUtils().SetTexture(m_pNoise, 2, FILTER_POINT, eSamplerAddressMode_Wrap);
	PostProcessUtils().SetTexture(m_pGradient, 3, FILTER_LINEAR);

	if (CRenderer::CV_r_HDRRendering)
	{
		if (!gRenDev->m_CurViewportID)
			PostProcessUtils().SetTexture(CRendererResources::s_ptexCurLumTexture, 4, FILTER_POINT);
		else
			PostProcessUtils().SetTexture(CRendererResources::s_ptexHDRToneMaps[0], 4, FILTER_POINT);
	}

	PostProcessUtils().DrawFullScreenTri(CRendererResources::s_ptexBackBuffer->GetWidth(), CRendererResources::s_ptexBackBuffer->GetHeight());
	PostProcessUtils().ShEndPass();

	gRenDev->m_RP.m_FlagsShader_RT = nFlagsShader_RT;

	m_bWasActive = true;
*/
}

/////////////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////////////////

struct SSonarHint
{
	SSonarHint() : vPos(ZERO), fTimeStarted(0.0f), fLifetime(0.0f)
	{
	}

	SSonarHint(Vec3 _vPos, float _fTimeStarted, float _fLifetime) : vPos(_vPos), fTimeStarted(_fTimeStarted), fLifetime(_fLifetime)
	{
	}

	Vec3  vPos;
	float fTimeStarted;
	float fLifetime;
};

typedef std::list<SSonarHint>    SSonarHintsLst;
typedef SSonarHintsLst::iterator SSonarHintsLstIter;

REINST(Implement a ISoundSystemEventListener)
//class CSoundEventsListener: public ISoundSystemEventListener
//{
//private:
//	SSonarHintsLst m_pSonarHints;
//
//	float m_fLastTimeHintsUpdate;
//	float m_fTimeUpdateThreshold;	// no need to update hints every time - do it only every N seconds
//
//	int32 m_nLastFrameID;
//
//public:
//	CSoundEventsListener()
//	{
//		m_fLastTimeHintsUpdate = 0.0f;
//		m_fTimeUpdateThreshold = 0.5f;
//		m_nLastFrameID = -1;
//	}
//
//	~CSoundEventsListener()
//	{
//		Release();
//	}
//
//	void Update()
//	{
//		SSonarHintsLstIter pCurr = m_pSonarHints.begin();
//		SSonarHintsLstIter pEndIt = m_pSonarHints.end();
//		while( pCurr != pEndIt /*; ++pCurr*/ )
//		{
//			if( (gEnv->pTimer->GetCurrTime() - pCurr->fTimeStarted) > pCurr->fLifetime )
//			{
//				pCurr = m_pSonarHints.erase( pCurr );
//			}
//			else
//				++pCurr;
//		}
//
//	}
//
//	void Release()
//	{
//		m_pSonarHints.resize(0);
//	}
//
//	virtual void OnSoundSystemEvent(ESoundSystemCallbackEvent event, ISound *pSound)
//	{
//		assert( pSound );
//
//		if(event == SOUNDSYSTEM_EVENT_ON_PLAYBACK_STARTED || event == SOUNDSYSTEM_EVENT_ON_PLAYBACK_STOPPED)
//		{
//			static const uint32 nSonarSoundSemantincs = (eSoundSemantic_Explosion|eSoundSemantic_Physics_Collision|eSoundSemantic_Physics_Footstep|eSoundSemantic_Vehicle|eSoundSemantic_Weapon);
//		  if( pSound->GetSemantic() & nSonarSoundSemantincs )
//			{
//				const float fCurrTime = gEnv->pTimer->GetCurrTime();
//				if( (fCurrTime - m_fLastTimeHintsUpdate) < 1.0f / 30.0f)//m_fTimeUpdateThreshold)
//					return;
//
//				const int32 nFrameID = gRenDev->GetFrameID( false );
//				if( m_nLastFrameID != nFrameID )
//				{
//					m_fLastTimeHintsUpdate = fCurrTime;
//					m_nLastFrameID = nFrameID;
//				}
//
//				SSonarHint pSonarHint( pSound->GetPosition() , fCurrTime, 0.5f + cry_frand());
//				m_pSonarHints.push_back( pSonarHint );
//			}
//		}
//	}
//
//	const SSonarHintsLst &GetSonarHintsLst() const
//	{
//		return m_pSonarHints;
//	}
//
//	static CSoundEventsListener *GetInstance()
//	{
//		static CSoundEventsListener pInstance;
//		return &pInstance;
//	}
//};

/////////////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////////////////

bool CSonarVision::Preprocess(const SRenderViewInfo& viewInfo)
{
	CTexture* pPrevFrame = CRendererResources::s_ptexPrevFrameScaled;
	if (!pPrevFrame)
		return false;

	if (gRenDev->IsCustomRenderModeEnabled(eRMF_SONARVISION))
	{
		UpdateSoundEvents();
		return true;
	}

	REINST(Remove a sound event listener)
	//if( m_pSoundEventsListener )
	//{
	//	m_pSoundEventsListener->Release();
	//	gEnv->pSoundSystem->RemoveEventListener( (ISoundSystemEventListener*)m_pSoundEventsListener );
	//	m_pSoundEventsListener = 0;
	//}

	return false;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////////////////

void CSonarVision::UpdateSoundEvents()
{
	REINST(Update sound events)
	//if( !m_pSoundEventsListener )
	//{
	//	m_pSoundEventsListener = CSoundEventsListener::GetInstance();
	//	gEnv->pSoundSystem->AddEventListener( (ISoundSystemEventListener*) m_pSoundEventsListener, false);
	//	return;
	//}

	//m_pSoundEventsListener->Update();
}

/////////////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////////////////

void CSonarVision::AmbientPass()
{
	ASSERT_LEGACY_PIPELINE
/*
	PROFILE_LABEL_SCOPE("AMBIENT_PASS");

	Vec4 vParamsPS = Vec4(1, 1, 1.0f, 1.0f);
	Vec4 vParamsVS = Vec4(cry_random(0, 1023), cry_random(0, 1023), cry_random(0, 1023), cry_random(0, 1023));

	PostProcessUtils().ShBeginPass(CShaderMan::s_shPostEffectsRenderModes, m_pszAmbientTechName, FEF_DONTSETTEXTURES | FEF_DONTSETSTATES);

	gcpRendD3D->FX_SetState(GS_NODEPTHTEST);

	CShaderMan::s_shPostEffectsRenderModes->FXSetVSFloat(m_pszParamNameVS, &vParamsVS, 1);
	CShaderMan::s_shPostEffectsRenderModes->FXSetPSFloat(m_pszParamNamePS, &vParamsPS, 1);

	PostProcessUtils().SetTexture(CRendererResources::s_ptexZTarget, 0, FILTER_POINT);
	PostProcessUtils().SetTexture(CRendererResources::s_ptexSceneNormalsMap, 1, FILTER_POINT);
	PostProcessUtils().SetTexture(CRendererResources::s_ptexBackBuffer, 2, FILTER_POINT);
	PostProcessUtils().SetTexture(CRendererResources::s_ptexBackBufferScaled[1], 3, FILTER_LINEAR);
	PostProcessUtils().SetTexture(m_pNoise, 4, FILTER_POINT, eSamplerAddressMode_Wrap);
	PostProcessUtils().SetTexture(m_pGradient, 5, FILTER_LINEAR);

	SD3DPostEffectsUtils::DrawFullScreenTriWPOS(CRendererResources::s_ptexBackBuffer->GetWidth(), CRendererResources::s_ptexBackBuffer->GetHeight());

	PostProcessUtils().ShEndPass();
*/
}

/////////////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////////////////

void CSonarVision::SoundHintsPass()
{
	// todo:
	//	-	scissor + depth bounds test for sonar hints
	//	- could maybe also render at 1/4 lower res

	Vec4 vParamsPS = Vec4(1, 1, 1.0f, 1.0f);

	REINST(Iterate over the sonar hints)
	//SSonarHintsLst pSonarHints = m_pSoundEventsListener->GetSonarHintsLst();
	//SSonarHintsLstIter pCurr = pSonarHints.begin();
	//SSonarHintsLstIter pEndIt = pSonarHints.end();

	//for(; pCurr != pEndIt ; ++pCurr )
	//{
	//	PostProcessUtils().ShBeginPass(CShaderMan::s_shPostEffectsRenderModes, m_pszSoundHintsTechName, FEF_DONTSETTEXTURES|FEF_DONTSETSTATES);

	//	gcpRendD3D->FX_SetState(GS_NODEPTHTEST|GS_BLSRC_ONE | GS_BLDST_ONE);

	//	vParamsPS = Vec4( pCurr->vPos.x, pCurr->vPos.y, pCurr->vPos.z, ((gEnv->pTimer->GetCurrTime() - pCurr->fTimeStarted) /pCurr->fLifetime) );
	//	CShaderMan::s_shPostEffectsRenderModes->FXSetPSFloat(m_pszParamNamePS, &vParamsPS, 1);

	//	PostProcessUtils().SetTexture(CRendererResources::s_ptexZTarget, 0, FILTER_POINT);
	//	PostProcessUtils().SetTexture(CRendererResources::s_ptexSceneNormalsMap, 1, FILTER_POINT);
	//	PostProcessUtils().SetTexture( m_pNoise, 2, FILTER_POINT, 0);

	//	SD3DPostEffectsUtils::DrawFullScreenTriWPOS(CRendererResources::s_ptexBackBuffer->GetWidth(), CRendererResources::s_ptexBackBuffer->GetHeight());

	//	PostProcessUtils().ShEndPass();
	//}

}

/////////////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////////////////

void CSonarVision::GhostingPass()
{
	ASSERT_LEGACY_PIPELINE
/*
	PROFILE_LABEL_SCOPE("GHOSTING_PASS");

	CTexture* pPrevFrame = CRendererResources::s_ptexPrevFrameScaled;
	Vec4 vParamsPS = Vec4(1, 1, 1.0f, 1.0f);

	// Update ghosting
	gcpRendD3D->FX_PushRenderTarget(0, pPrevFrame, NULL);
	gcpRendD3D->RT_SetViewport(0, 0, pPrevFrame->GetWidth(), pPrevFrame->GetHeight());

	PostProcessUtils().ShBeginPass(CShaderMan::s_shPostEffectsRenderModes, m_pszTechNameGhosting, FEF_DONTSETTEXTURES | FEF_DONTSETSTATES);

	uint32 nState = GS_NODEPTHTEST;
	nState |= GS_BLSRC_SRCALPHA | GS_BLDST_SRCALPHA;

	gcpRendD3D->FX_SetState(nState);

	PostProcessUtils().SetTexture(CRendererResources::s_ptexBackBufferScaled[0], 0, FILTER_POINT);

	vParamsPS.w = gEnv->pTimer->GetFrameTime();
	CShaderMan::s_shPostEffectsRenderModes->FXSetPSFloat(m_pszParamNamePS, &vParamsPS, 1);

	SD3DPostEffectsUtils::DrawFullScreenTriWPOS(CRendererResources::s_ptexBackBuffer->GetWidth(), CRendererResources::s_ptexBackBuffer->GetHeight());

	PostProcessUtils().ShEndPass();

	gcpRendD3D->FX_PopRenderTarget(0);
*/
}

/////////////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////////////////

void CSonarVision::FinalComposePass()
{
	ASSERT_LEGACY_PIPELINE
/*
	// Final composition: apply directional blur, add ghosting

	CTexture* pPrevFrame = CRendererResources::s_ptexPrevFrameScaled;
	Vec4 vParamsPS = Vec4(1, 1, 1.0f, 1.0f);
	Vec4 vParamsVS = Vec4(cry_random(0, 1023), cry_random(0, 1023), cry_random(0, 1023), cry_random(0, 1023));

	PostProcessUtils().CopyScreenToTexture(CRendererResources::s_ptexBackBuffer);                              // Unfortunately need to update backbuffer texture atm - refactor this
	PostProcessUtils().StretchRect(CRendererResources::s_ptexBackBuffer, CRendererResources::s_ptexBackBufferScaled[0]); // Todo: use msaa trickery for x360

	// Clean up..
	float fCurrTime = gEnv->pTimer->GetCurrTime() * 10.0f;
	float fCurrAnim = 0.25f * fabs((ceil(fCurrTime * 0.05f) - fCurrTime * 0.05f)) * 2.0f - 1.0f + fabs((ceil(fCurrTime * 0.45f + 0.25f) - (fCurrTime * 0.45f + 0.25f)) * 2.0f - 1.0f);
	PostProcessUtils().TexBlurDirectional(CRendererResources::s_ptexBackBufferScaled[0], Vec2(cry_random(0.0f, 0.125f), cry_random(2.0f, 3.0f)) * fCurrAnim * 4.0f, 1);

	// Final composition
	PostProcessUtils().ShBeginPass(CShaderMan::s_shPostEffectsRenderModes, m_pszTechFinalComposition, FEF_DONTSETTEXTURES | FEF_DONTSETSTATES);

	gcpRendD3D->FX_SetState(GS_NODEPTHTEST);

	CShaderMan::s_shPostEffectsRenderModes->FXSetVSFloat(m_pszParamNameVS, &vParamsVS, 1);
	CShaderMan::s_shPostEffectsRenderModes->FXSetPSFloat(m_pszParamNamePS, &vParamsPS, 1);

	PostProcessUtils().SetTexture(CRendererResources::s_ptexZTarget, 0, FILTER_POINT);
	PostProcessUtils().SetTexture(CRendererResources::s_ptexSceneNormalsMap, 1, FILTER_POINT);
	PostProcessUtils().SetTexture(CRendererResources::s_ptexBackBuffer, 2, FILTER_POINT);
	PostProcessUtils().SetTexture(CRendererResources::s_ptexBackBufferScaled[0], 3, FILTER_LINEAR);
	PostProcessUtils().SetTexture(pPrevFrame, 4, FILTER_LINEAR);
	PostProcessUtils().SetTexture(m_pNoise, 5, FILTER_POINT, eSamplerAddressMode_Wrap);
	PostProcessUtils().SetTexture(m_pGradient, 6, FILTER_LINEAR);

	SD3DPostEffectsUtils::DrawFullScreenTriWPOS(CRendererResources::s_ptexBackBuffer->GetWidth(), CRendererResources::s_ptexBackBuffer->GetHeight());

	PostProcessUtils().ShEndPass();
*/
}

/////////////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////////////////

void CSonarVision::Render()
{
	gRenDev->m_cEF.mfRefreshSystemShader("PostEffectsRenderModes", CShaderMan::s_shPostEffectsRenderModes);

	// todo:
	//	- when enabled: disable all other post processing ? sunshafts/edgeAA/color grading/shadows/ssao
	//	- if no blending needed - could skip hdr/general passes/light/shadows

	AmbientPass();

	SoundHintsPass();

	FinalComposePass();

	GhostingPass();
}

/////////////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////////////////

bool CThermalVision::Preprocess(const SRenderViewInfo& viewInfo)
{
	// Not implemented
	/*
	CTexture* pPrevFrame = CRendererResources::s_ptexPrevFrameScaled;
	if (!pPrevFrame)
	{
		m_bInit = true;
		return false;
	}

	if (gRenDev->IsCustomRenderModeEnabled(eRMF_THERMALVISION))
		return true;

	m_fBlending = 0.0f;
	m_pSonarParams[0].vPosition = gRenDev->GetCamera().GetPosition();
	m_pSonarParams[0].fRadius = 32.0f;
	m_pSonarParams[0].fLifetime = m_pSonarParams[0].fColorMul = 0.0f;
	m_pSonarParams[1] = m_pSonarParams[0];
	m_bInit = true;
	*/
	return false;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////////////////

void CThermalVision::AmbientPass()
{
	ASSERT_LEGACY_PIPELINE
/*
	PROFILE_LABEL_SCOPE("AMBIENT_PASS");

	// Render "ambient"/cold pass for thermal vision
	Vec4 vParamsPS = Vec4(1.0f, 1.0f, 1.0f, 1.0f / (CRenderer::CV_r_ThermalVisionViewDistance * CRenderer::CV_r_ThermalVisionViewDistance));
	Vec4 vParamsVS = Vec4(cry_random(0, 1023), cry_random(0, 1023), cry_random(0, 1023), cry_random(0, 1023));

	PostProcessUtils().ShBeginPass(CShaderMan::s_shPostEffectsRenderModes, m_pszAmbientTechName, FEF_DONTSETTEXTURES | FEF_DONTSETSTATES);

	gcpRendD3D->FX_SetState(GS_NODEPTHTEST);

	CShaderMan::s_shPostEffectsRenderModes->FXSetVSFloat(m_pszParamNameVS, &vParamsVS, 1);
	CShaderMan::s_shPostEffectsRenderModes->FXSetPSFloat(m_pszParamNamePS, &vParamsPS, 1);

	PostProcessUtils().SetTexture(CRendererResources::s_ptexZTarget, 0, FILTER_POINT);

	SD3DPostEffectsUtils::DrawFullScreenTriWPOS(CRendererResources::s_ptexBackBuffer->GetWidth(), CRendererResources::s_ptexBackBuffer->GetHeight());

	PostProcessUtils().ShEndPass();
*/
}

/////////////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////////////////

void CThermalVision::HeatSourcesPasses()
{
	ASSERT_LEGACY_PIPELINE
}

/////////////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////////////////

void CThermalVision::GhostingPass()
{
	ASSERT_LEGACY_PIPELINE
/*
	PROFILE_LABEL_SCOPE("GHOSTING_PASS");

	// Update ghosting

	CTexture* pPrevFrame = CRendererResources::s_ptexPrevFrameScaled;

	gcpRendD3D->FX_PushRenderTarget(0, pPrevFrame, NULL);
	gcpRendD3D->RT_SetViewport(0, 0, pPrevFrame->GetWidth(), pPrevFrame->GetHeight());

	PostProcessUtils().ShBeginPass(CShaderMan::s_shPostEffectsRenderModes, m_pszTechNameGhosting, FEF_DONTSETTEXTURES | FEF_DONTSETSTATES);

	uint32 nState = GS_NODEPTHTEST;
	nState |= GS_BLSRC_SRCALPHA | GS_BLDST_ONEMINUSSRCALPHA;

	gcpRendD3D->FX_SetState(nState);

	Vec4 vParamsVS = Vec4(cry_random(0, 1023), cry_random(0, 1023), cry_random(0, 1023), cry_random(0, 1023));
	CShaderMan::s_shPostEffectsRenderModes->FXSetVSFloat(m_pszParamNameVS, &vParamsVS, 1);

	PostProcessUtils().SetTexture(CRendererResources::s_ptexBackBufferScaled[0], 0, FILTER_POINT);

	SD3DPostEffectsUtils::DrawFullScreenTriWPOS(CRendererResources::s_ptexBackBuffer->GetWidth(), CRendererResources::s_ptexBackBuffer->GetHeight());

	PostProcessUtils().ShEndPass();

	gcpRendD3D->FX_PopRenderTarget(0);

	//	PostProcessUtils().TexBlurDirectional(pPrevFrame, Vec2(0,1)*8.0f , 1);
*/
}

/////////////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////////////////

void CThermalVision::FinalComposePass()
{
	ASSERT_LEGACY_PIPELINE
/*
	PROFILE_LABEL_SCOPE("COMPOSE_PASS");

	// Small tweak for stereo - use less strong nanovision bloom
	CD3DStereoRenderer& pS3DRend = gcpRendD3D->GetS3DRend();
	bool bPostProcStereo = pS3DRend.IsPostStereoEnabled();
	const uint64 prevHWFlags = gRenDev->m_RP.m_FlagsShader_RT;

	CTexture* pPrevFrame = CRendererResources::s_ptexPrevFrameScaled; // on MGPU mode always read from first GPU updated RT

	float fDstBlend = gcpRendD3D->IsCustomRenderModeEnabled(eRMF_NIGHTVISION) ? 1.0f : 0.0f;
	m_fBlending += (fDstBlend - m_fBlending) * gEnv->pTimer->GetFrameTime() * 2.0f;

	// No transition if off-screen
	if (m_bRenderOffscreen)
	{
		m_fBlending = 1.0f;
	}

	RECT srcRect;
	srcRect.top = srcRect.left = 0;
	srcRect.right = LONG(CRendererResources::s_ptexBackBufferScaled[0]->GetWidth() * gcpRendD3D->m_CurViewportScale.x);
	srcRect.bottom = LONG(CRendererResources::s_ptexBackBufferScaled[0]->GetHeight() * gcpRendD3D->m_CurViewportScale.y);

	PostProcessUtils().CopyScreenToTexture(CRendererResources::s_ptexBackBuffer);

	PostProcessUtils().StretchRect(CRendererResources::s_ptexSceneNormalsMap, CRendererResources::s_ptexBackBufferScaled[0], false, false, false, false, SPostEffectsUtils::eDepthDownsample_None, false, &srcRect);  // todo: use msaa trickery for x360
	PostProcessUtils().TexBlurIterative(CRendererResources::s_ptexBackBufferScaled[0], 1, false, CRendererResources::s_ptexBackBufferScaledTemp[0]);
	PostProcessUtils().StretchRect(CRendererResources::s_ptexBackBufferScaled[0], CRendererResources::s_ptexBackBufferScaled[1]);                                         // todo: use msaa trickery for x360
	PostProcessUtils().TexBlurGaussian(CRendererResources::s_ptexBackBufferScaled[1], 1, 1.0f, 5.0f, false, 0, false, CRendererResources::s_ptexBackBufferScaledTemp[1]); // iterative blur instead

	if (m_bRenderOffscreen)
	{
		gcpRendD3D->FX_ClearTarget(CRendererResources::s_ptexSceneDiffuse, Clr_Transparent);
		gcpRendD3D->FX_PushRenderTarget(0, CRendererResources::s_ptexSceneDiffuse, NULL);

		gRenDev->m_RP.m_FlagsShader_RT |= g_HWSR_MaskBit[HWSR_SAMPLE0];
	}

	PostProcessUtils().ShBeginPass(CShaderMan::s_shPostEffectsRenderModes, m_pszTechFinalComposition, FEF_DONTSETTEXTURES | FEF_DONTSETSTATES);

	gcpRendD3D->FX_SetState(GS_NODEPTHTEST);

	float fCamMovNoiseAmount = (1.0f + m_pCamMovNoiseAmount->GetParam()) * CRenderer::CV_r_NightVisionCamMovNoiseAmount;
	Vec4 vParamsPS = Vec4(m_fCameraMovementAmount * fCamMovNoiseAmount, 1.0f, min(1.0f, 10.0f * (1.0f - fabsf(m_fBlending * 2.0f - 1.0f))), m_fBlending);
	Vec4 vParamsVS = Vec4(cry_random(0, 1023), cry_random(0, 1023), cry_random(0, 1023), cry_random(0, 1023));
	Vec4 vParamsVS2 = Vec4(gcpRendD3D->m_CurViewportScale.x, gcpRendD3D->m_CurViewportScale.y, 0.0f, 0.0f);

	CShaderMan::s_shPostEffectsRenderModes->FXSetVSFloat(m_pszParamNameVS, &vParamsVS, 1);
	CShaderMan::s_shPostEffectsRenderModes->FXSetVSFloat(m_pszParamNameVS2, &vParamsVS2, 1);
	CShaderMan::s_shPostEffectsRenderModes->FXSetPSFloat(m_pszParamNamePS, &vParamsPS, 1);

	PostProcessUtils().SetTexture(CRendererResources::s_ptexBackBuffer, 0, FILTER_POINT);
	PostProcessUtils().SetTexture(CRendererResources::s_ptexBackBufferScaled[0], 1, FILTER_LINEAR);
	PostProcessUtils().SetTexture(CRendererResources::s_ptexBackBufferScaled[1], 2, FILTER_LINEAR);
	PostProcessUtils().SetTexture(m_pNoise, 3, FILTER_POINT, eSamplerAddressMode_Wrap);
	PostProcessUtils().SetTexture(m_pGradient, 4, FILTER_LINEAR);
	PostProcessUtils().SetTexture(CRendererResources::s_ptexSceneNormalsMap, 5, FILTER_POINT);

	if (!m_bRenderOffscreen)
	{
		PostProcessUtils().SetTexture((!bPostProcStereo) ? pPrevFrame : CRendererResources::s_ptexBackBufferScaled[0], 6, FILTER_LINEAR);
	}

	SD3DPostEffectsUtils::DrawFullScreenTriWPOS(CRendererResources::s_ptexBackBuffer->GetWidth(), CRendererResources::s_ptexBackBuffer->GetHeight());
	PostProcessUtils().ShEndPass();

	if (m_bRenderOffscreen)
	{
		gcpRendD3D->FX_PopRenderTarget(0);

		gRenDev->m_RP.m_FlagsShader_RT = prevHWFlags;
	}
*/
}

/////////////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////////////////

void CThermalVision::Render()
{
	ASSERT_LEGACY_PIPELINE
}

#pragma warning(pop)
