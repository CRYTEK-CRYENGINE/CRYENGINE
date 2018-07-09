// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "DX12DescriptorHeap.hpp"

namespace NCryDX12
{

//---------------------------------------------------------------------------------------------------------------------
CDescriptorHeap::CDescriptorHeap(CDevice* device)
	: CDeviceObject(device)
{

}

//---------------------------------------------------------------------------------------------------------------------
CDescriptorHeap::~CDescriptorHeap()
{

}

//---------------------------------------------------------------------------------------------------------------------
bool CDescriptorHeap::Init(const D3D12_DESCRIPTOR_HEAP_DESC& desc)
{
	if (!m_pDescriptorHeap)
	{
		ID3D12DescriptorHeap* heap;
		GetDevice()->GetD3D12Device()->CreateDescriptorHeap(&desc, IID_GFX_ARGS(&heap));

		m_pDescriptorHeap = heap;
		heap->Release();

		m_Desc12 = m_pDescriptorHeap->GetDesc();
		m_HeapStartCPU = m_pDescriptorHeap->GetCPUDescriptorHandleForHeapStart();
		m_HeapStartGPU = m_pDescriptorHeap->GetGPUDescriptorHandleForHeapStart();
		m_DescSize = GetDevice()->GetD3D12Device()->GetDescriptorHandleIncrementSize(m_Desc12.Type);
	}

	Reset();
	return true;
}

CDescriptorBlock::CDescriptorBlock(const SDescriptorBlock& block)
{
	m_pDescriptorHeap = reinterpret_cast<CDescriptorHeap*>(block.pBuffer);
	m_BlockStart = block.offset;
	m_Capacity = block.size;
	m_Cursor = 0;
}

CDescriptorBlock& CDescriptorBlock::operator=(const SDescriptorBlock& block)
{
	m_pDescriptorHeap = reinterpret_cast<CDescriptorHeap*>(block.pBuffer);
	m_BlockStart = block.offset;
	m_Capacity = block.size;
	m_Cursor = 0;

	return *this;
}

}
