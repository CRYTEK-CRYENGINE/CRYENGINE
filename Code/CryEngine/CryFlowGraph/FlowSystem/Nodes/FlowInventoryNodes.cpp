// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

// -------------------------------------------------------------------------
//  File name:   FlowInventoryNodes.cpp
//  Version:     v1.00
//  Created:     July 11th 2005 by Julien.
//  Compilers:   Visual Studio.NET 2003
//  Description:
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////

#include "StdAfx.h"
#include "CryAction.h"
#include "IActorSystem.h"
#include "ItemSystem.h"
#include "GameObjects/GameObject.h"
#include "Inventory.h"
#include "IGameRulesSystem.h"
#include <CryFlowGraph/IFlowBaseNode.h>

template<ENodeCloneType CLONE_TYPE>
class CFlowBaseNodeInventory : public CFlowBaseNode<CLONE_TYPE>
{
protected:
	bool InputEntityIsLocalPlayer(const IFlowNode::SActivationInfo* const pActInfo) const
	{
		bool bRet = true;

		if (pActInfo->pEntity)
		{
			IActor* pActor = gEnv->pGameFramework->GetIActorSystem()->GetActor(pActInfo->pEntity->GetId());
			if (pActor != gEnv->pGameFramework->GetClientActor())
			{
				bRet = false;
			}
		}
		else if (gEnv->bMultiplayer)
		{
			bRet = false;
		}

		return bRet;
	}

	//-------------------------------------------------------------
	// returns the actor associated with the input entity. In single player, it returns the local player if that actor does not exists.
	IActor* GetInputActor(const IFlowNode::SActivationInfo* const pActInfo) const
	{
		IActor* pActor = pActInfo->pEntity ? gEnv->pGameFramework->GetIActorSystem()->GetActor(pActInfo->pEntity->GetId()) : nullptr;
		if (!pActor && !gEnv->bMultiplayer)
		{
			pActor = gEnv->pGameFramework->GetClientActor();
		}

		return pActor;
	}
};


class CFlowNode_InventoryAddItem : public CFlowBaseNodeInventory<eNCT_Singleton>
{
public:
	CFlowNode_InventoryAddItem(SActivationInfo* pActInfo)
	{
	}

	void GetConfiguration(SFlowNodeConfig& config)
	{
		static const SInputPortConfig in_ports[] =
		{
			InputPortConfig_Void("add",         _HELP("Connect event here to add the item")),
			InputPortConfig<string>("item",     _HELP("The item to add to the inventory"),   _HELP("Item"),                                                                                _UICONFIG("enum_global_ref:item%s:ItemType")),
			InputPortConfig<string>("ItemType", "",                                          _HELP("Select from which items to choose"),                                                   0,                                            _UICONFIG("enum_string:All=,Givable=_givable,Selectable=_selectable")),
			InputPortConfig<bool>("Unique",     true,                                        _HELP("If true, only adds the item if the inventory doesn't already contain such an item")),
			{ 0 }
		};

		static const SOutputPortConfig out_ports[] =
		{
			OutputPortConfig<bool>("out", _HELP("true if the item was actually added, false if the player already had the item")),
			{ 0 }
		};

		config.sDescription = _HELP("Add an item to inventory.");
		config.nFlags |= EFLN_TARGET_ENTITY;
		config.pInputPorts = in_ports;
		config.pOutputPorts = out_ports;
		config.SetCategory(EFLN_APPROVED);
	}

	void ProcessEvent(EFlowEvent event, SActivationInfo* pActInfo)
	{
		if (event == eFE_Activate && IsPortActive(pActInfo, 0))
		{
			IActor* pActor = GetInputActor(pActInfo);
			if (!pActor || pActor != gEnv->pGameFramework->GetClientActor())  // to avoid some extra RMIs and object creation. Tho, this causes the node to not work properly if it is used with non players entities. (which was never intended anyway)
				return;

			IEntitySystem* pEntSys = gEnv->pEntitySystem;

			IInventory* pInventory = pActor->GetInventory();
			if (!pInventory)
				return;

			const string& itemClass = GetPortString(pActInfo, 1);
			const char* pItemClass = itemClass.c_str();
			IEntityClass* pClass = gEnv->pEntitySystem->GetClassRegistry()->FindClass(pItemClass);
			EntityId id = pInventory->GetItemByClass(pClass);

			if (id == 0 || GetPortBool(pActInfo, 3) == false)
			{
				if (gEnv->bServer)
				{
					gEnv->pGameFramework->GetIItemSystem()->GiveItem(pActor, pItemClass, false, true, true);
				}
				else
				{
					// TODO: instant output activation, with delayed effective change in the inventory, it potentially could cause problems in rare situations
					pInventory->RMIReqToServer_AddItem(pItemClass);
				}
				ActivateOutput(pActInfo, 0, true);
			}
			else
			{
				// item already in inventory
				ActivateOutput(pActInfo, 0, false);
			}
		}
		else if (event == eFE_PrecacheResources)
		{
			const string& itemClass = GetPortString(pActInfo, 1);

			if (!itemClass.empty())
			{
				IGameRules* pGameRules = CCryAction::GetCryAction()->GetIGameRulesSystem()->GetCurrentGameRules();
				CRY_ASSERT_MESSAGE(pGameRules != NULL, "No game rules active, can not precache resources");
				if (pGameRules)
				{
					pGameRules->PrecacheLevelResource(itemClass.c_str(), eGameResourceType_Item);
				}
			}
		}
	}

	virtual void GetMemoryUsage(ICrySizer* s) const
	{
		s->Add(*this);
	}
};

class CFlowNode_InventoryRemoveItem : public CFlowBaseNodeInventory<eNCT_Singleton>
{
public:
	CFlowNode_InventoryRemoveItem(SActivationInfo* pActInfo)
	{
	}

	enum EInputs
	{
		eIn_Trigger,
		eIn_Item,
		eIn_ItemType,
	};

	enum EOutputs
	{
		eOut_Success,
	};

	void GetConfiguration(SFlowNodeConfig& config)
	{
		static const SInputPortConfig in_ports[] =
		{
			InputPortConfig_Void("Remove",      _HELP("Connect event here to remove the item")),
			InputPortConfig<string>("item",     _HELP("The item to remove from the inventory"), _HELP("Item"),                               _UICONFIG("enum_global_ref:item%s:ItemType")),
			InputPortConfig<string>("ItemType", "",                                             _HELP("Select from which items to choose"),  0,                                            _UICONFIG("enum_string:All=,Givable=_givable,Selectable=_selectable")),
			{ 0 }
		};

		static const SOutputPortConfig out_ports[] =
		{
			OutputPortConfig<bool>("out", _HELP("true if the item was actually removed, false if the player did not have the item")),
			{ 0 }
		};

		config.sDescription = _HELP("Remove an item from inventory.");
		config.nFlags |= EFLN_TARGET_ENTITY;
		config.pInputPorts = in_ports;
		config.pOutputPorts = out_ports;
		config.SetCategory(EFLN_APPROVED);
	}

	void ProcessEvent(EFlowEvent event, SActivationInfo* pActInfo)
	{
		if (event == eFE_Activate && IsPortActive(pActInfo, eIn_Trigger))
		{
			IActor* pActor = GetInputActor(pActInfo);
			if (!pActor || pActor != gEnv->pGameFramework->GetClientActor()) // to avoid some extra RMIs and object creation. Tho, this causes the node to not work properly if it is used with non players entities. (which was never intended anyway)
			{
				return;
			}

			IInventory* pInventory = pActor->GetInventory();
			if (!pInventory)
			{
				return;
			}

			const string& itemClass = GetPortString(pActInfo, eIn_Item);
			IEntityClass* pClass = gEnv->pEntitySystem->GetClassRegistry()->FindClass(itemClass.c_str());
			EntityId id = pInventory->GetItemByClass(pClass);

			if (id != 0)
			{
				pInventory->RMIReqToServer_RemoveItem(itemClass.c_str());
				ActivateOutput(pActInfo, eOut_Success, true);
			}
			else
			{
				ActivateOutput(pActInfo, eOut_Success, false);
			}
		}
	}

	virtual void GetMemoryUsage(ICrySizer* s) const
	{
		s->Add(*this);
	}
};

class CFlowNode_InventoryRemoveAllItems : public CFlowBaseNodeInventory<eNCT_Singleton>
{
public:
	CFlowNode_InventoryRemoveAllItems(SActivationInfo* pActInfo)
	{
	}

	void GetConfiguration(SFlowNodeConfig& config)
	{
		static const SInputPortConfig in_ports[] =
		{
			InputPortConfig_Void("Activate", _HELP("Connect event here to remove all items from inventory")),
			{ 0 }
		};

		static const SOutputPortConfig out_ports[] =
		{
			OutputPortConfig<bool>("Done", _HELP("True when done successfully")),
			{ 0 }
		};

		config.sDescription = _HELP("When activated, removes all items from inventory.");
		config.nFlags |= EFLN_TARGET_ENTITY;
		config.pInputPorts = in_ports;
		config.pOutputPorts = out_ports;
		config.SetCategory(EFLN_APPROVED);
	}

	void ProcessEvent(EFlowEvent event, SActivationInfo* pActInfo)
	{
		if (event == eFE_Activate && IsPortActive(pActInfo, 0))
		{
			IGameFramework* pGF = gEnv->pGameFramework;

			IActor* pActor = GetInputActor(pActInfo);
			if (!pActor || pActor != gEnv->pGameFramework->GetClientActor())  // to avoid some extra RMIs and object creation. Tho, this causes the node to not work properly if it is used with non players entities. (which was never intended anyway)
				return;

			IInventory* pInventory = pActor->GetInventory();
			if (!pInventory)
				return;

			pInventory->RMIReqToServer_RemoveAllItems();
			// TODO: instant output activation, with delayed effective change in the inventory, it potentially could cause problems in rare situations
			ActivateOutput(pActInfo, 0, true);
		}
	}

	virtual void GetMemoryUsage(ICrySizer* s) const
	{
		s->Add(*this);
	}
};

class CFlowNode_InventoryHasItem : public CFlowBaseNodeInventory<eNCT_Singleton>
{
public:
	CFlowNode_InventoryHasItem(SActivationInfo* pActInfo)
	{
	}

	void GetConfiguration(SFlowNodeConfig& config)
	{
		static const SInputPortConfig in_ports[] =
		{
			InputPortConfig_Void("check",       _HELP("Connect event here to check the inventory for the item"), _HELP("Check")),
			InputPortConfig<string>("item",     _HELP("The item to add to the inventory"),                       _HELP("Item"),                             _UICONFIG("enum_global_ref:item%s:ItemType")),
			InputPortConfig<string>("ItemType", "",                                                              _HELP("Select from which items to choose"),0,                     _UICONFIG("enum_string:All=,Givable=_givable,Selectable=_selectable")),
			{ 0 }
		};

		static const SOutputPortConfig out_ports[] =
		{
			OutputPortConfig<bool>("out",        _HELP("True if the player has the item, false otherwise"), _HELP("Out")),
			OutputPortConfig_Void("False",       _HELP("Triggered if player does not have the item")),
			OutputPortConfig_Void("True",        _HELP("Triggered if player has the item")),
			OutputPortConfig<EntityId>("ItemId", _HELP("Outputs the item's entity id")),
			{ 0 }
		};

		config.sDescription = _HELP("Check inventory to see if an item is present.");
		config.nFlags |= EFLN_TARGET_ENTITY;
		config.pInputPorts = in_ports;
		config.pOutputPorts = out_ports;
		config.SetCategory(EFLN_APPROVED);
	}

	void ProcessEvent(EFlowEvent event, SActivationInfo* pActInfo)
	{
		if (event == eFE_Activate && IsPortActive(pActInfo, 0))
		{
			IActor* pActor = GetInputActor(pActInfo);
			if (!pActor)
				return;

			IInventory* pInventory = pActor->GetInventory();
			if (!pInventory)
				return;

			IEntityClass* pClass = gEnv->pEntitySystem->GetClassRegistry()->FindClass(GetPortString(pActInfo, 1));
			EntityId id = pInventory->GetItemByClass(pClass);

			ActivateOutput(pActInfo, 0, id != 0 ? true : false);
			ActivateOutput(pActInfo, id != 0 ? 2 : 1, true);
			ActivateOutput(pActInfo, 3, id);
		}
	}

	virtual void GetMemoryUsage(ICrySizer* s) const
	{
		s->Add(*this);
	}
};

class CFlowNode_InventorySelectItem : public CFlowBaseNodeInventory<eNCT_Singleton>
{
public:
	CFlowNode_InventorySelectItem(SActivationInfo* pActInfo)
	{
	}

	void GetConfiguration(SFlowNodeConfig& config)
	{
		static const SInputPortConfig in_ports[] =
		{
			InputPortConfig_Void("Activate", _HELP("Select the item")),
			InputPortConfig<string>("Item",  _HELP("Item to select, has to be in inventory"),0, _UICONFIG("enum_global:item_selectable")),
			{ 0 }
		};

		static const SOutputPortConfig out_ports[] =
		{
			OutputPortConfig<bool>("Done", _HELP("True when executed, if the item is in the inventory and could be selected")),
			{ 0 }
		};

		config.sDescription = _HELP("Select an item from within the inventory.");
		config.nFlags |= EFLN_TARGET_ENTITY;
		config.pInputPorts = in_ports;
		config.pOutputPorts = out_ports;
		config.SetCategory(EFLN_APPROVED);
	}

	void ProcessEvent(EFlowEvent event, SActivationInfo* pActInfo)
	{
		IGameFramework* pGF = gEnv->pGameFramework;

		if (event == eFE_Activate && IsPortActive(pActInfo, 0))
		{
			IActor* pActor = GetInputActor(pActInfo);
			if (!pActor)
				return;

			IInventory* pInventory = pActor->GetInventory();
			if (!pInventory)
				return;

			const string& item = GetPortString(pActInfo, 1);

			IEntityClass* pClass = gEnv->pEntitySystem->GetClassRegistry()->FindClass(item);
			if (0 == pInventory->GetItemByClass(pClass))
			{
				ActivateOutput(pActInfo, 0, false);
			}
			else
			{
				pGF->GetIItemSystem()->SetActorItem(pActor, item, true);
				ActivateOutput(pActInfo, 0, true);
			}
		}
	}

	virtual void GetMemoryUsage(ICrySizer* s) const
	{
		s->Add(*this);
	}
};

class CFlowNode_InventoryHolsterItem : public CFlowBaseNode<eNCT_Singleton>
{
public:
	CFlowNode_InventoryHolsterItem(SActivationInfo* pActInfo)
	{
	}

	void GetConfiguration(SFlowNodeConfig& config)
	{
		static const SInputPortConfig in_ports[] =
		{
			InputPortConfig_Void("Holster",   _HELP("Holster current item")),
			InputPortConfig_Void("UnHolster", _HELP("Unholster current item")),
			{ 0 }
		};

		static const SOutputPortConfig out_ports[] =
		{
			OutputPortConfig_Void("Done", _HELP("Done")),
			{ 0 }
		};

		config.pInputPorts = in_ports;
		config.pOutputPorts = out_ports;
		config.SetCategory(EFLN_APPROVED);
	}

	void ProcessEvent(EFlowEvent event, SActivationInfo* pActInfo)
	{
		if (event == eFE_Activate)
		{
			IGameFramework* pGF = gEnv->pGameFramework;

			IActor* pActor = pGF->GetClientActor();
			if (!pActor)
				return;
			const bool bHolster = IsPortActive(pActInfo, 0);
			pActor->HolsterItem(bHolster);
			ActivateOutput(pActInfo, 0, true);
		}
	}

	virtual void GetMemoryUsage(ICrySizer* s) const
	{
		s->Add(*this);
	}
};

class CFlowNode_InventoryItemSelected : public CFlowBaseNodeInventory<eNCT_Instanced>, public IItemSystemListener
{

	enum
	{
		INP_ACTIVE = 0,
		INP_CHECK
	};

public:
	CFlowNode_InventoryItemSelected(SActivationInfo* pActInfo) : m_actInfo(*pActInfo)
	{
		m_pGF = gEnv->pGameFramework;
	}

	~CFlowNode_InventoryItemSelected()
	{
		IItemSystem* pItemSys = m_pGF->GetIItemSystem();
		if (pItemSys)
			pItemSys->UnregisterListener(this);
	}

	IFlowNodePtr Clone(SActivationInfo* pActInfo)
	{
		return new CFlowNode_InventoryItemSelected(pActInfo);
	}

	void GetConfiguration(SFlowNodeConfig& config)
	{
		static const SInputPortConfig in_ports[] =
		{
			InputPortConfig<bool>("Active",  true,                                                                             _HELP("Set to active to get notified when a new item was selected")),
			InputPortConfig_AnyType("Check", _HELP("Trigger to instantly check which item the actor has currently selected")),
			{ 0 }
		};

		static const SOutputPortConfig out_ports[] =
		{
			OutputPortConfig<string>("ItemClass", _HELP("Outputs selected item's class")),
			OutputPortConfig<string>("ItemName",  _HELP("Outputs selected item's name")),
			OutputPortConfig<EntityId>("ItemId",  _HELP("Outputs selected item's entity id")),
			{ 0 }
		};

		config.sDescription = _HELP("Tracks the currently selected item of the actor. Use [Active] to enable.");
		config.nFlags |= EFLN_TARGET_ENTITY;
		config.pInputPorts = in_ports;
		config.pOutputPorts = out_ports;
		config.SetCategory(EFLN_APPROVED);
	}

	void ProcessEvent(EFlowEvent event, SActivationInfo* pActInfo)
	{
		if (event == eFE_Activate || event == eFE_Initialize)
		{
			if (IsPortActive(pActInfo, INP_ACTIVE))
			{
				bool active = GetPortBool(pActInfo, 0);
				if (active)
					m_pGF->GetIItemSystem()->RegisterListener(this);
				else
					m_pGF->GetIItemSystem()->UnregisterListener(this);
			}
			if (IsPortActive(pActInfo, INP_CHECK))
			{
				IActor* pActor = GetInputActor(pActInfo);
				if (pActor)
				{
					IInventory* pInventory = pActor->GetInventory();
					if (pInventory)
					{
						IEntity* pEntityItem = gEnv->pEntitySystem->GetEntity(pInventory->GetCurrentItem());
						TriggerOutputs(pActInfo, pEntityItem);
					}
				}
			}
		}
	}

	virtual void OnSetActorItem(IActor* pSetActor, IItem* pItem)
	{
		IActor* pActor = GetInputActor(&m_actInfo);
		if (!pActor || pActor != pSetActor)
			return;

		TriggerOutputs(&m_actInfo, pItem ? pItem->GetEntity() : NULL);
	}

	virtual void OnDropActorItem(IActor* pActor, IItem* pItem)
	{
	}

	virtual void OnSetActorAccessory(IActor* pActor, IItem* pItem)
	{
	}

	virtual void OnDropActorAccessory(IActor* pActor, IItem* pItem)
	{
	}

	virtual void TriggerOutputs(SActivationInfo* pActInfo, IEntity* pItem)
	{
		string name;
		if (pItem)
		{
			name = pItem->GetClass()->GetName();
			ActivateOutput(&m_actInfo, 0, name);
			name = pItem->GetName();
			ActivateOutput(&m_actInfo, 1, name);
			ActivateOutput(&m_actInfo, 2, pItem->GetId());
		}
		else
		{
			ActivateOutput(&m_actInfo, 0, name);
			ActivateOutput(&m_actInfo, 1, name);
			ActivateOutput(&m_actInfo, 2, 0);
		}
	}

	virtual void GetMemoryUsage(ICrySizer* s) const
	{
		s->Add(*this);
	}

protected:
	SActivationInfo m_actInfo;
	IGameFramework* m_pGF;
};

class CFlowNode_InventoryRemoveAllAmmo : public CFlowBaseNodeInventory<eNCT_Singleton>
{
public:
	CFlowNode_InventoryRemoveAllAmmo(SActivationInfo* pActInfo)
	{
	}

	void GetConfiguration(SFlowNodeConfig& config)
	{
		static const SInputPortConfig in_ports[] =
		{
			InputPortConfig_Void("Remove",            _HELP("Connect event here to remove all the ammo")),
			InputPortConfig<bool>("RemoveFromWeapon", false,                                              _HELP("If true, also removes all ammo from all weapons.")),
			{ 0 }
		};

		static const SOutputPortConfig out_ports[] =
		{
			OutputPortConfig<bool>("Out", _HELP("true if the ammo was actually removed, false otherwise")),
			{ 0 }
		};

		config.sDescription = _HELP("Remove ammo from current weapon and/or all ammo in inventory.");
		config.nFlags |= EFLN_TARGET_ENTITY;
		config.pInputPorts = in_ports;
		config.pOutputPorts = out_ports;
		config.SetCategory(EFLN_APPROVED);
	}

	void ProcessEvent(EFlowEvent event, SActivationInfo* pActInfo)
	{
		if (event == eFE_Activate && IsPortActive(pActInfo, 0))
		{
			IActor* pActor = GetInputActor(pActInfo);
			if (!pActor)
				return;

			IInventory* pInventory = pActor->GetInventory();

			if (pInventory)
			{
				pInventory->ResetAmmo();

				const bool removeFromWeapons = GetPortBool(pActInfo, 1);
				if (removeFromWeapons)
				{
					IItemSystem* pItemSystem = CCryAction::GetCryAction()->GetIItemSystem();
					const size_t numItems = pInventory->GetCount();
					for (size_t i = 0; i < numItems; ++i)
					{
						const EntityId item = pInventory->GetItem(i);
						if (IItem* pItem = pItemSystem->GetItem(item))
						{
							if (IWeapon* pWeapon = pItem->GetIWeapon())
							{
								const size_t numFiremodes = pWeapon->GetNumOfFireModes();
								for (size_t f = 0; f < numFiremodes; ++f)
								{
									if (IFireMode* pFireMode = pWeapon->GetFireMode(f))
									{
										if (IEntityClass* pAmmoType = pFireMode->GetAmmoType())
										{
											pWeapon->SetAmmoCount(pAmmoType, 0);
										}
									}
								}
							}
						}
					}
				}
				ActivateOutput(pActInfo, 0, true);
			}
			else
				ActivateOutput(pActInfo, 0, false);
		}
	}

	virtual void GetMemoryUsage(ICrySizer* s) const
	{
		s->Add(*this);
	}
};

class CFlowNode_AddEquipmentPack : public CFlowBaseNodeInventory<eNCT_Singleton>
{
public:
	enum EInputs
	{
		EIP_Trigger,
		EIP_EquipmentPack,
		EIP_AddToggle,
		EIP_SelectPrimary
	};

	enum EOutputs
	{
		EOP_Done
	};

	CFlowNode_AddEquipmentPack(SActivationInfo* pActInfo)
	{
	}

	void GetConfiguration(SFlowNodeConfig& config)
	{
		static const SInputPortConfig in_ports[] =
		{
			InputPortConfig_Void("Trigger",        _HELP("Trigger to give the pack")),
			InputPortConfig<string>("equip_Pack",  _HELP("Name of the pack")),
			InputPortConfig<bool>("Add",           false,                             _HELP("Replace/Add selected equipment pack; 0=Replace,1=Add")),
			InputPortConfig<bool>("SelectPrimary", false,                             _HELP("Select primary weapon by default")),
			{ 0 }
		};

		static const SOutputPortConfig out_ports[] =
		{
			OutputPortConfig_Void("Done", _HELP("Triggered when done.")),
			{ 0 }
		};

		config.sDescription = _HELP("Add to existing or replace existing equipment pack.");
		config.nFlags |= EFLN_TARGET_ENTITY;
		config.pInputPorts = in_ports;
		config.pOutputPorts = out_ports;
		config.SetCategory(EFLN_APPROVED);
	}

	void ProcessEvent(EFlowEvent event, SActivationInfo* pActInfo)
	{
		if (event == eFE_Activate && IsPortActive(pActInfo, EIP_Trigger))
		{
			IActor* pActor = GetInputActor(pActInfo);
			if (!pActor)
				return;

			IInventory* pInventory = pActor->GetInventory();
			if (!pInventory)
				return;

			const bool& addPack = GetPortBool(pActInfo, EIP_AddToggle);
			const string& packName = GetPortString(pActInfo, EIP_EquipmentPack);
			const bool& selectPrimary = GetPortBool(pActInfo, EIP_SelectPrimary);

			if (pActor->IsPlayer())
				pInventory->RMIReqToServer_AddEquipmentPack(packName.c_str(), addPack, selectPrimary);
			else
			{
				if (gEnv->bServer)
					CCryAction::GetCryAction()->GetIItemSystem()->GetIEquipmentManager()->GiveEquipmentPack(pActor, packName.c_str(), addPack, selectPrimary);
			}

			// TODO: instant output activation, with delayed effective change in the inventory, it potentially could cause problems in rare situations
			ActivateOutput(pActInfo, EOP_Done, true);
		}
		else if (event == eFE_PrecacheResources)
		{
			const string& packName = GetPortString(pActInfo, 1);

			if (!packName.empty())
			{
				IGameRules* const pGameRules = CCryAction::GetCryAction()->GetIGameRulesSystem()->GetCurrentGameRules();
				if (pGameRules)
				{
					pGameRules->PrecacheLevelResource(packName.c_str(), eGameResourceType_Loadout);
				}
				else
				{
					GameWarning("No game rules active, can not precache resources");
				}
			}
		}
	}

	virtual void GetMemoryUsage(ICrySizer* s) const
	{
		s->Add(*this);
	}
};

class CFlowNode_StorePlayerInventory : public CFlowBaseNode<eNCT_Singleton>
{
public:
	CFlowNode_StorePlayerInventory(SActivationInfo* pActInfo)
	{
	}

	void GetConfiguration(SFlowNodeConfig& config)
	{
		static const SInputPortConfig in_ports[] =
		{
			InputPortConfig_Void("Trigger", _HELP("Trigger to store Player's Inventory.")),
			{ 0 }
		};

		static const SOutputPortConfig out_ports[] =
		{
			OutputPortConfig_Void("Done", _HELP("Triggered when done.")),
			{ 0 }
		};

		config.pInputPorts = in_ports;
		config.pOutputPorts = out_ports;
		config.SetCategory(EFLN_ADVANCED);
	}

	void ProcessEvent(EFlowEvent event, SActivationInfo* pActInfo)
	{
		if (event == eFE_Activate && IsPortActive(pActInfo, 0))
		{
			CItemSystem* pItemSystem = static_cast<CItemSystem*>(CCryAction::GetCryAction()->GetIItemSystem());
			pItemSystem->SerializePlayerLTLInfo(false);
			ActivateOutput(pActInfo, 0, true);
		}
	}

	virtual void GetMemoryUsage(ICrySizer* s) const
	{
		s->Add(*this);
	}
};

class CFlowNode_RestorePlayerInventory : public CFlowBaseNode<eNCT_Singleton>
{
public:
	CFlowNode_RestorePlayerInventory(SActivationInfo* pActInfo)
	{
	}

	void GetConfiguration(SFlowNodeConfig& config)
	{
		static const SInputPortConfig in_ports[] =
		{
			InputPortConfig_Void("Trigger", _HELP("Trigger to restore Player's Inventory.")),
			{ 0 }
		};

		static const SOutputPortConfig out_ports[] =
		{
			OutputPortConfig_Void("Done", _HELP("Triggered when done.")),
			{ 0 }
		};

		config.pInputPorts = in_ports;
		config.pOutputPorts = out_ports;
		config.SetCategory(EFLN_ADVANCED);
	}

	void ProcessEvent(EFlowEvent event, SActivationInfo* pActInfo)
	{
		if (event == eFE_Activate && IsPortActive(pActInfo, 0))
		{
			CItemSystem* pItemSystem = static_cast<CItemSystem*>(CCryAction::GetCryAction()->GetIItemSystem());
			pItemSystem->SerializePlayerLTLInfo(true);
			ActivateOutput(pActInfo, 0, true);
		}
	}

	virtual void GetMemoryUsage(ICrySizer* s) const
	{
		s->Add(*this);
	}
};

REGISTER_FLOW_NODE("Inventory:ItemAdd", CFlowNode_InventoryAddItem);
REGISTER_FLOW_NODE("Inventory:ItemRemove", CFlowNode_InventoryRemoveItem);
REGISTER_FLOW_NODE("Inventory:ItemRemoveAll", CFlowNode_InventoryRemoveAllItems);
REGISTER_FLOW_NODE("Inventory:ItemCheck", CFlowNode_InventoryHasItem);
REGISTER_FLOW_NODE("Inventory:HolsterItem", CFlowNode_InventoryHolsterItem);
REGISTER_FLOW_NODE("Inventory:ItemSelect", CFlowNode_InventorySelectItem);
REGISTER_FLOW_NODE("Inventory:ItemSelected", CFlowNode_InventoryItemSelected);
REGISTER_FLOW_NODE("Inventory:AmmoRemoveAll", CFlowNode_InventoryRemoveAllAmmo);
REGISTER_FLOW_NODE("Inventory:EquipPackAdd", CFlowNode_AddEquipmentPack);
REGISTER_FLOW_NODE("Inventory:PlayerInventoryStore", CFlowNode_StorePlayerInventory);
REGISTER_FLOW_NODE("Inventory:PlayerInventoryRestore", CFlowNode_RestorePlayerInventory);
