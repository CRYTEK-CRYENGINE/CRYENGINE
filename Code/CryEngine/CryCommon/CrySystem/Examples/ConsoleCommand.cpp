#include <CrySystem/ISystem.h>
#include <CrySystem/IConsole.h>

// The function that will be executed with the command
void MyCommand(IConsoleCmdArgs* pArgs)
{
	// The first argument is always the name of the command
	const char* szCommand = pArgs->GetArg(0);
	if (pArgs->GetArgCount() > 1)
	{
		// Get the first argument, if any
		const char* szFirstArgument = pArgs->GetArg(1);

		/* Handle the command, and its argument here */
	}
}

void RegisterCommand()
{
	// Specify bit flags that define the CCommand's behavior, see EVarFlags
	const int variableFlags = VF_NULL;

	// Register the command in the console
	REGISTER_COMMAND("myCommand", MyCommand, variableFlags, "Executes the MyCommand function!");
}

void UnregisterCommand()
{
	// Make sure to unregister the command once we are done
	gEnv->pConsole->RemoveCommand("myCommand");
}

void ExecuteCommand()
{
	// Do not silence execution of the command from logs
	const bool silent = false;
	// Execute the command immediately, instead of the next frame
	const bool deferExecution = false;

	gEnv->pConsole->ExecuteString("myCommand", silent, deferExecution);
}