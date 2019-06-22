// Copyright 2001-2019 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"

#include "CommunicationConsoleVariables.h"
#include "Communication/CommunicationManager.h"
#include "Communication/CommunicationTestManager.h"

void SAIConsoleVarsLegacyCommunicationSystem::Init()
{
	DefineConstIntCVarName("ai_CommunicationSystem", CommunicationSystem, 1, VF_CHEAT | VF_CHEAT_NOCHECK,
		"[0-1] Enable/Disable the Communication subsystem.\n");

	DefineConstIntCVarName("ai_DebugDrawCommunication", DebugDrawCommunication, 0, VF_CHEAT | VF_CHEAT_NOCHECK,
		"Displays communication debug information.\n"
		"Usage: ai_DebugDrawCommunication [0/1/2]\n"
		" 0 - disabled. (default).\n"
		" 1 - draw playing and queued comms.\n"
		" 2 - draw debug history for each entity.\n"
		" 5 - extended debugging (to log)"
		" 6 - outputs info about each line played");

	DefineConstIntCVarName("ai_DebugDrawCommunicationHistoryDepth", DebugDrawCommunicationHistoryDepth, 5, VF_CHEAT | VF_CHEAT_NOCHECK,
		"Tweaks how many historical entries are displayed per entity.\n"
		"Usage: ai_DebugDrawCommunicationHistoryDepth [depth]");

	DefineConstIntCVarName("ai_RecordCommunicationStats", RecordCommunicationStats, 0, VF_CHEAT | VF_CHEAT_NOCHECK,
		"Turns on/off recording of communication stats to a log.\n"
		"Usage: ai_RecordCommunicationStats [0/1]\n"
	);

	REGISTER_CVAR2("ai_CommunicationManagerHeighThresholdForTargetPosition", &CommunicationManagerHeightThresholdForTargetPosition, 5.0f, VF_CHEAT | VF_CHEAT_NOCHECK,
		"Defines the threshold used to detect if the target is above or below the entity that wants to play a communication.\n");

	DefineConstIntCVarName("ai_CommunicationForceTestVoicePack", CommunicationForceTestVoicePack, 0, VF_CHEAT | VF_CHEAT_NOCHECK,
		"Forces all the AI agents to use a test voice pack. The test voice pack will have the specified name in the archetype"
		" or in the entity and the system will replace the _XX number with the _test string");

	REGISTER_COMMAND("ai_commTest", CommTest, VF_NULL,
		"Tests communication for the specified AI actor.\n"
		"If no communication name is specified all communications will be played.\n"
		"Usage: ai_commTest <actorName> [commName]\n");

	REGISTER_COMMAND("ai_commTestStop", CommTestStop, VF_NULL,
		"Stop currently playing communication for the specified AI actor.\n"
		"Usage: ai_commTestStop <actorName>\n");

	REGISTER_COMMAND("ai_writeCommStats", CommWriteStats, VF_NULL,
		"Dumps current statistics to log file.\n"
		"Usage: ai_writeCommStats\n");

	REGISTER_COMMAND("ai_resetCommStats", CommResetStats, VF_NULL,
		"Resets current communication statistics.\n"
		"Usage: ai_resetCommStats\n");
}

void SAIConsoleVarsLegacyCommunicationSystem::CommTest(IConsoleCmdArgs* args)
{
	if (args->GetArgCount() < 2)
	{
		AIWarning("<CommTest> Expecting actorName as parameter.");

		return;
	}

	CAIActor* actor = 0;

	if (IEntity* entity = gEnv->pEntitySystem->FindEntityByName(args->GetArg(1)))
		if (IAIObject* aiObject = entity->GetAI())
			actor = aiObject->CastToCAIActor();

	if (!actor)
	{
		AIWarning("<CommTest> Could not find entity named '%s' or it's not an actor.", args->GetArg(1));

		return;
	}

	const char* commName = 0;

	if (args->GetArgCount() > 2)
		commName = args->GetArg(2);

	int variationNumber = 0;

	if (args->GetArgCount() > 3)
		variationNumber = atoi(args->GetArg(3));

	gAIEnv.pCommunicationManager->GetTestManager().StartFor(actor->GetEntityID(), commName, variationNumber);
}

void SAIConsoleVarsLegacyCommunicationSystem::CommTestStop(IConsoleCmdArgs* args)
{
	if (args->GetArgCount() < 2)
	{
		AIWarning("<CommTest> Expecting actorName as parameter.");

		return;
	}

	CAIActor* actor = 0;

	if (IEntity* entity = gEnv->pEntitySystem->FindEntityByName(args->GetArg(1)))
		if (IAIObject* aiObject = entity->GetAI())
			actor = aiObject->CastToCAIActor();

	if (!actor)
	{
		AIWarning("<CommTest> Could not find entity named '%s' or it's not an actor.", args->GetArg(1));

		return;
	}

	gAIEnv.pCommunicationManager->GetTestManager().Stop(actor->GetEntityID());
}

void SAIConsoleVarsLegacyCommunicationSystem::CommWriteStats(IConsoleCmdArgs* args)
{
	gAIEnv.pCommunicationManager->WriteStatistics();
}

void SAIConsoleVarsLegacyCommunicationSystem::CommResetStats(IConsoleCmdArgs* args)
{
	gAIEnv.pCommunicationManager->ResetStatistics();
}