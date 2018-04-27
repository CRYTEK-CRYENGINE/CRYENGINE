// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "PhysicsSync.h"
#include <CryPhysics/IPhysics.h>
#include "GameChannel.h"
#include "CryAction.h"
#include "IActorSystem.h"
#include <CryAction/IDebugHistory.h>
#include "NetworkCVars.h"

static const int RTT_LOOPS = 2; // how many round trips to include in ping averaging

class CPhysicsSync::CDebugTimeline
{
public:
	void AddEvent(CTimeValue remote, CTimeValue local, CTimeValue now)
	{
		SState state = { remote, local, now };
		m_states.push_back(state);
		Render();
	}

	void Render()
	{
		CTimeValue now = gEnv->pTimer->GetAsyncTime();
		CTimeValue old = now - 1.0f;
		while (!m_states.empty() && m_states.front().now < old)
			m_states.pop_front();

		IPersistantDebug* pPD = CCryAction::GetCryAction()->GetIPersistantDebug();
		pPD->Begin("PhysSync", true);

		float yLocal = 550;
		float yRemote = 500;
		float yNow = 525;
#define TIME_TO_X(tm)             (((tm) - now).GetSeconds() + 1) * 780.0f + 10.0f
#define MARK_AT(x, y, clr, tmout) pPD->Add2DLine((x) - 10, (y) - 10, (x) + 10, (y) + 10, (clr), tmout); pPD->Add2DLine((x) - 10, (y) + 10, (x) + 10, (y) - 10, (clr), tmout)
		for (std::deque<SState>::iterator it = m_states.begin(); it != m_states.end(); ++it)
		{
			float xrem = TIME_TO_X(it->remote);
			float xloc = TIME_TO_X(it->local);
			float xnow = TIME_TO_X(it->now);
			MARK_AT(xrem, yRemote, ColorF(1, 1, 1, 1), 60);
			MARK_AT(xloc, yLocal, ColorF(1, 0, 0, 1), 60);
			MARK_AT(xnow, yNow, ColorF(0, 1, 0, 1), 60);
			pPD->Add2DLine(xnow, yNow, xrem, yRemote, ColorF(1, 1, 1, .5), 60);
			pPD->Add2DLine(xnow, yNow, xloc, yLocal, ColorF(1, 1, 1, .5), 60);
		}
	}

private:
	struct SState
	{
		CTimeValue remote;
		CTimeValue local;
		CTimeValue now;
	};
	std::deque<SState> m_states;
};

CPhysicsSync::CPhysicsSync(CGameChannel* pParent)
	: m_pParent(pParent)
	, m_pWorld(gEnv->pPhysicalWorld)
	, m_physPrevRemoteTime(0.0f)
	, m_pingEstimate(0.0f)
	, m_physEstimatedLocalLaggedTime(0.0f)
	, m_epochWhen(0.0f)
	, m_physEpochTimestamp(0)
	, m_ignoreSnapshot(false)
	, m_catchup(false)
{
	m_pDebugHistory = CCryAction::GetCryAction()->CreateDebugHistoryManager();
}

CPhysicsSync::~CPhysicsSync()
{
	if (m_pDebugHistory)
		m_pDebugHistory->Release();
}

static float CalcFrameWeight(float smoothing)
{
	float wt = clamp_tpl(gEnv->pTimer->GetFrameTime() * smoothing, 0.0f, 1.0f);
	return wt;
}

bool CPhysicsSync::OnPacketHeader(CTimeValue tm)
{
	// these may change, but setup some default behaviors now
	m_catchup = true;
	m_ignoreSnapshot = false;

	// quick check in case we're full
	if (m_pastPings.Full())
		m_pastPings.Pop();

	// add the current ping to our list of past ping samples
	INetChannel* pNetChannel = m_pParent->GetNetChannel();
	SPastPing curPing = { gEnv->pTimer->GetAsyncTime(), pNetChannel->GetPing(true) };
	m_pastPings.Push(curPing);

	// find the average ping so far
	float sumPing = 0;
	for (PingQueue::SIterator it = m_pastPings.Begin(); it != m_pastPings.End(); ++it)
		sumPing += it->value;
	CTimeValue averagePing = sumPing / m_pastPings.Size();

	// find how much the average ping has changed from our estimate, in order to adjust later
	CTimeValue deltaPing;
	CTimeValue prevPingEstimate = m_pingEstimate;
	if (m_pingEstimate != 0.0f)
	{
		float pingWeight = CalcFrameWeight(CNetworkCVars::Get().PhysSyncPingSmooth);
		m_pingEstimate = (1.0f - pingWeight) * m_pingEstimate.GetSeconds() + pingWeight * averagePing.GetSeconds();
	}
	else
	{
		m_pingEstimate = averagePing;
	}

	CTimeValue oneMS;
	oneMS.SetMilliSeconds(1);
	if (m_pingEstimate != 0.0f)
	{
		CTimeValue clampAmt = m_pingEstimate.GetSeconds() * 0.5f * 0.5f;
		if (oneMS < clampAmt)
			clampAmt = oneMS;
		deltaPing = CLAMP(m_pingEstimate - prevPingEstimate, -clampAmt, clampAmt);
		m_pingEstimate = prevPingEstimate + deltaPing;
	}
	else
		deltaPing = 0.0f;

	// expunge any pings that are now older than RTT_LOOPS round trip times
	CTimeValue oldTime = curPing.when - averagePing.GetSeconds() * RTT_LOOPS;
	while (m_pastPings.Size() > 1 && m_pastPings.Front().when < oldTime)
		m_pastPings.Pop();

	// current remote time is tm
	CTimeValue physCurRemoteTime = tm;
	// if we've not yet gotten a previous remote time, estimate it at half a ping ago
	if (m_physPrevRemoteTime == 0.0f)
		m_physPrevRemoteTime = physCurRemoteTime - 0.5f * averagePing.GetSeconds();
	CTimeValue physDeltaRemoteTime = physCurRemoteTime - m_physPrevRemoteTime;
	if (physDeltaRemoteTime < oneMS)
	{
		m_ignoreSnapshot = true;
		m_catchup = false;
		return true;
	}

	double timeGranularity = m_pWorld->GetPhysVars()->timeGranularity;

	// estimate how far we need to go back... the main indicator is the physics delta time
	// but we also adjust to the average ping changing
	CTimeValue stepForward = physDeltaRemoteTime + deltaPing.GetSeconds() * 0.5f;

	// now estimate what the local timestamp should be
	int curTimestamp = m_pWorld->GetiPhysicsTime();
	int translatedTimestamp = -1;
	bool resetEstimate = true;
	if (m_physEpochTimestamp != 0)
		resetEstimate = stepForward > 0.25f || stepForward < 0.0f;
	if (!resetEstimate)
	{
		m_physEstimatedLocalLaggedTime += stepForward;
	}
	else
	{
emergency_reset:
		m_physEstimatedLocalLaggedTime = -m_pingEstimate;
		m_physEpochTimestamp = curTimestamp;
		m_epochWhen = gEnv->pTimer->GetAsyncTime();
	}
	translatedTimestamp = int(m_physEpochTimestamp + m_physEstimatedLocalLaggedTime.GetSeconds() / timeGranularity + 0.5);
	if (translatedTimestamp >= curTimestamp)
	{
		m_catchup = false;
		translatedTimestamp = curTimestamp;
		if (translatedTimestamp > curTimestamp)
			CryLog("[phys] time compression occurred (%d ticks)", curTimestamp - translatedTimestamp);
	}
	else
	{
		m_catchup = stepForward<0.125f && stepForward> 0.0f;
	}

	//CRY_ASSERT(translatedTimestamp >= 0);
	if (translatedTimestamp < 0)
		translatedTimestamp = 0;
	CRY_ASSERT(translatedTimestamp <= curTimestamp);

	if (!resetEstimate && (curTimestamp - translatedTimestamp) * timeGranularity > 0.25f + m_pingEstimate.GetSeconds())
	{
		CryLog("[phys] way out of sync (%.2f seconds)... performing emergency reset", float((curTimestamp - translatedTimestamp) * timeGranularity));
		m_catchup = false;
		resetEstimate = true;
		goto emergency_reset;
	}

	m_pWorld->SetiSnapshotTime(translatedTimestamp, 0);
	m_pWorld->SetiSnapshotTime(curTimestamp, 1);
	// TODO: SnapTime()
	m_pWorld->SetiSnapshotTime(curTimestamp, 2);

	// store the things that we need to keep
	m_physPrevRemoteTime = physCurRemoteTime;

	if (!resetEstimate)
	{
		// slowly move the epoch time versus the current time towards -pingEstimate/2
		// avoids a side-case where the local time at epoch startup can completely annihilate any backstepping we might do
		int deltaTimestamp = curTimestamp - m_physEpochTimestamp;
		m_physEpochTimestamp = curTimestamp;
		m_epochWhen = gEnv->pTimer->GetAsyncTime();
		m_physEstimatedLocalLaggedTime -= CTimeValue(deltaTimestamp * timeGranularity);

		float lagWeight = CalcFrameWeight(CNetworkCVars::Get().PhysSyncLagSmooth);
		m_physEstimatedLocalLaggedTime = (1.0f - lagWeight) * m_physEstimatedLocalLaggedTime.GetSeconds() - lagWeight * 0.5f * m_pingEstimate.GetSeconds();
	}

	if (CNetworkCVars::Get().PhysDebug & 2)
	{
		if (!m_pDBTL.get())
			m_pDBTL.reset(new CDebugTimeline());

		CTimeValue loc = m_epochWhen + m_physEstimatedLocalLaggedTime;
		CTimeValue rem = loc - m_pingEstimate;
		CTimeValue now = gEnv->pTimer->GetAsyncTime();
		m_pDBTL->AddEvent(rem, loc, now);
	}

	OutputDebug((float)physDeltaRemoteTime.GetMilliSeconds(), (float)deltaPing.GetMilliSeconds(), (float)averagePing.GetMilliSeconds(), (float)curPing.value * 1000.0f, (float)stepForward.GetMilliSeconds(), ((float)curTimestamp - (float)translatedTimestamp) * (float)timeGranularity * 1000.0f);

	return true;
}

bool CPhysicsSync::OnPacketFooter()
{
	IEntitySystem* pEntitySystem = gEnv->pEntitySystem;

	IPersistantDebug* pDbg = 0;
	if (CNetworkCVars::Get().PhysDebug)
	{
		pDbg = CCryAction::GetCryAction()->GetIPersistantDebug();
	}

	if (!m_updatedEntities.empty())
	{
		pe_params_flags set_update;
		set_update.flagsOR = pef_update;

		while (!m_updatedEntities.empty())
		{
			IEntity* pEntity = pEntitySystem->GetEntity(m_updatedEntities.back());
			m_updatedEntities.pop_back();

			//temp code
			continue;

			if (!pEntity)
				continue;
			IPhysicalEntity* pPhysicalEntity = pEntity->GetPhysics();
			if (!pPhysicalEntity)
				continue;

			if (CNetworkCVars::Get().PhysDebug)
			{
				pDbg->Begin(string(pEntity->GetName()) + "_phys0", true);
				pe_status_pos p;
				pPhysicalEntity->GetStatus(&p);
				pDbg->AddSphere(p.pos, 0.5f, ColorF(1, 0, 0, 1), 1.0f);
			}

			// TODO: Need an elegant way to detect physicalization
			IActor* pClientActor = CCryAction::GetCryAction()->GetClientActor();
			if (pClientActor && pClientActor->GetEntity() == pEntity)
			{
				pe_params_flags flags;
				flags.flagsOR = pef_log_collisions;
				pPhysicalEntity->SetParams(&flags);
			}

			pe_params_flags flags;
			pPhysicalEntity->GetParams(&flags);
			if ((flags.flags & pef_update) == 0)
			{
				pPhysicalEntity->SetParams(&set_update);
				m_clearEntities.push_back(pPhysicalEntity);
			}
		}

		// more temp code
		return true;

#if 0
		float step = min(0.3f, m_deltaTime * m_pWorld->GetPhysVars()->timeGranularity);
		do
		{
			float thisStep = min(step, 0.1f);
			//			CryLogAlways( "step %f", thisStep );
			m_pWorld->TimeStep(thisStep, ent_flagged_only);
			step -= thisStep;
		}
		while (step > 0.0001f);

		pe_params_flags clear_update;
		clear_update.flagsAND = ~pef_update;
		if (CNetworkCVars::Get().PhysDebug && !m_clearEntities.empty())
			pDbg->Begin("final_phys", true);
		while (!m_clearEntities.empty())
		{
			if (CNetworkCVars::Get().PhysDebug)
			{
				pe_status_pos p;
				m_clearEntities.back()->GetStatus(&p);
				pDbg->AddSphere(p.pos, 0.5f, ColorF(0, 0, 1, 1), 1.0f);
			}

			m_clearEntities.back()->SetParams(&clear_update);
			m_clearEntities.pop_back();
		}
#endif
	}

	return true;
}

void CPhysicsSync::OutputDebug(float deltaPhys, float deltaPing, float averagePing, float ping, float stepForward, float deltaTimestamp)
{
	if (!CNetworkCVars::Get().PhysDebug)
	{
		for (size_t i = 0; i < m_vpHistories.size(); i++)
			m_vpHistories[i]->SetVisibility(false);
		return;
	}

	if (!m_pDebugHistory)
		m_pDebugHistory = gEnv->pGameFramework->CreateDebugHistoryManager();

	int nHist = 0;
	IDebugHistory* pHist;

#define HISTORY(val)                                                                         \
  if (m_vpHistories.size() <= nHist)                                                         \
  {                                                                                          \
    pHist = m_pDebugHistory->CreateHistory( # val);                                          \
    int y = nHist % 3;                                                                       \
    int x = nHist / 3;                                                                       \
    pHist->SetupLayoutAbs((float)(50 + x * 210), (float)(110 + y * 160), 200.f, 150.f, 5.f); \
    pHist->SetupScopeExtent(-360, +360, -0.01f, +0.01f);                                     \
    m_vpHistories.push_back(pHist);                                                          \
  }                                                                                          \
  else                                                                                       \
    pHist = m_vpHistories[nHist];                                                            \
  nHist++;                                                                                   \
  pHist->AddValue(val);                                                                      \
  pHist->SetVisibility(true)

	HISTORY(deltaPhys);
	HISTORY(deltaPing);
	//	HISTORY(averagePing);
	HISTORY(ping);
	float pingEstimate = m_pingEstimate.GetMilliSeconds();
	HISTORY(pingEstimate);
	HISTORY(deltaTimestamp);
	HISTORY(stepForward);
}
