// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include "ATLEntities.h"
#include <CrySystem/IStreamEngine.h>

#if defined(INCLUDE_AUDIO_PRODUCTION_CODE)
struct IRenderAuxGeom;
#endif // INCLUDE_AUDIO_PRODUCTION_CODE

namespace CryAudio
{
struct ICustomMemoryHeap;
class CATLAudioFileEntry;

// Filter for drawing debug info to the screen
enum class EAudioFileCacheManagerDebugFilter : EnumFlagsType
{
	All            = 0,
	Globals        = BIT(6), // a
	LevelSpecifics = BIT(7), // b
	UseCounted     = BIT(8), // c
};
CRY_CREATE_ENUM_FLAG_OPERATORS(EAudioFileCacheManagerDebugFilter);

class CFileCacheManager final : public IStreamCallback
{
public:

	explicit CFileCacheManager(AudioPreloadRequestLookup& preloadRequests);

	CFileCacheManager(CFileCacheManager const&) = delete;
	CFileCacheManager(CFileCacheManager&&) = delete;
	CFileCacheManager& operator=(CFileCacheManager const&) = delete;
	CFileCacheManager& operator=(CFileCacheManager&&) = delete;

	// Public methods
	void           Init(Impl::IImpl* const pIImpl);
	void           Release();
	FileEntryId    TryAddFileCacheEntry(XmlNodeRef const pFileNode, EDataScope const dataScope, bool const bAutoLoad);
	bool           TryRemoveFileCacheEntry(FileEntryId const audioFileEntryId, EDataScope const dataScope);
	void           UpdateLocalizedFileCacheEntries();
	ERequestStatus TryLoadRequest(PreloadRequestId const audioPreloadRequestId, bool const bLoadSynchronously, bool const bAutoLoadOnly);
	ERequestStatus TryUnloadRequest(PreloadRequestId const audioPreloadRequestId);
	ERequestStatus UnloadDataByScope(EDataScope const dataScope);

#if defined(INCLUDE_AUDIO_PRODUCTION_CODE)
	void DrawDebugInfo(IRenderAuxGeom& auxGeom, float const posX, float posY);
#endif // INCLUDE_AUDIO_PRODUCTION_CODE

private:

	// Internal type definitions.
	using AudioFileEntries = std::map<FileEntryId, CATLAudioFileEntry*>;

	// IStreamCallback
	virtual void StreamAsyncOnComplete(IReadStream* pStream, unsigned int nError) override;
	virtual void StreamOnComplete(IReadStream* pStream, unsigned int nError) override {}
	// ~IStreamCallback

	// Internal methods
	void AllocateHeap(size_t const size, char const* const szUsage);
	bool UncacheFileCacheEntryInternal(CATLAudioFileEntry* const pAudioFileEntry, bool const bNow, bool const bIgnoreUsedCount = false);
	bool DoesRequestFitInternal(size_t const requestSize);
	bool FinishStreamInternal(IReadStreamPtr const pStream, int unsigned const error);
	bool AllocateMemoryBlockInternal(CATLAudioFileEntry* const __restrict pAudioFileEntry);
	void UncacheFile(CATLAudioFileEntry* const pAudioFileEntry);
	void TryToUncacheFiles();
	void UpdateLocalizedFileEntryData(CATLAudioFileEntry* const pAudioFileEntry);
	bool TryCacheFileCacheEntryInternal(CATLAudioFileEntry* const pAudioFileEntry, FileEntryId const audioFileEntryId, bool const bLoadSynchronously, bool const bOverrideUseCount = false, size_t const useCount = 0);

	// Internal members
	Impl::IImpl*                    m_pIImpl;
	AudioPreloadRequestLookup&      m_preloadRequests;
	AudioFileEntries                m_audioFileEntries;
	_smart_ptr<::ICustomMemoryHeap> m_pMemoryHeap;
	size_t                          m_currentByteTotal;
	size_t                          m_maxByteTotal;
};
} // namespace CryAudio
