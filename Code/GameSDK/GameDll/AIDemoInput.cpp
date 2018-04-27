// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"

#if (USE_DEDICATED_INPUT)

#include "AIDemoInput.h"
#include "GameRulesModules/IGameRulesSpawningModule.h"
#include "GameCVars.h"
#include "Player.h"
#include "GameRules.h"
#include "Weapon.h"
#include "Utility/CryWatch.h"

static const float THETA = 3.14159265358979323f / 180.0f * 3;

static const char * s_visionModes[] = {"nanosuit_thermalvision",	"nanosuit_cryvision",	"nanosuit_nightvision"};

CDedicatedInput::CDedicatedInput(CPlayer* pPlayer) :
		m_pPlayer(pPlayer),
		m_rand( gEnv->bNoRandomSeed?0:(uint32)gEnv->pTimer->GetAsyncCurTime() ),
		m_timeBeforeChangeMode(0.0f),
		m_grenadeStateTime(0.0f),
		m_stance(STANCE_STAND),
		m_jump(0),
		m_grenadeState(GRENADE_PREPARE),
		m_timeToReachTarget(0),
		m_fire(eDB_Default),
		m_move(eDB_Default)
{
	IEntity* pEntity = pPlayer->GetEntity();
	Vec3 pos = pEntity->GetPos();

	m_spawnPosition = pos;
	m_targetPosition = pos;
	m_timeUntilDie = cry_random(30.0f, 90.0f);
	m_timeUntilRequestRespawn = cry_random(1.0f, 7.0f);
	m_timeUntilChangeWeapon = cry_random(10.0f, 15.0f);

	m_explosiveItemsIdx = 0;
	m_itemIdx = 0;

	Reset();
}

void CDedicatedInput::PreUpdate()	
{
	//Only do dedicated input updates whilst the game is in progress
	bool doBehaviour = true;
	CGameRules* const pGameRules = g_pGame->GetGameRules();
	if (pGameRules->IsGameInProgress() == false)
	{
		doBehaviour = false;
	}

	bool shouldFire = (g_pGameCVars->g_dummyPlayersFire != 0);
	bool shouldFireNoExplosives = (g_pGameCVars->g_dummyPlayersFire == 2);
	bool shouldMove = (g_pGameCVars->g_dummyPlayersMove != 0);
	bool shouldToggleSuitMode = (g_pGameCVars->g_dummyPlayersMove == 1);
	bool shouldChangeWeapon = (g_pGameCVars->g_dummyPlayersChangeWeapon != 0);

	if (m_fire != eDB_Default)
		shouldFire = (m_fire == eDB_True);

	if (m_move != eDB_Default)
		shouldMove = (m_move == eDB_True);

	shouldFire &= doBehaviour;
	shouldMove &= doBehaviour;
	shouldChangeWeapon &= doBehaviour;
		
	if (shouldChangeWeapon)
	{
		m_timeUntilChangeWeapon -= gEnv->pTimer->GetFrameTime();
		if (m_timeUntilChangeWeapon > 0.0f)
		{
			shouldChangeWeapon = false;
		}
	}
	if (shouldChangeWeapon)
	{
		// Only change weapon between 10->15seconds
		m_timeUntilChangeWeapon = cry_random(10.0f, 15.0f);

		GiveItems();
	}

	if (m_timeBeforeChangeMode <= 0.f)
	{
		m_timeBeforeChangeMode = cry_random(0.5f, 7.5f);

		m_mode = cry_random(0, NUM_MODES-(shouldFireNoExplosives?2:1));
		if(shouldFireNoExplosives && m_mode>=E_EXPLOSIVE)
			m_mode++;
		IItem* pItem = m_pPlayer->GetCurrentItem();
		if (pItem)
		{
			IWeapon* pWeapon = pItem->GetIWeapon();
			if (pWeapon)
			{
				pWeapon->StopFire();
			}
		}

		if (shouldFire == true)
		{
			switch (m_mode)
			{
			case E_EXPLOSIVE:
				m_grenadeState = GRENADE_PREPARE;
				m_grenadeStateTime = 0.f;
				m_pPlayer->SelectNextItem(1, true, eICT_Explosive|eICT_Grenade);
				break;
			case E_SMALLMEDIUM:
				{
					int looptimes = cry_random(1, 2);
					while(looptimes--)
						m_pPlayer->SelectNextItem(1, true, eICT_Primary | eICT_Secondary);
					break;
				}
			case E_MELEE:
				m_pPlayer->SelectNextItem(1, true, eICT_Primary | eICT_Secondary);
				break;
			}
		}

		if (shouldMove == true)
		{
			if (m_pPlayer->IsClient() && (cry_random(0, 3) != 0))
			{
				IEntityClass* pClass = gEnv->pEntitySystem->GetClassRegistry()->FindClass("Binoculars");
				CItem * binItem = pClass ? m_pPlayer->GetItemByClass(pClass) : NULL;
				IWeapon * binWeapon = binItem ? binItem->GetIWeapon() : NULL;
				if (binWeapon)
				{
					int whichMode = cry_random(0, (int)(CRY_ARRAY_COUNT(s_visionModes)) - 1);
					binWeapon->OnAction(m_pPlayer->GetEntityId(), ActionId(s_visionModes[whichMode]), eAAM_OnPress, 1.f);
				}
			}
		}
	}

	float frametime = gEnv->pTimer->GetFrameTime();
	m_timeBeforeChangeMode -= frametime;
	m_grenadeStateTime += frametime;

	float changeAmount = frametime * m_interpolationSpeed;
	m_interpolateToNewMoveLookInputs -= changeAmount;

	if (m_interpolateToNewMoveLookInputs <= 0.f)
	{
		m_interpolateToNewMoveLookInputs = 1.f;
		m_deltaMovement = m_targetDeltaMovement;
		m_deltaRotation = m_targetDeltaRotation;
		m_interpolationSpeed = cry_random(0.2f, 1.2f);
		m_targetDeltaMovement.Set(cry_random(-5.0f, 5.0f), cry_random(-2.0f, 8.0f), cry_random(-5.0f, 5.0f));
		m_targetDeltaRotation.Set(THETA * cry_random(-0.5f, 0.5f), 0.f, THETA * cry_random(-2.5f, 2.5f));

		m_targetDeltaRotation *= cry_random(0.0f, 1.0f);
		m_targetDeltaMovement *= cry_random(0.0f, 1.0f);

		if (cry_random(0, 1) == 0)
		{
			if (m_stance == STANCE_STAND)
			{					
				m_stance = STANCE_CROUCH;
			}
			else if (m_stance == STANCE_CROUCH)
			{
				m_stance = STANCE_STAND;
			}
		}
		m_jump = (m_stance == STANCE_STAND) ? (cry_random(0.0f, 1.0f) < g_pGameCVars->g_dummyPlayersJump) : 0;
	}
	else
	{
		float fractionDoneThisFrame = changeAmount / (m_interpolateToNewMoveLookInputs + changeAmount);
		CRY_ASSERT_TRACE (fractionDoneThisFrame >= 0.f && fractionDoneThisFrame <= 1.f, ("fractionDoneThisFrame = %f (changeAmount=%f, interpolate=%f)", fractionDoneThisFrame, changeAmount, m_interpolateToNewMoveLookInputs));
		m_deltaMovement = m_deltaMovement * (1.f - fractionDoneThisFrame) + m_targetDeltaMovement * fractionDoneThisFrame;
		m_deltaRotation = m_deltaRotation * (1.f - fractionDoneThisFrame) + m_targetDeltaRotation * fractionDoneThisFrame;
	}
	if (shouldMove == false)
	{
		m_stance = STANCE_STAND;
		m_jump = 0;
		m_interpolateToNewMoveLookInputs = 0.0f;
		m_deltaMovement.zero();
		m_deltaRotation.Set(0.0f,0.0f,0.0f);
		m_interpolationSpeed = 1.0f;
		m_targetDeltaMovement.zero();
		m_targetDeltaRotation.Set(0.0f,0.0f,0.0f);
	}

	CHANGED_NETWORK_STATE(m_pPlayer,  CPlayer::ASPECT_INPUT_CLIENT );

	HandleDeathAndSuicide(pGameRules);
}

void CDedicatedInput::GiveItems()
{
	const char *itemNames[] = 
	{
		"AY69",
		"CamoSCAR",
		"DesertSCAR",
		"DSG1",
		"Feline",
		"FY71",
		"Gauss",
		"Grendel",
		"Hammer",
		"Jackal",
		"K-Volt",
		"LTag",
		"Marshall",
		"mike",
		"Mk60",
		"Nova",
		"Revolver",
		"SCAR",
		"SCARAB"
	};
	const int numberItems = (CRY_ARRAY_COUNT(itemNames));

	int nameIdx = 0;

	// 1 = sequential
	// 2 = random
	if (g_pGameCVars->g_dummyPlayersChangeWeapon == 1)
	{
		nameIdx = m_itemIdx++;
		if (m_itemIdx >= numberItems)
		{
			m_itemIdx = 0;
		}
	}
	else
	{
		nameIdx = cry_random(0, numberItems - 1);
	}

	IGameFramework *pGameFramework = gEnv->pGameFramework;
	IItemSystem		*pItemSystem = pGameFramework->GetIItemSystem();

	//Check item name before giving (it will resolve case sensitive 'issues')
	const char *itemName = (const char*)(pItemSystem->Query(eISQ_Find_Item_By_Name, itemNames[nameIdx]));
	if(itemName)
	{
		pItemSystem->GiveItem(m_pPlayer, itemName, true, true, true);
		CryLog( "Giving new weapon item %s", itemName);
	}
	else
	{
		CryWarning(VALIDATOR_MODULE_GAME, VALIDATOR_WARNING, "DummyPlayer : Trying to give DummyPlayer unknown/removed item '%s'", itemNames[nameIdx]);
	}

	const char *explosiveItemNames[] =
	{
		"C4",
		"JAW",
		"FlashBangGrenades",
		"FragGrenades",
		"SmokeGrenades"
	};
	const int numberExplosiveItems = (CRY_ARRAY_COUNT(explosiveItemNames));

	// 1 = sequential
	// 2 = random
	if (g_pGameCVars->g_dummyPlayersChangeWeapon == 1)
	{
		nameIdx = m_explosiveItemsIdx++;
		if (m_explosiveItemsIdx >= numberExplosiveItems)
		{
			m_explosiveItemsIdx = 0;
		}
	}
	else
	{
		nameIdx = cry_random(0, numberExplosiveItems - 1);
	}

	itemName = (const char*)(pItemSystem->Query(eISQ_Find_Item_By_Name, explosiveItemNames[nameIdx]));
	if(itemName)
	{
		pItemSystem->GiveItem(m_pPlayer, itemName, true, false, true);
		CryLog( "Giving new explosive item %s", itemName);
	}
	else
	{
		CryWarning(VALIDATOR_MODULE_GAME, VALIDATOR_WARNING, "DummyPlayer : Trying to give DummyPlayer unknown/removed explosive item '%s'", explosiveItemNames[nameIdx]);
	}
}

void CDedicatedInput::Update()
{
	bool shouldFire = (g_pGameCVars->g_dummyPlayersFire != 0);
	bool shouldMove = (g_pGameCVars->g_dummyPlayersMove != 0);

	if (m_fire != eDB_Default)
		shouldFire = (m_fire == eDB_True);

	if (m_move != eDB_Default)
		shouldMove = (m_move == eDB_True);

	bool doBehaviour = true;
	CGameRules* const pGameRules = g_pGame->GetGameRules();
	if (pGameRules->IsGameInProgress() == false)
	{
		doBehaviour = false;
	}

	shouldFire &= doBehaviour;
	shouldMove &= doBehaviour;

	CMovementRequest request;
	request.SetStance((EStance) m_stance);
	if (shouldMove)
	{
		if (m_jump && m_pPlayer->GetActorStats()->inAir == 0.f && ! m_pPlayer->IsDead())
		{
			request.SetJump();
		}
		request.AddDeltaMovement(m_deltaMovement);
		request.AddDeltaRotation(m_deltaRotation);
		float pseudoSpeed = m_pPlayer->CalculatePseudoSpeed(false, m_deltaMovement.len());
		request.SetPseudoSpeed(pseudoSpeed);
	}
	else
	{
		request.SetPseudoSpeed(0.0f);
	}
	m_pPlayer->GetMovementController()->RequestMovement(request);

	IItem* pItem = m_pPlayer->GetCurrentItem();
	if (pItem)
	{
		IWeapon* pWeapon = pItem->GetIWeapon();
		if (pWeapon)
		{
			IFireMode *cfm = pWeapon->GetFireMode(pWeapon->GetCurrentFireMode());
			if ( cfm )
			{
				if (IInventory* pInventory = m_pPlayer->GetInventory())
				{
					int ac = max(pInventory->GetAmmoCount(cfm->GetAmmoType()), cfm->GetAmmoCount());
					if (0 == ac)
					{
						IItemSystem *iis = g_pGame->GetIGameFramework()->GetIItemSystem();
						iis->DropActorItem(m_pPlayer, pItem->GetEntityId());

						pInventory->RemoveItem(pItem->GetEntityId());
						iis->GiveItem(m_pPlayer, pItem->GetEntity()->GetClass()->GetName(), true, true, true);
					}
				}
				pWeapon->SetAmmoCount( cfm->GetAmmoType(), 100 );
			}
			if (shouldFire)
			{
				CWeapon *pcweap = (CWeapon*) pWeapon;

				bool currWeaponExplosive = false; 
				IInventory::EInventorySlots inventorySlot = IInventory::eInventorySlot_Last;
				IItem *pCurrentItem = m_pPlayer->GetCurrentItem(false);

				if (pCurrentItem)
				{
					IItemSystem *iis = g_pGame->GetIGameFramework()->GetIItemSystem();
					if (iis && m_pPlayer->GetInventory())
					{
						if (const char* const currentWeaponCat = iis->GetItemCategory(pCurrentItem->GetEntity()->GetClass()->GetName()))
						{
							inventorySlot = m_pPlayer->GetInventory()->GetSlotForItemCategory(currentWeaponCat);
							currWeaponExplosive = (inventorySlot == IInventory::eInventorySlot_Explosives) || (inventorySlot == IInventory::eInventorySlot_Grenades);
						}
					}
				}

				switch (m_mode)
				{
				case E_ZOOM_IN_OUT:
					{
						//					CryLog ("[DemoInput] Waiting to zoom in/out the %s belonging to %s '%s' - isZoomed=%d busy=%d zooming=%d selected=%d canZoom=%d", pItem->GetEntity()->GetName(), m_pPlayer->GetEntity()->GetClass()->GetName(), m_pPlayer->GetEntity()->GetName(), pWeapon->IsZoomed(), pItem->IsBusy(), pWeapon->IsZoomingInOrOut(), pItem->IsSelected(), pWeapon->CanZoom());
						if (! pWeapon->CanZoom() || !m_pPlayer->IsClient())
						{
							//						CryLog ("[DemoInput] %s '%s' can't zoom with current weapon!", m_pPlayer->GetEntity()->GetClass()->GetName(), m_pPlayer->GetEntity()->GetName());
							m_timeBeforeChangeMode = 0.f;
						}
						else if (! pItem->IsBusy() && !pWeapon->IsZoomingInOrOut() && pItem->IsSelected())
						{
							if (pWeapon->IsZoomed())
							{
								//							CryLog ("[DemoInput] %s '%s' zooming out!", m_pPlayer->GetEntity()->GetClass()->GetName(), m_pPlayer->GetEntity()->GetName());
								pWeapon->StopZoom(m_pPlayer->GetEntityId());
							}
							else
							{
								//							CryLog ("[DemoInput] %s '%s' zooming in!", m_pPlayer->GetEntity()->GetClass()->GetName(), m_pPlayer->GetEntity()->GetName());
								pWeapon->StartZoom(m_pPlayer->GetEntityId());
							}

							if (cry_random(0, 1))
							{
								m_timeBeforeChangeMode = 0.f;
							}
						}
					}
					break;

				case E_EXPLOSIVE:
					{
						if (!currWeaponExplosive)
						{
							m_pPlayer->SelectNextItem(1, true, eICT_Explosive|eICT_Grenade);
							break;
						}

						if (cfm)
						{
							switch(m_grenadeState)
							{
							case GRENADE_PREPARE:
								if (m_grenadeStateTime > 0.5f)
								{
									m_grenadeStateTime = 0.f;
									pWeapon->StartFire();
									m_grenadeState = GRENADE_PREPARE_WAIT;
								}
								break;
							case GRENADE_PREPARE_WAIT:
								if (m_grenadeStateTime > 0.5f)
								{
									m_grenadeStateTime = 0.f;
									pWeapon->StopFire();
									m_grenadeState = GRENADE_THROW_WAIT;
								}
								break;
							case GRENADE_THROW_WAIT:
								if (m_grenadeStateTime > 0.5f)
								{
									m_grenadeStateTime = 0.f;
									m_pPlayer->SelectNextItem(1, true, eICT_Explosive|eICT_Grenade);
									m_grenadeState = GRENADE_PREPARE;
								}
								break;
							}
						}

						break;
					}
				case E_SMALLMEDIUM:
					{
						if (currWeaponExplosive)
						{
							m_pPlayer->SelectNextItem(1, true, eICT_Primary | eICT_Secondary);
							break;
						}

						pWeapon->StartFire();
						break;
					}
				case E_MELEE:
					if (! pItem->IsBusy() && !pWeapon->IsZoomingInOrOut() && pItem->IsSelected())
					{
						pWeapon->MeleeAttack();
						if (cry_random(0, 1))
						{
							m_timeBeforeChangeMode = 0.f;
						}
					}
					break;
				}
			}
			else
			{
				if ( cfm )
				{
					cfm->StopFire();
				}
			}
		}
	}
	else
	{
		m_pPlayer->SelectNextItem(1, true, eICT_Primary | eICT_Secondary);
		m_timeBeforeChangeMode = 0.f;
	}
}

void CDedicatedInput::SelectEquipmentPackage()
{
	CEquipmentLoadout *pEquipmentLoadout = g_pGame->GetEquipmentLoadout();
	CGameRules *pGameRules = g_pGame->GetGameRules();

	if (pEquipmentLoadout && pGameRules)
	{
		if (m_pPlayer->IsClient())
		{
			const int packIndex = 0;	// TODO: Could get them to select random loadouts
			pEquipmentLoadout->SetSelectedPackage(packIndex);

			pGameRules->SetPendingLoadoutChange();
		}
		else
		{
			// This will set the equipment loadout to be the same as the local player
			pEquipmentLoadout->ClSendCurrentEquipmentLoadout(m_pPlayer->GetEntityId());
		}
	}
}

void CDedicatedInput::HandleDeathAndSuicide(CGameRules* const pGameRules)
{
	if (m_pPlayer->IsDead())
	{

#if !defined(_RELEASE)
		if (g_pGameCVars->g_dummyPlayersShowDebugText)
		{
			CryWatch ("$4%s '%s' is dead!", m_pPlayer->GetEntity()->GetClass()->GetName(), m_pPlayer->GetEntity()->GetName());
		}
#endif

		m_timeUntilRequestRespawn -= gEnv->pTimer->GetFrameTime();

		if(m_timeUntilRequestRespawn <= 0.f)
		{
			SelectEquipmentPackage();

			IGameRulesSpawningModule *pSpawningModule = pGameRules->GetSpawningModule();

			if (pSpawningModule && pSpawningModule->GetRemainingLives(m_pPlayer->GetEntityId()) > 0)
			{
				pSpawningModule->ClRequestRevive(m_pPlayer->GetEntityId());
			}

			m_timeUntilRequestRespawn = cry_random(1.0f, 7.0f);
		}
		m_timeUntilDie = cry_random(30.0f, 90.0f);
	}
	else if (g_pGameCVars->g_dummyPlayersCommitSuicide)
	{

#if !defined(_RELEASE)
		if (g_pGameCVars->g_dummyPlayersShowDebugText)
		{
			IItem * item = m_pPlayer->GetCurrentItem();
			CryWatch ("$3%s '%s' [%s] will commit suicide in %.1f seconds", m_pPlayer->GetEntity()->GetClass()->GetName(), m_pPlayer->GetEntity()->GetName(), item ? item->GetEntity()->GetClass()->GetName() : "", m_timeUntilDie);
		}
#endif

		m_timeUntilDie -= gEnv->pTimer->GetFrameTime();

		if (m_timeUntilDie <= 0.f)
		{
			HitInfo suicideInfo(m_pPlayer->GetEntityId(), m_pPlayer->GetEntityId(), m_pPlayer->GetEntityId(),
				1000.0f, 0.0f, 0, -1, CGameRules::EHitType::Normal, ZERO, ZERO, ZERO);

			pGameRules->SanityCheckHitInfo(suicideInfo, "CDedicatedInput::PreUpdate");
			pGameRules->ClientHit(suicideInfo);
		}
	}
}

void CDedicatedInput::PostUpdate()
{
	SMovementState movementState;
	m_pPlayer->GetMovementController()->GetMovementState(movementState);
	m_serializedInput.deltaMovement = m_deltaMovement;
	m_serializedInput.lookDirection = movementState.eyeDirection;
	m_serializedInput.bodyDirection = movementState.entityDirection;
	m_serializedInput.stance = m_stance;
}

void CDedicatedInput::OnAction( const ActionId& action, int activationMode, float value )
{
}

void CDedicatedInput::SetState( const SSerializedPlayerInput& input )
{
	GameWarning("CDedicatedInput::SetState called - should not happen");
}

void CDedicatedInput::GetState( SSerializedPlayerInput& input )
{
	input = m_serializedInput;

	// This code is taken from CPlayerInput::GetState
	if (g_pGameCVars->pl_serialisePhysVel)
	{
		//--- Serialise the physics vel instead, velocity over the NET_SERIALISE_PLAYER_MAX_SPEED will be clamped by the network so no guards here
		IPhysicalEntity* pEnt = m_pPlayer->GetEntity()->GetPhysics();
		if (pEnt)
		{
			pe_status_dynamics dynStat;
			pEnt->GetStatus(&dynStat);

			input.deltaMovement = dynStat.v / g_pGameCVars->pl_netSerialiseMaxSpeed;
			input.deltaMovement.z = 0.0f;
		}
	}
	else
	{
		Quat worldRot = m_pPlayer->GetBaseQuat();
		input.deltaMovement = worldRot.GetNormalized() * m_deltaMovement;
		// ensure deltaMovement has the right length
		input.deltaMovement = input.deltaMovement.GetNormalizedSafe(ZERO) * m_deltaMovement.GetLength();
	}
}

void CDedicatedInput::Reset()
{
	m_deltaMovement.Set(0.0f, 0.0f, 0.0f);
	m_deltaRotation.Set(0.0f, 0.0f, 0.0f);
	m_targetDeltaMovement.Set(0.0f, 0.0f, 0.0f);
	m_targetDeltaRotation.Set(0.0f, 0.0f, 0.0f);
	m_interpolateToNewMoveLookInputs = 0.f;
	m_interpolationSpeed = 1.f;
}

void CDedicatedInput::DisableXI(bool disabled)
{
}

IPlayerInput::EInputType CDedicatedInput::GetType() const
{
	return DEDICATED_INPUT;
}

void CDedicatedInput::GetMemoryUsage(ICrySizer * pSizer) const
{
	pSizer->Add(*this);
}

#endif //USE_DEDICATED_INPUT
