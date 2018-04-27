// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "Image.h"

#include "XmlArchive.h"

//////////////////////////////////////////////////////////////////////////
bool CImageEx::LoadGrayscale16Tiff(const string& fileName)
{
	return false;
}

//////////////////////////////////////////////////////////////////////////
bool CImageEx::SaveGrayscale16Tiff(const string& fileName)
{
	/*
	   TIFF * tif;
	   int Width, Height,Bpp;
	   tdata_t buf;

	   tif = TIFFOpen(fileName, "rb");
	   if (tif == 0)
	   {
	    MessageBox( NULL,"Not a Tiff file","Warning",MB_OK|MB_ICONEXCLAMATION );
	    return false;
	   }

	   TIFFGetField(tif, TIFFTAG_IMAGEWIDTH, &Width);
	   TIFFGetField(tif, TIFFTAG_IMAGELENGTH, &Height);
	   TIFFGetField(tif, TIFFTAG_BITSPERSAMPLE, &Bpp);

	   int bytes = Bpp/8;
	   int mask = Bpp-1;

	   Allocate(Width, Height);
	 */
	/*
	   buf = _TIFFmalloc(TIFFScanlineSize(tif));
	   for (int row = 0; row < Height; row++)
	   {
	    TIFFReadScanline(tif, buf, row);
	    unsigned char *pBuf = (unsigned char*)buf;
	    for (int x = 0; x < Width*bytes; x += bytes)
	    {
	      ValueAt( x,row ) = (*(uint32*)buf) & mask;
	      pBuf += bytes;
	    }
	   }
	   _TIFFfree(buf);
	 */

	//TIFFClose(tif);
	return true;
}

void CImageEx::SwapRedAndBlue()
{
	if (!IsValid())
		return;
	// Set the loop pointers
	uint32* pPixData = GetData();
	uint32* pPixDataEnd = pPixData + GetWidth() * GetHeight();
	// Switch R and B
	while (pPixData != pPixDataEnd)
	{
		// Extract the bits, shift them, put them back and advance to the next pixel
		*pPixData++ = (*pPixData & 0xFF000000) | ((*pPixData & 0x00FF0000) >> 16) | (*pPixData & 0x0000FF00) | ((*pPixData & 0x000000FF) << 16);
	}
}

void CImageEx::ReverseUpDown()
{
	if (!IsValid())
		return;

	uint32* pPixData = GetData();
	uint32* pReversePix = new uint32[GetWidth() * GetHeight()];

	for (int i = GetHeight() - 1, i2 = 0; i >= 0; i--, i2++)
	{
		for (int k = 0; k < GetWidth(); k++)
		{
			pReversePix[i2 * GetWidth() + k] = pPixData[i * GetWidth() + k];
		}
	}

	Attach(pReversePix, GetWidth(), GetHeight());
}

void CImageEx::FillAlpha(unsigned char value)
{
	if (!IsValid())
		return;
	// Set the loop pointers
	uint32* pPixData = GetData();
	uint32* pPixDataEnd = pPixData + GetWidth() * GetHeight();
	while (pPixData != pPixDataEnd)
	{
		*pPixData++ = (*pPixData & 0x00FFFFFF) | (value << 24);
	}
}

QImage CImageEx::ToQImage() const
{
	return QImage((const uchar*)GetData(), GetWidth(), GetHeight(), QImage::Format_ARGB32);
}

