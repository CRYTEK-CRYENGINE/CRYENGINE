#include <CryAction/IActionMapManager.h>
#include <CryGame/IGameFramework.h>

void ReBindActionInput()
{
	// Define the name of the action map in which our action resides
	const char* szMyActionMapName = "MyActionGroup";
	// Define the identifier of the action we are rebinding
	const ActionId myActionId = ActionId("MyAction");

	IActionMapManager* pActionMapManager = gEnv->pGameFramework->GetIActionMapManager();

	// Query the action map manager for our action map
	if (IActionMap* pActionMap = pActionMapManager->GetActionMap(szMyActionMapName))
	{
		// Specify the input that the action is currently triggered by
		const char* szCurrentInput = "enter";
		// Now specify the new input that the action should be triggered by
		const char* szNewInput = "space";

		// Rebind the action
		pActionMap->ReBindActionInput(myActionId, szCurrentInput, szNewInput);
	}
}

void ReBindActionInputWithDevice()
{
	// Define the name of the action map in which our action resides
	const char* szMyActionMapName = "MyActionGroup";
	// Define the identifier of the action we are rebinding
	const ActionId myActionId = ActionId("MyAction");

	IActionMapManager* pActionMapManager = gEnv->pGameFramework->GetIActionMapManager();

	// Query the action map manager for our action map
	if (IActionMap* pActionMap = pActionMapManager->GetActionMap(szMyActionMapName))
	{
		// Specify the device for which we want to rebind the input
		const EActionInputDevice inputDevice = eAID_KeyboardMouse;
		// Specify the device index, in case of multiple devices of the same type
		const int inputDeviceIndex = 0;
		// Now specify the new input that the action should be triggered by
		const char* szNewInput = "space";

		// Rebind the action
		pActionMap->ReBindActionInput(myActionId, szNewInput, inputDevice, inputDeviceIndex);
	}
}