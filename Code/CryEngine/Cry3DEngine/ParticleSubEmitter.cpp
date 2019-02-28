// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "ParticleSubEmitter.h"
#include "ParticleEmitter.h"
#include <CryAudio/IObject.h>

static const float fMIN_PULSE_PERIOD = 0.1f;

//////////////////////////////////////////////////////////////////////////
// CParticleSubEmitter implementation.
//////////////////////////////////////////////////////////////////////////

CParticleSubEmitter::CParticleSubEmitter(CParticleSource* pSource, CParticleContainer* pCont)
	: m_ChaosKey(0U)
	, m_nEmitIndex(0)
	, m_nSequenceIndex(0)
	, m_pContainer(pCont)
	, m_pSource(pSource)
	, m_startAudioTriggerId(CryAudio::InvalidControlId)
	, m_stopAudioTriggerId(CryAudio::InvalidControlId)
	, m_audioParameterId(CryAudio::InvalidControlId)
	, m_pIAudioObject(nullptr)
	, m_currentAudioOcclusionType(CryAudio::EOcclusionType::Ignore)
	, m_bExecuteAudioTrigger(false)
{
	assert(pCont);
	assert(pSource);
	SetLastLoc();
	m_fActivateAge = m_fLastEmitAge = -fHUGE;
	m_fStartAge = m_fStopAge = m_fRepeatAge = fHUGE;
	m_pForce = NULL;
}

CParticleSubEmitter::~CParticleSubEmitter()
{
	Deactivate();
}

//////////////////////////////////////////////////////////////////////////
void CParticleSubEmitter::Initialize(float fAge)
{
	const ResourceParticleParams& params = GetParams();

	m_nEmitIndex = 0;
	m_nSequenceIndex = m_pContainer->GetNextEmitterSequence();
	SetLastLoc();

	// Reseed randomness.
	m_ChaosKey = CChaosKey(cry_random_uint32());
	m_fActivateAge = fAge;

	// Compute lifetime params.
	m_fStartAge = m_fStopAge = m_fActivateAge + params.fSpawnDelay(VRANDOM);
	m_fLastEmitAge = -fHUGE;
	if (params.bContinuous)
	{
		if (params.fEmitterLifeTime)
			m_fStopAge += params.fEmitterLifeTime(VRANDOM);
		else
			m_fStopAge = fHUGE;
	}

	// Compute next repeat age.
	if (params.fPulsePeriod)
	{
		float fRepeat = params.fPulsePeriod(VRANDOM);
		fRepeat = max(fRepeat, fMIN_PULSE_PERIOD);
		m_fRepeatAge = m_fActivateAge + fRepeat;
		m_fStopAge = min(m_fStopAge, m_fRepeatAge);
	}
	else
		m_fRepeatAge = fHUGE;

	m_bExecuteAudioTrigger = true;
}

//////////////////////////////////////////////////////////////////////////
void CParticleSubEmitter::Deactivate()
{
	DeactivateAudio();

	if (m_pForce)
	{
		assert(!JobManager::IsWorkerThread());
		GetPhysicalWorld()->DestroyPhysicalEntity(m_pForce);
		m_pForce = 0;
	}
}

void CParticleSubEmitter::UpdateState(float fAgeAdjust)
{
	// Evolve emitter state.
	m_fActivateAge += fAgeAdjust;
	m_fLastEmitAge += fAgeAdjust;

	float fAge = GetAge();

	ParticleParams::ESpawn eSpawn = GetParams().eSpawnIndirection;
	float fActivateAge = eSpawn == eSpawn.ParentCollide ? GetSource().GetCollideAge()
	                     : eSpawn == eSpawn.ParentDeath ? GetSource().GetStopAge()
	                     : 0.f;
	if (fActivateAge > m_fActivateAge && fAge >= fActivateAge)
		Initialize(fActivateAge);

	m_fStopAge = min(m_fStopAge, GetSource().GetStopAge());
}

float CParticleSubEmitter::GetStopAge(ParticleParams::ESoundControlTime eControl) const
{
	switch (eControl)
	{
	default:
	case ParticleParams::ESoundControlTime::EmitterLifeTime:
		return GetStopAge();
	case ParticleParams::ESoundControlTime::EmitterExtendedLifeTime:
		return GetParticleStopAge();
	case ParticleParams::ESoundControlTime::EmitterPulsePeriod:
		return m_fRepeatAge;
	}
}

float CParticleSubEmitter::GetAgeRelativeTo(float fStopAge, float fAgeAdjust) const
{
	float fAge = GetAge() + fAgeAdjust;
	if (min(fAge, fStopAge) <= m_fStartAge)
		return 0.f;

	return div_min(fAge - m_fStartAge, fStopAge - m_fStartAge, 1.f);
}

float CParticleSubEmitter::GetStrength(float fAgeAdjust /* = 0.f */, ParticleParams::ESoundControlTime const eControl /* = EmitterLifeTime */) const
{
	float fStrength = GetMain().GetSpawnParams().fStrength;
	if (fStrength < 0.f)
	{
		if (GetParams().bContinuous)
			return GetRelativeAge(eControl, fAgeAdjust);
		else
			return div_min((float)m_nEmitIndex, GetParams().fCount.GetMaxValue(), 1.f);
	}
	else
		return min(fStrength, 1.f);
}

void CParticleSubEmitter::EmitParticles(SParticleUpdateContext& context)
{
	const ResourceParticleParams& params = GetParams();

	context.fDensityAdjust = 1.f;

	float fAge = GetAge();
	const float fFullLife = GetContainer().GetMaxParticleFullLife() + GetParticleTimer()->GetFrameTime() * 2;

	// Iterate through one or more pulse periods
	// Compute max possible pulses as an extra check to avoid infinite looping
	int nMaxPulses = 1;
	if (params.fPulsePeriod)
	{
		const float fMinPulsePeriod = max(params.fPulsePeriod(VMIN), fMIN_PULSE_PERIOD);
		nMaxPulses += int_ceil(context.fUpdateTime / fMinPulsePeriod);
	}

	while (nMaxPulses-- > 0)
	{
		// Determine time window to update.
		float fAge0 = max(m_fLastEmitAge, m_fStartAge);
		float fAge1 = min(fAge, m_fStopAge);

		// Skip time before emitted particles would still be alive.
		fAge0 = max(fAge0, fAge - fFullLife);

		if (fAge1 > m_fLastEmitAge && fAge0 <= fAge1)
		{
			const float fStrength = GetStrength((fAge0 + fAge1) * 0.5f - fAge);
			float fCount = params.fCount(VRANDOM, fStrength);
			if (!GetContainer().GetParent())
				fCount *= GetMain().GetEmitCountScale();
			if (fCount > 0.f)
			{
				const float fEmitterLife = m_fStopAge - m_fStartAge;
				float fParticleLife = params.fParticleLifeTime(VMAX, fStrength);
				if (fParticleLife == 0.f)
					fParticleLife = fEmitterLife;

				if (fParticleLife > 0.f)
				{
					EmitParticleData data;
					data.Location = GetSource().GetLocation();

					if (params.bContinuous && fEmitterLife > 0.f)
					{
						// Continuous emission rate which maintains fCount particles
						float fAgeIncrement = min(fParticleLife, fEmitterLife) / fCount;

						// Set up location interpolation.
						const QuatTS& locA = GetSource().GetLocation();
						const QuatTS& locB = m_LastLoc;

						struct
						{
							bool  bInterp;
							bool  bSlerp;
							float fInterp;
							float fInterpInc;
							Vec3  vArc;
							float fA;
							float fCosA;
							float fInvSinA;
						} Interp;

						Interp.bInterp = context.fUpdateTime > 0.f && !params.bMoveRelativeEmitter
						                 && m_LastLoc.s >= 0.f && !IsEquivalent(locA, locB, 0.0045f, 1e-5f);

						if (params.fMaintainDensity && (Interp.bInterp || params.nEnvFlags & ENV_PHYS_AREA))
						{
							float fAdjust = ComputeDensityIncrease(fStrength, fParticleLife, locA, Interp.bInterp ? &locB : 0);
							if (fAdjust > 1.f)
							{
								fAdjust = Lerp(1.f, fAdjust, params.fMaintainDensity);
								float fInvAdjust = 1.f / fAdjust;
								fAgeIncrement *= fInvAdjust;
								context.fDensityAdjust = Lerp(1.f, fInvAdjust, params.fMaintainDensity.fReduceAlpha);
							}
						}

						if (Interp.bInterp)
						{
							Interp.bSlerp = false;
							Interp.fInterp = div_min(fAge - fAge0, context.fUpdateTime, 1.f);
							Interp.fInterpInc = -div_min(fAgeIncrement, context.fUpdateTime, 1.f);

							if (!(GetCVars()->e_ParticlesDebug & AlphaBit('q')))
							{
								/*
								    Spherically interpolate based on rotation changes and velocity.
								    Instead of interpolating linearly along path (P0,P1,t);
								    Interpolate along an arc:

								    (P0,P1,y) + C x, where
								    a = half angle of arc segment = angle(R0,R1) / 2 = acos (R1 ~R0).w
								    C = max arc extension, perpendicular to (P0,P1)
								    = (P0,P1)/2 ^ (R1 ~R0).xyz.norm (1 - cos a)
								    y = (sin (a (2t-1)) / sin(a) + 1)/2
								    x = cos (a (2t-1)) - cos a
								 */

								Vec3 vVelB = GetSource().GetVelocity().vLin;
								Vec3 vDeltaAdjusted = locB.t - locA.t - vVelB * context.fUpdateTime;
								if (!vDeltaAdjusted.IsZero())
								{
									Quat qDelta = locB.q * locA.q.GetInverted();
									float fSinA = qDelta.v.GetLength();
									if (fSinA > 0.001f)
									{
										Interp.bSlerp = true;
										if (qDelta.w < 0.f)
											qDelta = -qDelta;
										Interp.fA = asin_tpl(fSinA);
										Interp.fInvSinA = 1.f / fSinA;
										Interp.fCosA = qDelta.w;
										Interp.vArc = vDeltaAdjusted ^ qDelta.v;
										Interp.vArc *= Interp.fInvSinA * Interp.fInvSinA * 0.5f;
									}
								}
							}
						}

						if (params.Connection.bConnectToOrigin)
							fAge -= fAgeIncrement;

						float fPast = fAge - fAge0, fMinPast = fAge - fAge1;
						for (; fPast > fMinPast; fPast -= fAgeIncrement)
						{
							if (Interp.bInterp)
							{
								// Interpolate the location linearly.
								data.Location.q.SetNlerp(locA.q, locB.q, Interp.fInterp);
								data.Location.s = locA.s + (locB.s - locA.s) * Interp.fInterp;

								if (Interp.bSlerp)
								{
									float fX, fY;
									sincos_tpl(Interp.fA * (2.f * Interp.fInterp - 1.f), &fY, &fX);
									fY = (fY * Interp.fInvSinA + 1.f) * 0.5f;
									fX -= Interp.fCosA;

									data.Location.t.SetLerp(locA.t, locB.t, fY);
									data.Location.t += Interp.vArc * fX;
								}
								else
								{
									data.Location.t.SetLerp(locA.t, locB.t, Interp.fInterp);
								}
								Interp.fInterp += Interp.fInterpInc;
							}

							if (!EmitParticle(context, data, fPast))
							{
								GetContainer().GetCounts().particles.reject += (fPast - fMinPast) / fAgeIncrement;
								break;
							}
						}
						m_fLastEmitAge = fAge - fPast;
					}
					else
					{
						// Instant emission of fCount particles
						for (int nEmit = int_round(fCount); nEmit > 0; nEmit--)
						{
							if (!EmitParticle(context, data, fAge - m_fStartAge))
							{
								GetContainer().GetCounts().particles.reject += nEmit;
								break;
							}
						}
						m_fLastEmitAge = fAge;
					}
				}
			}
		}

		// Check if next pulse also occurs in this update
		if (fAge >= m_fRepeatAge && GetSource().GetStopAge() > m_fRepeatAge)
			Initialize(m_fRepeatAge);
		else
			break;
	}

	SetLastLoc();
}

float CParticleSubEmitter::GetParticleScale() const
{
	return GetParams().bMoveRelativeEmitter.ScaleWithSize() ?
	       GetSource().GetLocation().s :
	       GetMain().GetParticleScale();
}

static inline float AdjustedScale(const QuatTS& loc)
{
	float fMinScale = max(loc.t.GetLengthFast(), 1.f) * 1e-5f;
	return loc.s + fMinScale;
}

bool CParticleSubEmitter::GetMoveRelative(Vec3& vPreTrans, QuatTS& qtsMove) const
{
	if (m_LastLoc.s < 0.f)
		return false;

	ParticleParams const& params = GetParams();

	vPreTrans = -m_LastLoc.t;
	const QuatTS& locCur = GetSource().GetLocation();

	if (!params.bMoveRelativeEmitter.bIgnoreRotation)
		qtsMove.q = locCur.q * m_LastLoc.q.GetInverted();
	else
		qtsMove.q.SetIdentity();

	if (!params.bMoveRelativeEmitter.bIgnoreSize)
		qtsMove.s = AdjustedScale(locCur) / AdjustedScale(m_LastLoc);
	else
		qtsMove.s = 1.f;

	qtsMove.t = locCur.t;
	return true;
}

void CParticleSubEmitter::UpdateForce()
{
	CRY_PROFILE_FUNCTION(PROFILE_PARTICLE);

	// Set or clear physical force.

	float fAge = GetAge();
	if (fAge >= m_fStartAge && fAge <= GetParticleStopAge())
	{
		struct SForceGeom
		{
			QuatTS qpLoc;               // world placement of force
			AABB   bbOuter, bbInner;    // local boundaries of force.
			Vec3   vForce3;
			float  fForceW;
		} force;

		//
		// Compute force geom.
		//

		const ResourceParticleParams& params = GetParams();

		float fStrength = GetStrength();
		SPhysEnviron const& PhysEnv = GetMain().GetPhysEnviron();

		// Location.
		force.qpLoc = GetSource().GetLocation();
		force.qpLoc.s = GetParticleScale();
		force.qpLoc.t = force.qpLoc * params.vPositionOffset;

		// Direction.
		GetEmitFocusDir(force.qpLoc, fStrength, &force.qpLoc.q);

		force.qpLoc.t = force.qpLoc * params.vPositionOffset;

		// Set inner box from spawn geometry.
		Quat qToLocal = force.qpLoc.q.GetInverted() * m_pSource->GetLocation().q;
		Vec3 vOffset = qToLocal * Vec3(params.vRandomOffset);
		force.bbInner = AABB(-vOffset, vOffset);

		// Emission directions.
		float fPhiMax = DEG2RAD(params.fEmitAngle(VMAX, fStrength)),
		      fPhiMin = DEG2RAD(params.fEmitAngle(VMIN, fStrength));

		AABB bbTrav;
		bbTrav.max.y = cosf(fPhiMin);
		bbTrav.min.y = cosf(fPhiMax);
		float fCosAvg = (bbTrav.max.y + bbTrav.min.y) * 0.5f;
		bbTrav.max.x = bbTrav.max.z = (bbTrav.min.y * bbTrav.max.y < 0.f ? 1.f : sin(fPhiMax));
		bbTrav.min.x = bbTrav.min.z = -bbTrav.max.x;
		bbTrav.Add(Vec3(ZERO));

		// Force magnitude: speed times relative particle density.
		float fPLife = params.fParticleLifeTime(0.5f, fStrength);
		float fTime = fAge - m_fStartAge;

		float fSpeed = params.fSpeed(0.5f, fStrength) * GetMain().GetSpawnParams().fSpeedScale;
		float fDist = Travel::TravelDistance(abs(fSpeed), params.fAirResistance(0.5f, fStrength, 0.5f), min(fTime, fPLife));
		fSpeed = Travel::TravelSpeed(abs(fSpeed), params.fAirResistance(0.5f, fStrength, 0.5f), min(fTime, fPLife));
		float fForce = fSpeed * params.fAlpha(0.5f, fStrength, 0.5f) * force.qpLoc.s;

		if (params.bContinuous && fPLife > 0.f)
		{
			// Ramp up/down over particle life.
			float fStopAge = GetStopAge();
			if (fTime < fPLife)
				fForce *= fTime / fPLife;
			else if (fTime > fStopAge)
				fForce *= 1.f - (fTime - fStopAge) / fPLife;
		}

		// Force direction.
		force.vForce3.zero();
		force.vForce3.y = fCosAvg * fForce;
		force.fForceW = sqrtf(1.f - square(fCosAvg)) * fForce;

		// Travel distance.
		bbTrav.min *= fDist;
		bbTrav.max *= fDist;

		// Set outer box.
		force.bbOuter = force.bbInner;
		force.bbOuter.Augment(bbTrav);

		// Expand by size.
		float fSize = params.fSize(0.5f, fStrength, VMAX);
		force.bbOuter.Expand(Vec3(fSize));

		// Scale: Normalise box size, so we can handle some geom changes through scaling.
		Vec3 vSize = force.bbOuter.GetSize() * 0.5f;
		float fRadius = max(max(vSize.x, vSize.y), vSize.z);

		force.qpLoc.s *= fRadius;

		if (fForce * force.qpLoc.s == 0.f)
		{
			// No force.
			if (m_pForce)
			{
				GetPhysicalWorld()->DestroyPhysicalEntity(m_pForce);
				m_pForce = NULL;
			}
			return;
		}

		float fIRadius = 1.f / fRadius;
		force.bbOuter.min *= fIRadius;
		force.bbOuter.max *= fIRadius;
		force.bbInner.min *= fIRadius;
		force.bbInner.max *= fIRadius;

		//
		// Create physical area for force.
		//

		primitives::box geomBox;
		geomBox.Basis.SetIdentity();
		geomBox.bOriented = 0;
		geomBox.center = force.bbOuter.GetCenter();
		geomBox.size = force.bbOuter.GetSize() * 0.5f;

		pe_status_pos spos;
		if (m_pForce)
		{
			// Check whether shape changed.
			m_pForce->GetStatus(&spos);
			if (spos.pGeom)
			{
				primitives::box curBox;
				spos.pGeom->GetBBox(&curBox);
				if (!IsEquivalent(curBox.center, geomBox.center, 0.001f)
				    || !IsEquivalent(curBox.size, geomBox.size, 0.001f))
					spos.pGeom = NULL;
			}
			if (!spos.pGeom)
			{
				GetPhysicalWorld()->DestroyPhysicalEntity(m_pForce);
				m_pForce = NULL;
			}
		}

		if (!m_pForce)
		{
			IGeometry* pGeom = m_pPhysicalWorld->GetGeomManager()->CreatePrimitive(primitives::box::type, &geomBox);
			m_pForce = m_pPhysicalWorld->AddArea(pGeom, force.qpLoc.t, force.qpLoc.q, force.qpLoc.s);
			if (!m_pForce)
				return;

			// Tag area with this emitter, so we can ignore it in the emitter family.
			pe_params_foreign_data fd;
			fd.pForeignData = (void*)&GetMain();
			fd.iForeignData = fd.iForeignFlags = 0;
			m_pForce->SetParams(&fd);
		}
		else
		{
			// Update position & box size as needed.
			if (!IsEquivalent(spos.pos, force.qpLoc.t, 0.01f)
			    || !IsEquivalent(spos.q, force.qpLoc.q)
			    || spos.scale != force.qpLoc.s)
			{
				pe_params_pos pos;
				pos.pos = force.qpLoc.t;
				pos.q = force.qpLoc.q;
				pos.scale = force.qpLoc.s;
				m_pForce->SetParams(&pos);
			}
		}

		// To do: 4D flow
		pe_params_area area;
		float fVMagSqr = force.vForce3.GetLengthSquared(),
		      fWMagSqr = square(force.fForceW);
		float fMag = sqrtf(fVMagSqr + fWMagSqr);
		area.bUniform = (fVMagSqr > fWMagSqr) * 2;
		if (area.bUniform)
		{
			force.vForce3 *= fMag * isqrt_tpl(fVMagSqr);
		}
		else
		{
			force.vForce3.z = fMag * (force.fForceW < 0.f ? -1.f : 1.f);
			force.vForce3.x = force.vForce3.y = 0.f;
		}
		area.falloff0 = force.bbInner.GetRadius();

		if (params.eForceGeneration == params.eForceGeneration.Gravity)
			area.gravity = force.vForce3;
		m_pForce->SetParams(&area);

		if (params.eForceGeneration == params.eForceGeneration.Wind)
		{
			pe_params_buoyancy buoy;
			buoy.iMedium = 1;
			buoy.waterDensity = buoy.waterResistance = 1;
			buoy.waterFlow = force.vForce3;
			buoy.waterPlane.n = -PhysEnv.m_UniformForces.plWater.n;
			buoy.waterPlane.origin = PhysEnv.m_UniformForces.plWater.n * PhysEnv.m_UniformForces.plWater.d;
			m_pForce->SetParams(&buoy);
		}
	}
	else
	{
		if (m_pForce)
		{
			GetPhysicalWorld()->DestroyPhysicalEntity(m_pForce);
			m_pForce = NULL;
		}
	}
}

void CParticleSubEmitter::UpdateAudio()
{
	CRY_PROFILE_FUNCTION(PROFILE_PARTICLE);
	SpawnParams const& spawnParams = GetMain().GetSpawnParams();

	if (spawnParams.bEnableAudio && GetCVars()->e_ParticlesAudio > 0)
	{
		ResourceParticleParams const& params = GetParams();

		if (m_pIAudioObject != nullptr)
		{
			if (m_audioParameterId != CryAudio::InvalidControlId && params.fSoundFXParam(VMAX, VMIN) < 1.0f)
			{
				float const value = params.fSoundFXParam(m_ChaosKey, GetStrength(0.0f, params.eSoundControlTime));
				m_pIAudioObject->SetParameter(m_audioParameterId, value);
			}

			m_pIAudioObject->SetTransformation(GetEmitTM());

			if (m_currentAudioOcclusionType != spawnParams.occlusionType)
			{
				m_pIAudioObject->SetOcclusionType(spawnParams.occlusionType);
				m_currentAudioOcclusionType = spawnParams.occlusionType;
			}

			if (m_bExecuteAudioTrigger)
			{
				m_pIAudioObject->SetCurrentEnvironments();
				m_pIAudioObject->ExecuteTrigger(m_startAudioTriggerId);
			}
		}
		else if (m_bExecuteAudioTrigger)
		{
			float fAge = GetAge();
			float fAge0 = m_fStartAge;
			float fAge1 = m_fStopAge + GetParticleTimer()->GetFrameTime();
			if (fAge >= fAge0 && fAge <= fAge1)
			{
				if (!params.sStartTrigger.empty())
				{
					m_startAudioTriggerId = CryAudio::StringToId(params.sStartTrigger.c_str());
				}

				if (!params.sStopTrigger.empty())
				{
					m_stopAudioTriggerId = CryAudio::StringToId(params.sStopTrigger.c_str());
				}

				if (m_startAudioTriggerId != CryAudio::InvalidControlId || m_stopAudioTriggerId != CryAudio::InvalidControlId)
				{
					CryAudio::SCreateObjectData const objectData("ParticleSubEmitter", spawnParams.occlusionType, GetEmitTM(), INVALID_ENTITYID, true);
					m_pIAudioObject = gEnv->pAudioSystem->CreateObject(objectData);
					m_currentAudioOcclusionType = spawnParams.occlusionType;

					if (!spawnParams.audioRtpc.empty())
					{
						m_audioParameterId = CryAudio::StringToId(spawnParams.audioRtpc.c_str());
						float const value = params.fSoundFXParam(m_ChaosKey, GetStrength(0.0f, params.eSoundControlTime));
						m_pIAudioObject->SetParameter(m_audioParameterId, value);
					}

					// Execute start trigger immediately.
					if (m_startAudioTriggerId != CryAudio::InvalidControlId)
					{
						m_pIAudioObject->ExecuteTrigger(m_startAudioTriggerId);
					}
				}
			}
		}
	}
	else if (m_pIAudioObject != nullptr)
	{
		DeactivateAudio();
	}

	m_bExecuteAudioTrigger = false;
}

int CParticleSubEmitter::EmitParticle(SParticleUpdateContext& context, const EmitParticleData& data_in, float fAge, QuatTS* plocPreTransform)
{
	const ResourceParticleParams& params = GetParams();

	EmitParticleData data = data_in;

	// Determine geometry
	if (!data.pStatObj)
		data.pStatObj = params.pStatObj;

	if (data.pStatObj && !data.pPhysEnt && params.eGeometryPieces != params.eGeometryPieces.Whole)
	{
		// Emit sub-meshes rather than parent mesh.
		IStatObj* pParentObj = data.pStatObj;
		QuatTS* plocMainPreTransform = plocPreTransform;

		int nPieces = 0;
		for (int i = pParentObj->GetSubObjectCount() - 1; i >= 0; i--)
		{
			if (IStatObj::SSubObject* pSub = GetSubGeometry(pParentObj, i))
			{
				if (params.eGeometryPieces == params.eGeometryPieces.AllPieces || (nPieces++ == 0 || cry_random(0, nPieces - 1) == 0))
				{
					// Emit this piece
					data.pStatObj = pSub->pStatObj;

					QuatTS locPreTransform;
					if (params.bNoOffset)
					{
						// Use piece's sub-location
						locPreTransform = QuatTS(pSub->localTM);
						if (plocMainPreTransform)
							locPreTransform = *plocMainPreTransform * locPreTransform;
						plocPreTransform = &locPreTransform;
					}
					if (params.eGeometryPieces == params.eGeometryPieces.AllPieces)
						// Emit each piece as a separate particle
						nPieces += EmitParticle(context, data, fAge, plocPreTransform);
				}
			}
		}
		if (params.eGeometryPieces == params.eGeometryPieces.AllPieces && nPieces > 0)
			return nPieces;
	}

	m_nEmitIndex++;

	// Init local particle, copy to container. Use raw data buffer, to avoid extra destructor.
	CRY_ALIGN(128) char buffer[sizeof(CParticle)];
	CParticle* pPart = new(buffer) CParticle;

	pPart->Init(context, fAge, this, data);

	if (plocPreTransform)
		pPart->PreTransform(*plocPreTransform);

	CParticle* pNewPart = GetContainer().AddParticle(context, *pPart);
	if (!pNewPart)
	{
		pPart->~CParticle();
		return 0;
	}

	if (GetContainer().GetChildFlags())
	{
		// Create child emitters (on actual particle address, not pPart)
		GetMain().CreateIndirectEmitters(pNewPart, &GetContainer());
	}

	return 1;
}

Vec3 CParticleSubEmitter::GetEmitFocusDir(const QuatTS& loc, float fStrength, Quat* pRot) const
{
	const ParticleParams& params = GetParams();

	Quat q = loc.q;
	Vec3 vFocus = loc.q.GetColumn1();

	if (params.bFocusGravityDir)
	{
		float fLenSq = GetMain().GetPhysEnviron().m_UniformForces.vAccel.GetLengthSquared();
		if (fLenSq > FLT_MIN)
		{
			vFocus = GetMain().GetPhysEnviron().m_UniformForces.vAccel * -isqrt_tpl(fLenSq);
			if (params.fFocusAngle || pRot)
				RotateTo(q, vFocus);
		}
	}

	if (params.fFocusCameraDir)
	{
		float fBias = params.fFocusCameraDir(m_ChaosKey, fStrength);
		Vec3 vCamDir = (gEnv->p3DEngine->GetRenderingCamera().GetPosition() - loc.t).GetNormalized();
		vFocus.SetSlerp(vFocus, vCamDir, fBias);
		if (params.fFocusAngle || pRot)
			RotateTo(q, vFocus);
	}

	if (params.fFocusAngle)
	{
		float fAngle = DEG2RAD(params.fFocusAngle(m_ChaosKey, fStrength));
		float fAzimuth = DEG2RAD(params.fFocusAzimuth(m_ChaosKey, fStrength));
		q = q * Quat::CreateRotationXYZ(Ang3(fAngle, fAzimuth, 0));
		vFocus = q.GetColumn1();
	}

	if (pRot)
		*pRot = q;

	return vFocus;
}

void CParticleSubEmitter::DeactivateAudio()
{
	if (m_pIAudioObject != nullptr)
	{
		if (m_audioParameterId != CryAudio::InvalidControlId)
		{
			ResourceParticleParams const& params = GetParams();
			float const value = params.fSoundFXParam(m_ChaosKey, GetStrength(0.0f, params.eSoundControlTime));
			m_pIAudioObject->SetParameter(m_audioParameterId, value);
		}

		if (m_stopAudioTriggerId != CryAudio::InvalidControlId)
		{
			m_pIAudioObject->SetTransformation(GetEmitTM());
			m_pIAudioObject->ExecuteTrigger(m_stopAudioTriggerId);
		}
		else
		{
			CRY_ASSERT(m_startAudioTriggerId != CryAudio::InvalidControlId);
			m_pIAudioObject->StopTrigger(m_startAudioTriggerId);
		}

		gEnv->pAudioSystem->ReleaseObject(m_pIAudioObject);
		m_pIAudioObject = nullptr;
		m_startAudioTriggerId = CryAudio::InvalidControlId;
		m_stopAudioTriggerId = CryAudio::InvalidControlId;
		m_audioParameterId = CryAudio::InvalidControlId;
	}
}

float CParticleSubEmitter::ComputeDensityIncrease(float fStrength, float fParticleLife, const QuatTS& locA, const QuatTS* plocB) const
{
	// Increase emission rate to compensate for moving emitter and non-uniform forces.
	const ResourceParticleParams& params = GetParams();
	SPhysEnviron const& PhysEnv = GetMain().GetPhysEnviron();

	AABB bbSource = params.GetEmitOffsetBounds();

	FStaticBounds opts;
	opts.fSpeedScale = GetMain().GetSpawnParams().fSpeedScale;
	opts.fMaxLife = fParticleLife;

	FEmitterFixed fixed = { m_ChaosKey, fStrength };
	float fSize = params.fSize(0.5f, fStrength, VMAX) * 2.f;
	float fGravityScale = params.fGravityScale(0.5f, fStrength, 0.5f);

	// Get emission volume for default orientation and forces
	SForceParams forces;
	static_cast<SPhysForces&>(forces) = PhysEnv.m_UniformForces;
	forces.vAccel = forces.vAccel * fGravityScale + params.vAcceleration;
	forces.vWind.zero();
	forces.fDrag = params.fAirResistance(0.5f, fStrength, 0.5f);
	forces.fStretch = 0.f;

	AABB bbTravel(AABB::RESET);
	float fDist = params.GetTravelBounds(bbTravel, IParticleEffect::ParticleLoc(Vec3(ZERO)), forces, opts, fixed);
	float fStandardVolume = Travel::TravelVolume(bbSource, bbTravel, fDist, fSize);

	// Get emission volume for current forces, in local space
	Quat qToA = locA.q.GetInverted();

	PhysEnv.GetForces(forces, GetSource().GetLocation().t, GetContainer().GetEnvironmentFlags());
	forces.vAccel = qToA * forces.vAccel * fGravityScale + params.vAcceleration;
	forces.vWind = qToA * forces.vWind * params.fAirResistance.fWindScale;

	fDist = params.GetTravelBounds(bbTravel, QuatTS(IDENTITY), forces, opts, fixed);
	if (plocB)
	{
		// Add previous emission from previous location, in local space
		QuatTS locToA = locA.GetInverted() * *plocB;
		bbSource.Add(AABB::CreateTransformedAABB(QuatT(locToA), bbSource));
		AABB bbTravelB;
		float fDistB = params.GetTravelBounds(bbTravelB, locToA, forces, opts, fixed);
		fDist = max(fDist, fDistB);
		bbTravel.Add(bbTravelB);
	}
	float fVolume = Travel::TravelVolume(bbSource, bbTravel, fDist, fSize);

	return div_min(fVolume, fStandardVolume, fMAX_DENSITY_ADJUST);
}

Matrix34 CParticleSubEmitter::GetEmitTM() const
{
	QuatTS const& loc = GetSource().GetLocation();
	Matrix34 emitTM(loc);
	emitTM.SetColumn(3, loc * GetParams().vPositionOffset);
	return emitTM;
}
