// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#ifndef IGENERALMEMORYHEAP_H
#define IGENERALMEMORYHEAP_H

class IGeneralMemoryHeap
{
public:
	// <interfuscator:shuffle>
	virtual bool  Cleanup() = 0;

	virtual int   AddRef() = 0;
	virtual int   Release() = 0;

	virtual bool  IsInAddressRange(void* ptr) const = 0;

	virtual void* Calloc(size_t nmemb, size_t size, const char* sUsage) = 0;
	virtual void* Malloc(size_t sz, const char* sUsage) = 0;

	//! Attempts to free the allocation. Returns the size of the allocation if successful, 0 if the heap doesn't own the address.
	virtual size_t Free(void* ptr) = 0;
	virtual void*  Realloc(void* ptr, size_t sz, const char* sUsage) = 0;
	virtual void*  ReallocAlign(void* ptr, size_t size, size_t alignment, const char* sUsage) = 0;
	virtual void*  Memalign(size_t boundary, size_t size, const char* sUsage) = 0;

	//! Get the size of the allocation. Returns 0 if the ptr doesn't belong to the heap.
	virtual size_t UsableSize(void* ptr) const = 0;
	// </interfuscator:shuffle>
protected:
	virtual ~IGeneralMemoryHeap() {}
};

#endif
