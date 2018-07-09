#include <CryAction/IActionMapManager.h>
#include <CryGame/IGameFramework.h>

// Example of how action maps can be registered, and how to receive callbacks when their actions are triggered
class CMyActionListener final : public IActionListener
{
	// Define the name of the action map in which our action will reside
	const char* m_szMyActionMapName = "MyActionGroup";
	// Define the identifier of the action we are registering, this should be a constant over the runtime of the application
	const ActionId m_myActionId = ActionId("MyAction");

	virtual ~CMyActionListener()
	{
		// Make sure to remove the listener when we are done
		gEnv->pGameFramework->GetIActionMapManager()->RemoveExtraActionListener(this, m_szMyActionMapName);
	}

	void RegisterAction()
	{
		IActionMapManager* pActionMapManager = gEnv->pGameFramework->GetIActionMapManager();

		// Create the action map in which our action will reside
		IActionMap* pActionMap = pActionMapManager->CreateActionMap(m_szMyActionMapName);
		// Register a listener to receive callbacks when actions in our action map are triggered
		pActionMapManager->AddExtraActionListener(this, m_szMyActionMapName);

		// Register the action in the group
		pActionMap->CreateAction(m_myActionId);

		// Now define the first input with which the user can trigger the input
		SActionInput input;
		// Define that this input is triggered with the keyboard or mouse
		input.inputDevice = eAID_KeyboardMouse;
		// Set the input to "enter"
		// defaultInput is used in case of future rebinding by the end-user at runtime
		input.input = input.defaultInput = "enter";
		// Determine the activation modes we want to listen for
		input.activationMode = eIS_Pressed | eIS_Released;

		// Now bind the input to the action we created earlier
		pActionMap->AddAndBindActionInput(m_myActionId, input);

		// Make sure that the action map is enabled by default
		// This function can also be used to toggle action maps at runtime, for example to disable vehicle inputs when exiting
		pActionMap->Enable(true);
	}

	// Called when any action is triggered
	virtual void OnAction(const ActionId &actionId, int activationMode, float value) override
	{
		const bool isInputPressed = (activationMode & eIS_Pressed) != 0;
		const bool isInputReleased = (activationMode & eIS_Released) != 0;

		// Check if the triggered action
		if (actionId == m_myActionId)
		{
			if (isInputPressed)
			{
				CryLogAlways("Action pressed!");
			}
			else if (isInputReleased)
			{
				CryLogAlways("Action released!");
			}
		}
	}
};