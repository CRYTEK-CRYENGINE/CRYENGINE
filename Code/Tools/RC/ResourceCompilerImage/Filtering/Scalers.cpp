// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include <assert.h>                         // assert()

#include "../ImageCompiler.h"								// CImageCompiler
#include "../ImageObject.h"									// ImageToProcess

///////////////////////////////////////////////////////////////////////////////////

void ImageToProcess::DownscaleTwiceHorizontally()
{
	assert(get());
	assert(get()->GetPixelFormat() == ePixelFormat_A32B32G32R32F);
	assert(get()->GetCubemap() != ImageObject::eCubemap_Yes);

	uint32 width, height, mips;
	get()->GetExtent(width, height, mips);
	assert(mips == 1);
	assert((width & 1) == 0);

	const uint32 newWidth = width >> 1;

	std::auto_ptr<ImageObject> pNewImage(new ImageObject(newWidth, height, 1, ePixelFormat_A32B32G32R32F, get()->GetCubemap()));
	pNewImage->CopyPropertiesFrom(*get());

	char* pSrcMem;
	uint32 srcPitch;
	get()->GetImagePointer(0, pSrcMem, srcPitch);

	char* pDstMem;
	uint32 dstPitch;
	pNewImage->GetImagePointer(0, pDstMem, dstPitch);

	for (uint32 y = 0; y < height; ++y)
	{
		const ColorF* const pSrc = (const ColorF*)(pSrcMem + y * srcPitch);
		ColorF* const pDst = (ColorF*)(pDstMem + y * dstPitch);
		for (uint32 x = 0; x < newWidth; ++x)
		{
			pDst[x] = (pSrc[x * 2 + 0] + pSrc[x * 2 + 1]) * 0.5f;
		}
	}

	set(pNewImage.release());
}

void ImageToProcess::DownscaleTwiceVertically()
{
	assert(get());
	assert(get()->GetPixelFormat() == ePixelFormat_A32B32G32R32F);
	assert(get()->GetCubemap() != ImageObject::eCubemap_Yes);

	uint32 width, height, mips;
	get()->GetExtent(width, height, mips);
	assert(mips == 1);
	assert((height & 1) == 0);

	const uint32 newHeight = height >> 1;

	std::auto_ptr<ImageObject> pNewImage(new ImageObject(width, newHeight, 1, ePixelFormat_A32B32G32R32F, get()->GetCubemap()));
	pNewImage->CopyPropertiesFrom(*get());

	char* pSrcMem;
	uint32 srcPitch;
	get()->GetImagePointer(0, pSrcMem, srcPitch);

	char* pDstMem;
	uint32 dstPitch;
	pNewImage->GetImagePointer(0, pDstMem, dstPitch);

	for (uint32 y = 0; y < newHeight; ++y)
	{
		const ColorF* const pSrc0 = (const ColorF*)(pSrcMem + (y * 2 + 0) * srcPitch);
		const ColorF* const pSrc1 = (const ColorF*)(pSrcMem + (y * 2 + 1) * srcPitch);
		ColorF* const pDst = (ColorF*)(pDstMem + y * dstPitch);
		for (uint32 x = 0; x < width; ++x)
		{
			pDst[x] = (pSrc0[x] + pSrc1[x]) * 0.5f;
		}
	}

	set(pNewImage.release());
}

void ImageToProcess::UpscalePow2TwiceHorizontally()
{
	assert(get());
	assert(get()->GetPixelFormat() == ePixelFormat_A32B32G32R32F);
	assert(get()->GetCubemap() != ImageObject::eCubemap_Yes);

	uint32 width, height, mips;
	get()->GetExtent(width, height, mips);
	assert(mips == 1);
	if (!Util::isPowerOfTwo(width))
	{
		assert(0);
		set(0);
		return;
	}

	const uint32 newWidth = width << 1;
	const uint32 widthMask = width - 1;

	std::auto_ptr<ImageObject> pNewImage(new ImageObject(newWidth, height, 1, ePixelFormat_A32B32G32R32F, get()->GetCubemap()));
	pNewImage->CopyPropertiesFrom(*get());

	char* pSrcMem;
	uint32 srcPitch;
	get()->GetImagePointer(0, pSrcMem, srcPitch);

	char* pDstMem;
	uint32 dstPitch;
	pNewImage->GetImagePointer(0, pDstMem, dstPitch);

	for (uint32 y = 0; y < height; ++y)
	{
		const ColorF* const pSrc = (const ColorF*)(pSrcMem + y * srcPitch);
		ColorF* const pDst = (ColorF*)(pDstMem + y * dstPitch);
		for (uint32 x = 0; x < width; ++x)
		{
			pDst[x * 2 + 0] = pSrc[(x - 1) & widthMask] * 0.25f + pSrc[x] * 0.75f;
			pDst[x * 2 + 1] = pSrc[(x + 1) & widthMask] * 0.25f + pSrc[x] * 0.75f;
		}
	}

	set(pNewImage.release());
}

void ImageToProcess::UpscalePow2TwiceVertically()
{
	assert(get());
	assert(get()->GetPixelFormat() == ePixelFormat_A32B32G32R32F);
	assert(get()->GetCubemap() != ImageObject::eCubemap_Yes);

	uint32 width, height, mips;
	get()->GetExtent(width, height, mips);
	assert(mips == 1);
	if (!Util::isPowerOfTwo(height))
	{
		assert(0);
		set(0);
		return;
	}

	const uint32 newHeight = height << 1;
	const uint32 heightMask = height - 1;

	std::auto_ptr<ImageObject> pNewImage(new ImageObject(width, newHeight, 1, ePixelFormat_A32B32G32R32F, get()->GetCubemap()));
	pNewImage->CopyPropertiesFrom(*get());

	char* pSrcMem;
	uint32 srcPitch;
	get()->GetImagePointer(0, pSrcMem, srcPitch);

	char* pDstMem;
	uint32 dstPitch;
	pNewImage->GetImagePointer(0, pDstMem, dstPitch);

	for (uint32 y = 0; y < height; ++y)
	{
		const ColorF* const pSrc0 = (const ColorF*)(pSrcMem + ((y - 1) & heightMask) * srcPitch);
		const ColorF* const pSrc1 = (const ColorF*)(pSrcMem +   y                    * srcPitch);
		const ColorF* const pSrc2 = (const ColorF*)(pSrcMem + ((y + 1) & heightMask) * srcPitch);
		ColorF* const pDst0 = (ColorF*)(pDstMem + (y * 2 + 0) * dstPitch);
		ColorF* const pDst1 = (ColorF*)(pDstMem + (y * 2 + 1) * dstPitch);
		for (uint32 x = 0; x < width; ++x)
		{
			pDst0[x] = pSrc0[x] * 0.25f + pSrc1[x] * 0.75f;
			pDst1[x] = pSrc2[x] * 0.25f + pSrc1[x] * 0.75f;
		}
	}

	set(pNewImage.release());
}
