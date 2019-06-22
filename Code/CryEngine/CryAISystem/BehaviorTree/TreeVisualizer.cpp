// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "TreeVisualizer.h"

#include <CryAISystem/IAIDebugRenderer.h>
#include <CryAISystem/BehaviorTree/Node.h>
#include <CryAISystem/BehaviorTree/TimestampCollection.h>
#include <CryCore/Containers/VariableCollection.h>

#include <CrySerialization/IArchive.h>
#include <CrySerialization/STL.h>

#ifdef DEBUG_MODULAR_BEHAVIOR_TREE
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
  const MessageQueue& behaviorLog,
  const TimestampCollection& timestampCollection,
  const Blackboard& blackboard, 
  const MessageQueue& eventsLog)
{
	if (gAIEnv.CVars.behaviorTree.ModularBehaviorTreeDebugTree)
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

	if (gAIEnv.CVars.behaviorTree.ModularBehaviorTreeDebugLog)
	{
		DrawBehaviorLog(behaviorLog);
	}

	if (gAIEnv.CVars.behaviorTree.ModularBehaviorTreeDebugTimestamps)
	{
		DrawTimestampCollection(timestampCollection);
	}

	if (gAIEnv.CVars.behaviorTree.ModularBehaviorTreeDebugBlackboard)
	{
		DrawBlackboard(blackboard);
	}

	if (gAIEnv.CVars.behaviorTree.ModularBehaviorTreeDebugEvents)
	{
		DrawEventLog(eventsLog);
	}
}

void TreeVisualizer::DrawNode(const DebugNode& node, const uint32 depth)
{
	// only show currently running nodes
	if (node.nodeStatus != Running)
		return;

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

	// Custom debug text from the node
	if (!node.customDebugText.empty())
	{
		str += " - " + node.customDebugText;
		hasCustomText = true;
	}

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

void TreeVisualizer::DrawTimestampCollection(const TimestampCollection& timestampCollection)
{
	SetLinePosition(timestampPosX, timestampPosY);
	DrawLine("Timestamp Collection", Col_Yellow);
	DrawLine("", Col_White);

	Timestamps::const_iterator it = timestampCollection.GetTimestamps().begin();
	Timestamps::const_iterator end = timestampCollection.GetTimestamps().end();
	for (; it != end; ++it)
	{
		stack_string s;
		ColorB color;
		if (it->IsValid())
		{
			s.Format("%s [%.2f]", it->id.timestampName, m_updateContext.frameStartTime.GetDifferenceInSeconds(it->time));
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

		stack_string variableValue;
		Serialization::TypeID typeId = variable->GetDataTypeId();
		if (typeId == Serialization::TypeID::get<int>())
		{
			int data;
			variable->GetData(data);
			variableValue.Format("%d", data);
		}
		else if (typeId == Serialization::TypeID::get<float>())
		{
			float data;
			variable->GetData(data);
			variableValue.Format("%.2f", data);
		}
		else if (typeId == Serialization::TypeID::get<Vec3>())
		{
			Vec3 data;
			variable->GetData(data);
			variableValue.Format("(%.2f, %.2f, %.2f)", data.x, data.y, data.z);
		}
		else if (typeId == Serialization::TypeID::get<bool>())
		{
			bool data;
			variable->GetData(data);
			variableValue.Format("%s", data ? "True" : "False");
		}
		else if (typeId == Serialization::TypeID::get<string>())
		{
			string data;
			variable->GetData(data);
			variableValue.Format("%s", data);
		}
		else if (typeId == Serialization::TypeID::get<double>())
		{
			double data;
			variable->GetData(data);
			variableValue.Format("%.5f", data);
		}
		else if (typeId == Serialization::TypeID::get<Vec2>())
		{
			Vec2 data;
			variable->GetData(data);
			variableValue.Format("(%f, %f)", id.name.c_str(), data.x, data.y);
		}
		else if (typeId == Serialization::TypeID::get<char>())
		{
			char data;
			variable->GetData(data);
			variableValue.Format("%c", data);
		}
		else
		{
			variableValue = "TYPE NOT SUPPORTED";
		}

		stack_string variableText;
		variableText.Format("%s - %s", id.name.c_str(), variableValue.c_str());
		DrawLine(variableText.c_str(), Col_White);
	}
	#endif
}

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
		CollectTimeStamps(treeInstance, updateContext);
		CollectEventsLog(treeInstance);
	}
}

void DebugTreeSerializer::Serialize(Serialization::IArchive& archive)
{
	archive(m_data, "aiMbt");
}

void DebugTreeSerializer::CollectTreeNodeInfo(const DebugNode& debugNode, const UpdateContext& updateContext, TreeNode& outNode)
{
	// only show currently running nodes
	if (debugNode.nodeStatus != Running)
		return;

	const uint32 xmlLine = static_cast<const Node*>(debugNode.node)->GetXmlLine();

	stack_string tempOutput;
	const char* szNodeType = static_cast<const Node*>(debugNode.node)->GetCreator()->GetTypeName();
	tempOutput += szNodeType;

	// Custom debug text from the node
	if (!debugNode.customDebugText.empty())
	{
		tempOutput += " - " + debugNode.customDebugText;
	}

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

void DebugTreeSerializer::CollectTimeStamps(const BehaviorTreeInstance& instance, const UpdateContext& updateContext)
{
	CTimeValue timeNow = GetAISystem()->GetFrameStartTime();
	Timestamps::const_iterator it = instance.timestampCollection.GetTimestamps().begin();
	Timestamps::const_iterator end = instance.timestampCollection.GetTimestamps().end();

	m_data.timeStamps.reserve(instance.timestampCollection.GetTimestamps().size());
	for (; it != end; ++it)
	{
		TimeStamp timeStamp;
		timeStamp.name = it->id.timestampName;
		timeStamp.bIsValid = it->IsValid();
		timeStamp.value = it->IsValid() ? (updateContext.frameStartTime.GetDifferenceInSeconds(it->time)) : 0.0f;

		m_data.timeStamps.push_back(timeStamp);
	}
}

void DebugTreeSerializer::CollectEventsLog(const BehaviorTreeInstance& instance)
{
	PersonalLog::Messages::const_reverse_iterator it = instance.eventsLog.GetMessages().rbegin();
	PersonalLog::Messages::const_reverse_iterator end = instance.eventsLog.GetMessages().rend();

	m_data.eventLog.reserve(instance.eventsLog.GetMessages().size());
	for (; it != end; ++it)
	{
		m_data.eventLog.push_back(*it);
	}
}

void DebugTreeSerializer::CollectExecutionErrorInfo(const DebugTree& debugTree)
{
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
}
}

#endif // DEBUG_MODULAR_BEHAVIOR_TREE
