#pragma once

#include "IPlatformAchievement.h"

namespace Cry
{
	namespace GamePlatform
	{
		//! Interface to the platform's remotely stored statistics for the local user
		struct IStatistics
		{
			//! Callbacks used to be notified of certain statistics events
			struct IListener
			{
				virtual void OnAchievementUnlocked(IAchievement& achievement) = 0;
			};

			virtual ~IStatistics() {}

			//! Adds a listener to receive callback on statistics events
			virtual void AddListener(IListener& listener) = 0;
			//! Removes a listener from receiving callback on statistics events
			virtual void RemoveListener(IListener& listener) = 0;
			//! Resets all statistics assigned to the local user
			virtual bool ResetStatistics() = 0;
			//! Resets all achievements assigned to the local user
			virtual bool ResetAchievements() = 0;
			//! Downloads all statistics
			virtual bool Download() = 0;
			//! Uploads changed statistics
			virtual bool Upload() = 0;
			//! Sets the value of a statistic by name
			virtual bool Set(const char* szName, int value) = 0;
			//! Sets the value of a statistic by name
			virtual bool Set(const char* szName, float value) = 0;
			//! Gets the value of a statistic by name
			virtual bool Get(const char* szName, int& value) const = 0;
			//! Gets the value of a statistic by name
			virtual bool Get(const char* szName, float& value) const = 0;
			
			//! Gets a pointer to a specific achievement by name
			virtual IAchievement* GetAchievement(const char* szName) = 0;
			//! Gets a pointer to a specific achievement by platform-specific identifier
			virtual IAchievement* GetAchievement(int id) = 0;
		};
	}
}