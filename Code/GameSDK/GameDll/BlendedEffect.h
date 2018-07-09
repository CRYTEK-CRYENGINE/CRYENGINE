// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

/*************************************************************************
-------------------------------------------------------------------------
$Id$
$DateTime$
Description:  BlendedEffect interface for ScreenEffects and some implementations

Includes also a wrapper implementation (based on ItemScheduler) that we should
use when creating new blend effects

-------------------------------------------------------------------------
History:
- 23:1:2008   Created by Benito G.R. - Refactor'd from John N. ScreenEffects.h/.cpp

*************************************************************************/

#ifndef _BLEND_EFFECTS_H_
#define _BLEND_EFFECTS_H_

#include <CryMemory/PoolAllocator.h>

//-BlendedEffects-----------------------
// BlendedEffect interface
struct IBlendedEffect 
{
public:
	virtual ~IBlendedEffect(){}
	//Summary
	// - called when the effect actually starts rather than when it is created.
	virtual void Init() {}; 

	//Summary
	//- t is from 0 to 1 and is the "amount" of effect to use.
	virtual void Update(float t) = 0; 

	virtual void Release() = 0;

};

// Use this template to create new blends when needed
// class YourBlendFX { void Init(float progress) { ... }; void Update(float point) {... } };
// and use a pBlendFX = CBlendedEffect<YourBlendFX>::Create(YourBlendFX(...));
template <class T>
class CBlendedEffect : public IBlendedEffect
{
	typedef stl::PoolAllocator<sizeof(T) + sizeof(void*), stl::PoolAllocatorSynchronizationSinglethreaded> Alloc;
	static Alloc m_alloc;

public:
	static void FreePool()
	{
		m_alloc.FreeMemory();
	}

public:
	static CBlendedEffect * Create()
	{
		return new (m_alloc.Allocate()) CBlendedEffect();
	}
	static CBlendedEffect * Create( const T& from )
	{
		return new (m_alloc.Allocate()) CBlendedEffect(from);
	}

	void Init() { m_impl.Init(); }
	void Update(float t) { m_impl.Update(t); }

	void Release() 
	{ 
		this->~CBlendedEffect();
		m_alloc.Deallocate(this);
	}
	void GetMemoryStatistics(ICrySizer * s) { s->Add(*this); }

private:
	T m_impl;

	CBlendedEffect() {}
	CBlendedEffect( const T& from ) : m_impl(from) {}
	~CBlendedEffect() {m_impl.Reset();}

};

template <class T>
typename CBlendedEffect<T>::Alloc CBlendedEffect<T>::m_alloc;

//-------------------BLENDED EFFECTS (add here new ones)--------------------------
class CFOVEffect
{
	public:
		CFOVEffect(float goalFOV);
		virtual ~CFOVEffect() {};

		virtual void Init();
		virtual void Update(float point);
		virtual void Reset();

	private:
	
		float m_currentFOV;
		float m_goalFOV;
		float m_startFOV;
};

// Post process effect
class CPostProcessEffect
{

 public:
		CPostProcessEffect(string paramName, float goalVal);
		virtual ~CPostProcessEffect() {};
	
		virtual void Init();
		virtual void Update(float point);
		virtual void Reset();

private:
	
		float m_startVal;
		float m_currentVal;
		float m_goalVal;
		string m_paramName;
};
#endif