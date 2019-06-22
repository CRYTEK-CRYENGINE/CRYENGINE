// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include "Common/PoolObject.h"
#include <CryAudio/IAudioInterfacesCommonData.h>
#include <CrySystem/IStreamEngine.h>
#include <CryMemory/IMemory.h>

namespace CryAudio
{
namespace Impl
{
struct IFile;
} // namespace Impl

enum class EFileFlags : EnumFlagsType
{
	None                      = 0,
	Cached                    = BIT(0),
	NotCached                 = BIT(1),
	NotFound                  = BIT(2),
	MemAllocFail              = BIT(3),
	Removable                 = BIT(4),
	Loading                   = BIT(5),
	UseCounted                = BIT(6),
	NeedsResetToManualLoading = BIT(7),
	Localized                 = BIT(8),
};
CRY_CREATE_ENUM_FLAG_OPERATORS(EFileFlags);

class CFile final : public CPoolObject<CFile, stl::PSyncNone>
{
public:

	CFile() = delete;
	CFile(CFile const&) = delete;
	CFile(CFile&&) = delete;
	CFile& operator=(CFile const&) = delete;
	CFile& operator=(CFile&&) = delete;

	explicit CFile(char const* const szPath = nullptr, Impl::IFile* const pImplData = nullptr)
		: m_path(szPath)
		, m_size(0)
		, m_useCount(0)
		, m_memoryBlockAlignment(CRY_MEMORY_ALLOCATION_ALIGNMENT)
		, m_flags(EFileFlags::NotFound)
		, m_contextId(GlobalContextId)
		, m_streamTaskType(eStreamTaskTypeCount)
		, m_pMemoryBlock(nullptr)
		, m_pReadStream(nullptr)
		, m_pImplData(pImplData)
#if defined(CRY_AUDIO_USE_DEBUG_CODE)
		, m_timeCached(0.0f)
#endif // CRY_AUDIO_USE_DEBUG_CODE
	{
	}

	CryFixedStringT<MaxFilePathLength> m_path;
	size_t                             m_size;
	size_t                             m_useCount;
	size_t                             m_memoryBlockAlignment;
	EFileFlags                         m_flags;
	ContextId                          m_contextId;
	EStreamTaskType                    m_streamTaskType;
	_smart_ptr<ICustomMemoryBlock>     m_pMemoryBlock;
	IReadStreamPtr                     m_pReadStream;
	Impl::IFile*                       m_pImplData;

#if defined(CRY_AUDIO_USE_DEBUG_CODE)
	float m_timeCached;
#endif // CRY_AUDIO_USE_DEBUG_CODE
};
} // namespace CryAudio
