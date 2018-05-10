// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

/********************************************************************
   -------------------------------------------------------------------------
   File name:   GoalOp_G02.h
   Description: Game 02 goalops
             These should move into GameDLL when interfaces allow!
   -------------------------------------------------------------------------
   History:
   - 25:02:2008 - Created by Matthew Jack
   - 2 Mar 2009 - Evgeny Adamenkov: Removed IRenderer
 *********************************************************************/

#ifndef __GoalOp_G02_H__
#define __GoalOp_G02_H__

#pragma once

#include "GoalOp.h"
#include "GoalOpFactory.h"

// Forward declarations
class COPPathFind;
class COPTrace;

/**
 * Factory for G02 goalops
 *
 */
class CGoalOpFactoryG02 : public IGoalOpFactory
{
	IGoalOp* GetGoalOp(const char* sGoalOpName, IFunctionHandler* pH, int nFirstParam, GoalParameters& params) const;
	IGoalOp* GetGoalOp(EGoalOperations op, GoalParameters& params) const;
};

////////////////////////////////////////////////////////////
//
//				CHARGE - chase target and charge when condition is met.
//
////////////////////////////////////////////////////////
class COPCharge : public CGoalOp
{
	CAIObject* m_moveTarget;
	CAIObject* m_sightTarget;
	int        m_looseAttentionId;

	Vec3       m_debugAntenna[9];
	Vec3       m_debugRange[2];
	Vec3       m_debugHit[2];
	bool       m_debugHitRes;
	float      m_debugHitRad;

	int        m_state;
	enum EChargeState
	{
		STATE_APPROACH,
		STATE_ANTICIPATE,
		STATE_CHARGE,
		STATE_FOLLOW_TROUGH,
	};

	float      m_distanceFront;
	float      m_distanceBack;
	bool       m_useLastOpResult;
	bool       m_lookAtLastOp;
	bool       m_initialized;
	CTimeValue m_anticipateTime;
	CTimeValue m_approachStartTime;
	bool       m_bailOut;

	float      m_stuckTime;

	Vec3       m_chargePos;
	Vec3       m_chargeStart;
	Vec3       m_chargeEnd;
	Vec3       m_lastOpPos;

	CPipeUser* m_pOperand;

	void ValidateRange();
	void SetChargeParams();
	void UpdateChargePos();

public:
	COPCharge(float distanceFront, float distanceBack, bool bUseLastOp, bool bLookatLastOp);
	~COPCharge();

	void          SteerToTarget();

	EGoalOpResult Execute(CPipeUser* pOperand);
	void          ExecuteDry(CPipeUser* pOperand);
	void          Reset(CPipeUser* pOperand);
	void          OnObjectRemoved(CAIObject* pObject);
	void          DebugDraw(CPipeUser* pOperand) const;
};

////////////////////////////////////////////////////////////
//
// SEEKCOVER - Seek cover dynamically
//
////////////////////////////////////////////////////////

class COPSeekCover : public CGoalOp
{
public:
	COPSeekCover(bool uncover, float radius, float minRadius, int iterations, bool useLastOpAsBackup, bool towardsLastOpResult);
	explicit COPSeekCover(const XmlNodeRef& node);
	~COPSeekCover();

	EGoalOpResult Execute(CPipeUser* pOperand);
	void          ExecuteDry(CPipeUser* pOperand);
	void          Reset(CPipeUser* pOperand);
	void          Serialize(TSerialize ser);
	void          DebugDraw(CPipeUser* pOperand) const;

private:

	struct SAIStrafeCoverPos
	{
		SAIStrafeCoverPos(const Vec3& pos, bool vis, bool valid, IAISystem::ENavigationType navType)
			: pos(pos)
			, vis(vis)
			, valid(valid)
			, navType(navType)
		{
		};

		Vec3                           pos;
		bool                           vis;
		bool                           valid;
		IAISystem::ENavigationType     navType;
		std::vector<SAIStrafeCoverPos> child;
	};

	bool IsTargetVisibleFrom(const Vec3& from, const Vec3& targetPos);

	typedef std::pair<Vec3, float> Vec3FloatPair;

	bool IsSegmentValid(IAISystem::tNavCapMask navCap, float rad, const Vec3& posFrom, Vec3& posTo, IAISystem::ENavigationType& navTypeFrom);
	void CalcStrafeCoverTree(SAIStrafeCoverPos& pos, int i, int j, const Vec3& center, const Vec3& forw, const Vec3& right,
	                         float radius, const Vec3& target, IAISystem::tNavCapMask navCap, float passRadius, bool insideSoftCover,
	                         const std::vector<Vec3FloatPair>& avoidPos, const std::vector<Vec3FloatPair>& softCoverPos);

	bool               GetPathTo(SAIStrafeCoverPos& pos, SAIStrafeCoverPos* pTarget, CNavPath& path, const Vec3& dirToTarget, IAISystem::tNavCapMask navCap, float passRadius);
	void               DrawStrafeCoverTree(SAIStrafeCoverPos& pos, const Vec3& target) const;

	bool               OverlapSegmentAgainstAvoidPos(const Vec3& from, const Vec3& to, float rad, const std::vector<Vec3FloatPair>& avoidPos);
	bool               OverlapCircleAgaintsAvoidPos(const Vec3& pos, float rad, const std::vector<Vec3FloatPair>& avoidPos);
	void               GetNearestPuppets(CPuppet* pSelf, const Vec3& pos, float radius, std::vector<Vec3FloatPair>& positions);
	bool               IsInDeepWater(const Vec3& pos);

	SAIStrafeCoverPos* GetFurthestFromTarget(SAIStrafeCoverPos& pos, const Vec3& center, const Vec3& forw, const Vec3& right, float& maxDist);
	SAIStrafeCoverPos* GetNearestToTarget(SAIStrafeCoverPos& pos, const Vec3& center, const Vec3& forw, const Vec3& right, float& minDist);
	SAIStrafeCoverPos* GetNearestHidden(SAIStrafeCoverPos& pos, const Vec3& from, float& minDist);

	void               UsePath(CPipeUser* pOperand, SAIStrafeCoverPos* pos, IAISystem::ENavigationType navType);

	SAIStrafeCoverPos*         m_pRoot;

	bool                       m_uncover;
	int                        m_state;
	float                      m_radius;
	float                      m_minRadius;
	int                        m_iterations;
	float                      m_endAccuracy;
	bool                       m_useLastOpAsBackup;
	bool                       m_towardsLastOpResult;
	CTimeValue                 m_lastTime;
	int                        m_notMovingTimeMs;
	COPTrace*                  m_pTraceDirective;
	Vec3                       m_center, m_forward, m_right;
	Vec3                       m_target;
	std::vector<Vec3FloatPair> m_avoidPos;
	std::vector<Vec3FloatPair> m_softCoverPos;
};

#endif //__GoalOp_G02_H__
