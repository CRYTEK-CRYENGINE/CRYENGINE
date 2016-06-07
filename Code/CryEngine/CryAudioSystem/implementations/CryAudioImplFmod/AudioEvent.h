// Copyright 2001-2016 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include "Common.h"
#include <ATLEntityData.h>

// Fmod forward declarations.
namespace FMOD
{
class ChannelGroup;
class DSP;

namespace Studio
{
class EventInstance;
class ParameterInstance;
}
}

namespace CryAudio
{
namespace Impl
{
namespace Fmod
{
class CAudioEnvironment;
class CAudioObject;

class CAudioEvent final : public IAudioEvent
{
public:

	explicit CAudioEvent(AudioEventId const _id)
		: m_id(_id)
		, m_eventPathId(AUDIO_INVALID_CRC32)
		, m_lowpassFrequencyMax(0.0f)
		, m_lowpassFrequencyMin(0.0f)
		, m_pInstance(nullptr)
		, m_pMasterTrack(nullptr)
		, m_pLowpass(nullptr)
		, m_pOcclusionParameter(nullptr)
		, m_pAudioObject(nullptr)
	{}

	virtual ~CAudioEvent() {}

	bool                         PrepareForOcclusion();
	void                         SetObstructionOcclusion(float const obstruction, float const occlusion);
	void                         Reset();
	AudioEventId                 GetId() const                                             { return m_id; }
	uint32                       GetEventPathId() const                                    { return m_eventPathId; }
	void                         SetEventPathId(uint32 const eventPathId)                  { m_eventPathId = eventPathId; }
	FMOD::Studio::EventInstance* GetInstance() const                                       { return m_pInstance; }
	void                         SetInstance(FMOD::Studio::EventInstance* const pInstance) { m_pInstance = pInstance; }
	CAudioObject*                GetAudioObjectData() const                                { return m_pAudioObject; }
	void                         SetAudioObjectData(CAudioObject* const pAudioObject)      { m_pAudioObject = pAudioObject; }
	void                         TrySetEnvironment(CAudioEnvironment const* const pEnvironment, float const value);

private:

	AudioEventId const               m_id;
	uint32                           m_eventPathId;

	float                            m_lowpassFrequencyMax;
	float                            m_lowpassFrequencyMin;

	FMOD::Studio::EventInstance*     m_pInstance;
	FMOD::ChannelGroup*              m_pMasterTrack;
	FMOD::DSP*                       m_pLowpass;
	FMOD::Studio::ParameterInstance* m_pOcclusionParameter;
	CAudioObject*                    m_pAudioObject;

	DELETE_DEFAULT_CONSTRUCTOR(CAudioEvent);
	PREVENT_OBJECT_COPY(CAudioEvent);
};
}
}
}
