// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

//! Abstraction of the virtual memory interface
class CVirtualMemory
{
public:
	CVirtualMemory();

	size_t GetSystemPageSize() const { return s_systemPageSize; }

	//! reserveSizeSize will be rounded up to the system page size
	//! the returned range will always be aligned to the system page size, so setting alignment is rarely necessary
	void* ReserveAddressRange(size_t reserveSize, const char* usage, size_t alignment = 0);
	//! pBase must have been returned by a previous call to ReserveAddressRange
	//! pass the same size and alignment values as for reserving
	void  UnreserveAddressRange(void* pBase, size_t reservedSize, size_t alignment = 0);

	// pBase will be aligned (down) to the system page size and must be in a previously reserved region
	// all pages overlapping the range [pBase, pBase+size[ will be (un)mapped
	void MapPages(void* pBase, size_t size);
	void UnmapPages(void* pBase, size_t size);

	static bool ValidateSystemPageSize();

private:
	static const size_t s_systemPageSize;
};
