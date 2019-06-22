// Copyright 2001-2019 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include <CrySystem/ConsoleRegistration.h>

struct SAIConsoleVarsLegacyCommunicationSystem
{
	void Init();

	static void CommTest(IConsoleCmdArgs* args);
	static void CommTestStop(IConsoleCmdArgs* args);
	static void CommWriteStats(IConsoleCmdArgs* args);
	static void CommResetStats(IConsoleCmdArgs* args);

	DeclareConstIntCVar(CommunicationSystem, 1);
	DeclareConstIntCVar(DebugDrawCommunication, 0);
	DeclareConstIntCVar(DebugDrawCommunicationHistoryDepth, 5);
	DeclareConstIntCVar(RecordCommunicationStats, 0);
	DeclareConstIntCVar(CommunicationForceTestVoicePack, 0);

	float CommunicationManagerHeightThresholdForTargetPosition;
};