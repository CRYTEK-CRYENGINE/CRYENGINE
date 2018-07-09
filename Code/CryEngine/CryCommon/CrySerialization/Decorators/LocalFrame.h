// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include <CryMath/Cry_Math.h>
#include <CrySerialization/Math.h>
#include <CrySerialization/Forward.h>

namespace Serialization
{

struct LocalPosition
{
	Vec3*       value;
	int         space;
	const char* parentName;
	const void* handle;

	LocalPosition(Vec3& vec, int space, const char* parentName, const void* handle)
		: value(&vec)
		, space(space)
		, parentName(parentName)
		, handle(handle)
	{
	}

	void Serialize(IArchive& ar);
};

struct LocalOrientation
{
	Quat*       value;
	int         space;
	const char* parentName;
	const void* handle;

	LocalOrientation(Quat& vec, int space, const char* parentName, const void* handle)
		: value(&vec)
		, space(space)
		, parentName(parentName)
		, handle(handle)
	{
	}

	void Serialize(IArchive& ar);
};

struct LocalFrame
{
	Quat*       rotation;
	Vec3*       position;
	const char* parentName;
	int         rotationSpace;
	int         positionSpace;
	const void* handle;

	LocalFrame(Quat* rotation, int rotationSpace, Vec3* position, int positionSpace, const char* parentName, const void* handle)
		: rotation(rotation)
		, position(position)
		, parentName(parentName)
		, rotationSpace(rotationSpace)
		, positionSpace(positionSpace)
		, handle(handle)
	{
	}

	void Serialize(IArchive& ar);
};

enum
{
	SPACE_JOINT,
	SPACE_ENTITY,
	SPACE_JOINT_WITH_PARENT_ROTATION,
	SPACE_JOINT_WITH_CHARACTER_ROTATION,
	SPACE_SOCKET_RELATIVE_TO_JOINT,
	SPACE_SOCKET_RELATIVE_TO_BINDPOSE
};

inline LocalPosition LocalToEntity(Vec3& position, const void* handle = 0)
{
	return LocalPosition(position, SPACE_ENTITY, "", handle ? handle : &position);
}
inline LocalPosition LocalToJoint(Vec3& position, const string& jointName, const void* handle = 0)
{
	return LocalPosition(position, SPACE_JOINT, jointName.c_str(), handle ? handle : &position);
}

inline LocalPosition LocalToJointCharacterRotation(Vec3& position, const string& jointName, const void* handle = 0)
{
	return LocalPosition(position, SPACE_JOINT_WITH_CHARACTER_ROTATION, jointName.c_str(), handle ? handle : &position);
}

bool Serialize(Serialization::IArchive& ar, Serialization::LocalPosition& value, const char* name, const char* label);
bool Serialize(Serialization::IArchive& ar, Serialization::LocalOrientation& value, const char* name, const char* label);
bool Serialize(Serialization::IArchive& ar, Serialization::LocalFrame& value, const char* name, const char* label);

}

#include "LocalFrameImpl.h"
