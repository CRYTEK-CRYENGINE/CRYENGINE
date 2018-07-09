// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include <stdint.h>
#include <vector>
#include "RequestData.h"
#include "Base64.h"
#include "CompilerData.h"
#include "Log.h"

namespace SRequestData
{

bool Load(const std::string& dataFilename, SInfo& info)
{
	FILE* hFile = nullptr;
	fopen_s(&hFile, dataFilename.c_str(), "r");

	if (hFile == nullptr)
		return false;

	fseek(hFile, 0, SEEK_END);
	int fileSize = ftell(hFile);
	fseek(hFile, 0, SEEK_SET);

	std::vector<char> dataStr;
	dataStr.resize(fileSize);

	fread(&dataStr[0], fileSize, 1, hFile);
	fclose(hFile);

	if (dataStr[0] != '/' && dataStr[1] != '*')
		return false;

	int dataStartIndex = 2;
	while (dataStr[dataStartIndex] == '\r' || dataStr[dataStartIndex] == '\n')
		dataStartIndex++;

	int dataEndIndex = dataStartIndex;
	while (dataStr[dataEndIndex] != '\r' && dataStr[dataEndIndex] != '\n' && dataStr[dataEndIndex] != '*')
		dataEndIndex++;

	std::string encodedData;
	encodedData.resize(dataEndIndex - dataStartIndex);
	memcpy(&encodedData[0], &dataStr[dataStartIndex], dataEndIndex - dataStartIndex);

	size_t dataLength = Base64::DecodedSize(encodedData.c_str(), encodedData.length());
	if (dataLength % 4 != 0)
	{
		ReportError("error: Cannot load shader description (%s). Invalid data length.\n", dataFilename.c_str());
		return false;
	}

	std::vector<uint32_t> data;
	data.resize(dataLength / 4);

	Base64::DecodeBuffer(encodedData.c_str(), encodedData.length(), &data[0]);

	int dataPtr = 0;
	char tempString[128];

	int profileNameLength = data[dataPtr++];

	if (profileNameLength > 127)
	{
		ReportError("error: Shader profile name too long (>127 char, %s).\n", dataFilename.c_str());
		return false;
	}

	memcpy(tempString, &data[dataPtr], profileNameLength);
	tempString[profileNameLength] = 0;
	dataPtr += (profileNameLength + 3) / 4;

	info.profileName = tempString;

	int entryNameLength = data[dataPtr++];

	if (entryNameLength > 127)
	{
		ReportError("error: Shader entry name too long (>127 char, %s).\n", dataFilename.c_str());
		return false;
	}

	memcpy(tempString, &data[dataPtr], entryNameLength);
	tempString[entryNameLength] = 0;
	dataPtr += (entryNameLength + 3) / 4;

	info.entryName = tempString;

	// read layout data
	{
		uint8_t* pLayoutData = reinterpret_cast<uint8_t*>(&data[dataPtr]);

		int setCount = *pLayoutData++;
		info.registerRanges.resize(setCount);

		for (int i = 0; i < setCount; i++)
		{
			int rangeCount = *pLayoutData++;
			info.registerRanges[i].resize(rangeCount);

			for (int j = 0; j < rangeCount; j++)
			{
				uint8_t slotTypeStagesByte = *pLayoutData++;
				uint8_t slotNumberDescCountByte = *pLayoutData++;
				info.registerRanges[i][j].setTypeAndStage(slotTypeStagesByte);
				info.registerRanges[i][j].setSlotNumberAndDescCount(slotNumberDescCountByte);
			}
		}

		dataPtr += (int)(std::distance(reinterpret_cast<uint8_t*>(&data[dataPtr]), pLayoutData) + 3)/ 4;
	}

	int elementCount = data[dataPtr++];
	info.vertexInputDesc.vertexInputElements.resize(elementCount);

	for (int i = 0; i < elementCount; i++)
	{
		int semanticNameLength = data[dataPtr++];
		memcpy(info.vertexInputDesc.vertexInputElements[i].semanticName, &data[dataPtr], semanticNameLength);
		info.vertexInputDesc.vertexInputElements[i].semanticName[semanticNameLength] = 0;
		dataPtr += (semanticNameLength + 3) / 4;

		info.vertexInputDesc.vertexInputElements[i].semanticIndex = data[dataPtr++];
	}
	info.vertexInputDesc.objectStreamMask = data[dataPtr++];

	switch (info.profileName[0])
	{
	case 'v':
		info.shaderStage = EShaderStage::Vertex;
		break;
	case 'h':
		info.shaderStage = EShaderStage::TessellationControl;
		break;
	case 'd':
		info.shaderStage = EShaderStage::TessellationEvaluation;
		break;
	case 'g':
		info.shaderStage = EShaderStage::Geometry;
		break;
	case 'p':
		info.shaderStage = EShaderStage::Fragment;
		break;
	case 'c':
		info.shaderStage = EShaderStage::Compute;
		break;
	}

	return true;
}
}
