// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "PowerOf2BlockPacker.h"

CPowerOf2BlockPacker::CPowerOf2BlockPacker(const uint32 dwLogWidth, const uint32 dwLogHeight)
{
	m_dwWidth = 1 << dwLogWidth;
	m_dwHeight = 1 << dwLogHeight;

	m_nUsedBlocks = 0;
	m_pTexture = NULL;
	m_timeLastUsed = 0.0f;

	m_BlockBitmap.resize(m_dwWidth * m_dwHeight, 0xffffffff);
}

CPowerOf2BlockPacker::~CPowerOf2BlockPacker()
{
	//Clear();
	FreeContainers();
	SAFE_RELEASE(m_pTexture);
}

void CPowerOf2BlockPacker::Clear()
{
	std::fill(m_BlockBitmap.begin(), m_BlockBitmap.end(), 0xffffffff);
	m_Blocks.clear();
	m_nUsedBlocks = 0;
}

uint32 CPowerOf2BlockPacker::GetBlockInfo(const uint32 dwBlockID, uint32& dwMinX, uint32& dwMinY, uint32& dwMaxX, uint32& dwMaxY)
{
	uint32 dwSize = (uint32)m_Blocks.size();

	if (dwBlockID >= dwSize)
		return 0xFFFFFFFF;      // to avoid crash

	SBlockMinMax& ref = m_Blocks[dwBlockID];

	dwMinX = ref.m_dwMinX;
	dwMinY = ref.m_dwMinY;
	dwMaxX = ref.m_dwMaxX;
	dwMaxY = ref.m_dwMaxY;

	if (ref.IsFree())
		return 0xFFFFFFFF;
	else
		return dwBlockID;
}

void CPowerOf2BlockPacker::UpdateSize(int nW, int nH)
{
	m_dwWidth = nW;
	m_dwHeight = nH;

	m_nUsedBlocks = 0;

	m_BlockBitmap.resize(m_dwWidth * m_dwHeight, 0xffffffff);
}

void CPowerOf2BlockPacker::RemoveBlock(const uint32 dwBlockID)
{
	uint32 dwSize = (uint32)m_Blocks.size();

	assert(dwBlockID < dwSize);
	if (dwBlockID >= dwSize)
		return;     // to avoid crash

	SBlockMinMax& ref = m_Blocks[dwBlockID];

	assert(!ref.IsFree());

	FillRect(ref, 0xffffffff);
	m_nUsedBlocks -= (ref.m_dwMaxX - ref.m_dwMinX) * (ref.m_dwMaxY - ref.m_dwMinY);

	ref.MarkFree();
}

uint32 CPowerOf2BlockPacker::AddBlock(const uint32 dwLogWidth, const uint32 dwLogHeight)
{
	uint32 dwLocalWidth = 1 << dwLogWidth;
	uint32 dwLocalHeight = 1 << dwLogHeight;

	for (uint32 dwY = 0; dwY < m_dwHeight; dwY += dwLocalHeight)
		for (uint32 dwX = 0; dwX < m_dwWidth; dwX += dwLocalWidth)
		{
			SBlockMinMax testblock;

			testblock.m_dwMinX = dwX;
			testblock.m_dwMaxX = dwX + dwLocalWidth;
			testblock.m_dwMinY = dwY;
			testblock.m_dwMaxY = dwY + dwLocalHeight;

			if (IsFree(testblock))
			{
				uint32 dwBlockID = FindFreeBlockIDOrCreateNew();

				m_Blocks[dwBlockID] = testblock;

				FillRect(testblock, dwBlockID);

				m_nUsedBlocks += dwLocalWidth * dwLocalHeight;

				return dwBlockID;
			}
		}

	return 0xffffffff;  // no space left to this block
}

void CPowerOf2BlockPacker::FillRect(const SBlockMinMax& rect, uint32 dwValue)
{
	for (uint32 dwY = rect.m_dwMinY; dwY < rect.m_dwMaxY; ++dwY)
		for (uint32 dwX = rect.m_dwMinX; dwX < rect.m_dwMaxX; ++dwX)
			m_BlockBitmap[dwX + dwY * m_dwWidth] = dwValue;
}

bool CPowerOf2BlockPacker::IsFree(const SBlockMinMax& rect)
{
	for (uint32 dwY = rect.m_dwMinY; dwY < rect.m_dwMaxY; ++dwY)
		for (uint32 dwX = rect.m_dwMinX; dwX < rect.m_dwMaxX; ++dwX)
			if (m_BlockBitmap.size() <= dwX + dwY * m_dwWidth || m_BlockBitmap[dwX + dwY * m_dwWidth] != 0xffffffff)
				return false;

	return true;
}

uint32 CPowerOf2BlockPacker::GetRandomBlock() const
{
	std::vector<SBlockMinMax>::const_iterator it, end = m_Blocks.end();

	uint32 dwCnt = 0;

	for (it = m_Blocks.begin(); it != end; ++it)
	{
		const SBlockMinMax& ref = *it;

		if (!ref.IsFree())
			dwCnt++;
	}

	if (dwCnt == 0)
		return 0xffffffff;

	uint32 dwI = 0;
	uint32 dwRndI = 0;
	uint32 dwRnd = cry_random(0U, dwCnt - 1);

	for (it = m_Blocks.begin(); it != end; ++it, ++dwI)
	{
		const SBlockMinMax& ref = *it;

		if (!ref.IsFree())
		{
			if (dwRndI == dwRnd)
				return dwI;

			++dwRndI;
		}
	}

	assert(0);
	return 0xffffffff;
}

uint32 CPowerOf2BlockPacker::FindFreeBlockIDOrCreateNew()
{
	std::vector<SBlockMinMax>::const_iterator it, end = m_Blocks.end();
	uint32 dwI = 0;

	for (it = m_Blocks.begin(); it != end; ++it, ++dwI)
	{
		const SBlockMinMax& ref = *it;

		if (ref.IsFree())
			return dwI;
	}

	m_Blocks.push_back(SBlockMinMax());

	return (uint32)m_Blocks.size() - 1;
}

void CPowerOf2BlockPacker::FreeContainers()
{
	Clear();
	stl::free_container(m_Blocks);
	stl::free_container(m_BlockBitmap);
}
