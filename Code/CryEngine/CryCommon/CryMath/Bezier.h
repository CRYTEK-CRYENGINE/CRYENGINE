// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#ifndef __BEZIER_H__
#define __BEZIER_H__

#include <CryMovie/AnimTime.h>
#include <CryMath/ISplines.h>
#include <CrySerialization/IArchive.h>
#include <CrySerialization/Math.h>
#include <CrySerialization/STL.h>
#include <CrySerialization/Enum.h>

struct SBezierControlPoint
{
	typedef spline::ETangentType ETangentType;

	SBezierControlPoint()
		: m_value(0.0f)
		, m_inTangent(ZERO)
		, m_outTangent(ZERO)
		//	, m_inTangentType(ETangentType::Auto)
		//	, m_outTangentType(ETangentType::Auto)
		, m_inTangentType(ETangentType::Smooth)
		, m_outTangentType(ETangentType::Smooth)
		, m_bBreakTangents(false)
	{
	}

	void Serialize(Serialization::IArchive& ar)
	{
		ar(m_value, "value", "Value");

		if (ar.isOutput())
		{
			bool breakTangents = m_bBreakTangents;
			ar(breakTangents, "breakTangents", "Break Tangents");
		}
		else
		{
			bool breakTangents = false;
			ar(breakTangents, "breakTangents", "Break Tangents");
			m_bBreakTangents = breakTangents;
		}

		if (ar.isOutput())
		{
			ETangentType inTangentType = m_inTangentType;
			ar(inTangentType, "inTangentType", "Incoming tangent type");
		}
		else
		{
			// ETangentType inTangentType = ETangentType::Auto;
			ETangentType inTangentType = ETangentType::Smooth;
			ar(inTangentType, "inTangentType", "Incoming tangent type");
			m_inTangentType = inTangentType;
		}

		ar(m_inTangent, "inTangent", (m_inTangentType == ETangentType::Custom) ? "Incoming Tangent" : NULL);

		if (ar.isOutput())
		{
			ETangentType outTangentType = m_outTangentType;
			ar(outTangentType, "outTangentType", "Outgoing tangent type");
		}
		else
		{
			// ETangentType outTangentType = ETangentType::Auto;
			ETangentType outTangentType = ETangentType::Smooth;
			ar(outTangentType, "outTangentType", "Outgoing tangent type");
			m_outTangentType = outTangentType;
		}

		ar(m_outTangent, "outTangent", (m_outTangentType == ETangentType::Custom) ? "Outgoing Tangent" : NULL);
	}

	float m_value;

	//! For 1D Bezier only the Y component is used.
	Vec2         m_inTangent;
	Vec2         m_outTangent;

	ETangentType m_inTangentType;
	ETangentType m_outTangentType;
	bool         m_bBreakTangents;
};

struct SBezierKey
{
	SBezierKey() : m_time(0) {}

	void Serialize(Serialization::IArchive& ar)
	{
		ar(m_time, "time", "Time");
		ar(m_controlPoint, "controlPoint", "Control Point");
	}

	SAnimTime           m_time;
	SBezierControlPoint m_controlPoint;
};

//! Serialize all keys in spline as one string in order to reduce file size.
bool Serialize(Serialization::IArchive& ar, std::vector<SBezierKey>& value, const char* name, const char* label);

namespace Bezier
{
inline float Evaluate(float t, float p0, float p1, float p2, float p3)
{
	const float a = 1 - t;
	const float aSq = a * a;
	const float tSq = t * t;
	return (aSq * a * p0) + (3.0f * aSq * t * p1) + (3.0f * a * tSq * p2) + (tSq * t * p3);
}

inline float EvaluateDeriv(float t, float p0, float p1, float p2, float p3)
{
	const float a = 1 - t;
	const float ta = t * a;
	const float aSq = a * a;
	const float tSq = t * t;
	return 3.0f * ((-p2 * tSq) + (p3 * tSq) - (p0 * aSq) + (p1 * aSq) + 2.0f * ((-p1 * ta) + (p2 * ta)));
}

inline float EvaluateX(const float t, const float duration, const SBezierControlPoint& start, const SBezierControlPoint& end)
{
	const float p0 = 0.0f;
	const float p1 = p0 + start.m_outTangent.x;
	const float p3 = duration;
	const float p2 = p3 + end.m_inTangent.x;
	return Evaluate(t, p0, p1, p2, p3);
}

inline float EvaluateY(const float t, const SBezierControlPoint& start, const SBezierControlPoint& end)
{
	const float p0 = start.m_value;
	const float p1 = p0 + start.m_outTangent.y;
	const float p3 = end.m_value;
	const float p2 = p3 + end.m_inTangent.y;
	return Evaluate(t, p0, p1, p2, p3);
}

//! Duration = (time at end key) - (time at start key)
inline float EvaluateDerivX(const float t, const float duration, const SBezierControlPoint& start, const SBezierControlPoint& end)
{
	const float p0 = 0.0f;
	const float p1 = p0 + start.m_outTangent.x;
	const float p3 = duration;
	const float p2 = p3 + end.m_inTangent.x;
	return EvaluateDeriv(t, p0, p1, p2, p3);
}

inline float EvaluateDerivY(const float t, const SBezierControlPoint& start, const SBezierControlPoint& end)
{
	const float p0 = start.m_value;
	const float p1 = p0 + start.m_outTangent.y;
	const float p3 = end.m_value;
	const float p2 = p3 + end.m_inTangent.y;
	return EvaluateDeriv(t, p0, p1, p2, p3);
}

//! Find interpolation factor where 2D bezier curve has the given x value. This only works for curves where x is monotonically increasing.
//! The passed x must be in the range [0, duration]. Uses the Newton-Raphson root finding method, usually takes 2 or 3 iterations.
//! Note: This is for "1D". 2D bezier curves are used in TrackView. The curves are restricted by the curve editor to be monotonically increasing.
inline float InterpolationFactorFromX(const float x, const float duration, const SBezierControlPoint& start, const SBezierControlPoint& end)
{
	float t = (x / duration);

	const float epsilon = 0.00001f;
	const uint maxSteps = 10;

	for (uint i = 0; i < maxSteps; ++i)
	{
		const float currentX = EvaluateX(t, duration, start, end) - x;
		if (fabs(currentX) <= epsilon)
		{
			break;
		}

		const float currentXDeriv = EvaluateDerivX(t, duration, start, end);
		t -= currentX / currentXDeriv;
	}

	return t;
}

SBezierKey ApplyInTangent(const SBezierKey& key, const SBezierKey& leftKey, const SBezierKey* pRightKey);

SBezierKey ApplyOutTangent(const SBezierKey& key, const SBezierKey* pLeftKey, const SBezierKey& rightKey);
}

#endif
