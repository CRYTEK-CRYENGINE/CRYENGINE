// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "DeepSelection.h"

#include "Objects/BaseObject.h"

//! Functor for sorting selected objects on deep selection mode.
struct NearDistance
{
	NearDistance(){}
	bool operator()(const CDeepSelection::RayHitObject& lhs, const CDeepSelection::RayHitObject& rhs) const
	{
		return lhs.distance < rhs.distance;
	}
};

//-----------------------------------------------------------------------------
CDeepSelection::CDeepSelection()
	: m_Mode(DSM_NONE)
	, m_previousMode(DSM_NONE)
	, m_CandidateObjectCount(0)
	, m_CurrentSelectedPos(-1)
{
	m_LastPickPoint.x = -1;
	m_LastPickPoint.y = -1;
}

//-----------------------------------------------------------------------------
CDeepSelection::~CDeepSelection()
{

}

//-----------------------------------------------------------------------------
void CDeepSelection::Reset(bool bResetLastPick)
{
	for (int i = 0; i < m_CandidateObjectCount; ++i)
		m_RayHitObjects[i].object->ClearFlags(OBJFLAG_NO_HITTEST);

	m_CandidateObjectCount = 0;
	m_CurrentSelectedPos = -1;

	m_RayHitObjects.clear();

	if (bResetLastPick)
	{
		m_LastPickPoint.x = -1;
		m_LastPickPoint.y = -1;
	}
}

//-----------------------------------------------------------------------------
void CDeepSelection::AddObject(float distance, CBaseObject* pObj)
{
	m_RayHitObjects.push_back(RayHitObject(distance, pObj));
}

//-----------------------------------------------------------------------------
bool CDeepSelection::OnCycling(const CPoint& pt)
{
	CPoint diff = m_LastPickPoint - pt;
	LONG epsilon = 2;
	m_LastPickPoint = pt;

	if (abs(diff.x) < epsilon && abs(diff.y) < epsilon)
		return true;
	else
		return false;
}

//-----------------------------------------------------------------------------
void CDeepSelection::ExcludeHitTest(int except)
{
	int nExcept = except % m_CandidateObjectCount;

	for (int i = 0; i < m_CandidateObjectCount; ++i)
	{
		m_RayHitObjects[i].object->SetFlags(OBJFLAG_NO_HITTEST);
	}

	m_RayHitObjects[nExcept].object->ClearFlags(OBJFLAG_NO_HITTEST);
}

//-----------------------------------------------------------------------------
int CDeepSelection::CollectCandidate(float fMinDistance, float fRange)
{
	m_CandidateObjectCount = 0;

	if (!m_RayHitObjects.empty())
	{
		std::sort(m_RayHitObjects.begin(), m_RayHitObjects.end(), NearDistance());

		for (std::vector<CDeepSelection::RayHitObject>::iterator itr = m_RayHitObjects.begin();
		     itr != m_RayHitObjects.end(); ++itr)
		{
			if (itr->distance - fMinDistance < fRange)
			{
				++m_CandidateObjectCount;
			}
			else
			{
				break;
			}
		}
	}

	return m_CandidateObjectCount;
}

//-----------------------------------------------------------------------------
CBaseObject* CDeepSelection::GetCandidateObject(int index)
{
	m_CurrentSelectedPos = index % m_CandidateObjectCount;

	return m_RayHitObjects[m_CurrentSelectedPos].object;
}

//-----------------------------------------------------------------------------
//!
void CDeepSelection::SetMode(EDeepSelectionMode mode)
{
	m_previousMode = m_Mode;
	m_Mode = mode;
}

