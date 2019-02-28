// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "AICorpse.h"

#include <CryAnimation/ICryAnimation.h>
#include <IItemSystem.h>

#include "Game.h"
#include "WeaponSystem.h"
#include "Actor.h"

#include "GameCVars.h"

#define AI_CORPSES_MINIMUM 4

namespace
{
	bool AllowCloneAttachedItem( const IEntityClass* pItemClass )
	{
		static const uint32 kAllowedClassesCount = 5;
		static const IEntityClass* allowedClassTable[kAllowedClassesCount] = 
		{
			gEnv->pEntitySystem->GetClassRegistry()->FindClass("Heavy_minigun"),
			gEnv->pEntitySystem->GetClassRegistry()->FindClass("Heavy_mortar"),
			gEnv->pEntitySystem->GetClassRegistry()->FindClass("LightningGun"),
		};

		for(uint32 i = 0; i < kAllowedClassesCount; ++i)
		{
			if(allowedClassTable[i] == pItemClass)
				return true;
		}

		return false;
	}

	bool ShouldDropOnCorpseRemoval( const IEntityClass* pItemClass )
	{
		static IEntityClass* pLightingGunClass = gEnv->pEntitySystem->GetClassRegistry()->FindClass("LightningGun");

		return (pLightingGunClass == pItemClass);
	}

	void RegisterEvents( IGameObjectExtension& goExt, IGameObject& gameObject )
	{
		const int eventID = eCGE_ItemTakenFromCorpse;
		gameObject.UnRegisterExtForEvents( &goExt, NULL, 0 );
		gameObject.RegisterExtForEvents( &goExt, &eventID, 1 );
	}

	struct SCorpseRemovalScore
	{
		SCorpseRemovalScore( const EntityId _corpseId )
			: corpseId(_corpseId)
			, distance(10000000000.0f)
			, upClose(0)
			, farAway(0)
			, priority(0)
			, visible(0)
		{

		}

		EntityId corpseId;
		float    distance;
		uint8    upClose;
		uint8    farAway;
		uint8	   priority;
		uint8    visible;

		bool operator<(const SCorpseRemovalScore& otherCorpse) const
		{
			// 1- Far away go first
			if(farAway > otherCorpse.farAway)
				return true;
			
			if (farAway < otherCorpse.farAway)
				return false;

			// 2- In between up-close and far away next
			if(upClose < otherCorpse.upClose)
				return true;

			if(upClose > otherCorpse.upClose)
				return false;

			// 3- Within mid range non visible go first (unless priority high)
			if(visible < otherCorpse.visible)
				return true;

			if(visible > otherCorpse.visible)
				return false;

			// 4- If everything else fails short by priority and distance
			if(priority < otherCorpse.priority)
				return true;

			if(priority > otherCorpse.priority)
				return false;

			if (distance > otherCorpse.distance)
				return true;

			return false;
		}
	};
}

CAICorpse::CAICorpse()
{
}

CAICorpse::~CAICorpse()
{
	CAICorpseManager* pAICorpseManager = CAICorpseManager::GetInstance();
	if(pAICorpseManager != NULL)
	{
		pAICorpseManager->UnregisterAICorpse( GetEntityId() );
	}

	for(uint32 i = 0; i < m_attachedItemsInfo.size(); ++i)
	{
		if( m_attachedItemsInfo[i].id )
		{
			gEnv->pEntitySystem->RemoveEntity( m_attachedItemsInfo[i].id );
		}
	}
}

bool CAICorpse::Init( IGameObject * pGameObject )
{
	SetGameObject(pGameObject);

	CAICorpseManager* pAICorpseManager = CAICorpseManager::GetInstance();
	assert(pAICorpseManager != NULL);

	pAICorpseManager->RegisterAICorpse( GetEntityId(), m_priority );

	return true;
}

void CAICorpse::PostInit( IGameObject * pGameObject )
{
	RegisterEvents( *this, *pGameObject );
}

bool CAICorpse::ReloadExtension( IGameObject * pGameObject, const SEntitySpawnParams &params )
{
	ResetGameObject();

	RegisterEvents( *this, *pGameObject );

	CRY_ASSERT_MESSAGE(false, "CAICorpse::ReloadExtension not implemented");

	return false;
}

bool CAICorpse::GetEntityPoolSignature( TSerialize signature )
{
	CRY_ASSERT_MESSAGE(false, "CAICorpse::GetEntityPoolSignature not implemented");

	return true;
}

void CAICorpse::Release()
{
	delete this;
}

void CAICorpse::FullSerialize( TSerialize ser )
{
#if AI_CORPSES_ENABLE_SERIALIZE
	ser.Value("modelName", m_modelName);
	ser.Value("priority", m_priority);

	uint32 attachedWeaponsCount = m_attachedItemsInfo.size();
	ser.Value("attachedWeaponsCount", attachedWeaponsCount);

	stack_string weaponGroup;

	if(ser.IsReading())
	{
		// Restore the model/physics
		/*
		GetEntity()->LoadCharacter( 0, m_modelName.c_str() );
		
				SEntityPhysicalizeParams physicalizeParams;
				physicalizeParams.type = PE_ARTICULATED;
				physicalizeParams.nSlot = 0;
		
				GetEntity()->Physicalize(physicalizeParams);
		
				ICharacterInstance* pCharacterInstance = GetEntity()->GetCharacter(0);
				if (ser.BeginOptionalGroup("character", pCharacterInstance != NULL))
				{
					assert(pCharacterInstance != NULL);
					pCharacterInstance->Serialize(ser);
		
					ser.EndGroup();
				}
		
				IEntityPhysicalProxy* pPhysicsProxy = static_cast<IEntityPhysicalProxy*>(GetEntity()->GetPhysicsInterface());
				if(ser.BeginOptionalGroup("characterPhysics", pPhysicsProxy != NULL))
				{
					assert(pPhysicsProxy != NULL);
					GetEntity()->Serialize(ser);
		
					ser.EndGroup();
				}*/
		
		
		m_attachedItemsInfo.clear();

		for(uint32 i = 0; i < attachedWeaponsCount; ++i)
		{
			weaponGroup.Format("Weapon_%d", i);
			ser.BeginGroup( weaponGroup.c_str() );
			{
				string className;
				string attachmentName;
				ser.Value( "weaponClass", className );
				ser.Value( "weaponAttachment", attachmentName );

				IEntityClass* pEntityClass = gEnv->pEntitySystem->GetClassRegistry()->FindClass( className.c_str() );
				if(pEntityClass != NULL)
				{
					AttachedItem attachedWeaponInfo;
					attachedWeaponInfo.pClass = pEntityClass;
					attachedWeaponInfo.attachmentName = attachmentName.c_str();
					m_attachedItemsInfo.push_back(attachedWeaponInfo);
				}
			}
			ser.EndGroup();
		}
	}
	else
	{
	/*
		ICharacterInstance* pCharacterInstance = GetEntity()->GetCharacter(0);
			if (ser.BeginOptionalGroup("character", pCharacterInstance != NULL))
			{
				assert(pCharacterInstance != NULL);
				pCharacterInstance->Serialize(ser);
	
				ser.EndGroup();
			}
	
			if(ser.BeginOptionalGroup("characterPhysics", GetEntity()->GetPhysicalEntity() != NULL))
			{
				assert(pPhysicsProxy != NULL);
				GetEntity()->Serialize(ser);
	
				ser.EndGroup();
			}*/
	

		for(uint32 i = 0; i < attachedWeaponsCount; ++i)
		{
			weaponGroup.Format("Weapon_%d", i);
			ser.BeginGroup( weaponGroup.c_str() );
			{
				string className = m_attachedItemsInfo[i].pClass->GetName();
				string attachmentName = m_attachedItemsInfo[i].attachmentName;
				ser.Value( "weaponClass", className );
				ser.Value( "weaponAttachment", attachmentName );
			}
			ser.EndGroup();
		}
	}
#endif //AI_CORPSES_ENABLE_SERIALIZE
}

void CAICorpse::PostSerialize()
{
#if AI_CORPSES_ENABLE_SERIALIZE
	ICharacterInstance* pCharacterInstance = GetEntity()->GetCharacter(0);
	if(pCharacterInstance != NULL)
	{
		IAttachmentManager* pAttachmentManager = pCharacterInstance->GetIAttachmentManager();
		for(uint32 i = 0; i < m_attachedItemsInfo.size(); ++i)
		{
			AttachedItem& weaponInfo = m_attachedItemsInfo[i];
			IAttachment* pAttachment = pAttachmentManager->GetInterfaceByName( weaponInfo.attachmentName.c_str() );
			if(pAttachment != NULL)
			{
				weaponInfo.id = CloneAttachedItem( weaponInfo, pAttachment );
			}
		}
	}
#endif //AI_CORPSES_ENABLE_SERIALIZE
}

void CAICorpse::HandleEvent( const SGameObjectEvent& gameObjectEvent )
{
	if(gameObjectEvent.event == eCGE_ItemTakenFromCorpse)
	{
		assert(gameObjectEvent.param != NULL);

		bool matchFound = false;
		const EntityId itemId = *static_cast<const EntityId*>(gameObjectEvent.param);

		for (uint32 i = 0; i < m_attachedItemsInfo.size(); ++i)
		{
			if(m_attachedItemsInfo[i].id == itemId)
			{
				ICharacterInstance* pCharacter = GetEntity()->GetCharacter(0);
				if(pCharacter != NULL)
				{
					IAttachment* pAttachment = pCharacter->GetIAttachmentManager()->GetInterfaceByName( m_attachedItemsInfo[i].attachmentName.c_str() );
					if(pAttachment != NULL)
					{
						pAttachment->ClearBinding();
					}
				}

				m_attachedItemsInfo.removeAt(i);
				matchFound = true;
				break;
			}
		}

		if(matchFound)
		{
			if(m_attachedItemsInfo.empty())
			{
				m_priority = 0; //Lower the priority once all attached items have been removed
			}
		}
	}
}

void CAICorpse::GetMemoryUsage( ICrySizer *pSizer ) const
{

}

void CAICorpse::SetupFromSource( IEntity& sourceEntity, ICharacterInstance& characterInstance, const uint32 priority)
{
	// 1.- Move resources from source entity, into AICorpse
	GetEntity()->SetFlags(GetEntity()->GetFlags() | (ENTITY_FLAG_CASTSHADOW));

	sourceEntity.MoveSlot(GetEntity(), 0);

	// Moving everything from one slot into another will also clear the render proxies in the source.
	// Thus, we need to invalidate the model so that it will be properly reloaded when a non-pooled
	// entity is restored from a save-game.
	CActor* sourceActor = static_cast<CActor*>(gEnv->pGameFramework->GetIActorSystem()->GetActor(sourceEntity.GetId()));
	if (sourceActor != NULL)
	{
		sourceActor->InvalidateCurrentModelName();
	}

	// 2.- After 'MoveSlot()', characterInstance is now stored inside CAICorpse
	// It needs to be now updated from the entity system
	characterInstance.SetFlags( characterInstance.GetFlags() | CS_FLAG_UPDATE );

#if AI_CORPSES_ENABLE_SERIALIZE
	m_modelName = characterInstance.GetFilePath();
#endif

	// 3.- Search for any attached weapon and clone them
	IItemSystem* pItemSystem = g_pGame->GetIGameFramework()->GetIItemSystem();

	IAttachmentManager* pAttachmentManager = characterInstance.GetIAttachmentManager();
	const uint32 attachmentCount = (uint32)pAttachmentManager->GetAttachmentCount();
	for(uint32 i = 0; i < attachmentCount ; ++i)
	{
		IAttachment* pAttachment = pAttachmentManager->GetInterfaceByIndex(i);
		assert(pAttachment != NULL);

		IAttachmentObject* pAttachmentObject = pAttachment->GetIAttachmentObject();
		if((pAttachmentObject == NULL) || (pAttachmentObject->GetAttachmentType() != IAttachmentObject::eAttachment_Entity))
			continue;

		const EntityId attachedEntityId = static_cast<CEntityAttachment*>(pAttachmentObject)->GetEntityId();

		IItem* pItem = pItemSystem->GetItem(attachedEntityId);
		if(pItem != NULL)
		{
			if(AllowCloneAttachedItem( pItem->GetEntity()->GetClass() ))
			{
				if(m_attachedItemsInfo.size() < m_attachedItemsInfo.max_size())
				{
					AttachedItem attachedItemInfo;
					attachedItemInfo.pClass = pItem->GetEntity()->GetClass();
					attachedItemInfo.attachmentName = pAttachment->GetName();

					attachedItemInfo.id = CloneAttachedItem( attachedItemInfo, pAttachment );
					m_attachedItemsInfo.push_back(attachedItemInfo);
				}
			}
		}	
	}

	//Only accept requested priority if it has attached weapons
	m_priority = (m_attachedItemsInfo.size() > 0) ? priority : 0;

	//Force physics to sleep immediately (if not already)
	IPhysicalEntity* pCorpsePhysics = GetEntity()->GetPhysics();
	if(pCorpsePhysics != NULL)
	{
		pe_action_awake awakeAction;
		awakeAction.bAwake = 0;
		pCorpsePhysics->Action( &awakeAction );
	}
}

void CAICorpse::AboutToBeRemoved()
{
	IItemSystem* pItemSystem = g_pGame->GetIGameFramework()->GetIItemSystem();

	ICharacterInstance* pCharacter = GetEntity()->GetCharacter(0);
	IAttachmentManager* pAttachmentManager = (pCharacter != NULL) ? pCharacter->GetIAttachmentManager() : NULL;

	for (uint32 i = 0; i < (uint32)m_attachedItemsInfo.size(); ++i)
	{
		AttachedItem& attachedItem = m_attachedItemsInfo[i];

		IItem* pItem = pItemSystem->GetItem( attachedItem.id );
		if((pItem != NULL) && ShouldDropOnCorpseRemoval( pItem->GetEntity()->GetClass() ))
		{
			IAttachment* pAttachment = (pAttachmentManager != NULL) ? pAttachmentManager->GetInterfaceByName( attachedItem.attachmentName.c_str() ) : NULL;
			if(pAttachment != NULL)
			{
				pAttachment->ClearBinding();
			}

			pItem->Drop( 1.0f, false, true );
			pItem->GetEntity()->SetFlags( pItem->GetEntity()->GetFlags() & ~ENTITY_FLAG_NO_SAVE );

			attachedItem.id = 0;
		}
	}
}

EntityId CAICorpse::CloneAttachedItem( const CAICorpse::AttachedItem& attachedItem, IAttachment* pAttachment  )
{
	stack_string clonedItemName;
	clonedItemName.Format("%s_%s", GetEntity()->GetName(), attachedItem.pClass->GetName() );
	SEntitySpawnParams params;
	params.sName = clonedItemName.c_str();
	params.pClass = attachedItem.pClass;

	// Flag as 'No Save' they will be recreated during serialization if needed
	params.nFlags |= (ENTITY_FLAG_NO_SAVE);				

	IEntity *pClonedItemEntity = gEnv->pEntitySystem->SpawnEntity(params);
	assert (pClonedItemEntity != NULL);

	IItem* pClonedItem = g_pGame->GetIGameFramework()->GetIItemSystem()->GetItem(pClonedItemEntity->GetId());
	assert(pClonedItem != NULL);

	pClonedItem->Physicalize(false, false);
	pClonedItem->SetOwnerId( GetEntityId() );

	// Set properties table to null, since they'll not be used
	IScriptTable* pClonedItemEntityScript = pClonedItemEntity->GetScriptTable();
	if (pClonedItemEntityScript != NULL)
	{
		pClonedItemEntity->GetScriptTable()->SetToNull("Properties");
	}

	// Swap attachments
	CEntityAttachment* pEntityAttachement = new CEntityAttachment();
	pEntityAttachement->SetEntityId( pClonedItemEntity->GetId() );

	pAttachment->AddBinding( pEntityAttachement );

	return pClonedItemEntity->GetId();
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

CAICorpseManager* CAICorpseManager::s_pThis = NULL;

CAICorpseManager::CAICorpseManager()
	: m_maxCorpses(AI_CORPSES_MINIMUM)
	, m_lastUpdatedCorpseIdx(0)
{
	assert(gEnv->bMultiplayer == false);
	assert(s_pThis == NULL);

	s_pThis = this;
}

CAICorpseManager::~CAICorpseManager()
{
	s_pThis = NULL;
}

void CAICorpseManager::Reset()
{
	m_corpsesArray.clear();

	m_maxCorpses = max( g_pGameCVars->g_aiCorpses_MaxCorpses, AI_CORPSES_MINIMUM );

	m_corpsesArray.reserve( m_maxCorpses );

	m_lastUpdatedCorpseIdx = 0;
}

EntityId CAICorpseManager::SpawnAICorpseFromEntity( IEntity& sourceEntity, const SCorpseParameters& corpseParams )
{
	assert(gEnv->bMultiplayer == false);

	if(g_pGameCVars->g_aiCorpses_Enable == 0)
		return 0;

	if(m_corpsesArray.size() == m_maxCorpses)
	{
		RemoveSomeCorpses();

		assert((uint32)m_corpsesArray.size() < m_maxCorpses);
	}

	MEMSTAT_CONTEXT(EMemStatContextTypes::MSC_Other, EMemStatContextFlags::MSF_None, "AICorpseManager::SpawnCorpse");

	EntityId corpseId = 0;
	IPhysicalEntity* pSourcePhysics = sourceEntity.GetPhysics();
	if ((pSourcePhysics != NULL) && (pSourcePhysics->GetType() == PE_ARTICULATED))
	{
		ICharacterInstance *pSourceCharacterInstance = sourceEntity.GetCharacter(0);

		if (pSourceCharacterInstance != NULL)
		{
			IEntityClass *pCorpseClass =  gEnv->pEntitySystem->GetClassRegistry()->FindClass("AICorpse");
			assert(pCorpseClass);

			stack_string corpseName;
			corpseName.Format("%s_Corpse", sourceEntity.GetName());
			SEntitySpawnParams params;
			params.pClass = pCorpseClass;
			params.sName = corpseName.c_str();

#if !AI_CORPSES_ENABLE_SERIALIZE
			params.nFlags |= ENTITY_FLAG_NO_SAVE;
#endif

			params.vPosition = sourceEntity.GetWorldPos();
			params.qRotation = sourceEntity.GetWorldRotation();

			IEntity *pCorpseEntity = gEnv->pEntitySystem->SpawnEntity(params, true);
			if(pCorpseEntity != NULL)
			{
				corpseId = pCorpseEntity->GetId();
				
				CorpseInfo* pCorpseInfo = FindCorpseInfo( corpseId );
				assert(pCorpseInfo != NULL);

				CAICorpse* pCorpse = pCorpseInfo->GetCorpse();
				assert(pCorpse != NULL);
				pCorpse->SetupFromSource( sourceEntity, *pSourceCharacterInstance, (uint32)corpseParams.priority);
			}
		}
	}

	return corpseId;
}

void CAICorpseManager::RegisterAICorpse( const EntityId corpseId, const uint32 priority )
{
	assert(!HasCorpseInfo(corpseId));

	m_corpsesArray.push_back(CorpseInfo(corpseId, priority));
}

void CAICorpseManager::UnregisterAICorpse( const EntityId corpseId )
{
	TCorpseArray::iterator corpsesEndIt = m_corpsesArray.end();
	for (TCorpseArray::iterator it = m_corpsesArray.begin(); it != corpsesEndIt; ++it)
	{
		if(it->corpseId != corpseId)
			continue;

		m_corpsesArray.erase(it);
		break;
	}
}

void CAICorpseManager::RemoveSomeCorpses()
{
	const uint32 corspeCount = (uint32)m_corpsesArray.size();
	assert(corspeCount > AI_CORPSES_MINIMUM);

	const uint32 maxCorpsesToRemove = std::min((corspeCount / AI_CORPSES_MINIMUM), (uint32)8);
	assert(maxCorpsesToRemove > 0);

	std::vector<SCorpseRemovalScore> corpseScoresInfo;
	corpseScoresInfo.reserve( corspeCount );

	const CCamera& viewCamera = gEnv->pSystem->GetViewCamera();
	const Vec3 cameraPosition = viewCamera.GetPosition();

	const float farAway = (g_pGameCVars->g_aiCorpses_ForceDeleteDistance * 0.85f);
	const float kUpCloseThreshold = (15.0f * 15.0f);   //Gives non-removal priority to corpses near the player
	const float kFarAwayThreshold = max((farAway * farAway), kUpCloseThreshold * 2.0f); //Gives removal priority to corpses far away from the player

	for(uint32 i = 0; i < corspeCount; ++i)
	{
		CorpseInfo& corpseInfo = m_corpsesArray[i];
		SCorpseRemovalScore removalScore(corpseInfo.corpseId);
		
		CAICorpse* pCorpse = corpseInfo.GetCorpse();
		if(pCorpse != NULL)
		{
			AABB corpseBounds;
			pCorpse->GetEntity()->GetWorldBounds(corpseBounds);
			corpseBounds.Expand( Vec3(0.1f, 0.1f, 0.1f) );
			
			const float distanceSqr = (cameraPosition - corpseBounds.GetCenter()).len2();
			removalScore.distance = distanceSqr;
			removalScore.upClose = (distanceSqr < kUpCloseThreshold);
			removalScore.farAway = (distanceSqr > kFarAwayThreshold);
			removalScore.visible = viewCamera.IsAABBVisible_F(corpseBounds);
			removalScore.priority = pCorpse->GetPriority();
		}

		corpseScoresInfo.push_back(removalScore);
	}

	std::sort(corpseScoresInfo.begin(), corpseScoresInfo.end());

	assert(maxCorpsesToRemove < corpseScoresInfo.size());

	const uint32 corpseScoresCount = corpseScoresInfo.size();
	for(uint32 i = 0; i < maxCorpsesToRemove; ++i)
	{
		RemoveCorpse(corpseScoresInfo[i].corpseId);
	}
}

void CAICorpseManager::RemoveCorpse( const EntityId corpseId )
{
	TCorpseArray::iterator corpsesEndIt = m_corpsesArray.end();
	for (TCorpseArray::iterator it = m_corpsesArray.begin(); it != corpsesEndIt; ++it)
	{
		if(it->corpseId != corpseId)
			continue;

		CAICorpse* pAICorpse = it->GetCorpse();
		if(pAICorpse != NULL)
		{
			pAICorpse->AboutToBeRemoved();
		}

		gEnv->pEntitySystem->RemoveEntity( corpseId );
		m_corpsesArray.erase(it);
		break;
	}
}

CAICorpseManager::CorpseInfo* CAICorpseManager::FindCorpseInfo( const EntityId corpseId )
{
	const size_t corpseCount = m_corpsesArray.size();
	for (size_t i = 0; i < corpseCount; ++i)
	{
		if(m_corpsesArray[i].corpseId != corpseId)
			continue;

		return &(m_corpsesArray[i]);
	}

	return NULL;
}

bool CAICorpseManager::HasCorpseInfo( const EntityId corpseId ) const
{
	const size_t corpseCount = m_corpsesArray.size();
	for (size_t i = 0; i < corpseCount; ++i)
	{
		if (m_corpsesArray[i].corpseId == corpseId)
			return true;
	}
	return false;
}

void CAICorpseManager::Update( const float frameTime )
{
	const uint32 maxCorpsesToUpdateThisFrame = 4;

	const float cullPhysicsDistanceSqr = g_pGameCVars->g_aiCorpses_CullPhysicsDistance * g_pGameCVars->g_aiCorpses_CullPhysicsDistance;
	const float forceDeleteDistanceSqr = g_pGameCVars->g_aiCorpses_ForceDeleteDistance * g_pGameCVars->g_aiCorpses_ForceDeleteDistance;

	if (m_lastUpdatedCorpseIdx >= (uint32)m_corpsesArray.size())
		m_lastUpdatedCorpseIdx = 0;

	const CCamera& viewCamera = gEnv->pSystem->GetViewCamera();

	CryFixedArray<EntityId, maxCorpsesToUpdateThisFrame> corpsesToDelete;
	const uint32 corpsesEndIdx = min(m_lastUpdatedCorpseIdx + maxCorpsesToUpdateThisFrame, (uint32)m_corpsesArray.size());

	for(uint32 i = m_lastUpdatedCorpseIdx; i < corpsesEndIdx; ++i)
	{
		CorpseInfo& corpseInfo = m_corpsesArray[i];

		IEntity* pCorpseEntity = corpseInfo.GetCorpseEntity();
		if(pCorpseEntity != NULL)
		{
			AABB corpseBbox;
			pCorpseEntity->GetWorldBounds(corpseBbox);
			corpseBbox.Expand(Vec3(0.1f, 0.1f, 0.1f));

			const Vec3 corpsePosition = corpseBbox.GetCenter();
			const float distanceSqr = (corpsePosition - viewCamera.GetPosition()).len2();

			const bool attemptDeleteFarAway = (distanceSqr > forceDeleteDistanceSqr);
			const bool cullPhysics = (distanceSqr > cullPhysicsDistanceSqr);
			const bool isVisible = viewCamera.IsAABBVisible_F(corpseBbox);
			
			corpseInfo.flags.SetFlags( CorpseInfo::eFlag_FarAway, attemptDeleteFarAway );
			
			if(attemptDeleteFarAway && !isVisible)
			{
				corpsesToDelete.push_back(corpseInfo.corpseId);
			}
			else if(cullPhysics != corpseInfo.flags.AreAllFlagsActive( CorpseInfo::eFlag_PhysicsDisabled ))
			{
				{
					pCorpseEntity->EnablePhysics(!cullPhysics);
					
					if(cullPhysics == false)
					{
						IPhysicalEntity* pCorpsePhysics = pCorpseEntity->GetPhysics();
						if(pCorpsePhysics != NULL)
						{
							pe_action_awake awakeAction;
							awakeAction.bAwake = 0;
							pCorpsePhysics->Action( &awakeAction );
						}
					}
				}

				corpseInfo.flags.SetFlags( CorpseInfo::eFlag_PhysicsDisabled, cullPhysics );
			}
		}
		else
		{
			//This should not happen, but in case remove the entity from the list
			GameWarning("AICorpseManager - Detected corpse with no entity, removing from list");
			corpsesToDelete.push_back(corpseInfo.corpseId);
		}
	}

	m_lastUpdatedCorpseIdx = corpsesEndIdx;

	for(uint32 i = 0; i < (uint32)corpsesToDelete.size(); ++i)
	{
		RemoveCorpse(corpsesToDelete[i]);
	}

	DebugDraw();
}

void CAICorpseManager::RemoveAllCorpses( const char* requester )
{
	while(m_corpsesArray.empty() == false)
	{
		RemoveCorpse( m_corpsesArray[0].corpseId );
	}

	m_lastUpdatedCorpseIdx = 0;

	CryLog("AICorpseManager: Removing all corpses as requested by '%s'", requester ? requester : "Unknown");
}

#if AI_CORPSES_DEBUG_ENABLED

void CAICorpseManager::DebugDraw()
{
	if(g_pGameCVars->g_aiCorpses_DebugDraw == 0)
		return;

	IRenderAuxGeom* pRenderAux = gEnv->pRenderer->GetIRenderAuxGeom();

	IRenderAuxText::Draw2dLabel( 50.0f, 50.0f, 1.5f, Col_White, false, "Corpse count %" PRISIZE_T " - Max %d", m_corpsesArray.size(), m_maxCorpses );

	for(size_t i = 0; i < m_corpsesArray.size(); ++i)
	{
		CorpseInfo& corpse = m_corpsesArray[i];

		CAICorpse* pCorpse = corpse.GetCorpse();
		if(pCorpse != NULL)
		{
			AABB corpseBbox;
			pCorpse->GetEntity()->GetWorldBounds(corpseBbox);
			const Vec3 refPosition = corpseBbox.IsEmpty() ? pCorpse->GetEntity()->GetWorldPos() : corpseBbox.GetCenter();

			IRenderAuxText::DrawLabelF( refPosition, 1.5f, "%s\nPriority %d\n%s\n%s",
				pCorpse->GetEntity()->GetName(), pCorpse->GetPriority(),
				corpse.flags.AreAnyFlagsActive( CorpseInfo::eFlag_FarAway ) ? "Far away, remove when not visible" : "Not far away",
				corpse.flags.AreAllFlagsActive( CorpseInfo::eFlag_PhysicsDisabled) ? "Physics disabled" : "Physics enabled" );
			pRenderAux->DrawCone( refPosition + Vec3(0.0f, 0.0f, 1.5f), Vec3(0.0f, 0.0f, -1.0f), 0.3f, 0.8f, Col_Red );
		}
	}
}

#endif //AI_CORPSES_DEBUG_ENABLED

CAICorpseManager::SCorpseParameters::Priority CAICorpseManager::GetPriorityForClass( const IEntityClass* pEntityClass )
{
	static const uint32 kPriorityClassesCount = 3;
	static const IEntityClass* classesTable[kPriorityClassesCount] = 
	{
		gEnv->pEntitySystem->GetClassRegistry()->FindClass("Scorcher"),
	};

	static const SCorpseParameters::Priority prioritiesTable[kPriorityClassesCount] = 
	{
		SCorpseParameters::ePriority_VeryHight,
		SCorpseParameters::ePriority_VeryHight,
		SCorpseParameters::ePriority_High
	};

	for(uint32 i = 0; i < kPriorityClassesCount; ++i)
	{
		if(classesTable[i] == pEntityClass)
			return prioritiesTable[i];
	}

	return SCorpseParameters::ePriority_Normal;
}
