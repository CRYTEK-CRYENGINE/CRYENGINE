// Copyright 2015-2018 Crytek GmbH / Crytek Group. All rights reserved. 

#pragma once

#include "StdAfx.h"
#include "FeatureCommon.h"

namespace pfx2
{

struct SContactPoint
{
	bool  m_collided;
	bool  m_sliding;
	bool  m_stopped;
	bool  m_ignore;

	float m_time;
	Vec3  m_point;
	Vec3  m_normal;
	const ISurfaceType* m_pSurfaceType;
	SContactPoint() { ZeroStruct(*this); }
};

struct SContactPredict: SContactPoint
{
	Vec3 m_testDir;
};

template<typename T, typename F = float> struct QuadPathT;
typedef QuadPathT<Vec3> QuadPath;

extern TDataType<SContactPoint> EPDT_ContactPoint;

SERIALIZATION_ENUM_DECLARE(ECollisionLimitMode, ,
	Unlimited,
	Ignore,
	Stop,
	Kill
	)

//////////////////////////////////////////////////////////////////////////
// CFeatureCollision

class CFeatureCollision : public CParticleFeature
{
public:
	CRY_PFX2_DECLARE_FEATURE

public:
	virtual void AddToComponent(CParticleComponent* pComponent, SComponentParams* pParams) override;
	virtual void Serialize(Serialization::IArchive& ar) override;
	virtual void PostInitParticles(CParticleComponentRuntime& runtime) override;
	virtual void PostUpdateParticles(CParticleComponentRuntime& runtime) override;

	bool  IsActive() const           { return m_terrain || m_staticObjects || m_dynamicObjects; }
	float GetElasticity() const      { return m_elasticity; }
	int   GetRayTraceFilter() const;

private:
	void DoCollisions(CParticleComponentRuntime& runtime) const;
	bool DoCollision(SContactPredict& contactNext, SContactPoint& contact, float& collideSpeed, QuadPath& path) const;
	bool PathWorldIntersection(SContactPoint& contact, const QuadPath& path, float t0, float t1, bool splitInflection) const;
	bool RayWorldIntersection(SContactPoint& contact, const Vec3& startIn, const Vec3& rayIn) const;
	bool TestSlide(SContactPoint& contact, Vec3& pos, Vec3& vel, const Vec3& acc) const;
	void Collide(SContactPoint& contact, float& collideSpeed, QuadPath& path) const;

	template<typename TCollisionLimit>
	void UpdateCollisionLimit(CParticleComponentRuntime& runtime) const;

	UUnitFloat          m_elasticity;
	UUnitFloat          m_friction;
	ECollisionLimitMode m_collisionsLimitMode = ECollisionLimitMode::Unlimited;
	UBytePos            m_maxCollisions       = 0;
	bool                m_rotateToNormal      = false;
	bool                m_terrain             = true;
	bool                m_staticObjects       = true;
	bool                m_dynamicObjects      = false;
	bool                m_water               = false;

	bool                m_doPrediction        = true;
	int                 m_objectFilter        = 0;
};

//////////////////////////////////////////////////////////////////////////
// CFeatureGPUCollision

class CFeatureGPUCollision : public CParticleFeature
{
public:
	CRY_PFX2_DECLARE_FEATURE

	CFeatureGPUCollision()
		: m_offset(0.5f)
		, m_radius(0.5f)
		, m_restitution(0.5f)
	{}

	virtual void AddToComponent(CParticleComponent* pComponent, SComponentParams* pParams) override;
	virtual void Serialize(Serialization::IArchive& ar) override;
private:
	float m_offset;
	float m_radius;
	float m_restitution;
};

}
