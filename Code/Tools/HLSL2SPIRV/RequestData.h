// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

struct SCompilerData;

namespace SRequestData
{
enum class EShaderStage
{
	INVALID = -1,
	Vertex,
	TessellationControl,
	TessellationEvaluation,
	Geometry,
	Fragment,
	Compute
};

enum class ERegisterRangeType : uint32_t
{
	ConstantBuffer,
	TextureAndBuffer,
	Sampler,
	UnorderedAccess,

	INVALID,
};

struct SRegisterRangeDesc
{
	SRegisterRangeDesc()
		: type(ERegisterRangeType::INVALID)
		, start(0)
		, count(0)
		, shaderStageMask(0)
	{}

	void setTypeAndStage(uint8_t typeAndStageByte)
	{
		shaderStageMask = typeAndStageByte & 0x3F;
		type = (ERegisterRangeType)(typeAndStageByte >> 6);
	}

	void setSlotNumberAndDescCount(uint8_t slotNumberAndDescCount)
	{
		start = slotNumberAndDescCount & 0x3F;
		count = slotNumberAndDescCount >> 6;
	}

	ERegisterRangeType type;
	uint32_t           start;
	uint32_t           count;
	uint32_t           shaderStageMask;
};

typedef std::vector<std::vector<SRegisterRangeDesc>> RegisterRanges;

struct SVertexInputElement
{
	SVertexInputElement()
		: semanticName("")
		, semanticIndex(0)
	{}

	char     semanticName[32];
	uint32_t semanticIndex;
};

struct SVertexInputDesc
{
	SVertexInputDesc()
		: objectStreamMask(0)
	{}

	std::vector<SVertexInputElement> vertexInputElements;
	uint8_t                          objectStreamMask;
};

struct SInfo
{
	SInfo()
		: shaderStage(EShaderStage::INVALID)
	{}

	std::string      profileName;
	std::string      entryName;

	RegisterRanges   registerRanges;
	SVertexInputDesc vertexInputDesc;

	EShaderStage     shaderStage;
};

bool Load(const std::string& dataFilename, SInfo& info);
}
