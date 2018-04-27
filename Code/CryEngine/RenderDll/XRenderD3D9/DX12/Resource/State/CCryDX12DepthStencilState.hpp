// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include "DX12/Device/CCryDX12DeviceChild.hpp"

class CCryDX12DepthStencilState : public CCryDX12DeviceChild<ID3D11DepthStencilStateToImplement>
{
public:
	DX12_OBJECT(CCryDX12DepthStencilState, CCryDX12DeviceChild<ID3D11DepthStencilStateToImplement> );

	static CCryDX12DepthStencilState* Create(const D3D11_DEPTH_STENCIL_DESC* pDepthStencilDesc);

	const D3D12_DEPTH_STENCIL_DESC& GetD3D12DepthStencilDesc() const
	{
		return m_Desc12;
	}

	#pragma region /* ID3D11DepthStencilState implementation */

	VIRTUALGFX void STDMETHODCALLTYPE GetDesc(
	  _Out_ D3D11_DEPTH_STENCIL_DESC* pDesc) FINALGFX;

	#pragma endregion

protected:
	CCryDX12DepthStencilState(const D3D11_DEPTH_STENCIL_DESC& desc11, const D3D12_DEPTH_STENCIL_DESC& desc12);

private:
	D3D11_DEPTH_STENCIL_DESC m_Desc11;
	D3D12_DEPTH_STENCIL_DESC m_Desc12;
};
