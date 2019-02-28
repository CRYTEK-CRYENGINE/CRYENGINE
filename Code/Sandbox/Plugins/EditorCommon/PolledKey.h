// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.
#pragma once

#include "ICommandManager.h"
#include "KeyboardShortcut.h"
#include "BoostPythonMacros.h"

#include "Command.h"

class CPolledKeyCommand : public CUiCommand
{
public:
	CPolledKeyCommand(bool& value, const string& module, const string& name,
	                  const string& description)
		: CUiCommand(module, name, CCommandDescription(description))
		, m_keyDown(value)
	{
	}

	~CPolledKeyCommand()
	{
		m_keyDown = false;
	}

	inline dll_string Execute(const CArgs& args)
	{
		assert(args.GetArgCount() == 1);

		bool p1;
		bool ok = FromString_(p1, args.GetArg(0).c_str());

		if (ok)
			m_keyDown = p1;
		else
			CryLogAlways("Cannot execute the command %s.%s(%s,%s)! Invalid argument type(s).",
			             m_module, m_name, args.GetArg(0).c_str(), args.GetArg(1).c_str());

		return "";
	}

	void         SetKeyDown(bool keyDown)     { m_keyDown = keyDown; }
	bool         GetKeyDown() const           { return m_keyDown; }

	virtual bool IsPolledKey() const override { return true; }

private:

	bool& m_keyDown;
};

#define REGISTER_POLLED_KEY(moduleName, keyName, description, shortcut)                                                               \
  namespace Private_CryCommand                                                                                                        \
  {                                                                                                                                   \
  static bool g_bPolledKey ## moduleName ## keyName = false;                                                                          \
  void SetPolledKey ## moduleName ## keyName(bool b) { g_bPolledKey ## moduleName ## keyName = b; }                                   \
  void RegisterPolledKeyCommand ## moduleName ## keyName()                                                                            \
  {                                                                                                                                   \
    ICommandManager* cmdMgr = GetIEditor()->GetICommandManager();                                                                     \
    cmdMgr->AddCommand(new CPolledKeyCommand(g_bPolledKey ## moduleName ## keyName, # moduleName, # keyName, description));           \
  }                                                                                                                                   \
  CAutoRegisterCommandHelper g_AutoRegPolledKeyCmdHelper ## moduleName ## keyName(RegisterPolledKeyCommand ## moduleName ## keyName); \
  }                                                                                                                                   \
  REGISTER_ONLY_PYTHON_COMMAND(Private_CryCommand::SetPolledKey ## moduleName ## keyName, moduleName, keyName, description);          \
  REGISTER_EDITOR_COMMAND_SHORTCUT(moduleName, keyName, shortcut);

#define GET_POLLED_KEY(moduleName, keyName) Private_CryCommand::g_bPolledKey ## moduleName ## keyName

