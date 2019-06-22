// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "ExecutionStackFileLogger.h"
#include <CryAISystem/BehaviorTree/Node.h>

#ifdef DEBUG_MODULAR_BEHAVIOR_TREE
namespace BehaviorTree
{

static bool IsCharAllowedInFileNames(const char ch)
{
	if (ch >= 'a' && ch <= 'z')
		return true;

	if (ch >= 'A' && ch <= 'Z')
		return true;

	if (ch >= '0' && ch <= '9')
		return true;

	if (ch == '_' || ch == '-' || ch == '.')
		return true;

	return false;
}

static void SanitizeFileName(CryPathString& fileName)
{
	for (CryPathString::iterator it = fileName.begin(); it != fileName.end(); ++it)
	{
		if (!IsCharAllowedInFileNames(*it))
			*it = '_';
	}
}

ExecutionStackFileLogger::ExecutionStackFileLogger(const EntityId entityId)
{
	if (const IEntity* pEntity = gEnv->pEntitySystem->GetEntity(entityId))
	{
		m_agentName = pEntity->GetName();
	}

	CryPathString fileName;
	fileName.Format("MBT_%s_%i.txt", m_agentName.c_str(), entityId);
	SanitizeFileName(fileName);

	CryPathString filePath("%USER%/MBT_Logs/");
	filePath.append(fileName);

	gEnv->pCryPak->AdjustFileName(filePath.c_str(), m_logFilePath, ICryPak::FLAGS_FOR_WRITING);
	if (!m_logFilePath.empty())
	{
		m_openState = NotYetAttemptedToOpenForWriteAccess;
	}
	else
	{
		m_openState = CouldNotAdjustFileName;
	}
}

ExecutionStackFileLogger::~ExecutionStackFileLogger()
{
	m_logFile.Close();
}

void ExecutionStackFileLogger::LogDebugTree(const DebugTree& debugTree, const UpdateContext& updateContext, const BehaviorTreeInstance& instance)
{
	if (m_openState == NotYetAttemptedToOpenForWriteAccess)
	{
		m_openState = m_logFile.Open(m_logFilePath, "w") ? OpenForWriteAccessSucceeded : OpenForWriteAccessFailed;

		// successfully opened? -> write header
		if (m_openState == OpenForWriteAccessSucceeded)
		{
			stack_string header;
			char startTime[1024] = "???";
			time_t ltime;
			if (time(&ltime) != (time_t)-1)
			{
				if (struct tm* pTm = localtime(&ltime))
				{
					strftime(startTime, CRY_ARRAY_COUNT(startTime), "%Y-%m-%d %H:%M:%S", pTm);
				}
			}
			header.Format("Modular Behavior Tree '%s' for agent '%s' with EntityId = %i (logging started at %s)\n\n", instance.behaviorTreeTemplate->mbtFilename.c_str(), m_agentName.c_str(), updateContext.entityId, startTime);
			m_logFile.Write(header.c_str(), header.length());
			m_logFile.Flush();
		}
	}

	if (m_openState == OpenForWriteAccessSucceeded)
	{
		if (const DebugNode* pRoot = debugTree.GetFirstNode().get())
		{
			stack_string textLine;

			// current time
			int hours, minutes, seconds, milliseconds;
			gEnv->pTimer->GetAsyncTime().Split(&hours, &minutes, &seconds, &milliseconds);
			textLine.Format("time = %i:%02i:%02i:%03i, system frame = %i\n", hours, minutes, seconds, milliseconds, (int)gEnv->nMainFrameID);
			m_logFile.Write(textLine.c_str(), textLine.length());

			LogNodeRecursively(*pRoot, 0);

			textLine = "-----------------------------------------\n";
			m_logFile.Write(textLine.c_str(), textLine.length());
		}
	}
}

void ExecutionStackFileLogger::LogNodeRecursively(const DebugNode& debugNode, const int indentLevel)
{
	const Node* pNode = static_cast<const Node*>(debugNode.node);
	const uint32 lineNum = pNode->GetXmlLine();
	const char* nodeType = pNode->GetCreator()->GetTypeName();
	stack_string customText;
	if (!debugNode.customDebugText.empty())
	{
		customText.append(" - ");
		customText.append(debugNode.customDebugText);
	}

	const char* strNodeStatus;
	switch (debugNode.nodeStatus)
	{
	case Running:
		strNodeStatus = "[RUNNING]  ";
		break;

	case Success:
		strNodeStatus = "[SUCCEEDED]";
		break;

	case Failure:
		strNodeStatus = "[FAILED]   ";
		break;

	case Invalid:
	default:
		strNodeStatus = "[INVALID]  ";
		break;
	}

	stack_string textLine;
	textLine.Format("%5i %s %*s %s%s\n", lineNum, strNodeStatus, indentLevel * 2, "", nodeType, customText.c_str());
	m_logFile.Write(textLine.c_str(), textLine.length());

	for (DebugNode::Children::const_iterator it = debugNode.children.begin(), end = debugNode.children.end(); it != end; ++it)
	{
		LogNodeRecursively(*(*it), indentLevel + 1);
	}
}

}
#endif  // DEBUG_MODULAR_BEHAVIOR_TREE
