// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#ifndef __DebugCallStack_h__
#define __DebugCallStack_h__

#if _MSC_VER > 1000
#pragma once
#endif

class ILogger;

//! Limits the maximal number of functions in call stack.
const int MAX_DEBUG_STACK_ENTRIES = 30;

//!============================================================================
//!
//! DebugCallStack class, capture call stack information from symbol files.
//!
//!============================================================================
class DebugCallStack
{
public:
	// Returns single instance of DebugStack
	static DebugCallStack*	instance();

	void SetLog(ILogger* log);

	//! Get current call stack information.
	void getCallStack( std::vector<string> &functions );

	int	 handleException( void *exception_pointer );

	struct ErrorHandlerScope
	{
		explicit ErrorHandlerScope(ILogger* log);
		~ErrorHandlerScope();
	};

private:
	DebugCallStack();
	virtual ~DebugCallStack();

	bool initSymbols();
	void doneSymbols();
	
	string	LookupFunctionName( void *adderss,bool fileInfo );
	int			updateCallStack( void *exception_pointer );
	void		FillStackTrace(int maxStackEntries = MAX_DEBUG_STACK_ENTRIES, int skipNumFunctions = 0);

	static	int unhandledExceptionHandler( void *exception_pointer );

	//! Return name of module where exception happened.
	const char* getExceptionModule() { return m_excModule; }
	const char* getExceptionLine() { return m_excLine; }

	void installErrorHandler();
	void uninstallErrorHandler();

	void PrintException(EXCEPTION_POINTERS* pex);

	std::vector<string> m_functions;
	static DebugCallStack* m_instance;

	char m_excLine[256];
	char m_excModule[128];

	void* m_pVectoredExceptionHandlerHandle;

	bool	m_symbols;

	CONTEXT  m_context;

	ILogger* m_pLog;
};

#endif // __DebugCallStack_h__