// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include "Entity.h"
#include "Common/PoolObject.h"
#include <CryAudio/IAudioInterfacesCommonData.h>

namespace CryAudio
{
class CPreloadRequest final : public CEntity<PreloadRequestId>, public CPoolObject<CPreloadRequest, stl::PSyncNone>
{
public:

	using FileIds = std::vector<FileId>;

#if defined(CRY_AUDIO_USE_DEBUG_CODE)
	explicit CPreloadRequest(
		PreloadRequestId const id,
		ContextId const contextId,
		bool const bAutoLoad,
		FileIds const& fileIds,
		char const* const szName)
		: CEntity<PreloadRequestId>(id, contextId, szName)
		, m_bAutoLoad(bAutoLoad)
		, m_fileIds(fileIds)
	{}
#else
	explicit CPreloadRequest(
		PreloadRequestId const id,
		ContextId const contextId,
		bool const bAutoLoad,
		FileIds const& fileIds)
		: CEntity<PreloadRequestId>(id, contextId)
		, m_bAutoLoad(bAutoLoad)
		, m_fileIds(fileIds)
	{}
#endif // CRY_AUDIO_USE_DEBUG_CODE

	bool const m_bAutoLoad;
	FileIds    m_fileIds;
};
} // namespace CryAudio
