// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#if defined(CRY_AUDIO_USE_OCCLUSION)
	#include "RayInfo.h"
	#include "Common/PoolObject.h"
	#include <CryAudio/IAudioInterfacesCommonData.h>

	#if defined(CRY_AUDIO_USE_DEBUG_CODE)
		#include <CryMath/Cry_Color.h>
	#endif // CRY_AUDIO_USE_DEBUG_CODE

namespace CryAudio
{
class CListener;

constexpr uint8 g_numberLow = 7;
constexpr uint8 g_numberMedium = 9;
constexpr uint8 g_numberHigh = 11;
constexpr uint8 g_numRaySamplePositionsLow = g_numberLow * g_numberLow;
constexpr uint8 g_numRaySamplePositionsMedium = g_numberMedium * g_numberMedium;
constexpr uint8 g_numRaySamplePositionsHigh = g_numberHigh * g_numberHigh;
constexpr uint8 g_numConcurrentRaysLow = 1;
constexpr uint8 g_numConcurrentRaysMedium = 2;
constexpr uint8 g_numConcurrentRaysHigh = 4;
constexpr uint8 g_numInitialSamplePositions = 5;

	#if defined(CRY_AUDIO_USE_DEBUG_CODE)
struct SRayDebugInfo final
{
	SRayDebugInfo() = default;

	Vec3  begin = ZERO;
	Vec3  end = ZERO;
	float occlusionValue = 0.0f;
	float distanceToNearestObstacle = FLT_MAX;
	uint8 numHits = 0;
	uint8 samplePosIndex = 0;
};
	#endif // CRY_AUDIO_USE_DEBUG_CODE

struct SOcclusionInfo final : public CPoolObject<SOcclusionInfo, stl::PSyncNone>
{
	SOcclusionInfo() = delete;
	SOcclusionInfo(SOcclusionInfo const&) = delete;
	SOcclusionInfo(SOcclusionInfo&&) = delete;
	SOcclusionInfo& operator=(SOcclusionInfo const&) = delete;
	SOcclusionInfo& operator=(SOcclusionInfo&&) = delete;

	explicit SOcclusionInfo(CListener* const pListener_)
		: pListener(pListener_)
		, lastQuerriedOcclusion(0.0f)
		, occlusion(0.0f)
		, currentListenerDistance(0.0f)
		, rayIndex(0)
		, raysOcclusion{{ 0.0f }}
		, occlusionTypeWhenAdaptive(EOcclusionType::Low)
	#if defined(CRY_AUDIO_USE_DEBUG_CODE)
		, collisionSpherePositions{ { ZERO } }
	#endif // CRY_AUDIO_USE_DEBUG_CODE
	{}

	CListener* pListener;
	float      lastQuerriedOcclusion;
	float      occlusion;
	float      currentListenerDistance;
	uint8      rayIndex;
	std::array<float, g_numRaySamplePositionsHigh> raysOcclusion;
	std::array<CRayInfo, g_numConcurrentRaysHigh>  raysInfo;
	EOcclusionType occlusionTypeWhenAdaptive;

	#if defined(CRY_AUDIO_USE_DEBUG_CODE)
	std::array<SRayDebugInfo, g_numConcurrentRaysHigh> rayDebugInfos;
	ColorB listenerOcclusionPlaneColor;
	std::array<Vec3, g_numRaySamplePositionsHigh>      collisionSpherePositions;
	#endif // CRY_AUDIO_USE_DEBUG_CODE
};

using OcclusionInfos = std::vector<SOcclusionInfo*>;
}      // namespace CryAudio
#endif // CRY_AUDIO_USE_OCCLUSION