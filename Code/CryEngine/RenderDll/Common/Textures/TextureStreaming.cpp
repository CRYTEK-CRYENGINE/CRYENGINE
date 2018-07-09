// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

/*=============================================================================
   TextureStreaming.cpp : Common Texture Streaming manager implementation.

   Revision history:
* Created by Honich Andrey
   - 19:8:2008   12:14 : Refactored by Anton Kaplanyan

   =============================================================================*/

#include "StdAfx.h"
#include "../CommonRender.h"
#include "Image/DDSImage.h"
#include <CryString/StringUtils.h>                // stristr()
#include <CryMemory/ILocalMemoryUsage.h>
#include <CryThreading/IJobManager_JobDelegator.h>

#include "TextureStreamPool.h"
#include "TextureHelpers.h"

#include "PlanningTextureStreamer.h"

#include "StatoscopeTextureStreaming.h"

// checks for MT-safety of called functions
#if !defined(CHK_RENDTH)
	#define CHK_RENDTH assert(gRenDev->m_pRT->IsRenderThread(true))
#endif
#if !defined(CHK_MAINTH)
	#define CHK_MAINTH assert(gRenDev->m_pRT->IsMainThread(true))
#endif
#if !defined(CHK_MAINORRENDTH)
	#define CHK_MAINORRENDTH assert(gRenDev->m_pRT->IsMainThread(true) || gRenDev->m_pRT->IsRenderThread(true) || gRenDev->m_pRT->IsLevelLoadingThread(true))
#endif

bool CTexture::s_bStreamingFromHDD = true;
CTextureArrayAlloc<STexStreamInState, CTexture::MaxStreamTasks> CTexture::s_StreamInTasks;
CTextureArrayAlloc<STexStreamPrepState*, CTexture::MaxStreamPrepTasks> CTexture::s_StreamPrepTasks;

#ifdef TEXSTRM_ASYNC_TEXCOPY
CTextureArrayAlloc<STexStreamOutState, CTexture::MaxStreamTasks> CTexture::s_StreamOutTasks;
#endif

volatile int CTexture::s_nBytesSubmittedToStreaming = { 0 };
volatile int CTexture::s_nMipsSubmittedToStreaming = { 0 };
volatile int CTexture::s_nNumStreamingRequests = { 0 };
int CTexture::s_nBytesRequiredNotSubmitted = 0;

#if !defined (_RELEASE) || defined(ENABLE_STATOSCOPE_RELEASE)
int CTexture::s_TextureUpdates = 0;
float CTexture::s_TextureUpdatesTime = 0.0f;
int CTexture::s_TexturesUpdatedRendered = 0;
float CTexture::s_TextureUpdatedRenderedTime = 0.0f;
int CTexture::s_StreamingRequestsCount = 0;
float CTexture::s_StreamingRequestsTime = 0.0f;
#endif

#ifdef ENABLE_TEXTURE_STREAM_LISTENER
ITextureStreamListener* CTexture::s_pStreamListener;
#endif

bool CTexture::s_bStreamDontKeepSystem = false;

int CTexture::s_nTexturesDataBytesLoaded = 0;
volatile int CTexture::s_nTexturesDataBytesUploaded = 0;
int CTexture::s_nStatsAllocFails;
bool CTexture::s_bOutOfMemoryTotally;
volatile size_t CTexture::s_nStatsStreamPoolInUseMem;
volatile size_t CTexture::s_nStatsStreamPoolBoundMem;
volatile size_t CTexture::s_nStatsStreamPoolBoundPersMem;
volatile size_t CTexture::s_nStatsCurManagedNonStreamedTexMem = { 0 };
volatile size_t CTexture::s_nStatsCurDynamicTexMem = { 0 };
volatile size_t CTexture::s_nStatsStreamPoolWanted = { 0 };
bool CTexture::s_bStatsComputeStreamPoolWanted = false;
std::vector<CTexture::WantedStat>* CTexture::s_pStatsTexWantedLists = NULL;

ITextureStreamer* CTexture::s_pTextureStreamer;

CryCriticalSection CTexture::s_streamFormatLock;
SStreamFormatCode CTexture::s_formatCodes[256];
uint32 CTexture::s_nFormatCodes = 1;
CTexture::TStreamFormatCodeKeyMap CTexture::s_formatCodeMap;

bool CTexture::IsStreamingInProgress()
{
	return s_StreamInTasks.GetNumLive() > 0;
}

#ifdef TEXSTRM_ASYNC_TEXCOPY
void STexStreamInState::CopyMips()
{
	FUNCTION_PROFILER_RENDERER();

	CTexture* tp = m_pTexture;

	if (!m_bAborted)
	{
		if (tp->m_pFileTexMips->m_pPoolItem)
		{
			const int nNewMipOffset = tp->m_nMinMipVidUploaded - m_nHigherUploadedMip;
			const int nNumMips = tp->GetNumMips() - tp->m_nMinMipVidUploaded;

			if (0)
			{
			}
	#if CRY_PLATFORM_DURANGO && (CRY_RENDERER_DIRECT3D >= 110) && (CRY_RENDERER_DIRECT3D < 120)
			else if (!gRenDev->m_pRT->IsRenderThread(true))
			{
				m_copyMipsFence = CTexture::StreamCopyMipsTexToTex_MoveEngine(tp->m_pFileTexMips->m_pPoolItem, 0, m_pNewPoolItem, 0 + nNewMipOffset, nNumMips);
			}
	#endif
			else
			{
				CTexture::StreamCopyMipsTexToTex(tp->m_pFileTexMips->m_pPoolItem, 0, m_pNewPoolItem, 0 + nNewMipOffset, nNumMips);
			}

			m_bValidLowMips = true;
		}
	}
	else
	{
		m_bValidLowMips = true;
	}
}

void STexStreamOutState::Reset()
{
	// if we have streaming error, release new pool item
	if (m_pNewPoolItem)
		CTexture::s_pPoolMgr->ReleaseItem(m_pNewPoolItem);

	m_pNewPoolItem = NULL;
	m_pTexture = 0;
	m_bDone = false;
	m_bAborted = false;

	#if CRY_PLATFORM_DURANGO
	m_copyFence = 0;
	#endif
}

bool STexStreamOutState::TryCommit()
{
	if (m_bDone)
	{
	#if CRY_PLATFORM_DURANGO && (CRY_RENDERER_DIRECT3D >= 110) && (CRY_RENDERER_DIRECT3D < 120)
		if (!m_pTexture->StreamOutCheckComplete_Durango(*this))
			return false;
	#endif

		if (!m_bAborted)
		{
			if (m_nStartMip < MAX_MIP_LEVELS)
			{
				m_pTexture->StreamAssignPoolItem(m_pNewPoolItem, m_nStartMip);
				m_pNewPoolItem = NULL;

				m_pTexture->SetWasUnload(false);
			}
			else
			{
				// Stream unload
				m_pTexture->ReleaseDeviceTexture(true, true);
				m_pTexture->SetWasUnload(true);
			}
		}

		m_pTexture->SetStreamingInProgress(CTexture::InvalidStreamSlot);
		m_pTexture->Release();
		m_pTexture = NULL;

		return true;
	}

	return false;
}
#endif

void CTexture::StreamReleaseMipsData(int nStartMip, int nEndMip)
{
	assert(m_pFileTexMips);
	assert(nStartMip <= nEndMip);
	nEndMip = min(nEndMip, m_nMips - 1);
	nStartMip = min(nStartMip, nEndMip);
	const int nSides = StreamGetNumSlices();
	for (int i = 0; i < nSides; i++)
		for (int j = nStartMip; j <= nEndMip; j++)
			m_pFileTexMips->m_pMipHeader[j].m_Mips[i].Free();
}

STexStreamInState::STexStreamInState()
{
	m_pNewPoolItem = NULL;
#if defined(TEXSTRM_DEFERRED_UPLOAD)
	m_pCmdList = NULL;
#endif
	Reset();
}

void STexStreamInState::Reset()
{
#if DEVRES_USE_PINNING
	if (m_bPinned)
	{
		m_pNewPoolItem->m_pDevTexture->Unpin();
		m_bPinned = false;
	}
#endif

	if (m_pNewPoolItem)
	{
		CTexture::s_pPoolMgr->ReleaseItem(m_pNewPoolItem);

		m_pNewPoolItem = NULL;
	}

#if defined(TEXSTRM_DEFERRED_UPLOAD)
	SAFE_RELEASE(m_pCmdList);
#endif

	for (size_t i = 0, c = m_nLowerUploadedMip - m_nHigherUploadedMip + 1; i != c; ++i)
		m_pStreams[i] = NULL;

	memset(&m_pTexture, 0, (char*)(this + 1) - (char*)&m_pTexture);
}

// streaming thread
void STexStreamInState::StreamAsyncOnComplete(IReadStream* pStream, unsigned nError)
{
	PROFILE_FRAME(Texture_StreamAsyncOnComplete);

	CTexture* tp = m_pTexture;

	int nMip = (int)pStream->GetUserData();
	STexStreamInMipState& mipState = m_mips[nMip];

	if (!nError && tp->m_pFileTexMips)
	{
#if defined(TEXSTRM_ASYNC_UPLOAD)
		tp->StreamUploadMip(pStream, nMip, m_nHigherUploadedMip, m_pNewPoolItem, mipState);
		mipState.m_bUploaded = true;
#elif CRY_PLATFORM_DURANGO && (CRY_RENDERER_DIRECT3D >= 110) && (CRY_RENDERER_DIRECT3D < 120)
		const void* pTextureData = pStream->GetBuffer();

		// If we've deferred the D3D texture initialisation, construct it now
		m_pNewPoolItem->m_pDevTexture->InitD3DTexture();

		if (tp->m_eSrcTileMode == eTM_None && !CTexture::IsInPlaceFormat(tp->m_eSrcFormat))
		{
			// Untiled, non-native, expand into mip headers
			tp->StreamExpandMip(pTextureData, nMip, m_nHigherUploadedMip, mipState.m_nSideDelta);
			mipState.m_bExpanded = true;

			pTextureData = NULL;
		}

		if (!mipState.m_bStreamInPlace)
		{
			// Not in place, prepare for upload
			tp->StreamUploadMip_Durango(pTextureData, nMip, m_nHigherUploadedMip, m_pNewPoolItem, mipState);
		}
#else
		if (!mipState.m_bStreamInPlace)
		{
			tp->StreamExpandMip(pStream->GetBuffer(), nMip, m_nHigherUploadedMip, mipState.m_nSideDelta);
			mipState.m_bExpanded = true;
		}
		else
		{
			mipState.m_bUploaded = true;
		}
#endif

		// Update the cached media type to optimise future requests
		EStreamSourceMediaType eMT = pStream->GetMediaType();
		int nAbsMip = m_nHigherUploadedMip + nMip;
		tp->m_pFileTexMips->m_pMipHeader[nAbsMip].m_eMediaType = eMT;
	}
	else
	{
		m_bAborted = true;
	}

	pStream->FreeTemporaryMemory(); // We don't need internal stream loaded buffer anymore.

	if (tp->m_pFileTexMips)
	{
		int nChunkSize = tp->m_pFileTexMips->m_pMipHeader[nMip + m_nHigherUploadedMip].m_SideSize * tp->GetNumSides();
		CryInterlockedAdd(&CTexture::s_nBytesSubmittedToStreaming, -nChunkSize);
	}
	CryInterlockedDecrement(&CTexture::s_nMipsSubmittedToStreaming);
	assert(CTexture::s_nBytesSubmittedToStreaming >= 0);

	const int nRef = CryInterlockedDecrement(&m_nAsyncRefCount);

	// Check to see if this is the last mip (and thus owns the job)
	if (nRef == 0)
	{
#if DEVRES_USE_PINNING
		// Should not be durango, it will hold pin until commit, to cover mip copy
		if (m_bPinned)
		{
			m_pNewPoolItem->m_pDevTexture->Unpin();
			m_bPinned = false;
		}
#endif

		if (!m_bAborted)
		{
#if CRY_PLATFORM_DURANGO && (CRY_RENDERER_DIRECT3D >= 110) && (CRY_RENDERER_DIRECT3D < 120)
			tp->StreamUploadMips_Durango(m_nHigherUploadedMip, m_nLowerUploadedMip - m_nHigherUploadedMip + 1, m_pNewPoolItem, *this);

			if (CTexture::s_bStreamDontKeepSystem)
			{
				tp->StreamReleaseMipsData(m_nHigherUploadedMip, m_nLowerUploadedMip);
			}
#endif

#if defined(TEXSTRM_DEFERRED_UPLOAD)

			ID3D11CommandList* pCmdList = tp->StreamCreateDeferred(m_nHigherUploadedMip, m_nLowerUploadedMip, m_pNewPoolItem, tp->m_pFileTexMips->m_pPoolItem);

			if (pCmdList)
			{
				m_pCmdList = pCmdList;
				m_bValidLowMips = true;

				for (int i = 0, c = m_nLowerUploadedMip - m_nHigherUploadedMip + 1; i != c; ++i)
					m_mips[i].m_bExpanded = false;

				if (CTexture::s_bStreamDontKeepSystem)
				{
					tp->StreamReleaseMipsData(m_nHigherUploadedMip, m_nLowerUploadedMip);
				}
			}

#endif

#if defined(TEXSTRM_ASYNC_TEXCOPY)
			if (!m_bValidLowMips && tp->CanAsyncCopy())
			{
				CopyMips();
			}
#endif
		}

#ifndef _RELEASE
		// collect statistics
		if (pStream->GetParams().nSize > 1024)
			CTexture::s_nStreamingThroughput += pStream->GetParams().nSize;
		const CTimeValue currentTime = iTimer->GetAsyncTime();
		if (currentTime - m_fStartTime > .01f)  // avoid measurement errors for small textures
			CTexture::s_nStreamingTotalTime += currentTime.GetSeconds() - m_fStartTime;
#endif

		m_bAllStreamsComplete = true;
	}
}

bool STexStreamInState::TryCommit()
{
	PROFILE_FRAME(Texture_StreamOnComplete_Render);

	CHK_RENDTH;

	CTexture* tp = m_pTexture;

	if (!m_bAborted)
	{
		STexPoolItem*& pNewPoolItem = m_pNewPoolItem;

#if !defined(TEXSTRM_ASYNC_UPLOAD) && !CRY_PLATFORM_DURANGO
		for (size_t i = 0, c = m_nLowerUploadedMip - m_nHigherUploadedMip + 1; i != c; ++i)
		{
			STexStreamInMipState& mipState = m_mips[i];

			if (mipState.m_bExpanded)
			{
				mipState.m_bUploaded = true;
				mipState.m_bExpanded = false;

				tp->StreamCopyMipsTexToMem(m_nHigherUploadedMip + i, m_nHigherUploadedMip + i, true, pNewPoolItem);
				if (CTexture::s_bStreamDontKeepSystem)
					tp->StreamReleaseMipsData(m_nHigherUploadedMip + i, m_nHigherUploadedMip + i);
			}
		}
#endif

#ifdef TEXSTRM_DEFERRED_UPLOAD
		if (m_pCmdList)
		{
			tp->StreamApplyDeferred(m_pCmdList);
			m_pCmdList->Release();
			m_pCmdList = NULL;
		}
#endif

#if defined(TEXSTRM_COMMIT_COOLDOWN)
		if ((m_nStallFrames++) < 4)
			return false;
#endif

#if CRY_PLATFORM_DURANGO && (CRY_RENDERER_DIRECT3D >= 110) && (CRY_RENDERER_DIRECT3D < 120)
		if (!tp->StreamInCheckTileComplete_Durango(*this))
			return false;
#endif

		if (!m_bValidLowMips)
		{
			STexPoolItem* pCurItem = tp->m_pFileTexMips->m_pPoolItem;

			if (pCurItem)
			{
				// it is a sync operation anyway, so we do it in the render thread
				// restore already loaded mips
				const int nNumMips = pCurItem->m_pOwner->m_nMips;
				const int nNewMipOffset = pNewPoolItem->m_pOwner->m_nMips - nNumMips;

				CTexture::StreamCopyMipsTexToTex(pCurItem, 0, pNewPoolItem, nNewMipOffset, nNumMips);
			}
			else
			{
				m_pTexture->StreamCopyMipsTexToMem(m_pTexture->GetNumMips() - m_pTexture->GetNumPersistentMips(), m_pTexture->GetNumMips() - 1, true, pNewPoolItem);
				if (CTexture::s_bStreamDontKeepSystem)
					m_pTexture->StreamReleaseMipsData(m_pTexture->GetNumMips() - m_pTexture->GetNumPersistentMips(), m_pTexture->GetNumMips() - 1);
			}
#if CRY_PLATFORM_DURANGO && (CRY_RENDERER_DIRECT3D >= 110) && (CRY_RENDERER_DIRECT3D < 120)
			m_copyMipsFence = CTexture::StreamInsertFence();
#endif

			m_bValidLowMips = true;
		}

#if CRY_PLATFORM_DURANGO && (CRY_RENDERER_DIRECT3D >= 110) && (CRY_RENDERER_DIRECT3D < 120)
		if (!tp->StreamInCheckCopyComplete_Durango(*this))
			return false;
#endif

#if DEVRES_USE_PINNING
		if (m_bPinned)
		{
			m_pNewPoolItem->m_pDevTexture->Unpin();
			m_bPinned = false;
		}
#endif

		if (pNewPoolItem)
		{
			if (CRenderer::CV_r_texturesstreamingmipfading)
			{
				tp->m_fCurrentMipBias = tp->m_fCurrentMipBias + float(m_nLowerUploadedMip - m_nHigherUploadedMip + 1);
			}

			// bind new texture
			const int nNewNumMips = m_nHigherUploadedMip;
			tp->StreamAssignPoolItem(pNewPoolItem, m_nActivateMip);
			pNewPoolItem = NULL;
			tp->SetWasUnload(false);
		}
	}
	else
	{
		if (CTexture::s_bStreamDontKeepSystem)
			tp->StreamReleaseMipsData(m_nHigherUploadedMip, m_nLowerUploadedMip);
	}

	tp->SetStreamingInProgress(CTexture::InvalidStreamSlot);

	m_pTexture->Release();
	m_pTexture = NULL;

	CTexture::StreamValidateTexSize();

	return true;
}

bool STexStreamPrepState::Commit()
{
	_smart_ptr<CImageFile> pNextImage;

	if (!m_bFailed)
	{
		if (m_pImage)
		{
			if (m_pTexture->IsStreamed())
			{
				if (m_pTexture->StreamPrepare(&*m_pImage))
				{
					m_bNeedsFinalise = true;
				}
				else
				{
					m_bCompleted = false;

					// StreamPrepare failed, so presumably the image can't be streamed. Since we only have an image assuming it was streamed,
					// load it again, with all mips.
					// StreamPrepare failure will mark the texture as non-streamable.
					pNextImage = CImageFile::mfStream_File(m_pImage->mfGet_filename(), m_pImage->mfGet_Flags() & ~FIM_STREAM_PREPARE, this);
				}
			}
			else
			{
				m_pTexture->Load(&*m_pImage);
			}
		}

		if (m_bNeedsFinalise)
		{
			if (m_pTexture->IsStreamed())
				m_bNeedsFinalise = !m_pTexture->StreamPrepare_Finalise(true);
		}
	}
	else
	{
		m_pTexture->SetNoTexture();
	}

	m_pImage = pNextImage;

	if (!m_pImage && !m_bNeedsFinalise && m_pTexture)
	{
		m_pTexture->PostCreate();
	}

	return !m_pImage && !m_bNeedsFinalise;
}

void STexStreamPrepState::OnImageFileStreamComplete(CImageFile* pImFile)
{
	if (!pImFile)
	{
		m_pImage = NULL;
		m_bFailed = true;
	}

	m_bCompleted = true;
}

int CTexture::StreamCalculateMipsSigned(float fMipFactor) const
{
	return StreamCalculateMipsSignedFP(fMipFactor) >> 8;
}

int CTexture::GetStreamableMipNumber() const
{
	assert(IsStreamed());
	return max(0, m_nMips - m_CacheFileHeader.m_nMipsPersistent);
}

bool CTexture::IsStreamedIn(const int nMinPrecacheRoundIds[MAX_STREAM_PREDICTION_ZONES]) const
{
	if (IsStreamed())
	{
		for (int nZoneIdx = 0; nZoneIdx < MAX_STREAM_PREDICTION_ZONES; ++nZoneIdx)
		{
			if (m_streamRounds[nZoneIdx].nRoundUpdateId < nMinPrecacheRoundIds[nZoneIdx])
				return false;
		}

		int nMinMip = s_pTextureStreamer->GetMinStreamableMipWithSkip();
		return max(GetRequiredMip(), nMinMip) >= CTexture::GetMinLoadedMip();
	}
	return true;
}

int CTexture::GetStreamableMemoryUsage(int nStartMip) const
{
	assert(IsStreamed());
	if (m_pFileTexMips == NULL)
	{
		assert(0);
		return 0;
	}

	return m_pFileTexMips->m_pMipHeader[nStartMip].m_SideSizeWithMips;
}

void CTexture::SetMinLoadedMip(int nMinMip)
{
#ifdef ENABLE_TEXTURE_STREAM_LISTENER
	if (m_nMinMipVidUploaded != nMinMip)
	{
		ITextureStreamListener* pListener = s_pStreamListener;
		if (pListener)
			pListener->OnTextureHasMip(this, min(nMinMip, (int)m_nMips));
	}
#endif

	m_nMinMipVidUploaded = nMinMip;
}

const bool CTexture::IsParticularMipStreamed(float fMipFactor) const
{
	if (!IsStreamed())
		return true;

	bool bHighPriority = false;
	if (m_pFileTexMips)
		bHighPriority = m_bStreamHighPriority != 0;
	int nMipClamp = (s_bStreamingFromHDD || bHighPriority) ? 0 : CRenderer::CV_r_TexturesStreamingMipClampDVD;
	const int nMip = max(nMipClamp, CTexture::StreamCalculateMipsSigned(fMipFactor));
	return m_nMinMipVidUploaded <= nMip;
}

void CTexture::PrecacheAsynchronously(float fMipFactor, int nFlags, int nUpdateId, int nCounter)
{
	if (!IsStreamed())
		return;   // already done

	s_pTextureStreamer->UpdateMip(this, fMipFactor, nFlags, nUpdateId, nCounter);

	// for distance streaming it's just the same as update rendering distance
	StreamLoadFromCache(nFlags);
}

void CTexture::StreamLoadFromCache(const int Flags)
{
	if (IsUnloaded())
	{
		if (!StreamPrepare(false))
		{
			// ignore error for optional attached alpha channel
			if (!m_bNoTexture && !(m_eFlags & FT_ALPHA) && (m_eFlags & FT_FROMIMAGE))
			{
				bool bRes = Reload();
				assert(bRes);
			}
		}
	}
}

bool CTexture::StreamPrepare(bool bFromLoad)
{
	CHK_MAINORRENDTH;

	if (!IsStreamable())
		return false;

	PROFILE_FRAME(Texture_StreamPrepare);

	CRY_DEFINE_ASSET_SCOPE("Texture", m_sAssetScopeName);

	// release the old texture
	if (m_pDevTexture)
		ReleaseDeviceTexture(false);

	if (!m_pFileTexMips)
	{
		const char* szExt = PathUtil::GetExt(m_SrcName.c_str());
		if (szExt != 0 && (!stricmp(szExt, "tif") || !stricmp(szExt, "hdr")) && !gEnv->pCryPak->IsFileExist(m_SrcName.c_str()))
		{
			m_SrcName = PathUtil::ReplaceExtension(m_SrcName, "dds");
			if (!gEnv->pCryPak->IsFileExist(m_SrcName))
				return false;
		}

#if !defined(_RELEASE)
		if ((m_eFlags & FT_TEX_NORMAL_MAP) && !TextureHelpers::VerifyTexSuffix(EFTT_NORMALS, m_SrcName))
		{
			FileWarning(m_SrcName.c_str(), "Normal map should have '%s' suffix in filename", TextureHelpers::LookupTexSuffix(EFTT_NORMALS));
		}
#endif

		if (m_bPostponed)
		{
			if (CTexture::s_pTextureStreamer->BeginPrepare(this, m_SrcName.c_str(), ((m_eFlags & FT_ALPHA) ? FIM_ALPHA : 0) | FIM_STREAM_PREPARE))
				return true;
		}

		uint32 nImageFlags = FIM_STREAM_PREPARE
		                     | ((m_eFlags & FT_ALPHA) ? FIM_ALPHA : 0)
		                     | ((m_eFlags & FT_STREAMED_PREPARE) ? FIM_READ_VIA_STREAMS : 0);

		_smart_ptr<CImageFile> pIM(CImageFile::mfLoad_file(m_SrcName, nImageFlags));
		if (!pIM || !StreamPrepare(&*pIM))
			return false;
	}

	return StreamPrepare_Finalise(bFromLoad);
}

bool CTexture::StreamPrepare(CImageFile* pIM)
{
	if (!pIM)
		return false;

	// TODO: support eTT_2DArray and eTT_CubeArray
	int nWidth = pIM->mfGet_width();
	int nHeight = pIM->mfGet_height();
	int nDepth = pIM->mfGet_depth();
	int nMips = pIM->mfGet_numMips();
	ETEX_Type eTT = pIM->mfGet_NumSides() == 1 ? eTT_2D : eTT_Cube;
	int nSides = eTT != eTT_Cube ? 1 : 6;
	ETEX_Format eSrcFormat = pIM->mfGetFormat();
	ETEX_Format eDstFormat = FormatFixup(eSrcFormat);
	const ColorF& cMinColor = pIM->mfGet_minColor();
	const ColorF& cMaxColor = pIM->mfGet_maxColor();
	ETEX_TileMode eSrcTileMode = pIM->mfGetTileMode();

#ifndef _RELEASE
	if (eSrcTileMode != eTM_None)
	{
		if (eSrcFormat != eDstFormat)
			__debugbreak();
	}
#endif

	int nMipsPersistent = max(pIM->mfGet_numPersistantMips(), DDSSplitted::GetNumLastMips(nWidth, nHeight, nMips, nSides, eSrcFormat, (m_eFlags & FT_ALPHA) ? FIM_ALPHA : 0));

	m_eFlags &= ~(FT_SPLITTED | FT_HAS_ATTACHED_ALPHA | FT_DONT_STREAM | FT_FROMIMAGE);

	bool bStreamable = IsStreamable(m_eFlags, eTT);

	// Can't stream volume textures and textures without mips
	if (eDstFormat == eTF_Unknown || nDepth > 1 || nMips < 2)
		bStreamable = false;

	if ((nWidth <= DDSSplitted::etexLowerMipMaxSize || nHeight <= DDSSplitted::etexLowerMipMaxSize) || nMips <= nMipsPersistent || nMipsPersistent == 0)
		bStreamable = false;

	const SPixFormat* pPF = CTexture::GetPixFormat(eDstFormat);
	if (!pPF && !DDSFormats::IsNormalMap(eDstFormat)) // special case for 3DC and CTX1
	{
		assert(0);
		gEnv->pLog->LogError("Failed to load texture %s': format '%s' is not supported", NameForTextureFormat(m_eDstFormat), m_SrcName.c_str());
		bStreamable = false;
	}

	if (!bStreamable)
	{
		if (m_pFileTexMips)
		{
			Unlink();
			StreamState_ReleaseInfo(this, m_pFileTexMips);
			m_pFileTexMips = NULL;
		}
		m_eFlags |= FT_DONT_STREAM;
		m_bStreamed = false;
		m_bStreamPrepared = false;
		SetWasUnload(false);
		SetStreamingInProgress(InvalidStreamSlot);
		m_bStreamRequested = false;
		m_bNoTexture = false;
		return false;
	}

	// Can't fail from this point on - commit everything.

	STexCacheFileHeader& fh = m_CacheFileHeader;
	if (pIM->mfGet_Flags() & FIM_SPLITTED)
		m_eFlags |= FT_SPLITTED;
	if (pIM->mfGet_Flags() & FIM_HAS_ATTACHED_ALPHA)
		m_eFlags |= FT_HAS_ATTACHED_ALPHA;      // if the image has alpha attached we store this in the CTexture

	// copy image properties
	m_nWidth = nWidth;
	m_nHeight = nHeight;
	m_nDepth = nDepth;
	m_nMips = nMips;
	m_eTT = eTT;
	m_nArraySize = nSides;
	m_eSrcFormat = eSrcFormat;
	m_eDstFormat = eDstFormat;
	m_eFlags |= FT_FROMIMAGE;
	m_bUseDecalBorderCol = (pIM->mfGet_Flags() & FIM_DECAL) != 0;
	m_bIsSRGB = (pIM->mfGet_Flags() & FIM_SRGB_READ) != 0;
	m_SrcName = pIM->mfGet_filename();
	assert(!m_pFileTexMips);
	m_pFileTexMips = StreamState_AllocateInfo(m_nMips);
	m_nStreamingPriority = 0;
	SetMinLoadedMip(MAX_MIP_LEVELS);
	m_nMinMipVidActive = MAX_MIP_LEVELS;
	m_fCurrentMipBias = 0.0f;
	m_cMinColor = cMinColor;
	m_cMaxColor = cMaxColor;
	m_cClearColor = ColorF(0.0f, 0.0f, 0.0f, 1.0f);
	m_eSrcTileMode = eSrcTileMode;
	m_bStreamed = true;

	// base range after normalization, fe. [0,1] for 8bit images, or [0,2^15] for RGBE/HDR data
	if (CImageExtensionHelper::IsDynamicRange(m_eSrcFormat))
	{
		m_cMinColor /= m_cMaxColor.a;
		m_cMaxColor /= m_cMaxColor.a;
	}

	// allocate and fill up streaming auxiliary structures
	fh.m_nSides = nSides;
	for (int iMip = 0; iMip < m_nMips; ++iMip)
	{
		m_pFileTexMips->m_pMipHeader[iMip].m_Mips = new SMipData[fh.m_nSides];
	}

	m_pFileTexMips->m_desc = pIM->mfGet_DDSDesc();

	for (int iSide = 0; iSide < nSides; ++iSide)
	{
		for (int iMip = 0; iMip < m_nMips; ++iMip)
		{
			m_pFileTexMips->m_pMipHeader[iMip].m_SideSize = CTexture::TextureDataSize(
				max(1, m_nWidth  >> iMip),
				max(1, m_nHeight >> iMip),
				max(1, m_nDepth  >> iMip),
				1, 1,
				m_eDstFormat,
				eSrcTileMode);
		}
	}

#if defined(TEXSTRM_STORE_DEVSIZES)
	for (int iMip = 0; iMip < m_nMips; ++iMip)
	{
		m_pFileTexMips->m_pMipHeader[iMip].m_DevSideSizeWithMips = CTexture::TextureDataSize(
			max(1, m_nWidth  >> iMip),
			max(1, m_nHeight >> iMip),
			max(1, m_nDepth  >> iMip),
			m_nMips - iMip,
			StreamGetNumSlices(),
			m_eDstFormat,
			eSrcTileMode);
	}
#endif

	m_pFileTexMips->m_nSrcStart = pIM->mfGet_StartSeek();

	for (int iMip = 0; iMip < m_nMips; iMip++)
	{
		m_pFileTexMips->m_pMipHeader[iMip].m_SideSizeWithMips = 0;
		for (int j = iMip; j < m_nMips; j++)
			m_pFileTexMips->m_pMipHeader[iMip].m_SideSizeWithMips += m_pFileTexMips->m_pMipHeader[j].m_SideSize;
	}

	// set up pixel format and check if it's supported for
	if (pPF)
		m_bIsSRGB &= !!(pPF->Options & FMTSUPPORT_SRGB);
	else
		m_bIsSRGB = false;

	fh.m_nMipsPersistent = nMipsPersistent;

	assert(m_eDstFormat != eTF_Unknown);

	StreamPrepare_Platform();

	PostCreate();

	// Always load lowest nMipsPersistent mips synchronously
	byte* buf = NULL;
	if (m_nMips > 1)
	{
		int nSyncStartMip = -1;
		int nSyncEndMip = -1;
		int nStartLowestM = max(0, m_nMips - m_CacheFileHeader.m_nMipsPersistent);
		if (nStartLowestM < m_nMinMipVidUploaded)
		{
			nSyncStartMip = nStartLowestM;
			nSyncEndMip = m_nMips - 1;
		}

		int nOffs = 0;
		assert(nSyncStartMip <= nSyncEndMip);

		const bool bIsDXT = CTexture::IsBlockCompressed(eDstFormat);
		const int nMipAlign = bIsDXT ? 4 : 1;

		for (int iSide = 0; iSide < nSides; iSide++)
		{
			nOffs = 0;
			for (int iMip = nSyncStartMip, nMipW = max(1, m_nWidth >> iMip), nMipH = max(1, m_nHeight >> iMip);
			     iMip <= nSyncEndMip;
			     ++iMip, nMipW = max(1, nMipW >> 1), nMipH = max(1, nMipH >> 1))
			{
				STexMipHeader& mh = m_pFileTexMips->m_pMipHeader[iMip];
				SMipData* mp = &mh.m_Mips[iSide];
				if (!mp->DataArray)
					mp->Init(mh.m_SideSize, Align(nMipW, nMipAlign), Align(nMipH, nMipAlign));

				if (eSrcTileMode != eTM_None)
					mp->m_bNative = 1;

				int nSrcSideSize = CTexture::TextureDataSize(
					max(1, m_nWidth  >> iMip),
					max(1, m_nHeight >> iMip),
					max(1, m_nDepth  >> iMip),
					1, 1,
					m_eSrcFormat,
					eSrcTileMode);

				CTexture::s_nTexturesDataBytesLoaded += nSrcSideSize;
				if (iSide == 0 || (m_eFlags & FT_REPLICATE_TO_ALL_SIDES) == 0)
				{
					assert(pIM->mfIs_image(iSide));
					buf = pIM->mfGet_image(iSide);
					if (mp->DataArray)
					{
						cryMemcpy(&mp->DataArray[0], &buf[nOffs], nSrcSideSize);
					}
					nOffs += nSrcSideSize;
				}
				else if (iSide > 0)
				{
					if (m_eFlags & FT_REPLICATE_TO_ALL_SIDES)
					{
						if (mp->DataArray && mh.m_Mips[0].DataArray)
						{
							cryMemcpy(&mp->DataArray[0], &mh.m_Mips[0].DataArray[0], nSrcSideSize);
						}
					}
					else
						assert(0);
				}
				else
					assert(0);
			}
		}
	}

	// store file position on DVD
	{
		if (m_eFlags & FT_SPLITTED)
		{
			for (int i = 0; i < m_nMips - m_CacheFileHeader.m_nMipsPersistent; ++i)
			{
				const int chunkNumber = i >= (m_nMips - m_CacheFileHeader.m_nMipsPersistent) ? 0 : m_nMips - i - m_CacheFileHeader.m_nMipsPersistent;

				DDSSplitted::TPath sLastChunkName;
				DDSSplitted::MakeName(sLastChunkName, m_SrcName.c_str(), chunkNumber, FIM_SPLITTED | ((GetFlags() & FT_ALPHA) ? FIM_ALPHA : 0));
				m_pFileTexMips->m_pMipHeader[i].m_eMediaType = gEnv->pCryPak->GetFileMediaType(sLastChunkName);
			}
		}
	}

	m_pFileTexMips->m_fMinMipFactor = StreamCalculateMipFactor((m_nMips - m_CacheFileHeader.m_nMipsPersistent) << 8);
	m_nStreamFormatCode = StreamComputeFormatCode(m_nWidth, m_nHeight, m_nMips, m_eDstFormat);

	Relink();

#if defined(TEXTURE_GET_SYSTEM_COPY_SUPPORT)
	if (m_eFlags & FT_KEEP_LOWRES_SYSCOPY)
		PrepareLowResSystemCopy(pIM->mfGet_image(0), false);
#endif

	return true;
}

bool CTexture::StreamPrepare_Finalise(bool bFromLoad)
{
	LOADING_TIME_PROFILE_SECTION;

	assert(m_pFileTexMips);
	for (int iSide = 0; iSide < m_CacheFileHeader.m_nSides; ++iSide)
	{
		for (int iMip = m_nMips - m_CacheFileHeader.m_nMipsPersistent; iMip < m_nMips; iMip++)
		{
			STexMipHeader& mh = m_pFileTexMips->m_pMipHeader[iMip];
			SMipData* mp = &mh.m_Mips[iSide];
			assert(mp->DataArray);

			// If native, assume we're tiled and prepped for the device - we shouldn't need to expand.
			if (!mp->m_bNative)
			{
				int nSrcSideSize = CTexture::TextureDataSize(
					max(1, m_nWidth  >> iMip),
					max(1, m_nHeight >> iMip),
					max(1, m_nDepth  >> iMip),
					1, 1, m_eSrcFormat);

				ExpandMipFromFile(mp->DataArray, mh.m_SideSize, mp->DataArray, nSrcSideSize, m_eSrcFormat, m_eDstFormat);
			}
		}
	}

	{
		// create texture
		assert(!m_pFileTexMips->m_pPoolItem);
		assert(!IsStreaming());
		STexPoolItem* pNewPoolItem = StreamGetPoolItem(m_nMips - m_CacheFileHeader.m_nMipsPersistent, m_CacheFileHeader.m_nMipsPersistent, true, true);
		if (!pNewPoolItem)
		{
			assert(0);
			gEnv->pLog->LogError("CTexture::StreamPrepare: Failed to allocate memory for persistent mip chain! Texture: '%s'", m_SrcName.c_str());
			CTexture::s_bOutOfMemoryTotally = true;
			return false;
		}

		// upload mips to texture
		StreamAssignPoolItem(pNewPoolItem, m_nMips - m_CacheFileHeader.m_nMipsPersistent);
		StreamReleaseMipsData(0, m_nMips - 1);
		SetWasUnload(false);
	}

	for (int z = 0; z < MAX_PREDICTION_ZONES; z++)
		m_streamRounds[z].nRoundUpdateId = gRenDev->GetStreamZoneRoundId(z);
	m_bPostponed = false;
	m_fCurrentMipBias = 0.f;

#ifdef ENABLE_TEXTURE_STREAM_LISTENER
	if (!m_bStatTracked)
	{
		ITextureStreamListener* pListener = s_pStreamListener;
		m_bStatTracked = 1;
		if (pListener)
			pListener->OnCreatedStreamedTexture(this, m_SrcName.c_str(), m_nMips, m_nMips - GetNumPersistentMips());
	}
#endif

	m_bStreamPrepared = true;

	return true;
}

//=========================================================================
void CTexture::StreamValidateTexSize()
{
	return;

#if 0 && defined(_DEBUG)
	CHK_MAINORRENDTH;

	// check if all textures in the list are valid and streamable
	static size_t j = 0;
	if (j >= s_DistanceSortedTextures.size())
		j = 0;
	for (size_t nIter = 0; nIter < 10 && j < s_DistanceSortedTextures.size(); ++j, ++nIter)
	{
		CTexture* tp = s_DistanceSortedTextures[j];
		if (tp != NULL)
		{
			assert(tp->IsValid());
			assert(tp->IsStreamed());
		}
	}

	int nSizeManagedStreamed = 0;
	int nSizeManagedNonStreamed = 0;
	int nSizeDynamic = 0;
	static SResourceContainer* pRL = CBaseResource::GetResourcesForClass(CTexture::mfGetClassName());
	ResourcesMapItor itor;
	if (!pRL)
		return;

	// do this check once per 120 frames
	if (gRenDev->GetRenderFrameID() % 120 != 0)
		return;

	for (itor = pRL->m_RMap.begin(); itor != pRL->m_RMap.end(); itor++)
	{
		CTexture* tp = (CTexture*)itor->second;
		if (!tp->IsDynamic())
		{
			if (tp->IsStreamed() && tp->m_bStreamPrepared)
			{
				assert(tp->m_pFileTexMips);
				if (tp->m_pFileTexMips->m_pPoolItem)
				{
					if (tp->m_pFileTexMips->m_pPoolItem)
						nSizeManagedStreamed += tp->m_pFileTexMips->m_pPoolItem->m_pOwner->m_Size;
				}
				else
					nSizeManagedStreamed += tp->GetDeviceDataSize();
			}
			else
				nSizeManagedNonStreamed += tp->GetDeviceDataSize();
		}
		else
			nSizeDynamic += tp->GetDeviceDataSize();
	}
	STexPoolItem* pIT = CTexture::s_FreeTexPoolItems.m_PrevFree;
	while (pIT != &CTexture::s_FreeTexPoolItems)
	{
		nSizeManagedStreamed += pIT->m_pOwner->m_Size;
		pIT = pIT->m_PrevFree;
	}

	//  assert(nSizeManagedStreamed == s_nStatsCurManagedStreamedTexMem);
	//  assert(nSizeManagedNonStreamed == s_nStatsCurManagedNonStreamedTexMem);
	//  assert(nSizeDynamic == s_nStatsCurDynamicTexMem);
#endif
}

uint8 CTexture::StreamComputeFormatCode(uint32 nWidth, uint32 nHeight, uint32 nMips, ETEX_Format fmt)
{
	// Must have a dimension
	if (nWidth == 0)
		return 0;
	if (nHeight == 0)
		return 0;

	// Must be PoT
	if (nWidth  & (nWidth  - 1))
		return 0;
	if (nHeight & (nHeight - 1))
		return 0;

	uint32 nMaxDim = 1 << (SStreamFormatCode::MaxMips - 1);

	// Determine how many missing tail mips there are
	uint32 nFullMips = IntegerLog2(max(nWidth, nHeight)) + 1;
	uint32 nTailMips = nFullMips - nMips;

	// Shift upto find aspect
	while ((nWidth != nMaxDim) && (nHeight != nMaxDim))
	{
		nWidth  <<= 1;
		nHeight <<= 1;
	}

	SStreamFormatCodeKey key(nWidth, nHeight, fmt, (uint8)nTailMips);

	CryAutoLock<CryCriticalSection> lock(s_streamFormatLock);
	TStreamFormatCodeKeyMap::iterator it = s_formatCodeMap.find(key);
	if (it == s_formatCodeMap.end())
	{
		if (s_nFormatCodes == 256)
			__debugbreak();

		SStreamFormatCode code;
		memset(&code, 0, sizeof(code));
		for (uint32 nMip = nTailMips, nMipWidth = nWidth, nMipHeight = nHeight; nMip < SStreamFormatCode::MaxMips; ++nMip, nMipWidth = max(1u, nMipWidth >> 1), nMipHeight = max(1u, nMipHeight >> 1))
		{
			uint32 nMip1Size = CTexture::TextureDataSize(nMipWidth, nMipHeight, 1, SStreamFormatCode::MaxMips - nMip, 1, fmt);

			bool bAppearsLinear = true;
			bool bAppearsPoT = true;

			// Determine how the size function varies with slices. Currently only supports linear, or aligning slices to next pot
			for (uint32 nSlices = 1; nSlices <= 32; ++nSlices)
			{
				uint32 nMipSize = CTexture::TextureDataSize(nMipWidth, nMipHeight, 1, SStreamFormatCode::MaxMips - nMip, nSlices, fmt);

				uint32 nExpectedLinearSize = nMip1Size * nSlices;
				uint32 nAlignedSlices = 1u << (32 - (nSlices > 1 ? countLeadingZeros32(nSlices - 1) : 32));
				uint32 nExpectedPoTSize = nMip1Size * nAlignedSlices;
				if (nExpectedLinearSize != nMipSize)
					bAppearsLinear = false;
				if (nExpectedPoTSize != nMipSize)
					bAppearsPoT = false;
			}

			// If this fires, we can't encode the size(slices) function
			if (!bAppearsLinear && !bAppearsPoT)
				__debugbreak();

			code.sizes[nMip].size = nMip1Size;
			code.sizes[nMip].alignSlices = !bAppearsLinear && bAppearsPoT;
		}

		it = s_formatCodeMap.insert(std::make_pair(key, s_nFormatCodes)).first;
		s_formatCodes[s_nFormatCodes++] = code;
	}
	return it->second;
}

#ifdef ENABLE_TEXTURE_STREAM_LISTENER
void CTexture::StreamUpdateStats()
{
	ITextureStreamListener* pListener = s_pStreamListener;
	if (pListener)
	{
		void* pBegunUsing[512];
		void* pStoppedUsing[512];

		int nBegun = 0, nStopped = 0;
		int curFrame = gRenDev->GetRenderFrameID();

		std::vector<CTexture*> texs;
		s_pTextureStreamer->StatsFetchTextures(texs);

		for (std::vector<CTexture*>::const_iterator it = texs.begin(), itEnd = texs.end(); it != itEnd; ++it)
		{
			CTexture* pTex = *it;
			if (curFrame - pTex->m_nAccessFrameID <= 2)
			{
				// In use
				if (!pTex->m_bUsedRecently)
				{
					pTex->m_bUsedRecently = 1;
					pBegunUsing[nBegun++] = pTex;

					IF_UNLIKELY (nBegun == CRY_ARRAY_COUNT(pBegunUsing))
					{
						pListener->OnBegunUsingTextures(pBegunUsing, nBegun);
						nBegun = 0;
					}
				}
			}
			else
			{
				if (pTex->m_bUsedRecently)
				{
					pTex->m_bUsedRecently = 0;
					pStoppedUsing[nStopped++] = pTex;

					IF_UNLIKELY (nStopped == CRY_ARRAY_COUNT(pStoppedUsing))
					{
						pListener->OnEndedUsingTextures(pStoppedUsing, nStopped);
						nStopped = 0;
					}
				}
			}
		}

		if (nBegun)
			pListener->OnBegunUsingTextures(pBegunUsing, nBegun);
		if (nStopped)
			pListener->OnEndedUsingTextures(pStoppedUsing, nStopped);
	}
}
#endif

//the format on disk must match exactly the format in memory
bool CTexture::CanStreamInPlace(int nMip, STexPoolItem* pNewPoolItem)
{
#if defined(SUPPORTS_INPLACE_TEXTURE_STREAMING)
	if (CRendererCVars::CV_r_texturesstreamingInPlace)
	{
	#if !CRY_PLATFORM_CONSOLE
		if (m_eTT != eTT_2D)
			return false;

		bool bFormatCompatible = false;
		switch (m_eSrcFormat)
		{
		case eTF_DXT1:
		case eTF_DXT3:
		case eTF_DXT5:
		case eTF_A8:
		case eTF_R16:
		case eTF_R16S:
		case eTF_R16F:
		case eTF_R32F:
		case eTF_R16G16S:
		case eTF_R16G16F:
		case eTF_R32G32F:
		case eTF_B4G4R4A4:
		case eTF_R16G16B16A16F:
		case eTF_R32G32B32A32F:
		case eTF_3DC:
		case eTF_3DCP:
		case eTF_CTX1:
		case eTF_BC6UH:
		case eTF_BC7:
		case eTF_R9G9B9E5:
		case eTF_EAC_R11:
		case eTF_EAC_R11S:
		case eTF_EAC_RG11:
		case eTF_EAC_RG11S:
		case eTF_ETC2:
		case eTF_ETC2A:
			bFormatCompatible = true;
			break;

		default:
			bFormatCompatible = false;
			break;
		}

		if (!bFormatCompatible)
			return false;
	#endif

	#if CRY_PLATFORM_ORBIS
		if (m_eSrcTileMode != eTM_Optimal)
			return false;
	#endif

	#if CRY_PLATFORM_DURANGO && (CRY_RENDERER_DIRECT3D >= 110) && (CRY_RENDERER_DIRECT3D < 120)
		CDeviceTexture* pDevTex = pNewPoolItem->m_pDevTexture;
		if (!pDevTex->IsInPool() || !m_pFileTexMips->m_pMipHeader[nMip].m_InPlaceStreamable)
			return false;
	#endif

		return true;
	}
#endif
	return false;
}

#if defined(TEXSTRM_ASYNC_TEXCOPY)
bool CTexture::CanAsyncCopy()
{
	#if defined(TEXSTRM_CUBE_DMA_BROKEN)
	return m_eTT == eTT_2D;
	#else
	return true;
	#endif
}
#endif

bool CTexture::StartStreaming(CTexture* pTex, STexPoolItem* pNewPoolItem, const int nStartMip, const int nEndMip, const int nActivateMip, EStreamTaskPriority estp)
{
	FUNCTION_PROFILER_RENDERER();

	CHK_RENDTH;

	if (pTex->TryAddRef() > 0)
	{
		if (pTex->IsStreaming())
			__debugbreak();

		// TODO: support eTT_2DArray and eTT_CubeArray
		if (pTex->m_eTT != eTT_2D && pTex->m_eTT != eTT_Cube)
			__debugbreak();

		STexStreamInState* pStreamState = StreamState_AllocateIn();
		if (pStreamState)
		{
			DDSSplitted::ChunkInfo chunks[16];

			DDSSplitted::DDSDesc desc = pTex->m_pFileTexMips->m_desc;
			desc.pName = pTex->m_SrcName.c_str();

			int nDeltaMips = desc.nMips - pTex->m_nMips;

			size_t nNumChunks = DDSSplitted::GetFilesToRead(chunks, 16, desc, nStartMip + nDeltaMips, nEndMip + nDeltaMips);
			assert(nNumChunks == nEndMip - nStartMip + 1);

			StreamReadBatchParams streamRequests[STexStreamInState::MaxStreams];

			size_t nStreamRequests = 0;

			pStreamState->m_pTexture = pTex;

			pStreamState->m_pNewPoolItem = pNewPoolItem;
#ifndef _RELEASE
			pStreamState->m_fStartTime = iTimer->GetAsyncTime().GetSeconds();
#endif
			pStreamState->m_nAsyncRefCount = 0;
			pStreamState->m_nHigherUploadedMip = nStartMip;
			pStreamState->m_nLowerUploadedMip = nEndMip;
			pStreamState->m_nActivateMip = nActivateMip;

			// update streaming frame ID
			int nSizeToSubmit = 0;

			int nSides = pTex->GetNumSides();

#if DEVRES_USE_PINNING
			CDeviceTexture* pDevTex = pNewPoolItem->m_pDevTexture;
			char* pPinnedBaseAddress = (char*)pDevTex->Pin();
			pStreamState->m_bPinned = true;
#endif

			for (DDSSplitted::ChunkInfo* it = chunks, * itEnd = chunks + nNumChunks; it != itEnd; ++it)
			{
				const DDSSplitted::ChunkInfo& chunk = *it;

				assert(chunk.nMipLevel >= nStartMip);

				uint32 nChunkMip = chunk.nMipLevel - nDeltaMips;
				uint16 nMipIdx = nChunkMip - nStartMip;

				STexStreamInMipState& mipState = pStreamState->m_mips[nMipIdx];
				mipState.m_nSideDelta = chunk.nSideDelta;

				StreamReadParams baseParams;
				baseParams.nFlags |= IStreamEngine::FLAGS_NO_SYNC_CALLBACK;
				baseParams.dwUserData = nMipIdx;
				baseParams.nLoadTime = 1;
				baseParams.nMaxLoadTime = 4;
				baseParams.ePriority = estp;
				baseParams.nOffset = chunk.nOffsetInFile;
				baseParams.nSize = chunk.nSizeInFile;
				baseParams.nPerceptualImportance = nEndMip - nStartMip;
				baseParams.eMediaType = (EStreamSourceMediaType)pTex->m_pFileTexMips->m_pMipHeader[nChunkMip].m_eMediaType;

				streamRequests[nStreamRequests].params = baseParams;
				streamRequests[nStreamRequests].pCallback = pStreamState;
				streamRequests[nStreamRequests].szFile = chunk.fileName.c_str();
				streamRequests[nStreamRequests].tSource = eStreamTaskTypeTexture;

				if (pTex->CanStreamInPlace(nChunkMip, pNewPoolItem))
				{
					byte* pBaseAddress = NULL;

#if CRY_PLATFORM_ORBIS
					CDeviceTexture* pDevTex = pNewPoolItem->m_pDevTexture;

					D3DTexture* pD3DTex = pDevTex->Get2DTexture();
	#if CRY_RENDERER_GNM
					// TODO: Fix this, this should NOT be here!
					// The texture here is NOT created with any CPU access flags (or USAGE_STREAMING), and therefore it may not have been paged in yet.
					// This is (afaik) a bug in the high-level code, it doesn't put any flags on created textures...
					CGnmHeap::GnmFlushPhysicalMemoryMapping();

					EGnmTextureAspect aspect = kGnmTextureAspectColor;
					size_t offset = pD3DTex->GnmGetSubResourceOffset(nMipIdx, 0, aspect);
					pBaseAddress = pD3DTex->GnmGetBaseAddress(aspect) + offset;
	#else
					size_t offset = pD3DTex->GetMipOffset(nMipIdx, 0);
					pBaseAddress = static_cast<byte*>(pD3DTex->RawPointer()) + offset;
	#endif
					streamRequests[nStreamRequests].params.nFlags |= IStreamEngine::FLAGS_WRITE_ONLY_EXTERNAL_BUFFER;
#elif CRY_PLATFORM_DURANGO && (CRY_RENDERER_DIRECT3D >= 110) && (CRY_RENDERER_DIRECT3D < 120)
					pBaseAddress = reinterpret_cast<byte*>(pPinnedBaseAddress + pDevTex->GetSurfaceOffset(nMipIdx, 0));

	#if !defined(TEXTURES_IN_CACHED_MEM)
					streamRequests[nStreamRequests].params.nFlags |= IStreamEngine::FLAGS_WRITE_ONLY_EXTERNAL_BUFFER;
	#endif
#endif

					if (pBaseAddress)
					{
						streamRequests[nStreamRequests].params.pBuffer = pBaseAddress;
						mipState.m_bStreamInPlace = true;
					}
				}

				++nStreamRequests;
				++pStreamState->m_nAsyncRefCount;

				nSizeToSubmit += pTex->m_pFileTexMips->m_pMipHeader[nChunkMip].m_SideSize * nSides;
			}

			size_t nStreams = gEnv->pSystem->GetStreamEngine()->StartBatchRead(pStreamState->m_pStreams, streamRequests, nStreamRequests);

			if (nStreams)
			{
				pTex->SetStreamingInProgress(s_StreamInTasks.GetIdxFromPtr(pStreamState));

				CryInterlockedAdd(&s_nBytesSubmittedToStreaming, nSizeToSubmit);
				CryInterlockedAdd(&s_nMipsSubmittedToStreaming, (int)nStreams);

				return true;
			}
			else
			{
#if DEVRES_USE_PINNING
				if (pStreamState->m_bPinned)
				{
					pNewPoolItem->m_pDevTexture->Unpin();
					pStreamState->m_bPinned = false;
				}
#endif

				pStreamState->m_pTexture->Release();
				pStreamState->m_pTexture = NULL;
				StreamState_ReleaseIn(pStreamState);
			}
		}
		else
		{
			s_pPoolMgr->ReleaseItem(pNewPoolItem);
			pTex->Release();
		}
	}
	else
	{
		s_pPoolMgr->ReleaseItem(pNewPoolItem);
	}

	return false;
}

void CTexture::StreamUploadMips(int nStartMip, int nEndMip, STexPoolItem* pNewPoolItem)
{
	assert(pNewPoolItem);

	StreamCopyMipsTexToMem(nStartMip, nEndMip, true, pNewPoolItem);

	// Restore mips data from the device texture
	if (s_bStreamDontKeepSystem && m_pFileTexMips && m_pFileTexMips->m_pPoolItem)
	{
		STexPoolItem* pSrcItem = m_pFileTexMips->m_pPoolItem;
		int nSrcOffset = m_nMips - pSrcItem->m_pOwner->m_nMips;
		int nDstOffset = m_nMips - pNewPoolItem->m_pOwner->m_nMips;
		StreamCopyMipsTexToTex(pSrcItem, (nEndMip + 1) - nSrcOffset, pNewPoolItem, (nEndMip + 1) - nDstOffset, m_nMips - (nEndMip + 1));
	}

	if (s_bStreamDontKeepSystem)
		StreamReleaseMipsData(nStartMip, nEndMip);
}

void CTexture::InitStreaming()
{
	CHK_MAINORRENDTH;

	iLog->Log("Init textures management (%" PRISIZE_T " Mb of video memory is available)...", gRenDev->m_MaxTextureMemory / 1024 / 1024);

	// reset all statistics
	s_nStreamingThroughput = 0;
	s_nStreamingTotalTime = 0;

	InitStreamingDev();

	if (!s_pTextureStreamer || s_nStreamingUpdateMode != CRenderer::CV_r_texturesstreamingUpdateType)
	{
		delete s_pTextureStreamer;

		switch (CRenderer::CV_r_texturesstreamingUpdateType)
		{
		default:
#ifdef TEXSTRM_SUPPORT_REACTIVE
		case 0:
			s_pTextureStreamer = new CReactiveTextureStreamer();
			break;
#endif
		case 1:
			s_pTextureStreamer = new CPlanningTextureStreamer();
			break;
		}

		s_nStreamingUpdateMode = CRenderer::CV_r_texturesstreamingUpdateType;
	}

	if (!s_pPoolMgr)
		s_pPoolMgr = new CTextureStreamPoolMgr();

	SSystemGlobalEnvironment* pEnv = iSystem->GetGlobalEnvironment();

#if CRY_PLATFORM_DESKTOP
	if (CRenderer::CV_r_texturesstreaming)
	{
		int nMinTexStreamPool = 192;
		int nMaxTexStreamPool = 1536;

	#if CRY_PLATFORM_WINDOWS && CRY_PLATFORM_32BIT
		if (!pEnv->pi.win64Bit)  // 32 bit executable
		{
			nMaxTexStreamPool = 512;
		}
	#endif

		ICVar* pICVarTexRes = iConsole->GetCVar("sys_spec_TextureResolution");
		if (pICVarTexRes)
		{
			const int valTexRes = pICVarTexRes->GetIVal();
			if (valTexRes == 0)
			{
				// Some cards report slightly lower byte numbers than their spec in MB suggests, so we have to be conservative
				// Note: On some MGPU systems the memory reported is the overall amount and not the memory available per GPU
				if (gRenDev->m_MaxTextureMemory >= (size_t)2800 * 1024 * 1024)
				{
					pICVarTexRes->Set(4);
				}
				else if (gRenDev->m_MaxTextureMemory >= (size_t)1900 * 1024 * 1024)
				{
					pICVarTexRes->Set(3);
				}
				else if (gRenDev->m_MaxTextureMemory >= (size_t)1450 * 1024 * 1024)
				{
					pICVarTexRes->Set(2);
				}
				else
				{
					pICVarTexRes->Set(1);
				}
			}
			else
			{
				pICVarTexRes->Set(valTexRes);
			}
		}

		CRenderer::CV_r_TexturesStreamPoolSize = clamp_tpl(CRenderer::CV_r_TexturesStreamPoolSize, nMinTexStreamPool, nMaxTexStreamPool);

		// Don't skip mips in the editor so that assets can be viewed in full resolution
		if (gEnv->IsEditor())
			CRenderer::CV_r_texturesstreamingSkipMips = 0;
	}
#endif

	s_nStreamingMode = CRenderer::CV_r_texturesstreaming;

	s_bStreamDontKeepSystem = CRenderer::CV_r_texturesstreamingonlyvideo == 0;

	if (CRenderer::CV_r_texturesstreaming)
	{
		iLog->Log("  Enabling of textures streaming...");
		iLog->Log("  Using %d Mb of textures pool for streaming...", CRenderer::GetTexturesStreamPoolSize());
	}
	else
		iLog->Log("  Disabling of textures streaming...");

	if (gRenDev->m_MaxTextureMemory <= 256 * 1024 * 1024)
	{
		SDynTexture::s_CurDynTexAtlasCloudsMaxsize = min(24u, SDynTexture::s_CurDynTexAtlasCloudsMaxsize);
		SDynTexture::s_CurDynTexAtlasSpritesMaxsize = min(32u, SDynTexture::s_CurDynTexAtlasSpritesMaxsize);
		SDynTexture::s_CurTexAtlasSize = min(128u, SDynTexture::s_CurTexAtlasSize);
		SDynTexture::s_CurDynTexMaxSize = min(128u, SDynTexture::s_CurDynTexMaxSize);
	}
	iLog->Log("  Video textures: Atlas clouds max size: %u Mb", SDynTexture::s_CurDynTexAtlasCloudsMaxsize);
	iLog->Log("  Video textures: Atlas sprites max size: %u Mb", SDynTexture::s_CurDynTexAtlasSpritesMaxsize);
	iLog->Log("  Video textures: Dynamic managed max size: %u Mb", SDynTexture::s_CurDynTexMaxSize);

	// re-init all textures
	iLog->Log("  Reloading all textures...");
	CTexture::ToggleTexturesStreaming();

	iLog->Log("  Finished reloading textures...");
	iLog->Log("  Finished initializing textures streaming...");

	if (gEnv->pLocalMemoryUsage)
		gEnv->pLocalMemoryUsage->DeleteGlobalData();

#if ENABLE_STATOSCOPE
	static bool bRegisterDGs = true;
	if (bRegisterDGs)
	{
		gEnv->pStatoscope->RegisterDataGroup(new SStatoscopeTextureStreamingDG());
		gEnv->pStatoscope->RegisterDataGroup(new SStatoscopeTextureStreamingItemsDG());

	#ifndef _RELEASE
		gEnv->pStatoscope->RegisterDataGroup(new SStatoscopeTextureStreamingPoolDG());
	#endif

		bRegisterDGs = false;
	}
#endif
}

void CTexture::RT_FlushStreaming(bool bAbort)
{
	CRY_PROFILE_REGION(PROFILE_RENDERER, "CTexture::RT_FlushStreaming");

	RT_FlushAllStreamingTasks(bAbort);

	// flush all pool items
	s_pPoolMgr->GarbageCollect(NULL, 0, 10000000);
}

void CTexture::RT_FlushAllStreamingTasks(const bool bAbort /* = false*/)
{
	CTimeValue time0 = iTimer->GetAsyncTime();

	// flush all tasks globally
	CHK_RENDTH;
	iLog->Log("Flushing pended textures...");

#ifdef TEXSTRM_ASYNC_TEXCOPY
	{
		for (int i = 0; i < MaxStreamTasks; ++i)
		{
			STexStreamOutState& os = *s_StreamOutTasks.GetPtrFromIdx(i);
			if (os.m_pTexture)
			{
				if (bAbort)
					os.m_bAborted = true;
				os.m_jobState.Wait();
			}
		}
	}
#endif

	if (bAbort)
	{
		for (int i = 0; i < MaxStreamPrepTasks; ++i)
		{
			STexStreamPrepState*& ps = *s_StreamPrepTasks.GetPtrFromIdx(i);
			if (ps)
			{
				_smart_ptr<CImageFile> pFile = ps->m_pImage;

				if (pFile)
					pFile->mfAbortStreaming();

				delete ps;
				ps = NULL;
				s_StreamPrepTasks.Release(&ps);
			}
		}
	}

	{
		for (int i = 0; i < MaxStreamTasks; ++i)
		{
			STexStreamInState& ins = *s_StreamInTasks.GetPtrFromIdx(i);
			if (ins.m_pTexture)
			{
				if (bAbort)
					ins.m_bAborted = true;

				for (int m = 0; m < (ins.m_nLowerUploadedMip - ins.m_nHigherUploadedMip) + 1; ++m)
				{
					IReadStreamPtr pStream = ins.m_pStreams[m];
					if (pStream)
					{
						if (bAbort)
							pStream->Abort();
						else
							pStream->Wait();
					}
				}
			}
		}
	}

	StreamState_Update();
	StreamState_UpdatePrep();

	s_pTextureStreamer->Flush();

	assert(s_nBytesSubmittedToStreaming == 0);
	iLog->Log("Finished flushing pended textures...");

	SRenderStatistics::Write().m_fTexUploadTime += (iTimer->GetAsyncTime() - time0).GetSeconds();
}

void CTexture::AbortStreamingTasks(CTexture* pTex)
{
	if (pTex->m_nStreamSlot != InvalidStreamSlot)
	{
		CHK_RENDTH;
		if (pTex->m_nStreamSlot & StreamOutMask)
		{
#ifdef TEXSTRM_ASYNC_TEXCOPY
			STexStreamOutState& streamState = *s_StreamOutTasks.GetPtrFromIdx(pTex->m_nStreamSlot & StreamIdxMask);

	#ifndef _RELEASE
			if (streamState.m_pTexture != pTex) __debugbreak();
	#endif

			streamState.m_bAborted = true;
			if (!streamState.m_bDone)
				streamState.m_jobState.Wait();
			while (!streamState.TryCommit())
				CrySleep(0);
			StreamState_ReleaseOut(&streamState);
#endif
		}
		else if (pTex->m_nStreamSlot & StreamPrepMask)
		{
			STexStreamPrepState*& pStreamState = *s_StreamPrepTasks.GetPtrFromIdx(pTex->m_nStreamSlot & StreamIdxMask);

#ifndef _RELEASE
			if (pStreamState->m_pTexture != pTex) __debugbreak();
#endif

			delete pStreamState;
			pStreamState = NULL;
			s_StreamPrepTasks.Release(&pStreamState);
		}
		else
		{
			STexStreamInState& streamState = *s_StreamInTasks.GetPtrFromIdx(pTex->m_nStreamSlot & StreamIdxMask);

#ifndef _RELEASE
			if (streamState.m_pTexture != pTex) __debugbreak();
#endif

			streamState.m_bAborted = true;

			for (size_t i = 0, c = streamState.m_nLowerUploadedMip - streamState.m_nHigherUploadedMip + 1; i < c; ++i)
			{
				IReadStream* pStream = streamState.m_pStreams[i];
				if (pStream)
				{
					pStream->Abort();
					assert(streamState.m_pStreams[i] == NULL);
				}
			}

			bool bCommitted = streamState.TryCommit();
			assert(bCommitted);

			StreamState_ReleaseIn(&streamState);
		}
	}
}

void CTexture::StreamState_Update()
{
	CHK_RENDTH;

	FUNCTION_PROFILER_RENDERER();

	// Finalise and garbage collect the stream out tasks

#ifdef TEXSTRM_ASYNC_TEXCOPY
	for (size_t i = 0, c = s_StreamOutTasks.GetNumLive(); i < MaxStreamTasks && c > 0; ++i)
	{
		STexStreamOutState& state = *s_StreamOutTasks.GetPtrFromIdx(i);
		if (state.m_pTexture)
		{
			if (state.TryCommit())
			{
				StreamState_ReleaseOut(&state);
			}

			--c;
		}
	}
#endif

	// Garbage collect the stream in slots

	for (uint16 i = 0, c = s_StreamInTasks.GetNumLive(); i < MaxStreamTasks && c > 0; ++i)
	{
		STexStreamInState& state = *s_StreamInTasks.GetPtrFromIdx(i);
		if (state.m_pTexture)
		{
			if (state.m_bAllStreamsComplete)
			{
				if (state.TryCommit())
					StreamState_ReleaseIn(&state);
			}
			else if (state.m_bAborted)
			{
				// An error occurred. Try and cancel all the stream tasks.
				for (int mi = 0, mc = state.m_nLowerUploadedMip - state.m_nHigherUploadedMip + 1; mi != mc; ++mi)
				{
					IReadStream* pStream = &*state.m_pStreams[mi];
					if (pStream != NULL)
					{
						pStream->TryAbort();
					}
				}
			}

#ifdef ENABLE_TEXTURE_STREAM_LISTENER
			if (state.m_bAllStreamsComplete && s_pStreamListener)
			{
				s_pStreamListener->OnUploadedStreamedTexture(state.m_pTexture);
			}
#endif

			--c;
		}
	}
}

void CTexture::StreamState_UpdatePrep()
{
	FUNCTION_PROFILER_RENDERER();

	// Garbage collect the stream prep slots

	for (uint16 i = 0, c = s_StreamPrepTasks.GetNumLive(); i < MaxStreamPrepTasks && c > 0; ++i)
	{
		STexStreamPrepState*& pState = *s_StreamPrepTasks.GetPtrFromIdx(i);
		if (pState && pState->m_bCompleted && pState->Commit())
			s_pTextureStreamer->EndPrepare(pState);
	}
}

STexStreamInState* CTexture::StreamState_AllocateIn()
{
	CHK_RENDTH;
	return s_StreamInTasks.Allocate();
}

void CTexture::StreamState_ReleaseIn(STexStreamInState* pState)
{
	CHK_RENDTH;

	pState->Reset();
	s_StreamInTasks.Release(pState);
}

#ifdef TEXSTRM_ASYNC_TEXCOPY
STexStreamOutState* CTexture::StreamState_AllocateOut()
{
	CHK_RENDTH;
	return s_StreamOutTasks.Allocate();
}

void CTexture::StreamState_ReleaseOut(STexStreamOutState* pState)
{
	CHK_RENDTH;

	pState->Reset();
	s_StreamOutTasks.Release(pState);
}
#endif

STexStreamingInfo* CTexture::StreamState_AllocateInfo(int nMips)
{
	// Temporary - will be replaced by custom allocator later.
	STexStreamingInfo* pInfo = (STexStreamingInfo*)CryModuleMalloc(sizeof(STexStreamingInfo));
	new(pInfo) STexStreamingInfo(nMips);
	return pInfo;
}

void CTexture::StreamState_ReleaseInfo(CTexture* pOwnerTex, STexStreamingInfo* pInfo)
{
	// Make sure we the streamer is notified, so jobs can be synced with
	s_pTextureStreamer->OnTextureDestroy(pOwnerTex);

	pInfo->~STexStreamingInfo();
	CryModuleFree(pInfo);
}

void CTexture::Relink()
{
	if (!IsStreamed() || IsInDistanceSortedList())
		return;

	s_pTextureStreamer->Relink(this);
}

void CTexture::Unlink()
{
	if (IsInDistanceSortedList())
	{
		s_pTextureStreamer->Unlink(this);
	}
}
