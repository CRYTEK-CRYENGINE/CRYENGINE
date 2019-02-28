// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

////////////////////////////////////////////////////////////////////////////
//
//  Crytek Engine Source File.
//  Copyright (C), Crytek Studios, 1999-2014.
// -------------------------------------------------------------------------
//  File name:   EngineListener.h
//  Version:     v1.00
//  Created:     03/03/2014 by Matthijs vd Meide
//  Compilers:   Visual Studio 2010
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////

#pragma once
#include <CryCore/Platform/platform.h>
#include <CrySystem/ISystem.h>
#include <CrySystem/IConsole.h>
#include <CryString/CryString.h>
#include <set>
#include <CryCore/BoostHelpers.h> // to make sure we get own throw_exceptions
#include <boost/circular_buffer.hpp>
#include <functional>
#include <algorithm>

#include <IEditor.h>
#include "ICommandManager.h"

//event handler for console events
//this is the "engine-side" event handler that takes care of caching etc
class CEngineListener : public IOutputPrintSink, public ICVarDumpSink, public IConsoleVarSink
{
public:
	//force refreshing of CVars and commands
	//this COULD get out of sync if someone was to register commands after initializing this listener
	void RefreshCVarsAndCommands();

private:
	//the type of string to use
	typedef ::string t_string;

	//the known CVars, sorted by name, case-insensitive
	struct SCVarNameComparer
	{
		bool operator()(const ICVar* left, const ICVar* right) const
		{
			return StringCompare(left->GetName(), right->GetName()) < 0;
		}
	};
	typedef std::set<ICVar*, SCVarNameComparer> t_cVars;

	//the known commands, sorted by name, case-insensitive
	struct SStringComparer
	{
		bool operator()(const t_string& left, const t_string& right) const
		{
			return StringCompare(left.c_str(), right.c_str()) < 0;
		}
	};
	typedef std::set<t_string, SStringComparer> t_commands;

	//updates a CVar
	void UpdateCVar(ICVar* pVar, bool changed);

	//releases all references to a CVar
	void ReleaseCVar(ICVar* pVar);

	//event handler: implements IOutputPrintSink
	virtual void Print(const char* inszText);

	//event handler: implements ICVarDumpSink
	virtual void OnElementFound(ICVar* pCVar) { UpdateCVar(pCVar, false); }

	//event handler: implements IConsoleVarSink
	virtual bool OnBeforeVarChange(ICVar* pVar, const char* sNewValue) { return true; }
	virtual void OnAfterVarChange(ICVar* pVar)                         { UpdateCVar(pVar, true); }
	virtual void OnVarUnregister(ICVar* pVar)                          { ReleaseCVar(pVar); }

protected:
	//initialize event handler
	void Init(size_t maxHistory);

	//get console instance
	IConsole* GetConsole() const { return m_system ? m_system->GetIConsole() : NULL; }

	//string comparison function to use
	static int StringCompare(const char* left, const char* right);

	//access the state lock
	CryCriticalSection* GetLock() { return &m_lock; }

public:
	//constructs an event listener for the specified engine instance
	CEngineListener(ISystem* system) : m_system(system), m_lines(0) {}

	//destructor
	~CEngineListener();

	//emits a CVar after it has been discovered or changed
	//this must be implemented by the deriving class
	virtual void EmitCVar(ICVar* pVar) const = 0;

	//emit a history line with an index (where 1 is the first entry in the log)
	//this must be implemented by the deriving class
	virtual void EmitLine(size_t index, const t_string& line) const = 0;

	//emits a CVar when it's destroyed
	//this must be implemented by the deriving class
	virtual void DestroyCVar(ICVar* pVar) const = 0;

	//call back the specified function with all known CVars
	void GetAllCVars(const std::function<void (ICVar*)>& function) const;

	//call back the specified function with all CVars of which the name contains the specified expression
	//the callbacks will be executed in alphabetic order
	//note: if you are interested in the case where the expression must be at the start of the name, use AutoComplete instead (it's faster)
	void GetFilteredCVars(const char* contains, const std::function<void (ICVar*)>& function) const;

	//call back the specified function with all known lines (limited by cache size)
	//this will call EmitLine in increasing order of index
	void GetAllLines(const std::function<void (size_t, const t_string&)>& function) const;

	//calls back the specified function with all known items of which the name contains the specified query
	//the callbacks will be executed in alphabetic order, the second argument may be NULL in case of a command (not a var) being returned
	void AutoComplete(const t_string& query, const std::function<void (const t_string&, ICVar*)>& function) const;

	//executes the given command
	void Execute(const t_string& command) const;

private:
	//the engine instance we're listening to
	ISystem* const m_system;

	//the history of log entries
	typedef boost::circular_buffer<t_string> t_history;
	t_history m_history;

	//the number of lines actually in the full log
	//note: not all of those are in the history
	size_t m_lines;

	//the critical section used to protect state
	CryCriticalSection m_lock;

	//collection of known CVars
	t_cVars m_cVars;

	//collection of known commands
	t_commands m_commands;
};

