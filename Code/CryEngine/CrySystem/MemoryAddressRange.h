// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#ifndef MEMORYADDRESSRANGE_H
#define MEMORYADDRESSRANGE_H

#include <CryMemory/IMemory.h>

class CMemoryAddressRange : public IMemoryAddressRange
{
public:
	static void*  ReserveSpace(size_t sz);
	static size_t GetSystemPageSize();

public:
	CMemoryAddressRange(char* pBaseAddress, size_t nPageSize, size_t nPageCount, const char* sName);
	CMemoryAddressRange(size_t capacity, const char* name);
	~CMemoryAddressRange();

	ILINE bool IsInRange(void* p) const
	{
		return m_pBaseAddress <= p && p < (m_pBaseAddress + m_nPageSize * m_nPageCount);
	}

public:
	void   Release();

	char*  GetBaseAddress() const;
	size_t GetPageCount() const;
	size_t GetPageSize() const;

	void*  MapPage(size_t pageIdx);
	void   UnmapPage(size_t pageIdx);

private:
	CMemoryAddressRange(const CMemoryAddressRange&);
	CMemoryAddressRange& operator=(const CMemoryAddressRange&);

private:
	char*  m_pBaseAddress;
	size_t m_nPageSize;
	size_t m_nPageCount;
#if CRY_PLATFORM_LINUX || CRY_PLATFORM_ANDROID || CRY_PLATFORM_APPLE
	size_t m_allocatedSpace; // Required to unmap latter on
#endif
};

#endif
