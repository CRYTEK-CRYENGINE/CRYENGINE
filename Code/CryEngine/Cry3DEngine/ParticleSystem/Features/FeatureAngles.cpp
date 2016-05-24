// Copyright 2001-2016 Crytek GmbH / Crytek Group. All rights reserved.

// -------------------------------------------------------------------------
//  Created:     30/10/2014 by Filipe amim
//  Description:
// -------------------------------------------------------------------------
//
////////////////////////////////////////////////////////////////////////////

#include "StdAfx.h"
#include <CrySerialization/Math.h>
#include "ParticleSystem/ParticleFeature.h"
#include "ParamTraits.h"

CRY_PFX2_DBG

volatile bool gFeatureAngles = false;

namespace pfx2
{

class CFeatureAnglesRotate2D : public CParticleFeature
{
public:
	CRY_PFX2_DECLARE_FEATURE

	CFeatureAnglesRotate2D()
		: m_initialAngle(0.0f)
		, m_randomAngle(0.0f)
		, m_initialSpin(0.0f)
		, m_randomSpin(0.0f) {}

	virtual void AddToComponent(CParticleComponent* pComponent, SComponentParams* pParams) override
	{
		pComponent->AddToUpdateList(EUL_InitUpdate, this);
		pComponent->AddParticleData(EPDT_Angle2D);

		if (!IsDefault(m_initialSpin, 0.0f) || !IsDefault(m_randomSpin, 0.0f))
		{
			pComponent->AddParticleData(EPDT_Spin2D);
			pComponent->AddToUpdateList(EUL_Update, this);
		}
	}

	virtual void Serialize(Serialization::IArchive& ar) override
	{
		CParticleFeature::Serialize(ar);
		ar(m_initialAngle, "InitialAngle", "Initial Angle");
		ar(m_randomAngle, "RandomAngle", "Random Angle");
		ar(m_initialSpin, "InitialSpin", "Initial Spin");
		ar(m_randomSpin, "RandomSpin", "Random Spin");

		if (ar.isInput() && GetVersion(ar) < 8)
		{
			m_initialAngle = -m_initialAngle;
			m_initialSpin = -m_initialSpin;
		}
	}

	virtual void InitParticles(const SUpdateContext& context) override
	{
		CRY_PFX2_PROFILE_DETAIL;

		CParticleContainer& container = context.m_container;

		IOFStream angles = container.GetIOFStream(EPDT_Angle2D);
		const float initAngle = -m_initialAngle.Get();
		const float randAngle = m_randomAngle.Get();
		CRY_PFX2_FOR_SPAWNED_PARTICLES(context)
		{
			const float snorm = SChaosKey().RandSNorm();
			const float angle = randAngle * snorm + initAngle;
			angles.Store(particleId, angle);
		}
		CRY_PFX2_FOR_END;

		if (container.HasData(EPDT_Spin2D))
		{
			IOFStream spins = container.GetIOFStream(EPDT_Spin2D);
			const float initSpin = -m_initialSpin.Get();
			const float randSpin = m_randomSpin.Get();
			CRY_PFX2_FOR_SPAWNED_PARTICLES(context)
			{
				const float snorm = SChaosKey().RandSNorm();
				const float spin = randSpin * snorm + initSpin;
				spins.Store(particleId, spin);
			}
			CRY_PFX2_FOR_END;
		}
	}

	virtual void Update(const SUpdateContext& context) override
	{
		CRY_PFX2_PROFILE_DETAIL;

		CParticleContainer& container = context.m_container;
		const floatv deltaTime = ToFloatv(context.m_deltaTime);
		IFStream spins = container.GetIFStream(EPDT_Spin2D);
		IOFStream angles = container.GetIOFStream(EPDT_Angle2D);

		CRY_PFX2_FOR_ACTIVE_PARTICLESGROUP(context)
		{
			const floatv spin0 = spins.Load(particleGroupId);
			const floatv angle0 = angles.Load(particleGroupId);
			const floatv angle1 = MAdd(spin0, deltaTime, angle0);
			angles.Store(particleGroupId, angle1);
		}
		CRY_PFX2_FOR_END;
	}

private:
	SAngle360 m_initialAngle;
	UAngle180 m_randomAngle;
	SAngle    m_initialSpin;
	UAngle    m_randomSpin;
};

CRY_PFX2_IMPLEMENT_FEATURE(CParticleFeature, CFeatureAnglesRotate2D, "Angles", "Rotate2D", defaultIcon, defaultColor);

class CFeatureAnglesRotate3D : public CParticleFeature
{
public:
	CRY_PFX2_DECLARE_FEATURE

	CFeatureAnglesRotate3D()
		: m_initAngles(ZERO)
		, m_randomAngles(ZERO)
		, m_initialSpin(ZERO)
		, m_randomSpin(ZERO) {}

	virtual void AddToComponent(CParticleComponent* pComponent, SComponentParams* pParams) override
	{
		pComponent->AddToUpdateList(EUL_InitUpdate, this);
		pComponent->AddParticleData(EPQF_Orientation);

		if (m_initialSpin != Vec3(ZERO) || m_randomSpin != Vec3(ZERO))
		{
			pComponent->AddParticleData(EPVF_AngularVelocity);
			pComponent->AddToUpdateList(EUL_Update, this);
		}
	}

	virtual void Serialize(Serialization::IArchive& ar) override
	{
		CParticleFeature::Serialize(ar);
		ar(m_initAngles, "InitialAngle", "Initial Angle");
		ar(m_randomAngles, "RandomAngle", "Random Angle");
		ar(m_initialSpin, "InitialSpin", "Initial Spin");
		ar(m_randomSpin, "RandomSpin", "Random Spin");
	}

	virtual void InitParticles(const SUpdateContext& context) override
	{
		CRY_PFX2_PROFILE_DETAIL;

		CParticleContainer& container = context.m_container;
		IOQuatStream orientations = container.GetIOQuatStream(EPQF_Orientation);
		const Ang3 initAngles = FromVec3(m_initAngles);
		const Ang3 randomInitAngles = FromVec3(m_randomAngles);

		CRY_PFX2_FOR_SPAWNED_PARTICLES(context)
		{
			const Quat wOrientation0 = orientations.Load(particleId);
			const Ang3 randAngle = RandomAng3(randomInitAngles);
			const Ang3 angle = initAngles + randAngle;
			const Quat oOrientation = Quat::CreateRotationXYZ(angle);
			const Quat wOrientation1 = wOrientation0 * oOrientation;
			orientations.Store(particleId, wOrientation1);
		}
		CRY_PFX2_FOR_END;

		if (container.HasData(EPDT_AngularVelocityX))
		{
			IOVec3Stream angularVelocities = container.GetIOVec3Stream(EPVF_AngularVelocity);
			const Ang3 initSpin = FromVec3(m_initialSpin);
			const Ang3 randSpin = FromVec3(m_randomSpin);
			CRY_PFX2_FOR_SPAWNED_PARTICLES(context)
			{
				const Ang3 angularVelocity = RandomAng3(randSpin) + initSpin;
				angularVelocities.Store(particleId, Vec3(angularVelocity));
			}
			CRY_PFX2_FOR_END;
		}
	}

	virtual void Update(const SUpdateContext& context) override
	{
		CRY_PFX2_PROFILE_DETAIL;

		CParticleContainer& container = context.m_container;
		const float deltaTime = context.m_deltaTime;
		const IVec3Stream angularVelocities = container.GetIVec3Stream(EPVF_AngularVelocity);
		IOQuatStream orientations = container.GetIOQuatStream(EPQF_Orientation);

		CRY_PFX2_FOR_ACTIVE_PARTICLES(context)
		{
			const Vec3 angularVelocity = angularVelocities.Load(particleId);
			const Quat orientation0 = orientations.Load(particleId);
			const Quat orientation1 = Quat::exp(angularVelocity * (deltaTime * 0.5f)) * orientation0;
			orientations.Store(particleId, orientation1);
		}
		CRY_PFX2_FOR_END;
	}

private:
	ILINE Ang3 FromVec3(Vec3 v)
	{
		return Ang3(DEG2RAD(-v.x), DEG2RAD(-v.y), DEG2RAD(v.z));
	}

	ILINE Ang3 RandomAng3(Ang3 mult)
	{
		SChaosKey randKey;
		return Ang3(
		  randKey.RandSNorm() * mult.x,
		  randKey.RandSNorm() * mult.y,
		  randKey.RandSNorm() * mult.z);
	}

	Vec3 m_initAngles;
	Vec3 m_randomAngles;
	Vec3 m_initialSpin;
	Vec3 m_randomSpin;
};

CRY_PFX2_IMPLEMENT_FEATURE(CParticleFeature, CFeatureAnglesRotate3D, "Angles", "Rotate3D", defaultIcon, defaultColor);

}
