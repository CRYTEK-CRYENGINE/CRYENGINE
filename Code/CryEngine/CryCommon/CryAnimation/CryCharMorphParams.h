// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

//! \cond INTERNAL

#pragma once

//! StartMorph will accept this.
struct CryCharMorphParams
{

	CryCharMorphParams(
	  float _fBlendIn = 0.15f,
	  float _fLength = 0,
	  float _fBlendOut = 0.15f,
	  float _fAmplitude = 1,
	  float _fStartTime = 0,
	  float _fSpeed = 1,
	  uint32 _nFlags = 0
	  )
	{
		m_fBlendIn = _fBlendIn;
		m_fLength = _fLength;
		m_fBlendOut = _fBlendOut;
		m_fAmplitude = _fAmplitude;
		m_fStartTime = _fStartTime;
		m_fSpeed = _fSpeed;
		m_nFlags = _nFlags;
		m_fBalance = 0.0f;
	}

	//! The blend-in time.
	f32 m_fBlendIn;
	//! The time to stay in static position.
	f32 m_fLength;
	//! The blend-out time.
	f32 m_fBlendOut;
	//! The maximum amplitude.
	f32 m_fAmplitude;
	//! The initial time phase from which to start morphing, within the cycle.
	f32 m_fStartTime;
	//! Multiplier of speed of the update; 1 is default.
	f32 m_fSpeed;
	//! Balance between left/right morph target from -1 to 1.
	f32 m_fBalance;

	enum FlagsEnum
	{
		//! With this flag set, the morph will not be time-updated (it'll be frozen at the point where it is).
		FLAGS_FREEZE        = 0x01,
		FLAGS_NO_BLENDOUT   = 0x02,
		FLAGS_INSTANTANEOUS = 0x04
	};

	//! Optional flags, as specified by the FlagsEnum.
	uint32 m_nFlags;
};

//! \endcond