// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "GotoTrack.h"

CGotoTrack::CGotoTrack()
{
	m_flags = 0;
	m_defaultValue = -1.0f;
}

TMovieSystemValue CGotoTrack::GetValue(SAnimTime time) const
{
	size_t nTotalKeys(m_keys.size());

	if (nTotalKeys < 1)
	{
		return TMovieSystemValue(m_defaultValue);
	}

	size_t nKey(0);

	for (nKey = 0; nKey < nTotalKeys; ++nKey)
	{
		if (time >= m_keys[nKey].m_time)
		{
			return TMovieSystemValue(m_keys[nKey].m_value);
		}
		else
		{
			break;
		}
	}

	return TMovieSystemValue(m_defaultValue);
}

void CGotoTrack::SetValue(SAnimTime time, const TMovieSystemValue& value)
{
	SDiscreteFloatKey oKey;
	oKey.m_value = stl::get<float>(value);
	SetKeyAtTime(time, &oKey);
}

void CGotoTrack::SetDefaultValue(const TMovieSystemValue& value)
{
	m_defaultValue = stl::get<float>(value);
}

void CGotoTrack::SerializeKey(SDiscreteFloatKey& key, XmlNodeRef& keyNode, bool bLoading)
{
	if (bLoading)
	{
		keyNode->getAttr("value", key.m_value);
	}
	else
	{
		keyNode->setAttr("value", key.m_value);
	}
}
