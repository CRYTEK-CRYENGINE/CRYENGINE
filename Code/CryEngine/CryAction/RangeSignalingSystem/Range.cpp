// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

/********************************************************************
   ---------------------------------------------------------------------
   File name:   Range.h
   $Id$
   $DateTime$
   Description: Single Range donut
   ---------------------------------------------------------------------
   History:
   - 24:08:2007 : Created by Ricardo Pillosu

 *********************************************************************/
#include "StdAfx.h"
#include "Range.h"
#include "PersonalRangeSignaling.h"

// Description:
//   Constructor
// Arguments:
//
// Return:
//
CRange::CRange(CPersonalRangeSignaling* pPersonal) : m_pPersonal(pPersonal), m_fRadius(-1.0f), m_fBoundary(-1.0f), m_pSignalData(NULL)
{
	CRY_ASSERT(pPersonal != NULL);
}

// Description:
//   Destructor
// Arguments:
//
// Return:
//
CRange::~CRange()
{
	if (gEnv->pAISystem)
		gEnv->pAISystem->FreeSignalExtraData(m_pSignalData);
}

// Description:
//   Constructor
// Arguments:
//
// Return:
//
void CRange::Init(float fRadius, float fBoundary, const char* sSignal, IAISignalExtraData* pData /*= NULL*/)
{
	CRY_ASSERT(fRadius >= 1.0f);
	CRY_ASSERT(fBoundary >= 0.0f);
	CRY_ASSERT(sSignal != NULL);

	m_sSignal = sSignal;
	m_fRadius = fRadius * fRadius;
	m_fBoundary = fBoundary + fRadius;
	m_fBoundary *= m_fBoundary;

	// Clone extra data
	if (pData && gEnv->pAISystem)
	{
		gEnv->pAISystem->FreeSignalExtraData(m_pSignalData);
		m_pSignalData = gEnv->pAISystem->CreateSignalExtraData();
		CRY_ASSERT(m_pSignalData);
		*m_pSignalData = *pData;
	}
}

// Description:
//   Destructor
// Arguments:
//
// Return:
//
bool CRange::IsInRange(const Vec3& vPos) const
{
	bool bResult = false;
	IEntity* pEntity = m_pPersonal->GetEntity();
	CRY_ASSERT(pEntity);
	if (pEntity)
	{
		Vec3 vOrigin = pEntity->GetPos();
		bResult = IsInRange((vPos - vOrigin).GetLengthSquared());
	}
	return bResult;
}

// Description:
//   Destructor
// Arguments:
//
// Return:
//
bool CRange::IsInRangePlusBoundary(const Vec3& vPos) const
{
	bool bResult = false;
	IEntity* pEntity = m_pPersonal->GetEntity();
	CRY_ASSERT(pEntity);
	if (pEntity)
	{
		Vec3 vOrigin = pEntity->GetPos();
		bResult = IsInRangePlusBoundary((vPos - vOrigin).GetLengthSquared());
	}
	return bResult;
}
