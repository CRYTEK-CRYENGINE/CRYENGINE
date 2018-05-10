// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "CentralEventManager.h"


CCrySignal<void(const CCentralEventManager::SQueryBlueprintRuntimeParamsChangedEventArgs&)> CCentralEventManager::QueryBlueprintRuntimeParamsChanged;
CCrySignal<void(const CCentralEventManager::SStartOrStopQuerySimulatorEventArgs&)> CCentralEventManager::StartOrStopQuerySimulator;
CCrySignal<void(const CCentralEventManager::SSingleStepQuerySimulatorOnceEventArgs&)> CCentralEventManager::SingleStepQuerySimulatorOnce;
CCrySignal<void(const CCentralEventManager::SQuerySimulatorRunningStateChangedEventArgs&)> CCentralEventManager::QuerySimulatorRunningStateChanged;
