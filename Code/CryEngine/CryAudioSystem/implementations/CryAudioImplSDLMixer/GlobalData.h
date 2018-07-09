// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

namespace CryAudio
{
namespace Impl
{
namespace SDL_mixer
{
static constexpr char const* s_szImplFolderName = "sdlmixer";

// XML tags
static constexpr char const* s_szFileTag = "Sample";

// XML attributes
static constexpr char const* s_szPathAttribute = "path";
static constexpr char const* s_szPanningEnabledAttribute = "enable_panning";
static constexpr char const* s_szAttenuationEnabledAttribute = "enable_dist_attenuation";
static constexpr char const* s_szAttenuationMinDistanceAttribute = "attenuation_dist_min";
static constexpr char const* s_szAttenuationMaxDistanceAttribute = "attenuation_dist_max";
static constexpr char const* s_szVolumeAttribute = "volume";
static constexpr char const* s_szLoopCountAttribute = "loop_count";
static constexpr char const* s_szFadeInTimeAttribute = "fade_in_time";
static constexpr char const* s_szFadeOutTimeAttribute = "fade_out_time";
static constexpr char const* s_szValueAttribute = "value";
static constexpr char const* s_szMutiplierAttribute = "value_multiplier";
static constexpr char const* s_szShiftAttribute = "value_shift";

// XML values
static constexpr char const* s_szTrueValue = "true";
static constexpr char const* s_szStartValue = "start";
static constexpr char const* s_szStopValue = "stop";
static constexpr char const* s_szPauseValue = "pause";
static constexpr char const* s_szResumeValue = "resume";

// Default values
static constexpr float const s_defaultFadeInTime = 0.0f;
static constexpr float const s_defaultFadeOutTime = 0.0f;
static constexpr float const s_defaultMinAttenuationDist = 0.0f;
static constexpr float const s_defaultMaxAttenuationDist = 100.0f;
static constexpr float const s_defaultParamMultiplier = 1.0f;
static constexpr float const s_defaultParamShift = 0.0f;
static constexpr float const s_defaultStateValue = 1.0f;
} // namespace SDL_mixer
} // namespace Impl
} // namespace CryAudio
