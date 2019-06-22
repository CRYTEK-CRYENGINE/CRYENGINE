// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include "../CryMath/Cry_Math.h"
#include "../CryCore/BaseTypes.h"
#include "../CryCore/smartptr.h"
#include "../CryCore/Platform/platform.h"
#include "../CryCore/CryEnumMacro.h"
#include "../CryCore/CryCrc32.h"
#include "../CryCore/Containers/CryArray.h"

#define CRY_AUDIO_DATA_ROOT "audio"

namespace CryAudio
{
using IdType = uint32;
using ControlId = IdType;
using SwitchStateId = IdType;
using EnvironmentId = IdType;
using PreloadRequestId = IdType;
using FileId = IdType;
using TriggerInstanceId = IdType;
using EnumFlagsType = IdType;
using AuxObjectId = IdType;
using LibraryId = IdType;
using ContextId = IdType;
using ListenerId = IdType;

using ListenerIds = DynArray<ListenerId>;

constexpr ControlId InvalidControlId = 0;
constexpr SwitchStateId InvalidSwitchStateId = 0;
constexpr EnvironmentId InvalidEnvironmentId = 0;
constexpr PreloadRequestId InvalidPreloadRequestId = 0;
constexpr FileId InvalidFileId = 0;
constexpr TriggerInstanceId InvalidTriggerInstanceId = 0;
constexpr AuxObjectId InvalidAuxObjectId = 0;
constexpr AuxObjectId DefaultAuxObjectId = 1;
constexpr ContextId InvalidContextId = 0;
constexpr ListenerId InvalidListenerId = 0;
constexpr uint8 MaxInfoStringLength = 128;
constexpr uint8 MaxControlNameLength = 128;
constexpr uint8 MaxFileNameLength = 128;
constexpr uint16 MaxFilePathLength = 256;
constexpr uint16 MaxObjectNameLength = 256;
constexpr uint16 MaxMiscStringLength = 512;
constexpr uint32 InvalidCRC32 = 0xFFFFffff;
constexpr float FloatEpsilon = 1.0e-3f;

constexpr char const* g_szImplCVarName = "s_ImplName";
constexpr char const* g_szListenersCVarName = "s_Listeners";
constexpr char const* g_szDefaultListenerName = "Default Listener";

constexpr char const* g_szLoseFocusTriggerName = "lose_focus";
constexpr char const* g_szGetFocusTriggerName = "get_focus";
constexpr char const* g_szMuteAllTriggerName = "mute_all";
constexpr char const* g_szUnmuteAllTriggerName = "unmute_all";
constexpr char const* g_szPauseAllTriggerName = "pause_all";
constexpr char const* g_szResumeAllTriggerName = "resume_all";
constexpr char const* g_szGlobalPreloadRequestName = "global_audio_system_preload";

constexpr char const* g_szDefaultLibraryName = "default_controls";
constexpr char const* g_szRootNodeTag = "AudioSystemData";
constexpr char const* g_szImplDataNodeTag = "ImplData";
constexpr char const* g_szEditorDataTag = "EditorData";
constexpr char const* g_szTriggersNodeTag = "Triggers";
constexpr char const* g_szParametersNodeTag = "Parameters";
constexpr char const* g_szSwitchesNodeTag = "Switches";
constexpr char const* g_szPreloadsNodeTag = "Preloads";
constexpr char const* g_szEnvironmentsNodeTag = "Environments";
constexpr char const* g_szSettingsNodeTag = "Settings";

constexpr char const* g_szTriggerTag = "Trigger";
constexpr char const* g_szParameterTag = "Parameter";
constexpr char const* g_szSwitchTag = "Switch";
constexpr char const* g_szStateTag = "State";
constexpr char const* g_szEnvironmentTag = "Environment";
constexpr char const* g_szPreloadRequestTag = "PreloadRequest";
constexpr char const* g_szSettingTag = "Setting";
constexpr char const* g_szPlatformTag = "Platform";

constexpr char const* g_szNameAttribute = "name";
constexpr char const* g_szVersionAttribute = "version";
constexpr char const* g_szNumTriggersAttribute = "triggers";
constexpr char const* g_szNumParametersAttribute = "parameters";
constexpr char const* g_szNumSwitchesAttribute = "switches";
constexpr char const* g_szNumStatesAttribute = "states";
constexpr char const* g_szNumEnvironmentsAttribute = "environments";
constexpr char const* g_szNumPreloadsAttribute = "preloads";
constexpr char const* g_szNumSettingsAttribute = "settings";
constexpr char const* g_szNumFilesAttribute = "files";
constexpr char const* g_szTypeAttribute = "type";

constexpr char const* g_szDataLoadType = "autoload";

constexpr char const* g_szConfigFolderName = "ace";
constexpr char const* g_szAssetsFolderName = "assets";
constexpr char const* g_szContextsFolderName = "contexts";

constexpr char const* g_szGlobalContextName = "global";

constexpr char const* g_szSwitchStateSeparator = "/";

/**
 * A utility function to convert a string value to an Id.
 * @param szSource - string to convert
 * @return a 32bit CRC computed on the lower case version of the passed string
 */
constexpr IdType StringToId(char const* const szSource)
{
	return static_cast<IdType>(CCrc32::ComputeLowercase_CompileTime(szSource));
}

constexpr PreloadRequestId GlobalPreloadRequestId = StringToId(g_szGlobalPreloadRequestName);
constexpr LibraryId DefaultLibraryId = StringToId(g_szDefaultLibraryName);
constexpr ContextId GlobalContextId = StringToId(g_szGlobalContextName);
constexpr ListenerId DefaultListenerId = StringToId("ThisIsTheHopefullyUniqueIdForTheDefaultListener");

static ListenerIds const g_defaultListenerIds{ DefaultListenerId };

/**
 * @enum CryAudio::ERequestFlags
 * @brief A strongly typed enum class representing flags that can be passed into methods via the SRequestUserData parameter that control how an internally generated request behaves or what to do along with it.
 * @var CryAudio::ERequestFlags::None
 * @var CryAudio::ERequestFlags::ExecuteBlocking
 * @var CryAudio::ERequestFlags::CallbackOnExternalOrCallingThread
 * @var CryAudio::ERequestFlags::CallbackOnAudioThread
 * @var CryAudio::ERequestFlags::DoneCallbackOnExternalThread
 * @var CryAudio::ERequestFlags::DoneCallbackOnAudioThread
 */
enum class ERequestFlags : EnumFlagsType
{
	None,                                       /**< Used to initialize variables of this type. */
	ExecuteBlocking                   = BIT(0), /**< Blocks the calling thread until the request has been processed. */
	CallbackOnExternalOrCallingThread = BIT(1), /**< Invokes a callback on the calling thread for blocking requests or invokes a callback on the external thread for non-blocking requests. */
	CallbackOnAudioThread             = BIT(2), /**< Invokes a callback on the audio thread informing of the outcome of the request. */
	DoneCallbackOnExternalThread      = BIT(3), /**< Invokes a callback on the external thread once a trigger instance finished playback of all its events. */
	DoneCallbackOnAudioThread         = BIT(4), /**< Invokes a callback on the audio thread once a trigger instance finished playback of all its events. */
};
CRY_CREATE_ENUM_FLAG_OPERATORS(ERequestFlags);

/**
 * @enum CryAudio::ERequestStatus
 * @brief A strongly typed enum class representing a list of possible statuses of an internally generated audio request. Used as a return type for many methods used by the AudioSystem internally and also for most of the CryAudio::Impl::IImpl calls.
 * @var CryAudio::ERequestStatus::None
 * @var CryAudio::ERequestStatus::ExecuteBlocking
 * @var CryAudio::ERequestStatus::CallbackOnExternalOrCallingThread
 * @var CryAudio::ERequestStatus::CallbackOnAudioThread
 * @var CryAudio::ERequestStatus::DoneCallbackOnExternalThread
 * @var CryAudio::ERequestStatus::DoneCallbackOnAudioThread
 */
enum class ERequestStatus : EnumFlagsType
{
	None,                    /**< Used to initialize variables of this type and to determine whether the variable was properly handled. */
	Success,                 /**< Returned if the request processed successfully. */
	SuccessNeedsRefresh,     /**< Audio middleware implementations return this if after an action they require to be refreshed. */
	PartialSuccess,          /**< Returned if the outcome of the request wasn't a complete success but also not complete failure. */
	Failure,                 /**< Returned if the request failed to process. */
	Pending,                 /**< Returned if the request was delivered but final execution is pending. It's then kept in the system until its status changed. */
};

/**
 * @enum CryAudio::ERequestResult
 * @brief A strongly typed enum class representing a list of possible outcomes of a request which gets communicated via the callbacks if the user decided to be informed of the outcome of a particular request.
 * @var CryAudio::ERequestResult::None
 * @var CryAudio::ERequestResult::Success
 * @var CryAudio::ERequestResult::Failure
 */
enum class ERequestResult : EnumFlagsType
{
	None,    /**< Used to initialize variables of this type and to determine whether the variable was properly handled. */
	Success, /**< Set if the request processed successfully. */
	Failure, /**< Set if the request failed to process. */
};

/**
 * @enum CryAudio::ETriggerResult
 * @brief A strongly typed enum class representing a list of possible outcomes of a trigger connection being executed.
 * @var CryAudio::ETriggerResult::Playing
 * @var CryAudio::ETriggerResult::Virtual
 * @var CryAudio::ETriggerResult::Pending
 * @var CryAudio::ETriggerResult::DoNotTrack
 * @var CryAudio::ETriggerResult::Failure
 */
enum class ETriggerResult : EnumFlagsType
{
	Playing,    /**< Returned if the trigger connection sucessfully started and is playing. */
	Virtual,    /**< Returned if the trigger connection sucessfully started and is virtual. */
	Pending,    /**< Returned if the trigger connection is pending. */
	DoNotTrack, /**< Returned if the trigger connection should not get tracked from the audio system. */
	Failure,    /**< Returned if the trigger connection fails to start. */
};

/**
 * @enum CryAudio::EOcclusionType
 * @brief A strongly typed enum class representing different audio occlusion types that can be set on audio objects.
 * @var CryAudio::EOcclusionType::None
 * @var CryAudio::EOcclusionType::Ignore
 * @var CryAudio::EOcclusionType::Adaptive
 * @var CryAudio::EOcclusionType::Low
 * @var CryAudio::EOcclusionType::Medium
 * @var CryAudio::EOcclusionType::High
 * @var CryAudio::EOcclusionType::Count
 */
enum class EOcclusionType : EnumFlagsType
{
	None,     /**< Used to initialize variables of this type and to determine whether the variable was properly handled. */
	Ignore,   /**< The audio object does not calculate occlusion against level geometry. */
	Adaptive, /**< The audio object switches between occlusion types depending on its distance to the audio listener. */
	Low,      /**< The audio object uses a coarse grained occlusion plane for calculation. */
	Medium,   /**< The audio object uses a medium grained occlusion plane for calculation. */
	High,     /**< The audio object uses a fine grained occlusion plane for calculation. */
	Count,    /**< Used to initialize arrays to this size. */
};

class CTransformation
{
public:

	CTransformation()
		: m_position(0.0f, 0.0f, 0.0f)
		, m_forward(0.0f, 1.0f, 0.0f)
		, m_up(0.0f, 0.0f, 1.0f)
	{}

	CTransformation(Vec3 const& position)
		: m_position(position)
		, m_forward(0.0f, 1.0f, 0.0f)
		, m_up(0.0f, 0.0f, 1.0f)
	{}

	CTransformation(Matrix34 const& transformation)
		: m_position(transformation.GetColumn3())
		, m_forward(transformation.GetColumn1()) //!< Assuming forward vector = (0,1,0), also assuming unscaled.
		, m_up(transformation.GetColumn2())      //!< Assuming up vector = (0,0,1).
	{
		m_forward.NormalizeFast();
		m_up.NormalizeFast();
	}

	bool IsEquivalent(CTransformation const& transformation, float const epsilon = VEC_EPSILON) const
	{
		return m_position.IsEquivalent(transformation.GetPosition(), epsilon) &&
		       m_forward.IsEquivalent(transformation.GetForward(), epsilon) &&
		       m_up.IsEquivalent(transformation.GetUp(), epsilon);
	}

	bool IsEquivalent(Matrix34 const& transformation, float const epsilon = VEC_EPSILON) const
	{
		return m_position.IsEquivalent(transformation.GetColumn3(), epsilon) &&
		       m_forward.IsEquivalent(transformation.GetColumn1(), epsilon) &&
		       m_up.IsEquivalent(transformation.GetColumn2(), epsilon);
	}

	ILINE Vec3 const&             GetPosition() const { return m_position; }
	ILINE Vec3 const&             GetForward() const  { return m_forward; }
	ILINE Vec3 const&             GetUp() const       { return m_up; }

	static CTransformation const& GetEmptyObject()    { static CTransformation const emptyInstance; return emptyInstance; }

private:

	Vec3 m_position;
	Vec3 m_forward;
	Vec3 m_up;
};

struct SRequestUserData
{
	explicit SRequestUserData(
		ERequestFlags const flags_ = ERequestFlags::None,
		void* const pOwner_ = nullptr,
		void* const pUserData_ = nullptr,
		void* const pUserDataOwner_ = nullptr)
		: flags(flags_)
		, pOwner(pOwner_)
		, pUserData(pUserData_)
		, pUserDataOwner(pUserDataOwner_)
	{}

	static SRequestUserData const& GetEmptyObject() { static SRequestUserData const emptyInstance; return emptyInstance; }

	ERequestFlags const            flags;
	void* const                    pOwner;
	void* const                    pUserData;
	void* const                    pUserDataOwner;
};

struct STriggerData
{
	STriggerData() = default;
	STriggerData(STriggerData const&) = delete;
	STriggerData(STriggerData&&) = delete;
	STriggerData& operator=(STriggerData const&) = delete;
	STriggerData& operator=(STriggerData&&) = delete;

	float radius = 0.0f;
};

struct SImplInfo
{
	CryFixedStringT<MaxInfoStringLength> name;
	CryFixedStringT<MaxInfoStringLength> folderName;
};
} // namespace CryAudio
