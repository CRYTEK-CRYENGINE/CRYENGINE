// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include <vector>
#include "GlyphBitmap.h"
#include "FontRenderer.h"
#include <CryCore/StlUtils.h>

typedef struct CCacheSlot
{
	unsigned int dwUsage;
	int          iCacheSlot;
	uint32       cCurrentChar;

	uint8        iCharWidth;          // size in pixel
	uint8        iCharHeight;         // size in pixel
	char         iCharOffsetX;
	char         iCharOffsetY;

	CGlyphBitmap pGlyphBitmap;

	void Reset()
	{
		dwUsage = 0;
		cCurrentChar = ~0;

		iCharWidth = 0;
		iCharHeight = 0;
		iCharOffsetX = 0;
		iCharOffsetY = 0;

		pGlyphBitmap.Clear();
	}

	void GetMemoryUsage(ICrySizer* pSizer) const
	{
		pSizer->AddObject(this, sizeof(*this));
		pSizer->AddObject(pGlyphBitmap);
	}

} CCacheSlot;

typedef std::unordered_map<uint32, CCacheSlot*, stl::hash_uint32> CCacheTable;

typedef std::vector<CCacheSlot*>                                  CCacheSlotList;
typedef std::vector<CCacheSlot*>::iterator                        CCacheSlotListItor;

#if CRY_PLATFORM_WINDOWS && CRY_PLATFORM_64BIT
	#undef GetCharWidth
	#undef GetCharHeight
#endif

class CGlyphCache
{
public:
	CGlyphCache();
	~CGlyphCache();

	int         Create(int iCacheSize, int iGlyphBitmapWidth, int iGlyphBitmapHeight, int iSmoothMethod, int iSmoothAmount, float fSizeRatio = 0.8f);
	int         Release();

	int         LoadFontFromFile(const string& szFileName);
	int         LoadFontFromMemory(unsigned char* pFileBuffer, int iDataSize);
	int         ReleaseFont();

	int         SetEncoding(FT_Encoding pEncoding) { return m_pFontRenderer.SetEncoding(pEncoding); };
	FT_Encoding GetEncoding()                      { return m_pFontRenderer.GetEncoding(); };

	int         GetGlyphBitmapSize(int* pWidth, int* pHeight);

	int         PreCacheGlyph(uint32 cChar);
	int         UnCacheGlyph(uint32 cChar);
	int         GlyphCached(uint32 cChar);

	CCacheSlot* GetLRUSlot();
	CCacheSlot* GetMRUSlot();

	int         GetGlyph(CGlyphBitmap** pGlyph, int* piWidth, int* piHeight, char& iCharOffsetX, char& iCharOffsetY, uint32 cChar);

	void        GetMemoryUsage(ICrySizer* pSizer) const
	{
		pSizer->AddObject(m_pSlotList);
		//pSizer->AddContainer(m_pCacheTable);
		pSizer->AddObject(m_pScaleBitmap);
		pSizer->AddObject(m_pFontRenderer);
	}

private:

	int CreateSlotList(int iListSize);
	int ReleaseSlotList();

	CCacheSlotList m_pSlotList;
	CCacheTable    m_pCacheTable;

	int            m_iGlyphBitmapWidth;
	int            m_iGlyphBitmapHeight;
	float          m_fSizeRatio;

	int            m_iSmoothMethod;
	int            m_iSmoothAmount;

	CGlyphBitmap*  m_pScaleBitmap;

	CFontRenderer  m_pFontRenderer;

	unsigned int   m_dwUsage;
};
