// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#ifndef ExecutionStackFileLogger_h
#define ExecutionStackFileLogger_h

#pragma once

#include <CryAISystem/BehaviorTree/IBehaviorTree.h>

#ifdef USING_BEHAVIOR_TREE_EXECUTION_STACKS_FILE_LOG
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

	void                      LogNodeRecursively(const DebugNode& debugNode, const UpdateContext& updateContext, const BehaviorTreeInstance& instance, const int indentLevel);

	string           m_agentName;
	char             m_logFilePath[ICryPak::g_nMaxPath];
	LogFileOpenState m_openState;
	CCryFile         m_logFile;
};
}
#endif  // USING_BEHAVIOR_TREE_EXECUTION_STACKS_FILE_LOG

#endif  // ExecutionStackFileLogger_h
