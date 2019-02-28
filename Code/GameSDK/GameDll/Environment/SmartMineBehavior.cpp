// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

/*************************************************************************
-------------------------------------------------------------------------

Description: Smart proximity mine HSM

-------------------------------------------------------------------------
History:
- 20:03:2012: Created by Benito G.R.

*************************************************************************/

#include "StdAfx.h"
#include "SmartMine.h"

#include "../Game.h"
#include "../GameCache.h"

#include "../SShootHelper.h"
#include "../WeaponSystem.h"
#include "../Audio/AudioSignalPlayer.h"

#include <CryAISystem/IFactionMap.h>

#define SMART_MINE_MODEL_SLOT  0

#define SMART_MINE_MODEL              "Objects/default/primitive_sphere.cgf" 
#define SMART_MINE_MATERIAL_DISARMED  "Materials/basecolors/base_green_bright" 
#define SMART_MINE_MATERIAL_ARMED     "Materials/basecolors/base_red_firebrick" 

#define SMART_MINE_TRIGGER_BOX						Vec3(5.0f, 5.0f, 3.0f)
#define SMART_MINE_DETONATION_RADIUS			2.5f
#define SMART_MINE_TRIGGERTODETONATEDELAY 0.75f
#define SMART_MINE_EXPLOSION_TYPE					"SmartMineExplosive"
#define SMART_MINE_DAMAGE									800

#define SMART_MINE_NONACTIVE_LIGHT_SCALE  0.25f

#define SMART_MINE_ANIMATION_DETONATE			"deploy_01"

#define SMART_MINE_AUDIOTYPE_DEFAULT "Cell"

#define SMART_MINE_MASS	1000.0f
#define SMART_MINE_JUMP_IMPULSE 5000.0f

#define SMART_MINE_FACTION "BlackOps"

namespace
{
	ILINE bool PointInShere( const Vec3& point, const Sphere& sphere )
	{
		return ( (sphere.center - point).GetLengthSquared() < (sphere.radius * sphere.radius) );
	}
};

struct SmartMinePersistantProperties
{
	enum ELightType
	{
		eLightType_Armed = 0,
		eLightType_Disarmed,
		eLightType_Count
	};

	struct LightProperties
	{
		LightProperties()
			: diffuseColor(ZERO)
			, diffuseMultiplier(1.0f)
			, specularMultiplier(1.0f)
			, hdrDynamic(0.0f)
		{

		}

		Vec3  diffuseColor;
		float diffuseMultiplier;
		float specularMultiplier;
		float hdrDynamic;
	};

	SmartMinePersistantProperties()
		: m_explosionType(SMART_MINE_EXPLOSION_TYPE)
		, m_triggerToDetonateDelay(SMART_MINE_TRIGGERTODETONATEDELAY)
		, m_detonationRadius(SMART_MINE_DETONATION_RADIUS)
		, m_damage(SMART_MINE_DAMAGE)
		, m_initialPhysicsType(PE_RIGID)
		, m_currentPhysicsType(PE_NONE)
		, m_jumpImpulse(SMART_MINE_JUMP_IMPULSE)
	{
	}

	void ReadProperties(SmartScriptTable& propertiesTable)
	{
		propertiesTable->GetValue("fTriggerToDetonateDelay", m_triggerToDetonateDelay);
		propertiesTable->GetValue("fDetonationRadius", m_detonationRadius);
		propertiesTable->GetValue("ExplosionType", m_explosionType);
		propertiesTable->GetValue("Damage", m_damage);

		SmartScriptTable lightArmed;
		if (propertiesTable->GetValue("LightArmed", lightArmed))
		{
			lightArmed->GetValue("clrDiffuse", m_lightProperties[eLightType_Armed].diffuseColor);
			lightArmed->GetValue("fDiffuseMultiplier", m_lightProperties[eLightType_Armed].diffuseMultiplier);
			lightArmed->GetValue("fSpecularMultiplier", m_lightProperties[eLightType_Armed].specularMultiplier);
			lightArmed->GetValue("fHDRDynamic", m_lightProperties[eLightType_Armed].hdrDynamic);
		}

		SmartScriptTable lightDisarmed;
		if (propertiesTable->GetValue("LightDisarmed", lightDisarmed))
		{
			lightDisarmed->GetValue("clrDiffuse", m_lightProperties[eLightType_Disarmed].diffuseColor);
			lightDisarmed->GetValue("fDiffuseMultiplier", m_lightProperties[eLightType_Disarmed].diffuseMultiplier);
			lightDisarmed->GetValue("fSpecularMultiplier", m_lightProperties[eLightType_Disarmed].specularMultiplier);
			lightDisarmed->GetValue("fHDRDynamic", m_lightProperties[eLightType_Disarmed].hdrDynamic);
		}

		SmartScriptTable physics;
		if (propertiesTable->GetValue("Physics", physics))
		{
			int physicalizeStatic = 0;
			physics->GetValue("bStatic", physicalizeStatic);
			physics->GetValue("fJumpImpulse", m_jumpImpulse);
			if (physicalizeStatic)
			{
				m_initialPhysicsType = PE_STATIC;
			}
		}
	}

	const char* m_explosionType;
	float m_triggerToDetonateDelay;
	float m_detonationRadius;
	int   m_damage;
	int   m_initialPhysicsType;
	int   m_currentPhysicsType;
	float m_jumpImpulse;

	LightProperties m_lightProperties[eLightType_Count];
};

struct SmartMineSetupProperties
{
	SmartMineSetupProperties(IEntity& smartMineEntity, SmartMinePersistantProperties& persistantProperties)
		: m_modelName(SMART_MINE_MODEL)
		, m_materialArmed(SMART_MINE_MATERIAL_ARMED)
		, m_materialDisarmed(SMART_MINE_MATERIAL_DISARMED)
		, m_triggerBox(SMART_MINE_TRIGGER_BOX)
		, m_faction(SMART_MINE_FACTION)
		, m_audioType(SMART_MINE_AUDIOTYPE_DEFAULT)
	{
		IScriptTable* pSmartMineScript = smartMineEntity.GetScriptTable();

		if (pSmartMineScript != NULL)
		{
			SmartScriptTable propertiesTable;
			if (pSmartMineScript->GetValue("Properties", propertiesTable))
			{
				propertiesTable->GetValue("object_Model", m_modelName);
				propertiesTable->GetValue("MaterialArmed", m_materialArmed);
				propertiesTable->GetValue("MaterialDisarmed", m_materialDisarmed);
				propertiesTable->GetValue("vTriggerBox", m_triggerBox);
				propertiesTable->GetValue("esFaction", m_faction);

				persistantProperties.ReadProperties(propertiesTable);

				m_triggerBox.Set( persistantProperties.m_detonationRadius * 1.5f, persistantProperties.m_detonationRadius * 1.5f, 3.0f );

				SmartScriptTable audioTable;
				if(propertiesTable->GetValue("Audio", audioTable))
				{
					audioTable->GetValue("esSmartMineType", m_audioType);
				}
			}
		}
	}

	Vec3				m_triggerBox;
	const char* m_modelName;
	const char* m_materialArmed;
	const char* m_materialDisarmed;
	const char* m_faction;

	const char* m_audioType;
};

// cppcheck-suppress uninitMemberVar
class CSmartMineBehavior
	: public CStateHierarchy< CSmartMine >
{
	DECLARE_STATE_CLASS_BEGIN( CSmartMine, CSmartMineBehavior )
		DECLARE_STATE_CLASS_ADD( CSmartMine, Tracking )
			DECLARE_STATE_CLASS_ADD( CSmartMine, Armed )
			DECLARE_STATE_CLASS_ADD( CSmartMine, Waiting )
		DECLARE_STATE_CLASS_ADD( CSmartMine, Detonating )
		DECLARE_STATE_CLASS_ADD( CSmartMine, Dead )
	DECLARE_STATE_CLASS_END( CSmartMine )

private:

	enum EVisualState
	{
		eVisualState_None  = -1,
		eVisualState_Armed =  0,
		eVisualState_Disarmed,
		eVisualState_Count
	};

	enum EModelType
	{
		eModelType_None = 0,
		eModelType_StaticObject,
		eModelType_AnimatedObject,
	};

	enum EAudioType  
	{
		eAudioType_Cell = 0,
		eAudioType_Count
	};

	enum ESmartMineSounds
	{
		eSmartMineSound_ArmedOff = 0,
		eSmartMineSound_ArmedOn,
		eSmartMineSound_ArmedLoop,
		eSmartMineSound_Detonate,
		eSmartMineSound_Count
	};

	enum EAudioHint
	{
		eAudioHint_Load = 0,
		eAudioHint_Unload,
	};

	typedef _smart_ptr<IMaterial>	TMaterialPtr;

	void SetupSmartMine( CSmartMine& smartMine, const SmartMineSetupProperties& smartMineProperties );
	void Physicalize( CSmartMine& smartMine, const int physicalizeType );
	void PreCacheResources( const SmartMineSetupProperties& smartMineProperties );
	void SendSoundSystemGameHint( const EAudioHint audioHint );
	void SetupLightSource( CSmartMine& smartMine );
	
	IEntityTriggerComponent* GetTriggerProxyForSmartMine( CSmartMine& smartMine ) const;

	void EnableSmartMineUpdate( CSmartMine& smartMine );
	void DisableSmartMineUpdate( CSmartMine& smartMine );

	void UpdateVisualState( CSmartMine& smartMine, const EVisualState visualState);

	bool UpdateArmedState( CSmartMine& smartMine, const float frameTime );
	bool UpdateDetonatingState( CSmartMine& smartMine, const float frameTime );

	void StartDetonation( CSmartMine& smartMine );

	SmartMinePersistantProperties m_properties;
	TMaterialPtr m_mineMaterials[eVisualState_Count];
	
	EVisualState m_visualState;
	EntityEffects::TAttachedEffectId m_lightEffectId;

	CAudioSignalPlayer m_loopAudioPlayer;
	TAudioSignalID m_audioSignalIds[eSmartMineSound_Count];

	EModelType	m_modelType;
	EAudioType	m_audioType;

	float m_detonateTimeout;
	bool  m_audioGameHintSent;
};

DEFINE_STATE_CLASS_BEGIN( CSmartMine, CSmartMineBehavior, eSmartMineState_Behavior, Waiting )
	// Constructor member initialization
	m_visualState = eVisualState_None;
	m_lightEffectId = 0;
	ZeroArray(m_audioSignalIds);
	m_modelType = eModelType_None;
	m_audioType = eAudioType_Cell;
	m_detonateTimeout = 0.0f;
	m_audioGameHintSent = false;
	DEFINE_STATE_CLASS_ADD( CSmartMine, CSmartMineBehavior, Tracking, Root )
		DEFINE_STATE_CLASS_ADD( CSmartMine, CSmartMineBehavior, Armed, Tracking )
		DEFINE_STATE_CLASS_ADD( CSmartMine, CSmartMineBehavior, Waiting, Tracking )
	DEFINE_STATE_CLASS_ADD( CSmartMine, CSmartMineBehavior, Detonating, Root )
	DEFINE_STATE_CLASS_ADD( CSmartMine, CSmartMineBehavior, Dead, Root )
DEFINE_STATE_CLASS_END( CSmartMine, CSmartMineBehavior )

const CSmartMineBehavior::TStateIndex CSmartMineBehavior::Root( CSmartMine& smartMine, const SStateEvent& event )
{
	switch ( event.GetEventId() )
	{
	case STATE_EVENT_INIT:
		{
			for(int i = 0; i < eVisualState_Count; ++i)
			{
				m_mineMaterials[i] = NULL;
			}
			
			m_visualState = eVisualState_None;
			m_modelType = eModelType_None;
			m_audioType = eAudioType_Cell;
			m_lightEffectId = 0;
			m_audioGameHintSent = false;

			SmartMineSetupProperties smartMineProperties(*smartMine.GetEntity(), m_properties);

			PreCacheResources( smartMineProperties );

			SetupSmartMine( smartMine, smartMineProperties );

			SendSoundSystemGameHint( eAudioHint_Load );
		}
		break;

	case STATE_EVENT_RELEASE:
		{
			for(int i = 0; i < eVisualState_Count; ++i)
			{
				m_mineMaterials[i] = NULL;
			}

			SendSoundSystemGameHint( eAudioHint_Unload );

			if (m_lightEffectId != 0)
			{
				smartMine.GetEffectsController().DetachEffect( m_lightEffectId );
				m_lightEffectId = 0;
			}
		}
		break;

	case STATE_EVENT_SERIALIZE:
		{
			TSerialize& serializer = static_cast<const SStateEventSerialize&>(event).GetSerializer();

			bool audioSystemHintSent = m_audioGameHintSent;
			serializer.Value("AudioSystemHintSent", audioSystemHintSent);
			int  physicsType = m_properties.m_currentPhysicsType;
			serializer.Value("PhysicsType", physicsType);

			if (serializer.IsReading())
			{
				if (audioSystemHintSent != m_audioGameHintSent)
				{
					const EAudioHint audioHint = audioSystemHintSent ? eAudioHint_Load : eAudioHint_Unload;
					SendSoundSystemGameHint( audioHint );
				}

				ICharacterInstance* pCharacter = smartMine.GetEntity()->GetCharacter( SMART_MINE_MODEL_SLOT );
				ISkeletonPose* pSkeletonPose = (pCharacter != NULL) ? pCharacter->GetISkeletonPose() : NULL;
				if (pSkeletonPose != NULL)
				{
					pSkeletonPose->SetDefaultPose();
				}

				Physicalize( smartMine, physicsType );

				smartMine.UpdateTacticalIcon();
			}
		}
		break;

	case STATE_EVENT_SMARTMINE_HIDE:
		{
			smartMine.NotifyMineListenersEvent( eMineEventListenerGameObjectEvent_Disabled );
			SendSoundSystemGameHint( eAudioHint_Unload );
		}
		break;

	case STATE_EVENT_SMARTMINE_UNHIDE:
		{
			smartMine.NotifyMineListenersEvent( eMineEventListenerGameObjectEvent_Enabled );
			SendSoundSystemGameHint( eAudioHint_Load );
		}
		break;
	}

	return State_Done;
}

const CSmartMineBehavior::TStateIndex CSmartMineBehavior::Tracking( CSmartMine& smartMine, const SStateEvent& event )
{
	switch ( event.GetEventId() )
	{

	case STATE_EVENT_ENTER:
		{
			smartMine.AddToTacticalManager();
		}
		break;

	case STATE_EVENT_SMARTMINE_ENTITY_ENTERED_AREA:
		{
			const SSmartMineEvent_TriggerEntity& triggerEntityEvent = static_cast<const SSmartMineEvent_TriggerEntity&>(event);
			
			smartMine.StartTrackingEntity( triggerEntityEvent.GetTriggerEntity() );

			if (smartMine.NeedsToKeepTracking())
			{
				return State_Armed;
			}
		}
		break;

	case STATE_EVENT_SMARTMINE_ENTITY_LEFT_AREA:
		{
			const SSmartMineEvent_TriggerEntity& triggerEntityEvent = static_cast<const SSmartMineEvent_TriggerEntity&>(event);

			smartMine.StopTrackingEntity( triggerEntityEvent.GetTriggerEntity() );

			if (!smartMine.NeedsToKeepTracking())
			{
				return State_Waiting;
			}
		}
		break;

	case STATE_EVENT_SMARTMINE_TRIGGER_DETONATE:
		{
			return State_Detonating;
		}
		break;
	}

	return State_Continue;
}

const CSmartMineBehavior::TStateIndex CSmartMineBehavior::Armed( CSmartMine& smartMine, const SStateEvent& event )
{
	switch(event.GetEventId())
	{
	case STATE_EVENT_ENTER:
		{
			EnableSmartMineUpdate( smartMine );
			CAudioSignalPlayer::JustPlay( m_audioSignalIds[eSmartMineSound_ArmedOn], smartMine.GetEntity()->GetWorldPos() );
			//m_loopAudioPlayer.Play( smartMine.GetEntityId() );
		}
		break;

	case STATE_EVENT_EXIT:
		{
			//m_loopAudioPlayer.Stop( );
			CAudioSignalPlayer::JustPlay( m_audioSignalIds[eSmartMineSound_ArmedOff], smartMine.GetEntity()->GetWorldPos() );
		}
		break;

	case STATE_EVENT_SMARTMINE_UPDATE:
		{
			const SSmartMineEvent_Update& updateEvent = static_cast<const SSmartMineEvent_Update&>(event);

			const bool targetNearEnough =  UpdateArmedState( smartMine, updateEvent.GetFrameTime() );
			if (targetNearEnough)
			{
				return State_Detonating;
			}
			else if (smartMine.NeedsToKeepTracking() == false)
			{
				return State_Waiting;
			}
		}
		break;
	}

	return State_Continue;
}

const CSmartMineBehavior::TStateIndex CSmartMineBehavior::Waiting( CSmartMine& smartMine, const SStateEvent& event )
{
	if(event.GetEventId() == STATE_EVENT_ENTER)
	{
		DisableSmartMineUpdate( smartMine );
		SetupLightSource( smartMine );
		UpdateVisualState( smartMine, eVisualState_Armed );
	}

	return State_Continue;
}

const CSmartMineBehavior::TStateIndex CSmartMineBehavior::Detonating( CSmartMine& smartMine, const SStateEvent& event )
{
	switch ( event.GetEventId() )
	{
	case STATE_EVENT_ENTER:
		{
			EnableSmartMineUpdate( smartMine );
			StartDetonation( smartMine );

			m_detonateTimeout = m_properties.m_triggerToDetonateDelay;
		}
		break;

	case STATE_EVENT_EXIT:
		{
			DisableSmartMineUpdate( smartMine );
		}
		break;

	case STATE_EVENT_SMARTMINE_UPDATE:
		{
			const SSmartMineEvent_Update& updateEvent = static_cast<const SSmartMineEvent_Update&>(event);

			const bool exploded = UpdateDetonatingState( smartMine, updateEvent.GetFrameTime() );
			if (exploded)
			{
				return State_Dead;
			}
		}
		break;
	}

	return State_Continue;
}

const CSmartMineBehavior::TStateIndex CSmartMineBehavior::Dead( CSmartMine& smartMine, const SStateEvent& event )
{
	switch ( event.GetEventId() )
	{
	case STATE_EVENT_ENTER:
		{
			smartMine.NotifyMineListenersEvent( eMineEventListenerGameObjectEvent_Destroyed );
			smartMine.RemoveFromTacticalManager();
			smartMine.GetEffectsController().DetachEffect( m_lightEffectId );
			m_lightEffectId = 0;

			smartMine.GetEntity()->Hide( true );
		}
		break;

	case STATE_EVENT_EXIT:
		{
			smartMine.GetEntity()->Hide( false );
		}
		break;
	}

	return State_Continue;
}

//////////////////////////////////////////////////////////////////////////

void CSmartMineBehavior::SetupSmartMine( CSmartMine& smartMine, const SmartMineSetupProperties& smartMineProperties )
{
	// Load model
	stack_string ext(PathUtil::GetExt(smartMineProperties.m_modelName));

	const bool isCharacter = (ext == "cdf") || (ext == "chr") || (ext == "skin") || (ext == "cga");
	if (isCharacter)
	{
		smartMine.GetEntity()->LoadCharacter( SMART_MINE_MODEL_SLOT, smartMineProperties.m_modelName );
		m_modelType = eModelType_AnimatedObject;
	}
	else
	{
		smartMine.GetEntity()->LoadGeometry( SMART_MINE_MODEL_SLOT, smartMineProperties.m_modelName );
		m_modelType = eModelType_StaticObject;
	}

	// Physicalize
	Physicalize( smartMine, m_properties.m_initialPhysicsType );

	// Other proxies
	smartMine.GetEntity()->CreateProxy(ENTITY_PROXY_TRIGGER);
	smartMine.GetEntity()->GetOrCreateComponent<IEntityAudioComponent>();
	
	IEntityTriggerComponent* pTriggerProxy = GetTriggerProxyForSmartMine( smartMine );
	if (pTriggerProxy != NULL)
	{
		const Vec3& triggerBox = smartMineProperties.m_triggerBox;
		AABB localBounds;
		localBounds.min = -triggerBox;
		localBounds.max =  triggerBox;
		pTriggerProxy->SetTriggerBounds( localBounds );
	}

	// Light
	SetupLightSource( smartMine );

	// AI Faction
	if (gEnv->pAISystem != NULL)
	{
		smartMine.SetFaction( gEnv->pAISystem->GetFactionMap().GetFactionID( smartMineProperties.m_faction ) );
	}

	struct 
	{
		ESmartMineSounds sound;
		const char* signalName[eAudioType_Count];
	} tmpSoundTable[eSmartMineSound_Count] = 
	{
		{ eSmartMineSound_ArmedOff, {"CellMine_Armed_Off"} },
		{ eSmartMineSound_ArmedOn,  {"CellMine_Armed_On"} },
		{ eSmartMineSound_ArmedLoop, {"CellMine_Armed_Loop"} },
		{ eSmartMineSound_Detonate, {"CellMine_Detonate"} },
	};

	assert((m_audioType >= 0) && (m_audioType < eAudioType_Count));

	for (int i = 0; i < eSmartMineSound_Count; ++i)
	{
		m_audioSignalIds[i] = g_pGame->GetGameAudio()->GetSignalID( tmpSoundTable[i].signalName[m_audioType] );
	}

	m_loopAudioPlayer.SetSignal( m_audioSignalIds[eSmartMineSound_ArmedLoop] );
}

void CSmartMineBehavior::Physicalize( CSmartMine& smartMine, const int physicalizeType )
{
	if (m_properties.m_currentPhysicsType != physicalizeType)
	{
		SEntityPhysicalizeParams physicalizeParams;
		physicalizeParams.nSlot = SMART_MINE_MODEL_SLOT;
		physicalizeParams.type = physicalizeType;
		physicalizeParams.mass = SMART_MINE_MASS;

		smartMine.GetEntity()->Physicalize( physicalizeParams );

		// Turn off all collisions apart from ray and explosion (bullets + grenades), if it is a rigid body
		if (physicalizeType == PE_RIGID)
		{
			IPhysicalEntity * pMinePhysics = smartMine.GetEntity()->GetPhysics();
			if (pMinePhysics != NULL)
			{
				pe_params_part ignoreCollisions;
				ignoreCollisions.flagsAND = geom_colltype_explosion|geom_colltype_ray;
				ignoreCollisions.flagsColliderOR = geom_no_particle_impulse|geom_no_coll_response;
				pMinePhysics->SetParams(&ignoreCollisions);
			}
		}

		m_properties.m_currentPhysicsType = physicalizeType;
	}
}

void CSmartMineBehavior::PreCacheResources( const SmartMineSetupProperties& smartMineProperties )
{
	g_pGame->GetGameCache().CacheMaterial( smartMineProperties.m_materialArmed );
	g_pGame->GetGameCache().CacheMaterial( smartMineProperties.m_materialDisarmed );

	m_mineMaterials[eVisualState_Armed] = gEnv->p3DEngine->GetMaterialManager()->LoadMaterial( smartMineProperties.m_materialArmed );
	m_mineMaterials[eVisualState_Disarmed] = gEnv->p3DEngine->GetMaterialManager()->LoadMaterial( smartMineProperties.m_materialDisarmed );

	IEntityClass* pAmmoClass = gEnv->pEntitySystem->GetClassRegistry()->FindClass( m_properties.m_explosionType );
	if (pAmmoClass != NULL)
	{
		const SAmmoParams* pAmmoParams = g_pGame->GetWeaponSystem()->GetAmmoParams( pAmmoClass );
		if (pAmmoParams != NULL)
		{
			pAmmoParams->CacheResources();
		}
	}
}

void CSmartMineBehavior::SendSoundSystemGameHint( const EAudioHint audioHint )
{
	/*if (gEnv->IsEditor())
		return;

	assert((m_audioType >= 0) && (m_audioType < eAudioType_Count));

	const char* AUDIO_HINT_NAMES[eAudioType_Count] = { "w_smart_mine" };

	if ((audioHint == eAudioHint_Load) && (m_audioGameHintSent == false))
	{
		gEnv->pSoundSystem->CacheAudioFile( AUDIO_HINT_NAMES[m_audioType], eAFCT_GAME_HINT );
		m_audioGameHintSent = true;
	}
	else if ((audioHint == eAudioHint_Unload) && (m_audioGameHintSent == true))
	{
		gEnv->pSoundSystem->RemoveCachedAudioFile( AUDIO_HINT_NAMES[m_audioType], false );
		m_audioGameHintSent = false;
	}*/
}

void CSmartMineBehavior::SetupLightSource( CSmartMine& smartMine )
{
	if (m_lightEffectId == 0)
	{
		const SmartMinePersistantProperties::LightProperties& lightProperties = m_properties.m_lightProperties[SmartMinePersistantProperties::eLightType_Armed];

		EntityEffects::SLightAttachParams lightAttachParams;
		lightAttachParams.color = lightProperties.diffuseColor;
		lightAttachParams.radius = m_properties.m_detonationRadius * SMART_MINE_NONACTIVE_LIGHT_SCALE;
		lightAttachParams.hdrDynamic = lightProperties.hdrDynamic;
		lightAttachParams.diffuseMultiplier = lightProperties.diffuseMultiplier;
		lightAttachParams.specularMultiplier = lightProperties.specularMultiplier;
		lightAttachParams.deferred = true;

		m_lightEffectId = smartMine.GetEffectsController().AttachLight( SMART_MINE_MODEL_SLOT, "light_attachment", lightAttachParams);
	}
}

IEntityTriggerComponent* CSmartMineBehavior::GetTriggerProxyForSmartMine( CSmartMine& smartMine ) const
{
	return static_cast<IEntityTriggerComponent*>(smartMine.GetEntity()->GetProxy(ENTITY_PROXY_TRIGGER));
}

void CSmartMineBehavior::EnableSmartMineUpdate( CSmartMine& smartMine )
{
	if (smartMine.GetGameObject()->GetUpdateSlotEnables( &smartMine, SMART_MINE_MAIN_UPDATE_SLOT ) == 0)
	{
		smartMine.GetGameObject()->EnableUpdateSlot( &smartMine, SMART_MINE_MAIN_UPDATE_SLOT );
	}
}

void CSmartMineBehavior::DisableSmartMineUpdate( CSmartMine& smartMine )
{
	if (smartMine.GetGameObject()->GetUpdateSlotEnables( &smartMine, SMART_MINE_MAIN_UPDATE_SLOT ) > 0)
	{
		smartMine.GetGameObject()->DisableUpdateSlot( &smartMine, SMART_MINE_MAIN_UPDATE_SLOT );
	}
}

void CSmartMineBehavior::UpdateVisualState( CSmartMine& smartMine, const EVisualState visualState )
{
	CRY_ASSERT( (visualState >= 0) && (visualState < eVisualState_Count) );

	if (m_visualState != visualState)
	{
		IMaterial* pMaterial = m_mineMaterials[visualState];
		if (pMaterial != NULL)
		{
			smartMine.GetEntity()->SetSlotMaterial( SMART_MINE_MODEL_SLOT, pMaterial );
		}

		m_visualState = visualState;
	}

	ILightSource* pLightSource = smartMine.GetEffectsController().GetLightSource( m_lightEffectId );
	if (pLightSource != NULL)
	{
		const SmartMinePersistantProperties::ELightType lightType = (visualState == eVisualState_Armed) ? SmartMinePersistantProperties::eLightType_Armed : SmartMinePersistantProperties::eLightType_Disarmed;
		const SmartMinePersistantProperties::LightProperties& properties = m_properties.m_lightProperties[lightType];

		SRenderLight& lightProperties = pLightSource->GetLightProperties();

		const Vec3 diffuseColor = properties.diffuseColor * properties.diffuseMultiplier;
		const float specularMultiplier = (float)__fsel( -properties.diffuseMultiplier, properties.specularMultiplier, (properties.specularMultiplier / (properties.diffuseMultiplier + FLT_EPSILON)));

		lightProperties.SetLightColor( ColorF(diffuseColor.x, diffuseColor.y, diffuseColor.z , 1.0f) );
		lightProperties.SetSpecularMult( specularMultiplier );

		const float radius = smartMine.NeedsToKeepTracking() ? m_properties.m_detonationRadius : (m_properties.m_detonationRadius * SMART_MINE_NONACTIVE_LIGHT_SCALE);
		lightProperties.SetRadius(radius);
	}

}

bool CSmartMineBehavior::UpdateArmedState( CSmartMine& smartMine, const float frameTime )
{
	//////////////////////////////////////////////////////////////////////////
	// Check if enemy entities are still valid

	CSmartMine::TrackedEntities entitiesToRemove;
	uint32 targetCount = smartMine.GetTrackedEntitiesCount();
	
	for (uint32 i = 0; i < targetCount; ++i)
	{
		const EntityId targetId = smartMine.GetTrackedEntityId( i );
		if (smartMine.ContinueTrackingEntity( targetId ))
			continue;

		entitiesToRemove.push_back( targetId );
	}

	for (uint32 i = 0; i < entitiesToRemove.size(); ++i)
	{
		smartMine.StopTrackingEntity( entitiesToRemove[i] );
	}

	//////////////////////////////////////////////////////////////////////////
	/// Check if it should detonate
	targetCount = smartMine.GetTrackedEntitiesCount();

	bool hostileTargets = false;
	bool friendlyTargets = false;
	bool shouldDetonate = false;

	const Sphere innerSphere( smartMine.GetEntity()->GetWorldPos(), m_properties.m_detonationRadius );

	for (uint32 i = 0; i < targetCount; ++i)
	{
		const EntityId targetId = smartMine.GetTrackedEntityId( i );
		IEntity* pEntity = gEnv->pEntitySystem->GetEntity( targetId );

		if (pEntity != NULL)
		{
			const bool isHostile = smartMine.IsHostileEntity( targetId );
			friendlyTargets |= !isHostile;
			hostileTargets  |=  isHostile;

			const Vec3 targetPosition = pEntity->GetWorldPos();
			if ( PointInShere( targetPosition, innerSphere ) == false )
				continue;

			shouldDetonate |= isHostile;
		}
	}

	shouldDetonate &= !friendlyTargets; //Only if no friendlies around too

	UpdateVisualState( smartMine, !friendlyTargets ? eVisualState_Armed : eVisualState_Disarmed );

	return shouldDetonate;
}

bool CSmartMineBehavior::UpdateDetonatingState( CSmartMine& smartMine, const float frameTime )
{
	const float newTimeout = m_detonateTimeout - frameTime;
	m_detonateTimeout = newTimeout;

	const bool detonate = (newTimeout <= 0.0f);
	if (detonate)
	{
		const Matrix34 mineWorldTM = smartMine.GetEntity()->GetWorldTM();
		const Vec3 upDirection = mineWorldTM.GetColumn2().GetNormalized();

		EntityId explosionOwnerId = smartMine.GetEntityId();
		ScriptHandle detonatedByEntity;
		IScriptTable* pScriptTable = smartMine.GetEntity()->GetScriptTable();
		if((pScriptTable != NULL) && pScriptTable->GetValue( "detonatedByEntity", detonatedByEntity))
		{
			explosionOwnerId = (EntityId)detonatedByEntity.n;
		}

		SShootHelper::Explode( explosionOwnerId, smartMine.GetEntityId(), m_properties.m_explosionType, mineWorldTM.GetTranslation() + (upDirection * 0.25f), upDirection, m_properties.m_damage );

		EntityScripts::CallScriptFunction( smartMine.GetEntity(), smartMine.GetEntity()->GetScriptTable(), "OnExploded" );
	}


	return detonate;
}

void CSmartMineBehavior::StartDetonation( CSmartMine& smartMine )
{
	if (m_modelType == eModelType_AnimatedObject)
	{
		ICharacterInstance* pCharacter = smartMine.GetEntity()->GetCharacter( SMART_MINE_MODEL_SLOT );
		ISkeletonAnim* pSkeletonAnimation = (pCharacter != NULL) ? pCharacter->GetISkeletonAnim() : NULL;

		if (pSkeletonAnimation != NULL)
		{
			CryCharAnimationParams animationParams;
			animationParams.m_nLayerID = 0;

			pSkeletonAnimation->StartAnimation( SMART_MINE_ANIMATION_DETONATE , animationParams );
		}
	}
	else
	{
		Physicalize( smartMine, PE_RIGID );

		if (IPhysicalEntity *pMinePhysics = smartMine.GetEntity()->GetPhysics())
		{
			const float impulse = m_properties.m_jumpImpulse;
			
			pe_action_impulse actionImpulse;
			actionImpulse.impulse = smartMine.GetEntity()->GetWorldTM().GetColumn2() * impulse;
			actionImpulse.iApplyTime = 2;
			pMinePhysics->Action( &actionImpulse );
		}
	}

	UpdateVisualState( smartMine, eVisualState_Armed );

	CAudioSignalPlayer::JustPlay( m_audioSignalIds[eSmartMineSound_Detonate], smartMine.GetEntity()->GetWorldPos() );
}