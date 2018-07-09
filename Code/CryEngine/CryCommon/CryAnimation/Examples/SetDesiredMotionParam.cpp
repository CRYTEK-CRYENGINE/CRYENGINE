#include <CryAnimation/ICryAnimation.h>


void SetBlendSpaceParameter(ICharacterInstance& character)
{
	// Specify which parameter to override, for the sake of this example we opt for travel speed
	// These parameters can also be seen in the Character Tool
	const EMotionParamID parameterId = eMotionParamID_TravelSpeed;
	// Specify the value we want to set the parameter to
	const float newValue = 5.f;

	// Now set the motion parameter, affecting compatible blend spaces starting with the next frame.
	character.GetISkeletonAnim()->SetDesiredMotionParam(parameterId, newValue, 0.f);
}