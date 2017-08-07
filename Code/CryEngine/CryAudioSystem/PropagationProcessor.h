// Copyright 2001-2016 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include <CryPhysics/IPhysics.h>
#include <CryAudio/IAudioSystem.h>
#include "ATLEntities.h"

namespace CryAudio
{
class CATLAudioObject;
struct SATLSoundPropagationData;

static const size_t s_maxRayHits = 5;

class CAudioRayInfo
{
public:

	CAudioRayInfo(CATLAudioObject* pObject_)
		: pObject(pObject_)
		, samplePosIndex(0)
		, numHits(0)
		, totalSoundOcclusion(0.0f)
#if defined(INCLUDE_AUDIO_PRODUCTION_CODE)
		, startPosition(ZERO)
		, direction(ZERO)
		, distanceToFirstObstacle(FLT_MAX)
#endif // INCLUDE_AUDIO_PRODUCTION_CODE
	{}

	~CAudioRayInfo() = default;

	void Reset();

	CATLAudioObject* pObject;
	size_t           samplePosIndex;
	size_t           numHits;
	float            totalSoundOcclusion;
	ray_hit          hits[s_maxRayHits];

#if defined(INCLUDE_AUDIO_PRODUCTION_CODE)
	Vec3  startPosition;
	Vec3  direction;
	float distanceToFirstObstacle;
#endif // INCLUDE_AUDIO_PRODUCTION_CODE
};

class CPropagationProcessor
{
public:

	static bool s_bCanIssueRWIs;

	typedef std::vector<CAudioRayInfo> RayInfoVec;
	typedef std::vector<float>         RayOcclusionVec;

	CPropagationProcessor(CObjectTransformation const& transformation);
	~CPropagationProcessor();

	void Init(CATLAudioObject* const pAudioObject, Vec3 const& audioListenerPosition);

	// PhysicsSystem callback
	static int OnObstructionTest(EventPhys const* pEvent);

	void       Update(
		float const deltaTime,
		float const distance,
		Vec3 const& audioListenerPosition,
		EObjectFlags const objectFlags);
	void       SetOcclusionType(EOcclusionType const occlusionType, Vec3 const& audioListenerPosition);
	bool       CanRunObstructionOcclusion() const;
	void       GetPropagationData(SATLSoundPropagationData& propagationData) const;
	void       ProcessPhysicsRay(CAudioRayInfo* const pAudioRayInfo);
	void       ReleasePendingRays();
	bool       HasPendingRays() const { return m_remainingRays > 0; }
	bool       HasNewOcclusionValues();
	void       SetOcclusionMultiplier(float const occlusionFadeOut);

private:

	void ProcessObstructionOcclusion();
	void CastObstructionRay(
	  Vec3 const& origin,
	  size_t const rayIndex,
	  size_t const samplePosIndex,
	  bool const bSynch);
	void RunObstructionQuery(Vec3 const& audioListenerPosition);
	void ProcessLow(
	  Vec3 const& audioListenerPosition,
	  Vec3 const& up,
	  Vec3 const& side,
	  bool const bSynch);
	void ProcessMedium(
	  Vec3 const& audioListenerPosition,
	  Vec3 const& up,
	  Vec3 const& side,
	  bool const bSynch);
	void ProcessHigh(
	  Vec3 const& audioListenerPosition,
	  Vec3 const& up,
	  Vec3 const& side,
	  bool const bSynch);
	size_t GetNumConcurrentRays() const;
	size_t GetNumSamplePositions() const;

	float                        m_obstruction;
	float                        m_lastQuerriedObstruction;
	float                        m_lastQuerriedOcclusion;
	float                        m_occlusion;
	float                        m_occlusionMultiplier;
	float                        m_currentListenerDistance;
	RayOcclusionVec              m_raysOcclusion;

	size_t                       m_remainingRays;
	size_t                       m_rayIndex;

	CObjectTransformation const& m_transformation;

	RayInfoVec                   m_raysInfo;
	EOcclusionType               m_occlusionType;
	EOcclusionType               m_originalOcclusionType;
	EOcclusionType               m_occlusionTypeWhenAdaptive;

#if defined(INCLUDE_AUDIO_PRODUCTION_CODE)
public:

	static size_t s_totalSyncPhysRays;
	static size_t s_totalAsyncPhysRays;

	void           DrawObstructionRays(IRenderAuxGeom& auxGeom, EObjectFlags const objectFlags) const;
	void           DrawRay(IRenderAuxGeom& auxGeom, size_t const rayIndex) const;
	EOcclusionType GetOcclusionType() const             { return m_occlusionType; }
	EOcclusionType GetOcclusionTypeWhenAdaptive() const { return m_occlusionTypeWhenAdaptive; }
	void           ResetRayData();

private:

	struct SRayDebugInfo
	{
		SRayDebugInfo()
			: begin(ZERO)
			, end(ZERO)
			, stableEnd(ZERO)
			, occlusionValue(0.0f)
			, distanceToNearestObstacle(FLT_MAX)
			, numHits(0)
		{}

		~SRayDebugInfo() {}

		Vec3  begin;
		Vec3  end;
		Vec3  stableEnd;
		float occlusionValue;
		float distanceToNearestObstacle;
		int   numHits;
	};

	typedef std::vector<SRayDebugInfo> RayDebugInfoVec;

	RayDebugInfoVec m_rayDebugInfos;
	mutable float   m_timeSinceLastUpdateMS;
#endif // INCLUDE_AUDIO_PRODUCTION_CODE
};
} // namespace CryAudio
