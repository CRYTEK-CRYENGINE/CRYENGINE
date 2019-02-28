// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "EnvironmentalWeapon.h"
#include "Player.h"
#include "PickAndThrowWeapon.h"
#include "PickAndThrowUtilities.h"
#include "EntityUtility/EntityScriptCalls.h"
#include "HitDeathReactions.h"
#include "IVehicleSystem.h"
#include "RecordingSystem.h"
#include <CryMath/Cry_GeoOverlap.h>
#include "StatsRecordingMgr.h"
#include "PersistantStats.h"
#include "ActorImpulseHandler.h"
#include "Network/Lobby/GameLobby.h"
#include "EntityUtility/EntityEffectsCloak.h"
#include "PlayerVisTable.h"

#ifndef _RELEASE
#include "Utility/CryWatch.h"
#endif //#ifndef _RELEASE


const float CEnvironmentalWeapon::SWeaponTrackSlice::k_fBeamRad = 0.2f;
const float CEnvironmentalWeapon::SWeaponTrackSlice::k_fInvStepSize = 1.f / ((float)(CEnvironmentalWeapon::SWeaponTrackSlice::k_numTrackPoints-1));

const char* const s_EnvironmentalWeaponName = "PickAndThrowWeapon";
static IEntityClass* s_pEnvironmentalWeaponClass;

const char * CEnvironmentalWeapon::kHealthScriptName = "currentHealth";

// This is the same way Stealth kill does it :( presumably to save sending extra hitInfo params across the network. 
const static float sDamageRequiredForCertainDeath					= 99999.0f;
const static float sNoRequiredKillVelocityWindow					= 0.25f;

// Sweep testing
const static int sMaxEntitiesToConsiderPerSweep = 2; 
static Vec3  s_radiusAdjust(2.0f,2.0f,2.5f); // used to grab all entities in box
static bool  s_bForceSweepTest			= true;

#if ALLOW_SWEEP_DEBUGGING
static bool  s_bRenderTris				= true; 
static bool  s_bRenderFrameStamps		= true; 
static bool  s_bRenderWeaponAABB		= false;
#endif //#if ALLOW_SWEEP_DEBUGGING

namespace EW
{
	void RegisterEvents( IGameObjectExtension& goExt, IGameObject& gameObject )
	{
		const int eventID = eGFE_OnCollision;
		gameObject.UnRegisterExtForEvents( &goExt, NULL, 0 );
		gameObject.RegisterExtForEvents( &goExt, &eventID, 1 );
	}
}


// ~Sweep testing

CEnvironmentalWeapon::CEnvironmentalWeapon()
	: m_prevWeaponTrackerPos(ZERO)
	, m_weaponTrackerVelocity(ZERO)
	, m_lastPos(ZERO)
	, m_initialThrowPos(ZERO)
	, m_overrideHitDir(ZERO)
	, m_overrideHitDirMinRatio(0.0f)
	, m_overrideHitDirMaxRatio(0.0f)
	, m_ragdollPostMeleeImpactSpeed(0.0f)
	, m_ragdollPostThrowImpactSpeed(0.0f)
	, m_mass(100.0f)
	, m_vehicleMeleeDamage(0.0f)
	, m_vehicleThrowDamage(0.0f)
	, m_vehicleThrowMinVelocity(5.0f)
	, m_vehicleThrowMaxVelocity(20.0f)
	, m_healthWhenGrabbed(0.0f)
	, m_noRequiredKillVelocityTimer(0.0f)
	, m_throwerId(0)
	, m_OwnerId(0)
	, m_previousThrowerId(0)
	, m_State(EWS_Initial)
	, m_OnClientHealthChangedFunc(nullptr)
	, m_livingConstraintID(0)
	, m_articulatedConstraintID(0)
	, m_parentVehicleId(0)
	, m_pLinkedVehicle(nullptr)
	, m_logicFlags(ELogicFlag_doInitWeaponState) // Only one flag set by default
	, m_currentAttackState(EAttackStateType_None)
	, m_bCanInstantKill(false)
	, m_bShowShieldedHitIndicator(false)
	, m_rootedAngleDotMin(-1.f)
	, m_rootedAngleDotMax(1.f)
	, m_bClientHasKilledWithThrow(false)
	, m_pDamageZoneHelper(nullptr)
	, m_pVelocityTracker(nullptr)
#ifndef _RELEASE
	, m_throwDir(ZERO)
	, m_throwOrigin(ZERO)
	, m_throwTarget(ZERO)
	, m_weaponTrackerPosAtImpact(ZERO)
#endif //#ifndef _RELEASE
{
	s_pEnvironmentalWeaponClass = gEnv->pEntitySystem->GetClassRegistry()->FindClass( s_EnvironmentalWeaponName );
	CRY_ASSERT_MESSAGE(s_pEnvironmentalWeaponClass, "CEnvironmentalWeapon::CEnvironmentalWeapon - couldn't find weapon class");
}

CEnvironmentalWeapon::~CEnvironmentalWeapon()
{
	if(CGameRules* pGameRules = g_pGame->GetGameRules())
	{
		g_pGame->GetGameRules()->UnRegisterRevivedListener(this); 
	}
}

// Private helpers
bool CEnvironmentalWeapon::LocationWithinFrontalCone(const Vec3& attackerLocation,const Vec3& victimLocation,const Vec3& attackerFacingdir, float attackerHitConeRads) const
{
	// Convert
	const Vec3 vecAttackerToVictim(victimLocation-attackerLocation);
	const float theta = acos(attackerFacingdir.dot(vecAttackerToVictim.GetNormalized()));
	return ( theta < (0.5f * attackerHitConeRads) );
}

float CEnvironmentalWeapon::CalculateVictimMass(IPhysicalEntity* pVictimPhysicalEntity) const
{
	float victimMass = 110.0f; // default mass of a living player (cant we GET this from somewhere...?) 
	if(pVictimPhysicalEntity->GetType() != PE_LIVING)
	{
		pe_simulation_params simulationParams;
		if(pVictimPhysicalEntity->GetParams(&simulationParams))
		{
			victimMass = simulationParams.mass; 
		}
	}
	return victimMass; 
}
// ~private helpers

bool CEnvironmentalWeapon::Init( IGameObject *pGameObject )
{
	SetGameObject(pGameObject);

	if (!pGameObject->BindToNetwork())
	{
		return false;
	}

	if( m_archetypename.length() > 0 )
	{
		IEntityArchetype* pArchetype = gEnv->pEntitySystem->LoadEntityArchetype( m_archetypename.c_str() );

		if( pArchetype )
		{
			IEntity* pEntity = GetEntity();

			IScriptTable *pPropertiesTable = pArchetype->GetProperties();
			if (pPropertiesTable)
				pEntity->GetScriptTable()->SetValue( "Properties", pPropertiesTable );
		}
	}

	if( m_logicFlags & ELogicFlag_hasParentVehicle )
	{
		if( m_parentVehicleId  != 0 )
		{
			//remember to attach to the vehicle later
			m_logicFlags |= ELogicFlag_doVehicleAttach;
		}
		else
		{
			CryWarning( VALIDATOR_MODULE_GAME, VALIDATOR_ERROR, "EnvironmentalWeapon: Missing parent vehicle data in client init, aborting" );
			return false;
		}
	}

	if ( m_logicFlags & ELogicFlag_spawnHidden )
	{
		GetEntity()->Hide(true);
	}
	
	//Attempt to register with the interactive entity monitor (Will fail on statically placed entities and get handled in response to the ENTITY_EVENT_START_LEVEL message instead)
	AttemptToRegisterInteractiveEntity(); 

	g_pGame->GetGameRules()->RegisterRevivedListener(this); 

	m_intersectionAssist.Init(GetEntityId(), g_pGameCVars->pl_pickAndThrow.intersectionAssistDeleteObjectOnEmbed ? CIntersectionAssistanceUnit::eRP_DeleteObject : CIntersectionAssistanceUnit::eRP_SleepObject);

	return true;
}

void CEnvironmentalWeapon::AttemptToRegisterInteractiveEntity()
{
	CActor* pClientActor = static_cast<CActor*>(g_pGame->GetIGameFramework()->GetClientActor());
	if(pClientActor)
	{
		bool bRooted = false;
		GetEntity()->GetScriptTable()->GetValue( "IsRooted", bRooted );

		// Test if can no longer interact / or *can* now interact
		CPlayer* pPlayer = static_cast<CPlayer*>(pClientActor);
		if(m_logicFlags & ELogicFlag_registeredWithIEMonitor)
		{
			if( bRooted || m_OwnerId != 0 || g_pGameCVars->g_mpNoEnvironmentalWeapons )
			{
				m_logicFlags &= ~ELogicFlag_registeredWithIEMonitor;
			}
		}
		else if(!bRooted && m_OwnerId == 0 && g_pGameCVars->g_mpNoEnvironmentalWeapons==0)
		{
			m_logicFlags |= ELogicFlag_registeredWithIEMonitor;
		}
	}
}


void CEnvironmentalWeapon::InitWeaponState_Held()
{	
	// Setup properties 
	IEntity* pWeaponEntity = GetEntity();
	CRY_ASSERT(pWeaponEntity); 

	DoDetachFromParent();

	// Since we are just forcing weapon state here, we dont run through the longwinded pickup anims etc, we are just forcing state
	IActor* pOwnerActor = gEnv->pGameFramework->GetIActorSystem()->GetActor(m_OwnerId);
	if(pOwnerActor)
	{
		// Force the player to attach and hold this weapon
		CRY_ASSERT_MESSAGE(pOwnerActor->GetActorClass() == CPlayer::GetActorClassType(), "CEnvironmentalWeapon::InitWeaponState_Held - Expected player, got something else");
		CPlayer* pPlayer = static_cast<CPlayer*>(pOwnerActor);
		pPlayer->EnterPickAndThrow(GetEntityId());

		EntityId weaponId = pPlayer->GetInventory()->GetItemByClass(s_pEnvironmentalWeaponClass);
		CPickAndThrowWeapon* pPickAndThrowWeapon = weaponId ? static_cast<CPickAndThrowWeapon*>(gEnv->pGameFramework->GetIItemSystem()->GetItem(weaponId)) : NULL;
		if(pPickAndThrowWeapon)
		{
			pPickAndThrowWeapon->QuickAttach();
		}
	}

	// LUA STATE 
	IScriptTable* pScriptTable = pWeaponEntity->GetScriptTable();
	if(pScriptTable)
	{
		pScriptTable->SetValue("bCurrentlyPickable", 0);// Set No longer pickable as is held.
		pScriptTable->SetValue("IsRooted", 0);			// Set No longer rooted as must have been torn from ground.

		// Let lua know who its owner is
		IEntity* pEntity = GetEntity(); 
		EntityScripts::CallScriptFunction(pEntity, pEntity->GetScriptTable(), "SetOwnerID",ScriptHandle(m_OwnerId));
	}

}

void CEnvironmentalWeapon::InitWeaponState_OnGround()
{
	// Setup properties 
	IEntity* pWeaponEntity = GetEntity();
	CRY_ASSERT(pWeaponEntity); 

	DoDetachFromParent();

	// LUA STATE 
	IScriptTable* pScriptTable = pWeaponEntity->GetScriptTable();
	if(pScriptTable)
	{
		pScriptTable->SetValue("bCurrentlyPickable", 1);
		pScriptTable->SetValue("IsRooted", 0);			// Set No longer rooted as must have been torn from ground.

		// Let lua we have no owner
		IEntity* pEntity = GetEntity(); 
		EntityScripts::CallScriptFunction(pEntity, pEntity->GetScriptTable(), "SetOwnerID",ScriptHandle(m_OwnerId));

		// Call OnAttached to ensure lua has chance to run any state logic it would have run when ripped normally
		HSCRIPTFUNCTION onAttached(NULL);
		if (pScriptTable && pScriptTable->GetValue("OnAttached", onAttached))
		{
			Script::Call(gEnv->pScriptSystem, onAttached, pScriptTable);
			gEnv->pScriptSystem->ReleaseFunc(onAttached);
		}

	}

	// Wake up physics for a second to force rigid body pos/orientation synch
	if( IPhysicalEntity* pPE = GetEntity()->GetPhysics() )
	{
		pe_action_awake action_awake;
		action_awake.bAwake = true;
		action_awake.minAwakeTime = 1.f;
		pPE->Action(&action_awake);
	}
}

void CEnvironmentalWeapon::ForceDrop()
{
	if( m_OwnerId )
	{
		InitWeaponState_OnGround();
		SetOwner( 0 );
	}
}


void CEnvironmentalWeapon::UnRoot()
{
	if( IEntity* pEntity = GetEntity() )
	{
		if( IScriptTable* pScriptTable = pEntity->GetScriptTable() )
		{
			pScriptTable->SetValue("IsRooted", 0);
		}

		EntityScripts::CallScriptFunction(pEntity, pEntity->GetScriptTable(), "OnAttached");

		//The lua function above calls Physicalize so now we need to set the no-squash flag
		IPhysicalEntity *pPhysics = pEntity->GetPhysics();
		if(pPhysics)
		{
			pe_params_flags paramFlags; 
			paramFlags.flagsOR  = pef_cannot_squash_players; 
			pPhysics->SetParams(&paramFlags);
		}
	}
}

void CEnvironmentalWeapon::Release()
{
	delete this;
}

void CEnvironmentalWeapon::RequestUse(EntityId requesterId)
{
	// If we are a client, only process use requests if we aren't still waiting on the server for a previous use request (e.g. for HMG weapon). 
	CActor* pActor = static_cast<CActor*>(gEnv->pGameFramework->GetIActorSystem()->GetActor(requesterId));
	if(!gEnv->bServer && pActor && !pActor->IsStillWaitingOnServerUseResponse())
	{
		pActor->SetStillWaitingOnServerUseResponse(true);
		GetGameObject()->InvokeRMI(SvRequestUseEnvironmentalWeapon(), EnvironmentalWeaponRequestParams(), eRMI_ToServer);
	}
	else
	{
		Use(requesterId);
	}
}

bool CEnvironmentalWeapon::DoDetachFromParent()
{
	bool wasAttached=false;

	IEntity* pWeaponEntity = GetEntity();
	if (IEntity *pParent = pWeaponEntity->GetParent())
	{
		// standard parent with assumed normal attachment - detaching
		pWeaponEntity->DetachThis(IEntity::ATTACHMENT_KEEP_TRANSFORMATION);
		wasAttached=true;
	}

	return wasAttached;
}

bool CEnvironmentalWeapon::Use(EntityId requesterId)
{
	IEntity* pWeaponEntity = GetEntity();
	if(pWeaponEntity)
	{
		IScriptTable* pScriptTable = pWeaponEntity->GetScriptTable();
		if(pScriptTable)
		{
			int val;
			pScriptTable->GetValue("bCurrentlyPickable", val);

			if(val == 1)
			{
				// No longer pickable as is held. 
				pScriptTable->SetValue("bCurrentlyPickable", 0);

				IActor* pActor = gEnv->pGameFramework->GetIActorSystem()->GetActor(requesterId);
				if(pActor)
				{
					SetOwner(requesterId);

					if (!DoDetachFromParent())
					{
						//see if we're linked to a vehicle
						if( m_pLinkedVehicle )
						{
							//send an event to detach
							SVehicleEventParams	vehicleEventParams;
							vehicleEventParams.entityId	= GetEntityId();
							vehicleEventParams.iParam		= m_pLinkedVehicle->GetEntityId();

							m_pLinkedVehicle->BroadcastVehicleEvent(eVE_OnDetachPartEntity, vehicleEventParams);
						}
					}

					CRY_ASSERT_MESSAGE(pActor->GetActorClass() == CPlayer::GetActorClassType(), "CEnvironmentalWeapon::Use - Expected player, got something else");
					static_cast<CPlayer*>(pActor)->EnterPickAndThrow( GetEntityId(), false );

					if(gEnv->bServer)
					{
						EnvironmentalWeaponParams params;
						params.requesterId = requesterId;
						GetGameObject()->InvokeRMIWithDependentObject(ClEnvironmentalWeaponUsed(), params, eRMI_ToRemoteClients, requesterId);
					}
					else if(pActor->IsClient())
					{
						pActor->SetStillWaitingOnServerUseResponse(false);
					}

					return true;
				}
			}
		}
	}

	return false;
}

void CEnvironmentalWeapon::SetOwner(EntityId ownerId)
{
	CPlayer* pPlayer(NULL);
	if(IActor* pClientActor = g_pGame->GetIGameFramework()->GetClientActor())
	{
		pPlayer = static_cast<CPlayer*>(pClientActor);
	}

	if(m_OwnerId != ownerId)
	{
		m_intersectionAssist.SetFocalEntityId(ownerId); 
		if(m_OwnerId && !ownerId)
		{
			m_State = EWS_OnGround;

			if(m_currentAttackState == EAttackStateType_EnactingPrimaryAttack)
			{
				OnFinishMelee(); 
			}

			m_intersectionAssist.BeginCheckingForObjectEmbed(); 
			
			IEntity* pEntity = GetEntity(); 
			IScriptTable* pScript = pEntity->GetScriptTable(); 
			if(pScript)
			{
				pScript->SetValue("bCurrentlyPickable", 1);
			}

			// Kick off scheduled kill timer, cannot be rooted now 
			SmartScriptTable propertiesTable;
			bool result = pScript ? pScript->GetValue("Properties", propertiesTable) : false;

			float idleKillTime = 0.0f; 
			if( result && propertiesTable->GetValue("idleKillTime",idleKillTime) && 
				idleKillTime >= 0.0f)
			{
				CGameRules *pGameRules=g_pGame->GetGameRules();
				assert(pGameRules);
				if (pGameRules)
				{
					pGameRules->ScheduleEntityRemoval(GetEntityId(), idleKillTime, true);
					m_logicFlags |= ELogicFlag_removalScheduled; 
				}
			}

			if( !m_throwerId )
			{
				//released + not thrown = dropped
				IActor* pOwnerActor = gEnv->pGameFramework->GetIActorSystem()->GetActor( m_OwnerId );
				CStatsRecordingMgr* pRecordingMgr = g_pGame->GetStatsRecorder();

				float health = GetCurrentHealth();
				
				if( IStatsTracker* pTracker = pRecordingMgr ? pRecordingMgr->GetStatsTracker( pOwnerActor ) : NULL )
				{
					pTracker->Event( eGSE_EnvironmentalWeaponEvent, new CEnvironmentalWeaponEvent( GetEntityId(), eEVE_Drop, (int16)health ) );
				}
			}
			
		}
		else if(ownerId)
		{
			m_State = EWS_Held;
			m_previousThrowerId = m_throwerId;
			m_throwerId = 0;

			if(g_pGame->GetIGameFramework()->GetClientActorId() == ownerId)
			{
				// we are now the owner
				m_intersectionAssist.SetClientHasAuthority(true);
			}
			else
			{
				m_intersectionAssist.SetClientHasAuthority(false);
			}

			m_intersectionAssist.AbortCheckingForObjectEmbed(); 

			// Disable kill timer
			// Kick off scheduled kill timer
			if(m_logicFlags & ELogicFlag_removalScheduled)
			{
				CGameRules *pGameRules=g_pGame->GetGameRules();
				assert(pGameRules);
				if (pGameRules)
				{
					pGameRules->AbortEntityRemoval(GetEntityId());
				}
			}

			//Unregister with interactive entity monitor
			if(pPlayer)
			{
				if(m_logicFlags & ELogicFlag_registeredWithIEMonitor)
				{
					m_logicFlags &= ~ELogicFlag_registeredWithIEMonitor;
				}
			}

			m_healthWhenGrabbed = GetCurrentHealth();

			// Delegate to whoever picked it up. Leave it with them until the next owner.
			DelegateAuthorityOnOwnershipChanged(m_OwnerId,ownerId);
		}

		m_OwnerId = ownerId;

		//Register with interactive entity monitor
		if(ownerId == 0 && pPlayer && !m_throwerId && (m_logicFlags & ELogicFlag_registeredWithIEMonitor) == 0)
		{
			AttemptToRegisterInteractiveEntity();
		}

		//Update physics netserialization (Only serialize when the shields aren't being held otherwise we get fighting from 1p/3p animation differences)
		{
			GetEntity()->PhysicsNetSerializeEnable(m_OwnerId == 0);
		}

		// Let lua know about this too
		IEntity* pEntity = GetEntity(); 
		EntityScripts::CallScriptFunction(pEntity, pEntity->GetScriptTable(), "SetOwnerID",ScriptHandle(m_OwnerId));
	}
}

void CEnvironmentalWeapon::Reset()
{
	m_lastPos = GetEntity()->GetWorldPos();

	IScriptTable* pScriptTable = GetEntity()->GetScriptTable();
	if(pScriptTable)
	{
		Script::CallMethod(pScriptTable, "OnReset");
	}
	m_logicFlags &= ~(ELogicFlag_removalScheduled | ELogicFlag_geomCollisionDetectedThisAttack | ELogicFlag_damageZoneHelperInitialised | ELogicFlag_damagePhaseActive | ELogicFlag_weaponSweepTestsValid); 
	m_entityHitList.clear(); 
	m_currentAttackState = EAttackStateType_None; 
	m_throwerId = 0;
	m_previousThrowerId = 0;
	m_pDamageZoneHelper = NULL; 


#ifndef _RELEASE
	m_debugCollisionPts_Geom.clear(); 
#endif //#ifndef _RELEASE
}

bool CEnvironmentalWeapon::NetSerialize( TSerialize ser, EEntityAspects aspect, uint8 profile, int flags )
{
	if(aspect == ASPECT_HEALTH_STATUS)
	{
		// Note - could always compress health e.g. 750 to a 0->100 (or 0.0 -> 1.0) value
		// before sending over network
		float fCurrentHealth, fPrevHealth;
		fCurrentHealth = fPrevHealth = GetCurrentHealth();
		CRY_ASSERT_MESSAGE( fCurrentHealth <= 8191, "ERROR - edmg policy max cannot store currentHealth ");
		ser.Value("currentHealth", fCurrentHealth, 'edmg' );

		if(ser.IsReading())
		{
			// Set our health to the new value
			if(fCurrentHealth != fPrevHealth)
			{
				GetEntity()->GetScriptTable()->SetValue(CEnvironmentalWeapon::kHealthScriptName, fCurrentHealth);

				if(m_OwnerId)
				{
					if( CPersistantStats* pStats = CPersistantStats::GetInstance() )
					{
						pStats->IncrementStatsForActor( m_OwnerId, EFPS_WallOfSteel,  fPrevHealth - fCurrentHealth );
					}
				}				

				// call the on changed method in lua
				if(m_OnClientHealthChangedFunc)
				{
					IScriptTable*  pTable					= GetEntity()->GetScriptTable();
					IScriptSystem*  pScriptSystem = gEnv->pScriptSystem;
					Script::Call(pScriptSystem, m_OnClientHealthChangedFunc, pTable);
				}
			}			
		}
	}

	return true;
}

void CEnvironmentalWeapon::Update(SEntityUpdateContext &ctx, int updateSlot)
{
	Vec3 pos = GetEntity()->GetWorldPos();
	if ((m_lastPos-pos).GetLengthSquared()>sqr(0.1f))
		m_lastPos = pos;

	// Do any late setup 
	if( m_logicFlags & ELogicFlag_doVehicleAttach )
	{
		DoVehicleAttach();
	}

	// This is pretty horrible checking every frame... :( post init isnt late enough as pick n throw weapon isnt around yet. 
	if( m_logicFlags & ELogicFlag_doInitWeaponState)
	{
		DoInitWeaponState(); 
	}

#ifndef _RELEASE
	UpdateDebugOutput(); 
#endif //#ifndef _RELEASE

	if(m_currentAttackState == EAttackStateType_EnactingPrimaryAttack)
	{
		TrackWeaponVelocity(ctx.fFrameTime);

		if((m_logicFlags & ELogicFlag_performSweepTests) && 
		   g_pGameCVars->pl_pickAndThrow.environmentalWeaponSweepTestsEnabled)
		{
			PerformWeaponSweepTracking(); 
		}
	}

	if(m_throwerId && ((m_logicFlags & ELogicFlag_registeredWithIEMonitor) == 0))
	{
		IPhysicalEntity *pPhysics = GetEntity()->GetPhysics();
		pe_status_dynamics status_dynamics;
		if(pPhysics->GetStatus(&status_dynamics) && status_dynamics.v.GetLengthSquared() < sqr(0.7f))
		{
			AttemptToRegisterInteractiveEntity(); 
		}
	}

	m_intersectionAssist.Update(ctx.fFrameTime);

	if(m_noRequiredKillVelocityTimer > 0.0f)
	{
		m_noRequiredKillVelocityTimer -= ctx.fFrameTime; 
	}
}

void CEnvironmentalWeapon::TrackWeaponVelocity(const float dt)
{
	if(m_pVelocityTracker)
	{
		// Store weapon tracker velocity scaled to represent M/S
		m_weaponTrackerVelocity = (GetEntity()->GetWorldTM() * m_pVelocityTracker->tm).GetTranslation() - m_prevWeaponTrackerPos;
		m_weaponTrackerVelocity = m_weaponTrackerVelocity.normalize();
		m_weaponTrackerVelocity *= __fres(dt); 
	}
#ifndef _RELEASE
	else
	{
		// Just make it obvious this weapon needs a helper setup, knock things straight up into the air
		m_weaponTrackerVelocity(0.0f,0.0f,5.0f); 
	}
#endif // #ifndef _RELEASE

	// Refresh for next
	UpdateCachedWeaponTrackerPos(); 
}

void CEnvironmentalWeapon::UpdateGlassBreakParams()
{
	if(m_logicFlags & ELogicFlag_damagePhaseActive)
	{
		// Set velocity so that glass receives a sensible collision event and breaks
		IPhysicalEntity* pPhysEnt = GetEntity()->GetPhysics();
		pe_action_set_velocity setVel;
		setVel.v = m_weaponTrackerVelocity;

		// We limit the speed as poles are *REALLY* heavy buggers
		float speedSq = setVel.v.GetLengthSquared();
		float maxSpeed = 45.0f;

		if (speedSq > maxSpeed*maxSpeed)
		{
			float speedScale = __fres(sqrt_tpl(speedSq)) * maxSpeed;
			setVel.v.x *= speedScale;
			setVel.v.y *= speedScale;
			setVel.v.z *= speedScale;
		}

		pPhysEnt->Action(&setVel);
	}
}

void CEnvironmentalWeapon::PerformGroundClearanceAdjust( const CActor* pOwner, const bool chargedThrown )
{
	assert(pOwner != NULL);
	assert(pOwner->IsClient());

	IEntity* pEntity = GetEntity(); 
	CIntersectionAssistanceUnit& intersectionAssist = GetIntersectionAssistUnit();
	if(intersectionAssist.Enabled())
	{
		if(chargedThrown)
		{
			// Throwing
			// - ALWAYS adjust upwards for floor clearance (no current object should be thrown below players feet) 
			// Gather Player + Ent bounding box data
			AABB wPlayerAABB;
			OBB wPlayerOBB; 
			EStance playerStance	= pOwner->GetStance();
			wPlayerAABB						= pOwner->GetStanceInfo(playerStance)->GetStanceBounds();
			wPlayerOBB.SetOBBfromAABB(Quat(IDENTITY), wPlayerAABB);
			wPlayerOBB.c					 += pOwner->GetEntity()->GetPos(); 

			AABB wEntABB;
			pEntity->GetLocalBounds(wEntABB);
			wEntABB.SetTransformedAABB(pEntity->GetWorldTM(),wEntABB);

			// FLOOR CLIPPING
			const float playerLowestZVal = wPlayerOBB.c.z - wPlayerOBB.h.z; 
			const float entLowestZVal    = wEntABB.GetCenter().z - wEntABB.GetRadius();

			float fFloorPenetrationAdjust = static_cast<float>(__fsel( entLowestZVal-playerLowestZVal, 0.f, playerLowestZVal - entLowestZVal ));

			// WATER PENETRATION (at least the 'global' water level - still may need work for individual water volumes) 
			const Vec3& entWPos = pEntity->GetPos(); 
			const float entWaterLevel = gEnv->p3DEngine->GetWaterLevel(&entWPos);
			if (entWaterLevel != WATER_LEVEL_UNKNOWN)
			{
				const float depth = entWaterLevel - entLowestZVal;
				if (depth >= 0.0f)
				{
					fFloorPenetrationAdjust = max(fFloorPenetrationAdjust, depth);
				}
			}

			// Always add a small amount of extra vertical clearance than should be needed to make extra sure (handles slopes etc better). 
			// Also add a small amount of clearance scaled on how sharp the player's downward look angle is.
			const CCamera& cam																		= gEnv->p3DEngine->GetRenderingCamera();
			const float lookZ																			= cam.GetViewdir().z; 
			float additionalThrowDirClearanceAdjust								= 0.0f; 
			if(lookZ < 0.0f) // [0.0f, -1.0f]
			{
				// nothing special, just a basic linear munge for now
				static float sMaxAngleScaledFloorClearanceAdjust = 1.0f;  
				additionalThrowDirClearanceAdjust  = -lookZ * sMaxAngleScaledFloorClearanceAdjust;
			}

			Matrix34 wMat = pEntity->GetWorldTM(); 
			static float sFloorClearanceMandatoryAdjust						= 0.2f; 
			wMat.AddTranslation(Vec3(0.0f,0.0f,fFloorPenetrationAdjust + additionalThrowDirClearanceAdjust + sFloorClearanceMandatoryAdjust));
			pEntity->SetWorldTM(wMat); 

		}
		else
		{
			// when dropping, test if this object clips, if so, move to safe position 
			QuatT safePos;

			// if current not obstructed, will be returned as the safe pos
			const bool bRequiresAdjust = intersectionAssist.GetSafeLocationForSubjectEntity(safePos);
			if(bRequiresAdjust)
			{
				Matrix34 adjustedwMat(safePos);
				pEntity->SetWorldTM(adjustedwMat);
			}
		}
	}
}

void CEnvironmentalWeapon::PerformRearClearanceAdjust( const CActor* pOwner, const bool chargedThrown )
{
	assert(pOwner != NULL);
	assert(pOwner->IsClient());

	IEntity* pEntity = GetEntity(); 
	if(chargedThrown)
	{
		IEntity* pPlayerEntity	= pOwner->GetEntity(); 

		// Generate an AABB for the entity in local player space ( so aabb y value matches player's 'forward' direction). 
		AABB entABB;
		const Matrix34& entWMat			= pEntity->GetWorldTM(); 
		const Matrix34& playerWMat	= pPlayerEntity->GetWorldTM(); 
		pEntity->GetLocalBounds(entABB);
		entABB.SetTransformedAABB(entWMat,entABB); // Wspace
		entABB.SetTransformedAABB(playerWMat.GetInverted(),entABB); 

		// REAR CLIPPING
		const float lowestYVal = entABB.GetCenter().y - (entABB.GetSize().y * 0.5f); 

		if(lowestYVal < 0.0f) // then behind player!
		{
			// get player entity local forward axis(Y) in wspace units
			Vec3 vAdjustVec = playerWMat.GetColumn1().GetNormalized(); // FORWARD
			vAdjustVec.scale(0.0f - lowestYVal); // scaled for correct adjust
			Matrix34 adjustedEntityMat = entWMat; 
			adjustedEntityMat.AddTranslation(vAdjustVec);
			pEntity->SetWorldTM(adjustedEntityMat); 
		}
	}
}

void CEnvironmentalWeapon::DoInitWeaponState()
{
	// Using owner ID and weapon state, we can determine + correctly setup all our state properties.
	// If we are in the initial state.. we haven't been interacted with yet.. We are either rooted, or on the floor
	if(m_State != EWS_Initial)
	{
		// Validity
		// either 1) We have an owner and are in the held state, or 2) we don't have an owner and are in the dropped state
		CRY_ASSERT_MESSAGE((m_OwnerId > 0 && m_State == EWS_Held) || (m_OwnerId == 0 && m_State == EWS_OnGround), "CEnvironmentalWeapon::PostInit() < ERROR - properties in invalid state");

		if(m_OwnerId)
		{
			InitWeaponState_Held();
		}
		else
		{
			InitWeaponState_OnGround(); 
		}
	}
	else 
	{
		// Env weapons can start off in rooted OR unrooted state
		bool bInitiallyRooted = true; 
		if(EntityScripts::GetEntityProperty(GetEntity(),"bInitiallyRooted", bInitiallyRooted) && !bInitiallyRooted)
		{
			InitWeaponState_OnGround(); 
			m_State = EWS_OnGround; 
		}
	}

	m_logicFlags &= ~ELogicFlag_doInitWeaponState; 

	if(g_pGame->GetIGameFramework()->GetClientActorId() == m_OwnerId)
	{
		// we are now the owner
		m_intersectionAssist.SetClientHasAuthority(true);
	}
	else
	{
		m_intersectionAssist.SetClientHasAuthority(false);
	}
}

void CEnvironmentalWeapon::DoVehicleAttach()
{
	// If the weapon is not rooted, it has already been detached and we shouldn't attach it
	bool rooted = false;
	GetEntity()->GetScriptTable()->GetValue( "IsRooted", rooted );
	if( rooted )
	{
		//get the Vehicle and ask it to link us to the part
		if( m_parentVehicleId  != 0 )
		{
			if( IVehicleSystem* pVSystem = gEnv->pGameFramework->GetIVehicleSystem() )
			{
				if( IVehicle* pVehicle = pVSystem->GetVehicle( m_parentVehicleId ) )
				{
					m_pLinkedVehicle = pVehicle;

					INetContext* pNetContext = gEnv->pGameFramework->GetNetContext();
					if (pNetContext)
					{
						//send an event to attach
						SVehicleEventParams	vehicleEventParams;
						vehicleEventParams.entityId	= GetEntityId();

						SNetObjectID netId = pNetContext->GetNetID( vehicleEventParams.entityId, false);
						vehicleEventParams.iParam = netId.id;

						pVehicle->BroadcastVehicleEvent(eVE_TryAttachPartEntityNetID, vehicleEventParams);

					}
				}
			}
		}
	}

	m_logicFlags &= ~ELogicFlag_doVehicleAttach;
}

bool CEnvironmentalWeapon::ProcessMeleeHit(const EntityHitRecord& hitRecord, IPhysicalEntity* pVictimPhysicalEntity) 
{
	IEntity* pEntity = GetEntity(); 

	// Determine what we have hit and react accordingly. 
	IActor* pActor = g_pGame->GetIGameFramework()->GetIActorSystem()->GetActor(hitRecord.m_entityId);
	if(pActor && pActor->IsPlayer())
	{	
		ProcessHitPlayer(pActor, hitRecord, m_weaponTrackerVelocity.GetNormalized(), m_OwnerId);

		// Hit processed, and this entity should not be hit again during this attack
		return true; 
	}
	else
	{
		// Abort if we are hitting a ragdoll. don't want to run foul of german law.
		bool bIsRagdoll = (pVictimPhysicalEntity != NULL ? pVictimPhysicalEntity->GetType() == PE_ARTICULATED : false);
		if(bIsRagdoll) //Check to make sure this is a corpse and not something else
		{
			static const IEntityClass* pCorpseClass = gEnv->pEntitySystem->GetClassRegistry()->FindClass("Corpse");
			const IEntity* pVictimEntity = gEnv->pEntitySystem->GetEntityFromPhysics(pVictimPhysicalEntity);
			if(pVictimEntity && pVictimEntity->GetClass() != pCorpseClass)
			{
				bIsRagdoll = false;
			}
		}

		if(!bIsRagdoll)
		{
			ProcessHitObject(hitRecord, m_weaponTrackerVelocity, pVictimPhysicalEntity, m_OwnerId);
			return true; 
		}
		else
		{
			return false; 
		}
	}	
}

#ifndef _RELEASE
void CEnvironmentalWeapon::UpdateDebugOutput() const
{
	IRenderAuxGeom* pAuxGeom = gEnv->pRenderer->GetIRenderAuxGeom();
	const ColorB red(255,0,0);
	const ColorB blue(0,0,255);
	const ColorB green(0,255,0);

	if(m_OwnerId > 0)
	{
		if(g_pGameCVars->pl_pickAndThrow.environmentalWeaponHealthDebugEnabled )
		{
			IActor* pActor = gEnv->pGameFramework->GetIActorSystem()->GetActor(m_OwnerId);
			if(pActor)
			{
				// If this weapon is being held...
				const CPlayer* pPlayer =  static_cast<CPlayer*>(pActor);
				if(pPlayer->IsClient())
				{
					float currentHealth = GetCurrentHealth();
	
					float initialHealth = 99999.0f; 
					EntityScripts::GetEntityProperty(GetEntity(),"initialHealth", initialHealth);
					CryWatch("CurrentHealthVals [ %.1f | %.1f ] ", currentHealth, initialHealth);
					CryWatch("CurrentHealthPercentage [ %.1f Percent ]", initialHealth > 0.0f ? (currentHealth / initialHealth * 100.0f) : 9999.0f);

					bool isDamageable = false; 
					EntityScripts::GetEntityProperty(GetEntity(),"damageable", isDamageable);
					CryWatch("Damageable : %s", isDamageable ? "TRUE" : "FALSE" );
				}
			}
		}

		// Debug visuals
		if(g_pGameCVars->pl_pickAndThrow.environmentalWeaponImpactDebugEnabled)
		{
			const char* attackStateName = "None";
			if(m_currentAttackState & EAttackStateType_EnactingPrimaryAttack)
			{
				attackStateName = "Primary Attack";
			}
			else if(m_currentAttackState & EAttackStateType_ChargedThrow)
			{
				attackStateName = "Charged Throw";
			}
			
			CryWatch("Current Attack State : [ %s ]", attackStateName);

			// Anim event debugging
			CryWatch("Animation Damage Phase : [ %s ]", (m_logicFlags & ELogicFlag_damagePhaseActive) ? "TRUE": "FALSE");

			// Render green if during current animation damage phase
			// Render red if not during current animation damage phase

			if(m_pDamageZoneHelper)
			{
				SAuxGeomRenderFlags originalFlags = pAuxGeom->GetRenderFlags();
				SAuxGeomRenderFlags newFlags	  = originalFlags; 
				newFlags.SetCullMode(e_CullModeNone); 
				newFlags.SetDepthTestFlag(e_DepthTestOff);

				const Matrix34& damageZoneHelperMat = GetEntity()->GetWorldTM() * m_pDamageZoneHelper->tm;
				ColorB drawCol	 = (m_logicFlags & ELogicFlag_damagePhaseActive) ? green : red; 
				const Vec3& wPos = damageZoneHelperMat.GetTranslation(); 
				const Vec3& vDir = damageZoneHelperMat.GetColumn1(); 

				pAuxGeom->SetRenderFlags(newFlags);
		
				// Mark helper
				pAuxGeom->DrawPoint(wPos,drawCol,20);

				// Draw facing dir
				const float facingLineLength = 1.0f; 
				Vec3 facingVec = vDir.GetNormalized() * facingLineLength; 
				pAuxGeom->DrawLine(wPos, drawCol, wPos + facingVec, drawCol, 1.0f);
				pAuxGeom->DrawSphere(wPos,0.1f,drawCol,false);
			

				if(!m_debugCollisionPts_Geom.empty())
				{
					const float fFontSize = 1.2f;
					float txtDrawColor[4] = {1.0f, 1.0f, 1.0f, 1.0f};

					std::vector<debugCollisionPt>::const_iterator endIter  = m_debugCollisionPts_Geom.end(); 
					for(std::vector<debugCollisionPt>::const_iterator iter = m_debugCollisionPts_Geom.begin(); iter != endIter; ++iter)
					{
						// Render collision pt
						pAuxGeom->DrawPoint(iter->m_wCollisionPos,iter->m_collisionCol,10);

						// Render number at pt
						const int count = static_cast<int>(iter - m_debugCollisionPts_Geom.begin()); 
						IRenderAuxText::DrawLabelExF(iter->m_wCollisionPos, fFontSize, txtDrawColor, true, true, "%d",count);

						if(iter->m_showDamageZoneHelper)
						{
							// Render Damage zone helper as it was at this point
							const Vec3 dzhPos = iter->m_damageHelperPos; 
							pAuxGeom->DrawSphere(dzhPos,0.01f,iter->m_collisionCol,false);

							// Draw facing dir for DZH as it was at this pt
							const float currentFacingLineLength = 0.25f; 
							pAuxGeom->DrawLine(dzhPos, drawCol, dzhPos + (iter->m_damageHelperDir*currentFacingLineLength), drawCol, 1.0f);
					
							// Render number at damage zone helper
							IRenderAuxText::DrawLabelExF(dzhPos, fFontSize, txtDrawColor, true, true, "%d",count);
						}
					}
				}

#if ALLOW_SWEEP_DEBUGGING
				// Render lines
				RenderDebugWeaponSweep(); 

				// Render Entity detection box (used by sweep)
				if(s_bRenderWeaponAABB)
				{
					IEntity* pEnt				  = GetEntity(); 
					if(pEnt)
					{
						Vec3 weapWPos = pEnt->GetWorldPos();
						AABB weaponAABB;
						weaponAABB.min = weapWPos - s_radiusAdjust;
						weaponAABB.max = weapWPos + s_radiusAdjust; 
						pAuxGeom->DrawAABB(weaponAABB,false,ColorB(255,255,255,255),eBBD_Faceted);
					}
				}
#endif //#if ALLOW_SWEEP_DEBUGGING

				pAuxGeom->SetRenderFlags(originalFlags);
			}
		}
	}

	if(g_pGameCVars->pl_pickAndThrow.chargedThrowDebugOutputEnabled)
	{
		// Render throw helper
		pAuxGeom->DrawPoint(m_throwDir,green,20);
		const float throwDebugLineLength = 1.0f; 
		pAuxGeom->DrawLine(m_throwOrigin, blue, m_throwTarget, red, 1.0f);
	}

	// STAT DUMP
	if(g_pGameCVars->pl_pickAndThrow.environmentalWeaponRenderStatsEnabled)
	{
		RenderDebugStats(); 
	}
}
#endif //#ifndef _RELEASE


#ifndef _RELEASE
void CEnvironmentalWeapon::RenderDebugStats() const
{
	const float fFontSize = 1.2f;
	float drawColor[4] = {1.0f, 1.0f, 1.0f, 1.0f};

	string sMsg(string().Format(" Entity ID: [%d]", GetEntityId()));
	sMsg += string().Format("\n OwnerID: [%d]", m_OwnerId );

	switch(m_State)
	{
	case EWS_Initial:
		{
			sMsg += string().Format("\n WeaponState: [%s]", "EWS_Initial" );
		}
		break;
	case EWS_Held:
		{
			sMsg += string().Format("\n WeaponState: [%s]", "EWS_Held" );
		}
		break;
	case EWS_OnGround:
		{
			sMsg += string().Format("\n WeaponState: [%s]", "EWS_OnGround" );
		}
		break;
	default : 
		{
			sMsg += string().Format("\n WeaponState: [%s]", "UNKNOWN" );
		}
		break;
	}

	sMsg += string().Format("\n Archetype: [%s]", m_archetypename.empty()? "NONE": m_archetypename.c_str() );

	SmartScriptTable propertiesTable;
	IScriptTable* pScriptTable = GetEntity()->GetScriptTable();
	if(pScriptTable)
	{
		bool bResult = false; 
		if(pScriptTable->GetValue("bCurrentlyPickable", bResult))
		{
			sMsg += string().Format("\n LUA - bCurrentlyPickable : [%s]", bResult ? "TRUE" : "FALSE");
		}

		if(pScriptTable->GetValue("IsRooted", bResult))
		{
			sMsg += string().Format("\n LUA - IsRooted : [%s]", bResult ? "TRUE" : "FALSE");
		}

		EntityId ownerId = 12345; 
		if(pScriptTable->GetValue("ownerID", ownerId))
		{
			sMsg += string().Format("\n LUA - ownerID : [%d]",ownerId);
		}


		float initialHealth = 99999.0f; 
		float currentHealth = GetCurrentHealth(); 
		EntityScripts::GetEntityProperty(GetEntity(),"initialHealth", initialHealth);

		sMsg += string().Format("\n Health [%.2f|%.2f] - %.2f percent", currentHealth, initialHealth,  initialHealth > 0.0f ? (currentHealth / initialHealth * 100.0f) : 9999.0f);

		bool isDamageable = false; 
		EntityScripts::GetEntityProperty(GetEntity(),"damageable", isDamageable);

		sMsg += string().Format("\n LUA - damageable : [%s]",isDamageable ? "TRUE" : "FALSE" );

		const char* pGrabTagOverride = NULL; 
		if(EntityScripts::GetEntityProperty(GetEntity(),"szGrabAnimTagOverride", pGrabTagOverride) && pGrabTagOverride[0])
		{
			sMsg += string().Format("\n LUA - szGrabAnimTagOverride : [%s]",pGrabTagOverride );
		}
		else
		{
			sMsg += string().Format("\n LUA - szGrabAnimTagOverride : [%s]", "NONE" );
		}

		if(EntityScripts::GetEntityProperty(GetEntity(),"szRootedGrabAnimTagOverride", pGrabTagOverride) && pGrabTagOverride[0])
		{
			sMsg += string().Format("\n LUA - szRootedGrabAnimTagOverride : [%s]",pGrabTagOverride );
		}
		else
		{
			sMsg += string().Format("\n LUA - szRootedGrabAnimTagOverride : [%s]", "NONE" );
		}
	}
	Vec3 vDrawPos = GetEntity()->GetWorldPos() + Vec3(0.0f,0.0f,0.6f);
	IRenderAuxText::DrawLabelEx(vDrawPos, fFontSize, drawColor, true, true, sMsg.c_str());
}
#endif // #ifndef _RELEASE

uint64 CEnvironmentalWeapon::GetEventMask() const
{
	return ENTITY_EVENT_BIT(ENTITY_EVENT_LEVEL_LOADED) | ENTITY_EVENT_BIT(ENTITY_EVENT_RESET) | ENTITY_EVENT_BIT(ENTITY_EVENT_LINK) | ENTITY_EVENT_BIT(ENTITY_EVENT_DELINK) | ENTITY_EVENT_BIT(ENTITY_EVENT_START_LEVEL) | ENTITY_EVENT_BIT(ENTITY_EVENT_XFORM);
}

void CEnvironmentalWeapon::ProcessEvent(const SEntityEvent& event)
{
	switch(event.event)
	{
	case ENTITY_EVENT_LEVEL_LOADED: 
	case ENTITY_EVENT_RESET:
		{
			Reset();
		}
		break;
	case ENTITY_EVENT_LINK:
		{

			if( IEntity* pWeaponEntity = GetEntity() )
			{
				IEntityLink* pLink = pWeaponEntity->GetEntityLinks();

				while( pLink && stricmp( pLink->name, "VehiclePartLink") )
				{
					pLink = pLink->next;
				}

				if( pLink )
				{
					//we're attached to a vehicle, get it
					if( IVehicleSystem* pVSystem = gEnv->pGameFramework->GetIVehicleSystem() )
					{
						if( IVehicle* pVehicle = pVSystem->GetVehicle( pLink->entityId ) )
						{
							m_logicFlags |= ELogicFlag_hasParentVehicle;
							m_pLinkedVehicle = pVehicle;
							m_parentVehicleId = pVehicle->GetEntityId();
							
							GetGameObject()->SetNetworkParent( pVehicle->GetEntityId() );
						}
					}
				}
			}
		}
		break;
	case ENTITY_EVENT_DELINK:
		{
			if( m_OwnerId == 0 )
			{
				//add a little push away from the car
				Vec3 pos = GetEntity()->GetWorldPos();
				Vec3 direction = pos - m_pLinkedVehicle->GetEntity()->GetWorldPos();
				direction.z = 0.1f;
				direction.normalize();

				//offset from car to remove any chance of initial inter-mesh penetration
				GetEntity()->SetPos( pos + direction * 0.1f );

				IPhysicalEntity *pPhysics = GetEntity()->GetPhysics();

				pe_action_awake action_awake;
				action_awake.bAwake = true;
				action_awake.minAwakeTime = 1.f;
				pPhysics->Action(&action_awake);
					

				//get current velocity etc
				pe_status_dynamics dynamics;
				pPhysics->GetStatus( &dynamics );

				UnRoot();

				//randomize launch dir
				direction.x += g_pGame->GetRandomFloat()*0.4f - 0.2f;
				direction.y += g_pGame->GetRandomFloat()*0.4f - 0.2f;
				
				//arbitrary amount of angular velocity for a pleasing effect
				Vec3 angular( (g_pGame->GetRandomFloat()*1.5f-2.0f)*direction.y, (g_pGame->GetRandomFloat()*1.5f-2.0f)*direction.x, 0.0f );


				pe_action_set_velocity velocity;
				velocity.v  = dynamics.v + direction * cry_random( 1.0f, 3.0f );
				velocity.w = dynamics.w + angular;
				velocity.bRotationAroundPivot = true;
					
				pPhysics->Action( &velocity );
			}

			m_logicFlags &= ~ELogicFlag_hasParentVehicle;
			m_pLinkedVehicle = NULL;
			m_parentVehicleId = 0;
		}
		break;
	case ENTITY_EVENT_START_LEVEL:
		{
			//Register with interactive entity monitor
			IActor* pClientActor(NULL);
			if( (m_logicFlags & ELogicFlag_registeredWithIEMonitor) == 0)
			{
				AttemptToRegisterInteractiveEntity(); 
			}
		}
		break;
	case ENTITY_EVENT_XFORM:
		{
			UpdateGlassBreakParams();
		}
		break;
	}
}

void CEnvironmentalWeapon::GetMemoryUsage( ICrySizer *pSizer ) const
{
	pSizer->Add(*this);
}

bool CEnvironmentalWeapon::ReloadExtension( IGameObject * pGameObject, const SEntitySpawnParams &params )
{
	ResetGameObject();
	EW::RegisterEvents(*this,*pGameObject);

	CRY_ASSERT_MESSAGE(false, "CEnvironmentalWeapon::ReloadExtension not implemented");

	return true;
}

bool CEnvironmentalWeapon::GetEntityPoolSignature( TSerialize signature )
{
	CRY_ASSERT_MESSAGE(false, "CEnvironmentalWeapon::GetEntityPoolSignature not implemented");

	return true;
}

void CEnvironmentalWeapon::InitClient(int channelId)
{
	if(m_State == EWS_OnGround)
	{
		//Wake up physics for a second to force rigid body synch
		pe_action_awake action_awake;
		action_awake.bAwake = true;
		action_awake.minAwakeTime = 1.f;
		GetEntity()->GetPhysics()->Action(&action_awake);
	}
}

IMPLEMENT_RMI(CEnvironmentalWeapon, SvRequestUseEnvironmentalWeapon)
{
	CGameRules* pGameRules = g_pGame->GetGameRules();
	IActor* pActor = pGameRules ? pGameRules->GetActorByChannelId(g_pGame->GetIGameFramework()->GetGameChannelId(pNetChannel)) : NULL;
	if(pActor)
	{
		if(!Use(pActor->GetEntityId()))
		{
			//Failed use - let the requesting actor know
			pActor->GetGameObject()->InvokeRMI(CActor::ClUseRequestProcessed(), CActor::NoParams(), eRMI_ToClientChannel, pActor->GetChannelId());
		}
	}

	return true;
}

IMPLEMENT_RMI(CEnvironmentalWeapon, ClEnvironmentalWeaponUsed)
{
	if(!Use(params.requesterId))
	{
		//Even if we couldn't actually use it, the client has still received the server response
		IActor* pActor = g_pGame->GetIGameFramework()->GetIActorSystem()->GetActor(params.requesterId);
		if(pActor && pActor->IsClient())
		{
			pActor->SetStillWaitingOnServerUseResponse(false);
		}
	}

	return true;
}

//------------------------------------------------------------------------
namespace CEnvironmentalWeaponGetSpawnInfo
{
	struct SInfo : public ISerializableInfo
	{	
		string m_archetypename;
		EntityId m_parentVehicleId; 
		bool m_hasParentVehicle;
		bool m_isHidden;
		
		SInfo()
		{
			m_parentVehicleId=0;
			m_hasParentVehicle=false;
			m_isHidden=false;
		}

		void SerializeWith( TSerialize ser )
		{
			ser.Value("archetypename", m_archetypename );
			ser.Value("parentVehicleId", m_parentVehicleId, 'eid');
			ser.Value("hasParentVehicle", m_hasParentVehicle, 'bool' );
			ser.Value("isHidden", m_isHidden, 'bool');
		}
	};
}

//------------------------------------------------------------------------
ISerializableInfoPtr CEnvironmentalWeapon::GetSpawnInfo()
{
	CEnvironmentalWeaponGetSpawnInfo::SInfo * pInfo = new CEnvironmentalWeaponGetSpawnInfo::SInfo;
	
	pInfo->m_hasParentVehicle = (m_logicFlags & ELogicFlag_hasParentVehicle? true : false);
	pInfo->m_parentVehicleId = m_parentVehicleId;
	
	if( IEntityArchetype* pArchetype = GetEntity()->GetArchetype() )
	{
		pInfo->m_archetypename = pArchetype->GetName();
	}
	else if(m_archetypename.length() > 1)
	{
		pInfo->m_archetypename = m_archetypename;
	}
	else
	{
		pInfo->m_archetypename = "";
	}

	if (GetEntity()->IsHidden())
	{
		pInfo->m_isHidden = true;
	}

	return pInfo;
}

//------------------------------------------------------------------------
void CEnvironmentalWeapon::SerializeSpawnInfo( TSerialize ser )
{
	CRY_ASSERT(ser.IsReading());

	ser.Value("archetypename", m_archetypename );
	ser.Value("parentVehicleId", m_parentVehicleId, 'eid');

	bool bHasParentVehicle = false; 
	ser.Value("hasParentVehicle", bHasParentVehicle, 'bool' );
	if(bHasParentVehicle)
	{
		m_logicFlags |= ELogicFlag_hasParentVehicle;
	}

	bool bIsHidden = false;
	ser.Value("isHidden", bIsHidden, 'bool');

	if (bIsHidden)
	{
		m_logicFlags |= ELogicFlag_spawnHidden;
	}
}

float CEnvironmentalWeapon::GetCurrentHealth() const
{
	float health = 0.0f;
	GetEntity()->GetScriptTable()->GetValue(CEnvironmentalWeapon::kHealthScriptName, health);
	return health; 
}

void CEnvironmentalWeapon::PostInit( IGameObject *pGameObject )
{
	EW::RegisterEvents(*this,*pGameObject);

	pGameObject->EnableUpdateSlot(this,0);

	// Cache our on health changed function
	IScriptTable*  pTable = GetEntity()->GetScriptTable();
	if (pTable != NULL)
	{
		pTable->GetValue("OnClientHealthChanged", m_OnClientHealthChangedFunc);

		SmartScriptTable properties;
		if (pTable->GetValue("Properties", properties))
		{
			// Cache the impact speed we apply to ragdolls
			properties->GetValue("RagdollPostMeleeImpactSpeed", m_ragdollPostMeleeImpactSpeed);
			properties->GetValue("RagdollPostThrowImpactSpeed", m_ragdollPostThrowImpactSpeed);

			// Cache our material fx data
			const char* pMFXLib; 
			if(properties->GetValue("szMFXLibrary", pMFXLib))
			{
				m_mfxLibraryName.Format(pMFXLib); 
			}

			m_rootedAngleDotMin = -1.f;
			m_rootedAngleDotMax = 1.f;
			if(properties->GetValue("RootedAngleMax", m_rootedAngleDotMin)) // Max in lua (angle) == min in cos angle (0=1, 1=180)
			{
				// Specified in degrees for half angle, from the front. 0-180
				m_rootedAngleDotMin = (float)__fsel(m_rootedAngleDotMin, cos_tpl(DEG2RAD(clamp_tpl<float>(m_rootedAngleDotMin,0.f,180.f))), -1.f);
			}
			if(properties->GetValue("RootedAngleMin", m_rootedAngleDotMax))
			{
				// Specified in degrees of half cone from the front. 0-180
				m_rootedAngleDotMax = (float)__fsel(m_rootedAngleDotMax, cos_tpl(DEG2RAD(clamp_tpl<float>(m_rootedAngleDotMax,0.f,180.f))), 1.f);
			}

			// Hit indicator
			const char* pClassificationName = NULL;
			properties->GetValue( "classification", pClassificationName );
			if(strcmpi(pClassificationName, "panel") == 0)
			{
				m_bShowShieldedHitIndicator = true;
			}
		}

		SmartScriptTable physParams;
		if (pTable->GetValue("physParams", physParams))
		{
			physParams->GetValue("mass", m_mass);
		}
	}

	//record weapon exists in stats log
	if( CStatsRecordingMgr* sr = g_pGame->GetStatsRecorder() )
	{
		HSCRIPTFUNCTION getClassification;
		int classification = 0;
		if (pTable != NULL)
		{
			pTable->GetValue("GetClassification", getClassification );
			Script::CallReturn( gEnv->pScriptSystem, getClassification, pTable, classification );
		}

		sr->EnvironmentalWeaponEntry( pGameObject->GetEntityId(), classification );
	}
}


void CEnvironmentalWeapon::SvOnHealthChanged(float fPrevHealth, float fCurrentHealth)
{
	CHANGED_NETWORK_STATE(this, ASPECT_HEALTH_STATUS);
	
	if( CPersistantStats* pStats = CPersistantStats::GetInstance() )
	{
		pStats->IncrementStatsForActor( m_OwnerId, EFPS_WallOfSteel,  fPrevHealth - fCurrentHealth );
	}
}

void CEnvironmentalWeapon::OnStartMelee(const Vec3& hitDir, const float hitDirMinRatio, const float hitDirMaxRatio, const bool bPerformSweepTests) 
{
	m_logicFlags |= ELogicFlag_processDamageAnimEvents;

	// Allowed to hit X new entities
	m_entityHitList.clear(); 
		
	// All clients (local or not) run the actual attack anims and v basic logic etc
	m_currentAttackState = EAttackStateType_EnactingPrimaryAttack; 

	// Allow fresh geom collision
	m_logicFlags &= ~ELogicFlag_geomCollisionDetectedThisAttack; 

	// Prepare for calcing velocity
	m_weaponTrackerVelocity = ZERO; 

	UpdateCachedWeaponTrackerPos();

	if(hitDirMaxRatio < 0.01f)
	{
		m_logicFlags &= ~ELogicFlag_hasHitOverride;			
	}
	else
	{
		m_logicFlags |= ELogicFlag_hasHitOverride;
		m_overrideHitDir = hitDir;
		m_overrideHitDirMinRatio = hitDirMinRatio;
		m_overrideHitDirMaxRatio = hitDirMaxRatio;
	}

	if(g_pGame->GetIGameFramework()->GetClientActorId() == m_OwnerId && bPerformSweepTests)
	{
		m_logicFlags |= ELogicFlag_performSweepTests; 
	}
	else
	{
		m_logicFlags &= ~ELogicFlag_performSweepTests; 
	}

#if ALLOW_SWEEP_DEBUGGING
	m_weaponSwingTrackedPoints.clear(); 
#endif //#if ALLOW_SWEEP_DEBUGGING

#ifndef _RELEASE
	m_debugCollisionPts_Geom.clear();
#endif
}

void CEnvironmentalWeapon::UpdateCachedWeaponTrackerPos()
{
	if(!m_pVelocityTracker)
	{
		// Make sure we actually have a pointer to the tracker
		RefreshObjectVelocityTracker(); 
	}

	if(m_pVelocityTracker)
	{
		// If we have one, we need to update the current location
		m_prevWeaponTrackerPos = (GetEntity()->GetWorldTM() * m_pVelocityTracker->tm).GetTranslation();
	}
}

void CEnvironmentalWeapon::OnFinishMelee() 
{
	if(g_pGame->GetIGameFramework()->GetClientActorId() == m_OwnerId)
	{
		GetGameObject()->EnablePhysicsEvent(false, eEPE_OnCollisionLogged);
	}

	m_currentAttackState = EAttackStateType_None; 

	m_logicFlags &= ~(ELogicFlag_processDamageAnimEvents | ELogicFlag_damagePhaseActive | ELogicFlag_weaponSweepTestsValid);
}

void CEnvironmentalWeapon::OnFinishChargedThrow() 
{
	if(g_pGame->GetIGameFramework()->GetClientActorId() == m_OwnerId)
	{
		GetGameObject()->EnablePhysicsEvent(false, eEPE_OnCollisionLogged);
	}

	m_currentAttackState = EAttackStateType_None; 
	CleanUpThrowerPhysicsConstraints();
}

void CEnvironmentalWeapon::CleanUpThrowerPhysicsConstraints()
{
	// Re-enable collisions with our thrower again if necessary
	IEntity* pEntity = GetEntity(); 
	if(m_livingConstraintID || m_articulatedConstraintID)
	{
		pe_action_update_constraint up;

		if(m_livingConstraintID)
		{
			IPhysicalEntity *pLivingPE = pEntity->GetPhysics();
			if (pLivingPE)
			{
				up.bRemove = true;
				up.idConstraint = m_livingConstraintID;
				m_livingConstraintID  = 0;
				pLivingPE->Action(&up);
			}
		}
		
		if(m_articulatedConstraintID)
		{
			IPhysicalEntity *pArticPE = pEntity->GetPhysics();
			if (pArticPE)
			{
				up.bRemove = true;
				up.idConstraint = m_articulatedConstraintID;
				m_articulatedConstraintID  = 0;
				pArticPE->Action(&up);
			}
		}

	}
}

bool AllowedToDamagePlayer(const EntityId attackerId, const EntityId victimEntityId)
{
	CGameRules *pGameRules = g_pGame->GetGameRules();
	if(pGameRules && pGameRules->IsTeamGame())
	{
		int clientTeamId = pGameRules->GetTeam(attackerId);
		int victimTeamId = pGameRules->GetTeam(victimEntityId);
		if( clientTeamId != victimTeamId ||
			  g_pGameCVars->g_friendlyfireratio > 0.0f)
		{
			return true; 
		}	
		else
		{
			return false; 
		}
	}
	
	return true; 
}

void CEnvironmentalWeapon::HandleEvent( const SGameObjectEvent& evnt )
{
	IGameFramework* pGameFramework = g_pGame->GetIGameFramework(); 

	if(evnt.event == eGFE_OnCollision && m_entityHitList.size() < s_MaxEntitiesAllowedHitsPerSwing)
	{
		const EventPhysCollision	*pEventPhysCollision = static_cast<const EventPhysCollision *>(evnt.ptr);
		if(m_currentAttackState & EAttackStateType_EnactingPrimaryAttack)
		{
			// The client doing the melee, or the client that just threw the object, is the one responsible for reporting kills/impacts
			if(pGameFramework->GetClientActorId() == m_OwnerId)
			{
				HandleMeleeCollisionEvent(pEventPhysCollision, (m_logicFlags&ELogicFlag_performSweepTests)==0);
			}
		}
		else if(m_currentAttackState & EAttackStateType_ChargedThrow)
		{
			if(pGameFramework->GetClientActorId() == m_throwerId)
			{
				if(m_bCanInstantKill)
				{
					HandleChargedThrowCollisionEvent(pEventPhysCollision);
				}
			}
			else
			{
				// remote players don't process the hit, we just re-enable collisions with thrower etc
				// as soon as *any* collision is detected with anything else
				CleanUpThrowerPhysicsConstraints(); 
			}
		}
	}
}


void CEnvironmentalWeapon::HandleMeleeCollisionEvent( const EventPhysCollision	*pEventPhysCollision, const bool bAllowActors )
{
	IGameFramework* pGameFramework = g_pGame->GetIGameFramework(); 
	IActor* pOwnerActor					   = pGameFramework->GetIActorSystem()->GetActor(m_OwnerId);
	if(pOwnerActor && pOwnerActor->IsPlayer())
	{
		CPlayer* pOwnerPlayer = static_cast<CPlayer*>(pOwnerActor);
		IPhysicalEntity *pPhysics = GetEntity()->GetPhysics();
		if(pPhysics)
		{
			// Work out which of the physical entities is the victim. 
			int victimIdx = 1; 
			IPhysicalEntity* pVictimPhysicalEntity = pEventPhysCollision->pEntity[1];
			if (pVictimPhysicalEntity == pPhysics)
			{
				pVictimPhysicalEntity = pEventPhysCollision->pEntity[0];
				victimIdx = 0; 
			}

			// Handle geom/dynamic obj/actor collisions seperately
			CRY_ASSERT(pVictimPhysicalEntity); 
			if(pVictimPhysicalEntity->GetiForeignData() != PHYS_FOREIGN_ID_STATIC && pVictimPhysicalEntity->GetiForeignData() != PHYS_FOREIGN_ID_TERRAIN)
			{
				IEntity* pVictimEntity = gEnv->pEntitySystem->GetEntityFromPhysics(pVictimPhysicalEntity);
				IActor*  pVictimActor  = NULL;
				if(pVictimEntity)
				{
					EntityId victimId = pVictimEntity->GetId();
					pVictimActor = pGameFramework->GetIActorSystem()->GetActor(victimId);
					if(pVictimActor && pVictimActor->IsPlayer())
					{
						if(!bAllowActors)
							return;
						HandleMeleeCollisionEvent_Actor(pEventPhysCollision, victimId, pOwnerPlayer, victimIdx, pVictimPhysicalEntity);
					}
					else
					{	
						HandleMeleeCollisionEvent_Object(pEventPhysCollision, pVictimEntity, pOwnerPlayer, victimIdx,pVictimPhysicalEntity);
					}
				}
			}
			else
			{
				if(pVictimPhysicalEntity->GetiForeignData() != PHYS_FOREIGN_ID_TERRAIN)
				{
					IRenderNode* pRenderNode = (IRenderNode*)pVictimPhysicalEntity->GetForeignData(PHYS_FOREIGN_ID_STATIC);
					if(pRenderNode != NULL && pRenderNode->GetRenderNodeType() == eERType_Brush)
					{
						IStatObj* pStatObj = pRenderNode->GetEntityStatObj();
						IRenderMesh* pRenderMesh = (IRenderMesh*)pStatObj->GetRenderMesh();
						if( pRenderMesh )
						{
							if( pRenderMesh->GetChunksSubObjects().size()>0 )
							{
								HandleMeleeCollisionEvent_BreakableGeometry(pEventPhysCollision, pOwnerPlayer, victimIdx, pVictimPhysicalEntity);
								return;
							}
						}
						else if( pStatObj->GetSubObjectCount()>0 )
						{
							HandleMeleeCollisionEvent_BreakableGeometry(pEventPhysCollision, pOwnerPlayer, victimIdx, pVictimPhysicalEntity);
							return;
						}	
					}
				}
			
				HandleMeleeCollisionEvent_Geometry(pEventPhysCollision, pOwnerPlayer, victimIdx);
			}
		}
	}
}


void CEnvironmentalWeapon::HandleMeleeCollisionEvent_Object(const EventPhysCollision *pEventPhysCollision, IEntity* pVictimEntity, CPlayer* pOwnerPlayer, int victimIdx, IPhysicalEntity* pVictimPhysicalEntity)
{
	// Rejection tests 
	EntityId victimId(0);
	if (pVictimEntity)						
	{
		victimId=pVictimEntity->GetId(); 
		if (EntityPreviouslyHit(victimId))
			return; 
	}

	// 1) Abort if collision not within our 'hitzone' cone 
	if(!PassesHitzoneConeCheck(pEventPhysCollision))
	{
		return;
	}

	// Pretty frustrating we cant do this in post-Init :( - leaving external from IsMeleecollisionWithinDamageZone to keep that func const :) (and no mutable fudges !) 
	if(!(m_logicFlags & ELogicFlag_damageZoneHelperInitialised))
	{
		InitDamageZoneHelper(); 
	}

	// 2) Abort if the entity was hit by a part of the weapon that isnt classed as the 'damaging' part. 
	const Vec3& collisionLocation = pEventPhysCollision->pt; 
	if(!IsMeleeCollisionWithinDamageZone(collisionLocation))
	{
#ifndef _RELEASE
		if(g_pGameCVars->pl_pickAndThrow.environmentalWeaponImpactDebugEnabled)
		{
			CryLog("[ENV-WEAPON]: Object hit by EnvWeapon but not in marked damage zone ");
		}
#endif //#ifndef _RELEASE
		return;
	}


	// Process
	EntityHitRecord hitRecord(victimId,
		pEventPhysCollision->n,
		pEventPhysCollision->idmat[victimIdx],
		pEventPhysCollision->partid[victimIdx],
		collisionLocation);

	if(ProcessMeleeHit(hitRecord, pVictimPhysicalEntity))
	{
		GenerateMaterialEffects(pEventPhysCollision->pt,pEventPhysCollision->idmat[victimIdx]);
		m_entityHitList.push_back(victimId);
	}

}

bool CEnvironmentalWeapon::PassesHitzoneConeCheck(const EventPhysCollision	*pEventPhysCollision) const
{
	const CCamera& cam			  = gEnv->p3DEngine->GetRenderingCamera();
	const Vec3& ownerPosition     = cam.GetPosition();
	const Vec3& collisionLocation = pEventPhysCollision->pt;
	Vec3 attackerFacingdir		  = cam.GetViewdir();
	attackerFacingdir			  = attackerFacingdir.normalize();
	if(!LocationWithinFrontalCone(ownerPosition, collisionLocation, attackerFacingdir,DEG2RAD(g_pGameCVars->pl_pickAndThrow.environmentalWeaponHitConeInDegrees)) )
	{
		// NOTE: we could have different hit cones for registering hits with entities / geometry
#ifndef _RELEASE
		if(g_pGameCVars->pl_pickAndThrow.environmentalWeaponImpactDebugEnabled)
		{
			CryLog("[ENV-WEAPON]: Something hit by EnvWeapon but not in %.f frontal hit cone", g_pGameCVars->pl_pickAndThrow.environmentalWeaponHitConeInDegrees);
		}
#endif //#ifndef _RELEASE
		return false;
	}

	// Passed ok
	return true;
}

void CEnvironmentalWeapon::HandleMeleeCollisionEvent_Actor(const EventPhysCollision *pEventPhysCollision, EntityId victimEntityId, CPlayer* pOwnerPlayer, int victimIdx, IPhysicalEntity* pVictimPhysicalEntity)
{
	
	// Rejection tests
	// 1) Abort if collision is with our owner
	// 2) Abort if already hit this entity during this attack																															
	if( victimEntityId == m_OwnerId ||																															
		EntityPreviouslyHit(victimEntityId))																																
	{
		return; 
	}

	// 3) Abort if victim entity not within our 'hitzone' cone 
	if(!PassesHitzoneConeCheck(pEventPhysCollision))
	{
		return;
	}

	// 4) Abort of the entity was hit by a part of the weapon that isnt classed as the 'damaging' part. 

	// Pretty frustrating we cant do this in post-Init :( - leaving external from IsMeleecollisionWithinDamageZone to keep that func const :) (and no mutable fudges !) 
	if(!(m_logicFlags & ELogicFlag_damageZoneHelperInitialised))
	{
		InitDamageZoneHelper(); 
	}


	const Vec3& collisionLocation = pEventPhysCollision->pt; 
	if(!IsMeleeCollisionWithinDamageZone(collisionLocation))
	{
#ifndef _RELEASE
		if(g_pGameCVars->pl_pickAndThrow.environmentalWeaponImpactDebugEnabled)
		{
			CryLog("[ENV-WEAPON]: Entity %d hit by EnvWeapon but not in marked damage zone ", victimEntityId);
		}
#endif //#ifndef _RELEASE
		return;
	}

	// 5) Check VisTable for blockages.
	IEntity* pVictimEntity = pVictimPhysicalEntity ? gEnv->pEntitySystem->GetEntityFromPhysics(pVictimPhysicalEntity) : NULL;
	if(pVictimEntity)
	{
		// VisTable check.
		CPlayerVisTable::SVisibilityParams targetQueryParams(victimEntityId);
		targetQueryParams.queryParams = eVQP_IgnoreGlass;
		targetQueryParams.heightOffset = (collisionLocation.z-pVictimEntity->GetWorldPos().z);

		const bool isVisible = g_pGame->GetPlayerVisTable()->CanLocalPlayerSee(targetQueryParams, 5);
		if(!isVisible)
		{
			return;
		}
	}
	
	// Add to our list of hit entities
	EntityHitRecord hitRecord(victimEntityId,
		pEventPhysCollision->n,
		pEventPhysCollision->idmat[victimIdx],
		pEventPhysCollision->partid[victimIdx],
		collisionLocation);
	
	if(ProcessMeleeHit(hitRecord, pVictimPhysicalEntity))
	{
		// If hit was successful, add to list of hit entities so don't hit again
		m_entityHitList.push_back(victimEntityId);

		// No matter what we hit (besides owner) Play any appropriate effects (TEST W HARDCODED MFX LIB)
		bool bCollisionWOwner = false; 
		if(pVictimEntity && pVictimEntity->GetId() == m_OwnerId)
		{
			bCollisionWOwner = true; 
		}

		if(!bCollisionWOwner)
		{	
			GenerateMaterialEffects(collisionLocation,pEventPhysCollision->idmat[victimIdx]);
		}

#ifndef _RELEASE
		if(m_pVelocityTracker)
		{
			// Cache the impact point for debug rendering
			m_weaponTrackerPosAtImpact = (GetEntity()->GetWorldTM() * m_pVelocityTracker->tm).GetTranslation();
		}
#endif //#ifndef _RELEASE
	}
}

void CEnvironmentalWeapon::InitDamageZoneHelper()
{
	static const char* pDamageZoneHelper = "damage_zone_helper"; 
	EntityId id							 = GetEntityId(); 
	int slot							   = PickAndThrow::FindActiveSlot(id);
	m_pDamageZoneHelper					 = PickAndThrow::FindHelperObject(pDamageZoneHelper,id,slot);
	m_logicFlags |= ELogicFlag_damageZoneHelperInitialised;  
}

void CEnvironmentalWeapon::HandleMeleeCollisionEvent_BreakableGeometry(const EventPhysCollision *pEventPhysCollision,  CPlayer* pOwnerPlayer, int victimIdx, IPhysicalEntity* pVictimPhysicalEntity)
{
	// 1) Abort if collision not within our 'hitzone' cone 
	if(!PassesHitzoneConeCheck(pEventPhysCollision))
	{
		return;
	}

	// Ensure helper object cached 
	if(!(m_logicFlags & ELogicFlag_damageZoneHelperInitialised))
	{
		OnObjectPhysicsPropertiesChanged(); 
	}

	// 2) Abort if the entity was hit by a part of the weapon that isnt classed as the 'damaging' part.
	const Vec3& collisionLocation = pEventPhysCollision->pt; 
	if(!IsMeleeCollisionWithinDamageZone(collisionLocation))
	{
#ifndef _RELEASE
		if(g_pGameCVars->pl_pickAndThrow.environmentalWeaponImpactDebugEnabled)
		{
			CryLog("[ENV-WEAPON]: Breakable Geom hit by EnvWeapon but not in marked damage zone ");
		}
#endif //#ifndef _RELEASE
		return;
	}

	GenerateMaterialEffects(collisionLocation,pEventPhysCollision->idmat[victimIdx]);	

	// Generate artificial collision here to let the physics system do everything it would normally do..
	// TODO: still need to tweak this slightly to obtain desired behaviour from trees + a few stubborn breakables 
	if(pVictimPhysicalEntity && (m_currentAttackState & EAttackStateType_EnactingPrimaryAttack))
	{
		Vec3 impulse = CalculateImpulse_Object(collisionLocation, m_weaponTrackerVelocity.GetNormalized(),m_OwnerId, CalculateVictimMass(pVictimPhysicalEntity));

		// We have to cheat a bit here (in the exact same way both SP pick and throw + melee 'punch' attack does).. collide between owner *player* and victim object (our env weapon object itself is geom_no_coll response so we can't use that).. 
		// but with properties setup to reflect materials / effects of environmental weapon. 
		ICharacterInstance *pUserCharacter = pOwnerPlayer->GetEntity()->GetCharacter(0);
		ISkeletonPose* pSkeletonPose = pUserCharacter ? pUserCharacter->GetISkeletonPose() : NULL;
		IPhysicalEntity* pCharacterPhysics = pSkeletonPose ? pSkeletonPose->GetCharacterPhysics() : NULL;
		if(pCharacterPhysics)
		{
			GenerateArtificialCollision(pCharacterPhysics, pVictimPhysicalEntity, pEventPhysCollision->idmat[0], collisionLocation,
				pEventPhysCollision->n, m_weaponTrackerVelocity, pEventPhysCollision->partid[1],
				pEventPhysCollision->idmat[1], pEventPhysCollision->iPrim[1], impulse);
		}
	}
}

void CEnvironmentalWeapon::GenerateArtificialCollision(IPhysicalEntity* pAttackerPhysEnt, IPhysicalEntity *pCollider, int colliderMatId, const Vec3 &position, const Vec3& normal, const Vec3 &vel, int partId, int surfaceIdx, int iPrim, const Vec3& impulse )
{
	//////////////////////////////////////////////////////////////////////////
	// Apply impulse to object.
	pe_action_impulse ai;
	ai.partid = partId;
	ai.point = position;
	ai.impulse = impulse;
	pCollider->Action(&ai);

	//////////////////////////////////////////////////////////////////////////
	// create a physical collision 
	pe_action_register_coll_event collision;

	collision.collMass = 0.005f; // this needs to be set to something.. but is seemingly ignored in calculations... (just doing what meleeCollisionHelper does here..)
	collision.partid[1] = partId;

	// collisions involving partId<-1 are to be ignored by game's damage calculations
	// usually created articially to make stuff break.
	collision.partid[0] = -2;
	collision.idmat[1] = surfaceIdx;
	collision.idmat[0] = colliderMatId;
	collision.n = normal;
	collision.pt = position;
	collision.vSelf = vel;
	collision.v = Vec3(0,0,0);
	collision.pCollider = pCollider;
	collision.iPrim[0] = -1;
	collision.iPrim[1] = iPrim;

	pAttackerPhysEnt->Action(&collision);
}

void CEnvironmentalWeapon::HandleMeleeCollisionEvent_Geometry(const EventPhysCollision	*pEventPhysCollision, CPlayer* pOwnerPlayer, int victimIdx)
{
	// 1) Abort if collision not within our 'hitzone' cone 
	if(!PassesHitzoneConeCheck(pEventPhysCollision))
	{
#ifndef _RELEASE
		m_debugCollisionPts_Geom.push_back(debugCollisionPt(pEventPhysCollision->pt, ColorB(255,0,0)));
#endif // #ifndef _RELEASE
		return;
	}

	// Pretty frustrating we cant do this in post-Init :( - leaving external from IsMeleecollisionWithinDamageZone to keep that func const :) (and no mutable fudges !) 
	if(!(m_logicFlags & ELogicFlag_damageZoneHelperInitialised))
	{
		InitDamageZoneHelper(); 
	}

	// 2) Abort if the entity was hit by a part of the weapon that isnt classed as the 'damaging' part. 
	const Vec3& collisionLocation = pEventPhysCollision->pt;
	if(!IsMeleeCollisionWithinDamageZone(collisionLocation))
	{
#ifndef _RELEASE
		if(g_pGameCVars->pl_pickAndThrow.environmentalWeaponImpactDebugEnabled)
		{
			CryLog("[ENV-WEAPON]: Geom hit by EnvWeapon but not in marked damage zone ");
		}

		if(m_pDamageZoneHelper)
		{
			const Matrix34& damageZoneHelperMat = GetEntity()->GetWorldTM() * m_pDamageZoneHelper->tm;
			const Vec3& dzhWPos = damageZoneHelperMat.GetTranslation(); 
			const Vec3& dzhDir  = damageZoneHelperMat.GetColumn1();
			m_debugCollisionPts_Geom.push_back(debugCollisionPt(pEventPhysCollision->pt, ColorB(0,0,255), dzhWPos, dzhDir));
		}

#endif //#ifndef _RELEASE
		return;
	}

	// Only play material effects on first geometry impact
	if(!(m_logicFlags & ELogicFlag_geomCollisionDetectedThisAttack))
	{
		GenerateMaterialEffects(collisionLocation,pEventPhysCollision->idmat[victimIdx]);
		m_logicFlags |= ELogicFlag_geomCollisionDetectedThisAttack; 

#ifndef _RELEASE
		if(m_pDamageZoneHelper)
		{
			const Matrix34& damageZoneHelperMat = GetEntity()->GetWorldTM() * m_pDamageZoneHelper->tm;
			const Vec3& dzhWPos = damageZoneHelperMat.GetTranslation(); 
			const Vec3& dzhDir  = damageZoneHelperMat.GetColumn1();
			m_debugCollisionPts_Geom.push_back(debugCollisionPt(pEventPhysCollision->pt, ColorB(0,255,0), dzhWPos, dzhDir));
		}
#endif // #ifndef _RELEASE
	}
}



void CEnvironmentalWeapon::GenerateMaterialEffects(const Vec3& pos, const int surfaceIndex)
{
	// Abort if no mfx lib set
	if(!m_mfxLibraryName.length())
	{
		return; 
	}

	IMaterialEffects* pMaterialEffects = gEnv->pGameFramework->GetIMaterialEffects();

	TMFXEffectId effectId = pMaterialEffects->GetEffectId(m_mfxLibraryName.c_str(),surfaceIndex);
	if (effectId != InvalidEffectId)
	{
		SMFXRunTimeEffectParams params;
		params.pos = pos;
		params.playflags = eMFXPF_All | eMFXPF_Disable_Delay;
		//params.soundSemantic = eSoundSemantic_Player_Foley;
		pMaterialEffects->ExecuteEffect(effectId, params);
	}
}

// Test against simple damage marker (pos + dir). Can always add start + end zone markers if necessary. For now start only will suffice. 
bool CEnvironmentalWeapon::IsMeleeCollisionWithinDamageZone(const Vec3& wSpaceCollisionPoint) const
{
	if(m_pDamageZoneHelper)
	{
		// Calculate inverse helper mat each time we have an impact. Can always cache the inverse mat once on startup
		// if need be, but more concerned with memory overhead at this point. Can easily change if necessary. 
		Matrix34 invMtxHelperWorld =  GetEntity()->GetWorldTM() * m_pDamageZoneHelper->tm;
		invMtxHelperWorld.InvertFast();

		// If the wSpaceCollisionPoint is in FRONT of the helper (based on both its pos and facing dir) we are in damage zone. 
		// IS calcing another vector and doing dot prod > 0 check in Wspace faster than this?
		return (invMtxHelperWorld.TransformPoint(wSpaceCollisionPoint).y > 0.0f);
	}

	// Always true if no helper.
	return true; 
}

bool CEnvironmentalWeapon::HandleChargedThrowCollisionEvent( const EventPhysCollision	*pEventPhysCollision )
{
	IGameFramework* pGameFramework = g_pGame->GetIGameFramework(); 
	IActor* pOwnerActor					   = pGameFramework->GetIActorSystem()->GetActor(m_throwerId);
	if(pOwnerActor && pOwnerActor->IsPlayer())
	{
		IPhysicalEntity *pPhysics = GetEntity()->GetPhysics();
		if(pPhysics)
		{
			// Work out which of the physical entities is the victim. 
			int victimIdx = 1; 
			IPhysicalEntity* pVictimPhysicalEntity = pEventPhysCollision->pEntity[1];
			if (pVictimPhysicalEntity == pPhysics)
			{
				pVictimPhysicalEntity = pEventPhysCollision->pEntity[0];
				victimIdx = 0; 
			}

			const int weaponIdx = 1 - victimIdx;

			const Vec3 &impactVelocity = pEventPhysCollision->vloc[weaponIdx];

			pe_status_dynamics status;
			pPhysics->GetStatus(&status);

			if (status.v.dot(impactVelocity) < 0.f)
			{
				// Prevent further hits from killing, won't effect this hit since the check is made before this point
				m_bCanInstantKill = false;
			}

			// We care about collisions with everything, including geometry
			if(pVictimPhysicalEntity)
			{
				bool bHandledPhysCollision = false;

				IEntity* pVictimEntity = gEnv->pEntitySystem->GetEntityFromPhysics(pVictimPhysicalEntity);
				if(pVictimEntity)
				{
					// Rejection test 1) Abort if collision is with our owner
					EntityId victimEntityId = pVictimEntity->GetId(); 																															
					if (( victimEntityId == m_throwerId) || EntityPreviouslyHit(victimEntityId))
					{
						return false;
					}
					else
					{
						// If we hit a player, who is not our owner.. lets process the hit
						IActor* pActor = g_pGame->GetIGameFramework()->GetIActorSystem()->GetActor(victimEntityId);
						if(pActor && pActor->IsPlayer())
						{
							bool blocked = false;
							EntityId playerWeaponId = pActor->GetInventory()->GetCurrentItem();
							CPickAndThrowWeapon* pPickAndThrow = static_cast<CPickAndThrowWeapon*>(g_pGame->GetIGameFramework()->QueryGameObjectExtension(playerWeaponId, "PickAndThrowWeapon"));
							if (pPickAndThrow && pPickAndThrow->ShieldsPlayer())
							{
								static const float maxDotProduct = cosf(DEG2RAD(65.f));
								static const float maxZ = sinf(DEG2RAD(70.f));
								IPhysicalEntity* pPlayerPhysics = pActor->GetEntity()->GetPhysics();
								pe_status_pos psp;
								pPlayerPhysics->GetStatus(&psp);
								const Vec3 playerCentrePos = psp.pos + (psp.BBox[0] + psp.BBox[1])*0.5f;
								const Vec3 playerDir = pActor->GetEntity()->GetRotation().GetColumn1();
								const Vec3 dir = m_lastPos - playerCentrePos;
								const float z = dir.GetNormalized().z;
								Vec3 dirXY = Vec3(dir.x, dir.y, 0.f);
								const float dotProduct = dirXY.GetNormalized().dot(playerDir);
								blocked = (dotProduct > maxDotProduct) && (z < maxZ);
							}

							if (!blocked)
							{
								// If we exceed required kill velocity
								float requiredKillVelocity  = g_pGameCVars->pl_pickAndThrow.minRequiredThrownEnvWeaponHitVelocity;
								if((m_noRequiredKillVelocityTimer > 0.0f) || impactVelocity.GetLengthSquared() > (requiredKillVelocity * requiredKillVelocity))
								{
									// Construct a hit record and calc impact direction
									EntityHitRecord hitRecord(victimEntityId,
											pEventPhysCollision->n,
											pEventPhysCollision->idmat[victimIdx],
											pEventPhysCollision->partid[victimIdx],
											pEventPhysCollision->pt);

									// Apply
									ProcessHitPlayer(pActor, hitRecord, impactVelocity.GetNormalized(), m_throwerId); 
								}

#ifndef _RELEASE
								if(g_pGameCVars->pl_pickAndThrow.environmentalWeaponImpactDebugEnabled && impactVelocity.GetLengthSquared() <= requiredKillVelocity*requiredKillVelocity)
								{
									CryLog("[ENV-WEAPON]: Entity %d hit by THROWN EnvWeapon but [%.f] below required velocity [%.f]", victimEntityId, impactVelocity.GetLength(), requiredKillVelocity);
								}
#endif //#ifndef _RELEASE
							}
							else
							{
								m_entityHitList.push_back(victimEntityId);
							}
						}
						else
						{
							EntityHitRecord hitRecord(victimEntityId,
								pEventPhysCollision->n,
								pEventPhysCollision->idmat[victimIdx],
								pEventPhysCollision->partid[victimIdx],
								pEventPhysCollision->pt);
							ProcessHitObject(hitRecord, impactVelocity, pVictimPhysicalEntity, m_throwerId);
						}

						m_entityHitList.push_back(victimEntityId);
					}
					
				}
				else
				{
					GenerateArtificialCollision(pPhysics, pVictimPhysicalEntity, pEventPhysCollision->idmat[0], pEventPhysCollision->pt,
						pEventPhysCollision->n, m_weaponTrackerVelocity, pEventPhysCollision->partid[victimIdx],
						pEventPhysCollision->idmat[victimIdx], pEventPhysCollision->iPrim[victimIdx], impactVelocity);
				}
				
				// We have hit *something* at this point. So we can clean up our constraints, the original thrower can be hit by rebounds etc
				CleanUpThrowerPhysicsConstraints(); 

				// Hit something we care about
				return true; 
			}
		}
	}

	// Didnt hit anything
	return false; 
}

void CEnvironmentalWeapon::GenerateHitInfo(EntityId attackerEntityId, float damage,  HitInfo& hitInfo, const EntityHitRecord& entityHitRecord, const Vec3& nImpactDir, int hitTypeId ) const
{
	Vec3 point		 = entityHitRecord.m_wSpaceHitLoc;
	Vec3 dir			 = nImpactDir; 
	Vec3 normal		 = -nImpactDir; // Set to be reverse to ensure we pass backFace hit rejection test
	int surfaceIdx = entityHitRecord.m_surfaceIdx;
	int partId		 = entityHitRecord.m_partId; 

	hitInfo				 = HitInfo(attackerEntityId, entityHitRecord.m_entityId, GetEntityId(), damage, 0.0f, surfaceIdx, partId, hitTypeId, point, dir, normal);

	// Set additional params
	hitInfo.knocksDown  = false; // May want this to true?
	hitInfo.remote		  = false; // false if local client doing the hit... true if not. 
}

Vec3 CEnvironmentalWeapon::CalculateImpulse_Player( const HitInfo& hitInfo, CPlayer& rPlayer, const Vec3& nImpactDir, EntityId attackerId ) const
{
	// Decide how fast we want to throw this ragdoll
	float desiredRagdollVelocity = m_ragdollPostMeleeImpactSpeed; 
	if(hitInfo.type == CGameRules::EHitType::EnvironmentalThrow)
	{
		desiredRagdollVelocity = m_ragdollPostThrowImpactSpeed; 
	}

	if (IPhysicalEntity* pVictimPhysicalEntity = rPlayer.GetEntity()->GetPhysics())
	{
		// Calc impulse required to move at targetVelocity
		float victimMass = CalculateVictimMass(pVictimPhysicalEntity); 
		Vec3 impulseVec  = hitInfo.dir * victimMass * desiredRagdollVelocity * g_pGameCVars->pl_pickAndThrow.environmentalWeaponImpulseScale; 

		// LOGGING
#ifndef _RELEASE
		if(g_pGameCVars->pl_pickAndThrow.environmentalWeaponImpactDebugEnabled)
		{
			CryLog("[ENV-WEAPON]: Victim Mass calculated at %.f", victimMass);
		}
#endif //#ifndef _RELEASE

		// Build an impulse that moves the ragdoll in the direction of the hit AND away from the player (driven by cvar ratios). This way we can make the hit 
		// look realistic, yet try and keep the ragdoll on screen so the attacker can see the results of their attack
		// Yes this is a crude NLerp blend , but looks fine (NLerps <= 90 degrees rot have minute error) and is much cheaper. Switch to more expensive Quat Slerp if notice problems. 
		const Matrix34& attackerMat = gEnv->pEntitySystem->GetEntity(attackerId)->GetWorldTM();
		Vec3 toVictimVec					= hitInfo.pos - attackerMat.GetTranslation();
		float awayFromPlayerRatio = min(g_pGameCVars->pl_pickAndThrow.awayFromPlayerImpulseRatio, 1.0f);

		if(m_currentAttackState & EAttackStateType_ChargedThrow)
		{
			// With a charged throw, the impact always pushes the victem purely in the 'realistic' impact dir
			awayFromPlayerRatio = 0.0f; 
		}

		Vec3 impulseVector;
		if( (m_logicFlags & ELogicFlag_hasHitOverride) == 0 )
		{	
			impulseVector = (awayFromPlayerRatio * toVictimVec.GetNormalized()) + ((1.0f - awayFromPlayerRatio) * nImpactDir);
		}
		else
		{
			const float hitOverrideDirRatio = (m_overrideHitDirMaxRatio-m_overrideHitDirMinRatio)*cry_random(0.0f, 1.0f) + m_overrideHitDirMinRatio;
			const Vec3 worldOverrideHitDir = attackerMat.TransformVector(m_overrideHitDir);

			impulseVector = ((1.f - hitOverrideDirRatio) * toVictimVec.GetNormalized()) + ((hitOverrideDirRatio) * worldOverrideHitDir.GetNormalized());
		}
		impulseVector.Normalize(); 
		impulseVector						   *= victimMass * desiredRagdollVelocity * g_pGameCVars->pl_pickAndThrow.environmentalWeaponImpulseScale; 
		return impulseVector;
	}

	// Failed
	CryLog("[ENV-WEAPON]: Player has no physics, Zero impulse returned ");
	return Vec3(0.0f,0.0f,0.0f); 
}


Vec3 CEnvironmentalWeapon::CalculateImpulse_Object( const Vec3& hitLoc, const Vec3& nImpactDir, EntityId attackerId, const float victimObjectMass ) const 
{
	
	// Build an impulse that moves the object in the direction of the hit AND away from the player (driven by cvar ratios). This way we can make the hit 
	// look realistic, yet try and keep the object on screen so the attacker can see the results of their attack
	// Yes this is a crude NLerp blend , but looks fine (NLerps <= 90 degrees rot have minute error) and is much cheaper. Switch to more expensive Quat Slerp if notice problems. 
	Vec3 toObjectVec		  = hitLoc - gEnv->pEntitySystem->GetEntity(attackerId)->GetWorldPos();
	float awayFromPlayerRatio = min(g_pGameCVars->pl_pickAndThrow.awayFromPlayerImpulseRatio, 1.0f);
	Vec3 impulseVec					  = (awayFromPlayerRatio * toObjectVec.GetNormalized()) + ((1.0f - awayFromPlayerRatio) * nImpactDir);
	impulseVec.Normalize(); 
	impulseVec						   *= m_mass* m_weaponTrackerVelocity.GetLengthFast() * g_pGameCVars->pl_pickAndThrow.environmentalWeaponObjectImpulseScale; 


	// We want to scale down the impulse a bit when hitting tiny low mass objects, otherwise we send them into orbit.
	// After all we are hitting them at roughly 90km/h with a 250kg object... 
	if(victimObjectMass && victimObjectMass < g_pGameCVars->pl_pickAndThrow.objectImpulseLowMassThreshold)
	{
		static float s_objectImpulseLowMassThreshold = g_pGameCVars->pl_pickAndThrow.objectImpulseLowMassThreshold;
		static float s_objectImpulseLowerScaleLimit  = g_pGameCVars->pl_pickAndThrow.objectImpulseLowerScaleLimit;
		static float s_diff							 = (1.0f - s_objectImpulseLowerScaleLimit); 

		// Only refresh from CVars every frame when not in release build
#ifndef _RELEASE
		s_objectImpulseLowMassThreshold = g_pGameCVars->pl_pickAndThrow.objectImpulseLowMassThreshold;
		s_objectImpulseLowerScaleLimit  = g_pGameCVars->pl_pickAndThrow.objectImpulseLowerScaleLimit;
		s_diff							= (1.0f - s_objectImpulseLowerScaleLimit); 
#endif //#ifndef _RELEASE

		// Lerp between min -> max impulse limits
		float tVal  = (victimObjectMass * __fres(s_objectImpulseLowMassThreshold)); // [0.0f,1.0f] (equivalent to 'victimObjectMass / g_pGameCVars->pl_pickAndThrow.objectImpulseLowMassThreshold' )
		impulseVec *= ((tVal * s_diff) + s_objectImpulseLowerScaleLimit);
	}


	return impulseVec;
}



void CEnvironmentalWeapon::ApplyImpulse(const HitInfo& hitInfo, const EntityId victimEntityId) const
{
	IEntity* pEntity = gEnv->pEntitySystem->GetEntity(victimEntityId); 
	if(!pEntity)
	{
		return; 
	}

	IPhysicalEntity* pVictimPhysicalEntity = pEntity->GetPhysics();

	if (pVictimPhysicalEntity)
	{
		CActor* pActor = static_cast<CActor*>(g_pGame->GetIGameFramework()->GetIActorSystem()->GetActor(pEntity->GetId()));
		if(pActor) // We ragdollize the actor even though they aren't dead (cue monty python 'im not dead!' shouts).
		{
			// Set the impulse we want to apply to the *living* entity
			const Vec3 vImpulseVec  = (hitInfo.impulseScale * hitInfo.dir); 

			pe_action_impulse livingEntImpulse;
			livingEntImpulse.impulse    = vImpulseVec * g_pGameCVars->pl_pickAndThrow.environmentalWeaponLivingToArticulatedImpulseRatio;
			livingEntImpulse.partid     = hitInfo.partId; 
			livingEntImpulse.iApplyTime = 0; 
			pVictimPhysicalEntity->Action(&livingEntImpulse);

			// Set the impulse we want applied ASAP after the on Ragdollized event ( To the articulated ragdoll )
			pe_action_impulse articulatedEntImpulse;
			articulatedEntImpulse.impulse    = vImpulseVec * (1.0f - g_pGameCVars->pl_pickAndThrow.environmentalWeaponLivingToArticulatedImpulseRatio);
			articulatedEntImpulse.point      = hitInfo.pos;
			articulatedEntImpulse.partid     = hitInfo.partId; 
			articulatedEntImpulse.iApplyTime = 0; 
			pActor->GetImpulseHander()->SetOnRagdollPhysicalizedImpulse(articulatedEntImpulse); 

			CRecordingSystem *pRecordingSystem = g_pGame->GetRecordingSystem();
			if (pRecordingSystem)
			{
				pRecordingSystem->OnForcedRagdollAndImpulse(victimEntityId, vImpulseVec, hitInfo.pos, hitInfo.partId, 0 );
			}

			// LOGGING
#ifndef _RELEASE
			if(g_pGameCVars->pl_pickAndThrow.environmentalWeaponImpactDebugEnabled)
			{
				CryLog("[ENV-WEAPON]: Ragdollizing Actor %d", pActor->GetEntityId());
				CryLog("[ENV-WEAPON]: Applying impulse - living - [ %.f, %.f, %.f ]to actor %d", livingEntImpulse.impulse.x, livingEntImpulse.impulse.y, livingEntImpulse.impulse.z,pActor->GetEntityId());
				CryLog("[ENV-WEAPON]: Applying impulse - articulated - [ %.f, %.f, %.f ]to actor %d to part %d, pt <%.1f,%.1f,%.1f>", articulatedEntImpulse.impulse.x, articulatedEntImpulse.impulse.y, articulatedEntImpulse.impulse.z,pActor->GetEntityId(), articulatedEntImpulse.partid, articulatedEntImpulse.point.x, articulatedEntImpulse.point.y, articulatedEntImpulse.point.z);
			}
#endif //#ifndef _RELEASE
		}
		// OBJECTS
		else
		{
			const Vec3 vImpulseVec  = (hitInfo.impulseScale * hitInfo.dir); 
			
			IActor* pOwnerActor	= gEnv->pGameFramework->GetIActorSystem()->GetActor(m_OwnerId);
			CPlayer* pOwnerPlayer = static_cast<CPlayer*>(pOwnerActor);
			if(pOwnerActor && pOwnerActor->IsPlayer())
			{
				SmartScriptTable propertiesTable;
				IScriptTable* pEntityScript = pEntity->GetScriptTable();
				if (pEntityScript)
				{
					if(pEntityScript->GetValue("Properties", propertiesTable))
					{
						const CLargeObjectInteraction& largeObjectInteraction = pOwnerPlayer->GetLargeObjectInteraction();
						if(largeObjectInteraction.IsLargeObject( pEntity, propertiesTable ))
						{
							if(!pOwnerPlayer->GetLargeObjectInteraction().IsBusy())
							{
								pOwnerPlayer->EnterLargeObjectInteraction(pEntity->GetId(), true); 
							}
						}
					}
				}
			}
			else
			{
				pEntity->AddImpulse(hitInfo.partId, hitInfo.pos, vImpulseVec, true, 1.0f, 1.0f);
			}

			// LOGGING
#ifndef _RELEASE
			if(g_pGameCVars->pl_pickAndThrow.environmentalWeaponImpactDebugEnabled)
			{
				CryLog("[ENV-WEAPON]: Applying impulse [ %.f, %.f, %.f ]to physics proxy with entityId: %d", vImpulseVec.x, vImpulseVec.y, vImpulseVec.z,pEntity->GetId());
			}
#endif //#ifndef _RELEASE
		}
	}
}

bool CEnvironmentalWeapon::EntityPreviouslyHit( const EntityId queryId ) const
{
	if(m_entityHitList.size() > 0)
	{
		TEntityHitList::const_iterator endIter = m_entityHitList.end(); 
		for(TEntityHitList::const_iterator iter = m_entityHitList.begin(); iter != endIter; ++iter)
		{
			if(*iter == queryId)
			{
				return true;
			}
		}
	}

	return false; 
}

void CEnvironmentalWeapon::ProcessHitPlayer( IActor* pVictimActor, const EntityHitRecord& entityHitRecord, const Vec3& nImpactDir, EntityId attackerId )
{

	CPlayer* pVictimPlayer = static_cast<CPlayer*>(pVictimActor);
	if(!pVictimPlayer->IsDead() && AllowedToDamagePlayer(attackerId, entityHitRecord.m_entityId))
	{
		// LOGGING
#ifndef _RELEASE
		if(g_pGameCVars->pl_pickAndThrow.environmentalWeaponImpactDebugEnabled)
		{
			CryLog("[ENV-WEAPON]: Enemy Player %d hit by EnvWeapon", pVictimPlayer->GetEntityId());
		}
#endif //#ifndef _RELEASE

		IStatsTracker* pTracker = NULL;
		
		if( m_OwnerId )
		{	
			if( IActor* pOwnerActor = gEnv->pGameFramework->GetIActorSystem()->GetActor( m_OwnerId ) )
			{
				if( CStatsRecordingMgr* pRecordingMgr = g_pGame->GetStatsRecorder() )
				{
					pTracker = pRecordingMgr->GetStatsTracker( pOwnerActor );
				}
			}
		}

		// Kill player
		HitInfo hitInfo;
		if(m_currentAttackState & EAttackStateType_EnactingPrimaryAttack)
		{
			// Melee
			GenerateHitInfo(attackerId,sDamageRequiredForCertainDeath, hitInfo, entityHitRecord,nImpactDir,CGameRules::EHitType::EnvironmentalMelee);

			if( pTracker )
			{
				pTracker->Event( eGSE_EnvironmentalWeaponEvent, new CEnvironmentalWeaponEvent( GetEntityId(), eEVE_MeleeKill, 0 ) );
			}
		}
		else 
		{
			// Charged throw
			CRY_ASSERT_MESSAGE(m_currentAttackState & EAttackStateType_ChargedThrow, "CEnvironmentalWeapon::ProcessHitPlayer < ERROR - attempting to process player hit with unknown attack state");
			
			GenerateHitInfo(attackerId, sDamageRequiredForCertainDeath, hitInfo, entityHitRecord, nImpactDir, CGameRules::EHitType::EnvironmentalThrow);

			if( pTracker )
			{
				pTracker->Event( eGSE_EnvironmentalWeaponEvent, new CEnvironmentalWeaponEvent( GetEntityId(), eEVE_ThrowKill, 0 ) );
			}

			CPersistantStats::GetInstance()->IncrementStatsForActor( m_OwnerId, EIPS_ThrownWeaponKills );

			if(pVictimPlayer->GetEntityId() == m_previousThrowerId)
			{
				if(strcmpi(GetClassificationName(), "pole") == 0)
				{
					CPersistantStats::GetInstance()->IncrementStatsForActor( m_OwnerId, EIPS_KillsWithTheirThrownPole );
				}
				else
				{
					CPersistantStats::GetInstance()->IncrementStatsForActor( m_OwnerId, EIPS_KillsWithTheirThrownShield );
				}
			}
		}

		// Now Calculate any impulse required and add it to the hitInfo
		Vec3 impulse = CalculateImpulse_Player(hitInfo, *pVictimPlayer, nImpactDir,attackerId);

		// Overwrite the hitInfo dir + impulse scale
		hitInfo.dir						= impulse.GetNormalized();
		hitInfo.normal				= -hitInfo.dir; 
		hitInfo.impulseScale  = impulse.GetLength(); 

		if(CGameRules *pGameRules = g_pGame->GetGameRules())
		{
			// Want to ensure this kit acts as if the victim was killed
			hitInfo.forceLocalKill = true; 
			pGameRules->ClientHit(hitInfo);
		}

		// LOCALLY we apply an impulse straight away to prevent visual lag.. other players will catch up when they receive the hitInfo
		ApplyImpulse(hitInfo,entityHitRecord.m_entityId);
		
		// Note - We could always pack impulse scale as (hitInfo.impulseScale * 0.1f) and re-scale it when extracting if bandwidth becomes an issue (but less clear to anyone debugging it) 
		// If we are getting impulses greater than our 8191.0f compression policy allows we need to adjust something
#ifndef _RELEASE
		if(hitInfo.impulseScale > 8191.0f) // 'dmg' compression policy max
		{
			CryLogAlways("[ENV-WEAPON]: CEnvironmentalWeapon::ProcessHitPlayer() < Warning - impulse scale greater than compression policy capacity. This should probably be adjusted"); 
		}
#endif //#ifndef _RELEASE
	}
}

void CEnvironmentalWeapon::ProcessHitObject( const EntityHitRecord& entityHitRecord, const Vec3& impactVel, IPhysicalEntity* pVictimPhysicalEntity, EntityId attackerId ) const
{
	const float impactSpeed = impactVel.GetLength();
	const Vec3 nImpactDir = (impactSpeed > 0.f) ? (impactVel / impactSpeed) : Vec3(0.f, 1.f, 0.f);

	// VEHICLES
	if (g_pGame->GetIGameFramework()->GetIVehicleSystem()->GetVehicle(entityHitRecord.m_entityId))
	{
		HitInfo hitInfo;
		if (m_currentAttackState & EAttackStateType_EnactingPrimaryAttack)
		{
			// Melee
			GenerateHitInfo(attackerId,m_vehicleMeleeDamage, hitInfo, entityHitRecord,nImpactDir,CGameRules::EHitType::EnvironmentalMelee);
		}
		else
		{
			// Charged throw
			CRY_ASSERT_MESSAGE(m_currentAttackState & EAttackStateType_ChargedThrow, "CEnvironmentalWeapon::GenerateHitInfo < ERROR - attempting to process hit when in an unknown state");
			float damageFraction = (impactSpeed - m_vehicleThrowMinVelocity) / (m_vehicleThrowMaxVelocity - m_vehicleThrowMinVelocity);
			damageFraction = clamp_tpl(damageFraction, 0.f, 1.f);
			GenerateHitInfo(attackerId, m_vehicleThrowDamage * damageFraction, hitInfo, entityHitRecord, nImpactDir, CGameRules::EHitType::EnvironmentalThrow);
		}

		CGameRules *pGameRules = g_pGame->GetGameRules();
		if (pGameRules)
		{
			pGameRules->ClientHit(hitInfo);
		}

		return;
	}

	// OTHER ENVIRONMENTAL WEAPONS
	CEnvironmentalWeapon *pEnvWeapon = static_cast<CEnvironmentalWeapon*>(g_pGame->GetIGameFramework()->QueryGameObjectExtension(entityHitRecord.m_entityId, "EnvironmentalWeapon"));
	if(pEnvWeapon && pEnvWeapon->m_OwnerId)
	{
		// If hitting an environmental weapon held by another player... don't want to impulse it!
		// TODO: Could add blocking here (just add owner id to 'already hit' list)
		return;
	}

	// OTHER
	HitInfo hitInfo;
	if(m_currentAttackState & EAttackStateType_EnactingPrimaryAttack )
	{
		// Melee
		GenerateHitInfo(attackerId,sDamageRequiredForCertainDeath, hitInfo, entityHitRecord,nImpactDir,CGameRules::EHitType::EnvironmentalMelee);
	
		// Now Calculate any impulse required and add it to the hitInfo
		Vec3 impulse = CalculateImpulse_Object(hitInfo.pos, nImpactDir,attackerId, CalculateVictimMass(pVictimPhysicalEntity));

		// Overwrite the hitInfo dir + impulse scale
		hitInfo.dir			  = impulse.GetNormalized();
		hitInfo.normal		  = -hitInfo.dir; 
		hitInfo.impulseScale  = impulse.GetLength(); 

		ApplyImpulse(hitInfo,entityHitRecord.m_entityId);
	}
	
}

void CEnvironmentalWeapon::OnStartChargedThrow()
{

	{
		GetEntity()->PhysicsNetSerializeEnable(true);
	}

	m_initialThrowPos = GetEntity()->GetWorldPos();

	m_bCanInstantKill = true;
	m_noRequiredKillVelocityTimer = sNoRequiredKillVelocityWindow;

	bool bOwnerIsLocal = (g_pGame->GetIGameFramework()->GetClientActorId() == m_OwnerId);
	if(bOwnerIsLocal)
	{
		GetGameObject()->EnablePhysicsEvent(true, eEPE_OnCollisionLogged);
	}
	m_currentAttackState = EAttackStateType_ChargedThrow; 
	m_throwerId = m_OwnerId; 
	m_entityHitList.clear(); 

	IPhysicalEntity *pPE = GetEntity()->GetPhysics();

	pe_params_flags pf; 
	pf.flagsOR = ref_small_and_fast;
	pPE->SetParams(&pf);

	// Want to ignore collisions with JUST the throwing player... this seems a bit fudgey.. but it achieves what we want. 
	if(m_throwerId)
	{
		IEntity* pThrowerEntity = gEnv->pEntitySystem->GetEntity(m_throwerId);
		if(pThrowerEntity)
		{
			IPhysicalEntity *pLivingThrowerPE = pThrowerEntity->GetPhysics();
			if (pLivingThrowerPE)
			{
				// Disable collisions with the player living entity
				pe_action_add_constraint ic;
				ic.flags = constraint_inactive|constraint_ignore_buddy;
				ic.pBuddy = pLivingThrowerPE;
				ic.pt[0].Set(0,0,0);
				m_livingConstraintID = pPE->Action(&ic);

				// Disable collisions with the player articulated entity
				ICharacterInstance* pChar = pThrowerEntity->GetCharacter(0); 
				if(pChar)
				{
					IPhysicalEntity* pArticThrowerPE = pChar->GetISkeletonPose()->GetCharacterPhysics();
					if(pArticThrowerPE)
					{
						ic.pBuddy = pArticThrowerPE;
						m_articulatedConstraintID = pPE->Action(&ic);
					}
				}
			}
		}
	}

#ifndef _RELEASE
	if(g_pGameCVars->pl_pickAndThrow.environmentalWeaponImpactDebugEnabled)
	{
		const CCamera& cam = gEnv->p3DEngine->GetRenderingCamera(); 
		m_throwDir = cam.GetViewdir(); 
	}
#endif //#ifndef_RELEASE

	IActor* pOwnerActor = gEnv->pGameFramework->GetIActorSystem()->GetActor( m_OwnerId );
	CStatsRecordingMgr* pRecordingMgr = g_pGame->GetStatsRecorder();

	int16 health = (int16)GetCurrentHealth();

	if( IStatsTracker* pTracker = pRecordingMgr ? pRecordingMgr->GetStatsTracker( pOwnerActor ) : NULL )
	{
		pTracker->Event( eGSE_EnvironmentalWeaponEvent, new CEnvironmentalWeaponEvent( GetEntityId(), eEVE_ThrowAttack, health ) );
	}
}

bool CEnvironmentalWeapon::OnAnimationEvent( const AnimEventInstance &event )
{
	if (event.m_EventNameLowercaseCRC32 == CActor::GetAnimationEventsTable().m_detachEnvironmentalWeapon)
	{
		OnRipDetachEvent();
		return true; 
	}

	// return true if successfully processed; 
	if(m_logicFlags & ELogicFlag_processDamageAnimEvents)
	{
		if(event.m_EventNameLowercaseCRC32 == CActor::GetAnimationEventsTable().m_meleeStartDamagePhase)
		{
			OnMeleeStartDamagePhaseEvent(); 
			return true;
		}
		else if (event.m_EventNameLowercaseCRC32 == CActor::GetAnimationEventsTable().m_meleeEndDamagePhase)
		{
			OnMeleeEndDamagePhaseEvent(); 
			return true; 
		}
	}

	return false; 
}

void CEnvironmentalWeapon::OnMeleeStartDamagePhaseEvent()
{
	// Only the client actually *doing* the physical melee decides if a collision is to be processed
	// and a hit has been achieved to allow for responsive impact/ragdoll effects 
	if(g_pGame->GetIGameFramework()->GetClientActorId() == m_OwnerId)
	{
		GetGameObject()->EnablePhysicsEvent(true, eEPE_OnCollisionLogged);

		pe_params_flags pf; 
		pf.flagsOR = ref_small_and_fast;
		GetEntity()->GetPhysics()->SetParams(&pf);

		m_logicFlags |= ELogicFlag_damagePhaseActive;  

		// LOGGING
#ifndef _RELEASE
		if(g_pGameCVars->pl_pickAndThrow.environmentalWeaponImpactDebugEnabled)
		{
			CryLog("[ENV-WEAPON]: Start Damage event received");
		}
#endif //#ifndef _RELEASE

	}
}

void CEnvironmentalWeapon::OnMeleeEndDamagePhaseEvent()
{
	// Don't care about any collisions for a while
	if(g_pGame->GetIGameFramework()->GetClientActorId() == m_OwnerId)
	{
		GetGameObject()->EnablePhysicsEvent(false, eEPE_OnCollisionLogged);

		pe_params_flags pf; 
		pf.flagsAND = ~ref_small_and_fast;
		GetEntity()->GetPhysics()->SetParams(&pf);
		m_logicFlags &= ~ELogicFlag_damagePhaseActive; 

		// LOGGING
#ifndef _RELEASE
		if(g_pGameCVars->pl_pickAndThrow.environmentalWeaponImpactDebugEnabled)
		{
			CryLog("[ENV-WEAPON]: End Damage event received" );
		}
#endif //#ifndef _RELEASE

	}
}

// If we ever have melee objects with damage zone helpers AND different health states(using diff models)
// Then we will have to recache damage zones when the model is changed. Currently don't have to worry about this...
// but better to be safe than sorry!
void CEnvironmentalWeapon::OnObjectPhysicsPropertiesChanged()
{
	// Setup Damage zone helper (if one even exists.. not *required*.. m_pDamageZoneHelper can validly be NULL e.g shields)
	InitDamageZoneHelper(); 

	// If the model has changed.. need to regrab this
	RefreshObjectVelocityTracker(true);
}

void CEnvironmentalWeapon::RefreshObjectVelocityTracker(const bool bAllowFallbackHelper)
{
	// Setup Object velocity tracker
	EntityId id							= GetEntityId(); 
	int slot							  = PickAndThrow::FindActiveSlot(id);
	m_pVelocityTracker			= PickAndThrow::FindHelperObject("velocity_tracker",id,slot);

	// If don't have one of these, for now we will *try* to just fall back onto the damage zone helper
	if(!m_pVelocityTracker && bAllowFallbackHelper)
	{
		m_pVelocityTracker				= PickAndThrow::FindHelperObject("damage_zone_helper",id,slot);
		CryLog("[ENV-WEAPON]: unable to locate valid velocity_tracker helper for entity ID[%d] - attempting to fall back on 'damage_zone_helper' ",id);
	}

#ifndef _RELEASE
	if(!m_pVelocityTracker && g_pGameCVars->pl_pickAndThrow.environmentalWeaponImpactDebugEnabled)
	{
		CryLog("[ENV-WEAPON]: unable to locate valid velocity_tracker helper for entity ID[%d]",id);
	}
#endif
}

void CEnvironmentalWeapon::PostInitClient( int channelId )
{
	if (g_pGame->GetHostMigrationState() != CGame::eHMS_NotMigrating)
	{
		return;
	}

	// Invoke RMI on client to ensure weapon state setup correctly
	InitWeaponStateParams params;
	params.m_ownerId = m_OwnerId;
	static_assert(EWS_Count < 128, "EWS_Count should be smaller than 128.");
	params.m_weaponState = static_cast<int8>(m_State); 
	GetGameObject()->InvokeRMIWithDependentObject(ClInitWeaponState(), params, eRMI_ToClientChannel,GetEntityId(),channelId);

	// server needs to wake up weapons on the ground to ensure that their physics position is synced correctly
	if(m_State == EWS_OnGround)
	{
		if( IPhysicalEntity* pPE = GetEntity()->GetPhysics())
		{
			pe_action_awake action_awake;
			action_awake.bAwake = true;
			action_awake.minAwakeTime = 1.f;
			pPE->Action(&action_awake);
		}
	}
}

IMPLEMENT_RMI(CEnvironmentalWeapon, ClInitWeaponState)
{
	if(m_OwnerId) //Host migration case - the player has dropped their weapon during host migration
	{
		CPlayer* pPlayer = static_cast<CPlayer*>(g_pGame->GetIGameFramework()->GetIActorSystem()->GetActor(m_OwnerId));
		if(pPlayer)
		{
			pPlayer->ExitPickAndThrow(true);
		}
	}

	m_OwnerId = params.m_ownerId; 
	m_State   = static_cast<E_WeaponState>(params.m_weaponState); 
	m_logicFlags |= ELogicFlag_doInitWeaponState;
	return true;
}

// When anybody drops/picks up an environmental weapon, we need to look at delegating authority
void CEnvironmentalWeapon::DelegateAuthorityOnOwnershipChanged(EntityId prevOwnerId, EntityId newOwnerId)
{
	// Handle delegating authority for improved fidelity for the player doing the attacking
	if(gEnv->bServer && prevOwnerId != newOwnerId)
	{
		INetContext* pNetContext = g_pGame->GetIGameFramework()->GetNetContext();
		if (!pNetContext)
		{
			return;
		}
 
		if(newOwnerId)
		{
			// If new owner is a remote player
			IActor* pActor = g_pGame->GetIGameFramework()->GetIActorSystem()->GetActor(newOwnerId);
			CRY_ASSERT_MESSAGE(pActor, "CEnvironmentalWeapon::DelegateAuthorityOnOwnershipChanged < Something has gone wrong here - perhaps trying to hand ownership to a player that has left the game?"); 
			INetChannel *pNetChannel = gEnv->pGameFramework->GetNetChannel(pActor->GetChannelId());
			if (pActor && pNetChannel)
			{
				// If somebody picked it up that wasn't the server.. hand over control. 
				pNetContext->DelegateAuthority(GetEntityId(), pNetChannel);
			}
		}
	}	
}

void CEnvironmentalWeapon::RegisterListener( IEnvironmentalWeaponEventListener* pListener )
{
#ifndef _RELEASE
	TListenerList::iterator iter = std::find(m_eventListeners.begin(), m_eventListeners.end(), pListener);
	CRY_ASSERT_MESSAGE(iter == m_eventListeners.end(), "CEnvironmentalWeapon::RegisterListener - this listener has already been registered!");
#endif

	CRY_ASSERT_MESSAGE(m_eventListeners.size() < s_maxListeners, "CEnvironmentalWeapon::RegisterListener < ERROR - adding more listeners than supported. Should increase listener container size - Let JONB know");
	
	if(m_eventListeners.size() < s_maxListeners)
	{
		m_eventListeners.push_back(pListener); 
	}
}

void CEnvironmentalWeapon::UnregisterListener( IEnvironmentalWeaponEventListener* pListener )
{
	TListenerList::iterator iter = std::find(m_eventListeners.begin(), m_eventListeners.end(), pListener);
	CRY_ASSERT_MESSAGE(iter != m_eventListeners.end(), "CEnvironmentalWeapon::UnregisterListener - this listener is not currently registered!");

	if(iter != m_eventListeners.end())
	{
		m_eventListeners.removeAt(static_cast<uint32>(iter - m_eventListeners.begin())); 
	}
}

void CEnvironmentalWeapon::OnGrabStart()
{
	IEntity* pEntity = GetEntity(); 
	IScriptTable* pScript = pEntity->GetScriptTable(); 
	if( pScript )
	{
		bool rooted = false; 
		pScript->GetValue("IsRooted", rooted);

		if(rooted)
		{
			// If we are currently rooted, lets report this to any listeners, else don't worry about it
			m_logicFlags |= ELogicFlag_beingRippedUp; 

			// Inform listeners
			const TListenerList::iterator endIter = m_eventListeners.end(); 
			for( TListenerList::iterator iter = m_eventListeners.begin(); iter != endIter; ++iter)
			{
				(*iter)->OnRipStart(); 
			}
		}
	}
}

void CEnvironmentalWeapon::OnGrabEnd()
{
	// If we WERE rooted, lets report this to any listeners, else don't worry about it
	if( m_logicFlags & ELogicFlag_beingRippedUp)
	{
		m_logicFlags &= ~ELogicFlag_beingRippedUp;

		// Inform listeners
		const TListenerList::iterator endIter = m_eventListeners.end(); 
		for( TListenerList::iterator iter = m_eventListeners.begin(); iter != endIter; ++iter)
		{
			(*iter)->OnRipEnd(); 
		}
	}
}

void CEnvironmentalWeapon::OnAttached()
{
	EntityScripts::CallScriptFunction(GetEntity(), GetEntity()->GetScriptTable(), "OnAttached");

	//The lua function above calls Physicalize so now we need to set the no-squash flag
	IPhysicalEntity *pPhysics = GetEntity()->GetPhysics();
	if(pPhysics)
	{
		pe_params_flags paramFlags; 
		paramFlags.flagsOR  = pef_cannot_squash_players; 
		pPhysics->SetParams(&paramFlags);
	}
}

void CEnvironmentalWeapon::OnDetachedFrom( const CActor* pOwner, const bool chargedThrow )
{
	const bool isOwnerLocalClient = (pOwner != NULL) && pOwner->IsClient();
	if(isOwnerLocalClient)
	{
		PerformGroundClearanceAdjust( pOwner, chargedThrow );
		PerformRearClearanceAdjust( pOwner, chargedThrow );
	}
}

void CEnvironmentalWeapon::CloakSyncWithEntity( const EntityId masterEntityId, const bool forceDecloak )
{
	IF_UNLIKELY(g_pGameCVars->pl_pickAndThrow.cloakedEnvironmentalWeaponsAllowed == 0)
		return;

	const EntityEffects::Cloak::CloakSyncParams cloakParams( masterEntityId, GetEntityId(), true, forceDecloak );
	const bool bObjCloaked = EntityEffects::Cloak::CloakSyncEntities( cloakParams ); 

	CRecordingSystem *pRecordingSystem = g_pGame->GetRecordingSystem();
	if (pRecordingSystem)
	{
		pRecordingSystem->OnObjectCloakSync( cloakParams.cloakSlaveId, cloakParams.cloakMasterId, bObjCloaked, true);
	}

	const int children = GetEntity()->GetChildCount();
	for(int i = 0; i < children; ++i)
	{
		const IEntity* pChild = GetEntity()->GetChild(i);

		const EntityId childId = pChild->GetId();
		const EntityEffects::Cloak::CloakSyncParams childCloakParams( masterEntityId, childId, true, forceDecloak );
		const bool bChildCloaked = EntityEffects::Cloak::CloakSyncEntities( childCloakParams );
		if(pRecordingSystem)
		{
			pRecordingSystem->OnObjectCloakSync( childCloakParams.cloakSlaveId, childCloakParams.cloakMasterId, bChildCloaked, true);
		}
	}
}

void CEnvironmentalWeapon::OnRipDetachEvent()
{
	// Inform listeners
	const TListenerList::iterator endIter = m_eventListeners.end(); 
	for( TListenerList::iterator iter = m_eventListeners.begin(); iter != endIter; ++iter)
	{
		(*iter)->OnRipDetach(); 
	}
}

void CEnvironmentalWeapon::SetVehicleDamageParams( float meleeDamage, float throwDamage, float throwMinVelocity, float throwMaxVelocity )
{
	m_vehicleMeleeDamage = meleeDamage;
	m_vehicleThrowDamage = throwDamage;
	m_vehicleThrowMinVelocity = throwMinVelocity;
	m_vehicleThrowMaxVelocity = throwMaxVelocity;
}

void CEnvironmentalWeapon::PerformWeaponSweepTracking()
{
	// If not in damage phase yet, nothing to do!
	if( (m_logicFlags & ELogicFlag_damagePhaseActive) 
#if ALLOW_SWEEP_DEBUGGING
		|| s_bForceSweepTest
#endif 
		)
	{
		if(!(m_logicFlags & ELogicFlag_damageZoneHelperInitialised))
		{
			InitDamageZoneHelper(); 
		}

		// get the pos of weapon tracker point + weapon dmg zone point
		if(m_pVelocityTracker)
		{
			// End pos
			const Matrix34& wMat = GetEntity()->GetWorldTM(); 
			Vec3 weaponEndPos = (wMat * m_pVelocityTracker->tm).GetTranslation();
		
			Vec3 weaponStartPos(0.0f,0.0f,0.0f);
			if(m_pDamageZoneHelper)
			{
				weaponStartPos = (wMat * m_pDamageZoneHelper->tm).GetTranslation();
			}
			else
			{
				// Should really have an m_pDamageZoneHelper.. but for the sake of debugging.. or if the artist hasn't placed one (needs adding if not..)
				IActor* pOwnerActor = gEnv->pGameFramework->GetIActorSystem()->GetActor(m_OwnerId);
				if(pOwnerActor)
				{
					weaponStartPos = pOwnerActor->GetEntity()->GetWorldPos(); 
				}
			}

			// Test if in accepted part of FOV for collision processing
			const CCamera& cam			  = gEnv->p3DEngine->GetRenderingCamera();
			const Vec3& ownerPosition     = cam.GetPosition();
			Vec3 attackerFacingdir		  = cam.GetViewdir();
			attackerFacingdir			  = attackerFacingdir.normalize(); 
			float cone					  = DEG2RAD(g_pGameCVars->pl_pickAndThrow.environmentalWeaponHitConeInDegrees); 

			const bool bDamagePhaseActive = (m_logicFlags & ELogicFlag_damagePhaseActive)>0;
			const bool bVisible = LocationWithinFrontalCone(ownerPosition, weaponStartPos, attackerFacingdir,cone) ||
														LocationWithinFrontalCone(ownerPosition, weaponEndPos, attackerFacingdir,cone);

			SWeaponTrackSlice newSlice( weaponStartPos, weaponEndPos-weaponStartPos, bDamagePhaseActive, bVisible );
	
			// If in valid FOV... and have results from previous frame ... do beam tests
			if((m_logicFlags & ELogicFlag_weaponSweepTestsValid) && newSlice.m_bVisible && m_prevFrameWeaponTrackSlice.m_bDamagePhaseActive && newSlice.m_bDamagePhaseActive)
			{
				// Perform swept sphere test
				PerformWeaponSweepTest(m_prevFrameWeaponTrackSlice, newSlice, ownerPosition); 
			}

			// As soon as we have 1 cached result, we can begin swept sphere tests from prev -> new
			m_prevFrameWeaponTrackSlice = newSlice; 
			m_logicFlags |= ELogicFlag_weaponSweepTestsValid;

	#if ALLOW_SWEEP_DEBUGGING
			// track all points when debugging for rendering.  
			m_weaponSwingTrackedPoints.push_back(newSlice); 
	#endif //#if ALLOW_SWEEP_DEBUGGING

		
		}
	}
}


void CEnvironmentalWeapon::PerformWeaponSweepTest( const SWeaponTrackSlice& prevSlice, SWeaponTrackSlice& currSlice, const Vec3& eyePos )
{
	// No invalid length slices.
	if(fabs_tpl(prevSlice.m_invLength*currSlice.m_invLength)<FLT_EPSILON)
		return;

	IEntity* pEnt = GetEntity();
	IPhysicalEntity* pPhysEnt = pEnt->GetPhysics();
	if(!pPhysEnt)
		return;



	// Result holders
	IPhysicalEntity *pEnts[sMaxEntitiesToConsiderPerSweep];
	IPhysicalEntity **ppEnts = pEnts;
	ray_hit se_hit;

	IPhysicalWorld* pPhysicsWorld = gEnv->pPhysicalWorld;
	
	const Vec3 wPos = pEnt->GetWorldPos();
	if(int numEntities = gEnv->pPhysicalWorld->GetEntitiesInBox(wPos- s_radiusAdjust, wPos + s_radiusAdjust, ppEnts, ent_living, sMaxEntitiesToConsiderPerSweep))
	{
		// Calc the sweep test values.
		Vec3 vSweepTests[SWeaponTrackSlice::k_numTrackPoints][2];
		{
			for(int sweep=0; sweep<SWeaponTrackSlice::k_numTrackPoints; ++sweep)
			{
				vSweepTests[sweep][0] = prevSlice.m_start + (prevSlice.m_dir*prevSlice.m_fSteps[sweep]);
				vSweepTests[sweep][1] = (currSlice.m_start + (currSlice.m_dir*currSlice.m_fSteps[sweep])) - vSweepTests[sweep][0];
			}
		}

		for(int i = 0; i < numEntities; ++i)
		{
			IPhysicalEntity* pTestPhysEnt = ppEnts[i];
			IEntity* pVictimEntity = gEnv->pEntitySystem->GetEntityFromPhysics(pTestPhysEnt);
			if(!pVictimEntity || pVictimEntity->GetId() != m_OwnerId)
			{
				// Generate N swept sphere tests along the N lines that run parallel to our swing direction between previous 'slice' (snapshot of weapon orientation) and current
				for(int sweep=0; sweep<SWeaponTrackSlice::k_numTrackPoints; ++sweep)
				{
					if( pPhysicsWorld->CollideEntityWithBeam(pTestPhysEnt, vSweepTests[sweep][0], vSweepTests[sweep][1], SWeaponTrackSlice::k_fBeamRad, &se_hit) )
					{
						// Existing functionality consumes EventPhysCollision structs, fill in only required data
						EventPhysCollision collisionEvent;
						collisionEvent.pEntity[0] = pPhysEnt;     // Us 
						collisionEvent.pEntity[1] = pTestPhysEnt; // Victim
						collisionEvent.pt		  = se_hit.pt;
						collisionEvent.n		  = se_hit.n;
						collisionEvent.idmat[1]   = se_hit.idmatOrg;
						collisionEvent.partid[1]  = se_hit.partid;

						HandleMeleeCollisionEvent(&collisionEvent, true);
						break; // Only hit each entity with the one of the sweep tests no point in checking the rest.
					}		
				}
			}
		}
	}
}

#if ALLOW_SWEEP_DEBUGGING
void CEnvironmentalWeapon::RenderDebugWeaponSweep() const
{
	IRenderAuxGeom* pAuxGeom = gEnv->pRenderer->GetIRenderAuxGeom();

	SAuxGeomRenderFlags originalFlags = pAuxGeom->GetRenderFlags();
	SAuxGeomRenderFlags newFlags	  = originalFlags; 
	newFlags.SetCullMode(e_CullModeNone); 
	newFlags.SetDepthWriteFlag(e_DepthWriteOff); 
	newFlags.SetAlphaBlendMode(e_AlphaBlended); 
	newFlags.SetDepthTestFlag(e_DepthTestOff);
	pAuxGeom->SetRenderFlags(newFlags);

	const ColorB red(255,0,0,255);
	const ColorB green(0,255,0,255);
	const ColorB black(0,0,0,255); 
	const ColorB yellow(125,125,0,255); 
	const ColorB white(255,255,255,255); 

	// Fade colours over t0 - t1 in attack anim so we can easily identify start/end
	const float minLightVal = 0.2f; 
	const float lightRange  = 1.0f - minLightVal; 

	const static uint numTrackedSlices  = m_weaponSwingTrackedPoints.size(); 
	const SWeaponTrackSlice* pPrevSlice = NULL;
	const float numSlices = (float)(m_weaponSwingTrackedPoints.size());

	Vec3 vSweeps[SWeaponTrackSlice::k_numTrackPoints][2];

	for(int i = 0; i < numSlices; ++i)
	{
		const SWeaponTrackSlice& currSlice = m_weaponSwingTrackedPoints[i]; 
		const int iCurr = i&1;
		for(int j = 0; j < SWeaponTrackSlice::k_numTrackPoints; ++j)
		{
			vSweeps[j][iCurr] = currSlice.m_start + (currSlice.m_dir*currSlice.m_fSteps[j]);
		}

		if(pPrevSlice)
		{
			const int iPrev = 1-iCurr;

			// render joining lines
			const float tVal	 = static_cast<float>(i) / numSlices;

			// Help cut out noise when debugging
			const static float minDesiredTValDisplay = 0.0f; 
			const static float maxDesiredTValDisplay = 1.0f; 
			if(tVal < minDesiredTValDisplay ||
				tVal > maxDesiredTValDisplay )
			{
				continue; 
			}

			const float light    	  = lightRange * tVal;
			ColorB activeColor	 	  = pPrevSlice->m_bDamagePhaseActive ? ((pPrevSlice->m_bVisible || currSlice.m_bVisible) ? green : yellow) : red;
			const float finalLightVal = minLightVal + light; 
			ColorB activeColorFaded   = ColorB((uint8)((float)(activeColor.r) * finalLightVal),(uint8)((float)(activeColor.g) * finalLightVal),(uint8)((float)(activeColor.b) * finalLightVal),255); 

			for(int j = 0; j < SWeaponTrackSlice::k_numTrackPoints; ++j)
			{
				// render step lines
				pAuxGeom->DrawLine(vSweeps[j][iPrev], activeColor, vSweeps[j][iCurr], activeColor, 1.0f);
				
				// optionally render tris
				if(s_bRenderTris)
				{
					const int k = j-1;
					if(k>=0 && k<SWeaponTrackSlice::k_numTrackPoints)
					{
						static const uint8 alpha = 30;
						activeColorFaded.a = alpha; 
						// Render quad from two tris
						pAuxGeom->DrawTriangle(vSweeps[j][iPrev],activeColorFaded, vSweeps[j][iCurr], activeColorFaded, vSweeps[k][iCurr],activeColorFaded);
						pAuxGeom->DrawTriangle(vSweeps[k][iPrev],activeColorFaded, vSweeps[k][iCurr], activeColorFaded, vSweeps[j][iPrev],activeColorFaded);
					}
				}			
			}
		}

		// Optionally render frame stamps
		if(s_bRenderFrameStamps)
		{	
			static uint8 dotSize = 8; 
			pAuxGeom->DrawPoint(vSweeps[SWeaponTrackSlice::k_numTrackPoints-1][iCurr],white,dotSize);
			pAuxGeom->DrawLine(vSweeps[0][iCurr], white, vSweeps[SWeaponTrackSlice::k_numTrackPoints-1][iCurr], white, 1.0f);
		}

		pPrevSlice = &currSlice; 
	}

	pAuxGeom->SetRenderFlags(originalFlags);
}

#endif //#if ALLOW_SWEEP_DEBUGGING

bool CEnvironmentalWeapon::IsRooted() const
{
	bool bRooted = true; 
	IScriptTable* pScriptTable = GetEntity()->GetScriptTable();
	if(pScriptTable)
	{
		pScriptTable->GetValue( "IsRooted", bRooted );
	}
	return bRooted; 
}

void CEnvironmentalWeapon:: EntityRevived(EntityId entityId)
{
	if(gEnv->IsClient() && gEnv->pGameFramework->GetClientActorId() == entityId)
	{
		AttemptToRegisterInteractiveEntity(); 
	}
}

const char* CEnvironmentalWeapon::GetClassificationName() const
{
	const char* pClassificationName = NULL;
	if( IScriptTable* pScriptTable = GetEntity()->GetScriptTable() )
	{
		SmartScriptTable propertiesTable;
		if( pScriptTable->GetValue( "Properties", propertiesTable ) )
		{
			propertiesTable->GetValue( "classification", pClassificationName );
		}
	}

	return pClassificationName;
}

bool CEnvironmentalWeapon::CanBeUsedByPlayer( CPlayer* pPlayer ) const
{
	if(g_pGameCVars->g_mpNoEnvironmentalWeapons)
	{
		return false;
	}

	if(pPlayer->IsStillWaitingOnServerUseResponse())
	{
		return false;
	}

	if( pPlayer->IsSliding() || pPlayer->IsExitingSlide() || pPlayer->IsSwimming() || pPlayer->IsOnLedge())
	{
		return false;
	}

	if( IsRooted() )
	{
		if( pPlayer->IsInAir() || !pPlayer->IsOnGround() )
		{
			return false;
		}

		//If we're not standing, make sure there is room to stand
		if(pPlayer->GetStance() != STANCE_STAND)
		{
			if(!pPlayer->TrySetStance(STANCE_STAND))
			{
				return false;
			}
		}

		if(m_rootedAngleDotMin>-1.f || m_rootedAngleDotMax<1.f) //Check facing dir of player
		{
			Vec3 playerFacingDir = pPlayer->GetEntity()->GetWorldTM().GetColumn1();
			playerFacingDir.z = 0.f;
			playerFacingDir.NormalizeSafe(Vec3(1.f,0.f,0.f));
			Vec3 envWeaponFacingDir = GetEntity()->GetWorldTM().GetColumn1();
			envWeaponFacingDir.z = 0.f;
			envWeaponFacingDir.NormalizeSafe(Vec3(-1.f,0.f,0.f));

			const float dot = playerFacingDir.Dot(envWeaponFacingDir);
			if( dot<m_rootedAngleDotMin || dot>m_rootedAngleDotMax)
			{
				return false;
			}
		}
	}

	return true;
}

void CEnvironmentalWeapon::CheckAchievements(IActor* pKillerActor)
{
	CGameRules *pGameRules(NULL);
	if(pKillerActor && pKillerActor->IsClient() && (pGameRules = g_pGame->GetGameRules()) && pGameRules->GetGameMode() == eGM_PowerStruggle)
	{
		const char* pGrabTagOverride = NULL;
		if( EntityScripts::GetEntityProperty(GetEntity(),"szGrabAnimTagOverride", pGrabTagOverride) && strcmpi(pGrabTagOverride, "spear") == 0)
		{
			g_pGame->GetPersistantStats()->IncrementClientStats(EIPS_SpearShieldKills, 1);

			CGameLobby *pGameLobby = g_pGame->GetGameLobby();
			if(m_throwerId == 0 && pGameLobby && !pGameLobby->IsPrivateGame())
			{
				g_pGame->GetGameAchievements()->GiveAchievement(eC3A_MP_Hit_Me_Baby_One_More_Time);
			}
		}
	}
	else if(m_throwerId)
	{
		CGameLobby *pGameLobby = g_pGame->GetGameLobby();
		if(m_bClientHasKilledWithThrow && pGameLobby && !pGameLobby->IsPrivateGame())
		{
			g_pGame->GetGameAchievements()->GiveAchievement(eC3A_MP_Odd_Job);
		}
		else
		{
			m_bClientHasKilledWithThrow = true;
		}
	}
}

void CEnvironmentalWeapon::OnHostMigration( const Quat& rotation, const Vec3& position, const Vec3& velocity )
{
	if(!position.IsZero()) //If non-zero, the most recent owner has dropped/thrown their weapon, and we need to synch its movement
	{
		GetEntity()->SetRotation(rotation);
		GetEntity()->SetPos(position);

		if(IPhysicalEntity* pPhysics = GetEntity()->GetPhysics())
		{
			pe_action_set_velocity action_set_velocity;
			action_set_velocity.v = velocity;
			pPhysics->Action(&action_set_velocity);
		}
	}

	InitWeaponStateParams params;
	params.m_ownerId = m_OwnerId;
	static_assert(EWS_Count < 128, "EWS_Count should be smaller than 128.");
	params.m_weaponState = static_cast<int8>(m_State); 
	GetGameObject()->InvokeRMIWithDependentObject(ClInitWeaponState(), params, eRMI_ToRemoteClients, GetEntityId());
}
