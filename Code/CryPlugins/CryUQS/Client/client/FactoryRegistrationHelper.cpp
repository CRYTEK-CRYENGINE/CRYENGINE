// Copyright 2001-2016 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"

// *INDENT-OFF* - <hard to read code and declarations due to inconsistent indentation>

namespace UQS
{
	namespace Client
	{

		void CFactoryRegistrationHelper::RegisterAllFactoryInstancesInHub(Core::IHub& hub)
		{
			Internal::CItemFactoryBase::RegisterAllInstancesInFactoryDatabase(hub.GetItemFactoryDatabase());
			Internal::CFunctionFactoryBase::RegisterAllInstancesInFactoryDatabase(hub.GetFunctionFactoryDatabase());
			Internal::CGeneratorFactoryBase::RegisterAllInstancesInFactoryDatabase(hub.GetGeneratorFactoryDatabase());
			Internal::CInstantEvaluatorFactoryBase::RegisterAllInstancesInFactoryDatabase(hub.GetInstantEvaluatorFactoryDatabase());
			Internal::CDeferredEvaluatorFactoryBase::RegisterAllInstancesInFactoryDatabase(hub.GetDeferredEvaluatorFactoryDatabase());
		}

	}
}
