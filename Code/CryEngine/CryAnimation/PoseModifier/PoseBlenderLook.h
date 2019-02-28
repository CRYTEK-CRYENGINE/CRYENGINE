// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include <CryAnimation/IAnimationPoseModifier.h>
#include <CryExtension/ClassWeaver.h>
#include "DirectionalBlender.h"

class CAttachmentBONE;

class CPoseBlenderLook : public IAnimationPoseBlenderDir
{
	CRYINTERFACE_BEGIN()
	CRYINTERFACE_ADD(IAnimationPoseModifier)
	CRYINTERFACE_ADD(IAnimationPoseBlenderDir)
	CRYINTERFACE_END()

	CRYGENERATE_CLASS_GUID(CPoseBlenderLook, "AnimationPoseModifier_PoseBlenderLook", "058c3e18-b987-4faf-8989-b9cb2cff0d64"_cry_guid)

	CPoseBlenderLook();
	virtual ~CPoseBlenderLook() {}

	// This interface
public:
	void SetState(bool state) override                                                    { m_blender.m_Set.bUseDirIK = state; }
	void SetTarget(const Vec3& target) override                                           { if (target.IsValid()) m_blender.m_Set.vDirIKTarget = target; }
	void SetLayer(uint32 layer) override                                                  { m_blender.m_Set.nDirLayer = uint8(std::max(layer, (uint32)1)); }
	void SetFadeoutAngle(f32 angleRadians) override                                       { m_blender.m_Set.fDirIKFadeoutRadians = angleRadians; }
	void SetFadeOutSpeed(f32 time) override                                               { m_blender.m_Set.fDirIKFadeOutTime = (time > 0.0f) ? 1.0f / time : FLT_MAX; }
	void SetFadeInSpeed(f32 time) override                                                { m_blender.m_Set.fDirIKFadeInTime = (time > 0.0f) ? 1.0f / time : FLT_MAX; }
	void SetFadeOutMinDistance(f32 minDistance) override                                  { m_blender.m_Set.fDirIKMinDistanceSquared = minDistance * minDistance; }
	void SetPolarCoordinatesOffset(const Vec2& offset) override                           { m_blender.m_Set.vPolarCoordinatesOffset = offset; }
	void SetPolarCoordinatesSmoothTimeSeconds(f32 smoothTimeSeconds) override             { m_blender.m_Set.fPolarCoordinatesSmoothTimeSeconds = smoothTimeSeconds; }
	void SetPolarCoordinatesMaxRadiansPerSecond(const Vec2& maxRadiansPerSecond) override { m_blender.m_Set.vPolarCoordinatesMaxRadiansPerSecond = maxRadiansPerSecond; }
	f32  GetBlend() const override                                                        { return m_blender.m_Get.fDirIKInfluence; }

public:
	//high-level setup code. most of it it redundant when we switch to VEGs
	virtual bool Prepare(const SAnimationPoseModifierParams& params) override;
	//brute force pose-blending in Jobs
	virtual bool Execute(const SAnimationPoseModifierParams& params) override;
	//just for final fix-up
	virtual void Synchronize() override;

	void         GetMemoryUsage(ICrySizer* pSizer) const override
	{
		pSizer->AddObject(this, sizeof(*this));
	}

private:
	ILINE void LimitEye(Vec3& eyeAimDirection) const
	{
		const float yawRadians = atan2_tpl(-eyeAimDirection.x, eyeAimDirection.y);
		const float pitchRadians = asin(eyeAimDirection.z);

		const float clampedYawRadians = clamp_tpl(yawRadians, -m_eyeLimitHalfYawRadians, m_eyeLimitHalfYawRadians);
		const float clampedPitchRadians = clamp_tpl(pitchRadians, -m_eyeLimitPitchRadiansDown, m_eyeLimitPitchRadiansUp);

		eyeAimDirection = Quat::CreateRotationZ(clampedYawRadians) * Quat::CreateRotationX(clampedPitchRadians) * Vec3(0, 1, 0);
	}

	bool PrepareInternal(const SAnimationPoseModifierParams& params);

public:

	CAttachmentBONE*    m_pAttachmentEyeLeft;
	CAttachmentBONE*    m_pAttachmentEyeRight;
	QuatT               m_ql;
	QuatT               m_qr;
	int32               m_EyeIdxL;
	int32               m_EyeIdxR;

	f32                 m_eyeLimitHalfYawRadians;
	f32                 m_eyeLimitPitchRadiansUp;
	f32                 m_eyeLimitPitchRadiansDown;

	Quat                m_additionalRotationLeft;
	Quat                m_additionalRotationRight;

	SDirectionalBlender m_blender;
};
