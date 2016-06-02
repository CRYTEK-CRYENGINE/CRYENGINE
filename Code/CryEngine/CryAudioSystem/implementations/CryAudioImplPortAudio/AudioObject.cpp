// Copyright 2001-2016 Crytek GmbH / Crytek Group. All rights reserved.

#include "stdafx.h"
#include "AudioObject.h"
#include "AudioEvent.h"

using namespace CryAudio::Impl::PortAudio;

//////////////////////////////////////////////////////////////////////////
CAudioObject::CAudioObject()
{
}

//////////////////////////////////////////////////////////////////////////
CAudioObject::~CAudioObject()
{
}

//////////////////////////////////////////////////////////////////////////
void CAudioObject::Update()
{
	for (auto& audioEventPair : m_activeEvents)
	{
		audioEventPair.second->Update();
	}
}

//////////////////////////////////////////////////////////////////////////
void CAudioObject::StopAudioEvent(uint32 const pathId)
{
	for (auto const& audioEventPair : m_activeEvents)
	{
		if (audioEventPair.second->pathId == pathId)
		{
			audioEventPair.second->Stop();
		}
	}
}

//////////////////////////////////////////////////////////////////////////
void CAudioObject::RegisterAudioEvent(CAudioEvent* const pPAAudioEvent)
{
	m_activeEvents.insert(std::make_pair(pPAAudioEvent->audioEventId, pPAAudioEvent));
}

//////////////////////////////////////////////////////////////////////////
void CAudioObject::UnregisterAudioEvent(CAudioEvent* const pPAAudioEvent)
{
	m_activeEvents.erase(pPAAudioEvent->audioEventId);
}
