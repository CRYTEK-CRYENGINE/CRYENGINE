// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include <StdAfx.h>
#include "VTOLVehicleManager.h"
#include "GameCVars.h"
#include "Actor.h"
#include "Player.h"
#include <IVehicleSystem.h>
#include "VehicleMovementBase.h"
#include <CryMath/Cry_GeoOverlap.h>
#include "GameRules.h"
#include "VehicleMovementMPVTOL.h"
#include "Battlechatter.h"
#include "StatsRecordingMgr.h"

#include "PersistantStats.h"
#include "Utility/CryWatch.h"

const char * CVTOLVehicleManager::s_explosionEffectName = "vehicles.MH60_BlackHawk.Damage.Destroyed";
const char * CVTOLVehicleManager::s_VTOLClassName		= "US_VTOL_GunShip";

CVTOLVehicleManager::CVTOLVehicleManager() :	
	m_pVehicleSystem(NULL), 
	m_explosionSignalId(INVALID_AUDIOSIGNAL_ID), 
	m_pExplosionEffect(NULL), 
	m_moduleRMIIndex(-1),
	m_bRegisteredWithPathFollower(false), 
	m_classId(-1)
{
	// Cache commonly required systems - 
	// NOTE : could just dependency inject these through in constructor
  m_pVehicleSystem = g_pGame->GetIGameFramework()->GetIVehicleSystem();
	assert( m_pVehicleSystem != NULL );
}

CVTOLVehicleManager::~CVTOLVehicleManager()
{
	Reset();

	RegisterVTOLWithPathFollower(false);

	SAFE_RELEASE(m_pExplosionEffect);

	CGameRules *pGameRules = g_pGame->GetGameRules();
	if (pGameRules)
	{
		pGameRules->UnRegisterModuleRMIListener(m_moduleRMIIndex);
		pGameRules->UnRegisterClientConnectionListener(this);
	}
}

void CVTOLVehicleManager::Init()
{
	CGameAudio *pGameAudio = g_pGame->GetGameAudio();

	m_explosionSignalId = pGameAudio->GetSignalID("ExplosionGenericLarge");
	//CRY_ASSERT(m_explosionSignalId != INVALID_AUDIOSIGNAL_ID);

	m_pExplosionEffect = gEnv->pParticleManager->FindEffect(s_explosionEffectName);
	if (m_pExplosionEffect)
	{
		m_pExplosionEffect->AddRef();
	}
	else
	{
		CryWarning(VALIDATOR_MODULE_GAME, VALIDATOR_ERROR, "Failed to find explosion effect %s", s_explosionEffectName);
	}

	CGameRules *pGameRules = g_pGame->GetGameRules();
	m_moduleRMIIndex = pGameRules->RegisterModuleRMIListener(this);
	pGameRules->RegisterClientConnectionListener(this);
}

void CVTOLVehicleManager::InitClient(int channelID)
{
	int numVehicles=0;
	for(TVTOLList::const_iterator iter = m_vtolList.begin(), end = m_vtolList.end(); iter!=end; ++iter )
	{
		numVehicles++;
		CRY_ASSERT_MESSAGE(numVehicles <= 1, "We only currently support handling a single VTOL here");

		const SVTOLInfo& info = iter->second;
		if(IVehicle* pVehicle = g_pGame->GetIGameFramework()->GetIVehicleSystem()->GetVehicle(info.entityId))
		{
			if(CVehicleMovementMPVTOL* pVTOLMovement = static_cast<CVehicleMovementMPVTOL*>(pVehicle->GetMovement()))
			{
				SPathFollowingAttachToPathParameters params;
				if(pVTOLMovement->FillPathingParams(params))
				{
					params.classId = m_classId;
					params.pathFollowerId = info.entityId;
					params.shouldStartAtInitialNode = false;
					params.forceSnap = true;
					g_pGame->GetGameRules()->GetGameObject()->InvokeRMIWithDependentObject(CGameRules::ClPathFollowingAttachToPath(), params, eRMI_ToClientChannel|eRMI_NoLocalCalls, info.entityId, channelID);
				}
			}
		}

		if (info.state == EVS_Invisible)
		{
			// VTOL is hidden, ensure the new client hides it
			CGameRules::SModuleRMIEntityParams params;
			params.m_listenerIndex = m_moduleRMIIndex;
			params.m_entityId = info.entityId;
			params.m_data = eRMITypeSingleEntity_vtol_hidden;

			CryLog("CVTOLVehicleManager::OnClientEnteredGame() has found the VTOL is invisible, telling clients to hide it");
			
			// RMI will patch entityIDs between clients
			g_pGame->GetGameRules()->GetGameObject()->InvokeRMIWithDependentObject(CGameRules::ClModuleRMISingleEntity(), params, eRMI_ToClientChannel|eRMI_NoLocalCalls, info.entityId, channelID);
		}
	}
}

void CVTOLVehicleManager::Reset()
{
	//Drop all the registered Ids
	IEntitySystem *pEntitySystem = gEnv->pEntitySystem;
	for (TVTOLList::const_iterator it = m_vtolList.begin(), end = m_vtolList.end(); it != end; ++ it)
	{
		const EntityId vtolId = it->second.entityId;
		pEntitySystem->RemoveEntityEventListener(vtolId, ENTITY_EVENT_RESET, this);
		pEntitySystem->RemoveEntityEventListener(vtolId, ENTITY_EVENT_DONE, this);
		pEntitySystem->RemoveEntityEventListener(vtolId, ENTITY_EVENT_HIDE, this);
		pEntitySystem->RemoveEntityEventListener(vtolId, ENTITY_EVENT_UNHIDE, this);
	}
	m_vtolList.clear();
}

void CVTOLVehicleManager::Update(float frameTime)
{
	// Update logic goes here
	if(CGameRules* pGameRules = g_pGame->GetGameRules())
	{
		CGameRules::TPlayers players;
		pGameRules->GetPlayers(players);

		for (TVTOLList::iterator iter=m_vtolList.begin(), end=m_vtolList.end(); iter!=end; ++iter)
		{
			SVTOLInfo& info = iter->second;
			IVehicle* pVehicle = m_pVehicleSystem->GetVehicle( info.entityId );
			if(pVehicle)
			{
				info.stateTime += frameTime;

				// Find difference in rotation
				Quat newRotation = pVehicle->GetEntity()->GetWorldRotation();
				newRotation.Normalize();
				Quat rotationDifference = newRotation*info.location.q.GetInverted();
				rotationDifference.Normalize();

				// Store new rotation + position
				info.location.q = newRotation;
				info.location.t = pVehicle->GetEntity()->GetWorldPos();

				if(info.state==EVS_Normal)
				{
					pVehicle->NeedsUpdate(IVehicle::eVUF_AwakePhysics);

					if(CVehicleMovementMPVTOL* pMovement = static_cast<CVehicleMovementMPVTOL*>(pVehicle->GetMovement()))
					{
						const uint8 pathComplete = pMovement->GetPathComplete();
						if(pathComplete==1)
						{
							if(CMPPathFollowingManager* pMPPathFollowingManager = g_pGame->GetGameRules()->GetMPPathFollowingManager())
							{
								pMPPathFollowingManager->NotifyListenersOfPathCompletion(info.entityId);
							}
							pMovement->SetPathCompleteNotified();
						}
					}
				}

				// Check status
				if(info.state==EVS_Normal && pVehicle->GetDamageRatio() >= 1.f)
				{
					if(gEnv->bServer)
					{
						DestructionDamageRatioReached(pVehicle, info, frameTime);
					}
				}
				else if(info.state == EVS_Invisible)
				{
					if(info.stateTime > g_pGameCVars->g_VTOLRespawnTime)
					{
						RespawnVTOL(pVehicle, info);
					}
				}

				//Process players
				TPlayerList& currentPlayerList = info.playersInside;
				TPlayerList oldPlayerList = currentPlayerList;
				currentPlayerList.clear();

				CGameRules::TPlayers::iterator iterPlayer = players.begin();
				const CGameRules::TPlayers::const_iterator endPlayer = players.end();
				while(iterPlayer != endPlayer)
				{
					// Adding safeguard to protect against cases where user alt-f4 quits program. 
					UpdateEntityInVTOL(info, *iterPlayer); 
					++iterPlayer;
				}
				
				//Find out who has been inside since the last update, who has just entered, and who has left
				TPlayerStatusList playerStatusList;
				int currentId;
				for(unsigned int prev = 0; prev < currentPlayerList.size(); ++prev)
				{
					currentId = currentPlayerList[prev];
					bool found = false;
					TPlayerList::iterator oldIter = oldPlayerList.begin();
					TPlayerList::iterator oldEnd = oldPlayerList.end();
					while(oldIter != oldEnd)
					{
						if(currentId == *oldIter) //In both lists so still inside
						{
							found = true;
							playerStatusList.push_back( TPlayerStatus(currentId, E_PEVS_StillInside) );
							oldPlayerList.erase(oldIter);
							break;
						}
						++oldIter;
					}
					if(!found) //Only in current list so entered
					{
						playerStatusList.push_back( TPlayerStatus(currentId, E_PEVS_Entered) );
					}
				}
				//Those remaining in old list have exited
				for(unsigned int old = 0; old < oldPlayerList.size(); ++old) 
				{
					playerStatusList.push_back( TPlayerStatus(oldPlayerList[old], E_PEVS_Exited) );
				}

				//Act based on current state
				TPlayerStatusList::iterator statusIter = playerStatusList.begin();
				TPlayerStatusList::iterator statusEnd = playerStatusList.end();
				while(statusIter != statusEnd)
				{
					switch(statusIter->second)
					{
					case E_PEVS_Entered:
						{
							OnPlayerEntered(statusIter->first);
						}
						break;
					case E_PEVS_Exited:
						{
							OnPlayerExited(statusIter->first);
						}
						break;
					}
					++statusIter;
				}

				UpdateRotationOfInternalPlayers(info, playerStatusList, rotationDifference);
			}
		}
	}
}

void CVTOLVehicleManager::UpdateEntityInVTOL( SVTOLInfo& vtolInfo, EntityId entityId )
{
	//////////////////////////////////////////////////////////////////////////////////////////////////////////////
	// Note.. would be nice if we could programatically spawn trigger boxes matching vehicle AABB dimensions		//
	// And attach to VTOLS.. then just listen for OnEnter and OnExit events rather than horrible polling :(			//
	// Spawning programatically avoids Design having to manually set then up (depends if *can* attach though)   //
	//////////////////////////////////////////////////////////////////////////////////////////////////////////////

	IEntity* pEntity = gEnv->pEntitySystem->GetEntity(entityId);
	if(!pEntity)
		return;

	AABB playerAABB;
	pEntity->GetWorldBounds(playerAABB); 

	const bool inVtol = TestIsInVTOL(vtolInfo, playerAABB);

	IActor* pActor = g_pGame->GetIGameFramework()->GetIActorSystem()->GetActor(entityId);
	if(CActor* pCActor = static_cast<CActor*>(pActor))
	{
		pCActor->ImmuneToForbiddenZone(inVtol);
	}

	// Perform AABB OBB intersect test between player and vehicle
	if(inVtol)
	{
		vtolInfo.playersInside.push_back(entityId);
		if(pActor && pActor->IsClient())
		{
			//In case we get in the VTOL after having already left the battlefield - turn off the warning UI
			CHUDEventDispatcher::CallEvent(SHUDEvent(eHUDEvent_ReturningToBattleArea));
		}
	}
}

bool CVTOLVehicleManager::TestIsInVTOL( const SVTOLInfo& info, const AABB& aabb ) const
{
	OBB obb;
	obb.SetOBBfromAABB(info.location.q, info.localBounds);
	obb.h.x *= g_pGameCVars->g_VTOLInsideBoundsScaleX;
	obb.h.y *= g_pGameCVars->g_VTOLInsideBoundsScaleY;
	obb.h.z *= g_pGameCVars->g_VTOLInsideBoundsScaleZ;
	obb.h.z += g_pGameCVars->g_VTOLInsideBoundsOffsetZ;
	return Overlap::AABB_OBB(aabb, info.location.t, obb);
}

//Takes a list of player IDs and changes their view rotation by the amount the vtol has rotated in the last frame
void CVTOLVehicleManager::UpdateRotationOfInternalPlayers(SVTOLInfo& info, const TPlayerStatusList& playerStatuses, const Quat& rotationDifference)
{
	const Ang3 angRot(0.f,0.f,rotationDifference.GetRotZ());
	if(angRot.z == 0.f)
	{
		return;
	}

	if(const int numPlayers = playerStatuses.size())
	{
		if(IEntity* pVTOL = gEnv->pEntitySystem->GetEntity(info.entityId))
		{
			const Matrix34 invertedWorldTM(pVTOL->GetWorldTM().GetInverted());

			for (int i = 0; i < numPlayers; ++ i)
			{
				const TPlayerStatus &playerStatus = playerStatuses[i];
				if (playerStatus.second == E_PEVS_StillInside)
				{
					if(CPlayer* pPlayer = GetPlayerFromEntityId(playerStatus.first))
					{
						if(IItem *pCurrentItem = pPlayer->GetCurrentItem())
						{
							if(IWeapon *pWeapon = pCurrentItem->GetIWeapon())
							{
								if(pWeapon->IsZoomed())
								{
									continue;
								}
							}
						}
						if( info.localBounds.IsContainPoint(invertedWorldTM*pPlayer->GetEntity()->GetWorldPos()) )
						{
							pPlayer->AddViewAngles(angRot);
						}
					}
				}
			}
		}
	}
}

void CVTOLVehicleManager::OnPlayerEntered(EntityId playerId)
{
	IEntity* pEntity = gEnv->pEntitySystem->GetEntity(playerId);
	if(pEntity)
	{
		//Increase the max ground velocity when entering - allows the player to keep moving with the vehicle at faster speeds than normal
		if( IPhysicalEntity* pPhysics = pEntity->GetPhysics() )
		{
			pe_player_dynamics playerDynamics;
			if(pPhysics->GetParams(&playerDynamics))
			{
				float currentMaxVelGround = playerDynamics.maxVelGround;

				pe_player_dynamics newPlayerDynamics;
				newPlayerDynamics.maxVelGround = currentMaxVelGround + g_pGameCVars->g_VTOLMaxGroundVelocity;
				pPhysics->SetParams(&newPlayerDynamics);
			}
		}

		if (CGameRules* pGameRules = g_pGame->GetGameRules()) 
		{
			if (CBattlechatter* pBattlechatter = pGameRules->GetBattlechatter())
			{
				pBattlechatter->PlayerHasEnteredAVTOL(pEntity);
			}
		}

		if( IActorSystem* pActorSystem = g_pGame->GetIGameFramework()->GetIActorSystem() )
		{
			if( IActor* pPlayerActor = pActorSystem->GetActor( playerId ) )
			{
				CStatsRecordingMgr::TryTrackEvent( pPlayerActor, eGSE_VTOL_EnterExit, true );
			}
		}

		if( CPersistantStats* pStats = CPersistantStats::GetInstance() )
		{
			pStats->OnEnteredVTOL( playerId );
		}

		// Set custom draw near z range for camera motion blur
		const bool bForceValue = true;
		const float customDrawNearZRange = 0.95f;
		gEnv->p3DEngine->SetPostEffectParam( "MotionBlur_DrawNearZRangeOverride", customDrawNearZRange, bForceValue );
	}
}

void CVTOLVehicleManager::OnPlayerExited(EntityId playerId)
{
	IEntity* pEntity = gEnv->pEntitySystem->GetEntity(playerId);
	if(pEntity)
	{
		//Decrease the max ground velocity when exiting
		if( IPhysicalEntity* pPhysics = pEntity->GetPhysics() )
		{
			pe_player_dynamics playerDynamics;
			if(pPhysics->GetParams(&playerDynamics))
			{
				float currentMaxVelGround = playerDynamics.maxVelGround;

				pe_player_dynamics newPlayerDynamics;
				newPlayerDynamics.maxVelGround = currentMaxVelGround - g_pGameCVars->g_VTOLMaxGroundVelocity;
				pPhysics->SetParams(&newPlayerDynamics);
			}
		}

		if( IActorSystem* pActorSystem = g_pGame->GetIGameFramework()->GetIActorSystem() )
		{
			if( IActor* pPlayerActor = pActorSystem->GetActor( playerId ) )
			{
				CStatsRecordingMgr::TryTrackEvent( pPlayerActor, eGSE_VTOL_EnterExit, false );
			}
		}

		if( CPersistantStats* pStats = CPersistantStats::GetInstance() )
		{
			pStats->OnExitedVTOL( playerId );
		}

		// Reset custom draw near z range for camera motion blur
		const bool bForceValue = true;
		gEnv->p3DEngine->SetPostEffectParam( "MotionBlur_DrawNearZRangeOverride", 0.0f, bForceValue );
	}
}

CPlayer* CVTOLVehicleManager::GetPlayerFromEntityId(const EntityId entityId) const
{
	if(IActorSystem* pActorSystem = g_pGame->GetIGameFramework()->GetIActorSystem())
	{
		if(IActor* pActor = pActorSystem->GetActor(entityId))
		{
			if(pActor->IsPlayer())
			{
				return static_cast<CPlayer*>(pActor);
			}
		}
	}
	return NULL;
}

void CVTOLVehicleManager::RegisterVTOL(EntityId entityId)
{
	if(g_pGameCVars->g_mpNoVTOL)
	{
		gEnv->pEntitySystem->RemoveEntity(entityId);
	}
	else
	{
		RegisterVTOLWithPathFollower(true);

		IEntity* pVTOLEntity = gEnv->pEntitySystem->GetEntity(entityId);
		if(pVTOLEntity)
		{
			SVTOLInfo& newInfo = m_vtolList[entityId];
			newInfo.Reset();
			newInfo.playersInside.reserve(8);
			newInfo.entityId = entityId;
			newInfo.location = QuatT(pVTOLEntity->GetWorldTM());
			CalculateBounds(pVTOLEntity, newInfo);

			gEnv->pEntitySystem->AddEntityEventListener(entityId, ENTITY_EVENT_DONE, this);
			gEnv->pEntitySystem->AddEntityEventListener(entityId, ENTITY_EVENT_RESET, this);
			gEnv->pEntitySystem->AddEntityEventListener(entityId, ENTITY_EVENT_HIDE, this);
			gEnv->pEntitySystem->AddEntityEventListener(entityId, ENTITY_EVENT_UNHIDE, this);

			SetupMovement(entityId);

			SmartScriptTable props, respawnprops;
			if (pVTOLEntity->GetScriptTable()->GetValue("Properties", props) && props->GetValue("Respawn", respawnprops))
			{
				respawnprops->SetValue("bAbandon", 0);
			}
		}
	}
}

void CVTOLVehicleManager::RespawnVTOL(IVehicle* pVehicle, SVTOLInfo& info)
{
	CryLog("CVTOLVehicleManager::RespawnVTOL()");

	//Reset parts and component damage
	int numComponents = pVehicle->GetComponentCount();
	for(int c = 0; c < numComponents; ++c)
	{
		IVehicleComponent* pComponent = pVehicle->GetComponent(c);
		int numParts = pComponent->GetPartCount();
		for(int p = 0; p < numParts; ++p)
		{
			pComponent->GetPart(p)->Reset();
		}
		pComponent->SetDamageRatio(0.f);
	}

	if (IVehicleMovement* pMovement = pVehicle->GetMovement())
	{
		pMovement->Reset();
	}

	SVehicleStatus& status = const_cast<SVehicleStatus&>(pVehicle->GetStatus());
	status.Reset();

	//Activate flowgraph Respawn output
	IScriptTable* pScriptTable = pVehicle->GetEntity()->GetScriptTable();
	Script::CallMethod(pScriptTable, "Respawn", pScriptTable, 0);
	info.state = EVS_Normal;
	info.stateTime = 0.f;

	pVehicle->GetEntity()->Hide(false);
	LockSeats(pVehicle, false);

	SetupMovement(info.entityId);

	//Add to HUD Radar
	SHUDEvent hudEvent(eHUDEvent_AddEntity);
	hudEvent.AddData((int)info.entityId);
	CHUDEventDispatcher::CallEvent(hudEvent);
}

void CVTOLVehicleManager::OnEntityEvent( IEntity *pEntity, const SEntityEvent& event )
{
	if(event.event == ENTITY_EVENT_DONE)
	{
		EntityId entityId = pEntity->GetId();
		m_vtolList.erase(entityId);
		gEnv->pEntitySystem->RemoveEntityEventListener(entityId, ENTITY_EVENT_RESET, this);
		gEnv->pEntitySystem->RemoveEntityEventListener(entityId, ENTITY_EVENT_DONE, this);
		gEnv->pEntitySystem->RemoveEntityEventListener(entityId, ENTITY_EVENT_HIDE, this);
		gEnv->pEntitySystem->RemoveEntityEventListener(entityId, ENTITY_EVENT_UNHIDE, this);
	}
	else if(event.event == ENTITY_EVENT_RESET)
	{
		// Only on leaving the editor gamemode
		if(event.nParam[0] == 0)
		{
			ResetVehicle(m_vtolList[pEntity->GetId()]);
		}
	}
	else if(event.event == ENTITY_EVENT_HIDE)
	{
		// Remove from HUD radar
		SHUDEvent hudEvent(eHUDEvent_RemoveEntity);
		hudEvent.AddData((int)pEntity->GetId());
		CHUDEventDispatcher::CallEvent(hudEvent);
	}
	else if(event.event == ENTITY_EVENT_UNHIDE)
	{
		// Add to HUD radar
		SHUDEvent hudEvent(eHUDEvent_AddEntity);
		hudEvent.AddData((int)pEntity->GetId());
		CHUDEventDispatcher::CallEvent(hudEvent);
	}
}

void CVTOLVehicleManager::CalculateBounds(IEntity* pVTOLEntity, SVTOLInfo& info)
{
	// Work out final local bounds - includes all children's bounds
	AABB aabb;
	pVTOLEntity->GetLocalBounds(aabb);

	const int numChildren = pVTOLEntity->GetChildCount();
	AABB childAABB;
	for(int child = 0; child < numChildren; ++child)
	{
		if(IEntity* pChild = pVTOLEntity->GetChild(child))
		{
			pChild->GetLocalBounds(childAABB);
			aabb.Add(childAABB);
		}
	}

	info.localBounds = aabb;
}

void CVTOLVehicleManager::LockSeats(IVehicle* pVehicle, bool lock)
{
	EVehicleSeatLockStatus newStatus = lock ? eVSLS_LockedForPlayers : eVSLS_Unlocked;

	const int numSeats = pVehicle->GetSeatCount();
	for(int i = 1; i <= numSeats; ++i)
	{
		IVehicleSeat* pSeat = pVehicle->GetSeatById(i);
		if(pSeat && pSeat->GetLockedStatus() != eVSLS_Locked)
		{
			pSeat->SetLocked(newStatus);
		}
	}
}

void CVTOLVehicleManager::CreateExplosion(IParticleEffect *inParticleEffect, const Vec3& pos, const float inEffectScale, TAudioSignalID inAudioSignal/*=INVALID_AUDIOSIGNAL_ID*/)
{
#if !defined(DEDICATED_SERVER)
	CRY_ASSERT_MESSAGE(inParticleEffect, "CreateExplosion() passsed a NULL inParticleEffect");
#endif

	if (gEnv->bServer)
	{
		// server wants to create actual explosions
		CGameRules *pGameRules = g_pGame->GetGameRules();

		ExplosionInfo explosionInfo;
		explosionInfo.pParticleEffect = inParticleEffect;
		explosionInfo.effect_name = s_explosionEffectName;// needed so the explosion can be synced to clients
		explosionInfo.type = pGameRules->GetHitTypeId("explosion");
		explosionInfo.effect_scale = inEffectScale; // this is likely not connected up in the particle effect but should be able to be so that the effect can be adjusted dynamically
		explosionInfo.pos = pos; 
		explosionInfo.dir.Set(0.0f,-1.0f,0.0f);
		explosionInfo.effect_scale = cry_random(0.9f,1.1f);
		explosionInfo.pressure = 1000.0f;
		explosionInfo.maxblurdistance = 10.0;
		explosionInfo.radius = 12.0f;
		explosionInfo.blindAmount = 0.0f;
		explosionInfo.flashbangScale = 8.0f;
		explosionInfo.damage = 0.0f;
		explosionInfo.hole_size = 0.0f;

		pGameRules->QueueExplosion(explosionInfo);
	}
	// the gamerules SHOULD be syncing the server queued ones above to clients!

	if (inAudioSignal != INVALID_AUDIOSIGNAL_ID)
	{
		CAudioSignalPlayer::JustPlay(inAudioSignal, pos);
	}
}

void CVTOLVehicleManager::DestroyVTOL(IEntity *pVTOL, SVTOLInfo &info)
{
	const Matrix34& mat = pVTOL->GetWorldTM();
	const float k_offsetDist = 5.0f;

	CreateExplosion(m_pExplosionEffect, mat.GetTranslation(), 2.0f, m_explosionSignalId);
	CreateExplosion(m_pExplosionEffect, mat.TransformPoint(Vec3(0.f, k_offsetDist, 0.f)), 2.0f);
	CreateExplosion(m_pExplosionEffect, mat.TransformPoint(Vec3(0.f,-k_offsetDist, 0.f)), 2.0f);

	//Hide existing vehicle
	pVTOL->SetPos(Vec3(0.f,0.f,0.f));
	pVTOL->Hide(true);
	info.state = EVS_Invisible;
	info.stateTime = 0.f;  // this may allow clients to do their own respawn handling, stopping the need for respawned RMI below

	//Remove from HUD radar
	SHUDEvent hudEvent(eHUDEvent_RemoveEntity);
	hudEvent.AddData((int)pVTOL->GetId());
	CHUDEventDispatcher::CallEvent(hudEvent);
}

void CVTOLVehicleManager::DestructionDamageRatioReached(IVehicle* pVehicle, SVTOLInfo& info, float frameTime)
{
	pVehicle->ClientEvictAllPassengers();
	LockSeats(pVehicle, true);

	IPhysicalEntity* pPhysics = pVehicle->GetEntity()->GetPhysics();
	if(pPhysics)
	{
		DestroyVTOL(pVehicle->GetEntity(), info);

		CGameRules::SModuleRMIEntityParams params;
		params.m_listenerIndex = m_moduleRMIIndex;
		params.m_entityId = info.entityId;
		params.m_data = eRMITypeSingleEntity_vtol_destroyed;

		// RMI will patch entityIDs between clients
		g_pGame->GetGameRules()->GetGameObject()->InvokeRMIWithDependentObject(CGameRules::ClModuleRMISingleEntity(), params, eRMI_ToRemoteClients, info.entityId);
	}

	//Damage any players inside
	CGameRules* pGameRules = g_pGame->GetGameRules();
	CVehicleMovementMPVTOL* pMovement = static_cast<CVehicleMovementMPVTOL*>(pVehicle->GetMovement());

	if (pGameRules && pMovement)
	{
		EntityId destroyerId = pMovement->GetDestroyerId();

		HitInfo hitInfo(destroyerId, 0, pVehicle->GetEntityId(), g_pGameCVars->g_VTOLOnDestructionPlayerDamage, 0, 0, 0, CGameRules::EHitType::VTOLExplosion);

		TPlayerList& currentPlayerList = info.playersInside;
		TPlayerList::const_iterator iter = currentPlayerList.begin();
		TPlayerList::const_iterator end = currentPlayerList.end();
		while(iter != end)
		{
			hitInfo.targetId = *iter;
			pGameRules->ServerHit(hitInfo);

			++iter;
		}
	}
}

void CVTOLVehicleManager::OnAttachRequest( const SPathFollowingAttachToPathParameters& params, const CWaypointPath* pPath )
{
	if(IVehicle* pVehicle = g_pGame->GetIGameFramework()->GetIVehicleSystem()->GetVehicle(params.pathFollowerId))
	{
		if(CVehicleMovementMPVTOL* pVTOLMovement = static_cast<CVehicleMovementMPVTOL*>(pVehicle->GetMovement()))
		{
			SPathFollowingAttachToPathParameters changePathParams(params);
			float fValOut = 0.f;
			CWaypointPath::E_NodeDataType dataTypeOut;
			if(pPath->HasDataAtNode(params.nodeIndex, dataTypeOut, fValOut) && dataTypeOut==CWaypointPath::ENDT_Speed)
			{
				changePathParams.speed = fValOut;
			}
			pVTOLMovement->SetPathInfo(changePathParams, pPath);
		}
	}
	else
	{
		CRY_ASSERT_MESSAGE(false, "CVTOLVehicleManager::OnAttachRequest - VTOL doesn't exist. And it really, really should.");
	}
}

void CVTOLVehicleManager::OnUpdateSpeedRequest( EntityId entityId, float speed )
{
	if(IVehicle* pVehicle = g_pGame->GetIGameFramework()->GetIVehicleSystem()->GetVehicle(entityId))
	{
		if(CVehicleMovementMPVTOL* pVTOLMovement = static_cast<CVehicleMovementMPVTOL*>(pVehicle->GetMovement()))
		{
			pVTOLMovement->UpdatePathSpeed(speed);
		}
	}
}

// message from server received on clients
void CVTOLVehicleManager::OnSingleEntityRMI(CGameRules::SModuleRMIEntityParams params)
{
	switch(params.m_data)
	{
		case eRMITypeSingleEntity_vtol_destroyed:
		{
			CryLog("CVTOLVehicleManager::OnSingleEntityRMI() eRMITypeSingleEntity_vtol_destroyed");
			IVehicle* pVehicle = m_pVehicleSystem->GetVehicle( params.m_entityId );
			CRY_ASSERT_MESSAGE(pVehicle, "have received destroyed VTOL RMI, but cannot find the vehicle for specified entity id");
			if (pVehicle)
			{
				DestroyVTOL(pVehicle->GetEntity(), m_vtolList[params.m_entityId]);
			}
			break;
		}
		case eRMITypeSingleEntity_vtol_hidden:				// for late joining clients only
		{

			CryLog("CVTOLVehicleManager::OnSingleEntityRMI() eRMITypeSingleEntity_vtol_hidden");
			IVehicle* pVehicle = m_pVehicleSystem->GetVehicle(params.m_entityId);
			CRY_ASSERT_MESSAGE(pVehicle, "have received hidden VTOL RMI, but cannot find the vehicle for specified entity id");
			if (pVehicle)
			{		
				//Hide existing vehicle
				IEntity* pVehicleEntity = pVehicle->GetEntity();
				pVehicleEntity->SetPos(Vec3(0.f,0.f,0.f));	// TODO - get Gary's fix for this if any
				pVehicleEntity->Hide(true);

				SVTOLInfo& info = m_vtolList[params.m_entityId];
				info.state = EVS_Invisible;
				info.stateTime = 0.f;		// this may allow clients to do their own respawn handling, stopping the need for respawned RMI below

				SHUDEvent hudEvent(eHUDEvent_RemoveEntity);
				hudEvent.AddData((int)params.m_entityId);
				CHUDEventDispatcher::CallEvent(hudEvent);
			}
			break;
		}
		case eRMITypeSingleEntity_vtol_respawned:
		{
			CryLog("CVTOLVehicleManager::OnSingleEntityRMI() eRMITypeSingleEntity_vtol_respawned");
			IVehicle* pVehicle = m_pVehicleSystem->GetVehicle(params.m_entityId);
			CRY_ASSERT_MESSAGE(pVehicle, "have received respawned VTOL RMI, but cannot find the vehicle for specified entity id");
			if (pVehicle)
			{
				RespawnVTOL(pVehicle, m_vtolList[params.m_entityId]);
			}
			break;
		}
		default:
			CRY_ASSERT_MESSAGE(0, string().Format("unhandled RMI data %d", params.m_data));
			break;
	}
}

void CVTOLVehicleManager::RegisterVTOLWithPathFollower(bool registerVTOL)
{
	if(!registerVTOL || !m_bRegisteredWithPathFollower)
	{
		CGameRules* pGameRules = g_pGame->GetGameRules();
		if(pGameRules)
		{
			CMPPathFollowingManager* pPathFollowingManager = pGameRules->GetMPPathFollowingManager();
			if(pPathFollowingManager)
			{
				if(registerVTOL)
				{
					if(m_classId == (uint16)-1)
					{
						g_pGame->GetIGameFramework()->GetNetworkSafeClassId(m_classId, s_VTOLClassName);
					}
					pPathFollowingManager->RegisterClassFollower(m_classId, this);
					m_bRegisteredWithPathFollower = true;
				}
				else
				{
					pPathFollowingManager->UnregisterClassFollower(m_classId);
					m_bRegisteredWithPathFollower = false;
				}
			}
		}	
	}
}

void CVTOLVehicleManager::SetupMovement(EntityId entityId)
{
	IVehicle* pVehicle = m_pVehicleSystem->GetVehicle(entityId);
	if(pVehicle)
	{
		if (CVehicleMovementBase* pMovement = StaticCast_CVehicleMovementBase(pVehicle->GetMovement()))
		{
			pMovement->Reset();
			pMovement->SetRemotePilot(true);
			pMovement->StartDriving(0);
		}

		IGameObject* pGameObject = pVehicle->GetGameObject(); 

		pGameObject->SetUpdateSlotEnableCondition(pVehicle, IVehicle::eVUS_Always, eUEC_WithoutAI);
		pGameObject->SetUpdateSlotEnableCondition(pVehicle, IVehicle::eVUS_EnginePowered, eUEC_WithoutAI);
		pGameObject->EnableUpdateSlot(pVehicle, IVehicle::eVUS_EnginePowered);
		pGameObject->EnableUpdateSlot(pVehicle, IVehicle::eVUS_Always);
	}
}

void CVTOLVehicleManager::ResetVehicle(SVTOLInfo& info)
{
	info.Reset();
	SetupMovement(info.entityId);
}

bool CVTOLVehicleManager::IsVTOL( EntityId entityId )
{
	return m_vtolList.count(entityId)!=0;
}

void CVTOLVehicleManager::OnOwnClientEnteredGame()
{
	for(TVTOLList::const_iterator iter=m_vtolList.begin(), end=m_vtolList.end(); iter!=end; ++iter)
	{
		const EntityId entityId = iter->second.entityId;
		if(IEntity* pEntity = gEnv->pEntitySystem->GetEntity(entityId))
		{
			if(!pEntity->IsHidden())
			{
				SHUDEvent hudEvent(eHUDEvent_AddEntity);
				hudEvent.AddData((int)entityId);
				CHUDEventDispatcher::CallEvent(hudEvent);
			}
		}
	}
}

bool CVTOLVehicleManager::IsPlayerInVTOL( EntityId entityId ) const
{
	for(TVTOLList::const_iterator iter=m_vtolList.begin(), end=m_vtolList.end(); iter!=end; ++iter)
	{
		const SVTOLInfo& info = iter->second; 
		if( std::find(info.playersInside.begin(), info.playersInside.end(), entityId) != info.playersInside.end() )
		{
			return true;
		}
	}
	return false; 
}

bool CVTOLVehicleManager::TestEntityInVTOL( const IEntity& entity ) const
{
	AABB aabb;
	entity.GetLocalBounds(aabb);
	for(TVTOLList::const_iterator iter=m_vtolList.begin(), end=m_vtolList.end(); iter!=end; ++iter)
	{
		if(TestIsInVTOL(iter->second, aabb))
		{
			return true;
		}
	}
	return false;
}

bool CVTOLVehicleManager::AnyEnemiesInside( EntityId friendlyPlayerId ) const
{
	if( CGameRules* gameRules = g_pGame->GetGameRules() )
	{
		for(TVTOLList::const_iterator iter=m_vtolList.begin(), end=m_vtolList.end(); iter!=end; ++iter)
		{
			const SVTOLInfo& info = iter->second; 
			for( TPlayerList::const_iterator playerIt = info.playersInside.begin(), playerEnd = info.playersInside.end(); playerIt != playerEnd; ++playerIt )
			{
				if( gameRules->GetThreatRating( friendlyPlayerId, *playerIt) == CGameRules::eHostile )
				{
					return true;
				}
			}
		}
	}
	return false;
}

void CVTOLVehicleManager::SVTOLInfo::Reset()
{
	state = EVS_Normal;
	stateTime = 0.f;
	playersInside.clear();
	if(IEntity* pEntity = gEnv->pEntitySystem->GetEntity(entityId))
	{
		location = QuatT(pEntity->GetWorldTM());
	}
}

