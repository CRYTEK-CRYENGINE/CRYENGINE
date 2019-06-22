// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include <ISwitchStateConnection.h>
#include <PoolObject.h>
#include <CryAudio/IAudioInterfacesCommonData.h>

namespace CryAudio
{
namespace Impl
{
namespace Fmod
{
class CVcaState final : public ISwitchStateConnection, public CPoolObject<CVcaState, stl::PSyncNone>
{
public:

	CVcaState() = delete;
	CVcaState(CVcaState const&) = delete;
	CVcaState(CVcaState&&) = delete;
	CVcaState& operator=(CVcaState const&) = delete;
	CVcaState& operator=(CVcaState&&) = delete;

#if defined(CRY_AUDIO_IMPL_FMOD_USE_DEBUG_CODE)
	explicit CVcaState(
		FMOD::Studio::VCA* const pVca,
		float const value,
		char const* const szName)
		: m_pVca(pVca)
		, m_value(value)
		, m_name(szName)
	{}
#else
	explicit CVcaState(
		FMOD::Studio::VCA* const pVca,
		float const value)
		: m_pVca(pVca)
		, m_value(value)
	{}
#endif  // CRY_AUDIO_IMPL_FMOD_USE_DEBUG_CODE

	virtual ~CVcaState() override = default;

	// ISwitchStateConnection
	virtual void Set(IObject* const pIObject) override;
	virtual void SetGlobally() override;
	// ~ISwitchStateConnection

private:

	FMOD::Studio::VCA* const m_pVca;
	float const              m_value;

#if defined(CRY_AUDIO_IMPL_FMOD_USE_DEBUG_CODE)
	CryFixedStringT<MaxControlNameLength> const m_name;
#endif  // CRY_AUDIO_IMPL_FMOD_USE_DEBUG_CODE
};
} // namespace Fmod
} // namespace Impl
} // namespace CryAudio
