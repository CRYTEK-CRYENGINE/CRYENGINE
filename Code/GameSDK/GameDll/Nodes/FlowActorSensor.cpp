// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "Game.h"
#include "Player.h"
#include "IPlayerEventListener.h"
#include "Weapon.h"
#include "GameCVars.h"
#include "GameRulesModules/IGameRulesKillListener.h"

#include <CryFlowGraph/IFlowBaseNode.h>
#include <IVehicleSystem.h>
#include <CryString/StringUtils.h>
#include <CryAISystem/IVisionMap.h>
#include <CryAISystem/VisionMapTypes.h>

CPlayer* RetrPlayer(EntityId id)
{
	if (id == 0)
		return NULL;
	CActor* pActor = g_pGame ? static_cast<CActor*>(g_pGame->GetIGameFramework()->GetIActorSystem()->GetActor(id)) : NULL;
	if (pActor != NULL && pActor->GetActorClass() == CPlayer::GetActorClassType())
		return static_cast<CPlayer*>(pActor);
	return NULL;
}

IVehicle* GetVehicle(EntityId id)
{
	if (id == 0)
		return NULL;
	return g_pGame ? g_pGame->GetIGameFramework()->GetIVehicleSystem()->GetVehicle(id) : NULL;
}

class CFlowNode_ActorSensor : public CFlowBaseNode<eNCT_Instanced>, public IPlayerEventListener, public IVehicleEventListener, public IGameRulesKillListener
{
public:
	CFlowNode_ActorSensor(SActivationInfo* pActInfo) : m_entityId(0), m_vehicleId(0), m_bEnabled(false)
	{
	}

	~CFlowNode_ActorSensor()
	{
		UnRegisterActor();
	}

	IFlowNodePtr Clone(SActivationInfo* pActInfo)
	{
		return new CFlowNode_ActorSensor(pActInfo);
	}

	void Serialize(SActivationInfo* pActivationInfo, TSerialize ser)
	{
		if (ser.IsReading())
		{
			UnRegisterActor();
		}
		ser.Value("m_entityId", m_entityId, 'eid');
		ser.Value("m_vehicleId", m_vehicleId, 'eid');
		if (ser.IsReading())
		{
			if (m_entityId != 0)
			{
				RegisterActor();
			}
			if (m_vehicleId != 0)
			{
				IVehicle* pVehicle = g_pGame->GetIGameFramework()->GetIVehicleSystem()->GetVehicle(m_vehicleId);
				if (pVehicle)
				{
					pVehicle->RegisterVehicleEventListener(this, "CFlowNode_ActorSensor");
				}
				else
					m_vehicleId = 0;
			}
		}
	}
	enum INS
	{
		EIP_TRIGGER = 0,
		EIP_ENABLE,
		EIP_DISABLE,
	};

	enum OUTS
	{
		EOP_ENTER = 0,
		EOP_EXIT,
		EOP_SEAT,
		EOP_ITEMPICKEDUP,
		EOP_ITEMDROPPED,
		EOP_ITEMUSED,
		EOP_NPCGRABBED,
		EOP_NPCTHROWN,
		EOP_OBJECTGRABBED,
		EOP_OBJECTTHROWN,
		EOP_STANCECHANGED,
		EOP_JUMPED,
		EOP_SPRINTED,
		EOP_SPECIALMOVE,
		EOP_ONDEATH,
		EOP_ONREVIVE,
		EOP_ONENTERSPECATOR,
		EOP_ONLEAVESPECATOR,
		EOP_ONHEALTHCHANGE,
		EOP_ONSTAMINACHANGE,
		EOP_ONTOGGLETHIRDPERSON,
		EOP_MAXHEALTH,
		EOP_ISINWATER,
		EOP_ISHEADUNDERWATER,
		EOP_OXYGENLEVEL
	};

	void GetConfiguration(SFlowNodeConfig& config)
	{
		static const SInputPortConfig in_ports[] =
		{
			InputPortConfig_Void("Get",     _HELP("Trigger \"On*\" outputs according to current state.")),
			InputPortConfig_Void("Enable",  _HELP("Trigger to enable")),
			InputPortConfig_Void("Disable", _HELP("Trigger to enable")),
			{ 0 }
		};
		static const SOutputPortConfig out_ports[] =
		{
			OutputPortConfig<EntityId>("EnterVehicle",       _HELP("Triggered when entering a vehicle")),
			OutputPortConfig<EntityId>("ExitVehicle",        _HELP("Triggered when exiting a vehicle")),
			OutputPortConfig<int>("SeatChange",              _HELP("Triggered when seat has changed")),
			OutputPortConfig<EntityId>("ItemPickedUp",       _HELP("Triggered when an item is picked up")),
			OutputPortConfig<EntityId>("ItemDropped",        _HELP("Triggered when an item is dropped")),
			OutputPortConfig<EntityId>("ItemUsed",           _HELP("Triggered when an item is used")),
			OutputPortConfig<EntityId>("NPCGrabbed",         _HELP("Triggered when an NPC is grabbed")),
			OutputPortConfig<EntityId>("NPCThrown",          _HELP("Triggered when an NPC is thrown")),
			OutputPortConfig<EntityId>("ObjectGrabbed",      _HELP("Triggered when an object is grabbed")),
			OutputPortConfig<EntityId>("ObjectThrown",       _HELP("Triggered when an object is thrown")),
			OutputPortConfig<int>("StanceChanged",           _HELP("Triggered when Stance changed. 0=Stand,1=Crouch,2=Prone,3=Relaxed,4=Stealth,5=Swim,6=ZeroG")),
			OutputPortConfig<int>("Jump",                    _HELP("Triggered On Jump")),
			OutputPortConfig<int>("Sprint",                  _HELP("Triggered on Sprint")),
			OutputPortConfig<int>("SpecialMove",             _HELP("Triggered On SpecialMove. 0=Jump,1=SpeedSprint")),
			OutputPortConfig<int>("OnDeath",                 _HELP("Triggered when Actor dies. Outputs killer's entityId")),
			OutputPortConfig<int>("OnRevive",                _HELP("Triggered when Actor revives. Outputs 0 if not god. 1 if god.")),
			OutputPortConfig<int>("OnEnterSpecator",         _HELP("Triggered when Actor enter specator mode. Outputs spectator mode uint")),
			OutputPortConfig<bool>("OnLeaveSpecator",        _HELP("Triggered when Actor leaves specator mode.")),
			OutputPortConfig<int>("OnHealthChange",          _HELP("Triggered when Actors health changed. Outputs current health.")),
			OutputPortConfig<float>("OnSprintStaminaChange", _HELP("Triggered when Actors sprint stamina changed. Outputs current stamina.")),
			OutputPortConfig<bool>("OnToggleThirdPerson",    _HELP("Triggered when Actors view mode changed. Outputs true for third person otherwise false.")),
			OutputPortConfig<int>("MaxHealth",               _HELP("Triggered when Actors health changed or when get is triggered. Outputs max health.")),
			OutputPortConfig<bool>("IsInWater",              _HELP("Triggered when Actors IsInWater status changed.")),
			OutputPortConfig<bool>("IsHeadUnderwater",       _HELP("Triggered when Actors IsHeadUnderwater status changed.")),
			OutputPortConfig<float>("OxygenLevel",           _HELP("Triggered when Actors oxygen level changes.")),
			{ 0 }
		};
		config.nFlags |= EFLN_TARGET_ENTITY;
		config.pInputPorts = in_ports;
		config.pOutputPorts = out_ports;
		config.sDescription = _HELP("Tracks the attached Entity and its Vehicle-related actions");
		config.SetCategory(EFLN_APPROVED);
	}

	void ProcessEvent(EFlowEvent event, SActivationInfo* pActInfo)
	{
		switch (event)
		{
		case eFE_Initialize:
			m_actInfo = *pActInfo;
			break;
		case eFE_SetEntityId:
			if (m_bEnabled)
			{
				UnRegisterActor();
				m_entityId = pActInfo->pEntity != 0 ? pActInfo->pEntity->GetId() : 0;
				RegisterActor();
			}
			break;
		case eFE_Activate:
			if (IsPortActive(pActInfo, EIP_TRIGGER))
				TriggerPorts(pActInfo);

			if (IsPortActive(pActInfo, EIP_DISABLE))
				UnRegisterActor();

			if (IsPortActive(pActInfo, EIP_ENABLE))
			{
				UnRegisterActor();
				m_entityId = pActInfo->pEntity != 0 ? pActInfo->pEntity->GetId() : 0;
				RegisterActor();
			}
			break;
		}

	}

	void RegisterActor()
	{
		m_bEnabled = true;

		if (m_entityId == 0)
			return;
		CPlayer* pPlayer = RetrPlayer(m_entityId);
		if (pPlayer != 0)
		{
			pPlayer->RegisterPlayerEventListener(this);
			if (g_pGame && g_pGame->GetGameRules())
				g_pGame->GetGameRules()->RegisterKillListener(this);
			return;
		}
		m_entityId = 0;
	}

	void UnRegisterActor()
	{
		m_bEnabled = false;

		if (m_entityId == 0)
			return;

		CPlayer* pPlayer = RetrPlayer(m_entityId);
		if (pPlayer != 0)
			pPlayer->UnregisterPlayerEventListener(this);
		m_entityId = 0;
		if (g_pGame && g_pGame->GetGameRules())
			g_pGame->GetGameRules()->UnRegisterKillListener(this);

		IVehicle* pVehicle = GetVehicle(m_vehicleId);
		if (pVehicle != 0)
			pVehicle->UnregisterVehicleEventListener(this);
		m_vehicleId = 0;
	}

	void TriggerPorts(SActivationInfo* pActInfo)
	{
		EntityId eid = pActInfo->pEntity != 0 ? pActInfo->pEntity->GetId() : 0;
		CPlayer* pPlayer = RetrPlayer(eid);
		if (pPlayer)
		{
			ActivateOutput(pActInfo, EOP_STANCECHANGED, static_cast<int>(pPlayer->GetStance()));

			if (pPlayer->IsDead())
				ActivateOutput(pActInfo, EOP_ONDEATH, pPlayer->IsGod());
			else
				ActivateOutput(pActInfo, EOP_ONREVIVE, pPlayer->IsGod());

			ActivateOutput(pActInfo, EOP_ONTOGGLETHIRDPERSON, pPlayer->IsThirdPerson());

			if (pPlayer->GetSpectatorMode() == CActor::eASM_None)
				ActivateOutput(pActInfo, EOP_ONLEAVESPECATOR, true);
			else
				ActivateOutput(pActInfo, EOP_ONENTERSPECATOR, static_cast<int>(pPlayer->GetSpectatorMode()));

			ActivateOutput(pActInfo, EOP_ONHEALTHCHANGE, (int) pPlayer->GetHealth());
			ActivateOutput(pActInfo, EOP_MAXHEALTH, (int) pPlayer->GetMaxHealth());
		}
	}

	// IPlayerEventListener
	virtual void OnEnterVehicle(IActor* pActor, const char* strVehicleClassName, const char* strSeatName, bool bThirdPerson)
	{
		if (pActor->GetEntityId() != m_entityId)
			return;
		CPlayer* pPlayer = static_cast<CPlayer*>(pActor);
		if (m_vehicleId)
		{
			if (IVehicle* pVehicle = GetVehicle(m_vehicleId))
				pVehicle->UnregisterVehicleEventListener(this);
		}
		IVehicle* pVehicle = pPlayer->GetLinkedVehicle();
		if (pVehicle == 0)
			return;
		pVehicle->RegisterVehicleEventListener(this, "CFlowNode_ActorSensor");
		m_vehicleId = pVehicle->GetEntityId();
		ActivateOutput(&m_actInfo, EOP_ENTER, m_vehicleId);
		IVehicleSeat* pSeat = pVehicle->GetSeatForPassenger(m_entityId);
		if (pSeat)
			ActivateOutput(&m_actInfo, EOP_SEAT, pSeat->GetSeatId());
	}

	virtual void OnExitVehicle(IActor* pActor)
	{
		if (pActor->GetEntityId() != m_entityId)
			return;
		IVehicle* pVehicle = GetVehicle(m_vehicleId);
		if (pVehicle == 0)
		{
			m_vehicleId = 0;
			return;
		}
		ActivateOutput(&m_actInfo, EOP_EXIT, m_vehicleId);
		pVehicle->UnregisterVehicleEventListener(this);
		m_vehicleId = 0;
	}

	virtual void OnToggleThirdPerson(IActor* pActor, bool bThirdPerson)
	{
		ActivateOutput(&m_actInfo, EOP_ONTOGGLETHIRDPERSON, bThirdPerson);
	}

	virtual void OnItemDropped(IActor* pActor, EntityId itemId)
	{
		ActivateOutput(&m_actInfo, EOP_ITEMDROPPED, itemId);
	}
	virtual void OnItemUsed(IActor* pActor, EntityId itemId)
	{
		ActivateOutput(&m_actInfo, EOP_ITEMUSED, itemId);
	}
	virtual void OnItemPickedUp(IActor* pActor, EntityId itemId)
	{
		ActivateOutput(&m_actInfo, EOP_ITEMPICKEDUP, itemId);
	}
	virtual void OnStanceChanged(IActor* pActor, EStance stance)
	{
		ActivateOutput(&m_actInfo, EOP_STANCECHANGED, static_cast<int>(stance));
	}
	virtual void OnSpecialMove(IActor* pActor, ESpecialMove move)
	{
		if (move == IPlayerEventListener::eSM_Jump)
		{
			ActivateOutput(&m_actInfo, EOP_JUMPED, static_cast<int>(move));
		}
		else if (move == IPlayerEventListener::eSM_SpeedSprint)
		{
			ActivateOutput(&m_actInfo, EOP_SPRINTED, static_cast<int>(move));
		}
		else
		{
			ActivateOutput(&m_actInfo, EOP_SPECIALMOVE, static_cast<int>(move));
		}
	}
	virtual void OnDeath(IActor* pActor, bool bIsGod)
	{
	}
	virtual void OnObjectGrabbed(IActor* pActor, bool bIsGrab, EntityId objectId, bool bIsNPC, bool bIsTwoHanded)
	{
		if (bIsGrab)
			ActivateOutput(&m_actInfo, bIsNPC ? EOP_NPCGRABBED : EOP_OBJECTGRABBED, objectId);
		else
			ActivateOutput(&m_actInfo, bIsNPC ? EOP_NPCTHROWN : EOP_OBJECTTHROWN, objectId);
	}
	virtual void OnRevive(IActor* pActor, bool bIsGod)
	{
		ActivateOutput(&m_actInfo, EOP_ONREVIVE, bIsGod);
	}
	virtual void OnSpectatorModeChanged(IActor* pActor, uint8 mode)
	{
		if (mode == CActor::eASM_None)
			ActivateOutput(&m_actInfo, EOP_ONLEAVESPECATOR, true);
		else
			ActivateOutput(&m_actInfo, EOP_ONENTERSPECATOR, static_cast<int>(mode));
	}
	virtual void OnSprintStaminaChanged(IActor* pActor, float fStamina)
	{
		ActivateOutput(&m_actInfo, EOP_ONSTAMINACHANGE, fStamina);
	}
	virtual void OnHealthChanged(IActor* pActor, float fHealth)
	{
		ActivateOutput(&m_actInfo, EOP_ONHEALTHCHANGE, (int) (fHealth));

		CPlayer* pPlayer = (CPlayer*)pActor;
		if (pPlayer)
			ActivateOutput(&m_actInfo, EOP_MAXHEALTH, (int) pPlayer->GetMaxHealth());
	}
	virtual void OnIsInWater(IActor* pActor, bool bIsInWater)
	{
		ActivateOutput(&m_actInfo, EOP_ISINWATER, bIsInWater);
	}
	virtual void OnHeadUnderwater(IActor* pActor, bool bHeadUnderwater)
	{
		ActivateOutput(&m_actInfo, EOP_ISHEADUNDERWATER, bHeadUnderwater);
	}
	virtual void OnOxygenLevelChanged(IActor* pActor, float fNewOxygenLevel)
	{
		ActivateOutput(&m_actInfo, EOP_OXYGENLEVEL, fNewOxygenLevel);
	}

	// ~IPlayerEventListener

	// IVehicleEventListener
	virtual void OnVehicleEvent(EVehicleEvent event, const SVehicleEventParams& params)
	{
		if (event == eVE_VehicleDeleted)
		{
			IVehicle* pVehicle = GetVehicle(m_vehicleId);
			if (pVehicle == 0)
			{
				m_vehicleId = 0;
				return;
			}
			pVehicle->UnregisterVehicleEventListener(this);
			m_vehicleId = 0;
		}
		else if (event == eVE_PassengerChangeSeat)
		{
			if (params.entityId == m_entityId)
			{
				ActivateOutput(&m_actInfo, EOP_SEAT, params.iParam); // seat id
			}
		}
	}
	// ~IVehicleEventListener

	// TODO: we use this instead of just OnDeath() because when that is called, pPlayer->m_stats.recentKiller still does not have the right value.
	// if that is fixed, we can remove all the GameRulesKillListener code from this node and just use OnDeath() instead. It would be slightly more efficient too.
	// IGameRulesKillListener
	virtual void OnEntityKilledEarly(const HitInfo& hitInfo)
	{
		if (hitInfo.targetId == m_entityId)
			ActivateOutput(&m_actInfo, EOP_ONDEATH, hitInfo.shooterId);
	}
	virtual void OnEntityKilled(const HitInfo& hitInfo) {};
	// ~IGameRulesKillListener

	virtual void GetMemoryUsage(ICrySizer* s) const
	{
		s->Add(*this);
	}

protected:
	EntityId        m_entityId;
	EntityId        m_vehicleId;
	SActivationInfo m_actInfo;
	bool            m_bEnabled;
};

class CFlowNode_WeaponSensor
	: public CFlowBaseNode<eNCT_Instanced>
	  , public IWeaponEventListener
	  , public IItemSystemListener
	  , public ISystemEventListener
	  , public IInventoryListener
{
public:
	CFlowNode_WeaponSensor(SActivationInfo* pActInfo)
		: m_entityId(0)
		, m_bShot(false)
		, m_bEnabled(false)
	{
	}

	~CFlowNode_WeaponSensor()
	{
		Disable();
	}

	enum INS
	{
		EIP_TRIGGER = 0,
		EIP_ENABLE,
		EIP_DISABLE,
	};

	enum OUTS
	{
		EOP_ONWEAPONCHANGE = 0,
		EOP_ONFIREMODECHANGE,
		EOP_ONSHOOT,
		EOP_ONZOOM,
		EOP_ONRELOAD,
		EOP_ONOUTOFAMMO,
		EOP_ONSTARTFIRE,
		EOP_ONSTOPFIRE,
		EOP_ONAMMOPOOLCHANGE,

		EOP_WEAPONID,
		EOP_WEAPONNAME,
		EOP_FIREMODENAME,
		EOP_ISMELEE,

		EOP_AMMONAME,
		EOP_AMMOCOUNT,
		EOP_CLIPSIZE,
		EOP_AMMOPOOL,

		EOP_SPREAD,

		EOP_ZOOMED,
		EOP_ZOOMMODE,
		EOP_ZOOMNAME,
		EOP_ZOOMSTEP,
		EOP_ZOOMMAXSTEP,
	};

	void GetConfiguration(SFlowNodeConfig& config)
	{
		static const SInputPortConfig inputs[] = {
			InputPortConfig_Void("Get",     _HELP("Force node to trigger current state")),
			InputPortConfig_Void("Enable",  _HELP("Trigger to enable")),
			InputPortConfig_Void("Disable", _HELP("Trigger to enable")),
			{ 0 }
		};
		static const SOutputPortConfig outputs[] = {
			OutputPortConfig_Void("OnWeaponChange",    _HELP("Triggers if weapon is changed")),
			OutputPortConfig_Void("OnFiremodeChange",  _HELP("Triggers if firemode is changed")),
			OutputPortConfig_Void("OnShoot",           _HELP("Triggers if weapon is shot")),
			OutputPortConfig_Void("OnZoom",            _HELP("Triggers if weapon zoom is changed")),
			OutputPortConfig_Void("OnReloaded",        _HELP("Triggers if weapon was reloaded")),
			OutputPortConfig_Void("OnOutOfAmmo",       _HELP("Triggers if current ammo clip is emptied")),
			OutputPortConfig_Void("OnStartFire",       _HELP("Triggers on start fire")),
			OutputPortConfig_Void("OnStopFire",        _HELP("Triggers on stop fire")),
			OutputPortConfig_Void("OnAmmoPoolChanged", _HELP("Triggers on ammo pool changes")),

			OutputPortConfig<EntityId>("WeaponId",     _HELP("Weapon EntityID")),
			OutputPortConfig<string>("WeaponName",     _HELP("Weapon name")),
			OutputPortConfig<string>("FireModeName",   _HELP("Name of the current firemode")),
			OutputPortConfig<bool>("IsMelee",          _HELP("Is melee weapon")),

			OutputPortConfig<string>("AmmoName",       _HELP("Name of currently used ammo")),
			OutputPortConfig<int>("AmmoCount",         _HELP("Current ammo remaining in weapon (including chambered rounds)")),
			OutputPortConfig<int>("ClipSize",          _HELP("Amount of ammo the weapon clip can hold")),
			OutputPortConfig<int>("AmmoPool",          _HELP("Current amount of ammo remaining in player inventory")),

			OutputPortConfig<float>("Spread",          _HELP("Spread of current weapon")),

			OutputPortConfig<bool>("Zoomed",           _HELP("Is weapon zoomed")),
			OutputPortConfig<int>("ZoomMode",          _HELP("Weapon zoom mode number, as ordered in weapon script")),
			OutputPortConfig<string>("ZoomName",       _HELP("Weapon zoom mode name")),
			OutputPortConfig<int>("CurrZoomStep",      _HELP("Current zoom step, defined as Stages in ZoomMode params")),
			OutputPortConfig<int>("MaxZoomStep",       _HELP("Maximum zoom steps available")),
			{ 0 }
		};

		config.nFlags |= EFLN_TARGET_ENTITY;
		config.pInputPorts = inputs;
		config.pOutputPorts = outputs;
		config.sDescription = _HELP("Tracks the attached Entity and its Vehicle-related actions");
		config.SetCategory(EFLN_APPROVED);
	}

	void ProcessEvent(EFlowEvent event, SActivationInfo* pActInfo)
	{
		switch (event)
		{
		case eFE_Initialize:
			m_actInfo = *pActInfo;
			UnRegisterWeapon();
			UnRegisterItemSystem();
			m_bShot = false;
			break;
		case eFE_SetEntityId:
			if (m_bEnabled)
			{
				Enable(pActInfo);
			}
			break;
		case eFE_Activate:
			if (IsPortActive(pActInfo, EIP_TRIGGER))
			{
				CPlayer* pPlayer = RetrPlayer(pActInfo->pEntity != 0 ? pActInfo->pEntity->GetId() : 0);
				UpdateWeaponChange(pPlayer ? pPlayer->GetWeapon(pPlayer->GetCurrentItemId()) : NULL);
				ActivateOutput(&m_actInfo, EOP_ONWEAPONCHANGE, true);
				ActivateOutput(&m_actInfo, EOP_ONFIREMODECHANGE, true);
			}

			if (IsPortActive(pActInfo, EIP_DISABLE))
			{
				Disable();
			}

			if (IsPortActive(pActInfo, EIP_ENABLE))
			{
				Enable(pActInfo);
			}
			break;
		}
	}

	virtual void GetMemoryUsage(ICrySizer* s) const
	{
		s->Add(*this);
	}

	IFlowNodePtr Clone(SActivationInfo* pActInfo)
	{
		return new CFlowNode_WeaponSensor(pActInfo);
	}

	// ISystemEventListener
	virtual void OnSystemEvent(ESystemEvent event, UINT_PTR wparam, UINT_PTR lparam)
	{
		if (event == ESYSTEM_EVENT_LEVEL_UNLOAD)
			Disable();
	}
	// ~ISystemEventListener

	// IItemSystemListener
	virtual void OnSetActorItem(IActor* pActor, IItem* pItem)
	{
		CPlayer* pPlayer = RetrPlayer(m_entityId);
		if (pPlayer == pActor)
		{
			UnRegisterWeapon();
			if (pItem)
				RegisterWeapon(pItem->GetEntityId());

			CWeapon* pWeapon = GetCurrentWeapon();
			UpdateWeaponChange(pWeapon);
			ActivateOutput(&m_actInfo, EOP_ONWEAPONCHANGE, true);
		}
	}

	virtual void OnDropActorItem(IActor* pActor, IItem* pItem)      {}
	virtual void OnSetActorAccessory(IActor* pActor, IItem* pItem)  {}
	virtual void OnDropActorAccessory(IActor* pActor, IItem* pItem) {}
	// ~IItemSystemListener

	// ~IInventoryListener
	virtual void OnSetAmmoCount(IEntityClass* pAmmoType, int count)
	{
		CWeapon* pWeapon = GetCurrentWeapon();
		IFireMode* pFireMode = pWeapon ? pWeapon->GetFireMode(pWeapon->GetCurrentFireMode()) : NULL;
		if (pFireMode && pAmmoType == pFireMode->GetAmmoType())
		{
			ActivateOutput(&m_actInfo, EOP_AMMOPOOL, pWeapon->GetInventoryAmmoCount(pFireMode->GetAmmoType()));
			ActivateOutput(&m_actInfo, EOP_ONRELOAD, true);
			ActivateOutput(&m_actInfo, EOP_ONAMMOPOOLCHANGE, true);
		}
	}

	virtual void OnAddItem(EntityId entityId)                  {}
	virtual void OnAddAccessory(IEntityClass* pAccessoryClass) {}
	virtual void OnClearInventory()                            {}
	// ~IInventoryListener

	// IWeaponEventListener
	virtual void OnStartFire(IWeapon* pWeapon, EntityId shooterId)
	{
		ActivateOutput(&m_actInfo, EOP_ONSTARTFIRE, true);
		ActivateOutput(&m_actInfo, EOP_SPREAD, 1.f);
	}

	virtual void OnStopFire(IWeapon* pWeapon, EntityId shooterId)
	{
		if (!m_bShot) return;

		m_bShot = false;
		ActivateOutput(&m_actInfo, EOP_ONSTOPFIRE, true);
		ActivateOutput(&m_actInfo, EOP_SPREAD, 1.f);
	}
	virtual void OnStartReload(IWeapon* pWeapon, EntityId shooterId, IEntityClass* pAmmoType) {}
	virtual void OnSetAmmoCount(IWeapon* pWeapon, EntityId shooterId)                         {}
	virtual void OnReadyToFire(IWeapon* pWeapon)                                              {}
	virtual void OnPickedUp(IWeapon* pWeapon, EntityId actorId, bool destroyed)               {}
	virtual void OnDropped(IWeapon* pWeapon, EntityId actorId)                                {}
	virtual void OnMelee(IWeapon* pWeapon, EntityId shooterId)                                {}
	virtual void OnSelected(IWeapon* pWeapon, bool selected)                                  {}
	virtual void OnStartTargetting(IWeapon* pWeapon)                                          {}
	virtual void OnStopTargetting(IWeapon* pWeapon)                                           {}
	virtual void OnEndBurst(IWeapon* pWeapon, EntityId shooterId)                             {};

	virtual void OnEndReload(IWeapon* pWeapon, EntityId shooterId, IEntityClass* pAmmoType)
	{
		IFireMode* pFireMode = pWeapon ? pWeapon->GetFireMode(pWeapon->GetCurrentFireMode()) : NULL;
		ActivateOutput(&m_actInfo, EOP_AMMOCOUNT, pFireMode ? pFireMode->GetAmmoCount() : 0);
		ActivateOutput(&m_actInfo, EOP_AMMOPOOL, pFireMode && pWeapon ? pWeapon->GetInventoryAmmoCount(pFireMode->GetAmmoType()) : 0);
		ActivateOutput(&m_actInfo, EOP_ONRELOAD, true);
		ActivateOutput(&m_actInfo, EOP_ONAMMOPOOLCHANGE, true);
	}

	virtual void OnShoot(IWeapon* pWeapon, EntityId shooterId, EntityId ammoId, IEntityClass* pAmmoType,
	                     const Vec3& pos, const Vec3& dir, const Vec3& vel)
	{
		m_bShot = true;
		IFireMode* pFireMode = pWeapon ? pWeapon->GetFireMode(pWeapon->GetCurrentFireMode()) : NULL;
		ActivateOutput(&m_actInfo, EOP_AMMOCOUNT, pFireMode ? pFireMode->GetAmmoCount() : 0);
		ActivateOutput(&m_actInfo, EOP_ONSHOOT, true);
		ActivateOutput(&m_actInfo, EOP_SPREAD, pFireMode ? pFireMode->GetSpreadForHUD() : 15.f);
	}

	virtual void OnFireModeChanged(IWeapon* pWeapon, int currentFireMode)
	{
		UpdateWeaponChange(GetCurrentWeapon());
		ActivateOutput(&m_actInfo, EOP_ONFIREMODECHANGE, true);
	}

	virtual void OnOutOfAmmo(IWeapon* pWeapon, IEntityClass* pAmmoType)
	{
		ActivateOutput(&m_actInfo, EOP_ONOUTOFAMMO, true);
	}

	virtual void OnZoomChanged(IWeapon* pWeapon, bool zoomed, int idx)
	{
		UpdateWeaponChange(GetCurrentWeapon());
		ActivateOutput(&m_actInfo, EOP_ONZOOM, true);
	}

	// ~IWeaponEventListener

private:
	void UpdateWeaponChange(CWeapon* pWeapon)
	{
		int iFireMode = pWeapon ? pWeapon->GetCurrentFireMode() : -1;
		IFireMode* pFireMode = pWeapon ? pWeapon->GetFireMode(iFireMode) : NULL;
		int iZoomMode = pWeapon ? pWeapon->GetCurrentZoomMode() : -1;
		IZoomMode* pZoomMode = pWeapon ? pWeapon->GetZoomMode(iZoomMode) : NULL;
		IEntity* pWeaponEntity = pWeapon ? pWeapon->GetEntity() : NULL;

		ActivateOutput(&m_actInfo, EOP_WEAPONID, pWeaponEntity ? pWeaponEntity->GetId() : 0);
		ActivateOutput(&m_actInfo, EOP_WEAPONNAME, pWeaponEntity ? string(pWeaponEntity->GetClass()->GetName()) : string(""));
		ActivateOutput(&m_actInfo, EOP_FIREMODENAME, pFireMode ? string(pFireMode->GetName()) : string(""));
		ActivateOutput(&m_actInfo, EOP_ISMELEE, pFireMode ? pFireMode->GetClipSize() == 0 ? true : false : true);

		ActivateOutput(&m_actInfo, EOP_AMMONAME, pFireMode ? string(pFireMode->GetAmmoType()->GetName()) : string(""));
		ActivateOutput(&m_actInfo, EOP_AMMOCOUNT, pFireMode ? pFireMode->GetAmmoCount() : 0);
		ActivateOutput(&m_actInfo, EOP_CLIPSIZE, pFireMode ? pFireMode->GetClipSize() : 0);
		ActivateOutput(&m_actInfo, EOP_AMMOPOOL, pFireMode && pWeapon ? pWeapon->GetInventoryAmmoCount(pFireMode->GetAmmoType()) : 0);

		ActivateOutput(&m_actInfo, EOP_ZOOMED, pWeapon ? pWeapon->IsZoomed() : false);
		ActivateOutput(&m_actInfo, EOP_ZOOMMODE, iZoomMode);
		ActivateOutput(&m_actInfo, EOP_ZOOMNAME, pWeapon ? string(pWeapon->GetZoomModeName(iZoomMode)) : string(""));
		ActivateOutput(&m_actInfo, EOP_ZOOMSTEP, pZoomMode ? pZoomMode->GetCurrentStep() : 0);
		ActivateOutput(&m_actInfo, EOP_ZOOMMAXSTEP, pZoomMode ? pZoomMode->GetMaxZoomSteps() : 0);
	}

	void Enable(SActivationInfo* pActInfo)
	{
		Disable();
		m_entityId = pActInfo->pEntity != 0 ? pActInfo->pEntity->GetId() : 0;
		CPlayer* pPlayer = RetrPlayer(m_entityId);
		EntityId currItemId = pPlayer ? pPlayer->GetCurrentItemId() : 0;
		RegisterItemSystem();
		RegisterWeapon(currItemId);
		m_bEnabled = true;

		gEnv->pSystem->GetISystemEventDispatcher()->RegisterListener(this, "CFlowNode_WeaponSensor");
	}

	void Disable()
	{
		UnRegisterWeapon();
		UnRegisterItemSystem();
		m_entityId = 0;
		m_bEnabled = false;

		gEnv->pSystem->GetISystemEventDispatcher()->RemoveListener(this);
	}

	void RegisterWeapon(EntityId itemId)
	{
		m_currentWeaponId = 0;
		CPlayer* pPlayer = RetrPlayer(m_entityId);
		if (pPlayer)
		{
			CWeapon* pCurrentWeapon = pPlayer->GetWeapon(itemId);

			if (pCurrentWeapon)
			{
				m_currentWeaponId = pCurrentWeapon->GetEntityId();
				pCurrentWeapon->AddEventListener(this, "CFlowNode_WeaponSensor");
			}
			pPlayer->GetInventory()->AddListener(this);
		}
	}

	void UnRegisterWeapon()
	{
		CPlayer* pPlayer = RetrPlayer(m_entityId);
		if (pPlayer)
		{
			CWeapon* pOldWeapon = m_currentWeaponId != 0 ? pPlayer->GetWeapon(m_currentWeaponId) : NULL;
			if (pOldWeapon)
			{
				pOldWeapon->RemoveEventListener(this);
			}
			pPlayer->GetInventory()->RemoveListener(this);
		}
		m_currentWeaponId = 0;
	}

	void RegisterItemSystem()
	{
		IItemSystem* pItemSys = g_pGame ? g_pGame->GetIGameFramework()->GetIItemSystem() : NULL;
		if (pItemSys)
			pItemSys->RegisterListener(this);
	}

	void UnRegisterItemSystem()
	{
		IItemSystem* pItemSys = g_pGame ? g_pGame->GetIGameFramework()->GetIItemSystem() : NULL;
		if (pItemSys)
			pItemSys->UnregisterListener(this);
	}

	CWeapon* GetCurrentWeapon()
	{
		CPlayer* pPlayer = RetrPlayer(m_entityId);
		return pPlayer ? pPlayer->GetWeapon(m_currentWeaponId) : NULL;
	}

private:
	EntityId        m_entityId;
	EntityId        m_currentWeaponId;
	SActivationInfo m_actInfo;
	bool            m_bEnabled;
	bool            m_bShot;
};

class CFlowNode_DifficultyLevel : public CFlowBaseNode<eNCT_Singleton>
{
	enum INPUTS
	{
		EIP_Trigger = 0,
	};

	enum OUTPUTS
	{
		EOP_Easy = 0,
		EOP_Medium,
		EOP_Hard,
		EOP_Delta,
		EOP_SuperSoldier,
		EOP_LAST,
	};

public:
	CFlowNode_DifficultyLevel(SActivationInfo* pActInfo) {}

	void GetConfiguration(SFlowNodeConfig& config)
	{
		static const SInputPortConfig in_ports[] =
		{
			InputPortConfig_Void("Trigger", _HELP("Trigger to get difficulty level.")),
			{ 0 }
		};
		static const SOutputPortConfig out_ports[] =
		{
			OutputPortConfig_Void("Easy",             _HELP("Easy")),
			OutputPortConfig_Void("Normal",           _HELP("Normal")),
			OutputPortConfig_Void("Hard",             _HELP("Hard")),
			OutputPortConfig_Void("Delta",            _HELP("SuperSoldier")),
			OutputPortConfig_Void("PostHumanWarrior", _HELP("PostHumanWarrior")),
			{ 0 }
		};
		config.pInputPorts = in_ports;
		config.pOutputPorts = out_ports;
		config.sDescription = _HELP("Get difficulty level.");
		config.SetCategory(EFLN_APPROVED);
	}

	virtual void GetMemoryUsage(ICrySizer* s) const
	{
		s->Add(*this);
	}

	void ProcessEvent(EFlowEvent event, SActivationInfo* pActInfo)
	{
		if (eFE_Activate == event && IsPortActive(pActInfo, EIP_Trigger))
		{
			const int level = g_pGameCVars->g_difficultyLevel - 1; // [0] or 1-4
			if (!(level < EOP_Easy) && level < EOP_LAST)
			{
				ActivateOutput(pActInfo, level, true);
			}
		}
	}
};

// THIS FLOWNODE IS A SINGLETON, although it has member variables!
// THIS IS INTENDED!!!!
class CFlowNode_OverrideFOV : public CFlowBaseNode<eNCT_Singleton>
{
	enum INPUTS
	{
		EIP_SetFOV = 0,
		EIP_GetFOV,
		EIP_ResetFOV
	};

	enum OUTPUTS
	{
		EOP_CurFOV = 0,
		EOP_ResetDone,
	};

public:
	CFlowNode_OverrideFOV(SActivationInfo* pActInfo) { m_storedFOV = 0.0f; }

	~CFlowNode_OverrideFOV()
	{
		if (m_storedFOV > 0.0f)
		{
			ICVar* pCVar = (gEnv && gEnv->pConsole) ? gEnv->pConsole->GetCVar("cl_fov") : 0;
			if (pCVar)
				pCVar->Set(m_storedFOV);
		}
	}

	void GetConfiguration(SFlowNodeConfig& config)
	{
		static const SInputPortConfig in_ports[] =
		{
			InputPortConfig<float>("SetFOV", _HELP("Trigger to override Camera's FieldOfView.")),
			InputPortConfig_Void("GetFOV",   _HELP("Trigger to get current Camera's FieldOfView.")),
			InputPortConfig_Void("ResetFOV", _HELP("Trigger to reset FieldOfView to default value.")),
			{ 0 }
		};
		static const SOutputPortConfig out_ports[] =
		{
			OutputPortConfig<float>("CurFOV",  _HELP("Current FieldOfView")),
			OutputPortConfig_Void("ResetDone", _HELP("Triggered after Reset")),
			{ 0 }
		};
		config.pInputPorts = in_ports;
		config.pOutputPorts = out_ports;
		config.sDescription = _HELP("Override Camera's FieldOfView. Cutscene ONLY!");
		config.SetCategory(EFLN_ADVANCED);
	}

	virtual void GetMemoryUsage(ICrySizer* s) const
	{
		s->Add(*this);
	}

	void Serialize(SActivationInfo* pActInfo, TSerialize ser)
	{
		ICVar* pCVar = gEnv->pConsole->GetCVar("cl_fov");

		if (ser.IsWriting())
		{
			float curFOV = 0.0f;

			// in case we're currently active, store current value as well
			if (m_storedFOV > 0.0f && pCVar)
				curFOV = pCVar->GetFVal();

			ser.Value("m_storedFOV", m_storedFOV);
			ser.Value("curFOV", curFOV);
		}
		else
		{
			float storedFOV = 0.0f;
			float curFOV = 0.0f;
			ser.Value("m_storedFOV", storedFOV);
			ser.Value("curFOV", curFOV);

			// if we're currently active, restore first
			if (m_storedFOV > 0.0f)
			{
				if (pCVar)
					pCVar->Set(m_storedFOV);
				m_storedFOV = 0.0f;
			}

			m_storedFOV = storedFOV;
			if (m_storedFOV > 0.0f && curFOV > 0.0f && pCVar)
				pCVar->Set(curFOV);
		}
	}

	void ProcessEvent(EFlowEvent event, SActivationInfo* pActInfo)
	{
		ICVar* pCVar = gEnv->pConsole->GetCVar("cl_fov");
		if (pCVar == 0)
			return;

		switch (event)
		{
		case eFE_Initialize:
			if (m_storedFOV > 0.0f)
			{
				pCVar->Set(m_storedFOV);
				m_storedFOV = 0.0f;
			}
			break;

		case eFE_Activate:
			// get fov
			if (IsPortActive(pActInfo, EIP_GetFOV))
				ActivateOutput(pActInfo, EOP_CurFOV, pCVar->GetFVal());

			// set fov (store backup if not already done)
			if (IsPortActive(pActInfo, EIP_SetFOV))
			{
				if (m_storedFOV <= 0.0f)
					m_storedFOV = pCVar->GetFVal();
				pCVar->Set(GetPortFloat(pActInfo, EIP_SetFOV));
			}
			if (IsPortActive(pActInfo, EIP_ResetFOV))
			{
				if (m_storedFOV > 0.0f)
				{
					pCVar->Set(m_storedFOV);
					m_storedFOV = 0.0f;
				}
			}
			break;
		}
	}

	float m_storedFOV;
};

//////////////////////////////////////////////////////////////////////////

class CFlowNode_ActorVisualDetector : public CFlowBaseNode<eNCT_Instanced>, public IEntityEventListener
{
public:
	CFlowNode_ActorVisualDetector(SActivationInfo* pActInfo)
		: m_enabled(false)
		, m_entitiesOnView(0)
		, m_observerEntityId(0)
	{
		m_actInfo = *pActInfo;
	}

	~CFlowNode_ActorVisualDetector()
	{
		Disable();
	}

	enum EInPorts
	{
		INP_ENABLE = 0,
		INP_DISABLE,
		INP_OBSERVER_FOV,
		INP_SIGHT_RANGE,
		INP_ENTITY_TO_LOOK_FOR,
	};

	enum EOutPorts
	{
		OUT_SEEN_ENTITY,
		OUT_NOT_SEEN,
	};

	void GetMemoryUsage(ICrySizer* s) const
	{
		s->Add(*this);
	}

	IFlowNodePtr Clone(SActivationInfo* pActInfo)
	{
		return new CFlowNode_ActorVisualDetector(pActInfo);
	}

	void Serialize(SActivationInfo* pActInfo, TSerialize ser)
	{
		ser.Value("enabled", m_enabled);   // no need to call Disable() first when reading, because eFE_Initialize is always triggered before reading.

		if (ser.IsReading())
		{
			m_actInfo = *pActInfo;
			if (m_enabled)
				Enable();
		}
	}

	void GetConfiguration(SFlowNodeConfig& config)
	{
		static const SInputPortConfig in_config[] =
		{
			InputPortConfig_Void("enable",               _HELP("Trigger to enable the visual detection")),
			InputPortConfig_Void("disable",              _HELP("Trigger to disable the visual detection")),
			InputPortConfig<float>("observerFOV",        20.f,                                             _HELP("Field of View, In degrees")),
			InputPortConfig<float>("sightRange",         50.f,                                             _HELP("Max sight range")),
			InputPortConfig<EntityId>("entityToLookFor", 0,                                                _HELP("If not specified, it looks for any actor")),
			{ 0 }
		};
		static const SOutputPortConfig out_config[] =
		{
			OutputPortConfig<EntityId>("seenEntity",     _HELP("Id of the entity that has been detected.")),
			OutputPortConfig<SFlowSystemVoid>("notSeen", _HELP("Triggered when no entity is visible anymore.")),
			{ 0 }
		};
		config.nFlags |= EFLN_TARGET_ENTITY;
		config.pInputPorts = in_config;
		config.pOutputPorts = out_config;
		config.SetCategory(EFLN_APPROVED);
		config.sDescription = _HELP("Sets a visibility detector for actors\nThe Node's entity is only used to provide position and orientation, it can be any entity type.");
	}

	///...................................................................
	void ProcessEvent(EFlowEvent event, SActivationInfo* pActInfo)
	{
		switch (event)
		{
		case eFE_Initialize:
			m_actInfo = *pActInfo;
			Disable();
			break;

		case eFE_Activate:
			{
				if (IsPortActive(&m_actInfo, INP_ENABLE))
				{
					Enable();
				}
				if (IsPortActive(&m_actInfo, INP_DISABLE))
				{
					Disable();
				}
			}
			break;
		}
	}

	///...................................................................
	void Enable()
	{
		Disable();
		IEntity* pObserverEntity = m_actInfo.pEntity;
		if (!pObserverEntity)
			return;

		m_observerEntityId = pObserverEntity->GetId();
		stack_string visionIdName = pObserverEntity->GetName();
		visionIdName += ".Vision";
		IVisionMap* pVisionMap = gEnv->pAISystem->GetVisionMap();
		m_visionId = pVisionMap->CreateVisionID(visionIdName);

		ObserverParams observerParams;
		observerParams.factionsToObserveMask = ~0u;
		observerParams.entityId = m_observerEntityId;

		const float fovCos = cosf(DEG2RAD(GetPortFloat(&m_actInfo, INP_OBSERVER_FOV)));
		observerParams.fovCos = fovCos;

		observerParams.callback = functor(*this, &CFlowNode_ActorVisualDetector::CallBackViewChanged);

		observerParams.typeMask = Player;
		EntityId entityToLookFor = GetPortEntityId(&m_actInfo, INP_ENTITY_TO_LOOK_FOR);
		if (entityToLookFor != gEnv->pGameFramework->GetClientActor()->GetEntityId())
			observerParams.typeMask |= AliveAgent | General;

		observerParams.skipList[0] = pObserverEntity->GetPhysics();
		observerParams.skipListSize = pObserverEntity->GetPhysics() ? 1 : 0;

		{
			IEntity* pEntityParent = pObserverEntity->GetParent();
			while (pEntityParent && observerParams.skipListSize < ObserverParams::MaxSkipListSize)
			{
				IPhysicalEntity* pPhysics = pEntityParent->GetPhysics();
				if (pPhysics)
				{
					observerParams.skipList[observerParams.skipListSize] = pPhysics;
					observerParams.skipListSize++;
				}
				pEntityParent = pEntityParent->GetParent();
			}
		}

		observerParams.eyePosition = pObserverEntity->GetWorldPos();
		observerParams.eyeDirection = pObserverEntity->GetForwardDir().GetNormalized();
		observerParams.sightRange = GetPortFloat(&m_actInfo, INP_SIGHT_RANGE);

		m_entitiesOnView = 0;
		m_enabled = true;
		pVisionMap->RegisterObserver(m_visionId, observerParams);
		gEnv->pEntitySystem->AddEntityEventListener(m_observerEntityId, ENTITY_EVENT_XFORM, this);
	}

	///...................................................................
	void Disable()
	{
		if (m_enabled)
		{
			m_enabled = false;

			if (gEnv->pAISystem)
				gEnv->pAISystem->GetVisionMap()->UnregisterObserver(m_visionId);

			if (gEnv->pEntitySystem)
				gEnv->pEntitySystem->RemoveEntityEventListener(m_observerEntityId, ENTITY_EVENT_XFORM, this);

			m_visionId = VisionID();
		}
	}

	///.......callback from the VisionMap............................................................
	void CallBackViewChanged(const VisionID& observerID, const ObserverParams& observerParams, const VisionID& observableID, const ObservableParams& observableParams, bool visible)
	{
		if (!m_enabled)
			return;

		EntityId entityToLookFor = GetPortEntityId(&m_actInfo, INP_ENTITY_TO_LOOK_FOR);
		if (entityToLookFor == 0 || entityToLookFor == observableParams.entityId)
		{
			if (visible)
			{
				ActivateOutput(&m_actInfo, OUT_SEEN_ENTITY, observableParams.entityId);
				m_entitiesOnView++;
			}
			else
			{
				m_entitiesOnView--;
				if (entityToLookFor != 0 || m_entitiesOnView == 0)  // the first check should not be needed...but i dont trust the visionmap!
					ActivateOutput(&m_actInfo, OUT_NOT_SEEN, true);
			}
		}
	}

	//////////////////////////////////////////////////////////////////////////
	// IEntityEventListener
	virtual void OnEntityEvent(IEntity* pEntity, const SEntityEvent& event)
	{
		if (m_enabled && event.event == ENTITY_EVENT_XFORM)
		{
			if (m_observerEntityId == pEntity->GetId())
			{
				ObserverParams observerParams;
				observerParams.eyePosition = pEntity->GetWorldPos();
				observerParams.eyeDirection = pEntity->GetForwardDir().GetNormalized();
				gEnv->pAISystem->GetVisionMap()->ObserverChanged(m_visionId, observerParams, eChangedPosition | eChangedOrientation);
			}
		}
	}

private:
	bool            m_enabled;
	uint32          m_entitiesOnView;
	VisionID        m_visionId;
	EntityId        m_observerEntityId;
	SActivationInfo m_actInfo;
};

//////////////////////////////////////////////////////////////////////////

class CFlowNode_PlayerOnPickUpAmmo : public CFlowBaseNode<eNCT_Instanced>, public IPlayerEventListener
{
	enum
	{
		INP_ENABLE = 0,
		INP_DISABLE,
		INP_AMMOTYPE
	};
	enum
	{
		OUT_PICKEDUP = 0
	};

public:
	CFlowNode_PlayerOnPickUpAmmo(SActivationInfo* pActInfo)
		: m_actInfo(*pActInfo)
		, m_enabled(false)
	{
	}

	~CFlowNode_PlayerOnPickUpAmmo()
	{
		Disable();
	}

	IFlowNodePtr Clone(SActivationInfo* pActInfo)
	{
		return new CFlowNode_PlayerOnPickUpAmmo(pActInfo);
	}

	void Serialize(SActivationInfo* pActivationInfo, TSerialize ser)
	{
		ser.Value("enabled", m_enabled);
		if (ser.IsReading() && m_enabled)
		{
			Enable();
		}
	}

	void GetConfiguration(SFlowNodeConfig& config)
	{
		static const SInputPortConfig in_ports[] =
		{
			InputPortConfig_Void("enable",  _HELP("Trigger to enable the node")),
			InputPortConfig_Void("disable", _HELP("Trigger to disable the node")),
			InputPortConfig<string>("Ammo", _HELP("The node will only react to this type of ammo. Empty = any ammo. (only actual pickable ammos such as arrows will work with this node, however)"),_HELP("Ammo"),  _UICONFIG("enum_global:ammos")),
			{ 0 }
		};

		static const SOutputPortConfig out_ports[] =
		{
			OutputPortConfig_AnyType("picked", _HELP("triggered when the specified type of ammo is pickedup")),
			{ 0 }
		};

		config.sDescription = _HELP("triggers when the especified ammo is picked up by the local player (picking up is NOT just taking from ammoboxes or similar.) ");
		config.pInputPorts = in_ports;
		config.pOutputPorts = out_ports;
		config.SetCategory(EFLN_APPROVED);
	}

	void ProcessEvent(EFlowEvent event, SActivationInfo* pActInfo)
	{
		m_actInfo = *pActInfo;
		switch (event)
		{
		case eFE_Initialize:
			Disable();
			break;

		case eFE_Activate:
			{
				if (IsPortActive(&m_actInfo, INP_ENABLE))
				{
					Enable();
				}
				if (IsPortActive(&m_actInfo, INP_DISABLE))
				{
					Disable();
				}
				break;
			}
		}
	}

	void Enable()
	{
		CPlayer* pPlayer = GetLocalPlayer();
		if (pPlayer)
		{
			pPlayer->RegisterPlayerEventListener(this);
			m_enabled = true;
		}
	}

	void Disable()
	{
		CPlayer* pPlayer = GetLocalPlayer();
		if (pPlayer)
		{
			pPlayer->UnregisterPlayerEventListener(this);
			m_enabled = false;
		}
	}

	CPlayer* GetLocalPlayer()
	{
		CActor* pLocalActor = static_cast<CActor*>(g_pGame->GetIGameFramework()->GetClientActor());
		if (pLocalActor && pLocalActor->IsPlayer())
			return static_cast<CPlayer*>(pLocalActor);
		else
			return NULL;
	}

	// IPlayerEventListener
	void OnPickedUpPickableAmmo(IActor* pActor, IEntityClass* pAmmoType, int count)
	{
		if (m_enabled)
		{
			const string& ammoClassName = GetPortString(&m_actInfo, INP_AMMOTYPE);
			if (ammoClassName.empty() || ammoClassName == pAmmoType->GetName())
			{
				ActivateOutput(&m_actInfo, OUT_PICKEDUP, true);
			}
		}
	}
	// IPlayerEventListener

	virtual void GetMemoryUsage(ICrySizer* s) const
	{
		s->Add(*this);
	}
	SActivationInfo m_actInfo;
	bool            m_enabled;
};

//////////////////////////////////////////////////////////////////////////

class CFlowNode_ActorOnDeath : public CFlowBaseNode<eNCT_Instanced>, public IGameRulesKillListener
{
	enum
	{
		INP_ENABLE = 0,
		INP_DISABLE,
		INP_AMMOTYPE
	};
	enum
	{
		OUT_VICTIM = 0,
		OUT_KILLER
	};

public:
	CFlowNode_ActorOnDeath(SActivationInfo* pActInfo)
		: m_actInfo(*pActInfo)
		, m_enabled(false)
		, m_ammoClassId(0)
	{
		m_pendingDeaths.reserve(5);
	}

	~CFlowNode_ActorOnDeath()
	{
		Disable();
	}

	IFlowNodePtr Clone(SActivationInfo* pActInfo)
	{
		return new CFlowNode_ActorOnDeath(pActInfo);
	}

	void Serialize(SActivationInfo* pActivationInfo, TSerialize ser)
	{
		ser.Value("enabled", m_enabled);
		ser.Value("pendingDeaths", m_pendingDeaths);
		if (ser.IsReading() && m_enabled)
		{
			Enable();
		}
	}

	void GetConfiguration(SFlowNodeConfig& config)
	{
		static const SInputPortConfig in_ports[] =
		{
			InputPortConfig_Void("enable",  _HELP("Trigger to enable the node")),
			InputPortConfig_Void("disable", _HELP("Trigger to disable the node")),
			InputPortConfig<string>("Ammo", _HELP("The node will only react when an actor is killed with this type of ammo. Empty = any ammo."),_HELP("Ammo"),  _UICONFIG("enum_global:ammos")),
			{ 0 }
		};

		static const SOutputPortConfig out_ports[] =
		{
			OutputPortConfig<EntityId>("victim", _HELP("entityID of the killed actor")),
			OutputPortConfig<EntityId>("killer", _HELP("entityID of the killer actor")),
			{ 0 }
		};

		config.sDescription = _HELP("triggers when an actor dies.");
		config.pInputPorts = in_ports;
		config.pOutputPorts = out_ports;
		config.SetCategory(EFLN_APPROVED);
	}

	void ProcessEvent(EFlowEvent event, SActivationInfo* pActInfo)
	{
		m_actInfo = *pActInfo;
		switch (event)
		{
		case eFE_Initialize:
			Disable();
			break;

		case eFE_Activate:
			{
				if (IsPortActive(pActInfo, INP_ENABLE))
				{
					Enable();
				}
				if (IsPortActive(pActInfo, INP_DISABLE))
				{
					Disable();
				}
				break;
			}

		case eFE_Update:
			{
				uint32 size = m_pendingDeaths.size();
				if (size > 0)
				{
					const TDeathInfo& info = m_pendingDeaths.back();
					ActivateOutput(pActInfo, OUT_VICTIM, info.victimId);
					ActivateOutput(pActInfo, OUT_KILLER, info.killerId);
					m_pendingDeaths.pop_back();
					size--;
				}
				if (size == 0)
					m_actInfo.pGraph->SetRegularlyUpdated(pActInfo->myID, false);
			}
		}
	}

	void Enable()
	{
		CGameRules* pGameRules = g_pGame->GetGameRules();
		if (pGameRules)
		{
			const string& ammo = GetPortString(&m_actInfo, INP_AMMOTYPE);
			bool isValidClass = g_pGame->GetIGameFramework()->GetNetworkSafeClassId(m_ammoClassId, ammo.c_str());
			if (!isValidClass)
				m_ammoClassId = 0;

			pGameRules->RegisterKillListener(this);
			m_enabled = true;
			m_pendingDeaths.clear();
		}
	}

	void Disable()
	{
		CGameRules* pGameRules = g_pGame ? g_pGame->GetGameRules() : nullptr;
		if (pGameRules)
		{
			pGameRules->UnRegisterKillListener(this);
		}
		m_enabled = false;
		m_pendingDeaths.clear();
	}

	// IGameRulesKillListener
	void OnEntityKilledEarly(const HitInfo& hitInfo)
	{
	}
	void OnEntityKilled(const HitInfo& hitInfo)
	{
		if (m_ammoClassId != 0 && m_ammoClassId != hitInfo.projectileClassId)
			return;

		TDeathInfo info;
		info.killerId = hitInfo.shooterId;
		info.victimId = hitInfo.targetId;
		m_pendingDeaths.push_back(info);
		m_actInfo.pGraph->SetRegularlyUpdated(m_actInfo.myID, true);
	}
	// ~IGameRulesKillListener

	virtual void GetMemoryUsage(ICrySizer* s) const
	{
		s->Add(*this);
	}

	struct TDeathInfo
	{
		void Serialize(TSerialize ser)
		{
			ser.Value("victimId", victimId);
			ser.Value("killerId", killerId);
		}

		EntityId victimId;
		EntityId killerId;
	};

	SActivationInfo         m_actInfo;
	std::vector<TDeathInfo> m_pendingDeaths;
	uint16                  m_ammoClassId;
	bool                    m_enabled;
};

class CFlowNode_EntityToScreenPos : public CFlowBaseNode<eNCT_Instanced>, public IGameFrameworkListener
{
	enum INPUTS
	{
		EIP_ENABLE = 0,
		EIP_DISABLE,
	};

	enum OUTPUTS
	{
		EOP_PX = 0,
		EOP_PY,
		EOP_VISIBLE,
		EOP_FALLOFFX,
		EOP_FALLOFFY,
	};

public:
	CFlowNode_EntityToScreenPos(SActivationInfo* pActInfo)
	{
	}

	~CFlowNode_EntityToScreenPos()
	{
		if (gEnv)
			gEnv->pGameFramework->UnregisterListener(this);
	}

	void GetConfiguration(SFlowNodeConfig& config)
	{
		static const SInputPortConfig in_ports[] =
		{
			InputPortConfig_Void("Enable",  _HELP("Trigger to enable this node")),
			InputPortConfig_Void("Disable", _HELP("Trigger to disable this node")),
			{ 0 }
		};
		static const SOutputPortConfig out_ports[] =
		{
			OutputPortConfig<float>("Px",             _HELP("X pos on screen (0-1)")),
			OutputPortConfig<float>("Py",             _HELP("Y pos on screen (0-1)")),
			OutputPortConfig<bool>("VisibleOnScreen", _HELP("True if visible on screen, otherwise false")),
			OutputPortConfig<int>("FalloffX",         _HELP("-1 if entity is left from viewfrustrum, 0 if inside, 1 if right")),
			OutputPortConfig<int>("FalloffY",         _HELP("-1 if entity is above the viewfrustrum, 0 if inside, 1 if beneath")),
			{ 0 }
		};
		config.nFlags |= EFLN_TARGET_ENTITY;
		config.pInputPorts = in_ports;
		config.pOutputPorts = out_ports;
		config.sDescription = _HELP("Get Screen pos for current entity");
		config.SetCategory(EFLN_ADVANCED);
	}

	virtual void GetMemoryUsage(ICrySizer* s) const
	{
		s->Add(*this);
	}

	IFlowNodePtr Clone(SActivationInfo* pActInfo)
	{
		return new CFlowNode_EntityToScreenPos(pActInfo);
	}

	void ProcessEvent(EFlowEvent event, SActivationInfo* pActInfo)
	{
		switch (event)
		{
		case eFE_Initialize:
			m_actInfo = *pActInfo;
			gEnv->pGameFramework->UnregisterListener(this);
			break;

		case eFE_SetEntityId:
			if (pActInfo->pEntity)
				m_entityId = pActInfo->pEntity->GetId();
			break;

		case eFE_Activate:
			if (IsPortActive(pActInfo, EIP_ENABLE))
			{
				gEnv->pGameFramework->RegisterListener(this, "CFlowNode_EntityToScreenPos", FRAMEWORKLISTENERPRIORITY_HUD);
			}
			if (IsPortActive(pActInfo, EIP_DISABLE))
			{
				gEnv->pGameFramework->UnregisterListener(this);
			}
			break;
		}
	}

	// IGameFrameworkListener
	virtual void OnSaveGame(ISaveGame* pSaveGame)         {}
	virtual void OnLoadGame(ILoadGame* pLoadGame)         {}
	virtual void OnLevelEnd(const char* nextLevel)        {}
	virtual void OnActionEvent(const SActionEvent& event) {}
	virtual void OnPostUpdate(float fDelta)
	{
		if (IEntity* pEntity = gEnv->pEntitySystem->GetEntity(m_entityId))
		{
			AABB box;
			pEntity->GetWorldBounds(box);
			Vec3 entityPos = box.GetCenter();

			Vec3 screenPos;
			gEnv->pRenderer->ProjectToScreen(entityPos.x, entityPos.y, entityPos.z, &screenPos.x, &screenPos.y, &screenPos.z);
			screenPos.x *= 0.01f;
			screenPos.y *= 0.01f;
			int falloffx = screenPos.x < 0 ? -1 : screenPos.x > 1 ? 1 : screenPos.z < 1 ? 0 : -1;
			int falloffy = screenPos.y < 0 ? -1 : screenPos.y > 1 ? 1 : screenPos.z < 1 ? 0 : -1;
			screenPos.x = clamp_tpl(screenPos.x, 0.0f, 1.f);
			screenPos.y = clamp_tpl(screenPos.y, 0.0f, 1.f);
			ActivateOutput(&m_actInfo, EOP_PX, screenPos.x);
			ActivateOutput(&m_actInfo, EOP_PY, screenPos.y);
			ActivateOutput(&m_actInfo, EOP_VISIBLE, falloffx + falloffy == 0);
			ActivateOutput(&m_actInfo, EOP_FALLOFFX, falloffx);
			ActivateOutput(&m_actInfo, EOP_FALLOFFY, falloffy);
		}
	}
	// ~IGameFrameworkListener

private:
	EntityId        m_entityId;
	SActivationInfo m_actInfo;
};

class CFlowNode_ScreenPosToWorldPos : public CFlowBaseNode<eNCT_Instanced>, public IGameFrameworkListener
{
	enum INPUTS
	{
		EIP_ENABLE = 0,
		EIP_DISABLE,
		EIP_PX,
		EIP_PY,
		EIP_DEPTH,
	};

	enum OUTPUTS
	{
		EOP_WORLDPOS = 0,
	};

public:
	CFlowNode_ScreenPosToWorldPos(SActivationInfo* pActInfo)
	{
	}

	~CFlowNode_ScreenPosToWorldPos()
	{
		if (gEnv)
			gEnv->pGameFramework->UnregisterListener(this);
	}

	IFlowNodePtr Clone(SActivationInfo* pActInfo)
	{
		return new CFlowNode_ScreenPosToWorldPos(pActInfo);
	}

	void GetConfiguration(SFlowNodeConfig& config)
	{
		static const SInputPortConfig in_ports[] =
		{
			InputPortConfig_Void("Enable",  _HELP("Trigger to enable this node")),
			InputPortConfig_Void("Disable", _HELP("Trigger to disable this node")),
			InputPortConfig<float>("Px",    0.5,                                   _HELP("X pos on screen (0-1)")),
			InputPortConfig<float>("Py",    0.5,                                   _HELP("Y pos on screen (0-1)")),
			InputPortConfig<float>("Depth", 0,                                     _HELP("Depth of the new position")),
			{ 0 }
		};
		static const SOutputPortConfig out_ports[] =
		{
			OutputPortConfig<Vec3>("WorldPos", _HELP("World position of the object")),
			{ 0 }
		};
		config.pInputPorts = in_ports;
		config.pOutputPorts = out_ports;
		config.sDescription = _HELP("Converts a screen position to a world position");
		config.SetCategory(EFLN_ADVANCED);
	}

	virtual void GetMemoryUsage(ICrySizer* s) const
	{
		s->Add(*this);
	}

	void ProcessEvent(EFlowEvent event, SActivationInfo* pActInfo)
	{
		switch (event)
		{
		case eFE_Initialize:
			m_actInfo = *pActInfo;
			gEnv->pGameFramework->UnregisterListener(this);
			break;
		case eFE_SetEntityId:
			break;
		case eFE_Activate:
			if (IsPortActive(pActInfo, EIP_ENABLE))
			{
				gEnv->pGameFramework->RegisterListener(this, "CFlowNode_ScreenPosToWorldPos", FRAMEWORKLISTENERPRIORITY_HUD);
			}
			if (IsPortActive(pActInfo, EIP_DISABLE))
			{
				gEnv->pGameFramework->UnregisterListener(this);
			}
			break;
		}
	}

	// IGameFrameworkListener
	virtual void OnSaveGame(ISaveGame* pSaveGame)         {}
	virtual void OnLoadGame(ILoadGame* pLoadGame)         {}
	virtual void OnLevelEnd(const char* nextLevel)        {}
	virtual void OnActionEvent(const SActionEvent& event) {}
	virtual void OnPostUpdate(float fDelta)
	{
		const CCamera& camera = GetISystem()->GetViewCamera();

		const float px = GetPortFloat(&m_actInfo, EIP_PX);
		const float py = GetPortFloat(&m_actInfo, EIP_PY);
		const float depth = GetPortFloat(&m_actInfo, EIP_DEPTH);

		const Vec2 vViewportCoords(px * camera.GetViewSurfaceX(), camera.GetViewSurfaceZ() - py * gEnv->pRenderer->GetHeight());

		Vec3 vPos0(0, 0, 0);
		gEnv->pRenderer->UnProjectFromScreen(vViewportCoords.x, vViewportCoords.y, 0, &vPos0.x, &vPos0.y, &vPos0.z);

		Vec3 vPos1(0, 0, 0);
		gEnv->pRenderer->UnProjectFromScreen(vViewportCoords.x, vViewportCoords.y, 1, &vPos1.x, &vPos1.y, &vPos1.z);

		const Vec3 vDir = (vPos1 - vPos0).GetNormalized();
		const Vec3 vWorldPos = vPos0 + vDir * depth;

		ActivateOutput(&m_actInfo, EOP_WORLDPOS, vWorldPos);
	}
	// ~IGameFrameworkListener

private:
	SActivationInfo m_actInfo;
};


REGISTER_FLOW_NODE("Actor:Sensor",	CFlowNode_ActorSensor);
REGISTER_FLOW_NODE("Game:WeaponSensor",	CFlowNode_WeaponSensor);
REGISTER_FLOW_NODE("Game:DifficultyLevel",	CFlowNode_DifficultyLevel);
REGISTER_FLOW_NODE("Camera:OverrideFOV",	CFlowNode_OverrideFOV);
REGISTER_FLOW_NODE("Actor:VisualDetector", CFlowNode_ActorVisualDetector );
REGISTER_FLOW_NODE("Actor:PlayerOnPickUpAmmo",	CFlowNode_PlayerOnPickUpAmmo);
REGISTER_FLOW_NODE("Actor:OnDeath",	CFlowNode_ActorOnDeath);
REGISTER_FLOW_NODE("Entity:EntityScreenPos", CFlowNode_EntityToScreenPos);
REGISTER_FLOW_NODE("Camera:ScreenToWorld", CFlowNode_ScreenPosToWorldPos);
