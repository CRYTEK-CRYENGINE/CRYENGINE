// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#ifndef __CET_GAMERULES_H__
#define __CET_GAMERULES_H__

#pragma once

#include "ClassRegistryReplicator.h"

void AddGameRulesCreation(IContextEstablisher* pEst, EContextViewState state);
void AddGameRulesReset(IContextEstablisher* pEst, EContextViewState state);
void AddSendGameType(IContextEstablisher* pEst, EContextViewState state, CClassRegistryReplicator* pRep, std::vector<SSendableHandle>* pWaitFor);
void AddSendResetMap(IContextEstablisher* pEst, EContextViewState state);
void AddInitImmersiveness(IContextEstablisher* pEst, EContextViewState state);
void AddClearOnHold(IContextEstablisher* pEst, EContextViewState state);
void AddPauseGame(IContextEstablisher* pEst, EContextViewState state, bool pause, bool forcePause);
void AddWaitForPrecachingToFinish(IContextEstablisher* pEst, EContextViewState state, bool* pGameStart);
void AddInitialSaveGame(IContextEstablisher* pEst, EContextViewState state);
void AddClientTimeSync(IContextEstablisher* pEst, EContextViewState state);

#endif
