// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

// -------------------------------------------------------------------------
//  File name:   ParticleEffect.h
//  Version:     v1.00
//  Created:     10/7/2003 by Timur.
//  Compilers:   Visual Studio.NET
//  Description:
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////

#pragma once

#include <CryParticleSystem/IParticles.h>
#include "Cry3DEngineBase.h"
#include <CryParticleSystem/ParticleParams.h>
#include "ParticleUtils.h"
#include "ParticleEnviron.h"
#include <CryCore/Containers/CryPtrArray.h>
#include <CryParticleSystem/Options.h>
#include <CrySystem/ICryMiniGUI.h>

#if CRY_PLATFORM_DESKTOP
	#define PARTICLE_EDITOR_FUNCTIONS
#endif

//////////////////////////////////////////////////////////////////////////
// Set of flags that can have 3 states: forced on, forced off, or neutral.
template<class T>
struct TrinaryFlags
{
	T On, Off;

	TrinaryFlags()
		: On(0), Off(0) {}

	void Clear()
	{ On = Off = 0; }

	// Set from an int where -1 is off, 0 is neutral, 1 is forced
	void SetState(int state, T flags)
	{
		On &= ~flags;
		Off &= ~flags;
		if (state < 0)
			Off |= flags;
		else if (state > 0)
			On |= flags;
	}

	TrinaryFlags& operator|=(T flags)
	{ On |= flags; return *this; }
	TrinaryFlags& operator&=(T flags)
	{ Off |= ~flags; return *this; }

	T operator&(T flags) const
	{ return (flags | On) & ~Off; }

	T operator&(TrinaryFlags<T> const& other) const
	{ return (On | other.On) & ~(Off | other.Off); }
};

//////////////////////////////////////////////////////////////////////////
// Requirements and attributes of particle effects
enum EEffectFlags
{
	// also uses EEnvironFlags
	// Additional effects.
	EFF_AUDIO = BIT(8),           // Ensures execution of audio relevant code.
	EFF_FORCE = BIT(9),           // Produces physical force.

	EFF_ANY   = EFF_AUDIO | EFF_FORCE,

	// Exclusive rendering methods.
	REN_SPRITE   = BIT(10),
	REN_GEOMETRY = BIT(11),
	REN_DECAL    = BIT(12),
	REN_LIGHTS   = BIT(13),         // Adds light to scene.

	REN_ANY      = REN_SPRITE | REN_GEOMETRY | REN_DECAL | REN_LIGHTS,

	// Visual effects of particles
	REN_CAST_SHADOWS = BIT(14),
	REN_BIND_CAMERA  = BIT(15),
	REN_SORT         = BIT(16),

	// General functionality
	EFF_DYNAMIC_BOUNDS = BIT(17),   // Requires updating particles every frame.
};

struct STileInfo
{
	Vec2  vSize;                      // Size of texture tile UVs.
	float fTileCount, fTileStart;     // Range of tiles in texture for emitter.
};

// Helper structures for emitter functions

struct SForceParams : SPhysForces
{
	float fDrag;
	float fStretch;
};

// Options for computing static bounds
struct FStaticBounds
{
	Vec3  vSpawnSize;
	float fAngMax;
	float fSpeedScale;
	bool  bWithSize;
	float fMaxLife;

	FStaticBounds()
		: vSpawnSize(0), fAngMax(0), fSpeedScale(1), bWithSize(true), fMaxLife(fHUGE) {}
};

struct FEmitterRandom
{
	static type_min RMin() { return VMIN; }
	static type_max RMax() { return VMAX; }
	static type_min EMin() { return VMIN; }
	static type_max EMax() { return VMAX; }
};

struct FEmitterFixed
{
	CChaosKey ChaosKey;
	float     fStrength;

	CChaosKey RMin() const { return ChaosKey; }
	CChaosKey RMax() const { return ChaosKey; }
	float     EMin() const { return fStrength; }
	float     EMax() const { return fStrength; }
};

struct SEmitParams;

//
// Additional runtime parameters.
//
struct ResourceParticleParams
	: ParticleParams, Cry3DEngineBase, ZeroInit<ResourceParticleParams>
{
	// Texture, material, geometry params.
	float                 fTexAspect;               // H/V aspect ratio.
	uint16                mConfigSpecMask;          // Allowed config specs.
	_smart_ptr<IMaterial> pMaterial;                // Used to override the material
	_smart_ptr<IStatObj>  pStatObj;                 // If it isn't set to 0, this object will be used instead of a sprite
	uint32                nEnvFlags;                // Summary of environment info needed for effect.
	TrinaryFlags<uint64>  nRenObjFlags;             // Flags for renderer, combining FOB_ and OS_.
	SParticleShaderData   ShaderData;               // Data to be used by the particle shaders

	ResourceParticleParams()
	{
		ComputeShaderData();
		ComputeEnvironmentFlags();
	}

	explicit ResourceParticleParams(const ParticleParams& params)
		: ParticleParams(params)
	{
		ComputeShaderData();
		ComputeEnvironmentFlags();
	}

	bool ResourcesLoaded() const
	{
		return nEnvFlags & EFF_LOADED;
	}

	int   LoadResources(const char* pEffectName);
	void  UnloadResources();
	void  UpdateTextureAspect();

	void  ComputeShaderData();
	void  ComputeEnvironmentFlags();
	bool  IsActive() const;

	void  GetStaticBounds(AABB& bbResult, const QuatTS& loc, const SPhysForces& forces, const FStaticBounds& opts) const;

	float GetTravelBounds(AABB& bbResult, const QuatTS& loc, const SForceParams& forces, const FStaticBounds& opts, const FEmitterFixed& var) const;

	float GetMaxObjectSize() const
	{
		return sqrt_fast_tpl(sqr(1.f + abs(fPivotX.GetMaxValue())) * sqr(fAspect.GetMaxValue()) + sqr(1.f + abs(fPivotY.GetMaxValue())));
	}
	float GetMaxObjectSize(IMeshObj* pObj) const
	{
		if (pObj)
		{
			AABB bb = pObj->GetAABB();
			if (bNoOffset || ePhysicsType == ePhysicsType.RigidBody)
				// Rendered at origin.
				return sqrt_fast_tpl(max(bb.min.GetLengthSquared(), bb.max.GetLengthSquared()));
			else
				// Rendered at center.
				return bb.GetRadius();
		}
		else
		{
			return GetMaxObjectSize();
		}
	}
	bool IsGeometryCentered() const
	{
		return !bNoOffset && ePhysicsType != ePhysicsType.RigidBody;
	}
	float GetMaxVisibleSize() const;

	void  GetMemoryUsage(ICrySizer* pSizer) const
	{
		pSizer->AddObject(this, sizeof(*this));
		TypeInfo().GetMemoryUsage(pSizer, this);
	}

protected:

	void GetMaxTravelBounds(AABB& bbResult, const QuatTS& loc, const SPhysForces& forces, const FStaticBounds& opts) const;

	template<class Var>
	void GetEmitParams(SEmitParams& emit, const QuatTS& loc, const SPhysForces& forces, const FStaticBounds& opts, const Var& var) const;
};

struct FMaxEffectLife
{
	OPT_STRUCT(FMaxEffectLife)
	OPT_VAR(bool, bAllChildren)
	OPT_VAR(bool, bIndirectChildren)
	OPT_VAR(bool, bParticleLife)
	OPT_VAR(float, fEmitterMaxLife)
};

struct SEffectCounts
{
	int nLoaded, nUsed, nEnabled, nActive;

	SEffectCounts()	{ ZeroStruct(*this); }
};


/*!	CParticleEffect implements IParticleEffect interface and contain all components necessary to
    to create the effect
 */
class CParticleEffect : public IParticleEffect, public Cry3DEngineBase, public stl::intrusive_linked_list_node<CParticleEffect>
{
public:
	CParticleEffect();
	CParticleEffect(const CParticleEffect& in, bool bResetInheritance = false);
	CParticleEffect(const char* sName);
	CParticleEffect(const ParticleParams& params);
	~CParticleEffect();

	// IParticle interface.
	virtual int               GetVersion() const override { return 1; }
	virtual IParticleEmitter* Spawn(const ParticleLoc& loc, const SpawnParams* pSpawnParams = NULL) override;

	virtual void              SetName(const char* sFullName) override;
	virtual const char*       GetName() const override { return m_strName.c_str(); }
	virtual stack_string      GetFullName() const override;

	virtual void              SetEnabled(bool bEnabled) override;
	virtual bool              IsEnabled(uint options = 0) const override;

	virtual bool              IsTemporary() const override;

	//////////////////////////////////////////////////////////////////////////
	//! Load resources, required by this particle effect (Textures and geometry).
	virtual bool LoadResources() override { return LoadResources(true); }
	virtual void UnloadResources() override	{ UnloadResources(true); }

	//////////////////////////////////////////////////////////////////////////
	// Child particle systems.
	//////////////////////////////////////////////////////////////////////////
	virtual int              GetChildCount() const override
	{ return m_children.size(); }
	virtual IParticleEffect* GetChild(int index) const override
	{ return &m_children[index]; }

	virtual void             ClearChilds() override;
	virtual void             InsertChild(int slot, IParticleEffect* pEffect) override;
	virtual int              FindChild(IParticleEffect* pEffect) const override;

	virtual void             SetParent(IParticleEffect* pParent) override;
	virtual IParticleEffect* GetParent() const override
	{ return m_parent; }

	//////////////////////////////////////////////////////////////////////////
	virtual void                  SetParticleParams(const ParticleParams& params) override;
	virtual const ParticleParams& GetParticleParams() const override;
	virtual const ParticleParams& GetDefaultParams() const override;

	virtual void                  Serialize(XmlNodeRef node, bool bLoading, bool bAll) override;
	virtual void                  Serialize(Serialization::IArchive& ar) override;
	virtual void                  Reload(bool bAll) override;

	virtual IParticleAttributes&  GetAttributes();

	virtual void                  GetMemoryUsage(ICrySizer* pSizer) const override;

	// Further interface.

	const ResourceParticleParams& GetParams() const
	{
		assert(m_pParticleParams);
		return *m_pParticleParams;
	}

	void                  InstantiateParams();
	const ParticleParams& GetDefaultParams(ParticleParams::EInheritance eInheritance, int nVersion) const;
	void                  PropagateParticleParams(const ParticleParams& params);

	const char*           GetBaseName() const;

	bool                  IsNull() const
	{
		// Empty placeholder effect.
		return !m_pParticleParams && m_children.empty();
	}
	bool                   ResourcesLoaded(bool bAll) const;
	bool                   LoadResources(bool bAll, cstr sSource = 0) const;
	void                   UnloadResources(bool bAll) const;
	bool                   IsActive() const { return IsEnabled(eCheckFeatures | eCheckConfig | eCheckChildren); }

	CParticleEffect*       FindChild(const char* szChildName) const;
	const CParticleEffect* FindActiveEffect(int nVersion) const;

	CParticleEffect*       GetIndirectParent() const;

	float                  Get(FMaxEffectLife const& opts) const;
	float                  GetMaxParticleFullLife() const
	{
		return Get(FMaxEffectLife().bParticleLife(1).bIndirectChildren(1));
	}
	float GetMaxParticleSize(bool bParent = false) const;
	float GetEquilibriumAge(bool bAll) const;

	void  GetEffectCounts(SEffectCounts& counts) const;

private:

	//////////////////////////////////////////////////////////////////////////
	// Name of effect or sub-effect, minimally qualified.
	// For top-level effects, this includes library.group.
	// For all child effects, is just the base name.
	string                  m_strName;
	ResourceParticleParams* m_pParticleParams;

	//! Parenting.
	CParticleEffect*               m_parent;
	SmartPtrArray<CParticleEffect> m_children;
};

//
// Travel utilities
//
namespace Travel
{
#define fDRAG_APPROX_THRESHOLD 0.01f              // Max inaccuracy we allow in fast drag force approximation

inline float TravelDistance(float fV, float fDrag, float fT)
{
	float fDecay = fDrag * fT;
	if (fDecay < fDRAG_APPROX_THRESHOLD)
		return fV * fT * (1.f - fDecay);
	else
		// Compute accurate distance with drag.
		return fV / fDrag * (1.f - expf(-fDecay));
}

inline float TravelSpeed(float fV, float fDrag, float fT)
{
	float fDecay = fDrag * fT;
	if (fDecay < fDRAG_APPROX_THRESHOLD)
		// Fast approx drag computation.
		return fV * (1.f - fDecay);
	else
		return fV * expf(-fDecay);
}

inline void Travel(Vec3& vPos, Vec3& vVel, float fTime, const SForceParams& forces)
{
	// Analytically compute new velocity and position, accurate for any time step.
	if (forces.fDrag * fTime >= fDRAG_APPROX_THRESHOLD)
	{
		//
		// Air resistance proportional to velocity is typical for slower laminar movement.
		// For drag d (units in 1/time), wind W, and gravity G:
		//
		//		V' = d (W-V) + G
		//
		//	The analytic solution is:
		//
		//		VT = G/d + W,									terminal velocity
		//		V = (V-VT) e^(-d t) + VT
		//		X = (V-VT) (1 - e^(-d t))/d + VT t
		//
		//	A faster approximation, accurate to 2nd-order t is:
		//
		//		e^(-d t) => 1 - d t + d^2 t^2/2
		//		X += V t + (G + (W-V) d) t^2/2
		//		V += (G + (W-V) d) t
		//

		float fInvDrag = 1.f / forces.fDrag;
		Vec3 vTerm = forces.vWind + forces.vAccel * fInvDrag;
		float fDecay = 1.f - expf(-forces.fDrag * fTime);
		float fT = fDecay * fInvDrag;
		vPos += vVel * fT + vTerm * (fTime - fT);
		vVel = Lerp(vVel, vTerm, fDecay);
	}
	else
	{
		// Fast approx drag computation.
		Vec3 vAccel = forces.vAccel + (forces.vWind - vVel) * forces.fDrag;
		vPos += vVel * fTime + vAccel * (fTime * fTime * 0.5f);
		vVel += vAccel * fTime;
	}
}

float TravelDistanceApprox(const Vec3& vVel, float fTime, const SForceParams& forces);

float TravelVolume(const AABB& bbSource, const AABB& bbTravel, float fDist, float fSize);
};

//
// Other utilities
//

IStatObj::SSubObject* GetSubGeometry(IStatObj* pParent, int i);
int                   GetSubGeometryCount(IStatObj* pParent);
