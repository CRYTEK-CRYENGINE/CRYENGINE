// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "AIConsoleVariables.h"
#include "BehaviorTree/BehaviorTreeManager.h"

namespace
{
struct PotentialDebugAgent
{
	Vec3        entityPosition;
	EntityId    entityId;
	const char* name;     // Pointer directly to the entity name

	PotentialDebugAgent()
		: entityPosition(ZERO)
		, entityId(0)
		, name(NULL)
	{
	}
};

typedef DynArray<PotentialDebugAgent> PotentialDebugAgents;

void GatherPotentialDebugAgents(PotentialDebugAgents& agents)
{
	// Add ai actors
	AutoAIObjectIter it(gEnv->pAISystem->GetAIObjectManager()->GetFirstAIObject(OBJFILTER_TYPE, AIOBJECT_ACTOR));
	for (; it->GetObject(); it->Next())
	{
		IAIObject* object = it->GetObject();
		IAIActor* actor = object ? object->CastToIAIActor() : NULL;
		IEntity* entity = object ? object->GetEntity() : NULL;

		if (entity && actor && actor->IsActive())
		{
			PotentialDebugAgent agent;
			agent.entityPosition = entity->GetWorldPos();
			agent.entityId = entity->GetId();
			agent.name = entity->GetName();
			agents.push_back(agent);
		}
	}

	// Add entities running modular behavior trees
	BehaviorTree::BehaviorTreeManager* behaviorTreeManager = gAIEnv.pBehaviorTreeManager;
	const size_t count = behaviorTreeManager->GetTreeInstanceCount();
	for (size_t index = 0; index < count; ++index)
	{
		const EntityId entityId = behaviorTreeManager->GetTreeInstanceEntityIdByIndex(index);
		if (IEntity* entity = gEnv->pEntitySystem->GetEntity(entityId))
		{
			PotentialDebugAgent agent;
			agent.entityPosition = entity->GetWorldPos();
			agent.entityId = entity->GetId();
			agent.name = entity->GetName();
			agents.push_back(agent);
		}
	}
}
}

void SAIConsoleVars::Init()
{
	AILogProgress("[AISYSTEM] Initialization: Creating console vars");

	navigation.Init();
	pathfinder.Init();
	pathFollower.Init();
	movement.Init();
	collisionAvoidance.Init();
	visionMap.Init();
	behaviorTree.Init();

	DefineConstIntCVarName("ai_DebugDraw", DebugDraw, -1, VF_CHEAT | VF_CHEAT_NOCHECK,
		"Toggles the AI debugging view.\n"
		"Usage: ai_DebugDraw [-1/0/1]\n"
		"Default is 0 (minimal), value -1 will draw nothing, value 1 displays AI rays and targets \n"
		"and enables the view for other AI debugging tools.");

	DefineConstIntCVarName("ai_RayCasterQuota", RayCasterQuota, 12, VF_CHEAT | VF_CHEAT_NOCHECK,
		"Amount of deferred rays allowed to be cast per frame!");

	DefineConstIntCVarName("ai_NetworkDebug", NetworkDebug, 0, VF_CHEAT | VF_CHEAT_NOCHECK,
		"Toggles the AI network debug.\n"
		"Usage: ai_NetworkDebug [0/1]\n"
		"Default is 0 (off). ai_NetworkDebug is used to direct DebugDraw information \n"
		"from the server to the client.");

	DefineConstIntCVarName("ai_IntersectionTesterQuota", IntersectionTesterQuota, 12, VF_CHEAT | VF_CHEAT_NOCHECK,
		"Amount of deferred intersection tests allowed to be cast per frame!");

	DefineConstIntCVarName("ai_UpdateAllAlways", UpdateAllAlways, 0, VF_CHEAT | VF_CHEAT_NOCHECK,
		"If non-zero then over-rides the auto-disabling of invisible/distant AI");

	DefineConstIntCVarName("ai_DebugDrawVegetationCollisionDist", DebugDrawVegetationCollisionDist, 0, VF_CHEAT | VF_CHEAT_NOCHECK,
		"Enables drawing vegetation collision closer than a distance projected onto the terrain.");

	DefineConstIntCVarName("ai_DrawPlayerRanges", DrawPlayerRanges, 0, VF_CHEAT | VF_CHEAT_NOCHECK,
		"Draws rings around player to assist in gauging target distance");

	DefineConstIntCVarName("ai_EnableWaterOcclusion", WaterOcclusionEnable, 1, VF_CHEAT | VF_CHEAT_NOCHECK,
		"Enables applying water occlusion to AI target visibility checks");

	DefineConstIntCVarName("ai_Recorder", Recorder, 0, VF_CHEAT | VF_CHEAT_NOCHECK, "Sets AI Recorder mode. Default is 0 - off.");

	DefineConstIntCVarName("ai_StatsDisplayMode", StatsDisplayMode, 0, VF_CHEAT | VF_CHEAT_NOCHECK,
		"Select display mode for the AI stats manager\n"
		"Usage: 0 - Hidden, 1 - Show\n");

	DefineConstIntCVarName("ai_DebugDrawSignalsHistory", DebugDrawSignalsHistory, 0, VF_CHEAT | VF_CHEAT_NOCHECK,
		"Displays a history of the 20 most recent signals sent by AI. \n"
		"Most recent signals are displayed at the top of the list. As time passes, old signals will turn gray.\n"
		"Usage: ai_DebugDrawSignalsList [0/1]\n"
		"Default is 0 (off)\n"
		"0 - off\n"
		"1 - on\n");

	DefineConstIntCVarName("ai_UpdateProxy", UpdateProxy, 1, VF_CHEAT | VF_CHEAT_NOCHECK,
		"Toggles update of AI proxy (model).\n"
		"Usage: ai_UpdateProxy [0/1]\n"
		"Default is 1 (on). Updates proxy (AI representation in game)\n"
		"set to 0 to disable proxy updating.");

#ifdef CONSOLE_CONST_CVAR_MODE
	DefineConstIntCVarName("ai_LogConsoleVerbosity", LogConsoleVerbosity, 0, VF_CHEAT | VF_CHEAT_NOCHECK, "None = 0, progress/errors/warnings = 1, event = 2, comment = 3");
	DefineConstIntCVarName("ai_LogFileVerbosity", LogFileVerbosity, 0, VF_CHEAT | VF_CHEAT_NOCHECK, "None = 0, progress/errors/warnings = 1, event = 2, comment = 3");
	DefineConstIntCVarName("ai_EnableWarningsErrors", EnableWarningsErrors, 0, VF_CHEAT | VF_CHEAT_NOCHECK, "Enable AI warnings and errors: 1 or 0");
#else
	DefineConstIntCVarName("ai_LogConsoleVerbosity", LogConsoleVerbosity, AI_LOG_OFF, VF_CHEAT | VF_CHEAT_NOCHECK, "None = 0, progress/errors/warnings = 1, event = 2, comment = 3");
	DefineConstIntCVarName("ai_LogFileVerbosity", LogFileVerbosity, AI_LOG_PROGRESS, VF_CHEAT | VF_CHEAT_NOCHECK, "None = 0, progress/errors/warnings = 1, event = 2, comment = 3");
	DefineConstIntCVarName("ai_EnableWarningsErrors", EnableWarningsErrors, 1, VF_CHEAT | VF_CHEAT_NOCHECK, "Enable AI warnings and errors: 1 or 0");
#endif

	REGISTER_CVAR2("ai_SystemUpdate", &AiSystem, 1, VF_CHEAT | VF_CHEAT_NOCHECK,
		"Toggles the regular AI system update.\n"
		"Usage: ai_SystemUpdate [0/1]\n"
		"Default is 1 (on). Set to 0 to disable ai system updating.");

	REGISTER_CVAR2("ai_OverlayMessageDuration", &OverlayMessageDuration, 5.0f, VF_DUMPTODISK, "How long (seconds) to overlay AI warnings/errors");

	// is not cheat protected because it changes during game, depending on your settings
	REGISTER_CVAR2("ai_UpdateInterval", &AIUpdateInterval, 0.13f, VF_NULL,
		"In seconds the amount of time between two full updates for AI  \n"
		"Usage: ai_UpdateInterval <number>\n"
		"Default is 0.1. Number is time in seconds");

	REGISTER_CVAR2("ai_SteepSlopeUpValue", &SteepSlopeUpValue, 1.0f, VF_CHEAT | VF_CHEAT_NOCHECK,
		"Indicates slope value that is borderline-walkable up.\n"
		"Usage: ai_SteepSlopeUpValue 0.5\n"
		"Default is 1.0 Zero means flat. Infinity means vertical. Set it smaller than ai_SteepSlopeAcrossValue");

	REGISTER_CVAR2("ai_SteepSlopeAcrossValue", &SteepSlopeAcrossValue, 0.6f, VF_CHEAT | VF_CHEAT_NOCHECK,
		"Indicates slope value that is borderline-walkable across.\n"
		"Usage: ai_SteepSlopeAcrossValue 0.8\n"
		"Default is 0.6 Zero means flat. Infinity means vertical. Set it greater than ai_SteepSlopeUpValue");

	REGISTER_CVAR2("ai_WaterOcclusion", &WaterOcclusionScale, .5f, VF_NULL,
		"scales how much water hides player from AI");

	REGISTER_CVAR2("ai_DrawOffset", &DebugDrawOffset, 0.1f, VF_CHEAT | VF_CHEAT_NOCHECK,
		"vertical offset during debug drawing (graph nodes, navigation paths, ...)");

	DefineConstIntCVarName("ai_IgnorePlayer", IgnorePlayer, 0, VF_CHEAT | VF_CHEAT_NOCHECK,
		"Makes AI ignore the player.\n"
		"Usage: ai_IgnorePlayer [0/1]\n"
		"Default is 0 (off). Set to 1 to make AI ignore the player.\n"
		"Used with ai_DebugDraw enabled.");

	REGISTER_COMMAND("ai_DebugAgent", DebugAgent, VF_NULL,
		"Start debugging an agent more in-depth. Pick by name, closest or in center of view.\n"
		"Example: ai_DebugAgent closest\n"
		"Example: ai_DebugAgent centerview\n"
		"Example: ai_DebugAgent name\n"
		"Call without parameters to stop the in-depth debugging.\n"
		"Example: ai_DebugAgent\n"
	);

	// Legacy CVars
	legacyCoverSystem.Init();
	legacyBubblesSystem.Init();
	legacySmartObjects.Init();
	legacyTacticalPointSystem.Init();
	legacyCommunicationSystem.Init();
	legacyPathObstacles.Init();
	legacyDebugDraw.Init();
	legacyGroupSystem.Init();
	legacyFormationSystem.Init();
	legacyPerception.Init();
	legacyTargetTracking.Init();
	legacyInterestSystem.Init();
	legacyPuppet.Init();
	legacyFiring.Init();
	legacyPuppetRod.Init();

	DefineConstIntCVarName("ai_DebugPathfinding", LegacyDebugPathFinding, 0, VF_CHEAT | VF_CHEAT_NOCHECK,
		"Toggles output of pathfinding information [default 0 is off]");

	DefineConstIntCVarName("ai_DebugDrawBannedNavsos", LegacyDebugDrawBannedNavsos, 0, VF_CHEAT | VF_CHEAT_NOCHECK,
		"Toggles drawing banned navsos [default 0 is off]");

	DefineConstIntCVarName("ai_CoverExactPositioning", LegacyCoverExactPositioning, 0, VF_CHEAT | VF_CHEAT_NOCHECK,
		"Enables using exact positioning for arriving at cover.\n"
		"Usage: ai_CoverPredictTarget [0/1]\n"
		"Default x is 0 (off)\n"
		"0 - disable\n"
		"1 - enable\n");

	DefineConstIntCVarName("ai_IgnoreVisibilityChecks", LegacyIgnoreVisibilityChecks, 0, VF_CHEAT | VF_CHEAT_NOCHECK,
		"Makes certain visibility checks (for teleporting etc) return false.");

	DefineConstIntCVarName("ai_RecordLog", LegacyRecordLog, 0, VF_CHEAT | VF_CHEAT_NOCHECK,
		"log all the AI state changes on stats_target");

	DefineConstIntCVarName("ai_Recorder_Auto", LegacyDebugRecordAuto, 0, VF_CHEAT | VF_CHEAT_NOCHECK,
		"Auto record the AI when in Editor mode game\n");

	DefineConstIntCVarName("ai_Recorder_Buffer", LegacyDebugRecordBuffer, 1024, VF_CHEAT | VF_CHEAT_NOCHECK,
		"Set the size of the AI debug recording buffer");

	DefineConstIntCVarName("ai_DrawAreas", LegacyDrawAreas, 0, VF_CHEAT | VF_CHEAT_NOCHECK,
		"Enables/Disables drawing behavior related areas.");

	DefineConstIntCVarName("ai_AdjustPathsAroundDynamicObstacles", LegacyAdjustPathsAroundDynamicObstacles, 1, VF_CHEAT | VF_CHEAT_NOCHECK,
		"Set to 1/0 to enable/disable AI path adjustment around dynamic obstacles");

	DefineConstIntCVarName("ai_OutputPersonalLogToConsole", LegacyOutputPersonalLogToConsole, 0, VF_CHEAT | VF_CHEAT_NOCHECK,
		"Output the personal log messages to the console.");

	DefineConstIntCVarName("ai_PredictivePathFollowing", LegacyPredictivePathFollowing, 1, VF_CHEAT | VF_CHEAT_NOCHECK,
		"Sets if AI should use the predictive path following if allowed by the type's config.");

	DefineConstIntCVarName("ai_ForceSerializeAllObjects", LegacyForceSerializeAllObjects, 0, VF_CHEAT | VF_CHEAT_NOCHECK,
		"Serialize all AI objects (ignore NO_SAVE flag).");

	REGISTER_CVAR2("ai_LogSignals", &LegacyLogSignals, 0, VF_CHEAT | VF_CHEAT_NOCHECK,
		"Logs all the signals received in CAIActor::NotifySignalReceived.");

	REGISTER_CVAR2("ai_CompatibilityMode", &LegacyCompatibilityMode, "", VF_NULL,
		"Set AI features to behave in earlier milestones - please use sparingly");

	REGISTER_CVAR2("ai_Locate", &LegacyDrawLocate, "none", VF_CHEAT | VF_CHEAT_NOCHECK,
		"Indicates position and some base states of specified objects.\n"
		"It will pinpoint position of the agents; it's name; it's attention target;\n"
		"draw red cone if the agent is allowed to fire; draw purple cone if agent is pressing trigger.\n"
		" none - off\n"
		" squad - squadmates\n"
		" enemy - all the enemies\n"
		" groupID - members of specified group");

	DefineConstIntCVarName("ai_ScriptBind", LegacyScriptBind, 1, VF_CHEAT | VF_CHEAT_NOCHECK,
		"[0-1] Enable/Disable the Script Bind AI subsystem.\n");
	// ~Legacy CVars
}

void SAIConsoleVars::DebugAgent(IConsoleCmdArgs* args)
{
	EntityId debugTargetEntity = 0;
	Vec3 debugTargetEntityPos(ZERO);

	if (args->GetArgCount() >= 2)
	{
		PotentialDebugAgents agents;
		GatherPotentialDebugAgents(agents);

		const stack_string str = args->GetArg(1);

		if (str == "closest")
		{
			// Find the closest agent to the camera
			const Vec3 cameraPosition = GetISystem()->GetViewCamera().GetMatrix().GetTranslation();

			EntityId closestId = 0;
			float closestSqDist = std::numeric_limits<float>::max();
			const char* name = NULL;

			PotentialDebugAgents::iterator it = agents.begin();
			PotentialDebugAgents::iterator end = agents.end();

			for (; it != end; ++it)
			{
				const PotentialDebugAgent& agent = (*it);
				const Vec3 entityPosition = agent.entityPosition;
				const float sqDistToCamera = Distance::Point_Point2D(cameraPosition, entityPosition);
				if (sqDistToCamera < closestSqDist)
				{
					closestId = agent.entityId;
					closestSqDist = sqDistToCamera;
					name = agent.name;
					debugTargetEntityPos = entityPosition;
				}
			}

			if (closestId)
			{
				debugTargetEntity = closestId;
				gEnv->pLog->Log("Started debugging agent with name '%s', who is closest to the camera.", name);
			}
			else
			{
				gEnv->pLog->LogWarning("Could not find an agent to debug.");
			}
		}
		else if (str == "centerview" || str == "centermost")
		{
			// Find the agent most in center of view
			const Vec3 cameraPosition = GetISystem()->GetViewCamera().GetMatrix().GetTranslation();
			const Vec3 cameraForward = GetISystem()->GetViewCamera().GetMatrix().GetColumn1().GetNormalized();

			EntityId closestToCenterOfView = 0;
			float highestDotProduct = -1.0f;
			const char* name = NULL;

			PotentialDebugAgents::iterator it = agents.begin();
			PotentialDebugAgents::iterator end = agents.end();

			for (; it != end; ++it)
			{
				const PotentialDebugAgent& agent = (*it);
				const Vec3 entityPosition = agent.entityPosition;
				const Vec3 cameraToAgentDir = (entityPosition - cameraPosition).GetNormalized();
				const float dotProduct = cameraForward.Dot(cameraToAgentDir);
				if (dotProduct > highestDotProduct)
				{
					closestToCenterOfView = agent.entityId;
					highestDotProduct = dotProduct;
					name = agent.name;
					debugTargetEntityPos = entityPosition;
				}
			}

			if (closestToCenterOfView)
			{
				debugTargetEntity = closestToCenterOfView;
				gEnv->pLog->Log("Started debugging agent with name '%s', who is centermost in the view.", name);
			}
			else
			{
				gEnv->pLog->LogWarning("Could not find an agent to debug.");
			}
		}
		else
		{
			// Find the agent by name
			if (IEntity* entity = gEnv->pEntitySystem->FindEntityByName(str))
			{
				debugTargetEntity = entity->GetId();
				debugTargetEntityPos = entity->GetWorldPos() + Vec3(0, 2, 0);
				gEnv->pLog->Log("Started debugging agent with name '%s'.", str.c_str());
			}
			else
			{
				gEnv->pLog->LogWarning("Could not debug agent with name '%s' because the entity doesn't exist.", str.c_str());
			}
		}
	}

#ifdef CRYAISYSTEM_DEBUG
	if (GetAISystem()->GetAgentDebugTarget() != debugTargetEntity)
	{
		GetAISystem()->SetAgentDebugTarget(debugTargetEntity);

		gEnv->pLog->Log("Debug target is at position %f, %f, %f",
			debugTargetEntityPos.x,
			debugTargetEntityPos.y,
			debugTargetEntityPos.z);
	}
	else
	{
		GetAISystem()->SetAgentDebugTarget(0);
	}
#endif

}