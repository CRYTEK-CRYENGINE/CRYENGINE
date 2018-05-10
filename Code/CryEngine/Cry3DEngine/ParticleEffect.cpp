// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

// -------------------------------------------------------------------------
//  File name:   particleeffect.cpp
//  Version:     v1.00
//  Created:     10/7/2003 by Timur.
//  Compilers:   Visual Studio.NET
//  Description:
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////

#include "StdAfx.h"
#include "ParticleEffect.h"
#include "ParticleEmitter.h"
#include "ParticleManager.h"
#include <CrySystem/Profilers/IStatoscope.h>
#include <CryAudio/IAudioSystem.h>

//////////////////////////////////////////////////////////////////////////
// TypeInfo XML serialisation code
#include <CryParticleSystem/ParticleParams_TypeInfo.h>
#include <CryCore/TypeInfo_impl.h>

DEFINE_INTRUSIVE_LINKED_LIST(CParticleEffect)

static const int nSERIALIZE_VERSION = 28;
static const int nMIN_SERIALIZE_VERSION = 19;

// Write a struct to an XML node.
template<class T>
void ToXML(IXmlNode& xml, T& val, const T& def_val, FToString flags)
{
	CTypeInfo const& info = TypeInfo(&val);

	for (const CTypeInfo::CVarInfo* pVar = info.NextSubVar(nullptr);
	     pVar != nullptr;
	     pVar = info.NextSubVar(pVar))
	{
		if (pVar->GetElemSize() == 0)
			continue;

		if (&pVar->Type == &TypeInfo((void**)0))
			// Never serialize pointers.
			continue;

		assert(pVar->GetDim() == 1);
		cstr name = pVar->GetName();

		if (name[0] == '_')
			// Do not serialize private vars.
			continue;

		string str = pVar->ToString(&val, flags, &def_val);
		if (flags.SkipDefault() && str.length() == 0)
			continue;

		xml.setAttr(name, str);
	}
}

// Read a struct from an XML node.
template<class T>
void FromXML(IXmlNode& xml, T& val, FFromString flags)
{
	CTypeInfo const& info = TypeInfo(&val);
	int nAttrs = xml.getNumAttributes();
	for (int a = 0; a < nAttrs; a++)
	{
		cstr sKey, sVal;
		xml.getAttributeByIndex(a, &sKey, &sVal);

		CTypeInfo::CVarInfo const* pVar = info.FindSubVar(sKey);
		if (pVar)
		{
			assert(pVar->GetDim() == 1);
			pVar->FromString(&val, sVal, flags);
		}
	}
}

template<class T>
bool GetAttr(IXmlNode const& node, cstr attr, T& val)
{
	cstr sAttr = node.getAttr(attr);
	return *sAttr && TypeInfo(&val).FromString(&val, sAttr, FFromString().SkipEmpty(1));
}

template<class T>
T GetAttrValue(IXmlNode const& node, cstr attr, T defval)
{
	GetAttr(node, attr, defval);
	return defval;
}

template<class T>
void SetParamRange(TVarParam<T>& param, float fValMin, float fValMax)
{
	if (abs(fValMin) > abs(fValMax))
		std::swap(fValMin, fValMax);
	if (fValMax != 0.f)
		param.Set(fValMax, (fValMax - fValMin) / fValMax);
	else
		param.Set(0.f, 0.f);
}

template<class T>
void AdjustParamRange(TVarParam<T>& param, float fAdjust)
{
	float fValMax = param(VMAX) + fAdjust,
	      fValMin = param(VMIN) + fAdjust;
	SetParamRange(param, fValMin, fValMax);
}

//////////////////////////////////////////////////////////////////////////
// ResourceParticleParams implementation

static const float fTRAVEL_SAFETY = 0.1f;
static const float fSIZE_SAFETY = 0.1f;

struct SEmitParams
{
	Vec3  vAxis;
	float fCosMin, fCosMax;
	float fSpeedMin, fSpeedMax;

	void SetOmniDir()
	{ fCosMin = -1.f; fCosMax = 1.f; }
	bool bOmniDir() const
	{ return fCosMin <= -1.f && fCosMax >= 1.f; }
	bool bSingleDir() const
	{ return fCosMin >= 1.f; }
};

namespace Travel
{
float TravelDistanceApprox(const Vec3& vVel0, float fTime, const SForceParams& forces)
{
	if (fTime <= 0.f)
		return 0.f;

	Vec3 vVel = vVel0;

	// Check if trajectory has a speed minimum
	float dt[3] = { fTime, 0.f, 0.f };
	float s[5] = { vVel.GetLengthFast() };
	int nS = 1;

	if (forces.fDrag * fTime >= fDRAG_APPROX_THRESHOLD)
	{
		float fInvDrag = 1.f / forces.fDrag;
		Vec3 vTerm = forces.vWind + forces.vAccel * fInvDrag;
		/*
		   s^2 = (V d + VT (1 - d))^2					; d = e^(-b t)
		     = (VT + (V-VT) e^(-b t))^2
		     = VT^2 + 2 VT*(V-VT) e^(-b t) + (V-VT)^2 e^(-2b t)
		   s^2\t = -b 2 VT*(V-VT) e^(-b t) - 2b (V-VT)^2 e^(-2b t) = 0
		   -VT*(V-VT) = (V-VT)^2 e^(-b t)
		   e^(-b t) = -VT*(V-VT) / (V-VT)^2
		   t = -log ( VT*(VT-V) / (VT-V)^2) / b
		 */
		Vec3 vDV = vTerm - vVel;
		float fDD = vDV.len2(), fTD = vTerm * vDV;
		if (fDD * fTD > 0.f)
		{
			float fT = -logf(fTD / fDD) * fInvDrag;
			if (fT > 0.f && fT < fTime)
			{
				dt[0] = fT;
				dt[1] = fTime - fT;
			}
		}

		for (int t = 0; dt[t] > 0.f; t++)
		{
			float fDecay = 1.f - expf(-forces.fDrag * dt[t] * 0.5f);
			for (int i = 0; i < 2; i++)
			{
				vVel = Lerp(vVel, vTerm, fDecay);
				s[nS++] = vVel.GetLengthFast();
			}
		}
	}
	else
	{
		// Fast approx drag computation.
		Vec3 vAccel = forces.vAccel + (forces.vWind - vVel) * forces.fDrag;
		float fVA = vVel * vAccel,
		      fAA = vAccel * vAccel;
		if (fVA * fAA < 0.f && -fVA < fTime * fAA)
		{
			dt[0] = -fVA / fAA;
			dt[1] = fTime - dt[0];
		}

		for (int t = 0; dt[t] > 0.f; t++)
		{
			Vec3 vD = forces.vAccel * (dt[t] * 0.5f);
			for (int i = 0; i < 2; i++)
			{
				vVel += vD;
				s[nS++] = vVel.GetLengthFast();
			}
		}
	}

	if (nS == 5)
	{
		// 2-segment quadratic approximation
		return ((s[0] + s[1] * 4.f + s[2]) * dt[0]
		        + (s[2] + s[3] * 4.f + s[4]) * dt[1]) * 0.16666666f;
	}
	else
	{
		// Single segment quadratic approximation
		return (s[0] + s[1] * 4.f + s[2]) * 0.16666666f * dt[0];
	}
}

float TravelVolume(const AABB& bbSource, const AABB& bbTravel, float fDist, float fSize)
{
	Vec3 V = bbSource.GetSize() + bbTravel.GetSize() + Vec3(fSize);
	Vec3 T = bbTravel.GetCenter().abs().GetNormalized() * fDist;

	return V.x * V.y * V.z + V.x * V.y * T.z + V.x * T.y * V.z + T.x * V.y * V.z;
}

void AddTravelVec(AABB& bb, Vec3 vVel, SForceParams const& force, float fTime)
{
	Vec3 vTravel(ZERO);
	Travel(vTravel, vVel, fTime, force);
	vTravel += vVel * force.fStretch;
	bb.Add(vTravel);
}

void AddTravel(AABB& bb, Vec3 const& vVel, SForceParams const& force, float fTime, int nAxes)
{
	if (force.fStretch != 0.f)
	{
		// Add start point
		bb.Add(vVel * force.fStretch);
	}

	// Add end point.
	AddTravelVec(bb, vVel, force, fTime);

	// Find time of min/max vals of each component, by solving for v[i] = 0
	if (nAxes && force.fDrag != 0)
	{
		// vt = a/d + w
		// v = (v0-vt) e^(-d t) + vt
		// vs = v + v\t s
		//		= (1 - d s)(v0-vt) e^(-d t) + vt
		//		= 0
		// e^(-d t) = vt / ((1 - d s)(vt-v0))
		// t = -log( vt / ((1 - d s)(vt-v0)) ) / d
		float fInvDrag = 1.f / force.fDrag;
		for (int i = 0; nAxes; i++, nAxes >>= 1)
		{
			if (nAxes & 1)
			{
				float fVT = force.vAccel[i] * fInvDrag + force.vWind[i];
				float d = (fVT - vVel[i]) * (1.f - force.fDrag * force.fStretch);
				if (fVT * d > 0.f)
				{
					float fT = -logf(fVT / d) * fInvDrag;
					if (fT > 0.f && fT < fTime)
						AddTravelVec(bb, vVel, force, fT);
				}
			}
		}
	}
	else
	{
		for (int i = 0; nAxes; i++, nAxes >>= 1)
		{
			if (nAxes & 1)
			{
				if (force.vAccel[i] != 0.f)
				{
					// ps = p + v s
					// vs = p\t + v\t s
					//		= v + a s
					// vs = 0
					//		= v0 + a (t+s)
					// t = -v0/a - s
					float fT = -vVel[i] / force.vAccel[i] - force.fStretch;
					if (fT > 0.f && fT < fTime)
						AddTravelVec(bb, vVel, force, fT);
				}
			}
		}
	}
}

Vec3 GetExtremeEmitVec(Vec3 const& vRefDir, SEmitParams const& emit)
{
	float fEmitCos = vRefDir * emit.vAxis;
	if (fEmitCos >= emit.fCosMin && fEmitCos <= emit.fCosMax)
	{
		// Emit dir is in the cone.
		return vRefDir * emit.fSpeedMax;
	}
	else
	{
		// Find dir in emission cone closest to ref dir.
		Vec3 vEmitPerpX = vRefDir - emit.vAxis * fEmitCos;
		float fPerpLenSq = vEmitPerpX.GetLengthSquared();

		float fCos = clamp_tpl(fEmitCos, emit.fCosMin, emit.fCosMax);
		Vec3 vEmitMax = emit.vAxis * fCos + vEmitPerpX * sqrt_fast_tpl((1.f - fCos * fCos) / (fPerpLenSq + FLT_MIN));
		vEmitMax *= if_else(vEmitMax * vRefDir >= 0.0f, emit.fSpeedMin, emit.fSpeedMax);
		return vEmitMax;
	}
}

void AddEmitDirs(AABB& bb, Vec3 const& vRefDir, SEmitParams const& emit, SForceParams const& force, float fTime, int nAxes)
{
	Vec3 vEmit = GetExtremeEmitVec(vRefDir, emit);
	AddTravel(bb, vEmit, force, fTime, nAxes);
	vEmit = GetExtremeEmitVec(-vRefDir, emit);
	AddTravel(bb, vEmit, force, fTime, nAxes);
}

inline float GetSinMax(float fCosMin, float fCosMax)
{
	return fCosMin * fCosMax < 0.f ? 1.f : sqrtf(1.f - min(fCosMin * fCosMin, fCosMax * fCosMax));
}

inline float MaxComponent(Vec3 const& v)
{
	return max(max(abs(v.x), abs(v.y)), abs(v.z));
}

// Compute bounds of a cone of emission, with applied gravity.
void TravelBB(AABB& bb, SEmitParams const& emit, SForceParams const& force, float fTime, int nAxes)
{
	if (emit.fSpeedMax == 0.f)
	{
		AddTravel(bb, Vec3(0), force, fTime, nAxes);
		return;
	}
	else if (emit.bSingleDir())
	{
		AddTravel(bb, emit.vAxis * emit.fSpeedMax, force, fTime, nAxes);
		if (emit.fSpeedMin != emit.fSpeedMax)
			AddTravel(bb, emit.vAxis * emit.fSpeedMin, force, fTime, nAxes);
		return;
	}

	// First expand box from emission in cardinal directions.
	AddEmitDirs(bb, Vec3(1, 0, 0), emit, force, fTime, nAxes & 1);
	AddEmitDirs(bb, Vec3(0, 1, 0), emit, force, fTime, nAxes & 2);
	AddEmitDirs(bb, Vec3(0, 0, 1), emit, force, fTime, nAxes & 4);

	// Add extreme dirs along gravity.
	Vec3 vDir;
	if (CheckNormalize(vDir, force.vAccel))
	{
		if (MaxComponent(vDir) < 0.999f)
			AddEmitDirs(bb, vDir, emit, force, fTime, 0);
	}

	// And wind.
	if (force.fDrag > 0.f && CheckNormalize(vDir, force.vWind))
	{
		if (MaxComponent(vDir) < 0.999f)
			AddEmitDirs(bb, vDir, emit, force, fTime, 0);
	}
}
};

void ResourceParticleParams::GetStaticBounds(AABB& bb, const QuatTS& loc, const SPhysForces& forces, const FStaticBounds& opts) const
{
	// Compute spawn source box.
	bb = GetEmitOffsetBounds();

	if (bSpaceLoop && bBindEmitterToCamera)
	{
		// Use CameraMaxDistance as additional space loop size.
		bb.max.y = max(bb.max.y, +fCameraMaxDistance);
	}

	bb.SetTransformedAABB(Matrix34(loc), bb);
	bb.Expand(opts.vSpawnSize);

	if (!bSpaceLoop)
	{
		AABB bbTrav(0.f);
		GetMaxTravelBounds(bbTrav, loc, forces, opts);

		// Expand by a safety factor.
		bbTrav.min *= (1.f + fTRAVEL_SAFETY);
		bbTrav.max *= (1.f + fTRAVEL_SAFETY);

		bb.Augment(bbTrav);
	}

	if (eFacing == eFacing.Water && HasWater(forces.plWater))
	{
		// Move to water plane
		float fDist0 = bb.min.z - forces.plWater.DistFromPlane(bb.min);
		float fDistD = abs(forces.plWater.n.x * (bb.max.x - bb.min.x)) + abs(forces.plWater.n.y * (bb.max.y - bb.min.y));
		bb.min.z = fDist0 - fDistD;
		bb.max.z = fDist0 + fDistD;
	}

	// Particle size.
	if (opts.bWithSize)
	{
		float fMaxSize = GetMaxVisibleSize();
		fMaxSize *= loc.s * (1.f + fSIZE_SAFETY);
		bb.Expand(Vec3(fMaxSize));
	}
	if (bMoveRelativeEmitter)
		// Expand a bit for constant movement inaccuracy.
		bb.Expand(Vec3(0.01f));
}

float ResourceParticleParams::GetMaxVisibleSize() const
{
	float fMaxSize = fSize.GetMaxValue() * GetMaxObjectSize(pStatObj);
	if (LightSource.fIntensity)
		fMaxSize = max(fMaxSize, LightSource.fRadius.GetMaxValue());
	fMaxSize += abs(fCameraDistanceOffset);
	return fMaxSize;
}

template<class Var>
void ResourceParticleParams::GetEmitParams(SEmitParams& emit, const QuatTS& loc, const SPhysForces& forces, const FStaticBounds& opts, const Var& var) const
{
	emit.vAxis = bFocusGravityDir ? -forces.vAccel.GetNormalizedSafe(Vec3(0, 0, -1)) : loc.q.GetColumn1();

	// Max sin and cos of emission relative to emit dir.
	if (fFocusCameraDir)
	{
		// Focus can point in any direction.
		emit.SetOmniDir();
	}
	else if (fFocusAngle)
	{
		// Incorporate focus variation into min/max cos.
		float fAngleMax = DEG2RAD(fFocusAngle(var.RMax(), var.EMax())), fAngleMin = DEG2RAD(fFocusAngle(var.RMin(), var.EMin()));
		float fAzimuthMax = DEG2RAD(fFocusAzimuth(var.RMax(), var.EMax())), fAzimuthMin = DEG2RAD(fFocusAzimuth(var.RMin(), var.EMin()));

		emit.fCosMax = cosf(DEG2RAD(fEmitAngle(VMIN, var.EMin())));
		emit.fCosMin = cosf(min(opts.fAngMax + DEG2RAD(fEmitAngle(VMAX, var.EMax())) + (fAngleMax - fAngleMin + fAzimuthMax - fAzimuthMin) * 0.5f, gf_PI));

		// Rotate focus about axis.
		emit.vAxis = (loc.q * Quat::CreateRotationXYZ(Ang3((fAngleMin + fAngleMax) * 0.5f, (fAzimuthMin + fAzimuthMax) * 0.5f, 0))).GetColumn1();
	}
	else
	{
		// Fixed focus.
		emit.fCosMax = cosf(DEG2RAD(fEmitAngle(VMIN, var.EMin())));
		emit.fCosMin = cosf(DEG2RAD(fEmitAngle(VMAX, var.EMax())));
	}

	if (bEmitOffsetDir)
	{
		AABB bb = GetEmitOffsetBounds();
		if (bb.max.z > 0)
			emit.fCosMax = 1.f;
		if (bb.min.z < 0)
			emit.fCosMin = -1.f;
		else if (bb.min.x < 0 || bb.max.x > 0 || bb.min.y < 0 || bb.max.y > 0)
			emit.fCosMin = min(emit.fCosMin, 0.f);
	}

	emit.fSpeedMin = fSpeed(VMIN, var.EMin()) * loc.s * opts.fSpeedScale;
	emit.fSpeedMax = fSpeed(VMAX, var.EMax()) * loc.s * opts.fSpeedScale;
}

void ResourceParticleParams::GetMaxTravelBounds(AABB& bbResult, const QuatTS& loc, const SPhysForces& forces, const FStaticBounds& opts) const
{
	float fTime = min(+opts.fMaxLife, GetMaxParticleLife());
	if (fTime <= 0.f)
		return;

	// Emission direction.
	SEmitParams emit;
	GetEmitParams(emit, loc, forces, opts, FEmitterRandom());

	SForceParams force;
	force.vWind = forces.vWind * fAirResistance.fWindScale;
	force.fStretch = !GetTailSteps() ? fStretch(VMAX) * max(fStretch.fOffsetRatio + 1.f, 0.f) : 0.f;

	// Perform separate checks for variations in drag and gravity.
	float afDrag[2] = { fAirResistance(VMAX), fAirResistance(VMIN) };
	float afGrav[2] = { fGravityScale(VMAX), fGravityScale(VMIN) };
	for (int iDragIndex = (afDrag[1] != afDrag[0] ? 1 : 0); iDragIndex >= 0; iDragIndex--)
	{
		force.fDrag = afDrag[iDragIndex];
		for (int iGravIndex = (afGrav[1] != afGrav[0] ? 1 : 0); iGravIndex >= 0; iGravIndex--)
		{
			force.vAccel = forces.vAccel * afGrav[iGravIndex] + vAcceleration * loc.s;
			Travel::TravelBB(bbResult, emit, force, fTime, 7);
		}
	}

	if (fTurbulence3DSpeed)
	{
		// Expansion from 3D turbulence.	a = T t^-/2;  d = a/2 t^2 = T/2 t^(3/2)
		float fAccel = fTurbulence3DSpeed(VMAX) * isqrt_tpl(fTime) * (1.f + fTRAVEL_SAFETY);
		SForceParams forcesTurb;
		forcesTurb.vAccel = Vec3(fAccel);
		forcesTurb.vWind.zero();
		forcesTurb.fDrag = fAirResistance(VMIN);
		forcesTurb.fStretch = 0.0f;
		Vec3 vTrav(ZERO), vVel(ZERO);
		Travel::Travel(vTrav, vVel, fTime, forcesTurb);
		bbResult.Expand(vTrav);
	}

	// Expansion from spiral vortex.
	if (fTurbulenceSpeed)
	{
		float fVortex = fTurbulenceSize(VMAX) * loc.s;
		bbResult.Expand(Vec3(fVortex));
	}
}

float ResourceParticleParams::GetTravelBounds(AABB& bbResult, const QuatTS& loc, const SForceParams& forces, const FStaticBounds& opts, const FEmitterFixed& var) const
{
	float fTime = min(+opts.fMaxLife, GetMaxParticleLife());
	if (fTime <= 0.f)
		return 0.f;

	// Emission direction
	SEmitParams emit;
	GetEmitParams(emit, loc, forces, opts, var);

	// Get destination BB

	bbResult.Reset();
	Travel::TravelBB(bbResult, emit, forces, fTime, 0);
	bbResult.Move(loc.t);

	// Estimate distance along arc
	return Travel::TravelDistanceApprox(emit.vAxis * (emit.fSpeedMax + emit.fSpeedMin) * 0.5f, fTime, forces);
}

void ResourceParticleParams::ComputeShaderData()
{
	ShaderData = SParticleShaderData();

	ShaderData.m_tileSize[0] = 1.0f / TextureTiling.nTilesX;
	ShaderData.m_tileSize[1] = 1.0f / TextureTiling.nTilesY;
	ShaderData.m_frameCount = (float)TextureTiling.nAnimFramesCount;
	ShaderData.m_firstTile = (float)TextureTiling.nFirstTile;

	ShaderData.m_expansion[0] =
	  eFacing == eFacing.Camera || eFacing == eFacing.CameraX ? 1.f   // X expansion and curvature
	  : -1.f;                                                         // X expansion only
	ShaderData.m_expansion[1] =
	  HasVariableVertexCount() ? 0.f                                  // no Y expansion or curvature
	  : eFacing == eFacing.Camera ? 1.f                               // Y expansion and curvature
	  : -1.f;                                                         // Y expansion only
	ShaderData.m_curvature =
	  eFacing == eFacing.Camera || eFacing == eFacing.CameraX ?
	  +fCurvature
	  : 0.f;
	ShaderData.m_textureFrequency = Connection ? float(Connection.fTextureFrequency) : 1.0f;

	// light energy normalizer for the light equation:
	//	L = max(0, cos(a)*(1-y)+y)
	//	cos(a) = dot(l, n)
	//	y = back lighting
	const float y = fDiffuseBacklighting;
	const float energyNorm = (y < 0.5) ? (1 - y) : (1.0f / (4.0f * y));
	const float kiloScale = 1000.0f;
	const float toLightUnitScale = kiloScale / RENDERER_LIGHT_UNIT_SCALE;
	ShaderData.m_diffuseLighting = fDiffuseLighting * energyNorm;
	ShaderData.m_emissiveLighting = fEmissiveLighting * toLightUnitScale;
	ShaderData.m_backLighting = fDiffuseBacklighting;

	ShaderData.m_softnessMultiplier = bSoftParticle.fSoftness;
	ShaderData.m_sphericalApproximation = fSphericalApproximation * 0.999f;
	ShaderData.m_thickness = fVolumeThickness;

	// x = alpha scale, y = alpha clip min, z = alpha clip max
	float fAlphaScale = fAlpha.GetMaxValue();
	ShaderData.m_alphaTest[0][0] = AlphaClip.fScale.Min * fAlphaScale;
	ShaderData.m_alphaTest[1][0] = (AlphaClip.fScale.Max - AlphaClip.fScale.Min) * fAlphaScale;
	ShaderData.m_alphaTest[0][1] = AlphaClip.fSourceMin.Min;
	ShaderData.m_alphaTest[1][1] = AlphaClip.fSourceMin.Max - AlphaClip.fSourceMin.Min;
	ShaderData.m_alphaTest[0][2] = AlphaClip.fSourceWidth.Min;
	ShaderData.m_alphaTest[1][2] = AlphaClip.fSourceWidth.Max - AlphaClip.fSourceWidth.Min;
}

void ResourceParticleParams::ComputeEnvironmentFlags()
{
	// Needs updated environ if particles interact with gravity or air.
	nEnvFlags = 0;

	if (!bEnabled)
		return;

	if (eFacing == eFacing.Water)
		nEnvFlags |= ENV_WATER;
	if (fAirResistance && fAirResistance.fWindScale)
		nEnvFlags |= ENV_WIND;
	if (fGravityScale || bFocusGravityDir)
		nEnvFlags |= ENV_GRAVITY;
	if (ePhysicsType == ePhysicsType.SimpleCollision)
	{
		if (bCollideTerrain)
			nEnvFlags |= ENV_TERRAIN;
		if (bCollideStaticObjects)
			nEnvFlags |= ENV_STATIC_ENT;
		if (bCollideDynamicObjects)
			nEnvFlags |= ENV_DYNAMIC_ENT;
		if (nEnvFlags & ENV_COLLIDE_ANY)
			nEnvFlags |= ENV_COLLIDE_INFO;
	}
	else if (ePhysicsType >= EPhysics::SimplePhysics && nMaxCollisionEvents > 0)
		nEnvFlags |= ENV_COLLIDE_INFO;

	// Rendering params.
	if (fSize && fAlpha)
	{
		if (pStatObj != 0)
			nEnvFlags |= REN_GEOMETRY;
		else if (eFacing == eFacing.Decal)
		{
			if (pMaterial != 0)
				nEnvFlags |= REN_DECAL;
		}
		else
		{
			if (pMaterial != 0)
				nEnvFlags |= REN_SPRITE;
		}
		if (bCastShadows && pStatObj)   // only particle with geometry can cast shadows right now
			nEnvFlags |= REN_CAST_SHADOWS;

		if ((nEnvFlags & (REN_SPRITE | REN_GEOMETRY)) && eBlendType != eBlendType.Additive && !Connection)
			nEnvFlags |= REN_SORT;
	}
	if (LightSource.fIntensity && LightSource.fRadius)
		nEnvFlags |= REN_LIGHTS;
	if (!sStartTrigger.empty() || !sStopTrigger.empty())
		nEnvFlags |= EFF_AUDIO;
	if (eForceGeneration != eForceGeneration.None)
		nEnvFlags |= EFF_FORCE;

	if (!fParticleLifeTime || bRemainWhileVisible           // Infinite particle lifetime
	    || bBindEmitterToCamera                             // Visible every frame
	    || (GetCVars()->e_ParticlesMinPhysicsDynamicBounds && ePhysicsType >= GetCVars()->e_ParticlesMinPhysicsDynamicBounds)
	    || bForceDynamicBounds
	    )
		nEnvFlags |= EFF_DYNAMIC_BOUNDS;

	if (bBindEmitterToCamera)
		nEnvFlags |= REN_BIND_CAMERA;

	//
	// Compute desired and allowed renderer flags.
	//

	nRenObjFlags.Clear();
	nRenObjFlags.SetState(1,
	                      eBlendType
	                      + !HasVariableVertexCount() * FOB_POINT_SPRITE
	                      + bOctagonalShape * FOB_OCTAGONAL
	                      + bTessellation * FOB_ALLOW_TESSELLATION
	                      + TextureTiling.bAnimBlend * OS_ANIM_BLEND
	                      + OS_ENVIRONMENT_CUBEMAP // this will be automatically turned off when lighting is disabled
	                      + bReceiveShadows * FOB_INSHADOW
	                      + bNotAffectedByFog * FOB_NO_FOG
	                      + (bSoftParticle && bSoftParticle.fSoftness) * FOB_SOFT_PARTICLE
	                      + bDrawOnTop * OS_NODEPTH_TEST
	                      + bDrawNear * FOB_NEAREST
	                      );

	// Disable impossible states.
	nRenObjFlags.SetState(-1,
	                      bDrawOnTop * FOB_NEAREST
	                      + (TextureTiling.nAnimFramesCount <= 1) * OS_ANIM_BLEND
	                      + (bDrawNear || bDrawOnTop) * FOB_SOFT_PARTICLE
	                      + bDrawNear * FOB_ALLOW_TESSELLATION
	                      + !fDiffuseLighting * (OS_ENVIRONMENT_CUBEMAP | FOB_INSHADOW)
	                      + HasVariableVertexCount() * (FOB_MOTION_BLUR | FOB_OCTAGONAL | FOB_POINT_SPRITE)
	                      );

	// Construct config spec mask for allowed consoles.
	mConfigSpecMask = ((BIT(eConfigMax) * 2 - 1) & ~(BIT(eConfigMin) - 1)) << CONFIG_LOW_SPEC;
	mConfigSpecMask |=
	  Platforms.PS4 * BIT(CONFIG_ORBIS)
	  + Platforms.XBoxOne * BIT(CONFIG_DURANGO)
	;
}

bool ResourceParticleParams::IsActive() const
{
	if (!bEnabled)
		return false;

	// If Quality cvar is set, it must match ConfigMin-Max params.
	if (GetCVars()->e_ParticlesQuality)
	{
		if (!(mConfigSpecMask & BIT(GetCVars()->e_ParticlesQuality)))
			return false;
	}

	// Get platform / config spec
	ESystemConfigSpec config_spec = gEnv->pSystem->GetConfigSpec();

	if (config_spec <= CONFIG_VERYHIGH_SPEC)
	{
		// PC platform. Match DX settings.
		if (!Platforms.PCDX11)
			return false;
	}

	if (config_spec > CONFIG_VERYHIGH_SPEC || !GetCVars()->e_ParticlesQuality)
	{
		// For consoles, or PC if e_ParticlesQuality not set, match against pure config spec
		if (!(mConfigSpecMask & BIT(config_spec)))
			return false;
	}

	return true;
}

//////////////////////////////////////////////////////////////////////////
int ResourceParticleParams::LoadResources(const char* pEffectName)
{
	// Load only what is not yet loaded. Check everything, but caller may check params.bResourcesLoaded first.
	// Call UnloadResources to force unload/reload.
	LOADING_TIME_PROFILE_SECTION(gEnv->pSystem);

	if (!bEnabled)
	{
		nEnvFlags = 0;
		return 0;
	}

	if (ResourcesLoaded() || !gEnv->pRenderer)
	{
		ComputeShaderData();
		ComputeEnvironmentFlags();
		return 0;
	}

	int nLoaded = 0;

	// Load material.
	if (!pMaterial && (!sTexture.empty() || !sMaterial.empty()))
	{
		if (!sMaterial.empty())
		{
			pMaterial = Get3DEngine()->GetMaterialManager()->LoadMaterial(sMaterial.c_str());
			if (!pMaterial || pMaterial == Get3DEngine()->GetMaterialManager()->GetDefaultMaterial())
			{
				CryWarning(VALIDATOR_MODULE_3DENGINE, VALIDATOR_WARNING, "Particle effect material %s not found",
				           sMaterial.c_str());
				// Load error texture for artist debugging.
				pMaterial = 0;
			}
		}
		else if (!sTexture.empty() && sGeometry.empty())
		{
			const uint32 textureLoadFlags = FT_DONT_STREAM;
			const int nTexId = GetRenderer()->EF_LoadTexture(sTexture.c_str(), textureLoadFlags)->GetTextureID();			
			if (nTexId <= 0)
				CryWarning(VALIDATOR_MODULE_3DENGINE, VALIDATOR_WARNING, "Particle effect texture %s not found", sTexture.c_str());
			else
				nLoaded++;

			pMaterial = gEnv->p3DEngine->GetMaterialManager()->CreateMaterial(pEffectName);
			if (gEnv->pRenderer)
			{
				SInputShaderResourcesPtr pResources = gEnv->pRenderer->EF_CreateInputShaderResource();
				pResources->m_Textures[EFTT_DIFFUSE].m_Name = sTexture.c_str();
				SShaderItem shaderItem = gEnv->pRenderer->EF_LoadShaderItem("Particles", false, 0, pResources);
				pMaterial->AssignShaderItem(shaderItem);
			}
			Vec3 white = Vec3(1.0f, 1.0f, 1.0f);
			float defaultOpacity = 1.0f;
			pMaterial->SetGetMaterialParamVec3("diffuse", white, false);
			pMaterial->SetGetMaterialParamFloat("opacity", defaultOpacity, false);
			pMaterial->RequestTexturesLoading(0.0f);
			if (nTexId > 0)
				GetRenderer()->RemoveTexture(nTexId);
		}
		else if (sGeometry.empty())
		{
			SShaderItem shader = pMaterial->GetShaderItem();
			if (!shader.m_pShader)
				CryWarning(VALIDATOR_MODULE_3DENGINE, VALIDATOR_WARNING, "Particle effect material %s has invalid shader",
				           sMaterial.c_str());
			nLoaded++;
		}
		fTexAspect = 0.0f;
	}
	if (eFacing == eFacing.Decal && sMaterial.empty())
	{
		CryWarning(VALIDATOR_MODULE_3DENGINE, VALIDATOR_WARNING, "Particle effect has no material for decal, texture = %s",
		           sTexture.c_str());
	}

	// Set aspect ratio.
	if (fTexAspect == 0.f)
	{
		UpdateTextureAspect();
	}

	// Load geometry.
	if (!pStatObj && !sGeometry.empty())
	{
		pStatObj = Get3DEngine()->LoadStatObj(sGeometry.c_str(), NULL, NULL, bStreamable);
		if (!pStatObj)
			CryWarning(VALIDATOR_MODULE_3DENGINE, VALIDATOR_WARNING, "Particle effect geometry not found: %s", sGeometry.c_str());
		else
			nLoaded++;
	}

	ComputeShaderData();
	ComputeEnvironmentFlags();

	nEnvFlags |= EFF_LOADED;
	return nLoaded;
}

void ResourceParticleParams::UpdateTextureAspect()
{
	TextureTiling.Correct();

	fTexAspect = 1.f;

	if (!pMaterial)
		return;
	SShaderItem shaderItem = pMaterial->GetShaderItem();
	IRenderShaderResources* pResources = shaderItem.m_pShaderResources;
	if (!pResources)
		return;

	SEfResTexture* pResTexture = pResources->GetTexture(EFTT_DIFFUSE);
	ITexture* pTexture = pResTexture ? pResTexture->m_Sampler.m_pITex : nullptr;
	if (pTexture)
	{
		float fWidth = pTexture->GetWidth() / (float)TextureTiling.nTilesX;
		float fHeight = pTexture->GetHeight() / (float)TextureTiling.nTilesY;
		if (fHeight == 0.f)
			fTexAspect = 0.f;
		else
			fTexAspect = fWidth / fHeight;
	}
}

//////////////////////////////////////////////////////////////////////////
void ResourceParticleParams::UnloadResources()
{
	pStatObj = 0;
	pMaterial = 0;
	fTexAspect = 0.f;
	nEnvFlags &= ~EFF_LOADED;
}

//////////////////////////////////////////////////////////////////////////
//
// Struct and TypeInfo for reading older params
//
struct CompatibilityParticleParams
	: ParticleParams, ZeroInit<CompatibilityParticleParams>, Cry3DEngineBase
{
	int    nVersion;
	string sSandboxVersion;

	// Version 26
	float fBounciness;
	DEFINE_ENUM(EMoveRelative,
	            No,
	            Yes,
	            YesWithTail
	            )
	EMoveRelative eMoveRelEmitter;        // Particle motion is in emitter space

	// Version 25
	ETrinary tDX11;
	bool     bGeometryInPieces;

	// Version 24
	float fAlphaTest;
	float fCameraDistanceBias;
	int   nDrawLast;

	// Version 22
	float  fPosRandomOffset;
	float  fAlphaScale;
	float  fGravityScaleBias;
	float  fCollisionPercentage;

	bool   bSimpleParticle;
	bool   bSecondGeneration, bSpawnOnParentCollision, bSpawnOnParentDeath;

	string sAllowHalfRes;

	// Version 20
	bool bBindToEmitter;
	bool bTextureUnstreamable, bGeometryUnstreamable;

	// Version 19
	bool bIgnoreAttractor;

	void Correct(class CParticleEffect* pEffect);

	// Suppress uninitMemberVar due to inheritance from ZeroInit templated class
	// cppcheck-suppress uninitMemberVar
	CompatibilityParticleParams(int nVer, cstr sSandboxVer, const ParticleParams& paramsDefault)
		: ParticleParams(paramsDefault),
		nVersion(nVer),
		sSandboxVersion(sSandboxVer)
	{
		if (nVersion < 21)
		{
			// Use old default values.
			fCount = 1.f;
			fParticleLifeTime = 1.f;
			fSpeed = 1.f;
		}
	}

	STRUCT_INFO;
};

//////////////////////////////////////////////////////////////////////////
STRUCT_INFO_BEGIN(CompatibilityParticleParams)
BASE_INFO(ParticleParams)

// Version 26
VAR_INFO(fBounciness)
VAR_INFO(eMoveRelEmitter)

// Version 24
ALIAS_INFO(fRotationalDragScale, fAirResistance.fRotationalDragScale)
ALIAS_INFO(fWindScale, fAirResistance.fWindScale)
VAR_INFO(fAlphaTest)
VAR_INFO(fCameraDistanceBias)
VAR_INFO(nDrawLast)

// Version 22
VAR_INFO(fPosRandomOffset)
VAR_INFO(fAlphaScale)
VAR_INFO(fGravityScaleBias)
VAR_INFO(fCollisionPercentage)

VAR_INFO(bSimpleParticle)
VAR_INFO(bSecondGeneration)
VAR_INFO(bSpawnOnParentCollision)
VAR_INFO(bSpawnOnParentDeath)

VAR_INFO(sAllowHalfRes)

// Ver 22
ALIAS_INFO(bOctogonalShape, bOctagonalShape)
ALIAS_INFO(bOffsetInnerScale, fOffsetInnerFraction)
VAR_INFO(bSimpleParticle)

// Version 20
VAR_INFO(bBindToEmitter)
VAR_INFO(bTextureUnstreamable)
VAR_INFO(bGeometryUnstreamable)

// Ver 19
ALIAS_INFO(fLightSourceRadius, LightSource.fRadius)
ALIAS_INFO(fLightSourceIntensity, LightSource.fIntensity)
ALIAS_INFO(fStretchOffsetRatio, fStretch.fOffsetRatio)
ALIAS_INFO(nTailSteps, fTailLength.nTailSteps)
VAR_INFO(bIgnoreAttractor)
STRUCT_INFO_END(CompatibilityParticleParams)

//////////////////////////////////////////////////////////////////////////
void CompatibilityParticleParams::Correct(CParticleEffect* pEffect)
{
	CTypeInfo const& info = ::TypeInfo(this);

	// Convert any obsolete parameters set.
	switch (nVersion)
	{
	case 19:
		if (bIgnoreAttractor)
			TargetAttraction.eTarget = TargetAttraction.eTarget.Ignore;

	case 20:
		// Obsolete parameter, set equivalent params.
		if (bBindToEmitter && !ePhysicsType)
		{
			bMoveRelativeEmitter.base() = true;
			vPositionOffset.zero();
			vRandomOffset.zero();
			fSpeed = 0.f;
			fInheritVelocity = 0.f;
			fGravityScale = 0.f;
			fAirResistance.Set(0.f);
			vAcceleration.zero();
			fTurbulence3DSpeed = 0.f;
			fTurbulenceSize = 0.f;
			fTurbulenceSpeed = 0.f;
			bSpaceLoop = false;
		}

		if (bTextureUnstreamable || bGeometryUnstreamable)
			bStreamable = false;

	case 21:
		if (fFocusAzimuth.GetRandomRange() > 0.f)
		{
			// Convert confusing semantics of FocusAzimuth random variation in previous version.
			if (fFocusAngle.GetMaxValue() == 0.f)
			{
				// If FocusAngle = 0, FocusAzimuth is inactive.
				fFocusAzimuth.Set(0.f);
			}
			else if (fFocusAzimuth.GetMaxValue() > 0.f && fFocusAzimuth.GetRandomRange() == 1.f)
			{
				// Assume intention was to vary between 0 and max value, as with all other params.
			}
			else
			{
				// Convert previous absolute-360-based random range to standard relative.
				float fValMin = fFocusAzimuth.GetMaxValue();
				float fValMax = fValMin + fFocusAzimuth.GetRandomRange() * 360.f;
				if (fValMax > 360.f)
				{
					fValMax -= 360.f;
					fValMin -= 360.f;
				}
				if (-fValMin > fValMax)
					fFocusAzimuth.Set(fValMin, (fValMax - fValMin) / -fValMin);
				else
					fFocusAzimuth.Set(fValMax, (fValMax - fValMin) / fValMax);
			}
		}

	case 22:
		// Replace special-purpose flag with equivalent settings.
		if (bSimpleParticle)
		{
			sMaterial = "";
			fEmissiveLighting = 1;
			fDiffuseLighting = 0;
			fDiffuseBacklighting = 0;
			bReceiveShadows = bCastShadows = bTessellation = false;
			bSoftParticle.set(false);
			bNotAffectedByFog = true;
		}

		if (fAlphaScale != 0.f)
			fAlpha.Set(fAlpha.GetMaxValue() * fAlphaScale);

		// Convert obsolete bias to proper random range.
		if (fGravityScaleBias != 0.f)
			AdjustParamRange(fGravityScale, fGravityScaleBias);

		if (sAllowHalfRes == "Allowed" || sAllowHalfRes == "Forced")
			bHalfRes = true;

		if (fCollisionPercentage != 0.f)
			fCollisionFraction = fCollisionPercentage * 0.01f;

		// Convert to current spawn enum.
		if (bSecondGeneration)
		{
			eSpawnIndirection =
				bSpawnOnParentCollision ? ESpawn::ParentCollide
				: bSpawnOnParentDeath ? ESpawn::ParentDeath
				: ESpawn::ParentStart;
		}

		if (fPosRandomOffset != 0.f)
		{
			// Convert to new roundness fraction.
			vRandomOffset = Vec3(vRandomOffset) + Vec3(fPosRandomOffset);
			fOffsetRoundness = fPosRandomOffset / max(max(vRandomOffset.x, vRandomOffset.y), vRandomOffset.z);
		}

	case 23:
	case 24:
		// Scaling by parent size.
		// Incorrect scaling of speed and offsets by parent particle size:
		//   Particle Version < 22, CryEngine < 3.3.3:         Correct
		//   Particle version = 22, CryEngine 3.3.4 .. 3.4.x:  Correct
		//   Particle version = 22, CryEngine 3.5.1 .. 3.5.3:  INCORRECT
		//   Particle version = 23, CryEngine 3.5.4:           INCORRECT
		//   Particle version = 24, CryEngine 3.5.5 .. 3.5.6:  INCORRECT
		//   Particle version = 24, CryEngine 3.5.7 .. 3.5.9:  Correct
		//   Particle version = 25, CryEngine 3.5.10 ..:       Correct
		if ((nVersion == 23 || nVersion >= 22 && sSandboxVersion >= "3.5.1" && sSandboxVersion < "3.5.7") && eSpawnIndirection && pEffect->GetParent())
		{
			// Print diagnostics when any corrections made.
			float fParentSize = pEffect->GetParent()->GetParticleParams().fSize.GetValueFromMod(1.f, 0.f);
			if (fParentSize != 1.f)
			{
				if (fSpeed)
				{
					Warning("Particle Effect '%s' (version %d, %s): Speed corrected by parent scale %g from %g to %g",
						pEffect->GetFullName().c_str(), nVersion, sSandboxVersion.c_str(), fParentSize,
						+fSpeed, +fSpeed * fParentSize);
					fSpeed.Set(fSpeed.GetMaxValue() * fParentSize);
				}
				if (!vPositionOffset.IsZero())
				{
					Warning("Particle Effect '%s' (version %d, %s): PositionOffset corrected by parent scale %g from (%g,%g,%g) to (%g,%g,%g)",
						pEffect->GetFullName().c_str(), nVersion, sSandboxVersion.c_str(), fParentSize,
						+vPositionOffset.x, +vPositionOffset.y, +vPositionOffset.z,
						+vPositionOffset.x * fParentSize, +vPositionOffset.y * fParentSize, +vPositionOffset.z * fParentSize);
					vPositionOffset.x = vPositionOffset.x * fParentSize;
					vPositionOffset.y = vPositionOffset.y * fParentSize;
					vPositionOffset.z = vPositionOffset.z * fParentSize;
				}
				if (!vRandomOffset.IsZero())
				{
					Warning("Particle Effect '%s' (version %d, %s): RandomOffset corrected by parent scale %g from (%g,%g,%g) to (%g,%g,%g)",
						pEffect->GetFullName().c_str(), nVersion, sSandboxVersion.c_str(), fParentSize,
						+vRandomOffset.x, +vRandomOffset.y, +vRandomOffset.z,
						+vRandomOffset.x * fParentSize, +vRandomOffset.y * fParentSize, +vRandomOffset.z * fParentSize);
					vRandomOffset.x = vRandomOffset.x * fParentSize;
					vRandomOffset.y = vRandomOffset.y * fParentSize;
					vRandomOffset.z = vRandomOffset.z * fParentSize;
				}
			}
		}

		// Alpha test changes.
		if (fAlphaTest != 0.f)
			AlphaClip.fSourceMin.Min = AlphaClip.fSourceMin.Max = fAlphaTest;

		// Sort param changes.
		if (nDrawLast)
			fSortOffset = nDrawLast * -0.01f;
		if (fCameraDistanceBias != 0.f)
			fCameraDistanceOffset = -fCameraDistanceBias;

	case 25:
		// DX11 spec
		if (tDX11 == ETrinary(false))
			Platforms.PCDX11 = false;

		// Fix reversed PivotY.
		fPivotY.Set(-fPivotY.GetMaxValue());

		if (bGeometryInPieces)
			eGeometryPieces = eGeometryPieces.AllPieces;
		else if (!sGeometry.empty())
		{
			if (_smart_ptr<IStatObj> pStatObj = Get3DEngine()->LoadStatObj(sGeometry.c_str(), NULL, NULL, bStreamable))
				if (GetSubGeometryCount(pStatObj))
					eGeometryPieces = eGeometryPieces.RandomPiece;
		}

	case 26:
		// Bounciness conversion
		if (fBounciness > 0.f)
			fElasticity = fBounciness;
		else if (fBounciness < 0.f)
		{
			nMaxCollisionEvents = 1;
			eFinalCollision = eFinalCollision.Die;
		}
		if (eMoveRelEmitter)
		{
			bMoveRelativeEmitter.base() = true;
			if (eMoveRelEmitter == EMoveRelative::YesWithTail)
				bMoveRelativeEmitter.bMoveTail = true;
		}
		// back lighting and energy conservative correction
		{
			fDiffuseBacklighting = fDiffuseBacklighting * 0.5f;
			const float y = fDiffuseBacklighting;
			const float energyDenorm = (y < 0.5f) ? 1.0f / (1 - y) : 4.0f * y;
			fDiffuseLighting = fDiffuseLighting * energyDenorm;
		}

	case 27:
		fEmissiveLighting = fEmissiveLighting * 10.0f;
	}
	;

	// Universal corrections.
	if (!fTailLength)
		fTailLength.nTailSteps = 0;

	TextureTiling.Correct();

	if (bSpaceLoop && fCameraMaxDistance == 0.f && GetEmitOffsetBounds().GetVolume() == 0.f)
	{
		Warning("Particle Effect '%s' has zero space loop volume: disabled", pEffect->GetFullName().c_str());
		bEnabled = false;
	}
}

//////////////////////////////////////////////////////////////////////////
// ParticleEffect implementation

//////////////////////////////////////////////////////////////////////////
CParticleEffect::CParticleEffect()
	: m_parent(0),
	m_pParticleParams(0)
{
}

CParticleEffect::CParticleEffect(const CParticleEffect& in, bool bResetInheritance)
	: m_parent(0),
	m_strName(in.m_strName),
	m_pParticleParams(new ResourceParticleParams(*in.m_pParticleParams))
{
	if (bResetInheritance)
		m_pParticleParams->eInheritance = ParticleParams::EInheritance();

	m_children.resize(in.m_children.size());
	for (int i = 0; i < m_children.size(); ++i)
	{
		m_children.at(i) = new CParticleEffect(in.m_children[i], bResetInheritance);
		m_children[i].m_parent = this;
	}
}

CParticleEffect::CParticleEffect(const char* sName)
	: m_parent(0),
	m_pParticleParams(0),
	m_strName(sName)
{
}

#define TEMPORARY_EFFECT_NAME "(Temporary)"

CParticleEffect::CParticleEffect(const ParticleParams& params)
	: m_parent(0),
	m_strName(TEMPORARY_EFFECT_NAME)
{
	m_pParticleParams = new ResourceParticleParams(params);
	LoadResources(false);

	if (!(m_pParticleParams->nEnvFlags & (REN_ANY | EFF_ANY)))
	{
		// Assume custom params with no specified texture or geometry can have geometry particles added.
		m_pParticleParams->nEnvFlags |= REN_GEOMETRY;
	}
}

bool CParticleEffect::IsTemporary() const
{
	return m_strName == TEMPORARY_EFFECT_NAME;
}

//////////////////////////////////////////////////////////////////////////
CParticleEffect::~CParticleEffect()
{
	UnloadResources();
	for (auto& child : m_children)
		child.m_parent = nullptr;
	delete m_pParticleParams;
}

void CParticleEffect::SetEnabled(bool bEnabled)
{
	if (bEnabled != IsEnabled())
	{
		InstantiateParams();
		m_pParticleParams->bEnabled = bEnabled;
		CParticleManager::Instance()->UpdateEmitters(this);
	}
}

//////////////////////////////////////////////////////////////////////////
void CParticleEffect::SetName(cstr sNewName)
{
	if (!m_parent)
	{
		// Top-level effect. Should be fully qualified with library and group names.
		// Remove and reinsert in set.
		if (m_strName != sNewName)
		{
			CParticleManager::Instance()->RenameEffect(this, sNewName);
			m_strName = sNewName;
		}
	}
	else
	{
		// Child effect. Use only final component, and prefix with parent name.
		cstr sBaseName = strrchr(sNewName, '.');
		sBaseName = sBaseName ? sBaseName + 1 : sNewName;

		// Ensure unique name.
		stack_string sNewBase;
		for (int i = m_parent->m_children.size() - 1; i >= 0; i--)
		{
			const CParticleEffect* pSibling = &m_parent->m_children[i];
			if (pSibling != this && pSibling->m_strName == sBaseName)
			{
				// Extract and replace number.
				cstr p = sBaseName + strlen(sBaseName);
				while (p > sBaseName && (p[-1] >= '0' && p[-1] <= '9'))
					p--;
				int nIndex = atoi(p);
				sNewBase.assign(sBaseName, p);
				sNewBase.append(stack_string(ToString(nIndex + 1)));
				sBaseName = sNewBase;

				// Reset loop.
				i = m_parent->m_children.size() - 1;
			}
		}

		m_strName = sBaseName;
	}
}

//////////////////////////////////////////////////////////////////////////
void CParticleEffect::InstantiateParams()
{
	if (!m_pParticleParams)
		m_pParticleParams = new ResourceParticleParams(CParticleManager::Instance()->GetDefaultParams());
}

//////////////////////////////////////////////////////////////////////////
const char* CParticleEffect::GetBaseName() const
{
	const char* sName = m_strName.c_str();
	if (m_parent)
	{
		// Should only have base name.
		assert(!strchr(sName, '.'));
	}
	else
	{
		// Return everything after lib name.
		if (const char* sBase = strchr(sName, '.'))
			sName = sBase + 1;
	}
	return sName;
}

stack_string CParticleEffect::GetFullName() const
{
	if (!m_parent)
		return m_strName;
	stack_string temp = m_parent->GetFullName();
	temp += ".";
	temp += m_strName;
	return temp;
}

//////////////////////////////////////////////////////////////////////////
void CParticleEffect::SetParent(IParticleEffect* pParent)
{
	if (pParent == m_parent)
		return;

	_smart_ptr<IParticleEffect> ref_ptr = this;
	if (m_parent)
	{
		int i = m_parent->FindChild(this);
		if (i >= 0)
			m_parent->m_children.erase(i);
		CParticleManager::Instance()->UpdateEmitters(m_parent);
	}
	if (pParent)
	{
		if (!m_parent)
			// Was previously top-level effect.
			CParticleManager::Instance()->DeleteEffect(this);
		static_cast<CParticleEffect*>(pParent)->m_children.push_back(this);
	}
	m_parent = static_cast<CParticleEffect*>(pParent);
	CParticleManager::Instance()->UpdateEmitters(this);
}

//////////////////////////////////////////////////////////////////////////
void CParticleEffect::ClearChilds()
{
	m_children.clear();
	CParticleManager::Instance()->UpdateEmitters(this);
}

//////////////////////////////////////////////////////////////////////////
void CParticleEffect::InsertChild(int slot, IParticleEffect* pEffect)
{
	if (slot < 0)
		slot = 0;
	if (slot > m_children.size())
		slot = m_children.size();

	assert(pEffect);
	CParticleEffect* pEff = (CParticleEffect*) pEffect;
	pEff->m_parent = this;
	m_children.insert(m_children.begin() + slot, pEff);
	CParticleManager::Instance()->UpdateEmitters(this);
}

//////////////////////////////////////////////////////////////////////////
int CParticleEffect::FindChild(IParticleEffect* pEffect) const
{
	int i = 0;
	for (auto& child : m_children)
	{
		if (&child == pEffect)
			return i;
		++i;
	}
	return -1;
}

CParticleEffect* CParticleEffect::FindChild(const char* szChildName) const
{
	for (auto& child : m_children)
	{
		if (child.m_strName == szChildName)
			return &child;
	}

	return 0;
}

const CParticleEffect* CParticleEffect::FindActiveEffect(int nVersion) const
{
	// Find active effect in tree most closely matching version.
	const CParticleEffect* pFoundEffect = 0;
	int nFoundVersion = 0;

	if (!nVersion)
		nVersion = nSERIALIZE_VERSION;

	// Check parent effect.
	if (m_pParticleParams && m_pParticleParams->IsActive())
	{
		// Check version against effect name.
		int nEffectVersion = atoi(GetBaseName());
		if (nEffectVersion <= nVersion)
		{
			pFoundEffect = this;
			nFoundVersion = nEffectVersion;
		}
	}

	// Look for matching children, which take priority.
	for (auto& child : m_children)
	{
		if (const CParticleEffect* pChild = child.FindActiveEffect(nVersion))
		{
			int nEffectVersion = atoi(pChild->GetBaseName());
			if (nEffectVersion >= nFoundVersion)
			{
				pFoundEffect = pChild;
				nFoundVersion = nEffectVersion;
			}
		}
	}

	return pFoundEffect;
}

//////////////////////////////////////////////////////////////////////////
bool CParticleEffect::LoadResources(bool bAll, cstr sSource) const
{
	// Check file access if sSource specified.
	if (sSource && !ResourcesLoaded(true) && !CParticleManager::Instance()->CanAccessFiles(GetFullName(), sSource))
		return false;

	int nLoaded = 0;

	if (IsEnabled() && !m_pParticleParams->ResourcesLoaded())
	{
		CRY_DEFINE_ASSET_SCOPE("Particle Effect", GetFullName());

#ifdef CRY_PFX1_BAIL_UNSUPPORTED
		if (GetParent() == 0)
			gEnv->pLog->Log("PFX1 feature set \"%s\"", GetName());
#endif

		nLoaded = m_pParticleParams->LoadResources(GetFullName().c_str());
	}
	if (bAll)
		for (auto& child : m_children)
			nLoaded += (int)child.LoadResources(true);
	return nLoaded > 0;
}

void CParticleEffect::UnloadResources(bool bAll) const
{
	if (m_pParticleParams)
		m_pParticleParams->UnloadResources();
	if (bAll)
		for (auto& child : m_children)
			child.UnloadResources(true);
}

bool CParticleEffect::ResourcesLoaded(bool bAll) const
{
	// Return whether all resources loaded.
	if (m_pParticleParams && !m_pParticleParams->ResourcesLoaded())
		return false;
	if (bAll)
		for (auto& child : m_children)
			if (!child.ResourcesLoaded(true))
				return false;
	return true;
}

const ParticleParams& CParticleEffect::GetDefaultParams() const
{
	return GetDefaultParams(m_pParticleParams ? m_pParticleParams->eInheritance : ParticleParams::EInheritance(), 0);
}

const ParticleParams& CParticleEffect::GetDefaultParams(ParticleParams::EInheritance eInheritance, int nVersion) const
{
	if (eInheritance == eInheritance.Parent)
	{
		if (m_parent)
			return m_parent->GetParams();
		eInheritance = eInheritance.System;
	}

	return CParticleManager::Instance()->GetDefaultParams(eInheritance, nVersion);
}

static int UpdateDefaultValues(const CTypeInfo& info, void* data, const void* def_data, const void* new_data)
{
	int nUpdated = 0;
	if (info.HasSubVars())
	{
		for (const CTypeInfo::CVarInfo* pVar = info.NextSubVar(nullptr);
		     pVar != nullptr;
		     pVar = info.NextSubVar(pVar))
			nUpdated += UpdateDefaultValues(pVar->Type, pVar->GetAddress(data), pVar->GetAddress(def_data), pVar->GetAddress(new_data));
	}
	else
	{
		if (info.ValueEqual(data, def_data) && !info.ValueEqual(data, new_data))
		{
			info.FromValue(data, new_data, info);
			return 1;
		}
	}
	return nUpdated;
}

void CParticleEffect::PropagateParticleParams(const ParticleParams& params)
{
	for (auto& child : m_children)
	{
		if (child.m_pParticleParams && child.m_pParticleParams->eInheritance == ParticleParams::EInheritance::Parent)
		{
			// Update all inherited values from new params.
			UpdateDefaultValues(TypeInfo(m_pParticleParams), child.m_pParticleParams, m_pParticleParams, &params);
			child.PropagateParticleParams(params);
		}
	}
}

//////////////////////////////////////////////////////////////////////////
bool CParticleEffect::IsEnabled(uint options) const
{
	bool bEnabled = m_pParticleParams && m_pParticleParams->bEnabled;

	if (bEnabled && (options & eCheckFeatures))
	{
		// Check whether effect does anything.
		if (!(m_pParticleParams->nEnvFlags & (REN_ANY | EFF_ANY)))
			bEnabled = false;
	}

	if (bEnabled && (options & eCheckConfig))
	{
		// Make sure effect and all indirect parents are active in current render context.
		for (const CParticleEffect* pEffect = this;; pEffect = pEffect->m_parent)
		{
			if (!(pEffect && pEffect->m_pParticleParams && pEffect->m_pParticleParams->IsActive()))
			{
				bEnabled = false;
				break;
			}
			if (!pEffect->m_pParticleParams->eSpawnIndirection)
				break;
		}
	}

	if (!bEnabled && (options & eCheckChildren))
	{
		// Check actual child effects.
		for (auto& child : m_children)
			if (child.GetIndirectParent())
				if (child.IsEnabled(options))
				{
					bEnabled = true;
					break;
				}
	}

	return bEnabled;
}

void CParticleEffect::SetParticleParams(const ParticleParams& params)
{
	CRY_DEFINE_ASSET_SCOPE("Particle Effect", GetFullName());

	InstantiateParams();
	if (params.sTexture != m_pParticleParams->sTexture || params.sMaterial != m_pParticleParams->sMaterial)
	{
		m_pParticleParams->pMaterial = 0;
		m_pParticleParams->nEnvFlags &= ~EFF_LOADED;
	}
	if (params.sGeometry != m_pParticleParams->sGeometry)
	{
		m_pParticleParams->pStatObj = 0;
		m_pParticleParams->nEnvFlags &= ~EFF_LOADED;
	}
	if (params.TextureTiling.nTilesX != m_pParticleParams->TextureTiling.nTilesX || params.TextureTiling.nTilesY != m_pParticleParams->TextureTiling.nTilesY)
	{
		m_pParticleParams->fTexAspect = 0.f;
		m_pParticleParams->nEnvFlags &= ~EFF_LOADED;
	}

	PropagateParticleParams(params);

	static_cast<ParticleParams&>(*m_pParticleParams) = params;
	m_pParticleParams->LoadResources(GetFullName().c_str());
	CParticleManager::Instance()->UpdateEmitters(this);
}

const ParticleParams& CParticleEffect::GetParticleParams() const
{
	if (m_pParticleParams)
		return *m_pParticleParams;
	return CParticleManager::Instance()->GetDefaultParams();
}

//////////////////////////////////////////////////////////////////////////
IParticleEmitter* CParticleEffect::Spawn(const ParticleLoc& loc, const SpawnParams* pSpawnParams)
{
	return CParticleManager::Instance()->CreateEmitter(loc, this, pSpawnParams);
}

//////////////////////////////////////////////////////////////////////////
void CParticleEffect::Serialize(XmlNodeRef node, bool bLoading, bool bAll)
{
	XmlNodeRef root = node;
	while (root->getParent())
		root = root->getParent();

	if (bLoading)
	{
		if (m_strName.empty())
		{
			if (m_parent)
				// Set simple name, will be automatically qualified with hierarchy.
				SetName(node->getAttr("Name"));
			else
			{
				// Qualify with library name.
				stack_string sFullName = root->getAttr("Name");
				if (sFullName.empty())
					sFullName = root->getTag();
				if (!sFullName.empty())
					sFullName += ".";
				sFullName += node->getAttr("Name");
				SetName(sFullName.c_str());
			}
		}

		int nVersion = nSERIALIZE_VERSION;
		cstr sSandboxVersion = "3.5";
		if (root->getAttr("ParticleVersion", nVersion))
		{
			if (nVersion < nMIN_SERIALIZE_VERSION || nVersion > nSERIALIZE_VERSION)
			{
				Warning("Particle Effect %s version (%d) out of supported range (%d-%d); may change in behavior",
				        GetName(), nVersion, nMIN_SERIALIZE_VERSION, nSERIALIZE_VERSION);
			}
		}
		root->getAttr("SandboxVersion", &sSandboxVersion);

		bool bEnabled = false;
		XmlNodeRef paramsNode = node->findChild("Params");
		if (paramsNode && (gEnv->IsEditor() || GetAttrValue(*paramsNode, "Enabled", true)))
		{
			// Init params, then read from XML.
			bEnabled = true;

			// Get defaults base, and initialize params
			ParticleParams::EInheritance eInheritance = GetAttrValue(*paramsNode, "Inheritance", ParticleParams::EInheritance());
			CompatibilityParticleParams params(nVersion, sSandboxVersion, GetDefaultParams(eInheritance, nVersion));

			FromXML(*paramsNode, params, FFromString().SkipEmpty(1));

			params.Correct(this);

			if (!m_pParticleParams)
				m_pParticleParams = new ResourceParticleParams(params);
			else
			{
				bool bLoadResources = m_pParticleParams->ResourcesLoaded();
				m_pParticleParams->UnloadResources();
				m_pParticleParams->~ResourceParticleParams();
				new(m_pParticleParams) ResourceParticleParams(params);
				if (bLoadResources)
					m_pParticleParams->LoadResources(GetFullName().c_str());
			}
		}

		if (bAll)
		{
			// Serialize children.
			XmlNodeRef childsNode = node->findChild("Childs");
			if (childsNode)
			{
				for (int i = 0; i < childsNode->getChildCount(); i++)
				{
					XmlNodeRef xchild = childsNode->getChild(i);
					_smart_ptr<IParticleEffect> pChildEffect;

					if (cstr sChildName = xchild->getAttr("Name"))
						pChildEffect = FindChild(sChildName);

					if (!pChildEffect)
					{
						pChildEffect = new CParticleEffect();
						pChildEffect->SetParent(this);
					}

					pChildEffect->Serialize(xchild, bLoading, bAll);
					if (pChildEffect->GetParent() && !pChildEffect->GetParticleParams().eSpawnIndirection)
						bEnabled = true;
				}
			}

			if (!gEnv->IsEditor() && !bEnabled && m_parent)
			{
				// Remove fully disabled child effects at load-time.
				SetParent(NULL);
			}
		}

		// Convert old internal targeting params
		if (nVersion < 23 && IsEnabled())
		{
			ParticleParams::STargetAttraction::ETargeting& eTarget = m_pParticleParams->TargetAttraction.eTarget;
			if (eTarget != eTarget.Ignore)
			{
				if (m_parent && m_parent->IsEnabled() && m_parent->GetParams().eForceGeneration == ParticleParams::EForce::_Target)
				{
					// Parent generated target attraction
					eTarget = eTarget.OwnEmitter;
				}
				if (GetParams().eForceGeneration == GetParams().eForceGeneration._Target)
				{
					m_pParticleParams->eForceGeneration = ParticleParams::EForce::None;
					if (m_children.empty())
						// Target attraction set on childless effect, intention probably to target self
						eTarget = eTarget.OwnEmitter;
				}
			}
		}
	}
	else
	{
		// Saving.
		bool bSerializeNamedFields = !!GetCVars()->e_ParticlesSerializeNamedFields;

		node->setAttr("Name", GetBaseName());
		root->setAttr("ParticleVersion", bSerializeNamedFields ? nSERIALIZE_VERSION : 23);

		if (m_pParticleParams)
		{
			// Save particle params.
			XmlNodeRef paramsNode = node->newChild("Params");

			ToXML<ParticleParams>(*paramsNode, *m_pParticleParams, GetDefaultParams(),
			                      FToString().SkipDefault(1).NamedFields(bSerializeNamedFields));
		}

		if (bAll && !m_children.empty())
		{
			// Serialize children.
			XmlNodeRef childsNode = node->newChild("Childs");
			for (auto& child : m_children)
			{
				XmlNodeRef xchild = childsNode->newChild("Particles");
				child.Serialize(xchild, bLoading, bAll);
			}
		}

		if (m_strName == "System.Default")
			gEnv->pParticleManager->SetDefaultEffect(this);
	}
}

void CParticleEffect::Serialize(Serialization::IArchive& ar)
{
}

void CParticleEffect::Reload(bool bAll)
{
	if (XmlNodeRef node = CParticleManager::Instance()->ReadEffectNode(GetFullName()))
	{
		Serialize(node, true, bAll);
		LoadResources(true);
	}
}

IParticleAttributes& CParticleEffect::GetAttributes()
{
	static class CNullParticleAttributes : public IParticleAttributes
	{
		virtual void         Reset(const IParticleAttributes* pCopySource = nullptr) {}
		virtual void         Serialize(Serialization::IArchive& ar)                  {}
		virtual void         TransferInto(IParticleAttributes* pReceiver) const      {}
		virtual TAttributeId FindAttributeIdByName(cstr name) const                  { return -1; }
		virtual uint         GetNumAttributes() const                                { return 0; }
		virtual cstr         GetAttributeName(uint idx) const                        { return nullptr; }
		virtual EType        GetAttributeType(uint idx) const                        { return ET_Boolean; }
		virtual const TValue& GetValue(TAttributeId idx) const                       { static TValue def; return def; }
		virtual TValue        GetValue(TAttributeId idx, const TValue& def) const    { return def; }
		virtual bool          SetValue(TAttributeId idx, const TValue& value)        { return false; }
	} nullAttributes;
	return nullAttributes;
}

void CParticleEffect::GetMemoryUsage(ICrySizer* pSizer) const
{
	if (!pSizer->AddObjectSize(this))
		return;

	pSizer->AddObject(m_strName);
	pSizer->AddObject(m_children);
	pSizer->AddObject(m_pParticleParams);
}

void CParticleEffect::GetEffectCounts(SEffectCounts& counts) const
{
	counts.nLoaded++;
	if (IsEnabled())
	{
		counts.nEnabled++;
		if (GetParams().ResourcesLoaded())
			counts.nUsed++;
		if (IsActive())
			counts.nActive++;
	}

	for (auto& child : m_children)
		child.GetEffectCounts(counts);
}

CParticleEffect* CParticleEffect::GetIndirectParent() const
{
	for (CParticleEffect const* pEffect = this; pEffect; pEffect = pEffect->m_parent)
		if (pEffect->IsEnabled() && pEffect->GetParams().eSpawnIndirection)
			return pEffect->m_parent;
	return 0;
}

float CParticleEffect::Get(FMaxEffectLife const& opts) const
{
	float fLife = 0.f;
	if (IsActive())
	{
		const ResourceParticleParams& params = GetParams();
		if (opts.fEmitterMaxLife() > 0.f)
		{
			fLife = params.fPulsePeriod ? fHUGE : params.GetMaxEmitterLife();
			if (const CParticleEffect* pParent = GetIndirectParent())
			{
				float fParentLife = pParent->GetParams().GetMaxParticleLife();
				fLife = min(fLife, fParentLife);
			}
			fLife = min(fLife, opts.fEmitterMaxLife());

			if (opts.bParticleLife())
				fLife += params.fParticleLifeTime.GetMaxValue();
		}
		else if (opts.bParticleLife())
			fLife += params.GetMaxParticleLife();
	}

	if (opts.bAllChildren() || opts.bIndirectChildren())
	{
		for (auto& child : m_children)
		{
			if (child.GetIndirectParent())
				fLife = max(fLife, child.Get(opts().fEmitterMaxLife(fLife).bAllChildren(1)));
			else if (opts.bAllChildren())
				fLife = max(fLife, child.Get(opts));
		}
	}
	return fLife;
}

float CParticleEffect::GetEquilibriumAge(bool bAll) const
{
	float fEquilibriumAge = 0.f;
	bool bHasEquilibrium = IsActive() && GetParams().HasEquilibrium();
	if (bHasEquilibrium)
	{
		fEquilibriumAge = GetParams().fSpawnDelay.GetMaxValue() + GetMaxParticleFullLife();
		if (const CParticleEffect* pParent = GetIndirectParent())
			fEquilibriumAge += pParent->GetEquilibriumAge(false);
	}

	if (bAll)
	{
		for (auto& child : m_children)
			if (child.IsEnabled(eCheckFeatures) && !child.GetParams().eSpawnIndirection)
				fEquilibriumAge = max(fEquilibriumAge, child.GetEquilibriumAge(true));
	}

	return fEquilibriumAge;
}

float CParticleEffect::GetMaxParticleSize(bool bParent) const
{
	if (!m_pParticleParams)
		return 0.f;

	float fMaxSize = m_pParticleParams->fSize.GetMaxValue();
	if (!bParent && m_pParticleParams->pStatObj)
		fMaxSize *= m_pParticleParams->pStatObj->GetRadius();

	if (m_parent && m_pParticleParams->eSpawnIndirection && m_pParticleParams->bMoveRelativeEmitter.ScaleWithSize())
		fMaxSize *= m_parent->GetMaxParticleSize(true);
	return fMaxSize;
}

IStatObj::SSubObject* GetSubGeometry(IStatObj* pParent, int i)
{
	assert(pParent);
	if (IStatObj::SSubObject* pSub = pParent->GetSubObject(i))
		if (pSub->nType == STATIC_SUB_OBJECT_MESH && pSub->pStatObj && pSub->pStatObj->GetRenderMesh())
			return pSub;
	return NULL;
}

int GetSubGeometryCount(IStatObj* pParent)
{
	int nPieces = 0;
	for (int i = pParent->GetSubObjectCount() - 1; i >= 0; i--)
		if (GetSubGeometry(pParent, i))
			nPieces++;
	return nPieces;
}
