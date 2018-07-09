// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "BoolTrack.h"

CBoolTrack::CBoolTrack(const CAnimParamType& paramType)
	: m_bDefaultValue(true)
	, m_paramType(paramType)
{
}

TMovieSystemValue CBoolTrack::GetValue(SAnimTime time) const
{
	bool value = m_bDefaultValue;
	int nkeys = m_keys.size();

	if (nkeys < 1)
	{
		return TMovieSystemValue(m_bDefaultValue);
	}

	int key = 0;

	while ((key < nkeys) && (time >= m_keys[key].m_time))
	{
		key++;
	}

	if (m_bDefaultValue)
	{
		value = !(key & 1); // True if even key.
	}
	else
	{
		value = (key & 1);  // False if even key.
	}

	return TMovieSystemValue(value);
}

TMovieSystemValue CBoolTrack::GetDefaultValue() const
{
	return TMovieSystemValue(m_bDefaultValue);
}

void CBoolTrack::SetDefaultValue(const TMovieSystemValue& defaultValue)
{
	m_bDefaultValue = stl::get<bool>(defaultValue);
}
