// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#ifndef __PLAYERPLUGIN_INTERACTIVE_ENTITY_MONITOR_H__
#define __PLAYERPLUGIN_INTERACTIVE_ENTITY_MONITOR_H__

#include "PlayerPlugin.h"

class CPlayerPlugin_InteractiveEntityMonitor : public CPlayerPlugin
{
public:
	SET_PLAYER_PLUGIN_NAME(CPlayerPlugin_InteractiveEntityMonitor);

	CPlayerPlugin_InteractiveEntityMonitor();
	~CPlayerPlugin_InteractiveEntityMonitor();

	static void PrecacheLevel();

private:
	enum EInteractiveEntityStatus
	{
		EIES_Highlighted			= BIT(0),
		EIES_ShootToInteract	= BIT(1),
	};

	typedef std::pair<EntityId, uint8> InteractiveEntityStatus;
	typedef std::list< InteractiveEntityStatus > InteractiveEntityList;

	virtual void Enter(CPlayerPluginEventDistributor* pEventDist);
	virtual void Update(const float dt);
	virtual void HandleEvent(EPlayerPlugInEvent theEvent, void * data);

	void Register(IEntity* pEntity, uint8 initialFlags);
	void Unregister(IEntity* pEntity);
	void EnableHighlighting(bool enable);

	bool m_bEnabled;

	InteractiveEntityList m_interactiveEntityList;

#ifndef _RELEASE
	typedef std::map<int, string> InteractiveEntityDebugMap;
	InteractiveEntityDebugMap m_debugMap;
#endif //_RELEASE
	
	static ColorF m_silhouetteInteractColor;
	static ColorF m_silhouetteShootColor;
	Vec3 m_playerPrevPos;
	float m_timeUntilRefresh;
};

#endif // __PLAYERPLUGIN_INTERACTIVE_ENTITY_MONITOR_H__

