// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

// -------------------------------------------------------------------------
//  File name:   BlockPacker.cpp
//  Created:     2012 by Vladimir Kajalin.
//  Description: 3D block atlas
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////

#include "StdAfx.h"

#if defined(FEATURE_SVO_GI)

	#include "BlockPacker.h"

const int nHS = 4;

CBlockPacker3D::CBlockPacker3D(const uint32 dwLogWidth, const uint32 dwLogHeight, const uint32 dwLogDepth, const bool bNonPow)
	: m_fLastUsed(0.0f)
	, m_nUsedBlocks(0)
{
	if (bNonPow)
	{
		m_dwWidth = dwLogWidth;
		m_dwHeight = dwLogHeight;
		m_dwDepth = dwLogDepth;
	}
	else
	{
		m_dwWidth = 1 << dwLogWidth;
		m_dwHeight = 1 << dwLogHeight;
		m_dwDepth = 1 << dwLogDepth;
	}

	m_BlockBitmap.resize(m_dwWidth * m_dwHeight * m_dwDepth, 0xffffffff);
	m_BlockUsageGrid.resize(m_dwWidth * m_dwHeight * m_dwDepth / nHS / nHS / nHS, 0);
	m_Blocks.reserve(m_dwWidth * m_dwHeight * m_dwDepth);
}

SBlockMinMax* CBlockPacker3D::GetBlockInfo(const uint32 dwBlockID)
{
	uint32 dwSize = (uint32)m_Blocks.size();

	assert(dwBlockID < dwSize);
	if (dwBlockID >= dwSize)
		return NULL;

	SBlockMinMax& ref = m_Blocks[dwBlockID];

	if (ref.IsFree())
		return NULL;

	return &m_Blocks[dwBlockID];
}

void CBlockPacker3D::UpdateSize(int nW, int nH, int nD)
{
	assert(m_nUsedBlocks == 0);

	m_dwWidth = nW;
	m_dwHeight = nH;
	m_dwDepth = nD;

	m_nUsedBlocks = 0;

	m_BlockBitmap.resize(m_dwWidth * m_dwHeight * m_dwDepth, 0xffffffff);
}

void CBlockPacker3D::RemoveBlock(const uint32 dwBlockID)
{
	uint32 dwSize = (uint32)m_Blocks.size();

	assert(dwBlockID < dwSize);
	if (dwBlockID >= dwSize)
		return;     // to avoid crash

	SBlockMinMax& ref = m_Blocks[dwBlockID];

	assert(!ref.IsFree());

	FillRect(ref, 0xffffffff);
	m_nUsedBlocks -= (ref.m_dwMaxX - ref.m_dwMinX) * (ref.m_dwMaxY - ref.m_dwMinY) * (ref.m_dwMaxZ - ref.m_dwMinZ);

	ref.MarkFree();

	//	m_BlocksList.remove(&ref);
}

void CBlockPacker3D::RemoveBlock(SBlockMinMax* pInfo)
{
	SBlockMinMax& ref = *pInfo;

	assert(!ref.IsFree());

	FillRect(ref, 0xffffffff);
	m_nUsedBlocks -= (ref.m_dwMaxX - ref.m_dwMinX) * (ref.m_dwMaxY - ref.m_dwMinY) * (ref.m_dwMaxZ - ref.m_dwMinZ);

	ref.MarkFree();

	//	m_BlocksList.remove(&ref);
}

SBlockMinMax* CBlockPacker3D::AddBlock(const uint32 dwLogWidth, const uint32 dwLogHeight, const uint32 dwLogDepth, void* pUserData, uint32 nCreateFrameId, uint32 nDataSize)
{
	if (!dwLogWidth || !dwLogHeight || !dwLogDepth)
		assert(!"Empty block");

	uint32 dwLocalWidth = dwLogWidth;
	uint32 dwLocalHeight = dwLogHeight;
	uint32 dwLocalDepth = dwLogDepth;

	int nCountNeeded = dwLocalWidth * dwLocalHeight * dwLocalDepth;

	int dwW = m_dwWidth / nHS;
	int dwH = m_dwHeight / nHS;
	int dwD = m_dwDepth / nHS;

	for (int nZ = 0; nZ < dwD; nZ++)
		for (int nY = 0; nY < dwH; nY++)
			for (int nX = 0; nX < dwW; nX++)
			{
				uint32 dwMinX = nX * nHS;
				uint32 dwMinY = nY * nHS;
				uint32 dwMinZ = nZ * nHS;

				uint32 dwMaxX = (nX + 1) * nHS;
				uint32 dwMaxY = (nY + 1) * nHS;
				uint32 dwMaxZ = (nZ + 1) * nHS;

				int nCountFree = nHS * nHS * nHS - m_BlockUsageGrid[nX + nY * dwW + nZ * dwW * dwH];

				if (nCountNeeded <= nCountFree)
				{
					SBlockMinMax testblock;
					testblock.m_pUserData = pUserData;
					testblock.m_nLastVisFrameId = nCreateFrameId;
					testblock.m_nDataSize = nDataSize;

					for (uint32 dwZ = dwMinZ; dwZ < dwMaxZ; dwZ += dwLocalDepth)
					{
						for (uint32 dwY = dwMinY; dwY < dwMaxY; dwY += dwLocalHeight)
						{
							for (uint32 dwX = dwMinX; dwX < dwMaxX; dwX += dwLocalWidth)
							{
								testblock.m_dwMinX = dwX;
								testblock.m_dwMaxX = dwX + dwLocalWidth;
								testblock.m_dwMinY = dwY;
								testblock.m_dwMaxY = dwY + dwLocalHeight;
								testblock.m_dwMinZ = dwZ;
								testblock.m_dwMaxZ = dwZ + dwLocalDepth;

								if (IsFree(testblock))
								{
									uint32 dwBlockID = FindFreeBlockIDOrCreateNew();

									m_Blocks[dwBlockID] = testblock;

									FillRect(testblock, dwBlockID);

									m_nUsedBlocks += dwLocalWidth * dwLocalHeight * dwLocalDepth;

									//							m_BlocksList.insertBeginning(&m_Blocks[dwBlockID]);

									return &m_Blocks[dwBlockID];
								}
							}
						}
					}
				}
			}

	return NULL;  // no space left to this block
}

void CBlockPacker3D::UpdateUsageGrid(const SBlockMinMax& rectIn)
{
	SBlockMinMax rectUM;

	rectUM.m_dwMinX = rectIn.m_dwMinX / nHS;
	rectUM.m_dwMinY = rectIn.m_dwMinY / nHS;
	rectUM.m_dwMinZ = rectIn.m_dwMinZ / nHS;

	rectUM.m_dwMaxX = (rectIn.m_dwMaxX - 1) / nHS + 1;
	rectUM.m_dwMaxY = (rectIn.m_dwMaxY - 1) / nHS + 1;
	rectUM.m_dwMaxZ = (rectIn.m_dwMaxZ - 1) / nHS + 1;

	int dwW = m_dwWidth / nHS;
	int dwH = m_dwHeight / nHS;
	int dwD = m_dwDepth / nHS;

	for (uint32 dwZ = rectUM.m_dwMinZ; dwZ < rectUM.m_dwMaxZ; ++dwZ)
	{
		for (uint32 dwY = rectUM.m_dwMinY; dwY < rectUM.m_dwMaxY; ++dwY)
		{
			for (uint32 dwX = rectUM.m_dwMinX; dwX < rectUM.m_dwMaxX; ++dwX)
			{
				SBlockMinMax rectTest = rectUM;

				rectTest.m_dwMinX = dwX * nHS;
				rectTest.m_dwMinY = dwY * nHS;
				rectTest.m_dwMinZ = dwZ * nHS;

				rectTest.m_dwMaxX = (dwX + 1) * nHS;
				rectTest.m_dwMaxY = (dwY + 1) * nHS;
				rectTest.m_dwMaxZ = (dwZ + 1) * nHS;

				m_BlockUsageGrid[dwX + dwY * dwW + dwZ * dwW * dwH] = GetUsedSlotsCount(rectTest);
			}
		}
	}
}

void CBlockPacker3D::FillRect(const SBlockMinMax& rect, uint32 dwValue)
{
	for (uint32 dwZ = rect.m_dwMinZ; dwZ < rect.m_dwMaxZ; ++dwZ)
		for (uint32 dwY = rect.m_dwMinY; dwY < rect.m_dwMaxY; ++dwY)
			for (uint32 dwX = rect.m_dwMinX; dwX < rect.m_dwMaxX; ++dwX)
				m_BlockBitmap[dwX + dwY * m_dwWidth + dwZ * m_dwWidth * m_dwHeight] = dwValue;

	UpdateUsageGrid(rect);
}

int CBlockPacker3D::GetUsedSlotsCount(const SBlockMinMax& rect)
{
	int nCount = 0;

	for (uint32 dwZ = rect.m_dwMinZ; dwZ < rect.m_dwMaxZ; ++dwZ)
		for (uint32 dwY = rect.m_dwMinY; dwY < rect.m_dwMaxY; ++dwY)
			for (uint32 dwX = rect.m_dwMinX; dwX < rect.m_dwMaxX; ++dwX)
				if (m_BlockBitmap[dwX + dwY * m_dwWidth + dwZ * m_dwWidth * m_dwHeight] != 0xffffffff)
					nCount++;

	return nCount;
}

bool CBlockPacker3D::IsFree(const SBlockMinMax& rect)
{
	for (uint32 dwZ = rect.m_dwMinZ; dwZ < rect.m_dwMaxZ; ++dwZ)
		for (uint32 dwY = rect.m_dwMinY; dwY < rect.m_dwMaxY; ++dwY)
			for (uint32 dwX = rect.m_dwMinX; dwX < rect.m_dwMaxX; ++dwX)
				if (m_BlockBitmap[dwX + dwY * m_dwWidth + dwZ * m_dwWidth * m_dwHeight] != 0xffffffff)
					return false;

	return true;
}

uint32 CBlockPacker3D::FindFreeBlockIDOrCreateNew()
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

#endif
