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
		CTimeValue now = GetGTimer()->GetAsyncTime();
		CTimeValue old = now - 1;
		while (!m_states.empty() && m_states.front().now < old)
			m_states.pop_front();

		IPersistantDebug* pPD = CCryAction::GetCryAction()->GetIPersistantDebug();
		pPD->Begin("PhysSync", true);

		float yLocal = 550;
		float yRemote = 500;
		float yNow = 525;
#define TIME_TO_X(tm)             (((tm) - now).GetSeconds() + 1) * 780 + 10
#define MARK_AT(x, y, clr, tmout) pPD->Add2DLine((x) - 10, (y) - 10, (x) + 10, (y) + 10, (clr), tmout); pPD->Add2DLine((x) - 10, (y) + 10, (x) + 10, (y) - 10, (clr), tmout)
		for (std::deque<SState>::iterator it = m_states.begin(); it != m_states.end(); ++it)
		{
			// Float inaccuracy is fine, debug/profiling
			float xrem = (float)TIME_TO_X(it->remote);
			float xloc = (float)TIME_TO_X(it->local);
			float xnow = (float)TIME_TO_X(it->now);
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
	, m_physPrevRemoteTime(0)
	, m_pingEstimate(0)
	, m_physEstimatedLocalLaggedTime(0)
	, m_epochWhen(0)
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

static mpfloat CalcFrameWeight(const mpfloat& smoothing)
{
	mpfloat wt = CLAMP(GetGTimer()->GetFrameTime().GetSeconds() * smoothing, 0, 1);
	return wt;
}

bool CPhysicsSync::OnPacketHeader(const CTimeValue& tm)
{
	// these may change, but setup some default behaviors now
	m_catchup = true;
	m_ignoreSnapshot = false;

	// quick check in case we're full
	if (m_pastPings.Full())
		m_pastPings.Pop();

	// add the current ping to our list of past ping samples
	INetChannel* pNetChannel = m_pParent->GetNetChannel();
	SPastPing curPing = { GetGTimer()->GetAsyncTime(), pNetChannel->GetPing(true) };
	m_pastPings.Push(curPing);

	// find the average ping so far
	CTimeValue sumPing;
	for (PingQueue::SIterator it = m_pastPings.Begin(); it != m_pastPings.End(); ++it)
		sumPing += it->value;
	CTimeValue averagePing = sumPing / m_pastPings.Size();

	// find how much the average ping has changed from our estimate, in order to adjust later
	CTimeValue deltaPing;
	CTimeValue prevPingEstimate = m_pingEstimate;
	if (m_pingEstimate != 0)
	{
		mpfloat pingWeight = CalcFrameWeight(CNetworkCVars::Get().PhysSyncPingSmooth);
		m_pingEstimate = (1 - pingWeight) * m_pingEstimate + pingWeight * averagePing;
	}
	else
	{
		m_pingEstimate = averagePing;
	}

	CTimeValue oneMS;
	oneMS.SetMilliSeconds(1);
	if (m_pingEstimate != 0)
	{
		CTimeValue clampAmt = m_pingEstimate * "0.5" * "0.5";
		if (oneMS < clampAmt)
			clampAmt = oneMS;
		deltaPing = CLAMP(m_pingEstimate - prevPingEstimate, -clampAmt, clampAmt);
		m_pingEstimate = prevPingEstimate + deltaPing;
	}
	else
		deltaPing.SetSeconds(0);

	// expunge any pings that are now older than RTT_LOOPS round trip times
	CTimeValue oldTime = curPing.when - averagePing.GetSeconds() * RTT_LOOPS;
	while (m_pastPings.Size() > 1 && m_pastPings.Front().when < oldTime)
		m_pastPings.Pop();

	// current remote time is tm
	CTimeValue physCurRemoteTime = tm;
	// if we've not yet gotten a previous remote time, estimate it at half a ping ago
	if (m_physPrevRemoteTime == 0)
		m_physPrevRemoteTime = physCurRemoteTime - "0.5" * averagePing;
	CTimeValue physDeltaRemoteTime = physCurRemoteTime - m_physPrevRemoteTime;
	if (physDeltaRemoteTime < oneMS)
	{
		m_ignoreSnapshot = true;
		m_catchup = false;
		return true;
	}

	// estimate how far we need to go back... the main indicator is the physics delta time
	// but we also adjust to the average ping changing
	CTimeValue stepForward = physDeltaRemoteTime + deltaPing.GetSeconds() * "0.5";

	// now estimate what the local timestamp should be
	CTimeValue curTimestamp = m_pWorld->GetPhysicsTime();
	CTimeValue translatedTimestamp = -1;
	bool resetEstimate = true;
	if (m_physEpochTimestamp != 0)
		resetEstimate = stepForward > "0.25" || stepForward < 0;
	if (!resetEstimate)
	{
		m_physEstimatedLocalLaggedTime += stepForward;
	}
	else
	{
emergency_reset:
		m_physEstimatedLocalLaggedTime = -m_pingEstimate;
		m_physEpochTimestamp = curTimestamp;
		m_epochWhen = GetGTimer()->GetAsyncTime();
	}
	translatedTimestamp = m_physEpochTimestamp + m_physEstimatedLocalLaggedTime;
	if (translatedTimestamp >= curTimestamp)
	{
		m_catchup = false;
		translatedTimestamp = curTimestamp;
		if (translatedTimestamp > curTimestamp)
			CryLog("[phys] time compression occurred (%f seconds)", (float)(curTimestamp - translatedTimestamp).GetSeconds());
	}
	else
	{
		m_catchup = stepForward < "0.125" && stepForward > 0;
	}

	//CRY_ASSERT(translatedTimestamp >= 0);
	if (translatedTimestamp < 0)
		translatedTimestamp.SetSeconds(0);
	CRY_ASSERT(translatedTimestamp <= curTimestamp);

	if (!resetEstimate && (curTimestamp - translatedTimestamp) > m_pingEstimate + "0.25")
	{
		CryLog("[phys] way out of sync (%.2f seconds)... performing emergency reset", (float)(curTimestamp - translatedTimestamp).GetSeconds());
		m_catchup = false;
		resetEstimate = true;
		goto emergency_reset;
	}

	m_pWorld->SetSnapshotTime(translatedTimestamp, 0);
	m_pWorld->SetSnapshotTime(curTimestamp, 1);
	// TODO: SnapTime()
	m_pWorld->SetSnapshotTime(curTimestamp, 2);

	// store the things that we need to keep
	m_physPrevRemoteTime = physCurRemoteTime;

	if (!resetEstimate)
	{
		// slowly move the epoch time versus the current time towards -pingEstimate/2
		// avoids a side-case where the local time at epoch startup can completely annihilate any backstepping we might do
		CTimeValue deltaTimestamp = curTimestamp - m_physEpochTimestamp;
		m_physEpochTimestamp = curTimestamp;
		m_epochWhen = GetGTimer()->GetAsyncTime();
		m_physEstimatedLocalLaggedTime -= deltaTimestamp;

		mpfloat lagWeight = CalcFrameWeight(CNetworkCVars::Get().PhysSyncLagSmooth);
		m_physEstimatedLocalLaggedTime = (1 - lagWeight) * m_physEstimatedLocalLaggedTime - lagWeight * "0.5" * m_pingEstimate;
	}

	if (CNetworkCVars::Get().PhysDebug & 2)
	{
		if (!m_pDBTL.get())
			m_pDBTL.reset(new CDebugTimeline());

		CTimeValue loc = m_epochWhen + m_physEstimatedLocalLaggedTime;
		CTimeValue rem = loc - m_pingEstimate;
		CTimeValue now = GetGTimer()->GetAsyncTime();
		m_pDBTL->AddEvent(rem, loc, now);
	}

	OutputDebug(physDeltaRemoteTime, deltaPing, averagePing, curPing.value, stepForward, (curTimestamp - translatedTimestamp));

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
				pDbg->AddSphere(p.pos, 0.5f, ColorF(1, 0, 0, 1), 1);
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

void CPhysicsSync::OutputDebug(const CTimeValue& deltaPhys, const CTimeValue& deltaPing, const CTimeValue& averagePing, const CTimeValue& ping, const CTimeValue& stepForward, const CTimeValue& deltaTimestamp)
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

	HISTORY(BADF deltaPhys.GetMilliSeconds());
	HISTORY(BADF deltaPing.GetMilliSeconds());
	//	HISTORY(averagePing);
	HISTORY(BADF ping.GetMilliSeconds());
	float pingEstimate = (float)m_pingEstimate.GetMilliSeconds();
	HISTORY(pingEstimate);
	HISTORY(BADF deltaTimestamp.GetMilliSeconds());
	HISTORY(BADF stepForward.GetMilliSeconds());
}
