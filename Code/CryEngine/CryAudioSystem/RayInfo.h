// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#if defined(CRY_AUDIO_USE_OCCLUSION)
	#include <CryPhysics/IPhysics.h>

namespace CryAudio
{
class CObject;

constexpr uint8 g_maxRayHits = 30;

struct SRayHitInfo
{
	SRayHitInfo() = default;

	float distance = 0.0f;
	short surfaceIndex = 0;
};

class CRayInfo
{
public:

	CRayInfo() = default;

	void Reset();

	CObject*    pObject = nullptr;
	uint8       samplePosIndex = 0;
	uint8       numHits = 0;
	float       totalSoundOcclusion = 0.0f;
	SRayHitInfo hits[g_maxRayHits];

	#if defined(CRY_AUDIO_USE_DEBUG_CODE)
	Vec3  startPosition = ZERO;
	Vec3  direction = ZERO;
	float distanceToFirstObstacle = FLT_MAX;
	#endif // CRY_AUDIO_USE_DEBUG_CODE
};
}      // namespace CryAudio
#endif // CRY_AUDIO_USE_OCCLUSION