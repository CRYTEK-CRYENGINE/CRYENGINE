// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include <CryAnimation/CryCharMorphParams.h>

class CryModelAnimationContainer;

class CryModEffMorph
{
public:
	// advances the current time of the played animation and returns the blending factor by which this animation affects the bone pose
	void Tick(f32 fDeltaTime);

	// starts the morphing sequence
	void StartMorph(int nMorphTargetId, const CryCharMorphParams& rParams);

	// returns false when this morph target is inactive
	bool isActive() const;

	// returns the blending factor for this morph target
	f32 getBlending() const;

	// returns the morph target
	int  getMorphTargetId() const;

	void setTime(f32 fTime)   { m_fTime = fTime; }
	void setSpeed(f32 fSpeed) { m_Params.m_fSpeed = fSpeed; }
	void stop();

	f32  getTime() const { return m_fTime; }
	void freeze()        { m_nFlags |= m_Params.FLAGS_FREEZE; }

	void GetMemoryUsage(ICrySizer* pSizer) const
	{
		pSizer->AddObject(this, sizeof(*this));
		pSizer->AddObject(&m_Params, sizeof(m_Params));
		pSizer->AddObject(m_fTime);
		pSizer->AddObject(m_nMorphTargetId);
		pSizer->AddObject(m_nFlags);
	}
public:

	// the animation container that will answer all questions regarding the morph target
	//CryModelAnimationContainer* m_pAnimations;

	// the blend time
	CryCharMorphParams m_Params;
	// time of morphing
	f32                m_fTime;
	// morph target id
	int                m_nMorphTargetId;
	unsigned           m_nFlags; // the copy of the flags from m_Params
};
