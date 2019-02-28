// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include <array>
#include <CryAudio/IAudioSystem.h>

enum EPlayBehavior
{
	ePlayBehavior_Single = 0,
	ePlayBehavior_Delay,
	ePlayBehavior_TriggerRate,
};

enum class EAreaState
{
	Outside = 0,
	Near,
	Inside
};

static int constexpr numOcclusionTypes = static_cast<int>(CryAudio::EOcclusionType::Count - 1);

class AudioEntitiesUtils
{
	AudioEntitiesUtils() = delete;

public:

	static void                                                      Init();
	static CryAudio::ControlId                                       GetObstructionOcclusionSwitch();
	static const std::array<CryAudio::ControlId, numOcclusionTypes>& GetObstructionOcclusionStateIds();
	static const std::array<const char*, numOcclusionTypes>&         GetObstructionNames();

	static float AreaFadeEpsilon;

protected:

	static CryAudio::ControlId                                m_obstructionOcclusionSwitch;
	static std::array<const char*, numOcclusionTypes>         m_obstructionNames;
	static std::array<CryAudio::ControlId, numOcclusionTypes> m_obstructionStateIds;
};
