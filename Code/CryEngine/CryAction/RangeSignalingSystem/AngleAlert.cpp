// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

/********************************************************************
   ---------------------------------------------------------------------
   File name:   AngleAlert.h
   $Id$
   $DateTime$
   Description: Single angle slice
   ---------------------------------------------------------------------
   History:
   - 11:09:2007 : Created by Ricardo Pillosu

 *********************************************************************/
#include "StdAfx.h"
#include "AngleAlert.h"
#include "PersonalRangeSignaling.h"
#include <CryAISystem/IAIObject.h>
// Description:
//   Constructor
// Arguments:
//
// Return:
//
CAngleAlert::CAngleAlert(CPersonalRangeSignaling* pPersonal) : m_pPersonal(pPersonal), m_fAngle(-1.0f), m_fBoundary(-1.0f), m_pSignalData(NULL)
{
	CRY_ASSERT(pPersonal != NULL);
}

// Description:
//   Destructor
// Arguments:
//
// Return:
//
CAngleAlert::~CAngleAlert()
{
	if (gEnv->pAISystem)
		gEnv->pAISystem->FreeSignalExtraData(m_pSignalData);
}

// Description:
//   Init
// Arguments:
//
// Return:
//
void CAngleAlert::Init(float fAngle, float fBoundary, const char* sSignal, IAISignalExtraData* pData /*= NULL*/)
{
	CRY_ASSERT(fAngle >= 1.0f);
	CRY_ASSERT(fBoundary >= 0.0f);
	CRY_ASSERT(sSignal != NULL);

	m_sSignal = sSignal;
	m_fAngle = DEG2RAD(fAngle);
	m_fBoundary = DEG2RAD(fBoundary) + m_fAngle;

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
//
// Arguments:
//
// Return:
//
bool CAngleAlert::Check(const Vec3& vPos) const
{
	return(GetAngleTo(vPos) < m_fAngle);
}

// Description:
//
// Arguments:
//
// Return:
//
bool CAngleAlert::CheckPlusBoundary(const Vec3& vPos) const
{
	return(GetAngleTo(vPos) < m_fBoundary);
}

// Description:
//
// Arguments:
//
// Return:
//
float CAngleAlert::GetAngleTo(const Vec3& vPos) const
{
	float fResult = 0.0f;
	IEntity* pEntity = m_pPersonal->GetEntity();
	CRY_ASSERT(pEntity);
	if (pEntity)
	{
		const Vec3& vEntityPos = pEntity->GetPos();
		const Vec3& vEntityDir = pEntity->GetAI()->GetViewDir();
		Vec3 vDiff = vPos - vEntityPos;
		vDiff.NormalizeSafe();
		fResult = acosf(vEntityDir.Dot(vDiff));
	}
	return fResult;
}
