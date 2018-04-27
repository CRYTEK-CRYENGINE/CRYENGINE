// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

// stdafx.h : include file for standard system include files,
// or project specific include files that are used frequently, but
// are changed infrequently
//

#pragma once

#define eCryModule eCryM_FlowGraph

#include <CryCore/Platform/platform.h>

#include <CrySystem/ISystem.h>
#include <CrySystem/ITimer.h>
#include <CryScriptSystem/IScriptSystem.h>

#include <CryEntitySystem/IEntity.h>
#include <CryEntitySystem/IEntitySystem.h>
#include <CryEntitySystem/IEntityComponent.h>

#include <CryPhysics/IPhysics.h>
#include <CrySystem/IConsole.h>
#include <CryAnimation/ICryAnimation.h>
#include <CryGame/IGameTokens.h>
#include <CryInput/IInput.h>
#include <CryRenderer/IRenderer.h>
#include <CryGame/IGame.h>
#include <CryGame/IGameFramework.h>
#include <CryFlowGraph/IFlowSystem.h>

#include <CryAISystem/IAISystem.h>

// Include CryAction specific headers
#include <../CryAction/IActorSystem.h>
#include <../CryAction/IViewSystem.h>
#include <../CryAction/ILevelSystem.h>
#include <../CryAction/IGameRulesSystem.h>
#include <../CryAction/IVehicleSystem.h>
#include <../CryAction/IForceFeedbackSystem.h>
#include <../CryAction/IAnimatedCharacter.h>
#include <../CryAction/IItemSystem.h>

#include "HelperFunctions.h"