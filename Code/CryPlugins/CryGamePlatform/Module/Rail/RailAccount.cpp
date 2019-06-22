// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "RailAccount.h"
#include "RailHelper.h"

namespace Cry
{
	namespace GamePlatform
	{
		namespace Rail
		{
			CAccount::CAccount(rail::RailID id)
				: m_id(id)
			{
			}

			CAccount::CAccount(const rail::RailFriendInfo& friendInfo)
				: m_id(friendInfo.friend_rail_id)
				, m_friendType(friendInfo.friend_type)
				, m_onLineState(friendInfo.online_state)
			{

			}

			CAccount::CAccount(const rail::PlayerPersonalInfo& personalInfo)
				: m_id(personalInfo.rail_id)
				, m_nickname(personalInfo.rail_name)
			{

			}

			const char* CAccount::GetNickname() const
			{
				if (IsLocal() && m_nickname.size() == 0)
				{
					if (rail::IRailPlayer* const pPlayer = Helper::Player())
					{
						const rail::RailResult res = pPlayer->GetPlayerName(&m_nickname);
						if (res != rail::kSuccess)
						{
							CryWarning(VALIDATOR_MODULE_SYSTEM, VALIDATOR_ERROR, "[Rail][Account] GetPlayerName() failed : %s", Helper::ErrorString(res));
						}
					}
				}

				return m_nickname.c_str();
			}

			AccountIdentifier CAccount::GetIdentifier() const
			{
				return AccountIdentifier(RailServiceID, m_id.get_id());
			}

			ServiceIdentifier CAccount::GetServiceIdentifier() const
			{
				return RailServiceID;
			}

			void CAccount::SetStatus(const char* szStatus)
			{
				if (!IsLocal())
				{
					CryWarning(VALIDATOR_MODULE_SYSTEM, VALIDATOR_ERROR, "[Rail][Account] SetStatus() should only be called on LOCAL account!");
					return;
				}

				if (rail::IRailGame* const pGame = Helper::Game())
				{
					size_t statusIndex = 0;
					for (; statusIndex < m_userDefinedStatuses.size(); ++statusIndex)
					{
						if (cry_strcmp(m_userDefinedStatuses[statusIndex].state_name_en_us.c_str(), szStatus) == 0)
						{
							break;
						}
					}

					if (statusIndex == m_userDefinedStatuses.size())
					{
						rail::RailGameDefineGamePlayingState newState;
						newState.game_define_game_playing_state = rail::kRailGamePlayingStateGameDefinePlayingState + m_userDefinedStatuses.size();
						newState.state_name_en_us = szStatus;
						newState.state_name_zh_cn = szStatus;

						m_userDefinedStatuses.push_back(newState);

						const rail::RailResult res = pGame->RegisterGameDefineGamePlayingState(m_userDefinedStatuses);
						if (res != rail::kSuccess)
						{
							CryWarning(VALIDATOR_MODULE_SYSTEM, VALIDATOR_ERROR, "[Rail][Account] SetStatus('%s') -> RegisterGameDefineGamePlayingState() failed : %s", szStatus, Helper::ErrorString(res));
							return;
						}
					}

					const uint32_t statusId = m_userDefinedStatuses[statusIndex].game_define_game_playing_state;

					const rail::RailResult res = pGame->SetGameDefineGamePlayingState(statusId);
					if (res != rail::kSuccess)
					{
						CryWarning(VALIDATOR_MODULE_SYSTEM, VALIDATOR_ERROR, "[Rail][Account] SetStatus('%s') -> SetGameDefineGamePlayingState(%u) failed : %s", szStatus, statusId, Helper::ErrorString(res));
						return;
					}
				}

				CryComment("[Rail][Account] Set status: '%s'", szStatus);
			}

			void CAccount::SetPresence(const SRichPresence& presence)
			{
				CryWarning(VALIDATOR_MODULE_SYSTEM, VALIDATOR_WARNING, "[Rail][Account] No rich presence support. Setting only status!");

				SetStatus(presence.headline);
			}

			void CAccount::GetPresence(SRichPresence& presence) const
			{
				presence = SRichPresence();
				presence.headline = m_currentStatus.c_str();
			}

			const DynArray<Cry::GamePlatform::AccountIdentifier>& CAccount::GetConnectedAccounts() const
			{
				return m_connectedAccounts;
			}

			bool CAccount::IsLocal() const
			{
				return m_id != rail::kInvalidRailId && m_id == Helper::PlayerID();
			}

			void CAccount::SetFriendInfo(const rail::RailFriendInfo& friendInfo)
			{
				m_id = friendInfo.friend_rail_id;
				m_friendType = friendInfo.friend_type;
				m_onLineState = friendInfo.online_state;
			}

			void CAccount::SetPersonalInfo(const rail::PlayerPersonalInfo& personalInfo)
			{
				CryComment("[Rail][Account] id:\t\t%" PRIu64, personalInfo.rail_id.get_id());
				CryComment("[Rail][Account] name:\t\t%s",     personalInfo.rail_name.c_str());
				CryComment("[Rail][Account] email:\t\t%s",    personalInfo.email_address.c_str());
				CryComment("[Rail][Account] level:\t\t%u",    personalInfo.rail_level);
				CryComment("[Rail][Account] avatar:\t\t%s",   personalInfo.avatar_url.c_str());

				m_id = personalInfo.rail_id;
				m_nickname = personalInfo.rail_name;
			}

			void CAccount::SetAvatarInfo(const rail::RailString& data, const rail::RailImageDataDescriptor& descriptor)
			{
				if (descriptor.pixel_format == rail::kRailImagePixelFormatR8G8B8A8 && descriptor.stride_in_bytes == 0)
				{
					const ETEX_Format format = eTF_R8G8B8A8;

					const unsigned char* rawData = reinterpret_cast<const unsigned char*>(data.c_str());

					const TextureId textureId = gEnv->pRenderer->UploadToVideoMemory(const_cast<unsigned char*>(rawData), descriptor.image_width, descriptor.image_height, format, format, 1);

					m_pAvatar.reset(gEnv->pRenderer->EF_GetTextureByID(textureId));
				}
				else
				{
					CryWarning(VALIDATOR_MODULE_SYSTEM, VALIDATOR_ERROR, "[Rail][Account] SetAvatarInfo() unsupported format '%d' (stride='%u')", (int)descriptor.pixel_format, descriptor.stride_in_bytes);
				}
			}

			TextureId CAccount::GetAvatar(EAvatarSize size) const
			{
				if (m_pAvatar == nullptr)
				{
					return NullTextureID;
				}

				int desiredWidth = 184;
				int desiredHeight = 184;

				switch (size)
				{
				case EAvatarSize::Large:
					desiredWidth = 184;
					desiredHeight = 184;
					break;
				case EAvatarSize::Medium:
					desiredWidth = 64;
					desiredHeight = 64;
					break;
				case EAvatarSize::Small:
					desiredWidth = 32;
					desiredHeight = 32;
					break;
				}

				if (m_pAvatar->GetWidth() != desiredWidth || m_pAvatar->GetHeight() != desiredHeight)
				{
					CryWarning(VALIDATOR_MODULE_SYSTEM, VALIDATOR_WARNING, "[Rail][Account] GetAvatar() returned image of size %dx%d instead of requested %dx%d",
						m_pAvatar->GetWidth(), m_pAvatar->GetHeight(),
						desiredWidth, desiredHeight);
				}

				return m_pAvatar->GetTextureID();
			}
		}
	}
}