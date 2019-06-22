// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include "Common.h"
#include <IParameterConnection.h>
#include <PoolObject.h>
#include <CryAudio/IAudioInterfacesCommonData.h>

namespace CryAudio
{
namespace Impl
{
namespace SDL_mixer
{
class CVolumeParameter final : public IParameterConnection, public CPoolObject<CVolumeParameter, stl::PSyncNone>
{
public:

	CVolumeParameter() = delete;
	CVolumeParameter(CVolumeParameter const&) = delete;
	CVolumeParameter(CVolumeParameter&&) = delete;
	CVolumeParameter& operator=(CVolumeParameter const&) = delete;
	CVolumeParameter& operator=(CVolumeParameter&&) = delete;

#if defined(CRY_AUDIO_IMPL_SDLMIXER_USE_DEBUG_CODE)
	explicit CVolumeParameter(
		SampleId const sampleId,
		float const multiplier,
		float const shift,
		char const* const szName)
		: m_sampleId(sampleId)
		, m_multiplier(multiplier)
		, m_shift(shift)
		, m_name(szName)
	{}
#else
	explicit CVolumeParameter(
		SampleId const sampleId,
		float const multiplier,
		float const shift)
		: m_sampleId(sampleId)
		, m_multiplier(multiplier)
		, m_shift(shift)
	{}
#endif  // CRY_AUDIO_IMPL_SDLMIXER_USE_DEBUG_CODE

	virtual ~CVolumeParameter() override = default;

	// IParameterConnection
	virtual void Set(IObject* const pIObject, float const value) override;
	virtual void SetGlobally(float const value) override;
	// ~IParameterConnection

private:

	SampleId const m_sampleId;
	float const    m_multiplier;
	float const    m_shift;

#if defined(CRY_AUDIO_IMPL_SDLMIXER_USE_DEBUG_CODE)
	CryFixedStringT<MaxControlNameLength> const m_name;
#endif  // CRY_AUDIO_IMPL_SDLMIXER_USE_DEBUG_CODE
};
} // namespace SDL_mixer
} // namespace Impl
} // namespace CryAudio