// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

/*************************************************************************
-------------------------------------------------------------------------
Description:
Network-syncs the actor entity ID being targeted by a player
**************************************************************************/

#ifndef __PLAYERPLUGIN_CURRENTLYTARGETTING_H__
#define __PLAYERPLUGIN_CURRENTLYTARGETTING_H__

#include "PlayerPlugin.h"
#include "Audio/AudioSignalPlayer.h"

class CPlayerPlugin_CurrentlyTargetting : public CPlayerPlugin
{
	public:
		SET_PLAYER_PLUGIN_NAME(CPlayerPlugin_CurrentlyTargetting);

		CPlayerPlugin_CurrentlyTargetting();

		ILINE EntityId GetCurrentTargetEntityId() const
		{
			return m_currentTarget;
		}

		ILINE const float GetCurrentTargetTime() const
		{
			return m_currentTargetTime;
		}

		virtual void NetSerialize(TSerialize ser, EEntityAspects aspect, uint8 profile, int flags);
	private:
		virtual void Update(const float dt);
		virtual void HandleEvent(EPlayerPlugInEvent theEvent, void * data);
		virtual void Enter(CPlayerPluginEventDistributor* pEventDist);
		virtual void Leave();

		CAudioSignalPlayer m_targetedSignal;
		EntityId m_currentTarget;
		float m_currentTargetTime;
		bool m_bTargetingLocalPlayer;
};

#endif // __PLAYERPLUGIN_CURRENTLYTARGETTING_H__

