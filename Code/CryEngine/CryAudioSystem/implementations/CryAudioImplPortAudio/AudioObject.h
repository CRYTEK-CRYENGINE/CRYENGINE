// Copyright 2001-2016 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include "ATLEntities.h"

namespace CryAudio
{
namespace Impl
{
namespace PortAudio
{
class CAudioEvent;

class CAudioObject final : public IAudioObject
{
public:

	CAudioObject();
	virtual ~CAudioObject();

	void Update();
	void StopAudioEvent(uint32 const pathId);
	void RegisterAudioEvent(CAudioEvent* const pPAAudioEvent);
	void UnregisterAudioEvent(CAudioEvent* const pPAAudioEvent);

private:

	std::unordered_map<AudioEventId, CAudioEvent*> m_activeEvents;

	DELETE_DEFAULT_CONSTRUCTOR(CAudioObject);
	PREVENT_OBJECT_COPY(CAudioObject);
};
}
}
}
