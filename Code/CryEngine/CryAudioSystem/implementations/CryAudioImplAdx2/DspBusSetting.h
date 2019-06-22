// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include <ISettingConnection.h>
#include <PoolObject.h>
#include <CryAudio/IAudioInterfacesCommonData.h>

namespace CryAudio
{
namespace Impl
{
namespace Adx2
{
#if defined(CRY_AUDIO_IMPL_ADX2_USE_DEBUG_CODE)
constexpr char const* g_debugNoneDspBusSetting = "<none>";
extern CryFixedStringT<MaxControlNameLength> g_debugCurrentDspBusSettingName;
#endif  // CRY_AUDIO_IMPL_ADX2_USE_DEBUG_CODE

class CDspBusSetting final : public ISettingConnection, public CPoolObject<CDspBusSetting, stl::PSyncNone>
{
public:

	CDspBusSetting() = delete;
	CDspBusSetting(CDspBusSetting const&) = delete;
	CDspBusSetting(CDspBusSetting&&) = delete;
	CDspBusSetting& operator=(CDspBusSetting const&) = delete;
	CDspBusSetting& operator=(CDspBusSetting&&) = delete;

	explicit CDspBusSetting(char const* const szName)
		: m_name(szName)
	{}

	virtual ~CDspBusSetting() override = default;

	// ISettingConnection
	virtual void Load() override;
	virtual void Unload() override;
	// ~ISettingConnection

private:

	CryFixedStringT<MaxControlNameLength> const m_name;
};
} // namespace Adx2
} // namespace Impl
} // namespace CryAudio
