// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "TimeRangesTrack.h"

void CTimeRangesTrack::SerializeKey(STimeRangeKey& key, XmlNodeRef& keyNode, bool bLoading)
{
	if (bLoading)
	{
		key.m_speed = 1;
		keyNode->getAttr("speed", key.m_speed);
		keyNode->getAttr("loop", key.m_bLoop);
		keyNode->getAttr("start", key.m_startTime);
		keyNode->getAttr("end", key.m_endTime);
	}
	else
	{
		if (key.m_speed != 1)
		{
			keyNode->setAttr("speed", key.m_speed);
		}

		if (key.m_bLoop)
		{
			keyNode->setAttr("loop", key.m_bLoop);
		}

		if (key.m_startTime != 0.0f)
		{
			keyNode->setAttr("start", key.m_startTime);
		}

		if (key.m_endTime != 0.0f)
		{
			keyNode->setAttr("end", key.m_endTime);
		}
	}
}

int CTimeRangesTrack::GetActiveKeyIndexForTime(const SAnimTime time)
{
	const unsigned int numKeys = m_keys.size();

	if (numKeys == 0 || m_keys[0].m_time > time)
	{
		return -1;
	}

	int lastFound = 0;

	for (unsigned int i = 0; i < numKeys; ++i)
	{
		STimeRangeKey& key = m_keys[i];

		if (key.m_time > time)
		{
			break;
		}
		else if (key.m_time <= time)
		{
			lastFound = i;
		}
	}

	return lastFound;
}
