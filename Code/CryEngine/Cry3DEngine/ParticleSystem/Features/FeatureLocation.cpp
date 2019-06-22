// Copyright 2015-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "FeatureCommon.h"
#include <CryMath/SNoise.h>
#include "Target.h"

namespace pfx2
{

MakeDataType(ESDT_SpatialExtents, Vec4, EDD_Spawner);
MakeDataType(ESDT_EmitOffset, Vec3, EDD_Spawner);

//////////////////////////////////////////////////////////////////////////
// CFeatureLocationOffset

class CFeatureLocationOffset : public CParticleFeature
{
public:
	CRY_PFX2_DECLARE_FEATURE

	virtual void AddToComponent(CParticleComponent* pComponent, SComponentParams* pParams) override
	{
		pComponent->GetDynamicData.add(this);
		pComponent->InitParticles.add(this);
		pComponent->UpdateGPUParams.add(this);
		m_scale.AddToComponent(pComponent, this);
	}

	virtual void Serialize(Serialization::IArchive& ar) override
	{
		CParticleFeature::Serialize(ar);
		ar(m_offset, "Offset", "Offset");
		ar(m_scale, "Scale", "Scale");
	}

	virtual void GetDynamicData(const CParticleComponentRuntime& runtime, EParticleDataType type, void* data, EDataDomain domain, SUpdateRange range) override
	{
		if (auto extents = ESDT_SpatialExtents.Cast(type, data, range))
		{
			if (!m_scale.HasModifiers())
				return;
			SSpawnerUpdateBuffer<float> sizes(runtime, m_scale, domain);
			for (auto i : range)
			{
				float e = abs(sizes[i].Length());
				extents[i] = Vec4(1, e, 0, 0);
			}
		}
		else if (auto offsets = ESDT_EmitOffset.Cast(type, data, range))
		{
			SSpawnerUpdateBuffer<float> sizes(runtime, m_scale, domain);
			for (auto i : range)
			{
				const float scale = (sizes[i].start + sizes[i].end) * 0.5f;
				offsets[i] += m_offset * scale;
			}
		}
	}

	virtual void InitParticles(CParticleComponentRuntime& runtime) override
	{
		CRY_PROFILE_FUNCTION(PROFILE_PARTICLE);

		CParticleContainer& parentContainer = runtime.GetParentContainer();
		CParticleContainer& container = runtime.GetContainer();
		const Quat defaultQuat = runtime.GetEmitter()->GetLocation().q;
		IPidStream parentIds = container.GetIPidStream(EPDT_ParentId);
		IQuatStream parentQuats = parentContainer.GetIQuatStream(EPQF_Orientation, defaultQuat);
		IOVec3Stream positions = container.GetIOVec3Stream(EPVF_Position);
		STempInitBuffer<float> scales(runtime, m_scale);

		Vec3 oOffset = m_offset;
		for (auto particleId : runtime.SpawnedRange())
		{
			const TParticleId parentId = parentIds.Load(particleId);
			const float scale = scales.SafeLoad(particleId);
			const Vec3 wPosition0 = positions.Load(particleId);
			const Quat wQuat = parentQuats.SafeLoad(parentId);
			const Vec3 wOffset = wQuat * oOffset;
			const Vec3 wPosition1 = wPosition0 + wOffset * scale;
			positions.Store(particleId, wPosition1);
		}
	}

	virtual void UpdateGPUParams(CParticleComponentRuntime& runtime, gpu_pfx2::SUpdateParams& params) override
	{
		params.offset = m_offset;
		params.scale.x = m_scale.GetValueRange(runtime)(0.5f);
		params.initFlags |= gpu_pfx2::eFeatureInitializationFlags_LocationOffset;
	}

private:
	Vec3                              m_offset {0};
	CParamMod<EDD_Particle, UFloat10> m_scale = 1;
};

CRY_PFX2_IMPLEMENT_FEATURE(CParticleFeature, CFeatureLocationOffset, "Location", "Offset", colorLocation);

//////////////////////////////////////////////////////////////////////////
// CFeatureLocationBox

class CFeatureLocationBox : public CParticleFeature
{
public:
	CRY_PFX2_DECLARE_FEATURE

	virtual void AddToComponent(CParticleComponent* pComponent, SComponentParams* pParams) override
	{
		pComponent->InitParticles.add(this);
		pComponent->GetDynamicData.add(this);
		pComponent->UpdateGPUParams.add(this);
		m_scale.AddToComponent(pComponent, this);
	}

	virtual void Serialize(Serialization::IArchive& ar) override
	{
		CParticleFeature::Serialize(ar);
		ar(m_box, "Dimension", "Dimension");
		ar(m_scale, "Scale", "Scale");
		m_distribution.Serialize(ar);
	}

	virtual void GetDynamicData(const CParticleComponentRuntime& runtime, EParticleDataType type, void* data, EDataDomain domain, SUpdateRange range) override
	{
		if (auto extents = ESDT_SpatialExtents.Cast(type, data, range))
		{
			SSpawnerUpdateBuffer<float> sizes(runtime, m_scale, domain);
			for (auto i : range)
			{
				float sizeAvg = (sizes[i].start + sizes[i].end) * 0.5f * 2.0f;
				Vec3 box = m_box * sizeAvg;
				extents[i] += Vec4(1, 
					box.x + box.y + box.z, 
					box.x * box.y + box.y * box.z + box.z * box.x,
					box.x * box.y * box.z
				);
			}
		}
	}

	virtual void InitParticles(CParticleComponentRuntime& runtime) override
	{
		CRY_PFX2_PROFILE_DETAIL;

		CParticleContainer& parentContainer = runtime.GetParentContainer();
		CParticleContainer& container = runtime.GetContainer();
		const Quat defaultQuat = runtime.GetEmitter()->GetLocation().q;
		IPidStream parentIds = container.GetIPidStream(EPDT_ParentId);
		IQuatStream parentQuats = parentContainer.GetIQuatStream(EPQF_Orientation, defaultQuat);
		IOVec3Stream positions = container.GetIOVec3Stream(EPVF_Position);
		STempInitBuffer<float> scales(runtime, m_scale);

		SDistributor<3, Vec3> distributor(m_distribution, runtime);
		distributor.SetRange(0, {-m_box.x, +m_box.x});
		distributor.SetRange(1, {-m_box.y, +m_box.y});
		distributor.SetRange(2, {-m_box.z, +m_box.z});

		for (auto particleId : runtime.SpawnedRange())
		{
			const TParticleId parentId = parentIds.Load(particleId);
			const float scale = scales.SafeLoad(particleId);
			const Vec3 wPosition0 = positions.Load(particleId);
			const Quat wQuat = parentQuats.SafeLoad(parentId);
			const Vec3 oOffset = distributor();
			const Vec3 wOffset = wQuat * oOffset;
			const Vec3 wPosition1 = wPosition0 + wOffset * scale;
			positions.Store(particleId, wPosition1);
		}
	}

	virtual void UpdateGPUParams(CParticleComponentRuntime& runtime, gpu_pfx2::SUpdateParams& params) override
	{
		params.box = m_box;
		params.scale.x = m_scale.GetValueRange(runtime)(0.5f);
		params.initFlags |= gpu_pfx2::eFeatureInitializationFlags_LocationBox;
	}

private:
	Vec3                              m_box = ZERO;
	CParamMod<EDD_Particle, UFloat10> m_scale;
	SDistribution<3, Vec3>            m_distribution;
};

CRY_PFX2_IMPLEMENT_FEATURE(CParticleFeature, CFeatureLocationBox, "Location", "Box", colorLocation);

//////////////////////////////////////////////////////////////////////////
// CFeatureLocationSphere

class CFeatureLocationSphere : public CParticleFeature
{
public:
	CRY_PFX2_DECLARE_FEATURE
	virtual void AddToComponent(CParticleComponent* pComponent, SComponentParams* pParams) override
	{
		pComponent->InitParticles.add(this);
		pComponent->GetDynamicData.add(this);
		pComponent->UpdateGPUParams.add(this);
		m_radius.AddToComponent(pComponent, this);
		m_velocity.AddToComponent(pComponent, this);
	}

	virtual void Serialize(Serialization::IArchive& ar) override
	{
		CParticleFeature::Serialize(ar);
		SERIALIZE_VAR(ar, m_radius);
		SERIALIZE_VAR(ar, m_innerFraction);
		SERIALIZE_VAR(ar, m_velocity);
		SERIALIZE_VAR(ar, m_axisScale);
		m_distribution.Serialize(ar);
	}

	virtual void InitParticles(CParticleComponentRuntime& runtime) override
	{
		CRY_PFX2_PROFILE_DETAIL;

		const float EPSILON = 1.0f / 2048.0f;
		const bool useRadius = m_radius.GetBaseValue() > EPSILON;
		const bool useVelocity = abs(m_velocity.GetBaseValue()) > EPSILON;

		if (useRadius && useVelocity)
			SphericalDist<true, true>(runtime);
		else if (useRadius)
			SphericalDist<true, false>(runtime);
		else if (useVelocity)
			SphericalDist<false, true>(runtime);
	}

	virtual void UpdateGPUParams(CParticleComponentRuntime& runtime, gpu_pfx2::SUpdateParams& params) override
	{
		params.scale = m_axisScale;
		params.radius = m_radius.GetValueRange(runtime)(0.5f);
		params.velocity = m_velocity.GetValueRange(runtime)(0.5f);
		params.initFlags |= gpu_pfx2::eFeatureInitializationFlags_LocationSphere;
	}

	virtual void GetDynamicData(const CParticleComponentRuntime& runtime, EParticleDataType type, void* data, EDataDomain domain, SUpdateRange range) override
	{
		if (auto extents = ESDT_SpatialExtents.Cast(type, data, range))
		{
			SSpawnerUpdateBuffer<float> sizes(runtime, m_radius, domain);
			for (auto i : range)
			{
				const Vec3 axis1 = m_axisScale * abs(sizes[i].end),
						   axis0 = m_axisScale * abs(sizes[i].start);
				extents[i] += Vec4(1,
					(axis1.x + axis1.y + axis1.z) * 0.5f,
					axis1.x * axis1.y + axis1.y * axis1.z + axis1.z * axis1.x,
					axis1.x * axis1.y * axis1.z - axis0.x * axis0.y * axis0.z
				) * (gf_PI * 4.0f / 3.0f);
			}
		}
	}

private:
	template<const bool UseRadius, const bool UseVelocity>
	void SphericalDist(CParticleComponentRuntime& runtime)
	{
		CParticleContainer& container = runtime.GetContainer();
		IOVec3Stream positions = container.GetIOVec3Stream(EPVF_Position);
		IOVec3Stream velocities = container.GetIOVec3Stream(EPVF_Velocity);

		STempInitBuffer<float> radii(runtime, m_radius);
		STempInitBuffer<float> velocityMults(runtime, m_velocity);

		SBallDistributor<DistributorTypes> distributor(m_distribution, runtime);
		distributor.SetRadiusRange({m_innerFraction, 1.0f});

		for (auto particleId : runtime.SpawnedRange())
		{
			const Vec3 sphere = distributor();
			const Vec3 distributor = sphere.CompMul(m_axisScale);

			if (UseRadius)
			{
				const float radius = radii.SafeLoad(particleId);
				const Vec3 wPosition0 = positions.Load(particleId);
				const Vec3 wPosition1 = wPosition0 + distributor * radius;
				positions.Store(particleId, wPosition1);
			}

			if (UseVelocity)
			{
				const float velocityMult = velocityMults.SafeLoad(particleId);
				const Vec3 wVelocity0 = velocities.Load(particleId);
				const Vec3 wVelocity1 = wVelocity0 + distributor * velocityMult;
				velocities.Store(particleId, wVelocity1);
			}
		}
	}

	CParamMod<EDD_Particle, UFloat10> m_radius;
	CParamMod<EDD_Particle, SFloat10> m_velocity;
	UUnitFloat                        m_innerFraction = 1;
	Vec3                              m_axisScale {1};
	SDistribution<3>                  m_distribution;
};

CRY_PFX2_IMPLEMENT_FEATURE(CParticleFeature, CFeatureLocationSphere, "Location", "Sphere", colorLocation);

//////////////////////////////////////////////////////////////////////////
// CFeatureLocationCircle

class CFeatureLocationCircle : public CParticleFeature
{
public:
	CRY_PFX2_DECLARE_FEATURE
	virtual void AddToComponent(CParticleComponent* pComponent, SComponentParams* pParams) override
	{
		pComponent->InitParticles.add(this);
		pComponent->GetDynamicData.add(this);
		pComponent->UpdateGPUParams.add(this);
		m_radius.AddToComponent(pComponent, this);
		m_velocity.AddToComponent(pComponent, this);
	}

	virtual void Serialize(Serialization::IArchive& ar) override
	{
		CParticleFeature::Serialize(ar);
		SERIALIZE_VAR(ar, m_radius);
		SERIALIZE_VAR(ar, m_velocity);
		SERIALIZE_VAR(ar, m_innerFraction);
		SERIALIZE_VAR(ar, m_axisScale);
		SERIALIZE_VAR(ar, m_axis);
		m_distribution.Serialize(ar);
	}

	virtual void InitParticles(CParticleComponentRuntime& runtime) override
	{
		CRY_PFX2_PROFILE_DETAIL;

		const float EPSILON = 1.0f / 2048.0f;
		const bool useRadius = m_radius.GetBaseValue() > EPSILON;
		const bool useVelocity = abs(m_velocity.GetBaseValue()) > EPSILON;

		if (useRadius && useVelocity)
			CircularDist<true, true>(runtime);
		else if (useRadius)
			CircularDist<true, false>(runtime);
		else if (useVelocity)
			CircularDist<false, true>(runtime);
	}

	virtual void UpdateGPUParams(CParticleComponentRuntime& runtime, gpu_pfx2::SUpdateParams& params) override
	{
		params.scale.x = m_axisScale.x;
		params.scale.y = m_axisScale.y;
		params.radius = m_radius.GetValueRange(runtime)(0.5f);
		params.velocity = m_velocity.GetValueRange(runtime)(0.5f);
		params.initFlags |= gpu_pfx2::eFeatureInitializationFlags_LocationCircle;
	}

	virtual void GetDynamicData(const CParticleComponentRuntime& runtime, EParticleDataType type, void* data, EDataDomain domain, SUpdateRange range) override
	{
		if (auto extents = ESDT_SpatialExtents.Cast(type, data, range))
		{
			SSpawnerUpdateBuffer<float> sizes(runtime, m_radius, domain);
			for (auto i : range)
			{
				const Vec2 axis1 = m_axisScale * abs(sizes[i].end),
				           axis0 = m_axisScale * abs(sizes[i].start);
				extents[i] += Vec4(1,
					axis1.x + axis1.y,
					axis1.x * axis1.y - axis0.x * axis0.y,
					0
				) * gf_PI;
			}
		}
	}

private:
	template<const bool UseRadius, const bool UseVelocity>
	void CircularDist(CParticleComponentRuntime& runtime)
	{
		CParticleContainer& parentContainer = runtime.GetParentContainer();
		CParticleContainer& container = runtime.GetContainer();
		const Quat defaultQuat = runtime.GetEmitter()->GetLocation().q;
		const IPidStream parentIds = container.GetIPidStream(EPDT_ParentId);
		const IQuatStream parentQuats = parentContainer.GetIQuatStream(EPQF_Orientation, defaultQuat);
		const Quat axisQuat = Quat::CreateRotationV0V1(Vec3(0.0f, 0.0f, 1.0f), m_axis.GetNormalizedSafe());
		IOVec3Stream positions = container.GetIOVec3Stream(EPVF_Position);
		IOVec3Stream velocities = container.GetIOVec3Stream(EPVF_Velocity);

		STempInitBuffer<float> radii(runtime, m_radius);
		STempInitBuffer<float> velocityMults(runtime, m_velocity);
		SDiskDistributor<DistributorTypes> distributor(m_distribution, runtime);
		distributor.SetRadiusRange({m_innerFraction, 1.0f});

		for (auto particleId : runtime.SpawnedRange())
		{
			TParticleId parentId = parentIds.Load(particleId);
			const Quat wQuat = parentQuats.SafeLoad(parentId);
			const Vec2 disc2 = distributor();
			const Vec3 disc3 = axisQuat * Vec3(disc2.x * m_axisScale.x, disc2.y * m_axisScale.y, 0.0f);

			if (UseRadius)
			{
				const float radius = radii.SafeLoad(particleId);
				const Vec3 oPosition = disc3 * radius;
				const Vec3 wPosition0 = positions.Load(particleId);
				const Vec3 wPosition1 = wPosition0 + wQuat * oPosition;
				positions.Store(particleId, wPosition1);
			}

			if (UseVelocity)
			{
				const float velocityMult = velocityMults.SafeLoad(particleId);
				const Vec3 wVelocity0 = velocities.Load(particleId);
				const Vec3 wVelocity1 = wVelocity0 + wQuat * disc3 * velocityMult;
				velocities.Store(particleId, wVelocity1);
			}
		}
	}

private:
	CParamMod<EDD_Particle, UFloat10> m_radius;
	CParamMod<EDD_Particle, SFloat10> m_velocity;
	UUnitFloat                        m_innerFraction = 1;
	Vec3                              m_axis {0, 0, 1};
	Vec2                              m_axisScale {1, 1};
	SDistribution<2>                  m_distribution;
};

CRY_PFX2_IMPLEMENT_FEATURE(CParticleFeature, CFeatureLocationCircle, "Location", "Circle", colorLocation);

//////////////////////////////////////////////////////////////////////////
// CFeatureLocationGeometry

extern TDataType<IMeshObj*>        EPDT_MeshGeometry;
extern TDataType<IPhysicalEntity*> EPDT_PhysicalEntity;

SERIALIZATION_DECLARE_ENUM(EGeometrySource,
                           Render = GeomType_Render,
                           Physics = GeomType_Physics
                           )

SERIALIZATION_DECLARE_ENUM(EGeometryLocation,
                           Vertices = GeomForm_Vertices,
                           Edges = GeomForm_Edges,
                           Surface = GeomForm_Surface,
                           Volume = GeomForm_Volume
                           )

class CFeatureLocationGeometry : public CParticleFeature
{
public:
	CRY_PFX2_DECLARE_FEATURE

	virtual void AddToComponent(CParticleComponent* pComponent, SComponentParams* pParams) override
	{
		pComponent->MainPreUpdate.add(this);
		pComponent->GetDynamicData.add(this);
		m_offset.AddToComponent(pComponent, this);
		m_velocity.AddToComponent(pComponent, this);
		if (m_orientToNormal)
			pComponent->AddParticleData(EPQF_Orientation);
	}

	virtual void Serialize(Serialization::IArchive& ar) override
	{
		CParticleFeature::Serialize(ar);
		SERIALIZE_VAR(ar, m_source);
		SERIALIZE_VAR(ar, m_location);
		SERIALIZE_VAR(ar, m_offset);
		SERIALIZE_VAR(ar, m_velocity);
		SERIALIZE_VAR(ar, m_orientToNormal);
		SERIALIZE_VAR(ar, m_augmentLocation);
	}

	virtual void MainPreUpdate(CParticleComponentRuntime& runtime) override
	{
		CRY_PROFILE_FUNCTION(PROFILE_PARTICLE);

		if (CParticleComponent* pParentComponent = runtime.GetComponent()->GetParentComponent())
		{
			if (IMeshObj* pMesh = pParentComponent->GetComponentParams().m_pMesh)
				pMesh->GetExtent((EGeomForm)m_location);
		}
		else if (CParticleEmitter* pEmitter = runtime.GetEmitter())
		{
			pEmitter->UpdateEmitGeomFromEntity();
			const GeomRef& emitterGeometry = pEmitter->GetEmitterGeometry();
			const EGeomType geomType = (EGeomType)m_source;
			const EGeomForm geomForm = (EGeomForm)m_location;
			emitterGeometry.GetExtent(geomType, geomForm);
		}
	}

	virtual void GetDynamicData(const CParticleComponentRuntime& runtime, EParticleDataType type, void* data, EDataDomain domain, SUpdateRange range) override
	{
		if (auto extents = ESDT_SpatialExtents.Cast(type, data, range))
		{
			if (!runtime.GetEmitter())
				return;

			GeomRef emitterGeometry = runtime.GetEmitter()->GetEmitterGeometry();
			CParticleComponent* pParentComponent = runtime.GetComponent()->GetParentComponent();
			if (pParentComponent)
			{
				if (IMeshObj* pMesh = pParentComponent->GetComponentParams().m_pMesh)
					emitterGeometry.Set(pMesh);
			}
			auto parentIds = runtime.IStream(ESDT_ParentId);
			auto parentMeshes = runtime.GetParentContainer().IStream(EPDT_MeshGeometry, +emitterGeometry.m_pMeshObj);
			auto parentPhysics = runtime.GetParentContainer().IStream(EPDT_PhysicalEntity);

			for (auto i : range)
			{
				if (pParentComponent)
				{
					TParticleId parentId = parentIds[i];
					if (IMeshObj* mesh = parentMeshes.Load(parentId))
						emitterGeometry.Set(mesh);
					if (m_source == EGeometrySource::Physics)
					{
						if (IPhysicalEntity* pPhysics = parentPhysics.Load(parentId))
							emitterGeometry.Set(pPhysics);
					}
				}
				float extent = emitterGeometry.GetExtent((EGeomType)m_source, (EGeomForm)m_location);
				int dim = (int)m_location;
				extents[i][dim] += extent;
			}
		}
	}

	virtual void InitParticles(CParticleComponentRuntime& runtime) override
	{
		CRY_PROFILE_FUNCTION(PROFILE_PARTICLE);

		const float EPSILON = 1.0f / 2048.0f;
		const bool useVelocity = abs(m_velocity.GetBaseValue()) > EPSILON;

		const CParticleEmitter* pEmitter = runtime.GetEmitter();
		const CParticleComponent* pParentComponent = runtime.GetComponent()->GetParentComponent();

		GeomRef emitterGeometry = pEmitter->GetEmitterGeometry();
		bool geometryCentered = false;
		if (pParentComponent)
		{
			IMeshObj* pMesh = pParentComponent->GetComponentParams().m_pMesh;
			if (pMesh)
				emitterGeometry.Set(pMesh);
			geometryCentered = pParentComponent->GetComponentParams().m_meshCentered;
		}
		if (!emitterGeometry)
			return;

		const EGeomType geomType = (EGeomType)m_source;
		const EGeomForm geomForm = (EGeomForm)m_location;
		QuatTS geomLocation = pEmitter->GetEmitterGeometryLocation();
		const QuatTS emitterLocation = pEmitter->GetLocation();
		CParticleContainer& container = runtime.GetContainer();
		const CParticleContainer& parentContainer = runtime.GetParentContainer();
		const IPidStream parentIds = container.GetIPidStream(EPDT_ParentId);
		const IVec3Stream parentPositions = parentContainer.GetIVec3Stream(EPVF_Position, geomLocation.t);
		const IQuatStream parentQuats = parentContainer.GetIQuatStream(EPQF_Orientation, geomLocation.q);
		const IFStream parentSizes = parentContainer.GetIFStream(EPDT_Size, geomLocation.s);
		const TIStream<IMeshObj*> parentMeshes = parentContainer.IStream(EPDT_MeshGeometry, +emitterGeometry.m_pMeshObj);
		const TIStream<IPhysicalEntity*> parentPhysics = parentContainer.IStream(EPDT_PhysicalEntity);
		IOVec3Stream positions = container.GetIOVec3Stream(EPVF_Position);
		IOVec3Stream velocities = container.GetIOVec3Stream(EPVF_Velocity);
		IOQuatStream orientations = container.GetIOQuatStream(EPQF_Orientation);

		STempInitBuffer<float> offsets(runtime, m_offset);
		STempInitBuffer<float> velocityMults(runtime, m_velocity);

		auto spawnRange = container.SpawnedRange();
		TParticleHeap::Array<PosNorm> randomPoints(runtime.MemHeap(), spawnRange.size());

		// Count children for each parent attachment object
		struct GeomChildren
		{
			GeomRef geom;
			uint pos;
		};
		THeapArray<GeomChildren*> parentGeom(runtime.MemHeap());
		THeapArray<GeomChildren> mapGeomChildren(runtime.MemHeap());

		if (!pParentComponent)
		{
			emitterGeometry.GetRandomPoints(
				randomPoints,
				runtime.Chaos().Rand(), geomType, geomForm,
				&geomLocation, geometryCentered);
		}
		else
		{
			auto FindGeomChildren = [&mapGeomChildren](const GeomRef& geom) -> GeomChildren&
			{
				for (auto& elem : mapGeomChildren)
					if (elem.geom == geom)
						return elem;
				mapGeomChildren.push_back(GeomChildren{geom, 0});
				return mapGeomChildren.back();
			};

			parentGeom.resize(spawnRange.size());
			mapGeomChildren.reserve(parentContainer.GetNumParticles());
			for (auto particleId : runtime.SpawnedRange())
			{
				GeomRef particleGeometry = emitterGeometry;
				TParticleId parentId = parentIds.Load(particleId);
				if (IMeshObj* mesh = parentMeshes.SafeLoad(parentId))
					particleGeometry.Set(mesh);
				if (m_source == EGeometrySource::Physics)
				{
					if (IPhysicalEntity* pPhysics = parentPhysics.SafeLoad(parentId))
						particleGeometry.Set(pPhysics);
				}

				auto& geom = FindGeomChildren(particleGeometry);

				parentGeom[particleId - spawnRange.m_begin] = &geom;
				geom.pos++;
			}

			// Assign geom position, and get random points
			uint pos = 0;
			for (auto& elem : mapGeomChildren)
			{
				uint count = elem.pos;
				elem.pos = pos;

				elem.geom.GetRandomPoints(
					randomPoints(pos, count),
					runtime.Chaos().Rand(), geomType, geomForm,
					nullptr, geometryCentered);
				pos += count;
			}
		}

		for (auto particleId : runtime.SpawnedRange())
		{
			PosNorm randPositionNormal;
			
			if (!pParentComponent)
			{
				randPositionNormal = randomPoints[particleId - spawnRange.m_begin];
				if (m_augmentLocation)
				{
					Vec3 wPosition = positions.Load(particleId);
					randPositionNormal.vPos += wPosition - emitterLocation.t;
				}
			}
			else
			{
				TParticleId parentId = parentIds.Load(particleId);
				geomLocation.t = parentPositions.SafeLoad(parentId);
				geomLocation.q = parentQuats.SafeLoad(parentId);
				geomLocation.s = parentSizes.SafeLoad(parentId);

				auto& geom = *parentGeom[particleId - spawnRange.m_begin];
				randPositionNormal = randomPoints[geom.pos++];
				randPositionNormal <<= geomLocation;
			}
			
			const float offset = offsets.SafeLoad(particleId);
			const Vec3 wPosition = randPositionNormal.vPos + randPositionNormal.vNorm * offset;
			assert((wPosition - geomLocation.t).len() < 100000);
			positions.Store(particleId, wPosition);

			if (useVelocity)
			{
				const float velocityMult = velocityMults.SafeLoad(particleId);
				const Vec3 wVelocity0 = velocities.Load(particleId);
				const Vec3 wVelocity1 = wVelocity0 + randPositionNormal.vNorm * velocityMult;
				velocities.Store(particleId, wVelocity1);
			}

			if (m_orientToNormal)
			{
				const Quat wOrient0 = orientations.Load(particleId);
				const Quat oOrient = Quat::CreateRotationV0V1(randPositionNormal.vNorm, Vec3(0.0f, 0.0f, 1.0f));
				const Quat wOrient1 = wOrient0 * oOrient;
				orientations.Store(particleId, wOrient1);
			}
		}
	}

	EGeometrySource                   m_source          = EGeometrySource::Render;
	EGeometryLocation                 m_location        = EGeometryLocation::Surface;
	CParamMod<EDD_Particle, SFloat10> m_offset          = 0;
	CParamMod<EDD_Particle, SFloat10> m_velocity        = 0;
	bool                              m_orientToNormal  = true;
	bool                              m_augmentLocation = false;
};

CRY_PFX2_IMPLEMENT_FEATURE(CParticleFeature, CFeatureLocationGeometry, "Location", "Geometry", colorLocation);

//////////////////////////////////////////////////////////////////////////
// CFeatureLocationNoise

class CFeatureLocationNoise : public CParticleFeature
{
private:
	typedef TValue<THardLimits<uint, 1, 6>> UIntOctaves;

public:
	CRY_PFX2_DECLARE_FEATURE

	CFeatureLocationNoise()
		: m_amplitude(1.0f)
		, m_size(1.0f)
		, m_rate(0.0f)
		, m_octaves(1)
	{}

	virtual void AddToComponent(CParticleComponent* pComponent, SComponentParams* pParams) override
	{
		pComponent->InitParticles.add(this);
		pComponent->UpdateGPUParams.add(this);
		m_amplitude.AddToComponent(pComponent, this);
	}

	virtual void Serialize(Serialization::IArchive& ar) override
	{
		CParticleFeature::Serialize(ar);
		ar(m_amplitude, "Amplitude", "Amplitude");
		ar(m_size, "Size", "Size");
		ar(m_rate, "Rate", "Rate");
		ar(m_octaves, "Octaves", "Octaves");
	}

	virtual void InitParticles(CParticleComponentRuntime& runtime) override
	{
		CRY_PROFILE_FUNCTION(PROFILE_PARTICLE);

		const float maxSize = (float)(1 << 12);
		const float minSize = rcp_fast(maxSize); // small enough and prevents SIMD exceptions
		const float invSize = rcp_fast(max(minSize, +m_size));
		const float time = mod(runtime.GetEmitter()->GetTime() * m_rate * minSize, 1.0f) * maxSize;
		const float delta = m_rate * runtime.DeltaTime();
		CParticleContainer& container = runtime.GetContainer();
		const IFStream ages = container.GetIFStream(EPDT_NormalAge);
		const IFStream lifeTimes = container.GetIFStream(EPDT_LifeTime);
		IOVec3Stream positions = container.GetIOVec3Stream(EPVF_Position);

		STempInitBuffer<float> sizes(runtime, m_amplitude);

		for (auto particleId : runtime.SpawnedRange())
		{
			const float amplitude = sizes.SafeLoad(particleId);
			const Vec3 wPosition0 = positions.Load(particleId);
			const float age = ages.Load(particleId);
			const float lifeTime = lifeTimes.Load(particleId);
			Vec4 sample;
			sample.x = wPosition0.x * invSize;
			sample.y = wPosition0.y * invSize;
			sample.z = wPosition0.z * invSize;
			sample.w = StartTime(time, delta, age * lifeTime);
			const Vec3 potential = Fractal(sample, m_octaves);
			const Vec3 wPosition1 = potential * amplitude + wPosition0;
			positions.Store(particleId, wPosition1);
		}
	}

	virtual void UpdateGPUParams(CParticleComponentRuntime& runtime, gpu_pfx2::SUpdateParams& params) override
	{
		params.amplitude = m_amplitude.GetValueRange(runtime)(0.5f);
		params.noiseSize = m_size;
		params.rate = m_rate;
		params.octaves = m_octaves;
		params.initFlags |= gpu_pfx2::eFeatureInitializationFlags_LocationNoise;
	}
	
private:
	// non-inline version with Vec4f conversion
	static float SNoise(const Vec4& v)
	{
		return ::SNoise(static_cast<const Vec4f&>(v));
	}

	ILINE static Vec3 Potential(const Vec4 sample)
	{
		const Vec4 offy = Vec4(149, 311, 191, 491);
		const Vec4 offz = Vec4(233, 197, 43, 59);
		const Vec3 potential = Vec3(
		  SNoise(sample),
		  SNoise(sample + offy),
		  SNoise(sample + offz));
		return potential;
	}

	ILINE static Vec3 Fractal(const Vec4 sample, const uint octaves)
	{
		Vec3 total = Vec3(ZERO);
		float mult = 1.0f;
		float totalMult = 0.0f;
		for (uint i = 0; i < octaves; ++i)
		{
			totalMult += mult;
			mult *= 0.5f;
		}
		mult = rcp_fast(totalMult);
		float size = 1.0f;
		for (uint i = 0; i < octaves; ++i)
		{
			total += Potential(sample * size) * mult;
			size *= 2.0f;
			mult *= 0.5f;
		}
		return total;
	}

	CParamMod<EDD_Particle, SFloat10> m_amplitude;
	UFloat10                          m_size;
	UFloat10                          m_rate;
	UIntOctaves                       m_octaves;
};

CRY_PFX2_IMPLEMENT_FEATURE(CParticleFeature, CFeatureLocationNoise, "Location", "Noise", colorLocation);

//////////////////////////////////////////////////////////////////////////
// CFeatureLocationBeam

class CFeatureLocationBeam : public CParticleFeature
{
public:
	CRY_PFX2_DECLARE_FEATURE

	virtual void AddToComponent(CParticleComponent* pComponent, SComponentParams* pParams) override
	{
		pComponent->InitParticles.add(this);
		pComponent->GetDynamicData.add(this);
		m_source.AddToComponent(pComponent);
		m_destination.AddToComponent(pComponent);
	}

	virtual void Serialize(Serialization::IArchive& ar) override
	{
		CParticleFeature::Serialize(ar);
		SERIALIZE_VAR(ar, m_source);
		if (ar.isInput() && GetVersion(ar) <= 5)
			ar(m_destination, "Destiny", "Destination");
		SERIALIZE_VAR(ar, m_destination);
		if (!ar(m_position, "Position", "Position") && ar.isInput())
		{
			// Compatibility with previous behavior: Add Linear:SpawnFraction modifier
			static char modifierText[] =
			"{ \
				\"value\": 1.0, \
				\"modifiers\": [ \
					{ \"Linear\": { \
						\"Domain\": \"SpawnFraction\", \
					} } \
				] \
			}";
			Serialization::LoadJsonBuffer(m_position, modifierText, strlen(modifierText));
		}
	}

	virtual void InitParticles(CParticleComponentRuntime& runtime) override
	{
		CRY_PROFILE_FUNCTION(PROFILE_PARTICLE);

		CParticleContainer& container = runtime.GetContainer();
		IOVec3Stream positions = container.GetIOVec3Stream(EPVF_Position);
		STempInitBuffer<float> fractions(runtime, m_position);

		for (auto particleId : runtime.SpawnedRange())
		{
			const Vec3 wSource = m_source.GetTarget(runtime, particleId);
			const Vec3 wDestination = m_destination.GetTarget(runtime, particleId);
			const float fraction = fractions[particleId];
			const Vec3 wPosition = wSource + (wDestination - wSource) * fraction;
			positions.Store(particleId, wPosition);
		}
	}

	virtual void GetDynamicData(const CParticleComponentRuntime& runtime, EParticleDataType type, void* data, EDataDomain domain, SUpdateRange range) override
	{
		if (auto extents = ESDT_SpatialExtents.Cast(type, data, range))
		{
			if (domain == EDD_Spawner)
			{
				auto parentIds = runtime.IStream(ESDT_ParentId);
				for (auto i : range)
				{
					const TParticleId parentId = parentIds[i];
					const Vec3 wSource = m_source.GetTarget(runtime, parentId, true);
					const Vec3 wDestination = m_destination.GetTarget(runtime, parentId, true);
					extents[i] += Vec4(1,
						(wSource - wDestination).GetLengthFast(),
						0, 0);
				}
			}
		}
	}

private:
	CTargetSource                       m_source      = ETargetSource::Parent;
	CTargetSource                       m_destination = ETargetSource::Target;
	CParamMod<EDD_Particle, UUnitFloat> m_position    = 1;
};

CRY_PFX2_IMPLEMENT_FEATURE(CParticleFeature, CFeatureLocationBeam, "Location", "Beam", colorLocation);

//////////////////////////////////////////////////////////////////////////
// CFeatureLocationBindToCamera

class CFeatureLocationBindToCamera : public CParticleFeature
{
public:
	CRY_PFX2_DECLARE_FEATURE

	virtual void AddToComponent(CParticleComponent* pComponent, SComponentParams* pParams) override
	{
		pComponent->PostInitParticles.add(this);
		if (!m_spawnOnly)
			pComponent->PreUpdateParticles.add(this);
	}

	virtual void Serialize(Serialization::IArchive& ar) override
	{
		CParticleFeature::Serialize(ar);
		SERIALIZE_VAR(ar, m_spawnOnly);
	}

	virtual void PostInitParticles(CParticleComponentRuntime& runtime) override
	{
		CRY_PFX2_PROFILE_DETAIL;

		const CCamera& camera = gEnv->p3DEngine->GetRenderingCamera();
		const QuatT wCameraPose = QuatT(camera.GetMatrix());

		CParticleContainer& container = runtime.GetContainer();
		const CParticleContainer& parentContainer = runtime.GetParentContainer();
		const auto parentIds = container.IStream(EPDT_ParentId);
		const auto parentPositions = parentContainer.IStream(EPVF_Position);
		const auto parentOrientations = parentContainer.IStream(EPQF_Orientation);
		auto positions = container.IOStream(EPVF_Position);
		auto orientations = container.IOStream(EPQF_Orientation);
		auto velocities = container.IOStream(EPVF_Velocity);

		for (auto particleId : runtime.SpawnedRange())
		{
			const TParticleId parentId = parentIds.Load(particleId);
			const Vec3 wParentPosition = parentPositions.Load(parentId);
			const Quat wParentOrientation = parentOrientations.Load(parentId);
			const QuatT worldToParent = QuatT(wParentPosition, wParentOrientation).GetInverted();

			Vec3 wPosition = positions.Load(particleId);
			wPosition = wCameraPose * (worldToParent * wPosition);
			positions.Store(particleId, wPosition);

			if (container.HasData(EPQF_Orientation))
			{
				Quat wOrientation = orientations.Load(particleId);
				wOrientation = wCameraPose.q * (worldToParent.q * wOrientation);
				orientations.Store(particleId, wOrientation);
			}

			Vec3 wVelocity = velocities.Load(particleId);
			wVelocity = wCameraPose.q * (worldToParent.q * wVelocity);
			velocities.Store(particleId, wVelocity);
		}
	}

	virtual void PreUpdateParticles(CParticleComponentRuntime& runtime) override
	{
		CRY_PFX2_PROFILE_DETAIL;

		const CCamera& camera = gEnv->p3DEngine->GetRenderingCamera();
		const QuatT wCurCameraPose = QuatT(camera.GetMatrix());
		const QuatT wPrevCameraPose = GetPSystem()->GetLastCameraPose();
		const Quat cameraRotation = wCurCameraPose.q * wPrevCameraPose.q.GetInverted();

		CParticleContainer& container = runtime.GetContainer();
		auto positions = container.IOStream(EPVF_Position);
		auto orientations = container.IOStream(EPQF_Orientation);
		auto velocities = container.IOStream(EPVF_Velocity);

		for (auto particleId : container.NonSpawnedRange())
		{
			Vec3 wPosition = positions.Load(particleId);
			wPosition = cameraRotation * (wPosition - wPrevCameraPose.t) + wCurCameraPose.t;
			positions.Store(particleId, wPosition);

			if (container.HasData(EPQF_Orientation))
			{
				Quat wOrientation = orientations.Load(particleId);
				wOrientation = cameraRotation * wOrientation;
				orientations.Store(particleId, wOrientation);
			}

			Vec3 wVelocity = velocities.Load(particleId);
			wVelocity = cameraRotation * wVelocity;
			velocities.Store(particleId, wVelocity);
		}
	}

private:
	bool m_spawnOnly = false;
};

CRY_PFX2_IMPLEMENT_FEATURE(CParticleFeature, CFeatureLocationBindToCamera, "Location", "BindToCamera", colorLocation);

//////////////////////////////////////////////////////////////////////////
// CFeatureLocationOmni

MakeDataType(ESVF_VelocityFinal, Vec3, EDD_Spawner);
MakeDataType(ESVF_Travel, Vec3, EDD_Spawner);

extern TDataType<Vec3> EPVF_PositionPrev;

using Matrix34v = Matrix34_tpl<floatv>;

static CubeRootApprox cubeRootApprox(0.125f);

ILINE void DoElements(int mask, const Vec3v& vv, std::function<void(const Vec3& v)> func)
{
#ifdef CRY_PFX2_USE_SSE
	static_assert(CRY_PFX2_GROUP_STRIDE == 4, "Particle data vectorization != 4");
	if (mask & 1)
		func(Vec3(get_element<0>(vv.x), get_element<0>(vv.y), get_element<0>(vv.z)));
	if (mask & 2)
		func(Vec3(get_element<1>(vv.x), get_element<1>(vv.y), get_element<1>(vv.z)));
	if (mask & 4)
		func(Vec3(get_element<2>(vv.x), get_element<2>(vv.y), get_element<2>(vv.z)));
	if (mask & 8)
		func(Vec3(get_element<3>(vv.x), get_element<3>(vv.y), get_element<3>(vv.z)));
#else
	if (mask)
		func(vv);
#endif
}

class CFeatureLocationOmni : public CParticleFeature
{
public:
	CRY_PFX2_DECLARE_FEATURE

	virtual void AddToComponent(CParticleComponent* pComponent, SComponentParams* pParams) override
	{
		pComponent->CullSpawners.add(this);
		pComponent->InitSpawners.add(this);
		pComponent->GetDynamicData.add(this);
		pComponent->KillParticles.add(this);
		pComponent->SpawnParticles.add(this);
		pComponent->InitParticles.add(this);
		m_visibilityRange.AddToComponent(pComponent, this);

		pComponent->AddParticleData(EPVF_PositionPrev);
		pComponent->AddParticleData(ESVF_VelocityFinal);
		pComponent->AddParticleData(ESVF_Travel);

		if (m_spawnOutsideView)
		{
			pParams->m_isPreAged = true;
			pComponent->InitParticles.add(this);
		}

		if (!m_useEmitterLocation)
		{
			// auto-set distance culling
			const float visibility = m_visibilityRange.GetValueRange().end;
			auto& maxCamDistance = pParams->m_visibility.m_maxCameraDistance;
			if (visibility < maxCamDistance)
				maxCamDistance = visibility;
		}
	}

	virtual void Serialize(Serialization::IArchive& ar) override
	{
		CParticleFeature::Serialize(ar);
		SERIALIZE_VAR(ar, m_visibilityRange);
		if (ar.isInput() && GetVersion(ar) < 14)
			ar(m_visibilityRange, "Visibility", "Visibility");
		SERIALIZE_VAR(ar, m_spawnOutsideView);
	#ifndef _RELEASE
		SERIALIZE_VAR(ar, m_useEmitterLocation);
	#endif
	}

	virtual void CullSpawners(CParticleComponentRuntime& runtime, TVarArray<SSpawnerDesc>& spawners) override
	{
		// Allow only one spawner per component
		uint numAllowed = 1 - runtime.Container(EDD_Spawner).Size();
		if (numAllowed < spawners.size())
			spawners.resize(numAllowed);
	}

	virtual void InitSpawners(CParticleComponentRuntime& runtime) override
	{
		// Need to compute average final velocity and travel for particles.
		// Run particles for full lifetime in a temporary runtime
		CParticleComponentRuntime runtimeTemp(runtime.GetEmitter(), runtime.GetComponent());
		runtimeTemp.RunParticles(32, runtime.ComponentParams().m_maxParticleLife);

		CParticleContainer& container = runtimeTemp.GetContainer();
		auto positions = container.IStream(EPVF_Position);
		auto positionsPrev = container.IStream(EPVF_PositionPrev, runtime.GetEmitter()->GetLocation().t);
		auto velocities = container.IStream(EPVF_Velocity);

		Vec3 velocityFinal(0), travel(0);
		for (auto particleId : runtimeTemp.FullRange())
		{
			velocityFinal += velocities.Load(particleId);
			travel += positions.Load(particleId) - positionsPrev.SafeLoad(particleId);
		}
		velocityFinal /= (float)container.GetNumParticles();
		travel /= (float)container.GetNumParticles();

		runtime.FillData(ESVF_VelocityFinal, velocityFinal, runtime.SpawnedRange(EDD_Spawner));
		runtime.FillData(ESVF_Travel, travel, runtime.SpawnedRange(EDD_Spawner));
	}

	virtual void GetDynamicData(const CParticleComponentRuntime& runtime, EParticleDataType type, void* data, EDataDomain domain, SUpdateRange range) override
	{
		if (auto extents = ESDT_SpatialExtents.Cast(type, data, range))
		{
			UpdateCameraData(runtime);

			for (auto i : range)
			{
				float scale = m_camData.maxDistance;
				const Vec3 boxUnit(m_camData.scrWidth.x, m_camData.scrWidth.y, 1.0f);
				float capHeight = 1.0f - boxUnit.GetInvLength();
				float extent1 = capHeight * scale * 4.0f / 3.0f;
				extents[i] += Vec4(1,
					extent1,
					extent1 * scale * 2.0f,
					extent1 * scale * scale
				);
			}
		}
	}

	virtual void KillParticles(CParticleComponentRuntime& runtime) override
	{
		CRY_PFX2_PROFILE_DETAIL;

		CParticleContainer& container = runtime.GetContainer();
		auto ages = container.IOStream(EPDT_NormalAge);
		auto positions = container.IStream(EPVF_Position);

		UpdateCameraData(runtime);

		// All particles no longer in sector are killed (age -> 1)
		Matrix34v fromWorld = m_camData.fromWorld;
		for (auto particleId : runtime.FullRangeV())
		{
			floatv age = ages.Load(particleId);
			Vec3v pos = positions.Load(particleId);
			Vec3v posCam = fromWorld * pos;
			age = if_else(InSector(posCam), age, convert<floatv>(1.0f));
			ages.Store(particleId, age);
		}
	}

	virtual void SpawnParticles(CParticleComponentRuntime& runtime) override
	{
		CRY_PFX2_PROFILE_DETAIL;

		if (runtime.IsPreRunning())
			return;

		UpdateCameraData(runtime);

		CRY_ASSERT(runtime.Container(EDD_Spawner).Size() == 1);
		const Vec3 velocityFinal = runtime.IStream(ESVF_VelocityFinal).Load(0);

		Vec3 travelPrev = velocityFinal * m_camData.deltaTimePrev;
		Matrix34v toPrev = m_camData.fromWorldPrev * Matrix34::CreateTranslationMat(-travelPrev) * m_camData.toWorld;
		Matrix34v toWorld = m_camData.toWorld;

		CParticleContainer& container = runtime.GetContainer();
		float deltaTime = runtime.DeltaTime();
		int particlesTotal, particlesPerFrame;
		runtime.GetMaxParticleCounts(particlesTotal, particlesPerFrame, rcp(deltaTime), rcp(deltaTime));

		// Randomly generate positions in current sector; only those not in previous sector spawn as particles
		THeapArray<Vec3> newPositions(runtime.MemHeap());
		newPositions.reserve(particlesTotal);
		for (uint i = CRY_PFX2_GROUP_ALIGN(particlesTotal); i > 0; i -= CRY_PFX2_GROUP_STRIDE)
		{
			Vec3v posCam = RandomSector<floatv>(runtime.ChaosV());
			Vec3v posCamPrev = toPrev * posCam;
			int mask = Any(~InSector(posCamPrev));
			if (mask)
			{
				Vec3v posv = toWorld * posCam;
				DoElements(mask, posv, [&newPositions](const Vec3& v) { newPositions.push_back(v); });
			}
		}
		
		if (newPositions.size())
		{
			const float life = runtime.ComponentParams().m_maxParticleLife;
			float fracNewSpawned = deltaTime / life;
			SSpawnEntry spawn = {};
			spawn.m_count = uint(newPositions.size() * (1.0f - fracNewSpawned));
			spawn.m_ageBegin = 0.0f;
			spawn.m_ageIncrement = life / float(spawn.m_count);
			runtime.AddParticles({&spawn, 1});
		}

		// Store generated positions in PositionPrev, for use in InitParticles
		auto positionsPrev = container.IOStream(EPVF_PositionPrev);
		uint iNew = 0;
		for (auto particleId : runtime.SpawnedRange())
		{
			if (iNew < newPositions.size())
			{
				positionsPrev.Store(particleId, newPositions[iNew++]);
			}
			else
			{
				Vec3 posCam = RandomSector<float>(runtime.Chaos());
				Vec3 pos = m_camData.toWorld * posCam;
				positionsPrev.Store(particleId, pos);
			}
		}
	}

	virtual void InitParticles(CParticleComponentRuntime& runtime) override
	{
		CRY_PFX2_PROFILE_DETAIL;

		if (runtime.Container(EDD_Spawner).Size() == 0)
			return;
		if (runtime.IsPreRunning())
			return;

		UpdateCameraData(runtime);

		CParticleContainer& container = runtime.GetContainer();

		container.CopyData(EPVF_Position, EPVF_PositionPrev, runtime.SpawnedRange());
		if (!m_spawnOutsideView)
			return;

		auto positions = container.IOStream(EPVF_Position);
		auto positionsPrev = container.IOStream(EPVF_PositionPrev);
		auto velocities = container.IOStream(EPVF_Velocity);
		auto normAges = container.IOStream(EPDT_NormalAge);

		const Vec3 travel = runtime.IStream(ESVF_Travel).Load(0);
		const Vec3v travelV = ToVec3v(travel);
		const Vec3v velocityFinalV = ToVec3v(runtime.IStream(ESVF_VelocityFinal).Load(0));

		for (auto particleId : runtime.SpawnedRangeV())
		{
			Vec3v pos = positions.Load(particleId);
			positionsPrev.Store(particleId, pos - travelV);

			Vec3v vel = velocities.Load(particleId);
			vel += velocityFinalV;
			velocities.Store(particleId, vel);
		}

		float lifeFraction = 1.5f * m_camData.maxDistance * travel.GetInvLengthSafe();
		float curAge = max(1.0f - 2.0f * lifeFraction, 0.0f);
		float ageInc = lifeFraction / runtime.SpawnedRange().size();

		for (auto particleId : runtime.SpawnedRange())
		{
			normAges[particleId] = curAge;
			curAge += ageInc;
		}
	}

private:

	// Camera data is shared per-feature definition, based on the global render camera and effect params
	struct SCameraData
	{
		std::atomic<uint32> nFrameId      {0};
		Vec2                scrWidth      {0};
		float               maxDistance   {0};
		Matrix34            toWorld       {IDENTITY};
		Matrix34            fromWorld     {IDENTITY};
		Matrix34            fromWorldPrev {IDENTITY};
		float               deltaTime     {0};
		float               deltaTimePrev {0};
	};
	SCameraData m_camData;

	void UpdateCameraData(const CParticleComponentRuntime& runtime)
	{
		if (m_camData.nFrameId.exchange(gEnv->nMainFrameID) == gEnv->nMainFrameID)
			return;
		CCamera cam = GetEffectCamera(runtime);
		m_camData.maxDistance = m_visibilityRange.GetValueRange(runtime).end;
		m_camData.maxDistance = min(m_camData.maxDistance, +runtime.ComponentParams().m_visibility.m_maxCameraDistance);

		// Clamp view distance based on particle size
		const float maxSize = runtime.ComponentParams().m_maxParticleSize;
		const float maxParticleSize = maxSize * runtime.ComponentParams().m_visibility.m_viewDistanceMultiple;
		const float maxAngularDensity = GetPSystem()->GetMaxAngularDensity(cam);
		const float maxDistance = maxAngularDensity * maxParticleSize * runtime.GetEmitter()->GetViewDistRatio();
		if (maxDistance < m_camData.maxDistance)
			m_camData.maxDistance = maxDistance;

		m_camData.fromWorldPrev = m_camData.fromWorld;
		m_camData.deltaTimePrev = m_camData.deltaTime;
		m_camData.deltaTime = runtime.DeltaTime();

		m_camData.scrWidth = ScreenWidth(cam);

		// Matrix rotates Z to Y, scales by range, and offsets backward by particle size
		const Matrix34 toCam(
			m_camData.maxDistance * m_camData.scrWidth.x, 0, 0, 0,
			0, 0, m_camData.maxDistance, -maxSize,
			0, -m_camData.maxDistance * m_camData.scrWidth.y, 0, 0
		);
		// Concatenate with camera world location
		m_camData.toWorld = cam.GetMatrix() * toCam;
		m_camData.fromWorld = m_camData.toWorld.GetInverted();

		if (m_camData.nFrameId == 0)
		{
			m_camData.fromWorldPrev = m_camData.fromWorld;
			m_camData.deltaTimePrev = m_camData.deltaTime;
		}
	}

	CCamera GetEffectCamera(const CParticleComponentRuntime& runtime) const
	{
		if (!m_useEmitterLocation)
		{
			return gEnv->p3DEngine->GetRenderingCamera();
		}
		else
		{
			CCamera camera = gEnv->p3DEngine->GetRenderingCamera();
			const CParticleEmitter& emitter = *runtime.GetEmitter();
			Matrix34 matEmitter = Matrix34(emitter.GetLocation());
			camera.SetMatrix(matEmitter);
			return camera;
		}
	}

	static Vec2 ScreenWidth(const CCamera& cam)
	{
		Vec3 corner = cam.GetEdgeF();
		return Vec2(abs(corner.x), abs(corner.z)) / corner.y;
	}

	uint InSector(const Vec3& pos) const
	{
		return max(abs(pos.x), abs(pos.y)) <= pos.z
			&& pos.len2() <= 1.0f;
	}

#ifdef CRY_PFX2_USE_SSE
	mask32v4 InSector(const Vec3v& pos) const
	{
		return max(abs(pos.x), abs(pos.y)) <= pos.z
			& pos.len2() <= convert<floatv>(1.0f);
	}
#endif

	template<typename T, typename TChaos>
	Vec3_tpl<T> RandomSector(TChaos& chaos) const
	{
		Vec3_tpl<T> pos(
			chaos.RandSNorm(),
			chaos.RandSNorm(),
			convert<T>(1.0f));
		T r = cubeRootApprox(chaos.RandUNorm());
		pos *= r * pos.GetInvLengthFast();
		return pos;
	}

	// Parameters
	CParamMod<EDD_None, UFloat100> m_visibilityRange;
	bool                           m_spawnOutsideView = false;

	// Debugging options
	bool                           m_useEmitterLocation = false;
};

CRY_PFX2_IMPLEMENT_FEATURE(CParticleFeature, CFeatureLocationOmni, "Location", "Omni", colorLocation);

}
