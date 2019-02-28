// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"

//[AlexMcC|17.02.10] This file (and its header) are copy/pasted from CryAISystem.
//They should be removed from Sandbox.

#include "AILog.h"

#include <CrySystem/ISystem.h>
#include <CrySystem/ITimer.h>
#include <CrySystem/IValidator.h>
#include <CrySystem/IConsole.h>
#include <CrySystem/ITestSystem.h>

// these should all be in sync - so testing one for 0 should be the same for all
ISystem* pSystem = 0;
ICVar* pAILogConsoleVerbosity = 0;
ICVar* pAILogFileVerbosity = 0;
ICVar* pAIEnableWarningsErrors = 0;
ICVar* pAIOverlayMessageDuration = 0;
ICVar* pAIShowBehaviorCalls = 0;

static const char outputPrefix[] = "AI: ";
static const unsigned outputPrefixLen = sizeof(outputPrefix) - 1;

#define DECL_OUTPUT_BUF char outputBufferLog[MAX_WARNING_LENGTH + outputPrefixLen - 512]

enum AI_LOG_VERBOSITY
{
	LOG_OFF      = 0,
	LOG_PROGRESS = 1,
	LOG_EVENT    = 2,
	LOG_COMMENT  = 3
};

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
	IConsole* console = system->GetIConsole();

	// NOTE Mrz 4, 2008: <pvl> this is the editor, let's assume devmode instead
	// of trying to access the function that lives in a different dll and isn't
	// exported
	bool inDevMode = true; //::IsAIInDevMode();
#ifdef _DEBUG
	int isDebug = 1;
#else
	int isDebug = 0;
#endif

	if (console)
	{
		pSystem = system;
		if (!pAILogConsoleVerbosity)
			pAILogConsoleVerbosity = console->GetCVar("ai_LogConsoleVerbosity");
		if (!pAILogFileVerbosity)
			pAILogFileVerbosity = console->GetCVar("ai_LogFileVerbosity");
		if (!pAIEnableWarningsErrors)
			pAIEnableWarningsErrors = console->GetCVar("ai_EnableWarningsErrors");
		if (!pAIOverlayMessageDuration)
			pAIOverlayMessageDuration = console->GetCVar("ai_OverlayMessageDuration");
	}
	if (!pAIShowBehaviorCalls)
		pAIShowBehaviorCalls = REGISTER_INT("ai_ShowBehaviorCalls", 0, VF_DUMPTODISK, "Prints out each behavior LUA call for each puppet: 1 or 0");
}

//====================================================================
// AIGetLogConsoleVerbosity
//====================================================================
int AIGetLogConsoleVerbosity()
{
	if (pAILogConsoleVerbosity)
		return pAILogConsoleVerbosity->GetIVal();
	else
		return -1;
}

//====================================================================
// AIGetLogFileVerbosity
//====================================================================
int AIGetLogFileVerbosity()
{
	if (pAILogFileVerbosity)
		return pAILogFileVerbosity->GetIVal();
	else
		return -1;
}

//===================================================================
// AIGetWarningErrorsEnabled
//===================================================================
bool AIGetWarningErrorsEnabled()
{
	if (pAIEnableWarningsErrors)
		return pAIEnableWarningsErrors->GetIVal() != 0;
	else
		return true;
}

//====================================================================
// AIWarning
//====================================================================
// use "pos: (...)" or "position: (...)" in format for possibility moving viewport camera
// to specified 3D position while dblClick in ErrorReportDialog
void AIWarning(const char* format, ...)
{
	if (!pSystem)
		return;
	if (pAIEnableWarningsErrors && 0 == pAIEnableWarningsErrors->GetIVal())
		return;

	DECL_OUTPUT_BUF;

	va_list args;
	va_start(args, format);
	cry_vsprintf(outputBufferLog, format, args);
	va_end(args);
	pSystem->Warning(VALIDATOR_MODULE_AI, VALIDATOR_WARNING, VALIDATOR_FLAG_AI, 0, "AI: Warning: %s", outputBufferLog);
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

	pSystem->GetILog()->Log(outputBufferLog);
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

	pSystem->GetILog()->UpdateLoadingScreen(outputBufferLog);
}

//====================================================================
// AILogProgress
//====================================================================
void AILogProgress(const char* format, ...)
{
	if (!pSystem)
		return;
	int cV = pAILogConsoleVerbosity->GetIVal();
	int fV = pAILogFileVerbosity->GetIVal();
	if (cV < LOG_PROGRESS && fV < LOG_PROGRESS)
		return;

	DECL_OUTPUT_BUF;

	memcpy(outputBufferLog, outputPrefix, outputPrefixLen);
	va_list args;
	va_start(args, format);
	cry_vsprintf(outputBufferLog + outputPrefixLen, sizeof(outputBufferLog) - outputPrefixLen, format, args);
	va_end(args);

	if ((cV >= LOG_PROGRESS) && (fV >= LOG_PROGRESS))
		pSystem->GetILog()->Log(outputBufferLog);
	else if (cV >= LOG_PROGRESS)
		pSystem->GetILog()->LogToConsole(outputBufferLog);
	else if (fV >= LOG_PROGRESS)
		pSystem->GetILog()->LogToFile(outputBufferLog);
}

//====================================================================
// AILogEvent
//====================================================================
void AILogEvent(const char* format, ...)
{
	if (!pSystem)
		return;
	int cV = pAILogConsoleVerbosity->GetIVal();
	int fV = pAILogFileVerbosity->GetIVal();
	if (cV < LOG_EVENT && fV < LOG_EVENT)
		return;

	DECL_OUTPUT_BUF;

	memcpy(outputBufferLog, outputPrefix, outputPrefixLen);
	va_list args;
	va_start(args, format);
	cry_vsprintf(outputBufferLog + outputPrefixLen, sizeof(outputBufferLog) - outputPrefixLen, format, args);
	va_end(args);

	if ((cV >= LOG_EVENT) && (fV >= LOG_EVENT))
		pSystem->GetILog()->Log(outputBufferLog);
	else if (cV >= LOG_EVENT)
		pSystem->GetILog()->LogToConsole(outputBufferLog);
	else if (fV >= LOG_EVENT)
		pSystem->GetILog()->LogToFile(outputBufferLog);
}

//====================================================================
// AILogComment
//====================================================================
void AILogComment(const char* format, ...)
{
	if (!pSystem)
		return;
	int cV = pAILogConsoleVerbosity->GetIVal();
	int fV = pAILogFileVerbosity->GetIVal();
	if (cV < LOG_COMMENT && fV < LOG_COMMENT)
		return;

	DECL_OUTPUT_BUF;

	memcpy(outputBufferLog, outputPrefix, outputPrefixLen);
	va_list args;
	va_start(args, format);
	cry_vsprintf(outputBufferLog + outputPrefixLen, sizeof(outputBufferLog) - outputPrefixLen, format, args);
	va_end(args);

	if ((cV >= LOG_COMMENT) && (fV >= LOG_COMMENT))
		pSystem->GetILog()->Log(outputBufferLog);
	else if (cV >= LOG_COMMENT)
		pSystem->GetILog()->LogToConsole(outputBufferLog);
	else if (fV >= LOG_COMMENT)
		pSystem->GetILog()->LogToFile(outputBufferLog);
}

// for error - we want a message box
#if CRY_PLATFORM_WINDOWS
	#include <CryCore/Platform/CryWindows.h>

//====================================================================
// AIError
//====================================================================
void AIError(const char* format, ...)
{
	if (!pSystem)
		return;
	if (pAIEnableWarningsErrors && 0 == pAIEnableWarningsErrors->GetIVal())
		return;

	DECL_OUTPUT_BUF;

	va_list args;
	va_start(args, format);
	cry_vsprintf(outputBufferLog, format, args);
	va_end(args);
	pSystem->Warning(VALIDATOR_MODULE_AI, VALIDATOR_ERROR, VALIDATOR_FLAG_AI, 0, "AI: Error: %s", outputBufferLog);

	if (GetISystem()->GetITestSystem())
	{
		// supress user interaction for automated test
		if (GetISystem()->GetITestSystem()->GetILog())
			GetISystem()->GetITestSystem()->GetILog()->LogError(outputBufferLog);
	}
	else
	{
		static bool sDoMsgBox = true;
		if (sDoMsgBox && !gEnv->IsEditor())
		{
			static char msg[4096];
			cry_sprintf(msg,
			            "AI: %s \n"
			            "ABORT to abort execution\n"
			            "RETRY to continue (Don't expect AI to work properly)\n"
			            "IGNORE to continue without any more of these AI error msg boxes", outputBufferLog);
			// Write out via MessageBox
			EQuestionResult nCode = CryMessageBox(
			  msg,
			  "AI Error", eMB_AbortRetryIgnore);

			// Abort: abort the program
			if (nCode == EQuestionResult::eQR_Abort)
				CryFatalError(" AI: %s", outputBufferLog);
			else if (nCode == EQuestionResult::eQR_Ignore)
				sDoMsgBox = false;
		}
	}
}

#else // CRY_PLATFORM_WINDOWS

void AIAssertHit(const char* expr, const char* filename, unsigned lineno)
{
}

//====================================================================
// AIError
//====================================================================
void AIError(const char* format, ...)
{
	if (!pSystem)
		return;
	if (pAIEnableWarningsErrors && 0 == pAIEnableWarningsErrors->GetIVal())
		return;

	DECL_OUTPUT_BUF;

	va_list args;
	va_start(args, format);
	cry_vsprintf(outputBufferLog, format, args);
	va_end(args);
	pSystem->Warning(VALIDATOR_MODULE_AI, VALIDATOR_ERROR, VALIDATOR_FLAG_AI, 0, "AI: Error: %s", outputBufferLog);
}

#endif  // CRY_PLATFORM_WINDOWS

