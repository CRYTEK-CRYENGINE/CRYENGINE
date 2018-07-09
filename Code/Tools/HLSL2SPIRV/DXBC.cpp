// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include <stdint.h>
#include <algorithm>
#include <string>
#include <vector>
#include "DXBC.h"
#include "CompilerData.h"
#include "..\HLSLCrossCompiler\include\hlslcc.hpp"
#include "..\HLSLCrossCompiler\include\hlslcc_bin.hpp"

static_assert(uint32_t(RGROUP_CBUFFER) == uint32_t(SRequestData::ERegisterRangeType::ConstantBuffer),   "Enum value from request needs to match enum value from hlslcc");
static_assert(uint32_t(RGROUP_TEXTURE) == uint32_t(SRequestData::ERegisterRangeType::TextureAndBuffer), "Enum value from request needs to match enum value from hlslcc");
static_assert(uint32_t(RGROUP_SAMPLER) == uint32_t(SRequestData::ERegisterRangeType::Sampler),          "Enum value from request needs to match enum value from hlslcc");
static_assert(uint32_t(RGROUP_UAV)     == uint32_t(SRequestData::ERegisterRangeType::UnorderedAccess),  "Enum value from request needs to match enum value from hlslcc");

namespace DXBC
{
#define FOURCC(a, b, c, d) ((uint32_t)(uint8_t)(a) | ((uint32_t)(uint8_t)(b) << 8) | ((uint32_t)(uint8_t)(c) << 16) | ((uint32_t)(uint8_t)(d) << 24))

enum
{
	FOURCC_SPRV = FOURCC('S', 'P', 'R', 'V'),
};

#undef FOURCC

struct SDXBCInfo
{
	uint32_t m_uMajorVersion, m_uMinorVersion;
};

struct SDXBCParseContext : SDXBCInputBuffer
{
	SDXBCInfo* m_pInfo;

	SDXBCParseContext(const uint8_t* pBegin, const uint8_t* pEnd, SDXBCInfo* pInfo)
		: SDXBCInputBuffer(pBegin, pEnd)
		, m_pInfo(pInfo)
	{
	}

	SDXBCParseContext(const SDXBCParseContext& kOther, uint32_t uOffset)
		: SDXBCInputBuffer(kOther.m_pBegin + uOffset, kOther.m_pEnd)
		, m_pInfo(kOther.m_pInfo)
	{
		m_pIter = m_pBegin;
	}

	bool ReadString(char* pBuffer, uint32_t uBufSize)
	{
		uint32_t uSize((uint32_t)strlen(reinterpret_cast<const char*>(m_pIter)) + 1);
		if (uSize > uBufSize)
			return false;
		return Read(pBuffer, uSize);
	}
};

struct SDXBCFile
{
	FILE* m_pFile;

	bool  Read(void* pElements, size_t uSize)
	{
		return fread(pElements, 1, uSize, m_pFile) == uSize;
	}

	bool Write(const void* pElements, size_t uSize)
	{
		return fwrite(pElements, 1, uSize, m_pFile) == uSize;
	}

	bool SeekRel(int32_t iOffset)
	{
		return fseek(m_pFile, iOffset, SEEK_CUR) == 0;
	}

	bool SeekAbs(uint32_t uPosition)
	{
		return fseek(m_pFile, uPosition, SEEK_SET) == 0;
	}
};

template<typename I, typename O>
bool CombineWithSPIRV(I& kInput, O& kOutput, const std::vector<uint8_t>& spirvData)
{
	uint32_t uNumChunksIn;
	if (!DXBCCopy(kOutput, kInput, DXBC_HEADER_SIZE) ||
	    !DXBCReadUint32(kInput, uNumChunksIn) ||
	    uNumChunksIn > DXBC_MAX_NUM_CHUNKS_IN)
		return false;

	uint32_t auChunkOffsetsIn[DXBC_MAX_NUM_CHUNKS_IN];
	for (uint32_t uChunk = 0; uChunk < uNumChunksIn; ++uChunk)
	{
		if (!DXBCReadUint32(kInput, auChunkOffsetsIn[uChunk]))
			return false;
	}

	uint32_t auZeroChunkIndex[DXBC_OUT_CHUNKS_INDEX_SIZE] = { 0 };
	if (!kOutput.Write(auZeroChunkIndex, DXBC_OUT_CHUNKS_INDEX_SIZE))
		return false;

	// Copy required input chunks just after the chunk index
	uint32_t uOutSize(DXBC_OUT_FIXED_SIZE);
	uint32_t uNumChunksOut(0);
	uint32_t auChunkOffsetsOut[DXBC_MAX_NUM_CHUNKS_OUT];
	for (uint32_t uChunk = 0; uChunk < uNumChunksIn; ++uChunk)
	{
		uint32_t uChunkCode, uChunkSizeIn;
		if (!kInput.SeekAbs(auChunkOffsetsIn[uChunk]) ||
		    !DXBCReadUint32(kInput, uChunkCode) ||
		    !DXBCReadUint32(kInput, uChunkSizeIn))
			return false;

		// Filter only input chunks of the specified types
		uint32_t uChunkSizeOut(DXBCSizeOutputChunk(uChunkCode, uChunkSizeIn));
		if (uChunkSizeOut > 0)
		{
			if (uNumChunksOut >= DXBC_MAX_NUM_CHUNKS_OUT)
				return false;

			if (!DXBCWriteUint32(kOutput, uChunkCode) ||
			    !DXBCWriteUint32(kOutput, uChunkSizeOut) ||
			    !DXBCCopy(kOutput, kInput, uChunkSizeOut))
				return false;

			auChunkOffsetsOut[uNumChunksOut] = uOutSize;
			++uNumChunksOut;
			uOutSize += DXBC_CHUNK_HEADER_SIZE + uChunkSizeOut;
		}
	}

	// Write SPRV chunk
	uint32_t uSPRVChunkOffset(uOutSize);
	uint32_t uSPRVChunkSize = (uint32_t)spirvData.size();
	if (!DXBCWriteUint32(kOutput, (uint32_t)FOURCC_SPRV) ||
	    !DXBCWriteUint32(kOutput, uSPRVChunkSize) ||
	    !kOutput.Write(&spirvData[0], uSPRVChunkSize))
		return false;
	uOutSize += DXBC_CHUNK_HEADER_SIZE + uSPRVChunkSize;

	// Write total size and chunk index
	if (!kOutput.SeekAbs(DXBC_SIZE_POSITION) ||
	    !DXBCWriteUint32(kOutput, uOutSize) ||
	    !kOutput.SeekAbs(DXBC_HEADER_SIZE) ||
	    !DXBCWriteUint32(kOutput, uNumChunksOut + 1))
		return false;
	for (uint32_t uChunk = 0; uChunk < uNumChunksOut; ++uChunk)
	{
		if (!DXBCWriteUint32(kOutput, auChunkOffsetsOut[uChunk]))
			return false;
	}
	DXBCWriteUint32(kOutput, uSPRVChunkOffset);

	return true;
}

bool RetrieveInfo(SDXBCParseContext& kContext)
{
	uint32_t uNumChunks;
	DXBCReadUint32(kContext, uNumChunks);

	uint32_t uChunk;
	for (uChunk = 0; uChunk < uNumChunks; ++uChunk)
	{
		uint32_t uChunkBegin;
		DXBCReadUint32(kContext, uChunkBegin);

		SDXBCParseContext kChunkHeaderContext(kContext, uChunkBegin);

		uint32_t uChunkFourCC, uChunkSize;
		DXBCReadUint32(kChunkHeaderContext, uChunkFourCC);
		DXBCReadUint32(kChunkHeaderContext, uChunkSize);

		if (uChunkFourCC == FOURCC_SHDR || uChunkFourCC == FOURCC_SHEX)
		{
			uint32_t uEncodedInfo;
			DXBCReadUint32(kChunkHeaderContext, uEncodedInfo);

			kContext.m_pInfo->m_uMajorVersion = (uEncodedInfo >> 4) & 0xF;
			kContext.m_pInfo->m_uMinorVersion = uEncodedInfo & 0xF;

			return true;
		}
	}
	return false;
}

bool Load(SCompilerData& compilerData)
{
	std::string dxbcFilename = compilerData.filename + DXBCFileExtension;

	FILE* hFile = nullptr;
	fopen_s(&hFile, dxbcFilename.c_str(), "rb");

	if (hFile == nullptr)
		return false;

	fseek(hFile, 0, SEEK_END);
	int fileSize = ftell(hFile);
	fseek(hFile, 0, SEEK_SET);

	compilerData.dxbc.resize(fileSize);

	fread(&compilerData.dxbc[0], fileSize, 1, hFile);
	fclose(hFile);

	enum { CHUNKS_POSITION = sizeof(uint32_t) * 7 };
	SDXBCInfo kInfo;
	SDXBCParseContext kContext(&compilerData.dxbc[0], &compilerData.dxbc[0] + fileSize, &kInfo);

	if (!kContext.SeekAbs(CHUNKS_POSITION))
		return false;

	{
		SDXBCParseContext kParseContext(kContext);
		if (!RetrieveInfo(kParseContext))
			return false;
	}

	uint32_t uNumChunks;
	if (!DXBCReadUint32(kContext, uNumChunks))
		return false;

	uint32_t uOffset;
	for (uint32_t uChunk = 0; uChunk < uNumChunks; ++uChunk)
	{
		uint32_t uChunkBegin;
		if (!DXBCReadUint32(kContext, uChunkBegin))
			return false;

		SDXBCParseContext kChunkHeaderContext(kContext, uChunkBegin);

		uint32_t uChunkFourCC, uChunkSize;
		if (!DXBCReadUint32(kChunkHeaderContext, uChunkFourCC) ||
		    !DXBCReadUint32(kChunkHeaderContext, uChunkSize))
			return false;

		SDXBCParseContext kChunkContext(kChunkHeaderContext.m_pIter, kChunkHeaderContext.m_pEnd, &kInfo);
		switch (uChunkFourCC)
		{
		case FOURCC_GLSL:
			if (!DXBCReadUint32(kChunkContext, compilerData.reflection.uNumSamplers) ||
			    !DXBCReadUint32(kChunkContext, compilerData.reflection.uNumImages) ||
			    !DXBCReadUint32(kChunkContext, compilerData.reflection.uNumStorageBuffers) ||
			    !DXBCReadUint32(kChunkContext, compilerData.reflection.uNumUniformBuffers) ||
			    !DXBCReadUint32(kChunkContext, compilerData.reflection.uImportsSize) ||
			    !DXBCReadUint32(kChunkContext, compilerData.reflection.uExportsSize) ||
			    !DXBCReadUint32(kChunkContext, compilerData.reflection.uInputHash) ||
			    !DXBCReadUint32(kChunkContext, compilerData.reflection.uSymbolsOffset))
				return false;

			uOffset = (uint32_t)(kChunkContext.m_pIter - kContext.m_pBegin);
			compilerData.reflection.uSamplersOffset = uOffset;
			uOffset += GLSL_SAMPLER_SIZE * compilerData.reflection.uNumSamplers;
			compilerData.reflection.uImagesOffset = uOffset;
			uOffset += GLSL_RESOURCE_SIZE * compilerData.reflection.uNumImages;
			compilerData.reflection.uStorageBuffersOffset = uOffset;
			uOffset += GLSL_RESOURCE_SIZE * compilerData.reflection.uNumStorageBuffers;
			compilerData.reflection.uUniformBuffersOffset = uOffset;
			uOffset += GLSL_RESOURCE_SIZE * compilerData.reflection.uNumUniformBuffers;
			compilerData.reflection.uImportsOffset = uOffset;
			uOffset += GLSL_SYMBOL_SIZE * compilerData.reflection.uImportsSize;
			compilerData.reflection.uExportsOffset = uOffset;
			uOffset += GLSL_SYMBOL_SIZE * compilerData.reflection.uExportsSize;
			compilerData.reflection.uGLSLSourceOffset = uOffset;
			break;
		}
	}

	return true;
}

bool CombineWithSPIRVAndSave(const std::string& shaderFilename, const std::string& outputFilename)
{
	std::string spvFilename = shaderFilename + SPVFileExtension;

	FILE* hFile = nullptr;
	fopen_s(&hFile, spvFilename.c_str(), "rb");

	if (hFile == nullptr)
		return false;

	fseek(hFile, 0, SEEK_END);
	int fileSize = ftell(hFile);
	fseek(hFile, 0, SEEK_SET);

	std::vector<uint8_t> spv;
	spv.resize(fileSize);

	fread(&spv[0], fileSize, 1, hFile);
	fclose(hFile);

	std::string dxbcFilename = shaderFilename + DXBCFileExtension;

	SDXBCFile dxbcFile;
	SDXBCFile outputFile;
	fopen_s(&dxbcFile.m_pFile, dxbcFilename.c_str(), "rb");
	fopen_s(&outputFile.m_pFile, outputFilename.c_str(), "wb");

	bool result =
	  dxbcFile.m_pFile != nullptr && outputFile.m_pFile != nullptr &&
	  CombineWithSPIRV(dxbcFile, outputFile, spv);

	return result;
}
}
