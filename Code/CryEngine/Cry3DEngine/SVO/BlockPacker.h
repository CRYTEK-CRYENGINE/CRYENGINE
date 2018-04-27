// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#if defined(FEATURE_SVO_GI)

	#include <vector>         // STL vector<>

struct SBlockMinMax
{
	SBlockMinMax()
	{
		ZeroStruct(*this);
		MarkFree();
	}

	uint8         m_dwMinX; // 0xffffffff if free, included
	uint8         m_dwMinY; // not defined if free, included
	uint8         m_dwMinZ; // not defined if free, included
	uint8         m_dwMaxX; // not defined if free, not included
	uint8         m_dwMaxY; // not defined if free, not included
	uint8         m_dwMaxZ; // not defined if free, not included
	uint16        m_nDataSize;
	void*         m_pUserData;
	uint32        m_nLastVisFrameId;

	SBlockMinMax* m_pNext, * m_pPrev;

	bool          IsFree() const
	{
		return m_dwMinX == 0xff;
	}

	void MarkFree()
	{
		m_dwMinX = 0xff;
		m_pUserData = 0;
		m_nDataSize = m_nLastVisFrameId = 0;
	}
};
      
class CBlockPacker3D
{
public:
	float m_fLastUsed;

public:
	// constructor
	// Arguments:
	//   dwLogHeight - e.g. specify 5 for 32, keep is small like ~ 5 or 6, don't use pixel size
	//   dwLogHeight - e.g. specify 5 for 32, keep is small like ~ 5 or 6, don't use pixel size
	CBlockPacker3D(const uint32 dwLogWidth, const uint32 dwLogHeight, const uint32 dwLogDepth, const bool bNonPow = false);

	// Arguments:
	//   dwLogHeight - e.g. specify 5 for 32
	//   dwLogHeight - e.g. specify 5 for 32
	// Returns:
	//   block * or 0 if there was no free space
	SBlockMinMax* AddBlock(const uint32 dwLogWidth, const uint32 dwLogHeight, const uint32 dwLogDepth, void* pUserData, uint32 nCreateFrameId, uint32 nDataSize);

	// Arguments:
	//   dwBlockID - as it was returned from AddBlock()
	SBlockMinMax* GetBlockInfo(const uint32 dwBlockID);

	void          UpdateSize(int nW, int nH, int nD);

	// Arguments:
	//   dwBlockID - as it was returned from AddBlock()
	void   RemoveBlock(const uint32 dwBlockID);
	void   RemoveBlock(SBlockMinMax* pInfo);

	uint32 GetNumSubBlocks() const
	{
		return m_nUsedBlocks;
	}

	uint32 GetNumBlocks() const
	{
		return m_Blocks.size();
	}

	//	void MarkAsInUse(SBlockMinMax * pInfo);

	//	SBlockMinMax * CBlockPacker3D::GetLruBlock();

private: // ----------------------------------------------------------

	// -----------------------------------------------------------------

	//	TDoublyLinkedList<SBlockMinMax> m_BlocksList;
	std::vector<SBlockMinMax> m_Blocks;                 //
	std::vector<uint32>       m_BlockBitmap;            // [m_dwWidth*m_dwHeight*m_dwDepth], elements are 0xffffffff if not used
	std::vector<byte>         m_BlockUsageGrid;
	uint32                    m_dwWidth;                // >0
	uint32                    m_dwHeight;               // >0
	uint32                    m_dwDepth;                // >0
	uint32                    m_nUsedBlocks;
	// -----------------------------------------------------------------

	//
	void FillRect(const SBlockMinMax& rect, uint32 dwValue);

	bool IsFree(const SBlockMinMax& rect);

	int  GetUsedSlotsCount(const SBlockMinMax& rect);

	void UpdateUsageGrid(const SBlockMinMax& rectIn);

	//
	uint32 FindFreeBlockIDOrCreateNew();
};

#endif
