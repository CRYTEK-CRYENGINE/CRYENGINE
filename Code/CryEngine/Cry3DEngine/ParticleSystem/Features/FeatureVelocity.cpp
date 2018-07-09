// Copyright 2015-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include <CrySerialization/Math.h>
#include "ParticleSystem/ParticleFeature.h"
#include "ParticleSystem/ParticleEmitter.h"
#include "ParamMod.h"


namespace pfx2
{

class CFeatureVelocityCone : public CParticleFeature
{
public:
	CRY_PFX2_DECLARE_FEATURE

	CFeatureVelocityCone()
		: m_velocity(0.0f)
		, m_angle(0.0f)
		, m_axis(0.0f, 0.0f, 1.0f)
	{}

	virtual void AddToComponent(CParticleComponent* pComponent, SComponentParams* pParams) override
	{
		pComponent->InitParticles.add(this);
		pComponent->UpdateGPUParams.add(this);
		m_angle.AddToComponent(pComponent, this);
		m_velocity.AddToComponent(pComponent, this);
	}

	virtual void Serialize(Serialization::IArchive& ar) override
	{
		CParticleFeature::Serialize(ar);
		ar(m_angle, "Angle", "Angle");
		ar(m_velocity, "Velocity", "Velocity");
		ar(m_axis, "Axis", "Axis");
	}

	virtual void InitParticles(CParticleComponentRuntime& runtime) override
	{
		CRY_PFX2_PROFILE_DETAIL;

		CParticleContainer& parentContainer = runtime.GetParentContainer();
		CParticleContainer& container = runtime.GetContainer();
		const Quat defaultQuat = runtime.GetEmitter()->GetLocation().q;
		const IPidStream parentIds = container.GetIPidStream(EPDT_ParentId);
		const IQuatStream parentQuats = parentContainer.GetIQuatStream(EPQF_Orientation, defaultQuat);
		const Quat axisQuat = Quat::CreateRotationV0V1(Vec3(0.0f, 0.0f, 1.0f), m_axis.GetNormalizedSafe());
		IOVec3Stream velocities = container.GetIOVec3Stream(EPVF_Velocity);
		const float baseAngle = m_angle.GetBaseValue();
		const float invBaseAngle = rcp_fast(max(FLT_EPSILON, baseAngle));

		STempInitBuffer<float> angles(runtime, m_angle);
		STempInitBuffer<float> velocityMults(runtime, m_velocity);

		for (auto particleId : runtime.SpawnedRange())
		{
			const TParticleId parentId = parentIds.Load(particleId);
			const Vec3 wVelocity0 = velocities.Load(particleId);
			const Quat wQuat = parentQuats.SafeLoad(parentId);
			const float angleMult = angles.SafeLoad(particleId);
			const float velocity = velocityMults.SafeLoad(particleId);

			const Vec2 disc = runtime.Chaos().RandCircle();
			const float angle = sqrtf(angleMult * invBaseAngle) * baseAngle;

			float as, ac;
			sincos(angle, &as, &ac);
			const Vec3 dir = axisQuat * Vec3(disc.x * as, disc.y * as, ac);
			const Vec3 oVelocity = dir * velocity;
			const Vec3 wVelocity1 = wVelocity0 + wQuat * oVelocity;
			velocities.Store(particleId, wVelocity1);
		}
	}

	virtual void UpdateGPUParams(CParticleComponentRuntime& runtime, gpu_pfx2::SUpdateParams& params) override
	{
		params.angle = m_angle.GetValueRange(runtime)(0.5f);
		params.velocity = m_velocity.GetValueRange(runtime)(0.5f);
		params.initFlags |= gpu_pfx2::eFeatureInitializationFlags_VelocityCone;
	}

private:
	CParamMod<EDD_PerParticle, UAngle180> m_angle;
	CParamMod<EDD_PerParticle, UFloat10>  m_velocity;
	Vec3                                  m_axis;
};

CRY_PFX2_IMPLEMENT_FEATURE(CParticleFeature, CFeatureVelocityCone, "Velocity", "Cone", colorVelocity);

//////////////////////////////////////////////////////////////////////////

class CFeatureVelocityDirectional : public CParticleFeature
{
public:
	CRY_PFX2_DECLARE_FEATURE

	CFeatureVelocityDirectional()
		: m_direction(ZERO)
		, m_scale(1.0f)
	{}

	virtual void AddToComponent(CParticleComponent* pComponent, SComponentParams* pParams) override
	{
		pComponent->InitParticles.add(this);
		pComponent->UpdateGPUParams.add(this);
		m_scale.AddToComponent(pComponent, this);
	}

	virtual void Serialize(Serialization::IArchive& ar) override
	{
		CParticleFeature::Serialize(ar);
		ar(m_direction, "Velocity", "Velocity");
		ar(m_scale, "Scale", "Scale");
	}

	virtual void InitParticles(CParticleComponentRuntime& runtime) override
	{
		CRY_PFX2_PROFILE_DETAIL;

		CParticleContainer& parentContainer = runtime.GetParentContainer();
		CParticleContainer& container = runtime.GetContainer();
		const Quat defaultQuat = runtime.GetEmitter()->GetLocation().q;
		IPidStream parentIds = container.GetIPidStream(EPDT_ParentId);
		IOVec3Stream velocities = container.GetIOVec3Stream(EPVF_Velocity);
		IQuatStream parentQuats = parentContainer.GetIQuatStream(EPQF_Orientation, defaultQuat);
		STempInitBuffer<float> scales(runtime, m_scale);

		for (auto particleId : runtime.SpawnedRange())
		{
			const TParticleId parentId = parentIds.Load(particleId);
			const Quat wQuat = parentQuats.SafeLoad(parentId);
			const float scale = scales.SafeLoad(particleId);
			const Vec3 wVelocity0 = velocities.Load(particleId);
			const Vec3 oVelocity = m_direction * scale;
			const Vec3 wVelocity1 = wVelocity0 + wQuat * oVelocity;
			velocities.Store(particleId, wVelocity1);
		}
	}

	virtual void UpdateGPUParams(CParticleComponentRuntime& runtime, gpu_pfx2::SUpdateParams& params) override
	{
		params.direction = m_direction;
		params.directionScale = m_scale.GetValueRange(runtime)(0.5f);
		params.initFlags |= gpu_pfx2::eFeatureInitializationFlags_VelocityDirectional;
	}

private:
	Vec3                                 m_direction;
	CParamMod<EDD_PerParticle, UFloat10> m_scale;
};

CRY_PFX2_IMPLEMENT_FEATURE(CParticleFeature, CFeatureVelocityDirectional, "Velocity", "Directional", colorVelocity);

//////////////////////////////////////////////////////////////////////////

class CFeatureVelocityOmniDirectional : public CParticleFeature
{
public:
	CRY_PFX2_DECLARE_FEATURE

	CFeatureVelocityOmniDirectional()
		: m_velocity(0.0f)
	{}

	virtual void AddToComponent(CParticleComponent* pComponent, SComponentParams* pParams) override
	{
		pComponent->InitParticles.add(this);
		pComponent->UpdateGPUParams.add(this);
		m_velocity.AddToComponent(pComponent, this);
	}

	virtual void Serialize(Serialization::IArchive& ar) override
	{
		CParticleFeature::Serialize(ar);
		ar(m_velocity, "Velocity", "Velocity");
	}

	virtual void InitParticles(CParticleComponentRuntime& runtime) override
	{
		CRY_PFX2_PROFILE_DETAIL;

		CParticleContainer& container = runtime.GetContainer();
		IOVec3Stream velocities = container.GetIOVec3Stream(EPVF_Velocity);

		STempInitBuffer<float> velocityMults(runtime, m_velocity);

		for (auto particleId : runtime.SpawnedRange())
		{
			const Vec3 wVelocity0 = velocities.Load(particleId);
			const float velocity = velocityMults.SafeLoad(particleId);
			const Vec3 oVelocity = runtime.Chaos().RandSphere() * velocity;
			const Vec3 wVelocity1 = wVelocity0 + oVelocity;
			velocities.Store(particleId, wVelocity1);
		}
	}

	virtual void UpdateGPUParams(CParticleComponentRuntime& runtime, gpu_pfx2::SUpdateParams& params) override
	{
		params.velocity = m_velocity.GetValueRange(runtime)(0.5f);
		params.initFlags |= gpu_pfx2::eFeatureInitializationFlags_VelocityOmniDirectional;
	}

private:
	CParamMod<EDD_PerParticle, UFloat10> m_velocity;
};

CRY_PFX2_IMPLEMENT_FEATURE(CParticleFeature, CFeatureVelocityOmniDirectional, "Velocity", "OmniDirectional", colorVelocity);

//////////////////////////////////////////////////////////////////////////

class CFeatureVelocityInherit : public CParticleFeature
{
public:
	CRY_PFX2_DECLARE_FEATURE

	CFeatureVelocityInherit()
		: m_scale(1.0f)
	{}

	virtual void AddToComponent(CParticleComponent* pComponent, SComponentParams* pParams) override
	{
		pComponent->InitParticles.add(this);
		pComponent->UpdateGPUParams.add(this);
		m_scale.AddToComponent(pComponent, this);
	}

	virtual void Serialize(Serialization::IArchive& ar) override
	{
		CParticleFeature::Serialize(ar);
		ar(m_scale, "Scale", "Scale");
	}

	virtual void InitParticles(CParticleComponentRuntime& runtime) override
	{
		CRY_PFX2_PROFILE_DETAIL;

		if (runtime.GetParentContainer().HasData(EPVF_AngularVelocity))
			InheritVelocity<true>(runtime);
		else
			InheritVelocity<false>(runtime);
	}

	template<bool hasRotation>
	void InheritVelocity(CParticleComponentRuntime& runtime)
	{
		CRY_PFX2_PROFILE_DETAIL;

		CParticleContainer& parentContainer = runtime.GetParentContainer();
		CParticleContainer& container = runtime.GetContainer();
		IPidStream parentIds = container.GetIPidStream(EPDT_ParentId);
		const IVec3Stream positions = container.GetIVec3Stream(EPVF_Position);
		const IVec3Stream parentPositions = parentContainer.GetIVec3Stream(EPVF_Position);
		const IVec3Stream parentVelocities = parentContainer.GetIVec3Stream(EPVF_Velocity);
		const IVec3Stream parentAngularVelocities = parentContainer.GetIVec3Stream(EPVF_AngularVelocity);
		IOVec3Stream velocities = container.GetIOVec3Stream(EPVF_Velocity);

		STempInitBuffer<float> scales(runtime, m_scale);

		for (auto particleId : runtime.SpawnedRange())
		{
			const float scale = scales.SafeLoad(particleId);
			const TParticleId parentId = parentIds.Load(particleId);
			const Vec3 wVelocity0 = velocities.Load(particleId);
			Vec3 wInheritVelocity = parentVelocities.Load(parentId);
			if (hasRotation)
			{
				const Vec3 wPos = positions.Load(particleId);
				const Vec3 wParentPos = parentPositions.Load(parentId);
				const Vec3 wParentAngularVelocity = parentAngularVelocities.Load(parentId);
				wInheritVelocity += wParentAngularVelocity ^ (wPos - wParentPos);
			}
			const Vec3 wVelocity1 = wVelocity0 + wInheritVelocity * scale;
			velocities.Store(particleId, wVelocity1);
		}
	}

	virtual void UpdateGPUParams(CParticleComponentRuntime& runtime, gpu_pfx2::SUpdateParams& params) override
	{
		params.velocityScale = m_scale.GetValueRange(runtime)(0.5f);
	}

private:
	CParamMod<EDD_PerParticle, SFloat> m_scale;
};

CRY_PFX2_IMPLEMENT_FEATURE(CParticleFeature, CFeatureVelocityInherit, "Velocity", "Inherit", colorVelocity);

//////////////////////////////////////////////////////////////////////////

class CFeatureVelocityCompass : public CParticleFeature
{
public:
	CRY_PFX2_DECLARE_FEATURE

	CFeatureVelocityCompass()
		: m_azimuth(0.0f)
		, m_angle(0.0f)
		, m_velocity(0.0f) {}

	virtual void AddToComponent(CParticleComponent* pComponent, SComponentParams* pParams) override
	{
		pComponent->InitParticles.add(this);
		m_azimuth.AddToComponent(pComponent, this);
		m_angle.AddToComponent(pComponent, this);
		m_velocity.AddToComponent(pComponent, this);
	}

	virtual void Serialize(Serialization::IArchive& ar) override
	{
		CParticleFeature::Serialize(ar);
		ar(m_azimuth, "Azimuth", "Azimuth");
		ar(m_angle, "Angle", "Angle");
		ar(m_velocity, "Velocity", "Velocity");
	}

	virtual void InitParticles(CParticleComponentRuntime& runtime) override
	{
		CRY_PFX2_PROFILE_DETAIL;

		const float halfPi = gf_PI * 0.5f;
		CParticleContainer& parentContainer = runtime.GetParentContainer();
		CParticleContainer& container = runtime.GetContainer();
		const Quat defaultQuat = runtime.GetEmitter()->GetLocation().q;
		IPidStream parentIds = container.GetIPidStream(EPDT_ParentId);
		IOVec3Stream velocities = container.GetIOVec3Stream(EPVF_Velocity);
		IQuatStream parentQuats = parentContainer.GetIQuatStream(EPQF_Orientation, defaultQuat);
		STempInitBuffer<float> azimuths(runtime, m_azimuth);
		STempInitBuffer<float> angles(runtime, m_angle);
		STempInitBuffer<float> velocityValues(runtime, m_velocity);

		for (auto particleId : runtime.SpawnedRange())
		{
			const TParticleId parentId = parentIds.Load(particleId);
			const Quat wQuat = parentQuats.SafeLoad(parentId);
			const float azimuth = azimuths.SafeLoad(particleId) + halfPi;
			const float altitude = angles.SafeLoad(particleId) + halfPi;
			const float velocity = velocityValues.SafeLoad(particleId);
			const Vec3 oVelocity = PolarCoordToVec3(azimuth, altitude) * velocity;
			const Vec3 wVelocity0 = velocities.Load(particleId);
			const Vec3 wVelocity1 = wVelocity0 + wQuat * oVelocity;
			velocities.Store(particleId, wVelocity1);
		}
	}

private:
	CParamMod<EDD_PerParticle, SAngle360> m_azimuth;
	CParamMod<EDD_PerParticle, UAngle180> m_angle;
	CParamMod<EDD_PerParticle, SFloat10>  m_velocity;
};

CRY_PFX2_IMPLEMENT_FEATURE(CParticleFeature, CFeatureVelocityCompass, "Velocity", "Compass", colorVelocity);

}
