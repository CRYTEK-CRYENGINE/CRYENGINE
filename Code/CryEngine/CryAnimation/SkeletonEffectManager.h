// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

struct ISkeletonAnim;
class CCharInstance;

class CSkeletonEffectManager
{
public:
	CSkeletonEffectManager();
	~CSkeletonEffectManager();

	void   Update(ISkeletonAnim* pSkeletonAnim, ISkeletonPose* pSkeletonPose, const QuatTS& entityLoc);
	void   SpawnEffect(CCharInstance* pCharInstance, const AnimEventInstance& animEvent, const QuatTS& entityLoc);

	void   KillAllEffects();
	size_t SizeOfThis()
	{
		return m_particlesEffects.capacity() * sizeof(EffectEntry);
	}
	void GetMemoryUsage(ICrySizer* pSizer) const
	{
		pSizer->AddObject(m_particlesEffects);
	}

private:
	struct EffectEntry
	{
		EffectEntry(_smart_ptr<IParticleEffect> pEffect, _smart_ptr<IParticleEmitter> pEmitter, int boneID, const Vec3& offset, const Vec3& dir);
		~EffectEntry();
		void GetMemoryUsage(ICrySizer* pSizer) const {}

		_smart_ptr<IParticleEffect>  pEffect;
		_smart_ptr<IParticleEmitter> pEmitter;
		int                          boneID;
		Vec3                         offset;
		Vec3                         dir;
	};

	void SpawnEffectParticles(CCharInstance* pCharInstance, const AnimEventInstance& animEvent, const QuatTS& entityLoc);
	void SpawnEffectAudio(CCharInstance* pCharInstance, const AnimEventInstance& animEvent, const QuatTS& entityLoc);

	void GetEffectJointLocation(ISkeletonPose* pSkeletonPose, QuatTS& loc, int boneID, const Vec3& offset, const Vec3& dir, const QuatTS& entityLoc);

	bool IsPlayingParticlesEffect(const char* effectName);
	DynArray<EffectEntry> m_particlesEffects;
};
