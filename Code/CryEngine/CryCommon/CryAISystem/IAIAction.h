// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

//! \cond INTERNAL

#pragma once

#include <CryEntitySystem/IEntity.h>

struct IGoalPipe;
struct IFlowGraph;

//! IAIAction references an Action Flow Graph.
//! Ordered sequence of elementary actions which will be executed to "use" a smart object.
struct IAIAction
{
	//! Event names used to inform listeners of an action when the action's state changes.
	enum EActionEvent
	{
		ActionEnd = 0,
		ActionStart,
		ActionCancel,
		ActionAbort,
	};

	struct IAIActionListener
	{
		// <interfuscator:shuffle>
		virtual ~IAIActionListener(){}
		virtual void OnActionEvent(EActionEvent event) = 0;
		// </interfuscator:shuffle>
	};

	// <interfuscator:shuffle>
	virtual ~IAIAction(){}
	//! \return Unique name of this AI Action.
	virtual const char* GetName() const = 0;

	//! Traverses all nodes of the underlying flow graph and checks for any nodes that are incompatible when being run in the context of an IAIAction.
	virtual bool TraverseAndValidateAction(EntityId idOfUser) const = 0;

	//! \return the goal pipe which executes this AI Action.
	virtual IGoalPipe* GetGoalPipe() const = 0;

	//! \return the Flow Graph associated to this AI Action.
	virtual IFlowGraph* GetFlowGraph() const = 0;

	//! \return the User entity associated to this AI Action.
	virtual IEntity* GetUserEntity() const = 0;

	//! \return the Object entity associated to this AI Action.
	virtual IEntity* GetObjectEntity() const = 0;

	//! \return true if action is active and marked as high priority.
	virtual bool IsHighPriority() const { return false; }

	//! Ends execution of this AI Action.
	virtual void EndAction() = 0;

	//! Cancels execution of this AI Action.
	virtual void CancelAction() = 0;

	//! Aborts execution of this AI Action - will start clean up procedure.
	virtual bool AbortAction() = 0;

	//! Marks this AI Action as modified.
	virtual void        Invalidate() = 0;

	virtual bool        IsSignaledAnimation() const = 0;
	virtual bool        IsExactPositioning() const = 0;
	virtual const char* GetAnimationName() const = 0;

	virtual const Vec3& GetAnimationPos() const = 0;
	virtual const Vec3& GetAnimationDir() const = 0;
	virtual bool        IsUsingAutoAssetAlignment() const = 0;

	virtual const Vec3& GetApproachPos() const = 0;

	virtual float       GetStartWidth() const = 0;
	virtual float       GetStartArcAngle() const = 0;
	virtual float       GetDirectionTolerance() const = 0;

	virtual void        RegisterListener(IAIActionListener* eventListener, const char* name) = 0;
	virtual void        UnregisterListener(IAIActionListener* eventListener) = 0;
	// </interfuscator:shuffle>
};

//! \endcond