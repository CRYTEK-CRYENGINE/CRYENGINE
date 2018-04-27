// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include "DX12/Device/CCryDX12DeviceChild.hpp"

#include "DX12/API/DX12SamplerState.hpp"

class CCryDX12SamplerState : public CCryDX12DeviceChild<ID3D11SamplerStateToImplement>
{
public:
	DX12_OBJECT(CCryDX12SamplerState, CCryDX12DeviceChild<ID3D11SamplerStateToImplement> );

	static CCryDX12SamplerState* Create(const D3D11_SAMPLER_DESC* pSamplerDesc);

	NCryDX12::CSamplerState&     GetDX12SamplerState()
	{
		return m_DX12SamplerState;
	}

	#pragma region /* ID3D11SamplerState implementation */

	VIRTUALGFX void STDMETHODCALLTYPE GetDesc(
	  _Out_ D3D11_SAMPLER_DESC* pDesc) FINALGFX;

	#pragma endregion

protected:
	CCryDX12SamplerState(const D3D11_SAMPLER_DESC& desc11, const D3D12_SAMPLER_DESC& desc12)
		: Super(nullptr, nullptr)
		, m_Desc11(desc11)
	{
		m_DX12SamplerState.GetSamplerDesc() = desc12;
	}

private:
	D3D11_SAMPLER_DESC      m_Desc11;

	NCryDX12::CSamplerState m_DX12SamplerState;
};
