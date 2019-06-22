// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include "FullscreenPass.h"

struct IUtilityRenderPass
{
	enum class EPassId : uint32
	{
		StretchRectPass = 0,
		StretchRegionPass,
		SharpeningUpsamplePass,
		NearestDepthUpsamplePass,
		DownsamplePass,
		StableDownsamplePass,
		DepthLinearizationPass,
		DepthDelinearizationPass,
		DepthDownsamplePass,
		GaussianBlurPass,
		MipmapGenPass,
		ClearSurfacePass,
		ClearRegionPass,
		AnisotropicVerticalBlurPass,

		MaxPassCount,
	};

	virtual ~IUtilityRenderPass() {}
};

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

class CStretchRectPass : public IUtilityRenderPass
{
public:
	CStretchRectPass() {}
	CStretchRectPass(CGraphicsPipeline* pGraphicsPipeline)
	{
		m_pass.SetGraphicsPipeline(pGraphicsPipeline);
	}
	void           Execute(CTexture* pSrcRT, CTexture* pDestRT);

	static EPassId GetPassId() { return EPassId::StretchRectPass; }

	CFullscreenPass m_pass;
};

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

class CStretchRegionPass : public IUtilityRenderPass
{
public:
	CStretchRegionPass(CGraphicsPipeline* pGraphicsPipeline) : m_pGraphicsPipeline(pGraphicsPipeline) {}
	~CStretchRegionPass() {}

	void           Execute(CTexture* pSrcRT, CTexture* pDestRT, const RECT* pSrcRect = NULL, const RECT* pDstRect = NULL, bool bBigDownsample = false, const ColorF& color = ColorF(1, 1, 1, 1), const int renderStateFlags = 0);
	bool           PreparePrimitive(CRenderPrimitive& prim, CPrimitiveRenderPass& targetPass, const RECT& rcS, int renderState, const D3DViewPort& targetViewport, bool bResample, bool bBigDownsample, CTexture* pSrcRT, CTexture* pDstRT, const ColorF& color);

	static EPassId GetPassId() { return EPassId::StretchRegionPass; }

protected:
	CPrimitiveRenderPass m_pass;
	CRenderPrimitive     m_Primitive;
	CGraphicsPipeline*   m_pGraphicsPipeline;

private:
	static CStretchRegionPass* s_pPass;
};

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

class CSharpeningUpsamplePass : public IUtilityRenderPass
{
public:
	CSharpeningUpsamplePass() {}
	CSharpeningUpsamplePass(CGraphicsPipeline* pGraphicsPipeline)
	{
		m_pass.SetGraphicsPipeline(pGraphicsPipeline);
	}
	void           Execute(CTexture* pSrcRT, CTexture* pDestRT);

	static EPassId GetPassId() { return EPassId::SharpeningUpsamplePass; }

	CFullscreenPass m_pass;
};

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

class CNearestDepthUpsamplePass : public IUtilityRenderPass
{
public:
	CNearestDepthUpsamplePass() {}
	CNearestDepthUpsamplePass(CGraphicsPipeline* pGraphicsPipeline)
	{
		for (auto& pass : m_pass)
		{
			pass.SetGraphicsPipeline(pGraphicsPipeline);
		}
	}

	void           Execute(CTexture* pOrgDS, CTexture* pSrcRT, CTexture* pSrcDS, CTexture* pDestRT, bool bAlphaBased = false);

	static EPassId GetPassId() { return EPassId::NearestDepthUpsamplePass; }

	std::array<CFullscreenPass, 2> m_pass;
};

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

class CDownsamplePass : public IUtilityRenderPass
{
public:
	CDownsamplePass() {}
	CDownsamplePass(CGraphicsPipeline* pGraphicsPipeline)
	{
		m_pass.SetGraphicsPipeline(pGraphicsPipeline);
	}
	enum EFilterType
	{
		FilterType_Box,
		FilterType_Tent,
		FilterType_Gauss,
		FilterType_Lanczos,
	};

	void           Execute(CTexture* pSrcRT, CTexture* pDestRT, int nSrcW, int nSrcH, int nDstW, int nDstH, EFilterType eFilter);

	static EPassId GetPassId() { return EPassId::DownsamplePass; }

	CFullscreenPass m_pass;
};

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

class CStableDownsamplePass : public IUtilityRenderPass
{
public:
	CStableDownsamplePass() {}
	CStableDownsamplePass(CGraphicsPipeline* pGraphicsPipeline)
	{
		m_pass.SetGraphicsPipeline(pGraphicsPipeline);
	}
	void           Execute(CTexture* pSrcRT, CTexture* pDestRT, bool bKillFireflies);

	static EPassId GetPassId() { return EPassId::StableDownsamplePass; }

	CFullscreenPass m_pass;
};

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

class CDepthLinearizationPass : public IUtilityRenderPass
{
public:
	CDepthLinearizationPass() {}
	CDepthLinearizationPass(CGraphicsPipeline* pGraphicsPipeline)
	{
		m_pass.SetGraphicsPipeline(pGraphicsPipeline);
	}
	void           Execute(CTexture* pSrcRT, CTexture* pDestRT);

	static EPassId GetPassId() { return EPassId::DepthLinearizationPass; }

	CFullscreenPass m_pass;
};

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

class CDepthDelinearizationPass : public IUtilityRenderPass
{
public:
	CDepthDelinearizationPass() {}
	CDepthDelinearizationPass(CGraphicsPipeline* pGraphicsPipeline)
	{
		m_pass.SetGraphicsPipeline(pGraphicsPipeline);
	}
	void           Execute(CTexture* pSrcRT, CTexture* pDestDS);

	static EPassId GetPassId() { return EPassId::DepthDelinearizationPass; }

	CFullscreenPass m_pass;
};

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

class CDepthDownsamplePass : public IUtilityRenderPass
{
public:
	CDepthDownsamplePass() {}
	CDepthDownsamplePass(CGraphicsPipeline* pGraphicsPipeline)
	{
		m_pass.SetGraphicsPipeline(pGraphicsPipeline);
	}
	void           Execute(CTexture* pSrcRT, CTexture* pDestRT, CTexture* pDestDS, bool bLinearizeSrcDepth, bool bFromSingleChannel);

	static EPassId GetPassId() { return EPassId::DepthDownsamplePass; }

	CFullscreenPass m_pass;
};

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

class CGaussianBlurPass : public IUtilityRenderPass
{
public:
	CGaussianBlurPass() {}
	CGaussianBlurPass(CGraphicsPipeline* pGraphicsPipeline)
		: m_scale(-FLT_MIN)
		, m_distribution(-FLT_MIN)
	{
		m_passH.SetGraphicsPipeline(pGraphicsPipeline);
		m_passV.SetGraphicsPipeline(pGraphicsPipeline);
	}

	void           Execute(CTexture* pScrDestRT, CTexture* pTempRT, float scale, float distribution, bool bAlphaOnly = false);

	static EPassId GetPassId() { return EPassId::GaussianBlurPass; }

	CFullscreenPass m_passH;
	CFullscreenPass m_passV;

protected:
	float GaussianDistribution1D(float x, float rho);
	void  ComputeParams(int texWidth, int texHeight, int numSamples, float scale, float distribution);

protected:
	float m_scale;
	float m_distribution;
	Vec4  m_paramsH[16];
	Vec4  m_paramsV[16];
	Vec4  m_weights[16];
};
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

class CMipmapGenPass : public IUtilityRenderPass
{
public:
	CMipmapGenPass(){}
	CMipmapGenPass(CGraphicsPipeline* pGraphicsPipeline)
	{
		for (auto& pass : m_downsamplePasses)
		{
			pass.SetGraphicsPipeline(pGraphicsPipeline);
		}
	}

	void           Execute(CTexture* pScrDestRT, int mipCount = 0);

	static EPassId GetPassId() { return EPassId::MipmapGenPass; }

	std::array<CFullscreenPass, 13> m_downsamplePasses;
};

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

class CClearSurfacePass : public IUtilityRenderPass
{
public:
	static void    Execute(const CTexture* pDepthTex, const int nFlags, const float cDepth, const uint8 cStencil);
	static void    Execute(const CTexture* pColorTex, const ColorF& cClear);
	static void    Execute(const CGpuBuffer* pBuf, const ColorF& cClear);
	static void    Execute(const CGpuBuffer* pBuf, const ColorI& cClear);

	static EPassId GetPassId() { return EPassId::ClearSurfacePass; }
};

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

class CClearRegionPass : public CClearSurfacePass
{
public:
	CClearRegionPass(CGraphicsPipeline* pGraphicsPipeline);
	virtual ~CClearRegionPass();

	void           Execute(CTexture* pDepthTex, const int nFlags, const float cDepth, const uint8 cStencil, const uint numRects, const RECT* pRects);
	void           Execute(CTexture* pColorTex, const ColorF& cClear, const uint numRects, const RECT* pRects);
	void           Execute(CGpuBuffer* pBuf, const ColorF& cClear, const uint numRanges, const RECT* pRanges);
	void           Execute(CGpuBuffer* pBuf, const ColorI& cClear, const uint numRanges, const RECT* pRanges);

	static EPassId GetPassId() { return EPassId::ClearRegionPass; }

protected:
	bool PreparePrimitive(CRenderPrimitive& prim, int renderState, int stencilState, const ColorF& cClear, float cDepth, int stencilRef, const RECT& rect, const D3DViewPort& targetViewport, int numRTVs);

	CPrimitiveRenderPass          m_clearPass;
	std::vector<CRenderPrimitive> m_clearPrimitives;
	buffer_handle_t               m_quadVertices;
	CGraphicsPipeline*            m_pGraphicsPipeline;
};

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

class CAnisotropicVerticalBlurPass : public IUtilityRenderPass
{
public:
	CAnisotropicVerticalBlurPass() {}
	CAnisotropicVerticalBlurPass(CGraphicsPipeline* pGraphicsPipeline)
	{
		for (auto& pass : m_passBlurAnisotropicVertical)
		{
			pass.SetGraphicsPipeline(pGraphicsPipeline);
		}
	}
	void           Execute(CTexture* pTex, int nAmount, float fScale, float fDistribution, bool bAlphaOnly);

	static EPassId GetPassId() { return EPassId::AnisotropicVerticalBlurPass; }

	std::array<CFullscreenPass, 2> m_passBlurAnisotropicVertical;
};
