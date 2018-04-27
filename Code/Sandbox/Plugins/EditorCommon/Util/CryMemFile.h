// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#ifndef _CRYMEMFILE_HEADER_
#define _CRYMEMFILE_HEADER_

// derived class to get correct memory allocation/deallocation with custom memory manager - and to avoid memory leaks from calling Detach()
class CCryMemFile : public CMemFile
{
	virtual BYTE* Alloc(SIZE_T nBytes)
	{
		return (BYTE*) malloc(nBytes);
	}

	virtual void Free(BYTE* lpMem)
	{
		/*return*/ free(lpMem);
	}

	virtual BYTE* Realloc(BYTE* lpMem, SIZE_T nBytes)
	{
		return (BYTE*)realloc(lpMem, nBytes);
	}

public: // ---------------------------------------------------------------

	CCryMemFile(UINT nGrowBytes = 1024) : CMemFile(nGrowBytes) {}
	CCryMemFile(BYTE* lpBuffer, UINT nBufferSize, UINT nGrowBytes = 0) : CMemFile(lpBuffer, nBufferSize, nGrowBytes) {}

	virtual ~CCryMemFile()
	{
		Close();    // call Close() to make sure the Free() is using my v-table
	}

	// only for temporary use
	BYTE* GetMemPtr() const
	{
		return m_lpBuffer;
	}

	BYTE* Detach()
	{
		assert(0);    // dangerous - most likely we cause memory leak - better use GetMemPtr
		return 0;
	}
};

#endif // _CRYMEMFILE_HEADER_

