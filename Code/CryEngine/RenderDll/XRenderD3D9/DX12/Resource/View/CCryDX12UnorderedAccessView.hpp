// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include "DX12/Resource/CCryDX12View.hpp"

class CCryDX12UnorderedAccessView : public CCryDX12View<ID3D11UnorderedAccessViewToImplement>
{
public:
	DX12_OBJECT(CCryDX12UnorderedAccessView, CCryDX12View<ID3D11UnorderedAccessViewToImplement> );

	static CCryDX12UnorderedAccessView* Create(CCryDX12Device* pDevice, ID3D11Resource* pResource11, const D3D11_UNORDERED_ACCESS_VIEW_DESC* pDesc11);

	#pragma region /* ID3D11UnorderedAccessView implementation */

	VIRTUALGFX void STDMETHODCALLTYPE GetDesc(
	  _Out_ D3D11_UNORDERED_ACCESS_VIEW_DESC* pDesc) FINALGFX;

	#pragma endregion

	template<class T>
	void BeginResourceStateTransition(T* pCmdList)
	{
		pCmdList->BeginResourceStateTransition(GetDX12View().GetDX12Resource(), GetDX12View(), D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
	}

	template<class T>
	void EndResourceStateTransition(T* pCmdList)
	{
		pCmdList->EndResourceStateTransition(GetDX12View().GetDX12Resource(), GetDX12View(), D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
	}

protected:
	CCryDX12UnorderedAccessView(ID3D11Resource* pResource11, const D3D11_UNORDERED_ACCESS_VIEW_DESC& desc11, ID3D12Resource* pResource12, const D3D12_UNORDERED_ACCESS_VIEW_DESC& desc12);

private:
	D3D11_UNORDERED_ACCESS_VIEW_DESC m_Desc11;
};
