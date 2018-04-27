// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.
#include "StdAfx.h"
#include "Serialization.h"
#include "FbxScene.h"

#include <CrySerialization/Math.h>
#include <CrySerialization/yasli/STL.h>

#include <assert.h>

namespace
{

template<typename T>
inline void SerializeSRT(Serialization::IArchive& ar, const T* pSource)
{
	Vec3 t(pSource->translation[0], pSource->translation[1], pSource->translation[2]);
	Quat r(pSource->rotation[3], pSource->rotation[0], pSource->rotation[1], pSource->rotation[2]);
	Vec3 s(pSource->scale[0], pSource->scale[1], pSource->scale[2]);
	ar(t, "t", "!Translation");
	ar(Serialization::AsAng3(r), "r", "!Rotation");
	if (s.x == s.y && s.x == s.z)
	{
		ar(s.x, "us", "!Uniform Scale");
	}
	else
	{
		ar(s, "s", "!Scale");
	}
}

} // unnamed namespace

void SNodeInfo::Serialize(Serialization::IArchive& ar)
{
	assert(pNode);
	SerializeSRT(ar, pNode);
	ar(pNode->numMeshes, "nm", "!Number of child meshes");
	ar(pNode->namedAttributes, "named_attributes", "!Attributes");
}

void SMeshInfo::Serialize(Serialization::IArchive& ar)
{
	SerializeSRT(ar, pMesh);

	ar(pMesh->numTriangles, "tri", "!Number of triangles");
	ar(pMesh->numVertices, "vtc", "!Number of vertices");
}

