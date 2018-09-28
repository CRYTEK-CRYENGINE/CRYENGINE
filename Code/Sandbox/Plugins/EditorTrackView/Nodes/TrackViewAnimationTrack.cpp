// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

// CryEngine Source File.
// Copyright 2001-2016 Crytek GmbH. All rights reserved.

#include "StdAfx.h"
#include "TrackViewAnimationTrack.h"
#include "TrackViewAnimNode.h"
#include "TrackViewEntityNode.h"

CTrackViewAnimationTrack::CTrackViewAnimationTrack(IAnimTrack* pTrack, CTrackViewAnimNode* pTrackAnimNode, CTrackViewNode* pParentNode, bool bIsSubTrack, uint subTrackIndex)
	: CTrackViewTrack(pTrack, pTrackAnimNode, pParentNode, bIsSubTrack, subTrackIndex)
{
}

CTimeValue CTrackViewAnimationTrack::GetKeyDuration(const uint index) const
{
	SCharacterKey key;
	GetKey(index, &key);

	if (key.m_defaultAnimDuration == 0)
	{
		key.m_defaultAnimDuration = GetKeyDurationFromAnimationData(key);
	}

	return key.GetCroppedAnimDuration() / key.m_speed;
}

CTimeValue CTrackViewAnimationTrack::GetKeyAnimDuration(const uint index) const
{
	SCharacterKey key;
	GetKey(index, &key);

	if (key.m_defaultAnimDuration == 0)
	{
		key.m_defaultAnimDuration = GetKeyDurationFromAnimationData(key);
	}

	return key.m_defaultAnimDuration;
}

CTimeValue CTrackViewAnimationTrack::GetKeyAnimStart(const uint index) const
{
	SCharacterKey key;
	GetKey(index, &key);

	return key.m_startTime;
}

CTimeValue CTrackViewAnimationTrack::GetKeyAnimEnd(const uint index) const
{
	SCharacterKey key;
	GetKey(index, &key);

	return key.m_endTime;
}

CTimeValue CTrackViewAnimationTrack::GetKeyDurationFromAnimationData(const SCharacterKey& key) const
{
	CTimeValue duration = 0;

	CTrackViewNode* pParent = GetParentNode();
	if (!pParent)
	{
		return duration;
	}

	ETrackViewNodeType nodeType = pParent->GetNodeType();
	if (nodeType != ETrackViewNodeType::eTVNT_AnimNode)
	{
		return duration;
	}

	CTrackViewAnimNode* animNode = static_cast<CTrackViewAnimNode*>(pParent);
	EAnimNodeType animNodeType = animNode->GetType();
	if (nodeType != eAnimNodeType_Entity)
	{
		return duration;
	}

	CTrackViewEntityNode* entityNode = static_cast<CTrackViewEntityNode*>(animNode);

	IEntity* pEntity = entityNode->GetEntity();
	if (!pEntity)
	{
		return duration;
	}

	ICharacterInstance* pCharacter = pEntity->GetCharacter(0);
	if (!pCharacter)
	{
		return duration;
	}

	IAnimationSet* pAnimations = pCharacter->GetIAnimationSet();
	if (!pAnimations)
	{
		return duration;
	}

	int animId = pAnimations->GetAnimIDByName(key.m_animation);
	if (animId >= 0)
	{
		duration = pAnimations->GetDuration(animId);
	}

	return duration;
}
