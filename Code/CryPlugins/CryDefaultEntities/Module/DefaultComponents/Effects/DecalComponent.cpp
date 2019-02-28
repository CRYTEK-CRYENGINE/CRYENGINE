#include "StdAfx.h"
#include "DecalComponent.h"

#include <Cry3DEngine/IRenderNode.h>

namespace Cry
{
namespace DefaultComponents
{
void CDecalComponent::Register(Schematyc::CEnvRegistrationScope& componentScope)
{
	{
		auto pFunction = SCHEMATYC_MAKE_ENV_FUNCTION(&CDecalComponent::Spawn, "{39ED7682-5C39-47EE-BEFF-0A39EB801EC2}"_cry_guid, "Spawn");
		pFunction->SetDescription("Spaws the decal");
		pFunction->SetFlags(Schematyc::EEnvFunctionFlags::Construction);
		componentScope.Register(pFunction);
	}
	{
		auto pFunction = SCHEMATYC_MAKE_ENV_FUNCTION(&CDecalComponent::Remove, "{7A31C5D3-51E2-4091-9A4D-A2459D37F67A}"_cry_guid, "Remove");
		pFunction->SetDescription("Removes the spawned decal, if any");
		pFunction->SetFlags(Schematyc::EEnvFunctionFlags::Construction);
		componentScope.Register(pFunction);
	}
}

void CDecalComponent::Initialize()
{
	if (m_bAutoSpawn)
	{
		Spawn();
	}
	else
	{
		Remove();
	}
}

void CDecalComponent::ProcessEvent(const SEntityEvent& event)
{
	switch (event.event)
	{
	case ENTITY_EVENT_XFORM:
		{
			Spawn();
		}
		break;
	case ENTITY_EVENT_COMPONENT_PROPERTY_CHANGED:
		{
			Spawn();
		}
		break;
	}
}

uint64 CDecalComponent::GetEventMask() const
{
	uint64 bitFlags = (m_bFollowEntityAfterSpawn && m_bSpawned) ? ENTITY_EVENT_BIT(ENTITY_EVENT_XFORM) : 0;
	bitFlags |= ENTITY_EVENT_BIT(ENTITY_EVENT_COMPONENT_PROPERTY_CHANGED);

	return bitFlags;
}

#ifndef RELEASE
void CDecalComponent::Render(const IEntity& entity, const IEntityComponent& component, SEntityPreviewContext& context) const
{
	if (context.bSelected)
	{
		const ColorB color = context.debugDrawInfo.color;

		const Matrix34& slotTransform = m_pEntity->GetSlotWorldTM(GetEntitySlotId());

		IRenderAuxGeom* pAuxRenderer = gEnv->pAuxGeomRenderer;

		const Vec3 x1(slotTransform.TransformPoint(Vec3(-1, 0, 0)));
		const Vec3 x2(slotTransform.TransformPoint(Vec3(1, 0, 0)));
		const Vec3 y1(slotTransform.TransformPoint(Vec3(0, -1, 0)));
		const Vec3 y2(slotTransform.TransformPoint(Vec3(0, 1, 0)));
		const Vec3 p(slotTransform.TransformPoint(Vec3(0, 0, 0)));
		const Vec3 n(slotTransform.TransformPoint(Vec3(0, 0, 1)));

		pAuxRenderer->DrawLine(p, color, n, color);
		pAuxRenderer->DrawLine(x1, color, x2, color);
		pAuxRenderer->DrawLine(y1, color, y2, color);

		const Vec3 p0 = slotTransform.TransformPoint(Vec3(-1, -1, 0));
		const Vec3 p1 = slotTransform.TransformPoint(Vec3(-1, 1, 0));
		const Vec3 p2 = slotTransform.TransformPoint(Vec3(1, 1, 0));
		const Vec3 p3 = slotTransform.TransformPoint(Vec3(1, -1, 0));
		pAuxRenderer->DrawLine(p0, color, p1, color);
		pAuxRenderer->DrawLine(p1, color, p2, color);
		pAuxRenderer->DrawLine(p2, color, p3, color);
		pAuxRenderer->DrawLine(p3, color, p0, color);
		pAuxRenderer->DrawLine(p0, color, p2, color);
		pAuxRenderer->DrawLine(p1, color, p3, color);
	}
}
#endif

void CDecalComponent::SetMaterialFileName(const char* szPath)
{
	m_materialFileName = szPath;
}
}
}
