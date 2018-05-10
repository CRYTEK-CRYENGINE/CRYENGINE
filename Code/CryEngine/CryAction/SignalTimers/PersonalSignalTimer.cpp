// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

/********************************************************************
   ---------------------------------------------------------------------
   File name:   PersonalSignalTimer.cpp
   $Id$
   $DateTime$
   Description: Manager per-actor signal timers
   ---------------------------------------------------------------------
   History:
   - 07:05:2007 : Created by Ricardo Pillosu

 *********************************************************************/
#include "StdAfx.h"
#include "PersonalSignalTimer.h"
#include "SignalTimers.h"
#include "IUIDraw.h"
#include <CryAISystem/IAIObject.h>
// Description:
//   Constructor
// Arguments:
//
// Return:
//
CPersonalSignalTimer::CPersonalSignalTimer(CSignalTimer* pParent) :
	m_bInit(false),
	m_pParent(pParent),
	m_EntityId(0),
	m_fRateMin(4.0f),
	m_fRateMax(6.0f),
	m_fTimer(0.0f),
	m_bEnabled(false),
	m_fTimerSinceLastReset(0.0f),
	m_iSignalsSinceLastReset(0)
{
	CRY_ASSERT(pParent != NULL);

	m_pDefaultFont = gEnv->pCryFont->GetFont("default");
	CRY_ASSERT(m_pDefaultFont);
}

// Description:
//   Destructor
// Arguments:
//
// Return:
//
CPersonalSignalTimer::~CPersonalSignalTimer()
{
	SetListener(false);
}

// Description:
//
// Arguments:
//
// Return:
//
bool CPersonalSignalTimer::Init(EntityId Id, const char* sSignal)
{
	CRY_ASSERT(m_bInit == false);
	CRY_ASSERT(sSignal != NULL);

	m_EntityId = Id;
	m_sSignal = sSignal;
	m_bInit = true;
	SetListener(true);

	return(m_bInit);
}

// Description:
//
// Arguments:
//
// Return:
//
bool CPersonalSignalTimer::Update(float fElapsedTime, uint32 uDebugOrder)
{
	CRY_ASSERT(m_bInit == true);

	bool bRet = true;
	if (m_bEnabled == true)
	{
		m_fTimer -= fElapsedTime;
		m_fTimerSinceLastReset += fElapsedTime;

		if (m_fTimer < 0.0f)
		{
			SendSignal();
			Reset();
		}
	}

	if (uDebugOrder > 0)
	{
		DebugDraw(uDebugOrder);
	}

	return(bRet);
}

// Description:
//
// Arguments:
//
// Return:
//
void CPersonalSignalTimer::ForceReset(bool bAlsoEnable)
{
	CRY_ASSERT(m_bInit == true);

	m_fTimerSinceLastReset = 0.0f;
	m_iSignalsSinceLastReset = 0;
	Reset(bAlsoEnable);
}

// Description:
//
// Arguments:
//
// Return:
//
void CPersonalSignalTimer::OnProxyReset()
{
	// Reset listener
	SetListener(true);
}

// Description:
//
// Arguments:
//
// Return:
//
void CPersonalSignalTimer::Reset(bool bAlsoEnable)
{
	CRY_ASSERT(m_bInit == true);

	if (m_fRateMin < m_fRateMax)
	{
		m_fTimer = cry_random(m_fRateMin, m_fRateMax);
	}
	else
	{
		m_fTimer = m_fRateMin;
	}

	SetEnabled(bAlsoEnable);
}

// Description:
//
// Arguments:
//
// Return:
//
EntityId CPersonalSignalTimer::GetEntityId() const
{
	CRY_ASSERT(m_bInit == true);

	return(m_EntityId);
}

// Description:
//
// Arguments:
//
// Return:
//
const string& CPersonalSignalTimer::GetSignalString() const
{
	CRY_ASSERT(m_bInit == true);

	return(m_sSignal);
}

// Description:
//
// Arguments:
//
// Return:
//
IEntity* CPersonalSignalTimer::GetEntity()
{
	CRY_ASSERT(m_bInit == true);

	return(gEnv->pEntitySystem->GetEntity(m_EntityId));
}

// Description:
//
// Arguments:
//
// Return:
//
IEntity const* CPersonalSignalTimer::GetEntity() const
{
	CRY_ASSERT(m_bInit == true);

	return(gEnv->pEntitySystem->GetEntity(m_EntityId));
}

// Description:
//
// Arguments:
//
// Return:
//
void CPersonalSignalTimer::SetRate(float fNewRateMin, float fNewRateMax)
{
	CRY_ASSERT(m_bInit == true);
	CRY_ASSERT(fNewRateMin > 0.0f);
	CRY_ASSERT(fNewRateMax > 0.0f);

	m_fRateMin = fNewRateMin;
	m_fRateMax = max(fNewRateMin, fNewRateMax);
}

// Description:
//
// Arguments:
//
// Return:
//
void CPersonalSignalTimer::SendSignal()
{
	CRY_ASSERT(m_bInit == true);

	IEntity* pEntity = GetEntity();
	if (pEntity && gEnv->pAISystem)
	{
		IAISignalExtraData* pData = gEnv->pAISystem->CreateSignalExtraData();
		pData->iValue = ++m_iSignalsSinceLastReset;
		pData->fValue = m_fTimerSinceLastReset;

		gEnv->pAISystem->SendSignal(SIGNALFILTER_SENDER, 1, m_sSignal, pEntity->GetAI(), pData);
	}
}

// Description:
//
// Arguments:
//
// Return:
//
void CPersonalSignalTimer::SetEnabled(bool bEnabled)
{
	CRY_ASSERT(m_bInit == true);

	if (bEnabled != m_bEnabled)
	{
		m_fTimerSinceLastReset = 0.0f;
		m_iSignalsSinceLastReset = 0;
		m_bEnabled = bEnabled;
		if (m_pParent->GetDebug() == true)
		{
			gEnv->pLog->Log(
			  "PersonalSignalTimer [%d]: Signal [%s] is %s",
			  m_EntityId,
			  m_sSignal.c_str(),
			  (bEnabled) ? "enabled" : "disabled");
		}
	}
}

// Description:
//
// Arguments:
//
// Return:
//
void CPersonalSignalTimer::DebugDraw(uint32 uOrder) const
{
	CRY_ASSERT(m_bInit == true);

	IUIDraw* pUI = CCryAction::GetCryAction()->GetIUIDraw();
	float x = 120.0f;
	float y = 100.0f + (float(uOrder) * 10.0f);
	float r = 0.0f;
	float g = 8.0f;
	float b = 0.0f;

	char txt[512] = "\0";
	if (GetEntity())
		cry_sprintf(txt, "%s > %s: %0.1f / %0.1f", GetEntity()->GetName(), m_sSignal.c_str(), m_fTimer, m_fRateMax);

	if (m_bEnabled == false)
	{
		r = 8.0f;
		g = b = 0.0f;
	}
	else if (m_fTimer < 0.5f)
	{
		r = g = 8.0f;
		b = 0.0f;
	}

	IRenderAuxText::Draw2dLabel( x,y, 13.0f, ColorF(r,g,b), false, "%s", txt);
}

// Description:
//
// Arguments:
//
// Return:
//
void CPersonalSignalTimer::SetListener(bool bAdd)
{
	IEntity* pEntity = GetEntity();
	;
	if (pEntity)
	{
		IAIObject* pAIObject = pEntity->GetAI();
		if (pAIObject)
		{
			CAIProxy* pAIProxy = (CAIProxy*)pAIObject->GetProxy();
			if (pAIProxy)
			{
				if (bAdd)
					pAIProxy->AddListener(this);
				else
					pAIProxy->RemoveListener(this);
			}
		}
	}
}

// Description:
//
// Arguments:
//
// Return:
//
void CPersonalSignalTimer::OnAIProxyEnabled(bool bEnabled)
{
	if (bEnabled)
	{
		ForceReset();
		SetEnabled(true);
	}
	else
	{
		SetEnabled(false);
	}
}
