// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "stdafx.h"

#if defined(CRY_AUDIO_USE_OCCLUSION)
	#include "PropagationProcessor.h"
	#include "Common.h"
	#include "Managers.h"
	#include "ListenerManager.h"
	#include "CVars.h"
	#include "Object.h"
	#include "System.h"
	#include "Listener.h"
	#include "ObjectRequestData.h"
	#include <Cry3DEngine/I3DEngine.h>
	#include <Cry3DEngine/ISurfaceType.h>

	#if defined(CRY_AUDIO_USE_DEBUG_CODE)
		#include "Debug.h"
		#include <CryRenderer/IRenderAuxGeom.h>
		#include <CryMath/Random.h>
	#endif // CRY_AUDIO_USE_DEBUG_CODE

namespace CryAudio
{
	#if defined(CRY_AUDIO_USE_DEBUG_CODE)
constexpr uint32 g_numIndices = 6;
constexpr vtx_idx g_auxIndices[g_numIndices] = { 2, 1, 0, 2, 3, 1 };
constexpr uint32 g_numPoints = 4;
	#endif // CRY_AUDIO_USE_DEBUG_CODE

float g_listenerHeadSize = 0.0f;
float g_listenerHeadSizeHalf = 0.0f;

enum class EOcclusionCollisionType : EnumFlagsType
{
	None    = 0,
	Static  = BIT(6), // a,
	Rigid   = BIT(7), // b,
	Water   = BIT(8), // c,
	Terrain = BIT(9), // d,
};
CRY_CREATE_ENUM_FLAG_OPERATORS(EOcclusionCollisionType);

struct SRayOffset final
{
	SRayOffset() = default;
	SRayOffset(SRayOffset const&) = delete;
	SRayOffset(SRayOffset&&) = delete;
	SRayOffset& operator=(SRayOffset const&) = delete;
	SRayOffset& operator=(SRayOffset&&) = delete;

	float x = 0.0f;
	float z = 0.0f;
};

SRayOffset g_raySamplePositionsLow[g_numRaySamplePositionsLow];
SRayOffset g_raySamplePositionsMedium[g_numRaySamplePositionsMedium];
SRayOffset g_raySamplePositionsHigh[g_numRaySamplePositionsHigh];

SRayOffset g_initialRaySamplePositionsLow[g_numInitialSamplePositions];
SRayOffset g_initialRaySamplePositionsMedium[g_numInitialSamplePositions];
SRayOffset g_initialRaySamplePositionsHigh[g_numInitialSamplePositions];

uint8 g_initialRaySamplePositionTopLeft = 0;
uint8 g_initialRaySamplePositionTopRight = 0;
uint8 g_initialRaySamplePositionCenter = 0;
uint8 g_initialRaySamplePositionBottomLeft = 0;
uint8 g_initialRaySamplePositionBottomRight = 0;

int CPropagationProcessor::s_occlusionRayFlags = 0;

///////////////////////////////////////////////////////////////////////////
void CRayInfo::Reset()
{
	totalSoundOcclusion = 0.0f;
	numHits = 0;
	#if defined(CRY_AUDIO_USE_DEBUG_CODE)
	startPosition.zero();
	direction.zero();
	distanceToFirstObstacle = FLT_MAX;
	#endif // CRY_AUDIO_USE_DEBUG_CODE
}

bool CPropagationProcessor::s_bCanIssueRWIs = false;

	#if defined(CRY_AUDIO_USE_DEBUG_CODE)
uint16 CPropagationProcessor::s_totalSyncPhysRays = 0;
uint16 CPropagationProcessor::s_totalAsyncPhysRays = 0;
	#endif // CRY_AUDIO_USE_DEBUG_CODE

///////////////////////////////////////////////////////////////////////////
int CPropagationProcessor::OnObstructionTest(EventPhys const* pEvent)
{
	auto const pRWIResult = static_cast<EventPhysRWIResult const*>(pEvent);

	if (pRWIResult->iForeignData == PHYS_FOREIGN_ID_SOUND_OBSTRUCTION)
	{
		auto& rayInfo = *(static_cast<CRayInfo*>(pRWIResult->pForeignData));
		rayInfo.numHits = static_cast<uint8>(pRWIResult->nHits);

		// The very first entry in "pHits" is reserved for a solid hit.
		// As we're using "rwi_max_piercing" when issuing a ray cast we're guaranteed to never have a solid hit.
		// This means we're safe to always ignore the very first entry returned in "ray_hit".
		CRY_ASSERT_MESSAGE(pRWIResult->pHits[0].dist < 0.0f, "<Audio> encountered a solid hit in %s", __FUNCTION__);

		// Skip the "solid hit entry".
		ray_hit const* const pHits = &(pRWIResult->pHits[1]);

		for (uint8 i = 0; i < rayInfo.numHits; ++i)
		{
			rayInfo.hits[i].distance = pHits[i].dist;
			rayInfo.hits[i].surfaceIndex = pHits[i].surface_idx;
		}

		SObjectRequestData<EObjectRequestType::ProcessPhysicsRay> const requestData(rayInfo.pObject, rayInfo);
		CRequest const request(&requestData);
		g_system.PushRequest(request);
	}
	else
	{
		CRY_ASSERT_MESSAGE(false, "<Audio> iForeignData must be PHYS_FOREIGN_ID_SOUND_OBSTRUCTION during %s", __FUNCTION__);
	}

	return 1;
}

///////////////////////////////////////////////////////////////////////////
CPropagationProcessor::CPropagationProcessor(CObject& object)
	: m_occlusionRayOffset(0.1f)
	, m_remainingRays(0)
	, m_object(object)
	, m_occlusionType(EOcclusionType::None)
	, m_originalOcclusionType(EOcclusionType::None)
{
}

//////////////////////////////////////////////////////////////////////////
void CPropagationProcessor::Init()
{
	auto const& listeners = m_object.GetListeners();
	m_occlusionInfos.reserve(listeners.size());

	for (auto const pListener : listeners)
	{
		AddListener(pListener);
	}
}

//////////////////////////////////////////////////////////////////////////
void CPropagationProcessor::Release()
{
	for (SOcclusionInfo const* const pInfo : m_occlusionInfos)
	{
		delete pInfo;
	}

	m_occlusionInfos.clear();
}

//////////////////////////////////////////////////////////////////////////
void CPropagationProcessor::UpdateOcclusionRayFlags()
{
	if ((g_cvars.m_occlusionCollisionTypes & EOcclusionCollisionType::Static) != 0)
	{
		s_occlusionRayFlags |= ent_static;
	}
	else
	{
		s_occlusionRayFlags &= ~ent_static;
	}

	if ((g_cvars.m_occlusionCollisionTypes & EOcclusionCollisionType::Rigid) != 0)
	{
		s_occlusionRayFlags |= (ent_sleeping_rigid | ent_rigid);
	}
	else
	{
		s_occlusionRayFlags &= ~(ent_sleeping_rigid | ent_rigid);
	}

	if ((g_cvars.m_occlusionCollisionTypes & EOcclusionCollisionType::Water) != 0)
	{
		s_occlusionRayFlags |= ent_water;
	}
	else
	{
		s_occlusionRayFlags &= ~ent_water;
	}

	if ((g_cvars.m_occlusionCollisionTypes & EOcclusionCollisionType::Terrain) != 0)
	{
		s_occlusionRayFlags |= ent_terrain;
	}
	else
	{
		s_occlusionRayFlags &= ~ent_terrain;
	}
}

///////////////////////////////////////////////////////////////////////////
void CPropagationProcessor::Update()
{
	#if defined(CRY_AUDIO_USE_DEBUG_CODE)
	if (g_cvars.m_occlusionGlobalType > 0)
	{
		m_occlusionType = clamp_tpl<EOcclusionType>(static_cast<EOcclusionType>(g_cvars.m_occlusionGlobalType), EOcclusionType::Ignore, EOcclusionType::High);
	}
	else
	{
		m_occlusionType = m_originalOcclusionType;
	}
	#endif // CRY_AUDIO_USE_DEBUG_CODE

	if (CanRunOcclusion())
	{
		for (auto const pInfo : m_occlusionInfos)
		{
			if (m_occlusionType == EOcclusionType::Adaptive)
			{
				if (pInfo->currentListenerDistance < g_cvars.m_occlusionHighDistance)
				{
					pInfo->occlusionTypeWhenAdaptive = EOcclusionType::High;
				}
				else if (pInfo->currentListenerDistance < g_cvars.m_occlusionMediumDistance)
				{
					pInfo->occlusionTypeWhenAdaptive = EOcclusionType::Medium;
				}
				else
				{
					pInfo->occlusionTypeWhenAdaptive = EOcclusionType::Low;
				}
			}
		}

		RunObstructionQuery();
	}
	else
	{
		for (auto const pInfo : m_occlusionInfos)
		{
			pInfo->occlusion = 0.0f;
		}
	}
}

//////////////////////////////////////////////////////////////////////////
void CPropagationProcessor::SetOcclusionType(EOcclusionType const occlusionType)
{
	m_occlusionType = occlusionType;
	m_originalOcclusionType = occlusionType;
}

//////////////////////////////////////////////////////////////////////////
void CPropagationProcessor::UpdateOcclusion()
{
	if (CanRunOcclusion())
	{
		if (g_cvars.m_occlusionInitialRayCastMode == 1)
		{
			for (auto const pInfo : m_occlusionInfos)
			{
				Vec3 const& listenerPosition = pInfo->pListener->GetTransformation().GetPosition();
				Vec3 const& objectPosition = m_object.GetTransformation().GetPosition();

				pInfo->occlusion = CastInitialRay(listenerPosition, objectPosition, (g_cvars.m_occlusionAccumulate > 0));

				for (auto& rayOcclusion : pInfo->raysOcclusion)
				{
					rayOcclusion = pInfo->occlusion;
				}
			}
		}
		else if (g_cvars.m_occlusionInitialRayCastMode > 1)
		{
			for (auto const pInfo : m_occlusionInfos)
			{
				float occlusionValues[g_numInitialSamplePositions];
				Vec3 const& listenerPosition = pInfo->pListener->GetTransformation().GetPosition();
				Vec3 const& objectPosition = m_object.GetTransformation().GetPosition();

				// TODO: this breaks if listener and object x and y coordinates are exactly the same.
				Vec3 const side((listenerPosition - objectPosition).Cross(Vec3Constants<float>::fVec3_OneZ).normalize());
				Vec3 const up((listenerPosition - objectPosition).Cross(side).normalize());
				bool const accumulate = g_cvars.m_occlusionAccumulate > 0;

				for (uint8 i = 0; i < g_numInitialSamplePositions; ++i)
				{
					switch (m_occlusionType)
					{
					case EOcclusionType::Adaptive:
						{
							switch (pInfo->occlusionTypeWhenAdaptive)
							{
							case EOcclusionType::Low:
								{
									SRayOffset const& rayOffset = g_initialRaySamplePositionsLow[i];
									Vec3 const origin(listenerPosition + up* rayOffset.z + side* rayOffset.x);
									occlusionValues[i] = CastInitialRay(origin, objectPosition, accumulate);

									break;
								}
							case EOcclusionType::Medium:
								{
									SRayOffset const& rayOffset = g_initialRaySamplePositionsMedium[i];
									Vec3 const origin(listenerPosition + up* rayOffset.z + side* rayOffset.x);
									occlusionValues[i] = CastInitialRay(origin, objectPosition, accumulate);

									break;
								}
							case EOcclusionType::High:
								{
									SRayOffset const& rayOffset = g_initialRaySamplePositionsHigh[i];
									Vec3 const origin(listenerPosition + up* rayOffset.z + side* rayOffset.x);
									occlusionValues[i] = CastInitialRay(origin, objectPosition, accumulate);

									break;
								}
							default:
								{
									break;
								}
							}

							break;
						}
					case EOcclusionType::Low:
						{
							SRayOffset const& rayOffset = g_initialRaySamplePositionsLow[i];
							Vec3 const origin(listenerPosition + up* rayOffset.z + side* rayOffset.x);
							occlusionValues[i] = CastInitialRay(origin, objectPosition, accumulate);

							break;
						}
					case EOcclusionType::Medium:
						{
							SRayOffset const& rayOffset = g_initialRaySamplePositionsMedium[i];
							Vec3 const origin(listenerPosition + up* rayOffset.z + side* rayOffset.x);
							occlusionValues[i] = CastInitialRay(origin, objectPosition, accumulate);

							break;
						}
					case EOcclusionType::High:
						{
							SRayOffset const& rayOffset = g_initialRaySamplePositionsHigh[i];
							Vec3 const origin(listenerPosition + up* rayOffset.z + side* rayOffset.x);
							occlusionValues[i] = CastInitialRay(origin, objectPosition, accumulate);

							break;
						}
					default:
						{
							break;
						}
					}
				}

				pInfo->occlusion = 0.0f;

				for (auto const value : occlusionValues)
				{
					pInfo->occlusion += value;
				}

				pInfo->occlusion /= static_cast<float>(g_numInitialSamplePositions);

				for (auto& rayOcclusion : pInfo->raysOcclusion)
				{
					rayOcclusion = pInfo->occlusion;
				}
			}
		}
		else
		{
			for (auto const pInfo : m_occlusionInfos)
			{
				pInfo->occlusion = 0.0f;
				pInfo->lastQuerriedOcclusion = 0.0f;
			}
		}
	}
	else
	{
		for (auto const pInfo : m_occlusionInfos)
		{
			pInfo->occlusion = 0.0f;
			pInfo->lastQuerriedOcclusion = 0.0f;
		}
	}
}

//////////////////////////////////////////////////////////////////////////
void CPropagationProcessor::AddListener(CListener* const pListener)
{
	auto const pInfo = new SOcclusionInfo(pListener);
	pInfo->currentListenerDistance = pListener->GetTransformation().GetPosition().GetDistance(m_object.GetTransformation().GetPosition());

	for (auto& rayInfo : pInfo->raysInfo)
	{
		rayInfo.pObject = &m_object;
	}

	#if defined(CRY_AUDIO_USE_DEBUG_CODE)
	pInfo->listenerOcclusionPlaneColor.set(cry_random<uint8>(0, 255), cry_random<uint8>(0, 255), cry_random<uint8>(0, 255), uint8(128));
	#endif // CRY_AUDIO_USE_DEBUG_CODE

	m_occlusionInfos.push_back(pInfo);
}

//////////////////////////////////////////////////////////////////////////
void CPropagationProcessor::RemoveListener(CListener* const pListener)
{
	auto iter(m_occlusionInfos.begin());
	auto const iterEnd(m_occlusionInfos.cend());

	for (; iter != iterEnd; ++iter)
	{
		SOcclusionInfo const* const pInfo = *iter;

		if (pInfo->pListener == pListener)
		{
			delete pInfo;

			if (iter != (iterEnd - 1))
			{
				(*iter) = m_occlusionInfos.back();
			}

			m_occlusionInfos.pop_back();
			break;
		}
	}
}

//////////////////////////////////////////////////////////////////////////
float CPropagationProcessor::CastInitialRay(Vec3 const& origin, Vec3 const& target, bool const accumulate)
{
	Vec3 const direction(target - origin);
	Vec3 directionNormalized(direction);
	directionNormalized.Normalize();
	Vec3 const finalDirection(direction - (directionNormalized* m_occlusionRayOffset));
	ray_hit hits[g_maxRayHits];

	// We use "rwi_max_piercing" to allow audio rays to always pierce surfaces regardless of the "pierceability" attribute.
	// Note: The very first entry of rayInfo.hits (solid slot) is always empty.
	int const numHits = gEnv->pPhysicalWorld->RayWorldIntersection(
		origin,
		finalDirection,
		s_occlusionRayFlags,
		rwi_max_piercing,
		hits,
		static_cast<int>(g_maxRayHits),
		nullptr,
		nullptr,
		nullptr,
		PHYS_FOREIGN_ID_SOUND_OBSTRUCTION);

	CRayInfo rayInfo;
	rayInfo.numHits = static_cast<uint8>(numHits);

	// The very first entry in "hits" is reserved for a solid hit.
	// As we're using "rwi_max_piercing" when issuing a ray cast we're guaranteed to never have a solid hit.
	// This means we're safe to always ignore the very first entry returned in "ray_hit".
	CRY_ASSERT_MESSAGE(hits[0].dist < 0.0f, "<Audio> encountered a solid hit in %s", __FUNCTION__);

	// Skip the "solid hit entry".
	ray_hit const* const pHits = &(hits[1]);

	for (uint8 i = 0; i < rayInfo.numHits; ++i)
	{
		rayInfo.hits[i].distance = pHits[i].dist;
		rayInfo.hits[i].surfaceIndex = pHits[i].surface_idx;
	}

	float finalOcclusion = 0.0f;

	if ((g_cvars.m_occlusionSetFullOnMaxHits > 0) && (rayInfo.numHits == g_maxRayHits))
	{
		finalOcclusion = 1.0f;
	}
	else if (rayInfo.numHits > 0)
	{
		ISurfaceTypeManager* const pSurfaceTypeManager = gEnv->p3DEngine->GetMaterialManager()->GetSurfaceTypeManager();

		for (uint8 i = 0; i < rayInfo.numHits; ++i)
		{
			float const distance = rayInfo.hits[i].distance;

			if (distance > 0.0f)
			{
				ISurfaceType* const pMat = pSurfaceTypeManager->GetSurfaceType(static_cast<int>(rayInfo.hits[i].surfaceIndex));

				if (pMat != nullptr)
				{
					ISurfaceType::SPhysicalParams const& physParams = pMat->GetPhyscalParams();

					if (accumulate)
					{
						finalOcclusion += physParams.sound_obstruction; // Not clamping b/w 0 and 1 for performance reasons.
					}
					else
					{
						finalOcclusion = std::max(finalOcclusion, physParams.sound_obstruction);
					}

					if (finalOcclusion >= 1.0f)
					{
						break;
					}
				}
			}
		}
	}

	return clamp_tpl(finalOcclusion, 0.0f, 1.0f);
}

//////////////////////////////////////////////////////////////////////////
bool CPropagationProcessor::CanRunOcclusion()
{
	bool canRun = false;

	if ((m_occlusionType != EOcclusionType::None) &&
	    (m_occlusionType != EOcclusionType::Ignore) &&
	    ((m_object.GetFlags() & EObjectFlags::Virtual) == 0) &&
	    s_bCanIssueRWIs)
	{
		for (auto const pInfo : m_occlusionInfos)
		{
			pInfo->currentListenerDistance = m_object.GetTransformation().GetPosition().GetDistance(pInfo->pListener->GetTransformation().GetPosition());

			if ((pInfo->currentListenerDistance > g_cvars.m_occlusionMinDistance) && (pInfo->currentListenerDistance < g_cvars.m_occlusionMaxDistance))
			{
				canRun = true;
			}
		}
	}

	#if defined(CRY_AUDIO_USE_DEBUG_CODE)
	canRun ? m_object.SetFlag(EObjectFlags::CanRunOcclusion) : m_object.RemoveFlag(EObjectFlags::CanRunOcclusion);
	#endif // CRY_AUDIO_USE_DEBUG_CODE

	return canRun;
}

//////////////////////////////////////////////////////////////////////////
float CPropagationProcessor::GetOcclusion(CListener* const pListener) const
{
	float occlusion = 0.0f;

	for (SOcclusionInfo const* const pInfo : m_occlusionInfos)
	{
		if (pListener == pInfo->pListener)
		{
			occlusion = pInfo->occlusion;
			break;
		}
	}

	return occlusion;
}

///////////////////////////////////////////////////////////////////////////
void CPropagationProcessor::ProcessPhysicsRay(CRayInfo& rayInfo)
{
	CRY_ASSERT((rayInfo.samplePosIndex >= 0) && (rayInfo.samplePosIndex < g_numRaySamplePositionsHigh));

	float finalOcclusion = 0.0f;
	uint8 numRealHits = 0;

	#if defined(CRY_AUDIO_USE_DEBUG_CODE)
	float minDistance = FLT_MAX;
	#endif // CRY_AUDIO_USE_DEBUG_CODE

	if ((g_cvars.m_occlusionSetFullOnMaxHits > 0) && (rayInfo.numHits == g_maxRayHits))
	{
		finalOcclusion = 1.0f;
		numRealHits = g_maxRayHits;
	}
	else if (rayInfo.numHits > 0)
	{
		ISurfaceTypeManager* const pSurfaceTypeManager = gEnv->p3DEngine->GetMaterialManager()->GetSurfaceTypeManager();
		bool const accumulate = g_cvars.m_occlusionAccumulate > 0;

		for (uint8 i = 0; i < rayInfo.numHits; ++i)
		{
			float const distance = rayInfo.hits[i].distance;

			if (distance > 0.0f)
			{
				ISurfaceType* const pMat = pSurfaceTypeManager->GetSurfaceType(static_cast<int>(rayInfo.hits[i].surfaceIndex));

				if (pMat != nullptr)
				{
					ISurfaceType::SPhysicalParams const& physParams = pMat->GetPhyscalParams();

					if (accumulate)
					{
						finalOcclusion += physParams.sound_obstruction; // Not clamping b/w 0 and 1 for performance reasons.
					}
					else
					{
						finalOcclusion = std::max(finalOcclusion, physParams.sound_obstruction);
					}

					++numRealHits;

	#if defined(CRY_AUDIO_USE_DEBUG_CODE)
					minDistance = std::min(minDistance, distance);
	#endif    // CRY_AUDIO_USE_DEBUG_CODE

					if (finalOcclusion >= 1.0f)
					{
						break;
					}
				}
			}
		}
	}

	rayInfo.numHits = numRealHits;
	rayInfo.totalSoundOcclusion = clamp_tpl(finalOcclusion, 0.0f, 1.0f);

	#if defined(CRY_AUDIO_USE_DEBUG_CODE)
	rayInfo.distanceToFirstObstacle = minDistance;

	if (m_remainingRays == 0)
	{
		CryFatalError("Negative ref or ray count on audio object");
	}
	#endif // CRY_AUDIO_USE_DEBUG_CODE

	if (--m_remainingRays == 0)
	{
		ProcessObstructionOcclusion();
	}
}

//////////////////////////////////////////////////////////////////////////
void CPropagationProcessor::ReleasePendingRays()
{
	m_remainingRays = 0;
}

//////////////////////////////////////////////////////////////////////////
bool CPropagationProcessor::HasNewOcclusionValues()
{
	bool hasNewValues = false;

	for (auto const pInfo : m_occlusionInfos)
	{
		if (fabs_tpl(pInfo->lastQuerriedOcclusion - pInfo->occlusion) > FloatEpsilon)
		{
			pInfo->lastQuerriedOcclusion = pInfo->occlusion;
			hasNewValues = true;
		}
	}

	return hasNewValues;
}

///////////////////////////////////////////////////////////////////////////
void CPropagationProcessor::ProcessObstructionOcclusion()
{
	for (auto const pInfo : m_occlusionInfos)
	{
		pInfo->occlusion = 0.0f;
		CRY_ASSERT_MESSAGE(pInfo->currentListenerDistance > 0.0f, "Distance to Listener is 0 during %s", __FUNCTION__);

		uint8 const numSamplePositions = GetNumSamplePositions(pInfo->occlusionTypeWhenAdaptive);
		uint8 const numConcurrentRays = GetNumConcurrentRays(pInfo->occlusionTypeWhenAdaptive);

		if (numSamplePositions > 0 && numConcurrentRays > 0)
		{
			for (uint8 i = 0; i < numConcurrentRays; ++i)
			{
				CRayInfo const& rayInfo = pInfo->raysInfo[i];
				pInfo->raysOcclusion[rayInfo.samplePosIndex] = rayInfo.totalSoundOcclusion;
			}

			// Calculate the new occlusion average.
			// TODO: Optimize this!
			for (uint8 i = 0; i < numSamplePositions; ++i)
			{
				pInfo->occlusion += pInfo->raysOcclusion[i];
			}

			pInfo->occlusion = (pInfo->occlusion / numSamplePositions);
		}

	#if defined(CRY_AUDIO_USE_DEBUG_CODE)
		if ((m_object.GetFlags() & EObjectFlags::CanRunOcclusion) != 0)
		{
			for (uint8 i = 0; i < numConcurrentRays; ++i)
			{
				CRayInfo const& rayInfo = pInfo->raysInfo[i];
				SRayDebugInfo& rayDebugInfo = pInfo->rayDebugInfos[i];
				rayDebugInfo.samplePosIndex = rayInfo.samplePosIndex;
				rayDebugInfo.begin = rayInfo.startPosition;
				rayDebugInfo.end = rayInfo.startPosition + rayInfo.direction;
				rayDebugInfo.distanceToNearestObstacle = rayInfo.distanceToFirstObstacle;
				rayDebugInfo.numHits = rayInfo.numHits;
				rayDebugInfo.occlusionValue = rayInfo.totalSoundOcclusion;
			}
		}
	#endif // CRY_AUDIO_USE_DEBUG_CODE
	}
}

///////////////////////////////////////////////////////////////////////////
void CPropagationProcessor::CastObstructionRay(
	Vec3 const& origin,
	uint8 const rayIndex,
	uint8 const samplePosIndex,
	bool const bSynch,
	SOcclusionInfo* const pInfo)
{
	Vec3 const direction(m_object.GetTransformation().GetPosition() - origin);
	Vec3 directionNormalized(direction);
	directionNormalized.Normalize();
	Vec3 const finalDirection(direction - (directionNormalized* m_occlusionRayOffset));

	// We use "rwi_max_piercing" to allow audio rays to always pierce surfaces regardless of the "pierceability" attribute.
	// Note: The very first entry of rayInfo.hits (solid slot) is always empty.
	CRayInfo& rayInfo = pInfo->raysInfo[rayIndex];
	rayInfo.samplePosIndex = samplePosIndex;

	#if defined(CRY_AUDIO_USE_DEBUG_CODE)
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
	#endif // CRY_AUDIO_USE_DEBUG_CODE

	++m_remainingRays;

	if (bSynch)
	{
		ray_hit hits[g_maxRayHits];

		int const numHits = gEnv->pPhysicalWorld->RayWorldIntersection(
			origin,
			finalDirection,
			s_occlusionRayFlags,
			rwi_max_piercing,
			hits,
			static_cast<int>(g_maxRayHits),
			nullptr,
			nullptr,
			nullptr,
			PHYS_FOREIGN_ID_SOUND_OBSTRUCTION);

		rayInfo.numHits = static_cast<uint8>(numHits);

		// The very first entry in "hits" is reserved for a solid hit.
		// As we're using "rwi_max_piercing" when issuing a ray cast we're guaranteed to never have a solid hit.
		// This means we're safe to always ignore the very first entry returned in "ray_hit".
		CRY_ASSERT_MESSAGE(hits[0].dist < 0.0f, "<Audio> encountered a solid hit in %s", __FUNCTION__);

		// Skip the "solid hit entry".
		ray_hit const* const pHits = &(hits[1]);

		for (uint8 i = 0; i < rayInfo.numHits; ++i)
		{
			rayInfo.hits[i].distance = pHits[i].dist;
			rayInfo.hits[i].surfaceIndex = pHits[i].surface_idx;
		}

		ProcessPhysicsRay(rayInfo);
	}
	else
	{
		gEnv->pPhysicalWorld->RayWorldIntersection(
			origin,
			finalDirection,
			s_occlusionRayFlags,
			rwi_max_piercing | rwi_queue,
			nullptr,
			static_cast<int>(g_maxRayHits),
			nullptr,
			nullptr,
			&rayInfo,
			PHYS_FOREIGN_ID_SOUND_OBSTRUCTION);
	}
}

///////////////////////////////////////////////////////////////////////////
void CPropagationProcessor::RunObstructionQuery()
{
	if (m_remainingRays == 0)
	{
		for (auto const pInfo : m_occlusionInfos)
		{
			// Make the physics ray cast call synchronous or asynchronous depending on the distance to the listener.
			bool const bSynch = (pInfo->currentListenerDistance <= g_cvars.m_occlusionMaxSyncDistance);

			Vec3 const& listenerPosition = pInfo->pListener->GetTransformation().GetPosition();

			// TODO: this breaks if listener and object x and y coordinates are exactly the same.
			Vec3 const side((listenerPosition - m_object.GetTransformation().GetPosition()).Cross(Vec3Constants<float>::fVec3_OneZ).normalize());
			Vec3 const up((listenerPosition - m_object.GetTransformation().GetPosition()).Cross(side).normalize());

			switch (m_occlusionType)
			{
			case EOcclusionType::Adaptive:
				{
					switch (pInfo->occlusionTypeWhenAdaptive)
					{
					case EOcclusionType::Low:
						{
							ProcessLow(up, side, bSynch, pInfo);
							break;
						}
					case EOcclusionType::Medium:
						{
							ProcessMedium(up, side, bSynch, pInfo);
							break;
						}
					case EOcclusionType::High:
						{
							ProcessHigh(up, side, bSynch, pInfo);
							break;
						}
					default:
						{
							CRY_ASSERT_MESSAGE(false, "Calculated Adaptive Occlusion Type invalid during %s", __FUNCTION__);
							break;
						}
					}

					break;
				}
			case EOcclusionType::Low:
				{
					ProcessLow(up, side, bSynch, pInfo);
					break;
				}
			case EOcclusionType::Medium:
				{
					ProcessMedium(up, side, bSynch, pInfo);
					break;
				}
			case EOcclusionType::High:
				{
					ProcessHigh(up, side, bSynch, pInfo);
					break;
				}
			default:
				{
					break;
				}
			}
		}
	}
}

//////////////////////////////////////////////////////////////////////////
void CPropagationProcessor::ProcessLow(
	Vec3 const& up,
	Vec3 const& side,
	bool const bSynch,
	SOcclusionInfo* const pInfo)
{
	for (uint8 i = 0; i < g_numConcurrentRaysLow; ++i)
	{
		if (pInfo->rayIndex >= g_numRaySamplePositionsLow)
		{
			pInfo->rayIndex = 0;
		}

		Vec3 const origin(pInfo->pListener->GetTransformation().GetPosition() + up* g_raySamplePositionsLow[pInfo->rayIndex].z + side* g_raySamplePositionsLow[pInfo->rayIndex].x);
		CastObstructionRay(origin, i, pInfo->rayIndex, bSynch, pInfo);
		++pInfo->rayIndex;
	}
}

//////////////////////////////////////////////////////////////////////////
void CPropagationProcessor::ProcessMedium(
	Vec3 const& up,
	Vec3 const& side,
	bool const bSynch,
	SOcclusionInfo* const pInfo)
{
	for (uint8 i = 0; i < g_numConcurrentRaysMedium; ++i)
	{
		if (pInfo->rayIndex >= g_numRaySamplePositionsMedium)
		{
			pInfo->rayIndex = 0;
		}

		Vec3 const origin(pInfo->pListener->GetTransformation().GetPosition() + up* g_raySamplePositionsMedium[pInfo->rayIndex].z + side* g_raySamplePositionsMedium[pInfo->rayIndex].x);
		CastObstructionRay(origin, i, pInfo->rayIndex, bSynch, pInfo);
		++pInfo->rayIndex;
	}
}

//////////////////////////////////////////////////////////////////////////
void CPropagationProcessor::ProcessHigh(
	Vec3 const& up,
	Vec3 const& side,
	bool const bSynch,
	SOcclusionInfo* const pInfo)
{
	for (uint8 i = 0; i < g_numConcurrentRaysHigh; ++i)
	{
		if (pInfo->rayIndex >= g_numRaySamplePositionsHigh)
		{
			pInfo->rayIndex = 0;
		}

		Vec3 const origin(pInfo->pListener->GetTransformation().GetPosition() + up* g_raySamplePositionsHigh[pInfo->rayIndex].z + side* g_raySamplePositionsHigh[pInfo->rayIndex].x);
		CastObstructionRay(origin, i, pInfo->rayIndex, bSynch, pInfo);
		++pInfo->rayIndex;
	}
}

//////////////////////////////////////////////////////////////////////////
uint8 CPropagationProcessor::GetNumConcurrentRays(EOcclusionType const occlusionTypeWhenAdaptive) const
{
	uint8 numConcurrentRays = 0;

	switch (m_occlusionType)
	{
	case EOcclusionType::Adaptive:
		{
			switch (occlusionTypeWhenAdaptive)
			{
			case EOcclusionType::Low:
				{
					numConcurrentRays = g_numConcurrentRaysLow;
					break;
				}
			case EOcclusionType::Medium:
				{
					numConcurrentRays = g_numConcurrentRaysMedium;
					break;
				}
			case EOcclusionType::High:
				{
					numConcurrentRays = g_numConcurrentRaysHigh;
					break;
				}
			default:
				{
					CRY_ASSERT_MESSAGE(false, "Calculated Adaptive Occlusion Type invalid during %s", __FUNCTION__);
					break;
				}
			}

			break;
		}
	case EOcclusionType::Low:
		{
			numConcurrentRays = g_numConcurrentRaysLow;
			break;
		}
	case EOcclusionType::Medium:
		{
			numConcurrentRays = g_numConcurrentRaysMedium;
			break;
		}
	case EOcclusionType::High:
		{
			numConcurrentRays = g_numConcurrentRaysHigh;
			break;
		}
	default:
		{
			break;
		}
	}

	return numConcurrentRays;
}

//////////////////////////////////////////////////////////////////////////
uint8 CPropagationProcessor::GetNumSamplePositions(EOcclusionType const occlusionTypeWhenAdaptive) const
{
	uint8 numSamplePositions = 0;

	switch (m_occlusionType)
	{
	case EOcclusionType::Adaptive:
		{
			switch (occlusionTypeWhenAdaptive)
			{
			case EOcclusionType::Low:
				{
					numSamplePositions = g_numRaySamplePositionsLow;
					break;
				}
			case EOcclusionType::Medium:
				{
					numSamplePositions = g_numRaySamplePositionsMedium;
					break;
				}
			case EOcclusionType::High:
				{
					numSamplePositions = g_numRaySamplePositionsHigh;
					break;
				}
			default:
				{
					CRY_ASSERT_MESSAGE(false, "Calculated Adaptive Occlusion Type invalid during %s", __FUNCTION__);
					break;
				}
			}

			break;
		}
	case EOcclusionType::Low:
		{
			numSamplePositions = g_numRaySamplePositionsLow;
			break;
		}
	case EOcclusionType::Medium:
		{
			numSamplePositions = g_numRaySamplePositionsMedium;
			break;
		}
	case EOcclusionType::High:
		{
			numSamplePositions = g_numRaySamplePositionsHigh;
			break;
		}
	default:
		{
			break;
		}
	}

	return numSamplePositions;
}

//////////////////////////////////////////////////////////////////////////
void CPropagationProcessor::UpdateOcclusionPlanes()
{
	g_listenerHeadSize = g_cvars.m_occlusionListenerPlaneSize;
	g_listenerHeadSizeHalf = g_listenerHeadSize * 0.5f;
	uint8 index = 0;

	// Low
	float step = g_listenerHeadSize / (g_numberLow - 1);

	for (uint8 i = 0; i < g_numberLow; ++i)
	{
		float const z = g_listenerHeadSizeHalf - i * step;

		for (uint8 j = 0; j < g_numberLow; ++j)
		{
			float const x = -g_listenerHeadSizeHalf + j * step;
			g_raySamplePositionsLow[index].x = x;
			g_raySamplePositionsLow[index].z = z;

			// Initial sample positions:
			// . . . . . . .
			// . 8 . . . 12.
			// . . . . . . .
			// . . . 24. . .
			// . . . . . . .
			// . 36. . . 40.
			// . . . . . . .

			switch (index)
			{
			case 8:
				{
					g_initialRaySamplePositionTopLeft = index;
					g_initialRaySamplePositionsLow[0].x = x;
					g_initialRaySamplePositionsLow[0].z = z;

					break;
				}
			case 12:
				{
					g_initialRaySamplePositionTopRight = index;
					g_initialRaySamplePositionsLow[1].x = x;
					g_initialRaySamplePositionsLow[1].z = z;

					break;
				}
			case 24:
				{
					g_initialRaySamplePositionCenter = index;
					g_initialRaySamplePositionsLow[2].x = x;
					g_initialRaySamplePositionsLow[2].z = z;

					break;
				}
			case 36:
				{
					g_initialRaySamplePositionBottomLeft = index;
					g_initialRaySamplePositionsLow[3].x = x;
					g_initialRaySamplePositionsLow[3].z = z;

					break;
				}
			case 40:
				{
					g_initialRaySamplePositionBottomRight = index;
					g_initialRaySamplePositionsLow[4].x = x;
					g_initialRaySamplePositionsLow[4].z = z;

					break;
				}
			default:
				{
					break;
				}
			}

			++index;
		}
	}

	index = 0;

	// Medium
	step = g_listenerHeadSize / (g_numberMedium - 1);

	for (uint8 i = 0; i < g_numberMedium; ++i)
	{
		float const z = g_listenerHeadSizeHalf - i * step;

		for (uint8 j = 0; j < g_numberMedium; ++j)
		{
			float const x = -g_listenerHeadSizeHalf + j * step;
			g_raySamplePositionsMedium[index].x = x;
			g_raySamplePositionsMedium[index].z = z;

			// Initial sample positions:
			// . . . . . . . . .
			// . 10. . . . . 16.
			// . . . . . . . . .
			// . . . . . . . . .
			// . . . . 40. . . .
			// . . . . . . . . .
			// . . . . . . . . .
			// . 64. . . . . 70.
			// . . . . . . . . .

			switch (index)
			{
			case 10:
				{
					g_initialRaySamplePositionTopLeft = index;
					g_initialRaySamplePositionsMedium[0].x = x;
					g_initialRaySamplePositionsMedium[0].z = z;

					break;
				}
			case 16:
				{
					g_initialRaySamplePositionTopRight = index;
					g_initialRaySamplePositionsMedium[1].x = x;
					g_initialRaySamplePositionsMedium[1].z = z;
					break;
				}
			case 40:
				{
					g_initialRaySamplePositionCenter = index;
					g_initialRaySamplePositionsMedium[2].x = x;
					g_initialRaySamplePositionsMedium[2].z = z;

					break;
				}
			case 64:
				{
					g_initialRaySamplePositionBottomLeft = index;
					g_initialRaySamplePositionsMedium[3].x = x;
					g_initialRaySamplePositionsMedium[3].z = z;

					break;
				}
			case 70:
				{
					g_initialRaySamplePositionBottomRight = index;
					g_initialRaySamplePositionsMedium[4].x = x;
					g_initialRaySamplePositionsMedium[4].z = z;

					break;
				}
			default:
				{
					break;
				}
			}

			++index;
		}
	}

	index = 0;

	// High
	step = g_listenerHeadSize / (g_numberHigh - 1);

	for (uint8 i = 0; i < g_numberHigh; ++i)
	{
		float const z = g_listenerHeadSizeHalf - i * step;

		for (uint8 j = 0; j < g_numberHigh; ++j)
		{
			float const x = -g_listenerHeadSizeHalf + j * step;
			g_raySamplePositionsHigh[index].x = x;
			g_raySamplePositionsHigh[index].z = z;

			// Initial sample positions:
			// . . . . . . . . . . .
			// . 12. . . . . . . 20.
			// . . . . . . . . . . .
			// . . . . . . . . . . .
			// . . . . . . . . . . .
			// . . . . . 60. . . . .
			// . . . . . . . . . . .
			// . . . . . . . . . . .
			// . . . . . . . . . . .
			// .100. . . . . . .108.
			// . . . . . . . . . . .

			switch (index)
			{
			case 12:
				{
					g_initialRaySamplePositionTopLeft = index;
					g_initialRaySamplePositionsHigh[0].x = x;
					g_initialRaySamplePositionsHigh[0].z = z;

					break;
				}
			case 20:
				{
					g_initialRaySamplePositionTopRight = index;
					g_initialRaySamplePositionsHigh[1].x = x;
					g_initialRaySamplePositionsHigh[1].z = z;

					break;
				}
			case 60:
				{
					g_initialRaySamplePositionCenter = index;
					g_initialRaySamplePositionsHigh[2].x = x;
					g_initialRaySamplePositionsHigh[2].z = z;

					break;
				}
			case 100:
				{
					g_initialRaySamplePositionBottomLeft = index;
					g_initialRaySamplePositionsHigh[3].x = x;
					g_initialRaySamplePositionsHigh[3].z = z;

					break;
				}
			case 108:
				{
					g_initialRaySamplePositionBottomRight = index;
					g_initialRaySamplePositionsHigh[4].x = x;
					g_initialRaySamplePositionsHigh[4].z = z;

					break;
				}
			default:
				{
					break;
				}
			}

			++index;
		}
	}
}

	#if defined(CRY_AUDIO_USE_DEBUG_CODE)
//////////////////////////////////////////////////////////////////////////
void CPropagationProcessor::DrawDebugInfo(IRenderAuxGeom& auxGeom)
{
	for (auto const pInfo : m_occlusionInfos)
	{
		uint8 const numConcurrentRays = GetNumConcurrentRays(pInfo->occlusionTypeWhenAdaptive);

		for (uint8 i = 0; i < numConcurrentRays; ++i)
		{
			SRayDebugInfo const& rayDebugInfo = pInfo->rayDebugInfos[i];
			bool const isRayObstructed = (rayDebugInfo.numHits > 0);
			Vec3 const rayEnd = isRayObstructed ?
			                    rayDebugInfo.begin + (rayDebugInfo.end - rayDebugInfo.begin).GetNormalized() * rayDebugInfo.distanceToNearestObstacle :
			                    rayDebugInfo.end; // Only draw the ray to the first collision point.

			if ((g_cvars.m_drawDebug & Debug::EDrawFilter::OcclusionRays) != 0)
			{
				SAuxGeomRenderFlags const previousRenderFlags = auxGeom.GetRenderFlags();
				SAuxGeomRenderFlags newRenderFlags(e_Def3DPublicRenderflags);
				newRenderFlags.SetCullMode(e_CullModeNone);
				ColorF const& rayColor = isRayObstructed ? Debug::s_rayColorObstructed : Debug::s_rayColorFree;
				auxGeom.SetRenderFlags(newRenderFlags);
				auxGeom.DrawLine(rayDebugInfo.begin, rayColor, rayEnd, rayColor, 1.0f);
				auxGeom.SetRenderFlags(previousRenderFlags);
			}

			if ((g_cvars.m_drawDebug & Debug::EDrawFilter::OcclusionCollisionSpheres) != 0)
			{
				if (isRayObstructed)
				{
					pInfo->collisionSpherePositions[rayDebugInfo.samplePosIndex] = rayEnd;
				}
				else
				{
					pInfo->collisionSpherePositions[rayDebugInfo.samplePosIndex] = ZERO;
				}
			}
		}

		if ((g_cvars.m_drawDebug & Debug::EDrawFilter::OcclusionCollisionSpheres) != 0)
		{
			uint8 const numSamplePositions = GetNumSamplePositions(pInfo->occlusionTypeWhenAdaptive);

			for (uint8 i = 0; i < g_numRaySamplePositionsHigh; ++i)
			{
				auto& spherePos = pInfo->collisionSpherePositions[i];

				if (i < numSamplePositions)
				{
					if (!spherePos.IsZero())
					{
						auxGeom.DrawSphere(spherePos, Debug::g_rayRadiusCollisionSphere, pInfo->listenerOcclusionPlaneColor);
					}
				}
				else
				{
					spherePos = ZERO;
				}
			}
		}
	}
}

//////////////////////////////////////////////////////////////////////////
void CPropagationProcessor::DrawListenerPlane(IRenderAuxGeom& auxGeom)
{
	SAuxGeomRenderFlags const previousRenderFlags = auxGeom.GetRenderFlags();
	SAuxGeomRenderFlags newRenderFlags;
	newRenderFlags.SetDepthTestFlag(e_DepthTestOff);
	newRenderFlags.SetAlphaBlendMode(e_AlphaBlended);
	newRenderFlags.SetCullMode(e_CullModeNone);
	auxGeom.SetRenderFlags(newRenderFlags);

	for (SOcclusionInfo const* const pInfo : m_occlusionInfos)
	{
		Vec3 const& listenerPosition = pInfo->pListener->GetTransformation().GetPosition();

		// TODO: this breaks if listener and object x and y coordinates are exactly the same.
		Vec3 const side((listenerPosition - m_object.GetTransformation().GetPosition()).Cross(Vec3Constants<float>::fVec3_OneZ).normalize());
		Vec3 const up((listenerPosition - m_object.GetTransformation().GetPosition()).Cross(side).normalize());

		Vec3 const quadVertices[g_numPoints] =
		{
			Vec3(listenerPosition + up * g_listenerHeadSizeHalf + side * g_listenerHeadSizeHalf),
			Vec3(listenerPosition + up * -g_listenerHeadSizeHalf + side * g_listenerHeadSizeHalf),
			Vec3(listenerPosition + up * g_listenerHeadSizeHalf + side * -g_listenerHeadSizeHalf),
			Vec3(listenerPosition + up * -g_listenerHeadSizeHalf + side * -g_listenerHeadSizeHalf) };

		auxGeom.DrawTriangles(quadVertices, g_numPoints, g_auxIndices, g_numIndices, pInfo->listenerOcclusionPlaneColor);

	}

	auxGeom.SetRenderFlags(previousRenderFlags);
}

///////////////////////////////////////////////////////////////////////////
void CPropagationProcessor::ResetRayData()
{
	if ((m_occlusionType != EOcclusionType::None) && (m_occlusionType != EOcclusionType::Ignore))
	{
		for (auto const pInfo : m_occlusionInfos)
		{
			for (uint8 i = 0; i < g_numConcurrentRaysHigh; ++i)
			{
				pInfo->raysInfo[i].Reset();
				pInfo->rayDebugInfos[i] = SRayDebugInfo();
			}
		}
	}
}
	#endif // CRY_AUDIO_USE_DEBUG_CODE
}        // namespace CryAudio
#endif   // CRY_AUDIO_USE_OCCLUSION
