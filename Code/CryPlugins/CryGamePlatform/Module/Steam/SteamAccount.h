// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include "IPlatformAccount.h"
#include "SteamTypes.h"

namespace Cry
{
	namespace GamePlatform
	{
		namespace Steam
		{
			class CAccount
				: public IAccount
			{
			public:
				CAccount(CSteamID id);
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

				const CSteamID& GetSteamID() const { return m_id; }

			protected:
				CSteamID m_id;
				DynArray<AccountIdentifier> m_connectedAccounts;
				mutable _smart_ptr<ITexture> m_pAvatars[3]; // Indexed by EAvatarSize
			};
		}
	}
}