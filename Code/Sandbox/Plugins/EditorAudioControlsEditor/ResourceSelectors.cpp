// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"

#include "AudioControlsEditorPlugin.h"
#include "ResourceSelectorDialog.h"

#include <IResourceSelectorHost.h>
#include <ListSelectionDialog.h>
#include <CryGame/IGameFramework.h>
#include <IEditor.h>

namespace ACE
{
dll_string ShowSelectDialog(SResourceSelectorContext const& context, char const* szPreviousValue, EAssetType const controlType)
{
	char* szLevelName;
	gEnv->pGameFramework->GetEditorLevel(&szLevelName, nullptr);
	CResourceSelectorDialog dialog(controlType, g_assetsManager.GetScope(szLevelName), context.parentWidget);
	dialog.setModal(true);

	return dialog.ChooseItem(szPreviousValue);
}

dll_string AudioTriggerSelector(SResourceSelectorContext const& context, char const* const szPreviousValue)
{
	return ShowSelectDialog(context, szPreviousValue, EAssetType::Trigger);
}

dll_string AudioSwitchSelector(SResourceSelectorContext const& context, char const* const szPreviousValue)
{
	return ShowSelectDialog(context, szPreviousValue, EAssetType::Switch);
}

dll_string AudioSwitchStateSelector(SResourceSelectorContext const& context, char const* const szPreviousValue)
{
	return ShowSelectDialog(context, szPreviousValue, EAssetType::State);
}

dll_string AudioParameterSelector(SResourceSelectorContext const& context, char const* const szPreviousValue)
{
	return ShowSelectDialog(context, szPreviousValue, EAssetType::Parameter);
}

dll_string AudioEnvironmentSelector(SResourceSelectorContext const& context, char const* const szPreviousValue)
{
	return ShowSelectDialog(context, szPreviousValue, EAssetType::Environment);
}

dll_string AudioPreloadRequestSelector(SResourceSelectorContext const& context, char const* const szPreviousValue)
{
	return ShowSelectDialog(context, szPreviousValue, EAssetType::Preload);
}

REGISTER_RESOURCE_SELECTOR("AudioTrigger", AudioTriggerSelector, "")
REGISTER_RESOURCE_SELECTOR("AudioSwitch", AudioSwitchSelector, "")
REGISTER_RESOURCE_SELECTOR("AudioSwitchState", AudioSwitchStateSelector, "")
REGISTER_RESOURCE_SELECTOR("AudioRTPC", AudioParameterSelector, "")
REGISTER_RESOURCE_SELECTOR("AudioEnvironment", AudioEnvironmentSelector, "")
REGISTER_RESOURCE_SELECTOR("AudioPreloadRequest", AudioPreloadRequestSelector, "")
} // namespace ACE

