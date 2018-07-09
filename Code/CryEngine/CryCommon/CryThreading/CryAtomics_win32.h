// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include <intsafe.h>
#include <intrin.h>
#include <CryCore/Assert/CryAssert.h>

//////////////////////////////////////////////////////////////////////////
// Interlocked API
//////////////////////////////////////////////////////////////////////////

// Returns the resulting incremented value
ILINE LONG CryInterlockedIncrement(volatile int* pDst)
{
	static_assert(sizeof(int) == sizeof(LONG), "Unsecured cast. int is not same size as LONG.");
	return _InterlockedIncrement((volatile LONG*)pDst);
}

// Returns the resulting decremented value
ILINE LONG CryInterlockedDecrement(volatile int* pDst)
{
	static_assert(sizeof(int) == sizeof(LONG), "Unsecured cast. int is not same size as LONG.");
	return _InterlockedDecrement((volatile LONG*)pDst);
}

// Returns the resulting added value
ILINE LONG CryInterlockedAdd(volatile int* pDst, int add)
{
	return CryInterlockedAdd((volatile LONG*)pDst, (LONG)add);
}

// Returns the resulting added value
ILINE LONG CryInterlockedAdd(volatile LONG* pDst, LONG add)
{
#if CRY_PLATFORM_X64
	return _InterlockedExchangeAdd(pDst, add) + add;
#else
	return CryInterlockedExchangeAdd(pDst, add) + add;
#endif
}

// Returns the resulting added value
ILINE int64 CryInterlockedAdd(volatile int64* pDst, int64 add)
{
#if CRY_PLATFORM_X64
	static_assert(sizeof(int64) == sizeof(LONGLONG), "Unsecured cast. int64 is not same size as LONGLONG.");
	return (int64)_InterlockedExchangeAdd64((volatile LONGLONG*)pDst, add) + add;
#else
	int64 val;
	int64 sum;
	do {
		val = *pDst;
		sum = val + add;
	}
	while (CryInterlockedCompareExchange64(pDst, sum, val) != val);
	return sum;
#endif
}

// Returns the resulting added value
ILINE size_t CryInterlockedAdd(volatile size_t* pDst, size_t add)
{
#if CRY_PLATFORM_X64
	static_assert(sizeof(size_t) == sizeof(LONG64), "Unsecured cast. size_t is not same size as LONG64.");
	return (size_t)_InterlockedExchangeAdd64((volatile LONG64*)pDst, add) + add;
#else
	static_assert(sizeof(size_t) == sizeof(LONG), "Unsecured cast. size_t is not same size as LONG.");
	return (size_t)CryInterlockedAdd((volatile LONG*)pDst, (LONG)add);
#endif
}

// Returns initial value prior exchange
ILINE LONG CryInterlockedExchange(volatile int* pDst, int exchange)
{
	return CryInterlockedExchange((volatile LONG*)pDst, (LONG)exchange);
}

// Returns initial value prior exchange
ILINE LONG CryInterlockedExchange(volatile LONG* pDst, LONG exchange)
{
	return _InterlockedExchange(pDst, exchange);
}

// Returns initial value prior exchange
ILINE LONG CryInterlockedExchangeAdd(volatile LONG* pDst, LONG value)
{
	return _InterlockedExchangeAdd(pDst, value);
}

// Returns initial value prior exchange
ILINE size_t CryInterlockedExchangeAdd(volatile size_t* pDst, size_t add)
{
#if CRY_PLATFORM_X64
	static_assert(sizeof(size_t) == sizeof(LONGLONG), "Unsecured cast. size_t is not same size as LONGLONG(int64).");
	return _InterlockedExchangeAdd64((volatile LONGLONG*)pDst, add); // intrinsic returns previous value
#else
	static_assert(sizeof(size_t) == sizeof(LONG), "Unsecured cast. size_t is not same size as LONG.");
	return (size_t)CryInterlockedExchangeAdd((volatile LONG*)pDst, (LONG)add); // intrinsic returns previous value
#endif
}

// Returns initial value prior exchange
ILINE LONG CryInterlockedExchangeAnd(volatile LONG* pDst, LONG value)
{
	return _InterlockedAnd(pDst, value);
}

// Returns initial value prior exchange
ILINE LONG CryInterlockedExchangeOr(volatile LONG* pDst, LONG value)
{
	return _InterlockedOr(pDst, value);
}

// Returns initial address prior exchange
ILINE void* CryInterlockedExchangePointer(void* volatile* pDst, void* pExchange)
{
#if CRY_PLATFORM_X64 || _MSC_VER > 1700
	return _InterlockedExchangePointer(pDst, pExchange);
#else
	static_assert(sizeof(void*) == sizeof(LONG), "Unsecured cast. void* is not same size as LONG.");
	return (void*)_InterlockedExchange((LONG volatile*)pDst, (LONG)pExchange);
#endif
}

// Returns initial address prior exchange
ILINE LONG CryInterlockedCompareExchange(LONG volatile* pDst, LONG exchange, LONG comperand)
{
	return _InterlockedCompareExchange(pDst, exchange, comperand);
}

// Returns initial address prior exchange
ILINE void* CryInterlockedCompareExchangePointer(void* volatile* pDst, void* pExchange, void* pComperand)
{
#if CRY_PLATFORM_X64 || _MSC_VER > 1700
	return _InterlockedCompareExchangePointer(pDst, pExchange, pComperand);
#else
	static_assert(sizeof(void*) == sizeof(LONG), "Unsecured cast. void* is not same size as LONG.");
	return (void*)_InterlockedCompareExchange((LONG volatile*)pDst, (LONG)pExchange, (LONG)pComperand);
#endif
}

// Returns initial value prior exchange
ILINE int64 CryInterlockedCompareExchange64(volatile int64* pDst, int64 exchange, int64 compare)
{
	static_assert(sizeof(int64) == sizeof(__int64), "Unsecured cast. int64 is not same size as __int64.");
	return _InterlockedCompareExchange64(pDst, exchange, compare);
}

#if CRY_PLATFORM_64BIT
// Returns initial value prior exchange
ILINE unsigned char CryInterlockedCompareExchange128(volatile int64* pDst, int64 exchangeHigh, int64 exchangeLow, int64* pComparandResult)
{
	static_assert(sizeof(int64) == sizeof(__int64), "Unsecured cast. int64 is not same size as __int64.");
	CRY_ASSERT_MESSAGE((((int64)pDst) & 15) == 0, "The destination data must be 16-byte aligned to avoid a general protection fault.");
	return _InterlockedCompareExchange128(pDst, exchangeHigh, exchangeLow, pComparandResult);
}
#endif
//////////////////////////////////////////////////////////////////////////
// Helper
//////////////////////////////////////////////////////////////////////////

class CSimpleThreadBackOff
{
public:
	static const uint32 kSoftYieldInterval = 0x3FF;
	static const uint32 kHardYieldInterval = 0x1FFF;

public:
	CSimpleThreadBackOff() : m_counter(0) {}

	void reset() { m_counter = 0; }

	void backoff()
	{
		// Note: Not using Sleep(x) and SwitchToThread()
		// Sleep(0): Give OS the CPU ... something none game related could block the core for a while (same for SwitchToThread())
		// Sleep(1): System timer resolution dependent. Usual default is 1/64sec. So the worst case is we have to wait 15.6ms.

		// Simply yield processor (good for hyper threaded systems. Allows the logical core to run)
		_mm_pause();
	}

private:
	int m_counter;
};
