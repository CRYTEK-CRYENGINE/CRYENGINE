// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "stdafx.h"

#include "AnimEventFootstepGenerator.h"
#include "Serialization.h"
#include "AnimationContent.h"
#include <CryAnimation/ICryAnimation.h>

// ---------------------------------------------------------------------------

enum { ANIMATION_FRAMERATE = 30 };

typedef std::map<uint32, std::vector<QuatT>> BoneSampleMap;

static bool SampleAnimation(BoneSampleMap* framesPerChannel, float* animationLength, string* errorMessage, const std::vector<unsigned int>& bonesToTrack, const char* characterPath, const char* animationName)
{
	for (size_t i = 0; i < bonesToTrack.size(); ++i)
		(*framesPerChannel)[bonesToTrack[i]].clear();

	*animationLength = -1.0f;

	// Create a character instance for the frame
	ICharacterInstance* character = gEnv->pCharacterManager->CreateInstance(characterPath);
	CRY_ASSERT(character);
	if (NULL == character)
	{
		*errorMessage = "Failed to create character instance!";
		return false;
	}
	character->AddRef();

	//
	ISkeletonAnim& skeletonAnimation = *character->GetISkeletonAnim();
	ISkeletonPose& skeletonPose = *character->GetISkeletonPose();
	IAnimationSet* animationSet = character->GetIAnimationSet();
	if (NULL == animationSet)
	{
		if (errorMessage)
			*errorMessage = "Character instance creation error!";
		return false;
	}

	const int animationId = animationSet->GetAnimIDByName(animationName);
	if (animationName[0] == '\0' || animationId < 0)
	{
		*errorMessage = "Character instance creation error!";
		return false;
	}

	// Play the animation
	CryCharAnimationParams params(0);
	params.m_nFlags = CA_REPEAT_LAST_KEY | CA_MANUAL_UPDATE | CA_ALLOW_ANIM_RESTART;
	params.m_fTransTime = 0.0f;
	params.m_fKeyTime = 0.0f;
	skeletonAnimation.StartAnimationById(animationId, params);

	*animationLength = skeletonAnimation.GetAnimFromFIFO(0, 0).GetExpectedTotalDurationSeconds();

	const uint32 frameCount = uint32((*animationLength * ANIMATION_FRAMERATE) + 0.5f);

	// Prepare sample vectors
	for (BoneSampleMap::iterator itS = framesPerChannel->begin(); itS != framesPerChannel->end(); ++itS)
		itS->second.resize(frameCount);

	// For each frame, compute pose.
	const float fInvNbFrames = 1.0f / float(frameCount);
	for (uint32 frame = 0; frame < frameCount; ++frame)
	{
		const float fNormalizedTime = float(frame) * fInvNbFrames;

		character->SetCharEditMode(CA_CharacterTool);
		skeletonPose.SetForceSkeletonUpdate(1);

		skeletonAnimation.SetLayerNormalizedTime(0, fNormalizedTime);

		// Play animation
		SAnimationProcessParams params;
		params.bOnRender = false;
		params.locationAnimation = QuatTS(IDENTITY);
		params.zoomAdjustedDistanceFromCamera = 1.0f;
		character->StartAnimationProcessing(params);

		// Directly compute and get the result on this thread
		character->FinishAnimationComputations();

		const QuatT& qRoot = skeletonPose.GetAbsJointByID(0);

		for (BoneSampleMap::iterator itS = framesPerChannel->begin(); itS != framesPerChannel->end(); ++itS)
		{
			QuatT qBone = skeletonPose.GetAbsJointByID(itS->first);
			qBone.t -= qRoot.t;
			qBone.q = qRoot.q.GetInverted() * qBone.q;

			itS->second[frame] = qBone;
		}
	}

	// Release character instance
	character->Release();
	return true;
}

namespace CharacterTool
{

class FootstepGenerator
{
public:
	FootstepGenerator(float fFootDownHeight, float fFootUpperLimit, uint32 nFootBoneID, const AnimEvent& event, bool bShuffle)
		: m_footDownHeight(fFootDownHeight)
		, m_footUpperLimit(fFootUpperLimit)
		, m_foleyDelay(0.0f)
		, m_footBoneId(nFootBoneID)
		, m_shuffle(bShuffle)
		, m_event(event)
		, m_generateFoley(false)
	{
	}

	void Populate(AnimEvents* events, float fAnimLen, const BoneSampleMap& mSamples)
	{
		BoneSampleMap::const_iterator itS = mSamples.find(m_footBoneId);
		if (mSamples.end() == itS)
			return;

		const std::vector<QuatT>& samples = itS->second;

		bool bFootWasUp = samples.front().t.z > m_footUpperLimit;
		bool bFootDown = samples.front().t.z < m_footDownHeight;
		const float fInvNbFrames = 1.0f / float(samples.size());
		for (size_t frame = 1; frame < samples.size(); ++frame)
		{
			const QuatT& sample = samples[frame];
			const bool bCurrDown = sample.t.z < m_footDownHeight;
			bool bCanPostEvent = !m_shuffle && bFootWasUp || m_shuffle && !bFootWasUp;
			if (!bFootDown && bCurrDown && bCanPostEvent)
			{
				const float fTime = float(frame) * fInvNbFrames;
				AnimEvent event = m_event;
				event.startTime = fTime;
				event.endTime = fTime;
				events->push_back(event);

				if (m_generateFoley)
				{
					const float fFoleyTime = clamp_tpl(fTime + (m_foleyDelay / fAnimLen), 0.0f, 1.0f);
					AnimEvent event = m_foleyEvent;
					event.startTime = fFoleyTime;
					event.endTime = fFoleyTime;
					events->push_back(event);
				}
			}

			if (bCurrDown)
				bFootWasUp = false;
			else
				bFootWasUp = bFootWasUp || sample.t.z > m_footUpperLimit;

			bFootDown = bCurrDown;
		}
	}

	unsigned int FootBoneID() const { return m_footBoneId; }

	void         SetFoleyEvent(const AnimEvent& event, float foleyDelay)
	{
		m_generateFoley = true;
		m_foleyDelay = foleyDelay;
		m_foleyEvent = event;
	}
private:
	float        m_footDownHeight;
	float        m_footUpperLimit;
	float        m_foleyDelay;
	bool         m_shuffle;
	unsigned int m_footBoneId;
	AnimEvent    m_event;
	bool         m_generateFoley;
	AnimEvent    m_foleyEvent;
};

void FootstepGenerationParameters::Serialize(IArchive& ar)
{
	ar(Serialization::Range(footHeightMM, 0.0f, 1000.0f), "footHeightMM", "Foot Height (mm)");
	string oldLeftFootJoint = leftFootJoint;
	ar(JointName(leftFootJoint), "leftFootJoint", "Left Foot Joint");
	string oldRightFootJoint = rightFootJoint;
	ar(JointName(rightFootJoint), "rightFootJoint", "Right Foot Joint");

	if (ar.openBlock("footEvent", "Foot Step"))
	{
		ar(leftFootEvent, "leftFootEvent", "Left");
		ar(rightFootEvent, "rightFootEvent", "Right");

		ar(generateFoleys, "generateFoleys", "Generate Foleys");
		ar(Serialization::Decorators::Range(foleyDelayFrames, 0, 40), "foleyDelayFrames", generateFoleys ? "Foley Delay (Frames)" : 0);
		ar(leftShuffleFoleyEvent, "leftShuffleFoleyEvent", generateFoleys ? "Left Foley" : 0);
		ar(rightShuffleFoleyEvent, "rightShuffleFoleyEvent", generateFoleys ? "Right Foley" : 0);
		ar.closeBlock();
	}
	if (ar.openBlock("shuffleEvent", "Foot Shuffle"))
	{
		ar(Serialization::Range(footShuffleUpperLimitMM, 0.0f, 1000.0f), "footShuffleUpperLimitMM", "Foot Shuffle Upper Limit (mm)");
		ar(leftShuffleEvent, "leftShuffleEvent", "Left");
		ar(rightShuffleEvent, "rightShuffleEvent", "Right");

		ar(Serialization::Range(shuffleFoleyDelayFrames, 0, 40), "shuffleFoleyDelayFrames", generateFoleys ? "Shuffle Foley Delay (Frames)" : 0);
		ar(leftFoleyEvent, "leftFoleyEvent", generateFoleys ? "Left Foley" : 0);
		ar(rightFoleyEvent, "rightFoleyEvent", generateFoleys ? "Right Foley" : 0);
		ar.closeBlock();
	}

	if (oldLeftFootJoint != leftFootJoint)
		SetLeftFootJoint(leftFootJoint.c_str());
	if (oldRightFootJoint != rightFootJoint)
		SetRightFootJoint(rightFootJoint.c_str());
}

void FootstepGenerationParameters::SetLeftFootJoint(const char* joint)
{
	leftFootJoint = joint;
	leftFootEvent.boneName = joint;
	leftFoleyEvent.boneName = joint;
	leftShuffleEvent.boneName = joint;
	leftShuffleFoleyEvent.boneName = joint;
}

void FootstepGenerationParameters::SetRightFootJoint(const char* joint)
{
	rightFootJoint = joint;
	rightFootEvent.boneName = joint;
	rightFoleyEvent.boneName = joint;
	rightShuffleEvent.boneName = joint;
	rightShuffleFoleyEvent.boneName = joint;
}

//////////////////////////////////////////////////////////////////////////
bool GenerateFootsteps(AnimationContent* content, string* errorMessage, ICharacterInstance* character, const char* animationName, const FootstepGenerationParameters& params)
{
	// Character path (to create intermediate char instances)
	const char* szFilePath = character->GetFilePath();

	// Sample the animation
	IDefaultSkeleton& skeleton = character->GetIDefaultSkeleton();
	float footHeight = params.footHeightMM * 0.001f;
	const uint32 leftFootIndex = skeleton.GetJointIDByName(params.leftFootJoint.c_str());
	const uint32 rightFootIndex = skeleton.GetJointIDByName(params.rightFootJoint.c_str());

	float footShuffleUpperLimit = params.footShuffleUpperLimitMM * 0.001f;
	FootstepGenerator leftFoot(footHeight, footShuffleUpperLimit, leftFootIndex, params.leftFootEvent, false);
	FootstepGenerator rightFoot(footHeight, footShuffleUpperLimit, rightFootIndex, params.rightFootEvent, false);
	FootstepGenerator leftShuffle(footHeight, footShuffleUpperLimit, leftFootIndex, params.leftShuffleEvent, true);
	FootstepGenerator rightShuffle(footHeight, footShuffleUpperLimit, rightFootIndex, params.rightShuffleEvent, true);

	std::vector<unsigned int> bones;
	bones.push_back(leftFoot.FootBoneID());
	bones.push_back(rightFoot.FootBoneID());
	bones.push_back(leftShuffle.FootBoneID());
	bones.push_back(rightShuffle.FootBoneID());
	std::sort(bones.begin(), bones.end());
	bones.erase(std::unique(bones.begin(), bones.end()), bones.end());

	BoneSampleMap animation;
	float animationLength = 0.0f;
	if (!SampleAnimation(&animation, &animationLength, errorMessage, bones, character->GetFilePath(), animationName))
		return false;

	// Analysis. Compute left and right foot step events and foleys if needed
	if (params.generateFoleys)
	{
		leftFoot.SetFoleyEvent(params.leftFoleyEvent, float(params.foleyDelayFrames) / ANIMATION_FRAMERATE);
		rightFoot.SetFoleyEvent(params.rightFoleyEvent, float(params.foleyDelayFrames) / ANIMATION_FRAMERATE);
		leftShuffle.SetFoleyEvent(params.leftShuffleFoleyEvent, float(params.shuffleFoleyDelayFrames) / ANIMATION_FRAMERATE);
		rightShuffle.SetFoleyEvent(params.rightShuffleFoleyEvent, float(params.shuffleFoleyDelayFrames) / ANIMATION_FRAMERATE);
	}

	AnimEvents events;
	leftFoot.Populate(&events, animationLength, animation);
	rightFoot.Populate(&events, animationLength, animation);
	leftShuffle.Populate(&events, animationLength, animation);
	rightShuffle.Populate(&events, animationLength, animation);
	std::stable_sort(events.begin(), events.end());

	content->events.insert(content->events.end(), events.begin(), events.end());
	return true;
}

}

