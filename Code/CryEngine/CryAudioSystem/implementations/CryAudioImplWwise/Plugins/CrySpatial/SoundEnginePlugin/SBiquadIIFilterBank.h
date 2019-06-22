// Copyright 2001-2019 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include "BiquadIIFilter.h"

namespace CryAudio
{
namespace Impl
{
namespace Wwise
{
namespace Plugins
{
struct SBiquadIIFilterBank final
{
	explicit SBiquadIIFilterBank(float const sampleRate)
		: filterBand00(BiquadIIFilter(EBiquadType::Peak, sampleRate))
		, filterBand01(BiquadIIFilter(EBiquadType::Peak, sampleRate))
		, filterBand02(BiquadIIFilter(EBiquadType::Peak, sampleRate))
		, filterBand03(BiquadIIFilter(EBiquadType::Peak, sampleRate))
		, filterBand04(BiquadIIFilter(EBiquadType::Peak, sampleRate))
		, filterBand05(BiquadIIFilter(EBiquadType::Peak, sampleRate))
		, filterBand06(BiquadIIFilter(EBiquadType::Peak, sampleRate))
		, filterBand07(BiquadIIFilter(EBiquadType::Peak, sampleRate))
		, filterBand08(BiquadIIFilter(EBiquadType::Peak, sampleRate))
		, filterBand09(BiquadIIFilter(EBiquadType::Highshelf, sampleRate))
		, filterBand10(BiquadIIFilter(EBiquadType::Peak, sampleRate))
		, filterBand11(BiquadIIFilter(EBiquadType::Highshelf, sampleRate))
	{
	}

	BiquadIIFilter filterBand00;
	BiquadIIFilter filterBand01;
	BiquadIIFilter filterBand02;
	BiquadIIFilter filterBand03;
	BiquadIIFilter filterBand04;
	BiquadIIFilter filterBand05;
	BiquadIIFilter filterBand06;
	BiquadIIFilter filterBand07;
	BiquadIIFilter filterBand08;
	BiquadIIFilter filterBand09;
	BiquadIIFilter filterBand10;
	BiquadIIFilter filterBand11;
};
} // namespace Plugins
} // namespace Wwise
} // namespace Impl
} // namespace CryAudio