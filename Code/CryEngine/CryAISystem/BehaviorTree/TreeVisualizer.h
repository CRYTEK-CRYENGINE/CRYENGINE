// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#ifndef TreeVisualizer_h
#define TreeVisualizer_h

#pragma once

#include <CryAISystem/BehaviorTree/IBehaviorTree.h>

#ifdef USING_BEHAVIOR_TREE_VISUALIZER
namespace BehaviorTree
{
class TreeVisualizer
{
public:
	TreeVisualizer(const UpdateContext& updateContext);

	void Draw(
	  const DebugTree& tree
	  , const char* behaviorTreeName
	  , const char* agentName
	#ifdef USING_BEHAVIOR_TREE_LOG
	  , const MessageQueue& behaviorLog
	#endif // USING_BEHAVIOR_TREE_LOG
	  , const TimestampCollection& timestampCollection
	  , const Blackboard& blackboard
	#ifdef USING_BEHAVIOR_TREE_EVENT_DEBUGGING
	  , const MessageQueue& eventsLog
	#endif // USING_BEHAVIOR_TREE_EVENT_DEBUGGING
	  );

private:
	void SetLinePosition(float x, float y);
	void DrawLine(const char* label, const ColorB& color);

	void DrawNode(const DebugNode& node, const uint32 depth);

	#ifdef USING_BEHAVIOR_TREE_LOG
	void DrawBehaviorLog(const MessageQueue& behaviorLog);
	#endif // USING_BEHAVIOR_TREE_LOG

	#ifdef USING_BEHAVIOR_TREE_TIMESTAMP_DEBUGGING
	void DrawTimestampCollection(const TimestampCollection& timestampCollection);
	#endif // USING_BEHAVIOR_TREE_TIMESTAMP_DEBUGGING

	void DrawBlackboard(const Blackboard& blackboard);

	#ifdef USING_BEHAVIOR_TREE_EVENT_DEBUGGING
	void DrawEventLog(const MessageQueue& eventsLog);
	#endif // USING_BEHAVIOR_TREE_EVENT_DEBUGGING

	const UpdateContext& m_updateContext;
	float                m_currentLinePositionX;
	float                m_currentLinePositionY;
};

class DebugTreeSerializer
{
private:
	struct TreeNode
	{
		void Serialize(Serialization::IArchive& archive);

		int                   xmlLine;
		string                node;
		std::vector<TreeNode> children;
	};

	struct MBTree
	{
		void Serialize(Serialization::IArchive& archive);

		string   name;
		TreeNode rootNode;
	};

	struct Variable
	{
		void Serialize(Serialization::IArchive& archive);

		string name;
		bool   value;
	};
	typedef std::vector<Variable> Variables;

	struct TimeStamp
	{
		void Serialize(Serialization::IArchive& archive);

		string name;
		float  value;
		bool   bIsValid;
	};
	typedef std::vector<TimeStamp> TimeStamps;

	typedef std::vector<string>    EventsLog;
	typedef std::vector<string>    ErrorLog;

	struct Data
	{
		void Serialize(Serialization::IArchive& archive);

		MBTree     tree;
		Variables  variables;
		TimeStamps timeStamps;
		EventsLog  eventLog;
		ErrorLog   errorLog;
	};

public:
	DebugTreeSerializer() {};
	DebugTreeSerializer(const BehaviorTreeInstance& treeInstance, const DebugTree& debugTree, const UpdateContext& updateContext, const bool bExecutionError);

	void Serialize(Serialization::IArchive& archive);

private:
	void CollectTreeNodeInfo(const DebugNode& debugNode, const UpdateContext& updateContext, TreeNode& outNode);
	void CollectVariablesInfo(const BehaviorTreeInstance& instance);
	void CollectTimeStamps(const BehaviorTreeInstance& instance);
	void CollectEventsLog(const BehaviorTreeInstance& instance);
	void CollectExecutionErrorInfo(const DebugTree& debugTree);

private:
	Data m_data;
};
}
#endif // USING_BEHAVIOR_TREE_VISUALIZER

#endif // TreeVisualizer_h
