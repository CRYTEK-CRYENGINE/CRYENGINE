// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "stdafx.h"
#include "EventListenerManager.h"
#include <CryCore/StlUtils.h>

#if defined(CRY_AUDIO_USE_DEBUG_CODE)
	#include "Common/Logger.h"
#endif // CRY_AUDIO_USE_DEBUG_CODE

namespace CryAudio
{
//////////////////////////////////////////////////////////////////////////
CEventListenerManager::~CEventListenerManager()
{
	stl::free_container(m_listeners);
}

//////////////////////////////////////////////////////////////////////////
ERequestStatus CEventListenerManager::AddRequestListener(SSystemRequestData<ESystemRequestType::AddRequestListener> const* const pRequestData)
{
	ERequestStatus result = ERequestStatus::Failure;

	for (auto const& listener : m_listeners)
	{
		if ((listener.OnEvent == pRequestData->func) &&
		    (listener.pObjectToListenTo == pRequestData->pObjectToListenTo) &&
		    (listener.eventMask == pRequestData->eventMask))
		{
			result = ERequestStatus::Success;
			break;
		}
	}

	if (result == ERequestStatus::Failure)
	{
		SEventListener eventListener;
		eventListener.pObjectToListenTo = pRequestData->pObjectToListenTo;
		eventListener.OnEvent = pRequestData->func;
		eventListener.eventMask = pRequestData->eventMask;
		m_listeners.push_back(eventListener);
		result = ERequestStatus::Success;
	}

	return result;
}

//////////////////////////////////////////////////////////////////////////
ERequestStatus CEventListenerManager::RemoveRequestListener(void (*func)(SRequestInfo const* const), void const* const pObjectToListenTo)
{
	ERequestStatus result = ERequestStatus::Failure;
	ListenerArray::iterator Iter(m_listeners.begin());
	ListenerArray::const_iterator const IterEnd(m_listeners.end());

	for (; Iter != IterEnd; ++Iter)
	{
		if (((Iter->OnEvent == func) || (func == nullptr)) && (Iter->pObjectToListenTo == pObjectToListenTo))
		{
			if (Iter != (IterEnd - 1))
			{
				(*Iter) = m_listeners.back();
			}

			m_listeners.pop_back();
			result = ERequestStatus::Success;

			break;
		}
	}

#if defined(CRY_AUDIO_USE_DEBUG_CODE)
	if (result == ERequestStatus::Failure)
	{
		Cry::Audio::Log(ELogType::Warning, "Failed to remove a request listener!");
	}
#endif // CRY_AUDIO_USE_DEBUG_CODE

	return result;
}

//////////////////////////////////////////////////////////////////////////
void CEventListenerManager::NotifyListener(SRequestInfo const* const pResultInfo)
{
	CRY_PROFILE_FUNCTION(PROFILE_AUDIO);

	for (auto const& listener : m_listeners)
	{
		if (((listener.eventMask & pResultInfo->systemEvent) != 0)                                               //check: is the listener interested in this specific event?
		    && ((listener.pObjectToListenTo == nullptr) || (listener.pObjectToListenTo == pResultInfo->pOwner))) //check: is the listener interested in events from this sender
		{
			listener.OnEvent(pResultInfo);
		}
	}
}
} // namespace CryAudio
