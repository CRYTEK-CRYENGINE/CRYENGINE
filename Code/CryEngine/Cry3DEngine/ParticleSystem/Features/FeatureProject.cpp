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
protected:
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
		SERIALIZE_VAR(ar, m_spawnOnly);
		SERIALIZE_VAR(ar, m_projectPosition);
		if (m_projectPosition)
			SERIALIZE_VAR(ar, m_offset);
		else
			m_offset = 0.0f;
		SERIALIZE_VAR(ar, m_projectVelocity);
		SERIALIZE_VAR(ar, m_projectAngles);
		if (m_projectAngles)
		{
			SERIALIZE_VAR(ar, m_alignAxis);
			SERIALIZE_VAR(ar, m_alignView);
		}
		SERIALIZE_VAR(ar, m_highAccuracy);
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

protected:
	virtual void Project(CParticleComponentRuntime& runtime, const SUpdateRange& range) const = 0;

	bool PositionOnly() const
	{
		return m_projectPosition && !m_projectVelocity && !m_projectAngles;
	}

	void ProjectToSamples(CParticleComponentRuntime& runtime, const SUpdateRange& range, TConstArray<Vec4> samples) const
	{
		CRY_PROFILE_FUNCTION(PROFILE_PARTICLE);

		const Matrix34 viewTM = gEnv->p3DEngine->GetRenderingCamera().GetMatrix();
		const Vec3 cameraPos = gEnv->p3DEngine->GetRenderingCamera().GetPosition();
		const Vec3 forward = -viewTM.GetColumn1();

		CParticleContainer& container = runtime.GetContainer();
		IVec3Stream positions = container.IStream(EPVF_Position);
		IOFStream posZ = container.IOStream(EPVF_Position[2]);
		IOVec3Stream velocities = container.IOStream(EPVF_Velocity);
		IOQuatStream orientations = container.IOStream(EPQF_Orientation);

		for (auto particleId : range)
		{
			if (m_projectPosition)
			{
				posZ[particleId] = samples[particleId].w + m_offset;
			}

			const Vec3& norm = samples[particleId];

			if (m_projectVelocity)
			{
				Vec3 velocity = velocities.Load(particleId);
				const Vec3 normalVelocity = norm * (norm | velocity);
				velocity -= normalVelocity;
				velocities.Store(particleId, velocity);
			}

			if (m_projectAngles)
			{
				const Quat orientation0 = orientations.Load(particleId);
				const Vec3 particleAxis = GetParticleAxis(m_alignAxis, orientation0);
				const Quat fieldAlign = Quat::CreateRotationV0V1(particleAxis, norm);
				const Quat orientation1 = fieldAlign * orientation0;

				if (m_alignView != EAlignView::None)
				{
					const Vec3 position = positions.Load(particleId);
					const Vec3 toCamera = m_alignView == EAlignView::Camera ? (cameraPos - position).GetNormalized() : forward;
					const Vec3 particleOtherAxis = GetParticleOtherAxis(m_alignAxis, orientation1);
					const Vec3 cameraAlignAxis = (norm ^ toCamera).GetNormalizedSafe();
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

protected:
	EAlignParticleAxis m_alignAxis       = EAlignParticleAxis::Normal;
	EAlignView         m_alignView       = EAlignView::None;
	UFloat             m_offset          = 0;
	bool               m_projectPosition = true;
	bool               m_projectVelocity = true;
	bool               m_projectAngles   = false;
	bool               m_highAccuracy    = false;
	bool               m_spawnOnly       = true;
};

//////////////////////////////////////////////////////////////////////////
// CFeatureProjectTerrain

class CFeatureProjectTerrain : public CFeatureProjectBase
{
public:
	CRY_PFX2_DECLARE_FEATURE

private:
	virtual void Project(CParticleComponentRuntime& runtime, const SUpdateRange& range) const override
	{
		CRY_PROFILE_FUNCTION(PROFILE_PARTICLE);

		const CTerrain& terrain = *Cry3DEngineBase::GetTerrain();

		CParticleContainer& container = runtime.GetContainer();
		IFStream posX = container.IStream(EPVF_Position[0]);
		IFStream posY = container.IStream(EPVF_Position[1]);
		IOFStream posZ = container.IOStream(EPVF_Position[2]);
		IFStream sizes = container.IStream(EPDT_Size);

		if (PositionOnly())
		{
			if (m_highAccuracy)
			{
				for (auto particleId : range)
				{
					const float size = sizes.Load(particleId);
					Vec4 normz = terrain.GetNormalAndZ(posX[particleId], posY[particleId], size);
					posZ[particleId] = normz.w + m_offset;
				}
			}
			else
			{
				for (auto particleId : range)
				{
					posZ[particleId] = terrain.GetZApr(posX[particleId], posY[particleId]) + m_offset;
				}
			}
		}
		else
		{
			THeapArray<Vec4> samples(runtime.MemHeap(), container.GetMaxParticles());
			for (auto particleId : range)
			{
				const float size = m_highAccuracy ? sizes.Load(particleId) : 0.0f;
				samples[particleId] = terrain.GetNormalAndZ(posX[particleId], posY[particleId], size);
			}
			ProjectToSamples(runtime, range, samples);
		}
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
		pComponent->AddEnvironFlags(ENV_WATER);
	}

private:
	virtual void Project(CParticleComponentRuntime& runtime, const SUpdateRange& range) const override
	{
		CRY_PROFILE_FUNCTION(PROFILE_PARTICLE);

		const SPhysEnviron& physicsEnvironment = runtime.GetEmitter()->GetPhysicsEnv();
		CParticleContainer& container = runtime.GetContainer();
		IVec3Stream positions = container.IStream(EPVF_Position);
		IOFStream posZ = container.IOStream(EPVF_Position[2]);
		IOFStream ages = container.IOStream(EPDT_NormalAge);
		const Plane oceanPlane(Vec3(0, 0, 1), -gEnv->p3DEngine->GetWaterLevel());

		const bool positionOnly = PositionOnly();
		THeapArray<Vec4> samples(runtime.MemHeap(), !positionOnly * container.GetMaxParticles());
		for (auto particleId : range)
		{
			Plane waterPlane;
			Vec3 position = positions.Load(particleId);
			physicsEnvironment.GetWaterPlane(waterPlane, position);
			if (waterPlane.d >= -WATER_LEVEL_UNKNOWN)
			{
				ages.Store(particleId, 1.0f);
				continue;
			}
			position.z = -waterPlane.d - waterPlane.n.x * position.x - waterPlane.n.y * position.y;
			if (m_highAccuracy && waterPlane == oceanPlane)
				position.z = gEnv->p3DEngine->GetAccurateOceanHeight(position);
			if (positionOnly)
				posZ[particleId] = position.z + m_offset;
			else
				samples[particleId] = Vec4(waterPlane.n, position.z);
		}
		if (!positionOnly)
			ProjectToSamples(runtime, range, samples);
	}
};

CRY_PFX2_IMPLEMENT_FEATURE(CParticleFeature, CFeatureProjectWater, "Project", "Water", colorProject);

}
