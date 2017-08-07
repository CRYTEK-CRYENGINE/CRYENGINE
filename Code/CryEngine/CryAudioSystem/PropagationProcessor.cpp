// Copyright 2001-2016 Crytek GmbH / Crytek Group. All rights reserved.

#include "stdafx.h"
#include "PropagationProcessor.h"
#include "AudioCVars.h"
#include "ATLAudioObject.h"
#include "AudioSystem.h"
#include <Cry3DEngine/I3DEngine.h>
#include <Cry3DEngine/ISurfaceType.h>

#if defined(INCLUDE_AUDIO_PRODUCTION_CODE)
	#include <CryRenderer/IRenderAuxGeom.h>
#endif // INCLUDE_AUDIO_PRODUCTION_CODE

namespace CryAudio
{
static size_t s_numRaySamplePositionsLow = 0;
static size_t s_numRaySamplePositionsMedium = 0;
static size_t s_numRaySamplePositionsHigh = 0;
static size_t const s_numConcurrentRaysLow = 1;
static size_t const s_numConcurrentRaysMedium = 2;
static size_t const s_numConcurrentRaysHigh = 4;
static float const s_listenerHeadSize = 0.15f; // Slightly bigger than the average size of a human head (15 cm)

struct SAudioRayOffset
{
	SAudioRayOffset()
		: x(0.0f)
		, z(0.0f)
	{}

	float x;
	float z;
};

using RaySamplePositions = std::vector<SAudioRayOffset>;
static RaySamplePositions s_raySamplePositionsLow;
static RaySamplePositions s_raySamplePositionsMedium;
static RaySamplePositions s_raySamplePositionsHigh;

///////////////////////////////////////////////////////////////////////////
void CAudioRayInfo::Reset()
{
	totalSoundOcclusion = 0.0f;
	numHits = 0;
#if defined(INCLUDE_AUDIO_PRODUCTION_CODE)
	startPosition.zero();
	direction.zero();
	distanceToFirstObstacle = FLT_MAX;
#endif // INCLUDE_AUDIO_PRODUCTION_CODE
}

bool CPropagationProcessor::s_bCanIssueRWIs = false;

///////////////////////////////////////////////////////////////////////////
int CPropagationProcessor::OnObstructionTest(EventPhys const* pEvent)
{
	EventPhysRWIResult const* const pRWIResult = static_cast<EventPhysRWIResult const* const>(pEvent);

	if (pRWIResult->iForeignData == PHYS_FOREIGN_ID_SOUND_OBSTRUCTION)
	{
		CAudioRayInfo* const pRayInfo = static_cast<CAudioRayInfo* const>(pRWIResult->pForeignData);

		if (pRayInfo != nullptr)
		{
			pRayInfo->numHits = min(static_cast<size_t>(pRWIResult->nHits) + 1, s_maxRayHits);
			SAudioObjectRequestData<EAudioObjectRequestType::ProcessPhysicsRay> requestData(pRayInfo);
			CAudioRequest request(&requestData);
			request.pObject = pRayInfo->pObject;
			CATLAudioObject::s_pAudioSystem->PushRequest(request);
		}
		else
		{
			CRY_ASSERT(false);
		}
	}
	else
	{
		CRY_ASSERT(false);
	}

	return 1;
}

///////////////////////////////////////////////////////////////////////////
CPropagationProcessor::CPropagationProcessor(CObjectTransformation const& transformation)
	: m_obstruction(0.0f)
	, m_occlusion(0.0f)
	, m_occlusionMultiplier(1.0f)
	, m_remainingRays(0)
	, m_rayIndex(0)
	, m_transformation(transformation)
	, m_currentListenerDistance(0.0f)
	, m_occlusionType(EOcclusionType::None)
	, m_originalOcclusionType(EOcclusionType::None)
	, m_occlusionTypeWhenAdaptive(EOcclusionType::Low) //will be updated in the first Update
#if defined(INCLUDE_AUDIO_PRODUCTION_CODE)
	, m_rayDebugInfos(s_numConcurrentRaysHigh)
	, m_timeSinceLastUpdateMS(0.0f)
#endif // INCLUDE_AUDIO_PRODUCTION_CODE
{
	if (s_raySamplePositionsLow.empty())
	{
		float const listenerHeadSizeHalf = s_listenerHeadSize * 0.5f;
		SAudioRayOffset temp;
		size_t const number = 3;
		s_raySamplePositionsLow.reserve(number * number);
		float const step = s_listenerHeadSize / (number - 1);

		for (size_t i = 0; i < number; ++i)
		{
			temp.z = listenerHeadSizeHalf - i * step;

			for (size_t j = 0; j < number; ++j)
			{
				temp.x = -listenerHeadSizeHalf + j * step;
				s_raySamplePositionsLow.push_back(temp);
			}
		}

		s_numRaySamplePositionsLow = s_raySamplePositionsLow.size();
	}

	if (s_raySamplePositionsMedium.empty())
	{
		float const listenerHeadSizeHalf = s_listenerHeadSize * 0.5f;
		SAudioRayOffset temp;
		size_t const number = 5;
		s_raySamplePositionsMedium.reserve(number * number);
		float const step = s_listenerHeadSize / (number - 1);

		for (size_t i = 0; i < number; ++i)
		{
			temp.z = listenerHeadSizeHalf - i * step;

			for (size_t j = 0; j < number; ++j)
			{
				temp.x = -listenerHeadSizeHalf + j * step;
				s_raySamplePositionsMedium.push_back(temp);
			}
		}

		s_numRaySamplePositionsMedium = s_raySamplePositionsMedium.size();
	}

	if (s_raySamplePositionsHigh.empty())
	{
		float const listenerHeadSizeHalf = s_listenerHeadSize * 0.5f;
		SAudioRayOffset temp;
		size_t const number = 7;
		s_raySamplePositionsHigh.reserve(number * number);
		float const step = s_listenerHeadSize / (number - 1);

		for (size_t i = 0; i < number; ++i)
		{
			temp.z = listenerHeadSizeHalf - i * step;

			for (size_t j = 0; j < number; ++j)
			{
				temp.x = -listenerHeadSizeHalf + j * step;
				s_raySamplePositionsHigh.push_back(temp);
			}
		}

		s_numRaySamplePositionsHigh = s_raySamplePositionsHigh.size();
	}

	m_raysOcclusion.resize(s_numRaySamplePositionsHigh, 0.0f);
	m_raysInfo.reserve(s_numConcurrentRaysHigh);
}

//////////////////////////////////////////////////////////////////////////
CPropagationProcessor::~CPropagationProcessor()
{
	stl::free_container(m_raysInfo);
}

//////////////////////////////////////////////////////////////////////////
void CPropagationProcessor::Init(CATLAudioObject* const pAudioObject, Vec3 const& audioListenerPosition)
{
	for (size_t i = 0; i < s_numConcurrentRaysHigh; ++i)
	{
		m_raysInfo.emplace_back(pAudioObject);
	}

	m_currentListenerDistance = audioListenerPosition.GetDistance(m_transformation.GetPosition());
}

///////////////////////////////////////////////////////////////////////////
void CPropagationProcessor::Update(
  float const deltaTime,
  float const distance,
  Vec3 const& audioListenerPosition,
  EObjectFlags const objectFlags)
{
#if defined(INCLUDE_AUDIO_PRODUCTION_CODE)
	if (g_cvars.m_audioObjectsRayType > 0)
	{
		m_occlusionType = clamp_tpl<EOcclusionType>(static_cast<EOcclusionType>(g_cvars.m_audioObjectsRayType), EOcclusionType::Ignore, EOcclusionType::High);
	}
	else
	{
		m_occlusionType = m_originalOcclusionType;
	}
#endif // INCLUDE_AUDIO_PRODUCTION_CODE

	m_currentListenerDistance = distance;

	if (CanRunObstructionOcclusion() && (objectFlags& EObjectFlags::Virtual) == 0)
	{
		if (m_currentListenerDistance < g_cvars.m_occlusionHighDistance)
		{
			m_occlusionTypeWhenAdaptive = EOcclusionType::High;
		}
		else if (m_currentListenerDistance < g_cvars.m_occlusionMediumDistance)
		{
			m_occlusionTypeWhenAdaptive = EOcclusionType::Medium;
		}
		else
		{
			m_occlusionTypeWhenAdaptive = EOcclusionType::Low;
		}

		RunObstructionQuery(audioListenerPosition);
	}
	else
	{
		m_obstruction = 0.0f;
		m_occlusion = 0.0f;
	}

#if defined(INCLUDE_AUDIO_PRODUCTION_CODE)
	m_timeSinceLastUpdateMS += deltaTime;
#endif // INCLUDE_AUDIO_PRODUCTION_CODE
}

//////////////////////////////////////////////////////////////////////////
void CPropagationProcessor::SetOcclusionType(EOcclusionType const occlusionType, Vec3 const& audioListenerPosition)
{
	m_occlusionType = m_originalOcclusionType = occlusionType;
	m_obstruction = 0.0f;
	m_occlusion = 0.0f;

	// First time run is synchronous and center ray only to get a quick initial value to start from.
	Vec3 const direction(m_transformation.GetPosition() - audioListenerPosition);
	m_currentListenerDistance = direction.GetLength();

	if (CanRunObstructionOcclusion())
	{
		Vec3 directionNormalized(direction / m_currentListenerDistance);
		Vec3 const finalDirection(direction - (directionNormalized * g_cvars.m_occlusionRayLengthOffset));

		CAudioRayInfo& rayInfo = m_raysInfo[0];
		static int const physicsFlags = ent_water | ent_static | ent_sleeping_rigid | ent_rigid | ent_terrain;
		rayInfo.numHits = static_cast<size_t>(gEnv->pPhysicalWorld->RayWorldIntersection(
		                                        audioListenerPosition,
		                                        finalDirection,
		                                        physicsFlags,
		                                        rwi_pierceability0,
		                                        rayInfo.hits,
		                                        static_cast<int>(s_maxRayHits),
		                                        nullptr,
		                                        0,
		                                        &rayInfo,
		                                        PHYS_FOREIGN_ID_SOUND_OBSTRUCTION));

		rayInfo.numHits = min(rayInfo.numHits + 1, s_maxRayHits);
		float totalOcclusion = 0.0f;

		if (rayInfo.numHits > 0)
		{
			ISurfaceTypeManager* const pSurfaceTypeManager = gEnv->p3DEngine->GetMaterialManager()->GetSurfaceTypeManager();
			CRY_ASSERT(rayInfo.numHits <= s_maxRayHits);

			for (size_t i = 0; i < rayInfo.numHits; ++i)
			{
				float const distance = rayInfo.hits[i].dist;

				if (distance > 0.0f)
				{
					ISurfaceType* const pMat = pSurfaceTypeManager->GetSurfaceType(rayInfo.hits[i].surface_idx);

					if (pMat != nullptr)
					{
						ISurfaceType::SPhysicalParams const& physParams = pMat->GetPhyscalParams();
						totalOcclusion += physParams.sound_obstruction;
					}
				}
			}
		}

		m_occlusion = clamp_tpl(totalOcclusion, 0.0f, 1.0f) * m_occlusionMultiplier;

		for (auto& rayOcclusion : m_raysOcclusion)
		{
			rayOcclusion = m_occlusion;
		}
	}
	else
	{
		m_lastQuerriedObstruction = 0.0f;
		m_lastQuerriedOcclusion = 0.0f;
	}
}

//////////////////////////////////////////////////////////////////////////
bool CryAudio::CPropagationProcessor::CanRunObstructionOcclusion() const
{
	return
	  m_occlusionType != EOcclusionType::None &&
	  m_occlusionType != EOcclusionType::Ignore &&
	  m_currentListenerDistance > g_cvars.m_occlusionMinDistance &&
	  m_currentListenerDistance < g_cvars.m_occlusionMaxDistance &&
	  s_bCanIssueRWIs;
}

///////////////////////////////////////////////////////////////////////////
void CPropagationProcessor::GetPropagationData(SATLSoundPropagationData& propagationData) const
{
	propagationData.obstruction = m_obstruction;
	propagationData.occlusion = m_occlusion;
}

///////////////////////////////////////////////////////////////////////////
void CPropagationProcessor::ProcessPhysicsRay(CAudioRayInfo* const pAudioRayInfo)
{
	CRY_ASSERT((0 <= pAudioRayInfo->samplePosIndex) && (pAudioRayInfo->samplePosIndex < s_numRaySamplePositionsHigh));

	float totalOcclusion = 0.0f;
	int numRealHits = 0;

#if defined(INCLUDE_AUDIO_PRODUCTION_CODE)
	float minDistance = FLT_MAX;
#endif // INCLUDE_AUDIO_PRODUCTION_CODE

	if (pAudioRayInfo->numHits > 0)
	{
		ISurfaceTypeManager* const pSurfaceTypeManager = gEnv->p3DEngine->GetMaterialManager()->GetSurfaceTypeManager();
		CRY_ASSERT(pAudioRayInfo->numHits <= s_maxRayHits);

		for (size_t i = 0; i < pAudioRayInfo->numHits; ++i)
		{
			float const distance = pAudioRayInfo->hits[i].dist;

			if (distance > 0.0f)
			{
				ISurfaceType* const pMat = pSurfaceTypeManager->GetSurfaceType(pAudioRayInfo->hits[i].surface_idx);

				if (pMat != nullptr)
				{
					ISurfaceType::SPhysicalParams const& physParams = pMat->GetPhyscalParams();
					totalOcclusion += physParams.sound_obstruction;// not clamping b/w 0 and 1 for performance reasons
					++numRealHits;

#if defined(INCLUDE_AUDIO_PRODUCTION_CODE)
					minDistance = min(minDistance, distance);
#endif      // INCLUDE_AUDIO_PRODUCTION_CODE
				}
			}
		}
	}

	pAudioRayInfo->numHits = numRealHits;
	pAudioRayInfo->totalSoundOcclusion = clamp_tpl(totalOcclusion, 0.0f, 1.0f);

#if defined(INCLUDE_AUDIO_PRODUCTION_CODE)
	pAudioRayInfo->distanceToFirstObstacle = minDistance;

	if (m_remainingRays == 0)
	{
		CryFatalError("Negative ref or ray count on audio object");
	}
#endif

	if (--m_remainingRays == 0)
	{
		ProcessObstructionOcclusion();
	}
}

//////////////////////////////////////////////////////////////////////////
void CPropagationProcessor::ReleasePendingRays()
{
	if (m_remainingRays > 0)
	{
		m_remainingRays = 0;
	}
}

//////////////////////////////////////////////////////////////////////////
bool CPropagationProcessor::HasNewOcclusionValues()
{
	bool bNewValues = false;

	if (fabs_tpl(m_lastQuerriedOcclusion - m_occlusion) > FloatEpsilon || fabs_tpl(m_lastQuerriedObstruction - m_obstruction) > FloatEpsilon)
	{
		m_lastQuerriedObstruction = m_obstruction;
		m_lastQuerriedOcclusion = m_occlusion;
		bNewValues = true;
	}

	return bNewValues;
}

///////////////////////////////////////////////////////////////////////////
void CPropagationProcessor::SetOcclusionMultiplier(float const occlusionFadeOut)
{
	m_occlusionMultiplier = occlusionFadeOut;
}

///////////////////////////////////////////////////////////////////////////
void CPropagationProcessor::ProcessObstructionOcclusion()
{
	m_occlusion = 0.0f;
	m_obstruction = 0.0f;
	CRY_ASSERT_MESSAGE(m_currentListenerDistance > 0.0f, "Distance to Listener is 0!");

	size_t const numSamplePositions = GetNumSamplePositions();
	size_t const numConcurrentRays = GetNumConcurrentRays();

	if (numSamplePositions > 0 && numConcurrentRays > 0)
	{
		for (size_t i = 0; i < numConcurrentRays; ++i)
		{
			CAudioRayInfo const& rayInfo = m_raysInfo[i];
			m_raysOcclusion[rayInfo.samplePosIndex] = rayInfo.totalSoundOcclusion;
		}

		// Calculate the new occlusion average.
		for (size_t i = 0; i < numSamplePositions; ++i)
		{
			m_occlusion += m_raysOcclusion[i];
		}

		m_occlusion = (m_occlusion / numSamplePositions) * m_occlusionMultiplier;
	}

#if defined(INCLUDE_AUDIO_PRODUCTION_CODE)
	if (m_timeSinceLastUpdateMS > 100.0f && CanRunObstructionOcclusion()) // only re-sample the rays about 10 times per second for a smoother debug drawing
	{
		m_timeSinceLastUpdateMS = 0.0f;

		for (size_t i = 0; i < s_numConcurrentRaysHigh; ++i)
		{
			CAudioRayInfo const& rayInfo = m_raysInfo[i];
			SRayDebugInfo& rayDebugInfo = m_rayDebugInfos[i];

			rayDebugInfo.begin = rayInfo.startPosition;
			rayDebugInfo.end = rayInfo.startPosition + rayInfo.direction;

			if (rayDebugInfo.stableEnd.IsZeroFast())
			{
				// to be moved to the PropagationProcessor Reset method
				rayDebugInfo.stableEnd = rayDebugInfo.end;
			}
			else
			{
				rayDebugInfo.stableEnd += (rayDebugInfo.end - rayDebugInfo.stableEnd) * 0.1f;
			}

			rayDebugInfo.distanceToNearestObstacle = rayInfo.distanceToFirstObstacle;
			rayDebugInfo.numHits = rayInfo.numHits;
			rayDebugInfo.occlusionValue = rayInfo.totalSoundOcclusion;
		}
	}
#endif // INCLUDE_AUDIO_PRODUCTION_CODE
}

///////////////////////////////////////////////////////////////////////////
void CPropagationProcessor::CastObstructionRay(
  Vec3 const& origin,
  size_t const rayIndex,
  size_t const samplePosIndex,
  bool const bSynch)
{
	static int const physicsFlags = ent_water | ent_static | ent_sleeping_rigid | ent_rigid | ent_terrain;
	CAudioRayInfo& rayInfo = m_raysInfo[rayIndex];
	rayInfo.samplePosIndex = samplePosIndex;
	Vec3 const direction(m_transformation.GetPosition() - origin);
	Vec3 directionNormalized(direction);
	directionNormalized.Normalize();
	Vec3 const finalDirection(direction - (directionNormalized * g_cvars.m_occlusionRayLengthOffset));

	int const numHits = gEnv->pPhysicalWorld->RayWorldIntersection(
	  origin,
	  finalDirection, physicsFlags,
	  bSynch ? rwi_pierceability0 : rwi_pierceability0 | rwi_queue,
	  rayInfo.hits,
	  static_cast<int>(s_maxRayHits),
	  nullptr,
	  0,
	  &rayInfo,
	  PHYS_FOREIGN_ID_SOUND_OBSTRUCTION);

	++m_remainingRays;

	if (bSynch)
	{
		rayInfo.numHits = min(static_cast<size_t>(numHits) + 1, s_maxRayHits);
		ProcessPhysicsRay(&rayInfo);
	}

#if defined(INCLUDE_AUDIO_PRODUCTION_CODE)
	rayInfo.startPosition = origin;
	rayInfo.direction = finalDirection;

	if (bSynch)
	{
		++s_totalSyncPhysRays;
	}
	else
	{
		++s_totalAsyncPhysRays;
	}
#endif // INCLUDE_AUDIO_PRODUCTION_CODE
}

///////////////////////////////////////////////////////////////////////////
void CPropagationProcessor::RunObstructionQuery(Vec3 const& audioListenerPosition)
{
	static Vec3 const worldUp(0.0f, 0.0f, 1.0f);

	if (m_remainingRays == 0)
	{
		// Make the physics ray cast call synchronous or asynchronous depending on the distance to the listener.
		bool const bSynch = (m_currentListenerDistance <= g_cvars.m_occlusionMaxSyncDistance);
		Vec3 const side((audioListenerPosition - m_transformation.GetPosition()).Cross(worldUp).normalize());
		Vec3 const up((audioListenerPosition - m_transformation.GetPosition()).Cross(side).normalize());

		switch (m_occlusionType)
		{
		case EOcclusionType::Adaptive:
			{
				switch (m_occlusionTypeWhenAdaptive)
				{
				case EOcclusionType::Low:
					ProcessLow(audioListenerPosition, up, side, bSynch);
					break;
				case EOcclusionType::Medium:
					ProcessMedium(audioListenerPosition, up, side, bSynch);
					break;
				case EOcclusionType::High:
					ProcessHigh(audioListenerPosition, up, side, bSynch);
					break;
				default:
					CRY_ASSERT_MESSAGE(false, "Calculated Adaptive Occlusion Type invalid");
					break;
				}
			}
			break;
		case EOcclusionType::Low:
			ProcessLow(audioListenerPosition, up, side, bSynch);
			break;
		case EOcclusionType::Medium:
			ProcessMedium(audioListenerPosition, up, side, bSynch);
			break;
		case EOcclusionType::High:
			ProcessHigh(audioListenerPosition, up, side, bSynch);
			break;
		}
	}
}

//////////////////////////////////////////////////////////////////////////
void CPropagationProcessor::ProcessLow(
  Vec3 const& audioListenerPosition,
  Vec3 const& up,
  Vec3 const& side,
  bool const bSynch)
{
	for (size_t i = 0; i < s_numConcurrentRaysLow; ++i)
	{
		if (m_rayIndex >= s_numRaySamplePositionsLow)
		{
			m_rayIndex = 0;
		}

		Vec3 const origin(audioListenerPosition + up * s_raySamplePositionsLow[m_rayIndex].z + side * s_raySamplePositionsLow[m_rayIndex].x);
		CastObstructionRay(origin, i, m_rayIndex, bSynch);
		++m_rayIndex;
	}
}

//////////////////////////////////////////////////////////////////////////
void CPropagationProcessor::ProcessMedium(
  Vec3 const& audioListenerPosition,
  Vec3 const& up,
  Vec3 const& side,
  bool const bSynch)
{
	for (size_t i = 0; i < s_numConcurrentRaysMedium; ++i)
	{
		if (m_rayIndex >= s_numRaySamplePositionsMedium)
		{
			m_rayIndex = 0;
		}

		Vec3 const origin(audioListenerPosition + up * s_raySamplePositionsMedium[m_rayIndex].z + side * s_raySamplePositionsMedium[m_rayIndex].x);
		CastObstructionRay(origin, i, m_rayIndex, bSynch);
		++m_rayIndex;
	}
}

//////////////////////////////////////////////////////////////////////////
void CPropagationProcessor::ProcessHigh(
  Vec3 const& audioListenerPosition,
  Vec3 const& up,
  Vec3 const& side,
  bool const bSynch)
{
	for (size_t i = 0; i < s_numConcurrentRaysHigh; ++i)
	{
		if (m_rayIndex >= s_numRaySamplePositionsHigh)
		{
			m_rayIndex = 0;
		}

		Vec3 const origin(audioListenerPosition + up * s_raySamplePositionsHigh[m_rayIndex].z + side * s_raySamplePositionsHigh[m_rayIndex].x);
		CastObstructionRay(origin, i, m_rayIndex, bSynch);
		++m_rayIndex;
	}
}

//////////////////////////////////////////////////////////////////////////
size_t CPropagationProcessor::GetNumConcurrentRays() const
{
	size_t numConcurrentRays = 0;

	switch (m_occlusionType)
	{
	case EOcclusionType::Adaptive:
		{
			switch (m_occlusionTypeWhenAdaptive)
			{
			case EOcclusionType::Low:
				numConcurrentRays = s_numConcurrentRaysLow;
				break;
			case EOcclusionType::Medium:
				numConcurrentRays = s_numConcurrentRaysMedium;
				break;
			case EOcclusionType::High:
				numConcurrentRays = s_numConcurrentRaysHigh;
				break;
			default:
				CRY_ASSERT_MESSAGE(false, "Calculated Adaptive Occlusion Type invalid");
				break;
			}
		}
		break;
	case EOcclusionType::Low:
		numConcurrentRays = s_numConcurrentRaysLow;
		break;
	case EOcclusionType::Medium:
		numConcurrentRays = s_numConcurrentRaysMedium;
		break;
	case EOcclusionType::High:
		numConcurrentRays = s_numConcurrentRaysHigh;
		break;
	}

	return numConcurrentRays;
}

//////////////////////////////////////////////////////////////////////////
size_t CPropagationProcessor::GetNumSamplePositions() const
{
	size_t numSamplePositions = 0;

	switch (m_occlusionType)
	{
	case EOcclusionType::Adaptive:
		{
			switch (m_occlusionTypeWhenAdaptive)
			{
			case EOcclusionType::Low:
				numSamplePositions = s_numRaySamplePositionsLow;
				break;
			case EOcclusionType::Medium:
				numSamplePositions = s_numRaySamplePositionsMedium;
				break;
			case EOcclusionType::High:
				numSamplePositions = s_numRaySamplePositionsHigh;
				break;
			default:
				CRY_ASSERT_MESSAGE(false, "Calculated Adaptive Occlusion Type invalid");
				break;
			}
		}
		break;
	case EOcclusionType::Low:
		numSamplePositions = s_numRaySamplePositionsLow;
		break;
	case EOcclusionType::Medium:
		numSamplePositions = s_numRaySamplePositionsMedium;
		break;
	case EOcclusionType::High:
		numSamplePositions = s_numRaySamplePositionsHigh;
		break;
	}

	return numSamplePositions;
}

#if defined(INCLUDE_AUDIO_PRODUCTION_CODE)

size_t CPropagationProcessor::s_totalSyncPhysRays = 0;
size_t CPropagationProcessor::s_totalAsyncPhysRays = 0;

///////////////////////////////////////////////////////////////////////////
void CPropagationProcessor::DrawObstructionRays(IRenderAuxGeom& auxGeom, EObjectFlags const objectFlags) const
{
	if (CanRunObstructionOcclusion() && (objectFlags& EObjectFlags::Virtual) == 0)
	{
		size_t const numConcurrentRays = GetNumConcurrentRays();
		CRY_ASSERT(numConcurrentRays > 0);

		for (size_t i = 0; i < numConcurrentRays; ++i)
		{
			DrawRay(auxGeom, i);
		}
	}
}

//////////////////////////////////////////////////////////////////////////
void CPropagationProcessor::DrawRay(IRenderAuxGeom& auxGeom, size_t const rayIndex) const
{
	if ((g_cvars.m_drawAudioDebug & EAudioDebugDrawFilter::DrawOcclusionRays) > 0)
	{
		static ColorB const obstructedRayColor(200, 20, 1, 255);
		static ColorB const freeRayColor(20, 200, 1, 255);
		static ColorB const intersectionSphereColor(250, 200, 1, 240);
		static float const collisionPtSphereRadius = 0.01f;
		SAuxGeomRenderFlags const previousRenderFlags = auxGeom.GetRenderFlags();
		SAuxGeomRenderFlags newRenderFlags(e_Def3DPublicRenderflags | e_AlphaBlended);
		newRenderFlags.SetCullMode(e_CullModeNone);

		bool const bRayObstructed = (m_rayDebugInfos[rayIndex].numHits > 0);
		Vec3 const rayEnd = bRayObstructed ?
			m_rayDebugInfos[rayIndex].begin + (m_rayDebugInfos[rayIndex].end - m_rayDebugInfos[rayIndex].begin).GetNormalized() * m_rayDebugInfos[rayIndex].distanceToNearestObstacle :
			m_rayDebugInfos[rayIndex].end; // only draw the ray to the first collision point

		ColorB const& rayColor = bRayObstructed ? obstructedRayColor : freeRayColor;

		auxGeom.SetRenderFlags(newRenderFlags);

		if (bRayObstructed)
		{
			// mark the nearest collision with a small sphere
			auxGeom.DrawSphere(rayEnd, collisionPtSphereRadius, intersectionSphereColor);
		}

		auxGeom.DrawLine(m_rayDebugInfos[rayIndex].begin, rayColor, rayEnd, rayColor, 1.0f);
		auxGeom.SetRenderFlags(previousRenderFlags);
	}
}

///////////////////////////////////////////////////////////////////////////
void CPropagationProcessor::ResetRayData()
{
	if (m_occlusionType != EOcclusionType::None && m_occlusionType != EOcclusionType::Ignore)
	{
		for (size_t i = 0; i < s_numConcurrentRaysHigh; ++i)
		{
			m_raysInfo[i].Reset();
			m_rayDebugInfos[i] = SRayDebugInfo();
		}
	}
}

#endif // INCLUDE_AUDIO_PRODUCTION_CODE
}      // namespace CryAudio
