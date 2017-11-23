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
	uint64 bitFlags = (m_bFollowEntityAfterSpawn && m_bSpawned) ? BIT64(ENTITY_EVENT_XFORM) : 0;
	if (m_bSpawned)
	{
		bitFlags |= BIT64(ENTITY_EVENT_COMPONENT_PROPERTY_CHANGED);
	}

	return bitFlags;
}

void CDecalComponent::SetMaterialFileName(const char* szPath)
{
	m_materialFileName = szPath;
}
}
}
