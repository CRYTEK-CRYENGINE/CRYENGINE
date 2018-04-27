#include "StdAfx.h"

#include <steam/steam_api.h>

#include "SteamUser.h"

#include <CryRenderer/IRenderer.h>

namespace Cry
{
	namespace GamePlatform
	{
		namespace Steam
		{
			CUser::CUser(CSteamID id)
				: m_id(id)
			{
			}

			CUser::~CUser()
			{
				m_id.Clear();
			}

			const char* CUser::GetNickname() const
			{
				ISteamFriends* pSteamFriends = SteamFriends();
				if (!pSteamFriends)
				{
					return nullptr;
				}
				return pSteamFriends->GetFriendPersonaName(m_id);
			}

			IUser::Identifier CUser::GetIdentifier() const
			{
				return m_id.ConvertToUint64();
			}

			void CUser::SetStatus(const char* status)
			{
				ISteamUser* pSteamUser = SteamUser();
				ISteamFriends* pSteamFriends = SteamFriends();
				if (!pSteamFriends || !pSteamUser || m_id != pSteamUser->GetSteamID())
				{
					return;
				}

				pSteamFriends->SetRichPresence("status", status);
			}

			const char* CUser::GetStatus() const
			{
				ISteamFriends* pSteamFriends = SteamFriends();
				if (!pSteamFriends)
				{
					return nullptr;
				}
				return pSteamFriends->GetFriendRichPresence(m_id, "status");
			}

			ITexture* CUser::GetAvatar(EAvatarSize size) const
			{
				int imageId = -1;
				ISteamFriends* pSteamFriends = SteamFriends();
				if (pSteamFriends)
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
					return nullptr;
				}

				uint32 width, height;
				if (!pSteamUtils->GetImageSize(imageId, &width, &height))
				{
					return nullptr;
				}

				int imageSize = width*  height*  4;
				std::vector<unsigned char> imageBuffer;
				imageBuffer.resize(imageSize);
				if (!pSteamUtils->GetImageRGBA(imageId, imageBuffer.data(), imageSize))
				{
					return nullptr;
				}

				ETEX_Format format = eTF_R8G8B8A8;
				int textureId = gEnv->pRenderer->UploadToVideoMemory(imageBuffer.data(), width, height, format, format, 1);

				return gEnv->pRenderer->EF_GetTextureByID(textureId);
			}
		}
	}
}