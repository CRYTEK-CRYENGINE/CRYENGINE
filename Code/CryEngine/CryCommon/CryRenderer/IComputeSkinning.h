// Copyright 2001-2016 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

struct IRenderMesh;

#pragma pack(push)
#pragma pack(1)

namespace compute_skinning
{
struct SSkinVertexIn
{
	Vec3 pos;
	uint morphDeltaOffset;
	Quat qtangent;
	Vec2 uv;
	uint triOffset;
	uint triCount;
};

struct SComputeShaderSkinVertexOut
{
	Vec3  pos;
	float pad1;
	Quat  qtangent;
	Vec3  tangent;
	float pad2;
	Vec3  bitangent;
	float pad3;
	Vec2  uv;
	float pad4[2];
};

// check ORBIS
struct SIndices
{
	uint32 ind;
};

struct SComputeShaderTriangleNT
{
	Vec3 normal;
	Vec3 tangent;
};

struct SMorphsDeltas
{
	Vec4 delta;
};

struct SMorphsBitField
{
	// packed as 32:32
	uint64 maskZoom;
};

struct SSkinning
{
	// packed as 8:24
	uint weightIndex;
};

struct SSkinningMap
{
	uint offset;
	uint count;
};

struct SActiveMorphs
{
	uint  morphIndex;
	float weight;
};

#pragma pack(pop)

enum EType
{
	eType_Invalid,
	eType_VertexInSrv,
	eType_VertexOutUav,
	eType_TriangleNtUav,
	eType_IndicesSrv,
	eType_TriangleAdjacencyIndicesSrv,
	eType_MorphsDeltaSrv,
	eType_MorphsBitFieldSrv,
	eType_SkinningSrv,
	eType_SkinningMapSrv,
	eType_AMOUNT
};
}
