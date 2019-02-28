// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "stdafx.h"
#include "PoseModifierDesc.h"

#include "PoseModifier.h"
#include "PoseModifierHelper.h"
#include "../Skeleton.h"
#include <CrySerialization/Forward.h>
#include "../Serialization/SerializationCommon.h"

namespace PoseModifier {
struct SNode;
}

#define AR_OLD_NEW(ar, value, oldName, newName, label) { if (ar.isInput()) { ar(value, oldName, label); } ar(value, newName, label); }

namespace PoseModifier
{

/*
   SNode
 */

bool SNode::IsSet() const
{
	return name.length() != 0;
}

int SNode::ResolveJointIndex(const CDefaultSkeleton& skeleton) const
{
	int index = skeleton.GetJointIDByCRC32(crc32);
	if (index > -1)
		return index;
	return -1;
}

struct SSerializableNode : public PoseModifier::SNode
{
	void Serialize(Serialization::IArchive& ar)
	{
		ar(Serialization::JointName(name), "name", "^");
		if (ar.isInput())
		{
			if (name.length())
				crc32 = CCrc32::ComputeLowercase(name.c_str());
			else
				crc32 = 0;
		}
	}
};

bool Serialize(Serialization::IArchive& ar, PoseModifier::SNode& value, const char* name, const char* label)
{
	return ar(static_cast<SSerializableNode&>(value), name, label);
}

void SConstraintPointNodeDesc::Serialize(Serialization::IArchive& ar)
{
	using Serialization::Range;

	ar(node, "node", "^");
	ar(Serialization::LocalToJoint(localOffset, node.name), "localOffset", "Local Offset");
	ar(Serialization::LocalToJointCharacterRotation(worldOffset, node.name), "worldOffset", "World Offset");
}

void Serialize(Serialization::IArchive& ar, PoseModifier::SConstraintPointDesc& value)
{
	using Serialization::Range;

	ar(value.node, "drivenNode", "Driven Node");
	if (value.node.name.empty() && ar.isEdit())
		ar.warning(value.node.name, "Driven Node is not specified.");

	ar(value.point, "point", "+Point");

	if (value.point.node.name.empty() && ar.isEdit())
		ar.warning(value.point, "Point Node is not specified.");

	if (ar.openBlock("Weight", "Weight"))
	{
		ar(Range(value.weight, 0.0f, 1.0f), "weight", "^");
		ar(value.weightNode, "weightNode", "^");
		ar.closeBlock();
	}

	if (ar.isInput())
	{
		value.weight = clamp_tpl(value.weight, 0.0f, 1.0f);
	}
}

void Serialize(Serialization::IArchive& ar, PoseModifier::SConstraintLineDesc& value)
{
	using Serialization::Range;

	ar(value.node, "drivenNode", "Driven Node");
	if (value.node.name.empty() && ar.isEdit())
		ar.warning(value.node.name, "Driven Node is not specified.");

	if (!ar(value.startPoint, "startPoint", "Start"))
	{
		ar(value.startPoint.node, "originNode");
		ar(value.startPoint.localOffset, "originOffset");
	}

	if (value.startPoint.node.name.empty() && ar.isEdit())
		ar.warning(value.startPoint, "Start Node is not specified.");

	if (!ar(value.endPoint, "endPoint", "End"))
	{
		ar(value.endPoint.node, "targetNode");
		ar(value.endPoint.localOffset, "targetOffset");
	}

	if (ar.openBlock("Weight", "Weight"))
	{
		ar(value.weightNode, "weightNode", "^");
		ar(Range(value.weight, 0.0f, 1.0f), "weight", "^");
		ar.closeBlock();
	}

	if (ar.isInput())
	{
		value.weight = clamp_tpl(value.weight, 0.0f, 1.0f);
	}
}

void Serialize(Serialization::IArchive& ar, PoseModifier::SConstraintAimDesc& value)
{
	using Serialization::Range;

	static const uint VERSION = 1;
	uint version = ar.isInput() ? 0 : VERSION;
	ar(version, "version", NULL);

	ar(value.node, "drivenNode", "Driven Node");
	ar(value.aimVector, "aimVector", "Aim Vector");
	ar(value.upVector, "upVector", "Up Vector");

	ar(value.targetNode, "targetNode", "Target Node");
	ar(Serialization::LocalToJoint(value.targetOffset, value.targetNode.name), "targetOffset", "Target Offset");

	ar(value.upNode, "upNode", "Up Node");
	ar(Serialization::LocalToJoint(value.upOffset, value.upNode.name), "upOffset", "Up Offset");

	ar(value.weightNode, "weightNode", "Weight Node");
	ar(Range(value.weight, 0.0f, 1.0f), "weight", "Weight");

	if (ar.isInput())
	{
		value.aimVector.NormalizeSafe(Vec3(1.0f, 0.0f, 0.0f));
		value.upVector.NormalizeSafe(Vec3(0.0f, 1.0f, 0.0f));

		if (fabsf(value.aimVector * value.upVector) > 0.9999f)
			value.upVector = value.aimVector.GetOrthogonal().normalize();
		Vec3 sideVector = value.aimVector.Cross(value.upVector).normalize();
		value.upVector = sideVector.Cross(value.aimVector).normalize();
		Quat frame = Quat(Matrix33(value.aimVector, value.upVector, sideVector));
		value.aimVector = frame.GetColumn0();
		value.upVector = frame.GetColumn1();

		switch (version)
		{
		case 0:
			value.aimVector = frame.GetInverted().GetColumn0();
			value.upVector = frame.GetInverted().GetColumn1();
			break;
		}
	}

	if (ar.isEdit())
	{
		if (value.node.name.empty())
			ar.warning(value.node.name, "Driven Node is not specified.");
		if (value.targetNode.name.empty())
			ar.warning(value.targetNode.name, "Target Node is not specified.");
		if (value.upNode.name.empty())
			ar.warning(value.upNode.name, "Up Node is not specified.");
	}
}

void Serialize(Serialization::IArchive& ar, PoseModifier::SDrivenTwistDesc& value)
{
	using Serialization::Range;

	ar(value.sourceNode, "sourceNode", "Source Node");
	ar(value.targetNode, "targetNode", "Target Node");

	ar(value.targetVector, "targetVector", "Target Vector");
	ar(Range(value.weight, 0.0f, 1.0f), "weight", "Weight");

	if (ar.isInput())
	{
		value.targetVector.NormalizeSafe(Vec3(1.0f, 0.0f, 0.0f));
		value.weight = clamp_tpl(value.weight, 0.0f, 1.0f);
	}

	if (ar.isEdit())
	{
		if (value.sourceNode.name.empty())
			ar.warning(value.sourceNode.name, "Source Node is not specified.");
		if (value.targetNode.name.empty())
			ar.warning(value.targetNode.name, "Target Node is not specified.");
	}
}

void Serialize(Serialization::IArchive& ar, PoseModifier::SIk2SegmentsDesc& value)
{
	using Serialization::Range;

	ar(value.rootNode, "rootNode", "Root Node");
	ar(value.linkNode, "linkNode", "Link Node");
	ar(value.endNode, "endNode", "End Node");
	ar(Serialization::LocalToJoint(value.endOffset, value.endNode.name), "endOffset", "End Offset");

	ar(value.targetNode, "targetNode", "Target Node");
	ar(Serialization::LocalToJoint(value.targetOffset, value.targetNode.name), "targetOffset", "Target Offset");

	ar(value.targetWeightNode, "targetWeightNode", "Target Weight Node");
	ar(Range(value.targetWeight, 0.0f, 1.0f), "targetWeight", "Target Weight");

	if (ar.isInput())
	{
		value.targetWeight = clamp_tpl(value.targetWeight, 0.0f, 1.0f);
	}

	if (ar.isEdit())
	{
		if (value.rootNode.name.empty())
			ar.warning(value.rootNode.name, "Root Node is not specified.");
		if (value.linkNode.name.empty())
			ar.warning(value.linkNode.name, "Link Node is not specified.");
	}
}

void Serialize(Serialization::IArchive& ar, PoseModifier::SIkCcdDesc& value)
{
	using Serialization::Range;

	ar(value.rootNode, "rootNode", "Root Node");
	ar(value.endNode, "endNode", "End Node");
	ar(Serialization::LocalToJoint(value.endOffset, value.endNode.name), "endOffset", "End Offset");

	ar(value.targetNode, "targetNode", "Target Node");
	ar(Serialization::LocalToJoint(value.targetOffset, value.targetNode.name), "targetOffset", "Target Offset");

	ar(value.weightNode, "weightNode", "Weight Node");
	ar(Range(value.weight, 0.0f, 1.0f), "weight", "Weight");

	ar(Range(value.iterations, uint(1), uint(100)), "iterations", "Iterations");
	ar(Range(value.stepSize, 0.0f, 5.0f), "stepSize", "Step Size");
	ar(Range(value.threshold, 0.0f, 5.0f), "threshold", "Threshold");

	if (ar.isInput())
	{
		value.weight = clamp_tpl(value.weight, 0.0f, 1.0f);
	}

	if (ar.isEdit())
	{
		if (value.rootNode.name.empty())
			ar.warning(value.rootNode.name, "Root Node is not specified.");
		if (value.endNode.name.empty())
			ar.warning(value.endNode.name, "End Node is not specified.");
	}
}

void Serialize(Serialization::IArchive& ar, PoseModifier::SDynamicsSpringDesc& value)
{
	ar(value.node, "drivenNode", "Driven Node");

	ar(value.length, "length", "Length");
	ar(value.stiffness, "stiffness", "Stiffness");
	ar(value.damping, "damping", "Damping");
	ar(value.gravity, "gravity", "Gravity");

	if (ar.isEdit())
	{
		bool bLimitPlaneXPositive = (value.flags & PoseModifier::SDynamicsSpringDesc::eFlag_LimitPlaneXPositive) == PoseModifier::SDynamicsSpringDesc::eFlag_LimitPlaneXPositive;
		bool bLimitPlaneXNegative = (value.flags & PoseModifier::SDynamicsSpringDesc::eFlag_LimitPlaneXNegative) == PoseModifier::SDynamicsSpringDesc::eFlag_LimitPlaneXNegative;
		bool bLimitPlaneYPositive = (value.flags & PoseModifier::SDynamicsSpringDesc::eFlag_LimitPlaneYPositive) == PoseModifier::SDynamicsSpringDesc::eFlag_LimitPlaneYPositive;
		bool bLimitPlaneYNegative = (value.flags & PoseModifier::SDynamicsSpringDesc::eFlag_LimitPlaneYNegative) == PoseModifier::SDynamicsSpringDesc::eFlag_LimitPlaneYNegative;
		bool bLimitPlaneZPositive = (value.flags & PoseModifier::SDynamicsSpringDesc::eFlag_LimitPlaneZPositive) == PoseModifier::SDynamicsSpringDesc::eFlag_LimitPlaneZPositive;
		bool bLimitPlaneZNegative = (value.flags & PoseModifier::SDynamicsSpringDesc::eFlag_LimitPlaneZNegative) == PoseModifier::SDynamicsSpringDesc::eFlag_LimitPlaneZNegative;

		ar(bLimitPlaneXPositive, "limitPlaneXPositive", "Limit Plane +X");
		ar(bLimitPlaneXNegative, "limitPlaneXNegative", "Limit Plane -X");
		ar(bLimitPlaneYPositive, "limitPlaneYPositive", "Limit Plane +Y");
		ar(bLimitPlaneYNegative, "limitPlaneYNegative", "Limit Plane -Y");
		ar(bLimitPlaneZPositive, "limitPlaneZPositive", "Limit Plane +Z");
		ar(bLimitPlaneZNegative, "limitPlaneZNegative", "Limit Plane -Z");

		value.flags &= ~(
		  PoseModifier::SDynamicsSpringDesc::eFlag_LimitPlaneXPositive | PoseModifier::SDynamicsSpringDesc::eFlag_LimitPlaneXNegative |
		  PoseModifier::SDynamicsSpringDesc::eFlag_LimitPlaneYPositive | PoseModifier::SDynamicsSpringDesc::eFlag_LimitPlaneYNegative |
		  PoseModifier::SDynamicsSpringDesc::eFlag_LimitPlaneZPositive | PoseModifier::SDynamicsSpringDesc::eFlag_LimitPlaneZNegative);

		if (bLimitPlaneXPositive) value.flags |= PoseModifier::SDynamicsSpringDesc::eFlag_LimitPlaneXPositive;
		if (bLimitPlaneXNegative) value.flags |= PoseModifier::SDynamicsSpringDesc::eFlag_LimitPlaneXNegative;
		if (bLimitPlaneYPositive) value.flags |= PoseModifier::SDynamicsSpringDesc::eFlag_LimitPlaneYPositive;
		if (bLimitPlaneYNegative) value.flags |= PoseModifier::SDynamicsSpringDesc::eFlag_LimitPlaneYNegative;
		if (bLimitPlaneZPositive) value.flags |= PoseModifier::SDynamicsSpringDesc::eFlag_LimitPlaneZPositive;
		if (bLimitPlaneZNegative) value.flags |= PoseModifier::SDynamicsSpringDesc::eFlag_LimitPlaneZNegative;
	}
	else
	{
		ar(value.flags, "flags", "");
	}

	if (ar.isInput())
	{
		value.length = std::max(value.length, 0.f);
		value.stiffness = clamp_tpl(value.stiffness, 0.0f, 100.f);
		value.damping = clamp_tpl(value.damping, 0.0f, 100.0f);
	}

	if (ar.isEdit())
	{
		if (value.node.name.empty())
			ar.warning(value.node.name, "Driven Node is not specified");
	}
}

void Serialize(Serialization::IArchive& ar, PoseModifier::SDynamicsPendulumDesc& value)
{
	using Serialization::Range;

	ar(value.node, "drivenNode", "Driven Node");

	ar(value.aimVector, "aimVector", "Aim Vector");
	ar(value.length, "length", "Length");
	ar(Range(value.stiffness, 0.0f, 100.0f), "stiffness", "Stiffness");
	ar(Range(value.damping, 0.0f, 100.0f), "damping", "Damping");
	ar(value.forceMovementMultiplier, "forceMovementMultiplier", "Movement Force");
	ar(value.gravity, "gravity", "Gravity");

	ar(Range(value.limitAngle, 0.0f, 180.0f), "limitAngle", "Limit Angle");
	ar(value.limitRotationAngles, "limitRotationAngles", "Limit Rotation");
	ar(value.bLimitPlane0, "limitPlane0", "Limit Plane Positive");
	ar(value.bLimitPlane1, "limitPlane1", "Limit Plane Negative");
	if (ar.isInput())
	{
		value.length = std::max(value.length, 0.1f);

		value.aimVector.NormalizeSafe(Vec3(1.0f, 0.0f, 0.0f));

		Vec3 sideVector = value.aimVector.Cross(value.upVector);
		value.upVector = sideVector.Cross(value.aimVector);
		value.upVector.NormalizeSafe(Vec3(0.0f, 1.0f, 0.0f));

		value.stiffness = clamp_tpl(value.stiffness, 0.0f, 100.0f);
		value.damping = clamp_tpl(value.damping, 0.0f, 100.0f);

		value.limitAngle = clamp_tpl(value.limitAngle, 0.0f, 180.0f);

		value.limitRotationAngles.x = clamp_tpl(value.limitRotationAngles.x, -180.0f, 180.0f);
		value.limitRotationAngles.y = clamp_tpl(value.limitRotationAngles.y, -180.0f, 180.0f);
		value.limitRotationAngles.z = clamp_tpl(value.limitRotationAngles.z, -180.0f, 180.0f);
	}

	if (ar.isEdit())
	{
		if (value.node.name.empty())
			ar.warning(value.node.name, "Driven Node is not specified.");
	}
}

void SNodeWeightDesc::Serialize(Serialization::IArchive& ar)
{
	using Serialization::Range;

	ar(enabled, "enabled", "^");
	ar(targetNode, "targetNode", "^Target");
	ar(weightNode, "weightNode", "Weight Node");
	ar(Range(weight, 0.0f, 1.0f), "weight", "Weight");

	if (ar.isEdit())
	{
		if (targetNode.name.empty())
			ar.warning(targetNode.name, "Target Node is not specified.");
	}
}

void Serialize(Serialization::IArchive& ar, PoseModifier::STransformBlenderDesc& value)
{
	using Serialization::Range;

	ar(value.node, "node", "Driven Node");
	ar(value.defaultTarget, "defaultTarget", "Default Target");
	ar(value.ordered, "ordered", "Order Dependent");
	ar(Range(value.weight, 0.0f, 1.0f), "weight", "Weight");
	ar(value.targets, "node", "Targets");

	if (ar.isEdit())
	{
		if (value.node.name.empty())
			ar.warning(value.node.name, "Driven Node is not specified.");
	}
}

} // namespace PoseModifier
