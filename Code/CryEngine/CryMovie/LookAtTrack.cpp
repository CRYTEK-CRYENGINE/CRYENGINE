// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "LookAtTrack.h"

bool CLookAtTrack::Serialize(XmlNodeRef& xmlNode, bool bLoading, bool bLoadEmptyTracks)
{
	if (bLoading)
	{
		xmlNode->getAttr("AnimationLayer", m_iAnimationLayer);
	}
	else
	{
		xmlNode->setAttr("AnimationLayer", m_iAnimationLayer);
	}

	return TAnimTrack<SLookAtKey>::Serialize(xmlNode, bLoading, bLoadEmptyTracks);
}

void CLookAtTrack::SerializeKey(SLookAtKey& key, XmlNodeRef& keyNode, bool bLoading)
{
	if (bLoading)
	{
		const char* pSelection = keyNode->getAttr("node");
		f32 smoothTime;

		if (!keyNode->getAttr("smoothTime", smoothTime))
		{
			smoothTime = 0.0f;
		}

		const char* lookPose = keyNode->getAttr("lookPose");

		if (lookPose)
		{
			cry_strcpy(key.m_lookPose, lookPose);
		}

		cry_strcpy(key.m_selection, pSelection);
		key.m_smoothTime = smoothTime;
	}
	else
	{
		keyNode->setAttr("node", key.m_selection);
		keyNode->setAttr("smoothTime", key.m_smoothTime);
		keyNode->setAttr("lookPose", key.m_lookPose);
	}
}
