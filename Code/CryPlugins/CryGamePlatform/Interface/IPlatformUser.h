#pragma once

#include <CryRenderer/ITexture.h>

namespace Cry
{
	namespace GamePlatform
	{
		//! Interface to access information on a specific user
		struct IUser
		{
			//! Unique identifier for a user on the specific platform
			using Identifier = uint64;
			
			//! Determines the size of a user's avatar, used when retrieving the avatar as a texture (see GetAvatar)
			enum class EAvatarSize
			{
				Large,
				Medium,
				Small
			};

			virtual ~IUser() {}

			//! Gets the public nickname of the user
			virtual const char* GetNickname() const = 0;
			//! Gets the unique platform-specific identifier of the user
			virtual Identifier GetIdentifier() const = 0;
			//! Sets the user's rich status
			virtual void SetStatus(const char* szStatus) = 0;
			//! Gets the user's rich status
			virtual const char* GetStatus() const = 0;
			//! Gets the user's avatar of the requested size as a texture
			virtual ITexture* GetAvatar(EAvatarSize size) const = 0;
		};
	}
}