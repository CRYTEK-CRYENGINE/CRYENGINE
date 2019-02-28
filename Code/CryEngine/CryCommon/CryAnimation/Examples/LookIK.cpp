#include <CryAnimation/ICryAnimation.h>

// Example for how to make the character look at a specific position
void SetLookInverseKinematicsTarget(ICharacterInstance& character, const Vec3& targetWorldPosition)
{
	// Query the pose blender look interface
	if (IAnimationPoseBlenderDir* pPoseBlenderLook = character.GetISkeletonPose()->GetIPoseBlenderLook())
	{
		// Enable the Look-IK
		pPoseBlenderLook->SetState(true);
		// Make Look-IK operate on the second animation layer
		pPoseBlenderLook->SetLayer(1);
		// Indicate the position we want to look at
		pPoseBlenderLook->SetTarget(targetWorldPosition);
	}
}