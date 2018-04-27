// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include "RequestData.h"
#include "GLSLReflection.h"

static const std::string SourceFileExtension = ".in";
static const std::string DXBCFileExtension = ".dxbc";
static const std::string SPVFileExtension = ".spv";
static const std::string VSFileExtension = ".vert";
static const std::string TCSFileExtension = ".tesc";
static const std::string TESFileExtension = ".tese";
static const std::string GSFileExtension = ".geom";
static const std::string FSFileExtension = ".frag";
static const std::string CSFileExtension = ".comp";
static const std::string OutFileExtension = ".out";

struct SCompilerData
{
	std::string                 filename; //without extension

	SRequestData::SInfo         request;
	std::vector<uint8_t>        dxbc;
	GLSLReflection::SReflection reflection;
};
