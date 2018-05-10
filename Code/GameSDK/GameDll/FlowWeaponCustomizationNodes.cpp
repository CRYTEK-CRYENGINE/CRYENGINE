// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "GameObjects/GameObject.h"
#include "Item.h"
#include "Weapon.h"
#include "FlowWeaponCustomizationNodes.h"
#include "GameRules.h"
#include "EquipmentLoadout.h"
#include <CrySystem/Scaleform/IFlashUI.h>

//--------------------------------------------------------------------------------------------
CFlashUIInventoryNode::~CFlashUIInventoryNode()
{
}

void CFlashUIInventoryNode::GetConfiguration( SFlowNodeConfig &config )
{
	static const SInputPortConfig in_config[] = {
		InputPortConfig_Void( "Call", "Calls the function" ),
		{0}
	};

	static const SOutputPortConfig out_config[] = {
		OutputPortConfig_Void( "OnCall", "Triggered if this node starts the action" ),
		OutputPortConfig<string>( "Args", UIARGS_DEFAULT_DELIMITER_NAME " separated argument string" ),
		{0}
	};


	config.nFlags |= EFLN_TARGET_ENTITY;
	config.pInputPorts = in_config;
	config.pOutputPorts = out_config;
	config.sDescription = "Outputs the players inventory as a " UIARGS_DEFAULT_DELIMITER_NAME " separated string for UI processing";
	config.SetCategory(EFLN_APPROVED);
}


void CFlashUIInventoryNode::ProcessEvent( EFlowEvent event, SActivationInfo *pActInfo )
{
	if(event == eFE_Activate && IsPortActive(pActInfo, 0))
	{
		IActor* pActor = GetInputActor( pActInfo );
		if(pActor)
		{
			IInventory* pInventory = pActor->GetInventory();
			if(pInventory)
			{
				SUIArguments args;
				int inv_cap = gEnv->pConsole->GetCVar("i_inventory_capacity")->GetIVal();
				for (int i = 0; i < inv_cap; i++)
				{
					const char* weaponName = pInventory->GetItemString(i);

					if(strcmp(weaponName, "") != 0)
					{
						//Get the weapon and check if it is a selectable item
						IEntityClassRegistry *pRegistry = gEnv->pEntitySystem->GetClassRegistry();
						EntityId item = pInventory->GetItemByClass(pRegistry->FindClass(weaponName));
						IEntity* pEntity = gEnv->pEntitySystem->GetEntity(item);

						if(pEntity)
						{
							CGameObject * pGameObject = (CGameObject*)pEntity->GetProxy(ENTITY_PROXY_USER);
							CItem* pItem = (CItem*)pGameObject->QueryExtension(pGameObject->GetEntity()->GetClass()->GetName());

							if(pItem && pItem->CanSelect())
							{
								args.AddArgument(weaponName);
							}
						}
					}
				}

				ActivateOutput(pActInfo, eO_OnCall, true);
				ActivateOutput(pActInfo, eO_Args, string(args.GetAsString()));
			}
		}
	}
}


CFlashUIInventoryNode::CFlashUIInventoryNode( SActivationInfo * pActInfo )
{

}

//--------------------------------------------------------------------------------------------
CFlashUIGetEquippedAccessoriesNode::~CFlashUIGetEquippedAccessoriesNode()
{
}

void CFlashUIGetEquippedAccessoriesNode::GetConfiguration( SFlowNodeConfig &config )
{
	static const SInputPortConfig in_config[] = {
		InputPortConfig_Void( "Call", "Calls the function" ),
		InputPortConfig<string>( "Weapon", "Weapon to get the equipment for" ),
		{0}
	};

	static const SOutputPortConfig out_config[] = {
		OutputPortConfig_Void( "OnCall", "Triggered if this node starts the action" ),
		OutputPortConfig<string>( "Args", UIARGS_DEFAULT_DELIMITER_NAME " separated argument string" ),
		{0}
	};


	config.nFlags |= EFLN_TARGET_ENTITY;
	config.pInputPorts = in_config;
	config.pOutputPorts = out_config;
	config.sDescription = "Outputs all equipped accessories in a " UIARGS_DEFAULT_DELIMITER_NAME " separated string";
	config.SetCategory(EFLN_APPROVED);
}

void CFlashUIGetEquippedAccessoriesNode::ProcessEvent( EFlowEvent event, SActivationInfo *pActInfo )
{
	if(event == eFE_Activate && IsPortActive(pActInfo, 0))
	{
		string accessories = "";
		IActor* pActor = GetInputActor( pActInfo );

		if(pActor)
		{
			IInventory* pInventory = pActor->GetInventory();
			if(pInventory)
			{

				//Get the item ID via the Input string
				const string weaponName = GetPortString(pActInfo, eI_Weapon);
				IEntityClassRegistry *pRegistery = gEnv->pEntitySystem->GetClassRegistry();
				EntityId item = pInventory->GetItemByClass(pRegistery->FindClass(weaponName));
				IEntity* pEntity = gEnv->pEntitySystem->GetEntity(item);

				if (pEntity)
				{
					CGameObject * pGameObject = (CGameObject*)pEntity->GetProxy(ENTITY_PROXY_USER);
					IItem* pWeapon = (IItem*)pGameObject->QueryExtension(pGameObject->GetEntity()->GetClass()->GetName());

					//If the weapon exists, return all equipped attachments in a separated string
					if(pWeapon)
					{
						//All equipped accessories for this weapon weapons
						accessories = static_cast<CItem*>(pWeapon)->GetAttachedAccessoriesString(UIARGS_DEFAULT_DELIMITER);
					}
				}
			}
		}

		//return, if 'accesories' is empty, it has no attachments, or something was invalid
		ActivateOutput(pActInfo, eO_OnCall, true);
		ActivateOutput(pActInfo, eO_Args, accessories);
	}
}

//--------------------------------------------------------------------------------------------
CFlashUIGetCompatibleAccessoriesNode ::~CFlashUIGetCompatibleAccessoriesNode ()
{
}

void CFlashUIGetCompatibleAccessoriesNode ::GetConfiguration( SFlowNodeConfig &config )
{
	static const SInputPortConfig in_config[] = {
		InputPortConfig_Void( "Call", "Calls the function" ),
		InputPortConfig<string>( "Weapon", "Weapon to get the equipment for" ),
		{0}
	};

	static const SOutputPortConfig out_config[] = {
		OutputPortConfig_Void( "OnCall", "Triggered if this node starts the action" ),
		OutputPortConfig<string>( "Args", UIARGS_DEFAULT_DELIMITER_NAME " separated argument string" ),
		{0}
	};


	config.nFlags |= EFLN_TARGET_ENTITY;
	config.pInputPorts = in_config;
	config.pOutputPorts = out_config;
	config.sDescription = "Outputs the all compatible accessories for a weapon in a " UIARGS_DEFAULT_DELIMITER_NAME " separated string";
	config.SetCategory(EFLN_APPROVED);
}

void CFlashUIGetCompatibleAccessoriesNode ::ProcessEvent( EFlowEvent event, SActivationInfo *pActInfo )
{
	if(event == eFE_Activate && IsPortActive(pActInfo, 0))
	{
		IActor* pActor = GetInputActor( pActInfo );

		if(pActor)
		{
			IInventory* pInventory = pActor->GetInventory();
			if(pInventory)
			{
				//Get the item ID via the Input string
				const string weapon_name = GetPortString(pActInfo, eI_Weapon);
				IEntityClassRegistry *pRegistery = gEnv->pEntitySystem->GetClassRegistry();
				EntityId item = pInventory->GetItemByClass(pRegistery->FindClass(weapon_name));

				//Fetch the actual weapon via the ID
				IEntity* pEntity = gEnv->pEntitySystem->GetEntity(item);
				if(pEntity)
				{
					CGameObject * pGameObject = (CGameObject*)pEntity->GetProxy(ENTITY_PROXY_USER);
					CWeapon* pWeapon = (CWeapon*)pGameObject->QueryExtension(pGameObject->GetEntity()->GetClass()->GetName());

					//If the weapon exists, ask for all compatible accessories
					if(pWeapon)
					{
						//All compatible accessories for this weapon
						const DynArray<string> pCompatibleAccessoriesVec = pWeapon->GetCompatibleAccessories();

						SUIArguments args;
						DynArray<string>::const_iterator it;
						for (it = pCompatibleAccessoriesVec.begin(); it != pCompatibleAccessoriesVec.end(); ++it)
						{
							args.AddArgument(*it);
						}
						ActivateOutput(pActInfo, eO_Args, string(args.GetAsString()));
					}
				}
			}
		}
		//return, if 'accessories' is empty, it has no compatible attachments, or the weapon/inventory was invalid
		ActivateOutput(pActInfo, eO_OnCall, true);
	}
}

//--------------------------------------------------------------------------------------------
CFlashUICheckAccessoryState ::~CFlashUICheckAccessoryState ()
{
}

void CFlashUICheckAccessoryState ::GetConfiguration( SFlowNodeConfig &config )
{
	static const SInputPortConfig in_config[] = {
		InputPortConfig_Void( "Call", "Calls the function" ),
		InputPortConfig<string>( "Accessory", "Accessory we are checking" ),
		InputPortConfig<string>( "Weapon", "Weapon we are checking" ),
		{0}
	};

	static const SOutputPortConfig out_config[] = {
		OutputPortConfig<bool>( "Equipped", "Accessory is equipped" ),
		OutputPortConfig<bool>( "InInventory", "Accessory is in inventory, not equipped" ),
		OutputPortConfig<bool>( "DontHave", "Entity does not possess accessory" ),
		{0}
	};


	config.nFlags |= EFLN_TARGET_ENTITY;
	config.pInputPorts = in_config;
	config.pOutputPorts = out_config;
	config.sDescription = "Outputs the state of a certain accessory of a weapon";
	config.SetCategory(EFLN_APPROVED);
}

void CFlashUICheckAccessoryState ::ProcessEvent( EFlowEvent event, SActivationInfo *pActInfo )
{
	if(event == eFE_Activate && IsPortActive(pActInfo, 0))
	{
		IActor* pActor = GetInputActor( pActInfo );
		bool is_equipped = false;
		bool is_inInventory = false;

		if(pActor)
		{
			IInventory* pInventory = pActor->GetInventory();

			if(pInventory)
			{
				IEntityClassRegistry *pRegistry = gEnv->pEntitySystem->GetClassRegistry();

				//Find the accessory's class in the registry
				const string accessory_name = GetPortString(pActInfo, eI_Accessory);				
				IEntityClass* pClass = pRegistry->FindClass(accessory_name);

				//Check if its in inventory
				if(pInventory->HasAccessory(pClass) != 0)
				{
					is_inInventory = true;
				}	

				//if it is, check if its equipped as well
				if(is_inInventory)
				{
					//Get the weapon ID via the Input string
					const string weapon_name = GetPortString(pActInfo, eI_Weapon);
					EntityId item = pInventory->GetItemByClass(pRegistry->FindClass(weapon_name));

					//Fetch the actual weapon via the ID
					IEntity* pEntity = gEnv->pEntitySystem->GetEntity(item);
					if(pEntity)
					{

						CGameObject * pGameObject = (CGameObject*)pEntity->GetProxy(ENTITY_PROXY_USER);
						const char* ext = pGameObject->GetEntity()->GetClass()->GetName();
						CWeapon* pWeapon = (CWeapon*)pGameObject->QueryExtension(pGameObject->GetEntity()->GetClass()->GetName());
						bool selectable = pWeapon->CanSelect();
						if(pWeapon)
						{
							if(pWeapon->GetAccessory(pClass->GetName()) != 0)
							{
								is_equipped = true;
							}					
						}
					}
				}
			}
		}

		if(!is_inInventory)
			ActivateOutput(pActInfo, eO_DontHave, true);
		else if(is_equipped)
			ActivateOutput(pActInfo, eO_Equipped, true);
		else
			ActivateOutput(pActInfo, eO_InInventory, true);

	}
}


//--------------------------------------------------------------------------------------------
void CSetEquipmentLoadoutNode::GetConfiguration( SFlowNodeConfig &config )
{
	static const SInputPortConfig in_config[] = {
		InputPortConfig_Void( "Set", "Sets the equipment loadout (only for MP)" ),
		InputPortConfig<int>( "pack", "which pack" ),
		{0}
	};

	static const SOutputPortConfig out_config[] = {
		{0}
	};


	config.nFlags |= EFLN_TARGET_ENTITY;
	config.pInputPorts = in_config;
	config.pOutputPorts = out_config;
	config.sDescription = "MP only: set an equipmentloadout for the local player";
	config.SetCategory(EFLN_APPROVED);
}

void CSetEquipmentLoadoutNode ::ProcessEvent( EFlowEvent event, SActivationInfo *pActInfo )
{
	if(event == eFE_Activate && IsPortActive(pActInfo, 0))
	{
		if(pActInfo->pEntity)
		{

			IActor* pActor = GetInputActor( pActInfo );
			CGameRules *pGameRules = g_pGame->GetGameRules();

			if(pActor)
			{
				int packIndex = GetPortInt(pActInfo, eI_EquipLoadout);
				CEquipmentLoadout *pEquipmentLoadout = g_pGame->GetEquipmentLoadout();
				if(pEquipmentLoadout)
				{
					pEquipmentLoadout->SetSelectedPackage(packIndex);
					pGameRules->SetPendingLoadoutChange();
				}
			}

		}

	}
}

//--------------------------------------------------------------------------------------------
REGISTER_FLOW_NODE( "UI:WeaponCustomization:CheckAccessoryState", CFlashUICheckAccessoryState  );
REGISTER_FLOW_NODE( "UI:WeaponCustomization:CompatibleAccessories", CFlashUIGetCompatibleAccessoriesNode  );
REGISTER_FLOW_NODE( "UI:WeaponCustomization:GetEquippedAccessories", CFlashUIGetEquippedAccessoriesNode );
REGISTER_FLOW_NODE( "UI:WeaponCustomization:GetInventoryForUI", CFlashUIInventoryNode );
REGISTER_FLOW_NODE( "Game:MP:SetEquipmentLoadout", CSetEquipmentLoadoutNode );
