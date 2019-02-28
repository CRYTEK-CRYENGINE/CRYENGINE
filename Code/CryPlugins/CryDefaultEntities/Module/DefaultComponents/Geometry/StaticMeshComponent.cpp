#include "StdAfx.h"
#include "StaticMeshComponent.h"
#include "DefaultComponents/ComponentHelpers/PhysicsParameters.h"

#include <Cry3DEngine/IRenderNode.h>

namespace Cry
{
namespace DefaultComponents
{
void CStaticMeshComponent::Register(Schematyc::CEnvRegistrationScope& componentScope)
{
	// Functions
	{
		auto pFunction = SCHEMATYC_MAKE_ENV_FUNCTION(&CStaticMeshComponent::LoadFromDisk, "{E900651C-169C-4FF2-B987-CFC5613D51EB}"_cry_guid, "LoadFromDisk");
		pFunction->SetDescription("Load the mesh from disk");
		pFunction->SetFlags({ Schematyc::EEnvFunctionFlags::Member, Schematyc::EEnvFunctionFlags::Construction });
		componentScope.Register(pFunction);
	}
	{
		auto pFunction = SCHEMATYC_MAKE_ENV_FUNCTION(&CStaticMeshComponent::ResetObject, "{86AA9C32-3D54-4622-8032-C99C194FF24A}"_cry_guid, "ResetObject");
		pFunction->SetDescription("Resets the object");
		pFunction->SetFlags({ Schematyc::EEnvFunctionFlags::Member, Schematyc::EEnvFunctionFlags::Construction });
		componentScope.Register(pFunction);
	}
	{
		auto pFunction = SCHEMATYC_MAKE_ENV_FUNCTION(&CStaticMeshComponent::SetMeshType, "{6C4F6C80-2F84-4C9B-ADBF-5C131EFDD98A}"_cry_guid, "SetType");
		pFunction->BindInput(1, 'type', "Type");
		pFunction->SetDescription("Changes the type of the object");
		pFunction->SetFlags({ Schematyc::EEnvFunctionFlags::Member });
		componentScope.Register(pFunction);
	}
	{
		auto pFunction = SCHEMATYC_MAKE_ENV_FUNCTION(&CStaticMeshComponent::SetObject, "{7E31601A-2766-4F3D-AF97-65F34556C664}"_cry_guid, "SetObject");
		pFunction->BindInput(1, 'path', "Path");
		pFunction->BindInput(2, 'setd', "Set Default Mass");
		pFunction->SetDescription("Changes the type of the object");
		pFunction->SetFlags({ Schematyc::EEnvFunctionFlags::Member });
		componentScope.Register(pFunction); 
	}
}

void CStaticMeshComponent::Initialize()
{
	LoadFromDisk();
	ResetObject();
}

void CStaticMeshComponent::LoadFromDisk()
{
	if (m_filePath.value.size() > 0)
	{
		m_pCachedStatObj = gEnv->p3DEngine->LoadStatObj(m_filePath.value);
	}
	else
	{
		m_pCachedStatObj = nullptr;
	}
}

void CStaticMeshComponent::SetObjectDirect(IStatObj* pObject, bool bSetDefaultMass)
{
	m_pCachedStatObj = pObject;
	m_filePath.value = m_pCachedStatObj->GetFilePath();

	if (bSetDefaultMass)
	{
		float mass = 0;
		float density = 0;

		// First get the mass or density from the statobj. If it's zero we should fall back to our default value/previous value.
		if (m_pCachedStatObj->GetPhysicalProperties(mass, density) && (mass > 0 || density > 0))
		{
			m_physics.m_mass = mass;
			m_physics.m_density = density;
			m_physics.m_weightType = mass > 0 ? SPhysicsParameters::EWeightType::Mass : SPhysicsParameters::EWeightType::Density;
		}
	}

	ResetObject();
}

void CStaticMeshComponent::SetObject(Schematyc::CSharedString path, bool bSetDefaultMass)
{
	IStatObj* pObject = gEnv->p3DEngine->LoadStatObj(path.c_str());

	if (pObject != nullptr)
	{
		SetObjectDirect(pObject, bSetDefaultMass);
	}
}

void CStaticMeshComponent::ResetObject()
{
	if (m_pCachedStatObj != nullptr)
	{
		m_pEntity->SetStatObj(m_pCachedStatObj, GetOrMakeEntitySlotId() | ENTITY_SLOT_ACTUAL, false);
		if (!m_materialPath.value.empty())
		{
			if (IMaterial* pMaterial = gEnv->p3DEngine->GetMaterialManager()->LoadMaterial(m_materialPath.value, false))
			{
				m_pEntity->SetSlotMaterial(GetEntitySlotId(), pMaterial);
			}
		}
	}
	else
	{
		FreeEntitySlot();
	}
}

void CStaticMeshComponent::ProcessEvent(const SEntityEvent& event)
{
	if (event.event == ENTITY_EVENT_COMPONENT_PROPERTY_CHANGED)
	{
		LoadFromDisk();
		ResetObject();

		// Update Editor UI to show the default object material
		if (m_materialPath.value.empty() && m_pCachedStatObj != nullptr)
		{
			if (IMaterial* pMaterial = m_pCachedStatObj->GetMaterial())
			{
				m_materialPath = pMaterial->GetName();
			}
		}
	}

	CBaseMeshComponent::ProcessEvent(event);
}

#ifndef RELEASE
void CStaticMeshComponent::Render(const IEntity& entity, const IEntityComponent& component, SEntityPreviewContext &context) const
{
	if (m_type == EMeshType::Collider && context.bSelected)
	{
		context.debugDrawInfo.tm = m_pEntity->GetSlotWorldTM(GetEntitySlotId());
		m_pCachedStatObj->DebugDraw(context.debugDrawInfo);
	}

}
#endif

void CStaticMeshComponent::SetFilePath(const char* szPath)
{
	m_filePath.value = szPath;
}
}} // namespace Cry::DefaultComponents
