// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once
////////////////////////////////////////////////////////////////////////////
//
//  CryEngine Source File.
//  Copyright (C), Crytek, 1999-2012
// -------------------------------------------------------------------------
//  File name:	DeepSelection.h
//  Created:	08-03-2010 by Dongjoon Kim
//  Description: Deep Selection Header
//
////////////////////////////////////////////////////////////////////////////

class CBaseObject;

//! Deep Selection
//! Additional output information of HitContext on using "deep selection mode".
//! At the deep selection mode, it supports second selection pass for easy
//! selection on crowded area with two different method.
//! One is to show pop menu of candidate objects list. Another is the cyclic
//! selection on pick clicking.
class CDeepSelection : public _i_reference_target_t
{
public:
	//! Deep Selection Mode Definition
	enum EDeepSelectionMode
	{
		DSM_NONE  = 0,  // Not using deep selection.
		DSM_POP   = 1,  // Deep selection mode with pop context menu.
		DSM_CYCLE = 2   // Deep selection mode with cyclic selection on each clinking same point.
	};

	//! Subclass for container of the selected object with hit distance.
	struct RayHitObject
	{
		RayHitObject(float dist, CBaseObject* pObj)
			: distance(dist)
			, object(pObj)
		{
		}

		float        distance;
		CBaseObject* object;
	};

	//! Constructor
	CDeepSelection();
	virtual ~CDeepSelection();

	void Reset(bool bResetLastPick = false);
	void AddObject(float distance, CBaseObject* pObj);
	//! Check if clicking point is same position with last position,
	//! to decide whether to continue cycling mode.
	bool                      OnCycling(const CPoint& pt);
	//! All objects in list are excluded for hitting test except one, current selection.
	void                      ExcludeHitTest(int except);
	void                      SetMode(EDeepSelectionMode mode);
	inline EDeepSelectionMode GetMode() const         { return m_Mode; }
	inline EDeepSelectionMode GetPreviousMode() const { return m_previousMode; }
	//! Collect object in the deep selection range. The distance from the minimum
	//! distance is less than deep selection range.
	int CollectCandidate(float fMinDistance, float fRange);
	//! Return the candidate object in index position, then it is to be current
	//! selection position.
	CBaseObject* GetCandidateObject(int index);
	//! Return the current selection position that is update in "GetCandidateObject"
	//! function call.
	inline int GetCurrentSelectPos() const     { return m_CurrentSelectedPos; }
	//! Return the number of objects in the deep selection range.
	inline int GetCandidateObjectCount() const { return m_CandidateObjectCount; }

private:
	//! Current mode
	EDeepSelectionMode        m_Mode;
	EDeepSelectionMode        m_previousMode;
	//! Last picking point to check whether cyclic selection continue.
	CPoint                    m_LastPickPoint;
	//! List of the selected objects with ray hitting
	std::vector<RayHitObject> m_RayHitObjects;
	int                       m_CandidateObjectCount;
	int                       m_CurrentSelectedPos;
};

