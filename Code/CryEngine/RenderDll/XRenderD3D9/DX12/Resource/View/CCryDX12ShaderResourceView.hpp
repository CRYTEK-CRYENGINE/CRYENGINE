// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include "DX12/Resource/CCryDX12View.hpp"

class CCryDX12ShaderResourceView : public CCryDX12View<ID3D11ShaderResourceViewToImplement>
{
public:
	DX12_OBJECT(CCryDX12ShaderResourceView, CCryDX12View<ID3D11ShaderResourceViewToImplement> );

	static CCryDX12ShaderResourceView* Create(CCryDX12Device* pDevice, ID3D11Resource* pResource, const D3D11_SHADER_RESOURCE_VIEW_DESC* pDesc);

	#pragma region /* ID3D11ShaderResourceView implementation */

	VIRTUALGFX void STDMETHODCALLTYPE GetDesc(
	  _Out_ D3D11_SHADER_RESOURCE_VIEW_DESC* pDesc) FINALGFX;

	#pragma endregion

	template<class T>
	void BeginResourceStateTransition(T* pCmdList)
	{
		GetDX12View().GetDX12Resource().VerifyBackBuffer();
		pCmdList->BeginResourceStateTransition(GetDX12View().GetDX12Resource(), GetDX12View(), D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE | D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
	}

	template<class T>
	void EndResourceStateTransition(T* pCmdList)
	{
		GetDX12View().GetDX12Resource().VerifyBackBuffer();
		pCmdList->EndResourceStateTransition(GetDX12View().GetDX12Resource(), GetDX12View(), D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE | D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
	}

protected:
	CCryDX12ShaderResourceView(ID3D11Resource* pResource11, const D3D11_SHADER_RESOURCE_VIEW_DESC& desc11, ID3D12Resource* pResource12, const D3D12_SHADER_RESOURCE_VIEW_DESC& desc12);

private:
	D3D11_SHADER_RESOURCE_VIEW_DESC m_Desc11;
};
