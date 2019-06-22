// Copyright 2001-2019 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include "CrySpatialFXParams.h"
#include "CrySpatialFXAttachmentParams.h"
#include "SAkUserData.h"
#include <AK/SoundEngine/Common/IAkMixerPlugin.h>
#include <vector>

namespace CryAudio
{
namespace Impl
{
namespace Wwise
{
namespace Plugins
{
constexpr int g_maxDelay = 24;
constexpr float g_defaultFilterQuality = 0.7f;
constexpr float g_linearFadeStrength = 0.0f;
constexpr float g_concealedFilterTargetGain = -6.0f;

constexpr float g_smallFadeLength = 100.0f;
constexpr int g_smallFadeLengthInteger = 100; // fade length in samples

constexpr float g_bigFadeLength = 200.0f;
constexpr int g_bigFadeLengthInteger = 200; // fade length in samples

enum class EFadeType
{
	None,
	FadeinLogarithmic,
	FadeoutLogarithmic,
	FadeinHyperbole,
	FadeoutHyperbole,
};

class CrySpatialFX final : public AK::IAkMixerEffectPlugin
{
public:

	CrySpatialFX() = default;
	~CrySpatialFX() = default;

	// Plug-in initialization.
	// Prepares the plug-in for data processing, allocates memory and sets up the initial conditions.
	AKRESULT Init(
		AK::IAkPluginMemAlloc* in_pAllocator,
		AK::IAkMixerPluginContext* in_pContext,
		AK::IAkPluginParam* in_pParams,
		AkAudioFormat& in_rFormat) override;

	// Free all individual DelayBuffers in case OnInputDisconnected() wasn't called.
	// m_pMemAlloc is a proxy for DelayBuffer creation so it doesn't need individual cleanup.
	AKRESULT Term(AK::IAkPluginMemAlloc* in_pAllocator) override;

	// The reset action should perform any actions required to reinitialize the
	// state of the plug-in to its original state (e.g. after Init() or on effect bypass).
	AKRESULT Reset() override;

	// Plug-in information query mechanism used when the sound engine requires
	// information about the plug-in to determine its behavior.
	AKRESULT GetPluginInfo(AkPluginInfo& out_rPluginInfo) override;

	// This function is called whenever a new input is connected to the underlying mixing bus.
	void OnInputConnected(AK::IAkMixerInputContext* in_pInput) override;

	// Delete pDelayBuffer and remove from m_lActiveDelayBuffers
	void  OnInputDisconnected(AK::IAkMixerInputContext* in_pInput) override;

	float ComputeFade(float const volumeFactor, float const strength, EFadeType fadeType);
	float DecibelToVolume(float const decibels);
	float VolumeToDecibel(float const volume);

	// Takes an Azimuth and Elevation to compute the L/R differences in delay, Volume and EQ
	void ComputeDelayChannelData(
		AkReal32& out_volumeNormalized,
		int& out_delay,
		float& out_eqGain);

	// Takes pointers to the corresponding buffers and combines the in/del buffer to an out buffer.
	void FillIntermediateBufferDirect(
		AkSampleType* pInChannel,
		AkSampleType* pDelChannel,
		AkSampleType* pOutChannel,
		SAkUserData* const userData,
		int const maxFrames,
		ESourceDirection const sourceDirection);

	// Takes pointers to the corresponding buffers and combines the in/del buffer to an out buffer.
	void FillIntermediateBufferConcealed(
		AkSampleType* pInChannel,
		AkSampleType* pDelChannel,
		AkSampleType* pOutChannel,
		SAkUserData* const userData,
		int const delayNext,
		int const maxFrames,
		ESourceDirection const sourceDirection);

	//EQ the inputBuffer before it is split
	void EQInputBuffer(
		AK::IAkMixerInputContext* pInputContext,
		AkAudioBuffer* pInputBuffer,
		float& out_compensationalVolume);

	// This function is called whenever an input (voice or bus) needs to be mixed.
	// During an audio frame, ConsumeInput() will be called for each input that need to be mixed.
	void ConsumeInput(
		AK::IAkMixerInputContext* in_pInputContext,
		AkRamp in_baseVolume,
		AkRamp in_emitListVolume,
		AkAudioBuffer* io_pInputBuffer,
		AkAudioBuffer* io_pMixBuffer) override;

	// This function is called once every audio frame, when all inputs have been mixed in
	// with ConsumeInput(). It is the time when the plugin may perform final DSP/bookkeeping.
	// After the bus buffer io_pMixBuffer has been pushed to the bus downstream
	// (or the output device), it is cleared out for the next frame.
	void OnMixDone(AkAudioBuffer* io_pMixBuffer) override;

	// This function is called once every audio frame, after all insert effects
	// on the bus have been processed, which occur after the post mix pass of OnMixDone().
	// After the bus buffer io_pMixBuffer has been pushed to the bus downstream
	// (or the output device), it is cleared out for the next frame.
	void OnEffectsProcessed(AkAudioBuffer* io_pMixBuffer) override;

	// This function is called once every audio frame, after all insert effects
	// on the bus have been processed, and after metering has been processed
	// (if applicable), which occur after OnEffectsProcessed().
	// After the bus buffer io_pMixBuffer has been pushed to the bus downstream
	// (or the output device), it is cleared out for the next frame.
	// Mixer plugins may use this hook for processing the signal after it has been metered.
	void OnFrameEnd(AkAudioBuffer* io_pMixBuffer, AK::IAkMetering* in_pMetering) override;

	// Consumes a mono input buffer that has ListenerRelativeRouting and spatializes it.
	void HRTF_MonoToStereo(
		AK::IAkMixerInputContext* const pInputContext,
		AkRamp const baseVolume,
		AkRamp const busVolume,
		AkAudioBuffer* const pInputBuffer,
		AkAudioBuffer* const pMixBuffer);

	// Consumes a mono input buffer that has ListenerRelativeRouting and pans it to surround.
	void HRTF_MonoTo5_1(
		AK::IAkMixerInputContext* const pInputContext,
		AkRamp const baseVolume,
		AkRamp const busVolume,
		AkAudioBuffer* pInputBuffer,
		AkAudioBuffer* pMixBuffer);

	// Consumes a mono input buffer that has ListenerRelativeRouting and pans it to surround.
	void HRTF_MonoTo7_1(
		AK::IAkMixerInputContext* const pInputContext,
		AkRamp const baseVolume,
		AkRamp const busVolume,
		AkAudioBuffer* pInputBuffer,
		AkAudioBuffer* pMixBuffer);

	// Consumes inputBuffers of Buses and Voices that are not ListenerRelative.
	void NonHRTF_StereoToStereo(
		AK::IAkMixerInputContext* pInputContext,
		AkRamp const baseVolume,
		AkRamp const busVolume,
		AkAudioBuffer* pInputBuffer,
		AkAudioBuffer* pMixBuffer);

private:

	void ComputeQuadrant();
	void FillDelayBuffer(AkSampleType* pInChannelDelayBuffer, AkSampleType* const pOutChannelDelayBuffer, int const delayCurrent);
	void EQDirectChannel(AK::IAkMixerInputContext* const pInputContext, int const cycleProxy, int const inputValidFrames, ESourceDirection sourceDirection);
	void EQConcealedChannel(AK::IAkMixerInputContext* const pInputContext, int const cycleProxy, int const inputValidFrames, ESourceDirection sourceDirection);

private:

	AkUInt16                   m_maxDelay = g_maxDelay;
	float                      m_bufferFadeStrength = 1.0f;
	float                      m_EQFadeStrength = g_linearFadeStrength;

	float                      m_sampleRate = 0.0f;
	int                        m_numOutputChannels = 0;
	AkUInt32                   m_bufferSize = 0;
	AkUInt16                   m_delayBufferSize = g_maxDelay;

	AkReal32                   m_gameAzimuth = 0.0f;
	AkReal32                   m_gameElevation = 0.0f;

	float                      m_quadrantFine = 0.0f;
	int                        m_quadrant = 0;

	CrySpatialFXParams*        m_pParams = nullptr;
	AK::IAkPluginMemAlloc*     m_pAllocator = nullptr;
	AK::IAkMixerPluginContext* m_pContext = nullptr;
	AkSampleType*              m_pMemAlloc = nullptr;
	AkSampleType*              m_pIntermediateBufferDirect = nullptr;
	AkSampleType*              m_pIntermediateBufferConcealed = nullptr;

	std::vector<SAkUserData*>  m_activeUserData;
};
} // namespace Plugins
} // namespace Wwise
} // namespace Impl
} // namespace CryAudio