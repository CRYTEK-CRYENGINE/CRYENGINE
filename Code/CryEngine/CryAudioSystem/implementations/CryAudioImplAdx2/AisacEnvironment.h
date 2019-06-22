// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include <IEnvironmentConnection.h>
#include <PoolObject.h>
#include <CryAudio/IAudioInterfacesCommonData.h>

#include <cri_atom_ex.h>

namespace CryAudio
{
namespace Impl
{
namespace Adx2
{
class CAisacEnvironment final : public IEnvironmentConnection, public CPoolObject<CAisacEnvironment, stl::PSyncNone>
{
public:

	CAisacEnvironment() = delete;
	CAisacEnvironment(CAisacEnvironment const&) = delete;
	CAisacEnvironment(CAisacEnvironment&&) = delete;
	CAisacEnvironment& operator=(CAisacEnvironment const&) = delete;
	CAisacEnvironment& operator=(CAisacEnvironment&&) = delete;

#if defined(CRY_AUDIO_IMPL_ADX2_USE_DEBUG_CODE)
	explicit CAisacEnvironment(
		CriAtomExAisacControlId const id,
		float const multiplier,
		float const shift,
		char const* const szName)
		: m_id(id)
		, m_multiplier(multiplier)
		, m_shift(shift)
		, m_name(szName)
	{}
#else
	explicit CAisacEnvironment(
		CriAtomExAisacControlId const id,
		float const multiplier,
		float const shift)
		: m_id(id)
		, m_multiplier(multiplier)
		, m_shift(shift)
	{}
#endif  // CRY_AUDIO_IMPL_ADX2_USE_DEBUG_CODE

	virtual ~CAisacEnvironment() override = default;

	// IEnvironmentConnection
	virtual void Set(IObject* const pIObject, float const amount) override;
	// ~IEnvironmentConnection

private:

	CriAtomExAisacControlId const m_id;
	float const                   m_multiplier;
	float const                   m_shift;

#if defined(CRY_AUDIO_IMPL_ADX2_USE_DEBUG_CODE)
	CryFixedStringT<MaxControlNameLength> const m_name;
#endif  // CRY_AUDIO_IMPL_ADX2_USE_DEBUG_CODE
};
} // namespace Adx2
} // namespace Impl
} // namespace CryAudio
