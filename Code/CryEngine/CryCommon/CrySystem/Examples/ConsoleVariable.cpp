#include <CrySystem/ISystem.h>
#include <CrySystem/IConsole.h>

// Minimal example of how a console variable can be implemented and used
// In this case we create a system, and a variable that determines whether or not the system should be currently updated
class CMySystem
{
	CMySystem()
	{
		// Specify bit flags that define the CVar's behavior, see EVarFlags
		const int variableFlags = VF_NULL;

		// Register the "g_mySystem_enable" command into the console, allowing modification by designers and code at run-time
		REGISTER_CVAR2("g_mySystem_enable", &m_enabled, m_enabled, variableFlags, "Determines whether or not the system should run");

		// Retrieve an ICVar instance for the variable from the console
		ICVar* pCVar = gEnv->pConsole->GetCVar("g_mySystem_enable");
		/* If necessary, utilize pCVar here */
	}

	~CMySystem()
	{
		// Make sure to unregister the variable once we go out of scope
		gEnv->pConsole->UnregisterVariable("g_mySystem_enable");
	}

	void Update()
	{
		// If the system was disabled via the console, skip update
		if (m_enabled == 0)
		{
			return;
		}

		/* Update system here */
	}

protected:
	// Store the variable locally here, the console takes a reference that will update the variable in place
	// We start off by setting this value to 1.
	int m_enabled = 1;
};