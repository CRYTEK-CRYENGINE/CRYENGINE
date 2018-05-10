// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

class CImageProperties;
class CImageCompiler;
class ImageObject;

// needs to be in the same order as in IDC_PREVIEWMODE
enum EPreviewMode
{
	ePM_RGB = 0,          // normal
	ePM_AAA,              // replicate the alpha channel as greyscale value
	ePM_KKK,              // replicate the multiplier channel as greyscale value
	ePM_RawRGB,           // debug unmultiplied RGB channels of RGBK
	ePM_RawCIE,           // debug CIE channels
	ePM_RawIRB,           // debug IRB channels
	ePM_RawYCbCr,         // debug Y'CbCr channels
	ePM_RawYFbFr,         // debug Y'FbFr channels
	ePM_RGBA,             // alpha to fade between RGB and background
	ePM_NormalLength,     // replicate length of the normal as greyscale value
	ePM_RGBLinearK,       // RGB * Alpha (good to fake HDR)
	ePM_RGBSquareK,       // RGB * Alpha^2 (good to fake HDR)
	ePM_AlphaTestBlend,   // Shows the Alpha Channel visualized with an AlpheTest of 0.5, blending with background is enabled
	ePM_AlphaTestNoBlend, // Shows the Alpha Channel visualized with an AlpheTest of 0.5, blending with background is disabled

	ePM_Num
};

class CImagePreview
{
public:
	CImagePreview();

	void AssignImage(const ImageObject* inImage);
	void AssignScale(const ImageObject* inImage);
	void AssignPreset(const CImageProperties* pProps, bool bPreviewGamma, bool bOriginal);

	void PrintTo(HDC inHdc, RECT &inRect, const char *inszTxt1, const char *inszTxt2 = NULL);

	//! /param infOffsetX
	//! /param infOffsetY
	//! /param iniScale64 64=1:1, bigger values magnify more
	//! /return true=blit was successful, false otherwise
	bool BlitTo(HDC inHdc, RECT &inRect, const float infOffsetX, const float infOffsetY, const int iniScale64);

	//! /return true=x and y is bound to 0.5 because the texture is smaller than the preview area, false otherwise
	bool ClampBlitOffset(const int iniWidth, const int iniHeight, float &inoutfX, float &inoutfY, const int iniScale64) const;

	// ----------------------------------------

	static const char *GetPreviewModeChoiceText(EPreviewMode ePreview)
	{
		switch (ePreview)
		{
		case ePM_RGB:              return "RGB";
		case ePM_AAA:              return "Alpha";
		case ePM_KKK:              return "K";
		case ePM_RawRGB:           return "Unmultiplied RGB";
		case ePM_RawCIE:           return "CIE";
		case ePM_RawIRB:           return "IRB";
		case ePM_RawYCbCr:         return "Y'CbCr";
		case ePM_RawYFbFr:         return "Y'FbFr";
		case ePM_RGBA:             return "RGB Alpha";
		case ePM_RGBLinearK:       return "RGB*K (HDR)";
		case ePM_RGBSquareK:       return "RGB*K*K (HDR)";
		case ePM_AlphaTestBlend:   return "AlphaTestBlend";
		case ePM_AlphaTestNoBlend: return "AlphaTestNoBlend";
		case ePM_NormalLength:     return "Debug Normals";
		default:                   assert(0); return "";
		}
	}

	static const char *GetPreviewModeDescription(EPreviewMode ePreview)
	{
		switch (ePreview)
		{
		case ePM_RGB:              return "Normal RGB preview mode\n(no gamma correction)";
		case ePM_AAA:              return "Preview Alpha channel in greyscale\n(no gamma correction)";
		case ePM_KKK:              return "Preview RGB multiplier in greyscale\n(no gamma correction)";
		case ePM_RawRGB:           return "Raw RGB preview mode\n(no gamma correction)";
		case ePM_RawCIE:           return "Raw CIE preview mode\n(no gamma correction)";
		case ePM_RawIRB:           return "Raw IRB preview mode\n(no gamma correction)";
		case ePM_RawYCbCr:         return "Raw Y'CbCr preview mode\n(no gamma correction)";
		case ePM_RawYFbFr:         return "Raw Y'FbFr preview mode\n(no gamma correction)";
		case ePM_RGBA:             return "RGB Alpha blended with the background\n(no gamma correction)";
		case ePM_RGBLinearK:       return "RGB multiplied with Alpha, useful to get cheap HDR textures\n(no gamma correction)";
		case ePM_RGBSquareK:       return "RGB multiplied with squared Alpha, useful to get cheap HDR textures\n(no gamma correction)";
		case ePM_AlphaTestBlend:   return "Shows the Alpha Channel visualized with an AlphaTest of 0.5\n(blending with the background is enabled)";
		case ePM_AlphaTestNoBlend: return "Shows the Alpha Channel visualized with an AlphaTest of 0.5\n(blending with the background is disabled)";
		case ePM_NormalLength:     return "Normal map debug: The brighter a pixel is the better the normal,\nblue indicates very short normals, red impossible normals (>1)";
		default:                   assert(0); return "";
		}
	}

private:
	// Arguments:
	//   pInputImage - has to be in the format A8R8G8B8
	// Returns:
	//   outBitmap - resulting preview image in the format A8R8G8B8
	ImageObject* GeneratePreviewImage(const ImageObject* pInputImage) const;

private:
	// ------------------------------------------------------------------
	const ImageObject*        m_pImage;                 // image to process
	const CImageProperties*   m_pPreset;                // preset to use

	uint32                    m_iScaleWidth;
	uint32                    m_iScaleHeight;

	EPreviewMode              m_ePreviewMode;
	bool                      m_bPreviewFiltered;       // activate the bilinear filter in the preview
	bool                      m_bPreviewTiled;

	bool                      m_bPreviewGamma;          // display image in gamma-space
};
