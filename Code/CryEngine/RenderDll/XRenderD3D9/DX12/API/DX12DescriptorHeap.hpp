// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include "DX12Base.hpp"

struct SDescriptorBlock;

namespace NCryDX12
{

struct SCursorHost
{
	SCursorHost() : m_Cursor(0) {}
	SCursorHost(SCursorHost&& other) : m_Cursor(std::move(other.m_Cursor)) {}
	SCursorHost& operator=(SCursorHost&& other) { m_Cursor = std::move(other.m_Cursor); return *this; }

	ILINE void SetCursor(UINT value)          { m_Cursor = value; }
	ILINE UINT GetCursor() const              { return m_Cursor; }
	ILINE void IncrementCursor(UINT step = 1) { m_Cursor += step; }
	ILINE void Reset()                        { m_Cursor = 0; }

	UINT       m_Cursor;
};

class CDescriptorHeap : public CDeviceObject, public SCursorHost
{
public:
	CDescriptorHeap(CDevice* device);
	virtual ~CDescriptorHeap();

	bool                        Init(const D3D12_DESCRIPTOR_HEAP_DESC& desc);

	ILINE ID3D12DescriptorHeap* GetD3D12DescriptorHeap() const
	{
		return /*PassAddRef*/ (m_pDescriptorHeap);
	}

	ILINE uint GetDescriptorSize() const
	{
		return m_DescSize;
	}

	ILINE UINT GetCapacity() const
	{
		return m_Desc12.NumDescriptors;
	}

	ILINE CD3DX12_CPU_DESCRIPTOR_HANDLE GetHandleOffsetCPU(INT offset) const
	{
		return CD3DX12_CPU_DESCRIPTOR_HANDLE(m_HeapStartCPU, m_Cursor + offset, m_DescSize);
	}
	ILINE CD3DX12_CPU_DESCRIPTOR_HANDLE GetHandleOffsetCPU_R(INT offset) const
	{
		return CD3DX12_CPU_DESCRIPTOR_HANDLE(m_HeapStartCPU, offset, m_DescSize);
	}
	ILINE CD3DX12_GPU_DESCRIPTOR_HANDLE GetHandleOffsetGPU(INT offset) const
	{
		return CD3DX12_GPU_DESCRIPTOR_HANDLE(m_HeapStartGPU, m_Cursor + offset, m_DescSize);
	}
	ILINE CD3DX12_GPU_DESCRIPTOR_HANDLE GetHandleOffsetGPU_R(INT offset) const
	{
		return CD3DX12_GPU_DESCRIPTOR_HANDLE(m_HeapStartGPU, offset, m_DescSize);
	}

	ILINE D3D12_GPU_DESCRIPTOR_HANDLE GetHandleGPUFromCPU(D3D12_CPU_DESCRIPTOR_HANDLE handle) const
	{
		//DX12_ASSERT(GetHandleOffsetCPU(0).ptr <= handle.ptr && handle.ptr < GetHandleOffsetCPU(GetCapacity()).ptr);
		D3D12_GPU_DESCRIPTOR_HANDLE rebase;
		rebase.ptr = m_HeapStartGPU.ptr + UINT64(handle.ptr - m_HeapStartCPU.ptr);
		return rebase;
	}
	ILINE D3D12_CPU_DESCRIPTOR_HANDLE GetHandleCPUFromGPU(D3D12_GPU_DESCRIPTOR_HANDLE handle) const
	{
		//DX12_ASSERT(GetHandleOffsetGPU(0).ptr <= handle.ptr && handle.ptr < GetHandleOffsetGPU(GetCapacity()).ptr);
		D3D12_CPU_DESCRIPTOR_HANDLE rebase;
		rebase.ptr = m_HeapStartCPU.ptr + SIZE_T(handle.ptr - m_HeapStartGPU.ptr);
		return rebase;
	}

private:
	DX12_PTR(ID3D12DescriptorHeap) m_pDescriptorHeap;
	D3D12_DESCRIPTOR_HEAP_DESC  m_Desc12;
	D3D12_CPU_DESCRIPTOR_HANDLE m_HeapStartCPU;
	D3D12_GPU_DESCRIPTOR_HANDLE m_HeapStartGPU;
	UINT                        m_DescSize;
};

class CDescriptorBlock : public SCursorHost, private NoCopy
{
public:
	CDescriptorBlock()
		: m_BlockStart(0)
		, m_Capacity(0)
	{}

	CDescriptorBlock(CDescriptorBlock&& other)
		: SCursorHost(std::move(other))
		, m_pDescriptorHeap(std::move(other.m_pDescriptorHeap))
		, m_BlockStart(std::move(other.m_BlockStart))
		, m_Capacity(std::move(other.m_Capacity))
	{}

	CDescriptorBlock(CDescriptorHeap* pHeap, UINT cursor, UINT capacity)
		: m_pDescriptorHeap(pHeap)
		, m_BlockStart(cursor)
		, m_Capacity(capacity)
	{
		DX12_ASSERT(cursor + capacity <= pHeap->GetCapacity());
	}

	CDescriptorBlock(const SDescriptorBlock& block);

	CDescriptorBlock& operator=(CDescriptorBlock&& other)
	{
		SCursorHost::operator=(std::move(other));
		m_pDescriptorHeap = std::move(other.m_pDescriptorHeap);
		m_BlockStart = std::move(other.m_BlockStart);
		m_Capacity = std::move(other.m_Capacity);

		return *this;
	}

	CDescriptorBlock& operator=(const SDescriptorBlock& block);

	ILINE CDescriptorHeap* GetDescriptorHeap() const
	{
		return m_pDescriptorHeap.get();
	}

	ILINE UINT GetDescriptorSize() const
	{
		return m_pDescriptorHeap->GetDescriptorSize();
	}

	ILINE UINT GetCapacity() const
	{
		return m_Capacity;
	}

	ILINE D3D12_CPU_DESCRIPTOR_HANDLE GetHandleOffsetCPU(INT offset) const
	{
		DX12_ASSERT((offset < 0) || (m_Cursor + offset < m_Capacity));
		return m_pDescriptorHeap->GetHandleOffsetCPU_R(static_cast<INT>(m_BlockStart + m_Cursor) + offset);
	}
	ILINE D3D12_GPU_DESCRIPTOR_HANDLE GetHandleOffsetGPU(INT offset) const
	{
		DX12_ASSERT((offset < 0) || (m_Cursor + offset < m_Capacity));
		return m_pDescriptorHeap->GetHandleOffsetGPU_R(static_cast<INT>(m_BlockStart + m_Cursor) + offset);
	}

	ILINE D3D12_GPU_DESCRIPTOR_HANDLE GetHandleGPUFromCPU(D3D12_CPU_DESCRIPTOR_HANDLE handle) const
	{
		DX12_ASSERT(m_pDescriptorHeap->GetHandleOffsetCPU(m_BlockStart).ptr <= handle.ptr && handle.ptr < m_pDescriptorHeap->GetHandleOffsetCPU(m_BlockStart + GetCapacity()).ptr);
		return m_pDescriptorHeap->GetHandleGPUFromCPU(handle);
	}
	ILINE D3D12_CPU_DESCRIPTOR_HANDLE GetHandleCPUFromGPU(D3D12_GPU_DESCRIPTOR_HANDLE handle) const
	{
		DX12_ASSERT(m_pDescriptorHeap->GetHandleOffsetGPU(m_BlockStart).ptr <= handle.ptr && handle.ptr < m_pDescriptorHeap->GetHandleOffsetGPU(m_BlockStart + GetCapacity()).ptr);
		return m_pDescriptorHeap->GetHandleCPUFromGPU(handle);
	}

	ILINE uint32 GetStartOffset() const { return m_BlockStart; }

private:
	DX12_PTR(CDescriptorHeap) m_pDescriptorHeap;
	UINT m_BlockStart;
	UINT m_Capacity;
};

}
