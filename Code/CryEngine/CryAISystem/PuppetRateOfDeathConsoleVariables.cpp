// Copyright 2001-2019 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "PuppetRateOfDeathConsoleVariables.h"

void SAIConsoleVarsLegacyPuppetROD::Init()
{
	REGISTER_CVAR2("ai_RODAliveTime", &RODAliveTime, 3.0f, VF_NULL,
		"The base level time the player can survive under fire.");
	REGISTER_CVAR2("ai_RODMoveInc", &RODMoveInc, 3.0f, VF_NULL,
		"Increment how the speed of the target affects the alive time (the value is doubled for supersprint). 0=disable");
	REGISTER_CVAR2("ai_RODStanceInc", &RODStanceInc, 2.0f, VF_NULL,
		"Increment how the stance of the target affects the alive time, 0=disable.\n"
		"The base value is for crouch, and it is doubled for prone.\n"
		"The crouch inc is disable in kill-zone and prone in kill and combat-near -zones");
	REGISTER_CVAR2("ai_RODDirInc", &RODDirInc, 0.0f, VF_NULL,
		"Increment how the orientation of the target affects the alive time. 0=disable");
	REGISTER_CVAR2("ai_RODKillZoneInc", &RODKillZoneInc, -4.0f, VF_NULL,
		"Increment how the target is within the kill-zone of the target.");
	REGISTER_CVAR2("ai_RODFakeHitChance", &RODFakeHitChance, 0.2f, VF_NULL,
		"Percentage of the missed hits that will instead be hits dealing very little damage.");
	REGISTER_CVAR2("ai_RODAmbientFireInc", &RODAmbientFireInc, 3.0f, VF_NULL,
		"Increment for the alive time when the target is within the kill-zone of the target.");

	REGISTER_CVAR2("ai_RODKillRangeMod", &RODKillRangeMod, 0.15f, VF_NULL,
		"Kill-zone distance = attackRange * killRangeMod.");
	REGISTER_CVAR2("ai_RODCombatRangeMod", &RODCombatRangeMod, 0.55f, VF_NULL,
		"Combat-zone distance = attackRange * combatRangeMod.");

	REGISTER_CVAR2("ai_RODCoverFireTimeMod", &RODCoverFireTimeMod, 1.0f, VF_NULL,
		"Multiplier for cover fire times set in weapon descriptor.");

	REGISTER_CVAR2("ai_RODReactionTime", &RODReactionTime, 1.0f, VF_NULL,
		"Uses rate of death as damage control method.");
	REGISTER_CVAR2("ai_RODReactionDistInc", &RODReactionDistInc, 0.1f, VF_NULL,
		"Increase for the reaction time when the target is in combat-far-zone or warn-zone.\n"
		"In warn-zone the increase is doubled.");
	REGISTER_CVAR2("ai_RODReactionDirInc", &RODReactionDirInc, 2.0f, VF_NULL,
		"Increase for the reaction time when the enemy is outside the players FOV or near the edge of the FOV.\n"
		"The increment is doubled when the target is behind the player.");
	REGISTER_CVAR2("ai_RODReactionLeanInc", &RODReactionLeanInc, 0.2f, VF_NULL,
		"Increase to the reaction to when the target is leaning.");
	REGISTER_CVAR2("ai_RODReactionSuperDarkIllumInc", &RODReactionSuperDarkIllumInc, 0.4f, VF_NULL,
		"Increase for reaction time when the target is in super dark light condition.");
	REGISTER_CVAR2("ai_RODReactionDarkIllumInc", &RODReactionDarkIllumInc, 0.3f, VF_NULL,
		"Increase for reaction time when the target is in dark light condition.");
	REGISTER_CVAR2("ai_RODReactionMediumIllumInc", &RODReactionMediumIllumInc, 0.2f, VF_NULL,
		"Increase for reaction time when the target is in medium light condition.");

	REGISTER_CVAR2("ai_RODLowHealthMercyTime", &RODLowHealthMercyTime, 1.5f, VF_NULL,
		"The amount of time the AI will not hit the target when the target crosses the low health threshold.");
}