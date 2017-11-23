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
				return SteamFriends()->GetFriendPersonaName(m_id);
			}

			IUser::Identifier CUser::GetIdentifier() const
			{
				return m_id.ConvertToUint64();
			}

			void CUser::SetStatus(const char* status)
			{
				if (m_id != SteamUser()->GetSteamID())
					return;

				SteamFriends()->SetRichPresence("status", status);
			}

			const char* CUser::GetStatus() const
			{
				return SteamFriends()->GetFriendRichPresence(m_id, "status");
			}

			ITexture* CUser::GetAvatar(EAvatarSize size) const
			{
				int imageId = -1;
				switch (size)
				{
					case EAvatarSize::Large:
						imageId = SteamFriends()->GetLargeFriendAvatar(m_id);
						break;
					case EAvatarSize::Medium:
						imageId = SteamFriends()->GetMediumFriendAvatar(m_id);
						break;
					case EAvatarSize::Small:
						imageId = SteamFriends()->GetSmallFriendAvatar(m_id);
						break;
				}

				if (imageId == -1)
				{
					CryWarning(VALIDATOR_MODULE_SYSTEM, VALIDATOR_WARNING, "[Steam] Failed to load avatar");
					return nullptr;
				}

				uint32 width, height;
				if (!SteamUtils()->GetImageSize(imageId, &width, &height))
				{
					return nullptr;
				}

				int imageSize = width*  height*  4;
				std::vector<unsigned char> imageBuffer;
				imageBuffer.resize(imageSize);
				if (!SteamUtils()->GetImageRGBA(imageId, imageBuffer.data(), imageSize))
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