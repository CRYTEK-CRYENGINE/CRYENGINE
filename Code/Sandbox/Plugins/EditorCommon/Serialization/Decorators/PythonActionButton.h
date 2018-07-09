// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include "EditorActionButton.h"
#include <Util/BoostPythonHelpers.h>

namespace Serialization
{
struct CommandActionButton : public StdFunctionActionButton
{
	explicit CommandActionButton(string command, const char* icon_ = "") : StdFunctionActionButton(std::bind(&CommandActionButton::ExecuteCommand, command), icon_) {}
	static void ExecuteCommand(string command)
	{
		GetIEditor()->GetICommandManager()->Execute(command.c_str());
	}
};
inline bool Serialize(Serialization::IArchive& ar, Serialization::CommandActionButton& button, const char* name, const char* label)
{
	if (ar.isEdit())
		return ar(Serialization::SStruct::forEdit(button), name, label);
	else
		return false;
}

struct PythonActionButton : public StdFunctionActionButton
{
	explicit PythonActionButton(string pythonCmd, const char* icon_ = "") : StdFunctionActionButton(std::bind(&PythonActionButton::ExecutePython, pythonCmd), icon_) {}
	static void ExecutePython(string cmd)
	{
		PyScript::Execute(cmd.c_str());
	}
};
inline bool Serialize(Serialization::IArchive& ar, Serialization::PythonActionButton& button, const char* name, const char* label)
{
	if (ar.isEdit())
		return ar(Serialization::SStruct::forEdit(button), name, label);
	else
		return false;
}
}

