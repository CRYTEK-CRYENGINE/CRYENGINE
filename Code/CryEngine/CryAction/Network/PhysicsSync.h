// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#ifndef __PHYSICSSYNC_H__
#define __PHYSICSSYNC_H__

#pragma once

#include <CryCore/Containers/MiniQueue.h>

struct IPhysicalWorld;
class CGameChannel;
struct IDebugHistory;
struct IDebugHistoryManager;

class CPhysicsSync
{
public:
	CPhysicsSync(CGameChannel* pParent);
	~CPhysicsSync();

	bool OnPacketHeader(CTimeValue);
	bool       OnPacketFooter();

	CTimeValue GetTime();
	void SetTime(CTimeValue);

	void UpdatedEntity(EntityId id)
	{
		m_updatedEntities.push_back(id);
	}

	bool IgnoreSnapshot() const { return false && m_ignoreSnapshot; }
	bool NeedToCatchup() const  { return m_catchup; }

private:
	void OutputDebug(float deltaPhys, float deltaPing, float averagePing, float ping, float stepForward, float deltaTimestamp);

	CGameChannel*    m_pParent;
	IPhysicalWorld*  m_pWorld;

	static const int MAX_PING_SAMPLES = 128;
	struct SPastPing
	{
		CTimeValue when;
		float      value;
	};
	typedef MiniQueue<SPastPing, MAX_PING_SAMPLES> PingQueue;
	PingQueue m_pastPings;

	class CDebugTimeline;
	std::unique_ptr<CDebugTimeline> m_pDBTL;

	CTimeValue                      m_physPrevRemoteTime;
	CTimeValue                      m_pingEstimate;
	CTimeValue                      m_physEstimatedLocalLaggedTime;
	CTimeValue                      m_epochWhen;
	int                             m_physEpochTimestamp;

	bool                            m_ignoreSnapshot;
	bool                            m_catchup;
	IDebugHistoryManager*           m_pDebugHistory;
	std::vector<IDebugHistory*>     m_vpHistories;

	std::vector<EntityId>           m_updatedEntities;
	std::vector<IPhysicalEntity*>   m_clearEntities;
};

#endif
