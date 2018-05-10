// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include <CryAnimation/IAnimationPoseModifier.h>
#include <CryExtension/ClassWeaver.h>
#include "DirectionalBlender.h"

class CPoseBlenderAim : public IAnimationPoseBlenderDir
{
	CRYINTERFACE_BEGIN()
	CRYINTERFACE_ADD(IAnimationPoseModifier)
	CRYINTERFACE_ADD(IAnimationPoseBlenderDir)
	CRYINTERFACE_END()

	CRYGENERATE_CLASS_GUID(CPoseBlenderAim, "AnimationPoseModifier_PoseBlenderAim", "058c3e18-b957-4faf-8989-b9cb2cff0d64"_cry_guid)

	virtual ~CPoseBlenderAim() {}

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
	//sometimes use for final adjustments
	virtual void Synchronize() override { m_blender.m_Get = m_blender.m_dataOut; };

	void         GetMemoryUsage(ICrySizer* pSizer) const override
	{
		pSizer->AddObject(this, sizeof(*this));
	}

private:
	bool PrepareInternal(const SAnimationPoseModifierParams& params);

public:
	SDirectionalBlender m_blender;
};
