// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#ifndef GameAIEnv_h
#define GameAIEnv_h


// Forward declarations:
namespace HazardSystem
{
	class HazardModule;
};

struct GameAIEnv
{
	GameAIEnv();

	class CAIPressureModule* pressureModule;
	class CAIAggressionModule* aggressionModule;
	class CAIBattleFrontModule* battleFrontModule;
	class SearchModule* searchModule;
	class StalkerModule* stalkerModule;
	class RangeModule* rangeModule;
	class AloneDetectorModule* aloneDetectorModule;
	class HazardSystem::HazardModule* hazardModule;
	class RadioChatterModule* radioChatterModule;
};

extern GameAIEnv gGameAIEnv;

#endif // GameAIEnv_h
