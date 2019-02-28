// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#define _CRT_SECURE_NO_WARNINGS
#include <assert.h>
#include <set>
#include <string>
#include <vector>
#include "GLSLTranspiler.h"
#include "Log.h"
#include "CompilerData.h"

namespace GLSLTranspiler
{
enum class ETextureType
{
	INVALID = -1,
	Float,
	Int,
	UInt,
	Shadow
};

enum class ETextureDimension
{
	INVALID = -1,
	_1D,
	_2D,
	_3D,
	Cube,
	_2DRect,
	_1DArray,
	_2DArray,
	CubeArray,
	_2DMS,
	_2DMSArray
};

enum { UnusedSamplerMarker = 256 };

struct STextureUniform
{
	STextureUniform()
		: Register(-1)
		, TextureType(ETextureType::INVALID)
		, TextureDimension(ETextureDimension::INVALID)
	{}

	bool operator<(const STextureUniform& rhs) const
	{
		return Register < rhs.Register;
	}

	int               Register;
	ETextureType      TextureType;
	ETextureDimension TextureDimension;
	std::string       Name;
	std::string       Precision;
};

struct SSamplerUniform
{
	SSamplerUniform()
		: Register(-1)
		, IsShadow(false)
	{}

	bool operator<(const SSamplerUniform& rhs) const
	{
		return Register < rhs.Register;
	}

	int         Register;
	bool        IsShadow;
	std::string Name;
};

static uint32_t FindLineStart(const char* text, uint32_t offset)
{
	while (offset > 0 && text[offset - 1] != '\n')
		--offset;

	return offset;
}

size_t InsertSubString(std::string& sourceCode, GLSLReflection::SReflection& reflection, const std::string& subStr, size_t position)
{
	size_t subStrLenght = subStr.size();
	std::string temp = sourceCode.substr(0, position) + subStr + sourceCode.substr(position);
	sourceCode = temp;

	for (auto& samplerInfo : reflection.SamplerInfos)
	{
		samplerInfo.NameOffset             += (samplerInfo.NameOffset             >= position) ? (uint32_t)subStrLenght : 0;
		samplerInfo.SamplerKeywordOffset   += (samplerInfo.SamplerKeywordOffset   >= position) ? (uint32_t)subStrLenght : 0;
		samplerInfo.PrecisionKeywordOffset += (samplerInfo.PrecisionKeywordOffset >= position) ? (uint32_t)subStrLenght : 0;
		samplerInfo.LayoutKeywordOffset    += (samplerInfo.LayoutKeywordOffset    >= position) ? (uint32_t)subStrLenght : 0;
	}

	for (auto& imageInfo : reflection.ImageInfos)
	{
		imageInfo.NameOffset               += (imageInfo.NameOffset               >= position) ? (uint32_t)subStrLenght : 0;
		imageInfo.PrecisionKeywordOffset   += (imageInfo.PrecisionKeywordOffset   >= position) ? (uint32_t)subStrLenght : 0;
		imageInfo.LayoutKeywordOffset      += (imageInfo.LayoutKeywordOffset      >= position) ? (uint32_t)subStrLenght : 0;
	}

	for (auto& storageBufferInfo : reflection.StorageBufferInfos)
	{
		storageBufferInfo.NameOffset             += (storageBufferInfo.NameOffset             >= position) ? (uint32_t)subStrLenght : 0;
		storageBufferInfo.PrecisionKeywordOffset += (storageBufferInfo.PrecisionKeywordOffset >= position) ? (uint32_t)subStrLenght : 0;
		storageBufferInfo.LayoutKeywordOffset    += (storageBufferInfo.LayoutKeywordOffset    >= position) ? (uint32_t)subStrLenght : 0;
	}

	for (auto& uniformBufferInfo : reflection.UniformBufferInfos)
	{
		uniformBufferInfo.NameOffset             += (uniformBufferInfo.NameOffset             >= position) ? (uint32_t)subStrLenght : 0;
		uniformBufferInfo.PrecisionKeywordOffset += (uniformBufferInfo.PrecisionKeywordOffset >= position) ? (uint32_t)subStrLenght : 0;
		uniformBufferInfo.LayoutKeywordOffset    += (uniformBufferInfo.LayoutKeywordOffset    >= position) ? (uint32_t)subStrLenght : 0;
	}

	return position + subStrLenght;
}

void DeleteSubString(std::string& sourceCode, GLSLReflection::SReflection& reflection, size_t position, size_t subStrLenght)
{
	std::string temp = sourceCode.substr(0, position) + sourceCode.substr(position + subStrLenght);
	sourceCode = temp;

	for (auto& samplerInfo : reflection.SamplerInfos)
	{
		samplerInfo.NameOffset             -= (samplerInfo.NameOffset             >= position) ? (uint32_t)subStrLenght : 0;
		samplerInfo.SamplerKeywordOffset   -= (samplerInfo.SamplerKeywordOffset   >= position) ? (uint32_t)subStrLenght : 0;
		samplerInfo.PrecisionKeywordOffset -= (samplerInfo.PrecisionKeywordOffset >= position) ? (uint32_t)subStrLenght : 0;
		samplerInfo.LayoutKeywordOffset    -= (samplerInfo.LayoutKeywordOffset    >= position) ? (uint32_t)subStrLenght : 0;
	}

	for (auto& imageInfo : reflection.ImageInfos)
	{
		imageInfo.NameOffset               -= (imageInfo.NameOffset               >= position) ? (uint32_t)subStrLenght : 0;
		imageInfo.PrecisionKeywordOffset   -= (imageInfo.PrecisionKeywordOffset   >= position) ? (uint32_t)subStrLenght : 0;
		imageInfo.LayoutKeywordOffset      -= (imageInfo.LayoutKeywordOffset      >= position) ? (uint32_t)subStrLenght : 0;
	}

	for (auto& storageBufferInfo : reflection.StorageBufferInfos)
	{
		storageBufferInfo.NameOffset             -= (storageBufferInfo.NameOffset             >= position) ? (uint32_t)subStrLenght : 0;
		storageBufferInfo.PrecisionKeywordOffset -= (storageBufferInfo.PrecisionKeywordOffset >= position) ? (uint32_t)subStrLenght : 0;
		storageBufferInfo.LayoutKeywordOffset    -= (storageBufferInfo.LayoutKeywordOffset    >= position) ? (uint32_t)subStrLenght : 0;
	}

	for (auto& uniformBufferInfo : reflection.UniformBufferInfos)
	{
		uniformBufferInfo.NameOffset             -= (uniformBufferInfo.NameOffset             >= position) ? (uint32_t)subStrLenght : 0;
		uniformBufferInfo.PrecisionKeywordOffset -= (uniformBufferInfo.PrecisionKeywordOffset >= position) ? (uint32_t)subStrLenght : 0;
		uniformBufferInfo.LayoutKeywordOffset    -= (uniformBufferInfo.LayoutKeywordOffset    >= position) ? (uint32_t)subStrLenght : 0;
	}
}

void ReplaceSubString(std::string& sourceCode, GLSLReflection::SReflection& reflection, const std::string& originalSubStr, const std::string& newSubStr)
{
	size_t subStrIndex = sourceCode.find(originalSubStr);
	size_t subStrLength = originalSubStr.size();

	while (subStrIndex != std::string::npos)
	{
		DeleteSubString(sourceCode, reflection, subStrIndex, subStrLength);
		InsertSubString(sourceCode, reflection, newSubStr, subStrIndex);

		subStrIndex = sourceCode.find(originalSubStr);
	}
}

void DeleteStatementWithKeyWord(std::string& sourceCode, GLSLReflection::SReflection& reflection, const std::string& keyWord)
{
	size_t keyWordIndex = sourceCode.find(keyWord);
	while (keyWordIndex != std::string::npos)
	{
		size_t startIndex = sourceCode.rfind("\n", keyWordIndex);

		if (startIndex == std::string::npos)
			startIndex = 0;
		else
			startIndex += 1;

		size_t endIndex = sourceCode.find("\n", keyWordIndex);
		if (endIndex == std::string::npos)
			endIndex = sourceCode.size() - 1;

		DeleteSubString(sourceCode, reflection, startIndex, endIndex - startIndex + 1);

		keyWordIndex = sourceCode.find(keyWord);
	}
}

bool GetSetAndBindingIndex(const SRequestData::SInfo& requestData, uint32_t registerRangeType, uint32_t resourceRegister, int& setIndex, int& bindingIndex)
{
	const SRequestData::RegisterRanges& registerRanges = requestData.registerRanges;

	for (setIndex = 0; setIndex < (int)registerRanges.size(); setIndex++)
	{
		bindingIndex = 0;
		for (size_t i = 0; i < registerRanges[setIndex].size(); i++)
		{
			const SRequestData::SRegisterRangeDesc& rangeDesc = registerRanges[setIndex][i];

			if (rangeDesc.shaderStageMask & (1u << unsigned int(requestData.shaderStage)))
			{
				if (uint32_t(rangeDesc.type) == registerRangeType && rangeDesc.start <= resourceRegister && rangeDesc.start + rangeDesc.count > resourceRegister)
				{
					bindingIndex += resourceRegister - rangeDesc.start;
					return true;
				}
			}

			bindingIndex += rangeDesc.count;
		}
	}

	setIndex = -1;
	bindingIndex = -1;
	return false;
}

std::string GetVariableName(const std::string& sourceCode, size_t nameOffset, size_t nameLength)
{
	char name[256];
	assert(nameLength < 256);

	memcpy(name, &sourceCode[nameOffset], nameLength);
	name[nameLength] = 0;

	return std::string(name);
}

void SamplerKeyWordToTextureParams(const std::string& sampler, ETextureType& textureType, ETextureDimension& textureDimension)
{
	if (sampler[0] == 'i')
		textureType = ETextureType::Int;
	else if (sampler[0] == 'u')
		textureType = ETextureType::UInt;
	else if (sampler.find("Shadow") != std::string::npos)
		textureType = ETextureType::Shadow;
	else
		textureType = ETextureType::Float;

	if (sampler.find("2DMSArray") != std::string::npos)
		textureDimension = ETextureDimension::_2DMSArray;
	else if (sampler.find("2DMS") != std::string::npos)
		textureDimension = ETextureDimension::_2DMS;
	else if (sampler.find("CubeArray") != std::string::npos)
		textureDimension = ETextureDimension::CubeArray;
	else if (sampler.find("2DArray") != std::string::npos)
		textureDimension = ETextureDimension::_2DArray;
	else if (sampler.find("1DArray") != std::string::npos)
		textureDimension = ETextureDimension::_1DArray;
	else if (sampler.find("2DRect") != std::string::npos)
		textureDimension = ETextureDimension::_2DRect;
	else if (sampler.find("Cube") != std::string::npos)
		textureDimension = ETextureDimension::Cube;
	else if (sampler.find("3D") != std::string::npos)
		textureDimension = ETextureDimension::_3D;
	else if (sampler.find("2D") != std::string::npos)
		textureDimension = ETextureDimension::_2D;
	else
		textureDimension = ETextureDimension::_1D;
}

std::string TextureParamsToTextureKeyWord(ETextureType textureType, ETextureDimension textureDimension)
{
	std::string textureKeyWord;

	if (textureType == ETextureType::Int)
		textureKeyWord += 'i';
	if (textureType == ETextureType::UInt)
		textureKeyWord += 'u';

	switch (textureDimension)
	{
	case ETextureDimension::_1D:
		textureKeyWord += "texture1D";
		break;
	case ETextureDimension::_2D:
		textureKeyWord += "texture2D";
		break;
	case ETextureDimension::_3D:
		textureKeyWord += "texture3D";
		break;
	case ETextureDimension::Cube:
		textureKeyWord += "textureCube";
		break;
	case ETextureDimension::_2DRect:
		textureKeyWord += "texture2DRect";
		break;
	case ETextureDimension::_1DArray:
		textureKeyWord += "texture1DArray";
		break;
	case ETextureDimension::_2DArray:
		textureKeyWord += "texture2DArray";
		break;
	case ETextureDimension::CubeArray:
		textureKeyWord += "textureCubeArray";
		break;
	case ETextureDimension::_2DMS:
		textureKeyWord += "texture2DMS";
		break;
	case ETextureDimension::_2DMSArray:
		textureKeyWord += "texture2DMSArray";
		break;
	}

	return textureKeyWord;
}

std::string GetFileExtension(SRequestData::EShaderStage shaderType)
{
	switch (shaderType)
	{
	case SRequestData::EShaderStage::Vertex:
		return VSFileExtension;
	case SRequestData::EShaderStage::TessellationControl:
		return TCSFileExtension;
	case SRequestData::EShaderStage::TessellationEvaluation:
		return TESFileExtension;
	case SRequestData::EShaderStage::Geometry:
		return GSFileExtension;
	case SRequestData::EShaderStage::Fragment:
		return FSFileExtension;
	case SRequestData::EShaderStage::Compute:
		return CSFileExtension;
	}

	return "";
}

bool TranspileToVulkanGLSL(const SCompilerData& compilerData, std::string& vulkanGLSL)
{
	const SRequestData::SInfo& requestData = compilerData.request;
	GLSLReflection::SReflection reflection(compilerData.reflection);
	vulkanGLSL = reinterpret_cast<const char*>(&compilerData.dxbc[reflection.uGLSLSourceOffset]);

	size_t insertPosition = 0;
	insertPosition = InsertSubString(vulkanGLSL, reflection, std::string("#version 450\n"), insertPosition);

	int setIndex, bindingIndex;
	char tempStr[256];

	for (size_t i = 0; i < reflection.ImageInfos.size(); i++)
	{
		const GLSLReflection::SResourceInfo& varInfo = reflection.ImageInfos[i];

		if (!GetSetAndBindingIndex(requestData, varInfo.ResourceGroup, varInfo.ResourceRegister, setIndex, bindingIndex))
		{
			ReportError("error: '%s' not found in register range desc (%s).\n", GetVariableName(vulkanGLSL, varInfo.NameOffset, varInfo.NameSize).c_str(), compilerData.filename.c_str());
			return false;
		}

		// exclude the deletion of "layout(r32ui)" and similar format specifiers and "layout(offset)"
		if (vulkanGLSL[varInfo.LayoutKeywordOffset + sizeof("layout(") - 1] != 'r' &&
			vulkanGLSL[varInfo.LayoutKeywordOffset + sizeof("layout(") - 1] != 'o')
			DeleteSubString(vulkanGLSL, reflection, varInfo.LayoutKeywordOffset, varInfo.LayoutKeywordSize);

		sprintf_s(tempStr, 256, "layout(set = %i, binding = %i) ", setIndex, bindingIndex);
		InsertSubString(vulkanGLSL, reflection, tempStr, FindLineStart(vulkanGLSL.c_str(), varInfo.NameOffset));
	}

	for (size_t i = 0; i < reflection.StorageBufferInfos.size(); i++)
	{
		const GLSLReflection::SResourceInfo& varInfo = reflection.StorageBufferInfos[i];

		if (!GetSetAndBindingIndex(requestData, varInfo.ResourceGroup, varInfo.ResourceRegister, setIndex, bindingIndex))
		{
			ReportError("error: '%s' not found in register range desc (%s).\n", GetVariableName(vulkanGLSL, varInfo.NameOffset, varInfo.NameSize).c_str(), compilerData.filename.c_str());
			return false;
		}

		// exclude the deletion of "layout(r32ui)" and similar format specifiers and "layout(offset)"
		if (vulkanGLSL[varInfo.LayoutKeywordOffset + sizeof("layout(") - 1] != 'r' &&
			vulkanGLSL[varInfo.LayoutKeywordOffset + sizeof("layout(") - 1] != 'o')
			DeleteSubString(vulkanGLSL, reflection, varInfo.LayoutKeywordOffset, varInfo.LayoutKeywordSize);

		sprintf_s(tempStr, 256, "layout(set = %i, binding = %i) ", setIndex, bindingIndex);
		InsertSubString(vulkanGLSL, reflection, tempStr, FindLineStart(vulkanGLSL.c_str(), varInfo.NameOffset));
	}

	for (size_t i = 0; i < reflection.UniformBufferInfos.size(); i++)
	{
		const GLSLReflection::SResourceInfo& varInfo = reflection.UniformBufferInfos[i];

		if (!GetSetAndBindingIndex(requestData, varInfo.ResourceGroup, varInfo.ResourceRegister, setIndex, bindingIndex))
		{
			ReportError("error: '%s' not found in register range desc (%s).\n", GetVariableName(vulkanGLSL, varInfo.NameOffset, varInfo.NameSize).c_str(), compilerData.filename.c_str());
			return false;
		}

		// exclude the deletion of "layout(r32ui)" and similar format specifiers and "layout(offset)"
		if (vulkanGLSL[varInfo.LayoutKeywordOffset + sizeof("layout(") - 1] != 'r' &&
			vulkanGLSL[varInfo.LayoutKeywordOffset + sizeof("layout(") - 1] != 'o')
			DeleteSubString(vulkanGLSL, reflection, varInfo.LayoutKeywordOffset, varInfo.LayoutKeywordSize);

		sprintf_s(tempStr, 256, "layout(set = %i, binding = %i) ", setIndex, bindingIndex);
		InsertSubString(vulkanGLSL, reflection, tempStr, FindLineStart(vulkanGLSL.c_str(), varInfo.NameOffset));
	}

	std::set<SSamplerUniform> samplerUniforms;
	std::set<STextureUniform> textureUniforms;

	for (size_t i = 0; i < reflection.SamplerInfos.size(); i++)
	{
		// dcl_Texture0_volFogShadowGatherTex_State0_volFogShadowGatherSState
		char samplerName[256] = {};
		memcpy_s(samplerName, sizeof(samplerName), &vulkanGLSL[reflection.SamplerInfos[i].NameOffset], reflection.SamplerInfos[i].NameSize);

		char samplerKeyWord[256] = {};
		memcpy_s(samplerKeyWord, sizeof(samplerKeyWord), &vulkanGLSL[reflection.SamplerInfos[i].SamplerKeywordOffset], reflection.SamplerInfos[i].SamplerKeywordSize);

		char precisionKeyword[256] = {};
		memcpy_s(precisionKeyword, sizeof(precisionKeyword), &vulkanGLSL[reflection.SamplerInfos[i].PrecisionKeywordOffset], reflection.SamplerInfos[i].PrecisionKeywordSize);

		uint32_t lineStart = reflection.SamplerInfos[i].NameOffset;
		while (lineStart > 0 && vulkanGLSL[lineStart -1] != '\n') --lineStart;

		uint32_t lineEnd = reflection.SamplerInfos[i].NameOffset;
		while (lineEnd < vulkanGLSL.size() && vulkanGLSL[lineEnd] != '\n') ++lineEnd;

		char resourceType[256] = {};
		char stateType[256] = {};
		char textureName[256] = {};
		char stateName[256] = {};
		char textureNameFull[256] = {};
		char stateNameFull[256] = {};
		uint32_t textureSlot = 0;
		uint32_t stateSlot = 0;

		bool failed = false;
		if (reflection.SamplerInfos[i].SamplerRegister != UnusedSamplerMarker)
		{
			uint32_t r = sscanf(samplerName, "dcl_%[a-zA-Z]%d_%[a-zA-Z]_%[a-zA-Z]%d_%[a-zA-Z]", resourceType, &textureSlot, textureName, stateType, &stateSlot, stateName);
			if (r != 6)
			{
				sprintf_s(textureNameFull, 256, "Resource%d", reflection.SamplerInfos[i].TextureRegister);
				sprintf_s(stateNameFull, 256, "State%d", reflection.SamplerInfos[i].SamplerRegister);
			}
			else
			{
				sprintf_s(textureNameFull, 256, "%s%d_%s", resourceType, textureSlot, textureName);
				sprintf_s(stateNameFull, 256, "%s%d_%s", stateType, stateSlot, stateName);
			}
		}
		else
		{
			uint32_t r = sscanf(samplerName, "dcl_%[a-zA-Z]%d_%[a-zA-Z]", resourceType, &textureSlot, textureName);
			if (r != 3)
			{
				sprintf_s(textureNameFull, 256, "Resource%d", reflection.SamplerInfos[i].TextureRegister);
			}
			else
			{
				sprintf_s(textureNameFull, 256, "%s%d_%s", resourceType, textureSlot, textureName);
			}
		}

		STextureUniform textureUniform;
		textureUniform.Register = reflection.SamplerInfos[i].TextureRegister;
		SamplerKeyWordToTextureParams(std::string(samplerKeyWord), textureUniform.TextureType, textureUniform.TextureDimension);
		textureUniform.Name = textureNameFull;
		textureUniform.Precision = precisionKeyword;
		textureUniforms.insert(textureUniform);

		if (reflection.SamplerInfos[i].SamplerRegister != UnusedSamplerMarker)
		{
			SSamplerUniform samplerUniform;
			samplerUniform.Register = reflection.SamplerInfos[i].SamplerRegister;
			samplerUniform.IsShadow = textureUniform.TextureType == ETextureType::Shadow;
			samplerUniform.Name = stateNameFull;
			samplerUniforms.insert(samplerUniform);
		}

		DeleteSubString(vulkanGLSL, reflection, lineStart, lineEnd - lineStart + 1);
		if (reflection.SamplerInfos[i].SamplerRegister != UnusedSamplerMarker)
			sprintf_s(tempStr, 256, "#define %s %s(%s, %s)\n", samplerName, samplerKeyWord, textureNameFull, stateNameFull);
		else
			sprintf_s(tempStr, 256, "#define %s %s\n", samplerName, textureNameFull);
		InsertSubString(vulkanGLSL, reflection, tempStr, lineStart);
	}

	for (std::set<SSamplerUniform>::const_iterator iter = samplerUniforms.begin(); iter != samplerUniforms.end(); iter++)
	{
		setIndex = 0;
		bindingIndex = 0; // TheoM: Check if aliasing descriptor (set=0, binding=0) for unused samplers causes any validation issues

		if (!GetSetAndBindingIndex(requestData, uint32_t(SRequestData::ERegisterRangeType::Sampler), iter->Register, setIndex, bindingIndex))
		{
			ReportError("error: %d sampler not found in register range desc (%s).\n", iter->Register, compilerData.filename.c_str());
			return false;
		}

		sprintf_s(tempStr, 256, "layout(set = %i, binding = %i) uniform %s %s;\n", setIndex, bindingIndex, iter->IsShadow ? "samplerShadow" : "sampler", iter->Name.c_str());

		insertPosition = InsertSubString(vulkanGLSL, reflection, tempStr, insertPosition);
	}

	for (std::set<STextureUniform>::const_iterator iter = textureUniforms.begin(); iter != textureUniforms.end(); iter++)
	{
		if (!GetSetAndBindingIndex(requestData, uint32_t(SRequestData::ERegisterRangeType::TextureAndBuffer), iter->Register, setIndex, bindingIndex))
		{
			ReportError("error: %d texture not found in register range desc (%s).\n", iter->Register, compilerData.filename.c_str());
			return false;
		}

		sprintf_s(tempStr, 256, "layout(set = %i, binding = %i) uniform %s %s %s;\n", setIndex, bindingIndex, iter->Precision.c_str(), TextureParamsToTextureKeyWord(iter->TextureType, iter->TextureDimension).c_str(), iter->Name.c_str());

		insertPosition = InsertSubString(vulkanGLSL, reflection, tempStr, insertPosition);
	}

	DeleteStatementWithKeyWord(vulkanGLSL, reflection, std::string("subroutine"));
	ReplaceSubString(vulkanGLSL, reflection, "gl_VertexID", "gl_VertexIndex");
	ReplaceSubString(vulkanGLSL, reflection, "gl_InstanceID", "gl_InstanceIndex");

	return true;
}

bool SaveVulkanGLSL(const SCompilerData& compilerData, const std::string& vulkanGLSL)
{
	std::string glslFilename = compilerData.filename + GetFileExtension(compilerData.request.shaderStage);

	FILE* hFile = nullptr;
	fopen_s(&hFile, glslFilename.c_str(), "wb");

	if (hFile == nullptr)
		return false;

	if (fwrite(vulkanGLSL.c_str(), vulkanGLSL.size(), 1, hFile) != 1)
		return false;

	fclose(hFile);
	return true;
}
}
