// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"

#include "DiscordAccount.h"

namespace Cry
{
	namespace GamePlatform
	{
		namespace Discord
		{
			CAccount::CAccount(const DiscordUser& user)
			{
				SetDiscordUser(user);
			}

			CAccount::~CAccount()
			{
			}

			const char* CAccount::GetNickname() const
			{
				return m_nickname.c_str();
			}

			AccountIdentifier CAccount::GetIdentifier() const
			{
				return CreateAccountIdentifier(m_id);
			}

			ServiceIdentifier CAccount::GetServiceIdentifier() const
			{
				return DiscordServiceID;
			}

			void CAccount::SetStatus(const char* szStatus)
			{
				SRichPresence presence;
				presence.headline = szStatus;

				SetPresence(presence);
			}

			void CAccount::SetPresence(const SRichPresence& presence)
			{
				m_presence = presence;

				DiscordRichPresence discordPresence = {};

				// NOTE: Field names can be a bit misleading:
				// * 'details' is the first, and shortest, presence line
				// * 'state' is optional and provides additional information 
 				discordPresence.details = m_presence.headline.c_str();
 				discordPresence.state = m_presence.activity.c_str();
				discordPresence.partySize = m_presence.partySize;
				discordPresence.partyMax = m_presence.partyMax;

				if (m_presence.countdownTimer == SRichPresence::ETimer::Remaining && m_presence.seconds > 0)
				{
					discordPresence.endTimestamp = time(0) + m_presence.seconds;
				}
				else if (m_presence.countdownTimer == SRichPresence::ETimer::Elapsed)
				{
					discordPresence.startTimestamp = time(0) + m_presence.seconds;
				}

				CryComment("[Discord] Setting rich presence (first line): %s", m_presence.headline.c_str());
				if (!m_presence.activity.empty())
				{
					CryComment("[Discord]\t\t\t\t\t\t(second line): %s (%d of %d)", m_presence.activity.c_str(), discordPresence.partySize, discordPresence.partyMax);
				}

				if (m_presence.seconds > 0)
				{
					CryComment("[Discord]\t\t\t\t\t\t(timer): %" PRIi64, m_presence.seconds);
				}

				Discord_UpdatePresence(&discordPresence);
			}

			void CAccount::GetPresence(SRichPresence& presence) const
			{
				presence = m_presence;
			}

			const DynArray<Cry::GamePlatform::AccountIdentifier>& CAccount::GetConnectedAccounts() const
			{
				return m_connectedAccounts;
			}

			bool CAccount::IsLocal() const
			{
				// luca 7/4/2018: As of now, Discord SDK doesn't provide account information for anybody else
				return true;
			}

			TextureId CAccount::GetAvatar(EAvatarSize size) const
			{
				// luca 7/3/2018: Need a web request to get the avatar https://discordapp.com/developers/docs/reference#image-formatting
				return NullTextureID;
			}

			void CAccount::SetDiscordUser(const DiscordUser& user)
			{
				CRY_ASSERT_MESSAGE(strlen(user.userId) < m_id.MAX_SIZE);

				m_id = user.userId;
				m_nickname = user.username;
				m_avatarHash = user.avatar;
			}
		}
	}
}