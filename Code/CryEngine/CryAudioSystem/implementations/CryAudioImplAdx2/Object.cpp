// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "stdafx.h"
#include "Object.h"

#include "Impl.h"
#include "Cue.h"
#include "CueInstance.h"
#include "Listener.h"
#include "Cvars.h"

#include <CryThreading/CryThread.h>

#if defined(CRY_AUDIO_IMPL_ADX2_USE_DEBUG_CODE)
	#include <Logger.h>
	#include <DebugStyle.h>
#endif  // CRY_AUDIO_IMPL_ADX2_USE_DEBUG_CODE

namespace CryAudio
{
namespace Impl
{
namespace Adx2
{
CryCriticalSection g_cs;

//////////////////////////////////////////////////////////////////////////
static void PlaybackEventCallback(void* pObj, CriAtomExPlaybackEvent playbackEvent, CriAtomExPlaybackInfoDetail const* pInfo)
{
	if ((playbackEvent != CriAtomExPlaybackEvent::CRIATOMEX_PLAYBACK_EVENT_ALLOCATE) && (pObj != nullptr))
	{
		auto const pObject = static_cast<CObject*>(pObj);

		{
			CryAutoLock<CryCriticalSection> const lock(CryAudio::Impl::Adx2::g_cs);

			CueInstances const& cueInstances = pObject->GetCueInstances();

			for (auto const pCueInstance : cueInstances)
			{
				if (pCueInstance->GetPlaybackId() == pInfo->id)
				{
					switch (playbackEvent)
					{
					case CriAtomExPlaybackEvent::CRIATOMEX_PLAYBACK_EVENT_FROM_NORMAL_TO_VIRTUAL:
						{
							pCueInstance->SetFlag(ECueInstanceFlags::IsVirtual);

							break;
						}
					case CriAtomExPlaybackEvent::CRIATOMEX_PLAYBACK_EVENT_FROM_VIRTUAL_TO_NORMAL:
						{
							pCueInstance->RemoveFlag(ECueInstanceFlags::IsVirtual);

							break;
						}
					case CriAtomExPlaybackEvent::CRIATOMEX_PLAYBACK_EVENT_REMOVE:
						{
							pCueInstance->SetFlag(ECueInstanceFlags::ToBeRemoved);

							break;
						}
					default:
						{
							break;
						}
					}

					break;
				}
			}
		}
	}
}

//////////////////////////////////////////////////////////////////////////
CObject::CObject(CTransformation const& transformation, CListener* const pListener)
	: m_pListener(pListener)
	, m_flags(EObjectFlags::IsVirtual) // Set to virtual because voices always start in virtual state.
	, m_occlusion(0.0f)
	, m_previousAbsoluteVelocity(0.0f)
	, m_transformation(transformation)
	, m_position(transformation.GetPosition())
	, m_previousPosition(transformation.GetPosition())
	, m_velocity(ZERO)
#if defined(CRY_AUDIO_IMPL_ADX2_USE_DEBUG_CODE)
	, m_absoluteVelocity(0.0f)
	, m_absoluteVelocityNormalized(0.0f)
#endif  // CRY_AUDIO_IMPL_ADX2_USE_DEBUG_CODE
{
	m_cueInstances.reserve(2);

	m_p3dSource = criAtomEx3dSource_Create(&g_3dSourceConfig, nullptr, 0);
	m_pPlayer = criAtomExPlayer_Create(&g_playerConfig, nullptr, 0);
	CRY_ASSERT_MESSAGE(m_pPlayer != nullptr, "m_pPlayer is null pointer during %s", __FUNCTION__);
	criAtomExPlayer_SetPlaybackEventCallback(m_pPlayer, PlaybackEventCallback, this);
	criAtomExPlayer_Set3dListenerHn(m_pPlayer, m_pListener->GetHandle());
	criAtomExPlayer_Set3dSourceHn(m_pPlayer, m_p3dSource);

	Fill3DAttributeTransformation(transformation, m_3dAttributes);
	criAtomEx3dSource_SetPosition(m_p3dSource, &m_3dAttributes.pos);
	criAtomEx3dSource_SetOrientation(m_p3dSource, &m_3dAttributes.fwd, &m_3dAttributes.up);
	criAtomEx3dSource_Update(m_p3dSource);
}

//////////////////////////////////////////////////////////////////////////
CObject::~CObject()
{
	if ((m_flags& EObjectFlags::TrackVelocityForDoppler) != 0)
	{
		CRY_ASSERT_MESSAGE(g_numObjectsWithDoppler > 0, "g_numObjectsWithDoppler is 0 but an object with doppler tracking still exists during %s", __FUNCTION__);
		g_numObjectsWithDoppler--;
	}

	criAtomExPlayer_Destroy(m_pPlayer);
	criAtomEx3dSource_Destroy(m_p3dSource);
}

//////////////////////////////////////////////////////////////////////////
void CObject::Update(float const deltaTime)
{
	EObjectFlags const previousFlags = m_flags;
	bool removedCueInstance = false;

	if (!m_cueInstances.empty())
	{
		m_flags |= EObjectFlags::IsVirtual;

		auto iter(m_cueInstances.begin());
		auto iterEnd(m_cueInstances.end());

		while (iter != iterEnd)
		{
			CCueInstance* const pCueInstance = *iter;

			if ((pCueInstance->GetFlags() & ECueInstanceFlags::ToBeRemoved) != 0)
			{
				ETriggerResult const result = ((pCueInstance->GetFlags() & ECueInstanceFlags::IsPending) != 0) ? ETriggerResult::Pending : ETriggerResult::Playing;
				gEnv->pAudioSystem->ReportFinishedTriggerConnectionInstance(pCueInstance->GetTriggerInstanceId(), result);
				g_pImpl->DestructCueInstance(pCueInstance);
				removedCueInstance = true;

				if (iter != (iterEnd - 1))
				{
					(*iter) = m_cueInstances.back();
				}

				m_cueInstances.pop_back();
				iter = m_cueInstances.begin();
				iterEnd = m_cueInstances.end();
			}
			else if ((pCueInstance->GetFlags() & ECueInstanceFlags::IsPending) != 0)
			{
				if (pCueInstance->PrepareForPlayback(*this))
				{
					ETriggerResult const result = ((pCueInstance->GetFlags() & ECueInstanceFlags::IsVirtual) == 0) ? ETriggerResult::Playing : ETriggerResult::Virtual;
					gEnv->pAudioSystem->ReportStartedTriggerConnectionInstance(pCueInstance->GetTriggerInstanceId(), result);

					UpdateVirtualState(pCueInstance);
				}

				++iter;
			}
			else
			{
				UpdateVirtualState(pCueInstance);

				++iter;
			}
		}
	}

	if ((previousFlags != m_flags) && !m_cueInstances.empty())
	{
		if (((previousFlags& EObjectFlags::IsVirtual) != 0) && ((m_flags& EObjectFlags::IsVirtual) == 0))
		{
			gEnv->pAudioSystem->ReportPhysicalizedObject(this);
		}
		else if (((previousFlags& EObjectFlags::IsVirtual) == 0) && ((m_flags& EObjectFlags::IsVirtual) != 0))
		{
			gEnv->pAudioSystem->ReportVirtualizedObject(this);
		}
	}

	if (removedCueInstance)
	{
		UpdateVelocityTracking();
	}

	if (((m_flags& EObjectFlags::MovingOrDecaying) != 0) && (deltaTime > 0.0f))
	{
		UpdateVelocities(deltaTime);
	}
}

//////////////////////////////////////////////////////////////////////////
void CObject::SetTransformation(CTransformation const& transformation)
{
	m_position = transformation.GetPosition();

	if (((m_flags& EObjectFlags::TrackAbsoluteVelocity) != 0) || ((m_flags& EObjectFlags::TrackVelocityForDoppler) != 0))
	{
		m_flags |= EObjectFlags::MovingOrDecaying;
	}
	else
	{
		m_previousPosition = m_position;
	}

	float const threshold = (m_pListener != nullptr) ? (m_position.GetDistance(m_pListener->GetPosition()) * g_cvars.m_positionUpdateThresholdMultiplier) : 0.0f;

	if (!m_transformation.IsEquivalent(transformation, threshold))
	{
		m_transformation = transformation;
		Fill3DAttributeTransformation(transformation, m_3dAttributes);

		criAtomEx3dSource_SetPosition(m_p3dSource, &m_3dAttributes.pos);
		criAtomEx3dSource_SetOrientation(m_p3dSource, &m_3dAttributes.fwd, &m_3dAttributes.up);

		if ((m_flags& EObjectFlags::TrackVelocityForDoppler) != 0)
		{
			Fill3DAttributeVelocity(m_velocity, m_3dAttributes);
			criAtomEx3dSource_SetVelocity(m_p3dSource, &m_3dAttributes.vel);
		}

		criAtomEx3dSource_Update(m_p3dSource);
	}
}

//////////////////////////////////////////////////////////////////////////
void CObject::SetOcclusion(IListener* const pIListener, float const occlusion, uint8 const numRemainingListeners)
{
	criAtomExPlayer_SetAisacControlById(m_pPlayer, g_occlusionAisacId, static_cast<CriFloat32>(occlusion));
	criAtomExPlayer_UpdateAll(m_pPlayer);

	m_occlusion = occlusion;
}

//////////////////////////////////////////////////////////////////////////
void CObject::StopAllTriggers()
{
#if defined(CRY_AUDIO_IMPL_ADX2_USE_DEBUG_CODE)
	for (auto const pCueInstance : m_cueInstances)
	{
		pCueInstance->StartFadeOut();
	}
#endif  // CRY_AUDIO_IMPL_ADX2_USE_DEBUG_CODE

	criAtomExPlayer_Stop(m_pPlayer);
}

//////////////////////////////////////////////////////////////////////////
ERequestStatus CObject::SetName(char const* const szName)
{
#if defined(CRY_AUDIO_IMPL_ADX2_USE_DEBUG_CODE)
	m_name = szName;
#endif  // CRY_AUDIO_IMPL_ADX2_USE_DEBUG_CODE

	return ERequestStatus::Success;
}

//////////////////////////////////////////////////////////////////////////
void CObject::AddListener(IListener* const pIListener)
{
	m_pListener = static_cast<CListener*>(pIListener);

	criAtomExPlayer_Stop(m_pPlayer);
	criAtomExPlayer_Set3dListenerHn(m_pPlayer, m_pListener->GetHandle());
}

//////////////////////////////////////////////////////////////////////////
void CObject::RemoveListener(IListener* const pIListener)
{
	if (m_pListener == static_cast<CListener*>(pIListener))
	{
		m_pListener = nullptr;
		criAtomExPlayer_Stop(m_pPlayer);
		criAtomExPlayer_Set3dListenerHn(m_pPlayer, nullptr);
	}
}

//////////////////////////////////////////////////////////////////////////
void CObject::DrawDebugInfo(IRenderAuxGeom& auxGeom, float const posX, float posY, char const* const szTextFilter)
{
#if defined(CRY_AUDIO_IMPL_ADX2_USE_DEBUG_CODE)

	if (((m_flags& EObjectFlags::TrackAbsoluteVelocity) != 0) || ((m_flags& EObjectFlags::TrackVelocityForDoppler) != 0))
	{
		bool isVirtual = false;
		// To do: add check for virtual states.

		if ((m_flags& EObjectFlags::TrackAbsoluteVelocity) != 0)
		{
			auxGeom.Draw2dLabel(
				posX,
				posY,
				Debug::g_objectFontSize,
				isVirtual ? Debug::s_globalColorVirtual : Debug::s_objectColorParameter,
				false,
				"[Adx2] %s: %2.2f m/s (%2.2f)\n",
				static_cast<char const*>(g_szAbsoluteVelocityAisacName),
				m_absoluteVelocity,
				m_absoluteVelocityNormalized);

			posY += Debug::g_objectLineHeight;
		}

		if ((m_flags& EObjectFlags::TrackVelocityForDoppler) != 0)
		{
			auxGeom.Draw2dLabel(
				posX,
				posY,
				Debug::g_objectFontSize,
				isVirtual ? Debug::s_globalColorVirtual : Debug::s_objectColorActive,
				false,
				"[Adx2] Doppler calculation enabled\n");
		}
	}

#endif  // CRY_AUDIO_IMPL_ADX2_USE_DEBUG_CODE
}

//////////////////////////////////////////////////////////////////////////
void CObject::AddCueInstance(CCueInstance* const pCueInstance)
{
	m_cueInstances.push_back(pCueInstance);
}

//////////////////////////////////////////////////////////////////////////
void CObject::StopCue(uint32 const cueId)
{
	for (auto const pCueInstance : m_cueInstances)
	{
		if (pCueInstance->GetCue().GetId() == cueId)
		{
			pCueInstance->Stop();
		}
	}
}

//////////////////////////////////////////////////////////////////////////
void CObject::PauseCue(uint32 const cueId)
{
	for (auto const pCueInstance : m_cueInstances)
	{
		if (pCueInstance->GetCue().GetId() == cueId)
		{
			pCueInstance->Pause();
		}
	}
}

//////////////////////////////////////////////////////////////////////////
void CObject::ResumeCue(uint32 const cueId)
{
	for (auto const pCueInstance : m_cueInstances)
	{
		if (pCueInstance->GetCue().GetId() == cueId)
		{
			pCueInstance->Resume();
		}
	}
}

//////////////////////////////////////////////////////////////////////////
void CObject::MutePlayer(CriBool const shouldMute)
{
	CriFloat32 const volume = (shouldMute == CRI_TRUE) ? 0.0f : 1.0f;
	criAtomExPlayer_SetVolume(m_pPlayer, volume);
	criAtomExPlayer_UpdateAll(m_pPlayer);
}

//////////////////////////////////////////////////////////////////////////
void CObject::PausePlayer(CriBool const shouldPause)
{
	criAtomExPlayer_Pause(m_pPlayer, shouldPause);
}

/////////////////////////////////////////////////////////////////////////
void CObject::UpdateVelocityTracking()
{
	bool trackAbsoluteVelocity = false;
	bool trackDoppler = false;

	CueInstances::iterator iter = m_cueInstances.begin();
	CueInstances::const_iterator iterEnd = m_cueInstances.end();

	while ((iter != iterEnd) && !(trackAbsoluteVelocity && trackDoppler))
	{
		CCueInstance* const pCueInstance = *iter;
		trackAbsoluteVelocity |= ((pCueInstance->GetFlags() & ECueInstanceFlags::HasAbsoluteVelocity) != 0);
		trackDoppler |= ((pCueInstance->GetFlags() & ECueInstanceFlags::HasDoppler) != 0);
		++iter;
	}

	if (trackAbsoluteVelocity)
	{
		if (g_cvars.m_maxVelocity > 0.0f)
		{
			m_flags |= EObjectFlags::TrackAbsoluteVelocity;
		}
#if defined(CRY_AUDIO_IMPL_ADX2_USE_DEBUG_CODE)
		else
		{
			Cry::Audio::Log(ELogType::Error, "Adx2 - Cannot enable absolute velocity tracking, because s_Adx2MaxVelocity is not greater than 0.");
		}
#endif    // CRY_AUDIO_IMPL_ADX2_USE_DEBUG_CODE
	}
	else
	{
		m_flags &= ~EObjectFlags::TrackAbsoluteVelocity;

		criAtomExPlayer_SetAisacControlById(m_pPlayer, g_absoluteVelocityAisacId, 0.0f);
		criAtomExPlayer_UpdateAll(m_pPlayer);

#if defined(CRY_AUDIO_IMPL_ADX2_USE_DEBUG_CODE)
		m_absoluteVelocity = 0.0f;
		m_absoluteVelocityNormalized = 0.0f;
#endif    // CRY_AUDIO_IMPL_ADX2_USE_DEBUG_CODE
	}

	if (trackDoppler)
	{
		if ((m_flags& EObjectFlags::TrackVelocityForDoppler) == 0)
		{
			m_flags |= EObjectFlags::TrackVelocityForDoppler;
			g_numObjectsWithDoppler++;
		}
	}
	else
	{
		if ((m_flags& EObjectFlags::TrackVelocityForDoppler) != 0)
		{
			m_flags &= ~EObjectFlags::TrackVelocityForDoppler;

			CriAtomExVector const zeroVelocity{ 0.0f, 0.0f, 0.0f };
			criAtomEx3dSource_SetVelocity(m_p3dSource, &zeroVelocity);
			criAtomEx3dSource_Update(m_p3dSource);

			CRY_ASSERT_MESSAGE(g_numObjectsWithDoppler > 0, "g_numObjectsWithDoppler is 0 but an object with doppler tracking still exists during %s", __FUNCTION__);
			g_numObjectsWithDoppler--;
		}
	}
}

//////////////////////////////////////////////////////////////////////////
void CObject::UpdateVirtualState(CCueInstance* const pCueInstance)
{
#if defined(CRY_AUDIO_IMPL_ADX2_USE_DEBUG_CODE)
	// Always update in production code for debug draw.
	if ((pCueInstance->GetFlags() & ECueInstanceFlags::IsVirtual) == 0)
	{
		m_flags &= ~EObjectFlags::IsVirtual;
	}
#else
	if (((m_flags& EObjectFlags::IsVirtual) != 0) && ((m_flags& EObjectFlags::UpdateVirtualStates) != 0))
	{
		if ((pCueInstance->GetFlags() & ECueInstanceFlags::IsVirtual) == 0)
		{
			m_flags &= ~EObjectFlags::IsVirtual;
		}
	}
#endif      // CRY_AUDIO_IMPL_ADX2_USE_DEBUG_CODE
}

///////////////////////////////////////////////////////////////////////////
void CObject::UpdateVelocities(float const deltaTime)
{
	Vec3 const deltaPos(m_position - m_previousPosition);

	if (!deltaPos.IsZero())
	{
		m_velocity = deltaPos / deltaTime;
		m_previousPosition = m_position;
	}
	else if (!m_velocity.IsZero())
	{
		// We did not move last frame, begin exponential decay towards zero.
		float const decay = std::max(1.0f - deltaTime / 0.05f, 0.0f);
		m_velocity *= decay;

		if (m_velocity.GetLengthSquared() < FloatEpsilon)
		{
			m_velocity = ZERO;
			m_flags &= ~EObjectFlags::MovingOrDecaying;
		}

		if ((m_flags& EObjectFlags::TrackVelocityForDoppler) != 0)
		{
			Fill3DAttributeVelocity(m_velocity, m_3dAttributes);
			criAtomEx3dSource_SetVelocity(m_p3dSource, &m_3dAttributes.vel);
			criAtomEx3dSource_Update(m_p3dSource);
		}
	}

	if ((m_flags& EObjectFlags::TrackAbsoluteVelocity) != 0)
	{
		float const absoluteVelocity = m_velocity.GetLength();

		if (absoluteVelocity == 0.0f || fabs(absoluteVelocity - m_previousAbsoluteVelocity) > g_cvars.m_velocityTrackingThreshold)
		{
			m_previousAbsoluteVelocity = absoluteVelocity;
			float const absoluteVelocityNormalized = (std::min(absoluteVelocity, g_cvars.m_maxVelocity) / g_cvars.m_maxVelocity);

			criAtomExPlayer_SetAisacControlById(m_pPlayer, g_absoluteVelocityAisacId, static_cast<CriFloat32>(absoluteVelocityNormalized));
			criAtomExPlayer_UpdateAll(m_pPlayer);

#if defined(CRY_AUDIO_IMPL_ADX2_USE_DEBUG_CODE)
			m_absoluteVelocity = absoluteVelocity;
			m_absoluteVelocityNormalized = absoluteVelocityNormalized;
#endif      // CRY_AUDIO_IMPL_ADX2_USE_DEBUG_CODE
		}
	}
}
} // namespace Adx2
} // namespace Impl
} // namespace CryAudio
