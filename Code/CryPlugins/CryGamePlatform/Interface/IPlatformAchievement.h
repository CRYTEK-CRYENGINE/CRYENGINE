#pragma once

namespace Cry
{
	namespace GamePlatform
	{
		//! Representation of an achievement that is registered on the platform's backend
		struct IAchievement
		{
			virtual ~IAchievement() {}

			//! Retrieves the name of the achievement
			virtual const char* GetName() const = 0;
			//! Checks whether the user has completed the achievement
			virtual bool IsCompleted() const = 0;
			//! Resets progress of the achievement
			virtual bool Reset() = 0;
			//! Unlocks the achievement
			virtual bool Achieve() = 0;
		};
	}
}