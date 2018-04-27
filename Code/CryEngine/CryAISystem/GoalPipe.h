// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#ifndef __GOALPIPE_H_
#define __GOALPIPE_H_

#if _MSC_VER > 1000
	#pragma once
#endif

#include <CryAISystem/IGoalPipe.h>

#include <CryAISystem/IAgent.h>
#include "GoalOp.h"

#include <vector>

// TODO evgeny: What is QGoal?
struct QGoal
{
	// _smart_ptr required only because we keep these in STL vectors
	// if pGoalOp is null, this is goal is actually a subpipe
	_smart_ptr<CGoalOp>   pGoalOp;
	EGoalOperations       op;
	string                sPipeName; // The name of the possible pipe.
	GoalParameters        params;
	bool                  bBlocking;
	IGoalPipe::EGroupType eGrouping;

	QGoal() : pGoalOp(0), bBlocking(false), eGrouping(IGoalPipe::eGT_NOGROUP) {}

	QGoal Clone();

	bool  operator==(const QGoal& other) const
	{
		return pGoalOp == other.pGoalOp;
	}

	void Serialize(TSerialize ser);
};

typedef std::vector<QGoal>    VectorOGoals;

typedef std::map<string, int> LabelsMap;

enum EPopGoalResult
{
	ePGR_AtEnd,
	ePGR_Succeed,
	ePGR_BreakLoop,
};

/*! This class defines a logical set of actions that an agent performs in succession.
 */
class CGoalPipe : public IGoalPipe
{
public:
	CGoalPipe(const char* sName, bool bDynamic = false);
	virtual ~CGoalPipe();

	CGoalPipe*          Clone();

	virtual const char* GetName() const { return m_sName.c_str(); }
	virtual void        HighPriority()  { m_bHighPriority = true; }

	virtual void        PushGoal(const XmlNodeRef& goalOpNode, EGroupType eGrouping);
	virtual void        PushGoal(IGoalOp* pGoalOp, EGoalOperations op, bool bBlocking, EGroupType eGrouping, const GoalParameters& params);
	virtual void        PushGoal(EGoalOperations op, bool bBlocking, EGroupType eGrouping, GoalParameters& params);
	virtual void        PushLabel(const char* label);
	virtual void        PushPipe(const char* szName, bool bBlocking, EGroupType eGrouping, GoalParameters& params);
	virtual void        SetDebugName(const char* name);

	virtual void        ParseParams(const GoalParams& node);
	virtual void        ParseParam(const char* param, const GoalParams& node);

	// Getters/setters

	const string&              GetNameAsString() const                        { return m_sName; }

	bool                       IsDynamic() const                              { return m_bDynamic; }

	int                        GetEventId() const                             { return m_nEventId; }
	void                       SetEventId(int id)                             { m_nEventId = id; }

	bool                       IsHighPriority() const                         { return m_bHighPriority; }

	const Vec3&                GetAttTargetPosAtStart() const                 { return m_vAttTargetPosAtStart; }
	void                       SetAttTargetPosAtStart(const Vec3& v)          { m_vAttTargetPosAtStart = v; }

	const string&              GetDebugName() const                           { return m_sDebugName; }

	const CWeakRef<CAIObject>& GetRefArgument() const                         { return m_refArgument; }
	CWeakRef<CAIObject>&       GetRefArgument()                               { return m_refArgument; }
	void                       SetRefArgument(const CWeakRef<CAIObject>& ref) { m_refArgument = ref; }

	EGoalOpResult              GetLastResult() const
	{
		return m_pSubPipe ? m_pSubPipe->GetLastResult() : m_lastResult;
	}

	void SetLastResult(EGoalOpResult res)
	{
		if (m_pSubPipe)
		{
			m_pSubPipe->SetLastResult(res);
		}
		m_lastResult = res;
	}

	int  GetPosition() const { return m_nPosition; }
	void SetPosition(int nPos)
	{
		if ((0 < nPos) && (static_cast<unsigned int>(nPos) < m_qGoalPipe.size()))
		{
			m_nPosition = nPos;
		}
	}

	CGoalPipe*       GetSubpipe()        { return m_pSubPipe; }
	const CGoalPipe* GetSubpipe()  const { return m_pSubPipe; }
	void             SetSubpipe(CGoalPipe* pPipe);

	void             SetLoop(bool bLoop) { m_bLoop = bLoop; }
	bool             IsLoop() const      { return m_bLoop; }

	uint32           GetNumGoalOps() const
	{
		return m_qGoalPipe.size();
	}

	CGoalOp* GetGoalOp(uint32 index)
	{
		CGoalOp* ret = 0;

		if (index < m_qGoalPipe.size())
		{
			ret = m_qGoalPipe[index].pGoalOp;
		}

		return ret;
	}

	const CGoalOp* GetGoalOp(uint32 index) const
	{
		CGoalOp* ret = 0;

		if (index < m_qGoalPipe.size())
		{
			ret = m_qGoalPipe[index].pGoalOp;
		}

		return ret;
	}

	bool IsGoalBlocking(uint32 index) const
	{
		//out of bounds goals are not blocking by default
		return (index < m_qGoalPipe.size() && m_qGoalPipe[index].bBlocking);
	}

	//////////////////////////////////////////////////////////////////////////

	EPopGoalResult PopGoal(QGoal& theGoal, CPipeUser* pPipeUser);
	EPopGoalResult PeekPopGoalResult() const;

	// Makes the IP of this pipe jump to the desired position
	bool Jump(int position);

	// TODO: cut the string version of Jump in a few weeks
	bool Jump(const char* label);

	// Does Jump(-1) or more to start from the beginning of current group
	void             ReExecuteGroup();

	bool             IsInSubpipe() const    { return m_pSubPipe != 0; }

	CGoalPipe*       GetLastSubpipe()       { return m_pSubPipe ? m_pSubPipe->GetLastSubpipe() : this; }
	const CGoalPipe* GetLastSubpipe() const { return m_pSubPipe ? m_pSubPipe->GetLastSubpipe() : this; }

	int              CountSubpipes() const  { return m_pSubPipe ? (m_pSubPipe->CountSubpipes() + 1) : 0; }
	bool             RemoveSubpipe(CPipeUser* pPipeUser, int& goalPipeId, bool keepInserted, bool keepHigherPriority);

	void             Serialize(TSerialize ser, VectorOGoals& activeGoals);
#ifdef SERIALIZE_DYNAMIC_GOALPIPES
	void             SerializeDynamic(TSerialize ser);
#endif // SERIALIZE_DYNAMIC_GOALPIPES

	void ResetGoalops(CPipeUser* pPipeUser);
	void Reset();

	//////////////////////////////////////////////////////////////////////////

	static EGoalOperations GetGoalOpEnum(const char* szName);
	static const char*     GetGoalOpName(EGoalOperations op);

	CGoalOp*               CreateGoalOp(EGoalOperations op, const XmlNodeRef& goalOpNode);

	size_t                 MemStats();

	//////////////////////////////////////////////////////////////////////////

	bool m_bKeepOnTop;//TODO make private!

private:
	CGoalPipe();
	string m_sName;

	// true, if the pipe was created after the initialization of the AI System
	// (including loading of aiconfig.lua);
	// more precisely, after goal pipe "_last_" was created, as of 28.01.2010
	bool m_bDynamic;

	int  m_nEventId;
	bool m_bHighPriority;

	// position of owner's attention target when pipe is selected OR new target is set
	Vec3                m_vAttTargetPosAtStart;

	string              m_sDebugName;
	CWeakRef<CAIObject> m_refArgument;
	EGoalOpResult       m_lastResult;
	VectorOGoals        m_qGoalPipe;
	LabelsMap           m_Labels;
	unsigned int        m_nPosition; // position in pipe
	CGoalPipe*          m_pSubPipe;
	bool                m_bLoop;

	//to be used for WAIT goalOp initialization
	int m_nCurrentBlockCounter;
};

#endif  // #ifndef __GOALPIPE_H_
