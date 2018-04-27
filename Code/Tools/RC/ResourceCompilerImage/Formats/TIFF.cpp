// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include <assert.h>          // assert()
#include <iterator>

#include "../ImageObject.h"  // ImageToProcess

#include "IRCLog.h"          // IRCLog

#include "IPTCHeader.h"      // IPTCHeader
#include "TIFF.h"

///////////////////////////////////////////////////////////////////////////////////

#include <tiffio.h>

///////////////////////////////////////////////////////////////////////////////////

namespace ImageTIFF
{

///////////////////////////////////////////////////////////////////////////////////

static string GetSpecialInstructionsFromTIFF(TIFF* tif)
{
	// get image metadata
	const unsigned char* pPtr = NULL;
	unsigned int wc = 0;

	if (!TIFFGetField(tif, TIFFTAG_PHOTOSHOP, &wc, &pPtr))		// 34377 IPTC TAG
	{
		return string();
	}

	const unsigned char* const pPtrEnd = pPtr + wc;

	while (pPtr < pPtrEnd)
	{
		const unsigned char* const pPtrStart = pPtr;

		if (pPtr[0] != '8' || pPtr[1] != 'B' || pPtr[2] != 'I' || pPtr[3] != 'M')
		{
			assert(0);
			RCLogError("%s failed", __FUNCTION__);
			return string();
		}
		pPtr += 4;

		const unsigned short Id = (((unsigned short)pPtr[0])<<8) | (unsigned short)pPtr[1];
		pPtr += 2;

		// get pascal string 
		const unsigned int dwNameSize = (unsigned int)pPtr[0];
		++pPtr;
		char szName[256];
		memcpy(szName, pPtr, dwNameSize);
		szName[dwNameSize] = 0;
		pPtr += dwNameSize;
/*
		OutputDebugString("Tag: '");
		OutputDebugString(szName);
		OutputDebugString("'\n");
*/

		// align 2 bytes
		if ((pPtr - pPtrStart) & 1)
		{
			++pPtr;
		}

		const unsigned int dwSize = 
			(((unsigned int)pPtr[0]) << 24) | 
			(((unsigned int)pPtr[1]) << 16) | 
			(((unsigned int)pPtr[2]) << 8) | 
			(unsigned int)pPtr[3];
		pPtr += 4;

		//if(strcmp(szName,"Caption")==0)
		//	return GetSpecialInstructionsFromPhotoshopCaption((unsigned char *)pPtr,dwSize);

		if (Id == 0x0404)
		{
			CIPTCHeader iptcHeader;   // stores the IPTC (ie caption, author) metadata - used to store settings.
			iptcHeader.Parse(pPtr, dwSize);
			std::vector<unsigned char> buffer;
			iptcHeader.GetCombinedFields(CIPTCHeader::FieldSpecialInstructions, buffer, " ");
			return string(reinterpret_cast<const char*>(&buffer[0]));
		}

		pPtr += dwSize;

		// align 2 bytes
		if ((pPtr - pPtrStart) & 1)
		{
			++pPtr;
		}
	}

	return string();
}

string LoadSpecialInstructionsByUsingTIFFLoader(const char* filenameRead)
{
	string specialInstructions;

	wstring widePath;
	Unicode::Convert(widePath, filenameRead);

	TIFF* const tif = TIFFOpenW(widePath.c_str(), "r");
	if (tif) 
	{
		specialInstructions = GetSpecialInstructionsFromTIFF(tif);
		TIFFClose(tif);
	}

	return specialInstructions;
}

///////////////////////////////////////////////////////////////////////////////////

// Note that a similar function exists in CryTIFPlugin.cpp
// of the new CryTif plugin (Jan 2013).
static bool IsFloatTif(TIFF* const tif, const bool isCryTif2012)
{
	uint32 dwBitsPerChannel = 0;
	uint32 dwFormat = 0;
	TIFFGetField(tif, TIFFTAG_BITSPERSAMPLE, &dwBitsPerChannel);
	TIFFGetField(tif, TIFFTAG_SAMPLEFORMAT, &dwFormat);

	if (dwBitsPerChannel == 32)
	{
		return dwFormat == SAMPLEFORMAT_IEEEFP;
	}

	if (dwBitsPerChannel == 16)
	{
		if (dwFormat == SAMPLEFORMAT_IEEEFP)
		{
			return true;
		}

		// Preventing interpretation of old CryTif/RC/Sandbox 16-bit
		// float images as 16-bit uint images.
		if (isCryTif2012)
		{
			// TIFFTAG_SUBFILETYPE exists in files written by the
			// generic Photoshop TIFF writer, but not in CryTif files.
			uint32 wSubfileType = 0;
			const bool bPhotoshopTif = (TIFFGetField(tif, TIFFTAG_SUBFILETYPE, &wSubfileType) != 0);

			// Old Sandbox (before 2013) incorrectly saved 16-bit float
			// cubemap images without specifying TIFFTAG_SAMPLEFORMAT at all.
			// Note that the generic Photoshop TIFF writer saves 16-bit 
			// uint images without specifying TIFFTAG_SAMPLEFORMAT tag as well,
			// it's why we have bPhotoshopTif check here.
			if (!bPhotoshopTif && dwFormat == 0)
			{
				if (StringHelpers::Contains(GetSpecialInstructionsFromTIFF(tif), "/preset=HDRCubemapRGBK_highQ"))
				{
					return true;
				}
			}

			// Old RC (before 2013) incorrectly saved 16-bit float images
			// as SAMPLEFORMAT_UINT (see UpdateAndSaveConfigToTIF() in
			// ImageCompiler.cpp in an old RC).
			if (!bPhotoshopTif && dwFormat == SAMPLEFORMAT_UINT)
			{
				if (StringHelpers::Contains(GetSpecialInstructionsFromTIFF(tif), "/preset="))
				{
					return true;
				}
			}
		}
	}

	return false;
}

///////////////////////////////////////////////////////////////////////////////////

static void FormPhotoshopDataBlock(std::vector<char>& data, const string& str)
{
	data.clear();

	// Use temporary variable to prevent failure in case of "merge strings"
	// compiler option turned off, because then every parameter has a different address.
#define ADD(ref) { const char* const cstr = ref; std::copy(cstr, cstr + sizeof(ref) - 1, std::back_inserter(data)); }
	ADD("8BIM");
	ADD("\x04\x04");
	ADD("\0\0");
	int irbSizePosition = int(data.size());
	ADD("\0\0\0\0");
	int irbDataStart = int(data.size());
	ADD("\x1C\x02");
	ADD("\0");
	ADD("\x00\x02");
	ADD("\x00\x02");
	ADD("\x1C\x02");
	ADD("\x28");
	int captionSizePosition = int(data.size());
	ADD("\0\0");
	int captionStartPosition = int(data.size());
	std::copy(str.c_str(), str.c_str() + str.size(), std::back_inserter(data));
#undef ADD

	int captionSize = int(data.size()) - captionStartPosition;
	char* captionSizePointer = &data[0] + captionSizePosition;
	captionSizePointer[0] = (captionSize >> 8) & 0xFF;
	captionSizePointer[1] = captionSize & 0xFF;

	int irbSize = int(data.size()) - irbDataStart;
	char* irbSizePointer = &data[0] + irbSizePosition;
	irbSizePointer[0] = (irbSize >> 24) & 0xFF;
	irbSizePointer[1] = (irbSize >> 16) & 0xFF;
	irbSizePointer[2] = (irbSize >>  8) & 0xFF;
	irbSizePointer[3] = irbSize & 0xFF;
}

///////////////////////////////////////////////////////////////////////////////////

// loads simple RAWImage from 8bit uint tiff source raster
static ImageObject* Load8BitImageFromTIFF(struct tiff* tif);
// loads simple FloatImage from 16bit uint tiff source raster
static ImageObject* Load16BitImageFromTIFF(struct tiff* tif);
// loads simple FloatImage from 16f HDR tiff source raster
static ImageObject* Load16BitHDRImageFromTIFF(struct tiff* tif);
// loads simple FloatImage from 32f HDR tiff source raster
static ImageObject* Load32BitHDRImageFromTIFF(struct tiff* tif);

ImageObject* LoadByUsingTIFFLoader(const char* filenameRead, CImageProperties* pProps, string& res_specialInstructions)
{
	res_specialInstructions.clear();

	wstring widePath;
	Unicode::Convert(widePath, filenameRead);

	TIFF* const tif = TIFFOpenW(widePath.c_str(), "r");

	std::auto_ptr<ImageObject> pRet;

	if (!tif) 
	{
		char messageBuffer[2048] = { 0 };
		FormatMessageA(FORMAT_MESSAGE_FROM_SYSTEM, NULL, GetLastError(), 0, messageBuffer, sizeof(messageBuffer) - 1, NULL);
		string messagetrim(messageBuffer);
		messagetrim.replace('\n',' ');
		RCLogError("%s: TIFFOpenW failed (%s) - (%s)", __FUNCTION__, messagetrim.c_str(), filenameRead); 
	}
	else
	{
		uint32 dwBitsPerChannel = 0;
		uint32 dwChannels = 0;
		TIFFGetField(tif, TIFFTAG_SAMPLESPERPIXEL, &dwChannels);
		TIFFGetField(tif, TIFFTAG_BITSPERSAMPLE, &dwBitsPerChannel);

		if (dwChannels != 1 && dwChannels != 2 && dwChannels != 3 && dwChannels != 4)
		{
			TIFFClose(tif);
			RCLogError("Unsupported TIFF pixel format (channel count: %d)", (int)dwChannels);
			assert(0);
			return 0;
		}

		const bool isCryTif2012 = pProps && pProps->GetConfigAsBool("CryTIF2012", false, true);

		const char* pFormatText;
		if (dwBitsPerChannel == 8)
		{
			// A/L/R8, GR8, BGR8, BGRA8
			pFormatText = "8-bit";
			pRet.reset(Load8BitImageFromTIFF(tif));
		}
		else if (dwBitsPerChannel == 16)
		{
			// A/L/R16, R16F, GR16, GR16f, ARGB16, ARGB16f
			if (IsFloatTif(tif, isCryTif2012))
			{
				pFormatText = "16-bit float";
				pRet.reset(Load16BitHDRImageFromTIFF(tif));
			}
			else
			{
				pFormatText = "16-bit int";
				pRet.reset(Load16BitImageFromTIFF(tif));
			}
		}
		else if (dwBitsPerChannel == 32 && IsFloatTif(tif, isCryTif2012))
		{
			// A/L/R32f, GR32f, ARGB32f
			pFormatText = "32-bit float";
			pRet.reset(Load32BitHDRImageFromTIFF(tif));
		}
		else
		{
			TIFFClose(tif);
			RCLogError("Unsupported TIFF pixel format");
			assert(0);
			return 0;
		}

		if (pProps && (pProps->GetVerbosityLevel() > 2))
		{
			RCLog("TIFF file format: %s, %d channels", pFormatText, (int)dwChannels);
		}

		if (!pRet.get())
		{
			TIFFClose(tif);
			RCLogError("Failed to read TIFF pixels");
			assert(0);
			return 0;
		}

		res_specialInstructions = GetSpecialInstructionsFromTIFF(tif);

		TIFFClose(tif);
	}

	if (pProps != 0)
	{
		pProps->ClearInputImageFlags();

		if (pRet.get())
		{
			pProps->m_bPreserveAlpha = pRet->HasNonOpaqueAlpha();
		}
	}

	return pRet.release();
}

static ImageObject* Load8BitImageFromTIFF(TIFF* tif)
{
	uint32 dwChannels = 0;
	uint32 dwPhotometric = 0;
	uint32 dwPlanar = 0;
	TIFFGetField(tif, TIFFTAG_SAMPLESPERPIXEL, &dwChannels);
	TIFFGetField(tif, TIFFTAG_PHOTOMETRIC, &dwPhotometric);
	TIFFGetField(tif, TIFFTAG_PLANARCONFIG, &dwPlanar);
	if (dwChannels != 1 && dwChannels != 2 && dwChannels != 3 && dwChannels != 4)
	{
		RCLogError("%s failed (channel count)", __FUNCTION__);
		return 0;
	}

	uint32 dwWidth = 0;
	uint32 dwHeight = 0;
	TIFFGetField(tif, TIFFTAG_IMAGEWIDTH, &dwWidth);
	TIFFGetField(tif, TIFFTAG_IMAGELENGTH, &dwHeight);
	if (dwWidth <= 0 || dwHeight <= 0)
	{
		RCLogError("%s failed (empty image)", __FUNCTION__);
		return 0;
	}

	EPixelFormat eFormat = ePixelFormat_X8R8G8B8;

	if (dwChannels == 1 && dwPhotometric != PHOTOMETRIC_MINISBLACK)
	{
		eFormat = ePixelFormat_R8;
	}
	else if (dwChannels == 4)
	{
		eFormat = ePixelFormat_A8R8G8B8;
	}

	std::auto_ptr<ImageObject> pRet(new ImageObject(dwWidth, dwHeight, 1, eFormat, ImageObject::eCubemap_UnknownYet));

	char* dst;
	uint32 dwPitch;
	pRet->GetImagePointer(0, dst, dwPitch);

	std::vector<char> buf(dwPitch);

	if (dwPlanar == PLANARCONFIG_SEPARATE)
	{
		for (uint32 dwC = 0; dwC < dwChannels; ++dwC)
		{
			for (uint32 dwY = 0; dwY < dwHeight; ++dwY)
			{
				if (TIFFReadScanline(tif, &buf[0], dwY, dwC) == -1)
				{
					RCLogError("%s failed (reading row %d, channel %d)", __FUNCTION__, dwY, dwC);
					return 0;
				}

				const uint8* srcLine = (const uint8*)&buf[0];
				uint8* dstLine = (uint8*)&dst[dwPitch * dwY];

				if (dwChannels == 1)
				{
					if (dwPhotometric != PHOTOMETRIC_MINISBLACK)
					{
						for (uint32 dwX = 0; dwX < dwWidth; ++dwX)
						{
							dstLine[0] = (dwC == 0 ? srcLine[0] : dstLine[0]);

							dstLine += 1;
							srcLine += 1;
						}
					}
					else
					{
						for (uint32 dwX = 0; dwX < dwWidth; ++dwX)
						{
							dstLine[0] = (dwC == 0 ? srcLine[0] : dstLine[0]);
							dstLine[1] = (dwC == 0 ? srcLine[0] : dstLine[1]);
							dstLine[2] = (dwC == 0 ? srcLine[0] : dstLine[2]);
							dstLine[3] = (dwC == 0 ? 0xFF       : dstLine[3]);

							dstLine += 4;
							srcLine += 1;
						}
					}
				}
				else if (dwChannels == 2)
				{
					if (dwPhotometric == PHOTOMETRIC_SEPARATED)
					{
						for (uint32 dwX = 0; dwX < dwWidth; ++dwX)
						{
							// don't swap red and blue -> RGB(A), but convert CMY to RGB (PHOTOMETRIC_SEPARATED refers to inks in TIFF, the value is inverted)
							dstLine[0] = (dwC == 0 ? 0xFF - srcLine[0] : dstLine[0]);
							dstLine[1] = (dwC == 1 ? 0xFF - srcLine[1] : dstLine[1]);
							dstLine[2] = (dwC == 0 ? 0x00              : dstLine[2]);
							dstLine[3] = (dwC == 0 ? 0xFF              : dstLine[3]);

							dstLine += 4;
							srcLine += 1;
						}
					}
					else if (dwPhotometric == PHOTOMETRIC_MINISBLACK)
					{
						for (uint32 dwX = 0; dwX < dwWidth; ++dwX)
						{
							dstLine[0] = (dwC == 0 ? srcLine[0] : dstLine[0]);
							dstLine[1] = (dwC == 0 ? srcLine[0] : dstLine[1]);
							dstLine[2] = (dwC == 0 ? srcLine[0] : dstLine[2]);
							dstLine[3] = (dwC == 1 ? srcLine[1] : dstLine[3]);

							dstLine += 4;
							srcLine += 1;
						}
					}
				}
				else
				{
					for (uint32 dwX = 0; dwX < dwWidth; ++dwX)
					{
						// swap red and blue -> BGR(A)
						dstLine[0] = (dwC == 2 ? srcLine[2] : dstLine[0]);
						dstLine[1] = (dwC == 1 ? srcLine[1] : dstLine[1]);
						dstLine[2] = (dwC == 0 ? srcLine[0] : dstLine[2]);
						dstLine[3] = (dwC == 3 ? srcLine[3] : 0xFF);

						dstLine += 4;
						srcLine += 1;
					}
				}
			}

		}

		return pRet.release();
	}

	for (uint32 dwY = 0; dwY < dwHeight; ++dwY)
	{
		if (TIFFReadScanline(tif, &buf[0], dwY, 0) == -1)
		{
			RCLogError("%s failed (reading row %d)", __FUNCTION__, dwY);
			return 0;
		}

		const uint8* srcLine = (const uint8*)&buf[0];
		uint8* dstLine = (uint8*)&dst[dwPitch * dwY];

		if (dwChannels == 1)
		{
			if (dwPhotometric != PHOTOMETRIC_MINISBLACK)
			{
				for (uint32 dwX = 0; dwX < dwWidth; ++dwX)
				{
					dstLine[0] = srcLine[0];

					dstLine += 1;
					srcLine += dwChannels;
				}
			}
			else
			{
				for (uint32 dwX = 0; dwX < dwWidth; ++dwX)
				{
					dstLine[0] = srcLine[0];
					dstLine[1] = srcLine[0];
					dstLine[2] = srcLine[0];
					dstLine[3] = 0xFF;

					dstLine += 4;
					srcLine += dwChannels;
				}
			}
		}
		else if (dwChannels == 2)
		{
			if (dwPhotometric == PHOTOMETRIC_SEPARATED)
			{
				for (uint32 dwX = 0; dwX < dwWidth; ++dwX)
				{
					// swap red and blue -> BGR(A), and convert CMY to RGB (PHOTOMETRIC_SEPARATED refers to inks in TIFF, the value is inverted)
					dstLine[0] = 0x00;
					dstLine[1] = 0xFF - srcLine[1];
					dstLine[2] = 0xFF - srcLine[0];
					dstLine[3] = 0xFF;

					dstLine += 4;
					srcLine += dwChannels;
				}
			}
			else if (dwPhotometric == PHOTOMETRIC_MINISBLACK)
			{
				for (uint32 dwX = 0; dwX < dwWidth; ++dwX)
				{
					dstLine[0] = srcLine[0];
					dstLine[1] = srcLine[0];
					dstLine[2] = srcLine[0];
					dstLine[3] = srcLine[1];

					dstLine += 4;
					srcLine += dwChannels;
				}
			}
		}
		else
		{
			for (uint32 dwX = 0; dwX < dwWidth; ++dwX)
			{
				// swap red and blue -> BGR(A)
				dstLine[0] = srcLine[2];
				dstLine[1] = srcLine[1];
				dstLine[2] = srcLine[0];
				dstLine[3] = (dwChannels == 3) ? 0xFF : srcLine[3];

				dstLine += 4;
				srcLine += dwChannels;
			}
		}
	}

	return pRet.release();
}

static ImageObject* Load16BitImageFromTIFF(TIFF* tif)
{
	uint32 dwChannels = 0;
	uint32 dwPhotometric = 0;
	uint32 dwPlanar = 0;
	TIFFGetField(tif, TIFFTAG_SAMPLESPERPIXEL, &dwChannels);
	TIFFGetField(tif, TIFFTAG_PHOTOMETRIC, &dwPhotometric);
	TIFFGetField(tif, TIFFTAG_PLANARCONFIG, &dwPlanar);
	if (dwChannels != 1 && dwChannels != 2 && dwChannels != 3 && dwChannels != 4)
	{
		RCLogError("%s failed (channel count)", __FUNCTION__);
		return 0;
	}

	uint32 dwWidth = 0;
	uint32 dwHeight = 0;
	TIFFGetField(tif, TIFFTAG_IMAGEWIDTH, &dwWidth);
	TIFFGetField(tif, TIFFTAG_IMAGELENGTH, &dwHeight);
	if (dwWidth <= 0 || dwHeight <= 0)
	{
		RCLogError("%s failed (empty image)", __FUNCTION__);
		return 0;
	}

	EPixelFormat eFormat = ePixelFormat_A16B16G16R16;

	if (dwChannels == 1 && dwPhotometric != PHOTOMETRIC_MINISBLACK)
	{
		eFormat = ePixelFormat_R16;
	}
	else if (dwChannels == 4)
	{
		eFormat = ePixelFormat_A16B16G16R16;
	}

	std::auto_ptr<ImageObject> pRet(new ImageObject(dwWidth, dwHeight, 1, eFormat, ImageObject::eCubemap_UnknownYet));

	char* dst;
	uint32 dwPitch;
	pRet->GetImagePointer(0, dst, dwPitch);

	std::vector<char> buf(dwPitch);

	if (dwPlanar == PLANARCONFIG_SEPARATE)
	{
		for (uint32 dwC = 0; dwC < dwChannels; ++dwC)
		{
			for (uint32 dwY = 0; dwY < dwHeight; ++dwY)
			{
				if (TIFFReadScanline(tif, &buf[0], dwY, dwC) == -1)
				{
					RCLogError("%s failed (reading row %d, channel %d)", __FUNCTION__, dwY, dwC);
					return 0;
				}

				const uint16* srcLine = (const uint16*)&buf[0];
				uint16* dstLine = (uint16*)&dst[dwPitch * dwY];

				if (dwChannels == 1)
				{
					if (dwPhotometric != PHOTOMETRIC_MINISBLACK)
					{
						for (uint32 dwX = 0; dwX < dwWidth; ++dwX)
						{
							dstLine[0] = (dwC == 0 ? srcLine[0] : dstLine[0]);

							dstLine += 1;
							srcLine += 1;
						}
					}
					else
					{
						for (uint32 dwX = 0; dwX < dwWidth; ++dwX)
						{
							dstLine[0] = (dwC == 0 ? srcLine[0] : dstLine[0]);
							dstLine[1] = (dwC == 0 ? srcLine[0] : dstLine[1]);
							dstLine[2] = (dwC == 0 ? srcLine[0] : dstLine[2]);
							dstLine[3] = (dwC == 0 ? 0xFFFF     : dstLine[3]);

							dstLine += 4;
							srcLine += 1;
						}
					}
				}
				else if (dwChannels == 2)
				{
					if (dwPhotometric == PHOTOMETRIC_SEPARATED)
					{
						for (uint32 dwX = 0; dwX < dwWidth; ++dwX)
						{
							// don't swap red and blue -> RGB(A), but convert CMY to RGB (PHOTOMETRIC_SEPARATED refers to inks in TIFF, the value is inverted)
							dstLine[0] = (dwC == 0 ? 0xFFFF - srcLine[0] : dstLine[0]);
							dstLine[1] = (dwC == 1 ? 0xFFFF - srcLine[1] : dstLine[1]);
							dstLine[2] = (dwC == 0 ? 0x0000              : dstLine[2]);
							dstLine[3] = (dwC == 0 ? 0xFFFF              : dstLine[3]);

							dstLine += 4;
							srcLine += 1;
						}
					}
					else if (dwPhotometric == PHOTOMETRIC_MINISBLACK)
					{
						for (uint32 dwX = 0; dwX < dwWidth; ++dwX)
						{
							dstLine[0] = (dwC == 0 ? srcLine[0] : dstLine[0]);
							dstLine[1] = (dwC == 0 ? srcLine[0] : dstLine[1]);
							dstLine[2] = (dwC == 0 ? srcLine[0] : dstLine[2]);
							dstLine[3] = (dwC == 1 ? srcLine[1] : dstLine[3]);

							dstLine += 4;
							srcLine += 1;
						}
					}
				}
				else
				{
					for (uint32 dwX = 0; dwX < dwWidth; ++dwX)
					{
						// don't swap red and blue -> RGB(A)
						dstLine[0] = (dwC == 0 ? srcLine[0] : dstLine[0]);
						dstLine[1] = (dwC == 1 ? srcLine[1] : dstLine[1]);
						dstLine[2] = (dwC == 2 ? srcLine[2] : dstLine[2]);
						dstLine[3] = (dwC == 3 ? srcLine[3] : 0xFFFF);

						dstLine += 4;
						srcLine += 1;
					}
				}
			}

		}

		return pRet.release();
	}

	for (uint32 dwY = 0; dwY < dwHeight; ++dwY)
	{
		if (TIFFReadScanline(tif, &buf[0], dwY, 0) == -1)
		{
			RCLogError("%s failed (reading row %d)", __FUNCTION__, dwY);
			return 0;
		}

		const uint16* srcLine = (const uint16*)&buf[0];
		uint16* dstLine = (uint16*)&dst[dwPitch * dwY];

		if (dwChannels == 1)
		{
			if (dwPhotometric != PHOTOMETRIC_MINISBLACK)
			{
				for (uint32 dwX = 0; dwX < dwWidth; ++dwX)
				{
					dstLine[0] = srcLine[0];

					dstLine += 1;
					srcLine += dwChannels;
				}
			}
			else
			{
				for (uint32 dwX = 0; dwX < dwWidth; ++dwX)
				{
					dstLine[0] = srcLine[0];
					dstLine[1] = srcLine[0];
					dstLine[2] = srcLine[0];
					dstLine[3] = 0xFFFF;

					dstLine += 4;
					srcLine += dwChannels;
				}
			}
		}
		else if (dwChannels == 2)
		{
			if (dwPhotometric == PHOTOMETRIC_SEPARATED)
			{
				for (uint32 dwX = 0; dwX < dwWidth; ++dwX)
				{
					// don't swap red and blue -> RGB(A), but convert CMY to RGB (PHOTOMETRIC_SEPARATED refers to inks in TIFF, the value is inverted)
					dstLine[0] = 0xFFFF - srcLine[0];
					dstLine[1] = 0xFFFF - srcLine[1];
					dstLine[2] = 0x0000;
					dstLine[3] = 0xFFFF;

					dstLine += 4;
					srcLine += dwChannels;
				}
			}
			else if (dwPhotometric == PHOTOMETRIC_MINISBLACK)
			{
				for (uint32 dwX = 0; dwX < dwWidth; ++dwX)
				{
					dstLine[0] = srcLine[0];
					dstLine[1] = srcLine[0];
					dstLine[2] = srcLine[0];
					dstLine[3] = srcLine[1];

					dstLine += 4;
					srcLine += dwChannels;
				}
			}
		}
		else
		{
			for (uint32 dwX = 0; dwX < dwWidth; ++dwX)
			{
				// don't swap red and blue -> RGB(A)
				dstLine[0] = srcLine[0];
				dstLine[1] = srcLine[1];
				dstLine[2] = srcLine[2];
				dstLine[3] = (dwChannels == 3) ? 0xFFFF : srcLine[3];

				dstLine += 4;
				srcLine += dwChannels;
			}
		}
	}

	return pRet.release();
}

static ImageObject* Load16BitHDRImageFromTIFF(TIFF* tif)
{
	uint32 dwChannels = 0;
	uint32 dwPhotometric = 0;
	uint32 dwPlanar = 0;
	TIFFGetField(tif, TIFFTAG_SAMPLESPERPIXEL, &dwChannels);
	TIFFGetField(tif, TIFFTAG_PHOTOMETRIC, &dwPhotometric);
	TIFFGetField(tif, TIFFTAG_PLANARCONFIG, &dwPlanar);
	if (dwChannels != 1 && dwChannels != 2 && dwChannels != 3 && dwChannels != 4)
	{
		RCLogError("%s failed (channel count)", __FUNCTION__);
		return 0;
	}

	uint32 dwWidth = 0;
	uint32 dwHeight = 0;
	TIFFGetField(tif, TIFFTAG_IMAGEWIDTH, &dwWidth);
	TIFFGetField(tif, TIFFTAG_IMAGELENGTH, &dwHeight);
	if (dwWidth <= 0 || dwHeight <= 0)
	{
		RCLogError("%s failed (empty image)", __FUNCTION__);
		return 0;
	}

	EPixelFormat eFormat = ePixelFormat_A16B16G16R16F;

	if (dwChannels == 1 && dwPhotometric != PHOTOMETRIC_MINISBLACK)
	{
		eFormat = ePixelFormat_R16F;
	}
	else if (dwChannels == 4)
	{
		eFormat = ePixelFormat_A16B16G16R16F;
	}

	std::auto_ptr<ImageObject> pRet(new ImageObject(dwWidth, dwHeight, 1, eFormat, ImageObject::eCubemap_UnknownYet));

	char* dst;
	uint32 dwPitch;
	pRet->GetImagePointer(0,dst,dwPitch);

	std::vector<char> buf(dwPitch);

	static const SHalf zero = SHalf(0.0f);
	static const SHalf one = SHalf(1.0f);

	if (dwPlanar == PLANARCONFIG_SEPARATE)
	{
		for (uint32 dwC = 0; dwC < dwChannels; ++dwC)
		{
			for (uint32 dwY = 0; dwY < dwHeight; ++dwY)
			{
				if (TIFFReadScanline(tif, &buf[0], dwY, dwC) == -1)
				{
					RCLogError("%s failed (reading row %d, channel %d)", __FUNCTION__, dwY, dwC);
					return 0;
				}

				const SHalf* srcLine = (const SHalf*)&buf[0];
				SHalf* dstLine = (SHalf*)&dst[dwPitch * dwY];

				if (dwChannels == 1)
				{
					if (dwPhotometric != PHOTOMETRIC_MINISBLACK)
					{
						for (uint32 dwX = 0; dwX < dwWidth; ++dwX)
						{
							dstLine[0] = (dwC == 0 ? srcLine[0] : dstLine[0]);

							dstLine += 1;
							srcLine += 1;
						}
					}
					else
					{
						for (uint32 dwX = 0; dwX < dwWidth; ++dwX)
						{
							dstLine[0] = (dwC == 0 ? srcLine[0] : dstLine[0]);
							dstLine[1] = (dwC == 0 ? srcLine[0] : dstLine[1]);
							dstLine[2] = (dwC == 0 ? srcLine[0] : dstLine[2]);
							dstLine[3] = (dwC == 0 ? one        : dstLine[3]);

							dstLine += 4;
							srcLine += 1;
						}
					}
				}
				else if (dwChannels == 2)
				{
					if (dwPhotometric == PHOTOMETRIC_SEPARATED)
					{
						for (uint32 dwX = 0; dwX < dwWidth; ++dwX)
						{
							// don't swap red and blue -> RGB(A), but convert CMY to RGB (PHOTOMETRIC_SEPARATED refers to inks in TIFF, the value is inverted)
							dstLine[0] = (dwC == 0 ? SHalf(one - srcLine[0]) : dstLine[0]);
							dstLine[1] = (dwC == 1 ? SHalf(one - srcLine[1]) : dstLine[1]);
							dstLine[2] = (dwC == 0 ? zero : dstLine[2]);
							dstLine[3] = (dwC == 0 ? one  : dstLine[3]);

							dstLine += 4;
							srcLine += 1;
						}
					}
					else if (dwPhotometric == PHOTOMETRIC_MINISBLACK)
					{
						for (uint32 dwX = 0; dwX < dwWidth; ++dwX)
						{
							dstLine[0] = (dwC == 0 ? srcLine[0] : dstLine[0]);
							dstLine[1] = (dwC == 0 ? srcLine[0] : dstLine[1]);
							dstLine[2] = (dwC == 0 ? srcLine[0] : dstLine[2]);
							dstLine[3] = (dwC == 1 ? srcLine[1] : dstLine[3]);

							dstLine += 4;
							srcLine += 1;
						}
					}
				}
				else
				{
					for (uint32 dwX = 0; dwX < dwWidth; ++dwX)
					{
						// don't swap red and blue -> RGB(A)
						dstLine[0] = (dwC == 0 ? srcLine[0] : dstLine[0]);
						dstLine[1] = (dwC == 1 ? srcLine[1] : dstLine[1]);
						dstLine[2] = (dwC == 2 ? srcLine[2] : dstLine[2]);
						dstLine[3] = (dwC == 3 ? srcLine[3] : one);

						dstLine += 4;
						srcLine += 1;
					}
				}
			}

		}

		return pRet.release();
	}

	for (uint32 dwY = 0; dwY < dwHeight; ++dwY)
	{
		if (TIFFReadScanline(tif, &buf[0], dwY, 0) == -1)
		{
			RCLogError("%s failed (reading row %d)", __FUNCTION__, dwY);
			return 0;
		}

		const SHalf* srcLine = (const SHalf*)&buf[0];
		SHalf* dstLine = (SHalf*)&dst[dwPitch * dwY];

		if (dwChannels == 1)
		{
			if (dwPhotometric != PHOTOMETRIC_MINISBLACK)
			{
				for (uint32 dwX = 0; dwX < dwWidth; ++dwX)
				{
					dstLine[0] = srcLine[0];

					dstLine += 1;
					srcLine += dwChannels;
				}
			}
			else
			{
				for (uint32 dwX = 0; dwX < dwWidth; ++dwX)
				{
					dstLine[0] = srcLine[0];
					dstLine[1] = srcLine[0];
					dstLine[2] = srcLine[0];
					dstLine[3] = one;

					dstLine += 4;
					srcLine += dwChannels;
				}
			}
		}
		else if (dwChannels == 2)
		{
			if (dwPhotometric == PHOTOMETRIC_SEPARATED)
			{
				for (uint32 dwX = 0; dwX < dwWidth; ++dwX)
				{
					// don't swap red and blue -> RGB(A), but convert CMY to RGB (PHOTOMETRIC_SEPARATED refers to inks in TIFF, the value is inverted)
					dstLine[0] = SHalf(1.0f - srcLine[0]);
					dstLine[1] = SHalf(1.0f - srcLine[1]);
					dstLine[2] = zero;
					dstLine[3] = one;

					dstLine += 4;
					srcLine += dwChannels;
				}
			}
			else if (dwPhotometric == PHOTOMETRIC_MINISBLACK)
			{
				for (uint32 dwX = 0; dwX < dwWidth; ++dwX)
				{
					dstLine[0] = srcLine[0];
					dstLine[1] = srcLine[0];
					dstLine[2] = srcLine[0];
					dstLine[3] = srcLine[1];

					dstLine += 4;
					srcLine += dwChannels;
				}
			}
		}
		else
		{
			for (uint32 dwX = 0; dwX < dwWidth; ++dwX)
			{
				// don't swap red and blue -> RGB(A)
				dstLine[0] = srcLine[0];
				dstLine[1] = srcLine[1];
				dstLine[2] = srcLine[2];
				dstLine[3] = (dwChannels == 3) ? one : srcLine[3];

				dstLine += 4;
				srcLine += dwChannels;
			}
		}
	}

	return pRet.release();
}

static ImageObject* Load32BitHDRImageFromTIFF(TIFF* tif)
{
	uint32 dwChannels = 0;
	uint32 dwPhotometric = 0;
	uint32 dwPlanar = 0;
	TIFFGetField(tif, TIFFTAG_SAMPLESPERPIXEL, &dwChannels);
	TIFFGetField(tif, TIFFTAG_PHOTOMETRIC, &dwPhotometric);
	TIFFGetField(tif, TIFFTAG_PLANARCONFIG, &dwPlanar);
	if (dwChannels != 1 && dwChannels != 2 && dwChannels != 3 && dwChannels != 4)
	{
		RCLogError("%s failed (channel count)", __FUNCTION__);
		return 0;
	}

	uint32 dwWidth = 0;
	uint32 dwHeight = 0;
	TIFFGetField(tif, TIFFTAG_IMAGEWIDTH, &dwWidth);
	TIFFGetField(tif, TIFFTAG_IMAGELENGTH, &dwHeight);
	if (dwWidth <= 0 || dwHeight <= 0)
	{
		RCLogError("%s failed (empty image)", __FUNCTION__);
		return 0;
	}

	EPixelFormat eFormat = ePixelFormat_A32B32G32R32F;

	if (dwChannels == 1 && dwPhotometric != PHOTOMETRIC_MINISBLACK)
	{
		eFormat = ePixelFormat_R32F;
	}
	else if (dwChannels == 4)
	{
		eFormat = ePixelFormat_A32B32G32R32F;
	}

	std::auto_ptr<ImageObject> pRet(new ImageObject(dwWidth, dwHeight, 1, eFormat, ImageObject::eCubemap_UnknownYet));

	char* dst;
	uint32 dwPitch;
	pRet->GetImagePointer(0,dst,dwPitch);

	std::vector<char> buf(dwPitch);

	if (dwPlanar == PLANARCONFIG_SEPARATE)
	{
		for (uint32 dwC = 0; dwC < dwChannels; ++dwC)
		{
			for (uint32 dwY = 0; dwY < dwHeight; ++dwY)
			{
				if (TIFFReadScanline(tif, &buf[0], dwY, dwC) == -1)
				{
					RCLogError("%s failed (reading row %d, channel %d)", __FUNCTION__, dwY, dwC);
					return 0;
				}

				const float* srcLine = (const float*)&buf[0];
				float* dstLine = (float*)&dst[dwPitch * dwY];

				if (dwChannels == 1)
				{
					if (dwPhotometric != PHOTOMETRIC_MINISBLACK)
					{
						for (uint32 dwX = 0; dwX < dwWidth; ++dwX)
						{
							dstLine[0] = (dwC == 0 ? srcLine[0] : dstLine[0]);

							dstLine += 1;
							srcLine += 1;
						}
					}
					else
					{
						for (uint32 dwX = 0; dwX < dwWidth; ++dwX)
						{
							dstLine[0] = (dwC == 0 ? srcLine[0] : dstLine[0]);
							dstLine[1] = (dwC == 0 ? srcLine[0] : dstLine[1]);
							dstLine[2] = (dwC == 0 ? srcLine[0] : dstLine[2]);
							dstLine[3] = (dwC == 0 ? 1.0f       : dstLine[3]);

							dstLine += 4;
							srcLine += 1;
						}
					}
				}
				else if (dwChannels == 2)
				{
					if (dwPhotometric == PHOTOMETRIC_SEPARATED)
					{
						for (uint32 dwX = 0; dwX < dwWidth; ++dwX)
						{
							// don't swap red and blue -> RGB(A), but convert CMY to RGB (PHOTOMETRIC_SEPARATED refers to inks in TIFF, the value is inverted)
							dstLine[0] = (dwC == 0 ? 1.0f - srcLine[0] : dstLine[0]);
							dstLine[1] = (dwC == 1 ? 1.0f - srcLine[1] : dstLine[1]);
							dstLine[2] = (dwC == 0 ? 0.0f              : dstLine[2]);
							dstLine[3] = (dwC == 0 ? 1.0f              : dstLine[3]);

							dstLine += 4;
							srcLine += 1;
						}
					}
					else if (dwPhotometric == PHOTOMETRIC_MINISBLACK)
					{
						for (uint32 dwX = 0; dwX < dwWidth; ++dwX)
						{
							dstLine[0] = (dwC == 0 ? srcLine[0] : dstLine[0]);
							dstLine[1] = (dwC == 0 ? srcLine[0] : dstLine[1]);
							dstLine[2] = (dwC == 0 ? srcLine[0] : dstLine[2]);
							dstLine[3] = (dwC == 1 ? srcLine[1] : dstLine[3]);

							dstLine += 4;
							srcLine += 1;
						}
					}
				}
				else
				{
					for (uint32 dwX = 0; dwX < dwWidth; ++dwX)
					{
						// don't swap red and blue -> RGB(A)
						dstLine[0] = (dwC == 0 ? srcLine[0] : dstLine[0]);
						dstLine[1] = (dwC == 1 ? srcLine[1] : dstLine[1]);
						dstLine[2] = (dwC == 2 ? srcLine[2] : dstLine[2]);
						dstLine[3] = (dwC == 3 ? srcLine[3] : 1.0f);

						dstLine += 4;
						srcLine += 1;
					}
				}
			}

		}

		return pRet.release();
	}

	for (uint32 dwY = 0; dwY < dwHeight; ++dwY)
	{
		if (TIFFReadScanline(tif, &buf[0], dwY, 0) == -1)
		{
			RCLogError("%s failed (reading row %d)", __FUNCTION__, dwY);
			return 0;
		}

		const float* srcLine = (const float*)&buf[0];
		float* dstLine = (float*)&dst[dwPitch * dwY];

		if (dwChannels == 1)
		{
			if (dwPhotometric != PHOTOMETRIC_MINISBLACK)
			{
				for (uint32 dwX = 0; dwX < dwWidth; ++dwX)
				{
					// clamp negative values
					const float v = Util::getMax(srcLine[0], 0.0f);
					dstLine[0] = v;

					++dstLine;
					++srcLine;
				}
			}
			else
			{
				for (uint32 dwX = 0; dwX < dwWidth; ++dwX)
				{
					// clamp negative values
					const float v = Util::getMax(srcLine[0], 0.0f);
					dstLine[0] = v;
					dstLine[1] = v;
					dstLine[2] = v;
					dstLine[3] = 1.0f;

					dstLine += 4;
					++srcLine;
				}
			}
		}
		else if (dwChannels == 2)
		{
			if (dwPhotometric == PHOTOMETRIC_SEPARATED)
			{
				for (uint32 dwX = 0; dwX < dwWidth; ++dwX)
				{
					// don't swap red and blue -> RGB(A), but convert CMY to RGB (PHOTOMETRIC_SEPARATED refers to inks in TIFF, the value is inverted)
					dstLine[0] = 1.0f - Util::getMax(srcLine[0], 0.0f);
					dstLine[1] = 1.0f - Util::getMax(srcLine[1], 0.0f);
					dstLine[2] = 0.0f;
					dstLine[3] = 1.0f;

					dstLine += 4;
					srcLine += dwChannels;
				}
			}
			else if (dwPhotometric == PHOTOMETRIC_MINISBLACK)
			{
				for (uint32 dwX = 0; dwX < dwWidth; ++dwX)
				{
					const float v = Util::getMax(srcLine[0], 0.0f);
					const float g = Util::getMax(srcLine[1], 0.0f);
					dstLine[0] = v;
					dstLine[1] = v;
					dstLine[2] = v;
					dstLine[3] = g;

					dstLine += 4;
					srcLine += dwChannels;
				}
			}
		}
		else
		{
			for (uint32 dwX = 0; dwX < dwWidth; ++dwX)
			{
				// clamp negative values; don't swap red and blue -> RGB(A) 
				dstLine[0] = Util::getMax(srcLine[0], 0.0f);
				dstLine[1] = Util::getMax(srcLine[1], 0.0f);
				dstLine[2] = Util::getMax(srcLine[2], 0.0f);
				dstLine[3] = (dwChannels == 3) ? 1.0f : Util::getMax(srcLine[3], 0.0f);

				dstLine += 4;
				srcLine += dwChannels;
			}
		}
	}

	return pRet.release();
}

///////////////////////////////////////////////////////////////////////////////////

static void GetTiffFieldData(std::vector<char>& res, TIFF* tif_in, ttag_t tag)
{
	res.clear();

	uint32 size = 0;
	void* pData = 0;
	TIFFGetField(tif_in, tag, &size, &pData);

	if (size > 0)
	{
		res.assign((const char*)pData, ((const char*)pData) + size);
	}
}

static bool UpdateAndSaveSettingsToTIF(const char *filenameRead, const char *filenameWrite, const std::vector<char>& photoshopData, bool isCryTif2012)
{
	assert(filenameRead && filenameRead[0]);
	assert(filenameWrite && filenameWrite[0]);

	wstring widePath;
	Unicode::Convert(widePath, filenameRead);

	TIFF* tif_in = TIFFOpenW(widePath.c_str(), "r");	
	if (!tif_in) 
	{			
		return false;
	}

	uint32 dwWidth = 0;
	uint32 dwHeight = 0;
	uint32 dwPlanes = 0;
	uint32 dwBitsPerSample = 0;
	uint32 dwRowsPerStrip = 0;
	uint32 dwCompression = 0;
	uint32 dwPhotometric = 0;
	uint32 dwPlanarConfig = 0;
	uint32 dwPredictor = 0;
	uint32 dwOrientation = 0;
	uint32 dwSampleFormat = 0;
	char filename[1024] = { 0 };
	std::vector<char> icc;

	TIFFGetField(tif_in, TIFFTAG_IMAGEWIDTH, &dwWidth);
	TIFFGetField(tif_in, TIFFTAG_IMAGELENGTH, &dwHeight);
	TIFFGetField(tif_in, TIFFTAG_SAMPLESPERPIXEL, &dwPlanes);
	TIFFGetField(tif_in, TIFFTAG_BITSPERSAMPLE, &dwBitsPerSample ); 
	TIFFGetField(tif_in, TIFFTAG_ROWSPERSTRIP, &dwRowsPerStrip ); 
	TIFFGetField(tif_in, TIFFTAG_COMPRESSION, &dwCompression);
	TIFFGetField(tif_in, TIFFTAG_PHOTOMETRIC, &dwPhotometric );
	TIFFGetField(tif_in, TIFFTAG_PLANARCONFIG, &dwPlanarConfig );
	TIFFGetField(tif_in, TIFFTAG_PREDICTOR, &dwPredictor );
	TIFFGetField(tif_in, TIFFTAG_ORIENTATION, &dwOrientation );
	TIFFGetField(tif_in, TIFFTAG_SAMPLEFORMAT, &dwSampleFormat);
	if (IsFloatTif(tif_in, isCryTif2012))
	{
		dwSampleFormat = SAMPLEFORMAT_IEEEFP;
	}
	{
		char* pFilename = 0;
		TIFFGetField(tif_in, TIFFTAG_IMAGEDESCRIPTION, &pFilename);
		if (pFilename)
		{
			cry_sprintf(filename, "%s", pFilename);
		}
	}
	GetTiffFieldData(icc, tif_in, TIFFTAG_ICCPROFILE);

	if ((dwPlanes != 1 && dwPlanes != 2 && dwPlanes != 3 && dwPlanes != 4) ||
		(dwBitsPerSample != 8 && dwBitsPerSample != 16 && dwBitsPerSample != 32)) 
	{
		TIFFClose(tif_in);
		return false;
	}

	const uint32 dwStrips = TIFFNumberOfStrips(tif_in);
	// Important: in case of zlib compression strips can be bigger than TIFFStripSize(),
	// see the comments in the reading loop below.
	std::vector<char> stripBuffer(TIFFStripSize(tif_in) + 2);
	std::vector<std::vector<char>> strips(dwStrips);

	// load pixels
	for (uint32 strip = 0; strip < dwStrips; ++strip)
	{
		for (;;) 
		{
			const tmsize_t sz = TIFFReadRawStrip(tif_in, strip, stripBuffer.data(), stripBuffer.size());
			if (sz < 0)
			{
				TIFFClose(tif_in);
				return false;
			}
			if (sz == stripBuffer.size())
			{
				// TIFFReadRawStrip() quietly reads not more bytes than requested, so let's
				// make sure that the whole strip is read.
				stripBuffer.resize(stripBuffer.size() * 2);
				continue;
			}

			strips[strip].assign(stripBuffer.begin(), stripBuffer.begin() + sz);
			break;
		}
	}

	TIFFClose(tif_in);
	tif_in = 0;

	// write image with resource compiler settings

	Unicode::Convert(widePath, filenameWrite);

	TIFF* const tif_out = TIFFOpenW(widePath.c_str(), "w");
	if (!tif_out)
	{
		return false;
	}

	TIFFSetField(tif_out, TIFFTAG_IMAGEWIDTH, dwWidth);
	TIFFSetField(tif_out, TIFFTAG_IMAGELENGTH, dwHeight);
	TIFFSetField(tif_out, TIFFTAG_SAMPLESPERPIXEL, dwPlanes);
	TIFFSetField(tif_out, TIFFTAG_BITSPERSAMPLE, dwBitsPerSample ); 
	TIFFSetField(tif_out, TIFFTAG_ROWSPERSTRIP, dwRowsPerStrip); 
	if (dwCompression != 0)
	{
		TIFFSetField(tif_out, TIFFTAG_COMPRESSION, dwCompression);
	}
	TIFFSetField(tif_out, TIFFTAG_PHOTOMETRIC, dwPhotometric);
	if (dwPlanarConfig != 0)
	{
		TIFFSetField(tif_out, TIFFTAG_PLANARCONFIG, dwPlanarConfig);
	}
	if (dwPredictor != 0)
	{
		TIFFSetField(tif_out, TIFFTAG_PREDICTOR, dwPredictor);
	}
	if (dwOrientation != 0)
	{
		TIFFSetField(tif_out, TIFFTAG_ORIENTATION, dwOrientation);
	}
	if (dwSampleFormat != 0)
	{
		TIFFSetField(tif_out, TIFFTAG_SAMPLEFORMAT, dwSampleFormat);
	}
	if (filename[0])
	{
		TIFFSetField(tif_out, TIFFTAG_IMAGEDESCRIPTION, filename);
	}
	if (!icc.empty())
	{
		TIFFSetField(tif_out, TIFFTAG_ICCPROFILE, icc.size(), &icc[0]);
	}

	// set resource compiler settings
	assert(!photoshopData.empty());
	TIFFSetField(tif_out, TIFFTAG_PHOTOSHOP, (uint32)photoshopData.size(), (const uchar*)&photoshopData[0]);  // 34377 IPTC TAG

	// write pixels
	for (uint32 strip = 0; strip < dwStrips; ++strip)
	{
		const tmsize_t written = (uint32)TIFFWriteRawStrip(tif_out, strip, strips[strip].data(), strips[strip].size());
		if (written != strips[strip].size())
		{
			TIFFClose(tif_out);
			return false;
		}
	}

	TIFFClose(tif_out);
	return true;
}

///////////////////////////////////////////////////////////////////////////////////

namespace
{
class CryTifConfigSink
	: public IConfigSink
{
public:
	CryTifConfigSink()
	{
	}

	~CryTifConfigSink()
	{
	}

	// interface IConfigSink -------------------------------------

	virtual void SetKeyValue(EConfigPriority ePri, const char* key, const char* value)
	{
		assert(ePri == eCP_PriorityFile);

		// We skip deprecated/obsolete keys to avoid having too long
		// and/or polluted config strings. 
		// Note that Photoshop silently truncates config strings
		// to 256 characters in length so this skipping helps us
		// to avoid hitting this limit.
		static const char* const keysBlackList[] =
		{
			"dither",		// obsolete
			"m0",			// obsolete
			"m1",			// obsolete
			"m2",			// obsolete
			"m3",			// obsolete
			"m4",			// obsolete
			"m5",			// obsolete
			"mipgamma",		// obsolete
			"pixelformat",  // no need to store it in file, it's read from preset only
		};
		for (size_t i = 0; i < sizeof(keysBlackList) / sizeof(keysBlackList[0]); ++i)
		{
			if (stricmp(key, keysBlackList[i]) == 0)
			{
				return;
			}
		}

		// determine if we need to use quotes around the value
		bool bQuotes = false;        
		{
			const char* p = value;
			while (*p)
			{
				if (!IConfig::IsValidNameChar(*p))
				{
					bQuotes = true;
					break;
				}
				++p;
			}
		}

		const string keyValue =
			bQuotes
			? string("/") + key + "=\"" + value + "\""
			: string("/") + key + "=" + value;

		// We skip keys with default values to avoid having too long
		// config strings. 
		// Note that Photoshop silently truncates config strings
		// to 255 characters in length so this skipping helps us
		// to avoid hitting this limit.
		for (int i = 0; ; ++i)
		{
			const char* const defaultProp = CImageProperties::GetDefaultProperty(i);
			if (!defaultProp)
			{
				break;
			}
			if (stricmp(keyValue.c_str() + 1, defaultProp) == 0)  // '+ 1' because defaultProp doesn't have "/" in the beginning
			{
				return;
			}
		}

		if (!m_result.empty())
		{
			m_result += " ";
		}
		m_result += keyValue;
	}
	//------------------------------------------------------------

	const string& GetAsString() const
	{
		return m_result;
	}

private:
	string m_result;
};
}

static string SerializePropertiesToSettings(const CImageProperties* pProps)
{
	CryTifConfigSink sink;
	pProps->GetMultiConfig().CopyToConfig(eCP_PriorityFile, &sink);

	// Photoshop silently truncates CryTIF settings to 255 characters, so
	// let's warn (actually error) the user if this limit is reached.
	// Note that we don't truncate the settings here if the limit
	// is reached - there is no reason to do so.
	const int maxLen = 255;
	if ((int)sink.GetAsString().length() > maxLen)
	{
		RCLogError("CryTIF settings are too long: %d characters (max is %d characters)", (int)sink.GetAsString().length(), maxLen);
	}

	return sink.GetAsString();
}

///////////////////////////////////////////////////////////////////////////////////

bool UpdateAndSaveSettingsToTIF(const char *filenameRead, const char *filenameWrite, const string& settings, bool isCryTif2012)
{
	std::vector<char> data;
	FormPhotoshopDataBlock(data, settings);

	if (!UpdateAndSaveSettingsToTIF(filenameRead, filenameWrite, data, isCryTif2012))
	{
		RCLogError("Failed to save settings to %s", filenameWrite);
		return false;
	}

	return true;
}


bool UpdateAndSaveSettingsToTIF(const char *filenameRead, const char *filenameWrite, const CImageProperties* pProps, const string* pOriginalSettings, bool bLogSettings)
{
	const string settings = SerializePropertiesToSettings(pProps);

	if (bLogSettings)
	{
		RCLog("Saving settings:");
		RCLog("  '%s'", settings.c_str());
	}

	if (pOriginalSettings && StringHelpers::Equals(*pOriginalSettings, settings))
	{
		RCLog("New settings are equal to existing settings - file writing is skipped.");
		return true;
	}

	const bool isCryTIF2012 = pProps && pProps->GetConfigAsBool("CryTIF2012", false, true);

	return UpdateAndSaveSettingsToTIF(filenameRead, filenameWrite, settings, isCryTIF2012);
}

///////////////////////////////////////////////////////////////////////////////////

// saves 8bit uint to simple RAWImage tiff source raster
static void Save8BitImageToTIFF(struct tiff* tif, const ImageObject* pImageObject);
// saves 16bit uint to simple RAWImage tiff source raster
static void Save16BitImageToTIFF(struct tiff* tif, const ImageObject* pImageObject);
// saves 16f HDR to simple FloatImage tiff source raster
static void Save16BitHDRImageToTIFF(struct tiff* tif, const ImageObject* pImageObject);
// saves 32f HDR to simple FloatImage tiff source raster
static void Save32BitHDRImageToTIFF(struct tiff* tif, const ImageObject* pImageObject);

static bool SaveByUsingTIFFSaver(const char *filenameWrite, const string& settings, const ImageObject* pImageObject)
{
	uint32 dwWidth, dwHeight, dwMips;
	pImageObject->GetExtent(dwWidth, dwHeight, dwMips);

	wstring widePath;
	Unicode::Convert(widePath, filenameWrite);

	TIFF* const pTiffFile = TIFFOpenW(widePath.c_str(), "w");
	if (!pTiffFile) 
	{
		return false;
	}

	{
		const EPixelFormat pixelFormat = pImageObject->GetPixelFormat();
		const PixelFormatInfo* const info = CPixelFormats::GetPixelFormatInfo(pixelFormat);

		assert(!info->bCompressed);

		const uint32 dwPlanes = info->nChannels;
		const uint32 dwBitsPerSample = CPixelFormats::GetBitsPerPixel(pixelFormat) / info->nChannels;

		// We need to set some values for basic tags before we can add any data
		TIFFSetField(pTiffFile, TIFFTAG_IMAGEWIDTH, dwWidth);
		TIFFSetField(pTiffFile, TIFFTAG_IMAGELENGTH, dwHeight);	
		TIFFSetField(pTiffFile, TIFFTAG_SAMPLESPERPIXEL, dwPlanes);
		TIFFSetField(pTiffFile, TIFFTAG_BITSPERSAMPLE, dwBitsPerSample); 
		TIFFSetField(pTiffFile, TIFFTAG_PHOTOMETRIC, (dwPlanes == 1 ? PHOTOMETRIC_MINISBLACK : (dwPlanes == 2 ? PHOTOMETRIC_SEPARATED : PHOTOMETRIC_RGB)));
		TIFFSetField(pTiffFile, TIFFTAG_ORIENTATION, ORIENTATION_TOPLEFT);
#ifdef CRYTIF_DISABLE_COMPRESSION
		TIFFSetField(pTiffFile, TIFFTAG_COMPRESSION, COMPRESSION_NONE);
		TIFFSetField(pTiffFile, TIFFTAG_PLANARCONFIG, PLANARCONFIG_CONTIG);
		TIFFSetField(pTiffFile, TIFFTAG_PREDICTOR, PREDICTOR_NONE);
		TIFFSetField(pTiffFile, TIFFTAG_ROWSPERSTRIP, 1); 
#else
		TIFFSetField(pTiffFile, TIFFTAG_COMPRESSION, COMPRESSION_DEFLATE);
		TIFFSetField(pTiffFile, TIFFTAG_PLANARCONFIG, PLANARCONFIG_CONTIG);
		TIFFSetField(pTiffFile, TIFFTAG_PREDICTOR, PREDICTOR_HORIZONTAL);

		tsize_t dstLineSize = dwWidth * (dwBitsPerSample / 8);
		tsize_t dstPitch = dstLineSize * dwPlanes;
		uint32 dstNumRowsPerStrip = 16 * uint32((32768 + dstPitch - 1) / dstPitch);

		dstNumRowsPerStrip = TIFFDefaultStripSize(pTiffFile, dstNumRowsPerStrip);
		TIFFSetField(pTiffFile, TIFFTAG_ROWSPERSTRIP, dstNumRowsPerStrip); 
#endif // CRYTIF_DISABLE_COMPRESSION

		// CryEngine expects a "source DCC filename" stored TIFFTAG_IMAGEDESCRIPTION,
		// but I have no idea what name should we write, so write empty string
		TIFFSetField(pTiffFile, TIFFTAG_IMAGEDESCRIPTION, "");

		// update meta data without re-opening the file again, as 
		// this causes problems with file-change listeners locking onto
		// the file between the close and the subsequent update
		if (!settings.empty())
		{
			std::vector<char> data;
			FormPhotoshopDataBlock(data, settings);

			// set resource compiler settings
			assert(!data.empty());
			TIFFSetField(pTiffFile, TIFFTAG_PHOTOSHOP, (uint32)data.size(), (const uchar*)&data[0]);  // 34377 IPTC TAG
		}

		if (info->eSampleType == eSampleType_Float)
		{
			Save32BitHDRImageToTIFF(pTiffFile, pImageObject);
		}
		else if (info->eSampleType == eSampleType_Half)
		{
			Save16BitHDRImageToTIFF(pTiffFile, pImageObject);
		}
		else if (info->eSampleType == eSampleType_Uint16)
		{
			Save16BitImageToTIFF(pTiffFile, pImageObject);
		}
		else if (info->eSampleType == eSampleType_Uint8)
		{
			Save8BitImageToTIFF(pTiffFile, pImageObject);
		}
	}

	TIFFClose(pTiffFile);

	return true;
}

bool SaveByUsingTIFFSaver(const char *filenameWrite, const CImageProperties* pProps, const ImageObject* pImageObject)
{
	assert(filenameWrite);
	assert(pImageObject);

	ImageToProcess image(pImageObject->CopyImage());

	EPixelFormat fmt = pImageObject->GetPixelFormat();
	if (fmt != ePixelFormat_A32B32G32R32F &&
	    fmt != ePixelFormat_G32R32F &&
	    fmt != ePixelFormat_R32F &&
	    fmt != ePixelFormat_A16B16G16R16F &&
	    fmt != ePixelFormat_G16R16F &&
	    fmt != ePixelFormat_R16F &&
	    fmt != ePixelFormat_A16B16G16R16 &&
	    fmt != ePixelFormat_G16R16 &&
	    fmt != ePixelFormat_R16 &&
	    fmt != ePixelFormat_X8R8G8B8 &&
	    fmt != ePixelFormat_A8R8G8B8 &&
	    fmt != ePixelFormat_A8L8 &&
	    fmt != ePixelFormat_A8)
	{
		// TODO: add support for converting all types of other formats to best precision instead of converting to 8bit!
		image.ConvertFormat(pProps, ePixelFormat_A8R8G8B8);
	}

	string settings;
	if (pProps != 0)
	{
		settings = SerializePropertiesToSettings(pProps);
	}

	return SaveByUsingTIFFSaver(filenameWrite, settings, image.get());
}

static void Save8BitImageToTIFF(struct tiff* tif, const ImageObject* pImageObject)
{
	TIFFSetField(tif, TIFFTAG_BITSPERSAMPLE, 8);

	int dwPlanes;
	CPixelFormats::GetPixelFormatInfo(pImageObject->GetPixelFormat(), 0, &dwPlanes, 0);

	uint32 dwWidth, dwHeight, dwMips;
	pImageObject->GetExtent(dwWidth, dwHeight, dwMips);

	char* pInputMem;
	uint32 dwInputPitch;		
	pImageObject->GetImagePointer(0, pInputMem, dwInputPitch);

	std::vector<char> raster(dwWidth * dwPlanes);
	for (uint32 row = 0; row < dwHeight; ++row)
	{
		char* pDst = (char*)&raster[0];
		const char* pSrc = (const char*)&pInputMem[dwInputPitch * row];

		for (uint32 dwX = 0; dwX < dwWidth; ++dwX)
		{
			if (dwPlanes == 1)
			{
				*pDst++ = *pSrc++;
			}
			else if (dwPlanes == 2)
			{
				// convert RGB to CMY (2 channel images are automatically made PHOTOMETRIC_SEPARATED, which refers to inks in TIFF, the value is inverted)
				*pDst++ = 0xFF - *pSrc++;
				*pDst++ = 0xFF - *pSrc++;
			}
			else if (dwPlanes == 3)
			{
				char b = *pSrc++;
				char g = *pSrc++;
				char r = *pSrc++;

				// swap red with blue
				*pDst++ = r;
				*pDst++ = g;
				*pDst++ = b;
			}
			else if (dwPlanes == 4)
			{
				char b = *pSrc++;
				char g = *pSrc++;
				char r = *pSrc++;
				char a = *pSrc++;

				// swap red with blue
				*pDst++ = r;
				*pDst++ = g;
				*pDst++ = b;
				*pDst++ = a;
			}
		}

		const int err = TIFFWriteScanline(tif, &raster[0], row, 0);
		assert(err >= 0);
	}
}

static void Save16BitImageToTIFF(struct tiff* tif, const ImageObject* pImageObject)
{
	TIFFSetField(tif, TIFFTAG_BITSPERSAMPLE, 16);

	int dwPlanes;
	CPixelFormats::GetPixelFormatInfo(pImageObject->GetPixelFormat(), 0, &dwPlanes, 0);

	uint32 dwWidth, dwHeight, dwMips;
	pImageObject->GetExtent(dwWidth, dwHeight, dwMips);

	char* pInputMem;
	uint32 dwInputPitch;		
	pImageObject->GetImagePointer(0, pInputMem, dwInputPitch);

	std::vector<short> raster(dwWidth * dwPlanes);
	for (uint32 row = 0; row < dwHeight; ++row)
	{
		uint16* pDst = (uint16*)&raster[0];
		const uint16* pSrc = (const uint16*)&pInputMem[dwInputPitch * row];

		for (uint32 dwX = 0; dwX < dwWidth; ++dwX)
		{
			for (int dwC = 0; dwC < dwPlanes; ++dwC)
			{
				if (dwPlanes == 2)
				{
					// convert RGB to CMY (2 channel images are automatically made PHOTOMETRIC_SEPARATED, which refers to inks in TIFF, the value is inverted)
					*pDst++ = 0xFFFF - *pSrc++;
				}
				else
				{
					*pDst++ = *pSrc++;
				}
			}
		}

		const int err = TIFFWriteScanline(tif, &raster[0], row, 0);
		assert(err >= 0);
	}
}

static void Save16BitHDRImageToTIFF(struct tiff* tif, const ImageObject* pImageObject)
{
	TIFFSetField(tif, TIFFTAG_BITSPERSAMPLE, 16);
	TIFFSetField(tif, TIFFTAG_SAMPLEFORMAT, SAMPLEFORMAT_IEEEFP);

	int dwPlanes;
	CPixelFormats::GetPixelFormatInfo(pImageObject->GetPixelFormat(), 0, &dwPlanes, 0);

	uint32 dwWidth, dwHeight, dwMips;
	pImageObject->GetExtent(dwWidth, dwHeight, dwMips);

	char* pInputMem;
	uint32 dwInputPitch;		
	pImageObject->GetImagePointer(0, pInputMem, dwInputPitch);

	std::vector<short> raster(dwWidth * dwPlanes);
	for (uint32 row = 0; row < dwHeight; ++row)
	{
		SHalf* pDst = (SHalf*)&raster[0];
		const SHalf* pSrc = (const SHalf*)&pInputMem[dwInputPitch * row];

		for (uint32 dwX = 0; dwX < dwWidth; ++dwX)
		{
			for (int dwC = 0; dwC < dwPlanes; ++dwC)
			{
				if (dwPlanes == 2)
				{
					// convert RGB to CMY (2 channel images are automatically made PHOTOMETRIC_SEPARATED, which refers to inks in TIFF, the value is inverted)
					*pDst++ = SHalf(1.0f - *pSrc++);
				}
				else
				{
					*pDst++ = *pSrc++;
				}
			}
		}

		const int err = TIFFWriteScanline(tif, &raster[0], row, 0);
		assert(err >= 0);
	}
}

static void Save32BitHDRImageToTIFF(struct tiff* tif, const ImageObject* pImageObject)
{
	TIFFSetField(tif, TIFFTAG_BITSPERSAMPLE, 16);
	TIFFSetField(tif, TIFFTAG_SAMPLEFORMAT, SAMPLEFORMAT_IEEEFP);

	int dwPlanes;
	CPixelFormats::GetPixelFormatInfo(pImageObject->GetPixelFormat(), 0, &dwPlanes, 0);

	uint32 dwWidth, dwHeight, dwMips;
	pImageObject->GetExtent(dwWidth, dwHeight, dwMips);

	char* pInputMem;
	uint32 dwInputPitch;		
	pImageObject->GetImagePointer(0, pInputMem, dwInputPitch);

	std::vector<short> raster(dwWidth * dwPlanes);
	for (uint32 row = 0; row < dwHeight; ++row)
	{
		SHalf* pDst = (SHalf*)&raster[0];
		const float* pSrc = (const float*)&pInputMem[dwInputPitch * row];

		for (uint32 dwX = 0; dwX < dwWidth; ++dwX)
		{
			for (int dwC = 0; dwC < dwPlanes; ++dwC)
			{
				if (dwPlanes == 2)
				{
					// convert RGB to CMY (2 channel images are automatically made PHOTOMETRIC_SEPARATED, which refers to inks in TIFF, the value is inverted)
					*pDst++ = SHalf(1.0f - *pSrc++);
				}
				else
				{
					*pDst++ = SHalf(*pSrc++);
				}
			}
		}

		const int err = TIFFWriteScanline(tif, &raster[0], row, 0);
		assert(err >= 0);
	}
}

} // namespace ImageTIFF
