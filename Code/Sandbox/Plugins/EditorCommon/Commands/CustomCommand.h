// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.
#pragma once
#include "Command.h"

class EDITOR_COMMON_API CCustomCommand : public CCommand0
{
public:
	CCustomCommand(const string& name, const string& command);

	void Register();
	void Unregister();

	// Only want to be able to set name on custom commands
	void           SetName(const char* name);
	void           SetCommandString(const char* commandStr);
	virtual bool   CanBeUICommand() const override { return GetUiInfo() != nullptr; }

	virtual bool   IsCustomCommand() const override  { return true; }
	virtual string GetCommandString() const override { return m_command; }

private:
	string m_command;
};
