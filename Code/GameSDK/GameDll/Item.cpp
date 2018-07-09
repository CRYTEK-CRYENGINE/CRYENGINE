// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

/*************************************************************************
 -------------------------------------------------------------------------
  $Id$
  $DateTime$
  
 -------------------------------------------------------------------------
  History:
  - 27:10:2004   11:26 : Created by Márcio Martins

*************************************************************************/
#include "StdAfx.h"
#include "Item.h"
#include "ItemSharedParams.h"
#include "Game.h"
#include "GameActions.h"
#include "IGameObject.h"
#include <CryNetwork/ISerialize.h>
#include "IWorldQuery.h"
#include <CryEntitySystem/IEntitySystem.h>

#include "Actor.h"
#include "Player.h"
#include "IActorSystem.h"
#include "IItemSystem.h"
#include "ActionMapManager.h"
#include "ScriptBind_Item.h"
#include "GameRules.h"
#include "GameCVars.h"
#include "Binocular.h"
#include "TacticalManager.h"
#include "GameCodeCoverage/GameCodeCoverageTracker.h"
#include "WeaponSystem.h"

#include "ItemResourceCache.h"

#include "UI/HUD/HUDUtils.h"
#include "UI/HUD/HUDEventDispatcher.h"
#include "UI/HUD/HUDEventWrapper.h"
#include "EntityUtility/EntityEffectsCloak.h"

#include "RecordingSystem.h"

#include "ItemAnimation.h"
#include "ItemAnimation.h"

#include <IVehicleSystem.h>
#include <IPerceptionManager.h>


#pragma warning(disable: 4355)	// 'this' used in base member initializer list

#if defined(USER_alexll)
#define ITEM_DEBUG_MEMALLOC
#endif

#ifdef ITEM_DEBUG_MEMALLOC
int gInstanceCount = 0;
#endif

#define NETSERIALIZE_MOUNTDIR 0

IEntitySystem *CItem::m_pEntitySystem=0;
IItemSystem *CItem::m_pItemSystem=0;
IGameFramework *CItem::m_pGameFramework=0;
IGameplayRecorder*CItem::m_pGameplayRecorder=0;

IEntityClass* CItem::sBinocularsClass = 0;
IEntityClass* CItem::sDebugGunClass = 0;
IEntityClass* CItem::sRefWeaponClass = 0;
IEntityClass* CItem::sLTagGrenade = 0;
IEntityClass* CItem::sFragHandGrenadesClass = 0;
IEntityClass* CItem::sNoWeaponClass = 0;
IEntityClass* CItem::sWeaponMeleeClass = 0;
IEntityClass* CItem::sBowClass = 0;
IEntityClass* CItem::sSilencerPistolClass = 0;
IEntityClass* CItem::sSilencerClass = 0;

SMannequinItemParams CItem::sNullManItemParams;

SItemFragmentTagCRCs CItem::sFragmentTagCRCs;
SItemActionParamCRCs	 CItem::sActionParamCRCs;
SItemAnimationEvents CItem::s_animationEventsTable;

struct CItem::ExchangeToNextItem
{
public:
	ExchangeToNextItem(CActor* _pActor, CItem* _pNextItem)
		:	owner(_pActor->GetEntityId())
		,	nextItem(_pNextItem->GetEntityId()) {}

	void execute(CItem* _this)
	{
		_this->ClearItemFlags(eIF_ExchangeItemScheduled);

		CActor* pActor = static_cast<CActor*>(gEnv->pGameFramework->GetIActorSystem()->GetActor(owner));
		CItem* pNextItem = static_cast<CItem*>(gEnv->pGameFramework->GetIItemSystem()->GetItem(nextItem));

		if (!pActor || !pNextItem)
			return;

		if( pActor->GetInventory()->FindItem(pNextItem->GetEntityId()) == -1  )
		{
			pActor->ExchangeItem(_this, pNextItem);
		}
	}

private:
	EntityId owner;
	EntityId nextItem;
};


namespace
{
	float GetFastSelectSpeedBias( const CItem* pItem, const float unholsteringSelectBiasTime )
	{
		if( pItem->AreAnyItemFlagsSet( CItem::eIF_Unholstering ) )
		{
			return unholsteringSelectBiasTime;
		}
		if( pItem->AreAnyItemFlagsSet( CItem::eIF_PlayFastSelectAsSpeedBias ) )
		{
			return 1.0f/g_pGameCVars->i_fastSelectMultiplier;
		}
		return 1.0f;
	}
}

//------------------------------------------------------------------------
CItem::CItem()
: m_scheduler(this),	// just to store the pointer.
	m_itemFlags(eIF_InformClientsAboutUse | eIF_UnholsteringPlaySelect | eIF_AccessoryAmmoAvailable),
	m_postSerializeMountedOwner(0),
	m_parentId(0),
	m_hostId(0),
	m_serializeActivePhysics(0),
	m_itemLowerMode(eILM_Raised),
	m_deferPhysicalization(eIPhys_Max),
	m_dropRayId(0),
	m_unholsteringSelectBiasTime(1.0f),
	m_delayedUnhideCntr(0),
	m_pItemActionController(NULL),
	m_pItemAnimationContext(NULL),
	m_pCurrentActionController(NULL),
	m_pCurrentManItemParams(&sNullManItemParams),
	m_attachedAccessoryHistory(0),
	m_subContext(TAG_ID_INVALID)
{
#ifdef ITEM_DEBUG_MEMALLOC
	++gInstanceCount;
#endif
	memset(m_animationTime, 0, sizeof(m_animationTime));
}

//------------------------------------------------------------------------
CItem::~CItem()
{
	CCCPOINT(Item_Destroy);
	
	UpdateCurrentActionController();
	if(IActionController* pActionController = GetActionController())
		ClearScopeContexts(pActionController);

	SAFE_RELEASE(m_pItemActionController);
	SAFE_DELETE(m_pItemAnimationContext);

	SetCurrentActionController(NULL);

	UnRegisterAsUser();

	AttachToBack(false);

	//Auto-detach from the parent
	if(m_parentId)
	{
		if(CItem* pParent = static_cast<CItem*>(m_pItemSystem->GetItem(m_parentId)))
		{
			pParent->RemoveAccessory(GetEntity()->GetClass());
		}
	}

	GetGameObject()->ReleaseProfileManager(this);

	if(IInventory *pInventory = GetActorInventory(GetOwnerActor()))
	{
		pInventory->RemoveItem(GetEntityId());
	}

	if (m_stats.selected)
	{
		UpdateActionControllerSelection(false);
	}

	if(!(GetISystem()->IsSerializingFile() && GetGameObject()->IsJustExchanging()))
	{
		RemoveAllAccessories();
	}

	if(m_pItemSystem)
	{
		m_pItemSystem->RemoveItem(GetEntityId(),GetEntity()->GetClass()->GetName());
	}

	Quiet();

#ifdef ITEM_DEBUG_MEMALLOC
	 --gInstanceCount;
#endif

	 if (m_dropRayId && g_pGame)
		 g_pGame->GetRayCaster().Cancel(m_dropRayId);
}

//------------------------------------------------------------------------
const char* CItem::GetWeaponComponentType()
{
	return "CItem";
}

//------------------------------------------------------------------------
const char* CItem::GetType() const
{
	return GetWeaponComponentType();
}

//------------------------------------------------------------------------
bool CItem::Init( IGameObject *pGameObject )
{
	SetGameObject(pGameObject);

#ifdef ITEM_DEBUG_MEMALLOC
	CGame::DumpMemInfo("CItem::Init Instance=%d %p Id=%d Class=%s", gInstanceCount, GetGameObject(), GetEntityId(), gEnv->pEntitySystem->GetEntity(GetEntityId())->GetClass()->GetName());
#endif

	if (!m_pGameFramework)
	{
		m_pEntitySystem = gEnv->pEntitySystem;
		m_pGameFramework= gEnv->pGameFramework;
		m_pGameplayRecorder = m_pGameFramework->GetIGameplayRecorder();
		m_pItemSystem = m_pGameFramework->GetIItemSystem();

		IEntityClassRegistry* pClassRegistry = gEnv->pEntitySystem->GetClassRegistry();

		sBinocularsClass = pClassRegistry->FindClass("Binoculars");
		sDebugGunClass = pClassRegistry->FindClass("DebugGun");
		sRefWeaponClass = pClassRegistry->FindClass("RefWeapon");
		sLTagGrenade = pClassRegistry->FindClass("LTagGrenade");
		sFragHandGrenadesClass = pClassRegistry->FindClass("FragGrenades");
		sNoWeaponClass = pClassRegistry->FindClass("NoWeapon");
		sWeaponMeleeClass = pClassRegistry->FindClass("WeaponMelee");
		sSilencerPistolClass = pClassRegistry->FindClass("SilencerPistol");
		sSilencerClass = pClassRegistry->FindClass("Silencer");

		sFragmentTagCRCs.Init();
		sActionParamCRCs.Init();
		s_animationEventsTable.Init();
	}

	if (!GetGameObject()->CaptureProfileManager(this))
		return false;

	IEntity* pEntity = GetEntity();

	// bind to network
	if ((0 == (pEntity->GetFlags() & (ENTITY_FLAG_CLIENT_ONLY | ENTITY_FLAG_SERVER_ONLY))) && ShouldBindOnInit())
	{
		if (!GetGameObject()->BindToNetwork())
		{
			GetGameObject()->ReleaseProfileManager(this);
			return false;
		}
	}

	// register with item system
	m_pItemSystem->AddItem(GetEntityId(), this);

	// attach script bind
	g_pGame->GetItemScriptBind()->AttachTo(this);

	// ignore invalid file access for CItem initialization
	SCOPED_ALLOW_FILE_ACCESS_FROM_THIS_THREAD();

	if(!ResetParams())
	{
		//failed to find all appropriate shared parameters bailing out
		return false;
	}

	ClearItemFlags(eIF_NoDrop);

	m_effectsController.InitWithEntity(pEntity);

	if(!InitActionController(pEntity))
	{
		return false;
	}

#ifdef ITEM_DEBUG_MEMALLOC
	CGame::DumpMemInfo("CItem::Init End %p Id=%d Class=%s", GetGameObject(), GetEntityId(), gEnv->pEntitySystem->GetEntity(GetEntityId())->GetClass()->GetName());
#endif

	return true;
}

bool CItem::InitActionController( IEntity* pEntity )
{
	if(!m_sharedparams->params.actionControllerFile.empty())
	{
		IMannequin &mannequinSys = gEnv->pGameFramework->GetMannequinInterface();
		IAnimationDatabaseManager& animationDatabaseManager = mannequinSys.GetAnimationDatabaseManager();

		const SControllerDef* pControllerDef = animationDatabaseManager.LoadControllerDef( m_sharedparams->params.actionControllerFile.c_str() );
		if (pControllerDef == NULL)
		{
			return false;
		}

		CRY_ASSERT(m_pItemAnimationContext == NULL);
		m_pItemAnimationContext = new SAnimationContext(*pControllerDef);
		CRY_ASSERT(m_pItemActionController == NULL);
		m_pItemActionController = mannequinSys.CreateActionController( pEntity, *m_pItemAnimationContext );
		UpdateCurrentActionController();
		if(m_pItemActionController == NULL)
		{
			return false;
		}

		if (!m_sharedparams->params.soundAdbFile.empty())
		{
			const IAnimationDatabase* pSoundDatabase = animationDatabaseManager.Load(m_sharedparams->params.soundAdbFile.c_str());
			const int scopeContextSound = pControllerDef->m_scopeContexts.Find("Audio");

			if(scopeContextSound >= 0 && pSoundDatabase)
			{
				m_pItemActionController->SetScopeContext(scopeContextSound, *GetEntity(), NULL, pSoundDatabase);
			}
		}
	}
	return true;
}




bool CItem::ResetParams()
{
	CGameSharedParametersStorage* pGameParamsStorage = g_pGame->GetGameSharedParametersStorage();
	IEntityClass* pItemClass = GetEntity()->GetClass();

	m_sharedparams = pGameParamsStorage->GetItemSharedParameters(pItemClass->GetName(), false);
	
	if(!m_sharedparams)
	{
		GameWarning("Uninitialised item params. Has the xml been setup correctly for item %s?", GetEntity()->GetName());

		return false;
	}
	
	m_sharedparams->CacheResources(pGameParamsStorage->GetItemResourceCache(), pItemClass);

	return true;
}

//------------------------------------------------------------------------
void CItem::Reset()
{
	CRY_PROFILE_FUNCTION(PROFILE_GAME);

	if (IScriptTable* pScriptTable = gEnv->pEntitySystem->GetEntity(GetEntityId())->GetScriptTable())
	{
		SmartScriptTable props;
		pScriptTable->GetValue("Properties", props);
		ReadProperties(props);
	}

#ifdef ITEM_DEBUG_MEMALLOC
	CGame::DumpMemInfo("CItem::Reset Start %p Id=%d Class=%s", GetGameObject(), GetEntityId(), gEnv->pEntitySystem->GetEntity(GetEntityId())->GetClass()->GetName());
#endif

	if (IsModifying())
		ResetAccessoriesScreen(GetOwnerActor());

	ResetOwner();  
  m_scheduler.Reset();
  
	m_effectsController.FreeAllEffects();

	InitItemFromParams();

	Quiet();

	ReAttachAccessories();

	const int numAccessories = m_accessories.size();

	for (int i = 0; i < numAccessories; i++)
	{
		FixAccessories(GetAccessoryParams(m_accessories[i].pClass), true);
	}

	InitRespawn();

	CloakEnable(false, false);

	ClearItemFlags(eIF_SelectGrabbingWeapon | eIF_NoDrop);

	if(m_sharedparams->params.has_first_select || m_properties.specialSelect)
		m_stats.first_selection = true; //Reset (just in case)
	else
		m_stats.first_selection = false;
	
	if(m_pItemActionController)
	{
		//Needs to be before OnReset
		m_pItemActionController->Reset();
	}

	OnReset();
	CheckViewChange();

	ClearItemFlags(eIF_Selecting | eIF_Unholstering);

#ifdef ITEM_DEBUG_MEMALLOC
	CGame::DumpMemInfo("  CItem::Reset End %p Id=%d Class=%s", GetGameObject(), GetEntityId(), gEnv->pEntitySystem->GetEntity(GetEntityId())->GetClass()->GetName());
	CryLogAlways("");
#endif

	UnlowerItem();

}

//------------------------------------------------------------------------
void CItem::ResetOwner()
{
  if (m_owner.GetId())
  {
    if (m_stats.used)
      StopUse(m_owner.GetId());

    CActor *pOwner = GetOwnerActor();
		if (!pOwner)
			return;

    if (pOwner->GetInventory()->FindItem(GetEntityId()) < 0)
		{
      SetOwnerId(0);
		}
  }
	UpdateCurrentActionController();
}

//------------------------------------------------------------------------
void CItem::PostInit( IGameObject * pGameObject )
{
	pGameObject->RegisterExtForEvents( this, NULL, 0 );

	// prevent ai from automatically disabling weapons
	for (int i=0; i<4;i++)
	{
		pGameObject->SetUpdateSlotEnableCondition(this, i, eUEC_WithoutAI);
	}

	Reset();
	
	PatchInitialSetup();	
	InitialSetup();		//Must be called after Patch
}

//------------------------------------------------------------------------
bool CItem::ReloadExtension( IGameObject * pGameObject, const SEntitySpawnParams &params )
{
	ResetGameObject();
	pGameObject->RegisterExtForEvents( this, NULL, 0 );

	CRY_ASSERT_MESSAGE(false, "CItem::ReloadExtension not implemented");
	
	return false;
}

//------------------------------------------------------------------------
bool CItem::GetEntityPoolSignature( TSerialize signature )
{
	CRY_ASSERT_MESSAGE(false, "CItem::GetEntityPoolSignature not implemented");
	
	return true;
}

//------------------------------------------------------------------------
void CItem::Release()
{
	delete this;
}

//------------------------------------------------------------------------
void CItem::Update( SEntityUpdateContext& ctx, int slot )
{
	CRY_PROFILE_FUNCTION(PROFILE_GAME);

	if (!IsDestroyed())
	{
		switch (slot)
		{
		case eIUS_General:
			{
				if(m_pItemActionController)
				{
					m_pItemActionController->Update(ctx.fFrameTime);
				}

				if(IsServer() && m_deferPhysicalization != eIPhys_Max)
				{
					GetGameObject()->SetAspectProfile(eEA_Physics, m_deferPhysicalization);
					m_deferPhysicalization = eIPhys_Max;
				}

				if (m_delayedUnhideCntr > 0)
				{
					//--- Reenable rendering of the char attachment
					m_delayedUnhideCntr--;
					if (m_delayedUnhideCntr == 0)
					{
						IAttachmentManager *pAttachmentManager = GetOwnerAttachmentManager();
						if (pAttachmentManager)
						{
							IAttachment *pAttachment = pAttachmentManager->GetInterfaceByName(m_sharedparams->params.attachment[m_stats.hand].c_str());
							if (pAttachment)
							{
								pAttachment->HideAttachment(false);

								if (m_stats.viewmode & eIVM_FirstPerson)
								{
									pAttachment->HideInRecursion(true);
								}
							}
						}
						ClearItemFlags(eIF_UseAnimActionUnhide);
					}
				}

				CActor* pOwner = GetOwnerActor();
				if ((pOwner == NULL) || !pOwner->IsClient())
					break;

				//Client only stuff

				if (IsSelected() && pOwner->GetActorClass() == CPlayer::GetActorClassType())
				{
					UpdateLowerItem(static_cast<CPlayer*>(pOwner));
				}
			}
			break;

		case eIUS_Scheduler:
			{
				m_scheduler.Update(ctx.fFrameTime);
			}
			break;
		}
	}
}

//------------------------------------------------------------------------
namespace 
{

	bool IsPointingToFriendly(CPlayer* pPlayer)
	{
		EntityId targetEntityId = pPlayer->GetGameObject()->GetWorldQuery()->GetLookAtEntityId();
		IEntity* pTargetEntity = gEnv->pEntitySystem->GetEntity(targetEntityId);

		if (!pTargetEntity)
			return false;

		AABB targetBounds(ZERO);
		pTargetEntity->GetWorldBounds(targetBounds);
		Vec3 targetPosition = targetBounds.GetCenter();
		Vec3 playerPosition = pPlayer->GetEntity()->GetWorldPos();
		float targetDistance = targetPosition.GetSquaredDistance(playerPosition);
		if (targetDistance > sqr(g_pGameCVars->g_friendlyLowerItemMaxDistance))
			return false;

		IAIObject* pPlayerAI = pPlayer->GetEntity()->GetAI();
		IAIObject* pTargetAI = pTargetEntity->GetAI();
		if (pTargetAI && pTargetAI->IsHostile(pPlayerAI, false))
			return false;

		IScriptTable* pTargetScriptTable = pTargetEntity->GetScriptTable();
		SmartScriptTable pProperties;
		if (pTargetScriptTable && pTargetScriptTable->GetValue("Properties", pProperties))
		{
			const char* esFaction = "";
			pProperties->GetValue("esFaction", esFaction);
			if (strcmp(esFaction, "Marines") == 0 || 
				strcmp(esFaction, "Infected") == 0 ||
				strcmp(esFaction, "Civilians") == 0)
				return true;
		}

		return false;
	}

}

//------------------------------------------------------------------------
void CItem::UpdateLowerItem(CPlayer* pLocalPlayer)
{
	eItemLowerMode lowerItemMode = eILM_Raised;
	if (IsPointingToFriendly(pLocalPlayer))
		lowerItemMode = eILM_Lower;
	if (pLocalPlayer->IsCinematicFlagActive(SPlayerStats::eCinematicFlag_LowerWeapon) ||
			pLocalPlayer->IsCinematicFlagActive(SPlayerStats::eCinematicFlag_LowerWeaponMP))
	{
		lowerItemMode = eILM_Cinematic;
	}

	CWeapon* pWeapon = static_cast<CWeapon*>(GetIWeapon());
	if (pWeapon && (pWeapon->IsReloading() || pWeapon->IsZoomed() || pWeapon->IsZoomingInOrOut() || pWeapon->IsFiring() || pWeapon->IsRippingOff() ))
		lowerItemMode = eILM_Raised;
	if (!IsSelected() || AreAnyItemFlagsSet(eIF_Selecting | eIF_Modifying))
		lowerItemMode = eILM_Raised;

	if (lowerItemMode != eILM_Raised)
		LowerItem(lowerItemMode);
	else
		UnlowerItem();
}

//------------------------------------------------------------------------
bool CItem::SetAspectProfile( EEntityAspects aspect, uint8 profile )
{
	//CryLogAlways("%s::SetProfile(%d: %s)", GetEntity()->GetName(), profile, profile==eIPhys_Physicalized?"Physicalized":"NotPhysicalized");

	if (aspect == eEA_Physics)
	{
		switch (profile)
		{
		case eIPhys_PhysicalizedStatic:
			{
				m_stats.physicalizedSlot = (m_stats.pickable && GetEntity()->IsSlotValid(eIGS_Aux0)) ? eIGS_Aux0 : eIGS_ThirdPerson;

				SEntityPhysicalizeParams params;
				params.type = PE_STATIC;
				params.nSlot = m_stats.physicalizedSlot;

				GetEntity()->Physicalize(params);

				return true;
			}
			break;
		case eIPhys_PhysicalizedRigid:
			{
				m_stats.physicalizedSlot = (m_stats.pickable && GetEntity()->IsSlotValid(eIGS_Aux0)) ? eIGS_Aux0 : eIGS_ThirdPerson;

				SEntityPhysicalizeParams params;
				params.type = PE_RIGID;
				params.nSlot = m_stats.physicalizedSlot;
				params.mass = m_sharedparams->params.mass;

				pe_params_buoyancy buoyancy;
				buoyancy.waterDamping = 1.5;
				buoyancy.waterResistance = 1000;
				buoyancy.waterDensity = 1;
				params.pBuoyancy = &buoyancy;

				GetEntity()->Physicalize(params);

				IPhysicalEntity *pPhysics = GetEntity()->GetPhysics();
				if (pPhysics)
				{
					pe_action_awake action;
					action.bAwake = m_owner.GetId()!=0;
					action.minAwakeTime = 1.0f;
					pPhysics->Action(&action);

					pe_params_part ignoreCollisions;
					ignoreCollisions.flagsAND = ~(geom_colltype_explosion|geom_colltype_ray);
					ignoreCollisions.flagsColliderOR = geom_no_particle_impulse;
					pPhysics->SetParams(&ignoreCollisions);
				}
			}
			return true;
		case eIPhys_NotPhysicalized:
			{
				if(m_stats.physicalizedSlot != eIGS_Last)
				{
					{
						SEntityPhysicalizeParams params;
						params.type = PE_NONE;
						params.nSlot = m_stats.physicalizedSlot;
						GetEntity()->Physicalize(params);
					}

					m_stats.physicalizedSlot = eIGS_Last;
				}
			}
			return true;
		}
	}

	return false;
}

uint8 CItem::GetDefaultProfile( EEntityAspects aspect )
{
	if (aspect == eEA_Physics)
	{
		return m_properties.physics;
	}
	else
	{
		return 0;
	}
}

//------------------------------------------------------------------------
void CItem::HandleEvent( const SGameObjectEvent &evt )
{
}

//------------------------------------------------------------------------
void CItem::ProcessEvent(const SEntityEvent& event)
{
	CRY_PROFILE_FUNCTION(PROFILE_GAME);

	switch (event.event)
	{
	case ENTITY_EVENT_ANIM_EVENT:
		{
			if (m_pItemActionController)
			{
				const AnimEventInstance* pAnimEvent = reinterpret_cast<const AnimEventInstance*>(event.nParam[0]);
				ICharacterInstance* pCharacter = reinterpret_cast<ICharacterInstance*>(event.nParam[1]);
				if (pAnimEvent && pCharacter)
				{
					m_pItemActionController->OnAnimationEvent(pCharacter, *pAnimEvent);
				}
			}
		}
		break;
	case ENTITY_EVENT_TIMER:
		{
			switch (event.nParam[0])
			{
			case eIT_Flying:
				m_stats.flying = false;
				
				//Add an small impulse, sometimes item keeps floating in the air
				GetEntity()->AddImpulse(-1, Vec3(0.0f,0.0f,0.0f), Vec3(0.0f,0.0f,-1.0f)*m_sharedparams->params.drop_impulse, false, 1.0f);
				break;
			}
      break;
		}
	case ENTITY_EVENT_RESET:

		if (gEnv->IsEditor() && !m_stats.mounted)
		{
			IInventory *pInventory=GetActorInventory(GetOwnerActor());

			if (event.nParam[0]) // entering game mode in editor
			{
				m_editorstats = SEditorStats(GetOwnerId(), pInventory ? pInventory->GetCurrentItem() == GetEntityId() : 0);
				const bool bhasOwner = GetOwnerId() != 0;
				Pickalize(!bhasOwner, !bhasOwner);
			}
			else // leaving game mode
			{
				Reset();

				if (m_editorstats.ownerId)
				{
					SetItemFlags(eIF_NoDrop);

					AttachToBack(false);

					int previousValue = g_pGameCVars->g_inventoryNoLimits;
					g_pGameCVars->g_inventoryNoLimits = 1;

					PickUp(m_editorstats.ownerId, false, false, false);

					g_pGameCVars->g_inventoryNoLimits = previousValue;

					IItemSystem *pItemSystem=g_pGame->GetIGameFramework()->GetIItemSystem();

					if (m_editorstats.current && pInventory && pInventory->GetCurrentItem()==GetEntityId())
					{
						//if (pInventory)
						pInventory->SetCurrentItem(0);
						pItemSystem->SetActorItem(GetActor(m_editorstats.ownerId), GetEntityId(), false);
					}
					else if (pInventory && pInventory->GetCurrentItem()==GetEntityId())
						pItemSystem->SetActorItem(GetActor(m_editorstats.ownerId), (EntityId)0, false);

				}
				else
				{
					SetOwnerId(0);

					RemoveAllAccessories();
					PatchInitialSetup();
					InitialSetup();
				}
			}
		}
		else
		{
			Reset();
		}
    break;

	case ENTITY_EVENT_PRE_SERIALIZE:
		AttachToHand(false); //only required because we don't have the "arms" weapon anymore if there is no item
		break;

	case ENTITY_EVENT_DEACTIVATED:
		if (m_pItemActionController)
		{
			m_pItemActionController->Reset();
		}
		break;
	}
}

uint64 CItem::GetEventMask() const
{
	return ENTITY_EVENT_BIT(ENTITY_EVENT_ANIM_EVENT) | ENTITY_EVENT_BIT(ENTITY_EVENT_TIMER) | ENTITY_EVENT_BIT(ENTITY_EVENT_RESET) | ENTITY_EVENT_BIT(ENTITY_EVENT_PRE_SERIALIZE) | ENTITY_EVENT_BIT(ENTITY_EVENT_DEACTIVATED);
}

//------------------------------------------------------------------------
bool CItem::NetSerialize( TSerialize ser, EEntityAspects aspect, uint8 profile, int pflags )
{
	if (aspect == eEA_Physics)
	{
		pe_type type = PE_NONE;
		switch (profile)
		{
		case eIPhys_PhysicalizedStatic:
			type = PE_STATIC;
			break;
		case eIPhys_PhysicalizedRigid:
			type = PE_RIGID;
			break;
		case eIPhys_NotPhysicalized:
			type = PE_NONE;
			break;
		default:
			return false;
		}

		if (type == PE_NONE)
			return true;

		NET_PROFILE_SCOPE("Physics", ser.IsReading());

		if (ser.IsWriting())
		{
			if (!GetEntity()->GetPhysicalEntity() || GetEntity()->GetPhysicalEntity()->GetType() != type)
			{
				gEnv->pPhysicalWorld->SerializeGarbageTypedSnapshot( ser, type, 0 );
				return true;
			}
		}

		GetEntity()->PhysicsNetSerializeTyped( ser, type, pflags );
	}
#if NETSERIALIZE_MOUNTDIR
	else if (aspect == eEA_GameServerA)
	{
		NET_PROFILE_SCOPE("MountDir", ser.IsReading());

		ser.Value("mountDir", m_stats.mount_dir, 'dir2');
	}
#endif //NETSERIALIZE_MOUNTDIR

	return true;
}

NetworkAspectType CItem::GetNetSerializeAspects()
{
	return eEA_Physics
#if NETSERIALIZE_MOUNTDIR
		| eEA_GameServerA
#endif
		;
}

//------------------------------------------------------------------------
void CItem::FullSerialize( TSerialize ser )
{
	assert(ser.GetSerializationTarget() != eST_Network);
	if(ser.IsReading())
	{
		AttachToBack(false);
	}

	m_stats.Serialize(ser);

	EntityId ownerId = m_owner.GetId();
	ser.Value("ownerId", ownerId);
	ser.Value("parentId", m_parentId);
	ser.Value("hostId", m_hostId);
	
	bool bCloaked = AreAnyItemFlagsSet(eIF_Cloaked);
	ser.Value("m_cloaked", bCloaked);
	SetItemFlag(eIF_SerializeCloaked, bCloaked);

	bool bAccessoryAmmoAvailable = AreAnyItemFlagsSet(eIF_AccessoryAmmoAvailable);
	ser.Value("accAmmoAvail", bAccessoryAmmoAvailable);
	SetItemFlag(eIF_AccessoryAmmoAvailable, bAccessoryAmmoAvailable);

	m_serializeActivePhysics = Vec3(0,0,0);
	
	bool bSerializeDestroyed = IsDestroyed();
	ser.Value("m_serializeDestroyed", bSerializeDestroyed);
	SetItemFlag(eIF_SerializeDestroyed, bSerializeDestroyed);

	bool bSerializeRigidPhysics = true;

	if(ser.IsWriting())
	{
		if(IPhysicalEntity *pPhys = GetEntity()->GetPhysics())
		{
			bSerializeRigidPhysics = (pPhys->GetType()==PE_RIGID);
			pe_status_dynamics dyn;
			if (pPhys->GetStatus(&dyn))
			{
				if(dyn.v.len() > 0.1f)
					m_serializeActivePhysics = dyn.v.GetNormalized();
			}
		}
	}

	ser.Value("m_serializeActivePhysics", m_serializeActivePhysics);

	ser.Value("m_serializeRigidPhysics", bSerializeRigidPhysics);
	SetItemFlag(eIF_SerializeRigidPhysics, bSerializeRigidPhysics);


	ser.EnumValue("m_deferPhysicalization", m_deferPhysicalization, eIPhys_PhysicalizedRigid, eIPhys_Max);

	if (ser.IsReading())
	{
		IVehicle* pVehicleHost = g_pGame->GetIGameFramework()->GetIVehicleSystem()->GetVehicle( m_hostId );
		const bool notMountedToVehicle = (pVehicleHost == NULL);
		if(notMountedToVehicle)
		{
			if (m_stats.mounted && m_sharedparams->params.usable)
			{
				if(m_owner.GetId())
				{
					IActor *pActor = gEnv->pGameFramework->GetIActorSystem()->GetActor(m_owner.GetId());
					CPlayer *pPlayer = static_cast<CPlayer*> (pActor);
					if(pPlayer)
					{
						if(m_stats.used)
							pPlayer->UseItem(GetEntityId());
						else
							StopUse(m_owner.GetId());
					}
					m_stats.used = false;
				}

				m_postSerializeMountedOwner = ownerId;
			}
			else
			{
				SetOwnerId(ownerId);
			}
		}
	}

	//serialize attachments
	CryFixedStringT<32> name;
	int attachmentAmount = m_accessories.size();
	ser.Value("attachmentAmount", attachmentAmount);
	if(ser.IsWriting())
	{
		for(int i = 0; i < attachmentAmount; i++)
		{
			name = m_accessories[i].pClass->GetName();
			EntityId id = m_accessories[i].accessoryId;
			ser.BeginGroup("Accessory");
			ser.Value("Name", name);
			ser.Value("Id", id);
			ser.EndGroup();
		}
	}
	else if(ser.IsReading())
	{
		m_accessories.clear();
		for(int i = 0; i < attachmentAmount; ++i)
		{
			EntityId id = 0;
			ser.BeginGroup("Accessory");
			ser.Value("Name", name);
			ser.Value("Id", id);

			if(IEntityClass* pAccessoryClass = gEnv->pEntitySystem->GetClassRegistry()->FindClass(name.c_str()))
			{
				m_accessories.push_back(SAccessoryInfo(pAccessoryClass, id));
			}	

			ser.EndGroup();
		}
	}

	if(ser.IsReading())
	{
		SetViewMode(m_stats.viewmode);
		SetItemFlag(eIF_PostSerializeSelect, m_stats.selected);
	}
}

//------------------------------------------------------------------------
void CItem::PostSerialize()
{
	IActor* pOwner = GetOwnerActor();
	
	if(pOwner)
	{
		if (ShouldDoPostSerializeReset())
		{
			if (IInventory* pInventory = pOwner->GetInventory())
			{
				const EntityId holstered = pInventory->GetHolsteredItem();
				bool  selectOnPickedUp = true;

				{
					CItem* pCurrentItem = static_cast<CItem*>(m_pItemSystem->GetItem(pInventory->GetCurrentItem()));

					//Select if:
					//1) Item was serialized as the selected one
					//2) The current item will never be auto selected on pick up, like binoculars, pick&throw or noweapon (this code path is not executed for it)
					selectOnPickedUp = AreAnyItemFlagsSet(eIF_PostSerializeSelect) || (pCurrentItem && !pCurrentItem->ShouldDoPostSerializeReset());
				}

				if (IsHeavyWeapon() && pOwner->IsClient())
				{
					Drop(0,false,false);
					Use( pOwner->GetEntityId() ); // pickup gets the weapon added into the inventory, and we dont want that for heavy weapons if is the player. 
				}
				else if (pOwner->GetInventory()->FindItem(GetEntityId()) != -1)
				{
					Drop(0,false,false);
					PickUp(pOwner->GetEntityId(), false, selectOnPickedUp, false);
				}

				if(m_stats.selected)
				{
					if(holstered != GetEntityId())
					{
						if(pOwner->GetEntity()->IsHidden())
						{
							Hide(true); //Some AI is hidden in the levels by designers
						}
					}      
				}
				else if(holstered != GetEntityId())
				{
					AttachToHand(false, true);
				}
			}
		}
	}
	else 
	{
		if (m_owner.GetId()!=0)
		{
			IEntity* pEntityOwner = gEnv->pEntitySystem->GetEntity( m_owner.GetId() );
			if (pEntityOwner)
				CryWarning(VALIDATOR_MODULE_GAME, VALIDATOR_ERROR, "PostSerialize(). Item %s has ownerId %i that is an entity (%s) but not an actor.", GetEntity()->GetName(), m_owner.GetId(), pEntityOwner->GetName());
			else
			{				
				CryWarning(VALIDATOR_MODULE_GAME, VALIDATOR_ERROR, "PostSerialize(). Item %s has ownerId %i but owner actor does not exist!", GetEntity()->GetName(), m_owner.GetId());
			}
		}

		//make sure to update the proximity grid for teleported items
		GetEntity()->InvalidateTM(ENTITY_XFORM_POS);
	}
	
	//Back attachments
	if(IsAttachedToBack())
	{
		// Set attachment back to 'None' so it can be correctly reattached again
		m_stats.attachment = eIA_None;
		AttachToBack(true);
	}

	if (m_stats.mounted && !m_hostId)
	{
		Vec3 olddir=m_stats.mount_dir;
		Vec3 oldaimdir=m_stats.mount_last_aimdir;
		MountAt(GetWorldPos());

		m_stats.mount_dir=olddir;
		m_stats.mount_last_aimdir=oldaimdir;
	}

	ReAttachAccessories();

	//this fix breaks holding NPC serialization, when "use" was pressed during saving
	/*if(pOwner && this == pOwner->GetCurrentItem() && !pOwner->GetLinkedVehicle())
	{
		pOwner->HolsterItem(true);	//this "fixes" old attachments that are not replaced still showing up in the model ..
		pOwner->HolsterItem(false);
	}*/

	//Fix incorrect view mode (in same cases) and not physicalized item after dropping/picking (in same cases too)
	if(!pOwner && m_stats.dropped)
	{
		SetViewMode(eIVM_ThirdPerson);

		Pickalize(true, false);
		GetEntity()->EnablePhysics(true);

		Physicalize(true, AreAnyItemFlagsSet(eIF_SerializeRigidPhysics));

		if(m_serializeActivePhysics.len())	//this fixes objects being frozen in air because they were rephysicalized
		{
			GetEntity()->AddImpulse(-1, Vec3(0.0075f,0,0.0075f), m_serializeActivePhysics*m_sharedparams->params.drop_impulse, true, 1.0f);
			m_serializeActivePhysics = Vec3(0,0,0);
		}
	}

	if(m_postSerializeMountedOwner)
	{
		IActor *pActor = gEnv->pGameFramework->GetIActorSystem()->GetActor(m_postSerializeMountedOwner);
		CPlayer *pPlayer = static_cast<CPlayer*> (pActor);
		if(pPlayer && m_sharedparams->params.usable)
		{
			m_stats.used = false;
			SetOwnerId(m_postSerializeMountedOwner);
			StopUse(pPlayer->GetEntityId());
			pPlayer->UseItem(GetEntityId());
			assert(m_owner.GetId());
		}
		m_postSerializeMountedOwner = 0;		
	}

	if(AreAnyItemFlagsSet(eIF_SerializeCloaked))
		CloakSync(false);
	else
		CloakEnable(false, false);

	if(AreAnyItemFlagsSet(eIF_SerializeDestroyed))
		OnDestroyed();

	//Benito: Force registration at the end of the serialization if needed to request audio/animation assets
	//        During serialization 'PickUp code' does not execute the whole code path.
	if (CActor* pActor = GetOwnerActor())
	{
		if (!pActor->IsDead())
		{
			RegisterAsUser();
		}
	}

	ClearItemFlags(eIF_PostSerializeSelect);
}

//------------------------------------------------------------------------
const TAccessoryParamsVector& CItem::GetAccessoriesParamsVector() const
{
	return m_sharedparams->accessoryparams;
}

//------------------------------------------------------------------------
void CItem::SerializeLTL( TSerialize ser )
{
	ser.BeginGroup("ItemLTLSerialization");

	//serialize attachments
	int attachmentAmount = m_accessories.size();
	ser.Value("attachmentAmount", attachmentAmount);

	if(ser.IsWriting())
	{
		for(int i = 0; i < attachmentAmount; i++)
		{
			string name(m_accessories[i].pClass->GetName());
			ser.BeginGroup("Accessory");
			ser.Value("Name", name);
			ser.EndGroup();
		}
	}
	else if(ser.IsReading())
	{
		m_accessories.clear();

		IEntityClassRegistry* pClassRegistry = gEnv->pEntitySystem->GetClassRegistry();
		string name;
		IActor *pActor = GetOwnerActor();
		for(int i = 0; i < attachmentAmount; ++i)
		{
			ser.BeginGroup("Accessory");
			ser.Value("Name", name);
			ser.EndGroup();

			if(pActor)
			{
				AddAccessory(pClassRegistry->FindClass(name));
			}
		}
		if(attachmentAmount)
		{
			ReAttachAccessories();

			if(!IsSelected())
			{
				Hide(true);
	
				const int numAccessories = m_accessories.size();
				
				for(int i = 0; i < numAccessories; i++)
				{
					if(CItem* pItem = static_cast<CItem*>(m_pItemSystem->GetItem(m_accessories[i].accessoryId)))
					{
						pItem->Hide(true);
					}
				}
			}
		}

	}

	ser.EndGroup();
}

//------------------------------------------------------------------------
void CItem::SetOwnerId(EntityId ownerId)
{
	if (m_owner.GetId() != ownerId)
	{
		if(ownerId)
		{
			g_pGame->GetTacticalManager()->RemoveEntity(GetEntityId(), CTacticalManager::eTacticalEntity_Item);
			CActorWeakPtr pActorWeak;
			if (CActor* pActor = static_cast<CActor*>(g_pGame->GetIGameFramework()->GetIActorSystem()->GetActor(ownerId)))
			{
				pActorWeak = pActor->GetWeakPtr();
			}
			m_owner.Set(ownerId, pActorWeak);
		}
		else
		{
			g_pGame->GetTacticalManager()->AddEntity(GetEntityId(), CTacticalManager::eTacticalEntity_Item);
			m_owner.Reset();
		}
		UpdateCurrentActionController();
	}
}

//------------------------------------------------------------------------
EntityId CItem::GetOwnerId() const
{
	return m_owner.GetId();
}

//------------------------------------------------------------------------
void CItem::SetParentId(EntityId parentId)
{
	m_parentId = parentId;
}

//------------------------------------------------------------------------
EntityId CItem::GetParentId() const
{
	return m_parentId;
}

//------------------------------------------------------------------------
void CItem::SetHand(int hand)
{
	m_stats.hand = hand;

	bool ok=true;

	CItem *pParent=m_parentId?static_cast<CItem*>(m_pItemSystem->GetItem(m_parentId)):NULL;
	if (pParent)
			ok = pParent->IsSelected();

	if (m_stats.mounted || (pParent && pParent->GetStats().mounted))
		ok=true;

	if (m_stats.viewmode&eIVM_FirstPerson && ok)
	{
		const SGeometryDef* pGeomDef = m_sharedparams->GetGeometryForSlot(eIGS_FirstPerson);
		if(pGeomDef)
		{
			SetGeometry(eIGS_FirstPerson, pGeomDef->modelPath, pGeomDef->material, pGeomDef->useParentMaterial, pGeomDef->pos, pGeomDef->angles, pGeomDef->scale);
		}
	}
}

//------------------------------------------------------------------------
void CItem::SwitchToHand(int hand)
{
	if(m_stats.hand != hand)
	{
		AttachToBack(false);

		AttachToHand(false);
		SetHand(hand);
		AttachToHand(true);

		if (CRecordingSystem* pRecordingSystem = g_pGame->GetRecordingSystem())
		{
			pRecordingSystem->OnItemSwitchToHand(this, hand);
		}
	}
}

//------------------------------------------------------------------------
void CItem::Use(EntityId userId)
{
	if (m_sharedparams->params.usable && m_stats.mounted)
	{
		if (!m_owner.GetId())
			StartUse(userId);
		else if (m_owner.GetId() == userId)
			StopUse(userId);
	}
}

//------------------------------------------------------------------------
struct CItem::SelectAction
{
	void execute(CItem *_item)
	{
		_item->SetBusy(false);
		_item->ForcePendingActions();
		_item->ClearItemFlags(eIF_Selecting);
	}
};

//------------------------------------------------------------------------
void CItem::DoSelectWeaponGrab()
{
	CRY_ASSERT_MESSAGE(AreAnyItemFlagsSet(eIF_SelectGrabbingWeapon), "Triggering delayed weapon grab when weapon is not expecting it!");

	ClearItemFlags(eIF_SelectGrabbingWeapon);
	Hide(false);
	AttachToBack(false);
	AttachToHand(true);
}


void CItem::Select(bool select)
{
	bool wasSelected = m_stats.selected;
	
	if(!m_owner.GetId())
	{
		select = false;
	}

	CActor* pOwnerActor = GetOwnerActor();

	m_itemLowerMode = eILM_Raised;
	m_stats.selected=select;

	CheckViewChange();

	IAISystem* pAISystem = gEnv->pAISystem;

	if(select != wasSelected)
	{
		UpdateActionControllerSelection(select);
	}

	if(m_pSelectAction)
	{
		m_pSelectAction->ItemSelectCancelled();
	}
	m_pSelectAction.reset();

	if (select)
	{
		const bool isTP = (pOwnerActor && pOwnerActor->IsThirdPerson());
		const bool playSelect = ShouldPlaySelectAction();
		const bool itemWantsToSelect = CItem::ShouldPlaySelectAction();
		const bool itemWantsToDelayUnstow = (itemWantsToSelect && isTP && m_sharedparams->params.select_delayed_grab_3P && (m_stats.attachment != eIA_None));
		const float selectBiasTime = GetFastSelectSpeedBias( this, m_unholsteringSelectBiasTime );
		bool canDoDelayedUnstow = true;

		if (!m_stats.mounted && GetOwner())
		{
		  GetEntity()->SetWorldTM(GetOwner()->GetWorldTM());	// move somewhere near the owner so the sound can play
		}
		
		const float speedOverride = GetSelectSpeed(pOwnerActor);
	
		//LAW, shotgun have 2 different select animations
		if (!IsMounted() && playSelect)
		{
			if (IActionController *pActionController = GetActionController())
			{
				CMannequinUserParamsManager& mannequinUserParams = g_pGame->GetIGameFramework()->GetMannequinInterface().GetMannequinUserParamsManager();

				const SMannequinItemParams* pParams = mannequinUserParams.FindOrCreateParams<SMannequinItemParams>(pActionController);

				const SMannequinItemParams::Fragments::SSelect& selectFragment = pParams->fragments.Select;

				const CTagDefinition* pTagDefinition = selectFragment.pTagDefinition;
				const FragmentID fragID = selectFragment.fragmentID;
			
				if (fragID != FRAGMENT_ID_INVALID)
				{
					TagState actionTags = TAG_STATE_EMPTY;

					CRY_ASSERT(pTagDefinition);
					
					CTagState fragTags(*pTagDefinition);

					if (AreAnyItemFlagsSet(eIF_Unholstering|eIF_UseFastSelectTag))
					{
						fragTags.Set(selectFragment.fragmentTagIDs.fast_select, true);
					}

					if (m_stats.first_selection)
					{
						if(m_properties.specialSelect)
						{
							fragTags.Set(selectFragment.fragmentTagIDs.special_first, true);
						}
						else
						{
							fragTags.Set(selectFragment.fragmentTagIDs.first, true);
						}
					}

					if (m_stats.attachment == eIA_StowPrimary)
					{
						fragTags.Set(selectFragment.fragmentTagIDs.primary, true);
					}
					else if (m_stats.attachment == eIA_StowSecondary)
					{
						fragTags.Set(selectFragment.fragmentTagIDs.secondary, true);
					}

					SetFragmentTags(fragTags);

					actionTags = fragTags.GetMask();
					
					CRY_ASSERT(!m_pSelectAction);
					m_pSelectAction = new CItemSelectAction(PP_PlayerAction, fragID, actionTags, *this);

					if( AreAnyItemFlagsSet(eIF_PlayFastSelectAsSpeedBias) )
					{
						m_pSelectAction->SetSpeedBias( g_pGameCVars->i_fastSelectMultiplier );
					}

					PlayFragment(m_pSelectAction, speedOverride);
				}
				else
				{
					canDoDelayedUnstow = false;
				}
			}
		}

		bool isUnholstering = AreAnyItemFlagsSet(eIF_Unholstering);

		ClearItemFlags(eIF_Unholstering|eIF_PlayFastSelect|eIF_PlayFastSelectAsSpeedBias|eIF_UseFastSelectTag);
		
		m_stats.first_selection = false;

		uint32 selectBusyTimer = 0;
		if (m_sharedparams->params.select_override == 0.0f)
			selectBusyTimer = max(250U, (uint32)(selectBiasTime * GetCurrentAnimationTime(eIGS_Owner))) - 250;
		else
			selectBusyTimer = (uint32)(m_sharedparams->params.select_override*selectBiasTime*1000);
		SetBusy(true);
		GetScheduler()->TimerAction(selectBusyTimer, CSchedulerAction<SelectAction>::Create(), false);
		SetItemFlags(eIF_Selecting);

		const bool delayedUnstow = (itemWantsToDelayUnstow && canDoDelayedUnstow);
		if (!delayedUnstow)
		{
			if((!isUnholstering && GetEntity()->GetClass() != sFragHandGrenadesClass) || !playSelect)
			{
				Hide(false);
				AttachToBack(false);
				if (ShouldAttachWhenSelected())
				{
					AttachToHand(true);
				}
			}
			else
			{
				SetItemFlags( eIF_UseAnimActionUnhide );
			}

			if (m_stats.attachment == eIA_WeaponCharacter && !AreAnyItemFlagsSet(eIF_UseAnimActionUnhide))
			{
				//--- Disable rendering of the char attachment for a frame to fix the animation lag causing a glitch (TomB)
				//--- Only stop for one frame when doing a fastSelect (SteveN).
				m_delayedUnhideCntr = playSelect ? 2 : 1;
				IAttachmentManager *pAttachmentManager = GetOwnerAttachmentManager();
				if (pAttachmentManager)
				{
					IAttachment *pAttachment = pAttachmentManager->GetInterfaceByName(m_sharedparams->params.attachment[m_stats.hand].c_str());
					if (pAttachment)
					{
						pAttachment->HideAttachment(true);
					}
				}
			}
		}

		SetItemFlag(eIF_SelectGrabbingWeapon, delayedUnstow);

		if (pOwnerActor)
		{
			IEntity* pOwnerEntity = pOwnerActor->GetEntity();

			// update smart objects states
			if (!gEnv->bMultiplayer && pAISystem && pAISystem->GetSmartObjectManager())
			{
				pAISystem->GetSmartObjectManager()->ModifySmartObjectStates( pOwnerEntity, GetEntity()->GetClass()->GetName() );
				pAISystem->GetSmartObjectManager()->ModifySmartObjectStates( pOwnerEntity, "WeaponDrawn" );
			}
		}    

		for (unsigned int i = 0; i < m_accessories.size(); ++i)
		{
			const SAccessoryInfo& accessory = m_accessories[i];
			int slot = m_stats.fp ? 0 : 1;
			const SAccessoryParams* pParams = GetAccessoryParams(accessory.pClass);
			ShowAttachmentHelper(slot, pParams->show_helper.c_str(), true);
		}
	}
	else
	{
		GetScheduler()->Reset(true);

		AttachToHand(false);
		if (!m_stats.mounted && !AttachToBack(true))
		{
			SetViewMode(0);
			Hide(true);
		}

		EnableUpdate(false);

		if(!m_stats.dropped) 
		{
			ResetAccessoriesScreen(pOwnerActor);
		}

		if (m_stats.mounted)
		{
			m_stats.fp = false; // so that OnEnterFirstPerson is called next select
		}

		// update smart objects states
		if ( !gEnv->bMultiplayer && pAISystem && pAISystem->GetSmartObjectManager() && pOwnerActor && !pOwnerActor->IsDead() )
		{
			CryFixedStringT<256> tmpString( "-WeaponDrawn," );
			tmpString += GetEntity()->GetClass()->GetName();
			pAISystem->GetSmartObjectManager()->ModifySmartObjectStates( pOwnerActor->GetEntity(), tmpString.c_str() );
		}

		RemoveOwnerAttachedAccessories();
	}

	// ensure attachments get cloaked
	if (select)
	{
		CloakSync(false);
	}
	else
	{
		CloakEnable(false, false);
	}

	OnSelected(select);
}

//------------------------------------------------------------------------
void CItem::UpdateActionControllerSelection(bool bSelected)
{
	if (IActionController *pActionController = GetActionController())
	{
		CTagState &tagState = (m_subContext == TAG_ID_INVALID) ? pActionController->GetContext().state : pActionController->GetContext().subStates[m_subContext];
		UpdateTags(pActionController, tagState, bSelected);

		if(CActor * pActor = GetOwnerActor())
		{
			if(pActor->IsClient() || gEnv->bMultiplayer)
			{
				if(bSelected)
				{
					UpdateScopeContexts(pActionController);
				}
				else
				{
					ClearScopeContexts(pActionController);
				}
			}
		}
	}
}

//------------------------------------------------------------------------
void CItem::Unholstering(bool playSelect, float biasSelectTime)
{
	SetItemFlags(eIF_Unholstering);
	SetItemFlag(eIF_UnholsteringPlaySelect, playSelect);
	m_unholsteringSelectBiasTime = biasSelectTime;
}

//------------------------------------------------------------------------
uint32 CItem::StartDeselection(bool fastDeselect)
{
	return 0;
}

//------------------------------------------------------------------------
void CItem::CancelDeselection()
{

}

//------------------------------------------------------------------------
bool CItem::IsDeselecting() const
{
	return false;
}

void CItem::DetachItem(IEntity* pThisItemEntity, CActor* pOwnerActor, float impulseScale, Vec3 localDropDirection)
{
	if(!m_stats.detached)
	{
		m_stats.detached = true;

		AttachToHand(false,true);
		AttachToBack(false);

		SetViewMode(eIVM_ThirdPerson);

		if(pOwnerActor)
		{
			pThisItemEntity->EnablePhysics(true);
			Physicalize(true, true);
		}

		DisableCollisionWithPlayers();

		if (pOwnerActor && gEnv->bServer)
		{
			// bump network priority for a moment
			pOwnerActor->GetGameObject()->Pulse('bang');
			GetGameObject()->Pulse('bang');

			if (IMovementController * pMC = pOwnerActor->GetMovementController())
			{
				SMovementState moveState;
				pMC->GetMovementState(moveState);

				Vec3 dropDirection(0.0f,0.0f,-1.0f);
				Vec3 dropVelocity(ZERO);

				if(pOwnerActor->IsPlayer())
				{
					if(IPhysicalEntity *pItemPhysicsEntity = pThisItemEntity->GetPhysics())
					{
						IEntity* pOwnerEntity = pOwnerActor->GetEntity();
						const Vec3& viewPosition = moveState.eyePosition;

						if(localDropDirection.IsZero())
						{
							dropDirection = moveState.aimDirection;
						}
						else
						{
							dropDirection = pOwnerEntity->GetRotation() * localDropDirection;
						}

						dropDirection.z = -1.f;

						SActorStats* pActorStats = pOwnerActor->GetActorStats();
						assert(pActorStats);

						const Vec3 velocity = pOwnerActor->GetActorPhysics().velocity;

						if (velocity.len2() > 0.0f)
						{
							const float dot = clamp_tpl(velocity.GetNormalized().Dot(dropDirection), 0.0f, 1.0f);
							dropVelocity = velocity * dot;
						}

						int numSkipEntities = 1;
						IPhysicalEntity* ignoreList[2];

						ignoreList[0] = pItemPhysicsEntity;

						if(IPhysicalEntity* pOwnerPhysics = pOwnerEntity->GetPhysics())
						{
							ignoreList[1] = pOwnerPhysics;
							numSkipEntities++;
						}

						if (dropVelocity.len2() > 0.0f)
						{
							pe_action_set_velocity asv;
							asv.v = dropVelocity;

							pItemPhysicsEntity->Action(&asv);
						}			

						m_dropPosition = pOwnerEntity->GetWorldPos();
						m_dropImpulse = dropDirection*m_sharedparams->params.drop_impulse*impulseScale;

						int entityTypes = ent_static|ent_terrain|ent_rigid|ent_sleeping_rigid;
						int flags = rwi_stop_at_pierceable|14;

						if (m_dropRayId)
							g_pGame->GetRayCaster().Cancel(m_dropRayId);
						
						m_dropRayId = g_pGame->GetRayCaster().Queue(RayCastRequest::HighPriority,
																												RayCastRequest(viewPosition-dropDirection*0.25f, dropDirection,
																												entityTypes, flags, ignoreList, numSkipEntities),
																												functor(*this, &CItem::DropAfterRaycast));
					}
				}
			}
		}
	}
}

//------------------------------------------------------------------------
void CItem::Drop(float impulseScale, bool selectNext, bool byDeath)
{
	CActor *pOwnerActor = GetOwnerActor();
	IInventory *pInventory = GetActorInventory(pOwnerActor);
	const bool ownerWasAI = pOwnerActor ? !pOwnerActor->IsPlayer() : false;

	IEntity* pThisItemEntity = GetEntity();
	EntityId thisItemEntityId = pThisItemEntity->GetId();

	if (pOwnerActor)
	{
		if (pInventory && (pInventory->GetCurrentItem() == thisItemEntityId))
		{
			CCCPOINT(Item_DroppedWhileCurrentItem);
			g_pGame->GetIGameFramework()->GetIItemSystem()->SetActorItem(pOwnerActor, (EntityId)0, false);
		}
		else
		{
			CCCPOINT(Item_DroppedWhileNotCurrentItem);
		}

		if (IsServer()) 
		{
			GetGameObject()->SetNetworkParent(0);

			if (!byDeath)
			{
				if ((pThisItemEntity->GetFlags()&(ENTITY_FLAG_CLIENT_ONLY|ENTITY_FLAG_SERVER_ONLY)) == 0)
				{
					pOwnerActor->GetGameObject()->InvokeRMI(CActor::ClDrop(), CActor::DropItemParams(thisItemEntityId, selectNext, byDeath), eRMI_ToAllClients|eRMI_NoLocalCalls);
				}
			}
		}

		if( !pOwnerActor->IsPlayer() )
		{
			m_stats.first_selection = true;
		}
	}
	else
	{
		CCCPOINT(Item_DroppedAndHasNoOwner);
	}

	if (pThisItemEntity->IsHidden())
	{
		IAttachmentManager* pAttachmentManager = GetOwnerAttachmentManager();
		IAttachment* pAttachment = 0;
		if (pAttachmentManager)
		{
			if (pOwnerActor && !pOwnerActor->IsPlayer())
				pAttachment = pAttachmentManager->GetInterfaceByName(m_sharedparams->params.aiAttachment[m_stats.hand].c_str());
			if (!pAttachment)
				pAttachment = pAttachmentManager->GetInterfaceByName(m_sharedparams->params.attachment[m_stats.hand].c_str());
			if (pAttachment)
			{
				QuatTS pose = pAttachment->GetAttWorldAbsolute();
				pThisItemEntity->SetPosRotScale(pose.t, pose.q, Vec3(1.0f, 1.0f, 1.0f));
			}
		}
	}

	m_stats.dropped = true;

	if(pOwnerActor && m_stats.used)
	{
		StopUse(pOwnerActor->GetEntityId());
	}

	bool performCloakFade = AreAnyItemFlagsSet(eIF_Cloaked);

	Select(false);
	
	Hide(false);
	SetViewMode(eIVM_ThirdPerson);

	if (performCloakFade)
	{
		CloakEnable(false, true);
	}

	// AI should ignore collisions from this item for a while
	// to not 'scare' himself and the friends around him
	if (IPerceptionManager::GetInstance())
	{
		IPerceptionManager::GetInstance()->IgnoreStimulusFrom(pThisItemEntity->GetId(), AISTIM_COLLISION, 2.0f);
	}

	DetachItem(pThisItemEntity, pOwnerActor, impulseScale);

	EntityId ownerId = GetOwnerId();

	if (pInventory)
	{
		pInventory->RemoveItem(thisItemEntityId);
	}

	SetOwnerId(0);

	Pickalize(true, true);
	EnableUpdate(false);

	if (g_pGame->GetGameRules())
	{
		g_pGame->GetGameRules()->OnItemDropped(GetEntityId(), pOwnerActor?pOwnerActor->GetEntityId():0);
	}

	if (pOwnerActor && pOwnerActor->IsClient())
	{
		ResetAccessoriesScreen(pOwnerActor);

		if (selectNext && !pOwnerActor->IsDead())
		{
			CBinocular *pBinoculars = static_cast<CBinocular*>(pOwnerActor->GetItemByClass(CItem::sBinocularsClass));

			if (pInventory && pInventory->GetLastItem()
				  && (!pBinoculars || (pBinoculars->GetEntityId() != pInventory->GetLastItem())))
			{
				pOwnerActor->SelectLastItem(false, true);
			}
			else
			{
				pOwnerActor->SelectNextItem(1, false, eICT_Primary|eICT_Secondary);
			}
		}

		if (CanSelect())
		{
			m_pItemSystem->DropActorItem(pOwnerActor, GetEntityId());
		}
		else
		{
			m_pItemSystem->DropActorAccessory(pOwnerActor, GetEntityId());
		}
	}

	CloakEnable(false, false);

	Quiet();

	OnDropped(ownerId, ownerWasAI);
}

//------------------------------------------------------------------------
void CItem::DropAfterRaycast(const QueuedRayID& rayID, const RayCastResult& result)
{
	if (result.hitCount == 0)
	{
		const Vec3 impulsePoint = GetEntity()->GetWorldTM().TransformPoint(Vec3(0.0075f,0,0.0075f));
		GetEntity()->AddImpulse(-1, impulsePoint, m_dropImpulse, true, 1.0f);
	}
	else
	{
		Matrix34 worldTM = GetEntity()->GetWorldTM();
		worldTM.SetTranslation(m_dropPosition);
		GetEntity()->SetWorldTM(worldTM);
	}
	m_dropRayId = 0;
}

//------------------------------------------------------------------------
void CItem::PickUp(EntityId pickerId, bool sound, bool select, bool keepHistory, const char* setup)
{
	CActor *pPickerActor = GetActor(pickerId);
	if (!pPickerActor)
		return;

	if (pPickerActor->IsPlayer())
		UnlockDefaultAttachments(pPickerActor);

	CCCPOINT_IF( sound &&  select, Item_PickUpAndSelect_PlaySound);
	CCCPOINT_IF( sound && !select, Item_PickUpAndStore_PlaySound);
	CCCPOINT_IF(!sound &&  select, Item_PickUpAndSelect_NoSound);
	CCCPOINT_IF(!sound && !select, Item_PickUpAndStore_NoSound);

	bool isServer = IsServer();
	IEntity* pPickerEntity = pPickerActor->GetEntity();
	IEntity* pThisItemEntity = GetEntity();
	EntityId thisItemEntityId = pThisItemEntity->GetId();
	
	TriggerRespawn();

	pThisItemEntity->EnablePhysics(false);
	Physicalize(false, false);
	Pickalize(false, false);

	bool soundEnabled = IsSoundEnabled();
	EnableSound(sound);

	SetViewMode(0);		
	SetOwnerId(pickerId);

	CopyRenderFlags(GetOwner());

	Hide(true);
	m_stats.dropped = false;
	m_stats.brandnew = false;
	m_stats.detached = false;

	// move the entity to picker position
	Matrix34 tm(pPickerEntity->GetWorldTM());
	tm.AddTranslation(Vec3(0,0,2));
	pThisItemEntity->SetWorldTM(tm);

	IInventory *pInventory = pPickerActor->GetInventory();
	if (!pInventory)
	{
		GameWarning("Actor '%s' has no inventory, when trying to pickup '%s'!",pPickerEntity->GetName(), pThisItemEntity->GetName());
		return;
	}

	const char* itemClassName = GetEntity()->GetClass()->GetName();
	bool itemClassNotInInventory = (pInventory->GetCountOfClass(itemClassName) == 0);

	PickUpAccessories(pPickerActor, pInventory);

	OnPickedUp(pickerId, m_sharedparams->params.unique && !itemClassNotInInventory);	
	// Note:
	// some weapons will get ammo when picked up
	// this will dictate the result of CanSelect() below
	// so we'll grab the ammo before trying to select

	if (itemClassNotInInventory || !m_sharedparams->params.unique)
	{
		// add to inventory
		pInventory->AddItem(GetEntity()->GetId());

		if (select && itemClassNotInInventory)
		{
			//Note [Benito]:
			// Some of this checks here fix some issues with Save/Load, some code in CItem::PostSerialize messes up 
			// the inventory serialization by calling Drop/Pick, because the order in which the items are serialized can be any.
			// Not sure behind the reasons for such a code in PostSerialize
			const bool hasHeavyWeaponEquipped = (pPickerActor->GetActorClass() == CPlayer::GetActorClassType()) ? static_cast<CPlayer*>(pPickerActor)->HasHeavyWeaponEquipped() : false;

			const bool shouldCheck = (pPickerActor->IsClient() || gEnv->bServer);
			const bool allowSelectOnPickUp = !pPickerActor->GetLinkedVehicle() && !pPickerActor->IsSwimming() && (pPickerActor->GetGrabbedEntityId() == 0) && !hasHeavyWeaponEquipped;
			if (!shouldCheck || allowSelectOnPickUp)
			{
				if(CanSelect())
				{
					m_pItemSystem->SetActorItem(pPickerActor, GetEntity()->GetId(), keepHistory);
				}
				else
				{
					m_pItemSystem->SetActorAccessory(pPickerActor, GetEntity()->GetId(), keepHistory);
				}

				CHANGED_NETWORK_STATE(pPickerActor, CPlayer::ASPECT_CURRENT_ITEM);
			}
		}

		EnableSound(soundEnabled);

		if (g_pGame->GetGameRules())
		{
			g_pGame->GetGameRules()->OnItemPickedUp(thisItemEntityId, pickerId);
		}

		if (select)
		{
			PlayAction(GetFragmentIds().pickedup);
		}

		//AI back weapon attachments
		if(!IsSelected())
		{
			AttachToBack(true);
		}
	}
	else if (m_sharedparams->params.unique && !itemClassNotInInventory)
	{
		if (isServer && g_pGame->GetGameRules())
		{
			g_pGame->GetGameRules()->OnItemPickedUp(GetEntityId(), pickerId);
		}

		if (isServer && !IsDemoPlayback())
		{
			RemoveEntity();
		}

		if (select)
		{
			PlayAction(GetFragmentIds().pickedup);
		}
	}

	if (isServer && pPickerActor)
	{
		//--- We reset the client's inventories at the point where we add the first item to the server inventory
		//--- this ensures that they are all kept in synch
		GetGameObject()->SetNetworkParent(pickerId);
		if ((pThisItemEntity->GetFlags()&(ENTITY_FLAG_CLIENT_ONLY|ENTITY_FLAG_SERVER_ONLY)) == 0)
		{
			pPickerActor->GetGameObject()->InvokeRMIWithDependentObject(CActor::ClPickUp(), CActor::PickItemParams(thisItemEntityId, m_stats.selected, sound, false), eRMI_ToAllClients|eRMI_NoLocalCalls, thisItemEntityId);
		}
	}

	if (!gEnv->bMultiplayer)
	{
		int numAccessories = m_accessories.size();
		for (int i = 0; i < numAccessories; i++)
		{
			const SAccessoryParams* pParams = GetAccessoryParams(m_accessories[i].pClass);
			if (pParams->extendsMagazine)
			{
				const char* name = m_accessories[i].pClass->GetName();
				AttachAccessory(m_accessories[i].pClass, false, true, true);
				DoSwitchAccessory(name);
				break;
			}
		}
	}
	else
	{
		//add any attachemnts on the weapon to the "this life's allowed detachments"
		if( pPickerActor->GetEntityId() == gEnv->pGameFramework->GetClientActorId() )
		{
			if( CEquipmentLoadout* pLoadout = g_pGame->GetEquipmentLoadout() )
			{
				int numAccessories = m_accessories.size();
				for (int i = 0; i < numAccessories; i++)
				{
					pLoadout->UpdateWeaponAttachments( GetEntity()->GetClass(), m_accessories[i].pClass );
				}
			}
		}
	}
}

//------------------------------------------------------------------------
void CItem::UnlockDefaultAttachments(CActor* pActor)
{
	IInventory* pInventory = pActor->GetInventory();

	const TDefaultAccessories& defaultAccessoires = GetSharedItemParams()->defaultAccessories;
	int numDefaultAccessories = defaultAccessoires.size();
	for (int i = 0; i < numDefaultAccessories; ++i)
	{
		pInventory->AddAccessory(defaultAccessoires[i]);
	}
}

//------------------------------------------------------------------------
CItem::ePhysicalization CItem::FindPhysicalisationType(bool enable, bool rigid)
{
	ePhysicalization physType = eIPhys_NotPhysicalized;

	if(enable)
	{
		physType = rigid ? eIPhys_PhysicalizedRigid : eIPhys_PhysicalizedStatic;
	}

	return physType;
}

//------------------------------------------------------------------------
void CItem::DeferPhysicalize(bool enable, bool rigid)
{
	m_deferPhysicalization = FindPhysicalisationType(enable, rigid);

	RequireUpdate(eIUS_General);
}

//------------------------------------------------------------------------
void CItem::Physicalize(bool enable, bool rigid)
{
	if (IsServer())
		GetGameObject()->SetAspectProfile(eEA_Physics, FindPhysicalisationType(enable, rigid));

	m_deferPhysicalization = eIPhys_Max;
}

//------------------------------------------------------------------------
void CItem::Pickalize(bool enable, bool dropped)
{
	if (enable)
	{
		m_stats.flying = dropped;
		m_stats.dropped = true;
		m_stats.pickable = true;

		if(dropped)
		{
			const int dropFlyTimeout = 750;
			GetEntity()->KillTimer(eIT_Flying);
			GetEntity()->SetTimer(eIT_Flying, dropFlyTimeout);
		}

		if(GetEntity()->IsSlotValid(eIGS_Aux0))
		{
			DrawSlot(eIGS_Aux0, true);
			DrawSlot(eIGS_FirstPerson, false);
			DrawSlot(eIGS_ThirdPerson, false);
		}
	}
	else
	{
		if(GetEntity()->IsSlotValid(eIGS_Aux0))
		{
			DrawSlot(eIGS_Aux0, false);
		}

		m_stats.flying = false;
		m_stats.pickable = false;
	}
}

//----------------------------------------------------------------------
void CItem::DisableCollisionWithPlayers()
{
	IPhysicalEntity *pPhysicalEntity = GetEntity()->GetPhysics();
	if (pPhysicalEntity != NULL)
	{
		pe_params_part pp;
		pp.flagsAND = ~geom_colltype_player;

		pPhysicalEntity->SetParams(&pp);
	}
}

//----------------------------------------------------------------------
void CItem::RegisterFPWeaponForRenderingAlways(bool registerRenderAlways)
{
	auto params = GetEntity()->GetRenderNodeParams();
	if (registerRenderAlways)
		params.additionalRenderNodeFlags |= ERF_RENDER_ALWAYS;
	else
		params.additionalRenderNodeFlags &= ~ERF_RENDER_ALWAYS;
	GetEntity()->SetRenderNodeParams(params);
}
//------------------------------------------------------------------------
void CItem::Impulse(const Vec3 &position, const Vec3 &direction, float impulse)
{
	if (direction.len2() <= 0.001f)
		return;

	GetEntity()->AddImpulse(-1, position, direction.GetNormalized()*impulse, true, 1);
}

//------------------------------------------------------------------------
void CItem::LowerItem(eItemLowerMode mode)
{
	if (mode == eILM_Raised)
		return;

	IWeapon* pIWeapon = GetIWeapon();
	if (pIWeapon)
	{
		IFireMode* pCurrentFiremode = pIWeapon->GetFireMode(pIWeapon->GetCurrentFireMode());
		if (pCurrentFiremode && pCurrentFiremode->IsFiring())
			pIWeapon->StopFire();
	}

	if (mode == eILM_Lower)
	{
		SHUDEventWrapper::CantFire(); // Show can't fire hud feedback
	}

	if (m_itemLowerMode == eILM_Raised && mode == eILM_Lower)
	{
		PlayAction(GetFragmentIds().friendly_enter, 0, false, eIPAF_Default);
		m_itemLowerMode = eILM_Lower;
	}
	else if (m_itemLowerMode != eILM_Cinematic && mode == eILM_Cinematic)
	{
		PlayAction(GetFragmentIds().weapon_lower_enter, 0, false, eIPAF_Default);
		m_itemLowerMode = eILM_Cinematic;
	}
}

//------------------------------------------------------------------------
void CItem::UnlowerItem()
{
	if (IsLowered() && !IsDeselecting())
	{
		if (m_itemLowerMode == eILM_Lower)
			PlayAction(GetFragmentIds().friendly_leave);
		else if (m_itemLowerMode == eILM_Cinematic)
			PlayAction(GetFragmentIds().weapon_lower_leave);
		OnUnlowerItem();
		m_itemLowerMode = eILM_Raised;
	}
}

//------------------------------------------------------------------------
bool CItem::CanPickUp(EntityId userId) const
{
	CActor* pOwner = GetOwnerActor();
	bool hasOwner = pOwner != 0 && !pOwner->IsDead();
	bool canPickUp = m_sharedparams->params.pickable && m_stats.pickable && m_properties.pickable && !m_stats.flying && !hasOwner && !GetEntity()->IsHidden();
	if (!canPickUp)
		return false;

	int teamId = g_pGame->GetGameRules()->GetTeam(GetEntity()->GetId());
	if (teamId && teamId != g_pGame->GetGameRules()->GetTeam(userId))
		return false;

	CActor* pActor = GetActor(userId);
	if (pActor && pActor->IsSwimming())
		return false;

	return true;
}

//------------------------------------------------------------------------
bool CItem::CanDrop() const
{
	if (m_sharedparams->params.droppable)
		return true;

	return false;
}

//------------------------------------------------------------------------
bool CItem::CanUse(EntityId userId) const
{
	bool bUsable = m_sharedparams->params.usable && m_properties.usable && IsMounted() && (!m_stats.used || (m_owner.GetId() == userId) && !GetEntity()->IsHidden());
	if (!bUsable)
		return (false);

	if (IsMounted() && (m_owner.GetId() != userId))
	{	
		// if not a machine gun on a vehicle, check if we're in front of it 
		CActor *pActor = GetActor(userId);
		if (!pActor)
			return (true); 
		if (pActor->GetLinkedVehicle())
			return (true);

		Vec3 vActorDir = pActor->GetEntity()->GetWorldTM().TransformVector(FORWARD_DIRECTION);
		
		Vec3 mountDirection = GetEntity()->GetParent() ? GetEntity()->GetParent()->GetWorldTM().TransformVector(m_stats.mount_dir) : m_stats.mount_dir;
		float fDot = vActorDir * mountDirection;

		if (fDot<0)
			return (false);		
	}

	return (true);
}

//------------------------------------------------------------------------
bool CItem::IsMounted() const
{
	return m_stats.mounted;
}

//-------------------------------------------------------------------------
bool CItem::IsMountable() const 
{ 
	return m_sharedparams->params.mountable; 
}

//--------------------------------------------------------------------------
const SParams &CItem::GetParams() const 
{ 
	return m_sharedparams->params; 
};

//---------------------------------------------------------------------
bool CItem::GivesAmmo() 
{ 
	return (AreAnyItemFlagsSet(eIF_AccessoryAmmoAvailable) && !m_sharedparams->bonusAccessoryAmmo.empty() || m_sharedparams->params.attachment_gives_ammo);
}

//---------------------------------------------------------------------
bool CItem::IsAutoDroppable() const
{
	return m_sharedparams->params.auto_droppable;
}

//---------------------------------------------------------------------
bool CItem::CanPickUpAutomatically() const
{
	return m_sharedparams->params.auto_pickable;
}

//------------------------------------------------------------------------
void CItem::SetMountedAngleLimits(float min_pitch, float max_pitch, float yaw_range)
{
	m_properties.mounted_min_pitch = min(min_pitch,max_pitch);//(min_pitch<=0.0f)?min_pitch:0.0f; //Assert values
	m_properties.mounted_max_pitch = max(min_pitch,max_pitch);//(max_pitch>=0.0f)?max_pitch:0.0f;
	m_properties.mounted_yaw_range = yaw_range;
}


//------------------------------------------------------------------------
Vec3 CItem::GetMountedAngleLimits() const
{
	if(m_stats.mounted)
		return Vec3(m_properties.mounted_min_pitch, m_properties.mounted_max_pitch, m_properties.mounted_yaw_range);
	else 
		return ZERO;
}

//------------------------------------------------------------------------
bool CItem::IsUsed() const
{
	return m_stats.used;
}

//------------------------------------------------------------------------
bool CItem::InitRespawn()
{
	if (m_respawnprops.respawn)
	{
		CGameRules *pGameRules=g_pGame->GetGameRules();
		assert(pGameRules);
		if (pGameRules)
			pGameRules->CreateEntityRespawnData(GetEntityId());

		return true;
	}

	return false;
};

//------------------------------------------------------------------------
void CItem::TriggerRespawn()
{
	if (!m_stats.brandnew)
		return;
	
	if (m_respawnprops.respawn)
	{

		CGameRules *pGameRules=g_pGame->GetGameRules();
		assert(pGameRules);
		if (pGameRules)
			pGameRules->ScheduleEntityRespawn(GetEntityId(), m_respawnprops.unique, m_respawnprops.timer);
	}
}

//------------------------------------------------------------------------
bool CItem::CanSelect() const
{
	return m_sharedparams->params.selectable;
}

//------------------------------------------------------------------------
bool CItem::CanDeselect() const
{
	return !IsSelecting();
}

//------------------------------------------------------------------------
bool CItem::IsSelecting() const
{
	return AreAnyItemFlagsSet(eIF_Selecting);
}

//------------------------------------------------------------------------
bool CItem::IsSelected() const
{
	return m_stats.selected;
}

//------------------------------------------------------------------------
void CItem::UpdateTags(const IActionController *pActionController, class CTagState &tagState, bool selected) const
{
	uint32 weaponClassCrC = CCrc32::ComputeLowercase(GetParams().itemClass.c_str());
	uint32 weaponTypeCrC  = CCrc32::ComputeLowercase(GetParams().tag.c_str());
	
	tagState.SetByCRC(weaponClassCrC, selected);
	tagState.SetByCRC(weaponTypeCrC, selected);

	CMannequinUserParamsManager& mannequinUserParams = g_pGame->GetIGameFramework()->GetMannequinInterface().GetMannequinUserParamsManager();
	const SMannequinItemParams* pParams = mannequinUserParams.FindOrCreateParams<SMannequinItemParams>(pActionController);

	if(pParams && pParams->tagIDs.shoulder.IsValid())
	{
		tagState.Set(pParams->tagIDs.shoulder, true);
	}

	UpdateMountedTags(pParams, tagState, selected);
	UpdateAccessoryTags(pParams, tagState, selected);
}

//------------------------------------------------------------------------
void CItem::UpdateScopeContexts( IActionController *pController, int nCharacterSlot /*= 0*/ )
{
	bool foundTop = false;
	bool foundBottom = false;

	nCharacterSlot = IsOwnerFP() ? eIGS_FirstPerson : eIGS_ThirdPerson;

	IMannequin &mannequinSys = gEnv->pGameFramework->GetMannequinInterface();
	const SMannequinItemParams *pParams = mannequinSys.GetMannequinUserParamsManager().FindOrCreateParams<SMannequinItemParams>(pController);

	ICharacterInstance *pCharInst = GetEntity()->GetCharacter(nCharacterSlot);
	if (pParams->contextIDs.Weapon != TAG_ID_INVALID)
	{
		if (pCharInst)
		{
			const IAnimationDatabase* pDatabase = !m_sharedparams->params.adbFile.empty() ? mannequinSys.GetAnimationDatabaseManager().Load(m_sharedparams->params.adbFile.c_str()) : NULL;
			pController->SetScopeContext(pParams->contextIDs.Weapon, *GetEntity(), pCharInst, pDatabase);
	
			const int numAccessories = m_accessories.size();

			for (int i = 0; i < numAccessories && !(foundTop && foundBottom); i++)
			{
				const CItem::SAccessoryInfo &accessoryInfo = m_accessories[i];
				const SAccessoryParams *aparams = GetAccessoryParams(accessoryInfo.pClass);

				if (!foundTop && (aparams->category == g_pItemStrings->scope) && (pParams->contextIDs.attachment_top != TAG_ID_INVALID)) 
				{
					foundTop = true;
					if(CItem* pAccessory = static_cast<CItem*>(m_pItemSystem->GetItem(accessoryInfo.accessoryId)))
					{
						const IAnimationDatabase* pAccessoryDatabase = !pAccessory->GetParams().adbFile.empty() ? mannequinSys.GetAnimationDatabaseManager().Load(pAccessory->GetParams().adbFile.c_str()) : NULL;
						pController->SetScopeContext(pParams->contextIDs.attachment_top, *pAccessory->GetEntity(), pAccessory->GetEntity()->GetCharacter(0), pAccessoryDatabase);
					}
				}

				if (!foundBottom && (aparams->category == g_pItemStrings->bottom) && (pParams->contextIDs.attachment_bottom != TAG_ID_INVALID))
				{
					foundBottom = true;
					if(CItem* pAccessory = static_cast<CItem*>(m_pItemSystem->GetItem(accessoryInfo.accessoryId)))
					{
						const IAnimationDatabase* pAccessoryDatabase = !pAccessory->GetParams().adbFile.empty() ? mannequinSys.GetAnimationDatabaseManager().Load(pAccessory->GetParams().adbFile.c_str()) : NULL;
						pController->SetScopeContext(pParams->contextIDs.attachment_bottom, *pAccessory->GetEntity(), pAccessory->GetEntity()->GetCharacter(0), pAccessoryDatabase);
					}
				}
			}
		}
		else 
		{
			pController->ClearScopeContext(pParams->contextIDs.Weapon);
		}
	}

	if(!foundTop && (pParams->contextIDs.attachment_top != TAG_ID_INVALID))
	{
		pController->ClearScopeContext(pParams->contextIDs.attachment_top);
	}

	if(!foundBottom && (pParams->contextIDs.attachment_bottom != TAG_ID_INVALID))
	{
		pController->ClearScopeContext(pParams->contextIDs.attachment_bottom);
	}
}

//------------------------------------------------------------------------
void CItem::ClearScopeContexts(IActionController *pController)
{
	CMannequinUserParamsManager& mannequinUserParams = g_pGame->GetIGameFramework()->GetMannequinInterface().GetMannequinUserParamsManager();
	const SMannequinItemParams *pParams = mannequinUserParams.FindOrCreateParams<SMannequinItemParams>(pController);

	if (pParams->contextIDs.Weapon != TAG_ID_INVALID)
	{
		pController->ClearScopeContext(pParams->contextIDs.Weapon);
	}
	if (pParams->contextIDs.attachment_top != TAG_ID_INVALID)
	{
		pController->ClearScopeContext(pParams->contextIDs.attachment_top);
	}
	if (pParams->contextIDs.attachment_bottom != TAG_ID_INVALID)
	{
		pController->ClearScopeContext(pParams->contextIDs.attachment_bottom);
	}
}

//------------------------------------------------------------------------
void CItem::UpdateAccessoryTags(const SMannequinItemParams* pParams, CTagState &tagState, bool selected) const
{
	if(pParams)
	{
		const SMannequinItemParams::TagGroupIDs& itemTagGroupIDs = pParams->tagGroupIDs;
		if (itemTagGroupIDs.scope_attachment != TAG_ID_INVALID)
		{
			tagState.ClearGroup(itemTagGroupIDs.scope_attachment);
		}

		if (itemTagGroupIDs.barrel_attachment != TAG_ID_INVALID)
		{
			tagState.ClearGroup(itemTagGroupIDs.barrel_attachment);
		}

		if (itemTagGroupIDs.underbarrel_attachment != TAG_ID_INVALID)
		{
			tagState.ClearGroup(itemTagGroupIDs.underbarrel_attachment);
		}
	}

	if(selected)
	{
		int numAccessories = m_accessories.size();
		for(int i = 0; i < numAccessories; i++)
		{
			if(CItem* pAccessory = static_cast<CItem*>(m_pItemSystem->GetItem(m_accessories[i].accessoryId)))
			{
				if(uint32 accessoryTypeCrC = CCrc32::ComputeLowercase(pAccessory->GetParams().tag.c_str()))
				{
					tagState.SetByCRC(accessoryTypeCrC, true);
				}
			}
		}
	}
}

//-------------------------------------------------------------------------
void CItem::UpdateMountedTags(const SMannequinItemParams* pParams, CTagState& tagState, bool selected) const
{
	if (pParams)
	{
		if(pParams->tagGroupIDs.mountedWeapons.IsValid())
		{
			TagID tagID = TAG_ID_INVALID;

			if(selected)
			{
				if(IsMounted())
				{
					tagID = pParams->tagIDs.weaponMounted;
				}
				else if(IsRippedOff())
				{
					tagID = pParams->tagIDs.weaponDetached;
				}
			}

			tagState.SetGroup(pParams->tagGroupIDs.mountedWeapons, tagID);
		}
	}
}

//------------------------------------------------------------------------
void CItem::EnableSound(bool enable)
{
	m_stats.sound_enabled = enable;
}

//------------------------------------------------------------------------
bool CItem::IsSoundEnabled() const
{
	return m_stats.sound_enabled;
}

//------------------------------------------------------------------------
void CItem::MountAt(const Vec3 &pos)
{
	if (!m_sharedparams->params.mountable)
		return;

	m_stats.mounted = true;

	SetViewMode(eIVM_ThirdPerson);
	
	Matrix34 tm(GetEntity()->GetWorldTM());
	tm.SetTranslation(pos);
	GetEntity()->SetWorldTM(tm);

	m_stats.mount_dir = GetEntity()->GetWorldTM().TransformVector(FORWARD_DIRECTION);
}

//------------------------------------------------------------------------
void CItem::MountAtEntity(EntityId entityId, const Vec3 &pos, const Ang3 &angles)
{
	if (!m_sharedparams->params.mountable)
		return;

	IEntity *pHost = m_pEntitySystem->GetEntity(entityId);
	if (!pHost)
		return;

	m_hostId = entityId;
	m_stats.mounted = true;

	SetViewMode(eIVM_ThirdPerson);

	pHost->AttachChild(GetEntity(), 0);

	Matrix34 tm = Matrix33(Quat::CreateRotationXYZ(angles));
	tm.SetTranslation(pos);
	GetEntity()->SetLocalTM(tm);

	m_stats.mount_dir = GetEntity()->GetWorldTM().TransformVector(FORWARD_DIRECTION);
}

//------------------------------------------------------------------------
const Matrix34 CItem::GetWorldTM() const
{
	if (m_stats.attachment == eIA_WeaponCharacter)
	{
		IAttachmentManager *pAttachmentManager = GetOwnerAttachmentManager();
		if (pAttachmentManager)
		{
			IAttachment *pAttachment = pAttachmentManager->GetInterfaceByName(m_sharedparams->params.attachment[m_stats.hand].c_str());
			if (pAttachment)
			{
				return Matrix34(pAttachment->GetAttWorldAbsolute());
			}
		}

		CRY_ASSERT_MESSAGE(0, "Weapon thinks it is attached but isn't");
		return GetEntity()->GetWorldTM();
	}
	else
	{
		return GetEntity()->GetWorldTM();
	}
}

const Vec3 CItem::GetWorldPos() const
{
	if (m_stats.attachment == eIA_WeaponCharacter)
	{
		IAttachmentManager *pAttachmentManager = GetOwnerAttachmentManager();
		if (pAttachmentManager)
		{
			IAttachment *pAttachment = pAttachmentManager->GetInterfaceByName(m_sharedparams->params.attachment[m_stats.hand].c_str());
			if (pAttachment)
			{
				return pAttachment->GetAttWorldAbsolute().t;
			}
		}

		CRY_ASSERT_MESSAGE(0, "Weapon thinks it is attached but isn't");
		return GetEntity()->GetWorldPos();
	}
	else
	{
		return GetEntity()->GetWorldPos();
	}

}

void CItem::GetRelativeLocation(QuatT& location) const
{
	if (m_stats.attachment == eIA_WeaponCharacter)
	{
		IAttachmentManager *pAttachmentManager = GetOwnerAttachmentManager();
		if (pAttachmentManager)
		{
			IAttachment *pAttachment = pAttachmentManager->GetInterfaceByName(m_sharedparams->params.attachment[m_stats.hand].c_str());
			if (pAttachment)
			{
				location = pAttachment->GetAttModelRelative();
				return;
			}
		}

		CRY_ASSERT_MESSAGE(0, "Weapon thinks it is attached but isn't");
	}

	const bool bRelativeToParent = false;
	const int slotIndex = 0; 
	const Matrix34& localTM = GetEntity()->GetSlotLocalTM(slotIndex,bRelativeToParent);

	location.t = localTM.GetTranslation();
	location.q = Quat(localTM);
}

//------------------------------------------------------------------------
IEntity *CItem::GetOwner() const
{
	if (!m_owner.GetId())
		return 0;

	return m_pEntitySystem->GetEntity(m_owner.GetId());
}

//------------------------------------------------------------------------
CActor *CItem::GetOwnerActor() const
{
	return m_owner.GetActorWeakPtr().lock().get();
}

//------------------------------------------------------------------------
CPlayer *CItem::GetOwnerPlayer() const
{
	CActor* pOwnerActor = GetOwnerActor();
	if (pOwnerActor && (pOwnerActor->GetActorClass() == CPlayer::GetActorClassType()))
	{
		return static_cast<CPlayer*>(pOwnerActor);
	}

	return NULL;
}


//------------------------------------------------------------------------
CActor *CItem::GetActor(EntityId actorId) const
{
	if(!m_pGameFramework)
		return NULL;

	IActor* pActor = m_pGameFramework->GetIActorSystem()->GetActor(actorId);
	if (!pActor)
		return NULL;

	return static_cast<CActor*>(pActor);
}

//------------------------------------------------------------------------
IInventory *CItem::GetActorInventory(IActor *pActor) const
{
	if (!pActor)
		return 0;

	return pActor->GetInventory();
}

//------------------------------------------------------------------------
CItem *CItem::GetActorItem(IActor *pActor) const
{
	if (!pActor)
		return 0;

	IInventory *pInventory = pActor->GetInventory();
	if (!pInventory)
		return 0;

	EntityId id = pInventory->GetCurrentItem();
	if (!id)
		return 0;

	return static_cast<CItem *>(m_pItemSystem->GetItem(id));
}

//------------------------------------------------------------------------
CActor *CItem::GetActorByNetChannel(INetChannel *pNetChannel) const
{
	return static_cast<CActor *>(m_pGameFramework->GetIActorSystem()->GetActorByChannelId(m_pGameFramework->GetGameChannelId(pNetChannel)));
}

//------------------------------------------------------------------------
const char *CItem::GetDisplayName() const
{
	return g_pGame->GetWeaponSystem()->GetItemPackages().GetFullItemName(this);
}
//------------------------------------------------------------------------
bool CItem::HasFastSelect(EntityId nextItemId)const
{
	CItem* pNextItem = static_cast<CItem*>(m_pItemSystem->GetItem(nextItemId));

	if (pNextItem && pNextItem->GetParams().fast_select)
		return true;
	if (IsRippedOff() || IsMounted())
		return true;

	return false;
}

bool CItem::ShouldPlaySelectAction() const
{
	// GetOwnerActor() is NULL in KillCam, where we never want to do ItemSelectActions.
	return (!AreAnyItemFlagsSet(eIF_Unholstering) ||  AreAnyItemFlagsSet(eIF_UnholsteringPlaySelect)) && GetOwnerActor();
}

//------------------------------------------------------------------------
void CItem::OnAction(EntityId actorId, const ActionId& actionId, int activationMode, float value)
{
}

//------------------------------------------------------------------------
void CItem::StartUse(EntityId userId)
{
	if (!m_sharedparams->params.usable || m_owner.GetId())
		return;

	// holster user item here
	SetOwnerId(userId);

	CActor* pOwner=GetOwnerActor();
	if (!pOwner)
		return;

	EntityId entityId = GetEntityId();
	
	m_pItemSystem->SetActorItem(pOwner, entityId, true);

	m_stats.used = true;

	ApplyViewLimit(userId, true);

	EnableUpdate(true, eIUS_General);
	RequireUpdate(eIUS_General);

	pOwner->LockInteractor(entityId, true);

	pOwner->LinkToMountedWeapon(entityId);

	if (IsServer() && AreAnyItemFlagsSet(eIF_InformClientsAboutUse))
		pOwner->GetGameObject()->InvokeRMI(CActor::ClStartUse(), CActor::ItemIdParam(entityId), eRMI_ToAllClients|eRMI_NoLocalCalls);

	OnStartUsing();
}

//------------------------------------------------------------------------
void CItem::StopUse(EntityId userId)
{
	if (userId != m_owner.GetId())
		return;

	CActor *pActor = GetOwnerActor();
	if (!pActor)
		return;

	//absolutely make sure we deselect the weapon as 'SelectLastItem' can fail
	m_pItemSystem->SetActorItem(pActor, EntityId(0), false);
	if (!pActor->IsDead())
	{
		pActor->SelectLastItem(true);
	}

	ApplyViewLimit(userId, false);
		
	EnableUpdate(false);

	m_stats.used = false;

	SetOwnerId(0);

	EntityId entityId = GetEntityId();

	pActor->LockInteractor(entityId, false);
	pActor->LinkToMountedWeapon(0);

	if (IsServer())
		pActor->GetGameObject()->InvokeRMI(CActor::ClStopUse(), CActor::ItemIdParam(entityId), eRMI_ToAllClients|eRMI_NoLocalCalls);
}

//------------------------------------------------------------------------
void CItem::ApplyViewLimit(EntityId userId, bool apply)
{
	CActor *pActor = GetActor(userId);
	if (!pActor)
		return;

	SActorParams &params = pActor->GetActorParams();

	if (apply)
	{
		params.viewLimits.SetViewLimit(	m_stats.mount_dir, 
																		DEG2RAD(m_properties.mounted_yaw_range),
																		0.f,
																		DEG2RAD(m_properties.mounted_max_pitch),
																		DEG2RAD(m_properties.mounted_min_pitch),
																		SViewLimitParams::eVLS_Item);

		pActor->SetSpeedMultipler(SActorParams::eSMR_Item, 0.0f);
	}
	else
	{
		params.viewLimits.ClearViewLimit(SViewLimitParams::eVLS_Item);

		pActor->SetSpeedMultipler(SActorParams::eSMR_Item, 1.0f);
	}
}

//------------------------------------------------------------------------
bool CItem::AttachToHand(bool attach, bool checkAttachment)
{
	//Skip mounted
	if (m_stats.mounted)
		return false;

	IAttachmentManager* pAttachmentManager = GetOwnerAttachmentManager();
	if (!pAttachmentManager)
		return false;

	const CActor* pOwner = GetOwnerActor();

	IAttachment* pAttachment = 0;
	if (pOwner && !pOwner->IsPlayer())
		pAttachment = pAttachmentManager->GetInterfaceByName(m_sharedparams->params.aiAttachment[m_stats.hand].c_str());
	if (!pAttachment)
		pAttachment = pAttachmentManager->GetInterfaceByName(m_sharedparams->params.attachment[m_stats.hand].c_str());
	if (!pAttachment)
	{
		GameWarning("Item owner '%s' doesn't have third-person item attachment point '%s'!", GetOwner()->GetName(), m_sharedparams->params.attachment[m_stats.hand].c_str());
		return false;
	}

	if (!attach)
	{
		switch (m_stats.attachment)
		{
		case eIA_WeaponCharacter:
			//--- Ensure that the weapon entity is in the right place, then deliberate drop through to weaponEntity
			GetEntity()->SetWorldTM(GetWorldTM());

		case eIA_WeaponEntity:
			pAttachment->ClearBinding();
			m_stats.attachment = eIA_None;
			break;
		}
	}
	else
	{
		CRY_ASSERT_MESSAGE(!IsAttachedToBack(), "Logic flow error, should never seek to attach to the hand whilst attached to the back!");

		bool firstPerson = (m_stats.viewmode & eIVM_FirstPerson);
		int slot = firstPerson ? eIGS_FirstPerson : eIGS_ThirdPerson;
		SEntitySlotInfo slotInfo;
		bool validSlot = GetEntity()->GetSlotInfo(slot, slotInfo);

		IAttachmentObject* pNewAttachment = NULL;
		
		if (validSlot && firstPerson)
		{
			if (m_stats.attachment != eIA_WeaponCharacter)
			{
				if(slotInfo.pCharacter)
				{
					CSKELAttachment *pChrAttachment = new CSKELAttachment();
					pChrAttachment->m_pCharInstance = slotInfo.pCharacter;
	
					pNewAttachment = pChrAttachment;

					m_stats.attachment = eIA_WeaponCharacter;
				}
				else if(slotInfo.pStatObj)
				{
					CCGFAttachment* pCGFAttachment = new CCGFAttachment();
					pCGFAttachment->pObj = slotInfo.pStatObj;
					pCGFAttachment->m_pReplacementMaterial = slotInfo.pMaterial;

					pNewAttachment = pCGFAttachment;

					m_stats.attachment = eIA_WeaponCharacter;
				}
			}
		}
		else
		{
			if (m_stats.attachment != eIA_WeaponEntity)
			{
				CEntityAttachment *pEntityAttachment = new CEntityAttachment();
				pEntityAttachment->SetEntityId(GetEntityId());

				pNewAttachment = pEntityAttachment;

				m_stats.attachment = eIA_WeaponEntity;
			}
		}

		if(pNewAttachment)
		{
			pAttachment->AddBinding(pNewAttachment);
			pAttachment->HideAttachment(0);
		}

		DrawSlot(slot, (m_stats.attachment != eIA_WeaponCharacter), (m_stats.viewmode & eIVM_FirstPerson));
	}

	AttachToShadowHand(attach && ((m_stats.viewmode&eIVM_FirstPerson) != 0));

	return true;
}

CItem::eItemAttachment CItem::ChooseAttachmentPoint(bool attach, IAttachment **attachmentPt) const
{
	eItemAttachment ret = eIA_None;
	IAttachment *attachRet = NULL;
	IAttachmentManager *pAttachmentManager = GetOwnerAttachmentManager();

	if (pAttachmentManager)
	{
		if (attach)
		{
			attachRet = pAttachmentManager->GetInterfaceByName(m_sharedparams->params.bone_attachment_01.c_str());
			ret = eIA_StowPrimary;
			if (!attachRet)
			{
				GameWarning("Item owner '%s' doesn't have third-person item attachment point '%s'!", GetOwner()->GetName(), m_sharedparams->params.bone_attachment_01.c_str());
			}
			if(attachRet && attachRet->GetIAttachmentObject())
			{
				attachRet = pAttachmentManager->GetInterfaceByName(m_sharedparams->params.bone_attachment_02.c_str());
				if (!attachRet)
				{
					GameWarning("Item owner '%s' doesn't have third-person item attachment point '%s'!", GetOwner()->GetName(), m_sharedparams->params.bone_attachment_02.c_str());
					ret = eIA_None;
				}
				else if (attachRet->GetIAttachmentObject())
				{
					attachRet = NULL;
					ret = eIA_None;
				}
				else
				{
					ret = eIA_StowSecondary;
				}
			}
		}
		else
		{
			ret = m_stats.attachment;
			if(m_stats.attachment == eIA_StowPrimary)
			{
				attachRet = pAttachmentManager->GetInterfaceByName(m_sharedparams->params.bone_attachment_01.c_str());
			}
			else if(m_stats.attachment == eIA_StowSecondary)
			{
				attachRet = pAttachmentManager->GetInterfaceByName(m_sharedparams->params.bone_attachment_02.c_str());
			}
		}
	}

	if (attachmentPt != NULL)
	{
		*attachmentPt = attachRet;
	}

	return ret;
}

//------------------------------------------------------------------------
bool CItem::AttachToBack(bool attach)
{
	if (m_sharedparams && m_sharedparams->params.attach_to_back)
	{
		if(IsAttachedToBack() == attach)
			return false;

		if(CActor* pActor = GetOwnerActor())
		{
			//--- Don't attach if we're in fp mode or on drop
			if (attach && (!pActor->IsThirdPerson() || m_stats.dropped))
				return false;

			if(SActorStats* pStats = pActor->GetActorStats())
			{
				if(pStats->isGrabbed && attach)
					return false;
			}

			IAttachment *pAttachment = NULL;
			eItemAttachment attachmentId = ChooseAttachmentPoint(attach, &pAttachment);

			CloakSync(false);

			m_stats.attachment = eIA_None;

			if (pAttachment)
			{
				if (!attach)
				{
					pAttachment->ClearBinding();
				}
				else
				{
					CRY_ASSERT_MESSAGE(!IsAttachedToHand(), "Logic flow error, should never seek to attach to the back whilst attached to the hand!");

					m_stats.attachment = attachmentId;
					if(GetOwnerActor() && GetOwnerActor()->IsPlayer())
					{
						SetViewMode(0);
						Hide(true);

						if (ICharacterInstance *pTPChar = GetEntity()->GetCharacter(eIGS_ThirdPerson))
						{
							CSKELAttachment *pChrAttachment = new CSKELAttachment();
							pChrAttachment->m_pCharInstance = pTPChar;
							pAttachment->AddBinding(pChrAttachment);
							pAttachment->HideAttachment(0);
							pAttachment->HideInShadow(0);
						}
						else if (IStatObj *pStatObj = GetEntity()->GetStatObj(eIGS_ThirdPerson))
						{
							CCGFAttachment *pCGFAttachment = new CCGFAttachment();
							pCGFAttachment->pObj = pStatObj;

							pAttachment->AddBinding(pCGFAttachment);
							pAttachment->HideAttachment(0);
							pAttachment->HideInShadow(0);
						}
					}
					else
					{
						SetViewMode(eIVM_ThirdPerson);

						CEntityAttachment *pEntityAttachment = new CEntityAttachment();
						pEntityAttachment->SetEntityId(GetEntityId());

						pAttachment->AddBinding(pEntityAttachment);
						pAttachment->HideAttachment(0);

						//After QS/QL, hide if owner is hidden
						if(pActor->GetEntity()->IsHidden())
							Hide(true);
						else 
							Hide(false);
					}
				}

				return true;
			}
		}
	}
	
	return false;
}

//------------------------------------------------------------------------
void CItem::RequireUpdate(int slot)
{
	if (slot==-1)
		for (int i=0;i<4;i++)
			GetGameObject()->ForceUpdateExtension(this, i);	
	else
		GetGameObject()->ForceUpdateExtension(this, slot);
}

//------------------------------------------------------------------------
void CItem::EnableUpdate(bool enable, int slot)
{
	if (enable)
	{
		if (slot==-1)
			for (int i=0;i<4;i++)
				GetGameObject()->EnableUpdateSlot(this, i);
		else
			GetGameObject()->EnableUpdateSlot(this, slot);

	}
	else
	{
		if (slot==-1)
		{
			for (int i=0;i<4;i++)
				GetGameObject()->DisableUpdateSlot(this, i);
		}
		else
			GetGameObject()->DisableUpdateSlot(this, slot);
	}
}

//------------------------------------------------------------------------
void CItem::Hide(bool hide, bool remoteUpdate)
{
	IEntity* pEntity = GetEntity();
	CActor* pOwner = GetOwnerActor();

	if (pOwner && pOwner->GetEntity()->IsHidden())
		hide = true;

	if ((hide && m_stats.fp) || IsServer() || remoteUpdate)
	{
		pEntity->SetFlags(pEntity->GetFlags()|ENTITY_FLAG_UPDATE_HIDDEN);
	}
	else
	{
		pEntity->SetFlags(pEntity->GetFlags()&~ENTITY_FLAG_UPDATE_HIDDEN);
	}

	pEntity->Hide(hide);
}

//------------------------------------------------------------------------
void CItem::HideItem(bool hide)
{
	IAttachmentManager *pAttachmentManager = GetOwnerAttachmentManager();

	if (pAttachmentManager)
	{
		IAttachment *pAttachment = pAttachmentManager->GetInterfaceByName(m_sharedparams->params.attachment[m_stats.hand].c_str());
		if (pAttachment)
		{
			pAttachment->HideAttachment(hide ? 1 : 0);
		}
	}

	Hide(hide, true);
}

void CItem::CloakEnable(bool enable, bool fade, float cloakBlendSpeedScale, bool bFadeByDistance, uint8 colorChannel, bool bIgnoreCloakRefractionColor, EntityId syncedFromId /*= 0*/ )
{
	EntityEffects::Cloak::CloakEntity(GetEntityId(), enable, fade, cloakBlendSpeedScale, bFadeByDistance, colorChannel, bIgnoreCloakRefractionColor);

	CRecordingSystem* pRecordingSystem = syncedFromId ? g_pGame->GetRecordingSystem() : NULL;
	if(pRecordingSystem)
	{
		pRecordingSystem->OnObjectCloakSync( GetEntityId(), syncedFromId, enable, fade);
	}

	const int numAccessories = m_accessories.size();

	for (int i = 0; i < numAccessories; i++)
	{
		EntityEffects::Cloak::CloakEntity(m_accessories[i].accessoryId, enable, fade, cloakBlendSpeedScale, bFadeByDistance, colorChannel, bIgnoreCloakRefractionColor);

		if(pRecordingSystem)
		{
			pRecordingSystem->OnObjectCloakSync(m_accessories[i].accessoryId, syncedFromId, enable, fade);
		}
	}
	
	SetItemFlag(eIF_Cloaked, enable);
}

void CItem::CloakSync(bool fade)
{
	// check if the owner is cloaked
	IEntity* pOwner = GetOwner();

	if(!pOwner)
		return;

	IEntityRender* pOwnerRP = pOwner->GetRenderInterface();
	if (pOwnerRP)
	{
		//uint8 ownerMask = pOwnerRP->GetMaterialLayersMask();
		//bool isCloaked = (ownerMask&MTL_LAYER_CLOAK) != 0;
		//float cloakBlendSpeedScale = pOwnerRP->GetCloakBlendTimeScale();
		//bool bFadeByDistance = pOwnerRP->DoesCloakFadeByDistance();
		//uint8 colorChannel = pOwnerRP->GetCloakColorChannel();
		//bool bIgnoreCloakRefractionColor = pOwnerRP->DoesIgnoreCloakRefractionColor();
		//CloakEnable(isCloaked, fade, cloakBlendSpeedScale, bFadeByDistance, colorChannel, bIgnoreCloakRefractionColor, pOwner->GetId());
	}
}

void CItem::AddAccessoryAmmoToInventory(IEntityClass* pAmmoType, int count, IInventory* pOwnerInventory)
{
	if(pOwnerInventory)
	{
		int capacity = pOwnerInventory->GetAmmoCapacity(pAmmoType);
		int current = pOwnerInventory->GetAmmoCount(pAmmoType);
		
		if((!gEnv->IsEditor()) && ((current+count) > capacity))
		{
			//If still there's some place, full inventory to maximum...
			if(current<capacity)
			{
				pOwnerInventory->SetAmmoCount(pAmmoType,capacity);
			}
		}
		else
		{
			int newCount = current+count;
			pOwnerInventory->SetAmmoCount(pAmmoType, newCount);
		}
	}
}

void CItem::ProcessAccessoryAmmo(IInventory* pOwnerInventory, IWeapon* pParentWeapon)
{
	if (AreAnyItemFlagsSet(eIF_AccessoryAmmoAvailable) && !m_sharedparams->bonusAccessoryAmmo.empty())
	{
		for (TAccessoryAmmoMap::const_iterator it = m_sharedparams->bonusAccessoryAmmo.begin(); it != m_sharedparams->bonusAccessoryAmmo.end(); ++it)
		{
			IEntityClass* pAmmoClass = it->first;
			int count = it->second;

			if(pParentWeapon)
			{
				TAccessoryAmmoMap::const_iterator capacity = m_sharedparams->accessoryAmmoCapacity.find(pAmmoClass);

				if(capacity != m_sharedparams->accessoryAmmoCapacity.end() && count > capacity->second)
				{
					//any amount above the capacity is intended to straight into the weapon
					int parentCount = pParentWeapon->GetAmmoCount(pAmmoClass);

					int numToAdd = count - capacity->second;
					count = capacity->second;

					pParentWeapon->SetAmmoCount(pAmmoClass, parentCount + numToAdd);
				}
			}

			AddAccessoryAmmoToInventory(pAmmoClass, count, pOwnerInventory);
		}

		ClearItemFlags(eIF_AccessoryAmmoAvailable);
	}
}

void CItem::ProcessAccessoryAmmoCapacities(IInventory* pOwnerInventory, bool addCapacity)
{
	if (pOwnerInventory)
	{
		TAccessoryAmmoMap::const_iterator iter;
		for (iter = m_sharedparams->accessoryAmmoCapacity.begin(); iter != m_sharedparams->accessoryAmmoCapacity.end(); ++iter)
		{
			int currentCapacity = pOwnerInventory->GetAmmoCapacity(iter->first);
			int additionalCapacity = iter->second;
			int newCapacity = 0;

			if(!addCapacity)
			{
				additionalCapacity = -additionalCapacity;
			}

			newCapacity = currentCapacity + additionalCapacity;

			CRY_ASSERT_MESSAGE(newCapacity >= 0, string().Format("Trying to set ammo capacity for '%s' to a negative number", iter->first->GetName()));

			pOwnerInventory->SetAmmoCapacity(iter->first, newCapacity);
		}
	}
}

bool CItem::CheckAmmoRestrictions(IInventory * pInventory)
{
	if(pInventory->GetCountOfClass(GetEntity()->GetClass()->GetName()) == 0)
		return true;

	if(AreAnyItemFlagsSet(eIF_AccessoryAmmoAvailable) && !m_sharedparams->bonusAccessoryAmmo.empty())
	{
		for (TAccessoryAmmoMap::const_iterator it = m_sharedparams->bonusAccessoryAmmo.begin(); it != m_sharedparams->bonusAccessoryAmmo.end(); ++it)
		{
			int invAmmo  = pInventory->GetAmmoCount(it->first);
			int invLimit = pInventory->GetAmmoCapacity(it->first);

			if(invAmmo>=invLimit)
				return false;
		}
	}

	return true;
}

void CItem::GetMemoryUsage(ICrySizer *s) const
{	
	s->AddObject(this, sizeof(*this));
	GetInternalMemoryUsage(s);
}

void CItem::GetInternalMemoryUsage(ICrySizer *s) const
{
	m_properties.GetMemoryStatistics(s);
	m_scheduler.GetMemoryStatistics(s);
}


SItemStrings* g_pItemStrings = 0;

SItemStrings::~SItemStrings()
{
	g_pItemStrings = 0;
}

SItemStrings::SItemStrings()
	: nw("nw")
	, left_item_attachment("left_weapon")
	, right_item_attachment("weapon")
	, bottom("bottom")
	, barrel("barrel")
	, scope("scope")
	, ammo("ammo")
{
	g_pItemStrings = this;
}

void CItem::DeselectItem( EntityId itemId )
{
	IItem *pItem = m_pItemSystem->GetItem(itemId);
	if (pItem)
	{
		if (pItem->GetOwnerId())
		{
			CActor *pActor = static_cast<CActor*>(g_pGame->GetIGameFramework()->GetIActorSystem()->GetActor(pItem->GetOwnerId()));
			if (pActor)
			{
				if (pActor->GetInventory()->GetCurrentItem() == pItem->GetEntityId())
				{
					int category = GetItemCategoryType(m_pItemSystem->GetItemCategory(pItem->GetEntity()->GetClass()->GetName()));
					pActor->SelectNextItem(1, false, category);
				}
			}
		}
	}
}
 
const SMountParams* CItem::GetMountedParams() const
{
	return m_sharedparams->pMountParams;
}

void CItem::PickUpAccessories(CActor *pPickerActor, IInventory* pInventory)
{
	bool isServer = IsServer();

	if (isServer && pPickerActor->IsPlayer())
	{
		const int numAccessories = m_accessories.size();

		// go through through accessories map and give a copy of each to the player
		for (int i = 0; i < numAccessories; i++)
		{
			if(!pInventory->HasAccessory(m_accessories[i].pClass))
			{
				g_pGame->GetIGameFramework()->GetIItemSystem()->GiveItem(pPickerActor, m_accessories[i].pClass->GetName(), false, false, true);
			}
			else if(CItem* pAccessory = static_cast<CItem*>(m_pItemSystem->GetItem(m_accessories[i].accessoryId)))
			{
				pAccessory->OnPickedUp(pPickerActor->GetEntityId(), true);
			}
		}

		for(TInitialSetup::iterator it = m_initialSetup.begin(); it != m_initialSetup.end(); ++it)
		{
			if(!pInventory->HasAccessory(*it))
			{
				g_pGame->GetIGameFramework()->GetIItemSystem()->GiveItem(pPickerActor, (*it)->GetName(), false, false, true);
			}
		}
	}
}

ColorF CItem::GetSilhouetteColor() const
{
	return CHUDUtils::GetHUDColor();
}

void CItem::GetFPOffset(QuatT &offset) const
{
	offset.t = m_sharedparams->params.fp_offset;
	offset.q = m_sharedparams->params.fp_rot_offset;
}

void CItem::RegisterAsUser()
{
	if (CActor* pOwnerActor = GetOwnerActor())
	{
		if(pOwnerActor->IsClient())
		{
			if(!AreAnyItemFlagsSet(eIF_RegisteredAs1pUser))
			{
				RegisterAs1pDbaUser();
				RegisterAs1pChrUser();
				RegisterAs1pAudioCacheUser();
				SetItemFlags(eIF_RegisteredAs1pUser);
			}
		}
		else
		{
			if(!AreAnyItemFlagsSet(eIF_RegisteredAs3pUser))
			{
				RegisterAs3pAudioCacheUser();
				SetItemFlags(eIF_RegisteredAs3pUser);
			}
		}
	}
}

void CItem::UnRegisterAsUser()
{
	if(AreAnyItemFlagsSet(eIF_RegisteredAs1pUser))
	{
		UnRegisterAs1pDbaUser();
		UnRegisterAs1pChrUser();
		UnRegisterAs1pAudioCacheUser();
		ClearItemFlags(eIF_RegisteredAs1pUser);
	}
	
	if(AreAnyItemFlagsSet(eIF_RegisteredAs3pUser))
	{
		UnRegisterAs3pAudioCacheUser();
		ClearItemFlags(eIF_RegisteredAs3pUser);
	}
}

void CItem::RegisterAs1pDbaUser()
{
	CRY_ASSERT(!AreAnyItemFlagsSet(eIF_RegisteredAs1pUser));

	if (!m_sharedparams->animationPrecache.empty())
	{
		CItemAnimationDBAManager& fpDbaManager = g_pGame->GetGameSharedParametersStorage()->GetItemResourceCache().Get1pDBAManager();
		
		fpDbaManager.AddDBAUser(m_sharedparams->animationGroup, m_sharedparams, GetEntityId());
	}
}

void CItem::UnRegisterAs1pDbaUser()
{
	CRY_ASSERT(AreAnyItemFlagsSet(eIF_RegisteredAs1pUser));

	CItemAnimationDBAManager& fpDbaManager = g_pGame->GetGameSharedParametersStorage()->GetItemResourceCache().Get1pDBAManager();

	fpDbaManager.RemoveDBAUser(m_sharedparams->animationGroup, GetEntityId());
}

void CItem::RegisterAs1pChrUser()
{
	CRY_ASSERT(!AreAnyItemFlagsSet(eIF_RegisteredAs1pUser));

	const SGeometryDef* pGD = m_sharedparams->GetGeometryForSlot(eIGS_FirstPerson);
	if (pGD)
	{
		gEnv->pCharacterManager->StreamKeepCharacterResourcesResident(pGD->modelPath.c_str(), 0, true);
	}
}

void CItem::UnRegisterAs1pChrUser()
{
	CRY_ASSERT(AreAnyItemFlagsSet(eIF_RegisteredAs1pUser));

	const SGeometryDef* pGD = m_sharedparams->GetGeometryForSlot(eIGS_FirstPerson);
	if (pGD)
	{
		gEnv->pCharacterManager->StreamKeepCharacterResourcesResident(pGD->modelPath.c_str(), 0, false);
	}
}


void CItem::RegisterAs1pAudioCacheUser()
{
	CRY_ASSERT(!AreAnyItemFlagsSet(eIF_RegisteredAs1pUser));

	AudioCacheItemAndAccessories(true, "_fp");
}

void CItem::UnRegisterAs1pAudioCacheUser()
{
	CRY_ASSERT(AreAnyItemFlagsSet(eIF_RegisteredAs1pUser));

	AudioCacheItemAndAccessories(false, "_fp");
}

void CItem::RegisterAs3pAudioCacheUser()
{
	CRY_ASSERT(!AreAnyItemFlagsSet(eIF_RegisteredAs3pUser));

	AudioCacheItemAndAccessories(true, "_3p");
}

void CItem::UnRegisterAs3pAudioCacheUser()
{
	CRY_ASSERT(AreAnyItemFlagsSet(eIF_RegisteredAs3pUser));

	AudioCacheItemAndAccessories(false, "_3p");
}

void CItem::AudioCacheItemAndAccessories(const bool enable, const char* type)
{
	MEMSTAT_CONTEXT(EMemStatContextTypes::MSC_Other, 0, "CItem::AudioCacheItem");
	IEntityClass* pClass = GetEntity()->GetClass();
	g_pGame->GetWeaponSystem()->GetWeaponAlias().UpdateClass(&pClass);
	AudioCacheItem(enable, pClass, "", type);

	const CItem::TAccessoryArray& accessoryArray = GetAccessories();
	const int accesssoryCount = accessoryArray.size();
	for(int i = 0; i < accesssoryCount; i++)
	{
		const IEntityClass* pAccClass = accessoryArray[i].pClass;
		AudioCacheItem(enable, pAccClass, "att_", type);
	}
}

/*static*/ void CItem::AudioCacheItem(const bool enable, const IEntityClass* pClass, const char* prefix, const char* postfix)
{
	/*CryFixedStringT<32> soundCache;
	soundCache.Format("%s%s%s", prefix, pClass->GetName(), postfix);

	if(enable)
	{
		gEnv->pSoundSystem->CacheAudioFile(soundCache.c_str(), eAFCT_GAME_HINT);
	}
	else
	{
		gEnv->pSoundSystem->RemoveCachedAudioFile(soundCache.c_str(), false);
	}*/
}

bool CItem::IsAudioCached(const IEntityClass *pClass, const char* type)
{
	return true;
	/*CryFixedStringT<32> soundCache;
	IEntityClass *local_pClass = (IEntityClass*) pClass;

	g_pGame->GetWeaponSystem()->GetWeaponAlias().UpdateClass(&local_pClass);

	soundCache.Format("%s%s", local_pClass->GetName(), type);

	bool inmem = gEnv->pSoundSystem->EntryOrHintHasFlags(soundCache.c_str(), eAFCS_CACHED);

	return inmem;*/
}


IAttachmentManager *CItem::GetOwnerAttachmentManager() const
{
	IEntity *pOwner = GetOwner();
	if (pOwner)
	{
		ICharacterInstance *pOwnerCharacter = pOwner->GetCharacter(0);
		if (pOwnerCharacter)
		{
			return pOwnerCharacter->GetIAttachmentManager();
		}
	}

	return NULL;
}

void CItem::Prepare1pAnimationDbas()
{
	if (!m_sharedparams->animationPrecache.empty())
	{
		CItemAnimationDBAManager& fpDbaManager = g_pGame->GetGameSharedParametersStorage()->GetItemResourceCache().Get1pDBAManager();

		fpDbaManager.RequestDBAPreLoad(m_sharedparams->animationGroup, m_sharedparams);
	}
}

void CItem::Prepare1pChrs()
{
	const SGeometryDef* pGD = m_sharedparams->GetGeometryForSlot(eIGS_FirstPerson);
	if (pGD)
	{
		CItemPrefetchCHRManager& pf = g_pGame->GetGameSharedParametersStorage()->GetItemResourceCache().GetPrefetchCHRManager();

		pf.Prefetch(pGD->modelPath);
	}
}


void CItem::ScheduleExchangeToNextItem(CActor* pActor, CItem* pNextItem, CItem* pDeselectItem)
{
	SetItemFlags(eIF_ExchangeItemScheduled);
	uint32 deselectTime = 0;
	if (pDeselectItem)
	{
		pDeselectItem->PlayAction(GetFragmentIds().deselect);
		deselectTime = pDeselectItem->GetCurrentAnimationTime(eIGS_Owner);
	}
	GetScheduler()->TimerAction(deselectTime, CSchedulerAction<ExchangeToNextItem>::Create(ExchangeToNextItem(pActor, pNextItem)), false);
}



bool CItem::IsIdentical(CItem* pOtherItem) const
{
	return GetDisplayName() == pOtherItem->GetDisplayName();
}

bool CItem::ShouldDoPostSerializeReset() const
{
	return !m_stats.mounted;
}

bool CItem::CanFireUnderWater() const
{
	return m_sharedparams->params.usable_under_water;
}

void CItem::SetCurrentActionController( IActionController* pActionController )
{
	if(m_pCurrentActionController!=pActionController)
	{
		m_pCurrentActionController = pActionController;
		if (m_pCurrentActionController)
		{
			CMannequinUserParamsManager& mannequinUserParams = g_pGame->GetIGameFramework()->GetMannequinInterface().GetMannequinUserParamsManager();
			m_pCurrentManItemParams = mannequinUserParams.FindOrCreateParams<SMannequinItemParams>(m_pCurrentActionController);
		}
		else
		{
			m_pCurrentManItemParams = &sNullManItemParams;
		}
	}
}

void CItem::UpdateCurrentActionController()
{
	IActionController* pActionController = m_pItemActionController;
	if(!pActionController)
	{
		const EntityId ownerId = GetOwnerId();
		if(CActor* pActor = static_cast<CActor*>(g_pGame->GetIGameFramework()->GetIActorSystem()->GetActor(ownerId)))
		{
			if(IAnimatedCharacter *pAnimChar = pActor->GetAnimatedCharacter())
			{
				pActionController = pAnimChar->GetActionController();
			}
		}
		else if(gEnv->bMultiplayer)
		{
			if(CRecordingSystem* pRecSys = g_pGame->GetRecordingSystem())
			{
				pActionController = pRecSys->GetActionController(ownerId);
			}
		}
	}
	SetCurrentActionController(pActionController);
}

void CItem::SetIgnoreHeat( bool ignoreHeat )
{
	if(ignoreHeat)
	{
		m_itemFlags |= eIF_IgnoreHeat;
	}
	else
	{
		m_itemFlags &= ~eIF_IgnoreHeat;
	}

	IEntityRender* pItemRenderProxy = (GetEntity()->GetRenderInterface());
	if(pItemRenderProxy)
	{
		//pItemRenderProxy->SetIgnoreHeatAmount(ignoreHeat);
	}

	const int numAccessories = m_accessories.size();
	IEntityRender* pItemAccessoryRenderProxy(NULL);
	for(int i = 0; i < numAccessories; ++i)
	{
		IEntity* pAccessory = gEnv->pEntitySystem->GetEntity(m_accessories[i].accessoryId);
		if(pAccessory && (pItemAccessoryRenderProxy = (pAccessory->GetRenderInterface())))
		{
			//pItemAccessoryRenderProxy->SetIgnoreHeatAmount(ignoreHeat);
		}
	}
}

const string CItem::GetAttachedAccessoriesString(const char* separator)
{
	string accessories = "";
	
	TAccessoryArray::const_iterator it;
	for (it = m_accessories.begin(); it != m_accessories.end(); ++it)
	{
		if (it != m_accessories.begin())
			accessories.append(separator);

		accessories.append((*it).pClass->GetName());	
	}

	return accessories;
}
