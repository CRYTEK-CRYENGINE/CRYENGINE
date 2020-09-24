#include "StdAfx.h"
#include "WheelComponent.h"
#include <Cry3DEngine/IRenderNode.h>

namespace Cry
{
	namespace DefaultComponents
	{
		void CWheelComponent::Initialize()
		{
			ApplyStatObj(true);
			CPhysicsPrimitiveComponent::Initialize();
		}

		void CWheelComponent::ApplyStatObj(bool onInit)
		{
			int slot = GetOrMakeEntitySlotId();
			if (!m_pStatObj || m_pStatObj->IsDefaultObject() || strcmp(m_pStatObj->GetFilePath(), m_filePath.value))
			{
				m_pStatObj = !m_filePath.value.empty() ? gEnv->p3DEngine->LoadStatObj(m_filePath.value) : nullptr;
				if (m_pStatObj && m_pStatObj->IsDefaultObject())
					m_pStatObj = nullptr;
				GetEntity()->SetStatObj(m_pStatObj, slot | ENTITY_SLOT_ACTUAL, false);

				phys_geometry *geom = m_pStatObj ? m_pStatObj->GetPhysGeom() : nullptr;
				if (geom)
				{
					m_center = geom->origin;
					m_axis.zero()[idxmax3(geom->Ibody)] = 1;
					m_axis = geom->q * m_axis;
					primitives::box bbox;
					geom->pGeom->GetBBox(&bbox);
					if (!onInit) 
					{
						m_height = (bbox.Basis * m_axis).abs() * bbox.size * 2;
						m_radius = (bbox.size.x + bbox.size.y + bbox.size.z - m_height * 0.5f) * 0.5f;
					}
				}
				else
				{
					m_center = Vec3(ZERO);
					m_axis = Vec3(1, 0, 0);
				}
			}
			GetEntity()->SetSlotFlags(slot, (GetEntity()->GetSlotFlags(slot) & ~ENTITY_SLOT_CAST_SHADOW) | (uint32)m_castShadows * ENTITY_SLOT_CAST_SHADOW);
			if (!m_pEntity->GetSlotMaterial(slot) != m_materialPath.value.IsEmpty() || 
				!m_materialPath.value.IsEmpty() && strcmp(m_pEntity->GetSlotMaterial(slot)->GetName(), m_materialPath.value.Left(m_materialPath.value.length() - 4)))
			{
				m_pEntity->SetSlotMaterial(slot, m_materialPath.value.IsEmpty() ? nullptr : gEnv->p3DEngine->GetMaterialManager()->LoadMaterial(m_materialPath.value, false));
			}
		}

		void CWheelComponent::ProcessEvent(const SEntityEvent& evt)
		{
			if (evt.event == ENTITY_EVENT_COMPONENT_PROPERTY_CHANGED)
			{
				ApplyStatObj();
			}
			CPhysicsPrimitiveComponent::ProcessEvent(evt);
		}

		void CWheelComponent::Register(Schematyc::CEnvRegistrationScope& componentScope)
		{
		}
	}
}