// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "CCryDX12BlendState.hpp"

CCryDX12BlendState* CCryDX12BlendState::Create(const D3D11_BLEND_DESC* pBlendStateDesc)
{
	D3D12_BLEND_DESC desc12;
	ZeroMemory(&desc12, sizeof(D3D11_BLEND_DESC));

	desc12.AlphaToCoverageEnable = pBlendStateDesc->AlphaToCoverageEnable;
	desc12.IndependentBlendEnable = pBlendStateDesc->IndependentBlendEnable;

	for (UINT i = 0; i < D3D12_SIMULTANEOUS_RENDER_TARGET_COUNT; ++i)
	{
		const D3D11_RENDER_TARGET_BLEND_DESC& rt11 = pBlendStateDesc->RenderTarget[i];
		D3D12_RENDER_TARGET_BLEND_DESC& rt12 = desc12.RenderTarget[i];

		rt12.BlendEnable = rt11.BlendEnable;
		rt12.BlendOp = static_cast<D3D12_BLEND_OP>(rt11.BlendOp);
		rt12.BlendOpAlpha = static_cast<D3D12_BLEND_OP>(rt11.BlendOpAlpha);
		rt12.DestBlend = static_cast<D3D12_BLEND>(rt11.DestBlend);
		rt12.DestBlendAlpha = static_cast<D3D12_BLEND>(rt11.DestBlendAlpha);
		rt12.SrcBlend = static_cast<D3D12_BLEND>(rt11.SrcBlend);
		rt12.SrcBlendAlpha = static_cast<D3D12_BLEND>(rt11.SrcBlendAlpha);
		rt12.RenderTargetWriteMask = rt11.RenderTargetWriteMask;
		rt12.LogicOpEnable = FALSE;
		rt12.LogicOp = D3D12_LOGIC_OP_CLEAR;
	}

	return DX12_NEW_RAW(CCryDX12BlendState(*pBlendStateDesc, desc12));
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

CCryDX12BlendState::CCryDX12BlendState(const D3D11_BLEND_DESC& desc11, const D3D12_BLEND_DESC& desc12)
	: Super(nullptr, nullptr)
	, m_Desc11(desc11)
	, m_Desc12(desc12)
{

}

#pragma region /* ID3D11BlendState implementation */

void STDMETHODCALLTYPE CCryDX12BlendState::GetDesc(
  _Out_ D3D11_BLEND_DESC* pDesc)
{
	if (pDesc)
	{
		*pDesc = m_Desc11;
	}
}

#pragma endregion
