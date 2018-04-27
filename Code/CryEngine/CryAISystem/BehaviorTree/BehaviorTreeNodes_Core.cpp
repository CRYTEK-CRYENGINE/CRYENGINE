// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "BehaviorTreeNodes_Core.h"

#include <CryAISystem/BehaviorTree/Composite.h>
#include <CryAISystem/BehaviorTree/Decorator.h>
#include <CryAISystem/BehaviorTree/Action.h>
#include <CryAISystem/BehaviorTree/SerializationSupport.h>
#include <CrySystem/Timer.h>
#include "BehaviorTreeManager.h"
#include "BehaviorTreeGraft.h"

#ifdef USING_BEHAVIOR_TREE_SERIALIZATION
	#include <CrySerialization/Enum.h>
	#include <CrySerialization/ClassFactory.h>
#endif

namespace BehaviorTree
{
//////////////////////////////////////////////////////////////////////////

// Executes children one at a time in order.
// Succeeds when all children succeeded.
// Fails when a child failed.
class Sequence : public CompositeWithChildLoader
{
	typedef CompositeWithChildLoader BaseClass;

public:
	typedef uint8 IndexType;

	struct RuntimeData
	{
		IndexType currentChildIndex;

		RuntimeData()
			: currentChildIndex(0)
		{
		}
	};

	virtual LoadResult LoadFromXml(const XmlNodeRef& xml, const LoadContext& context) override
	{
		IF_UNLIKELY (BaseClass::LoadFromXml(xml, context) == LoadFailure)
			return LoadFailure;

		const size_t maxChildCount = std::numeric_limits<IndexType>::max();
		IF_UNLIKELY ((size_t)xml->getChildCount() > maxChildCount)
		{
			ErrorReporter(*this, context).LogError("Too many children. Max %d children are supported.", maxChildCount);
			return LoadFailure;
		}

		return LoadSuccess;
	}

#ifdef USING_BEHAVIOR_TREE_XML_DESCRIPTION_CREATION
	virtual XmlNodeRef CreateXmlDescription() override
	{
		XmlNodeRef xml = BaseClass::CreateXmlDescription();
		xml->setTag("Sequence");
		return xml;
	}
#endif

#ifdef USING_BEHAVIOR_TREE_SERIALIZATION
	virtual void Serialize(Serialization::IArchive& archive) override
	{
		BaseClass::Serialize(archive);

		if (m_children.empty())
			archive.error(m_children, "Must contain a node");

		if (m_children.size() == 1)
			archive.warning(m_children, "Sequence with only one node is superfluous");
	}
#endif

	virtual void OnInitialize(const UpdateContext& context) override
	{
	}

protected:
	virtual Status Update(const UpdateContext& context) override
	{
		CRY_PROFILE_FUNCTION(PROFILE_AI);

		RuntimeData& runtimeData = GetRuntimeData<RuntimeData>(context);

		IndexType& currentChildIndex = runtimeData.currentChildIndex;

		while (true)
		{
			assert(currentChildIndex < m_children.size());
			const Status s = m_children[currentChildIndex]->Tick(context);

			if (s == Running || s == Failure)
			{
				return s;
			}

			if (++currentChildIndex == m_children.size())
			{
				return Success;
			}
		}

		assert(false);
		return Invalid;
	}

	virtual void OnTerminate(const UpdateContext& context) override
	{
		RuntimeData& runtimeData = GetRuntimeData<RuntimeData>(context);
		const IndexType currentChildIndex = runtimeData.currentChildIndex;

		if (currentChildIndex < m_children.size())
		{
			m_children[currentChildIndex]->Terminate(context);
		}
	}

private:
	virtual void HandleEvent(const EventContext& context, const Event& event) override
	{
		RuntimeData& runtimeData = GetRuntimeData<RuntimeData>(context);
		const IndexType currentChildIndex = runtimeData.currentChildIndex;
		if (currentChildIndex < m_children.size())
		{
			m_children[currentChildIndex]->SendEvent(context, event);
		}
	}
};

//////////////////////////////////////////////////////////////////////////

// Executes children one at a time in order.
// Succeeds if child succeeds, otherwise tries next.
// Fails when all children failed.
class Selector : public CompositeWithChildLoader
{
public:
	typedef uint8                    ChildIndexType;
	typedef CompositeWithChildLoader BaseClass;

	struct RuntimeData
	{
		ChildIndexType currentChildIndex;

		RuntimeData()
			: currentChildIndex(0)
		{
		}
	};

#ifdef USING_BEHAVIOR_TREE_XML_DESCRIPTION_CREATION
	virtual XmlNodeRef CreateXmlDescription() override
	{
		XmlNodeRef xml = BaseClass::CreateXmlDescription();
		xml->setTag("Selector");
		return xml;
	}
#endif

#ifdef USING_BEHAVIOR_TREE_SERIALIZATION
	virtual void Serialize(Serialization::IArchive& archive) override
	{
		BaseClass::Serialize(archive);

		if (m_children.empty())
			archive.error(m_children, "Must contain a node");

		if (m_children.size() == 1)
			archive.warning(m_children, "Selector with only one node is superfluous");
	}
#endif

protected:
	virtual void OnTerminate(const UpdateContext& context) override
	{
		RuntimeData& runtimeData = GetRuntimeData<RuntimeData>(context);

		if (runtimeData.currentChildIndex < m_children.size())
		{
			m_children[runtimeData.currentChildIndex]->Terminate(context);
		}
	}

	virtual Status Update(const UpdateContext& context) override
	{
		CRY_PROFILE_FUNCTION(PROFILE_AI);

		RuntimeData& runtimeData = GetRuntimeData<RuntimeData>(context);

		ChildIndexType& currentChildIndex = runtimeData.currentChildIndex;

		while (true)
		{
			const Status s = m_children[currentChildIndex]->Tick(context);

			if (s == Success || s == Running)
			{
				return s;
			}

			if (++currentChildIndex == m_children.size())
			{
				return Failure;
			}
		}

		assert(false);
		return Invalid;
	}

private:
	virtual void HandleEvent(const EventContext& context, const Event& event) override
	{
		RuntimeData& runtimeData = GetRuntimeData<RuntimeData>(context);

		if (runtimeData.currentChildIndex < m_children.size())
		{
			m_children[runtimeData.currentChildIndex]->SendEvent(context, event);
		}
	}
};

//////////////////////////////////////////////////////////////////////////

// Composite node which executes it's children in parallel.
// By default it returns Success when all children have succeeded
// and Failure when any of any child has failed.
// The behavior can be customized.
class Parallel : public CompositeWithChildLoader
{
public:
	typedef CompositeWithChildLoader BaseClass;

	struct RuntimeData
	{
		uint32 runningChildren;     // Each bit is a child
		uint32 successCount;
		uint32 failureCount;

		RuntimeData()
			: runningChildren(0)
			, successCount(0)
			, failureCount(0)
		{
		}
	};

	Parallel()
		: m_successMode(SuccessMode_All)
		, m_failureMode(FailureMode_Any)
	{
	}

	virtual LoadResult LoadFromXml(const XmlNodeRef& node, const LoadContext& context) override
	{
		IF_UNLIKELY (node->getChildCount() > 32)
		{
			ErrorReporter(*this, context).LogError("Too many children. Max 32 children allowed.");
			return LoadFailure;
		}

		m_successMode = SuccessMode_All;
		m_failureMode = FailureMode_Any;

		stack_string failureMode = node->getAttr("failureMode");
		if (!failureMode.empty())
		{
			if (!failureMode.compare("all"))
			{
				m_failureMode = FailureMode_All;
			}
			else if (!failureMode.compare("any"))
			{
				m_failureMode = FailureMode_Any;
			}
			else
			{
				gEnv->pLog->LogError("Error in the %s behavior tree : the parallel node at %d has an invalid value for the attribute failureMode.", context.treeName, node->getLine());
			}
		}

		stack_string successMode = node->getAttr("successMode");
		if (!successMode.empty())
		{
			if (!successMode.compare("any"))
			{
				m_successMode = SuccessMode_Any;
			}
			else if (!successMode.compare("all"))
			{
				m_successMode = SuccessMode_All;
			}
			else
			{
				gEnv->pLog->LogError("Error in the %s behavior tree : the parallel node at %d has an invalid value for the attribute successMode.", context.treeName, node->getLine());
			}
		}

		return CompositeWithChildLoader::LoadFromXml(node, context);
	}

#ifdef USING_BEHAVIOR_TREE_XML_DESCRIPTION_CREATION
	virtual XmlNodeRef CreateXmlDescription() override
	{
		XmlNodeRef xml = BaseClass::CreateXmlDescription();

		xml->setTag("Parallel");
		if (m_failureMode == FailureMode_All)
			xml->setAttr("failureMode", "all");
		if (m_successMode == SuccessMode_Any)
			xml->setAttr("successMode", "any");

		return xml;
	}
#endif

#ifdef USING_BEHAVIOR_TREE_SERIALIZATION
	virtual void Serialize(Serialization::IArchive& archive) override
	{
		archive(m_successMode, "successMode", "^Success Mode");
		archive(m_failureMode, "failureMode", "^Failure Mode");

		BaseClass::Serialize(archive);

		if (m_children.empty())
			archive.error(m_children, "Must contain a node");

		if (m_children.size() == 1)
			archive.warning(m_children, "Parallel with only one node is superfluous");
	}
#endif

	virtual void OnInitialize(const UpdateContext& context) override
	{
		RuntimeData& runtimeData = GetRuntimeData<RuntimeData>(context);

		uint32 runningChildren = 0;
		for (size_t childIndex = 0, n = m_children.size(); childIndex < n; ++childIndex)
		{
			runningChildren |= 1 << childIndex;
		}

		runtimeData.runningChildren = runningChildren;
		runtimeData.successCount = 0;
		runtimeData.failureCount = 0;
	}

	virtual void OnTerminate(const UpdateContext& context) override
	{
		RuntimeData& runtimeData = GetRuntimeData<RuntimeData>(context);

		const uint32 runningChildren = runtimeData.runningChildren;
		for (size_t childIndex = 0, n = m_children.size(); childIndex < n; ++childIndex)
		{
			const bool childIsRunning = (runningChildren & (1 << childIndex)) != 0;
			if (childIsRunning)
			{
				m_children[childIndex]->Terminate(context);
			}
		}
	}

	virtual Status Update(const UpdateContext& context) override
	{
		RuntimeData& runtimeData = GetRuntimeData<RuntimeData>(context);

		uint32& runningChildren = runtimeData.runningChildren;
		uint32& successCount = runtimeData.successCount;
		uint32& failureCount = runtimeData.failureCount;

		for (size_t childIndex = 0, n = m_children.size(); childIndex < n; ++childIndex)
		{
			const bool childIsRunning = (runningChildren & (1 << childIndex)) != 0;

			if (childIsRunning)
			{
				const Status s = m_children[childIndex]->Tick(context);

				if (s == Running)
					continue;
				else
				{
					if (s == Success)
						++successCount;
					else
						++failureCount;

					// Mark child as not running
					runningChildren = runningChildren & ~uint32(1u << childIndex);
				}
			}
		}

		if (m_successMode == SuccessMode_All)
		{
			if (successCount == m_children.size())
			{
				return Success;
			}
		}
		else if (m_successMode == SuccessMode_Any)
		{
			if (successCount > 0)
			{
				return Success;
			}
		}

		if (m_failureMode == FailureMode_All)
		{
			if (failureCount == m_children.size())
			{
				return Failure;
			}
		}
		else if (m_failureMode == FailureMode_Any)
		{
			if (failureCount > 0)
			{
				return Failure;
			}
		}

		return Running;
	}

	enum SuccessMode
	{
		SuccessMode_Any,
		SuccessMode_All,
	};

	enum FailureMode
	{
		FailureMode_Any,
		FailureMode_All,
	};

private:
	virtual void HandleEvent(const EventContext& context, const Event& event) override
	{
		RuntimeData& runtimeData = GetRuntimeData<RuntimeData>(context);

		const uint32 runningChildren = runtimeData.runningChildren;
		for (size_t childIndex = 0, n = m_children.size(); childIndex < n; ++childIndex)
		{
			const bool childIsRunning = (runningChildren & (1 << childIndex)) != 0;
			if (childIsRunning)
			{
				m_children[childIndex]->SendEvent(context, event);
			}
		}
	}

	SuccessMode m_successMode;
	FailureMode m_failureMode;
};

#ifdef USING_BEHAVIOR_TREE_SERIALIZATION
SERIALIZATION_ENUM_BEGIN_NESTED(Parallel, SuccessMode, "Success Mode")
SERIALIZATION_ENUM(Parallel::SuccessMode_Any, "SuccessMode_Any", "Any")
SERIALIZATION_ENUM(Parallel::SuccessMode_All, "SuccessMode_All", "All")
SERIALIZATION_ENUM_END()

SERIALIZATION_ENUM_BEGIN_NESTED(Parallel, FailureMode, "Failure Mode")
SERIALIZATION_ENUM(Parallel::FailureMode_Any, "FailureMode_Any", "Any")
SERIALIZATION_ENUM(Parallel::FailureMode_All, "FailureMode_All", "All")
SERIALIZATION_ENUM_END()
#endif

//////////////////////////////////////////////////////////////////////////

// The loop fails when the child fails, otherwise the loop says it's running.
//
// If the child tick reports back that the child succeeded, the child will
// immediately be ticked once more IF it was running _before_ the tick.
// The effect of this is that if you have a child that runs over a period
// of time, that child will be restarted immediately when it succeeds.
// This behavior prevents the case where a non-instant loop's child could
// mishandle an event if it was received during the window of time between
// succeeding and being restarted by the loop node.
//
// However, if the child immediately succeeds, it will be ticked
// only the next frame. Otherwise, it would get stuck in an infinite loop.

class Loop : public Decorator
{
	typedef Decorator BaseClass;

public:
	struct RuntimeData
	{
		uint8 amountOfTimesChildSucceeded;
		bool  childWasRunningLastTick;

		RuntimeData()
			: amountOfTimesChildSucceeded(0)
			, childWasRunningLastTick(false)
		{
		}
	};

	Loop()
		: m_desiredRepeatCount(0)
	{
	}

	virtual LoadResult LoadFromXml(const XmlNodeRef& xml, const LoadContext& context) override
	{
		IF_UNLIKELY (BaseClass::LoadFromXml(xml, context) == LoadFailure)
			return LoadFailure;

		m_desiredRepeatCount = 0;     // 0 means infinite
		xml->getAttr("count", m_desiredRepeatCount);

		return LoadSuccess;
	}

#ifdef USING_BEHAVIOR_TREE_XML_DESCRIPTION_CREATION
	virtual XmlNodeRef CreateXmlDescription() override
	{
		XmlNodeRef xml = BaseClass::CreateXmlDescription();
		xml->setTag("Loop");
		if (m_desiredRepeatCount)
			xml->setAttr("count", m_desiredRepeatCount);
		return xml;
	}
#endif

#ifdef USING_BEHAVIOR_TREE_SERIALIZATION
	virtual void Serialize(Serialization::IArchive& archive) override
	{
		archive(m_desiredRepeatCount, "repeatCount", "Repeat count (0 means infinite)");
		BaseClass::Serialize(archive);
	}
#endif

protected:
	virtual Status Update(const UpdateContext& context) override
	{
		RuntimeData& runtimeData = GetRuntimeData<RuntimeData>(context);

		Status childStatus = m_child->Tick(context);

		if (childStatus == Success)
		{
			const bool finiteLoop = (m_desiredRepeatCount > 0);
			if (finiteLoop)
			{
				if (runtimeData.amountOfTimesChildSucceeded + 1 >= m_desiredRepeatCount)
				{
					return Success;
				}

				++runtimeData.amountOfTimesChildSucceeded;
			}

			if (runtimeData.childWasRunningLastTick)
			{
				childStatus = m_child->Tick(context);
			}
		}

		runtimeData.childWasRunningLastTick = (childStatus == Running);

		if (childStatus == Failure)
		{
			return Failure;
		}

		return Running;
	}

private:
	uint8 m_desiredRepeatCount;     // 0 means infinite
};

//////////////////////////////////////////////////////////////////////////

// Similar to the loop. It will tick the child node and keep running
// while it's running. If the child succeeds, this node succeeds.
// If the child node fails we try again, a defined amount of times.
// The only reason this is its own node and not a configuration of
// the Loop node is because of readability. It became hard to read.
class LoopUntilSuccess : public Decorator
{
	typedef Decorator BaseClass;

public:
	struct RuntimeData
	{
		uint8 failedAttemptCountSoFar;
		bool  childWasRunningLastTick;

		RuntimeData()
			: failedAttemptCountSoFar(0)
			, childWasRunningLastTick(false)
		{
		}
	};

	LoopUntilSuccess()
		: m_maxAttemptCount(0)
	{
	}

	virtual LoadResult LoadFromXml(const XmlNodeRef& xml, const LoadContext& context) override
	{
		IF_UNLIKELY (BaseClass::LoadFromXml(xml, context) == LoadFailure)
			return LoadFailure;

		m_maxAttemptCount = 0;     // 0 means infinite
		xml->getAttr("attemptCount", m_maxAttemptCount);

		return LoadSuccess;
	}

#ifdef USING_BEHAVIOR_TREE_XML_DESCRIPTION_CREATION
	virtual XmlNodeRef CreateXmlDescription() override
	{
		XmlNodeRef xml = BaseClass::CreateXmlDescription();

		xml->setTag("LoopUntilSuccess");
		if (m_maxAttemptCount)
			xml->setAttr("attemptCount", m_maxAttemptCount);

		return xml;
	}
#endif

#ifdef USING_BEHAVIOR_TREE_SERIALIZATION
	virtual void Serialize(Serialization::IArchive& archive) override
	{
		archive(m_maxAttemptCount, "repeatCount", "Attempt count (0 means infinite)");
		BaseClass::Serialize(archive);
	}
#endif

protected:
	virtual Status Update(const UpdateContext& context) override
	{
		RuntimeData& runtimeData = GetRuntimeData<RuntimeData>(context);

		Status childStatus = m_child->Tick(context);

		if (childStatus == Failure)
		{
			const bool finiteLoop = (m_maxAttemptCount > 0);
			if (finiteLoop)
			{
				if (runtimeData.failedAttemptCountSoFar + 1 >= m_maxAttemptCount)
				{
					return Failure;
				}

				++runtimeData.failedAttemptCountSoFar;
			}

			if (runtimeData.childWasRunningLastTick)
			{
				childStatus = m_child->Tick(context);
			}
		}

		runtimeData.childWasRunningLastTick = (childStatus == Running);

		if (childStatus == Success)
		{
			return Success;
		}

		return Running;
	}

private:
	uint8 m_maxAttemptCount;     // 0 means infinite
};

//////////////////////////////////////////////////////////////////////////

struct Case
{
	LoadResult LoadFromXml(const XmlNodeRef& xml, const LoadContext& context)
	{
		if (!xml->isTag("Case"))
		{
			gEnv->pLog->LogError("Priority node must only contain childs nodes of the type 'Case', and there is a child of the '%s' at line '%d'.", xml->getTag(), xml->getLine());
			return LoadFailure;
		}

		string conditionString = xml->getAttr("condition");
		if (conditionString.empty())
			conditionString.Format("true");

		condition.Reset(conditionString, context.variableDeclarations);
		if (!condition.Valid())
		{
			gEnv->pLog->LogError("Priority case condition '%s' couldn't be parsed.", conditionString.c_str());
			return LoadFailure;
		}

#ifdef USING_BEHAVIOR_TREE_EDITOR
		m_conditionString = conditionString;
#endif

		if (!xml->getChildCount() == 1)
		{
			gEnv->pLog->LogError("Priority case must have exactly one child.");
			return LoadFailure;
		}

		XmlNodeRef childXml = xml->getChild(0);
		node = context.nodeFactory.CreateNodeFromXml(childXml, context);
		if (!node)
		{
			gEnv->pLog->LogError("Priority case failed to load child.");
			return LoadFailure;
		}

		return LoadSuccess;
	}

#ifdef USING_BEHAVIOR_TREE_XML_DESCRIPTION_CREATION
	XmlNodeRef CreateXmlDescription()
	{
		XmlNodeRef xml = GetISystem()->CreateXmlNode("Case");

		if (node)
		{
			xml->setAttr("condition", m_conditionString);
			xml->addChild(node->CreateXmlDescription());
		}

		return xml;
	}
#endif

#ifdef USING_BEHAVIOR_TREE_SERIALIZATION
	void Serialize(Serialization::IArchive& archive)
	{
		archive(m_conditionString, "condition", "^Condition");
		if (m_conditionString.empty())
			archive.error(m_conditionString, "Condition must be specified");

		archive(node, "node", "+<>" NODE_COMBOBOX_FIXED_WIDTH ">");
		if (!node)
			archive.error(node, "Node must be specified");
	}
#endif

#ifdef USING_BEHAVIOR_TREE_EDITOR
	string m_conditionString;
#endif

	Variables::Expression condition;
	INodePtr              node;
};

class Priority : public Node
{
	typedef Node BaseClass;

private:
	typedef std::vector<Case> Cases;
	Cases m_cases;

public:
	typedef uint8 CaseIndexType;

	struct RuntimeData
	{
		CaseIndexType currentCaseIndex;

		RuntimeData()
			: currentCaseIndex(0)
		{
		}
	};

	virtual LoadResult LoadFromXml(const XmlNodeRef& xml, const LoadContext& context) override
	{
		IF_UNLIKELY (BaseClass::LoadFromXml(xml, context) == LoadFailure)
			return LoadFailure;

		const int childCount = xml->getChildCount();
		IF_UNLIKELY (childCount < 1)
		{
			ErrorReporter(*this, context).LogError("A priority node must have at least one case childs.");
			return LoadFailure;
		}

		const size_t maxChildCount = std::numeric_limits<CaseIndexType>::max();
		IF_UNLIKELY ((size_t)childCount >= maxChildCount)
		{
			ErrorReporter(*this, context).LogError("Max %d children allowed.", maxChildCount);
			return LoadFailure;
		}

		const int defaultChildIndex = childCount - 1;
		for (int i = 0; i < childCount; ++i)
		{
			Case priorityCase;
			XmlNodeRef caseXml = xml->getChild(i);

			IF_UNLIKELY (priorityCase.LoadFromXml(caseXml, context) == LoadFailure)
			{
				ErrorReporter(*this, context).LogError("Failed to load Case.");
				return LoadFailure;
			}

			m_cases.push_back(priorityCase);
		}

		return LoadSuccess;
	}

#ifdef USING_BEHAVIOR_TREE_XML_DESCRIPTION_CREATION
	virtual XmlNodeRef CreateXmlDescription() override
	{
		XmlNodeRef xml = BaseClass::CreateXmlDescription();

		xml->setTag("Priority");

		for (Cases::iterator caseIt = m_cases.begin(), end = m_cases.end(); caseIt != end; ++caseIt)
			xml->addChild(caseIt->CreateXmlDescription());

		return xml;
	}
#endif

#ifdef USING_BEHAVIOR_TREE_SERIALIZATION
	virtual void Serialize(Serialization::IArchive& archive) override
	{
		archive(m_cases, "cases", "^[<>]");

		if (m_cases.empty())
			archive.error(m_cases, "Must contain a case");

		if (m_cases.size() == 1)
			archive.warning(m_cases, "Priority with only one case is superfluous");

		BaseClass::Serialize(archive);
	}
#endif

protected:
	virtual void OnInitialize(const UpdateContext& context) override
	{
		RuntimeData& runtimeData = GetRuntimeData<RuntimeData>(context);

		// Set the current child index to be out of bounds to
		// force a re-evaluation.
		runtimeData.currentCaseIndex = static_cast<CaseIndexType>(m_cases.size());
	}

	virtual void OnTerminate(const UpdateContext& context) override
	{
		CaseIndexType& currentChildIndex = GetRuntimeData<RuntimeData>(context).currentCaseIndex;

		if (currentChildIndex < m_cases.size())
		{
			m_cases[currentChildIndex].node->Terminate(context);
			currentChildIndex = static_cast<CaseIndexType>(m_cases.size());
		}
	}

	virtual Status Update(const UpdateContext& context) override
	{
		CRY_PROFILE_FUNCTION(PROFILE_AI);

		CaseIndexType& currentChildIndex = GetRuntimeData<RuntimeData>(context).currentCaseIndex;

		if (context.variables.VariablesChangedSinceLastTick() || currentChildIndex >= m_cases.size())
		{
			CaseIndexType selectedChild = PickFirstChildWithSatisfiedCondition(context.variables.collection);
			if (selectedChild != currentChildIndex)
			{
				if (currentChildIndex < m_cases.size())
					m_cases[currentChildIndex].node->Terminate(context);

				currentChildIndex = selectedChild;
			}
		}

		IF_UNLIKELY (currentChildIndex >= m_cases.size())
			return Failure;

		return m_cases[currentChildIndex].node->Tick(context);
	}

private:
	virtual void HandleEvent(const EventContext& context, const Event& event) override
	{
		const CaseIndexType& i = GetRuntimeData<RuntimeData>(context).currentCaseIndex;

		if (i < m_cases.size())
			m_cases[i].node->SendEvent(context, event);
	}

	CaseIndexType PickFirstChildWithSatisfiedCondition(Variables::Collection& variables)
	{
		for (Cases::iterator caseIt = m_cases.begin(), end = m_cases.end(); caseIt != end; ++caseIt)
		{
			if (caseIt->condition.Evaluate(variables))
			{
				return (CaseIndexType)std::distance(m_cases.begin(), caseIt);
			}
		}

		return static_cast<CaseIndexType>(m_cases.size());
	}
};

//////////////////////////////////////////////////////////////////////////

#if defined(DEBUG_MODULAR_BEHAVIOR_TREE) || defined(USING_BEHAVIOR_TREE_EDITOR)
	#define STORE_INFORMATION_FOR_STATE_MACHINE_NODE
#endif

typedef byte StateIndex;
static const StateIndex StateIndexInvalid = ((StateIndex) ~0);

struct Transition
{
	LoadResult LoadFromXml(const XmlNodeRef& transitionXml)
	{
		destinationStateCRC32 = 0;
		destinationStateIndex = StateIndexInvalid;
#ifdef STORE_INFORMATION_FOR_STATE_MACHINE_NODE
		xmlLine = transitionXml->getLine();
#endif

		string to = transitionXml->getAttr("to");
		if (to.empty())
		{
			gEnv->pLog->LogError("Transition is missing 'to' attribute.");
			return LoadFailure;
		}

		destinationStateCRC32 = CCrc32::ComputeLowercase(to);

#ifdef STORE_INFORMATION_FOR_STATE_MACHINE_NODE
		destinationStateName = to;
#endif

		string onEvent = transitionXml->getAttr("onEvent");
		if (onEvent.empty())
		{
			gEnv->pLog->LogError("Transition is missing 'onEvent' attribute.");
			return LoadFailure;
		}

		triggerEvent = Event(onEvent);

#ifdef STORE_INFORMATION_FOR_STATE_MACHINE_NODE
		triggerEventName = onEvent;
#endif

		return LoadSuccess;
	}

#ifdef USING_BEHAVIOR_TREE_XML_DESCRIPTION_CREATION
	XmlNodeRef CreateXmlDescription() const
	{
		XmlNodeRef xml = GetISystem()->CreateXmlNode("Transition");
		xml->setAttr("to", destinationStateName);
		xml->setAttr("onEvent", triggerEventName);
		return xml;
	}
#endif

#ifdef USING_BEHAVIOR_TREE_SERIALIZATION
	void Serialize(Serialization::IArchive& archive)
	{
		archive(triggerEventName, "triggerEventName", "^>" STATE_TRANSITION_EVENT_FIXED_WIDTH ">Trigger event");
		if (triggerEventName.empty())
			archive.error(triggerEventName, "Must specify a trigger event");

		archive(destinationStateName, "destinationState", "^Destination state");
		if (destinationStateName.empty())
			archive.error(destinationStateName, "Must specify a destination state");
	}
#endif

#ifdef STORE_INFORMATION_FOR_STATE_MACHINE_NODE
	string destinationStateName;
	string triggerEventName;
	int    xmlLine;
#endif

	uint32     destinationStateCRC32;
	Event      triggerEvent;
	StateIndex destinationStateIndex;
};

typedef std::vector<Transition> Transitions;

struct State
{
	State()
		: transitions()
		, nameCRC32(0)
#ifdef STORE_INFORMATION_FOR_STATE_MACHINE_NODE
		, name()
#endif
#ifdef DEBUG_MODULAR_BEHAVIOR_TREE
		, xmlLine(0)
#endif
		, node()
	{
	}

	LoadResult LoadFromXml(const XmlNodeRef& stateXml, const LoadContext& context)
	{
		if (!stateXml->isTag("State"))
		{
			gEnv->pLog->LogError("StateMachine node must contain child nodes of the type 'State', and there is a child of the '%s' at line '%d'.", stateXml->getTag(), stateXml->getLine());
			return LoadFailure;
		}

		const char* stateName;
		if (stateXml->getAttr("name", &stateName))
		{
			nameCRC32 = CCrc32::ComputeLowercase(stateName);

#ifdef STORE_INFORMATION_FOR_STATE_MACHINE_NODE
			name = stateName;
#endif

#ifdef DEBUG_MODULAR_BEHAVIOR_TREE
			xmlLine = stateXml->getLine();
#endif  // DEBUG_MODULAR_BEHAVIOR_TREE

		}
		else
		{
			gEnv->pLog->LogError("A state node must contain a valid 'name' attribute. The state node at the line %d does not.", stateXml->getLine());
			return LoadFailure;
		}

		const XmlNodeRef transitionsXml = stateXml->findChild("Transitions");
		if (transitionsXml)
		{
			for (int i = 0; i < transitionsXml->getChildCount(); ++i)
			{
				Transition transition;
				if (transition.LoadFromXml(transitionsXml->getChild(i)) == LoadFailure)
				{
					gEnv->pLog->LogError("Failed to load transition.");
					return LoadFailure;
				}

				transitions.push_back(transition);
			}
		}

		const XmlNodeRef behaviorTreeXml = stateXml->findChild("BehaviorTree");
		if (!behaviorTreeXml)
		{
#ifdef STORE_INFORMATION_FOR_STATE_MACHINE_NODE
			gEnv->pLog->LogError("A state node must contain a 'BehaviorTree' child. The state node '%s' at %d does not.", name.c_str(), stateXml->getLine());
#else
			gEnv->pLog->LogError("A state node must contain a 'BehaviorTree' child. The state node at the line %d does not.", stateXml->getLine());
#endif
			return LoadFailure;
		}

		if (behaviorTreeXml->getChildCount() != 1)
		{
			gEnv->pLog->LogError("A state node must contain a 'BehaviorTree' child, which in turn must have exactly one child.");
			return LoadFailure;
		}

		node = context.nodeFactory.CreateNodeFromXml(behaviorTreeXml->getChild(0), context);
		if (!node)
		{
			gEnv->pLog->LogError("State failed to load child.");
			return LoadFailure;
		}

		return LoadSuccess;
	}

#ifdef USING_BEHAVIOR_TREE_XML_DESCRIPTION_CREATION
	XmlNodeRef CreateXmlDescription()
	{
		XmlNodeRef stateXml = GetISystem()->CreateXmlNode("State");

		stateXml->setAttr("name", name);

		if (transitions.size() > 0)
		{
			XmlNodeRef transitionXml = GetISystem()->CreateXmlNode("Transitions");
			for (Transitions::iterator transitionIt = transitions.begin(), end = transitions.end(); transitionIt != end; ++transitionIt)
				transitionXml->addChild(transitionIt->CreateXmlDescription());

			stateXml->addChild(transitionXml);
		}

		if (node)
		{
			XmlNodeRef behaviorXml = GetISystem()->CreateXmlNode("BehaviorTree");
			behaviorXml->addChild(node->CreateXmlDescription());
			stateXml->addChild(behaviorXml);
		}

		return stateXml;
	}
#endif

#ifdef USING_BEHAVIOR_TREE_SERIALIZATION
	void Serialize(Serialization::IArchive& archive)
	{
	#ifdef DEBUG_MODULAR_BEHAVIOR_TREE
		HandleXmlLineNumberSerialization(archive, xmlLine);
	#endif

		archive(name, "name", "^State Name");
		if (name.empty())
			archive.error(name, "State name must be specified");

		archive(transitions, "transitions", "+[<>]Transitions");

		archive(node, "node", "+<>" NODE_COMBOBOX_FIXED_WIDTH ">");
		if (!node)
			archive.error(node, "Node must be specified");
	}
#endif

	const Transition* GetTransitionForEvent(const Event& event)
	{
		Transitions::iterator it = transitions.begin();
		Transitions::iterator end = transitions.end();

		for (; it != end; ++it)
		{
			const Transition& transition = *it;
			if (transition.triggerEvent == event)
				return &transition;
		}

		return NULL;
	}

	Transitions transitions;
	uint32      nameCRC32;

#ifdef STORE_INFORMATION_FOR_STATE_MACHINE_NODE
	string name;
#endif

#ifdef DEBUG_MODULAR_BEHAVIOR_TREE
	uint32 xmlLine;
#endif

	INodePtr node;
};

// A state machine is a composite node that holds one or more children.
// There is one selected child at any given time. Default is the first.
//
// A state machine's status is the same as that of it's currently
// selected child: running, succeeded or failed.
//
// One state can transition to another state on a specified event.
// The transition happens immediately in the sense of selection,
// but the new state is only updated the next time the state machine
// itself is being updated.
//
// Transitioning to the same state will cause the state to first be
// terminated and then initialized and updated.
class StateMachine : public Node
{
	typedef Node BaseClass;

public:
	typedef uint8 StateIndexType;

	struct RuntimeData
	{
		StateIndexType currentStateIndex;
		StateIndexType pendingStateIndex;
		bool           transitionIsPending;

		RuntimeData()
			: currentStateIndex(0)
			, pendingStateIndex(0)
			, transitionIsPending(false)
		{
		}
	};

	virtual LoadResult LoadFromXml(const XmlNodeRef& xml, const LoadContext& context) override
	{
		const size_t maxChildCount = std::numeric_limits<StateIndexType>::max();

		IF_UNLIKELY ((size_t)xml->getChildCount() >= maxChildCount)
		{
			ErrorReporter(*this, context).LogError("Too many children. Max %d allowed.", maxChildCount);
			return LoadFailure;
		}

		IF_UNLIKELY (xml->getChildCount() <= 0)
		{
			ErrorReporter(*this, context).LogError("A state machine node must contain at least one child state node.");
			return LoadFailure;
		}

		for (int i = 0; i < xml->getChildCount(); ++i)
		{
			State state;
			XmlNodeRef stateXml = xml->getChild(i);

			if (state.LoadFromXml(stateXml, context) == LoadFailure)
			{
				ErrorReporter(*this, context).LogError("Failed to load State.");
				return LoadFailure;
			}

			m_states.push_back(state);
		}

		return LinkAllTransitions();
	}

#ifdef USING_BEHAVIOR_TREE_XML_DESCRIPTION_CREATION
	virtual XmlNodeRef CreateXmlDescription() override
	{
		XmlNodeRef xml = BaseClass::CreateXmlDescription();

		xml->setTag("StateMachine");

		for (States::iterator stateIt = m_states.begin(), end = m_states.end(); stateIt != end; ++stateIt)
			xml->addChild(stateIt->CreateXmlDescription());

		return xml;
	}
#endif

#ifdef USING_BEHAVIOR_TREE_SERIALIZATION
	virtual void Serialize(Serialization::IArchive& archive) override
	{
		archive(m_states, "states", "^[+<>]States");

		if (m_states.empty())
			archive.error(m_states, "Must contain a state.");

		if (m_states.size() == 1)
			archive.warning(m_states, "State machine with only one state is superfluous");

		BaseClass::Serialize(archive);
	}
#endif

#if defined(STORE_INFORMATION_FOR_STATE_MACHINE_NODE) && defined(USING_BEHAVIOR_TREE_NODE_CUSTOM_DEBUG_TEXT)
	virtual void GetCustomDebugText(const UpdateContext& updateContext, stack_string& debugText) const
	{
		const RuntimeData& runtimeData = GetRuntimeData<RuntimeData>(updateContext);
		if (runtimeData.currentStateIndex < m_states.size())
			debugText += m_states[runtimeData.currentStateIndex].name.c_str();
	}
#endif

protected:
	virtual void OnInitialize(const UpdateContext& context) override
	{
		assert(m_states.size() > 0);

		RuntimeData& runtimeData = GetRuntimeData<RuntimeData>(context);
		runtimeData.currentStateIndex = 0;
		runtimeData.pendingStateIndex = 0;
		runtimeData.transitionIsPending = false;
	}

	virtual void OnTerminate(const UpdateContext& context) override
	{
		RuntimeData& runtimeData = GetRuntimeData<RuntimeData>(context);

		if (runtimeData.currentStateIndex < m_states.size())
			m_states[runtimeData.currentStateIndex].node->Terminate(context);
	}

	virtual Status Update(const UpdateContext& context) override
	{
		CRY_PROFILE_FUNCTION(PROFILE_AI);

		RuntimeData& runtimeData = GetRuntimeData<RuntimeData>(context);

		if (runtimeData.transitionIsPending)
		{
			runtimeData.transitionIsPending = false;
			m_states[runtimeData.currentStateIndex].node->Terminate(context);
			runtimeData.currentStateIndex = runtimeData.pendingStateIndex;
		}

		return m_states[runtimeData.currentStateIndex].node->Tick(context);
	}

private:
	virtual void HandleEvent(const EventContext& context, const Event& event) override
	{
		RuntimeData& runtimeData = GetRuntimeData<RuntimeData>(context);

		StateIndexType& pendingStateIndex = runtimeData.pendingStateIndex;

		if (pendingStateIndex >= m_states.size())
			return;

		const Transition* transition = m_states[pendingStateIndex].GetTransitionForEvent(event);
		if (transition)
		{
			assert(transition->destinationStateIndex != StateIndexInvalid);
			pendingStateIndex = transition->destinationStateIndex;
			runtimeData.transitionIsPending = true;
		}

		m_states[runtimeData.currentStateIndex].node->SendEvent(context, event);
	}

	LoadResult LinkAllTransitions()
	{
		for (States::iterator stateIt = m_states.begin(), end = m_states.end(); stateIt != end; ++stateIt)
		{
			State& state = *stateIt;

			for (Transitions::iterator transitionIt = state.transitions.begin(), end = state.transitions.end(); transitionIt != end; ++transitionIt)
			{
				Transition& transition = *transitionIt;

				StateIndex stateIndex = GetIndexOfState(transition.destinationStateCRC32);
				if (stateIndex == StateIndexInvalid)
				{
#ifdef STORE_INFORMATION_FOR_STATE_MACHINE_NODE
					gEnv->pLog->LogError("Cannot transition to unknown state '%s' at line %d.", transition.destinationStateName, transition.xmlLine);
#endif
					return LoadFailure;
				}

				transition.destinationStateIndex = stateIndex;
			}
		}

		return LoadSuccess;
	}

	StateIndex GetIndexOfState(uint32 stateNameLowerCaseCRC32)
	{
		size_t index, size = m_states.size();
		assert(size < ((1 << (sizeof(StateIndex) * 8)) - 1));               // -1 because StateIndexInvalid is reserved.
		for (index = 0; index < size; index++)
		{
			if (m_states[index].nameCRC32 == stateNameLowerCaseCRC32)
				return static_cast<StateIndex>(index);
		}

		return StateIndexInvalid;
	}

	typedef std::vector<State> States;
	States m_states;
};

//////////////////////////////////////////////////////////////////////////

// Send an event to this behavior tree. In reality, the event will be
// queued and dispatched at a later time, often the next frame.
class SendEvent : public Action
{
	typedef Action BaseClass;

public:
	struct RuntimeData
	{
	};

	virtual LoadResult LoadFromXml(const XmlNodeRef& xml, const LoadContext& context) override
	{
		IF_UNLIKELY (BaseClass::LoadFromXml(xml, context) == LoadFailure)
			return LoadFailure;

		const stack_string eventName = xml->getAttr("name");
		IF_UNLIKELY (eventName.empty())
		{
			ErrorReporter(*this, context).LogError("Could not find the 'name' attribute.");
			return LoadFailure;
		}

		m_eventToSend = Event(eventName);

		return LoadSuccess;
	}

#ifdef USING_BEHAVIOR_TREE_XML_DESCRIPTION_CREATION
	virtual XmlNodeRef CreateXmlDescription() override
	{
		XmlNodeRef xml = BaseClass::CreateXmlDescription();
		xml->setTag("SendEvent");
		xml->setAttr("name", m_eventToSend.GetName());
		return xml;
	}
#endif

#ifdef USING_BEHAVIOR_TREE_SERIALIZATION
	virtual void Serialize(Serialization::IArchive& archive) override
	{
		archive(m_eventToSend, "event", "^");
		if (strlen(m_eventToSend.GetName()) == 0)
			archive.error(m_eventToSend, "Event must be specified");
		BaseClass::Serialize(archive);
	}
#endif

protected:
	virtual void OnInitialize(const UpdateContext& context) override
	{
		gAIEnv.pBehaviorTreeManager->HandleEvent(context.entityId, m_eventToSend);
	}

	virtual Status Update(const UpdateContext& context) override
	{
		return Success;
	}

private:
	Event m_eventToSend;
};

//////////////////////////////////////////////////////////////////////////

// Same as SendEvent with the exception that this node never finishes.
// Usually used for transitions in state machines etc.
class SendTransitionEvent : public SendEvent
{
	typedef SendEvent BaseClass;

public:
	virtual Status Update(const UpdateContext& context) override
	{
		return Running;
	}

#ifdef USING_BEHAVIOR_TREE_XML_DESCRIPTION_CREATION
	virtual XmlNodeRef CreateXmlDescription() override
	{
		XmlNodeRef xml = BaseClass::CreateXmlDescription();
		xml->setTag("SendTransitionEvent");
		return xml;
	}
#endif

};

//////////////////////////////////////////////////////////////////////////

#if defined(USING_BEHAVIOR_TREE_NODE_CUSTOM_DEBUG_TEXT) || defined(USING_BEHAVIOR_TREE_EDITOR)
	#define STORE_CONDITION_STRING
#endif

class IfCondition : public Decorator
{
public:
	typedef Decorator BaseClass;

	struct RuntimeData
	{
		bool gateIsOpen;

		RuntimeData()
			: gateIsOpen(false)
		{
		}
	};

	IfCondition()
		: m_valueToCheck(false)
	{
	}

	virtual LoadResult LoadFromXml(const XmlNodeRef& xml, const LoadContext& context) override
	{
		const stack_string conditionString = xml->getAttr("condition");
		if (conditionString.empty())
		{
			ErrorReporter(*this, context).LogError("Attribute 'condition' is missing or empty.");
			return LoadFailure;
		}

#ifdef STORE_CONDITION_STRING
		m_conditionString = conditionString.c_str();
#endif

		m_valueToCheck = true;
		if (xml->getAttr("equalTo", m_valueToCheck))
		{
			ErrorReporter(*this, context).LogWarning("Deprecated attribute 'equalTo' was used. Please change the expression.");
		}

		m_condition = Variables::Expression(conditionString, context.variableDeclarations);
		if (!m_condition.Valid())
		{
			ErrorReporter(*this, context).LogError("Failed to parse condition '%s'.", conditionString.c_str());
			return LoadFailure;
		}

		return LoadChildFromXml(xml, context);
	}

#ifdef USING_BEHAVIOR_TREE_XML_DESCRIPTION_CREATION
	virtual XmlNodeRef CreateXmlDescription() override
	{
		XmlNodeRef xml = BaseClass::CreateXmlDescription();
		xml->setTag("IfCondition");
		xml->setAttr("condition", m_conditionString);
		return xml;
	}
#endif

#ifdef USING_BEHAVIOR_TREE_SERIALIZATION
	virtual void Serialize(Serialization::IArchive& archive) override
	{
		archive(m_conditionString, "condition", "^Condition");
		if (m_conditionString.empty())
			archive.error(m_conditionString, "Condition must be specified");

		BaseClass::Serialize(archive);
	}
#endif

#ifdef USING_BEHAVIOR_TREE_NODE_CUSTOM_DEBUG_TEXT
	virtual void GetCustomDebugText(const UpdateContext& updateContext, stack_string& debugText) const
	{
		debugText.Format("(%s) == %s", m_conditionString.c_str(), m_valueToCheck ? "true" : "false");
	}
#endif

protected:
	virtual void OnInitialize(const UpdateContext& context) override
	{
		RuntimeData& runtimeData = GetRuntimeData<RuntimeData>(context);

		runtimeData.gateIsOpen = false;

		if (m_condition.Evaluate(context.variables.collection) == m_valueToCheck)
		{
			runtimeData.gateIsOpen = true;
		}
	}

	virtual Status Update(const UpdateContext& context) override
	{
		RuntimeData& runtimeData = GetRuntimeData<RuntimeData>(context);

		if (runtimeData.gateIsOpen)
			return BaseClass::Update(context);
		else
			return Failure;
	}

private:
#ifdef STORE_CONDITION_STRING
	string m_conditionString;
#endif
	Variables::Expression m_condition;
	bool m_valueToCheck;
};

//////////////////////////////////////////////////////////////////////////

class AssertCondition : public Action
{
	typedef Action BaseClass;

public:
	struct RuntimeData
	{
	};

	virtual LoadResult LoadFromXml(const XmlNodeRef& xml, const LoadContext& context) override
	{
		IF_UNLIKELY (BaseClass::LoadFromXml(xml, context) == LoadFailure)
			return LoadFailure;

		const stack_string conditionString = xml->getAttr("condition");
		if (conditionString.empty())
		{
			ErrorReporter(*this, context).LogError("Missing or invalid 'condition' attribute.");
			return LoadFailure;
		}

#ifdef USING_BEHAVIOR_TREE_NODE_CUSTOM_DEBUG_TEXT
		m_conditionString = conditionString.c_str();
#endif

		m_condition = Variables::Expression(conditionString, context.variableDeclarations);
		if (!m_condition.Valid())
		{
			ErrorReporter(*this, context).LogError("Failed to parse condition '%s'.", conditionString.c_str());
			return LoadFailure;
		}

		return LoadSuccess;
	}

#ifdef USING_BEHAVIOR_TREE_XML_DESCRIPTION_CREATION
	virtual XmlNodeRef CreateXmlDescription() override
	{
		XmlNodeRef xml = BaseClass::CreateXmlDescription();
		xml->setTag("AssertCondition");
		xml->setAttr("condition", m_conditionString);
		return xml;
	}
#endif

#ifdef USING_BEHAVIOR_TREE_SERIALIZATION
	virtual void Serialize(Serialization::IArchive& archive) override
	{
		archive(m_conditionString, "condition", "^Condition");
		if (m_conditionString.empty())
			archive.error(m_conditionString, "Condition must be specified");

		BaseClass::Serialize(archive);
	}
#endif

#ifdef USING_BEHAVIOR_TREE_NODE_CUSTOM_DEBUG_TEXT
	virtual void GetCustomDebugText(const UpdateContext& updateContext, stack_string& debugText) const
	{
		debugText.Format("%s", m_conditionString.c_str());
	}
#endif

protected:
	virtual Status Update(const UpdateContext& context) override
	{
		if (m_condition.Evaluate(context.variables.collection))
		{
			return Success;
		}

		return Failure;
	}

private:
#ifdef STORE_CONDITION_STRING
	string m_conditionString;
#endif
	Variables::Expression m_condition;
};

//////////////////////////////////////////////////////////////////////////

class MonitorCondition : public Action
{
	typedef Action BaseClass;

public:
	struct RuntimeData
	{
	};

	virtual LoadResult LoadFromXml(const XmlNodeRef& xml, const LoadContext& context) override
	{
		IF_UNLIKELY (BaseClass::LoadFromXml(xml, context) == LoadFailure)
			return LoadFailure;

		const stack_string conditionString = xml->getAttr("condition");
		if (conditionString.empty())
		{
			ErrorReporter(*this, context).LogError("Expected 'condition' attribute.");
			return LoadFailure;
		}

		m_condition = Variables::Expression(conditionString, context.variableDeclarations);
		IF_UNLIKELY (!m_condition.Valid())
		{
			ErrorReporter(*this, context).LogError("Couldn't get behavior variables.");
			return LoadFailure;
		}

#ifdef USING_BEHAVIOR_TREE_NODE_CUSTOM_DEBUG_TEXT
		m_conditionString = conditionString.c_str();
#endif

		return LoadSuccess;
	}

#ifdef USING_BEHAVIOR_TREE_NODE_CUSTOM_DEBUG_TEXT
	virtual void GetCustomDebugText(const UpdateContext& updateContext, stack_string& debugText) const override
	{
		debugText.Format("(%s)", m_conditionString.c_str());
	}
#endif

	virtual Status Update(const UpdateContext& context) override
	{
		CRY_PROFILE_FUNCTION(PROFILE_AI);

		if (m_condition.Evaluate(context.variables.collection))
		{
			return Success;
		}

		return Running;
	}

#ifdef USING_BEHAVIOR_TREE_XML_DESCRIPTION_CREATION
	virtual XmlNodeRef CreateXmlDescription() override
	{
		XmlNodeRef xml = BaseClass::CreateXmlDescription();
		xml->setTag("MonitorCondition");
		xml->setAttr("condition", m_conditionString);
		return xml;
	}
#endif

#ifdef USING_BEHAVIOR_TREE_SERIALIZATION
	virtual void Serialize(Serialization::IArchive& archive) override
	{
		archive(m_conditionString, "condition", "^Condition");
		if (m_conditionString.empty())
			archive.error(m_conditionString, "Condition must be specified");

		BaseClass::Serialize(archive);
	}
#endif

private:
#ifdef STORE_CONDITION_STRING
	string m_conditionString;
#endif
	Variables::Expression m_condition;
};

//////////////////////////////////////////////////////////////////////////

// A gate that opens in relation of the defined random chance
//

class RandomGate : public Decorator
{
public:
	typedef Decorator BaseClass;

	struct RuntimeData
	{
		bool gateIsOpen;

		RuntimeData()
			: gateIsOpen(false)
		{
		}
	};

	RandomGate()
		: m_opensWithChance(.0f)
	{
	}

	virtual LoadResult LoadFromXml(const XmlNodeRef& xml, const LoadContext& context) override
	{
		if (!xml->getAttr("opensWithChance", m_opensWithChance))
		{
			gEnv->pLog->LogError("RandomGate expected the 'opensWithChance' attribute at line %d.", xml->getLine());
			return LoadFailure;
		}

		m_opensWithChance = clamp_tpl(m_opensWithChance, .0f, 1.0f);

		return LoadChildFromXml(xml, context);
	}

#ifdef USING_BEHAVIOR_TREE_XML_DESCRIPTION_CREATION
	virtual XmlNodeRef CreateXmlDescription() override
	{
		XmlNodeRef xml = BaseClass::CreateXmlDescription();
		xml->setTag("RandomGate");
		xml->setAttr("opensWithChance", m_opensWithChance);
		return xml;
	}
#endif

#ifdef USING_BEHAVIOR_TREE_SERIALIZATION
	virtual void Serialize(Serialization::IArchive& archive) override
	{
		archive(m_opensWithChance, "opensWithChance", "^Chance to open");
		BaseClass::Serialize(archive);
	}
#endif

protected:
	virtual void OnInitialize(const UpdateContext& context) override
	{
		RuntimeData& runtimeData = GetRuntimeData<RuntimeData>(context);

		runtimeData.gateIsOpen = false;

		const float randomValue = cry_random(0.0f, 1.0f);
		if (randomValue <= m_opensWithChance)
			runtimeData.gateIsOpen = true;
	}

	virtual Status Update(const UpdateContext& context) override
	{
		RuntimeData& runtimeData = GetRuntimeData<RuntimeData>(context);

		if (runtimeData.gateIsOpen)
			return BaseClass::Update(context);
		else
			return Failure;
	}

private:
	float m_opensWithChance;
};

//////////////////////////////////////////////////////////////////////////

// Returns failure after X seconds
class Timeout : public Action
{
	typedef Action BaseClass;

public:
	struct RuntimeData
	{
		Timer timer;
	};

	Timeout()
		: m_duration(0.0f)
	{
	}

	virtual LoadResult LoadFromXml(const XmlNodeRef& xml, const LoadContext& context) override
	{
		if (Action::LoadFromXml(xml, context) == LoadFailure)
		{
			// TODO: Report error
			return LoadFailure;
		}

		if (!xml->getAttr("duration", m_duration))
		{
			// TODO: Report error
			return LoadFailure;
		}

		return LoadSuccess;
	}

#ifdef USING_BEHAVIOR_TREE_XML_DESCRIPTION_CREATION
	virtual XmlNodeRef CreateXmlDescription() override
	{
		XmlNodeRef xml = BaseClass::CreateXmlDescription();
		xml->setTag("Timeout");
		xml->setAttr("duration", m_duration);
		return xml;
	}
#endif

#ifdef USING_BEHAVIOR_TREE_SERIALIZATION
	virtual void Serialize(Serialization::IArchive& archive) override
	{
		archive(m_duration, "duration", "^Duration");
		BaseClass::Serialize(archive);
	}
#endif

	virtual void OnInitialize(const UpdateContext& context) override
	{
		RuntimeData& runtimeData = GetRuntimeData<RuntimeData>(context);
		runtimeData.timer.Reset(m_duration);
	}

	virtual Status Update(const UpdateContext& context) override
	{
		RuntimeData& runtimeData = GetRuntimeData<RuntimeData>(context);
		return runtimeData.timer.Elapsed() ? Failure : Running;
	}

#ifdef USING_BEHAVIOR_TREE_NODE_CUSTOM_DEBUG_TEXT
	virtual void GetCustomDebugText(const UpdateContext& updateContext, stack_string& debugText) const
	{
		const RuntimeData& runtimeData = GetRuntimeData<RuntimeData>(updateContext);
		debugText.Format("%0.1f (%0.1f)", runtimeData.timer.GetSecondsLeft(), m_duration);
	}
#endif

private:
	float m_duration;
};

//////////////////////////////////////////////////////////////////////////

// Returns success after X seconds
class Wait : public Action
{
	typedef Action BaseClass;

public:
	struct RuntimeData
	{
		Timer timer;
	};

	Wait()
		: m_duration(0.0f)
		, m_variation(0.0f)
	{
	}

	virtual LoadResult LoadFromXml(const XmlNodeRef& xml, const LoadContext& context)
	{
		if (Action::LoadFromXml(xml, context) == LoadFailure)
		{
			// TODO: Report error
			return LoadFailure;
		}

		if (!xml->getAttr("duration", m_duration))
		{
			// TODO: Report error
			return LoadFailure;
		}

		xml->getAttr("variation", m_variation);

		return LoadSuccess;
	}

#ifdef USING_BEHAVIOR_TREE_XML_DESCRIPTION_CREATION
	virtual XmlNodeRef CreateXmlDescription() override
	{
		XmlNodeRef xml = BaseClass::CreateXmlDescription();
		xml->setTag("Wait");
		xml->setAttr("duration", m_duration);
		xml->setAttr("variation", m_variation);
		return xml;
	}
#endif

#ifdef USING_BEHAVIOR_TREE_SERIALIZATION
	virtual void Serialize(Serialization::IArchive& archive) override
	{
		archive(m_duration, "duration", "^<Duration");
		archive(m_variation, "variation", "^<Variation");
		BaseClass::Serialize(archive);
	}
#endif

	virtual void OnInitialize(const UpdateContext& context)
	{
		if (m_duration >= 0.0f)
		{
			RuntimeData& runtimeData = GetRuntimeData<RuntimeData>(context);
			runtimeData.timer.Reset(m_duration, m_variation);
		}
	}

	virtual Status Update(const UpdateContext& context)
	{
		RuntimeData& runtimeData = GetRuntimeData<RuntimeData>(context);
		return runtimeData.timer.Elapsed() ? Success : Running;
	}

#ifdef USING_BEHAVIOR_TREE_NODE_CUSTOM_DEBUG_TEXT
	virtual void GetCustomDebugText(const UpdateContext& updateContext, stack_string& debugText) const
	{
		const RuntimeData& runtimeData = GetRuntimeData<RuntimeData>(updateContext);
		debugText.Format("%0.1f (%0.1f)", runtimeData.timer.GetSecondsLeft(), m_duration);
	}
#endif

private:
	float m_duration;
	float m_variation;
};

//////////////////////////////////////////////////////////////////////////

// Wait for a specified behavior tree event and then succeed or fail
// depending how the node is configured.
class WaitForEvent : public Action
{
	typedef Action BaseClass;

public:
	struct RuntimeData
	{
		bool eventReceieved;

		RuntimeData()
			: eventReceieved(false)
		{
		}
	};

	WaitForEvent()
		: m_statusToReturn(Success)
	{
	}

	virtual LoadResult LoadFromXml(const XmlNodeRef& xml, const LoadContext& context) override
	{
		IF_UNLIKELY (BaseClass::LoadFromXml(xml, context) == LoadFailure)
			return LoadFailure;

		const stack_string eventName = xml->getAttr("name");
		IF_UNLIKELY (eventName.empty())
		{
			gEnv->pLog->LogError("WaitForEvent could not find the 'name' attribute at line %d.", xml->getLine());
			return LoadFailure;
		}

		m_eventToWaitFor = Event(eventName);

		const stack_string resultString = xml->getAttr("result");
		if (!resultString.empty())
		{
			if (resultString == "Success")
				m_statusToReturn = Success;
			else if (resultString == "Failure")
				m_statusToReturn = Failure;
			else
			{
				ErrorReporter(*this, context).LogError("Invalid 'result' attribute. Expected 'Success' or 'Failure'.");
				return LoadFailure;
			}
		}

		return LoadSuccess;
	}

#ifdef USING_BEHAVIOR_TREE_XML_DESCRIPTION_CREATION
	virtual XmlNodeRef CreateXmlDescription() override
	{
		XmlNodeRef xml = BaseClass::CreateXmlDescription();
		xml->setTag("WaitForEvent");
		xml->setAttr("name", m_eventToWaitFor.GetName());
		return xml;
	}
#endif

#ifdef USING_BEHAVIOR_TREE_SERIALIZATION
	virtual void Serialize(Serialization::IArchive& archive) override
	{
		archive(m_eventToWaitFor, "event", "^");
		if (strlen(m_eventToWaitFor.GetName()) == 0)
			archive.error(m_eventToWaitFor, "Event must be specified");
		BaseClass::Serialize(archive);
	}
#endif

	virtual void HandleEvent(const EventContext& context, const Event& event) override
	{
		if (m_eventToWaitFor == event)
		{
			RuntimeData& runtimeData = GetRuntimeData<RuntimeData>(context);
			runtimeData.eventReceieved = true;
		}
	}

#if defined(USING_BEHAVIOR_TREE_NODE_CUSTOM_DEBUG_TEXT) && defined(USING_BEHAVIOR_TREE_EVENT_DEBUGGING)
	virtual void GetCustomDebugText(const UpdateContext& updateContext, stack_string& debugText) const override
	{
		debugText = m_eventToWaitFor.GetName();
	}
#endif

protected:
	virtual Status Update(const UpdateContext& context) override
	{
		RuntimeData& runtimeData = GetRuntimeData<RuntimeData>(context);

		if (runtimeData.eventReceieved)
			return m_statusToReturn;
		else
			return Running;
	}

private:
	Event  m_eventToWaitFor;
	Status m_statusToReturn;
};

//////////////////////////////////////////////////////////////////////////

class IfTime : public Decorator
{
public:
	typedef Decorator BaseClass;

	struct RuntimeData
	{
		bool gateIsOpen;

		RuntimeData()
			: gateIsOpen(false)
		{
		}
	};

	enum CompareOp
	{
		MoreThan,
		LessThan,
	};

	IfTime()
		: m_timeThreshold(0.0f)
		, m_compareOp(MoreThan)
		, m_openGateIfTimestampWasNeverSet(false)
	{
	}

	virtual LoadResult LoadFromXml(const XmlNodeRef& xml, const LoadContext& context) override
	{
		const char* timestampName = xml->getAttr("since");
		if (!timestampName)
		{
			gEnv->pLog->LogError("%s: Missing attribute 'since' on line %d.", context.treeName, xml->getLine());
			return LoadFailure;
		}

#ifdef USING_BEHAVIOR_TREE_EDITOR
		m_timeStampName = timestampName;
#endif

		if (xml->getAttr("isMoreThan", m_timeThreshold))
		{
			m_compareOp = MoreThan;
		}
		else if (xml->getAttr("isLessThan", m_timeThreshold))
		{
			m_compareOp = LessThan;
		}
		else
		{
			gEnv->pLog->LogError("%s: Missing attribute 'isMoreThan' or 'isLessThan' on line %d.", context.treeName, xml->getLine());
			return LoadFailure;
		}

		xml->getAttr("orNeverBeenSet", m_openGateIfTimestampWasNeverSet);

		m_timestampID = TimestampID(timestampName);

		return LoadChildFromXml(xml, context);
	}

#ifdef USING_BEHAVIOR_TREE_XML_DESCRIPTION_CREATION
	virtual XmlNodeRef CreateXmlDescription() override
	{
		XmlNodeRef xml = BaseClass::CreateXmlDescription();

		xml->setTag("IfTime");

		xml->setAttr("since", m_timeStampName);

		if (m_compareOp == MoreThan)
			xml->setAttr("isMoreThan", m_timeThreshold);

		if (m_compareOp == LessThan)
			xml->setAttr("isLessThan", m_timeThreshold);

		if (m_openGateIfTimestampWasNeverSet)
			xml->setAttr("orNeverBeenSet", 1);

		return xml;
	}
#endif

#ifdef USING_BEHAVIOR_TREE_SERIALIZATION
	virtual void Serialize(Serialization::IArchive& archive) override
	{
		archive(m_timeStampName, "name", "^");
		if (m_timeStampName.empty())
			archive.error(m_timeStampName, "Timestamp must be specified");

		archive(m_compareOp, "compareOp", "^");
		archive(m_timeThreshold, "timeThreshold", "^");

		archive(m_openGateIfTimestampWasNeverSet, "openGateIfTimestampWasNeverSet", "^Open if it was never set");

		BaseClass::Serialize(archive);
	}
#endif

protected:
	virtual void OnInitialize(const UpdateContext& context) override
	{
		CRY_PROFILE_FUNCTION(PROFILE_AI);

		RuntimeData& runtimeData = GetRuntimeData<RuntimeData>(context);

		const bool timestampHasBeenSetAtLeastOnce = context.timestamps.HasBeenSetAtLeastOnce(m_timestampID);

		if (timestampHasBeenSetAtLeastOnce)
		{
			bool valid = false;
			CTimeValue elapsedTime;
			context.timestamps.GetElapsedTimeSince(m_timestampID, elapsedTime, valid);
			if (valid)
			{
				if ((m_compareOp == MoreThan) && (elapsedTime > m_timeThreshold))
				{
					runtimeData.gateIsOpen = true;
					return;
				}
				else if ((m_compareOp == LessThan) && (elapsedTime < m_timeThreshold))
				{
					runtimeData.gateIsOpen = true;
					return;
				}
			}
		}
		else
		{
			if (m_openGateIfTimestampWasNeverSet)
			{
				runtimeData.gateIsOpen = true;
				return;
			}
		}

		runtimeData.gateIsOpen = false;
	}

	virtual Status Update(const UpdateContext& context) override
	{
		RuntimeData& runtimeData = GetRuntimeData<RuntimeData>(context);

		if (runtimeData.gateIsOpen)
			return BaseClass::Update(context);
		else
			return Failure;
	}

private:
	TimestampID m_timestampID;
	float       m_timeThreshold;
	CompareOp   m_compareOp;
	bool        m_openGateIfTimestampWasNeverSet;

#ifdef USING_BEHAVIOR_TREE_EDITOR
	string m_timeStampName;
#endif
};

#ifdef USING_BEHAVIOR_TREE_SERIALIZATION
SERIALIZATION_ENUM_BEGIN_NESTED(IfTime, CompareOp, "Compare")
SERIALIZATION_ENUM(IfTime::MoreThan, "MoreThan", "More Than")
SERIALIZATION_ENUM(IfTime::LessThan, "LessThan", "Less Than")
SERIALIZATION_ENUM_END()
#endif

//////////////////////////////////////////////////////////////////////////

class WaitUntilTime : public Action
{
	typedef Action BaseClass;

public:
	struct RuntimeData
	{
	};

	WaitUntilTime()
		: m_timeThreshold(0.0f)
		, m_succeedIfTimestampWasNeverSet(false)
		, m_compareOp(MoreThan)
	{
	}

	enum CompareOp
	{
		MoreThan,
		LessThan,
	};

	virtual LoadResult LoadFromXml(const XmlNodeRef& xml, const LoadContext& context) override
	{
		const char* timestampName = xml->getAttr("since");
		if (!timestampName)
		{
			gEnv->pLog->LogError("%s: Failed to find 'since' attribute on line %d.", context.treeName, xml->getLine());
			return LoadFailure;
		}

#ifdef USING_BEHAVIOR_TREE_EDITOR
		m_timeStampName = timestampName;
#endif

		if (xml->getAttr("isMoreThan", m_timeThreshold))
		{
			m_compareOp = MoreThan;
		}
		else if (xml->getAttr("isLessThan", m_timeThreshold))
		{
			m_compareOp = LessThan;
		}
		else
		{
			gEnv->pLog->LogError("%s: Missing attribute 'isMoreThan' or 'isLessThan' on line %d.", context.treeName, xml->getLine());
			return LoadFailure;
		}

		xml->getAttr("succeedIfNeverBeenSet", m_succeedIfTimestampWasNeverSet);

		m_timestampID = TimestampID(timestampName);

		return LoadSuccess;
	}

#ifdef USING_BEHAVIOR_TREE_XML_DESCRIPTION_CREATION
	virtual XmlNodeRef CreateXmlDescription() override
	{
		XmlNodeRef xml = BaseClass::CreateXmlDescription();

		xml->setTag("WaitUntilTime");

		xml->setAttr("since", m_timeStampName);

		if (m_compareOp == MoreThan)
			xml->setAttr("isMoreThan", m_timeThreshold);

		if (m_compareOp == LessThan)
			xml->setAttr("isLessThan", m_timeThreshold);

		if (m_succeedIfTimestampWasNeverSet)
			xml->setAttr("succeedIfNeverBeenSet", 1);

		return xml;
	}
#endif

#ifdef USING_BEHAVIOR_TREE_SERIALIZATION
	virtual void Serialize(Serialization::IArchive& archive) override
	{
		archive(m_timeStampName, "name", "^");
		if (m_timeStampName.empty())
			archive.error(m_timeStampName, "Timestamp must be specified");

		archive(m_compareOp, "compareOp", "^");
		archive(m_timeThreshold, "timeThreshold", "^");

		archive(m_succeedIfTimestampWasNeverSet, "succeedIfTimestampWasNeverSet", "^Succeed if it was never set");

		BaseClass::Serialize(archive);
	}
#endif

protected:
	virtual Status Update(const UpdateContext& context) override
	{
		CRY_PROFILE_FUNCTION(PROFILE_AI);

		if (m_succeedIfTimestampWasNeverSet && !context.timestamps.HasBeenSetAtLeastOnce(m_timestampID))
		{
			return Success;
		}

		bool valid = false;
		CTimeValue elapsedTime;
		context.timestamps.GetElapsedTimeSince(m_timestampID, elapsedTime, valid);
		if (valid)
		{
			if ((m_compareOp == MoreThan) && (elapsedTime > m_timeThreshold))
				return Success;
			else if ((m_compareOp == LessThan) && (elapsedTime < m_timeThreshold))
				return Success;
		}

		return Running;
	}

private:
	float       m_timeThreshold;
	TimestampID m_timestampID;
	CompareOp   m_compareOp;
	bool        m_succeedIfTimestampWasNeverSet;

#ifdef USING_BEHAVIOR_TREE_EDITOR
	string m_timeStampName;
#endif
};

#ifdef USING_BEHAVIOR_TREE_SERIALIZATION
SERIALIZATION_ENUM_BEGIN_NESTED(WaitUntilTime, CompareOp, "Compare")
SERIALIZATION_ENUM(WaitUntilTime::MoreThan, "MoreThan", "More Than")
SERIALIZATION_ENUM(WaitUntilTime::LessThan, "LessThan", "Less Than")
SERIALIZATION_ENUM_END()
#endif

//////////////////////////////////////////////////////////////////////////

class AssertTime : public Action
{
	typedef Action BaseClass;

public:
	struct RuntimeData
	{
	};

	AssertTime()
		: m_timeThreshold(0.0f)
		, m_succeedIfTimestampWasNeverSet(false)
		, m_compareOp(MoreThan)
	{
	}

	virtual LoadResult LoadFromXml(const XmlNodeRef& xml, const LoadContext& context) override
	{
		const stack_string timestampName = xml->getAttr("since");
		IF_UNLIKELY (timestampName.empty())
		{
			ErrorReporter(*this, context).LogError("Missing or invalid 'since' attribute.");
			return LoadFailure;
		}

#ifdef USING_BEHAVIOR_TREE_EDITOR
		m_timeStampName = timestampName.c_str();
#endif

		if (xml->getAttr("isMoreThan", m_timeThreshold))
		{
			m_compareOp = MoreThan;
		}
		else if (xml->getAttr("isLessThan", m_timeThreshold))
		{
			m_compareOp = LessThan;
		}
		else
		{
			ErrorReporter(*this, context).LogError("Missing attribute 'isMoreThan' or 'isLessThan'.");
			return LoadFailure;
		}

		xml->getAttr("orNeverBeenSet", m_succeedIfTimestampWasNeverSet);

		m_timestampID = TimestampID(timestampName);

		return LoadSuccess;
	}

	enum CompareOp
	{
		MoreThan,
		LessThan,
	};

#ifdef USING_BEHAVIOR_TREE_XML_DESCRIPTION_CREATION
	virtual XmlNodeRef CreateXmlDescription() override
	{
		XmlNodeRef xml = BaseClass::CreateXmlDescription();

		xml->setTag("AssertTime");

		xml->setAttr("since", m_timeStampName);

		if (m_compareOp == MoreThan)
			xml->setAttr("isMoreThan", m_timeThreshold);

		if (m_compareOp == LessThan)
			xml->setAttr("isLessThan", m_timeThreshold);

		if (m_succeedIfTimestampWasNeverSet)
			xml->setAttr("orNeverBeenSet", 1);

		return xml;
	}
#endif

#ifdef USING_BEHAVIOR_TREE_SERIALIZATION
	virtual void Serialize(Serialization::IArchive& archive) override
	{
		archive(m_timeStampName, "name", "^");
		if (m_timeStampName.empty())
			archive.error(m_timeStampName, "Timestamp must be specified");

		archive(m_compareOp, "compareOp", "^");
		archive(m_timeThreshold, "timeThreshold", "^");

		archive(m_succeedIfTimestampWasNeverSet, "succeedIfTimestampWasNeverSet", "^Succeed if it was never set");

		BaseClass::Serialize(archive);
	}
#endif

protected:
	virtual Status Update(const UpdateContext& context) override
	{
		CRY_PROFILE_FUNCTION(PROFILE_AI);

		if (m_succeedIfTimestampWasNeverSet && !context.timestamps.HasBeenSetAtLeastOnce(m_timestampID))
		{
			return Success;
		}

		bool valid = false;
		CTimeValue elapsedTime(0.0f);
		context.timestamps.GetElapsedTimeSince(m_timestampID, elapsedTime, valid);

		if (valid)
		{
			if ((m_compareOp == MoreThan) && (elapsedTime > m_timeThreshold))
				return Success;
			else if ((m_compareOp == LessThan) && (elapsedTime < m_timeThreshold))
				return Success;
		}

		return Failure;
	}

private:

	float       m_timeThreshold;
	TimestampID m_timestampID;
	CompareOp   m_compareOp;
	bool        m_succeedIfTimestampWasNeverSet;

#ifdef USING_BEHAVIOR_TREE_EDITOR
	string m_timeStampName;
#endif

};

#ifdef USING_BEHAVIOR_TREE_SERIALIZATION
SERIALIZATION_ENUM_BEGIN_NESTED(AssertTime, CompareOp, "Compare")
SERIALIZATION_ENUM(AssertTime::MoreThan, "MoreThan", "More Than")
SERIALIZATION_ENUM(AssertTime::LessThan, "LessThan", "Less Than")
SERIALIZATION_ENUM_END()
#endif

//////////////////////////////////////////////////////////////////////////

class Fail : public Action
{
	typedef Action BaseClass;

public:
	struct RuntimeData
	{
	};

#ifdef USING_BEHAVIOR_TREE_XML_DESCRIPTION_CREATION
	virtual XmlNodeRef CreateXmlDescription() override
	{
		XmlNodeRef xml = BaseClass::CreateXmlDescription();
		xml->setTag("Fail");
		return xml;
	}
#endif

protected:
	virtual Status Update(const UpdateContext& context)
	{
		return Failure;
	}
};

//////////////////////////////////////////////////////////////////////////

class SuppressFailure : public Decorator
{
	typedef Decorator BaseClass;

public:
	struct RuntimeData
	{
	};

#ifdef USING_BEHAVIOR_TREE_XML_DESCRIPTION_CREATION
	virtual XmlNodeRef CreateXmlDescription() override
	{
		XmlNodeRef xml = BaseClass::CreateXmlDescription();
		xml->setTag("SuppressFailure");
		return xml;
	}
#endif

	virtual Status Update(const UpdateContext& context)
	{
		Status s = m_child->Tick(context);

		if (s == Running)
			return Running;
		else
			return Success;
	}
};

//////////////////////////////////////////////////////////////////////////

#if defined(USING_BEHAVIOR_TREE_LOG) || defined(USING_BEHAVIOR_TREE_EDITOR)
	#define STORE_LOG_MESSAGE
#endif

class Log : public Action
{
	typedef Action BaseClass;

public:
	struct RuntimeData
	{
	};

	virtual LoadResult LoadFromXml(const XmlNodeRef& node, const LoadContext& context) override
	{
#ifdef STORE_LOG_MESSAGE
		m_message = node->getAttr("message");
#endif

		return LoadSuccess;
	}

#ifdef USING_BEHAVIOR_TREE_XML_DESCRIPTION_CREATION
	virtual XmlNodeRef CreateXmlDescription() override
	{
		XmlNodeRef xml = BaseClass::CreateXmlDescription();
		xml->setTag("Log");
		xml->setAttr("message", m_message);
		return xml;
	}
#endif

#ifdef USING_BEHAVIOR_TREE_SERIALIZATION
	virtual void Serialize(Serialization::IArchive& archive) override
	{
		archive(m_message, "message", "^");
		if (m_message.empty())
			archive.error(m_message, "Log message is empty");

		BaseClass::Serialize(archive);
	}
#endif

protected:
	virtual Status Update(const UpdateContext& context) override
	{
#ifdef STORE_LOG_MESSAGE
		stack_string textBuffer;
		textBuffer.Format("%s (%d)", m_message ? m_message : "", GetXmlLine());
		context.behaviorLog.AddMessage(m_message);
#endif

		return Success;
	}

private:
#ifdef STORE_LOG_MESSAGE
	string m_message;
#endif
};

//////////////////////////////////////////////////////////////////////////

// This node effectively halts the execution of the modular behavior
// tree because it never finishes.
class Halt : public Action
{
	typedef Action BaseClass;

public:
	struct RuntimeData
	{
	};

#ifdef USING_BEHAVIOR_TREE_XML_DESCRIPTION_CREATION
	virtual XmlNodeRef CreateXmlDescription() override
	{
		XmlNodeRef xml = BaseClass::CreateXmlDescription();
		xml->setTag("Halt");
		return xml;
	}
#endif

protected:
	virtual Status Update(const UpdateContext& context) override
	{
		return Running;
	}
};

//////////////////////////////////////////////////////////////////////////

class Breakpoint : public Action
{
public:
	struct RuntimeData
	{
	};

protected:
	virtual Status Update(const UpdateContext& context)
	{
		CryDebugBreak();
		return Success;
	}
};

//////////////////////////////////////////////////////////////////////////

class Graft : public Action
{
public:
	typedef Action BaseClass;

	struct RuntimeData final : public IGraftNode
	{
		virtual bool RunBehavior(EntityId entityId, const char* behaviorName, XmlNodeRef behaviorXmlNode) override
		{
			if (behaviorTreeInstance.get() != NULL)
			{
				BehaviorVariablesContext variables(
				  behaviorTreeInstance->variables,
				  behaviorTreeInstance->behaviorTreeTemplate->variableDeclarations,
				  behaviorTreeInstance->variables.Changed());

				IEntity* entity = gEnv->pEntitySystem->GetEntity(entityId);
				assert(entity);

				UpdateContext context(
				  entityId,
				  *entity,
				  variables,
				  behaviorTreeInstance->timestampCollection,
				  behaviorTreeInstance->blackboard
#ifdef USING_BEHAVIOR_TREE_LOG
				  ,
				  behaviorTreeInstance->behaviorLog
#endif
				  );

				behaviorTreeInstance->behaviorTreeTemplate->rootNode->Terminate(context);
			}

			behaviorTreeInstance = gAIEnv.pBehaviorTreeManager->CreateBehaviorTreeInstanceFromXml(behaviorName, behaviorXmlNode);
			if (behaviorTreeInstance.get() != NULL)
			{
#ifdef USING_BEHAVIOR_TREE_NODE_CUSTOM_DEBUG_TEXT
				behaviorTreeName = behaviorName;
#endif
				return true;
			}
			else
			{
				return false;
			}
		}

		BehaviorTreeInstancePtr behaviorTreeInstance;

#ifdef USING_BEHAVIOR_TREE_NODE_CUSTOM_DEBUG_TEXT
		string behaviorTreeName;
#endif
	};

	virtual LoadResult LoadFromXml(const XmlNodeRef& xml, const LoadContext& context) override
	{
		if (BaseClass::LoadFromXml(xml, context) == LoadFailure)
			return LoadFailure;

		return LoadSuccess;
	}

	virtual void OnInitialize(const UpdateContext& context) override
	{
		RuntimeData& runtimeData = GetRuntimeData<RuntimeData>(context);
		gAIEnv.pGraftManager->GraftNodeReady(context.entityId, &runtimeData);
	}

	virtual Status Update(const UpdateContext& context) override
	{
		RuntimeData& runtimeData = GetRuntimeData<RuntimeData>(context);

		if (!runtimeData.behaviorTreeInstance.get())
			return Running;

		Status behaviorStatus = runtimeData.behaviorTreeInstance->behaviorTreeTemplate->rootNode->Tick(context);
		if (behaviorStatus == Failure)
		{
			ErrorReporter(*this, context).LogError("Graft behavior failed to execute.");
			return Failure;
		}

		if (behaviorStatus == Success)
		{
			gAIEnv.pGraftManager->GraftBehaviorComplete(context.entityId);
			runtimeData.behaviorTreeInstance.reset();
#ifdef USING_BEHAVIOR_TREE_NODE_CUSTOM_DEBUG_TEXT
			runtimeData.behaviorTreeName.clear();
#endif
		}

		return Running;
	}

	virtual void OnTerminate(const UpdateContext& context) override
	{
		RuntimeData& runtimeData = GetRuntimeData<RuntimeData>(context);
		if (runtimeData.behaviorTreeInstance.get())
			runtimeData.behaviorTreeInstance->behaviorTreeTemplate->rootNode->Terminate(context);

		gAIEnv.pGraftManager->GraftNodeTerminated(context.entityId);
	}

	virtual void HandleEvent(const EventContext& context, const Event& event) override
	{
		RuntimeData& runtimeData = GetRuntimeData<RuntimeData>(context);
		if (runtimeData.behaviorTreeInstance.get())
		{
			runtimeData.behaviorTreeInstance->behaviorTreeTemplate->signalHandler.ProcessSignal(event.GetCRC(), runtimeData.behaviorTreeInstance->variables);
			runtimeData.behaviorTreeInstance->timestampCollection.HandleEvent(event.GetCRC());
			runtimeData.behaviorTreeInstance->behaviorTreeTemplate->rootNode->SendEvent(context, event);
		}
	}

#ifdef USING_BEHAVIOR_TREE_NODE_CUSTOM_DEBUG_TEXT
	virtual void GetCustomDebugText(const UpdateContext& updateContext, stack_string& debugText) const
	{
		const RuntimeData& runtimeData = GetRuntimeData<RuntimeData>(updateContext);
		debugText.Format("%s", runtimeData.behaviorTreeName.c_str());
	}
#endif
};

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void RegisterBehaviorTreeNodes_Core()
{
	assert(gAIEnv.pBehaviorTreeManager);

	IBehaviorTreeManager& manager = *gAIEnv.pBehaviorTreeManager;

	const char* COLOR_FLOW = "00ff00";
	REGISTER_BEHAVIOR_TREE_NODE_WITH_SERIALIZATION(manager, Sequence, "Flow\\Sequence", COLOR_FLOW);
	REGISTER_BEHAVIOR_TREE_NODE_WITH_SERIALIZATION(manager, Selector, "Flow\\Selector", COLOR_FLOW);
	REGISTER_BEHAVIOR_TREE_NODE_WITH_SERIALIZATION(manager, Parallel, "Flow\\Parallel", COLOR_FLOW);
	REGISTER_BEHAVIOR_TREE_NODE_WITH_SERIALIZATION(manager, Loop, "Flow\\Loop", COLOR_FLOW);
	REGISTER_BEHAVIOR_TREE_NODE_WITH_SERIALIZATION(manager, LoopUntilSuccess, "Flow\\Loop until success", COLOR_FLOW);
	REGISTER_BEHAVIOR_TREE_NODE_WITH_SERIALIZATION(manager, Priority, "Flow\\Priority", COLOR_FLOW);
	REGISTER_BEHAVIOR_TREE_NODE_WITH_SERIALIZATION(manager, StateMachine, "Flow\\State Machine\\State machine", COLOR_FLOW);
	REGISTER_BEHAVIOR_TREE_NODE_WITH_SERIALIZATION(manager, SendTransitionEvent, "Flow\\State Machine\\Send transition event", COLOR_FLOW);

	const char* COLOR_CONDITION = "00ffff";
	REGISTER_BEHAVIOR_TREE_NODE_WITH_SERIALIZATION(manager, IfCondition, "Conditions\\Condition gate", COLOR_CONDITION);
	REGISTER_BEHAVIOR_TREE_NODE_WITH_SERIALIZATION(manager, AssertCondition, "Conditions\\Assert condition", COLOR_CONDITION);

	const char* COLOR_FAIL = "ff0000";
	const char* COLOR_DEBUG = "000000";
	const char* COLOR_TIME = "ffffff";
	REGISTER_BEHAVIOR_TREE_NODE_WITH_SERIALIZATION(manager, Timeout, "Time\\Timeout", COLOR_FAIL);
	REGISTER_BEHAVIOR_TREE_NODE_WITH_SERIALIZATION(manager, Wait, "Time\\Wait", COLOR_TIME);
	REGISTER_BEHAVIOR_TREE_NODE_WITH_SERIALIZATION(manager, MonitorCondition, "Time\\Wait for condition", COLOR_TIME);
	REGISTER_BEHAVIOR_TREE_NODE_WITH_SERIALIZATION(manager, WaitForEvent, "Time\\Wait for event", COLOR_TIME);
	REGISTER_BEHAVIOR_TREE_NODE_WITH_SERIALIZATION(manager, WaitUntilTime, "Time\\Wait for timestamp", COLOR_TIME);
	REGISTER_BEHAVIOR_TREE_NODE_WITH_SERIALIZATION(manager, IfTime, "Time\\Timestamp gate", COLOR_TIME);
	REGISTER_BEHAVIOR_TREE_NODE_WITH_SERIALIZATION(manager, AssertTime, "Time\\Assert timestamp", COLOR_DEBUG);

	const char* COLOR_CORE = "0000ff";
	REGISTER_BEHAVIOR_TREE_NODE_WITH_SERIALIZATION(manager, SendEvent, "Core\\Send Event", COLOR_CORE);
	REGISTER_BEHAVIOR_TREE_NODE_WITH_SERIALIZATION(manager, RandomGate, "Core\\Random gate", COLOR_CORE);
	REGISTER_BEHAVIOR_TREE_NODE_WITH_SERIALIZATION(manager, Fail, "Core\\Fail", COLOR_FAIL);
	REGISTER_BEHAVIOR_TREE_NODE_WITH_SERIALIZATION(manager, SuppressFailure, "Core\\Suppress failure", COLOR_FAIL);
	REGISTER_BEHAVIOR_TREE_NODE_WITH_SERIALIZATION(manager, Log, "Core\\Log message", COLOR_DEBUG);
	REGISTER_BEHAVIOR_TREE_NODE_WITH_SERIALIZATION(manager, Halt, "Core\\Halt", COLOR_DEBUG);

	REGISTER_BEHAVIOR_TREE_NODE(manager, Breakpoint);

	REGISTER_BEHAVIOR_TREE_NODE(manager, Graft);
}
}
