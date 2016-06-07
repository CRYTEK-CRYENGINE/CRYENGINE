// Copyright 2001-2016 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include "AK/SoundEngine/Common/AkTypes.h"
#include "AK/AkWwiseSDKVersion.h"
#include <CryAudio/IAudioSystem.h>

#define WWISE_IMPL_DATA_ROOT   AUDIO_SYSTEM_DATA_ROOT CRY_NATIVE_PATH_SEPSTR "wwise"
#define WWISE_IMPL_INFO_STRING "Wwise " AK_WWISESDK_VERSIONNAME

#define ASSERT_WWISE_OK(x) (CRY_ASSERT(x == AK_Success))
#define IS_WWISE_OK(x)     (x == AK_Success)

// several Wwise-specific helper functions
//////////////////////////////////////////////////////////////////////////
inline void FillAKVector(Vec3 const& vCryVector, AkVector& vAKVector)
{
	vAKVector.X = vCryVector.x;
	vAKVector.Y = vCryVector.z;
	vAKVector.Z = vCryVector.y;
}

///////////////////////////////////////////////////////////////////////////
inline void FillAKObjectPosition(CAudioObjectTransformation const& transformation, AkSoundPosition& outTransformation)
{
	FillAKVector(transformation.GetPosition(), outTransformation.Position);
	FillAKVector(transformation.GetForward(), outTransformation.Orientation);
}

///////////////////////////////////////////////////////////////////////////
inline void FillAKListenerPosition(CAudioObjectTransformation const& transformation, AkListenerPosition& outTransformation)
{
	FillAKVector(transformation.GetPosition(), outTransformation.Position);
	FillAKVector(transformation.GetForward(), outTransformation.OrientationFront);
	FillAKVector(transformation.GetUp(), outTransformation.OrientationTop);
}
