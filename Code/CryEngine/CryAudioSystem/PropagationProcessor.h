// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#if defined(CRY_AUDIO_USE_OCCLUSION)
	#include "OcclusionInfo.h"
	#include <CryAudio/IAudioInterfacesCommonData.h>
	#include <CryPhysics/IPhysics.h>

namespace CryAudio
{
class CPropagationProcessor final
{
public:

	CPropagationProcessor() = delete;
	CPropagationProcessor(CPropagationProcessor const&) = delete;
	CPropagationProcessor(CPropagationProcessor&&) = delete;
	CPropagationProcessor& operator=(CPropagationProcessor const&) = delete;
	CPropagationProcessor& operator=(CPropagationProcessor&&) = delete;

	static bool s_bCanIssueRWIs;

	CPropagationProcessor(CObject& object);
	~CPropagationProcessor() = default;

	void Init();
	void Release();

	// PhysicsSystem callback
	static int  OnObstructionTest(EventPhys const* pEvent);
	static void UpdateOcclusionRayFlags();
	static void UpdateOcclusionPlanes();

	void        Update();
	void        SetOcclusionType(EOcclusionType const occlusionType);
	float       GetOcclusion(CListener* const pListener) const;
	void        ProcessPhysicsRay(CRayInfo& rayInfo);
	void        ReleasePendingRays();
	bool        HasPendingRays() const { return m_remainingRays > 0; }
	bool        HasNewOcclusionValues();
	void        UpdateOcclusion();
	void        SetOcclusionRayOffset(float const offset) { m_occlusionRayOffset = std::max(0.0f, offset); }
	void        AddListener(CListener* const pListener);
	void        RemoveListener(CListener* const pListener);

private:

	void ProcessObstructionOcclusion();
	void CastObstructionRay(
		Vec3 const& origin,
		uint8 const rayIndex,
		uint8 const samplePosIndex,
		bool const bSynch,
		SOcclusionInfo* const pInfo);
	void  RunObstructionQuery();
	void  ProcessLow(Vec3 const& up, Vec3 const& side, bool const bSynch, SOcclusionInfo* const pInfo);
	void  ProcessMedium(Vec3 const& up, Vec3 const& side, bool const bSynch, SOcclusionInfo* const pInfo);
	void  ProcessHigh(Vec3 const& up, Vec3 const& side, bool const bSynch, SOcclusionInfo* const pInfo);
	uint8 GetNumConcurrentRays(EOcclusionType const occlusionTypeWhenAdaptive) const;
	uint8 GetNumSamplePositions(EOcclusionType const occlusionTypeWhenAdaptive) const;
	bool  CanRunOcclusion();
	float CastInitialRay(Vec3 const& origin, Vec3 const& target, bool const accumulate);

	float          m_occlusionRayOffset;

	uint8          m_remainingRays;

	CObject&       m_object;

	EOcclusionType m_occlusionType;
	EOcclusionType m_originalOcclusionType;

	OcclusionInfos m_occlusionInfos;

	static int     s_occlusionRayFlags;

	#if defined(CRY_AUDIO_USE_DEBUG_CODE)
public:

	static uint16 s_totalSyncPhysRays;
	static uint16 s_totalAsyncPhysRays;

	void                  DrawDebugInfo(IRenderAuxGeom& auxGeom);
	void                  DrawListenerPlane(IRenderAuxGeom& auxGeom);
	EOcclusionType        GetOcclusionType() const      { return m_occlusionType; }
	OcclusionInfos const& GetOcclusionInfos() const     { return m_occlusionInfos; }
	float                 GetOcclusionRayOffset() const { return m_occlusionRayOffset; }
	void                  ResetRayData();
	#endif // CRY_AUDIO_USE_DEBUG_CODE
};
}      // namespace CryAudio
#endif // CRY_AUDIO_USE_OCCLUSION