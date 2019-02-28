// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include "DX12/Device/CCryDX12DeviceChild.hpp"

class CCryDX12BlendState : public CCryDX12DeviceChild<ID3D11BlendStateToImplement>
{
public:
	DX12_OBJECT(CCryDX12BlendState, CCryDX12DeviceChild<ID3D11BlendStateToImplement> );

	static CCryDX12BlendState* Create(const D3D11_BLEND_DESC* pBlendStateDesc);

	const D3D12_BLEND_DESC& GetD3D12BlendDesc() const
	{
		return m_Desc12;
	}

	#pragma region /* ID3D11BlendState implementation */

	VIRTUALGFX void STDMETHODCALLTYPE GetDesc(
	  _Out_ D3D11_BLEND_DESC* pDesc) FINALGFX;

	#pragma endregion

protected:
	CCryDX12BlendState(const D3D11_BLEND_DESC& desc11, const D3D12_BLEND_DESC& desc12);

private:
	D3D11_BLEND_DESC m_Desc11;

	D3D12_BLEND_DESC m_Desc12;
};
