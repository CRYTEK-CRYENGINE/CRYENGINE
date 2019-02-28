// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#ifndef PAGEMAPPINGHEAP_H
#define PAGEMAPPINGHEAP_H

#include "MemoryAddressRange.h"

#include <CryMemory/IMemory.h>

class CPageMappingHeap : public IPageMappingHeap
{
public:
	CPageMappingHeap(char* pAddressSpace, size_t nNumPages, size_t nPageSize, const char* sName);
	CPageMappingHeap(size_t addressSpace, const char* sName);
	~CPageMappingHeap();

public: // IPageMappingHeap Members
	virtual void   Release();

	virtual size_t GetGranularity() const;
	virtual bool   IsInAddressRange(void* ptr) const;

	virtual size_t FindLargestFreeBlockSize() const;

	virtual void*  Map(size_t sz);
	virtual void   Unmap(void* ptr, size_t sz);

private:
	CPageMappingHeap(const CPageMappingHeap&);
	CPageMappingHeap& operator=(const CPageMappingHeap&);

private:
	void Init();

private:
	mutable CryCriticalSectionNonRecursive m_lock;
	CMemoryAddressRange                    m_addrRange;
	std::vector<uint32>                    m_pageBitmap;
};

#endif
