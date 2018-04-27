// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#ifndef _LERP_PARAM_
#define _LERP_PARAM_

#pragma once

//==================================================================================================
// Name: CLerpParam
// Desc: Black boxed interpolating parameter
// Author: James Chilvers
//==================================================================================================
class CLerpParam
{
public:
	CLerpParam()
	{
		m_oldValue = 0.0f;
		m_newValue = 0.0f;
		m_currentLerp = 0.0f;
		m_lerpSpeed = 1.0f;
	}

	CLerpParam(float oldValue,float newValue,float currentLerp,float lerpSpeed)
	{
		m_oldValue = oldValue;
		m_newValue = newValue;
		m_currentLerp = currentLerp;
		m_lerpSpeed = lerpSpeed;
	}

	// Gets
	ILINE float GetValue() const { return LERP(m_oldValue,m_newValue,m_currentLerp); }
	ILINE float GetValueEaseInEaseOut() const
	{
		float currentLerpSquared = m_currentLerp * m_currentLerp;
		float easeInOutLerp = ( 3.0f * currentLerpSquared ) - ( 2.0f * currentLerpSquared * m_currentLerp );
		return LERP(m_oldValue,m_newValue,easeInOutLerp);
	}
	ILINE float GetOldValue() const { return m_oldValue; }
	ILINE float GetNewValue() const { return m_newValue; }
	ILINE float GetCurrentLerp() const { return m_currentLerp; }
	ILINE float GetLerpSpeed() const { return m_lerpSpeed; }
	ILINE bool HasFinishedLerping() const { return (m_currentLerp == 1.0f); }

	// Sets
	ILINE void Set(float oldValue,float newValue,float currentLerp,float lerpSpeed)
	{
		m_oldValue = oldValue;
		m_newValue = newValue;
		m_currentLerp = currentLerp;
		m_lerpSpeed = lerpSpeed;
	}

	ILINE void Set(float oldValue,float newValue,float currentLerp)
	{
		m_oldValue = oldValue;
		m_newValue = newValue;
		m_currentLerp = currentLerp;
	}

	ILINE void SetOldValue(float oldValue) { m_oldValue = oldValue; }
	ILINE void SetNewValue(float newValue) 
	{ 
		m_oldValue = GetValue();
		m_newValue = newValue; 
		m_currentLerp = 0.0f;
	}
	ILINE void SetCurrentLerp(float currentLerp) { m_currentLerp = currentLerp; }
	ILINE void SetLerpSpeed(float lerpSpeed) { m_lerpSpeed = lerpSpeed; }
	ILINE void SetOldValueToNewValue() { m_oldValue = m_newValue; }

	// Updates
	ILINE void UpdateLerp(float frameTime)
	{
		if(m_currentLerp < 1.0f)
		{
			if(m_newValue == m_oldValue)
			{
				m_currentLerp = 1.0f;
			}
			else
			{
				m_currentLerp = min(m_currentLerp + (frameTime * m_lerpSpeed),1.0f);
			}
		}
	}

private:
	float m_oldValue;
	float m_newValue;
	float m_currentLerp;
	float m_lerpSpeed;
};//------------------------------------------------------------------------------------------------

#endif // _LERP_PARAM_
