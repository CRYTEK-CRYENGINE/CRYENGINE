// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

struct SCompilerData;

namespace GLSLReflection
{
struct SResourceInfo
{
	uint32_t NameOffset;
	uint32_t NameSize;
	uint32_t LayoutKeywordOffset;
	uint32_t LayoutKeywordSize;
	uint32_t PrecisionKeywordOffset;
	uint32_t PrecisionKeywordSize;
	uint32_t ResourceRegister;
	uint32_t ResourceGroup;
};

struct SSamplerInfo
{
	uint32_t NameOffset;
	uint32_t NameSize;
	uint32_t TextureRegister;
	uint32_t SamplerRegister;
	uint32_t LayoutKeywordOffset;
	uint32_t LayoutKeywordSize;
	uint32_t SamplerKeywordOffset;
	uint32_t SamplerKeywordSize;
	uint32_t PrecisionKeywordOffset;
	uint32_t PrecisionKeywordSize;
};

struct SReflection
{
	uint32_t                   uGLSLSourceOffset;
	uint32_t                   uSamplersOffset;
	uint32_t                   uNumSamplers;
	uint32_t                   uImagesOffset;
	uint32_t                   uNumImages;
	uint32_t                   uStorageBuffersOffset;
	uint32_t                   uNumStorageBuffers;
	uint32_t                   uUniformBuffersOffset;
	uint32_t                   uNumUniformBuffers;
	uint32_t                   uImportsOffset;
	uint32_t                   uImportsSize;
	uint32_t                   uExportsOffset;
	uint32_t                   uExportsSize;
	uint32_t                   uInputHash;
	uint32_t                   uSymbolsOffset;

	std::vector<SResourceInfo> ImageInfos;
	std::vector<SResourceInfo> StorageBufferInfos;
	std::vector<SResourceInfo> UniformBufferInfos;
	std::vector<SSamplerInfo>  SamplerInfos;
};

bool ProcessDXBC(SCompilerData& compilerData);
}
