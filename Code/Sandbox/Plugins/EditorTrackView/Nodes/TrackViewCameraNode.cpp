// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

// CryEngine Source File.
// Copyright (C), Crytek, 1999-2014.

#include "StdAfx.h"
#include "TrackViewCameraNode.h"
#include "TrackViewSequence.h"
#include "RenderViewport.h"
#include "ViewManager.h"

CTrackViewCameraNode::CTrackViewCameraNode(IAnimSequence* pSequence, IAnimNode* pAnimNode, CTrackViewNode* pParentNode, CEntityObject* pEntityObject)
	: CTrackViewEntityNode(pSequence, pAnimNode, pParentNode, pEntityObject)
{
	m_pAnimCameraNode = GetAnimNode()->QueryCameraNodeInterface();
}

void CTrackViewCameraNode::OnNodeAnimated(IAnimNode* pNode)
{
	CTrackViewEntityNode::OnNodeAnimated(pNode);

	const SAnimTime time = GetSequence()->GetTime();

	CEntityObject* pEntityObject = GetNodeEntity(false);
	if (pEntityObject)
	{
		CCameraObject* pCameraObject = static_cast<CCameraObject*>(pEntityObject);

		float fov;
		GetParameter(time, eAnimParamType_FOV, fov);
		pCameraObject->SetFOV(DEG2RAD(fov));

		float nearZ;
		GetParameter(time, eAnimParamType_NearZ, nearZ);
		pCameraObject->SetNearZ(nearZ);

		Vec4 multipliers;
		GetParameter(time, eAnimParamType_ShakeMultiplier, multipliers);
		pCameraObject->SetAmplitudeAMult(multipliers.x);
		pCameraObject->SetAmplitudeBMult(multipliers.y);
		pCameraObject->SetFrequencyAMult(multipliers.z);
		pCameraObject->SetFrequencyBMult(multipliers.w);
	}
}

void CTrackViewCameraNode::SetAsViewCamera()
{
	CEntityObject* pCameraEntity = GetNodeEntity();
	CRenderViewport* pRenderViewport = static_cast<CRenderViewport*>(GetIEditor()->GetViewManager()->GetGameViewport());
	pRenderViewport->SetCameraObject(pCameraEntity);
}

void CTrackViewCameraNode::BindToEditorObjects()
{
	CTrackViewEntityNode::BindToEditorObjects();

	CEntityObject* pEntityObject = GetNodeEntity(false);
	if (pEntityObject)
	{
		CCameraObject* pCameraObject = static_cast<CCameraObject*>(pEntityObject);
		pCameraObject->RegisterCameraListener(this);
	}
}

void CTrackViewCameraNode::UnBindFromEditorObjects()
{
	CEntityObject* pEntityObject = GetNodeEntity(false);
	if (pEntityObject)
	{
		CCameraObject* pCameraObject = static_cast<CCameraObject*>(pEntityObject);
		pCameraObject->UnregisterCameraListener(this);
	}

	CTrackViewEntityNode::UnBindFromEditorObjects();
}

void CTrackViewCameraNode::GetShakeRotation(const SAnimTime time, Quat& rotation)
{
	GetAnimNode()->QueryCameraNodeInterface()->GetShakeRotation(time, rotation);
}

void CTrackViewCameraNode::OnFovChange(const float fov)
{
	if (!GetSequence()->IsAnimating())
	{
		const SAnimTime time = GetSequence()->GetTime();
		SetParameter(time, eAnimParamType_FOV, fov);
	}
}

void CTrackViewCameraNode::OnNearZChange(const float nearZ)
{
	if (!GetSequence()->IsAnimating())
	{
		const SAnimTime time = GetSequence()->GetTime();
		SetParameter(time, eAnimParamType_NearZ, nearZ);
	}
}

void CTrackViewCameraNode::OnShakeAmpAChange(const Vec3 amplitude)
{
	if (!GetSequence()->IsAnimating())
	{
		const SAnimTime time = GetSequence()->GetTime();
		SetParameter(time, eAnimParamType_ShakeAmplitudeA, amplitude);
	}
}

void CTrackViewCameraNode::OnShakeAmpBChange(const Vec3 amplitude)
{
	if (!GetSequence()->IsAnimating())
	{
		const SAnimTime time = GetSequence()->GetTime();
		SetParameter(time, eAnimParamType_ShakeAmplitudeB, amplitude);
	}
}

void CTrackViewCameraNode::OnShakeFreqAChange(const Vec3 frequency)
{
	if (!GetSequence()->IsAnimating())
	{
		const SAnimTime time = GetSequence()->GetTime();
		SetParameter(time, eAnimParamType_ShakeFrequencyA, frequency);
	}
}

void CTrackViewCameraNode::OnShakeFreqBChange(const Vec3 frequency)
{
	if (!GetSequence()->IsAnimating())
	{
		const SAnimTime time = GetSequence()->GetTime();
		SetParameter(time, eAnimParamType_ShakeFrequencyB, frequency);
	}
}

void CTrackViewCameraNode::OnShakeMultChange(const float amplitudeAMult, const float amplitudeBMult, const float frequencyAMult, const float frequencyBMult)
{
	if (!GetSequence()->IsAnimating())
	{
		const SAnimTime time = GetSequence()->GetTime();
		SetParameter(time, eAnimParamType_ShakeMultiplier, Vec4(amplitudeAMult, amplitudeBMult, frequencyAMult, frequencyBMult));
	}
}

void CTrackViewCameraNode::OnShakeNoiseChange(const float noiseAAmpMult, const float noiseBAmpMult, const float noiseAFreqMult, const float noiseBFreqMult)
{
	if (!GetSequence()->IsAnimating())
	{
		const SAnimTime time = GetSequence()->GetTime();
		SetParameter(time, eAnimParamType_ShakeNoise, Vec4(noiseAAmpMult, noiseBAmpMult, noiseAFreqMult, noiseBFreqMult));
	}
}

void CTrackViewCameraNode::OnShakeWorkingChange(const float timeOffsetA, const float timeOffsetB)
{
	if (!GetSequence()->IsAnimating())
	{
		const SAnimTime time = GetSequence()->GetTime();
		SetParameter(time, eAnimParamType_ShakeWorking, Vec4(timeOffsetA, timeOffsetB, 0.0f, 0.0f));
	}
}

void CTrackViewCameraNode::OnCameraShakeSeedChange(const int seed)
{
	if (!GetSequence()->IsAnimating())
	{
		GetAnimNode()->QueryCameraNodeInterface()->SetCameraShakeSeed(seed);
	}
}

template<class T> void CTrackViewCameraNode::SetParameter(SAnimTime time, EAnimParamType paramType, const T& param)
{
	m_pAnimCameraNode->SetParameter(time, paramType, TMovieSystemValue(param));
}

template<class T> void CTrackViewCameraNode::GetParameter(SAnimTime time, EAnimParamType paramType, T& param)
{
	param = stl::get<T>(m_pAnimCameraNode->GetParameter(time, paramType));
}

