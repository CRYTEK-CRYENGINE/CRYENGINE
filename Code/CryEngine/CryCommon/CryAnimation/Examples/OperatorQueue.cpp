#include <CryAnimation/ICryAnimation.h>
#include <CryExtension/CryCreateClassInstance.h>

// Example of using operator queues to offset the positioning of one specific joint in a character
void OffsetJointPosition(ICharacterInstance& character)
{
	// Start by creating an instance of the operator queue
	// Note that this can be cached, and should not be re-created each frame
	std::shared_ptr<IAnimationOperatorQueue> pOperatorQueue;
	CryCreateClassInstanceForInterface(cryiidof<IAnimationOperatorQueue>(), pOperatorQueue);

	// For the sake of this example we will assume that our character's skeleton has a joint titled "Bip01 L Hand"
	const char* szJointName = "Bip01 L Hand";
	// Look up the identifier of the joint we want to manipulate
	// This id can be cached, to spare the run-time lookup
	const int jointId = character.GetIDefaultSkeleton().GetJointIDByName(szJointName);

	// Start per-frame actions, should be executed in an update loop
	// Offset the joint upwards by 0.1m
	pOperatorQueue->PushPosition(jointId, IAnimationOperatorQueue::eOp_Additive, Vec3(0, 0, 0.1f));
	// Now push the operator queue into the character for processing next frame
	character.GetISkeletonAnim()->PushPoseModifier(0, pOperatorQueue);
}