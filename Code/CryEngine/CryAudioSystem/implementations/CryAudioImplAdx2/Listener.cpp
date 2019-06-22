// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "stdafx.h"
#include "Listener.h"

namespace CryAudio
{
namespace Impl
{
namespace Adx2
{
//////////////////////////////////////////////////////////////////////////
CListener::CListener(CTransformation const& transformation, CriAtomEx3dListenerHn const pHandle)
	: m_pHandle(pHandle)
	, m_isMovingOrDecaying(false)
	, m_velocity(ZERO)
	, m_position(transformation.GetPosition())
	, m_previousPosition(transformation.GetPosition())
	, m_transformation(transformation)
{
	Fill3DAttributeTransformation(transformation, m_3dAttributes);
	criAtomEx3dListener_SetPosition(pHandle, &m_3dAttributes.pos);
	criAtomEx3dListener_SetOrientation(pHandle, &m_3dAttributes.fwd, &m_3dAttributes.up);
	criAtomEx3dListener_SetVelocity(pHandle, &m_3dAttributes.vel);
	criAtomEx3dListener_Update(pHandle);
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
#if defined(CRY_AUDIO_IMPL_ADX2_USE_DEBUG_CODE)
	m_name = szName;
#endif  // CRY_AUDIO_IMPL_ADX2_USE_DEBUG_CODE
}

//////////////////////////////////////////////////////////////////////////
void CListener::SetTransformation(CTransformation const& transformation)
{
	m_transformation = transformation;
	m_position = transformation.GetPosition();

	Fill3DAttributeTransformation(transformation, m_3dAttributes);

#if defined(CRY_AUDIO_IMPL_ADX2_USE_DEBUG_CODE)
	// Always update velocity in non-release builds for debug draw.
	m_isMovingOrDecaying = true;
	Fill3DAttributeVelocity(m_velocity, m_3dAttributes);
#else
	if (g_numObjectsWithDoppler > 0)
	{
		m_isMovingOrDecaying = true;
		Fill3DAttributeVelocity(m_velocity, m_3dAttributes);
	}
	else
	{
		m_previousPosition = m_position;
	}
#endif  // CRY_AUDIO_IMPL_ADX2_USE_DEBUG_CODE

	criAtomEx3dListener_SetPosition(m_pHandle, &m_3dAttributes.pos);
	criAtomEx3dListener_SetOrientation(m_pHandle, &m_3dAttributes.fwd, &m_3dAttributes.up);
	criAtomEx3dListener_SetVelocity(m_pHandle, &m_3dAttributes.vel);

	criAtomEx3dListener_Update(m_pHandle);
}

//////////////////////////////////////////////////////////////////////////
void CListener::SetVelocity()
{
	Fill3DAttributeVelocity(m_velocity, m_3dAttributes);
	criAtomEx3dListener_SetVelocity(m_pHandle, &m_3dAttributes.vel);
	criAtomEx3dListener_Update(m_pHandle);
}
} // namespace Adx2
} // namespace Impl
} // namespace CryAudio
