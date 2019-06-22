// Copyright 2001-2019 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include <CrySystem/IEngineModule.h>
#include <CrySystem/ISystem.h>

// *INDENT-OFF* - <hard to read code and declarations due to inconsistent indentation>

namespace Cry
{
	namespace UDR
	{	
		struct IUDREngineModule : public Cry::IDefaultModule
		{
			CRYINTERFACE_DECLARE_GUID(IUDREngineModule, "560ddced-bd31-44c0-96ba-550cc1317a39"_cry_guid);
		};

	}
}

