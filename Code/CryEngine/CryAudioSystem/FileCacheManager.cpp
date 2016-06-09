// Copyright 2001-2016 Crytek GmbH / Crytek Group. All rights reserved.

#include "stdafx.h"
#include "FileCacheManager.h"
#include "SoundCVars.h"
#include <IAudioImpl.h>
#include <CryRenderer/IRenderer.h>
#include <CryRenderer/IRenderAuxGeom.h>
#include <CryMemory/IMemory.h>

using namespace CryAudio::Impl;

//////////////////////////////////////////////////////////////////////////
CFileCacheManager::CFileCacheManager(AudioPreloadRequestLookup& rPreloadRequests)
	: m_rPreloadRequests(rPreloadRequests)
	, m_nCurrentByteTotal(0)
	, m_nMaxByteTotal(0)
	, m_pImpl(nullptr)
{
}

//////////////////////////////////////////////////////////////////////////
CFileCacheManager::~CFileCacheManager()
{
}

//////////////////////////////////////////////////////////////////////////
void CFileCacheManager::Init(IAudioImpl* const pImpl)
{
	m_pImpl = pImpl;
	AllocateHeap(static_cast<size_t>(g_audioCVars.m_fileCacheManagerSize), "AudioFileCacheManager");
}

//////////////////////////////////////////////////////////////////////////
void CFileCacheManager::Release()
{
	CRY_ASSERT(m_cAudioFileEntries.empty());
	m_pImpl = nullptr;
}

//////////////////////////////////////////////////////////////////////////
void CFileCacheManager::Update()
{
	// Not used for now as we do not queue entries!
}

//////////////////////////////////////////////////////////////////////////
void CFileCacheManager::AllocateHeap(size_t const nSize, char const* const sUsage)
{
	if (nSize > 0)
	{
#if CRY_PLATFORM_DURANGO
		m_pMemoryHeap = gEnv->pSystem->GetIMemoryManager()->CreateCustomMemoryHeapInstance(IMemoryManager::eapAPU);
#else
		m_pMemoryHeap = gEnv->pSystem->GetIMemoryManager()->CreateCustomMemoryHeapInstance(IMemoryManager::eapCustomAlignment);
#endif // CRY_PLATFORM_DURANGO
		if (m_pMemoryHeap != nullptr)
		{
			m_nMaxByteTotal = nSize << 10;
		}
	}
}

//////////////////////////////////////////////////////////////////////////
AudioFileEntryId CFileCacheManager::TryAddFileCacheEntry(XmlNodeRef const pFileNode, EAudioDataScope const eDataScope, bool const bAutoLoad)
{
	AudioFileEntryId nID = INVALID_AUDIO_FILE_ENTRY_ID;
	SAudioFileEntryInfo fileEntryInfo;

	if (m_pImpl->ParseAudioFileEntry(pFileNode, &fileEntryInfo) == eAudioRequestStatus_Success)
	{
		CryFixedStringT<MAX_AUDIO_FILE_PATH_LENGTH> sFullPath(m_pImpl->GetAudioFileLocation(&fileEntryInfo));
		sFullPath += CRY_NATIVE_PATH_SEPSTR;
		sFullPath += fileEntryInfo.szFileName;
		POOL_NEW_CREATE(CATLAudioFileEntry, pNewAudioFileEntry)(sFullPath, fileEntryInfo.pImplData);

		if (pNewAudioFileEntry != nullptr)
		{
			pNewAudioFileEntry->m_memoryBlockAlignment = fileEntryInfo.memoryBlockAlignment;

			if (fileEntryInfo.bLocalized)
			{
				pNewAudioFileEntry->m_flags |= eAudioFileFlags_Localized;
			}

			nID = static_cast<AudioFileEntryId>(AudioStringToId(pNewAudioFileEntry->m_path.c_str()));
			CATLAudioFileEntry* const __restrict pExisitingAudioFileEntry = stl::find_in_map(m_cAudioFileEntries, nID, nullptr);

			if (pExisitingAudioFileEntry == nullptr)
			{
				if (!bAutoLoad)
				{
					// Can now be ref-counted and therefore manually unloaded.
					pNewAudioFileEntry->m_flags |= eAudioFileFlags_UseCounted;
				}

				pNewAudioFileEntry->m_dataScope = eDataScope;
				pNewAudioFileEntry->m_path.MakeLower();
				size_t const nFileSize = gEnv->pCryPak->FGetSize(pNewAudioFileEntry->m_path.c_str());

				if (nFileSize > 0)
				{
					pNewAudioFileEntry->m_size = nFileSize;
					pNewAudioFileEntry->m_flags = (pNewAudioFileEntry->m_flags | eAudioFileFlags_NotCached) & ~eAudioFileFlags_NotFound;
					pNewAudioFileEntry->m_streamTaskType = eStreamTaskTypeSound;
				}
				else
				{
					g_audioLogger.Log(eAudioLogType_Warning, "Couldn't find audio file %s for pre-loading.", pNewAudioFileEntry->m_path.c_str());
				}

				m_cAudioFileEntries[nID] = pNewAudioFileEntry;
			}
			else
			{
				if ((pExisitingAudioFileEntry->m_flags & eAudioFileFlags_UseCounted) > 0 && bAutoLoad)
				{
					// This file entry is upgraded from "manual loading" to "auto loading" but needs a reset to "manual loading" again!
					pExisitingAudioFileEntry->m_flags = (pExisitingAudioFileEntry->m_flags | eAudioFileFlags_NeedsResetToManualLoading) & ~eAudioFileFlags_UseCounted;
					g_audioLogger.Log(eAudioLogType_Always, "Upgraded file entry from \"manual loading\" to \"auto loading\": %s", pExisitingAudioFileEntry->m_path.c_str());
				}

				// Entry already exists, free the memory!
				m_pImpl->DeleteAudioFileEntry(pNewAudioFileEntry->m_pImplData);
				POOL_FREE(pNewAudioFileEntry);
			}
		}
	}

	return nID;
}

//////////////////////////////////////////////////////////////////////////
bool CFileCacheManager::TryRemoveFileCacheEntry(AudioFileEntryId const nAudioFileID, EAudioDataScope const eDataScope)
{
	bool bSuccess = false;
	TAudioFileEntries::iterator const Iter(m_cAudioFileEntries.find(nAudioFileID));

	if (Iter != m_cAudioFileEntries.end())
	{
		CATLAudioFileEntry* const pAudioFileEntry = Iter->second;

		if (pAudioFileEntry->m_dataScope == eDataScope)
		{
			UncacheFileCacheEntryInternal(pAudioFileEntry, true, true);
			m_pImpl->DeleteAudioFileEntry(pAudioFileEntry->m_pImplData);
			POOL_FREE(pAudioFileEntry);
			m_cAudioFileEntries.erase(Iter);
		}
		else if ((eDataScope == eAudioDataScope_LevelSpecific) && ((pAudioFileEntry->m_flags & eAudioFileFlags_NeedsResetToManualLoading) > 0))
		{
			pAudioFileEntry->m_flags = (pAudioFileEntry->m_flags | eAudioFileFlags_UseCounted) & ~eAudioFileFlags_NeedsResetToManualLoading;
			g_audioLogger.Log(eAudioLogType_Always, "Downgraded file entry from \"auto loading\" to \"manual loading\": %s", pAudioFileEntry->m_path.c_str());
		}
	}

	return bSuccess;
}

//////////////////////////////////////////////////////////////////////////
void CFileCacheManager::UpdateLocalizedFileCacheEntries()
{
	TAudioFileEntries::iterator Iter(m_cAudioFileEntries.begin());
	TAudioFileEntries::const_iterator const IterEnd(m_cAudioFileEntries.end());

	for (; Iter != IterEnd; ++Iter)
	{
		CATLAudioFileEntry* const pAudioFileEntry = Iter->second;

		if (pAudioFileEntry != nullptr && (pAudioFileEntry->m_flags & eAudioFileFlags_Localized) > 0)
		{
			if ((pAudioFileEntry->m_flags & (eAudioFileFlags_Cached | eAudioFileFlags_Loading)) > 0)
			{
				// The file needs to be unloaded first.
				size_t const nUseCount = pAudioFileEntry->m_useCount;
				pAudioFileEntry->m_useCount = 0;// Needed to uncache without an error.
				UncacheFile(pAudioFileEntry);

				UpdateLocalizedFileEntryData(pAudioFileEntry);

				TryCacheFileCacheEntryInternal(pAudioFileEntry, Iter->first, true, true, nUseCount);
			}
			else
			{
				// The file is not cached or loading, it is safe to update the corresponding CATLAudioFileEntry data.
				UpdateLocalizedFileEntryData(pAudioFileEntry);
			}
		}
	}
}

//////////////////////////////////////////////////////////////////////////
EAudioRequestStatus CFileCacheManager::TryLoadRequest(AudioPreloadRequestId const nPreloadRequestID, bool const bLoadSynchronously, bool const bAutoLoadOnly)
{
	bool bFullSuccess = false;
	bool bFullFailure = true;
	CATLPreloadRequest* const pPreloadRequest = stl::find_in_map(m_rPreloadRequests, nPreloadRequestID, nullptr);

	if (pPreloadRequest != nullptr && !pPreloadRequest->m_fileEntryIds.empty() && (!bAutoLoadOnly || (bAutoLoadOnly && pPreloadRequest->m_bAutoLoad)))
	{
		bFullSuccess = true;

		for (const AudioFileEntryId& fileID : pPreloadRequest->m_fileEntryIds)
		{
			CATLAudioFileEntry* const pAudioFileEntry = stl::find_in_map(m_cAudioFileEntries, fileID, nullptr);

			if (pAudioFileEntry != nullptr)
			{
				bool const bTemp = TryCacheFileCacheEntryInternal(pAudioFileEntry, fileID, bLoadSynchronously);
				bFullSuccess = bFullSuccess && bTemp;
				bFullFailure = bFullFailure && !bTemp;
			}
		}
	}

	return (bFullSuccess) ? eAudioRequestStatus_Success : ((bFullFailure) ? eAudioRequestStatus_Failure : eAudioRequestStatus_PartialSuccess);
}

//////////////////////////////////////////////////////////////////////////
EAudioRequestStatus CFileCacheManager::TryUnloadRequest(AudioPreloadRequestId const nPreloadRequestID)
{
	bool bFullSuccess = false;
	bool bFullFailure = true;
	CATLPreloadRequest* const pPreloadRequest = stl::find_in_map(m_rPreloadRequests, nPreloadRequestID, nullptr);

	if (pPreloadRequest != nullptr && !pPreloadRequest->m_fileEntryIds.empty())
	{
		bFullSuccess = true;

		for (const AudioFileEntryId& fileID : pPreloadRequest->m_fileEntryIds)
		{
			CATLAudioFileEntry* const pAudioFileEntry = stl::find_in_map(m_cAudioFileEntries, fileID, nullptr);

			if (pAudioFileEntry != nullptr)
			{
				bool const bTemp = UncacheFileCacheEntryInternal(pAudioFileEntry, true);
				bFullSuccess = bFullSuccess && bTemp;
				bFullFailure = bFullFailure && !bTemp;
			}
		}
	}

	return (bFullSuccess) ? eAudioRequestStatus_Success : ((bFullFailure) ? eAudioRequestStatus_Failure : eAudioRequestStatus_PartialSuccess);
}

//////////////////////////////////////////////////////////////////////////
EAudioRequestStatus CFileCacheManager::UnloadDataByScope(EAudioDataScope const eDataScope)
{
	TAudioFileEntries::iterator Iter(m_cAudioFileEntries.begin());
	TAudioFileEntries::const_iterator IterEnd(m_cAudioFileEntries.end());

	while (Iter != IterEnd)
	{
		CATLAudioFileEntry* const pAudioFileEntry = Iter->second;

		if (pAudioFileEntry != nullptr && pAudioFileEntry->m_dataScope == eDataScope)
		{
			if (UncacheFileCacheEntryInternal(pAudioFileEntry, true, true))
			{
				m_pImpl->DeleteAudioFileEntry(pAudioFileEntry->m_pImplData);
				POOL_FREE(pAudioFileEntry);
				Iter = m_cAudioFileEntries.erase(Iter);
				IterEnd = m_cAudioFileEntries.end();
				continue;
			}
		}

		++Iter;
	}

	return eAudioRequestStatus_Success;
}

//////////////////////////////////////////////////////////////////////////
bool CFileCacheManager::UncacheFileCacheEntryInternal(CATLAudioFileEntry* const pAudioFileEntry, bool const bNow, bool const bIgnoreUsedCount /* = false */)
{
	bool bSuccess = false;

	// In any case decrement the used count.
	if (pAudioFileEntry->m_useCount > 0)
	{
		--pAudioFileEntry->m_useCount;
	}

	if (pAudioFileEntry->m_useCount < 1 || bIgnoreUsedCount)
	{
		// Must be cached to proceed.
		if ((pAudioFileEntry->m_flags & eAudioFileFlags_Cached) > 0)
		{
			// Only "use-counted" files can become removable!
			if ((pAudioFileEntry->m_flags & eAudioFileFlags_UseCounted) > 0)
			{
				pAudioFileEntry->m_flags |= eAudioFileFlags_Removable;
			}

			if (bNow || bIgnoreUsedCount)
			{
				UncacheFile(pAudioFileEntry);
			}
		}
		else if ((pAudioFileEntry->m_flags & (eAudioFileFlags_Loading | eAudioFileFlags_MemAllocFail)) > 0)
		{
			g_audioLogger.Log(eAudioLogType_Always, "Trying to remove a loading or mem-failed file cache entry %s", pAudioFileEntry->m_path.c_str());

			// Reset the entry in case it's still loading or was a memory allocation fail.
			UncacheFile(pAudioFileEntry);
		}

		// The file was either properly uncached, queued for uncache or not cached at all.
		bSuccess = true;
	}

	return bSuccess;
}

//////////////////////////////////////////////////////////////////////////
void CFileCacheManager::StreamAsyncOnComplete(IReadStream* pStream, unsigned int nError)
{
	// We "user abort" quite frequently so this is not something we want to assert on.
	CRY_ASSERT(nError == 0 || nError == ERROR_USER_ABORT);

	FinishStreamInternal(pStream, nError);
}

//////////////////////////////////////////////////////////////////////////
void CFileCacheManager::DrawDebugInfo(IRenderAuxGeom& auxGeom, float const fPosX, float const fPosY)
{
#if defined(INCLUDE_AUDIO_PRODUCTION_CODE)
	if ((g_audioCVars.m_drawAudioDebug & eADDF_SHOW_FILECACHE_MANAGER_INFO) > 0)
	{
		EAudioDataScope eDataScope = eAudioDataScope_All;

		if ((g_audioCVars.m_fileCacheManagerDebugFilter & eAFCMDF_ALL) == 0)
		{
			if ((g_audioCVars.m_fileCacheManagerDebugFilter & eAFCMDF_GLOBALS) > 0)
			{
				eDataScope = eAudioDataScope_Global;
			}
			else if ((g_audioCVars.m_fileCacheManagerDebugFilter & eAFCMDF_LEVEL_SPECIFICS) > 0)
			{
				eDataScope = eAudioDataScope_LevelSpecific;
			}
		}

		CTimeValue const frameTime = gEnv->pTimer->GetAsyncTime();

		CryFixedStringT<MAX_AUDIO_MISC_STRING_LENGTH> sTemp;
		float const fEntryDrawSize = 1.1f;
		float const fEntryStepSize = 12.0f;
		float fPositionY = fPosY + 20.0f;
		float fPositionX = fPosX + 20.0f;
		float fTime = 0.0f;
		float fRatio = 0.0f;
		float fOriginalAlpha = 0.7f;
		float* pfColor = nullptr;

		// The colors.
		float fWhite[4] = { 1.0f, 1.0f, 1.0f, fOriginalAlpha };
		float fCyan[4] = { 0.0f, 1.0f, 1.0f, fOriginalAlpha };
		float fOrange[4] = { 1.0f, 0.5f, 0.0f, fOriginalAlpha };
		float fGreen[4] = { 0.0f, 1.0f, 0.0f, fOriginalAlpha };
		float fRed[4] = { 1.0f, 0.0f, 0.0f, fOriginalAlpha };
		float fRedish[4] = { 0.7f, 0.0f, 0.0f, fOriginalAlpha };
		float fBlue[4] = { 0.1f, 0.2f, 0.8f, fOriginalAlpha };
		float fYellow[4] = { 1.0f, 1.0f, 0.0f, fOriginalAlpha };

		auxGeom.Draw2dLabel(fPosX, fPositionY, 1.6f, fOrange, false, "FileCacheManager (%d of %d KiB) [Entries: %d]", static_cast<int>(m_nCurrentByteTotal >> 10), static_cast<int>(m_nMaxByteTotal >> 10), static_cast<int>(m_cAudioFileEntries.size()));
		fPositionY += 15.0f;

		TAudioFileEntries::const_iterator Iter(m_cAudioFileEntries.begin());
		TAudioFileEntries::const_iterator const IterEnd(m_cAudioFileEntries.end());

		for (; Iter != IterEnd; ++Iter)
		{
			CATLAudioFileEntry* const pAudioFileEntry = Iter->second;

			if (pAudioFileEntry->m_dataScope == eAudioDataScope_Global &&
			    ((g_audioCVars.m_fileCacheManagerDebugFilter == eAFCMDF_ALL) ||
			     (g_audioCVars.m_fileCacheManagerDebugFilter & eAFCMDF_GLOBALS) > 0 ||
			     ((g_audioCVars.m_fileCacheManagerDebugFilter & eAFCMDF_USE_COUNTED) > 0 &&
			      (pAudioFileEntry->m_flags & eAudioFileFlags_UseCounted) > 0)))
			{
				if ((pAudioFileEntry->m_flags & eAudioFileFlags_Loading) > 0)
				{
					pfColor = fRed;
				}
				else if ((pAudioFileEntry->m_flags & eAudioFileFlags_MemAllocFail) > 0)
				{
					pfColor = fBlue;
				}
				else if ((pAudioFileEntry->m_flags & eAudioFileFlags_Removable) > 0)
				{
					pfColor = fGreen;
				}
				else if ((pAudioFileEntry->m_flags & eAudioFileFlags_NotCached) > 0)
				{
					pfColor = fWhite;
				}
				else if ((pAudioFileEntry->m_flags & eAudioFileFlags_NotFound) > 0)
				{
					pfColor = fRedish;
				}
				else
				{
					pfColor = fCyan;
				}

				fTime = (frameTime - pAudioFileEntry->m_timeCached).GetSeconds();
				fRatio = fTime / 5.0f;
				fOriginalAlpha = pfColor[3];
				pfColor[3] *= clamp_tpl(fRatio, 0.2f, 1.0f);

				sTemp.clear();

				if ((pAudioFileEntry->m_flags & eAudioFileFlags_UseCounted) > 0)
				{
					if (pAudioFileEntry->m_size < 1024)
					{
						sTemp.Format(sTemp + "%s (%" PRISIZE_T " Byte) [%" PRISIZE_T "]", pAudioFileEntry->m_path.c_str(), pAudioFileEntry->m_size, pAudioFileEntry->m_useCount);
					}
					else
					{
						sTemp.Format(sTemp + "%s (%" PRISIZE_T " KiB) [%" PRISIZE_T "]", pAudioFileEntry->m_path.c_str(), pAudioFileEntry->m_size >> 10, pAudioFileEntry->m_useCount);
					}
				}
				else
				{
					if (pAudioFileEntry->m_size < 1024)
					{
						sTemp.Format(sTemp + "%s (%" PRISIZE_T " Byte)", pAudioFileEntry->m_path.c_str(), pAudioFileEntry->m_size);
					}
					else
					{
						sTemp.Format(sTemp + "%s (%" PRISIZE_T " KiB)", pAudioFileEntry->m_path.c_str(), pAudioFileEntry->m_size >> 10);
					}
				}

				auxGeom.Draw2dLabel(fPositionX, fPositionY, fEntryDrawSize, pfColor, false, "%s", sTemp.c_str());
				pfColor[3] = fOriginalAlpha;
				fPositionY += fEntryStepSize;
			}
		}

		Iter = m_cAudioFileEntries.begin();

		for (; Iter != IterEnd; ++Iter)
		{
			CATLAudioFileEntry* const pAudioFileEntry = Iter->second;

			if (pAudioFileEntry->m_dataScope == eAudioDataScope_LevelSpecific &&
			    ((g_audioCVars.m_fileCacheManagerDebugFilter == eAFCMDF_ALL) ||
			     (g_audioCVars.m_fileCacheManagerDebugFilter & eAFCMDF_LEVEL_SPECIFICS) > 0 ||
			     ((g_audioCVars.m_fileCacheManagerDebugFilter & eAFCMDF_USE_COUNTED) > 0 &&
			      (pAudioFileEntry->m_flags & eAudioFileFlags_UseCounted) > 0)))
			{
				if ((pAudioFileEntry->m_flags & eAudioFileFlags_Loading) > 0)
				{
					pfColor = fRed;
				}
				else if ((pAudioFileEntry->m_flags & eAudioFileFlags_MemAllocFail) > 0)
				{
					pfColor = fBlue;
				}
				else if ((pAudioFileEntry->m_flags & eAudioFileFlags_Removable) > 0)
				{
					pfColor = fGreen;
				}
				else if ((pAudioFileEntry->m_flags & eAudioFileFlags_NotCached) > 0)
				{
					pfColor = fWhite;
				}
				else if ((pAudioFileEntry->m_flags & eAudioFileFlags_NotFound) > 0)
				{
					pfColor = fRedish;
				}
				else
				{
					pfColor = fYellow;
				}

				fTime = (frameTime - pAudioFileEntry->m_timeCached).GetSeconds();
				fRatio = fTime / 5.0f;
				fOriginalAlpha = pfColor[3];
				pfColor[3] *= clamp_tpl(fRatio, 0.2f, 1.0f);

				sTemp.clear();

				if ((pAudioFileEntry->m_flags & eAudioFileFlags_UseCounted) > 0)
				{
					if (pAudioFileEntry->m_size < 1024)
					{
						sTemp.Format(sTemp + "%s (%" PRISIZE_T " Byte) [%" PRISIZE_T "]", pAudioFileEntry->m_path.c_str(), pAudioFileEntry->m_size, pAudioFileEntry->m_useCount);
					}
					else
					{
						sTemp.Format(sTemp + "%s (%" PRISIZE_T " KiB) [%" PRISIZE_T "]", pAudioFileEntry->m_path.c_str(), pAudioFileEntry->m_size >> 10, pAudioFileEntry->m_useCount);
					}
				}
				else
				{
					if (pAudioFileEntry->m_size < 1024)
					{
						sTemp.Format(sTemp + "%s (%" PRISIZE_T " Byte)", pAudioFileEntry->m_path.c_str(), pAudioFileEntry->m_size);
					}
					else
					{
						sTemp.Format(sTemp + "%s (%" PRISIZE_T " KiB)", pAudioFileEntry->m_path.c_str(), pAudioFileEntry->m_size >> 10);
					}
				}

				auxGeom.Draw2dLabel(fPositionX, fPositionY, fEntryDrawSize, pfColor, false, "%s", sTemp.c_str());
				pfColor[3] = fOriginalAlpha;
				fPositionY += fEntryStepSize;
			}

		}
	}
#endif // INCLUDE_AUDIO_PRODUCTION_CODE
}

//////////////////////////////////////////////////////////////////////////
bool CFileCacheManager::DoesRequestFitInternal(size_t const nRequestSize)
{
	// Make sure these unsigned values don't flip around.
	CRY_ASSERT(m_nCurrentByteTotal <= m_nMaxByteTotal);
	bool bSuccess = false;

	if (nRequestSize <= (m_nMaxByteTotal - m_nCurrentByteTotal))
	{
		// Here the requested size is available without the need of first cleaning up.
		bSuccess = true;
	}
	else
	{
		// Determine how much memory would get freed if all eAFF_REMOVABLE files get thrown out.
		// We however skip files that are already queued for unload. The request will get queued up in that case.
		size_t nPossibleMemoryGain = 0;

		// Check the single file entries for removability.
		TAudioFileEntries::const_iterator Iter(m_cAudioFileEntries.begin());
		TAudioFileEntries::const_iterator const IterEnd(m_cAudioFileEntries.end());

		for (; Iter != IterEnd; ++Iter)
		{
			CATLAudioFileEntry* const pAudioFileEntry = Iter->second;

			if (pAudioFileEntry != nullptr &&
			    (pAudioFileEntry->m_flags & eAudioFileFlags_Cached) > 0 &&
			    (pAudioFileEntry->m_flags & eAudioFileFlags_Removable) > 0)
			{
				nPossibleMemoryGain += pAudioFileEntry->m_size;
			}
		}

		size_t const nMaxAvailableSize = (m_nMaxByteTotal - (m_nCurrentByteTotal - nPossibleMemoryGain));

		if (nRequestSize <= nMaxAvailableSize)
		{
			// Here we need to cleanup first before allowing the new request to be allocated.
			TryToUncacheFiles();

			// We should only indicate success if there's actually really enough room for the new entry!
			bSuccess = (m_nMaxByteTotal - m_nCurrentByteTotal) >= nRequestSize;
		}
	}

	return bSuccess;
}

//////////////////////////////////////////////////////////////////////////
bool CFileCacheManager::FinishStreamInternal(IReadStreamPtr const pStream, int unsigned const nError)
{
	bool bSuccess = false;

	AudioFileEntryId const nFileID = static_cast<AudioFileEntryId>(pStream->GetUserData());
	CATLAudioFileEntry* const pAudioFileEntry = stl::find_in_map(m_cAudioFileEntries, nFileID, nullptr);
	CRY_ASSERT(pAudioFileEntry != nullptr);

	// Must be loading in to proceed.
	if (pAudioFileEntry != nullptr && (pAudioFileEntry->m_flags & eAudioFileFlags_Loading) > 0)
	{
		if (nError == 0)
		{
			pAudioFileEntry->m_pReadStream = nullptr;
			pAudioFileEntry->m_flags = (pAudioFileEntry->m_flags | eAudioFileFlags_Cached) & ~(eAudioFileFlags_Loading | eAudioFileFlags_NotCached);

#if defined(INCLUDE_AUDIO_PRODUCTION_CODE)
			pAudioFileEntry->m_timeCached = gEnv->pTimer->GetAsyncTime();
#endif // INCLUDE_AUDIO_PRODUCTION_CODE

			SAudioFileEntryInfo sFileEntryInfo;
			sFileEntryInfo.memoryBlockAlignment = pAudioFileEntry->m_memoryBlockAlignment;
			sFileEntryInfo.pFileData = pAudioFileEntry->m_pMemoryBlock->GetData();
			sFileEntryInfo.size = pAudioFileEntry->m_size;
			sFileEntryInfo.pImplData = pAudioFileEntry->m_pImplData;
			sFileEntryInfo.szFileName = PathUtil::GetFile(pAudioFileEntry->m_path.c_str());

			m_pImpl->RegisterInMemoryFile(&sFileEntryInfo);
			bSuccess = true;
		}
		else if (nError == ERROR_USER_ABORT)
		{
			// We abort this stream only during entry Uncache().
			// Therefore there's no need to call Uncache() during stream abort with error code ERROR_USER_ABORT.
			g_audioLogger.Log(eAudioLogType_Always, "AFCM: user aborted stream for file %s (error: %u)", pAudioFileEntry->m_path.c_str(), nError);
		}
		else
		{
			UncacheFileCacheEntryInternal(pAudioFileEntry, true, true);
			g_audioLogger.Log(eAudioLogType_Error, "AFCM: failed to stream in file %s (error: %u)", pAudioFileEntry->m_path.c_str(), nError);
		}
	}

	return bSuccess;
}

//////////////////////////////////////////////////////////////////////////
bool CFileCacheManager::AllocateMemoryBlockInternal(CATLAudioFileEntry* const __restrict pAudioFileEntry)
{
	// Must not have valid memory yet.
	CRY_ASSERT(pAudioFileEntry->m_pMemoryBlock == nullptr);

	if (m_pMemoryHeap != nullptr)
	{
		pAudioFileEntry->m_pMemoryBlock = m_pMemoryHeap->AllocateBlock(pAudioFileEntry->m_size, pAudioFileEntry->m_path.c_str(), pAudioFileEntry->m_memoryBlockAlignment);
	}

	if (pAudioFileEntry->m_pMemoryBlock == nullptr)
	{
		// Memory block is either full or too fragmented, let's try to throw everything out that can be removed and allocate again.
		TryToUncacheFiles();

		// And try again!
		if (m_pMemoryHeap != nullptr)
		{
			pAudioFileEntry->m_pMemoryBlock = m_pMemoryHeap->AllocateBlock(pAudioFileEntry->m_size, pAudioFileEntry->m_path.c_str(), pAudioFileEntry->m_memoryBlockAlignment);
		}
	}

	return pAudioFileEntry->m_pMemoryBlock != nullptr;
}

//////////////////////////////////////////////////////////////////////////
void CFileCacheManager::UncacheFile(CATLAudioFileEntry* const pAudioFileEntry)
{
	m_nCurrentByteTotal -= pAudioFileEntry->m_size;

	if (pAudioFileEntry->m_pReadStream)
	{
		pAudioFileEntry->m_pReadStream->Abort();
		pAudioFileEntry->m_pReadStream = nullptr;
	}

	if (pAudioFileEntry->m_pMemoryBlock != nullptr && pAudioFileEntry->m_pMemoryBlock->GetData() != nullptr)
	{
		SAudioFileEntryInfo sFileEntryInfo;
		sFileEntryInfo.memoryBlockAlignment = pAudioFileEntry->m_memoryBlockAlignment;
		sFileEntryInfo.pFileData = pAudioFileEntry->m_pMemoryBlock->GetData();
		sFileEntryInfo.size = pAudioFileEntry->m_size;
		sFileEntryInfo.pImplData = pAudioFileEntry->m_pImplData;
		sFileEntryInfo.szFileName = PathUtil::GetFile(pAudioFileEntry->m_path.c_str());

		m_pImpl->UnregisterInMemoryFile(&sFileEntryInfo);
	}

	pAudioFileEntry->m_pMemoryBlock = nullptr;
	pAudioFileEntry->m_flags = (pAudioFileEntry->m_flags | eAudioFileFlags_NotCached) & ~(eAudioFileFlags_Cached | eAudioFileFlags_Removable);
	CRY_ASSERT(pAudioFileEntry->m_useCount == 0);
	pAudioFileEntry->m_useCount = 0;

#if defined(INCLUDE_AUDIO_PRODUCTION_CODE)
	pAudioFileEntry->m_timeCached.SetValue(0);
#endif // INCLUDE_AUDIO_PRODUCTION_CODE
}

//////////////////////////////////////////////////////////////////////////
void CFileCacheManager::TryToUncacheFiles()
{
	TAudioFileEntries::iterator Iter(m_cAudioFileEntries.begin());
	TAudioFileEntries::const_iterator const IterEnd(m_cAudioFileEntries.end());

	for (; Iter != IterEnd; ++Iter)
	{
		CATLAudioFileEntry* const pAudioFileEntry = Iter->second;

		if (pAudioFileEntry != nullptr &&
		    (pAudioFileEntry->m_flags & eAudioFileFlags_Cached) != 0 &&
		    (pAudioFileEntry->m_flags & eAudioFileFlags_Removable) != 0)
		{
			UncacheFileCacheEntryInternal(pAudioFileEntry, true);
		}
	}
}

///////////////////////////////////////////////////////////////////////////
void CFileCacheManager::UpdateLocalizedFileEntryData(CATLAudioFileEntry* const pAudioFileEntry)
{
	static SAudioFileEntryInfo oFileEntryInfo;
	oFileEntryInfo.bLocalized = true;
	oFileEntryInfo.size = 0;
	oFileEntryInfo.pFileData = nullptr;
	oFileEntryInfo.memoryBlockAlignment = 0;

	CryFixedStringT<MAX_AUDIO_FILE_NAME_LENGTH> sFileName(PathUtil::GetFile(pAudioFileEntry->m_path.c_str()));
	oFileEntryInfo.pImplData = pAudioFileEntry->m_pImplData;
	oFileEntryInfo.szFileName = sFileName.c_str();

	pAudioFileEntry->m_path = m_pImpl->GetAudioFileLocation(&oFileEntryInfo);
	pAudioFileEntry->m_path += CRY_NATIVE_PATH_SEPSTR;
	pAudioFileEntry->m_path += sFileName.c_str();
	pAudioFileEntry->m_path.MakeLower();

	pAudioFileEntry->m_size = gEnv->pCryPak->FGetSize(pAudioFileEntry->m_path.c_str());
	CRY_ASSERT(pAudioFileEntry->m_size > 0);
}

///////////////////////////////////////////////////////////////////////////
bool CFileCacheManager::TryCacheFileCacheEntryInternal(
  CATLAudioFileEntry* const pAudioFileEntry,
  AudioFileEntryId const nFileID,
  bool const bLoadSynchronously,
  bool const bOverrideUseCount /* = false */,
  size_t const nUseCount /* = 0 */)
{
	bool bSuccess = false;

	if (!pAudioFileEntry->m_path.empty() &&
	    (pAudioFileEntry->m_flags & eAudioFileFlags_NotCached) > 0 &&
	    (pAudioFileEntry->m_flags & (eAudioFileFlags_Cached | eAudioFileFlags_Loading)) == 0)
	{
		if (DoesRequestFitInternal(pAudioFileEntry->m_size) && AllocateMemoryBlockInternal(pAudioFileEntry))
		{
			StreamReadParams oStreamReadParams;
			oStreamReadParams.nOffset = 0;
			oStreamReadParams.nFlags = IStreamEngine::FLAGS_NO_SYNC_CALLBACK;
			oStreamReadParams.dwUserData = static_cast<DWORD_PTR>(nFileID);
			oStreamReadParams.nLoadTime = 0;
			oStreamReadParams.nMaxLoadTime = 0;
			oStreamReadParams.ePriority = estpUrgent;
			oStreamReadParams.pBuffer = pAudioFileEntry->m_pMemoryBlock->GetData();
			oStreamReadParams.nSize = static_cast<int unsigned>(pAudioFileEntry->m_size);

			pAudioFileEntry->m_flags |= eAudioFileFlags_Loading;
			pAudioFileEntry->m_pReadStream = gEnv->pSystem->GetStreamEngine()->StartRead(eStreamTaskTypeFSBCache, pAudioFileEntry->m_path.c_str(), this, &oStreamReadParams);

			if (bLoadSynchronously)
			{
				pAudioFileEntry->m_pReadStream->Wait();
			}

			// Always add to the total size.
			m_nCurrentByteTotal += pAudioFileEntry->m_size;
			bSuccess = true;
		}
		else
		{
			// Cannot have a valid memory block!
			CRY_ASSERT(pAudioFileEntry->m_pMemoryBlock == nullptr || pAudioFileEntry->m_pMemoryBlock->GetData() == nullptr);

			// This unfortunately is a total memory allocation fail.
			pAudioFileEntry->m_flags |= eAudioFileFlags_MemAllocFail;

			// The user should be made aware of it.
			g_audioLogger.Log(eAudioLogType_Error, "AFCM: could not cache \"%s\" as we are out of memory!", pAudioFileEntry->m_path.c_str());
		}
	}
	else if ((pAudioFileEntry->m_flags & (eAudioFileFlags_Cached | eAudioFileFlags_Loading)) > 0)
	{
		// The user should be made aware of it.
		g_audioLogger.Log(eAudioLogType_Warning, "AFCM: could not cache \"%s\" as it is either already loaded or currently loading!", pAudioFileEntry->m_path.c_str());

		bSuccess = true;
	}
	else if ((pAudioFileEntry->m_flags & eAudioFileFlags_NotFound) > 0)
	{
		// The user should be made aware of it.
		g_audioLogger.Log(eAudioLogType_Error, "AFCM: could not cache \"%s\" as it was not found at the target location!", pAudioFileEntry->m_path.c_str());
	}

	// Increment the used count on GameHints.
	if ((pAudioFileEntry->m_flags & eAudioFileFlags_UseCounted) != 0 && (pAudioFileEntry->m_flags & (eAudioFileFlags_Cached | eAudioFileFlags_Loading)) > 0)
	{
		if (bOverrideUseCount)
		{
			pAudioFileEntry->m_useCount = nUseCount;
		}
		else
		{
			++pAudioFileEntry->m_useCount;
		}

		// Make sure to handle the eAFCS_REMOVABLE flag according to the m_nUsedCount count.
		if (pAudioFileEntry->m_useCount != 0)
		{
			pAudioFileEntry->m_flags &= ~eAudioFileFlags_Removable;
		}
		else
		{
			pAudioFileEntry->m_flags |= eAudioFileFlags_Removable;
		}
	}

	return bSuccess;
}
