// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "CCryDX12DepthStencilState.hpp"

CCryDX12DepthStencilState* CCryDX12DepthStencilState::Create(const D3D11_DEPTH_STENCIL_DESC* pDepthStencilDesc)
{
	D3D12_DEPTH_STENCIL_DESC desc12;
	ZeroMemory(&desc12, sizeof(D3D12_DEPTH_STENCIL_DESC));

	desc12.BackFace.StencilDepthFailOp = static_cast<D3D12_STENCIL_OP>(pDepthStencilDesc->BackFace.StencilDepthFailOp);
	desc12.BackFace.StencilFailOp = static_cast<D3D12_STENCIL_OP>(pDepthStencilDesc->BackFace.StencilFailOp);
	desc12.BackFace.StencilPassOp = static_cast<D3D12_STENCIL_OP>(pDepthStencilDesc->BackFace.StencilPassOp);
	desc12.BackFace.StencilFunc = static_cast<D3D12_COMPARISON_FUNC>(pDepthStencilDesc->BackFace.StencilFunc);

	desc12.FrontFace.StencilDepthFailOp = static_cast<D3D12_STENCIL_OP>(pDepthStencilDesc->FrontFace.StencilDepthFailOp);
	desc12.FrontFace.StencilFailOp = static_cast<D3D12_STENCIL_OP>(pDepthStencilDesc->FrontFace.StencilFailOp);
	desc12.FrontFace.StencilPassOp = static_cast<D3D12_STENCIL_OP>(pDepthStencilDesc->FrontFace.StencilPassOp);
	desc12.FrontFace.StencilFunc = static_cast<D3D12_COMPARISON_FUNC>(pDepthStencilDesc->FrontFace.StencilFunc);

	desc12.DepthEnable = pDepthStencilDesc->DepthEnable;
	desc12.DepthFunc = static_cast<D3D12_COMPARISON_FUNC>(pDepthStencilDesc->DepthFunc);
	desc12.DepthWriteMask = static_cast<D3D12_DEPTH_WRITE_MASK>(pDepthStencilDesc->DepthWriteMask);
	desc12.StencilEnable = pDepthStencilDesc->StencilEnable;
	desc12.StencilReadMask = pDepthStencilDesc->StencilReadMask;
	desc12.StencilWriteMask = pDepthStencilDesc->StencilWriteMask;

	return DX12_NEW_RAW(CCryDX12DepthStencilState(*pDepthStencilDesc, desc12));
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

CCryDX12DepthStencilState::CCryDX12DepthStencilState(const D3D11_DEPTH_STENCIL_DESC& desc11, const D3D12_DEPTH_STENCIL_DESC& desc12)
	: Super(nullptr, nullptr)
	, m_Desc11(desc11)
	, m_Desc12(desc12)
{

}

#pragma region /* ID3D11DepthStencilState implementation */

void STDMETHODCALLTYPE CCryDX12DepthStencilState::GetDesc(
  _Out_ D3D11_DEPTH_STENCIL_DESC* pDesc)
{
	if (pDesc)
	{
		*pDesc = m_Desc11;
	}
}

#pragma endregion
