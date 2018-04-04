// Copyright 2001-2017 Crytek GmbH / Crytek Group. All rights reserved. 

#include "stdafx.h"
#include "SkeletonEffectManager.h"

#include "CharacterInstance.h"
#include <CryAnimation/ICryAnimation.h>
#include "cvars.h"

CSkeletonEffectManager::CSkeletonEffectManager()
{
}

CSkeletonEffectManager::~CSkeletonEffectManager()
{
	KillAllEffects();
}

void CSkeletonEffectManager::Update(ISkeletonAnim* pSkeleton, ISkeletonPose* pSkeletonPose, const QuatTS& entityLoc)
{
	for (int i = 0; i < m_effects.size(); )
	{
		EffectEntry& entry = m_effects[i];

		// If the animation has stopped, kill the effect.
		const bool effectStillPlaying = (entry.pEmitter ? entry.pEmitter->IsAlive() : false);
		if (effectStillPlaying)
		{
			// Update the effect position.
			QuatTS loc;
			GetEffectLoc(pSkeletonPose, loc, entry.boneID, entry.offset, entry.dir, entityLoc);
			if (entry.pEmitter)
				entry.pEmitter->SetLocation(loc);

			++i;
		}
		else
		{
			if (Console::GetInst().ca_DebugSkeletonEffects > 0)
			{
				CryLogAlways("CSkeletonEffectManager::Update(this=%p): Killing effect \"%s\" because %s.", this,
				             (m_effects[i].pEffect ? m_effects[i].pEffect->GetName() : "<EFFECT NULL>"), (effectStillPlaying ? "animation has ended" : "effect has ended"));
			}
			if (m_effects[i].pEmitter)
				m_effects[i].pEmitter->Activate(false);
			m_effects.erase(m_effects.begin() + i);
		}
	}
}

void CSkeletonEffectManager::KillAllEffects()
{
	if (Console::GetInst().ca_DebugSkeletonEffects)
	{
		for (int effectIndex = 0, effectCount = m_effects.size(); effectIndex < effectCount; ++effectIndex)
		{
			IParticleEffect* pEffect = m_effects[effectIndex].pEffect;
			CryLogAlways("CSkeletonEffectManager::KillAllEffects(this=%p): Killing effect \"%s\" because animated character is in simplified movement.", this, (pEffect ? pEffect->GetName() : "<EFFECT NULL>"));
		}
	}

	for (int i = 0, count = m_effects.size(); i < count; ++i)
	{
		if (m_effects[i].pEmitter)
			m_effects[i].pEmitter->Activate(false);
	}

	m_effects.clear();
}

void CSkeletonEffectManager::SpawnEffect(CCharInstance* pCharInstance, const char* effectName, const char* boneName, const Vec3& offset, const Vec3& dir, const QuatTS& entityLoc)
{
	ISkeletonPose* pISkeletonPose = pCharInstance->GetISkeletonPose();
	const IDefaultSkeleton& rIDefaultSkeleton = pCharInstance->GetIDefaultSkeleton();

	// Check whether we are already playing this effect, and if so dont restart it.
	bool alreadyPlayingEffect = false;
	if (!Console::GetInst().ca_AllowMultipleEffectsOfSameName)
		alreadyPlayingEffect = IsPlayingEffect(effectName);

	if (alreadyPlayingEffect)
	{
		if (Console::GetInst().ca_DebugSkeletonEffects)
			CryLogAlways("CSkeletonEffectManager::SpawnEffect(this=%p): Refusing to start effect \"%s\" because effect is already running.", this, (effectName ? effectName : "<MISSING EFFECT NAME>"));
	}
	else
	{
		IParticleEffect* pEffect = gEnv->pParticleManager->FindEffect(effectName);

#if !defined(DEDICATED_SERVER)
		if (!gEnv->IsDedicated() && !pEffect)
		{
			gEnv->pLog->LogError("Cannot find effect \"%s\" requested by an animation event.", (effectName ? effectName : "<MISSING EFFECT NAME>"));
		}
#endif
		int boneID = (boneName && boneName[0] ? rIDefaultSkeleton.GetJointIDByName(boneName) : -1);
		boneID = (boneID == -1 ? 0 : boneID);
		QuatTS loc;
		GetEffectLoc(pISkeletonPose, loc, boneID, offset, dir, entityLoc);
		IParticleEmitter* pEmitter = (pEffect ? pEffect->Spawn(false, loc) : 0);
		if (pEffect && pEmitter)
		{
			if (Console::GetInst().ca_DebugSkeletonEffects)
			{
				CryLogAlways("CSkeletonEffectManager::SpawnEffect(this=%p): starting effect \"%s\", requested by an animation event", this, (effectName ? effectName : "<MISSING EFFECT NAME>"));
			}
			m_effects.push_back(EffectEntry(pEffect, pEmitter, boneID, offset, dir));
		}
	}
}

CSkeletonEffectManager::EffectEntry::EffectEntry(_smart_ptr<IParticleEffect> pEffect, _smart_ptr<IParticleEmitter> pEmitter, int boneID, const Vec3& offset, const Vec3& dir)
	: pEffect(pEffect), pEmitter(pEmitter), boneID(boneID), offset(offset), dir(dir)
{
}

CSkeletonEffectManager::EffectEntry::~EffectEntry()
{
}

void CSkeletonEffectManager::GetEffectLoc(ISkeletonPose* pSkeletonPose, QuatTS& loc, int boneID, const Vec3& offset, const Vec3& dir, const QuatTS& entityLoc)
{
	if (dir.len2() > 0)
		loc.q = Quat::CreateRotationXYZ(Ang3(dir * 3.14159f / 180.0f));
	else
		loc.q.SetIdentity();
	loc.t = offset;
	loc.s = 1.f;

	if (pSkeletonPose)
		loc = pSkeletonPose->GetAbsJointByID(boneID) * loc;
	loc = entityLoc * loc;
}

bool CSkeletonEffectManager::IsPlayingEffect(const char* effectName)
{
	bool isPlaying = false;
	for (int effectIndex = 0, effectCount = m_effects.size(); effectIndex < effectCount; ++effectIndex)
	{
		IParticleEffect* pEffect = m_effects[effectIndex].pEffect;
		if (pEffect && stricmp(pEffect->GetName(), effectName) == 0)
			isPlaying = true;
	}
	return isPlaying;
}
