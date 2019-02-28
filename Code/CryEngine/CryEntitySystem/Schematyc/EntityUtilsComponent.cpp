// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

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
	}
}

} // Schematyc
