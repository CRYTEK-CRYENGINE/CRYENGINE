// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

/********************************************************************
   ---------------------------------------------------------------------
   File name:   SignalTimer.h
   $Id$
   $DateTime$
   Description: Signal entities based on configurable timers
   ---------------------------------------------------------------------
   History:
   - 16:04:2007 : Created by Ricardo Pillosu

 *********************************************************************/
#include "StdAfx.h"
#include "SignalTimers.h"
#include "PersonalSignalTimer.h"

CSignalTimer* CSignalTimer::m_pInstance = NULL;

// Description:
//   Constructor
// Arguments:
//
// Return:
//
CSignalTimer::CSignalTimer()
{
	m_bInit = false;
	m_bDebug = false;
}

// Description:
//   Destructor
// Arguments:
//
// Return:
//
CSignalTimer::~CSignalTimer()
{
	for (uint32 uIndex = 0; uIndex < m_vecPersonalSignalTimers.size(); ++uIndex)
	{
		SAFE_DELETE(m_vecPersonalSignalTimers[uIndex]);
	}

	m_vecPersonalSignalTimers.clear();
}

// Description:
//   Create
// Arguments:
//
// Return:
//
CSignalTimer& CSignalTimer::ref()
{
	CRY_ASSERT(NULL != m_pInstance);
	return(*m_pInstance);
}

// Description:
//   Create
// Arguments:
//
// Return:
//
bool CSignalTimer::Create()
{
	if (NULL == m_pInstance)
	{
		m_pInstance = new CSignalTimer();
	}
	else
	{
		CRY_ASSERT("Trying to Create() the singleton more than once");
	}

	return(m_pInstance != NULL);
}

// Description:
//
// Arguments:
//
// Return:
//
void CSignalTimer::Init()
{
	if (m_bInit == false)
	{
		m_bInit = true;
	}
}

// Description:
//
// Arguments:
//
// Return:
//
void CSignalTimer::Reset()
{
	for (uint32 uIndex = 0; uIndex < m_vecPersonalSignalTimers.size(); ++uIndex)
	{
		m_vecPersonalSignalTimers[uIndex]->ForceReset(false);
	}
}

// Description:
//
// Arguments:
//
// Return:
//
void CSignalTimer::OnEditorSetGameMode(bool bGameMode)
{
	CRY_ASSERT(m_bInit == true);

	if (bGameMode == true)
	{
		Reset();
	}
}

// Description:
//
// Arguments:
//
// Return:
//
void CSignalTimer::OnProxyReset(EntityId IdEntity)
{
	CRY_ASSERT(IdEntity > 0);

	for (uint32 uIndex = 0; uIndex < m_vecPersonalSignalTimers.size(); ++uIndex)
	{
		if (m_vecPersonalSignalTimers[uIndex]->GetEntityId() == IdEntity)
		{
			m_vecPersonalSignalTimers[uIndex]->OnProxyReset();
		}
	}
}

// Description:
//
// Arguments:
//
// Return:
//
bool CSignalTimer::Update(float fElapsedTime)
{
	bool bRet = true;
	uint32 uOrder = 0;
	std::vector<CPersonalSignalTimer*>::iterator iter = m_vecPersonalSignalTimers.begin();

	while (iter != m_vecPersonalSignalTimers.end())
	{
		CPersonalSignalTimer* pPersonal = *iter;

		if (pPersonal && pPersonal->Update(fElapsedTime, ((m_bDebug == true) ? ++uOrder : 0)) == false)
		{
			SAFE_DELETE(pPersonal);
			iter = m_vecPersonalSignalTimers.erase(iter);
		}
		else
		{
			++iter;
		}
	}

	return(bRet);
}

// Description:
//
// Arguments:
//
// Return:
//
void CSignalTimer::Shutdown()
{
	SAFE_DELETE(m_pInstance);
}

// Description:
//
// Arguments:
//
// Return:
//
bool CSignalTimer::EnablePersonalManager(EntityId IdEntity, const char* sSignal)
{
	CRY_ASSERT(m_bInit == true);
	CRY_ASSERT(IdEntity > 0);
	CRY_ASSERT(sSignal != NULL);

	bool bRet = true;
	CPersonalSignalTimer* pPersonal = GetPersonalSignalTimer(IdEntity, sSignal);

	if (pPersonal == NULL)
	{
		pPersonal = CreatePersonalSignalTimer(IdEntity, sSignal);
		bRet = false;
	}

	pPersonal->SetEnabled(true);

	return(bRet);
}

// Description:
//
// Arguments:
//
// Return:
//
bool CSignalTimer::DisablePersonalSignalTimer(EntityId IdEntity, const char* sSignal)
{
	CRY_ASSERT(m_bInit == true);
	CRY_ASSERT(IdEntity > 0);
	CRY_ASSERT(sSignal != NULL);

	bool bRet = false;
	CPersonalSignalTimer* pPersonal = GetPersonalSignalTimer(IdEntity, sSignal);

	if (pPersonal != NULL)
	{
		pPersonal->SetEnabled(false);
		bRet = true;
	}

	return(bRet);
}

// Description:
//
// Arguments:
//
// Return:
//
bool CSignalTimer::ResetPersonalTimer(EntityId IdEntity, const char* sSignal)
{
	CRY_ASSERT(m_bInit == true);
	CRY_ASSERT(IdEntity > 0);
	CRY_ASSERT(sSignal != NULL);

	bool bRet = false;
	CPersonalSignalTimer* pPersonal = GetPersonalSignalTimer(IdEntity, sSignal);

	if (pPersonal != NULL)
	{
		pPersonal->ForceReset();
		bRet = true;
	}

	return(bRet);
}

// Description:
//
// Arguments:
//
// Return:
//
bool CSignalTimer::EnableAllPersonalManagers(EntityId IdEntity)
{
	CRY_ASSERT(m_bInit == true);
	CRY_ASSERT(IdEntity > 0);

	bool bRet = false;

	for (uint32 uIndex = 0; uIndex < m_vecPersonalSignalTimers.size(); ++uIndex)
	{
		if (m_vecPersonalSignalTimers[uIndex]->GetEntityId() == IdEntity)
		{
			m_vecPersonalSignalTimers[uIndex]->SetEnabled(true);
			bRet = true;
		}
	}

	return(bRet);
}

// Description:
//
// Arguments:
//
// Return:
//
bool CSignalTimer::DisablePersonalSignalTimers(EntityId IdEntity)
{
	CRY_ASSERT(m_bInit == true);
	CRY_ASSERT(IdEntity > 0);

	bool bRet = false;

	for (uint32 uIndex = 0; uIndex < m_vecPersonalSignalTimers.size(); ++uIndex)
	{
		if (m_vecPersonalSignalTimers[uIndex]->GetEntityId() == IdEntity)
		{
			m_vecPersonalSignalTimers[uIndex]->SetEnabled(false);
			bRet = true;
		}
	}

	return(bRet);
}

// Description:
//
// Arguments:
//
// Return:
//
bool CSignalTimer::ResetPersonalTimers(EntityId IdEntity)
{
	CRY_ASSERT(m_bInit == true);
	CRY_ASSERT(IdEntity > 0);

	bool bRet = false;

	for (uint32 uIndex = 0; uIndex < m_vecPersonalSignalTimers.size(); ++uIndex)
	{
		if (m_vecPersonalSignalTimers[uIndex]->GetEntityId() == IdEntity)
		{
			m_vecPersonalSignalTimers[uIndex]->ForceReset();
			bRet = true;
		}
	}

	return(bRet);
}

// Description:
//
// Arguments:
//
// Return:
//
bool CSignalTimer::SetTurnRate(EntityId IdEntity, const char* sSignal, float fTime, float fTimeMax)
{
	CRY_ASSERT(m_bInit == true);
	CRY_ASSERT(IdEntity > 0);
	CRY_ASSERT(fTime > 0.0f);
	CRY_ASSERT(sSignal != NULL);

	bool bRet = false;
	CPersonalSignalTimer* pPersonal = GetPersonalSignalTimer(IdEntity, sSignal);

	if (pPersonal != NULL)
	{
		pPersonal->SetRate(fTime, max(fTimeMax, fTime));
		pPersonal->ForceReset();
		bRet = true;
	}

	return(bRet);
}

// Description:
//
// Arguments:
//
// Return:
//
CPersonalSignalTimer* CSignalTimer::GetPersonalSignalTimer(EntityId IdEntity, const char* sSignal) const
{
	CRY_ASSERT(m_bInit == true);
	CRY_ASSERT(IdEntity > 0);
	CRY_ASSERT(sSignal != NULL);

	CPersonalSignalTimer* pPersonal = NULL;

	for (uint32 uIndex = 0; uIndex < m_vecPersonalSignalTimers.size(); ++uIndex)
	{
		if (m_vecPersonalSignalTimers[uIndex]->GetEntityId() == IdEntity &&
		    m_vecPersonalSignalTimers[uIndex]->GetSignalString().compareNoCase(sSignal) == 0)
		{
			pPersonal = m_vecPersonalSignalTimers[uIndex];
			break;
		}
	}

	return(pPersonal);
}

// Description:
//
// Arguments:
//
// Return:
//
void CSignalTimer::SetDebug(bool bDebug)
{
	CRY_ASSERT(m_bInit == true);

	m_bDebug = bDebug;
}

// Description:
//
// Arguments:
//
// Return:
//
bool CSignalTimer::GetDebug() const
{
	return(m_bDebug);
}

// Description:
//
// Arguments:
//
// Return:
//
CPersonalSignalTimer* CSignalTimer::CreatePersonalSignalTimer(EntityId IdEntity, const char* sSignal)
{
	CRY_ASSERT(m_bInit == true);
	CRY_ASSERT(IdEntity > 0);
	CRY_ASSERT(sSignal != NULL);

	CPersonalSignalTimer* pPersonal = new CPersonalSignalTimer(this);

	if (pPersonal->Init(IdEntity, sSignal) == true)
	{
		m_vecPersonalSignalTimers.push_back(pPersonal);
	}
	else
	{
		SAFE_DELETE(pPersonal);
	}

	return(pPersonal);
}
