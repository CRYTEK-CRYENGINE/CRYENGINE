// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

namespace compute_skinning
{

#pragma pack(push)
#pragma pack(1)

struct SSkinVertexIn
{
	Vec3 pos;
	uint morphDeltaOffset;
	Quat qtangent;
	Vec2 uv;
	uint triOffset;
	uint triCount;
};

struct SActiveMorphs
{
	uint  morphIndex;
	float weight;
};

#pragma pack(pop)

}
