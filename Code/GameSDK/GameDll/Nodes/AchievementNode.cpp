// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

/*************************************************************************
   -------------------------------------------------------------------------
   History:
   - 12:05:2010   Created by Steve Humphreys
*************************************************************************/

#include "StdAfx.h"
#include "Network/Lobby/GameAchievements.h"
#include "Network/Lobby/GameLobbyData.h"
#include "PersistantStats.h"
#include "UI/ProfileOptions.h"
#include "UI/HUD/HUDUtils.h"
#include "UI/HUD/HUDEventWrapper.h"

#include <CryFlowGraph/IFlowBaseNode.h>
#include <ILevelSystem.h>

class CAchievementNode : public CFlowBaseNode<eNCT_Singleton>
{
	enum INPUTS
	{
		EIP_Trigger = 0,
		EIP_Achievement,
		EIP_LevelComplete,
	};

public:
	CAchievementNode(SActivationInfo* pActInfo)
	{
	}

	virtual void GetMemoryUsage(ICrySizer* s) const
	{
		s->Add(*this);
	}

	virtual void GetConfiguration(SFlowNodeConfig& config)
	{
		static const SInputPortConfig in_config[] = {
			InputPortConfig_Void("Trigger",                                                                                                                                                                                                                                               _HELP("Trigger to give the achievement")),
			InputPortConfig<int>("Achievement",                                                                                                                                                                                                                                           0,                                             _HELP("The achievement to trigger"),_HELP("Achievement"),
			                     _UICONFIG("enum_string:Tutorial=0,JailBreak_WelcomeToTheJungle=1,Fields=2,Canyon=3,Swamp=4,River=5,Island=6,Cave=7,Fields_CanYouHearMe=24,JailBreak_WhoNeedsRockets=25,Canyon_WhiteRider=26,River_RoadKiller=27,Islands_PingPong=28,TheImproviser=32")),
			InputPortConfig_Void("LevelComplete",                                                                                                                                                                                                                                         _HELP("Trigger this at the end of each level"),_HELP("LevelComplete")),
			{ 0 }
		};
		static const SOutputPortConfig out_config[] = {
			{ 0 }
		};
		config.sDescription = _HELP("Triggers an achievement/trophy for the local player");
		config.pInputPorts = in_config;
		config.pOutputPorts = out_config;
		config.SetCategory(EFLN_APPROVED);
	}

	virtual void ProcessEvent(EFlowEvent event, SActivationInfo* pActInfo)
	{
		switch (event)
		{
		case eFE_Activate:
			{
				if (IsPortActive(pActInfo, EIP_LevelComplete))
				{
					// Send the level name to the persistant stats system.
					ILevelSystem* pLS = g_pGame->GetIGameFramework()->GetILevelSystem();
					ILevelInfo* pLevelInfo = pLS ? pLS->GetCurrentLevel() : NULL;
					if (pLevelInfo)
					{
						stack_string levelName = pLevelInfo->GetName();
						levelName.MakeLower();
						if (AccountLevelForStats(levelName.c_str()))
						{
							g_pGame->GetPersistantStats()->OnSPLevelComplete(levelName.c_str());
						}
					}
				}

				if (IsPortActive(pActInfo, EIP_Trigger))
				{
					int achievementId = -1;
					int input = GetPortInt(pActInfo, EIP_Achievement);

					/*	COMPILE_TIME_ASSERT( 0 == eC3A_Tutorial );
					   COMPILE_TIME_ASSERT( 7 == eC3A_Cave );

					   COMPILE_TIME_ASSERT( 23 == eC3A_SPLevel_CanYouHearMe );
					   COMPILE_TIME_ASSERT( 24 == eC3A_SPLevel_Jailbreak_Rockets );
					   COMPILE_TIME_ASSERT( 25 == eC3A_SPLevel_Donut_Surf );
					   COMPILE_TIME_ASSERT( 26 == eC3A_SPLevel_Roadkill );
					   COMPILE_TIME_ASSERT( 30 == eC3A_SPSkill_Improviser );*/

					if ((input >= 0 && input <= 7) || (input >= 24 && input <= 28) || (input == eC3A_SPSkill_Improviser))
					{
						achievementId = input;
					}
					else
					{
						assert(false);
					}

					if (achievementId != -1)
					{
						g_pGame->GetGameAchievements()->GiveAchievement((ECrysis3Achievement)achievementId);
					}
				}
				break;
			}
		}
	}

private:

	bool AccountLevelForStats(const char* levelName) const
	{
		return (strcmp(levelName, "tutorial") != 0);
	}
};
//////////////////////////////////////////////////////////////////////////

class CTutorialPlayedSPNode : public CFlowBaseNode<eNCT_Singleton>
{
public:
	enum INPUTS
	{
		EIP_FlagTutorialAsPlayed = 0,
		EIP_FlagTutorialAsNOTPlayed,
		EIP_GetFlagState
	};

	enum OUTPUTS
	{
		OUT_HasBeenPlayed = 0
	};

	CTutorialPlayedSPNode(SActivationInfo* pActInfo)
	{
	}

	virtual void GetMemoryUsage(ICrySizer* s) const
	{
		s->Add(*this);
	}

	virtual void GetConfiguration(SFlowNodeConfig& config)
	{
		static const SInputPortConfig in_config[] =
		{
			InputPortConfig_Void("FlagTutorialAsPlayed",    _HELP("Trigger to flag the tutorial as already played")),
			InputPortConfig_Void("FlagTutorialAsNOTPlayed", _HELP("Trigger to flag the tutorial as not played (intended as development tool only)")),
			InputPortConfig_Void("GetFlagState",            _HELP("Trigger to get the flag state (played/notplayed)")),
			{ 0 }
		};
		static const SOutputPortConfig out_config[] =
		{
			OutputPortConfig<bool>("HasBeenPlayed", "true = already played, false = not yet played"),
			{ 0 }
		};
		config.sDescription = _HELP("Manages the player profile tutorial flag (SINGLE PLAYER)");
		config.pInputPorts = in_config;
		config.pOutputPorts = out_config;
		config.SetCategory(EFLN_APPROVED);
	}

	virtual void ProcessEvent(EFlowEvent event, SActivationInfo* pActInfo)
	{
		CPersistantStats* pStats = g_pGame->GetPersistantStats();
		CRY_ASSERT(pStats != NULL);
		if (!pStats)
			return;

		switch (event)
		{
		case eFE_Activate:
			{
				bool saveProfile = false;
				if (IsPortActive(pActInfo, EIP_FlagTutorialAsPlayed))
				{
					int val = pStats->GetStat(EIPS_TutorialAlreadyPlayed);
					pStats->SetClientStat(EIPS_TutorialAlreadyPlayed, val + 1);
					saveProfile = true;
				}
				if (IsPortActive(pActInfo, EIP_FlagTutorialAsNOTPlayed))
				{
					pStats->SetClientStat(EIPS_TutorialAlreadyPlayed, 0);
					saveProfile = true;
				}
				if (IsPortActive(pActInfo, EIP_GetFlagState))
				{
					int val = pStats->GetStat(EIPS_TutorialAlreadyPlayed);
					ActivateOutput(pActInfo, OUT_HasBeenPlayed, val != 0);
				}
				if (saveProfile)
					g_pGame->GetProfileOptions()->SaveProfile(ePR_All);
			}
			break;
		}
	}
};

REGISTER_FLOW_NODE("Game:GiveAchievement", CAchievementNode);
REGISTER_FLOW_NODE("Game:TutorialPlayedSP", CTutorialPlayedSPNode);
