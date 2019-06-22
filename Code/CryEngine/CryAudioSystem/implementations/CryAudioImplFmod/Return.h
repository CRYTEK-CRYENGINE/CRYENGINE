// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include <IEnvironmentConnection.h>
#include <PoolObject.h>
#include <CryAudio/IAudioInterfacesCommonData.h>

namespace CryAudio
{
namespace Impl
{
namespace Fmod
{
class CReturn : public IEnvironmentConnection, public CPoolObject<CReturn, stl::PSyncNone>
{
public:

	CReturn() = delete;
	CReturn(CReturn const&) = delete;
	CReturn(CReturn&&) = delete;
	CReturn& operator=(CReturn const&) = delete;
	CReturn& operator=(CReturn&&) = delete;

#if defined(CRY_AUDIO_IMPL_FMOD_USE_DEBUG_CODE)
	explicit CReturn(FMOD::Studio::Bus* const pBus, char const* const szName)
		: m_pBus(pBus)
		, m_name(szName)
	{}
#else
	explicit CReturn(FMOD::Studio::Bus* const pBus)
		: m_pBus(pBus)
	{}
#endif  // CRY_AUDIO_IMPL_FMOD_USE_DEBUG_CODE

	virtual ~CReturn() override;

	// IEnvironmentConnection
	virtual void Set(IObject* const pIObject, float const amount) override;
	// ~IEnvironmentConnection

	FMOD::Studio::Bus* GetBus() const { return m_pBus; }

private:

	FMOD::Studio::Bus* const m_pBus;

#if defined(CRY_AUDIO_IMPL_FMOD_USE_DEBUG_CODE)
	CryFixedStringT<MaxControlNameLength> const m_name;
#endif  // CRY_AUDIO_IMPL_FMOD_USE_DEBUG_CODE
};
} // namespace Fmod
} // namespace Impl
} // namespace CryAudio
