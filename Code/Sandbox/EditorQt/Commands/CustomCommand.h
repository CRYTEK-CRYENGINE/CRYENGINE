// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.
#pragma once

class ICommandManager;

class CCustomCommand : public CCommand0
{
public:
	CCustomCommand(const string& name, const string& command);

	void Register();
	void Unregister();

	// Only want to be able to set name on custom commands
	void           SetName(const char* name);
	void           SetCommandString(const char* commandStr);

	virtual bool   IsCustomCommand() const override  { return true; }
	virtual string GetCommandString() const override { return m_command; }

private:

	string m_command;
};

