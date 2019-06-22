// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include "PlatformTypes.h"
#include <CryCore/Containers/CryArray.h>

namespace Cry
{
	namespace GamePlatform
	{
		//! Interface to a service account. 
		//! Each user is identified on a specific service by means of his account.
		struct IAccount
		{
			virtual ~IAccount() {}

			//! Gets the public nickname of the user
			virtual const char* GetNickname() const = 0;
			//! Gets the unique identifier of the user
			virtual AccountIdentifier GetIdentifier() const = 0;
			//! Gets the unique identifier of the service (shorthand for GetIdentifier().Service())
			virtual ServiceIdentifier GetServiceIdentifier() const = 0;
			//! Sets a simple string as the user's presence
			virtual void SetStatus(const char* szStatus) = 0;
			//! Sets the user's rich presence
			virtual void SetPresence(const SRichPresence& presence) = 0;
			//! Gets the user's rich presence
			virtual void GetPresence(SRichPresence& presence) const = 0;
			//! Gets the user's avatar of the requested size as a texture
			virtual TextureId GetAvatar(EAvatarSize size) const = 0;
			//! Gets other services' accounts that were linked to this account
			virtual const DynArray<AccountIdentifier>& GetConnectedAccounts() const = 0;
			//! Returns true if this is the local player's account
			virtual bool IsLocal() const = 0;
		};
	}
}