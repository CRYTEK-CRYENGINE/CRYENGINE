// Copyright 2001-2017 Crytek GmbH / Crytek Group. All rights reserved. 

#pragma once

#include <CryAudio/IAudioInterfacesCommonData.h>
#include <CryAudio/IProfileData.h>

namespace CryAudio
{
class CProfileData final : public IProfileData
{
public:

	CProfileData() = default;
	CProfileData(CProfileData const&) = delete;
	CProfileData(CProfileData&&) = delete;
	CProfileData& operator=(CProfileData const&) = delete;
	CProfileData& operator=(CProfileData&&) = delete;

	// CryAudio::IProfileData
	virtual char const* GetImplName() const override;
	// ~CryAudio::IProfileData

	void SetImplName(char const* const szName);

private:

	CryFixedStringT<MaxMiscStringLength> m_implName;
};
} // namespace CryAudio
