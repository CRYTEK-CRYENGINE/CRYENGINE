#include <CryAnimation/ICryAnimation.h>

void StartAnimation(ICharacterInstance* pCharacter)
{
	// Start an animation named "MyAnimation" (it should be visible with the same name in the Character Tool)
	const char* szAnimationName = "MyAnimation";
	// Loop the animation at normal speed
	CryCharAnimationParams animationParams;
	animationParams.m_nFlags = CA_LOOPED;
	// Start playing back the animation
	pCharacter->GetISkeletonAnim()->StartAnimation(szAnimationName, animationParams);
}