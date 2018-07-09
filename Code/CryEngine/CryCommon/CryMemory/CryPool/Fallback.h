// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

// Created by: Michael Kopietz
// Modified: -
//
//---------------------------------------------------------------------------

#ifndef __CCRYPOOLFALLBACK__
#define __CCRYPOOLFALLBACK__

namespace NCryPoolAlloc
{

enum EFallbackMode
{
	EFM_DISABLED,
	EFM_ENABLED,
	EFM_ALWAYS
};
template<class TAllocator>
class CFallback : public TAllocator
{
	EFallbackMode m_Fallback;
public:
	ILINE CFallback() :
		m_Fallback(EFM_DISABLED)
	{

	}

	template<class T>
	ILINE T Allocate(size_t Size, size_t Align = 1)
	{
		if (EFM_ALWAYS == m_Fallback)
			return reinterpret_cast<T>(CPA_ALLOC(Align, Size));
		T pRet = TAllocator::template Allocate<T>(Size, Align);
		if (!pRet && EFM_ENABLED == m_Fallback)
			return reinterpret_cast<T>(CPA_ALLOC(Align, Size));
		return pRet;
	}

	template<class T>
	ILINE bool Free(T Handle)
	{
		if (!Handle)
			return true;
		if (EFM_ALWAYS == m_Fallback)
		{
			CPA_FREE(Handle);
			return true;
		}

		if (EFM_ENABLED == m_Fallback && TAllocator::InBounds(Handle, true))
		{
			CPA_FREE(Handle);
			return true;
		}
		return TAllocator::template Free<T>(Handle);
	}

	void          FallbackMode(EFallbackMode M) { m_Fallback = M; }
	EFallbackMode FallbaclMode() const          { return m_Fallback; }
};

}

#endif
