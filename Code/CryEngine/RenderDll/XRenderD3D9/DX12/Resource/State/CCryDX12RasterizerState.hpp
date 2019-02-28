// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include "DX12/Device/CCryDX12DeviceChild.hpp"

class CCryDX12RasterizerState : public CCryDX12DeviceChild<ID3D11RasterizerStateToImplement>
{
public:
	DX12_OBJECT(CCryDX12RasterizerState, CCryDX12DeviceChild<ID3D11RasterizerStateToImplement> );

	static CCryDX12RasterizerState* Create(const D3D11_RASTERIZER_DESC* pRasterizerDesc);

	const D3D12_RASTERIZER_DESC& GetD3D12RasterizerDesc() const
	{
		return m_Desc12;
	}
	const D3D11_RASTERIZER_DESC& GetD3D11RasterizerDesc() const
	{
		return m_Desc11;
	}

	#pragma region /* ID3D11RasterizerState implementation */

	VIRTUALGFX void STDMETHODCALLTYPE GetDesc(
	  _Out_ D3D11_RASTERIZER_DESC* pDesc) FINALGFX;

	#pragma endregion

protected:
	CCryDX12RasterizerState(const D3D11_RASTERIZER_DESC& desc11, const D3D12_RASTERIZER_DESC& desc12);

private:
	D3D11_RASTERIZER_DESC m_Desc11;
	D3D12_RASTERIZER_DESC m_Desc12;
};
