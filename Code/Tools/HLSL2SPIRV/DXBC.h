// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

struct SCompilerData;

namespace DXBC
{
bool Load(SCompilerData& compilerData);
bool CombineWithSPIRVAndSave(const std::string& shaderFilename, const std::string& outputFilename);
}
