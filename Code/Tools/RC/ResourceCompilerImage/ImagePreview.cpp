// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "stdafx.h"
#include <assert.h>                         // assert()

#include "IRCLog.h"                         // IRCLog
#include "IConfig.h"                        // IConfig

#include "ImageCompiler.h"                  // CImageCompiler
#include "ImageUserDialog.h"                // CImageUserDialog
#include "ImagePreview.h"                   // CImagePreview
#include "ImageObject.h"                    // ImageObject
#include "SimpleBitmap.h"                   // CSimpleBitmap

///////////////////////////////////////////////////////////////////////////////////

CImagePreview::CImagePreview()
	: m_pImage(0)
	, m_pPreset(0)
	, m_iScaleWidth(0)
	, m_iScaleHeight(0)
	, m_ePreviewMode(ePM_RGB)
	, m_bPreviewFiltered(false)
	, m_bPreviewTiled(false)
	, m_bPreviewGamma(false) 
{ 
}


void CImagePreview::AssignImage(const ImageObject* inImage)
{
	m_pImage = inImage;

	AssignScale(inImage);
}


void CImagePreview::AssignScale(const ImageObject* inImage)
{
	uint32 dwWidth, dwHeight, dwMips;

	inImage->GetExtent(dwWidth, dwHeight, dwMips);

	m_iScaleWidth  = dwWidth;
	m_iScaleHeight = dwHeight;
}


void CImagePreview::AssignPreset(const CImageProperties* pProps, bool bPreviewGamma, bool bOriginal)
{
	m_pPreset = pProps;

	m_ePreviewMode = (bOriginal ? pProps->m_ePreviewModeOriginal : pProps->m_ePreviewModeProcessed);
	m_bPreviewFiltered = pProps->m_bPreviewFiltered;
	m_bPreviewTiled = pProps->m_bPreviewTiled;

	m_bPreviewGamma = bPreviewGamma;
}


void CImagePreview::PrintTo(HDC inHdc, RECT &inRect, const char *inszTxt1, const char *inszTxt2)
{
	FillRect(inHdc, &inRect, GetSysColorBrush(COLOR_3DFACE));
	SetBkMode(inHdc, TRANSPARENT);
	SetTextAlign(inHdc, TA_CENTER);
	ExtTextOut(inHdc, (inRect.left + inRect.right) / 2,(inRect.top + inRect.bottom) / 2 - 8, ETO_CLIPPED, &inRect, inszTxt1, (int)strlen(inszTxt1), 0);
	if (inszTxt2)
	{
		ExtTextOut(inHdc, (inRect.left + inRect.right) / 2, (inRect.top + inRect.bottom) / 2 + 8, ETO_CLIPPED, &inRect, inszTxt2, (int)strlen(inszTxt2), 0);
	}
}


static inline const Color4<int> GetColorScaled(const Color4<uint8>& c, const int mul)
{
	return Color4<int>(
		c.components[0] * mul,
		c.components[1] * mul,
		c.components[2] * mul,
		c.components[3] * mul);
}

static inline const Color4<int> GetColorSum(const Color4<int>& c0, const Color4<int>& c1)
{
	return Color4<int>(
		c0.components[0] + c1.components[0],
		c0.components[1] + c1.components[1],
		c0.components[2] + c1.components[2],
		c0.components[3] + c1.components[3]);
}

static inline const Color4<uint8> GetColorLerp(const Color4<uint8>& c0, const Color4<uint8>& c1, const int lerp256)
{
	const Color4<int> c = GetColorSum(GetColorScaled(c0, 256 - lerp256), GetColorScaled(c1, lerp256));

	return Color4<uint8>(
		c.components[0] >> 8,
		c.components[1] >> 8,
		c.components[2] >> 8,
		c.components[3] >> 8);
}

const Color4<uint8> GetColorBilinear(
	const Color4<uint8>& x0y0,
	const Color4<uint8>& x1y0,
	const Color4<uint8>& x0y1,
	const Color4<uint8>& x1y1,
	const int lerpX256,
	const int lerpY256)
{
	const int lerpInvX256 = 256 - lerpX256;
	const int lerpInvY256 = 256 - lerpY256;

	const int k00 = (lerpInvX256 * lerpInvY256 + 128) >> 8;
	const int k10 = (lerpX256    * lerpInvY256 + 128) >> 8;
	const int k01 = (lerpInvX256 * lerpY256 + 128) >> 8;
	const int k11 = (lerpX256    * lerpY256 + 128) >> 8;

	const Color4<int> c = GetColorSum(
		GetColorSum(GetColorScaled(x0y0, k00), GetColorScaled(x1y0, k10)),
		GetColorSum(GetColorScaled(x0y1, k01), GetColorScaled(x1y1, k11)));

	return Color4<uint8>(
		 c.components[0] >> 8,
		 c.components[1] >> 8,
		 c.components[2] >> 8,
		 c.components[3] >> 8);
}


// fractional part of samplePos is normalized coordinate
static bool ComputeSamplingAddress(
	int& resSample0, int& resSample1, int& resLerp256,
	const bool bClamp, const bool bLinear, const float samplePos, const int sampleCount)
{
	float f = samplePos * sampleCount;

	if (bClamp)
	{
		if (samplePos < 0 || samplePos >= 1)
		{
			return false;
		}
		Util::clampMinMax(f, 0.5f, sampleCount - 0.5f);
	}

	const int i256 = int((f - 0.5f) * 256);
	const int ik = i256 & 0xff;
	const int i = (i256 < 0)
		? (sampleCount - 1) - (((-i256) >> 8) % sampleCount)
		: (i256 >> 8) % sampleCount;

	resSample0 = i;
	resSample1 = (i + 1 >= sampleCount) ? 0 : i + 1;

	resLerp256 = ik;
	if (!bLinear)
	{
		resLerp256 = (resLerp256 <= 127) ? 0 : 256;
	}

	return true;
}



bool CImagePreview::BlitTo(HDC inHdc, RECT &inRect, const float infOffsetX, const float infOffsetY, const int iniScale64)
{	
	assert(m_pImage);   // this was checked earlier

	const bool bHasBeenRenormalized = 
		 m_pImage->HasImageFlags(CImageExtensionHelper::EIF_RenormalizedTexture);
	const bool bHasColorModel = 
		 m_pImage->HasImageFlags(CImageExtensionHelper::EIF_Colormodel);
	const bool bConvertToGammaSpace = m_bPreviewGamma && (bHasBeenRenormalized || bHasColorModel ||
		!m_pImage->HasImageFlags(CImageExtensionHelper::EIF_SRGBRead));
	const bool bIsSigned = 
		 CPixelFormats::IsFormatSigned(m_pImage->GetPixelFormat());
	const bool bIsFloatingPoint = 
		 CPixelFormats::IsFormatFloatingPoint(m_pImage->GetPixelFormat(), false);

	const ImageObject::EColorNormalization eColorNorm = ImageObject::eColorNormalization_Normalize;
	const ImageObject::EAlphaNormalization eAlphaNorm = ImageObject::eAlphaNormalization_Normalize;

	ImageToProcess image(m_pImage->CopyImage());
	assert(image.get() != m_pImage);
	image.get()->CopyImageFlagsFrom(*m_pImage);

	if (bHasBeenRenormalized || bHasColorModel || bConvertToGammaSpace || bIsSigned || bIsFloatingPoint)
	{
		image.ConvertFormat(m_pPreset, ePixelFormat_A32B32G32R32F);
	}

	if (bHasBeenRenormalized || bHasColorModel)
	{
		// expand range in linear space
		if (image.get()->HasImageFlags(CImageExtensionHelper::EIF_SRGBRead))
		{
			image.GammaToLinearRGBA32F(true);
		}

		image.get()->ExpandImageRange(eColorNorm, eAlphaNorm, bIsFloatingPoint ? 4 : 0);
	}

	if (m_ePreviewMode != ePM_RawRGB &&
	    m_ePreviewMode != ePM_RawCIE &&
	    m_ePreviewMode != ePM_RawIRB &&
	    m_ePreviewMode != ePM_RawYCbCr &&
	    m_ePreviewMode != ePM_RawYFbFr)
	{
		image.ConvertModel(m_pPreset, CImageProperties::eColorModel_RGB);
	}

	// pseudo tone-mapping to allow the previewer to show a human-friendly image
	if (bIsFloatingPoint)
	{
		image.get()->NormalizeImageRange(eColorNorm, eAlphaNorm, true, 0);
	}

	if (bConvertToGammaSpace)
	{
		image.LinearRGBAAnyFToGammaRGBAAnyF();
	}

	image.ConvertFormat(m_pPreset, ePixelFormat_A8R8G8B8);

	if (CPixelFormats::IsFormatSingleChannel(m_pImage->GetPixelFormat()))
	{
		image.get()->Swizzle("rrr1");
	}

	const int iWidth  = inRect.right - inRect.left;
	const int iHeight = inRect.bottom - inRect.top;

	const int iX = (int)(infOffsetX * m_iScaleWidth  * 1.0f);
	const int iY = (int)(infOffsetY * m_iScaleHeight * 1.0f);

	const int iScaleX = (int)(iWidth  * 64.0f / iniScale64 * 0.5f);
	const int iScaleY = (int)(iHeight * 64.0f / iniScale64 * 0.5f);

	{
		BITMAPINFO bmi;
		memset(&bmi, 0, sizeof(bmi));
		bmi.bmiHeader.biSize = sizeof(bmi.bmiHeader);
		bmi.bmiHeader.biPlanes = 1;
		bmi.bmiHeader.biBitCount = 32;
		bmi.bmiHeader.biCompression = BI_RGB;
		bmi.bmiHeader.biWidth = iWidth;
		bmi.bmiHeader.biHeight = -iHeight;   // '-h' - top-to-bottom, '+h' - bottom-to-top

		CSimpleBitmap<Color4<uint8> > bmp;
		bmp.SetSize(iWidth, iHeight);

		// Preparing checkerboard background
		{
			const Color4<uint8> c0(0xc0, 0xc0, 0xc0, 0xff);
			const Color4<uint8> c1(0xf0, 0xf0, 0xf0, 0xff);
			for (int y = 0; y < iHeight; ++y)
			{
				Color4<uint8>* const pDst = &bmp.GetRef(0, y);
				for (int x = 0; x < iWidth; ++x)
				{
					pDst[x] = ((x ^ y) & 0x8) ? c0 : c1;
				}
			}
		}
		
		const ImageObject* const pImage = GeneratePreviewImage(image.get());
		if (!pImage)
		{
			return false;
		}

		// Selecting proper mip image
		int mip = 0;
		for (;;)
		{
			const float delta_s = (2.0f * iScaleX) / m_iScaleWidth;
			const float step_s = delta_s / iWidth;
			const float step_u = step_s * pImage->GetWidth(mip);
			if (step_u <= 1.5f || mip + 1 >= pImage->GetMipCount())
			{
				break;
			}
			++mip;
		}

		// s texture coordinate
		float s0 = (float)(-iScaleX + iX) / m_iScaleWidth;
		float step_s;
		{
			const float s1 = (float)(iScaleX + iX) / m_iScaleWidth;
			const float delta_s = s1 - s0;
			step_s = delta_s / iWidth;
			s0 += 0.5f * step_s;
		}

		// t texture coordinate
		float t0 = (float)(-iScaleY + iY) / m_iScaleHeight;
		float step_t;
		{
			const float t1 = (float)(iScaleY + iY) / m_iScaleHeight;
			const float delta_t = t1 - t0;
			step_t = delta_t / iHeight;
			t0 += 0.5f * step_t;
		}

		const int imageW = pImage->GetWidth(mip);
		const int imageH = pImage->GetHeight(mip);

		for (int y = 0; y < iHeight; ++y)
		{
			Color4<uint8>* const pDst = &bmp.GetRef(0, y);

			int y0, y1, yLerp256;
			if (!ComputeSamplingAddress(y0, y1, yLerp256, !m_bPreviewTiled, m_bPreviewFiltered, t0 + step_t * y, imageH))
			{
				continue;
			}

			const Color4<uint8>* const pSrcRow0 = &pImage->GetPixel<Color4<uint8> >(mip, 0, y0);
			const Color4<uint8>* const pSrcRow1 = &pImage->GetPixel<Color4<uint8> >(mip, 0, y1);

			for (int x = 0; x < iWidth; ++x)
			{
				int x0, x1, xLerp256;
				if (!ComputeSamplingAddress(x0, x1, xLerp256, !m_bPreviewTiled, m_bPreviewFiltered, s0 + step_s * x, imageW))
				{
					continue;
				}

				Color4<uint8> c = GetColorBilinear(pSrcRow0[x0], pSrcRow0[x1], pSrcRow1[x0], pSrcRow1[x1], xLerp256, yLerp256);

				// Blending

				if (m_ePreviewMode == ePM_AlphaTestNoBlend)
				{
					if (c.components[3] < 128)
					{
						continue;
					}
					c.components[3] = 255;
				}
				else if (m_ePreviewMode == ePM_AlphaTestBlend)
				{
					if (c.components[3] < 128)
					{
						continue;
					}
				}

				if (c.components[3] >= 255)
				{
					pDst[x] = c;
				}
				else
				{
					pDst[x] = GetColorLerp(pDst[x], c, int(c.components[3] * (256 / 255.0f)));
				}
			}
		}

		::SetDIBitsToDevice(
			inHdc,
			inRect.left, inRect.top,
			iWidth, iHeight,
			0, 0,
			0, iHeight,
			bmp.Get(0, 0),
			&bmi,
			DIB_RGB_COLORS);

		delete pImage;
	}

	return true;
}


bool CImagePreview::ClampBlitOffset(const int iniWidth, const int iniHeight, float &inoutfX, float &inoutfY, const int iniScale64) const
{
	bool bRet = true;

	uint32 dwWidth, dwHeight, dwMips;

	m_pImage->GetExtentTransformed(dwWidth, dwHeight, dwMips, *m_pPreset);

	if (iniWidth >= (int)(dwWidth * iniScale64) / 64)
	{
		inoutfX = 0.5f;
	}
	else
	{
		float fXMin = (float)((iniWidth * 64) / iniScale64) / (float)dwWidth * 0.5f;
		float fXMax = 1.0f - fXMin;

		if (inoutfX < fXMin) inoutfX = fXMin;
		if (inoutfX > fXMax) inoutfX = fXMax;

		bRet = false;
	}

	if (iniHeight >= (int)(dwHeight * iniScale64) / 64)
	{
		inoutfY = 0.5f;
	}
	else
	{
		float fYMin = (float)((iniHeight * 64) / iniScale64) / (float)dwHeight * 0.5f;
		float fYMax = 1.0f - fYMin;

		if (inoutfY < fYMin) inoutfY = fYMin;
		if (inoutfY > fYMax) inoutfY = fYMax;

		bRet = false;
	}

	return bRet;
}


ImageObject* CImagePreview::GeneratePreviewImage(const ImageObject* pInputImage) const
{
	if (!pInputImage || (pInputImage->GetPixelFormat() != ePixelFormat_A8R8G8B8))
	{
		return 0;
	}

	const uint32 dwWidth = pInputImage->GetWidth(0);
	const uint32 dwHeight = pInputImage->GetHeight(0);
	const uint32 dwMipCount = pInputImage->GetMipCount();

	ImageObject* const pOutputImage = new ImageObject(dwWidth, dwHeight, dwMipCount, ePixelFormat_A8R8G8B8, pInputImage->GetCubemap());
	if (!pOutputImage)
	{
		return 0;
	}
		
	Vec4 cMinColor;
	Vec4 cMaxColor;
	pInputImage->GetColorRange(cMinColor, cMaxColor);
	const Vec4 cScaleColor = cMaxColor - cMinColor;

	for (uint32 mip = 0; mip < dwMipCount; ++mip)
	{
		const uint32 dwLocalWidth = pInputImage->GetWidth(mip);
		const uint32 dwLocalHeight = pInputImage->GetHeight(mip);

		const Color4<uint8>* pPixelsSrc = pInputImage->GetPixelsPointer<Color4<uint8> >(mip);

		for (uint32 dwY = 0; dwY < dwLocalHeight; ++dwY)
		{
			Color4<uint8>* pPixelsDst = &pOutputImage->GetPixel<Color4<uint8> >(mip, 0, dwY);

			for (uint32 dwX = 0; dwX < dwLocalWidth; ++dwX, ++pPixelsSrc)
			{
				uint32 b = pPixelsSrc->components[0];
				uint32 g = pPixelsSrc->components[1];
				uint32 r = pPixelsSrc->components[2];
				uint32 a = pPixelsSrc->components[3];

				switch (m_ePreviewMode)
				{
				case ePM_RGB:
					a = 255;

				case ePM_RGBA:
				case ePM_AlphaTestBlend:
				case ePM_AlphaTestNoBlend:
					break;

				case ePM_AAA:
				case ePM_KKK:
					r = g = b = a;
					a = 255;
					break;

				case ePM_NormalLength:
					{
						const float x = (r - 128.0f) / 128.0f;
						const float y = (g - 128.0f) / 128.0f;
						const float z = (b - 128.0f) / 128.0f;

						const uint32 grey = (uint32)(sqrtf(x * x + y * y + z * z) * 255.0f);

						if (grey < 2)		// (0,0,0) normal = blue
						{
							b = 255;
							g = 0;
							r = 0;
							a = 255;
						}
						else if (grey > 255)		// illegal normals = red
						{
							const uint32 val = Util::getMin(grey - 255, 255U);
							b = g = Util::getMax(192 - val, 0U);
							r = 255;
							a = 255;
						}
						else
						{
							r = g = b = grey;
							a = 255;
						}
					}
					break;

				case ePM_RGBLinearK:
					{
						const float mul = (a * (1 / 255.0f)) * CImageProperties::GetRGBKMaxValue();

						b = Util::getClamped(uint(b * mul), 0U, 255U);
						g = Util::getClamped(uint(g * mul), 0U, 255U);
						r = Util::getClamped(uint(r * mul), 0U, 255U);
						a = 255;
					}
					break;

				case ePM_RGBSquareK:
					{
						const float mul = Util::square(a * (1 / 255.0f)) * CImageProperties::GetRGBKMaxValue();

						b = Util::getClamped(uint(b * mul), 0U, 255U);
						g = Util::getClamped(uint(g * mul), 0U, 255U);
						r = Util::getClamped(uint(r * mul), 0U, 255U);
						a = 255;
					}
					break;

				case ePM_RawRGB:
				case ePM_RawCIE:
				case ePM_RawIRB:
				case ePM_RawYCbCr:
				case ePM_RawYFbFr:
					a = 255;
					break;

				default:
					assert(0);
					break;
				}

				// dynamic scaling
				{
					Vec4 col(r / 255.0f, g / 255.0f, b / 255.0f, a / 255.f);
					col = col * cScaleColor + cMinColor;

					r = (uint32)Util::getClamped(col.x * 255.0f + 0.5f, 0.0f, 255.0f);
					g = (uint32)Util::getClamped(col.y * 255.0f + 0.5f, 0.0f, 255.0f);
					b = (uint32)Util::getClamped(col.z * 255.0f + 0.5f, 0.0f, 255.0f);
					a = (uint32)Util::getClamped(col.w * 255.0f + 0.5f, 0.0f, 255.0f);
				}

				pPixelsDst->components[0] = b;
				pPixelsDst->components[1] = g;
				pPixelsDst->components[2] = r;
				pPixelsDst->components[3] = a;
				++pPixelsDst;
			}
		}
	}

	return pOutputImage;
}
