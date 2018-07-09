// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include <CryAudio/IAudioInterfacesCommonData.h>

namespace CryAudio
{
namespace Impl
{
struct IImpl;
struct SObject3DAttributes;
}

class CATLListener;

class CAudioListenerManager final
{
public:

	CAudioListenerManager() = default;
	~CAudioListenerManager();

	CAudioListenerManager(CAudioListenerManager const&) = delete;
	CAudioListenerManager(CAudioListenerManager&&) = delete;
	CAudioListenerManager&           operator=(CAudioListenerManager const&) = delete;
	CAudioListenerManager&           operator=(CAudioListenerManager&&) = delete;

	void                             Init(Impl::IImpl* const pIImpl);
	void                             Release();
	void                             Update(float const deltaTime);
	CATLListener*                    CreateListener(char const* const szName = nullptr);
	void                             ReleaseListener(CATLListener* const pListener);
	size_t                           GetNumActiveListeners() const;
	Impl::SObject3DAttributes const& GetActiveListenerAttributes() const;

#if defined(INCLUDE_AUDIO_PRODUCTION_CODE)
	char const* GetActiveListenerName() const;
#endif // INCLUDE_AUDIO_PRODUCTION_CODE

private:

	std::vector<CATLListener*> m_activeListeners;
	Impl::IImpl*               m_pIImpl = nullptr;
};
} // namespace CryAudio
