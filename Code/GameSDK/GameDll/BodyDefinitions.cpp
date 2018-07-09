// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "BodyDefinitions.h"

CBodyDestrutibilityInstance::~CBodyDestrutibilityInstance()
{
	CleanUpOriginalMaterials();
	DeleteMikeAttachmentEntity();
}

void CBodyDestrutibilityInstance::ReserveSlots( const int totalAttachmentsCount, const int destructibleAttachmentsCount, const int destructibleBonesCount, const int destructionEventsCount )
{
	CRY_ASSERT(destructibleAttachmentsCount >= 0);
	CRY_ASSERT(destructibleBonesCount >= 0);
	CRY_ASSERT(destructionEventsCount >= 0);

	m_attachmentStatus.reserve(destructibleAttachmentsCount);
	m_boneStatus.reserve(destructibleBonesCount);

	m_availableDestructionEvents.resize(destructionEventsCount, true);

	m_originalMaterials.reserve( totalAttachmentsCount );

	DeleteMikeAttachmentEntity();
}

void CBodyDestrutibilityInstance::Reset()
{
	for (uint32 i = 0; i < m_attachmentStatus.size(); ++i)
	{
		m_attachmentStatus[i].Reset();
	}

	for (uint32 i = 0; i < m_boneStatus.size(); ++i)
	{
		m_boneStatus[i].Reset();
	}

	for (uint32 i = 0; i < m_availableDestructionEvents.size(); ++i)
	{
		m_availableDestructionEvents[i] = true;
	}

	m_eventsModified = false;
	m_lastEventForHitReactionsCrc = 0;
	m_currentHealthRatioEventIdx = 0;

	CleanUpOriginalMaterials();
	DeleteMikeAttachmentEntity();
}

void CBodyDestrutibilityInstance::DeleteMikeAttachmentEntity()
{
	if (m_mikeAttachmentEntityId)
	{
		gEnv->pEntitySystem->RemoveEntity(m_mikeAttachmentEntityId);
		m_mikeAttachmentEntityId = 0;
	}
}

void CBodyDestrutibilityInstance::InitWithProfileId( const TBodyDestructibilityProfileId profileId )
{
	m_id = profileId;
	m_attachmentStatus.clear();
	m_boneStatus.clear();
	m_availableDestructionEvents.clear();
}

void CBodyDestrutibilityInstance::InitializeMikeDeath( const EntityId entityId, float alphaTestFadeOutTime, float alphaTestFadeOutDelay, float alphaTestMax )
{
	CRY_ASSERT(m_mikeAttachmentEntityId == 0);

	m_mikeAttachmentEntityId = entityId;
	m_mikeExplodeAlphaTestFadeOutTimer = alphaTestFadeOutTime + alphaTestFadeOutDelay;
	m_mikeExplodeAlphaTestFadeOutScale = -(1.0f / alphaTestFadeOutTime);
	m_mikeExplodeAlphaTestMax = alphaTestMax;
}

void CBodyDestrutibilityInstance::ReplaceMaterial( IEntity& characterEntity, ICharacterInstance& characterInstance, IMaterial& replacementMaterial )
{
	IMaterial* pCurrentMaterial = characterInstance.GetIMaterial();
	if ((pCurrentMaterial != NULL) && stricmp(pCurrentMaterial->GetName(), replacementMaterial.GetName()))
	{
		characterEntity.SetSlotMaterial(0, &replacementMaterial);
	}

	const bool storeOriginalReplacementMaterials = m_originalMaterials.empty();

	IAttachmentManager *pAttachmentManager = characterInstance.GetIAttachmentManager();
	CRY_ASSERT(pAttachmentManager);

	const int attachmentCount = pAttachmentManager->GetAttachmentCount();
	for (int attachmentIdx = 0; attachmentIdx < attachmentCount; ++attachmentIdx)
	{
		IAttachmentObject *pAttachmentObject = pAttachmentManager->GetInterfaceByIndex(attachmentIdx)->GetIAttachmentObject();
		if (pAttachmentObject)
		{
			IMaterial* pAttachMaterial = (IMaterial*)pAttachmentObject->GetBaseMaterial();
			if ((pAttachMaterial != NULL) && stricmp(pAttachMaterial->GetName(), replacementMaterial.GetName()))
			{
				if (storeOriginalReplacementMaterials)
				{
					IMaterial* pOriginalReplacementMaterial = pAttachmentObject->GetReplacementMaterial();
					if(pOriginalReplacementMaterial != NULL)
					{
						pOriginalReplacementMaterial->AddRef();
						m_originalMaterials.push_back(TAttachmentMaterialPair((uint32)attachmentIdx, pOriginalReplacementMaterial));
					}
				}

				pAttachmentObject->SetReplacementMaterial(&replacementMaterial);
			}
		}

	}
}

void CBodyDestrutibilityInstance::ResetMaterials( IEntity& characterEntity, ICharacterInstance& characterInstance )
{
	characterEntity.SetSlotMaterial(0, NULL);

	IAttachmentManager *pAttachmentManager = characterInstance.GetIAttachmentManager();
	CRY_ASSERT(pAttachmentManager);
	
	const uint32 attachmentCount = (uint32)pAttachmentManager->GetAttachmentCount();

	for(TOriginalMaterials::iterator it = m_originalMaterials.begin(); it != m_originalMaterials.end(); ++it)
	{
		IAttachmentObject *pAttachmentObject = (it->first < attachmentCount) ? pAttachmentManager->GetInterfaceByIndex(it->first)->GetIAttachmentObject() : NULL;
		if (pAttachmentObject)
		{
			pAttachmentObject->SetReplacementMaterial(it->second);
			it->second->Release();
		}
	}

	m_originalMaterials.clear();
}

void CBodyDestrutibilityInstance::Update( float frameTime )
{
	if (m_mikeAttachmentEntityId && m_mikeExplodeAlphaTestFadeOutTimer > 0.0f)
	{
		m_mikeExplodeAlphaTestFadeOutTimer -= min(frameTime, m_mikeExplodeAlphaTestFadeOutTimer);

		float alphaTestValue = max(min(m_mikeExplodeAlphaTestFadeOutTimer*m_mikeExplodeAlphaTestFadeOutScale+1.0f, 1.0f), 0.0f) * m_mikeExplodeAlphaTestMax;
		IEntity* pJellyEntity = gEnv->pEntitySystem->GetEntity(m_mikeAttachmentEntityId);
		ICharacterInstance* pJellyCharacter = pJellyEntity ? pJellyEntity->GetCharacter(0) : 0;
		IMaterial* pJellyMaterial = pJellyCharacter ? pJellyCharacter->GetIMaterial() : 0;
		if (pJellyMaterial)
		{
			int numSubMaterials = pJellyMaterial->GetSubMtlCount();
			for (int i = 0; i < numSubMaterials; ++i)
			{
				IMaterial* pSubJellyMaterial = pJellyMaterial->GetSubMtl(i);
				pSubJellyMaterial->SetGetMaterialParamFloat("alpha", alphaTestValue, false);
			}
		}
	}
}

void CBodyDestrutibilityInstance::CleanUpOriginalMaterials()
{
	for(TOriginalMaterials::iterator it = m_originalMaterials.begin(); it != m_originalMaterials.end(); ++it)
	{
		it->second->Release();
	}

	m_originalMaterials.clear();
}