// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include <stdint.h>
#include <vector>
#include <array>
#include "GLSLReflection.h"
#include "CompilerData.h"
#include "Log.h"
#include "..\HLSLCrossCompiler\include\hlslcc.h"
#include "..\HLSLCrossCompiler\include\hlslcc_bin.hpp"

namespace GLSLReflection
{
inline uint32_t DecodeTextureIndex(uint32_t uSamplerField)     { return uSamplerField >> 22; }
inline uint32_t DecodeSamplerIndex(uint32_t uSamplerField)     { return (uSamplerField >> 12) & 0x3FF; }
inline uint32_t DecodeTextureUnitIndex(uint32_t uSamplerField) { return (uSamplerField >> 2) & 0x3FF; }
inline bool     DecodeNormalSample(uint32_t uSamplerField)     { return ((uSamplerField >> 1) & 0x1) == 1; }
inline bool     DecodeCompareSample(uint32_t uSamplerField)    { return (uSamplerField & 0x1) == 1; }
inline uint32_t DecodeResourceIndex(uint32_t uResourceField)   { return uResourceField; }

static bool     FindLayoutKeyword(const SCompilerData& compilerData, uint32_t samplerTypeOffset, uint32_t& layoutKeywordOffset, uint32_t& layoutKeywordSize)
{
	const char* vulkanGLSL = reinterpret_cast<const char*>(&compilerData.dxbc[compilerData.reflection.uGLSLSourceOffset]);
	int32_t keywordOffset = samplerTypeOffset - sizeof("layout(") - 1;

	while (keywordOffset >= 0 && vulkanGLSL[keywordOffset] != '\n')
	{
		if (strncmp(vulkanGLSL + keywordOffset, "layout(",  sizeof("layout(")  - 1) == 0 &&
			strncmp(vulkanGLSL + keywordOffset, "layout(r", sizeof("layout(r") - 1) != 0 &&		// layout(rgba32f)
			strncmp(vulkanGLSL + keywordOffset, "layout(r", sizeof("layout(o") - 1) != 0)		// layout(offset)
		{
			// find end of keyword string
			int32_t keywordEnd = keywordOffset + sizeof("layout(") - 1;
			while (keywordEnd < compilerData.dxbc.size() && vulkanGLSL[keywordEnd] != ')' && vulkanGLSL[keywordEnd] != '\n')
				++keywordEnd;

			layoutKeywordOffset = keywordOffset;
			layoutKeywordSize = keywordEnd - keywordOffset + (vulkanGLSL[keywordEnd] == ')');

			return true;
		}

		--keywordOffset;
	}

	return false;
}

static bool     FindPrecisionKeywords(const SCompilerData& compilerData, uint32_t samplerTypeOffset, uint32_t& precisionKeywordOffset, uint32_t& precisionKeywordSize)
{
	const char* vulkanGLSL = reinterpret_cast<const char*>(&compilerData.dxbc[compilerData.reflection.uGLSLSourceOffset]);
	const std::array<uint32_t, 3> sizes = {sizeof("lowp"), sizeof("mediump"), sizeof("highp")};
	int32_t keywordOffset = samplerTypeOffset - *std::min_element(sizes.begin(), sizes.end()) - 1;
	std::array<uint32_t, 3> matches = {UINT32_MAX, UINT32_MAX, UINT32_MAX};

	while (keywordOffset >= 0 && vulkanGLSL[keywordOffset] != '\n')
	{
		if ((matches[0] = strncmp(vulkanGLSL + keywordOffset, "lowp"   , sizes[0] - 1) == 0 ? sizes[0] : matches[0]) != UINT32_MAX ||
		    (matches[1] = strncmp(vulkanGLSL + keywordOffset, "mediump", sizes[1] - 1) == 0 ? sizes[1] : matches[1]) != UINT32_MAX ||
		    (matches[2] = strncmp(vulkanGLSL + keywordOffset, "highp"  , sizes[2] - 1) == 0 ? sizes[2] : matches[2]) != UINT32_MAX)
		{
			// find end of keyword string
			int32_t keywordEnd = keywordOffset + *std::min_element(matches.begin(), matches.end()) - 1;
			while (keywordEnd < compilerData.dxbc.size() && vulkanGLSL[keywordEnd] != ' ' && vulkanGLSL[keywordEnd] != '\n')
				++keywordEnd;

			precisionKeywordOffset = keywordOffset;
			precisionKeywordSize = keywordEnd - keywordOffset;

			return true;
		}

		--keywordOffset;
	}

	return false;
}

static bool     FindSamplerKeyword(const SCompilerData& compilerData, uint32_t samplerNameOffset, uint32_t& samplerKeywordOffset, uint32_t& samplerKeywordSize)
{
	const char* vulkanGLSL = reinterpret_cast<const char*>(&compilerData.dxbc[compilerData.reflection.uGLSLSourceOffset]);
	int32_t keywordOffset = samplerNameOffset - sizeof("sampler") - 1;

	while (keywordOffset >= 0 && vulkanGLSL[keywordOffset] != '\n')
	{
		if (strncmp(vulkanGLSL + keywordOffset, "sampler", sizeof("sampler") - 1) == 0)
		{
			// find end of keyword string
			int32_t keywordEnd = keywordOffset + sizeof("sampler") - 1;
			while (keywordEnd < compilerData.dxbc.size() && vulkanGLSL[keywordEnd] != ' ' && vulkanGLSL[keywordEnd] != '\n')
				++keywordEnd;

			// make sure to include any format prefixes 'u'/'i'/'b'/etc.
			while (keywordOffset > 0 && vulkanGLSL[keywordOffset - 1] != ' ')
				--keywordOffset;

			samplerKeywordOffset = keywordOffset;
			samplerKeywordSize = keywordEnd - keywordOffset;

			return true;
		}

		--keywordOffset;
	}

	return false;
}

bool            ProcessDXBC(SCompilerData& compilerData)
{
	uint8_t* pSamplers       = reinterpret_cast<uint8_t*>(&compilerData.dxbc[compilerData.reflection.uSamplersOffset]);
	uint8_t* pImages         = reinterpret_cast<uint8_t*>(&compilerData.dxbc[compilerData.reflection.uImagesOffset]);
	uint8_t* pStorageBuffers = reinterpret_cast<uint8_t*>(&compilerData.dxbc[compilerData.reflection.uStorageBuffersOffset]);
	uint8_t* pUniformBuffers = reinterpret_cast<uint8_t*>(&compilerData.dxbc[compilerData.reflection.uUniformBuffersOffset]);

	for (uint32_t i = 0; i < compilerData.reflection.uNumSamplers; i++)
	{
		uint32_t* pSampler = reinterpret_cast<uint32_t*>(pSamplers + i * GLSL_SAMPLER_SIZE);

		uint32_t uSamplerField        = pSampler[0];
		uint32_t uEmbeddedNormalName  = pSampler[1];
		uint32_t uEmbeddedCompareName = pSampler[2];

		uint32_t uTextureIndex = DecodeTextureIndex(uSamplerField);
		uint32_t uSamplerIndex = DecodeSamplerIndex(uSamplerField);
		uint32_t uTextureUnitIndex = DecodeTextureUnitIndex(uSamplerField);
		bool bNormalSample = DecodeNormalSample(uSamplerField);
		bool bCompareSample = DecodeCompareSample(uSamplerField);

		uint32_t nameOffset;
		uint32_t nameSize;

		if (bCompareSample)
		{
			nameOffset = uEmbeddedCompareName >> 12;
			nameSize = uEmbeddedCompareName & 0x3FF;
		}
		else
		{
			nameOffset = uEmbeddedNormalName >> 12;
			nameSize = uEmbeddedNormalName & 0x3FF;
		}

		uint32_t samplerKeywordOffset = 0, samplerKeywordSize = 0;
		if (!FindSamplerKeyword(compilerData, nameOffset, samplerKeywordOffset, samplerKeywordSize))
		{
			char samplerName[256] = {};
			memcpy_s(samplerName, sizeof(samplerName), &compilerData.dxbc[compilerData.reflection.uGLSLSourceOffset + nameOffset], nameSize);

			ReportError("error: sampler keyword not found for sampler %s.\n", samplerName);
			return false;
		}

		uint32_t precisionKeywordOffset = 0, precisionKeywordSize = 0;
		if (!FindPrecisionKeywords(compilerData, samplerKeywordOffset, precisionKeywordOffset, precisionKeywordSize))
		{
		}

		uint32_t layoutKeywordOffset = 0, layoutKeywordSize = 0;
		if (!FindLayoutKeyword(compilerData, samplerKeywordOffset, layoutKeywordOffset, layoutKeywordSize))
		{
		}

		char samplerKeyword[512] = {};
		memcpy_s(samplerKeyword, sizeof(samplerKeyword), &compilerData.dxbc[compilerData.reflection.uGLSLSourceOffset + samplerKeywordOffset], samplerKeywordSize);
		
		if (strstr(samplerKeyword, "Buffer") != nullptr)
		{
			GLSLReflection::SResourceInfo imageInfo;
			imageInfo.NameOffset = nameOffset;
			imageInfo.NameSize = nameSize;
			imageInfo.LayoutKeywordOffset = layoutKeywordOffset;
			imageInfo.LayoutKeywordSize = layoutKeywordSize;
			imageInfo.PrecisionKeywordOffset = precisionKeywordOffset;
			imageInfo.PrecisionKeywordSize = precisionKeywordSize;
			imageInfo.ResourceRegister = uTextureIndex;
			imageInfo.ResourceGroup = RGROUP_TEXTURE;

			compilerData.reflection.ImageInfos.push_back(imageInfo);
		}
		else
		{
			GLSLReflection::SSamplerInfo samplerInfo;
			samplerInfo.NameOffset = nameOffset;
			samplerInfo.NameSize = nameSize;
			samplerInfo.LayoutKeywordOffset = layoutKeywordOffset;
			samplerInfo.LayoutKeywordSize = layoutKeywordSize;
			samplerInfo.SamplerKeywordOffset = samplerKeywordOffset;
			samplerInfo.SamplerKeywordSize = samplerKeywordSize;
			samplerInfo.PrecisionKeywordOffset = precisionKeywordOffset;
			samplerInfo.PrecisionKeywordSize = precisionKeywordSize;
			samplerInfo.TextureRegister = uTextureIndex;
			samplerInfo.SamplerRegister = uSamplerIndex;

			compilerData.reflection.SamplerInfos.push_back(samplerInfo);
		}
	}

	// OPCODE_DCL_UNORDERED_ACCESS_VIEW_TYPED, RGROUP_UAV
	for (uint32_t i = 0; i < compilerData.reflection.uNumImages; i++)
	{
		uint32_t* pImage = reinterpret_cast<uint32_t*>(pImages + i * GLSL_RESOURCE_SIZE);

		uint32_t uResourceField = pImage[0];
		uint32_t uResourceName  = pImage[1];
		uint32_t uResourceGroup = pImage[2];
		uint32_t uResourceIndex = DecodeResourceIndex(uResourceField);

		uint32_t nameOffset = uResourceName >> 12;
		uint32_t nameSize = uResourceName & 0x3FF;

		uint32_t precisionKeywordOffset = 0, precisionKeywordSize = 0;
		if (!FindPrecisionKeywords(compilerData, nameOffset, precisionKeywordOffset, precisionKeywordSize))
		{
		}

		uint32_t layoutKeywordOffset = 0, layoutKeywordSize = 0;
		if (!FindLayoutKeyword(compilerData, nameOffset, layoutKeywordOffset, layoutKeywordSize))
		{
		}

		GLSLReflection::SResourceInfo imageInfo;
		imageInfo.NameOffset = nameOffset;
		imageInfo.NameSize = nameSize;
		imageInfo.LayoutKeywordOffset = layoutKeywordOffset;
		imageInfo.LayoutKeywordSize = layoutKeywordSize;
		imageInfo.PrecisionKeywordOffset = precisionKeywordOffset;
		imageInfo.PrecisionKeywordSize = precisionKeywordSize;
		imageInfo.ResourceRegister = uResourceIndex;
		imageInfo.ResourceGroup = uResourceGroup;

		compilerData.reflection.ImageInfos.push_back(imageInfo);
	}

	// OPCODE_DCL_UNORDERED_ACCESS_VIEW_RAW, RGROUP_UAV
	// OPCODE_DCL_UNORDERED_ACCESS_VIEW_STRUCTURED, RGROUP_UAV
	// OPCODE_DCL_RESOURCE_RAW, RGROUP_TEXTURE
	// OPCODE_DCL_RESOURCE_STRUCTURED, RGROUP_TEXTURE
	for (uint32_t i = 0; i < compilerData.reflection.uNumStorageBuffers; i++)
	{
		uint32_t* pStorageBuffer = reinterpret_cast<uint32_t*>(pStorageBuffers + i * GLSL_RESOURCE_SIZE);

		uint32_t uResourceField = pStorageBuffer[0];
		uint32_t uResourceName  = pStorageBuffer[1];
		uint32_t uResourceGroup = pStorageBuffer[2];
		uint32_t uResourceIndex = DecodeResourceIndex(uResourceField);

		uint32_t nameOffset = uResourceName >> 12;
		uint32_t nameSize = uResourceName & 0x3FF;

		uint32_t layoutKeywordOffset = 0, layoutKeywordSize = 0;
		if (!FindLayoutKeyword(compilerData, nameOffset, layoutKeywordOffset, layoutKeywordSize))
		{
		}

		GLSLReflection::SResourceInfo storageBufferInfo;
		storageBufferInfo.NameOffset = nameOffset;
		storageBufferInfo.NameSize = nameSize;
		storageBufferInfo.LayoutKeywordOffset = layoutKeywordOffset;
		storageBufferInfo.LayoutKeywordSize = layoutKeywordSize;
		storageBufferInfo.PrecisionKeywordOffset = 0;
		storageBufferInfo.PrecisionKeywordSize = 0;
		storageBufferInfo.ResourceRegister = uResourceIndex;
		storageBufferInfo.ResourceGroup = uResourceGroup;

		compilerData.reflection.StorageBufferInfos.push_back(storageBufferInfo);
	}

	// OPCODE_DCL_CONSTANT_BUFFER, RGROUP_CBUFFER
	for (uint32_t i = 0; i < compilerData.reflection.uNumUniformBuffers; i++)
	{
		uint32_t* pUniformBuffer = reinterpret_cast<uint32_t*>(pUniformBuffers + i * GLSL_RESOURCE_SIZE);

		uint32_t uResourceField = pUniformBuffer[0];
		uint32_t uResourceName  = pUniformBuffer[1];
		uint32_t uResourceGroup = pUniformBuffer[2];
		uint32_t uResourceIndex = DecodeResourceIndex(uResourceField);

		uint32_t nameOffset = uResourceName >> 12;
		uint32_t nameSize = uResourceName & 0x3FF;

		uint32_t layoutKeywordOffset = 0, layoutKeywordSize = 0;
		if (!FindLayoutKeyword(compilerData, nameOffset, layoutKeywordOffset, layoutKeywordSize))
		{
		}

		GLSLReflection::SResourceInfo uniformBufferInfo;
		uniformBufferInfo.NameOffset = nameOffset;
		uniformBufferInfo.NameSize = nameSize;
		uniformBufferInfo.LayoutKeywordOffset = layoutKeywordOffset;
		uniformBufferInfo.LayoutKeywordSize = layoutKeywordSize;
		uniformBufferInfo.PrecisionKeywordOffset = 0;
		uniformBufferInfo.PrecisionKeywordSize = 0;
		uniformBufferInfo.ResourceRegister = uResourceIndex;
		uniformBufferInfo.ResourceGroup = uResourceGroup;

		compilerData.reflection.UniformBufferInfos.push_back(uniformBufferInfo);
	}

	return true;
}
}
