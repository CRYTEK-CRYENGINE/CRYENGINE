// Copyright 2015-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "FeatureCommon.h"
#include "FeatureAngles.h"

namespace pfx2
{

//////////////////////////////////////////////////////////////////////////
// CFeatureProjectBase

class CFeatureProjectBase : public CParticleFeature
{
public:
	typedef TParticleHeap::Array<PosNorm> TPosNormArray;

public:
	CFeatureProjectBase()
		: m_alignAxis(EAlignParticleAxis::Normal)
		, m_alignView(EAlignView::None)
		, m_offset(0.0f)
		, m_projectPosition(true)
		, m_projectVelocity(true)
		, m_projectAngles(false)
		, m_spawnOnly(true) {}

	virtual void AddToComponent(CParticleComponent* pComponent, SComponentParams* pParams) override
	{
		if (m_spawnOnly)
			pComponent->InitParticles.add(this);
		else
			pComponent->UpdateParticles.add(this);
		if (m_projectPosition)
			pComponent->AddParticleData(EPVF_Position);
		if (m_projectVelocity)
			pComponent->AddParticleData(EPVF_Velocity);
		if (m_projectAngles)
			pComponent->AddParticleData(EPQF_Orientation);
	}

	virtual void Serialize(Serialization::IArchive& ar) override
	{
		CParticleFeature::Serialize(ar);
		ar(m_spawnOnly, "SpawnOnly", "Spawn Only");
		ar(m_projectPosition, "ProjectPosition", "Project Positions");
		if (m_spawnOnly && m_projectPosition)
			ar(m_offset, "Offset", "Offset");
		else
			m_offset = 0.0f;
		ar(m_projectVelocity, "ProjectVelocity", "Project Velocities");
		ar(m_projectAngles, "ProjectAngles", "Project Angles");
		if (m_projectAngles)
		{
			ar(m_alignAxis, "AlignAxis", "Align Axis");
			ar(m_alignView, "AlignView", "Align View");
		}
	}

	virtual void InitParticles(CParticleComponentRuntime& runtime) override
	{
		CRY_PFX2_PROFILE_DETAIL;
		Project(runtime, runtime.SpawnedRange());
	}

	virtual void UpdateParticles(CParticleComponentRuntime& runtime) override
	{
		CRY_PFX2_PROFILE_DETAIL;
		Project(runtime, runtime.FullRange());
	}

private:
	virtual void FillSamples(CParticleComponentRuntime& runtime, const SUpdateRange& range, TPosNormArray& samples) = 0;

	ILINE void Project(CParticleComponentRuntime& runtime, const SUpdateRange& range)
	{
		const Matrix34 viewTM = gEnv->p3DEngine->GetRenderingCamera().GetMatrix();
		const Vec3 cameraPos = gEnv->p3DEngine->GetRenderingCamera().GetPosition();
		const Vec3 forward = -viewTM.GetColumn1();

		CParticleContainer& container = runtime.GetContainer();

		TPosNormArray posNormArray(runtime.MemHeap(), container.GetMaxParticles());
		FillSamples(runtime, range, posNormArray);

		IOVec3Stream positions = container.GetIOVec3Stream(EPVF_Position);
		IOVec3Stream velocities = container.GetIOVec3Stream(EPVF_Velocity);
		IOQuatStream orientations = container.GetIOQuatStream(EPQF_Orientation);

		for (auto particleId : range)
		{
			const PosNorm posNormSample = posNormArray[particleId];

			if (m_projectPosition)
			{
				const Vec3 position1 = posNormSample.vPos + posNormSample.vNorm * m_offset;
				positions.Store(particleId, position1);
			}

			if (m_projectVelocity)
			{
				const Vec3 velocity0 = velocities.Load(particleId);
				const Vec3 normalVelocity = posNormSample.vNorm * posNormSample.vNorm.Dot(velocity0);
				const Vec3 tangentVelocity = velocity0 - normalVelocity;
				velocities.Store(particleId, tangentVelocity);
			}

			if (m_projectAngles)
			{
				const Quat orientation0 = orientations.Load(particleId);
				const Vec3 particleAxis = GetParticleAxis(m_alignAxis, orientation0);
				const Quat fieldAlign = Quat::CreateRotationV0V1(particleAxis, posNormSample.vNorm);
				const Quat orientation1 = fieldAlign * orientation0;

				if (m_alignView != EAlignView::None)
				{
					const Vec3 position = positions.Load(particleId);
					const Vec3 toCamera = m_alignView == EAlignView::Camera ? (cameraPos - position).GetNormalized() : forward;
					const Vec3 particleOtherAxis = GetParticleOtherAxis(m_alignAxis, orientation1);
					const Vec3 cameraAlignAxis = posNormSample.vNorm.Cross(toCamera).GetNormalizedSafe();
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
	}

private:
	EAlignParticleAxis m_alignAxis;
	EAlignView         m_alignView;
	UFloat             m_offset;
	bool               m_projectPosition;
	bool               m_projectVelocity;
	bool               m_projectAngles;
	bool               m_spawnOnly;
};

//////////////////////////////////////////////////////////////////////////
// CFeatureProjectTerrain

class CFeatureProjectTerrain : public CFeatureProjectBase
{
public:
	CRY_PFX2_DECLARE_FEATURE

private:
	virtual void FillSamples(CParticleComponentRuntime& runtime, const SUpdateRange& range, TPosNormArray& samples) override
	{
		CTerrain* pTerrain = runtime.GetEmitter()->GetTerrain();

		const CParticleContainer& container = runtime.GetContainer();
		const IVec3Stream positions = container.GetIVec3Stream(EPVF_Position);
		const IFStream sizes = container.GetIFStream(EPDT_Size);

		for (auto particleId : range)
		{
			const Vec3 position = positions.Load(particleId);
			const float size = sizes.Load(particleId);
			samples[particleId] = SampleTerrain(*pTerrain, position, size);
		}
	}

	ILINE PosNorm SampleTerrain(const CTerrain& terrain, const Vec3 position, const float size) const
	{
		PosNorm out;
		out.vPos.x = position.x;
		out.vPos.y = position.y;

		Vec3 samplers[4] =
		{
			Vec3(position.x + size, position.y       , 0.0f),
			Vec3(position.x - size, position.y       , 0.0f),
			Vec3(position.x       , position.y + size, 0.0f),
			Vec3(position.x       , position.y - size, 0.0f),
		};
		for (uint i = 0; i < 4; ++i)
			samplers[i].z = terrain.GetZApr(samplers[i].x, samplers[i].y);

		out.vPos.z = (samplers[0].z + samplers[1].z + samplers[2].z + samplers[3].z) * 0.25f;
		out.vNorm = (samplers[1] - samplers[0]).Cross(samplers[3] - samplers[2]).GetNormalizedSafe();

		return out;
	}
};

CRY_PFX2_IMPLEMENT_FEATURE(CParticleFeature, CFeatureProjectTerrain, "Project", "Terrain", colorProject);

//////////////////////////////////////////////////////////////////////////
// CFeatureProjectWater

class CFeatureProjectWater : public CFeatureProjectBase
{
public:
	CRY_PFX2_DECLARE_FEATURE

	virtual void AddToComponent(CParticleComponent* pComponent, SComponentParams* pParams) override
	{
		CFeatureProjectBase::AddToComponent(pComponent, pParams);
		pComponent->GetEffect()->AddEnvironFlags(ENV_WATER);
	}

private:
	virtual void FillSamples(CParticleComponentRuntime& runtime, const SUpdateRange& range, TPosNormArray& samples) override
	{
		const SPhysEnviron& physicsEnvironment = runtime.GetEmitter()->GetPhysicsEnv();
		CParticleContainer& container = runtime.GetContainer();
		const IVec3Stream positions = container.GetIVec3Stream(EPVF_Position);
		IOFStream ages = container.GetIOFStream(EPDT_NormalAge);

		for (auto particleId : range)
		{
			Plane waterPlane;
			const Vec3 position = positions.Load(particleId);
			physicsEnvironment.GetWaterPlane(waterPlane, position);
			if (waterPlane.d < -WATER_LEVEL_UNKNOWN)
			{
				const float distToWater = waterPlane.DistFromPlane(position);
				samples[particleId].vPos = position - waterPlane.n * distToWater;
				samples[particleId].vNorm = waterPlane.n;
			}
			else
			{
				samples[particleId].vPos = Vec3(ZERO);
				samples[particleId].vNorm = Vec3(ZERO);
				ages.Store(particleId, 1.0f);
			}
		}
	}
};

CRY_PFX2_IMPLEMENT_FEATURE(CParticleFeature, CFeatureProjectWater, "Project", "Water", colorProject);

}
