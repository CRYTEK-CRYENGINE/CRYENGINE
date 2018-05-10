// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "Game.h"
#include "Weapon.h"
#include "Actor.h"

#include <CryString/StringUtils.h>
#include <CryFlowGraph/IFlowBaseNode.h>

namespace
{
IWeapon* GetWeapon(IActor* pActor)
{
	IInventory* pInventory = pActor->GetInventory();
	if (!pInventory)
		return 0;
	EntityId itemId = pInventory->GetCurrentItem();
	if (itemId == 0)
		return 0;
	IItem* pItem = g_pGame->GetIGameFramework()->GetIItemSystem()->GetItem(itemId);
	if (pItem == 0)
		return 0;
	IWeapon* pWeapon = pItem->GetIWeapon();
	return pWeapon;
}

IWeapon* GetWeapon(IActor* pActor, const char* className)
{
	IInventory* pInventory = pActor->GetInventory();
	if (!pInventory)
		return 0;
	IEntityClass* pEntityClass = gEnv->pEntitySystem->GetClassRegistry()->FindClass(className);
	if (!pEntityClass)
		return 0;
	EntityId itemId = pInventory->GetItemByClass(pEntityClass);
	if (itemId == 0)
		return 0;
	IItem* pItem = g_pGame->GetIGameFramework()->GetIItemSystem()->GetItem(itemId);
	if (pItem == 0)
		return 0;
	IWeapon* pWeapon = pItem->GetIWeapon();
	return pWeapon;
}

IActor* GetActor(CFlowBaseNodeInternal::SActivationInfo* pActInfo)
{
	const IEntity* pEntity = pActInfo->pEntity;
	if (pEntity)
	{
		return g_pGame->GetIGameFramework()->GetIActorSystem()->GetActor(pEntity->GetId());
	}
	return NULL;
}
};

// WeaponTracker not yet finished

/*
   class CFlowNode_WeaponTracker : public CFlowBaseNode, public IItemSystemListener, public IWeaponEventListener
   {
   public:
   CFlowNode_WeaponTracker( SActivationInfo * pActInfo ) : m_entityId(0)
   {
   }

   ~CFlowNode_WeaponTracker()
   {
   Unregister();
   }

   void Register()
   {
   IItemSystem* pItemSys = g_pGame->GetIGameFramework()->GetIItemSystem();
   if (pItemSys)
   pItemSys->RegisterListener(this);
   }

   void Unregister()
   {
   IItemSystem* pItemSys = g_pGame->GetIGameFramework()->GetIItemSystem();
   if (pItemSys)
   pItemSys->UnregisterListener(this);
   }

   IFlowNodePtr Clone(SActivationInfo* pActInfo)
   {
   return new CFlowNode_WeaponTracker(pActInfo);
   }

   void GetConfiguration( SFlowNodeConfig& config )
   {
   static const SInputPortConfig in_ports[] =
   {
   InputPortConfig_Void( "Activate",    _HELP("Activate") ),
   InputPortConfig_Void( "Deactivate",  _HELP("Deactivate") ),
   {0}
   };
   static const SOutputPortConfig out_ports[] =
   {
   OutputPortConfig<string>( "AccessoryAdded",   _HELP("Accessory was added")),
   OutputPortConfig<string>( "AccessoryRemoved", _HELP("Accessory was removed")),
   {0}
   };
   config.nFlags |= EFLN_TARGET_ENTITY;
   config.pInputPorts = in_ports;
   config.pOutputPorts = out_ports;
   config.sDescription = _HELP("Listens for Player's accessory changes");
   config.SetCategory(EFLN_APPROVED);
   }

   void ProcessEvent( EFlowEvent event, SActivationInfo *pActInfo )
   {
   switch (event)
   {
   case eFE_Initialize:
   m_actInfo = *pActInfo;
   break;
   case eFE_SetEntityId:
   m_entityId = pActInfo->pEntity ? pActInfo->pEntity->GetId() : 0;
   break;
   case eFE_Activate:
   {
   if (IsPortActive(pActInfo, 1))
   {
   UnregisterWeapon();
   Unregister();
   }
   if (IsPortActive(pActInfo, 0))
   {
   Register();
   RegisterCurrent(pActInfo);
   }
   }
   break;
   }
   }

   protected:
   virtual void OnSetActorItem(IActor *pActor, IItem *pItem )
   {
   if (pActor->GetEntityId() != m_entityId)
   return;

   UnregisterWeapon();
   if (pItem == 0)
   return;

   IWeapon* pWeapon = pItem->GetIWeapon();
   if (pWeapon == 0)
   return;
   RegisterWeapon(pWeapon);
   }

   void RegisterCurrent(SActivationInfo* pActInfo)
   {
   IActor* pActor = GetActor(pActInfo);
   if (pActor == 0)
   return;
   IWeapon* pWeapon = GetWeapon(pActor);
   if (pWeapon == 0)
   return;
   RegisterWeapon(pWeapon);
   }

   void RegisterWeapon(IWeapon* pWeapon)
   {
   m_weaponId = pWeapon->GetEntity()->GetId();
   pWeapon->AddEventListener(this);
   }

   void UnregisterWeapon()
   {
   if (m_weaponId != 0)
   {
   IItemSystem* pItemSys = g_pGame->GetIGameFramework()->GetIItemSystem();
   if (pItemSys != 0)
   {
   IItem* pItem = pItemSys->GetItem(m_weaponId);
   if (pItem != 0)
   {
   IWeapon* pWeapon = pItem->GetIWeapon();
   if (pWeapon != 0)
   pWeapon->RemoveEventListener(this);
   }
   }
   }
   m_weaponId = 0;
   }

   SActivationInfo m_actInfo;
   EntityId m_entityId;
   EntityId m_weaponId;
   };
 */

class CFlowNode_WeaponCheckAccessory : public CFlowBaseNode<eNCT_Singleton>
{
public:
	CFlowNode_WeaponCheckAccessory(SActivationInfo* pActInfo) {}

	void GetConfiguration(SFlowNodeConfig& config)
	{
		static const SInputPortConfig in_ports[] =
		{
			InputPortConfig_Void("Check",        _HELP("Trigger this port to check for accessory on the Actor's current weapon")),
			InputPortConfig<string>("Accessory", _HELP("Name of accessory to check for."),                                        0,_UICONFIG("enum_global:item")),
			{ 0 }
		};
		static const SOutputPortConfig out_ports[] =
		{
			OutputPortConfig_Void("False", _HELP("Triggered if accessory is not attached.")),
			OutputPortConfig_Void("True",  _HELP("Triggered if accessory is attached.")),
			{ 0 }
		};
		config.nFlags |= EFLN_TARGET_ENTITY;
		config.pInputPorts = in_ports;
		config.pOutputPorts = out_ports;
		config.sDescription = _HELP("Checks if target actor's current weapon has [Accessory] attached.");
		config.SetCategory(EFLN_APPROVED);
	}

	void ProcessEvent(EFlowEvent event, SActivationInfo* pActInfo)
	{
		if (eFE_Activate == event && IsPortActive(pActInfo, 0))
		{
			IActor* pActor = GetActor(pActInfo);
			if (pActor == 0)
				return;

			CWeapon* pWeapon = static_cast<CWeapon*>(GetWeapon(pActor));
			if (pWeapon != 0)
			{
				CItem* pAcc = pWeapon->GetAccessory(GetPortString(pActInfo, 1).c_str());
				if (pAcc != 0)
					ActivateOutput(pActInfo, 1, true); // [True]
				else
					ActivateOutput(pActInfo, 0, true); // [False]
			}
		}
	}

	virtual void GetMemoryUsage(ICrySizer* s) const
	{
		s->Add(*this);
	}
};

class CFlowNode_WeaponCheckZoom : public CFlowBaseNode<eNCT_Singleton>
{
public:
	CFlowNode_WeaponCheckZoom(SActivationInfo* pActInfo) {}

	void GetConfiguration(SFlowNodeConfig& config)
	{
		static const SInputPortConfig in_ports[] =
		{
			InputPortConfig_Void("Check",     _HELP("Trigger this port to check if Actor's current weapon is zoomed")),
			InputPortConfig<string>("Weapon", _HELP("Name of Weapon to check. Empty=All."),                            0,_UICONFIG("enum_global:weapon")),
			{ 0 }
		};
		static const SOutputPortConfig out_ports[] =
		{
			OutputPortConfig_Void("False", _HELP("Triggered if weapon is not zoomed.")),
			OutputPortConfig_Void("True",  _HELP("Triggered if weapon is zoomed.")),
			{ 0 }
		};
		config.nFlags |= EFLN_TARGET_ENTITY;
		config.pInputPorts = in_ports;
		config.pOutputPorts = out_ports;
		config.sDescription = _HELP("Checks if target actor's current weapon is zoomed.");
		config.SetCategory(EFLN_APPROVED);
	}

	void ProcessEvent(EFlowEvent event, SActivationInfo* pActInfo)
	{
		if (eFE_Activate == event && IsPortActive(pActInfo, 0))
		{
			IActor* pActor = GetActor(pActInfo);
			if (pActor == 0)
				return;

			CWeapon* pWeapon = static_cast<CWeapon*>(GetWeapon(pActor));
			bool bZoomed = false;
			if (pWeapon != 0)
			{
				const string& weaponClass = GetPortString(pActInfo, 1);
				if (weaponClass.empty() == true || weaponClass == pWeapon->GetEntity()->GetClass()->GetName())
				{
					bZoomed = pWeapon->IsZoomed();
				}
			}
			ActivateOutput(pActInfo, bZoomed ? 1 : 0, true);
		}
	}

	virtual void GetMemoryUsage(ICrySizer* s) const
	{
		s->Add(*this);
	}
};

class CFlowNode_WeaponAccessory : public CFlowBaseNode<eNCT_Singleton>
{
public:
	CFlowNode_WeaponAccessory(SActivationInfo* pActInfo) {}

	enum INPUTS
	{
		EIP_ATTACH = 0,
		EIP_DETACH,
		EIP_WEAPON,
		EIP_ACCESSORY,
	};

	enum OUTPUTS
	{
		EOP_ATTACHED = 0,
		EOP_DETACHED,
	};

	void GetConfiguration(SFlowNodeConfig& config)
	{
		static const SInputPortConfig in_ports[] =
		{
			InputPortConfig_Void("Attach",       _HELP("Trigger to attach accessory on the Actor's weapon")),
			InputPortConfig_Void("Detach",       _HELP("Trigger to detach accessory from the Actor's weapon")),
			InputPortConfig<string>("Weapon",    _HELP("Name of weapon the accessory should be attached/detached"),0,  _UICONFIG("enum_global:weapon")),
			InputPortConfig<string>("Accessory", _HELP("Name of accessory"),                                       0,  _UICONFIG("enum_global:item")),
			{ 0 }
		};
		static const SOutputPortConfig out_ports[] =
		{
			OutputPortConfig_Void("Attached", _HELP("Triggered if accessory was attached.")),
			OutputPortConfig_Void("Detached", _HELP("Triggered if accessory was detached.")),
			{ 0 }
		};
		config.nFlags |= EFLN_TARGET_ENTITY;
		config.pInputPorts = in_ports;
		config.pOutputPorts = out_ports;
		config.sDescription = _HELP("Attach/Detach [Accessory] from Actor's weapon [Weapon]. Both must be in the Inventory.");
		config.SetCategory(EFLN_APPROVED);
	}

	void ProcessEvent(EFlowEvent event, SActivationInfo* pActInfo)
	{
		if (eFE_Activate == event)
		{
			const bool bAttach = IsPortActive(pActInfo, EIP_ATTACH);
			const bool bDetach = IsPortActive(pActInfo, EIP_DETACH);
			if (!bAttach && !bDetach)
				return;

			IActor* pActor = GetActor(pActInfo);
			if (pActor == 0)
				return;

			const string& className = GetPortString(pActInfo, EIP_WEAPON);

			CWeapon* pWeapon = static_cast<CWeapon*>(className.empty() ? GetWeapon(pActor) : GetWeapon(pActor, className.c_str()));
			if (pWeapon != 0)
			{
				ItemString acc = ItemString(GetPortString(pActInfo, EIP_ACCESSORY));
				if (bAttach && pWeapon->GetAccessory(acc) == 0)
				{
					pWeapon->AttachAccessory(acc, true, false);
					ActivateOutput(pActInfo, EOP_ATTACHED, true);
				}
				else if (bDetach && pWeapon->GetAccessory(acc) != 0)
				{
					pWeapon->DetachAccessory(acc);
					ActivateOutput(pActInfo, EOP_DETACHED, true);
				}
			}
		}
	}

	virtual void GetMemoryUsage(ICrySizer* s) const
	{
		s->Add(*this);
	}
};

//////////////////////////////////////////////////////////////////////////
class CFlowNode_Explosion : public CFlowBaseNode<eNCT_Singleton>
{
public:
	enum EInputs
	{
		IN_TRIGGER = 0,
		IN_EFFECT,
		IN_EFFECT_SCALE,
		IN_MIN_RADIUS,
		IN_RADIUS,
		IN_MIN_PHYS_RADIUS,
		IN_PHYS_RADIUS,
		IN_PRESSURE,
		IN_DAMAGE,
		IN_DECAL,
		IN_HOLESIZE,
		IN_POSITION,
		IN_ORIENTATION,
	};

	CFlowNode_Explosion(SActivationInfo* pActInfo){}

	void GetConfiguration(SFlowNodeConfig& config)
	{
		static const SInputPortConfig inputs[] =
		{
			InputPortConfig_Void("Trigger",         _HELP("Trigger the explosion")),
			InputPortConfig<string>("Effect",       _HELP("")),
			InputPortConfig<float>("EffectScale",   1.f,                                        _HELP("Effect Scale")),
			InputPortConfig<float>("MinRadius",     5.f,                                        _HELP("Minimun radius")),
			InputPortConfig<float>("Radius",        10.f,                                       _HELP("Radius")),
			InputPortConfig<float>("MinPhysRadius", 2.5f,                                       _HELP("Minimun physical radius")),
			InputPortConfig<float>("PhysRadius",    5.f,                                        _HELP("Physical radius")),
			InputPortConfig<float>("Pressure",      1000.f,                                     _HELP("Pressure")),
			InputPortConfig<float>("Damage",        1000.f,                                     _HELP("Damage")),
			InputPortConfig<string>("Decal",        _HELP("")),
			InputPortConfig<float>("HoleSize",      1000.f,                                     _HELP("HoleSize")),
			InputPortConfig<Vec3>("Position",       _HELP("World position")),
			InputPortConfig<Vec3>("Orientation",    _HELP("Expressed as degrees on each axis")),
			{ 0 }
		};
		static const SOutputPortConfig outputs[] = {
			{ 0 }
		};

		config.sDescription = _HELP("Triggers an explosion.");
		config.pInputPorts = inputs;
		config.pOutputPorts = outputs;
		config.SetCategory(EFLN_APPROVED);
	}

	void ProcessEvent(EFlowEvent event, SActivationInfo* pActInfo)
	{
		switch (event)
		{
		case eFE_Activate:
			{
				if (IsPortActive(pActInfo, IN_TRIGGER))
				{
					string effect = GetPortString(pActInfo, IN_EFFECT);
					float effectScale = GetPortFloat(pActInfo, IN_EFFECT_SCALE);
					float minRadius = GetPortFloat(pActInfo, IN_MIN_RADIUS);
					float radius = GetPortFloat(pActInfo, IN_RADIUS);
					float minPhysRadius = GetPortFloat(pActInfo, IN_MIN_PHYS_RADIUS);
					float physRadius = GetPortFloat(pActInfo, IN_PHYS_RADIUS);
					float pressure = GetPortFloat(pActInfo, IN_PRESSURE);
					float damage = GetPortFloat(pActInfo, IN_DAMAGE);
					string decal = GetPortString(pActInfo, IN_DECAL);
					float holeSize = GetPortFloat(pActInfo, IN_HOLESIZE);
					Vec3 position = GetPortVec3(pActInfo, IN_POSITION);
					Vec3 orientation = GetPortVec3(pActInfo, IN_ORIENTATION);
					float angle = 0;
					Vec3 dir;
					Matrix34 mat;
					mat.SetRotationXYZ(Ang3(DEG2RAD(orientation.x), DEG2RAD(orientation.y), DEG2RAD(orientation.z)));
					dir = mat.GetColumn1();

					CGameRules* pGameRules = g_pGame->GetGameRules();

					if (pGameRules)
					{
						ExplosionInfo info(0, 0, 0, damage, position, dir, minRadius, radius, minPhysRadius, physRadius, angle, pressure, holeSize, 0);
						info.SetEffect(effect, effectScale, 0.0f);
						info.type = pGameRules->GetHitTypeId("Explosion");
						pGameRules->QueueExplosion(info);
					}
				}
				break;
			}
		}
	}

	virtual void GetMemoryUsage(ICrySizer* s) const
	{
		s->Add(*this);
	}
};


REGISTER_FLOW_NODE("Weapon:AccessoryCheck",		CFlowNode_WeaponCheckAccessory);
REGISTER_FLOW_NODE("Weapon:ZoomCheck",		CFlowNode_WeaponCheckZoom);
REGISTER_FLOW_NODE("Weapon:Accessory",		CFlowNode_WeaponAccessory);
REGISTER_FLOW_NODE("Weapon:Explosion",CFlowNode_Explosion);
