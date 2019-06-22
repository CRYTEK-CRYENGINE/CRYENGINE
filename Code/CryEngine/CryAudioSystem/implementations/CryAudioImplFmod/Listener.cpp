// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "stdafx.h"
#include "Listener.h"

namespace CryAudio
{
namespace Impl
{
namespace Fmod
{
//////////////////////////////////////////////////////////////////////////
CListener::CListener(CTransformation const& transformation, int const id)
	: m_id(id)
	, m_isMovingOrDecaying(false)
	, m_velocity(ZERO)
	, m_position(transformation.GetPosition())
	, m_previousPosition(transformation.GetPosition())
	, m_transformation(transformation)
{
	Fill3DAttributeTransformation(transformation, m_attributes);
	FMOD_RESULT const fmodResult = g_pStudioSystem->setListenerAttributes(id, &m_attributes);
	CRY_AUDIO_IMPL_FMOD_ASSERT_OK;
}

//////////////////////////////////////////////////////////////////////////
void CListener::Update(float const deltaTime)
{
	if (m_isMovingOrDecaying && (deltaTime > 0.0f))
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
				m_isMovingOrDecaying = false;
			}

			SetVelocity();
		}
		else
		{
			m_isMovingOrDecaying = false;
			SetVelocity();
		}
	}
}

//////////////////////////////////////////////////////////////////////////
void CListener::SetName(char const* const szName)
{
#if defined(CRY_AUDIO_IMPL_FMOD_USE_DEBUG_CODE)
	m_name = szName;
#endif  // CRY_AUDIO_IMPL_FMOD_USE_DEBUG_CODE
}

//////////////////////////////////////////////////////////////////////////
void CListener::SetTransformation(CTransformation const& transformation)
{
	m_transformation = transformation;
	m_position = transformation.GetPosition();

	Fill3DAttributeTransformation(m_transformation, m_attributes);

#if defined(CRY_AUDIO_IMPL_FMOD_USE_DEBUG_CODE)
	// Always update velocity in non-release builds for debug draw.
	m_isMovingOrDecaying = true;
	Fill3DAttributeVelocity(m_velocity, m_attributes);
#else
	if (g_numObjectsWithDoppler > 0)
	{
		m_isMovingOrDecaying = true;
		Fill3DAttributeVelocity(m_velocity, m_attributes);
	}
	else
	{
		m_previousPosition = m_position;
	}
#endif  // CRY_AUDIO_IMPL_FMOD_USE_DEBUG_CODE

	FMOD_RESULT const fmodResult = g_pStudioSystem->setListenerAttributes(m_id, &m_attributes);
	CRY_AUDIO_IMPL_FMOD_ASSERT_OK;
}

//////////////////////////////////////////////////////////////////////////
void CListener::SetVelocity()
{
	Fill3DAttributeVelocity(m_velocity, m_attributes);
	FMOD_RESULT const fmodResult = g_pStudioSystem->setListenerAttributes(m_id, &m_attributes);
	CRY_AUDIO_IMPL_FMOD_ASSERT_OK;
}
} // namespace Fmod
} // namespace Impl
} // namespace CryAudio