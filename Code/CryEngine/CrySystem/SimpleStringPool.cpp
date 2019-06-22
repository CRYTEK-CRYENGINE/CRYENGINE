// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "SimpleStringPool.h"
#include <CryMemory/CrySizer.h>

CSimpleStringPool::CSimpleStringPool()
{
	m_blockSize = STD_BLOCK_SIZE - offsetof(BLOCK, s);
	m_blocks = 0;
	m_start = 0;
	m_ptr = 0;
	m_end = 0;
	nUsedSpace = 0;
	nUsedBlocks = 0;
	m_free_blocks = 0;
	m_reuseStrings = false;
}

CSimpleStringPool::CSimpleStringPool(bool reuseStrings)
	: m_blockSize(STD_BLOCK_SIZE - offsetof(BLOCK, s))
	, m_blocks(NULL)
	, m_free_blocks(NULL)
	, m_end(0)
	, m_ptr(0)
	, m_start(0)
	, nUsedSpace(0)
	, nUsedBlocks(0)
	, m_reuseStrings(reuseStrings)
{
}

CSimpleStringPool::~CSimpleStringPool()
{
	BLOCK* pBlock = m_blocks;
	while (pBlock)
	{
		BLOCK* temp = pBlock->next;
		g_nTotalAllocInXmlStringPools -= (offsetof(BLOCK, s) + pBlock->size * sizeof(char));
		free(pBlock);
		pBlock = temp;
	}
	pBlock = m_free_blocks;
	while (pBlock)
	{
		BLOCK* temp = pBlock->next;
		g_nTotalAllocInXmlStringPools -= (offsetof(BLOCK, s) + pBlock->size * sizeof(char));
		free(pBlock);
		pBlock = temp;
	}
	m_blocks = 0;
	m_ptr = 0;
	m_start = 0;
	m_end = 0;
}

void CSimpleStringPool::SetBlockSize(unsigned int nBlockSize)
{
	if (nBlockSize > 1024 * 1024)
		nBlockSize = 1024 * 1024;
	unsigned int size = 512;
	while (size < nBlockSize)
		size *= 2;

	m_blockSize = size - offsetof(BLOCK, s);
}

void CSimpleStringPool::Clear()
{
	BLOCK* pLast = m_free_blocks;
	if (pLast)
	{
		while (pLast->next)
			pLast = pLast->next;

		pLast->next = m_blocks;
	}
	else
	{
		m_free_blocks = m_blocks;
	}

	m_blocks = 0;
	m_start = 0;
	m_ptr = 0;
	m_end = 0;
	nUsedSpace = 0;
	if (m_reuseStrings)
	{
		m_stringToExistingStringMap.clear();
	}
}

char * CSimpleStringPool::Append(const char * ptr, int nStrLen)
{

	MEMSTAT_CONTEXT(EMemStatContextType::Other, "StringPool");

	assert(nStrLen <= 100000);

	if (m_reuseStrings)
	{
		if (char* existingString = FindExistingString(ptr, nStrLen))
		{
			return existingString;
		}
	}

	char* ret = m_ptr;
	if (m_ptr && nStrLen + 1 < (m_end - m_ptr))
	{
		memcpy(m_ptr, ptr, nStrLen);
		m_ptr = m_ptr + nStrLen;
		*m_ptr++ = 0; // add null termination.
	}
	else
	{
		int nNewBlockSize = std::max(nStrLen + 1, (int)m_blockSize);
		AllocBlock(nNewBlockSize, nStrLen + 1);
		PREFAST_ASSUME(m_ptr);
		memcpy(m_ptr, ptr, nStrLen);
		m_ptr = m_ptr + nStrLen;
		*m_ptr++ = 0; // add null termination.
		ret = m_start;
	}

	if (m_reuseStrings)
	{
		MEMSTAT_CONTEXT(EMemStatContextType::Other, "String map");
		assert(!FindExistingString(ptr, nStrLen));
		m_stringToExistingStringMap[SStringData(ret, nStrLen)] = ret;
	}

	nUsedSpace += nStrLen;
	return ret;
}

char * CSimpleStringPool::ReplaceString(const char * str1, const char * str2)
{
	if (m_reuseStrings)
	{
		CryFatalError("Can't replace strings in an xml node that reuses strings");
	}

	int nStrLen1 = strlen(str1);
	int nStrLen2 = strlen(str2);

	// undo ptr1 add.
	if (m_ptr != m_start)
		m_ptr = m_ptr - nStrLen1 - 1;

	assert(m_ptr == str1);

	int nStrLen = nStrLen1 + nStrLen2;

	char* ret = m_ptr;
	if (m_ptr && nStrLen + 1 < (m_end - m_ptr))
	{
		if (m_ptr != str1)  memcpy(m_ptr, str1, nStrLen1);
		memcpy(m_ptr + nStrLen1, str2, nStrLen2);
		m_ptr = m_ptr + nStrLen;
		*m_ptr++ = 0; // add null termination.
	}
	else
	{
		int nNewBlockSize = std::max(nStrLen + 1, (int)m_blockSize);
		if (m_ptr == m_start)
		{
			ReallocBlock(nNewBlockSize * 2); // Reallocate current block.
			PREFAST_ASSUME(m_ptr);
			memcpy(m_ptr + nStrLen1, str2, nStrLen2);
		}
		else
		{
			AllocBlock(nNewBlockSize, nStrLen + 1);
			PREFAST_ASSUME(m_ptr);
			memcpy(m_ptr, str1, nStrLen1);
			memcpy(m_ptr + nStrLen1, str2, nStrLen2);
		}

		m_ptr = m_ptr + nStrLen;
		*m_ptr++ = 0; // add null termination.
		ret = m_start;
	}
	nUsedSpace += nStrLen;
	return ret;
}

void CSimpleStringPool::GetMemoryUsage(ICrySizer * pSizer) const
{
	BLOCK* pBlock = m_blocks;
	while (pBlock)
	{
		pSizer->AddObject(pBlock, offsetof(BLOCK, s) + pBlock->size * sizeof(char));
		pBlock = pBlock->next;
	}

	pBlock = m_free_blocks;
	while (pBlock)
	{
		pSizer->AddObject(pBlock, offsetof(BLOCK, s) + pBlock->size * sizeof(char));
		pBlock = pBlock->next;
	}
}

void CSimpleStringPool::AllocBlock(int blockSize, int nMinBlockSize)
{
	if (m_free_blocks)
	{
		BLOCK* pBlock = m_free_blocks;
		BLOCK* pPrev = 0;
		while (pBlock)
		{
			if (pBlock->size >= nMinBlockSize)
			{
				// Reuse free block
				if (pPrev)
					pPrev->next = pBlock->next;
				else
					m_free_blocks = pBlock->next;

				pBlock->next = m_blocks;
				m_blocks = pBlock;
				m_ptr = pBlock->s;
				m_start = pBlock->s;
				m_end = pBlock->s + pBlock->size;
				return;
			}
			pPrev = pBlock;
			pBlock = pBlock->next;
		}

	}
	size_t nMallocSize = offsetof(BLOCK, s) + blockSize * sizeof(char);
	g_nTotalAllocInXmlStringPools += nMallocSize;

	BLOCK* pBlock = (BLOCK*)malloc(nMallocSize);
	;
	assert(pBlock);
	pBlock->size = blockSize;
	pBlock->next = m_blocks;
	m_blocks = pBlock;
	m_ptr = pBlock->s;
	m_start = pBlock->s;
	m_end = pBlock->s + blockSize;
	nUsedBlocks++;
}

void CSimpleStringPool::ReallocBlock(int blockSize)
{
	if (m_reuseStrings)
	{
		CryFatalError("Can't replace strings in an xml node that reuses strings");
	}

	BLOCK* pThisBlock = m_blocks;
	BLOCK* pPrevBlock = m_blocks->next;
	m_blocks = pPrevBlock;

	size_t nMallocSize = offsetof(BLOCK, s) + blockSize * sizeof(char);
	if (pThisBlock)
	{
		g_nTotalAllocInXmlStringPools -= (offsetof(BLOCK, s) + pThisBlock->size * sizeof(char));
	}
	g_nTotalAllocInXmlStringPools += nMallocSize;

	BLOCK* pBlock = (BLOCK*)realloc(pThisBlock, nMallocSize);
	assert(pBlock);
	pBlock->size = blockSize;
	pBlock->next = m_blocks;
	m_blocks = pBlock;
	m_ptr = pBlock->s;
	m_start = pBlock->s;
	m_end = pBlock->s + blockSize;
}

char * CSimpleStringPool::FindExistingString(const char * szString, int nStrLen) const
{
	SStringData testData(szString, nStrLen);
	char* szResult = stl::find_in_map(m_stringToExistingStringMap, testData, NULL);
	assert(!szResult || !stricmp(szResult, szString));
	return szResult;
}
