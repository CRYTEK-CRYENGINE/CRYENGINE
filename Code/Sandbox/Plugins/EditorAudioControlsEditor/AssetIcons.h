// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include <SharedData.h>
#include <CryIcon.h>

namespace ACE
{
static CryIcon s_errorIcon;
static CryIcon s_environmentIcon;
static CryIcon s_folderIcon;
static CryIcon s_libraryIcon;
static CryIcon s_parameterIcon;
static CryIcon s_preloadIcon;
static CryIcon s_stateIcon;
static CryIcon s_switchIcon;
static CryIcon s_triggerIcon;

//////////////////////////////////////////////////////////////////////////
inline void InitAssetIcons()
{
	s_errorIcon = CryIcon("icons:Dialogs/dialog-error.ico");
	s_environmentIcon = CryIcon("icons:audio/assets/environment.ico");
	s_folderIcon = CryIcon("icons:General/Folder.ico");
	s_libraryIcon = CryIcon("icons:General/File.ico");
	s_parameterIcon = CryIcon("icons:audio/assets/parameter.ico");
	s_preloadIcon = CryIcon("icons:audio/assets/preload.ico");
	s_stateIcon = CryIcon("icons:audio/assets/state.ico");
	s_switchIcon = CryIcon("icons:audio/assets/switch.ico");
	s_triggerIcon = CryIcon("icons:audio/assets/trigger.ico");
}

//////////////////////////////////////////////////////////////////////////
inline CryIcon const& GetAssetIcon(EAssetType const type)
{
	switch (type)
	{
	case EAssetType::Trigger:
		return s_triggerIcon;
		break;
	case EAssetType::Parameter:
		return s_parameterIcon;
		break;
	case EAssetType::Switch:
		return s_switchIcon;
		break;
	case EAssetType::State:
		return s_stateIcon;
		break;
	case EAssetType::Environment:
		return s_environmentIcon;
		break;
	case EAssetType::Preload:
		return s_preloadIcon;
		break;
	case EAssetType::Folder:
		return s_folderIcon;
		break;
	case EAssetType::Library:
		return s_libraryIcon;
		break;
	default:
		return s_errorIcon;
		break;
	}
}
} // namespace ACE