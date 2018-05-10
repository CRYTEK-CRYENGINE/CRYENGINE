// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "stdafx.h"

#include "EffectPlayer.h"

#include <CryAnimation/ICryAnimation.h>
#include <CryInput/IInput.h>
#include <CryEntitySystem/IEntitySystem.h>

namespace CharacterTool {

EffectPlayer::EffectPlayer()
	: m_pSkeleton2(0), m_pSkeletonPose(0), m_pIDefaultSkeleton(0)
{
	GetIEditor()->RegisterNotifyListener(this);
}

EffectPlayer::~EffectPlayer()
{
	KillAllEffects();
	GetIEditor()->UnregisterNotifyListener(this);
}

void EffectPlayer::SetSkeleton(ISkeletonAnim* pSkeletonAnim, ISkeletonPose* pSkeletonPose, IDefaultSkeleton* pIDefaultSkeleton)
{
	m_pSkeleton2 = pSkeletonAnim;
	m_pSkeletonPose = pSkeletonPose;
	m_pIDefaultSkeleton = pIDefaultSkeleton;
}

void EffectPlayer::Update(const QuatT& rPhysEntity)
{
	for (int i = 0; i < m_effects.size(); )
	{
		EffectEntry& entry = m_effects[i];

		// If the animation has stopped, kill the effect.
		const bool effectStillPlaying = (entry.pEmitter ? entry.pEmitter->IsAlive() : false);
		if (effectStillPlaying)
		{
			// Update the effect position.
			Matrix34 tm;
			GetEffectTM(tm, entry.boneID, entry.offset, entry.dir);
			if (entry.pEmitter)
				entry.pEmitter->SetMatrix(Matrix34(rPhysEntity) * tm);
			++i;
		}
		else
		{
			if (m_effects[i].pEmitter)
				m_effects[i].pEmitter->Activate(false);
			m_effects.erase(m_effects.begin() + i);
		}
	}
}

void EffectPlayer::KillAllEffects()
{
	for (int i = 0, count = m_effects.size(); i < count; ++i)
	{
		if (m_effects[i].pEmitter)
			m_effects[i].pEmitter->Activate(false);
	}
	m_effects.clear();
}

void EffectPlayer::SpawnEffect(const char* effectName, const char* boneName, const Vec3& offset, const Vec3& dir)
{
	// Check whether we are already playing this effect, and if so dont restart it.
	bool alreadyPlayingEffect = false;
	if (!gEnv->pConsole->GetCVar("ca_AllowMultipleEffectsOfSameName")->GetIVal())
		alreadyPlayingEffect = IsPlayingEffect(effectName);

	if (!alreadyPlayingEffect)
	{
		IParticleEffect* pEffect = gEnv->pParticleManager->FindEffect(effectName);
		if (!pEffect)
		{
			CryWarning(VALIDATOR_MODULE_EDITOR, VALIDATOR_WARNING, "Cannot find effect \"%s\" triggered by an animation event.", (effectName ? effectName : "<MISSING EFFECT NAME>"));
		}
		else
		{
			int boneID = (boneName && boneName[0] && m_pIDefaultSkeleton ? m_pIDefaultSkeleton->GetJointIDByName(boneName) : -1);
			boneID = (boneID == -1 ? 0 : boneID);
			Matrix34 tm;
			GetEffectTM(tm, boneID, offset, dir);
			SpawnParams sp;
			sp.bNowhere = true;
			if (IParticleEmitter* pEmitter = pEffect->Spawn(tm, sp))
			{
				// Make sure the emitter is not rendered in the game.
				m_effects.push_back(EffectEntry(pEffect, pEmitter, boneID, offset, dir));
			}
		}
	}
}

void EffectPlayer::OnEditorNotifyEvent(EEditorNotifyEvent event)
{
	switch (event)
	{
	case eNotify_OnClearLevelContents:
		KillAllEffects();
		break;
	}
}

EffectPlayer::EffectEntry::EffectEntry(const _smart_ptr<IParticleEffect>& pEffect, const _smart_ptr<IParticleEmitter>& pEmitter, int boneID, const Vec3& offset, const Vec3& dir)
	: pEffect(pEffect), pEmitter(pEmitter), boneID(boneID), offset(offset), dir(dir)
{
}

EffectPlayer::EffectEntry::~EffectEntry()
{
}

void EffectPlayer::GetEffectTM(Matrix34& tm, int boneID, const Vec3& offset, const Vec3& dir)
{
	if (dir.len2() > 0)
		tm = Matrix33::CreateRotationXYZ(Ang3(dir * 3.14159f / 180.0f));
	else
		tm.SetIdentity();
	tm.AddTranslation(offset);

	if (m_pSkeletonPose)
		tm = Matrix34(m_pSkeletonPose->GetAbsJointByID(boneID)) * tm;
}

bool EffectPlayer::IsPlayingEffect(const char* effectName)
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

void EffectPlayer::Render(SRendParams& params, const SRenderingPassInfo& passInfo)
{
	for (int effectIndex = 0, effectCount = m_effects.size(); effectIndex < effectCount; ++effectIndex)
	{
		IParticleEmitter* pEmitter = m_effects[effectIndex].pEmitter;
		if (pEmitter)
		{
			pEmitter->Update();
			pEmitter->Render(params, passInfo);
		}
	}
}

}

