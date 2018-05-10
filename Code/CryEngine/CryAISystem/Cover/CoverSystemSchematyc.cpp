// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "CoverSystemSchematyc.h"

#include "Components/Cover/CoverUserComponent.h"

namespace CoverSystemSchematyc
{
	void Register(Schematyc::IEnvRegistrar& registrar, Schematyc::CEnvRegistrationScope& parentScope)
	{
		//Register Components
		CEntityAICoverUserComponent::Register(registrar);
		
		const CryGUID CoverSystemGUID = "736C4B57-6DEE-405C-9C06-58659FAB999A"_cry_guid;

		parentScope.Register(SCHEMATYC_MAKE_ENV_MODULE(CoverSystemGUID, "Covers"));
		Schematyc::CEnvRegistrationScope coversScope = registrar.Scope(CoverSystemGUID);

		// Nothing registered now
	}
}
