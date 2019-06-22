// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include "Common/SharedData.h"
#include <CryIcon.h>

namespace ACE
{
static CryIcon s_errorIcon;
static CryIcon s_environmentIcon;
static CryIcon s_folderIcon;
static CryIcon s_libraryIcon;
static CryIcon s_parameterIcon;
static CryIcon s_preloadIcon;
static CryIcon s_settingIcon;
static CryIcon s_stateIcon;
static CryIcon s_switchIcon;
static CryIcon s_triggerIcon;
static CryIcon s_listenerIcon;

//////////////////////////////////////////////////////////////////////////
inline void InitAssetIcons()
{
	s_errorIcon = CryIcon("icons:Dialogs/dialog-error.ico");
	s_environmentIcon = CryIcon("icons:audio/assets/environment.ico");
	s_folderIcon = CryIcon("icons:General/Folder.ico");
	s_libraryIcon = CryIcon("icons:General/File.ico");
	s_parameterIcon = CryIcon("icons:audio/assets/parameter.ico");
	s_preloadIcon = CryIcon("icons:audio/assets/preload.ico");
	s_settingIcon = CryIcon("icons:audio/assets/setting.ico");
	s_stateIcon = CryIcon("icons:audio/assets/state.ico");
	s_switchIcon = CryIcon("icons:audio/assets/switch.ico");
	s_triggerIcon = CryIcon("icons:audio/assets/trigger.ico");
	s_listenerIcon = CryIcon("icons:audio/assets/listener.ico");
}

//////////////////////////////////////////////////////////////////////////
inline CryIcon const& GetAssetIcon(EAssetType const type)
{
	switch (type)
	{
	case EAssetType::Trigger:
		{
			return s_triggerIcon;
		}
	case EAssetType::Parameter:
		{
			return s_parameterIcon;
		}
	case EAssetType::Switch:
		{
			return s_switchIcon;
		}
	case EAssetType::State:
		{
			return s_stateIcon;
		}
	case EAssetType::Environment:
		{
			return s_environmentIcon;
		}
	case EAssetType::Preload:
		{
			return s_preloadIcon;
		}
	case EAssetType::Setting:
		{
			return s_settingIcon;
		}
	case EAssetType::Folder:
		{
			return s_folderIcon;
		}
	case EAssetType::Library:
		{
			return s_libraryIcon;
		}
	default:
		{
			return s_errorIcon;
		}
	}
}
} // namespace ACE