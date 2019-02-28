// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "QuatTBlender.h"

QuatTBlender::QuatTBlender():
m_blendLocationStart(Quat::CreateIdentity(), Vec3(ZERO))
,m_blendLocationEnd(Quat::CreateIdentity(), Vec3(ZERO))
,m_current(Quat::CreateIdentity(), Vec3(ZERO))
,m_totalBlendPeriod(0.0f)
,m_blendTimeElapsed(0.0f)
,m_translationBlendRequired(true)
{
		
}

QuatTBlender::~QuatTBlender()
{

}

void QuatTBlender::Init( const QuatT& blendLocationStart, const QuatT& blendLocationEnd, const float totalBlendPeriod )
{
	m_blendLocationStart		= blendLocationStart;
	m_blendLocationEnd			= blendLocationEnd;
	m_current					= blendLocationStart; 
	m_totalBlendPeriod			= totalBlendPeriod;
	m_blendTimeElapsed			= 0.0f; 
	m_translationBlendRequired	= true; 

	// Validity
	CRY_ASSERT_MESSAGE( m_totalBlendPeriod >= 0.0f, "QuatTBlender::Init(...) < error param totalBlendPeriod is invalid");
}

void QuatTBlender::Init( const Quat& blendLocationStart, const Quat& blendLocationEnd, const float totalBlendPeriod )
{
	m_blendLocationStart.q		= blendLocationStart;
	m_blendLocationStart.t	    = ZERO; 

	m_blendLocationEnd.q		= blendLocationEnd;
	m_blendLocationEnd.t		= ZERO; 

	m_current.q					= blendLocationStart; 
	m_current.t					= ZERO; 

	m_totalBlendPeriod			= totalBlendPeriod;
	m_blendTimeElapsed			= 0.0f; 
	m_translationBlendRequired	= false; 

	// Validity
	CRY_ASSERT_MESSAGE( m_totalBlendPeriod >= 0.0f, "QuatTBlender::Init(...) < error param totalBlendPeriod is invalid");
}

const bool QuatTBlender::UpdateBlendOverTime( const float dt )
{
	CRY_ASSERT_MESSAGE( m_totalBlendPeriod >= 0.0f, "QuatTBlender::UpdateBlendOverTime(...) < error param totalBlendPeriod is invalid");

	// Calc new T val
	m_blendTimeElapsed += dt;
	float t						  = m_blendTimeElapsed/m_totalBlendPeriod;
	t = clamp_tpl(t, 0.0f, 1.0f); 
	
	if(t >= 1.0f)
	{
		m_current = m_blendLocationEnd;
	}
	else
	{
		// Calc blended pos /orientation
		if(m_translationBlendRequired)
		{
			m_current.t.SetLerp(m_blendLocationStart.t,m_blendLocationEnd.t,t);
		}
		m_current.q.SetSlerp(m_blendLocationStart.q,m_blendLocationEnd.q,t);
	}

	// Done - return boolean flag to indicate if blend complete
	return (t >= 1.0f);
}

const QuatT& QuatTBlender::GetCurrent() const
{
	return m_current; 
}


const float QuatTBlender::GetElapsedTime() const
{
	return m_blendTimeElapsed; 
}

const float QuatTBlender::GetTotalBlendPeriod() const
{
	return m_totalBlendPeriod; 
}

const bool QuatTBlender::BlendComplete() const
{
 return m_blendTimeElapsed >= m_totalBlendPeriod; 
}
