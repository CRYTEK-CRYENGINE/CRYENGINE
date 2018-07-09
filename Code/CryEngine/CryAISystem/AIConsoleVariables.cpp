// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "AIConsoleVariables.h"
#include "Communication/CommunicationManager.h"
#include "Communication/CommunicationTestManager.h"
#include "Navigation/NavigationSystem/NavigationSystem.h"
#include "BehaviorTree/BehaviorTreeManager.h"
#include <CryAISystem/IAIBubblesSystem.h>

void AIConsoleVars::Init()
{
	AILogProgress("[AISYSTEM] Initialization: Creating console vars");

	IConsole* pConsole = gEnv->pConsole;

	REGISTER_CVAR2("ai_CompatibilityMode", &CompatibilityMode, "", VF_NULL,
	               "Set AI features to behave in earlier milestones - please use sparingly");

	REGISTER_CVAR2("ai_DebugDrawAStarOpenList", &DebugDrawAStarOpenList, "0", VF_CHEAT | VF_CHEAT_NOCHECK,
	               "Draws the A* open list for the specified AI agent.\n"
	               "Usage: ai_DebugDrawAStarOpenList [AI agent name]\n"
	               "Default is 0, which disables the debug draw. Requires ai_DebugPathfinding=1 to be activated.");

	REGISTER_CVAR2("ai_DebugDrawAStarOpenListTime", &DebugDrawAStarOpenListTime, 10.0f, VF_CHEAT | VF_CHEAT_NOCHECK,
	               "The amount of time to draw the A* open list.");

	REGISTER_CVAR2("ai_DrawNode", &DrawNode, "none", VF_CHEAT | VF_CHEAT_NOCHECK,
	               "Toggles visibility of named agent's position on AI triangulation.\n"
	               "See also: ai_DrawNodeLinkType and ai_DrawNodeLinkCutoff\n"
	               "Usage: ai_DrawNode [ai agent's name]\n"
	               " none - switch off\n"
	               " all - to display nodes of all the AI agents\n"
	               " player - to display player's node\n"
	               " AI agent's name - to display node of particular agent");
	REGISTER_CVAR2("ai_Locate", &DrawLocate, "none", VF_CHEAT | VF_CHEAT_NOCHECK,
	               "Indicates position and some base states of specified objects.\n"
	               "It will pinpoint position of the agents; it's name; it's attention target;\n"
	               "draw red cone if the agent is allowed to fire; draw purple cone if agent is pressing trigger.\n"
	               " none - off\n"
	               " squad - squadmates\n"
	               " enemy - all the enemies\n"
	               " groupID - members of specified group");
	REGISTER_CVAR2("ai_DrawPath", &DrawPath, "none", VF_CHEAT | VF_CHEAT_NOCHECK,
	               "Draws the generated paths of the AI agents. ai_drawoffset is used.\n"
	               "Usage: ai_DrawPath [name]\n"
	               " none - off (default)\n"
	               " squad - squadmates\n"
	               " enemy - all the enemies");
	REGISTER_CVAR2("ai_DrawPathAdjustment", &DrawPathAdjustment, "", VF_CHEAT | VF_CHEAT_NOCHECK,
	               "Draws the path adjustment for the AI agents.\n"
	               "Usage: ai_DrawPathAdjustment [name]\n"
	               "Default is none (nobody).");
	REGISTER_CVAR2("ai_DrawRefPoints", &DrawRefPoints, "", VF_CHEAT | VF_CHEAT_NOCHECK,
	               "Toggles reference points and beacon view for debugging AI.\n"
	               "Usage: ai_DrawRefPoints \"all\", agent name, group id \n"
	               "Default is the empty string (off). Indicates the AI reference points by drawing\n"
	               "balls at their positions.");
	REGISTER_CVAR2("ai_StatsTarget", &StatsTarget, "none", VF_CHEAT | VF_CHEAT_NOCHECK,
	               "Focus debugging information on a specific AI\n"
	               "Display current goal pipe, current goal, subpipes and agentstats information for the selected AI agent.\n"
	               "Long green line will represent the AI forward direction (game forward).\n"
	               "Long red/blue (if AI firing on/off) line will represent the AI view direction.\n"
	               "Usage: ai_StatsTarget AIName\n"
	               "Default is 'none'. AIName is the name of the AI\n"
	               "on which to focus.");
	DefineConstIntCVarName("ai_DebugDrawPhysicsAccess", DebugDrawPhysicsAccess, 0, VF_CHEAT | VF_CHEAT_NOCHECK,
	                       "Displays current physics access statistics for the AI module.");

	DefineConstIntCVarName("ai_RayCasterQuota", RayCasterQuota, 12, VF_CHEAT | VF_CHEAT_NOCHECK,
	                       "Amount of deferred rays allowed to be cast per frame!");

	DefineConstIntCVarName("ai_IntersectionTesterQuota", IntersectionTesterQuota, 12, VF_CHEAT | VF_CHEAT_NOCHECK,
	                       "Amount of deferred intersection tests allowed to be cast per frame!");

	REGISTER_CVAR2("ai_DrawShooting", &DrawShooting, "none", VF_CHEAT | VF_CHEAT_NOCHECK,
	               "Name of puppet to show fire stats");
	REGISTER_CVAR2("ai_DrawAgentStats", &DrawAgentStats, "NkcBbtGgSfdDL", VF_NULL,
	               "Flag field specifying which of the overhead agent stats to display:\n"
	               "N - name\n"
	               "k - groupID\n"
	               "d - distances\n"
	               "c - cover info\n"
	               "B - currently selected behavior node\n"
	               "b - current behavior\n"
	               "t - target info\n"
	               "G - goal pipe\n"
	               "g - goal op\n"
	               "S - stance\n"
	               "f - fire\n"
	               "w - territory/wave\n"
	               "p - pathfinding status\n"
	               "l - light level (perception) status\n"
	               "D - various direction arrows (aim target, move target, ...) status\n"
	               "L - personal log\n"
	               "a - alertness\n"
	               "\n"
	               "Default is NkcBbtGgSfdDL");

	REGISTER_CVAR2("ai_DrawAgentStatsGroupFilter", &DrawAgentStatsGroupFilter, "", VF_NULL,
	               "Filters Debug Draw Agent Stats displays to the specified groupIDs separated by spaces\n"
	               "usage: ai_DrawAgentStatsGroupFilter 1011 1012");
	REGISTER_CVAR2("ai_DrawPerceptionHandlerModifiers", &DrawPerceptionHandlerModifiers, "none", VF_CHEAT | VF_CHEAT_NOCHECK,
	               "Draws perception handler modifiers on a specific AI\n"
	               "Usage: ai_DrawPerceptionHandlerModifiers AIName\n"
	               "Default is 'none'. AIName is the name of the AI");
	REGISTER_CVAR2("ai_ForceAGAction", &ForceAGAction, "0", VF_CHEAT | VF_CHEAT_NOCHECK,
	               "Forces all AI characters to specified AG action input. 0 to disable.\n");
	REGISTER_CVAR2("ai_ForceAGSignal", &ForceAGSignal, "0", VF_CHEAT | VF_CHEAT_NOCHECK,
	               "Forces all AI characters to specified AG signal input. 0 to disable.\n");
	REGISTER_CVAR2("ai_ForcePosture", &ForcePosture, "0", VF_CHEAT | VF_CHEAT_NOCHECK,
	               "Forces all AI characters to specified posture. 0 to disable.\n");
	REGISTER_CVAR2("ai_ForceLookAimTarget", &ForceLookAimTarget, "none", VF_CHEAT | VF_CHEAT_NOCHECK,
	               "Forces all AI characters to use/not use a fixed look/aim target\n"
	               "none disables\n"
	               "x, y, xz or yz sets it to the appropriate direction\n"
	               "otherwise it forces looking/aiming at the entity with this name (no name -> (0, 0, 0))");

	REGISTER_CVAR2("ai_CheckWalkabilityOptimalSectionLength", &CheckWalkabilityOptimalSectionLength, 1.75f, VF_NULL,
	               "The maximum segment length used by CheckWalkabilityFast when querying for world geometry from physics.\n"
	               "Default: 1.75\n");

	REGISTER_CVAR2("ai_TacticalPointUpdateTime", &TacticalPointUpdateTime, 0.0005f, VF_NULL,
	               "Maximum allowed update time in main AI thread for Tactical Point System\n"
	               "Usage: ai_TacticalPointUpdateTime <number>\n"
	               "Default is 0.0003");

	DefineConstIntCVarName("ai_UpdateAllAlways", UpdateAllAlways, 0, VF_CHEAT | VF_CHEAT_NOCHECK,
	                       "If non-zero then over-rides the auto-disabling of invisible/distant AI");
	DefineConstIntCVarName("ai_UseCalculationStopperCounter", UseCalculationStopperCounter, 0, VF_CHEAT | VF_CHEAT_NOCHECK,
	                       "Uses a (calibrated) counter instead of time in AI updates");

	// is not cheat protected because it changes during game, depending on your settings
	REGISTER_CVAR2("ai_UpdateInterval", &AIUpdateInterval, 0.13f, VF_NULL,
	               "In seconds the amount of time between two full updates for AI  \n"
	               "Usage: ai_UpdateInterval <number>\n"
	               "Default is 0.1. Number is time in seconds");
	DefineConstIntCVarName("ai_AdjustPathsAroundDynamicObstacles", AdjustPathsAroundDynamicObstacles, 1, VF_CHEAT | VF_CHEAT_NOCHECK,
	                       "Set to 1/0 to enable/disable AI path adjustment around dynamic obstacles");
	// is not cheat protected because it changes during game, depending on your settings
	DefineConstIntCVarName("ai_EnableORCA", EnableORCA, 1, VF_CHEAT | VF_CHEAT_NOCHECK,
	                       "Enable obstacle avoidance system.");
	DefineConstIntCVarName("ai_CollisionAvoidanceUpdateVelocities", CollisionAvoidanceUpdateVelocities, 1, VF_CHEAT | VF_CHEAT_NOCHECK,
	                       "Enable/disable agents updating their velocities after processing collision avoidance.");
	DefineConstIntCVarName("ai_CollisionAvoidanceEnableRadiusIncrement", CollisionAvoidanceEnableRadiusIncrement, 1, VF_CHEAT | VF_CHEAT_NOCHECK,
	                       "Enable/disable agents adding an increment to their collision avoidance radius when moving.");
	DefineConstIntCVarName("ai_CollisionAvoidanceClampVelocitiesWithNavigationMesh", CollisionAvoidanceClampVelocitiesWithNavigationMesh, 0, VF_CHEAT | VF_CHEAT_NOCHECK,
	                       "Enable/Disable the clamping of the speed resulting from ORCA with the navigation mesh");
	DefineConstIntCVarName("ai_DebugDrawCollisionAvoidance", DebugDrawCollisionAvoidance, 0, VF_CHEAT | VF_CHEAT_NOCHECK,
	                       "Enable debugging obstacle avoidance system.");
	// Bubble System cvars
	REGISTER_CVAR2("ai_BubblesSystem", &EnableBubblesSystem, 1, VF_CHEAT | VF_CHEAT_NOCHECK,
	               "Enables/disables bubble notifier.");
	REGISTER_CVAR2("ai_BubblesSystemDecayTime", &BubblesSystemDecayTime, 15.0f, VF_CHEAT | VF_CHEAT_NOCHECK,
	               "Specifies the decay time for the bubbles drawn on screen.");

	DefineConstIntCVarName("ai_BubblesSystemAlertnessFilter", BubblesSystemAlertnessFilter, 7, VF_CHEAT | VF_CHEAT_NOCHECK,
	                       "Specifies the type of messages you want to receive:\n"
	                       "0 - none\n"
	                       "1 - Only logs in the console\n"
	                       "2 - Only bubbles\n"
	                       "3 - Logs and bubbles\n"
	                       "4 - Only blocking popups\n"
	                       "5 - Blocking popups and logs\n"
	                       "6 - Blocking popups and bubbles\n"
	                       "7 - All notifications");
	DefineConstIntCVarName("ai_BubblesSystemUseDepthTest", BubblesSystemUseDepthTest, 0, VF_CHEAT | VF_CHEAT_NOCHECK,
	                       "Specifies if the BubblesSystem should use the depth test to show the messages"
	                       " inside the 3D world.");
	DefineConstIntCVarName("ai_BubblesSystemAllowPrototypeDialogBubbles", BubblesSystemAllowPrototypeDialogBubbles, 0, VF_CHEAT | VF_CHEAT_NOCHECK,
	                       "Enabling the visualization of the bubbles created to prototype AI dialogs");
	REGISTER_CVAR2("ai_BubblesSystemFontSize", &BubblesSystemFontSize, 45.0f, VF_CHEAT | VF_CHEAT_NOCHECK, "Font size for the BubblesSystem.");
	REGISTER_CVAR2_CB("ai_BubblesSystemNameFilter", &BubblesSystemNameFilter, "", VF_CHEAT | VF_CHEAT_NOCHECK,
	                  "Filter BubblesSystem messages by name. Do not filter, if empty.", &AIBubblesNameFilterCallback);
	// Bubble System cvars

	// Pathfinding dangers
	DefineConstIntCVarName("ai_PathfinderDangerCostForAttentionTarget", PathfinderDangerCostForAttentionTarget, 5, VF_CHEAT | VF_CHEAT_NOCHECK,
	                       "Cost used in the heuristic calculation for the danger created by the attention target position.");
	DefineConstIntCVarName("ai_PathfinderDangerCostForExplosives", PathfinderDangerCostForExplosives, 2, VF_CHEAT | VF_CHEAT_NOCHECK,
	                       "Cost used in the heuristic calculation for the danger created by the position of explosive objects.");
	DefineConstIntCVarName("ai_PathfinderAvoidanceCostForGroupMates", PathfinderAvoidanceCostForGroupMates, 2, VF_CHEAT | VF_CHEAT_NOCHECK,
	                       "Cost used in the heuristic calculation for the avoidance of the group mates's positions.");
	REGISTER_CVAR2("ai_PathfinderExplosiveDangerRadius", &PathfinderExplosiveDangerRadius, 5.0f, VF_CHEAT | VF_CHEAT_NOCHECK,
	               "Range used to evaluate the explosive threats in the path calculation. Outside this range a location is considered safe.");
	REGISTER_CVAR2("ai_PathfinderExplosiveDangerMaxThreatDistance", &PathfinderExplosiveDangerMaxThreatDistance, 50.0f, VF_CHEAT | VF_CHEAT_NOCHECK,
	               "Range used to decide if evaluate an explosive danger as an actual threat.");
	REGISTER_CVAR2("ai_PathfinderGroupMatesAvoidanceRadius", &PathfinderGroupMatesAvoidanceRadius, 4.0f, VF_CHEAT | VF_CHEAT_NOCHECK,
	               "Range used to evaluate the group mates avoidance in the path calculation.");

	// Pathfinding dangers

	DefineConstIntCVarName("ai_DebugMovementSystem", DebugMovementSystem, 0, VF_CHEAT | VF_CHEAT_NOCHECK,
	                       "Draw debug information to the screen regarding character movement.");

	DefineConstIntCVarName("ai_DebugMovementSystemActorRequests", DebugMovementSystemActorRequests, 0, VF_CHEAT | VF_CHEAT_NOCHECK,
	                       "Draw queued movement requests of actors in the world as text above their head.\n"
	                       "0 - Off\n"
	                       "1 - Draw request queue of only the currently selected agent\n"
	                       "2 - Draw request queue of all agents");

	DefineConstIntCVarName("ai_OutputPersonalLogToConsole", OutputPersonalLogToConsole, 0, VF_CHEAT | VF_CHEAT_NOCHECK,
	                       "Output the personal log messages to the console.");

	DefineConstIntCVarName("ai_DrawModularBehaviorTreeStatistics", DrawModularBehaviorTreeStatistics, 0, VF_CHEAT | VF_CHEAT_NOCHECK,
	                       "Draw modular behavior statistics to the screen.");

	DefineConstIntCVarName("ai_MNMPathfinderPositionInTrianglePredictionType", MNMPathfinderPositionInTrianglePredictionType, 1, VF_CHEAT | VF_CHEAT_NOCHECK,
	                       "Defines which type of prediction for the point inside each triangle used by the pathfinder heuristic to search for the "
	                       "path with minimal cost.\n"
	                       "0 - Triangle center.\n"
	                       "1 - Advanced prediction.\n");

	REGISTER_CVAR2("ai_MovementSystemPathReplanningEnabled", &MovementSystemPathReplanningEnabled, 0, VF_NULL,
	               "Enable/disable path-replanning of moving actors in the MovementSystem every time\n"
	               "a navigation-mesh change at runtime affects their current path.\n"
	               "Ignores designer-placed paths.\n"
	               "0 - disabled (default)\n"
	               "1 - enabled");

	REGISTER_CVAR2("ai_CollisionAvoidanceRange", &CollisionAvoidanceRange, 10.0f, VF_CHEAT | VF_CHEAT_NOCHECK,
	               "Range for collision avoidance.");
	REGISTER_CVAR2("ai_CollisionAvoidanceTargetCutoffRange", &CollisionAvoidanceTargetCutoffRange, 3.0f, VF_CHEAT | VF_CHEAT_NOCHECK,
	               "Distance from it's current target for an agent to stop avoiding obstacles. Other actors will still avoid the agent.");
	REGISTER_CVAR2("ai_CollisionAvoidancePathEndCutoffRange", &CollisionAvoidancePathEndCutoffRange, 0.5f, VF_CHEAT | VF_CHEAT_NOCHECK,
	               "Distance from it's current path end for an agent to stop avoiding obstacles. Other actors will still avoid the agent.");
	REGISTER_CVAR2("ai_CollisionAvoidanceSmartObjectCutoffRange", &CollisionAvoidanceSmartObjectCutoffRange, 1.0f, VF_CHEAT | VF_CHEAT_NOCHECK,
	               "Distance from it's next smart object for an agent to stop avoiding obstacles. Other actors will still avoid the agent.");
	REGISTER_CVAR2("ai_CollisionAvoidanceAgentExtraFat", &CollisionAvoidanceAgentExtraFat, 0.025f, VF_CHEAT | VF_CHEAT_NOCHECK,
	               "Extra radius to use in Collision Avoidance calculations as a buffer.");
	REGISTER_CVAR2("ai_CollisionAvoidanceRadiusIncrementIncreaseRate", &CollisionAvoidanceRadiusIncrementIncreaseRate, 0.25f, VF_CHEAT | VF_CHEAT_NOCHECK,
	               "Increase rate of the collision avoidance radius increment.");
	REGISTER_CVAR2("ai_CollisionAvoidanceRadiusIncrementDecreaseRate", &CollisionAvoidanceRadiusIncrementDecreaseRate, 2.0f, VF_CHEAT | VF_CHEAT_NOCHECK,
	               "Decrease rate of the collision avoidance radius increment.");
	REGISTER_CVAR2("ai_CollisionAvoidanceTimestep", &CollisionAvoidanceTimeStep, 0.1f, VF_CHEAT | VF_CHEAT_NOCHECK,
	               "TimeStep used to calculate an agent's collision free velocity.");
	REGISTER_CVAR2("ai_CollisionAvoidanceMinSpeed", &CollisionAvoidanceMinSpeed, 0.2f, VF_CHEAT | VF_CHEAT_NOCHECK,
	               "Minimum speed allowed to be used by ORCA.");
	REGISTER_CVAR2("ai_CollisionAvoidanceAgentTimeHorizon", &CollisionAvoidanceAgentTimeHorizon, 2.5f, VF_CHEAT | VF_CHEAT_NOCHECK,
	               "Time horizon used to calculate an agent's collision free velocity against other agents.");
	REGISTER_CVAR2("ai_CollisionAvoidanceObstacleTimeHorizon", &CollisionAvoidanceObstacleTimeHorizon, 1.5f, VF_CHEAT | VF_CHEAT_NOCHECK,
	               "Time horizon used to calculate an agent's collision free velocity against static obstacles.");
	REGISTER_CVAR2("ai_DebugCollisionAvoidanceForceSpeed", &DebugCollisionAvoidanceForceSpeed, 0.0f, VF_CHEAT | VF_CHEAT_NOCHECK,
	               "Force agents velocity to it's current direction times the specified value.");
	REGISTER_CVAR2("ai_DebugDrawCollisionAvoidanceAgentName", &DebugDrawCollisionAvoidanceAgentName, "", VF_CHEAT | VF_CHEAT_NOCHECK,
	               "Name of the agent to draw collision avoidance data for.");
	REGISTER_CVAR2("ai_SOMSpeedRelaxed", &SOMSpeedRelaxed, 0.4f, VF_NULL,
	               "Time before the AI will see the enemy while relaxed.\n"
	               "Usage: ai_SOMSpeedRelaxed 0.4\n"
	               "Default is 0.4. A lower value causes the AI to react to the enemy faster.");
	REGISTER_CVAR2("ai_SOMSpeedCombat", &SOMSpeedCombat, 0.15f, VF_NULL,
	               "Time before the AI will see the enemy while alarmed.\n"
	               "Usage: ai_SOMSpeedCombat 0.15\n"
	               "Default is 0.15. A lower value causes the AI to react to the enemy faster.");

	REGISTER_CVAR2("ai_SightRangeSuperDarkIllumMod", &SightRangeSuperDarkIllumMod, 0.25f, VF_NULL,
	               "Multiplier for sightrange when the target is in super dark light condition.");
	REGISTER_CVAR2("ai_SightRangeDarkIllumMod", &SightRangeDarkIllumMod, 0.5f, VF_NULL,
	               "Multiplier for sightrange when the target is in dark light condition.");
	REGISTER_CVAR2("ai_SightRangeMediumIllumMod", &SightRangeMediumIllumMod, 0.8f, VF_NULL,
	               "Multiplier for sightrange when the target is in medium light condition.");
	REGISTER_CVAR2("ai_PathfindTimeLimit", &AllowedTimeForPathfinding, 0.08f, VF_NULL,
	               "Specifies how many seconds an individual AI can hold the pathfinder blocked\n"
	               "Usage: ai_PathfindTimeLimit 0.15\n"
	               "Default is 0.08. A lower value will result in more path requests that end in NOPATH -\n"
	               "although the path may actually exist.");
	REGISTER_CVAR2("ai_PathfinderUpdateTime", &PathfinderUpdateTime, 0.0005f, VF_NULL,
	               "Maximum pathfinder time per AI update");
	REGISTER_CVAR2("ai_DrawAgentFOV", &DrawAgentFOV, 0, VF_CHEAT | VF_CHEAT_NOCHECK,
	               "Toggles the vision cone of the AI agent.\n"
	               "Usage: ai_DrawagentFOV [0..1]\n"
	               "Default is 0 (off), value 1 will draw the cone all the way to\n"
	               "the sight range, value 0.1 will draw the cone to distance of 10%\n"
	               "of the sight range, etc. ai_DebugDraw must be enabled before\n"
	               "this tool can be used.");
	REGISTER_CVAR2("ai_FilterAgentName", &FilterAgentName, "", VF_CHEAT | VF_CHEAT_NOCHECK,
	               "Only draws the AI info of the agent with the given name.\n"
	               "Usage: ai_FilterAgentName name\n"
	               "Default is none (draws all of them if ai_debugdraw is on)\n");
	REGISTER_CVAR2("ai_AgentStatsDist", &AgentStatsDist, 150, VF_CHEAT | VF_CHEAT_NOCHECK,
	               "Sets agent statistics draw distance, such as current goalpipe, command and target.\n"
	               "Only information on enabled AI objects will be displayed.\n"
	               "To display more information on particular AI agent, use ai_StatsTarget.\n"
	               "Yellow line represents direction where AI is trying to move;\n"
	               "Red line represents direction where AI is trying to look;\n"
	               "Blue line represents forward dir (entity forward);\n"
	               "Usage: ai_AgentStatsDist [view distance]\n"
	               "Default is 20 meters. Works with ai_DebugDraw enabled.");
	REGISTER_CVAR2("ai_DebugDrawArrowLabelsVisibilityDistance", &DebugDrawArrowLabelsVisibilityDistance, 50.0f, VF_CHEAT | VF_CHEAT_NOCHECK,
	               "Provided ai_DebugDraw > 0, if the camera is closer to an agent than this distance,\n"
	               "agent arrows for look/fire/move arrows will have labels.\n"
	               "Usage: ai_DebugDrawArrowLabelsVisibilityDistance [distance]\n"
	               "Default is 50. \n");
	DefineConstIntCVarName("ai_DebugDraw", DebugDraw, -1, VF_CHEAT | VF_CHEAT_NOCHECK,
	                       "Toggles the AI debugging view.\n"
	                       "Usage: ai_DebugDraw [-1/0/1]\n"
	                       "Default is 0 (minimal), value -1 will draw nothing, value 1 displays AI rays and targets \n"
	                       "and enables the view for other AI debugging tools.");
	DefineConstIntCVarName("ai_NetworkDebug", NetworkDebug, 0, VF_CHEAT | VF_CHEAT_NOCHECK,
	                       "Toggles the AI network debug.\n"
	                       "Usage: ai_NetworkDebug [0/1]\n"
	                       "Default is 0 (off). ai_NetworkDebug is used to direct DebugDraw information \n"
	                       "from the server to the client.");
	DefineConstIntCVarName("ai_DebugDrawHideSpotSearchRays", DrawHideSpotSearchRays, 0, VF_CHEAT | VF_CHEAT_NOCHECK,
	                       "Toggle drawing rays used in HM_ONLY_IF_CAN_SHOOT_FROM_THERE hide spot search option.\n"
	                       "Usage: ai_DebugDrawHideSpotSearchRays [0..1]");
	DefineConstIntCVarName("ai_DebugDrawVegetationCollisionDist", DebugDrawVegetationCollisionDist, 0, VF_CHEAT | VF_CHEAT_NOCHECK,
	                       "Enables drawing vegetation collision closer than a distance projected onto the terrain.");
	DefineConstIntCVarName("ai_DebugDrawHidespotRange", DebugDrawHideSpotRange, 0, VF_CHEAT | VF_CHEAT_NOCHECK,
	                       "Sets the range for drawing hidespots around the player (needs ai_DebugDraw > 0).");
	DefineConstIntCVarName("ai_DebugDrawDynamicHideObjectsRange", DebugDrawDynamicHideObjectsRange, 0, VF_CHEAT | VF_CHEAT_NOCHECK,
	                       "Sets the range for drawing dynamic hide objects around the player (needs ai_DebugDraw > 0).");
	DefineConstIntCVarName("ai_DebugDrawVolumeVoxels", DebugDrawVolumeVoxels, 0, VF_CHEAT | VF_CHEAT_NOCHECK,
	                       "Toggles the AI debugging drawing of voxels in volume generation.\n"
	                       "Usage: ai_DebugDrawVolumeVoxels [0, 1, 2 etc]\n"
	                       "Default is 0 (off)\n"
	                       "+n draws all voxels with original value >= n\n"
	                       "-n draws all voxels with original value =  n");
	DefineConstIntCVarName("ai_DebugPathfinding", DebugPathFinding, 0, VF_CHEAT | VF_CHEAT_NOCHECK,
	                       "Toggles output of pathfinding information [default 0 is off]");
	DefineConstIntCVarName("ai_DebugDrawBannedNavsos", DebugDrawBannedNavsos, 0, VF_CHEAT | VF_CHEAT_NOCHECK,
	                       "Toggles drawing banned navsos [default 0 is off]");
	DefineConstIntCVarName("ai_DebugDrawGroups", DebugDrawGroups, 0, VF_CHEAT | VF_CHEAT_NOCHECK,
	                       "Toggles the AI Groups debugging view.\n"
	                       "Usage: ai_DebugDrawGroups [0/1]\n"
	                       "Default is 0 (off). ai_DebugDrawGroups displays groups' leaders, members and their desired positions.");
	DefineConstIntCVarName("ai_DebugDrawCoolMisses", DebugDrawCoolMisses, 0, VF_CHEAT | VF_CHEAT_NOCHECK,
	                       "Toggles displaying the cool miss locations around the player.\n"
	                       "Usage: ai_DebugDrawCoolMisses [0/1]");
	DefineConstIntCVarName("ai_DebugDrawFireCommand", DebugDrawFireCommand, 0, VF_CHEAT | VF_CHEAT_NOCHECK,
	                       "Toggles displaying the fire command targets and modifications.\n"
	                       "Usage: ai_DebugDrawFireCommand [0/1]");
	DefineConstIntCVarName("ai_UseSimplePathfindingHeuristic", UseSimplePathfindingHeuristic, 0, VF_CHEAT | VF_CHEAT_NOCHECK,
	                       "Toggles the AI using a straight simple distance heuristic for debugging.\n"
	                       "Usage: ai_UseSimpleHeuristic [0/1]");
	DefineConstIntCVarName("ai_CoverSystem", CoverSystem, 1, VF_CHEAT | VF_CHEAT_NOCHECK,
	                       "Enables the cover system.\n"
	                       "Usage: ai_CoverSystem [0/1]\n"
	                       "Default is 1 (on)\n"
	                       "0 - off - use anchors\n"
	                       "1 - use offline cover surfaces\n");
	REGISTER_CVAR2("ai_CoverPredictTarget", &CoverPredictTarget, 1.0f, VF_NULL,
	               "Enables simple cover system target location prediction.\n"
	               "Usage: ai_CoverPredictTarget x\n"
	               "Default x is 0.0 (off)\n"
	               "x - how many seconds to look ahead\n");
	REGISTER_CVAR2("ai_CoverSpacing", &CoverSpacing, 0.5f, VF_NULL,
	               "Minimum spacing between agents when choosing cover.\n"
	               "Usage: ai_CoverPredictTarget <x>\n"
	               "x - Spacing width in meters\n");
	DefineConstIntCVarName("ai_CoverExactPositioning", CoverExactPositioning, 0, VF_CHEAT | VF_CHEAT_NOCHECK,
	                       "Enables using exact positioning for arriving at cover.\n"
	                       "Usage: ai_CoverPredictTarget [0/1]\n"
	                       "Default x is 0 (off)\n"
	                       "0 - disable\n"
	                       "1 - enable\n");
	DefineConstIntCVarName("ai_CoverMaxEyeCount", CoverMaxEyeCount, 2, VF_CHEAT | VF_CHEAT_NOCHECK,
	                       "Max numbers of observers to consider when selecting cover.\n"
	                       "Usage: ai_CoverMaxEyeCount <x>\n");
	DefineConstIntCVarName("ai_DebugDrawCover", DebugDrawCover, 0, VF_CHEAT | VF_CHEAT_NOCHECK,
	                       "Displays cover debug information.\n"
	                       "Usage: ai_DebugDrawCover [0/1/2]\n"
	                       "Default is 0 (off)\n"
	                       "0 - off\n"
	                       "1 - currently being used\n"
	                       "2 - all in 50m range (slow)\n");
	DefineConstIntCVarName("ai_DebugDrawCoverOccupancy", DebugDrawCoverOccupancy, 0, VF_CHEAT | VF_CHEAT_NOCHECK,
	                       "Renders red balls at the location of occupied cover.\n"
	                       "Usage: ai_DebugDrawCoverOccupancy [0/1]\n"
	                       "Default is 0 (off)\n"
	                       "0 - off\n"
	                       "1 - render red balls at the location of occupied cover\n");
	DefineConstIntCVarName("ai_DebugDrawNavigation", DebugDrawNavigation, 0, VF_CHEAT | VF_CHEAT_NOCHECK,
	                       "Displays the navigation debug information.\n"
	                       "Usage: ai_DebugDrawNavigation [0/1]\n"
	                       "Default is 0 (off)\n"
	                       "0 - off\n"
	                       "1 - triangles and contour\n"
	                       "2 - triangles, mesh and contours\n"
	                       "3 - triangles, mesh contours and external links\n"
	                       "4 - triangles, mesh contours, external links and triangle IDs\n"
	                       "5 - triangles, mesh contours, external links and island IDs\n"
	                       "6 - triangles with backfaces, mesh contours and external links\n");
	DefineConstIntCVarName("ai_MNMDebugTriangleOnCursor", DebugTriangleOnCursor, 0, VF_CHEAT | VF_CHEAT_NOCHECK,
		"Displays the basic information about the MNM Triangle where the cursor is pointing.\n"
		"Usage: ai_MNMDebugTriangleOnCursor [0/1]\n"
		"Default is 0 (off)\n"
		"0 - off\n"
		"1 - show triangle information\n");
	DefineConstIntCVarName("ai_IslandConnectionsSystemProfileMemory", IslandConnectionsSystemProfileMemory, 0, VF_CHEAT | VF_CHEAT_NOCHECK,
	                       "Enables/Disables the memory profile for the island connections system.");
	DefineConstIntCVarName("ai_NavigationSystemMT", NavigationSystemMT, 1, VF_CHEAT | VF_CHEAT_NOCHECK,
	                       "Enables navigation information updates on a separate thread.\n"
	                       "Usage: ai_NavigationSystemMT [0/1]\n"
	                       "Default is 1 (on)\n"
	                       "0 - off\n"
	                       "1 - on\n");
	DefineConstIntCVarName("ai_NavGenThreadJobs", NavGenThreadJobs, 1, VF_CHEAT | VF_CHEAT_NOCHECK,
	                       "Number of tile generation jobs per thread per frame.\n"
	                       "Usage: ai_NavGenThreadJobs [1+]\n"
	                       "Default is 1. The more you have, the faster it will go but the frame rate will drop while it works.\n"
	                       "Recommendations:\n"
	                       " Fast machine [10]\n"
	                       " Slow machine [4]\n"
	                       " Smooth [1]\n");
	REGISTER_CVAR2("ai_MNMDebugDrawFlag", &MNMDebugDrawFlag, "", VF_CHEAT | VF_CHEAT_NOCHECK,
		"Color the MNM triangles is overriden by the specified annotation flag color.\n"
		"Usage: ai_MNMDebugDrawFlag 'flagName'\n"
		"Default is 'empty'\n");
	REGISTER_CVAR2("ai_NavmeshTileDistanceDraw", &NavmeshTileDistanceDraw, 200.0f, VF_CHEAT | VF_CHEAT_NOCHECK,
	               "Maximum distance from the camera for tile to be drawn.\n"
	               "Usage: ai_NavmeshTileDistanceDraw [0.0-...]\n"
	               "Default is 200.0\n");
	REGISTER_CVAR2("ai_NavmeshStabilizationTimeToUpdate", &NavmeshStabilizationTimeToUpdate, 0.3f, VF_CHEAT | VF_CHEAT_NOCHECK,
		"Time that navmesh needs to be without any new updates to apply the latest changes.\n"
		"Usage: ai_NavmeshStabilizationTimeToUpdate [0.0-...]\n"
		"Default is 0.3\n");
	DefineConstIntCVarName("ai_DebugDrawNavigationWorldMonitor", DebugDrawNavigationWorldMonitor, 0, VF_CHEAT | VF_CHEAT_NOCHECK,
	                       "Enables displaying bounding boxes for world changes.\n"
	                       "Usage: ai_DebugDrawNavigationWorldMonitor [0/1]\n"
	                       "Default is 0 (off)\n"
	                       "0 - off\n"
	                       "1 - on\n");
	DefineConstIntCVarName("ai_DebugDrawCoverPlanes", DebugDrawCoverPlanes, 0, VF_CHEAT | VF_CHEAT_NOCHECK,
	                       "Displays cover planes.\n"
	                       "Usage: ai_DebugDrawCoverPlanes [0/1]\n"
	                       "Default is 0 (off)\n");
	DefineConstIntCVarName("ai_DebugDrawCoverLocations", DebugDrawCoverLocations, 0, VF_CHEAT | VF_CHEAT_NOCHECK,
	                       "Displays cover locations.\n"
	                       "Usage: ai_DebugDrawCoverLocations [0/1]\n"
	                       "Default is 0 (off)\n");
	DefineConstIntCVarName("ai_DebugDrawCoverSampler", DebugDrawCoverSampler, 0, VF_CHEAT | VF_CHEAT_NOCHECK,
	                       "Displays cover sampler debug rendering.\n"
	                       "Usage: ai_DebugDrawCoverSampler [0/1/2/3]\n"
	                       "Default is 0 (off)\n"
	                       "0 - off\n"
	                       "1 - display floor sampling\n"
	                       "2 - display surface sampling\n"
	                       "3 - display both\n");
	DefineConstIntCVarName("ai_DebugDrawDynamicCoverSampler", DebugDrawDynamicCoverSampler, 0, VF_CHEAT | VF_CHEAT_NOCHECK,
	                       "Displays dynamic cover sampler debug rendering.\n"
	                       "Usage: ai_DebugDrawDynamicCoverSampler [0/1]\n"
	                       "Default is 0 (off)\n"
	                       "0 - off\n"
	                       "1 - on\n");
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
	DefineConstIntCVarName("ai_DrawNodeLinkType", DrawNodeLinkType, 0, VF_CHEAT | VF_CHEAT_NOCHECK,
	                       "Sets the link parameter to draw with ai_DrawNode.\n"
	                       "Values are:\n"
	                       " 0 - pass radius (default)\n"
	                       " 1 - exposure\n"
	                       " 2 - water max depth\n"
	                       " 3 - water min depth");
	REGISTER_CVAR2("ai_DrawNodeLinkCutoff", &DrawNodeLinkCutoff, 0.0f, VF_CHEAT | VF_CHEAT_NOCHECK,
	               "Sets the link cutoff value in ai_DrawNodeLinkType. If the link value is more than\n"
	               "ai_DrawNodeLinkCutoff the number gets displayed in green, otherwise red.");
	REGISTER_CVAR2("ai_SystemUpdate", &AiSystem, 1, VF_CHEAT | VF_CHEAT_NOCHECK,
	               "Toggles the regular AI system update.\n"
	               "Usage: ai_SystemUpdate [0/1]\n"
	               "Default is 1 (on). Set to 0 to disable ai system updating.");
	DefineConstIntCVarName("ai_SoundPerception", SoundPerception, 1, VF_CHEAT | VF_CHEAT_NOCHECK,
	                       "Toggles AI sound perception.\n"
	                       "Usage: ai_SoundPerception [0/1]\n"
	                       "Default is 1 (on). Used to prevent AI from hearing sounds for\n"
	                       "debugging purposes. Works with ai_DebugDraw enabled.");
	DefineConstIntCVarName("ai_IgnorePlayer", IgnorePlayer, 0, VF_CHEAT | VF_CHEAT_NOCHECK,
	                       "Makes AI ignore the player.\n"
	                       "Usage: ai_IgnorePlayer [0/1]\n"
	                       "Default is 0 (off). Set to 1 to make AI ignore the player.\n"
	                       "Used with ai_DebugDraw enabled.");
	DefineConstIntCVarName("ai_IgnoreVisibilityChecks", IgnoreVisibilityChecks, 0, VF_CHEAT | VF_CHEAT_NOCHECK,
	                       "Makes certain visibility checks (for teleporting etc) return false.");
	DefineConstIntCVarName("ai_DrawFormations", DrawFormations, 0, VF_CHEAT | VF_CHEAT_NOCHECK,
	                       "Draws all the currently active formations of the AI agents.\n"
	                       "Usage: ai_DrawFormations [0/1]\n"
	                       "Default is 0 (off). Set to 1 to draw the AI formations.");
	DefineConstIntCVarName("ai_DrawSmartObjects", DrawSmartObjects, 0, VF_CHEAT | VF_CHEAT_NOCHECK,
	                       "Draws smart object debug information.\n"
	                       "Usage: ai_DrawSmartObjects [0/1]\n"
	                       "Default is 0 (off). Set to 1 to draw the smart objects.");
	DefineConstIntCVarName("ai_DrawReadibilities", DrawReadibilities, 0, VF_CHEAT | VF_CHEAT_NOCHECK,
	                       "Draws all the currently active readibilities of the AI agents.\n"
	                       "Usage: ai_DrawReadibilities [0/1]\n"
	                       "Default is 0 (off). Set to 1 to draw the AI readibilities.");
	DefineConstIntCVarName("ai_DrawGoals", DrawGoals, 0, VF_CHEAT | VF_CHEAT_NOCHECK,
	                       "Draws all the active goal ops debug info.\n"
	                       "Usage: ai_DrawGoals [0/1]\n"
	                       "Default is 0 (off). Set to 1 to draw the AI goal op debug info.");
	DefineConstIntCVarName("ai_AllTime", AllTime, 0, VF_CHEAT | VF_CHEAT_NOCHECK,
	                       "Displays the update times of all agents, in milliseconds.\n"
	                       "Usage: ai_AllTime [0/1]\n"
	                       "Default is 0 (off). Times all agents and displays the time used updating\n"
	                       "each of them. The name is colour coded to represent the update time.\n"
	                       "	Green: less than 1 ms (ok)\n"
	                       "	White: 1 ms to 5 ms\n"
	                       "	Red: more than 5 ms\n"
	                       "You must enable ai_DebugDraw before you can use this tool.");
	DefineConstIntCVarName("ai_ProfileGoals", ProfileGoals, 0, VF_CHEAT | VF_CHEAT_NOCHECK,
	                       "Toggles timing of AI goal execution.\n"
	                       "Usage: ai_ProfileGoals [0/1]\n"
	                       "Default is 0 (off). Records the time used for each AI goal (like\n"
	                       "approach, run or pathfind) to execute. The longest execution time\n"
	                       "is displayed on screen. Used with ai_DebugDraw enabled.");
	DefineConstIntCVarName("ai_BeautifyPath", BeautifyPath, 1, VF_CHEAT | VF_CHEAT_NOCHECK,
	                       "Toggles AI optimisation of the generated path.\n"
	                       "Usage: ai_BeautifyPath [0/1]\n"
	                       "Default is 1 (on). Optimisation is on by default. Set to 0 to\n"
	                       "disable path optimisation (AI uses non-optimised path).");
	DefineConstIntCVarName("ai_PathStringPullingIterations", PathStringPullingIterations, 5, VF_CHEAT | VF_CHEAT_NOCHECK,
	                       "Defines the number of iteration used for the string pulling operation to simplify the path");
	DefineConstIntCVarName("ai_AttemptStraightPath", AttemptStraightPath, 1, VF_CHEAT | VF_CHEAT_NOCHECK,
	                       "Toggles AI attempting a simple straight path when possible.\n"
	                       "Default is 1 (on).");
	DefineConstIntCVarName("ai_CrowdControlInPathfind", CrowdControlInPathfind, 0, VF_CHEAT | VF_CHEAT_NOCHECK,
	                       "Toggles AI using crowd control in pathfinding.\n"
	                       "Usage: ai_CrowdControlInPathfind [0/1]\n"
	                       "Default is 0 (off).");
	DefineConstIntCVarName("ai_DebugDrawCrowdControl", DebugDrawCrowdControl, 0, VF_CHEAT | VF_CHEAT_NOCHECK,
	                       "Draws crowd control debug information. 0=off, 1=on");
	DefineConstIntCVarName("ai_PredictivePathFollowing", PredictivePathFollowing, 1, VF_CHEAT | VF_CHEAT_NOCHECK,
	                       "Sets if AI should use the predictive path following if allowed by the type's config.");
	REGISTER_CVAR2("ai_SteepSlopeUpValue", &SteepSlopeUpValue, 1.0f, VF_CHEAT | VF_CHEAT_NOCHECK,
	               "Indicates slope value that is borderline-walkable up.\n"
	               "Usage: ai_SteepSlopeUpValue 0.5\n"
	               "Default is 1.0 Zero means flat. Infinity means vertical. Set it smaller than ai_SteepSlopeAcrossValue");
	REGISTER_CVAR2("ai_SteepSlopeAcrossValue", &SteepSlopeAcrossValue, 0.6f, VF_CHEAT | VF_CHEAT_NOCHECK,
	               "Indicates slope value that is borderline-walkable across.\n"
	               "Usage: ai_SteepSlopeAcrossValue 0.8\n"
	               "Default is 0.6 Zero means flat. Infinity means vertical. Set it greater than ai_SteepSlopeUpValue");
	DefineConstIntCVarName("ai_UpdateProxy", UpdateProxy, 1, VF_CHEAT | VF_CHEAT_NOCHECK,
	                       "Toggles update of AI proxy (model).\n"
	                       "Usage: ai_UpdateProxy [0/1]\n"
	                       "Default is 1 (on). Updates proxy (AI representation in game)\n"
	                       "set to 0 to disable proxy updating.");
	DefineConstIntCVarName("ai_DrawType", DrawType, -1, VF_CHEAT | VF_CHEAT_NOCHECK,
	                       "Display all AI object of specified type. If object is enabled it will be displayed.\n"
	                       "with blue ball, otherwise with red ball. Yellow line will represent forward direction of the object.\n"
	                       " <0 - off\n"
	                       " 0 - display all the dummy objects\n"
	                       " >0 - type of AI objects to display");
	DefineConstIntCVarName("ai_DrawModifiers", DrawModifiers, 0, VF_CHEAT | VF_CHEAT_NOCHECK,
	                       "Toggles the AI debugging view of navigation modifiers.");
	REGISTER_CVAR2("ai_DrawOffset", &DebugDrawOffset, 0.1f, VF_CHEAT | VF_CHEAT_NOCHECK,
	               "vertical offset during debug drawing (graph nodes, navigation paths, ...)");
	DefineConstIntCVarName("ai_DrawTargets", DrawTargets, 0, VF_CHEAT | VF_CHEAT_NOCHECK,
	                       "Distance to display the perception events of all enabled puppets.\n"
	                       "Displays target type and priority");
	DefineConstIntCVarName("ai_DrawBadAnchors", DrawBadAnchors, -1, VF_CHEAT | VF_CHEAT_NOCHECK,
	                       "Toggles drawing out of bounds AI objects of particular type for debugging AI.\n"
	                       "Valid only for 3D navigation. Draws red spheres at positions of anchors which are\n"
	                       "located out of navigation volumes. Those anchors have to be moved.\n"
	                       " 0 - off, 1 - on");
	DefineConstIntCVarName("ai_DrawStats", DrawStats, 0, VF_CHEAT | VF_CHEAT_NOCHECK,
	                       "Toggles drawing stats (in a table on top left of screen) for AI objects within specified range.\n"
	                       "Will display attention target, goal pipe and current goal.");
	DefineConstIntCVarName("ai_DrawAttentionTargetPositions", DrawAttentionTargetsPosition, 0, VF_CHEAT | VF_CHEAT_NOCHECK,
	                       "Displays markers for the AI's current attention target position. ");
	REGISTER_CVAR2("ai_ExtraRadiusDuringBeautification", &ExtraRadiusDuringBeautification, 0.2f, VF_CHEAT | VF_CHEAT_NOCHECK,
	               "Extra radius added to agents during beautification.");
	REGISTER_CVAR2("ai_ExtraForbiddenRadiusDuringBeautification", &ExtraForbiddenRadiusDuringBeautification, 1.0f, VF_CHEAT | VF_CHEAT_NOCHECK,
	               "Extra radius added to agents close to forbidden edges during beautification.");
	DefineConstIntCVarName("ai_RecordLog", RecordLog, 0, VF_CHEAT | VF_CHEAT_NOCHECK,
	                       "log all the AI state changes on stats_target");
	DefineConstIntCVarName("ai_DrawGroupTactic", DrawGroupTactic, 0, VF_CHEAT | VF_CHEAT_NOCHECK,
	                       "draw group tactic: 0 = disabled, 1 = draw simple, 2 = draw complex.");
	DefineConstIntCVarName("ai_DrawTrajectory", DrawTrajectory, 0, VF_CHEAT | VF_CHEAT_NOCHECK,
	                       "Record and draw the actual path taken by the agent specified in ai_StatsTarget.\n"
	                       "Path is displayed in aqua, and only a certain length will be displayed, after which\n"
	                       "old path gradually disappears as new path is drawn.\n"
	                       "0=do not record, 1=record.");
	DefineConstIntCVarName("ai_DrawHidespots", DrawHideSpots, 0, VF_CHEAT | VF_CHEAT_NOCHECK,
	                       "Draws latest hide-spot positions for all agents within specified range.");
	DefineConstIntCVarName("ai_DrawRadar", DrawRadar, 0, VF_CHEAT | VF_CHEAT_NOCHECK,
	                       "Draws AI radar: 0=no radar, >0 = size of the radar on screen");
	DefineConstIntCVarName("ai_DrawRadarDist", DrawRadarDist, 20, VF_CHEAT | VF_CHEAT_NOCHECK,
	                       "AI radar draw distance in meters, default=20m.");
	// should be off for release
	DefineConstIntCVarName("ai_Recorder_Auto", DebugRecordAuto, 0, VF_CHEAT | VF_CHEAT_NOCHECK,
	                       "Auto record the AI when in Editor mode game\n");
	DefineConstIntCVarName("ai_Recorder_Buffer", DebugRecordBuffer, 1024, VF_CHEAT | VF_CHEAT_NOCHECK,
	                       "Set the size of the AI debug recording buffer");
	DefineConstIntCVarName("ai_DrawDistanceLUT", DrawDistanceLUT, 0, VF_CHEAT | VF_CHEAT_NOCHECK,
	                       "Draws the distance lookup table graph overlay.");
	DefineConstIntCVarName("ai_DrawAreas", DrawAreas, 0, VF_CHEAT | VF_CHEAT_NOCHECK,
	                       "Enables/Disables drawing behavior related areas.");
	REGISTER_CVAR2("ai_BurstWhileMovingDestinationRange", &BurstWhileMovingDestinationRange, 2.0f, 0,
	               "When using FIREMODE_BURST_WHILE_MOVING - only fire when within this distance to the destination.");
	DefineConstIntCVarName("ai_DrawProbableTarget", DrawProbableTarget, 0, VF_CHEAT | VF_CHEAT_NOCHECK,
	                       "Enables/Disables drawing the position of probable target.");
	DefineConstIntCVarName("ai_DebugDrawDamageParts", DebugDrawDamageParts, 0, VF_CHEAT | VF_CHEAT_NOCHECK,
	                       "Draws the damage parts of puppets and vehicles.");
	DefineConstIntCVarName("ai_DebugDrawStanceSize", DebugDrawStanceSize, 0, VF_CHEAT | VF_CHEAT_NOCHECK,
	                       "Draws the game logic representation of the stance size of the AI agents.");

	DefineConstIntCVarName("ai_DebugTargetSilhouette", DebugTargetSilhouette, 0, VF_CHEAT | VF_CHEAT_NOCHECK,
	                       "Draws the silhouette used for missing the target while shooting.");

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

	DefineConstIntCVarName("ai_AmbientFireEnable", AmbientFireEnable, 1, VF_CHEAT | VF_CHEAT_NOCHECK,
	                       "Enable ambient fire system.");
	REGISTER_CVAR2("ai_AmbientFireUpdateInterval", &AmbientFireUpdateInterval, 1.0f, VF_NULL,
	               "Ambient fire update interval. Controls how often puppet's ambient fire status is updated.");
	REGISTER_CVAR2("ai_AmbientFireQuota", &AmbientFireQuota, 2, 0,
	               "Number of units allowed to hit the player at a time.");
	DefineConstIntCVarName("ai_DebugDrawAmbientFire", DebugDrawAmbientFire, 0, VF_CHEAT | VF_CHEAT_NOCHECK,
	                       "Displays fire quota on puppets.");

	DefineConstIntCVarName("ai_PlayerAffectedByLight", PlayerAffectedByLight, 1, VF_CHEAT | VF_CHEAT_NOCHECK,
	                       "Sets if player is affected by light from observable perception checks");

	DefineConstIntCVarName("ai_DebugDrawExpensiveAccessoryQuota", DebugDrawExpensiveAccessoryQuota, 0, VF_CHEAT | VF_CHEAT_NOCHECK,
	                       "Displays expensive accessory usage quota on puppets.");

	DefineConstIntCVarName("ai_DebugDrawDamageControl", DebugDrawDamageControl, 0, VF_CHEAT | VF_CHEAT_NOCHECK,
	                       "Debugs the damage control system 0=disabled, 1=collect, 2=collect&draw.");

	DefineConstIntCVarName("ai_DrawFakeTracers", DrawFakeTracers, 0, VF_CHEAT | VF_CHEAT_NOCHECK,
	                       "Draws fake tracers around the player.");
	DefineConstIntCVarName("ai_DrawFakeHitEffects", DrawFakeHitEffects, 0, VF_CHEAT | VF_CHEAT_NOCHECK,
	                       "Draws fake hit effects the player.");
	DefineConstIntCVarName("ai_DrawFakeDamageInd", DrawFakeDamageInd, 0, VF_CHEAT | VF_CHEAT_NOCHECK,
	                       "Draws fake damage indicators on the player.");
	DefineConstIntCVarName("ai_DrawPlayerRanges", DrawPlayerRanges, 0, VF_CHEAT | VF_CHEAT_NOCHECK,
	                       "Draws rings around player to assist in gauging target distance");
	DefineConstIntCVarName("ai_DrawPerceptionIndicators", DrawPerceptionIndicators, 0, VF_CHEAT | VF_CHEAT_NOCHECK,
	                       "Draws indicators showing enemy current perception level of player");
	DefineConstIntCVarName("ai_DrawPerceptionDebugging", DrawPerceptionDebugging, 0, VF_CHEAT | VF_CHEAT_NOCHECK,
	                       "Draws indicators showing enemy view intersection with perception modifiers");
	DefineConstIntCVarName("ai_DrawPerceptionModifiers", DrawPerceptionModifiers, 0, VF_CHEAT | VF_CHEAT_NOCHECK,
	                       "Draws perception modifier areas in game mode");
	DefineConstIntCVarName("ai_DebugGlobalPerceptionScale", DebugGlobalPerceptionScale, 0, VF_CHEAT | VF_CHEAT_NOCHECK,
	                       "Draws global perception scale multipliers on screen");
	DefineConstIntCVarName("ai_TargetTracking", TargetTracking, 1, VF_CHEAT | VF_CHEAT_NOCHECK,
	                       "Enable/disable target tracking. 0=disable, any other value = Enable");
	DefineConstIntCVarName("ai_TargetTracks_GlobalTargetLimit", TargetTracks_GlobalTargetLimit, 0, VF_CHEAT | VF_CHEAT_NOCHECK,
	                       "Global override to control the number of agents that can actively target another agent (unless there is no other choice)\n"
	                       "A value of 0 means no global limit is applied. If the global target limit is less than the agent's target limit, the global limit is used.");
	DefineConstIntCVarName("ai_DebugTargetTracksTarget", TargetTracks_TargetDebugDraw, 0, VF_CHEAT | VF_CHEAT_NOCHECK,
	                       "Draws lines to illustrate where each agent's target is\n"
	                       "Usage: ai_DebugTargetTracking 0/1/2\n"
	                       "0 = Off. 1 = Show best target. 2 = Show all possible targets.");
	REGISTER_CVAR2("ai_DebugTargetTracksAgent", &TargetTracks_AgentDebugDraw, "none", VF_CHEAT | VF_CHEAT_NOCHECK,
	               "Draws the target tracks for the given agent\n"
	               "Usage: ai_DebugTargetTracksAgent AIName\n"
	               "Default is 'none'. AIName is the name of the AI agent to debug");
	DefineConstIntCVarName("ai_DebugTargetTracksConfig", TargetTracks_ConfigDebugDraw, 0, VF_CHEAT | VF_CHEAT_NOCHECK,
	                       "Draws the information contained in the loaded target track configurations to the screen");
	REGISTER_CVAR2("ai_DebugTargetTracksConfig_Filter", &TargetTracks_ConfigDebugFilter, "none", VF_CHEAT | VF_CHEAT_NOCHECK,
	               "Filter what configurations are drawn when debugging target tracks\n"
	               "Usage: ai_DebugTargetTracks_Filter Filter\n"
	               "Default is 'none'. Filter is a substring that must be in the configuration name");

	REGISTER_CVAR2("ai_DrawSelectedTargets", &DrawSelectedTargets, 0, VF_CHEAT | VF_CHEAT_NOCHECK,
	               "[0-1] Enable/Disable the debug helpers showing the AI's selected targets.");

	DefineConstIntCVarName("ai_ForceStance", ForceStance, -1, VF_CHEAT | VF_CHEAT_NOCHECK,
	                       "Forces all AI characters to specified stance:\n"
	                       "Disable = -1, Stand = 0, Crouch = 1, Prone = 2, Relaxed = 3, Stealth = 4, Cover = 5, Swim = 6, Zero-G = 7");

	DefineConstIntCVarName("ai_ForceAllowStrafing", ForceAllowStrafing, -1, VF_CHEAT | VF_CHEAT_NOCHECK,
	                       "Forces all AI characters to use/not use strafing (-1 disables)");

	DefineConstIntCVarName("ai_InterestSystem", InterestSystem, 1, VF_CHEAT | VF_CHEAT_NOCHECK,
	                       "Enable interest system");
	DefineConstIntCVarName("ai_DebugInterestSystem", DebugInterest, 0, VF_CHEAT | VF_CHEAT_NOCHECK,
	                       "Display debugging information on interest system");
	DefineConstIntCVarName("ai_InterestSystemCastRays", InterestSystemCastRays, 1, VF_CHEAT | VF_CHEAT_NOCHECK,
	                       "Makes the Interest System check visibility with rays");

	REGISTER_CVAR2("ai_BannedNavSoTime", &BannedNavSoTime, 15.0f, VF_NULL,
	               "Time indicating how long invalid navsos should be banned.");
	DefineConstIntCVarName("ai_DebugDrawAdaptiveUrgency", DebugDrawAdaptiveUrgency, 0, VF_CHEAT | VF_CHEAT_NOCHECK,
	                       "Enables drawing the adaptive movement urgency.");
	DefineConstIntCVarName("ai_DebugDrawReinforcements", DebugDrawReinforcements, -1, VF_CHEAT | VF_CHEAT_NOCHECK,
	                       "Enables debug draw for reinforcement logic for specified group.\n"
	                       "Usage: ai_DebugDrawReinforcements <groupid>, or -1 to disable.");
	DefineConstIntCVarName("ai_DebugDrawPlayerActions", DebugDrawPlayerActions, 0, VF_CHEAT | VF_CHEAT_NOCHECK,
	                       "Debug draw special player actions.");

	DefineConstIntCVarName("ai_EnableWaterOcclusion", WaterOcclusionEnable, 1, VF_CHEAT | VF_CHEAT_NOCHECK,
	                       "Enables applying water occlusion to AI target visibility checks");
	REGISTER_CVAR2("ai_WaterOcclusion", &WaterOcclusionScale, .5f, VF_NULL,
	               "scales how much water hides player from AI");

	DefineConstIntCVarName("ai_DebugDrawEnabledActors", DebugDrawEnabledActors, 0, VF_CHEAT | VF_CHEAT_NOCHECK,
	                       "list of AI Actors that are enabled and metadata");
	DefineConstIntCVarName("ai_DebugDrawEnabledPlayers", DebugDrawEnabledPlayers, 0, VF_CHEAT | VF_CHEAT_NOCHECK,
	                       "list of AI players that are enabled and metadata");

	DefineConstIntCVarName("ai_DrawUpdate", DebugDrawUpdate, 0, VF_CHEAT | VF_CHEAT_NOCHECK,
	                       "list of AI forceUpdated entities");

	DefineConstIntCVarName("ai_DebugDrawLightLevel", DebugDrawLightLevel, 0, VF_CHEAT | VF_CHEAT_NOCHECK,
	                       "Debug AI light level manager");

	DefineConstIntCVarName("ai_SimpleWayptPassability", SimpleWayptPassability, 1, VF_CHEAT | VF_CHEAT_NOCHECK,
	                       "Use simplified and faster passability recalculation for human waypoint links where possible.");

	REGISTER_CVAR2("ai_MinActorDynamicObstacleAvoidanceRadius", &MinActorDynamicObstacleAvoidanceRadius, 0.6f, VF_NULL,
	               "Minimum value in meters to be added to the obstacle's own size for actors\n"
	               "(pathRadius property can override it if bigger)");
	REGISTER_CVAR2("ai_ExtraVehicleAvoidanceRadiusBig", &ExtraVehicleAvoidanceRadiusBig, 4.0f, VF_NULL,
	               "Value in meters to be added to a big obstacle's own size while computing obstacle\n"
	               "size for purposes of vehicle steering. See also ai_ObstacleSizeThreshold.");
	REGISTER_CVAR2("ai_ExtraVehicleAvoidanceRadiusSmall", &ExtraVehicleAvoidanceRadiusSmall, 0.5f, VF_NULL,
	               "Value in meters to be added to a big obstacle's own size while computing obstacle\n"
	               "size for purposes of vehicle steering. See also ai_ObstacleSizeThreshold.");
	REGISTER_CVAR2("ai_ObstacleSizeThreshold", &ObstacleSizeThreshold, 1.2f, VF_NULL,
	               "Obstacle size in meters that differentiates small obstacles from big ones so that vehicles can ignore the small ones");
	REGISTER_CVAR2("ai_DrawGetEnclosingFailures", &DrawGetEnclosingFailures, 0.0f, VF_CHEAT | VF_CHEAT_NOCHECK,
	               "Set to the number of seconds you want GetEnclosing() failures visualized.  Set to 0 to turn visualization off.");

	DefineConstIntCVarName("ai_EnablePerceptionStanceVisibleRange", EnablePerceptionStanceVisibleRange, 0, VF_CHEAT | VF_CHEAT_NOCHECK, "Turn on use of max perception range for AI based on player's stance");
	REGISTER_CVAR2("ai_CrouchVisibleRange", &CrouchVisibleRange, 15.0f, VF_CHEAT | VF_CHEAT_NOCHECK, "Max perception range for AI when player is crouching");
	REGISTER_CVAR2("ai_ProneVisibleRange", &ProneVisibleRange, 6.0f, VF_CHEAT | VF_CHEAT_NOCHECK, "Max perception range for AI when player is proning");
	DefineConstIntCVarName("ai_IgnoreVisualStimulus", IgnoreVisualStimulus, 0, VF_CHEAT | VF_CHEAT_NOCHECK, "Have the Perception Handler ignore all visual stimulus always");
	DefineConstIntCVarName("ai_IgnoreSoundStimulus", IgnoreSoundStimulus, 0, VF_CHEAT | VF_CHEAT_NOCHECK, "Have the Perception Handler ignore all sound stimulus always");
	DefineConstIntCVarName("ai_IgnoreBulletRainStimulus", IgnoreBulletRainStimulus, 0, VF_CHEAT | VF_CHEAT_NOCHECK, "Have the Perception Handler ignore all bullet rain stimulus always");

	REGISTER_CVAR2("ai_MNMPathFinderQuota", &MNMPathFinderQuota, 0.001f, VF_CHEAT | VF_CHEAT_NOCHECK,
	               "Set path finding frame time quota in seconds (Set to 0 for no limit)");
	REGISTER_CVAR2("ai_MNMPathFinderDebug", &MNMPathFinderDebug, 0, VF_CHEAT | VF_CHEAT_NOCHECK,
	               "[0-1] Enable/Disable debug draw statistics on pathfinder load");

	REGISTER_CVAR2("ai_MNMProfileMemory", &MNMProfileMemory, 0, VF_CHEAT | VF_CHEAT_NOCHECK,
	               "[0-1] Display navigation system memory statistics");

	REGISTER_CVAR2("ai_MNMDebugAccessibility", &MNMDebugAccessibility, 0, VF_CHEAT | VF_CHEAT_NOCHECK,
	               "[0-1] Display navigation reachable areas in blue and not reachable areas in red");

	REGISTER_CVAR2("ai_MNMEditorBackgroundUpdate", &MNMEditorBackgroundUpdate, 1, VF_NULL,
	               "[0-1] Enable/Disable editor background update of the Navigation Meshes");

	DefineConstIntCVarName("ai_UseSmartPathFollower", UseSmartPathFollower, 1, VF_CHEAT | VF_CHEAT_NOCHECK, "Enables Smart PathFollower (default: 1).");
	DefineConstIntCVarName("ai_UseSmartPathFollower_AABB_based", SmartpathFollower_UseAABB_CheckWalkibility, 1, VF_CHEAT | VF_CHEAT_NOCHECK, "Enables Smart PathFollower to use AABB checks and other optimizations");
	REGISTER_CVAR2("ai_UseSmartPathFollower_LookAheadDistance", &SmartPathFollower_LookAheadDistance, 10.0f, VF_NULL, "LookAheadDistance of SmartPathFollower");
	REGISTER_CVAR2("ai_SmartPathFollower_LookAheadPredictionTimeForMovingAlongPathWalk", &SmartPathFollower_LookAheadPredictionTimeForMovingAlongPathWalk, 0.5f, VF_NULL,
	               "Defines the time frame the AI is allowed to look ahead while moving strictly along a path to decide whether to cut towards the next point. (Walk only)\n");
	REGISTER_CVAR2("ai_SmartPathFollower_LookAheadPredictionTimeForMovingAlongPathRunAndSprint", &SmartPathFollower_LookAheadPredictionTimeForMovingAlongPathRunAndSprint, 0.25f, VF_NULL,
	               "Defines the time frame the AI is allowed to look ahead while moving strictly along a path to decide whether to cut towards the next point. (Run and Sprint only)\n");
	REGISTER_CVAR2("ai_SmartPathFollower_decelerationHuman", &SmartPathFollower_decelerationHuman, 7.75f, VF_NULL, "Deceleration multiplier for non-vehicles");
	REGISTER_CVAR2("ai_SmartPathFollower_decelerationVehicle", &SmartPathFollower_decelerationVehicle, 1.0f, VF_NULL, "Deceleration multiplier for vehicles");

	DefineConstIntCVarName("ai_MNMPathfinderMT", MNMPathfinderMT, 1, VF_CHEAT | VF_CHEAT_NOCHECK, "Enable/Disable Multi Threading for the pathfinder.");
	DefineConstIntCVarName("ai_MNMPathfinderConcurrentRequests", MNMPathfinderConcurrentRequests, 4, VF_CHEAT | VF_CHEAT_NOCHECK,
	                       "Defines the amount of concurrent pathfinder requests that can be served at the same time.");

	DefineConstIntCVarName("ai_MNMRaycastImplementation", MNMRaycastImplementation, 2, VF_CHEAT | VF_CHEAT_NOCHECK,
	                       "Defines which type of raycast implementation to use on the MNM meshes."
	                       "0 - Version 1. This version will be deprecated as it sometimes does not handle correctly the cases where the ray coincides with triangle egdes, which has been fixed in the new version.\n"
	                       "1 - Version 2. This version also fails in some cases when the ray goes exactly through triangle corners and then hitting the wall. \n"
	                       "2 - Version 3. The newest version that should be working in all special cases. This version is around 40% faster then the previous one. \n"
	                       "Any other value is used for the biggest version.");

	DefineConstIntCVarName("ai_SmartPathFollower_useAdvancedPathShortcutting", SmartPathFollower_useAdvancedPathShortcutting, 1, VF_NULL, "Enables a more failsafe way of preventing the AI to shortcut through obstacles (0 = disable, any other value = enable)");
	DefineConstIntCVarName("ai_SmartPathFollower_useAdvancedPathShortcutting_debug", SmartPathFollower_useAdvancedPathShortcutting_debug, 0, VF_NULL, "Show debug lines for when CVar ai_SmartPathFollower_useAdvancedPathShortcutting_debug is enabled");

	DefineConstIntCVarName("ai_DrawPathFollower", DrawPathFollower, 0, VF_CHEAT | VF_CHEAT_NOCHECK, "Enables PathFollower debug drawing displaying agent paths and safe follow target.");

#ifdef CONSOLE_CONST_CVAR_MODE
	DefineConstIntCVarName("ai_LogConsoleVerbosity", LogConsoleVerbosity, 0, VF_CHEAT | VF_CHEAT_NOCHECK, "None = 0, progress/errors/warnings = 1, event = 2, comment = 3");
	DefineConstIntCVarName("ai_LogFileVerbosity", LogFileVerbosity, 0, VF_CHEAT | VF_CHEAT_NOCHECK, "None = 0, progress/errors/warnings = 1, event = 2, comment = 3");
	DefineConstIntCVarName("ai_EnableWarningsErrors", EnableWarningsErrors, 0, VF_CHEAT | VF_CHEAT_NOCHECK, "Enable AI warnings and errors: 1 or 0");
#else
	DefineConstIntCVarName("ai_LogConsoleVerbosity", LogConsoleVerbosity, AI_LOG_OFF, VF_CHEAT | VF_CHEAT_NOCHECK, "None = 0, progress/errors/warnings = 1, event = 2, comment = 3");
	DefineConstIntCVarName("ai_LogFileVerbosity", LogFileVerbosity, AI_LOG_PROGRESS, VF_CHEAT | VF_CHEAT_NOCHECK, "None = 0, progress/errors/warnings = 1, event = 2, comment = 3");
	DefineConstIntCVarName("ai_EnableWarningsErrors", EnableWarningsErrors, 1, VF_CHEAT | VF_CHEAT_NOCHECK, "Enable AI warnings and errors: 1 or 0");
#endif

	REGISTER_CVAR2("ai_OverlayMessageDuration", &OverlayMessageDuration, 5.0f, VF_DUMPTODISK, "How long (seconds) to overlay AI warnings/errors");

	DefineConstIntCVarName("ai_Recorder", Recorder, 0, VF_CHEAT | VF_CHEAT_NOCHECK, "Sets AI Recorder mode. Default is 0 - off.");
	DefineConstIntCVarName("ai_StatsDisplayMode", StatsDisplayMode, 0, VF_CHEAT | VF_CHEAT_NOCHECK,
	                       "Select display mode for the AI stats manager\n"
	                       "Usage: 0 - Hidden, 1 - Show\n");

	DefineConstIntCVarName("ai_VisionMapNumberOfPVSUpdatesPerFrame", VisionMapNumberOfPVSUpdatesPerFrame, 1, VF_CHEAT | VF_CHEAT_NOCHECK, "");
	DefineConstIntCVarName("ai_VisionMapNumberOfVisibilityUpdatesPerFrame", VisionMapNumberOfVisibilityUpdatesPerFrame, 1, VF_CHEAT | VF_CHEAT_NOCHECK, "");

	DefineConstIntCVarName("ai_DebugDrawVisionMap", DebugDrawVisionMap, 0, VF_CHEAT | VF_CHEAT_NOCHECK,
	                       "Toggles the debug drawing of the AI VisionMap.");

	DefineConstIntCVarName("ai_DebugDrawVisionMapStats", DebugDrawVisionMapStats, 1, VF_CHEAT | VF_CHEAT_NOCHECK,
	                       "Enables the debug drawing of the AI VisionMap to show stats information.");

	DefineConstIntCVarName("ai_DebugDrawVisionMapVisibilityChecks", DebugDrawVisionMapVisibilityChecks, 1, VF_CHEAT | VF_CHEAT_NOCHECK,
	                       "Enables the debug drawing of the AI VisionMap to show the status of the visibility checks.");

	DefineConstIntCVarName("ai_DebugDrawVisionMapObservers", DebugDrawVisionMapObservers, 1, VF_CHEAT | VF_CHEAT_NOCHECK,
	                       "Enables the debug drawing of the AI VisionMap to show the location/stats of the observers.\n");

	DefineConstIntCVarName("ai_DebugDrawVisionMapObserversFOV", DebugDrawVisionMapObserversFOV, 0, VF_CHEAT | VF_CHEAT_NOCHECK,
	                       "Enables the debug drawing of the AI VisionMap to draw representations of the observers FOV.\n");

	DefineConstIntCVarName("ai_DebugDrawVisionMapObservables", DebugDrawVisionMapObservables, 1, VF_CHEAT | VF_CHEAT_NOCHECK,
	                       "Enables the debug drawing of the AI VisionMap to show the location/stats of the observables.");

	DefineConstIntCVarName("ai_DrawFireEffectEnabled", DrawFireEffectEnabled, 1, VF_CHEAT | VF_CHEAT_NOCHECK,
	                       "Enabled AI to sweep fire when starting to shoot after a break.");

	REGISTER_CVAR2("ai_DrawFireEffectDecayRange", &DrawFireEffectDecayRange, 30.0f, VF_CHEAT | VF_CHEAT_NOCHECK,
	               "Distance under which the draw fire duration starts decaying linearly.");

	REGISTER_CVAR2("ai_DrawFireEffectMinDistance", &DrawFireEffectMinDistance, 7.5f, VF_CHEAT | VF_CHEAT_NOCHECK,
	               "Distance under which the draw fire will be disabled.");

	REGISTER_CVAR2("ai_DrawFireEffectMinTargetFOV", &DrawFireEffectMinTargetFOV, 7.5f, VF_CHEAT | VF_CHEAT_NOCHECK,
	               "FOV under which the draw fire will be disabled.");

	REGISTER_CVAR2("ai_DrawFireEffectMaxAngle", &DrawFireEffectMaxAngle, 5.0f, VF_CHEAT | VF_CHEAT_NOCHECK,
	               "Maximum angle actors actors are allowed to go away from their aiming direction during draw fire.");

	REGISTER_CVAR2("ai_DrawFireEffectTimeScale", &DrawFireEffectTimeScale, 1.0f, VF_NULL,
	               "Scale for the weapon's draw fire time setting.");

	DefineConstIntCVarName("ai_AllowedToHitPlayer", AllowedToHitPlayer, 1, VF_CHEAT | VF_CHEAT_NOCHECK,
	                       "If turned off, all agents will miss the player all the time.");

	DefineConstIntCVarName("ai_AllowedToHit", AllowedToHit, 1, VF_CHEAT | VF_CHEAT_NOCHECK,
	                       "If turned off, all agents will miss all the time.");

	DefineConstIntCVarName("ai_EnableCoolMisses", EnableCoolMisses, 1, VF_CHEAT | VF_CHEAT_NOCHECK,
	                       "If turned on, when agents miss the player, they will pick cool objects to shoot at.");

	REGISTER_CVAR2("ai_CoolMissesBoxSize", &CoolMissesBoxSize, 10.0f, VF_NULL,
	               "Horizontal size of the box to collect potential cool objects to shoot at.");

	REGISTER_CVAR2("ai_CoolMissesBoxHeight", &CoolMissesBoxHeight, 2.5f, VF_NULL,
	               "Vertical size of the box to collect potential cool objects to shoot at.");

	REGISTER_CVAR2("ai_CoolMissesMinMissDistance", &CoolMissesMinMissDistance, 7.5f, VF_NULL,
	               "Maximum distance to go away from the player.");

	REGISTER_CVAR2("ai_CoolMissesMaxLightweightEntityMass", &CoolMissesMaxLightweightEntityMass, 20.0f, VF_NULL,
	               "Maximum mass of a non-destroyable, non-deformable, non-breakable rigid body entity to be considered.");

	REGISTER_CVAR2("ai_CoolMissesProbability", &CoolMissesProbability, 0.35f, VF_NULL,
	               "Agents' chance to perform a cool miss!");

	REGISTER_CVAR2("ai_CoolMissesCooldown", &CoolMissesCooldown, 0.25f, VF_NULL,
	               "Global time between potential cool misses.");

	DefineConstIntCVarName("ai_DynamicHidespotsEnabled", DynamicHidespotsEnabled, 0, VF_CHEAT | VF_CHEAT_NOCHECK,
	                       "If enabled, dynamic hidespots are considered when evaluating.");

	DefineConstIntCVarName("ai_ForceSerializeAllObjects", ForceSerializeAllObjects, 0, VF_CHEAT | VF_CHEAT_NOCHECK,
	                       "Serialize all AI objects (ignore NO_SAVE flag).");

	REGISTER_CVAR2("ai_LobThrowMinAllowedDistanceFromFriends", &LobThrowMinAllowedDistanceFromFriends, 15.0f, VF_CHEAT | VF_CHEAT_NOCHECK,
	               "Minimum distance a grenade (or any object thrown using a lob) should land from mates to accept the throw trajectory.");
	REGISTER_CVAR2("ai_LobThrowTimePredictionForFriendPositions", &LobThrowTimePredictionForFriendPositions, 2.0f, VF_CHEAT | VF_CHEAT_NOCHECK,
	               "Time frame used to predict the next position of moving mates to score the landing position of the lob throw");
	REGISTER_CVAR2("ai_LobThrowPercentageOfDistanceToTargetUsedForInaccuracySimulation", &LobThrowPercentageOfDistanceToTargetUsedForInaccuracySimulation, 0.0,
	               VF_CHEAT | VF_CHEAT_NOCHECK, "This value identifies percentage of the distance to the target that will be used to simulate human inaccuracy with parabolic throws.");
	DefineConstIntCVarName("ai_LobThrowUseRandomForInaccuracySimulation", LobThrowSimulateRandomInaccuracy, 0, VF_CHEAT | VF_CHEAT_NOCHECK,
	                       "Uses random variation for simulating inaccuracy in the lob throw.");

	REGISTER_CVAR2("ai_LogSignals", &LogSignals, 0, VF_CHEAT | VF_CHEAT_NOCHECK,
	               "Logs all the signals received in CAIActor::NotifySignalReceived.");

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

	REGISTER_COMMAND("ai_debugMNMAgentType", DebugMNMAgentType, VF_NULL,
	                 "Enabled MNM debug draw for an specific agent type.\n"
	                 "Usage: ai_debugMNMAgentType [AgentTypeName]\n");

	REGISTER_COMMAND("ai_MNMCalculateAccessibility", MNMCalculateAccessibility, VF_NULL,
	                 "Calculate mesh reachability starting from designers placed MNM Seeds.\n");

	REGISTER_COMMAND("ai_MNMComputeConnectedIslands", MNMComputeConnectedIslands, VF_DEV_ONLY,
	                 "Computes connected islands on the mnm mesh.\n");

	REGISTER_COMMAND("ai_NavigationReloadConfig", NavigationReloadConfig, VF_DEV_ONLY,
	                 "Usage: ai_NavigationReloadConfig\n"
	                 "Reloads navigation config file. May lead to broken subsystems and weird bugs. For DEBUG purposes ONLY!\n");

	REGISTER_COMMAND("ai_DebugAgent", DebugAgent, VF_NULL,
	                 "Start debugging an agent more in-depth. Pick by name, closest or in center of view.\n"
	                 "Example: ai_DebugAgent closest\n"
	                 "Example: ai_DebugAgent centerview\n"
	                 "Example: ai_DebugAgent name\n"
	                 "Call without parameters to stop the in-depth debugging.\n"
	                 "Example: ai_DebugAgent\n"
	                 );
	REGISTER_CVAR2("ai_ModularBehaviorTree", &ModularBehaviorTree, 1, VF_CHEAT | VF_CHEAT_NOCHECK,
	               "[0-1] Enable/Disable the usage of the modular behavior tree system.");

	REGISTER_CVAR2("ai_DebugTimestamps", &DebugTimestamps, 1, VF_CHEAT | VF_CHEAT_NOCHECK,
	               "[0-1] Enable/Disable the debug text of the modular behavior tree's timestamps.");

	DefineConstIntCVarName("ai_LogModularBehaviorTreeExecutionStacks", LogModularBehaviorTreeExecutionStacks, 0, VF_CHEAT | VF_CHEAT_NOCHECK,
	                       "[0-2] Enable/Disable logging of the execution stacks of modular behavior trees to individual files in the MBT_Logs directory.\n"
	                       "0 - Off\n"
	                       "1 - Log execution stacks of only the currently selected agent\n"
	                       "2 - Log execution stacks of all currently active agents");

	REGISTER_CVAR2("ai_MNMAllowDynamicRegenInEditor", &MNMAllowDynamicRegenInEditor, 0, VF_CHEAT | VF_CHEAT_NOCHECK,
	               "[0-1] Allow dynamic regeneration of MNM when in the editor, when in game mode. Put this CVar in your configuration file rather than changing it during execution.");

}

void AIConsoleVars::CommTest(IConsoleCmdArgs* args)
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

void AIConsoleVars::CommTestStop(IConsoleCmdArgs* args)
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

void AIConsoleVars::CommWriteStats(IConsoleCmdArgs* args)
{
	gAIEnv.pCommunicationManager->WriteStatistics();
}

void AIConsoleVars::CommResetStats(IConsoleCmdArgs* args)
{
	gAIEnv.pCommunicationManager->ResetStatistics();
}

void AIConsoleVars::DebugMNMAgentType(IConsoleCmdArgs* args)
{
	if (args->GetArgCount() < 2)
	{
		AIWarning("<DebugMNMAgentType> Expecting Agent Type as parameter.");
		return;
	}

	const char* agentTypeName = args->GetArg(1);

	NavigationAgentTypeID agentTypeID = gAIEnv.pNavigationSystem->GetAgentTypeID(agentTypeName);
	gAIEnv.pNavigationSystem->SetDebugDisplayAgentType(agentTypeID);
}

void AIConsoleVars::MNMCalculateAccessibility(IConsoleCmdArgs* args)
{
	gAIEnv.pNavigationSystem->CalculateAccessibility();
}

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

void AIConsoleVars::MNMComputeConnectedIslands(IConsoleCmdArgs* args)
{
	gAIEnv.pNavigationSystem->ComputeIslands();
}

void AIConsoleVars::NavigationReloadConfig(IConsoleCmdArgs* args)
{
	if (INavigationSystem* pNavigationSystem = gAIEnv.pNavigationSystem)
	{
		// TODO pavloi 2016.03.09: hack implementation. See comments in ReloadConfig.
		if (pNavigationSystem->ReloadConfig())
		{
			pNavigationSystem->ClearAndNotify();
		}
		else
		{
			AIWarning("Errors during config reloading");
		}
		return;
	}
	AIWarning("Unable to obtain navigation system to reload config.");
}

void AIConsoleVars::DebugAgent(IConsoleCmdArgs* args)
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

void AIConsoleVars::AIBubblesNameFilterCallback(ICVar* pCvar)
{
	if (IAIBubblesSystem* pBubblesSystem = GetAISystem()->GetAIBubblesSystem())
	{
		pBubblesSystem->SetNameFilter(pCvar->GetString());
	}
}
