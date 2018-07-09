// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

/*=============================================================================
   PostProcessGame : game related post processing

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

void CHudSilhouettes::Render()
{
	ASSERT_LEGACY_PIPELINE
}

/////////////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////////////////
void CHudSilhouettes::RenderDeferredSilhouettes(float fBlendParam, float fType)
{
	ASSERT_LEGACY_PIPELINE
/*
	gcpRendD3D->RT_SetViewport(PostProcessUtils().m_pScreenRect.left, PostProcessUtils().m_pScreenRect.top, PostProcessUtils().m_pScreenRect.right, PostProcessUtils().m_pScreenRect.bottom);
	PostProcessUtils().m_pCurDepthSurface = &gcpRendD3D->m_DepthBufferOrig;

	CTexture* pScreen = CRendererResources::s_ptexSceneNormalsMap;
	CTexture* pMask = CRendererResources::s_ptexBlack;
	CTexture* pMaskBlurred = CRendererResources::s_ptexBlack;

	// skip processing, nothing was added to mask
	if ((SRendItem::BatchFlags(EFSLIST_GENERAL) | SRendItem::BatchFlags(EFSLIST_TRANSP)) & FB_CUSTOM_RENDER)
	{
		// could render directly to frame buffer ? save 1 resolve - no glow though
		{
			// store silhouettes/signature temporary render target, so that we can post process this afterwards
			gcpRendD3D->FX_PushRenderTarget(0, CRendererResources::s_ptexBackBufferScaled[0], NULL);
			gcpRendD3D->RT_SetViewport(0, 0, CRendererResources::s_ptexBackBufferScaled[0]->GetWidth(), CRendererResources::s_ptexBackBufferScaled[0]->GetHeight());

			static CCryNameTSCRC pTech1Name("BinocularView");
			PostProcessUtils().ShBeginPass(CShaderMan::s_shPostEffectsGame, pTech1Name, FEF_DONTSETTEXTURES | FEF_DONTSETSTATES);

			gRenDev->FX_SetState(GS_NODEPTHTEST);

			// Set VS params
			const float uvOffset = 2.0f;
			Vec4 vsParams = Vec4(uvOffset, 0.0f, 0.0f, 0.0f);
			CShaderMan::s_shPostEffectsGame->FXSetVSFloat(m_vsParamName, &vsParams, 1);

			// Set PS default params
			Vec4 pParams = Vec4(0, 0, 0, (!fType) ? 1.0f : 0.0f);
			CShaderMan::s_shPostEffectsGame->FXSetPSFloat(m_psParamName, &pParams, 1);

			PostProcessUtils().SetTexture(CRendererResources::s_ptexSceneNormalsMap, 0, FILTER_POINT);
			PostProcessUtils().SetTexture(CRendererResources::s_ptexZTarget, 1, FILTER_POINT);
			PostProcessUtils().DrawFullScreenTri(CRendererResources::s_ptexSceneTarget->GetWidth(), CRendererResources::s_ptexSceneTarget->GetHeight());

			PostProcessUtils().ShEndPass();
			gcpRendD3D->FX_PopRenderTarget(0);

			pMask = CRendererResources::s_ptexBackBufferScaled[0];
			pMaskBlurred = CRendererResources::s_ptexBackBufferScaled[1];
		}

		////////////////////////////////////////////////////////////////////////////////////////////////
		// compute glow

		{
			GetUtils().StretchRect(CRendererResources::s_ptexBackBufferScaled[0], CRendererResources::s_ptexBackBufferScaled[1]);

			// blur - for glow
			//			GetUtils().TexBlurGaussian(CRendererResources::s_ptexBackBufferScaled[0], 1, 1.5f, 2.0f, false);
			GetUtils().TexBlurIterative(CRendererResources::s_ptexBackBufferScaled[1], 1, false);

			gcpRendD3D->RT_SetViewport(0, 0, gcpRendD3D->GetWidth(), gcpRendD3D->GetHeight());
		}

		////////////////////////////////////////////////////////////////////////////////////////////////
		// finally add silhouettes to screen

		{
			static CCryNameTSCRC pTechSilhouettesName("BinocularViewSilhouettes");

			GetUtils().ShBeginPass(CShaderMan::s_shPostEffectsGame, pTechSilhouettesName, FEF_DONTSETTEXTURES | FEF_DONTSETSTATES);

			gRenDev->FX_SetState(GS_NODEPTHTEST | GS_BLSRC_ONE | GS_BLDST_ONE);

			GetUtils().SetTexture(pMask, 0);
			GetUtils().SetTexture(pMaskBlurred, 1);

			// Set PS default params
			Vec4 pParams = Vec4(0, 0, 0, fBlendParam * 0.33f);
			static CCryNameR pParamName("psParams");
			CShaderMan::s_shPostEffectsGame->FXSetPSFloat(pParamName, &pParams, 1);

			GetUtils().DrawFullScreenTri(CRendererResources::s_ptexBackBuffer->GetWidth(), CRendererResources::s_ptexBackBuffer->GetHeight(), 0, &gcpRendD3D->m_FullResRect);

			GetUtils().ShEndPass();
		}
	}

	gcpRendD3D->RT_SetViewport(GetUtils().m_pScreenRect.left, GetUtils().m_pScreenRect.top, GetUtils().m_pScreenRect.right, GetUtils().m_pScreenRect.bottom);
*/
}

/////////////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////////////////
void CHudSilhouettes::RenderDeferredSilhouettesOptimised(float fBlendParam, float fType)
{
	ASSERT_LEGACY_PIPELINE
/*
	const bool bHasSilhouettesToRender = ((SRendItem::BatchFlags(EFSLIST_GENERAL) |
	                                       SRendItem::BatchFlags(EFSLIST_TRANSP))
	                                      & FB_CUSTOM_RENDER) ? true : false;

	if (bHasSilhouettesToRender)
	{
		// Down Sample
		GetUtils().StretchRect(CRendererResources::s_ptexSceneNormalsMap, CRendererResources::s_ptexBackBufferScaled[0]);

		PROFILE_LABEL_SCOPE("HUD_SILHOUETTES_DEFERRED_PASS");

		// Draw silhouettes
		GetUtils().ShBeginPass(CShaderMan::s_shPostEffectsGame, m_deferredSilhouettesOptimisedTech, FEF_DONTSETTEXTURES | FEF_DONTSETSTATES);

		gRenDev->FX_SetState(GS_NODEPTHTEST | GS_BLSRC_ONE | GS_BLDST_ONE | GS_NOCOLMASK_A);

		GetUtils().SetTexture(CRendererResources::s_ptexBackBufferScaled[0], 0, FILTER_LINEAR);

		// Set vs params
		const float uvOffset = 1.5f;
		Vec4 vsParams = Vec4(uvOffset, 0.0f, 0.0f, 0.0f);
		CShaderMan::s_shPostEffectsGame->FXSetVSFloat(m_vsParamName, &vsParams, 1);

		// Set ps params
		const float fillStrength = m_pFillStr->GetParam();
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

		CShaderMan::s_shPostEffectsGame->FXSetPSFloat(m_psParamName, &psParams, 1);

		GetUtils().DrawFullScreenTri(CRendererResources::s_ptexBackBuffer->GetWidth(), CRendererResources::s_ptexBackBuffer->GetHeight(), 0, &gcpRendD3D->m_FullResRect);

		GetUtils().ShEndPass();
	}
*/
}

/////////////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////////////////

void CAlienInterference::Render()
{
	ASSERT_LEGACY_PIPELINE
/*
	PROFILE_LABEL_SCOPE("ALIEN_INTERFERENCE");

	gRenDev->m_cEF.mfRefreshSystemShader("PostEffectsGame", CShaderMan::s_shPostEffectsGame);

	float fAmount = m_pAmount->GetParam();

	static CCryNameTSCRC pTechName("AlienInterference");
	PostProcessUtils().ShBeginPass(CShaderMan::s_shPostEffectsGame, pTechName, FEF_DONTSETSTATES);

	gcpRendD3D->FX_SetState(GS_NODEPTHTEST);

	Vec4 vParams = Vec4(1, 1, (float)PostProcessUtils().m_iFrameCounter, fAmount);
	static CCryNameR pParamName("psParams");
	CShaderMan::s_shPostEffectsGame->FXSetPSFloat(pParamName, &vParams, 1);

	static CCryNameR pParamAlienInterferenceName("AlienInterferenceTint");
	vParams = m_pTintColor->GetParamVec4();
	vParams.x *= 2.0f;
	vParams.y *= 2.0f;
	vParams.z *= 2.0f;
	CShaderMan::s_shPostEffectsGame->FXSetPSFloat(pParamAlienInterferenceName, &vParams, 1);

	PostProcessUtils().DrawFullScreenTri(CRendererResources::s_ptexBackBuffer->GetWidth(), CRendererResources::s_ptexBackBuffer->GetHeight());

	PostProcessUtils().ShEndPass();
*/
}

/////////////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////////////////

void CScreenFrost::Render()
{
	ASSERT_LEGACY_PIPELINE
/*
	float fAmount = m_pAmount->GetParam();

	if (fAmount <= 0.02f)
	{
		m_fRandOffset = cry_random(0.0f, 1.0f);
		return;
	}

	gRenDev->m_cEF.mfRefreshSystemShader("PostEffectsGame", CShaderMan::s_shPostEffectsGame);

	float fCenterAmount = m_pCenterAmount->GetParam();

	PostProcessUtils().StretchRect(CRendererResources::s_ptexBackBuffer, CRendererResources::s_ptexBackBufferScaled[1]);

	//////////////////////////////////////////////////////////////////////////////////////////////////
	// display frost
	static CCryNameTSCRC pTechName("ScreenFrost");
	PostProcessUtils().ShBeginPass(CShaderMan::s_shPostEffectsGame, pTechName, FEF_DONTSETSTATES);

	gcpRendD3D->FX_SetState(GS_NODEPTHTEST);

	static CCryNameR pParam0Name("screenFrostParamsVS");
	static CCryNameR pParam1Name("screenFrostParamsPS");

	PostProcessUtils().ShSetParamVS(pParam0Name, Vec4(1, 1, 1, m_fRandOffset));
	PostProcessUtils().ShSetParamPS(pParam1Name, Vec4(1, 1, fCenterAmount, fAmount));

	PostProcessUtils().DrawFullScreenTri(CRendererResources::s_ptexBackBuffer->GetWidth(), CRendererResources::s_ptexBackBuffer->GetHeight());

	PostProcessUtils().ShEndPass();
*/
}

/////////////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////////////////

const char* CRainDrops::GetName() const
{
	return "RainDrops";
}

bool CRainDrops::Preprocess(const SRenderViewInfo& viewInfo)
{
	bool bQualityCheck = CPostEffectsMgr::CheckPostProcessQuality(eRQ_Medium, eSQ_Medium);
	if (!bQualityCheck)
		return false;

	if (CRenderer::CV_r_RainDropsEffect == 0)
		return false;

	bool bRainActive = IsActiveRain();

	if (m_bFirstFrame)
	{
		//initialize with valid value on 1st frame
		m_pPrevView = PostProcessUtils().m_pView;
	}

	if (CRenderer::CV_r_RainDropsEffect > 2)
		return true;

	return bRainActive;
}

bool CRainDrops::IsActiveRain()
{
	static float s_fLastSpawnTime = -1.0f;

	if (m_pAmount->GetParam() > 0.09f || m_nAliveDrops)
	{
		s_fLastSpawnTime = 0.0f;
		return true;
	}

	if (s_fLastSpawnTime == 0.0f)
	{
		s_fLastSpawnTime = PostProcessUtils().m_pTimer->GetCurrTime();
	}

	if (fabs(PostProcessUtils().m_pTimer->GetCurrTime() - s_fLastSpawnTime) < 1.0f)
	{
		return true;
	}
	return false;
}

void CRainDrops::SpawnParticle(SRainDrop*& pParticle, int iRTWidth, int iRTHeight)
{
	static SRainDrop pNewDrop;

	static float fLastSpawnTime = 0.0f;

	float fUserSize = 5.0f;//m_pSize->GetParam();
	float fUserSizeVar = 2.5f;//m_pSizeVar->GetParam();

	if (cry_random(0.0f, 1.0f) > 0.5f && fabs(PostProcessUtils().m_pTimer->GetCurrTime() - fLastSpawnTime) > m_pSpawnTimeDistance->GetParam())
	{
		pParticle->m_pPos.x = cry_random(0.0f, 1.0f);
		pParticle->m_pPos.y = cry_random(0.0f, 1.0f);

		pParticle->m_fLifeTime = pNewDrop.m_fLifeTime + pNewDrop.m_fLifeTimeVar * cry_random(-1.0f, 1.0f);
		//pParticle->m_fSize = (pNewDrop.m_fSize + pNewDrop.m_fSizeVar * cry_random(-1.0f, 1.0f)) * 2.5f;
		//pNewDrop.m_fSize + pNewDrop.m_fSizeVar
		pParticle->m_fSize = 1.0f / (10.0f * (fUserSize + 0.5f * fUserSizeVar * cry_random(-1.0f, 1.0f)));

		pParticle->m_pPos.x -= pParticle->m_fSize / (float)iRTWidth;
		pParticle->m_pPos.y -= pParticle->m_fSize / (float)iRTHeight;

		pParticle->m_fSpawnTime = PostProcessUtils().m_pTimer->GetCurrTime();
		pParticle->m_fWeight = 0.0f; // default weight to force rain drop to be stopped for a while

		fLastSpawnTime = PostProcessUtils().m_pTimer->GetCurrTime();
	}
	else
	{
		pParticle->m_fSize = 0.0f;
	}

}

void CRainDrops::UpdateParticles(int iRTWidth, int iRTHeight)
{
	ASSERT_LEGACY_PIPELINE
/*
	SRainDropsItor pItor, pItorEnd = m_pDropsLst.end();

	// Store camera parameters
	Vec3 vz = gRenDev->GetCamera().GetRenderVectorZ();    // front vec
	float fDot = vz.Dot(Vec3(0, 0, -1.0f));
	float fGravity = 1.0f - fabs(fDot);

	float fCurrFrameTime = 10.0f * gEnv->pTimer->GetFrameTime();

	bool bAllowSpawn(m_pAmount->GetParam() > 0.005f);

	m_nAliveDrops = 0;
	static int s_nPrevAliveDrops = 0;

	for (pItor = m_pDropsLst.begin(); pItor != pItorEnd; ++pItor)
	{
		SRainDrop* pCurr = (*pItor);

		float fCurrLifeTime = (PostProcessUtils().m_pTimer->GetCurrTime() - pCurr->m_fSpawnTime) / pCurr->m_fLifeTime;

		// particle died, spawn new
		if (fabs(fCurrLifeTime) > 1.0f || pCurr->m_fSize < 0.01f)
		{
			if (bAllowSpawn)
			{
				SpawnParticle(pCurr, iRTWidth, iRTHeight);
			}
			else
			{
				pCurr->m_fSize = 0.0f;
				continue;
			}
		}

		m_nAliveDrops++;

		// update position, etc

		// add gravity
		pCurr->m_pPos.y += m_pVelocityProj.y * cry_random(-0.2f, 1.0f);
		pCurr->m_pPos.y += fCurrFrameTime * fGravity * std::min(pCurr->m_fWeight, 0.5f * pCurr->m_fSize);
		// random horizontal movement and size + camera horizontal velocity
		pCurr->m_pPos.x += m_pVelocityProj.x * cry_random(-0.2f, 1.0f);
		pCurr->m_pPos.x += fCurrFrameTime * std::min(pCurr->m_fWeight, 0.25f * pCurr->m_fSize) * fGravity * cry_random(-1.0f, 1.0f);

		// Increase/decrease weight randomly
		pCurr->m_fWeight = clamp_tpl<float>(pCurr->m_fWeight + fCurrFrameTime * pCurr->m_fWeightVar * cry_random(-4.0f, 4.0f), 0.0f, 1.0f);
	}

	if (s_nPrevAliveDrops == 0 && m_nAliveDrops > 0)
		m_bFirstFrame = true;

	s_nPrevAliveDrops = m_nAliveDrops;
	*/
}

Matrix44 CRainDrops::ComputeCurrentView(int iViewportWidth, int iViewportHeight)
{
	ASSERT_LEGACY_PIPELINE
	return Matrix44();

/*
	Matrix44 pCurrView(PostProcessUtils().m_pView);
	Matrix44 pCurrProj(PostProcessUtils().m_pProj);

	Matrix33 pLerpedView;
	pLerpedView.SetIdentity();

	float fCurrFrameTime = gEnv->pTimer->GetFrameTime();
	// scale down speed a bit
	float fAlpha = iszero(fCurrFrameTime) ? 0.0f : .0005f / (fCurrFrameTime);

	// Interpolate matrixes and position
	pLerpedView = Matrix33(pCurrView) * (1 - fAlpha) + Matrix33(m_pPrevView) * fAlpha;

	Vec3 pLerpedPos = Vec3::CreateLerp(pCurrView.GetRow(3), m_pPrevView.GetRow(3), fAlpha);

	// Compose final 'previous' viewProjection matrix
	Matrix44 pLView = pLerpedView;
	pLView.m30 = pLerpedPos.x;
	pLView.m31 = pLerpedPos.y;
	pLView.m32 = pLerpedPos.z;

	m_pViewProjPrev = pLView * pCurrProj;
	m_pViewProjPrev.Transpose();

	// Compute camera velocity vector
	Vec3 vz = gRenDev->GetCamera().GetRenderVectorZ();    // front vec
	Vec3 vMoveDir = gRenDev->GetCamera().GetPosition() - vz;
	Vec4 vCurrPos = Vec4(vMoveDir.x, vMoveDir.y, vMoveDir.z, 1.0f);

	Matrix44 pViewProjCurr(PostProcessUtils().m_pViewProj);

	Vec4 pProjCurrPos = pViewProjCurr * vCurrPos;

	pProjCurrPos.x = ((pProjCurrPos.x + pProjCurrPos.w) * 0.5f + (1.0f / (float)iViewportWidth) * pProjCurrPos.w) / pProjCurrPos.w;
	pProjCurrPos.y = ((pProjCurrPos.w - pProjCurrPos.y) * 0.5f + (1.0f / (float)iViewportHeight) * pProjCurrPos.w) / pProjCurrPos.w;

	Vec4 pProjPrevPos = m_pViewProjPrev * vCurrPos;

	pProjPrevPos.x = ((pProjPrevPos.x + pProjPrevPos.w) * 0.5f + (1.0f / (float)iViewportWidth) * pProjPrevPos.w) / pProjPrevPos.w;
	pProjPrevPos.y = ((pProjPrevPos.w - pProjPrevPos.y) * 0.5f + (1.0f / (float)iViewportHeight) * pProjPrevPos.w) / pProjPrevPos.w;

	m_pVelocityProj = Vec3(pProjCurrPos.x - pProjPrevPos.x, pProjCurrPos.y - pProjPrevPos.y, 0);

	return pCurrView;
*/
}

void CRainDrops::Render()
{
	PROFILE_LABEL_SCOPE("RAIN_DROPS");

	ASSERT_LEGACY_PIPELINE

		/*

	gRenDev->m_cEF.mfRefreshSystemShader("PostEffectsGame", CShaderMan::s_shPostEffectsGame);

	int iViewportX, iViewportY, iViewportWidth, iViewportHeight;
	gcpRendD3D->GetViewport(&iViewportX, &iViewportY, &iViewportWidth, &iViewportHeight);

	Matrix44 pCurrView = ComputeCurrentView(iViewportWidth, iViewportHeight);

	uint16 uPrevDytex = (m_uCurrentDytex + 1) % 2;
	CTexture*& rpPrevDytex = CRendererResources::s_ptexRainDropsRT[uPrevDytex];
	assert(rpPrevDytex);
	CTexture*& rpCurrDytex = CRendererResources::s_ptexRainDropsRT[m_uCurrentDytex];
	assert(rpCurrDytex);
	int iRTWidth = rpCurrDytex->GetWidth();
	int iRTHeight = rpCurrDytex->GetHeight();

	UpdateParticles(iRTWidth, iRTHeight);

	gcpRendD3D->FX_PushRenderTarget(0, rpCurrDytex, NULL);
	gcpRendD3D->RT_SetViewport(0, 0, iRTWidth, iRTHeight);
	{
		ApplyExtinction(rpPrevDytex, iViewportWidth, iViewportHeight, iRTWidth, iRTHeight);
		DrawRaindrops(iViewportWidth, iViewportHeight, iRTWidth, iRTHeight);
	}

	gcpRendD3D->FX_PopRenderTarget(0);

	gcpRendD3D->RT_SetViewport(iViewportX, iViewportY, iViewportWidth, iViewportHeight);

	DrawFinal(rpCurrDytex);

	m_uCurrentDytex = (m_uCurrentDytex + 1) % 2;

	// store previous frame data
	m_pPrevView = pCurrView;
	m_bFirstFrame = false;
	*/
}

void CRainDrops::DrawRaindrops(int iViewportWidth, int iViewportHeight, int iRTWidth, int iRTHeight)
{
	ASSERT_LEGACY_PIPELINE
		/*
	PROFILE_LABEL_SCOPE("RAIN_DROPS_RAINDROPS");

	float fScreenW = iRTWidth;
	float fScreenH = iRTHeight;

	float fInvScreenW = 1.0f / fScreenW;
	float fInvScreenH = 1.0f / fScreenH;

	gcpRendD3D->Set2DMode(true, iRTWidth, iRTHeight);

	//clear and set render flags
	uint64 nSaveFlagsShader_RT = gRenDev->m_RP.m_FlagsShader_RT;
	gRenDev->m_RP.m_FlagsShader_RT &= ~(g_HWSR_MaskBit[HWSR_SAMPLE0] | g_HWSR_MaskBit[HWSR_SAMPLE1] | g_HWSR_MaskBit[HWSR_SAMPLE2]);

	//if( fDot >= - 0.25)
	{

		// render particles into effects rain map
		static CCryNameTSCRC pTech0Name("RainDropsGen");

		PostProcessUtils().ShBeginPass(CShaderMan::s_shPostEffectsGame, pTech0Name, FEF_DONTSETTEXTURES | FEF_DONTSETSTATES);

		gcpRendD3D->FX_SetState(GS_BLSRC_ONE | GS_BLDST_ONE | GS_NODEPTHTEST);  //additive

		// override blend operation from additive to max (max(src,dst) gets written)
		SStateBlend bl = gcpRendD3D->m_StatesBL[gcpRendD3D->m_nCurStateBL];
		bl.Desc.RenderTarget[0].BlendOp = D3D11_BLEND_OP_MAX;
		bl.Desc.RenderTarget[0].BlendOpAlpha = D3D11_BLEND_OP_MAX;
		gcpRendD3D->SetBlendState(&bl);

		SRainDropsItor pItor, pItorEnd = m_pDropsLst.end();
		for (pItor = m_pDropsLst.begin(); pItor != pItorEnd; ++pItor)
		{
			SRainDrop* pCurr = (*pItor);

			if (pCurr->m_fSize < 0.01f)
			{
				continue;
			}

			// render a sprite
			float x0 = pCurr->m_pPos.x * fScreenW;
			float y0 = pCurr->m_pPos.y * fScreenH;

			float x1 = (pCurr->m_pPos.x + pCurr->m_fSize * (fScreenH / fScreenW)) * fScreenW;
			float y1 = (pCurr->m_pPos.y + pCurr->m_fSize) * fScreenH;

			float fCurrLifeTime = (PostProcessUtils().m_pTimer->GetCurrTime() - pCurr->m_fSpawnTime) / pCurr->m_fLifeTime;
			Vec4 vRainParams = Vec4(1.0f, 1.0f, 1.0f, 1.0f - fCurrLifeTime);

			static CCryNameR pParam0Name("vRainParams");
			CShaderMan::s_shPostEffectsGame->FXSetPSFloat(pParam0Name, &vRainParams, 1);
			PostProcessUtils().DrawScreenQuad(256, 256, x0, y0, x1, y1);
		}

		// restore previous blend operation
		bl.Desc.RenderTarget[0].BlendOp = D3D11_BLEND_OP_ADD;
		bl.Desc.RenderTarget[0].BlendOpAlpha = D3D11_BLEND_OP_ADD;
		gcpRendD3D->SetBlendState(&bl);

		PostProcessUtils().ShEndPass();
	}

	gcpRendD3D->Set2DMode(false, 1, 1);

	gRenDev->m_RP.m_FlagsShader_RT = nSaveFlagsShader_RT;
	*/
}

void CRainDrops::ApplyExtinction(CTexture*& rptexPrevRT, int iViewportWidth, int iViewportHeight, int iRTWidth, int iRTHeight)
{
	ASSERT_LEGACY_PIPELINE
		/*
	//restore black texture to backbuffer for cleaning
	PostProcessUtils().CopyTextureToScreen(CRendererResources::s_ptexBlack);

	PROFILE_LABEL_SCOPE("RAIN_DROPS_EXTINCTION");
	if (!m_bFirstFrame)
	{
		// Store camera parameters
		Vec3 vx = gRenDev->GetCamera().GetRenderVectorX();    // up vec
		Vec3 vy = gRenDev->GetCamera().GetRenderVectorY();    // right vec
		Vec3 vz = gRenDev->GetCamera().GetRenderVectorZ();    // front vec
		float fDot = vz.Dot(Vec3(0, 0, -1.0f));
		float fGravity = (1.0f - fabs(fDot));
		float fFrameScale = 4.0f * gEnv->pTimer->GetFrameTime();
		Vec4 vRainNormalMapParams = Vec4(m_pVelocityProj.x * ((float)iViewportWidth), 0, fFrameScale * fGravity, fFrameScale * 1.0f + m_pVelocityProj.y * ((float)iViewportHeight)) * 0.25f;

		//texture to restore
		CTexture* pPrevTexture = rptexPrevRT;

		// apply extinction
		static CCryNameTSCRC pTech1Name("RainDropsExtinction");
		PostProcessUtils().ShBeginPass(CShaderMan::s_shPostEffectsGame, pTech1Name, FEF_DONTSETSTATES);   //FEF_DONTSETTEXTURES |

		//gcpRendD3D->FX_SetState(GS_NODEPTHTEST);

		static CCryNameR pParam1Name("vRainNormalMapParams");
		PostProcessUtils().ShSetParamVS(pParam1Name, vRainNormalMapParams);

		vRainNormalMapParams.w = fFrameScale;
		static CCryNameR pParam0Name("vRainParams");
		PostProcessUtils().ShSetParamPS(pParam0Name, vRainNormalMapParams);

		PostProcessUtils().SetTexture(pPrevTexture, 0, FILTER_LINEAR);

		PostProcessUtils().DrawFullScreenTri(iRTWidth, iRTHeight);

		PostProcessUtils().ShEndPass();
	}
	*/
}

void CRainDrops::DrawFinal(CTexture*& rptexCurrRT)
{
	PROFILE_LABEL_SCOPE("RAIN_DROPS_FINAL");

	ASSERT_LEGACY_PIPELINE
	/*

	//clear and set render flags
	uint64 nSaveFlagsShader_RT = gRenDev->m_RP.m_FlagsShader_RT;
	gRenDev->m_RP.m_FlagsShader_RT &= ~(g_HWSR_MaskBit[HWSR_SAMPLE0] | g_HWSR_MaskBit[HWSR_SAMPLE1] | g_HWSR_MaskBit[HWSR_SAMPLE2]);

	// display rain
	static CCryNameTSCRC pTechName("RainDropsFinal");
	PostProcessUtils().ShBeginPass(CShaderMan::s_shPostEffectsGame, pTechName, FEF_DONTSETTEXTURES | FEF_DONTSETSTATES);

	//gcpRendD3D->FX_SetState(GS_NODEPTHTEST);

	static CCryNameR pParam0Name("vRainNormalMapParams");
	PostProcessUtils().ShSetParamVS(pParam0Name, Vec4(1.0f, 1.0f, 1.0f, -1.0f));
	static CCryNameR pParam1Name("vRainParams");
	PostProcessUtils().ShSetParamPS(pParam1Name, Vec4(1.0f, 1.0f, 1.0f, 1.0f));

	PostProcessUtils().SetTexture(rptexCurrRT, 0, FILTER_LINEAR, eSamplerAddressMode_Mirror);
	PostProcessUtils().SetTexture(CRendererResources::s_ptexBackBuffer, 1, FILTER_LINEAR);

	PostProcessUtils().DrawFullScreenTri(CRendererResources::s_ptexBackBuffer->GetWidth(), CRendererResources::s_ptexBackBuffer->GetHeight());

	PostProcessUtils().ShEndPass();

	//reset render flags
	gRenDev->m_RP.m_FlagsShader_RT = nSaveFlagsShader_RT;
	*/
}

/////////////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////////////////

bool CFlashBang::Preprocess(const SRenderViewInfo& viewInfo)
{

	float fActive = m_pActive->GetParam();
	if (fActive || m_fSpawnTime)	{
		if (fActive)
			m_fSpawnTime = 0.0f;

		m_pActive->SetParam(0.0f);

		return true;
	}

	return false;
}

void CFlashBang::Render()
{
	ASSERT_LEGACY_PIPELINE
/*
	float fTimeDuration = m_pTime->GetParam();
	float fDifractionAmount = m_pDifractionAmount->GetParam();
	float fBlindTime = m_pBlindAmount->GetParam();

	if (!m_fSpawnTime)
	{
		m_fSpawnTime = PostProcessUtils().m_pTimer->GetCurrTime();

		// Create temporary ghost image and capture screen
		SAFE_DELETE(m_pGhostImage);

		m_pGhostImage = new SDynTexture(CRendererResources::s_ptexBackBuffer->GetWidth() >> 1, CRendererResources::s_ptexBackBuffer->GetHeight() >> 1, eTF_R8G8B8A8, eTT_2D, FT_STATE_CLAMP, "GhostImageTempRT");
		m_pGhostImage->Update(CRendererResources::s_ptexBackBuffer->GetWidth() >> 1, CRendererResources::s_ptexBackBuffer->GetHeight() >> 1);

		if (m_pGhostImage && m_pGhostImage->m_pTexture)
		{
			PostProcessUtils().StretchRect(CRendererResources::s_ptexBackBuffer, m_pGhostImage->m_pTexture);
		}
	}

	// Update current time
	float fCurrTime = (PostProcessUtils().m_pTimer->GetCurrTime() - m_fSpawnTime) / fTimeDuration;

	// Effect finished
	if (fCurrTime > 1.0f)
	{
		m_fSpawnTime = 0.0f;
		m_pActive->SetParam(0.0f);

		SAFE_DELETE(m_pGhostImage);

		return;
	}

	// make sure to update dynamic texture if required
	if (m_pGhostImage && !m_pGhostImage->m_pTexture)
	{
		m_pGhostImage->Update(CRendererResources::s_ptexBackBuffer->GetWidth() >> 1, CRendererResources::s_ptexBackBuffer->GetHeight() >> 1);
	}

	if (!m_pGhostImage || !m_pGhostImage->m_pTexture)
	{
		return;
	}

	gRenDev->m_cEF.mfRefreshSystemShader("PostEffectsGame", CShaderMan::s_shPostEffectsGame);

	//////////////////////////////////////////////////////////////////////////////////////////////////
	static CCryNameTSCRC pTechName("FlashBang");
	PostProcessUtils().ShBeginPass(CShaderMan::s_shPostEffectsGame, pTechName, FEF_DONTSETSTATES | FEF_DONTSETTEXTURES);

	gcpRendD3D->FX_SetState(GS_NODEPTHTEST);

	float fLuminance = 1.0f - fCurrTime;  //PostProcessUtils().InterpolateCubic(0.0f, 1.0f, 0.0f, 1.0f, fCurrTime);

	// opt: some pre-computed constants
	Vec4 vParams = Vec4(fLuminance, fLuminance * fDifractionAmount, 3.0f * fLuminance * fBlindTime, fLuminance);
	static CCryNameR pParamName("vFlashBangParams");
	CShaderMan::s_shPostEffectsGame->FXSetPSFloat(pParamName, &vParams, 1);

	PostProcessUtils().SetTexture(CRendererResources::s_ptexBackBuffer, 0, FILTER_POINT);
	PostProcessUtils().SetTexture(m_pGhostImage->m_pTexture, 1, FILTER_LINEAR);

	PostProcessUtils().DrawFullScreenTri(CRendererResources::s_ptexBackBuffer->GetWidth(), CRendererResources::s_ptexBackBuffer->GetHeight());

	PostProcessUtils().ShEndPass();
*/
}

////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////

void CFilterKillCamera::Render()
{
	ASSERT_LEGACY_PIPELINE
/*
	PROFILE_LABEL_SCOPE("KILL_CAMERA");

	gRenDev->m_cEF.mfRefreshSystemShader("PostEffectsGame", CShaderMan::s_shPostEffectsGame);

	float grainStrength = m_pGrainStrength->GetParam();
	Vec4 chromaShift = m_pChromaShift->GetParamVec4(); // xyz = offset, w = strength
	Vec4 vignette = m_pVignette->GetParamVec4();       // xy = screen scale, z = radius, w = blind noise vignette scale
	Vec4 colorScale = m_pColorScale->GetParamVec4();

	// Scale vignette with overscan borders
	Vec2 overscanBorders = Vec2(0.0f, 0.0f);
	gRenDev->EF_Query(EFQ_OverscanBorders, overscanBorders);
	const float vignetteOverscanMaxValue = 4.0f;
	const Vec2 vignetteOverscanScalar = Vec2(1.0f, 1.0f) + (overscanBorders * vignetteOverscanMaxValue);
	vignette.x *= vignetteOverscanScalar.x;
	vignette.y *= vignetteOverscanScalar.y;

	float inverseVignetteRadius = 1.0f / clamp_tpl<float>(vignette.z * 2.0f, 0.001f, 2.0f);
	Vec2 vignetteScreenScale(max(vignette.x, 0.0f), max(vignette.y, 0.0f));

	// Blindness
	Vec4 blindness = m_pBlindness->GetParamVec4(); // x = blind duration, y = blind fade out duration, z = blindness grey scale, w = blind noise min scale
	float blindDuration = max(blindness.x, 0.0f);
	float blindFadeOutDuration = max(blindness.y, 0.0f);
	float blindGreyScale = clamp_tpl<float>(blindness.z, 0.0f, 1.0f);
	float blindNoiseMinScale = clamp_tpl<float>(blindness.w, 0.0f, 10.0f);
	float blindNoiseVignetteScale = clamp_tpl<float>(vignette.w, 0.0f, 10.0f);

	float blindAmount = 0.0f;
	if (m_blindTimer < blindDuration)
	{
		blindAmount = 1.0f;
	}
	else
	{
		float blindFadeOutTimer = m_blindTimer - blindDuration;
		if (blindFadeOutTimer < blindFadeOutDuration)
		{
			blindAmount = 1.0f - (blindFadeOutTimer / blindFadeOutDuration);
		}
	}

	// Rendering
	PostProcessUtils().ShBeginPass(CShaderMan::s_shPostEffectsGame, m_techName, FEF_DONTSETSTATES);

	gcpRendD3D->FX_SetState(GS_NODEPTHTEST | GS_BLSRC_SRCALPHA | GS_BLDST_ONEMINUSSRCALPHA);

	int x = 0;
	int y = 0;
	int width = 0;
	int height = 0;
	gcpRendD3D->GetViewport(&x, &y, &width, &height);

	// Set PS default params
	const int PARAM_COUNT = 4;
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

	CShaderMan::s_shPostEffects->FXSetPSFloat(m_paramName, pParams, PARAM_COUNT);

	PostProcessUtils().DrawFullScreenTri(CRendererResources::s_ptexBackBuffer->GetWidth(), CRendererResources::s_ptexBackBuffer->GetHeight());

	PostProcessUtils().ShEndPass();
*/
}

////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////

enum ENanoGlassDebugView
{
	eNANO_GLASS_DEBUG_VIEW_Off = 0,
	eNANO_GLASS_DEBUG_VIEW_WireFrame
};

void CNanoGlass::Render()
{
	ASSERT_LEGACY_PIPELINE
/*
	// Update HUD Flash here so we can create a mask from it
	C3DHud* pHud3D = (C3DHud*) PostEffectMgr()->GetEffect(EPostEffectID::HUD3D);
	const bool bIsHudRendering = pHud3D->Preprocess(const SRenderViewInfo& viewInfo);
	if(bIsHudRendering && gcpRendD3D->m_nGraphicsPipeline == 0)
	{
		pHud3D->FlashUpdateRT();
	}

	PROFILE_LABEL_SCOPE("NANO_GLASS_FILTER");

	// Clear sample flags
	gRenDev->m_RP.m_FlagsShader_RT &= ~(g_HWSR_MaskBit[HWSR_SAMPLE0] |
	                                       g_HWSR_MaskBit[HWSR_SAMPLE1] |
	                                       g_HWSR_MaskBit[HWSR_SAMPLE2] |
	                                       g_HWSR_MaskBit[HWSR_SAMPLE4] |
	                                       g_HWSR_MaskBit[HWSR_DEBUG1] |
	                                       g_HWSR_MaskBit[HWSR_DEBUG2]);

	// Update RT pointers
	const int HUD_MASK_INDEX = 1;
	m_pHudMask = CRendererResources::s_ptexBackBufferScaled[HUD_MASK_INDEX];
	m_pHudMaskTemp = CRendererResources::s_ptexBackBufferScaledTemp[HUD_MASK_INDEX];

	DownSampleBackBuffer();

	if (bIsHudRendering)
	{
		CreateHudMask(pHud3D);
	}

	bool bDebugPass = false;
	RenderPass(bDebugPass, bIsHudRendering);

#ifndef _RELEASE
	if (CRenderer::CV_r_PostProcessNanoGlassDebugView)
	{
		bDebugPass = true;
		RenderPass(bDebugPass, bIsHudRendering);
	}
#endif
*/
}

void CNanoGlass::DownSampleBackBuffer()
{
	ASSERT_LEGACY_PIPELINE
/*
	// Only need downsample when using back buffer as a brightness scalar
	const float backBufferBrightessScalar = m_pFilter->GetParamVec4().x;
	if (backBufferBrightessScalar > 0.0f)
	{
		PROFILE_LABEL_SCOPE("DOWN_SAMPLE_BACK_BUFFER");

		// Optimized platform specific down sampling of texture
		const bool bClearAlpha = false;
		const bool bDecodeSrcRGBK = false;
		const bool bEncodeDstRGBK = false;
		const bool bBigDownSample = true;
		GetUtils().StretchRect(CRendererResources::s_ptexBackBuffer, CRendererResources::s_ptexBackBufferScaled[2], bClearAlpha, bDecodeSrcRGBK, bEncodeDstRGBK, bBigDownSample);
	}
*/
}

void CNanoGlass::CreateHudMask(CHud3D* pHud3D)
{
	ASSERT_LEGACY_PIPELINE
/*
	PROFILE_LABEL_SCOPE("CREATE_HUD_MASK");

	// Render HUD
	gRenDev->m_RP.m_FlagsShader_RT |= g_HWSR_MaskBit[HWSR_SAMPLE1];

	CD3DStereoRenderer& pS3DRend = gcpRendD3D->GetS3DRend();
	bool bPostProcStereo = pS3DRend.IsPostStereoEnabled();

	if (bPostProcStereo && CRenderer::CV_r_StereoHudScreenDist)
	{
		// Not perfect S3D solution, as mask will be slightly larger, but looks ok and is cheap
		// Ideally we want to render the NanoGlass twice (once for each eye) - but we don't have the budget!
		const float maxParallax = 0.005f * pS3DRend.GetStereoStrength();

		// Render left eye
		pHud3D->SetMaxParallax(-maxParallax);
		pHud3D->UpdateBloomRT(m_pHudMask, NULL);

		// Render right eye
		pHud3D->SetMaxParallax(maxParallax);
		pHud3D->UpdateBloomRT(m_pHudMask, NULL);

		pHud3D->SetMaxParallax(0);
	}
	else
	{
		pHud3D->UpdateBloomRT(m_pHudMask, NULL);
	}

	gRenDev->m_RP.m_FlagsShader_RT &= ~g_HWSR_MaskBit[HWSR_SAMPLE1];

	// Blur
	const int blurIterationCount = 2;

	const bool bDilate = false;
	GetUtils().TexBlurIterative(m_pHudMask, blurIterationCount, bDilate, m_pHudMaskTemp);
*/
}

void CNanoGlass::RenderPass(bool bDebugPass, bool bIsHudRendering)
{
	ASSERT_LEGACY_PIPELINE
/*

	// Set PS default params
	const int PARAM_COUNT = 7;
	Vec4 psParams[PARAM_COUNT];
	Vec4 filter = m_pFilter->GetParamVec4(); // x = backBufferBrightessScalar, y = mistAlpha, z = noiseStrength, w = overChargeStrength
	float overChargeStrength = filter.w;
	float backBufferBrightessScalar = filter.x;
	float refractionBlurScale = filter.w;
	float effectAlpha = m_pEffectAlpha->GetParam();
	float introAnimPos = m_pStartAnimPos->GetParam();
	float outroAnimPos = m_pEndAnimPos->GetParam();
	float brightness = m_pBrightness->GetParam();
	Vec4 movement0 = m_pMovement[0]->GetParamVec4();
	Vec4 movement1 = m_pMovement[1]->GetParamVec4();
	Vec4 vignette = m_pVignette->GetParamVec4(); // xy = vignetteScreenScale , z = mistSinStrength, w = free
	Vec4 hexColor = m_pHexColor->GetParamVec4();
	float hitStrength = hexColor.w;

	// Calc animPos
	const float animScale = 1.5f;
	const float animPos = (1.0 - clamp_tpl<float>((introAnimPos - outroAnimPos), 0.0f, 1.0f)) * animScale;

	// Calc vignette
	Vec2 vignetteScreenScale(max(vignette.x, 0.0f), max(vignette.y, 0.0f));

	// Update time (Use UI time as this is animated during menu)
	float frameTime = GetUtils().m_pTimer->GetFrameTime(ITimer::ETIMER_UI);
	m_noiseTime += frameTime * movement1.z;
	m_time += frameTime * movement0.x;

	// Calc mist
	float mistSinStrength = vignette.z;
	float mistAlpha = filter.y;
	float mistScale = fabs_tpl(sin_tpl(m_time)) * mistSinStrength;
	mistAlpha -= clamp_tpl<float>(((mistScale * mistScale * mistScale) * mistAlpha), 0.0f, 1.0f);

	// Corner Glow
	Vec4 cornerGlow = m_pCornerGlow->GetParamVec4();
	float minCornerGlow = cornerGlow.x;
	float maxCornerGlow = cornerGlow.y;
	float maxGlowRandTime = cornerGlow.z;
	m_cornerGlowCountdownTimer -= frameTime;
	if (m_cornerGlowCountdownTimer < 0.0f)
	{
		m_cornerGlowCountdownTimer = cry_random(0.0f, maxGlowRandTime);
		m_cornerGlowStrength = minCornerGlow + LERP(minCornerGlow, maxCornerGlow, cry_random(0.0f, 1.0f));
	}

	// Noise Thresh
	float noiseThresh = 0.83f;
	float overChargeNoiseThresh = 0.83f;
	float hitStrengthNoiseThresh = 0.79f;
	float overChargeHitStrengthNoiseThresh = 0.79f;
	float finalNoiseThresh = LERP(noiseThresh, overChargeNoiseThresh, overChargeStrength);
	float finalHitNoiseThresh = LERP(hitStrengthNoiseThresh, overChargeHitStrengthNoiseThresh, overChargeStrength);
	noiseThresh = LERP(finalNoiseThresh, finalHitNoiseThresh, hitStrength);

	// psParams[0] - x = hexTexScale y = backBufferBrightessScalar, z = vignetteTexOffset, w = vignetteSaturation
	psParams[0] = m_pHexParams->GetParamVec4();
	psParams[0].y = backBufferBrightessScalar;

	// psParams[1] - x = animPos, y = noiseThresh, z = time, w = brightness
	psParams[1].x = animPos;
	psParams[1].y = noiseThresh;
	psParams[1].z = m_time;
	psParams[1].w = brightness;

	// psParams[2] - x = noiseTime, y = movementWaveStrength, z = vignetteFallOffScale, w = movementWaveFrequency
	const float waveStrengthScale = 0.1f; // Use scalar here so post effect param is between 0 and 1
	psParams[2] = movement0;
	psParams[2].x = m_noiseTime;
	psParams[2].y *= waveStrengthScale;

	// psParams[3] - x = movementStrengthX, y = movementStrengthY, z = noiseStrength, w = vignetteStrength
	const float noiseSpeedScale = 0.05f; // Use scalar here so post effect param is between 0 and 1
	psParams[3] = movement1;
	psParams[3].z = filter.z;

	// psParams[4] - x = effectAlpha, y = mistAlpha, zw = vignetteScreenScale
	psParams[4].x = effectAlpha;
	psParams[4].y = mistAlpha;
	psParams[4].z = vignetteScreenScale.x;
	psParams[4].w = vignetteScreenScale.y;

	// psParams[5] - xyz = hex Color, w = hitStrength
	psParams[5] = hexColor;
	psParams[5].w = hitStrength;

	// psParams[6] - x = overChargeStrength, y = cornerGlowStrength, zw = free
	psParams[6].x = overChargeStrength;
	psParams[6].y = m_cornerGlowStrength * overChargeStrength;
	psParams[6].z = 0.0f;
	psParams[6].w = 0.0f;

	// Setup shader
	gRenDev->m_cEF.mfRefreshSystemShader("PostEffectsGame", CShaderMan::s_shPostEffectsGame);
	CShaderMan::s_shPostEffects->FXSetPSFloat(m_paramName, psParams, PARAM_COUNT);

	const uint32 nThreadID = gRenDev->GetRenderThreadID();
	SShaderTechnique* pShaderTech = CShaderMan::s_shPostEffectsGame->mfFindTechnique(m_shaderTechName);
	SRenderData& pRenderData = m_pRenderData[nThreadID];

	if (pRenderData.pRenderElement && pShaderTech)
	{
		// Draw 3d mesh in 2d

		// Set scissor
		gcpRendD3D->FX_Commit();
		gcpRendD3D->EF_Scissor(true, 0, 0, gcpRendD3D->GetWidth(), gcpRendD3D->GetHeight());

		// Render state
		int32 nRenderState = GS_NODEPTHTEST | GS_BLSRC_SRCALPHA | GS_BLDST_ONEMINUSSRCALPHA;

		// Set intro flag
		const bool bIntroPlaying = ((introAnimPos > 0.0f) && (introAnimPos < 1.0f)) ? true : false;
		const bool bOutroPlaying = ((outroAnimPos > 0.0f) && (outroAnimPos < 1.0f)) ? true : false;
		const bool bAnimPlaying = bIntroPlaying || bOutroPlaying;
		if (bAnimPlaying)
		{
			gRenDev->m_RP.m_FlagsShader_RT |= g_HWSR_MaskBit[HWSR_SAMPLE0];
		}

		// Set overcharge strength
		if (overChargeStrength > 0.0f)
		{
			gRenDev->m_RP.m_FlagsShader_RT |= g_HWSR_MaskBit[HWSR_SAMPLE1];
		}

		// Set movement wave flag
		if (psParams[2].y > 0.0f)
		{
			gRenDev->m_RP.m_FlagsShader_RT |= g_HWSR_MaskBit[HWSR_SAMPLE2];
		}

		// Set back buffer brightness scalar flag
		const bool bUsingBackBuffer = (backBufferBrightessScalar > 0.0f) ? true : false;
		if (bUsingBackBuffer)
		{
			gRenDev->m_RP.m_FlagsShader_RT |= g_HWSR_MaskBit[HWSR_SAMPLE4];
		}

#ifndef _RELEASE
		// Turn on debug view in shader
		if (bDebugPass && CRenderer::CV_r_PostProcessNanoGlassDebugView)
		{
			gRenDev->m_RP.m_FlagsShader_RT |= g_HWSR_MaskBit[HWSR_DEBUG1];
			if (CRenderer::CV_r_PostProcessNanoGlassDebugView == eNANO_GLASS_DEBUG_VIEW_WireFrame)
			{
				nRenderState |= GS_WIREFRAME;
			}
		}
#endif

		GetUtils().ShBeginPass(CShaderMan::s_shPostEffectsGame, m_shaderTechName, FEF_DONTSETTEXTURES | FEF_DONTSETSTATES);

		// Set view proj matrix
		Matrix44A mObjCurr, mViewProj;
		C3DHud* p3DHUD = reinterpret_cast<C3DHud*>(PostEffectMgr()->GetEffect(EPostEffectID::HUD3D));
		float fHUDFov = 55.0f;  // Safe default
		if (p3DHUD)
		{
			CEffectParam* pFov = p3DHUD->GetFov();
			if (pFov)
			{
				fHUDFov = clamp_tpl<float>(pFov->GetParam(), 1.0f, 180.0f);
			}
		}

		// Precalculate the pixel aspect ratio for the nano glass on PC so as to better fit it to the screen
		float fBackBufferRatio = (float)CRendererResources::s_ptexBackBuffer->GetWidth() / (float)CRendererResources::s_ptexBackBuffer->GetHeight();
		float f16_9 = 16.0f / 9.0f;
		float fInvPAR = f16_9 / fBackBufferRatio;

		// Patch projection matrix to have HUD FOV
		mViewProj = gRenDev->m_ProjMatrix;

		// Apply overscan border aspect ratio
		const float aspect = GetUtils().GetOverscanBorderAspectRatio() * fInvPAR;

		float h = 1 / tanf(DEG2RAD(fHUDFov / 2));
		mViewProj.m00 = h / aspect;
		mViewProj.m11 = h;

		// Render in camera space to remove precision bugs
		const bool bCameraSpace = (pRenderData.pRenderObject->m_ObjFlags & FOB_NEAREST) ? true : false;
		Matrix44A mView;
		GetUtils().GetViewMatrix(mView, bCameraSpace);

		mViewProj = mView * mViewProj;
		mObjCurr = pRenderData.pRenderObject->m_II.m_Matrix.GetTransposed();
		mViewProj = mObjCurr * mViewProj;
		mViewProj.Transpose();

		CShaderMan::s_shPostEffectsGame->FXSetVSFloat(m_viewProjMatrixParamName, (Vec4*) mViewProj.GetData(), 4);

		// Set render state
		gcpRendD3D->FX_SetState(nRenderState);
		gcpRendD3D->SetCullMode(R_CULL_BACK);

		// Set ps params
		CShaderMan::s_shPostEffects->FXSetPSFloat(m_paramName, psParams, PARAM_COUNT);

		// Set textures
		CTexture* pHudMaskTex = bIsHudRendering ? m_pHudMask : CRendererResources::s_ptexBlackAlpha;

		GetUtils().SetTexture(m_pHexOutline, 0, FILTER_LINEAR, eSamplerAddressMode_Mirror);
		GetUtils().SetTexture(pHudMaskTex, 1, FILTER_LINEAR, eSamplerAddressMode_Clamp, true);
		GetUtils().SetTexture(m_pNoise, 2, FILTER_LINEAR, eSamplerAddressMode_Wrap);

		if (bUsingBackBuffer)
		{
			GetUtils().SetTexture(CRendererResources::s_ptexBackBufferScaled[2], 3, FILTER_LINEAR, eSamplerAddressMode_Clamp);
		}

		if (bAnimPlaying)
		{
			GetUtils().SetTexture(m_pHexRand, 4, FILTER_LINEAR, eSamplerAddressMode_Wrap);
			GetUtils().SetTexture(m_pHexGrad, 5, FILTER_LINEAR, eSamplerAddressMode_Wrap);
		}

		// Draw mesh
		CD3D9Renderer* const __restrict rd = gcpRendD3D;
		CREMeshImpl* pRenderMesh = (CREMeshImpl*) pRenderData.pRenderElement;

		// Create/Update vertex buffer stream
		if (pRenderMesh->m_pRenderMesh)
			pRenderMesh->m_pRenderMesh->CheckUpdate(pRenderMesh->m_pRenderMesh->_GetVertexFormat(), 0);

		gcpRendD3D->m_RP.m_pRE = const_cast<CRenderElement*>(pRenderData.pRenderElement);
		if (gcpRendD3D->FX_CommitStreams(&pShaderTech->m_Passes[0], true))
		{
			gRenDev->m_RP.m_FirstVertex = pRenderMesh->m_nFirstVertId;
			gRenDev->m_RP.m_FirstIndex = pRenderMesh->m_nFirstIndexId;
			gRenDev->m_RP.m_RendNumIndices = pRenderMesh->m_nNumIndices;
			gRenDev->m_RP.m_RendNumVerts = pRenderMesh->m_nNumVerts;
			gRenDev->m_RP.m_pRE->mfDraw(CShaderMan::s_shPostEffectsGame, &pShaderTech->m_Passes[0]);
		}

		gcpRendD3D->EF_Scissor(false, 0, 0, 0, 0);

		GetUtils().ShEndPass();
	}
*/
}

/////////////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////////////////

void CScreenBlood::Render()
{
	ASSERT_LEGACY_PIPELINE
/*
	PROFILE_LABEL_SCOPE("SCREEN BLOOD");

	static CCryNameTSCRC pTechName("ScreenBlood");
	GetUtils().ShBeginPass(CShaderMan::s_shPostEffectsGame, pTechName, FEF_DONTSETSTATES);
	gcpRendD3D->FX_SetState(GS_NODEPTHTEST | GS_BLSRC_DSTCOL | GS_BLDST_SRCALPHA);

	// Border params
	const Vec4 borderParams = m_pBorder->GetParamVec4();
	const float borderRange = borderParams.z;
	const Vec2 borderOffset = Vec2(borderParams.x, borderParams.y);
	const float alpha = borderParams.w;

	// Use overscan borders to scale blood thickness around screen
	const float overscanScalar = 3.0f;
	Vec2 overscanBorders = Vec2(0.0f, 0.0f);
	gcpRendD3D->EF_Query(EFQ_OverscanBorders, overscanBorders);
	overscanBorders = Vec2(1.0f, 1.0f) + ((overscanBorders + borderOffset) * overscanScalar);

	const float borderScale = max(0.2f, borderRange - (m_pAmount->GetParam() * borderRange));
	Vec4 pParams = Vec4(overscanBorders.x, overscanBorders.y, alpha, borderScale);
	static CCryNameR pParamName("psParams");
	CShaderMan::s_shPostEffects->FXSetPSFloat(pParamName, &pParams, 1);
	GetUtils().DrawFullScreenTri(CRendererResources::s_ptexBackBuffer->GetWidth(), CRendererResources::s_ptexBackBuffer->GetHeight());

	GetUtils().ShEndPass();

	//m_nRenderFlags = PSP_UPDATE_BACKBUFFER;
*/
}

/////////////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////////////////

void CScreenFader::Render()
{
	ASSERT_LEGACY_PIPELINE
}

////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////

#pragma warning(pop)
