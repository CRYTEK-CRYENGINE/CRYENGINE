// Copyright 2001-2016 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include "IAudioInterfacesCommonData.h"
#include "../CryCore/CryCrc32.h"
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

/**
 * @namespace CryAudio
 * @brief Most parent audio namespace used throughout the entire engine.
 */
namespace CryAudio
{
static constexpr char* RelativeVelocityTrackingSwitchName = "relative_velocity_tracking";
static constexpr char* RelativeVelocityParameterName = "relative_velocity";
static constexpr char* AbsoluteVelocityTrackingSwitchName = "absolute_velocity_tracking";
static constexpr char* AbsoluteVelocityParameterName = "absolute_velocity";
static constexpr char* LoseFocusTriggerName = "lose_focus";
static constexpr char* GetFocusTriggerName = "get_focus";
static constexpr char* MuteAllTriggerName = "mute_all";
static constexpr char* UnmuteAllTriggerName = "unmute_all";
static constexpr char* DoNothingTriggerName = "do_nothing";
static constexpr char* OcclusionCalcSwitchName = "ObstructionOcclusionCalculationType";
static constexpr char* IgnoreStateName = "ignore";
static constexpr char* AdaptiveStateName = "adaptive";
static constexpr char* LowStateName = "low";
static constexpr char* MediumStateName = "medium";
static constexpr char* HighStateName = "high";
static constexpr char* OnStateName = "on";
static constexpr char* OffStateName = "off";
static constexpr char* GlobalPreloadRequestName = "global_atl_preloads";

/**
* A utility function to convert a string value to an Id at compile time.
* @param szSource - string to convert
* @return a 32bit CRC computed on the lower case version of the passed string
*/
static constexpr uint32 StringToId_CompileTime(char const* const szSource)
{
	return CCrc32::ComputeLowercase_CompileTime(szSource);
}

/**
* A utility function to convert a string value to an Id during run time.
* @param szSource - string to convert
* @return a 32bit CRC computed on the lower case version of the passed string
*/
static uint32 StringToId_RunTime(char const* const szSource)
{
	return CCrc32::ComputeLowercase(szSource);
}

static constexpr ControlId RelativeVelocityTrackingSwitchId = StringToId_CompileTime(RelativeVelocityTrackingSwitchName);
static constexpr ControlId RelativeVelocityParameterId = StringToId_CompileTime(RelativeVelocityParameterName);
static constexpr ControlId AbsoluteVelocityTrackingSwitchId = StringToId_CompileTime(AbsoluteVelocityTrackingSwitchName);
static constexpr ControlId AbsoluteVelocityParameterId = StringToId_CompileTime(AbsoluteVelocityParameterName);
static constexpr ControlId LoseFocusTriggerId = StringToId_CompileTime(LoseFocusTriggerName);
static constexpr ControlId GetFocusTriggerId = StringToId_CompileTime(GetFocusTriggerName);
static constexpr ControlId MuteAllTriggerId = StringToId_CompileTime(MuteAllTriggerName);
static constexpr ControlId UnmuteAllTriggerId = StringToId_CompileTime(UnmuteAllTriggerName);
static constexpr ControlId DoNothingTriggerId = StringToId_CompileTime(DoNothingTriggerName);
static constexpr ControlId OcclusionCalcSwitchId = StringToId_CompileTime(OcclusionCalcSwitchName);
static constexpr SwitchStateId IgnoreStateId = StringToId_CompileTime(IgnoreStateName);
static constexpr SwitchStateId AdaptiveStateId = StringToId_CompileTime(AdaptiveStateName);
static constexpr SwitchStateId LowStateId = StringToId_CompileTime(LowStateName);
static constexpr SwitchStateId MediumStateId = StringToId_CompileTime(MediumStateName);
static constexpr SwitchStateId HighStateId = StringToId_CompileTime(HighStateName);
static constexpr SwitchStateId OnStateId = StringToId_CompileTime(OnStateName);
static constexpr SwitchStateId OffStateId = StringToId_CompileTime(OffStateName);
static constexpr PreloadRequestId GlobalPreloadRequestId = StringToId_CompileTime(GlobalPreloadRequestName);

// Forward declarations.
struct IListener;
struct IProfileData;
namespace Impl
{
struct IImpl;
}

enum class ESystemEvents : EnumFlagsType
{
	None            = 0,
	ImplSet         = BIT(0),
	TriggerExecuted = BIT(1),
	TriggerFinished = BIT(2),
	FilePlay        = BIT(3),
	FileStarted     = BIT(4),
	FileStopped     = BIT(5),
	All             = 0xFFFFFFFF,
};
CRY_CREATE_ENUM_FLAG_OPERATORS(ESystemEvents);

enum class EDataScope : EnumFlagsType
{
	None,
	Global,
	LevelSpecific,
	All,
};

enum class EOcclusionType : EnumFlagsType
{
	None,
	Ignore,
	Adaptive,
	Low,
	Medium,
	High,
	Count,
};
CRY_CREATE_ENUM_FLAG_OPERATORS(EOcclusionType);

struct SRequestInfo
{
	explicit SRequestInfo(
	  ERequestResult const requestResult_,
	  void* const pOwner_,
	  void* const pUserData_,
	  void* const pUserDataOwner_,
	  ESystemEvents const systemEvent_,
	  ControlId const audioControlId_,
	  IObject* const pAudioObject_,
	  CATLStandaloneFile* pStandaloneFile_,
	  CATLEvent* pAudioEvent_)
		: requestResult(requestResult_)
		, pOwner(pOwner_)
		, pUserData(pUserData_)
		, pUserDataOwner(pUserDataOwner_)
		, systemEvent(systemEvent_)
		, audioControlId(audioControlId_)
		, pAudioObject(pAudioObject_)
		, pStandaloneFile(pStandaloneFile_)
		, pAudioEvent(pAudioEvent_)
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
	IObject* const       pAudioObject;
	CATLStandaloneFile*  pStandaloneFile;
	CATLEvent*           pAudioEvent;
};

struct SCreateObjectData
{
	explicit SCreateObjectData(
	  char const* const szName_ = nullptr,
	  EOcclusionType const occlusionType_ = EOcclusionType::Ignore,
	  CObjectTransformation const& transformation_ = CObjectTransformation::GetEmptyObject(),
	  EntityId const entityId_ = INVALID_ENTITYID,
	  bool const bSetCurrentEnvironments_ = false)
		: szName(szName_)
		, occlusionType(occlusionType_)
		, transformation(transformation_)
		, entityId(entityId_)
		, bSetCurrentEnvironments(bSetCurrentEnvironments_)
	{}

	static SCreateObjectData const& GetEmptyObject() { static SCreateObjectData const emptyInstance; return emptyInstance; }

	char const* const               szName;
	EOcclusionType const            occlusionType;

	// We opt for copying the transformation instead of storing a reference in order to prevent a potential dangling-reference bug.
	// Callers might pass a vector or matrix to the constructor, which implicitly convert to CAudioObjectTransformation.
	// Implicit conversion introduces a temporary object, and a reference could potentially dangle,
	// as the temporary gets destroyed before this request gets passed to the AudioSystem where it gets ultimately copied for internal processing.
	CObjectTransformation const transformation;

	EntityId const              entityId;
	bool const                  bSetCurrentEnvironments;
};

struct SExecuteTriggerData : public SCreateObjectData
{
	explicit SExecuteTriggerData(
	  char const* const _szName,
	  EOcclusionType const _occlusionType,
	  CObjectTransformation const& _transformation,
	  bool const _bSetCurrentEnvironments,
	  ControlId const _triggerId)
		: SCreateObjectData(_szName, _occlusionType, _transformation, _bSetCurrentEnvironments)
		, triggerId(_triggerId)
	{}

	ControlId const triggerId;
};

struct ISystemModule : public Cry::IDefaultModule
{
	CRYINTERFACE_DECLARE_GUID(ISystemModule, "6c7ba422-375b-4325-ae00-918679610d2e"_cry_guid);
};

struct IImplModule : public Cry::IDefaultModule
{
	CRYINTERFACE_DECLARE_GUID(IImplModule, "5c4adbec-a343-49ce-b799-2a856cdd553b"_cry_guid);
};

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
	 * Loads a trigger's data. This can be used in "loading on demand" situations to prepare the data referenced by a trigger.
	 * @param triggerId - ID of the trigger in question.
	 * @param userData - optional struct used to pass additional data to the internal request.
	 * @return void
	 * @see UnloadTrigger
	 */
	virtual void LoadTrigger(ControlId const triggerId, SRequestUserData const& userData = SRequestUserData::GetEmptyObject()) = 0;

	/**
	 * Unloads a trigger's data. This can be used in "unloading on demand" situations to free the data referenced by a trigger.
	 * @param triggerId - ID of the trigger in question.
	 * @param userData - optional struct used to pass additional data to the internal request.
	 * @return void
	 * @see LoadTrigger
	 */
	virtual void UnloadTrigger(ControlId const triggerId, SRequestUserData const& userData = SRequestUserData::GetEmptyObject()) = 0;

	/**
	 * Performs the actions passed in the "triggerData" parameter on an audio object and then executes the passed trigger ID.
	 * For convenience and efficiency this is used as a "fire and forget" type action where the user does not need to explicitly handle an audio object.
	 * @param triggerData - reference to an object that holds all of the data necessary for the trigger execution.
	 * @param userData - optional struct used to pass additional data to the internal request.
	 * @return void
	 */
	virtual void ExecuteTriggerEx(SExecuteTriggerData const& triggerData, SRequestUserData const& userData = SRequestUserData::GetEmptyObject()) = 0;

	/**
	 * Executes the passed trigger ID. This is used for "fire and forget" type operations and for 2D type events exclusively.
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
	 * Globally set a parameter to a given value.
	 * @param parameterId - ID of the parameter in question.
	 * @param value - floating point value to which the parameter should be set.
	 * @param userData - optional struct used to pass additional data to the internal request.
	 * @return void
	 */
	virtual void SetParameter(ControlId const parameterId, float const value, SRequestUserData const& userData = SRequestUserData::GetEmptyObject()) = 0;

	/**
	 * Globally set a switch to a given state.
	 * @param switchId - ID of the switch in question.
	 * @param switchStateId - ID of the switch's state in question.
	 * @param userData - optional struct used to pass additional data to the internal request.
	 * @return void
	 */
	virtual void SetSwitchState(ControlId const switchId, SwitchStateId const switchStateId, SRequestUserData const& userData = SRequestUserData::GetEmptyObject()) = 0;

	/**
	 * Globally plays a file.
	 * @param playFileInfo - reference to a struct that holds data necessary for playback.
	 * @param userData - optional struct used to pass additional data to the internal request.
	 * @return void
	 * @see StopFile
	 */
	virtual void PlayFile(SPlayFileInfo const& playFileInfo, SRequestUserData const& userData = SRequestUserData::GetEmptyObject()) = 0;

	/**
	 * Globally stops a file.
	 * @param szName - name of the file in question.
	 * @param userData - optional struct used to pass additional data to the internal request.
	 * @return void
	 * @see PlayFile
	 */
	virtual void StopFile(char const* const szName, SRequestUserData const& userData = SRequestUserData::GetEmptyObject()) = 0;

	/**
	 * Used by audio middleware implementations to inform the AudioSystem that a file started playback.
	 * @param standaloneFile - reference to the instance of the file that started playback.
	 * @param bSuccessfullyStarted - boolean indicating whether playback started successfully or not.
	 * @param userData - optional struct used to pass additional data to the internal request.
	 * @return void
	 * @see ReportStoppedFile
	 */
	virtual void ReportStartedFile(CATLStandaloneFile& standaloneFile, bool const bSuccessfullyStarted, SRequestUserData const& userData = SRequestUserData::GetEmptyObject()) = 0;

	/**
	 * Used by audio middleware implementations to inform the AudioSystem that a file stopped playback.
	 * @param standaloneFile - reference to the instance of the file that stopped playback.
	 * @param userData - optional struct used to pass additional data to the internal request.
	 * @return void
	 * @see ReportStartedFile
	 */
	virtual void ReportStoppedFile(CATLStandaloneFile& standaloneFile, SRequestUserData const& userData = SRequestUserData::GetEmptyObject()) = 0;

	/**
	 * Used by audio middleware implementations to inform the AudioSystem that an event finished producing sound.
	 * @param event - reference to the instance of the event that finished producing sound.
	 * @param bSuccess - boolean indicating whether the event finished successfully or not.
	 * @param userData - optional struct used to pass additional data to the internal request.
	 * @return void
	 */
	virtual void ReportFinishedEvent(CATLEvent& event, bool const bSuccess, SRequestUserData const& userData = SRequestUserData::GetEmptyObject()) = 0;

	/**
	 * Used by the engine to inform the AudioSystem that the application window lost focus.
	 * @param userData - optional struct used to pass additional data to the internal request.
	 * @return void
	 * @see GotFocus
	 */
	virtual void LostFocus(SRequestUserData const& userData = SRequestUserData::GetEmptyObject()) = 0;

	/**
	 * Used by the engine to inform the AudioSystem that the application window got focus.
	 * @param userData - optional struct used to pass additional data to the internal request.
	 * @return void
	 * @see LostFocus
	 */
	virtual void GotFocus(SRequestUserData const& userData = SRequestUserData::GetEmptyObject()) = 0;

	/**
	 * Used to instruct the AudioSystem that it should mute all active sounds.
	 * @param userData - optional struct used to pass additional data to the internal request.
	 * @return void
	 * @see UnmuteAll
	 */
	virtual void MuteAll(SRequestUserData const& userData = SRequestUserData::GetEmptyObject()) = 0;

	/**
	 * Used to instruct the AudioSystem that it should unmute all active sounds.
	 * @param userData - optional struct used to pass additional data to the internal request.
	 * @return void
	 * @see MuteAll
	 */
	virtual void UnmuteAll(SRequestUserData const& userData = SRequestUserData::GetEmptyObject()) = 0;

	/**
	 * Used to instruct the AudioSystem that it should stop all playing sounds.
	 * @param userData - optional struct used to pass additional data to the internal request.
	 * @return void
	 */
	virtual void StopAllSounds(SRequestUserData const& userData = SRequestUserData::GetEmptyObject()) = 0;

	/**
	 * Used to reload the registered audio middleware.
	 * This is useful when for instance an audio project changed while the application was running.
	 * @param szLevelName - name of the currently loaded level so that level specific data gets reloaded as well.
	 * @param userData - optional struct used to pass additional data to the internal request.
	 * @return void
	 */
	virtual void Refresh(char const* const szLevelName, SRequestUserData const& userData = SRequestUserData::GetEmptyObject()) = 0;

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
	 * Reloads all of the audio controls and their connections.
	 * @param szFolderPath - path to where the audio controls data has been stored.
	 * @param szLevelName - name of the currently loaded level to also reload level specific data.
	 * @param userData - optional struct used to pass additional data to the internal request.
	 * @return void
	 */
	virtual void ReloadControlsData(char const* const szFolderPath, char const* const szLevelName, SRequestUserData const& userData = SRequestUserData::GetEmptyObject()) = 0;

	/**
	 * Used to register a callback function that is called whenever a given event occurred.
	 * @param func - address of the function to be called.
	 * @param pObjectToListenTo - address of the object in which events one is interested. If set to nullptr events of the given type produced by any object will be listened to.
	 * @param eventMask - a combination of CryAudio::ESystemEvents one is interested in. If set to ESystemEvents::All, all events generated by the given object will be received.
	 * @return void
	 * @see RemoveRequestListener
	 */
	virtual void AddRequestListener(void (* func)(SRequestInfo const* const), void* const pObjectToListenTo, ESystemEvents const eventMask) = 0;

	/**
	 * Used to unregister a callback function.
	 * @param func - address of the function to be called.
	 * @param pObjectToListenTo - address of the object in which events one is interested. If set to nullptr events of the given type produced by any object will be listened to.
	 * @return void
	 * @see AddRequestListener
	 */
	virtual void RemoveRequestListener(void (* func)(SRequestInfo const* const), void* const pObjectToListenTo) = 0;

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
	 * @param szName - optional name of the listener to be created.
	 * @return Pointer to a freshly constructed CryAudio::IListener instance.
	 * @see ReleaseListener
	 */
	virtual IListener* CreateListener(char const* const szName = nullptr) = 0;

	/**
	 * Destructs the passed audio listener instance.
	 * @param pIListener - Pointer to the audio listener that needs destruction.
	 * @return void
	 * @see CreateListener
	 */
	virtual void ReleaseListener(IListener* const pIListener) = 0;

	/**
	 * Constructs an instance of an audio object.
	 * Note: Retrieving an object this way requires the object to be freed via ReleaseObject once not needed anymore!
	 * @param objectData - optional data used during audio object construction.
	 * @param userData - optional struct used to pass additional data to the internal request.
	 * @return Pointer to a freshly constructed CryAudio::IObject instance.
	 * @see ReleaseObject
	 */
	virtual IObject* CreateObject(SCreateObjectData const& objectData = SCreateObjectData::GetEmptyObject(), SRequestUserData const& userData = SRequestUserData::GetEmptyObject()) = 0;

	/**
	 * Destructs the passed audio object instance.
	 * @param pIObject - Pointer to the audio object that needs destruction.
	 * @return void
	 * @see CreateObject
	 */
	virtual void ReleaseObject(IObject* const pIObject) = 0;

	/**
	 * Retrieve an audio file's attributes.
	 * @param szName - name of the file in question.
	 * @param fileData - out parameter which receives the file's data.
	 * @return void
	 */
	virtual void GetFileData(char const* const szName, SFileData& fileData) = 0;

	/**
	 * Retrieve an audio trigger's attributes.
	 * @param triggerId - id of the trigger in question.
	 * @param triggerData - out parameter which receives the trigger's data.
	 * @return void
	 */
	virtual void GetTriggerData(ControlId const triggerId, STriggerData& triggerData) = 0;

	/**
	 * This method is called by the LevelSystem whenever a level is loaded.
	 * It allows the AudioSystem to handle its data accordingly.
	 * @param szLevelName - name of the level that is being loaded.
	 * @return void
	 */
	virtual void OnLoadLevel(char const* const szLevelName) = 0;

	/**
	 * This method is called by the LevelSystem whenever a level is unloaded.
	 * It allows the AudioSystem to handle its data accordingly.
	 * @return void
	 */
	virtual void OnUnloadLevel() = 0;

	/**
	 * This method is called whenever the language is changed.
	 * It allows the AudioSystem to reload language specific data.
	 * @return void
	 */
	virtual void OnLanguageChanged() = 0;

	/**
	 * Retrieve ProfileData interface which gives access to various methods retrieving audio system internal data.
	 * @return void
	 */
	virtual IProfileData* GetProfileData() const = 0;
	// </interfuscator:shuffle>
};
} // namespace CryAudio

AUTO_TYPE_INFO(CryAudio::EOcclusionType);
