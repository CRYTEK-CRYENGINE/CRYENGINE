// Copyright 2001-2019 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include "stdafx.h"
#include <AK/SoundEngine/Common/AkTypes.h>

namespace CryAudio
{
namespace Impl
{
namespace Wwise
{
namespace Plugins
{
enum class EBiquadType
{
	Lowpass,
	Highpass,
	Bandpass,
	Notch,
	Peak,
	Lowshelf,
	Highshelf,
};

class BiquadIIFilter final
{
public:

	BiquadIIFilter(EBiquadType const filtertype, float const sampleRate)
	{
		m_filterType = filtertype;
		m_sampleRate = sampleRate;
	}

	~BiquadIIFilter() = default;

	void     ComputeCoefficients(int const frequency, float const qualityFactor, float const peakGain);

	AkReal32 ProcessSample(AkReal32 const sample);

private:

	float       m_sampleRate = 48000.0f;
	EBiquadType m_filterType = EBiquadType::Lowpass;

	AkReal32    m_lastSample1 = 0.0f;
	AkReal32    m_lastSample2 = 0.0f;

	AkReal32    m_coefficientA0 = 0.0f;
	AkReal32    m_coefficientA1 = 0.0f;
	AkReal32    m_coefficientA2 = 0.0f;
	AkReal32    m_coefficientB0 = 0.0f;
	AkReal32    m_coefficientB1 = 0.0f;
};
} // namespace Plugins
} // namespace Wwise
} // namespace Impl
} // namespace CryAudio
