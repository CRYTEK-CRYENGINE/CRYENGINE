// Copyright 2001-2017 Crytek GmbH / Crytek Group. All rights reserved. 

#pragma once

#include "SequenceManager.h"
#include <CryFlowGraph/IFlowBaseNode.h>

namespace AIActionSequence
{

//////////////////////////////////////////////////////////////////////////

struct SequenceFlowNodeBase
	: public CFlowBaseNode<eNCT_Instanced>
	  , public SequenceActionBase
{
	// nothing
};

//////////////////////////////////////////////////////////////////////////

struct GoalPipeListenerHelper
{
public:
	GoalPipeListenerHelper()
		: m_goalPipeId(0)
	{}

	void RegisterGoalPipeListener(IGoalPipeListener* listener, EntityId entityId, int goalPipeId);
	void UnRegisterGoalPipeListener(IGoalPipeListener* listener, EntityId entityId);

private:
	int m_goalPipeId;
};

//////////////////////////////////////////////////////////////////////////

class CFlowNode_AISequenceStart
	: public SequenceFlowNodeBase
{
public:
	enum InputPort
	{
		InputPort_Start,
		InputPort_Interruptible,
		InputPort_ResumeAfterInterruption,
	};

	enum OutputPort
	{
		OutputPort_Link,
	};

	CFlowNode_AISequenceStart(SActivationInfo* pActInfo)
		: m_actInfo(*pActInfo)
		, m_restartOnPostSerialize(false)
		, m_waitingForTheForwardingEntityToBeSetOnAllTheNodes(false)
	{
	}

	virtual ~CFlowNode_AISequenceStart();

	virtual IFlowNodePtr Clone(SActivationInfo* pActInfo) { return new CFlowNode_AISequenceStart(pActInfo); }

	virtual void         Serialize(SActivationInfo* pActInfo, TSerialize ser);
	virtual void         GetConfiguration(SFlowNodeConfig& config);
	virtual void         ProcessEvent(EFlowEvent event, SActivationInfo* pActInfo);
	virtual void         PostSerialize(SActivationInfo* pActInfo);
	virtual void         GetMemoryUsage(ICrySizer* sizer) const { sizer->Add(*this); }

	void                 HandleSequenceEvent(SequenceEvent sequenceEvent);

private:
	void InitializeSequence(SActivationInfo* pActInfo);

	bool            m_restartOnPostSerialize;
	bool            m_waitingForTheForwardingEntityToBeSetOnAllTheNodes;
	SActivationInfo m_actInfo;
};

//////////////////////////////////////////////////////////////////////////

class CFlowNode_AISequenceEnd
	: public SequenceFlowNodeBase
{
public:
	enum InputPort
	{
		InputPort_End,
	};

	enum OutputPort
	{
		OutputPort_Done,
	};

	CFlowNode_AISequenceEnd(SActivationInfo* pActInfo) {}
	virtual ~CFlowNode_AISequenceEnd() {}

	virtual IFlowNodePtr Clone(SActivationInfo* pActInfo) { return new CFlowNode_AISequenceEnd(pActInfo); }

	virtual void         GetConfiguration(SFlowNodeConfig& config);
	virtual void         ProcessEvent(EFlowEvent event, SActivationInfo* pActInfo);
	virtual void         GetMemoryUsage(ICrySizer* sizer) const { sizer->Add(*this); }

private:
};

//////////////////////////////////////////////////////////////////////////

class CFlowNode_AISequenceBookmark
	: public SequenceFlowNodeBase
{
public:
	enum InputPort
	{
		InputPort_Set,
	};

	enum OutputPort
	{
		OutputPort_Link,
	};

	CFlowNode_AISequenceBookmark(SActivationInfo* pActInfo)
		: m_actInfo(*pActInfo)
	{}

	virtual IFlowNodePtr Clone(SActivationInfo* pActInfo) { return new CFlowNode_AISequenceBookmark(pActInfo); }

	virtual void         GetConfiguration(SFlowNodeConfig& config);
	virtual void         ProcessEvent(EFlowEvent event, SActivationInfo* pActInfo);
	virtual void         GetMemoryUsage(ICrySizer* sizer) const { sizer->Add(*this); }

	void                 HandleSequenceEvent(SequenceEvent sequenceEvent);

private:
	SActivationInfo m_actInfo;
};

//////////////////////////////////////////////////////////////////////////

class CFlowNode_AISequenceActionMove
	: public SequenceFlowNodeBase
	  , public IGoalPipeListener
	  , public GoalPipeListenerHelper
{
public:
	enum InputPort
	{
		InputPort_Start,
		InputPort_Speed,
		InputPort_Stance,
		InputPort_DestinationEntity,
		InputPort_Position,
		InputPort_Direction,
		InputPort_EndDistance,
	};

	enum OutputPort
	{
		OutputPort_Done,
	};

	CFlowNode_AISequenceActionMove(SActivationInfo* pActInfo)
		: m_actInfo(*pActInfo)
	{
	}
	virtual ~CFlowNode_AISequenceActionMove();

	virtual IFlowNodePtr Clone(SActivationInfo* pActInfo) { return new CFlowNode_AISequenceActionMove(pActInfo); }

	virtual void         GetConfiguration(SFlowNodeConfig& config);
	virtual void         ProcessEvent(EFlowEvent event, SActivationInfo* pActInfo);
	virtual void         GetMemoryUsage(ICrySizer* sizer) const { sizer->Add(*this); }

	void                 HandleSequenceEvent(SequenceEvent sequenceEvent);

	virtual void         OnGoalPipeEvent(IPipeUser* pipeUser, EGoalPipeEvent event, int goalPipeId, bool& unregisterListenerAfterEvent);

private:
	void GetPositionAndDirectionForDestination(OUT Vec3& position, OUT Vec3& direction);

	SActivationInfo m_actInfo;
};

//////////////////////////////////////////////////////////////////////////

class CFlowNode_AISequenceActionMoveAlongPath
	: public SequenceFlowNodeBase
	  , public IGoalPipeListener
	  , public GoalPipeListenerHelper
{
public:
	enum InputPort
	{
		InputPort_Start,
		InputPort_Speed,
		InputPort_Stance,
		InputPort_PathName,
	};

	enum OutputPort
	{
		OutputPort_Done,
	};

	CFlowNode_AISequenceActionMoveAlongPath(SActivationInfo* pActInfo)
		: m_actInfo(*pActInfo)
	{
	}
	virtual ~CFlowNode_AISequenceActionMoveAlongPath();

	virtual IFlowNodePtr Clone(SActivationInfo* pActInfo) { return new CFlowNode_AISequenceActionMoveAlongPath(pActInfo); }

	virtual void         GetConfiguration(SFlowNodeConfig& config);
	virtual void         ProcessEvent(EFlowEvent event, SActivationInfo* pActInfo);
	virtual void         GetMemoryUsage(ICrySizer* sizer) const { sizer->Add(*this); }

	void                 HandleSequenceEvent(SequenceEvent sequenceEvent);

	virtual void         OnGoalPipeEvent(IPipeUser* pipeUser, EGoalPipeEvent event, int goalPipeId, bool& unregisterListenerAfterEvent);

private:
	void GetTeleportEndPositionAndDirection(const char* pathName, Vec3& position, Vec3& direction);

	SActivationInfo m_actInfo;
};

//////////////////////////////////////////////////////////////////////////

class CFlowNode_AISequenceActionAnimation
	: public SequenceFlowNodeBase
	  , public IGoalPipeListener
	  , public GoalPipeListenerHelper
{
public:
	enum InputPort
	{
		InputPort_Start,
		InputPort_Stop,
		InputPort_Animation,
		InputPort_DestinationEntity,
		InputPort_Position,
		InputPort_Direction,
		InputPort_Speed,
		InputPort_Stance,
		InputPort_OneShot,
		InputPort_StartRadius,
		InputPort_DirectionTolerance,
		InputPort_LoopDuration,
	};

	enum OutputPort
	{
		OutputPort_Done,
	};

	CFlowNode_AISequenceActionAnimation(SActivationInfo* pActInfo)
		: m_actInfo(*pActInfo)
		, m_running(false)
	{
	}
	virtual ~CFlowNode_AISequenceActionAnimation();

	virtual IFlowNodePtr Clone(SActivationInfo* pActInfo) { return new CFlowNode_AISequenceActionAnimation(pActInfo); }

	virtual void         GetConfiguration(SFlowNodeConfig& config);
	virtual void         ProcessEvent(EFlowEvent event, SActivationInfo* pActInfo);
	virtual void         GetMemoryUsage(ICrySizer* sizer) const { sizer->Add(*this); }

	void                 HandleSequenceEvent(SequenceEvent sequenceEvent);
	virtual void         OnGoalPipeEvent(IPipeUser* pipeUser, EGoalPipeEvent event, int goalPipeId, bool& unregisterListenerAfterEvent);

private:
	void       GetPositionAndDirectionForDestination(Vec3& position, Vec3& direction);
	void       ClearAnimation(bool bHurry);
	IAIObject* GetAssignedEntityAIObject();

	SActivationInfo m_actInfo;
	bool            m_running;
};

//////////////////////////////////////////////////////////////////////////

class CFlowNode_AISequenceActionWait
	: public SequenceFlowNodeBase
	  , public IGoalPipeListener
	  , public GoalPipeListenerHelper
{
public:
	enum InputPort
	{
		InputPort_Start,
		InputPort_Time,
	};

	enum OutputPort
	{
		OutputPort_Done,
	};

	CFlowNode_AISequenceActionWait(SActivationInfo* pActInfo)
		: m_actInfo(*pActInfo)
	{}

	virtual ~CFlowNode_AISequenceActionWait();

	virtual IFlowNodePtr Clone(SActivationInfo* pActInfo) { return new CFlowNode_AISequenceActionWait(pActInfo); }

	virtual void         GetConfiguration(SFlowNodeConfig& config);
	virtual void         ProcessEvent(EFlowEvent event, SActivationInfo* pActInfo);
	virtual void         GetMemoryUsage(ICrySizer* sizer) const { sizer->Add(*this); }

	void                 HandleSequenceEvent(SequenceEvent sequenceEvent);

	virtual void         OnGoalPipeEvent(IPipeUser* pipeUser, EGoalPipeEvent event, int goalPipeId, bool& unregisterListenerAfterEvent);

private:
	SActivationInfo m_actInfo;
};

//////////////////////////////////////////////////////////////////////////

class CFlowNode_AISequenceActionShoot
	: public SequenceFlowNodeBase
	  , public IGoalPipeListener
	  , public GoalPipeListenerHelper
{
public:
	enum InputPort
	{
		InputPort_Start,
		InputPort_TargetEntity,
		InputPort_TargetPosition,
		InputPort_Duration,
	};

	enum OutputPort
	{
		OutputPort_Done,
	};

	CFlowNode_AISequenceActionShoot(SActivationInfo* pActInfo)
		: m_actInfo(*pActInfo)
	{
	}

	virtual ~CFlowNode_AISequenceActionShoot();

	virtual IFlowNodePtr Clone(SActivationInfo* pActInfo) { return new CFlowNode_AISequenceActionShoot(pActInfo); }

	virtual void         GetConfiguration(SFlowNodeConfig& config);
	virtual void         ProcessEvent(EFlowEvent event, SActivationInfo* pActInfo);
	virtual void         GetMemoryUsage(ICrySizer* sizer) const { sizer->Add(*this); }

	void                 HandleSequenceEvent(SequenceEvent sequenceEvent);

	virtual void         OnGoalPipeEvent(IPipeUser* pipeUser, EGoalPipeEvent event, int goalPipeId, bool& unregisterListenerAfterEvent);

private:
	SActivationInfo m_actInfo;
};

//////////////////////////////////////////////////////////////////////////

class CFlowNode_AISequenceHoldFormation
	: public SequenceFlowNodeBase
{
public:
	enum InputPort
	{
		InputPort_Start,
		InputPort_FormationName,
	};

	enum OutputPort
	{
		OutputPort_Done,
	};

	CFlowNode_AISequenceHoldFormation(SActivationInfo* pActInfo)
		: m_actInfo(*pActInfo)
	{
	}

	virtual ~CFlowNode_AISequenceHoldFormation() {}

	virtual IFlowNodePtr Clone(SActivationInfo* pActInfo) { return new CFlowNode_AISequenceHoldFormation(pActInfo); }

	virtual void         GetConfiguration(SFlowNodeConfig& config);
	virtual void         ProcessEvent(EFlowEvent event, SActivationInfo* pActInfo);
	virtual void         GetMemoryUsage(ICrySizer* sizer) const { sizer->Add(*this); }

private:
	SActivationInfo m_actInfo;
};

class CFlowNode_AISequenceJoinFormation
	: public SequenceFlowNodeBase
{
public:
	enum InputPort
	{
		InputPort_Start,
		InputPort_LeaderId,
	};

	enum OutputPort
	{
		OutputPort_Done,
	};

	CFlowNode_AISequenceJoinFormation(SActivationInfo* pActInfo)
		: m_actInfo(*pActInfo)
	{
	}

	virtual ~CFlowNode_AISequenceJoinFormation() {}

	virtual IFlowNodePtr Clone(SActivationInfo* pActInfo) { return new CFlowNode_AISequenceJoinFormation(pActInfo); }

	virtual void         GetConfiguration(SFlowNodeConfig& config);
	virtual void         ProcessEvent(EFlowEvent event, SActivationInfo* pActInfo);
	virtual void         GetMemoryUsage(ICrySizer* sizer) const { sizer->Add(*this); }

private:

	void SendSignal(IAIActor* pIAIActor, const char* signalName, IEntity* pSender);

	SActivationInfo m_actInfo;
};

//////////////////////////////////////////////////////////////////////////

class CFlowNode_AISequenceAction_Stance
	: public SequenceFlowNodeBase
{
public:
	enum InputPort
	{
		InputPort_Start,
		InputPort_Stance,
	};

	enum OutputPort
	{
		OutputPort_Done,
	};

	CFlowNode_AISequenceAction_Stance(SActivationInfo* pActInfo)
		: m_actInfo(*pActInfo)
	{
	}

	virtual ~CFlowNode_AISequenceAction_Stance() {}

	virtual IFlowNodePtr Clone(SActivationInfo* pActInfo) { return new CFlowNode_AISequenceAction_Stance(pActInfo); }

	virtual void         GetConfiguration(SFlowNodeConfig& config);
	virtual void         ProcessEvent(EFlowEvent event, SActivationInfo* pActInfo);
	virtual void         GetMemoryUsage(ICrySizer* sizer) const { sizer->Add(*this); }

	void                 HandleSequenceEvent(SequenceEvent sequenceEvent);

private:
	SActivationInfo m_actInfo;
};

//////////////////////////////////////////////////////////////////////////

} // namespace AIActionSequence
