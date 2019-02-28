// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

/*=============================================================================
   PostProcessDOF : depth of field post processing

   Revision history:
* 23/02/2005: Re-factored/Converted to CryEngine 2.0 by Tiago Sousa
* Created by Tiago Sousa

   =============================================================================*/

#include "StdAfx.h"
#include "DriverD3D.h"
#include <Cry3DEngine/I3DEngine.h>
#include "D3DPostProcess.h"

#pragma warning(push)
#pragma warning(disable: 4244)

/////////////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////////////////

SDepthOfFieldParams CDepthOfField::GetParams()
{
	SDepthOfFieldParams pParams;

	bool bGameDof = IsActive();

	float fFocusRange = m_pFocusRange->GetParam();
	float fMaxCoC = m_pMaxCoC->GetParam();
	float fBlurAmount = m_pBlurAmount->GetParam();

	float fTodFocusRange = (CRenderer::CV_r_dof == 2) ? m_pTimeOfDayFocusRange->GetParam() : 0;
	float fTodBlurAmount = (CRenderer::CV_r_dof == 2) ? m_pTimeOfDayBlurAmount->GetParam() : 0;

	// Very high spec uses a different pipeline and needs a stronger blur for the same visual effect
	EShaderQuality nShaderQuality = gRenDev->EF_GetShaderQuality(eST_PostProcess);
	const float fMaxBlurAmount = (nShaderQuality >= eSQ_VeryHigh) ? 0.5f : 0.4f;
	const float fBlurScale = (nShaderQuality >= eSQ_VeryHigh) ? 1.8f : 1.0f;

	// Hack to reduce amount of blur for transparent/particles with full view depth behind them
	fTodBlurAmount = min(fTodBlurAmount * fBlurScale, fMaxBlurAmount);

	float fUserFocusRange = m_pUserFocusRange->GetParam();
	float fUserFocusDistance = m_pUserFocusDistance->GetParam();
	float fUserBlurAmount = m_pUserBlurAmount->GetParam();
	float fFrameTime = clamp_tpl<float>(gEnv->pTimer->GetFrameTime() * 3.0f, 0.0f, 1.0f);

	if (bGameDof)
		fUserFocusRange = fUserFocusDistance = fUserBlurAmount = 0.0f;

	m_fUserFocusRangeCurr += (fUserFocusRange - m_fUserFocusRangeCurr) * fFrameTime;
	m_fUserFocusDistanceCurr += (fUserFocusDistance - m_fUserFocusDistanceCurr) * fFrameTime;
	m_fUserBlurAmountCurr += (fUserBlurAmount - m_fUserBlurAmountCurr) * fFrameTime;

	pParams.bGameMode = false;
	if (bGameDof)
	{
		// For gameplay dof, game code setting up independently near/far focus planes and additionally might use a focus mask

		if (fFocusRange < 0.0f)
		{
			pParams.bGameMode = true;
			float fFocusMin = m_pFocusMin->GetParam();
			float fFocusMax = m_pFocusMax->GetParam();
			float fFocusLimit = m_pFocusLimit->GetParam();

			// near blur plane distance, far blur plane distance, focus plane distance, blur amount
			pParams.vFocus = Vec4(fFocusMin, fFocusLimit, fFocusLimit - fFocusMax, fBlurAmount);
		}
		else
		{
			float fFocusRange_ = m_pFocusRange->GetParam();
			float fFocusDistance = m_pFocusDistance->GetParam();

			pParams.vFocus = Vec4(-fFocusRange_ * 0.5f, fFocusRange_ * 0.5f, fFocusDistance, fBlurAmount);
		}
	}
	else
	{
		// For non-gameplay mode, we use time of day values for far dof, blended with regular dof params

		// near blur plane distance, far blur plane distance, focus plane distance, blur amount
		static float s_fTodFocusRange = 0.0f;
		static float s_fTodBlurAmount = 0.0f;
		bool bUseGameSettings = (m_pUserActive->GetParam()) ? true : false;

		if (bUseGameSettings)
		{
			s_fTodFocusRange += (m_fUserFocusRangeCurr - s_fTodFocusRange) * fFrameTime;
			s_fTodBlurAmount += (m_fUserBlurAmountCurr - s_fTodBlurAmount) * fFrameTime;

			// near blur plane distance, far blur plane distance, focus plane distance, blur amount
			pParams.vFocus = Vec4(-m_fUserFocusRangeCurr * 0.5f, m_fUserFocusRangeCurr * 0.5f, m_fUserFocusDistanceCurr, m_fUserBlurAmountCurr);
		}
		else
		{
			s_fTodFocusRange += (fTodFocusRange * 2.0f - s_fTodFocusRange) * fFrameTime;
			s_fTodBlurAmount += (fTodBlurAmount - s_fTodBlurAmount) * fFrameTime;

			pParams.vFocus = Vec4(-s_fTodFocusRange * 0.5f, s_fTodFocusRange * 0.5f, 0, s_fTodBlurAmount);
			//vParams=Vec4(-5, 1000, 00, 2);//s_fTodBlurAmount);
		}
	}

	pParams.vMinZParams.x = m_pFocusMinZ->GetParam();
	pParams.vMinZParams.y = m_pFocusMinZScale->GetParam();
	pParams.vMinZParams.z = pParams.vMinZParams.w = 0.0f;

	pParams.pMaskTex = 0;
	pParams.fMaskBlendAmount = 1.0;
	if (m_pUseMask->GetParam())
		pParams.pMaskTex = const_cast<CTexture*>(static_cast<CParamTexture*>(m_pMaskTex)->GetParamTexture());

	// masked blur has higher priority than usual dof mask
	if (m_pMaskedBlurAmount->GetParam())
	{
		pParams.fMaskBlendAmount = m_pMaskedBlurAmount->GetParam();
		pParams.pMaskTex = const_cast<CTexture*>(static_cast<CParamTexture*>(m_pMaskedBlurMaskTex)->GetParamTexture());
	}

	return pParams;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////////////////

static float ngon_rad(float theta, float n)
{
	return cosf(M_PI / n) / cosf(theta - (2 * M_PI / n) * floorf((n * theta + PI) / (2 * M_PI)));
}

// Shirleys concentric mapping
static Vec4 ToUnitDisk(Vec4& O, float blades, float fstop)
{
	float max_fstops = 8;
	float min_fstops = 1;
	float normalizedStops = 1.0f;//clamp_tpl((fstop - max_fstops) / (max_fstops - min_fstops), 0.0f, 1.0f);

	float phi, r;
	const float a = 2 * O.x - 1;
	const float b = 2 * O.y - 1;
	if (abs(a) > abs(b)) // use squares instead of absolute values
	{
		r = a;
		phi = (M_PI / 4.0) * (b / (a + 1e-6f));
	}
	else
	{
		r = b;
		phi = (M_PI / 2.0) - (M_PI / 4.0) * (a / (b + 1e-6f));
	}

	float rr = r * powf(ngon_rad(phi, blades), normalizedStops);
	rr = abs(rr) * (rr > 0 ? 1.0f : -1.0f);

	//normalizedStops *= -0.4*M_PI;
	return Vec4(rr * cosf(phi + normalizedStops), rr * sinf(phi + normalizedStops), 0.0f, 0.0f);
}

void CDepthOfField::Render()
{
	ASSERT_LEGACY_PIPELINE
/*
	SDepthOfFieldParams dofParams = GetParams();

	if (dofParams.vFocus.w < 0.0001f)
		return;

	CShader* pSH = CShaderMan::s_shPostMotionBlur;

	const int nThreadID = gRenDev->GetRenderThreadID();
	gRenDev->m_cEF.mfRefreshSystemShader("MotionBlur", CShaderMan::s_shPostMotionBlur);

	gcpRendD3D->FX_SetActiveRenderTargets();

	uint64 nSaveFlagsShader_RT = gRenDev->m_RP.m_FlagsShader_RT;
	gRenDev->m_RP.m_FlagsShader_RT &= ~(g_HWSR_MaskBit[HWSR_SAMPLE0] | g_HWSR_MaskBit[HWSR_SAMPLE1] | g_HWSR_MaskBit[HWSR_SAMPLE2]);

	Vec4 vFocus = dofParams.vFocus;
	vFocus.w *= 2;  // For backwards compatibility

	const float fFNumber = 8;
	const float fNumApertureSides = 8;

	if (dofParams.bGameMode)
	{
		dofParams.vFocus.x = 1.0f / (vFocus.z + 1e-6f);
		dofParams.vFocus.y = -vFocus.y / (vFocus.z + 1e-6f);

		dofParams.vFocus.z = -1.0f / (vFocus.w + 1e-6f);
		dofParams.vFocus.w = vFocus.x / (vFocus.w + 1e-6f);
	}
	else
	{
		dofParams.vFocus.x = 1.0f / (vFocus.y + 1e-6f);
		dofParams.vFocus.y = -vFocus.z / (vFocus.y + 1e-6f);

		dofParams.vFocus.z = 1.0f / (vFocus.x + 1e-6f);
		dofParams.vFocus.w = -vFocus.z / (vFocus.x + 1e-6f);
	}

	Vec4 vDofParams0 = dofParams.vFocus;

	const float fNearestDofScaleBoost = dofParams.bGameMode ? CRenderer::CV_r_dofMinZBlendMult : 1.0f;
	Vec4 vDofParams1 = Vec4(CRenderer::CV_r_dofMinZ + dofParams.vMinZParams.x, CRenderer::CV_r_dofMinZScale + dofParams.vMinZParams.y, fNearestDofScaleBoost, vFocus.w);

	PROFILE_LABEL_SCOPE("DOF");

	// For better blending later
	GetUtils().StretchRect(CRendererResources::s_ptexHDRTarget, CRendererResources::s_ptexSceneTarget);

	CTexture* pTexDofLayersTmp[2] = { CRendererResources::s_ptexHDRTargetScaledTmp[0], CRendererResources::s_ptexHDRTargetScaledTempRT[0] };

	assert(pTexDofLayersTmp[0]->GetWidth() == CRendererResources::s_ptexHDRDofLayers[0]->GetWidth() && pTexDofLayersTmp[0]->GetHeight() == CRendererResources::s_ptexHDRDofLayers[0]->GetHeight());
	assert(pTexDofLayersTmp[1]->GetWidth() == CRendererResources::s_ptexHDRDofLayers[1]->GetWidth() && pTexDofLayersTmp[1]->GetHeight() == CRendererResources::s_ptexHDRDofLayers[1]->GetHeight());
	assert(pTexDofLayersTmp[0]->GetPixelFormat() == CRendererResources::s_ptexHDRDofLayers[0]->GetPixelFormat() && pTexDofLayersTmp[1]->GetPixelFormat() == CRendererResources::s_ptexHDRDofLayers[1]->GetPixelFormat());

	{
		// 1st downscale stage
		{
			PROFILE_LABEL_SCOPE("DOWNSCALE LAYERS");

			gcpRendD3D->FX_PushRenderTarget(0, CRendererResources::s_ptexHDRDofLayers[0], NULL); // near
			gcpRendD3D->FX_PushRenderTarget(1, CRendererResources::s_ptexHDRDofLayers[1], NULL); // far
			gcpRendD3D->FX_PushRenderTarget(2, CRendererResources::s_ptexSceneCoC[0], NULL);     // CoC near/far

			static CCryNameTSCRC techNameDownscaleDof("DownscaleDof");
			GetUtils().ShBeginPass(CShaderMan::s_shPostMotionBlur, techNameDownscaleDof, FEF_DONTSETTEXTURES | FEF_DONTSETSTATES);
			gcpRendD3D->FX_SetState(GS_NODEPTHTEST);

			Vec4 vParams = Vec4(CRendererResources::s_ptexHDRDofLayers[0]->GetWidth(), CRendererResources::s_ptexHDRDofLayers[0]->GetHeight(), 1.0f / CRendererResources::s_ptexHDRDofLayers[0]->GetWidth(), 1.0f / CRendererResources::s_ptexHDRDofLayers[0]->GetHeight());
			CShaderMan::s_shPostMotionBlur->FXSetPSFloat(m_pDofFocusParam0Name, &vDofParams0, 1);
			CShaderMan::s_shPostMotionBlur->FXSetVSFloat(m_pDofFocusParam1Name, &vParams, 1);
			CShaderMan::s_shPostMotionBlur->FXSetPSFloat(m_pDofFocusParam1Name, &vDofParams1, 1);

			GetUtils().SetTexture(CRendererResources::s_ptexZTarget, 0, FILTER_POINT);
			GetUtils().SetTexture(CRendererResources::s_ptexHDRTarget, 1, FILTER_LINEAR);
			SD3DPostEffectsUtils::DrawFullScreenTri(CRendererResources::s_ptexSceneTarget->GetWidth(), CRendererResources::s_ptexSceneTarget->GetHeight(), 0, &gcpRendD3D->m_FullResRect);

			GetUtils().ShEndPass();
			gcpRendD3D->FX_PopRenderTarget(0);
			gcpRendD3D->FX_PopRenderTarget(1);
			gcpRendD3D->FX_PopRenderTarget(2);
			gcpRendD3D->FX_SetActiveRenderTargets(); // Avoiding false d3d error (due to deferred rt setup, when ping-pong'ing between RTs we can bump into RTs still bound when binding it as a SRV)
		}

		// 2nd downscale stage (tile min CoC)
		{
			PROFILE_LABEL_SCOPE("MIN COC DOWNSCALE");
			for (uint32 i = 1; i < MIN_DOF_COC_K; i++)
			{
				gcpRendD3D->FX_PushRenderTarget(0, CRendererResources::s_ptexSceneCoC[i], NULL);  // near

				static CCryNameTSCRC techNameTileMinCoC("TileMinCoC");
				GetUtils().ShBeginPass(CShaderMan::s_shPostMotionBlur, techNameTileMinCoC, FEF_DONTSETTEXTURES | FEF_DONTSETSTATES);
				gcpRendD3D->FX_SetState(GS_NODEPTHTEST);

				const Vec4 vParams = Vec4(CRendererResources::s_ptexSceneCoC[i - 1]->GetWidth(), CRendererResources::s_ptexSceneCoC[i - 1]->GetHeight(), 1.0f / CRendererResources::s_ptexSceneCoC[i - 1]->GetWidth(), 1.0f / CRendererResources::s_ptexSceneCoC[i - 1]->GetHeight());
				CShaderMan::s_shPostMotionBlur->FXSetVSFloat(m_pDofFocusParam1Name, &vParams, 1);
				CShaderMan::s_shPostMotionBlur->FXSetPSFloat(m_pDofFocusParam1Name, &vDofParams1, 1);

				GetUtils().SetTexture(CRendererResources::s_ptexSceneCoC[i - 1], 0, FILTER_LINEAR);
				SD3DPostEffectsUtils::DrawFullScreenTri(CRendererResources::s_ptexSceneTarget->GetWidth(), CRendererResources::s_ptexSceneTarget->GetHeight(), 0, &gcpRendD3D->m_FullResRect);

				GetUtils().ShEndPass();
				gcpRendD3D->FX_PopRenderTarget(0);
				gcpRendD3D->FX_SetActiveRenderTargets(); // Avoiding false d3d error
			}
		}
	}

	{
		// 1st gather pass
		{
			const int32 nSquareTapsSide = 7;
			const float fRecipTaps = 1.0f / ((float)nSquareTapsSide - 1.0f);

			Vec4 vTaps[nSquareTapsSide * nSquareTapsSide];
			for (int32 y = 0; y < nSquareTapsSide; ++y)
			{
				for (int32 x = 0; x < nSquareTapsSide; ++x)
				{
					Vec4 t = Vec4(x * fRecipTaps, y * fRecipTaps, 0, 0);
					vTaps[x + y * nSquareTapsSide] = ToUnitDisk(t, fNumApertureSides, fFNumber);
				}
			}

			PROFILE_LABEL_SCOPE("FAR/NEAR LAYER");
			gcpRendD3D->FX_PushRenderTarget(0, pTexDofLayersTmp[0], NULL);
			gcpRendD3D->FX_PushRenderTarget(1, pTexDofLayersTmp[1], NULL);
			gcpRendD3D->FX_PushRenderTarget(2, CRendererResources::s_ptexSceneCoCTemp, NULL);
			gcpRendD3D->FX_SetActiveRenderTargets(); // Avoiding false d3d error

			gRenDev->m_RP.m_FlagsShader_RT &= ~g_HWSR_MaskBit[HWSR_SAMPLE0];
			static CCryNameTSCRC techNameDOF("Dof");
			GetUtils().ShBeginPass(CShaderMan::s_shPostMotionBlur, techNameDOF, FEF_DONTSETTEXTURES | FEF_DONTSETSTATES);
			gcpRendD3D->FX_SetState(GS_NODEPTHTEST);
			vDofParams1.z = nSquareTapsSide * nSquareTapsSide;
			CShaderMan::s_shPostMotionBlur->FXSetPSFloat(m_pDofFocusParam0Name, &vDofParams0, 1);
			CShaderMan::s_shPostMotionBlur->FXSetPSFloat(m_pDofFocusParam1Name, &vDofParams1, 1);
			CShaderMan::s_shPostMotionBlur->FXSetPSFloat(m_pDofTaps, vTaps, nSquareTapsSide * nSquareTapsSide);

			GetUtils().SetTexture(CRendererResources::s_ptexZTargetScaled[0], 0, FILTER_POINT);
			GetUtils().SetTexture(CRendererResources::s_ptexHDRDofLayers[0], 1, FILTER_LINEAR);
			GetUtils().SetTexture(CRendererResources::s_ptexHDRDofLayers[1], 2, FILTER_LINEAR);
			GetUtils().SetTexture(CRendererResources::s_ptexSceneCoC[0], 3, FILTER_LINEAR);
			GetUtils().SetTexture(CRendererResources::s_ptexSceneCoC[MIN_DOF_COC_K - 1], 4, FILTER_POINT);

			SD3DPostEffectsUtils::DrawFullScreenTri(CRendererResources::s_ptexSceneTarget->GetWidth(), CRendererResources::s_ptexSceneTarget->GetHeight(), 0, &gcpRendD3D->m_FullResRect);

			GetUtils().ShEndPass();
			gcpRendD3D->FX_PopRenderTarget(2);
			gcpRendD3D->FX_PopRenderTarget(1);
			gcpRendD3D->FX_PopRenderTarget(0);
		}

		// 2nd gather iteration
		{
			const int32 nSquareTapsSide = 3;
			const float fRecipTaps = 1.0f / ((float)nSquareTapsSide - 1.0f);

			Vec4 vTaps[nSquareTapsSide * nSquareTapsSide];
			for (int32 y = 0; y < nSquareTapsSide; ++y)
			{
				for (int32 x = 0; x < nSquareTapsSide; ++x)
				{
					Vec4 t = Vec4(x * fRecipTaps, y * fRecipTaps, 0, 0);
					vTaps[x + y * nSquareTapsSide] = ToUnitDisk(t, fNumApertureSides, fFNumber);
				}
			}

			PROFILE_LABEL_SCOPE("FAR/NEAR LAYER ITERATION");
			gcpRendD3D->FX_SetActiveRenderTargets(); // Avoiding false d3d error
			gcpRendD3D->FX_PushRenderTarget(0, CRendererResources::s_ptexHDRDofLayers[0], NULL);
			gcpRendD3D->FX_PushRenderTarget(1, CRendererResources::s_ptexHDRDofLayers[1], NULL);
			gcpRendD3D->FX_PushRenderTarget(2, CRendererResources::s_ptexSceneCoC[0], NULL);
			gcpRendD3D->FX_SetActiveRenderTargets(); // Avoiding false d3d error

			gRenDev->m_RP.m_FlagsShader_RT |= g_HWSR_MaskBit[HWSR_SAMPLE0];
			static CCryNameTSCRC techNameDOF("Dof");
			GetUtils().ShBeginPass(CShaderMan::s_shPostMotionBlur, techNameDOF, FEF_DONTSETTEXTURES | FEF_DONTSETSTATES);
			gcpRendD3D->FX_SetState(GS_NODEPTHTEST);
			vDofParams1.z = nSquareTapsSide * nSquareTapsSide;
			CShaderMan::s_shPostMotionBlur->FXSetPSFloat(m_pDofFocusParam0Name, &vDofParams0, 1);
			CShaderMan::s_shPostMotionBlur->FXSetPSFloat(m_pDofFocusParam1Name, &vDofParams1, 1);
			CShaderMan::s_shPostMotionBlur->FXSetPSFloat(m_pDofTaps, vTaps, nSquareTapsSide * nSquareTapsSide);

			GetUtils().SetTexture(CRendererResources::s_ptexZTargetScaled[0], 0, FILTER_POINT);
			GetUtils().SetTexture(pTexDofLayersTmp[0], 1, FILTER_LINEAR);
			GetUtils().SetTexture(pTexDofLayersTmp[1], 2, FILTER_LINEAR);
			GetUtils().SetTexture(CRendererResources::s_ptexSceneCoCTemp, 3, FILTER_POINT);
			GetUtils().SetTexture(CRendererResources::s_ptexSceneCoC[MIN_DOF_COC_K - 1], 4, FILTER_POINT);
			SD3DPostEffectsUtils::DrawFullScreenTri(CRendererResources::s_ptexSceneTarget->GetWidth(), CRendererResources::s_ptexSceneTarget->GetHeight(), 0, &gcpRendD3D->m_FullResRect);

			GetUtils().ShEndPass();
			gcpRendD3D->FX_PopRenderTarget(2);
			gcpRendD3D->FX_PopRenderTarget(1);
			gcpRendD3D->FX_PopRenderTarget(0);
			gcpRendD3D->FX_SetActiveRenderTargets(); // Avoiding false d3d error
		}

		// Final composition
		{
			PROFILE_LABEL_SCOPE("COMPOSITE");
			gcpRendD3D->FX_PushRenderTarget(0, CRendererResources::s_ptexHDRTarget, NULL);

			static CCryNameTSCRC techNameCompositeDof("CompositeDof");
			GetUtils().ShBeginPass(CShaderMan::s_shPostMotionBlur, techNameCompositeDof, FEF_DONTSETTEXTURES | FEF_DONTSETSTATES);
			gcpRendD3D->SetCullMode(R_CULL_BACK);
			gcpRendD3D->FX_SetState(GS_NODEPTHTEST); //|GS_BLSRC_ONE|GS_BLDST_SRCALPHA);
			CShaderMan::s_shPostMotionBlur->FXSetPSFloat(m_pDofFocusParam0Name, &vDofParams0, 1);
			CShaderMan::s_shPostMotionBlur->FXSetPSFloat(m_pDofFocusParam1Name, &vDofParams1, 1);

			GetUtils().SetTexture(CRendererResources::s_ptexZTarget, 0, FILTER_POINT);
			GetUtils().SetTexture(CRendererResources::s_ptexHDRDofLayers[0], 1, FILTER_LINEAR);
			GetUtils().SetTexture(CRendererResources::s_ptexHDRDofLayers[1], 2, FILTER_LINEAR);
			GetUtils().SetTexture(CRendererResources::s_ptexSceneCoCTemp, 3, FILTER_LINEAR);
			GetUtils().SetTexture(CRendererResources::s_ptexSceneTarget, 4, FILTER_POINT);
			SD3DPostEffectsUtils::DrawFullScreenTri(CRendererResources::s_ptexSceneTarget->GetWidth(), CRendererResources::s_ptexSceneTarget->GetHeight(), 0, &gcpRendD3D->m_FullResRect);

			GetUtils().ShEndPass();

			gcpRendD3D->FX_PopRenderTarget(0);
		}

		CRendererResources::s_ptexHDRTarget->SetResolved(true);
		gcpRendD3D->FX_SetActiveRenderTargets();
	}

	gRenDev->m_RP.m_FlagsShader_RT = nSaveFlagsShader_RT;
*/
}

#pragma warning(pop)
