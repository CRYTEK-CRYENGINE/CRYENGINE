// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

/*************************************************************************
-------------------------------------------------------------------------
$Id$
$DateTime$
Description:  Register the factory templates used to create classes from names
e.g. REGISTER_FACTORY(pFramework, "Player", CPlayer, false);
or   REGISTER_FACTORY(pFramework, "Player", CPlayerG4, false);

Since overriding this function creates template based linker errors,
it's been replaced by a standalone function in its own cpp file.

-------------------------------------------------------------------------
History:
- 17:8:2005   Created by Nick Hesketh - Refactor'd from Game.cpp/h

*************************************************************************/

#include "StdAfx.h"
#include "Game.h"
#include "Player.h"

//
#include "Item.h"
#include "Weapon.h"
#include "VehicleWeapon.h"
#include "VehicleWeaponGuided.h"
#include "VehicleWeaponControlled.h"
#include "VehicleMountedWeapon.h"
#include "Binocular.h"
#include "C4.h"
#include "DebugGun.h"
#include "GunTurret.h"
#include "JAW.h"
#include "AIGrenade.h"
#include "Accessory.h"
#include "HandGrenades.h"
#include "EnvironmentalWeapon.h"
#include "Laser.h"
#include "flashlight.h"
#include "DoubleMagazine.h"
#include "LTAG.h"
#include "HeavyMountedWeapon.h"
#include "HeavyWeapon.h"
#include "PickAndThrowWeapon.h"
#include "NoWeapon.h"
#include "WeaponMelee.h"
#include "UseableTurret.h"
#include "CinematicWeapon.h"

#include "DummyPlayer.h"

#include "ReplayObject.h"
#include "ReplayActor.h"

#include "MultiplayerEntities/CarryEntity.h"

#include "VehicleMovementBase.h"
#include "Vehicle/VehicleMovementDummy.h"
#include "VehicleActionAutomaticDoor.h"
#include "VehicleActionDeployRope.h"
#include "VehicleActionEntityAttachment.h"
#include "VehicleActionLandingGears.h"
#include "VehicleActionAutoTarget.h"
#include "VehicleSeatActionSound.h"
#include "VehicleDamageBehaviorBurn.h"
#include "VehicleDamageBehaviorCameraShake.h"
#include "VehicleDamageBehaviorExplosion.h"
#include "VehicleDamageBehaviorTire.h"
#include "VehicleDamageBehaviorAudioFeedback.h"
#include "VehicleMovementStdWheeled.h"
#include "VehicleMovementStdTank.h"
#include "VehicleMovementArcadeWheeled.h"
//#include "VehicleMovementHelicopterArcade.h"
#include "VehicleMovementHelicopter.h"
#include "VehicleMovementStdBoat.h"
#include "VehicleMovementTank.h"
#include "VehicleMovementMPVTOL.h"

#include "ScriptControlledPhysics.h"

#include "GameRules.h"
#include "GameRulesModules/GameRulesModulesManager.h"
#include "GameRulesModules/IGameRulesTeamsModule.h"
#include "GameRulesModules/GameRulesStandardTwoTeams.h"
#include "GameRulesModules/GameRulesGladiatorTeams.h"
#include "GameRulesModules/IGameRulesStateModule.h"
#include "GameRulesModules/GameRulesStandardState.h"
#include "GameRulesModules/GameRulesStandardVictoryConditionsTeam.h"
#include "GameRulesModules/GameRulesStandardVictoryConditionsPlayer.h"
#include "GameRulesModules/GameRulesObjectiveVictoryConditionsTeam.h"
#include "GameRulesModules/GameRulesObjectiveVictoryConditionsIndividualScore.h"
#include "GameRulesModules/GameRulesExtractionVictoryConditions.h"
#include "GameRulesModules/GameRulesSurvivorOneVictoryConditions.h"
#include "GameRulesModules/GameRulesStandardSetup.h"
#include "GameRulesModules/GameRulesStandardScoring.h"
#include "GameRulesModules/GameRulesAssistScoring.h"
#include "GameRulesModules/GameRulesStandardPlayerStats.h"
#include "GameRulesModules/IGameRulesSpawningModule.h"
#include "GameRulesModules/GameRulesSpawningBase.h"
#include "GameRulesModules/GameRulesMPSpawning.h"
#include "GameRulesModules/GameRulesMPSpawningWithLives.h"
#include "GameRulesModules/GameRulesMPWaveSpawning.h"
#include "GameRulesModules/GameRulesMPDamageHandling.h"
#include "GameRulesModules/GameRulesMPActorAction.h"
#include "GameRulesModules/GameRulesMPSpectator.h"
#include "GameRulesModules/GameRulesSPDamageHandling.h"
#include "GameRulesModules/GameRulesObjective_Predator.h"
#include "GameRulesModules/GameRulesStandardRounds.h"
#include "GameRulesModules/GameRulesStatsRecording.h"
#include "GameRulesModules/GameRulesObjective_PowerStruggle.h"
#include "GameRulesModules/GameRulesObjective_Extraction.h"
#include "GameRulesModules/GameRulesSimpleEntityBasedObjective.h"
#include "GameRulesModules/GameRulesObjective_CTF.h"

#include "Environment/Tornado.h"
#include "Environment/Shake.h"
#include "Environment/Rain.h"
#include "Environment/Snow.h"
#include "Environment/InteractiveObject.h"
#include "Environment/DeflectorShield.h"
#include "Environment/DangerousRigidBody.h"
#include "Environment/Ledge.h"
#include "Environment/WaterPuddle.h"
#include "Environment/SmartMine.h"
#include "Environment/MineField.h"
#include "Environment/DoorPanel.h"
#include "Environment/VicinityDependentObjectMover.h"
#include "Environment/WaterRipplesGenerator.h"
#include "Environment/LightningArc.h"

#include "AI/AICorpse.h"

#include "Turret/Turret/Turret.h"
#include "MPPath.h"

#include <IItemSystem.h>
#include <IVehicleSystem.h>
#include <IGameRulesSystem.h>
#include <CryGame/IGameVolumes.h>

#include "GameCVars.h"

#define HIDE_FROM_EDITOR(className)																																				\
  { IEntityClass *pItemClass = gEnv->pEntitySystem->GetClassRegistry()->FindClass(className);\
  pItemClass->SetFlags(pItemClass->GetFlags() | ECLF_INVISIBLE); }																				\

#define REGISTER_EDITOR_VOLUME_CLASS(frameWork, className)                                          \
{	                                                                                                  \
	IGameVolumes* pGameVolumes = frameWork->GetIGameVolumesManager();                                 \
	IGameVolumesEdit* pGameVolumesEdit = pGameVolumes ? pGameVolumes->GetEditorInterface() : NULL;    \
	if (pGameVolumesEdit != NULL)                                                                     \
	{                                                                                                 \
		pGameVolumesEdit->RegisterEntityClass( className );                                             \
	}                                                                                                 \
} 

#define REGISTER_GAME_OBJECT(framework, name, script)\
	{\
	IEntityClassRegistry::SEntityClassDesc clsDesc;\
	clsDesc.sName = #name;\
	clsDesc.sScriptFile = script;\
struct C##name##Creator : public IGameObjectExtensionCreatorBase\
		{\
		IGameObjectExtension* Create(IEntity *pEntity)\
			{\
				return pEntity->GetOrCreateComponentClass<C##name>();\
			}\
			void GetGameObjectExtensionRMIData( void ** ppRMI, size_t * nCount )\
			{\
			C##name::GetGameObjectExtensionRMIData( ppRMI, nCount );\
			}\
		};\
		static C##name##Creator _creator;\
		framework->GetIGameObjectSystem()->RegisterExtension(#name, &_creator, &clsDesc);\
	}

#define REGISTER_GAME_OBJECT_WITH_IMPL(framework, name, impl, script)\
	{\
	IEntityClassRegistry::SEntityClassDesc clsDesc;\
	clsDesc.sName = #name;\
	clsDesc.sScriptFile = script;\
struct C##name##Creator : public IGameObjectExtensionCreatorBase\
		{\
		IGameObjectExtension* Create(IEntity *pEntity)\
			{\
				return pEntity->GetOrCreateComponentClass<C##impl>();\
			}\
			void GetGameObjectExtensionRMIData( void ** ppRMI, size_t * nCount )\
			{\
			C##impl::GetGameObjectExtensionRMIData( ppRMI, nCount );\
			}\
		};\
		static C##name##Creator _creator;\
		framework->GetIGameObjectSystem()->RegisterExtension(#name, &_creator, &clsDesc);\
	}

#define REGISTER_GAME_OBJECT_EXTENSION(framework, name)\
	{\
struct C##name##Creator : public IGameObjectExtensionCreatorBase\
		{\
		IGameObjectExtension* Create(IEntity *pEntity)\
			{\
				return pEntity->GetOrCreateComponentClass<C##name>();\
			}\
			void GetGameObjectExtensionRMIData( void ** ppRMI, size_t * nCount )\
			{\
			C##name::GetGameObjectExtensionRMIData( ppRMI, nCount );\
			}\
		};\
		static C##name##Creator _creator;\
		framework->GetIGameObjectSystem()->RegisterExtension(#name, &_creator, NULL);\
	}

// Register the factory templates used to create classes from names. Called via CGame::Init()
void InitGameFactory(IGameFramework *pFramework)
{
	assert(pFramework);

	REGISTER_FACTORY(pFramework, "Player", CPlayer, false);
	REGISTER_FACTORY(pFramework, "PlayerHeavy", CPlayer, false);
	
	REGISTER_FACTORY(pFramework, "DamageTestEnt", CPlayer, true);

#if (USE_DEDICATED_INPUT)
	REGISTER_FACTORY(pFramework, "DummyPlayer", CDummyPlayer, true);
#endif

	//REGISTER_FACTORY(pFramework, "Civilian", CPlayer, true);

	// Null AI for AI pool
	REGISTER_FACTORY(pFramework, "NullAI", CPlayer, true);

	// Characters	
	REGISTER_FACTORY(pFramework, "Characters/Human", CPlayer, true);
	
	// Items
	REGISTER_FACTORY(pFramework, "Item", CItem, false);
	REGISTER_FACTORY(pFramework, "Accessory", CAccessory, false);
	REGISTER_FACTORY(pFramework, "Laser", CLaser, false);
	REGISTER_FACTORY(pFramework, "FlashLight", CFlashLight, false);
	REGISTER_FACTORY(pFramework, "DoubleMagazine", CDoubleMagazine, false);
	REGISTER_FACTORY(pFramework, "HandGrenades", CHandGrenades, false);

	// Weapons
	REGISTER_FACTORY(pFramework, "Weapon", CWeapon, false);
	REGISTER_FACTORY(pFramework, "VehicleWeapon", CVehicleWeapon, false);
	REGISTER_FACTORY(pFramework, "VehicleWeaponGuided", CVehicleWeaponGuided, false);
	REGISTER_FACTORY(pFramework, "VehicleWeaponControlled", CVehicleWeaponControlled, false);
	REGISTER_FACTORY(pFramework, "VehicleWeaponPulseC", CVehicleWeaponPulseC, false);
	REGISTER_FACTORY(pFramework, "VehicleMountedWeapon", CVehicleMountedWeapon, false);
	REGISTER_FACTORY(pFramework, "Binocular", CBinocular, false);
	REGISTER_FACTORY(pFramework, "C4", CC4, false);
	REGISTER_FACTORY(pFramework, "DebugGun", CDebugGun, false);
	REGISTER_FACTORY(pFramework, "GunTurret", CGunTurret, false);
	REGISTER_FACTORY(pFramework, "JAW", CJaw, false);
	REGISTER_FACTORY(pFramework, "AIGrenade", CAIGrenade, false);
	REGISTER_FACTORY(pFramework, "AISmokeGrenades", CAIGrenade, false);
	REGISTER_FACTORY(pFramework, "AIEMPGrenade", CAIGrenade, false);
	REGISTER_FACTORY(pFramework, "LTAG", CLTag, false);
	REGISTER_FACTORY(pFramework, "PickAndThrowWeapon", CPickAndThrowWeapon, false);
	REGISTER_FACTORY(pFramework, "NoWeapon", CNoWeapon, false);
	REGISTER_FACTORY(pFramework, "HeavyMountedWeapon", CHeavyMountedWeapon, false);
	REGISTER_FACTORY(pFramework, "HeavyWeapon", CHeavyWeapon, false);
	REGISTER_FACTORY(pFramework, "WeaponMelee", CWeaponMelee, false);
	REGISTER_FACTORY(pFramework, "UseableTurret", CUseableTurret, false);
	REGISTER_FACTORY(pFramework, "CinematicWeapon", CCinematicWeapon, false);
	
	// vehicle objects
	IVehicleSystem* pVehicleSystem = pFramework->GetIVehicleSystem();

#define REGISTER_VEHICLEOBJECT(name, obj) \
	REGISTER_FACTORY((IVehicleSystem*)pVehicleSystem, name, obj, false); \
	obj::m_objectId = pVehicleSystem->AssignVehicleObjectId(name);

	// damage behaviours
	REGISTER_VEHICLEOBJECT("Burn", CVehicleDamageBehaviorBurn);
	REGISTER_VEHICLEOBJECT("CameraShake", CVehicleDamageBehaviorCameraShake);
	REGISTER_VEHICLEOBJECT("Explosion", CVehicleDamageBehaviorExplosion);
	REGISTER_VEHICLEOBJECT("BlowTire", CVehicleDamageBehaviorBlowTire);
	REGISTER_VEHICLEOBJECT("AudioFeedback", CVehicleDamageBehaviorAudioFeedback);

	// actions
	REGISTER_VEHICLEOBJECT("AutomaticDoor", CVehicleActionAutomaticDoor);
	REGISTER_VEHICLEOBJECT("EntityAttachment", CVehicleActionEntityAttachment);
	REGISTER_VEHICLEOBJECT("LandingGears", CVehicleActionLandingGears);
	REGISTER_VEHICLEOBJECT("AutoAimTarget", CVehicleActionAutoTarget);

	//seat actions
	REGISTER_VEHICLEOBJECT("DeployRope", CVehicleActionDeployRope);
	REGISTER_VEHICLEOBJECT("Sound", CVehicleSeatActionSound);

	// vehicle movements
	REGISTER_FACTORY(pVehicleSystem, "DummyMovement", CVehicleMovementDummy, false);
	//REGISTER_FACTORY(pVehicleSystem, "HelicopterArcade", CVehicleMovementHelicopterArcade, false);
	REGISTER_FACTORY(pVehicleSystem, "Helicopter", CVehicleMovementHelicopter, false);
	REGISTER_FACTORY(pVehicleSystem, "StdBoat", CVehicleMovementStdBoat, false);
	REGISTER_FACTORY(pVehicleSystem, "StdWheeled", CVehicleMovementStdWheeled, false);
	REGISTER_FACTORY(pVehicleSystem, "StdTank", CVehicleMovementStdTank, false);
	REGISTER_FACTORY(pVehicleSystem, "ArcadeWheeled", CVehicleMovementArcadeWheeled, false);
	REGISTER_FACTORY(pVehicleSystem, "Tank", CVehicleMovementTank, false);
	REGISTER_FACTORY(pVehicleSystem, "MPVTOL", CVehicleMovementMPVTOL, false);


	// Custom GameObjects
	REGISTER_GAME_OBJECT(pFramework, Tornado, "Scripts/Entities/Environment/Tornado.lua");
	REGISTER_GAME_OBJECT(pFramework, Shake, "Scripts/Entities/Environment/Shake.lua");
	REGISTER_GAME_OBJECT(pFramework, Rain, "Scripts/Entities/Environment/Rain.lua");
	REGISTER_GAME_OBJECT(pFramework, Snow, "Scripts/Entities/Environment/Snow.lua");
	REGISTER_GAME_OBJECT(pFramework, InteractiveObjectEx, "Scripts/Entities/PlayerInteractive/InteractiveObjectEx.lua");
	REGISTER_GAME_OBJECT(pFramework, DeployableBarrier, "Scripts/Entities/PlayerInteractive/DeployableBarrier.lua");
	REGISTER_GAME_OBJECT(pFramework, ReplayObject, "");
	REGISTER_GAME_OBJECT(pFramework, ReplayActor, "");
	REGISTER_GAME_OBJECT(pFramework, DeflectorShield, "Scripts/Entities/Others/DeflectorShield.lua");
	HIDE_FROM_EDITOR("DeflectorShield");
	REGISTER_GAME_OBJECT(pFramework, EnvironmentalWeapon, "Scripts/Entities/Multiplayer/EnvironmentWeapon_Rooted.lua");
	REGISTER_GAME_OBJECT(pFramework, DangerousRigidBody, "Scripts/Entities/Multiplayer/DangerousRigidBody.lua");
	REGISTER_GAME_OBJECT(pFramework, AICorpse, "");
	HIDE_FROM_EDITOR("ReplayObject");
	HIDE_FROM_EDITOR("ReplayActor");
	HIDE_FROM_EDITOR("AICorpse");
	HIDE_FROM_EDITOR("NullAI");

	//////////////////////////////////////////////////////////////////////////
	/// Shape/Volume objects
	REGISTER_GAME_OBJECT(pFramework, MPPath, "Scripts/Entities/Multiplayer/MPPath.lua");
	HIDE_FROM_EDITOR("MPPath");
	REGISTER_EDITOR_VOLUME_CLASS( pFramework, "MPPath" );

	REGISTER_GAME_OBJECT(pFramework, LedgeObject, "Scripts/Entities/ContextualNavigation/LedgeObject.lua");
	HIDE_FROM_EDITOR("LedgeObject");
	REGISTER_EDITOR_VOLUME_CLASS( pFramework, "LedgeObject" );
	REGISTER_GAME_OBJECT(pFramework, LedgeObjectStatic, "Scripts/Entities/ContextualNavigation/LedgeObjectStatic.lua");
	HIDE_FROM_EDITOR("LedgeObjectStatic");
	REGISTER_EDITOR_VOLUME_CLASS( pFramework, "LedgeObjectStatic" );

	REGISTER_GAME_OBJECT(pFramework, WaterPuddle, "Scripts/Entities/Environment/WaterPuddle.lua");
	HIDE_FROM_EDITOR("WaterPuddle");
	REGISTER_EDITOR_VOLUME_CLASS(pFramework, "WaterPuddle");
	//////////////////////////////////////////////////////////////////////////


	REGISTER_GAME_OBJECT(pFramework, SmartMine, "Scripts/Entities/Environment/SmartMine.lua");
	REGISTER_GAME_OBJECT(pFramework, MineField, "Scripts/Entities/Environment/MineField.lua");
	REGISTER_GAME_OBJECT(pFramework, DoorPanel, "Scripts/Entities/Environment/DoorPanel.lua");
	REGISTER_GAME_OBJECT(pFramework, VicinityDependentObjectMover, "Scripts/Entities/Environment/VicinityDependentObjectMover.lua");
	REGISTER_GAME_OBJECT(pFramework, WaterRipplesGenerator, "Scripts/Entities/Environment/WaterRipplesGenerator.lua");
	REGISTER_GAME_OBJECT(pFramework, LightningArc, "Scripts/Entities/Environment/LightningArc.lua");

	REGISTER_GAME_OBJECT_WITH_IMPL(pFramework, CTFFlag, CarryEntity, "Scripts/Entities/Multiplayer/CTFFlag.lua");
	
	REGISTER_GAME_OBJECT_WITH_IMPL(pFramework, Turret, Turret, "Scripts/Entities/Turret/Turret.lua");
	
	REGISTER_GAME_OBJECT_EXTENSION(pFramework, ScriptControlledPhysics);

	HIDE_FROM_EDITOR("CTFFlag");
	IEntityClassRegistry::SEntityClassDesc stdClass;
	stdClass.flags |= ECLF_INVISIBLE;
	stdClass.sName = "Corpse";
	gEnv->pEntitySystem->GetClassRegistry()->RegisterStdClass(stdClass);

	//GameRules
	REGISTER_FACTORY(pFramework, "GameRules", CGameRules, false);

	IGameRulesModulesManager *pGameRulesModulesManager = CGameRulesModulesManager::GetInstance();

	REGISTER_FACTORY(pGameRulesModulesManager, "StandardTwoTeams", CGameRulesStandardTwoTeams, false);
	REGISTER_FACTORY(pGameRulesModulesManager, "GladiatorTeams", CGameRulesGladiatorTeams, false);
	REGISTER_FACTORY(pGameRulesModulesManager, "StandardState", CGameRulesStandardState, false);
	REGISTER_FACTORY(pGameRulesModulesManager, "StandardVictoryConditionsTeam", CGameRulesStandardVictoryConditionsTeam, false);
	REGISTER_FACTORY(pGameRulesModulesManager, "StandardVictoryConditionsPlayer", CGameRulesStandardVictoryConditionsPlayer, false);
	REGISTER_FACTORY(pGameRulesModulesManager, "ObjectiveVictoryConditionsTeam", CGameRulesObjectiveVictoryConditionsTeam, false);
	REGISTER_FACTORY(pGameRulesModulesManager, "ObjectiveVictoryConditionsIndiv", CGameRulesObjectiveVictoryConditionsIndividualScore, false);
	REGISTER_FACTORY(pGameRulesModulesManager, "ExtractionVictoryConditions", CGameRulesExtractionVictoryConditions, false);
#if SURVIVOR_ONE_ENABLED
	REGISTER_FACTORY(pGameRulesModulesManager, "SurvivorOneVictoryConditions", CGameRulesSurvivorOneVictoryConditions, false);
#endif
	REGISTER_FACTORY(pGameRulesModulesManager, "StandardSetup", CGameRulesStandardSetup, false);
	REGISTER_FACTORY(pGameRulesModulesManager, "StandardScoring", CGameRulesStandardScoring, false);
	REGISTER_FACTORY(pGameRulesModulesManager, "AssistScoring", CGameRulesAssistScoring, false);
	REGISTER_FACTORY(pGameRulesModulesManager, "StandardPlayerStats", CGameRulesStandardPlayerStats, false);
	REGISTER_FACTORY(pGameRulesModulesManager, "SpawningBase", CGameRulesSpawningBase, false);
	REGISTER_FACTORY(pGameRulesModulesManager, "MPRSSpawning", CGameRulesRSSpawning, false);
	REGISTER_FACTORY(pGameRulesModulesManager, "StandardStatsRecording", CGameRulesStatsRecording, false);
	REGISTER_FACTORY(pGameRulesModulesManager, "MPSpawningWithLives", CGameRulesMPSpawningWithLives, false);
	REGISTER_FACTORY(pGameRulesModulesManager, "MPWaveSpawning", CGameRulesMPWaveSpawning, false);
	REGISTER_FACTORY(pGameRulesModulesManager, "MPDamageHandling", CGameRulesMPDamageHandling, false);
	REGISTER_FACTORY(pGameRulesModulesManager, "MPActorAction", CGameRulesMPActorAction, false);
	REGISTER_FACTORY(pGameRulesModulesManager, "MPSpectator", CGameRulesMPSpectator, false);
	REGISTER_FACTORY(pGameRulesModulesManager, "SPDamageHandling", CGameRulesSPDamageHandling, false);
	REGISTER_FACTORY(pGameRulesModulesManager, "Objective_Predator", CGameRulesObjective_Predator, false);
	REGISTER_FACTORY(pGameRulesModulesManager, "StandardRounds", CGameRulesStandardRounds, false);
	REGISTER_FACTORY(pGameRulesModulesManager, "Objective_PowerStruggle", CGameRulesObjective_PowerStruggle, false);
	REGISTER_FACTORY(pGameRulesModulesManager, "Objective_Extraction", CGameRulesObjective_Extraction, false);
	REGISTER_FACTORY(pGameRulesModulesManager, "Objective_SimpleEntityBased", CGameRulesSimpleEntityBasedObjective, false);
	REGISTER_FACTORY(pGameRulesModulesManager, "Objective_CTF", CGameRulesObjective_CTF, false);

	pGameRulesModulesManager->Init();
}
