// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "Turret.h"

#include "ScriptBind_Turret.h"

#include <ICryMannequin.h>
#include <CryAnimation/IAttachment.h>

#include "AnimationActionTurret.h"

#include "ProceduralContextTurretAimPose.h"

#include <IItemSystem.h>

#include "TurretHelpers.h"
using namespace TurretHelpers;

#include "State.h"
#include "TurretBehaviorEvents.h"

#include <CryAISystem/VisionMapTypes.h>
#include <CryAISystem/IAIObject.h>
#include <CryAISystem/IAIActor.h>
#include <CryAISystem/IAIObjectManager.h>

#include <IVehicleSystem.h>

#include "TacticalManager.h"


#include "BodyManager.h"
#include "BodyDamage.h"
#include "BodyDestruction.h"

#include "Laser.h"
#include "ItemSharedParams.h"

#include "GameRules.h"
#include "PersistantStats.h"

#include "TurretSoundManager.h"

#include <CryAISystem/ITargetTrackManager.h>
#include "Weapon.h"

#include "AI/GameAISystem.h"

#include "Actor.h"


static const char* const FALLBACK_TURRET_MODEL_NAME = "";
static const char* const FALLBACK_TURRET_PRIMARY_WEAPON_CLASS_NAME = "Turret_Gun";
static const char* const FALLBACK_TURRET_PRIMARY_WEAPON_JOINT_NAME = "weaponjoint";
static const float FALLBACK_TURRET_MAX_HEALTH = 1000.f;
static const char* const FALLBACK_TURRET_FACTION_NAME = "";
static const float FALLBACK_TURRET_RADAR_RANGE = 10.0f;
static const float FALLBACK_TURRET_RADAR_FOV_DEGREES = 180.0f;
static const float FALLBACK_TURRET_RADAR_VERTICAL_OFFSET = 2.0f;
static const float FALLBACK_TURRET_EYE_RANGE = 20.0f;
static const float FALLBACK_TURRET_EYE_FOV_DEGREES = 25.0f;
static const char* const FALLBACK_TURRET_EYE_JOINT_NAME = FALLBACK_TURRET_PRIMARY_WEAPON_JOINT_NAME;
static const char* const FALLBACK_TURRET_CONTROLLER_DEF = "Animations/Mannequin/ADB/turretControllerDefs.xml";
static const char* const FALLBACK_TURRET_ANIMATION_DATABASE = "Animations/Mannequin/ADB/turret.adb";
static const float FALLBACK_TURRET_PRIMARY_WEAPON_FOV_DEGREES = 15.0f;
static const float FALLBACK_TURRET_PRIMARY_WEAPON_RANGE_CHECK_OFFSET = 2.0f;
static const char* const FALLBACK_TURRET_LASER_JOINT_NAME = FALLBACK_TURRET_PRIMARY_WEAPON_JOINT_NAME;
static const Vec3 FALLBACK_TURRET_LASER_LOCAL_OFFSET = Vec3( 0, 0, 0 );
static const float FALLBACK_TURRET_LASER_THICKNESS = 0.8f;
static const bool FALLBACK_TURRET_LASER_SHOW_DOT = true;
static const float FALLBACK_TURRET_LASER_MAX_RANGE = 150.0f;
static const char* const FALLBACK_TURRET_LASER_GEOMETRY = "";
static const char* const FALLBACK_TURRET_LASER_DOT_EFFECT = "";
static const char* const FALLBACK_TURRET_LASER_SOURCE_EFFECT = "";
static const float FALLBACK_TURRET_SOUND_DEGREES_SECOND_THRESHOLD = 0.05f;
static const float FALLBACK_TURRET_CLOAK_DETECTION_DISTANCE = 4.0f;

static const int DEFAULT_TURRET_MODEL_SLOT = 0;
static const char* const DEFAULT_TURRET_PRIMARY_WEAPON_ATTACHMENT_NAME = "PrimaryWeapon";
static const Vec3 DEFAULT_TURRET_AI_OFFSET = Vec3( 0, 0, 1 );
static const float DEFAULT_TURRET_ACCURACY_DISTANCE_MIN = 2.0f;
static const float DEFAULT_TURRET_ACCURACY_DISTANCE_MAX = 4.0f;
static const float DEFAULT_TURRET_ACCURACY_RANGE = ( DEFAULT_TURRET_ACCURACY_DISTANCE_MAX - DEFAULT_TURRET_ACCURACY_DISTANCE_MIN );
static const float DEFAULT_TURRET_UPDATE_DISTANCE_SQUARED = ( FALLBACK_TURRET_EYE_FOV_DEGREES * FALLBACK_TURRET_EYE_FOV_DEGREES );
static const int DEFAULT_TURRET_INVALID_GROUPID = -1;


DEFINE_STATE_MACHINE( CTurret, Behaviour );



#define MAN_TURRET_FRAGMENTS( x )

#define MAN_TURRET_TAGS( x ) \
	x( PrimaryWeapon_Idle ) \
	x( PrimaryWeapon_Firing )

#define MAN_TURRET_TAGGROUPS( x ) \
	x( PrimaryWeaponState )

#define MAN_TURRET_SCOPES( x )

#define MAN_TURRET_CONTEXTS( x )

#define MAN_TURRET_FRAGMENT_TAGS( x )

MANNEQUIN_USER_PARAMS( SMannequinTurretParams, MAN_TURRET_FRAGMENTS, MAN_TURRET_TAGS, MAN_TURRET_TAGGROUPS, MAN_TURRET_SCOPES, MAN_TURRET_CONTEXTS, MAN_TURRET_FRAGMENT_TAGS );

namespace TurretBehaviorStateNames
{
	const char* g_turretStateNames[ eTurretBehaviorState_Count ] =
	{
		"Undeployed",
		"PartiallyDeployed",
		"Deployed",
		"Dead",
	};

	const char** GetNames()
	{
		return &g_turretStateNames[ 0 ];
	}

	ETurretBehaviorState FindId( const char* const name )
	{
		static_assert( eTurretBehaviorState_Count == 4, "Mismatch between g_turretStateNames and ETurretBehaviorState" );
		static_assert( eTurretBehaviorState_Undeployed == 0, "Mismatch between g_turretStateNames and ETurretBehaviorState" );
		static_assert( eTurretBehaviorState_PartiallyDeployed == 1, "Mismatch between g_turretStateNames and ETurretBehaviorState" );
		static_assert( eTurretBehaviorState_Deployed == 2, "Mismatch between g_turretStateNames and ETurretBehaviorState" );
		static_assert( eTurretBehaviorState_Dead == 3, "Mismatch between g_turretStateNames and ETurretBehaviorState" );

		for ( size_t i = 0; i < eTurretBehaviorState_Count; ++i )
		{
			const char* const stateName = g_turretStateNames[ i ];
			if ( strcmp( stateName, name ) == 0 )
			{
				return static_cast< ETurretBehaviorState >( i );
			}
		}

		return eTurretBehaviorState_Invalid;
	}
}



struct STurretSerializationInfo
{
	float health;
	float maxHealth;
	int stateId;
	uint8 factionId;
	uint8 factionIdWhenAiRemoved;

	STurretSerializationInfo()
		: health( FALLBACK_TURRET_MAX_HEALTH )
		, maxHealth( FALLBACK_TURRET_MAX_HEALTH )
		, stateId( eTurretBehaviorState_Invalid )
		, factionId( IFactionMap::InvalidFactionID )
		, factionIdWhenAiRemoved( IFactionMap::InvalidFactionID )
	{
	}

	void Serialize( TSerialize serializer )
	{
		serializer.Value( "Health", health );
		serializer.Value( "MaxHealth", maxHealth );
		serializer.Value( "StateId", stateId );
		serializer.Value( "FactionId", factionId );
		serializer.Value( "FactionIdWhenAiRemoved", factionIdWhenAiRemoved );
	}
};


CTurret::CTurret()
: m_pActionController(nullptr)
, m_pAnimationContext(nullptr)
, m_pAimProceduralContext(nullptr)
, m_pUserParams(nullptr)
, m_primaryWeaponId(0)
, m_primaryWeaponFovCos(cos(DEG2RAD(FALLBACK_TURRET_PRIMARY_WEAPON_FOV_DEGREES)))
, m_primaryWeaponRangeCheckOffset(FALLBACK_TURRET_PRIMARY_WEAPON_RANGE_CHECK_OFFSET)
, m_laserBeamOffset(FALLBACK_TURRET_LASER_LOCAL_OFFSET)
, m_laserSourceEffectSlot(-1)
, m_laserJointId(-1)
, m_forcedVisibleTargetEntityId(0)
, m_allowFire(true)
, m_registeredAsKillListener(false)
, m_targetEntityId(0)
, m_targetWorldPosition(ZERO)
, m_visionRadarOffset(0.0f)
, m_validVisbleTargetEntityId(0)
, m_eyeJointId(-1)
, m_eyeVisionRangeParameter(FALLBACK_TURRET_EYE_RANGE)
, m_stateId(eTurretBehaviorState_Invalid)
, m_cloakDetectionDistance(FALLBACK_TURRET_CLOAK_DETECTION_DISTANCE)
, m_updateDistanceSquared(DEFAULT_TURRET_UPDATE_DISTANCE_SQUARED)
, m_registeredInAutoAimSystem(false)
, m_factionWhenAiRepresentationRemoved(IFactionMap::InvalidFactionID)
, m_factionOld(IFactionMap::InvalidFactionID)
{
	m_trackedTargets.reserve(1);
}


CTurret::~CTurret()
{
	UnRegisterAsKillListener();

	RemoveWeapons();

	const IGameObject* const pGameObject = GetGameObject();
	if ( pGameObject != NULL )
	{
		CScriptBind_Turret* const pScriptBindTurret = GetScriptBindTurret();
		assert( pScriptBindTurret != NULL );
		pScriptBindTurret->DettachFrom( this );

		RemoveFromTacticalManager();
		UnRegisterFromAutoAimSystem();

		RemoveAiRepresentation();
	}
	RemoveVision();

	SAFE_RELEASE( m_pActionController );
	SAFE_DELETE( m_pAnimationContext );

	StateMachineReleaseBehaviour();

	UnRegisterFromAwarenessToPlayerHelper();
}


bool CTurret::Init( IGameObject* pGameObject )
{
	SetGameObject( pGameObject );

	StateMachineInitBehaviour();

	CScriptBind_Turret* const pScriptBindTurret = GetScriptBindTurret();
	assert( pScriptBindTurret != NULL );
	pScriptBindTurret->AttachTo( this );

	CacheResources();

	RegisterInAwarenessToPlayerHelper();

	return true;
}


void CTurret::Release()
{
	delete this;
}


void CTurret::CacheResources()
{
	CacheBodyDamageResources();
}


void CTurret::InitClient( int channelId )
{

}


void CTurret::PostInit( IGameObject* pGameObject )
{
	const bool runningUnderEditor = gEnv->IsEditor();
	const bool enteringGameMode = ( ! runningUnderEditor );
	Reset( enteringGameMode );
}


void CTurret::PostInitClient( int channelId )
{

}


bool CTurret::ReloadExtension( IGameObject* pGameObject, const SEntitySpawnParams& params )
{
	return false;
}


void CTurret::PostReloadExtension( IGameObject* pGameObject, const SEntitySpawnParams& params )
{
	CScriptBind_Turret* const pScriptBindTurret = GetScriptBindTurret();
	assert( pScriptBindTurret != NULL );
	pScriptBindTurret->AttachTo( this );
}


bool CTurret::GetEntityPoolSignature( TSerialize signature )
{
	signature.BeginGroup( "Turret" );
	signature.EndGroup();

	return true;
}


void CTurret::FullSerialize( TSerialize serializer )
{
	// Loading is moved to PostSerialize due to the fact that we want a minimal serialization pattern here,
	// meaning starting from a clean entity, and just adding the necessary delta to get to the desired state
	// but ai system serialization resets ai objects after FullSerialize calls on entities, so we cannot call
	// reset on the entity here and add the delta... we're doing that on PostSerialize.
	// If the way the object manager serialization changes we can move everything to this function and get rid
	// of m_pSerializationInfo completely.
	m_pSerializationInfo.reset( new STurretSerializationInfo() );

	m_pSerializationInfo->health = GetHealth();
	m_pSerializationInfo->maxHealth = GetMaxHealth();
	m_pSerializationInfo->factionId = GetFactionId();
	m_pSerializationInfo->stateId = static_cast< int >( GetStateId() );
	m_pSerializationInfo->factionIdWhenAiRemoved = m_factionWhenAiRepresentationRemoved;
	m_pSerializationInfo->Serialize( serializer );

	if ( ! serializer.IsReading() )
	{
		m_pSerializationInfo.reset();
	}
}


bool CTurret::NetSerialize( TSerialize serializer, EEntityAspects aspect, uint8 profile, int flags )
{
	return true;
}


void CTurret::PostSerialize()
{
	if ( m_pSerializationInfo.get() )
	{
		const bool enteringGameMode = true;
		Reset( enteringGameMode );

		m_factionWhenAiRepresentationRemoved = m_pSerializationInfo->factionIdWhenAiRemoved;

		SetMaxHealth( m_pSerializationInfo->maxHealth );
		SetHealth( m_pSerializationInfo->health );
		SetFactionId( m_pSerializationInfo->factionId );

		const ETurretBehaviorState stateId = static_cast< ETurretBehaviorState >( m_pSerializationInfo->stateId );
		SetInitialStateById( stateId );

		UpdateAiLocation();
		UpdateRadarVisionLocation();
		UpdateEyeVisionLocation();

		RegisterInAwarenessToPlayerHelper();
	}

	m_pSerializationInfo.reset();
}


void CTurret::SerializeSpawnInfo( TSerialize serializer )
{

}


ISerializableInfoPtr CTurret::GetSpawnInfo()
{
	return 0;
}


void CTurret::Update( SEntityUpdateContext& context, int updateSlot )
{
	const float frameTimeSeconds = context.fFrameTime;
}


void CTurret::PostUpdate( float frameTimeSeconds )
{
}


void CTurret::PostRemoteSpawn()
{


}


void CTurret::HandleEvent( const SGameObjectEvent& event )
{

}


void CTurret::ProcessEvent( const SEntityEvent& event )
{
	switch ( event.event )
	{
	case ENTITY_EVENT_RESET:
		{
			const bool enteringGameMode = ( event.nParam[ 0 ] == 1 );
			Reset( enteringGameMode );
		}
		break;

	case ENTITY_EVENT_PREPHYSICSUPDATE:
		{
			OnPrePhysicsUpdate();
		}
		break;

	case ENTITY_EVENT_XFORM:
		{
			UpdateAiLocation();
			UpdateRadarVisionLocation();
			UpdateEyeVisionLocation();
		}
		break;

	case ENTITY_EVENT_HIDE:
		{
			UnRegisterFromAutoAimSystem();
			RemoveFromTacticalManager();
			RemoveVision();
			RemoveAiRepresentation();
			RemoveWeapons();
		}
		break;

	case ENTITY_EVENT_UNHIDE:
		{
			const bool isDead = IsDead();
			if ( ! isDead && ! gEnv->pSystem->IsSerializingFile() ) 
			{
				InitWeapons();
				InitAiRepresentation();
				ResetVision();
				AddToTacticalManager();
				RegisterInAutoAimSystem();
			}
		}
		break;

	case ENTITY_EVENT_PRE_SERIALIZE:
		{
			RemoveWeapons();
			RemoveVision();
			RemoveAiRepresentation();
		}
		break;

	case ENTITY_EVENT_SCRIPT_EVENT:
		{
			const char* const eventName = reinterpret_cast< const char* >( event.nParam[ 0 ] );
			ProcessScriptEvent( eventName );
		}
		break;
	}

	if ( m_pSoundManager.get() )
	{
		m_pSoundManager->ProcessEvent( event );
	}
}


void CTurret::SetChannelId( uint16 id )
{


}


void CTurret::GetMemoryUsage( ICrySizer* pSizer ) const
{
	pSizer->Add( *this );
	pSizer->AddContainer( m_trackedTargets );

// 	pSizer->Add( m_pStateMachine );
// 	pSizer->Add( m_pActionController );
// 	pSizer->Add( m_pAnimationContext );
}



void CTurret::Reset( const bool enteringGameMode )
{
	RemoveAiRepresentation();
	m_factionWhenAiRepresentationRemoved = IFactionMap::InvalidFactionID;

	IEntity* const pEntity = GetEntity();

	UnPhysicalize();

	const char* const modelFilename = GetModelName();
	const int loadCharacterStatus = pEntity->LoadCharacter( DEFAULT_TURRET_MODEL_SLOT, modelFilename );

	SetAllowFire( true );

	Physicalize();

	InitActionController();

	InitAiRepresentation(eIARM_RebuildFromScratch);

	if ( enteringGameMode )
	{
		InitWeapons();
		ResetVision();
	}
	else
	{
		RemoveWeapons();
		RemoveVision();
	}

	ResetTarget();
	ResetHealth();

	InitAutoAimParams();
	RegisterInAutoAimSystem();

	AddToTacticalManager();

	CreateBodyDamage();

	ResetBehaviorParams();
	StateMachineResetBehaviour();

	const ETurretBehaviorState initialStateId = GetInitialBehaviorStateId();
	SetInitialStateById( initialStateId );

	IGameObject* const pGameObject = GetGameObject();
	assert( pGameObject );
	pGameObject->EnablePrePhysicsUpdate( ePPU_Always );

	pGameObject->EnableUpdateSlot( this, 0 );
	pGameObject->SetUpdateSlotEnableCondition( this, 0, eUEC_VisibleOrInRange );

	InitTurretSoundManager();
}


void CTurret::InitAiRepresentation( const EInitAiRepresentationMode mode )
{
	RemoveAiRepresentation();

	IEntity* const pEntity = GetEntity();
	const bool isHidden = pEntity->IsHidden();
	IF_UNLIKELY( isHidden )
	{
		return;
	}

	const EntityId entityId = pEntity->GetId();

	AIObjectParams params(AIOBJECT_TARGET, NULL, entityId);
	gEnv->pAISystem->GetAIObjectManager()->CreateAIObject(params);

	uint8 factionId = GetDefaultFactionId();
	if ( mode == eIARM_RestorePreviousState )
	{
		if ( m_factionWhenAiRepresentationRemoved != IFactionMap::InvalidFactionID )
		{
			factionId = m_factionWhenAiRepresentationRemoved;
		}
	}
	SetFactionId( factionId );

	const int groupId = GetEntityInstanceProperty( pEntity, "groupId", DEFAULT_TURRET_INVALID_GROUPID );
	SetGroupId( groupId );

	CreateRateOfDeathHelper();

	UpdateAiLocation();
}


void CTurret::RemoveAiRepresentation()
{
	NotifyAiThatTurretIsRemoved();

	IEntity* const pEntity = GetEntity();

	RemoveRateOfDeathHelper();

	const uint8 factionId = GetFactionId();
	if ( factionId != IFactionMap::InvalidFactionID )
	{
		m_factionWhenAiRepresentationRemoved = factionId;
	}

	SetTargetTrackClassThreat( 0.0f );

	gEnv->pAISystem->GetAIObjectManager()->RemoveObjectByEntityId(GetEntityId());

	m_factionOld = IFactionMap::InvalidFactionID;
}

void CTurret::NotifyAiThatTurretIsRemoved()
{
	// Treating removal as if the turret died since AI already handles that.
	IEntity* const pEntity = GetEntity();
	IAIObject* const pAiObject = pEntity->GetAI();
	gEnv->pAISystem->NotifyTargetDead( pAiObject );
}

void CTurret::UpdateAiLocation()
{
	IEntity* const pEntity = GetEntity();
	IAIObject* const pAiObject = pEntity->GetAI();
	if ( pAiObject == NULL )
	{
		return;
	}

	const Matrix34& worldTM = pEntity->GetWorldTM();

	const Vec3 aiWorldPosition = worldTM.TransformPoint( DEFAULT_TURRET_AI_OFFSET );
	const Vec3& aiWorldDirection = pEntity->GetForwardDir();

	pAiObject->SetPos( aiWorldPosition, aiWorldDirection );
}


void CTurret::Enable()
{
	IEntity* const pEntity = GetEntity();
	pEntity->Hide( false );
}


void CTurret::Disable()
{
	IEntity* const pEntity = GetEntity();
	pEntity->Hide( true );
}


void CTurret::OnPropertyChange()
{
	const bool enteringGameMode = false;
	Reset( enteringGameMode );
}


void CTurret::OnHit( const HitInfo& hitInfo )
{
	const bool hittingMyself = ( hitInfo.weaponId == m_primaryWeaponId );
	IF_UNLIKELY ( hittingMyself )
	{
		return;
	}

	SStateEventHit stateEventHit( &hitInfo );
	StateMachineHandleEventBehaviour( stateEventHit );
}


void CTurret::OnDestroyed()
{
	RemoveAiRepresentation();
}


void CTurret::OnPrePhysicsUpdate()
{
	IEntity* const pEntity = GetEntity();
	
	if (GetGameObject()->GetUpdateSlotEnables(this, 0) == 0)
	{
		const EntityId localPlayerEntityId = g_pGame->GetIGameFramework()->GetClientActorId();
		const IEntity* const pLocalPlayerEntity = gEnv->pEntitySystem->GetEntity( localPlayerEntityId );
		if ( pLocalPlayerEntity )
		{
			const Vec3 playerWorldPos = pLocalPlayerEntity->GetWorldPos();
			const Vec3 turretWorldPos = pEntity->GetWorldPos();
			const float distanceSquared = playerWorldPos.GetSquaredDistance2D( turretWorldPos );
			const bool farAway = ( m_updateDistanceSquared < distanceSquared );
			if ( farAway )
			{
				return;
			}

			GetGameObject()->EnableUpdateSlot(this, 0);
		}
	}

	const float frameTimeSeconds = GetFrameTimeSeconds();

	UpdateVision();

	UpdateLaserBeam( frameTimeSeconds );

	UpdateStateMachine( frameTimeSeconds );

	m_behaviorParams.UpdateTimeNow();
	const SStateEventPrePhysicsUpdate prePhysicsUpdateEvent( frameTimeSeconds );
	StateMachineHandleEventBehaviour( prePhysicsUpdateEvent );

	UpdateRateOfDeathHelper( frameTimeSeconds );
	UpdateBodyDestruction( frameTimeSeconds );

	UpdateActionController( frameTimeSeconds );

	UpdateTurretSoundManager( frameTimeSeconds );

	UpdateWeaponTarget();

	UpdateFactionChange();
}


void CTurret::Physicalize()
{
	IEntity* const pEntity = GetEntity();

	UnPhysicalize();

	SEntityPhysicalizeParams physicsParams;

	physicsParams.nSlot = DEFAULT_TURRET_MODEL_SLOT;
	physicsParams.mass = 0;
	physicsParams.density = 0;
	physicsParams.type = PE_RIGID;

	pEntity->Physicalize( physicsParams );
}


void CTurret::UnPhysicalize()
{
	IEntity* const pEntity = GetEntity();

	SEntityPhysicalizeParams pp;
	pp.type = PE_NONE;
	pEntity->Physicalize( pp );
}


const char* CTurret::GetModelName() const
{
	const IEntity* const pEntity = GetEntity();

	const char* const characterModelName = GetEntityProperty< const char* >( pEntity, "objModel", FALLBACK_TURRET_MODEL_NAME );
	return characterModelName;
}


const char* CTurret::GetPrimaryWeaponClassName() const
{
	const IEntity* const pEntity = GetEntity();
	
	const char* const primaryWeaponClassName = GetEntityProperty< const char* >( pEntity, "PrimaryWeapon", FALLBACK_TURRET_PRIMARY_WEAPON_CLASS_NAME );
	return primaryWeaponClassName;
}


float CTurret::GetPrimaryWeaponFovDegrees() const
{
	const IEntity* const pEntity = GetEntity();

	const float primaryWeaponFovDegrees = GetEntityProperty< float >( pEntity, "PrimaryWeaponFovDegrees", FALLBACK_TURRET_PRIMARY_WEAPON_FOV_DEGREES );
	return primaryWeaponFovDegrees;
}


float CTurret::GetPrimaryWeaponRangeCheckOffset() const
{
	const IEntity* const pEntity = GetEntity();

	const float primaryWeaponRangeCheckOffset = GetEntityProperty< float >( pEntity, "PrimaryWeaponRangeCheckOffset", FALLBACK_TURRET_PRIMARY_WEAPON_RANGE_CHECK_OFFSET );
	return primaryWeaponRangeCheckOffset;
}


const char* CTurret::GetWeaponJointName() const
{
	const IEntity* const pEntity = GetEntity();

	const char* const primaryWeaponJointName = GetEntityProperty< const char* >( pEntity, "PrimaryWeaponJointName", FALLBACK_TURRET_PRIMARY_WEAPON_JOINT_NAME );
	return primaryWeaponJointName;
}


const char* CTurret::GetDefaultFactionName() const
{
	const IEntity* const pEntity = GetEntity();

	const char* const factionName = GetEntityProperty< const char* >( pEntity, "esFaction", FALLBACK_TURRET_FACTION_NAME );
	return factionName;
}


uint8 CTurret::GetDefaultFactionId() const
{
	const IFactionMap& factionMap = gEnv->pAISystem->GetFactionMap();

	const char* const factionName = GetDefaultFactionName();
	const uint8 factionId = factionMap.GetFactionID( factionName );

	return factionId;
}


uint8 CTurret::GetFactionId() const
{
	IEntity* const pEntity = GetEntity();
	const IAIObject* const pAiObject = pEntity->GetAI();
	if ( pAiObject != NULL )
	{
		const uint8 factionId = pAiObject->GetFactionID();
		return factionId;
	}

	return IFactionMap::InvalidFactionID;
}


void CTurret::SetFactionId( const uint8 factionId )
{
	IF_UNLIKELY ( factionId == IFactionMap::InvalidFactionID )
	{
		return;
	}

	IEntity* const pEntity = GetEntity();
	IAIObject* const pAiObject = pEntity->GetAI();
	if ( pAiObject != NULL )
	{
		pAiObject->SetFactionID( factionId );
	}
}



void CTurret::HandleHit( const HitInfo* pHit )
{
	CRY_ASSERT( pHit );

	const float damage = pHit->damage;
	const float oldHealth = GetHealth();

	const EntityId localPlayerEntityId = g_pGame->GetIGameFramework()->GetClientActorId();
	const bool hasBeenHitByPlayer = ( pHit->shooterId == localPlayerEntityId );
	SetHealth( oldHealth - damage, hasBeenHitByPlayer );
	const float newHealth = GetHealth();

	UpdateBodyDestructionHit( *pHit, oldHealth, newHealth );
}


float CTurret::GetHealth() const
{
	return m_health.GetHealth();
}


void CTurret::SetHealth( const float health, const bool hasBeenHitByPlayer /* = false */ )
{
	const bool wasDead = m_health.IsDead();

	m_health.SetHealth( health );

	const bool isDead = m_health.IsDead();

	const bool killed = ( ! wasDead && isDead );
	const bool resurrected = ( wasDead && ! isDead );
	if ( killed )
	{
		RemoveLaserBeam();
		RemoveFromTacticalManager();
		UnRegisterFromAutoAimSystem();
		RemoveAiRepresentation();
		UnRegisterAsKillListener();
		NotifyDestroyed( hasBeenHitByPlayer );
	}
	else if ( resurrected )
	{
		InitAiRepresentation();
		AddToTacticalManager();
		RegisterInAutoAimSystem();
		InitLaserBeam();
	}
}


float CTurret::GetMaxHealth() const
{
	return m_health.GetHealthMax();
}


void CTurret::SetMaxHealth( const float maxHealth )
{
	m_health.SetHealthMax( maxHealth );
}


bool CTurret::IsDead() const
{
	return m_health.IsDead();
}


void CTurret::NotifyDestroyed( const bool hasBeenDestroyedByPlayer /* = false */ ) const
{
	OutputFlowEvent( "Destroyed" );

	if ( IAISystem* pAISystem = gEnv->pAISystem )
	{
		IEntity* const pTurretEntity = GetEntity();
		const float notificationRadius = 50.0f;
		const stack_string signalName = hasBeenDestroyedByPlayer ? "OnTurretHasBeenDestroyedByThePlayer" : "OnTurretHasBeenDestroyed";
		pAISystem->SendAnonymousSignal( 1, signalName.c_str(), pTurretEntity->GetWorldPos(), notificationRadius, pTurretEntity->GetAI() );
	}
}


void CTurret::OutputFlowEvent( const char* const eventName ) const
{
	IEntity* const pEntity = GetEntity();
	CRY_ASSERT( pEntity );

	SEntityEvent event( ENTITY_EVENT_SCRIPT_EVENT );
	event.nParam[ 0 ] = ( INT_PTR )( eventName );
	event.nParam[ 1 ] = IEntityClass::EVT_BOOL;
	bool value = true;
	event.nParam[ 2 ] = ( INT_PTR )( &value );
	pEntity->SendEvent( event );
}


void CTurret::InitActionController()
{
	IMannequin* const pMannequinInterface = GetMannequinInterface();
	assert( pMannequinInterface != NULL );
	
	IAnimationDatabaseManager& animationDatabaseManager = pMannequinInterface->GetAnimationDatabaseManager();

	IEntity* const pEntity = GetEntity();
	SmartScriptTable pMannequinScriptTable = GetMannequinPropertiesTable( pEntity );

	const char* const controllerDefFilename = GetProperty< const char* >( pMannequinScriptTable, "fileControllerDef", FALLBACK_TURRET_CONTROLLER_DEF );
	const SControllerDef* pControllerDef = animationDatabaseManager.LoadControllerDef( controllerDefFilename );
	if ( pControllerDef == NULL )
	{
		return;
	}

	m_pAimProceduralContext = NULL;
	SAFE_RELEASE( m_pActionController );

	SAFE_DELETE( m_pAnimationContext );
	m_pAnimationContext = new SAnimationContext( *pControllerDef );

	m_pActionController = pMannequinInterface->CreateActionController( pEntity, *m_pAnimationContext );

	InitMannequinUserParams();

	const TagID scopeTurret = m_pAnimationContext->controllerDef.m_scopeContexts.Find( "TurretCharacter" );
	ICharacterInstance* const pCharacterInstance = pEntity->GetCharacter( DEFAULT_TURRET_MODEL_SLOT );
	if ( pCharacterInstance == NULL )
	{
		return;
	}

	const char* const animationDatabaseFilename = GetProperty< const char* >( pMannequinScriptTable, "fileAnimationDatabase", FALLBACK_TURRET_ANIMATION_DATABASE );
	const IAnimationDatabase* const pAnimationDatabaseTurret = animationDatabaseManager.Load( animationDatabaseFilename );
	assert( pAnimationDatabaseTurret != NULL );
	m_pActionController->SetScopeContext( scopeTurret, *pEntity, pCharacterInstance, pAnimationDatabaseTurret );

	InitAimProceduralContext();

	StartInitialMannequinActions();
}


void CTurret::ResetActionController()
{
	if ( m_pActionController == NULL )
	{
		return;
	}

	m_pActionController->Reset();
}


void CTurret::UpdateActionController( const float frameTimeSeconds )
{
	if ( m_pActionController == NULL )
	{
		return;
	}

	UpdateMannequinTags( frameTimeSeconds );
	UpdateMannequinParams( frameTimeSeconds );

	m_pActionController->Update( frameTimeSeconds );
}


void CTurret::StartInitialMannequinActions()
{
	if ( m_pActionController == NULL )
	{
		return;
	}

	StartFragmentByName( "PrimaryWeapon" );
}


void CTurret::UpdateMannequinTags( const float frameTimeSeconds )
{
	assert( m_pActionController != NULL );
	assert( m_pAnimationContext != NULL );
	assert( m_pUserParams != NULL );

	bool isFiringPrimaryWeapon = false;
	const IWeapon* const pPrimaryWeapon = GetPrimaryWeapon();
	if ( pPrimaryWeapon != NULL )
	{
		const IFireMode* const pPrimaryWeaponFireMode = pPrimaryWeapon->GetFireMode( 0 );
		if ( pPrimaryWeaponFireMode != NULL )
		{
			isFiringPrimaryWeapon = pPrimaryWeaponFireMode->IsFiring();
		}
	}

	const TagID primaryWeaponStateTag = isFiringPrimaryWeapon ? m_pUserParams->tagIDs.PrimaryWeapon_Firing : m_pUserParams->tagIDs.PrimaryWeapon_Idle;
	m_pAnimationContext->state.SetGroup( m_pUserParams->tagGroupIDs.PrimaryWeaponState, primaryWeaponStateTag );
}


void CTurret::UpdateMannequinParams( const float frameTimeSeconds )
{
	assert( m_pActionController != NULL );

	QuatT targetLocation = QuatT( IDENTITY );
	targetLocation.t = GetTargetWorldPosition();

	m_pActionController->SetParam( "TargetPos", targetLocation );
}


void CTurret::InitMannequinUserParams()
{
	m_pUserParams = NULL;
	if ( m_pAnimationContext == NULL )
	{
		return;
	}

	IMannequin* const pMannequinInterface = GetMannequinInterface();
	assert( pMannequinInterface != NULL );

	CMannequinUserParamsManager& mannequinUserParams = pMannequinInterface->GetMannequinUserParamsManager();

	m_pUserParams = mannequinUserParams.FindOrCreateParams< SMannequinTurretParams >( m_pAnimationContext->controllerDef );
	assert( m_pUserParams );
}


void CTurret::InitAimProceduralContext()
{
	assert( m_pActionController != NULL );
	m_pAimProceduralContext = static_cast< const CProceduralContextTurretAimPose* >( m_pActionController->FindOrCreateProceduralContext(CProceduralContextTurretAimPose::GetCID()) );
}


void CTurret::StartFragmentByName( const char* fragmentName )
{
	assert( fragmentName != NULL );
	assert( fragmentName[ 0 ] != 0 );

	if ( m_pActionController == NULL )
	{
		return;
	}

	const FragmentID fragmentId = m_pAnimationContext->controllerDef.m_fragmentIDs.Find( fragmentName );
	IAction* const pAction = new CAnimationActionTurret( fragmentId );
	m_pActionController->Queue( *pAction );
}


void CTurret::RemoveItemAttachments()
{
	IEntity* const pEntity = GetEntity();
	
	const int slot = 0;
	ICharacterInstance* const pCharacterInstance = pEntity->GetCharacter( slot );
	if ( pCharacterInstance == NULL )
	{
		return;
	}

	IAttachmentManager* const pAttachmentManager = pCharacterInstance->GetIAttachmentManager();
	assert( pAttachmentManager != NULL );

	pAttachmentManager->RemoveAttachmentByName( DEFAULT_TURRET_PRIMARY_WEAPON_ATTACHMENT_NAME );
}


void CTurret::CreateItemAttachments()
{
	IEntity* const pEntity = GetEntity();

	const int slot = 0;
	ICharacterInstance* const pCharacterInstance = pEntity->GetCharacter( slot );
	if ( pCharacterInstance == NULL )
	{
		return;
	}

	IAttachmentManager* const pAttachmentManager = pCharacterInstance->GetIAttachmentManager();
	assert( pAttachmentManager != NULL );

	const char* const weaponJointName = GetWeaponJointName();
	IAttachment* const pPrimaryWeaponAttachment = pAttachmentManager->CreateAttachment( DEFAULT_TURRET_PRIMARY_WEAPON_ATTACHMENT_NAME, CA_BONE, weaponJointName );
	assert( pPrimaryWeaponAttachment != NULL );

	CEntityAttachment* const pPrimaryWeaponAttachmentObject = new CEntityAttachment();
	pPrimaryWeaponAttachmentObject->SetEntityId( m_primaryWeaponId );
	pPrimaryWeaponAttachment->AddBinding( pPrimaryWeaponAttachmentObject );
}


void CTurret::RemoveWeapons()
{
	RemoveLaserBeam();

	IItemSystem* const pItemSystem = GetItemSystem();
	if ( pItemSystem == NULL )
	{
		return;
	}

	StopFirePrimaryWeapon();
	RemoveItemAttachments();

	if ( m_primaryWeaponId )
	{
		IEntitySystem* const pEntitySystem = GetEntitySystem();
		assert( pEntitySystem != NULL );

		const bool forceRemoveNow = true;
		pEntitySystem->RemoveEntity( m_primaryWeaponId, forceRemoveNow );
	}

	m_primaryWeaponId = 0;
}


void CTurret::InitWeapons()
{
	IEntity* const pEntity = GetEntity();
	const bool isHidden = pEntity->IsHidden();
	IF_UNLIKELY( isHidden )
	{
		return;
	}

	RemoveWeapons();

	IItemSystem* const pItemSystem = GetItemSystem();
	if ( pItemSystem == NULL )
	{
		return;
	}

	assert( m_primaryWeaponId == 0 );

	stack_string itemName = pEntity->GetName();
	itemName += ".PrimaryWeapon";

	SEntitySpawnParams entitySpawnParams;
	entitySpawnParams.sName = itemName;
	entitySpawnParams.nFlags |= ( ENTITY_FLAG_NO_PROXIMITY | ENTITY_FLAG_NEVER_NETWORK_STATIC | ENTITY_FLAG_NO_SAVE );

	IEntitySystem* const pEntitySystem = GetEntitySystem();
	assert( pEntitySystem != NULL );

	const IEntityClassRegistry* const pEntityClassRegistry = pEntitySystem->GetClassRegistry();
	assert( pEntityClassRegistry != NULL );

	const char* const primaryWeaponClassName = GetPrimaryWeaponClassName();
	entitySpawnParams.pClass = pEntityClassRegistry->FindClass( primaryWeaponClassName );
	if ( entitySpawnParams.pClass == NULL )
	{
		return;
	}

	IEntity* const pItemEntity = pEntitySystem->SpawnEntity( entitySpawnParams );
	if ( pItemEntity == NULL )
	{
		return;
	}

	m_primaryWeaponId = pItemEntity->GetId();
	IItem* const pItem = pItemSystem->GetItem( m_primaryWeaponId );
	assert( pItem != NULL );

	const EntityId turretEntityId = pEntity->GetId();
	if ( CWeapon* pWeapon = static_cast< CWeapon* >( pItem->GetIWeapon() ) )
	{
		pWeapon->SetHostId( turretEntityId );

		CTacticalManager* const pTacticalManager = GetTacticalManager();
		if ( pTacticalManager != NULL )
		{
			pTacticalManager->RemoveEntity( m_primaryWeaponId, CTacticalManager::eTacticalEntity_Item );
		}
	}

	pItem->EnableUpdate( true, eIUS_General );

	const float primaryWeaponFovDegrees = GetPrimaryWeaponFovDegrees();
	m_primaryWeaponFovCos = cos( DEG2RAD( primaryWeaponFovDegrees ) );

	m_primaryWeaponRangeCheckOffset = GetPrimaryWeaponRangeCheckOffset();

	InitLaserBeam();

	CreateItemAttachments();
}


void CTurret::UpdateWeaponTarget()
{
	const Vec3 targetWorldPosition = GetTargetWorldPosition();
	const Vec3 rateOfDeathWorldOffset = GetRateOfDeathTargetOffset();
	const float targetDistance = ( GetEntity()->GetPos() - targetWorldPosition ).GetLength();

	assert( 0.0f < DEFAULT_TURRET_ACCURACY_RANGE );
	const float targetDistanceScale = ( targetDistance <= DEFAULT_TURRET_ACCURACY_DISTANCE_MAX ) ? max( targetDistance - DEFAULT_TURRET_ACCURACY_DISTANCE_MIN, 0.0f ) / DEFAULT_TURRET_ACCURACY_RANGE : 1.0f;
	const Vec3 adjustedRateOfDeathWorldOffset = rateOfDeathWorldOffset * targetDistanceScale;
	const Vec3 finalTargetWorldPosition = targetWorldPosition + adjustedRateOfDeathWorldOffset;
	SetWeaponTargetWorldPosition( finalTargetWorldPosition );
}


void CTurret::SetWeaponTargetWorldPosition( const Vec3& targetWorldPosition )
{
	IEntity* const pWeaponEntity = gEnv->pEntitySystem->GetEntity( m_primaryWeaponId );
	if ( ! pWeaponEntity )
	{
		return;
	}

	Vec3 targetWorldDirection = targetWorldPosition - pWeaponEntity->GetWorldPos();
	targetWorldDirection.NormalizeSafe( Vec3( 0, 1, 0 ) );
	const Quat weaponWorldOrientation = Quat( Matrix33::CreateRotationVDir( targetWorldDirection, 0 ) );
	pWeaponEntity->SetRotation( weaponWorldOrientation );
}


void CTurret::InitLaserBeam()
{
	// For E3: Reusing laser weapon attachment CLaserBeam helper class directly.

	RemoveLaserBeam();

	IEntity* const pEntity = GetEntity();
	SmartScriptTable pLaserTable = GetLaserPropertiesTable( pEntity );

	const bool laserEnabled = GetProperty< bool >( pLaserTable, "bEnabled", false );
	if ( ! laserEnabled )
	{
		return;
	}

	const char* const laserJointName = GetProperty< const char* >( pLaserTable, "JointName", FALLBACK_TURRET_LASER_JOINT_NAME );
	m_laserJointId = GetJointIdByName( laserJointName );
	if ( m_laserJointId < 0 )
	{
		return;
	}

	m_pLaserBeam.reset( new CLaserBeam() );
	m_pLaserParams.reset( new SLaserParams() );

	m_pLaserParams->laser_geometry_tp = GetProperty< const char* >( pLaserTable, "objGeometry", FALLBACK_TURRET_LASER_GEOMETRY );
	m_pLaserParams->laser_dot[ 0 ] = GetProperty< const char* >( pLaserTable, "DotEffect", FALLBACK_TURRET_LASER_DOT_EFFECT );
	
	m_pLaserParams->laser_dot[ 1 ] = m_pLaserParams->laser_dot[ 0 ];
	m_pLaserParams->laser_thickness[ 0 ] = GetProperty< float >( pLaserTable, "fThickness", FALLBACK_TURRET_LASER_THICKNESS );
	m_pLaserParams->laser_thickness[ 1 ] = m_pLaserParams->laser_thickness[ 0 ];
	m_pLaserParams->laser_range[ 0 ] = GetProperty< float >( pLaserTable, "fRange", FALLBACK_TURRET_LASER_MAX_RANGE );
	m_pLaserParams->laser_range[ 1 ] = m_pLaserParams->laser_range[ 0 ];
	m_pLaserParams->show_dot = GetProperty< bool >( pLaserTable, "bShowDot", FALLBACK_TURRET_LASER_SHOW_DOT );

	m_laserBeamOffset = FALLBACK_TURRET_LASER_LOCAL_OFFSET;
	m_laserBeamOffset.x = GetProperty< float >( pLaserTable, "fOffsetX", m_laserBeamOffset.x );
	m_laserBeamOffset.y = GetProperty< float >( pLaserTable, "fOffsetY", m_laserBeamOffset.y );
	m_laserBeamOffset.z = GetProperty< float >( pLaserTable, "fOffsetZ", m_laserBeamOffset.z );

	// Side effect of using CLaserBeam directly, need to set the parent to an item to work nicely:
	const EntityId laserParentEntityId = m_primaryWeaponId;
	m_pLaserBeam->Initialize( laserParentEntityId, m_pLaserParams.get(), eIGS_ThirdPerson );
	m_pLaserBeam->SetGeometrySlot( eIGS_ThirdPerson );
	m_pLaserBeam->TurnOnLaser();

	const char* laserSourceEffectName = GetProperty< const char* >( pLaserTable, "SourceEffect", FALLBACK_TURRET_LASER_SOURCE_EFFECT );
	IParticleEffect* const pSourceEffect = gEnv->pParticleManager->FindEffect( laserSourceEffectName );
	if ( pSourceEffect )
	{
		m_laserSourceEffectSlot = pEntity->LoadParticleEmitter( -1, pSourceEffect );
	}
}


void CTurret::RemoveLaserBeam()
{
	if ( 0 <= m_laserSourceEffectSlot )
	{
		IEntity* const pEntity = GetEntity();
		pEntity->FreeSlot( m_laserSourceEffectSlot );
		m_laserSourceEffectSlot = -1;
	}

	m_pLaserBeam.reset();
	m_pLaserParams.reset();
	m_laserJointId = -1;
}


void CTurret::UpdateLaserBeam( const float frameTimeSeconds )
{
	if ( m_pLaserBeam.get() == NULL )
	{
		return;
	}

	if ( m_laserJointId < 0 )
	{
		return;
	}

	IEntity* const pEntity = GetEntity();
	ICharacterInstance* const pCharacterInstance = pEntity->GetCharacter( DEFAULT_TURRET_MODEL_SLOT );
	assert( pCharacterInstance != NULL );

	ISkeletonPose* const pSkeletonPose = pCharacterInstance->GetISkeletonPose();
	assert( pSkeletonPose != NULL );

	const Matrix34& worldTM = pEntity->GetWorldTM();
	const QuatT& laserJointAbsoluteModelLocation = pSkeletonPose->GetAbsJointByID( m_laserJointId );
	QuatT offsetedLaserJointAbsoluteModelLocation = laserJointAbsoluteModelLocation;
	offsetedLaserJointAbsoluteModelLocation.t += ( laserJointAbsoluteModelLocation.q * m_laserBeamOffset );

	const EntityId laserEntityId = m_pLaserBeam->GetLaserEntityId();
	IEntity* const pLaserEntity = gEnv->pEntitySystem->GetEntity( laserEntityId );
	if ( pLaserEntity )
	{
		pLaserEntity->SetLocalTM( Matrix34::CreateTranslationMat( m_laserBeamOffset ) );
	}

	const Vec3 laserWorldPosition = worldTM * offsetedLaserJointAbsoluteModelLocation.t;
	const Matrix34 laserWorldOrientation = worldTM * Matrix34( offsetedLaserJointAbsoluteModelLocation.q );
	const Vec3 laserWorldDirection = laserWorldOrientation.GetColumn1();

	const bool isHidden = pEntity->IsHidden();
	CLaserBeam::SLaserUpdateDesc laserUpdateDesc( laserWorldPosition, laserWorldDirection, frameTimeSeconds, isHidden );
	m_pLaserBeam->UpdateLaser( laserUpdateDesc );

	if ( 0 <= m_laserSourceEffectSlot )
	{
		pEntity->SetSlotLocalTM( m_laserSourceEffectSlot, Matrix34( offsetedLaserJointAbsoluteModelLocation ) );
	}
}


IWeapon* CTurret::GetPrimaryWeapon() const
{
	IItemSystem* const pItemSystem = GetItemSystem();
	if ( pItemSystem == NULL )
	{
		return NULL;
	}

	IItem* const pItem = pItemSystem->GetItem( m_primaryWeaponId );
	if ( pItem == NULL )
	{
		return NULL;
	}

	IWeapon* pWeapon = pItem->GetIWeapon();
	return pWeapon;
}


void CTurret::StartFirePrimaryWeapon()
{
	IWeapon* const pWeapon = GetPrimaryWeapon();
	if ( pWeapon == NULL )
	{
		return;
	}

	pWeapon->StartFire();
}


void CTurret::StopFirePrimaryWeapon()
{
	IWeapon* const pWeapon = GetPrimaryWeapon();
	if ( pWeapon == NULL )
	{
		return;
	}

	pWeapon->StopFire();
}

bool CTurret::IsEntityPlayer( IEntity* pTargetEntity ) const
{
	if ( pTargetEntity == NULL )
	{
		return false;
	}

	EntityId localPlayerId = g_pGame->GetIGameFramework()->GetClientActorId();
	return localPlayerId == pTargetEntity->GetId();
}


bool CTurret::IsEntityCloaked( IEntity* pTargetEntity ) const
{
	if ( pTargetEntity == NULL )
	{
		return false;
	}

	const IAIObject* const pTargetAiObject = pTargetEntity->GetAI();
	if ( pTargetAiObject == NULL )
	{
		return false;
	}

	const IAIActor* const pTargetAiActor = pTargetAiObject->CastToIAIActor();
	if ( pTargetAiActor == NULL )
	{
		return false;
	}

	const IEntity* const pTurretEntity = GetEntity();
	const Vec3 turretWorldPosition = pTurretEntity->GetWorldPos();

	const bool checkCloak = true;
	const bool checkCloakDistance = false;
	const bool isCloaked = pTargetAiActor->IsInvisibleFrom( turretWorldPosition, checkCloak, checkCloakDistance );

	const Vec3 targetWorldPosition = pTargetEntity->GetWorldPos();
	const float cloakDetectionDistanceSquared = m_cloakDetectionDistance * m_cloakDetectionDistance;
	const float distanceSquared = Vec3( targetWorldPosition - turretWorldPosition ).GetLengthSquared();
	const bool inCloakDetectionDistance = ( distanceSquared < cloakDetectionDistanceSquared );

	const bool isInvisible = ( isCloaked && ! inCloakDetectionDistance );
	return isInvisible;
}


bool ShouldIgnoreAi( IAIObject* pAiObject )
{
#ifndef _RELEASE
	assert( gEnv->pConsole != NULL );

	if ( pAiObject == NULL )
	{
		return false;
	}

	static ICVar* s_pIgnorePlayer = gEnv->pConsole->GetCVar( "ai_IgnorePlayer" );
	if ( s_pIgnorePlayer == NULL )
	{
		return false;
	}

	const bool ignorePlayer = ( s_pIgnorePlayer->GetIVal() != 0 );
	const unsigned short aiType = pAiObject->GetAIType();
	const bool isAiPlayer = ( aiType == AIOBJECT_PLAYER );

	const bool shouldIgnoreAi = ( isAiPlayer && ignorePlayer );
	return shouldIgnoreAi;
#else
	return false;
#endif
}


bool CTurret::IsEntityHostileAndThreatening( IEntity* pTargetEntity ) const
{
	if ( pTargetEntity == NULL )
	{
		return false;
	}

	IEntity* const pTurretEntity = GetEntity();

	bool isAlive = true;
	bool isHostileAndThreatening = false;
	IAIObject* const pTurretAiObject = pTurretEntity->GetAI();
	IAIObject* pTargetAiObject = pTargetEntity->GetAI();

	{
		IVehicleSystem* const pVehicleSystem = g_pGame->GetIGameFramework()->GetIVehicleSystem();
		const EntityId targetEntityId = pTargetEntity->GetId();
		const IVehicle* const pVehicle = pVehicleSystem->GetVehicle( targetEntityId );
		if ( pVehicle != NULL )
		{
			const IActor* const pDriverActor = pVehicle->GetDriver();
			if ( pDriverActor != NULL )
			{
				IEntity* const pDriverEntity = pDriverActor->GetEntity();
				pTargetAiObject = pDriverEntity->GetAI();
			}
		}
	}

	if ( pTurretAiObject != NULL && pTargetAiObject != NULL )
	{
		const uint8 turretFactionId = pTurretAiObject->GetFactionID();
		const uint8 targetFactionId = pTargetAiObject->GetFactionID();

		const IFactionMap& factionMap = gEnv->pAISystem->GetFactionMap();
		const IFactionMap::ReactionType reactionType = factionMap.GetReaction( turretFactionId, targetFactionId );
		isHostileAndThreatening = ( reactionType == IFactionMap::Hostile ) && pTargetAiObject->IsThreateningForHostileFactions();
	}

	isHostileAndThreatening &= ! ShouldIgnoreAi( pTargetAiObject );

	return isHostileAndThreatening;
}


bool CTurret::IsEntityInVVV( IEntity* pTargetEntity ) const
{
	IF_UNLIKELY ( pTargetEntity == NULL )
	{
		return false;
	}

	const EntityId targetEntityId = pTargetEntity->GetId();
	const EntityId localPlayerEntityId = g_pGame->GetIGameFramework()->GetClientActorId();
	const bool isTargetLocalPlayer = ( targetEntityId == localPlayerEntityId );
	IF_LIKELY ( ! isTargetLocalPlayer )
	{
		return false;
	}

	IEntity* const pTurretEntity = GetEntity();
	const IAIObject* const pTurretAiObject = pTurretEntity->GetAI();
	IF_UNLIKELY ( pTurretAiObject == NULL )
	{
		return false;
	}

	const float perceptionScale = gEnv->pAISystem->GetGlobalVisualScale( pTurretAiObject );
	IF_LIKELY ( 0.99f <= perceptionScale )
	{
		// This assumes that if we're calling this function targetEntity is within range
		return false;
	}


	const IAIObject* const pTargetAiObject = pTargetEntity->GetAI();
	IF_UNLIKELY ( pTargetAiObject == NULL )
	{
		return false;
	}

	const IVisionMap* const pVisionMap = gEnv->pAISystem->GetVisionMap();
	assert( pVisionMap != NULL );

	const ObserverParams* const pObserverParams = pVisionMap->GetObserverParams( m_eyeVisionId );
	IF_UNLIKELY ( pObserverParams == NULL )
	{
		return false;
	}

	const float currentScaledMaxRange = pObserverParams->sightRange * perceptionScale;

	const Vec3& turretPos = pTurretAiObject->GetPos();
	const Vec3& targetPos = pTargetAiObject->GetPos();

	const float targetDistance = turretPos.GetDistance( targetPos );

	const bool isInVVV = ( currentScaledMaxRange < targetDistance );
	return isInVVV;
}


void CTurret::UpdateStateMachine( const float frameTimeSeconds )
{
	const bool debugStateMachine = false;
	StateMachineUpdateBehaviour( frameTimeSeconds, debugStateMachine );
}



void CTurret::SetTargetEntity( IEntity* pTarget )
{
	if ( pTarget != NULL )
	{
		const EntityId newTargetEntityId = pTarget->GetId();
		if( m_targetEntityId != newTargetEntityId )
		{
			m_targetEntityId = newTargetEntityId;
			NotifySelectedTarget( pTarget );
		}
	}
	else
	{
		m_targetEntityId = 0;
	}
}

void CTurret::NotifySelectedTarget( IEntity* pTargetEntity )
{
	assert( pTargetEntity != NULL );
	if ( IAIObject* const pTargetAiObject = pTargetEntity->GetAI() )
	{
		IAISignalExtraData* const pData = gEnv->pAISystem->CreateSignalExtraData();
		pData->nID = GetEntityId();
		pData->point = GetEntity()->GetWorldPos();
		gEnv->pAISystem->SendSignal( SIGNALFILTER_SENDER, 1, "OnTargetedByTurret", pTargetAiObject, pData );
	}
}

IEntity* CTurret::GetTargetEntity() const
{
	IEntitySystem* const pEntitySystem = GetEntitySystem();
	assert( pEntitySystem != NULL );

	IEntity* const pEntity = pEntitySystem->GetEntity( m_targetEntityId );
	return pEntity;
}


void CTurret::SetTargetWorldPosition( const Vec3& targetWorldPosition )
{
	m_targetWorldPosition = targetWorldPosition;
}


const Vec3& CTurret::GetTargetWorldPosition() const
{
	return m_targetWorldPosition;
}


void CTurret::ResetTarget()
{
	SetTargetEntity( NULL );

	const IEntity* const pEntity = GetEntity();
	const Vec3 turretWorldPosition = pEntity->GetWorldPos();
	const Vec3& turretForwardDirection = pEntity->GetForwardDir();
	const Vec3 targetWorldPosition = turretWorldPosition + turretForwardDirection * 10;
	SetTargetWorldPosition( targetWorldPosition );
}


bool CTurret::IsEntityInVisionRange( IEntity* pEntity ) const
{
	const IAIObject* const pAiObject = pEntity->GetAI();
	if ( pAiObject == NULL )
	{
		return false;
	}

	const VisionID& otherVisionId = pAiObject->GetVisionID();
	return IsVisionIdInVisionRange( otherVisionId );
}


bool CTurret::IsVisionIdInVisionRange( const VisionID& otherVisionId ) const
{
	const bool isDead = IsDead();
	if ( isDead )
	{
		return false;
	}
	else
	{
		const IVisionMap* const pVisionMap = gEnv->pAISystem->GetVisionMap();
		assert( pVisionMap != NULL );

		const bool isVisibleByRadar = pVisionMap->IsVisible( m_radarVisionId, otherVisionId );
		const bool isVisibleByEye = pVisionMap->IsVisible( m_eyeVisionId, otherVisionId );
		const bool isVisible = ( isVisibleByRadar || isVisibleByEye );
		return isVisible;
	}
}


int CTurret::GetAwarenessToActor( IAIObject* pAIObject, CActor* pActor ) const
{
	int awareness = 0;

	if (!pActor->IsCloaked())
	{
		const bool turretIsDeployed = (GetStateId() == eTurretBehaviorState_Deployed);
		if (turretIsDeployed && IsVisionIdInVisionRange( pAIObject->GetVisionID() ))
		{
			awareness = 2;
		}
	}

	return awareness;
}



bool CTurret::IsEntityInRange( const IEntity* pEntity, const float range ) const
{
	assert( pEntity != NULL );

	const float rangeSquared = range * range;

	const IEntity* const pTurretEntity = GetEntity();
	const Vec3 turretEntityWorldPosition = pTurretEntity->GetWorldPos();
	const Vec3 entityWorldPosition = pEntity->GetWorldPos();

	const float distanceSquared = turretEntityWorldPosition.GetSquaredDistance( entityWorldPosition );

	const bool inRange = ( distanceSquared <= rangeSquared );
	return inRange;
}



void CTurret::ResetHealth()
{
	const IEntity* const pEntity = GetEntity();

	SmartScriptTable pDamagePropertiesTable = GetDamagePropertiesTable( pEntity );
	const float maxHealthPropertyValue = GetProperty< float >( pDamagePropertiesTable, "health", FALLBACK_TURRET_MAX_HEALTH );
	const float maxHealth = max( 1.0f, maxHealthPropertyValue );
	SetMaxHealth( maxHealth );
	SetHealth( maxHealth );
}


void CTurret::ResetVision()
{
	IEntity* const pEntity = GetEntity();

	RemoveVision();
	m_updateDistanceSquared = DEFAULT_TURRET_UPDATE_DISTANCE_SQUARED;

	const bool isHidden = pEntity->IsHidden();
	IF_UNLIKELY( isHidden )
	{
		return;
	}

	SmartScriptTable pVisionPropertiesTable = GetVisionPropertiesTable( pEntity );

	IVisionMap* const pVisionMap = gEnv->pAISystem->GetVisionMap();
	assert( pVisionMap != NULL );

	{
		stack_string radarVisionIdName = pEntity->GetName();
		radarVisionIdName += ".RadarVisionId";
		m_radarVisionId = pVisionMap->CreateVisionID( radarVisionIdName );

		m_visionRadarOffset = GetProperty< float >( pVisionPropertiesTable, "fRadarVerticalOffset", FALLBACK_TURRET_RADAR_VERTICAL_OFFSET );

		const float radarFovDegrees = GetProperty< float >( pVisionPropertiesTable, "fRadarFov", FALLBACK_TURRET_RADAR_FOV_DEGREES );

		ObserverParams radarObserverParams;
		InitObserverParams( radarObserverParams, radarFovDegrees );
		radarObserverParams.eyePosition = CalculateRadarVisionWorldPosition();
		radarObserverParams.eyeDirection = pEntity->GetForwardDir();
		radarObserverParams.sightRange = GetProperty< float >( pVisionPropertiesTable, "fRadarRange", FALLBACK_TURRET_RADAR_RANGE );

		pVisionMap->RegisterObserver( m_radarVisionId, radarObserverParams );

		m_updateDistanceSquared = max( m_updateDistanceSquared, square( radarObserverParams.sightRange ) );
	}

	{
		stack_string eyeVisionIdName = pEntity->GetName();
		eyeVisionIdName += ".EyeVisionId";
		m_eyeVisionId = pVisionMap->CreateVisionID( eyeVisionIdName );

		const char* const eyeJointName = GetProperty< const char* >( pVisionPropertiesTable, "EyeJointName", FALLBACK_TURRET_EYE_JOINT_NAME );
		m_eyeJointId = GetJointIdByName( eyeJointName );

		const float eyeFovDegrees = GetProperty< float >( pVisionPropertiesTable, "fEyeFov", FALLBACK_TURRET_EYE_FOV_DEGREES );
	
		ObserverParams eyeObserverParams;
		InitObserverParams( eyeObserverParams, eyeFovDegrees );
		eyeObserverParams.eyePosition = CalculateEyeVisionWorldPosition();
		eyeObserverParams.eyeDirection = CalculateEyeVisionWorldDirection();
		eyeObserverParams.sightRange = GetProperty< float >( pVisionPropertiesTable, "fEyeRange", FALLBACK_TURRET_EYE_RANGE );
		m_eyeVisionRangeParameter = eyeObserverParams.sightRange;
		m_eyeVisionScaleExpireTime = CTimeValue();

		pVisionMap->RegisterObserver( m_eyeVisionId, eyeObserverParams );

		m_updateDistanceSquared = max( m_updateDistanceSquared, square( eyeObserverParams.sightRange ) );
	}

	const float cloakDetectionDistance = GetProperty< float >( pVisionPropertiesTable, "fCloakDetectionDistance", FALLBACK_TURRET_CLOAK_DETECTION_DISTANCE );
	const float cloakDetectionDistanceMultiplier = GetEntityInstanceProperty( pEntity, "fCloakDetectionDistanceMultiplier", 1.0f );
	m_cloakDetectionDistance = cloakDetectionDistance * cloakDetectionDistanceMultiplier;

	SetObservable( true );
}

void CTurret::InitObserverParams( ObserverParams& observerParamsOut, const float fovDegrees )
{
	IEntity* const pEntity = GetEntity();

	const IAIObject* const pAiObject = pEntity->GetAI();
	assert( pAiObject!= NULL );

	observerParamsOut.callback = functor( *this, &CTurret::VisionChanged );

	observerParamsOut.entityId = pEntity->GetId();

	const uint8 factionId = pAiObject->GetFactionID();
	observerParamsOut.factionsToObserveMask = ~0ul ^ ( 1 << factionId );

	const float fovCos = cosf( DEG2RAD( fovDegrees ) );
	observerParamsOut.fovCos = fovCos;

	observerParamsOut.typesToObserveMask = Player | AliveAgent | General;

	observerParamsOut.skipList[ 0 ] = pEntity->GetPhysics();
	observerParamsOut.skipListSize = 1;
}


void CTurret::RemoveVision()
{
	IVisionMap* const pVisionMap = gEnv->pAISystem->GetVisionMap();
	assert( pVisionMap != NULL );

	pVisionMap->UnregisterObserver( m_radarVisionId );
	m_radarVisionId = VisionID();

	pVisionMap->UnregisterObserver( m_eyeVisionId );
	m_eyeVisionId = VisionID();

	ClearVisibleTargets();
	ClearForcedVisibleTarget();
}


void CTurret::UpdateVision()
{
	UpdateVisionFaction();

	if ( 0 <= m_eyeJointId )
	{
		UpdateEyeVisionLocation();
		UpdateEyeVisionScaleExpiration();
	}

	UpdateVisibleTargets();
}


void CTurret::UpdateEyeVisionScaleExpiration()
{
	if ( m_eyeVisionScaleExpireTime == CTimeValue() )
	{
		return;
	}

	const CTimeValue timeNow = TurretHelpers::GetTimeNow();
	if ( m_eyeVisionScaleExpireTime < timeNow )
	{
		SetEyeVisionRangeScale( 1.0f, -1.0f );
		m_eyeVisionScaleExpireTime = CTimeValue();
	}
}


void CTurret::UpdateRadarVisionLocation()
{
	if ( m_radarVisionId == 0 )
	{
		return;
	}

	const IEntity* const pEntity = GetEntity();

	ObserverParams observerParams;
	observerParams.eyePosition = CalculateRadarVisionWorldPosition();
	observerParams.eyeDirection = pEntity->GetForwardDir();

	IVisionMap* const pVisionMap = gEnv->pAISystem->GetVisionMap();
	assert( pVisionMap != NULL );

	pVisionMap->ObserverChanged( m_radarVisionId, observerParams, eChangedPosition | eChangedOrientation );
}


void CTurret::UpdateEyeVisionLocation()
{
	if ( m_eyeVisionId == 0 )
	{
		return;
	}

	ObserverParams observerParams;
	observerParams.eyePosition = CalculateEyeVisionWorldPosition();
	observerParams.eyeDirection = CalculateEyeVisionWorldDirection();

	IVisionMap* const pVisionMap = gEnv->pAISystem->GetVisionMap();
	assert( pVisionMap != NULL );

	pVisionMap->ObserverChanged( m_eyeVisionId, observerParams, eChangedPosition | eChangedOrientation );
}


void CTurret::SetEyeVisionRangeScale( const float scale, const float timeoutSeconds )
{
	if ( m_eyeVisionId == 0 )
	{
		return;
	}

	const float clampedVisionRange = max( 0.0f, scale );
	const float scaledVisionRange = m_eyeVisionRangeParameter * clampedVisionRange;

	IVisionMap* const pVisionMap = gEnv->pAISystem->GetVisionMap();
	assert( pVisionMap != NULL );

	const ObserverParams* const pCurrentObserverParams = pVisionMap->GetObserverParams( m_eyeVisionId );
	assert( pCurrentObserverParams );

	ObserverParams observerParams;
	observerParams.fovCos = pCurrentObserverParams->fovCos;
	observerParams.sightRange = scaledVisionRange;

	pVisionMap->ObserverChanged( m_eyeVisionId, observerParams, eChangedSightRange | eChangedFOV );

	const CTimeValue timeNow = TurretHelpers::GetTimeNow();
	m_eyeVisionScaleExpireTime = ( 0 < timeoutSeconds ) ? timeNow + CTimeValue( timeoutSeconds ) : CTimeValue();
}


float CTurret::GetEyeVisionRangeScaleToSeePosition( const Vec3& worldPosition, const float additionalDistanceOffset ) const
{
	IF_UNLIKELY( m_eyeVisionRangeParameter <= 0.1f )
	{
		return 1.0f;
	}

	const Vec3 eyeWorldPosition = CalculateEyeVisionWorldPosition();
	const float distance = eyeWorldPosition.GetDistance( worldPosition );
	const float distanceWithOffset = distance + additionalDistanceOffset;
	const float necessaryScale = ( distanceWithOffset / m_eyeVisionRangeParameter );
	const float clampedScale = max( 1.0f, necessaryScale );
	return clampedScale;
}


float CTurret::GetCachedEyeVisionRangeParamValue() const
{
	return m_eyeVisionRangeParameter;
}


void CTurret::UpdateVisionFaction()
{
	IEntity* const pEntity = GetEntity();

	const IAIObject* const pAiObject = pEntity->GetAI();
	if ( pAiObject == NULL )
	{
		return;
	}

	ObserverParams observerParams;
	const uint8 factionId = pAiObject->GetFactionID();
	observerParams.factionsToObserveMask = ~0ul ^ ( 1 << factionId );

	IVisionMap* const pVisionMap = gEnv->pAISystem->GetVisionMap();
	assert( pVisionMap != NULL );

	{
		const ObserverParams* pEyeObserverParams = pVisionMap->GetObserverParams( m_eyeVisionId );
		if ( pEyeObserverParams != NULL )
		{
			if ( pEyeObserverParams->typesToObserveMask != observerParams.typesToObserveMask )
			{
				pVisionMap->ObserverChanged( m_eyeVisionId, observerParams, eChangedFaction );
			}
		}
	}

	{
		const ObserverParams* pRadarObserverParams = pVisionMap->GetObserverParams( m_radarVisionId );
		if ( pRadarObserverParams != NULL )
		{
			if ( pRadarObserverParams->factionsToObserveMask != observerParams.factionsToObserveMask )
			{
				pVisionMap->ObserverChanged( m_radarVisionId, observerParams, eChangedFaction );
			}
		}
	}
}


Vec3 CTurret::CalculateRadarVisionWorldPosition() const
{
	const IEntity* const pEntity = GetEntity();
	const Matrix34& worldTM = pEntity->GetWorldTM();

	const Vec3 entityWorldUp = worldTM.GetColumn2();
	const Vec3 entityWorldPosition = pEntity->GetWorldPos();

	const Vec3 radarWorldPosition = entityWorldPosition + ( m_visionRadarOffset * entityWorldUp );
	return radarWorldPosition;
}


Vec3 CTurret::CalculateEyeVisionWorldPosition() const
{
	if ( m_eyeJointId < 0 )
	{
		return CalculateRadarVisionWorldPosition();
	}

	IEntity* const pEntity = GetEntity();
	ICharacterInstance* const pCharacterInstance = pEntity->GetCharacter( DEFAULT_TURRET_MODEL_SLOT );
	assert( pCharacterInstance != NULL );

	ISkeletonPose* const pSkeletonPose = pCharacterInstance->GetISkeletonPose();
	assert( pSkeletonPose != NULL );

	const Matrix34& worldTM = pEntity->GetWorldTM();
	const QuatT& eyeJointAbsoluteModelLocation = pSkeletonPose->GetAbsJointByID( m_eyeJointId );

	const Vec3 eyeWorldPosition = worldTM * eyeJointAbsoluteModelLocation.t;
	return eyeWorldPosition;
}


Vec3 CTurret::CalculateEyeVisionWorldDirection() const
{
	if ( m_eyeJointId < 0 )
	{
		const IEntity* const pEntity = GetEntity();
		const Vec3& entityWorldDirection = pEntity->GetForwardDir();
		return entityWorldDirection;
	}

	IEntity* const pEntity = GetEntity();
	ICharacterInstance* const pCharacterInstance = pEntity->GetCharacter( DEFAULT_TURRET_MODEL_SLOT );
	assert( pCharacterInstance != NULL );

	ISkeletonPose* const pSkeletonPose = pCharacterInstance->GetISkeletonPose();
	assert( pSkeletonPose != NULL );

	const Matrix34& worldTM = pEntity->GetWorldTM();
	const QuatT& eyeJointAbsPos = pSkeletonPose->GetAbsJointByID( m_eyeJointId );

	const Matrix34 eyeWorldOrientation = worldTM * Matrix34( eyeJointAbsPos.q );
	const Vec3 eyeWorldDirection = eyeWorldOrientation.GetColumn1();
	return eyeWorldDirection;
}


void CTurret::VisionChanged( const VisionID& observerID, const ObserverParams& observerParams, const VisionID& observableID, const ObservableParams& observableParams, bool visible )
{
	if ( visible )
	{
		AddVisibleTarget( observableParams.entityId );
	}
	else
	{
		RemoveVisibleTarget( observableParams.entityId );
	}
}


IEntity* CTurret::GetValidVisibleTarget() const
{
	const IEntitySystem* const pEntitySystem = GetEntitySystem();
	assert( pEntitySystem != NULL );

	IEntity* const pForcedVisibleTarget = pEntitySystem->GetEntity( m_forcedVisibleTargetEntityId );
	if ( pForcedVisibleTarget != NULL )
	{
		return pForcedVisibleTarget;
	}

	IEntity* const pValidVisibleTarget = pEntitySystem->GetEntity( m_validVisbleTargetEntityId );
	return pValidVisibleTarget;
}


void CTurret::AddVisibleTarget( const EntityId targetEntityId )
{
	TrackedTargetsList::iterator it = std::find( m_trackedTargets.begin(), m_trackedTargets.end(), targetEntityId );
	const bool entryFound = ( it != m_trackedTargets.end() );
	if ( entryFound )
	{
		STrackedTarget& trackedTarget = *it;
		trackedTarget.referenceCount++;
	}
	else
	{
		m_trackedTargets.push_back( STrackedTarget( targetEntityId ) );
	}
}


void CTurret::RemoveVisibleTarget( const EntityId targetEntityId )
{
	TrackedTargetsList::iterator it = std::find( m_trackedTargets.begin(), m_trackedTargets.end(), targetEntityId );
	const bool entryFound = ( it != m_trackedTargets.end() );
	if ( ! entryFound )
	{
		return;
	}

	STrackedTarget& trackedTarget = *it;
	trackedTarget.referenceCount--;
	if ( trackedTarget.referenceCount == 0 )
	{
		m_trackedTargets.erase( it );
	}
}


void CTurret::UpdateVisibleTargets()
{
	const IEntitySystem* const pEntitySystem = GetEntitySystem();
	assert( pEntitySystem != NULL );

	m_validVisbleTargetEntityId = 0;
	for ( size_t i = 0; i < m_trackedTargets.size(); ++i )
	{
		STrackedTarget& trackedTarget = m_trackedTargets[ i ];

		IEntity* const pEntity = pEntitySystem->GetEntity( trackedTarget.entityId );

		const bool isEntityHostile = IsEntityHostileAndThreatening( pEntity );
		const bool isEntityCloaked = IsEntityCloaked( pEntity );
		const bool isEntityInVVV = IsEntityInVVV( pEntity );
		const bool isEntityPlayer = IsEntityPlayer( pEntity );

		trackedTarget.valid = ( isEntityHostile && ! isEntityCloaked && ! isEntityInVVV );

		if ( ( m_validVisbleTargetEntityId == 0 || isEntityPlayer ) && trackedTarget.valid )
		{
			m_validVisbleTargetEntityId = trackedTarget.entityId;
		}
	}
}


void CTurret::ClearVisibleTargets()
{
	m_trackedTargets.clear();
}


void CTurret::SetStateById( const ETurretBehaviorState forcedStateId )
{
	SStateEventForceState stateEventForceState( forcedStateId );
	StateMachineHandleEventBehaviour( stateEventForceState );
}


ETurretBehaviorState CTurret::GetStateId() const
{
	return m_stateId;
}


void CTurret::NotifyBehaviorStateEnter( const ETurretBehaviorState stateId )
{
	m_stateId = stateId;
}


void CTurret::AddToTacticalManager()
{
	const IEntity* const pEntity = GetEntity();
	const bool isHidden = pEntity->IsHidden();
	IF_UNLIKELY( isHidden )
	{
		return;
	}

	CTacticalManager* const pTacticalManager = GetTacticalManager();
	if ( pTacticalManager != NULL )
	{
		const EntityId entityId = GetEntityId();
		pTacticalManager->AddEntity( entityId, CTacticalManager::eTacticalEntity_Unit );
	}
}


void CTurret::RemoveFromTacticalManager()
{
	CTacticalManager* const pTacticalManager = GetTacticalManager();
	if ( pTacticalManager != NULL )
	{
		const EntityId entityId = GetEntityId();
		pTacticalManager->RemoveEntity( entityId, CTacticalManager::eTacticalEntity_Unit );
	}
}


void CTurret::UpdateTacticalIcon()
{
	CTacticalManager* const pTacticalManager = GetTacticalManager();
	if ( pTacticalManager == NULL )
	{
		return;
	}

	const EntityId turretEntityId = GetEntityId();

	const EntityId localPlayerEntityId = g_pGame->GetIGameFramework()->GetClientActorId();
	IEntity* const pPlayerEntity = gEnv->pEntitySystem->GetEntity( localPlayerEntityId );

	const bool isHostileToPlayer = IsEntityHostileAndThreatening( pPlayerEntity );
	const ETacticalInformationIconType overrideIcon = isHostileToPlayer ? eIconType_NumIcons : eIconType_Friendly;

	pTacticalManager->SetEntityOverrideIcon( turretEntityId, overrideIcon );
}


void CTurret::AddToRadar()
{
	const EntityId entityId = GetEntityId();

	SHUDEvent hudEvent( eHUDEvent_AddEntity );
	hudEvent.AddData( SHUDEventData( static_cast< int >( entityId ) ) );
	CHUDEventDispatcher::CallEvent( hudEvent );
}


void CTurret::RemoveFromRadar()
{
	const EntityId entityId = GetEntityId();

	SHUDEvent hudEvent( eHUDEvent_RemoveEntity );
	hudEvent.AddData( SHUDEventData( static_cast< int >( entityId ) ) );
	CHUDEventDispatcher::CallEvent( hudEvent );
}


void CTurret::SetTargetTrackClassThreat( const float classThreat )
{
	ITargetTrackManager* const pTargetTrackManager = gEnv->pAISystem->GetTargetTrackManager();
	IF_UNLIKELY ( pTargetTrackManager == NULL )
	{
		return;
	}

	const IEntity* const pTurretEntity = GetEntity();
	const tAIObjectID aiObjectId = pTurretEntity->GetAIObjectID();
	IF_UNLIKELY ( aiObjectId == INVALID_AIOBJECTID )
	{
		return;
	}

	pTargetTrackManager->SetTargetClassThreat( aiObjectId, classThreat );
}


int16 CTurret::GetJointIdByName( const char* jointName ) const
{
	assert( jointName != NULL );
	
	IEntity* const pEntity = GetEntity();
	ICharacterInstance* const pCharacterInstance = pEntity->GetCharacter( DEFAULT_TURRET_MODEL_SLOT );
	if ( pCharacterInstance == NULL )
	{
		return -1;
	}

	IDefaultSkeleton& rIDefaultSkeleton = pCharacterInstance->GetIDefaultSkeleton();
	const int16 jointId = rIDefaultSkeleton.GetJointIDByName( jointName );
	return jointId;
}


TurretBehaviorParams::SBehavior& CTurret::GetBehaviorParams()
{
	return m_behaviorParams;
}


float CTurret::CalculateYawRadians() const
{
	const IEntity* const pEntity = GetEntity();

	const Matrix34& worldTM = pEntity->GetWorldTM();
	const Matrix34 invertedWorldTM = worldTM.GetInverted();

	const Vec3 entityPosition = worldTM.GetTranslation();
	const Vec3 eyeWorldDirection = CalculateEyeVisionWorldDirection();
	const Vec3 currentLookWorldPosition = entityPosition + eyeWorldDirection;
	const Vec3 currentLookLocalPosition = invertedWorldTM * currentLookWorldPosition;

	float yawRadians = 0;
	float yawDistance = 0;
	TurretHelpers::CalculateTargetYaw( currentLookLocalPosition, yawRadians, yawDistance );

	return yawRadians;
}


void CTurret::ResetBehaviorParams()
{
	const IEntity* const pEntity = GetEntity();
	SmartScriptTable pBehaviorProperties = GetBehaviorPropertiesTable( pEntity );

	m_behaviorParams.Reset( pBehaviorProperties );
}


ETurretBehaviorState CTurret::GetInitialBehaviorStateId() const
{
	const IEntity* const pEntity = GetEntity();

	const char* const initialBehaviorStateName = GetEntityProperty< const char* >( pEntity, "esTurretState", "" );

	const ETurretBehaviorState initialBehaviorStateId = TurretBehaviorStateNames::FindId( initialBehaviorStateName );
	if ( initialBehaviorStateId != eTurretBehaviorState_Invalid )
	{
		return initialBehaviorStateId;
	}

	return eTurretBehaviorState_Deployed;
}


void CTurret::SetInitialStateById( const ETurretBehaviorState stateId )
{
	if ( stateId == m_stateId )
	{
		return;
	}

	ResetActionController();
	StartInitialMannequinActions();

	SetStateById( stateId );
}


void CTurret::CreateRateOfDeathHelper()
{
	RemoveRateOfDeathHelper();

	IEntity* const pEntity = GetEntity();
	const IAIObject* const pAiObject = pEntity->GetAI();
	if ( pAiObject )
	{
		m_pRateOfDeathHelper.reset( new CRateOfDeath_Turret( pAiObject ) );

		SmartScriptTable pRateOfDeathTable = GetRateOfDeathPropertiesTable( pEntity );
		m_pRateOfDeathHelper->Init( pRateOfDeathTable );
	}
}


void CTurret::RemoveRateOfDeathHelper()
{
	m_pRateOfDeathHelper.reset();
}


void CTurret::UpdateRateOfDeathHelper( const float frameTimeSeconds )
{
	if ( m_pRateOfDeathHelper.get() )
	{
		IEntity* const pTargetEntity = GetValidVisibleTarget();
		m_pRateOfDeathHelper->SetTarget( pTargetEntity );

		m_pRateOfDeathHelper->Update( frameTimeSeconds );
	}
}


Vec3 CTurret::GetRateOfDeathTargetOffset() const
{
	if ( m_pRateOfDeathHelper.get() )
	{
		return m_pRateOfDeathHelper->GetTargetOffset();
	}
	else
	{
		return Vec3( 0, 0, 0 );
	}
}


void CTurret::CacheBodyDamageResources()
{
	const IEntity* const pEntity = GetEntity();
	IScriptTable* const pScriptTable = pEntity->GetScriptTable();
	if ( pScriptTable == NULL )
	{
		return;
	}

	SmartScriptTable pPropertiesTable;
	const bool hasPropertiesTable = pScriptTable->GetValue( "Properties", pPropertiesTable );
	if ( ! hasPropertiesTable )
	{
		return;
	}

	CBodyDamageManager* pBodyDamageManager = g_pGame->GetBodyDamageManager();
	CRY_ASSERT( pBodyDamageManager );

	pBodyDamageManager->CacheBodyDamage( pPropertiesTable );
}


void CTurret::CreateBodyDamage()
{
	RemoveBodyDamage();

	CBodyDamageManager* const pBodyDamageManager = g_pGame->GetBodyDamageManager();
	CRY_ASSERT( pBodyDamageManager );

	IEntity* const pEntity = GetEntity();
	const TBodyDamageProfileId bodyDamageProfileId = pBodyDamageManager->GetBodyDamage( *pEntity );

	const bool hasValidDamageProfile = ( bodyDamageProfileId != INVALID_BODYDAMAGEPROFILEID );
	const bool hasValidCharacterForBodyDamage = ( pEntity->GetCharacter( DEFAULT_TURRET_MODEL_SLOT ) != NULL );
	if ( hasValidDamageProfile && hasValidCharacterForBodyDamage )
	{
		const EntityId entityId = pEntity->GetId();

		pBodyDamageManager->RegisterBodyDamageProfileIdBinding( entityId, bodyDamageProfileId );
		pBodyDamageManager->PhysicalizePlayer( bodyDamageProfileId, *pEntity );
	}

	pBodyDamageManager->GetBodyDestructibility( *pEntity, m_bodyDestructionInstance );
}


void CTurret::RemoveBodyDamage()
{
	CBodyDamageManager* const pBodyDamageManager = g_pGame->GetBodyDamageManager();
	CRY_ASSERT( pBodyDamageManager );

	const EntityId entityId = GetEntityId();
	pBodyDamageManager->UnregisterBodyDamageProfileIdBinding( entityId );

	const TBodyDestructibilityProfileId profileId = m_bodyDestructionInstance.GetProfileId();
	if ( profileId != INVALID_BODYDESTRUCTIBILITYPROFILEID )
	{
		IEntity* pEntity = GetEntity();
		pBodyDamageManager->ResetInstance( *pEntity, m_bodyDestructionInstance );
	}
}


void CTurret::UpdateBodyDestructionHit( const HitInfo& hitInfo, const float oldHealth, const float newHealth )
{
	const TBodyDestructibilityProfileId profileId = m_bodyDestructionInstance.GetProfileId();
	if ( profileId != INVALID_BODYDESTRUCTIBILITYPROFILEID )
	{
		CBodyDamageManager* const pBodyDamageManager = g_pGame->GetBodyDamageManager();
		CRY_ASSERT( pBodyDamageManager );

		IEntity* const pEntity = GetEntity();
		pBodyDamageManager->ProcessDestructiblesHit( *pEntity, m_bodyDestructionInstance, hitInfo, oldHealth, newHealth );
	}
}


void CTurret::UpdateBodyDestruction( const float frameTimeSeconds )
{
	const TBodyDestructibilityProfileId profileId = m_bodyDestructionInstance.GetProfileId();
	if ( profileId != INVALID_BODYDESTRUCTIBILITYPROFILEID )
	{
		m_bodyDestructionInstance.Update( frameTimeSeconds );
	}
}


bool CTurret::IsInPrimaryWeaponRange( const Vec3& worldPosition ) const
{
	const IEntity* const pWeaponEntity = gEnv->pEntitySystem->GetEntity( m_primaryWeaponId );
	IF_UNLIKELY ( pWeaponEntity == NULL )
	{
		return false;
	}

	const Vec3 weaponWorldPosition = pWeaponEntity->GetPos();
	const Vec3 weaponWorldDirection = pWeaponEntity->GetForwardDir();

	{
		const CCamera& viewCamera = gEnv->pSystem->GetViewCamera();
		const Matrix34& viewCameraMatrix = viewCamera.GetMatrix();
		const Vec3 cameraWorldDirectionForward = viewCameraMatrix.GetColumn1().GetNormalized();

		const Vec3 cameraWorldPosition = viewCameraMatrix.GetTranslation();
		const Vec3 cameraToTargetWorldDirection = ( weaponWorldPosition - cameraWorldPosition ).GetNormalizedSafe( cameraWorldDirectionForward );

		const float dot = cameraToTargetWorldDirection.Dot( cameraWorldDirectionForward );
		if ( dot < 0.f )
		{
			return true;
		}
	}

	// Moving the origin of the check a bit backwards so that we are still able to see targets that are very close to the weapon.
	const Vec3 adjustedWeaponPosition = weaponWorldPosition - ( weaponWorldDirection * m_primaryWeaponRangeCheckOffset );

	Vec3 direction = worldPosition - adjustedWeaponPosition;
	direction.NormalizeSafe( Vec3( 0, 1, 0 ) );

	const float dot = direction.Dot( weaponWorldDirection );

	const bool inFov = ( m_primaryWeaponFovCos <= dot );
	
	const bool inWeaponRange = inFov;
	return inWeaponRange;
}


void CTurret::SetThreateningForHostileFactions( const bool threatening )
{
	IEntity* const pEntity = GetEntity();
	IAIObject* const pAiObject = pEntity->GetAI();
	if ( pAiObject == NULL )
	{
		return;
	}

	pAiObject->SetThreateningForHostileFactions( threatening );
}


void CTurret::SetObservable( const bool observable )
{
	IEntity* const pEntity = GetEntity();
	IAIObject* const pAiObject = pEntity->GetAI();
	if ( pAiObject == NULL )
	{
		return;
	}

	pAiObject->SetObservable( observable );
}


void CTurret::SetForcedVisibleTarget( const EntityId forcedVisibleTargetEntityId )
{
	m_forcedVisibleTargetEntityId = forcedVisibleTargetEntityId;
}


void CTurret::ClearForcedVisibleTarget()
{
	SetForcedVisibleTarget( 0 );
}


EntityId CTurret::GetForcedVisibleTarget() const
{
	return m_forcedVisibleTargetEntityId;
}


void CTurret::SetAllowFire( const bool allowFire )
{
	m_allowFire = allowFire;
}


bool CTurret::GetAllowFire() const
{
	return m_allowFire;
}

void CTurret::InitTurretSoundManager()
{
	m_pSoundManager.reset();

	IEntity* const pEntity = GetEntity();
	IEntityAudioComponent* pIEntityAudioComponent = pEntity->GetOrCreateComponent<IEntityAudioComponent>();

	ICharacterInstance* const pCharacterInstance = pEntity->GetCharacter( DEFAULT_TURRET_MODEL_SLOT );

	SmartScriptTable pSoundTable = GetSubPropertiesTable( pEntity, "Sound" );
	m_pSoundManager.reset( new CTurretSoundManager( pSoundTable, pCharacterInstance, pIEntityAudioComponent ) );
}


void CTurret::UpdateTurretSoundManager( const float frameTimeSeconds )
{
	if ( m_pSoundManager.get() == NULL )
	{
		return;
	}

	IEntity* const pEntity = GetEntity();
	ICharacterInstance* const pCharacterInstance = pEntity->GetCharacter( DEFAULT_TURRET_MODEL_SLOT );

	const bool isActive = ( m_stateId == eTurretBehaviorState_Deployed );

	m_pSoundManager->Update( pCharacterInstance, isActive, m_targetEntityId, frameTimeSeconds );
}


void CTurret::NotifyPreparingToFire( const CTimeValue& nextStartFireTime )
{
	if ( m_pSoundManager.get() == NULL )
	{
		return;
	}

	m_pSoundManager->NotifyPreparingToFire( nextStartFireTime );
}


void CTurret::NotifyCancelPreparingToFire()
{
	if ( m_pSoundManager.get() == NULL )
	{
		return;
	}

	m_pSoundManager->NotifyCancelPreparingToFire();
}

void CTurret::OnEntityKilledEarly(const HitInfo &hitInfo) 
{

}

void CTurret::OnEntityKilled( const HitInfo &hitInfo )
{
}

void CTurret::RegisterAsKillListener()
{
	if ( ! m_registeredAsKillListener )
	{
		CGameRules* const pGameRules = g_pGame->GetGameRules();
		if ( pGameRules != NULL )
		{
			pGameRules->RegisterKillListener( this );
			m_registeredAsKillListener = true;
		}
	}
}

void CTurret::UnRegisterAsKillListener()
{
	if ( m_registeredAsKillListener )
	{
		CGameRules* const pGameRules = g_pGame->GetGameRules();
		if ( pGameRules != NULL )
		{
			pGameRules->UnRegisterKillListener( this );
			m_registeredAsKillListener = false;
		}
	}
}


void CTurret::SetGroupId( const int groupId )
{
	IEntity* const pEntity = GetEntity();

	IAIObject* const pAiObject = pEntity->GetAI();
	assert( pAiObject!= NULL );

	pAiObject->SetGroupId( groupId );
}


int CTurret::GetGroupId() const
{
	IEntity* const pEntity = GetEntity();

	const IAIObject* const pAiObject = pEntity->GetAI();
	if ( pAiObject == NULL )
	{
		return -1;
	}

	const int groupId = pAiObject->GetGroupId();
	return groupId;
}


void CTurret::NotifyGroupTargetSpotted( const IEntity* pTargetEntity )
{
	if ( pTargetEntity == NULL )
	{
		return;
	}

	// Caution, don't use this code as a reference on how to communicate with other ai group members!
	// Since the turret is not a full fledged ai, only an AIObject, we need to workaround the issue that it doesn't have
	// a real attention target on the ai system side, and no real way to share it with the other members of the group.
	const int groupId = GetGroupId();
	if ( groupId < 0 )
	{
		return;
	}

	IAIObject* const pGroupMemeberAiObject = gEnv->pAISystem->GetGroupMember( groupId, 0 );
	if ( pGroupMemeberAiObject == NULL )
	{
		return;
	}

	// Do not notify in case the group is hostile.
	const bool isGroupHostile = IsEntityHostileAndThreatening( pGroupMemeberAiObject->GetEntity() );
	if ( isGroupHostile )
	{
		return;
	}

	const tAIObjectID groupMemberAiObjectId = pGroupMemeberAiObject->GetAIObjectID();

	TargetTrackHelpers::SStimulusEvent stimulusEventInfo;
	stimulusEventInfo.eStimulusType = TargetTrackHelpers::eEST_Generic;
	stimulusEventInfo.eTargetThreat = AITHREAT_THREATENING;
	stimulusEventInfo.vPos = pTargetEntity->GetWorldPos();

	const float radius = 100.0f;

	ITargetTrackManager* const pTargetTrackManager = gEnv->pAISystem->GetTargetTrackManager();
	assert( pTargetTrackManager );
	pTargetTrackManager->HandleStimulusEventInRange( groupMemberAiObjectId, "TurretSpottedTarget", stimulusEventInfo, radius );

	const char* const signalName = "GroupMemberEnteredCombat";
	const uint32 signalNameCrc32 = CCrc32::Compute( signalName );
	gEnv->pAISystem->SendSignal( SIGNALFILTER_GROUPONLY, 1, signalName, pGroupMemeberAiObject, NULL, signalNameCrc32 );
}


void CTurret::InitAutoAimParams()
{
	UnRegisterFromAutoAimSystem();

	m_autoAimParams = SAutoaimTargetRegisterParams();

	IEntity* const pEntity = GetEntity();
	SmartScriptTable pAutoAimParamsTable = GetAutoAimParamsTable( pEntity );

	m_autoAimParams.fallbackOffset = GetProperty< float >( pAutoAimParamsTable, "fallbackOffset", m_autoAimParams.fallbackOffset );
	m_autoAimParams.innerRadius = GetProperty< float >( pAutoAimParamsTable, "innerRadius", m_autoAimParams.innerRadius );
	m_autoAimParams.outerRadius = GetProperty< float >( pAutoAimParamsTable, "outerRadius", m_autoAimParams.outerRadius );
	m_autoAimParams.snapRadius = GetProperty< float >( pAutoAimParamsTable, "snapRadius", m_autoAimParams.snapRadius );
	m_autoAimParams.snapRadiusTagged = GetProperty< float >( pAutoAimParamsTable, "snapRadiusTagged", m_autoAimParams.snapRadiusTagged );

	ICharacterInstance* const pCharacterInstance = pEntity->GetCharacter( DEFAULT_TURRET_MODEL_SLOT );
	IF_UNLIKELY ( pCharacterInstance == NULL )
	{
		return;
	}

	IDefaultSkeleton& rIDefaultSkeleton = pCharacterInstance->GetIDefaultSkeleton();

	const char* const primaryBoneName = GetProperty< const char* >( pAutoAimParamsTable, "primaryTargetBone", "" );
	m_autoAimParams.primaryBoneId = rIDefaultSkeleton.GetJointIDByName( primaryBoneName );
	m_autoAimParams.hasSkeleton |= ( m_autoAimParams.primaryBoneId != -1 );

	const char* const secondaryBoneName = GetProperty< const char* >( pAutoAimParamsTable, "secondaryTargetBone", "" );
	m_autoAimParams.secondaryBoneId = rIDefaultSkeleton.GetJointIDByName( secondaryBoneName );
	m_autoAimParams.hasSkeleton |= ( m_autoAimParams.secondaryBoneId != -1 );

	const char* const physicsBoneName = GetProperty< const char* >( pAutoAimParamsTable, "physicsTargetBone", "" );
	m_autoAimParams.physicsBoneId = rIDefaultSkeleton.GetJointIDByName( physicsBoneName );
	m_autoAimParams.hasSkeleton |= ( m_autoAimParams.physicsBoneId != -1 );
}


void CTurret::RegisterInAutoAimSystem()
{
	const IEntity* const pEntity = GetEntity();
	const bool isHidden = pEntity->IsHidden();
	IF_UNLIKELY( isHidden )
	{
		return;
	}

	UnRegisterFromAutoAimSystem();

	const EntityId entityId = GetEntityId();
	CAutoAimManager& autoAimManager = g_pGame->GetAutoAimManager();
	m_registeredInAutoAimSystem = autoAimManager.RegisterAutoaimTargetObject( entityId, m_autoAimParams );
}


void CTurret::UnRegisterFromAutoAimSystem()
{
	IF_UNLIKELY ( ! m_registeredInAutoAimSystem )
	{
		return;
	}

	const EntityId entityId = GetEntityId();
	CAutoAimManager& autoAimManager = g_pGame->GetAutoAimManager();
	autoAimManager.UnregisterAutoaimTarget( entityId );
	m_registeredInAutoAimSystem = false;
}


void CTurret::RegisterInAwarenessToPlayerHelper()
{
	if ( CGameAISystem* pGameAiSystem = g_pGame->GetGameAISystem() )
	{
		pGameAiSystem->GetAIAwarenessToPlayerHelper().RegisterAwarenessEntity( this );
	}
}


void CTurret::UnRegisterFromAwarenessToPlayerHelper()
{
	if ( CGameAISystem* pGameAiSystem = g_pGame->GetGameAISystem() )
	{
		pGameAiSystem->GetAIAwarenessToPlayerHelper().UnregisterAwarenessEntity( this );
	}
}


Vec3 CTurret::CalculateRadarForwardWorldDirection() const
{
	const IEntity* const pTargetEntity = GetTargetEntity();
	if ( pTargetEntity != NULL )
	{
		const IEntity* const pTurretEntity = GetEntity();
		const Vec3 turretWorldPosition = pTurretEntity->GetWorldPos();
		const Vec3 targetEntityWorldPosition = pTargetEntity->GetWorldPos();
		const Vec3 turretForwardWorldDirection = pTurretEntity->GetForwardDir();
		const Vec3 turretToTargetWorldDirection = ( targetEntityWorldPosition - turretWorldPosition ).GetNormalizedSafe( turretForwardWorldDirection );
		return turretToTargetWorldDirection;
	}
	else
	{
		const Vec3 eyeVisionWorldDirection = CalculateEyeVisionWorldDirection();
		return eyeVisionWorldDirection;
	}
}


void CTurret::UpdateFactionChange()
{
	const uint8 factionId = GetFactionId();
	const bool factionChanged = ( m_factionOld != factionId );
	if ( factionChanged )
	{
		UpdateTacticalIcon();
		
		m_factionOld = factionId;

		// Refresh radar icon so it can appear friendly
		RemoveFromRadar();
		AddToRadar();
	}
}


void CTurret::ProcessScriptEvent( const char* const eventName )
{
	static const uint32 s_dropPodLaunchCrc = CCrc32::Compute( "DropPod_Launch" );
	static const uint32 s_dropPodLandCrc = CCrc32::Compute( "DropPod_Land" );

	const uint32 eventNameCrc = CCrc32::Compute( eventName );
	if ( eventNameCrc == s_dropPodLaunchCrc )
	{
		ResetActionController();
		UpdateActionController( 0.1f );
		StartInitialMannequinActions();
		SetStateById( eTurretBehaviorState_Undeployed );
	}
	else if ( eventNameCrc == s_dropPodLandCrc )
	{
		SetStateById( eTurretBehaviorState_Deployed );
	}
}
