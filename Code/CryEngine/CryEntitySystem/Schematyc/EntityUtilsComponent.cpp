// Copyright 2001-2016 Crytek GmbH / Crytek Group. All rights reserved.

#include "stdafx.h"
#include "EntityUtilsComponent.h"

#include <CryRenderer/IRenderAuxGeom.h>

#include <CrySchematyc/Env/Elements/EnvFunction.h>
#include <CrySchematyc/Utils/SharedString.h>
#include <CrySchematyc/IObject.h>
#include <CrySchematyc/Env/Elements/EnvComponent.h>
#include <CrySchematyc/Env/Elements/EnvSignal.h>

#include  <CrySchematyc/Env/IEnvRegistrar.h>

namespace Schematyc
{
	ExplicitEntityId CEntityUtilsComponent::GetEntityId() const
	{
		return ExplicitEntityId(m_pEntity->GetId());
	}

	void CEntityUtilsComponent::SetTransform(const CryTransform::CTransform& transform)
	{
		m_pEntity->SetWorldTM(transform.ToMatrix34());
	}

	CryTransform::CTransform CEntityUtilsComponent::GetTransform()
	{
		return CryTransform::CTransform(m_pEntity->GetWorldTM());
	}

	void CEntityUtilsComponent::SetRotation(const CryTransform::CRotation& rotation)
	{
		Matrix34 transform = m_pEntity->GetWorldTM();
		transform.SetRotation33(rotation.ToMatrix33());
		m_pEntity->SetWorldTM(transform);
	}

	CryTransform::CRotation CEntityUtilsComponent::GetRotation()
	{
		return CryTransform::CRotation(m_pEntity->GetWorldRotation());
	}

	void CEntityUtilsComponent::SetVisible(bool bVisible)
	{
		m_pEntity->Invisible(!bVisible);
	}

	bool CEntityUtilsComponent::IsVisible() const
	{
		return !m_pEntity->IsInvisible();
	}

	void CEntityUtilsComponent::ReflectType(CTypeDesc<CEntityUtilsComponent>& desc)
	{
		desc.SetGUID("e88093df-904f-4c52-af38-911e26777cdc"_cry_guid);
		desc.SetLabel("Entity");
		desc.SetDescription("Entity utilities component");
		desc.SetIcon("icons:schematyc/entity_utils_component.ico");
		desc.SetComponentFlags({ EFlags::Singleton, EFlags::HideFromInspector, EFlags::HiddenFromUser });
	}

	void CEntityUtilsComponent::Register(IEnvRegistrar& registrar)
	{
		CEnvRegistrationScope scope = registrar.Scope(IEntity::GetEntityScopeGUID());
		{
			CEnvRegistrationScope componentScope = scope.Register(SCHEMATYC_MAKE_ENV_COMPONENT(CEntityUtilsComponent));
			// Functions
			{
				auto pFunction = SCHEMATYC_MAKE_ENV_FUNCTION(&CEntityUtilsComponent::GetEntityId, "c01d8df5-058f-406f-8c4c-8426e856f294"_cry_guid, "GetEntityId");
				pFunction->SetDescription("Get Entity Id");
				pFunction->BindOutput(0, 'id', "EntityId");
				componentScope.Register(pFunction);
			}

			{
				auto pFunction = SCHEMATYC_MAKE_ENV_FUNCTION(&CEntityUtilsComponent::SetTransform, "FA08CEA0-A0C5-4340-9F8A-E38D74488BAF"_cry_guid, "SetTransform");
				pFunction->SetDescription("Set Entity Transformation");
				pFunction->BindInput(1, 'tr', "transform");
				componentScope.Register(pFunction);
			}

			{
				auto pFunction = SCHEMATYC_MAKE_ENV_FUNCTION(&CEntityUtilsComponent::GetTransform, "8A99E1BA-A5CD-4DE8-A19F-D07DF5D3B245"_cry_guid, "GetTransform");
				pFunction->SetDescription("Get Entity Transformation");
				pFunction->BindOutput(0, 'tr', "transform");
				componentScope.Register(pFunction);
			}

			{
				auto pFunction = SCHEMATYC_MAKE_ENV_FUNCTION(&CEntityUtilsComponent::SetRotation, "{53FDFFFB-A216-4001-BA26-9E81A7D2160D}"_cry_guid, "SetRotation");
				pFunction->SetDescription("Set Entity Rotation");
				pFunction->BindInput(1, 'rot', "rotation");
				componentScope.Register(pFunction);
			}

			{
				auto pFunction = SCHEMATYC_MAKE_ENV_FUNCTION(&CEntityUtilsComponent::GetRotation, "{B03F7198-583E-4C9C-BDC7-92D904920D2C}"_cry_guid, "GetRotation");
				pFunction->SetDescription("Get Entity Rotation");
				pFunction->BindOutput(0, 'rot', "rotation");
				componentScope.Register(pFunction);
			}

			{
				auto pFunction = SCHEMATYC_MAKE_ENV_FUNCTION(&CEntityUtilsComponent::SetVisible, "abc4938d-a631-4a36-9f10-22cf6dc9dabd"_cry_guid, "SetVisible");
				pFunction->SetDescription("Show/hide geometry");
				pFunction->SetFlags(EEnvFunctionFlags::Construction);
				pFunction->BindInput(1, 'vis', "Visible");
				componentScope.Register(pFunction);
			}
			{
				auto pFunction = SCHEMATYC_MAKE_ENV_FUNCTION(&CEntityUtilsComponent::IsVisible, "5aa5e8f0-b4f4-491d-8074-d8b129500d09"_cry_guid, "IsVisible");
				pFunction->SetDescription("Is geometry visible?");
				pFunction->SetFlags(EEnvFunctionFlags::Construction);
				pFunction->BindOutput(0, 'vis', "Visible");
				componentScope.Register(pFunction);
			}
		}
	}

} // Schematyc