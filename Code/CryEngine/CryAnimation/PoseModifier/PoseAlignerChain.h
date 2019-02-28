// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include <CryAnimation/IAnimationPoseModifier.h>
#include <CryExtension/ClassWeaver.h>

struct IKLimbType;

class CPoseAlignerChain :
	public IAnimationPoseAlignerChain
{
private:
	struct SState
	{
		LimbIKDefinitionHandle solver;
		int                    rootJointIndex;
		int                    targetJointIndex;
		int                    contactJointIndex;

		STarget                target;
		ELockMode              eLockMode;
		const IKLimbType* m_pIkLimbType;
	};

	CPoseAlignerChain();
	virtual ~CPoseAlignerChain() {}

public:
	CRYINTERFACE_BEGIN()
	CRYINTERFACE_ADD(IAnimationPoseModifier)
	CRYINTERFACE_ADD(IAnimationPoseAlignerChain)
	CRYINTERFACE_END()

	CRYGENERATE_CLASS_GUID(CPoseAlignerChain, "AnimationPoseModifier_PoseAlignerChain", "6de1ac58-16c9-4c33-8fec-f3ee1f6bdf11"_cry_guid)

public:
	virtual void Initialize(LimbIKDefinitionHandle solver, int contactJointIndex) override;
	virtual void SetTarget(const STarget& target) override   { m_state.target = target; }
	virtual void SetTargetLock(ELockMode eLockMode) override { m_state.eLockMode = eLockMode; }

private:
	bool StoreAnimationPose(const SAnimationPoseModifierParams& params, const IKLimbType& chain, QuatT* pRelative, QuatT* pAbsolute);
	void RestoreAnimationPose(const SAnimationPoseModifierParams& params, const IKLimbType& chain, QuatT* pRelative, QuatT* pAbsolute);
	void BlendAnimateionPose(const SAnimationPoseModifierParams& params, const IKLimbType& chain, QuatT* pRelative, QuatT* pAbsolute);

	void AlignToTarget(const SAnimationPoseModifierParams& params);
	bool MoveToTarget(const SAnimationPoseModifierParams& params, Vec3& contactPosition);

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
	SState m_state;
	SState m_stateExecute;

	// Execute states
	Vec3              m_targetLockPosition;
	Quat              m_targetLockOrientation;
	Plane             m_targetLockPlane;
};
