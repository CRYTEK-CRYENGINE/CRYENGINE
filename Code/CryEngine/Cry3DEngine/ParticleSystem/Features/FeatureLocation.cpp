// Copyright 2001-2017 Crytek GmbH / Crytek Group. All rights reserved. 

// -------------------------------------------------------------------------
//  Created:     29/09/2014 by Filipe amim
//  Description:
// -------------------------------------------------------------------------
//
////////////////////////////////////////////////////////////////////////////

#include "StdAfx.h"
#include <CrySystem/CryUnitTest.h>
#include <CryMath/SNoise.h>
#include <CrySerialization/Math.h>
#include "ParticleSystem/ParticleFeature.h"
#include "ParticleSystem/ParticleEmitter.h"
#include "ParamMod.h"
#include "Target.h"

CRY_PFX2_DBG

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
		, CParticleFeature(gpu_pfx2::eGpuFeatureType_LocationOffset)
	{}

	virtual void AddToComponent(CParticleComponent* pComponent, SComponentParams* pParams) override
	{
		pComponent->AddToUpdateList(EUL_GetExtents, this);
		pComponent->AddToUpdateList(EUL_GetEmitOffset, this);
		pComponent->AddToUpdateList(EUL_InitUpdate, this);
		m_scale.AddToComponent(pComponent, this);

		if (auto pInt = GetGpuInterface())
		{
			gpu_pfx2::SFeatureParametersLocationOffset params;
			params.offset = m_offset;
			params.scale = m_scale.GetBaseValue();
			pInt->SetParameters(params);
		}
	}

	virtual void Serialize(Serialization::IArchive& ar) override
	{
		CParticleFeature::Serialize(ar);
		ar(m_offset, "Offset", "Offset");
		ar(m_scale, "Scale", "Scale");
	}

	virtual void GetSpatialExtents(const SUpdateContext& context, TConstArray<float> scales, TVarArray<float> extents) override
	{
		if (!m_scale.HasModifiers())
			return;

		TFloatArray sizes(*context.m_pMemHeap, context.m_parentContainer.GetLastParticleId());
		auto modRange = m_scale.GetValues(context, sizes, EMD_PerInstance, true);

		const size_t numInstances = context.m_runtime.GetNumInstances();
		for (size_t i = 0; i < numInstances; ++i)
		{
			const TParticleId parentId = context.m_runtime.GetInstance(i).m_parentId;
			float e = abs(sizes[parentId]) * modRange.Length();
			e = e * scales[i] + 1.0f;
			extents[i] += e;
		}
	}

	virtual Vec3 GetEmitOffset(const SUpdateContext& context, TParticleId parentId) override
	{
		TFloatArray scales(*context.m_pMemHeap, parentId + 1);
		const SUpdateRange range(parentId, parentId + 1);

		auto modRange = m_scale.GetValues(context, scales.data(), range, EMD_PerInstance, true);
		const float scale = scales[parentId] * (modRange.start + modRange.end) * 0.5f;
		return m_offset * scale;
	}

	virtual void InitParticles(const SUpdateContext& context) override
	{
		CRY_PROFILE_FUNCTION(PROFILE_PARTICLE);

		CParticleContainer& parentContainer = context.m_parentContainer;
		CParticleContainer& container = context.m_container;
		const Quat defaultQuat = context.m_runtime.GetEmitter()->GetLocation().q;
		IPidStream parentIds = container.GetIPidStream(EPDT_ParentId);
		IQuatStream parentQuats = parentContainer.GetIQuatStream(EPQF_Orientation, defaultQuat);
		IOVec3Stream positions = container.GetIOVec3Stream(EPVF_Position);
		STempModBuffer scales(context, m_scale);
		scales.ModifyInit(context, m_scale, container.GetSpawnedRange());

		Vec3 oOffset = m_offset;
		CRY_PFX2_FOR_SPAWNED_PARTICLES(context)
		{
			const TParticleId parentId = parentIds.Load(particleId);
			const float scale = scales.m_stream.SafeLoad(particleId);
			const Vec3 wPosition0 = positions.Load(particleId);
			const Quat wQuat = parentQuats.SafeLoad(parentId);
			const Vec3 wOffset = wQuat * oOffset;
			const Vec3 wPosition1 = wPosition0 + wOffset * scale;
			positions.Store(particleId, wPosition1);
		}
		CRY_PFX2_FOR_END;
	}

private:
	Vec3 m_offset;
	CParamMod<SModParticleSpawnInit, UFloat10> m_scale;
};

CRY_PFX2_IMPLEMENT_FEATURE(CParticleFeature, CFeatureLocationOffset, "Location", "Offset", colorLocation);

//////////////////////////////////////////////////////////////////////////
// CFeatureLocationBox

class CFeatureLocationBox : public CParticleFeature
{
public:
	CRY_PFX2_DECLARE_FEATURE

	CFeatureLocationBox()
		: m_box(ZERO)
		, CParticleFeature(gpu_pfx2::eGpuFeatureType_LocationBox)
	{}

	virtual void AddToComponent(CParticleComponent* pComponent, SComponentParams* pParams) override
	{
		pComponent->AddToUpdateList(EUL_InitUpdate, this);
		pComponent->AddToUpdateList(EUL_GetExtents, this);
		m_scale.AddToComponent(pComponent, this);
		if (auto pInt = GetGpuInterface())
		{
			gpu_pfx2::SFeatureParametersLocationBox params;
			params.box = m_box;
			params.scale = m_scale.GetBaseValue();
			pInt->SetParameters(params);
		}
	}

	virtual void Serialize(Serialization::IArchive& ar) override
	{
		CParticleFeature::Serialize(ar);
		ar(m_box, "Dimension", "Dimension");
		ar(m_scale, "Scale", "Scale");
	}

	virtual void GetSpatialExtents(const SUpdateContext& context, TConstArray<float> scales, TVarArray<float> extents) override
	{
		size_t numInstances = context.m_runtime.GetNumInstances();
		TFloatArray sizes(*context.m_pMemHeap, context.m_parentContainer.GetLastParticleId());
		auto modRange = m_scale.GetValues(context, sizes, EMD_PerInstance, true);
		float avg = (modRange.start + modRange.end) * 0.5f;

		for (size_t i = 0; i < numInstances; ++i)
		{
			// Increase each dimension by 1 to include boundaries; works properly for boxes, rects, and lines
			const TParticleId parentId = context.m_runtime.GetInstance(i).m_parentId;
			const float s = abs(scales[i] * sizes[parentId] * avg);
			extents[i] += (m_box.x * s + 1.0f) * (m_box.y * s + 1.0f) * (m_box.z * s + 1.0f);
		}
	}

	virtual void InitParticles(const SUpdateContext& context) override
	{
		CRY_PFX2_PROFILE_DETAIL;

		CParticleContainer& parentContainer = context.m_parentContainer;
		CParticleContainer& container = context.m_container;
		const Quat defaultQuat = context.m_runtime.GetEmitter()->GetLocation().q;
		IPidStream parentIds = container.GetIPidStream(EPDT_ParentId);
		IQuatStream parentQuats = parentContainer.GetIQuatStream(EPQF_Orientation, defaultQuat);
		IOVec3Stream positions = container.GetIOVec3Stream(EPVF_Position);
		STempModBuffer scales(context, m_scale);
		scales.ModifyInit(context, m_scale, container.GetSpawnedRange());

		CRY_PFX2_FOR_SPAWNED_PARTICLES(context)
		{
			const TParticleId parentId = parentIds.Load(particleId);
			const float scale = scales.m_stream.SafeLoad(particleId);
			const Vec3 wPosition0 = positions.Load(particleId);
			const Quat wQuat = parentQuats.SafeLoad(parentId);
			const Vec3 oOffset = Vec3(
			  context.m_spawnRng.RandSNorm() * m_box.x,
			  context.m_spawnRng.RandSNorm() * m_box.y,
			  context.m_spawnRng.RandSNorm() * m_box.z);
			const Vec3 wOffset = wQuat * oOffset;
			const Vec3 wPosition1 = wPosition0 + wOffset * scale;
			positions.Store(particleId, wPosition1);
		}
		CRY_PFX2_FOR_END;
	}

private:
	Vec3 m_box;
	CParamMod<SModParticleSpawnInit, UFloat10> m_scale;
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
		, CParticleFeature(gpu_pfx2::eGpuFeatureType_LocationSphere)
	{}

	virtual void AddToComponent(CParticleComponent* pComponent, SComponentParams* pParams) override
	{
		pComponent->AddToUpdateList(EUL_InitUpdate, this);
		pComponent->AddToUpdateList(EUL_GetExtents, this);
		m_radius.AddToComponent(pComponent, this);
		m_velocity.AddToComponent(pComponent, this);
		if (auto pInt = GetGpuInterface())
		{
			gpu_pfx2::SFeatureParametersLocationSphere params;
			params.scale = m_axisScale;
			params.radius = m_radius.GetBaseValue();
			params.velocity = m_velocity.GetBaseValue();
			pInt->SetParameters(params);
		}
	}

	virtual void Serialize(Serialization::IArchive& ar) override
	{
		CParticleFeature::Serialize(ar);
		ar(m_radius, "Radius", "Radius");
		ar(m_velocity, "Velocity", "Velocity");
		ar(m_axisScale, "AxisScale", "Axis Scale");
	}

	virtual void InitParticles(const SUpdateContext& context) override
	{
		CRY_PFX2_PROFILE_DETAIL;

		const float EPSILON = 1.0f / 2048.0f;
		const bool useRadius = m_radius.GetBaseValue() > EPSILON;
		const bool useVelocity = abs(m_velocity.GetBaseValue()) > EPSILON;

		if (useRadius && useVelocity)
			SphericalDist<true, true>(context);
		else if (useRadius)
			SphericalDist<true, false>(context);
		else if (useVelocity)
			SphericalDist<false, true>(context);
	}

	virtual void GetSpatialExtents(const SUpdateContext& context, TConstArray<float> scales, TVarArray<float> extents) override
	{
		size_t numInstances = context.m_runtime.GetNumInstances();
		TFloatArray sizes(*context.m_pMemHeap, context.m_parentContainer.GetLastParticleId());
		auto modRange = m_radius.GetValues(context, sizes, EMD_PerInstance, true);

		for (size_t i = 0; i < numInstances; ++i)
		{
			// Increase each dimension by 1 to include sphere bounds; works properly for spheres and circles
			const TParticleId parentId = context.m_runtime.GetInstance(i).m_parentId;
			const Vec3 axisMax = m_axisScale * abs(scales[i] * sizes[parentId] * modRange.end) + Vec3(1.0f),
			           axisMin = m_axisScale * abs(scales[i] * sizes[parentId] * modRange.start);
			const float v = (axisMax.x * axisMax.y * axisMax.z - axisMin.x * axisMin.y * axisMin.z) * (gf_PI * 4.0f / 3.0f);
			extents[i] += v;
		}
	}

private:
	template<const bool UseRadius, const bool UseVelocity>
	void SphericalDist(const SUpdateContext& context)
	{
		CParticleContainer& container = context.m_container;
		IOVec3Stream positions = container.GetIOVec3Stream(EPVF_Position);
		IOVec3Stream velocities = container.GetIOVec3Stream(EPVF_Velocity);
		const float baseRadius = m_radius.GetBaseValue();
		const float invBaseRadius = __fres(baseRadius);

		STempModBuffer radii(context, m_radius);
		STempModBuffer velocityMults(context, m_velocity);
		radii.ModifyInit(context, m_radius, container.GetSpawnedRange());
		velocityMults.ModifyInit(context, m_velocity, container.GetSpawnedRange());

		CRY_PFX2_FOR_SPAWNED_PARTICLES(context)
		{
			const Vec3 sphere = context.m_spawnRng.RandSphere();
			const Vec3 sphereDist = sphere.CompMul(m_axisScale);
			const float radiusMult = abs(radii.m_stream.SafeLoad(particleId));
			const float velocityMult = velocityMults.m_stream.SafeLoad(particleId);

			if (UseRadius)
			{
				const float radius = sqrt(radiusMult * invBaseRadius) * baseRadius;
				const Vec3 wPosition0 = positions.Load(particleId);
				const Vec3 wPosition1 = wPosition0 + sphereDist * radius;
				positions.Store(particleId, wPosition1);
			}

			if (UseVelocity)
			{
				const Vec3 wVelocity0 = velocities.Load(particleId);
				const Vec3 wVelocity1 = wVelocity0 + sphereDist * velocityMult;
				velocities.Store(particleId, wVelocity1);
			}
		}
		CRY_PFX2_FOR_END;
	}

	CParamMod<SModParticleSpawnInit, UFloat10> m_radius;
	CParamMod<SModParticleSpawnInit, SFloat10> m_velocity;
	Vec3 m_axisScale;
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
		, CParticleFeature(gpu_pfx2::eGpuFeatureType_LocationCircle)
	{}

	virtual void AddToComponent(CParticleComponent* pComponent, SComponentParams* pParams) override
	{
		pComponent->AddToUpdateList(EUL_InitUpdate, this);
		pComponent->AddToUpdateList(EUL_GetExtents, this);
		m_radius.AddToComponent(pComponent, this);
		m_velocity.AddToComponent(pComponent, this);
		if (auto pInt = GetGpuInterface())
		{
			gpu_pfx2::SFeatureParametersLocationCircle params;
			params.scale = m_axisScale;
			params.radius = m_radius.GetBaseValue();
			params.velocity = m_velocity.GetBaseValue();
			pInt->SetParameters(params);
		}
	}

	virtual void Serialize(Serialization::IArchive& ar) override
	{
		CParticleFeature::Serialize(ar);
		ar(m_radius, "Radius", "Radius");
		ar(m_velocity, "Velocity", "Velocity");
		ar(m_axisScale, "AxisScale", "Axis Scale");
		ar(m_axis, "Axis", "Axis");
	}

	virtual void InitParticles(const SUpdateContext& context) override
	{
		CRY_PFX2_PROFILE_DETAIL;

		const float EPSILON = 1.0f / 2048.0f;
		const bool useRadius = m_radius.GetBaseValue() > EPSILON;
		const bool useVelocity = abs(m_velocity.GetBaseValue()) > EPSILON;

		if (useRadius && useVelocity)
			CircularDist<true, true>(context);
		else if (useRadius)
			CircularDist<true, false>(context);
		else if (useVelocity)
			CircularDist<false, true>(context);
	}

	virtual void GetSpatialExtents(const SUpdateContext& context, TConstArray<float> scales, TVarArray<float> extents) override
	{
		const size_t numInstances = context.m_runtime.GetNumInstances();
		TFloatArray sizes(*context.m_pMemHeap, context.m_parentContainer.GetLastParticleId());
		auto modRange = m_radius.GetValues(context, sizes, EMD_PerInstance, true);

		for (size_t i = 0; i < numInstances; ++i)
		{
			const TParticleId parentId = context.m_runtime.GetInstance(i).m_parentId;
			const Vec2 axisMax = m_axisScale * abs(scales[i] * sizes[parentId] * modRange.end) + Vec2(1.0f),
			           axisMin = m_axisScale * abs(scales[i] * sizes[parentId] * modRange.start);
			const float v = (axisMax.x * axisMax.y - axisMin.x * axisMin.y) * gf_PI;
			extents[i] += v;
		}
	}

private:
	template<const bool UseRadius, const bool UseVelocity>
	void CircularDist(const SUpdateContext& context)
	{
		CParticleContainer& parentContainer = context.m_parentContainer;
		CParticleContainer& container = context.m_container;
		const Quat defaultQuat = context.m_runtime.GetEmitter()->GetLocation().q;
		const IPidStream parentIds = container.GetIPidStream(EPDT_ParentId);
		const IQuatStream parentQuats = parentContainer.GetIQuatStream(EPQF_Orientation, defaultQuat);
		const Quat axisQuat = Quat::CreateRotationV0V1(Vec3(0.0f, 0.0f, 1.0f), m_axis.GetNormalizedSafe());
		const float baseRadius = m_radius.GetBaseValue();
		const float invBaseRadius = __fres(baseRadius);
		IOVec3Stream positions = container.GetIOVec3Stream(EPVF_Position);
		IOVec3Stream velocities = container.GetIOVec3Stream(EPVF_Velocity);

		STempModBuffer radii(context, m_radius);
		STempModBuffer velocityMults(context, m_velocity);
		radii.ModifyInit(context, m_radius, container.GetSpawnedRange());
		velocityMults.ModifyInit(context, m_velocity, container.GetSpawnedRange());

		CRY_PFX2_FOR_SPAWNED_PARTICLES(context)
		{
			TParticleId parentId = parentIds.Load(particleId);
			const float radiusMult = abs(radii.m_stream.SafeLoad(particleId));
			const float velocityMult = velocityMults.m_stream.SafeLoad(particleId);
			const Quat wQuat = parentQuats.SafeLoad(parentId);

			const Vec2 disc2 = context.m_spawnRng.RandCircle();
			const Vec3 disc3 = axisQuat * Vec3(disc2.x * m_axisScale.x, disc2.y * m_axisScale.y, 0.0f);

			if (UseRadius)
			{
				const float radius = sqrt(radiusMult * invBaseRadius) * baseRadius;
				const Vec3 oPosition = disc3 * radius;
				const Vec3 wPosition0 = positions.Load(particleId);
				const Vec3 wPosition1 = wPosition0 + wQuat * oPosition;
				positions.Store(particleId, wPosition1);
			}

			if (UseVelocity)
			{
				const Vec3 wVelocity0 = velocities.Load(particleId);
				const Vec3 wVelocity1 = wVelocity0 + wQuat * disc3 * velocityMult;
				velocities.Store(particleId, wVelocity1);
			}
		}
		CRY_PFX2_FOR_END;
	}

private:
	CParamMod<SModParticleSpawnInit, UFloat10> m_radius;
	CParamMod<SModParticleSpawnInit, SFloat10> m_velocity;
	Vec3 m_axis;
	Vec2 m_axisScale;
};

CRY_PFX2_IMPLEMENT_FEATURE(CParticleFeature, CFeatureLocationCircle, "Location", "Circle", colorLocation);

//////////////////////////////////////////////////////////////////////////
// CFeatureLocationGeometry

extern EParticleDataType EPDT_MeshGeometry, EPDT_PhysicalEntity;

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

	CFeatureLocationGeometry()
		: m_offset(0.0f)
		, m_velocity(0.0f)
		, m_source(EGeometrySource::Render)
		, m_location(EGeometryLocation::Surface)
		, m_orientToNormal(false) {}

	virtual void AddToComponent(CParticleComponent* pComponent, SComponentParams* pParams) override
	{
		pComponent->AddToUpdateList(EUL_MainPreUpdate, this);
		pComponent->AddToUpdateList(EUL_GetExtents, this);
		m_offset.AddToComponent(pComponent, this);
		m_velocity.AddToComponent(pComponent, this);
		if (m_orientToNormal)
			pComponent->AddParticleData(EPQF_Orientation);

		if (CParticleComponent* pParentComponent = pComponent->GetParentComponent())
		{
			if (IMeshObj* pMesh = pParentComponent->GetComponentParams().m_pMesh)
				pMesh->GetExtent((EGeomForm)m_location);
		}
	}

	virtual void Serialize(Serialization::IArchive& ar) override
	{
		CParticleFeature::Serialize(ar);
		ar(m_source, "Source", "Source");
		ar(m_location, "Location", "Location");
		ar(m_offset, "Offset", "Offset");
		ar(m_velocity, "Velocity", "Velocity");
		ar(m_orientToNormal, "OrientParticles", "Orient Particles");
	}

	virtual void MainPreUpdate(CParticleComponentRuntime* pComponentRuntime) override
	{
		CRY_PROFILE_FUNCTION(PROFILE_PARTICLE);

		if (CParticleEmitter* pEmitter = pComponentRuntime->GetEmitter())
		{
			pEmitter->UpdateEmitGeomFromEntity();
			const GeomRef& emitterGeometry = pEmitter->GetEmitterGeometry();
			const EGeomType geomType = (EGeomType)m_source;
			const EGeomForm geomForm = (EGeomForm)m_location;
			emitterGeometry.GetExtent(geomType, geomForm);
		}
	}

	virtual void InitParticles(const SUpdateContext& context) override
	{
		CRY_PROFILE_FUNCTION(PROFILE_PARTICLE);

		const float EPSILON = 1.0f / 2048.0f;
		const bool useVelocity = abs(m_velocity.GetBaseValue()) > EPSILON;

		if (useVelocity)
			SampleGeometry<true>(context);
		else
			SampleGeometry<false>(context);
	}

	virtual void GetSpatialExtents(const SUpdateContext& context, TConstArray<float> scales, TVarArray<float> extents) override
	{
		if (CParticleEmitter* pEmitter = context.m_runtime.GetEmitter())
		{
			GeomRef emitterGeometry = pEmitter->GetEmitterGeometry();
			if (CParticleComponent* pParentComponent = context.m_runtime.GetComponent()->GetParentComponent())
			{
				if (IMeshObj* pMesh = pParentComponent->GetComponentParams().m_pMesh)
					emitterGeometry.Set(pMesh);
			}
			const bool hasParentParticles = context.m_params.m_parentId != gInvalidId;
			const TIStream<IMeshObj*> parentMeshes = context.m_parentContainer.GetTIStream<IMeshObj*>(EPDT_MeshGeometry, emitterGeometry.m_pMeshObj);
			const TIStream<IPhysicalEntity*> parentPhysics = context.m_parentContainer.GetTIStream<IPhysicalEntity*>(EPDT_PhysicalEntity);

			size_t numInstances = context.m_runtime.GetNumInstances();
			for (size_t i = 0; i < numInstances; ++i)
			{
				if (hasParentParticles)
				{
					TParticleId parentId = context.m_runtime.GetInstance(i).m_parentId;
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
	}

private:
	template<const bool UseVelocity>
	ILINE void SampleGeometry(const SUpdateContext& context)
	{
		if (m_orientToNormal)
			SampleGeometry<UseVelocity, true>(context);
		else
			SampleGeometry<UseVelocity, false>(context);
	}

	template<const bool UseVelocity, const bool OrientToNormal>
	void SampleGeometry(const SUpdateContext& context)
	{
		const CParticleEmitter* pEmitter = context.m_runtime.GetEmitter();
		const CParticleComponent* pParentComponent = context.m_runtime.GetComponent()->GetParentComponent();

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
		CParticleContainer& container = context.m_container;
		const CParticleContainer& parentContainer = context.m_parentContainer;
		const IPidStream parentIds = container.GetIPidStream(EPDT_ParentId);
		const IVec3Stream parentPositions = parentContainer.GetIVec3Stream(EPVF_Position, geomLocation.t);
		const IQuatStream parentQuats = parentContainer.GetIQuatStream(EPQF_Orientation, geomLocation.q);
		const IFStream parentSizes = parentContainer.GetIFStream(EPDT_Size, geomLocation.s);
		const TIStream<IMeshObj*> parentMeshes = parentContainer.GetTIStream<IMeshObj*>(EPDT_MeshGeometry, emitterGeometry.m_pMeshObj);
		const TIStream<IPhysicalEntity*> parentPhysics = parentContainer.GetTIStream<IPhysicalEntity*>(EPDT_PhysicalEntity);
		IOVec3Stream positions = container.GetIOVec3Stream(EPVF_Position);
		IOVec3Stream velocities = container.GetIOVec3Stream(EPVF_Velocity);
		IOQuatStream orientations = container.GetIOQuatStream(EPQF_Orientation);
		const bool hasParentParticles = context.m_params.m_parentId != gInvalidId;

		STempModBuffer offsets(context, m_offset);
		STempModBuffer velocityMults(context, m_velocity);
		offsets.ModifyInit(context, m_offset, container.GetSpawnedRange());
		velocityMults.ModifyInit(context, m_velocity, container.GetSpawnedRange());

		CRY_PFX2_FOR_SPAWNED_PARTICLES(context)
		{
			GeomRef particleGeometry = emitterGeometry;
			if (hasParentParticles)
			{
				TParticleId parentId = parentIds.Load(particleId);
				geomLocation.t = parentPositions.SafeLoad(parentId);
				geomLocation.q = parentQuats.SafeLoad(parentId);
				geomLocation.s = parentSizes.SafeLoad(parentId);
				if (IMeshObj* mesh = parentMeshes.SafeLoad(parentId))
					particleGeometry.Set(mesh);
				if (m_source == EGeometrySource::Physics)
				{
					if (IPhysicalEntity* pPhysics = parentPhysics.SafeLoad(parentId))
						particleGeometry.Set(pPhysics);
				}
			}

			CRndGen rng(context.m_spawnRng.Rand());
			PosNorm randPositionNormal;
			particleGeometry.GetRandomPos(
			  randPositionNormal, rng, geomType, geomForm,
			  geomLocation, geometryCentered);

			const float offset = offsets.m_stream.SafeLoad(particleId);
			const Vec3 wPosition = randPositionNormal.vPos + randPositionNormal.vNorm * offset;
			positions.Store(particleId, wPosition);

			if (UseVelocity)
			{
				const float velocityMult = velocityMults.m_stream.SafeLoad(particleId);
				const Vec3 wVelocity0 = velocities.Load(particleId);
				const Vec3 wVelocity1 = wVelocity0 + randPositionNormal.vNorm * velocityMult;
				velocities.Store(particleId, wVelocity1);
			}

			if (OrientToNormal)
			{
				const Quat wOrient0 = orientations.Load(particleId);
				const Quat oOrient = Quat::CreateRotationV0V1(randPositionNormal.vNorm, Vec3(0.0f, 0.0f, 1.0f));
				const Quat wOrient1 = wOrient0 * oOrient;
				orientations.Store(particleId, wOrient1);
			}
		}
		CRY_PFX2_FOR_END;
	}

	CParamMod<SModParticleSpawnInit, SFloat10> m_offset;
	CParamMod<SModParticleSpawnInit, SFloat10> m_velocity;
	EGeometrySource                            m_source;
	EGeometryLocation                          m_location;
	bool m_orientToNormal;
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
		, CParticleFeature(gpu_pfx2::eGpuFeatureType_LocationNoise) {}

	virtual void AddToComponent(CParticleComponent* pComponent, SComponentParams* pParams) override
	{
		pComponent->AddToUpdateList(EUL_InitUpdate, this);
		m_amplitude.AddToComponent(pComponent, this);
		if (auto pInt = GetGpuInterface())
		{
			gpu_pfx2::SFeatureParametersLocationNoise params;
			params.amplitude = m_amplitude.GetBaseValue();
			params.size = m_size;
			params.rate = m_rate;
			params.octaves = m_octaves;
			pInt->SetParameters(params);
		}
	}

	virtual void Serialize(Serialization::IArchive& ar) override
	{
		CParticleFeature::Serialize(ar);
		ar(m_amplitude, "Amplitude", "Amplitude");
		ar(m_size, "Size", "Size");
		ar(m_rate, "Rate", "Rate");
		ar(m_octaves, "Octaves", "Octaves");
	}

	virtual void InitParticles(const SUpdateContext& context) override
	{
		CRY_PROFILE_FUNCTION(PROFILE_PARTICLE);

		const float maxSize = (float)(1 << 12);
		const float minSize = rcp_fast(maxSize); // small enough and prevents SIMD exceptions
		const float time = mod(context.m_time * m_rate * minSize, 1.0f) * maxSize;
		const float invSize = rcp_fast(max(minSize, +m_size));
		const float delta = m_rate * context.m_deltaTime;
		CParticleContainer& container = context.m_container;
		const IFStream ages = container.GetIFStream(EPDT_NormalAge);
		IOVec3Stream positions = container.GetIOVec3Stream(EPVF_Position);

		STempModBuffer sizes(context, m_amplitude);
		sizes.ModifyInit(context, m_amplitude, container.GetSpawnedRange());

		CRY_PFX2_FOR_SPAWNED_PARTICLES(context)
		{
			const float amplitude = sizes.m_stream.SafeLoad(particleId);
			const Vec3 wPosition0 = positions.Load(particleId);
			const float age = ages.Load(particleId);
			Vec4 sample;
			sample.x = wPosition0.x * invSize;
			sample.y = wPosition0.y * invSize;
			sample.z = wPosition0.z * invSize;
			sample.w = StartTime(time, delta, age);
			const Vec3 potential = Fractal(sample, m_octaves);
			const Vec3 wPosition1 = potential * amplitude + wPosition0;
			positions.Store(particleId, wPosition1);
		}
		CRY_PFX2_FOR_END;
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
		mult = __fres(totalMult);
		float size = 1.0f;
		for (uint i = 0; i < octaves; ++i)
		{
			total += Potential(sample * size) * mult;
			size *= 2.0f;
			mult *= 0.5f;
		}
		return total;
	}

	CParamMod<SModParticleSpawnInit, SFloat10> m_amplitude;
	UFloat10    m_size;
	UFloat10    m_rate;
	UIntOctaves m_octaves;
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
		pComponent->AddToUpdateList(EUL_InitUpdate, this);
		pComponent->AddToUpdateList(EUL_GetExtents, this);
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

	virtual void InitParticles(const SUpdateContext& context) override
	{
		CRY_PROFILE_FUNCTION(PROFILE_PARTICLE);

		CParticleContainer& parentContainer = context.m_parentContainer;
		CParticleContainer& container = context.m_container;
		const Quat defaultQuat = context.m_runtime.GetEmitter()->GetLocation().q;
		IPidStream parentIds = container.GetIPidStream(EPDT_ParentId);
		IQuatStream parentQuats = parentContainer.GetIQuatStream(EPQF_Orientation, defaultQuat);
		const IFStream fractions = container.GetIFStream(EPDT_SpawnFraction);
		IOVec3Stream positions = container.GetIOVec3Stream(EPVF_Position);

		CRY_PFX2_FOR_SPAWNED_PARTICLES(context)
		{
			const Vec3 wSource = m_source.GetTarget(context, particleId);
			const Vec3 wDestination = m_destination.GetTarget(context, particleId);
			const float fraction = fractions.SafeLoad(particleId);
			const Vec3 wPosition = wSource + (wDestination - wSource) * fraction;
			positions.Store(particleId, wPosition);
		}
		CRY_PFX2_FOR_END;
	}

	virtual void GetSpatialExtents(const SUpdateContext& context, TConstArray<float> scales, TVarArray<float> extents) override
	{
		size_t numInstances = context.m_runtime.GetNumInstances();
		for (size_t i = 0; i < numInstances; ++i)
		{
			TParticleId parentId = context.m_runtime.GetInstance(i).m_parentId;
			const Vec3 wSource = m_source.GetTarget(context, parentId, true);
			const Vec3 wDestination = m_destination.GetTarget(context, parentId, true);
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
		pComponent->AddToUpdateList(EUL_PostInitUpdate, this);
		if (!m_spawnOnly)
			pComponent->AddToUpdateList(EUL_PreUpdate, this);
	}

	virtual void Serialize(Serialization::IArchive& ar) override
	{
		CParticleFeature::Serialize(ar);
		ar(m_spawnOnly, "SpawnOnly", "Spawn Only");
	}

	virtual void PostInitParticles(const SUpdateContext& context) override
	{
		CRY_PFX2_PROFILE_DETAIL;

		const CCamera& camera = gEnv->p3DEngine->GetRenderingCamera();
		const QuatT wCameraPose = QuatT(camera.GetMatrix());

		CParticleContainer& container = context.m_container;
		const CParticleContainer& parentContainer = context.m_parentContainer;
		const IPidStream parentIds = container.GetIPidStream(EPDT_ParentId);
		const IVec3Stream parentPositions = parentContainer.GetIVec3Stream(EPVF_Position);
		const IQuatStream parentOrientations = parentContainer.GetIQuatStream(EPQF_Orientation);
		IOVec3Stream positions = container.GetIOVec3Stream(EPVF_Position);
		IOVec3Stream velocities = container.GetIOVec3Stream(EPVF_Velocity);

		CRY_PFX2_FOR_SPAWNED_PARTICLES(context)
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
		CRY_PFX2_FOR_END;
	}

	virtual void PreUpdate(const SUpdateContext& context) override
	{
		CRY_PFX2_PROFILE_DETAIL;

		const CCamera& camera = gEnv->p3DEngine->GetRenderingCamera();
		const QuatT wCurCameraPose = QuatT(camera.GetMatrix());
		const QuatT wPrevCameraPoseInv = GetPSystem()->GetLastCameraPose().GetInverted();
		const QuatT cameraMotion = GetPSystem()->GetCameraMotion();

		CParticleContainer& container = context.m_container;
		const CParticleContainer& parentContainer = context.m_parentContainer;
		const IPidStream parentIds = container.GetIPidStream(EPDT_ParentId);
		const IVec3Stream parentPositions = parentContainer.GetIVec3Stream(EPVF_Position);
		const TIStream<uint8> states = container.GetTIStream<uint8>(EPDT_State);
		IOVec3Stream positions = container.GetIOVec3Stream(EPVF_Position);
		IOVec3Stream velocities = container.GetIOVec3Stream(EPVF_Velocity);

		CRY_PFX2_FOR_ACTIVE_PARTICLES(context)
		{
			const uint8 state = states.Load(particleId);
			if (state == ES_NewBorn)
				continue;

			const Vec3 wPosition0 = positions.Load(particleId);
			const Vec3 cPosition0 = wPrevCameraPoseInv * wPosition0;
			const Vec3 wPosition1 = wCurCameraPose * cPosition0;
			positions.Store(particleId, wPosition1);

			const Vec3 wVelocity0 = velocities.Load(particleId);
			const Vec3 wVelocity1 = cameraMotion.q * wVelocity0;
			velocities.Store(particleId, wVelocity1);
		}
		CRY_PFX2_FOR_END;
	}

private:
	bool m_spawnOnly;
};

CRY_PFX2_IMPLEMENT_FEATURE(CParticleFeature, CFeatureLocationBindToCamera, "Location", "BindToCamera", colorLocation);

//////////////////////////////////////////////////////////////////////////
// CFeatureLocationOmni

namespace
{
// Box
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

// Sector
ILINE bool InPosZSector(const Vec3& pos, Vec2 scrWidth, float epsilon = 0.0f)
{
	return abs(pos.x) <= pos.z * scrWidth.x + epsilon
	       && abs(pos.y) <= pos.z * scrWidth.y + epsilon;
}

ILINE bool InUnitZSector(const Vec3& pos, Vec2 scrWidth, float epsilon = 0.0f)
{
	return InPosZSector(pos, scrWidth, epsilon)
	       && pos.GetLengthSquared() <= 1.0f + epsilon * 2.0f;
}

ILINE Vec3 RandomUnitZSector(SChaosKey& chaosKey, Vec2 scrWidth)
{
	Vec3 pos(chaosKey.RandSNorm() * scrWidth.x, chaosKey.RandSNorm() * scrWidth.y, chaosKey.RandUNorm());
	float r = pow(pos.z, 0.333333f);
	pos.z = 1.0f;
	pos *= r * rsqrt_fast(pos.GetLengthSquared());
	assert(InUnitZSector(pos, scrWidth, 0.0001f));
	return pos;
}

void WrapUnitZSector(Vec3& pos, const Vec3& posPrev, Vec2 scrWidth)
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

	if (maxMoveIn > 0.0f)
	{
		const float moveDist = (minMoveOut - maxMoveIn) * trunc(minMoveOut / (minMoveOut - maxMoveIn));
		if (moveDist < 1e6f)
			pos += delta * moveDist;
	}
	assert(InUnitZSector(pos, scrWidth, 0.001f));
}

CRY_UNIT_TEST(WrapSectorTest)
{
	SChaosKey chaosKey(0u);
	for (int i = 0; i < 100; ++i)
	{
		Vec2 scrWidth(chaosKey.Rand(SChaosKey::Range(0.1f, 3.0f)), chaosKey.Rand(SChaosKey::Range(0.1f, 3.0f)));
		Vec3 pos = RandomUnitZSector(chaosKey, scrWidth);
		Vec3 pos2 = pos + chaosKey.RandSphere();
		if (!InUnitZSector(pos2, scrWidth))
		{
			Vec3 pos3 = pos2;
			WrapUnitZSector(pos3, pos, scrWidth);
		}
	}
};

template<int A>
ILINE void RotateZ(Vec3& pos, float s, float c)
{
	float z = c * pos.z + s * pos[A];
	pos[A] = c * pos[A] - s * pos.z;
	pos.z = z;
}

template<int A>
ILINE bool WrapRotation(Vec3& pos, Vec3& posPrev, Vec3& posRot, Vec2 scrWidth)
{
	if (abs(posRot[A]) > posRot.z * scrWidth[A])
	{
		float angScr = atan(scrWidth[A]);
		float ang = atan2(abs(posRot[A]), posRot.z);
		float angRot = (angScr + angScr) * trunc((angScr + ang) / (angScr + angScr)) * fsgnf(posRot[A]);
		float s, c;
		sincos(angRot, &s, &c);
		RotateZ<A>(posRot, s, c);
		posPrev = posRot;
		RotateZ<A>(pos, s, c);
		return true;
	}
	return false;
}

ILINE int WrapRotation(Vec3& pos, Vec3& posPrev, const Matrix33& camRot, Vec2 scrWidth)
{
	Vec3 posRot = camRot * posPrev;
	int wrapped = WrapRotation<0>(pos, posPrev, posRot, scrWidth)
	              + WrapRotation<1>(pos, posPrev, posRot, scrWidth);
	assert(InPosZSector(posRot, scrWidth, 0.001f));
	return wrapped;
}

CRY_UNIT_TEST(WrapRotationTest)
{
	SChaosKey chaosKey(0u);
	for (int i = 0; i < 100; ++i)
	{
		Vec2 scrWidth(chaosKey.Rand(SChaosKey::Range(0.1f, 3.0f)), chaosKey.Rand(SChaosKey::Range(0.1f, 3.0f)));
		Vec3 pos = RandomUnitZSector(chaosKey, scrWidth);
		if (i % 4 == 0)
			pos.x = 0;
		else if (i % 4 == 1)
			pos.y = 0;
		AngleAxis rot;
		rot.axis = i % 4 == 0 ? Vec3(1, 0, 0) : i % 4 == 1 ? Vec3(0, 1, 0) : Vec3(chaosKey.RandCircle());
		rot.angle = chaosKey.RandUNorm() * gf_PI;
		Vec3 pos2 = AngleAxis(-rot.angle, rot.axis) * pos;
		if (!InPosZSector(pos2, scrWidth))
		{
			Vec3 pos1 = pos;
			Vec3 pos3 = pos2;
			WrapRotation(pos3, pos1, Matrix33::CreateRotationAA(-rot.angle, rot.axis), scrWidth);
		}
	}
};
}

EParticleDataType PDT(EPVF_AuxPosition, float, 3);

class CFeatureLocationOmni : public CParticleFeature
{
public:
	CRY_PFX2_DECLARE_FEATURE

	virtual void AddToComponent(CParticleComponent* pComponent, SComponentParams* pParams) override
	{
		pComponent->AddParticleData(EPVF_AuxPosition);
		pComponent->AddToUpdateList(EUL_GetExtents, this);
		pComponent->AddToUpdateList(EUL_InitUpdate, this);
		pComponent->AddToUpdateList(EUL_Update, this);
		m_visibility.AddToComponent(pComponent, this);
	}

	virtual void Serialize(Serialization::IArchive& ar) override
	{
		CParticleFeature::Serialize(ar);
		SERIALIZE_VAR(ar, m_visibility);
		SERIALIZE_VAR(ar, m_wrapSector);
		SERIALIZE_VAR(ar, m_wrapRotation);
		SERIALIZE_VAR(ar, m_useEmitterLocation);
	}

	virtual void GetSpatialExtents(const SUpdateContext& context, TConstArray<float> scales, TVarArray<float> extents) override
	{
		UpdateCameraData(context);
		const float visibility = m_visibility.GetValueRange(context).end;
		const size_t numInstances = context.m_runtime.GetNumInstances();
		for (size_t i = 0; i < numInstances; ++i)
		{
			const Vec3 box = Vec3(m_camData.scrWidth.x, m_camData.scrWidth.y, 1.0f) * (visibility * scales[i]);
			float extent = (box.x * 2.0f + 1.0f) * (box.y * 2.0f + 1.0f) * (box.z + 1.0f);
			if (m_wrapSector)
				extent *= 0.25f;
			extents[i] += extent;
		}
	}

	virtual void InitParticles(const SUpdateContext& context) override
	{
		CRY_PFX2_PROFILE_DETAIL;

		CParticleContainer& container = context.m_container;
		CParticleComponentRuntime& runtime = context.m_runtime;

		IOVec3Stream positions = container.GetIOVec3Stream(EPVF_Position);
		IOVec3Stream auxPositions = container.GetIOVec3Stream(EPVF_AuxPosition);

		UpdateCameraData(context);

		CRY_PFX2_FOR_SPAWNED_PARTICLES(context)
		{
			// Overwrite position
			Vec3 pos;
			if (m_wrapSector)
			{
				pos = RandomUnitZSector(context.m_spawnRng, m_camData.scrWidth);
				auxPositions.Store(particleId, pos);   // in unit-camera space
			}
			else
			{
				pos = RandomPosZBox(context.m_spawnRng);
			}
			pos = m_camData.toWorld * pos;
			positions.Store(particleId, pos);
		}
		CRY_PFX2_FOR_END;
	}

	virtual void Update(const SUpdateContext& context) override
	{
		CRY_PFX2_PROFILE_DETAIL;

		CParticleContainer& container = context.m_container;
		CParticleComponentRuntime& runtime = context.m_runtime;

		IOVec3Stream positions = container.GetIOVec3Stream(EPVF_Position);
		IOVec3Stream auxPositions = container.GetIOVec3Stream(EPVF_AuxPosition);

		UpdateCameraData(context);
		bool doCamRot = m_wrapRotation && m_camData.camRot.angle > 0.001f;
		Matrix33 camMat = Matrix33::CreateRotationAA(-m_camData.camRot.angle, m_camData.camRot.axis);

		// Wrap positions
		if (m_wrapSector)
		{
			CRY_PFX2_FOR_ACTIVE_PARTICLES(context)
			{
				Vec3 pos = positions.Load(particleId);
				Vec3 posCam = m_camData.fromWorld * pos;

				if (!InUnitZSector(posCam, m_camData.scrWidth /*, 0.002f*/))
				{
					Vec3 posCamPrev = auxPositions.Load(particleId);

					// Adjust for camera rotation
					if (doCamRot && WrapRotation(posCam, posCamPrev, camMat, m_camData.scrWidth))
					{
						if (!InUnitZSector(posCam, m_camData.scrWidth /*, 0.002f*/))
							WrapUnitZSector(posCam, posCamPrev, m_camData.scrWidth);
					}
					else
					{
						// Adjust for translation
						WrapUnitZSector(posCam, posCamPrev, m_camData.scrWidth);
					}

					pos = m_camData.toWorld * posCam;
					positions.Store(particleId, pos);
				}
				auxPositions.Store(particleId, posCam);
			}
			CRY_PFX2_FOR_END;
		}
		else
		{
			CRY_PFX2_FOR_ACTIVE_PARTICLES(context)
			{
				Vec3 pos = positions.Load(particleId);
				Vec3 posCam = m_camData.fromWorld * pos;

				if (WrapPosZBox(posCam))
				{
					pos = m_camData.toWorld * posCam;
					positions.Store(particleId, pos);
				}
			}
			CRY_PFX2_FOR_END;
		}
	}

private:

	// Camera data is shared per-effect, based on the global render camera and effect params
	struct SCameraData
	{
		int32     nFrameId;
		Vec2      scrWidth;
		float     maxDistance;
		Matrix34  toWorld;
		Matrix34  fromWorld;
		AngleAxis camRot;

		SCameraData()
			: nFrameId(-1), camRot(0, Vec3(0)) {}
	};
	SCameraData m_camData;

	void UpdateCameraData(const SUpdateContext& context)
	{
		if (gEnv->nMainFrameID == m_camData.nFrameId)
			return;
		CCamera cam = GetEffectCamera(context);
		m_camData.maxDistance = m_visibility.GetValueRange(context).end;
		m_camData.maxDistance = min(m_camData.maxDistance, +context.m_params.m_visibility.m_maxCameraDistance);

		// Clamp view distance based on particle size
		const float maxParticleSize = context.m_params.m_maxParticleSize * context.m_params.m_visibility.m_viewDistanceMultiple;
		const float maxAngularDensity = GetPSystem()->GetMaxAngularDensity(cam);
		const float maxDistance = maxAngularDensity * maxParticleSize * context.m_runtime.GetEmitter()->GetViewDistRatio();
		if (maxDistance < m_camData.maxDistance)
			m_camData.maxDistance = maxDistance;

		m_camData.toWorld = UnitToWorld(cam, m_camData.maxDistance, context.m_params.m_maxParticleSize);
		if (m_camData.nFrameId != -1)
			// Find rotation from last frame
			m_camData.camRot = Quat(m_camData.fromWorld * m_camData.toWorld);
		m_camData.fromWorld = m_camData.toWorld.GetInverted();
		m_camData.scrWidth = ScreenWidth(cam);
		m_camData.nFrameId = gEnv->nMainFrameID;
	}

	CCamera GetEffectCamera(const SUpdateContext& context) const
	{
		if (!m_useEmitterLocation)
		{
			return gEnv->p3DEngine->GetRenderingCamera();
		}
		else
		{
			CCamera camera = gEnv->p3DEngine->GetRenderingCamera();
			const CParticleEmitter& emitter = *context.m_runtime.GetEmitter();
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
		return Vec2(abs(corner.x / corner.y), abs(corner.z / corner.y));
	}

	CParamMod<SModEffectField, UFloat100> m_visibility;

	// Debugging and profiling options
	bool m_wrapSector         = true;
	bool m_wrapRotation       = true;
	bool m_useEmitterLocation = false;
};

CRY_PFX2_IMPLEMENT_FEATURE(CParticleFeature, CFeatureLocationOmni, "Location", "Omni", colorLocation);

}
