// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

//! \cond INTERNAL

#pragma once

	#include "IBehaviorTree.h"

	#include "SerializationSupport.h"

namespace BehaviorTree
{
class Node : public INode
{
public:
	//! When you "tick" a node the following rules apply:
	//! 1. If the node was not previously running the node will first be
	//!  initialized and then immediately updated.
	//! 2. If the node was previously running, but no longer is, it will be
	//!  terminated.
	virtual Status Tick(const UpdateContext& unmodifiedContext) override
	{
		CRY_PROFILE_FUNCTION(PROFILE_AI);

	#ifdef DEBUG_MODULAR_BEHAVIOR_TREE
		DebugTree* debugTree = unmodifiedContext.debugTree;
		if (debugTree)
			debugTree->Push(this);
	#endif // DEBUG_MODULAR_BEHAVIOR_TREE

		UpdateContext context = unmodifiedContext;

		const RuntimeDataID runtimeDataID = MakeRuntimeDataID(context.entityId, m_id);
		context.runtimeData = GetCreator()->GetRuntimeData(runtimeDataID);
		const bool nodeNeedsToBeInitialized = (context.runtimeData == NULL);
		if (nodeNeedsToBeInitialized)
		{
			context.runtimeData = GetCreator()->AllocateRuntimeData(runtimeDataID);
		}

		if (nodeNeedsToBeInitialized)
		{
	#ifdef USING_BEHAVIOR_TREE_LOG
			if (!m_startLog.empty())
				context.behaviorLog.AddMessage(m_startLog.c_str());
	#endif // USING_BEHAVIOR_TREE_LOG
			OnInitialize(context);
		}

		const Status status = Update(context);

		if (status != Running)
		{
			OnTerminate(context);
			GetCreator()->FreeRuntimeData(runtimeDataID);
			context.runtimeData = NULL;

	#ifdef USING_BEHAVIOR_TREE_LOG
			if (status == Success && !m_successLog.empty())
				context.behaviorLog.AddMessage(m_successLog.c_str());
			else if (status == Failure && !m_failureLog.empty())
				context.behaviorLog.AddMessage(m_failureLog.c_str());
	#endif // USING_BEHAVIOR_TREE_LOG
		}

	#ifdef DEBUG_MODULAR_BEHAVIOR_TREE
		if (debugTree)
			debugTree->Pop(status);
	#endif // DEBUG_MODULAR_BEHAVIOR_TREE

		return status;
	}

	//! Call this to explicitly terminate a node.
	//! The node will itself take care of cleaning things up.
	//! It's safe to call Terminate on an already terminated node, although it is of course redundant.
	virtual void Terminate(const UpdateContext& unmodifiedContext) override
	{
		UpdateContext context = unmodifiedContext;
		const RuntimeDataID runtimeDataID = MakeRuntimeDataID(context.entityId, m_id);
		context.runtimeData = GetCreator()->GetRuntimeData(runtimeDataID);
		if (context.runtimeData != NULL)
		{
			OnTerminate(context);
			GetCreator()->FreeRuntimeData(runtimeDataID);
			context.runtimeData = NULL;
		}
	}

	//! Send an event to the node.
	//! The event will be dispatched to the correct HandleEvent method with validated runtime data.
	//! Never override this!
	virtual void SendEvent(const EventContext& unmodifiedContext, const Event& event) override
	{
		void* runtimeData = GetCreator()->GetRuntimeData(MakeRuntimeDataID(unmodifiedContext.entityId, m_id));
		if (runtimeData)
		{
			EventContext context = unmodifiedContext;
			context.runtimeData = runtimeData;
			HandleEvent(context, event);
		}
	}

	//! Load up a behavior tree node with information from an xml node.
	virtual LoadResult LoadFromXml(const XmlNodeRef& xml, const struct LoadContext& context) override
	{

	#ifdef USING_BEHAVIOR_TREE_LOG
		m_startLog = xml->getAttr("_startLog");
		m_successLog = xml->getAttr("_successLog");
		m_failureLog = xml->getAttr("_failureLog");
	#endif // USING_BEHAVIOR_TREE_LOG

	#ifdef USING_BEHAVIOR_TREE_COMMENTS
		m_comment = xml->getAttr("_comment");
	#endif // USING_BEHAVIOR_TREE_COMMENTS

		return LoadSuccess;
	}

	#ifdef USING_BEHAVIOR_TREE_XML_DESCRIPTION_CREATION
	virtual XmlNodeRef CreateXmlDescription() override
	{
		XmlNodeRef node = GetISystem()->CreateXmlNode("Node");

		auto setAttrOptional = [this, &node](const char* szKey, const string& value)
		{
			if (!value.empty())
			{
				node->setAttr(szKey, value);
			}
		};
		#ifdef USING_BEHAVIOR_TREE_LOG
		setAttrOptional("_startLog", m_startLog);
		setAttrOptional("_successLog", m_successLog);
		setAttrOptional("_failureLog", m_failureLog);
		#endif // USING_BEHAVIOR_TREE_LOG
		#ifdef USING_BEHAVIOR_TREE_COMMENTS
		setAttrOptional("_comment", m_comment);
		#endif // USING_BEHAVIOR_TREE_COMMENTS

		return node;
	}
	#endif

	#ifdef USING_BEHAVIOR_TREE_SERIALIZATION
	virtual void Serialize(Serialization::IArchive& archive) override
	{
		#ifdef DEBUG_MODULAR_BEHAVIOR_TREE
		HandleXmlLineNumberSerialization(archive, m_xmlLine);
		#endif

		#ifdef USING_BEHAVIOR_TREE_COMMENTS
		HandleCommentSerialization(archive, m_comment);
		#endif // USING_BEHAVIOR_TREE_COMMENTS
	}
	#endif

	void                SetCreator(INodeCreator* creator) { m_creator = creator; }
	INodeCreator*       GetCreator()                      { return m_creator; }
	const INodeCreator* GetCreator() const                { return m_creator; }

	#ifdef DEBUG_MODULAR_BEHAVIOR_TREE
	void   SetXmlLine(const uint32 line) { m_xmlLine = line; }
	uint32 GetXmlLine() const            { return m_xmlLine; }
	#endif // DEBUG_MODULAR_BEHAVIOR_TREE

	#ifdef USING_BEHAVIOR_TREE_NODE_CUSTOM_DEBUG_TEXT
	virtual void GetCustomDebugText(const UpdateContext& updateContext, stack_string& debugText) const {}
	#endif // USING_BEHAVIOR_TREE_NODE_CUSTOM_DEBUG_TEXT

	template<typename RuntimeDataType>
	RuntimeDataType& GetRuntimeData(const EventContext& context)
	{
		assert(context.runtimeData);
		return *reinterpret_cast<RuntimeDataType*>(context.runtimeData);
	}

	template<typename RuntimeDataType>
	RuntimeDataType& GetRuntimeData(const UpdateContext& context)
	{
		assert(context.runtimeData);
		return *reinterpret_cast<RuntimeDataType*>(context.runtimeData);
	}

	template<typename RuntimeDataType>
	const RuntimeDataType& GetRuntimeData(const UpdateContext& context) const
	{
		assert(context.runtimeData);
		return *reinterpret_cast<const RuntimeDataType*>(context.runtimeData);
	}

	NodeID GetNodeID() const { return m_id; }

	NodeID m_id;     //!< TODO: Make this accessible only to the creator.

protected:

	Node()
		: m_id(0)
		, m_creator(NULL)

	#ifdef DEBUG_MODULAR_BEHAVIOR_TREE
		, m_xmlLine(0)
	#endif //DEBUG_MODULAR_BEHAVIOR_TREE
	{
	}
#ifndef SWIG
	//! Called before the first call to Update.
	virtual void OnInitialize(const UpdateContext& context) {}

	//! Called when a node is being terminated. Clean up after yourself here!
	//! Not to be confused with Terminate which you call when you want to terminate a node. :) Confused? Read it again.
	//! OnTerminate is called in one of the following cases:
	//! a) The node itself returned Success/Failure in Update.
	//! b) Another node told this node to Terminate while this node was running.
	virtual void OnTerminate(const UpdateContext& context) {}

	//! Do you node's work here.
	//! - Note that OnInitialize will have been automatically called for you
	//! before you get your first update.
	//! - If you return Success or Failure the node will automatically
	//! get OnTerminate called on itself.
	virtual Status Update(const UpdateContext& context) { return Running; }
#endif

private:

	//! This is where you would put your event handling code for your node.
	//! It's always called with valid data. This method should never be called directly.
	virtual void HandleEvent(const EventContext& context, const Event& event) = 0;

	INodeCreator* m_creator;

	#ifdef USING_BEHAVIOR_TREE_LOG
	string m_startLog;
	string m_successLog;
	string m_failureLog;
	#endif // USING_BEHAVIOR_TREE_LOG

	#ifdef DEBUG_MODULAR_BEHAVIOR_TREE
	uint32 m_xmlLine;
	#endif // DEBUG_MODULAR_BEHAVIOR_TREE

	#ifdef USING_BEHAVIOR_TREE_COMMENTS
	string m_comment;
	#endif // USING_BEHAVIOR_TREE_COMMENTS

};

DECLARE_SHARED_POINTERS(Node);

//! An object of this class will help out when reporting warnings and errors from within the modular behavior tree code.
//! If possible, it automatically adds the node type, xml line and the name of the agent
//! using the node before routing the message along to gEnv->pLog.
class ErrorReporter
{
public:
	ErrorReporter(const Node& node, const LoadContext& context)
	{

	#ifdef DEBUG_MODULAR_BEHAVIOR_TREE
		const uint32 xmlLine = node.GetXmlLine();
		const char* nodeTypeName = node.GetCreator()->GetTypeName();
		m_prefixString.Format("%s(%d) [Tree='%s']", nodeTypeName, xmlLine, context.treeName);
	#else // DEBUG_MODULAR_BEHAVIOR_TREE
		m_prefixString.Format("[Tree='%s']", context.treeName);
	#endif // DEBUG_MODULAR_BEHAVIOR_TREE

	}

	ErrorReporter(const Node& node, const UpdateContext& context)
	{
		const IEntity* entity = gEnv->pEntitySystem->GetEntity(context.entityId);
		const char* agentName = entity ? entity->GetName() : "(null)";

	#ifdef DEBUG_MODULAR_BEHAVIOR_TREE
		const uint32 xmlLine = node.GetXmlLine();
		const char* nodeTypeName = node.GetCreator()->GetTypeName();
		m_prefixString.Format("%s(%d) [%s]", nodeTypeName, xmlLine, agentName);
	#else // DEBUG_MODULAR_BEHAVIOR_TREE
		m_prefixString.Format("[%s]", agentName);
	#endif // DEBUG_MODULAR_BEHAVIOR_TREE

	}

	//! Formats a warning message with node type, xml line and the name of the agent using the node.
	//! This information is not accessible in all configurations and is thus compiled in only when possible.
	//! The message is routed through gEnv->pLog->LogWarning().
	void LogWarning(const char* format, ...) const
	{
		va_list argumentList;
		char formattedLog[512];

		va_start(argumentList, format);
		cry_vsprintf(formattedLog, format, argumentList);
		va_end(argumentList);

		stack_string finalMessage;
		finalMessage.Format("%s %s", m_prefixString.c_str(), formattedLog);
		gEnv->pLog->LogWarning("%s", finalMessage.c_str());
	}

	//! Formats a error message with node type, xml line and the name of the agent using the node.
	//! This information is not accessible in all configurations and is thus compiled in only when possible.
	//! The message is routed through gEnv->pLog->LogError().
	void LogError(const char* format, ...) const
	{
		va_list argumentList;
		char formattedLog[512];

		va_start(argumentList, format);
		cry_vsprintf(formattedLog, format, argumentList);
		va_end(argumentList);

		stack_string finalMessage;
		finalMessage.Format("%s %s", m_prefixString.c_str(), formattedLog);
		gEnv->pLog->LogError("%s", finalMessage.c_str());
	}

private:
	CryFixedStringT<128> m_prefixString;
};
}

//! \endcond