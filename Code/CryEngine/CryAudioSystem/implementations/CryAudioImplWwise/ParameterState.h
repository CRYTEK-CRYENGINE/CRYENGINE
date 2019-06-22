// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include <ISwitchStateConnection.h>
#include <PoolObject.h>
#include <CryAudio/IAudioInterfacesCommonData.h>
#include <AK/SoundEngine/Common/AkTypes.h>

namespace CryAudio
{
namespace Impl
{
namespace Wwise
{
class CParameterState final : public ISwitchStateConnection, public CPoolObject<CParameterState, stl::PSyncNone>
{
public:

	CParameterState() = delete;
	CParameterState(CParameterState const&) = delete;
	CParameterState(CParameterState&&) = delete;
	CParameterState& operator=(CParameterState const&) = delete;
	CParameterState& operator=(CParameterState&&) = delete;

#if defined(CRY_AUDIO_IMPL_WWISE_USE_DEBUG_CODE)
	explicit CParameterState(
		AkUInt32 const id,
		AkRtpcValue const rtpcValue,
		char const* const szName)
		: m_id(id)
		, m_rtpcValue(rtpcValue)
		, m_name(szName)
	{}
#else
	explicit CParameterState(
		AkUInt32 const id,
		AkRtpcValue const rtpcValue)
		: m_id(id)
		, m_rtpcValue(rtpcValue)
	{}
#endif  // CRY_AUDIO_IMPL_WWISE_USE_DEBUG_CODE

	virtual ~CParameterState() override = default;

	// ISwitchStateConnection
	virtual void Set(IObject* const pIObject) override;
	virtual void SetGlobally() override;
	// ~ISwitchStateConnection

private:

	AkUInt32 const    m_id;
	AkRtpcValue const m_rtpcValue;

#if defined(CRY_AUDIO_IMPL_WWISE_USE_DEBUG_CODE)
	CryFixedStringT<MaxControlNameLength> const m_name;
#endif  // CRY_AUDIO_IMPL_WWISE_USE_DEBUG_CODE
};
} // namespace Wwise
} // namespace Impl
} // namespace CryAudio
