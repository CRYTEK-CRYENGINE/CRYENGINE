// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#ifndef __image_dxtc_h__
#define __image_dxtc_h__
#pragma once

#include <Cry3DEngine/ImageExtensionHelper.h>

class CImage_DXTC
{
	// Typedefs
public:
protected:
	//////////////////////////////////////////////////////////////////////////
	// Extracted from Compressorlib.h on SDKs\CompressATI directory.
	// Added here because we are not really using the elements inside
	// this header file apart from those definitions as we are currently
	// loading the DLL CompressATI2.dll manually as recommended by the rendering
	// team.
	typedef enum
	{
		FORMAT_ARGB_8888,
		FORMAT_ARGB_TOOBIG
	} UNCOMPRESSED_FORMAT;

	typedef enum
	{
		COMPRESSOR_ERROR_NONE,
		COMPRESSOR_ERROR_NO_INPUT_DATA,
		COMPRESSOR_ERROR_NO_OUTPUT_POINTER,
		COMPRESSOR_ERROR_UNSUPPORTED_SOURCE_FORMAT,
		COMPRESSOR_ERROR_UNSUPPORTED_DESTINATION_FORMAT,
		COMPRESSOR_ERROR_UNABLE_TO_INIT_CODEC,
		COMPRESSOR_ERROR_GENERIC
	} COMPRESSOR_ERROR;
	//////////////////////////////////////////////////////////////////////////

	// Methods
public:
	CImage_DXTC();
	~CImage_DXTC();

	// Arguments:
	//   pQualityLoss - 0 if info is not needed, pointer to the result otherwise - not need to preinitialize
	bool                      Load(const char* filename, CImageEx& outImage, bool* pQualityLoss = 0); // true if success

	static inline const char* NameForTextureFormat(ETEX_Format ETF) { return CImageExtensionHelper::NameForTextureFormat(ETF); }
	static inline bool        IsBlockCompressed(ETEX_Format ETF)    { return CImageExtensionHelper::IsBlockCompressed(ETF); }
	static inline bool        IsLimitedHDR(ETEX_Format ETF)         { return CImageExtensionHelper::IsRangeless(ETF); }
	static inline int         BitsPerPixel(ETEX_Format eTF)         { return CImageExtensionHelper::BitsPerPixel(eTF); }

	int                       TextureDataSize(int nWidth, int nHeight, int nDepth, int nMips, ETEX_Format eTF);

private:
	static COMPRESSOR_ERROR CheckParameters(
	  int width,
	  int height,
	  UNCOMPRESSED_FORMAT destinationFormat,
	  const void* sourceData,
	  void* destinationData,
	  int destinationDataSize);

	static COMPRESSOR_ERROR DecompressTextureBTC(
	  int width,
	  int height,
	  ETEX_Format sourceFormat,
	  UNCOMPRESSED_FORMAT destinationFormat,
	  const int imageFlags,
	  const void* sourceData,
	  void* destinationData,
	  int destinationDataSize,
	  int destinationPageOffset);
};

#endif // __image_dxtc_h__

