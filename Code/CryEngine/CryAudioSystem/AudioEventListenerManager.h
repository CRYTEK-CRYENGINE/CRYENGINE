// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include "AudioInternalInterfaces.h"

namespace CryAudio
{
class CAudioEventListenerManager final
{
public:

	CAudioEventListenerManager() = default;
	~CAudioEventListenerManager();

	CAudioEventListenerManager(CAudioEventListenerManager const&) = delete;
	CAudioEventListenerManager(CAudioEventListenerManager&&) = delete;
	CAudioEventListenerManager& operator=(CAudioEventListenerManager const&) = delete;
	CAudioEventListenerManager& operator=(CAudioEventListenerManager&&) = delete;

	ERequestStatus              AddRequestListener(SAudioManagerRequestData<EAudioManagerRequestType::AddRequestListener> const* const pRequestData);
	ERequestStatus              RemoveRequestListener(void (* func)(SRequestInfo const* const), void const* const pObjectToListenTo);
	void                        NotifyListener(SRequestInfo const* const pRequestInfo);

#if defined(INCLUDE_AUDIO_PRODUCTION_CODE)
	size_t GetNumEventListeners() const { return m_listeners.size(); }
#endif //INCLUDE_AUDIO_PRODUCTION_CODE

private:

	using ListenerArray = std::vector<SAudioEventListener>;
	ListenerArray m_listeners;
};
} // namespace CryAudio
