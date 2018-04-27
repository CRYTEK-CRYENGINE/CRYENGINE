// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

// *INDENT-OFF* - <hard to read code and declarations due to inconsistent indentation>

namespace UQS
{
	namespace Client
	{

		//===================================================================================
		//
		// CFactoryRegistrationHelper
		//
		// - registers all factories of item types, evaluators, generators, etc. in the IHub
		// - it's a convenient way for client code to register all factories in just one call
		// - should be called upon Core::EHubEvent::RegisterYourFactoriesNow
		//
		//===================================================================================

		class CFactoryRegistrationHelper
		{
		public:
			static void RegisterAllFactoryInstancesInHub(Core::IHub& hub);
		};

		inline void CFactoryRegistrationHelper::RegisterAllFactoryInstancesInHub(Core::IHub& hub)
		{
			Internal::CItemFactoryBase::RegisterAllInstancesInFactoryDatabase(hub.GetItemFactoryDatabase());
			Internal::CFunctionFactoryBase::RegisterAllInstancesInFactoryDatabase(hub.GetFunctionFactoryDatabase());
			Internal::CGeneratorFactoryBase::RegisterAllInstancesInFactoryDatabase(hub.GetGeneratorFactoryDatabase());
			Internal::CInstantEvaluatorFactoryBase::RegisterAllInstancesInFactoryDatabase(hub.GetInstantEvaluatorFactoryDatabase());
			Internal::CDeferredEvaluatorFactoryBase::RegisterAllInstancesInFactoryDatabase(hub.GetDeferredEvaluatorFactoryDatabase());
		}

	}
}
