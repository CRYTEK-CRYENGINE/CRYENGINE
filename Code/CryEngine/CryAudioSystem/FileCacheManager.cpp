// Copyright 2001-2019 Crytek GmbH / Crytek Group. All rights reserved.

#include "stdafx.h"
#include "FileCacheManager.h"
#include "File.h"
#include "PreloadRequest.h"
#include "CVars.h"
#include "System.h"
#include "Request.h"
#include "CallbackRequestData.h"
#include "Common/FileInfo.h"
#include "Common/IImpl.h"
#include <CryRenderer/IRenderer.h>
#include <CryString/CryPath.h>

#if defined(CRY_AUDIO_USE_DEBUG_CODE)
	#include "Common/Logger.h"
	#include "Debug.h"
	#include "Common/DebugStyle.h"
	#include <CryRenderer/IRenderAuxGeom.h>
	#include <CrySystem/ITimer.h>
#endif // CRY_AUDIO_USE_DEBUG_CODE

namespace CryAudio
{
#if defined(CRY_AUDIO_USE_DEBUG_CODE)
std::map<CryFixedStringT<MaxMiscStringLength>, CFile*> g_afcmDebugInfo;

enum class EFileCacheManagerDebugFilter : EnumFlagsType
{
	All         = 0,
	Globals     = BIT(6), // a
	UserDefined = BIT(7), // b
	UseCounted  = BIT(8), // c
};
CRY_CREATE_ENUM_FLAG_OPERATORS(EFileCacheManagerDebugFilter);
#endif // CRY_AUDIO_USE_DEBUG_CODE

//////////////////////////////////////////////////////////////////////////
CFileCacheManager::~CFileCacheManager()
{
	CRY_ASSERT_MESSAGE(m_files.empty(), "There are still file entries during %s", __FUNCTION__);
}

//////////////////////////////////////////////////////////////////////////
void CFileCacheManager::Initialize()
{
#if CRY_PLATFORM_DURANGO
	m_pMemoryHeap = gEnv->pSystem->GetIMemoryManager()->CreateCustomMemoryHeapInstance(IMemoryManager::eapAPU);

	#if defined(CRY_AUDIO_USE_DEBUG_CODE)
	if (m_pMemoryHeap != nullptr)
	{
		m_maxByteTotal = static_cast<size_t>(g_cvars.m_fileCacheManagerSize) << 10;
	}
	#endif // CRY_AUDIO_USE_DEBUG_CODE
#else
	m_pMemoryHeap = gEnv->pSystem->GetIMemoryManager()->CreateCustomMemoryHeapInstance(IMemoryManager::eapCustomAlignment);
#endif // CRY_PLATFORM_DURANGO
}

//////////////////////////////////////////////////////////////////////////
FileId CFileCacheManager::TryAddFileCacheEntry(XmlNodeRef const& fileNode, ContextId const contextId, bool const isAutoLoad)
{
	FileId fileId = InvalidFileId;
	Impl::SFileInfo fileInfo;

	if (g_pIImpl->ConstructFile(fileNode, &fileInfo))
	{
		CryFixedStringT<MaxFilePathLength> const filePath(fileInfo.filePath);
		MEMSTAT_CONTEXT(EMemStatContextType::AudioSystem, "CryAudio::CFile");
		auto const pFile = new CFile(filePath.c_str(), fileInfo.pImplData);

		if (pFile != nullptr)
		{
			pFile->m_memoryBlockAlignment = fileInfo.memoryBlockAlignment;

			if (fileInfo.isLocalized)
			{
				pFile->m_flags |= EFileFlags::Localized;
			}

			fileId = static_cast<FileId>(StringToId(pFile->m_path.c_str()));
			CFile* const __restrict pExisitingFile = stl::find_in_map(m_files, fileId, nullptr);

			if (pExisitingFile == nullptr)
			{
				if (!isAutoLoad)
				{
					// Can now be ref-counted and therefore manually unloaded.
					pFile->m_flags |= EFileFlags::UseCounted;
				}

				pFile->m_contextId = contextId;
				pFile->m_path.MakeLower();
				size_t const fileSize = gEnv->pCryPak->FGetSize(pFile->m_path.c_str());

				if (fileSize > 0)
				{
					pFile->m_size = fileSize;
					pFile->m_flags = (pFile->m_flags | EFileFlags::NotCached) & ~EFileFlags::NotFound;
					pFile->m_streamTaskType = eStreamTaskTypeSound;
				}
#if defined(CRY_AUDIO_USE_DEBUG_CODE)
				else
				{
					Cry::Audio::Log(ELogType::Warning, "Couldn't find audio file %s for pre-loading.", pFile->m_path.c_str());
				}
#endif    // CRY_AUDIO_USE_DEBUG_CODE

				m_files[fileId] = pFile;
			}
			else
			{
				if (((pExisitingFile->m_flags & EFileFlags::UseCounted) != EFileFlags::None) && isAutoLoad)
				{
					// This file entry is upgraded from "manual loading" to "auto loading" but needs a reset to "manual loading" again!
					pExisitingFile->m_flags = (pExisitingFile->m_flags | EFileFlags::NeedsResetToManualLoading) & ~EFileFlags::UseCounted;

#if defined(CRY_AUDIO_USE_DEBUG_CODE)
					Cry::Audio::Log(ELogType::Always, R"(Upgraded file entry from "manual loading" to "auto loading": %s)", pExisitingFile->m_path.c_str());
#endif      // CRY_AUDIO_USE_DEBUG_CODE
				}

				// Entry already exists, free the memory!
				g_pIImpl->DestructFile(pFile->m_pImplData);
				delete pFile;
			}
		}
	}

	return fileId;
}

//////////////////////////////////////////////////////////////////////////
bool CFileCacheManager::TryRemoveFileCacheEntry(FileId const id, ContextId const contextId)
{
	bool bSuccess = false;
	Files::iterator const iter(m_files.find(id));

	if (iter != m_files.end())
	{
		CFile* const pFile = iter->second;

		if (pFile->m_contextId == contextId)
		{
			UncacheFileCacheEntryInternal(pFile, true, true);
			g_pIImpl->DestructFile(pFile->m_pImplData);
			delete pFile;
			m_files.erase(iter);
		}
		else if ((contextId != GlobalContextId) && ((pFile->m_flags & EFileFlags::NeedsResetToManualLoading) != EFileFlags::None))
		{
			pFile->m_flags = (pFile->m_flags | EFileFlags::UseCounted) & ~EFileFlags::NeedsResetToManualLoading;

#if defined(CRY_AUDIO_USE_DEBUG_CODE)
			Cry::Audio::Log(ELogType::Always, R"(Downgraded file entry from "auto loading" to "manual loading": %s)", pFile->m_path.c_str());
#endif  // CRY_AUDIO_USE_DEBUG_CODE
		}
	}

	return bSuccess;
}

//////////////////////////////////////////////////////////////////////////
void CFileCacheManager::UpdateLocalizedFileCacheEntries()
{
	for (auto const& filePair : m_files)
	{
		CFile* const pFile = filePair.second;

		if (pFile != nullptr && (pFile->m_flags & EFileFlags::Localized) != EFileFlags::None)
		{
			if ((pFile->m_flags & (EFileFlags::Cached | EFileFlags::Loading)) != EFileFlags::None)
			{
				// The file needs to be unloaded first.
				size_t const useCount = pFile->m_useCount;
				pFile->m_useCount = 0;   // Needed to uncache without an error.
				UncacheFile(pFile);
				UpdateLocalizedFileData(pFile);
				TryCacheFileCacheEntryInternal(pFile, filePair.first, true, true, useCount);
			}
			else
			{
				// The file is not cached or loading, it is safe to update the corresponding CFile data.
				UpdateLocalizedFileData(pFile);
			}
		}
	}
}

//////////////////////////////////////////////////////////////////////////
ERequestStatus CFileCacheManager::TryLoadRequest(
	PreloadRequestId const preloadRequestId,
	bool const loadSynchronously,
	bool const autoLoadOnly,
	ERequestFlags const flags /*= ERequestFlags::None*/,
	void* const pOwner /*= nullptr*/,
	void* const pUserData /*= nullptr*/,
	void* const pUserDataOwner /*= nullptr*/)
{
	bool isSuccess = false;
	CPreloadRequest* const pPreloadRequest = stl::find_in_map(g_preloadRequests, preloadRequestId, nullptr);

	if (pPreloadRequest != nullptr && (!autoLoadOnly || (autoLoadOnly && pPreloadRequest->m_bAutoLoad)))
	{
		isSuccess = true;

		for (FileId const fileId : pPreloadRequest->m_fileIds)
		{
			CFile* const pFile = stl::find_in_map(m_files, fileId, nullptr);

			if (pFile != nullptr)
			{
				isSuccess = TryCacheFileCacheEntryInternal(pFile, fileId, loadSynchronously) && isSuccess;
			}
		}
	}

	ERequestStatus const requestStatus = (isSuccess ? ERequestStatus::Success : ERequestStatus::Failure);
	SendFinishedPreloadRequest(preloadRequestId, isSuccess, flags, pOwner, pUserData, pUserDataOwner);

	return requestStatus;
}

//////////////////////////////////////////////////////////////////////////
ERequestStatus CFileCacheManager::TryUnloadRequest(PreloadRequestId const preloadRequestId)
{
	bool isSuccess = false;
	CPreloadRequest* const pPreloadRequest = stl::find_in_map(g_preloadRequests, preloadRequestId, nullptr);

	if (pPreloadRequest != nullptr)
	{
		isSuccess = true;

		for (FileId const fileId : pPreloadRequest->m_fileIds)
		{
			CFile* const pFile = stl::find_in_map(m_files, fileId, nullptr);

			if (pFile != nullptr)
			{
				isSuccess = UncacheFileCacheEntryInternal(pFile, false) && isSuccess;
			}
		}
	}

	return (isSuccess ? ERequestStatus::Success : ERequestStatus::Failure);
}

//////////////////////////////////////////////////////////////////////////
void CFileCacheManager::UnloadDataByContext(ContextId const contextId)
{
	Files::iterator iter(m_files.begin());
	Files::const_iterator iterEnd(m_files.end());

	while (iter != iterEnd)
	{
		CFile* const pFile = iter->second;

		if ((pFile != nullptr) && (pFile->m_contextId == contextId))
		{
			if (UncacheFileCacheEntryInternal(pFile, true, true))
			{
				g_pIImpl->DestructFile(pFile->m_pImplData);
				delete pFile;
				iter = m_files.erase(iter);
				iterEnd = m_files.end();
				continue;
			}
		}

		++iter;
	}
}

//////////////////////////////////////////////////////////////////////////
bool CFileCacheManager::UncacheFileCacheEntryInternal(CFile* const pFile, bool const uncacheImmediately, bool const ignoreUsedCount /* = false */)
{
	bool bSuccess = false;

	// In any case decrement the used count.
	if (pFile->m_useCount > 0)
	{
		--pFile->m_useCount;
	}

	if (pFile->m_useCount < 1 || ignoreUsedCount)
	{
		// Must be cached to proceed.
		if ((pFile->m_flags & EFileFlags::Cached) != EFileFlags::None)
		{
			// Only "use-counted" files can become removable!
			if ((pFile->m_flags & EFileFlags::UseCounted) != EFileFlags::None)
			{
				pFile->m_flags |= EFileFlags::Removable;
			}

			if (uncacheImmediately || ignoreUsedCount)
			{
				UncacheFile(pFile);
			}
		}
		else if ((pFile->m_flags & EFileFlags::Loading) != EFileFlags::None)
		{
#if defined(CRY_AUDIO_USE_DEBUG_CODE)
			Cry::Audio::Log(ELogType::Always, "Trying to remove a loading file cache entry %s", pFile->m_path.c_str());
#endif  // CRY_AUDIO_USE_DEBUG_CODE

			// Abort loading and reset the entry.
			UncacheFile(pFile);
		}
		else if ((pFile->m_flags & EFileFlags::MemAllocFail) != EFileFlags::None)
		{
#if defined(CRY_AUDIO_USE_DEBUG_CODE)
			Cry::Audio::Log(ELogType::Always, "Resetting a memalloc-failed file cache entry %s", pFile->m_path.c_str());
#endif  // CRY_AUDIO_USE_DEBUG_CODE

			// Only reset the entry.
			pFile->m_flags = (pFile->m_flags | EFileFlags::NotCached) & ~EFileFlags::MemAllocFail;
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

#if defined(CRY_AUDIO_USE_DEBUG_CODE)
//////////////////////////////////////////////////////////////////////////
void CFileCacheManager::UpdateDebugInfo(char const* const szDebugFilter)
{
	g_afcmDebugInfo.clear();

	if (!m_files.empty())
	{
		bool const drawAll = (g_cvars.m_fileCacheManagerDebugFilter == EFileCacheManagerDebugFilter::All);
		bool const drawUseCounted = ((g_cvars.m_fileCacheManagerDebugFilter & EFileCacheManagerDebugFilter::UseCounted) != 0);
		bool const drawGlobals = (g_cvars.m_fileCacheManagerDebugFilter & EFileCacheManagerDebugFilter::Globals) != 0;
		bool const drawUserDefined = (g_cvars.m_fileCacheManagerDebugFilter & EFileCacheManagerDebugFilter::UserDefined) != 0;

		CryFixedStringT<MaxControlNameLength> lowerCaseSearchString(szDebugFilter);
		lowerCaseSearchString.MakeLower();

		for (auto const& filePair : m_files)
		{
			CFile* const pFile = filePair.second;
			char const* const szFileName = PathUtil::GetFileName(pFile->m_path);
			CryFixedStringT<MaxControlNameLength> lowerCaseFileName(szFileName);
			lowerCaseFileName.MakeLower();

			if ((lowerCaseSearchString.empty() || (lowerCaseSearchString == "0")) || (lowerCaseFileName.find(lowerCaseSearchString) != CryFixedStringT<MaxControlNameLength>::npos))
			{
				bool canEmplace = false;

				if (pFile->m_contextId == GlobalContextId)
				{
					canEmplace = (drawAll || drawGlobals) || (drawUseCounted && ((pFile->m_flags & EFileFlags::UseCounted) != EFileFlags::None));
				}
				else
				{
					canEmplace = (drawAll || drawUserDefined) || (drawUseCounted && (pFile->m_flags & EFileFlags::UseCounted) != EFileFlags::None);
				}

				if (canEmplace)
				{
					CryFixedStringT<MaxMiscStringLength> debugText;

					if (pFile->m_size < 1024)
					{
						debugText.Format(debugText + "%s (%" PRISIZE_T " Byte)", pFile->m_path.c_str(), pFile->m_size);
					}
					else
					{
						debugText.Format(debugText + "%s (%" PRISIZE_T " KiB)", pFile->m_path.c_str(), pFile->m_size >> 10);
					}

					g_afcmDebugInfo.emplace(std::piecewise_construct, std::forward_as_tuple(debugText), std::forward_as_tuple(pFile));
				}
			}
		}
	}
}

//////////////////////////////////////////////////////////////////////////
void CFileCacheManager::DrawDebugInfo(IRenderAuxGeom& auxGeom, float const posX, float posY)
{
	#if CRY_PLATFORM_DURANGO
	auxGeom.Draw2dLabel(posX, posY, Debug::g_listHeaderFontSize, Debug::s_globalColorHeader, false, "FileCacheManager (%d of %d KiB) [Entries: %d]", static_cast<int>(m_currentByteTotal >> 10), static_cast<int>(m_maxByteTotal >> 10), static_cast<int>(m_files.size()));
	#else
	auxGeom.Draw2dLabel(posX, posY, Debug::g_listHeaderFontSize, Debug::s_globalColorHeader, false, "FileCacheManager (%d KiB) [Entries: %d]", static_cast<int>(m_currentByteTotal >> 10), static_cast<int>(m_files.size()));
	#endif // CRY_PLATFORM_DURANGO

	posY += Debug::g_listHeaderLineHeight;

	if (!g_afcmDebugInfo.empty())
	{
		float const frameTime = gEnv->pTimer->GetAsyncTime().GetSeconds();

		for (auto const& infoPair : g_afcmDebugInfo)
		{
			CFile* const pFile = infoPair.second;
			ColorF color = (pFile->m_contextId == GlobalContextId) ? Debug::s_afcmColorContextGlobal : Debug::s_afcmColorContextUserDefined;

			if ((pFile->m_flags & EFileFlags::Loading) != EFileFlags::None)
			{
				color = Debug::s_afcmColorFileLoading;
			}
			else if ((pFile->m_flags & EFileFlags::MemAllocFail) != EFileFlags::None)
			{
				color = Debug::s_afcmColorFileMemAllocFail;
			}
			else if ((pFile->m_flags & EFileFlags::Removable) != EFileFlags::None)
			{
				color = Debug::s_afcmColorFileRemovable;
			}
			else if ((pFile->m_flags & EFileFlags::NotCached) != EFileFlags::None)
			{
				color = Debug::s_afcmColorFileNotCached;
			}
			else if ((pFile->m_flags & EFileFlags::NotFound) != EFileFlags::None)
			{
				color = Debug::s_afcmColorFileNotFound;
			}

			float const ratio = (frameTime - pFile->m_timeCached) / 2.0f;

			if (ratio <= 1.0f)
			{
				color[0] *= clamp_tpl(ratio, 0.0f, color[0]);
				color[1] *= clamp_tpl(ratio, 0.0f, color[1]);
				color[2] *= clamp_tpl(ratio, 0.0f, color[2]);
			}

			if ((pFile->m_flags & EFileFlags::UseCounted) != EFileFlags::None)
			{
				auxGeom.Draw2dLabel(posX, posY, Debug::g_listFontSize, color, false, "%s [%" PRISIZE_T "]", infoPair.first.c_str(), pFile->m_useCount);
			}
			else
			{
				auxGeom.Draw2dLabel(posX, posY, Debug::g_listFontSize, color, false, "%s", infoPair.first.c_str());
			}

			posY += Debug::g_listLineHeight;
		}
	}
}
#endif // CRY_AUDIO_USE_DEBUG_CODE

//////////////////////////////////////////////////////////////////////////
bool CFileCacheManager::FinishStreamInternal(IReadStreamPtr const pStream, int unsigned const error)
{
	bool bSuccess = false;

	auto const fileId = static_cast<FileId>(pStream->GetUserData());
	CFile* const pFile = stl::find_in_map(m_files, fileId, nullptr);
	CRY_ASSERT(pFile != nullptr);

	// Must be loading in to proceed.
	if ((pFile != nullptr) && ((pFile->m_flags & EFileFlags::Loading) != EFileFlags::None))
	{
		if (error == 0)
		{
			pFile->m_pReadStream = nullptr;
			pFile->m_flags = (pFile->m_flags | EFileFlags::Cached) & ~(EFileFlags::Loading | EFileFlags::NotCached);

#if defined(CRY_AUDIO_USE_DEBUG_CODE)
			pFile->m_timeCached = gEnv->pTimer->GetAsyncTime().GetSeconds();
#endif  // CRY_AUDIO_USE_DEBUG_CODE

			Impl::SFileInfo fileInfo;
			fileInfo.memoryBlockAlignment = pFile->m_memoryBlockAlignment;
			fileInfo.pFileData = pFile->m_pMemoryBlock->GetData();
			fileInfo.size = pFile->m_size;
			fileInfo.pImplData = pFile->m_pImplData;
			cry_strcpy(fileInfo.fileName, PathUtil::GetFile(pFile->m_path.c_str()));
			cry_strcpy(fileInfo.filePath, pFile->m_path.c_str());
			fileInfo.isLocalized = (pFile->m_flags & EFileFlags::Localized) != EFileFlags::None;

			g_pIImpl->RegisterInMemoryFile(&fileInfo);
			bSuccess = true;
		}
		else if (error == ERROR_USER_ABORT)
		{
			// We abort this stream only during entry Uncache().
			// Therefore there's no need to call Uncache() during stream abort with error code ERROR_USER_ABORT.

#if defined(CRY_AUDIO_USE_DEBUG_CODE)
			Cry::Audio::Log(ELogType::Always, "AFCM: user aborted stream for file %s (error: %u)", pFile->m_path.c_str(), error);
#endif  // CRY_AUDIO_USE_DEBUG_CODE
		}
		else
		{
			UncacheFileCacheEntryInternal(pFile, true, true);

#if defined(CRY_AUDIO_USE_DEBUG_CODE)
			Cry::Audio::Log(ELogType::Error, "AFCM: failed to stream in file %s (error: %u)", pFile->m_path.c_str(), error);
#endif  // CRY_AUDIO_USE_DEBUG_CODE
		}
	}

	return bSuccess;
}

//////////////////////////////////////////////////////////////////////////
bool CFileCacheManager::AllocateMemoryBlockInternal(CFile* const __restrict pFile)
{
	MEMSTAT_CONTEXT(EMemStatContextType::AudioSystem, "CFileCacheManager::AllocateMemoryBlockInternal");

	// Must not have valid memory yet.
	CRY_ASSERT(pFile->m_pMemoryBlock == nullptr);

	if (m_pMemoryHeap != nullptr)
	{
		MEMSTAT_CONTEXT(EMemStatContextType::AudioSystem, "CryAudio::CFile::m_pMemoryBlock");
		pFile->m_pMemoryBlock = m_pMemoryHeap->AllocateBlock(pFile->m_size, pFile->m_path.c_str(), pFile->m_memoryBlockAlignment);
	}

	if (pFile->m_pMemoryBlock == nullptr)
	{
		// Memory block is either full or too fragmented, let's try to throw everything out that can be removed and allocate again.
		TryToUncacheFiles();

		// And try again!
		if (m_pMemoryHeap != nullptr)
		{
			MEMSTAT_CONTEXT(EMemStatContextType::AudioSystem, "CryAudio::CFile::m_pMemoryBlock");
			pFile->m_pMemoryBlock = m_pMemoryHeap->AllocateBlock(pFile->m_size, pFile->m_path.c_str(), pFile->m_memoryBlockAlignment);
		}
	}

	return pFile->m_pMemoryBlock != nullptr;
}

//////////////////////////////////////////////////////////////////////////
void CFileCacheManager::UncacheFile(CFile* const pFile)
{
#if defined(CRY_AUDIO_USE_DEBUG_CODE)
	m_currentByteTotal -= pFile->m_size;
#endif  // CRY_AUDIO_USE_DEBUG_CODE

	if (pFile->m_pReadStream)
	{
		pFile->m_pReadStream->Abort();
		pFile->m_pReadStream = nullptr;
	}

	if (((pFile->m_flags & EFileFlags::Cached) != EFileFlags::None) && (pFile->m_pMemoryBlock != nullptr) && (pFile->m_pMemoryBlock->GetData() != nullptr))
	{
		Impl::SFileInfo fileInfo;
		fileInfo.memoryBlockAlignment = pFile->m_memoryBlockAlignment;
		fileInfo.pFileData = pFile->m_pMemoryBlock->GetData();
		fileInfo.size = pFile->m_size;
		fileInfo.pImplData = pFile->m_pImplData;
		cry_strcpy(fileInfo.fileName, PathUtil::GetFile(pFile->m_path.c_str()));
		cry_strcpy(fileInfo.filePath, pFile->m_path.c_str());

		g_pIImpl->UnregisterInMemoryFile(&fileInfo);
	}

	pFile->m_pMemoryBlock = nullptr;
	pFile->m_flags = (pFile->m_flags | EFileFlags::NotCached) & ~(EFileFlags::Cached | EFileFlags::Removable | EFileFlags::Loading);
	CRY_ASSERT(pFile->m_useCount == 0);
	pFile->m_useCount = 0;

#if defined(CRY_AUDIO_USE_DEBUG_CODE)
	pFile->m_timeCached = 0.0f;
#endif // CRY_AUDIO_USE_DEBUG_CODE
}

//////////////////////////////////////////////////////////////////////////
void CFileCacheManager::TryToUncacheFiles()
{
	for (auto const& filePair : m_files)
	{
		CFile* const pFile = filePair.second;

		if ((pFile != nullptr) &&
		    ((pFile->m_flags & EFileFlags::Cached) != EFileFlags::None) &&
		    ((pFile->m_flags & EFileFlags::Removable) != EFileFlags::None))
		{
			UncacheFileCacheEntryInternal(pFile, true);
		}
	}
}

///////////////////////////////////////////////////////////////////////////
void CFileCacheManager::UpdateLocalizedFileData(CFile* const pFile)
{
	CryFixedStringT<MaxFileNameLength> fileName(PathUtil::GetFile(pFile->m_path.c_str()));

	pFile->m_path = g_pIImpl->GetFileLocation(pFile->m_pImplData);
	pFile->m_path += "/";
	pFile->m_path += fileName.c_str();
	pFile->m_path.MakeLower();

	pFile->m_size = gEnv->pCryPak->FGetSize(pFile->m_path.c_str());
	CRY_ASSERT(pFile->m_size > 0);
}

///////////////////////////////////////////////////////////////////////////
bool CFileCacheManager::TryCacheFileCacheEntryInternal(
	CFile* const pFile,
	FileId const id,
	bool const loadSynchronously,
	bool const overrideUseCount /*= false*/,
	size_t const useCount /*= 0*/)
{
	bool bSuccess = false;

	if (!pFile->m_path.empty() &&
	    ((pFile->m_flags & EFileFlags::NotCached) != EFileFlags::None) &&
	    (pFile->m_flags & (EFileFlags::Cached | EFileFlags::Loading)) == EFileFlags::None)
	{
		if (AllocateMemoryBlockInternal(pFile))
		{
			StreamReadParams streamReadParams;
			streamReadParams.nOffset = 0;
			streamReadParams.nFlags = IStreamEngine::FLAGS_NO_SYNC_CALLBACK;
			streamReadParams.dwUserData = static_cast<DWORD_PTR>(id);
			streamReadParams.nLoadTime = 0;
			streamReadParams.nMaxLoadTime = 0;
			streamReadParams.ePriority = estpUrgent;
			streamReadParams.pBuffer = pFile->m_pMemoryBlock->GetData();
			streamReadParams.nSize = static_cast<int unsigned>(pFile->m_size);

			pFile->m_flags |= EFileFlags::Loading;
			pFile->m_pReadStream = gEnv->pSystem->GetStreamEngine()->StartRead(eStreamTaskTypeFSBCache, pFile->m_path.c_str(), this, &streamReadParams);

			if (loadSynchronously)
			{
				pFile->m_pReadStream->Wait();
			}

#if defined(CRY_AUDIO_USE_DEBUG_CODE)
			m_currentByteTotal += pFile->m_size;
#endif  // CRY_AUDIO_USE_DEBUG_CODE

			bSuccess = true;
		}
		else
		{
			// Cannot have a valid memory block!
			CRY_ASSERT(pFile->m_pMemoryBlock == nullptr || pFile->m_pMemoryBlock->GetData() == nullptr);

			// This unfortunately is a total memory allocation fail.
			pFile->m_flags |= EFileFlags::MemAllocFail;

#if defined(CRY_AUDIO_USE_DEBUG_CODE)
			// The user should be made aware of it.
			Cry::Audio::Log(ELogType::Error, R"(AFCM: could not cache "%s" as we are out of memory!)", pFile->m_path.c_str());
#endif  // CRY_AUDIO_USE_DEBUG_CODE
		}
	}
	else if ((pFile->m_flags & (EFileFlags::Cached | EFileFlags::Loading)) != EFileFlags::None)
	{

#if defined(CRY_AUDIO_USE_DEBUG_CODE)
		if ((pFile->m_flags & EFileFlags::Loading) != EFileFlags::None)
		{
			Cry::Audio::Log(ELogType::Warning, R"(AFCM: could not cache "%s" as it's already loading!)", pFile->m_path.c_str());
		}
#endif // CRY_AUDIO_USE_DEBUG_CODE

		bSuccess = true;
	}
#if defined(CRY_AUDIO_USE_DEBUG_CODE)
	else if ((pFile->m_flags & EFileFlags::NotFound) != EFileFlags::None)
	{
		// The user should be made aware of it.
		Cry::Audio::Log(ELogType::Error, R"(AFCM: could not cache "%s" as it was not found at the target location!)", pFile->m_path.c_str());
	}
#endif // CRY_AUDIO_USE_DEBUG_CODE

	// Increment the used count on GameHints.
	if (((pFile->m_flags & EFileFlags::UseCounted) != EFileFlags::None) && ((pFile->m_flags & (EFileFlags::Cached | EFileFlags::Loading)) != EFileFlags::None))
	{
		if (overrideUseCount)
		{
			pFile->m_useCount = useCount;
		}
		else
		{
			++pFile->m_useCount;
		}

		// Make sure to handle the eAFCS_REMOVABLE flag according to the m_nUsedCount count.
		if (pFile->m_useCount != 0)
		{
			pFile->m_flags &= ~EFileFlags::Removable;
		}
		else
		{
			pFile->m_flags |= EFileFlags::Removable;
		}
	}

	return bSuccess;
}

//////////////////////////////////////////////////////////////////////////
void CFileCacheManager::SendFinishedPreloadRequest(
	PreloadRequestId const preloadRequestId,
	bool const isFullSuccess,
	ERequestFlags const flags,
	void* const pOwner,
	void* const pUserData,
	void* const pUserDataOwner)
{
	if ((flags& ERequestFlags::SubsequentCallbackOnExternalThread) != ERequestFlags::None)
	{
		SCallbackRequestData<ECallbackRequestType::ReportFinishedPreload> const requestData(preloadRequestId, isFullSuccess);
		CRequest const request(&requestData, ERequestFlags::CallbackOnExternalOrCallingThread, pOwner, pUserData, pUserDataOwner);
		g_system.PushRequest(request);
	}
	else if ((flags& ERequestFlags::SubsequentCallbackOnAudioThread) != ERequestFlags::None)
	{
		SCallbackRequestData<ECallbackRequestType::ReportFinishedPreload> const requestData(preloadRequestId, isFullSuccess);
		CRequest const request(&requestData, ERequestFlags::CallbackOnAudioThread, pOwner, pUserData, pUserDataOwner);
		g_system.PushRequest(request);
	}
}
} // namespace CryAudio
