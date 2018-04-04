// Copyright 2001-2017 Crytek GmbH / Crytek Group. All rights reserved. 

/*=============================================================================
   D3DPostProcess : Direct3D specific post processing special effects

   =============================================================================*/

#ifndef _D3DPOSTPROCESS_H_
#define _D3DPOSTPROCESS_H_

#include "../Common/PostProcess/PostEffects.h"

struct SD3DPostEffectsUtils : public SPostEffectsUtils
{
	//  friend class CD3D9Renderer;

public:

	enum EFilterType
	{
		FilterType_Box,
		FilterType_Tent,
		FilterType_Gauss,
		FilterType_Lanczos,
	};

	virtual void CopyTextureToScreen(CTexture*& pSrc, const RECT* srcRegion = NULL, const int filterMode = -1);
	virtual void CopyScreenToTexture(CTexture*& pDst, const RECT* srcRegion = NULL);
	virtual void StretchRect(CTexture* pSrc, CTexture*& pDst, bool bClearAlpha = false, bool bDecodeSrcRGBK = false, bool bEncodeDstRGBK = false, bool bBigDownsample = false, EDepthDownsample depthDownsampleMode = eDepthDownsample_None, bool bBindMultisampled = false, const RECT* srcRegion = NULL);
	void         SwapRedBlue(CTexture* pSrc, CTexture* pDst);
	virtual void TexBlurIterative(CTexture* pTex, int nIterationsMul = 1, bool bDilate = false, CTexture* pBlurTmp = 0); // 2 terations minimum (src => temp => src)
	virtual void TexBlurGaussian(CTexture* pTex, int nAmount, float fScale, float fDistribution, bool bAlphaOnly, CTexture* pMask = 0, bool bSRGB = false, CTexture* pBlurTmp = 0);
	virtual void TexBlurDirectional(CTexture* pTex, const Vec2& vDir, int nIterationsMul = 1, CTexture* pBlurTmp = 0); // 2 terations minimum (src => temp => src)
	virtual void DrawQuadFS(CShader* pShader, bool bOutputCamVec, int nWidth, int nHeight, float x0 = 0, float y0 = 0, float x1 = 1, float y1 = 1, float z = 0);

	// Begins drawing a stencil pre-pass mask
	void BeginStencilPrePass(const bool bAddToStencil = false, const bool bDebug = false, const bool bResetStencil = false, const uint8 nStencilRefReset = 0);
	// Ends drawing a stencil pre-pass mask
	void EndStencilPrePass();
	// Setup render states for passes using stencil masks
	void SetupStencilStates(int32 nStFunc);

	// Override fill mode (wireframe/point) with solid mode
	void SetFillModeSolid(bool bEnable);

	// Downsample source to target using specified filter
	// If bSetTarget is not set then destination target is ignored and currently set render target is used instead
	void Downsample(CTexture* pSrc, CTexture* pDst, int nSrcW, int nSrcH, int nDstW, int nDstH, EFilterType eFilter = FilterType_Box, bool bSetTarget = true);

	// Downsample to half resolution while minimizing temporal aliasing (useful for bloom)
	void           DownsampleStable(CTexture* pSrcRT, CTexture* pDstRT, bool bKillFireflies = false);

	void           DownsampleDepth(CTexture* pSrc, CTexture* pDst, bool bFromSingleChannel);

	CTexture*      GetBackBufferTexture();

	SDepthTexture* GetDepthSurface(CTexture* pTex);
	void           ResolveRT(CTexture*& pDst, const RECT* pSrcRect);

	////////////////////////////////////////////////////////////////////////////////////////////////////
	////////////////////////////////////////////////////////////////////////////////////////////////////

	static SD3DPostEffectsUtils& GetInstance()
	{
		return m_pInstance;
	}

private:

	static SD3DPostEffectsUtils m_pInstance;

	SD3DPostEffectsUtils()
	{
		m_pQuadParams = "g_vQuadParams";
		m_pQuadPosParams = "g_vQuadPosParams";
		m_pFrustumLTParams = "g_vViewFrustumLT";
		m_pFrustumLBParams = "g_vViewFrustumLB";
		m_pFrustumRTParams = "g_vViewFrustumRT";
		m_pFrustumRBParams = "g_vViewFrustumRB";
	}

	virtual ~SD3DPostEffectsUtils()
	{
	}

private:

	CCryNameR m_pQuadParams;
	CCryNameR m_pQuadPosParams;
	CCryNameR m_pFrustumLTParams;
	CCryNameR m_pFrustumLBParams;
	CCryNameR m_pFrustumRTParams;
	CCryNameR m_pFrustumRBParams;
};

// to be removed
#define GetUtils()         SD3DPostEffectsUtils::GetInstance()

#define PostProcessUtils() SD3DPostEffectsUtils::GetInstance()

#endif
