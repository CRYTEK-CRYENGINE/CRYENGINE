// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "AnimSplineTrack.h"

#include <CryMath/ISplines.h>

CAnimSplineTrack::CAnimSplineTrack(const CAnimParamType& paramType)
	: m_paramType(paramType)
	, m_defaultValue(ZERO)
{
}

void CAnimSplineTrack::SetValue(SAnimTime time, const TMovieSystemValue& value)
{
	S2DBezierKey key;
	key.m_time = time;
	key.m_controlPoint.m_value = stl::get<float>(value);
	SetKeyAtTime(time, &key);
}

TMovieSystemValue CAnimSplineTrack::GetValue(SAnimTime time) const
{
	if (GetNumKeys() == 0)
	{
		return GetDefaultValue();
	}
	else
	{
		return SampleCurve(time);
	}
}

TMovieSystemValue CAnimSplineTrack::GetDefaultValue() const
{
	return TMovieSystemValue(m_defaultValue.y);
}

void CAnimSplineTrack::SetDefaultValue(const TMovieSystemValue& value)
{
	float floatValue = stl::get<float>(value);
	m_defaultValue.y = floatValue;
}

bool CAnimSplineTrack::Serialize(XmlNodeRef& xmlNode, bool bLoading, bool bLoadEmptyTracks)
{
	if (!TAnimTrack<S2DBezierKey>::Serialize(xmlNode, bLoading, bLoadEmptyTracks))
	{
		return false;
	}

	if (bLoading)
	{
		xmlNode->getAttr("defaultValue", m_defaultValue);
	}
	else
	{
		xmlNode->setAttr("defaultValue", m_defaultValue);
	}

	return true;
}

void CAnimSplineTrack::SerializeKey(S2DBezierKey& key, XmlNodeRef& keyNode, bool bLoading)
{
	if (bLoading)
	{
		// For backwards compatibility we first try to read Vec2 (time, value), then only the float value directly (new format)
		Vec2 vecValue;
		if (!keyNode->getAttr("value", vecValue))
		{
			if (!keyNode->getAttr("value", key.m_controlPoint.m_value))
			{
				CryLog("[CRYMOVIE:TAnimSplineTrack<Vec2>::Serialize]Ill formed legacy track:missing value information.");
				return;
			}
		}
		else
		{
			key.m_controlPoint.m_value = vecValue.y;
		}

		int flags = 0;
		keyNode->getAttr("flags", flags);
		spline::Flags spFlags(flags);
		key.m_controlPoint.m_inTangentType = spFlags.inTangentType;
		key.m_controlPoint.m_outTangentType = spFlags.outTangentType;
		key.m_controlPoint.m_bBreakTangents = !spFlags.tangentsUnified;

		// In-/Out-tangent
		if (!keyNode->getAttr("ds", key.m_controlPoint.m_inTangent))
		{
			CryLog("[CRYMOVIE:TAnimSplineTrack<Vec2>::Serialize]Ill formed legacy track:missing ds spline information.");
			return;
		}

		if (!keyNode->getAttr("dd", key.m_controlPoint.m_outTangent))
		{
			CryLog("[CRYMOVIE:TAnimSplineTrack<Vec2>::Serialize]Ill formed legacy track:dd spline information.");
			return;
		}

		// For historical reasons the incoming tangent is inverted on disk
		key.m_controlPoint.m_inTangent = -key.m_controlPoint.m_inTangent;
	}
	else
	{
		keyNode->setAttr("value", key.m_controlPoint.m_value);

		spline::Flags spFlags;
		spFlags.inTangentType = key.m_controlPoint.m_inTangentType;
		spFlags.outTangentType = key.m_controlPoint.m_outTangentType;
		spFlags.tangentsUnified = !key.m_controlPoint.m_bBreakTangents;

		int flags = spFlags;
		if (flags != 0)
		{
			keyNode->setAttr("flags", flags);
		}

		// For historical reasons the incoming tangent is inverted on disk
		const Vec2 inTangent = -key.m_controlPoint.m_inTangent;

		keyNode->setAttr("ds", inTangent);
		keyNode->setAttr("dd", key.m_controlPoint.m_outTangent);
	}
}

float CAnimSplineTrack::SampleCurve(SAnimTime time) const
{
	if (time <= m_keys.front().m_time)
	{
		return m_keys.front().m_controlPoint.m_value;
	}
	else if (time >= m_keys.back().m_time)
	{
		return m_keys.back().m_controlPoint.m_value;
	}

	const Keys::const_iterator iter = std::upper_bound(m_keys.begin(), m_keys.end(), time, SCompKeyTime());
	const Keys::const_iterator startIter = iter - 1;

	if (startIter->m_controlPoint.m_outTangentType == spline::ETangentType::Step)
	{
		return iter->m_controlPoint.m_value;
	}

	if (iter->m_controlPoint.m_inTangentType == spline::ETangentType::Step)
	{
		return startIter->m_controlPoint.m_value;
	}

	const SAnimTime deltaTime = iter->m_time - startIter->m_time;

	if (deltaTime == SAnimTime(0))
	{
		return startIter->m_controlPoint.m_value;
	}

	const float timeInSegment = (time - startIter->m_time).ToFloat();
	const float factor = Bezier::InterpolationFactorFromX(timeInSegment, deltaTime.ToFloat(), startIter->m_controlPoint, iter->m_controlPoint);
	return Bezier::EvaluateY(factor, startIter->m_controlPoint, iter->m_controlPoint);
}
