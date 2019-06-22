// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include "IPlatformUser.h"
#include "IPlatformAccount.h"

namespace Cry
{
	namespace GamePlatform
	{
		//! Standard implementation of a game platform user.
		class CUser final : public IUser
		{
		public:
			explicit CUser(DynArray<IAccount*> accounts);
			virtual ~CUser();

			// IUser
			virtual const char* GetNickname() const override;
			virtual UserIdentifier GetIdentifier() const override;

			virtual void SetStatus(const char* status) override;
			virtual void SetPresence(const SRichPresence& presence) override;

			virtual TextureId GetAvatar(EAvatarSize size) const override;
			virtual IAccount* GetAccount(const ServiceIdentifier& svcId) const override;
			virtual bool HasAccount(const IAccount& account) const override;
			// ~IUser

			void SetAccounts(DynArray<IAccount*> accounts);
			const DynArray<IAccount*>& GetAccounts() const;
			void RemoveAccount(const IAccount& account);
			const char* ToDebugString() const;

		protected:
			DynArray<IAccount*> m_accounts;
		};
	}
}