// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "stdafx.h"
#include "EntityNodeProxy.h"
#include <CryAnimation/CryCharAnimationParams.h>
#include <CryAction/IMaterialEffects.h>
#include <CryAnimation/ICryAnimation.h>

CRYREGISTER_CLASS(CEntityComponentTrackViewNode);

CEntityComponentTrackViewNode::CEntityComponentTrackViewNode()
{
	m_componentFlags.Add(EEntityComponentFlags::NoSave);
}

void CEntityComponentTrackViewNode::Initialize()
{
}

void CEntityComponentTrackViewNode::ProcessEvent(const SEntityEvent& event)
{
	switch (event.event)
	{
	case ENTITY_EVENT_ANIM_EVENT:
		const AnimEventInstance* pAnimEvent = reinterpret_cast<const AnimEventInstance*>(event.nParam[0]);
		ICharacterInstance* pCharacter = reinterpret_cast<ICharacterInstance*>(event.nParam[1]);

		if (pAnimEvent)
		{
			if (pAnimEvent->m_EventName && stricmp(pAnimEvent->m_EventName, "footstep") == 0)
			{
				QuatT jointTransform(IDENTITY);

				if (pAnimEvent->m_BonePathName[0] && pCharacter)
				{
					uint32 boneID = pCharacter->GetIDefaultSkeleton().GetJointIDByName(pAnimEvent->m_BonePathName);
					jointTransform = pCharacter->GetISkeletonPose()->GetAbsJointByID(boneID);
				}

				// Setup FX params
				SMFXRunTimeEffectParams params;
				params.audioProxyEntityId = m_pEntity->GetId();
				params.angle = m_pEntity->GetWorldAngles().z + (gf_PI * 0.5f);

				params.pos = params.decalPos = m_pEntity->GetSlotWorldTM(0) * jointTransform.t;

				REINST("set semantic for audio object")
				//params.soundSemantic = eSoundSemantic_Physics_Footstep;
				params.audioProxyOffset = m_pEntity->GetWorldTM().GetInverted().TransformVector(params.pos - m_pEntity->GetWorldPos());

				IMaterialEffects* pMaterialEffects = gEnv->pMaterialEffects;
				TMFXEffectId effectId = InvalidEffectId;

				// note: for some reason material libraries are named "footsteps" when queried by name
				// and "footstep" when queried by ID
				if (pMaterialEffects)
				{
					if (pAnimEvent->m_CustomParameter[0])
					{
						effectId = pMaterialEffects->GetEffectIdByName("footstep", pAnimEvent->m_CustomParameter);
					}
					else
					{
						effectId = pMaterialEffects->GetEffectIdByName("footstep", "default");
					}
				}

				if (effectId != InvalidEffectId)
					pMaterialEffects->ExecuteEffect(effectId, params);
				else
					gEnv->pSystem->Warning(VALIDATOR_MODULE_EDITOR, VALIDATOR_WARNING, VALIDATOR_FLAG_AUDIO, 0, "Failed to find material for footstep sounds");
			}
		}
	}
}

//////////////////////////////////////////////////////////////////////////
uint64 CEntityComponentTrackViewNode::GetEventMask() const
{
	return ENTITY_EVENT_BIT(ENTITY_EVENT_ANIM_EVENT);
}
