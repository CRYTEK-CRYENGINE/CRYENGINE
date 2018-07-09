// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

/*************************************************************************
-------------------------------------------------------------------------

Description: 

-------------------------------------------------------------------------
History:
- 10:02:2011: Benito G.R.		

*************************************************************************/

#include "StdAfx.h"
#include "EntityEffects.h"

#include <CryParticleSystem/ParticleParams.h>
#include <CryAnimation/ICryAnimation.h>

IParticleEmitter* EntityEffects::SpawnParticleFX( const char* effectName, const EntityEffects::SEffectSpawnParams& spawnParams, const char* requester /*= NULL*/)
{
	IParticleEffect* pParticleEffect = gEnv->pParticleManager->FindEffect(effectName, requester ? requester : "");
	
	return SpawnParticleFX(pParticleEffect, spawnParams);
}

IParticleEmitter* EntityEffects::SpawnParticleFX( IParticleEffect* pParticleEffect, const EntityEffects::SEffectSpawnParams& spawnParams)
{
	if (pParticleEffect)
	{
		SpawnParams sp;
		sp.bPrime = spawnParams.prime;
		IParticleEmitter* pEmitter = pParticleEffect->Spawn(ParticleLoc(spawnParams.position, spawnParams.direction, spawnParams.scale), &sp);

		if (spawnParams.speed > 0.0f)
		{
			ParticleParams particleParams = pParticleEffect->GetParticleParams();
			particleParams.fSpeed = spawnParams.speed;
			pParticleEffect->SetParticleParams(particleParams);
		}

		return pEmitter;
	}

	return NULL;
}

void EntityEffects::SpawnParticleWithEntity( const EntityId targetEntityId, const int targetSlot, const char* effectName, const char* helperName, const EntityEffects::SEffectSpawnParams& spawnParams )
{
	IEntity* pTargetEntity = gEnv->pEntitySystem->GetEntity(targetEntityId);
	IParticleEffect* pParticleEffect = gEnv->pParticleManager->FindEffect(effectName);

	SpawnParticleWithEntity(pTargetEntity, targetSlot, pParticleEffect, helperName, spawnParams);
}

void EntityEffects::SpawnParticleWithEntity( const IEntity* pTargetEntity, const int targetSlot, const char* effectName, const char* helperName, const EntityEffects::SEffectSpawnParams& spawnParams )
{
	IParticleEffect* pParticleEffect = gEnv->pParticleManager->FindEffect(effectName);

	SpawnParticleWithEntity(pTargetEntity, targetSlot, pParticleEffect, helperName, spawnParams);
}

void EntityEffects::SpawnParticleWithEntity( const IEntity* pTargetEntity, const int targetSlot, IParticleEffect* pParticleEffect, const char* helperName, const EntityEffects::SEffectSpawnParams& spawnParams )
{
	SEffectSpawnParams newSpawnParams = spawnParams;

	if (pTargetEntity)
	{
		SEntitySlotInfo slotInfo;
		if (pTargetEntity->GetSlotInfo(targetSlot, slotInfo))
		{
			if (slotInfo.pStatObj)	
			{
				//Get helper position from static object
				const Vec3 localHelperPosition = slotInfo.pStatObj->GetHelperPos(helperName);
				newSpawnParams.position = pTargetEntity->GetSlotWorldTM(targetSlot).TransformPoint(localHelperPosition);
			}
			else if (slotInfo.pCharacter)	
			{
				//Get helper position from character
				IAttachmentManager *pAttachmentManager = slotInfo.pCharacter->GetIAttachmentManager();
				
				if (IAttachment *pAttachment = pAttachmentManager->GetInterfaceByName(helperName))
				{
					newSpawnParams.position = pAttachment->GetAttWorldAbsolute().t;
				}
				else
				{
					IDefaultSkeleton& rIDefaultSkeleton = slotInfo.pCharacter->GetIDefaultSkeleton();
					ISkeletonPose* pSkeletonPose = slotInfo.pCharacter->GetISkeletonPose();
					
					Vec3 localJointPosition = ZERO;
					const int16 jointId = rIDefaultSkeleton.GetJointIDByName(helperName);
					if (jointId >= 0)
					{
						localJointPosition = pSkeletonPose->GetAbsJointByID(jointId).t;
					}

					newSpawnParams.position = pTargetEntity->GetSlotWorldTM(targetSlot).TransformPoint(localJointPosition);
				}
			}
		}
	}

	SpawnParticleFX(pParticleEffect, newSpawnParams);
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

namespace EntityEffects
{
	CEffectsController::CEffectsController()
		: m_pOwnerEntity(NULL)
		, m_effectGeneratorId(0)
	{
	}

	void CEffectsController::InitWithEntity(IEntity *pEntity)
	{
		CRY_ASSERT_MESSAGE(pEntity, "Init Effect controller with NULL entity, this will crash!");
		CRY_ASSERT_MESSAGE((m_pOwnerEntity == NULL), "Effect controller had already an entity assigned");

		m_pOwnerEntity = pEntity;
	}

	void CEffectsController::FreeAllEffects()
	{
		int index = (int)m_attachedEffects.size() - 1;

		while(index >= 0)
		{
			CRY_ASSERT(index < (int)m_attachedEffects.size());

			DetachEffect(m_attachedEffects[index].id);
			index--;
		}

		stl::free_container(m_attachedEffects);
		m_effectGeneratorId = 0;
	}

	int CEffectsController::FindSafeSlot(int firstSafeSlot)
	{
		int i = -1;
		if(firstSafeSlot >= 0)
		{
			SEntitySlotInfo dummy;
			i = firstSafeSlot;
			while (m_pOwnerEntity->GetSlotInfo(i, dummy))
			{
				i++;
			}
		}

		return i;
	}

	TAttachedEffectId CEffectsController::AttachParticleEffect(IParticleEffect* pParticleEffect, const SEffectAttachParams& attachParams)
	{
		CRY_ASSERT(m_pOwnerEntity);

		if (pParticleEffect)
		{
			SEffectInfo effectInfo;

			int attachSlot = FindSafeSlot(attachParams.firstSafeSlot);

			//Offset particle to desired location
			effectInfo.entityEffectSlot = m_pOwnerEntity->LoadParticleEmitter(attachSlot, pParticleEffect, 0, attachParams.prime, false);
			Matrix34 localEffectMtx(IParticleEffect::ParticleLoc(attachParams.offset, attachParams.direction, attachParams.scale));
			m_pOwnerEntity->SetSlotLocalTM(effectInfo.entityEffectSlot, localEffectMtx);

			++m_effectGeneratorId;
			effectInfo.id = m_effectGeneratorId;
			m_attachedEffects.push_back(effectInfo);

			return m_effectGeneratorId;
		}

		return 0;
	}

	TAttachedEffectId CEffectsController::AttachParticleEffect(const char* effectName, const SEffectAttachParams& attachParams)
	{
		CRY_ASSERT(m_pOwnerEntity);

		IParticleEffect* pParticleEffect = gEnv->pParticleManager->FindEffect(effectName);

		return AttachParticleEffect(pParticleEffect, attachParams);
	}

	TAttachedEffectId CEffectsController::AttachParticleEffect(IParticleEffect* pParticleEffect, const int targetSlot, const char *helperName, const SEffectAttachParams &attachParams)
	{
		CRY_ASSERT(m_pOwnerEntity);

		if (pParticleEffect)
		{
			SEntitySlotInfo slotInfo;
			SEffectInfo effectInfo;

			const bool validSlot = m_pOwnerEntity->GetSlotInfo(targetSlot, slotInfo);

			if (!validSlot || slotInfo.pStatObj)
			{
				//Get helper position on static object (if any)
				Vec3 localHelperPosition = attachParams.offset;
				if (validSlot)
				{
					const Matrix34& localSlotMtx = m_pOwnerEntity->GetSlotLocalTM(targetSlot, false);

					localHelperPosition = slotInfo.pStatObj->GetHelperPos(helperName) + attachParams.offset;
					localHelperPosition = localSlotMtx.TransformPoint(localHelperPosition);
				}

				int attachSlot = FindSafeSlot(attachParams.firstSafeSlot);

				//Offset particle to desired location
				effectInfo.entityEffectSlot = m_pOwnerEntity->LoadParticleEmitter(attachSlot, pParticleEffect, 0, attachParams.prime, false);
				Matrix34 localEffectMtx(IParticleEffect::ParticleLoc(localHelperPosition, attachParams.direction, attachParams.scale));
				m_pOwnerEntity->SetSlotLocalTM(effectInfo.entityEffectSlot, localEffectMtx);

				++m_effectGeneratorId;
				effectInfo.id = m_effectGeneratorId;
				m_attachedEffects.push_back(effectInfo);

				return m_effectGeneratorId;
			}
			else if (slotInfo.pCharacter)
			{
				IAttachmentManager *pAttachmentManager = slotInfo.pCharacter->GetIAttachmentManager();
				IAttachment *pAttachment = pAttachmentManager->GetInterfaceByName(helperName);

				if (pAttachment)
				{
					CEffectAttachment *pEffectAttachment = new CEffectAttachment(pParticleEffect, attachParams.offset, attachParams.direction, attachParams.scale, attachParams.prime);
					pAttachment->AddBinding(pEffectAttachment);
					pEffectAttachment->ProcessAttachment(pAttachment);
				}
				else
				{
					GameWarning("[EntityEffects] Can not attach '%s' to entity '%s', attachment point helper '%s' does not exist", pParticleEffect->GetName(), m_pOwnerEntity->GetName(), helperName); 
					return 0;
				}

				++m_effectGeneratorId;
				effectInfo.id = m_effectGeneratorId;
				effectInfo.characterEffectSlot = targetSlot;
				effectInfo.helperName = helperName;
				m_attachedEffects.push_back(effectInfo);

				return m_effectGeneratorId;
			}
		}

		return 0;
	}

	TAttachedEffectId CEffectsController::AttachParticleEffect(const char *effectName, const int targetSlot, const char *helperName, const SEffectAttachParams &attachParams)
	{
		CRY_ASSERT(m_pOwnerEntity);

		IParticleEffect* pParticleEffect = gEnv->pParticleManager->FindEffect(effectName);

		return AttachParticleEffect(pParticleEffect, targetSlot, helperName, attachParams);
	}

	TAttachedEffectId CEffectsController::AttachLight(const int targetSlot, const char *helperName, const SLightAttachParams &attachParams)
	{
		CRY_ASSERT(m_pOwnerEntity);

		SRenderLight light;
		light.m_Flags |= attachParams.deferred ? DLF_DEFERRED_LIGHT : 0;
		light.m_Flags |= attachParams.castShadows ?  DLF_CASTSHADOW_MAPS : 0;
		light.SetLightColor(ColorF(attachParams.color.x * attachParams.diffuseMultiplier, attachParams.color.y * attachParams.diffuseMultiplier, attachParams.color.z * attachParams.diffuseMultiplier, 1.0f));
		light.SetSpecularMult( (float)__fsel( -attachParams.diffuseMultiplier, attachParams.specularMultiplier, (attachParams.specularMultiplier / (attachParams.diffuseMultiplier + FLT_EPSILON)) ) );
		light.m_nLightStyle = attachParams.style;
		light.SetAnimSpeed(attachParams.animSpeed);
		light.m_fLightFrustumAngle = 45.0f;
		light.SetRadius(attachParams.radius);
		light.m_fLightFrustumAngle = attachParams.projectFov * 0.5f;
		light.m_fHDRDynamic = attachParams.hdrDynamic;
		light.m_nEntityId = m_pOwnerEntity->GetId();

		if (attachParams.projectTexture && attachParams.projectTexture[0])
		{
			light.m_pLightImage = gEnv->pRenderer->EF_LoadTexture(attachParams.projectTexture, 0);

			if (!light.m_pLightImage || !light.m_pLightImage->IsTextureLoaded())
			{
				GameWarning("[EntityEffects] Entity '%s' failed to load projecting light texture '%s'!", m_pOwnerEntity->GetName(), attachParams.projectTexture);
				return 0;
			}
		}

		if ((light.m_pLightImage != NULL) && light.m_pLightImage->IsTextureLoaded())
		{
			light.m_Flags |= DLF_PROJECT;
		}
		else
		{
			if (light.m_pLightImage)
			{
				light.m_pLightImage->Release();
			}
			light.m_pLightImage = NULL;
			light.m_Flags |= DLF_POINT;
		}

		IMaterial* pMaterial = NULL;
		if (attachParams.material && attachParams.material[0])
		{
			pMaterial = gEnv->p3DEngine->GetMaterialManager()->LoadMaterial(attachParams.material);
		}

		SEntitySlotInfo slotInfo;
		SEffectInfo effectInfo;

		const bool validSlot = m_pOwnerEntity->GetSlotInfo(targetSlot, slotInfo);

		if (!validSlot || slotInfo.pStatObj)
		{
			//Get helper position on static object (if any)
			Vec3 helperPos(ZERO);
			Vec3 localHelperPosition = attachParams.offset;

			if (validSlot)
			{
				helperPos = slotInfo.pStatObj->GetHelperPos(helperName);
				
				if (helperPos.IsZero())
				{
					const int childCount = m_pOwnerEntity->GetChildCount();

					for (int i=0;i<childCount;++i)
					{
						if (IEntity* pChild = m_pOwnerEntity->GetChild(i))
						{
							if (IStatObj* statObj = pChild->GetStatObj(targetSlot))
							{
								helperPos = statObj->GetHelperPos(helperName);

								if (!helperPos.IsZero())
								{
									helperPos += pChild->GetPos();
									break;
								}
							}
						}
					}
				}

				localHelperPosition = helperPos + attachParams.offset;
				localHelperPosition = m_pOwnerEntity->GetSlotLocalTM(targetSlot, false).TransformPoint(localHelperPosition);
			}

			int attachSlot = FindSafeSlot(attachParams.firstSafeSlot);

			++m_effectGeneratorId;
			effectInfo.id = m_effectGeneratorId;
			effectInfo.entityEffectSlot = m_pOwnerEntity->LoadLight(attachSlot, &light);

			if ((effectInfo.entityEffectSlot >= 0) && pMaterial)
			{
				m_pOwnerEntity->SetSlotMaterial(effectInfo.entityEffectSlot, pMaterial);
			}

			Matrix34 localEffectMtx = Matrix34(Matrix33::CreateRotationVDir(attachParams.direction));
			localEffectMtx.SetTranslation(localHelperPosition);
			m_pOwnerEntity->SetSlotLocalTM(effectInfo.entityEffectSlot, localEffectMtx);

			m_attachedEffects.push_back(effectInfo);

			return m_effectGeneratorId;
		}
		else if (slotInfo.pCharacter)
		{
			IAttachmentManager *pAttachmentManager = slotInfo.pCharacter->GetIAttachmentManager();
			IAttachment *pAttachment = pAttachmentManager->GetInterfaceByName(helperName);

			if (pAttachment)
			{
				CLightAttachment *pLightAttachment = new CLightAttachment();
				pLightAttachment->LoadLight(light);

				ILightSource* pLightSource = pLightAttachment->GetLightSource();
				if (pLightSource)
				{
					pLightSource->SetMaterial(pMaterial);       
					pLightSource->SetCastingException(attachParams.pCasterException);
				}
				pAttachment->AddBinding(pLightAttachment);
				
				const bool customOffset = (attachParams.offset != Vec3Constants<float>::fVec3_Zero) || (attachParams.direction != Vec3Constants<float>::fVec3_OneY);
				if (customOffset)
				{
					pAttachment->SetAttRelativeDefault(QuatT(Quat::CreateRotationVDir(attachParams.direction), attachParams.offset));
				}
			}
			else
			{
				GameWarning("[EntityEffects] Entity '%s' trying to attach light to attachment '%s' which does not exist!", m_pOwnerEntity->GetName(), helperName);
				return 0;
			}

			++m_effectGeneratorId;
			effectInfo.id = m_effectGeneratorId;
			effectInfo.helperName = helperName;
			effectInfo.characterEffectSlot = targetSlot;

			m_attachedEffects.push_back(effectInfo);

			return m_effectGeneratorId;
		}

		return 0;
	}

	void CEffectsController::DetachEffect(const TAttachedEffectId effectId)
	{
		CRY_ASSERT(m_pOwnerEntity);

		TAttachedEffects::iterator effectIt = std::find(m_attachedEffects.begin(), m_attachedEffects.end(), effectId);

		if (effectIt != m_attachedEffects.end())
		{
			const SEffectInfo& effectInfo = *effectIt;

			if (effectInfo.entityEffectSlot >= 0)
			{
				m_pOwnerEntity->FreeSlot(effectInfo.entityEffectSlot);
			}	
			else
			{
				ICharacterInstance *pCharacter = m_pOwnerEntity->GetCharacter(effectInfo.characterEffectSlot);
				if (pCharacter)
				{
					IAttachmentManager *pAttachmentManager = pCharacter->GetIAttachmentManager();
					IAttachment *pAttachment = pAttachmentManager->GetInterfaceByName(effectInfo.helperName.c_str());
					if(pAttachment)
					{
						pAttachment->ClearBinding();
					}
				}
			}

			m_attachedEffects.erase(effectIt);
		}
	}

	IParticleEmitter* CEffectsController::GetEffectEmitter(const TAttachedEffectId effectId) const
	{
		CRY_ASSERT(m_pOwnerEntity);

		TAttachedEffects::const_iterator effectCit = std::find(m_attachedEffects.begin(), m_attachedEffects.end(), effectId);

		if (effectCit != m_attachedEffects.end()) 
		{
			const SEffectInfo &effectInfo = *effectCit;

			if (effectInfo.entityEffectSlot >= 0)
			{
				SEntitySlotInfo slotInfo;
				if(m_pOwnerEntity->GetSlotInfo(effectInfo.entityEffectSlot, slotInfo) && slotInfo.pParticleEmitter)
				{
					return slotInfo.pParticleEmitter;
				}
			}

			if (effectInfo.characterEffectSlot >= 0)
			{
				SEntitySlotInfo slotInfo;
				if (m_pOwnerEntity->GetSlotInfo(effectInfo.characterEffectSlot, slotInfo) && slotInfo.pCharacter)
				{
					IAttachmentManager *pAttachmentManager = slotInfo.pCharacter->GetIAttachmentManager();
					IAttachment *pAttachment = pAttachmentManager->GetInterfaceByName(effectInfo.helperName.c_str());
					if (pAttachment)
					{
						IAttachmentObject *pAttachmentObject = pAttachment->GetIAttachmentObject();
						if (pAttachmentObject != NULL && (pAttachmentObject->GetAttachmentType() == IAttachmentObject::eAttachment_Effect))
						{
							return static_cast<CEffectAttachment *>(pAttachmentObject)->GetEmitter();
						}
					}
				}
			}
		}

		return NULL;
	}

	ILightSource* CEffectsController::GetLightSource(const TAttachedEffectId effectId) const
	{
		CRY_ASSERT(m_pOwnerEntity);

		TAttachedEffects::const_iterator effectCit = std::find(m_attachedEffects.begin(), m_attachedEffects.end(), effectId);

		if (effectCit != m_attachedEffects.end()) 
		{
			const SEffectInfo &effectInfo = *effectCit;

			if (effectInfo.entityEffectSlot >= 0)
			{
				SEntitySlotInfo slotInfo;
				if(m_pOwnerEntity->GetSlotInfo(effectInfo.entityEffectSlot, slotInfo) && slotInfo.pLight)
				{
					return slotInfo.pLight;
				}
			}

			if (effectInfo.characterEffectSlot >= 0)
			{
				SEntitySlotInfo slotInfo;
				if (m_pOwnerEntity->GetSlotInfo(effectInfo.characterEffectSlot, slotInfo) && slotInfo.pCharacter)
				{
					IAttachmentManager *pAttachmentManager = slotInfo.pCharacter->GetIAttachmentManager();
					IAttachment *pAttachment = pAttachmentManager->GetInterfaceByName(effectInfo.helperName.c_str());
					if (pAttachment)
					{
						IAttachmentObject *pAttachmentObject = pAttachment->GetIAttachmentObject();
						if (pAttachmentObject != NULL && (pAttachmentObject->GetAttachmentType() == IAttachmentObject::eAttachment_Light))
						{
							return static_cast<CLightAttachment *>(pAttachmentObject)->GetLightSource();
						}
					}
				}
			}
		}

		return NULL;
	}

	void CEffectsController::GetMemoryStatistics( ICrySizer* pSizer ) const
	{
		pSizer->AddContainer(m_attachedEffects);
	}

	void CEffectsController::SetEffectWorldTM( const TAttachedEffectId effectId, const Matrix34& effectWorldTM )
	{
		CRY_ASSERT(m_pOwnerEntity);

		TAttachedEffects::const_iterator effectCit = std::find(m_attachedEffects.begin(), m_attachedEffects.end(), effectId);

		if (effectCit != m_attachedEffects.end())
		{
			const SEffectInfo &effectInfo = *effectCit;
			SEntitySlotInfo slotInfo;

			if (effectInfo.entityEffectSlot >= 0)
			{
				if (m_pOwnerEntity->GetSlotInfo(effectInfo.entityEffectSlot, slotInfo) && (slotInfo.pParticleEmitter||slotInfo.pLight))
				{
					const Matrix34& worldMatrix = m_pOwnerEntity->GetWorldTM();
					Matrix34 localMatrix = worldMatrix.GetInverted() * effectWorldTM;

					m_pOwnerEntity->SetSlotLocalTM(effectInfo.entityEffectSlot, localMatrix);
				}
			}
		}
	}

	void CEffectsController::UpdateEntitySlotEffectLocationsFromHelpers()
	{
		const int numEffects = m_attachedEffects.size();

		for(int i = 0; i < numEffects; i++)
		{
			SEffectInfo& effectInfo = m_attachedEffects[i];
			
			if(effectInfo.entityEffectSlot >= 0 && effectInfo.characterEffectSlot >= 0 && !effectInfo.helperName.empty())
			{
				SEntitySlotInfo slotInfo;
				m_pOwnerEntity->GetSlotInfo(effectInfo.characterEffectSlot, slotInfo);
				if(slotInfo.pStatObj)
				{
					Matrix34 localMatrix = m_pOwnerEntity->GetSlotLocalTM(effectInfo.characterEffectSlot, false) * slotInfo.pStatObj->GetHelperTM(effectInfo.helperName.c_str());
					m_pOwnerEntity->SetSlotLocalTM(effectInfo.entityEffectSlot, localMatrix);
				}
			}
		}
	}
};

