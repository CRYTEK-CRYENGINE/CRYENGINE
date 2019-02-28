// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "FontRenderer.h"
#include <freetype.h>
#include <ftoutln.h>
#include <ftglyph.h>
#include <ftimage.h>

//-------------------------------------------------------------------------------------------------
CFontRenderer::CFontRenderer()
	: m_pLibrary(0), m_pFace(0), m_pGlyph(0), m_fSizeRatio(0.8f), m_pEncoding(FONT_ENCODING_UNICODE),
	m_iGlyphBitmapWidth(0), m_iGlyphBitmapHeight(0)
{
}

//-------------------------------------------------------------------------------------------------
CFontRenderer::~CFontRenderer()
{
	FT_Done_Face(m_pFace);
	;
	FT_Done_FreeType(m_pLibrary);
	m_pFace = NULL;
	m_pLibrary = NULL;
}

//-------------------------------------------------------------------------------------------------
int CFontRenderer::LoadFromFile(const string& szFileName)
{
	int iError = FT_Init_FreeType(&m_pLibrary);

	if (iError)
	{
		return 0;
	}

	if (m_pFace)
	{
		FT_Done_Face(m_pFace);
		m_pFace = 0;
	}

	iError = FT_New_Face(m_pLibrary, szFileName.c_str(), 0, &m_pFace);

	if (iError)
	{
		return 0;
	}

	SetEncoding(FONT_ENCODING_UNICODE);

	return 1;
}

//-------------------------------------------------------------------------------------------------
int CFontRenderer::LoadFromMemory(unsigned char* pBuffer, int iBufferSize)
{
	int iError = FT_Init_FreeType(&m_pLibrary);

	if (iError)
	{
		return 0;
	}

	if (m_pFace)
	{
		FT_Done_Face(m_pFace);
		m_pFace = 0;
	}
	iError = FT_New_Memory_Face(m_pLibrary, pBuffer, iBufferSize, 0, &m_pFace);

	if (iError)
	{
		return 0;
	}

	SetEncoding(FONT_ENCODING_UNICODE);

	return 1;
}

//-------------------------------------------------------------------------------------------------
int CFontRenderer::Release()
{
	FT_Done_Face(m_pFace);
	;
	FT_Done_FreeType(m_pLibrary);
	m_pFace = NULL;
	m_pLibrary = NULL;

	return 1;
}

//-------------------------------------------------------------------------------------------------
int CFontRenderer::SetGlyphBitmapSize(int iWidth, int iHeight)
{
	m_iGlyphBitmapWidth = iWidth;
	m_iGlyphBitmapHeight = iHeight;

	FT_Set_Pixel_Sizes(m_pFace, (int)(m_iGlyphBitmapWidth * m_fSizeRatio), (int)(m_iGlyphBitmapHeight * m_fSizeRatio));

	return 1;
}

//-------------------------------------------------------------------------------------------------
int CFontRenderer::GetGlyphBitmapSize(int* pWidth, int* pHeight)
{
	if (pWidth)
	{
		*pWidth = m_iGlyphBitmapWidth;
	}

	if (pHeight)
	{
		*pHeight = m_iGlyphBitmapHeight;
	}

	return 1;
}

//-------------------------------------------------------------------------------------------------
int CFontRenderer::SetEncoding(FT_Encoding pEncoding)
{
	if (FT_Select_Charmap(m_pFace, pEncoding))
	{
		return 0;
	}

	return 1;
}

//-------------------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------------------
/*int CFontRenderer::CreateFontBitmap(CFBitmap **pFontBitmap, Vec3d *pTexCoord, int iWidth, int iHeight)
   {
   CFBitmap *pBitmap = new CFBitmap;

   if ((!pBitmap) || (!pBitmap->Create(iWidth, iHeight)))
   {
    if (pBitmap)
    {
      delete pBitmap;
    }

    return 0;
   }

   memset(pBitmap->GetData(), 0, pBitmap->GetWidth() * pBitmap->GetHeight());

   int iGlyphWidth = iWidth >> 4;
   int iGlyphHeight = iHeight >> 4;

   CFBitmap *pGlyph = new CFBitmap;

   if ((!pGlyph) || (!pGlyph->Create(iGlyphWidth, iGlyphHeight)))
   {
    pBitmap->Release();

    if (pGlyph)
    {
      pGlyph->Release();
    }

    return 0;
   }

   int iCharWidth;
   int iCharHeight;

   for (int i = 0; i < 16; i++)
   {
    for (int j = 0; j < 16; j++)
    {
      memset(pGlyph->GetData(), 0, pGlyph->GetHeight() * pGlyph->GetWidth());

      int iCharCode = i * 16 + j;

      if (!FT_GetChar(pGlyph, &iCharWidth, &iCharHeight, 1, 0, iGlyphWidth, iGlyphHeight, iCharCode))
      {
        continue;
      }

            pTexCoord[i * 16 + j].z = ((float)iCharWidth / (float)iGlyphWidth);
      pBitmap->BlitFrom(pGlyph, 0, 0, j * iGlyphWidth, i * iGlyphHeight, iGlyphWidth, iGlyphHeight);
    }
   }

   pGlyph->Release();

 * pFontBitmap = pBitmap;

   return 1;
   }

 */

//-------------------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------------------
int CFontRenderer::GetGlyph(CGlyphBitmap* pGlyphBitmap, uint8* iGlyphWidth, uint8* iGlyphHeight, char& iCharOffsetX, char& iCharOffsetY, int iX, int iY, int iCharCode)
{
	int iError = FT_Load_Char(m_pFace, iCharCode, FT_LOAD_DEFAULT);

	if (iError)
	{
		return 0;
	}

	m_pGlyph = m_pFace->glyph;

	iError = FT_Render_Glyph(m_pGlyph, FT_RENDER_MODE_NORMAL);

	if (iError)
	{
		return 0;
	}

	if (iGlyphWidth)
		*iGlyphWidth = m_pGlyph->bitmap.width;

	if (iGlyphHeight)
		*iGlyphHeight = m_pGlyph->bitmap.rows;

	int iTopOffset = (m_iGlyphBitmapHeight - (int)(m_iGlyphBitmapHeight * m_fSizeRatio)) + m_pGlyph->bitmap_top;

	iCharOffsetX = (char)m_pGlyph->bitmap_left;
	//	iCharOffsetY = (char)m_pGlyph->bitmap_top;
	iCharOffsetY = (char)((int)(m_iGlyphBitmapHeight * m_fSizeRatio) - m_pGlyph->bitmap_top);   // is that correct? - we need the baseline

	unsigned char* pBuffer = pGlyphBitmap->GetBuffer();
	uint32 dwGlyphWidth = pGlyphBitmap->GetWidth();

	assert(pBuffer);

	assert(iX + m_pGlyph->bitmap.width <= pGlyphBitmap->GetWidth());      // might happen if font characters are too big or cache dimenstions in font.xml is too small "<font path="VeraMono.ttf" w="320" h="368"/>"
	assert(iY + m_pGlyph->bitmap.rows <= pGlyphBitmap->GetHeight());

	for (int i = 0; i < m_pGlyph->bitmap.rows; i++)
	{
		int iNewY = i + iY;

		for (int j = 0; j < m_pGlyph->bitmap.width; j++)
		{
			unsigned char cColor = m_pGlyph->bitmap.buffer[(i * m_pGlyph->bitmap.width) + j];
			int iOffset = iNewY * dwGlyphWidth + iX + j;

			if (iOffset >= (int)dwGlyphWidth * m_iGlyphBitmapHeight)
				continue;

			pBuffer[iOffset] = cColor;
			//			pBuffer[iOffset] = cColor/2+32;		// debug - visualize character in a block
		}
	}

	return 1;
}

int CFontRenderer::GetGlyphScaled(CGlyphBitmap* pGlyphBitmap, int* iGlyphWidth, int* iGlyphHeight, int iX, int iY, float fScaleX, float fScaleY, int iCharCode)
{
	return 1;
}

//-------------------------------------------------------------------------------------------------
/*
   int CFontRenderer::FT_GetIndex(int iCharCode)
   {
   if (iCharCode < 256)
   {
    int iIndex = 0;
    int iUnicode;

    // try unicode
    for (int i = 0; i < m_pFace->num_charmaps; i++)
    {
      if ((m_pFace->charmaps[i]->platform_id == 3) && (m_pFace->charmaps[i]->encoding_id == 1))
      {
        iUnicode = i;

        FT_Set_Charmap(m_pFace, m_pFace->charmaps[i]);

        iIndex = FT_Get_Char_Index(m_pFace, iCharCode);

        // not unicode, try ascii
        if (iIndex == 0)
        {
          for (int i = 0; i < m_pFace->num_charmaps; i++)
          {
            if ((m_pFace->charmaps[i]->platform_id == 0) && (m_pFace->charmaps[i]->encoding_id == 0))
            {
              FT_Set_Charmap(m_pFace, m_pFace->charmaps[i]);

              iIndex = FT_Get_Char_Index(m_pFace, iCharCode);

              // not ascii either, reuse unicode default "missing char"
              if (iIndex == 0)
              {
                FT_Set_Charmap(m_pFace, m_pFace->charmaps[iUnicode]);

                return FT_Get_Char_Index(m_pFace, iCharCode);
              }
            }
          }
        }

        return  iIndex;
      }
    }

    return 0;
   }
   else
   {
    for (int i = 0; i < m_pFace->num_charmaps; i++)
    {
      if ((m_pFace->charmaps[i]->platform_id == 3) && (m_pFace->charmaps[i]->encoding_id == 1))
      {
        FT_Set_Charmap(m_pFace, m_pFace->charmaps[i]);

        return FT_Get_Char_Index(m_pFace, iCharCode);
      }
    }

    return 0;
   }

   return 0;
   }
 */
