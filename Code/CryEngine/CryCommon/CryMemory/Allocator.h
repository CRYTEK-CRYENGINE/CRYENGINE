// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

// Created by: Scott Peter
//---------------------------------------------------------------------------

#ifndef _ALLOCATOR_H__
#define _ALLOCATOR_H__

#include "CryMemoryAllocator.h"

//! Default allocator implementation.
struct StdAllocator
{
	// Class-specific alloc/free/size functions. Use aligned versions only when necessary.
	template<class T>
	static void* Allocate(T*& p)
	{
		return p = NeedAlign<T>() ?
		           (T*)CryModuleMemalign(sizeof(T), alignof(T)) :
		           (T*)CryModuleMalloc(sizeof(T));
	}

	template<class T>
	static void Deallocate(T* p)
	{
		if (NeedAlign<T>())
			CryModuleMemalignFree(p);
		else
			CryModuleFree(p);
	}

	template<class T>
	static size_t GetMemSize(const T* p)
	{
		return NeedAlign<T>() ?
		       sizeof(T) + alignof(T) :
		       sizeof(T);
	}

	template<typename T>
	void GetMemoryUsage(ICrySizer* pSizer) const { /*nothing*/ }
protected:

	template<class T>
	static bool NeedAlign()
	{ PREFAST_SUPPRESS_WARNING(6326); return alignof(T) > _ALIGNMENT; }
};

//! Delete template function for any allocator.
template<class TAlloc, class T>
void Delete(TAlloc& alloc, T* ptr)
{
	if (ptr)
	{
		ptr->~T();
		alloc.Deallocate(ptr);
	}
}

#endif // _ALLOCATOR_H__
