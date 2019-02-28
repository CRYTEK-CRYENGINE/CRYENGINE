#include "StdAfx.h"
#include "AreaComponent.h"

#include "CharacterControllerComponent.h"
#include "Vehicles/VehicleComponent.h"
#include "RigidBodyComponent.h"

#include <Cry3DEngine/IRenderNode.h>

namespace Cry
{
namespace DefaultComponents
{
void CAreaComponent::Register(Schematyc::CEnvRegistrationScope& componentScope)
{
}

CAreaComponent::CAreaComponent()
	: m_shapeParameters{ this } 
	, m_type(CAreaComponent::EType::Box)
{
}

CAreaComponent::~CAreaComponent()
{
	SEntityPhysicalizeParams physParams;
	physParams.type = PE_NONE;
	m_pEntity->Physicalize(physParams);
}

void CAreaComponent::Initialize()
{
	Physicalize();
}

void CAreaComponent::SShapeParameters::Serialize(Serialization::IArchive& archive)
{
	switch (pComponent->GetType())
	{
		case CAreaComponent::EType::Box:
		{
			archive(size, "BoxSize", "Size");
		}
		break;
		case CAreaComponent::EType::Sphere:
		{
			archive(Serialization::Range(size.x, 0.f, 10000.f), "Radius", "Radius");
		}
		break;
		case CAreaComponent::EType::Cylinder:
		case CAreaComponent::EType::Capsule:
		{
			archive(Serialization::Range(size.x, 0.f, 10000.f), "Radius", "Radius");
			archive(Serialization::Range(size.y, 0.f, 10000.f), "Height", "Height");
		}
		break;
	}
}

void CAreaComponent::Physicalize()
{
	// Always dephysicalize first, otherwise set gravity won't be reset
	SEntityPhysicalizeParams physParams;
	physParams.type = PE_NONE;
	m_pEntity->Physicalize(physParams);

	IGeometry* pGeometry;

	switch (m_type)
	{
	case CAreaComponent::EType::Box:
	{
		// Now create the new area physics
		primitives::box primitive;
		primitive.size = m_shapeParameters.size;
		primitive.Basis = IDENTITY;
		primitive.center = Vec3(0, 0, primitive.size.z);
		primitive.bOriented = 1;

		pGeometry = gEnv->pPhysicalWorld->GetGeomManager()->CreatePrimitive(static_cast<int>(primitive.type), &primitive);
	}
	break;
	case CAreaComponent::EType::Sphere:
	{
		primitives::sphere primitive;
		primitive.center = ZERO;
		primitive.r = m_shapeParameters.size.x;

		pGeometry = gEnv->pPhysicalWorld->GetGeomManager()->CreatePrimitive(static_cast<int>(primitive.type), &primitive);
	}
	break;
	case CAreaComponent::EType::Cylinder:
	{
		primitives::cylinder primitive;
		primitive.center = ZERO;
		primitive.axis = Vec3(0, 0, 1);
		primitive.r = m_shapeParameters.size.x;
		primitive.hh = m_shapeParameters.size.y * 0.5f;

		pGeometry = gEnv->pPhysicalWorld->GetGeomManager()->CreatePrimitive(static_cast<int>(primitive.type), &primitive);
	}
	break;
	case CAreaComponent::EType::Capsule:
	{
		primitives::cylinder primitive;
		primitive.center = ZERO;
		primitive.axis = Vec3(0, 0, 1);
		primitive.r = m_shapeParameters.size.x;
		primitive.hh = m_shapeParameters.size.y * 0.5f;

		pGeometry = gEnv->pPhysicalWorld->GetGeomManager()->CreatePrimitive(static_cast<int>(primitive.type), &primitive);
	}
	break;
	default:
		CRY_ASSERT(false);
		return;
	}

	IPhysicalEntity* pPhysicalEntity = gEnv->pPhysicalWorld->AddArea(pGeometry, m_pEntity->GetWorldPos(), m_pEntity->GetWorldRotation(), m_pEntity->GetScale().GetLength());
	m_pEntity->AssignPhysicalEntity(pPhysicalEntity);

	// Set gravity params
	if (m_bCustomGravity)
	{
		pe_params_area areaParams;
		areaParams.gravity = m_gravity;
		pPhysicalEntity->SetParams(&areaParams);
	}

	// Set air / water params
	if (m_buoyancyParameters.medium != SBuoyancyParameters::EType::None)
	{
		pe_params_buoyancy buoyancyParams;
		buoyancyParams.iMedium = (pe_params_buoyancy::EMediumType)m_buoyancyParameters.medium;
		buoyancyParams.waterFlow = m_buoyancyParameters.flow;
		buoyancyParams.flowVariance = 0.f;
		buoyancyParams.waterDensity = m_buoyancyParameters.density;
		buoyancyParams.waterResistance = m_buoyancyParameters.resistance;
		buoyancyParams.waterPlane.n = Vec3(0, 0, -1);
		buoyancyParams.waterPlane.origin = Vec3(0, 0, gEnv->p3DEngine->GetWaterLevel());
		pPhysicalEntity->SetParams(&buoyancyParams);
	}
}

void CAreaComponent::ProcessEvent(const SEntityEvent& event)
{
	if (event.event == ENTITY_EVENT_COMPONENT_PROPERTY_CHANGED)
	{
		Physicalize();
	}
}

uint64 CAreaComponent::GetEventMask() const
{
	return ENTITY_EVENT_BIT(ENTITY_EVENT_COMPONENT_PROPERTY_CHANGED);
}

#ifndef RELEASE
void CAreaComponent::Render(const IEntity& entity, const IEntityComponent& component, SEntityPreviewContext &context) const
{
	if (context.bSelected)
	{
		IPhysicalEntity* pPhysicalEntity = m_pEntity->GetPhysicalEntity();
		if (pPhysicalEntity == nullptr)
		{
			return;
		}

		pe_status_pos pos;
		if (pPhysicalEntity->GetStatus(&pos) == 0)
		{
			return;
		}

		Vec3 samples[8][8][8];
		QuatT transform(pos.pos, pos.q);
		AABB bbox = AABB(pos.BBox[0], pos.BBox[1]);
		float frameTime = gEnv->pTimer->GetCurrTime();
		float theta = frameTime - floor(frameTime);

		float len[3] =
		{
			fabs_tpl(bbox.min.x - bbox.max.x),
			fabs_tpl(bbox.min.y - bbox.max.y),
			fabs_tpl(bbox.min.z - bbox.max.z)
		};

		pe_status_area area;
		for (size_t i = 0; i < 8; ++i)
		{
			for (size_t j = 0; j < 8; ++j)
			{
				for (size_t k = 0; k < 8; ++k)
				{
					area.ctr = transform * Vec3(
						bbox.min.x + i * (len[0] / 8.f)
						, bbox.min.y + j * (len[1] / 8.f)
						, bbox.min.z + k * (len[2] / 8.f));
					samples[i][j][k] = (pPhysicalEntity->GetStatus(&area)) ? area.pb.waterFlow : Vec3(0, 0, 0);
				}
			}
		}

		for (size_t i = 0; i < 8; ++i)
		{
			for (size_t j = 0; j < 8; ++j)
			{
				for (size_t k = 0; k < 8; ++k)
				{
					Vec3 ctr = transform * Vec3(
						bbox.min.x + i * (len[0] / 8.f),
						bbox.min.y + j * (len[1] / 8.f),
						bbox.min.z + k * (len[2] / 8.f));
					Vec3 dir = samples[i][j][k].GetNormalized();

					gEnv->pRenderer->GetIRenderAuxGeom()->DrawCone(ctr, dir, 0.03f, 0.03f, context.debugDrawInfo.color);
				}
			}
		}
	}
}
#endif
}
}
