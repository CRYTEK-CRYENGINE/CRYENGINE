// Copyright 2001-2017 Crytek GmbH / Crytek Group. All rights reserved. 

#pragma once

#include "../CryMath/Cry_Math.h"
#include "../CryCore/BaseTypes.h"
#include "../CryCore/smartptr.h"
#include "../CryCore/Platform/platform.h"
#include "../CryCore/CryEnumMacro.h"

#define AUDIO_SYSTEM_DATA_ROOT "audio"

namespace CryAudio
{
using IdType = uint32;
using ControlId = IdType;
using SwitchStateId = IdType;
using EnvironmentId = IdType;
using PreloadRequestId = IdType;
using FileEntryId = IdType;
using TriggerImplId = IdType;
using TriggerInstanceId = IdType;
using EnumFlagsType = IdType;
using AuxObjectId = IdType;

static constexpr ControlId InvalidControlId = 0;
static constexpr SwitchStateId InvalidSwitchStateId = 0;
static constexpr EnvironmentId InvalidEnvironmentId = 0;
static constexpr PreloadRequestId InvalidPreloadRequestId = 0;
static constexpr FileEntryId InvalidFileEntryId = 0;
static constexpr TriggerImplId InvalidTriggerImplId = 0;
static constexpr TriggerInstanceId InvalidTriggerInstanceId = 0;
static constexpr EnumFlagsType InvalidEnumFlagType = 0;
static constexpr AuxObjectId InvalidAuxObjectId = 0;
static constexpr AuxObjectId DefaultAuxObjectId = 1;
static constexpr uint8 MaxControlNameLength = 128;
static constexpr uint8 MaxFileNameLength = 128;
static constexpr uint16 MaxFilePathLength = 256;
static constexpr uint16 MaxObjectNameLength = 256;
static constexpr uint16 MaxMiscStringLength = 512;
static constexpr uint32 InvalidCRC32 = 0xFFFFffff;
static constexpr float FloatEpsilon = 1.0e-3f;

// Forward declarations.
struct IObject;
class CATLEvent;
class CATLStandaloneFile;

/**
* An enum listing flags that can be passed into methods via the SRequestUserData parameter.
* These flags control how an internally generated request behaves.
*/
enum class ERequestFlags : EnumFlagsType
{
	None,
	ExecuteBlocking                   = BIT(0), // Blocks the calling thread until the request has been processed.
	CallbackOnExternalOrCallingThread = BIT(1), // Blocking requests will issue a callback on the calling thread, non-blocking requests will issue a callback on the external thread.
	CallbackOnAudioThread             = BIT(2), // Issues a callback on the audio thread.
	DoneCallbackOnExternalThread      = BIT(3), // Issues a callback on the external thread once a trigger instance finished playback of all its events.
	DoneCallbackOnAudioThread         = BIT(4), // Issues a callback on the audio thread once a trigger instance finished playback of all its events.
};
CRY_CREATE_ENUM_FLAG_OPERATORS(ERequestFlags);

/**
 * An enum that lists possible statuses of an internally generated audio request.
 * Used as a return type for many methods used by the AudioSystem internally,
 * and also for most of the CryAudio::Impl::IImpl calls.
 */
enum class ERequestStatus : EnumFlagsType
{
	None,
	Success,
	SuccessNeedsRefresh,
	PartialSuccess,
	Failure,
	Pending,
	FailureInvalidObjectId,
	FailureInvalidControlId,
	FailureInvalidRequest,
};

enum class ERequestResult : EnumFlagsType
{
	None,
	Success,
	Failure,
};

class CObjectTransformation
{
public:

	CObjectTransformation()
		: m_position(ZERO)
		, m_forward(Vec3Constants<float>::fVec3_OneY)
		, m_up(Vec3Constants<float>::fVec3_OneZ)
	{}

	CObjectTransformation(Vec3 const& position)
		: m_position(position)
		, m_forward(Vec3Constants<float>::fVec3_OneY)
		, m_up(Vec3Constants<float>::fVec3_OneZ)
	{}

	CObjectTransformation(Matrix34 const& transformation)
		: m_position(transformation.GetColumn3())
		, m_forward(transformation.GetColumn1())   //!< Assuming forward vector = (0,1,0), also assuming unscaled.
		, m_up(transformation.GetColumn2())        //!< Assuming up vector = (0,0,1).
	{
		m_forward.NormalizeFast();
		m_up.NormalizeFast();
	}

	static CObjectTransformation const& GetEmptyObject() { static CObjectTransformation const emptyInstance; return emptyInstance; }

	ILINE Vec3 const& GetPosition() const { return m_position; }
	ILINE Vec3 const& GetForward() const  { return m_forward; }
	ILINE Vec3 const& GetUp() const       { return m_up; }

	bool              IsEquivalent(CObjectTransformation const& transformation, float const epsilon = VEC_EPSILON) const
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

struct SFileData
{
	SFileData() = default;
	SFileData(SFileData const&) = delete;
	SFileData(SFileData&&) = delete;
	SFileData& operator=(SFileData const&) = delete;
	SFileData& operator=(SFileData&&) = delete;

	float duration = 0.0f;
};

struct STriggerData
{
	STriggerData() = default;
	STriggerData(STriggerData const&) = delete;
	STriggerData(STriggerData&&) = delete;
	STriggerData& operator=(STriggerData const&) = delete;
	STriggerData& operator=(STriggerData&&) = delete;

	float radius = 0.0f;
	float occlusionFadeOutDistance = 0.0f;
};

struct SPlayFileInfo
{
	explicit SPlayFileInfo(
	  char const* const _szFile,
	  bool const _bLocalized = true,
	  ControlId const _usedPlaybackTrigger = InvalidControlId)
		: szFile(_szFile)
		, bLocalized(_bLocalized)
		, usedTriggerForPlayback(_usedPlaybackTrigger)
	{}

	char const* const szFile;
	bool const        bLocalized;
	ControlId const   usedTriggerForPlayback;
};
} // namespace CryAudio
