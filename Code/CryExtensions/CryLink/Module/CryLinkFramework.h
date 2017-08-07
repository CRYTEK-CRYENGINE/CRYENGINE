// Copyright 2001-2015 Crytek GmbH. All rights reserved.

#pragma once

#include "CryLink/IFramework.h"
#include "CryLinkService.h"

#include <CryExtension/ClassWeaver.h>

#define CRY_LINK_EXTENSION_CRYCLASS_NAME "CryExtension_CryLink"

namespace CryLinkService
{
	class CCryLinkFramework : public IFramework
	{
		CRYINTERFACE_BEGIN()
			CRYINTERFACE_ADD(IFramework)
		CRYINTERFACE_END()

		CRYGENERATE_CLASS_GUID(CCryLinkFramework, CRY_LINK_EXTENSION_CRYCLASS_NAME, "8edceb7a-34a7-4853-bbd8-cf7d2599eeab"_cry_guid)

	public:
		virtual ~CCryLinkFramework();

		//IFramework
		virtual ICryLinkService& GetService() override;
		//~IFramework

	private:
		CCryLinkService m_cryLinkService;
	};

}

