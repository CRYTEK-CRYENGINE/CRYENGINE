// Copyright 2001-2017 Crytek GmbH / Crytek Group. All rights reserved. 


#pragma once

#include <atomic>
#include <array>

#define CMDQUEUE_IMMEDIATE 0
#define CMDQUEUE_NUM       1

#define CMDTYPE_READ       0
#define CMDTYPE_WRITE      1
#define CMDTYPE_ANY        2
#define CMDTYPE_NUM        3

// *INDENT-OFF*
namespace std
{
ILINE UINT64 (&max(UINT64 (&c)[CMDQUEUE_NUM], const UINT64 (&a)[CMDQUEUE_NUM], const UINT64 (&b)[CMDQUEUE_NUM]))[CMDQUEUE_NUM]
{
	c[CMDQUEUE_IMMEDIATE] = max(a[CMDQUEUE_IMMEDIATE], b[CMDQUEUE_IMMEDIATE]);

	return c;
}

ILINE void upr(UINT64& RESTRICT_REFERENCE a, const UINT64 b)
{
	// Generate conditional moves instead of branches
	a = a >= b ? a : b;
}

ILINE void upr(UINT64 (&RESTRICT_REFERENCE a)[CMDQUEUE_NUM], const UINT64 (&b)[CMDQUEUE_NUM])
{
	upr(a[CMDQUEUE_IMMEDIATE], b[CMDQUEUE_IMMEDIATE]);
}
}

namespace NCryDX11
{

typedef std::array<ID3D11Fence*, 64> DX11NaryFence;

typedef std::atomic<UINT64> FVAL64;
typedef FVAL64              FSET64[CMDQUEUE_NUM];

ILINE UINT64 MaxFenceValue(FVAL64& a, const UINT64& b)
{
	UINT64 utilizedValue = b;
	UINT64 previousValue = a;
	while (previousValue < utilizedValue &&
	       !a.compare_exchange_weak(previousValue, utilizedValue))
		;

	return previousValue;
}

ILINE UINT64 (&MaxFenceValues(UINT64 (&c)[CMDQUEUE_NUM], const FVAL64 (&a)[CMDQUEUE_NUM], const FVAL64 (&b)[CMDQUEUE_NUM]))[CMDQUEUE_NUM]
{
	c[CMDQUEUE_IMMEDIATE] = std::max(a[CMDQUEUE_IMMEDIATE], b[CMDQUEUE_IMMEDIATE]);

	return c;
}
// *INDENT-ON*

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

class CCommandListFence
{
public:
	CCommandListFence(CDevice* device);
	~CCommandListFence();

	bool Init();

	ILINE const DX11NaryFence* GetFence() threadsafe_const
	{
		return &m_Fence;
	}

	ILINE UINT64 GetCurrentValue() threadsafe_const
	{
		return m_CurrentValue;
	}

	ILINE void SetCurrentValue(UINT64 fenceValue) threadsafe
	{
	#ifdef DX11_IN_ORDER_ACQUIRATION
		DX11_ASSERT(m_CurrentValue <= fenceValue, "Setting new fence which is older than the current!");
		// We do not allow smaller fences being submitted, and fences always submit in-order, no max() neccessary
		m_CurrentValue = fenceValue;
	#else
		// CLs may submit in any order. Is it higher than last known completed fence? If so, update it!
		MaxFenceValue(m_CurrentValue, fenceValue);
	#endif
	}

	ILINE bool IsCompleted(UINT64 fenceValue) threadsafe_const
	{
		// Check against last known completed value first to avoid unnecessary fence read
		return
		  (m_LastCompletedValue >= fenceValue) || (AdvanceCompletion() >= fenceValue);
	}

	void WaitForFence(UINT64 fenceValue) threadsafe;

	UINT64 ProbeCompletion() threadsafe_const;
	UINT64 AdvanceCompletion() threadsafe_const;

	ILINE UINT64 GetLastCompletedFenceValue() threadsafe_const
	{
		return m_LastCompletedValue;
	}

private:
	CDevice*          m_pDevice;
	DX11NaryFence     m_Fence;

	FVAL64            m_CurrentValue;
	FVAL64            m_SubmittedValue;
	mutable FVAL64    m_LastCompletedValue;
	mutable CryRWLock m_FenceAccessLock; // Need to synchronize calls to vkResetFences
};

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

class CCommandListFenceSet
{
public:
	CCommandListFenceSet(CDevice* device);
	~CCommandListFenceSet();

	bool                Init();

	ILINE const DX11NaryFence* GetD3D11Fences() threadsafe_const
	{
		return m_Fences;
	}

	ILINE DX11NaryFence* GetD3D11Fence(const int id) threadsafe
	{
		return &m_Fences[id];
	}

	ILINE UINT64 GetSignalledValue(const int id) threadsafe_const
	{
		return m_SignalledValues[id];
	}

	ILINE void SetSignalledValue(const UINT64 fenceValue, const int id) threadsafe
	{
	#ifdef DX11_IN_ORDER_SUBMISSION
		DX11_ASSERT(m_SignalledValues[id] <= fenceValue, "Setting new fence which is older than the signalled!");
		// We do not allow smaller fences being signalled, and fences always signal in-order, no max() neccessary
		m_SignalledValues[id] = fenceValue;
	#else
		// CLs may signal in any order. Is it higher than last known signalled fence? If so, update it!
		MaxFenceValue(m_SignalledValues[id], fenceValue);
	#endif
	}

	ILINE UINT64 GetSubmittedValue(const int id) threadsafe_const
	{
		return m_SubmittedValues[id];
	}

	ILINE void SetSubmittedValue(const UINT64 fenceValue, const int id) threadsafe
	{
	#ifdef DX11_IN_ORDER_SUBMISSION
		DX11_ASSERT(m_SubmittedValues[id] <= fenceValue, "Setting new fence which is older than the submitted!");
		// We do not allow smaller fences being submitted, and fences always submit in-order, no max() neccessary
		m_SubmittedValues[id] = fenceValue;
	#else
		// CLs may submit in any order. Is it higher than last known submitted fence? If so, update it!
		MaxFenceValue(m_SubmittedValues[id], fenceValue);
	#endif
	}

	ILINE void GetSubmittedValues(UINT64 (&fenceValues)[CMDQUEUE_NUM]) threadsafe_const
	{
		fenceValues[CMDQUEUE_IMMEDIATE] = m_SubmittedValues[CMDQUEUE_IMMEDIATE];
	}

	ILINE const FVAL64 (&GetSubmittedValues() threadsafe_const)[CMDQUEUE_NUM]
	{
		return m_SubmittedValues;
	}

	ILINE UINT64 GetCurrentValue(const int id) threadsafe_const
	{
		return m_CurrentValues[id];
	}

	ILINE void GetCurrentValues(UINT64 (&fenceValues)[CMDQUEUE_NUM]) threadsafe_const
	{
		fenceValues[CMDQUEUE_IMMEDIATE] = m_CurrentValues[CMDQUEUE_IMMEDIATE];
	}

	ILINE const FVAL64 (&GetCurrentValues() threadsafe_const)[CMDQUEUE_NUM]
	{
		return m_CurrentValues;
	}

	// thread-save
	ILINE void SetCurrentValue(const UINT64 fenceValue, const int id) threadsafe
	{
	#ifdef DX11_IN_ORDER_ACQUIRATION
		DX11_ASSERT(m_CurrentValues[id] <= fenceValue, "Setting new fence which is older than the current!");
		// We do not allow smaller fences being submitted, and fences always submit in-order, no max() neccessary
		m_CurrentValues[id] = fenceValue;
	#else
		// CLs may submit in any order. Is it higher than last known completed fence? If so, update it!
		MaxFenceValue(m_CurrentValues[id], fenceValue);
	#endif
	}

	ILINE bool IsCompleted(const UINT64 fenceValue, const int id) threadsafe_const
	{
		// Check against last known completed value first to avoid unnecessary fence read
		return
		  (m_LastCompletedValues[id] >= fenceValue) || (AdvanceCompletion(id) >= fenceValue);
	}

	ILINE bool IsCompleted(const UINT64 (&fenceValues)[CMDQUEUE_NUM]) threadsafe_const
	{
		// Check against last known completed value first to avoid unnecessary fence read
		return
		  ((m_LastCompletedValues[CMDQUEUE_IMMEDIATE] >= fenceValues[CMDQUEUE_IMMEDIATE]) || (AdvanceCompletion(CMDQUEUE_IMMEDIATE) >= fenceValues[CMDQUEUE_IMMEDIATE]));
	}

	void SignalFence(const UINT64 fenceValue, const int id) threadsafe_const;
	void WaitForFence(const UINT64 fenceValue, const int id) threadsafe_const;
	void WaitForFence(const UINT64 (&fenceValues)[CMDQUEUE_NUM]) threadsafe_const;

	UINT64 ProbeCompletion(const int id) threadsafe_const;

	UINT64 AdvanceCompletion(const int id) threadsafe_const
	{
		// Check current completed fence
		UINT64 currentCompletedValue = ProbeCompletion(id);

		if (m_LastCompletedValues[id] < currentCompletedValue)
		{
			DX11_LOG(DX11_FENCE_ANALYZER, "Completed GPU fence %s: %lld to %lld", id == CMDQUEUE_IMMEDIATE ? "imm" : "def", m_LastCompletedValues[id] + 1, currentCompletedValue);
		}

#ifdef DX11_IN_ORDER_TERMINATION
		DX11_ASSERT(m_LastCompletedValues[id] <= currentCompletedValue, "Getting new fence which is older than the last!");
		// We do not allow smaller fences being submitted, and fences always complete in-order, no max() neccessary
		m_LastCompletedValues[id] = currentCompletedValue;
#else
		// CLs may terminate in any order. Is it higher than last known completed fence? If so, update it!
		MaxFenceValue(m_LastCompletedValues[id], currentCompletedValue);
#endif

		return currentCompletedValue;
	}

	ILINE void AdvanceCompletion() threadsafe_const
	{
		AdvanceCompletion(CMDQUEUE_IMMEDIATE);
	}

	ILINE UINT64 GetLastCompletedFenceValue(const int id) threadsafe_const
	{
		return m_LastCompletedValues[id];
	}

	ILINE void GetLastCompletedFenceValues(UINT64 (&fenceValues)[CMDQUEUE_NUM]) threadsafe_const
	{
		fenceValues[CMDQUEUE_IMMEDIATE] = m_LastCompletedValues[CMDQUEUE_IMMEDIATE];
	}

	ILINE const FVAL64 (&GetLastCompletedFenceValues() threadsafe_const)[CMDQUEUE_NUM]
	{
		return m_LastCompletedValues;
	}

private:
	CDevice*        m_pDevice;
	DX11NaryFence   m_Fences[CMDQUEUE_NUM];

	// Maximum fence-value of all command-lists currently in flight (allocated, running or free)
	FVAL64 m_CurrentValues  [CMDQUEUE_NUM];
	// Maximum fence-value of all command-lists passed to the driver (running only)
	FVAL64 m_SubmittedValues[CMDQUEUE_NUM]; // passed to async
	FVAL64 m_SignalledValues[CMDQUEUE_NUM]; // processed by async

	// Maximum fence-value of all command-lists executed by the driver (free only)
	mutable FVAL64 m_LastCompletedValues[CMDQUEUE_NUM];
};

}
