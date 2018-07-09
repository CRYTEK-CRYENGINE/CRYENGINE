// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "FeatureAngles.h"
#include "FeatureCommon.h"

namespace pfx2
{

MakeDataType(EPDT_Angle2D, float);
MakeDataType(EPDT_Spin2D, float);

//////////////////////////////////////////////////////////////////////////
// CFeatureAnglesRotate2D

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
		pComponent->InitParticles.add(this);
		pComponent->AddParticleData(EPDT_Angle2D);

		if (m_initialSpin != 0.0f || m_randomSpin != 0.0f)
			pComponent->AddParticleData(EPDT_Spin2D);
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

	virtual void InitParticles(CParticleComponentRuntime& runtime) override
	{
		CRY_PFX2_PROFILE_DETAIL;

		CParticleContainer& container = runtime.GetContainer();

		IOFStream angles = container.GetIOFStream(EPDT_Angle2D);
		const floatv initAngle = ToFloatv(m_initialAngle.Get());
		const floatv randAngle = ToFloatv(m_randomAngle.Get());
		for (auto particleGroupId : runtime.SpawnedRangeV())
		{
			const floatv snorm = runtime.ChaosV().RandSNorm();
			const floatv angle = MAdd(randAngle, snorm, initAngle);
			angles.Store(particleGroupId, angle);
		}

		if (container.HasData(EPDT_Spin2D))
		{
			IOFStream spins = container.GetIOFStream(EPDT_Spin2D);
			const floatv initSpin = ToFloatv(m_initialSpin.Get());
			const floatv randSpin = ToFloatv(m_randomSpin.Get());
			for (auto particleGroupId : runtime.SpawnedRangeV())
			{
				const floatv snorm = runtime.ChaosV().RandSNorm();
				const floatv spin = MAdd(randSpin, snorm, initSpin);
				spins.Store(particleGroupId, spin);
			}
		}
	}

private:
	SAngle360 m_initialAngle;
	UAngle180 m_randomAngle;
	SAngle    m_initialSpin;
	UAngle    m_randomSpin;
};

CRY_PFX2_IMPLEMENT_FEATURE(CParticleFeature, CFeatureAnglesRotate2D, "Angles", "Rotate2D", colorAngles);

//////////////////////////////////////////////////////////////////////////
// CFeatureAnglesRotate2D

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
		pComponent->InitParticles.add(this);
		pComponent->AddParticleData(EPQF_Orientation);

		if (m_initialSpin != Vec3(ZERO) || m_randomSpin != Vec3(ZERO))
			pComponent->AddParticleData(EPVF_AngularVelocity);
	}

	virtual void Serialize(Serialization::IArchive& ar) override
	{
		CParticleFeature::Serialize(ar);
		ar(m_initAngles, "InitialAngle", "Initial Angle");
		ar(m_randomAngles, "RandomAngle", "Random Angle");
		ar(m_initialSpin, "InitialSpin", "Initial Spin");
		ar(m_randomSpin, "RandomSpin", "Random Spin");
	}

	virtual void InitParticles(CParticleComponentRuntime& runtime) override
	{
		CRY_PFX2_PROFILE_DETAIL;

		CParticleContainer& container = runtime.GetContainer();
		IOQuatStream orientations = container.GetIOQuatStream(EPQF_Orientation);
		const Ang3 initAngles = FromVec3(m_initAngles);
		const Ang3 randomInitAngles = FromVec3(m_randomAngles);

		for (auto particleId : runtime.SpawnedRange())
		{
			const Quat wOrientation0 = orientations.Load(particleId);
			const Ang3 randAngle = RandomAng3(runtime.Chaos(), randomInitAngles);
			const Ang3 angle = initAngles + randAngle;
			const Quat oOrientation = Quat::CreateRotationXYZ(angle);
			const Quat wOrientation1 = wOrientation0 * oOrientation;
			orientations.Store(particleId, wOrientation1);
		}

		if (container.HasData(EPVF_AngularVelocity))
		{
			IOVec3Stream angularVelocities = container.GetIOVec3Stream(EPVF_AngularVelocity);
			const Ang3 initSpin = FromVec3(m_initialSpin);
			const Ang3 randSpin = FromVec3(m_randomSpin);
			for (auto particleId : runtime.SpawnedRange())
			{
				const Quat wOrientation0 = orientations.Load(particleId);
				const Ang3 angularVelocity = RandomAng3(runtime.Chaos(), randSpin) + initSpin;
				const Vec3 wAngularVelocity = wOrientation0 * Vec3(angularVelocity);
				angularVelocities.Store(particleId, Vec3(wAngularVelocity));
			}
		}
	}

private:
	ILINE Ang3 FromVec3(Vec3 v)
	{
		return Ang3(DEG2RAD(-v.x), DEG2RAD(-v.y), DEG2RAD(v.z));
	}

	ILINE Ang3 RandomAng3(SChaosKey& chaosKey, Ang3 mult)
	{
		return Ang3(
		  chaosKey.RandSNorm() * mult.x,
		  chaosKey.RandSNorm() * mult.y,
		  chaosKey.RandSNorm() * mult.z);
	}

	Vec3 m_initAngles;
	Vec3 m_randomAngles;
	Vec3 m_initialSpin;
	Vec3 m_randomSpin;
};

CRY_PFX2_IMPLEMENT_FEATURE(CParticleFeature, CFeatureAnglesRotate3D, "Angles", "Rotate3D", colorAngles);

//////////////////////////////////////////////////////////////////////////
// CFeatureAnglesProject

class CFeatureAnglesAlign : public CParticleFeature
{
private:
	struct CTypeScreen
	{
		CTypeScreen(CParticleComponentRuntime& runtime, const CFeatureAnglesAlign& feature)
			: m_forward(-gEnv->p3DEngine->GetRenderingCamera().GetViewdir()) {}
		ILINE Vec3 Sample(TParticleId particleId) const
		{
			return m_forward;
		}
		ILINE bool CanAlign(TParticleId particleId) const { return true; }
		const Vec3 m_forward;
	};

	struct CTypeCamera
	{
		CTypeCamera(CParticleComponentRuntime& runtime, const CFeatureAnglesAlign& feature)
			: m_positions(runtime.GetContainer().GetIVec3Stream(EPVF_Position))
			, m_cameraPos(gEnv->p3DEngine->GetRenderingCamera().GetPosition()) {}
		ILINE Vec3 Sample(TParticleId particleId) const
		{
			const Vec3 position = m_positions.Load(particleId);
			const Vec3 alignAxis = (m_cameraPos - position).GetNormalized();
			return alignAxis;
		}
		ILINE bool CanAlign(TParticleId particleId) const { return true; }
		const IVec3Stream m_positions;
		const Vec3 m_cameraPos;
	};

	struct CTypeVelocity
	{
		CTypeVelocity(CParticleComponentRuntime& runtime, const CFeatureAnglesAlign& feature)
			: m_velocity(runtime.GetContainer().GetIVec3Stream(EPVF_Velocity)) {}
		ILINE Vec3 Sample(TParticleId particleId) const
		{
			return m_velocity.Load(particleId).GetNormalized();
		}
		ILINE bool CanAlign(TParticleId particleId) const
		{
			return m_velocity.Load(particleId).GetLengthSquared() > FLT_EPSILON;
		}
		const IVec3Stream m_velocity;
	};

	struct CTypeParent
	{
		CTypeParent(CParticleComponentRuntime& runtime, const CFeatureAnglesAlign& feature)
			: m_parentIds(runtime.GetContainer().GetIPidStream(EPDT_ParentId))
			, m_parentOrientations(runtime.GetParentContainer().GetIQuatStream(EPQF_Orientation))
			, m_axis(feature.m_axis.GetNormalizedSafe()) {}
		ILINE Vec3 Sample(TParticleId particleId) const
		{
			const TParticleId parentId = m_parentIds.Load(particleId);
			const Quat parentOrientation = m_parentOrientations.Load(parentId);
			return parentOrientation * m_axis;
		}
		ILINE bool CanAlign(TParticleId particleId) const
		{
			const TParticleId parentId = m_parentIds.Load(particleId);
			return parentId != gInvalidId;
		}
		const IPidStream m_parentIds;
		const IQuatStream m_parentOrientations;
		const Vec3 m_axis;
	};

	struct CTypeWorld
	{
		CTypeWorld(CParticleComponentRuntime& runtime, const CFeatureAnglesAlign& feature)
			: m_axis(feature.m_axis.GetNormalizedSafe()) {}
		ILINE Vec3 Sample(TParticleId particleId) const
		{
			return m_axis;
		}
		ILINE bool CanAlign(TParticleId particleId) const { return true; }
		const Vec3 m_axis;
	};

public:
	CRY_PFX2_DECLARE_FEATURE

	CFeatureAnglesAlign()
		: m_alignParticleAxis(EAlignParticleAxis::Normal)
		, m_alignType(EAlignType::Screen)
		, m_alignView(EAlignView::None)
		, m_axis(0.0f, 0.0f, 1.0f) {}
	
	virtual void AddToComponent(CParticleComponent* pComponent, SComponentParams* pParams) override
	{
		pComponent->UpdateParticles.add(this);
		pComponent->AddParticleData(EPQF_Orientation);
	}

	virtual void Serialize(Serialization::IArchive& ar) override
	{
		CParticleFeature::Serialize(ar);
		ar(m_alignParticleAxis, "ParticleAxis", "Particle Axis");
		ar(m_alignType, "Type", "Align Type");
		if (m_alignType == EAlignType::Parent || m_alignType == EAlignType::World)
			ar(m_axis, "Axis", "Axis");
		if (m_alignType == EAlignType::Parent || m_alignType == EAlignType::Velocity)
			ar(m_alignView, "AlignView", "Align View");
		else
			m_alignView = EAlignView::None;
	}

	virtual void UpdateParticles(CParticleComponentRuntime& runtime) override
	{
		CRY_PFX2_PROFILE_DETAIL;

		switch (m_alignType)
		{
		case EAlignType::Screen:
			Align<CTypeScreen>(runtime);
			break;
		case EAlignType::Camera:
			Align<CTypeCamera>(runtime);
			break;
		case EAlignType::Velocity:
			Align<CTypeVelocity>(runtime);
			break;
		case EAlignType::Parent:
			Align<CTypeParent>(runtime);
			break;
		case EAlignType::World:
			Align<CTypeWorld>(runtime);
			break;
		}
	}

private:

	template<typename TTypeSampler>
	void Align(CParticleComponentRuntime& runtime)
	{
		TTypeSampler typeSampler(runtime, *this);
		const Matrix34 viewTM = gEnv->p3DEngine->GetRenderingCamera().GetMatrix();
		const Vec3 cameraPos = gEnv->p3DEngine->GetRenderingCamera().GetPosition();
		const Vec3 forward = -viewTM.GetColumn1();

		CParticleContainer& container = runtime.GetContainer();
		const IVec3Stream positions = container.GetIVec3Stream(EPVF_Position);
		IOQuatStream orientations = container.GetIOQuatStream(EPQF_Orientation);

		for (auto particleId : runtime.FullRange())
		{
			if (!typeSampler.CanAlign(particleId))
				continue;

			const Quat orientation0 = orientations.Load(particleId);
			const Vec3 particleAxis = GetParticleAxis(m_alignParticleAxis, orientation0);
			const Vec3 alignVec = typeSampler.Sample(particleId);
			const Quat fieldAlign = Quat::CreateRotationV0V1(particleAxis, alignVec);
			const Quat orientation1 = fieldAlign * orientation0;

			if (m_alignView != EAlignView::None)
			{
				const Vec3 position = positions.Load(particleId);
				const Vec3 toCamera = m_alignView == EAlignView::Camera ? (cameraPos - position).GetNormalized() : forward;
				const Vec3 particleOtherAxis = GetParticleOtherAxis(m_alignParticleAxis, orientation1);
				const Vec3 cameraAlignAxis = alignVec.Cross(toCamera).GetNormalizedSafe();
				const Quat cameraAlign = Quat::CreateRotationV0V1(particleOtherAxis, cameraAlignAxis);
				const Quat orientation2 = cameraAlign * orientation1;
				orientations.Store(particleId, orientation2);
			}
			else
			{
				orientations.Store(particleId, orientation1);
			}
		}
	}

private:
	EAlignParticleAxis m_alignParticleAxis;
	EAlignType         m_alignType;
	EAlignView         m_alignView;
	Vec3               m_axis;
};

CRY_PFX2_IMPLEMENT_FEATURE(CParticleFeature, CFeatureAnglesAlign, "Angles", "Align", colorAngles);

}
