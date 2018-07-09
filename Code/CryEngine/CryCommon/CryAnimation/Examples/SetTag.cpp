#include <CryAnimation/ICryMannequin.h>
#include <CryGame/IGameFramework.h>

// Sets the state of a specified tag for the action controller
void SetTag(IActionController& actionController)
{
	// Name of the tag that we want to set, for the sake of our example we named it "Rotate"
	const char* szTagName = "Rotate";
	// Whether or not the tag should be enabled
	const bool isTagEnabled = true;
	// Cache the tag state for the action controller
	CTagState& tagState = actionController.GetContext().state;

	// Find the tag, ideally this should be cached to avoid run-time lookups
	TagID tagId = tagState.GetDef().Find(szTagName);

	// Now set the specified tag to the desired state (isTagEnabled)
	tagState.Set(tagId, isTagEnabled);
}