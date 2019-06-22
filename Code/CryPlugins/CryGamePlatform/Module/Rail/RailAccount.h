// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include "IPlatformAccount.h"
#include "RailTypes.h"

namespace Cry
{
	namespace GamePlatform
	{
		namespace Rail
		{
			class CAccount
				: public IAccount
			{
			public:
				explicit CAccount(rail::RailID id);
				explicit CAccount(const rail::RailFriendInfo& friendInfo);
				explicit CAccount(const rail::PlayerPersonalInfo& personalInfo);

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

				void SetFriendInfo(const rail::RailFriendInfo& friendInfo);
				void SetPersonalInfo(const rail::PlayerPersonalInfo& personalInfo);
				void SetAvatarInfo(const rail::RailString& data, const rail::RailImageDataDescriptor& descriptor);

				const rail::RailID& GetRailID() const { return m_id; }
				const rail::EnumRailFriendType& GetFriendType() const { return m_friendType; }
				const rail::RailFriendOnLineState& GetOnLineState() const { return m_onLineState; }

			protected:
				rail::RailID m_id = rail::kInvalidRailId;
				mutable rail::RailString m_nickname;
				rail::EnumRailFriendType m_friendType = rail::kRailFriendTypeUnknown;
				rail::RailFriendOnLineState m_onLineState;
				rail::RailString m_currentStatus;
				_smart_ptr<ITexture> m_pAvatar; // Indexed by EAvatarSize

				DynArray<AccountIdentifier> m_connectedAccounts;
				rail::RailArray<rail::RailGameDefineGamePlayingState> m_userDefinedStatuses;
			};
		}
	}
}