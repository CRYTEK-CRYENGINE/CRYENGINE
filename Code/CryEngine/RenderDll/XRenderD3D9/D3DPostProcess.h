// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

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

	// Downsample source to target using specified filter
// If bSetTarget is not set then destination target is ignored and currently set render target is used instead
	void           Downsample(CTexture* pSrc, CTexture* pDst, int nSrcW, int nSrcH, int nDstW, int nDstH, EFilterType eFilter = FilterType_Box, bool bSetTarget = true);

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
