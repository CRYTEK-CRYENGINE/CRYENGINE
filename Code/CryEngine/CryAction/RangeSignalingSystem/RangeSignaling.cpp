// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

/********************************************************************
   ---------------------------------------------------------------------
   File name:   RangeSignaling.cpp
   $Id$
   $DateTime$
   Description: Signal entities based on ranges from other entities
   ---------------------------------------------------------------------
   History:
   - 09:04:2007 : Created by Ricardo Pillosu

 *********************************************************************/
#include "StdAfx.h"
#include "RangeSignaling.h"
#include "PersonalRangeSignaling.h"

CRangeSignaling* CRangeSignaling::m_pInstance = NULL;

// Description:
//   Constructor
// Arguments:
//
// Return:
//
CRangeSignaling::CRangeSignaling()
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
CRangeSignaling::~CRangeSignaling()
{
	Reset();
}

// Description:
//   Ref
// Arguments:
//
// Return:
//
CRangeSignaling& CRangeSignaling::ref()
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
bool CRangeSignaling::Create()
{
	if (NULL == m_pInstance)
	{
		m_pInstance = new CRangeSignaling();
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
void CRangeSignaling::Init()
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
void CRangeSignaling::Reset()
{
	for (MapPersonals::iterator iter = m_Personals.begin(); iter != m_Personals.end(); ++iter)
	{
		CPersonalRangeSignaling* pPersonal = iter->second;
		SAFE_DELETE(pPersonal);
	}

	m_Personals.clear();
}

// Description:
//
// Arguments:
//
// Return:
//
void CRangeSignaling::OnEditorSetGameMode(bool bGameMode)
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
void CRangeSignaling::OnProxyReset(EntityId IdEntity)
{
	CRY_ASSERT(IdEntity > 0);

	MapPersonals::iterator iter = m_Personals.find(IdEntity);

	if (iter != m_Personals.end())
	{
		iter->second->OnProxyReset();
	}
}

// Description:
//
// Arguments:
//
// Return:
//
bool CRangeSignaling::Update(float fElapsedTime)
{
	bool bRet = true;

	for (MapPersonals::iterator iter = m_Personals.begin(); iter != m_Personals.end(); ++iter)
	{
		CPersonalRangeSignaling* pPersonal = iter->second;
		bRet |= pPersonal->Update(fElapsedTime);
	}

	return(bRet);
}

// Description:
//
// Arguments:
//
// Return:
//
void CRangeSignaling::Shutdown()
{
	SAFE_DELETE(m_pInstance);
}

// Description:
//
// Arguments:
//
// Return:
//
void CRangeSignaling::SetDebug(bool bDebug)
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
bool CRangeSignaling::GetDebug() const
{
	return(m_bDebug);
}

// Description:
//
// Arguments:
//
// Return:
//
bool CRangeSignaling::AddRangeSignal(EntityId IdEntity, float fRadius, float fBoundary, const char* sSignal, IAISignalExtraData* pData /*=NULL*/)
{
	CRY_ASSERT(m_bInit == true);
	CRY_ASSERT(IdEntity > 0);
	CRY_ASSERT(sSignal != NULL);
	CRY_ASSERT(fRadius > 0.5f);
	CRY_ASSERT(fBoundary >= 0.0f);

	bool bRet = false;
	CPersonalRangeSignaling* pPersonal = GetPersonalRangeSignaling(IdEntity);

	if (pPersonal == NULL)
	{
		pPersonal = CreatePersonalRangeSignaling(IdEntity);
	}

	if (pPersonal != NULL)
	{
		bRet = pPersonal->AddRangeSignal(fRadius, fBoundary, sSignal, pData);
	}

	return(bRet);
}

// Description:
//
// Arguments:
//
// Return:
//
bool CRangeSignaling::AddTargetRangeSignal(EntityId IdEntity, EntityId IdTarget, float fRadius, float fBoundary, const char* sSignal, IAISignalExtraData* pData /*= NULL*/)
{
	CRY_ASSERT(m_bInit == true);
	CRY_ASSERT(IdEntity > 0);
	CRY_ASSERT(IdTarget > 0);
	CRY_ASSERT(sSignal != NULL);
	CRY_ASSERT(fRadius > 0.5f);
	CRY_ASSERT(fBoundary >= 0.0f);

	bool bRet = false;
	CPersonalRangeSignaling* pPersonal = GetPersonalRangeSignaling(IdEntity);

	if (pPersonal == NULL)
	{
		pPersonal = CreatePersonalRangeSignaling(IdEntity);
	}

	if (pPersonal != NULL)
	{
		bRet = pPersonal->AddTargetRangeSignal(IdTarget, fRadius, fBoundary, sSignal, pData);
	}

	return(bRet);
}

// Description:
//
// Arguments:
//
// Return:
//
bool CRangeSignaling::AddAngleSignal(EntityId IdEntity, float fAngle, float fBoundary, const char* sSignal, IAISignalExtraData* pData /*=NULL*/)
{
	CRY_ASSERT(m_bInit == true);
	CRY_ASSERT(IdEntity > 0);
	CRY_ASSERT(sSignal != NULL);
	CRY_ASSERT(fAngle > 0.5f);
	CRY_ASSERT(fBoundary >= 0.0f);

	bool bRet = true;
	CPersonalRangeSignaling* pPersonal = GetPersonalRangeSignaling(IdEntity);

	if (pPersonal == NULL)
	{
		pPersonal = CreatePersonalRangeSignaling(IdEntity);
	}

	bRet = pPersonal->AddAngleSignal(fAngle, fBoundary, sSignal, pData);

	return(bRet);
}

// Description:
//
// Arguments:
//
// Return:
//
bool CRangeSignaling::DestroyPersonalRangeSignaling(EntityId IdEntity)
{
	CRY_ASSERT(m_bInit == true);
	CRY_ASSERT(IdEntity > 0);

	return(stl::member_find_and_erase(m_Personals, IdEntity));
}

// Description:
//
// Arguments:
//
// Return:
//
void CRangeSignaling::ResetPersonalRangeSignaling(EntityId IdEntity)
{
	CRY_ASSERT(m_bInit == true);
	CRY_ASSERT(IdEntity > 0);

	MapPersonals::iterator iter = m_Personals.find(IdEntity);

	if (iter != m_Personals.end())
	{
		iter->second->Reset();
	}
}

// Description:
//
// Arguments:
//
// Return:
//
void CRangeSignaling::EnablePersonalRangeSignaling(EntityId IdEntity, bool bEnable)
{
	CRY_ASSERT(m_bInit == true);
	CRY_ASSERT(IdEntity > 0);

	MapPersonals::iterator iter = m_Personals.find(IdEntity);

	if (iter != m_Personals.end())
	{
		iter->second->SetEnabled(bEnable);
	}
}

// Description:
//
// Arguments:
//
// Return:
//
CPersonalRangeSignaling* CRangeSignaling::GetPersonalRangeSignaling(EntityId IdEntity) const
{
	CRY_ASSERT(m_bInit == true);
	CRY_ASSERT(IdEntity > 0);

	CPersonalRangeSignaling* pPersonal = NULL;
	MapPersonals::const_iterator iter = m_Personals.find(IdEntity);

	if (iter != m_Personals.end())
	{
		pPersonal = iter->second;
	}

	return(pPersonal);
}

// Description:
//
// Arguments:
//
// Return:
//
CPersonalRangeSignaling* CRangeSignaling::CreatePersonalRangeSignaling(EntityId IdEntity)
{
	CRY_ASSERT(m_bInit == true);
	CRY_ASSERT(IdEntity > 0);

	CPersonalRangeSignaling* pPersonal = new CPersonalRangeSignaling(this);

	if (pPersonal->Init(IdEntity) == true)
	{
		m_Personals.insert(std::pair<EntityId, CPersonalRangeSignaling*>(IdEntity, pPersonal));
	}
	else
	{
		SAFE_DELETE(pPersonal);
	}

	return(pPersonal);
}
