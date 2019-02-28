// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

/*************************************************************************
   -------------------------------------------------------------------------
   $Id$
   $DateTime$
   Description:

   -------------------------------------------------------------------------
   History:
   - 22:03:2006: Created by Mathieu Pinard

*************************************************************************/
#include "StdAfx.h"
#include <CryAnimation/ICryAnimation.h>
#include "IVehicleSystem.h"
#include "VehiclePartAnimated.h"
#include "VehiclePartAnimatedChar.h"
#include "VehicleAnimation.h"

//------------------------------------------------------------------------
CVehicleAnimation::CVehicleAnimation()
	: m_pPartAnimated(nullptr)
	, m_layerId(0)
	, m_currentStateId(0)
	, m_currentAnimIsWaiting(false)
{}

//------------------------------------------------------------------------
bool CVehicleAnimation::Init(IVehicle* pVehicle, const CVehicleParams& table)
{
	m_currentAnimIsWaiting = false;

	if (table.haveAttr("part"))
	{
		if (IVehiclePart* pPart = pVehicle->GetPart(table.getAttr("part")))
		{
			m_pPartAnimated = CAST_VEHICLEOBJECT(CVehiclePartAnimated, pPart);
			if (!m_pPartAnimated)
			{
				m_pPartAnimated = CAST_VEHICLEOBJECT(CVehiclePartAnimatedChar, pPart);
			}
		}
	}

	if (!m_pPartAnimated)
		return false;

	if (CVehicleParams statesTable = table.findChild("States"))
	{
		int c = statesTable.getChildCount();
		int i = 0;

		m_animationStates.reserve(c);

		for (; i < c; i++)
		{
			if (CVehicleParams stateTable = statesTable.getChild(i))
				ParseState(stateTable, pVehicle);
		}
	}

	if (m_animationStates.empty() == false)
		m_currentStateId = 0;
	else
		m_currentStateId = InvalidVehicleAnimStateId;

	m_layerId = m_pPartAnimated->AssignAnimationLayer();

	return true;
}

//------------------------------------------------------------------------
bool CVehicleAnimation::ParseState(const CVehicleParams& table, IVehicle* pVehicle)
{
	m_animationStates.resize(m_animationStates.size() + 1);
	SAnimationState& animState = m_animationStates.back();

	animState.name = table.getAttr("name");
	animState.animation = table.getAttr("animation");
	animState.sound = table.getAttr("sound");
	animState.pSoundHelper = NULL;

	if (table.haveAttr("sound"))
		animState.pSoundHelper = pVehicle->GetHelper(table.getAttr("sound"));
	else
		animState.pSoundHelper = NULL;

	//animState.soundId = INVALID_SOUNDID;

	table.getAttr("speedDefault", animState.speedDefault);
	table.getAttr("speedMin", animState.speedMin);
	table.getAttr("speedMax", animState.speedMax);
	table.getAttr("isLooped", animState.isLooped);
	animState.isLoopedEx = false;
	table.getAttr("isLoopedEx", animState.isLoopedEx);

	if (CVehicleParams materialsTable = table.findChild("Materials"))
	{
		int i = 0;
		int c = materialsTable.getChildCount();

		animState.materials.reserve(c);

		for (; i < c; ++i)
		{
			if (CVehicleParams materialTable = materialsTable.getChild(i))
			{
				animState.materials.resize(animState.materials.size() + 1);

				SAnimationStateMaterial& stateMaterial = animState.materials.back();

				stateMaterial.material = materialTable.getAttr("name");
				stateMaterial.setting = materialTable.getAttr("setting");

				if (materialTable.haveAttr("min"))
				{
					materialTable.getAttr("min", stateMaterial._min);
				}
				else
				{
					stateMaterial._min = 0.001f;
				}

				if (materialTable.haveAttr("max"))
				{
					materialTable.getAttr("max", stateMaterial._max);
				}
				else
				{
					stateMaterial._max = 0.999f;
				}

				materialTable.getAttr("invertValue", stateMaterial.invertValue);
			}
		}
	}

	return true;
}

//------------------------------------------------------------------------
void CVehicleAnimation::Reset()
{
	if (m_currentStateId != InvalidVehicleAnimStateId)
	{
		const SAnimationState& animState = m_animationStates[m_currentStateId];

		if (IEntity* pEntity = m_pPartAnimated->GetEntity())
		{
			for (TAnimationStateMaterialVector::const_iterator ite = animState.materials.begin(), end = animState.materials.end(); ite != end; ++ite)
			{
				const SAnimationStateMaterial& stateMaterial = *ite;

				if (IMaterial* pMaterial = FindMaterial(stateMaterial, m_pPartAnimated->GetMaterial()))
				{
					float value = max(0.00f, min(1.0f, animState.speedDefault));

					if (stateMaterial.invertValue)
					{
						value = 1.0f - value;
					}

					value = stateMaterial._min + (value * (stateMaterial._max - stateMaterial._min));

					pMaterial->SetGetMaterialParamFloat(stateMaterial.setting, value, false);
				}
			}
		}

		if (m_currentStateId != 0)
			ChangeState(0);
	}

	if (m_layerId > -1)
		StopAnimation();
}

//------------------------------------------------------------------------
bool CVehicleAnimation::StartAnimation()
{
	if (m_currentStateId == -1)
		m_currentStateId = 0;

	SAnimationState& animState = m_animationStates[m_currentStateId];

	if (animState.materials.empty() == false)
	{
		m_pPartAnimated->CloneMaterial();
	}

	if (IEntity* pEntity = m_pPartAnimated->GetEntity())
	{
		if (ICharacterInstance* pCharInstance = pEntity->GetCharacter(m_pPartAnimated->GetSlot()))
		{
			ISkeletonAnim* pSkeletonAnim = pCharInstance->GetISkeletonAnim();
			CRY_ASSERT(pSkeletonAnim);

			CryCharAnimationParams animParams;
			animParams.m_nFlags = CA_FORCE_SKELETON_UPDATE;
			animParams.m_nLayerID = m_layerId;

			if (animState.isLooped)
				animParams.m_nFlags |= CA_LOOP_ANIMATION;

			if (animState.isLoopedEx)
			{
				animParams.m_nFlags |= CA_REPEAT_LAST_KEY;
				uint32 numAnimsLayer0 = pSkeletonAnim->GetNumAnimsInFIFO(animParams.m_nLayerID);
				if (numAnimsLayer0)
				{
					CAnimation& animation = pSkeletonAnim->GetAnimFromFIFO(animParams.m_nLayerID, numAnimsLayer0 - 1);
					const float animationTime = pSkeletonAnim->GetAnimationNormalizedTime(&animation);
					if (animationTime == 1.0f)
						pSkeletonAnim->SetAnimationNormalizedTime(&animation, 0.f);
				}

			}

			// cope with empty animation names (for disabling some in certain vehicle modifications)
			if (animState.animation.empty())
				return false;

			if (!pSkeletonAnim->StartAnimation(animState.animation, animParams))
				return false;

			if (!animState.sound.empty())
			{
				IEntityAudioComponent* pEntitySoundsProxy = pEntity->GetOrCreateComponent<IEntityAudioComponent>();
				CRY_ASSERT(pEntitySoundsProxy);

				REINST("send an animation start event?");
				/*int soundFlags = FLAG_SOUND_DEFAULT_3D;
				   if (animState.isLooped)
				   soundFlags |= FLAG_SOUND_LOOP;

				   Vec3 pos(ZERO);
				   if (animState.pSoundHelper)
				   pos = animState.pSoundHelper->GetVehicleSpaceTranslation();

				   animState.soundId = pEntitySoundsProxy->PlaySound(animState.sound.c_str(), pos, FORWARD_DIRECTION, soundFlags, 0, eSoundSemantic_Vehicle);*/
			}

			pSkeletonAnim->SetLayerPlaybackScale(m_layerId, animState.speedDefault);
			return true;
		}
	}

	return true;
}

//------------------------------------------------------------------------
void CVehicleAnimation::StopAnimation()
{
	if (m_layerId < 0)
		return;

	if (IsUsingManualUpdates())
		SetTime(0.0f);

	IEntity* pEntity = m_pPartAnimated->GetEntity();
	CRY_ASSERT(pEntity);

	SAnimationState& animState = m_animationStates[m_currentStateId];

	if (ICharacterInstance* pCharInstance = pEntity->GetCharacter(m_pPartAnimated->GetSlot()))
	{
		ISkeletonAnim* pSkeletonAnim = pCharInstance->GetISkeletonAnim();
		CRY_ASSERT(pSkeletonAnim);

		pSkeletonAnim->StopAnimationInLayer(m_layerId, 0.0f);
	}

	REINST("send an animation stop event?");
	/*if (animState.soundId != INVALID_SOUNDID)
	   {
	   IEntityAudioComponent* pEntitySoundsProxy = pEntity->GetOrCreateComponent<IEntityAudioComponent>();
	   CRY_ASSERT(pEntitySoundsProxy);

	   pEntitySoundsProxy->StopSound(animState.soundId);
	   animState.soundId = INVALID_SOUNDID;
	   }*/

	m_currentAnimIsWaiting = false;
}

//------------------------------------------------------------------------
TVehicleAnimStateId CVehicleAnimation::GetState()
{
	return m_currentStateId;
}

//------------------------------------------------------------------------
bool CVehicleAnimation::ChangeState(TVehicleAnimStateId stateId)
{
	if (stateId <= InvalidVehicleAnimStateId || stateId > m_animationStates.size())
		return false;

	SAnimationState& animState = m_animationStates[stateId];

	m_currentStateId = stateId;

	if (IEntity* pEntity = m_pPartAnimated->GetEntity())
	{
		if (ICharacterInstance* pCharInstance = pEntity->GetCharacter(m_pPartAnimated->GetSlot()))
		{
			ISkeletonAnim* pSkeletonAnim = pCharInstance->GetISkeletonAnim();
			CRY_ASSERT(pSkeletonAnim);

			if (pSkeletonAnim->GetNumAnimsInFIFO(m_layerId) > 0)
			{
				const CAnimation& animation = pSkeletonAnim->GetAnimFromFIFO(m_layerId, 0);
				const f32 animationNormalizedTime = pSkeletonAnim->GetAnimationNormalizedTime(&animation);
				if (animationNormalizedTime > 0.0f)
				{
					float speed = pSkeletonAnim->GetLayerPlaybackScale(m_layerId) * -1.0f;
					pSkeletonAnim->SetLayerPlaybackScale(m_layerId, speed);
				}
				else
					StopAnimation();
			}
			else
				StartAnimation();
		}
	}

	return true;
}

//------------------------------------------------------------------------
string CVehicleAnimation::GetStateName(TVehicleAnimStateId stateId)
{
	if (stateId <= InvalidVehicleAnimStateId || stateId > m_animationStates.size())
		return "";

	SAnimationState& animState = m_animationStates[stateId];
	return animState.name;
}

//------------------------------------------------------------------------
TVehicleAnimStateId CVehicleAnimation::GetStateId(const string& name)
{
	TVehicleAnimStateId stateId = 0;

	for (TAnimationStateVector::iterator ite = m_animationStates.begin(); ite != m_animationStates.end(); ++ite)
	{
		SAnimationState& animState = *ite;
		if (animState.name == name)
			return stateId;

		stateId++;
	}

	return InvalidVehicleAnimStateId;
}

//------------------------------------------------------------------------
void CVehicleAnimation::SetSpeed(float speed)
{
	if (m_currentStateId == InvalidVehicleAnimStateId)
		return;

	const SAnimationState& animState = m_animationStates[m_currentStateId];

	IEntity* pEntity = m_pPartAnimated->GetEntity();
	CRY_ASSERT(pEntity);

	ICharacterInstance* pCharInstance = pEntity->GetCharacter(m_pPartAnimated->GetSlot());
	if (!pCharInstance)
		return;

	ISkeletonAnim* pSkeletonAnim = pCharInstance->GetISkeletonAnim();
	CRY_ASSERT(pSkeletonAnim);

	pSkeletonAnim->SetLayerPlaybackScale(m_layerId, max(min(speed, animState.speedMax), animState.speedMin));

	for (TAnimationStateMaterialVector::const_iterator ite = animState.materials.begin(), end = animState.materials.end(); ite != end; ++ite)
	{
		const SAnimationStateMaterial& stateMaterial = *ite;

		IMaterial* pPartMaterial = m_pPartAnimated->GetMaterial();

		if (pPartMaterial)
		{
			if (IMaterial* pMaterial = FindMaterial(stateMaterial, pPartMaterial))
			{
				float value = max(0.0f, min(1.0f, speed));

				if (stateMaterial.invertValue)
				{
					value = 1.0f - value;
				}

				value = stateMaterial._min + (value * (stateMaterial._max - stateMaterial._min));

				pMaterial->SetGetMaterialParamFloat(stateMaterial.setting, value, false);
			}
		}
	}
}

//------------------------------------------------------------------------
IMaterial* CVehicleAnimation::FindMaterial(const SAnimationStateMaterial& animStateMaterial, IMaterial* pMaterial)
{
	CRY_ASSERT(pMaterial);
	if (!pMaterial)
		return NULL;

	if (animStateMaterial.material == pMaterial->GetName())
		return pMaterial;

	for (int i = 0; i < pMaterial->GetSubMtlCount(); i++)
	{
		if (IMaterial* pSubMaterial = pMaterial->GetSubMtl(i))
		{
			if (IMaterial* pFoundSubMaterial = FindMaterial(animStateMaterial, pSubMaterial))
				return pFoundSubMaterial;
		}
	}

	return NULL;
}

//------------------------------------------------------------------------
void CVehicleAnimation::ToggleManualUpdate(bool isEnabled)
{
	if (m_layerId < 0 || !m_pPartAnimated)
		return;

	if (ICharacterInstance* pCharInstance =
	      m_pPartAnimated->GetEntity()->GetCharacter(m_pPartAnimated->GetSlot()))
	{
		ISkeletonAnim* pSkeletonAnim = pCharInstance->GetISkeletonAnim();
		CRY_ASSERT(pSkeletonAnim);

		uint32 numAnimsLayer0 = pSkeletonAnim->GetNumAnimsInFIFO(m_layerId);
		if (0 < numAnimsLayer0)
		{
			CAnimation& animation = pSkeletonAnim->GetAnimFromFIFO(m_layerId, numAnimsLayer0 - 1);

			if (isEnabled)
				animation.SetStaticFlag(CA_MANUAL_UPDATE);
			else
				animation.ClearStaticFlag(CA_MANUAL_UPDATE);
		}
	}
}

//------------------------------------------------------------------------
float CVehicleAnimation::GetAnimTime(bool raw /*=false*/)
{
	if (m_layerId < 0 || !m_pPartAnimated)
		return 0.0f;

	if (ICharacterInstance* pCharInstance =
	      m_pPartAnimated->GetEntity()->GetCharacter(m_pPartAnimated->GetSlot()))
	{
		ISkeletonAnim* pSkeletonAnim = pCharInstance->GetISkeletonAnim();
		CRY_ASSERT(pSkeletonAnim);

		uint32 numAnimsLayer0 = pSkeletonAnim->GetNumAnimsInFIFO(m_layerId);
		if (0 < numAnimsLayer0)
		{
			CAnimation& animation = pSkeletonAnim->GetAnimFromFIFO(m_layerId, numAnimsLayer0 - 1);
			const float animationNormalizedTime = pSkeletonAnim->GetAnimationNormalizedTime(&animation);
			if (!raw && animationNormalizedTime == 0.0f)
				return 1.0f;

			return max(0.0f, animationNormalizedTime);
		}
		else
		{
			return 1.0f;
		}
	}

	return 0.0f;
}

//------------------------------------------------------------------------
void CVehicleAnimation::SetTime(float time, bool force)
{
	if (m_layerId < 0 || !m_pPartAnimated)
		return;

	if (ICharacterInstance* pCharInstance =
	      m_pPartAnimated->GetEntity()->GetCharacter(m_pPartAnimated->GetSlot()))
	{
		ISkeletonAnim* pSkeletonAnim = pCharInstance->GetISkeletonAnim();
		CRY_ASSERT(pSkeletonAnim);

		uint32 numAnimsLayer0 = pSkeletonAnim->GetNumAnimsInFIFO(m_layerId);
		if (0 < numAnimsLayer0)
		{
			CAnimation& animation = pSkeletonAnim->GetAnimFromFIFO(m_layerId, numAnimsLayer0 - 1);
			if (force)
				animation.SetStaticFlag(CA_MANUAL_UPDATE);

			if (animation.HasStaticFlag(CA_MANUAL_UPDATE))
				pSkeletonAnim->SetAnimationNormalizedTime(&animation, time);

			//else
			//CryLogAlways("Error: can't use SetTime on a VehicleAnimation that wasn't set for manual updates.");
		}
	}
}

//------------------------------------------------------------------------
bool CVehicleAnimation::IsUsingManualUpdates()
{
	if (ICharacterInstance* pCharInstance =
	      m_pPartAnimated->GetEntity()->GetCharacter(m_pPartAnimated->GetSlot()))
	{
		ISkeletonAnim* pSkeletonAnim = pCharInstance->GetISkeletonAnim();
		CRY_ASSERT(pSkeletonAnim);

		uint32 numAnimsLayer0 = pSkeletonAnim->GetNumAnimsInFIFO(m_layerId);
		if (0 < numAnimsLayer0)
		{
			CAnimation& animation = pSkeletonAnim->GetAnimFromFIFO(m_layerId, numAnimsLayer0 - 1);
			if (animation.HasStaticFlag(CA_MANUAL_UPDATE))
				return true;
		}
	}

	return false;
}
