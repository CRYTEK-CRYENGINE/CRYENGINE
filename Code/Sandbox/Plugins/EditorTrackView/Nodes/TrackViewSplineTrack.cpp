// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

// CryEngine Source File.
// Copyright (C), Crytek, 1999-2015.

#include "StdAfx.h"
#include "TrackViewSplineTrack.h"
#include "TrackViewSequence.h"
#include "TrackViewUndo.h"

#include <CryMath/Bezier.h>
#include <CryMath/Bezier_impl.h>

CTrackViewKeyHandle CTrackViewSplineTrack::CreateKey(const SAnimTime time)
{
	const float value = stl::get<float>(GetValue(time));
	CTrackViewKeyHandle handle = CTrackViewTrack::CreateKey(time);

	S2DBezierKey bezierKey;
	handle.GetKey(&bezierKey);
	bezierKey.m_controlPoint.m_value = value;
	handle.SetKey(&bezierKey);

	GetSequence()->OnNodeChanged(this, ITrackViewSequenceListener::eNodeChangeType_KeysChanged);

	return handle;
}

void CTrackViewSplineTrack::SetKey(uint keyIndex, const STrackKey* pKey)
{
	CTrackViewTrack::SetKey(keyIndex, pKey);

	if (keyIndex > 0)
	{
		UpdateKeyTangents(keyIndex - 1);
	}

	UpdateKeyTangents(keyIndex);

	if ((keyIndex + 1) < GetKeyCount())
	{
		UpdateKeyTangents(keyIndex + 1);
	}
}

void CTrackViewSplineTrack::SetValue(const SAnimTime time, const TMovieSystemValue& value)
{
	CTrackViewTrack::SetValue(time, value);

	CTrackViewKeyHandle handle = GetKeyByTime(time);
	uint keyIndex = handle.GetIndex();
	if (keyIndex > 0)
	{
		UpdateKeyTangents(keyIndex - 1);
	}

	UpdateKeyTangents(keyIndex);

	if ((keyIndex + 1) < GetKeyCount())
	{
		UpdateKeyTangents(keyIndex + 1);
	}
}

void CTrackViewSplineTrack::OffsetKeys(const TMovieSystemValue& offset)
{
	assert(CUndo::IsRecording() || CUndo::IsSuspended());
	assert(offset.index() == eTDT_Float);
	CUndo::Record(new CUndoTrackObject(this, GetSequence() != nullptr));

	const uint keyCount = GetKeyCount();
	for (uint keyIndex = 0; keyIndex < keyCount; ++keyIndex)
	{
		CTrackViewKeyHandle handle = GetKey(keyIndex);
		S2DBezierKey key;
		handle.GetKey(&key);
		key.m_controlPoint.m_value += stl::get<float>(offset);
		handle.SetKey(&key);
	}
}

void CTrackViewSplineTrack::UpdateKeyTangents(uint keyIndex)
{
	S2DBezierKey key;
	GetKey(keyIndex, &key);

	const bool bHasLeftKey = keyIndex > 0;
	S2DBezierKey leftKey;
	if (bHasLeftKey)
	{
		GetKey(keyIndex - 1, &leftKey);
	}

	const bool bHasRightKey = (keyIndex + 1) < GetKeyCount();
	S2DBezierKey rightKey;
	if (bHasRightKey)
	{
		GetKey(keyIndex + 1, &rightKey);
	}

	const SAnimTime leftTime = bHasLeftKey ? leftKey.m_time : key.m_time;
	const SAnimTime rightTime = bHasRightKey ? rightKey.m_time : key.m_time;

	// Rebase to [0, rightTime - leftTime] to increase float precision
	const float floatTime = (key.m_time - leftTime).ToFloat();
	const float floatLeftTime = 0.0f;
	const float floatRightTime = (rightTime - leftTime).ToFloat();

	key.m_controlPoint = Bezier::CalculateInTangent(floatTime, key.m_controlPoint,
	                                                floatLeftTime, bHasLeftKey ? &leftKey.m_controlPoint : nullptr,
	                                                floatRightTime, bHasRightKey ? &rightKey.m_controlPoint : nullptr);

	key.m_controlPoint = Bezier::CalculateOutTangent(floatTime, key.m_controlPoint,
	                                                 floatLeftTime, bHasLeftKey ? &leftKey.m_controlPoint : nullptr,
	                                                 floatRightTime, bHasRightKey ? &rightKey.m_controlPoint : nullptr);

	CTrackViewTrack::SetKey(keyIndex, &key);
}

string CTrackViewSplineTrack::GetKeyDescription(const uint index) const
{
	S2DBezierKey key;
	GetKey(index, &key);

	string description;
	description.Format("%.3f", key.m_controlPoint.m_value);
	return description;
}

