// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include "ParticleSystem/ParticleFeature.h"
#include "ParamMod.h"
#include "ParticleEnviron.h"

namespace pfx2
{

class  CFeatureCollision;
struct IFeatureMotion;

struct ILocalEffector : public _i_reference_target_t
{
public:
	bool         IsEnabled() const { return m_enabled; }
	virtual void AddToMotionFeature(CParticleComponent* pComponent, IFeatureMotion* pFeature) = 0;
	virtual uint ComputeEffector(const SUpdateContext& context, IOVec3Stream localVelocities, IOVec3Stream localAccelerations) { return 0; }
	virtual void ComputeMove(const SUpdateContext& context, IOVec3Stream localMoves, float fTime) {}
	virtual void Serialize(Serialization::IArchive& ar);
	virtual void SetParameters(gpu_pfx2::IParticleFeature* gpuInterface) const {}
private:
	SEnable m_enabled;
};

struct IFeatureMotion
{
	virtual void AddToComputeList(ILocalEffector* pEffector) = 0;
	virtual void AddToMoveList(ILocalEffector* pEffector) = 0;
};

//////////////////////////////////////////////////////////////////////////

class CFeatureMotionPhysics : public CParticleFeature, public IFeatureMotion
{
private:
	typedef _smart_ptr<ILocalEffector> PLocalEffector;

public:
	CRY_PFX2_DECLARE_FEATURE

	static uint DefaultForType() { return EFT_Motion; }

	CFeatureMotionPhysics();

	// CParticleFeature
	virtual EFeatureType GetFeatureType() override { return EFT_Motion; }
	virtual void         AddToComponent(CParticleComponent* pComponent, SComponentParams* pParams) override;

	virtual void Serialize(Serialization::IArchive& ar) override;
	virtual void InitParticles(const SUpdateContext& context) override;
	virtual void UpdateParticles(const SUpdateContext& context) override;
	// ~CParticleFeature

	// IFeatureMotion
	virtual void AddToComputeList(ILocalEffector* pEffector) override;
	virtual void AddToMoveList(ILocalEffector* pEffector) override;
	// ~IFeatureMotion

private:
	using SArea = SPhysEnviron::SArea;
	uint ComputeEffectors(const SUpdateContext& context, const SArea& area) const;
	void Integrate(const SUpdateContext& context);
	void AngularLinearIntegral(const SUpdateContext& context);
	void AngularDragFastIntegral(const SUpdateContext& context);

	CParamMod<SModParticleField, SFloat> m_gravity;
	CParamMod<SModParticleField, UFloat> m_drag;
	UFloat                               m_windMultiplier;
	UFloat                               m_angularDragMultiplier;
	bool                                 m_perParticleForceComputation;
	Vec3                                 m_uniformAcceleration;
	Vec3                                 m_uniformWind;
	std::vector<PLocalEffector>          m_localEffectors;

	std::vector<ILocalEffector*>         m_computeList;
	std::vector<ILocalEffector*>         m_moveList;
	uint                                 m_environFlags;
};

//////////////////////////////////////////////////////////////////////////

SERIALIZATION_DECLARE_ENUM(EPhysicsType,
	Particle,
	Mesh
	)

// Dynamic enum for surface types
typedef DynamicEnum<struct SSurfaceType> ESurfaceType;

class CFeatureMotionCryPhysics : public CParticleFeature
{
public:
	CRY_PFX2_DECLARE_FEATURE

	CFeatureMotionCryPhysics();

	virtual EFeatureType GetFeatureType() override { return EFT_Motion; }

	virtual void AddToComponent(CParticleComponent* pComponent, SComponentParams* pParams) override;
	virtual void Serialize(Serialization::IArchive& ar) override;
	virtual void PostInitParticles(const SUpdateContext& context) override;
	virtual void UpdateParticles(const SUpdateContext& context) override;
	virtual void DestroyParticles(const SUpdateContext& context) override;

private:
	typedef TValue<float, ConvertScale<1000, 1, THardMin<0>>> UDensity;

	EPhysicsType m_physicsType;
	ESurfaceType m_surfaceType;
	SFloat       m_gravity;
	UFloat       m_drag;
	UDensity     m_density;
	SFloat       m_thickness;
	Vec3         m_uniformAcceleration;
};

extern TDataType<Vec3>
	EPVF_Acceleration,
	EPVF_VelocityField;

}
