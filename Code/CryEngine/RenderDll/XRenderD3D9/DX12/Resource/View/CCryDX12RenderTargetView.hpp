// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include "DX12/Resource/CCryDX12View.hpp"

class CCryDX12RenderTargetView : public CCryDX12View<ID3D11RenderTargetViewToImplement>
{
public:
	DX12_OBJECT(CCryDX12RenderTargetView, CCryDX12View<ID3D11RenderTargetViewToImplement> );

	static CCryDX12RenderTargetView* Create(CCryDX12Device* pDevice, ID3D11Resource* pResource, const D3D11_RENDER_TARGET_VIEW_DESC* pDesc);

	#pragma region /* ID3D11RenderTargetView implementation */

	VIRTUALGFX void STDMETHODCALLTYPE GetDesc(
	  _Out_ D3D11_RENDER_TARGET_VIEW_DESC* pDesc) FINALGFX
	{
		if (pDesc)
		{
			*pDesc = m_Desc11;
		}
	}

	#pragma endregion

	template<class T>
	void BeginResourceStateTransition(T* pCmdList)
	{
		GetDX12View().GetDX12Resource().VerifyBackBuffer();
		pCmdList->BeginResourceStateTransition(GetDX12View().GetDX12Resource(), GetDX12View(), D3D12_RESOURCE_STATE_RENDER_TARGET);
	}

	template<class T>
	void EndResourceStateTransition(T* pCmdList)
	{
		GetDX12View().GetDX12Resource().VerifyBackBuffer();
		pCmdList->EndResourceStateTransition(GetDX12View().GetDX12Resource(), GetDX12View(), D3D12_RESOURCE_STATE_RENDER_TARGET);
	}

protected:
	CCryDX12RenderTargetView(ID3D11Resource* pResource11, const D3D11_RENDER_TARGET_VIEW_DESC& rDesc11, ID3D12Resource* pResource12, const D3D12_RENDER_TARGET_VIEW_DESC& rDesc12);

	CCryDX12RenderTargetView(ID3D11Resource* pResource11, const D3D11_RENDER_TARGET_VIEW_DESC& rDesc11, ID3D12Resource* pResource12);

private:
	D3D11_RENDER_TARGET_VIEW_DESC m_Desc11;
};
