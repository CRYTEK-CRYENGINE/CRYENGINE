// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

class CAlphaBitmap;

/*!
 *	Utility Class to manipulate images.
 */
class PLUGIN_API CImageUtil
{
public:
	//////////////////////////////////////////////////////////////////////////
	// Image loading.
	//////////////////////////////////////////////////////////////////////////
	//! Load image, detect image type by file extension.
	// Arguments:
	//   pQualityLoss - 0 if info is not needed, pointer to the result otherwise - not need to preinitialize
	static bool LoadImage(const string& fileName, CImageEx& image, bool* pQualityLoss = 0);
	static bool LoadImage(const char* fileName, CImageEx& image, bool* pQualityLoss = 0);
	//! Save image, detect image type by file extension.
	static bool SaveImage(const string& fileName, CImageEx& image);

	// General image fucntions
	static bool LoadJPEG(const string& strFileName, CImageEx& image);
	static bool SaveJPEG(const string& strFileName, CImageEx& image);

	static bool SaveBitmap(const string& szFileName, CImageEx& image, bool inverseY = true);
	static bool SaveBitmap(LPCSTR szFileName, DWORD dwWidth, DWORD dwHeight, DWORD dwBPP, HDC hdc);
	static bool LoadBmp(const string& file, CImageEx& image);
	static bool LoadBmp(const string& fileName, CImageEx& image, const RECT& rc);

	//! Save image in PGM format.
	static bool SavePGM(const string& fileName, uint32 dwWidth, uint32 dwHeight, uint32* pData);
	//! Load image in PGM format.
	static bool LoadPGM(const string& fileName, uint32* pWidthOut, uint32* pHeightOut, uint32** pImageDataOut);

	//////////////////////////////////////////////////////////////////////////
	// Image scaling.
	//////////////////////////////////////////////////////////////////////////
	//! Scale source image to fit size of target image.
	static void ScaleToFit(const CByteImage& srcImage, CByteImage& trgImage);
	//! Scale source image to fit size of target image.
	static void ScaleToFit(const CImageEx& srcImage, CImageEx& trgImage);
	//! Scale source image to fit twice side by side in target image.
	static void ScaleToDoubleFit(const CImageEx& srcImage, CImageEx& trgImage);
	//! Scale source image twice down image with filering
	enum _EAddrMode {WRAP, CLAMP};
	static void DownScaleSquareTextureTwice(const CImageEx& srcImage, CImageEx& trgImage, _EAddrMode eAddressingMode = WRAP);

	//! Smooth image.
	static void SmoothImage(CByteImage& image, int numSteps);

	//////////////////////////////////////////////////////////////////////////
	// filtered lookup
	//////////////////////////////////////////////////////////////////////////

	//! behaviour outside of the texture is not defined
	//! \param iniX in fix point 24.8
	//! \param iniY in fix point 24.8
	//! \return 0..255
	static unsigned char GetBilinearFilteredAt(const int iniX256, const int iniY256, const CByteImage& image);

private:

	static bool LoadImageWithGDIPlus(const string& fileName, CImageEx& image);
	static bool FillFromBITMAPObj(const BITMAP* bitmap, CImageEx& image);
	static bool CreateBitmapFromImage(const CImageEx& image, CBitmap& bitmapObj);
	static bool Save(const string& strFileName, CImageEx& inImage);
};

