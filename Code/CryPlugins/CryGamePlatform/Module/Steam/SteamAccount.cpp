// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"

#include "SteamAccount.h"

namespace Cry
{
	namespace GamePlatform
	{
		namespace Steam
		{
			CAccount::CAccount(CSteamID id)
				: m_id(id)
			{
			}

			CAccount::~CAccount()
			{
				m_id.Clear();
			}

			const char* CAccount::GetNickname() const
			{
				if (ISteamFriends* pSteamFriends = SteamFriends())
				{
					const char* szNickName = pSteamFriends->GetFriendPersonaName(m_id);

					// https://partner.steamgames.com/doc/api/ISteamFriends#GetFriendPersonaName
					if (szNickName[0] != '\0' && cry_strcmp(szNickName, "[unknown]") != 0)
					{
						return szNickName;
					}
				}

				return nullptr;
			}

			AccountIdentifier CAccount::GetIdentifier() const
			{
				return AccountIdentifier(SteamServiceID, m_id.ConvertToUint64());
			}

			ServiceIdentifier CAccount::GetServiceIdentifier() const
			{
				return SteamServiceID;
			}

			void CAccount::SetStatus(const char* szStatus)
			{
				ISteamUser* pSteamUser = SteamUser();
				ISteamFriends* pSteamFriends = SteamFriends();
				if (!pSteamFriends || !pSteamUser || m_id != pSteamUser->GetSteamID())
				{
					return;
				}

				CryComment("[Steam] Setting rich presence '%s'", szStatus);

				pSteamFriends->SetRichPresence("status", szStatus);
			}

			void CAccount::SetPresence(const SRichPresence& presence)
			{
				CryWarning(VALIDATOR_MODULE_SYSTEM, VALIDATOR_WARNING, "[Steam] No rich presence support. Setting only status!");

				SetStatus(presence.headline);
			}

			void CAccount::GetPresence(SRichPresence& presence) const
			{
				presence = SRichPresence();

				if (ISteamFriends* pSteamFriends = SteamFriends())
				{
					presence.headline = pSteamFriends->GetFriendRichPresence(m_id, "status");
				}
			}

			const DynArray<Cry::GamePlatform::AccountIdentifier>& CAccount::GetConnectedAccounts() const
			{
				return m_connectedAccounts;
			}

			bool CAccount::IsLocal() const
			{
				if (ISteamUser* pSteamUser = SteamUser())
				{
					return pSteamUser->GetSteamID() == m_id;
				}

				return false;
			}

			TextureId CAccount::GetAvatar(EAvatarSize size) const
			{
				const size_t avatarIdx = static_cast<size_t>(size);
				if (m_pAvatars[avatarIdx])
				{
					return m_pAvatars[avatarIdx]->GetTextureID();
				}

				int imageId = -1;

				if (ISteamFriends* pSteamFriends = SteamFriends())
				{
					switch (size)
					{
						case EAvatarSize::Large:
							imageId = pSteamFriends->GetLargeFriendAvatar(m_id);
							break;
						case EAvatarSize::Medium:
							imageId = pSteamFriends->GetMediumFriendAvatar(m_id);
							break;
						case EAvatarSize::Small:
							imageId = pSteamFriends->GetSmallFriendAvatar(m_id);
							break;
					}
				}

				ISteamUtils* pSteamUtils = SteamUtils();

				if (imageId == -1 || !pSteamUtils)
				{
					CryWarning(VALIDATOR_MODULE_SYSTEM, VALIDATOR_WARNING, "[Steam] Failed to load avatar");
					return NullTextureID;
				}

				uint32 width, height;
				if (!pSteamUtils->GetImageSize(imageId, &width, &height))
				{
					return NullTextureID;
				}

				const size_t imageSize = width * height * 4;
				std::vector<unsigned char> imageBuffer;
				imageBuffer.resize(imageSize);
				if (!pSteamUtils->GetImageRGBA(imageId, imageBuffer.data(), imageSize))
				{
					return NullTextureID;
				}

				ETEX_Format format = eTF_R8G8B8A8;
				const int textureId = gEnv->pRenderer->UploadToVideoMemory(imageBuffer.data(), width, height, format, format, 1);

				m_pAvatars[avatarIdx].reset(gEnv->pRenderer->EF_GetTextureByID(textureId));

				return textureId;
			}
		}
	}
}