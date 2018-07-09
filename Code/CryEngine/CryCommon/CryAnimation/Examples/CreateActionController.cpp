#include <CryAnimation/ICryMannequin.h>
#include <CryGame/IGameFramework.h>

// Creates a Mannequin action controller for the specified entity
void CreateActionController(IEntity* pEntity)
{
	// For the sake of this example, load the controller definition from <assets folder>/Animations/Mannequin/MyController.xml
	const char* szControllerDefinitionPath = "Animations/Mannequin/MyController.xml";

	// Query the Mannequin interface from the game framework
	IMannequin &mannequin = gEnv->pGameFramework->GetMannequinInterface();
	IAnimationDatabaseManager& databaseManager = mannequin.GetAnimationDatabaseManager();

	// Load the controller definition from disk
	const SControllerDef *pControllerDefinition = databaseManager.LoadControllerDef(szControllerDefinitionPath);
	if (pControllerDefinition != nullptr)
	{
		// Create an animation context using the controller definition
		SAnimationContext animationContext(*pControllerDefinition);
		// Now create an action controller for the entity
		IActionController* pActionController = mannequin.CreateActionController(pEntity, animationContext);

		/* 
			pActionController can now be used, until it is released manually (see below)
			Most importantly, pActionController->Update will need to be called every frame for fragments to be played
		*/

		// The action controller has to be released when we are done
		// Typically this would be when the character is being destroyed
		pActionController->Release();
	}
}