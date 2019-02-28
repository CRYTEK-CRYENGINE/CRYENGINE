// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include <CryRenderer/IRenderer.h>
#include <CryRenderer/IShader.h>

#include <CrySchematyc/ResourceTypes.h>
#include <CrySchematyc/MathTypes.h>
#include <CrySchematyc/Env/IEnvRegistrar.h>

#include "DefaultComponents/ComponentHelpers/PhysicsParameters.h"

namespace Cry
{
namespace DefaultComponents
{
class CPhysicsPrimitiveComponent
	: public IEntityComponent
#ifndef RELEASE
	  , public IEntityComponentPreviewer
#endif
{
	// IEntityComponent
	virtual void Initialize() final
	{
		AddPrimitive();
	}

	virtual void   ProcessEvent(const SEntityEvent& event) final;

	virtual uint64 GetEventMask() const final
	{
		uint64 bitFlags = ENTITY_EVENT_BIT(ENTITY_EVENT_COMPONENT_PROPERTY_CHANGED);

		if (!isneg(m_physics.m_mass) || m_physics.m_density > 0)
		{
			bitFlags |= ENTITY_EVENT_BIT(ENTITY_EVENT_PHYSICAL_TYPE_CHANGED);
		}

		return bitFlags;
	}

	virtual void OnTransformChanged() final
	{
		if (IPhysicalEntity* pPhysicalEntity = m_pEntity->GetPhysicalEntity())
		{
			pe_params_part partParams;
			partParams.partid = m_pEntity->GetPhysicalEntityPartId0(GetEntitySlotId());
			if (pPhysicalEntity->GetParams(&partParams))
			{
				AddPrimitive();
			}
		}
	}

#ifndef RELEASE
	virtual IEntityComponentPreviewer* GetPreviewer() final { return this; }
#endif

	virtual void OnShutDown() final
	{
		if (IPhysicalEntity* pPhysicalEntity = m_pEntity->GetPhysicalEntity())
		{
			pPhysicalEntity->RemoveGeometry(m_pEntity->GetPhysicalEntityPartId0(GetEntitySlotId()));
		}
	}

	virtual void Serialize(Serialization::IArchive& archive) override
	{
		// We're only reading legacy data here, so no reason to continue if we're not reading.
		if (!archive.isInput())
		{
			return;
		}

		struct SProperties
		{
			void Serialize(Serialization::IArchive& archive)
			{
				const bool hasMass = archive(mass, "mass");
				const bool hasDensity = archive(density, "density");
				hasData = hasMass && hasDensity;
			}

			float mass;
			float density;
			bool hasData = false;
		};

		SProperties subData;
		const bool read = archive(subData, "properties") && subData.hasData;

		if (read)
		{
			float mass = subData.mass;
			float density = subData.density;
			m_physics.m_mass = mass;
			m_physics.m_density = density;

			if (mass <= 0 && density <= 0)
			{
				m_physics.m_weightType = SPhysicsParameters::EWeightType::Immovable;
			}
			else
			{
				m_physics.m_weightType = density > mass ? SPhysicsParameters::EWeightType::Density : SPhysicsParameters::EWeightType::Mass;
			}
		}
	}
	// ~IEntityComponent

#ifndef RELEASE
	// IEntityComponentPreviewer
	virtual void SerializeProperties(Serialization::IArchive& archive) final {}

	virtual void Render(const IEntity& entity, const IEntityComponent& component, SEntityPreviewContext& context) const override
	{
		if (context.bSelected)
		{
			Matrix34 slotTransform = GetWorldTransformMatrix();

			IGeometry* pPrimGeom = CreateGeometry();

			geom_world_data geomWorldData;
			geomWorldData.R = Matrix33(slotTransform);
			geomWorldData.scale = slotTransform.GetUniformScale();
			geomWorldData.offset = slotTransform.GetTranslation();

			gEnv->pSystem->GetIPhysRenderer()->DrawGeometry(pPrimGeom, &geomWorldData, -1, 0, ZERO, context.debugDrawInfo.color);

			pPrimGeom->Release();
		}
	}
	// ~IEntityComponentPreviewer
#endif

public:
	CPhysicsPrimitiveComponent() {}
	virtual ~CPhysicsPrimitiveComponent() = default;

	virtual std::unique_ptr<pe_geomparams> GetGeomParams() const { return stl::make_unique<pe_geomparams>(); }
	virtual IGeometry*                     CreateGeometry() const = 0;

	void                                   AddPrimitive();

public:
	Schematyc::SurfaceTypeName m_surfaceTypeName;
	bool                       m_bReactToCollisions = true;

protected:
	SPhysicsParameters m_physics;
};
}
}