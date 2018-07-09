// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once
#include "HlslParser.h"

struct SPsslConversion
{
	bool                    bMatrixStorageRowMajor; // Default HLSL convention is column-major. CryFX convention is row-major.
	bool                    bSupportSmallTypes;     // If set, non-app-visible variables may be emitted as 16-bit types.
	bool                    bIndentWithTabs;        // If set, tabs will be used for indentation (otherwise, 4 spaces are used for each tab).
	bool                    bSupportLegacySamplers; // If set, the use of sampler1D/sampler2D/sampler3D/samplerCUBE is attempted to be upgraded.
	HlslParser::EShaderType shaderType;             // The shader-type being transformed.
	std::string             comment;                // A C-style comment to insert into the header of the generated PSSL code.
	std::string             entryPoint;             // The name of the entry-point function.
};

// Converts the specified parsed HLSL program to PSSL
// The conversion context to use is specified as well.
std::string ToPssl(const HlslParser::SProgram& hlsl, const SPsslConversion& conversion);
