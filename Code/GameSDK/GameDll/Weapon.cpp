// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

/*************************************************************************
   -------------------------------------------------------------------------
   $Id$
   $DateTime$

   -------------------------------------------------------------------------
   History:
   - 22:8:2005   12:50 : Created by Márcio Martins

*************************************************************************/
#include "StdAfx.h"
#include "Game.h"
#include "GameCVars.h"
#include <CryEntitySystem/IEntitySystem.h>
#include <CryScriptSystem/IScriptSystem.h>
#include <IActionMapManager.h>
#include <IGameObject.h>
#include <IGameObjectSystem.h>
#include <IVehicleSystem.h>
#include "WeaponSystem.h"
#include "Weapon.h"
#include <CryNetwork/ISerialize.h>
#include "ScriptBind_Weapon.h"
#include "Player.h"
#include "PlayerStateEvents.h"
#include "PlayerPlugin_Interaction.h"
#include "GameRules.h"
#include "Projectile.h"
#include "ItemResourceCache.h"
#include "RecordingSystem.h"

#include "Laser.h"
#include "IronSight.h"
#include "Single.h"
#include <CryAnimation/CryCharAnimationParams.h>
#include "ItemSharedParams.h"
#include "WeaponSharedParams.h"
#include "UI/HUD/HUDEventDispatcher.h"
#include "WeaponStats.h"

#include "PlayerInput.h"
#include <IWorldQuery.h>

#include "TacticalManager.h"

#include "GameCodeCoverage/GameCodeCoverageTracker.h"

#include "UI/HUD/HUDUtils.h"
#include "UI/HUD/HUDEventWrapper.h"
#include "PersistantStats.h"

#include <CryExtension/CryCreateClassInstance.h>
#include <CryAISystem/IAIObjectManager.h>

#include "ItemAnimation.h"
#include "Melee.h"
#include "GameRulesModules/IGameRulesObjectivesModule.h"
#include "GameRulesModules/IGameRulesTeamsModule.h"

//#pragma optimize("", off)
//#pragma inline_depth(0)

#define INITIAL_COLLECTED_AMMO_RESERVE 5

float CWeapon::s_dofValue = 0.0f;
float CWeapon::s_dofSpeed = 0.0f;
float CWeapon::s_focusValue = 0.0f;
uint8 CWeapon::s_requestedActions = 0;
bool CWeapon::s_lockActionRequests = false;
TAmmoVector CWeapon::s_tmpCollectedAmmo;

static IEntityClass* s_CellFelineClass = NULL;
static IEntityClass* s_CellHammerClass = NULL;
static IEntityClass* s_CellSCARABClass = NULL;
static IEntityClass* s_CellSCARClass = NULL;
static IEntityClass* s_CellGaussClass = NULL;
static IEntityClass* s_CellK_VoltClass = NULL;
static IEntityClass* s_CellMikeClass = NULL;

class CAnimActionDeselect : public CItemAction
{
public:

	DEFINE_ACTION("WeaponDeselectAction");
	CAnimActionDeselect(FragmentID fragID, TagState fragTags)
		: CItemAction(PP_PlayerAction, fragID, fragTags, IAction::NoAutoBlendOut | IAction::FragmentIsOneShot)
	{}
};

template<typename T>
void ReleaseAndClear(std::vector<T*>& vec)
{
	//TODO: replace these with vectors of shared pointers so we don't need to do this!
	typename std::vector<T*>::const_iterator itEnd = vec.end();
	for (typename std::vector<T*>::iterator it = vec.begin(); it != itEnd; ++it)
	{
		(*it)->Release();
	}

	vec.resize(0);
}

struct CWeapon::EndChangeFireModeAction
{
	EndChangeFireModeAction(CWeapon* _weapon) : weapon(_weapon){};
	CWeapon* weapon;

	void execute(CItem* _this)
	{
		weapon->EndChangeFireMode();
	}
};

struct CWeapon::MeleeReactionTimer
{
	MeleeReactionTimer(CWeapon* pWeapon, int layer) : m_pWeapon(pWeapon), m_layer(layer) {}

	void execute(CItem* _this)
	{
		m_pWeapon->SetBusy(false);
	}

private:
	CWeapon* m_pWeapon;
	int      m_layer;
};

//------------------------------------------------------------------------
void CWeapon::StaticReset()
{
	stl::free_container(s_tmpCollectedAmmo);
}

CWeapon::CWeapon()
	: m_fm(0),
	m_melee(0),
	m_zm(0),
	m_zmId(0),
	m_primaryZmId(0),
	m_secondaryZmId(0),
	m_pFiringLocator(0),
	m_fire_alternation(false),
	m_destination(0, 0, 0),
	m_restartZoom(false),
	m_restartZoomStep(0),
	m_targetOn(false),
	m_switchingFireMode(false),
	m_nextShotTime(0.0f),
	m_lastRecoilUpdate(0),
	m_forcingRaise(false),
	m_doingMagazineSwap(false),
	m_delayedFireActionTimeOut(0.0f),
	m_delayedZoomActionTimeOut(0.0f),
	m_delayedMeleeActionTimeOut(0.0f),
	m_isClientOwnerOverride(false),
	m_minDropAmmoAvailable(true),
	m_isDeselecting(false),
	m_isRegisteredAmmoWithInventory(false),
	m_listeners(1),
	m_snapToTargetTimer(0.0f),
	m_crosshairMode(eWeaponCrossHair_Default),
	m_currentCrosshairVisibility(1.0f),
	m_zoomTimeMultiplier(1.f),
	m_selectSpeedMultiplier(1.f),
	m_isProxyWeapon(false),
	m_reloadButtonTimeStamp(0.0f),
	m_pWeaponStats(NULL),
	m_switchFireModeTimeStap(0.0f),
	m_addedAmmoCapacity(false),
	m_refillBelt(false),
	m_extendedClipAdded(false),
	m_DropAllowedFlag(true),
	m_bReloadWhenSelected(false),
	m_shouldPlayWeaponSelectAction(true),
	m_deselectAction(NULL),
	m_previousOwnerId(0),
#ifdef SHOT_DEBUG
	m_pShotDebug(NULL),
#endif //SHOT_DEBUG
	m_bIsHighlighted(false),
	m_weaponNextShotTimer(0.f)
{
	RegisterActions();

	s_CellFelineClass = gEnv->pEntitySystem->GetClassRegistry()->FindClass("CellFeline");
	s_CellHammerClass = gEnv->pEntitySystem->GetClassRegistry()->FindClass("CellHammer");
	s_CellSCARABClass = gEnv->pEntitySystem->GetClassRegistry()->FindClass("CellSCARAB");
	s_CellSCARClass = gEnv->pEntitySystem->GetClassRegistry()->FindClass("CellSCAR");
	s_CellGaussClass = gEnv->pEntitySystem->GetClassRegistry()->FindClass("CellGauss");
	s_CellK_VoltClass = gEnv->pEntitySystem->GetClassRegistry()->FindClass("CellK-Volt");
	s_CellMikeClass = gEnv->pEntitySystem->GetClassRegistry()->FindClass("CellMike");

	m_animationFiringLocator.Init(this);
	m_firemode = 0;
	m_prevFiremode = 0;
	m_reloadState = eNRS_NoReload;
	m_isFiring = false;
	m_isFiringStarted = false;
	m_fireCounter = 0;
	m_expended_ammo = 0;
	m_shootCounter = 0;
	m_meleeCounter = 0;
	m_attackIndex = -1;
	m_doMelee = false;
	m_netInitialised = false;
	m_lastRecvInventoryAmmo = 0;
	m_netNextShot = 0.f;
	m_pWeaponStats = new CWeaponStats();

	CryCreateClassInstanceForInterface(cryiidof<IAnimationOperatorQueue>(), m_BeltModifier);
}

//------------------------------------------------------------------------1
CWeapon::~CWeapon()
{
	// deactivate everything
	for (TFireModeVector::iterator it = m_firemodes.begin(); it != m_firemodes.end(); ++it)
		(*it)->Activate(false);
	// deactivate zoommodes
	for (TZoomModeVector::iterator it = m_zoommodes.begin(); it != m_zoommodes.end(); ++it)
		(*it)->Activate(false);

	if (m_melee)
	{
		m_melee->Activate(false);
	}

	ClearModes();

	if (IInventory* pInventory = GetActorInventory(GetOwnerActor()))
	{
		if (pInventory->FindItem(GetEntityId()) >= 0)
		{
			UnregisterUsedAmmoWithInventory(pInventory);
		}
	}

	HighlightWeapon(false);

	SAFE_DELETE(m_pWeaponStats);
	SAFE_RELEASE(m_deselectAction);

#ifdef SHOT_DEBUG
	SAFE_DELETE(m_pShotDebug);
#endif //SHOT_DEBUG
}

//------------------------------------------------------------------------
void CWeapon::InitItemFromParams()
{
	BaseClass::InitItemFromParams();

	InitAmmo();
	InitFireModes();
	InitZoomModes();
	InitAIData();
	InitWeaponStats();
	InitCompatibleAccessories();
}

//------------------------------------------------------------------------
void CWeapon::InitCompatibleAccessories()
{
	m_compatibleAccessories.clear();

	int n = m_sharedparams->accessoryparams.size();
	for (int i = 0; i < n; i++)
	{
		const SAccessoryParams accessory = m_sharedparams->accessoryparams[i];
		string name = accessory.pAccessoryClass->GetName();
		m_compatibleAccessories.push_back(name);
	}
}

//------------------------------------------------------------------------
void CWeapon::InitFireModes()
{
	CRY_PROFILE_FUNCTION(PROFILE_GAME);

	ReleaseAndClear(m_firemodes);
	m_fmIds.clear();
	m_fm = NULL;

	SAFE_RELEASE(m_melee);

	int n = m_weaponsharedparams->firemodeParams.size();

	m_firemodes.reserve(n);
	m_fmIds.reserve(n);

	int fireModeId = 0;
	for (int i = 0; i < n; i++)
	{
		const SParentFireModeParams* pNewParams = &m_weaponsharedparams->firemodeParams[i];

		CFireMode* pFireMode = g_pGame->GetWeaponSystem()->CreateFireMode(pNewParams->initialiseParams.modeType.c_str());

		if (!pFireMode)
		{
			GameWarning("Cannot create firemode '%s' in weapon '%s'! Skipping...", pNewParams->initialiseParams.modeType.c_str(), GetEntity()->GetName());
			continue;
		}

		pFireMode->SetName(pNewParams->initialiseParams.modeName.c_str());
		pFireMode->InitFireMode(this, pNewParams);
		pFireMode->Enable(pNewParams->initialiseParams.enabled);
		pFireMode->PostInit();

		m_firemodes.push_back(pFireMode);
		m_fmIds.insert(TFireModeIdMap::value_type(CryHashStringId(pNewParams->initialiseParams.modeName.c_str()), fireModeId));

		fireModeId++;
	}

	if (m_weaponsharedparams->pMeleeModeParams)
	{
		m_melee = g_pGame->GetWeaponSystem()->CreateMeleeMode();

		if (m_melee)
		{
			m_melee->InitMeleeMode(this, m_weaponsharedparams->pMeleeModeParams);
		}
		else
		{
			GameWarning("Cannot create melee mode for weapon '%s'!", GetEntity()->GetName());
		}
	}

	SetCurrentFireMode(0);
}

//------------------------------------------------------------------------
void CWeapon::InitZoomModes()
{
	CRY_PROFILE_FUNCTION(PROFILE_GAME);

	ReleaseAndClear(m_zoommodes);
	m_zmIds.clear();
	m_zmNames.clear();
	m_zmId = 0;
	m_zm = 0;
	m_primaryZmId = 0;
	m_secondaryZmId = 0;

	int n = m_weaponsharedparams->zoommodeParams.size();
	m_zoommodes.reserve(n);
	m_zmIds.reserve(n);
	m_zmNames.reserve(n);

	for (int i = 0; i < n; i++)
	{
		const SParentZoomModeParams* pNewParams = &m_weaponsharedparams->zoommodeParams[i];

		CIronSight* pZoomMode = static_cast<CIronSight*>(g_pGame->GetWeaponSystem()->CreateZoomMode(pNewParams->initialiseParams.modeType.c_str()));
		if (!pZoomMode)
		{
			GameWarning("Cannot create zoommode '%s' in weapon '%s'! Skipping...", pNewParams->initialiseParams.modeType.c_str(), GetEntity()->GetName());
			continue;
		}

		pZoomMode->InitZoomMode(this, pNewParams, m_zoommodes.size());
		pZoomMode->Enable(pNewParams->initialiseParams.enabled);

		m_zoommodes.push_back(pZoomMode);
		int idx = m_zoommodes.size() - 1;
		m_zmIds.insert(TZoomModeIdMap::value_type(CryHashStringId(pNewParams->initialiseParams.modeName.c_str()), idx));
		m_zmNames.insert(TZoomModeNameMap::value_type(idx, pNewParams->initialiseParams.modeName.c_str()));
	}
}

//------------------------------------------------------------------------
void CWeapon::InitAmmo()
{
	m_ammo = m_weaponsharedparams->ammoParams.ammo;
	m_bonusammo = m_weaponsharedparams->ammoParams.bonusAmmo;

	if (gEnv->bMultiplayer)
	{
		CGameRules* pGameRules = g_pGame->GetGameRules();
		if (pGameRules)
		{
			IGameRulesObjectivesModule* pObjectivesModule = pGameRules->GetObjectivesModule();
			if (pObjectivesModule)
			{
				// Get override ammo amount
				pObjectivesModule->UpdateInitialAmmoCounts(GetEntity()->GetClass(), m_ammo, m_bonusammo);
			}
		}
	}
}

void CWeapon::InitAIData()
{
	const AIWeaponDescriptor& descriptor = m_weaponsharedparams->aiWeaponDescriptor.descriptor;
	if (descriptor.smartObjectClass != "")
	{
		// check if the smartobject class has been overridden in the level
		SmartScriptTable props;
		if (GetEntity()->GetScriptTable() && GetEntity()->GetScriptTable()->GetValue("Properties", props))
		{
			const char* smartObjectClassProperties = NULL;
			if (!props->GetValue("soclasses_SmartObjectClass", smartObjectClassProperties) ||
			    (smartObjectClassProperties == NULL || smartObjectClassProperties[0] == 0))
			{
				props->SetValue("soclasses_SmartObjectClass", descriptor.smartObjectClass.c_str());
			}
		}
	}
}

//------------------------------------------------------------------------
void CWeapon::InitWeaponStats()
{
	m_pWeaponStats->SetBaseStats(m_sharedparams);
}

//------------------------------------------------------------------------
bool CWeapon::Init(IGameObject* pGameObject)
{
	if (!BaseClass::Init(pGameObject))
		return false;

	m_heatController.InitWithEntity(GetEntity(), 0.1f);

	g_pGame->GetWeaponScriptBind()->AttachTo(this);

	if (!gEnv->bMultiplayer)
		g_pGame->GetTacticalManager()->AddEntity(GetEntityId(), CTacticalManager::eTacticalEntity_Item);

	return true;
}

//------------------------------------------------------------------------
void CWeapon::Release()
{
	if (!gEnv->bMultiplayer && g_pGame && g_pGame->GetTacticalManager())
	{
		g_pGame->GetTacticalManager()->RemoveEntity(GetEntityId(), CTacticalManager::eTacticalEntity_Item);
	}

	//--- Guard against NULL shared params here because this may be called on an uninitialised item
	if (m_sharedparams)
	{
		if (IsSelected())
		{
			RemoveOwnerAttachedAccessories();
			AttachToHand(false);
		}
	}
	//move it here as the virtual dtor creates issues for CJaw being derived from this class and
	//	IWeaponFiringLocator and having m_pFiringLocator point to itself creates issues when deleting this
	if (m_pFiringLocator)
		m_pFiringLocator->WeaponReleased();

	BaseClass::Release();
}

//------------------------------------------------------------------------
void CWeapon::SNetWeaponData::NetSerialise(TSerialize ser)
{
	ser.Value("firemode", m_firemode, 'ui3'); // 0 - 7
	ser.Value("firing", m_isFiring, 'bool');
	ser.Value("zoomState", m_zoomState, 'bool');
	ser.Value("ammo", m_weapon_ammo, 'clip');
	ser.Value("fired", m_fireCounter, 'ui8'); // 0 - 255
}

//------------------------------------------------------------------------
void CWeapon::SNetWeaponReloadData::NetSerialise(TSerialize ser)
{
	ser.Value("reload", m_reload, 'reld');
	ser.Value("invammo", m_inventory_ammo, 'ammo');
	ser.Value("expendedAmmo", m_expended_ammo, 'ui8');
}

//------------------------------------------------------------------------
void CWeapon::SNetWeaponMeleeData::NetSerialise(TSerialize ser)
{
	ser.Value("meleeCount", m_meleeCounter, 'ui2');
}

//------------------------------------------------------------------------
void CWeapon::SNetWeaponChargeData::NetSerialise(TSerialize ser)
{
	ser.Value("m_charging", m_charging, 'bool');
}

static int calcNumShots(uint8 prevCount, uint8 newCount)
{
	int result = 0;

	if (newCount == prevCount)
	{
		result = 0;
	}
	else if (newCount > prevCount)
	{
		result = newCount - prevCount;
	}
	else // newCount < prevCount <-- wrap case
	{
		result = (255 - prevCount) + newCount + 1; // this needs to account for 0 being a shot as well, so we +1 to include that
	}

	return result;
}

//------------------------------------------------------------------------
bool CWeapon::NetSerialize(TSerialize ser, EEntityAspects aspect, uint8 profile, int flags)
{
	if (!BaseClass::NetSerialize(ser, aspect, profile, flags))
		return false;

	if (aspect == ASPECT_RELOAD)
	{
		NET_PROFILE_SCOPE("WeaponReload", ser.IsReading());

		CActor* owner = GetOwnerActor();

		SNetWeaponReloadData data = { 0 };
		if (ser.IsWriting())
		{
			data.m_reload = GetReloadState();
			data.m_inventory_ammo = 0;
			data.m_expended_ammo = m_expended_ammo;

			if (owner)
			{
				if (IInventory* inv = owner->GetInventory())
				{
					if (IFireMode* ifm = GetFireMode(GetCurrentFireMode()))
					{
						data.m_inventory_ammo = GetInventoryAmmoCount(ifm->GetAmmoType());
					}
				}
			}

			NetStateSent();
		}

		data.NetSerialise(ser);

		if (ser.IsReading())
		{
			if (m_reloadState != data.m_reload)
			{
				ClSetReloadState(data.m_reload);
			}

			if (owner)
			{
				int ammo_diff = 0;
				if (owner->IsClient())
				{
					ammo_diff = calcNumShots(data.m_expended_ammo, m_expended_ammo); // see if we've fired any extra shots before getting this update
				}

				// this should probably go in its own aspect
				if (m_lastRecvInventoryAmmo != data.m_inventory_ammo)
				{
					if (IInventory* inv = owner->GetInventory())
					{
						if (IFireMode* ifm = GetFireMode(GetCurrentFireMode()))
						{
							IEntityClass* pAmmoType = ifm->GetAmmoType();

							//Triggers on initial spawn if the client has Loadout Pro and they get told about their increased ammo amount before their capacity amount
							//(Due to one going by netserialise and the other by RMI, meaning they can arrive in the wrong order and the ammo amount therefore being clamped to the wrong value)
							IF_UNLIKELY (inv->GetAmmoCapacity(pAmmoType) < data.m_inventory_ammo)
							{
								inv->SetAmmoCapacity(pAmmoType, data.m_inventory_ammo);
							}

							inv->SetAmmoCount(pAmmoType, data.m_inventory_ammo - ammo_diff);
						}
					}
				}
			}

			m_lastRecvInventoryAmmo = data.m_inventory_ammo; //set this regardless as this can be sent before the client knows it's picked it up
		}
	}

	if (aspect == ASPECT_STREAM)
	{
		NET_PROFILE_SCOPE("WeaponStream", ser.IsReading());

		SNetWeaponData data = { 0 };

		if (ser.IsWriting())
		{
			data.m_weapon_ammo = NetGetCurrentAmmoCount();
			data.m_firemode = GetCurrentFireMode();
			data.m_isFiring = m_isFiring;
			data.m_fireCounter = m_fireCounter;
			EZoomState zoomState = GetZoomState();
			data.m_zoomState = (zoomState == eZS_ZoomingIn || zoomState == eZS_ZoomedIn);
		}

		data.NetSerialise(ser);

		if (ser.IsReading())
		{
			bool allowUpdate = NetAllowUpdate(false);

			if (m_firemode != data.m_firemode)
			{
				SetCurrentFireMode(data.m_firemode);
			}

			int ammo_diff = 0;
			if (allowUpdate)
			{
				m_isFiring = data.m_isFiring;
				ammo_diff = m_netInitialised ? calcNumShots(m_fireCounter, data.m_fireCounter) : 0;
				m_shootCounter += ammo_diff;
				m_fireCounter = data.m_fireCounter;

				if (m_shootCounter > 0 || m_isFiring || m_doMelee)
					RequireUpdate(eIUS_FireMode); // force update

				EZoomState zoomState = GetZoomState();
				bool isZooming = (zoomState == eZS_ZoomingIn || zoomState == eZS_ZoomedIn);
				if (isZooming != data.m_zoomState)
				{
					if (data.m_zoomState)
					{
						StartZoom(m_owner.GetId(), 1);
					}
					else
					{
						StopZoom(m_owner.GetId());
					}
				}
			}

			NetSetCurrentAmmoCount(data.m_weapon_ammo - ammo_diff);

			m_netInitialised = true;
		}
	}
	else if (aspect == ASPECT_MELEE)
	{
		NET_PROFILE_SCOPE("WeaponMelee", ser.IsReading());

		SNetWeaponMeleeData data;

		if (ser.IsWriting())
		{
			data.m_meleeCounter = m_meleeCounter;
			data.m_attackIndex = m_attackIndex;
		}

		data.NetSerialise(ser);

		if (ser.IsReading())
		{
			bool allowUpdate = NetAllowUpdate(false);

			if (allowUpdate && m_meleeCounter != data.m_meleeCounter)
			{
				m_meleeCounter = data.m_meleeCounter;
				m_doMelee = true;
				m_shootCounter = 0;
				m_netNextShot = 0.f;
				RequireUpdate(eIUS_FireMode); // force update
			}
		}
	}
	else if (aspect == ASPECT_CHARGING)
	{
		CFireMode* pFireMode = static_cast<CFireMode*>(GetFireMode(GetCurrentFireMode()));

		SNetWeaponChargeData data;

		if (ser.IsWriting() && pFireMode)
		{
			data.m_charging = pFireMode->IsCharging();
		}

		data.NetSerialise(ser);

		if (ser.IsReading() && pFireMode)
		{
			pFireMode->NetSetCharging(data.m_charging);
		}
	}

	// zoom modes are not serialized

	return true;
}

NetworkAspectType CWeapon::GetNetSerializeAspects()
{
	return BaseClass::GetNetSerializeAspects() | ASPECT_RELOAD | ASPECT_STREAM | ASPECT_MELEE | ASPECT_CHARGING;
}

//------------------------------------------------------------------------
void CWeapon::FullSerialize(TSerialize ser)
{
	BaseClass::FullSerialize(ser);

	ser.BeginGroup("WeaponAmmo");
	if (ser.IsReading())
	{
		m_ammo.clear();
		m_bonusammo.clear();
	}
	TAmmoVector::iterator it = m_ammo.begin();
	int ammoAmount = m_ammo.size();
	ser.Value("AmmoAmount", ammoAmount);
	for (int i = 0; i < ammoAmount; ++i)
	{
		CryFixedStringT<32> name;
		int amount = 0;
		if (ser.IsWriting())
		{
			if (it->pAmmoClass)
			{
				name = it->pAmmoClass->GetName();
				amount = it->count;
			}
			++it;
		}

		ser.BeginGroup("Ammo");
		ser.Value("AmmoName", name);
		ser.Value("Bullets", amount);
		ser.EndGroup();

		if (ser.IsReading())
		{
			IEntityClass* pClass = gEnv->pEntitySystem->GetClassRegistry()->FindClass(name.c_str());
			assert(pClass);

			SWeaponAmmoUtils::SetAmmo(m_ammo, pClass, amount);
		}
	}
	it = m_bonusammo.begin();
	ammoAmount = m_bonusammo.size();
	ser.Value("BonusAmmoAmount", ammoAmount);
	for (int i = 0; i < ammoAmount; ++i)
	{
		string name;
		int amount = 0;
		if (ser.IsWriting() && it->pAmmoClass)
		{
			name = it->pAmmoClass->GetName();
			amount = it->count;
			++it;
		}

		ser.BeginGroup("Ammo");
		ser.Value("AmmoName", name);
		ser.Value("Bullets", amount);
		ser.EndGroup();

		if (ser.IsReading())
		{
			IEntityClass* pClass = gEnv->pEntitySystem->GetClassRegistry()->FindClass(name.c_str());
			assert(pClass);

			SWeaponAmmoUtils::SetAmmo(m_bonusammo, pClass, amount);
		}
	}

	ser.Value("minDropAmmoAvail", m_minDropAmmoAvailable);
	ser.EndGroup();

	ser.BeginGroup("WeaponStats");
	if (GetOwnerId())
	{
		//if (m_fm)
		// m_fm->Serialize(ser);
		int numFiremodes = GetNumOfFireModes();
		ser.Value("numFiremodes", numFiremodes);
		if (ser.IsReading())
		{
			assert(numFiremodes == GetNumOfFireModes());
			if (numFiremodes != GetNumOfFireModes())
				CryWarning(VALIDATOR_MODULE_GAME, VALIDATOR_ERROR, "Num of firemodes changed - loading will be corrupted.");
		}
		for (int i = 0; i < numFiremodes; ++i)
			m_firemodes[i]->Serialize(ser);

		ser.Value("currentFireMode", m_firemode);

		bool hasZoom = (m_zm) ? true : false;
		ser.Value("hasZoom", hasZoom);
		if (hasZoom && ser.IsReading())
		{
			SetCurrentZoomMode(0);

			if (m_zm->IsZoomed())
			{
				m_zm->ExitZoom(true);
			}

			m_restartZoom = false;
			m_restartZoomStep = 0;
		}

		bool reloading = false;
		if (ser.IsWriting())
			reloading = m_fm ? m_fm->IsReloading() : 0;
		ser.Value("FireModeReloading", reloading);
		if (ser.IsReading() && reloading)
			Reload();
		ser.Value("Alternation", m_fire_alternation);
	}
	ser.Value("addedAmmoCapacity", m_addedAmmoCapacity);
	ser.Value("extendedClipAdded", m_extendedClipAdded);
	ser.Value("isRegisteredAmmoWithInventory", m_isRegisteredAmmoWithInventory);
	ser.EndGroup();
}

//------------------------------------------------------------------------
void CWeapon::PostSerialize()
{
	m_nextShotTime = 0.0f; //Reset this after loading

	int savedFireMode = m_firemode;
	// firemode is reset here since item is selected
	BaseClass::PostSerialize();

	const int numAccessories = m_accessories.size();

	for (int i = 0; i < numAccessories; i++)
	{
		FixAccessories(GetAccessoryParams(m_accessories[i].pClass), true);
	}

	//Firemodes
	int numFiremodes = GetNumOfFireModes();
	for (int i = 0; i < numFiremodes; ++i)
		m_firemodes[i]->PostSerialize();

	// restore saved firemode
	if (GetOwnerId())
		SetCurrentFireMode(savedFireMode);
}

//------------------------------------------------------------------------
void CWeapon::SerializeLTL(TSerialize ser)
{
	BaseClass::SerializeLTL(ser);

	ser.BeginGroup("WeaponAmmo");
	if (ser.IsReading())
	{
		m_ammo.clear();
		m_bonusammo.clear();
		m_nextShotTime = 0.0f; //Reset this after loading
	}
	TAmmoVector::iterator it = m_ammo.begin();
	int ammoAmount = m_ammo.size();
	ser.Value("AmmoAmount", ammoAmount);
	for (int i = 0; i < ammoAmount; ++i)
	{
		string name;
		int amount = 0;
		if (ser.IsWriting())
		{
			if (it->pAmmoClass)
			{
				name = it->pAmmoClass->GetName();
				amount = it->count;
			}
			++it; // elements are added to this vector in reading time. so we want to limit the use of this iterator to the inside of this writing code to avoid problems.
		}

		ser.BeginGroup("Ammo");
		ser.Value("AmmoName", name);
		ser.Value("Bullets", amount);
		ser.EndGroup();

		if (ser.IsReading())
		{
			IEntityClass* pClass = gEnv->pEntitySystem->GetClassRegistry()->FindClass(name);
			assert(pClass);

			SWeaponAmmoUtils::SetAmmo(m_ammo, pClass, amount);
		}
	}
	ser.EndGroup();

	if (GetOwnerId())
	{
		ser.BeginGroup("WeaponStats");

		//if (m_fm)
		// m_fm->Serialize(ser);
		int numFiremodes = GetNumOfFireModes();
		ser.Value("numFiremodes", numFiremodes);
		if (ser.IsReading())
		{
			assert(numFiremodes == GetNumOfFireModes());
			if (numFiremodes != GetNumOfFireModes())
				CryWarning(VALIDATOR_MODULE_GAME, VALIDATOR_ERROR, "Num of firemodes changed - loading will be corrupted.");
		}
		for (int i = 0; i < numFiremodes; ++i)
			m_firemodes[i]->Serialize(ser);

		int currentFireMode = GetCurrentFireMode();
		ser.Value("currentFireMode", currentFireMode);
		if (ser.IsReading())
			SetCurrentFireMode(currentFireMode);

		bool hasZoom = (m_zm) ? true : false;
		ser.Value("hasZoom", hasZoom);
		if (hasZoom)
		{
			int zoomMode = m_zmId;
			ser.Value("ZoomMode", zoomMode);
			bool isZoomed = m_zm->IsZoomed();
			ser.Value("Zoomed", isZoomed);
			int zoomStep = m_zm->GetCurrentStep();
			ser.Value("ZoomStep", zoomStep);
			ser.Value("PrimaryZoomMode", m_primaryZmId);
			ser.Value("SecondaryZoomMode", m_secondaryZmId);

			m_zm->Serialize(ser);

			if (ser.IsReading())
			{
				if (m_zmId != zoomMode)
					SetCurrentZoomMode(zoomMode);

				m_restartZoom = isZoomed;
				m_restartZoomStep = zoomStep;

				if (!isZoomed)
					m_zm->ExitZoom();
			}
		}

		bool reloading = false;
		if (ser.IsWriting())
			reloading = m_fm ? m_fm->IsReloading() : 0;
		ser.Value("FireModeReloading", reloading);
		if (ser.IsReading() && reloading)
			Reload();
		ser.Value("Alternation", m_fire_alternation);
		ser.EndGroup();

	}
}

//------------------------------------------------------------------------
void CWeapon::Update(SEntityUpdateContext& ctx, int update)
{
	CRY_PROFILE_FUNCTION(PROFILE_GAME);

	if (!IsDestroyed())
	{
		switch (update)
		{
		case eIUS_General:
			{
				bool requiresUpdate = m_heatController.Update(ctx.fFrameTime);

				const bool needsDofUpdate = (fabsf(s_dofSpeed) > 0.001f);
				const bool dofSpeedNegative = (s_dofSpeed < 0.0f);

				m_snapToTargetTimer = max(m_snapToTargetTimer - ctx.fFrameTime, 0.0f);

				if (needsDofUpdate)
				{
					const float newDofValue = s_dofValue + (s_dofSpeed * ctx.fFrameTime);
					s_dofValue = clamp_tpl(newDofValue, 0.0f, 1.0f);

					gEnv->p3DEngine->SetPostEffectParam("Dof_BlurAmount", s_dofValue);

					//GameWarning("Actual DOF value = %f",m_dofValue);
					if (dofSpeedNegative)
					{
						const float newFocusValue = s_focusValue = (s_dofSpeed * ctx.fFrameTime * 150.0f);
						s_focusValue = newFocusValue;
						gEnv->p3DEngine->SetPostEffectParam("Dof_FocusLimit", 20.0f + newFocusValue);
					}
				}

				if (m_weaponNextShotTimer > 0.f)
				{
					m_weaponNextShotTimer -= ctx.fFrameTime;
					requiresUpdate = true;
				}

				if (requiresUpdate)
				{
					RequireUpdate(eIUS_General);
				}

				if (IsSelected())
					m_scopeReticule.Update(this);

#ifdef SHOT_DEBUG
				if (m_pShotDebug)
				{
					m_pShotDebug->Update(gEnv->pTimer->GetFrameTime());
				}
#endif  //SHOT_DEBUG
			}
			break;

		case eIUS_Zooming:
			{
				if (m_zm && m_stats.selected)
				{
					m_zm->Update(ctx.fFrameTime, ctx.frameID);
				}
			}
			break;

		case eIUS_FireMode:
			{
				NetUpdateFireMode(ctx);

				if (m_fm)
					m_fm->Update(ctx.fFrameTime, ctx.frameID);
				if (m_melee)
					m_melee->Update(ctx.fFrameTime, ctx.frameID);
			}
			break;
		}

#ifndef _RELEASE
		if (m_fm && IsSelected() && g_pGameCVars->i_debug_zoom_mods)
		{
			m_fm->DebugUpdate(ctx.fFrameTime);
		}
#endif

		BaseClass::Update(ctx, update);

		if (!IsOwnerClient())
			return;

		//Client only stuff
		CPlayer* pPlayer = GetOwnerPlayer();

		const float sprintFactor = pPlayer ? pPlayer->m_weaponFPAiming.GetSprintFactor() : 1.f;
		const bool delayedZoomActionTimerActive = (m_delayedZoomActionTimeOut > 0.0f);
		const bool zoomWillTriggerThisFrame = (m_delayedZoomActionTimeOut <= ctx.fFrameTime);

		float currentMeleeAttackActionTimeOut = m_delayedMeleeActionTimeOut;
		m_delayedFireActionTimeOut = (float)__fsel(-sprintFactor, max(m_delayedFireActionTimeOut - ctx.fFrameTime, 0.0f), m_delayedFireActionTimeOut);
		m_delayedMeleeActionTimeOut = max(m_delayedMeleeActionTimeOut - ctx.fFrameTime, 0.0f);

		if (delayedZoomActionTimerActive)
		{
			m_delayedZoomActionTimeOut -= ctx.fFrameTime;
			if (m_zm && zoomWillTriggerThisFrame)
			{
				m_delayedZoomActionTimeOut = 0.0f;
				m_zm->StartZoom(m_delayedZoomStayZoomedVal, true, 1);
				RequestSetZoomState(true);
			}
		}

		if (currentMeleeAttackActionTimeOut > 0.f && m_delayedMeleeActionTimeOut == 0.f)
			MeleeAttack();
	}
}

void CWeapon::PostUpdate(float frameTime)
{
	if (m_fm)
		m_fm->PostUpdate(frameTime);
}

//------------------------------------------------------------------------
void CWeapon::HandleEvent(const SGameObjectEvent& evt)
{
	BaseClass::HandleEvent(evt);
	if (evt.event == eGFE_PauseGame)
		CancelCharge();
}

//------------------------------------------------------------------------
void CWeapon::ProcessEvent(const SEntityEvent& event)
{
	CRY_PROFILE_FUNCTION(PROFILE_GAME);

	switch (event.event)
	{
	case ENTITY_EVENT_HIDE:
		{
			if (m_fm && !m_fm->AllowZoom())
				m_fm->Cancel();
			CancelCharge();
			StopFire();
			if (IsOwnerClient())
			{
				ClearInputFlags();
			}
		}
		break;

	case ENTITY_EVENT_RESET:
		{
			//Leaving game mode
			if (gEnv->IsEditor() && !event.nParam[0])
			{
				m_listeners.Clear();
				break;
			}
		}
		break;

	case ENTITY_EVENT_ANIM_EVENT:
		{
			if (m_weaponsharedparams->reloadMagazineParams.magazineEventCRC32 != 0)
			{
				const AnimEventInstance* pAnimEvent = reinterpret_cast<const AnimEventInstance*>(event.nParam[0]);
				ICharacterInstance* pCharacter = reinterpret_cast<ICharacterInstance*>(event.nParam[1]);
				if (pAnimEvent && pCharacter)
				{
					AnimationEvent(pCharacter, *pAnimEvent);
				}
			}
		}
		break;
	}

	BaseClass::ProcessEvent(event);
}

uint64 CWeapon::GetEventMask() const
{
	return BaseClass::GetEventMask() | ENTITY_EVENT_BIT(ENTITY_EVENT_HIDE) | ENTITY_EVENT_BIT(ENTITY_EVENT_RESET) | ENTITY_EVENT_BIT(ENTITY_EVENT_ANIM_EVENT);
}

bool CWeapon::ResetParams()
{
	bool success = BaseClass::ResetParams();

	m_weaponsharedparams = g_pGame->GetGameSharedParametersStorage()->GetWeaponSharedParameters(GetEntity()->GetClass()->GetName(), false);

	if (!m_weaponsharedparams)
	{
		GameWarning("Uninitialised weapon params. Has weaponParams=\"1\" been included in your xml for item %s?", GetEntity()->GetName());

		return false;
	}

	m_weaponsharedparams->CacheResources();

	return success;
}

void CWeapon::PreResetParams()
{
	ResetOwner();
	// deactivate everything
	for (TFireModeVector::iterator it = m_firemodes.begin(); it != m_firemodes.end(); ++it)
		(*it)->Activate(false);
	// deactivate zoommodes
	for (TZoomModeVector::iterator it = m_zoommodes.begin(); it != m_zoommodes.end(); ++it)
		(*it)->Activate(false);

	if (m_melee)
	{
		m_melee->Activate(false);
	}

	ClearModes();
}

//------------------------------------------------------------------------
void CWeapon::Reset()
{
	CRY_PROFILE_FUNCTION(PROFILE_GAME);

	ClearModes();

	BaseClass::Reset();
	ClearInputFlags();

	SetCurrentFireMode(0);
	SetCurrentZoomMode(0);

	const int numAccessories = m_accessories.size();

	// have to refix them here.. (they get overriden by SetCurrentFireMode above)
	for (int i = 0; i < numAccessories; i++)
	{
		FixAccessories(GetAccessoryParams(m_accessories[i].pClass), true);
	}

	m_doingMagazineSwap = false;
	m_isDeselecting = false;
	m_extendedClipAdded = false;
	m_enterModifyAction = 0;

	if (gEnv->IsEditor())
	{
		const CPlayer* const pPlayer = static_cast<CPlayer*>(g_pGame->GetIGameFramework()->GetClientActor());
		if (pPlayer && pPlayer->GetInventory()->FindItem(GetEntityId()) != -1)
		{
			Drop(0.0f);
			InitItemFromParams();
		}
	}
}

//------------------------------------------------------------------------
void CWeapon::ClearModes()
{
	ReleaseAndClear(m_firemodes);
	m_fm = NULL;

	ReleaseAndClear(m_zoommodes);
	m_zm = NULL;

	SAFE_RELEASE(m_melee);
}

//------------------------------------------------------------------------
void CWeapon::UpdateFPView(float frameTime)
{
	BaseClass::UpdateFPView(frameTime);

	UpdateCrosshair(frameTime);
	UpdateBulletBelt();

	if (m_fm)
	{
		m_fm->UpdateFPView(frameTime);
	}

	if (m_zm)
	{
		m_zm->UpdateFPView(frameTime);
	}
}

//------------------------------------------------------------------------
void CWeapon::MeleeAttack(bool bShort /* = false */)
{
	if (!m_melee || !CanMeleeAttack())
		return;

	if (m_zm && (m_zm->IsZoomed() || m_zm->IsZoomingInOrOut()))
	{
		if (m_fm && !m_fm->AllowZoom())
			m_fm->Cancel();

		ExitZoom(true);
		// ExitZoom will try to fade the crosshair back in, so overrule it
		FadeCrosshair(0.0f, 0.0f);
	}

	if (m_fm)
	{
		m_fm->StopFire();
		if (m_fm->IsReloading())
		{
			if (m_fm->CanCancelReload())
				RequestCancelReload();
			else
			{
				RequestCancelReload();
				return;
			}
		}
	}

	m_melee->CloseRangeAttack(bShort);
	m_melee->StartAttack();
}

//------------------------------------------------------------------------
bool CWeapon::CanMeleeAttack() const
{
	if (AreAnyItemFlagsSet(eIF_Modifying | eIF_Transitioning | eIF_BlockActions) || m_isDeselecting)
		return false;

	CActor* pOwnerActor = GetOwnerActor();
	if (pOwnerActor)
	{
		if (pOwnerActor->GetActorStats()->bStealthKilling)
		{
			return false;
		}

		if (pOwnerActor->IsPlayer())
		{
			CPlayer* pOwnerPlayer = static_cast<CPlayer*>(pOwnerActor);

			const bool hasLargeObject = pOwnerPlayer->GetCurrentInteractionInfo().interactionType == eInteraction_LargeObject;
			if (hasLargeObject)
				return false;
		}
	}

	if (IsSelecting() || IsDeselecting())
		return false;

	if (m_zm && m_zm->IsZoomed() && m_zm->GetMaxZoomSteps() > 1)
		return false;

	return m_melee && m_melee->CanAttack();
}

//------------------------------------------------------------------------

bool CWeapon::ShouldPlaySelectAction() const
{
	return m_shouldPlayWeaponSelectAction && CItem::ShouldPlaySelectAction();
}

//------------------------------------------------------------------------
void CWeapon::Select(bool select)
{
	CActor* pOwner = GetOwnerActor();
	if (select && (IsDestroyed() || (pOwner && pOwner->IsDead())))
		return;

	const bool isOwnerClient = IsOwnerClient();

	//If actor is grabbed by player, don't let him select weapon
	if (select && pOwner && pOwner->GetActorStats() && pOwner->GetActorStats()->isGrabbed)
	{
		pOwner->HolsterItem(true);
		return;
	}

	BaseClass::Select(select);

	CCCPOINT_IF(select, Weapon_Select);
	CCCPOINT_IF(!select, Weapon_Deselect);

	m_isDeselecting = false;

	if (!select)
	{
		if (IsZoomed() || IsZoomingInOrOut())
			ExitZoom(true);

		if (m_enterModifyAction)
		{
			m_enterModifyAction->Stop();
			m_enterModifyAction = 0;
		}

		if (isOwnerClient)
		{
			gEnv->p3DEngine->SetPostEffectParam("Dof_Active", 0.0f);
		}

		m_switchingFireMode = false;

		// network
		m_shootCounter = 0;
		m_netNextShot = 0.f;
		m_isFiring = false;
		m_isFiringStarted = false;
		//

		SetBusy(false);

		if (isOwnerClient && !gEnv->bMultiplayer && pOwner)
		{
			SetToDefaultFireModeIfNeeded(*pOwner);
		}

		if (m_deselectAction)
		{
			m_deselectAction->Stop();
			SAFE_RELEASE(m_deselectAction);
		}
	}

	if (gEnv->bMultiplayer && isOwnerClient && pOwner)
	{
		pOwner->NotifyCurrentItemChanged(this);
	}

	if (isOwnerClient)
	{
		FadeCrosshair(1.0f, WEAPON_FADECROSSHAIR_SELECT);
	}

	if (!select)
		SetNextShotTime(false);

	if (m_fm)
		m_fm->Activate(select);

	if (m_zm)
		m_zm->Activate(select);

#ifdef SHOT_DEBUG
	if (select && ((g_pGameCVars->pl_shotDebug == 1 && isOwnerClient) || (g_pGameCVars->pl_shotDebug == 2)) && pOwner)
	{
		if (!m_pShotDebug)
		{
			m_pShotDebug = new CShotDebug(*pOwner, *this);
		}
	}
	else
	{
		SAFE_DELETE(m_pShotDebug);
	}
#endif //SHOT_DEBUG

	if (select)
	{
		SetNextShotTime(true);
	}

	if (m_melee)
	{
		m_melee->Activate(select);
	}

	if (select)
	{
		SetCurrentFireMode(m_firemode);
		m_weaponNextShotTimer = 0.f;
	}

	SetPlaySelectAction(true);
}

//---------------------------------------------------------------------
uint32 CWeapon::StartDeselection(bool fastDeselect)
{
	CActor* pOwnerActor = GetOwnerActor();
	const float speedMul = fastDeselect ? g_pGameCVars->i_fastSelectMultiplier : -1.0f;
	if (m_isDeselecting)
	{
		if (m_deselectAction)
		{
			float timeRemaining = ((float)GetCurrentAnimationTime(eIGS_Owner) / 1000.0f) - m_deselectAction->GetActiveTime();

			if (fastDeselect)
			{
				float oldSpeedBias = m_deselectAction->GetSpeedBias();
				m_deselectAction->SetSpeedBias(speedMul);

				timeRemaining *= oldSpeedBias / speedMul;
			}

			return (uint32)(timeRemaining * 1000.0f);
		}
		else
		{
			//--- Adjust existing animation play speed if necessary
			uint32 curTime = GetCurrentAnimationTime(eIGS_Owner);
			if (pOwnerActor)
			{
				IAnimatedCharacter* pAnimCharacter = pOwnerActor->GetAnimatedCharacter();
				CAnimationPlayerProxy* pAnimPlayerProxy = pAnimCharacter ? pAnimCharacter->GetAnimationPlayerProxy(eAnimationGraphLayer_UpperBody) : NULL;

				if (pAnimPlayerProxy)
				{
					CAnimation* pAnim = const_cast<CAnimation*>(pAnimPlayerProxy->GetTopAnimation(pOwnerActor->GetEntity(), ITEM_OWNER_ACTION_LAYER));
					if (pAnim)
					{
						float remainingTime = (1.0f - pAnim->GetCurrentSegmentNormalizedTime()) * ((float)curTime);
						if (fastDeselect)
						{
							float invSpeedMulChange = pAnim->GetPlaybackScale() / speedMul;
							remainingTime *= invSpeedMulChange;
							pAnim->SetPlaybackScale(speedMul);
						}
						curTime = (uint32)remainingTime;
					}
				}
			}
			return curTime;
		}
	}

	StopFire();

	if (IsZoomed() || IsZoomingInOrOut())
	{
		ExitZoom(true);
	}

	CCCPOINT(Weapon_StartDeselect);

	if (gEnv->bMultiplayer)
	{
		CPlayer* pPlayer = static_cast<CPlayer*>(GetOwnerActor());

		if (pPlayer && pPlayer->IsClient())
		{
			pPlayer->NotifyCurrentItemChanged(this);
		}
	}

	bool bShouldPlaySelectionActions = ShouldPlaySelectAction();
	if (bShouldPlaySelectionActions)
	{
		IActionController* pActionController = pOwnerActor ? pOwnerActor->GetAnimatedCharacter()->GetActionController() : 0;
		if (pActionController)
		{
			const SControllerDef& contDef = pActionController->GetContext().controllerDef;

			FragmentID fragID = contDef.m_fragmentIDs.Find("Deselect");
			if (fragID != FRAGMENT_ID_INVALID)
			{
				const CTagDefinition* pFragTagDef = contDef.GetFragmentTagDef(fragID);
				TagState fragTags = TAG_STATE_EMPTY;
				if (pFragTagDef)
				{
					eItemAttachment attachmentId = ChooseAttachmentPoint(true);
					if (attachmentId == eIA_StowPrimary)
					{
						pFragTagDef->Set(fragTags, pFragTagDef->Find("primary"), true);
					}
					if (attachmentId == eIA_StowSecondary)
					{
						pFragTagDef->Set(fragTags, pFragTagDef->Find("secondary"), true);
					}
				}

				IAction* pAction = new CAnimActionDeselect(fragID, fragTags);

				SAFE_RELEASE(m_deselectAction);
				m_deselectAction = pAction;
				m_deselectAction->AddRef();
				bShouldPlaySelectionActions = PlayFragment(pAction, speedMul);
			}
		}
	}

	m_isDeselecting = true;
	SetBusy(true);

	return bShouldPlaySelectionActions ? GetCurrentAnimationTime(eIGS_Owner) : 0;
}

//---------------------------------------------------------------------
void CWeapon::CancelDeselection()
{
	m_isDeselecting = false;
	SetBusy(false);
}

//---------------------------------------------------------------------
bool CWeapon::IsDeselecting() const
{
	return m_isDeselecting;
}

//---------------------------------------------------------------------
void CWeapon::AddAmmoCapacity()
{
	if (m_addedAmmoCapacity)
		return;
	CActor* pOwner = GetOwnerActor();
	if (!pOwner)
		return;
	IInventory* pOwnerInventory = pOwner->GetInventory();
	if (!pOwnerInventory)
		return;
	if (pOwnerInventory->GetItemByClass(GetEntity()->GetClass()) != 0)
		return;
	const CWeaponSharedParams* pWeaponParams = GetWeaponSharedParams();

	for (size_t i = 0; i < pWeaponParams->ammoParams.capacityAmmo.size(); ++i)
	{
		IEntityClass* pAmmoType = pWeaponParams->ammoParams.capacityAmmo[i].pAmmoClass;
		int additionalCapacity = pWeaponParams->ammoParams.capacityAmmo[i].count;
		int currentCapacity = pOwnerInventory->GetAmmoCapacity(pAmmoType);
		pOwnerInventory->SetAmmoCapacity(pAmmoType, currentCapacity + additionalCapacity);
	}

	if (gEnv->bMultiplayer)
	{
		ProcessAllAccessoryAmmoCapacities(pOwnerInventory, true);
	}

	m_addedAmmoCapacity = true;
}

void CWeapon::DropAmmoCapacity()
{
	if (!m_addedAmmoCapacity)
		return;
	CActor* pOwner = GetOwnerActor();
	if (!pOwner)
		return;
	IInventory* pOwnerInventory = pOwner->GetInventory();
	if (!pOwnerInventory)
		return;
	const CWeaponSharedParams* pWeaponParams = GetWeaponSharedParams();

	for (size_t i = 0; i < pWeaponParams->ammoParams.capacityAmmo.size(); ++i)
	{
		IEntityClass* pAmmoType = pWeaponParams->ammoParams.capacityAmmo[i].pAmmoClass;
		int additionalCapacity = pWeaponParams->ammoParams.capacityAmmo[i].count;
		int currentCapacity = pOwnerInventory->GetAmmoCapacity(pAmmoType);
		int currentInventory = pOwnerInventory->GetAmmoCount(pAmmoType);
		int currentBonus = GetBonusAmmoCount(pAmmoType);
		int newCapacity = currentCapacity - additionalCapacity;
		int newBonus = currentBonus + max(currentInventory - newCapacity, 0);
		pOwnerInventory->SetAmmoCapacity(pAmmoType, currentCapacity - additionalCapacity);
		SetBonusAmmoCount(pAmmoType, newBonus);
	}

	if (gEnv->bMultiplayer)
	{
		ProcessAllAccessoryAmmoCapacities(pOwnerInventory, false);
	}

	m_addedAmmoCapacity = false;
}

void CWeapon::ProcessAllAccessoryAmmoCapacities(IInventory* pOwnerInventory, bool addCapacity)
{
	const int numAccessories = m_accessories.size();

	for (int i = 0; i < numAccessories; i++)
	{
		if (CItem* pAccessory = static_cast<CItem*>(m_pItemSystem->GetItem(m_accessories[i].accessoryId)))
		{
			pAccessory->ProcessAccessoryAmmoCapacities(pOwnerInventory, addCapacity);
		}
	}
}

//------------------------------------------------------------------------
void CWeapon::PickUp(EntityId picker, bool sound, bool select, bool keepHistory, const char* setup)
{
	SetOwnerId(picker);
	if (IsClient() && gEnv->pGameFramework->GetClientActorId() == picker)
	{
		if (IEntity* pEntity = GetEntity())
		{
			IEntityClass* pEntityClass = pEntity->GetClass();
			const char* collectibleId = pEntityClass->GetName();
			if (pEntityClass == s_CellFelineClass)
			{
				collectibleId = "Feline";
			}
			else if (pEntityClass == s_CellHammerClass)
			{
				collectibleId = "Hammer";
			}
			else if (pEntityClass == s_CellSCARABClass)
			{
				collectibleId = "SCARAB";
			}
			else if (pEntityClass == s_CellSCARClass)
			{
				collectibleId = "SCAR";
			}
			else if (pEntityClass == s_CellGaussClass)
			{
				collectibleId = "Gauss";
			}
			else if (pEntityClass == s_CellK_VoltClass)
			{
				collectibleId = "K-Volt";
			}
			else if (pEntityClass == s_CellMikeClass)
			{
				collectibleId = "Mike";
			}

			CPersistantStats* pStats = g_pGame->GetPersistantStats();
			if (pStats && pStats->GetStat(collectibleId, EMPS_SPWeaponByName) == 0)
			{
				pStats->SetMapStat(EMPS_SPWeaponByName, collectibleId, eDatabaseStatValueFlag_Available);
				if (!gEnv->bMultiplayer && sound)
				{
					// Show hud unlock msg
					SHUDEventWrapper::DisplayWeaponUnlockMsg(collectibleId);
				}
			}

			CEquipmentLoadout* pLoadout = g_pGame->GetEquipmentLoadout();
			if (pLoadout)
			{
				IEntityClass* pClass = pEntity->GetClass();
				if (pClass)
				{
					const CEquipmentLoadout::SEquipmentItem* pLoadoutItem = pLoadout->GetItemByName(pClass->GetName());
					if (pLoadoutItem)
					{
						if (pLoadoutItem->m_category == CEquipmentLoadout::eELC_EXPLOSIVE)
						{
							SHUDEvent event(eHUDEvent_UpdateExplosivesAmmo);
							event.AddData(true);
							CHUDEventDispatcher::CallEvent(event);
						}
					}
				}
			}
		}
	}
	AddAmmoCapacity();
	BaseClass::PickUp(picker, sound, select, keepHistory, setup);
}

//------------------------------------------------------------------------
void CWeapon::Drop(float impulseScale, bool selectNext, bool byDeath)
{
	//Need to check if in game for proper ammo/bonus reseting
	bool inGame = (!gEnv->IsEditor()) || (gEnv->IsEditing());
	if (inGame)
	{
		CActor* pOwner = GetOwnerActor();
		IInventory* pOwnerInventory = pOwner ? pOwner->GetInventory() : NULL;

		//Note: Net-synch should work, since this code is executed in server and  client,
		//      and as long as server has the right ammo/bonus count, it should be fine
		//			This code as well needs to be executed before the owner gets reseted
		if (pOwnerInventory)
		{
			bool ownerIsAI = !pOwner->IsPlayer();
			if (ownerIsAI)
			{
				OnDroppedByAI(pOwnerInventory);
			}
			else
			{
				OnDroppedByPlayer(pOwnerInventory);
				m_previousOwnerId = pOwner->GetEntityId();
			}
		}
	}

	DropAmmoCapacity();

	for (size_t i = 0; i < m_firemodes.size(); ++i)
	{
		m_firemodes[i]->SetProjectileSpeedScale(1.0f);
		m_firemodes[i]->StopPendingFire();
	}

	BaseClass::Drop(impulseScale, selectNext, byDeath);
}

//------------------------------------------------------------------------
void CWeapon::SetFiringLocator(IWeaponFiringLocator* pLocator)
{
	if (!m_animationFiringLocator.IsSet())
	{
		if (m_pFiringLocator && m_pFiringLocator != pLocator)
			m_pFiringLocator->WeaponReleased();
		m_pFiringLocator = pLocator;
	}
	else
	{
		m_animationFiringLocator.SetOtherFiringLocator(pLocator);
	}
};

//------------------------------------------------------------------------
IWeaponFiringLocator* CWeapon::GetFiringLocator() const
{
	return m_pFiringLocator;
};

//------------------------------------------------------------------------
void CWeapon::AddEventListener(IWeaponEventListener* pListener, const char* who)
{
	m_listeners.Add(pListener, who);
}

//------------------------------------------------------------------------
void CWeapon::RemoveEventListener(IWeaponEventListener* pListener)
{
	m_listeners.Remove(pListener);
}

//------------------------------------------------------------------------
Vec3 CWeapon::GetFiringPos(const Vec3& probableHit) const
{
	if (m_fm)
		return m_fm->GetFiringPos(probableHit);

	return Vec3(0, 0, 0);
}

//------------------------------------------------------------------------
Vec3 CWeapon::GetFiringDir(const Vec3& probableHit, const Vec3& firingPos) const
{
	if (m_fm)
		return m_fm->GetFiringDir(probableHit, firingPos);

	return Vec3(0, 0, 0);
}

//------------------------------------------------------------------------
void CWeapon::StartFire()
{
	if (m_fm && !IsDestroyed())
	{
		CActor* pOwner = GetOwnerActor();
		if (!pOwner || (!pOwner->IsDead() && pOwner->CanFire()))
		{
			m_fm->StartFire();

			m_weaponNextShotTimer = m_fm->GetNextShotTime();
			OnStartFire(GetOwnerId());
		}
	}
}

//------------------------------------------------------------------------
void CWeapon::StartFire(const SProjectileLaunchParams& launchParams)
{
	// Ignore launch params for weapons which don't use it
	StartFire();
}

//------------------------------------------------------------------------
void CWeapon::StopFire()
{
	if (m_fm)
	{
		m_fm->StopFire();
		OnStopFire(GetOwnerId());
	}
}

//------------------------------------------------------------------------
bool CWeapon::CanFire() const
{
	CActor* pOwnerActor = GetOwnerActor();
	return m_fm && m_fm->CanFire() && (!pOwnerActor || !pOwnerActor->IsFallen());
}

//------------------------------------------------------------------------
bool CWeapon::CanStopFire() const
{
	return !IsAnimationControlled();
}

//------------------------------------------------------------------------
void CWeapon::StartZoom(EntityId actorId, int zoomed)
{
	if (m_zm == NULL)
		return;

	CActor* pOwner = GetOwnerActor();
	if (pOwner)
	{
		if (pOwner->IsDead() || !pOwner->CanUseIronSights())
			return;
	}

	if (IsDestroyed())
		return;

	if (m_fm && !((CFireMode*)m_fm)->CanZoom())
		return;

	bool interruptingZoomOutWithZoom = (m_zm->IsZoomingInOrOut() && !m_zm->IsZoomingIn());  //This allows us to re-zoom while the zooming out anim is still playing giving better responsiveness

	bool stayZoomed = (m_zm->IsZoomed() && ((m_zm->IsZoomingInOrOut() && !m_zm->IsToggle()) || interruptingZoomOutWithZoom));

	if (!m_zm->IsZoomed() || interruptingZoomOutWithZoom)
	{
		bool delayZoom = false;
		if (pOwner && pOwner->IsPlayer())
		{
			// We only update the delay for the local client.
			if (pOwner->IsClient())
			{
				SPlayerStats* pStats = static_cast<SPlayerStats*>(pOwner->GetActorStats());
				if (pOwner->IsSprinting())
				{
					pStats->bIgnoreSprinting = true;
					m_delayedZoomActionTimeOut = GetParams().sprintToZoomDelay;
					m_delayedZoomStayZoomedVal = stayZoomed;
					delayZoom = true;
				}
			}
		}
		if (!delayZoom && m_delayedZoomActionTimeOut == 0)
		{
			m_zm->StartZoom(stayZoomed, true, zoomed);
		}
		RequestSetZoomState(true);
	}
}

//------------------------------------------------------------------------
void CWeapon::StopZoom(EntityId actorId)
{
	CActor* pOwner = GetOwnerActor();
	SPlayerStats* pStats = pOwner ? static_cast<SPlayerStats*>(pOwner->GetActorStats()) : 0;
	if (pStats)
		pStats->bIgnoreSprinting = false;

	if (m_zm && m_delayedZoomActionTimeOut == 0.0f)
	{
		m_zm->StopZoom();
		RequestSetZoomState(false);
	}
	else
	{
		m_delayedZoomActionTimeOut = 0.0f;
	}
}

//------------------------------------------------------------------------
bool CWeapon::CanZoom() const
{
	return m_zm && m_zm->CanZoom();
}

//------------------------------------------------------------------------
void CWeapon::ExitZoom(bool force)
{
	if (m_zm && (IsZoomed() || IsZoomingIn()))
		m_zm->ExitZoom(force);
}

//------------------------------------------------------------------------
bool CWeapon::CanModify() const
{
	if (gEnv->bMultiplayer)
	{
		if (m_sharedparams->accessoryparams.size())
		{
			CGameRules* pGameRules = g_pGame->GetGameRules();
			if (IGameRulesTeamsModule* pTeamsModule = pGameRules->GetTeamsModule())
			{
				if (EntityId ownerId = GetOwnerId())
				{
					return pTeamsModule->CanTeamModifyWeapons(pGameRules->GetTeam(ownerId));
				}
			}
		}
		else
		{
			return false;
		}
	}

	return true;
}

//------------------------------------------------------------------------
bool CWeapon::IsZoomed() const
{
	return m_zm && m_zm->IsZoomed();
}

//------------------------------------------------------------------------
bool CWeapon::IsZoomingInOrOut() const
{
	return m_zm && m_zm->IsZoomingInOrOut();
}

//------------------------------------------------------------------------
bool CWeapon::IsZoomingIn() const
{
	if (m_zm)
	{
		return (m_zm->GetZoomState() == eZS_ZoomingIn);
	}

	return false;
}

//------------------------------------------------------------------------
bool CWeapon::IsZoomingOut() const
{
	return IsZoomingInOrOut() && !IsZoomingIn();
}

//------------------------------------------------------------------------
bool CWeapon::IsZoomOutScheduled() const
{
	if (m_zm)
	{
		CIronSight* pIronSight = static_cast<CIronSight*>(m_zm);
		return pIronSight->IsZoomingOutScheduled();
	}
	return false;
}

//------------------------------------------------------------------------
void CWeapon::CancelZoomOutSchedule()
{
	if (m_zm)
	{
		CIronSight* pIronSight = static_cast<CIronSight*>(m_zm);
		pIronSight->CancelZoomOutSchedule();
	}
}

//------------------------------------------------------------------------
bool CWeapon::IsZoomStable() const
{
	if (m_zm)
	{
		return (m_zm->IsStable());
	}

	return false;
}

//------------------------------------------------------------------------
EZoomState CWeapon::GetZoomState() const
{
	if (m_zm)
		return m_zm->GetZoomState();
	return eZS_ZoomedOut;
}

//------------------------------------------------------------------------
float CWeapon::GetZoomTransition() const
{
	float zoomTransition = 0.0f;
	if (m_zm)
	{
		if (m_zm->GetCurrentStep() > 1)
			zoomTransition = 1.0f;
		else
			zoomTransition = m_zm->GetZoomTransition();
	}
	return zoomTransition;
}

//------------------------------------------------------------------------
float CWeapon::GetZoomInTime() const
{
	float time = m_zm ? m_zm->GetZoomInTime() : 0.0f;

	return time;
}

//------------------------------------------------------------------------
bool CWeapon::ShouldSnapToTarget() const
{
	if (m_zm && m_zm->AllowsZoomSnap())
	{
		return (m_snapToTargetTimer > 0.0f);
	}

	return false;
}

//---------------------------------------------------------------------
bool CWeapon::IsFiring() const
{
	return m_fm && m_fm->IsFiring();
}

//---------------------------------------------------------------------
bool CWeapon::IsReloading(bool includePending) const
{
	return (m_fm && m_fm->IsReloading(includePending));
}

bool CWeapon::IsValidAssistTarget(IEntity* pEntity, IEntity* pSelf, bool includeVehicles /*=false*/)
{
	if (!pEntity)
		return false;

	IActor* pActor = m_pGameFramework->GetIActorSystem()->GetActor(pEntity->GetId());
	IAIObject* pAI = pEntity->GetAI();

	if (!pActor && includeVehicles && pAI)
	{
		IVehicle* pVehicle = m_pGameFramework->GetIVehicleSystem()->GetVehicle(pEntity->GetId());
		if (pVehicle && pVehicle->GetStatus().health > 0.f && pAI->IsHostile(pSelf->GetAI(), false))
			return true;
	}

	// Check for target validity
	if (!pActor)
		return false;

	if (!pAI)
	{
		if (pActor->IsPlayer() && pEntity != pSelf && !pEntity->IsHidden() && !pActor->IsDead())
		{
			int ownteam = g_pGame->GetGameRules()->GetTeam(pSelf->GetId());
			int targetteam = g_pGame->GetGameRules()->GetTeam(pEntity->GetId());

			// Assist aiming on non-allied players only
			return (targetteam == 0 && ownteam == 0) || (targetteam != 0 && targetteam != ownteam);
		}
		else
		{
			return false;
		}
	}

	return (pEntity != pSelf && !pEntity->IsHidden() &&
	        !pActor->IsDead() && pAI->GetAIType() != AIOBJECT_VEHICLE &&
	        pAI->IsHostile(pSelf->GetAI(), false));
}

//------------------------------------------------------------------------
void CWeapon::RestartZoom(bool force)
{
	if (m_restartZoom || force)
	{
		if (m_zm && !IsBusy() && m_zm->CanZoom())
		{
			m_zm->StartZoom(true, false, m_restartZoomStep);

			m_restartZoom = false;
		}
	}
}

//------------------------------------------------------------------------
void CWeapon::MountAt(const Vec3& pos)
{
	BaseClass::MountAt(pos);

	gEnv->pAISystem->GetAIObjectManager()->CreateAIObject(AIObjectParams(AIOBJECT_MOUNTEDWEAPON, 0, GetEntityId()));
}

//------------------------------------------------------------------------
void CWeapon::MountAtEntity(EntityId entityId, const Vec3& pos, const Ang3& angles)
{
	BaseClass::MountAtEntity(entityId, pos, angles);

	if (gEnv->bServer && !m_bonusammo.empty())
	{
		TAmmoVector::const_iterator bonusAmmoEndCit = m_bonusammo.end();
		for (TAmmoVector::const_iterator bonusAmmoCit = m_bonusammo.begin(); bonusAmmoCit != bonusAmmoEndCit; ++bonusAmmoCit)
		{
			const SWeaponAmmo& bonusAmmo = *bonusAmmoCit;
			SetInventoryAmmoCount(bonusAmmo.pAmmoClass, GetInventoryAmmoCount(bonusAmmo.pAmmoClass) + bonusAmmo.count);
		}

		m_bonusammo.clear();
	}
}

//------------------------------------------------------------------------
void CWeapon::Reload(bool force)
{
	CActor* pOwner = GetOwnerActor();
	bool ownerIsPlayer = pOwner && pOwner->IsPlayer();

	bool canReload = CanReload();
	if (canReload)
		UnlowerItem();

	CPlayer* pOwnerPlayer(NULL);
	if (ownerIsPlayer)
	{
		pOwnerPlayer = static_cast<CPlayer*>(pOwner);
		IPlayerInput* pPlayerInput = pOwnerPlayer->GetPlayerInput();
		if (pPlayerInput && pPlayerInput->GetType() == IPlayerInput::PLAYER_INPUT)
			static_cast<CPlayerInput*>(pPlayerInput)->ForceStopSprinting();
	}

	if (m_fm)
	{
		if (canReload || force)
		{
			int zoomStep = m_zm ? m_zm->GetCurrentStep() : 0;
			m_fm->Reload(zoomStep);

			if (pOwnerPlayer)
			{
				pOwnerPlayer->SetLastReloadTime(gEnv->pTimer->GetFrameStartTime().GetSeconds());
			}

			if (!pOwner || pOwner->IsClient())
				RequestReload();

			OnReloaded();

			m_bReloadWhenSelected = false;
		}
	}
}

//------------------------------------------------------------------------
bool CWeapon::CanReload() const
{
	bool bCanReload = m_fm ? m_fm->CanReload() : false;

	// We don't want animation events to reload the weapon if the clip is empty
	bCanReload = bCanReload && !IsAnimationControlled();

	return bCanReload;
}

//------------------------------------------------------------------------
bool CWeapon::OutOfAmmo(bool allFireModes) const
{
	if (!allFireModes)
		return m_fm && m_fm->OutOfAmmo();

	for (size_t i = 0; i < m_firemodes.size(); i++)
		if (!m_firemodes[i]->OutOfAmmo())
			return false;

	return true;
}

//------------------------------------------------------------------------
bool CWeapon::OutOfAmmoTypes() const
{
	CActor* pOwner = GetOwnerActor();
	IInventory* pInventory = pOwner ? pOwner->GetInventory() : 0;
	if (!pInventory)
		return true;

	for (size_t i = 0; i < m_ammo.size(); ++i)
	{
		IEntityClass* pAmmoType = m_ammo[i].pAmmoClass;
		int inventoryCount = pInventory->GetAmmoCount(pAmmoType);
		int clipCount = GetAmmoCount(pAmmoType);
		if (inventoryCount > 0 || clipCount > 0)
		{
			return false;
		}
	}

	return true;
}

//------------------------------------------------------------------------
bool CWeapon::LowAmmo(float thresholdPerCent) const
{
	return m_fm && m_fm->LowAmmo(thresholdPerCent);
}

//------------------------------------------------------------------------
void CWeapon::SetBonusAmmoCount(IEntityClass* pAmmoType, int amount)
{
	SWeaponAmmoUtils::SetAmmo(m_bonusammo, pAmmoType, amount);
}

//------------------------------------------------------------------------
int CWeapon::GetBonusAmmoCount(IEntityClass* pAmmoType) const
{
	return SWeaponAmmoUtils::GetAmmoCount(m_bonusammo, pAmmoType);
}

//------------------------------------------------------------------------
int CWeapon::GetAmmoCount(IEntityClass* pAmmoType) const
{
	CActor* pOwnerActor = GetOwnerActor();
	bool weaponOverchargeActive = false;
	if (pOwnerActor != NULL)
	{
		// AI has infinite ammo
		if (pOwnerActor->IsPlayer() == false)
		{
			return 99;
		}
		weaponOverchargeActive = (pOwnerActor->GetOverchargeDamageScale() > 1.001f);
	}

	const bool canOvercharge = GetSharedItemParams()->params.can_overcharge;
	if (canOvercharge && weaponOverchargeActive && (m_fm != NULL))
	{
		return m_fm->GetClipSize();
	}

	return SWeaponAmmoUtils::GetAmmoCount(m_ammo, pAmmoType);
}

//------------------------------------------------------------------------
void CWeapon::SetAmmoCount(IEntityClass* pAmmoType, int count)
{
	CActor* pOwnerActor = GetOwnerActor();
	int currentCount = 0;
	int currentCapacity = 0;
	if (pAmmoType != NULL && pOwnerActor != NULL)
	{
		currentCount = GetAmmoCount(pAmmoType);
		IInventory* pInventory = GetActorInventory(pOwnerActor);
		if (pInventory)
		{
			currentCapacity = pInventory->GetAmmoCapacity(pAmmoType);
		}
	}
	const bool ammoChanged = currentCount != count;

	const bool bExistingAmmoType = SWeaponAmmoUtils::SetAmmo(m_ammo, pAmmoType, count);
	if (!bExistingAmmoType && m_isRegisteredAmmoWithInventory)
	{
		if (IInventory* pInventory = GetActorInventory(GetOwnerActor()))
		{
			if (pInventory->FindItem(GetEntityId()) >= 0)
			{
				pInventory->AddAmmoUser(pAmmoType);
			}
		}
	}

	CHANGED_NETWORK_STATE(this, ASPECT_STREAM);

	// send game event
	IEntity* pOwnerEntity = gEnv->pEntitySystem->GetEntity(m_owner.GetId());
	if (pOwnerEntity)
	{
		m_pGameFramework->GetIGameplayRecorder()->Event(pOwnerEntity, GameplayEvent(eGE_AmmoCount, GetEntity() /*->GetClass()*/->GetName(), float(count), (void*)(EXPAND_PTR)GetFireModeIdxWithAmmo(pAmmoType)));
	}

	OnSetAmmoCount(GetOwnerId());
}

//------------------------------------------------------------------------
bool CWeapon::CanPickUpAmmo(IInventory* pDestinationInventory)
{
	for (TAmmoVector::iterator it = m_ammo.begin(); it != m_ammo.end(); ++it)
	{
		IEntityClass* pAmmoClass = it->pAmmoClass;
		if (SWeaponAmmoUtils::FindAmmoConst(m_weaponsharedparams->ammoParams.accessoryAmmo, pAmmoClass) != NULL)
		{
			continue;
		}

		int actualCount = pDestinationInventory->GetAmmoCount(pAmmoClass);
		int maxCapacity = pDestinationInventory->GetAmmoCapacity(pAmmoClass);

		if (maxCapacity == 0 || actualCount < maxCapacity)
		{
			return true;
		}
	}
	return false;
}

//------------------------------------------------------------------------
int CWeapon::GetInventoryAmmoCount(IEntityClass* pAmmoType) const
{
	if (m_hostId)
	{
		IVehicle* pVehicle = m_pGameFramework->GetIVehicleSystem()->GetVehicle(m_hostId);
		if (pVehicle)
			return pVehicle->GetAmmoCount(pAmmoType);

		return 0;
	}

	if (g_pGameCVars->g_infiniteAmmo)
	{
		return 999;
	}

	IF_UNLIKELY (g_pGameCVars->g_infiniteAmmoTutorialMode == 1)
	{
		const char* levelName = m_pGameFramework->GetLevelName();
		if (levelName != NULL && levelName[0] && !stricmp(levelName, "Tutorial"))
		{
			return 999;
		}
	}

	IInventory* pInventory = GetActorInventory(GetOwnerActor());
	if (!pInventory)
		return 0;

	return pInventory->GetAmmoCount(pAmmoType);
}

//------------------------------------------------------------------------
void CWeapon::SetInventoryAmmoCount(IEntityClass* pAmmoType, int count)
{
	if (!pAmmoType)
		return;

	if (m_hostId)
	{
		IVehicle* pVehicle = m_pGameFramework->GetIVehicleSystem()->GetVehicle(m_hostId);
		if (pVehicle)
			pVehicle->SetAmmoCount(pAmmoType, count);

		return;
	}

	IInventory* pInventory = GetActorInventory(GetOwnerActor());

	SetInventoryAmmoCountInternal(pInventory, pAmmoType, count);
}

//-----------------------------------------------------------------------
bool CWeapon::SetInventoryAmmoCountInternal(IInventory* pInventory, IEntityClass* pAmmoType, int count)
{
	bool ammoChanged = false;

	if (pInventory)
	{
		IActor* pInventoryOwner = pInventory->GetActor();
		bool isLocalClient = pInventoryOwner ? pInventoryOwner->IsClient() : false;

		const int capacity = pInventory->GetAmmoCapacity(pAmmoType);
		const int current = pInventory->GetAmmoCount(pAmmoType);
		if (count >= capacity)
		{
			//If still there's some place, full inventory to maximum...
			if (current != capacity)
			{
				ammoChanged = true;
				pInventory->SetAmmoCount(pAmmoType, capacity);
			}
		}
		else
		{
			ammoChanged = true;
			pInventory->SetAmmoCount(pAmmoType, count);
		}

		CHANGED_NETWORK_STATE(this, ASPECT_RELOAD);

		if (isLocalClient && ammoChanged)
		{
			const int difference = min(count, capacity) - current;

			SHUDEvent eventPickUp(eHUDEvent_OnAmmoPickUp);
			eventPickUp.AddData(SHUDEventData((void*)GetIWeapon()));
			eventPickUp.AddData(SHUDEventData(difference));
			eventPickUp.AddData(SHUDEventData((void*)pAmmoType));
			CHUDEventDispatcher::CallEvent(eventPickUp);
		}
	}

	return ammoChanged;
}

//------------------------------------------------------------------------
IFireMode* CWeapon::GetFireMode(int idx) const
{
	return GetCFireMode(idx);
}

//------------------------------------------------------------------------
IFireMode* CWeapon::GetFireMode(const char* name) const
{
	return GetCFireMode(name);
}

//------------------------------------------------------------------------
CFireMode* CWeapon::GetCFireMode(int idx) const
{
	if (idx >= 0 && idx < (int)m_firemodes.size())
		return m_firemodes[idx];
	return 0;
}

//------------------------------------------------------------------------
CFireMode* CWeapon::GetCFireMode(const char* name) const
{
	TFireModeIdMap::const_iterator it = m_fmIds.find(CryHashStringId::GetIdForName(name));
	if (it == m_fmIds.end())
		return 0;

	return GetCFireMode(it->second);
}

//------------------------------------------------------------------------
int CWeapon::GetFireModeIdxWithAmmo(const IEntityClass* pAmmoClass) const
{
	TFireModeIdMap::const_iterator it = m_fmIds.begin(), itEnd = m_fmIds.end();
	for (; it != itEnd; ++it)
	{
		IFireMode* pFireMode = GetFireMode(it->second);
		if (pFireMode && pFireMode->GetAmmoType() == pAmmoClass)
			return it->second;
	}
	return -1;
}

//------------------------------------------------------------------------
int CWeapon::GetFireModeIdx(const char* name) const
{
	TFireModeIdMap::const_iterator it = m_fmIds.find(CryHashStringId::GetIdForName(name));
	if (it != m_fmIds.end())
		return it->second;
	return -1;
}

//------------------------------------------------------------------------
int CWeapon::GetCurrentFireMode() const
{
	return m_firemode;
}

//------------------------------------------------------------------------
int CWeapon::GetPreviousFireMode() const
{
	return m_prevFiremode;
}

//------------------------------------------------------------------------
void CWeapon::SetCurrentFireMode(int idx)
{
	if (m_firemodes.empty())
		return;

	CActor* pOwnerActor = GetOwnerActor();
	if (pOwnerActor && !pOwnerActor->IsClient()) //Play transition anim for remote players
	{
		PlayChangeFireModeTransition(static_cast<CFireMode*>(GetFireMode(idx)));
	}

	if (m_fm)
		m_fm->Activate(false);

	if (idx >= (int)m_firemodes.size())
		m_fm = 0;
	else
		m_fm = m_firemodes[idx];

	if (m_fm)
	{
		m_fm->Activate(true);

		if (IsServer())
		{
			if (GetOwnerId())
			{
				m_pGameplayRecorder->Event(GetOwner(), GameplayEvent(eGE_WeaponFireModeChanged, m_fm->GetName(), (float)idx, (void*)(EXPAND_PTR)GetEntityId()));
			}

			CHANGED_NETWORK_STATE(this, ASPECT_STREAM);
		}
	}

	m_prevFiremode = m_firemode;
	m_firemode = idx;

	if (m_fm)
	{
		bool isDefaultFireMode = static_cast<CFireMode*>(m_fm)->GetParentShared()->initialiseParams.enabled;
		if (isDefaultFireMode || (m_secondaryZmId == 0) || !m_fm->IsEnabled())
			SetCurrentZoomMode(m_primaryZmId);
		else
			SetCurrentZoomMode(m_secondaryZmId);
	}

	// network
	m_shootCounter = 0;
	m_netNextShot = 0.f;
	m_isFiring = false;
	m_isFiringStarted = false;
	//

	OnFireModeChanged(m_firemode);
}

//------------------------------------------------------------------------
void CWeapon::SetCurrentFireMode(const char* name)
{
	TFireModeIdMap::iterator it = m_fmIds.find(CryHashStringId::GetIdForName(name));
	if (it == m_fmIds.end())
		return;

	SetCurrentFireMode(it->second);
}

//------------------------------------------------------------------------
void CWeapon::ChangeFireMode()
{
	const int iCurrentFireMode = GetCurrentFireMode();
	const int newId = GetNextFireMode(iCurrentFireMode);

	if (newId != iCurrentFireMode && !IsReloading())
	{
		RequestFireMode(newId);
	}
}

//------------------------------------------------------------------------
int CWeapon::GetNextFireMode(int currMode) const
{
	if (m_firemodes.empty() || (currMode > ((int)m_firemodes.size() - 1)))
		return 0;

	int t = currMode;
	do
	{
		t++;
		if (t == m_firemodes.size())
			t = 0;
		if (IFireMode* pFM = GetFireMode(t))
			if (pFM->IsEnabled() && !IsFiremodeDisabledByAccessory(t))
				return t;
	}
	while (t != currMode);

	return t;
}

//------------------------------------------------------------------------
bool CWeapon::IsFiremodeDisabledByAccessory(int idx) const
{
	const size_t numAccessories = m_accessories.size();

	for (size_t i = 0; i < numAccessories; i++)
	{
		const SAccessoryParams* pParams = GetAccessoryParams(m_accessories[i].pClass);

		const size_t numDisabledFiremodes = pParams->disableFiremodes.size();

		for (size_t fireModeIndex = 0; fireModeIndex < numDisabledFiremodes; fireModeIndex++)
		{
			if (GetFireModeIdx(pParams->disableFiremodes[fireModeIndex].c_str()) == idx)
			{
				return true;
			}
		}
	}

	return false;
}

//------------------------------------------------------------------------
IZoomMode* CWeapon::GetZoomMode(int idx) const
{
	if (idx >= 0 && idx < (int)m_zoommodes.size())
		return m_zoommodes[idx];
	return 0;
}

//------------------------------------------------------------------------
IZoomMode* CWeapon::GetZoomMode(const char* name) const
{
	TZoomModeIdMap::const_iterator it = m_zmIds.find(CryHashStringId::GetIdForName(name));
	if (it == m_zmIds.end())
		return 0;

	return GetZoomMode(it->second);
}

//------------------------------------------------------------------------
const char* CWeapon::GetZoomModeName(int idx) const
{
	TZoomModeNameMap::const_iterator it = m_zmNames.find(idx);
	if (it != m_zmNames.end())
		return it->second.c_str();

	return "";
}

//------------------------------------------------------------------------
int CWeapon::GetZoomModeIdx(const char* name) const
{
	TZoomModeIdMap::const_iterator it = m_zmIds.find(CryHashStringId::GetIdForName(name));
	if (it != m_zmIds.end())
		return it->second;
	return -1;
}

//------------------------------------------------------------------------
int CWeapon::GetCurrentZoomMode() const
{
	return m_zmId;
}

//------------------------------------------------------------------------
void CWeapon::SetCurrentZoomMode(int idx)
{
	if (m_zoommodes.empty() || (idx == m_zmId && m_zm != 0))
		return;

	bool wasZoomed = !IsModifying() && (m_zm ? m_zm->IsZoomed() : false);
	bool wasZoomingOut = !IsModifying() && (m_zm ? (m_zm->IsZoomingInOrOut() && !m_zm->IsZoomingIn()) : false);

	if (m_zm)
		m_zm->Activate(false);

	m_zm = m_zoommodes[idx];
	m_zmId = idx;

	if (m_zm)
	{
		m_zm->Activate(true);

		if (wasZoomed && !wasZoomingOut)
			m_zm->StartZoom();
	}
}

//------------------------------------------------------------------------
void CWeapon::SetCurrentZoomMode(const char* name)
{
	TZoomModeIdMap::iterator it = m_zmIds.find(CryHashStringId::GetIdForName(name));
	if (it == m_zmIds.end())
		return;

	SetCurrentZoomMode(it->second);
}

//------------------------------------------------------------------------
void CWeapon::ChangeZoomMode()
{
	if (m_zoommodes.empty())
		return;

	int t = m_zmId;
	do
	{
		t++;
		if (t == m_zoommodes.size())
			t = 0;
		if (GetZoomMode(t)->IsEnabled())
		{
			m_primaryZmId = t;
			SetCurrentZoomMode(t);
		}
	}
	while (t != m_zmId);
}

//------------------------------------------------------------------------
void CWeapon::EnableZoomMode(int idx, bool enable)
{
	IZoomMode* pZoomMode = GetZoomMode(idx);
	if (pZoomMode)
		pZoomMode->Enable(enable);
}

//------------------------------------------------------------------------
bool CWeapon::IsServerSpawn(IEntityClass* pAmmoType) const
{
	return g_pGame->GetWeaponSystem()->IsServerSpawn(pAmmoType);
}

//------------------------------------------------------------------------
CProjectile* CWeapon::SpawnAmmo(IEntityClass* pAmmoType, bool remote)
{
	return g_pGame->GetWeaponSystem()->SpawnAmmo(pAmmoType, remote);
}

//------------------------------------------------------------------------
void CWeapon::FadeCrosshair(float to, float time, float delay)
{
	if (m_currentCrosshairVisibility == to)
		return;
	if (m_crosshairMode == eWeaponCrossHair_ForceOff)
		return;

	SHUDEvent hudEvent(eHUDEvent_FadeCrosshair);
	hudEvent.AddData(to);
	hudEvent.AddData(time);
	hudEvent.AddData(delay);
	hudEvent.AddData(eFadeCrosshair_Ironsight);
	CHUDEventDispatcher::CallEvent(hudEvent);
	m_currentCrosshairVisibility = to;
}

//------------------------------------------------------------------------
void CWeapon::UpdateCrosshair(float frameTime)
{
	if (!m_restartZoom)
		return;

	RestartZoom();
}

//------------------------------------------------------------------------
void CWeapon::SetCrosshairMode(EWeaponCrosshair mode)
{
	m_crosshairMode = mode;
	if (GetOwnerId() == g_pGame->GetIGameFramework()->GetClientActorId())
	{
		SHUDEvent event(eHUDEvent_OnCrosshairModeChanged);
		event.AddData(SHUDEventData((int)mode));
		CHUDEventDispatcher::CallEvent(event);
	}
}

//------------------------------------------------------------------------
void CWeapon::AccessoriesChanged(bool initialLoadoutSetup)
{
	const int numFiremodes = m_firemodes.size();
	const int numZoommodes = m_zoommodes.size();

	IEntityClass* pAccessories[ITEM_MAX_NUM_ACCESSORIES] = { 0 };
	GetCurrentAccessories(pAccessories);

	for (int i = 0; i < numFiremodes; i++)
	{
		CFireMode* pFireMode = m_firemodes[i];

		const SFireModeParams* pCurrentParams = pFireMode->GetShared();
		const SFireModeParams* pNewParams = GetAccessoryAlteredFireModeParams(pFireMode, pAccessories);

		if (pNewParams != pCurrentParams)
		{
			pFireMode->ResetSharedParams(pNewParams);

			if ((pFireMode == m_fm) && (pNewParams->fireparams.clip_size != pCurrentParams->fireparams.clip_size))
			{
				IEntityClass* pCurrentAmmoClass = pFireMode->GetAmmoType();
				IInventory* pInventory = GetActorInventory(GetOwnerActor());

				if (pInventory && pCurrentAmmoClass)
				{
					const SWeaponAmmo* pCurrentAmmo = SWeaponAmmoUtils::FindAmmo(m_ammo, pCurrentAmmoClass);

					if ((pCurrentAmmo) && (pCurrentAmmo->count > 0))
					{
						if (gEnv->bMultiplayer)
						{
							const int clipSize = pFireMode->GetClipSize(); //still call getclipsize in case there are any modifiers applied
							const int clipDiff = clipSize - pCurrentAmmo->count;

							if ((clipDiff < 0) || (initialLoadoutSetup && clipDiff > 0)) //Only make an instant change when clip size decreased, otherwise we let the reload handle it (Apart from on initial loadout setup)
							{
								const int inventoryCount = pInventory->GetAmmoCount(pCurrentAmmoClass);

								SetAmmoCount(pCurrentAmmoClass, pCurrentAmmo->count + clipDiff);
								SetInventoryAmmoCountInternal(pInventory, pCurrentAmmoClass, max(0, inventoryCount - clipDiff));
							}
						}
						else
						{
							int clipSize = pFireMode->GetClipSize();
							int currentOnClip = pCurrentAmmo->count;
							int currentOnInventory = pInventory->GetAmmoCount(pCurrentAmmoClass);
							int currentInventoryCapacity = pInventory->GetAmmoCapacity(pCurrentAmmoClass);
							int addToClip = clipSize - currentOnClip;
							int capacityDiference = pCurrentParams->fireparams.clip_size - pNewParams->fireparams.clip_size;
							bool adding = capacityDiference < 0;

							if (addToClip > 0)
								addToClip = min(addToClip, currentOnInventory);

							if (adding != m_extendedClipAdded)
								pInventory->SetAmmoCapacity(pCurrentAmmoClass, currentInventoryCapacity + capacityDiference);
							m_extendedClipAdded = adding;

							SetAmmoCount(pCurrentAmmoClass, currentOnClip + addToClip);
							pInventory->SetAmmoCount(pCurrentAmmoClass, currentOnInventory - addToClip);
						}
					}
				}
			}
		}
	}

	for (int i = 0; i < numZoommodes; i++)
	{
		CIronSight* pZoomMode = static_cast<CIronSight*>(m_zoommodes[i]);

		const SZoomModeParams* pCurrentParams = pZoomMode->GetShared();
		const SZoomModeParams* pNewParams = GetAccessoryAlteredZoomModeParams(pZoomMode, pAccessories);

		if (pNewParams != pCurrentParams)
		{
			pZoomMode->ResetSharedParams(pNewParams);
		}
	}

	if (m_melee) //Melee is shared between all firemodes and therefore only needs to be checked once
	{
		const SMeleeModeParams* pNewMeleeParams = GetAccessoryAlteredMeleeParams(pAccessories);
		const SMeleeModeParams* pCurrentMeleeParams = m_melee->GetMeleeModeParams();
		if (pNewMeleeParams != pCurrentMeleeParams)
		{
			m_melee->InitMeleeMode(this, pNewMeleeParams);
		}
	}

	m_zoomTimeMultiplier = 1.f;
	float selectTimeMultiplier = 1.f;

	const size_t numAccessories = m_accessories.size();

	for (size_t accessoryIndex = 0; accessoryIndex < numAccessories; accessoryIndex++)
	{
		const CItemSharedParams* pItemShared = g_pGame ? g_pGame->GetGameSharedParametersStorage()->GetItemSharedParameters(m_accessories[accessoryIndex].pClass->GetName(), false) : NULL;
		if (pItemShared)
		{
			m_zoomTimeMultiplier *= pItemShared->params.zoomTimeMultiplier;
			selectTimeMultiplier *= pItemShared->params.selectTimeMultiplier;
		}
	}

	m_selectSpeedMultiplier = (1.f / max(selectTimeMultiplier, FLT_EPSILON));

	CItem::AccessoriesChanged(initialLoadoutSetup);
}

void CWeapon::GetCurrentAccessories(IEntityClass** pAccessoriesOut)
{
	const int numAccessoryParams = m_sharedparams->accessoryparams.size();
	for (int i = 0; i < numAccessoryParams; i++)
	{
		IEntityClass* pAccessory = m_sharedparams->accessoryparams[i].pAccessoryClass;
		if (HasAccessory(pAccessory))
		{
			pAccessoriesOut[0] = pAccessory;

			for (int j = i + 1; j < numAccessoryParams; j++)
			{
				IEntityClass* pSecondAccessory = m_sharedparams->accessoryparams[j].pAccessoryClass;
				if (HasAccessory(pSecondAccessory))
				{
					pAccessoriesOut[1] = pSecondAccessory;

					for (int k = j + 1; k < numAccessoryParams; k++)
					{
						IEntityClass* pThirdAccessory = m_sharedparams->accessoryparams[k].pAccessoryClass;
						if (HasAccessory(pThirdAccessory))
						{
							pAccessoriesOut[2] = pThirdAccessory;

							for (int l = k + 1; l < numAccessoryParams; l++)
							{
								IEntityClass* pForthAccessory = m_sharedparams->accessoryparams[l].pAccessoryClass;
								if (HasAccessory(pForthAccessory))
								{
									pAccessoriesOut[3] = pForthAccessory;
									return;
								}
							}
						}
					}

					return;
				}
			}

			return;
		}
	}
}

const SFireModeParams* CWeapon::GetAccessoryAlteredFireModeParams(CFireMode* pFireMode, IEntityClass** pAccessories)
{
	const SParentFireModeParams* pParentParams = pFireMode->GetParentShared();

	if (pAccessories[3])
	{
		if (const SFireModeParams* pNewParams = pParentParams->FindAccessoryFireModeParams(pAccessories[0], pAccessories[1], pAccessories[2], pAccessories[3]))
		{
			return pNewParams;
		}
	}

	if (pAccessories[2])
	{
		if (const SFireModeParams* pNewParams = pParentParams->FindAccessoryFireModeParams(pAccessories[0], pAccessories[1], pAccessories[2]))
		{
			return pNewParams;
		}
	}

	for (int i = 0; i < ITEM_MAX_NUM_ACCESSORIES; ++i)
	{
		for (int j = i + 1; j < ITEM_MAX_NUM_ACCESSORIES; ++j)
		{
			if (pAccessories[j])
			{
				if (const SFireModeParams* pNewParams = pParentParams->FindAccessoryFireModeParams(pAccessories[i], pAccessories[j]))
				{
					return pNewParams;
				}
			}
		}
	}

	for (int i = 0; i < ITEM_MAX_NUM_ACCESSORIES; ++i)
	{
		if (const SFireModeParams* pNewParams = pParentParams->FindAccessoryFireModeParams(pAccessories[i]))
		{
			return pNewParams;
		}
	}

	return pParentParams->pBaseFireMode;
}

const SMeleeModeParams* CWeapon::GetAccessoryAlteredMeleeParams(IEntityClass** pAccessories)
{
	if (pAccessories[3])
	{
		if (const SMeleeModeParams* pNewParams = m_weaponsharedparams->FindAccessoryMeleeParams(pAccessories[0], pAccessories[1], pAccessories[2], pAccessories[3]))
		{
			return pNewParams;
		}
	}

	if (pAccessories[2])
	{
		if (const SMeleeModeParams* pNewParams = m_weaponsharedparams->FindAccessoryMeleeParams(pAccessories[0], pAccessories[1], pAccessories[2]))
		{
			return pNewParams;
		}
	}

	for (int i = 0; i < ITEM_MAX_NUM_ACCESSORIES; ++i)
	{
		for (int j = i + 1; j < ITEM_MAX_NUM_ACCESSORIES; ++j)
		{
			if (pAccessories[j])
			{
				if (const SMeleeModeParams* pNewParams = m_weaponsharedparams->FindAccessoryMeleeParams(pAccessories[i], pAccessories[j]))
				{
					return pNewParams;
				}
			}
		}
	}

	for (int i = 0; i < ITEM_MAX_NUM_ACCESSORIES; ++i)
	{
		if (const SMeleeModeParams* pNewParams = m_weaponsharedparams->FindAccessoryMeleeParams(pAccessories[i]))
		{
			return pNewParams;
		}
	}

	return m_weaponsharedparams->pMeleeModeParams;
}

const SZoomModeParams* CWeapon::GetAccessoryAlteredZoomModeParams(CIronSight* pZoomMode, IEntityClass** pAccessories)
{
	const SParentZoomModeParams* pParentParams = pZoomMode->GetParentShared();

	if (pAccessories[3])
	{
		if (const SZoomModeParams* pNewParams = pParentParams->FindAccessoryZoomModeParams(pAccessories[0], pAccessories[1], pAccessories[2], pAccessories[3]))
		{
			return pNewParams;
		}
	}

	if (pAccessories[2])
	{
		if (const SZoomModeParams* pNewParams = pParentParams->FindAccessoryZoomModeParams(pAccessories[0], pAccessories[1], pAccessories[2]))
		{
			return pNewParams;
		}
	}

	for (int i = 0; i < ITEM_MAX_NUM_ACCESSORIES; ++i)
	{
		for (int j = i + 1; j < ITEM_MAX_NUM_ACCESSORIES; ++j)
		{
			if (pAccessories[j])
			{
				if (const SZoomModeParams* pNewParams = pParentParams->FindAccessoryZoomModeParams(pAccessories[i], pAccessories[j]))
				{
					return pNewParams;
				}
			}
		}
	}

	for (int i = 0; i < ITEM_MAX_NUM_ACCESSORIES; ++i)
	{
		if (const SZoomModeParams* pNewParams = pParentParams->FindAccessoryZoomModeParams(pAccessories[i]))
		{
			return pNewParams;
		}
	}

	return &pParentParams->baseZoomMode;
}

float CWeapon::GetZoomTimeMultiplier()
{
	float zoomTimeMultiplier = m_zoomTimeMultiplier;

	CActor* pOwner = GetOwnerActor();

	if (pOwner && pOwner->IsPlayer())
	{
		CPlayerModifiableValues& playerMods = static_cast<CPlayer*>(pOwner)->GetModifiableValues();

		// Only counteracts attachment slowdown. Therefore should be a minimum of 1.f
		zoomTimeMultiplier = max(1.f, zoomTimeMultiplier * playerMods.GetValue(kPMV_WeaponAttachment_ZoomTimeScale));

		//Now apply Weapons Training modifier
		zoomTimeMultiplier *= playerMods.GetValue(kPMV_WeaponZoomTimeScale);
	}

	return zoomTimeMultiplier;
}

//------------------------------------------------------------------------
float CWeapon::GetMuzzleFlashScale() const
{
	if (m_stats.fp && m_zm && m_zm->IsZoomed())
	{
		CIronSight* pZoomMode = static_cast<CIronSight*>(m_zm);
		return pZoomMode->GetShared()->zoomParams.muzzle_flash_scale;
	}

	return 1.0f;
}

void CWeapon::AllowDrop()
{
	m_DropAllowedFlag = true;
}

void CWeapon::DisallowDrop()
{
	m_DropAllowedFlag = false;
}

//------------------------------------------------------------------------
void CWeapon::SetHostId(EntityId hostId)
{
	m_hostId = hostId;
}

//------------------------------------------------------------------------
EntityId CWeapon::GetHostId() const
{
	return m_hostId;
}

//------------------------------------------------------------------------
void CWeapon::FixAccessories(const SAccessoryParams* params, bool attach)
{
	if (params)
	{
		if (!attach)
		{
			for (size_t i = 0; i < params->firemodes.size(); i++)
			{
				if (params->exclusive)
				{
					CFireMode* pFiremode = static_cast<CFireMode*>(GetFireMode(params->firemodes[i].c_str()));
					if (pFiremode)
					{
						pFiremode->EnableByAccessory(false);
					}
				}
			}
			if (IFireMode* pFM = GetFireMode(GetCurrentFireMode()))
			{
				if (!pFM->IsEnabled())
					ChangeFireMode();
			}

			if (GetZoomModeIdx(params->zoommode.c_str()) != -1)
			{
				EnableZoomMode(GetZoomModeIdx(params->zoommode.c_str()), false);
				if ((m_zmId == 0) || (m_zmId != m_secondaryZmId))
					ChangeZoomMode();
				m_primaryZmId = 0;
			}

			if (GetZoomModeIdx(params->zoommodeSecondary.c_str()) != -1)
			{
				m_secondaryZmId = 0;
			}
		}
		else //!attach
		{
			if (!params->switchToFireMode.empty())
			{
				SetCurrentFireMode(params->switchToFireMode.c_str());
			}
			else if (IsFiremodeDisabledByAccessory(GetCurrentFireMode()))
			{
				ChangeFireMode();
			}

			for (size_t i = 0; i < params->firemodes.size(); i++)
			{
				CFireMode* pFiremode = static_cast<CFireMode*>(GetFireMode(params->firemodes[i].c_str()));
				if (pFiremode)
				{
					if (!params->defaultAccessory)
					{
						pFiremode->EnableByAccessory(true);
					}
					else
					{
						pFiremode->Enable(true);
					}

					if (GetAmmoCount(pFiremode->GetAmmoType()) == 0)
					{
						const bool fromInventory = true;
						pFiremode->FillAmmo(fromInventory);
					}
				}
			}
			if (GetZoomModeIdx(params->zoommode.c_str()) != -1)
			{
				int zoomModeId = GetZoomModeIdx(params->zoommode.c_str());
				EnableZoomMode(zoomModeId, true);
				if ((m_zmId == 0) || (m_zmId != m_secondaryZmId))
					SetCurrentZoomMode(zoomModeId);
				m_primaryZmId = zoomModeId;
			}

			int secondaryZoommode = GetZoomModeIdx(params->zoommodeSecondary.c_str());
			if (secondaryZoommode != -1)
			{
				m_secondaryZmId = secondaryZoommode;
			}
		}

		m_pWeaponStats->SetStatsFromAccessory(params, attach);
	}

	OnSetAmmoCount(GetOwnerId());
}

//------------------------------------------------------------------------
void CWeapon::SetDestinationEntity(EntityId targetId)
{
	// default: Set bbox center as destination
	IEntity* pEntity = gEnv->pEntitySystem->GetEntity(targetId);

	if (pEntity)
	{
		AABB box;
		pEntity->GetWorldBounds(box);

		SetDestination(box.GetCenter());
	}
}

//------------------------------------------------------------------------
bool CWeapon::PredictProjectileHit(IPhysicalEntity* pShooter,
                                   const Vec3& pos, const Vec3& dir, const Vec3& launchVelocity, float speed,
                                   Vec3& predictedPosOut, float& projectileSpeedOut, Vec3* pTrajectoryPositions,
                                   unsigned int* trajectorySizeInOut, float timeStep, Vec3* pTrajectoryVelocities,
                                   const bool predictionForAI) const
{
	CRY_PROFILE_FUNCTION(PROFILE_GAME);

	IFireMode* pFireMode = GetFireMode(GetCurrentFireMode());
	if (!pFireMode)
		return false;

	IEntityClass* pAmmoType = pFireMode->GetAmmoType();
	if (!pAmmoType)
		return false;

	CProjectile* pTestProjectile = g_pGame->GetWeaponSystem()->SpawnAmmo(pAmmoType);
	if (!pTestProjectile)
		return false;
	IPhysicalEntity* pProjectilePhysEntity = pTestProjectile->GetEntity()->GetPhysics();
	if (!pProjectilePhysEntity)
		return false;

	const float projectileLaunchSpeed = pTestProjectile->GetSpeed();
	projectileSpeedOut = projectileLaunchSpeed;

	Vec3 firstAppliedVelocity(ZERO);

	pTestProjectile->SetVelocity(pos, dir, launchVelocity, speed / projectileLaunchSpeed, &firstAppliedVelocity, 1);

	pe_params_flags particleFlags;
	particleFlags.flagsAND = ~(pef_log_collisions & pef_traceable & pef_log_poststep);
	pProjectilePhysEntity->SetParams(&particleFlags, 1);

	pe_params_particle partPar;
	partPar.pColliderToIgnore = pShooter;
	if (predictionForAI)
	{
		// If the prediction is made for the AI, then we need to reset this parameters
		// to match the prediction made in the FireCommand.
		partPar.accThrust = 0.0f;
		partPar.kAirResistance = 0.0f;
	}
	pProjectilePhysEntity->SetParams(&partPar, 1);

	pe_params_pos paramsPos;
	paramsPos.iSimClass = 6;
	paramsPos.pos = pos;
	pProjectilePhysEntity->SetParams(&paramsPos, 1);

	unsigned int n = 0;
	const unsigned int maxSize = trajectorySizeInOut ? *trajectorySizeInOut : 0;

	if (pTrajectoryPositions && n < maxSize)
	{
		pTrajectoryPositions[n] = pos;

		if (pTrajectoryVelocities)
		{
			pTrajectoryVelocities[n] = firstAppliedVelocity;
		}

		++n;
	}

	const float propLifeTime = pTestProjectile->GetLifeTime();
	const float lifeTime = propLifeTime > 0.0f ? propLifeTime : 3.0f;

	uint stationary = 0;
	Vec3 lastPosition = pos;

	pProjectilePhysEntity->StartStep(lifeTime);

	pe_status_pos statusPos;
	pe_status_dynamics statusDynamics;

	for (float t = 0.0f; t < lifeTime; t += timeStep)
	{
		pProjectilePhysEntity->DoStep(timeStep);

		pProjectilePhysEntity->GetStatus(&statusPos);
		pProjectilePhysEntity->GetStatus(&statusDynamics);

		const Vec3 position = statusPos.pos;
		const Vec3 velocity = statusDynamics.v;

		float distSq = Distance::Point_PointSq(lastPosition, position);
		lastPosition = position;

		// Early out when almost stationary.
		if (distSq < sqr(0.01f))
			stationary++;
		else
			stationary = 0;
		if (stationary > 2)
			break;

		if (pTrajectoryPositions && n < maxSize)
		{
			pTrajectoryPositions[n] = position;
			if (pTrajectoryVelocities)
			{
				pTrajectoryVelocities[n] = velocity;
			}
			++n;
		}
	}

	if (trajectorySizeInOut)
		*trajectorySizeInOut = n;

	pProjectilePhysEntity->GetStatus(&statusPos);
	pTestProjectile->Destroy();

	predictedPosOut = statusPos.pos;

	return true;
}

//------------------------------------------------------------------------
const AIWeaponDescriptor& CWeapon::GetAIWeaponDescriptor() const
{
	if (!m_fm)
		return m_weaponsharedparams->aiWeaponDescriptor.descriptor;
	return static_cast<CFireMode*>(m_fm)->GetShared()->aiDescriptor.descriptor;
}

//------------------------------------------------------------------------
void CWeapon::OnDestroyed()
{
	BaseClass::OnDestroyed();

	if (m_fm)
	{
		if (m_fm->IsFiring())
			m_fm->StopFire();
	}
}

bool CWeapon::HasAttachmentAtHelper(const char* helper)
{
	CPlayer* pPlayer = static_cast<CPlayer*>(m_pGameFramework->GetClientActor());
	if (pPlayer)
	{
		IInventory* pInventory = pPlayer->GetInventory();
		if (pInventory)
		{
			const int numAccessories = m_accessories.size();

			for (int i = 0; i < numAccessories; i++)
			{
				const SAccessoryParams* params = GetAccessoryParams(m_accessories[i].pClass);
				if (params && !strcmp(params->attach_helper.c_str(), helper))
				{
					// found a child item that can be used
					return true;
				}
			}
			for (int i = 0; i < pInventory->GetAccessoryCount(); i++)
			{
				const IEntityClass* pAccessory = pInventory->GetAccessoryClass(i);

				if (pAccessory)
				{
					const SAccessoryParams* invAccessory = GetAccessoryParams(pAccessory);
					if (invAccessory && !strcmp(invAccessory->attach_helper.c_str(), helper))
					{
						// found an accessory in the inventory that can be used
						return true;
					}
				}
			}

		}
	}
	return false;
}

//--------------------------------------------------------------------------
void CWeapon::GetAttachmentsAtHelper(const char* helper, CCryFixedStringListT<5, 30>& attachments)
{
	CPlayer* pPlayer = static_cast<CPlayer*>(m_pGameFramework->GetClientActor());
	if (pPlayer)
	{
		IInventory* pInventory = pPlayer->GetInventory();
		if (pInventory)
		{
			attachments.Clear();
			for (int i = 0; i < pInventory->GetAccessoryCount(); i++)
			{
				const IEntityClass* pAccessory = pInventory->GetAccessoryClass(i);

				if (pAccessory)
				{
					const SAccessoryParams* invAccessory = GetAccessoryParams(pAccessory);
					if (invAccessory && !strcmp(invAccessory->attach_helper.c_str(), helper))
					{
						attachments.Add(pAccessory->GetName());
					}
				}
			}
		}
	}
}

//----------------------------------------------
void CWeapon::StartChangeFireMode()
{
	//Check if the weapon has enough firemodes
	if (m_fmIds.size() <= 1)
		return;
	CFireMode* pNewFiremode = static_cast<CFireMode*>(GetFireMode(GetNextFireMode(GetCurrentFireMode())));
	if (pNewFiremode == m_fm)
		return;
	if (gEnv->pTimer->GetCurrTime() < m_switchFireModeTimeStap)
		return;

	//Deactivate target display if needed
	if (m_fm && !m_fm->AllowZoom() && IsTargetOn())
		m_fm->Cancel();

	StopFire();
	SetBusy(true);
	m_switchingFireMode = true;

	const SFireModeParams* pNewParams = NULL;

	if (pNewFiremode)
	{
		pNewParams = pNewFiremode->GetShared();
	}

	PlayChangeFireModeTransition(pNewFiremode);

	GetScheduler()->TimerAction(GetCurrentAnimationTime(eIGS_Owner), CSchedulerAction<EndChangeFireModeAction>::Create(EndChangeFireModeAction(this)), false);

	SetBusy(true);
	ChangeFireMode();

	if (pNewParams && pNewParams->fireparams.changeFMFireDelayFraction > 0.f)
	{
		const uint32 endChangeTime = uint32(GetCurrentAnimationTime(eIGS_Owner) * pNewParams->fireparams.changeFMFireDelayFraction);

		GetScheduler()->TimerAction(endChangeTime, CSchedulerAction<EndChangeFireModeAction>::Create(EndChangeFireModeAction(this)), false);
	}
	else
	{
		EndChangeFireMode();
	}
}

void CWeapon::EndChangeFireMode()
{
	SetBusy(false);
	ForcePendingActions();

	m_switchingFireMode = false;
}

//-----------------------------------------------------------------
EntityId CWeapon::GetLaserAttachment() const
{
	return 0;
}

//-----------------------------------------------------------------
bool CWeapon::IsLaserAttached() const
{
	return GetLaserAttachment() != 0;
}

//-----------------------------------------------------------------
void CWeapon::ActivateLaser(bool activate)
{
	EntityId laserID = GetLaserAttachment();

	//Only if Laser is attached
	if (laserID != 0)
	{
		CLaser* pLaser = static_cast<CLaser*>(m_pItemSystem->GetItem(laserID));
		if (pLaser)
			pLaser->ActivateLaser(activate);
	}
	else
	{
		GameWarning("No Laser attached!! Laser could not be activated/deactivated");
	}
}

//------------------------------------------------------------------
bool CWeapon::IsSilent() const
{
	if (GetEntity()->GetClass() == CItem::sBowClass)
		return true;

	return (m_fm != NULL) ? m_fm->IsSilenced() : false;
}
//------------------------------------------------------------------
bool CWeapon::IsLaserActivated() const
{
	EntityId laserID = GetLaserAttachment();

	//Only if LAM is attached
	if (laserID != 0)
	{
		CLaser* pLaser = static_cast<CLaser*>(m_pItemSystem->GetItem(laserID));
		if (pLaser)
			return pLaser->IsLaserActivated();
	}

	return false;
}

//---------------------------------------------------
void CWeapon::TriggerMeleeReaction()
{
	if (m_fm)
		m_fm->CancelReload();

	const int layer = 1;

	PlayAction(GetFragmentIds().meleeReaction, layer);
	SetBusy(true);
	GetScheduler()->TimerAction(GetCurrentAnimationTime(eIGS_Owner), CSchedulerAction<MeleeReactionTimer>::Create(MeleeReactionTimer(this, layer)), false);
}

//-----------------------------------------------------
void CWeapon::StartUse(EntityId userId)
{
	BaseClass::StartUse(userId);
}

//-----------------------------------------------------
void CWeapon::StopUse(EntityId userId)
{
	BaseClass::StopUse(userId);

	if (m_stats.mounted)
	{
		if (IsZoomed() || IsZoomingInOrOut())
			ExitZoom();

		StopFire(); //Stop firing just in case
	}
}

//------------------------------------------------------
bool CWeapon::CheckAmmoRestrictions(IInventory* pInventory)
{
	if (HasCompatibleAmmo(pInventory))
	{
		//Check for accessories that give ammo
		if (CheckAmmoRestrictionsForAccessories(pInventory))
		{
			return true;
		}

		return CheckAmmoRestrictionsForBonusAndMagazineAmmo(*pInventory);
	}

	return false;
}

//-------------------------------------------------------------
int CWeapon::GetMaxZoomSteps()
{
	if (m_zm)
		return m_zm->GetMaxZoomSteps();

	return 0;
}

//----------------------------------------------------------
void CWeapon::GetMemoryUsage(ICrySizer* s) const
{
	s->AddObject(this, sizeof(*this));
	GetInternalMemoryUsage(s);
}

void CWeapon::GetInternalMemoryUsage(ICrySizer* s) const
{
	{
		SIZER_COMPONENT_NAME(s, "FireModes");
		s->AddContainer(m_fmIds);
		s->AddContainer(m_firemodes);
	}
	{
		SIZER_COMPONENT_NAME(s, "ZoomModes");
		s->AddContainer(m_zmIds);
		s->AddContainer(m_zoommodes);
	}

	{
		SIZER_COMPONENT_NAME(s, "Ammo");
		s->AddContainer(m_ammo);
		s->AddContainer(m_bonusammo);
	}

	if (m_melee)
	{
		SIZER_COMPONENT_NAME(s, "Melee");
		m_melee->GetMemoryUsage(s);
	}

	s->AddObject(m_listeners);
	CItem::GetInternalMemoryUsage(s); // collect memory of parent class
}

//------------------------------------------------------
bool CWeapon::AIUseEyeOffset() const
{
	return m_weaponsharedparams->aiWeaponOffsets.useEyeOffset;
}

//--------------------------------------------------------
bool CWeapon::AIUseOverrideOffset(EStance stance, float lean, float peekOver, Vec3& offset) const
{
	// do checks for if(found) here
	if (m_weaponsharedparams->aiWeaponOffsets.stanceWeponOffsetLeanLeft.empty() ||
	    m_weaponsharedparams->aiWeaponOffsets.stanceWeponOffsetLeanRight.empty() || m_weaponsharedparams->aiWeaponOffsets.stanceWeponOffset.empty())
		return false;

	TStanceWeaponOffset::const_iterator itrOffsetLeft(m_weaponsharedparams->aiWeaponOffsets.stanceWeponOffsetLeanLeft.find(stance));
	if (itrOffsetLeft == m_weaponsharedparams->aiWeaponOffsets.stanceWeponOffsetLeanLeft.end())
		return false;

	TStanceWeaponOffset::const_iterator itrOffsetRight(m_weaponsharedparams->aiWeaponOffsets.stanceWeponOffsetLeanRight.find(stance));
	if (itrOffsetRight == m_weaponsharedparams->aiWeaponOffsets.stanceWeponOffsetLeanRight.end())
		return false;

	TStanceWeaponOffset::const_iterator itrOffset(m_weaponsharedparams->aiWeaponOffsets.stanceWeponOffset.find(stance));
	if (itrOffset == m_weaponsharedparams->aiWeaponOffsets.stanceWeponOffset.end())
		return false;

	const Vec3& normal(itrOffset->second);
	const Vec3& lLeft(itrOffsetLeft->second);
	const Vec3& lRightt(itrOffsetRight->second);

	offset = SStanceInfo::GetOffsetWithLean(lean, peekOver, normal, lLeft, lRightt, Vec3(ZERO));

	return true;
}

//----------------------------------------------------------
bool CWeapon::FilterView(SViewParams& viewParams)
{
	bool ret = BaseClass::FilterView(viewParams);

	if (m_zm && m_zm->IsZoomed())
	{
		m_zm->FilterView(viewParams);
	}

	return ret;
}

//--------------------------------------------------
void CWeapon::PostFilterView(struct SViewParams& viewParams)
{
	BaseClass::PostFilterView(viewParams);

	if (m_zm && m_zm->IsZoomed())
		m_zm->PostFilterView(viewParams);
}

//-------------------------------------------------
void CWeapon::OnZoomIn()
{
	bool hasSniperScope = false;

	const int numAccessories = m_accessories.size();

	for (int i = 0; i < numAccessories; i++)
	{
		CItem* pItem = static_cast<CItem*>(m_pItemSystem->GetItem(m_accessories[i].accessoryId));
		if (pItem && pItem->GetParams().scopeAttachment)
		{
			if (const SAccessoryParams* params = GetAccessoryParams(m_accessories[i].pClass))
			{
				pItem->DrawSlot(eIGS_FirstPerson, false);
				ResetCharacterAttachment(eIGS_FirstPerson, params->attach_helper.c_str(), params->attachToOwner);
				pItem->DrawSlot(eIGS_Aux1, false);
				SetCharacterAttachment(eIGS_FirstPerson, params->attach_helper.c_str(), pItem->GetEntity(), eIGS_Aux1, params->attachToOwner);
				hasSniperScope = true;
			}
		}
	}

	if (!hasSniperScope)
	{
		Hide(true);
	}
}

//-------------------------------------------------
void CWeapon::OnZoomOut()
{
	bool hasSniperScope = false;

	const int numAccessories = m_accessories.size();

	for (int i = 0; i < numAccessories; i++)
	{
		CItem* pItem = static_cast<CItem*>(m_pItemSystem->GetItem(m_accessories[i].accessoryId));
		if (pItem && pItem->GetParams().scopeAttachment)
		{
			if (const SAccessoryParams* params = GetAccessoryParams(m_accessories[i].pClass))
			{
				pItem->DrawSlot(eIGS_Aux1, false);
				ResetCharacterAttachment(eIGS_FirstPerson, params->attach_helper.c_str(), params->attachToOwner);
				pItem->DrawSlot(eIGS_FirstPerson, false);
				SetCharacterAttachment(eIGS_FirstPerson, params->attach_helper.c_str(), pItem->GetEntity(), eIGS_FirstPerson, params->attachToOwner);
				hasSniperScope = true;
			}
		}
	}

	if (!hasSniperScope)
	{
		Hide(false);
	}
}
//-------------------------------------------------
void CWeapon::OnZoomedIn()
{
}

//-------------------------------------------------
void CWeapon::OnZoomedOut()
{
	ForcePendingActions();
}

//-------------------------------------------------
bool CWeapon::GetScopePosition(Vec3& pos)
{
	const int numAccessories = m_accessories.size();

	for (int i = 0; i < numAccessories; i++)
	{
		CItem* pItem = static_cast<CItem*>(m_pItemSystem->GetItem(m_accessories[i].accessoryId));
		if (pItem && pItem->GetParams().scopeAttachment)
		{
			if (const SAccessoryParams* params = GetAccessoryParams(m_accessories[i].pClass))
			{
				pos = GetSlotHelperPos(eIGS_FirstPerson, params->attach_helper.c_str(), true);
				Matrix33 rot = GetSlotHelperRotation(eIGS_FirstPerson, params->attach_helper.c_str(), true);
				Vec3 dirZ = rot.GetColumn1();
				if (pItem->GetParams().scopeAttachment == 1)
				{
					const float sniperZOfffset = 0.029f;
					pos += (sniperZOfffset * dirZ);
				}
				else if (pItem->GetParams().scopeAttachment == 2)
				{
					const float lawZOffset = -0.028f;
					const float lawXOffset = -0.017f;
					pos += (lawZOffset * dirZ);
					Vec3 dirX = rot.GetColumn2();
					pos += (lawXOffset * dirX);
				}

				return true;
			}
		}
	}
	return false;
}

//------------------------------------------------------
bool CWeapon::HasScopeAttachment() const
{
	const int numAccessories = m_accessories.size();

	for (int i = 0; i < numAccessories; i++)
	{
		CItem* pItem = static_cast<CItem*>(m_pItemSystem->GetItem(m_accessories[i].accessoryId));
		if (pItem && pItem->GetParams().scopeAttachment)
		{
			return true;
		}
	}

	return false;
}

//------------------------------------------------------
void CWeapon::SetNextShotTime(bool activate)
{
	if (activate)
	{
		// MUST BE CALLED from Select(true), after firemode activation
		// Prevent exploit fire rate by switching weapons
		if (m_fm && m_nextShotTime > 0.0f)
		{
			CTimeValue time = gEnv->pTimer->GetFrameStartTime();
			float dt = m_nextShotTime - time.GetSeconds();
			if (dt > 0.0f)
				m_fm->SetNextShotTime(dt);
			m_nextShotTime = 0.0f;
		}
	}
	else
	{
		// MUST BE CALLED from Select(false), before firemode deactivation
		// save game time when the weapon can next be fired
		m_nextShotTime = 0.0f;
		if (m_fm)
		{
			float delay = m_fm->GetNextShotTime();
			if (delay > 0.0f)
			{
				CTimeValue time = gEnv->pTimer->GetFrameStartTime();
				m_nextShotTime = time.GetSeconds() + delay;
			}
		}
	}
}

//-----------------------------------------------------
bool CWeapon::CanDrop() const
{
	return (BaseClass::CanDrop() && !IsModifying() && m_DropAllowedFlag);
}

//--------------------------------------------------------
bool CWeapon::Query(EWeaponQuery query, const void* param /*=NULL*/)
{
	switch (query)
	{
	case eWQ_Has_Accessory_Laser:
		{
			return IsLaserAttached();
		}
		break;

	case eWQ_Is_Laser_Activated:
		{
			return IsLaserActivated();
		}
		break;

	case eWQ_Activate_Laser:
		{
			if (!param)
				return false;
			ActivateLaser(*((bool*)param));
		}
		break;

	default:
		return false;

	}

	return true;
}

//--------------------------------------------------------
void CWeapon::ApplyFPViewRecoil(int nFrameId, Ang3 recoilAngles)
{
	if (m_lastRecoilUpdate == nFrameId)
		return;

	m_lastRecoilUpdate = nFrameId;

	if (CActor* pOwner = GetOwnerActor())
	{
		pOwner->AddViewAngleOffsetForFrame(recoilAngles);
	}
}

//--------------------------------------------------------
float CWeapon::GetMovementModifier() const
{
	bool firing = m_fm ? m_fm->IsFiring() : false;

	float speedScale = 1.0f;

	if (m_zm && m_zm->IsZoomed())
	{
		speedScale *= static_cast<CIronSight*>(m_zm)->GetStageMovementModifier();
	}

	if (!firing)
		return GetPlayerMovementModifiers().movementSpeedScale * speedScale;
	else
		return GetPlayerMovementModifiers().firingMovementSpeedScale * speedScale;
}

//--------------------------------------------------------
float CWeapon::GetRotationModifier(bool usingMouse) const
{
	bool firing = m_fm ? m_fm->IsFiring() : false;

	float rotModifier = 1.0f;
	if (m_zm && m_zm->IsZoomed())
	{
		rotModifier *= static_cast<CIronSight*>(m_zm)->GetStageRotationModifier();
	}

	const SPlayerMovementModifiers& modifiers = GetPlayerMovementModifiers();

	if (!firing)
		rotModifier *= usingMouse ? modifiers.mouseRotationSpeedScale : modifiers.rotationSpeedScale;
	else
		rotModifier *= usingMouse ? modifiers.mouseFiringRotationSpeedScale : modifiers.firingRotationSpeedScale;

	return rotModifier;
}

//--------------------------------------------------------
const SPlayerMovementModifiers& CWeapon::GetPlayerMovementModifiers() const
{
	if (IsZoomed() || IsZoomingIn())
	{
		assert((m_zmId >= 0) && (m_zmId < (int)m_weaponsharedparams->zoomedMovementModifiers.size()));

		return m_weaponsharedparams->zoomedMovementModifiers[m_zmId];
	}
	else
		return m_weaponsharedparams->defaultMovementModifiers;
}

//--------------------------------------------------------
bool CWeapon::ShouldSendOnShootHUDEvent() const
{
	return !m_fm->IsSilenced() && !IsProxyWeapon();
}

void CWeapon::GetAngleLimits(EStance stance, float& minAngle, float& maxAngle)
{
}

bool CWeapon::UpdateAimAnims(SParams_WeaponFPAiming& aimAnimParams)
{
	if (m_sharedparams->params.hasAimAnims)
	{
		aimAnimParams.overlayFactor = 1.0f;
		aimAnimParams.rotationFactor = 1.0f;
		aimAnimParams.strafeFactor = 1.0f;
		aimAnimParams.movementFactor = 1.0f;

		const bool zooming = IsZoomed() || IsZoomingIn();
		const CIronSight* pIronsight = static_cast<CIronSight*>(m_zm);
		if (zooming)
			aimAnimParams.overlayFactor *= m_sharedparams->params.ironsightAimAnimFactor;
		if (pIronsight)
		{
			if (zooming)
			{
				aimAnimParams.rotationFactor *= pIronsight->GetShared()->zoomParams.ironsightRotationAnimFactor;
				aimAnimParams.strafeFactor *= pIronsight->GetShared()->zoomParams.ironsightStrafeAnimFactor;
				aimAnimParams.movementFactor *= pIronsight->GetShared()->zoomParams.ironsightMovementAnimFactor;
			}
			else
			{
				aimAnimParams.rotationFactor *= pIronsight->GetShared()->zoomParams.shoulderRotationAnimFactor;
				aimAnimParams.strafeFactor *= pIronsight->GetShared()->zoomParams.shoulderStrafeAnimFactor;
				aimAnimParams.movementFactor *= pIronsight->GetShared()->zoomParams.shoulderMovementAnimFactor;
			}
		}

		IFireMode* pFireMode = GetFireMode(GetCurrentFireMode());
		aimAnimParams.shoulderLookParams =
		  pFireMode ?
		  &static_cast<CFireMode*>(pFireMode)->GetShared()->aimLookParams :
		  &m_sharedparams->params.aimLookParams;

		return true;
	}

	return false;
}

// returns a non-localised name for development purposes
const char* CWeapon::GetName()
{
	return GetEntity()->GetClass()->GetName();
}

bool CWeapon::IsOwnerSliding() const
{
	CActor* pOwner = GetOwnerActor();
	if ((pOwner != NULL) && pOwner->IsPlayer())
	{
		CPlayer* pPlayer = static_cast<CPlayer*>(pOwner);
		return pPlayer->IsSliding();
	}
	return false;
}

bool CWeapon::IsOwnerClient() const
{
	CActor* pOwner = GetOwnerActor();
	return pOwner ? pOwner->IsClient() : m_isClientOwnerOverride;
}

void CWeapon::SetOwnerId(EntityId ownerId)
{
	if (!ownerId)
	{
		IInventory* pInventory = GetActorInventory(GetOwnerActor());
		if (pInventory)
			UnregisterUsedAmmoWithInventory(pInventory);
		CItem::SetOwnerId(ownerId);
	}
	else
	{
		CItem::SetOwnerId(ownerId);
		IInventory* pInventory = GetActorInventory(GetOwnerActor());
		if (pInventory)
			RegisterUsedAmmoWithInventory(pInventory);
	}
}

void CWeapon::SetOwnerClientOverride(bool isClient)
{
	m_isClientOwnerOverride = isClient;
}

bool CWeapon::IsCurrentFireModeFromAccessory() const
{
	if (!m_fm)
		return false;

	IEntityClass* pDefaultAmmo = 0;
	if (!m_ammo.empty())
		pDefaultAmmo = m_ammo[0].pAmmoClass;

	const SParentFireModeParams* pParentParams = static_cast<CFireMode*>(m_fm)->GetParentShared();
	const SFireModeParams* pParams = static_cast<CFireMode*>(m_fm)->GetShared();
	if (!pParentParams)
		return false;

	bool onlyEnabledFromAccessory =
	  (pParentParams->initialiseParams.enabled == false) &&
	  pParams->fireparams.ammo_type_class != pDefaultAmmo;

	return onlyEnabledFromAccessory;
}

void CWeapon::OnFireWhenOutOfAmmo()
{
	if (IsReloading())
		return;

	if (m_fm && !m_fm->GetShared()->fireparams.autoReload)
	{
		SHUDEvent noAmmoEvent(eHUDEvent_ShowNoAmmoWarning);
		noAmmoEvent.AddData(false); // dropping
		CHUDEventDispatcher::CallEvent(noAmmoEvent);
		return;
	}

	const bool stillHasAmmo = !OutOfAmmoTypes();

	if (CanReload())
		Reload();
	else if (IsCurrentFireModeFromAccessory())
	{
		StartChangeFireMode();
		m_weaponNextShotTimer = 0.f;
	}
	else if (!stillHasAmmo)
		OutOfAmmoDeselect();
	else
		OutOfAmmoType();
}

void CWeapon::OutOfAmmoDeselect()
{
	if (!CanDeselect())
		return;

	CActor* actor = GetOwnerActor();
	CPlayer* player = (actor && actor->IsPlayer()) ? (CPlayer*)actor : NULL;
	if (!player || player->CanSwitchItems())
	{
		AutoSelectNextItem();
	}
	if (player && player->IsClient())
	{
		SHUDEvent noAmmoEvent(eHUDEvent_ShowNoAmmoWarning);
		noAmmoEvent.AddData(true); // dropping
		CHUDEventDispatcher::CallEvent(noAmmoEvent);
	}
}

void CWeapon::OutOfAmmoType()
{
	SHUDEventWrapper::DisplayInfo(eInfo_Warning, 3.0f, "@hud_out_of_ammo");
	if (!OutOfAmmoTypes() && !gEnv->bMultiplayer)
	{
		SHUDEventWrapper::InteractionRequest(true, "@ui_interaction_changeammotypes", "menu_open_customizeweapon", "singleplayer", 3.0f);
	}
}

void CWeapon::AutoSelectNextItem()
{
	CActor* pOwner = GetOwnerActor();
	const bool isClient = pOwner && pOwner->IsClient();

	if (isClient)
	{
		ClearInputFlags();

		if (IsRippedOff())
		{
			pOwner->SelectNextItem(1, true, eICT_Primary | eICT_Secondary);
		}
		else if (!IsMounted())
		{
			IInventory* pInventory = pOwner->GetInventory();
			IItemSystem* pItemSystem = m_pGameFramework->GetIItemSystem();

			EntityId primaryWeaponId = 0;
			EntityId secondaryWeaponId = 0;

			int numItems = pInventory->GetCount();
			for (int i = 0; i < numItems; ++i)
			{
				EntityId itemId = pInventory->GetItem(i);
				IItem* pItem = pItemSystem->GetItem(itemId);
				if (pItem)
				{
					CWeapon* pWeapon = static_cast<CWeapon*>(pItem->GetIWeapon());
					if (pWeapon && !pWeapon->OutOfAmmo(false))
					{
						// Weapon is a candidate for switching to
						const char* const pWeaponCategory = m_pItemSystem->GetItemCategory(pWeapon->GetEntity()->GetClass()->GetName());
						if (!stricmp(pWeaponCategory, "primary"))
						{
							primaryWeaponId = pWeapon->GetEntityId();
							break;
						}
						else if (!secondaryWeaponId && !stricmp(pWeaponCategory, "secondary"))
						{
							secondaryWeaponId = pWeapon->GetEntityId();
						}
					}
				}
			}

			EntityId itemToSelect = primaryWeaponId ? primaryWeaponId : secondaryWeaponId;
			if (itemToSelect)
			{
				pOwner->ScheduleItemSwitch(itemToSelect, true);

				if (itemToSelect == secondaryWeaponId && OutOfAmmoTypes())
				{
					NET_BATTLECHATTER(BC_SecondaryWeapon, pOwner);
				}
			}
		}
	}

}

//////////////////////////////////////////////////////////////////////////
void CWeapon::PickUpAmmo(EntityId pickerId)
{
	static TAmmoVector& collectedAmmo = s_tmpCollectedAmmo;
	collectedAmmo.reserve(INITIAL_COLLECTED_AMMO_RESERVE);

	IActor* pPickerActor = m_pGameFramework->GetIActorSystem()->GetActor(pickerId);
	IInventory* pInventory = pPickerActor ? pPickerActor->GetInventory() : NULL;

	CRY_ASSERT(pPickerActor);
	CRY_ASSERT(pInventory);

	if (!pInventory)
		return;

	int pickedUpAmmo = 0;

	const int bonusAmmoTypeCount = m_bonusammo.size();
	int bonusAmmoTypeEmptied = 0;
	int iTotalRemainingAmmo = 0;

	TAmmoVector::iterator bonusAmmoEndIt = m_bonusammo.end();
	for (TAmmoVector::iterator bonusAmmoIt = m_bonusammo.begin(); bonusAmmoIt != bonusAmmoEndIt; ++bonusAmmoIt)
	{
		IEntityClass* pAmmoClass = bonusAmmoIt->pAmmoClass;
		const int bonusAmmoCount = bonusAmmoIt->count;
		const int currentAmmoCount = pInventory->GetAmmoCount(pAmmoClass);
		const int maxAmmoCapacity = pInventory->GetAmmoCapacity(pAmmoClass);

		if (bonusAmmoCount > 0)
		{
			if ((bonusAmmoCount + currentAmmoCount) >= maxAmmoCapacity)
			{
				const int remainingBonusAmmo = bonusAmmoCount - (maxAmmoCapacity - currentAmmoCount);
				bonusAmmoIt->count = remainingBonusAmmo;
				int pickedRemainingAmmo = (maxAmmoCapacity - currentAmmoCount);
				pickedUpAmmo += pickedRemainingAmmo;
				iTotalRemainingAmmo += remainingBonusAmmo;

				if (pickedRemainingAmmo > 0)
					collectedAmmo.push_back(SWeaponAmmo(pAmmoClass, maxAmmoCapacity));
			}
			else
			{
				bonusAmmoTypeEmptied++;
				bonusAmmoIt->count = 0;
				pickedUpAmmo += (bonusAmmoCount);

				collectedAmmo.push_back(SWeaponAmmo(pAmmoClass, currentAmmoCount + bonusAmmoCount));
			}
		}
	}

	// Pick magazine one, only if I have this weapon (not only compatible)
	bool pickerHasThisWeapon = false;
	EntityId otherItemEntityId = pInventory->GetItemByClass(GetEntity()->GetClass());
	if (otherItemEntityId != 0)
	{
		CItem* pOtherItem = static_cast<CItem*>(m_pItemSystem->GetItem(otherItemEntityId));
		if (IsIdentical(pOtherItem))
			pickerHasThisWeapon = true;
	}

	if (pickerHasThisWeapon)
	{
		TAmmoVector::iterator ammoEndCit = m_ammo.end();
		for (TAmmoVector::iterator ammoCit = m_ammo.begin(); ammoCit != ammoEndCit; ++ammoCit)
		{
			IEntityClass* pAmmoClass = ammoCit->pAmmoClass;
			const int magazineAmmoCount = ammoCit->count;

			if (magazineAmmoCount > 0)
			{
				int collectedIndex = -1;
				int count = 0;

				TAmmoVector::iterator collectedAmmoEndCit = collectedAmmo.end();
				for (TAmmoVector::iterator collectedAmmoCit = collectedAmmo.begin(); collectedAmmoCit != collectedAmmoEndCit; ++collectedAmmoCit)
				{
					if (collectedAmmoCit->pAmmoClass == pAmmoClass)
					{
						collectedIndex = count;
						break;
					}
					count++;
				}

				const int currentAmmoCount = collectedIndex >= 0 ? collectedAmmo[collectedIndex].count : pInventory->GetAmmoCount(pAmmoClass);
				const int maxAmmoCapacity = pInventory->GetAmmoCapacity(pAmmoClass);

				if ((magazineAmmoCount + currentAmmoCount) >= maxAmmoCapacity)
				{
					const int remainingMagazineAmmo = magazineAmmoCount - (maxAmmoCapacity - currentAmmoCount);
					ammoCit->count = remainingMagazineAmmo;
					pickedUpAmmo += (maxAmmoCapacity - currentAmmoCount);
					iTotalRemainingAmmo += remainingMagazineAmmo;

					if (collectedIndex >= 0)
					{
						collectedAmmo[collectedIndex].count = maxAmmoCapacity;
					}
					else
					{
						collectedAmmo.push_back(SWeaponAmmo(pAmmoClass, maxAmmoCapacity));
					}
				}
				else
				{
					ammoCit->count = 0;
					pickedUpAmmo += magazineAmmoCount;

					if (collectedIndex >= 0)
					{
						collectedAmmo[collectedIndex].count = currentAmmoCount + magazineAmmoCount;
					}
					else
					{
						collectedAmmo.push_back(SWeaponAmmo(pAmmoClass, currentAmmoCount + magazineAmmoCount));
					}
				}
			}
		}
	}

	bool ammoChanged = false;
	TAmmoVector::iterator collectedAmmoEndCit = collectedAmmo.end();
	for (TAmmoVector::iterator collectedAmmoCit = collectedAmmo.begin(); collectedAmmoCit != collectedAmmoEndCit; ++collectedAmmoCit)
	{
		ammoChanged |= SetInventoryAmmoCountInternal(pInventory, collectedAmmoCit->pAmmoClass, collectedAmmoCit->count);
	}

	if (IsServer() && ammoChanged)
	{
		pPickerActor->GetGameObject()->InvokeRMIWithDependentObject(CActor::ClPickUp(), CActor::PickItemParams(GetEntityId(), m_stats.selected, true, true), eRMI_ToAllClients | eRMI_NoLocalCalls, GetEntityId());
	}

	if (ammoChanged && !collectedAmmo.empty())
	{
		PlayAction(GetFragmentIds().pickedup_ammo, 0, false, eIPAF_Default);
	}

	collectedAmmo.clear();

	// Finally check if we run out of bonus and/or magazine ammo
	bool clearBonusAmmo = (bonusAmmoTypeCount > 0) && (bonusAmmoTypeEmptied == bonusAmmoTypeCount);
	if (clearBonusAmmo)
	{
		m_bonusammo.clear();
	}

	//In SP, if we have this weapon, as soon as we get any ammo from it, it's deleted
	if (!gEnv->bMultiplayer)
	{
		if ((pickerHasThisWeapon) && (pickedUpAmmo > 0))
		{
			RemoveEntity();
		}
	}
	else
	{
		if (iTotalRemainingAmmo == 0)
		{
			RemoveEntity();
		}
	}
}

//////////////////////////////////////////////////////////////////////////
bool CWeapon::HasSomeAmmoToPickUp(EntityId pickerId) const
{
	IActor* pPickerActor = m_pGameFramework->GetIActorSystem()->GetActor(pickerId);
	IInventory* pInventory = pPickerActor ? pPickerActor->GetInventory() : NULL;

	CRY_ASSERT(pPickerActor);
	CRY_ASSERT(pInventory);

	if (pInventory)
	{
		bool anyBonusAmmoLeft = !m_bonusammo.empty();

		if (anyBonusAmmoLeft)
		{
			return true;
		}

		bool pickerHasThisWeapon = pInventory->GetCountOfClass(GetEntity()->GetClass()->GetName()) != 0;
		if (pickerHasThisWeapon)
		{
			TAmmoVector::const_iterator ammoEndCit = m_ammo.end();
			for (TAmmoVector::const_iterator ammoCit = m_ammo.begin(); ammoCit != ammoEndCit; ++ammoCit)
			{
				if (ammoCit->count > 0)
					return true;
			}
		}
	}

	return false;
}

//////////////////////////////////////////////////////////////////////////
ColorF CWeapon::GetSilhouetteColor() const
{
	return HasSomeAmmoToPickUp(m_pGameFramework->GetClientActorId()) ? CHUDUtils::GetHUDColor() : ColorF(0.89411f, 0.10588f, 0.10588f, 1.0f);
}

//////////////////////////////////////////////////////////////////////////
void CWeapon::RegisterUsedAmmoWithInventory(IInventory* pInventory)
{
	if (pInventory && (m_isRegisteredAmmoWithInventory == false))
	{
		TAmmoVector::const_iterator ammoMapEndCit = m_ammo.end();
		for (TAmmoVector::const_iterator ammoCit = m_ammo.begin(); ammoCit != ammoMapEndCit; ++ammoCit)
		{
			pInventory->AddAmmoUser(ammoCit->pAmmoClass);
		}

		m_isRegisteredAmmoWithInventory = true;
	}
}

//////////////////////////////////////////////////////////////////////////
void CWeapon::UnregisterUsedAmmoWithInventory(IInventory* pInventory)
{
	if (pInventory && (m_isRegisteredAmmoWithInventory == true))
	{
		TAmmoVector::const_iterator ammoMapEndCit = m_ammo.end();
		for (TAmmoVector::const_iterator ammoCit = m_ammo.begin(); ammoCit != ammoMapEndCit; ++ammoCit)
		{
			pInventory->RemoveAmmoUser(ammoCit->pAmmoClass);
		}

		m_isRegisteredAmmoWithInventory = false;
	}
}

//////////////////////////////////////////////////////////////////////////
bool CWeapon::HasCompatibleAmmo(IInventory* pInventory) const
{
	if (pInventory)
	{
		TAmmoVector::const_iterator ammoMapEndCit = m_ammo.end();
		for (TAmmoVector::const_iterator ammoCit = m_ammo.begin(); ammoCit != ammoMapEndCit; ++ammoCit)
		{
			// Some weapon in the inventory uses a compatible ammo type, so it can be picked up
			if (pInventory->GetNumberOfUsersForAmmo(ammoCit->pAmmoClass) > 0)
			{
				return true;
			}
		}
	}

	return false;
}

//////////////////////////////////////////////////////////////////////////
bool CWeapon::CheckAmmoRestrictionsForAccessories(IInventory* pInventory) const
{
	const int numAccessories = m_accessories.size();

	for (int i = 0; i < numAccessories; i++)
	{
		if (CItem* pItem = static_cast<CItem*>(m_pItemSystem->GetItem(m_accessories[i].accessoryId)))
		{
			if (pItem->GivesAmmo() && pItem->CheckAmmoRestrictions(pInventory))
			{
				return true;
			}
		}
	}

	return false;
}

//////////////////////////////////////////////////////////////////////////
bool CWeapon::CheckAmmoRestrictionsForBonusAndMagazineAmmo(IInventory& inventory) const
{
	if (!m_bonusammo.empty())
	{
		const TAmmoVector::const_iterator bonusAmmoEndCit = m_bonusammo.end();
		for (TAmmoVector::const_iterator bonusAmmoCit = m_bonusammo.begin(); bonusAmmoCit != bonusAmmoEndCit; ++bonusAmmoCit)
		{
			IEntityClass* pAmmoClass = bonusAmmoCit->pAmmoClass;
			const int invAmmo = inventory.GetAmmoCount(pAmmoClass);
			const int invLimit = inventory.GetAmmoCapacity(pAmmoClass);

			if (invAmmo < invLimit)
			{
				return true;
			}
		}
	}

	if (!m_ammo.empty())
	{
		const TAmmoVector& accesoryAmmoMap = m_weaponsharedparams->ammoParams.accessoryAmmo;
		const TAmmoVector::const_iterator ammoEndCit = m_ammo.end();

		for (TAmmoVector::const_iterator ammoCit = m_ammo.begin(); ammoCit != ammoEndCit; ++ammoCit)
		{
			IEntityClass* pAmmoClass = ammoCit->pAmmoClass;
			const int invAmmo = inventory.GetAmmoCount(pAmmoClass);
			const int invLimit = inventory.GetAmmoCapacity(pAmmoClass);

			if (invAmmo < invLimit)
			{
				return true;
			}
		}
	}

	return false;
}

void CWeapon::ShowDebugInfo()
{
#if !defined(_RELEASE)

	IActor* pClientActor = m_pGameFramework->GetClientActor();
	IInventory* pInventory = pClientActor ? pClientActor->GetInventory() : NULL;

	if (!pInventory)
	{
		GameWarning("Can not display debug info for item, client actor or his inventory is missing");
		return;
	}

	IRenderAuxGeom* pRenderAux = gEnv->pAuxGeomRenderer;

	SAuxGeomRenderFlags oldFlags = pRenderAux->GetRenderFlags();
	SAuxGeomRenderFlags newFlags = e_Def3DPublicRenderflags;
	newFlags.SetAlphaBlendMode(e_AlphaBlended);
	newFlags.SetDepthTestFlag(e_DepthTestOff);
	newFlags.SetCullMode(e_CullModeNone);
	pRenderAux->SetRenderFlags(newFlags);

	const Vec3 baseText = GetWorldPos();
	const Vec3 textLineOffset(0.0f, 0.0f, 0.14f);
	const float textColor[4] = { 1.0f, 1.0f, 1.0f, 1.0f };
	float lineCounter = 0.0f;

	IEntityClass* pWeaponClass = GetEntity()->GetClass();

	bool pickerHasThisWeapon = pInventory->GetCountOfClass(pWeaponClass->GetName()) != 0;

	IRenderAuxText::DrawLabelExF(baseText, 1.25f, textColor, true, false, pickerHasThisWeapon ? "'Weapon (%s) in inventory" : "Weapon (%s) NOT in inventory", pWeaponClass->GetName());

	lineCounter += 2.0f;
	IRenderAuxText::DrawLabelEx(baseText - (textLineOffset * lineCounter), 1.25f, textColor, true, false, "---- Bonus Ammo -------");

	TAmmoVector::iterator bonusAmmoEndIt = m_bonusammo.end();
	for (TAmmoVector::iterator bonusAmmoIt = m_bonusammo.begin(); bonusAmmoIt != bonusAmmoEndIt; ++bonusAmmoIt)
	{
		const SWeaponAmmo& bonusAmmo = *bonusAmmoIt;
		if (bonusAmmo.pAmmoClass)
		{
			lineCounter += 1.0f;
			IRenderAuxText::DrawLabelExF(baseText - (textLineOffset * lineCounter), 1.25f, textColor, true, false, "  Ammo: '%s' - %d", bonusAmmo.pAmmoClass->GetName(), bonusAmmo.count);
		}
	}

	lineCounter += 2.0f;
	IRenderAuxText::DrawLabelEx(baseText - (textLineOffset * lineCounter), 1.25f, textColor, true, false, "---- Magazine Ammo -------");

	TAmmoVector::iterator ammoEndCit = m_ammo.end();
	for (TAmmoVector::iterator ammoCit = m_ammo.begin(); ammoCit != ammoEndCit; ++ammoCit)
	{
		const SWeaponAmmo& ammo = *ammoCit;
		if (ammo.pAmmoClass)
		{
			lineCounter += 1.0f;
			IRenderAuxText::DrawLabelExF(baseText - (textLineOffset * lineCounter), 1.25f, textColor, true, false, "  Ammo: '%s' - %d", ammo.pAmmoClass->GetName(), ammo.count);
		}

	}

	pRenderAux->SetRenderFlags(oldFlags);

#endif
}

float CWeapon::GetMeleeRange() const
{
	if (m_melee)
	{
		return m_melee->GetRange();
	}

	return 0.f;
}

float CWeapon::GetSelectSpeed(CActor* pOwnerActor)
{
	float speedOverride = m_selectSpeedMultiplier;

	if (pOwnerActor && pOwnerActor->IsPlayer())
	{
		CPlayerModifiableValues& playerMods = static_cast<CPlayer*>(pOwnerActor)->GetModifiableValues();

		// Only counteracts attachment slowdown. Therefore should be a maximum of 1.f
		speedOverride = min(1.f, speedOverride * playerMods.GetValue(kPMV_WeaponAttachment_SelectSpeedScale));

		//Now apply Weapons Training modifier
		speedOverride *= playerMods.GetValue(kPMV_WeaponSelectSpeedScale);
	}

	return speedOverride;
}

void CWeapon::AddShootHeatPulse(CActor* pOwnerActor, const float heatWeapon, const float weaponHeatTime, const float heatOwner, const float ownerHeatTime)
{
	m_heatController.AddHeatPulse(heatWeapon, weaponHeatTime);

	RequireUpdate(eIUS_General);

	if (pOwnerActor)
	{
		pOwnerActor->AddHeatPulse(heatOwner, ownerHeatTime);
	}
}

void CWeapon::GetFPOffset(QuatT& offset) const
{
	if (m_zm)
	{
		if (m_zm->GetCurrentStep() > 1)
		{
			m_zm->GetFPOffset(offset);
		}
		else if (m_zm->IsZoomingInOrOut())
		{
			float ironSightWeight = m_zm->GetZoomTransition();

			//--- Apply a smoothed non-linear blend between OFFSET_BLEND_FACTOR & 1-OFFSET_BLEND_FACTOR, to mask any
			//--- motion as well as possible
			const float OFFSET_BLEND_FACTOR = 0.15f;

			ironSightWeight = (ironSightWeight - OFFSET_BLEND_FACTOR) / (1.0f - (OFFSET_BLEND_FACTOR * 2.0f));
			ironSightWeight = clamp_tpl(ironSightWeight, 0.0f, 1.0f);

			ironSightWeight = sin_tpl(gf_PI * 0.5f * ironSightWeight);
			ironSightWeight *= ironSightWeight;

			QuatT ironSightOffset;
			QuatT weaponOffset(m_sharedparams->params.fp_rot_offset, m_sharedparams->params.fp_offset);
			m_zm->GetFPOffset(ironSightOffset);

			offset.q.SetSlerp(weaponOffset.q, ironSightOffset.q, ironSightWeight);
			offset.t.SetLerp(weaponOffset.t, ironSightOffset.t, ironSightWeight);
		}
		else if (m_zm->IsZoomed())
		{
			m_zm->GetFPOffset(offset);
		}
		else
		{
			offset.t = m_sharedparams->params.fp_offset;
			offset.q = m_sharedparams->params.fp_rot_offset;
		}
	}
	else
	{
		offset.t = m_sharedparams->params.fp_offset;
		offset.q = m_sharedparams->params.fp_rot_offset;
	}
}

void CWeapon::SetToDefaultFireModeIfNeeded(const CActor& ownerActor)
{
	//Skip if holstering
	if (ownerActor.GetHolsteredItemId() == GetEntityId())
		return;

	bool autoSwitch = m_fm ? static_cast<CFireMode*>(m_fm)->GetShared()->fireparams.autoSwitch : true;
	if (m_fmIds.size() && (m_firemode != 0) && autoSwitch)
	{
		SetCurrentFireMode(0);
	}
}

bool CWeapon::CanRefillAmmoType(IEntityClass* pAmmoType, const char* refillType) const
{
	if (refillType != 0)
	{
		const SAmmoParams* pParams = g_pGame->GetWeaponSystem()->GetAmmoParams(pAmmoType);
		const char* ammoCategory = pParams->ammo_category.c_str();
		bool isSameCategory = strcmpi(refillType, ammoCategory) == 0;
		return isSameCategory;
	}
	return true;
}

bool CWeapon::RefillAllAmmo(const char* refillType, bool refillAll)
{
	bool ammoCollected = false;

	IInventory* pInventory = GetActorInventory(GetOwnerActor());
	if (!pInventory)
		return ammoCollected;

	size_t numAmmoTypes = GetWeaponSharedParams()->ammoParams.ammo.size();
	for (size_t i = 0; i < numAmmoTypes; ++i)
	{
		IEntityClass* pAmmoTypeClass = GetWeaponSharedParams()->ammoParams.ammo[i].pAmmoClass;
		CFireMode* pFireMode = FindFireModeForAmmoType(pAmmoTypeClass);
		if ((refillAll || CanRefillAmmoType(pAmmoTypeClass, refillType)) && RefillInventoryAmmo(pInventory, pAmmoTypeClass, pFireMode))
			ammoCollected = true;
	}

	const TAccessoryParamsVector& allAccessories = GetAccessoriesParamsVector();
	const int numAccessoryParams = allAccessories.size();
	for (int i = 0; i < numAccessoryParams; ++i)
	{
		IEntityClass* pAccessoryClass = allAccessories[i].pAccessoryClass;
		CGameSharedParametersStorage* pGameParamsStorage = g_pGame->GetGameSharedParametersStorage();
		CItemSharedParams* pItemSharedParams = pGameParamsStorage->GetItemSharedParameters(pAccessoryClass->GetName(), false);
		if (pItemSharedParams)
		{
			TAccessoryAmmoMap::iterator begin = pItemSharedParams->accessoryAmmoCapacity.begin();
			TAccessoryAmmoMap::iterator end = pItemSharedParams->accessoryAmmoCapacity.end();
			TAccessoryAmmoMap::iterator it;
			for (it = begin; it != end; ++it)
			{
				IEntityClass* pAmmoTypeClass = it->first;
				CFireMode* pFireMode = FindFireModeForAmmoType(pAmmoTypeClass);
				if ((refillAll || CanRefillAmmoType(pAmmoTypeClass, refillType)) && RefillInventoryAmmo(pInventory, pAmmoTypeClass, pFireMode))
					ammoCollected = true;
			}
		}
	}

	return ammoCollected;
}

CFireMode* CWeapon::FindFireModeForAmmoType(IEntityClass* pAmmoType) const
{
	bool foundSameAmmoType = false;
	for (size_t i = 0; i < m_firemodes.size(); ++i)
	{
		CFireMode* pFireMode = m_firemodes[i];
		if (pFireMode->GetAmmoType() == pAmmoType)
		{
			if (pFireMode->IsEnabled())
				return pFireMode;
			foundSameAmmoType = true;
		}
	}
	if (!foundSameAmmoType)
	{
		for (size_t i = 0; i < m_firemodes.size(); ++i)
		{
			CFireMode* pFireMode = m_firemodes[i];
			if (pFireMode->IsEnabled())
				return pFireMode;
		}
	}
	return 0;
}

bool CWeapon::RefillInventoryAmmo(IInventory* pInventory, IEntityClass* pAmmoTypeClass, CFireMode* pFireMode)
{
	bool ammoCollected = false;

	const int ammoTypeCount = pInventory->GetAmmoCount(pAmmoTypeClass);
	const int ammoTypeCapacity = pInventory->GetAmmoCapacity(pAmmoTypeClass);
	const int currentClipCapacity = pFireMode ? pFireMode->GetClipSize() + pFireMode->GetChamberSize() : 0;
	const int currentClipCount = GetAmmoCount(pAmmoTypeClass);

	if (ammoTypeCount < ammoTypeCapacity)
	{
		SetInventoryAmmoCount(pAmmoTypeClass, ammoTypeCapacity);
		ammoCollected = true;
	}

	if (currentClipCount < currentClipCapacity)
	{
		SetAmmoCount(pAmmoTypeClass, currentClipCapacity);
		ammoCollected = true;

		const int difference = currentClipCapacity - currentClipCount;
		SHUDEvent eventPickUp(eHUDEvent_OnAmmoPickUp);
		eventPickUp.AddData(SHUDEventData((void*)GetIWeapon()));
		eventPickUp.AddData(SHUDEventData(difference));
		eventPickUp.AddData(SHUDEventData((void*)pAmmoTypeClass));
		CHUDEventDispatcher::CallEvent(eventPickUp);
	}

	return ammoCollected;
}

void CWeapon::BoostMelee(bool enableBoost)
{
}

void CWeapon::UpdateBulletBelt()
{
	const SBulletBeltParams& beltParams = m_weaponsharedparams->bulletBeltParams;
	if (beltParams.numBullets <= 0)
		return;

	if (!m_fm)
		return;
	const SFireModeParams& fireModeParams = *static_cast<CFireMode*>(m_fm)->GetShared();

	IEntityClass* pAmmoClass = beltParams.pAmmoClass;
	if (!pAmmoClass)
		pAmmoClass = fireModeParams.fireparams.ammo_type_class;

	int ammoCount = -1;
	if (fireModeParams.fireparams.clip_size > 0)
		ammoCount = GetAmmoCount(pAmmoClass);
	else if (fireModeParams.fireparams.clip_size == 0)
		ammoCount = GetInventoryAmmoCount(pAmmoClass);

	if ((ammoCount < beltParams.numBullets) && (ammoCount >= 0))
	{
		ICharacterInstance* pCharacter = GetEntity()->GetCharacter(eIGS_FirstPerson);

		if (pCharacter)
		{
			if (m_fm && m_fm->GetAmmoType() == beltParams.pAmmoClass && m_fm->IsReloading())
			{
				if (m_refillBelt)
				{
					return;
				}
			}
			else
			{
				m_refillBelt = false;
			}

			//			ISkeletonPose* pSkelPose = pCharacter->GetISkeletonPose();
			IDefaultSkeleton& rIDefaultSkeleton = pCharacter->GetIDefaultSkeleton();
			{
				CryFixedStringT<32> jointName;
				const int bulletNum = max(ammoCount, 1);

				jointName.Format("%s%d", beltParams.jointName.c_str(), bulletNum);

				const int16 jointId = rIDefaultSkeleton.GetJointIDByName(jointName.c_str());

				CRY_ASSERT_TRACE(jointId >= 0, ("Invalid joint name '%s' in bullet belt params of %s belonging to %s", jointName.c_str(), GetEntity()->GetEntityTextDescription().c_str(), GetOwnerActor() ? GetOwnerActor()->GetEntity()->GetEntityTextDescription().c_str() : "nobody"));

				if (jointId >= 0)
				{
					IAnimationOperatorQueue* beltModifier = m_BeltModifier.get();
					beltModifier->PushPosition(jointId, IAnimationOperatorQueue::eOp_Override, Vec3(0.0f, -2.0f, 2.0f));

					pCharacter->GetISkeletonAnim()->PushPoseModifier(1, cryinterface_cast<IAnimationPoseModifier>(m_BeltModifier), "BulletBelt");
				}
			}
		}
	}
}

bool CWeapon::AllowInteraction(EntityId interactionEntity, EInteractionType interactionType)
{
	return (interactionType == eInteraction_PickupAmmo) || (!IsBusy() || IsReloading()) && !IsModifying() && !IsDeselecting();
}

bool CWeapon::CanLedgeGrab() const
{
	return GetSharedItemParams()->params.can_ledge_grab;
}

//------------------------------------------------------------------------
void CWeapon::SetFragmentTags(CTagState& fragTags)
{
	if (m_fm)
	{
		int ammoCount = m_fm->GetAmmoCount();
		SetAmmoCountFragmentTags(fragTags, ammoCount);
	}
}

//------------------------------------------------------------------------
void CWeapon::SetAmmoCountFragmentTags(CTagState& fragTags, int ammoCount)
{
	if (ammoCount > 1)
	{
		TagID clipRemaining = fragTags.GetDef().Find(CItem::sFragmentTagCRCs.ammo_clipRemaining);
		if (clipRemaining != TAG_ID_INVALID)
		{
			fragTags.Set(clipRemaining, true);
		}
	}

	TagID ammoCountID = TAG_ID_INVALID;

	switch (ammoCount)
	{
	case 0:
		ammoCountID = fragTags.GetDef().Find(CItem::sFragmentTagCRCs.ammo_empty);
		break;

	case 1:
		ammoCountID = fragTags.GetDef().Find(CItem::sFragmentTagCRCs.ammo_last1);
		break;

	case 2:
		ammoCountID = fragTags.GetDef().Find(CItem::sFragmentTagCRCs.ammo_last2);
		break;

	case 3:
		ammoCountID = fragTags.GetDef().Find(CItem::sFragmentTagCRCs.ammo_last3);
		break;

	default:
		break;
	}

	if (ammoCountID != TAG_ID_INVALID)
	{
		fragTags.Set(ammoCountID, true);
	}

	const int numAccessories = m_accessories.size();
	for (int i = 0; i < numAccessories; i++)
	{
		CItem* pAccessory = static_cast<CItem*>(m_pItemSystem->GetItem(m_accessories[i].accessoryId));
		if (pAccessory)
			pAccessory->SetAccessoryReloadTags(fragTags);
	}
}

//------------------------------------------------------------------------
void CWeapon::HighlightWeapon(bool highlight, bool fromDrop /*=false */)
{
	if (highlight != m_bIsHighlighted)
	{
		m_bIsHighlighted = highlight;
	}
}

//------------------------------------------------------------------------
void CWeapon::Use(EntityId userId)
{
	CItem::Use(userId);
}

//------------------------------------------------------------------------
void CWeapon::UpdateCurrentActionController()
{
	CItem::UpdateCurrentActionController();

	const bool usingStandAloneActionController = (m_pItemActionController && m_pCurrentActionController == m_pItemActionController);
	if (usingStandAloneActionController)
	{
		if (m_fm)
			m_fm->UpdateMannequinTags(true);
	}
}

//------------------------------------------------------------------------
void CWeapon::PlayChangeFireModeTransition(CFireMode* pNewFiremode)
{
	if (m_fm && pNewFiremode != m_fm)
	{
		const CTagDefinition* pTagDefinition = NULL;
		int fragmentID = GetFragmentID("change_firemode", &pTagDefinition);
		if (fragmentID != FRAGMENT_ID_INVALID)
		{
			TagState actionTags = TAG_STATE_EMPTY;
			if (pTagDefinition && pNewFiremode)
			{
				CTagState fragTags(*pTagDefinition);

				const int sz = 64;
				CryFixedStringT<sz> sourceName = static_cast<CFireMode*>(m_fm)->GetShared()->fireparams.tag.c_str();
				sourceName = CryFixedStringT<sz>("from_") + sourceName;

				CryFixedStringT<sz> targetName = pNewFiremode->GetShared()->fireparams.tag.c_str();
				targetName = CryFixedStringT<sz>("to_") + targetName;

				TagID sourceId = fragTags.GetDef().Find(sourceName.c_str());
				TagID targetId = fragTags.GetDef().Find(targetName.c_str());

				fragTags.Set(sourceId, true);
				fragTags.Set(targetId, true);

				actionTags = fragTags.GetMask();
			}

			CItemAction* pAction = new CItemAction(PP_PlayerAction, fragmentID, actionTags);
			PlayFragment(pAction);
		}
	}
}

//------------------------------------------------------------------------
void CWeapon::OnHostMigrationCompleted()
{
	CFireMode* pFireMode = (CFireMode*) GetFireMode(GetCurrentFireMode());
	if (pFireMode)
	{
		pFireMode->OnHostMigrationCompleted();
	}

	m_netInitialised = false;
	m_shootCounter = 0;
}
