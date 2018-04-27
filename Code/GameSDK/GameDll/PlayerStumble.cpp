// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

/*************************************************************************

-------------------------------------------------------------------------
History:
- 21:07:2010  Created by Jens Schöbel

*************************************************************************/

#include "StdAfx.h"
#include "PlayerStumble.h"

using namespace PlayerActor::Stumble;

//////////////////////////////////////////////////////////////////////////
CPlayerStumble::CPlayerStumble()
	: m_finalActionImpulse(ZERO)
	, m_currentPeriodicScaleX(0.0f)
	, m_currentPeriodicScaleY(0.0f)
	, m_currentPastTime(-100.f)
{
	ZeroStruct(m_stumbleParameter);
}

//////////////////////////////////////////////////////////////////////////
void CPlayerStumble::Disable()
{
  m_currentPastTime = -100.f;
}

//////////////////////////////////////////////////////////////////////////
bool CPlayerStumble::Update(float deltaTime, const pe_status_dynamics &dynamics)
{
  if ( m_currentPastTime < -1.f )
    return false;

  m_currentPastTime += deltaTime;

  if ( m_currentPastTime > m_stumbleParameter.periodicTime )
  {
    m_currentPastTime -= m_stumbleParameter.periodicTime;
    m_currentPeriodicScaleX = 1.0f - cry_random(0.0f, m_stumbleParameter.randomness);
    m_currentPeriodicScaleY = 1.0f - cry_random(0.0f, m_stumbleParameter.randomness);
  }
  m_finalActionImpulse = -cry_random(m_stumbleParameter.minDamping, m_stumbleParameter.maxDamping) * dynamics.v;

  Vec3 stumblingImpulseDir;
  stumblingImpulseDir.x = cosf( m_currentPastTime / m_stumbleParameter.periodicTime * gf_PI2 ) * m_currentPeriodicScaleX * m_stumbleParameter.strengthX;
  stumblingImpulseDir.y = cosf( m_currentPastTime / m_stumbleParameter.periodicTime * gf_PI2 ) * m_currentPeriodicScaleY * m_stumbleParameter.strengthY;
  stumblingImpulseDir.z = 0;
  m_finalActionImpulse += stumblingImpulseDir;

  return true;
}

//////////////////////////////////////////////////////////////////////////
void CPlayerStumble::Start(StumbleParameters* stumbleParameters)
{

  m_currentPastTime = 0;

  m_stumbleParameter.maxDamping = clamp_tpl(stumbleParameters->maxDamping, 0.0f, 10000.0f);
  m_stumbleParameter.minDamping = clamp_tpl(stumbleParameters->minDamping, 0.0f, 10000.0f);

  if ( m_stumbleParameter.minDamping > m_stumbleParameter.maxDamping )
  {
    float fTemp = m_stumbleParameter.minDamping;
    m_stumbleParameter.minDamping = m_stumbleParameter.maxDamping;
    m_stumbleParameter.maxDamping = fTemp;
  }

  m_stumbleParameter.randomness   = clamp_tpl(stumbleParameters->randomness, 0.0f, 2.0f);
  m_stumbleParameter.periodicTime = stumbleParameters->periodicTime;
  m_stumbleParameter.strengthX    = stumbleParameters->strengthX;
  m_stumbleParameter.strengthY    = stumbleParameters->strengthY;

  m_currentPeriodicScaleX = 1.0f - cry_random(0.0f, stumbleParameters->randomness);
  m_currentPeriodicScaleY = 1.0f - cry_random(0.0f, stumbleParameters->randomness);
}
