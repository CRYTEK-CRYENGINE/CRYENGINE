// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#ifndef IKTorsoAim_h
#define IKTorsoAim_h

#include <CryExtension/ClassWeaver.h>

#include "IAnimatedCharacter.h"

#define TORSOAIM_MAX_JOINTS 5

class CRY_ALIGN(32) CIKTorsoAim:
	public IAnimationPoseModifierTorsoAim
{
public:
	CRYINTERFACE_BEGIN()
		CRYINTERFACE_ADD(IAnimationPoseModifier)
		CRYINTERFACE_ADD(IAnimationPoseModifierTorsoAim)
	CRYINTERFACE_END()

	CRYGENERATE_CLASS_GUID(CIKTorsoAim, "AnimationPoseModifier_IKTorsoAim", "2058e99d-d052-43e2-8898-5eff40b942e4"_cry_guid)

	CIKTorsoAim();
	virtual ~CIKTorsoAim() {}

public:
	void Enable(bool enable);
	static void InitCVars();

	// IAnimationIKTorsoAim
public:
	virtual void SetBlendWeight(float blend);
	virtual void SetBlendWeightPosition(float blend);
	virtual void SetTargetDirection(const Vec3 &direction);
	virtual void SetViewOffset(const QuatT &offset);
	virtual void SetAbsoluteTargetPosition(const Vec3 &targetPosition);
	virtual void SetFeatherWeights(uint32 weights, const f32 * customBlends);
	virtual void SetJoints(uint32 jntEffector, uint32 jntAdjustment);
	virtual void SetBaseAnimTrackFactor(float factor);

	// IAnimationPoseModifier
public:
	virtual bool Prepare(const SAnimationPoseModifierParams &params) override;
	virtual bool Execute(const SAnimationPoseModifierParams &params) override;
	virtual void Synchronize() override;

	virtual void GetMemoryUsage(ICrySizer* pSizer) const override
	{
		pSizer->AddObject(this, sizeof(*this));
	}

	const QuatT& GetLastProcessedEffector() const
	{
		return m_lastProcessedEffector;
	}

private:

	struct SParams
	{
		SParams();

		QuatT viewOffset;
		Vec3  targetDirection;
		Vec3  absoluteTargetPosition;
		float baseAnimTrackFactor;
		float weights[TORSOAIM_MAX_JOINTS];
		float blend;
		float blendPosition;
		int   numParents;
		int   effectorJoint;
		int   aimJoint;
	};

	struct STorsoAim_CVars
	{
		STorsoAim_CVars()
			: m_initialized(false)
			, STAP_DEBUG(0)
			, STAP_DISABLE(0)
			, STAP_TRANSLATION_FUDGE(0)
			, STAP_TRANSLATION_FEATHER(0)
			, STAP_LOCK_EFFECTOR(0)
			, STAP_OVERRIDE_TRACK_FACTOR(0.0f)
		{
		}

		~STorsoAim_CVars()
		{
			ReleaseCVars();
		}

		void InitCvars();

		int   STAP_DEBUG;
		int   STAP_DISABLE;
		int   STAP_TRANSLATION_FUDGE;
		int   STAP_TRANSLATION_FEATHER;
		int   STAP_LOCK_EFFECTOR;
		float STAP_OVERRIDE_TRACK_FACTOR;

	private:

		void ReleaseCVars();

		bool m_initialized;
	};

	SParams m_params;
	SParams m_setParams;

	bool m_active;
	bool m_blending;
	float m_blendRate;

	Vec3 m_aimToEffector;
	Quat m_effectorDirQuat;
	QuatT m_lastProcessedEffector;

	static STorsoAim_CVars s_CVars;

	void Init();

};

#endif // IKTorsoAim_h
