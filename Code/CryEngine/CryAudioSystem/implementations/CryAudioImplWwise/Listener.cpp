// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "stdafx.h"
#include "Listener.h"
#include "Common.h"

#include <AK/SoundEngine/Common/AkSoundEngine.h>

namespace CryAudio
{
namespace Impl
{
namespace Wwise
{
//////////////////////////////////////////////////////////////////////////
CListener::CListener(CTransformation const& transformation, AkGameObjectID const id)
	: m_id(id)
	, m_hasMoved(false)
	, m_isMovingOrDecaying(false)
	, m_velocity(ZERO)
	, m_position(transformation.GetPosition())
	, m_previousPosition(transformation.GetPosition())
	, m_transformation(transformation)
{
	AkListenerPosition listenerPos;
	FillAKListenerPosition(transformation, listenerPos);
	AK::SoundEngine::SetPosition(id, listenerPos);
}

//////////////////////////////////////////////////////////////////////////
void CListener::Update(float const deltaTime)
{
	if (m_isMovingOrDecaying && (deltaTime > 0.0f))
	{
		m_hasMoved = m_velocity.GetLengthSquared() > FloatEpsilon;
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
				m_isMovingOrDecaying = false;
			}
		}
		else
		{
			m_isMovingOrDecaying = false;
		}
	}
}

//////////////////////////////////////////////////////////////////////////
void CListener::SetName(char const* const szName)
{
#if defined(CRY_AUDIO_IMPL_WWISE_USE_DEBUG_CODE)
	m_name = szName;
#endif  // CRY_AUDIO_IMPL_WWISE_USE_DEBUG_CODE
}

//////////////////////////////////////////////////////////////////////////
void CListener::SetTransformation(CTransformation const& transformation)
{
	m_transformation = transformation;
	m_position = transformation.GetPosition();

	AkListenerPosition listenerPos;
	FillAKListenerPosition(transformation, listenerPos);

#if defined(CRY_AUDIO_IMPL_WWISE_USE_DEBUG_CODE)
	// Always update velocity in non-release builds for debug draw.
	m_isMovingOrDecaying = true;
#else
	if (g_numObjectsWithRelativeVelocity > 0)
	{
		m_isMovingOrDecaying = true;
	}
	else
	{
		m_previousPosition = m_position;
	}
#endif  // CRY_AUDIO_IMPL_WWISE_USE_DEBUG_CODE

	AK::SoundEngine::SetPosition(m_id, listenerPos);
}
} // namespace Wwise
} // namespace Impl
} // namespace CryAudio
