#pragma once

#include "IPlatformUser.h"

namespace Cry
{
	namespace GamePlatform
	{
		namespace Steam
		{
			class CUser
				: public IUser
			{
			public:
				CUser(CSteamID id);
				virtual ~CUser();

				// IUser
				virtual const char* GetNickname() const override;
				virtual IUser::Identifier GetIdentifier() const override;

				virtual void SetStatus(const char* status) override;
				virtual const char* GetStatus() const override;

				virtual ITexture* GetAvatar(EAvatarSize size) const override;
				// ~IUser

			protected:
				CSteamID m_id;
			};
		}
	}
}