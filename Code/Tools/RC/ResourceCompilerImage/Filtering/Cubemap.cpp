// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"

#include "../ImageCompiler.h"               // CImageCompiler
#include "../ImageUserDialog.h"             // CImageUserDialog
#include "../ImageObject.h"                 // ImageToProcess

#include "IRCLog.h"                         // IRCLog
#include "ThreadUtils.h"                    // ThreadUtils
#include "StringHelpers.h"                  // StringHelpers

#include "Cubemap.h"
#include <CryString/UnicodeFunctions.h>

// we can't build debug-builds with the concurrency-runtime,
// as _CRT_DBG_MALLOC interferes with concurrency-runtime's alloca/freea
#if !defined(_DEBUG) && !defined(_CRT_DBG_MALLOC)
#pragma warning(push)
#pragma warning(disable: 4355)  // warning C4355: 'this': used in base member initializer list
#include <ppl.h>
#pragma warning(pop)
#define PROCESS_IN_PARALLEL
#endif

static ThreadUtils::CriticalSection s_atiCubemapLock;

///////////////////////////////////////////////////////////////////////////////////

static void AtiCubeMapGen_MessageOutputFunc(WCHAR* pTitle, WCHAR* pMessage)
{
	string t, m;
	Unicode::Convert(t, pTitle);
	Unicode::Convert(m, pTitle);

	RCLogWarning("ATI CubeMapGen: %s: %s", t.c_str(), m.c_str());
}

void CImageCompiler::CreateCubemapMipMaps(
	ImageToProcess& image,
	const SCubemapFilterParams& params,
	uint32 indwReduceResolution,
	const bool inbRemoveMips)
{
	const EPixelFormat srcPixelFormat = image.get()->GetPixelFormat();

	if (srcPixelFormat != ePixelFormat_A32B32G32R32F)
	{
		RCLogError("%s called for a non-float image. Inform an RC programmer.", __FUNCTION__);
		image.set(0);
		return;
	}

	{
		const uint32 dwMinWidth = CPixelFormats::GetPixelFormatInfo(srcPixelFormat)->minWidth;
		const uint32 dwMinHeight = CPixelFormats::GetPixelFormatInfo(srcPixelFormat)->minHeight;

		if (dwMinWidth != 1 || dwMinHeight != 1)
		{
			RCLogError("%s: unexpected min height/width. Inform an RC programmer.", __FUNCTION__);
			image.set(0);
			return;
		}
	}

	uint32 srcWidth, srcHeight, srcMips;
	image.get()->GetExtent(srcWidth, srcHeight, srcMips);

	if (srcMips != 1)
	{
		RCLogError("%s called for a mipmapped image. Inform an RC programmer.", __FUNCTION__);
		image.set(0);
		return;
	}

	if (image.get()->GetCubemap() != ImageObject::eCubemap_Yes)
	{
		RCLogError("%s called for an image which is not a cubemap. CreateMipMaps() should be used instead. Inform an RC programmer.", __FUNCTION__);
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
	for (uint32 dstMip = 0; dstMip < 1; ++dstMip)
	{
		if (m_bInternalPreview)
		{
			RECT prvRect;
			RECT dstRect;

			// Preview-rectangle calculation is based on the original size of the image
			prvRect = m_pImageUserDialog->GetPreviewRectangle(dstMip + indwReduceResolution);
			dstRect = prvRect;

			// Initialize out-of-rectangle region with zeros
			uint32 dstWidth, dstHeight;
			char* pDestMem;
			uint32 dwDestPitch;

			pRet->GetImagePointer(dstMip, pDestMem, dwDestPitch);

			dstWidth = pRet->GetWidth(dstMip);
			dstHeight = pRet->GetHeight(dstMip);

			memset(pDestMem, 0, dwDestPitch * dstHeight);
		}

		// Filter each cubemap's face independently to prevent color bleeding
		// across face-boundaries
		// TODO: we would have to clip the face's regions against the
		// preview-rectangle, currently all of the cubemap is calculated
#ifdef PROCESS_IN_PARALLEL
		Concurrency::parallel_for(0, 6, 1, [&](int iSide)
#else
		for (int iSide = 0; iSide < 6; ++iSide)
#endif
		{
			RECT srcRect;
			RECT dstRect;

			srcRect.left   = (iSide + 0) * (image.get()->GetWidth(0) / 6);
			dstRect.left   = (iSide + 0) * (       pRet->GetWidth(0) / 6);
			srcRect.right  = (iSide + 1) * (image.get()->GetWidth(0) / 6);
			dstRect.right  = (iSide + 1) * (       pRet->GetWidth(0) / 6);
			srcRect.top    = 0;
			dstRect.top    = 0;
			srcRect.bottom = image.get()->GetHeight(0);
			dstRect.bottom =        pRet->GetHeight(0);

			pRet->FilterImage(&m_Props, image.get(), int(0), int(dstMip), &srcRect, &dstRect);
		}
#ifdef PROCESS_IN_PARALLEL
		);
#endif

		if (m_bInternalPreview)
		{
			m_Progress.Phase3(srcHeight, srcHeight, dstMip + indwReduceResolution);
		}
	}

	SetErrorMessageCallback(AtiCubeMapGen_MessageOutputFunc);

	// clear cubemap processor for filtering cubemap
	m_AtiCubemanGen.Clear();

	// use background thread if we are using dialogs, else there is no need to create additional threads
	const bool bInteractive = m_Props.GetConfigAsBool("userdialog", false, true);
	m_AtiCubemanGen.m_NumFilterThreads = bInteractive ? 1 : 0;//max(1, min(m_CC.threads, CP_MAX_FILTER_THREADS));

	// input and output cubemap set to have save dimensions, 
	m_AtiCubemanGen.Init(pRet->GetWidth(0) / 6, pRet->GetHeight(0), dstMips, 4);

	// Load the 6 faces of the input cubemap and copy them into the cubemap processor
	{
		char* pMem;
		uint32 nPitch;
		pRet->GetImagePointer(0, pMem, nPitch);

		const uint32 nSidePitch = nPitch / 6;
		if (nPitch % 6 != 0)
		{
			RCLogError("%s: invalid pitch: %d. Inform an RC programmer.", __FUNCTION__, (int)nPitch);
			image.set(0);
			return;
		}

#ifdef PROCESS_IN_PARALLEL
		Concurrency::parallel_for(0, 6, 1, [&](int iSide)
#else
		for (int iSide = 0; iSide < 6; ++iSide)
#endif
		{
			m_AtiCubemanGen.SetInputFaceData(
				iSide,                      // FaceIdx,
				CP_VAL_FLOAT32,             // SrcType,
				4,                          // SrcNumChannels,
				nPitch,                     // SrcPitch,
				pMem + nSidePitch * iSide,  // SrcDataPtr,
				1000000.0f,                 // MaxClamp,
				1.0f,                       // Degamma,
				1.0f);                      // Scale
		}
#ifdef PROCESS_IN_PARALLEL
		);
#endif
	}

	if (m_AtiCubemanGen.m_NumFilterThreads > 0)
	{
		s_atiCubemapLock.Lock();
	}

	//Filter cubemap
	m_AtiCubemanGen.InitiateFiltering(
		params.BaseFilterAngle,         //BaseFilterAngle, 
		params.InitialMipAngle,         //InitialMipAngle, 
		params.MipAnglePerLevelScale,   //MipAnglePerLevelScale, 
		params.FilterType,              //FilterType, 
		params.FixupType,               //FixupType, 
		params.FixupWidth,              //FixupWidth, 
		params.bUseSolidAngle,          //bUseSolidAngle,
		params.BRDFGlossScale,          //GlossScale,
		params.BRDFGlossBias,           //GlossBias
		params.SampleCountGGX);         //SampleCountGGX

	if (m_AtiCubemanGen.m_NumFilterThreads > 0)
	{
		s_atiCubemapLock.Unlock();

		//Report status of filtering , and loop until filtering is complete
		while (m_AtiCubemanGen.GetStatus() == CP_STATUS_PROCESSING)
		{
			// allow user to abort the operation
			if (bInteractive && (::GetKeyState(VK_ESCAPE) & 0x80))
			{
				m_AtiCubemanGen.TerminateActiveThreads();
			}

			Sleep(100);
		}
	}

	if (m_AtiCubemanGen.m_NumMipLevels != dstMips)
	{
		RCLogError("%s: invalid mip level count. Inform an RC programmer.", __FUNCTION__);
		image.set(0);
		return;
	}

	// Download data into it
#ifdef PROCESS_IN_PARALLEL
	Concurrency::parallel_for(0, 6, 1, [this, &dstMips, &pRet](int iSide)
#else
	for (int iSide = 0; iSide < 6; ++iSide)
#endif
	{
		for (int dstMip = 0; dstMip < dstMips; ++dstMip)
		{
			char* pMem;
			uint32 nPitch;
			pRet->GetImagePointer(dstMip, pMem, nPitch);
			const uint32 nSidePitch = nPitch / 6;
			char* const pDestMem = pMem + iSide * nSidePitch;
			m_AtiCubemanGen.GetOutputFaceData(iSide, dstMip, CP_VAL_FLOAT32, 4, nPitch, pDestMem, 1.0f, 1.0f);
		}
	}
#ifdef PROCESS_IN_PARALLEL
	);
#endif

	image.set(pRet.release());
}
