#include "StdAfx.h"
#include "Console.h"

#include "MonoRuntime.h"

#include "Wrappers/MonoDomain.h"
#include "Wrappers/MonoClass.h"
#include "Wrappers/MonoString.h"

#include <CrySystem/ConsoleRegistration.h>

static void AddConsoleCommandFunction(ConsoleCommandFunc consoleCommandFunc, MonoInternals::MonoString* commandName, int nFlags, MonoInternals::MonoString* sHelp, bool isManaged)
{
	std::shared_ptr<CMonoString> pCommandName = CMonoDomain::CreateString(commandName);
	std::shared_ptr<CMonoString> pCommandHelp = CMonoDomain::CreateString(sHelp);

	ConsoleRegistrationHelper::AddCommand(pCommandName->GetString(), consoleCommandFunc, nFlags, pCommandHelp->GetString(), isManaged);
}

static void AddConsoleVariableString(ConsoleVarFunc consoleVariableFunc, MonoInternals::MonoString* variableName, MonoInternals::MonoString* variableValue, int nFlags, MonoInternals::MonoString* sHelp)
{
	std::shared_ptr<CMonoString> pConsoleVariableName = CMonoDomain::CreateString(variableName);
	std::shared_ptr<CMonoString> pConsoleVariableValue = CMonoDomain::CreateString(variableValue);
	std::shared_ptr<CMonoString> pConsoleVariableHelp = CMonoDomain::CreateString(sHelp);

	ConsoleRegistrationHelper::RegisterString(pConsoleVariableName->GetString(), pConsoleVariableValue->GetString(), nFlags, pConsoleVariableHelp->GetString(), consoleVariableFunc);
}

static void AddConsoleVariableInt64(ConsoleVarFunc consoleVariableFunc, MonoInternals::MonoString* variableName, int64 variableValue, int nFlags, MonoInternals::MonoString* sHelp)
{
	std::shared_ptr<CMonoString> pConsoleVariableName = CMonoDomain::CreateString(variableName);
	std::shared_ptr<CMonoString> pConsoleVariableHelp = CMonoDomain::CreateString(sHelp);

	ConsoleRegistrationHelper::RegisterInt64(pConsoleVariableName->GetString(), variableValue, nFlags, pConsoleVariableHelp->GetString(), consoleVariableFunc);
}

static void AddConsoleVariableInt(ConsoleVarFunc consoleVariableFunc, MonoInternals::MonoString* variableName, int variableValue, int nFlags, MonoInternals::MonoString* sHelp)
{
	std::shared_ptr<CMonoString> pConsoleVariableName = CMonoDomain::CreateString(variableName);
	std::shared_ptr<CMonoString> pConsoleVariableHelp = CMonoDomain::CreateString(sHelp);

	ConsoleRegistrationHelper::RegisterInt(pConsoleVariableName->GetString(), variableValue, nFlags, pConsoleVariableHelp->GetString(), consoleVariableFunc);
}

static void AddConsoleVariableFloat(ConsoleVarFunc consoleVariableFunc, MonoInternals::MonoString* variableName, float variableValue, int nFlags, MonoInternals::MonoString* sHelp)
{
	std::shared_ptr<CMonoString> pConsoleVariableName = CMonoDomain::CreateString(variableName);
	std::shared_ptr<CMonoString> pConsoleVariableHelp = CMonoDomain::CreateString(sHelp);

	ConsoleRegistrationHelper::RegisterFloat(pConsoleVariableName->GetString(), variableValue, nFlags, pConsoleVariableHelp->GetString(), consoleVariableFunc);
}

void CConsoleCommandInterface::RegisterFunctions(std::function<void(const void* pMethod, const char* szMethodName)> func)
{
	func(AddConsoleCommandFunction, "AddConsoleCommandFunction");
	func(AddConsoleVariableString, "AddConsoleVariableString");
	func(AddConsoleVariableInt64, "AddConsoleVariableInt64");
	func(AddConsoleVariableInt, "AddConsoleVariableInt");
	func(AddConsoleVariableFloat, "AddConsoleVariableFloat");
}

