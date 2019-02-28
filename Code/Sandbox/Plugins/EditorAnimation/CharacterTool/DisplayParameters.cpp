// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "stdafx.h"

#include "DisplayParameters.h"
#include "Serialization.h"
#include "Serialization/Decorators/EditorActionButton.h"

namespace CharacterTool
{

SERIALIZATION_ENUM_BEGIN(CharacterMovement, "Character Movement")
SERIALIZATION_ENUM(CHARACTER_MOVEMENT_INPLACE, "inplace", "In place (Only grid moves)")
SERIALIZATION_ENUM(CHARACTER_MOVEMENT_REPEATED, "repeated", "Repeated")
SERIALIZATION_ENUM(CHARACTER_MOVEMENT_CONTINUOUS, "continuous", "Continuous (Animation driven)")
SERIALIZATION_ENUM_END()

SERIALIZATION_ENUM_BEGIN(CompressionPreview, "Compression Preview")
SERIALIZATION_ENUM(COMPRESSION_PREVIEW_COMPRESSED, "compressed", "Preview Compressed Only")
SERIALIZATION_ENUM(COMPRESSION_PREVIEW_BOTH, "both", "Side by Side (Original and Compressed)")
SERIALIZATION_ENUM_END()

SERIALIZATION_ENUM_BEGIN_NESTED(DisplayPhysicsOptions, EDbgRenderProxy, "Debug Proxy")
SERIALIZATION_ENUM(DisplayPhysicsOptions::EDbgRenderProxy::DISABLED, "none", "Disabled")
SERIALIZATION_ENUM(DisplayPhysicsOptions::EDbgRenderProxy::SOLID, "solid", "Solid")
SERIALIZATION_ENUM(DisplayPhysicsOptions::EDbgRenderProxy::TRANSLUCENT, "trans", "Translucent")
SERIALIZATION_ENUM_END()

SERIALIZATION_ENUM_BEGIN_NESTED(DisplayPhysicsOptions, EDbgRenderLimits, "Debug Limits")
SERIALIZATION_ENUM(DisplayPhysicsOptions::EDbgRenderLimits::NONE, "none", "None")
SERIALIZATION_ENUM(DisplayPhysicsOptions::EDbgRenderLimits::ALL, "all", "All")
SERIALIZATION_ENUM(DisplayPhysicsOptions::EDbgRenderLimits::SELECTED, "sel", "Selected Bone")
SERIALIZATION_ENUM_END()


void DisplayAnimationOptions::Serialize(Serialization::IArchive& ar)
{
	auto oldMovement = movement;
	ar(movement, "movement", "Movement");
	if (movement != oldMovement)
		resetGrid = true;
	ar.doc("In Place: Motion is extracted and moves the grid backwards, giving the illusion the character moves. This is useful because far away from the origin precision errors will start appearing; or if you don't want to loose the character.\nRepeated: Play the animation as is, on the spot. Motion is not extracted.\nContinuous: Motion is extracted and moves the character in the world.");

	ar(compressionPreview, "compressionPreview", "Compression");

	ar(animationEventGizmos, "animationEventGizmos", "Animation Event Gizmos");

	ar(showTrail, "showTrail", movement == CHARACTER_MOVEMENT_CONTINUOUS ? "Trail" : 0);
	ar.doc("A trail of blue dots will show the last positions of the character (only usable when characters move in the world)");

	ar(showLocator, "showLocator", movement != CHARACTER_MOVEMENT_REPEATED ? "Locomotion Locator" : 0);
	ar.doc("Show the locomotion locator, as well as the current speed/turnspeed/traveldir/slope.\nThe red sphere shows the position, the green arrow shows the orientation, and the yellow arrow the velocity.\nThis option does not appear when displaying Repeated motion as there is no motion extraction in that mode.");

	ar(showDccToolOrigin, "showDccToolOrigin", "DCC Tool Origin");
	ar.doc("When playing an animation, show where the origin of the DCC tool was during export");

	ar(Serialization::ActionButton(
	     [this]
		{
			resetCharacter = true;
	  }
	     ), "resetCharacter", "Reset Character");
	ar.doc("Stops all animations and moves the character back to the origin.");
}

void DisplayRenderingOptions::Serialize(Serialization::IArchive& ar)
{
	ar(showEdges, "showEdges", "Edges");
}

void DisplaySkeletonOptions::Serialize(Serialization::IArchive& ar)
{
	ar(jointFilter, "jointFilter", "<Joint Filter");
	ar(showJoints, "showJoints", "Joints");
	ar(showJointNames, "showJointNames", "Joint Names");
	ar(yasli::Range(showJointNamesFontSize, 1.0f, 3.0f), "showJointNamesFontSize", "Joint Names Font Size");
	ar(showSkeletonBoundingBox, "showSkeletonBoundingBox", "Bounding Box");
}

void DisplaySecondaryAnimationOptions::Serialize(Serialization::IArchive& ar)
{
	ar(showDynamicProxies, "showDynamicProxies", "Dynamic Proxies");
	ar(showAuxiliaryProxies, "showAuxiliaryProxies", "Auxiliary Proxies");
	ar(showClothProxies, "showClothProxies", "Cloth Proxies");
#if INCLUDE_SECONDARY_ANIMATION_RAGDOLL
	ar(showRagdollProxies, "showRagdollProxies", "Ragdoll Proxies");
#endif
}

void DisplayPhysicsOptions::Serialize(Serialization::IArchive& ar)
{
	ar(showPhysicalProxies, "showPhysicalProxies", "Physical Proxies");
	ar(showRagdollJointLimits, "showRagdollJointLimits", "Ragdoll Joint Limits");
}

void DisplayFollowJointOptions::Serialize(Serialization::IArchive& ar)
{
	ar(JointName(jointName), "jointName", "Joint");
	ar(lockAlign, "lockAlign", "Align");
	ar(usePosition, "usePosition", "Position");
	ar(useRotation, "useRotation", "Orientation");
}

void DisplayOptions::Serialize(Serialization::IArchive& ar)
{
	ar(attachmentGizmos, "attachmentGizmos", "Attachment Gizmos");
	ar(poseModifierGizmos, "poseModifierGizmos", "Pose Modifier Gizmos");

	ar(animation, "animation", "+Animation");

	if (ar.openBlock("rendering", "Rendering"))
	{
		rendering.Serialize(ar);
		viewport.rendering.Serialize(ar);

		ar.closeBlock();
	}

	ar(skeleton, "skeleton", "Skeleton");

	if (ar.openBlock("camera", "Camera"))
	{
		viewport.camera.Serialize(ar);

		auto oldFollowJoint = followJoint.jointName;
		ar(followJoint, "followJoint", "+Follow Joint");
		if (followJoint.jointName != oldFollowJoint)
			animation.updateCameraTarget = true;
		ar.closeBlock();
	}

	ar(secondaryAnimation, "secondaryAnimation", "Secondary Animation");
	ar(physics, "physics", "Physics");
	ar(viewport.grid, "grid", "Grid");
	ar(viewport.lighting, "lighting", "Lighting");
	ar(viewport.background, "background", "Background");
}

}

