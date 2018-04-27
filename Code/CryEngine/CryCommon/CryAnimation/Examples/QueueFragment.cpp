#include <CryAnimation/ICryMannequin.h>
#include <CryGame/IGameFramework.h>

// Creates a Mannequin action controller for the specified entity
void QueueFragment(IActionController& actionController, ICharacterInstance& character, IEntity& entity)
{
	// For the sake of this example, load the controller definition from <assets folder>/Animations/Mannequin/MyDatabase.adb
	const char* szAnimationDatabasePath = "Animations/Mannequin/MyDatabase.adb";
	// Assume that we have a scope context named "MyContext", created via the Mannequin Editor
	const char* szScopeContextName = "MyContext";
	// Automatically queue a fragment with name "Idle", assumed to have been created via the Mannequin Editor
	const char* szFragmentName = "Idle";

	// Query the Mannequin interface from the game framework
	IMannequin &mannequin = gEnv->pGameFramework->GetMannequinInterface();
	IAnimationDatabaseManager& databaseManager = mannequin.GetAnimationDatabaseManager();
	const SControllerDef& controllerDefinition = actionController.GetContext().controllerDef;

	// Load the animation database from disk
	const IAnimationDatabase *pAnimationDatabase = databaseManager.Load(szAnimationDatabasePath);

	// Activate the specified context for this entity, and assign it to the specified character instance
	const TagID contextId = controllerDefinition.m_scopeContexts.Find(szScopeContextName);
	actionController.SetScopeContext(contextId, entity, &character, pAnimationDatabase);

	// Query the specified fragment from the controller definition
	const TagID fragmentId = controllerDefinition.m_fragmentIDs.Find(szFragmentName);
	// Create the action with the specified fragment id
	// Note that this should be stored, since _smart_ptr will destroy the action at the end of the scope
	// Ideally this should be destroyed just before the action controller.
	_smart_ptr<IAction> pFragment = new TAction<SAnimationContext>(0, fragmentId);

	// Queue the fragment to start playing immediately on next update
	actionController.Queue(*pFragment.get());
}