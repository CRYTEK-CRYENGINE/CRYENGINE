// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "GdiUtil.h"
#include "Util/Image.h"
#include <CryString/StringUtils.h>
#include "Controls/QuestionDialog.h"

CGdiCanvas::CGdiCanvas()
{
	m_hCompatibleDC = 0;
	m_width = m_height = 0;
}

CGdiCanvas::~CGdiCanvas()
{
	Free();
}

bool CGdiCanvas::Create(HDC hCompatibleDC, UINT aWidth, UINT aHeight)
{
	if (!hCompatibleDC)
		return false;

	if (!aWidth || !aHeight)
		return false;

	Free();

	m_hCompatibleDC = hCompatibleDC;
	CDC cdc;
	cdc.Attach(hCompatibleDC);

	try
	{
		if (!m_memBmp.CreateCompatibleBitmap(&cdc, aWidth, aHeight))
		{
			throw false;
		}

		if (!m_memDC.CreateCompatibleDC(&cdc))
		{
			throw false;
		}

		m_memDC.SelectObject(m_memBmp);
		cdc.Detach();
		m_memDC.SetBkMode(TRANSPARENT);
		m_memDC.SetStretchBltMode(HALFTONE);
	}
	catch (bool)
	{
		cdc.Detach();
		return false;
	}

	m_width = aWidth;
	m_height = aHeight;

	return true;
}

bool CGdiCanvas::Resize(HDC hCompatibleDC, UINT aWidth, UINT aHeight)
{
	if (m_memBmp.GetSafeHandle())
	{
		m_memBmp.DeleteObject();
	}

	CDC dc;

	m_hCompatibleDC = hCompatibleDC;
	dc.Attach(m_hCompatibleDC);

	if (!m_memBmp.CreateCompatibleBitmap(&dc, aWidth, aHeight))
	{
		dc.Detach();
		return false;
	}

	dc.Detach();
	m_memDC.SelectObject(m_memBmp);
	m_width = aWidth;
	m_height = aHeight;

	return true;
}

void CGdiCanvas::Clear(COLORREF aClearColor)
{
	m_memDC.FillSolidRect(0, 0, m_width, m_height, aClearColor);
}

UINT CGdiCanvas::GetWidth()
{
	return m_width;
}

UINT CGdiCanvas::GetHeight()
{
	return m_height;
}

bool CGdiCanvas::BlitTo(HDC hDestDC, int aDestX, int aDestY, int aDestWidth, int aDestHeight, int aSrcX, int aSrcY, int aRop)
{
	CDC dc;
	BITMAP bmpInfo;

	dc.Attach(hDestDC);

	if (!m_memBmp.GetBitmap(&bmpInfo))
	{
		dc.Detach();
		return false;
	}

	aDestWidth = aDestWidth == -1 ? bmpInfo.bmWidth : aDestWidth;
	aDestHeight = aDestHeight == -1 ? bmpInfo.bmHeight : aDestHeight;
	dc.BitBlt(aDestX, aDestY, aDestWidth, aDestHeight, &m_memDC, aSrcX, aSrcY, aRop);
	dc.Detach();

	return true;
}

bool CGdiCanvas::BlitTo(HWND hDestWnd, int aDestX, int aDestY, int aDestWidth, int aDestHeight, int aSrcX, int aSrcY, int aRop)
{
	HDC hDC = ::GetDC(hDestWnd);
	bool bRet = BlitTo(hDC, aDestX, aDestY, aDestWidth, aDestHeight, aSrcX, aSrcY, aRop);
	ReleaseDC(hDestWnd, hDC);

	return bRet;
}

void CGdiCanvas::Free()
{
	if (m_memBmp.GetSafeHandle())
		m_memBmp.DeleteObject();

	if (m_memDC.GetSafeHdc())
		m_memDC.DeleteDC();

	m_width = 0;
	m_height = 0;
	m_hCompatibleDC = 0;
}

CDC& CGdiCanvas::GetDC()
{
	return m_memDC;
}

bool CGdiCanvas::GradientFillRect(CRect& rRect, COLORREF aStartColor, COLORREF aEndColor, int aFillType)
{
	return ::GradientFillRect(m_memDC.GetSafeHdc(), rRect, aStartColor, aEndColor, aFillType);
}

bool CGdiCanvas::BitBltWithAlpha(CDC& rBmpDC, int aDestX, int aDestY, int aDestWidth, int aDestHeight, int aSrcX, int aSrcY, int aSrcWidth, int aSrcHeight, BLENDFUNCTION* pBlendFunc)
{
	BLENDFUNCTION bf;

	if (!pBlendFunc)
	{
		bf.BlendOp = AC_SRC_OVER;
		bf.BlendFlags = 0;
		bf.SourceConstantAlpha = 0xFF;
		bf.AlphaFormat = AC_SRC_ALPHA;
	}
	else
	{
		bf = *pBlendFunc;
	}

	CBitmap* pBmp = rBmpDC.GetCurrentBitmap();

	assert(pBmp);

	if (!pBmp)
		return false;

	BITMAP bmpInfo;

	if (!pBmp->GetBitmap(&bmpInfo))
		return false;

	aDestWidth = aDestWidth == -1 ? bmpInfo.bmWidth : aDestWidth;
	aDestHeight = aDestHeight == -1 ? bmpInfo.bmHeight : aDestHeight;
	aSrcWidth = aSrcWidth == -1 ? bmpInfo.bmWidth : aSrcWidth;
	aSrcHeight = aSrcHeight == -1 ? bmpInfo.bmHeight : aSrcHeight;

	return TRUE == AlphaBlend(m_memDC.GetSafeHdc(), aDestX, aDestY, aDestWidth, aDestHeight, rBmpDC.GetSafeHdc(), aSrcX, aSrcY, aSrcWidth, aSrcHeight, bf);
}

void CGdiCanvas::BreakTextByChars(const char* pText, string& rOutStr, const CRect& rRect)
{
	UINT i = 0, nSize = strlen(pText);
	CString chunk;
	CSize textSize;
	int rectWidth = rRect.Width();

	rOutStr = "";

	while (i < nSize)
	{
		chunk += pText[i];
		textSize = m_memDC.GetTextExtent(chunk);

		// if current chunk of text os bigger than the rect width, cut
		if (textSize.cx > rectWidth)
		{
			// special case, rect width smaller than the first char in the text
			if (!i)
			{
				rOutStr = pText;
				return;
			}

			rOutStr += chunk.Mid(0, chunk.GetLength() - 1);
			rOutStr += string("\r");
			chunk = "";
			continue;
		}

		++i;
	}

	rOutStr += chunk;
}

bool CGdiCanvas::ComputeThumbsLayoutInfo(float aContainerWidth, float aThumbWidth, float aMargin, UINT aThumbCount, UINT& rThumbsPerRow, float& rNewMargin)
{
	rThumbsPerRow = 0;
	rNewMargin = 0;

	if (aThumbWidth <= 0 || aMargin <= 0 || (aThumbWidth + aMargin * 2) <= 0)
	{
		return false;
	}

	if (aContainerWidth <= 0)
	{
		return true;
	}

	rThumbsPerRow = (int) aContainerWidth / (aThumbWidth + aMargin * 2);

	if ((aThumbWidth + aMargin * 2) * aThumbCount < aContainerWidth)
	{
		rNewMargin = aMargin;
	}
	else
	{
		if (rThumbsPerRow > 0)
		{
			rNewMargin = (aContainerWidth - rThumbsPerRow * aThumbWidth);

			if (rNewMargin > 0)
			{
				rNewMargin = (float)rNewMargin / rThumbsPerRow / 2.0f;
			}
		}
	}

	return true;
}

void CGdiCanvas::MakeBlendFunc(BYTE aAlpha, BLENDFUNCTION& rBlendFunc)
{
	rBlendFunc.BlendOp = AC_SRC_OVER;
	rBlendFunc.BlendFlags = 0;
	rBlendFunc.SourceConstantAlpha = aAlpha;
	rBlendFunc.AlphaFormat = AC_SRC_ALPHA;
}

COLORREF CGdiCanvas::ScaleColor(COLORREF aColor, float aScale)
{
	if (!aColor)
	{
		// help out scaling, by starting at very low black
		aColor = RGB(1, 1, 1);
	}

	int r = GetRValue(aColor);
	int g = GetGValue(aColor);
	int b = GetBValue(aColor);

	r *= aScale;
	g *= aScale;
	b *= aScale;

	return RGB(CLAMP(r, 0, 255), CLAMP(g, 0, 255), CLAMP(b, 0, 255));
}

void CGdiCanvas::DrawBox(int aLeft, int aTop, int aWidth, int aHeight, COLORREF aFillColor1, COLORREF aFillColor2,
                         bool bDrawBorder, bool b3DLitBorder, bool b1PixelRoundCorner, float aBorderLightPower,
                         float aBorderShadowPower, COLORREF aCustomBorderColor)
{
	CRect rc;

	if (aFillColor2 == kDrawBoxSameColorAsFill)
		aFillColor2 = aFillColor1;

	if (aCustomBorderColor == kDrawBoxSameColorAsFill)
		aCustomBorderColor = aFillColor1;

	COLORREF lightBorderColor = ScaleColor(aCustomBorderColor, aBorderLightPower);
	COLORREF shadowBorderColor = ScaleColor(aCustomBorderColor, aBorderShadowPower);

	rc.SetRect(aLeft + 1, aTop + 1, aLeft + aWidth - 2, aTop + aHeight - 2);
	GradientFillRect(rc, aFillColor1, aFillColor2, GRADIENT_FILL_RECT_V);

	if (!b1PixelRoundCorner)
	{
		if (b3DLitBorder)
		{
			m_memDC.Draw3dRect(aLeft, aTop, aWidth, aHeight, lightBorderColor, shadowBorderColor);
		}
		else
		{
			m_memDC.Draw3dRect(aLeft, aTop, aWidth, aHeight, lightBorderColor, lightBorderColor);
		}
	}
	else
	{
		CPen penLight, penShadow;

		penLight.CreatePen(PS_SOLID, 1, lightBorderColor);
		penShadow.CreatePen(PS_SOLID, 1, shadowBorderColor);

		m_memDC.SelectObject(penLight);
		// top
		m_memDC.MoveTo(aLeft + 1, aTop);
		m_memDC.LineTo(aLeft + aWidth - 2, aTop);
		// left
		m_memDC.MoveTo(aLeft, aTop + 1);
		m_memDC.LineTo(aLeft, aTop + aHeight - 2);

		m_memDC.SelectObject((b3DLitBorder) ? penShadow : penLight);
		// bottom
		m_memDC.MoveTo(aLeft + aWidth - 3, aTop + aHeight - 2);
		m_memDC.LineTo(aLeft, aTop + aHeight - 2);
		// right
		m_memDC.MoveTo(aLeft + aWidth - 2, aTop + 1);
		m_memDC.LineTo(aLeft + aWidth - 2, aTop + aHeight - 2);

		penLight.DeleteObject();
		penShadow.DeleteObject();
	}
}

//---

CAlphaBitmap::CAlphaBitmap()
{
	m_width = m_height = 0;
	m_hOldBmp = NULL;
}

CAlphaBitmap::~CAlphaBitmap()
{
	Free();
}

bool CAlphaBitmap::Load(const char* pFilename, bool bVerticalFlip)
{
	Free();
	WCHAR wzFilename[MAX_PATH];
	Unicode::Convert(wzFilename, pFilename);
	Gdiplus::Bitmap img(wzFilename);
	Gdiplus::BitmapData bmData;

	if (0 != img.GetLastStatus())
		return false;

	HDC hDC = ::GetDC(GetDesktopWindow());
	CDC dc;

	dc.Attach(hDC);

	if (!m_dcForBlitting.CreateCompatibleDC(&dc))
	{
		dc.Detach();
		ReleaseDC(GetDesktopWindow(), hDC);
		return false;
	}

	if (bVerticalFlip)
	{
		img.RotateFlip(Gdiplus::RotateNoneFlipY);
	}

	img.LockBits(NULL, Gdiplus::ImageLockModeRead, PixelFormat32bppARGB, &bmData);

	if (!bmData.Scan0)
	{
		dc.Detach();
		ReleaseDC(GetDesktopWindow(), hDC);
		return false;
	}

	// premultiply alpha, AlphaBlend GDI expects it
	for (UINT y = 0; y < bmData.Height; ++y)
	{
		BYTE* pPixel = (BYTE*) bmData.Scan0 + bmData.Width * 4 * y;

		for (UINT x = 0; x < bmData.Width; ++x)
		{
			pPixel[0] = ((int)pPixel[0] * pPixel[3] + 127) >> 8;
			pPixel[1] = ((int)pPixel[1] * pPixel[3] + 127) >> 8;
			pPixel[2] = ((int)pPixel[2] * pPixel[3] + 127) >> 8;
			pPixel += 4;
		}
	}

	BITMAPINFO bmi;

	ZeroMemory(&bmi, sizeof(BITMAPINFO));
	bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
	bmi.bmiHeader.biWidth = bmData.Width;
	bmi.bmiHeader.biHeight = bmData.Height;
	bmi.bmiHeader.biPlanes = 1;
	bmi.bmiHeader.biBitCount = 32;
	bmi.bmiHeader.biCompression = BI_RGB;
	bmi.bmiHeader.biSizeImage = bmData.Width * bmData.Height * 4;
	void* pBits = NULL;
	HBITMAP hBmp = CreateDIBSection(hDC, &bmi, DIB_RGB_COLORS, &pBits, 0, 0);

	if (!hBmp)
	{
		dc.Detach();
		ReleaseDC(GetDesktopWindow(), hDC);
		img.UnlockBits(&bmData);
		return false;
	}

	if (!pBits || !bmData.Scan0)
	{
		dc.Detach();
		ReleaseDC(GetDesktopWindow(), hDC);
		img.UnlockBits(&bmData);
		return false;
	}

	memcpy(pBits, bmData.Scan0, bmi.bmiHeader.biSizeImage);
	m_bmp.Attach(hBmp);
	m_width = bmData.Width;
	m_height = bmData.Height;
	img.UnlockBits(&bmData);
	m_dcForBlitting.SelectObject(m_bmp);
	dc.Detach();
	ReleaseDC(GetDesktopWindow(), hDC);

	return true;
}

bool CAlphaBitmap::Save(const char* pFileName)
{
	BITMAP bp;

	if (m_bmp.GetBitmap(&bp) == 0)
		return false;

	if (bp.bmBitsPixel != 32)
		return false;

	BITMAPINFO bminfo;
	memset(&bminfo, 0, sizeof(bminfo));

	bminfo.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
	bminfo.bmiHeader.biWidth = bp.bmWidth;
	bminfo.bmiHeader.biHeight = bp.bmHeight;
	bminfo.bmiHeader.biPlanes = bp.bmPlanes;
	bminfo.bmiHeader.biBitCount = bp.bmBitsPixel;
	bminfo.bmiHeader.biCompression = BI_RGB;
	bminfo.bmiHeader.biSizeImage = 0;
	bminfo.bmiHeader.biXPelsPerMeter = 0;
	bminfo.bmiHeader.biYPelsPerMeter = 0;
	bminfo.bmiHeader.biClrUsed = 0;
	bminfo.bmiHeader.biClrImportant = 0;

	int imgSize = bp.bmWidthBytes * bp.bmHeight;
	_smart_ptr<IMemoryBlock> imgBuffer = gEnv->pCryPak->PoolAllocMemoryBlock(imgSize, "");
	int returnedBytes = m_bmp.GetBitmapBits(imgSize, imgBuffer->GetData());
	if (returnedBytes == 0)
		return false;
	Gdiplus::Bitmap bitmap(&bminfo, imgBuffer->GetData());
	CLSID clsid;

	if (!GetCLSIDObjFromFileExt(pFileName, &clsid))
		return false;

	wstring wideFileName = CryStringUtils::UTF8ToWStr(pFileName);
	if (bitmap.Save(wideFileName.c_str(), &clsid, NULL) != Gdiplus::Ok)
		return false;

	return true;
}

bool CAlphaBitmap::GetCLSIDObjFromFileExt(const char* fileName, CLSID* pClsid) const
{
	if (pClsid == NULL)
		return false;

	if (strstr(fileName, ".png"))
	{
		if (GetEncoderClsid("image/png", pClsid) == -1)
			return false;
	}
	else if (strstr(fileName, ".jpg"))
	{
		if (GetEncoderClsid("image/jpeg", pClsid) == -1)
			return false;
	}
	else if (strstr(fileName, ".gif"))
	{
		if (GetEncoderClsid("image/gif", pClsid) == -1)
			return false;
	}
	else if (strstr(fileName, ".tif"))
	{
		if (GetEncoderClsid("image/tiff", pClsid) == -1)
			return false;
	}
	else if (strstr(fileName, ".bmp"))
	{
		if (GetEncoderClsid("image/bmp", pClsid) == -1)
			return false;
	}

	return true;
}

bool CAlphaBitmap::Create(void* pData, UINT aWidth, UINT aHeight, bool bVerticalFlip, bool bPremultiplyAlpha)
{
	Free();

	if (!aWidth || !aHeight)
	{
		return false;
	}

	HDC hDC = ::GetDC(GetDesktopWindow());
	CDC dc;

	dc.Attach(hDC);

	if (!m_dcForBlitting.CreateCompatibleDC(&dc))
	{
		dc.Detach();
		ReleaseDC(GetDesktopWindow(), hDC);
		return false;
	}

	if (!m_bmp.CreateCompatibleBitmap(&dc, aWidth, aHeight))
	{
		dc.Detach();
		ReleaseDC(GetDesktopWindow(), hDC);
		return false;
	}

	std::vector<UINT> vBuffer;

	if (pData)
	{
		// copy over the raw 32bpp data
		if (bVerticalFlip)
		{
			UINT nBufLen = aWidth * aHeight;
			vBuffer.resize(nBufLen);

			if (IsBadReadPtr(pData, nBufLen * 4))
			{
				//TODO: remove after testing alot the browser, it doesnt happen anymore
				CQuestionDialog::SCritical(QObject::tr(""), QObject::tr("Bad image data ptr!"));
				dc.Detach();
				ReleaseDC(GetDesktopWindow(), hDC);
				Free();
				return false;
			}

			assert(!vBuffer.empty());

			if (vBuffer.empty())
			{
				dc.Detach();
				ReleaseDC(GetDesktopWindow(), hDC);
				Free();
				return false;
			}

			UINT scanlineSize = aWidth * 4;

			for (UINT i = 0, iCount = aHeight; i < iCount; ++i)
			{
				// top scanline position
				UINT* pTopScanPos = (UINT*)&vBuffer[0] + i * aWidth;
				// bottom scanline position
				UINT* pBottomScanPos = (UINT*)pData + (aHeight - i - 1) * aWidth;

				// save a scanline from top
				memcpy(pTopScanPos, pBottomScanPos, scanlineSize);
			}

			pData = &vBuffer[0];
		}

		// premultiply alpha, AlphaBlend GDI expects it
		if (bPremultiplyAlpha)
		{
			for (UINT y = 0; y < aHeight; ++y)
			{
				BYTE* pPixel = (BYTE*) pData + aWidth * 4 * y;

				for (UINT x = 0; x < aWidth; ++x)
				{
					pPixel[0] = ((int)pPixel[0] * pPixel[3] + 127) >> 8;
					pPixel[1] = ((int)pPixel[1] * pPixel[3] + 127) >> 8;
					pPixel[2] = ((int)pPixel[2] * pPixel[3] + 127) >> 8;
					pPixel += 4;
				}
			}
		}

		BITMAPINFO bmi;
		ZeroMemory(&bmi.bmiHeader, sizeof(bmi.bmiHeader));
		bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
		bmi.bmiHeader.biBitCount = 32;
		bmi.bmiHeader.biWidth = aWidth;
		bmi.bmiHeader.biHeight = aHeight;
		bmi.bmiHeader.biPlanes = 1;
		bmi.bmiHeader.biSizeImage = aWidth * aHeight * 4;
		bmi.bmiHeader.biCompression = BI_RGB;

		if (!SetDIBits(m_dcForBlitting.GetSafeHdc(), m_bmp, 0, aHeight, pData, &bmi, DIB_RGB_COLORS))
		{
			dc.Detach();
			ReleaseDC(GetDesktopWindow(), hDC);
			return false;
		}
	}
	else
	{
		BITMAP bm;

		ZeroMemory(&bm, sizeof(BITMAP));

		if (!m_bmp.GetBitmap(&bm))
		{
			dc.Detach();
			ReleaseDC(GetDesktopWindow(), hDC);
			return false;
		}

		if (bm.bmBits)
		{
			// just clear up the bitmap bits to zero
			ZeroMemory(bm.bmBits, bm.bmWidth * bm.bmHeight * 4);
		}
	}

	// select it into the DC so we can draw on it or blit it
	m_hOldBmp = (HBITMAP)m_dcForBlitting.SelectObject(m_bmp);
	// we dont need this screen DC anymore
	m_width = aWidth;
	m_height = aHeight;
	dc.Detach();
	ReleaseDC(GetDesktopWindow(), hDC);

	return true;
}

CBitmap& CAlphaBitmap::GetBitmap()
{
	return m_bmp;
}

CDC& CAlphaBitmap::GetDC()
{
	return m_dcForBlitting;
}

void CAlphaBitmap::Free()
{
	if (m_dcForBlitting.GetSafeHdc())
	{
		m_dcForBlitting.SelectObject(m_hOldBmp);

		if (m_bmp.m_hObject)
		{
			m_bmp.DeleteObject();
		}

		m_dcForBlitting.DeleteDC();
		m_hOldBmp = NULL;
	}
}

UINT CAlphaBitmap::GetWidth()
{
	return m_width;
}

UINT CAlphaBitmap::GetHeight()
{
	return m_height;
}

int CAlphaBitmap::GetEncoderClsid(const char* format, CLSID* pClsid) const
{
	UINT num = 0;
	UINT size = 0;

	wstring wformat = CryStringUtils::UTF8ToWStr(format);

	Gdiplus::ImageCodecInfo* pImageCodecInfo = NULL;

	Gdiplus::GetImageEncodersSize(&num, &size);
	if (size == 0)
		return -1;

	pImageCodecInfo = (Gdiplus::ImageCodecInfo*)(malloc(size));
	if (pImageCodecInfo == NULL)
		return -1;

	Gdiplus::GetImageEncoders(num, size, pImageCodecInfo);

	for (UINT j = 0; j < num; ++j)
	{
		if (wcscmp(wformat.c_str(), pImageCodecInfo[j].MimeType) == 0)
		{
			*pClsid = pImageCodecInfo[j].Clsid;
			free(pImageCodecInfo);
			return j;  // Success
		}
	}

	free(pImageCodecInfo);
	return -1;  // Failure
}

bool GradientFillRect(HDC hDC, CRect& rRect, COLORREF aStartColor, COLORREF aEndColor, int aFillType)
{
	TRIVERTEX vert[2];
	GRADIENT_RECT gRect;

	vert[0].x = rRect.left;
	vert[0].y = rRect.top;
	vert[0].Red = GetRValue(aStartColor) << 8;
	vert[0].Green = GetGValue(aStartColor) << 8;
	vert[0].Blue = GetBValue(aStartColor) << 8;
	vert[0].Alpha = 0x0000;

	vert[1].x = rRect.right;
	vert[1].y = rRect.bottom;
	vert[1].Red = GetRValue(aEndColor) << 8;
	vert[1].Green = GetGValue(aEndColor) << 8;
	vert[1].Blue = GetBValue(aEndColor) << 8;
	vert[1].Alpha = 0x0000;

	gRect.UpperLeft = 0;
	gRect.LowerRight = 1;

	return TRUE == GradientFill(hDC, vert, 2, &gRect, 1, aFillType);
}

