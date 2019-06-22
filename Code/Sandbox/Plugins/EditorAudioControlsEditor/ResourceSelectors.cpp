// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"

#include "AssetsManager.h"
#include "ResourceSelectorDialog.h"
#include "ListenerSelectorDialog.h"

#include <IResourceSelectorHost.h>
#include <ListSelectionDialog.h>
#include <IEditor.h>

namespace ACE
{
SResourceSelectionResult ShowSelectDialog(
	SResourceSelectorContext const& context,
	char const* szPreviousValue,
	EAssetType controlType,
	bool const combinedAssets)
{
	CResourceSelectorDialog dialog(controlType, combinedAssets, context.parentWidget);
	CResourceSelectorDialog::SResourceSelectionDialogResult dialogResult = dialog.ChooseItem(szPreviousValue);
	SResourceSelectionResult result{ dialogResult.selectionAccepted, dialogResult.selectedItem.c_str() };
	return result;
}

SResourceSelectionResult AudioTriggerSelector(SResourceSelectorContext const& context, char const* szPreviousValue)
{
	return ShowSelectDialog(context, szPreviousValue, EAssetType::Trigger, false);
}

SResourceSelectionResult AudioSwitchSelector(SResourceSelectorContext const& context, char const* szPreviousValue) // Deprecated.
{
	return ShowSelectDialog(context, szPreviousValue, EAssetType::Switch, false);
}

SResourceSelectionResult AudioStateSelector(SResourceSelectorContext const& context, char const* szPreviousValue) // Deprecated.
{
	return ShowSelectDialog(context, szPreviousValue, EAssetType::State, false);
}

SResourceSelectionResult AudioSwitchStateSelector(SResourceSelectorContext const& context, char const* szPreviousValue)
{
	return ShowSelectDialog(context, szPreviousValue, EAssetType::State, true);
}

SResourceSelectionResult AudioParameterSelector(SResourceSelectorContext const& context, char const* szPreviousValue)
{
	return ShowSelectDialog(context, szPreviousValue, EAssetType::Parameter, false);
}

SResourceSelectionResult AudioEnvironmentSelector(SResourceSelectorContext const& context, char const* szPreviousValue)
{
	return ShowSelectDialog(context, szPreviousValue, EAssetType::Environment, false);
}

SResourceSelectionResult AudioPreloadRequestSelector(SResourceSelectorContext const& context, char const* szPreviousValue)
{
	return ShowSelectDialog(context, szPreviousValue, EAssetType::Preload, false);
}

SResourceSelectionResult AudioSettingSelector(SResourceSelectorContext const& context, char const* szPreviousValue)
{
	return ShowSelectDialog(context, szPreviousValue, EAssetType::Setting, false);
}

SResourceSelectionResult AudioListenerSelector(SResourceSelectorContext const& context, char const* szPreviousValue)
{
	CListenerSelectorDialog dialog(context.parentWidget);
	CListenerSelectorDialog::SResourceSelectionDialogResult dialogResult = dialog.ChooseListener(szPreviousValue);
	SResourceSelectionResult result{ dialogResult.selectionAccepted, dialogResult.selectedListener.c_str() };
	return result;
}

REGISTER_RESOURCE_SELECTOR("AudioTrigger", AudioTriggerSelector, "")
REGISTER_RESOURCE_SELECTOR("AudioSwitch", AudioSwitchSelector, "") // Deprecated.
REGISTER_RESOURCE_SELECTOR("AudioState", AudioStateSelector, "")   // Deprecated.
REGISTER_RESOURCE_SELECTOR("AudioSwitchState", AudioSwitchStateSelector, "")
REGISTER_RESOURCE_SELECTOR("AudioParameter", AudioParameterSelector, "")
REGISTER_RESOURCE_SELECTOR("AudioEnvironment", AudioEnvironmentSelector, "")
REGISTER_RESOURCE_SELECTOR("AudioPreloadRequest", AudioPreloadRequestSelector, "")
REGISTER_RESOURCE_SELECTOR("AudioSetting", AudioSettingSelector, "")
REGISTER_RESOURCE_SELECTOR("AudioListener", AudioListenerSelector, "")
} // namespace ACE
