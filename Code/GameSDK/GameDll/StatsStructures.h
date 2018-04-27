// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

/*************************************************************************
-------------------------------------------------------------------------

Description:		Declares Structures and Enums needed for stats gathering/storing

History:
- 18:04:2012		Created by Andrew Blackwell
*************************************************************************/

#ifndef __STATSSTRUCTURES_H__
#define __STATSSTRUCTURES_H__

#include <CryString/CryFixedString.h>
#include "AutoEnum.h"
#include <CryCore/TypeInfo_impl.h>


//////////////////////////////////////////////////////////////////////////
// Declaration of enums and names of stats and macros to make it easier to deal with them

#define IntPersistantStats(f) \
	f(EIPS_KillsSuitDefault, eSF_RemoteClients, "Kills in default suit mode") \
	f(EIPS_KillsSuitArmor, eSF_RemoteClients, "Kills in armor suit mode") \
	f(EIPS_KillsSuitStealth, eSF_RemoteClients, "Kills in stealth suit mode") \
	f(EIPS_KillsNoSuit, eSF_RemoteClients, "kills without a nanosuit") \
	\
	f(EIPS_DeathsSuitDefault, eSF_RemoteClients, "Deaths in default suit mode") \
	f(EIPS_DeathsSuitArmor, eSF_RemoteClients, "Deaths in armor mode") \
	f(EIPS_DeathsSuitStealth, eSF_RemoteClients, "Deaths in stealth mode") \
	f(EIPS_DeathsNoSuit, eSF_RemoteClients, "Deaths without a nanosuit") \
	\
	f(EIPS_Suicides, eSF_RemoteClients, "Suicides") \
	f(EIPS_SuicidesByFalling, eSF_RemoteClients, "Suicides by falling") \
	f(EIPS_SuicidesByFallingCollided, eSF_RemoteClients, "Suicides by falling on Collided buildings") \
	f(EIPS_SuicidesByFrag, eSF_RemoteClients, "Suicides by frag grenades") \
	\
	f(EIPS_TeamRadar, eSF_LocalClient, "Times activating team radar") \
	f(EIPS_MicrowaveBeam, eSF_LocalClient, "Times activating microwave beam") \
	f(EIPS_SuitBoost, eSF_LocalClient, "Times activating suit boost") \
	f(EIPS_HealthRestore, eSF_LocalClient, "Regenerated upto full health") \
	f(EIPS_5HealthRestoresInOneLife, eSF_LocalClient, "Regenerated upto full health 5 times in 1 life") \
	f(EIPS_SuitBoostKills, eSF_LocalClient, "Kills with suit boost active") \
	f(EIPS_MicrowaveBeamKills, eSF_LocalClient, "kills with the microwave beam") \
	f(EIPS_KillsSuitSupercharged, eSF_LocalClient, "Kills with suit supercharged") \
	f(EIPS_PowerStompKills, eSF_LocalClient, "Kills with power stomp attack") \
	\
	f(EIPS_SkillKills, eSF_LocalClient, "Skill kill kills! (Any skill kill such as First blood, air kill or kill joy)") \
	f(EIPS_StealthKills, eSF_LocalClient, "Stealth kills (cloaked takedown from behind) (skill kill)") \
	f(EIPS_StealthKillsWithSPModuleEnabled, eSF_LocalClient, "Stealth kills (cloaked takedown from behind) while the SP suit module is enabled") \
	f(EIPS_StealthKillsNotDetectedByAI, eSF_LocalClient, "Stealth kills (cloaked takedown from behind) non detected by AI (SP only)") \
	f(EIPS_AirKillKills, eSF_LocalClient, "Victim was in the air when killed") \
	f(EIPS_AirDeathKills, eSF_LocalClient, "Victim was in the air when killed (skill kill) (only happen if another more important skill kill doesn't happen)") \
	f(EIPS_FirstBlood, eSF_LocalClient, "First kill of the game (skill kill)") \
	f(EIPS_NanosuitUpgradePoints, eSF_LocalClient, "Nanosuit upgrade points (SP only)") \
	f(EIPS_NanosuitUpgradePointsAccumulated, eSF_LocalClient, "Total nanosuit upgrade points ever accumulated (SP only)") \
	f(EIPS_RunOver, eSF_LocalClient, "Killed a player by running them over in a vehicle") \
	f(EIPS_ScanObject, eSF_LocalClient, "Scanned objects (SP only)") \
	f(EIPS_GrabAndThrow, eSF_LocalClient, "Player grab and throw object (SP only)") \
	f(EIPS_ThrownObjectKill, eSF_LocalClient, "Killed a player by throwing an object") \
	f(EIPS_ThrownObjectKillWithoutAlertingAI, eSF_LocalClient, "Killed a player by throwing an object without alerting") \
	f(EIPS_KillJoyKills, eSF_LocalClient, "Killed a player on a kill streak of 3 or more (skill kill)") \
	f(EIPS_BlindKills, eSF_LocalClient, "Kills when you were flashbanged (skill kill)") \
	f(EIPS_RumbledKills, eSF_LocalClient, "Kills a player while they were cloaked (skill kill)") \
	f(EIPS_NearDeathExperienceKills, eSF_LocalClient, "Kill when you have been on low health for a second or more (skill kill)") \
	f(EIPS_MeleeTakeDownKills, eSF_LocalClient, "Melee kills (skill kill)") \
	f(EIPS_MeleeTakeDownKillsNoAmmo, eSF_LocalClient, "Melee kill when you have no ammo in your selected weapon") \
	f(EIPS_PowerMeleeTakeDownKills, eSF_LocalClient, "Power Melee kills (skill kill)(SP only?)") \
	f(EIPS_HeadShotKills, eSF_LocalClient, "headshot kill (skill kill)") \
	f(EIPS_DoubleKills, eSF_LocalClient, "Kill 2 people within 5 seconds between each kill (skill kill)") \
	f(EIPS_TripleKills, eSF_LocalClient, "Kill 3 people within 5 seconds between each kill (skill kill)") \
	f(EIPS_QuadKills, eSF_LocalClient, "Kill 4 people within 5 seconds between each kill (skill kill)") \
	f(EIPS_QuinKills, eSF_LocalClient, "Kill 5 people within 5 seconds between each kill (skill kill)") \
	f(EIPS_GotYourBackKills, eSF_LocalClient, "Kill someone when they were aiming at a teammate (skill kill)") \
	f(EIPS_PiercingKills, eSF_LocalClient, "Kill someone by shooting through an object (skill kill)") \
	f(EIPS_GuardianKills, eSF_LocalClient, "Kill someone who recently damage a teammate (skill kill)") \
	f(EIPS_BlindingKills, eSF_LocalClient, "Kill someone after you have flashbanged them (with in 5 seconds)(skill kill)") \
	f(EIPS_FlushedKills, eSF_LocalClient, "Kill someone after you have thrown a grenade at them and they have moved (skill kill)") \
	f(EIPS_DualWeaponKills, eSF_LocalClient, "Kill someone using 2 weapons (skill kill)") \
	f(EIPS_InterventionKills, eSF_LocalClient, "Kill someone who is ironsighting/zooming at a teammate (within 1 second) (skill kill)") \
	f(EIPS_KickedCarKills, eSF_LocalClient, "Killed someone by kicking a car into them (skill kill)") \
	\
	f(EIPS_KillAssists, eSF_LocalClient, "Kill Assists") \
	f(EIPS_KillsWithoutAssist, eSF_RemoteClients, "Kills without assists") \
	f(EIPS_FlagCaptures, eSF_LocalClient, "Flag captures (or relay)") \
	f(EIPS_TakeLateFlagCaptureLead, eSF_LocalClient, "Flag capture in the last 10 seconds that puts your team into the lead") \
	f(EIPS_FlagCarrierKills, eSF_LocalClient, "Flag carriers killed") \
	f(EIPS_WonCTFWithoutGivingUpAScore, eSF_LocalClient, "Win CTF without letting the other team score") \
	f(EIPS_WonExtractDefendingNoGiveUp, eSF_LocalClient, "Win extraction when defending without letting the other team score") \
	f(EIPS_CaptureObjectives, eSF_LocalClient, "Captured an objective") \
	f(EIPS_CarryObjectives, eSF_LocalClient, "Extraction ticks extracted") \
	f(EIPS_BombTheBase, eSF_LocalClient, "Bomb the base completed(No longer a supported mode)") \
	f(EIPS_CrashSiteAttackingKills, eSF_LocalClient, "Kill an attacker at a crashsite") \
	f(EIPS_CrashSiteDefendingKills, eSF_LocalClient, "Kill an attacker when defending a crashsite") \
	f(EIPS_AssaultDefendingKills, eSF_LocalClient, "Kill a player while defending in assault") \
	f(EIPS_ExtractionDefendingKills, eSF_LocalClient, "Kill a player while defending in extraction") \
	f(EIPS_KilledAllEnemies, eSF_LocalClient, "Killed every member of the enemy team") \
	f(EIPS_KilledAllEnemiesNotDied, eSF_LocalClient,  "Killed every member of the enemy team and not died") \
	f(EIPS_TaggedAndBagged, eSF_LocalClient, "tagged a player, kill then and then teabagged them") \
	\
	f(EIPS_CrouchedKills, eSF_LocalClient, "Kills while crouched") \
	f(EIPS_CrouchedMeleeKills, eSF_LocalClient, "Melee kills while crouched") \
	f(EIPS_BlindSelf, eSF_LocalClient, "Times flashbanged yourself") \
	f(EIPS_RipOffMountedWeapon, eSF_LocalClient, "Times ripped off a mounted weapon (e.g HMG)") \
	f(EIPS_SnipedFoot, eSF_LocalClient, "Times sniped someone in the foot") \
	f(EIPS_VictimOnFinalKillcam, eSF_LocalClient, "Victim on final killcam") \
	f(EIPS_FinalIntel5SecRemaining, eSF_LocalClient, "Finish collecting intel with in the last 5 seconds") \
	f(EIPS_KillAllAssaultAttackers, eSF_LocalClient, "Kill all assault attackers") \
	f(EIPS_AssaultKillLastAttack5pc, eSF_LocalClient, "Kill the last attacker when they are on 10% of less intel") \
	f(EIPS_GameOverAfter2Before4, eSF_LocalClient, "Finish a game between 2am and 4am") \
	f(EIPS_CloakedWatchNearbyKill, eSF_LocalClient, "cloaked, seeing someone else being killed withinn 8m and facing") \
	f(EIPS_NumCloakedVictimKills, eSF_LocalClient, "Killed a player who was cloaked 2 seconds ago") \
	f(EIPS_CrouchingOverCorpses, eSF_LocalClient, "Corpses teabagged") \
	f(EIPS_OneHitArmorMeleeKills, eSF_LocalClient, "Melee kills in armor mode") \
	f(EIPS_LedgeGrabs, eSF_LocalClient, "ledge grabs") \
	f(EIPS_TaggedEntities, eSF_LocalClient, "Tagged entities (MP)") \
	f(EIPS_TaggedEntitiesSP, eSF_LocalClient, "Tagged entities (SP)") \
	f(EIPS_TaggedEntitiesWithThreatAwareness, eSF_LocalClient, "Tagged entities with threat awareness on(SP only?)") \
	f(EIPS_TaggedEntitiesWithSensorPackage, eSF_LocalClient, "Tagged entities with sensor package on(SP only?)") \
	f(EIPS_TagAssist, eSF_LocalClient, "tag kill assists") \
	\
	f(EIPS_CompleteOnlineMatches, eSF_LocalClient, "Completed online matches") \
	\
	f(EIPS_ArmorActivations, eSF_LocalClient, "Times switched to armor mode (minimum of 3 seconds between changes)") \
	f(EIPS_StealthActivations, eSF_LocalClient, "Times switched to armor mode (minimum of 3 seconds between changes)") \
	f(EIPS_ArmourHits, eSF_LocalClient, "Hit while you were in armor mode") \
	f(EIPS_InAirDeaths, eSF_LocalClient, "Deaths while you were in the air") \
	f(EIPS_InAirGrenadeKills, eSF_LocalClient, "Grenades killed an enemy while they were in the air") \
	f(EIPS_3FastGrenadeKills, eSF_LocalClient, "3 grenade kills") \
	f(EIPS_MeleeDeaths, eSF_LocalClient, "Killed by being melee hit") \
	f(EIPS_WinningKill, eSF_LocalClient, "Winning kill of the game") \
	f(EIPS_WinningKillAndFirstBlood, eSF_LocalClient, "First blood and Winning kill of the game") \
	f(EIPS_FriendlyFires, eSF_LocalClient, "Friendly fire hits") \
	f(EIPS_LoneWolfKills, eSF_LocalClient, "Kills without a teammate within 15m") \
	f(EIPS_WarBirdKills, eSF_LocalClient, "Killing within 5 seconds of uncloaking") \
	f(EIPS_SafetyInNumbersKills, eSF_LocalClient, "Kills with a teammate within 15m") \
	f(EIPS_GrenadeSurvivals, eSF_LocalClient, "Hit by a grenade but not killed") \
	f(EIPS_Groinhits, eSF_LocalClient, "Hit another player in the groin") \
	f(EIPS_CloakedReloads, eSF_LocalClient, "Reloads while cloaked") \
	f(EIPS_BulletPenetrationKills, eSF_LocalClient, "Kill someone by shooting through an object") \
	f(EIPS_ShotsInMyBack, eSF_LocalClient, "Times you were shot in the back") \
	f(EIPS_ShotInBack, eSF_LocalClient, "Times you shot someone in the back") \
	f(EIPS_MountedKills, eSF_LocalClient, "Kills with a mounted weapon (mounted HMG)") \
	f(EIPS_UnmountedKills, eSF_LocalClient, "Kills with a ripped off mounted weapon (ripped off HMG)") \
	f(EIPS_RecoveryKills, eSF_LocalClient, "Kill while you have the fatality bonus on") \
	f(EIPS_RetaliationKills, eSF_LocalClient, "Kill the player who last killed you") \
	\
	f(EIPS_AirHeadshots, eSF_LocalClient, "Headshot kills while the target was in the air") \
	\
	f(EIPS_SuppressorKills, eSF_LocalClient, "Kills with a silencer attached") \
	f(EIPS_ReflexSightKills, eSF_LocalClient, "Ironsight kills with a reflex attachment") \
	f(EIPS_LaserSightKills, eSF_LocalClient, "Kills with a laser sight attachment") \
	f(EIPS_AssaultScopeKills, eSF_LocalClient, "Ironsight/zoomed kills with an assault scope") \
	\
	f(EIPS_GrenadeLauncherKills, eSF_LocalClient, "kills with the grenade launcher attachment") \
	f(EIPS_SlidingKills, eSF_LocalClient, "Kills while sliding") \
	f(EIPS_GameComplete, eSF_LocalClient, "Completed the game stat (every skill assessment and all unlocks)") \
	\
	f(EIPS_Achievements1, eSF_LocalClient, "Bit field of achievements(Not stored online, not used?)") \
	f(EIPS_Achievements2, eSF_LocalClient, "Bit field of achievements(Not stored online, not used?)") \
	\
	f(EIPS_Overall, eSF_LocalClient, "Seconds running the game (Saved online via misc stats, not general)") \
	f(EIPS_LobbyTime, eSF_LocalClient, "Seconds in the lobby (Saved online via misc stats, not general)") \
	\
	f(EIPS_SuitDefaultTime, eSF_LocalClient, "Seconds in default suit mode") \
	f(EIPS_SuitArmorTime, eSF_LocalClient, "Seconds in armor mode") \
	f(EIPS_SuitStealthTime, eSF_LocalClient, "Seconds in Stealth mode") \
	f(EIPS_NoSuitTime, eSF_LocalClient, "Seconds in without a nanosuit") \
	\
	f(EIPS_VisorActiveTime, eSF_LocalClient, "Seconds visor is active") \
	f(EIPS_TimePlayed, eSF_LocalClient, "Seconds in game") \
	\
	f(EIPS_TimeCrouched, eSF_LocalClient, "Seconds crouched") \
	f(EIPS_CloakedNearEnemy, eSF_LocalClient, "Seconds cloaked near an enemy") \
	\
	f(EIPS_GrabbedEnemies, eSF_LocalClient, "Number of grabbed enemy AIs (SP only)") \
	f(EIPS_GrabbedEnemiesWithSuperStrenght, eSF_LocalClient, "Number of grabbed enemy AIs with Super Strength module active (SP only)") \
	f(EIPS_Kicks, eSF_LocalClient, "Number of performed kicks (SP only)") \
	f(EIPS_BruteForceKickKills, eSF_LocalClient, "Number of enemies killed with a kicked object with BruteForce module (SP only)") \
	f(EIPS_ArmorEfficiency_DamageAbsorbed, eSF_LocalClient, "Amount of damage absorbed by armor mode with ArmorEfficiency on (SP only)")\
	f(EIPS_HeavyArmor_DamageAbsorbed, eSF_LocalClient, "Amount of damage absorbed by armor mode with HeavyArmor on (SP only)")\
	f(EIPS_Deflection_DamageAbsorbed, eSF_LocalClient, "Amount of damage absorbed by armor mode with Deflection on (SP only)")\
	\
	f(EIPS_TutorialAlreadyPlayed, eSF_LocalClient, "If the tutorial has already been played (SP only)")\
	\
	f(EIPS_RunOnceVersion, eSF_LocalClient, "What version of RunOnce have we executed up to") \
	f(EIPS_RunOnceTrackingVersion, eSF_LocalClient, "What tracking version of RunOnce have we executed") \
	f(EIPS_TimeInVTOLs, eSF_LocalClient, "Amount of time spent in VTOLs" ) \
	f(EIPS_MountedVTOLKills, eSF_LocalClient, "Kills with a VTOL mounted HMG") \
	f(EIPS_ThrownWeaponKills, eSF_LocalClient, "Kills with a thrown environmental weapon") \
	f(EIPS_HunterHideAndSeek, eSF_LocalClient, "Didn't die as a marine through entire Hunter match") \
	f(EIPS_TheHunter, eSF_LocalClient, "Killed all marines in a round of Hunter") \
	\
	f(EIPS_XPBonusArmor, eSF_LocalClient, "Additional XP % gained with XP Bonus Armor") \
	f(EIPS_XPBonusStealth, eSF_LocalClient, "Additional XP % gained with XP Bonus Stealth") \
	f(EIPS_XPBonusPower, eSF_LocalClient, "Additional XP % gained with XP Bonus Power") \
	f(EIPS_HeavyWeaponKills, eSF_LocalClient, "Kills with a heavy weapon") \
	f(EIPS_MarineKillsAsHunter, eSF_LocalClient, "Number of marines killed while you are a hunter in hunter gamemode") \
	\
	f(EIPS_SwarmerActivations, eSF_LocalClient, "Number of times the Swarmer has been activated") \
	f(EIPS_EMPBlastActivations, eSF_LocalClient, "Number of times the EMP Blast has been activated") \
	f(EIPS_HunterKillsAsMarine, eSF_LocalClient, "Number of hunters killed as a marine") \
	f(EIPS_SpearShieldKills, eSF_LocalClient, "Number of kills with theSpear shield") \
	f(EIPS_AllSpearsCaptured, eSF_LocalClient, "Number of times all 3 spears have been captures") \
	f(EIPS_EmergencyStat1, eSF_LocalClient, "For future use") \
	f(EIPS_EmergencyStat2, eSF_LocalClient, "For future use") \
	/*NOTE: the following stats are currently only used for chance challenges and dont necessarily need syncing*/ \
	f(EIPS_PlayersKilledWithTheirGun, eSF_LocalClient, "Number of players killed with their own dropped weapon") \
	f(EIPS_KillsWithTheirThrownPole, eSF_LocalClient, "Number of players killed with their own thrown pole") \
	f(EIPS_KillsWithTheirThrownShield, eSF_LocalClient, "Number of players killed with their own thrown shield") \
	f(EIPS_DoubleKillsWithPole, eSF_LocalClient, "Number of double kills with a pole") \
	f(EIPS_TripleKillswithPole, eSF_LocalClient, "Number of triple kills with a pole") \
	f(EIPS_DoubleKillsWithPanel, eSF_LocalClient, "Number of double kills with a panel") \
	f(EIPS_TripleKillswithPanel, eSF_LocalClient, "Number of triple kills with a panel") \
	f(EIPS_VTOLsDestroyed, eSF_LocalClient, "Number of VTOLs destroyed") \
	f(EIPS_VTOLsDestroyedOrbitalStrike, eSF_LocalClient, "Number of VTOLS destroyed with the orbital strike") \
	f(EIPS_PlayersInVTOLKilled, eSF_LocalClient, "Number of players in a VTOL killed") \
	f(EIPS_TaggedEntireEnemyTeam, eSF_LocalClient, "Number of matches the player has tagged all enemy players") \
	f(EIPS_EnemyTeamKilledInXSeconds, eSF_LocalClient, "Number of matches the player has killed all the enemy team in X seconds") \
	f(EIPS_C4AttachedToTeamMateKills, eSF_LocalClient, "Number of players killed by exploding C4 currently stuck to your team mate") \
	f(EIPS_ThrownObjectDoubleKills, eSF_LocalClient, "Number of Double kills with one throw of environmental weapons" ) \
	f(EIPS_ThrownObjectDistantKills, eSF_LocalClient, "Number of kills with environmental weapons thrown over a certain distance" ) \
	f(EIPS_AirStompDoubleKills, eSF_LocalClient, "Number of Double Kills with Airstomp" ) \
	f(EIPS_AirStompTripleKills, eSF_LocalClient, "Number of Triple Kills with Airstomp" ) \
	f(EIPS_EmergencyStat3, eSF_LocalClient, "Kills of vtols with enemies in" ) \
	f(EIPS_EmergencyStat4, eSF_LocalClient, "If we need it" ) \
	
AUTOENUM_BUILDENUMWITHTYPE_WITHINVALID_WITHNUM(EIntPersistantStats, IntPersistantStats, EIPS_Invalid, EIPS_Max);

#define FloatPersistantStats(f) \
	f(EFPS_DistanceAir, eSF_LocalClient ,"Distance travelled in the air") \
	f(EFPS_DistanceRan, eSF_LocalClient, "Distance ran (includes sprinting, but not sliding)") \
	f(EFPS_DistanceSprint, eSF_LocalClient, "Distance travelled sprinting") \
	f(EFPS_DistanceSlid, eSF_LocalClient, "Distance travelled sliding") \
	\
	f(EFPS_EnergyUsed, eSF_LocalClient, "Nanosuit energy used") \
	f(EFPS_DamageTaken, eSF_LocalClient, "Damage taken") \
	f(EFPS_DamageDelt, eSF_LocalClient, "Damage delt to other players") \
	f(EFPS_FlagCarriedTime, eSF_RemoteClients, "Time carried the flag (or relay)") \
	f(EFPS_KillCamTime, eSF_LocalClient, "Time watching the killcam") \
	\
	f(EFPS_CrashSiteHeldTime, eSF_LocalClient, "Time held the crashsite") \
	f(EFPS_IntelCollectedTime, eSF_LocalClient, "Time collecting intel for assault") \
	f(EFPS_FallDistance, eSF_LocalClient, "Distance fallen (and taken damage)") \
	\
	f(EFPS_DistanceSwum, eSF_LocalClient, "Distance Swum" )\
	f(EFPS_DamageToFlyers, eSF_LocalClient, "Damage delt to flying Vehicles" ) \
	f(EFPS_WallOfSteel, eSF_LocalClient, "Damage absorbed by shield panels" ) \
	

AUTOENUM_BUILDENUMWITHTYPE_WITHINVALID_WITHNUM(EFloatPersistantStats, FloatPersistantStats, EFPS_Invalid, EFPS_Max);

#define StreakIntPersistantStats(f) \
	f(ESIPS_Kills, eSF_RemoteClients, "Longest killstreak") \
	f(ESIPS_KillsNoReloadWeapChange, eSF_LocalClient, "Most kills without reloading (or changing weapon)") \
	f(ESIPS_Deaths, eSF_RemoteClients, "Longest death streak") \
	f(ESIPS_Win, eSF_LocalClient|eSF_StreakMultiSession, "longest win streak") \
	f(ESIPS_OnlineRankedWin, eSF_LocalClient|eSF_StreakMultiSession, "Longest win streak (online ranked games)") \
	f(ESIPS_Lose, eSF_LocalClient|eSF_StreakMultiSession, "Longest losing streak") \
	f(ESIPS_Headshots, eSF_LocalClient, "Longest headshot streak (bullets - not kills)") \
	f(ESIPS_AssaultAttackersKilled, eSF_LocalClient, "Largest number of assault attackers killed (in a round)") \
	f(ESIPS_GameOverLateAndLowScore, eSF_LocalClient, "Number of games when playing between 23:30 and before 05:30 and coming in the bottom 1/3 of the score board in a row") \
	f(ESIPS_MultiKillStreak, eSF_RemoteClients, "Largest multikill streak (killing people within 5 seconds of each other)") \
	f(ESIPS_HeadshotsPerLife, eSF_LocalClient, "Most headshots per life (bullets - not kills") \
	f(ESIPS_HeadshotKillsPerLife, eSF_LocalClient, "Most headshot kills per life") \
	f(ESIPS_HeadshotKillsPerMatch, eSF_LocalClient, "Most headshot kills per match") \
	f(ESIPS_HealthRestoresPerLife, eSF_LocalClient, "Most health restores per life") \
	f(ESIPS_MeleeKillsThisSession, eSF_LocalClient, "Most Melee kills") \
	f(ESIPS_TimeAlive, eSF_LocalClient, "Longest time spent alive") \


AUTOENUM_BUILDENUMWITHTYPE_WITHINVALID_WITHNUM(EStreakIntPersistantStats, StreakIntPersistantStats, ESIPS_Invalid, ESIPS_Max);

#define StreakFloatPersistantStats(f) \
	f(ESFPS_DistanceAir, eSF_LocalClient, "Longest distance travelled in the air (without hitting the ground)") \
	f(ESFPS_HeightOnRooftops, eSF_LocalClient, "Highest height you've been to rooftop gardens") \

AUTOENUM_BUILDENUMWITHTYPE_WITHINVALID_WITHNUM(EStreakFloatPersistantStats, StreakFloatPersistantStats, ESFPS_Invalid, ESFPS_Max);

#define MapPersistantStats(f) \
	f(EMPS_Gamemodes, eSF_LocalClient|eSF_MapParamGameRules, "Games entered (by gamemode)") \
	f(EMPS_GamemodesMVP, eSF_LocalClient|eSF_MapParamGameRules, "Games you were the most valuble player (by gamemode)") \
	f(EMPS_GamemodesHighscore, eSF_LocalClient|eSF_MapParamGameRules, "Highest Score (by gamemode)") \
	f(EMPS_GamemodesTime, eSF_LocalClient|eSF_MapParamGameRules, "Time spent playing (by gamemode)") \
	f(EMPS_GamesWon, eSF_LocalClient|eSF_MapParamGameRules, "Games Won (by gamemode)") \
	f(EMPS_GamesDrawn, eSF_LocalClient|eSF_MapParamGameRules, "Games drawn (by gamemode)") \
	f(EMPS_GamesLost, eSF_LocalClient|eSF_MapParamGameRules, "Games lost (by gamemode)") \
	\
	f(EMPS_Levels, eSF_LocalClient|eSF_MapParamLevel, "Games entered (by level)") \
	f(EMPS_LevelsInteractiveKills, eSF_LocalClient|eSF_MapParamLevel, "Interactive Kills (by level)") \
	\
	f(EMPS_WeaponHits, eSF_LocalClient|eSF_MapParamWeapon, "Hits (by weapon)") \
	f(EMPS_WeaponShots, eSF_RemoteClients|eSF_MapParamWeapon, "Shots fired (by weapon)") \
	f(EMPS_WeaponShotsSilenced, eSF_LocalClient|eSF_MapParamWeapon, "Silenced Shots fired (by weapon)") \
	f(EMPS_WeaponKills, eSF_RemoteClients|eSF_MapParamWeapon, "Kills (by weapon)") \
	f(EMPS_WeaponDeaths, eSF_RemoteClients|eSF_MapParamWeapon, "Times died whilst holding weapon") \
	f(EMPS_WeaponHeadshotKills, eSF_RemoteClients|eSF_MapParamWeapon, "Headshot kills (by weapon)") \
	f(EMPS_WeaponHeadshots, eSF_LocalClient|eSF_MapParamWeapon, "Headshots (hits - not kills) (by weapon)") \
	f(EMPS_WeaponTime, eSF_LocalClient|eSF_MapParamWeapon, "Time used (by weapon)") \
	f(EMPS_WeaponUsage, eSF_RemoteClients|eSF_MapParamWeapon, "Times selected weapon") \
	\
	f(EMPS_KillsByDamageType, eSF_RemoteClients|eSF_MapParamHitType, "Kills (by damage type)") \
	\
	f(EMPS_AttachmentTime, eSF_LocalClient|eSF_MapParamNone, "SP ONLY ALSO BROKEN!      0123456789012345678901234567890123456789012345678") \
	f(EMPS_SPLevelByDifficulty, eSF_LocalClient|eSF_MapParamNone, "SP levels by difficulty") \
	f(EMPS_SPSouvenirsByLevel, eSF_LocalClient|eSF_MapParamNone, "SP souvenirs by level") \
	f(EMPS_SPFlashbacksByName, eSF_LocalClient|eSF_MapParamNone, "SP flashbacks by name") \
	f(EMPS_SPKeysByVehicle, eSF_LocalClient|eSF_MapParamNone, "SP keys by vehicle") \
	f(EMPS_SPVideoByName, eSF_LocalClient|eSF_MapParamNone, "SP Video by name") \
	f(EMPS_SPEmailByName, eSF_LocalClient|eSF_MapParamNone, "SP Email by name") \
	f(EMPS_SPAudiologByName, eSF_LocalClient|eSF_MapParamNone, "SP audiologs by name") \
	f(EMPS_SPInterestVideoByName, eSF_LocalClient|eSF_MapParamNone, "SP interest videos by name") \
	f(EMPS_SPWeaponByName, eSF_LocalClient|eSF_MapParamNone, "SP weapons by name") \
	f(EMPS_SPSpeedCameraByName, eSF_LocalClient|eSF_MapParamNone, "SP Speed camera by name") \
	\
	f(EMPS_WeaponUnlocked, eSF_LocalClient|eSF_MapParamWeapon, "unlocked weapon (by weapon)") \
	f(EMPS_AttachmentUnlocked, eSF_LocalClient|eSF_MapParamNone, "SP Only unlocked attachments") \
	f(EMPS_EnemyKilled, eSF_LocalClient|eSF_MapParamNone, "Enemy killed by entityId") \
	f(EMPS_KillsFromExplosion, eSF_LocalClient|eSF_MapParamNone, "kills per explosion by projectile id") \
	
AUTOENUM_BUILDENUMWITHTYPE_WITHINVALID_WITHNUM(EMapPersistantStats, MapPersistantStats, EMPS_Invalid, EMPS_Max);

#define DerivedIntPersistantStats(f) \
	f(EDIPS_GamesPlayed) \
	f(EDIPS_GamesWon) \
	f(EDIPS_GamesLost) \
	f(EDIPS_GamesDrawn) \
	f(EDIPS_GamesMVP) \
	\
	f(EDIPS_Kills) \
	f(EDIPS_Deaths) \
	f(EDIPS_NanoSuitStregthKills) \
	\
	f(EDIPS_Hits) \
	f(EDIPS_Shots) \
	f(EDIPS_Misses) \
	f(EDIPS_Headshots) \
	f(EDIPS_HeadshotKills) \
	f(EDIPS_SPBowKills) \
	\
	f(EDIPS_XP) \
	f(EDIPS_Rank) \
	f(EDIPS_DefaultRank) \
	f(EDIPS_ArmorRank) \
	f(EDIPS_StealthRank) \
	f(EDIPS_Reincarnation) \
	\
	f(EDIPS_SuitFullyRanked) \
	\
	f(EDIPS_SupportBonus)\
	f(EDIPS_SuitModeChanges)\
	\
	f(EDIPS_NumSPLevels_Hard) \
	f(EDIPS_NumSPLevels_Delta) \
	\
	f(EDIPS_AliveTime) \
	f(EDIPS_EnvironmentalWeaponKills)\
	\
	f(EDIPS_NumEmailsCollected)\


AUTOENUM_BUILDENUMWITHTYPE_WITHINVALID_WITHNUM(EDerivedIntPersistantStats, DerivedIntPersistantStats, EDIPS_Invalid, EDIPS_Max);

#define DerivedFloatPersistantStats(f) \
	f(EDFPS_WinLoseRatio) \
	f(EDFPS_KillDeathRatio) \
	f(EDFPS_SkillKillKillRatio) \
	f(EDFPS_Accuracy) \
	f(EDFPS_LifeExpectancy ) \
	f(EDFPS_MicrowaveBeamEfficiency) \
	f(EDFPS_SuitArmorUsagePercent) \
	f(EDFPS_SuitStealthUsagePercent) \
	f(EDFPS_SuitDefaultUsagePercent) \
	f(EDFPS_SuitArmorKillDeathRatio) \
	f(EDFPS_SuitStealthKillDeathRatio) \
	f(EDFPS_SuitDefaultKillDeathRatio) \


AUTOENUM_BUILDENUMWITHTYPE_WITHINVALID_WITHNUM(EDerivedFloatPersistantStats, DerivedFloatPersistantStats, EDFPS_Invalid, EDFPS_Max);

#define DerivedStringPersistantStats(f) \
	f(EDSPS_FavouriteSuitMode) \
	f(EDSPS_FavouriteSuitModeWithDefault) \
	f(EDSPS_FavouriteWeapon) \
	f(EDSPS_FavouriteAttachment) \

AUTOENUM_BUILDENUMWITHTYPE_WITHINVALID_WITHNUM(EDerivedStringPersistantStats, DerivedStringPersistantStats, EDSPS_Invalid, EDSPS_Max);

#define DerivedFloatMapPersistantStats(f) \
	f(EDFMPS_Accuracy) \
	f(EDFMPS_WeaponKDRatio ) \

AUTOENUM_BUILDENUMWITHTYPE_WITHINVALID_WITHNUM(EDerivedFloatMapPersistantStats, DerivedFloatMapPersistantStats, EDFMPS_Invalid, EDFMPS_Max);

#define DerivedIntMapPersistantStats(f) \
	f(EDIMPS_GamesPlayed) \
	f(EDIMPS_DisplayedWeaponKills) \

AUTOENUM_BUILDENUMWITHTYPE_WITHINVALID_WITHNUM(EDerivedIntMapPersistantStats, DerivedIntMapPersistantStats, EDIMPS_Invalid, EDIMPS_Max);

#define DerivedStringMapPersistantStats(f) \
	f(EDSMPS_FavouriteGamemode) \
	f(EDSMPS_FavouritePrimaryWeapon) \
	f(EDSMPS_FavouritePrimaryWeaponLocalized) \

AUTOENUM_BUILDENUMWITHTYPE_WITHINVALID_WITHNUM(EDerivedStringMapPersistantStats, DerivedStringMapPersistantStats, EDSMPS_Invalid, EDSMPS_Max);


//////////////////////////////////////////////////////////////////////////
//Session Stats Structure
struct SSessionStats
{
	//////////////////////////////////////////////////////////////////////////
	// Streak stat structure
	template<class T> struct SStreak
	{
		T m_curVal;
		T m_maxVal;
		T m_maxThisSessionVal;
		bool m_multiSession;

		SStreak();

		void Increment();
		void Increment(T inc);
		void Set(T inc);
		void Reset();
		void ResetSession();
	};

	//////////////////////////////////////////////////////////////////////////
	// Stat Map structure
	struct SMap
	{
		typedef VectorMap<CryFixedStringT<64>, int, std::less<CryFixedStringT<64> >, stl::STLGlobalAllocator<std::pair<CryFixedStringT<64>, int > > > MapNameToCount;
		MapNameToCount m_map;

		SMap();

		void Update(const char* name, int amount = 1);
		void Clear();

		MapNameToCount::iterator FindOrInsert(const char* name);

		void SetStat(const char* name, int amount);
		int GetStat(const char* name) const;
		int GetTotal() const;

#ifndef _RELEASE
		void watch() const;
#endif
	};

	SSessionStats();

	void Add(const SSessionStats* pSessionStats);
	void Clear();
	void ResetClientSessionStats();

	void UpdateClientGUID();
	void UpdateGUID(EntityId actorId);

	static float GetRatio(int a, int b);

	ILINE int GetStat(EIntPersistantStats stat) const
	{
		CRY_ASSERT(stat >= 0 && stat < CRY_ARRAY_COUNT(m_intStats));
		return m_intStats[ stat ];
	}
	ILINE float GetStat(EFloatPersistantStats stat) const
	{
		CRY_ASSERT(stat >= 0 && stat < CRY_ARRAY_COUNT(m_floatStats));
		return m_floatStats[ stat ];
	}
	ILINE int GetStat(EStreakIntPersistantStats stat) const
	{
		CRY_ASSERT(stat >= 0 && stat < CRY_ARRAY_COUNT(m_streakIntStats));
		return m_streakIntStats[ stat ].m_maxVal;
	}
	ILINE float GetStat(EStreakFloatPersistantStats stat) const
	{
		CRY_ASSERT(stat >= 0 && stat < CRY_ARRAY_COUNT(m_streakFloatStats));
		return m_streakFloatStats[ stat ].m_maxVal;
	}

	ILINE void SetStat( EIntPersistantStats stat, int newValue )
	{
		m_intStats[ stat ] = newValue;
	}
	ILINE void SetStat( EFloatPersistantStats stat, float newValue )
	{
		m_floatStats[ stat ] = newValue;
	}
	ILINE void SetStat( EStreakIntPersistantStats stat, int newValue )
	{
		m_streakIntStats[ stat ].Set( newValue );
	}
	ILINE void SetStat( EStreakFloatPersistantStats stat, float newValue )
	{
		m_streakFloatStats[ stat ].Set( newValue );
	}

	ILINE int GetMaxThisSessionStat(EStreakIntPersistantStats stat) const
	{
		CRY_ASSERT(stat >= 0 && stat < CRY_ARRAY_COUNT(m_streakIntStats));
		return m_streakIntStats[stat].m_maxThisSessionVal;
	}

	ILINE float GetMaxThisSessionStat(EStreakFloatPersistantStats stat) const
	{
		CRY_ASSERT(stat >= 0 && stat < CRY_ARRAY_COUNT(m_streakFloatStats));
		return m_streakFloatStats[stat].m_maxThisSessionVal;
	}

	int GetStat(const char* name, EMapPersistantStats) const;
	const SMap::MapNameToCount& GetStatMap(EMapPersistantStats) const;
	int GetDerivedStat(EDerivedIntPersistantStats stat) const;
	float GetDerivedStat(EDerivedFloatPersistantStats stat) const;
	int GetDerivedStat(const char* name, EDerivedIntMapPersistantStats) const;
	float GetDerivedStat(const char* name, EDerivedFloatMapPersistantStats) const;
	const char *GetDerivedStat(EDerivedStringPersistantStats);
	const char *GetDerivedStat(EDerivedStringMapPersistantStats);

	float GetStatStrings(EIntPersistantStats stat, CryFixedStringT<64>& valueString);
	float GetStatStrings(EFloatPersistantStats stat, CryFixedStringT<64>& valueString);
	float GetStatStrings(EStreakIntPersistantStats stat, CryFixedStringT<64>& valueString);
	float GetStatStrings(EStreakFloatPersistantStats stat, CryFixedStringT<64>& valueString);

	float GetStatStrings(EDerivedIntPersistantStats stat, CryFixedStringT<64>& valueString);
	float GetStatStrings(EDerivedFloatPersistantStats stat, CryFixedStringT<64>& valueString);
	void GetStatStrings(EDerivedStringPersistantStats stat, CryFixedStringT<64>& valueString);
	void GetStatStrings(EDerivedStringMapPersistantStats stat, CryFixedStringT<64>& valueString);
	float GetStatStrings(const char* name, EDerivedIntMapPersistantStats stat, CryFixedStringT<64>& valueString);
	float GetStatStrings(const char* name, EDerivedFloatMapPersistantStats stat, CryFixedStringT<64>& valueString);
	float GetStatStrings(const char* name, EMapPersistantStats stat, CryFixedStringT<64>& valueString);
	float GetStatStrings(EMapPersistantStats stat, CryFixedStringT<64>& valueString);

	float GetStatStringsFromValue(EMapPersistantStats stat, int statValue, CryFixedStringT<64>& valueString);

	int m_intStats[EIPS_Max];
	float m_floatStats[EFPS_Max];
	SStreak<int> m_streakIntStats[ESIPS_Max];
	SStreak<float> m_streakFloatStats[ESFPS_Max];
	SMap m_mapStats[EMPS_Max];

	CryFixedStringT<CRYLOBBY_USER_GUID_STRING_LENGTH> m_guid;
	float m_lastKillTime;

	CryFixedStringT<32> m_dummyStr;

	// Just used by history
	uint32 m_xpHistoryDelta;
};


//Inline Functions
//----------------------------------------------------------
template <class T>
SSessionStats::SStreak<T>::SStreak()
{
	m_curVal = 0;
	m_maxVal = 0;
	m_multiSession = false;
}

//----------------------------------------------------------
template <class T>
void SSessionStats::SStreak<T>::Increment(T inc)
{
	T newVal = m_curVal + inc;
	if(m_multiSession)
	{
		m_maxThisSessionVal += inc;		
	}
	else
	{
		m_maxThisSessionVal = max(m_maxThisSessionVal,newVal);
	}
	m_maxVal = max(m_maxVal,newVal);
	m_curVal = newVal;
}

//----------------------------------------------------------
template<class T>
void SSessionStats::SStreak<T>::Increment()
{
	T newVal = m_curVal + 1;
	if(m_multiSession)
	{
		m_maxThisSessionVal += 1;
	}
	else
	{
		m_maxThisSessionVal = max(m_maxThisSessionVal,newVal);		
	}
	m_maxVal = max(m_maxVal,newVal);
	m_curVal = newVal;
}

//----------------------------------------------------------
template<class T>
void SSessionStats::SStreak<T>::Set(T inc)
{
	m_maxVal = max(m_maxVal, inc);
	m_maxThisSessionVal = max(m_maxThisSessionVal, inc);
}
//----------------------------------------------------------
template <class T>
void SSessionStats::SStreak<T>::Reset()
{
	m_curVal = 0;
	if(m_multiSession)
	{
		m_maxThisSessionVal = 0;
	}
}
//----------------------------------------------------------
template <class T>
void SSessionStats::SStreak<T>::ResetSession()
{
	if(!m_multiSession)
	{
		m_maxThisSessionVal = 0;
	}
}

#endif // __STATSSTRUCTURES_H__
