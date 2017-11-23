#pragma once

#include <CryRenderer/IRenderer.h>
#include <CryRenderer/IShader.h>

#include <CrySchematyc/ResourceTypes.h>
#include <CrySchematyc/MathTypes.h>
#include <CrySchematyc/Env/IEnvRegistrar.h>

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

			virtual void ProcessEvent(const SEntityEvent& event) final
			{
				if (event.event == ENTITY_EVENT_COMPONENT_PROPERTY_CHANGED)
				{
					// Validate designer inputs
#ifndef RELEASE
					// Reset mass or density to 0 in the UI if the other is changed to be positive.
					// It is not possible to use both at the same time, this makes that clearer for the designer.
					if (m_mass != m_prevMass && m_mass > 0)
					{
						m_density = m_prevDensity = 0;
						m_prevMass = m_mass;
					}
					if (m_density != m_prevDensity && m_density > 0)
					{
						m_mass = m_prevMass = 0;
						m_prevDensity = m_density;
					}
#endif

					m_pEntity->UpdateComponentEventMask(this);

					AddPrimitive();
				}
				else if (event.event == ENTITY_EVENT_PHYSICAL_TYPE_CHANGED)
				{
					AddPrimitive();
				}
			}

			virtual uint64 GetEventMask() const final
			{
				uint64 bitFlags = BIT64(ENTITY_EVENT_COMPONENT_PROPERTY_CHANGED);

				if (m_mass > 0 || m_density > 0)
				{
					bitFlags |= BIT64(ENTITY_EVENT_PHYSICAL_TYPE_CHANGED);
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
			// ~IEntityComponent

#ifndef RELEASE
			// IEntityComponentPreviewer
			virtual void SerializeProperties(Serialization::IArchive& archive) final {}

			virtual void Render(const IEntity& entity, const IEntityComponent& component, SEntityPreviewContext &context) const final
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
			virtual IGeometry* CreateGeometry() const = 0;

			void AddPrimitive()
			{
				if (IPhysicalEntity* pPhysicalEntity = m_pEntity->GetPhysicalEntity())
				{
					pPhysicalEntity->RemoveGeometry(m_pEntity->GetPhysicalEntityPartId0(GetEntitySlotId()));

					if (m_mass > 0 || m_density > 0)
					{
						IGeometry* pPrimGeom = CreateGeometry();
						phys_geometry* pPhysGeom = gEnv->pPhysicalWorld->GetGeomManager()->RegisterGeometry(pPrimGeom, 0);

						std::unique_ptr<pe_geomparams> pGeomParams = GetGeomParams();

						if (m_mass > 0)
						{
							pGeomParams->mass = m_mass;
						}
						else
						{
							pGeomParams->mass = -1.f;
						}

						if (m_density > 0)
						{
							pGeomParams->density = m_density;
						}
						else
						{
							pGeomParams->density = -1.f;
						}

						if (m_surfaceTypeName.value.size() > 0)
						{
							string surfaceType = "mat_" + m_surfaceTypeName.value;

							if (ISurfaceType* pSurfaceType = gEnv->p3DEngine->GetMaterialManager()->GetSurfaceTypeByName(surfaceType))
							{
								pGeomParams->surface_idx = pSurfaceType->GetId();
							}
							else
							{
								CryWarning(VALIDATOR_MODULE_GAME, VALIDATOR_ERROR, "Invalid surface type %s specified on primitive component in entity %s!", m_surfaceTypeName.value.c_str(), m_pEntity->GetName());
							}
						}

						Matrix34 slotTransform = m_pTransform->ToMatrix34();
						pGeomParams->pos = slotTransform.GetTranslation();
						pGeomParams->q = Quat(slotTransform);
						pGeomParams->scale = slotTransform.GetUniformScale();

						if (!m_bReactToCollisions)
						{
							pGeomParams->flags |= geom_no_coll_response;
						}

						pPhysicalEntity->AddGeometry(pPhysGeom, pGeomParams.get(), m_pEntity->GetPhysicalEntityPartId0(GetOrMakeEntitySlotId()));
					}
				}
			}

		public:
			Schematyc::PositiveFloat m_mass = 10.f;
			Schematyc::PositiveFloat m_density = 0.f;

			// Used to reset mass or density to -1 in the UI
#ifndef RELEASE
			float m_prevMass = 0.f;
			float m_prevDensity = 0.f;
#endif

			Schematyc::SurfaceTypeName m_surfaceTypeName;
			bool m_bReactToCollisions = true;
		};
	}
}