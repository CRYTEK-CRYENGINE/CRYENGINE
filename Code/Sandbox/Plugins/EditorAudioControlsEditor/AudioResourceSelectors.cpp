// Copyright 2001-2016 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "IResourceSelectorHost.h"
#include "QParentWndWidget.h"
#include "ListSelectionDialog.h"
#include "QAudioControlEditorIcons.h"
#include "GameEngine.h"
#include "AudioControlsEditorPlugin.h"
#include "ATLControlsResourceDialog.h"
#include <CryGame/IGameFramework.h>
#include "IEditor.h"

using namespace ACE;

namespace
{
dll_string ShowSelectDialog(const SResourceSelectorContext& context, const char* szPreviousValue, const EACEControlType controlType)
{
	CATLControlsModel* pModel = CAudioControlsEditorPlugin::GetATLModel();
	assert(pModel);

	QParentWndWidget parent(context.parentWindow);
	parent.center();
	parent.setWindowModality(Qt::ApplicationModal);

	ATLControlsDialog dialog(&parent, controlType);

	char* szLevelName;
	GetIEditor()->GetGame()->GetIGameFramework()->GetEditorLevel(&szLevelName, nullptr);
	dialog.SetScope(pModel->GetScope(szLevelName));
	return dialog.ChooseItem(szPreviousValue);
}

dll_string AudioTriggerSelector(const SResourceSelectorContext& context, const char* szPreviousValue)
{
	return ShowSelectDialog(context, szPreviousValue, eACEControlType_Trigger);
}

dll_string AudioSwitchSelector(const SResourceSelectorContext& context, const char* szPreviousValue)
{
	return ShowSelectDialog(context, szPreviousValue, eACEControlType_Switch);
}

dll_string AudioSwitchStateSelector(const SResourceSelectorContext& context, const char* szPreviousValue)
{
	return ShowSelectDialog(context, szPreviousValue, eACEControlType_State);
}

dll_string AudioRTPCSelector(const SResourceSelectorContext& context, const char* szPreviousValue)
{
	return ShowSelectDialog(context, szPreviousValue, eACEControlType_RTPC);
}

dll_string AudioEnvironmentSelector(const SResourceSelectorContext& context, const char* szPreviousValue)
{
	return ShowSelectDialog(context, szPreviousValue, eACEControlType_Environment);
}

dll_string AudioPreloadRequestSelector(const SResourceSelectorContext& context, const char* szPreviousValue)
{
	return ShowSelectDialog(context, szPreviousValue, eACEControlType_Preload);
}

REGISTER_RESOURCE_SELECTOR("AudioTrigger", AudioTriggerSelector, "")
REGISTER_RESOURCE_SELECTOR("AudioSwitch", AudioSwitchSelector, "")
REGISTER_RESOURCE_SELECTOR("AudioSwitchState", AudioSwitchStateSelector, "")
REGISTER_RESOURCE_SELECTOR("AudioRTPC", AudioRTPCSelector, "")
REGISTER_RESOURCE_SELECTOR("AudioEnvironment", AudioEnvironmentSelector, "")
REGISTER_RESOURCE_SELECTOR("AudioPreloadRequest", AudioPreloadRequestSelector, "")
}
