// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#ifndef _AIACTIONS_H_
#define _AIACTIONS_H_

#if _MSC_VER > 1000
	#pragma once
#endif

#include <CryFlowGraph/IFlowSystem.h>
#include <CryAISystem/IAIAction.h>
#include <CryCore/Containers/CryListenerSet.h>

#include "SmartObjects.h"

// forward declaration
class CAIActionManager;

///////////////////////////////////////////////////
// CAIAction references a Flow Graph - sequence of elementary actions
// which should be executed to "use" a smart object
///////////////////////////////////////////////////
class CAIAction : public IAIAction
{
protected:
	// CAIActionManager needs right to access and modify these private data members
	friend class CAIActionManager;

	// unique name of this AI Action
	string m_Name;

	// points to a flow graph which would be used to execute the AI Action
	IFlowGraphPtr m_pFlowGraph;

	void NotifyListeners(EActionEvent event)
	{
		for (TActionListeners::Notifier notifier(m_listeners); notifier.IsValid(); notifier.Next())
			notifier->OnActionEvent(event);
	}

public:
	CAIAction(const CAIAction& src) : m_listeners(1)
	{
		m_Name = src.m_Name;
		m_pFlowGraph = src.m_pFlowGraph;

		// [11/11/2010 willw] Do not copy the listeners, as they registered to listen to src not this.
		//m_listeners = src.m_listeners;
	}
	CAIAction() : m_listeners(1) {}
	virtual ~CAIAction() {}

	// traverses all nodes of the underlying flow graph and checks for any nodes that are incompatible when being run in the context of an IAIAction
	virtual bool TraverseAndValidateAction(EntityId idOfUser) const;

	// returns the unique name of this AI Action
	virtual const char* GetName() const { return m_Name; }

	// returns the goal pipe which executes this AI Action
	virtual IGoalPipe* GetGoalPipe() const { return NULL; };

	// returns the Flow Graph associated to this AI Action
	virtual IFlowGraph* GetFlowGraph() const { return m_pFlowGraph; }

	// returns the User entity associated to this AI Action
	// no entities could be assigned to base class
	virtual IEntity* GetUserEntity() const { return NULL; }

	// returns the Object entity associated to this AI Action
	// no entities could be assigned to base class
	virtual IEntity* GetObjectEntity() const { return NULL; }

	// ends execution of this AI Action
	// don't do anything since this action can't be executed
	virtual void EndAction() {}

	// cancels execution of this AI Action
	// don't do anything since this action can't be executed
	virtual void CancelAction() {}

	// aborts execution of this AI Action - will start clean up procedure
	virtual bool AbortAction() { AIAssert(!"Aborting inactive AI action!"); return false; }

	// marks this AI Action as modified
	virtual void Invalidate() { m_pFlowGraph = NULL; }

	// removes pointers to entities
	virtual void        OnEntityRemove()                                                     {}

	virtual bool        IsSignaledAnimation() const                                          { return false; }
	virtual bool        IsExactPositioning() const                                           { return false; }
	virtual const char* GetAnimationName() const                                             { return NULL; }

	virtual const Vec3& GetAnimationPos() const                                              { return Vec3Constants<float>::fVec3_Zero; }
	virtual const Vec3& GetAnimationDir() const                                              { return Vec3Constants<float>::fVec3_OneY; }
	virtual bool        IsUsingAutoAssetAlignment() const                                    { return false; }

	virtual const Vec3& GetApproachPos() const                                               { return Vec3Constants<float>::fVec3_Zero; }

	virtual float       GetStartWidth() const                                                { return 0; }
	virtual float       GetStartArcAngle() const                                             { return 0; }
	virtual float       GetDirectionTolerance() const                                        { return 0; }

	virtual void        RegisterListener(IAIActionListener* eventListener, const char* name) { m_listeners.Add(eventListener, name); }
	virtual void        UnregisterListener(IAIActionListener* eventListener)                 { m_listeners.Remove(eventListener); }

private:

	typedef CListenerSet<IAIActionListener*> TActionListeners;

	TActionListeners m_listeners;

};

///////////////////////////////////////////////////
// CAnimationAction references a goal pipe
// which should be executed to "use" a smart object
///////////////////////////////////////////////////
class CAnimationAction : public IAIAction
{
protected:
	// animation parameters
	bool   bSignaledAnimation;
	bool   bExactPositioning;
	string sAnimationName;

	// approaching parameters
	Vec3  vApproachPos;
	float fApproachSpeed;
	int   iApproachStance;

	// exact positioning parameters
	Vec3  vAnimationPos;
	Vec3  vAnimationDir;
	float fStartWidth;
	float fDirectionTolerance;
	float fStartArcAngle;
	bool  bAutoTarget;

	void NotifyListeners(EActionEvent event)
	{
		for (TActionListeners::Notifier notifier(m_listeners); notifier.IsValid(); notifier.Next())
			notifier->OnActionEvent(event);
	}

public:
	CAnimationAction(
	  const char* animationName,
	  bool signaled,
	  float approachSpeed,
	  int approachStance,
	  float startWidth,
	  float directionTolerance,
	  float startArcAngle)
		: sAnimationName(animationName)
		, bSignaledAnimation(signaled)
		, fApproachSpeed(approachSpeed)
		, iApproachStance(approachStance)
		, fStartWidth(startWidth)
		, fDirectionTolerance(directionTolerance)
		, fStartArcAngle(startArcAngle)
		, bExactPositioning(true)
		, vApproachPos(ZERO)
		, bAutoTarget(false)
		, m_listeners(1)
	{}
	CAnimationAction(
	  const char* animationName,
	  bool signaled)
		: sAnimationName(animationName)
		, bSignaledAnimation(signaled)
		, fApproachSpeed(0.0f)
		, iApproachStance(0)
		, fStartWidth(0.0f)
		, fDirectionTolerance(0.0f)
		, fStartArcAngle(0.0f)
		, bExactPositioning(false)
		, vApproachPos(ZERO)
		, bAutoTarget(false)
		, m_listeners(1)
	{}
	virtual ~CAnimationAction() {}

	virtual bool TraverseAndValidateAction(EntityId idOfUser) const { return true; }  // CAnimationAction doesn't deal with flow graph, hence no "incompatible" nodes

	// returns the unique name of this AI Action
	virtual const char* GetName() const { return NULL; }

	// returns the goal pipe which executes this AI Action
	virtual IGoalPipe* GetGoalPipe() const;

	// returns the Flow Graph associated to this AI Action
	virtual IFlowGraph* GetFlowGraph() const { return NULL; }

	// returns the User entity associated to this AI Action
	// no entities could be assigned to base class
	virtual IEntity* GetUserEntity() const { return NULL; }

	// returns the Object entity associated to this AI Action
	// no entities could be assigned to base class
	virtual IEntity* GetObjectEntity() const { return NULL; }

	// ends execution of this AI Action
	// don't do anything since this action can't be executed
	virtual void EndAction() {}

	// cancels execution of this AI Action
	// don't do anything since this action can't be executed
	virtual void CancelAction() {}

	// aborts execution of this AI Action - will start clean up procedure
	virtual bool AbortAction() { AIAssert(!"Aborting inactive AI action!"); return false; }

	// marks this AI Action as modified
	virtual void Invalidate() {}

	// removes pointers to entities
	virtual void OnEntityRemove() {}

	void         SetTarget(const Vec3& position, const Vec3& direction)
	{
		vAnimationPos = position;
		vAnimationDir = direction;
	}

	void SetAutoTarget(const Vec3& position, const Vec3& direction)
	{
		vAnimationPos = position;
		vAnimationDir = direction;
		bAutoTarget = true;
	}

	void SetApproachPos(const Vec3& position)
	{
		vApproachPos = position;
	}

	virtual bool        IsSignaledAnimation() const                                          { return bSignaledAnimation; }
	virtual bool        IsExactPositioning() const                                           { return bExactPositioning; }
	virtual const char* GetAnimationName() const                                             { return sAnimationName; }

	virtual const Vec3& GetAnimationPos() const                                              { return vAnimationPos; }
	virtual const Vec3& GetAnimationDir() const                                              { return vAnimationDir; }
	virtual bool        IsUsingAutoAssetAlignment() const                                    { return bAutoTarget; }

	virtual const Vec3& GetApproachPos() const                                               { return vApproachPos; }

	virtual float       GetStartWidth() const                                                { return fStartWidth; }
	virtual float       GetStartArcAngle() const                                             { return fStartArcAngle; }
	virtual float       GetDirectionTolerance() const                                        { return fDirectionTolerance; }

	virtual void        RegisterListener(IAIActionListener* eventListener, const char* name) { m_listeners.Add(eventListener, name); }
	virtual void        UnregisterListener(IAIActionListener* eventListener)                 { m_listeners.Remove(eventListener); }

private:

	typedef CListenerSet<IAIActionListener*> TActionListeners;

	TActionListeners m_listeners;

};

///////////////////////////////////////////////////
// CActiveAction represents single active CAIAction
///////////////////////////////////////////////////
class CActiveAction
	: public CAIAction
{
protected:
	// CAIActionManager needs right to access and modify these private data members
	friend class CAIActionManager;

	// entities participants in this AI Action
	IEntity* m_pUserEntity;
	IEntity* m_pObjectEntity;

	// AI Action is suspended if this counter isn't zero
	// it shows by how many other AI Actions this was suspended
	int m_SuspendCounter;

	// id of goal pipe used for tracking
	int m_idGoalPipe;

	// alertness threshold level
	int m_iThreshold;

	// next user's and object's states
	string m_nextUserState;
	string m_nextObjectState;

	// canceled user's and object's states
	string m_canceledUserState;
	string m_canceledObjectState;

	// set to true if action should be deleted on next update
	bool m_bDeleted;

	// set to true if action is high priority
	bool m_bHighPriority;

	// exact positioning parameters
	bool   m_bSignaledAnimation;
	bool   m_bExactPositioning;
	string m_sAnimationName;
	Vec3   m_vAnimationPos;
	Vec3   m_vAnimationDir;
	Vec3   m_vApproachPos;
	bool   m_bAutoTarget;
	float  m_fStartWidth;
	float  m_fDirectionTolerance;
	float  m_fStartArcAngle;

public:
	CActiveAction()
		: m_vAnimationPos(ZERO)
		, m_vAnimationDir(ZERO)
		, m_vApproachPos(ZERO)
		, m_bSignaledAnimation(0)
		, m_bExactPositioning(0)
		, m_bAutoTarget(0)
		, m_fStartWidth(0)
		, m_fStartArcAngle(0)
		, m_fDirectionTolerance(0)
		, m_pUserEntity(NULL)
		, m_pObjectEntity(NULL)
		, m_SuspendCounter(0)
		, m_idGoalPipe(0)
		, m_iThreshold(0)
		, m_bDeleted(false)
		, m_bHighPriority(false)
	{}

	CActiveAction(const CActiveAction& src)
		: CAIAction()
	{
		*this = src;
	}

	CActiveAction& operator=(const CActiveAction& src)
	{
		m_Name = src.m_Name;
		m_pFlowGraph = src.m_pFlowGraph;

		m_pUserEntity = src.m_pUserEntity;
		m_pObjectEntity = src.m_pObjectEntity;
		m_SuspendCounter = src.m_SuspendCounter;
		m_idGoalPipe = src.m_idGoalPipe;
		m_iThreshold = src.m_iThreshold;
		m_nextUserState = src.m_nextUserState;
		m_nextObjectState = src.m_nextObjectState;
		m_canceledUserState = src.m_canceledUserState;
		m_canceledObjectState = src.m_canceledObjectState;
		m_bDeleted = src.m_bDeleted;
		m_bHighPriority = src.m_bHighPriority;
		m_bSignaledAnimation = src.m_bSignaledAnimation;
		m_bExactPositioning = src.m_bExactPositioning;
		m_sAnimationName = src.m_sAnimationName;
		m_vAnimationPos = src.m_vAnimationPos;
		m_vAnimationDir = src.m_vAnimationDir;
		m_vApproachPos = src.m_vApproachPos;
		m_bAutoTarget = src.m_bAutoTarget;
		m_fStartWidth = src.m_fStartWidth;
		m_fStartArcAngle = src.m_fStartArcAngle;
		m_fDirectionTolerance = src.m_fDirectionTolerance;
		return *this;
	}

	// returns the User entity associated to this AI Action
	virtual IEntity* GetUserEntity() const { return m_pUserEntity; }

	// returns the Object entity associated to this AI Action
	virtual IEntity* GetObjectEntity() const { return m_pObjectEntity; }

	// returns true if action is active and marked as high priority
	virtual bool IsHighPriority() const { return m_bHighPriority; }

	// ends execution of this AI Action
	virtual void EndAction();

	// cancels execution of this AI Action
	virtual void CancelAction();

	// aborts execution of this AI Action - will start clean up procedure
	virtual bool AbortAction();

	// removes pointers to entities
	virtual void        OnEntityRemove() { m_pUserEntity = m_pObjectEntity = NULL; }

	bool                operator==(const CActiveAction& other) const;

	virtual bool        IsSignaledAnimation() const       { return m_bSignaledAnimation; }
	virtual bool        IsExactPositioning() const        { return m_bExactPositioning; }
	virtual const char* GetAnimationName() const          { return m_sAnimationName; }

	virtual const Vec3& GetAnimationPos() const           { return m_vAnimationPos; }
	virtual const Vec3& GetAnimationDir() const           { return m_vAnimationDir; }
	virtual bool        IsUsingAutoAssetAlignment() const { return m_bAutoTarget; }

	virtual const Vec3& GetApproachPos() const            { return m_vApproachPos; }

	virtual float       GetStartWidth() const             { return m_fStartWidth; }
	virtual float       GetStartArcAngle() const          { return m_fStartArcAngle; }
	virtual float       GetDirectionTolerance() const     { return m_fDirectionTolerance; }

	void                Serialize(TSerialize ser);
};

#undef LoadLibrary
///////////////////////////////////////////////////
// CAIActionManager keeps track of all AIActions
///////////////////////////////////////////////////
class CAIActionManager : public IAIActionManager
{
private:
	// library of all defined AI Actions
	typedef std::map<string, CAIAction> TActionsLib;
	TActionsLib m_ActionsLib;

	// list of all active AI Actions (including suspended)
	typedef std::list<CActiveAction> TActiveActions;
	TActiveActions m_ActiveActions;

protected:
	// suspends all active AI Actions in which the entity is involved
	// note: it's safe to pass pEntity == NULL
	int SuspendActionsOnEntity(IEntity* pEntity, int goalPipeId, const IAIAction* pAction, bool bHighPriority, int& numHigherPriority);

	// resumes all active AI Actions in which the entity is involved
	// (resuming depends on it how many times it was suspended)
	// note: it's safe to pass pEntity == NULL
	void ResumeActionsOnEntity(IEntity* pEntity, int goalPipeId);

	// implementing IGoalPipeListener interface
	virtual void OnGoalPipeEvent(IPipeUser* pPipeUser, EGoalPipeEvent event, int goalPipeId, bool& unregisterListenerAfterEvent);

public:
	CAIActionManager();
	virtual ~CAIActionManager();

	// stops all active actions
	void Reset();

	// returns an existing AI Action from the library or NULL if not found
	virtual IAIAction* GetAIAction(const char* sName);

	// returns an existing AI Action by its index in the library or NULL index is out of range
	virtual IAIAction* GetAIAction(size_t index);

	virtual void       ReloadActions();

	// returns an existing AI Action by its name specified in the rule
	// or creates a new temp. action for playing the animation specified in the rule
	IAIAction* GetAIAction(const CCondition* pRule);

	// adds an AI Action in the list of active actions
	void ExecuteAIAction(const IAIAction* pAction, IEntity* pUser, IEntity* pObject, int maxAlertness, int goalPipeId,
	                     const char* userState, const char* objectState, const char* userCanceledState, const char* objectCanceledState, IAIAction::IAIActionListener* pListener = NULL);

	/// Looks up the action by name and returns a handle to it.
	void ExecuteAIAction(const char* sActionName, IEntity* pUser, IEntity* pObject, int maxAlertness, int goalPipeId, IAIAction::IAIActionListener* pListener = NULL);

	// aborts specific AI Action (specified by goalPipeId) or all AI Actions (goalPipeId == 0) in which pEntity is a user
	virtual void AbortAIAction(IEntity* pEntity, int goalPipeId = 0);

	// finishes specific AI Action (specified by goalPipeId) for the pEntity as a user
	virtual void FinishAIAction(IEntity* pEntity, int goalPipeId);

	// marks AI Action from the list of active actions as deleted
	void ActionDone(CActiveAction& action, bool bRemoveAction = true);

	// removes deleted AI Action from the list of active actions
	void Update();

	// loads the library of AI Action Flow Graphs
	void LoadLibrary(const char* sPath);

	// notification sent by smart objects system
	void OnEntityRemove(IEntity* pEntity);

	void Serialize(TSerialize ser);
};

#endif
