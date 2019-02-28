// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

// CryEngine Header File.
// Copyright (C), Crytek, 1999-2014.

#pragma once

#include "TrackViewEntityNode.h"
#include "Objects/CameraObject.h"

////////////////////////////////////////////////////////////////////////////
//
// This class represents an IAnimNode that is a camera
//
////////////////////////////////////////////////////////////////////////////
class CTrackViewCameraNode : public CTrackViewEntityNode, public ICameraObjectListener
{
public:
	CTrackViewCameraNode(IAnimSequence* pSequence, IAnimNode* pAnimNode, CTrackViewNode* pParentNode, CEntityObject* pEntityObject);

	virtual void OnNodeAnimated(IAnimNode* pNode) override;

	// Set as view camera
	virtual void SetAsViewCamera();

	virtual void BindToEditorObjects() override;
	virtual void UnBindFromEditorObjects() override;

	// Get camera shake rotation
	void GetShakeRotation(const SAnimTime time, Quat& rotation);

private:
	template<class T> void SetParameter(SAnimTime time, EAnimParamType paramType, const T& type);
	template<class T> void GetParameter(SAnimTime time, EAnimParamType paramType, T& type);

	virtual void           OnFovChange(const float fov);
	virtual void           OnNearZChange(const float nearZ);
	virtual void           OnFarZChange(const float farZ) {}
	virtual void           OnOmniCameraChange(const bool isOmni) {}
	virtual void           OnShakeAmpAChange(const Vec3 amplitude);
	virtual void           OnShakeAmpBChange(const Vec3 amplitude);
	virtual void           OnShakeFreqAChange(const Vec3 frequency);
	virtual void           OnShakeFreqBChange(const Vec3 frequency);
	virtual void           OnShakeMultChange(const float amplitudeAMult, const float amplitudeBMult, const float frequencyAMult, const float frequencyBMult);
	virtual void           OnShakeNoiseChange(const float noiseAAmpMult, const float noiseBAmpMult, const float noiseAFreqMult, const float noiseBFreqMult);
	virtual void           OnShakeWorkingChange(const float timeOffsetA, const float timeOffsetB);
	virtual void           OnCameraShakeSeedChange(const int seed);

	IAnimCameraNode* m_pAnimCameraNode;
};

