// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include "PSNTypes.h"

namespace Cry
{
	namespace GamePlatform
	{
		namespace PSN
		{
			inline SceUserServiceUserId ExtractSceUserID(const AccountIdentifier& accountId)
			{
				if (accountId.Service() == PSNServiceID)
				{
					AccountIdentifierValue psnId;
					if(accountId.GetAsUint64(psnId))
					{
						return SceUserServiceUserId(psnId);
					}
				}
				
				CRY_ASSERT_MESSAGE(false, "[PSN] AccountIdentifier '%s' does not seem to contain a valid PSN ID", accountId.ToDebugString());
				return SCE_USER_SERVICE_USER_ID_INVALID;
			}
		}
	}
}