// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "stdafx.h"
#include "FileCacheManager.h"
#include "AudioCVars.h"
#include "Common/Logger.h"
#include <IAudioImpl.h>
#include <CryRenderer/IRenderer.h>
#include <CryMemory/IMemory.h>
#include <CryString/CryPath.h>

#if defined(INCLUDE_AUDIO_PRODUCTION_CODE)
	#include <CryRenderer/IRenderAuxGeom.h>
#endif // INCLUDE_AUDIO_PRODUCTION_CODE

namespace CryAudio
{
//////////////////////////////////////////////////////////////////////////
CFileCacheManager::CFileCacheManager(AudioPreloadRequestLookup& preloadRequests)
	: m_preloadRequests(preloadRequests)
	, m_currentByteTotal(0)
	, m_maxByteTotal(0)
	, m_pIImpl(nullptr)
{
}

//////////////////////////////////////////////////////////////////////////
void CFileCacheManager::Init(Impl::IImpl* const pIImpl)
{
	m_pIImpl = pIImpl;
	AllocateHeap(static_cast<size_t>(g_cvars.m_fileCacheManagerSize), "AudioFileCacheManager");
}

//////////////////////////////////////////////////////////////////////////
void CFileCacheManager::Release()
{
	CRY_ASSERT(m_audioFileEntries.empty());
	m_pIImpl = nullptr;
}

//////////////////////////////////////////////////////////////////////////
void CFileCacheManager::AllocateHeap(size_t const size, char const* const szUsage)
{
	if (size > 0)
	{
#if CRY_PLATFORM_DURANGO
		m_pMemoryHeap = gEnv->pSystem->GetIMemoryManager()->CreateCustomMemoryHeapInstance(IMemoryManager::eapAPU);
#else
		m_pMemoryHeap = gEnv->pSystem->GetIMemoryManager()->CreateCustomMemoryHeapInstance(IMemoryManager::eapCustomAlignment);
#endif // CRY_PLATFORM_DURANGO
		if (m_pMemoryHeap != nullptr)
		{
			m_maxByteTotal = size << 10;
		}
	}
}

//////////////////////////////////////////////////////////////////////////
FileEntryId CFileCacheManager::TryAddFileCacheEntry(XmlNodeRef const pFileNode, EDataScope const dataScope, bool const bAutoLoad)
{
	FileEntryId fileEntryId = CryAudio::InvalidFileEntryId;
	Impl::SFileInfo fileInfo;

	if (m_pIImpl->ConstructFile(pFileNode, &fileInfo) == ERequestStatus::Success)
	{
		CryFixedStringT<MaxFilePathLength> fullPath(m_pIImpl->GetFileLocation(&fileInfo));
		fullPath += "/";
		fullPath += fileInfo.szFileName;
		CATLAudioFileEntry* pFileEntry = new CATLAudioFileEntry(fullPath, fileInfo.pImplData);

		if (pFileEntry != nullptr)
		{
			pFileEntry->m_memoryBlockAlignment = fileInfo.memoryBlockAlignment;

			if (fileInfo.bLocalized)
			{
				pFileEntry->m_flags |= EFileFlags::Localized;
			}

			fileEntryId = static_cast<FileEntryId>(StringToId(pFileEntry->m_path.c_str()));
			CATLAudioFileEntry* const __restrict pExisitingFileEntry = stl::find_in_map(m_audioFileEntries, fileEntryId, nullptr);

			if (pExisitingFileEntry == nullptr)
			{
				if (!bAutoLoad)
				{
					// Can now be ref-counted and therefore manually unloaded.
					pFileEntry->m_flags |= EFileFlags::UseCounted;
				}

				pFileEntry->m_dataScope = dataScope;
				pFileEntry->m_path.MakeLower();
				size_t const fileSize = gEnv->pCryPak->FGetSize(pFileEntry->m_path.c_str());

				if (fileSize > 0)
				{
					pFileEntry->m_size = fileSize;
					pFileEntry->m_flags = (pFileEntry->m_flags | EFileFlags::NotCached) & ~EFileFlags::NotFound;
					pFileEntry->m_streamTaskType = eStreamTaskTypeSound;
				}
				else
				{
					Cry::Audio::Log(ELogType::Warning, "Couldn't find audio file %s for pre-loading.", pFileEntry->m_path.c_str());
				}

				m_audioFileEntries[fileEntryId] = pFileEntry;
			}
			else
			{
				if ((pExisitingFileEntry->m_flags & EFileFlags::UseCounted) > 0 && bAutoLoad)
				{
					// This file entry is upgraded from "manual loading" to "auto loading" but needs a reset to "manual loading" again!
					pExisitingFileEntry->m_flags = (pExisitingFileEntry->m_flags | EFileFlags::NeedsResetToManualLoading) & ~EFileFlags::UseCounted;
					Cry::Audio::Log(ELogType::Always, R"(Upgraded file entry from "manual loading" to "auto loading": %s)", pExisitingFileEntry->m_path.c_str());
				}

				// Entry already exists, free the memory!
				m_pIImpl->DestructFile(pFileEntry->m_pImplData);
				delete pFileEntry;
			}
		}
	}

	return fileEntryId;
}

//////////////////////////////////////////////////////////////////////////
bool CFileCacheManager::TryRemoveFileCacheEntry(FileEntryId const audioFileEntryId, EDataScope const dataScope)
{
	bool bSuccess = false;
	AudioFileEntries::iterator const iter(m_audioFileEntries.find(audioFileEntryId));

	if (iter != m_audioFileEntries.end())
	{
		CATLAudioFileEntry* const pAudioFileEntry = iter->second;

		if (pAudioFileEntry->m_dataScope == dataScope)
		{
			UncacheFileCacheEntryInternal(pAudioFileEntry, true, true);
			m_pIImpl->DestructFile(pAudioFileEntry->m_pImplData);
			delete pAudioFileEntry;
			m_audioFileEntries.erase(iter);
		}
		else if ((dataScope == EDataScope::LevelSpecific) && ((pAudioFileEntry->m_flags & EFileFlags::NeedsResetToManualLoading) > 0))
		{
			pAudioFileEntry->m_flags = (pAudioFileEntry->m_flags | EFileFlags::UseCounted) & ~EFileFlags::NeedsResetToManualLoading;
			Cry::Audio::Log(ELogType::Always, R"(Downgraded file entry from "auto loading" to "manual loading": %s)", pAudioFileEntry->m_path.c_str());
		}
	}

	return bSuccess;
}

//////////////////////////////////////////////////////////////////////////
void CFileCacheManager::UpdateLocalizedFileCacheEntries()
{
	for (auto const& audioFileEntryPair : m_audioFileEntries)
	{
		CATLAudioFileEntry* const pAudioFileEntry = audioFileEntryPair.second;

		if (pAudioFileEntry != nullptr && (pAudioFileEntry->m_flags & EFileFlags::Localized) > 0)
		{
			if ((pAudioFileEntry->m_flags & (EFileFlags::Cached | EFileFlags::Loading)) > 0)
			{
				// The file needs to be unloaded first.
				size_t const useCount = pAudioFileEntry->m_useCount;
				pAudioFileEntry->m_useCount = 0;   // Needed to uncache without an error.
				UncacheFile(pAudioFileEntry);
				UpdateLocalizedFileEntryData(pAudioFileEntry);
				TryCacheFileCacheEntryInternal(pAudioFileEntry, audioFileEntryPair.first, true, true, useCount);
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
ERequestStatus CFileCacheManager::TryLoadRequest(PreloadRequestId const audioPreloadRequestId, bool const bLoadSynchronously, bool const bAutoLoadOnly)
{
	bool bFullSuccess = false;
	bool bFullFailure = true;
	CATLPreloadRequest* const pPreloadRequest = stl::find_in_map(m_preloadRequests, audioPreloadRequestId, nullptr);

	if (pPreloadRequest != nullptr && (!bAutoLoadOnly || (bAutoLoadOnly && pPreloadRequest->m_bAutoLoad)))
	{
		bFullSuccess = true;

		for (FileEntryId const audioFileEntryId : pPreloadRequest->m_fileEntryIds)
		{
			CATLAudioFileEntry* const pAudioFileEntry = stl::find_in_map(m_audioFileEntries, audioFileEntryId, nullptr);

			if (pAudioFileEntry != nullptr)
			{
				bool const bTemp = TryCacheFileCacheEntryInternal(pAudioFileEntry, audioFileEntryId, bLoadSynchronously);
				bFullSuccess = bFullSuccess && bTemp;
				bFullFailure = bFullFailure && !bTemp;
			}
		}
	}

	return (bFullSuccess) ? ERequestStatus::Success : ((bFullFailure) ? ERequestStatus::Failure : ERequestStatus::PartialSuccess);
}

//////////////////////////////////////////////////////////////////////////
ERequestStatus CFileCacheManager::TryUnloadRequest(PreloadRequestId const audioPreloadRequestId)
{
	bool bFullSuccess = false;
	bool bFullFailure = true;
	CATLPreloadRequest* const pPreloadRequest = stl::find_in_map(m_preloadRequests, audioPreloadRequestId, nullptr);

	if (pPreloadRequest != nullptr)
	{
		bFullSuccess = true;

		for (FileEntryId const audioFileEntryId : pPreloadRequest->m_fileEntryIds)
		{
			CATLAudioFileEntry* const pAudioFileEntry = stl::find_in_map(m_audioFileEntries, audioFileEntryId, nullptr);

			if (pAudioFileEntry != nullptr)
			{
				bool const bTemp = UncacheFileCacheEntryInternal(pAudioFileEntry, false);
				bFullSuccess = bFullSuccess && bTemp;
				bFullFailure = bFullFailure && !bTemp;
			}
		}
	}

	return (bFullSuccess) ? ERequestStatus::Success : ((bFullFailure) ? ERequestStatus::Failure : ERequestStatus::PartialSuccess);
}

//////////////////////////////////////////////////////////////////////////
ERequestStatus CFileCacheManager::UnloadDataByScope(EDataScope const dataScope)
{
	AudioFileEntries::iterator iter(m_audioFileEntries.begin());
	AudioFileEntries::const_iterator iterEnd(m_audioFileEntries.end());

	while (iter != iterEnd)
	{
		CATLAudioFileEntry* const pAudioFileEntry = iter->second;

		if (pAudioFileEntry != nullptr && pAudioFileEntry->m_dataScope == dataScope)
		{
			if (UncacheFileCacheEntryInternal(pAudioFileEntry, true, true))
			{
				m_pIImpl->DestructFile(pAudioFileEntry->m_pImplData);
				delete pAudioFileEntry;
				iter = m_audioFileEntries.erase(iter);
				iterEnd = m_audioFileEntries.end();
				continue;
			}
		}

		++iter;
	}

	return ERequestStatus::Success;
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
		if ((pAudioFileEntry->m_flags & EFileFlags::Cached) > 0)
		{
			// Only "use-counted" files can become removable!
			if ((pAudioFileEntry->m_flags & EFileFlags::UseCounted) > 0)
			{
				pAudioFileEntry->m_flags |= EFileFlags::Removable;
			}

			if (bNow || bIgnoreUsedCount)
			{
				UncacheFile(pAudioFileEntry);
			}
		}
		else if ((pAudioFileEntry->m_flags & EFileFlags::Loading) > 0)
		{
			Cry::Audio::Log(ELogType::Always, "Trying to remove a loading file cache entry %s", pAudioFileEntry->m_path.c_str());

			// Abort loading and reset the entry.
			UncacheFile(pAudioFileEntry);
		}
		else if ((pAudioFileEntry->m_flags & EFileFlags::MemAllocFail) > 0)
		{
			// Only reset the entry.
			Cry::Audio::Log(ELogType::Always, "Resetting a memalloc-failed file cache entry %s", pAudioFileEntry->m_path.c_str());
			pAudioFileEntry->m_flags = (pAudioFileEntry->m_flags | EFileFlags::NotCached) & ~EFileFlags::MemAllocFail;
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

#if defined(INCLUDE_AUDIO_PRODUCTION_CODE)
//////////////////////////////////////////////////////////////////////////
void CFileCacheManager::DrawDebugInfo(IRenderAuxGeom& auxGeom, float const posX, float posY)
{
	if ((g_cvars.m_drawAudioDebug & EAudioDebugDrawFilter::ShowFileCacheManagerInfo) > 0)
	{
		CTimeValue const frameTime = gEnv->pTimer->GetAsyncTime();

		CryFixedStringT<MaxMiscStringLength> tempString;
		float time = 0.0f;
		float ratio = 0.0f;
		float originalAlpha = 0.7f;
		float* pColor = nullptr;

		float white[4] = { 1.0f, 1.0f, 1.0f, originalAlpha };
		float cyan[4] = { 0.0f, 1.0f, 1.0f, originalAlpha };
		float orange[4] = { 1.0f, 0.5f, 0.0f, originalAlpha };
		float green[4] = { 0.0f, 1.0f, 0.0f, originalAlpha };
		float red[4] = { 1.0f, 0.0f, 0.0f, originalAlpha };
		float redish[4] = { 0.7f, 0.0f, 0.0f, originalAlpha };
		float blue[4] = { 0.1f, 0.2f, 0.8f, originalAlpha };
		float yellow[4] = { 1.0f, 1.0f, 0.0f, originalAlpha };

		auxGeom.Draw2dLabel(posX, posY, 1.5f, orange, false, "FileCacheManager (%d of %d KiB) [Entries: %d]", static_cast<int>(m_currentByteTotal >> 10), static_cast<int>(m_maxByteTotal >> 10), static_cast<int>(m_audioFileEntries.size()));
		posY += 16.0f;

		if (!m_audioFileEntries.empty())
		{
			CryFixedStringT<MaxControlNameLength> lowerCaseSearchString(g_cvars.m_pDebugFilter->GetString());
			lowerCaseSearchString.MakeLower();

			bool const bDrawAll = (g_cvars.m_fileCacheManagerDebugFilter == EAudioFileCacheManagerDebugFilter::All);
			bool const bDrawUseCounted = ((g_cvars.m_fileCacheManagerDebugFilter & EAudioFileCacheManagerDebugFilter::UseCounted) > 0);
			bool const bDrawGlobals = (g_cvars.m_fileCacheManagerDebugFilter & EAudioFileCacheManagerDebugFilter::Globals) > 0;
			bool const bDrawLevelSpecifics = (g_cvars.m_fileCacheManagerDebugFilter & EAudioFileCacheManagerDebugFilter::LevelSpecifics) > 0;

			std::vector<CryFixedStringT<MaxFilePathLength>> soundBanksSorted; // Create list to sort soundbanks alphabetically.
			soundBanksSorted.reserve(m_audioFileEntries.size());

			for (auto const& soundBank : m_audioFileEntries)
			{
				soundBanksSorted.emplace_back(soundBank.second->m_path.c_str());
			}

			std::sort(soundBanksSorted.begin(), soundBanksSorted.end());

			for (auto const& soundBankName : soundBanksSorted)
			{
				for (auto const& audioFileEntryPair : m_audioFileEntries)
				{
					if (audioFileEntryPair.second->m_path == soundBankName)
					{
						CATLAudioFileEntry* const pAudioFileEntry = audioFileEntryPair.second;

						char const* const szSoundBankFileName = PathUtil::GetFileName(pAudioFileEntry->m_path);
						CryFixedStringT<MaxControlNameLength> lowerCaseSoundBankFileName(szSoundBankFileName);
						lowerCaseSoundBankFileName.MakeLower();

						if ((lowerCaseSearchString.empty() || (lowerCaseSearchString == "0")) || (lowerCaseSoundBankFileName.find(lowerCaseSearchString) != CryFixedStringT<MaxControlNameLength>::npos))
						{
							bool bDraw = false;

							if (pAudioFileEntry->m_dataScope == EDataScope::Global)
							{
								bDraw = (bDrawAll || bDrawGlobals) || (bDrawUseCounted && ((pAudioFileEntry->m_flags & EFileFlags::UseCounted) > 0));
								pColor = cyan;
							}
							else if (pAudioFileEntry->m_dataScope == EDataScope::LevelSpecific)
							{
								bDraw = (bDrawAll || bDrawLevelSpecifics) || (bDrawUseCounted && (pAudioFileEntry->m_flags & EFileFlags::UseCounted) > 0);
								pColor = yellow;
							}

							if (bDraw)
							{
								if ((pAudioFileEntry->m_flags & EFileFlags::Loading) > 0)
								{
									pColor = red;
								}
								else if ((pAudioFileEntry->m_flags & EFileFlags::MemAllocFail) > 0)
								{
									pColor = blue;
								}
								else if ((pAudioFileEntry->m_flags & EFileFlags::Removable) > 0)
								{
									pColor = green;
								}
								else if ((pAudioFileEntry->m_flags & EFileFlags::NotCached) > 0)
								{
									pColor = white;
								}
								else if ((pAudioFileEntry->m_flags & EFileFlags::NotFound) > 0)
								{
									pColor = redish;
								}

								time = (frameTime - pAudioFileEntry->m_timeCached).GetSeconds();
								ratio = time / 5.0f;
								originalAlpha = pColor[3];
								pColor[3] *= clamp_tpl(ratio, 0.2f, 1.0f);

								tempString.clear();

								if ((pAudioFileEntry->m_flags & EFileFlags::UseCounted) > 0)
								{
									if (pAudioFileEntry->m_size < 1024)
									{
										tempString.Format(tempString + "%s (%" PRISIZE_T " Byte) [%" PRISIZE_T "]", pAudioFileEntry->m_path.c_str(), pAudioFileEntry->m_size, pAudioFileEntry->m_useCount);
									}
									else
									{
										tempString.Format(tempString + "%s (%" PRISIZE_T " KiB) [%" PRISIZE_T "]", pAudioFileEntry->m_path.c_str(), pAudioFileEntry->m_size >> 10, pAudioFileEntry->m_useCount);
									}
								}
								else
								{
									if (pAudioFileEntry->m_size < 1024)
									{
										tempString.Format(tempString + "%s (%" PRISIZE_T " Byte)", pAudioFileEntry->m_path.c_str(), pAudioFileEntry->m_size);
									}
									else
									{
										tempString.Format(tempString + "%s (%" PRISIZE_T " KiB)", pAudioFileEntry->m_path.c_str(), pAudioFileEntry->m_size >> 10);
									}
								}

								auxGeom.Draw2dLabel(posX, posY, 1.25f, pColor, false, "%s", tempString.c_str());
								pColor[3] = originalAlpha;
								posY += 11.0f;
							}
						}
					}
				}
			}
		}
	}
}
#endif // INCLUDE_AUDIO_PRODUCTION_CODE

//////////////////////////////////////////////////////////////////////////
bool CFileCacheManager::DoesRequestFitInternal(size_t const requestSize)
{
	// Make sure these unsigned values don't flip around.
	CRY_ASSERT(m_currentByteTotal <= m_maxByteTotal);
	bool bSuccess = false;

	if (requestSize <= (m_maxByteTotal - m_currentByteTotal))
	{
		// Here the requested size is available without the need of first cleaning up.
		bSuccess = true;
	}
	else
	{
		// Determine how much memory would get freed if all eAFF_REMOVABLE files get thrown out.
		// We however skip files that are already queued for unload. The request will get queued up in that case.
		size_t potentialMemoryGain = 0;

		// Check the single file entries for removability.
		for (auto const& audioFileEntryPair : m_audioFileEntries)
		{
			CATLAudioFileEntry* const pAudioFileEntry = audioFileEntryPair.second;

			if (pAudioFileEntry != nullptr &&
			    (pAudioFileEntry->m_flags & EFileFlags::Cached) > 0 &&
			    (pAudioFileEntry->m_flags & EFileFlags::Removable) > 0)
			{
				potentialMemoryGain += pAudioFileEntry->m_size;
			}
		}

		size_t const maxAvailableSize = (m_maxByteTotal - (m_currentByteTotal - potentialMemoryGain));

		if (requestSize <= maxAvailableSize)
		{
			// Here we need to cleanup first before allowing the new request to be allocated.
			TryToUncacheFiles();

			// We should only indicate success if there's actually really enough room for the new entry!
			bSuccess = (m_maxByteTotal - m_currentByteTotal) >= requestSize;
		}
	}

	return bSuccess;
}

//////////////////////////////////////////////////////////////////////////
bool CFileCacheManager::FinishStreamInternal(IReadStreamPtr const pStream, int unsigned const error)
{
	bool bSuccess = false;

	FileEntryId const audioFileEntryId = static_cast<FileEntryId>(pStream->GetUserData());
	CATLAudioFileEntry* const pAudioFileEntry = stl::find_in_map(m_audioFileEntries, audioFileEntryId, nullptr);
	CRY_ASSERT(pAudioFileEntry != nullptr);

	// Must be loading in to proceed.
	if (pAudioFileEntry != nullptr && (pAudioFileEntry->m_flags & EFileFlags::Loading) > 0)
	{
		if (error == 0)
		{
			pAudioFileEntry->m_pReadStream = nullptr;
			pAudioFileEntry->m_flags = (pAudioFileEntry->m_flags | EFileFlags::Cached) & ~(EFileFlags::Loading | EFileFlags::NotCached);

#if defined(INCLUDE_AUDIO_PRODUCTION_CODE)
			pAudioFileEntry->m_timeCached = gEnv->pTimer->GetAsyncTime();
#endif  // INCLUDE_AUDIO_PRODUCTION_CODE

			Impl::SFileInfo fileEntryInfo;
			fileEntryInfo.memoryBlockAlignment = pAudioFileEntry->m_memoryBlockAlignment;
			fileEntryInfo.pFileData = pAudioFileEntry->m_pMemoryBlock->GetData();
			fileEntryInfo.size = pAudioFileEntry->m_size;
			fileEntryInfo.pImplData = pAudioFileEntry->m_pImplData;
			fileEntryInfo.szFileName = PathUtil::GetFile(pAudioFileEntry->m_path.c_str());
			fileEntryInfo.szFilePath = pAudioFileEntry->m_path.c_str();
			fileEntryInfo.bLocalized = (pAudioFileEntry->m_flags & EFileFlags::Localized) != 0;

			m_pIImpl->RegisterInMemoryFile(&fileEntryInfo);
			bSuccess = true;
		}
		else if (error == ERROR_USER_ABORT)
		{
			// We abort this stream only during entry Uncache().
			// Therefore there's no need to call Uncache() during stream abort with error code ERROR_USER_ABORT.
			Cry::Audio::Log(ELogType::Always, "AFCM: user aborted stream for file %s (error: %u)", pAudioFileEntry->m_path.c_str(), error);
		}
		else
		{
			UncacheFileCacheEntryInternal(pAudioFileEntry, true, true);
			Cry::Audio::Log(ELogType::Error, "AFCM: failed to stream in file %s (error: %u)", pAudioFileEntry->m_path.c_str(), error);
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
	m_currentByteTotal -= pAudioFileEntry->m_size;

	if (pAudioFileEntry->m_pReadStream)
	{
		pAudioFileEntry->m_pReadStream->Abort();
		pAudioFileEntry->m_pReadStream = nullptr;
	}

	if (pAudioFileEntry->m_pMemoryBlock != nullptr && pAudioFileEntry->m_pMemoryBlock->GetData() != nullptr)
	{
		Impl::SFileInfo fileEntryInfo;
		fileEntryInfo.memoryBlockAlignment = pAudioFileEntry->m_memoryBlockAlignment;
		fileEntryInfo.pFileData = pAudioFileEntry->m_pMemoryBlock->GetData();
		fileEntryInfo.size = pAudioFileEntry->m_size;
		fileEntryInfo.pImplData = pAudioFileEntry->m_pImplData;
		fileEntryInfo.szFileName = PathUtil::GetFile(pAudioFileEntry->m_path.c_str());

		m_pIImpl->UnregisterInMemoryFile(&fileEntryInfo);
	}

	pAudioFileEntry->m_pMemoryBlock = nullptr;
	pAudioFileEntry->m_flags = (pAudioFileEntry->m_flags | EFileFlags::NotCached) & ~(EFileFlags::Cached | EFileFlags::Removable);
	CRY_ASSERT(pAudioFileEntry->m_useCount == 0);
	pAudioFileEntry->m_useCount = 0;

#if defined(INCLUDE_AUDIO_PRODUCTION_CODE)
	pAudioFileEntry->m_timeCached.SetValue(0);
#endif // INCLUDE_AUDIO_PRODUCTION_CODE
}

//////////////////////////////////////////////////////////////////////////
void CFileCacheManager::TryToUncacheFiles()
{
	for (auto const& audioFileEntryPair : m_audioFileEntries)
	{
		CATLAudioFileEntry* const pAudioFileEntry = audioFileEntryPair.second;

		if (pAudioFileEntry != nullptr &&
		    (pAudioFileEntry->m_flags & EFileFlags::Cached) > 0 &&
		    (pAudioFileEntry->m_flags & EFileFlags::Removable) > 0)
		{
			UncacheFileCacheEntryInternal(pAudioFileEntry, true);
		}
	}
}

///////////////////////////////////////////////////////////////////////////
void CFileCacheManager::UpdateLocalizedFileEntryData(CATLAudioFileEntry* const pAudioFileEntry)
{
	static Impl::SFileInfo fileEntryInfo;
	fileEntryInfo.bLocalized = true;
	fileEntryInfo.size = 0;
	fileEntryInfo.pFileData = nullptr;
	fileEntryInfo.memoryBlockAlignment = 0;

	CryFixedStringT<MaxFileNameLength> fileName(PathUtil::GetFile(pAudioFileEntry->m_path.c_str()));
	fileEntryInfo.pImplData = pAudioFileEntry->m_pImplData;
	fileEntryInfo.szFileName = fileName.c_str();

	pAudioFileEntry->m_path = m_pIImpl->GetFileLocation(&fileEntryInfo);
	pAudioFileEntry->m_path += "/";
	pAudioFileEntry->m_path += fileName.c_str();
	pAudioFileEntry->m_path.MakeLower();

	pAudioFileEntry->m_size = gEnv->pCryPak->FGetSize(pAudioFileEntry->m_path.c_str());
	CRY_ASSERT(pAudioFileEntry->m_size > 0);
}

///////////////////////////////////////////////////////////////////////////
bool CFileCacheManager::TryCacheFileCacheEntryInternal(
  CATLAudioFileEntry* const pAudioFileEntry,
  FileEntryId const audioFileEntryId,
  bool const bLoadSynchronously,
  bool const bOverrideUseCount /*= false*/,
  size_t const useCount /*= 0*/)
{
	bool bSuccess = false;

	if (!pAudioFileEntry->m_path.empty() &&
	    (pAudioFileEntry->m_flags & EFileFlags::NotCached) > 0 &&
	    (pAudioFileEntry->m_flags & (EFileFlags::Cached | EFileFlags::Loading)) == 0)
	{
		if (DoesRequestFitInternal(pAudioFileEntry->m_size) && AllocateMemoryBlockInternal(pAudioFileEntry))
		{
			StreamReadParams streamReadParams;
			streamReadParams.nOffset = 0;
			streamReadParams.nFlags = IStreamEngine::FLAGS_NO_SYNC_CALLBACK;
			streamReadParams.dwUserData = static_cast<DWORD_PTR>(audioFileEntryId);
			streamReadParams.nLoadTime = 0;
			streamReadParams.nMaxLoadTime = 0;
			streamReadParams.ePriority = estpUrgent;
			streamReadParams.pBuffer = pAudioFileEntry->m_pMemoryBlock->GetData();
			streamReadParams.nSize = static_cast<int unsigned>(pAudioFileEntry->m_size);

			pAudioFileEntry->m_flags |= EFileFlags::Loading;
			pAudioFileEntry->m_pReadStream = gEnv->pSystem->GetStreamEngine()->StartRead(eStreamTaskTypeFSBCache, pAudioFileEntry->m_path.c_str(), this, &streamReadParams);

			if (bLoadSynchronously)
			{
				pAudioFileEntry->m_pReadStream->Wait();
			}

			// Always add to the total size.
			m_currentByteTotal += pAudioFileEntry->m_size;
			bSuccess = true;
		}
		else
		{
			// Cannot have a valid memory block!
			CRY_ASSERT(pAudioFileEntry->m_pMemoryBlock == nullptr || pAudioFileEntry->m_pMemoryBlock->GetData() == nullptr);

			// This unfortunately is a total memory allocation fail.
			pAudioFileEntry->m_flags |= EFileFlags::MemAllocFail;

			// The user should be made aware of it.
			Cry::Audio::Log(ELogType::Error, R"(AFCM: could not cache "%s" as we are out of memory!)", pAudioFileEntry->m_path.c_str());
		}
	}
	else if ((pAudioFileEntry->m_flags & (EFileFlags::Cached | EFileFlags::Loading)) > 0)
	{

#if defined(INCLUDE_AUDIO_PRODUCTION_CODE)
		if ((pAudioFileEntry->m_flags & EFileFlags::Loading) > 0)
		{
			Cry::Audio::Log(ELogType::Warning, R"(AFCM: could not cache "%s" as it's already loading!)", pAudioFileEntry->m_path.c_str());
		}
#endif // INCLUDE_AUDIO_PRODUCTION_CODE

		bSuccess = true;
	}
	else if ((pAudioFileEntry->m_flags & EFileFlags::NotFound) > 0)
	{
		// The user should be made aware of it.
		Cry::Audio::Log(ELogType::Error, R"(AFCM: could not cache "%s" as it was not found at the target location!)", pAudioFileEntry->m_path.c_str());
	}

	// Increment the used count on GameHints.
	if ((pAudioFileEntry->m_flags & EFileFlags::UseCounted) > 0 && (pAudioFileEntry->m_flags & (EFileFlags::Cached | EFileFlags::Loading)) > 0)
	{
		if (bOverrideUseCount)
		{
			pAudioFileEntry->m_useCount = useCount;
		}
		else
		{
			++pAudioFileEntry->m_useCount;
		}

		// Make sure to handle the eAFCS_REMOVABLE flag according to the m_nUsedCount count.
		if (pAudioFileEntry->m_useCount != 0)
		{
			pAudioFileEntry->m_flags &= ~EFileFlags::Removable;
		}
		else
		{
			pAudioFileEntry->m_flags |= EFileFlags::Removable;
		}
	}

	return bSuccess;
}
} // namespace CryAudio
