// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#ifndef __Communication_h__
#define __Communication_h__

#pragma once

#include <CryAISystem/ICommunicationManager.h>
#include <CryCore/Containers/VariableCollection.h>

struct SCommunicationChannelParams
{
	enum ECommunicationChannelType
	{
		Global = 0,
		Group,
		Personal,
		Invalid,
	};

	SCommunicationChannelParams() : type(Invalid), minSilence(0), priority(0), actorMinSilence(0), ignoreActorSilence(false), flushSilence(0) {}

	// Minimum silence this channel imposes once normal communication is finished.
	float                     minSilence;
	// Minimum silence this channel imposes on manager if its higher priority and flushes the system.
	float                     flushSilence;

	string                    name;
	CommChannelID             parentID;
	ECommunicationChannelType type;
	uint8                     priority;

	// Minimum silence this channel imposes on an actor once it starts to play.
	float actorMinSilence;
	// Indicates whether this channel should ignore actor silenced restriction.
	bool  ignoreActorSilence;
};

struct SCommunicationVariation
{
	SCommunicationVariation()
		: timeout(0.0f)
		, flags(0)
	{
		condition.reset();
	}

	string                   animationName;
	string                   soundName;
	string                   voiceName;

	float                    timeout;

	uint32                   flags;
	Variables::ExpressionPtr condition;
};

struct SCommunication
{
	enum EVariationChoiceMethod
	{
		Random = 0,
		Sequence,
		RandomSequence,
		Match, // only valid for responses
	};

	enum ECommunicationFlags
	{
		LookAtTarget    = 1 << 0,

		FinishAnimation = 1 << 8,
		FinishSound     = 1 << 9,
		FinishVoice     = 1 << 10,
		FinishTimeout   = 1 << 11,
		FinishAll       = FinishAnimation | FinishSound | FinishVoice | FinishTimeout,

		BlockMovement   = 1 << 12,
		BlockFire       = 1 << 13,
		BlockAll        = BlockMovement | BlockFire,

		AnimationAction = 1 << 16,
	};

	string                 name;
	CommID                 responseID;

	EVariationChoiceMethod choiceMethod;
	EVariationChoiceMethod responseChoiceMethod;

	struct History
	{
		History()
			: played(0)
		{
		}

		uint32 played;
	} history;

	std::vector<SCommunicationVariation> variations;

	bool forceAnimation;
	bool hasAnimation;
};

#endif //__Communication_h__
