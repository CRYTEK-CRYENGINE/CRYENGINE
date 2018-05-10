// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include <CryAnimation/IAnimationPoseModifier.h>
#include <CryExtension/ClassWeaver.h>

class CPoseMatching :
	public IAnimationPoseMatching
{
public:
	CRYINTERFACE_BEGIN()
	CRYINTERFACE_ADD(IAnimationPoseModifier)
	CRYINTERFACE_ADD(IAnimationPoseMatching)
	CRYINTERFACE_END()

	CRYGENERATE_CLASS_GUID(CPoseMatching, "AnimationPoseModifier_PoseMatching", "18318a27-2246-464e-a4b7-adffa51a9508"_cry_guid)

	CPoseMatching();
	virtual ~CPoseMatching() {}

	// IAnimationPoseMatching
public:
	virtual void SetAnimations(const uint* pAnimationIds, uint count) override;
	virtual bool GetMatchingAnimation(uint& animationId) const override;

	// IAnimationPoseModifier
public:
	virtual bool Prepare(const SAnimationPoseModifierParams& params) override;
	virtual bool Execute(const SAnimationPoseModifierParams& params) override;
	virtual void Synchronize() override;

	void         GetMemoryUsage(ICrySizer* pSizer) const override
	{
		pSizer->AddObject(this, sizeof(*this));
	}

private:
	uint        m_jointStartIndex;
	const uint* m_pAnimationIds;
	uint        m_animationCount;

	int         m_animationMatchId;
};
