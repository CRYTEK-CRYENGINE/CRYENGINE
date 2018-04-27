// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "Utility/AttachmentUtils.h"
#include <CryEntitySystem/IEntity.h>
#include <CryEntitySystem/IEntityComponent.h>
#include <CryAnimation/ICryAnimation.h>
#include <CryAnimation/IAttachment.h>
#include "RecordingSystem.h"
#include "EntityUtility/EntityEffectsCloak.h"

namespace AttachmentUtils
{
	int FindFirstRenderedSlot( const IEntity* pEntity )
	{
		IF_UNLIKELY(pEntity == NULL)
			return -1;

		int slot = -1;
		const int objectSlots = pEntity->GetSlotCount();
		for (int i = 0 ; i < objectSlots; ++i)
		{
			if (pEntity->GetSlotFlags(i)&ENTITY_SLOT_RENDER)
			{
				slot = i;
				break;
			}
		}

		return slot;
	}

	bool IsEntityRenderedInNearestMode( const IEntity* pEntity )
	{
		if(pEntity)
		{
			const int slotIndex = 0;
			const uint32 slotFlags = pEntity->GetSlotFlags(slotIndex);

			return ((slotFlags & ENTITY_SLOT_RENDER_NEAREST) != 0);
		}
		return false;
	}

	void SetEntityRenderInNearestMode( IEntity* pEntity, const bool drawNear )
	{
		IF_UNLIKELY (pEntity == NULL)
			return;

		const int nslots = pEntity->GetSlotCount();
		for (int i = 0; i < nslots; ++i)
		{
			const uint32 slotFlags = pEntity->GetSlotFlags(i);
			if(slotFlags & ENTITY_SLOT_RENDER)
			{
				const uint32 newSlotFlags = drawNear ? (slotFlags|ENTITY_SLOT_RENDER_NEAREST) : slotFlags&(~ENTITY_SLOT_RENDER_NEAREST);
				pEntity->SetSlotFlags(i, newSlotFlags);
			}
		}

		if(IEntityRender* pProxy = pEntity->GetRenderInterface())
		{
			if(IRenderNode* pRenderNode = pProxy->GetRenderNode())
			{
				pRenderNode->SetRndFlags(ERF_REGISTER_BY_POSITION, drawNear);
			}
		}
	}

	void SyncRenderInNearestModeWithEntity( IEntity* pMasterEntity, IEntity* pSlaveEntity )
	{
		SetEntityRenderInNearestMode( pSlaveEntity, IsEntityRenderedInNearestMode(pMasterEntity) );
	}

	void SyncCloakWithEntity( const EntityId masterEntityId, const IEntity* pSlaveEntity, const bool forceDecloak /*= false*/ )
	{
		const EntityEffects::Cloak::CloakSyncParams cloakParams( masterEntityId, pSlaveEntity->GetId(), true, forceDecloak );
		const bool bObjCloaked = EntityEffects::Cloak::CloakSyncEntities( cloakParams ); 

		if(CRecordingSystem *pRecordingSystem = g_pGame->GetRecordingSystem())
		{
			pRecordingSystem->OnObjectCloakSync( cloakParams.cloakSlaveId, cloakParams.cloakMasterId, bObjCloaked, true);
		}

		const int children = pSlaveEntity->GetChildCount();
		for(int i = 0; i < children; ++i)
		{
			SyncCloakWithEntity( masterEntityId, pSlaveEntity->GetChild(i), forceDecloak );
		}
	}

	void SetupShadowAttachmentBinding( IEntity* pPlayerEntity, IEntity* pObjectEntity, const uint32 attNameCRC32Lower, const int shadowSlotIndex /*= kDefaultPlayerShadowSlot*/, const bool bAllowCharacters /*= true*/ )
	{
		IAttachment* pShadowAttachment = GetAttachment(pPlayerEntity, attNameCRC32Lower, shadowSlotIndex);
		if(!pShadowAttachment)
			return;

		const int firstRenderedSlot = FindFirstRenderedSlot(pObjectEntity);
		if(firstRenderedSlot<0)
			return;

		SEntitySlotInfo objectSlotInfo;
		if(pObjectEntity->GetSlotInfo(firstRenderedSlot, objectSlotInfo))
		{
			// Use a CSKELAttachment/CCGFAttachment for Shadow Attachments.
			if(bAllowCharacters && objectSlotInfo.pCharacter)
			{
				CSKELAttachment* pSkelAttachment = new CSKELAttachment();
				pSkelAttachment->m_pCharInstance = objectSlotInfo.pCharacter;
				pShadowAttachment->AddBinding( pSkelAttachment );
			}
			else if(objectSlotInfo.pStatObj)
			{
				CCGFAttachment* pStatObjAttachment = new CCGFAttachment();
				pStatObjAttachment->pObj = objectSlotInfo.pStatObj;
				pShadowAttachment->AddBinding( pStatObjAttachment );
			}

			pShadowAttachment->HideAttachment(1);
			pShadowAttachment->HideInShadow(0);
		}
	}

	void SetupEntityAttachmentBinding( IAttachment* pPlayerAttachment, const EntityId objectEntityId )
	{
		CEntityAttachment* pEntityAttachment = new CEntityAttachment();
		pEntityAttachment->SetEntityId( objectEntityId );
		pPlayerAttachment->AddBinding( pEntityAttachment );
	}

	IAttachment* GetAttachment( IEntity* pEntity, const char* pAttachmentName, const int slotIndex )
	{
		const uint32 crc = CCrc32::ComputeLowercase(pAttachmentName);
		return GetAttachment( pEntity, crc, slotIndex );
	}

	IAttachment* GetAttachment( IEntity* pEntity, const uint32 attNameCRC32Lower, const int slotIndex )
	{
		SEntitySlotInfo info;
		if( pEntity && pEntity->GetSlotInfo(slotIndex, info) && info.pCharacter )
		{
			return info.pCharacter->GetIAttachmentManager()->GetInterfaceByNameCRC( attNameCRC32Lower );
		}
		return NULL;
	}

	void AttachObject( const bool bFirstPerson, IEntity* pPlayerEntity, IEntity* pObjectEntity, const char* pAttachmentName, const TAttachmentFlags flags /*= eAF_Default*/, const int playerMainSlot /*= kDefaultPlayerMainSlot*/, const int playerShadowSlot /*= kDefaultPlayerShadowSlot*/ )
	{
		const uint32 crc = CCrc32::ComputeLowercase(pAttachmentName);
		AttachObject( bFirstPerson, pPlayerEntity, pObjectEntity, crc, flags, playerMainSlot, playerShadowSlot );
	}

	void AttachObject( const bool bFirstPerson, IEntity* pPlayerEntity, IEntity* pObjectEntity, const uint32 attNameCRC32Lower, const TAttachmentFlags flags /*= eAF_Default*/, const int playerMainSlot /*= kDefaultPlayerMainSlot*/, const int playerShadowSlot /*= kDefaultPlayerShadowSlot*/ )
	{
		IAttachment* pPlayerMainAttachment = GetAttachment(pPlayerEntity, attNameCRC32Lower, playerMainSlot);
		if(!pPlayerMainAttachment)
			return;

		// Sync the RenderNearest flag.
		if(flags&eAF_SyncRenderNearest)
		{
			SyncRenderInNearestModeWithEntity( pPlayerEntity, pObjectEntity );
		}

		// Sync the cloaking.
		if(flags&eAF_SyncCloak)
		{
			SyncCloakWithEntity( pPlayerEntity->GetId(), pObjectEntity );
		}

		// Setup the EntityAttachment.
		SetupEntityAttachmentBinding( pPlayerMainAttachment, pObjectEntity->GetId() );

		if(bFirstPerson)
		{
			// Setup Shadow Attachment.
			SetupShadowAttachmentBinding( pPlayerEntity, pObjectEntity, attNameCRC32Lower, playerShadowSlot, (flags&eAF_AllowShadowCharAtt)!=0 );
		}

		// Show newly attached attachment.
		pPlayerMainAttachment->HideAttachment(0);
	}

	void DetachObject( IEntity* pPlayerEntity, IEntity* pObjectEntity, const char* pAttachmentName, const TAttachmentFlags flags /*= eAF_Default*/, const int playerMainSlot /*= kDefaultPlayerMainSlot*/, const int playerShadowSlot /*= kDefaultPlayerShadowSlot*/ )
	{
		const uint32 crc = CCrc32::ComputeLowercase(pAttachmentName);
		DetachObject( pPlayerEntity, pObjectEntity, crc, flags, playerMainSlot, playerShadowSlot );
	}

	void DetachObject( IEntity* pPlayerEntity, IEntity* pObjectEntity, const uint32 attNameCRC32Lower, const TAttachmentFlags flags /*= eAF_Default*/, const int playerMainSlot /*= kDefaultPlayerMainSlot*/, const int playerShadowSlot /*= kDefaultPlayerShadowSlot*/ )
	{
		if(!pPlayerEntity)
			return;

		// Set the RenderNearest flag to false.
		if(flags&eAF_SyncRenderNearest)
		{
			SetEntityRenderInNearestMode( pObjectEntity, false );
		}

		// Force de-cloak.
		if(pObjectEntity && (flags&eAF_SyncCloak))
		{
			SyncCloakWithEntity( pPlayerEntity->GetId(), pObjectEntity, true );
		}

		// Clear the Main attachment binding.
		if(IAttachment* pPlayerMainAttachment = GetAttachment(pPlayerEntity, attNameCRC32Lower, playerMainSlot))
		{
			pPlayerMainAttachment->ClearBinding();
			pPlayerMainAttachment->HideInShadow(0);
		}

		// Clear the Shadow attachment binding.
		if(IAttachment* pPlayerShadowAttachment = GetAttachment(pPlayerEntity, attNameCRC32Lower, playerShadowSlot))
		{
			pPlayerShadowAttachment->ClearBinding();
		}
	}


}
