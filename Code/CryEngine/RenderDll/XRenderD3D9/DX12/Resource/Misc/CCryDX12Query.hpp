// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include "DX12/Resource/CCryDX12Asynchronous.hpp"

class CCryDX12Query : public CCryDX12Asynchronous<ID3D11QueryToImplement>
{
public:
	DX12_OBJECT(CCryDX12Query, CCryDX12Asynchronous<ID3D11QueryToImplement> );

	static CCryDX12Query* Create(ID3D12Device* pDevice, const D3D11_QUERY_DESC* pDesc);

	#pragma region /* ID3D11Asynchronous implementation */

	VIRTUALGFX UINT STDMETHODCALLTYPE GetDataSize() FINALGFX
	{
		if (m_Desc.Query == D3D11_QUERY_EVENT)
			return sizeof(BOOL);
		else if (m_Desc.Query == D3D11_QUERY_TIMESTAMP)
			return sizeof(UINT64);
		else if (m_Desc.Query == D3D11_QUERY_TIMESTAMP_DISJOINT)
			return sizeof(D3D11_QUERY_DATA_TIMESTAMP_DISJOINT);
		else if (m_Desc.Query == D3D11_QUERY_OCCLUSION)
			return sizeof(UINT64);
		else if (m_Desc.Query == D3D11_QUERY_OCCLUSION_PREDICATE)
			return sizeof(UINT64);
		else if (m_Desc.Query == D3D11_QUERY_PIPELINE_STATISTICS)
			return sizeof(D3D11_QUERY_DATA_PIPELINE_STATISTICS);
		else
			return 0;
	}

	#pragma endregion

	#pragma region /* ID3D11Query implementation */

	VIRTUALGFX void STDMETHODCALLTYPE GetDesc(
	  _Out_ D3D11_QUERY_DESC* pDesc) FINALGFX;

	#pragma endregion

protected:
	CCryDX12Query(const D3D11_QUERY_DESC* pDesc);

	D3D11_QUERY_DESC m_Desc;
};

class CCryDX12EventQuery : public CCryDX12Query
{
public:
	DX12_OBJECT(CCryDX12EventQuery, CCryDX12Query);

	CCryDX12EventQuery(const D3D11_QUERY_DESC* pDesc);

	bool   Init(ID3D12Device* pDevice);

	UINT64 GetFenceValue() const
	{
		return m_FenceValue;
	}

	void SetFenceValue(UINT64 fenceValue)
	{
		m_FenceValue = fenceValue;
	}

private:
	UINT64 m_FenceValue;
};

class CCryDX12ResourceQuery : public CCryDX12EventQuery
{
public:
	DX12_OBJECT(CCryDX12ResourceQuery, CCryDX12EventQuery);

	CCryDX12ResourceQuery(const D3D11_QUERY_DESC* pDesc);

	bool Init(ID3D12Device* pDevice);

	UINT GetQueryIndex() const
	{
		return m_QueryIndex;
	}

	void SetQueryIndex(UINT queryIndex)
	{
		m_QueryIndex = queryIndex;
	}

	ID3D12Resource* GetQueryResource() const
	{
		return m_pResource;
	}

	void SetQueryResource(ID3D12Resource* pResource)
	{
		m_pResource = pResource;
	}

private:
	UINT            m_QueryIndex;
	ID3D12Resource* m_pResource;
};
