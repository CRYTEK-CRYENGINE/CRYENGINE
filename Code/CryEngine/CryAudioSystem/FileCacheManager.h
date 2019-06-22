// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include <CryAudio/IAudioSystem.h>
#include <CrySystem/IStreamEngine.h>
#include <CryMemory/IMemory.h>

#if defined(CRY_AUDIO_USE_DEBUG_CODE)
struct IRenderAuxGeom;
#endif // CRY_AUDIO_USE_DEBUG_CODE

namespace CryAudio
{
struct ICustomMemoryHeap;
class CFile;

class CFileCacheManager final : public IStreamCallback
{
public:

	CFileCacheManager() = default;
	~CFileCacheManager();

	CFileCacheManager(CFileCacheManager const&) = delete;
	CFileCacheManager(CFileCacheManager&&) = delete;
	CFileCacheManager& operator=(CFileCacheManager const&) = delete;
	CFileCacheManager& operator=(CFileCacheManager&&) = delete;

	// Public methods
	void           Initialize();
	FileId         TryAddFileCacheEntry(XmlNodeRef const& fileNode, ContextId const contextId, bool const bAutoLoad);
	bool           TryRemoveFileCacheEntry(FileId const id, ContextId const contextId);
	void           UpdateLocalizedFileCacheEntries();
	ERequestStatus TryLoadRequest(PreloadRequestId const preloadRequestId, bool const bLoadSynchronously, bool const bAutoLoadOnly);
	ERequestStatus TryUnloadRequest(PreloadRequestId const preloadRequestId);
	ERequestStatus UnloadDataByContext(ContextId const contextId);

#if defined(CRY_AUDIO_USE_DEBUG_CODE)
	void   UpdateDebugInfo(char const* const szDebugFilter);
	void   DrawDebugInfo(IRenderAuxGeom& auxGeom, float const posX, float posY);
	size_t GetTotalCachedFileSize() const { return m_currentByteTotal; }
#endif // CRY_AUDIO_USE_DEBUG_CODE

private:

	// Internal type definitions.
	using Files = std::map<FileId, CFile*>;

	// IStreamCallback
	virtual void StreamAsyncOnComplete(IReadStream* pStream, unsigned int nError) override;
	virtual void StreamOnComplete(IReadStream* pStream, unsigned int nError) override {}
	// ~IStreamCallback

	// Internal methods
	bool UncacheFileCacheEntryInternal(CFile* const pFile, bool const bNow, bool const bIgnoreUsedCount = false);
	bool FinishStreamInternal(IReadStreamPtr const pStream, int unsigned const error);
	bool AllocateMemoryBlockInternal(CFile* const __restrict pFile);
	void UncacheFile(CFile* const pFile);
	void TryToUncacheFiles();
	void UpdateLocalizedFileData(CFile* const pFile);
	bool TryCacheFileCacheEntryInternal(CFile* const pFile, FileId const id, bool const bLoadSynchronously, bool const bOverrideUseCount = false, size_t const useCount = 0);

	// Internal members
	Files                           m_files;
	_smart_ptr<::ICustomMemoryHeap> m_pMemoryHeap;

#if defined(CRY_AUDIO_USE_DEBUG_CODE)
	size_t m_currentByteTotal = 0;

	#if CRY_PLATFORM_DURANGO
	size_t m_maxByteTotal = 0;
	#endif // CRY_PLATFORM_DURANGO
#endif   // CRY_AUDIO_USE_DEBUG_CODE
};
} // namespace CryAudio
