// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

// -------------------------------------------------------------------------
//  File name:   GameStateRecorder.cpp
//  Version:     v1.00
//  Created:     3/2008 by Luciano Morpurgo.
//  Compilers:   Visual Studio.NET
//  Description: Checks the player and other game specific objects' states and communicate them to the TestManager
//							 Implements IGameStateRecorder
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////

#include "StdAfx.h"
#include <CryGame/IGame.h>
#include <CryGame/IGameFramework.h>
#include "IActorSystem.h"
#include "GameStateRecorder.h"
#include "CryAction.h"
#include "Player.h"
#include "Item.h"
#include "Weapon.h"
#include "Accessory.h"
#include "Inventory.h"
#include "Game.h"
#include <CryAISystem/IAIObjectManager.h>

// avoid fake mistakes when health is raising progressively 
// there can be small mismatches due to time-dependent health regeneration
#define CHECK_MISMATCH(cur,rec,thr) (cur < rec-thr || cur > rec+thr)


///////////////////////////////////////////////////////////////////////
bool IsGrenade(const char* className)
{
	return className && (!strcmp(className,"OffhandGrenade") ||
				!strcmp(className,"OffhandFlashbang"));
}

///////////////////////////////////////////////////////////////////////
bool IsAmmoGrenade(const char* className)
{
	return className && (!strcmp(className,"explosivegrenade") ||
			!strcmp(className,"flashbang") ||
			!strcmp(className,"smokegrenade") ||
			!strcmp(className,"empgrenade"));
}

///////////////////////////////////////////////////////////////////////
CGameStateRecorder::CGameStateRecorder() 
{
	m_mode = GPM_Disabled;
	m_bRecording = false;
	m_bEnable = false;
	m_bLogWarning = true;
	m_currentFrame = 0;
	m_pSingleActor = NULL;
	//m_pRecordGameEventFtor = new RecordGameEventFtor(this);
	m_demo_actorInfo = REGISTER_STRING( "demo_actor_info","player",0,"name of actor which game state info is displayed" );
	m_demo_actorFilter = REGISTER_STRING( "demo_actor_filter","player",0,"name of actor which game state is recorded ('player','all',<entity name>" );
	REGISTER_CVAR2( "demo_force_game_state",&m_demo_forceGameState,2,0,"Forces game state values into game while playing timedemo: only health and suit energy (1) or all (2)" );

}

////////////////////////////////////////////////////////////////////////////
CGameStateRecorder::~CGameStateRecorder()
{
}

////////////////////////////////////////////////////////////////////////////
void CGameStateRecorder::Release()
{
//	if(m_pRecordGameEventFtor)
		//delete m_pRecordGameEventFtor;
	IConsole* pConsole = gEnv->pConsole;
	if(pConsole)
	{
		pConsole->UnregisterVariable( "demo_force_game_state", true );
		pConsole->UnregisterVariable( "demo_actor_info", true );
		pConsole->UnregisterVariable( "demo_actor_filter", true );
	}
	delete this;
}

////////////////////////////////////////////////////////////////////////////
void CGameStateRecorder::Enable(bool bEnable, bool bRecording)
{
	m_bRecording = bRecording;
	m_bEnable = bEnable;
	//m_mode = mode;
	if(!bEnable)
		m_mode = GPM_Disabled;

	if(m_bEnable && bRecording)
	{
		gEnv->pGameFramework->GetIGameplayRecorder()->RegisterListener(this);
	}
	else
	{
		gEnv->pGameFramework->GetIGameplayRecorder()->UnregisterListener(this);
	}
	
	if(m_bEnable)
		StartSession();
}

/////////////////////////////////////////////////////////////////////////////
void CGameStateRecorder::AddActorToStats(const CActor* pActor)
{
	if(m_bRecording)
		DumpWholeGameState(pActor);
	else
	{
		IEntity* pEntity = pActor->GetEntity();
		EntityId id;
		if(pEntity != NULL && (id=pEntity->GetId()))
		{
			m_GameStates.insert(std::make_pair(id,SActorGameState()));
		}
	}
}

////////////////////////////////////////////////////////////////////////////
void CGameStateRecorder::StartSession()
{
	m_GameStates.clear();
	m_itSingleActorGameState = m_GameStates.end();
	m_IgnoredEvents.clear();

	const char* filterName = m_demo_actorFilter->GetString();
	// send game events to record the initial game state
/*	if(m_mode)
	{
		CActor *pActor = static_cast<CActor *>(gEnv->pGameFramework->GetClientActor());
*/
		
	m_pSingleActor = GetActorOfName(filterName);
	if(m_pSingleActor)// && !pActor->GetSpectatorMode() && pActor->IsPlayer())
	{
		m_mode = GPM_SingleActor;
		AddActorToStats(m_pSingleActor);
		m_itSingleActorGameState = m_GameStates.begin();// position of requested actor's id (player by default)
	}
//	}
	else if (!strcmpi(filterName,"all") && gEnv->pAISystem)
	{
		IAIObjectManager* pAIObjMgr = gEnv->pAISystem->GetAIObjectManager();

		m_mode = GPM_AllActors;
		{
			AutoAIObjectIter it(pAIObjMgr->GetFirstAIObject(OBJFILTER_TYPE, AIOBJECT_ACTOR));
			for(; it->GetObject(); it->Next())
			{
				IAIObject* pObject = it->GetObject();
				if(pObject)
				{
					CActor* pActor = static_cast<CActor *>(gEnv->pGameFramework->GetIActorSystem()->GetActor(pObject->GetEntityID()));
					if(pActor)
						AddActorToStats(pActor);
				}
			}
		}

		{
			AutoAIObjectIter it(pAIObjMgr->GetFirstAIObject(OBJFILTER_TYPE, AIOBJECT_VEHICLE));
			for(; it->GetObject(); it->Next())
			{
				IAIObject* pObject = it->GetObject();
				if(pObject)
				{
					CActor* pActor = static_cast<CActor *>(gEnv->pGameFramework->GetIActorSystem()->GetActor(pObject->GetEntityID()));
					if(pActor)
						AddActorToStats(pActor);
				}
			}
		}
	}
}

////////////////////////////////////////////////////////////////////////////
void CGameStateRecorder::DumpWholeGameState(const CActor* pActor)
{
	GameplayEvent event;
	IEntity* pEntity = pActor->GetEntity();
	
	// health
	float health = pActor->GetHealth();
	event.event = eGE_HealthChanged;
	event.value = health;
	SendGamePlayEvent(pEntity,event);
	
	// Inventory
	CInventory *pInventory = (CInventory*)(pActor->GetInventory());
	if(!pInventory)
		return;

	int count = pInventory->GetCount();
	for(int i=0; i<count; ++i)
	{
		EntityId itemId = pInventory->GetItem(i);
		
		CItem* pItem=NULL;
		TItemName itemName = GetItemName(itemId,&pItem);
		if(pItem && itemName)	
		{
			event.event = eGE_ItemPickedUp;
			event.description = itemName;
			SendGamePlayEvent(pEntity,event);

			if(pActor->GetCurrentItem() == pItem)
			{
				event.event = eGE_ItemSelected;
				event.description = itemName;
				event.value = 1; // for initialization
				SendGamePlayEvent(pEntity,event);
			}
			
			CWeapon* pWeapon = (CWeapon*)(pItem->GetIWeapon());
			if(pWeapon)
			{
				IEntityClass* pItemClass = pWeapon->GetEntity()->GetClass();
				if(pItemClass != NULL && !strcmpi(pItemClass->GetName(),"binoculars"))
					continue; // no fire mode or ammo recorded for binocular (which is a weapon)

				// fire mode
				int fireModeIdx = pWeapon->GetCurrentFireMode();

				event.event = eGE_WeaponFireModeChanged;
				event.value = (float)fireModeIdx;
				event.description = itemName;
				SendGamePlayEvent(pEntity,event);
				// count ammo
				const TAmmoVector& weaponAmmosVector = pWeapon->GetAmmoVector();
				TAmmoVector::const_iterator ammoEndCit = weaponAmmosVector.end();
				for(TAmmoVector::const_iterator ammoCit = weaponAmmosVector.begin(); ammoCit != ammoEndCit; ++ammoCit)
				{
					const SWeaponAmmo& weaponAmmo = *ammoCit;
					const char* ammoClass = weaponAmmo.pAmmoClass ? weaponAmmo.pAmmoClass->GetName() : NULL;
					if(ammoClass)
					{
						event.event = eGE_AmmoCount;
						event.value = (float)weaponAmmo.count;
						event.extra = (void*)ammoClass;
						event.description = (const char*)itemName;
						SendGamePlayEvent(pEntity,event);
					}
				}
			}
		}
	}

	count = pInventory->GetAccessoryCount();

	for(int i=0; i< count; i++)
	{
		const IEntityClass* pAccessory = pInventory->GetAccessoryClass(i);
		if(pAccessory)
		{
			TItemName classItem = pAccessory->GetName();
			event.event = eGE_AccessoryPickedUp;
			//event.value = classIdx;
			event.description = classItem;
			SendGamePlayEvent(pEntity,event);
		}
	}

	for(pInventory->AmmoIteratorFirst(); !pInventory->AmmoIteratorEnd(); pInventory->AmmoIteratorNext())
	{
		const IEntityClass* pAmmoClass = pInventory->AmmoIteratorGetClass();
		if(pAmmoClass)
		{
			const char* ammoClassName = pAmmoClass->GetName();
			int ammoCount = pInventory->AmmoIteratorGetCount();
			event.event = eGE_AmmoPickedUp;
			event.description = ammoClassName;
			event.value = (float)ammoCount;
			SendGamePlayEvent(pEntity, event);
		}
	}

}

////////////////////////////////////////////////////////////////////////////
void CGameStateRecorder::Update()
{
	if(m_bRecording)
	{
		switch(m_mode)
		{
			case GPM_SingleActor:
				{
					//CActor *pActor = static_cast<CActor *>(gEnv->pGameFramework->GetClientActor());
					//if(pActor && !pActor->GetSpectatorMode() && pActor->IsPlayer())
					if(m_pSingleActor)
					{
						float health = m_pSingleActor->GetHealth();
						if(m_itSingleActorGameState != m_GameStates.end())
						{
							//SActorGameState& playerState = m_itSingleActorGameState->second;
							//if( health!= playerState.health)
							{
//								playerState.health = health;
								GameplayEvent event;
								event.event = eGE_HealthChanged;
								event.value = health;
								OnGameplayEvent(m_pSingleActor->GetEntity(),event);
							}							
						}
					}
				}
				break;

			case GPM_AllActors:
			default:
				break;
		}
	}
}


////////////////////////////////////////////////////////////////////////////
void CGameStateRecorder::OnRecordedGameplayEvent(IEntity *pEntity, const GameplayEvent &event, int currentFrame, bool bRecording)
{
	EntityId id;
	m_currentFrame = currentFrame;

	int demo_forceGameState = 0;
	if(!bRecording)
	{
		ICVar *pVar = gEnv->pConsole->GetCVar( "demo_force_game_state" );
		if(pVar)
			demo_forceGameState = pVar->GetIVal();
	}
	
	if(!pEntity || !(id = pEntity->GetId()))
		return;

	if(m_IgnoredEvents.size())
		if(event.event == m_IgnoredEvents[0])
		{
			m_IgnoredEvents.erase(m_IgnoredEvents.begin());
			return;
		}

		TGameStates::iterator itActor = m_GameStates.find(id);
	if(itActor == m_GameStates.end())
	{
		m_GameStates.insert(std::make_pair(id,SActorGameState()));
		itActor = m_GameStates.find(id);
	}
	if(itActor == m_GameStates.end())
	{
		GameWarning("TimeDemo:GameState: actor %s not found in records",pEntity->GetName());
		return;
	}

	SActorGameState& gstate = itActor->second;

	switch(event.event)
	{
		case eGE_HealthChanged: 
			{
				gstate.health = event.value;

				if(!m_bRecording)
				{
					CActor *pActor = (CActor*)(gEnv->pGameFramework->GetIActorSystem()->GetActor(id));
					if(pActor)
					{
						if(m_bLogWarning)
						{
							if(CHECK_MISMATCH(pActor->GetHealth(), gstate.health,10))
							{
								if(!gstate.bHealthDifferent)
								{
									GameWarning("TimeDemo:GameState: Frame %d - Actor %s - HEALTH mismatch (%8.2f, %8.2f)",m_currentFrame,pEntity->GetName(), pActor->GetHealth(),gstate.health);
									gstate.bHealthDifferent = true;
								}
							}
							else
								gstate.bHealthDifferent = false;
						}

						if( demo_forceGameState)
							pActor->SetHealth(gstate.health);
					}
				}
			}
			break;
		case eGE_WeaponFireModeChanged:
			{
				TItemName sel = (event.description);
				if(sel)
				{
					TItemContainer& Items = gstate.Items;
					TItemContainer::iterator iti = Items.find(sel);
					uint8 recFireModeIdx = uint8(event.value);
					if(iti != Items.end())
						iti->second.fireMode = recFireModeIdx;

					CActor *pActor = (CActor*)(gEnv->pGameFramework->GetIActorSystem()->GetActor(id));
					if(pActor && pActor->GetInventory())
					{
						IItem* pItem = pActor->GetInventory()->GetItemByName(sel);
						CWeapon* pWeapon;
						if(pItem && (pWeapon = (CWeapon*)(pItem->GetIWeapon())))
						{

						int fireModeIdx = pWeapon->GetCurrentFireMode();
						if(m_bLogWarning)
							{
									CheckDifference(recFireModeIdx,fireModeIdx,"TimeDemo:GameState: Frame %d - Actor %s - FIRE MODE mismatch for weapon %s (rec:%d, cur:%d)",pEntity,pItem->GetEntity() ? pItem->GetEntity()->GetName() : "(null)");
							}

							if(demo_forceGameState==2 && fireModeIdx!= recFireModeIdx)
								pWeapon->SetCurrentFireMode(recFireModeIdx);
						}
					}
				}
			}
			break;

		case eGE_WeaponReload:
			{
				const char* ammoType = event.description;
				TAmmoContainer& ammoMags = gstate.AmmoMags;
				TAmmoContainer::iterator it = ammoMags.find(ammoType);
				if(it!=ammoMags.end())
				{
					it->second -= (uint16)event.value;
				
					CActor *pActor = (CActor*)(gEnv->pGameFramework->GetIActorSystem()->GetActor(id));
					if(pActor)
					{
						CInventory* pInventory = (CInventory*)(pActor->GetInventory());
						if(pInventory)
						{
							{
								IEntityClass* pAmmoClass = gEnv->pEntitySystem->GetClassRegistry()->FindClass(ammoType);
								if(pAmmoClass)
								{
									if(m_bLogWarning)
										CheckDifference(it->second,pInventory->GetAmmoCount(pAmmoClass),"TimeDemo:GameState: Frame %d - Actor %s - WEAPON RELOAD, ammo count mismatch for ammo class %s (rec:%d, cur:%d)",pEntity,ammoType);
		
									if(demo_forceGameState == 2)
										pInventory->SetAmmoCount(pAmmoClass,it->second);
								}
							}
						}
					}
				}
			}
			break;

		case eGE_ItemSelected:
			{
				TItemName itemName = event.description;
				gstate.itemSelected = itemName;
				if(itemName)
				{
					if( !bRecording && (event.value > 0 || demo_forceGameState==2)) // EVENT.VALUE > 0 means initialization
					{
						CActor *pActor = (CActor*)(gEnv->pGameFramework->GetIActorSystem()->GetActor(id));
						if(pActor && pActor->GetInventory())
						{
							IItem* pItem = pActor->GetInventory()->GetItemByName(itemName);
							if(pItem)
								pActor->SelectItem(pItem->GetEntityId(),false, false);
						}
					}
				}

				if(m_bLogWarning)
				{	
					CActor *pActor = (CActor*)(gEnv->pGameFramework->GetIActorSystem()->GetActor(id));
					if(pActor)
					{
						IItem* pItem = pActor->GetCurrentItem();
						const char* curItemName= pItem && pItem->GetEntity() ? pItem->GetEntity()->GetName(): NULL;
						CheckDifferenceString(itemName,curItemName,"TimeDemo:GameState: Frame %d - Actor %s - SELECTED ITEM mismatch (rec:%s, cur:%s)",pEntity);
					}
				}
			}
			break;

		case eGE_ItemExchanged:
			{
				// prevent unwanted record/play mismatch error with current item during item exchanging
				// two unneeded game events are sent (selecting fists)
				m_IgnoredEvents.push_back(eGE_ItemSelected);
				m_IgnoredEvents.push_back(eGE_ItemSelected);
			}
			break;

		case eGE_ItemPickedUp:
			{
				TItemName sel = (TItemName)event.description;
//				gstate.itemSelected = sel;
				TItemContainer& Items = gstate.Items;
				TItemContainer::iterator it = Items.find(sel);
				if(it == Items.end())
				{
					Items.insert(std::make_pair(sel,SItemProperties()));
					it = Items.find(sel);
				}

				if(it != Items.end())
				{
					it->second.count++;

					CActor *pActor = (CActor*)(gEnv->pGameFramework->GetIActorSystem()->GetActor(id));
					if(pActor && !m_bRecording)
					{
						CInventory* pInventory = (CInventory*)(pActor->GetInventory());
						if(pInventory)
						{
							// just check if the item is the inventory
							if(m_bLogWarning && !pInventory->GetItemByName(sel))
								GameWarning("TimeDemo:GameState: Frame %d - Actor %s - Recorded PICKED UP ITEM class '%s' not found in current inventory",m_currentFrame,pEntity->GetName(),sel);

							if(demo_forceGameState == 2)
							{
								IEntity* pItemEntity = gEnv->pEntitySystem->FindEntityByName(sel);
								if(pItemEntity)
									pInventory->AddItem(pItemEntity->GetId());
								else
									GameWarning("TimeDemo:GameState: Frame %d - Actor %s - PICKED UP ITEM entity %s not found in level",m_currentFrame,pEntity->GetName(),sel);
							}
						}
					}
				}
				else if(m_bLogWarning)
				{
					if(!sel)
						sel = "(null)";
					GameWarning("TimeDemo:GameState: Frame %d - Actor %s - PICKED UP ITEM %s not found in recorded inventory",m_currentFrame,pEntity->GetName(),sel);
				}

			}
			break;

		case eGE_AmmoPickedUp:
			{
				uint16 ammoCount = (uint16)(event.value);
				TItemName sel = (TItemName)event.description;
				TAmmoContainer& Ammo = gstate.AmmoMags;

				TAmmoContainer::iterator it = Ammo.find(sel);
				if(it == Ammo.end())
					Ammo.insert(std::make_pair(sel,ammoCount));
				else
					it->second = ammoCount;

				if( !m_bRecording)
				{
					CActor *pActor = (CActor*)(gEnv->pGameFramework->GetIActorSystem()->GetActor(id));
					if(pActor)
					{
						CInventory* pInventory = (CInventory*)(pActor->GetInventory());
						if(pInventory)
						{
							IEntityClass* pClass = gEnv->pEntitySystem->GetClassRegistry()->FindClass(sel);
							if(m_bLogWarning)
								CheckDifference(ammoCount,pInventory->GetAmmoCount(pClass),"TimeDemo:GameState: Frame %d - Actor %s - AMMO PICKEDUP: count mismatch for ammo class %s (rec:%d, cur:%d)", pEntity,sel);

							if(demo_forceGameState == 2)
								pInventory->SetAmmoCount(pClass,ammoCount);
						}
					}
				}
			}
			break;

		case eGE_AccessoryPickedUp:
			{
				TItemName sel = (TItemName)event.description;
				//				gstate.itemSelected = sel;
				TAccessoryContainer& Accessories = gstate.Accessories;
				TAccessoryContainer::iterator it = Accessories.find(sel);
				if(it == Accessories.end())
				{
					Accessories.insert(std::make_pair(sel,1));
				}
				else
					it->second++;

				CActor *pActor = (CActor*)(gEnv->pGameFramework->GetIActorSystem()->GetActor(id));
				if(pActor)
				{
					CInventory* pInventory = (CInventory*)(pActor->GetInventory());
					if(pInventory)
					{
						IEntityClass* pClass = gEnv->pEntitySystem->GetClassRegistry()->FindClass(sel);
						
						if(m_bLogWarning && !pInventory->HasAccessory(pClass))
							GameWarning("TimeDemo:GameState: Frame %d - Actor %s - ACCESSORY PICKEDUP %s not found in current inventory", m_currentFrame, pEntity->GetName(),sel ? sel:"(null)");

						if(demo_forceGameState == 2 && pClass)					
							pInventory->AddAccessory(pClass);// doesn't actually add it if it's there already

					}
				}
			}
			break;

		case eGE_ItemDropped:
			{
				TItemName sel = (TItemName)event.description;
				//gstate.itemSelected = sel;
				TItemContainer& Items = gstate.Items;
				TItemContainer::iterator it = Items.find(sel);
				if(it != Items.end())
				{
					it->second.count--;
					if(it->second.count==0)
						Items.erase(it);

					if(demo_forceGameState == 2)
					{
						CActor *pActor = (CActor*)(gEnv->pGameFramework->GetIActorSystem()->GetActor(id));
						if(pActor)
						{
							CInventory* pInventory = (CInventory*)(pActor->GetInventory());
							if(pInventory)
							{
								IEntityClass* pClass = gEnv->pEntitySystem->GetClassRegistry()->FindClass(sel);
								if(pClass)
								{
									EntityId itemId = pInventory->GetItemByClass(pClass);
									if(itemId)
										pInventory->RemoveItem(itemId);
								}
							}
						}
					}
				}
				else
					GameWarning("TimeDemo:GameState: Frame %d - Actor %s - ITEM DROPPED, wrong item selected (%s)",m_currentFrame,pEntity->GetName(),sel);
			}
			break;

		case eGE_AmmoCount: 
			{
				TItemContainer& Items = gstate.Items;
				TItemName itemClassDesc = event.description;
				TItemContainer::iterator it = Items.find(itemClassDesc);
				if(it != Items.end())
				{
					SItemProperties& item = it->second;
					const char* ammoClassDesc = (const char*)(event.extra);
					if(ammoClassDesc)
					{
						string szAmmoClassDesc(ammoClassDesc);
						bool bAmmoMag = IsAmmoGrenade(ammoClassDesc);
						TAmmoContainer& itemAmmo = bAmmoMag ? gstate.AmmoMags : item.Ammo;

						if(itemAmmo.find(szAmmoClassDesc)==itemAmmo.end())
							itemAmmo.insert(std::make_pair(szAmmoClassDesc,int(event.value)));
						else
							itemAmmo[szAmmoClassDesc] = (uint16)event.value;

						if(!bRecording && (m_bLogWarning || demo_forceGameState==2))
						{
							CActor *pActor = (CActor*)(gEnv->pGameFramework->GetIActorSystem()->GetActor(id));
							if(pActor)
							{
								CInventory* pInventory = (CInventory*)(pActor->GetInventory());
								if(pInventory)
								{
									IItem* pItem = pInventory->GetItemByName(itemClassDesc);

									if(pItem)
									{
										CWeapon* pWeapon = (CWeapon*)(pItem->GetIWeapon());
										if(pWeapon)
										{
											IEntityClass* pAmmoClass = gEnv->pEntitySystem->GetClassRegistry()->FindClass(ammoClassDesc);
											if(pAmmoClass)
											{
												if(m_bLogWarning)
												{
													int curAmmoCount = (bAmmoMag ? pInventory->GetAmmoCount(pAmmoClass) : 
														pWeapon->GetAmmoCount(pAmmoClass));
													CheckDifference( int(event.value),curAmmoCount,"TimeDemo:GameState: Frame %d - Actor %s - AMMO COUNT mismatch for ammo class %s (rec:%d, cur:%d)",pEntity,ammoClassDesc);
												}

												if(demo_forceGameState==2)
													pWeapon->SetAmmoCount(pAmmoClass,int(event.value));

											}
										}
									}
								}
							}
						}
					}
					else
						GameWarning("TimeDemo:GameState: Frame %d - Actor %s - AMMO COUNT null ammoClass descriptor",m_currentFrame,pEntity->GetName());
				}
				else
					GameWarning("TimeDemo:GameState: Frame %d - Actor %s - AMMO COUNT wrong item selected (%s)",m_currentFrame,pEntity->GetName(),itemClassDesc);
			}
			break;

		case eGE_AttachedAccessory: 
			{
				if(!bRecording)
				{
					CActor *pActor = (CActor*)(gEnv->pGameFramework->GetIActorSystem()->GetActor(id));
					if(pActor)
					{
						CInventory* pInventory = (CInventory*)(pActor->GetInventory());
						if(pInventory)
						{
							const char* sel = event.description;
							IEntityClass* pClass = gEnv->pEntitySystem->GetClassRegistry()->FindClass(sel);
							if(!sel)
								sel = "(null)";
							if(!pInventory->HasAccessory(pClass))
							{
								if(m_bLogWarning)
									GameWarning("TimeDemo:GameState: Frame %d - Actor %s - ATTACHED ACCESSORY %s not found in current inventory", m_currentFrame,pEntity->GetName(),sel);
							}
							else
							{
								CItem* pCurrentItem = (CItem*)(pActor->GetCurrentItem());
								if(pCurrentItem)
									pCurrentItem->SwitchAccessory(sel);
							}
						}
					}
				}
			}
			break;

		case eGE_EntityGrabbed:
			{
		/*
						if(demo_forceGameState==2)
								{
									TItemName itemName = event.description;
									if(itemName)
									{
										IEntity * pGrabbedEntity = gEnv->pEntitySystem->FindEntityByName(itemName);
										if(pGrabbedEntity)
										{
											CActor *pActor = (CActor*)(gEnv->pGameFramework->GetIActorSystem()->GetActor(id));
											if(pActor)
											{
												IItem* pItem = GetItemOfName(pActor,itemName);
												if(!pItem)
												{
													// it's a pickable entity, won't end up in the inventory
													//(otherwise it would be managed by eGE_ItemPickedUp)
													COffHand* pOffHand = static_cast<COffHand*>(pActor->GetWeaponByClass(CItem::sOffHandClass));
													if(pOffHand && !pOffHand->GetPreHeldEntityId())
														pOffHand->ForcePickUp(pGrabbedEntity->GetId());
												}
											}
										}
									}	
								}*/
				
			}
			break;

		default:
			break;
	}
}

////////////////////////////////////////////////////////////////////////////
void CGameStateRecorder::OnGameplayEvent(IEntity *pEntity, const GameplayEvent &event)
{
	EntityId entityID;
	if (pEntity)
	{
		entityID = pEntity->GetId();
	}
	else
	{
		GameWarning("TimeDemo:GameState::OnGamePlayEvent: Entity not found");
		return;
	}

	CActor *pActor = static_cast<CActor*>(gEnv->pGameFramework->GetIActorSystem()->GetActor(entityID));
	if(!pActor)
	{
		GameWarning("TimeDemo:GameState::OnGamePlayEvent: Entity %s has no actor", pEntity->GetName());
		return;
	}

	GameplayEvent  event2 = event;
	event2.extra = 0;// event2 is the forwarded event, extra either will be not used by the listener or re-set as a string
	uint8 eType = event.event;

	bool bPlayer = (pActor->IsPlayer() && m_mode);
	if(bPlayer || m_mode==GPM_AllActors)
	{
		//items
		switch(eType)
		{
			case eGE_ItemPickedUp:
				{
					CheckInventory(pActor,0);//,*m_pRecordGameEventFtor);
				}
				break;

			case eGE_ItemDropped:
				{
					TItemName itemName = GetItemName(EntityId((TRUNCATE_PTR)event.extra));
					if(!itemName ) //if(itemIdx < 0)
						break;
					event2.description = itemName;
					SendGamePlayEvent(pEntity,event2);
					IEntity* pItemEntity = gEnv->pEntitySystem->FindEntityByName(itemName);
					if(!pItemEntity)
						break;
					IItem* pItem = g_pGame->GetIGameFramework()->GetIItemSystem()->GetItem(pItemEntity->GetId());
					if(!pItem)
						break;

					IEntityClass* pItemClass = pItem->GetEntity()->GetClass();
					if(pItemClass != NULL && !strcmpi(pItemClass->GetName(),"SOCOM"))
					{
						IItem* pCurrentItem = pActor->GetCurrentItem();
						if(pCurrentItem)
						{
							IEntityClass* pCurrentItemClass = pCurrentItem->GetEntity()->GetClass();
							if(pCurrentItemClass != NULL && !strcmpi(pCurrentItemClass->GetName(),"SOCOM"))
							{
								GameplayEvent event3;
								event3.event = eGE_ItemSelected;
								TItemName currItemName = GetItemName(pCurrentItem->GetEntity()->GetId());
								if(!currItemName)
									break;
								event3.value = 0;
								event3.description = (const char*)currItemName;
								SendGamePlayEvent(pEntity,event3);
							}
						}
					}
				}
				break;

			case eGE_WeaponFireModeChanged:
				{
					TItemName itemIdx = GetItemName(EntityId((TRUNCATE_PTR)event.extra));
					if(!itemIdx)//if(itemIdx < 0)
						break;
					event2.description = (const char*)itemIdx;
					SendGamePlayEvent(pEntity,event2);
				}
				break;

			case eGE_ItemSelected:
				{
					EntityId itemId = EntityId((TRUNCATE_PTR)event.extra);
					TItemName itemIdx = GetItemName(itemId);
					if(itemId && !itemIdx)
						break;
					event2.value = 0;
					event2.description = (const char*)itemIdx;
					SendGamePlayEvent(pEntity,event2);
				}
				break;

			case eGE_AttachedAccessory:
				{
					if(!IsGrenade(event.description)) // NOT OffHandGrenade
						SendGamePlayEvent(pEntity,event2);
				}
				break;

			case eGE_AmmoCount:
				{
					const char* itemIdx = event.description;
					if(!itemIdx)
						break;

					TGameStates::iterator itGS;
					/*if(pActor->IsPlayer())
						itGS = m_itSingleActorGameState;
					else */if(pActor->GetEntity())
						itGS = m_GameStates.find(pActor->GetEntity()->GetId());
					else
						break;

					if(itGS == m_GameStates.end())
						break;

					SActorGameState& gstate = itGS->second;

					IEntity* pItemEntity = gEnv->pEntitySystem->FindEntityByName(itemIdx);
					if(!pItemEntity)
						break;
					IItem* pItem = g_pGame->GetIGameFramework()->GetIItemSystem()->GetItem(pItemEntity->GetId());
					if(!pItem)
						break;

					CWeapon* pWeapon = (CWeapon*)(pItem->GetIWeapon());
					if(pWeapon)
					{
						TItemContainer::iterator it = gstate.Items.find(itemIdx);
						if(it==gstate.Items.end())
							break;
						SItemProperties& recItem = it->second;
						
						bool bGrenade = false;
						TAmmoVector::const_iterator ammoCit = pWeapon->GetAmmoVector().begin();
						TAmmoVector::const_iterator ammoEndCit = pWeapon->GetAmmoVector().end();
						if (ammoCit != ammoEndCit)
						{
							{
								SWeaponAmmo weaponAmmo = *ammoCit;
								if(!weaponAmmo.pAmmoClass)
								{
									// special case for grenades
									if(IsAmmoGrenade((const char*)(event.extra)))
									{
										weaponAmmo.pAmmoClass = gEnv->pEntitySystem->GetClassRegistry()->FindClass((const char*)event.extra);
										weaponAmmo.count = (int)event.value;
										bGrenade = true;
									}
								}
							}

							++ammoCit;

							for(; ammoCit != ammoEndCit; ++ammoCit)
							{
								const SWeaponAmmo& weaponAmmo = *ammoCit;
								const int ammoCount = weaponAmmo.count;

								const char* ammoClass = weaponAmmo.pAmmoClass ? weaponAmmo.pAmmoClass->GetName() : NULL;
								if(ammoClass)
								{
									TAmmoContainer& recAmmo = bGrenade? gstate.AmmoMags : recItem.Ammo;
									if(recAmmo.find(ammoClass) == recAmmo.end())
										recAmmo.insert(std::make_pair(ammoClass,0));
									if(ammoCount != recAmmo[ammoClass])
									{
										event2.event = eGE_AmmoCount;
										event2.value = (float)ammoCount;
										if(event2.value < 0)
											event2.value = 0;
										event2.extra = (void*)ammoClass;
										event2.description = (const char*)itemIdx;
										SendGamePlayEvent(pEntity,event2);
									}
								}
							}
						}
					}

				}
				break;

			case eGE_EntityGrabbed:
				{
					EntityId grabbedEntityId = EntityId((TRUNCATE_PTR)event.extra);
					IEntity * pGrabbedEntity = gEnv->pEntitySystem->GetEntity(grabbedEntityId);
					if(pGrabbedEntity)
					{
						event2.description = pGrabbedEntity->GetName();
						SendGamePlayEvent(pEntity,event2);
					}
				}
				break;

			case eGE_WeaponReload:
			case eGE_ZoomedIn:
			case eGE_ZoomedOut:
			case eGE_HealthChanged:
			case eGE_ItemExchanged:
				SendGamePlayEvent(pEntity,event2);
				break;

			default:
				break;
		}

	}
}

void CGameStateRecorder::SendGamePlayEvent(IEntity *pEntity, const GameplayEvent &event)
{
	for(TListeners::iterator it = m_listeners.begin(), itEnd = m_listeners.end(); it!=itEnd; ++it)
		(*it)->OnGameplayEvent(pEntity,event);

	OnRecordedGameplayEvent(pEntity,event,0,true); //updates actor game state during recording
}

////////////////////////////////////////////////////////////////////////////
void CGameStateRecorder::GetMemoryStatistics(ICrySizer * s)
{
	SIZER_SUBCOMPONENT_NAME(s, "GameStateRecorder");
	s->Add(*this);
	//s->AddObject(m_listeners);
	s->AddContainer(m_GameStates);
}

////////////////////////////////////////////////////////////////////////////
void CGameStateRecorder::RegisterListener(IGameplayListener* pL)
{
	if(pL)
		stl::push_back_unique(m_listeners,pL);
}

////////////////////////////////////////////////////////////////////////////
void CGameStateRecorder::UnRegisterListener(IGameplayListener* pL)
{
	stl::find_and_erase(m_listeners, pL);
}


//////////////////////////////////////////////////////////////////////////
float CGameStateRecorder::RenderInfo(float y, bool bRecording)
{
	float retY = 0;

	if (bRecording)
	{
		float fColor[4] = {1,0,0,1};
		IRenderAuxText::Draw2dLabel( 1,y+retY, 1.3f, fColor,false," Recording game state");
		retY +=15;
	}
	else 
	{
		const float xp = 300;
		const float xr = 400;
		const float xri = 600;

		float fColor[4] = {0,1,0,1};
		float fColorWarning[4] = {1,1,0,1};

		const char* actorName = m_demo_actorInfo->GetString();
		CActor *pActor = GetActorOfName(actorName);
		if(pActor)
		{
			IRenderAuxText::Draw2dLabel( 1,y+retY, 1.3f, fColor,false," Game state - Actor: %s --------------------------------------------------",pActor->GetEntity()? pActor->GetEntity()->GetName(): "(no entity)");
			retY +=15;

			if(m_itSingleActorGameState != m_GameStates.end() && pActor->GetEntity() && m_itSingleActorGameState->first == pActor->GetEntity()->GetId())
			{
				ICVar *pVar = gEnv->pConsole->GetCVar( "demo_force_game_state" );
				if(pVar)
				{
					int demo_forceGameState = pVar->GetIVal();
					if(demo_forceGameState)
					{
						IRenderAuxText::Draw2dLabel( 1,y+retY, 1.3f, fColor,false,demo_forceGameState==1 ?
							" Override mode = (health)" : " Override mode = (all)");
						retY +=15;
					}
				}

				IRenderAuxText::Draw2dLabel( xp,y+retY, 1.3f, fColor,false,"Current");
				IRenderAuxText::Draw2dLabel( xr,y+retY, 1.3f, fColor,false,"Recorded");
				retY +=15;

				SActorGameState& gstate = m_itSingleActorGameState->second;
				float recordedHealth = gstate.health;
				float health = pActor->GetHealth();
				bool bError = CHECK_MISMATCH(health, recordedHealth,10);

				IRenderAuxText::Draw2dLabel( 1,y+retY, 1.3f, fColor,false," Health:");
				IRenderAuxText::Draw2dLabel( xp,y+retY, 1.3f, bError? fColorWarning : fColor, false,"%d",(int)health);
				IRenderAuxText::Draw2dLabel( xr,y+retY, 1.3f, bError? fColorWarning : fColor, false,"%d",(int)recordedHealth);
				retY +=15;

				// items
				IRenderAuxText::Draw2dLabel( 1,y+retY, 1.3f, fColor,false," Inventory ---------------------------------------------------------------------------------------");
				retY +=15;
				IRenderAuxText::Draw2dLabel( xp,y+retY, 1.3f, fColor,false,"Current");
				IRenderAuxText::Draw2dLabel( xri,y+retY, 1.3f, fColor,false,"Recorded");
				retY +=15;

				CInventory *pInventory = (CInventory*)(pActor->GetInventory());
				if(pInventory)
				{
					int nInvItems = pInventory->GetCount();
						
					TItemContainer& Items = gstate.Items;

					int i=0;
					EntityId curSelectedId = pActor->GetCurrentItemId();
					TItemName curSelClass = GetItemName(curSelectedId);
					bool bSelectedError = !equal_strings(gstate.itemSelected,curSelClass);

					for(; i< nInvItems; i++)
					{
						IItem *pItem = g_pGame->GetIGameFramework()->GetIItemSystem()->GetItem(pInventory->GetItem(i));
						if(pItem)	
						{
							TItemName szItemName = GetItemName(pItem->GetEntityId());

							TItemContainer::iterator it = Items.find(szItemName);
							bError = it==Items.end();
							IRenderAuxText::Draw2dLabel( 1,y+retY, 1.3f, fColor,false," %2d)",i+1);

							EntityId curId = pItem->GetEntityId();
							TItemName curClass = GetItemName(curId);
							if(equal_strings(curClass,curSelClass) )
								IRenderAuxText::Draw2dLabel( xp-16,y+retY, 1.3f,bSelectedError ? fColorWarning:fColor, false, "[]");

							if(equal_strings(szItemName, gstate.itemSelected))
								IRenderAuxText::Draw2dLabel( xri-16,y+retY, 1.3f,bSelectedError ? fColorWarning:fColor, false, "[]");

							char itemName[32];
							const char* originalItemName = pItem->GetEntity() ? pItem->GetEntity()->GetName():"(null)";
							cry_strcpy(itemName,originalItemName);
							IRenderAuxText::Draw2dLabel( 1,y+retY, 1.3f, bError? fColorWarning : fColor, false,"     %s",itemName);

							if(bError)
								IRenderAuxText::Draw2dLabel( xp,y+retY, 1.3f, fColorWarning, false, "Missing");
							else
							{
								SItemProperties& recItem = it->second;
								CWeapon *pWeapon = (CWeapon*)(pItem->GetIWeapon());

								IEntityClass* pItemClass = pItem->GetEntity()->GetClass();
								if(pItemClass && !strcmpi(pItemClass->GetName(),"binoculars"))
									pWeapon = NULL; // no fire mode or ammo recorded for binocular (which is a weapon)

								if(pWeapon)
								{
									int idx = 0;
									// ammo
									float xa = 0;
									const TAmmoVector& weaponAmmosVector = pWeapon->GetAmmoVector();
									TAmmoVector::const_iterator ammoEndCit = weaponAmmosVector.end();
									for(TAmmoVector::const_iterator ammoCit = weaponAmmosVector.begin(); ammoCit != ammoEndCit; ++ammoCit)
									{
										const SWeaponAmmo& weaponAmmo = *ammoCit;
										int ammoCount = weaponAmmo.count;
										const char* ammoClass = weaponAmmo.pAmmoClass ? weaponAmmo.pAmmoClass->GetName() : NULL;
										if(ammoClass)
										{
											TAmmoContainer::iterator ammoCountIter = recItem.Ammo.find(ammoClass);
											if(ammoCountIter != recItem.Ammo.end())
											{
												int recAmmoCount = recItem.Ammo[ammoClass];
												bool bError2 = ammoCount!=recAmmoCount;
												IRenderAuxText::Draw2dLabel( xp+xa,y+retY, 1.3f, bError2? fColorWarning : fColor, false,"Am%d:%d",idx,ammoCount);
												IRenderAuxText::Draw2dLabel( xri+xa,y+retY, 1.3f, bError2? fColorWarning : fColor, false,"Am%d:%d",idx,recAmmoCount);
												xa += 50;
												++idx;
												if(idx%5 ==0)
												{
													xa=0;
													retY+=15;
												}
											}
										}
									}

									// current fire mode
									int curFireModeIdx = pWeapon->GetCurrentFireMode();
									int recFireModeIdx = recItem.fireMode;
									bool bError3 = curFireModeIdx!= recFireModeIdx;
									IRenderAuxText::Draw2dLabel( xp+xa,y+retY, 1.3f, bError3? fColorWarning : fColor, false,"FMode:%d",curFireModeIdx);
									IRenderAuxText::Draw2dLabel( xri+xa,y+retY, 1.3f, bError3? fColorWarning : fColor, false,"FMode:%d",recFireModeIdx);
								}
								else
								{
									IRenderAuxText::Draw2dLabel( xp,y+retY, 1.3f, fColor, false, "Ok");
								}
							}

						}
						retY +=15;
					}

					/// Accessories

					int nInvAccessories = pInventory->GetAccessoryCount();

					TAccessoryContainer& Accessories = gstate.Accessories;
					int nRecAccessories = Accessories.size();
					if(nRecAccessories)
					{
						IRenderAuxText::Draw2dLabel( 1,y+retY, 1.3f, bError? fColorWarning : fColor, false," Accessories");
						retY +=15;
					}

					for(int j=0 ; j< nInvAccessories; j++,i++)
					{
						const IEntityClass* pClass = pInventory->GetAccessoryClass(j);
						if(pClass)
						{
							TItemName szItemName = pClass->GetName();
							TAccessoryContainer::iterator it = Accessories.find(szItemName);
							bError = it==Accessories.end();
							IRenderAuxText::Draw2dLabel( 1,y+retY, 1.3f, fColor,false," %2d)",i+1);

							char itemName[32];
							cry_strcpy(itemName,szItemName);
							IRenderAuxText::Draw2dLabel( 1,y+retY, 1.3f, bError? fColorWarning : fColor, false,"     %s",itemName);

							if(bError)
								IRenderAuxText::Draw2dLabel( xp,y+retY, 1.3f, fColorWarning, false, "Missing");
							else
								IRenderAuxText::Draw2dLabel( xp,y+retY, 1.3f, fColor, false, "Ok");

							retY +=15;
						}
					}
					/// Ammo Mags
					int nInvAmmo = pInventory->GetAmmoPackCount();
					if(nInvAmmo)
					{
						IRenderAuxText::Draw2dLabel( 1,y+retY, 1.3f, bError? fColorWarning : fColor, false," Ammo Packs");
						retY +=15;
					}

					
					for(pInventory->AmmoIteratorFirst(); !pInventory->AmmoIteratorEnd(); pInventory->AmmoIteratorNext())
					{
						TAmmoContainer& Mags = gstate.AmmoMags;
						const IEntityClass* pAmmoClass = pInventory->AmmoIteratorGetClass();
						if(pAmmoClass)
						{
							int invAmmoCount = pInventory->AmmoIteratorGetCount();
							const char* ammoClassName = pAmmoClass->GetName();
							bool bNotFound = Mags.find(ammoClassName) == Mags.end();
							int recAmmoCount = bNotFound ? 0 :Mags[ammoClassName];
							bool bError2 =  bNotFound || invAmmoCount!= recAmmoCount;

							IRenderAuxText::Draw2dLabel( 1,y+retY, 1.3f, bError2? fColorWarning : fColor, false,"  %s:",ammoClassName);
							IRenderAuxText::Draw2dLabel( xp,y+retY, 1.3f, bError2? fColorWarning : fColor, false,"%d",invAmmoCount);
							if(bNotFound)
								IRenderAuxText::Draw2dLabel( xr,y+retY, 1.3f, fColorWarning, false,"NotRecd");
							else
								IRenderAuxText::Draw2dLabel( xr,y+retY, 1.3f, bError2? fColorWarning : fColor, false,"%d",recAmmoCount);
							retY +=15;

						}
					}
				}
			}
			else // m_itSingleActorGameState != m_GameStates.end()
			{
				IRenderAuxText::Draw2dLabel( 1,y+retY, 1.3f, fColor,false, "<<Not Recorded>>");
				retY +=15;
			}
		}
		else // pActor
		{
			IRenderAuxText::Draw2dLabel( 1,y+retY, 1.3f, fColor,false, "<<Actor %s not in the map>>",actorName ? actorName:"(no name)");
			retY +=15;		
		}

	}
	return retY;
}

///////////////////////////////////////////////////////////////////////
TItemName CGameStateRecorder::GetItemName(EntityId id, CItem** pItemOut)
{
	if(id)
	{
		CItem* pItem = (CItem*)(gEnv->pGameFramework->GetIItemSystem()->GetItem(id));
		if(pItem != NULL && pItem->GetEntity())
		{
			if(pItemOut)
				*pItemOut = pItem;
			return pItem->GetEntity()->GetName();
		}
	}
	return 0;
}

///////////////////////////////////////////////////////////////////////
CItem* CGameStateRecorder::GetItemOfName(CActor* pActor, TItemName itemName)
{
	CInventory* pInventory= (CInventory*)(pActor->GetInventory());
	if(pInventory != NULL && itemName)
		return (CItem*)(pInventory->GetItemByName(itemName));

	return 0;
}


///////////////////////////////////////////////////////////////////////
/*template <class EventHandlerFunc> */void CGameStateRecorder::CheckInventory(CActor* pActor, IItem *pItem)//, EventHandlerFunc eventHandler)
{

	if(pActor)
	{
		TGameStates::iterator itGS;
		if(pActor->IsPlayer())
			itGS = m_itSingleActorGameState;
		else if(pActor->GetEntity())
			itGS = m_GameStates.find(pActor->GetEntity()->GetId());
		else
			return;

		if(itGS != m_GameStates.end())
		{
			SActorGameState& gstate = itGS->second;

			// items

			CInventory *pInventory = (CInventory*)(pActor->GetInventory());
			if(pInventory)
			{
				bool bSingleItem = (pItem!=0);
				int nInvItems = (bSingleItem ? 1 : pInventory->GetCount());
				TItemContainer& Items = gstate.Items;
				
				for(int i=0; i< nInvItems; i++)
				{
					if(!bSingleItem) 
						pItem = g_pGame->GetIGameFramework()->GetIItemSystem()->GetItem(pInventory->GetItem(i));

					if(pItem)
					{
						TItemName szItemName = GetItemName(pItem->GetEntityId());

						TItemContainer::iterator it = Items.find(szItemName);
						if(it==Items.end())
						{
							it = (Items.insert(std::make_pair(szItemName,SItemProperties()))).first;

							GameplayEvent pickupEvent;
							pickupEvent.event = eGE_ItemPickedUp;
							pickupEvent.description = szItemName;
							//eventHandler(pActor,event);
							SendGamePlayEvent(pActor->GetEntity(), pickupEvent);

							SItemProperties& recItem = it->second;
							CWeapon *pWeapon = (CWeapon*)(pItem->GetIWeapon());
							if(pWeapon)
							{
								// ammo
								const TAmmoVector& weaponAmmosVector = pWeapon->GetAmmoVector();
								TAmmoVector::const_iterator ammoEndCit = weaponAmmosVector.end();
								for(TAmmoVector::const_iterator ammoCit = weaponAmmosVector.begin(); ammoCit != ammoEndCit; ++ammoCit)
								{
									const SWeaponAmmo& weaponAmmo = *ammoCit;
									const int ammoCount = weaponAmmo.count;
									const char* ammoClass = weaponAmmo.pAmmoClass ? weaponAmmo.pAmmoClass->GetName() : NULL;
									if(ammoClass)
									{
										TAmmoContainer& recAmmo = recItem.Ammo;

										if(recAmmo.find(ammoClass) == recAmmo.end())
											recAmmo.insert(std::make_pair(ammoClass,0));
										int recAmmoCount = recAmmo[ammoClass];
										if(ammoCount!=recAmmoCount)
										{
											GameplayEvent ammoCountEvent;
											ammoCountEvent.event = eGE_AmmoCount;
											ammoCountEvent.value = (float)ammoCount;
											ammoCountEvent.extra = (void*)(ammoClass);
											ammoCountEvent.description = (const char*)szItemName;
											//eventHandler(pActor,event);
											SendGamePlayEvent(pActor->GetEntity(), ammoCountEvent);

										}
									}
								}
								// current fire mode
								int curFireModeIdx = pWeapon->GetCurrentFireMode();
								int recFireModeIdx = recItem.fireMode;
								if(curFireModeIdx!= recFireModeIdx)
								{
									GameplayEvent fireModeEvent;
									fireModeEvent.event = eGE_WeaponFireModeChanged;
									fireModeEvent.value = (float)curFireModeIdx;
									fireModeEvent.description = (const char*)szItemName;
									//eventHandler(pActor,event);
									SendGamePlayEvent(pActor->GetEntity(),fireModeEvent);
								}
							}
						}
					}
				}

				/// Accessories

				int nInvAccessories = pInventory->GetAccessoryCount();

				TAccessoryContainer& Accessories = gstate.Accessories;

				for(int j=0 ; j< nInvAccessories; j++)
				{
					const IEntityClass* pClass = pInventory->GetAccessoryClass(j);
					if(pClass)
					{
						TItemName itemClass = pClass->GetName();
						TAccessoryContainer::iterator it = Accessories.find(itemClass);
						if(it==Accessories.end())
						{
							GameplayEvent event;
							event.event = eGE_AccessoryPickedUp;
							//event.value = accIdx;
							event.description = itemClass;
//						eventHandler(pActor,event);
							SendGamePlayEvent(pActor->GetEntity(),event);
						}
					}
				}

				/// Ammo
				for(pInventory->AmmoIteratorFirst(); !pInventory->AmmoIteratorEnd(); pInventory->AmmoIteratorNext())
				{
					TAmmoContainer& Mags = gstate.AmmoMags;
					const IEntityClass* pAmmoClass = pInventory->AmmoIteratorGetClass();
					if(pAmmoClass)
					{
						const char* ammoClassName = pAmmoClass->GetName();
						int ammoCount = pInventory->AmmoIteratorGetCount();
						if(Mags.find(ammoClassName) == Mags.end() || ammoCount!= Mags[ammoClassName])
						{
							GameplayEvent event;
							event.event = eGE_AmmoPickedUp;
							event.description = ammoClassName;
							event.value = (float)ammoCount;
//							eventHandler(pActor,event);
							SendGamePlayEvent(pActor->GetEntity(),event);
						}
					}
				}
			}
		}
	}
}

///////////////////////////////////////////////////////////////////////////////////////
CActor* CGameStateRecorder::GetActorOfName( const char* name)
{
	if(!strcmpi(name,"all"))
		return NULL;

	if(!strcmpi(name,"player"))
		return static_cast<CActor *>(gEnv->pGameFramework->GetClientActor());
	else
	{
		IEntity* pEntity = gEnv->pEntitySystem->FindEntityByName(name);
		if(pEntity)
			return static_cast<CActor *>(gEnv->pGameFramework->GetIActorSystem()->GetActor(pEntity->GetId()));
	}
	return NULL;
}

