// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

//! \cond INTERNAL

#pragma once

#include "BehaviorTreeDefines.h"
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
	#ifdef DEBUG_MODULAR_BEHAVIOR_TREE
			if (!m_startLog.empty())
				context.behaviorLog.AddMessage(m_startLog.c_str());
	#endif // DEBUG_MODULAR_BEHAVIOR_TREE
			OnInitialize(context);
		}

		const Status status = Update(context);

	#ifdef DEBUG_MODULAR_BEHAVIOR_TREE
		if (debugTree)
		{
			DebugNodePtr topNode = debugTree->GetTopNode();
			assert(topNode);
			assert(topNode->node == this);
			GetCustomDebugText(context, topNode->customDebugText);	// FIXME: stack_string may suddenly switch to heap memory in a different DLL => bang! (GetCustomDebugText() would need refactoring to output to a string interface)
		}
	#endif // DEBUG_MODULAR_BEHAVIOR_TREE

		if (status != Running)
		{
			OnTerminate(context);
			GetCreator()->FreeRuntimeData(runtimeDataID);
			context.runtimeData = NULL;

	#ifdef DEBUG_MODULAR_BEHAVIOR_TREE
			if (status == Success && !m_successLog.empty())
				context.behaviorLog.AddMessage(m_successLog.c_str());
			else if (status == Failure && !m_failureLog.empty())
				context.behaviorLog.AddMessage(m_failureLog.c_str());
	#endif // DEBUG_MODULAR_BEHAVIOR_TREE
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
	virtual LoadResult LoadFromXml(const XmlNodeRef& xml, const struct LoadContext& context, const bool isLoadingFromEditor) override
	{
		#ifdef DEBUG_MODULAR_BEHAVIOR_TREE
		m_startLog = xml->getAttr("_startLog");
		m_successLog = xml->getAttr("_successLog");
		m_failureLog = xml->getAttr("_failureLog");
		m_comment = xml->getAttr("_comment");
		#endif // DEBUG_MODULAR_BEHAVIOR_TREE

		return LoadSuccess;
	}

	#ifdef USING_BEHAVIOR_TREE_XML_DESCRIPTION_CREATION
	//! Save behavior tree node information in an xml node. Opposite of LoadFromXML.
	//! Saved information that is saved here should be read back when calling LoadFromXML and viceversa
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
		#ifdef DEBUG_MODULAR_BEHAVIOR_TREE
		setAttrOptional("_startLog", m_startLog);
		setAttrOptional("_successLog", m_successLog);
		setAttrOptional("_failureLog", m_failureLog);
		setAttrOptional("_comment", m_comment);
		#endif // DEBUG_MODULAR_BEHAVIOR_TREE
		return node;
	}
	#endif

	#ifdef USING_BEHAVIOR_TREE_SERIALIZATION
	//! Serialize node data to be shown in the Behavior Tree Editor
	//! All properties that are saved/loaded in the xml should be accessible (somehow) from the Editor
	virtual void Serialize(Serialization::IArchive& archive) override
	{
		HandleXmlLineNumberSerialization(archive, m_xmlLine);

		HandleCommentSerialization(archive, m_comment);
	}
	#endif

	void                SetCreator(INodeCreator* creator) { m_creator = creator; }
	INodeCreator*       GetCreator()                      { return m_creator; }
	const INodeCreator* GetCreator() const                { return m_creator; }

	#ifdef DEBUG_MODULAR_BEHAVIOR_TREE
	void   SetXmlLine(const uint32 line) { m_xmlLine = line; }
	uint32 GetXmlLine() const            { return m_xmlLine; }
	virtual void GetCustomDebugText(const UpdateContext& updateContext, stack_string& debugText) const {}
	#endif // DEBUG_MODULAR_BEHAVIOR_TREE

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

	//! Called when a node is being terminated (Terminate() was called)
	//! It can happen when in the following cases:
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

	#ifdef DEBUG_MODULAR_BEHAVIOR_TREE
	string m_startLog;
	string m_successLog;
	string m_failureLog;
	#endif // DEBUG_MODULAR_BEHAVIOR_TREE

	uint32 m_xmlLine;
	string m_comment;
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


	static string ErrorMessageMissingOrEmptyAttribute(const string& tag, const string& fieldMissing)
	{
		string missingOrEmptyAttributeMessage;
		missingOrEmptyAttributeMessage.Format("%s tag is missing '%s' attribute or empty.", tag.c_str(), fieldMissing.c_str());
		return missingOrEmptyAttributeMessage;
	}

	static string ErrorMessageInvalidAttribute(const string& tag, const string& invalidField, const string& providedInvalidValue, const string& reason)
	{
		string invalidAttributeMessage;
		invalidAttributeMessage.Format("%s tag has invalid attribute '%s' with value '%s'. Reason: %s.", tag.c_str(), invalidField.c_str(), providedInvalidValue.c_str(), reason.c_str());
		return invalidAttributeMessage;
	}

	static string ErrorMessageTooManyChildren(const string& tag, const int currentChildCount, const int maxSupportedChildren)
	{
		string tooManyChildrenMessage;
		tooManyChildrenMessage.Format("%s has %d children. Max %d children are supported.", tag.c_str(), currentChildCount, maxSupportedChildren);
		return tooManyChildrenMessage;
	}


private:
	CryFixedStringT<128> m_prefixString;
};
}

//! \endcond