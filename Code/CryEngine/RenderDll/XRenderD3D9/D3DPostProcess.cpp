// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

/*=============================================================================
   D3DPostProcess : Direct3D specific post processing special effects

   Revision history:
* 23/02/2005: Re-factored/Converted to CryEngine 2.0 by Tiago Sousa
* Created by Tiago Sousa

   =============================================================================*/

#include "StdAfx.h"
#include "DriverD3D.h"
#include <Cry3DEngine/I3DEngine.h>
#include "D3DPostProcess.h"

#if CRY_PLATFORM_WINDOWS
	#include <CryInput/IHardwareMouse.h>
#endif

SD3DPostEffectsUtils SD3DPostEffectsUtils::m_pInstance;

////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////

void SD3DPostEffectsUtils::ResolveRT(CTexture*& pDst, const RECT* pSrcRect)
{
	ASSERT_LEGACY_PIPELINE
	assert(pDst);
	if (!pDst)
		return;
	/*
	CRenderDisplayContext* pActiveContext = gcpRendD3D.GetActiveDisplayContext();
	CTexture* pSrc = pActiveContext->GetColorOutput();
	if (pSrc)
	{
		RECT defaultRect;
		if (!pSrcRect)
		{
			defaultRect.left = 0;
			defaultRect.top = 0;
			defaultRect.right = min(pDst->GetWidth(), pActiveContext->GetResolution().x);
			defaultRect.bottom = min(pDst->GetHeight(), pActiveContext->GetResolution().y);

			pSrcRect = &defaultRect;
		}

		const SResourceRegionMapping region =
		{
			{ 0, 0, 0, 0 },
			{ pSrcRect->left, pSrcRect->top, 0, 0 },
			{ pSrcRect->right - pSrcRect->left, pSrcRect->bottom - pSrcRect->top, 1, 1 }
		};

		HRESULT hr = 0;
		SRenderStatistics::Write().m_RTCopied++;
		SRenderStatistics::Write().m_RTCopiedSize += pDst->GetDeviceDataSize();

		GetDeviceObjectFactory().GetCoreCommandList().GetCopyInterface()->Copy(pSrc->GetDevTexture(), pDst->GetDevTexture(), region);
	}
	*/
}

void SD3DPostEffectsUtils::CopyTextureToScreen(CTexture*& pSrc, const RECT* pSrcRegion, const int filterMode)
{
	ASSERT_LEGACY_PIPELINE
/*
	static CCryNameTSCRC pRestoreTechName("TextureToTexture");
	PostProcessUtils().ShBeginPass(CShaderMan::s_shPostEffects, pRestoreTechName, FEF_DONTSETTEXTURES | FEF_DONTSETSTATES);
	gRenDev->FX_SetState(GS_NODEPTHTEST);
	PostProcessUtils().SetTexture(pSrc, 0, filterMode >= 0 ? filterMode : FILTER_POINT);
	PostProcessUtils().DrawFullScreenTri(pSrc->GetWidth(), pSrc->GetHeight(), 0, pSrcRegion);
	PostProcessUtils().ShEndPass();
*/
}

void SD3DPostEffectsUtils::CopyScreenToTexture(CTexture*& pDst, const RECT* pSrcRegion)
{
	ResolveRT(pDst, pSrcRegion);
}

////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////

void SD3DPostEffectsUtils::StretchRect(CTexture* pSrc, CTexture*& pDst, bool bClearAlpha, bool bDecodeSrcRGBK, bool bEncodeDstRGBK, bool bBigDownsample, EDepthDownsample depthDownsample, bool bBindMultisampled, const RECT* srcRegion)
{
	if (!pSrc || !pDst)
	{
		return;
	}

	PROFILE_LABEL_SCOPE("STRETCHRECT");

	// OLD PIPELINE
	ASSERT_LEGACY_PIPELINE
}
////////////////////////////////////////////////////////////////////////////////////////////////////

/////////////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////////////////

void SD3DPostEffectsUtils::Downsample(CTexture* pSrc, CTexture* pDst, int nSrcW, int nSrcH, int nDstW, int nDstH, EFilterType eFilter /*= FilterType_Box*/, bool bSetTarget /*= true*/)
{
	ASSERT_LEGACY_PIPELINE
}

////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////

void CD3D9Renderer::GetReprojectionMatrix(Matrix44A& matReproj,
                                          const Matrix44A& matView,
                                          const Matrix44A& matProj,
                                          const Matrix44A& matPrevView,
                                          const Matrix44A& matPrevProj,
                                          float fFarPlane) const
{
	// Current camera screen-space to projection-space
	const Matrix44A matSc2Pc
	  (2.f, 0.f, -1.f, 0.f,
	  0.f, 2.f, -1.f, 0.f,
	  0.f, 0.f, 1.f, 0.f,
	  0.f, 0.f, 0.f, 1.f / fFarPlane);

	// Current camera view-space to projection-space
	const Matrix44A matVc2Pc
	  (matProj.m00, 0.f, 0.f, 0.f,
	  0.f, matProj.m11, 0.f, 0.f,
	  0.f, 0.f, 1.f, 0.f,
	  0.f, 0.f, 0.f, 1.f);

	// Current camera projection-space to world-space
	const Matrix44A matPc2Wc = (matVc2Pc * matView).GetInverted();

	// Previous camera view-space to projection-space
	const Matrix44A matVp2Pp
	  (matPrevProj.m00, 0.f, 0.f, 0.f,
	  0.f, matPrevProj.m11, 0.f, 0.f,
	  0.f, 0.f, 1.f, 0.f,
	  0.f, 0.f, 0.f, 1.f);

	// Previous camera world-space to projection-space
	const Matrix44A matWp2Pp = matVp2Pp * matPrevView;

	// Previous camera projection-space to texture-space
	const Matrix44A matPp2Tp
	  (0.5f, 0.f, 0.5f, 0.f,
	  0.f, 0.5f, 0.5f, 0.f,
	  0.f, 0.f, 1.f, 0.f,
	  0.f, 0.f, 0.f, 1.f);

	// Final reprojection matrix (from current camera screen-space to previous camera texture-space)
	matReproj = matPp2Tp * matWp2Pp * matPc2Wc * matSc2Pc;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////////////////

void CPostEffectsMgr::Begin()
{
	PostProcessUtils().Log("### POST-PROCESSING BEGINS ### ");
	PostProcessUtils().m_pTimer = gEnv->pTimer;
	static EShaderQuality nPrevShaderQuality = eSQ_Low;
	static ERenderQuality nPrevRenderQuality = eRQ_Low;

	EShaderQuality nShaderQuality = (EShaderQuality) gcpRendD3D->EF_GetShaderQuality(eST_PostProcess);
	ERenderQuality nRenderQuality = gRenDev->EF_GetRenderQuality();
	if (nPrevShaderQuality != nShaderQuality || nPrevRenderQuality != nRenderQuality)
	{
		CPostEffectsMgr::Reset(true);
		nPrevShaderQuality = nShaderQuality;
		nPrevRenderQuality = nRenderQuality;
	}

	//gcpRendD3D->RT_SetViewport(0, 0, gcpRendD3D->GetWidth(), gcpRendD3D->GetHeight());

	SPostEffectsUtils::m_fWaterLevel = gRenDev->m_p3DEngineCommon.m_OceanInfo.m_fWaterLevel;

	PostProcessUtils().UpdateOverscanBorderAspectRatio();
}

void CPostEffectsMgr::End(CRenderView* pRenderView)
{
	//gcpRendD3D->RT_SetViewport(PostProcessUtils().m_pScreenRect.left, PostProcessUtils().m_pScreenRect.top, PostProcessUtils().m_pScreenRect.right, PostProcessUtils().m_pScreenRect.bottom);
	const int kFloatMaxContinuousInt = 0x1000000;  // 2^24
	const bool bStereo = gcpRendD3D->GetS3DRend().IsStereoEnabled();
	const bool bStereoSequentialSubmission = gcpRendD3D->GetS3DRend().GetStereoSubmissionMode() == EStereoSubmission::STEREO_SUBMISSION_SEQUENTIAL;

	if (!bStereo || (bStereo && (!bStereoSequentialSubmission || pRenderView->GetCurrentEye() == CCamera::eEye_Right)))
		SPostEffectsUtils::m_iFrameCounter = (SPostEffectsUtils::m_iFrameCounter + 1) % kFloatMaxContinuousInt;

	PostProcessUtils().Log("### POST-PROCESSING ENDS ### ");
}
