// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include "StdAfx.h"
#include "PhysicsPrimitiveComponent.h"
#include <CrySerialization/Enum.h>

namespace Cry
{
	namespace DefaultComponents
	{
		void CPhysicsPrimitiveComponent::ProcessEvent(const SEntityEvent& event)
		{
			if (event.event == ENTITY_EVENT_COMPONENT_PROPERTY_CHANGED)
			{
				switch (m_physics.m_weightType)
				{
				case SPhysicsParameters::EWeightType::Mass:
					{
						m_physics.m_mass = max((float)m_physics.m_mass, SPhysicsParameters::s_weightTypeEpsilon);

					}
					break;
				case SPhysicsParameters::EWeightType::Density:
					{
						m_physics.m_density = max((float)m_physics.m_density, SPhysicsParameters::s_weightTypeEpsilon);
					}
					break;
				}

				m_pEntity->UpdateComponentEventMask(this);

				AddPrimitive();
			}
			else if (event.event == ENTITY_EVENT_PHYSICAL_TYPE_CHANGED)
			{
				AddPrimitive();
			}
		}

		void CPhysicsPrimitiveComponent::AddPrimitive()
		{
			if (IPhysicalEntity* pPhysicalEntity = m_pEntity->GetPhysicalEntity())
			{
				pPhysicalEntity->RemoveGeometry(m_pEntity->GetPhysicalEntityPartId0(GetEntitySlotId()));
								
				IGeometry* pPrimGeom = CreateGeometry();
				phys_geometry* pPhysGeom = gEnv->pPhysicalWorld->GetGeomManager()->RegisterGeometry(pPrimGeom, 0);

				std::unique_ptr<pe_geomparams> pGeomParams = GetGeomParams();

				switch (m_physics.m_weightType)
				{
				case SPhysicsParameters::EWeightType::Mass:
					{
						pGeomParams->mass = m_physics.m_mass;
						pGeomParams->density = -1.0f;
					}
					break;
				case SPhysicsParameters::EWeightType::Density:
					{
						pGeomParams->density = m_physics.m_density;
						pGeomParams->mass = -1.0f;
					}
					break;
				case SPhysicsParameters::EWeightType::Immovable:
					{
						pGeomParams->density = -1.0f;
						pGeomParams->mass = 0.0f;
					}
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
}