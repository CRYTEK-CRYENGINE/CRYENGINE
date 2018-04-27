// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

/********************************************************************
   -------------------------------------------------------------------------
   File name:   AILog.cpp
   $Id$
   Description:

   -------------------------------------------------------------------------
   History:
   - ?
   - 2 Mar 2009	: Evgeny Adamenkov: Replaced IRenderer with CDebugDrawContext

 *********************************************************************/
#include "StdAfx.h"

#ifdef CRYAISYSTEM_VERBOSITY

	#include "AILog.h"
	#include "CAISystem.h"
	#include "CryAISystem.h"

	#include <CrySystem/ISystem.h>
	#include <CrySystem/ITimer.h>
	#include <CrySystem/IValidator.h>
	#include <CrySystem/IConsole.h>
	#include "DebugDrawContext.h"

// these should all be in sync - so testing one for 0 should be the same for all
ISystem* pSystem = 0;

static const char outputPrefix[] = "AI: ";
static const unsigned outputPrefixLen = sizeof(outputPrefix) - 1;

	#define DECL_OUTPUT_BUF char outputBufferLog[MAX_WARNING_LENGTH + outputPrefixLen];

static const int maxSavedMsgs = 5;
static const int maxSavedMsgLength = MAX_WARNING_LENGTH + outputPrefixLen + 1;
enum ESavedMsgType {SMT_WARNING, SMT_ERROR};
struct SSavedMsg
{
	ESavedMsgType savedMsgType;
	char          savedMsg[maxSavedMsgLength];
	CTimeValue    time;
};
static SSavedMsg savedMsgs[maxSavedMsgs];
static int savedMsgIndex = 0;

//====================================================================
// DebugDrawLabel
//====================================================================
static void DebugDrawLabel(ESavedMsgType type, float timeFrac, int col, int row, const char* szText)
{
	float ColumnSize = 11;
	float RowSize = 11;
	float baseY = 10;
	ColorB colorWarning(0, 255, 255);
	ColorB colorError(255, 255, 0);
	ColorB& color = (type == SMT_ERROR) ? colorError : colorWarning;
	CDebugDrawContext dc;

	float alpha = 1.0f;
	static float fadeFrac = 0.5f;
	if (timeFrac < fadeFrac && fadeFrac > 0.0f)
		alpha = timeFrac / fadeFrac;
	color.a = static_cast<uint8>(255 * alpha);

	float actualCol = ColumnSize * static_cast<float>(col);
	float actualRow;
	if (row >= 0)
		actualRow = baseY + RowSize * static_cast<float>(row);
	else
		actualRow = dc->GetHeight() - (baseY + RowSize * static_cast<float>(-row));

	dc->Draw2dLabel(actualCol, actualRow, 1.2f, color, false, "%s", szText);
}

//====================================================================
// DisplaySavedMsgs
//====================================================================
void AILogDisplaySavedMsgs()
{
	float savedMsgDuration = gAIEnv.CVars.OverlayMessageDuration;
	if (savedMsgDuration < 0.01f)
		return;
	static int col = 1;

	int row = -1;
	CTimeValue currentTime = gEnv->pTimer->GetFrameStartTime();
	CTimeValue time = currentTime - CTimeValue(savedMsgDuration);
	for (int i = 0; i < maxSavedMsgs; ++i)
	{
		int index = (maxSavedMsgs + savedMsgIndex - i) % maxSavedMsgs;
		if (savedMsgs[index].time < time)
			return;
		// get rid of msgs from the future - can happen during load/save
		if (savedMsgs[index].time > currentTime)
			savedMsgs[index].time = time;
		//      savedMsgIndex = (maxSavedMsgs + savedMsgIndex - 1) % maxSavedMsgs;

		float timeFrac = (savedMsgs[index].time - time).GetSeconds() / savedMsgDuration;
		DebugDrawLabel(savedMsgs[index].savedMsgType, timeFrac, col, row, savedMsgs[index].savedMsg);
		--row;
	}
}

//====================================================================
// AIInitLog
//====================================================================
void AIInitLog(ISystem* system)
{
	if (pSystem)
		AIWarning("Re-registering AI Logging");

	AIAssert(system);
	if (!system)
		return;
	IConsole* console = gEnv->pConsole;
	#ifdef _DEBUG
	int isDebug = 1;
	#else
	int isDebug = 0;
	#endif

	if (console)
		pSystem = system;

	for (int i = 0; i < maxSavedMsgs; ++i)
	{
		savedMsgs[i].savedMsg[0] = '\0';
		savedMsgs[i].savedMsgType = SMT_WARNING;
		savedMsgs[i].time = CTimeValue(0.0f);
		savedMsgIndex = 0;
	}
}

//====================================================================
// AIGetLogConsoleVerbosity
//====================================================================
int AIGetLogConsoleVerbosity()
{
	return gAIEnv.CVars.LogConsoleVerbosity;
}

//====================================================================
// AIGetLogFileVerbosity
//====================================================================
int AIGetLogFileVerbosity()
{
	return gAIEnv.CVars.LogFileVerbosity;
}

//====================================================================
// AICheckLogVerbosity
//====================================================================
bool AICheckLogVerbosity(const AI_LOG_VERBOSITY CheckVerbosity)
{
	bool bResult = false;

	const int iAILogVerbosity = AIGetLogFileVerbosity();
	const int iAIConsoleVerbosity = AIGetLogConsoleVerbosity();

	if (iAILogVerbosity >= CheckVerbosity || iAIConsoleVerbosity >= CheckVerbosity)
	{
		// Check against actual log system
		const int nVerbosity = gEnv->pLog->GetVerbosityLevel();
		bResult = (nVerbosity >= CheckVerbosity);
	}

	return bResult;
}

//===================================================================
// AIGetWarningErrorsEnabled
//===================================================================
bool AIGetWarningErrorsEnabled()
{
	return gAIEnv.CVars.EnableWarningsErrors != 0;
}

//====================================================================
// AIError
//====================================================================
void AIError(const char* format, ...)
{
	if (!pSystem || !AIGetWarningErrorsEnabled() || !AICheckLogVerbosity(AI_LOG_ERROR))
		return;

	DECL_OUTPUT_BUF;

	va_list args;
	va_start(args, format);
	cry_vsprintf(outputBufferLog, format, args);
	va_end(args);

	if (gEnv->IsEditor())
	{
		pSystem->Warning(VALIDATOR_MODULE_AI, VALIDATOR_ERROR, VALIDATOR_FLAG_AI, 0, "!AI: Error: %s", outputBufferLog);
	}
	else
	{
		gEnv->pLog->LogError("%s", outputBufferLog);
	}

	savedMsgIndex = (savedMsgIndex + 1) % maxSavedMsgs;
	savedMsgs[savedMsgIndex].savedMsgType = SMT_ERROR;
	cry_strcpy(savedMsgs[savedMsgIndex].savedMsg, outputBufferLog);
	savedMsgs[savedMsgIndex].time = gEnv->pTimer->GetFrameStartTime();
}

//====================================================================
// AIWarning
//====================================================================
void AIWarning(const char* format, ...)
{
	if (!pSystem || !AIGetWarningErrorsEnabled() || !AICheckLogVerbosity(AI_LOG_WARNING))
		return;

	DECL_OUTPUT_BUF;

	va_list args;
	va_start(args, format);
	cry_vsprintf(outputBufferLog, format, args);
	va_end(args);
	pSystem->Warning(VALIDATOR_MODULE_AI, VALIDATOR_WARNING, VALIDATOR_FLAG_AI, 0, "AI: %s", outputBufferLog);

	savedMsgIndex = (savedMsgIndex + 1) % maxSavedMsgs;
	savedMsgs[savedMsgIndex].savedMsgType = SMT_WARNING;
	cry_strcpy(savedMsgs[savedMsgIndex].savedMsg, outputBufferLog);
	savedMsgs[savedMsgIndex].time = gEnv->pTimer->GetFrameStartTime();
}

//====================================================================
// AILogAlways
//====================================================================
void AILogAlways(const char* format, ...)
{
	if (!pSystem)
		return;

	DECL_OUTPUT_BUF;

	memcpy(outputBufferLog, outputPrefix, outputPrefixLen);
	va_list args;
	va_start(args, format);
	cry_vsprintf(outputBufferLog + outputPrefixLen, sizeof(outputBufferLog) - outputPrefixLen, format, args);
	va_end(args);

	gEnv->pLog->Log("%s", outputBufferLog);
}

void AILogLoading(const char* format, ...)
{
	if (!pSystem)
		return;

	const char outputPrefix2[] = "--- AI: ";
	const unsigned outputPrefixLen2 = sizeof(outputPrefix2) - 1;

	DECL_OUTPUT_BUF;

	memcpy(outputBufferLog, outputPrefix2, outputPrefixLen2);
	va_list args;
	va_start(args, format);
	cry_vsprintf(outputBufferLog + outputPrefixLen2, sizeof(outputBufferLog) - outputPrefixLen2, format, args);
	va_end(args);

	gEnv->pLog->UpdateLoadingScreen("%s", outputBufferLog);
}

//====================================================================
// AIHandleLogMessage
//====================================================================
void AIHandleLogMessage(const char* outputBufferLog)
{
	const int cV = AIGetLogConsoleVerbosity();
	const int fV = AIGetLogFileVerbosity();

	if ((cV >= AI_LOG_PROGRESS) && (fV >= AI_LOG_PROGRESS))
		gEnv->pLog->Log("%s", outputBufferLog);
	else if (cV >= AI_LOG_PROGRESS)
		gEnv->pLog->LogToConsole("%s", outputBufferLog);
	else if (fV >= AI_LOG_PROGRESS)
		gEnv->pLog->LogToFile("%s", outputBufferLog);
}

//====================================================================
// AILogProgress
//====================================================================
void AILogProgress(const char* format, ...)
{
	if (!pSystem || !AICheckLogVerbosity(AI_LOG_PROGRESS))
		return;

	DECL_OUTPUT_BUF;

	memcpy(outputBufferLog, outputPrefix, outputPrefixLen);
	va_list args;
	va_start(args, format);
	cry_vsprintf(outputBufferLog + outputPrefixLen, sizeof(outputBufferLog) - outputPrefixLen, format, args);
	va_end(args);

	AIHandleLogMessage(outputBufferLog);
}

//====================================================================
// AILogEvent
//====================================================================
void AILogEvent(const char* format, ...)
{
	if (!pSystem || !AICheckLogVerbosity(AI_LOG_EVENT))
		return;

	DECL_OUTPUT_BUF;

	memcpy(outputBufferLog, outputPrefix, outputPrefixLen);
	va_list args;
	va_start(args, format);
	cry_vsprintf(outputBufferLog + outputPrefixLen, sizeof(outputBufferLog) - outputPrefixLen, format, args);
	va_end(args);

	AIHandleLogMessage(outputBufferLog);
}

//====================================================================
// AILogComment
//====================================================================
void AILogComment(const char* format, ...)
{
	if (!pSystem || !AICheckLogVerbosity(AI_LOG_COMMENT))
		return;

	DECL_OUTPUT_BUF;

	memcpy(outputBufferLog, outputPrefix, outputPrefixLen);
	va_list args;
	va_start(args, format);
	cry_vsprintf(outputBufferLog + outputPrefixLen, sizeof(outputBufferLog) - outputPrefixLen, format, args);
	va_end(args);

	AIHandleLogMessage(outputBufferLog);
}

#endif // CRYAISYSTEM_VERBOSITY
