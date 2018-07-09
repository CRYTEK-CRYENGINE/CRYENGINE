// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include "VertexData.h"

struct VertexCommand;

typedef void (* VertexCommandFunction)(VertexCommand&, CVertexData&);

struct CRY_ALIGN(16) RecTangents
{
	RecTangents()
	{
		t[0] = t[1] = t[2] = t[3] = 0.0f;
		n[0] = n[1] = n[2] = n[3] = 0.0f;
	};

	float t[4];
	float n[4];
};

//

struct VertexCommand
{
private:
	friend class CVertexCommandBuffer;

public:
	VertexCommand(VertexCommandFunction function) : Execute((VertexCommandFunction)function) {}

private:
	VertexCommandFunction Execute;
	uint8                 length;
};

//

struct VertexCommandCopy :
	public VertexCommand
{
public:
	strided_pointer<const Vec3>   pVertexPositions;
	strided_pointer<const uint32> pVertexColors;
	strided_pointer<const Vec2>   pVertexCoords;

	const vtx_idx*                pIndices;

public:
	VertexCommandCopy() :
		VertexCommand((VertexCommandFunction)Execute),
		pVertexPositions(NULL),
		pVertexColors(NULL),
		pVertexCoords(NULL),
		pIndices(NULL)
	{}

public:
	static void Execute(VertexCommandCopy& command, CVertexData& vertexData);
};

struct SSoftwareVertexFrame;
struct VertexCommandAdd :
	public VertexCommand
{
public:
	VertexCommandAdd() :
		VertexCommand((VertexCommandFunction)Execute),
		pVectors(NULL),
		pIndices(NULL),
		count(0),
		weight(0.0f)
	{}

public:
	static void Execute(VertexCommandAdd& command, CVertexData& vertexData);

public:
	strided_pointer<const Vec3>    pVectors;
	strided_pointer<const vtx_idx> pIndices;
	uint                           count;

	float                          weight;
};

//

struct VertexCommandSkin :
	public VertexCommand
{
public:
	VertexCommandSkin() :
		VertexCommand((VertexCommandFunction)Execute),
		pTransformations(NULL),
		pTransformationRemapTable(NULL),
		transformationCount(0),
		vertexTransformCount(0)
	{
	}

public:
	static void Execute(VertexCommandSkin& command, CVertexData& vertexData);

private:

	enum TemplateFlags
	{
		VELOCITY_VECTOR = 0x1,
		VELOCITY_ZERO   = 0x2,
	};

	template<uint32 TEMPLATE_FLAGS>
	static void ExecuteInternal(VertexCommandSkin& command, CVertexData& vertexData);

public:
	const DualQuat*                                  pTransformations;
	const JointIdType*                               pTransformationRemapTable;
	uint                                             transformationCount;

	strided_pointer<const Vec3>                      pVertexPositions;
	strided_pointer<const Vec3>                      pVertexPositionsPrevious;
	strided_pointer<const Quat>                      pVertexQTangents;
	strided_pointer<const SoftwareVertexBlendIndex>  pVertexTransformIndices;
	strided_pointer<const SoftwareVertexBlendWeight> pVertexTransformWeights;
	uint                                             vertexTransformCount;
};

struct VertexCommandTangents :
	public VertexCommand
{
public:
	VertexCommandTangents() : VertexCommand((VertexCommandFunction)Execute),
		pRecTangets(NULL),
		pTangentUpdateData(NULL),
		tangetUpdateDataCount(0),
		pTangentUpdateVertIds(NULL),
		tangetUpdateVertIdsCount(0)
	{
	}

public:
	static void Execute(VertexCommandTangents& command, CVertexData& vertexData);

public:
	RecTangents*                   pRecTangets;
	const STangentUpdateTriangles* pTangentUpdateData;
	uint                           tangetUpdateDataCount;
	const vtx_idx*                 pTangentUpdateVertIds;
	uint                           tangetUpdateVertIdsCount;
};
