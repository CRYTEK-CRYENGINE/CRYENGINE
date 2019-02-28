// Copyright 2015-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include <CrySystem/CryUnitTest.h>
#include <CryMath/SNoise.h>
#include <CrySerialization/Math.h>
#include "ParticleSystem/ParticleSystem.h"
#include "ParamMod.h"
#include "Target.h"

namespace pfx2
{

//////////////////////////////////////////////////////////////////////////
// CFeatureLocationOffset

class CFeatureLocationOffset : public CParticleFeature
{
public:
	CRY_PFX2_DECLARE_FEATURE

	CFeatureLocationOffset()
		: m_offset(ZERO)
		, m_scale(1.0f)
	{}

	virtual void AddToComponent(CParticleComponent* pComponent, SComponentParams* pParams) override
	{
		pComponent->GetSpatialExtents.add(this);
		pComponent->GetEmitOffset.add(this);
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

	virtual void GetSpatialExtents(const CParticleComponentRuntime& runtime, TConstArray<float> scales, TVarArray<float> extents) override
	{
		if (!m_scale.HasModifiers())
			return;

		TFloatArray sizes(runtime.MemHeap(), runtime.GetParentContainer().GetMaxParticles());
		auto modRange = m_scale.GetValues(runtime, sizes, EDD_InstanceUpdate);

		const uint numInstances = runtime.GetNumInstances();
		for (uint i = 0; i < numInstances; ++i)
		{
			const TParticleId parentId = runtime.GetInstance(i).m_parentId;
			float e = abs(sizes[parentId]) * modRange.Length();
			e = e * scales[i] + 1.0f;
			extents[i] += e;
		}
	}

	virtual void GetEmitOffset(const CParticleComponentRuntime& runtime, TParticleId parentId, Vec3& offset) override
	{
		TFloatArray scales(runtime.MemHeap(), parentId + 1);
		const SUpdateRange range(parentId, parentId + 1);

		auto modRange = m_scale.GetValues(runtime, scales.data(), range, EDD_InstanceUpdate);
		const float scale = scales[parentId] * (modRange.start + modRange.end) * 0.5f;
		offset += m_offset * scale;
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
	Vec3                                 m_offset;
	CParamMod<EDD_PerParticle, UFloat10> m_scale;
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
		pComponent->GetSpatialExtents.add(this);
		pComponent->UpdateGPUParams.add(this);
		m_scale.AddToComponent(pComponent, this);
	}

	virtual void Serialize(Serialization::IArchive& ar) override
	{
		CParticleFeature::Serialize(ar);
		ar(m_box, "Dimension", "Dimension");
		ar(m_scale, "Scale", "Scale");
	}

	virtual void GetSpatialExtents(const CParticleComponentRuntime& runtime, TConstArray<float> scales, TVarArray<float> extents) override
	{
		uint numInstances = runtime.GetNumInstances();
		TFloatArray sizes(runtime.MemHeap(), runtime.GetParentContainer().GetMaxParticles());
		auto modRange = m_scale.GetValues(runtime, sizes, EDD_InstanceUpdate);
		float avg = (modRange.start + modRange.end) * 0.5f;

		for (uint i = 0; i < numInstances; ++i)
		{
			// Increase each dimension by 1 to include boundaries; works properly for boxes, rects, and lines
			const TParticleId parentId = runtime.GetInstance(i).m_parentId;
			const float s = abs(scales[i] * sizes[parentId] * avg);
			extents[i] += (m_box.x * s + 1.0f) * (m_box.y * s + 1.0f) * (m_box.z * s + 1.0f);
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

		for (auto particleId : runtime.SpawnedRange())
		{
			const TParticleId parentId = parentIds.Load(particleId);
			const float scale = scales.SafeLoad(particleId);
			const Vec3 wPosition0 = positions.Load(particleId);
			const Quat wQuat = parentQuats.SafeLoad(parentId);
			const Vec3 oOffset = Vec3(
			  runtime.Chaos().RandSNorm() * m_box.x,
			  runtime.Chaos().RandSNorm() * m_box.y,
			  runtime.Chaos().RandSNorm() * m_box.z);
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
	Vec3                                 m_box = ZERO;
	CParamMod<EDD_PerParticle, UFloat10> m_scale;
};

CRY_PFX2_IMPLEMENT_FEATURE(CParticleFeature, CFeatureLocationBox, "Location", "Box", colorLocation);

//////////////////////////////////////////////////////////////////////////
// CFeatureLocationSphere

class CFeatureLocationSphere : public CParticleFeature
{
public:
	CRY_PFX2_DECLARE_FEATURE

	CFeatureLocationSphere()
		: m_radius(0.0f)
		, m_velocity(0.0f)
		, m_axisScale(1.0f, 1.0f, 1.0f)
	{}

	virtual void AddToComponent(CParticleComponent* pComponent, SComponentParams* pParams) override
	{
		pComponent->InitParticles.add(this);
		pComponent->GetSpatialExtents.add(this);
		pComponent->UpdateGPUParams.add(this);
		m_radius.AddToComponent(pComponent, this);
		m_velocity.AddToComponent(pComponent, this);
	}

	virtual void Serialize(Serialization::IArchive& ar) override
	{
		CParticleFeature::Serialize(ar);
		ar(m_radius, "Radius", "Radius");
		ar(m_velocity, "Velocity", "Velocity");
		ar(m_axisScale, "AxisScale", "Axis Scale");
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

	virtual void GetSpatialExtents(const CParticleComponentRuntime& runtime, TConstArray<float> scales, TVarArray<float> extents) override
	{
		uint numInstances = runtime.GetNumInstances();
		TFloatArray sizes(runtime.MemHeap(), runtime.GetParentContainer().GetMaxParticles());
		auto modRange = m_radius.GetValues(runtime, sizes, EDD_InstanceUpdate);

		for (uint i = 0; i < numInstances; ++i)
		{
			// Increase each dimension by 1 to include sphere bounds; works properly for spheres and circles
			const TParticleId parentId = runtime.GetInstance(i).m_parentId;
			const Vec3 axisMax = m_axisScale * abs(scales[i] * sizes[parentId] * modRange.end) + Vec3(1.0f),
			           axisMin = m_axisScale * abs(scales[i] * sizes[parentId] * modRange.start);
			const float v = (axisMax.x * axisMax.y * axisMax.z - axisMin.x * axisMin.y * axisMin.z) * (gf_PI * 4.0f / 3.0f);
			extents[i] += v;
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

		for (auto particleId : runtime.SpawnedRange())
		{
			const Vec3 sphere = runtime.Chaos().RandSphere();
			const Vec3 sphereDist = sphere.CompMul(m_axisScale);

			if (UseRadius)
			{
				const float radius = radii.SafeLoad(particleId);
				const Vec3 wPosition0 = positions.Load(particleId);
				const Vec3 wPosition1 = wPosition0 + sphereDist * radius;
				positions.Store(particleId, wPosition1);
			}

			if (UseVelocity)
			{
				const float velocityMult = velocityMults.SafeLoad(particleId);
				const Vec3 wVelocity0 = velocities.Load(particleId);
				const Vec3 wVelocity1 = wVelocity0 + sphereDist * velocityMult;
				velocities.Store(particleId, wVelocity1);
			}
		}
	}

	CParamMod<EDD_PerParticle, UFloat10> m_radius;
	CParamMod<EDD_PerParticle, SFloat10> m_velocity;
	Vec3                                 m_axisScale;
};

CRY_PFX2_IMPLEMENT_FEATURE(CParticleFeature, CFeatureLocationSphere, "Location", "Sphere", colorLocation);

//////////////////////////////////////////////////////////////////////////
// CFeatureLocationDisc

class CFeatureLocationCircle : public CParticleFeature
{
public:
	CRY_PFX2_DECLARE_FEATURE

	CFeatureLocationCircle()
		: m_radius(0.0f)
		, m_velocity(0.0f)
		, m_axisScale(1.0f, 1.0f)
		, m_axis(0.0f, 0.0f, 1.0f)
	{}

	virtual void AddToComponent(CParticleComponent* pComponent, SComponentParams* pParams) override
	{
		pComponent->InitParticles.add(this);
		pComponent->GetSpatialExtents.add(this);
		pComponent->UpdateGPUParams.add(this);
		m_radius.AddToComponent(pComponent, this);
		m_velocity.AddToComponent(pComponent, this);
	}

	virtual void Serialize(Serialization::IArchive& ar) override
	{
		CParticleFeature::Serialize(ar);
		ar(m_radius, "Radius", "Radius");
		ar(m_velocity, "Velocity", "Velocity");
		ar(m_axisScale, "AxisScale", "Axis Scale");
		ar(m_axis, "Axis", "Axis");
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

	virtual void GetSpatialExtents(const CParticleComponentRuntime& runtime, TConstArray<float> scales, TVarArray<float> extents) override
	{
		const uint numInstances = runtime.GetNumInstances();
		TFloatArray sizes(runtime.MemHeap(), runtime.GetParentContainer().GetMaxParticles()); 
		auto modRange = m_radius.GetValues(runtime, sizes, EDD_InstanceUpdate);

		for (uint i = 0; i < numInstances; ++i)
		{
			const TParticleId parentId = runtime.GetInstance(i).m_parentId;
			const Vec2 axisMax = m_axisScale * abs(scales[i] * sizes[parentId] * modRange.end) + Vec2(1.0f),
			           axisMin = m_axisScale * abs(scales[i] * sizes[parentId] * modRange.start);
			const float v = (axisMax.x * axisMax.y - axisMin.x * axisMin.y) * gf_PI;
			extents[i] += v;
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

		for (auto particleId : runtime.SpawnedRange())
		{
			TParticleId parentId = parentIds.Load(particleId);
			const Quat wQuat = parentQuats.SafeLoad(parentId);

			const Vec2 disc2 = runtime.Chaos().RandCircle();
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
	CParamMod<EDD_PerParticle, UFloat10> m_radius;
	CParamMod<EDD_PerParticle, SFloat10> m_velocity;
	Vec3                                 m_axis;
	Vec2                                 m_axisScale;
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
		pComponent->GetSpatialExtents.add(this);
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

	virtual void GetSpatialExtents(const CParticleComponentRuntime& runtime, TConstArray<float> scales, TVarArray<float> extents) override
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
		const TIStream<IMeshObj*> parentMeshes = runtime.GetParentContainer().IStream(EPDT_MeshGeometry, +emitterGeometry.m_pMeshObj);
		const TIStream<IPhysicalEntity*> parentPhysics = runtime.GetParentContainer().IStream(EPDT_PhysicalEntity);

		uint numInstances = runtime.GetNumInstances();
		for (uint i = 0; i < numInstances; ++i)
		{
			if (pParentComponent)
			{
				TParticleId parentId = runtime.GetInstance(i).m_parentId;
				if (IMeshObj* mesh = parentMeshes.Load(parentId))
					emitterGeometry.Set(mesh);
				if (m_source == EGeometrySource::Physics)
				{
					if (IPhysicalEntity* pPhysics = parentPhysics.Load(parentId))
						emitterGeometry.Set(pPhysics);
				}
			}
			float extent = emitterGeometry.GetExtent((EGeomType)m_source, (EGeomForm)m_location);
			for (int dim = (int)m_location; dim > 0; --dim)
				extent *= scales[i];
			extents[i] += extent;
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

		auto spawnRange = container.GetSpawnedRange();
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

	EGeometrySource                      m_source          = EGeometrySource::Render;
	EGeometryLocation                    m_location        = EGeometryLocation::Surface;
	CParamMod<EDD_PerParticle, SFloat10> m_offset          = 0;
	CParamMod<EDD_PerParticle, SFloat10> m_velocity        = 0;
	bool                                 m_orientToNormal  = true;
	bool                                 m_augmentLocation = false;
};

CRY_PFX2_IMPLEMENT_FEATURE(CParticleFeature, CFeatureLocationGeometry, "Location", "Geometry", colorLocation);

//////////////////////////////////////////////////////////////////////////
// CFeatureLocationNoise

class CFeatureLocationNoise : public CParticleFeature
{
private:
	typedef TValue<uint, THardLimits<1, 6>> UIntOctaves;

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
	ILINE static float SNoise(const Vec4 v)
	{
		return ::SNoise(Vec4f(v));
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

	CParamMod<EDD_PerParticle, SFloat10> m_amplitude;
	UFloat10                             m_size;
	UFloat10                             m_rate;
	UIntOctaves                          m_octaves;
};

CRY_PFX2_IMPLEMENT_FEATURE(CParticleFeature, CFeatureLocationNoise, "Location", "Noise", colorLocation);

//////////////////////////////////////////////////////////////////////////
// CFeatureLocationBeam

class CFeatureLocationBeam : public CParticleFeature
{
public:
	CRY_PFX2_DECLARE_FEATURE

	CFeatureLocationBeam()
		: m_source(ETargetSource::Parent)
		, m_destination(ETargetSource::Target) {}

	virtual void AddToComponent(CParticleComponent* pComponent, SComponentParams* pParams) override
	{
		pComponent->InitParticles.add(this);
		pComponent->GetSpatialExtents.add(this);
		pComponent->AddParticleData(EPDT_SpawnFraction);
	}

	virtual void Serialize(Serialization::IArchive& ar) override
	{
		CParticleFeature::Serialize(ar);
		ar(m_source, "Source", "Source");
		if (ar.isInput() && GetVersion(ar) <= 5)
			ar(m_destination, "Destiny", "Destination");
		ar(m_destination, "Destination", "Destination");
	}

	virtual void InitParticles(CParticleComponentRuntime& runtime) override
	{
		CRY_PROFILE_FUNCTION(PROFILE_PARTICLE);

		CParticleContainer& parentContainer = runtime.GetParentContainer();
		CParticleContainer& container = runtime.GetContainer();
		const Quat defaultQuat = runtime.GetEmitter()->GetLocation().q;
		IPidStream parentIds = container.GetIPidStream(EPDT_ParentId);
		IQuatStream parentQuats = parentContainer.GetIQuatStream(EPQF_Orientation, defaultQuat);
		const IFStream fractions = container.GetIFStream(EPDT_SpawnFraction);
		IOVec3Stream positions = container.GetIOVec3Stream(EPVF_Position);

		for (auto particleId : runtime.SpawnedRange())
		{
			const Vec3 wSource = m_source.GetTarget(runtime, particleId);
			const Vec3 wDestination = m_destination.GetTarget(runtime, particleId);
			const float fraction = fractions.SafeLoad(particleId);
			const Vec3 wPosition = wSource + (wDestination - wSource) * fraction;
			positions.Store(particleId, wPosition);
		}
	}

	virtual void GetSpatialExtents(const CParticleComponentRuntime& runtime, TConstArray<float> scales, TVarArray<float> extents) override
	{
		uint numInstances = runtime.GetNumInstances();
		for (uint i = 0; i < numInstances; ++i)
		{
			TParticleId parentId = runtime.GetInstance(i).m_parentId;
			const Vec3 wSource = m_source.GetTarget(runtime, parentId, true);
			const Vec3 wDestination = m_destination.GetTarget(runtime, parentId, true);
			extents[i] += (wSource - wDestination).GetLengthFast() * scales[i];
		}
	}

private:
	CTargetSource m_source;
	CTargetSource m_destination;
};

CRY_PFX2_IMPLEMENT_FEATURE(CParticleFeature, CFeatureLocationBeam, "Location", "Beam", colorLocation);

//////////////////////////////////////////////////////////////////////////
// CFeatureLocationcamera

class CFeatureLocationBindToCamera : public CParticleFeature
{
public:
	CRY_PFX2_DECLARE_FEATURE

	CFeatureLocationBindToCamera()
		: m_spawnOnly(false) {}

	virtual void AddToComponent(CParticleComponent* pComponent, SComponentParams* pParams) override
	{
		pComponent->PostInitParticles.add(this);
		if (!m_spawnOnly)
			pComponent->PreUpdateParticles.add(this);
	}

	virtual void Serialize(Serialization::IArchive& ar) override
	{
		CParticleFeature::Serialize(ar);
		ar(m_spawnOnly, "SpawnOnly", "Spawn Only");
	}

	virtual void PostInitParticles(CParticleComponentRuntime& runtime) override
	{
		CRY_PFX2_PROFILE_DETAIL;

		const CCamera& camera = gEnv->p3DEngine->GetRenderingCamera();
		const QuatT wCameraPose = QuatT(camera.GetMatrix());

		CParticleContainer& container = runtime.GetContainer();
		const CParticleContainer& parentContainer = runtime.GetParentContainer();
		const IPidStream parentIds = container.GetIPidStream(EPDT_ParentId);
		const IVec3Stream parentPositions = parentContainer.GetIVec3Stream(EPVF_Position);
		const IQuatStream parentOrientations = parentContainer.GetIQuatStream(EPQF_Orientation);
		IOVec3Stream positions = container.GetIOVec3Stream(EPVF_Position);
		IOVec3Stream velocities = container.GetIOVec3Stream(EPVF_Velocity);

		for (auto particleId : runtime.SpawnedRange())
		{
			const TParticleId parentId = parentIds.Load(particleId);
			const Vec3 wParentPosition = parentPositions.Load(parentId);
			const Quat wParentOrientation = parentOrientations.Load(parentId);
			const QuatT worldToParent = QuatT(wParentPosition, wParentOrientation).GetInverted();

			const Vec3 wPosition0 = positions.Load(particleId);
			const Vec3 wPosition1 = wCameraPose * (worldToParent * wPosition0);
			positions.Store(particleId, wPosition1);

			const Vec3 wVelocity0 = velocities.Load(particleId);
			const Vec3 wVelocity1 = wCameraPose.q * (worldToParent.q * wVelocity0);
			velocities.Store(particleId, wVelocity1);
		}
	}

	virtual void PreUpdateParticles(CParticleComponentRuntime& runtime) override
	{
		CRY_PFX2_PROFILE_DETAIL;

		const CCamera& camera = gEnv->p3DEngine->GetRenderingCamera();
		const QuatT wCurCameraPose = QuatT(camera.GetMatrix());
		const QuatT wPrevCameraPoseInv = GetPSystem()->GetLastCameraPose().GetInverted();
		const QuatT cameraMotion = GetPSystem()->GetCameraMotion();

		CParticleContainer& container = runtime.GetContainer();
		const CParticleContainer& parentContainer = runtime.GetParentContainer();
		const IPidStream parentIds = container.GetIPidStream(EPDT_ParentId);
		const IVec3Stream parentPositions = parentContainer.GetIVec3Stream(EPVF_Position);
		IOVec3Stream positions = container.GetIOVec3Stream(EPVF_Position);
		IOVec3Stream velocities = container.GetIOVec3Stream(EPVF_Velocity);

		for (auto particleId : container.GetNonSpawnedRange())
		{
			const Vec3 wPosition0 = positions.Load(particleId);
			const Vec3 cPosition0 = wPrevCameraPoseInv * wPosition0;
			const Vec3 wPosition1 = wCurCameraPose * cPosition0;
			positions.Store(particleId, wPosition1);

			const Vec3 wVelocity0 = velocities.Load(particleId);
			const Vec3 wVelocity1 = cameraMotion.q * wVelocity0;
			velocities.Store(particleId, wVelocity1);
		}
	}

private:
	bool m_spawnOnly;
};

CRY_PFX2_IMPLEMENT_FEATURE(CParticleFeature, CFeatureLocationBindToCamera, "Location", "BindToCamera", colorLocation);

//////////////////////////////////////////////////////////////////////////
// CFeatureLocationOmni

namespace
{
//
// Box
//
ILINE Vec3 RandomPosZBox(SChaosKey& chaosKey)
{
	return Vec3(chaosKey.RandSNorm(), chaosKey.RandSNorm(), chaosKey.RandUNorm());
}

ILINE bool WrapPosZBox(Vec3& pos)
{
	Vec3 pos0 = pos;
	pos.x -= std::floor(pos.x * 0.5f + 0.5f) * 2.0f;
	pos.y -= std::floor(pos.y * 0.5f + 0.5f) * 2.0f;
	pos.z -= std::floor(pos.z);
	return pos != pos0;
}

//
// Sector
//
ILINE bool InScreen(const Vec3& pos, Vec2 scrWidth, float epsilon = 0.0f)
{
	return abs(pos.x) <= pos.z * scrWidth.x + epsilon
	    && abs(pos.y) <= pos.z * scrWidth.y + epsilon;
}

ILINE bool InSector(const Vec3& pos, Vec2 scrWidth, float epsilon = 0.0f)
{
	return InScreen(pos, scrWidth, epsilon)
	       && pos.len2() <= 1.0f + epsilon * 2.0f;
}

ILINE Vec3 RandomSector(SChaosKey& chaosKey, Vec2 scrWidth)
{
	Vec3 pos(chaosKey.RandSNorm() * scrWidth.x, chaosKey.RandSNorm() * scrWidth.y, 1.0f);
	float r = pow(chaosKey.RandUNorm(), 0.333333f);
	pos *= r * pos.GetInvLengthFast();
	assert(InSector(pos, scrWidth, 0.001f));
	return pos;
}

void WrapSector(Vec3& pos, const Vec3& posPrev, Vec2 scrWidth)
{
	Vec3 delta = posPrev - pos;

	float maxMoveIn = 0.0f;
	float minMoveOut = std::numeric_limits<float>::max();

	// (P + D t) | N = 0
	// t = -P|N / D|N     ; N unnormalized
	auto GetPlaneDists = [&](float px, float py, float pz)
	{
		float dist = px * pos.x + py * pos.y + pz * pos.z;
		float dot = -(px * delta.x + py * delta.y + pz * delta.z);

		if (dist > 0.0f && dot > 0.0f && dist > dot * maxMoveIn)
			maxMoveIn = dist / dot;
		else if (dot < 0.0f && dist > dot * minMoveOut)
			minMoveOut = dist / dot;
	};

	GetPlaneDists(+1, 0, -scrWidth.x);
	GetPlaneDists(-1, 0, -scrWidth.x);
	GetPlaneDists(0, +1, -scrWidth.y);
	GetPlaneDists(0, -1, -scrWidth.y);

	// Wrap into sphere
	// r^2 = (P + D t)^2 = 1
	// P*P + P*D 2t + D*D t^2 = 1
	// r^2\t = 2 P*D 2 + 2 D*D t
	float dists[2];
	float dd = delta | delta, dp = delta | pos, pp = pos | pos;
	int n = solve_quadratic(dd, dp * 2.0f, pp - 1.0f, dists);
	while (--n >= 0)
	{
		float dr = dp + dd * dists[n];
		if (dr < 0.0f)
			maxMoveIn = max(maxMoveIn, dists[n]);
		else if (dists[n] > 0.0f)
			minMoveOut = min(minMoveOut, dists[n]);
	}

	if (maxMoveIn > 0.0f && maxMoveIn < minMoveOut)
	{
		const float moveDist = (minMoveOut - maxMoveIn) * trunc(minMoveOut / (minMoveOut - maxMoveIn));
		if (moveDist < 1e6f)
			pos += delta * moveDist;
	}
	assert(InSector(pos, scrWidth, 0.001f));
}

CRY_UNIT_TEST(WrapSectorTest)
{
	SChaosKey chaosKey(0u);
	for (int i = 0; i < 100; ++i)
	{
		Vec2 scrWidth(chaosKey.Rand(SChaosKey::Range(0.1f, 3.0f)), chaosKey.Rand(SChaosKey::Range(0.1f, 3.0f)));
		Vec3 pos = RandomSector(chaosKey, scrWidth);
		Vec3 pos2 = pos + chaosKey.RandSphere();
		if (!InSector(pos2, scrWidth))
		{
			Vec3 pos3 = pos2;
			WrapSector(pos3, pos, scrWidth);
		}
	}
};

ILINE int WrapRotation(Vec3& pos, Vec3& posPrev, const Matrix33& camRot, Vec2 scrWidth)
{
	Vec3 posRot = camRot * posPrev;
	if (InScreen(posRot, scrWidth))
		return false;

	if (abs(posRot.x) > posRot.z * scrWidth.x)
		posPrev.x = -posPrev.x;
	if (abs(posRot.y) > posRot.z * scrWidth.y)
		posPrev.y = -posPrev.y;

	pos += posPrev - posRot;
	assert(InScreen(pos, scrWidth, 0.001f));

	return true;
}

CRY_UNIT_TEST(WrapRotationTest)
{
	SChaosKey chaosKey(0u);
	for (int i = 0; i < 100; ++i)
	{
		Vec2 scrWidth(chaosKey.Rand(SChaosKey::Range(0.1f, 3.0f)), chaosKey.Rand(SChaosKey::Range(0.1f, 3.0f)));
		Vec3 pos = RandomSector(chaosKey, scrWidth);
		if (i % 4 == 0)
			pos.x = 0;
		else if (i % 4 == 1)
			pos.y = 0;
		AngleAxis rot;
		rot.axis = i % 4 == 0 ? Vec3(1, 0, 0) : i % 4 == 1 ? Vec3(0, 1, 0) : Vec3(chaosKey.RandCircle());
		rot.angle = (i+1) * 0.01f * gf_PI;
		Vec3 pos2 = AngleAxis(-rot.angle, rot.axis) * pos;
		if (!InScreen(pos2, scrWidth))
		{
			Vec3 pos1 = pos;
			Vec3 pos3 = pos2;
			WrapRotation(pos3, pos1, Matrix33::CreateRotationAA(-rot.angle, rot.axis), scrWidth);
		}
	}
};
}

MakeDataType(EPVF_AuxPosition, Vec3);

class CFeatureLocationOmni : public CParticleFeature
{
public:
	CRY_PFX2_DECLARE_FEATURE

	virtual void AddToComponent(CParticleComponent* pComponent, SComponentParams* pParams) override
	{
		pComponent->AddParticleData(EPVF_AuxPosition);
		pComponent->GetSpatialExtents.add(this);
		pComponent->InitParticles.add(this);
		pComponent->UpdateParticles.add(this);
		m_visibility.AddToComponent(pComponent, this);

		if (!m_useEmitterLocation)
		{
			// auto-set distance culling
			const float visibility = m_visibility.GetValueRange().end;
			auto& maxCamDistance = pParams->m_visibility.m_maxCameraDistance;
			if (visibility < maxCamDistance)
				maxCamDistance = visibility;
		}
	}

	virtual void Serialize(Serialization::IArchive& ar) override
	{
		CParticleFeature::Serialize(ar);
		SERIALIZE_VAR(ar, m_visibility);
	#ifndef _RELEASE
		SERIALIZE_VAR(ar, m_wrapSector);
		SERIALIZE_VAR(ar, m_wrapRotation);
		SERIALIZE_VAR(ar, m_wrapTranslation);
		SERIALIZE_VAR(ar, m_useEmitterLocation);
	#endif
	}

	virtual void GetSpatialExtents(const CParticleComponentRuntime& runtime, TConstArray<float> scales, TVarArray<float> extents) override
	{
		UpdateCameraData(runtime);
		const uint numInstances = runtime.GetNumInstances();
		for (uint i = 0; i < numInstances; ++i)
		{
			float scale = m_camData.maxDistance * scales[i];
			const Vec3 boxUnit(m_camData.scrWidth.x, m_camData.scrWidth.y, 1.0f);
			float extent;
			if (m_wrapSector)
			{
				float capHeight = 1.0f - rsqrt(boxUnit.len2());
				extent = (capHeight * scale) * sqr(scale + 1.0f) * 4.0f / 3.0f;
			}
			else
			{
				const Vec3 box = boxUnit * scale;
				extent = (box.x * 2.0f + 1.0f) * (box.y * 2.0f + 1.0f) * (box.z + 1.0f);
			}
			extents[i] += extent;
		}
	}

	virtual void InitParticles(CParticleComponentRuntime& runtime) override
	{
		CRY_PFX2_PROFILE_DETAIL;

		CParticleContainer& container = runtime.GetContainer();
		IOVec3Stream positions = container.GetIOVec3Stream(EPVF_Position);
		IOVec3Stream auxPositions = container.GetIOVec3Stream(EPVF_AuxPosition);

		UpdateCameraData(runtime);

		for (auto particleId : runtime.SpawnedRange())
		{
			// Overwrite position
			Vec3 pos;
			if (m_wrapSector)
			{
				pos = RandomSector(runtime.Chaos(), m_camData.scrWidth);
				auxPositions.Store(particleId, pos);
			}
			else
			{
				pos = RandomPosZBox(runtime.Chaos());
			}
			pos = m_camData.toWorld * pos;
			positions.Store(particleId, pos);
		}
	}

	virtual void UpdateParticles(CParticleComponentRuntime& runtime) override
	{
		CRY_PFX2_PROFILE_DETAIL;

		CParticleContainer& container = runtime.GetContainer();
		IOVec3Stream positions = container.GetIOVec3Stream(EPVF_Position);
		IOVec3Stream auxPositions = container.GetIOVec3Stream(EPVF_AuxPosition);

		UpdateCameraData(runtime);
		bool doCamRot = m_wrapRotation && m_camData.camRot.angle > 0.001f;
		Matrix33 camMat = Matrix33::CreateRotationAA(-m_camData.camRot.angle, m_camData.camRot.axis);

		// Wrap positions
		if (m_wrapSector)
		{
			for (auto particleId : runtime.FullRange())
			{
				Vec3 pos = positions.Load(particleId);
				Vec3 posCam = m_camData.fromWorld * pos;

				if (!InSector(posCam, m_camData.scrWidth))
				{
					Vec3 posCamPrev = auxPositions.Load(particleId);

					// Adjust for camera rotation
					if (doCamRot && WrapRotation(posCam, posCamPrev, camMat, m_camData.scrWidth))
					{
						if (m_wrapTranslation && !InSector(posCam, m_camData.scrWidth))
							WrapSector(posCam, posCamPrev, m_camData.scrWidth);
					}
					else if (m_wrapTranslation)
					{
						// Adjust for translation
						WrapSector(posCam, posCamPrev, m_camData.scrWidth);
					}

					assert(InSector(posCam, m_camData.scrWidth, 0.001f));
					pos = m_camData.toWorld * posCam;
					positions.Store(particleId, pos);
				}
				auxPositions.Store(particleId, posCam);
			}
		}
		else
		{
			if (m_wrapTranslation) for (auto particleId : runtime.FullRange())
			{
				Vec3 pos = positions.Load(particleId);
				Vec3 posCam = m_camData.fromWorld * pos;

				if (WrapPosZBox(posCam))
				{
					pos = m_camData.toWorld * posCam;
					positions.Store(particleId, pos);
				}
			}
		}
	}

private:

	// Camera data is shared per-effect, based on the global render camera and effect params
	struct SCameraData
	{
		int32     nFrameId    {-1};
		Vec2      scrWidth    {0};
		float     maxDistance {0};
		Matrix34  toWorld     {IDENTITY};
		Matrix34  fromWorld   {IDENTITY};
		AngleAxis camRot      {0, Vec3(0)};
	};
	SCameraData m_camData;

	void UpdateCameraData(const CParticleComponentRuntime& runtime)
	{
		if (gEnv->nMainFrameID == m_camData.nFrameId)
			return;
		CCamera cam = GetEffectCamera(runtime);
		m_camData.maxDistance = m_visibility.GetValueRange(runtime).end;
		m_camData.maxDistance = min(m_camData.maxDistance, +runtime.ComponentParams().m_visibility.m_maxCameraDistance);

		// Clamp view distance based on particle size
		const float maxParticleSize = runtime.ComponentParams().m_maxParticleSize * runtime.ComponentParams().m_visibility.m_viewDistanceMultiple;
		const float maxAngularDensity = GetPSystem()->GetMaxAngularDensity(cam);
		const float maxDistance = maxAngularDensity * maxParticleSize * runtime.GetEmitter()->GetViewDistRatio();
		if (maxDistance < m_camData.maxDistance)
			m_camData.maxDistance = maxDistance;

		m_camData.toWorld = UnitToWorld(cam, m_camData.maxDistance, runtime.ComponentParams().m_maxParticleSize);
		if (m_camData.nFrameId != -1)
			// Find rotation from last frame
			m_camData.camRot = Quat(m_camData.fromWorld * m_camData.toWorld);
		m_camData.fromWorld = m_camData.toWorld.GetInverted();
		m_camData.scrWidth = ScreenWidth(cam);
		m_camData.nFrameId = gEnv->nMainFrameID;
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

	Matrix34 UnitToWorld(const CCamera& cam, float range, float size) const
	{
		// Matrix rotates Z to Y, scales by range, and offsets backward by particle size
		Vec2 scrWidth = m_wrapSector ? Vec2(1) : ScreenWidth(cam);
		const Matrix34 toCam(
		  range * scrWidth.x, 0, 0, 0,
		  0, 0, range, -size,
		  0, -range * scrWidth.y, 0, 0
		  );

		// Concatenate with camera world location
		return cam.GetMatrix() * toCam;
	}

	static Vec2 ScreenWidth(const CCamera& cam)
	{
		Vec3 corner = cam.GetEdgeF();
		return Vec2(abs(corner.x), abs(corner.z)) / corner.y;
	}

	// Parameters
	CParamMod<EDD_InstanceUpdate, UFloat100> m_visibility;

	// Debugging and profiling options
	bool m_wrapSector         = true;
	bool m_wrapTranslation    = true;
	bool m_wrapRotation       = true;
	bool m_useEmitterLocation = false;
};

CRY_PFX2_IMPLEMENT_FEATURE(CParticleFeature, CFeatureLocationOmni, "Location", "Omni", colorLocation);

}
