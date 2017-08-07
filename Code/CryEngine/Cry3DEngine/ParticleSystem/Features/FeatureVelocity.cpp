// Copyright 2001-2016 Crytek GmbH / Crytek Group. All rights reserved.

// -------------------------------------------------------------------------
//  Created:     29/10/2014 by Filipe amim
//  Description:
// -------------------------------------------------------------------------
//
////////////////////////////////////////////////////////////////////////////

#include "StdAfx.h"
#include <CrySerialization/Math.h>
#include "ParticleSystem/ParticleFeature.h"
#include "ParticleSystem/ParticleEmitter.h"
#include "ParamMod.h"

CRY_PFX2_DBG

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
		, CParticleFeature(gpu_pfx2::eGpuFeatureType_VelocityCone)
	{}

	virtual void AddToComponent(CParticleComponent* pComponent, SComponentParams* pParams) override
	{
		pComponent->AddToUpdateList(EUL_InitUpdate, this);
		m_angle.AddToComponent(pComponent, this);
		m_velocity.AddToComponent(pComponent, this);

		if (auto pInt = GetGpuInterface())
		{
			gpu_pfx2::SFeatureParametersVelocityCone params;
			params.angle = m_angle.GetBaseValue();
			params.velocity = m_velocity.GetBaseValue();
			pInt->SetParameters(params);
		}
	}

	virtual void Serialize(Serialization::IArchive& ar) override
	{
		CParticleFeature::Serialize(ar);
		ar(m_angle, "Angle", "Angle");
		ar(m_velocity, "Velocity", "Velocity");
		ar(m_axis, "Axis", "Axis");
	}

	virtual void InitParticles(const SUpdateContext& context) override
	{
		CRY_PFX2_PROFILE_DETAIL;

		CParticleContainer& parentContainer = context.m_parentContainer;
		CParticleContainer& container = context.m_container;
		const Quat defaultQuat = context.m_runtime.GetEmitter()->GetLocation().q;
		const IPidStream parentIds = container.GetIPidStream(EPDT_ParentId);
		const IQuatStream parentQuats = parentContainer.GetIQuatStream(EPQF_Orientation, defaultQuat);
		const Quat axisQuat = Quat::CreateRotationV0V1(Vec3(0.0f, 0.0f, 1.0f), m_axis.GetNormalizedSafe());
		IOVec3Stream velocities = container.GetIOVec3Stream(EPVF_Velocity);
		const float baseAngle = m_angle.GetBaseValue();
		const float invBaseAngle = rcp_fast(max(FLT_EPSILON, baseAngle));

		STempInitBuffer<float> angles(context, m_angle);
		STempInitBuffer<float> velocityMults(context, m_velocity);

		for (auto particleId : context.GetSpawnedRange())
		{
			const TParticleId parentId = parentIds.Load(particleId);
			const Vec3 wVelocity0 = velocities.Load(particleId);
			const Quat wQuat = parentQuats.SafeLoad(parentId);
			const float angleMult = angles.m_stream.SafeLoad(particleId);
			const float velocity = velocityMults.m_stream.SafeLoad(particleId);

			const Vec2 disc = context.m_spawnRng.RandCircle();
			const float angle = sqrtf(angleMult * invBaseAngle) * baseAngle;

			float as, ac;
			sincos(angle, &as, &ac);
			const Vec3 dir = axisQuat * Vec3(disc.x * as, disc.y * as, ac);
			const Vec3 oVelocity = dir * velocity;
			const Vec3 wVelocity1 = wVelocity0 + wQuat * oVelocity;
			velocities.Store(particleId, wVelocity1);
		}
	}

private:
	CParamMod<SModParticleSpawnInit, UAngle180> m_angle;
	CParamMod<SModParticleSpawnInit, UFloat10>  m_velocity;
	Vec3 m_axis;
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
		, CParticleFeature(gpu_pfx2::eGpuFeatureType_VelocityDirectional)
	{}

	virtual void AddToComponent(CParticleComponent* pComponent, SComponentParams* pParams) override
	{
		pComponent->AddToUpdateList(EUL_InitUpdate, this);
		m_scale.AddToComponent(pComponent, this);

		if (auto pGpu = GetGpuInterface())
		{
			gpu_pfx2::SFeatureParametersVelocityDirectional params;
			params.direction = m_direction;
			params.scale = m_scale.GetBaseValue();
			pGpu->SetParameters(params);
		}
	}

	virtual void Serialize(Serialization::IArchive& ar) override
	{
		CParticleFeature::Serialize(ar);
		ar(m_direction, "Velocity", "Velocity");
		ar(m_scale, "Scale", "Scale");
	}

	virtual void InitParticles(const SUpdateContext& context) override
	{
		CRY_PFX2_PROFILE_DETAIL;

		CParticleContainer& parentContainer = context.m_parentContainer;
		CParticleContainer& container = context.m_container;
		const Quat defaultQuat = context.m_runtime.GetEmitter()->GetLocation().q;
		IPidStream parentIds = container.GetIPidStream(EPDT_ParentId);
		IOVec3Stream velocities = container.GetIOVec3Stream(EPVF_Velocity);
		IQuatStream parentQuats = parentContainer.GetIQuatStream(EPQF_Orientation, defaultQuat);
		STempInitBuffer<float> scales(context, m_scale);

		for (auto particleId : context.GetSpawnedRange())
		{
			const TParticleId parentId = parentIds.Load(particleId);
			const Quat wQuat = parentQuats.SafeLoad(parentId);
			const float scale = scales.m_stream.SafeLoad(particleId);
			const Vec3 wVelocity0 = velocities.Load(particleId);
			const Vec3 oVelocity = m_direction * scale;
			const Vec3 wVelocity1 = wVelocity0 + wQuat * oVelocity;
			velocities.Store(particleId, wVelocity1);
		}
	}

private:
	Vec3 m_direction;
	CParamMod<SModParticleSpawnInit, UFloat10> m_scale;
};

CRY_PFX2_IMPLEMENT_FEATURE(CParticleFeature, CFeatureVelocityDirectional, "Velocity", "Directional", colorVelocity);

//////////////////////////////////////////////////////////////////////////

class CFeatureVelocityOmniDirectional : public CParticleFeature
{
public:
	CRY_PFX2_DECLARE_FEATURE

	CFeatureVelocityOmniDirectional()
		: m_velocity(0.0f)
		, CParticleFeature(gpu_pfx2::eGpuFeatureType_VelocityOmniDirectional)
	{}

	virtual void AddToComponent(CParticleComponent* pComponent, SComponentParams* pParams) override
	{
		pComponent->AddToUpdateList(EUL_InitUpdate, this);
		m_velocity.AddToComponent(pComponent, this);

		if (auto pGpu = GetGpuInterface())
		{
			gpu_pfx2::SFeatureParametersVelocityOmniDirectional params;
			params.velocity = m_velocity.GetBaseValue();
			pGpu->SetParameters(params);
		}
	}

	virtual void Serialize(Serialization::IArchive& ar) override
	{
		CParticleFeature::Serialize(ar);
		ar(m_velocity, "Velocity", "Velocity");
	}

	virtual void InitParticles(const SUpdateContext& context) override
	{
		CRY_PFX2_PROFILE_DETAIL;

		CParticleContainer& container = context.m_container;
		IOVec3Stream velocities = container.GetIOVec3Stream(EPVF_Velocity);

		STempInitBuffer<float> velocityMults(context, m_velocity);

		for (auto particleId : context.GetSpawnedRange())
		{
			const Vec3 wVelocity0 = velocities.Load(particleId);
			const float velocity = velocityMults.m_stream.SafeLoad(particleId);
			const Vec3 oVelocity = context.m_spawnRng.RandSphere() * velocity;
			const Vec3 wVelocity1 = wVelocity0 + oVelocity;
			velocities.Store(particleId, wVelocity1);
		}
	}

private:
	CParamMod<SModParticleSpawnInit, UFloat10> m_velocity;
};

CRY_PFX2_IMPLEMENT_FEATURE(CParticleFeature, CFeatureVelocityOmniDirectional, "Velocity", "OmniDirectional", colorVelocity);

//////////////////////////////////////////////////////////////////////////

class CFeatureVelocityInherit : public CParticleFeature
{
public:
	CRY_PFX2_DECLARE_FEATURE

	CFeatureVelocityInherit()
		: m_scale(1.0f)
		, CParticleFeature(gpu_pfx2::eGpuFeatureType_VelocityInherit) {}

	virtual void AddToComponent(CParticleComponent* pComponent, SComponentParams* pParams) override
	{
		pComponent->AddToUpdateList(EUL_InitUpdate, this);
		m_scale.AddToComponent(pComponent, this);

		if (auto gpuInt = GetGpuInterface())
		{
			gpu_pfx2::SFeatureParametersScale params;
			params.scale = m_scale.GetBaseValue();
			gpuInt->SetParameters(params);
		}
	}

	virtual void Serialize(Serialization::IArchive& ar) override
	{
		CParticleFeature::Serialize(ar);
		ar(m_scale, "Scale", "Scale");
	}

	virtual void InitParticles(const SUpdateContext& context) override
	{
		CRY_PFX2_PROFILE_DETAIL;

		if (context.m_parentContainer.HasData(EPVF_AngularVelocity))
			InheritVelocity<true>(context);
		else
			InheritVelocity<false>(context);
	}

	template<bool hasRotation>
	void InheritVelocity(const SUpdateContext& context)
	{
		CRY_PFX2_PROFILE_DETAIL;

		CParticleContainer& parentContainer = context.m_parentContainer;
		CParticleContainer& container = context.m_container;
		IPidStream parentIds = container.GetIPidStream(EPDT_ParentId);
		const IVec3Stream positions = container.GetIVec3Stream(EPVF_Position);
		const IVec3Stream parentPositions = parentContainer.GetIVec3Stream(EPVF_Position);
		const IVec3Stream parentVelocities = parentContainer.GetIVec3Stream(EPVF_Velocity);
		const IVec3Stream parentAngularVelocities = parentContainer.GetIVec3Stream(EPVF_AngularVelocity);
		IOVec3Stream velocities = container.GetIOVec3Stream(EPVF_Velocity);

		STempInitBuffer<float> scales(context, m_scale);

		for (auto particleId : context.GetSpawnedRange())
		{
			const float scale = scales.m_stream.SafeLoad(particleId);
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

private:
	CParamMod<SModParticleSpawnInit, SFloat> m_scale;
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
		pComponent->AddToUpdateList(EUL_InitUpdate, this);
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

	virtual void InitParticles(const SUpdateContext& context) override
	{
		CRY_PFX2_PROFILE_DETAIL;

		const float halfPi = gf_PI * 0.5f;
		CParticleContainer& parentContainer = context.m_parentContainer;
		CParticleContainer& container = context.m_container;
		const Quat defaultQuat = context.m_runtime.GetEmitter()->GetLocation().q;
		IPidStream parentIds = container.GetIPidStream(EPDT_ParentId);
		IOVec3Stream velocities = container.GetIOVec3Stream(EPVF_Velocity);
		IQuatStream parentQuats = parentContainer.GetIQuatStream(EPQF_Orientation, defaultQuat);
		STempInitBuffer<float> azimuths(context, m_azimuth);
		STempInitBuffer<float> angles(context, m_angle);
		STempInitBuffer<float> velocityValues(context, m_velocity);

		for (auto particleId : context.GetSpawnedRange())
		{
			const TParticleId parentId = parentIds.Load(particleId);
			const Quat wQuat = parentQuats.SafeLoad(parentId);
			const float azimuth = azimuths.m_stream.SafeLoad(particleId) + halfPi;
			const float altitude = angles.m_stream.SafeLoad(particleId) + halfPi;
			const float velocity = velocityValues.m_stream.SafeLoad(particleId);
			const Vec3 oVelocity = PolarCoordToVec3(azimuth, altitude) * velocity;
			const Vec3 wVelocity0 = velocities.Load(particleId);
			const Vec3 wVelocity1 = wVelocity0 + wQuat * oVelocity;
			velocities.Store(particleId, wVelocity1);
		}
	}

private:
	CParamMod<SModParticleSpawnInit, SAngle360> m_azimuth;
	CParamMod<SModParticleSpawnInit, UAngle180> m_angle;
	CParamMod<SModParticleSpawnInit, SFloat10> m_velocity;
};

CRY_PFX2_IMPLEMENT_FEATURE(CParticleFeature, CFeatureVelocityCompass, "Velocity", "Compass", colorVelocity);

}
