// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include <ISwitchStateConnection.h>
#include <PoolObject.h>
#include <CryAudio/IAudioInterfacesCommonData.h>

#include <cri_atom_ex.h>

namespace CryAudio
{
namespace Impl
{
namespace Adx2
{
class CAisacState final : public ISwitchStateConnection, public CPoolObject<CAisacState, stl::PSyncNone>
{
public:

	CAisacState() = delete;
	CAisacState(CAisacState const&) = delete;
	CAisacState(CAisacState&&) = delete;
	CAisacState& operator=(CAisacState const&) = delete;
	CAisacState& operator=(CAisacState&&) = delete;

#if defined(CRY_AUDIO_IMPL_ADX2_USE_DEBUG_CODE)
	explicit CAisacState(CriAtomExAisacControlId const id, CriFloat32 const value, char const* const szName)
		: m_id(id)
		, m_value(value)
		, m_name(szName)
	{}
#else
	explicit CAisacState(CriAtomExAisacControlId const id, CriFloat32 const value)
		: m_id(id)
		, m_value(value)
	{}
#endif  // CRY_AUDIO_IMPL_ADX2_USE_DEBUG_CODE

	virtual ~CAisacState() override = default;

	// ISwitchStateConnection
	virtual void Set(IObject* const pIObject) override;
	virtual void SetGlobally() override;
	// ~ISwitchStateConnection

private:

	CriAtomExAisacControlId const m_id;
	CriFloat32 const              m_value;

#if defined(CRY_AUDIO_IMPL_ADX2_USE_DEBUG_CODE)
	CryFixedStringT<MaxControlNameLength> const m_name;
#endif  // CRY_AUDIO_IMPL_ADX2_USE_DEBUG_CODE
};
} // namespace Adx2
} // namespace Impl
} // namespace CryAudio
