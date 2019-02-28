// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "JoystickUtils.h"
#include <CryInput/IJoystick.h>
#include <CryMath/ISplines.h>
#include <CryAnimation/IFacialAnimation.h>

float JoystickUtils::Evaluate(IJoystickChannel* pChannel, float time)
{
	float total = 0.0f;
	for (int splineIndex = 0, splineCount = (pChannel ? int(pChannel->GetSplineCount()) : 0); splineIndex < splineCount; ++splineIndex)
	{
		ISplineInterpolator* pSpline = (pChannel ? pChannel->GetSpline(splineIndex) : 0);
		float v = 0.0f;
		if (pSpline)
			pSpline->InterpolateFloat(time, v);
		total += v;
	}
	return total;
}

void JoystickUtils::SetKey(IJoystickChannel* pChannel, float time, float value, bool createIfMissing)
{
	// Add up all the splines except the last one.
	float total = 0.0f;
	for (int splineIndex = 0, splineCount = (pChannel ? int(pChannel->GetSplineCount()) : 0); splineIndex < splineCount - 1; ++splineIndex)
	{
		ISplineInterpolator* pSpline = (pChannel ? pChannel->GetSpline(splineIndex) : 0);
		float v = 0.0f;
		if (pSpline)
			pSpline->InterpolateFloat(time, v);
		total += v;
	}

	// Set the key on the last spline so that the total value equals the required value.
	int splineCount = (pChannel ? pChannel->GetSplineCount() : 0);
	ISplineInterpolator* interpolator = (pChannel && splineCount >= 0 ? pChannel->GetSpline(splineCount - 1) : 0);
	int keyIndex = (interpolator ? interpolator->FindKey(FacialEditorSnapTimeToFrame(time), 1.0e-5f) : -1);
	if (interpolator && keyIndex < 0)
	{
		if (createIfMissing)
			interpolator->InsertKeyFloat(FacialEditorSnapTimeToFrame(time), value - total);
	}
	else if (interpolator)
	{
		interpolator->SetKeyValueFloat(keyIndex, value - total);
	}
}

void JoystickUtils::Serialize(IJoystickChannel* pChannel, XmlNodeRef node, bool bLoading)
{
	int splineCount = (pChannel ? pChannel->GetSplineCount() : 0);
	ISplineInterpolator* interpolator = (pChannel && splineCount >= 0 ? pChannel->GetSpline(splineCount - 1) : 0);
	if (interpolator)
		interpolator->SerializeSpline(node, bLoading);
}

bool JoystickUtils::HasKey(IJoystickChannel* pChannel, float time)
{
	int splineCount = (pChannel ? pChannel->GetSplineCount() : 0);
	ISplineInterpolator* interpolator = (pChannel && splineCount >= 0 ? pChannel->GetSpline(splineCount - 1) : 0);
	int keyIndex = (interpolator ? interpolator->FindKey(FacialEditorSnapTimeToFrame(time), 1.0e-5f) : -1);
	return (keyIndex >= 0);
}

void JoystickUtils::RemoveKey(IJoystickChannel* pChannel, float time)
{
	int splineCount = (pChannel ? pChannel->GetSplineCount() : 0);
	ISplineInterpolator* interpolator = (pChannel && splineCount >= 0 ? pChannel->GetSpline(splineCount - 1) : 0);
	int keyIndex = (interpolator ? interpolator->FindKey(FacialEditorSnapTimeToFrame(time), 1.0e-5f) : -1);
	if (interpolator)
		interpolator->RemoveKey(keyIndex);
}

void JoystickUtils::RemoveKeysInRange(IJoystickChannel* pChannel, float startTime, float endTime)
{
	int splineCount = (pChannel ? pChannel->GetSplineCount() : 0);
	ISplineInterpolator* interpolator = (pChannel && splineCount >= 0 ? pChannel->GetSpline(splineCount - 1) : 0);
	if (interpolator)
		interpolator->RemoveKeysInRange(startTime, endTime);
}

void JoystickUtils::PlaceKey(IJoystickChannel* pChannel, float time)
{
	float frameTime = FacialEditorSnapTimeToFrame(time);
	int splineCount = (pChannel ? pChannel->GetSplineCount() : 0);
	ISplineInterpolator* interpolator = (pChannel && splineCount >= 0 ? pChannel->GetSpline(splineCount - 1) : 0);
	if (pChannel && interpolator && !HasKey(pChannel, frameTime))
	{
		float value;
		interpolator->InterpolateFloat(frameTime, value);
		interpolator->InsertKeyFloat(frameTime, value);
	}
}

