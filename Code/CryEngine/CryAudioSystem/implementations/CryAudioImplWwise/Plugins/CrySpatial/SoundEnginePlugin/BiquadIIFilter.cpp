// Copyright 2001-2019 Crytek GmbH / Crytek Group. All rights reserved.

#include "stdafx.h"
#include "BiquadIIFilter.h"
#include "CrySpatialMath.h"
#include <cmath>

namespace CryAudio
{
namespace Impl
{
namespace Wwise
{
namespace Plugins
{
//////////////////////////////////////////////////////////////////////////
void BiquadIIFilter::ComputeCoefficients(int const frequency, float const qualityFactor, float const peakGain)
{
	double const factorV = pow(10.0f, fabs(peakGain) / 20.0);
	double const factorK = tan(g_pi * (frequency / m_sampleRate));
	double const factorKSpuare = factorK * factorK;

	switch (m_filterType)
	{
	case EBiquadType::Lowpass:
		{
			double const norm = 1 / (1 + factorK / qualityFactor + factorKSpuare);
			m_coefficientA0 = static_cast<AkReal32>(factorKSpuare * norm);
			m_coefficientA1 = static_cast<AkReal32>(2 * m_coefficientA0);
			m_coefficientA2 = m_coefficientA0;
			m_coefficientB0 = static_cast<AkReal32>(2 * (factorKSpuare - 1) * norm);
			m_coefficientB1 = static_cast<AkReal32>((1 - factorK / qualityFactor + factorKSpuare) * norm);
			break;
		}
	case EBiquadType::Highpass:
		{
			double const norm = 1 / (1 + factorK / qualityFactor + factorKSpuare);
			m_coefficientA0 = static_cast<AkReal32>(1 * norm);
			m_coefficientA1 = static_cast<AkReal32>(-2 * m_coefficientA0);
			m_coefficientA2 = m_coefficientA0;
			m_coefficientB0 = static_cast<AkReal32>(2 * (factorKSpuare - 1) * norm);
			m_coefficientB1 = static_cast<AkReal32>((1 - factorK / qualityFactor + factorKSpuare) * norm);
			break;
		}
	case EBiquadType::Bandpass:
		{
			double const norm = 1 / (1 + factorK / qualityFactor + factorKSpuare);
			m_coefficientA0 = static_cast<AkReal32>(factorK / qualityFactor * norm);
			m_coefficientA1 = 0.0f;
			m_coefficientA2 = -m_coefficientA0;
			m_coefficientB0 = static_cast<AkReal32>(2 * (factorKSpuare - 1) * norm);
			m_coefficientB1 = static_cast<AkReal32>((1 - factorK / qualityFactor + factorKSpuare) * norm);
			break;
		}
	case EBiquadType::Notch:
		{
			double const norm = 1 / (1 + factorK / qualityFactor + factorKSpuare);
			m_coefficientA0 = static_cast<AkReal32>((1 + factorKSpuare) * norm);
			m_coefficientA1 = static_cast<AkReal32>(2 * (factorKSpuare - 1) * norm);
			m_coefficientA2 = m_coefficientA0;
			m_coefficientB0 = m_coefficientA1;
			m_coefficientB1 = static_cast<AkReal32>((1 - factorK / qualityFactor + factorKSpuare) * norm);
			break;
		}
	case EBiquadType::Peak:
		{
			if (peakGain >= 0)    // boost
			{
				double const norm = 1 / (1 + 1 / qualityFactor * factorK + factorKSpuare);
				m_coefficientA0 = static_cast<AkReal32>((1 + factorV / qualityFactor * factorK + factorKSpuare) * norm);
				m_coefficientA1 = static_cast<AkReal32>(2 * (factorKSpuare - 1) * norm);
				m_coefficientA2 = static_cast<AkReal32>((1 - factorV / qualityFactor * factorK + factorKSpuare) * norm);
				m_coefficientB0 = m_coefficientA1;
				m_coefficientB1 = static_cast<AkReal32>((1 - 1 / qualityFactor * factorK + factorKSpuare) * norm);
			}
			else    // cut
			{
				double const norm = 1 / (1 + factorV / qualityFactor * factorK + factorKSpuare);
				m_coefficientA0 = static_cast<AkReal32>((1 + 1 / qualityFactor * factorK + factorKSpuare) * norm);
				m_coefficientA1 = static_cast<AkReal32>(2 * (factorKSpuare - 1) * norm);
				m_coefficientA2 = static_cast<AkReal32>((1 - 1 / qualityFactor * factorK + factorKSpuare) * norm);
				m_coefficientB0 = m_coefficientA1;
				m_coefficientB1 = static_cast<AkReal32>((1 - factorV / qualityFactor * factorK + factorKSpuare) * norm);
			}

			break;
		}
	case EBiquadType::Lowshelf:
		{
			if (peakGain >= 0) // boost
			{
				double const squareRoot2FactorV = sqrt(2 * factorV);
				double const norm = 1 / (1 + g_rootTwo * factorK + factorKSpuare);
				m_coefficientA0 = static_cast<AkReal32>((1 + squareRoot2FactorV * factorK + factorV * factorKSpuare) * norm);
				m_coefficientA1 = static_cast<AkReal32>(2 * (factorV * factorKSpuare - 1) * norm);
				m_coefficientA2 = static_cast<AkReal32>((1 - squareRoot2FactorV * factorK + factorV * factorKSpuare) * norm);
				m_coefficientB0 = static_cast<AkReal32>(2 * (factorKSpuare - 1) * norm);
				m_coefficientB1 = static_cast<AkReal32>((1 - g_rootTwo * factorK + factorKSpuare) * norm);
			}
			else    // cut
			{
				double const squareRoot2FactorV = sqrt(2 * factorV);
				double const norm = 1 / (1 + squareRoot2FactorV * factorK + factorV * factorKSpuare);
				m_coefficientA0 = static_cast<AkReal32>((1 + g_rootTwo * factorK + factorKSpuare) * norm);
				m_coefficientA1 = static_cast<AkReal32>(2 * (factorKSpuare - 1) * norm);
				m_coefficientA2 = static_cast<AkReal32>((1 - g_rootTwo * factorK + factorKSpuare) * norm);
				m_coefficientB0 = static_cast<AkReal32>(2 * (factorV * factorKSpuare - 1) * norm);
				m_coefficientB1 = static_cast<AkReal32>((1 - squareRoot2FactorV * factorK + factorV * factorKSpuare) * norm);
			}

			break;
		}
	case EBiquadType::Highshelf:
		{
			if (peakGain >= 0) // boost
			{
				double const squareRoot2FactorV = sqrt(2 * factorV);
				double const norm = 1 / (1 + g_rootTwo * factorK + factorKSpuare);
				m_coefficientA0 = static_cast<AkReal32>((factorV + squareRoot2FactorV * factorK + factorKSpuare) * norm);
				m_coefficientA1 = static_cast<AkReal32>(2 * (factorKSpuare - factorV) * norm);
				m_coefficientA2 = static_cast<AkReal32>((factorV - squareRoot2FactorV * factorK + factorKSpuare) * norm);
				m_coefficientB0 = static_cast<AkReal32>(2 * (factorKSpuare - 1) * norm);
				m_coefficientB1 = static_cast<AkReal32>((1 - g_rootTwo * factorK + factorKSpuare) * norm);
			}
			else    // cut
			{
				double const squareRoot2FactorV = sqrt(2 * factorV);
				double const norm = 1 / (factorV + squareRoot2FactorV * factorK + factorKSpuare);
				m_coefficientA0 = static_cast<AkReal32>((1 + g_rootTwo * factorK + factorKSpuare) * norm);
				m_coefficientA1 = static_cast<AkReal32>(2 * (factorKSpuare - 1) * norm);
				m_coefficientA2 = static_cast<AkReal32>((1 - g_rootTwo * factorK + factorKSpuare) * norm);
				m_coefficientB0 = static_cast<AkReal32>(2 * (factorKSpuare - factorV) * norm);
				m_coefficientB1 = static_cast<AkReal32>((factorV - squareRoot2FactorV * factorK + factorKSpuare) * norm);
			}

			break;
		}
	}
}

//////////////////////////////////////////////////////////////////////////
AkReal32 BiquadIIFilter::ProcessSample(AkReal32 const sample)
{
	AkReal32 const fCurOut = sample * m_coefficientA0 + m_lastSample1;
	m_lastSample1 = sample * m_coefficientA1 + m_lastSample2 - m_coefficientB0 * fCurOut;
	m_lastSample2 = sample * m_coefficientA2 - m_coefficientB1 * fCurOut;
	return fCurOut;
}
}// namespace Plugins
}// namespace Wwise
}// namespace Impl
}// namespace CryAudio