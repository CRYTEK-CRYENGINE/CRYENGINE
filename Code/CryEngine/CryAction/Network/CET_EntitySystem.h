// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#ifndef __CET_ENTITYSYSTEM_H__
#define __CET_ENTITYSYSTEM_H__

#pragma once

enum EFakeSpawn
{
	eFS_Players    = 0x1,
	eFS_GameRules  = 0x2,
	eFS_Others     = 0x4,
	eFS_All        = eFS_Players | eFS_GameRules | eFS_Others,
	eFS_Opt_Rebind = 0x40000
};

void AddEntitySystemReset(IContextEstablisher* pEst, EContextViewState state, bool skipPlayers, bool skipGamerules);
void AddRandomSystemReset(IContextEstablisher* pEst, EContextViewState state, bool loadingNewLevel);
void AddLoadLevelEntities(IContextEstablisher* pEst, EContextViewState state);
void AddFakeSpawn(IContextEstablisher* pEst, EContextViewState state, unsigned what);
void AddEntitySystemEvent(IContextEstablisher* pEst, EContextViewState state, const SEntityEvent& evt);
void AddEntitySystemGameplayStart(IContextEstablisher* pEst, EContextViewState state);
#endif
