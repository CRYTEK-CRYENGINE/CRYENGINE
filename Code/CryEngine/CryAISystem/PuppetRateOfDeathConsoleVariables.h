// Copyright 2001-2019 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include <CrySystem/ConsoleRegistration.h>

struct SAIConsoleVarsLegacyPuppetROD
{
	void Init();

	float RODAliveTime;
	float RODMoveInc;
	float RODStanceInc;
	float RODDirInc;
	float RODAmbientFireInc;
	float RODKillZoneInc;
	float RODFakeHitChance;

	float RODKillRangeMod;
	float RODCombatRangeMod;

	float RODReactionTime;
	float RODReactionSuperDarkIllumInc;
	float RODReactionDarkIllumInc;
	float RODReactionMediumIllumInc;
	float RODReactionDistInc;
	float RODReactionDirInc;
	float RODReactionLeanInc;

	float RODLowHealthMercyTime;

	float RODCoverFireTimeMod;
};