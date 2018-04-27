// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include <assert.h>                         // assert()

#include "../ImageCompiler.h"               // CImageCompiler
#include "../ImageUserDialog.h"             // CImageUserDialog
#include "../ImageObject.h"                 // ImageToProcess

#include "IRCLog.h"                         // IRCLog
#include "FIR-Windows.h"                    // EWindowFunction

// we can't build debug-builds with the concurrency-runtime,
// as _CRT_DBG_MALLOC interferes with concurrency-runtime's alloca/freea
#if	!defined(_DEBUG) && !defined(_CRT_DBG_MALLOC)
#pragma warning(push)
#pragma warning(disable: 4355)  // warning C4355: 'this': used in base member initializer list
#include <ppl.h>
#pragma warning(pop)
#define PROCESS_IN_PARALLEL
#endif

///////////////////////////////////////////////////////////////////////////////////

void CImageCompiler::CreateMipMaps(
	ImageToProcess& image, 
	uint32 indwReduceResolution,
	const bool inbRemoveMips,
	const bool inbRenormalize) 
{
	const EPixelFormat srcPixelFormat = image.get()->GetPixelFormat();

	if (srcPixelFormat != ePixelFormat_A32B32G32R32F)
	{
		assert(0);
		image.set(0);
		return;
	}

	{
		const uint32 dwMinWidth = CPixelFormats::GetPixelFormatInfo(srcPixelFormat)->minWidth;
		const uint32 dwMinHeight = CPixelFormats::GetPixelFormatInfo(srcPixelFormat)->minHeight;

		if (dwMinWidth != 1 || dwMinHeight != 1)
		{
			assert(0);
			image.set(0);
			return;
		}
	}

	uint32 srcWidth, srcHeight, srcMips;
	image.get()->GetExtent(srcWidth, srcHeight, srcMips);

	if (srcMips != 1)
	{
		assert(0);
		RCLogError("%s called for a mipmapped image. Inform an RC programmer.", __FUNCTION__);
		image.set(0);
		return;
	}

	if (image.get()->GetCubemap() == ImageObject::eCubemap_Yes)
	{
		assert(0);
		RCLogError("%s called for a cubemap. CreateCubemapMipMaps() should be used instead. Inform an RC programmer.", __FUNCTION__);
		image.set(0);
		return;
	}

	// skipping unnecessary big MIPs
	if (m_bInternalPreview)
	{
		indwReduceResolution += m_pImageUserDialog->GetPreviewReduceResolution(srcWidth, srcHeight);

		if (m_pImageUserDialog->PreviewGenerationCanceled())
		{
			image.set(0);
			return;
		}
	}

	std::auto_ptr<ImageObject> pRet;
	pRet.reset(image.get()->AllocateImage(&m_Props, inbRemoveMips, indwReduceResolution));

	if (m_bInternalPreview)
	{
		m_Progress.Phase1();
	}

	uint32 dstMips = pRet->GetMipCount();

#ifdef PROCESS_IN_PARALLEL
	Concurrency::parallel_for(0U, dstMips, 1U, [&](uint32 dstMip)
#else
	for (uint32 dstMip = 0; dstMip < dstMips; ++dstMip)
#endif
	{
		RECT  prvRect;
		RECT* srcRect = NULL;
		RECT* dstRect = NULL;

		// Alpha coverage can't be maintained if not all of the image is calculated
		if (m_bInternalPreview && !m_Props.GetMaintainAlphaCoverage())
		{
			// Preview-rectangle calculation is based on the original size of the image
			prvRect = m_pImageUserDialog->GetPreviewRectangle(dstMip + indwReduceResolution);
			dstRect = &prvRect;

			// Initialize out-of-rectangle region with zeros
			uint32 dstWidth, dstHeight;
			char* pDestMem;
			uint32 dwDestPitch;

			pRet->GetImagePointer(dstMip, pDestMem, dwDestPitch);

			dstWidth = pRet->GetWidth(dstMip);
			dstHeight = pRet->GetHeight(dstMip);

			memset(pDestMem, 0, dwDestPitch * dstHeight);
		}

		pRet->FilterImage(&m_Props, image.get(), 0, int(dstMip), srcRect, dstRect);
		if (m_Props.GetMaintainAlphaCoverage())
		{
			pRet->TransferAlphaCoverage(&m_Props, image.get(), 0, int(dstMip));
		}

		if (m_bInternalPreview)
		{
			m_Progress.Phase3(srcHeight, srcHeight, dstMip + indwReduceResolution);
		}
	}
#ifdef PROCESS_IN_PARALLEL
	);
#endif

	if (inbRenormalize)
	{
		pRet->NormalizeVectors(0, dstMips);
	}

	image.set(pRet.release());
}

void ImageToProcess::CreateHighPass(uint32 dwMipDown)
{
	const EPixelFormat ePixelFormat = get()->GetPixelFormat();

	if (ePixelFormat != ePixelFormat_A32B32G32R32F)
	{
		assert(0);
		set(0);
		return;
	}

	uint32 dwWidth, dwHeight, dwMips;
	get()->GetExtent(dwWidth, dwHeight, dwMips);

	if (dwMipDown >= dwMips)
	{
		RCLogWarning("CImageCompiler::CreateHighPass can't go down %i MIP levels for high pass as there are not enough MIP levels available, going down by %i instead", dwMipDown, dwMips-1);
		dwMipDown = dwMips - 1;
	}

	std::auto_ptr<ImageObject> pRet(new ImageObject(dwWidth, dwHeight, dwMips, ePixelFormat, get()->GetCubemap()));
	assert(pRet->GetMipCount() == dwMips);
	pRet->CopyPropertiesFrom(*get());

	uint32 dstMips = pRet->GetMipCount();
	for (uint32 dstMip = 0; dstMip < dwMips; ++dstMip)
	{
		// linear interpolation
		pRet->FilterImage(eWindowFunction_Triangle, 0, 0.0f, 0.0f, get(), int(dwMipDown), int(dstMip), NULL, NULL);

		const uint32 pixelCountIn = get()->GetWidth(dstMip) * get()->GetHeight(dstMip);
		const uint32 pixelCountOut = pRet->GetWidth(dstMip) * pRet->GetHeight(dstMip);
		ColorF* pPixelsIn = get()->GetPixelsPointer<ColorF>(dstMip);
		ColorF* pPixelsOut = pRet->GetPixelsPointer<ColorF>(dstMip);
		for (uint32 i = 0; i < pixelCountIn; ++i)
		{
			pPixelsOut[i] = pPixelsIn[i] - pPixelsOut[i] + ColorF(0.5f, 0.5f, 0.5f, 0.5f);
			pPixelsOut[i].clamp(0.0f, 1.0f);
		}
	}

	for (uint32 dstMip = dwMips; dstMip < dstMips; ++dstMip)
	{
		// mips below the chosen highpass mip are grey
		const uint32 pixelCount = pRet->GetWidth(dstMip) * pRet->GetHeight(dstMip);
		ColorF* pPixels = pRet->GetPixelsPointer<ColorF>(dstMip);
		for (uint32 i = 0; i < pixelCount; ++i)
		{
			pPixels[i] = ColorF(0.5f, 0.5f, 0.5f, 1.0f);
		}
	}

	set(pRet.release());
}
