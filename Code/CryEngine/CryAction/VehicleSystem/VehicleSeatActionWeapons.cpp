// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"

#include "VehicleSeatActionWeapons.h"

#include "CryAction.h"
#include "GameObjects/GameObject.h"
#include "IGameObject.h"
#include "IVehicleSystem.h"
#include "Vehicle.h"
#include "VehiclePartBase.h"
#include "VehicleUtils.h"

#include "IActorSystem.h"
#include "IItemSystem.h"
#include "IItem.h"
#include "IWeapon.h"

//#pragma optimize("", off)
//#pragma inline_depth(0)

//------------------------------------------------------------------------
CVehicleSeatActionWeapons::CVehicleSeatActionWeapons()
	: m_pVehicle(0),
	m_passengerId(0),
	m_fireTarget(ZERO),
	m_lastActionActivationMode(0),
	m_weaponIndex(0),
	m_shotDelay(-1.0f),
	m_nextShotTimer(0.0f),
	m_respawnTimer(0.f),
	m_isUsingShootingByUpdate(false),
	m_isSecondary(false),
	m_attackInput(eAI_Primary),
	m_isShootingToCrosshair(true),
	m_isShooting(false),

	m_Forced(false),
	m_isMounted(true),
	m_pSeat(NULL)
{
}

//------------------------------------------------------------------------
CVehicleSeatActionWeapons::~CVehicleSeatActionWeapons()
{
	IEntitySystem* pEntitySystem = gEnv->pEntitySystem;

	for (TVehicleWeaponVector::iterator ite = m_weapons.begin(); ite != m_weapons.end(); ++ite)
	{
		if (IWeapon* pWeapon = GetIWeapon(*ite))
		{
			pWeapon->SetFiringLocator(NULL);
			pWeapon->RemoveEventListener(this);
		}

		if (ite->weaponEntityId != INVALID_ENTITYID)
		{
			if (static_cast<CVehicle*>(m_pVehicle)->SpawnAndDeleteEntities())
				pEntitySystem->RemoveEntity(ite->weaponEntityId, true);

			(*ite).weaponEntityId = INVALID_ENTITYID;
		}
	}

	m_pVehicle->SetObjectUpdate(this, IVehicle::eVOU_NoUpdate);

	m_pVehicle->UnregisterVehicleEventListener(this);
}

//------------------------------------------------------------------------
void CVehicleSeatActionWeapons::BindWeaponToNetwork(EntityId weaponId)
{
	if (IGameObject* pGameObject = CCryAction::GetCryAction()->GetGameObject(weaponId))
	{
		pGameObject->BindToNetworkWithParent(eBTNM_Normal, m_pVehicle->GetEntityId());
	}
}

//------------------------------------------------------------------------
void CVehicleSeatActionWeapons::OnSpawnComplete()
{
	for (TVehicleWeaponVector::iterator ite = m_weapons.begin(); ite != m_weapons.end(); ++ite)
	{
		BindWeaponToNetwork(ite->weaponEntityId);
	}
}

//------------------------------------------------------------------------
bool CVehicleSeatActionWeapons::Init(IVehicle* pVehicle, IVehicleSeat* pSeat, const CVehicleParams& table)
{
	m_pVehicle = pVehicle;
	m_pVehicle->RegisterVehicleEventListener(this, "SeatActionWeapons");
	m_pSeat = pSeat;

	IEntity* pVehicleEntity = pVehicle->GetEntity();

	CVehicleParams weaponsParamsTable = table.findChild("Weapons");
	if (!weaponsParamsTable)
		return false;

	CVehicleParams weaponsArrayTable = weaponsParamsTable.findChild("Weapons");
	if (!weaponsArrayTable)
		return false;

	int i = 0;
	int c = weaponsArrayTable.getChildCount();
	m_weapons.reserve(c);

	for (; i < c; i++)
	{
		if (CVehicleParams weaponTable = weaponsArrayTable.getChild(i))
		{
			string className = weaponTable.getAttr("class");
			if (!className.empty())
			{
				IVehiclePart* pVehiclePart = NULL;
				string partName = weaponTable.getAttr("part");

				if (!partName.empty())
					pVehiclePart = m_pVehicle->GetPart(partName);

				SVehicleWeapon vehicleWeapon;
				IEntity* pWeaponEntity = 0;

				vehicleWeapon.pEntityClass = gEnv->pEntitySystem->GetClassRegistry()->FindClass(className);
				if (!vehicleWeapon.pEntityClass)
				{
					CryLog("[CVehicleSeatActionWeapons]: EntityClass %s not found!", className.c_str());
					continue;
				}

				bool isServer = gEnv->bServer;
				bool bClientOnly = (pVehicleEntity->GetFlags() & ENTITY_FLAG_CLIENT_ONLY) != 0;
				if ((isServer || bClientOnly) && static_cast<CVehicle*>(m_pVehicle)->SpawnAndDeleteEntities())
				{
					pWeaponEntity = SpawnWeapon(vehicleWeapon, pVehicleEntity, partName, i);
				}

				vehicleWeapon.weaponEntityId = pWeaponEntity ? pWeaponEntity->GetId() : 0;
				vehicleWeapon.pPart = pVehiclePart;
				weaponTable.getAttr("inheritVelocity", vehicleWeapon.inheritVelocity);

				string contextName = weaponTable.getAttr("animSubContext");
				IActionController* pAC = m_pVehicle->GetAnimationComponent().GetActionController();
				if (pAC && !contextName.empty())
				{
					vehicleWeapon.contextID = pAC->GetContext().controllerDef.m_subContextIDs.Find(contextName.c_str());
				}

				if (pWeaponEntity)
					SetupWeapon(vehicleWeapon);

				m_weapons.push_back(vehicleWeapon);
				TVehicleHelperVector& helpers = m_weapons.back().helpers;

				if (CVehicleParams helpersTable = weaponTable.findChild("Helpers"))
				{
					int k = 0;
					int cHelpers = helpersTable.getChildCount();
					helpers.reserve(c);

					for (; k < cHelpers; k++)
					{
						if (CVehicleParams helperRef = helpersTable.getChild(k))
						{
							if (IVehicleHelper* pHelper = m_pVehicle->GetHelper(helperRef.getAttr("value")))
								helpers.push_back(pHelper);
						}
					}
				}

				if (CVehicleParams actionsTable = weaponTable.findChild("Actions"))
				{
					TWeaponActionMap& actions = m_weapons.back().actions;

					int k = 0;
					int numActionsTables = actionsTable.getChildCount();

					for (; k < numActionsTables; k++)
					{
						if (CVehicleParams actionTable = actionsTable.getChild(k))
						{
							string name = actionTable.getAttr("name");
							SWeaponAction action;

							if (CVehicleParams animsTable = actionTable.findChild("Animations"))
							{
								int n = 0;
								int numAnimations = animsTable.getChildCount();

								for (; n < numAnimations; n++)
								{
									if (CVehicleParams animTable = animsTable.getChild(n))
									{
										string animName = animTable.getAttr("name");
										if (!animName.empty())
											action.animations.push_back(animName);
									}
								}
							}

							actions.insert(TWeaponActionMap::value_type(name, action));
						}
					}
				}

				if (pWeaponEntity)
					UpdateWeaponTM(vehicleWeapon);
			}
		}
	}

	const size_t weaponCount = m_weapons.size();
	if (weaponCount == 0)
		return false;
	else if (weaponCount > 1)
		m_isUsingShootingByUpdate = true;

	// save a backup of the weapon list, so we can restore them on Reset()
	m_weaponsCopy = m_weapons;

	m_weaponIndex = 0;
	m_isShooting = false;
	m_lastActionActivationMode = 0;

	weaponsParamsTable.getAttr("respawnTime", m_respawnTimer);

	if (!weaponsParamsTable.getAttr("isSecondary", m_isSecondary))
		m_isSecondary = false;

	if (!weaponsParamsTable.getAttr("mounted", m_isMounted))
		m_isMounted = true;

	weaponsParamsTable.getAttr("shotDelay", m_shotDelay);

	if (weaponsParamsTable.getAttr("disablesShootToCrosshair", m_isShootingToCrosshair))
		m_isShootingToCrosshair = !m_isShootingToCrosshair;
	else
		m_isShootingToCrosshair = true;

	m_attackInput = m_isSecondary ? eAI_Secondary : eAI_Primary;

	if (const char* attackInput = weaponsParamsTable.getAttr("attackInput"))
	{
		if (!strcmpi("primary", attackInput))
		{
			m_attackInput = eAI_Primary;
		}
		else if (!strcmpi("secondary", attackInput))
		{
			m_attackInput = eAI_Secondary;
		}
	}

	m_pVehicle->SetObjectUpdate(this, IVehicle::eVOU_PassengerUpdate);

	return true;
}

IEntity* CVehicleSeatActionWeapons::SpawnWeapon(SVehicleWeapon& weapon, IEntity* pVehicleEntity, const string& partName, int weaponIndex)
{
	SEntitySpawnParams spawnParams;

	spawnParams.pClass = weapon.pEntityClass;
	if (!spawnParams.pClass)
	{
		return NULL;
	}

	bool isServer = gEnv->bServer;
	bool bClientOnly = (pVehicleEntity->GetFlags() & ENTITY_FLAG_CLIENT_ONLY) != 0;
	if ((isServer || bClientOnly) && static_cast<CVehicle*>(m_pVehicle)->SpawnAndDeleteEntities())
	{
		char pWeaponName[512];
		cry_sprintf(pWeaponName, "%s_Weapon_%s_%s_%d",
		            pVehicleEntity->GetName(), spawnParams.pClass->GetName(), partName.c_str(), weaponIndex + 1);

		spawnParams.sName = pWeaponName;
		spawnParams.vPosition.zero();
		spawnParams.nFlags = ENTITY_FLAG_NEVER_NETWORK_STATIC;

		if (bClientOnly)
		{
			spawnParams.nFlags |= ENTITY_FLAG_CLIENT_ONLY;
		}

		IEntity* pWeaponEntity = gEnv->pEntitySystem->SpawnEntity(spawnParams);

		if (pWeaponEntity)
		{
			return pWeaponEntity;
		}
	}

	return NULL;
}

//------------------------------------------------------------------------
void CVehicleSeatActionWeapons::SetupWeapon(SVehicleWeapon& weapon)
{
	CRY_ASSERT(weapon.weaponEntityId);

	if (weapon.weaponEntityId == 0)
		return;

	IItem* pItem = GetIItem(weapon);
	IVehiclePart* pVehiclePart = weapon.pPart;

	if (pItem)
	{
		EntityId mountId = pVehiclePart ? pVehiclePart->GetEntity()->GetId() : m_pVehicle->GetEntityId();

		static Vec3 offset(ZERO);
		static Ang3 angles(ZERO);
		pItem->MountAtEntity(mountId, offset, angles);

		pItem->SetSubContextID(weapon.contextID);

		if (IWeapon* pWeapon = pItem->GetIWeapon())
			pWeapon->SetHostId(m_pVehicle->GetEntityId());
	}
}

//------------------------------------------------------------------------
void CVehicleSeatActionWeapons::Reset()
{
	m_weaponIndex = 0;
	m_isShooting = false;
	m_lastActionActivationMode = 0;
	m_fireTarget.Set(0, 0, 0);
	m_nextShotTimer = 0.0f;

	if (m_weaponsCopy.size() != m_weapons.size())
	{
		m_weapons = m_weaponsCopy;
	}

	for (TVehicleWeaponVector::iterator ite = m_weapons.begin(); ite != m_weapons.end(); ++ite)
	{
		SetupWeapon(*ite);

		UpdateWeaponTM(*ite);

		IEntity* pEntity = GetEntity(*ite);

		if (pEntity && pEntity->IsHidden() && !m_pVehicle->GetEntity()->IsHidden())
			pEntity->Hide(false);
	}
}

void CVehicleSeatActionWeapons::StartUsing()
{
	if (m_passengerId || m_Forced)
	{
		if (!gEnv->pSystem->IsSerializingFile())
		{
			m_fireTarget.zero();
		}

		for (TVehicleWeaponVector::iterator ite = m_weapons.begin(); ite != m_weapons.end(); ++ite)
		{
			if (IItem* pItem = GetIItem(*ite))
			{
				pItem->StartUse(m_passengerId);

				if (IWeapon* pWeapon = pItem->GetIWeapon())
				{
					pWeapon->SetFiringLocator(this);
					pWeapon->AddEventListener(this, __FUNCTION__);

					if (pWeapon->OutOfAmmo(false))
					{
						pWeapon->Reload(false);
					}
				}
			}
		}
	}
}

//------------------------------------------------------------------------
void CVehicleSeatActionWeapons::StartUsing(EntityId passengerId)
{

	IEntity* ent = gEnv->pEntitySystem->GetEntity(passengerId);
	m_Forced = m_Forced && (!ent || !ent->GetAI());

	if (!passengerId && !m_Forced)
		return;

	if (m_passengerId && m_passengerId != passengerId)
	{
		CRY_ASSERT(!"error: another passenger is already linked to this weapon seat action");
	}

	m_passengerId = passengerId;

	StartUsing();
}

void CVehicleSeatActionWeapons::ForceUsage()
{
	m_Forced = true;
	m_passengerId = 0;
}

//------------------------------------------------------------------------
void CVehicleSeatActionWeapons::StopUsing()
{
	if (m_passengerId || m_Forced)
	{
		for (TVehicleWeaponVector::iterator ite = m_weapons.begin(); ite != m_weapons.end(); ++ite)
		{
			SVehicleWeapon& vehicleWeapon = *ite;

			if (IItem* pItem = GetIItem(vehicleWeapon))
			{
				if (IWeapon* pWeapon = pItem->GetIWeapon())
				{
					pWeapon->StopFire();
					pWeapon->RemoveEventListener(this);
				}

				pItem->StopUse(m_passengerId);
			}
		}
	}

	m_passengerId = 0;
	m_fireTarget.zero();
}

//------------------------------------------------------------------------
void CVehicleSeatActionWeapons::OnAction(const TVehicleActionId actionId, int activationMode, float value)
{
	if (CanTriggerActionFire(actionId))
	{
		if (activationMode == eAAM_OnPress)
			StartFire();
		else if (activationMode == eAAM_OnRelease)
			StopFire();
	}
	else if (CanTriggerActionFiremode(actionId) && activationMode == eAAM_OnPress)
	{
		if (IWeapon* pWeapon = GetIWeapon(GetCurrentWeapon()))
			pWeapon->ChangeFireMode();
	}
	else if (CanTriggerActionZoom(actionId))
	{
		if (IWeapon* pWeapon = GetIWeapon(GetCurrentWeapon()))
		{
			if (IFireMode* pFireMode = pWeapon->GetFireMode(pWeapon->GetCurrentFireMode()))
			{
				if (activationMode == eAAM_OnPress && !pFireMode->IsReloading())
				{
					if (pFireMode->AllowZoom() && pWeapon->CanZoom())
						pWeapon->StartZoom(m_passengerId, 1);
					else
						pFireMode->Cancel();
				}
				else if (activationMode == eAAM_OnRelease && (pWeapon->IsZoomingInOrOut() || pWeapon->IsZoomed()))
				{
					pWeapon->StopZoom(m_passengerId);
				}
			}
		}
	}
}

bool CVehicleSeatActionWeapons::CanTriggerActionFire(const TVehicleActionId actionId) const
{
	if (actionId == eVAI_Attack1)
	{
		return (m_attackInput == eAI_Primary);
	}
	else if (actionId == eVAI_Attack2)
	{
		return (m_attackInput == eAI_Secondary);
	}

	return false;
}

bool CVehicleSeatActionWeapons::CanTriggerActionFiremode(const TVehicleActionId actionId) const
{
	return (actionId == eVAI_FireMode && !m_isSecondary);
}

bool CVehicleSeatActionWeapons::CanTriggerActionZoom(const TVehicleActionId actionId) const
{
	return (actionId == eVAI_Attack2 && !m_isSecondary);
}

void CVehicleSeatActionWeapons::Update(const float frameTime)
{
	if (m_passengerId == 0)
	{
		DoUpdate(frameTime);
	}
}

void CVehicleSeatActionWeapons::UpdateFromPassenger(const float frameTime, EntityId playerId)
{
	if (m_passengerId == playerId)
	{
		DoUpdate(frameTime);
	}
}

//------------------------------------------------------------------------
void CVehicleSeatActionWeapons::DoUpdate(float frameTime)
{
	CRY_PROFILE_FUNCTION(PROFILE_ACTION);

	bool distant = m_pVehicle->IsProbablyDistant();
	bool visible = m_pVehicle->GetGameObject()->IsProbablyVisible();
	bool updateTM = !gEnv->IsClient() || visible || !distant;

	if (m_Forced)
	{
		SVehicleWeapon& vehicleWeapon = GetVehicleWeapon();
		if (IWeapon* pWeapon = GetIWeapon(vehicleWeapon))
		{
			pWeapon->SetTargetLocation(m_fireTarget);
			pWeapon->SetDestination(m_fireTarget);
		}
	}

	for (int i = 0; i < m_weapons.size(); ++i)
	{
		SVehicleWeapon& weapon = m_weapons[i];
		if (weapon.weaponEntityId)
		{
			if (updateTM && (m_passengerId || weapon.helpers.empty()))
			{
				// update weapon positions
				UpdateWeaponTM(weapon);
			}

			if (m_passengerId && gEnv->IsClient())
			{
				if (IItem* pItem = GetIItem(weapon))
					pItem->UpdateFPView(frameTime);
			}
		}
		else if (gEnv->bServer && weapon.m_respawnTime > 0.f) //Check for respawning weapons
		{
			weapon.m_respawnTime -= frameTime;
			if (weapon.m_respawnTime < 0.f)
			{
				IEntity* pRespawnedItem = SpawnWeapon(weapon, m_pVehicle->GetEntity(), weapon.pPart->GetName(), i);
				if (pRespawnedItem)
				{
					weapon.weaponEntityId = pRespawnedItem->GetId();
					SetupWeapon(weapon);
					BindWeaponToNetwork(weapon.weaponEntityId);
					m_pSeat->SetLocked(eVSLS_Unlocked);

					m_pVehicle->GetGameObject()->InvokeRMIWithDependentObject(CVehicle::ClRespawnWeapon(), CVehicle::RespawnWeaponParams(weapon.weaponEntityId, m_pSeat->GetSeatId(), GetId(), i), eRMI_ToRemoteClients, weapon.weaponEntityId);
				}
			}
		}
	}

	if (!m_passengerId || m_weapons.empty())
		return;

	if (m_isUsingShootingByUpdate)
	{
		if (m_lastActionActivationMode == eAAM_OnPress || m_lastActionActivationMode == eAAM_OnRelease)
		{
			if (m_isShooting == false && m_lastActionActivationMode == eAAM_OnPress)
			{
				m_isShooting = true;
				m_lastActionActivationMode = 0;
			}
			else if (m_isShooting == true && m_lastActionActivationMode == eAAM_OnRelease)
			{
				m_isShooting = false;
				m_lastActionActivationMode = 0;
				m_nextShotTimer = 0.0f;
			}
		}

		if (m_shotDelay > 0.0f)
		{
			m_nextShotTimer -= frameTime;
		}

		if (m_isShooting && CanFireWeapon())
		{
			SVehicleWeapon& vehicleWeapon = GetVehicleWeapon();

			if (IWeapon* pWeapon = GetIWeapon(vehicleWeapon))
			{
				pWeapon->StartFire();
				pWeapon->StopFire();
			}
		}
	}
}

//------------------------------------------------------------------------
SVehicleWeapon* CVehicleSeatActionWeapons::GetVehicleWeapon(EntityId weaponId)
{
	// todo: all these iterations should be removed
	for (TVehicleWeaponVector::iterator ite = m_weapons.begin(); ite != m_weapons.end(); ++ite)
	{
		if (ite->weaponEntityId == weaponId)
		{
			return &(*ite);
		}
	}

	return NULL;
}

//------------------------------------------------------------------------
SVehicleWeapon& CVehicleSeatActionWeapons::GetVehicleWeapon()
{
	if (m_weapons.size() > 1)
		return m_weapons[m_weaponIndex];
	else
		return m_weapons[0];
}

//------------------------------------------------------------------------
SVehicleWeapon& CVehicleSeatActionWeapons::GetWeaponInfo(int weaponIndex)
{
	if (weaponIndex < m_weapons.size())
		return m_weapons[weaponIndex];
	else
		return m_weapons[0];
}

//------------------------------------------------------------------------
IItem* CVehicleSeatActionWeapons::GetIItem(const SVehicleWeapon& vehicleWeapon)
{
	return CCryAction::GetCryAction()->GetIItemSystem()->GetItem(vehicleWeapon.weaponEntityId);
}

//------------------------------------------------------------------------
IWeapon* CVehicleSeatActionWeapons::GetIWeapon(const SVehicleWeapon& vehicleWeapon)
{
	IItem* pItem = GetIItem(vehicleWeapon);

	if (pItem)
		return pItem->GetIWeapon();

	return NULL;
}

//------------------------------------------------------------------------
IEntity* CVehicleSeatActionWeapons::GetEntity(const SVehicleWeapon& weapon)
{
	return gEnv->pEntitySystem->GetEntity(weapon.weaponEntityId);
}

//------------------------------------------------------------------------
IVehicleHelper* CVehicleSeatActionWeapons::GetHelper(SVehicleWeapon& vehicleWeapon)
{
	TVehicleHelperVector& helpers = vehicleWeapon.helpers;

	if (!helpers.empty())
	{
		IWeapon* pWeapon = GetIWeapon(vehicleWeapon);
		if (pWeapon)
		{
			IFireMode* pFireMode = pWeapon->GetFireMode(pWeapon->GetCurrentFireMode());
			if (pFireMode)
			{
				int index = pFireMode->GetCurrentBarrel();
				if (index < helpers.size())
					return helpers[index];
			}
		}

		return helpers[0];
	}

	return NULL;
}

//------------------------------------------------------------------------
void CVehicleSeatActionWeapons::Serialize(TSerialize ser, EEntityAspects aspects)
{
	if (ser.GetSerializationTarget() != eST_Network)
	{
		char buffer[16];
		int n = m_weapons.size();

		for (int i = 0; i < n; ++i)
		{
			cry_sprintf(buffer, "weapon_%d", i);
			ser.Value(buffer, m_weapons[i].weaponEntityId, 'eid');
		}
		ser.Value("m_fireTarget", m_fireTarget);
	}
}

//------------------------------------------------------------------------
void CVehicleSeatActionWeapons::PostSerialize()
{
	for (TVehicleWeaponVector::iterator ite = m_weapons.begin(); ite != m_weapons.end(); ++ite)
	{
		if (IItem* pItem = GetIItem(*ite))
		{
			if (IWeapon* pWeapon = pItem->GetIWeapon())
			{
				pWeapon->SetFiringLocator(this);
				pWeapon->AddEventListener(this, __FUNCTION__);

				if (m_passengerId)
					pItem->StartUse(m_passengerId);
			}
		}
	}
}

//------------------------------------------------------------------------
void CVehicleSeatActionWeapons::StartFire()
{
	if (m_weapons.empty())
		return;

	if (!m_isUsingShootingByUpdate)
	{
		SVehicleWeapon& vehicleWeapon = GetWeaponInfo(0);
		IWeapon* const pWeapon = GetIWeapon(vehicleWeapon);

		if (pWeapon)
		{
			UpdateWeaponTM(vehicleWeapon);

			CryAudio::IAudioSystem const* const pIAudioSystem = gEnv->pAudioSystem;
			IVehicleMovement const* const pMovement = m_pVehicle->GetMovement();
			if (pMovement != nullptr && pWeapon->CanFire())
			{
				IEntityAudioComponent* const pAudioProxy = pMovement->GetAudioProxy();
				if (pIAudioSystem != nullptr && pAudioProxy != nullptr)
				{
					CryAudio::ControlId audioControlID = CryAudio::InvalidControlId;
					if (m_attackInput == eAI_Primary)
					{
						audioControlID = pMovement->GetPrimaryWeaponAudioTrigger();
					}
					else if (m_attackInput == eAI_Secondary)
					{
						audioControlID = pMovement->GetSecondaryWeaponAudioTrigger();
					}

					pAudioProxy->ExecuteTrigger(audioControlID);
				}
			}
			pWeapon->StartFire();
			m_isShooting = true;
		}
		else
		{
			GameWarning("%s: Weapon not available (EntId %i)", m_pVehicle->GetEntity()->GetName(), vehicleWeapon.weaponEntityId);
		}
	}
	else
	{
		// the update function will do the remaining of the job
		m_lastActionActivationMode = eAAM_OnPress;
	}
}

//------------------------------------------------------------------------
void CVehicleSeatActionWeapons::StopFire()
{
	if (m_weapons.empty())
		return;

	if (!m_isUsingShootingByUpdate)
	{
		SVehicleWeapon& vehicleWeapon = GetWeaponInfo(0);
		IWeapon* const pWeapon = GetIWeapon(vehicleWeapon);
		if (pWeapon)
		{
			pWeapon->StopFire();
		}

		const IVehicleMovement* const pMovement = m_pVehicle->GetMovement();
		if (pMovement)
		{
			IEntityAudioComponent* const pAudioProxy = pMovement->GetAudioProxy();
			if (pAudioProxy)
			{
				CryAudio::ControlId audioControlID = CryAudio::InvalidControlId;
				if (m_attackInput == eAI_Primary)
				{
					audioControlID = pMovement->GetPrimaryWeaponAudioStopTrigger();
				}
				else if (m_attackInput == eAI_Secondary)
				{
					IFireMode* const pFireMode = pWeapon->GetFireMode(pWeapon->GetCurrentFireMode());
					audioControlID = pMovement->GetSecondaryWeaponAudioStopTrigger();
				}

				pAudioProxy->ExecuteTrigger(audioControlID);
			}
		}

		m_isShooting = false;
	}
	else
	{
		// the update function will do the remaining of the job
		m_lastActionActivationMode = eAAM_OnRelease;
	}
}

//------------------------------------------------------------------------
void CVehicleSeatActionWeapons::OnOutOfAmmo(IWeapon* pWeapon, IEntityClass* pAmmoType)
{
	pWeapon->Reload(false);
}

//------------------------------------------------------------------------
bool CVehicleSeatActionWeapons::CanFireWeapon() const
{
	return (m_shotDelay <= 0.0f || m_nextShotTimer <= 0.0f);
}

//------------------------------------------------------------------------
unsigned int CVehicleSeatActionWeapons::GetWeaponCount() const
{
	return m_weapons.size();
}

//------------------------------------------------------------------------
EntityId CVehicleSeatActionWeapons::GetCurrentWeaponEntityId() const
{
	return GetWeaponEntityId(GetCurrentWeapon());
}

//------------------------------------------------------------------------
EntityId CVehicleSeatActionWeapons::GetWeaponEntityId(unsigned int index) const
{
	if (index < m_weapons.size())
	{
		const SVehicleWeapon& vehicleWeapon = m_weapons[index];
		return vehicleWeapon.weaponEntityId;
	}

	return 0;
}

//------------------------------------------------------------------------
IItem* CVehicleSeatActionWeapons::GetIItem(unsigned int index)
{
	if (index < m_weapons.size())
	{
		return GetIItem(m_weapons[index]);
	}

	return 0;
}

//------------------------------------------------------------------------
IWeapon* CVehicleSeatActionWeapons::GetIWeapon(unsigned int index)
{
	if (index < m_weapons.size())
	{
		return GetIWeapon(m_weapons[index]);
	}

	return 0;
}

//------------------------------------------------------------------------
void CVehicleSeatActionWeapons::ClSetupWeapon(unsigned int index, EntityId weaponId)
{
	IItem* pItem = CCryAction::GetCryAction()->GetIItemSystem()->GetItem(weaponId);
	if (!pItem)
		return;

	SVehicleWeapon& weaponInfo = GetWeaponInfo(index);
	weaponInfo.weaponEntityId = weaponId;

	IVehiclePart* pVehiclePart = weaponInfo.pPart;

	EntityId mountId = pVehiclePart ? pVehiclePart->GetEntity()->GetId() : m_pVehicle->GetEntityId();

	static Vec3 offset(ZERO);
	static Ang3 angles(ZERO);
	pItem->MountAtEntity(mountId, offset, angles);
	pItem->SetSubContextID(weaponInfo.contextID);

	if (IWeapon* pWeapon = pItem->GetIWeapon())
	{
		pWeapon->SetHostId(m_pVehicle->GetEntityId());

		if (pWeapon->GetFiringLocator() != this)
		{
			pWeapon->SetFiringLocator(this);
		}
		pWeapon->AddEventListener(this, __FUNCTION__);
	}

	UpdateWeaponTM(weaponInfo);
}

//------------------------------------------------------------------------
void CVehicleSeatActionWeapons::UpdateWeaponTM(SVehicleWeapon& weapon)
{
	CRY_PROFILE_FUNCTION(PROFILE_ACTION);

	if (IEntity* pEntityWeapon = GetEntity(weapon))
	{
		if (!weapon.helpers.empty())
		{
			// weapon is always kept at first helper
			Matrix34 worldTM;
			weapon.helpers[0]->GetWorldTM(worldTM);
			pEntityWeapon->SetWorldTM(worldTM);
		}
		else if (weapon.pPart)
		{
			const Matrix34& worldTM = weapon.pPart->GetWorldTM();
			pEntityWeapon->SetWorldTM(worldTM);
		}

#if ENABLE_VEHICLE_DEBUG
		if (VehicleCVars().v_draw_tm)
			VehicleUtils::DrawTM(pEntityWeapon->GetWorldTM());
#endif
	}
}

//------------------------------------------------------------------------
int CVehicleSeatActionWeapons::GetSkipEntities(IPhysicalEntity** pSkipEnts, int nMaxSkip)
{
	int nSkip = 0;

	// skip vehicle and all child entities
	IEntity* pVehicleEntity = m_pVehicle->GetEntity();

	if (nSkip < nMaxSkip)
		pSkipEnts[nSkip++] = pVehicleEntity->GetPhysics();

	int count = pVehicleEntity->GetChildCount();

	for (int c = 0; c < count && nSkip < nMaxSkip; ++c)
		if (IPhysicalEntity* pPhysics = pVehicleEntity->GetChild(c)->GetPhysics())
			pSkipEnts[nSkip++] = pPhysics;

	return nSkip;
}

//------------------------------------------------------------------------
bool CVehicleSeatActionWeapons::GetProbableHit(EntityId weaponId, const IFireMode* pFireMode, Vec3& hitPos)
{
	if (m_fireTarget.IsZero())
		return false;

	Vec3 pos;
	if (!GetFiringPos(weaponId, pFireMode, pos))
		return false;

	IPhysicalEntity* pSkipEntities[10];
	int nSkip = GetSkipEntities(pSkipEntities, 10);

	Vec3 dir = m_fireTarget - pos;
	static ray_hit hit;
	if (gEnv->pPhysicalWorld->RayWorldIntersection(pos, dir, ent_all,
	                                               rwi_stop_at_pierceable | rwi_ignore_back_faces, &hit, 1, pSkipEntities, nSkip))
	{
		hitPos = hit.pt;
	}
	else
		hitPos = m_fireTarget;

	return true;
}

//------------------------------------------------------------------------
bool CVehicleSeatActionWeapons::GetFiringPos(EntityId weaponId, const IFireMode* pFireMode, Vec3& pos)
{
	CRY_PROFILE_FUNCTION(PROFILE_ACTION);

	for (TVehicleWeaponVector::iterator ite = m_weapons.begin(); ite != m_weapons.end(); ++ite)
	{
		SVehicleWeapon& weapon = *ite;

		if (weapon.weaponEntityId == weaponId)
		{
			if (IEntity* pEntityWeapon = GetEntity(weapon))
			{
				if (!weapon.helpers.empty())
				{
					if (IVehicleHelper* pHelper = GetHelper(weapon))
						pos = pHelper->GetWorldSpaceTranslation();
					else
						pos = pEntityWeapon->GetWorldTM().GetTranslation();
				}
				else
				{
					IWeapon* pWeapon = GetIWeapon(weapon);
					pFireMode = pWeapon ? pWeapon->GetFireMode(pWeapon->GetCurrentFireMode()) : 0;

					if (pFireMode && pFireMode->HasFireHelper())
					{
						// weapon helper
						pos = pFireMode->GetFireHelperPos();
					}
					else
					{
						// fallback, entity pos
						pos = pEntityWeapon->GetWorldTM().GetTranslation();
					}
				}

				return true;
			}
			break;
		}
	}

	return false;
}

//------------------------------------------------------------------------
bool CVehicleSeatActionWeapons::GetFiringDir(EntityId weaponId, const IFireMode* pFireMode, Vec3& dir, const Vec3& probableHit, const Vec3& firingPos)
{
	CRY_PROFILE_FUNCTION(PROFILE_ACTION);

	IActor* pActor = GetUserActor();

	// for player, use direction to probable hit
	if (pActor && pActor->IsPlayer() && m_isShootingToCrosshair)
	{
		IVehicleView* pView = m_pSeat ? m_pSeat->GetCurrentView() : 0;
		if (pView && pView->ShootToCrosshair())
		{
			if (!probableHit.IsZero())
			{
				dir = (probableHit - firingPos).normalize();
				return true;
			}
		}

		{
			// fallback: fill in view direction, but return false to indicate
			// it's not the precise firing direction
			if (pActor != NULL)
				dir = pActor->GetViewRotation().GetColumn1();
			else
				dir = pActor->GetEntity()->GetWorldRotation().GetColumn1();
			return false;
		}
	}

	// for AI, always return weapon/helper direction
	for (TVehicleWeaponVector::iterator ite = m_weapons.begin(); ite != m_weapons.end(); ++ite)
	{
		SVehicleWeapon& weapon = *ite;

		if (weapon.weaponEntityId == weaponId)
		{
			if (IEntity* pWeaponEntity = GetEntity(weapon))
			{
				dir = probableHit - firingPos;
				dir.NormalizeSafe();
				return true;
			}
			break;
		}
	}

	return false;

}
//------------------------------------------------------------------------
bool CVehicleSeatActionWeapons::GetActualWeaponDir(EntityId weaponId, const IFireMode* pFireMode, Vec3& dir, const Vec3& probableHit, const Vec3& firingPos)
{
	CRY_PROFILE_FUNCTION(PROFILE_ACTION);

	IActor* pActor = GetUserActor();

	// for player, use direction to probable hit
	if (pActor && pActor->IsPlayer() && m_isShootingToCrosshair)
	{
		IVehicleView* pView = m_pSeat ? m_pSeat->GetCurrentView() : 0;
		if (pView && pView->ShootToCrosshair())
		{
			if (!probableHit.IsZero())
			{
				dir = (probableHit - firingPos).normalize();
				return true;
			}
		}

		{
			// fallback: fill in view direction, but return false to indicate
			// it's not the precise firing direction
			if (pActor != NULL)
				dir = pActor->GetViewRotation().GetColumn1();
			else
				dir = pActor->GetEntity()->GetWorldRotation().GetColumn1();
			return false;
		}
	}

	// for AI, always return weapon/helper direction
	// for AI, always return weapon/helper direction
	for (TVehicleWeaponVector::iterator ite = m_weapons.begin(); ite != m_weapons.end(); ++ite)
	{
		SVehicleWeapon& weapon = *ite;

		if (weapon.weaponEntityId == weaponId)
		{
			if (IEntity* pWeaponEntity = GetEntity(weapon))
			{
				if (!weapon.helpers.empty())
				{
					if (IVehicleHelper* pHelper = GetHelper(weapon))
					{
						Matrix34 tm;
						pHelper->GetWorldTM(tm);
						dir = tm.GetColumn1();
					}
					else
					{
						dir = pWeaponEntity->GetWorldTM().GetColumn1();
					}
				}
				else
				{
					IWeapon* pWeapon = GetIWeapon(weapon);
					pFireMode = pWeapon ? pWeapon->GetFireMode(pWeapon->GetCurrentFireMode()) : 0;
					// [MR] dont use helper direction until its fixed
					//if (pFireMode && pFireMode->HasFireHelper())
					if (0)
						dir = pFireMode->GetFireHelperDir();
					else
						dir = pWeaponEntity->GetWorldTM().GetColumn1();
				}
				return true;
			}
			break;
		}
	}

	return false;
}

//------------------------------------------------------------------------
bool CVehicleSeatActionWeapons::GetFiringVelocity(EntityId weaponId, const IFireMode* pFireMode, Vec3& vel, const Vec3& firingDir)
{
	SVehicleWeapon* pVehicleWeapon = GetVehicleWeapon(weaponId);

	if (pVehicleWeapon && !pVehicleWeapon->inheritVelocity)
		return false;

	const SVehicleStatus& status = m_pVehicle->GetStatus();
	vel.zero();

	if (status.speed > 0.01f)
	{
		float dot = status.vel * firingDir;
		if (dot > 0.f)
			vel = status.vel.GetNormalized() * dot;
	}

	return true;
}

//------------------------------------------------------------------------
IActor* CVehicleSeatActionWeapons::GetUserActor()
{
	IActorSystem* pActorSystem = CCryAction::GetCryAction()->GetIActorSystem();

	return pActorSystem->GetActor(m_passengerId);
}

//------------------------------------------------------------------------
void CVehicleSeatActionWeapons::OnVehicleEvent(EVehicleEvent event, const SVehicleEventParams& params)
{
	if (event == eVE_Destroyed)
	{
		if (m_isShooting)
		{
			StopFire();
		}

		// set AI state
		IAISystem* pAISystem = gEnv->pAISystem;

		for (TVehicleWeaponVector::iterator ite = m_weapons.begin(); ite != m_weapons.end(); ++ite)
		{
			if (IEntity* pEntity = GetEntity(*ite))
			{
				if (pAISystem && pAISystem->IsEnabled())
				{
					pAISystem->GetSmartObjectManager()->SetSmartObjectState(pEntity, "Busy");
				}
				pEntity->Hide(true);
			}
		}
	}
	else if (event == eVE_WeaponRemoved)
	{
		TVehicleWeaponVector::iterator ite = m_weapons.begin();
		TVehicleWeaponVector::iterator itEnd = m_weapons.end();
		for (; ite != itEnd; ++ite)
		{
			if (ite->weaponEntityId == params.entityId)
			{
				// one of our weapons is being removed from the vehicle
				SVehicleWeapon& vehicleWeapon = *ite;

				IWeapon* pWeapon = GetIWeapon(vehicleWeapon);
				pWeapon->RemoveEventListener(this);

				pWeapon->SetFiringLocator(NULL);
				pWeapon->SetHostId(0);

				//Network dependencies
				//A removed weapon is invariably going to get deleted sooner or later.
				// The "linked list" dependencies need to be kept up to date or late joiners will have problems.
				CVehicle* pVehicleImpl = static_cast<CVehicle*>(m_pVehicle);
				EntityId childId = vehicleWeapon.networkChildId;

				if (childId)
				{
					if (IGameObject* pGameObject = CCryAction::GetCryAction()->GetGameObject(childId))
					{
						pGameObject->SetNetworkParent(vehicleWeapon.networkParentId);

						if (SVehicleWeapon* pChildVehicleWeapon = pVehicleImpl->GetVehicleWeaponAllSeats(childId))
						{
							pChildVehicleWeapon->networkParentId = vehicleWeapon.networkParentId;
						}
					}
				}

				EntityId lastCreatedWeaponId = pVehicleImpl->GetLastCreatedWeaponId();
				if (lastCreatedWeaponId == vehicleWeapon.weaponEntityId)
				{
					EntityId vehicleId = pVehicleImpl->GetEntityId();
					if (childId && childId != vehicleId)
					{
						pVehicleImpl->SetLastCreatedWeaponId(childId);
					}
					else if (vehicleWeapon.networkParentId)
					{
						pVehicleImpl->SetLastCreatedWeaponId(vehicleWeapon.networkParentId);
					}
					else
					{
						pVehicleImpl->SetLastCreatedWeaponId(0); //No weapons left
					}
				}
				//~Network dependencies

				vehicleWeapon.weaponEntityId = 0;

				if (m_respawnTimer)
				{
					vehicleWeapon.m_respawnTime = m_respawnTimer;
				}

				break;
			}
		}
	}
}

//------------------------------------------------------------------------
void CVehicleSeatActionWeapons::OnShoot(IWeapon* pWeapon, EntityId shooterId, EntityId ammoId, IEntityClass* pAmmoType, const Vec3& pos, const Vec3& dir, const Vec3& vel)
{
	// Update the next shot timer
	if (m_shotDelay > 0.0f)
		m_nextShotTimer = m_shotDelay;

	for (TVehicleWeaponVector::const_iterator it = m_weapons.begin(); it != m_weapons.end(); ++it)
	{
		const SVehicleWeapon& vehicleWeapon = *it;
		if (GetIWeapon(vehicleWeapon) == pWeapon)
		{
			TWeaponActionMap::const_iterator itAction = vehicleWeapon.actions.find(CONST_TEMP_STRING("fire"));
			if (itAction != vehicleWeapon.actions.end())
			{
				const SWeaponAction& action = itAction->second;
				IFireMode* pFireMode = pWeapon->GetFireMode(pWeapon->GetCurrentFireMode());

				if (pFireMode && pFireMode->GetCurrentBarrel() < action.animations.size())
				{
					IVehicleAnimation* pAnim = m_pVehicle->GetAnimation(action.animations[pFireMode->GetCurrentBarrel()].c_str());
					if (pAnim)
					{
						pAnim->StartAnimation();
					}
				}
			}
			break;
		}
	}

	if (m_weapons.size() > 1)
	{
		m_weaponIndex++;
		if (m_weaponIndex >= m_weapons.size())
			m_weaponIndex = 0;
	}

	m_pVehicle->GetGameObject()->Pulse('bang');
}

//------------------------------------------------------------------------
Vec3 CVehicleSeatActionWeapons::GetAverageFiringPos()
{
	CRY_PROFILE_FUNCTION(PROFILE_ACTION);

	AABB bounds;
	bounds.Reset();

	TVehicleWeaponVector::iterator weaponIte = m_weapons.begin();
	TVehicleWeaponVector::iterator weaponIteEnd = m_weapons.end();

	for (; weaponIte != weaponIteEnd; ++weaponIte)
	{
		SVehicleWeapon& weapon = *weaponIte;
		TVehicleHelperVector::iterator helperIte = weapon.helpers.begin();
		TVehicleHelperVector::iterator helperIteEnd = weapon.helpers.end();
		for (; helperIte != helperIteEnd; ++helperIte)
		{
			IVehicleHelper* pHelper = *helperIte;
			bounds.Add(pHelper->GetVehicleSpaceTranslation());
		}
	}

	return bounds.GetCenter();
}

void CVehicleSeatActionWeapons::OnWeaponRespawned(int weaponIndex, EntityId newWeaponEntityId)
{
	CRY_ASSERT_MESSAGE(weaponIndex < m_weapons.size(), "CVehicleSeatActionWeapons::WeaponRespawned - Invalid weaponIndex!");

	ClSetupWeapon(weaponIndex, newWeaponEntityId);
	m_pSeat->SetLocked(eVSLS_Unlocked);
}

void CVehicleSeatActionWeapons::OnVehicleActionControllerDeleted()
{
	for (TVehicleWeaponVector::iterator ite = m_weapons.begin(); ite != m_weapons.end(); ++ite)
	{
		if (IItem* pItem = GetIItem(*ite))
		{
			pItem->SetCurrentActionController(NULL);
		}
	}
}

void CVehicleSeatActionWeapons::GetMemoryUsage(ICrySizer* s) const
{
	s->Add(*this);
	s->AddObject(m_weapons);
}

DEFINE_VEHICLEOBJECT(CVehicleSeatActionWeapons)
