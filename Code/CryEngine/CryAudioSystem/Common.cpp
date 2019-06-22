// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "stdafx.h"
#include "Common.h"
#include "System.h"
#include "Object.h"
#include "Listener.h"
#include "LoseFocusTrigger.h"
#include "GetFocusTrigger.h"
#include "MuteAllTrigger.h"
#include "UnmuteAllTrigger.h"
#include "PauseAllTrigger.h"
#include "ResumeAllTrigger.h"

#if defined(CRY_AUDIO_USE_DEBUG_CODE)
	#include "PreviewTrigger.h"
#endif // CRY_AUDIO_USE_DEBUG_CODE

namespace CryAudio
{
Impl::IImpl* g_pIImpl = nullptr;
Impl::IObject* g_pIObject = nullptr;
CSystem g_system;
ESystemStates g_systemStates = ESystemStates::None;
TriggerLookup g_triggerLookup;
ParameterLookup g_parameterLookup;
SwitchLookup g_switchLookup;
PreloadRequestLookup g_preloadRequests;
EnvironmentLookup g_environmentLookup;
SettingLookup g_settingLookup;
TriggerInstances g_triggerInstances;
TriggerInstanceIdLookup g_triggerInstanceIdLookup;
ContextLookup g_contextLookup;
TriggerInstanceIds g_triggerInstanceIds;

CLoseFocusTrigger g_loseFocusTrigger;
CGetFocusTrigger g_getFocusTrigger;
CMuteAllTrigger g_muteAllTrigger;
CUnmuteAllTrigger g_unmuteAllTrigger;
CPauseAllTrigger g_pauseAllTrigger;
CResumeAllTrigger g_resumeAllTrigger;
Objects g_activeObjects;

SImplInfo g_implInfo;
CryFixedStringT<MaxFilePathLength> g_configPath = "";

TriggerInstanceId g_triggerInstanceIdCounter = 1;

SPoolSizes g_poolSizes;

#if defined(CRY_AUDIO_USE_DEBUG_CODE)
CListener g_defaultListener(DefaultListenerId, false, g_szDefaultListenerName);
CListener g_previewListener(g_previewListenerId, false, g_szPreviewListenerName);
Objects g_constructedObjects;
CObject g_previewObject(CTransformation::GetEmptyObject(), "Preview Object");
CPreviewTrigger g_previewTrigger;
SPoolSizes g_debugPoolSizes;
ContextInfo g_contextInfo;
ContextDebugInfo g_contextDebugInfo;
ParameterValues g_parameters;
ParameterValues g_parametersGlobally;
SwitchStateIds g_switchStates;
SwitchStateIds g_switchStatesGlobally;
#else
CListener g_defaultListener(DefaultListenerId, false);
#endif // CRY_AUDIO_USE_DEBUG_CODE
}      // namespace CryAudio
