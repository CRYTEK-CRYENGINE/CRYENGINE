// Copyright 2001-2016 Crytek GmbH / Crytek Group. All rights reserved.

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

/////////////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////////////////

SDepthTexture* SD3DPostEffectsUtils::GetDepthSurface(CTexture* pTex)
{
	return &gcpRendD3D->m_DepthBufferOrig;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////

void SD3DPostEffectsUtils::ResolveRT(CTexture*& pDst, const RECT* pSrcRect)
{
	assert(pDst);
	if (!pDst)
		return;

	CTexture* pSrc = gcpRendD3D->m_pNewTarget[0]->m_pTex;
	if (pSrc)
	{
		assert(pSrc->GetDevTexture()->LookupRTV(EDefaultResourceViews::RenderTarget) == gcpRendD3D->m_pNewTarget[0]->m_pTarget);

		RECT defaultRect;
		if (!pSrcRect)
		{
			defaultRect.left = 0;
			defaultRect.top = 0;
			defaultRect.right = min(pDst->GetWidth(), gcpRendD3D->m_pNewTarget[0]->m_Width);
			defaultRect.bottom = min(pDst->GetHeight(), gcpRendD3D->m_pNewTarget[0]->m_Height);

			pSrcRect = &defaultRect;
		}

		const SResourceRegionMapping region =
		{
			{ 0, 0, 0, 0 },
			{ pSrcRect->left, pSrcRect->top, 0, 0 },
			{ pSrcRect->right - pSrcRect->left, pSrcRect->bottom - pSrcRect->top, 1, 1 }
		};

		HRESULT hr = 0;
		gcpRendD3D->m_RP.m_PS[gcpRendD3D->m_RP.m_nProcessThreadID].m_RTCopied++;
		gcpRendD3D->m_RP.m_PS[gcpRendD3D->m_RP.m_nProcessThreadID].m_RTCopiedSize += pDst->GetDeviceDataSize();

		GetDeviceObjectFactory().GetCoreCommandList().GetCopyInterface()->Copy(pSrc->GetDevTexture(), pDst->GetDevTexture(), region);
	}
}

void SD3DPostEffectsUtils::CopyTextureToScreen(CTexture*& pSrc, const RECT* pSrcRegion, const int filterMode)
{
	static CCryNameTSCRC pRestoreTechName("TextureToTexture");
	PostProcessUtils().ShBeginPass(CShaderMan::s_shPostEffects, pRestoreTechName, FEF_DONTSETTEXTURES | FEF_DONTSETSTATES);
	gRenDev->FX_SetState(GS_NODEPTHTEST);
	PostProcessUtils().SetTexture(pSrc, 0, filterMode >= 0 ? filterMode : FILTER_POINT);
	PostProcessUtils().DrawFullScreenTri(pSrc->GetWidth(), pSrc->GetHeight(), 0, pSrcRegion);
	PostProcessUtils().ShEndPass();
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

	uint64 nSaveFlagsShader_RT = gRenDev->m_RP.m_FlagsShader_RT;
	gRenDev->m_RP.m_FlagsShader_RT &= ~(g_HWSR_MaskBit[HWSR_SAMPLE0] | g_HWSR_MaskBit[HWSR_SAMPLE1] | g_HWSR_MaskBit[HWSR_SAMPLE2] | g_HWSR_MaskBit[HWSR_SAMPLE4] | g_HWSR_MaskBit[HWSR_SAMPLE5] | g_HWSR_MaskBit[HWSR_REVERSE_DEPTH]);

	// Get current viewport
	int iTempX, iTempY, iWidth, iHeight;
	gRenDev->GetViewport(&iTempX, &iTempY, &iWidth, &iHeight);
	bool bResample = 0;

	if (pSrc->GetWidth() != pDst->GetWidth() || pSrc->GetHeight() != pDst->GetHeight())
	{
		bResample = 1;
	}

	const D3DFormat dstFmt = DeviceFormats::ConvertFromTexFormat(pDst->GetDstFormat());
	const D3DFormat srcFmt = DeviceFormats::ConvertFromTexFormat(pSrc->GetDstFormat());
	if (bResample == false && gRenDev->m_RP.m_FlagsShader_RT == 0 && dstFmt == srcFmt)
	{
		GetDeviceObjectFactory().GetCoreCommandList().GetCopyInterface()->Copy(pSrc->GetDevTexture(), pDst->GetDevTexture());
		gRenDev->m_RP.m_FlagsShader_RT = nSaveFlagsShader_RT;
		return;
	}

	gcpRendD3D->FX_PushRenderTarget(0, pDst, NULL);
	gcpRendD3D->FX_SetActiveRenderTargets();
	gcpRendD3D->RT_SetViewport(0, 0, pDst->GetWidth(), pDst->GetHeight());

	bool bEnableRTSample0 = bBindMultisampled;

	if (bEnableRTSample0)
		gRenDev->m_RP.m_FlagsShader_RT |= g_HWSR_MaskBit[HWSR_SAMPLE0];

	if (bClearAlpha)     // clear alpha to 0
		gRenDev->m_RP.m_FlagsShader_RT |= g_HWSR_MaskBit[HWSR_SAMPLE1];
	if (bDecodeSrcRGBK)  // decode RGBK src texture
		gRenDev->m_RP.m_FlagsShader_RT |= g_HWSR_MaskBit[HWSR_SAMPLE2];
	if (depthDownsample != eDepthDownsample_None)  // take minimum/maximum depth from the 4 samples
	{
		gRenDev->m_RP.m_FlagsShader_RT |= g_HWSR_MaskBit[HWSR_SAMPLE4];
		if (depthDownsample == eDepthDownsample_Min)
			gRenDev->m_RP.m_FlagsShader_RT |= g_HWSR_MaskBit[HWSR_REVERSE_DEPTH];
	}
	if (bEncodeDstRGBK)  // encode RGBK dst texture
		gRenDev->m_RP.m_FlagsShader_RT |= g_HWSR_MaskBit[HWSR_SAMPLE5];

	static CCryNameTSCRC pTechTexToTex("TextureToTexture");
	static CCryNameTSCRC pTechTexToTexResampled("TextureToTextureResampled");
	ShBeginPass(CShaderMan::s_shPostEffects, bResample ? pTechTexToTexResampled : pTechTexToTex, FEF_DONTSETTEXTURES | FEF_DONTSETSTATES);

	gRenDev->FX_SetState(GS_NODEPTHTEST);

	// Get sample size ratio (based on empirical "best look" approach)
	float fSampleSize = ((float)pSrc->GetWidth() / (float)pDst->GetWidth()) * 0.5f;

	// Set samples position
	//float s1 = fSampleSize / (float) pSrc->GetWidth();  // 2.0 better results on lower res images resizing
	//float t1 = fSampleSize / (float) pSrc->GetHeight();

	CTexture* pOffsetTex = bBigDownsample ? pDst : pSrc;

	float s1 = 0.5f / (float) pOffsetTex->GetWidth();  // 2.0 better results on lower res images resizing
	float t1 = 0.5f / (float) pOffsetTex->GetHeight();

	Vec4 pParams0, pParams1;

	if (bBigDownsample)
	{
		// Use rotated grid + middle sample (~quincunx)
		pParams0 = Vec4(s1 * 0.96f, t1 * 0.25f, -s1 * 0.25f, t1 * 0.96f);
		pParams1 = Vec4(-s1 * 0.96f, -t1 * 0.25f, s1 * 0.25f, -t1 * 0.96f);
	}
	else
	{
		// Use box filtering (faster - can skip bilinear filtering, only 4 taps)
		pParams0 = Vec4(-s1, -t1, s1, -t1);
		pParams1 = Vec4(s1, t1, -s1, t1);
	}

	static CCryNameR pParam0Name("texToTexParams0");
	static CCryNameR pParam1Name("texToTexParams1");

	CShaderMan::s_shPostEffects->FXSetPSFloat(pParam0Name, &pParams0, 1);
	CShaderMan::s_shPostEffects->FXSetPSFloat(pParam1Name, &pParams1, 1);

	pSrc->Apply(0, bResample ? EDefaultSamplerStates::LinearClamp : EDefaultSamplerStates::PointClamp, EFTT_UNKNOWN, -1, EDefaultResourceViews::Default, !!gRenDev->m_RP.m_MSAAData.Type && bBindMultisampled); // bind as msaa target (if valid)

	DrawFullScreenTri(pDst->GetWidth(), pDst->GetHeight(), 0, srcRegion);

	ShEndPass();

	// Restore previous viewport
	gcpRendD3D->FX_PopRenderTarget(0);
	gcpRendD3D->RT_SetViewport(iTempX, iTempY, iWidth, iHeight);

	gRenDev->m_RP.m_FlagsShader_RT = nSaveFlagsShader_RT;
}

void SD3DPostEffectsUtils::SwapRedBlue(CTexture* pSrc, CTexture* pDst)
{
#if CRY_PLATFORM_ORBIS
	PROFILE_LABEL_SCOPE("SWAP_RED_BLUE");

	CRenderer* rd = gRenDev;

	// Save current viewport
	int iTempX, iTempY, iWidth, iHeight;
	rd->GetViewport(&iTempX, &iTempY, &iWidth, &iHeight);

	gcpRendD3D->FX_PushRenderTarget(0, pDst, NULL);
	gcpRendD3D->RT_SetViewport(0, 0, pDst->GetWidth(), pDst->GetHeight());

	static CCryNameTSCRC pTechName("TextureToTextureSwapRedBlue");
	ShBeginPass(CShaderMan::s_shPostEffects, pTechName, FEF_DONTSETTEXTURES | FEF_DONTSETSTATES);

	rd->FX_SetState(GS_NODEPTHTEST);
	pSrc->Apply(0, EDefaultSamplerStates::PointClamp, EFTT_UNKNOWN, -1, EDefaultResourceViews::Default);
	DrawFullScreenTri(pDst->GetWidth(), pDst->GetHeight(), 0, NULL);

	ShEndPass();

	// Restore previous viewport
	gcpRendD3D->FX_PopRenderTarget(0);
	gcpRendD3D->RT_SetViewport(iTempX, iTempY, iWidth, iHeight);
#endif
}

void SD3DPostEffectsUtils::DownsampleDepth(CTexture* pSrc, CTexture* pDst, bool bFromSingleChannel)
{
	PROFILE_LABEL_SCOPE("DOWNSAMPLE_DEPTH");

	if (!pDst)
		return;

	CD3D9Renderer* const __restrict rd = gcpRendD3D;

	uint64 nSaveFlagsShader_RT = rd->m_RP.m_FlagsShader_RT;
	rd->m_RP.m_FlagsShader_RT &= ~(g_HWSR_MaskBit[HWSR_SAMPLE0] | g_HWSR_MaskBit[HWSR_SAMPLE1] | g_HWSR_MaskBit[HWSR_SAMPLE2] | g_HWSR_MaskBit[HWSR_SAMPLE4] | g_HWSR_MaskBit[HWSR_SAMPLE5]);

	// Get current viewport
	int prevVPX, prevVPY, prevVPWidth, prevVPHeight;
	rd->GetViewport(&prevVPX, &prevVPY, &prevVPWidth, &prevVPHeight);

	bool bUseDeviceDepth = (pSrc == NULL);

	rd->FX_PushRenderTarget(0, pDst, NULL);
	rd->RT_SetViewport(0, 0, pDst->GetWidth(), pDst->GetHeight());

	if (bUseDeviceDepth)
		rd->m_RP.m_FlagsShader_RT |= g_HWSR_MaskBit[HWSR_SAMPLE0];

	if (bFromSingleChannel)
		rd->m_RP.m_FlagsShader_RT |= g_HWSR_MaskBit[HWSR_SAMPLE1];

	static CCryNameTSCRC pTech("DownsampleDepth");
	ShBeginPass(CShaderMan::s_shPostEffects, pTech, FEF_DONTSETTEXTURES | FEF_DONTSETSTATES);

	rd->FX_SetState(GS_NODEPTHTEST);

	int srcWidth = bUseDeviceDepth ? rd->GetWidth() : pSrc->GetWidth();
	int srcHeight = bUseDeviceDepth ? rd->GetHeight() : pSrc->GetHeight();

	D3DShaderResource* depthReadOnlySRV = rd->m_DepthBufferOrig.pTexture->GetDevTexture(/*bMSAA*/)->LookupSRV(EDefaultResourceViews::DepthOnly);
	if (bUseDeviceDepth)
		rd->m_DevMan.BindSRV(CSubmissionQueue_DX11::TYPE_PS, &depthReadOnlySRV, 0, 1);
	else
		SetTexture(pSrc, 0, FILTER_POINT, eSamplerAddressMode_Clamp);

	// Handle uneven source size by dropping last row/column
	static CCryNameR paramName("DownsampleDepth_Params");
	Vec4 params(0, 0, 0, 0);
	int scaledWidth = (srcWidth + 1) / 2;
	int scaledHeight = (srcHeight + 1) / 2;
	params.x = (float)pDst->GetWidth() / (float)scaledWidth;
	params.y = (float)pDst->GetHeight() / (float)scaledHeight;
	CShaderMan::s_shPostEffects->FXSetPSFloat(paramName, &params, 1);

	DrawFullScreenTri(pDst->GetWidth(), pDst->GetHeight());

	ShEndPass();

	if (bUseDeviceDepth)
	{
		D3DShaderResource* pNullSRV[1] = { NULL };
		rd->m_DevMan.BindSRV(CSubmissionQueue_DX11::TYPE_PS, pNullSRV, 0, 1);

		rd->FX_Commit();
	}

	// Restore previous viewport
	rd->FX_PopRenderTarget(0);
	rd->RT_SetViewport(prevVPX, prevVPY, prevVPWidth, prevVPHeight);

	rd->m_RP.m_FlagsShader_RT = nSaveFlagsShader_RT;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////

void SD3DPostEffectsUtils::Downsample(CTexture* pSrc, CTexture* pDst, int nSrcW, int nSrcH, int nDstW, int nDstH, EFilterType eFilter, bool bSetTarget)
{
	if (!pSrc)
		return;

	PROFILE_LABEL_SCOPE("DOWNSAMPLE");

	uint64 nSaveFlagsShader_RT = gRenDev->m_RP.m_FlagsShader_RT;
	gRenDev->m_RP.m_FlagsShader_RT &= ~(g_HWSR_MaskBit[HWSR_SAMPLE0] | g_HWSR_MaskBit[HWSR_SAMPLE1] | g_HWSR_MaskBit[HWSR_SAMPLE2]);

	// Get current viewport
	int iTempX, iTempY, iWidth, iHeight;
	gRenDev->GetViewport(&iTempX, &iTempY, &iWidth, &iHeight);

	if (bSetTarget)
		gcpRendD3D->FX_PushRenderTarget(0, pDst, NULL);
	gcpRendD3D->RT_SetViewport(0, 0, nDstW, nDstH);

	// Currently only exact multiples supported
	Vec2 vSamples(float(nSrcW) / nDstW, float(nSrcH) / nDstH);
	const Vec2 vSampleSize(1.f / nSrcW, 1.f / nSrcH);
	const Vec2 vPixelSize(1.f / nDstW, 1.f / nDstH);
	// Adjust UV space if source rect smaller than texture
	const float fClippedRatioX = float(nSrcW) / pSrc->GetWidth();
	const float fClippedRatioY = float(nSrcH) / pSrc->GetHeight();

	// Base kernel size in pixels
	float fBaseKernelSize = 1.f;
	// How many lines of border samples to skip
	float fBorderSamplesToSkip = 0.f;

	switch (eFilter)
	{
	default:
	case FilterType_Box:
		fBaseKernelSize = 1.f;
		fBorderSamplesToSkip = 0.f;
		break;
	case FilterType_Tent:
		fBaseKernelSize = 2.f;
		fBorderSamplesToSkip = 0.f;
		gRenDev->m_RP.m_FlagsShader_RT |= g_HWSR_MaskBit[HWSR_SAMPLE0];
		break;
	case FilterType_Gauss:
		// The base kernel for Gaussian filter is 3x3 pixels [-1.5 .. 1.5]
		// Samples on the borders are ignored due to small contribution
		// so the actual kernel size is N*3 - 2 where N is number of samples per pixel
		fBaseKernelSize = 3.f;
		fBorderSamplesToSkip = 1.f;
		gRenDev->m_RP.m_FlagsShader_RT |= g_HWSR_MaskBit[HWSR_SAMPLE1];
		break;
	case FilterType_Lanczos:
		fBaseKernelSize = 3.f;
		fBorderSamplesToSkip = 0.f;
		gRenDev->m_RP.m_FlagsShader_RT |= g_HWSR_MaskBit[HWSR_SAMPLE2];
		break;
	}

	// Kernel position step
	const Vec2 vSampleStep(1.f / vSamples.x, 1.f / vSamples.y);
	// The actual kernel radius in pixels
	const Vec2 vKernelRadius = 0.5f * Vec2(fBaseKernelSize, fBaseKernelSize) - fBorderSamplesToSkip * vSampleStep;

	// UV offset from pixel center to first (top-left) sample
	const Vec2 vFirstSampleOffset(0.5f * vSampleSize.x - vKernelRadius.x * vPixelSize.x,
	                              0.5f * vSampleSize.y - vKernelRadius.y * vPixelSize.y);
	// Kernel position of first (top-left) sample
	const Vec2 vFirstSamplePos = -vKernelRadius + 0.5f * vSampleStep;

	static CCryNameTSCRC pTechName("TextureToTextureResampleFilter");
	ShBeginPass(CShaderMan::s_shPostEffects, pTechName, FEF_DONTSETTEXTURES | FEF_DONTSETSTATES);

	gRenDev->FX_SetState(GS_NODEPTHTEST);

	const Vec4 pParams0(vKernelRadius.x, vKernelRadius.y, fClippedRatioX, fClippedRatioY);
	const Vec4 pParams1(vSampleSize.x, vSampleSize.y, vFirstSampleOffset.x, vFirstSampleOffset.y);
	const Vec4 pParams2(vSampleStep.x, vSampleStep.y, vFirstSamplePos.x, vFirstSamplePos.y);

	static CCryNameR pParam0Name("texToTexParams0");
	static CCryNameR pParam1Name("texToTexParams1");
	static CCryNameR pParam2Name("texToTexParams2");
	CShaderMan::s_shPostEffects->FXSetPSFloat(pParam0Name, &pParams0, 1);
	CShaderMan::s_shPostEffects->FXSetPSFloat(pParam1Name, &pParams1, 1);
	CShaderMan::s_shPostEffects->FXSetPSFloat(pParam2Name, &pParams2, 1);

	SetTexture(pSrc, 0, FILTER_NONE);

	DrawFullScreenTri(nDstW, nDstH);

	ShEndPass();

	// Restore previous viewport
	if (bSetTarget)
		gcpRendD3D->FX_PopRenderTarget(0);
	gcpRendD3D->RT_SetViewport(iTempX, iTempY, iWidth, iHeight);

	gRenDev->m_RP.m_FlagsShader_RT = nSaveFlagsShader_RT;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////

void SD3DPostEffectsUtils::DownsampleStable(CTexture* pSrcRT, CTexture* pDstRT, bool bKillFireflies)
{
	PROFILE_LABEL_SCOPE("DOWNSAMPLE_STABLE");

	uint64 nSaveFlagsShader_RT = gRenDev->m_RP.m_FlagsShader_RT;
	gRenDev->m_RP.m_FlagsShader_RT &= ~(g_HWSR_MaskBit[HWSR_SAMPLE0] | g_HWSR_MaskBit[HWSR_SAMPLE1] | g_HWSR_MaskBit[HWSR_SAMPLE2] | g_HWSR_MaskBit[HWSR_SAMPLE4] | g_HWSR_MaskBit[HWSR_SAMPLE5]);

	int iTempX, iTempY, iWidth, iHeight;
	gRenDev->GetViewport(&iTempX, &iTempY, &iWidth, &iHeight);

	gcpRendD3D->FX_PushRenderTarget(0, pDstRT, NULL);
	gcpRendD3D->RT_SetViewport(0, 0, pDstRT->GetWidth(), pDstRT->GetHeight());

	if (bKillFireflies)
		gRenDev->m_RP.m_FlagsShader_RT |= g_HWSR_MaskBit[HWSR_SAMPLE0];

	static CCryNameTSCRC techName("DownsampleStable");
	ShBeginPass(CShaderMan::s_shPostEffects, techName, FEF_DONTSETTEXTURES | FEF_DONTSETSTATES);

	gcpRendD3D->FX_SetState(GS_NODEPTHTEST);
	pSrcRT->Apply(0, EDefaultSamplerStates::LinearClamp);

	DrawFullScreenTri(pDstRT->GetWidth(), pDstRT->GetHeight());

	ShEndPass();

	gcpRendD3D->FX_PopRenderTarget(0);
	gcpRendD3D->RT_SetViewport(iTempX, iTempY, iWidth, iHeight);

	gRenDev->m_RP.m_FlagsShader_RT = nSaveFlagsShader_RT;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////

void SD3DPostEffectsUtils::TexBlurIterative(CTexture* pTex, int nIterationsMul, bool bDilate, CTexture* pBlurTmp)
{
	if (!pTex)
		return;

	SDynTexture* tpBlurTemp = 0;

	if (!pBlurTmp)
	{
		tpBlurTemp = new SDynTexture(pTex->GetWidth(), pTex->GetHeight(), pTex->GetDstFormat(), eTT_2D, FT_STATE_CLAMP | FT_USAGE_RENDERTARGET, "TempBlurRT");
		if (tpBlurTemp)
			tpBlurTemp->Update(pTex->GetWidth(), pTex->GetHeight());

		if (!tpBlurTemp || !tpBlurTemp->m_pTexture)
		{
			SAFE_DELETE(tpBlurTemp);
			return;
		}
	}

	PROFILE_LABEL_SCOPE("TEXBLUR_16TAPS");

	CTexture* pTempRT = pBlurTmp ? pBlurTmp : tpBlurTemp->m_pTexture;

	// Get current viewport
	int iTempX, iTempY, iWidth, iHeight;
	gRenDev->GetViewport(&iTempX, &iTempY, &iWidth, &iHeight);

	// Iterative blur (aka Kawase): 4 taps, 16 taps, 64 taps, 256 taps, etc
	uint64 nFlagsShaderRT = gRenDev->m_RP.m_FlagsShader_RT;
	gRenDev->m_RP.m_FlagsShader_RT &= ~(g_HWSR_MaskBit[HWSR_SAMPLE0] | g_HWSR_MaskBit[HWSR_SAMPLE1]);

	//// Dilate - use 2 passes, horizontal+vertical
	//if( bDilate )
	//{
	//	gRenDev->m_RP.m_FlagsShader_RT |= g_HWSR_MaskBit[HWSR_SAMPLE0];
	//	nIterationsMul = 1;
	//}

	for (int i = 1; i <= nIterationsMul; ++i)
	{
		////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
		// 1st iteration (4 taps)

		gcpRendD3D->FX_PushRenderTarget(0, pTempRT, NULL);
		gcpRendD3D->FX_SetActiveRenderTargets();// Avoiding invalid d3d error (due to deferred rt setup, when ping-pong'ing between RTs we can bump into RTs still bound when binding it as a SRV)
		gcpRendD3D->RT_SetViewport(0, 0, pTex->GetWidth(), pTex->GetHeight());

		// only regular gaussian blur supporting masking
		static CCryNameTSCRC pTechName("Blur4Taps");

		uint32 nPasses;
		CShaderMan::s_shPostEffects->FXSetTechnique(pTechName);
		CShaderMan::s_shPostEffects->FXBegin(&nPasses, FEF_DONTSETTEXTURES | FEF_DONTSETSTATES);

		gRenDev->FX_SetState(GS_NODEPTHTEST);

		// setup texture offsets, for texture sampling
		// Get sample size ratio (based on empirical "best look" approach)
		float fSampleSize = 1.0f * ((float) i);//((float)pTex->GetWidth()/(float)pTex->GetWidth()) * 0.5f;

		// Set samples position
		float s1 = fSampleSize / (float) pTex->GetWidth();  // 2.0 better results on lower res images resizing
		float t1 = fSampleSize / (float) pTex->GetHeight();

		// Use rotated grid
		Vec4 pParams0 = Vec4(s1 * 0.96f, t1 * 0.25f, -s1 * 0.25f, t1 * 0.96f);//Vec4(-s1, -t1, s1, -t1);
		Vec4 pParams1 = Vec4(-s1 * 0.96f, -t1 * 0.25f, s1 * 0.25f, -t1 * 0.96f);//Vec4(s1, t1, -s1, t1);

		static CCryNameR pParam0Name("texToTexParams0");
		static CCryNameR pParam1Name("texToTexParams1");

		CShaderMan::s_shPostEffects->FXSetPSFloat(pParam0Name, &pParams0, 1);
		CShaderMan::s_shPostEffects->FXSetPSFloat(pParam1Name, &pParams1, 1);

		CShaderMan::s_shPostEffects->FXBeginPass(0);

		SetTexture(pTex, 0, FILTER_LINEAR);

		DrawFullScreenTri(pTex->GetWidth(), pTex->GetHeight());

		CShaderMan::s_shPostEffects->FXEndPass();
		CShaderMan::s_shPostEffects->FXEnd();

		gcpRendD3D->FX_PopRenderTarget(0);

		////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
		// 2nd iteration (4 x 4 taps)
		if (bDilate)
			gRenDev->m_RP.m_FlagsShader_RT |= g_HWSR_MaskBit[HWSR_SAMPLE1];

		gcpRendD3D->FX_PushRenderTarget(0, pTex, NULL);
		gcpRendD3D->FX_SetActiveRenderTargets();// Avoiding invalid d3d error (due to deferred rt setup, when ping-pong'ing between RTs we can bump into RTs still bound when binding it as a SRV)
		gcpRendD3D->RT_SetViewport(0, 0, pTex->GetWidth(), pTex->GetHeight());

		CShaderMan::s_shPostEffects->FXSetTechnique(pTechName);
		CShaderMan::s_shPostEffects->FXBegin(&nPasses, FEF_DONTSETTEXTURES | FEF_DONTSETSTATES);

		gRenDev->FX_SetState(GS_NODEPTHTEST);
		// increase kernel size for second iteration
		fSampleSize = 2.0f * ((float) i);
		// Set samples position
		s1 = fSampleSize / (float) pTex->GetWidth();  // 2.0 better results on lower res images resizing
		t1 = fSampleSize / (float) pTex->GetHeight();

		// Use rotated grid
		pParams0 = Vec4(s1 * 0.96f, t1 * 0.25f, -s1 * 0.25f, t1 * 0.96f);//Vec4(-s1, -t1, s1, -t1);
		pParams1 = Vec4(-s1 * 0.96f, -t1 * 0.25f, s1 * 0.25f, -t1 * 0.96f);//Vec4(s1, t1, -s1, t1);

		CShaderMan::s_shPostEffects->FXSetPSFloat(pParam0Name, &pParams0, 1);
		CShaderMan::s_shPostEffects->FXSetPSFloat(pParam1Name, &pParams1, 1);

		CShaderMan::s_shPostEffects->FXBeginPass(0);

		SetTexture(pTempRT, 0, FILTER_LINEAR);

		DrawFullScreenTri(pTex->GetWidth(), pTex->GetHeight());

		CShaderMan::s_shPostEffects->FXEndPass();
		CShaderMan::s_shPostEffects->FXEnd();

		gcpRendD3D->FX_PopRenderTarget(0);
	}

	gRenDev->m_RP.m_FlagsShader_RT = nFlagsShaderRT;

	// Restore previous viewport
	gcpRendD3D->RT_SetViewport(iTempX, iTempY, iWidth, iHeight);

	SAFE_DELETE(tpBlurTemp);
}

////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////

void SD3DPostEffectsUtils::TexBlurDirectional(CTexture* pTex, const Vec2& vDir, int nIterationsMul, CTexture* pBlurTmp)
{
	if (!pTex)
		return;

	SDynTexture* tpBlurTemp = 0;
	if (!pBlurTmp)
	{
		tpBlurTemp = new SDynTexture(pTex->GetWidth(), pTex->GetHeight(), pTex->GetDstFormat(), eTT_2D, FT_STATE_CLAMP | FT_USAGE_RENDERTARGET, "TempBlurRT");
		if (tpBlurTemp)
			tpBlurTemp->Update(pTex->GetWidth(), pTex->GetHeight());

		if (!tpBlurTemp || !tpBlurTemp->m_pTexture)
		{
			SAFE_DELETE(tpBlurTemp);
			return;
		}
	}

	PROFILE_LABEL_SCOPE("TEXBLUR_DIRECTIONAL");

	CTexture* pTempRT = pBlurTmp;
	if (!pBlurTmp)
		pTempRT = tpBlurTemp->m_pTexture;

	static CCryNameTSCRC pTechName("BlurDirectional");
	static CCryNameR pParam0Name("texToTexParams0");
	static CCryNameR pParam1Name("texToTexParams1");
	static CCryNameR pParam2Name("texToTexParams2");
	static CCryNameR pParam3Name("texToTexParams3");

	// Get current viewport
	int iTempX, iTempY, iWidth, iHeight;
	gRenDev->GetViewport(&iTempX, &iTempY, &iWidth, &iHeight);

	// Iterative directional blur: 1 iter: 8 taps, 64 taps, 2 iter: 512 taps, 4096 taps...
	float fSampleScale = 1.0f;
	for (int i = nIterationsMul; i >= 1; --i)
	{
		// 1st iteration (4 taps)

		gcpRendD3D->FX_PushRenderTarget(0, pTempRT, NULL);
		gcpRendD3D->RT_SetViewport(0, 0, pTex->GetWidth(), pTex->GetHeight());

		uint32 nPasses;
		CShaderMan::s_shPostEffects->FXSetTechnique(pTechName);
		CShaderMan::s_shPostEffects->FXBegin(&nPasses, FEF_DONTSETTEXTURES | FEF_DONTSETSTATES);

		gRenDev->FX_SetState(GS_NODEPTHTEST);

		float fSampleSize = fSampleScale;

		// Set samples position
		float s1 = fSampleSize / (float) pTex->GetWidth();  // 2.0 better results on lower res images resizing
		float t1 = fSampleSize / (float) pTex->GetHeight();
		Vec2 vBlurDir;
		vBlurDir.x = s1 * vDir.x;
		vBlurDir.y = t1 * vDir.y;

		// Use rotated grid
		Vec4 pParams0 = Vec4(-vBlurDir.x * 4.0f, -vBlurDir.y * 4.0f, -vBlurDir.x * 3.0f, -vBlurDir.y * 3.0f);
		Vec4 pParams1 = Vec4(-vBlurDir.x * 2.0f, -vBlurDir.y * 2.0f, -vBlurDir.x, -vBlurDir.y);
		Vec4 pParams2 = Vec4(vBlurDir.x * 2.0f, vBlurDir.y * 2.0f, vBlurDir.x, vBlurDir.y);
		Vec4 pParams3 = Vec4(vBlurDir.x * 4.0f, vBlurDir.y * 4.0f, vBlurDir.x * 3.0f, vBlurDir.y * 3.0f);

		CShaderMan::s_shPostEffects->FXSetPSFloat(pParam0Name, &pParams0, 1);
		CShaderMan::s_shPostEffects->FXSetPSFloat(pParam1Name, &pParams1, 1);
		CShaderMan::s_shPostEffects->FXSetPSFloat(pParam2Name, &pParams2, 1);
		CShaderMan::s_shPostEffects->FXSetPSFloat(pParam3Name, &pParams3, 1);

		CShaderMan::s_shPostEffects->FXBeginPass(0);

		SetTexture(pTex, 0, FILTER_LINEAR, eSamplerAddressMode_Border);
		SetTexture(CTexture::s_ptexScreenNoiseMap, 1, FILTER_POINT, eSamplerAddressMode_Wrap);

		DrawFullScreenTri(pTex->GetWidth(), pTex->GetHeight());

		CShaderMan::s_shPostEffects->FXEndPass();
		CShaderMan::s_shPostEffects->FXEnd();

		gcpRendD3D->FX_PopRenderTarget(0);

		////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
		// 2nd iteration (4 x 4 taps)

		gcpRendD3D->FX_PushRenderTarget(0, pTex, NULL);
		gcpRendD3D->RT_SetViewport(0, 0, pTex->GetWidth(), pTex->GetHeight());

		CShaderMan::s_shPostEffects->FXSetTechnique(pTechName);
		CShaderMan::s_shPostEffects->FXBegin(&nPasses, FEF_DONTSETTEXTURES | FEF_DONTSETSTATES);

		gRenDev->FX_SetState(GS_NODEPTHTEST);

		fSampleScale *= 0.75f;

		fSampleSize = fSampleScale;
		s1 = fSampleSize / (float) pTex->GetWidth();  // 2.0 better results on lower res images resizing
		t1 = fSampleSize / (float) pTex->GetHeight();
		vBlurDir.x = vDir.x * s1;
		vBlurDir.y = vDir.y * t1;

		pParams0 = Vec4(-vBlurDir.x * 4.0f, -vBlurDir.y * 4.0f, -vBlurDir.x * 3.0f, -vBlurDir.y * 3.0f);
		pParams1 = Vec4(-vBlurDir.x * 2.0f, -vBlurDir.y * 2.0f, -vBlurDir.x, -vBlurDir.y);
		pParams2 = Vec4(vBlurDir.x, vBlurDir.y, vBlurDir.x * 2.0f, vBlurDir.y * 2.0f);
		pParams3 = Vec4(vBlurDir.x * 3.0f, vBlurDir.y * 3.0f, vBlurDir.x * 4.0f, vBlurDir.y * 4.0f);

		CShaderMan::s_shPostEffects->FXSetPSFloat(pParam0Name, &pParams0, 1);
		CShaderMan::s_shPostEffects->FXSetPSFloat(pParam1Name, &pParams1, 1);
		CShaderMan::s_shPostEffects->FXSetPSFloat(pParam2Name, &pParams2, 1);
		CShaderMan::s_shPostEffects->FXSetPSFloat(pParam3Name, &pParams3, 1);

		CShaderMan::s_shPostEffects->FXBeginPass(0);

		SetTexture(pTempRT, 0, FILTER_LINEAR, eSamplerAddressMode_Border);
		SetTexture(CTexture::s_ptexScreenNoiseMap, 1, FILTER_POINT, eSamplerAddressMode_Wrap);

		DrawFullScreenTri(pTex->GetWidth(), pTex->GetHeight());

		CShaderMan::s_shPostEffects->FXEndPass();
		CShaderMan::s_shPostEffects->FXEnd();

		gcpRendD3D->FX_PopRenderTarget(0);

		fSampleScale *= 0.5f;
	}

	// Restore previous viewport
	gcpRendD3D->RT_SetViewport(iTempX, iTempY, iWidth, iHeight);

	SAFE_DELETE(tpBlurTemp);
}

////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////

void SD3DPostEffectsUtils::TexBlurGaussian(CTexture* pTex, int nAmount, float fScale, float fDistribution, bool bAlphaOnly, CTexture* pMask, bool bSRGB, CTexture* pBlurTmp)
{
	if (!pTex)
		return;

	uint64 nSaveFlagsShader_RT = gRenDev->m_RP.m_FlagsShader_RT;
	gRenDev->m_RP.m_FlagsShader_RT &= ~(g_HWSR_MaskBit[HWSR_SAMPLE0] | g_HWSR_MaskBit[HWSR_SAMPLE1] | g_HWSR_MaskBit[HWSR_SAMPLE2]);

	PROFILE_LABEL_SCOPE("TEXBLUR_GAUSSIAN");

	if (bSRGB)
		gRenDev->m_RP.m_FlagsShader_RT |= g_HWSR_MaskBit[HWSR_SAMPLE0];

	CTexture* pTempRT = pBlurTmp;
	SDynTexture* tpBlurTemp = 0;
	if (!pBlurTmp)
	{
		tpBlurTemp = new SDynTexture(pTex->GetWidth(), pTex->GetHeight(), pTex->GetDstFormat(), eTT_2D, FT_STATE_CLAMP | FT_USAGE_RENDERTARGET, "TempBlurRT");
		if (tpBlurTemp)
			tpBlurTemp->Update(pTex->GetWidth(), pTex->GetHeight());

		if (!tpBlurTemp || !tpBlurTemp->m_pTexture)
		{
			SAFE_DELETE(tpBlurTemp);
			return;
		}

		pTempRT = tpBlurTemp->m_pTexture;
	}

	// Get current viewport
	int iTempX, iTempY, iWidth, iHeight;
	gRenDev->GetViewport(&iTempX, &iTempY, &iWidth, &iHeight);
	gcpRendD3D->RT_SetViewport(0, 0, pTex->GetWidth(), pTex->GetHeight());

	Vec4 vWhite(1.0f, 1.0f, 1.0f, 1.0f);

	// TODO: Make test with Martin's idea about the horizontal blur pass with vertical offset.

	// only regular gaussian blur supporting masking
	static CCryNameTSCRC pTechName("GaussBlurBilinear");
	static CCryNameTSCRC pTechNameMasked("MaskedGaussBlurBilinear");
	static CCryNameTSCRC pTechName1("GaussAlphaBlur");

	//ShBeginPass(CShaderMan::m_shPostEffects, , FEF_DONTSETTEXTURES | FEF_DONTSETSTATES);
	uint32 nPasses;
	CShaderMan::s_shPostEffects->FXSetTechnique((!bAlphaOnly) ? ((pMask) ? pTechNameMasked : pTechName) : pTechName1);
	CShaderMan::s_shPostEffects->FXBegin(&nPasses, FEF_DONTSETTEXTURES | FEF_DONTSETSTATES);

	gRenDev->FX_SetState(GS_NODEPTHTEST);

	// setup texture offsets, for texture sampling
	float s1 = 1.0f / (float) pTex->GetWidth();
	float t1 = 1.0f / (float) pTex->GetHeight();

	// Horizontal/Vertical pass params
	const int nSamples = 16;
	int nHalfSamples = (nSamples >> 1);

	Vec4 pHParams[32], pVParams[32], pWeightsPS[32];
	float pWeights[32], fWeightSum = 0;

	memset(pWeights, 0, sizeof(pWeights));

	int s;
	for (s = 0; s < nSamples; ++s)
	{
		if (fDistribution != 0.0f)
			pWeights[s] = GaussianDistribution1D(static_cast<float>(s - nHalfSamples), fDistribution);
		else
			pWeights[s] = 0.0f;
		fWeightSum += pWeights[s];
	}

	// normalize weights
	for (s = 0; s < nSamples; ++s)
	{
		pWeights[s] /= fWeightSum;
	}

	// set bilinear offsets
	for (s = 0; s < nHalfSamples; ++s)
	{
		float off_a = pWeights[s * 2];
		float off_b = ((s * 2 + 1) <= nSamples - 1) ? pWeights[s * 2 + 1] : 0;
		float a_plus_b = (off_a + off_b);
		if (a_plus_b == 0)
			a_plus_b = 1.0f;
		float offset = off_b / a_plus_b;

		pWeights[s] = off_a + off_b;
		pWeights[s] *= fScale;
		pWeightsPS[s] = vWhite * pWeights[s];

		float fCurrOffset = (float) s * 2 + offset - nHalfSamples;
		pHParams[s] = Vec4(s1 * fCurrOffset, 0, 0, 0);
		pVParams[s] = Vec4(0, t1 * fCurrOffset, 0, 0);
	}

	static CCryNameR clampTCName("clampTC");
	static CCryNameR pParam0Name("psWeights");
	static CCryNameR pParam1Name("PI_psOffsets");

	Vec4 clampTC(0.0f, 1.0f, 0.0f, 1.0f);
	if (pTex->GetWidth() == gcpRendD3D->GetWidth() && pTex->GetHeight() == gcpRendD3D->GetHeight())
	{
		// clamp manually in shader since texture clamp won't apply for smaller viewport
		clampTC = Vec4(0.0f, gRenDev->m_RP.m_CurDownscaleFactor.x, 0.0f, gRenDev->m_RP.m_CurDownscaleFactor.y);
	}

	//for(int p(1); p<= nAmount; ++p)
	{
		//Horizontal

		gcpRendD3D->FX_PushRenderTarget(0, pTempRT, NULL);
		gcpRendD3D->FX_SetActiveRenderTargets();// Avoiding invalid d3d error (due to deferred rt setup, when ping-pong'ing between RTs we can bump into RTs still bound when binding it as a SRV)
		gcpRendD3D->RT_SetViewport(0, 0, pTex->GetWidth(), pTex->GetHeight());

		// !force updating constants per-pass! (dx10..)
		CShaderMan::s_shPostEffects->FXBeginPass(0);

		CShaderMan::s_shPostEffects->FXSetPSFloat(clampTCName, &clampTC, 1);
		CShaderMan::s_shPostEffects->FXSetPSFloat(pParam0Name, pWeightsPS, nHalfSamples);

		pTex->Apply(0, EDefaultSamplerStates::LinearClamp);
		if (pMask)
			pMask->Apply(1, EDefaultSamplerStates::LinearClamp);
		CShaderMan::s_shPostEffects->FXSetVSFloat(pParam1Name, pHParams, nHalfSamples);
		DrawFullScreenTri(pTex->GetWidth(), pTex->GetHeight());

		CShaderMan::s_shPostEffects->FXEndPass();

		gcpRendD3D->FX_PopRenderTarget(0);

		//Vertical
		gcpRendD3D->FX_PushRenderTarget(0, pTex, NULL);
		gcpRendD3D->FX_SetActiveRenderTargets();// Avoiding invalid d3d error (due to deferred rt setup, when ping-pong'ing between RTs we can bump into RTs still bound when binding it as a SRV)
		gcpRendD3D->RT_SetViewport(0, 0, pTex->GetWidth(), pTex->GetHeight());

		// !force updating constants per-pass! (dx10..)
		CShaderMan::s_shPostEffects->FXBeginPass(0);

		CShaderMan::s_shPostEffects->FXSetPSFloat(clampTCName, &clampTC, 1);
		CShaderMan::s_shPostEffects->FXSetPSFloat(pParam0Name, pWeightsPS, nHalfSamples);

		CShaderMan::s_shPostEffects->FXSetVSFloat(pParam1Name, pVParams, nHalfSamples);
		pTempRT->Apply(0, EDefaultSamplerStates::LinearClamp);
		if (pMask)
			pMask->Apply(1, EDefaultSamplerStates::LinearClamp);
		DrawFullScreenTri(pTex->GetWidth(), pTex->GetHeight());

		CShaderMan::s_shPostEffects->FXEndPass();

		gcpRendD3D->FX_PopRenderTarget(0);
	}

	CShaderMan::s_shPostEffects->FXEnd();

	//    ShEndPass( );

	// Restore previous viewport
	gcpRendD3D->RT_SetViewport(iTempX, iTempY, iWidth, iHeight);

	//release dyntexture
	SAFE_DELETE(tpBlurTemp);

	gRenDev->m_RP.m_FlagsShader_RT = nSaveFlagsShader_RT;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////

void SD3DPostEffectsUtils::BeginStencilPrePass(const bool bAddToStencil, const bool bDebug, const bool bResetStencil, const uint8 nStencilRefReset)
{
	if (!bAddToStencil && !bResetStencil)
	{
		gcpRendD3D->m_nStencilMaskRef++;
	}

	if (gcpRendD3D->m_nStencilMaskRef > STENC_MAX_REF)
	{
		// Stencil initialized to 1 - 0 is reserved for MSAAed samples
		gcpRendD3D->EF_ClearTargetsImmediately(FRT_CLEAR_STENCIL);
		gcpRendD3D->m_nStencilMaskRef = 1;
	}

	gcpRendD3D->FX_SetStencilState(
	  STENC_FUNC(FSS_STENCFUNC_ALWAYS) |
	  STENCOP_FAIL(FSS_STENCOP_REPLACE) |
	  STENCOP_ZFAIL(FSS_STENCOP_REPLACE) |
	  STENCOP_PASS(FSS_STENCOP_REPLACE),
	  bResetStencil ? nStencilRefReset : gcpRendD3D->m_nStencilMaskRef, 0xFFFFFFFF, 0xFFFF
	  );

	gcpRendD3D->FX_SetState(GS_STENCIL | GS_NODEPTHTEST | (!bDebug ? GS_NOCOLMASK_RGBA : 0));
}

////////////////////////////////////////////////////////////////////////////////////////////////////

void SD3DPostEffectsUtils::EndStencilPrePass()
{
}

////////////////////////////////////////////////////////////////////////////////////////////////////

void SD3DPostEffectsUtils::SetupStencilStates(int32 nStFunc)
{
	if (nStFunc < 0)
	{
		return;
	}

	gcpRendD3D->FX_SetStencilState(
	  STENC_FUNC(nStFunc) |
	  STENCOP_FAIL(FSS_STENCOP_KEEP) |
	  STENCOP_ZFAIL(FSS_STENCOP_KEEP) |
	  STENCOP_PASS(FSS_STENCOP_KEEP),
	  gcpRendD3D->m_nStencilMaskRef, 0xFFFFFFFF, 0xFFFFFFFF);

}

////////////////////////////////////////////////////////////////////////////////////////////////////

void SD3DPostEffectsUtils::SetFillModeSolid(bool bEnable)
{
	if (bEnable)
	{
		if (gcpRendD3D->GetWireframeMode() > R_SOLID_MODE)
		{
			SStateRaster RS = gcpRendD3D->m_StatesRS[gcpRendD3D->m_nCurStateRS];
			RS.Desc.FillMode = D3D11_FILL_SOLID;
			gcpRendD3D->SetRasterState(&RS);
		}
	}
	else
	{
		if (gcpRendD3D->GetWireframeMode() == R_WIREFRAME_MODE)
		{
			SStateRaster RS = gcpRendD3D->m_StatesRS[gcpRendD3D->m_nCurStateRS];
			RS.Desc.FillMode = D3D11_FILL_WIREFRAME;
			gcpRendD3D->SetRasterState(&RS);
		}
		else if (gcpRendD3D->GetWireframeMode() == R_POINT_MODE)
		{
		}
	}
}

/////////////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////////////////

void SD3DPostEffectsUtils::DrawQuadFS(CShader* pShader, bool bOutputCamVec, int nWidth, int nHeight, float x0, float y0, float x1, float y1, float z)
{
	const Vec4 cQuadConst(1.0f / (float) nWidth, 1.0f / (float) nHeight, z, 1.0f);
	pShader->FXSetVSFloat(m_pQuadParams, &cQuadConst, 1);

	const Vec4 cQuadPosConst(x0, y0, x1, y1);
	pShader->FXSetVSFloat(m_pQuadPosParams, &cQuadPosConst, 1);

	if (bOutputCamVec)
	{
		UpdateFrustumCorners();
		const Vec4 vLT(m_vLT, 1.0f);
		pShader->FXSetVSFloat(m_pFrustumLTParams, &vLT, 1);
		const Vec4 vLB(m_vLB, 1.0f);
		pShader->FXSetVSFloat(m_pFrustumLBParams, &vLB, 1);
		const Vec4 vRT(m_vRT, 1.0f);
		pShader->FXSetVSFloat(m_pFrustumRTParams, &vRT, 1);
		const Vec4 vRB(m_vRB, 1.0f);
		pShader->FXSetVSFloat(m_pFrustumRBParams, &vRB, 1);
	}

	gcpRendD3D->FX_Commit();
	if (!FAILED(gcpRendD3D->FX_SetVertexDeclaration(0, EDefaultInputLayouts::P3F_C4B_T2F)))
	{
		gcpRendD3D->FX_SetVStream(0, gcpRendD3D->m_pQuadVB, 0, sizeof(SVF_P3F_C4B_T2F));
		gcpRendD3D->FX_DrawPrimitive(eptTriangleStrip, 0, gcpRendD3D->m_nQuadVBSize);
	}
}

/////////////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////////////////

bool CD3D9Renderer::FX_PostProcessScene(bool bEnable)
{
	if (!gcpRendD3D)
	{
		return false;
	}

	if (bEnable)
	{
		//if(!CTexture::s_ptexBackBuffer)
		{
			// Create all resources if required
			PostProcessUtils().Create();
		}
	}

	return true;
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
	ERenderQuality nRenderQuality = gRenDev->m_RP.m_eQuality;
	if (nPrevShaderQuality != nShaderQuality || nPrevRenderQuality != nRenderQuality)
	{
		CPostEffectsMgr::Reset(true);
		nPrevShaderQuality = nShaderQuality;
		nPrevRenderQuality = nRenderQuality;
	}

	gcpRendD3D->ResetToDefault();

	SPostEffectsUtils::m_pCurDepthSurface = &gcpRendD3D->m_DepthBufferOrig;

	gcpRendD3D->RT_SetViewport(0, 0, gcpRendD3D->GetWidth(), gcpRendD3D->GetHeight());

	SPostEffectsUtils::m_fWaterLevel = gRenDev->m_p3DEngineCommon.m_OceanInfo.m_fWaterLevel;

	PostProcessUtils().SetFillModeSolid(true);
	PostProcessUtils().UpdateOverscanBorderAspectRatio();
}

void CPostEffectsMgr::End()
{
	gcpRendD3D->RT_SetViewport(PostProcessUtils().m_pScreenRect.left, PostProcessUtils().m_pScreenRect.top, PostProcessUtils().m_pScreenRect.right, PostProcessUtils().m_pScreenRect.bottom);

	gcpRendD3D->FX_ResetPipe();

	PostProcessUtils().SetFillModeSolid(false);

	gcpRendD3D->m_RP.m_PersFlags2 &= ~RBPF2_NOPOSTAA;

	const uint32 nThreadID = gRenDev->m_RP.m_nProcessThreadID;

	gRenDev->SetPreviousFrameCameraMatrix(gcpRendD3D->GetCameraMatrix());
	gRenDev->m_CameraMatrixNearestPrev = gRenDev->m_CameraMatrixNearest;

	const int kFloatMaxContinuousInt = 0x1000000;  // 2^24
	const bool bStereo = gcpRendD3D->GetS3DRend().IsStereoEnabled();
	const bool bStereoSequentialSubmission = gcpRendD3D->GetS3DRend().GetStereoSubmissionMode() == STEREO_SUBMISSION_SEQUENTIAL;

	if (!bStereo || (bStereo && (!bStereoSequentialSubmission || gRenDev->m_CurRenderEye == RIGHT_EYE)))
		SPostEffectsUtils::m_iFrameCounter = (SPostEffectsUtils::m_iFrameCounter + 1) % kFloatMaxContinuousInt;

	PostProcessUtils().Log("### POST-PROCESSING ENDS ### ");
}
