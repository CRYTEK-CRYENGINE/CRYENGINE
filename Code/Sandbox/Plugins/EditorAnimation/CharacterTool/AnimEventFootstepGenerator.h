// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include "AnimEvent.h"

struct ICharacterInstance;

namespace CharacterTool
{

struct FootstepGenerationParameters
{
	bool      generateFoleys;
	int       foleyDelayFrames;
	int       shuffleFoleyDelayFrames;
	float     footHeightMM;
	float     footShuffleUpperLimitMM;
	string    leftFootJoint;
	string    rightFootJoint;
	AnimEvent leftFootEvent;
	AnimEvent rightFootEvent;
	AnimEvent leftFoleyEvent;
	AnimEvent rightFoleyEvent;
	AnimEvent leftShuffleEvent;
	AnimEvent rightShuffleEvent;
	AnimEvent leftShuffleFoleyEvent;
	AnimEvent rightShuffleFoleyEvent;

	void      Serialize(Serialization::IArchive& ar);

	FootstepGenerationParameters()
		: foleyDelayFrames(3)
		, shuffleFoleyDelayFrames(3)
		, footHeightMM(200.0f)
		, footShuffleUpperLimitMM(275.0f)
		, generateFoleys(true)
	{
		SetLeftFootJoint("Bip01 L Foot");
		SetRightFootJoint("Bip01 R Foot");

		leftFootEvent.startTime = -1;
		leftFootEvent.endTime = -1;
		leftFootEvent.type = "footstep";
		rightFootEvent.startTime = -1;
		rightFootEvent.endTime = -1;
		rightFootEvent.type = "footstep";

		leftFoleyEvent.startTime = -1;
		leftFoleyEvent.endTime = -1;
		leftFoleyEvent.type = "foley";
		rightFoleyEvent.startTime = -1;
		rightFoleyEvent.endTime = -1;
		rightFoleyEvent.type = "foley";

		leftShuffleEvent.startTime = -1;
		leftShuffleEvent.endTime = -1;
		leftShuffleEvent.type = "foley";
		rightShuffleEvent.startTime = -1;
		rightShuffleEvent.endTime = -1;
		rightShuffleEvent.type = "foley";

		leftShuffleFoleyEvent.startTime = -1;
		leftShuffleFoleyEvent.endTime = -1;
		leftShuffleFoleyEvent.type = "foley";
		rightShuffleFoleyEvent.startTime = -1;
		rightShuffleFoleyEvent.endTime = -1;
		rightShuffleFoleyEvent.type = "foley";
	}

	void SetLeftFootJoint(const char* jointName);
	void SetRightFootJoint(const char* jointName);
};

struct AnimationContent;
bool GenerateFootsteps(AnimationContent* content, string* errorMessage, ICharacterInstance* character, const char* animationName, const FootstepGenerationParameters& params);

}

