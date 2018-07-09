// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

struct SCompilerData;
namespace SRequestData
{
enum class EShaderStage;
}

namespace GLSLTranspiler
{
std::string GetFileExtension(SRequestData::EShaderStage shaderType);
bool        TranspileToVulkanGLSL(const SCompilerData& compilerData, std::string& vulkanGLSL);
bool        SaveVulkanGLSL(const SCompilerData& compilerData, const std::string& vulkanGLSL);
}
