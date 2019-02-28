#include <CryAnimation/ICryAnimation.h>

// Basic example of how the absolute joint orientation can be retrieved from a character's current pose
void GetJointOrientation(IEntity& entity, int characterSlotId)
{
	if(ICharacterInstance* pCharacter = entity.GetCharacter(characterSlotId))
	{
		// Get the default skeleton, a read-only construct in which we can get joint identifiers from names
		const IDefaultSkeleton& defaultSkeleton = pCharacter->GetIDefaultSkeleton();
		// Get the internal identifier of our joint, labeled in the DCC (Max / Maya for example)
		const int32 jointId = defaultSkeleton.GetJointIDByName("Bip 01 Head");

		const QuatT& jointOrientationModelSpace = pCharacter->GetISkeletonPose()->GetAbsJointByID(jointId);
		
		// Calculate the orientation of the joint in world coordinates
		const QuatT jointOrientation = QuatT(entity.GetSlotWorldTM(characterSlotId)) * jointOrientationModelSpace;
		// jointOrientation.q contains a quaternion with the world-space rotation of the joint
		const Quat& jointRotation = jointOrientation.q;
		// jointOrientation.t contains a vector with the world-space position of the joint
		const Vec3& jointPosition = jointOrientation.t;
	}
}