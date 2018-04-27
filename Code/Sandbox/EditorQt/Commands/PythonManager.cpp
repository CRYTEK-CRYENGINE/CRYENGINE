// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.
#include <StdAfx.h>
#include "PythonManager.h"

#include "Util/BoostPythonHelpers.h"

static std::vector<SPythonModule> g_pythonModules;

void CEditorPythonManager::Init()
{
	PyScript::InitializePython();
}

void CEditorPythonManager::Deinit()
{
	PyScript::ShutdownPython();
}

void CEditorPythonManager::Execute(const char* command)
{
	PyScript::Execute(command);
}

float CEditorPythonManager::GetAsFloat(const char* variable)
{
	return PyScript::GetAsFloat(variable);
}

void CEditorPythonManager::RegisterPythonCommand(const SPythonCommand& command)
{
	for (SPythonModule& module : g_pythonModules)
	{
		if (module.m_name == command.m_module)
		{
			module.m_commands.push_back(command);
			return;
		}
	}

	//module not found
	g_pythonModules.push_back(SPythonModule(command.m_module));
	g_pythonModules.back().m_commands.push_back(command);
}

void CEditorPythonManager::RegisterPythonModule(const SPythonModule& module)
{
	SPythonModule* registeredModule = nullptr;

	for (SPythonModule& m : g_pythonModules)
	{
		if (m.m_name == module.m_name)
		{
			registeredModule = &m;
			break;
		}
	}

	if (!registeredModule)
	{
		m_pythonModules.push_back(module);
		registeredModule = &m_pythonModules.back();
	}

	assert(registeredModule->m_commands.size() != 0); // Register commands before module or module has no commands

	for (SPythonCommand& cmd : registeredModule->m_commands)
	{
		cmd.m_registerFunc();
	}

	string importStatement;
	// TODO for keeping back compatibility, doing auto import as "from sandbox import *". 
	// But when we get rid of python calls inside sandbox we should remove the import completely.
	importStatement.Format("from sandbox import %s", module.m_name);
	PyRun_SimpleString(importStatement.c_str());
}

