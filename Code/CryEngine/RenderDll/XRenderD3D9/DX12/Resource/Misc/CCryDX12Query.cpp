// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "CCryDX12Query.hpp"

CCryDX12Query* CCryDX12Query::Create(ID3D12Device* pDevice, const D3D11_QUERY_DESC* pDesc)
{
	switch (pDesc->Query)
	{
	case D3D11_QUERY_EVENT:
		{
			auto pResult = DX12_NEW_RAW(CCryDX12EventQuery(pDesc));
			if (!pResult->Init(pDevice))
				SAFE_RELEASE(pResult);

			return pResult;
		}
		break;
	case D3D11_QUERY_TIMESTAMP:
	case D3D11_QUERY_OCCLUSION:
	case D3D11_QUERY_OCCLUSION_PREDICATE:
		{
			auto pResult = DX12_NEW_RAW(CCryDX12ResourceQuery(pDesc));
			if (!pResult->Init(pDevice))
				SAFE_RELEASE(pResult);

			return pResult;
		}
		break;

	case D3D11_QUERY_TIMESTAMP_DISJOINT:
	default:
		return DX12_NEW_RAW(CCryDX12Query(pDesc));
	}
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

CCryDX12Query::CCryDX12Query(const D3D11_QUERY_DESC* pDesc)
	: Super()
	, m_Desc(*pDesc)
{

}

/* ID3D11Query implementation */

void STDMETHODCALLTYPE CCryDX12Query::GetDesc(
  _Out_ D3D11_QUERY_DESC* pDesc)
{
	*pDesc = m_Desc;
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

CCryDX12EventQuery::CCryDX12EventQuery(const D3D11_QUERY_DESC* pDesc)
	: Super(pDesc)
	, m_FenceValue(0)
{
}

bool CCryDX12EventQuery::Init(ID3D12Device* pDevice)
{
	m_FenceValue = 0LL;
	return true;
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

CCryDX12ResourceQuery::CCryDX12ResourceQuery(const D3D11_QUERY_DESC* pDesc)
	: Super(pDesc)
	, m_QueryIndex(0)
	, m_pResource(nullptr)
{
}

bool CCryDX12ResourceQuery::Init(ID3D12Device* pDevice)
{
	Super::Init(pDevice);
	m_QueryIndex = 0;
	m_pResource = nullptr;
	return true;
}
