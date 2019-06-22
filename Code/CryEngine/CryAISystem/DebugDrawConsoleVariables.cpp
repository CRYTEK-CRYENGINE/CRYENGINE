// Copyright 2001-2019 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "DebugDrawConsoleVariables.h"

void SAIConsoleVarsLegacyDebugDraw::Init()
{
	DefineConstIntCVarName("ai_DebugDrawCoolMisses", DebugDrawCoolMisses, 0, VF_CHEAT | VF_CHEAT_NOCHECK,
		"Toggles displaying the cool miss locations around the player.\n"
		"Usage: ai_DebugDrawCoolMisses [0/1]");

	DefineConstIntCVarName("ai_DebugDrawFireCommand", DebugDrawFireCommand, 0, VF_CHEAT | VF_CHEAT_NOCHECK,
		"Toggles displaying the fire command targets and modifications.\n"
		"Usage: ai_DebugDrawFireCommand [0/1]");

	DefineConstIntCVarName("ai_DrawGoals", DrawGoals, 0, VF_CHEAT | VF_CHEAT_NOCHECK,
		"Draws all the active goal ops debug info.\n"
		"Usage: ai_DrawGoals [0/1]\n"
		"Default is 0 (off). Set to 1 to draw the AI goal op debug info.");

	DefineConstIntCVarName("ai_DrawStats", DrawStats, 0, VF_CHEAT | VF_CHEAT_NOCHECK,
		"Toggles drawing stats (in a table on top left of screen) for AI objects within specified range.\n"
		"Will display attention target, goal pipe and current goal.");

	DefineConstIntCVarName("ai_DrawAttentionTargetPositions", DrawAttentionTargetsPosition, 0, VF_CHEAT | VF_CHEAT_NOCHECK,
		"Displays markers for the AI's current attention target position. ");

	DefineConstIntCVarName("ai_DrawTrajectory", DrawTrajectory, 0, VF_CHEAT | VF_CHEAT_NOCHECK,
		"Record and draw the actual path taken by the agent specified in ai_StatsTarget.\n"
		"Path is displayed in aqua, and only a certain length will be displayed, after which\n"
		"old path gradually disappears as new path is drawn.\n"
		"0=do not record, 1=record.");

	DefineConstIntCVarName("ai_DrawRadar", DrawRadar, 0, VF_CHEAT | VF_CHEAT_NOCHECK,
		"Draws AI radar: 0=no radar, >0 = size of the radar on screen");

	DefineConstIntCVarName("ai_DrawRadarDist", DrawRadarDist, 20, VF_CHEAT | VF_CHEAT_NOCHECK,
		"AI radar draw distance in meters, default=20m.");

	DefineConstIntCVarName("ai_DrawProbableTarget", DrawProbableTarget, 0, VF_CHEAT | VF_CHEAT_NOCHECK,
		"Enables/Disables drawing the position of probable target.");

	DefineConstIntCVarName("ai_DebugDrawDamageParts", DebugDrawDamageParts, 0, VF_CHEAT | VF_CHEAT_NOCHECK,
		"Draws the damage parts of puppets and vehicles.");

	DefineConstIntCVarName("ai_DebugDrawStanceSize", DebugDrawStanceSize, 0, VF_CHEAT | VF_CHEAT_NOCHECK,
		"Draws the game logic representation of the stance size of the AI agents.");

	DefineConstIntCVarName("ai_DrawUpdate", DebugDrawUpdate, 0, VF_CHEAT | VF_CHEAT_NOCHECK,
		"list of AI forceUpdated entities");

	DefineConstIntCVarName("ai_DebugDrawEnabledActors", DebugDrawEnabledActors, 0, VF_CHEAT | VF_CHEAT_NOCHECK,
		"list of AI Actors that are enabled and metadata");

	DefineConstIntCVarName("ai_DebugDrawEnabledPlayers", DebugDrawEnabledPlayers, 0, VF_CHEAT | VF_CHEAT_NOCHECK,
		"list of AI players that are enabled and metadata");

	DefineConstIntCVarName("ai_DebugDrawLightLevel", DebugDrawLightLevel, 0, VF_CHEAT | VF_CHEAT_NOCHECK,
		"Debug AI light level manager");

	DefineConstIntCVarName("ai_DebugDrawPhysicsAccess", DebugDrawPhysicsAccess, 0, VF_CHEAT | VF_CHEAT_NOCHECK,
		"Displays current physics access statistics for the AI module.");

	DefineConstIntCVarName("ai_DebugTargetSilhouette", DebugTargetSilhouette, 0, VF_CHEAT | VF_CHEAT_NOCHECK,
		"Draws the silhouette used for missing the target while shooting.");

	DefineConstIntCVarName("ai_DebugDrawDamageControl", DebugDrawDamageControl, 0, VF_CHEAT | VF_CHEAT_NOCHECK,
		"Debugs the damage control system 0=disabled, 1=collect, 2=collect&draw.");

	DefineConstIntCVarName("ai_DebugDrawExpensiveAccessoryQuota", DebugDrawExpensiveAccessoryQuota, 0, VF_CHEAT | VF_CHEAT_NOCHECK,
		"Displays expensive accessory usage quota on puppets.");

	DefineConstIntCVarName("ai_DrawFakeDamageInd", DrawFakeDamageInd, 0, VF_CHEAT | VF_CHEAT_NOCHECK,
		"Draws fake damage indicators on the player.");

	DefineConstIntCVarName("ai_DebugDrawAdaptiveUrgency", DebugDrawAdaptiveUrgency, 0, VF_CHEAT | VF_CHEAT_NOCHECK,
		"Enables drawing the adaptive movement urgency.");

	DefineConstIntCVarName("ai_DebugDrawPlayerActions", DebugDrawPlayerActions, 0, VF_CHEAT | VF_CHEAT_NOCHECK,
		"Debug draw special player actions.");

	DefineConstIntCVarName("ai_DrawType", DrawType, -1, VF_CHEAT | VF_CHEAT_NOCHECK,
		"Display all AI object of specified type. If object is enabled it will be displayed.\n"
		"with blue ball, otherwise with red ball. Yellow line will represent forward direction of the object.\n"
		" <0 - off\n"
		" 0 - display all the dummy objects\n"
		" >0 - type of AI objects to display");

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

	REGISTER_CVAR2("ai_DrawSelectedTargets", &DrawSelectedTargets, 0, VF_CHEAT | VF_CHEAT_NOCHECK,
		"[0-1] Enable/Disable the debug helpers showing the AI's selected targets.");
}