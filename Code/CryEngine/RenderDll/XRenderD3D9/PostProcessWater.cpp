// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "DriverD3D.h"
#include <Cry3DEngine/I3DEngine.h>
#include "D3DPostProcess.h"

#pragma warning(push)
#pragma warning(disable: 4244)

////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////

void CUnderwaterGodRays::Render()
{
	ASSERT_LEGACY_PIPELINE
/*
	PROFILE_LABEL_SCOPE("GODRAYS");

	// Get current viewport
	int iTempX, iTempY, iWidth, iHeight;
	gcpRendD3D->GetViewport(&iTempX, &iTempY, &iWidth, &iHeight);

	float fAmount = m_pAmount->GetParam();
	float fWatLevel = SPostEffectsUtils::m_fWaterLevel;

	static CCryNameTSCRC pTechName("UnderwaterGodRays");
	static CCryNameR pParam0Name("PI_GodRaysParamsVS");
	static CCryNameR pParam1Name("PI_GodRaysParamsPS");

	uint64 nSaveFlagsShader_RT = gRenDev->m_RP.m_FlagsShader_RT;
	gRenDev->m_RP.m_FlagsShader_RT &= g_HWSR_MaskBit[HWSR_REVERSE_DEPTH];

	gRenDev->m_RP.m_FlagsShader_RT |= g_HWSR_MaskBit[HWSR_REVERSE_DEPTH];

	//////////////////////////////////////////////////////////////////////////////////////////////////
	// Render god-rays into low-res render target for less fillrate hit

	gcpRendD3D->FX_ClearTarget(CRendererResources::s_ptexBackBufferScaled[1], Clr_Transparent);
	gcpRendD3D->FX_PushRenderTarget(0, CRendererResources::s_ptexBackBufferScaled[1], 0);
	gcpRendD3D->RT_SetViewport(0, 0, CRendererResources::s_ptexBackBufferScaled[1]->GetWidth(), CRendererResources::s_ptexBackBufferScaled[1]->GetHeight());

	//  PostProcessUtils().ShBeginPass(CShaderMan::m_shPostEffects, pTechName, FEF_DONTSETSTATES);
	uint32 nPasses;
	CShaderMan::s_shPostEffects->FXSetTechnique(pTechName);
	CShaderMan::s_shPostEffects->FXBegin(&nPasses, FEF_DONTSETSTATES);

	int nSlicesCount = 10;
	bool bInVisarea = (gRenDev->m_p3DEngineCommon.m_pCamVisAreaInfo.nFlags & S3DEngineCommon::VAF_EXISTS_FOR_POSITION);
	Vec3 vSunDir = bInVisarea ? Vec3(0.5f, 0.5f, 1.0f) : -gEnv->p3DEngine->GetSunDirNormalized();

	for (int r(0); r < nSlicesCount; ++r)
	{
		// !force updating constants per-pass!
		CShaderMan::s_shPostEffects->FXBeginPass(0);

		// Set per instance params
		Vec4 pParams = Vec4(fWatLevel, fAmount, r, 1.0f / (float) nSlicesCount);
		CShaderMan::s_shPostEffects->FXSetVSFloat(pParam0Name, &pParams, 1);

		CShaderMan::s_shPostEffects->FXSetPSFloat(pParam1Name, &pParams, 1);

		gcpRendD3D->SetCullMode(R_CULL_NONE);
		gcpRendD3D->FX_SetState(GS_BLSRC_ONE | GS_BLDST_ONE | GS_NODEPTHTEST);

		PostProcessUtils().DrawFullScreenTri(CRendererResources::s_ptexBackBuffer->GetWidth(), CRendererResources::s_ptexBackBuffer->GetHeight(), 0);

		CShaderMan::s_shPostEffects->FXEndPass();
	}

	CShaderMan::s_shPostEffects->FXEnd();
	gRenDev->m_RP.m_FlagsShader_RT = nSaveFlagsShader_RT;

	// Restore previous viewport
	gcpRendD3D->FX_PopRenderTarget(0);
	gcpRendD3D->RT_SetViewport(iTempX, iTempY, iWidth, iHeight);

	//////////////////////////////////////////////////////////////////////////////////////////////////
	// Display god-rays

	CCryNameTSCRC pTechName0("UnderwaterGodRaysFinal");

	PostProcessUtils().ShBeginPass(CShaderMan::s_shPostEffects, pTechName0, FEF_DONTSETSTATES);
	gcpRendD3D->FX_SetState(GS_NODEPTHTEST);
	Vec4 pParams = Vec4(CRenderer::CV_r_water_godrays_distortion, 0, 0, 0);
	CShaderMan::s_shPostEffects->FXSetPSFloat(pParam1Name, &pParams, 1);
	PostProcessUtils().DrawFullScreenTri(CRendererResources::s_ptexBackBuffer->GetWidth(), CRendererResources::s_ptexBackBuffer->GetHeight());
	PostProcessUtils().ShEndPass();
*/
}

/////////////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////////////////

void CWaterDroplets::Render()
{
	ASSERT_LEGACY_PIPELINE
/*
	gRenDev->m_cEF.mfRefreshSystemShader("PostEffectsGame", CShaderMan::s_shPostEffectsGame);

	static CCryNameTSCRC pTechName("WaterDroplets");
	PostProcessUtils().ShBeginPass(CShaderMan::s_shPostEffectsGame, pTechName, FEF_DONTSETSTATES);

	gcpRendD3D->FX_SetState(GS_NODEPTHTEST);

	const float fUserAmount = m_pAmount->GetParam();

	const float fAtten = 1.0f; //clamp_tpl<float>(fabs( gRenDev->GetRCamera().vOrigin.z - SPostEffectsUtils::m_fWaterLevel ), 0.0f, 1.0f);
	Vec4 vParams = Vec4(1, 1, 1, min(fUserAmount, 1.0f) * fAtten);
	static CCryNameR pParamName("waterDropletsParams");
	CShaderMan::s_shPostEffectsGame->FXSetPSFloat(pParamName, &vParams, 1);

	PostProcessUtils().DrawFullScreenTri(CRendererResources::s_ptexBackBuffer->GetWidth(), CRendererResources::s_ptexBackBuffer->GetHeight());

	PostProcessUtils().ShEndPass();
	*/
}

/////////////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////////////////

void CWaterFlow::Render()
{
	ASSERT_LEGACY_PIPELINE
/*
	gRenDev->m_cEF.mfRefreshSystemShader("PostEffectsGame", CShaderMan::s_shPostEffectsGame);

	float fAmount = m_pAmount->GetParam();

	static CCryNameTSCRC pTechName("WaterFlow");
	PostProcessUtils().ShBeginPass(CShaderMan::s_shPostEffectsGame, pTechName, FEF_DONTSETSTATES);

	gcpRendD3D->FX_SetState(GS_NODEPTHTEST);

	Vec4 vParams = Vec4(1, 1, 1, fAmount);
	static CCryNameR pParamName("waterFlowParams");
	CShaderMan::s_shPostEffectsGame->FXSetPSFloat(pParamName, &vParams, 1);

	PostProcessUtils().DrawFullScreenTri(CRendererResources::s_ptexBackBuffer->GetWidth(), CRendererResources::s_ptexBackBuffer->GetHeight());

	PostProcessUtils().ShEndPass();
*/
}

#pragma warning(pop)
