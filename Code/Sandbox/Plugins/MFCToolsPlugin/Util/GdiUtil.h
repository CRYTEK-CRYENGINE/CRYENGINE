// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once
////////////////////////////////////////////////////////////////////////////
//
//  CryEngine Source File.
//  Copyright (C), Crytek, 1999-2010.
// -------------------------------------------------------------------------
//  File name:	GdiUtil.h
//  Version:	v1.00
//  Created:	12/07/2010 by Nicusor Nedelcu
//  Description:	Utilitarian classes for double buffer GDI rendering and 32bit bitmaps
//
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////

const COLORREF kDrawBoxSameColorAsFill = 0xFFFFFFFF;

//! This is a class that manages a doublebuffer in GDI, so you can have flicker-free drawing
class PLUGIN_API CGdiCanvas
{
public:

	CGdiCanvas();
	~CGdiCanvas();

	//! create the canvas from a compatible DC
	bool Create(HDC hCompatibleDC, UINT aWidth, UINT aHeight);
	//! resize the canvas, without destroying DCs
	bool Resize(HDC hCompatibleDC, UINT aWidth, UINT aHeight);
	//! clear the canvas with a color
	void Clear(COLORREF aClearColor = RGB(255, 255, 255));
	//! \return canvas width
	UINT GetWidth();
	//! \return canvas height
	UINT GetHeight();
	//! blit/copy the image from the canvas to other destination DC
	bool BlitTo(HDC hDestDC, int aDestX = 0, int aDestY = 0, int aDestWidth = -1, int aDestHeight = -1, int aSrcX = 0, int aSrcY = 0, int aRop = SRCCOPY);
	//! blit/copy the image from the canvas to other destination window
	bool BlitTo(HWND hDestWnd, int aDestX = 0, int aDestY = 0, int aDestWidth = -1, int aDestHeight = -1, int aSrcX = 0, int aSrcY = 0, int aRop = SRCCOPY);
	//! free the canvas data
	void Free();
	//! \return the canvas' DC
	CDC& GetDC();
	//! fill a rectangle with a gradient fill, using two colors, start and end
	//! \param rRect the rectangle to be filled
	//! \param aStartColor the start color of the gradient
	//! \param aEndColor the end color of the gradient
	//! \param aFillType the mode to fill the rectangle: GRADIENT_FILL_RECT_H - horizontal fill, GRADIENT_FILL_RECT_V - vertical fill
	bool GradientFillRect(CRect& rRect, COLORREF aStartColor, COLORREF aEndColor, int aFillType = GRADIENT_FILL_RECT_H);
	//! this will blit/copy a bitmap with alpha channel, using the given blend mode
	//! \param rBmp the bitmap to blit
	//! \param aDestX destination X on the canvas
	//! \param aDestY destination Y on the canvas
	//! \param aDestWidth destination width on the canvas, if -1 then the bitmap width will be used
	//! \param aDestHeight destination height on the canvas, if -1 then the bitmap height will be used
	//! \param pBlendFunc the blending function, if NULL then the normal alpha blend will be used
	bool BitBltWithAlpha(CDC& rBmpDC, int aDestX, int aDestY, int aDestWidth = -1, int aDestHeight = -1, int aSrcX = 0, int aSrcY = 0, int aSrcWidth = -1, int aSrcHeight = -1, BLENDFUNCTION* pBlendFunc = NULL);
	//! this function breaks a text enclosed in a rectangle, on a character basis, because GDI is not capable of such feature, only on a word-break basis
	//! it will insert \r chars in the text
	//! \remark it will use the current font selected in the canvas' DC
	void BreakTextByChars(const char* pText, string& rOutStr, const CRect& rRect);
	//! function used to compute thumbs per row and spacing, used in asset browser and other tools where thumb layout is needed and maybe GDI canvas used
	//! \param aContainerWidth the thumbs' container width
	//! \param aThumbWidth the thumb image width
	//! \param aMargin the thumb default minimum horizontal margin
	//! \param aThumbCount the thumb count
	//! \param rThumbsPerRow returned thumb count per single row
	//! \param rNewMargin returned new computed margin between thumbs
	//! \note The margin between thumbs will grow/shrink dynamically to keep up with the thumb count per row
	static bool ComputeThumbsLayoutInfo(float aContainerWidth, float aThumbWidth, float aMargin, UINT aThumbCount, UINT& rThumbsPerRow, float& rNewMargin);
	//! this function will setup a blendfunc struct with a specified alpha value
	static void MakeBlendFunc(BYTE aAlpha, /*out*/ BLENDFUNCTION& rBlendFunc);
	//! draw a full-featured box with many options
	//TODO: make a struct as args
	void DrawBox(int aLeft, int aTop, int aWidth, int aHeight, COLORREF aFillColor1, COLORREF aFillColor2 = kDrawBoxSameColorAsFill,
	             bool bDrawBorder = true, bool b3DLitBorder = true, bool b1PixelRoundCorner = true, float aBorderLightPower = 1.5f,
	             float aBorderShadowPower = 0.5f, COLORREF aCustomBorderColor = kDrawBoxSameColorAsFill);
	static COLORREF ScaleColor(COLORREF aColor, float aScale);

protected:

	HDC     m_hCompatibleDC;
	CDC     m_memDC;
	CBitmap m_memBmp;
	UINT    m_width, m_height;
};

//! This class loads alpha-channel bitmaps and holds a DC for use with AlphaBlend function
class PLUGIN_API CAlphaBitmap
{
public:

	CAlphaBitmap();
	~CAlphaBitmap();

	//! load a bitmap file from a PNG or other alpha-capable format
	bool Load(const char* pFilename, bool bVerticalFlip = false);
	//! save a bitmap to a file
	bool Save(const char* pFileName);
	//! creates the bitmap from raw 32bpp data
	//! \param pData the 32bpp raw image data, RGBA, can be NULL and it would create just an empty bitmap
	//! \param aWidth the bitmap width
	//! \param aHeight the bitmap height
	bool     Create(void* pData, UINT aWidth, UINT aHeight, bool bVerticalFlip = false, bool bPremultiplyAlpha = false);
	//! \return the actual bitmap
	CBitmap& GetBitmap();
	//! \return the DC used to blit the bitmap onto other DCs
	CDC&     GetDC();
	//! free the bitmap and DC
	void     Free();
	//! \return bitmap width
	UINT     GetWidth();
	//! \return bitmap height
	UINT     GetHeight();

private:
	int  GetEncoderClsid(const char* format, CLSID* pClsid) const;
	bool GetCLSIDObjFromFileExt(const char* fileName, CLSID* pClsid) const;

protected:

	CBitmap m_bmp;
	CDC     m_dcForBlitting;
	HBITMAP m_hOldBmp;
	UINT    m_width, m_height;
};

//! fill a rectangle with a gradient fill, using two colors, start and end
//! \param hDC the HDC to draw in
//! \param rRect the rectangle to be filled
//! \param aStartColor the start color of the gradient
//! \param aEndColor the end color of the gradient
//! \param aFillType the mode to fill the rectangle: GRADIENT_FILL_RECT_H - horizontal fill, GRADIENT_FILL_RECT_V - vertical fill
bool GradientFillRect(HDC hDC, CRect& rRect, COLORREF aStartColor, COLORREF aEndColor, int aFillType);

