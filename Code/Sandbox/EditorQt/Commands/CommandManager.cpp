// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "CommandManager.h"
#include "IconManager.h"
#include "Commands/QCommandAction.h"
#include "PolledKey.h"
#include "Qt/QtUtil.h"
#include "KeyboardShortcut.h"
#include "CustomCommand.h"
#include "CryIcon.h"

CEditorCommandManager::CEditorCommandManager()
	: m_bWarnDuplicate(true) {}

void CEditorCommandManager::RegisterAutoCommands()
{
	CAutoRegisterCommandHelper::RegisterAll();
	CAutoRegisterUiCommandHelper::RegisterAll();
}

CEditorCommandManager::~CEditorCommandManager()
{
	CommandTable::const_iterator iter = m_commands.begin(), end = m_commands.end();

	for (; iter != end; ++iter)
	{
		if (iter->second.deleter)
		{
			iter->second.deleter(iter->second.pCommand);
		}
		else
		{
			delete iter->second.pCommand;
		}
	}

	m_commands.clear();

	RemoveAllCustomCommands();
	CAutoRegisterCommandHelper::UnregisterAll();
}

string CEditorCommandManager::GetFullCommandName(const string& module, const string& name)
{
	string fullName = module;
	fullName += ".";
	fullName += name;
	return fullName;
}

bool CEditorCommandManager::AddCommand(CCommand* pCommand, TPfnDeleter deleter)
{
	assert(pCommand);

	string module = pCommand->GetModule();
	string name = pCommand->GetName();

	if (IsRegistered(module, name) && m_bWarnDuplicate)
	{
		string errMsg;

		errMsg.Format("Error: Command %s.%s already registered!", module.c_str(), name.c_str());
		Warning(errMsg);

		return false;
	}

	if (pCommand->CanBeUICommand())
	{
		CUiCommand* uicommand = static_cast<CUiCommand*>(pCommand);

		if (uicommand->GetUiInfo() == nullptr)
		{
			QCommandAction* action = nullptr;
			if (uicommand->IsPolledKey())
			{
				CPolledKeyCommand* polledCommand = static_cast<CPolledKeyCommand*>(pCommand);
				action = new QCommandAction(*polledCommand);
			}
			else
			{
				action = new QCommandAction(*uicommand);
			}
			uicommand->SetUiInfo(action);
		}
	}

	SCommandTableEntry entry;

	entry.pCommand = pCommand;
	entry.deleter = deleter;

	m_commands.insert(
	  CommandTable::value_type(GetFullCommandName(module, name),
	                           entry));

	signalChanged();
	return true;
}

bool CEditorCommandManager::AddCommandModule(CCommandModuleDescription* pCommand)
{
	m_commandModules[pCommand->GetName()] = pCommand;
	signalChanged();
	return true;
}

bool CEditorCommandManager::UnregisterCommand(const char* cmdFullName)
{
	CommandTable::iterator itr = m_commands.find(cmdFullName);

	if (itr != m_commands.end())
	{
		if (itr->second.deleter)
		{
			itr->second.deleter(itr->second.pCommand);
		}
		else
		{
			delete itr->second.pCommand;
		}

		m_commands.erase(itr);

		signalChanged();
		return true;
	}
	return false;
}

string CEditorCommandManager::Execute(const string& module, const string& name, const CCommand::CArgs& args)
{
	string fullName = GetFullCommandName(module, name);
	CommandTable::iterator iter = m_commands.find(fullName);

	if (iter != m_commands.end())
	{
		LogCommand(fullName, args);

		return ExecuteAndLogReturn(iter->second.pCommand, args);
	}
	else
	{
		string errMsg;

		errMsg.Format("Error: Trying to execute a unknown command, '%s'!", fullName);
		CryLogAlways(errMsg);
	}

	return "";
}

string CEditorCommandManager::Execute(const string& cmdLine)
{
	string cmdTxt, argsTxt;
	size_t argStart = cmdLine.find_first_of(' ');

	cmdTxt = cmdLine.substr(0, argStart);
	argsTxt = "";

	if (argStart != string::npos)
	{
		argsTxt = cmdLine.substr(argStart + 1);
		argsTxt.Trim();
	}

	CommandTable::iterator itr = m_commands.find(cmdTxt);

	if (itr != m_commands.end())
	{
		CCommand::CArgs argList;

		GetArgsFromString(argsTxt, argList);
		LogCommand(cmdTxt, argList);

		return ExecuteAndLogReturn(itr->second.pCommand, argList);
	}
	else
	{
		GetIEditorImpl()->GetSystem()->GetIConsole()->ExecuteString(cmdLine);
	}

	return "";
}

void CEditorCommandManager::GetCommandList(std::vector<string>& cmds) const
{
	cmds.clear();
	cmds.reserve(m_commands.size());
	CommandTable::const_iterator iter = m_commands.begin(), end = m_commands.end();

	for (; iter != end; ++iter)
	{
		cmds.push_back(iter->first);
	}

	std::sort(cmds.begin(), cmds.end());
}

void CEditorCommandManager::GetCommandList(std::vector<CCommand*>& cmds) const
{
	cmds.clear();
	cmds.reserve(m_commands.size());
	CommandTable::const_iterator iter = m_commands.begin(), end = m_commands.end();

	for (; iter != end; ++iter)
	{
		cmds.push_back(iter->second.pCommand);
	}

	GetCustomCommandList(cmds);

	std::sort(cmds.begin(), cmds.end());
}

const CCommandModuleDescription* CEditorCommandManager::GetCommandModuleDescription(const char* moduleName)
{
	auto it = m_commandModules.find(moduleName);
	if (it != m_commandModules.end())
	{
		return it->second;
	}
	else
	{
		CCommandModuleDescription* default = new CCommandModuleDescription(moduleName);
		m_commandModules[moduleName] = default;
		return default;
	}
}

CCommand* CEditorCommandManager::GetCommand(const char* cmdFullName) const
{
	auto it = m_commands.find(cmdFullName);
	if (it != m_commands.end())
	{
		return it->second.pCommand;
	}
	else
	{
		// go through the custom commands
		CCommand* pResult = nullptr;
		for (CCustomCommand* pCommand : m_CustomCommands)
		{
			if (pCommand->GetCommandString() == cmdFullName)
			{
				pResult = pCommand;
				break;
			}
		}
		return pResult;
	}
}

string CEditorCommandManager::AutoComplete(const string& substr) const
{
	std::vector<string> cmds;
	GetCommandList(cmds);

	// If substring is empty return first command.
	if (substr.empty() && (cmds.empty() == false))
		return cmds[0];

	size_t substrLen = substr.length();

	for (size_t i = 0; i < cmds.size(); ++i)
	{
		size_t cmdLen = cmds[i].length();

		if (cmdLen >= substrLen && !strncmp(cmds[i].c_str(), substr.c_str(), substrLen))
		{
			if (substrLen == cmdLen)
			{
				++i;

				if (i < cmds.size())
					return cmds[i];
				else
					return cmds[i - 1];
			}

			return cmds[i];
		}
	}

	// Not found
	return "";
}

bool CEditorCommandManager::IsRegistered(const char* module, const char* name) const
{
	string fullName = GetFullCommandName(module, name);
	CommandTable::const_iterator iter = m_commands.find(fullName);

	if (iter != m_commands.end())
		return true;
	else
		return false;
}

bool CEditorCommandManager::IsRegistered(const char* cmdLine_) const
{
	string cmdTxt, argsTxt, cmdLine(cmdLine_);
	size_t argStart = cmdLine.find_first_of(' ');
	cmdTxt = cmdLine.substr(0, argStart);
	CommandTable::const_iterator iter = m_commands.find(cmdTxt);

	if (iter != m_commands.end())
		return true;
	else
		return false;
}

// get any registered command actions. cmdFullName can also include arguments
QCommandAction* CEditorCommandManager::GetCommandAction(string command, const char* text) const
{
	bool bNoArguments = true;

	string cmd;
	// we need to get first substring
	size_t delim_pos = command.find_first_of(' ');
	// if we have substrings, then we have arguments
	bNoArguments = (delim_pos == string::npos);

	CommandTable::const_iterator it;

	if (!bNoArguments)
	{
		cmd = command.substr(0, delim_pos);
		it = m_commands.find(cmd);
	}
	else
	{
		it = m_commands.find(command);
	}

	if (it != m_commands.end())
	{
		if (bNoArguments && it->second.pCommand->CanBeUICommand())
		{
			return static_cast<QCommandAction*>(static_cast<CUiCommand*>(it->second.pCommand)->GetUiInfo());
		}
		else
		{
			return new QCommandAction(text ? text : "No Label - Replace Me!", command.c_str(), nullptr);
		}
	}

	CryLog("Command not found: %s", command.c_str());

	return nullptr;
}

void CEditorCommandManager::RegisterAction(QCommandAction* action, const string& command)
{
	auto it = m_commands.find(command);
	if (it != m_commands.end())
	{
		if (it->second.pCommand->CanBeUICommand())
		{
			CUiCommand* command = static_cast<CUiCommand*>(it->second.pCommand);

			QCommandAction* oldAction = static_cast<QCommandAction*>(command->GetUiInfo());
			QList<QKeySequence> oldDefaults = oldAction->GetDefaultShortcuts();
			if (!oldDefaults.isEmpty())
			{
				//merge default shortcuts declared from code (command declaration) and from data (ui file)
				QList<QKeySequence> defaults = action->GetDefaultShortcuts();

				for (QKeySequence& sequence : oldDefaults)
				{
					if (defaults.indexOf(sequence) == -1)
					{
						defaults.push_back(sequence);
					}
				}

				action->setShortcuts(defaults);
			}
			
			if (action->QAction::icon().isNull())
			{
				string iconStr = command->GetUiInfo()->icon;

				if (iconStr.empty())
				{
					QString icon = iconStr.c_str();
					action->setIcon(CryIcon(icon));
				}
			}

			command->SetUiInfo(action);

			signalChanged();
		}
	}
	else
	{
		// Check if the command has arguments as well. If it does don't take args into account when checking if it's been registered
		size_t idx = command.find(' ');
		if (idx != string::npos)
		{
			auto it = m_commands.find(command.substr(0, idx));
			if (it != m_commands.end())
				return;
		}
		CryWarning(VALIDATOR_MODULE_EDITOR, VALIDATOR_WARNING, "Command \'%s\' is not registered.", command.c_str());
	}
}

void CEditorCommandManager::ResetShortcut(const char* commandFullName)
{
	QCommandAction* action = GetCommandAction(commandFullName);
	if (action)
	{
		action->setShortcuts(action->GetDefaultShortcuts());
		signalChanged();
	}
}

void CEditorCommandManager::ResetAllShortcuts()
{
	for (auto it = m_commands.begin(); it != m_commands.end(); ++it)
	{
		if (it->second.pCommand->CanBeUICommand())
		{
			CUiCommand* uiCmd = static_cast<CUiCommand*>(it->second.pCommand);
			QCommandAction* action = static_cast<QCommandAction*>(uiCmd->GetUiInfo());

			action->setShortcuts(action->GetDefaultShortcuts());
		}
	}

	signalChanged();
}

void CEditorCommandManager::AddCustomCommand(CCustomCommand* pCommand)
{
	auto it = std::find(m_CustomCommands.begin(), m_CustomCommands.end(), pCommand);
	if (it == m_CustomCommands.end())
	{
		m_CustomCommands.push_back(pCommand);
		QCommandAction* action = new QCommandAction(*pCommand);
		pCommand->SetUiInfo(action);
		signalChanged();
	}
}

void CEditorCommandManager::RemoveCustomCommand(CCustomCommand* pCommand)
{
	auto it = std::find(m_CustomCommands.begin(), m_CustomCommands.end(), pCommand);
	if (it != m_CustomCommands.end())
	{
		m_CustomCommands.erase(it);
		signalChanged();
		// Fire signal before actually deleting (command model depends on this pointer being valid).
		// We could bypass the need for this by not tracking a pointer in the model and just tracking the command string
		delete pCommand;
	}
}

void CEditorCommandManager::RemoveAllCustomCommands()
{
	for (auto it = m_CustomCommands.begin(); it != m_CustomCommands.end(); ++it)
	{
		delete *it;
	}

	m_CustomCommands.clear();
}

void CEditorCommandManager::GetCustomCommandList(std::vector<CCommand*>& cmds) const
{
	//intentionally not cleaning cmds array
	cmds.insert(cmds.end(), m_CustomCommands.begin(), m_CustomCommands.end());
}

void CEditorCommandManager::LogCommand(const string& fullCmdName, const CCommand::CArgs& args) const
{
	string cmdLine = fullCmdName;

	for (int i = 0; i < args.GetArgCount(); ++i)
	{
		cmdLine += " ";
		bool bString = args.IsStringArg(i);

		if (bString)
			cmdLine += "'";

		cmdLine += args.GetArg(i);

		if (bString)
			cmdLine += "'";
	}

	CLogFile::WriteLine(cmdLine.c_str());

	CCommand* command = GetCommand(fullCmdName);
	if (command && !command->IsAvailableInScripting())
		return;

	/// If this same command is also available in the script system,
	/// log this to the script terminal, too.

	// First, recreate a command line to be compatible to the script system.
	cmdLine = fullCmdName;
	cmdLine += "(";

	for (int i = 0; i < args.GetArgCount(); ++i)
	{
		bool bString = args.IsStringArg(i);

		if (bString)
			cmdLine += "\"";

		cmdLine += args.GetArg(i);

		if (bString)
			cmdLine += "\"";

		if (i < args.GetArgCount() - 1)
			cmdLine += ",";
	}

	cmdLine += ")";
}

string CEditorCommandManager::ExecuteAndLogReturn(CCommand* pCommand, const CCommand::CArgs& args)
{
	string result = pCommand->Execute(args).c_str();
	string returnMsg;

	returnMsg.Format("Returned: %s", result);
	CLogFile::WriteLine(returnMsg.c_str());

	return result;
}

void CEditorCommandManager::GetArgsFromString(const string& argsTxt, CCommand::CArgs& argList)
{
	const char quoteSymbol = '\'';
	int curPos = 0;
	int prevPos = 0;
	string arg = argsTxt.Tokenize(" ", curPos);

	while (!arg.empty())
	{
		if (arg[0] == quoteSymbol) // A special consideration for a quoted string
		{
			if (arg.length() < 2 || arg[arg.length() - 1] != quoteSymbol)
			{
				size_t openingQuotePos = argsTxt.find(quoteSymbol, prevPos);
				size_t closingQuotePos = argsTxt.find(quoteSymbol, curPos);

				if (closingQuotePos != string::npos)
				{
					arg = argsTxt.substr(openingQuotePos + 1, closingQuotePos - openingQuotePos - 1);
					size_t nextArgPos = argsTxt.find(' ', closingQuotePos + 1);
					curPos = nextArgPos != string::npos ? nextArgPos + 1 : argsTxt.length();

					for (; curPos < argsTxt.length(); ++curPos)  // Skip spaces.
					{
						if (argsTxt[curPos] != ' ')
							break;
					}
				}
			}
			else
			{
				arg = arg.substr(1, arg.length() - 2);
			}
		}

		argList.Add(arg.c_str());
		prevPos = curPos;
		arg = argsTxt.Tokenize(" ", curPos);
	}
}

void CEditorCommandManager::SetUiDescription(const char* module, const char* name, const CUiCommand::UiInfo& info) const
{
	string fullCmdName = GetFullCommandName(module, name);
	assert(!fullCmdName.empty());
	if (!fullCmdName.empty())
	{
		QCommandAction* pAction = GetCommandAction(fullCmdName);
		assert(pAction && "Cannot find QCommandAction for UI description.");

		if (pAction)
		{
			*pAction = info;
		}
	}
}

QAction* CEditorCommandManager::GetAction(const char* cmdFullName, const char* text) const
{
	return (QAction*)GetCommandAction(cmdFullName, text);
}

void CEditorCommandManager::SetEditorUIActionsEnabled(bool bEnabled)
{
	CommandTable::const_iterator iter = m_commands.begin(), end = m_commands.end();

	for (; iter != end; ++iter)
	{
		CCommand* cmd = iter->second.pCommand;

		if (cmd->CanBeUICommand())
		{
			QCommandAction* action = GetCommandAction(cmd->GetCommandString());

			action->setEnabled(bEnabled);
		}
	}

	for (CCustomCommand* ccmd : m_CustomCommands)
	{
		if (ccmd->CanBeUICommand())
		{
			CUiCommand* uiCommand = static_cast<CUiCommand*>(ccmd);
			CUiCommand::UiInfo* info = uiCommand->GetUiInfo();
			if (!info)
				continue;

			QCommandAction* action = static_cast<QCommandAction*>(info);
			if (!action)
				continue;

			action->setEnabled(bEnabled);
		}
	}

	if (!bEnabled)
	{
		// shameless hack, keep this one action always enabled to allow resume of editor functionality
		QCommandAction* suspendAction = GetCommandAction("general.suspend_game_input");

		if (suspendAction)
		{
			suspendAction->setEnabled(true);
		}
	}
}

