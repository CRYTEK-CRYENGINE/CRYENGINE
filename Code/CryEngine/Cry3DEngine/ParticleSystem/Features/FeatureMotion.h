// Copyright 2001-2017 Crytek GmbH / Crytek Group. All rights reserved. 

#pragma once

#include "ParticleSystem/ParticleFeature.h"
#include "ParamMod.h"

namespace pfx2
{

class  CFeatureCollision;
struct IFeatureMotion;

struct ILocalEffector : public _i_reference_target_t
{
public:
	bool         IsEnabled() const { return m_enabled; }
	virtual void AddToMotionFeature(CParticleComponent* pComponent, IFeatureMotion* pFeature) = 0;
	virtual void ComputeEffector(const SUpdateContext& context, IOVec3Stream localVelocities, IOVec3Stream localAccelerations) {}
	virtual void ComputeMove(const SUpdateContext& context, IOVec3Stream localMoves, float fTime) {}
	virtual void Serialize(Serialization::IArchive& ar);
	virtual void SetParameters(gpu_pfx2::IParticleFeatureGpuInterface* gpuInterface) const {}
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

	CFeatureMotionPhysics();

	// CParticleFeature
	virtual EFeatureType GetFeatureType() override { return EFT_Motion; }
	virtual void         AddToComponent(CParticleComponent* pComponent, SComponentParams* pParams) override;

	virtual void Serialize(Serialization::IArchive& ar) override;
	virtual void InitParticles(const SUpdateContext& context) override;
	virtual void Update(const SUpdateContext& context) override;
	// ~CParticleFeature

	// IFeatureMotion
	virtual void AddToComputeList(ILocalEffector* pEffector) override;
	virtual void AddToMoveList(ILocalEffector* pEffector) override;
	// ~IFeatureMotion

private:
	void Integrate(const SUpdateContext& context);
	void LinearIntegral(const SUpdateContext& context);
	void QuadraticIntegral(const SUpdateContext& context);
	void DragFastIntegral(const SUpdateContext& context);
	void AngularLinearIntegral(const SUpdateContext& context);
	void AngularDragFastIntegral(const SUpdateContext& context);

	std::vector<PLocalEffector>          m_localEffectors;
	std::vector<ILocalEffector*>         m_computeList;
	std::vector<ILocalEffector*>         m_moveList;
	CParamMod<SModParticleField, SFloat> m_gravity;
	CParamMod<SModParticleField, UFloat> m_drag;
	Vec3        m_uniformAcceleration;
	Vec3        m_uniformWind;
	UFloat      m_windMultiplier;
	UFloat      m_angularDragMultiplier;
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
	virtual void Update(const SUpdateContext& context) override;

private:
	typedef TValue<float, USoftLimit<10>, ConvertScale<1000, 1>> UDensity;

	EPhysicsType m_physicsType;
	ESurfaceType m_surfaceType;
	SFloat       m_gravity;
	UFloat       m_drag;
	UDensity     m_density;
	SFloat       m_thickness;
	Vec3         m_uniformAcceleration;
};

extern EParticleDataType
  EPVF_Acceleration,
  EPVF_VelocityField;

}
