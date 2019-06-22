// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include <CryAudio/IAudioInterfacesCommonData.h>
#include <map>

namespace CryAudio
{
constexpr ControlId g_loseFocusTriggerId = StringToId(g_szLoseFocusTriggerName);
constexpr ControlId g_getFocusTriggerId = StringToId(g_szGetFocusTriggerName);
constexpr ControlId g_muteAllTriggerId = StringToId(g_szMuteAllTriggerName);
constexpr ControlId g_unmuteAllTriggerId = StringToId(g_szUnmuteAllTriggerName);
constexpr ControlId g_pauseAllTriggerId = StringToId(g_szPauseAllTriggerName);
constexpr ControlId g_resumeAllTriggerId = StringToId(g_szResumeAllTriggerName);

namespace Impl
{
struct IImpl;
struct IObject;
struct IEnvironmentConnection;
struct IParameterConnection;
struct ISettingConnection;
struct ISwitchStateConnection;
struct ITriggerConnection;
} // namespace Impl

class CListener;
class CSystem;
class CObject;
class CLoseFocusTrigger;
class CGetFocusTrigger;
class CMuteAllTrigger;
class CUnmuteAllTrigger;
class CPauseAllTrigger;
class CResumeAllTrigger;
class CTrigger;
class CParameter;
class CSwitch;
class CPreloadRequest;
class CEnvironment;
class CSetting;
class CTriggerInstance;

enum class ESystemStates : EnumFlagsType
{
	None             = 0,
	ImplShuttingDown = BIT(0),
	IsMuted          = BIT(1),
#if defined(CRY_AUDIO_USE_DEBUG_CODE)
	IsPaused         = BIT(2),
	PoolsAllocated   = BIT(3),
#endif  // CRY_AUDIO_USE_DEBUG_CODE
};
CRY_CREATE_ENUM_FLAG_OPERATORS(ESystemStates);

using TriggerLookup = std::map<ControlId, CTrigger const*>;
using ParameterLookup = std::map<ControlId, CParameter const*>;
using SwitchLookup = std::map<ControlId, CSwitch const*>;
using PreloadRequestLookup = std::map<PreloadRequestId, CPreloadRequest*>;
using EnvironmentLookup = std::map<EnvironmentId, CEnvironment const*>;
using SettingLookup = std::map<ControlId, CSetting const*>;
using TriggerInstances = std::map<TriggerInstanceId, CTriggerInstance*>;
using TriggerInstanceIdLookup = std::map<TriggerInstanceId, CObject*>;
using ContextLookup = std::map<ContextId, CryFixedStringT<MaxFileNameLength>>;

using TriggerInstanceIds = std::vector<TriggerInstanceId>;
using TriggerConnections = std::vector<Impl::ITriggerConnection*>;
using ParameterConnections = std::vector<Impl::IParameterConnection*>;
using SwitchStateConnections = std::vector<Impl::ISwitchStateConnection*>;
using EnvironmentConnections = std::vector<Impl::IEnvironmentConnection*>;
using SettingConnections = std::vector<Impl::ISettingConnection*>;
using Objects = std::vector<CObject*>;
using Listeners = std::vector<CListener*>;

extern Impl::IImpl* g_pIImpl;
extern CListener g_defaultListener;
extern Impl::IObject* g_pIObject;
extern CSystem g_system;
extern ESystemStates g_systemStates;
extern TriggerLookup g_triggerLookup;
extern ParameterLookup g_parameterLookup;
extern SwitchLookup g_switchLookup;
extern PreloadRequestLookup g_preloadRequests;
extern EnvironmentLookup g_environmentLookup;
extern SettingLookup g_settingLookup;
extern TriggerInstances g_triggerInstances;
extern TriggerInstanceIdLookup g_triggerInstanceIdLookup;
extern ContextLookup g_contextLookup;
extern TriggerInstanceIds g_triggerInstanceIds;
extern CLoseFocusTrigger g_loseFocusTrigger;
extern CGetFocusTrigger g_getFocusTrigger;
extern CMuteAllTrigger g_muteAllTrigger;
extern CUnmuteAllTrigger g_unmuteAllTrigger;
extern CPauseAllTrigger g_pauseAllTrigger;
extern CResumeAllTrigger g_resumeAllTrigger;
extern Objects g_activeObjects;

extern SImplInfo g_implInfo;
extern CryFixedStringT<MaxFilePathLength> g_configPath;

extern TriggerInstanceId g_triggerInstanceIdCounter;
constexpr TriggerInstanceId g_maxTriggerInstanceId = std::numeric_limits<TriggerInstanceId>::max();

struct SPoolSizes final
{
	uint16 triggers = 0;
	uint16 parameters = 0;
	uint16 switches = 0;
	uint16 states = 0;
	uint16 environments = 0;
	uint16 preloads = 0;
	uint16 settings = 0;
	uint16 files = 0;
};

extern SPoolSizes g_poolSizes;

static void IncrementTriggerInstanceIdCounter()
{
	if (g_triggerInstanceIdCounter == g_maxTriggerInstanceId)
	{
		// Set to 1 because 0 is an invalid id.
		g_triggerInstanceIdCounter = 1;
	}
	else
	{
		++g_triggerInstanceIdCounter;
	}
}

#if defined(CRY_AUDIO_USE_DEBUG_CODE)
extern CListener g_previewListener;
extern Objects g_constructedObjects;

constexpr char const* g_szGlobalName = "Global";

constexpr char const* g_szPreviewListenerName = "Preview Listener";
constexpr ListenerId g_previewListenerId = StringToId("ThisIsTheHopefullyUniqueIdForThePreviewListener");

constexpr char const* g_szPreviewTriggerName = "preview_trigger";
constexpr ControlId g_previewTriggerId = StringToId(g_szPreviewTriggerName);

class CPreviewTrigger;
extern CPreviewTrigger g_previewTrigger;
extern CObject g_previewObject;
extern SPoolSizes g_debugPoolSizes;

using SwitchStateIds = std::map<ControlId, SwitchStateId>;
using ParameterValues = std::map<ControlId, float>;
using EnvironmentValues = std::map<EnvironmentId, float>;

extern ParameterValues g_parameters;
extern ParameterValues g_parametersGlobally;
extern SwitchStateIds g_switchStates;
extern SwitchStateIds g_switchStatesGlobally;

struct SContextInfo final
{
	SContextInfo() = delete;

	explicit SContextInfo(
		ContextId const contextId_,
		bool isRegistered_,
		bool isActive_)
		: contextId(contextId_)
		, isRegistered(isRegistered_)
		, isActive(isActive_)
	{}

	ContextId const contextId;
	bool            isRegistered;
	bool            isActive;
};

using ContextInfo = std::map<CryFixedStringT<MaxFileNameLength>, SContextInfo>;
extern ContextInfo g_contextInfo;

using ContextDebugInfo = std::map<CryFixedStringT<MaxFileNameLength>, bool>;
extern ContextDebugInfo g_contextDebugInfo;
#endif // CRY_AUDIO_USE_DEBUG_CODE
}      // namespace CryAudio
