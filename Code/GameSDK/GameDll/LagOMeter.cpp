// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

/*************************************************************************
	-------------------------------------------------------------------------
	$Id$
	$DateTime$
	Description: Logs data about client latency

	-------------------------------------------------------------------------
	History:
	- 30:11:2011  : Created by Martin Sherburn

*************************************************************************/

#include "StdAfx.h"
#include "LagOMeter.h"

#if USE_LAGOMETER

#define PERCEIVED_LAG_NOTIFICATION_TIMEOUT		(3.0f)

#include "IGameRulesSystem.h"
#include "EquipmentLoadout.h"
#include "Item.h"
#include "TelemetryCollector.h"
#include <CryAction/IDebugHistory.h>
#include <CrySystem/Profilers/IStatoscope.h>

//========================================================================
CLagOMeter::CLagOMeter()
	: m_rollingHitId(1)
	, m_pDebugHistoryManager(NULL)
	, m_pUploadGraph(NULL)
	, m_pDownloadGraph(NULL)
	, m_buttonHeld(false)
	, m_perceivedLagEventRecorded(false)
	, m_timeSinceLastPerceivedLagEvent(0.0f)
{
	UpdateBandwidthGraphVisibility();

	if(gEnv->pInput)
	{
		gEnv->pInput->AddEventListener(this);
	}
}

CLagOMeter::~CLagOMeter()
{
	SAFE_RELEASE(m_pDebugHistoryManager);


	if(gEnv->pInput)
	{
		gEnv->pInput->RemoveEventListener(this);
	}
}

void CLagOMeter::OnClientRequestHit(HitInfo &hitInfo)
{
	CRY_ASSERT(hitInfo.shooterId == gEnv->pGameFramework->GetClientActorId());
	hitInfo.lagOMeterHitId = m_rollingHitId;
	m_hitHistory[m_rollingHitId].requestTime = gEnv->pTimer->GetAsyncTime();
	++m_rollingHitId;
	if (m_rollingHitId >= HIT_HISTORY_COUNT)
	{
		// Hit Id 0 is never used, so we can identify it as an invalid hit id
		m_rollingHitId = 1;
	}
}

void CLagOMeter::OnClientReceivedKill(const CActor::KillParams &killParams)
{
	CTimeValue currentTime = gEnv->pTimer->GetAsyncTime();
	if (killParams.shooterId == gEnv->pGameFramework->GetClientActorId())
	{
		// Try to find the corresponding request hit info to see how far lagged behind we are
		int index = killParams.lagOMeterHitId;
		if (index > 0 && index < HIT_HISTORY_COUNT)
		{
			SHitRequestHistory& item = m_hitHistory[index];
			if (item.requestTime.GetValue() != 0)
			{
				CTimeValue diff = currentTime - item.requestTime;
				CTelemetryCollector* pTelemetryCollector = (CTelemetryCollector*)g_pGame->GetITelemetryCollector();
				if (pTelemetryCollector)
				{
					pTelemetryCollector->LogEvent("Kill Latency", diff.GetMilliSeconds());
				}
			}
		}
	}
}

void CLagOMeter::UpdateBandwidthGraphVisibility()
{
	EBandwidthGraph graphs = eBG_None;
	switch (g_pGameCVars->net_graph)
	{
	case 1:
		graphs = eBG_Download;
		break;
	case 2:
		graphs = eBG_Upload;
		break;
	case 3:
		graphs = eBG_Both;
		break;
	}
	if (graphs == eBG_None)
	{
		m_pUploadGraph = NULL;
		m_pDownloadGraph = NULL;
		SAFE_RELEASE(m_pDebugHistoryManager);
	}
	else
	{
		const float leftX = 0.3f;
		const float topY = 0.7f;
		const float width = 0.4f;
		const float height = 0.2f;
		const float margin = 0.0f;
		if (!m_pDebugHistoryManager)
		{
			m_pDebugHistoryManager = g_pGame->GetIGameFramework()->CreateDebugHistoryManager();
			m_pUploadGraph = m_pDebugHistoryManager->CreateHistory("Upload", "Upload (kbits/s)");
			m_pUploadGraph->SetupLayoutRel(leftX, topY, width, height, margin);
			m_pUploadGraph->SetupScopeExtent(0.0f, 1024.0f, 0.0f, 1.0f);
			m_pDownloadGraph = m_pDebugHistoryManager->CreateHistory("Download", "Download (kbits/s)");
			m_pDownloadGraph->SetupLayoutRel(leftX, topY, width, height, margin);
			m_pDownloadGraph->SetupScopeExtent(0.0f, 1024.0f, 0.0f, 1.0f);
		}
		m_pUploadGraph->SetVisibility((graphs & eBG_Upload) != 0);
		m_pDownloadGraph->SetVisibility((graphs & eBG_Download) != 0);
		if (graphs == eBG_Both)
		{
			// If both graphs are shown then lay them out one above the other
			m_pUploadGraph->SetupLayoutRel(leftX, topY - 0.02f, width, height / 2.0f, margin);
			m_pDownloadGraph->SetupLayoutRel(leftX, topY + height / 2.0f, width, height / 2.0f, margin);
		}
		else
		{
			m_pUploadGraph->SetupLayoutRel(leftX, topY, width, height, margin);
			m_pDownloadGraph->SetupLayoutRel(leftX, topY, width, height, margin);
		}
	}
}

void CLagOMeter::Update() PREFAST_SUPPRESS_WARNING (6262)
{
	if (m_pDebugHistoryManager)
	{
		static CTimeValue lastSampleTime;
		static uint64 bandwidthSentSample = 0;
		static uint64 bandwidthReceivedSample = 0;
		const CTimeValue timeValue = gEnv->pTimer->GetAsyncTime();
		CTimeValue deltaTime = timeValue - lastSampleTime;
		if (deltaTime.GetSeconds() >= 1.0f)
		{
			INetwork* pNetwork = gEnv->pNetwork;
			SBandwidthStats stats;
			pNetwork->GetBandwidthStatistics(&stats);

			uint64 totalSent = stats.m_total.m_totalBandwidthSent;
			uint64 totalReceived = stats.m_total.m_totalBandwidthRecvd;

			uint64 deltaSent = totalSent - bandwidthSentSample;
			uint64 deltaReceived = totalReceived - bandwidthReceivedSample;
			float sentPerSec = deltaSent / (deltaTime.GetSeconds() * 1024.0f);	// Convert to kbits/s
			float receivedPerSec = deltaReceived / (deltaTime.GetSeconds() * 1024.0f);	// Convert to kbits/s

			m_pUploadGraph->AddValue((float)sentPerSec);
			m_pDownloadGraph->AddValue((float)receivedPerSec);

			lastSampleTime = timeValue;
			bandwidthSentSample = totalSent;
			bandwidthReceivedSample = totalReceived;
		} 
	}
#if CRY_PLATFORM_WINDOWS
	const float stallThreshold = 50;
#else
	const float stallThreshold = 100;
#endif
	CTimeValue currentTime = gEnv->pTimer->GetAsyncTime();
	if (m_prevTime.GetValue() != 0)
	{
		CTimeValue delta = currentTime - m_prevTime;
		if (delta.GetMilliSeconds() > stallThreshold)
		{
			CTelemetryCollector* pTelemetryCollector = (CTelemetryCollector*)g_pGame->GetITelemetryCollector();
			if (!pTelemetryCollector || !pTelemetryCollector->CanLogEvent())
			{
				CryLogAlways("Main Thread Stall [%fms]", delta.GetMilliSeconds());
			}
			else
			{
				pTelemetryCollector->LogEvent("Main Thread Stall", delta.GetMilliSeconds());
			}
		}
	}
	m_prevTime = currentTime;

	if(m_perceivedLagEventRecorded)
	{
		CTelemetryCollector* pTelemetryCollector = (CTelemetryCollector*)g_pGame->GetITelemetryCollector();
		if (pTelemetryCollector && pTelemetryCollector->CanLogEvent())
		{
			pTelemetryCollector->LogEvent("Perceived Lag", 1.0f);
		}
		if (gEnv->pStatoscope)
		{
			gEnv->pStatoscope->AddUserMarker("LagOmeter", "Perceived Lag Event");
		}
		m_perceivedLagEventRecorded = false;
		m_timeSinceLastPerceivedLagEvent = PERCEIVED_LAG_NOTIFICATION_TIMEOUT;
	}

	if(m_timeSinceLastPerceivedLagEvent>0.0f)
	{
		
		static Vec2 textPos(150.0f,200.0f);

		float dt = gEnv->pTimer->GetFrameTime();
		float alpha = m_timeSinceLastPerceivedLagEvent < 1.0f ? m_timeSinceLastPerceivedLagEvent: 1.0f;

		ColorF textCol(1.0f,1.0f,1.0f,alpha);
		IRenderAuxText::Draw2dLabel(textPos.x, textPos.y, 2.0f, textCol, false, "PERCEIVED LAG EVENT RECORDED");

		m_timeSinceLastPerceivedLagEvent -= dt;
	}
}

bool CLagOMeter::OnInputEvent( const SInputEvent &rInputEvent )
{

	// Users register a lag event by pressing...
	// Keyboard: [SHIFT] + L 
	// so this is what we are looking to detect here...
	const EKeyId Button_Held = eKI_XI_ShoulderL;
	const EKeyId Button_Tap = eKI_XI_DPadDown;

	if(rInputEvent.deviceType==eIDT_Gamepad)
	{
		if(rInputEvent.keyId == Button_Held)
		{
			m_buttonHeld = rInputEvent.state == eIS_Down;
		}
		else if(rInputEvent.state==eIS_Released && rInputEvent.keyId == Button_Tap && m_buttonHeld )
		{
			m_perceivedLagEventRecorded = true;
			return true;
		}
	}
	else if(rInputEvent.deviceType==eIDT_Keyboard)
	{
		if(rInputEvent.state==eIS_Released && rInputEvent.keyId == eKI_L && (rInputEvent.modifiers & eMM_LShift))
		{
			m_perceivedLagEventRecorded = true;
			return true;
		}
	}

	return false;
}

#endif // __LAGOMETER_H__
