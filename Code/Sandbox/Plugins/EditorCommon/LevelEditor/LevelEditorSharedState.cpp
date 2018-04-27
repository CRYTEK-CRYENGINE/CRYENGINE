// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include <StdAfx.h>
#include "LevelEditorSharedState.h"
#include "ICommandManager.h"
#include "Commands/QCommandAction.h"

LevelEditorSharedState::LevelEditorSharedState()
	: m_showDisplayInfo(false)
	, m_displayInfoLevel(eDisplayInfoLevel_Low)
{
}

void LevelEditorSharedState::OnEditorNotifyEvent(EEditorNotifyEvent aEventId)
{
	switch (aEventId)
	{
	case eNotify_OnMainFrameInitialized:
		InitActions();
		ResetState();
		break;
	}
}

void LevelEditorSharedState::InitActions()
{
	showDisplayInfoChanged.Connect([this]()  { GetIEditor()->GetICommandManager()->SetChecked("level.toggle_display_info", IsDisplayInfoEnabled()); });
	displayInfoLevelChanged.Connect([this]() { GetIEditor()->GetICommandManager()->SetChecked("level.display_info_low", GetDisplayInfoLevel() == LevelEditorSharedState::eDisplayInfoLevel_Low); });
	displayInfoLevelChanged.Connect([this]() { GetIEditor()->GetICommandManager()->SetChecked("level.display_info_medium", GetDisplayInfoLevel() == LevelEditorSharedState::eDisplayInfoLevel_Med); });
	displayInfoLevelChanged.Connect([this]() { GetIEditor()->GetICommandManager()->SetChecked("level.display_info_high", GetDisplayInfoLevel() == LevelEditorSharedState::eDisplayInfoLevel_High); });
}

void LevelEditorSharedState::ResetState()
{
	//If r_DisplayInfo was found in config file, use it, otherwise load from personalization manager
	const bool fromConfig = (GetIEditor()->GetSystem()->GetIConsole()->GetCVar("r_DisplayInfo")->GetFlags() & VF_WASINCONFIG);
	//If user has r_displayInfo in config, use the value.
	if (fromConfig)
	{
		int val = gEnv->pConsole->GetCVar("r_displayInfo")->GetIVal();
		m_showDisplayInfo = (val != 0);
		switch (val)
		{
		case 1:
			m_displayInfoLevel = eDisplayInfoLevel_Med;
			break;
		case 2:
			m_displayInfoLevel = eDisplayInfoLevel_High;
			break;
		case 3:
			m_displayInfoLevel = eDisplayInfoLevel_Low;
			break;

		default:
			//4 & 5 are not supported
			break;
		}
	}
	else
	{
		//Otherwise use setting from personalization manager
		QVariant showDisplayInfo = GET_PERSONALIZATION_PROPERTY(LevelEditorSharedState, "ShowDisplayInfo");
		if (showDisplayInfo.isValid())
		{
			m_showDisplayInfo = showDisplayInfo.toBool();
		}

		QVariant displayInfoLevel = GET_PERSONALIZATION_PROPERTY(LevelEditorSharedState, "DisplayInfoLevel");
		if (displayInfoLevel.isValid())
		{
			int val = displayInfoLevel.toInt();
			if (val < eDisplayInfoLevel_Low || val > eDisplayInfoLevel_High)
			{
				val = eDisplayInfoLevel_Low;
			}
			m_displayInfoLevel = static_cast<eDisplayInfoLevel>(val);
		}

		SetDisplayInfoCVar();
	}

	showDisplayInfoChanged();
	displayInfoLevelChanged();
}

bool LevelEditorSharedState::IsDisplayInfoEnabled() const
{
	return m_showDisplayInfo;
}

LevelEditorSharedState::eDisplayInfoLevel LevelEditorSharedState::GetDisplayInfoLevel() const
{
	return m_displayInfoLevel;
}

void LevelEditorSharedState::ToggleDisplayInfo()
{
	m_showDisplayInfo = !m_showDisplayInfo;
	SetDisplayInfoCVar();
	showDisplayInfoChanged();
	SaveState();
}

void LevelEditorSharedState::SetDisplayInfoLevel(eDisplayInfoLevel level)
{
	m_displayInfoLevel = level;
	SetDisplayInfoCVar();
	displayInfoLevelChanged();
	SaveState();
}

void LevelEditorSharedState::SetDisplayInfoCVar()
{
	//Translate GUI state to cVar
	int cVar = 0;
	if (m_showDisplayInfo)
	{
		switch (m_displayInfoLevel)
		{
		case eDisplayInfoLevel_Low:
			cVar = 3;
			break;
		case eDisplayInfoLevel_Med:
			cVar = 1;
			break;
		case eDisplayInfoLevel_High:
			cVar = 2;
			break;
		}
	}

	gEnv->pConsole->GetCVar("r_displayInfo")->Set(cVar);
}

void LevelEditorSharedState::SaveState()
{
	SET_PERSONALIZATION_PROPERTY(LevelEditorSharedState, "ShowDisplayInfo", m_showDisplayInfo);
	SET_PERSONALIZATION_PROPERTY(LevelEditorSharedState, "DisplayInfoLevel", m_displayInfoLevel);
}
