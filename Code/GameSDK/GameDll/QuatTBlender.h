// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

/*************************************************************************
	-------------------------------------------------------------------------
	$Id$
	$DateTime$
	Description: 
		Basic reusable utility object to allow simple blending of a Quat T 
		Between a start + end location, over some custom blend period. 
	-------------------------------------------------------------------------
	History:
	- 01:09:2011  : Created by Jonathan Bunner

*************************************************************************/


#ifndef _QUATTBLENDER_H_
#define _QUATTBLENDER_H_

#if _MSC_VER > 1000
# pragma once
#endif

class QuatTBlender
{
public:

	// General
	QuatTBlender();
	~QuatTBlender(); 

	// Configure the new/existing QuatTBlender for a new blend 
	void Init(const QuatT& blendLocationStart, const QuatT&  blendLocationEnd, const float totalBlendPeriod);

	// Configure the new/existing QuatTBlender for a new blend (not caring about Translation) 
	void Init(const Quat& blendLocationStart, const Quat&  blendLocationEnd, const float totalBlendPeriod);

	// Updates T value (using total accumulated dt elapsed since Init() called) 
	// returns true if blend completed, false if not
	const bool	 UpdateBlendOverTime(const float dt);

	// Returns cached QuatT from last Blend Update
	const QuatT& GetCurrent() const;

	// Returns the currently elapsed time between [0.0f, totalBlendPeriod]
	const float  GetElapsedTime() const;

	// Returns the total period we are blending over
	const float  GetTotalBlendPeriod() const;

	// Returns true elapsed blend time >= totalBlendtime, else false
	const bool   BlendComplete() const;
	
private: 

	// The T-0 quatT (e.g. at tVal 0 - this is returned)
	QuatT m_blendLocationStart;	

	// The T-1 quatT (e.g. at tVal 1 - this is returned)
	QuatT m_blendLocationEnd;

	// The currently blended quatT (e.g. at tVal 0.5 a blended QuatT from start/end is generated)
	QuatT m_current; 

	// The time period over which the blend will 
	float m_totalBlendPeriod;

	// The T-0 quatT (e.g. at tVal 0 - this is returned)
	float m_blendTimeElapsed; 

	// Simple state var
	bool m_translationBlendRequired;
};

#endif // #ifndef _QUATTBLENDER_H_