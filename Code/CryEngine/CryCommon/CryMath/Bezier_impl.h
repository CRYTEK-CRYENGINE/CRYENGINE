// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#ifndef __BEZIER_IMPL_H__
#define __BEZIER_IMPL_H__

#include <CryMath/Bezier.h>

bool Serialize(Serialization::IArchive& ar, std::vector<SBezierKey>& value, const char* name, const char* label)
{
	if (ar.isEdit() || ar.caps(ar.BINARY))
	{
		yasli::ContainerSTL<std::vector<SBezierKey>, SBezierKey> ser(&value);
		return ar(static_cast<Serialization::IContainer&>(ser), name, label);
	}
	else
	{
		string str;
		if (ar.isOutput())
		{
			string keystr;
			const size_t vecSize = value.size();
			for (size_t i = 0; i < vecSize; ++i)
			{
				const SBezierKey& key = value[i];
				const SBezierControlPoint& cp = key.m_controlPoint;

				keystr.Format("%d:%g:%g:%g:%g:%g:%d:%d:%d,",
				              key.m_time.GetTicks(), cp.m_value,
				              cp.m_inTangent.x, cp.m_inTangent.y,
				              cp.m_outTangent.x, cp.m_outTangent.y,
				              int(cp.m_inTangentType), int(cp.m_outTangentType), int(cp.m_bBreakTangents));

				str += keystr;
			}
		}
		if (!ar(str, name, label))
			return false;
		if (ar.isInput())
		{
			int curPos = 0;
			uint32 nKeys = 0;
			string key = str.Tokenize(",", curPos);
			while (!key.empty())
			{
				++nKeys;
				key = str.Tokenize(",", curPos);
			}
			;
			value.resize(nKeys);

			nKeys = 0;
			curPos = 0;
			key = str.Tokenize(",", curPos);
			while (!key.empty())
			{
				int32 keyTime;
				float keyValue;
				Vec2 keyInTan;
				Vec2 keyOutTan;
				int keyInTanType, keyOutTanType, keyBreakTan;

				int res = sscanf(key, "%d:%g:%g:%g:%g:%g:%d:%d:%d",
				                 &keyTime, &keyValue,
				                 &keyInTan.x, &keyInTan.y,
				                 &keyOutTan.x, &keyOutTan.y,
				                 &keyInTanType, &keyOutTanType, &keyBreakTan);
				if (res != 9)
				{
					continue;
				}

				SBezierKey& bezierKey = value[nKeys];
				bezierKey.m_time = SAnimTime(keyTime);

				SBezierControlPoint& cp = bezierKey.m_controlPoint;
				cp.m_value = keyValue;
				cp.m_inTangent = keyInTan;
				cp.m_outTangent = keyOutTan;
				cp.m_inTangentType = SBezierControlPoint::ETangentType(keyInTanType);
				cp.m_outTangentType = SBezierControlPoint::ETangentType(keyOutTanType);
				cp.m_bBreakTangents = (keyBreakTan != 0);

				++nKeys;
				key = str.Tokenize(",", curPos);
			}
			;

		}
	}
	return true;
}

namespace Bezier
{

SBezierControlPoint CalculateInTangent(
  float time, const SBezierControlPoint& point,
  float leftTime, const SBezierControlPoint* pLeftPoint,
  float rightTime, const SBezierControlPoint* pRightPoint)
{
	SBezierControlPoint newPoint = point;

	// In tangent X can never be positive
	newPoint.m_inTangent.x = std::min(point.m_inTangent.x, 0.0f);

	if (pLeftPoint)
	{
		switch (point.m_inTangentType)
		{
		case SBezierControlPoint::ETangentType::Custom:
			{
				// Need to clamp tangent if it is reaching over last point
				const float deltaTime = time - leftTime;
				if (deltaTime < -newPoint.m_inTangent.x)
				{
					if (newPoint.m_inTangent.x == 0)
					{
						newPoint.m_inTangent = Vec2(ZERO);
					}
					else
					{
						float scaleFactor = deltaTime / -newPoint.m_inTangent.x;
						newPoint.m_inTangent.x = -deltaTime;
						newPoint.m_inTangent.y *= scaleFactor;
					}
				}
			}
			break;
		case SBezierControlPoint::ETangentType::Zero:
		// Fall through. Zero for y is same as Auto, x is set to 0.0f
		// case SBezierControlPoint::ETangentType::Auto:
		case SBezierControlPoint::ETangentType::Smooth:
			{
				const SBezierControlPoint& rightPoint = pRightPoint ? *pRightPoint : point;
				const float deltaTime = (pRightPoint ? rightTime : time) - leftTime;
				if (deltaTime > 0.0f)
				{
					const float ratio = (time - leftTime) / deltaTime;
					const float deltaValue = rightPoint.m_value - pLeftPoint->m_value;
					const bool bIsZeroTangent = (point.m_inTangentType == SBezierControlPoint::ETangentType::Zero);
					newPoint.m_inTangent = Vec2(-(deltaTime * ratio) / 3.0f, bIsZeroTangent ? 0.0f : -(deltaValue * ratio) / 3.0f);
				}
				else
				{
					newPoint.m_inTangent = Vec2(ZERO);
				}
			}
			break;
		case SBezierControlPoint::ETangentType::Linear:
			newPoint.m_inTangent = Vec2((leftTime - time) / 3.0f,
			                            (pLeftPoint->m_value - point.m_value) / 3.0f);
			break;
		}
	}

	return newPoint;
}

SBezierControlPoint CalculateOutTangent(
  float time, const SBezierControlPoint& point,
  float leftTime, const SBezierControlPoint* pLeftPoint,
  float rightTime, const SBezierControlPoint* pRightPoint)
{
	SBezierControlPoint newPoint = point;

	// Out tangent X can never be negative
	newPoint.m_outTangent.x = std::max(point.m_outTangent.x, 0.0f);

	if (pRightPoint)
	{
		switch (point.m_outTangentType)
		{
		case SBezierControlPoint::ETangentType::Custom:
			{
				// Need to clamp tangent if it is reaching over next point
				const float deltaTime = rightTime - time;
				if (deltaTime < newPoint.m_outTangent.x)
				{
					if (newPoint.m_outTangent.x == 0)
					{
						newPoint.m_outTangent = Vec2(ZERO);
					}
					else
					{
						float scaleFactor = deltaTime / newPoint.m_outTangent.x;
						newPoint.m_outTangent.x = deltaTime;
						newPoint.m_outTangent.y *= scaleFactor;
					}
				}
			}
			break;
		case SBezierControlPoint::ETangentType::Zero:
		// Fall through. Zero for y is same as Auto, x is set to 0.0f
		// case SBezierControlPoint::ETangentType::Auto:
		case SBezierControlPoint::ETangentType::Smooth:
			{
				const SBezierControlPoint& leftPoint = pLeftPoint ? *pLeftPoint : point;
				const float deltaTime = rightTime - (pLeftPoint ? leftTime : time);
				if (deltaTime > 0.0f)
				{
					const float ratio = (rightTime - time) / deltaTime;
					const float deltaValue = pRightPoint->m_value - leftPoint.m_value;
					const bool bIsZeroTangent = (point.m_outTangentType == SBezierControlPoint::ETangentType::Zero);
					newPoint.m_outTangent = Vec2((deltaTime * ratio) / 3.0f, bIsZeroTangent ? 0.0f : (deltaValue * ratio) / 3.0f);
				}
				else
				{
					newPoint.m_outTangent = Vec2(ZERO);
				}
			}
			break;
		case SBezierControlPoint::ETangentType::Linear:
			newPoint.m_outTangent = Vec2((rightTime - time) / 3.0f,
			                             (pRightPoint->m_value - point.m_value) / 3.0f);
			break;
		}
	}

	return newPoint;
}

SBezierKey ApplyInTangent(const SBezierKey& key, const SBezierKey& leftKey, const SBezierKey* pRightKey)
{
	SBezierKey newKey = key;

	if (leftKey.m_controlPoint.m_outTangentType == SBezierControlPoint::ETangentType::Step)
	{
		newKey.m_controlPoint.m_inTangent = Vec2(0.0f, 0.0f);
		return newKey;
	}
	else if (key.m_controlPoint.m_inTangentType != SBezierControlPoint::ETangentType::Step)
	{
		const SAnimTime leftTime = leftKey.m_time;
		const SAnimTime rightTime = pRightKey ? pRightKey->m_time : key.m_time;

		// Rebase to [0, rightTime - leftTime] to increase float precision
		const float floatTime = (key.m_time - leftTime).ToFloat();
		const float floatLeftTime = 0.0f;
		const float floatRightTime = (rightTime - leftTime).ToFloat();

		newKey.m_controlPoint = Bezier::CalculateInTangent(floatTime, key.m_controlPoint,
		                                                   floatLeftTime, &leftKey.m_controlPoint,
		                                                   floatRightTime, pRightKey ? &pRightKey->m_controlPoint : NULL);
	}
	else
	{
		newKey.m_controlPoint.m_inTangent = Vec2(0.0f, 0.0f);
		newKey.m_controlPoint.m_value = leftKey.m_controlPoint.m_value;
	}

	return newKey;
}

SBezierKey ApplyOutTangent(const SBezierKey& key, const SBezierKey* pLeftKey, const SBezierKey& rightKey)
{
	SBezierKey newKey = key;

	if (rightKey.m_controlPoint.m_inTangentType == SBezierControlPoint::ETangentType::Step
	    && key.m_controlPoint.m_outTangentType != SBezierControlPoint::ETangentType::Step)
	{
		newKey.m_controlPoint.m_outTangent = Vec2(0.0f, 0.0f);
	}
	else if (key.m_controlPoint.m_outTangentType != SBezierControlPoint::ETangentType::Step)
	{
		const SAnimTime leftTime = pLeftKey ? pLeftKey->m_time : key.m_time;
		const SAnimTime rightTime = rightKey.m_time;

		// Rebase to [0, rightTime - leftTime] to increase float precision
		const float floatTime = (key.m_time - leftTime).ToFloat();
		const float floatLeftTime = 0.0f;
		const float floatRightTime = (rightTime - leftTime).ToFloat();

		newKey.m_controlPoint = Bezier::CalculateOutTangent(floatTime, key.m_controlPoint,
		                                                    floatLeftTime, pLeftKey ? &pLeftKey->m_controlPoint : NULL,
		                                                    floatRightTime, &rightKey.m_controlPoint);
	}
	else
	{
		newKey.m_controlPoint.m_outTangent = Vec2(0.0f, 0.0f);
		newKey.m_controlPoint.m_value = rightKey.m_controlPoint.m_value;
	}

	return newKey;
}
}

#endif
