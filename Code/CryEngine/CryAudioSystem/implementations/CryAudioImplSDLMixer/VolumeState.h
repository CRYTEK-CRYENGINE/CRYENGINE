// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include "Common.h"
#include <ISwitchStateConnection.h>
#include <PoolObject.h>
#include <CryAudio/IAudioInterfacesCommonData.h>

namespace CryAudio
{
namespace Impl
{
namespace SDL_mixer
{
class CVolumeState final : public ISwitchStateConnection, public CPoolObject<CVolumeState, stl::PSyncNone>
{
public:

	CVolumeState() = delete;
	CVolumeState(CVolumeState const&) = delete;
	CVolumeState(CVolumeState&&) = delete;
	CVolumeState& operator=(CVolumeState const&) = delete;
	CVolumeState& operator=(CVolumeState&&) = delete;

#if defined(CRY_AUDIO_IMPL_SDLMIXER_USE_DEBUG_CODE)
	explicit CVolumeState(
		SampleId const sampleId,
		float const value,
		char const* const szName)
		: m_sampleId(sampleId)
		, m_value(value)
		, m_name(szName)
	{}
#else
	explicit CVolumeState(
		SampleId const sampleId,
		float const value)
		: m_sampleId(sampleId)
		, m_value(value)
	{}
#endif  // CRY_AUDIO_IMPL_SDLMIXER_USE_DEBUG_CODE

	virtual ~CVolumeState() override = default;

	// ISwitchStateConnection
	virtual void Set(IObject* const pIObject) override;
	virtual void SetGlobally() override;
	// ~ISwitchStateConnection

private:

	SampleId const m_sampleId;
	float const    m_value;

#if defined(CRY_AUDIO_IMPL_SDLMIXER_USE_DEBUG_CODE)
	CryFixedStringT<MaxControlNameLength> const m_name;
#endif  // CRY_AUDIO_IMPL_SDLMIXER_USE_DEBUG_CODE
};
} // namespace SDL_mixer
} // namespace Impl
} // namespace CryAudio