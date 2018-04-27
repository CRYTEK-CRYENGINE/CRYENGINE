// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#ifndef __POWEROF2BLOCKPACKER_H__
#define __POWEROF2BLOCKPACKER_H__

#include <vector>           // STL vector<>

class CPowerOf2BlockPacker
{
public:
	CTexture* m_pTexture;
	CTimeValue m_timeLastUsed;

public:
	// constructor
	// Arguments:
	//   dwLogHeight - e.g. specify 5 for 32, keep is small like ~ 5 or 6, don't use pixel size
	//   dwLogHeight - e.g. specify 5 for 32, keep is small like ~ 5 or 6, don't use pixel size
	CPowerOf2BlockPacker(const uint32 dwLogWidth, const uint32 dwLogHeight);

	~CPowerOf2BlockPacker();

	// Arguments:
	//   dwLogHeight - e.g. specify 5 for 32
	//   dwLogHeight - e.g. specify 5 for 32
	// Returns:
	//   dwBlockID (to remove later), 0xffffffff if there was no free space
	uint32 AddBlock(const uint32 dwLogWidth, const uint32 dwLogHeight);

	// Arguments:
	//   dwBlockID - as it was returned from AddBlock()
	uint32 GetBlockInfo(const uint32 dwBlockID, uint32& dwMinX, uint32& dwMinY, uint32& dwMaxX, uint32& dwMaxY);

	void   UpdateSize(int nW, int nH);

	// Arguments:
	//   dwBlockID - as it was returned from AddBlock()
	void RemoveBlock(const uint32 dwBlockID);

	// used for debugging
	// Return
	//   0xffffffff if not block was found
	uint32 GetRandomBlock() const;

	uint32 GetNumUsedBlocks() const
	{
		return m_nUsedBlocks;
	}

	void Clear();

	void FreeContainers();

private: // ----------------------------------------------------------

	struct SBlockMinMax
	{
		uint32 m_dwMinX;      // 0xffffffff if free, included
		uint32 m_dwMinY;      // not defined if free, included
		uint32 m_dwMaxX;      // not defined if free, not included
		uint32 m_dwMaxY;      // not defined if free, not included

		bool   IsFree() const
		{
			return m_dwMinX == 0xffffffff;
		}

		void MarkFree()
		{
			m_dwMinX = 0xffffffff;
		}

		~SBlockMinMax()
		{
			MarkFree();
		}
	};

	// -----------------------------------------------------------------

	std::vector<SBlockMinMax> m_Blocks;                 //
	std::vector<uint32>       m_BlockBitmap;            // [m_dwWidth*m_dwHeight], elements are 0xffffffff if not used
	uint32                    m_dwWidth;                // >0
	uint32                    m_dwHeight;               // >0
	uint32                    m_nUsedBlocks;

	// -----------------------------------------------------------------

	//
	void FillRect(const SBlockMinMax& rect, uint32 dwValue);

	bool IsFree(const SBlockMinMax& rect);

	//
	uint32 FindFreeBlockIDOrCreateNew();
};

#endif
