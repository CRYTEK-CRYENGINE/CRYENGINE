// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include <SharedData.h>
#include <CryIcon.h>

namespace ACE
{
inline CryIcon GetAssetIcon(EAssetType const type)
{
	CryIcon icon = CryIcon("icons:Dialogs/dialog-error.ico");

	switch (type)
	{
	case EAssetType::Trigger:
		icon = CryIcon("icons:audio/system/trigger.ico");
		break;
	case EAssetType::Parameter:
		icon = CryIcon("icons:audio/system/parameter.ico");
		break;
	case EAssetType::Switch:
		icon = CryIcon("icons:audio/system/switch.ico");
		break;
	case EAssetType::State:
		icon = CryIcon("icons:audio/system/state.ico");
		break;
	case EAssetType::Environment:
		icon = CryIcon("icons:audio/system/environment.ico");
		break;
	case EAssetType::Preload:
		icon = CryIcon("icons:audio/system/preload.ico");
		break;
	case EAssetType::Folder:
		icon = CryIcon("icons:General/Folder.ico");
		break;
	case EAssetType::Library:
		icon = CryIcon("icons:General/File.ico");
		break;
	default:
		icon = CryIcon("icons:Dialogs/dialog-error.ico");
		break;
	}

	return icon;
}
} // namespace ACE

