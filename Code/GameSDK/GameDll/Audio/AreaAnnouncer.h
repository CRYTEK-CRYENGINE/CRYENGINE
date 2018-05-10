// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

/*************************************************************************
-------------------------------------------------------------------------

	Plays Announcements based upon AreaBox triggers placed in levels

History:
- 25:02:2010		Created by Ben Parbury
*************************************************************************/

#ifndef __AREAANNOUNCER_H__
#define __AREAANNOUNCER_H__

#include "GameRulesModules/IGameRulesRevivedListener.h"
#include <CryCore/Containers/CryFixedArray.h>
#include "Audio/AudioSignalPlayer.h"

#define AREA_ANNOUNCERS 2

class CAreaAnnouncer : IGameRulesRevivedListener
{
public:
	CAreaAnnouncer();
	~CAreaAnnouncer();

	void Init();
	void Reset();

	void Update(const float dt);

	virtual void EntityRevived(EntityId entityId);

#if !defined(_RELEASE)
	static void CmdPlay(IConsoleCmdArgs* pCmdArgs);
	static void CmdReload(IConsoleCmdArgs* pCmdArgs);
#endif

protected:

	struct SAnnouncementArea
	{
#if !defined(_RELEASE)
		const static int k_maxNameLength = 64;
		char m_name[k_maxNameLength];
#endif
		EntityId m_areaProxyId;
		TAudioSignalID m_signal[AREA_ANNOUNCERS];
	};

	bool AnnouncerRequired();
	void LoadAnnouncementArea(const IEntity* pEntity, const char* areaName);

	TAudioSignalID BuildAnnouncement(const EntityId clientId);
	TAudioSignalID GenerateAnnouncement(const int* actorCount, const int k_areaCount, const EntityId clientId);
	int GetAreaAnnouncerTeamIndex(const EntityId clientId);

	const static int k_maxAnnouncementAreas = 16;
	CryFixedArray<SAnnouncementArea, k_maxAnnouncementAreas> m_areaList;
};

#endif // __AREAANNOUNCER_H__