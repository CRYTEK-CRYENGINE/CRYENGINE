// Copyright 2001-2016 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include "AK/SoundEngine/Common/AkTypes.h"
#include "AK/AkWwiseSDKVersion.h"
#include <CryAudio/IAudioSystem.h>

#define WWISE_IMPL_DATA_ROOT   AUDIO_SYSTEM_DATA_ROOT CRY_NATIVE_PATH_SEPSTR "wwise"
#define WWISE_IMPL_INFO_STRING "Wwise " AK_WWISESDK_VERSIONNAME

#define ASSERT_WWISE_OK(x) (CRY_ASSERT(x == AK_Success))
#define IS_WWISE_OK(x)     (x == AK_Success)

namespace CryAudio
{
namespace Impl
{
namespace Wwise
{
// several Wwise-specific helper functions
//////////////////////////////////////////////////////////////////////////
inline void FillAKVector(Vec3 const& vCryVector, AkVector& vAKVector)
{
	vAKVector.X = vCryVector.x;
	vAKVector.Y = vCryVector.z;
	vAKVector.Z = vCryVector.y;
}

///////////////////////////////////////////////////////////////////////////
inline void FillAKObjectPosition(CObjectTransformation const& transformation, AkSoundPosition& outTransformation)
{
	AkVector vec1, vec2;
	FillAKVector(transformation.GetPosition(), vec1);
	outTransformation.SetPosition(vec1);
	FillAKVector(transformation.GetForward(), vec1);
	FillAKVector(transformation.GetUp(), vec2);
	outTransformation.SetOrientation(vec1, vec2);
}

///////////////////////////////////////////////////////////////////////////
inline void FillAKListenerPosition(CObjectTransformation const& transformation, AkListenerPosition& outTransformation)
{
	AkVector vec1, vec2;
	FillAKVector(transformation.GetPosition(), vec1);
	outTransformation.SetPosition(vec1);
	FillAKVector(transformation.GetForward(), vec1);
	FillAKVector(transformation.GetUp(), vec2);
	outTransformation.SetOrientation(vec1, vec2);
}
} // namespace Wwise
} // namespace Impl
} // namespace CryAudio
