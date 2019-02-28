// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#ifndef AILOG_H
#define AILOG_H

#if _MSC_VER > 1000
	#pragma once
#endif

#ifndef _DEBUG
// comment this out to remove asserts at compile time
	#define ENABLE_AI_ASSERT
#else
	#define ENABLE_AI_ASSERT
#endif

/// Sets up console variables etc
void AIInitLog(struct ISystem* system);

/// Indicates if AI warnings/errors are enabled (e.g. for validation code you
/// don't want to run if there would be no output)
bool AIGetWarningErrorsEnabled();

/// Reports an AI Warning. This should be used when AI gets passed data from outside that
/// is "bad", but we can handle it and it shouldn't cause serious problems.
void AIWarning(const char* format, ...) PRINTF_PARAMS(1, 2);

/// Reports an AI Error. This should be used when AI either gets passed data that is so
/// bad that we can't handle it, or we want to detect/check against a logical/programming
/// error and be sure this check isn't ever compiled out (like an assert might be). Unless
/// in performance critical code, prefer a manual test with AIError to using AIAssert.
/// Note that in the editor this will return (so problems can be fixed). In game it will cause
/// execution to halt.
/// Use "Pos: (...)" inside format string to setup the position in Error Report dialog
void AIError(const char* format, ...) PRINTF_PARAMS(1, 2);

/// Asserts that a condition is true. Can be enabled/disabled at compile time (will be enabled
/// for testing). Can also be enabled/disabled at run time (if enabled at compile time), so
/// code below this should handle the condition = false case. It should be used to trap
/// logical/programming errors, NOT data/script errors - i.e. once the code has been tested and
/// debugged, it should never get hit, whatever changes are made to data/script
#ifdef ENABLE_AI_ASSERT
	#define AIAssert(exp) CRY_ASSERT(exp)
#else
	#define AIAssert(exp) ((void)0)
#endif

/// Used to log progress points - e.g. startup/shutdown/loading of AI. With the default (for testers etc)
/// verbosity settings this will write to the log file and the console.
void AILogProgress(const char* format, ...) PRINTF_PARAMS(1, 2);

/// Used to log significant events like AI state changes. With the default (for testers etc)
/// verbosity settings this will write to the log file, but not the console.
void AILogEvent(const char* format, ...) PRINTF_PARAMS(1, 2);

/// Used to log events like that AI people might be interested in during their private testing.
/// With the default (for testers etc) verbosity settings this will not write to the log file
/// or the console.
void AILogComment(const char* format, ...) PRINTF_PARAMS(1, 2);

/// Should probably never be used since it bypasses verbosity checks
void AILogAlways(const char* format, ...) PRINTF_PARAMS(1, 2);

/// Displays messages during loading - also gives an opportunity for system/display to update
void AILogLoading(const char* format, ...) PRINTF_PARAMS(1, 2);

#endif // file included

