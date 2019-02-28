// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.
#include "stdafx.h"

#include <assert.h>                 // assert()
#include "IRCLog.h"                 // IRCLog
#include "IConfig.h"                // IConfig
#include "ImageCompiler.h"          // CImageCompiler
#include "ImageObject.h"


ImageObject::ImageObject(const uint32 width, const uint32 height, const uint32 maxMipCount, const EPixelFormat pixelFormat, ECubemap eCubemap, const int compressedBlockWidth, const int compressedBlockHeight)
	: m_pixelFormat(pixelFormat)
	, m_eCubemap(eCubemap)
	, m_colMinARGB(0.0f, 0.0f, 0.0f, 0.0f)
	, m_colMaxARGB(1.0f, 1.0f, 1.0f, 1.0f)
	, m_averageBrightness(0.0f)
	, m_pAttachedImage(0)
	, m_imageFlags(0)
	, m_numPersistentMips(0)
{
	assert(width > 0);
	assert(height > 0);
	assert(maxMipCount > 0);

	const PixelFormatInfo* const pFmt = CPixelFormats::GetPixelFormatInfo(m_pixelFormat);
	assert(pFmt);

	m_compressedBlockWidth = (pFmt->bCompressed && compressedBlockWidth != -1) ? compressedBlockWidth : pFmt->blockWidth;
	m_compressedBlockHeight = (pFmt->bCompressed && compressedBlockHeight != -1) ? compressedBlockHeight : pFmt->blockHeight;

	const uint32 faceWidth = (eCubemap == eCubemap_Yes) ? width / 6 : width;

	if (faceWidth < pFmt->minWidth)
	{
		assert(0);
	}

	if (height < pFmt->minHeight)
	{
		assert(0);
	}

	switch (eCubemap)
	{
	case eCubemap_UnknownYet:
		assert(!pFmt->bCompressed);
		assert(maxMipCount == 1);
		break;
	case eCubemap_Yes:
		assert(width == height * 6);
		assert(Util::isPowerOfTwo(height));
		break;
	case eCubemap_No:
		break;
	default:
		assert(0);
		break;
	}

	const uint32 mipCount = Util::getMin(maxMipCount, CPixelFormats::ComputeMaxMipCount(m_pixelFormat, width, height, (eCubemap == eCubemap_Yes)));

	m_mips.reserve(mipCount);

	for (uint32 mip = 0; mip < mipCount; ++mip)
	{
		MipLevel* const pEntry = new MipLevel;

		uint32 localWidth = width >> mip;
		uint32 localHeight = height >> mip;
		if (localWidth < 1)
		{
			localWidth = 1;
		}
		if (localHeight < 1)
		{
			localHeight = 1;
		}

		pEntry->m_width = localWidth;
		pEntry->m_height = localHeight;

		if (pFmt->bCompressed)
		{
			const uint32 blocksInRow = (pEntry->m_width + (m_compressedBlockWidth - 1)) / m_compressedBlockWidth;
			pEntry->m_pitch = blocksInRow * pFmt->bytesPerBlock;
			pEntry->m_rowCount = (localHeight + (m_compressedBlockHeight - 1)) / m_compressedBlockHeight;
		}
		else
		{
			assert(m_compressedBlockWidth == 1);
			assert(m_compressedBlockHeight == 1);
			pEntry->m_pitch = pEntry->m_width * pFmt->bytesPerBlock;
			pEntry->m_rowCount = localHeight;
		}

		pEntry->Alloc();

		m_mips.push_back(pEntry);
	}
}


void ImageObject::ResetImage(const uint32 width, const uint32 height, const uint32 maxMipCount, const EPixelFormat pixelFormat, ECubemap eCubemap, const int compressedBlockWidth, const int compressedBlockHeight)
{
	delete m_pAttachedImage;
	for (uint32 mip = 0; mip < uint32(m_mips.size()); ++mip)
		delete m_mips[mip];

	m_pixelFormat = pixelFormat;
	m_eCubemap = eCubemap;
	m_colMinARGB = Vec4(0.0f, 0.0f, 0.0f, 0.0f);
	m_colMaxARGB = Vec4(1.0f, 1.0f, 1.0f, 1.0f);
	m_averageBrightness = 0.0f;
	m_pAttachedImage = 0;
	m_imageFlags = 0;
	m_numPersistentMips = 0;
	m_mips.clear();

	assert(width > 0);
	assert(height > 0);
	assert(maxMipCount > 0);

	const PixelFormatInfo* const pFmt = CPixelFormats::GetPixelFormatInfo(m_pixelFormat);
	assert(pFmt);

	m_compressedBlockWidth = (pFmt->bCompressed && compressedBlockWidth != -1) ? compressedBlockWidth : pFmt->blockWidth;
	m_compressedBlockHeight = (pFmt->bCompressed && compressedBlockHeight != -1) ? compressedBlockHeight : pFmt->blockHeight;

	const uint32 faceWidth = (eCubemap == eCubemap_Yes) ? width / 6 : width;

	if (faceWidth < pFmt->minWidth)
	{
		assert(0);
	}

	if (height < pFmt->minHeight)
	{
		assert(0);
	}

	switch (eCubemap)
	{
	case eCubemap_UnknownYet:
		assert(!pFmt->bCompressed);
		assert(maxMipCount == 1);
		break;
	case eCubemap_Yes:
		assert(width == height * 6);
		assert(Util::isPowerOfTwo(height));
		break;
	case eCubemap_No:
		break;
	default:
		assert(0);
		break;
	}

	const uint32 mipCount = Util::getMin(maxMipCount, CPixelFormats::ComputeMaxMipCount(m_pixelFormat, width, height, (eCubemap == eCubemap_Yes)));

	m_mips.reserve(mipCount);

	for (uint32 mip = 0; mip < mipCount; ++mip)
	{
		MipLevel* const pEntry = new MipLevel;

		uint32 localWidth = width >> mip;
		uint32 localHeight = height >> mip;
		if (localWidth < 1)
		{
			localWidth = 1;
		}
		if (localHeight < 1)
		{
			localHeight = 1;
		}

		pEntry->m_width = localWidth;
		pEntry->m_height = localHeight;

		if (pFmt->bCompressed)
		{
			const uint32 blocksInRow = (pEntry->m_width + (m_compressedBlockWidth - 1)) / m_compressedBlockWidth;
			pEntry->m_pitch = blocksInRow * pFmt->bytesPerBlock;
			pEntry->m_rowCount = (localHeight + (m_compressedBlockHeight - 1)) / m_compressedBlockHeight;
		}
		else
		{
			assert(m_compressedBlockWidth == 1);
			assert(m_compressedBlockHeight == 1);
			pEntry->m_pitch = pEntry->m_width * pFmt->bytesPerBlock;
			pEntry->m_rowCount = localHeight;
		}

		pEntry->Alloc();

		m_mips.push_back(pEntry);
	}
}


// clone this image-object's contents, but not the properties
ImageObject* ImageObject::CopyImage() const
{
	const EPixelFormat srcPixelformat = GetPixelFormat();

	std::auto_ptr<ImageObject> pRet(AllocateImage(0));

	uint32 dwMips = pRet->GetMipCount();
	for (uint32 dwMip = 0; dwMip < dwMips; ++dwMip)
	{
		uint32 dwLocalWidth = GetWidth(dwMip);			// we get error on NVidia with this (assumes input is 4x4 as well)
		uint32 dwLocalHeight = GetHeight(dwMip);

		uint32 dwLines = dwLocalHeight;

		if (CPixelFormats::GetPixelFormatInfo(srcPixelformat)->bCompressed)
		{
			dwLines = (dwLocalHeight + GetCompressedBlockHeight() - 1) / GetCompressedBlockHeight();
		}

		char *pMem;
		uint32 dwPitch;
		GetImagePointer(dwMip,pMem,dwPitch);

		char *pDstMem;
		uint32 dwDstPitch;
		pRet->GetImagePointer(dwMip, pDstMem, dwDstPitch);

		for (uint32 dwY = 0; dwY < dwLines; ++dwY)
		{
			memcpy(&pDstMem[dwDstPitch * dwY], &pMem[dwPitch * dwY], Util::getMin(dwPitch, dwDstPitch));
		}
	}

	// recursive for all attached images
	{
		const ImageObject* const pAttached = GetAttachedImage();
		if (pAttached)
		{
			ImageObject* const p = pAttached->CopyImage();
			pRet->SetAttachedImage(p);
		}
	}

	return pRet.release();
}


ImageObject* ImageObject::CopyImageArea(int offsetx, int offsety, int width, int height) const
{
	const EPixelFormat srcPixelformat = GetPixelFormat();
	if (srcPixelformat != ePixelFormat_X8R8G8B8)
	{
		assert(0);
		RCLogError("%s: unsupported source format. Contact an RC programmer.", __FUNCTION__);
		return NULL;
	}

	uint32 dwWidth, dwHeight, dwMips;
	GetExtent(dwWidth, dwHeight, dwMips);

	ImageObject* const pRet = new ImageObject(width, height, 1, srcPixelformat, GetCubemap());

	pRet->CopyPropertiesFrom(*this);

	char *pMem;
	uint32 dwPitch;
	GetImagePointer(0, pMem, dwPitch);

	char *pDstMem;
	uint32 dwDstPitch;
	pRet->GetImagePointer(0, pDstMem, dwDstPitch);

	for (uint32 dwY = 0; dwY < height; ++dwY)
	{
		for (uint32 dwX = 0; dwX < width; ++dwX)
		{
			pDstMem[(dwDstPitch * dwY) + ((dwX * 4) + 0)] = pMem[(dwPitch * (dwY + offsety)) + (((dwX + offsetx) * 4) + 0)];
			pDstMem[(dwDstPitch * dwY) + ((dwX * 4) + 1)] = pMem[(dwPitch * (dwY + offsety)) + (((dwX + offsetx) * 4) + 1)];
			pDstMem[(dwDstPitch * dwY) + ((dwX * 4) + 2)] = pMem[(dwPitch * (dwY + offsety)) + (((dwX + offsetx) * 4) + 2)];
			pDstMem[(dwDstPitch * dwY) + ((dwX * 4) + 3)] = pMem[(dwPitch * (dwY + offsety)) + (((dwX + offsetx) * 4) + 3)];
		}	
	}

	return pRet;
}

void ImageObject::ClearImage()
{
	const EPixelFormat srcPixelformat = GetPixelFormat();

	uint32 dwMips = GetMipCount();
	for (uint32 dwMip = 0; dwMip < dwMips; ++dwMip)
	{
		uint32 dwLocalWidth = GetWidth(dwMip);			// we get error on NVidia with this (assumes input is 4x4 as well)
		uint32 dwLocalHeight = GetHeight(dwMip);

		uint32 dwLines = dwLocalHeight;

		if (CPixelFormats::GetPixelFormatInfo(srcPixelformat)->bCompressed)
		{
			dwLines = (dwLocalHeight + GetCompressedBlockHeight() - 1) / GetCompressedBlockHeight();
		}

		char *pMem;
		uint32 dwPitch;
		GetImagePointer(dwMip, pMem, dwPitch);

		memset(pMem, 0, dwPitch * dwLines);
	}

	// recursive for all attached images
	{
		ImageObject* const pAttached = GetAttachedImage();
		if (pAttached)
		{
			pAttached->ClearImage();
		}
	}
}

// allocate an image with the same properties as the given image and the requested format, with dropping top-mips if requested
ImageObject* ImageObject::AllocateImage(const uint32 highestMip, const EPixelFormat pixelFormat, const int compressedBlockWidth, const int compressedBlockHeight) const
{
	ImageObject* pRet = new ImageObject(GetWidth(highestMip), GetHeight(highestMip), GetMipCount() - highestMip, pixelFormat, GetCubemap(), compressedBlockWidth, compressedBlockHeight);
	// block compression formats may cause the allocator to drop the bottom mips
	assert(pRet->GetMipCount() <= GetMipCount() - highestMip);
	pRet->CopyPropertiesFrom(*this);

	return pRet;
}


// allocate an image with the same properties as the given image, with dropping top-mips if requested
ImageObject* ImageObject::AllocateImage(const uint32 highestMip) const
{
	ImageObject* pRet = new ImageObject(GetWidth(highestMip), GetHeight(highestMip), GetMipCount() - highestMip, GetPixelFormat(), GetCubemap(), GetCompressedBlockWidth(), GetCompressedBlockHeight());
	// block compression formats may cause the allocator to drop the bottom mips
	assert(pRet->GetMipCount() <= GetMipCount() - highestMip);
	pRet->CopyPropertiesFrom(*this);

	return pRet;
}


// allocate an image with the same properties as the given image, with dropping top-mips if requested
ImageObject* ImageObject::AllocateImage(const CImageProperties* pProps, bool bRemoveMips, uint32& reduceResolution) const
{
	const EPixelFormat srcFormat = GetPixelFormat();
	uint32 srcWidth, srcHeight, srcMips;
	GetExtent(srcWidth, srcHeight, srcMips);

	if (GetCubemap() == eCubemap_Yes)
	{
		srcWidth /= 6;
	}

	const uint32 calculatedMipCount = CPixelFormats::ComputeMaxMipCount(srcFormat, srcWidth, srcHeight, false);

	uint32 newMipCount = Util::getMax(calculatedMipCount - reduceResolution, 1U);
	uint32 newWidth = Util::getMax(srcWidth  >> reduceResolution, 1U);
	uint32 newHeight = Util::getMax(srcHeight >> reduceResolution, 1U);

	const int maxTextureSize = pProps->GetMaxTextureSize();
	const int minTextureSize = pProps->GetMinTextureSize();

	if ((maxTextureSize == 1) || ((maxTextureSize > 1) && !Util::isPowerOfTwo(maxTextureSize)))
	{
		RCLogError("%s failed: MaxTextureSize specified is %d, but expected to be power-of-2 greater than 1 (2, 4, 8, 16, ...).", __FUNCTION__, maxTextureSize);
		return NULL;
	}

	if ((minTextureSize == 1) || ((minTextureSize > 1) && !Util::isPowerOfTwo(minTextureSize)))
	{
		RCLogError("%s failed: MinTextureSize specified is %d, but expected to be power-of-2 greater than 1 (2, 4, 8, 16, ...).", __FUNCTION__, minTextureSize);
		return NULL;
	}

	if ((minTextureSize > maxTextureSize))
	{
		RCLogError("%s failed: MinTextureSize specified is %d, but larger than MaxTextureSize specified as %d.", __FUNCTION__, minTextureSize, maxTextureSize);
		return NULL;
	}

	if (maxTextureSize > 0)
	{
		while ((newWidth > (uint32)maxTextureSize) || (newHeight > (uint32)maxTextureSize))
		{
			newWidth = Util::getMax(newWidth >> 1, 1U);
			newHeight = Util::getMax(newHeight >> 1, 1U);

			--newMipCount;
			++reduceResolution;
		}
	}

	if (minTextureSize > 0)
	{
		while ((newWidth < (uint32)minTextureSize) || (newHeight < (uint32)minTextureSize))
		{
			newWidth = Util::getMax(newWidth << 1, 1U);
			newHeight = Util::getMax(newHeight << 1, 1U);

			++newMipCount;
			--reduceResolution;
		}
	}

	newMipCount = (bRemoveMips ? 1U : Util::getMax(newMipCount, 1U));

	if (GetCubemap() == eCubemap_Yes)
	{
		newWidth *= 6;
		srcWidth *= 6;
	}

	ImageObject* const pRet = new ImageObject(newWidth, newHeight, newMipCount, ePixelFormat_A32B32G32R32F, GetCubemap());
	assert(pRet->GetMipCount() == newMipCount);
	pRet->CopyPropertiesFrom(*this);

	return pRet;
}


void ImageToProcess::ConvertFormat(const CImageProperties* pProps, EPixelFormat fmtTo)
{
	ConvertFormat(pProps, fmtTo, eQuality_Normal);
}

void ImageToProcess::ConvertFormat(const CImageProperties* pProps, EPixelFormat fmtTo, EQuality quality)
{
	CImageProperties::EImageCompressor const compressor = pProps->GetImageCompressor();

	ConvertFormatWithSpecifiedCompressor(pProps, fmtTo, quality, compressor);

	if (!get())
	{
		return;
	}

	if (get()->GetPixelFormat() == ePixelFormat_L8)
	{
		get()->AddImageFlags(CImageExtensionHelper::EIF_Greyscale);
	}
}


void ImageToProcess::ConvertFormatWithSpecifiedCompressor(const CImageProperties* pProps, EPixelFormat fmtDst, EQuality quality, CImageProperties::EImageCompressor compressor)
{
	uint32 dwWidth, dwHeight, dwMips;
	get()->GetExtent(dwWidth, dwHeight, dwMips);

	// Some compressors and/or graphic card drivers cannot handle images with
	// dimensions smaller than a particular size, so we should use an uncompressed format
	{
		const PixelFormatInfo* pFmt = CPixelFormats::GetPixelFormatInfo(fmtDst);

		if (pFmt->bSquarePow2 && ((dwWidth != dwHeight) || !Util::isPowerOfTwo(dwWidth)))
		{
			RCLogError("Format %s requires a square power-of-two image, but received (%d x %d). Contact an RC programmer.", pFmt->szName, dwWidth, dwHeight);
			set(0);
			return;
		}

		if (dwWidth < pFmt->minWidth || dwHeight < pFmt->minHeight)
		{
			RCLogWarning("Format %s requires an image at least (%d x %d), the image is too small (%d x %d) for compression, resorting to non-compressed format.",
				pFmt->szName, (int)pFmt->minWidth, (int)pFmt->minHeight, (int)dwWidth, (int)dwHeight);

			if (pFmt->nChannels == 1)
			{
				fmtDst = pFmt->bHasAlpha ? ePixelFormat_A8 : ePixelFormat_L8;
			}
			else if (pFmt->nChannels == 2)
			{
				fmtDst = pFmt->bHasAlpha ? ePixelFormat_A8L8 : ePixelFormat_X8R8G8B8;
			}
			else
			{
				fmtDst = pFmt->bHasAlpha ? ePixelFormat_A8R8G8B8 : ePixelFormat_X8R8G8B8;
			}

			pFmt = CPixelFormats::GetPixelFormatInfo(fmtDst);
		}

		if ((pFmt->nChannels <= 2) && pProps->m_bPreserveZ)
		{
			// cannot use 3DC if we have denormalized normals
			if (pProps->m_bPreserveAlpha)
			{
				fmtDst = ePixelFormat_A8R8G8B8;
			}
			else
			{
				fmtDst = ePixelFormat_X8R8G8B8;
			}
		}
	}

	// Special treatment of data for the BC6 format, which is only for floating-points
	if (((fmtDst == ePixelFormat_BC6UH) && (get()->GetPixelFormat() != ePixelFormat_A32B32G32R32F)) ||
		((get()->GetPixelFormat() == ePixelFormat_BC6UH) && (fmtDst != ePixelFormat_A32B32G32R32F)))
	{
		// convert to intermediate fp-buffer, then to the final format
		ConvertFormat(pProps, ePixelFormat_A32B32G32R32F, quality);
		ConvertFormat(pProps, fmtDst, quality);
		return;
	}

	// Special treatment of data for the RGBE format, which is only for floating-points
	if (((fmtDst == ePixelFormat_E5B9G9R9) && (get()->GetPixelFormat() != ePixelFormat_A32B32G32R32F)) ||
		((get()->GetPixelFormat() == ePixelFormat_E5B9G9R9) && (fmtDst != ePixelFormat_A32B32G32R32F)))
	{
		// convert to intermediate fp-buffer, then to the final format
		ConvertFormat(pProps, ePixelFormat_A32B32G32R32F, quality);
		ConvertFormat(pProps, fmtDst, quality);
		return;
	}

	// Special treatment of data for ASTC LDR formats
	if (((fmtDst >= ePixelFormat_ASTC_LDR_L) && (fmtDst <= ePixelFormat_ASTC_LDR_RGBA) && (get()->GetPixelFormat() != ePixelFormat_A8R8G8B8)) ||
		((get()->GetPixelFormat() >= ePixelFormat_ASTC_LDR_L) && (get()->GetPixelFormat() <= ePixelFormat_ASTC_LDR_RGBA) && (fmtDst != ePixelFormat_A8R8G8B8)))
	{
		ConvertFormat(pProps, ePixelFormat_A8R8G8B8);
		ConvertFormat(pProps, fmtDst);
		return;
	}

	// Special treatment of data for ASTC HDR formats
	if (((fmtDst >= ePixelFormat_ASTC_HDR_L) && (fmtDst <= ePixelFormat_ASTC_HDR_RGBA) && (get()->GetPixelFormat() != ePixelFormat_A16B16G16R16F)) ||
		((get()->GetPixelFormat() >= ePixelFormat_ASTC_HDR_L) && (get()->GetPixelFormat() <= ePixelFormat_ASTC_HDR_RGBA) && (fmtDst != ePixelFormat_A16B16G16R16F)))
	{
		ConvertFormat(pProps, ePixelFormat_A16B16G16R16F);
		ConvertFormat(pProps, fmtDst);
		return;
	}

	// Handle some special combinations of input and output formats which can/should be processed
	// faster or in a more robust fashion.
	{
		if (fmtDst == get()->GetPixelFormat())
		{
			return;
		}

		// some compressors cannot handle 1/2/3-channel input data,
		// so let's convert 1/2/3-channel data to 4-channel
		if (get()->GetPixelFormat() == ePixelFormat_L8)
		{
			ConvertFromL8ToXRGB8();
		}

		if (get()->GetPixelFormat() == ePixelFormat_A8)
		{
			ConvertFromA8ToARGB8();
		}
		
		if (get()->GetPixelFormat() == ePixelFormat_A8L8)
		{
			ConvertFromAL8ToARGB8();
		}

		if (get()->GetPixelFormat() == ePixelFormat_R8G8B8)
		{
			ConvertBetweenAnyRGB(ePixelFormat_X8R8G8B8);
		}

		if (CPixelFormats::IsPixelFormatAnyRGB(get()->GetPixelFormat()) && (fmtDst == ePixelFormat_L8))
		{
			ConvertFormat(pProps, ePixelFormat_X8R8G8B8, quality);
			ConvertFromAnyRGB8ToL8();
			return;
		}

		if (CPixelFormats::IsPixelFormatAnyRGB(get()->GetPixelFormat()) && (fmtDst == ePixelFormat_A8))
		{
			ConvertFormat(pProps, ePixelFormat_A8R8G8B8, quality);
			ConvertFromAnyRGB8ToA8();
			return;
		}

		if (CPixelFormats::IsPixelFormatAnyRGB(get()->GetPixelFormat()) && (fmtDst == ePixelFormat_A8L8))
		{
			ConvertFormat(pProps, ePixelFormat_A8R8G8B8, quality);
			ConvertFromAnyRGB8ToAL8();
			return;
		}

		if (CPixelFormats::IsPixelFormatAnyRGB(get()->GetPixelFormat()) && (fmtDst == ePixelFormat_R8))
		{
			ConvertFormat(pProps, ePixelFormat_A8R8G8B8, quality);
			ConvertFromRGBA8ToR8();
			return;
		}

		if (CPixelFormats::IsPixelFormatAnyRGB(get()->GetPixelFormat()) && (fmtDst == ePixelFormat_R16))
		{
			ConvertFormat(pProps, ePixelFormat_A16B16G16R16, quality);
			ConvertFromRGBA16ToR16();
			return;
		}

		if (CPixelFormats::IsPixelFormatAnyRGB(get()->GetPixelFormat()) && (fmtDst == ePixelFormat_R16F))
		{
			ConvertFormat(pProps, ePixelFormat_A16B16G16R16F, quality);
			ConvertFromRGBA16FToR16F();
			return;
		}

		if (CPixelFormats::IsPixelFormatAnyRGB(get()->GetPixelFormat()) && (fmtDst == ePixelFormat_R32F))
		{
			ConvertFormat(pProps, ePixelFormat_A32B32G32R32F, quality);
			ConvertFromRGBA32FToR32F();
			return;
		}

		if ((get()->GetPixelFormat() == ePixelFormat_R8) && CPixelFormats::IsPixelFormatAnyRGB(fmtDst))
		{
			ConvertFromR8ToRGBA8();
			ConvertFormat(pProps, fmtDst, quality);
			return;
		}

		if ((get()->GetPixelFormat() == ePixelFormat_R16) && CPixelFormats::IsPixelFormatAnyRGB(fmtDst))
		{
			ConvertFromR16ToRGBA16();
			ConvertFormat(pProps, fmtDst, quality);
			return;
		}

		if ((get()->GetPixelFormat() == ePixelFormat_R16F) && CPixelFormats::IsPixelFormatAnyRGB(fmtDst))
		{
			ConvertFromR16FToRGBA16F();
			ConvertFormat(pProps, fmtDst, quality);
			return;
		}

		if ((get()->GetPixelFormat() == ePixelFormat_R32F) && CPixelFormats::IsPixelFormatAnyRGB(fmtDst))
		{
			ConvertFromR32FToRGBA32F();
			ConvertFormat(pProps, fmtDst, quality);
			return;
		}

		if (CPixelFormats::IsPixelFormatAnyRGB(get()->GetPixelFormat()) && CPixelFormats::IsPixelFormatAnyRGB(fmtDst))
		{
			if ((get()->GetPixelFormat() == ePixelFormat_E5B9G9R9) || (fmtDst == ePixelFormat_E5B9G9R9))
			{
				ConvertBetweenRGB32FAndRGBE(fmtDst);
			}
			else {
				ConvertBetweenAnyRGB(fmtDst);
			}

			return;
		}
		else if (CPixelFormats::IsPixelFormatAnyRG(get()->GetPixelFormat()) && CPixelFormats::IsPixelFormatAnyRG(fmtDst))
		{
			ConvertBetweenAnyRGB(fmtDst);
			return;
		}

		if (CPixelFormats::IsFormatWithoutAlpha(fmtDst))
		{
			// some DXT1 compressors may require that the alpha channel contains opaque alpha values
			get()->SetConstantAlpha(255);
		}
		else if (CPixelFormats::IsFormatWithThresholdAlpha(fmtDst))
		{
			// for BC1a/DXT1a we should have valid values in alpha channel
			if (CPixelFormats::IsFormatWithoutAlpha(get()->GetPixelFormat()))
			{
				// force input image X8R8G8B8 to be opaque
				get()->SetConstantAlpha(255);
			}
			else
			{
				// Let's use use our own alpha value threshold to prevent DXT1a compressors
				// from using their internal alpha thresholds.
				// Note: after averaging of pure black (0) and white (255) alpha values in mipmap
				// generation, the averaged alpha value could be 127 or 128, depending on 
				// rounding-off errors. In this case, if the threshold used is 128, then produced binary
				// alpha value will be 0 or 255, almost randomly. So we should use slightly biased
				// threshold value, 127 or 129 for example, or even 126 or 130.
				get()->ThresholdAlpha(127);
			}
		}

		if (fmtDst == get()->GetPixelFormat())
		{
			return;
		}
	}

	// convert to CTX1 Xbox 360 specific format
	if (fmtDst == ePixelFormat_CTX1)
	{
		RCLogError("Failed to convert image to CTX1 format because this version of RC doesn't support Xbox 360 platform");
		set(0);
		return;
	}

	// CTSquish is the only compressor managing to de/compress these formats at the moment
	if ((fmtDst == ePixelFormat_BC6UH) || (get()->GetPixelFormat() == ePixelFormat_BC6UH) ||
	    (fmtDst == ePixelFormat_BC7  ) || (get()->GetPixelFormat() == ePixelFormat_BC7  ) ||
	    (fmtDst == ePixelFormat_BC7t ) || (get()->GetPixelFormat() == ePixelFormat_BC7t ) ||
	    (fmtDst == ePixelFormat_BC4s ) || (get()->GetPixelFormat() == ePixelFormat_BC4s ) ||
	    (fmtDst == ePixelFormat_BC5s ) || (get()->GetPixelFormat() == ePixelFormat_BC5s ))
	{
		if (compressor != CImageProperties::eImageCompressor_CTSquish &&
		    compressor != CImageProperties::eImageCompressor_CTSquishHiQ &&
		    compressor != CImageProperties::eImageCompressor_CTSquishFast)
		{
			compressor = CImageProperties::eImageCompressor_CTSquish;
		}
	}

	// try CTSquish
	if (compressor == CImageProperties::eImageCompressor_CTSquish ||
	    compressor == CImageProperties::eImageCompressor_CTSquishHiQ ||
	    compressor == CImageProperties::eImageCompressor_CTSquishFast)
	{
		if (compressor == CImageProperties::eImageCompressor_CTSquishFast)
		{
			quality = eQuality_Fast;
		}
		else if (compressor == CImageProperties::eImageCompressor_CTSquishHiQ)
		{
			quality = eQuality_Slow;
		}

		const EResult res = ConvertFormatWithCTSquisher(pProps, fmtDst, quality);

		if (res == eResult_Failed || res == eResult_Success)
		{
			return;
		}
		
		if (res != eResult_UnsupportedFormat)
		{
			RCLogError("Internal error in %s.", __FUNCTION__);
			set(0);
			return;
		}
	}

	// destination-format fall-back when none of the above captures it
	switch (fmtDst) 
	{
	case ePixelFormat_DXT3t:
		fmtDst = ePixelFormat_DXT3;
		break;
	case ePixelFormat_DXT5t:
		fmtDst = ePixelFormat_DXT5;
		break;
	case ePixelFormat_BC1:
		fmtDst = ePixelFormat_DXT1;
		break;
	case ePixelFormat_BC1a:
		fmtDst = ePixelFormat_DXT1a;
		break;
	case ePixelFormat_BC2:
		fmtDst = ePixelFormat_DXT3;
		break;
	case ePixelFormat_BC2t:
		fmtDst = ePixelFormat_DXT3;
		break;
	case ePixelFormat_BC3:
		fmtDst = ePixelFormat_DXT5;
		break;
	case ePixelFormat_BC3t:
		fmtDst = ePixelFormat_DXT5;
		break;
	case ePixelFormat_BC4:
		fmtDst = ePixelFormat_3DCp;
		break;
	case ePixelFormat_BC5:
		fmtDst = ePixelFormat_3DC;
		break;
	case ePixelFormat_BC4s:
	case ePixelFormat_BC5s:
		RCLogError("No compressor for signed BC4/5 is available.");
		set(0);
		return;
	case ePixelFormat_BC6UH:
		RCLogError("No compressor for BC6 is available.");
		set(0);
		return;
	case ePixelFormat_BC7:
	case ePixelFormat_BC7t:
		RCLogError("No compressor for BC7 is available.");
		set(0);
		return;
	default:
		break;
	}

	switch (get()->GetPixelFormat()) 
	{
	case ePixelFormat_BC6UH:
		RCLogError("No decompressor for BC6 is available.");
		set(0);
		return;
	case ePixelFormat_BC7:
	case ePixelFormat_BC7t:
		RCLogError("No decompressor for BC7 is available.");
		set(0);
		return;
	case ePixelFormat_CTX1:
		RCLogError("No decompressor for CTX1 is available.");
		set(0);
		return;
	default:
		break;
	}

#if defined(TOOLS_SUPPORT_ASTC)
	if (((fmtDst >= ePixelFormat_ASTC_LDR_L) && (fmtDst <= ePixelFormat_ASTC_HDR_RGBA)) ||
		((get()->GetPixelFormat() >= ePixelFormat_ASTC_LDR_L) && (get()->GetPixelFormat() <= ePixelFormat_ASTC_HDR_RGBA)))
	{
		const EResult res = ConvertFormatWithASTCCompressor(pProps, fmtDst, quality);

		if (res == eResult_Failed || res == eResult_Success)
		{
			return;
		}

		if (res != eResult_UnsupportedFormat)
		{
			RCLogError("Internal error in %s.", __FUNCTION__);
			set(0);
			return;
		}
	}
#endif

	// None of the following compressors can convert to/from floating point images, so we must
	// convert through RGBA8/RGBX8.

	if (fmtDst == ePixelFormat_A32B32G32R32F || fmtDst == ePixelFormat_A16B16G16R16F)
	{
		ConvertFormat(pProps, CPixelFormats::IsFormatWithoutAlpha(get()->GetPixelFormat()) ? ePixelFormat_X8R8G8B8 : ePixelFormat_A8R8G8B8, quality);
		ConvertFormat(pProps, fmtDst, quality);
		return;
	}

	if (get()->GetPixelFormat() == ePixelFormat_A32B32G32R32F || get()->GetPixelFormat() == ePixelFormat_A16B16G16R16F)
	{
		ConvertFormat(pProps, CPixelFormats::IsFormatWithoutAlpha(fmtDst) ? ePixelFormat_X8R8G8B8 : ePixelFormat_A8R8G8B8, quality);
		ConvertFormat(pProps, fmtDst, quality);
		return;
	}

	// try PVRTC (PVRTC can covert to/from a POWERVR PVRT format)
#if defined(TOOLS_SUPPORT_POWERVR)
	{
		const EResult res = ConvertFormatWithPVRTCCompressor(pProps, fmtDst, quality);

		if (res == eResult_Failed || res == eResult_Success)
		{
			return;
		}

		assert(res == eResult_UnsupportedFormat);
	}
#endif

	RCLogError("Unsupported format converting (fmtDst: %d).", fmtDst);
	set(0);
}

void ImageToProcess::ConvertModel(const CImageProperties* pProps, CImageProperties::EColorModel modelTo)
{
	const uint iFlags = get()->GetImageFlags() & CImageExtensionHelper::EIF_Colormodel;
	CImageProperties::EColorModel modelFrom =
		(iFlags == CImageExtensionHelper::EIF_Colormodel_CIE ? CImageProperties::eColorModel_CIE :
		(iFlags == CImageExtensionHelper::EIF_Colormodel_YCC ? CImageProperties::eColorModel_YCbCr :
		(iFlags == CImageExtensionHelper::EIF_Colormodel_YFF ? CImageProperties::eColorModel_YFbFr :
		(iFlags == CImageExtensionHelper::EIF_Colormodel_IRB ? CImageProperties::eColorModel_IRB :
		                                                       CImageProperties::eColorModel_RGB))));

	// no need to convert if they are the same colormodel
	if (modelFrom == modelTo)
	{
		return;
	}

	// otherwise convert any format to RGB first, if not alreay RGB,
	// (because we only have Model-to/from-RGB functionality)
	// and then convert it to the target model
	if (modelFrom != CImageProperties::eColorModel_RGB)
	{
		if (modelFrom == CImageProperties::eColorModel_CIE)
		{
			ConvertBetweenRGBA32FAndCIE32F(false);
		}
		else if (modelFrom == CImageProperties::eColorModel_YCbCr)
		{
			ConvertBetweenRGB32FAndYCbCr32F(false);
		}
		else if (modelFrom == CImageProperties::eColorModel_YFbFr)
		{
			ConvertBetweenRGB32FAndYFbFr32F(false);
		}
		else if (modelFrom == CImageProperties::eColorModel_IRB)
		{
			ConvertBetweenRGBA32FAndIRB32F(false);
		}
		else
		{
			assert(0);
		}
	}

	if (modelTo != CImageProperties::eColorModel_RGB)
	{
		if (modelTo == CImageProperties::eColorModel_CIE)
		{
			ConvertBetweenRGBA32FAndCIE32F(true);
		}
		else if (modelTo == CImageProperties::eColorModel_YCbCr)
		{
			ConvertBetweenRGB32FAndYCbCr32F(true);
		}
		else if (modelTo == CImageProperties::eColorModel_YFbFr)
		{
			ConvertBetweenRGB32FAndYFbFr32F(true);
		}
		else if (modelTo == CImageProperties::eColorModel_IRB)
		{
			ConvertBetweenRGBA32FAndIRB32F(true);
		}
		else
		{
			assert(0);
		}
	}
}


ImageObject::~ImageObject()
{
	for (size_t i = 0; i < m_mips.size(); ++i)
	{
		delete m_mips[i];
	}
	m_mips.clear();

	delete m_pAttachedImage;
	m_pAttachedImage = 0;
}


bool ImageObject::SaveImage(const char* filename, bool bForceDX10) const
{
	wstring wideFilename;
	Unicode::Convert(wideFilename, filename);

	FILE* const out = _wfopen(wideFilename.c_str(), L"wb");
	if (!out)
	{
		RCLogError("%s: failed to create file %s", __FUNCTION__, filename);
		return false;
	}

	const bool bOk = SaveImage(out, bForceDX10);

	fclose(out);

	if (!bOk)
	{
		::remove(filename);
	}

	return bOk;
}


bool ImageObject::SaveImage(FILE* const out, bool bForceDX10) const
{
	const uint32 mipCount = m_mips.size();
	const uint32 sliceCount = 1;

	assert(mipCount > 0);
	assert(m_mips[0]);

	CImageExtensionHelper::DDS_FILE_DESC desc;
	CImageExtensionHelper::DDS_HEADER_DXT10 exthead;

	desc.dwMagic = MAKEFOURCC('D','D','S',' ');

	if (!CPixelFormats::BuildSurfaceHeader(this, mipCount, desc.header, bForceDX10))
	{
		return false;
	}

	if (desc.header.IsDX10Ext())
	{
		if (!CPixelFormats::BuildSurfaceExtendedHeader(this, sliceCount, exthead))
		{
			return false;
		}
	}

	fwrite(&desc, sizeof(desc), 1, out);

	if (desc.header.IsDX10Ext())
	{
		fwrite(&exthead, sizeof(exthead), 1, out);
	}

	const int sideCount = (GetImageFlags() & CImageExtensionHelper::EIF_Cubemap) ? 6 : 1;
	if (sideCount == 6)
	{
		assert(m_eCubemap == eCubemap_Yes);
	}
	else
	{
		assert(m_eCubemap == eCubemap_No);
	}

	for (int side = 0; side < sideCount; ++side)
	{
		for (uint32 mip = 0; mip < mipCount; ++mip)
		{
			const MipLevel& level = *m_mips[mip];

			const size_t pitch = level.m_pitch;
			const size_t sidePitch = pitch / sideCount;
			assert(sidePitch * sideCount == pitch);
			const size_t nSideSize = level.GetSize() / sideCount;
			assert(nSideSize * sideCount == level.GetSize());
			for(int row = 0; row < level.m_rowCount; ++row)
			{
				fwrite(level.m_pData + row * pitch + side * sidePitch, sidePitch, 1, out);
			}
		}
	}

	// write out extra data (fe. attached alpha-images),
	// also ensure writing coherent header-types for all
	// DDS-blocks within this file (eg. all headers are
	// DX9-type or all headers are DX10-type, not mixed)
	SaveExtendedData(out, desc.header.IsDX10Ext());

	return true;
}


bool ImageObject::SaveExtendedData(FILE *out, bool bForceDX10) const
{
	fwrite("CExt", 4, 1, out);  // marker for the start of Crytek Extended data

	ImageObject* const pAttached = GetAttachedImage();
	if (pAttached)
	{
		// inherit cubemap and decal image flags to attached alpha image
		pAttached->AddImageFlags(GetImageFlags() & (CImageExtensionHelper::EIF_Cubemap | CImageExtensionHelper::EIF_Decal));

		fwrite("AttC", 4, 1, out);  // Attached Channel chunk

		uint32 size = 0;
		fwrite(&size, 4, 1, out);

		const long startPos = ftell(out);

		if (!pAttached->SaveImage(out, bForceDX10))
		{
			return false;
		}

		const long endPos = ftell(out);

		// write real size of the attached data
		size = endPos - startPos;
		fseek(out, startPos - 4, SEEK_SET);
		fwrite(&size, 4, 1, out);
		fseek(out, endPos, SEEK_SET);
	}

	fwrite("CEnd", 4, 1, out);  // marker for the end of Crytek Extended data

	return true;
}


bool ImageObject::LoadImage(const char* filename, bool bForceDX10)
{
	wstring wideFilename;
	Unicode::Convert(wideFilename, filename);

	FILE* const out = _wfopen(wideFilename.c_str(), L"rb");
	if (!out)
	{
		RCLogError("%s: failed to open file %s", __FUNCTION__, filename);
		return false;
	}

	const bool bOk = LoadImage(out, bForceDX10);

	fclose(out);

	return bOk;
}


bool ImageObject::LoadImage(FILE* const in, bool bForceDX10)
{
	CImageExtensionHelper::DDS_FILE_DESC desc;
	CImageExtensionHelper::DDS_HEADER_DXT10 exthead;

	uint32 mipCount = 0;
	uint32 sliceCount = 0;

	fread(&desc, sizeof(desc), 1, in);

	if (desc.dwMagic != MAKEFOURCC('D','D','S',' '))
	{
		return false;
	}

	if (desc.header.IsDX10Ext())
	{
		fread(&exthead, sizeof(exthead), 1, in);
	}

	if (!CPixelFormats::ParseSurfaceHeader(this, mipCount, desc.header, bForceDX10))
	{
		return false;
	}

	if (desc.header.IsDX10Ext())
	{
		if (!CPixelFormats::ParseSurfaceExtendedHeader(this, sliceCount, exthead))
		{
			return false;
		}
	}

	assert(mipCount > 0);
	assert(m_mips[0]);

	const int sideCount = (GetImageFlags() & CImageExtensionHelper::EIF_Cubemap) ? 6 : 1;
	if (sideCount == 6)
	{
		assert(m_eCubemap == eCubemap_Yes);
	}
	else
	{
		assert(m_eCubemap == eCubemap_No);
	}

	for (int side = 0; side < sideCount; ++side)
	{
		for (uint32 mip = 0; mip < mipCount; ++mip)
		{
			const MipLevel& level = *m_mips[mip];

			const size_t pitch = level.m_pitch;
			const size_t sidePitch = pitch / sideCount;
			assert(sidePitch * sideCount == pitch);
			const size_t nSideSize = level.GetSize() / sideCount;
			assert(nSideSize * sideCount == level.GetSize());
			for(int row = 0; row < level.m_rowCount; ++row)
			{
				fread(level.m_pData + row * pitch + side * sidePitch, sidePitch, 1, in);
			}
		}
	}

	// write out extra data (fe. attached alpha-images),
	// also ensure writing coherent header-types for all
	// DDS-blocks within this file (eg. all headers are
	// DX9-type or all headers are DX10-type, not mixed)
	LoadExtendedData(in, desc.header.IsDX10Ext());

	return true;
}


bool ImageObject::LoadExtendedData(FILE *in, bool bForceDX10)
{
	char Marker[5] = {0};

	fread(Marker, 4, 1, in); 
	if (!strcmp("CExt", Marker)) // marker for the start of Crytek Extended data
	{
		fread(Marker, 4, 1, in);  
		if (!strcmp("AttC", Marker)) // Attached Channel chunk
		{
			uint32 size = 0;
			fread(&size, 4, 1, in);

			ImageObject* const pAttached = new ImageObject(128, 128, 1, ePixelFormat_A32B32G32R32F, ImageObject::eCubemap_UnknownYet);

			if (!pAttached->LoadImage(in, bForceDX10))
			{
				return false;
			}

			SetAttachedImage(pAttached);

			fread(Marker, 4, 1, in);  // Attached Channel chunk
		}

		if (!strcmp("CEnd", Marker)) // marker for the end of Crytek Extended data
		{
			fread(Marker, 4, 1, in); 
		}
	}

	fseek(in, -4, SEEK_CUR); 

	return true;
}
