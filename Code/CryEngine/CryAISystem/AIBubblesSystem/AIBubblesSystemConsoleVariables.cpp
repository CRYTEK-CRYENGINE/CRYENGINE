// Copyright 2001-2019 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"

#include "AIBubblesSystemConsoleVariables.h"

#include <CryAISystem/IAIBubblesSystem.h>

void SAIConsoleVarsLegacyBubblesSystem::Init()
{
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

	REGISTER_CVAR2("ai_BubblesSystem", &EnableBubblesSystem, 1, VF_CHEAT | VF_CHEAT_NOCHECK,
		"Enables/disables bubble notifier.");

	REGISTER_CVAR2("ai_BubblesSystemDecayTime", &BubblesSystemDecayTime, 15.0f, VF_CHEAT | VF_CHEAT_NOCHECK,
		"Specifies the decay time for the bubbles drawn on screen.");
}

void SAIConsoleVarsLegacyBubblesSystem::AIBubblesNameFilterCallback(ICVar* pCvar)
{
	if (IAIBubblesSystem* pBubblesSystem = GetAISystem()->GetAIBubblesSystem())
	{
		pBubblesSystem->SetNameFilter(pCvar->GetString());
	}
}