// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "DX12CommandListFence.hpp"
#include "DriverD3D.h"

namespace NCryDX12
{

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

CCommandListFence::CCommandListFence(CDevice* device)
	: m_pDevice(device)
	, m_LastCompletedValue(0)
	, m_CurrentValue(0)
{
}

//---------------------------------------------------------------------------------------------------------------------
CCommandListFence::~CCommandListFence()
{
	m_pFence->Release();
	CloseHandle(m_FenceEvent);
}

//---------------------------------------------------------------------------------------------------------------------
bool CCommandListFence::Init()
{
	ID3D12Fence* fence = NULL;
	if (S_OK != m_pDevice->GetD3D12Device()->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_GFX_ARGS(&fence)))
	{
		DX12_ERROR("Could not create fence object!");
		return false;
	}

	m_pFence = fence;
	fence->Release();

	m_pFence->Signal(m_LastCompletedValue);
	m_FenceEvent = CreateEventEx(NULL, FALSE, FALSE, EVENT_ALL_ACCESS);

	return true;
}

void CCommandListFence::WaitForFence(UINT64 fenceValue)
{
	if (!IsCompleted(fenceValue))
	{
		DX12_LOG(DX12_FENCE_ANALYZER, "Waiting CPU for fence: %lld (is %lld currently)", fenceValue, m_pFence->GetCompletedValue());
		{
#ifdef DX12_LINKEDADAPTER
			// Waiting in a multi-adapter situation can be more complex
			if (m_pDevice->WaitForCompletion(m_pFence, fenceValue))
#endif
			{
				m_pFence->SetEventOnCompletion(fenceValue, m_FenceEvent);
				WaitForSingleObject(m_FenceEvent, INFINITE);
			}
		}
		DX12_LOG(DX12_FENCE_ANALYZER, "Completed CPU on fence: %lld", m_pFence->GetCompletedValue());

		AdvanceCompletion();
	}
}

//---------------------------------------------------------------------------------------------------------------------
CCommandListFenceSet::CCommandListFenceSet(CDevice* device)
	: m_pDevice(device)
{
	// *INDENT-OFF*
	m_LastCompletedValues[CMDQUEUE_GRAPHICS] = m_LastCompletedValues[CMDQUEUE_COMPUTE] = m_LastCompletedValues[CMDQUEUE_COPY] = 0;
	m_SubmittedValues    [CMDQUEUE_GRAPHICS] = m_SubmittedValues    [CMDQUEUE_COMPUTE] = m_SubmittedValues    [CMDQUEUE_COPY] = 0;
	m_SignalledValues    [CMDQUEUE_GRAPHICS] = m_SignalledValues    [CMDQUEUE_COMPUTE] = m_SignalledValues    [CMDQUEUE_COPY] = 0;
	m_CurrentValues      [CMDQUEUE_GRAPHICS] = m_CurrentValues      [CMDQUEUE_COMPUTE] = m_CurrentValues      [CMDQUEUE_COPY] = 0;
	// *INDENT-ON*
}

//---------------------------------------------------------------------------------------------------------------------
CCommandListFenceSet::~CCommandListFenceSet()
{
	for (int i = 0; i < CMDQUEUE_NUM; ++i)
	{
		m_pFences[i]->Release();

		CloseHandle(m_FenceEvents[i]);
	}
}

//---------------------------------------------------------------------------------------------------------------------
bool CCommandListFenceSet::Init()
{
	for (int i = 0; i < CMDQUEUE_NUM; ++i)
	{
		ID3D12Fence* fence = NULL;
		if (S_OK != m_pDevice->GetD3D12Device()->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_GFX_ARGS(&fence)))
		{
			DX12_ERROR("Could not create fence object!");
			return false;
		}

		m_pFences[i] = fence;
		m_pFences[i]->Signal(m_LastCompletedValues[i]);
		m_FenceEvents[i] = CreateEventEx(NULL, FALSE, FALSE, EVENT_ALL_ACCESS);
	}

	return true;
}

void CCommandListFenceSet::WaitForFence(const UINT64 fenceValue, const int id) const
{
	DX12_LOG(DX12_FENCE_ANALYZER, "Waiting CPU for fence %s: %lld (is %lld currently)",
	         id == CMDQUEUE_GRAPHICS ? "gfx" : id == CMDQUEUE_COMPUTE ? "cmp" : "cpy",
	         fenceValue,
	         m_pFences[id]->GetCompletedValue());

	{
#ifdef DX12_LINKEDADAPTER
		// NOTE: Waiting in a multi-adapter situation can be more complex
		if (!m_pDevice->WaitForCompletion(m_pFences[id], fenceValue))
#endif
		{
			m_pFences[id]->SetEventOnCompletion(fenceValue, m_FenceEvents[id]);
			WaitForSingleObject(m_FenceEvents[id], INFINITE);
		}
	}

	DX12_LOG(DX12_FENCE_ANALYZER, "Completed CPU on fence %s: %lld",
	         id == CMDQUEUE_GRAPHICS ? "gfx" : id == CMDQUEUE_COMPUTE ? "cmp" : "cpy",
	         m_pFences[id]->GetCompletedValue());

	AdvanceCompletion(id);
}

void CCommandListFenceSet::WaitForFence(const UINT64 (&fenceValues)[CMDQUEUE_NUM]) const
{
	// TODO: the pool which waits for the fence can be omitted (in-order-execution)
	DX12_LOG(DX12_FENCE_ANALYZER, "Waiting CPU for fences: [%lld,%lld,%lld] (is [%lld,%lld,%lld] currently)",
	         fenceValues[CMDQUEUE_GRAPHICS],
	         fenceValues[CMDQUEUE_COMPUTE],
	         fenceValues[CMDQUEUE_COPY],
	         m_pFences[CMDQUEUE_GRAPHICS]->GetCompletedValue(),
	         m_pFences[CMDQUEUE_COMPUTE]->GetCompletedValue(),
	         m_pFences[CMDQUEUE_COPY]->GetCompletedValue());

	{
#ifdef DX12_LINKEDADAPTER
		if (m_pDevice->IsMultiAdapter())
		{
			// NOTE: Waiting in a multi-adapter situation can be more complex
			// In this case we can't collect all events to wait for all events at the same time
			for (int id = 0; id < CMDQUEUE_NUM; ++id)
			{
				if (fenceValues[id] && (m_LastCompletedValues[id] < fenceValues[id]))
				{
					m_pDevice->WaitForCompletion(m_pFences[id], fenceValues[id]);
				}
			}
		}
		else
#endif
		{
			// NOTE: event does ONLY trigger when the value has been set (it'll lock when trying with 0)
			int numEvents = 0;
			for (int id = 0; id < CMDQUEUE_NUM; ++id)
			{
				if (fenceValues[id] && (m_LastCompletedValues[id] < fenceValues[id]))
				{
					m_pFences[id]->SetEventOnCompletion(fenceValues[id], m_FenceEvents[numEvents++]);
				}
			}

			if (numEvents)
				WaitForMultipleObjects(numEvents, m_FenceEvents, TRUE, INFINITE);
		}
	}

	DX12_LOG(DX12_FENCE_ANALYZER, "Completed CPU on fences: [%lld,%lld,%lld]",
	         m_pFences[CMDQUEUE_GRAPHICS]->GetCompletedValue(),
	         m_pFences[CMDQUEUE_COMPUTE]->GetCompletedValue(),
	         m_pFences[CMDQUEUE_COPY]->GetCompletedValue());

	AdvanceCompletion();
}

}
