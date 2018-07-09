// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "TreeVisualizer.h"

#include <CryAISystem/IAIDebugRenderer.h>
#include <CryAISystem/BehaviorTree/Node.h>
#include <CryAISystem/BehaviorTree/TimestampCollection.h>
#include <CryCore/Containers/VariableCollection.h>

#include <CrySerialization/IArchive.h>
#include <CrySerialization/STL.h>

#ifdef USING_BEHAVIOR_TREE_VISUALIZER
namespace BehaviorTree
{
const float nodePosX = 10.0f;
const float nodePosY = 30.0f;
const float behaviorLogPosX = 600.0f;
const float behaviorLogPosY = 30.0f;
const float timestampPosX = behaviorLogPosX;
const float timestampPosY = 370.0f;
const float blackboardPosX = 900.0f;
const float blackboardPosY = 30.0f;

const float eventLogPosX = 1250.0f;
const float eventLogPosY = 370.0f;

const float fontSize = 1.25f;
const float lineHeight = 11.5f * fontSize;

TreeVisualizer::TreeVisualizer(const UpdateContext& updateContext)
	: m_updateContext(updateContext)
	, m_currentLinePositionX(0.0f)
	, m_currentLinePositionY(0.0f)
{
}

void TreeVisualizer::Draw(
  const DebugTree& tree,
  const char* behaviorTreeName,
  const char* agentName,
	#ifdef USING_BEHAVIOR_TREE_LOG
  const MessageQueue& behaviorLog,
	#endif // USING_BEHAVIOR_TREE_LOG
  const TimestampCollection& timestampCollection,
  const Blackboard& blackboard
	#ifdef USING_BEHAVIOR_TREE_EVENT_DEBUGGING
  , const MessageQueue& eventsLog
	#endif // USING_BEHAVIOR_TREE_EVENT_DEBUGGING
  )
{
	// Nodes
	{
		const DebugNode* firstNode = tree.GetFirstNode().get();
		if (firstNode)
		{
			SetLinePosition(nodePosX, nodePosY);
			stack_string caption;
			caption.Format("  Modular Behavior Tree '%s' for agent '%s'", behaviorTreeName, agentName);
			DrawLine(caption, Col_Yellow);
			DrawLine("", Col_White);
			DrawNode(*firstNode, 0);
		}
	}

	#ifdef USING_BEHAVIOR_TREE_LOG
	DrawBehaviorLog(behaviorLog);
	#endif // USING_BEHAVIOR_TREE_LOG

	#ifdef USING_BEHAVIOR_TREE_TIMESTAMP_DEBUGGING
	if (gAIEnv.CVars.DebugTimestamps)
	{
		DrawTimestampCollection(timestampCollection);
	}
	#endif // USING_BEHAVIOR_TREE_TIMESTAMP_DEBUGGING

	DrawBlackboard(blackboard);

	#ifdef USING_BEHAVIOR_TREE_EVENT_DEBUGGING
	DrawEventLog(eventsLog);
	#endif // USING_BEHAVIOR_TREE_EVENT_DEBUGGING
}

void TreeVisualizer::DrawNode(const DebugNode& node, const uint32 depth)
{
	// Construct a nice readable debug text for this node
	stack_string str;

	// Line
	const uint32 xmlLine = static_cast<const Node*>(node.node)->GetXmlLine();
	if (xmlLine > 0)
		str.Format("%5d ", xmlLine);
	else
		str.Format("      ", xmlLine);

	// Indention
	for (uint32 i = 0; i < (depth + 1); ++i)
	{
		if ((i % 2) == 1)
			str += "- ";
		else
			str += "  ";
	}

	// Node type
	const char* nodeType = static_cast<const Node*>(node.node)->GetCreator()->GetTypeName();
	str += nodeType;

	bool hasCustomText = false;

	#ifdef USING_BEHAVIOR_TREE_NODE_CUSTOM_DEBUG_TEXT
	// Custom debug text from the node
	stack_string customDebugText;

	UpdateContext updateContext = m_updateContext;
	const Node* nodeToDraw = static_cast<const Node*>(node.node);
	const RuntimeDataID runtimeDataID = MakeRuntimeDataID(updateContext.entityId, nodeToDraw->m_id);
	updateContext.runtimeData = nodeToDraw->GetCreator()->GetRuntimeData(runtimeDataID);

	nodeToDraw->GetCustomDebugText(updateContext, customDebugText);
	if (!customDebugText.empty())
	{
		str += " - " + customDebugText;
		hasCustomText = true;
	}
	#endif // USING_BEHAVIOR_TREE_NODE_CUSTOM_DEBUG_TEXT

	// Draw
	ColorB color;
	const bool leaf = node.children.empty();
	if (leaf)
		color = Col_SlateBlue;
	else if (hasCustomText)
		color = Col_White;
	else
		color = Col_DarkSlateGrey;

	DrawLine(str.c_str(), color);

	// Recursively draw children
	DebugNode::Children::const_iterator it = node.children.begin();
	DebugNode::Children::const_iterator end = node.children.end();
	for (; it != end; ++it)
	{
		DrawNode(*(*it), depth + 1);
	}
}

void TreeVisualizer::DrawLine(const char* label, const ColorB& color)
{
	gEnv->pAISystem->GetAIDebugRenderer()->Draw2dLabel(m_currentLinePositionX, m_currentLinePositionY, fontSize, color, false, label);
	m_currentLinePositionY += lineHeight;
}

	#ifdef USING_BEHAVIOR_TREE_LOG
void TreeVisualizer::DrawBehaviorLog(const MessageQueue& behaviorLog)
{
	SetLinePosition(behaviorLogPosX, behaviorLogPosY);
	DrawLine("Behavior Log", Col_Yellow);
	DrawLine("", Col_White);

	PersonalLog::Messages::const_iterator it = behaviorLog.GetMessages().begin();
	PersonalLog::Messages::const_iterator end = behaviorLog.GetMessages().end();
	for (; it != end; ++it)
		DrawLine(*it, Col_White);
}
	#endif // USING_BEHAVIOR_TREE_LOG

	#ifdef USING_BEHAVIOR_TREE_TIMESTAMP_DEBUGGING
void TreeVisualizer::DrawTimestampCollection(const TimestampCollection& timestampCollection)
{
	SetLinePosition(timestampPosX, timestampPosY);
	DrawLine("Timestamp Collection", Col_Yellow);
	DrawLine("", Col_White);

	CTimeValue timeNow = gEnv->pTimer->GetFrameStartTime();
	Timestamps::const_iterator it = timestampCollection.GetTimestamps().begin();
	Timestamps::const_iterator end = timestampCollection.GetTimestamps().end();
	for (; it != end; ++it)
	{
		stack_string s;
		ColorB color;
		if (it->IsValid())
		{
			s.Format("%s [%.2f]", it->id.timestampName, (timeNow - it->time).GetSeconds());
			color = Col_ForestGreen;
		}
		else
		{
			s.Format("%s [--]", it->id.timestampName);
			color = Col_Gray;
		}
		DrawLine(s.c_str(), color);
	}
}
	#endif // USING_BEHAVIOR_TREE_TIMESTAMP_DEBUGGING

void TreeVisualizer::SetLinePosition(float x, float y)
{
	m_currentLinePositionX = x;
	m_currentLinePositionY = y;
}

void TreeVisualizer::DrawBlackboard(const Blackboard& blackboard)
{
	#ifdef STORE_BLACKBOARD_VARIABLE_NAMES
	SetLinePosition(blackboardPosX, blackboardPosY);

	DrawLine("Blackboard variables", Col_Yellow);
	DrawLine("", Col_White);

	Blackboard::BlackboardVariableArray blackboardVariableArray = blackboard.GetBlackboardVariableArray();

	Blackboard::BlackboardVariableArray::const_iterator it = blackboardVariableArray.begin();
	Blackboard::BlackboardVariableArray::const_iterator end = blackboardVariableArray.end();
	for (; it != end; ++it)
	{
		BlackboardVariableId id = it->first;
		IBlackboardVariablePtr variable = it->second;

		Serialization::TypeID typeId = variable->GetDataTypeId();
		if (typeId == Serialization::TypeID::get<Vec3>())
		{
			stack_string variableText;

			Vec3 data;
			variable->GetData(data);
			variableText.Format("%s - (%f, %f, %f)", id.name.c_str(), data.x, data.y, data.z);

			DrawLine(variableText.c_str(), Col_White);
		}
	}
	#endif
}

	#ifdef USING_BEHAVIOR_TREE_EVENT_DEBUGGING
void TreeVisualizer::DrawEventLog(const MessageQueue& eventsLog)
{
	SetLinePosition(eventLogPosX, eventLogPosY);
	DrawLine("Event Log", Col_Yellow);
	DrawLine("", Col_White);

	PersonalLog::Messages::const_reverse_iterator it = eventsLog.GetMessages().rbegin();
	PersonalLog::Messages::const_reverse_iterator end = eventsLog.GetMessages().rend();
	for (; it != end; ++it)
		DrawLine(*it, Col_White);
}
	#endif // USING_BEHAVIOR_TREE_EVENT_DEBUGGING

//////////////////////////////////////////////////////////////////////////

void DebugTreeSerializer::TreeNode::Serialize(Serialization::IArchive& archive)
{
	archive(xmlLine, "xmlLine");
	archive(node, "node");
	archive(children, "children");
}

void DebugTreeSerializer::MBTree::Serialize(Serialization::IArchive& archive)
{
	archive(name, "name");
	archive(rootNode, "rootNode");
}

void DebugTreeSerializer::Variable::Serialize(Serialization::IArchive& archive)
{
	archive(name, "name");
	archive(value, "value");
}

void DebugTreeSerializer::TimeStamp::Serialize(Serialization::IArchive& archive)
{
	archive(name, "name");
	archive(value, "value");
	archive(bIsValid, "isValid");
}

void DebugTreeSerializer::Data::Serialize(Serialization::IArchive& archive)
{
	archive(tree, "tree");
	archive(variables, "variables");
	archive(timeStamps, "timeStamps");
	archive(eventLog, "eventLog");
	archive(errorLog, "errorLog");
}
//////////////////////////////////////////////////////////////////////////

DebugTreeSerializer::DebugTreeSerializer(const BehaviorTreeInstance& treeInstance, const DebugTree& debugTree, const UpdateContext& updateContext, const bool bExecutionError)
{
	m_data.tree.name = treeInstance.behaviorTreeTemplate->mbtFilename.c_str();

	if (bExecutionError)
	{
		CollectExecutionErrorInfo(debugTree);
	}
	else
	{
		const DebugNode* pFirstNode = debugTree.GetFirstNode().get();
		if (pFirstNode)
		{
			CollectTreeNodeInfo(*pFirstNode, updateContext, m_data.tree.rootNode);
		}
		CollectVariablesInfo(treeInstance);
		CollectTimeStamps(treeInstance);
		CollectEventsLog(treeInstance);
	}
}

void DebugTreeSerializer::Serialize(Serialization::IArchive& archive)
{
	archive(m_data, "aiMbt");
}

void DebugTreeSerializer::CollectTreeNodeInfo(const DebugNode& debugNode, const UpdateContext& updateContext, TreeNode& outNode)
{
	const uint32 xmlLine = static_cast<const Node*>(debugNode.node)->GetXmlLine();

	stack_string tempOutput;
	const char* szNodeType = static_cast<const Node*>(debugNode.node)->GetCreator()->GetTypeName();
	tempOutput += szNodeType;

	#ifdef USING_BEHAVIOR_TREE_NODE_CUSTOM_DEBUG_TEXT
	// Custom debug text from the node
	stack_string customDebugText;

	UpdateContext updateContextCopy = updateContext;
	const Node* pNodeToDraw = static_cast<const Node*>(debugNode.node);
	const RuntimeDataID runtimeDataID = MakeRuntimeDataID(updateContext.entityId, pNodeToDraw->m_id);
	updateContextCopy.runtimeData = pNodeToDraw->GetCreator()->GetRuntimeData(runtimeDataID);

	pNodeToDraw->GetCustomDebugText(updateContextCopy, customDebugText);
	if (!customDebugText.empty())
	{
		tempOutput += " - " + customDebugText;
	}
	#endif // USING_BEHAVIOR_TREE_NODE_CUSTOM_DEBUG_TEXT

	outNode.xmlLine = xmlLine;
	outNode.node = tempOutput.c_str();
	outNode.children.reserve(debugNode.children().size());

	DebugNode::Children::const_iterator it = debugNode.children.begin();
	DebugNode::Children::const_iterator endIt = debugNode.children.end();
	for (; it != endIt; ++it)
	{
		outNode.children.push_back(TreeNode());
		CollectTreeNodeInfo(*(*it), updateContext, outNode.children.back());
	}
}

void DebugTreeSerializer::CollectVariablesInfo(const BehaviorTreeInstance& instance)
{
	#ifdef DEBUG_VARIABLE_COLLECTION
	DynArray<::Variables::DebugHelper::DebugVariable> outVariables;
	::Variables::DebugHelper::CollectDebugVariables(instance.variables, instance.behaviorTreeTemplate->variableDeclarations, outVariables);

	m_data.variables.reserve(outVariables.size());
	for (DynArray<::Variables::DebugHelper::DebugVariable>::iterator it = outVariables.begin(), end = outVariables.end(); it != end; ++it)
	{
		Variable variable;
		variable.name = it->name;
		variable.value = it->value;
		m_data.variables.push_back(variable);
	}
	#endif
}

void DebugTreeSerializer::CollectTimeStamps(const BehaviorTreeInstance& instance)
{
	#ifdef USING_BEHAVIOR_TREE_TIMESTAMP_DEBUGGING
	CTimeValue timeNow = gEnv->pTimer->GetFrameStartTime();
	Timestamps::const_iterator it = instance.timestampCollection.GetTimestamps().begin();
	Timestamps::const_iterator end = instance.timestampCollection.GetTimestamps().end();

	m_data.timeStamps.reserve(instance.timestampCollection.GetTimestamps().size());
	for (; it != end; ++it)
	{
		TimeStamp timeStamp;
		timeStamp.name = it->id.timestampName;
		timeStamp.bIsValid = it->IsValid();
		timeStamp.value = it->IsValid() ? (timeNow - it->time).GetSeconds() : 0.0f;

		m_data.timeStamps.push_back(timeStamp);
	}
	#endif
}

void DebugTreeSerializer::CollectEventsLog(const BehaviorTreeInstance& instance)
{
	#ifdef USING_BEHAVIOR_TREE_EVENT_DEBUGGING
	PersonalLog::Messages::const_reverse_iterator it = instance.eventsLog.GetMessages().rbegin();
	PersonalLog::Messages::const_reverse_iterator end = instance.eventsLog.GetMessages().rend();

	m_data.eventLog.reserve(instance.eventsLog.GetMessages().size());
	for (; it != end; ++it)
	{
		m_data.eventLog.push_back(*it);
	}
	#endif
}

void DebugTreeSerializer::CollectExecutionErrorInfo(const DebugTree& debugTree)
{
	#ifdef DEBUG_MODULAR_BEHAVIOR_TREE
	m_data.errorLog.reserve(debugTree.GetSucceededAndFailedNodes().size() + 1);

	DynArray<DebugNodePtr>::const_iterator it = debugTree.GetSucceededAndFailedNodes().begin();
	DynArray<DebugNodePtr>::const_iterator end = debugTree.GetSucceededAndFailedNodes().end();

	string messageLine("ERROR: Root Node failed or succeeded");
	m_data.errorLog.push_back(messageLine);
	for (; it != end; ++it)
	{
		const BehaviorTree::Node* node = static_cast<const BehaviorTree::Node*>((*it)->node);
		messageLine.Format("(%d) %s.", node->GetXmlLine(), node->GetCreator()->GetTypeName());
		m_data.errorLog.push_back(messageLine);
	}
	#endif // DEBUG_MODULAR_BEHAVIOR_TREE
}

}

#endif // USING_BEHAVIOR_TREE_VISUALIZER
