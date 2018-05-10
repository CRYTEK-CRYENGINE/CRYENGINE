// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include <CryMath/Cry_Math.h>
#include <CryMath/Cry_Color.h>
#include <CryMath/Bezier.h>
#include <CryMovie/AnimTime.h>
#include <CryMath/ISplines.h>

#include <CrySerialization/Color.h>
#include "Serialization.h"

#include <QObject>

struct SCurveEditorKey : SBezierKey
{
	SCurveEditorKey()
		: m_bSelected(false)
		, m_bModified(false)
		, m_bAdded(false)
		, m_bDeleted(false)
	{
	}

	bool m_bSelected : 1;
	bool m_bModified : 1;
	bool m_bAdded    : 1;
	bool m_bDeleted  : 1;
};

struct SCurveEditorCurve : ISplineEvaluator
{
	SCurveEditorCurve()
		: m_bModified(false)
		, m_defaultValue(0.0f)
		, m_bBezier2D(true)
		, m_color(255, 255, 255)
		, m_externData(nullptr)
	{}

	bool Serialize(IArchive& ar, const char* name = "", const char* label = "")
	{
		ar(m_keys, "keys");
		ar(m_defaultValue, "defaultValue");
		ar(m_color, "color");
		ar(m_bBezier2D, "2D");

		return true;
	}

	// ISplineEvaluator implementation
	virtual int GetNumDimensions() const override
	{
		return m_bBezier2D ? 2 : 1;
	}
	virtual int GetKeyCount() const override
	{
		return m_keys.size();
	}
	virtual void GetKey(int i, KeyType& keyOut) const override
	{
		const auto thisKey = ApplyTangents(m_keys[i], true);
		keyOut.time = thisKey.m_time;
		keyOut.flags.inTangentType = thisKey.m_controlPoint.m_inTangentType;
		keyOut.flags.outTangentType = thisKey.m_controlPoint.m_outTangentType;

		if (m_bBezier2D)
		{
			keyOut.value[0] = keyOut.time;
			keyOut.value[1] = thisKey.m_controlPoint.m_value;
			*(Vec2*)keyOut.ds = -3.0f * thisKey.m_controlPoint.m_inTangent;
			*(Vec2*)keyOut.dd = 3.0f * thisKey.m_controlPoint.m_outTangent;
		}
		else
		{
			const int numKeys = m_keys.size();
			const auto& prevKey = m_keys[max(0, i - 1)];
			const auto& nextKey = m_keys[min(i + 1, numKeys - 1)];
			const float inSegmentLength = thisKey.m_time - prevKey.m_time;
			const float outSegmentLength = nextKey.m_time - thisKey.m_time;

			const Vec2 inTangent = thisKey.m_controlPoint.m_inTangent;
			const Vec2 outTangent = thisKey.m_controlPoint.m_outTangent;
			const float inSlope = (inTangent.y / min(-FLT_EPSILON, inTangent.x)) * inSegmentLength;
			const float outSlope = (outTangent.y / max(FLT_EPSILON, outTangent.x)) * outSegmentLength;

			keyOut.value[0] = thisKey.m_controlPoint.m_value;
			keyOut.ds[0] = inSlope;
			keyOut.dd[0] = outSlope;
		}
	}
	virtual void FromSpline(const ISplineEvaluator& spline) override
	{
		const float oneThird = 1.0f / 3.0f;

		switch (spline.GetNumDimensions())
		{
		case 1:
			m_bBezier2D = false;
			break;
		case 2:
			m_bBezier2D = true;
			break;
		default:
			m_keys.clear();
			return;
		}

		m_keys.resize(spline.GetKeyCount());
		const int numKeys = m_keys.size();
		for (int i = 0; i < numKeys; ++i)
		{
			auto& key = m_keys[i];
			KeyType thisKey;
			spline.GetKey(i, thisKey);

			key.m_controlPoint.m_inTangentType = thisKey.flags.inTangentType;
			key.m_controlPoint.m_outTangentType = thisKey.flags.outTangentType;

			if (m_bBezier2D)
			{
				key.m_time = thisKey.value[0];
				key.m_controlPoint.m_value = thisKey.value[1];
				key.m_controlPoint.m_inTangent = -oneThird * *(Vec2*)thisKey.ds;
				key.m_controlPoint.m_outTangent = oneThird * *(Vec2*)thisKey.dd;
			}
			else
			{
				KeyType prevKey;
				KeyType nextKey;
				spline.GetKey(max(0, i - 1), prevKey);
				spline.GetKey(min(i + 1, numKeys - 1), nextKey);

				const float inSegmentLength = thisKey.time - prevKey.time;
				const float outSegmentLength = nextKey.time - thisKey.time;
				const float inSlope = -thisKey.ds[0] * oneThird;
				const float outSlope = thisKey.dd[0] * oneThird;

				key.m_time = thisKey.time;
				key.m_controlPoint.m_value = thisKey.value[0];
				key.m_controlPoint.m_inTangent = Vec2(-oneThird * inSegmentLength, inSlope);
				key.m_controlPoint.m_outTangent = Vec2(oneThird * outSegmentLength, outSlope);
			}
		}
	}

	virtual void Interpolate(float time, ValueType& value) override
	{
		assert(!"SCurveEditorCurve::Interpolate not implemented");
	}

	// Compute working tangents, based on flags.
	// For 1D curves, need to compute tangents for x so that the cubic 2D Bezier does a linear interpolation in
	// that dimension, because we actually want to draw a cubic 1D Bezier curve
	SCurveEditorKey ApplyOutTangent(const SCurveEditorKey& key, const bool strict, const SCurveEditorKey* pos = nullptr) const
	{
		if (!pos)
			pos = &key;
		SCurveEditorKey ret = key;
		static_cast<SBezierKey&>(ret) = Bezier::ApplyOutTangent(key, (pos > &m_keys.front() ? pos - 1 : nullptr), pos[1]);

		if (strict && !m_bBezier2D)
		{
			const Vec2 outTangent = ret.m_controlPoint.m_outTangent;
			const float outSegmentLength = pos[1].m_time - pos->m_time;
			const float outSlope = (outTangent.y / max(FLT_EPSILON, outTangent.x)) * outSegmentLength;
			ret.m_controlPoint.m_outTangent.x = outSegmentLength / 3.0f;
			ret.m_controlPoint.m_outTangent.y = outSlope / 3.0f;
		}

		return ret;
	}
	SCurveEditorKey ApplyInTangent(const SCurveEditorKey& key, const bool strict, const SCurveEditorKey* pos = nullptr) const
	{
		if (!pos)
			pos = &key;
		SCurveEditorKey ret = key;
		static_cast<SBezierKey&>(ret) = Bezier::ApplyInTangent(key, pos[-1], (pos < &m_keys.back() ? pos + 1 : nullptr));

		if (strict && !m_bBezier2D)
		{
			const Vec2 inTangent = ret.m_controlPoint.m_inTangent;
			const float inSegmentLength = pos->m_time - pos[-1].m_time;
			const float inSlope = (inTangent.y / min(-FLT_EPSILON, inTangent.x)) * inSegmentLength;
			ret.m_controlPoint.m_inTangent.x = -inSegmentLength / 3.0f;
			ret.m_controlPoint.m_inTangent.y = -inSlope / 3.0f;
		}

		return ret;
	}
	SCurveEditorKey ApplyTangents(const SCurveEditorKey& key, const bool strict, const SCurveEditorKey* pos = nullptr) const
	{
		SCurveEditorKey ret = key;
		ret.m_controlPoint.m_inTangent = ApplyInTangent(key, strict, pos).m_controlPoint.m_inTangent;
		ret.m_controlPoint.m_outTangent = ApplyOutTangent(key, strict, pos).m_controlPoint.m_outTangent;
		return ret;
	}

	bool IsSelected() const
	{
		for (const auto& key : m_keys)
			if (key.m_bSelected)
				return true;
		return false;
	}

	bool                         m_bBezier2D; // 2D Bezier curves are used for better curve control; if false, the editor will enforce that the resulting curve is always 1D.
	bool                         m_bModified;
	float                        m_defaultValue;
	ColorB                       m_color;
	void*                        m_externData;

	DynArray<char>               userSideLoad;

	std::vector<SCurveEditorKey> m_keys;
};

typedef std::vector<SCurveEditorCurve> TCurveEditorCurves;

class CCurveEditor;

struct EDITOR_COMMON_API SCurveEditorContent : public QObject
{
	Q_OBJECT

public:
	void Serialize(IArchive& ar)
	{
		ar(m_curves, "curves");
	}

Q_SIGNALS:
	void SignalChanging(CCurveEditor& editor);
	void SignalAboutToBeChanged(CCurveEditor& editor);
	void SignalChanged(CCurveEditor& editor);

public:
	TCurveEditorCurves m_curves;
};

