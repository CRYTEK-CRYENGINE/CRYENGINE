// Copyright 2001-2019 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

// *INDENT-OFF* - <hard to read code and declarations due to inconsistent indentation>

namespace Cry
{
	namespace UDR
	{	
		struct ITreeManager;
		struct INodeStack;
		struct IRecursiveSyncObject;

		struct IUDRSystem
		{
			struct SConfig
			{
				//! Specifies if, while in Sandbox, UDR data should be reset when user switches to game mode.
				bool resetDataOnSwitchToGameMode = false;
				
				// Pack together editor configuration parameters (if there are enough).
			};
			
			virtual ~IUDRSystem() = default;
			
			//! Initializes the UDR system. Should be called after the constructor and before any other function.
			//!\return True if successfully initialized and ready to be used. False otherwise.
			virtual bool                  Initialize() = 0;
			
			//! Clears all stored data by the UDR system including recorded data. Does not modify the configuration of the system.
			virtual void                  Reset() = 0;
			
			//! Updates the UDR system. Should be called exactly once per frame.
			//! \param frameStartTime Start time of the frame.
			//! \param frameDeltaTime Duration of the frame.
			virtual void                  Update(const CTimeValue frameStartTime, const float frameDeltaTime) = 0;
		
			virtual ITreeManager&         GetTreeManager() = 0;
			virtual INodeStack&           GetNodeStack() = 0;
			virtual IRecursiveSyncObject& GetRecursiveSyncObject() = 0;
					
			//! Set UDR system configuration.
			//! \param config Configuration to be set.
			//! \return True if successfully set. False otherwise.
			virtual bool                  SetConfig(const SConfig& config) = 0;

			//! Get current configuration of the UDR System.
			//! \return Current configuration.
			virtual const SConfig&        GetConfig() const = 0;

		};

	}
}

