// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include "IAudioInterfacesCommonData.h"
#include "../CrySystem/IEngineModule.h"
#include "../CryEntitySystem/IEntityBasicTypes.h"

// General macros.
//#define ENABLE_AUDIO_PORT_MESSAGES

#if defined(ENABLE_AUDIO_PORT_MESSAGES) && defined(_MSC_VER)
	#define AUDIO_STRINGANIZE2(x) # x
	#define AUDIO_STRINGANIZE1(x) AUDIO_STRINGANIZE2(x)
	#define TODO(y)               __pragma(message(__FILE__ "(" AUDIO_STRINGANIZE1(__LINE__) ") : " "[AUDIO] TODO >>> " AUDIO_STRINGANIZE2(y)))
	#define REINST_FULL(y)        __pragma(message(__FILE__ "(" AUDIO_STRINGANIZE1(__LINE__) ") : " "[AUDIO] REINST " __FUNCSIG__ " >>> " AUDIO_STRINGANIZE2(y)))
	#define REINST(y)             __pragma(message(__FILE__ "(" AUDIO_STRINGANIZE1(__LINE__) ") : " "[AUDIO] REINST " __FUNCTION__ " >>> " AUDIO_STRINGANIZE2(y)))
#else
	#define TODO(y)
	#define REINST_FULL(y)
	#define REINST(y)
#endif

class XmlNodeRef;

/**
 * @namespace CryAudio
 * @brief Most parent audio namespace used throughout the entire engine.
 */
namespace CryAudio
{
// Forward declarations.
struct IObject;
struct IListener;

namespace Impl
{
struct IImpl;
struct IObject;
struct ITriggerInfo;
} // namespace Impl

/**
 * @enum CryAudio::ESystemEvents
 * @brief A strongly typed enum class representing different audio system events that can be listened to.
 * @var CryAudio::ESystemEvents::None
 * @var CryAudio::ESystemEvents::ImplSet
 * @var CryAudio::ESystemEvents::TriggerExecuted
 * @var CryAudio::ESystemEvents::TriggerFinished
 * @var CryAudio::ESystemEvents::ContextActivated
 * @var CryAudio::ESystemEvents::ContextDeactivated
 * @var CryAudio::ESystemEvents::All
 */
enum class ESystemEvents : EnumFlagsType
{
	None               = 0,          /**< Used to initialize variables of this type and to determine whether the variable was properly handled. */
	ImplSet            = BIT(0),     /**< Invoked once the audio middleware implementation has been set. */
	TriggerExecuted    = BIT(1),     /**< Invoked once a trigger finished starting all of its event connections. */
	TriggerFinished    = BIT(2),     /**< Invoked once all of the spawned event instances finished playing. */
	ContextActivated   = BIT(3),     /**< Invoked once a context got activated. */
	ContextDeactivated = BIT(4),     /**< Invoked once a context got deactivated. */
	All                = 0xFFFFFFFF, /**< Listen to all supported audio system events. */
};
CRY_CREATE_ENUM_FLAG_OPERATORS(ESystemEvents);

/**
 * @enum CryAudio::ELogType
 * @brief A strongly typed enum class representing different audio specific log types.
 * @var CryAudio::ELogType::None
 * @var CryAudio::ELogType::Comment
 * @var CryAudio::ELogType::Warning
 * @var CryAudio::ELogType::Error
 * @var CryAudio::ELogType::Always
 */
enum class ELogType : EnumFlagsType
{
	None,    /**< Used to initialize variables of this type and to determine whether the variable was properly handled. */
	Comment, /**< The message will be displayed in standard color but verbosity level must be set to at least 4. */
	Warning, /**< The message will be displayed in orange color. */
	Error,   /**< The message will be displayed in red color. */
	Always,  /**< The message will be displayed in standard color and always printed regardless of verbosity level. */
};

/**
 * @enum CryAudio::EDefaultTriggerType
 * @brief A strongly typed enum class representing different default audio trigger types.
 * @var CryAudio::EDefaultTriggerType::None
 * @var CryAudio::EDefaultTriggerType::LoseFocus
 * @var CryAudio::EDefaultTriggerType::GetFocus
 * @var CryAudio::EDefaultTriggerType::MuteAll
 * @var CryAudio::EDefaultTriggerType::UnmuteAll
 * @var CryAudio::EDefaultTriggerType::PauseAll
 * @var CryAudio::EDefaultTriggerType::ResumeAll
 */
enum class EDefaultTriggerType : EnumFlagsType
{
	None,      /**< Used to initialize variables of this type and to determine whether the variable was properly handled. */
	LoseFocus, /**< Specifies to use the lose_focus default trigger. */
	GetFocus,  /**< Specifies to use the get_focus default trigger. */
	MuteAll,   /**< Specifies to use the mute_all default trigger. */
	UnmuteAll, /**< Specifies to use the unmute_all default trigger. */
	PauseAll,  /**< Specifies to use the pause_all default trigger. */
	ResumeAll, /**< Specifies to use the resume_all default trigger. */
};

struct SRequestInfo
{
	explicit SRequestInfo(
		ERequestResult const requestResult_,
		void* const pOwner_,
		void* const pUserData_,
		void* const pUserDataOwner_,
		ESystemEvents const systemEvent_,
		ControlId const audioControlId_,
		EntityId const entityId_)
		: requestResult(requestResult_)
		, pOwner(pOwner_)
		, pUserData(pUserData_)
		, pUserDataOwner(pUserDataOwner_)
		, systemEvent(systemEvent_)
		, audioControlId(audioControlId_)
		, entityId(entityId_)
	{}

	SRequestInfo(SRequestInfo const&) = delete;
	SRequestInfo(SRequestInfo&&) = delete;
	SRequestInfo& operator=(SRequestInfo const&) = delete;
	SRequestInfo& operator=(SRequestInfo&&) = delete;

	ERequestResult const requestResult;
	void* const          pOwner;
	void* const          pUserData;
	void* const          pUserDataOwner;
	ESystemEvents const  systemEvent;
	ControlId const      audioControlId;
	EntityId const       entityId;
};

struct SCreateObjectData
{
	explicit SCreateObjectData(
		char const* const szName_ = nullptr,
		EOcclusionType const occlusionType_ = EOcclusionType::Ignore,
		CTransformation const& transformation_ = CTransformation::GetEmptyObject(),
		bool const setCurrentEnvironments_ = false,
		ListenerIds const& listenerIds_ = g_defaultListenerIds)
		: szName(szName_)
		, occlusionType(occlusionType_)
		, transformation(transformation_)
		, setCurrentEnvironments(setCurrentEnvironments_)
		, listenerIds(listenerIds_)
	{}

	static SCreateObjectData const& GetEmptyObject() { static SCreateObjectData const emptyInstance; return emptyInstance; }

	char const* const               szName;
	EOcclusionType const            occlusionType;

	// We opt for copying the transformation instead of storing a reference in order to prevent a potential dangling-reference bug.
	// Callers might pass a vector or matrix to the constructor, which implicitly convert to CAudioObjectTransformation.
	// Implicit conversion introduces a temporary object, and a reference could potentially dangle,
	// as the temporary gets destroyed before this request gets passed to the AudioSystem where it gets ultimately copied for internal processing.
	CTransformation const transformation;

	bool const            setCurrentEnvironments;
	ListenerIds const     listenerIds;
};

struct SExecuteTriggerData : public SCreateObjectData
{
	explicit SExecuteTriggerData(
		ControlId const triggerId_,
		char const* const szName_ = nullptr,
		EOcclusionType const occlusionType_ = EOcclusionType::Ignore,
		CTransformation const& transformation_ = CTransformation::GetEmptyObject(),
		EntityId const entityId_ = INVALID_ENTITYID,
		bool const setCurrentEnvironments_ = false)
		: SCreateObjectData(szName_, occlusionType_, transformation_, setCurrentEnvironments_)
		, triggerId(triggerId_)
		, entityId(entityId_)
	{}

	ControlId const triggerId;
	EntityId const  entityId;
};

struct ISystemModule : public Cry::IDefaultModule
{
	CRYINTERFACE_DECLARE_GUID(ISystemModule, "6c7ba422-375b-4325-ae00-918679610d2e"_cry_guid);
};

struct IImplModule : public Cry::IDefaultModule
{
	CRYINTERFACE_DECLARE_GUID(IImplModule, "5c4adbec-a343-49ce-b799-2a856cdd553b"_cry_guid);
};

//! Main interface to the audio system, allowing access to audio playback via implementation plug-ins.
struct IAudioSystem
{
	// <interfuscator:shuffle>
	/** @cond */
	virtual ~IAudioSystem() = default;
	/** @endcond */

	/**
	 * This is called during shutdown of the engine which releases AudioSystem resources.
	 * @return void
	 */
	virtual void Release() = 0;

	/**
	 * Used by audio middleware implementations to register themselves with the AudioSystem.
	 * @param pIImpl - pointer to the audio middleware implementation to register.
	 * @param userData - optional struct used to pass additional data to the internal request.
	 * @return void
	 */
	virtual void SetImpl(Impl::IImpl* const pIImpl, SRequestUserData const& userData = SRequestUserData::GetEmptyObject()) = 0;

	/**
	 * Performs the actions passed in the "triggerData" parameter. This is used for 3D type events exclusively. For 2D type events refer to ExecuteTrigger.
	 * For convenience and efficiency this is used as a "fire and forget" type action where the user does not need to explicitly handle an audio object.
	 * Make sure to only start non-looped type events this way otherwise they will turn into runaway loops.
	 * @param triggerData - reference to an object that holds all of the data necessary for the trigger execution.
	 * @param userData - optional struct used to pass additional data to the internal request.
	 * @return void
	 */
	virtual void ExecuteTriggerEx(SExecuteTriggerData const& triggerData, SRequestUserData const& userData = SRequestUserData::GetEmptyObject()) = 0;

	/**
	 * Executes the passed trigger ID. This is used for 2D type events exclusively. For 3D type events refer to ExecuteTriggerEx.
	 * @param triggerId - ID of the trigger to execute.
	 * @param userData - optional struct used to pass additional data to the internal request.
	 * @return void
	 * @see StopTrigger
	 */
	virtual void ExecuteTrigger(ControlId const triggerId, SRequestUserData const& userData = SRequestUserData::GetEmptyObject()) = 0;

	/**
	 * Stops all instances of the passed trigger ID or all instances of all active triggers if CryAudio::InvalidControlId (default) is passed.
	 * @param triggerId - ID of the trigger to stop.
	 * @param userData - optional struct used to pass additional data to the internal request.
	 * @return void
	 * @see ExecuteTrigger
	 */
	virtual void StopTrigger(ControlId const triggerId = InvalidControlId, SRequestUserData const& userData = SRequestUserData::GetEmptyObject()) = 0;

	/**
	 * Set a parameter to a given value on the global object.
	 * @param parameterId - ID of the parameter in question.
	 * @param value - floating point value to which the parameter should be set.
	 * @param userData - optional struct used to pass additional data to the internal request.
	 * @return void
	 */
	virtual void SetParameter(ControlId const parameterId, float const value, SRequestUserData const& userData = SRequestUserData::GetEmptyObject()) = 0;

	/**
	 * Set a parameter to a given value on all objects.
	 * @param parameterId - ID of the parameter in question.
	 * @param value - floating point value to which the parameter should be set.
	 * @param userData - optional struct used to pass additional data to the internal request.
	 * @return void
	 */
	virtual void SetParameterGlobally(ControlId const parameterId, float const value, SRequestUserData const& userData = SRequestUserData::GetEmptyObject()) = 0;

	/**
	 * Set a switch to a given state on the global object.
	 * @param switchId - ID of the switch in question.
	 * @param switchStateId - ID of the switch's state in question.
	 * @param userData - optional struct used to pass additional data to the internal request.
	 * @return void
	 */
	virtual void SetSwitchState(ControlId const switchId, SwitchStateId const switchStateId, SRequestUserData const& userData = SRequestUserData::GetEmptyObject()) = 0;

	/**
	 * Set a switch to a given state on all objects.
	 * @param switchId - ID of the switch in question.
	 * @param switchStateId - ID of the switch's state in question.
	 * @param userData - optional struct used to pass additional data to the internal request.
	 * @return void
	 */
	virtual void SetSwitchStateGlobally(ControlId const switchId, SwitchStateId const switchStateId, SRequestUserData const& userData = SRequestUserData::GetEmptyObject()) = 0;

	/**
	 * Used by audio middleware implementations to inform the AudioSystem that an instance of a trigger connection has started.
	 * @param triggerInstanceId - id of the the instance of the trigger connection that started.
	 * @param result - trigger result of the trigger connection instance when is started.
	 * @param userData - optional struct used to pass additional data to the internal request.
	 * @return void
	 */
	virtual void ReportStartedTriggerConnectionInstance(TriggerInstanceId const triggerInstanceId, ETriggerResult const result, SRequestUserData const& userData = SRequestUserData::GetEmptyObject()) = 0;

	/**
	 * Used by audio middleware implementations to inform the AudioSystem that an instance of a trigger connection finished producing sound.
	 * @param triggerInstanceId - id of the the instance of the trigger connection that finished producing sound.
	 * @param result - trigger result of the trigger connection instance that finished producing sound.
	 * @param userData - optional struct used to pass additional data to the internal request.
	 * @return void
	 */
	virtual void ReportFinishedTriggerConnectionInstance(TriggerInstanceId const triggerInstanceId, ETriggerResult const result, SRequestUserData const& userData = SRequestUserData::GetEmptyObject()) = 0;

	/**
	 * Used by audio middleware implementations to inform the AudioSystem that an object got physical.
	 * @param pIObject - middleware implementation specific object that got physicalized.
	 * @param userData - optional struct used to pass additional data to the internal request.
	 * @return void
	 */
	virtual void ReportPhysicalizedObject(Impl::IObject* const pIObject, SRequestUserData const& userData = SRequestUserData::GetEmptyObject()) = 0;

	/**
	 * Used by audio middleware implementations to inform the AudioSystem that an object got virtual.
	 * @param pIObject - middleware implementation specific object that got virtual.
	 * @param userData - optional struct used to pass additional data to the internal request.
	 * @return void
	 */
	virtual void ReportVirtualizedObject(Impl::IObject* const pIObject, SRequestUserData const& userData = SRequestUserData::GetEmptyObject()) = 0;

	/**
	 * Used to instruct the AudioSystem that it should stop all playing sounds.
	 * @param userData - optional struct used to pass additional data to the internal request.
	 * @return void
	 */
	virtual void StopAllSounds(SRequestUserData const& userData = SRequestUserData::GetEmptyObject()) = 0;

	/**
	 * Loads all of the data referenced by the given preload request.
	 * @param id - ID of the preload request in question.
	 * @param bAutoLoadOnly - boolean indicating whether to load the given preload request only if it's been set to AutoLoad.
	 * @param userData - optional struct used to pass additional data to the internal request.
	 * @return void
	 * @see UnloadSingleRequest
	 */
	virtual void PreloadSingleRequest(PreloadRequestId const id, bool const bAutoLoadOnly, SRequestUserData const& userData = SRequestUserData::GetEmptyObject()) = 0;

	/**
	 * Unloads all of the data referenced by the given preload request.
	 * @param id - ID of the preload request in question.
	 * @param userData - optional struct used to pass additional data to the internal request.
	 * @return void
	 * @see PreloadSingleRequest
	 */
	virtual void UnloadSingleRequest(PreloadRequestId const id, SRequestUserData const& userData = SRequestUserData::GetEmptyObject()) = 0;

	/**
	 * Activates the given context. Loads all meta data and auto-loaded preload requests that belong to that context.
	 * This should only get called during loading the game or a level!
	 * @param contextId - id of the context to activate.
	 * @return void
	 * @see DeactivateContext
	 */
	virtual void ActivateContext(ContextId const contextId) = 0;

	/**
	 * Deactivates the given context. unloads all meta data and preload requests that belong to that context.
	 * This should only get called during loading the game or a level!
	 * @param contextId - id of the context to deactivate.
	 * @return void
	 * @see ActivateContext
	 */
	virtual void DeactivateContext(ContextId const contextId) = 0;

	/**
	 * Loads a setting.
	 * @param id - ID of the setting in question.
	 * @param userData - optional struct used to pass additional data to the internal request.
	 * @return void
	 * @see UnloadSetting
	 */
	virtual void LoadSetting(ControlId const id, SRequestUserData const& userData = SRequestUserData::GetEmptyObject()) = 0;

	/**
	 * Unloads a setting.
	 * @param id - ID of thes etting in question.
	 * @param userData - optional struct used to pass additional data to the internal request.
	 * @return void
	 * @see LoadSetting
	 */
	virtual void UnloadSetting(ControlId const id, SRequestUserData const& userData = SRequestUserData::GetEmptyObject()) = 0;

	/**
	 * Reloads all of the audio controls and their connections.
	 * @param userData - optional struct used to pass additional data to the internal request.
	 * @return void
	 */
	virtual void ReloadControlsData(SRequestUserData const& userData = SRequestUserData::GetEmptyObject()) = 0;

	/**
	 * Used to register a callback function that is called whenever a given event occurred.
	 * @param func - address of the function to be called.
	 * @param pObjectToListenTo - address of the object in which events one is interested. If set to nullptr events of the given type produced by any object will be listened to.
	 * @param eventMask - a combination of CryAudio::ESystemEvents one is interested in. If set to ESystemEvents::All, all events generated by the given object will be received.
	 * @return void
	 * @see RemoveRequestListener
	 */
	virtual void AddRequestListener(void (*func)(SRequestInfo const* const), void* const pObjectToListenTo, ESystemEvents const eventMask) = 0;

	/**
	 * Used to unregister a callback function.
	 * @param func - address of the function to be called.
	 * @param pObjectToListenTo - address of the object in which events one is interested. If set to nullptr events of the given type produced by any object will be listened to.
	 * @return void
	 * @see AddRequestListener
	 */
	virtual void RemoveRequestListener(void (*func)(SRequestInfo const* const), void* const pObjectToListenTo) = 0;

	/**
	 * Ideally called by the application's main thread.
	 * Note: If requests are set to call back from an external thread then this will be the thread that calls back.
	 * @return void
	 */
	virtual void ExternalUpdate() = 0;

	/**
	 * Returns the path in which audio data is stored.
	 * @return char const pointer to the string holding the location.
	 */
	virtual char const* GetConfigPath() const = 0;

	/**
	 * Constructs an instance of an audio listener.
	 * Note: Retrieving a listener this way requires the instance to be freed via ReleaseListener once not needed anymore!
	 * @param transformation - transformation of the listener to be created.
	 * @param szName - name of the listener to be created.
	 * @return Pointer to a freshly constructed CryAudio::IListener instance.
	 * @see ReleaseListener
	 */
	virtual IListener* CreateListener(CTransformation const& transformation, char const* const szName) = 0;

	/**
	 * Destructs the passed audio listener instance.
	 * @param pIListener - Pointer to the audio listener that needs destruction.
	 * @return void
	 * @see CreateListener
	 */
	virtual void ReleaseListener(IListener* const pIListener) = 0;

	/**
	 * Returns a pointer to the requested listener.
	 * @param id - id of the requested listener. If none is provided, a pointer to the default listener is returned.
	 * @return Pointer to the requested listener.
	 */
	virtual IListener* GetListener(ListenerId const id = DefaultListenerId) = 0;

	/**
	 * Constructs an instance of an audio object.
	 * Note: Retrieving an object this way requires the object to be freed via ReleaseObject once not needed anymore!
	 * @param objectData - optional data used during audio object construction.
	 * @param userData - optional struct used to pass additional data to the internal request.
	 * @return Pointer to a freshly constructed CryAudio::IObject instance.
	 * @see ReleaseObject
	 */
	virtual IObject* CreateObject(SCreateObjectData const& objectData = SCreateObjectData::GetEmptyObject()) = 0;

	/**
	 * Destructs the passed audio object instance.
	 * @param pIObject - Pointer to the audio object that needs destruction.
	 * @return void
	 * @see CreateObject
	 */
	virtual void ReleaseObject(IObject* const pIObject) = 0;

	/**
	 * Retrieve an audio trigger's attributes.
	 * @param triggerId - id of the trigger in question.
	 * @param triggerData - out parameter which receives the trigger's data.
	 * @return void
	 */
	virtual void GetTriggerData(ControlId const triggerId, STriggerData& triggerData) = 0;

	/**
	 * Retrieve information about the current middleware implementation.
	 * @param[out] implInfo - a reference to an instance of SImplInfo
	 * @return void
	 */
	virtual void GetImplInfo(SImplInfo& implInfo) = 0;

	/**
	 * Logs an audio specific message and adds an audio tag plus time stamp to the string.
	 * Note: Don't use this method directly, instead use Cry::Audio::Log()!
	 * @param type - log message type (ELogType)
	 * @param szFormat, ... - printf-style format string and its argument
	 * @return void
	 */
	virtual void Log(ELogType const type, char const* const szFormat, ...) = 0;

	//////////////////////////////////////////////////////////////////////////
	// NOTE: The methods below are ONLY USED when INCLUDE_AUDIO_PRODUCTION_CODE is defined!
	//////////////////////////////////////////////////////////////////////////

	/**
	 * Executes a trigger from the given id on the preview object.
	 * @param triggerId - id of the trigger.
	 * @return void
	 * @see StopPreviewTrigger
	 */
	virtual void ExecutePreviewTrigger(ControlId const triggerId) = 0;

	/**
	 * Constructs a trigger from the given info struct and executes it on the preview object.
	 * @param triggerInfo - info struct to construct a trigger.
	 * @return void
	 * @see StopPreviewTrigger
	 */
	virtual void ExecutePreviewTriggerEx(Impl::ITriggerInfo const& triggerInfo) = 0;

	/**
	 * Constructs a trigger from the given XML node and executes it on the preview object.
	 * @param node - XML node to construct a trigger.
	 * @return void
	 * @see StopPreviewTrigger
	 */
	virtual void ExecutePreviewTriggerEx(XmlNodeRef const& node) = 0;

	/**
	 * Stops the active trigger on the preview object.
	 * @return void
	 * @see ExecutePreviewTrigger
	 */
	virtual void StopPreviewTrigger() = 0;
	// </interfuscator:shuffle>
};
} // namespace CryAudio

AUTO_TYPE_INFO(CryAudio::EOcclusionType);
