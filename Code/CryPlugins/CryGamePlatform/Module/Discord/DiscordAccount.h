// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include "IPlatformAccount.h"
#include "DiscordTypes.h"

namespace Cry
{
	namespace GamePlatform
	{
		namespace Discord
		{
			class CAccount
				: public IAccount
			{
			public:
				CAccount() = default;
				explicit CAccount(const DiscordUser& user);
				virtual ~CAccount();

				// IAccount
				virtual const char* GetNickname() const override;
				virtual AccountIdentifier GetIdentifier() const override;
				virtual ServiceIdentifier GetServiceIdentifier() const override;
				virtual void SetStatus(const char* szStatus) override;
				virtual void SetPresence(const SRichPresence& presence) override;
				virtual void GetPresence(SRichPresence& presence) const override;
				virtual TextureId GetAvatar(EAvatarSize size) const override;
				virtual const DynArray<AccountIdentifier>& GetConnectedAccounts() const override;
				virtual bool IsLocal() const override;
				// ~IAccount

				const AccountIdentifierValue& GetDiscordID() const { return m_id; }
				void SetDiscordUser(const DiscordUser& user);

			protected:
				AccountIdentifierValue m_id;
				string m_nickname;
				string m_avatarHash;

				SRichPresence m_presence;

				DynArray<AccountIdentifier> m_connectedAccounts;
			};
		}
	}
}