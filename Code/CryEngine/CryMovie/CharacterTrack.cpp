// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "CharacterTrack.h"

#define LOOP_TRANSITION_TIME SAnimTime(1.0f)

//////////////////////////////////////////////////////////////////////////
bool CCharacterTrack::Serialize(XmlNodeRef& xmlNode, bool bLoading, bool bLoadEmptyTracks)
{
	if (bLoading)
	{
		xmlNode->getAttr("AnimationLayer", m_iAnimationLayer);
	}
	else
	{
		xmlNode->setAttr("AnimationLayer", m_iAnimationLayer);
	}

	return TAnimTrack<SCharacterKey>::Serialize(xmlNode, bLoading, bLoadEmptyTracks);
}

void CCharacterTrack::SerializeKey(SCharacterKey& key, XmlNodeRef& keyNode, bool bLoading)
{
	if (bLoading)
	{
		const char* str;

		str = keyNode->getAttr("anim");
		cry_strcpy(key.m_animation, str);

		key.m_bLoop = false;
		key.m_bBlendGap = false;
		key.m_bInPlace = false;
		key.m_speed = 1;

		keyNode->getAttr("speed", key.m_speed);
		keyNode->getAttr("loop", key.m_bLoop);
		keyNode->getAttr("blendGap", key.m_bBlendGap);
		keyNode->getAttr("unload", key.m_bUnload);
		keyNode->getAttr("inplace", key.m_bInPlace);
		keyNode->getAttr("start", key.m_startTime);
		keyNode->getAttr("end", key.m_endTime);
	}
	else
	{
		if (strlen(key.m_animation) > 0)
			keyNode->setAttr("anim", key.m_animation);
		if (key.m_speed != 1)
			keyNode->setAttr("speed", key.m_speed);
		if (key.m_bLoop)
			keyNode->setAttr("loop", key.m_bLoop);
		if (key.m_bBlendGap)
			keyNode->setAttr("blendGap", key.m_bBlendGap);
		if (key.m_bUnload)
			keyNode->setAttr("unload", key.m_bUnload);
		if (key.m_bInPlace)
			keyNode->setAttr("inplace", key.m_bInPlace);
		if (key.m_startTime != 0.0f)
			keyNode->setAttr("start", key.m_startTime);
		if (key.m_endTime != 0.0f)
			keyNode->setAttr("end", key.m_endTime);
	}
}

float CCharacterTrack::GetKeyDuration(int key) const
{
	assert(key >= 0 && key < (int)m_keys.size());
	const float EPSILON = 0.001f;
	if (m_keys[key].m_bLoop)
	{
		SAnimTime lastTime = m_timeRange.end;
		if (key + 1 < (int)m_keys.size() && m_keys[key + 1].GetAnimDuration() > SAnimTime(0.0f))
		{
			lastTime = m_keys[key + 1].m_time + min(LOOP_TRANSITION_TIME, SAnimTime(GetKeyDuration(key + 1)));
		}
		// duration is unlimited but cannot last past end of track or time of next key on track.
		return max(lastTime - m_keys[key].m_time, SAnimTime(0.0f)).ToFloat();
	}
	else
	{
		return m_keys[key].GetCroppedAnimDuration();
	}
}
