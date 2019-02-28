// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

// -------------------------------------------------------------------------
//  File name:   ILipSyncProvider.h
//  Version:     v1.00
//  Created:     2014-08-29 by Christian Werle.
//  Description: Interface for the lip-sync provider that gets injected into the IEntitySoundProxy.
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////

#pragma once

enum ELipSyncMethod
{
	eLSM_None,
	eLSM_MatchAnimationToSoundName,
};

struct IEntityAudioComponent;

struct ILipSyncProvider
{
	virtual ~ILipSyncProvider() {}

	//! Use this to start loading as soon as the sound gets requested (this can be safely ignored).
	virtual void RequestLipSync(IEntityAudioComponent* pProxy, const CryAudio::ControlId audioTriggerId, const ELipSyncMethod lipSyncMethod) = 0;

	virtual void StartLipSync(IEntityAudioComponent* pProxy, const CryAudio::ControlId audioTriggerId, const ELipSyncMethod lipSyncMethod) = 0;
	virtual void PauseLipSync(IEntityAudioComponent* pProxy, const CryAudio::ControlId audioTriggerId, const ELipSyncMethod lipSyncMethod) = 0;
	virtual void UnpauseLipSync(IEntityAudioComponent* pProxy, const CryAudio::ControlId audioTriggerId, const ELipSyncMethod lipSyncMethod) = 0;
	virtual void StopLipSync(IEntityAudioComponent* pProxy, const CryAudio::ControlId audioTriggerId, const ELipSyncMethod lipSyncMethod) = 0;
	virtual void UpdateLipSync(IEntityAudioComponent* pProxy, const CryAudio::ControlId audioTriggerId, const ELipSyncMethod lipSyncMethod) = 0;
};

DECLARE_SHARED_POINTERS(ILipSyncProvider);
