// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "stdafx.h"
#include "AudioEventManager.h"
#include "AudioCVars.h"
#include "ATLAudioObject.h"
#include <IAudioImpl.h>

#if defined(INCLUDE_AUDIO_PRODUCTION_CODE)
	#include <CryRenderer/IRenderAuxGeom.h>
#endif // INCLUDE_AUDIO_PRODUCTION_CODE

namespace CryAudio
{
//////////////////////////////////////////////////////////////////////////
CAudioEventManager::~CAudioEventManager()
{
	if (m_pIImpl != nullptr)
	{
		Release();
	}
}

//////////////////////////////////////////////////////////////////////////
void CAudioEventManager::Init(Impl::IImpl* const pIImpl)
{
	m_pIImpl = pIImpl;
	CRY_ASSERT(m_constructedAudioEvents.empty());
}

//////////////////////////////////////////////////////////////////////////
void CAudioEventManager::Release()
{
	// Events cannot survive a middleware switch because we cannot
	// know which event types the new middleware backend will support so
	// the existing ones have to be destroyed now and new ones created
	// after the switch.
	if (!m_constructedAudioEvents.empty())
	{
		for (auto const pEvent : m_constructedAudioEvents)
		{
			m_pIImpl->DestructEvent(pEvent->m_pImplData);
			delete pEvent;
		}

		m_constructedAudioEvents.clear();
	}

	m_pIImpl = nullptr;
}

//////////////////////////////////////////////////////////////////////////
CATLEvent* CAudioEventManager::ConstructAudioEvent()
{
	CATLEvent* pEvent = new CATLEvent();
	pEvent->m_pImplData = m_pIImpl->ConstructEvent(*pEvent);
	m_constructedAudioEvents.push_back(pEvent);

	return pEvent;
}

//////////////////////////////////////////////////////////////////////////
void CAudioEventManager::ReleaseEvent(CATLEvent* const pEvent)
{
	CRY_ASSERT(pEvent != nullptr);

	m_constructedAudioEvents.remove(pEvent);
	m_pIImpl->DestructEvent(pEvent->m_pImplData);
	delete pEvent;
}

//////////////////////////////////////////////////////////////////////////
size_t CAudioEventManager::GetNumConstructed() const
{
	return m_constructedAudioEvents.size();
}

#if defined(INCLUDE_AUDIO_PRODUCTION_CODE)
//////////////////////////////////////////////////////////////////////////
void CAudioEventManager::DrawDebugInfo(IRenderAuxGeom& auxGeom, Vec3 const& listenerPosition, float const posX, float posY) const
{
	static float const headerColor[4] = { 1.0f, 0.5f, 0.0f, 0.7f };
	static float const itemPlayingColor[4] = { 0.1f, 0.7f, 0.1f, 0.9f };
	static float const itemLoadingColor[4] = { 0.9f, 0.2f, 0.2f, 0.9f };
	static float const itemVirtualColor[4] = { 0.1f, 0.8f, 0.8f, 0.9f };
	static float const itemOtherColor[4] = { 0.8f, 0.8f, 0.8f, 0.9f };

	CryFixedStringT<MaxControlNameLength> lowerCaseSearchString(g_cvars.m_pDebugFilter->GetString());
	lowerCaseSearchString.MakeLower();

	auxGeom.Draw2dLabel(posX, posY, 1.5f, headerColor, false, "Audio Events [%" PRISIZE_T "]", m_constructedAudioEvents.size());
	posY += 16.0f;

	for (auto const pEvent : m_constructedAudioEvents)
	{
		if (pEvent->m_pTrigger != nullptr)
		{
			Vec3 const& position = pEvent->m_pAudioObject->GetTransformation().GetPosition();
			float const distance = position.GetDistance(listenerPosition);

			if (g_cvars.m_debugDistance <= 0.0f || (g_cvars.m_debugDistance > 0.0f && distance < g_cvars.m_debugDistance))
			{
				char const* const szTriggerName = pEvent->m_pTrigger->m_name.c_str();
				CryFixedStringT<MaxControlNameLength> lowerCaseTriggerName(szTriggerName);
				lowerCaseTriggerName.MakeLower();
				bool const bDraw = ((lowerCaseSearchString.empty() || (lowerCaseSearchString == "0")) || (lowerCaseTriggerName.find(lowerCaseSearchString) != CryFixedStringT<MaxControlNameLength>::npos));

				if (bDraw)
				{
					float const* pColor = itemOtherColor;

					if (pEvent->IsPlaying())
					{
						pColor = itemPlayingColor;
					}
					else if (pEvent->m_state == EEventState::Loading)
					{
						pColor = itemLoadingColor;
					}
					else if (pEvent->m_state == EEventState::Virtual)
					{
						pColor = itemVirtualColor;
					}

					auxGeom.Draw2dLabel(posX, posY, 1.25f, pColor, false, "%s on %s", szTriggerName, pEvent->m_pAudioObject->m_name.c_str());

					posY += 11.0f;
				}
			}
		}
	}
}
#endif // INCLUDE_AUDIO_PRODUCTION_CODE
}      // namespace CryAudio
