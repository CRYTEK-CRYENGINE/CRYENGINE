// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include <CrySystem/ICryPlugin.h>
#include "IPlatformBase.h"

namespace Cry
{
	namespace GamePlatform
	{
		//! Game Platform entry point. Use it to work with platform services.
		//! This can be queried via gEnv->pSystem->GetIPluginManager()->QueryPlugin<Cry::GamePlatform::IPlugin>();
		struct IPlugin : public Cry::IEnginePlugin, public IBase
		{
			CRYINTERFACE_DECLARE_GUID(IPlugin, "{7A7D908B-D12F-4827-B410-F50386018036}"_cry_guid);
			
			virtual ~IPlugin() = default;

			//! Gets an IUser representation of the local user, useful for getting local user information such as user name
			virtual IUser* GetLocalClient() const = 0;
			
			//! Gets a list of all users that are friends with the local user
			//! \note Can be expensive. Don't call too often.
			virtual const DynArray<IUser*>& GetFriends() const = 0;

#if CRY_GAMEPLATFORM_EXPERIMENTAL
			//! Gets a list of all users that are blocked by the local user
			//! \note Can be expensive. Don't call too often.
			virtual const DynArray<IUser*>& GetBlockedUsers() const = 0;

			//! Gets a list of all users that are muted by the local user
			//! \note Can be expensive. Don't call too often.
			virtual const DynArray<IUser*>& GetMutedUsers() const = 0;
#endif // CRY_GAMEPLATFORM_EXPERIMENTAL

			//! Gets an IUser representation of another user by platform user id
			//! \note This function can rarely fail, as UserIdentifier is expected to be created by IUser objects only
			virtual IUser* GetUserById(const UserIdentifier& id) const = 0;

			//! Gets an IUser representation of another user by account id, if available
			virtual IUser* GetUserById(const AccountIdentifier& accountId) const = 0;

			//! Gets the main platform service.
			virtual IService* GetMainService() const = 0;
			//! Gets a service by ID. Could also be the main one.
			virtual IService* GetService(const ServiceIdentifier& svcId) const = 0;

			//! \internal Register the main platform service. This service shall provides all essential features 
			//! and is expected to implement them.
			virtual void RegisterMainService(IService& service) = 0;
			//! \internal Register a supporting service. These should at a minimum provide some social feature.
			virtual void RegisterService(IService& service) = 0;
		};
	}
}