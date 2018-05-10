// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include "SandboxAPI.h"
#include <CrySystem/ILog.h>
#include <CrySystem/IConsole.h>

//struct IConsole;
//struct ICVar;

//////////////////////////////////////////////////////////////////////////
// Global log functions.
//////////////////////////////////////////////////////////////////////////
//! Displays error message.
extern void Error(const char* format, ...);
//! Log to console and file.
extern void Log(const char* format, ...);
//! Display Warning dialog.
extern void Warning(const char* format, ...);

/*!
 *	CLogFile implements ILog interface.
 */
class SANDBOX_API CLogFile : public ILogCallback
{
public:
	static const char* GetLogFileName();
	static void        AttachListBox(HWND hWndListBox) { m_hWndListBox = hWndListBox; };
	static void        AttachEditBox(HWND hWndEditBox) { m_hWndEditBox = hWndEditBox; };

	//! Write to log spanpshot of current process memory usage.
	static string GetMemUsage();

	//DEPRECATED, use CryLog directly, do not use this file outside of Sandbox project
	static void    WriteString(const char* pszString);
	static void    WriteLine(const char* pszLine);
	static void    FormatLine(PSTR pszMessage, ...);

	//////////////////////////////////////////////////////////////////////////
	// ILogCallback
	//////////////////////////////////////////////////////////////////////////
	virtual void OnWriteToConsole(const char* sText, bool bNewLine);
	virtual void OnWriteToFile(const char* sText, bool bNewLine);
	//////////////////////////////////////////////////////////////////////////

	// logs some useful information
	// should be called after CryLog() is available
	static void AboutSystem();

private:
	static void OpenFile();

	// Attached control(s)
	static HWND m_hWndListBox;
	static HWND m_hWndEditBox;
	static bool m_bShowMemUsage;
};


