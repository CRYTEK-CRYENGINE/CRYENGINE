// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once


struct CRY_ALIGN(16) SStateBlend
{
	uint64 nHashVal;
	D3D11_BLEND_DESC Desc;
	ID3D11BlendState* pState;

	SStateBlend()
	{
		memset(this, 0, sizeof(*this));
	}

	static uint64 GetHash(const D3D11_BLEND_DESC& InDesc)
	{
		uint32 hashLow = InDesc.AlphaToCoverageEnable |
			(InDesc.RenderTarget[0].BlendEnable << 1) | (InDesc.RenderTarget[1].BlendEnable << 2) | (InDesc.RenderTarget[2].BlendEnable << 3) | (InDesc.RenderTarget[3].BlendEnable << 4) |
			(InDesc.RenderTarget[0].SrcBlend << 5) | (InDesc.RenderTarget[0].DestBlend << 10) |
			(InDesc.RenderTarget[0].SrcBlendAlpha << 15) | (InDesc.RenderTarget[0].DestBlendAlpha << 20) |
			(InDesc.RenderTarget[0].BlendOp << 25) | (InDesc.RenderTarget[0].BlendOpAlpha << 28);
		uint32 hashHigh = InDesc.RenderTarget[0].RenderTargetWriteMask |
			(InDesc.RenderTarget[1].RenderTargetWriteMask << 4) |
			(InDesc.RenderTarget[2].RenderTargetWriteMask << 8) |
			(InDesc.RenderTarget[3].RenderTargetWriteMask << 12) |
			(InDesc.IndependentBlendEnable << 16);

		return (((uint64)hashHigh) << 32) | ((uint64)hashLow);
	}

	bool SkipCleanup()  const { return false; }
};

struct CRY_ALIGN(16) SStateRaster
{
	uint64 nValuesHash;
	uint32 nHashVal;
	ID3D11RasterizerState* pState;
	D3D11_RASTERIZER_DESC Desc;

	SStateRaster()
	{
		memset(this, 0, sizeof(*this));
		Desc.DepthClipEnable = true;
		Desc.FillMode = D3D11_FILL_SOLID;
		Desc.FrontCounterClockwise = TRUE;
	}

	static uint32 GetHash(const D3D11_RASTERIZER_DESC& InDesc)
	{
		uint32 nHash;
		nHash = InDesc.FillMode | (InDesc.CullMode << 2) |
			(InDesc.DepthClipEnable << 4) | (InDesc.FrontCounterClockwise << 5) |
			(InDesc.ScissorEnable << 6) | (InDesc.MultisampleEnable << 7) | (InDesc.AntialiasedLineEnable << 8) |
			(InDesc.DepthBias << 9);
		return nHash;
	}

	static uint64 GetValuesHash(const D3D11_RASTERIZER_DESC& InDesc)
	{
		uint64 nHash;
		//avoid breaking strict aliasing rules
		union f32_u
		{
			float        floatVal;
			unsigned int uintVal;
		};
		f32_u uDepthBiasClamp;
		uDepthBiasClamp.floatVal = InDesc.DepthBiasClamp;
		f32_u uSlopeScaledDepthBias;
		uSlopeScaledDepthBias.floatVal = InDesc.SlopeScaledDepthBias;
		nHash = (((uint64)uDepthBiasClamp.uintVal) |
			((uint64)uSlopeScaledDepthBias.uintVal) << 32);
		return nHash;
	}

	bool SkipCleanup() const { return (Desc.DepthBiasClamp==0 && Desc.SlopeScaledDepthBias == 0); }
};
inline uint32 sStencilState(const D3D11_DEPTH_STENCILOP_DESC& Desc)
{
	uint32 nST = (Desc.StencilFailOp << 0) |
		(Desc.StencilDepthFailOp << 4) |
		(Desc.StencilPassOp << 8) |
		(Desc.StencilFunc << 12);
	return nST;
}
struct CRY_ALIGN(16) SStateDepth
{
	uint64 nHashVal;
	D3D11_DEPTH_STENCIL_DESC Desc;
	ID3D11DepthStencilState* pState;

	SStateDepth() : nHashVal(), pState()
	{
		Desc.DepthEnable = TRUE;
		Desc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ALL;
		Desc.DepthFunc = D3D11_COMPARISON_LESS;
		Desc.StencilEnable = FALSE;
		Desc.StencilReadMask = D3D11_DEFAULT_STENCIL_READ_MASK;
		Desc.StencilWriteMask = D3D11_DEFAULT_STENCIL_WRITE_MASK;

		Desc.FrontFace.StencilFailOp = D3D11_STENCIL_OP_KEEP;
		Desc.FrontFace.StencilDepthFailOp = D3D11_STENCIL_OP_KEEP;
		Desc.FrontFace.StencilPassOp = D3D11_STENCIL_OP_KEEP;
		Desc.FrontFace.StencilFunc = D3D11_COMPARISON_ALWAYS;

		Desc.BackFace = Desc.FrontFace;
	}

	static uint64 GetHash(const D3D11_DEPTH_STENCIL_DESC& InDesc)
	{
		uint64 nHash;
		nHash = (InDesc.DepthEnable << 0) |
			(InDesc.DepthWriteMask << 1) |
			(InDesc.DepthFunc << 2) |
			(InDesc.StencilEnable << 6) |
			(InDesc.StencilReadMask << 7) |
			(InDesc.StencilWriteMask << 15) |
			(((uint64)sStencilState(InDesc.FrontFace)) << 23) |
			(((uint64)sStencilState(InDesc.BackFace)) << 39);
		return nHash;
	}

	bool SkipCleanup() const { return false; }
};

class CDeviceStatesManagerDX11
{
public:
	CDeviceStatesManagerDX11() {}
	~CDeviceStatesManagerDX11() {}

	void ShutDown();
	void ReleaseUnusedStates(uint32 currentFrameID);

	static CDeviceStatesManagerDX11* GetInstance();

	uint32 GetOrCreateBlendState(const D3D11_BLEND_DESC& desc);
	uint32 GetOrCreateRasterState(const D3D11_RASTERIZER_DESC& rasterizerDec, const bool bAllowMSAA = true);
	uint32 GetOrCreateDepthState(const D3D11_DEPTH_STENCIL_DESC& desc);

public:
	std::vector<SStateBlend>      m_StatesBL;
	std::vector<SStateRaster>     m_StatesRS;
	std::vector<SStateDepth>      m_StatesDP;
};

