// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

// -------------------------------------------------------------------------
//  File name:   ParticleSubEmitter.h
//  Version:     v1.00
//  Created:     20/04/2010 by Corey.
//  Description: Split out from ParticleEmitter.h
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////

#ifndef __particlesubemitter_h__
#define __particlesubemitter_h__
#pragma once

#include "ParticleEffect.h"
#include "ParticleContainer.h"
#include "ParticleEnviron.h"
#include "ParticleUtils.h"
#include <CryCore/BitFiddling.h>

class CParticleEmitter;
struct SParticleUpdateContext;
struct IAudioProxy;

//////////////////////////////////////////////////////////////////////////
class CRY_ALIGN(16) CParticleSubEmitter: public Cry3DEngineBase, public _plain_reference_target<int>
	// Maintains an emitter source state, emits particles to a container
	// Ref count increased only by emitted particles
{
public:

	CParticleSubEmitter(CParticleSource * pSource, CParticleContainer * pCont);
	~CParticleSubEmitter();

	ResourceParticleParams const& GetParams() const
	{ return m_pContainer->GetParams(); }
	CParticleContainer&           GetContainer() const
	{ return *m_pContainer; }
	CParticleSource&              GetSource() const
	{ assert(m_pSource && m_pSource->NumRefs()); return *m_pSource; }
	CParticleEmitter&             GetMain() const
	{ return GetContainer().GetMain(); }

	// State.
	bool IsActive() const
	{ return GetAge() >= m_fActivateAge; }
	void Deactivate();

	// Timing.
	float GetAge() const
	{ return GetSource().GetAge(); }
	float GetStartAge() const
	{ return m_fStartAge; }
	float GetRepeatAge() const
	{ return m_fRepeatAge; }
	float GetStopAge() const
	{ return m_fStopAge; }
	float GetParticleStopAge() const
	{ return GetStopAge() + GetParams().GetMaxParticleLife(); }
	float GetRelativeAge(float fAgeAdjust = 0.f) const
	{ return GetAgeRelativeTo(m_fStopAge, fAgeAdjust); }

	float GetStopAge(ParticleParams::ESoundControlTime eControl) const;
	float GetRelativeAge(ParticleParams::ESoundControlTime eControl, float fAgeAdjust = 0.f) const
	{ return GetAgeRelativeTo(min(GetStopAge(eControl), m_fRepeatAge), fAgeAdjust); }

	float GetStrength(float fAgeAdjust = 0.f, ParticleParams::ESoundControlTime eControl = ParticleParams::ESoundControlTime::EmitterLifeTime) const;

	Vec3 GetEmitFocusDir(const QuatTS &loc, float fStrength, Quat * pRot = 0) const;

	// Actions.
	void UpdateState(float fAgeAdjust = 0.f);
	void UpdateAudio();
	void ResetLoc()
	{ m_LastLoc.s = -1.f; }
	void SetLastLoc()
	{ m_LastLoc = GetSource().GetLocation(); }
	float GetParticleScale() const;
	bool GetMoveRelative(Vec3 & vPreTrans, QuatTS & qtMove) const;
	void UpdateForce();

	bool HasForce() const
	{ return (m_pForce != 0); }

	int EmitParticle(SParticleUpdateContext & context, const EmitParticleData &data, float fAge = 0.f, QuatTS * plocPreTransform = NULL);
	void EmitParticles(SParticleUpdateContext & context);

	uint32    GetEmitIndex() const
	{ return m_nEmitIndex; }
	uint16    GetSequence() const
	{ return m_nSequenceIndex; }
	CChaosKey GetChaosKey() const
	{ return m_ChaosKey; }

	void OffsetPosition(const Vec3& delta)
	{
		m_LastLoc.t += delta;
	}

	void GetMemoryUsage(ICrySizer* pSizer) const
	{
	}

private:
	// Associated structures.
	CParticleContainer* m_pContainer;         // Direct or shared container to emit particles into.
	_smart_ptr<CParticleSource> m_pSource;

	CryAudio::ControlId m_startAudioTriggerId;
	CryAudio::ControlId m_stopAudioTriggerId;
	CryAudio::ControlId m_audioParameterId;
	CryAudio::IObject* m_pIAudioObject;
	CryAudio::EOcclusionType m_currentAudioOcclusionType;
	bool m_bExecuteAudioTrigger;

	// State.
	float m_fStartAge;              // Relative age when scheduled to start (default 0).
	float m_fStopAge;               // Relative age when scheduled to end (fHUGE if never).
	float m_fRepeatAge;             // Relative age when scheduled to repeat (fHUGE if never).
	float m_fLastEmitAge;           // Age of emission of last particle.
	float m_fActivateAge;           // Cached age for last activation mode.

	CChaosKey m_ChaosKey;           // Seed for randomising; inited every pulse.
	QuatTS m_LastLoc;               // Location at time of last update.

	uint32 m_nEmitIndex;
	uint16 m_nSequenceIndex;

	// External objects.
	IPhysicalEntity* m_pForce;

	// Methods.
	void Initialize(float fAge);
	float GetAgeRelativeTo(float fStopAge, float fAgeAdjust = 0.f) const;
	void DeactivateAudio();
	float ComputeDensityIncrease(float fStrength, float fParticleLife, const QuatTS &locA, const QuatTS * plocB) const;
	Matrix34 GetEmitTM() const;

};

#endif // __particlesubemitter_h__
