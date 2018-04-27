// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#if 0
// deprecated and won't compile at all...
/********************************************************************
   -------------------------------------------------------------------------
   File name:   GoalOp_G04.h
   Description: Game 04 goalops
             These should move into GameDLL when interfaces allow!
   -------------------------------------------------------------------------
   History:
   - 21:02:2008 - Created by Matthew Jack
   - 2 Mar 2009 - Evgeny Adamenkov: Removed IRenderer
 *********************************************************************/

#ifndef __GoalOp_G04_H__
	#define __GoalOp_G04_H__

	#pragma once

	#include "GoalOp.h"
	#include "GoalOpFactory.h"

// Forward declarations
class COPPathFind;
class COPTrace;

/**
 * Factory for G04 goalops
 *
 */
class CGoalOpFactoryG04 : public IGoalOpFactory
{
	IGoalOp* GetGoalOp(const char* sGoalOpName, IFunctionHandler* pH, int nFirstParam, GoalParameters& params) const;
	IGoalOp* GetGoalOp(EGoalOperations op, GoalParameters& params) const;
};

////////////////////////////////////////////////////////////////////////////
//
//				G4APPROACH - makes agent approach a target using his environment
//
////////////////////////////////////////////////////////////////////////////

class COPG4Approach : public CGoalOp
{
	enum EOPG4AMode { EOPG4AS_Evaluate, EOPG4AS_GoNearHidepoint, EOPG4AS_GoToHidepoint, EOPG4AS_Direct };

	EOPG4AMode            m_eApproachMode;
	EAIRegister           m_nReg; // Register from which to derive the target
	float                 m_fLastDistance;
	float                 m_fMinEndDistance, m_fMaxEndDistance;
	float                 m_fEndAccuracy;
	bool                  m_bNeedHidespot;
	bool                  m_bForceDirect;
	float                 m_fNotMovingTime;
	CTimeValue            m_fLastTime;
	int                   m_iApproachQueryID, m_iRegenerateCurrentQueryID;

	CStrongRef<CAIObject> m_refHideTarget;
	COPTrace*             m_pTraceDirective;
	COPPathFind*          m_pPathfindDirective;
public:
	// fEndDistance - goalpipe finishes at this range
	COPG4Approach(float fMinEndDistance, float MaxEndDistance, bool bForceDirect, EAIRegister nReg);
	COPG4Approach(const XmlNodeRef& node);
	COPG4Approach(const COPG4Approach& rhs);
	virtual ~COPG4Approach();

	EGoalOpResult Execute(CPipeUser* pOperand);
	void          DebugDraw(CPipeUser* pOperand) const;
	void          ExecuteDry(CPipeUser* pOperand);
	void          Reset(CPipeUser* pOperand);
	void          Serialize(TSerialize ser);

};

#endif // __GoalOp_G04_H__
#endif // 0
