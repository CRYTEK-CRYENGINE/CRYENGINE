// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#ifndef ExecutionStackFileLogger_h
#define ExecutionStackFileLogger_h

#pragma once

#include <CryAISystem/BehaviorTree/IBehaviorTree.h>

#ifdef DEBUG_MODULAR_BEHAVIOR_TREE
namespace BehaviorTree
{
class ExecutionStackFileLogger
{
public:
	explicit ExecutionStackFileLogger(const EntityId entityId);
	~ExecutionStackFileLogger();
	void LogDebugTree(const DebugTree& debugTree, const UpdateContext& updateContext, const BehaviorTreeInstance& instance);

private:
	enum LogFileOpenState
	{
		CouldNotAdjustFileName,
		NotYetAttemptedToOpenForWriteAccess,
		OpenForWriteAccessFailed,
		OpenForWriteAccessSucceeded
	};

	ExecutionStackFileLogger(const ExecutionStackFileLogger&);
	ExecutionStackFileLogger& operator=(const ExecutionStackFileLogger&);

	void                      LogNodeRecursively(const DebugNode& debugNode, const int indentLevel);

	string           m_agentName;
	CryPathString    m_logFilePath;
	LogFileOpenState m_openState;
	CCryFile         m_logFile;
};
}
#endif  // DEBUG_MODULAR_BEHAVIOR_TREE

#endif  // ExecutionStackFileLogger_h
