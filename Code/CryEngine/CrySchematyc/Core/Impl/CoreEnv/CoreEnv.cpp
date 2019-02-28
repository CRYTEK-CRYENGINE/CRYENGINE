// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "CoreEnv.h"

#include <CrySchematyc/Env/IEnvRegistrar.h>

#include <CrySchematyc/Env/Elements/EnvModule.h>

#include "CoreEnvSignals.h"

namespace Schematyc
{
void RegisterCoreEnvPackage(IEnvRegistrar& registrar)
{
	RegisterCoreEnvSignals(registrar);

	registrar.RootScope().Register(SCHEMATYC_MAKE_ENV_MODULE(g_stdModuleGUID, "Standard"));
	{
		CEnvRegistrationScope scope = registrar.Scope(g_stdModuleGUID);
		scope.Register(SCHEMATYC_MAKE_ENV_MODULE(g_logModuleGUID, "Log"));
		scope.Register(SCHEMATYC_MAKE_ENV_MODULE(g_mathModuleGUID, "Math"));
		scope.Register(SCHEMATYC_MAKE_ENV_MODULE(g_resourceModuleGUID, "Resource"));

		CryInvokeStaticCallbacks<Schematyc::IEnvRegistrar&>(registrar);
		//CryInvokeStaticCallbacks<int>(0);
	}
}
} // Schematyc
