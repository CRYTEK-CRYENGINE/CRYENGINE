// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

/*************************************************************************
   -------------------------------------------------------------------------

   Interface for force feedback system

   -------------------------------------------------------------------------
   History:
   - 18-02-2010:	Created by Benito Gangoso Rodriguez
   - 24-09-2012: Modified by Dario Sancho
* added support for Durango Triggers

*************************************************************************/

#pragma once

#ifndef _IFORCEFEEDBACKSYSTEM_H_
	#define _IFORCEFEEDBACKSYSTEM_H_

typedef uint16 ForceFeedbackFxId;
static const ForceFeedbackFxId InvalidForceFeedbackFxId = 0xFFFF;
struct SFFTriggerOutputData;

struct IFFSPopulateCallBack
{
	virtual ~IFFSPopulateCallBack(){}
	// Description:
	//			Callback function to retrieve all effects available
	//			Use it in conjunction with IForceFeedbackSystem::EnumerateEffects
	// See Also:
	//			IForceFeedbackSystem::EnumerateEffects
	//			IForceFeedbackSystem::GetEffectNamesCount
	// Arguments:
	//     pName - Name of one of the effects retrieved
	virtual void AddFFSEffectName(const char* const pName) = 0;
};

struct SForceFeedbackRuntimeParams
{
	SForceFeedbackRuntimeParams()
		: intensity(1.0f)
		, delay(0.0f)
	{

	}

	SForceFeedbackRuntimeParams(float _intensity, float _delay)
		: intensity(_intensity)
		, delay(_delay)
	{

	}

	float intensity;    //Scales overall intensity of the effect (0.0f - 1.0f)
	float delay;        //Start playback delay
};

struct IForceFeedbackSystem
{
	virtual ~IForceFeedbackSystem(){}
	// Description:
	//			Execute a force feedback effect by id
	// See Also:
	//			IForceFeedbackSystem::GetEffectIdByName
	// Arguments:
	//			id - Effect id
	//			runtimeParams - Runtime params for effect, including intensity, delay...
	virtual void PlayForceFeedbackEffect(ForceFeedbackFxId id, const SForceFeedbackRuntimeParams& runtimeParams) = 0;

	// Description:
	//			Stops an specific effect by id (all running instances, if more than one)
	// See Also:
	//			IForceFeedbackSystem::GetEffectIdByName
	//			IForceFeedbackSystem::StopAllEffects
	// Arguments:
	//     id - Effect id
	virtual void StopForceFeedbackEffect(ForceFeedbackFxId id) = 0;

	// Description:
	//			Returns the internal id of the effect for a given name, if defined
	//			If not found, it will return InvalidForceFeedbackFxId
	// Arguments:
	//     effectName - Name of the effect
	// Return Value:
	//     Effect id for the given name. InvalidForceFeedbackFxId if the effect was not found
	virtual ForceFeedbackFxId GetEffectIdByName(const char* effectName) const = 0;

	// Description:
	//			Stops all running effects
	// See Also:
	//			IForceFeedbackSystem::StopForceFeedbackEffect
	virtual void StopAllEffects() = 0;

	// Description:
	//			This function can be used to request custom frame vibration values for the frame
	//			It can be useful, if a very specific vibration pattern/rules are used
	//			This custom values will be added to any other pre-defined effect running
	// Arguments:
	//			amplifierA - Vibration amount from 0.0 to 1.0 for high frequency motor
	//			amplifierB - Vibration amount from 0.0 to 1.0 for low frequency motor
	virtual void AddFrameCustomForceFeedback(const float amplifierA, const float amplifierB, const float amplifierLT = 0.0f, const float amplifierRT = 0.0f) = 0;

	// Description:
	//			This function can be used to request custom vibration values for the triggers
	//			It can be useful, if a very specific vibration pattern/rules are used
	//			This custom values will be added to any other pre-defined effect running
	// Arguments:
	//			leftGain - Vibration amount from 0.0 to 1.0 for left trigger motor
	//			rightGain - Vibration amount from 0.0 to 1.0 for right trigger motor
	//			leftEnvelope - Envelope value (uint16) from 0 to 2000 for left trigger motor
	//			rightEnvelope - Envelope value (uint16) from 0 to 2000 for left trigger motor
	virtual void AddCustomTriggerForceFeedback(const SFFTriggerOutputData& triggersData) = 0;

	// Description:
	//			Use this function to retrieve all effects names available.
	//			pCallback will be used and invoked once for every effect available, passing its name
	// See Also:
	//			IFFSPopulateCallBack::AddFFSEffectName
	// Arguments:
	//     pCallBack - Pointer to object which implements IFFSPopulateCallBack interace
	virtual void EnumerateEffects(IFFSPopulateCallBack* pCallBack) = 0;    // intended to be used only from the editor

	// Description:
	//			Returns the number of effects available
	// See Also:
	//			IFFSPopulateCallBack::AddFFSEffectName
	//			IForceFeedbackSystem::EnumerateEffects
	// Return Value:
	//			Number of effects
	virtual int GetEffectNamesCount() const = 0;

	// Description:
	//			Prevents force feedback effects from starting. Each with bSuppressEffects = true
	//			will increment the lock count, with false will decrement
	virtual void SuppressEffects(bool bSuppressEffects) = 0;
};

#endif
