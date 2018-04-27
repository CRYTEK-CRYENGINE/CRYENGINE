// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.
#include <StdAfx.h>
#include "CustomCommand.h"

#include "CommandManager.h"
#include "CommandModuleDescription.h"
#include "ICommandManager.h"

CCustomCommand::CCustomCommand(const string& name, const string& command)
	: CCommand0("custom", name, CCommandDescription(""), Functor0())
	, m_command(command)
{
}

void CCustomCommand::Register()
{
	GetIEditorImpl()->GetCommandManager()->AddCustomCommand(this);
}

void CCustomCommand::Unregister()
{
	GetIEditorImpl()->GetCommandManager()->RemoveCustomCommand(this);
}

void CCustomCommand::SetName(const char* name)
{
	m_name = name;
}

void CCustomCommand::SetCommandString(const char* commandStr)
{
	m_command = commandStr;
}

