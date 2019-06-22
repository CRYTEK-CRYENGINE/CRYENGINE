// Copyright 2001-2019 Crytek GmbH / Crytek Group. All rights reserved.

#include "stdafx.h"

#include "BiquadIIFilter.h"
#include "CrySpatialFX.h"
#include "CrySpatialConfig.h"
#include "CrySpatialMath.h"
#include "SAkUserData.h"
#include <AK/SoundEngine/Common/IAkPluginMemAlloc.h>
#include <AK/SoundEngine/Platforms/SSE/AkSimd.h>
#include <memory>
#include <algorithm>

namespace CryAudio
{
namespace Impl
{
namespace Wwise
{
namespace Plugins
{

AK_IMPLEMENT_PLUGIN_FACTORY(CrySpatialFX, AkPluginTypeMixer, CrySpatialConfig::CompanyID, CrySpatialConfig::PluginID)

//////////////////////////////////////////////////////////////////////////
float CrySpatialFX::DecibelToVolume(float const decibel)
{
	return powf(10.0f, 0.05f * decibel);
}

//////////////////////////////////////////////////////////////////////////
float CrySpatialFX::VolumeToDecibel(float const volume)
{
	return 20.0f * log10f(volume);
}

//////////////////////////////////////////////////////////////////////////
float CrySpatialFX::ComputeFade(float const volumeFactor, float const strength = 1.5f, EFadeType fadeType = EFadeType::FadeinHyperbole)
{
	float returnVolume = 0.0f;
	switch (fadeType)
	{
	case EFadeType::FadeinHyperbole:
		{
			returnVolume = (powf(/*euler*/ 2.718f, (strength * (volumeFactor - 1))) * volumeFactor);
			break;
		}
	case EFadeType::FadeoutHyperbole:
		{
			returnVolume = (powf(/*euler*/ 2.718f, (strength * (-volumeFactor))) * (-volumeFactor + 1));
			break;
		}
	case EFadeType::FadeinLogarithmic:
		{
			returnVolume = (powf(/*euler*/ 2.718f, (strength * (-volumeFactor))) * (volumeFactor - 1)) + 1;
			break;
		}
	case EFadeType::FadeoutLogarithmic:
		{
			returnVolume = (powf(/*euler*/ 2.718f, (strength * (volumeFactor - 1))) * (-volumeFactor)) + 1;
			break;
		}
	default:  // EFadeType::FadeinHyperbole
		{
			returnVolume = (powf(/*euler*/ 2.718f, (strength * (volumeFactor - 1))) * volumeFactor);
			break;
		}
	}

	return returnVolume;
}

//////////////////////////////////////////////////////////////////////////
void CrySpatialFX::ComputeQuadrant()
{
	float bigAzimuth = 0.0f;
	auto smallAzimuth = static_cast<float>(fabs(static_cast<AkReal64>(m_gameAzimuth) / g_piHalf));
	smallAzimuth = modff(smallAzimuth, &bigAzimuth);

	if (m_gameAzimuth < 0.0f)
	{
		bigAzimuth = (bigAzimuth == 0.0f) ? 3.0f : 2.0f;
	}

	m_quadrant = static_cast<int>(bigAzimuth);
	m_quadrantFine = smallAzimuth;
}

//////////////////////////////////////////////////////////////////////////
AK::IAkPlugin* CreateCrySpatialFX(AK::IAkPluginMemAlloc* in_pAllocator)
{
	return AK_PLUGIN_NEW(in_pAllocator, CrySpatialFX());
}

//////////////////////////////////////////////////////////////////////////
AK::IAkPluginParam* CreateCrySpatialFXParams(AK::IAkPluginMemAlloc* in_pAllocator)
{
	return AK_PLUGIN_NEW(in_pAllocator, CrySpatialFXParams());
}

//////////////////////////////////////////////////////////////////////////
AK::IAkPluginParam* CreateCrySpatialFXAttachmentParams(AK::IAkPluginMemAlloc* in_pAllocator)
{
	return AK_PLUGIN_NEW(in_pAllocator, CrySpatialFXAttachmentParams());
}

//////////////////////////////////////////////////////////////////////////
AK::PluginRegistration CrySpatialFXAttachmentParamsRegistration(
	AkPluginTypeEffect,
	CrySpatialAttachmentConfig::CompanyID,
	CrySpatialAttachmentConfig::PluginID,
	nullptr,
	CreateCrySpatialFXAttachmentParams);

//////////////////////////////////////////////////////////////////////////
AKRESULT CrySpatialFX::Init(
	AK::IAkPluginMemAlloc* in_pAllocator,
	AK::IAkMixerPluginContext* in_pContext,
	AK::IAkPluginParam* in_pParams,
	AkAudioFormat& in_rFormat)
{
	m_pParams = (CrySpatialFXParams*)in_pParams;
	m_pAllocator = in_pAllocator;
	m_pContext = in_pContext;
	m_numOutputChannels = in_rFormat.GetNumChannels();
	m_sampleRate = static_cast<float>(in_pContext->GlobalContext()->GetSampleRate()); //!< float for calculations with int

	AkAudioSettings settings;
	in_pContext->GlobalContext()->GetAudioSettings(settings);
	m_bufferSize = settings.uNumSamplesPerFrame;

	m_pIntermediateBufferDirect = static_cast<AkSampleType*>(AK_PLUGIN_ALLOC(m_pAllocator, sizeof(AkSampleType) * m_bufferSize));
	AKPLATFORM::AkMemSet(m_pIntermediateBufferDirect, 0, sizeof(AkSampleType) * m_bufferSize);

	m_pIntermediateBufferConcealed = static_cast<AkSampleType*>(AK_PLUGIN_ALLOC(m_pAllocator, sizeof(AkSampleType) * m_bufferSize));
	AKPLATFORM::AkMemSet(m_pIntermediateBufferConcealed, 0, sizeof(AkSampleType) * m_bufferSize);

	return AK_Success;
}

//////////////////////////////////////////////////////////////////////////
AKRESULT CrySpatialFX::Term(AK::IAkPluginMemAlloc* in_pAllocator)
{
	AK_PLUGIN_FREE(m_pAllocator, m_pIntermediateBufferConcealed);
	AK_PLUGIN_FREE(m_pAllocator, m_pIntermediateBufferDirect);

	for (SAkUserData* const pVar : m_activeUserData)
	{
		AK_PLUGIN_FREE(m_pAllocator, pVar->pVoiceDelayBuffer->GetChannel(0));
		delete pVar;
	}

	m_activeUserData.clear();

	AK_PLUGIN_DELETE(in_pAllocator, this);

	return AK_Success;
}

//////////////////////////////////////////////////////////////////////////
AKRESULT CrySpatialFX::Reset()
{
	return AK_Success;
}

//////////////////////////////////////////////////////////////////////////
AKRESULT CrySpatialFX::GetPluginInfo(AkPluginInfo& out_rPluginInfo)
{
	out_rPluginInfo.eType = AkPluginTypeMixer;
	out_rPluginInfo.bIsInPlace = true;
	out_rPluginInfo.uBuildVersion = AK_WWISESDK_VERSION_COMBINED;
	return AK_Success;
}

//////////////////////////////////////////////////////////////////////////
void CrySpatialFX::OnInputConnected(AK::IAkMixerInputContext* in_pInput)
{
	SAkUserData* userData = new SAkUserData();

	// DelayBuffer Allocation
	AkChannelConfig cfg;
	cfg.SetStandard(AK_SPEAKER_SETUP_MONO);

	{
		std::unique_ptr<AkAudioBuffer> pDelayBuffer = std::make_unique<AkAudioBuffer>();
		m_pMemAlloc = (AkSampleType*)(AK_PLUGIN_ALLOC(m_pAllocator, sizeof(AkSampleType) * m_delayBufferSize));
		pDelayBuffer->AttachContiguousDeinterleavedData(m_pMemAlloc, m_delayBufferSize, 0, cfg);
		pDelayBuffer->ZeroPadToMaxFrames();
		pDelayBuffer->uValidFrames = pDelayBuffer->MaxFrames();
		userData->pVoiceDelayBuffer = std::move(pDelayBuffer);
	}

	// Setup FilterBanks
	{
		float const sampleRate = m_sampleRate;
		userData->pFilterBankA = new SBiquadIIFilterBank(sampleRate);
		userData->pFilterBankB = new SBiquadIIFilterBank(sampleRate);
	}

	AkEmitterListenerPair emitterListener;

	if (in_pInput->HasListenerRelativeRouting())
	{
		// Set Azimuth and Elevation
		in_pInput->Get3DPosition(0, emitterListener);
		m_pContext->GlobalContext()->ComputeSphericalCoordinates(emitterListener, m_gameAzimuth, m_gameElevation);

		userData->lastSourceDirection = (m_gameAzimuth > 0) ? ESourceDirection::Right : ESourceDirection::Left;
		userData->inputType = EVoiceType::IsSpatialized;
	}
	else
	{
		userData->inputType = EVoiceType::NotSpatialized;
	}

	in_pInput->SetUserData(userData);
	m_activeUserData.push_back(userData);
}

//////////////////////////////////////////////////////////////////////////
void CrySpatialFX::OnInputDisconnected(AK::IAkMixerInputContext* in_pInput)
{
	SAkUserData* const userData = static_cast<SAkUserData*>(in_pInput->GetUserData());
	AK_PLUGIN_FREE(m_pAllocator, userData->pVoiceDelayBuffer->GetChannel(0));
	m_activeUserData.erase(
		std::remove_if(m_activeUserData.begin(), m_activeUserData.end(), [userData](SAkUserData* const element) { return element == userData; }),
		m_activeUserData.end());

	delete userData;
}

//////////////////////////////////////////////////////////////////////////
void CrySpatialFX::ConsumeInput(
	AK::IAkMixerInputContext* in_pInputContext,
	AkRamp in_baseVolume,
	AkRamp in_emitListVolume,
	AkAudioBuffer* io_pInputBuffer,
	AkAudioBuffer* io_pMixBuffer)
{
	SAkUserData const* const userData = static_cast<SAkUserData const*>(in_pInputContext->GetUserData());

	switch (userData->inputType)
	{
	case EVoiceType::IsSpatialized:
		{
			switch (m_numOutputChannels)
			{
			case 2:
				{
					HRTF_MonoToStereo(
						in_pInputContext,
						in_baseVolume,
						in_emitListVolume,
						io_pInputBuffer,
						io_pMixBuffer);

					break;
				}
			case 6:
				{
					HRTF_MonoTo5_1(
						in_pInputContext,
						in_baseVolume,
						in_emitListVolume,
						io_pInputBuffer,
						io_pMixBuffer);

					break;
				}
			case 8:
				{
					HRTF_MonoTo7_1(
						in_pInputContext,
						in_baseVolume,
						in_emitListVolume,
						io_pInputBuffer,
						io_pMixBuffer);

					break;
				}
			default:
				{
					break;
				}
			}

			break;
		}
	case EVoiceType::NotSpatialized:
		{
			NonHRTF_StereoToStereo(
				in_pInputContext,
				in_baseVolume,
				in_emitListVolume,
				io_pInputBuffer,
				io_pMixBuffer);

			break;
		}
	default:
		{
			break;
		}
	}
}

//////////////////////////////////////////////////////////////////////////
void CrySpatialFX::OnMixDone(AkAudioBuffer* io_pMixBuffer)
{
}

//////////////////////////////////////////////////////////////////////////
void CrySpatialFX::OnEffectsProcessed(AkAudioBuffer* io_pMixBuffer)
{
	// Execute DSP after insert effects have been processed here
}

//////////////////////////////////////////////////////////////////////////
void CrySpatialFX::OnFrameEnd(AkAudioBuffer* io_pMixBuffer, AK::IAkMetering* in_pMetering)
{
	// Execute DSP after metering has been processed here
}

//////////////////////////////////////////////////////////////////////////
void CrySpatialFX::FillDelayBuffer(AkSampleType* pInChannelDelayBuffer, AkSampleType* const pOutChannelDelayBuffer, int const delayCurrent)
{
	if (delayCurrent > 0)
	{
		int const offset = m_bufferSize - delayCurrent;
		pInChannelDelayBuffer += offset;

		memcpy(pOutChannelDelayBuffer, pInChannelDelayBuffer, sizeof(AkSampleType) * delayCurrent);
	}
}

//////////////////////////////////////////////////////////////////////////
void CrySpatialFX::HRTF_MonoToStereo(
	AK::IAkMixerInputContext* const pInputContext,
	AkRamp const baseVolume,
	AkRamp const busVolume,
	AkAudioBuffer* pInputBuffer,
	AkAudioBuffer* pMixBuffer)
{
	pInputBuffer->ZeroPadToMaxFrames();
	AkRamp const loudnessMatch(DecibelToVolume(-3.0f), DecibelToVolume(-3.0f));

	SAkUserData* const userData = static_cast<SAkUserData*>(pInputContext->GetUserData());

	int const inputValidFrames = pInputBuffer->uValidFrames;
	int const cycleProxy = userData->voiceCycleDominantEQ;

	// Set Azimuth and Elevation
	AkEmitterListenerPair emitterListener;
	pInputContext->Get3DPosition(0, emitterListener);
	m_pContext->GlobalContext()->ComputeSphericalCoordinates(emitterListener, m_gameAzimuth, m_gameElevation);

	// Clamp Azimuth and Elevation in case data from game is faulty
	if (m_gameAzimuth > g_pi)
	{
		m_gameAzimuth = g_pi;
	}

	if (m_gameAzimuth < (-g_pi))
	{
		m_gameAzimuth = (-g_pi);
	}

	if (m_gameElevation > 1.0f)
	{
		m_gameElevation = 1.0f;
	}

	if (m_gameElevation < -1.0f)
	{
		m_gameElevation = -1.0f;
	}

	ComputeQuadrant();

	ESourceDirection const sourceDirection = (m_gameAzimuth > 0) ? ESourceDirection::Right : ESourceDirection::Left;

	float compensationalFilterVolume = 1.0f;
	EQInputBuffer(pInputContext, pInputBuffer, compensationalFilterVolume);

	userData->voiceCycleDominantEQ = (userData->voiceCycleDominantEQ == 0) ? 2 : (userData->voiceCycleDominantEQ == 1) ? 2 : 1;

	// Compute Delay, Volume difference & EQ Gain for delayed Channel
	int delayCurrent = 0;
	AkReal32 volumeFactor = 0.0f;
	float sideEQGain = 0.0f;
	ComputeDelayChannelData(volumeFactor, delayCurrent, sideEQGain);

	// Fade strength to be relative to correlation through delay
	m_bufferFadeStrength = (abs(userData->voiceDelayPrev - delayCurrent) / m_maxDelay) * 1.0f /*This is intentional for tweaking*/;

	{
		// Construct internal Buffers
		AkSampleType* pInChannelDirect = pInputBuffer->GetChannel(0);
		AkSampleType* pDelChannelDirect = userData->pVoiceDelayBuffer->GetChannel(0);
		AkSampleType* pOutChannelDirect = m_pIntermediateBufferDirect;

		FillIntermediateBufferDirect(
			pInChannelDirect,
			pDelChannelDirect,
			pOutChannelDirect,
			userData,
			m_bufferSize,
			sourceDirection);

		AkSampleType* pInChannelConcealed = pInputBuffer->GetChannel(0);
		AkSampleType* pDelChannelConcealed = userData->pVoiceDelayBuffer->GetChannel(0);
		AkSampleType* pOutChannelConcealed = m_pIntermediateBufferConcealed;

		FillIntermediateBufferConcealed(
			pInChannelConcealed,
			pDelChannelConcealed,
			pOutChannelConcealed,
			userData,
			delayCurrent,
			m_bufferSize,
			sourceDirection);

		AkSampleType* pInChannelDelayBuffer = pInputBuffer->GetChannel(0);
		AkSampleType* pOutChannelDelayBuffer = userData->pVoiceDelayBuffer->GetChannel(0);
		FillDelayBuffer(pInChannelDelayBuffer, pOutChannelDelayBuffer, delayCurrent);

		userData->pVoiceDelayBuffer->uValidFrames = (delayCurrent == 0) ? 0 : delayCurrent;
	}

	EQConcealedChannel(pInputContext, cycleProxy, inputValidFrames, sourceDirection);
	EQDirectChannel(pInputContext, cycleProxy, inputValidFrames, sourceDirection);

	// Calculate Volume for Left and Right Channel
	AkRamp localbaseVolume = baseVolume * busVolume;
	AkRamp const compensationalFilterVolumeRamp(compensationalFilterVolume, compensationalFilterVolume);

	localbaseVolume *= loudnessMatch;
	// localbaseVolume *= compensationalFilterVolumeRamp;

	// Mix into OutBuffer
	int channelNoDelay;
	int channelDelay;

	switch (sourceDirection)
	{
	case ESourceDirection::Right:
		{
			channelNoDelay = 1;
			channelDelay = 0;
			break;
		}
	case ESourceDirection::Left:
		{
			channelNoDelay = 0;
			channelDelay = 1;
			break;
		}
	default:
		{
			break;
		}
	}

	m_pContext->GlobalContext()->MixChannel(
		m_pIntermediateBufferDirect,
		pMixBuffer->GetChannel(channelNoDelay),
		localbaseVolume.fPrev,
		localbaseVolume.fNext,
		inputValidFrames);

	m_pContext->GlobalContext()->MixChannel(
		m_pIntermediateBufferConcealed,
		pMixBuffer->GetChannel(channelDelay),
		localbaseVolume.fPrev,
		localbaseVolume.fNext,
		inputValidFrames);

	pMixBuffer->uValidFrames = inputValidFrames;

	// Write Data
	userData->voiceDelayPrev = delayCurrent;
	userData->lastSourceDirection = sourceDirection;
}

//////////////////////////////////////////////////////////////////////////
void CrySpatialFX::HRTF_MonoTo5_1(
	AK::IAkMixerInputContext* const pInputContext,
	AkRamp const baseVolume,
	AkRamp const busVolume,
	AkAudioBuffer* pInputBuffer,
	AkAudioBuffer* pMixBuffer)
{
	SAkUserData* const userData = static_cast<SAkUserData*>(pInputContext->GetUserData());
	pInputBuffer->ZeroPadToMaxFrames();

	AkRamp const localbaseVolume = baseVolume * busVolume;
	AkUInt32 const allocSize = AK::SpeakerVolumes::Matrix::GetRequiredSize(pInputBuffer->NumChannels(), pMixBuffer->NumChannels());
	AK::SpeakerVolumes::MatrixPtr const volumesPrev = (AK::SpeakerVolumes::MatrixPtr)AkAlloca(allocSize);
	AK::SpeakerVolumes::MatrixPtr const volumesNext = (AK::SpeakerVolumes::MatrixPtr)AkAlloca(allocSize);
	pInputContext->GetSpatializedVolumes(volumesPrev, volumesNext);

	// Set Azimuth and Elevation
	AkEmitterListenerPair emitterListener;
	pInputContext->Get3DPosition(0, emitterListener);
	m_pContext->GlobalContext()->ComputeSphericalCoordinates(emitterListener, m_gameAzimuth, m_gameElevation);

	// Clamp azimuth and elevation in case data from game is faulty
	if (m_gameAzimuth > g_pi)
	{
		m_gameAzimuth = g_pi;
	}

	if (m_gameAzimuth < (-g_pi))
	{
		m_gameAzimuth = (-g_pi);
	}

	if (m_gameElevation > 1.0f)
	{
		m_gameElevation = 1.0f;
	}

	if (m_gameElevation < -1.0f)
	{
		m_gameElevation = -1.0f;
	}

	ComputeQuadrant();

	float filterVolume = 0.0f;
	int filterFrequency = 800;

	switch (m_quadrant)
	{
	case 2:   // fall through
	case 1:
		{
			filterVolume = (m_quadrantFine * (-5.0f));
			filterFrequency -= static_cast<int>(m_quadrantFine * 200);
			break;
		}
	default:
		{
			break;
		}
	}

	// Filter Sources behind Listener
	{
		BiquadIIFilter* pFilterDominant = &userData->pFilterBankB->filterBand11; // FilterDominant
		BiquadIIFilter* pFilterResidual = &userData->pFilterBankA->filterBand11; // FilterResidual

		switch (userData->voiceCycleDominantDelay)
		{
		case 0:
			{
				pFilterDominant = &userData->pFilterBankA->filterBand11;
				break;
			}
		case 1:
			{
				pFilterDominant = &userData->pFilterBankA->filterBand11;
				pFilterResidual = &userData->pFilterBankB->filterBand11;
				break;
			}
		case 2:
			{
				pFilterDominant = &userData->pFilterBankB->filterBand11;
				pFilterResidual = &userData->pFilterBankA->filterBand11;
				break;
			}
		default:
			{
				break;
			}
		}

		pFilterDominant->ComputeCoefficients(filterFrequency, g_defaultFilterQuality, filterVolume);
		AkSampleType* pChannelInPlace = pInputBuffer->GetChannel(0);

		int const inputValidFrames = pInputBuffer->uValidFrames;

		switch (userData->voiceCycleDominantDelay)
		{
		case 0:
			{
				for (int i = 1; i <= g_smallFadeLengthInteger; ++i)
				{
					AkSampleType const sampleFiltered = pFilterDominant->ProcessSample(*pChannelInPlace);

					*pChannelInPlace = sampleFiltered * ComputeFade((i / g_smallFadeLength), g_linearFadeStrength);
					++pChannelInPlace;
				}

				for (int i = g_smallFadeLengthInteger; i < inputValidFrames; ++i)
				{
					*pChannelInPlace = pFilterDominant->ProcessSample(*pChannelInPlace);
					++pChannelInPlace;
				}

				userData->voiceCycleDominantDelay = 2;
				break;
			}
		case 1:   // Fall through
		case 2:
			{
				for (int i = 1; i <= g_smallFadeLengthInteger; ++i) // Blend
				{
					AkReal32 const filteredSampleDominant = pFilterDominant->ProcessSample(*pChannelInPlace);
					AkReal32 const filteredSampleResidual = pFilterResidual->ProcessSample(*pChannelInPlace);

					float const fadeValue = (i / g_smallFadeLength);

					*pChannelInPlace =
						((filteredSampleResidual * ComputeFade((1.0f - fadeValue), g_linearFadeStrength)) +
						 (filteredSampleDominant * ComputeFade(fadeValue, g_linearFadeStrength))); // Linear Fade because correlation is 1

					++pChannelInPlace;
				}

				for (int i = g_smallFadeLengthInteger; i < inputValidFrames; ++i) // Write Rest with new Filters
				{
					*pChannelInPlace = pFilterDominant->ProcessSample(*pChannelInPlace);
					++pChannelInPlace;
				}

				userData->voiceCycleDominantDelay = (userData->voiceCycleDominantDelay == 2) ? 1 : 2;
				break;
			}
		default:
			{
				break;
			}
		}
	}

	m_pContext->GlobalContext()->Mix1inNChannels(
		pInputBuffer->GetChannel(0),
		pMixBuffer,
		localbaseVolume.fPrev,
		localbaseVolume.fNext,
		volumesPrev,
		volumesNext
		);

	pMixBuffer->uValidFrames = pInputBuffer->uValidFrames;
}

//////////////////////////////////////////////////////////////////////////
void CrySpatialFX::HRTF_MonoTo7_1(
	AK::IAkMixerInputContext* const pInputContext,
	AkRamp const baseVolume,
	AkRamp const busVolume,
	AkAudioBuffer* pInputBuffer,
	AkAudioBuffer* pMixBuffer)
{
	SAkUserData* const userData = static_cast<SAkUserData*>(pInputContext->GetUserData());
	pInputBuffer->ZeroPadToMaxFrames();

	AkRamp const localbaseVolume = baseVolume * busVolume;
	AkUInt32 const allocSize = AK::SpeakerVolumes::Matrix::GetRequiredSize(pInputBuffer->NumChannels(), pMixBuffer->NumChannels());
	AK::SpeakerVolumes::MatrixPtr const volumesPrev = (AK::SpeakerVolumes::MatrixPtr)AkAlloca(allocSize);
	AK::SpeakerVolumes::MatrixPtr const volumesNext = (AK::SpeakerVolumes::MatrixPtr)AkAlloca(allocSize);
	pInputContext->GetSpatializedVolumes(volumesPrev, volumesNext);

	// Set Azimuth and Elevation
	AkEmitterListenerPair emitterListener;
	pInputContext->Get3DPosition(0, emitterListener);
	m_pContext->GlobalContext()->ComputeSphericalCoordinates(emitterListener, m_gameAzimuth, m_gameElevation);

	// Clamp azimuth and elevation in case data from game is faulty
	if (m_gameAzimuth > g_pi)
	{
		m_gameAzimuth = g_pi;
	}

	if (m_gameAzimuth < (-g_pi))
	{
		m_gameAzimuth = (-g_pi);
	}

	if (m_gameElevation > 1.0f)
	{
		m_gameElevation = 1.0f;
	}

	if (m_gameElevation < -1.0f)
	{
		m_gameElevation = -1.0f;
	}

	ComputeQuadrant();

	// filter sources behind Listener
	float filterVolume = 0.0f;
	int filterFrequency = 800;

	switch (m_quadrant)
	{
	case 2:   // fall through
	case 1:
		{
			filterVolume = (m_quadrantFine * (-5.0f));
			filterFrequency -= static_cast<int>(m_quadrantFine * 200);
			break;
		}
	default:
		{
			break;
		}
	}

	{
		BiquadIIFilter* pFilterDominant = &userData->pFilterBankB->filterBand11; // FilterDominant
		BiquadIIFilter* pFilterResidual = &userData->pFilterBankA->filterBand11; // FilterResidual

		switch (userData->voiceCycleDominantDelay)
		{
		case 0:
			{
				pFilterDominant = &userData->pFilterBankA->filterBand11;
				break;
			}
		case 1:
			{
				pFilterDominant = &userData->pFilterBankA->filterBand11;
				pFilterResidual = &userData->pFilterBankB->filterBand11;
				break;
			}
		case 2:
			{
				pFilterDominant = &userData->pFilterBankB->filterBand11;
				pFilterResidual = &userData->pFilterBankA->filterBand11;
				break;
			}
		default:
			{
				break;
			}

		}

		pFilterDominant->ComputeCoefficients(filterFrequency, g_defaultFilterQuality, filterVolume);
		AkSampleType* pChannelInPlace = pInputBuffer->GetChannel(0);

		int inputValidFrames = pInputBuffer->uValidFrames;

		switch (userData->voiceCycleDominantDelay)
		{
		case 0:
			{
				for (int i = 1; i <= g_smallFadeLengthInteger; ++i)
				{
					AkSampleType const filteredSample = pFilterDominant->ProcessSample(*pChannelInPlace);

					*pChannelInPlace = filteredSample * ComputeFade((i / g_smallFadeLength), g_linearFadeStrength);
					++pChannelInPlace;
				}

				for (int i = g_smallFadeLengthInteger; i < inputValidFrames; ++i)
				{
					*pChannelInPlace = pFilterDominant->ProcessSample(*pChannelInPlace);
					++pChannelInPlace;
				}

				userData->voiceCycleDominantDelay = 2;
				break;
			}
		case 1:   // Fall through
		case 2:
			{
				for (int i = 1; i <= g_smallFadeLengthInteger; ++i)     // Blend
				{
					AkReal32 const filteredSampleDominant = pFilterDominant->ProcessSample(*pChannelInPlace);
					AkReal32 const filteredSampleResidual = pFilterResidual->ProcessSample(*pChannelInPlace);

					float fadeValue = (i / g_smallFadeLength);

					*pChannelInPlace = ((filteredSampleResidual * ComputeFade((1.0f - fadeValue), g_linearFadeStrength)) +
					                    (filteredSampleDominant * ComputeFade(fadeValue, g_linearFadeStrength))); // Linear Fade because correlation is 1

					++pChannelInPlace;
				}

				for (int i = g_smallFadeLengthInteger; i < inputValidFrames; ++i)     // Write Rest with new Filters
				{
					*pChannelInPlace = pFilterDominant->ProcessSample(*pChannelInPlace);
					++pChannelInPlace;
				}

				userData->voiceCycleDominantDelay = (userData->voiceCycleDominantDelay == 2) ? 1 : 2;
				break;
			}
		default:
			{
				break;
			}
		}
	}

	m_pContext->GlobalContext()->Mix1inNChannels(
		pInputBuffer->GetChannel(0),
		pMixBuffer,
		localbaseVolume.fPrev,
		localbaseVolume.fNext,
		volumesPrev,
		volumesNext
		);

	pMixBuffer->uValidFrames = pInputBuffer->uValidFrames;
}

//////////////////////////////////////////////////////////////////////////
void CrySpatialFX::NonHRTF_StereoToStereo(
	AK::IAkMixerInputContext* pInputContext,
	AkRamp const baseVolume,
	AkRamp const busVolume,
	AkAudioBuffer* pInputBuffer,
	AkAudioBuffer* pMixBuffer)
{
	AkRamp const localbaseVolume = baseVolume * busVolume;

	m_pContext->GlobalContext()->MixChannel(
		pInputBuffer->GetChannel(0),
		pMixBuffer->GetChannel(0),
		localbaseVolume.fPrev,
		localbaseVolume.fNext,
		pInputBuffer->uValidFrames);

	m_pContext->GlobalContext()->MixChannel(
		pInputBuffer->GetChannel(1),
		pMixBuffer->GetChannel(1),
		localbaseVolume.fPrev,
		localbaseVolume.fNext,
		pInputBuffer->uValidFrames);

	pMixBuffer->uValidFrames = pInputBuffer->uValidFrames;
}

//////////////////////////////////////////////////////////////////////////
void CrySpatialFX::ComputeDelayChannelData(
	AkReal32& out_volumeNormalized,
	int& out_delay,
	float& out_eqGain)
{
	auto const elevationFactor = static_cast<float>(fabs(m_gameElevation) / g_piHalf);
	float const elevationFactorInversedClamp = (elevationFactor > 0.85f) ? 0 : 1 - (elevationFactor / 0.85f);

	if ((m_quadrant == 0) || (m_quadrant == 3)) // azimuth 0 to (Pi/2)
	{
		out_delay =
			static_cast<int>(
				(g_maxDelay * ComputeFade(m_quadrantFine, 2.4f, EFadeType::FadeinLogarithmic))
				* ComputeFade(elevationFactorInversedClamp, 3.6f, EFadeType::FadeinLogarithmic));

		out_volumeNormalized = static_cast<AkReal32>(1.0f - (m_quadrantFine * elevationFactorInversedClamp));
	}
	else    // azimuth (Pi/2) to (Pi)
	{
		out_delay =
			static_cast<int>(
				(g_maxDelay * ComputeFade(m_quadrantFine, 2.4f, EFadeType::FadeoutLogarithmic))
				* ComputeFade(elevationFactorInversedClamp, 3.6f, EFadeType::FadeinLogarithmic));

		out_volumeNormalized = static_cast<AkReal32>(1.0f - ((1.0f - m_quadrantFine) * elevationFactorInversedClamp));
	}
}

//////////////////////////////////////////////////////////////////////////
void CrySpatialFX::FillIntermediateBufferDirect(
	AkSampleType* pInChannel,
	AkSampleType* pDelChannel,
	AkSampleType* pOutChannel,
	SAkUserData* const userData,
	int const maxFrames,
	ESourceDirection const sourceDirection)
{
	if (sourceDirection == userData->lastSourceDirection) // L/R didn't switch. Write InBuffer -> IntermediateDirectBuffer
	{
		memcpy(pOutChannel, pInChannel, sizeof(AkSampleType) * maxFrames);
	}
	else
	{
		AkSampleType* pInProxy = pInChannel;
		int const voiceDelayPrevious = userData->voiceDelayPrev;

		float const fadeFactor = 1.0f / g_smallFadeLength;

		for (int i = 0; i < voiceDelayPrevious; ++i)
		{
			*pOutChannel =
				((*pDelChannel * ComputeFade((i * fadeFactor), m_bufferFadeStrength, EFadeType::FadeoutLogarithmic))
				 + (*pInChannel * ComputeFade((i * fadeFactor), m_bufferFadeStrength, EFadeType::FadeinLogarithmic)));

			++pOutChannel;
			++pDelChannel;
			++pInChannel;
		}

		for (int i = voiceDelayPrevious; i < g_smallFadeLengthInteger; ++i)
		{
			*pOutChannel =
				((*pInProxy * ComputeFade((i * fadeFactor), m_bufferFadeStrength, EFadeType::FadeoutLogarithmic))
				 + (*pInChannel * ComputeFade((i * fadeFactor), m_bufferFadeStrength, EFadeType::FadeinLogarithmic)));

			++pOutChannel;
			++pInProxy;
			++pInChannel;
		}

		int const cycles = (maxFrames - g_smallFadeLengthInteger);

		memcpy(pOutChannel, pInChannel, sizeof(AkSampleType) * cycles);
	}
}

//////////////////////////////////////////////////////////////////////////
void CrySpatialFX::FillIntermediateBufferConcealed(
	AkSampleType* pInChannel,
	AkSampleType* pDelChannel,
	AkSampleType* pOutChannel,
	SAkUserData* userData,
	int const delayCurrent,
	int const maxFrames,
	ESourceDirection const sourceDirection)
{
	int outSampleOffset = 0;                                                                                 // needed to compensate delay changes since the last Engine tick
	int const delayPrev = (sourceDirection == userData->lastSourceDirection) ? userData->voiceDelayPrev : 0; // If we switch L/R then prev delay is 0
	AkSampleType* pInProxy = pInChannel;

	for (int i = 0; i < delayPrev; ++i)   // Write Delay from last cycle
	{
		*pOutChannel = *pDelChannel;
		++pOutChannel;
		++pDelChannel;
	}

	// Cross fade over 100 samples
	if (delayPrev != delayCurrent)
	{
		if (delayPrev < delayCurrent)
		{
			AkSampleType* pInProxy = pInChannel;
			int const sampleGap = delayCurrent - delayPrev;

			for (int i = 0; i < sampleGap; ++i)
			{
				*pOutChannel = *pInProxy;
				++pOutChannel;
				++pInProxy;
			}

			float const fadeFactor = 1.0f / g_smallFadeLength;

			for (int i = 1; i <= g_smallFadeLengthInteger; ++i)
			{
				float const fadeValue = i * fadeFactor;

				*pOutChannel =
					((*pInProxy * ComputeFade(fadeValue, m_bufferFadeStrength, EFadeType::FadeoutLogarithmic)) +
					 (*pInChannel * ComputeFade(fadeValue, m_bufferFadeStrength, EFadeType::FadeinLogarithmic)));

				++pOutChannel;
				++pInProxy;
				++pInChannel;
			}

			outSampleOffset = g_smallFadeLengthInteger + sampleGap;
		}
		else    // delay gets smaller
		{
			int const samplesToDrop = delayPrev - delayCurrent;
			AkSampleType* pInProxy = pInChannel;
			pInChannel += samplesToDrop;

			float const fadeFactor = 1.0f / g_smallFadeLength;

			for (int i = 1; i <= g_smallFadeLength; ++i)
			{
				float const fadeValue = i * fadeFactor;

				*pOutChannel =
					(*pInProxy * ComputeFade(fadeValue, m_bufferFadeStrength, EFadeType::FadeoutLogarithmic)) +
					(*pInChannel * ComputeFade(fadeValue, m_bufferFadeStrength, EFadeType::FadeinLogarithmic));

				++pOutChannel;
				++pInProxy;
				++pInChannel;
			}

			outSampleOffset = g_smallFadeLengthInteger;
		}
	}

	// Write from current inputBuffer
	int const cycles = (maxFrames - (delayPrev + outSampleOffset));

	memcpy(pOutChannel, pInChannel, sizeof(AkSampleType) * cycles);
}

//////////////////////////////////////////////////////////////////////////
void CrySpatialFX::EQInputBuffer(
	AK::IAkMixerInputContext* pInputContext,
	AkAudioBuffer* pInputBuffer,
	float& out_compensationalVolume)
{
	out_compensationalVolume = 1.0f;
	SAkUserData* const userData = static_cast<SAkUserData*>(pInputContext->GetUserData());
	int const inputValidFrames = pInputBuffer->uValidFrames;

	auto const elevationFactor = static_cast<float>(fabs(m_gameElevation) / g_piHalf);
	float const elevationFactorInversedClamp = (elevationFactor > 0.85f) ? 0.0f : 1.0f - (elevationFactor / 0.85f);

	BiquadIIFilter
	* filterDominant0,
	* filterDominant1,
	* filterDominant2,
	* filterDominant3,
	* filterDominant4,
	* filterDominant5,
	* filterDominant6,
	* filterDominant7,
	* filterDominant8,

	* filterResidual0,
	* filterResidual1,
	* filterResidual2,
	* filterResidual3,
	* filterResidual4,
	* filterResidual5,
	* filterResidual6,
	* filterResidual7,
	* filterResidual8;

	// Dominant || Residual
	switch (userData->voiceCycleDominantEQ)
	{
	case 0:
		{
			filterDominant0 = &userData->pFilterBankA->filterBand00;
			filterDominant1 = &userData->pFilterBankA->filterBand01;
			filterDominant2 = &userData->pFilterBankA->filterBand02;
			filterDominant3 = &userData->pFilterBankA->filterBand03;
			filterDominant4 = &userData->pFilterBankA->filterBand04;
			filterDominant5 = &userData->pFilterBankA->filterBand05;
			filterDominant6 = &userData->pFilterBankA->filterBand06;
			filterDominant7 = &userData->pFilterBankA->filterBand07;
			filterDominant8 = &userData->pFilterBankA->filterBand08;

			break;
		}
	case 1:
		{
			filterDominant0 = &userData->pFilterBankA->filterBand00;
			filterDominant1 = &userData->pFilterBankA->filterBand01;
			filterDominant2 = &userData->pFilterBankA->filterBand02;
			filterDominant3 = &userData->pFilterBankA->filterBand03;
			filterDominant4 = &userData->pFilterBankA->filterBand04;
			filterDominant5 = &userData->pFilterBankA->filterBand05;
			filterDominant6 = &userData->pFilterBankA->filterBand06;
			filterDominant7 = &userData->pFilterBankA->filterBand07;
			filterDominant8 = &userData->pFilterBankA->filterBand08;

			filterResidual0 = &userData->pFilterBankB->filterBand00;
			filterResidual1 = &userData->pFilterBankB->filterBand01;
			filterResidual2 = &userData->pFilterBankB->filterBand02;
			filterResidual3 = &userData->pFilterBankB->filterBand03;
			filterResidual4 = &userData->pFilterBankB->filterBand04;
			filterResidual5 = &userData->pFilterBankB->filterBand05;
			filterResidual6 = &userData->pFilterBankB->filterBand06;
			filterResidual7 = &userData->pFilterBankB->filterBand07;
			filterResidual8 = &userData->pFilterBankB->filterBand08;

			break;
		}
	case 2:
		{
			filterResidual0 = &userData->pFilterBankA->filterBand00;
			filterResidual1 = &userData->pFilterBankA->filterBand01;
			filterResidual2 = &userData->pFilterBankA->filterBand02;
			filterResidual3 = &userData->pFilterBankA->filterBand03;
			filterResidual4 = &userData->pFilterBankA->filterBand04;
			filterResidual5 = &userData->pFilterBankA->filterBand05;
			filterResidual6 = &userData->pFilterBankA->filterBand06;
			filterResidual7 = &userData->pFilterBankA->filterBand07;
			filterResidual8 = &userData->pFilterBankA->filterBand08;

			filterDominant0 = &userData->pFilterBankB->filterBand00;
			filterDominant1 = &userData->pFilterBankB->filterBand01;
			filterDominant2 = &userData->pFilterBankB->filterBand02;
			filterDominant3 = &userData->pFilterBankB->filterBand03;
			filterDominant4 = &userData->pFilterBankB->filterBand04;
			filterDominant5 = &userData->pFilterBankB->filterBand05;
			filterDominant6 = &userData->pFilterBankB->filterBand06;
			filterDominant7 = &userData->pFilterBankB->filterBand07;
			filterDominant8 = &userData->pFilterBankB->filterBand08;

			break;
		}
	default:
		{
			break;
		}
	}

	// BAND00 500Hz
	int band00Frequency;
	float band00Quality = 3.0f;
	float band00Gain;

	switch (m_quadrant)
	{
	case 0:   // Fall through
	case 3:
		{
			band00Frequency = 500 + static_cast<int>(100.0f * ComputeFade(m_quadrantFine, 1.5f, EFadeType::FadeinHyperbole));
			band00Gain = (2.0f - (2.0f * m_quadrantFine)) * elevationFactorInversedClamp;
			out_compensationalVolume *= DecibelToVolume(1.0f - m_quadrantFine);
			break;
		}
	case 1:   // Fall through
	case 2:
		{
			band00Frequency = 600 + static_cast<int>(650.0f * ComputeFade(m_quadrantFine, 0.6f, EFadeType::FadeinLogarithmic));
			band00Quality = 3.0f - (0.8f * ComputeFade(m_quadrantFine, 1.7f, EFadeType::FadeinLogarithmic));
			band00Gain = ((6.5f * ComputeFade(m_quadrantFine, 0.6f, EFadeType::FadeinLogarithmic))) * elevationFactorInversedClamp;
			out_compensationalVolume *= DecibelToVolume(2.0f * m_quadrantFine);
			break;
		}
	default:
		{
			break;
		}
	}

	filterDominant0->ComputeCoefficients(band00Frequency, band00Quality, band00Gain);

	// BAND01 1000Hz
	int band01Frequency;
	float band01Quality;
	float band01Gain;

	switch (m_quadrant)
	{
	case 0:   // Fall through
	case 3:
		{
			band01Frequency = (1000 + static_cast<int>(1000.0f * ComputeFade(m_quadrantFine, 0.9f, EFadeType::FadeinHyperbole)));
			band01Quality = 3.5f + (1.5f * ComputeFade(m_quadrantFine, 1.0f, EFadeType::FadeinHyperbole));
			band01Gain = (-6.0f + (-2.0f * m_quadrantFine)) * elevationFactorInversedClamp;
			break;
		}
	case 1:   // Fall through
	case 2:
		{
			band01Frequency = (2000 + static_cast<int>(1500.0f * ComputeFade(m_quadrantFine, 1.4f, EFadeType::FadeinLogarithmic)));
			band01Quality = 5.0f + (3.2f * m_quadrantFine);
			band01Gain = (-8.0f + (-4.0f * ComputeFade(m_quadrantFine, 1.5f, EFadeType::FadeinLogarithmic))) * elevationFactorInversedClamp;
			break;
		}
	default:
		{
			break;
		}
	}

	filterDominant1->ComputeCoefficients(band01Frequency, band01Quality, band01Gain);

	// BAND02 3000 Hz
	int band02Frequency;
	float band02Quality;
	float band02Gain;

	switch (m_quadrant)
	{
	case 0:   // Fall through
	case 3:
		{
			band02Frequency = 2750 + static_cast<int>(3750.0f * m_quadrantFine);
			band02Quality = 1.3f + (-1.1f * m_quadrantFine);
			band02Gain = 5.0f - (3.0f * m_quadrantFine);
			break;
		}
	case 1:   // Fall through
	case 2:
		{
			band02Frequency = 6500 + static_cast<int>(4300.0f * m_quadrantFine);
			band02Quality = 0.2f + (1.0f * ComputeFade(m_quadrantFine, 1.5, EFadeType::FadeinHyperbole));
			band02Gain = 2.0f - (2.0f * m_quadrantFine);
			break;
		}
	default:
		{
			break;
		}
	}

	filterDominant2->ComputeCoefficients(band02Frequency, band02Quality, band02Gain);

	// BAND03
	int band03Frequency;
	float band03Quality;
	float band03Gain;

	switch (m_quadrant)
	{
	case 0:   // Fall through
	case 3:
		{
			band03Frequency = 8300 - static_cast<int>(2300.0f * m_quadrantFine);
			band03Quality = 10.0f - (3.0f * m_quadrantFine);
			band03Gain = (-10.0f + (1.5f * m_quadrantFine)) * elevationFactorInversedClamp;
			break;
		}
	case 1:   // Fall through
	case 2:
		{
			band03Frequency = 6000 - static_cast<int>(3000.0f * m_quadrantFine);
			band03Quality = 7.0f - (4.0f * ComputeFade(m_quadrantFine, 1.1f, EFadeType::FadeinLogarithmic));
			band03Gain = (-8.5f + (-3.5f * ComputeFade(m_quadrantFine, 1.5f, EFadeType::FadeinHyperbole))) * elevationFactorInversedClamp;
			break;
		}
	default:
		{
			break;
		}
	}

	filterDominant3->ComputeCoefficients(band03Frequency, band03Quality, band03Gain);

	// BAND04
	int band04Frequency;
	float band04Gain;
	float band04Quality;

	switch (m_quadrant)
	{
	case 0:   // Fall through
	case 3:
		{
			band04Frequency = 10650 - static_cast<int>(1650.0f * ComputeFade(m_quadrantFine, 2.3f, EFadeType::FadeinHyperbole));
			band04Quality = (7.7f + (2.3f * m_quadrantFine));
			band04Gain = (-15.0f + (5.0f * ComputeFade(m_quadrantFine, 1.4f, EFadeType::FadeinHyperbole))) * elevationFactorInversedClamp;
			break;
		}
	case 1:   // Fall through
	case 2:
		{
			band04Frequency = 9000 - static_cast<int>(1500.0f * ComputeFade(m_quadrantFine, 2.2f, EFadeType::FadeinLogarithmic));
			band04Quality = (10.0f - (2.1f * ComputeFade(m_quadrantFine, 1.6f, EFadeType::FadeinLogarithmic)));
			band04Gain = (-10.0f + (5.0f * m_quadrantFine)) * elevationFactorInversedClamp;
			break;
		}
	default:
		{
			break;
		}
	}

	filterDominant4->ComputeCoefficients(band04Frequency, band04Quality, band04Gain);

	// BAND05
	int band05Frequency;
	float band05Gain;
	float band05Quality;

	switch (m_quadrant)
	{
	case 0:   // fall through
	case 3:
		{
			band05Frequency = 12850 - static_cast<int>(3850.0f * m_quadrantFine);
			band05Quality = 10.0f - (5.0f * m_quadrantFine);
			band05Gain = (-7.5f + (9.5f * ComputeFade(m_quadrantFine, 1.0f, EFadeType::FadeoutLogarithmic))) * elevationFactorInversedClamp;
			break;
		}
	case 1:   // Fall through
	case 2:
		{
			band05Frequency = 9000 + static_cast<int>(3000.0f * ComputeFade(m_quadrantFine, 1.2f, EFadeType::FadeinLogarithmic));
			band05Quality = 5.0f - (4.1f * m_quadrantFine);
			band05Gain = (2.0f + (1.0f * ComputeFade(m_quadrantFine, 1.3f, EFadeType::FadeinLogarithmic))) * elevationFactorInversedClamp;
			break;
		}
	default:
		{
			break;
		}
	}

	filterDominant5->ComputeCoefficients(band05Frequency, band05Quality, band05Gain);

	int band06Frequency;
	float band06Quality;
	float band06Gain;

	int band07Frequency;
	float band07Quality;
	float band07Gain;

	int band08Frequency;
	float band08Quality;
	float band08Gain;

	if (m_gameElevation > 0)
	{
		auto const elevation = static_cast<float>(fabs(m_gameElevation));

		band06Frequency = 6000 - static_cast<int>(3700.0f * ComputeFade(elevation, 0.9f, EFadeType::FadeinLogarithmic));
		band07Frequency = 6000 + static_cast<int>(3100.0f * ComputeFade(elevation, 0.4f, EFadeType::FadeinLogarithmic));
		band08Frequency = 4000 + static_cast<int>(8000.0f * ComputeFade(elevation, 1.0f, EFadeType::FadeinLogarithmic));

		band06Quality = 3.0f + (7.0f * ComputeFade(elevation, 0.8f, EFadeType::FadeinLogarithmic));
		band07Quality = 4.5f + (5.5f * ComputeFade(elevation, 2.7f, EFadeType::FadeinHyperbole));
		band08Quality = 4.5f + (5.5f * ComputeFade(elevation, 1.7f, EFadeType::FadeinLogarithmic));
		;

		band06Gain = -15.0f * ComputeFade(elevation, 0.8f, EFadeType::FadeinLogarithmic);
		band07Gain = 6.0f * ComputeFade(elevation, 1.7f, EFadeType::FadeinLogarithmic);
		band08Gain = -15.0f * ComputeFade(elevation, 2.5f, EFadeType::FadeinLogarithmic);
	}
	else
	{
		auto const elevation = static_cast<float>(fabs(m_gameElevation));

		band06Frequency = 6000 + static_cast<int>(4700.0f * elevation);
		band07Frequency = 6000 - static_cast<int>(900.0f * elevation);
		band08Frequency = 4000 - static_cast<int>(800.0f * elevation);

		band06Quality = 3.0f + (4.7f * elevation);
		band07Quality = 4.5f - (1.2f * elevation);
		band08Quality = 4.5f + (5.5f * elevation);

		band06Gain = -10.0f * elevation;
		band07Gain = -14.0f * ComputeFade(elevation, 1.5f, EFadeType::FadeinLogarithmic);
		band08Gain = -8.0f * ComputeFade(elevation, 1.0f, EFadeType::FadeinLogarithmic);
	}

	filterDominant6->ComputeCoefficients(band06Frequency, band06Quality, band06Gain);
	filterDominant7->ComputeCoefficients(band07Frequency, band07Quality, band07Gain);
	filterDominant8->ComputeCoefficients(band08Frequency, band08Quality, band08Gain);

	AkSampleType* pChannel = pInputBuffer->GetChannel(0);

	switch (userData->voiceCycleDominantEQ)
	{
	case 0:
		{
			float const fadeFactor = 1 / g_bigFadeLength;

			for (int i = 0; i < g_bigFadeLengthInteger; ++i)
			{
				float sampleFiltered =
					filterDominant8->ProcessSample(
						filterDominant7->ProcessSample(
							filterDominant6->ProcessSample(
								filterDominant5->ProcessSample(
									filterDominant4->ProcessSample(
										filterDominant3->ProcessSample(
											filterDominant2->ProcessSample(
												filterDominant1->ProcessSample(
													filterDominant0->ProcessSample(*pChannel)))))))));

				*pChannel = sampleFiltered * (i * fadeFactor);
				++pChannel;
			}

			int const cyclesRemaining = inputValidFrames - g_bigFadeLengthInteger;

			for (int i = 0; i < cyclesRemaining; ++i)
			{
				*pChannel =
					filterDominant8->ProcessSample(
						filterDominant7->ProcessSample(
							filterDominant6->ProcessSample(
								filterDominant5->ProcessSample(
									filterDominant4->ProcessSample(
										filterDominant3->ProcessSample(
											filterDominant2->ProcessSample(
												filterDominant1->ProcessSample(
													filterDominant0->ProcessSample(*pChannel)))))))));
				++pChannel;
			}
			break;
		}
	case 1:   // fall through
	case 2:
		{
			float const fadeFactor = 1 / g_bigFadeLength;

			for (int i = 0; i < g_bigFadeLengthInteger; ++i) // Blend
			{
				float newFilter =
					filterDominant8->ProcessSample(
						filterDominant7->ProcessSample(
							filterDominant6->ProcessSample(
								filterDominant5->ProcessSample(
									filterDominant4->ProcessSample(
										filterDominant3->ProcessSample(
											filterDominant2->ProcessSample(
												filterDominant1->ProcessSample(
													filterDominant0->ProcessSample(*pChannel)))))))));

				float oldFilter =
					filterResidual8->ProcessSample(
						filterResidual7->ProcessSample(
							filterResidual6->ProcessSample(
								filterResidual5->ProcessSample(
									filterResidual4->ProcessSample(
										filterResidual3->ProcessSample(
											filterResidual2->ProcessSample(
												filterResidual1->ProcessSample(
													filterResidual0->ProcessSample(*pChannel)))))))));

				float fadeValue = i * fadeFactor;

				*pChannel =
					(newFilter * ComputeFade(fadeValue, g_linearFadeStrength, EFadeType::FadeinLogarithmic))
					+ (oldFilter * ComputeFade(fadeValue, g_linearFadeStrength, EFadeType::FadeoutLogarithmic));

				++pChannel;
			}

			int const cyclesRemaining = inputValidFrames - g_bigFadeLengthInteger;

			for (int i = 0; i < cyclesRemaining; ++i) // Write Rest with new Filters
			{
				*pChannel =
					filterDominant8->ProcessSample(
						filterDominant7->ProcessSample(
							filterDominant6->ProcessSample(
								filterDominant5->ProcessSample(
									filterDominant4->ProcessSample(
										filterDominant3->ProcessSample(
											filterDominant2->ProcessSample(
												filterDominant1->ProcessSample(
													filterDominant0->ProcessSample(*pChannel)))))))));

				++pChannel;
			}
			break;
		}
	default:
		{
			break;
		}
	}
}

//////////////////////////////////////////////////////////////////////////
void CrySpatialFX::EQConcealedChannel(
	AK::IAkMixerInputContext* const pInputContext,
	int const cycleProxy,
	int const inputValidFrames,
	ESourceDirection sourceDirection)
{
	SAkUserData* const userData = static_cast<SAkUserData*>(pInputContext->GetUserData());

	auto const elevationFactor = static_cast<float>(fabs(m_gameElevation) / g_piHalf);
	float const elevationFactorInversedClamp = (elevationFactor > 0.85f) ? 0.0f : 1.0f - (elevationFactor / 0.85f);

	BiquadIIFilter* filterDominant11 = &userData->pFilterBankA->filterBand11;
	BiquadIIFilter* filterResidual11 = &userData->pFilterBankB->filterBand11;
	BiquadIIFilter* filterDominant10 = &userData->pFilterBankA->filterBand10;
	BiquadIIFilter* filterResidual10 = &userData->pFilterBankB->filterBand10;
	BiquadIIFilter* filterResidual09;

	switch (cycleProxy)
	{
	case 0:
		{
			filterDominant11 = &userData->pFilterBankA->filterBand11;
			filterDominant10 = &userData->pFilterBankA->filterBand10;
			break;
		}
	case 1:
		{
			filterDominant11 = &userData->pFilterBankA->filterBand11;
			filterDominant10 = &userData->pFilterBankA->filterBand10;
			filterResidual11 = &userData->pFilterBankB->filterBand11;
			filterResidual10 = &userData->pFilterBankB->filterBand10;
			filterResidual09 = &userData->pFilterBankB->filterBand09;
			break;
		}
	case 2:
		{
			filterDominant11 = &userData->pFilterBankB->filterBand11;
			filterDominant10 = &userData->pFilterBankB->filterBand10;
			filterResidual11 = &userData->pFilterBankA->filterBand11;
			filterResidual10 = &userData->pFilterBankA->filterBand10;
			filterResidual09 = &userData->pFilterBankA->filterBand09;
			break;
		}
	default:
		{
			break;
		}
	}

	int band10Frequency = 1000;
	float band10Quality;
	float band10Gain;

	int band11Frequency;
	float band11Quality = 1.0f;
	float band11Gain;

	switch (m_quadrant)
	{
	case 0:   // Fall through
	case 3:
		{
			band10Quality = 0.8f + (1.2f * m_quadrantFine);
			band10Gain = (4.0f * ComputeFade(m_quadrantFine, 3.2f, EFadeType::FadeinHyperbole)) * elevationFactorInversedClamp;

			band11Frequency = 9000 - static_cast<int>(6000.0f * m_quadrantFine);
			band11Gain = (6.0f + (-18.0f * ComputeFade(m_quadrantFine, 0.7f, EFadeType::FadeinHyperbole))) * elevationFactorInversedClamp;
			break;
		}
	case 1:   // Fall through
	case 2:
		{
			band10Frequency += static_cast<int>(1000.0f * m_quadrantFine);
			band10Quality = 2.0f - (1.7f * ComputeFade(m_quadrantFine, 1.6f, EFadeType::FadeinHyperbole));
			band10Gain = (4.0f + (-4.0f * ComputeFade(m_quadrantFine, 3.4f, EFadeType::FadeinHyperbole))) * elevationFactorInversedClamp;

			band11Frequency = 3000 + static_cast<int>(5000.0f * ComputeFade(m_quadrantFine, 1.1f, EFadeType::FadeinLogarithmic));
			band11Gain = (-12.0f * ComputeFade(m_quadrantFine, 2.4f, EFadeType::FadeoutLogarithmic)) * elevationFactorInversedClamp;
			break;
		}
	default:
		{
			break;
		}
	}

	filterDominant10->ComputeCoefficients(band10Frequency, band10Quality, band10Gain);
	filterDominant11->ComputeCoefficients(band11Frequency, band11Quality, band11Gain);

	AkSampleType* pChannelInPlace = m_pIntermediateBufferConcealed;

	switch (cycleProxy)
	{
	case 0:
		{
			float const fadeFactor = 1.0f / g_bigFadeLength;

			for (int i = 1; i <= g_bigFadeLengthInteger; ++i) // fade in first cycle
			{
				float const filteredSample =
					filterDominant11->ProcessSample(
						filterDominant10->ProcessSample(*pChannelInPlace));

				*pChannelInPlace = filteredSample * ComputeFade((i * fadeFactor), 1.0f);
				++pChannelInPlace;
			}

			for (int i = g_bigFadeLengthInteger; i < inputValidFrames; ++i)
			{
				*pChannelInPlace =
					filterDominant11->ProcessSample(
						filterDominant10->ProcessSample(*pChannelInPlace));

				++pChannelInPlace;
			}

			break;
		}
	case 1:   // Fall through
	case 2:
		{
			float const fadeFactor = 1.0f / g_bigFadeLength;

			if (userData->lastSourceDirection == sourceDirection)
			{
				for (int i = 1; i <= g_bigFadeLengthInteger; ++i)     // Blend
				{
					float const filteredSampleDominant =
						filterDominant11->ProcessSample(
							filterDominant10->ProcessSample(*pChannelInPlace));

					float const filteredSampleResidual =
						filterResidual11->ProcessSample(
							filterResidual10->ProcessSample(*pChannelInPlace));

					float const fadeValue = i * fadeFactor;

					*pChannelInPlace =
						((filteredSampleResidual * ComputeFade(fadeValue, g_linearFadeStrength, EFadeType::FadeoutLogarithmic))
						 + (filteredSampleDominant * ComputeFade(fadeValue, g_linearFadeStrength, EFadeType::FadeinLogarithmic)));  // Linear Fade because correlation is 1

					++pChannelInPlace;
				}
			}
			else
			{

				for (int i = 1; i <= g_bigFadeLengthInteger; ++i)     // Blend
				{
					float const filteredSampleDominant =
						filterDominant11->ProcessSample(
							filterDominant10->ProcessSample(*pChannelInPlace));

					float const filteredSampleResidual =
						filterResidual09->ProcessSample(*pChannelInPlace);

					float const fadeValue = i * fadeFactor;

					*pChannelInPlace =
						((filteredSampleResidual * ComputeFade(fadeValue, g_linearFadeStrength, EFadeType::FadeoutLogarithmic))
						 + (filteredSampleDominant * ComputeFade(fadeValue, g_linearFadeStrength, EFadeType::FadeinLogarithmic)));  // Linear Fade because correlation is 1

					++pChannelInPlace;
				}
			}

			for (int i = g_bigFadeLengthInteger; i < inputValidFrames; ++i)     // Write Rest with new Filters
			{
				*pChannelInPlace =
					filterDominant11->ProcessSample(
						filterDominant10->ProcessSample(*pChannelInPlace));

				++pChannelInPlace;
			}

			break;
		}
	default:
		{
			break;
		}
	}
}

//////////////////////////////////////////////////////////////////////////
void CrySpatialFX::EQDirectChannel(
	AK::IAkMixerInputContext* const pInputContext,
	int const cycleProxy,
	int const inputValidFrames,
	ESourceDirection sourceDirection)
{
	SAkUserData* const userData = static_cast<SAkUserData*>(pInputContext->GetUserData());

	auto const elevationFactor = static_cast<float>(fabs(m_gameElevation) / g_piHalf);
	float const elevationFactorInversedClamp = (elevationFactor > 0.85f) ? 0.0f : 1.0f - (elevationFactor / 0.85f);

	BiquadIIFilter* filterDominant09 = &userData->pFilterBankA->filterBand09; // FilterDominant
	BiquadIIFilter* filterResidual09 = &userData->pFilterBankB->filterBand09; // FilterResidual
	BiquadIIFilter* filterResidual10 = &userData->pFilterBankB->filterBand09; // FilterResidual
	BiquadIIFilter* filterResidual11 = &userData->pFilterBankB->filterBand09; // FilterResidual

	switch (cycleProxy)
	{
	case 0:
		{
			filterDominant09 = &userData->pFilterBankA->filterBand09;
			break;
		}
	case 1:
		{
			filterDominant09 = &userData->pFilterBankA->filterBand09;
			filterResidual09 = &userData->pFilterBankB->filterBand09;
			filterResidual10 = &userData->pFilterBankB->filterBand10;
			filterResidual11 = &userData->pFilterBankB->filterBand11;
			break;
		}
	case 2:
		{
			filterDominant09 = &userData->pFilterBankB->filterBand09;
			filterResidual09 = &userData->pFilterBankA->filterBand09;
			filterResidual10 = &userData->pFilterBankA->filterBand10;
			filterResidual11 = &userData->pFilterBankA->filterBand11;
			break;
		}
	default:
		{
			break;
		}
	}

	int band09Frequency;
	float band09Quality = 1.0f;
	float band09Gain;

	switch (m_quadrant)
	{
	case 0:   // Fall through
	case 3:
		{
			band09Frequency = 9000 - static_cast<int>(7700.0f * ComputeFade(m_quadrantFine, 3.1f, EFadeType::FadeinLogarithmic));
			band09Gain = (6.0f + (2.0f * ComputeFade(m_quadrantFine, 1.6f, EFadeType::FadeinLogarithmic))) * elevationFactorInversedClamp;
			break;
		}
	case 1:   // Fall through
	case 2:
		{
			band09Frequency = 1300 + static_cast<int>(4200.0f * ComputeFade(m_quadrantFine, 0.7f, EFadeType::FadeinHyperbole));
			band09Gain = (8.0f * ComputeFade(m_quadrantFine, 1.1f, EFadeType::FadeoutLogarithmic)) * elevationFactorInversedClamp;
			break;
		}
	default:
		{
			break;
		}
	}

	filterDominant09->ComputeCoefficients(band09Frequency, band09Quality, band09Gain);

	float* pChannelInPlace = m_pIntermediateBufferDirect;

	switch (cycleProxy)
	{
	case 0:
		{
			float const fadeFactor = 1.0f / g_bigFadeLength;

			for (int i = 1; i <= g_bigFadeLengthInteger; ++i) // fade in first cycle
			{
				float const filteredSample = filterDominant09->ProcessSample(*pChannelInPlace);

				float const fadeValue = i * fadeFactor;

				*pChannelInPlace = filteredSample * ComputeFade(fadeValue, 1.0f);
				++pChannelInPlace;
			}

			for (int i = g_bigFadeLengthInteger; i < inputValidFrames; ++i)
			{
				*pChannelInPlace = filterDominant09->ProcessSample(*pChannelInPlace);

				++pChannelInPlace;
			}

			break;
		}
	case 1:   // Fall through
	case 2:
		{
			float const fadeFactor = 1.0f / g_bigFadeLength;

			if (userData->lastSourceDirection == sourceDirection)
			{
				for (int i = 1; i <= g_bigFadeLengthInteger; ++i)   // Blend
				{
					float const filteredSampleDominant = filterDominant09->ProcessSample(*pChannelInPlace);
					float const filteredSampleResidual = filterResidual09->ProcessSample(*pChannelInPlace);

					float const fadeValue = i * fadeFactor;

					*pChannelInPlace =
						((filteredSampleResidual * ComputeFade(fadeValue, g_linearFadeStrength, EFadeType::FadeoutLogarithmic))
						 + (filteredSampleDominant * ComputeFade(fadeValue, g_linearFadeStrength, EFadeType::FadeinLogarithmic))); // Linear Fade because correlation is 1
					++pChannelInPlace;
				}
			}
			else
			{
				for (int i = 1; i <= g_bigFadeLengthInteger; ++i)   // Blend
				{
					float const filteredSampleDominant = filterDominant09->ProcessSample(*pChannelInPlace);
					float const filteredSampleResidual =
						filterResidual10->ProcessSample(
							filterResidual11->ProcessSample(*pChannelInPlace));

					float const fadeValue = i * fadeFactor;

					*pChannelInPlace =
						((filteredSampleResidual * ComputeFade(fadeValue, g_linearFadeStrength, EFadeType::FadeoutLogarithmic))
						 + (filteredSampleDominant * ComputeFade(fadeValue, g_linearFadeStrength, EFadeType::FadeinLogarithmic))); // Linear Fade because correlation is 1
					++pChannelInPlace;
				}
			}

			for (int i = g_bigFadeLengthInteger; i < inputValidFrames; ++i)   // Write Rest with new Filters
			{
				*pChannelInPlace = filterDominant09->ProcessSample(*pChannelInPlace);

				++pChannelInPlace;
			}

			break;
		}
	default:
		{
			break;
		}
	}
}

}// namespace Plugins
}// namespace Wwise
}// namespace Impl
}// namespace CryAudio