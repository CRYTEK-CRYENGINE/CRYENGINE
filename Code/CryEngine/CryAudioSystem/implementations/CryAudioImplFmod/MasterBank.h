// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include <CryAudio/IAudioInterfacesCommonData.h>

namespace CryAudio
{
namespace Impl
{
namespace Fmod
{
struct SMasterBank final
{
	SMasterBank() = delete;
	SMasterBank& operator=(SMasterBank const&) = delete;

	explicit SMasterBank(
		CryFixedStringT<MaxFilePathLength> const& bankPath_,
		CryFixedStringT<MaxFilePathLength> const& stringsBankPath_,
		CryFixedStringT<MaxFilePathLength> const& assetsBankPath_,
		CryFixedStringT<MaxFilePathLength> const& streamsBankPath_)
		: pBank(nullptr)
		, pStringsBank(nullptr)
		, pAssetsBank(nullptr)
		, pStreamsBank(nullptr)
		, bankPath(bankPath_)
		, stringsBankPath(stringsBankPath_)
		, assetsBankPath(assetsBankPath_)
		, streamsBankPath(streamsBankPath_)
	{}

	~SMasterBank() = default;

	FMOD::Studio::Bank*                pBank;
	FMOD::Studio::Bank*                pStringsBank;
	FMOD::Studio::Bank*                pAssetsBank;
	FMOD::Studio::Bank*                pStreamsBank;

	CryFixedStringT<MaxFilePathLength> bankPath;
	CryFixedStringT<MaxFilePathLength> stringsBankPath;
	CryFixedStringT<MaxFilePathLength> assetsBankPath;
	CryFixedStringT<MaxFilePathLength> streamsBankPath;
};
} // namespace Fmod
} // namespace Impl
} // namespace CryAudio
