// Copyright 2001-2019 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include "AK/AkWwiseSDKVersion.h"

#if (AK_WWISESDK_VERSION_MAJOR != 2019) || (AK_WWISESDK_VERSION_MINOR != 1) || (AK_WWISESDK_VERSION_SUBMINOR > 7)
	#error This version of Wwise is not supported. The supported versions are 2019.1.0 to 2019.1.7
#endif

#include "AK/SoundEngine/Common/AkTypes.h"
#include <CryAudio/IAudioSystem.h>

#define CRY_AUDIO_IMPL_WWISE_INFO_STRING "Wwise " AK_WWISESDK_VERSIONNAME

#define CRY_AUDIO_IMPL_WWISE_ASSERT_OK(x) (CRY_ASSERT(x == AK_Success))
#define CRY_AUDIO_IMPL_WWISE_IS_OK(x)     (x == AK_Success)

#if defined(CRY_AUDIO_IMPL_WWISE_USE_DEBUG_CODE)
	#include <CryThreading/CryThread.h>
#endif // CRY_AUDIO_IMPL_WWISE_USE_DEBUG_CODE

namespace CryAudio
{
namespace Impl
{
namespace Wwise
{
class CImpl;
class CListener;
class CEventInstance;

extern CImpl* g_pImpl;

extern uint32 g_numObjectsWithRelativeVelocity;

using EventInstances = std::vector<CEventInstance*>;
using Listeners = std::vector<CListener*>;

//////////////////////////////////////////////////////////////////////////
inline void FillAKVector(Vec3 const& vCryVector, AkVector& vAKVector)
{
	vAKVector.X = vCryVector.x;
	vAKVector.Y = vCryVector.z;
	vAKVector.Z = vCryVector.y;
}

///////////////////////////////////////////////////////////////////////////
inline void FillAKObjectPosition(CTransformation const& transformation, AkSoundPosition& outTransformation)
{
	AkVector vec1, vec2;
	FillAKVector(transformation.GetPosition(), vec1);
	outTransformation.SetPosition(vec1);
	FillAKVector(transformation.GetForward(), vec1);
	FillAKVector(transformation.GetUp(), vec2);
	outTransformation.SetOrientation(vec1, vec2);
}

///////////////////////////////////////////////////////////////////////////
inline void FillAKListenerPosition(CTransformation const& transformation, AkListenerPosition& outTransformation)
{
	AkVector vec1, vec2;
	FillAKVector(transformation.GetPosition(), vec1);
	outTransformation.SetPosition(vec1);
	FillAKVector(transformation.GetForward(), vec1);
	FillAKVector(transformation.GetUp(), vec2);
	outTransformation.SetOrientation(vec1, vec2);
}

#if defined(CRY_AUDIO_IMPL_WWISE_USE_DEBUG_CODE)
class CEventInstance;
class CObject;

extern CryCriticalSection g_cs;
extern std::unordered_map<AkPlayingID, CEventInstance*> g_playingIds;
extern std::unordered_map<AkGameObjectID, CObject*> g_gameObjectIds;

using States = std::map<CryFixedStringT<MaxControlNameLength>, CryFixedStringT<MaxControlNameLength>>;
extern States g_debugStates;

enum class EDebugListFilter : EnumFlagsType
{
	None           = 0,
	EventInstances = BIT(6), // a
	States         = BIT(7), // b
};
CRY_CREATE_ENUM_FLAG_OPERATORS(EDebugListFilter);

constexpr EDebugListFilter g_debugListMask = EDebugListFilter::EventInstances | EDebugListFilter::States;
#endif // CRY_AUDIO_IMPL_WWISE_USE_DEBUG_CODE
}      // namespace Wwise
}      // namespace Impl
}      // namespace CryAudio
