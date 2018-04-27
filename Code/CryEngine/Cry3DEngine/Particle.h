// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#ifndef PARTICLE_H
#define PARTICLE_H

#include "ParticleEffect.h"
#include "ParticleMemory.h"
#include "ParticleEnviron.h"
#include "ParticleUtils.h"
#include <CryRenderer/RenderElements/CREParticle.h>

class CParticleContainer;
class CParticleSubEmitter;
class CParticleEmitter;
struct SParticleRenderData;
struct SParticleVertexContext;
struct SParticleUpdateContext;
struct SLocalRenderVertices;
struct STargetForces;

//////////////////////////////////////////////////////////////////////////
#define fMAX_COLLIDE_DEVIATION 0.1f
#define fMAX_DENSITY_ADJUST    32.f

// dynamic particle data
// To do opt: subclass for geom particles.

//////////////////////////////////////////////////////////////////////////
struct STimeState
{
protected:
	float m_fAge;                       // Current age.
	float m_fStopAge;                   // Age of death.
	float m_fCollideAge;                // Age of first collision (for SpawnOnCollision children).

public:
	STimeState()
		: m_fAge(0.f), m_fStopAge(0.f), m_fCollideAge(fHUGE) {}

	float GetAge() const
	{ return m_fAge; }
	float GetStopAge() const
	{ return m_fStopAge; }
	float GetCollideAge() const
	{ return m_fCollideAge; }

	float GetRelativeAge(float fAgeAdjust = 0.f) const
	{
		float fRelativeAge = div_min(max(m_fAge + fAgeAdjust, 0.f), m_fStopAge, 1.f);
		assert(fRelativeAge >= 0.f && fRelativeAge <= 1.f);
		return fRelativeAge;
	}
	bool IsAlive(float fAgeAdjust = 0.f) const
	{ return m_fAge + fAgeAdjust < m_fStopAge; }

	void Start(float fAgeAdjust = 0.f)
	{ m_fAge = -fAgeAdjust; m_fStopAge = m_fCollideAge = fHUGE; }
	void Stop(float fAgeAdjust = 0.f)
	{ m_fStopAge = min(m_fStopAge, m_fAge + fAgeAdjust); }
	void Collide(float fAgeAdjust = 0.f)
	{ m_fCollideAge = m_fAge + fAgeAdjust; }
	void Kill()
	{ m_fStopAge = -fHUGE; }
};

struct SMoveState
{
protected:
	QuatTS    m_Loc;            // Position, orientation, and size.
	float     m_fAngle;         // Scalar angle, for camera-facing rotation.
	Velocity3 m_Vel;            // Linear and rotational velocity.

public:
	SMoveState()
		: m_Loc(IDENTITY), m_fAngle(0.0f), m_Vel(ZERO) {}

	QuatTS const&    GetLocation() const
	{ return m_Loc; }
	Velocity3 const& GetVelocity() const
	{ return m_Vel; }
	Vec3             GetVelocityAt(Vec3 const& vWorldPos) const
	{ return m_Vel.VelocityAt(vWorldPos - m_Loc.t); }

	void PreTransform(QuatTS const& qp)
	{
		m_Loc = m_Loc * qp;
		m_Vel.vLin = m_Vel.vLin * qp.q * qp.s;
		m_Vel.vRot = m_Vel.vRot * qp.q;
	}
	void Transform(QuatTS const& qp)
	{
		m_Loc = qp * m_Loc;
		m_Vel.vLin = qp.q * m_Vel.vLin * qp.s;
		m_Vel.vRot = qp.q * m_Vel.vRot;
	}
	void OffsetPosition(const Vec3& delta)
	{
		m_Loc.t += delta;
	}
};

struct SParticleState : STimeState, SMoveState
{
	friend class CParticle;
};

//////////////////////////////////////////////////////////////////////////
class CRY_ALIGN(16) CParticleSource: public Cry3DEngineBase, public _plain_reference_target<int>, public SParticleState, public GeomRef
{
public:

	CParticleSource()
		: m_pEmitter(0)
	{
	}

	const CParticleSubEmitter* GetEmitter() const
	{ return m_pEmitter; }
	const GeomRef&             GetEmitGeom() const
	{ return *this; }

	using _plain_reference_target<int>::AddRef;
	using _plain_reference_target<int>::Release;

protected:

	CParticleSubEmitter* m_pEmitter;              // Parent emitter, if this is a child emitter.
};

//////////////////////////////////////////////////////////////////////////
class CRY_ALIGN(32) CParticle: public CParticleSource
{
public:

	~CParticle();

	void Init(SParticleUpdateContext const & context, float fAge, CParticleSubEmitter * pEmitter, const EmitParticleData &data);
	void Update(SParticleUpdateContext const & context, float fUpdateTime, bool bNew = false);
	void GetPhysicsState();

	float GetMinDist(Vec3 const& vP) const
	{
		static const float fSizeFactor = 1.0f;
		return (m_Loc.t - vP).GetLengthFast() - m_Loc.s * fSizeFactor;
	}

	float GetAlphaMod() const
	{
		return GetParams().fAlpha.GetValueFromBase(m_BaseMods.Alpha, GetRelativeAge());
	}

	ILINE uint16 GetEmitterSequence() const
	{
		return m_nEmitterSequence;
	}

	void Hide()
	{
		// Disable rendering.
		m_BaseMods.Alpha = 0;
	}

	void UpdateBounds(AABB & bb, SParticleState const & state) const;
	void UpdateBounds(AABB& bb) const
	{
		UpdateBounds(bb, *this);
	}
	void OffsetPosition(const Vec3 &delta);

	Vec3 GetVisualVelocity(SParticleState const & state, float fTime = 0.f) const;

	// Associated structures.
	CParticleContainer&           GetContainer() const
	{ return *m_pContainer; }
	CParticleSource&              GetSource() const;
	CParticleEmitter&             GetMain() const;
	ResourceParticleParams const& GetParams() const;

	// Rendering functions.
	bool RenderGeometry(SRendParams & RenParamsShared, SParticleVertexContext & context, const SRenderingPassInfo &passInfo) const;
	void AddLight(const SRendParams &RenParams, const SRenderingPassInfo &passInfo) const;
	void SetVertices(SLocalRenderVertices & alloc, SParticleVertexContext & context, uint8 uAlpha) const;
	void GetTextureRect(RectF & rectTex, Vec3 & vTexBlend) const;
	void ComputeRenderData(SParticleRenderData & RenderData, const SParticleVertexContext &context, float fObjectSize = 1.f) const;
	float ComputeRenderAlpha(const SParticleRenderData &RenderData, float fRelativeAge, SParticleVertexContext & context) const;
	void GetRenderMatrix(Vec3 & vX, Vec3 & vY, Vec3 & vZ, Vec3 & vT, const QuatTS &loc, const SParticleRenderData &RenderData, const SParticleVertexContext &context) const;
	void GetRenderMatrix(Matrix34& mat, const QuatTS& loc, const SParticleRenderData& RenderData, const SParticleVertexContext& context) const
	{
		Vec3 av[4];
		GetRenderMatrix(av[0], av[1], av[2], av[3], loc, RenderData, context);
		mat.SetFromVectors(av[0], av[1], av[2], av[3]);
	}
	void SetVertexLocation(SVF_Particle& Vert, const QuatTS& loc, const SParticleRenderData& RenderData, const SParticleVertexContext& context) const
	{
		Vec3 vZ;
		GetRenderMatrix(Vert.xaxis, vZ, Vert.yaxis, Vert.xyz, loc, RenderData, context);
	}

#ifdef PARTICLE_EDITOR_FUNCTIONS
	void UpdateAllocations(int nPrevHistorySteps);
#endif
	static size_t GetAllocationSize(const CParticleContainer * pCont);
	void GetMemoryUsage(ICrySizer* pSizer) const { /*nothing*/ }

private:

	//////////////////////////////////////////////////////////////////////////
	// For particles with tail, keeps history of previous locations.
	struct CRY_ALIGN(16) SParticleHistory
	{
		float fAge;
		QuatTS Loc;

		bool IsUsed() const { return fAge >= 0.f; }
		void SetUnused()    { fAge = -1.f; }
	};

	// Track sliding state.
	struct SSlideInfo
	{
		_smart_ptr<IPhysicalEntity>
		      pEntity;              // Physical entity hit.
		Vec3  vNormal;              // Normal of sliding surface.
		float fFriction;            // Sliding friction, proportional to normal force.
		float fSlidingTime;         // Cumulative amount of time sliding.

		SSlideInfo()
		{ Clear(); }
		void Clear()
		{ ClearSliding(Vec3(ZERO)); }
		void ClearSliding(const Vec3& vNormal_)
		{
			pEntity = 0;
			fFriction = 0;
			fSlidingTime = -1.f;
			vNormal = vNormal_;
		}
		void SetSliding(IPhysicalEntity* pEntity_, const Vec3& vNormal_, float fFriction_)
		{
			pEntity = pEntity_;
			vNormal = vNormal_;
			fFriction = fFriction_;
			fSlidingTime = 0.f;
		}
		bool IsSliding() const
		{ return fSlidingTime >= 0.f; }
	};

	// Track predicted collisions.
	struct SHitInfo
	{
		Vec3  vPos;                   // Hit position.
		Vec3  vPathDir;               // Direction of reverse path.
		float fPathLength;            // Length of reverse path.
		Vec3  vNormal;                // Normal of hit surface.
		_smart_ptr<IPhysicalEntity>
		      pEntity;                // Physical entity hit.
		int   nSurfaceIdx;            // Surface index of hit; -1 if no hit.

		SHitInfo()
		{ Clear(); }
		void Clear()
		{
			nSurfaceIdx = -1;
			fPathLength = 0.f;
			pEntity = 0;
		}

		bool HasPath() const
		{ return fPathLength > 0.f; }
		bool HasHit() const
		{ return nSurfaceIdx >= 0; }
		void SetMiss(const Vec3& vStart_, const Vec3& vEnd_)
		{ SetHit(vStart_, vEnd_, Vec3(0.f)); }
		void SetHit(const Vec3& vStart_, const Vec3& vEnd_, const Vec3& vNormal_, int nSurfaceIdx_ = -1, IPhysicalEntity* pEntity_ = 0)
		{
			vPos = vEnd_;
			vPathDir = vStart_ - vEnd_;
			fPathLength = vPathDir.GetLength();
			if (fPathLength > 0.f)
				vPathDir /= fPathLength;
			vNormal = vNormal_;
			nSurfaceIdx = nSurfaceIdx_;
			pEntity = pEntity_;
		}

		// If path invalid, returns false.
		// If path valid returns true; if hit.dist < 1, then hit was matched.
		bool TestHit(ray_hit& hit, const Vec3& vPos0, const Vec3& vPos1, const Vec3& vVel0, const Vec3& vVel1, float fMaxDev, float fRadius = 0.f) const;
	};

	struct SCollisionInfo
	{
		SSlideInfo Sliding;
		SHitInfo   Hit;
		int32      nCollisionLeft;          // Number of collisions this particle is allowed to have: max = unlimited; 0 = no collide, -1 = stop

		SCollisionInfo(int32 nMaxCollisions = 0)
			: nCollisionLeft(nMaxCollisions ? nMaxCollisions : 0x7FFFFFFF) {}

		void Clear()
		{
			Sliding.Clear();
			Hit.Clear();
		}

		int32 Collide()
		{ return --nCollisionLeft; }
		void  Stop()
		{ nCollisionLeft = -1; }
		bool  CanCollide() const
		{ return nCollisionLeft > 0; }
		bool  Stopped() const
		{ return nCollisionLeft < 0; }
	};

	// Constant values.

	SParticleHistory* m_aPosHistory;                // History of positions, for tail. Allocated and maintained by particle.
	SCollisionInfo* m_pCollisionInfo;               // Predicted collision info.

	// Base modifications (random variation, emitter strength) for this particle of variable parameters.
	// Stored as compressed fraction from 0..1.
	struct SBaseMods
	{
		// Random modifiers for unsigned params.
		TFixed<uint8, 1>
		Size,
		  Aspect,
		  StretchOrTail,                            // Used for either stretch or tail (exclusive features)

		  AirResistance,
		  Turbulence3DSpeed,
		  TurbulenceSize,

		  LightSourceIntensity,
		  LightSourceRadius,
		  Alpha;

		// Random modifiers for signed params.
		TFixed<int8, 1>
		PivotX,
		  PivotY,
		  GravityScale,
		  TurbulenceSpeed,
		  fTargetRadius;

		Color3B Color;

		void    Init()
		{
			memset(this, 0xFF, sizeof(*this));
		}
	} m_BaseMods;

	uint8 m_nTileVariant;                           // Selects texture tile.

	uint16 m_nEmitterSequence : 15,                 // Which sequence particle is part of (for connected rendering).
	       m_bFlippedTexture : 1;                   // Reverse texture U.

	// External references.
	CParticleContainer* m_pContainer;       // Container particle lives in.

	// Functions.
private:

	void        SetState(SParticleState const& state) { static_cast<SParticleState&>(*this) = state; }
	float       GetBaseRadius() const                 { return GetParams().GetMaxObjectSize(GetMesh()); }
	float       GetVisibleRadius() const              { assert(m_Loc.s >= 0.f); return m_Loc.s * GetBaseRadius(); }
	float       GetPhysicalRadius() const             { return GetVisibleRadius() * GetParams().fThickness; }
	inline Vec3 GetNormal() const                     { return m_Loc.q.GetColumn1(); }

	void InitPos(SParticleUpdateContext const & context, QuatTS const & loc, float fEmissionRelAge);
	Vec3 GenerateOffset(SParticleUpdateContext const & context);

	void AddPosHistory(SParticleState const & stateNew);
	void AlignTo(SParticleState & state, const Vec3 &vNormal) const;
	float UpdateAlignment(SParticleState & state, SParticleUpdateContext const & context, Plane const & plWater, float fTime = 0.f) const;
	Vec3 VortexRotation(SParticleState const & state, bool bVelocity, float fTime = 0.f) const;
	void TargetMovement(ParticleTarget const & target, SParticleState & state, float fTime, float fRelativeAge) const;
	float TravelSlide(SParticleState & state, SSlideInfo & sliding, float fTime, const Vec3 &vExtAccel, float fMaxSlide, float fMinStepTime) const;
	void Move(SParticleState & state, float fTime, STargetForces const & forces) const;
	float MoveLinear(SParticleState & state, SCollisionInfo & coll, float fTime, STargetForces const & forces, float fMaxLinearDev, float fMaxSlideDev, float fMinStepTime) const;
	bool CheckCollision(ray_hit & hit, float fTime, SParticleUpdateContext const & context, const STargetForces &forces, const SParticleState &stateNew, SCollisionInfo & collNew);

	void Physicalize();
	int GetSurfaceIndex() const;
	void GetCollisionParams(int nCollSurfaceIdx, float& fElasticity, float& fDrag) const;

	void SetTailVertices(const SVF_Particle &BaseVert, SParticleRenderData RenderData, SLocalRenderVertices & alloc, SParticleVertexContext const & context) const;

	void DebugBounds(SParticleState const & state) const
#if defined(_DEBUG)
	;
#else
	{}
#endif

};

#endif // PARTICLE
