// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include <CrySerialization/Forward.h>

namespace PoseModifier
{

struct SNode
{
public:
	string name;
	uint32 crc32;

public:
	SNode() :
		crc32(0)
	{
	}

public:
	bool IsSet() const;
	int  ResolveJointIndex(const CDefaultSkeleton& skeleton) const;
};

struct SConstraintPointNodeDesc
{
	SNode node;
	Vec3  localOffset;
	Vec3  worldOffset;

	SConstraintPointNodeDesc() :
		localOffset(ZERO)
		, worldOffset(ZERO)
	{
	}

	void Serialize(Serialization::IArchive& ar);
};

struct SConstraintPointDesc
{
	SNode                    node;
	SNode                    weightNode;

	SConstraintPointNodeDesc point;

	float                    weight;

	SConstraintPointDesc() :
		weight(1.0f)
	{
	}
};

struct SConstraintLineDesc
{
	SNode                    node;
	SNode                    weightNode;

	SConstraintPointNodeDesc startPoint;
	SConstraintPointNodeDesc endPoint;

	float                    weight;

	SConstraintLineDesc() :
		weight(1.0f)
	{
	}
};

struct SConstraintAimDesc
{
	SNode node;
	Vec3  aimVector;
	Vec3  upVector;

	SNode targetNode;
	SNode upNode;
	SNode weightNode;

	Vec3  targetOffset;
	Vec3  upOffset;

	float weight;

	SConstraintAimDesc() :
		aimVector(1.0f, 0.0f, 0.0f),
		upVector(0.0f, 1.0f, 0.0f),
		targetOffset(0.0f, 0.0f, 0.0f),
		upOffset(0.0f, 0.0f, 0.0f),
		weight(1.0f)
	{
	}
};

struct SDrivenTwistDesc
{
	SNode sourceNode;
	SNode targetNode;
	Vec3  targetVector;
	float weight;

	SDrivenTwistDesc() :
		targetVector(1.0f, 0.0f, 0.0f),
		weight(1.0f)
	{
	}
};

struct SIk2SegmentsDesc
{
	SNode rootNode;
	SNode linkNode;
	SNode endNode;
	SNode targetNode;
	SNode targetWeightNode;

	Vec3  endOffset;
	Vec3  targetOffset;
	float targetWeight;

	SIk2SegmentsDesc() :
		endOffset(ZERO),
		targetOffset(ZERO),
		targetWeight(1.0f)
	{
	}
};

struct SIkCcdDesc
{
	uint  iterations;
	float stepSize;
	float threshold;
	SNode rootNode;
	SNode endNode;
	SNode targetNode;
	SNode weightNode;

	Vec3  endOffset;
	Vec3  targetOffset;
	float weight;

	SIkCcdDesc() :
		iterations(30),
		stepSize(0.5f),
		threshold(0.001f),
		endOffset(ZERO),
		targetOffset(ZERO),
		weight(1.0f)
	{
	}
};

struct SDynamicsSpringDesc
{
	enum EFlag
	{
		eFlag_LimitPlaneXPositive = 1 << 0,
		eFlag_LimitPlaneXNegative = 1 << 1,
		eFlag_LimitPlaneYPositive = 1 << 2,
		eFlag_LimitPlaneYNegative = 1 << 3,
		eFlag_LimitPlaneZPositive = 1 << 4,
		eFlag_LimitPlaneZNegative = 1 << 5,
	};

	SNode node;
	Vec3  positionOffset;
	float length;
	float stiffness;
	float damping;
	Vec3  gravity;
	uint  flags;

	SDynamicsSpringDesc() :
		positionOffset(ZERO),
		length(0.1f),
		stiffness(1.0f),
		damping(0.1f),
		gravity(0.0f, 0.0f, -10.0f),
		flags(0)
	{
	}
};

struct SDynamicsPendulumDesc
{
	SNode node;

	Vec3  aimVector;
	Vec3  upVector;

	float length;
	float stiffness;
	float damping;

	Vec3  forceMovementMultiplier;
	Vec3  gravity;

	Vec3  limitRotationAngles;
	float limitAngle;
	bool  bLimitPlane0;
	bool  bLimitPlane1;

	SDynamicsPendulumDesc() :
		aimVector(1.0f, 0.0f, 0.0f),
		upVector(0.0f, 1.0f, 0.0f),
		length(0.1f),
		stiffness(1.0f),
		damping(0.1f),
		forceMovementMultiplier(1.0f),
		gravity(0.0f, 0.0f, -10.0f),
		limitRotationAngles(ZERO),
		limitAngle(180.0f),
		bLimitPlane0(false),
		bLimitPlane1(false)
	{
	}
};

struct SNodeWeightDesc
{
	SNode targetNode;
	SNode weightNode;
	float weight;
	bool  enabled;

	SNodeWeightDesc() :
		weight(1.0f),
		enabled(true)
	{
	}

	void Serialize(Serialization::IArchive& ar);
};

struct STransformBlenderDesc
{
	SNode                        node;
	SNode                        defaultTarget;

	std::vector<SNodeWeightDesc> targets;

	float                        weight;

	bool                         ordered;

	STransformBlenderDesc() :
		weight(1.0f)
		, ordered(false)
	{
	}
};

void Serialize(Serialization::IArchive& ar, PoseModifier::SConstraintPointNodeDesc& value);
void Serialize(Serialization::IArchive& ar, PoseModifier::SConstraintPointDesc& value);
void Serialize(Serialization::IArchive& ar, PoseModifier::SConstraintLineDesc& value);
void Serialize(Serialization::IArchive& ar, PoseModifier::SConstraintAimDesc& value);
void Serialize(Serialization::IArchive& ar, PoseModifier::SDrivenTwistDesc& value);
void Serialize(Serialization::IArchive& ar, PoseModifier::SIk2SegmentsDesc& value);
void Serialize(Serialization::IArchive& ar, PoseModifier::SIkCcdDesc& value);
void Serialize(Serialization::IArchive& ar, PoseModifier::SDynamicsSpringDesc& value);
void Serialize(Serialization::IArchive& ar, PoseModifier::SDynamicsPendulumDesc& value);
void Serialize(Serialization::IArchive& ar, PoseModifier::STransformBlenderDesc& value);
bool Serialize(Serialization::IArchive& ar, PoseModifier::SNode& value, const char* name, const char* label);

} // namespace PoseModifier
