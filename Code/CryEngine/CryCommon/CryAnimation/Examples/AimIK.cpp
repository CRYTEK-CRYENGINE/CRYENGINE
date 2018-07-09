#include <CryAnimation/ICryAnimation.h>

// Example for how to make the character aim at a specific position
void SetAimInverseKinematicsTarget(ICharacterInstance& character, const Vec3& targetWorldPosition)
{
	// Query the pose blender aim interface
	if (IAnimationPoseBlenderDir* pPoseBlenderAim = character.GetISkeletonPose()->GetIPoseBlenderAim())
	{
		// Enable the Aim-IK
		pPoseBlenderAim->SetState(true);
		// Make Aim-IK operate on the second animation layer
		pPoseBlenderAim->SetLayer(1);
		// Indicate the position we want to aim at
		pPoseBlenderAim->SetTarget(targetWorldPosition);
	}
}