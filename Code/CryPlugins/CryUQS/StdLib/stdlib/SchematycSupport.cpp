// Copyright 2001-2016 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include <CryAISystem/IAISystem.h>
#include <CryAISystem/INavigationSystem.h>

// *INDENT-OFF* - <hard to read code and declarations due to inconsistent indentation>

#if UQS_SCHEMATYC_SUPPORT

namespace UQS
{
	namespace StdLib
	{

		static const Schematyc::CryGUID g_uqsStdlibModuleGUID = "29fade44-bd65-470f-863e-458342f1ab92"_cry_guid;
		static const Schematyc::CryGUID g_uqsStdlibPackageGUID = "156dba39-d156-495a-97c5-ed1cf951ecc7"_cry_guid;

		static void OnRegisterInSchematyc(Schematyc::IEnvRegistrar& registrar);

		void CStdLibRegistration::RegisterInSchematyc()
		{
			const char* szName = "UQSStdLib";
			const char* szDescription = "UQS Standard Library";
			Schematyc::EnvPackageCallback callback = SCHEMATYC_DELEGATE(&OnRegisterInSchematyc);
			gEnv->pSchematyc->GetEnvRegistry().RegisterPackage(SCHEMATYC_MAKE_ENV_PACKAGE(g_uqsStdlibPackageGUID, szName, Schematyc::g_szCrytek, szDescription, callback));
		}

		static void OnRegisterInSchematyc(Schematyc::IEnvRegistrar& registrar)
		{
			registrar.RootScope().Register(SCHEMATYC_MAKE_ENV_MODULE(g_uqsStdlibModuleGUID, "UQSStdlib"));
			{
				Schematyc::CEnvRegistrationScope scope = registrar.Scope(g_uqsStdlibModuleGUID);
			
				scope.Register(SCHEMATYC_MAKE_ENV_DATA_TYPE(NavigationAgentTypeID));
				// leave out the more common data types like int, float, bool, etc. (they already get registered by schematyc itself)
			
			}
		}

		void CStdLibRegistration::UnregisterInSchematyc()
		{
			gEnv->pSchematyc->GetEnvRegistry().DeregisterPackage(g_uqsStdlibPackageGUID);
		}

	}
}

namespace Schematyc   // freestanding ReflectType() functions need to be in the 'Schematyc' namespace
{

	//
	// NavigationAgentTypeID
	//

	/* static ? */ void NavigationAgentTypeIDToString(Schematyc::IString& output, const NavigationAgentTypeID& input)
	{
		output.assign("");

		if (const IAISystem* pAISystem = gEnv->pAISystem)
		{
			if (const INavigationSystem* pNavigationSystem = pAISystem->GetNavigationSystem())
			{
				if (const char* szAgentTypeName = pNavigationSystem->GetAgentTypeName(input))
				{
					output.assign(szAgentTypeName);
				}
			}
		}
	}

	static void ReflectType(Schematyc::CTypeDesc<NavigationAgentTypeID>& desc)
	{
		desc.SetGUID("52fb5a3e-b615-496b-a7b2-e9dfe483f755"_cry_guid);
		desc.SetLabel("NavigationAgentTypeID");
		desc.SetDescription("");
		desc.SetDefaultValue(NavigationAgentTypeID());
		desc.SetToStringOperator<&NavigationAgentTypeIDToString>();
	}
}

static bool Serialize(Serialization::IArchive& archive, NavigationAgentTypeID& value, const char* szName, const char* szLabel)
{
	return UQS::StdLib::NavigationAgentTypeID_Serialize(archive, value, szName, szLabel);
}

#endif // UQS_SCHEMATYC_SUPPORT
