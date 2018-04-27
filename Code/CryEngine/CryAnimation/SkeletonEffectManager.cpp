// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

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
	for (int i = 0; i < m_particlesEffects.size(); )
	{
		EffectEntry& entry = m_particlesEffects[i];

		// If the animation has stopped, kill the effect.
		const bool effectStillPlaying = (entry.pEmitter ? entry.pEmitter->IsAlive() : false);
		if (effectStillPlaying)
		{
			// Update the effect position.
			QuatTS loc;
			GetEffectJointLocation(pSkeletonPose, loc, entry.boneID, entry.offset, entry.dir, entityLoc);
			if (entry.pEmitter)
				entry.pEmitter->SetLocation(loc);

			++i;
		}
		else
		{
			if (Console::GetInst().ca_DebugSkeletonEffects > 0)
			{
				CryLogAlways("CSkeletonEffectManager::Update(this=%p): Killing effect \"%s\" because %s.", this,
				             (m_particlesEffects[i].pEffect ? m_particlesEffects[i].pEffect->GetName() : "<EFFECT NULL>"), (effectStillPlaying ? "animation has ended" : "effect has ended"));
			}
			if (m_particlesEffects[i].pEmitter)
				m_particlesEffects[i].pEmitter->Activate(false);
			m_particlesEffects.erase(m_particlesEffects.begin() + i);
		}
	}
}

void CSkeletonEffectManager::KillAllEffects()
{
	if (Console::GetInst().ca_DebugSkeletonEffects)
	{
		for (int effectIndex = 0, effectCount = m_particlesEffects.size(); effectIndex < effectCount; ++effectIndex)
		{
			IParticleEffect* pEffect = m_particlesEffects[effectIndex].pEffect;
			CryLogAlways("CSkeletonEffectManager::KillAllEffects(this=%p): Killing effect \"%s\" because animated character is in simplified movement.", this, (pEffect ? pEffect->GetName() : "<EFFECT NULL>"));
		}
	}

	for (int i = 0, count = m_particlesEffects.size(); i < count; ++i)
	{
		if (m_particlesEffects[i].pEmitter)
			m_particlesEffects[i].pEmitter->Activate(false);
	}

	m_particlesEffects.clear();
}

void CSkeletonEffectManager::SpawnEffect(CCharInstance* pCharInstance, const AnimEventInstance& animEvent, const QuatTS& entityLoc)
{
	const unsigned char& firstLetter = animEvent.m_EventName[0];
	switch (firstLetter)
	{
	case 'a':
		if (strcmp(animEvent.m_EventName, "audio_trigger") == 0) { SpawnEffectAudio(pCharInstance, animEvent, entityLoc); }
		break;
	case 'e':
		if (strcmp(animEvent.m_EventName, "effect") == 0) { SpawnEffectParticles(pCharInstance, animEvent, entityLoc); }
		break;
	}
}

void CSkeletonEffectManager::SpawnEffectAudio(CCharInstance* pCharInstance, const AnimEventInstance& animEvent, const QuatTS& entityLoc)
{
	const char* triggerName = animEvent.m_CustomParameter;
	const char* boneName = animEvent.m_BonePathName;
	const Vec3& offset = animEvent.m_vOffset;
	const Vec3& dir = animEvent.m_vDir;

	ISkeletonPose* pISkeletonPose = pCharInstance->GetISkeletonPose();
	const IDefaultSkeleton& rIDefaultSkeleton = pCharInstance->GetIDefaultSkeleton();

	// Determine position
	int boneID = (boneName && boneName[0] ? rIDefaultSkeleton.GetJointIDByName(boneName) : -1);
	boneID = (boneID == -1 ? 0 : boneID);
	QuatTS loc;
	GetEffectJointLocation(pISkeletonPose, loc, boneID, offset, dir, entityLoc);

	// Spawn audio
	CryAudio::ControlId const triggerId = CryAudio::StringToId(triggerName);
	CryAudio::SExecuteTriggerData triggerData(triggerId, triggerName, CryAudio::EOcclusionType::Ignore, loc.t, INVALID_ENTITYID, true);
	gEnv->pAudioSystem->ExecuteTriggerEx(triggerData);
}

void CSkeletonEffectManager::SpawnEffectParticles(CCharInstance* pCharInstance, const AnimEventInstance& animEvent, const QuatTS& entityLoc)
{
	const char* effectName = animEvent.m_CustomParameter;
	const char* boneName = animEvent.m_BonePathName;
	const Vec3& offset = animEvent.m_vOffset;
	const Vec3& dir = animEvent.m_vDir;

	ISkeletonPose* pISkeletonPose = pCharInstance->GetISkeletonPose();
	const IDefaultSkeleton& rIDefaultSkeleton = pCharInstance->GetIDefaultSkeleton();

	// Check whether we are already playing this effect, and if so dont restart it.
	bool alreadyPlayingEffect = false;
	if (!Console::GetInst().ca_AllowMultipleEffectsOfSameName)
		alreadyPlayingEffect = IsPlayingParticlesEffect(effectName);

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
		GetEffectJointLocation(pISkeletonPose, loc, boneID, offset, dir, entityLoc);
		IParticleEmitter* pEmitter = (pEffect ? pEffect->Spawn(false, loc) : 0);
		if (pEffect && pEmitter)
		{
			if (Console::GetInst().ca_DebugSkeletonEffects)
			{
				CryLogAlways("CSkeletonEffectManager::SpawnEffect(this=%p): starting effect \"%s\", requested by an animation event", this, (effectName ? effectName : "<MISSING EFFECT NAME>"));
			}
			m_particlesEffects.push_back(EffectEntry(pEffect, pEmitter, boneID, offset, dir));
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

void CSkeletonEffectManager::GetEffectJointLocation(ISkeletonPose* pSkeletonPose, QuatTS& loc, int boneID, const Vec3& offset, const Vec3& dir, const QuatTS& entityLoc)
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

bool CSkeletonEffectManager::IsPlayingParticlesEffect(const char* effectName)
{
	bool isPlaying = false;
	for (int effectIndex = 0, effectCount = m_particlesEffects.size(); effectIndex < effectCount; ++effectIndex)
	{
		IParticleEffect* pEffect = m_particlesEffects[effectIndex].pEffect;
		if (pEffect && stricmp(pEffect->GetName(), effectName) == 0)
			isPlaying = true;
	}
	return isPlaying;
}
